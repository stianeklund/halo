#!/usr/bin/env python3
"""SHAPE ATLAS -- donor/recipient template-transfer scheduler (lift-accel Phase 2).

CONCEPT
-------
Group every function in the pristine binary by a normalized "shape key".
Functions sharing a shape key were compiled from (near-)identical source
templates -- MSVC 7.1 emits near-identical code for near-identical C, so two
functions with the same instruction skeleton are almost always two instances
of the same hand-written pattern (accessor pairs, per-axis variants, table
dispatchers, ...).

If a shape group contains at least one already-ported function with a known
high VC71 match, that function is a DONOR: every other (unported) member of
the group is a RECIPIENT that can very likely be lifted by instantiating the
donor's proven source template, rather than from scratch. A group of >=3
unported members with NO donor is "unsolved bulk" -- solving any one member
is strong evidence for how to solve the rest.

This is a SCHEDULING AID ONLY. It never grants match credit; every recipient
still goes through the normal lift + VC71/equivalence verify pipeline. All it
does is tell an agent (human or auto-lift) "here is a near-free target, and
here is the template to copy."

SHAPE KEY
---------
    shape_key = "<insn_count>:<mnemonic_seq_hash>:<opclass_seq_hash>"

  * insn_count          -- raw instruction count (cheap, exact discriminator).
  * mnemonic_seq_hash   -- sha256 of the ordered mnemonic sequence.
  * opclass_seq_hash    -- sha256 of the ordered sequence with registers
                            canonicalized to first-appearance-order aliases
                            (%R0, %R1, ...) and immediates/displacements/
                            absolute addresses collapsed to IMM/OFF class
                            tokens, so identical templates at DIFFERENT
                            addresses (different callees, different literal
                            offsets, different registers doing the same
                            *role*) still collide.

This reuses tools/verify/compare_obj.py's own normalization exactly:
`extract_mnemonic_sequence` for the first hash, and
`extract_normalized_sequence` (canonicalize_registers + normalize_instruction)
for the second -- the same functions vc71_verify.py uses to score a lift, so
"same shape" here means the literal thing VC71 scoring is blind to.

BYTES / DISASSEMBLY
--------------------
Function bytes come from `tools/verify/xbe_reference.function_bytes()` --
the exact extraction path `vc71_verify`'s SYNTH_REFS fallback uses to
synthesize a reference "from the pristine XBE" (see `_synth_ref` there).
Reused as-is: it already knows how to bound a function (next kb.json start
vs. capstone `true_end`) without a delinked reference.

For instruction TEXT (needed by compare_obj's regex-based normalizers, which
expect llvm-objdump's AT&T text) this module batches many functions into ONE
synthesized COFF object -- adapting `xbe_reference.build_coff` (one symbol)
into `_build_multi_coff` (N symbols, one per function, laid back-to-back at
their exact extracted length) -- and disassembles each batch with a single
`llvm-objdump` call. That is the "two-pass" performance guard the design
called for: whole-binary disassembly is O(few thousand functions / batch)
subprocess spawns instead of O(functions), which is what keeps a ~8000-
function build in the tens of seconds rather than hours. No functions are
skipped or bucketed by a cheaper pre-key; measured full-binary timing (see
`build`'s printed timings) came in fast enough that the size+call-count
bucketing fallback described in the design doc was not needed.

USAGE
-----
    rtk python3 tools/shape/shape_atlas.py build [--batch-size N] [--quiet]
    rtk python3 tools/shape/shape_atlas.py report [--limit N]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
KB_JSON = REPO_ROOT / "kb.json"
ARTIFACT_DIR = REPO_ROOT / "artifacts"
ATLAS_JSON = ARTIFACT_DIR / "shape_atlas.json"
VC71_SCORES_JSON = REPO_ROOT / "tools" / "verify" / "vc71_scores.json"

sys.path.insert(0, str(REPO_ROOT / "tools" / "verify"))
sys.path.insert(0, str(REPO_ROOT / "tools" / "audit"))

import xbe_reference as xr  # noqa: E402  (reuse pristine-XBE extraction path)
from compare_obj import (  # noqa: E402  (reuse VC71's own normalization)
    _parse_objdump_insn,
    extract_mnemonic_sequence,
    extract_normalized_sequence,
)

EXACT_SCORE_THRESHOLD = 99.0
DEFAULT_BATCH_SIZE = 1000
BULK_THRESHOLD = 3  # >=N unported members with no donor => "unsolved bulk"


# ---------------------------------------------------------------------------
# kb.json -> flat function registry (addr -> record), first occurrence wins.
# ---------------------------------------------------------------------------

def _parse_name_from_decl(decl: str) -> str:
    cleaned = re.sub(r"@<\w+>", "", decl)
    m = re.search(r"(\w+)\s*\(", cleaned)
    return m.group(1) if m else ""


def load_kb_functions(kb: dict) -> dict[int, dict]:
    """Return {addr: {"name", "ported", "object", "source_path", "decl"}}.

    Mirrors llm_auto_lift.LiftabilityScorer's object-level source_path
    convention (obj["source"] -> "src/halo/<source>") rather than the
    inconsistent per-function source_path/src/file fields kb.json carries as
    legacy leftovers, so donor source_path always agrees with what auto-lift
    would show for the same function.
    """
    seen: dict[int, dict] = {}
    for obj in kb.get("objects", []):
        obj_name = obj.get("name", "")
        source = obj.get("source") or ""
        source_path = (f"src/halo/{source}" if source and not source.startswith("src/")
                        else source)
        for func in obj.get("functions", []):
            addr_s = func.get("addr")
            if not addr_s:
                continue
            try:
                addr = int(addr_s, 16)
            except ValueError:
                continue
            if addr in seen:
                continue  # first occurrence wins (kb.json in-flight dup guard)
            decl = func.get("decl", "")
            name = func.get("name") or _parse_name_from_decl(decl) or f"FUN_{addr:08x}"
            seen[addr] = {
                "name": name,
                "ported": bool(func.get("ported")),
                "object": obj_name,
                "source_path": source_path,
                "decl": decl,
            }
    return seen


# ---------------------------------------------------------------------------
# Batched disassembly: N functions per synthesized COFF / objdump call.
# ---------------------------------------------------------------------------

def _build_multi_coff(entries: list[tuple[str, bytes]]) -> bytes:
    """Single-section i386 COFF holding many functions, one symbol each.

    Adapts xbe_reference.build_coff (single symbol) to batch N functions into
    one llvm-objdump invocation. Functions are laid out back-to-back at their
    exact xbe_reference.function_bytes() length -- no MSVC-style padding
    exists between them and none is needed, since the symbol table alone
    marks every boundary precisely.
    """
    code = b"".join(c for _, c in entries)
    text_off = 20 + 40
    symtab_off = text_off + len(code)

    strtab = bytearray()
    symtab = bytearray()
    offset = 0
    for name, c in entries:
        name_b = name.encode()
        if len(name_b) <= 8:
            name_field = name_b.ljust(8, b"\0")
        else:
            name_field = struct.pack("<II", 0, 4 + len(strtab))
            strtab += name_b + b"\0"
        symtab += name_field + struct.pack(
            "<IhHBB", offset, 1, xr.IMAGE_SYM_DTYPE_FUNCTION,
            xr.IMAGE_SYM_CLASS_EXTERNAL, 0)
        offset += len(c)

    hdr = struct.pack(
        "<HHIIIHH",
        xr.IMAGE_FILE_MACHINE_I386, 1, 0, symtab_off, len(entries), 0,
        xr.IMAGE_FILE_LINE_NUMS_STRIPPED | xr.IMAGE_FILE_32BIT_MACHINE)
    sec = struct.pack(
        "<8sIIIIIIHHI", b".text\0\0\0", 0, 0, len(code), text_off,
        0, 0, 0, 0, xr.SCN_TEXT_CHARACTERISTICS)
    strtab_full = struct.pack("<I", 4 + len(strtab)) + bytes(strtab)
    return hdr + sec + code + bytes(symtab) + strtab_full


_HEADER_RE = re.compile(r'^(?:[0-9a-f]+ )?<([^>]+)>:')


def _parse_batch(stdout: str) -> dict[str, list[str]]:
    """{symbol_name: [instruction_lines]} from a batched objdump -d listing.

    Simpler than compare_obj.disassemble()/xbe_reference.objdump_insns()
    deliberately: those handle MSVC inter-function padding and Ghidra
    LAB_/switchD_ labels, neither of which can occur here -- every byte in
    the batch came from a precisely-bounded xbe_reference.function_bytes()
    call and the only symbols in the object are the ones we added.
    """
    functions: dict[str, list[str]] = {}
    current: str | None = None
    lines: list[str] = []
    for raw in stdout.splitlines():
        line = raw.rstrip()
        m = _HEADER_RE.match(line)
        if m:
            if current is not None:
                functions[current] = lines
            current = re.sub(r'@\d+$', '', m.group(1).lstrip("_@"))
            lines = []
            continue
        insn = _parse_objdump_insn(line)
        if insn is not None:
            lines.append(insn)
    if current is not None:
        functions[current] = lines
    return functions


def disassemble_batched(resolved: dict[int, dict], *, batch_size: int = DEFAULT_BATCH_SIZE,
                         quiet: bool = False) -> dict[int, list[str]]:
    """Instruction-text lines per address, disassembled `batch_size` at a time."""
    addrs = sorted(resolved.keys())
    insns_by_addr: dict[int, list[str]] = {}
    n_batches = (len(addrs) + batch_size - 1) // batch_size or 1
    for bi in range(0, len(addrs), batch_size):
        chunk = addrs[bi:bi + batch_size]
        entries = [(f"FUN_{a:08x}", resolved[a]["code"]) for a in chunk]
        obj_bytes = _build_multi_coff(entries)
        fd, tmp_path = tempfile.mkstemp(suffix=".obj")
        try:
            with os.fdopen(fd, "wb") as tf:
                tf.write(obj_bytes)
            r = subprocess.run(
                ["llvm-objdump", "-d", "--no-show-raw-insn", "--no-leading-addr", tmp_path],
                capture_output=True, text=True)
            if r.returncode != 0:
                if not quiet:
                    print(f"[shape] WARN: objdump failed on batch "
                          f"{bi // batch_size + 1}/{n_batches}: "
                          f"{r.stderr.strip()[:200]}", file=sys.stderr)
                continue
            parsed = _parse_batch(r.stdout)
        finally:
            os.unlink(tmp_path)
        for a in chunk:
            lines = parsed.get(f"FUN_{a:08x}")
            if lines is not None:
                insns_by_addr[a] = lines
    return insns_by_addr


# ---------------------------------------------------------------------------
# Shape key + grouping -- PURE functions, unit-testable without the XBE.
# ---------------------------------------------------------------------------

def compute_shape_key(insns: list[str]) -> str:
    """Address-independent shape key for one function's instruction text.

    Two functions produce the same key iff they have the same instruction
    count, the same ordered mnemonics, AND the same ordered operand-class
    sequence (canonical register roles + IMM/OFF class tokens) -- i.e. they
    are very likely two instantiations of the same source template.
    """
    mnem_seq = extract_mnemonic_sequence(insns)
    opclass_seq = extract_normalized_sequence(insns)
    mnem_hash = hashlib.sha256(",".join(mnem_seq).encode()).hexdigest()[:16]
    opclass_hash = hashlib.sha256(",".join(opclass_seq).encode()).hexdigest()[:16]
    return f"{len(insns)}:{mnem_hash}:{opclass_hash}"


def group_functions(insns_by_addr: dict[int, list[str]]) -> dict[str, list[int]]:
    """{shape_key: [addr, ...]} over every address with disassembled insns."""
    groups: dict[str, list[int]] = {}
    for addr in sorted(insns_by_addr.keys()):
        key = compute_shape_key(insns_by_addr[addr])
        groups.setdefault(key, []).append(addr)
    return groups


def donor_tier_and_score(name: str, scores_by_name: dict[str, dict]
                          ) -> tuple[str, float | None]:
    """("exact"|"ported", score|None) for a ported candidate donor.

    "exact": a recorded VC71 score is known and >= EXACT_SCORE_THRESHOLD.
    "ported": every other ported candidate -- score unknown, OR known but
    below the exact threshold. (The design calls out "score unknown" as the
    common "ported" case; a known-but-lower score is treated the same way
    since ported=true is itself the only hard gate on donor eligibility --
    tier is purely a confidence label layered on top.)
    """
    entry = scores_by_name.get(name)
    score = None
    if isinstance(entry, dict):
        score = entry.get("score")
    tier = "exact" if (score is not None and score >= EXACT_SCORE_THRESHOLD) else "ported"
    return tier, score


def select_donor(members: list[int], resolved: dict[int, dict],
                  scores_by_name: dict[str, dict]) -> dict | None:
    """Best donor record for a shape group, or None if no member is ported.

    Preference order: "exact" tier over "ported" tier; within a tier, highest
    known score; ties broken by address order (stable, deterministic)."""
    candidates = []
    for a in members:
        rec = resolved[a]
        if not rec.get("ported"):
            continue
        tier, score = donor_tier_and_score(rec["name"], scores_by_name)
        candidates.append((a, tier, score))
    if not candidates:
        return None

    def _rank(item):
        a, tier, score = item
        tier_rank = 0 if tier == "exact" else 1
        score_rank = -(score if score is not None else -1.0)
        return (tier_rank, score_rank, a)

    candidates.sort(key=_rank)
    a, tier, score = candidates[0]
    rec = resolved[a]
    return {
        "addr": f"0x{a:x}",
        "name": rec["name"],
        "object": rec["object"],
        "source_path": rec["source_path"],
        "tier": tier,
        "score": score,
    }


def classify_groups(groups: dict[str, list[int]], resolved: dict[int, dict],
                     scores_by_name: dict[str, dict]) -> tuple[list[dict], dict]:
    """Build the stored group records + aggregate stats.

    Only groups with >=2 members are returned (singletons carry no scheduling
    value and would dominate the file with one-line entries); stats are
    computed over ALL groups so the singleton count is still visible.
    """
    out: list[dict] = []
    groups_total = len(groups)
    groups_ge2 = 0
    transfer_groups = 0
    unsolved_bulk_groups = 0
    transfer_recipient_bytes = 0
    donor_exact_groups = 0
    donor_ported_groups = 0

    for shape_key, members in groups.items():
        if len(members) < 2:
            continue
        groups_ge2 += 1

        donor = select_donor(members, resolved, scores_by_name)
        recipients = [a for a in members if not resolved[a]["ported"]]

        is_transfer = donor is not None and bool(recipients)
        is_bulk = donor is None and len(members) >= BULK_THRESHOLD

        if is_transfer:
            transfer_groups += 1
            transfer_recipient_bytes += sum(resolved[a]["bytes"] for a in recipients)
            if donor["tier"] == "exact":
                donor_exact_groups += 1
            else:
                donor_ported_groups += 1
        if is_bulk:
            unsolved_bulk_groups += 1

        member_records = [
            {
                "addr": f"0x{a:x}",
                "name": resolved[a]["name"],
                "ported": resolved[a]["ported"],
                "bytes": resolved[a]["bytes"],
                "object": resolved[a]["object"],
                "source_path": resolved[a]["source_path"],
            }
            for a in members
        ]
        first = resolved[members[0]]
        out.append({
            "shape_key": shape_key,
            "insn_count": first.get("insn_count"),
            "members": member_records,
            "donor": donor,
            "recipients": [f"0x{a:x}" for a in recipients],
            "is_transfer": is_transfer,
            "is_unsolved_bulk": is_bulk,
        })

    stats = {
        "groups_total": groups_total,
        "groups_ge2": groups_ge2,
        "transfer_groups": transfer_groups,
        "unsolved_bulk_groups": unsolved_bulk_groups,
        "transfer_recipient_bytes": transfer_recipient_bytes,
        "donor_exact_groups": donor_exact_groups,
        "donor_ported_groups": donor_ported_groups,
    }
    return out, stats


# ---------------------------------------------------------------------------
# vc71 score lookup
# ---------------------------------------------------------------------------

def load_vc71_scores_by_name() -> dict[str, dict]:
    if not VC71_SCORES_JSON.exists():
        return {}
    try:
        data = json.loads(VC71_SCORES_JSON.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    scores = data.get("scores") if isinstance(data, dict) else None
    return scores if isinstance(scores, dict) else {}


# ---------------------------------------------------------------------------
# End-to-end build
# ---------------------------------------------------------------------------

def build_atlas(*, batch_size: int = DEFAULT_BATCH_SIZE, quiet: bool = False) -> dict:
    t0 = time.time()
    kb = json.loads(KB_JSON.read_text(encoding="utf-8"))
    functions = load_kb_functions(kb)

    resolved: dict[int, dict] = {}
    fail_reasons: Counter = Counter()
    for addr, rec in functions.items():
        code, err = xr.function_bytes(addr)
        if not code:
            fail_reasons[err or "unknown"] += 1
            continue
        resolved[addr] = {**rec, "bytes": len(code), "code": code}
    t_resolve = time.time() - t0
    if not quiet:
        print(f"[shape] {len(resolved)}/{len(functions)} functions resolved to "
              f"XBE bytes in {t_resolve:.1f}s ({len(functions) - len(resolved)} unresolved)")
        for reason, n in fail_reasons.most_common(5):
            print(f"[shape]   unresolved x{n}: {reason}")

    t1 = time.time()
    insns_by_addr = disassemble_batched(resolved, batch_size=batch_size, quiet=quiet)
    for a, insns in insns_by_addr.items():
        resolved[a]["insn_count"] = len(insns)
    t_disasm = time.time() - t1
    n_batches = (len(resolved) + batch_size - 1) // batch_size or 1
    if not quiet:
        print(f"[shape] disassembled {len(insns_by_addr)}/{len(resolved)} functions "
              f"in {t_disasm:.1f}s across {n_batches} batch(es)")

    t2 = time.time()
    groups = group_functions(insns_by_addr)
    scores_by_name = load_vc71_scores_by_name()
    group_records, stats = classify_groups(groups, resolved, scores_by_name)
    t_group = time.time() - t2

    stats["total_functions"] = len(functions)
    stats["resolved_functions"] = len(resolved)
    stats["disassembled_functions"] = len(insns_by_addr)
    stats["build_seconds"] = round(time.time() - t0, 2)

    if not quiet:
        print(f"[shape] grouped into {stats['groups_total']} shape(s) "
              f"({stats['groups_ge2']} with >=2 members) in {t_group:.1f}s")
        print(f"[shape] total build time: {stats['build_seconds']}s")

    return {
        "version": 1,
        "generated_unix": int(time.time()),
        "stats": stats,
        "groups": group_records,
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _print_stats(stats: dict) -> None:
    print("Shape atlas stats:")
    print(f"  total_functions            {stats.get('total_functions')}")
    print(f"  resolved_functions         {stats.get('resolved_functions')}")
    print(f"  disassembled_functions     {stats.get('disassembled_functions')}")
    print(f"  groups_total               {stats.get('groups_total')}")
    print(f"  groups_ge2 (>=2 members)   {stats.get('groups_ge2')}")
    print(f"  transfer_groups            {stats.get('transfer_groups')} "
          f"(exact-donor {stats.get('donor_exact_groups')}, "
          f"ported-donor {stats.get('donor_ported_groups')})")
    print(f"  unsolved_bulk_groups       {stats.get('unsolved_bulk_groups')}")
    print(f"  transfer_recipient_bytes   {stats.get('transfer_recipient_bytes')}")


def cmd_build(args: argparse.Namespace) -> int:
    atlas = build_atlas(batch_size=args.batch_size, quiet=args.quiet)
    ARTIFACT_DIR.mkdir(parents=True, exist_ok=True)
    ATLAS_JSON.write_text(json.dumps(atlas, indent=1), encoding="utf-8")
    print(f"[shape] wrote {ATLAS_JSON} ({ATLAS_JSON.stat().st_size} bytes)")
    _print_stats(atlas["stats"])
    return 0


def cmd_report(args: argparse.Namespace) -> int:
    if not ATLAS_JSON.exists():
        print(f"No atlas at {ATLAS_JSON}. Run `build` first.")
        return 1
    atlas = json.loads(ATLAS_JSON.read_text(encoding="utf-8"))
    _print_stats(atlas["stats"])

    groups = atlas.get("groups", [])
    transfer = [g for g in groups if g.get("is_transfer")]
    bulk = [g for g in groups if g.get("is_unsolved_bulk")]

    def _recipient_bytes(g: dict) -> int:
        by_addr = {m["addr"]: m["bytes"] for m in g["members"]}
        return sum(by_addr.get(a, 0) for a in g["recipients"])

    transfer.sort(key=_recipient_bytes, reverse=True)
    bulk.sort(key=lambda g: sum(m["bytes"] for m in g["members"]), reverse=True)

    print(f"\nTop {min(args.limit, len(transfer))} transfer groups "
          f"(donor -> recipients), by recipient bytes:")
    for g in transfer[:args.limit]:
        donor = g["donor"]
        score_s = f"{donor['score']:.1f}%" if donor.get("score") is not None else "?"
        print(f"  key={g['shape_key'][:28]:<28} insns={g['insn_count']:<4} "
              f"recipients={len(g['recipients']):<3} bytes={_recipient_bytes(g):<6} "
              f"donor={donor['name']} ({donor['tier']}, {score_s}) {donor['source_path']}")

    print(f"\nTop {min(args.limit, len(bulk))} unsolved-bulk groups "
          f"(no donor, >={BULK_THRESHOLD} unported):")
    for g in bulk[:args.limit]:
        total_bytes = sum(m["bytes"] for m in g["members"])
        names = ", ".join(m["name"] for m in g["members"][:4])
        more = " ..." if len(g["members"]) > 4 else ""
        print(f"  key={g['shape_key'][:28]:<28} insns={g['insn_count']:<4} "
              f"members={len(g['members']):<3} bytes={total_bytes:<6} {names}{more}")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)

    p_build = sub.add_parser("build", help="Compute shape keys for every kb.json function")
    p_build.add_argument("--batch-size", type=int, default=DEFAULT_BATCH_SIZE)
    p_build.add_argument("--quiet", action="store_true")
    p_build.set_defaults(func=cmd_build)

    p_report = sub.add_parser("report", help="Print top transfer / unsolved-bulk groups")
    p_report.add_argument("--limit", type=int, default=20)
    p_report.set_defaults(func=cmd_report)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
