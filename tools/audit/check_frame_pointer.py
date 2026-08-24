#!/usr/bin/env python3
"""Post-link guard: naked helpers that read their CALLER's EBP frame.

A handful of lifted helpers cannot be expressed in portable C because they read
a stack frame they do not own.  The canonical one is FUN_000d1540 in
src/halo/interface/hud.c, the HUD return-address canary, whose entire body is:

    mov eax, dword ptr [ebp + 4]
    ret

Because it establishes no prologue, EBP at entry still holds the *caller's*
frame pointer, so [ebp+4] is the caller's return address.  That is only true if
every caller actually keeps EBP as a frame pointer.

Clang omits the frame pointer at -Og/-O2/-O3 unless -fno-omit-frame-pointer is
passed, and then happily allocates EBP as a general-purpose scratch register.
When that happens the helper reads a garbage address.  On 2026-08-24 the Debug
config was missing the flag (it was set only on CMAKE_C_FLAGS_RELEASE), EBP in
FUN_000d7800 held a player handle of 0, and the helper faulted:

    exception code=0xc0000005 thread=28 address=0x006cf3b0 read=0x00000004

The HUD render thread died before the first frame while the other three engine
threads kept ticking, so the map loaded to a black screen with no assert.

Checking the compiler flag is not enough: what matters is the codegen that came
out the other end.  So this script disassembles the linked PE, finds every
direct CALL to each registered helper, and verifies the calling function opens
with a real `push ebp; mov ebp, esp`.

Exit status: 0 = clean, 1 = a caller lacks a frame pointer, 2 = usage error.
"""

import argparse
import os
import sys

try:
    import pefile
except ImportError:
    print("error: pefile not installed (pip install 'pefile~=2023.2.7')",
          file=sys.stderr)
    sys.exit(2)

try:
    import capstone
except ImportError:
    print("error: capstone not installed (pip install capstone)",
          file=sys.stderr)
    sys.exit(2)

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_PE = os.path.join(ROOT_DIR, "build", "halo")

# Helpers that read their caller's EBP frame.  Add to this list whenever a new
# naked helper depends on the caller's frame pointer.
CALLER_FRAME_HELPERS = (
    ("FUN_000d1540", "hud.c return-address canary, `mov eax,[ebp+4]`"),
)

# push ebp / mov ebp, esp -- the two encodings clang and MSVC emit.
FRAME_PROLOGUES = (
    bytes((0x55, 0x89, 0xE5)),  # push ebp; mov ebp, esp  (clang)
    bytes((0x55, 0x8B, 0xEC)),  # push ebp; mov ebp, esp  (MSVC form)
)


def _exports(pe):
    """Map export name -> RVA."""
    out = {}
    directory = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
    if directory is None:
        return out
    for sym in directory.symbols:
        if sym.name:
            out[sym.name.decode("ascii", "replace")] = sym.address
    return out


def _function_extents(exports):
    """Approximate each export's extent as [rva, next_export_rva).

    The PE carries no symbol sizes, so sort the export RVAs and treat the gap to
    the following export as the function body.  Padding between functions is
    included, which is harmless here: we only ever scan for CALL instructions
    and read the first bytes of each function.
    """
    ordered = sorted(set(exports.values()))
    bounds = {}
    for i, rva in enumerate(ordered):
        end = ordered[i + 1] if i + 1 < len(ordered) else rva + 0x400
        bounds[rva] = end
    return ordered, bounds


def check(pe_path, verbose=False):
    if not os.path.isfile(pe_path):
        print(f"error: PE not found: {pe_path}\n  build it first "
              f"(python3 tools/build/build.py)", file=sys.stderr)
        return 2

    pe = pefile.PE(pe_path, fast_load=False)
    image = pe.get_memory_mapped_image()
    exports = _exports(pe)
    if not exports:
        print(f"error: no exports in {pe_path}; cannot attribute callers",
              file=sys.stderr)
        return 2

    targets = {}
    for name, why in CALLER_FRAME_HELPERS:
        rva = exports.get(name)
        if rva is None:
            # Not every helper exists in every build profile.
            if verbose:
                print(f"note: {name} not in this build, skipping")
            continue
        targets[rva] = (name, why)

    if not targets:
        if verbose:
            print("no caller-frame helpers present; nothing to check")
        return 0

    ordered, bounds = _function_extents(exports)
    rva_to_name = {}
    for name, rva in exports.items():
        # Several aliases can share an RVA; keep the first for reporting.
        rva_to_name.setdefault(rva, name)

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    failures = []
    call_count = 0

    for rva in ordered:
        end = min(bounds[rva], len(image))
        if rva >= len(image):
            continue
        body = image[rva:end]
        prologue_ok = body[:3] in FRAME_PROLOGUES
        caller = rva_to_name.get(rva, f"rva_{rva:#x}")

        for insn in md.disasm(body, rva):
            if insn.mnemonic != "call" or not insn.op_str.startswith("0x"):
                continue
            try:
                dest = int(insn.op_str, 16)
            except ValueError:
                continue
            if dest not in targets:
                continue
            call_count += 1
            helper, why = targets[dest]
            if prologue_ok:
                continue
            failures.append((caller, rva, insn.address, helper, why,
                             body[:3].hex()))

    if verbose:
        print(f"checked {call_count} call(s) to "
              f"{len(targets)} caller-frame helper(s) in {pe_path}")

    if not failures:
        return 0

    print("error: frame pointer omitted in caller of a caller-frame helper",
          file=sys.stderr)
    for caller, caller_rva, site, helper, why, opening in failures:
        print(f"  {caller} (rva {caller_rva:#x}) calls {helper} at "
              f"rva {site:#x}", file=sys.stderr)
        print(f"    {helper}: {why}", file=sys.stderr)
        print(f"    caller opens with {opening}, not `push ebp; mov ebp,esp`",
              file=sys.stderr)
    print("", file=sys.stderr)
    print("  The helper will read a garbage frame and fault at runtime "
          "(black screen,", file=sys.stderr)
    print("  no assert).  Ensure -fno-omit-frame-pointer is in CMAKE_C_FLAGS "
          "for the", file=sys.stderr)
    print("  active build type, then rebuild.", file=sys.stderr)
    return 1


def main():
    parser = argparse.ArgumentParser(
        description="Verify callers of caller-EBP-reading naked helpers keep a "
                    "frame pointer.")
    parser.add_argument("pe", nargs="?", default=DEFAULT_PE,
                        help="linked PE to inspect (default: build/halo)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="report what was checked even when clean")
    args = parser.parse_args()
    return check(args.pe, verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
