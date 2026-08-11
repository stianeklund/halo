#!/usr/bin/env python3
"""End-to-end regression test for the lifted zlib inflate path.

The cache decompressor (``cache_files_decompress_windows.c``) is the only
consumer of ``inflate()`` in the game, and it is reached exactly once per map
that is not already in the HDD cache -- uncached multiplayer maps and campaign
level advance.  A defect anywhere in the inflate chain therefore does not show
up until someone loads a fresh map, at which point the engine halts with
``decompression fucked up with error code (-3)``.

VC71 byte-match does not catch this class of defect: a missing ``DUMPBITS(14)``
in the TABLE state of ``inflate_blocks`` cost two instructions out of ~1000 and
still scored 96%, while making every dynamic-Huffman block undecodable
(shipped 2026-08-08, found 2026-08-11 from a live XBDM capture).

This test executes the *patched XBE* under Unicorn and drives the real
``inflateInit_`` / ``inflate`` / ``inflateEnd`` entry points over streams
produced by the host zlib, then compares the inflated bytes against the
original payload.  Every lifted function on the inflate path participates:

    inflate                 0x1155e0    inflate_blocks          0x113a90
    inflateInit_            0x1155c0    inflate_blocks_new      0x1139d0
    inflateInit2_           0x1154a0    inflate_blocks_reset    0x113930
    inflateEnd              0x115430    inflate_blocks_free
    inflateReset            0x1153c0    inflate_codes           (+ _new/_free)
    huft_build              0x115ba0    inflate_fast            0x114fa0
    inflate_trees_bits      0x116010    inflate_flush           0x116280
    inflate_trees_dynamic   0x1160c0    inflate_trees_fixed     0x116250
    adler32                 0x110a10    csmemcpy                0x8e0b0

Only ``zalloc``/``zfree`` are supplied by the harness (as hooked stubs), because
the engine's own allocators reach into XAPI heap state that is not present in a
bare image.  Everything else runs the shipped code.

Usage:
    rtk python3 tools/equivalence/test_inflate_roundtrip.py          # standalone
    rtk pytest tools/equivalence/test_inflate_roundtrip.py           # pytest
"""

from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path

from unicorn import (
    UC_ARCH_X86,
    UC_HOOK_CODE,
    UC_HOOK_MEM_UNMAPPED,
    UC_MODE_32,
    UC_PROT_ALL,
    Uc,
    UcError,
)
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

_REPO_ROOT = Path(__file__).resolve().parents[2]
PATCHED_XBE = _REPO_ROOT / "halo-patched" / "default.xbe"
PRISTINE_XBE = _REPO_ROOT / "halo-patched" / "cachebeta.xbe"

# Every lifted function on the inflate path, by original VA.  Each must still be
# redirected into our compiled section, and each must actually execute during the
# round trip -- otherwise this file would be testing Bungie's zlib, not ours.
INFLATE_PATH = {
    0x113930: "inflate_blocks_reset",
    0x1139D0: "inflate_blocks_new",
    0x113A90: "inflate_blocks",
    0x114FA0: "inflate_fast",
    0x1153C0: "inflateReset",
    0x115430: "inflateEnd",
    0x1154A0: "inflateInit2_",
    0x1155C0: "inflateInit_",
    0x1155E0: "inflate",
    0x115BA0: "huft_build",
    0x116010: "inflate_trees_bits",
    0x1160C0: "inflate_trees_dynamic",
    0x116280: "inflate_flush",
}
# inflate_trees_fixed (0x116250) only runs for fixed-Huffman blocks and
# inflate_codes/_new/_free have no stable single entry to hook here; they are
# still covered behaviourally by the payloads.

# --- entry points (kb.json addresses, absolute VAs into the debug build) ------
INFLATE_INIT_ = 0x1155C0
INFLATE = 0x1155E0
INFLATE_END = 0x115430
INFLATE_RESET = 0x1153C0

# zlib return codes
Z_OK = 0
Z_STREAM_END = 1
Z_NO_FLUSH = 0

Z_STREAM_SIZE = 0x38  # sizeof(z_stream) the version check insists on

# --- harness memory map (all well clear of the XBE's own 0x00010000..0x0400xxxx)
STACK_BASE = 0x70000000
STACK_SIZE = 0x00100000
HEAP_BASE = 0x60000000
HEAP_SIZE = 0x00800000
BUF_BASE = 0x50000000
BUF_SIZE = 0x00800000
STUB_BASE = 0x7F000000
STUB_SIZE = 0x1000

ZALLOC_STUB = STUB_BASE + 0x00
ZFREE_STUB = STUB_BASE + 0x10
MAGIC_RET = STUB_BASE + 0x20  # emulation stops when EIP reaches here

ZSTREAM = HEAP_BASE  # z_stream lives at the bottom of the heap
VERSION_STR = HEAP_BASE + 0x100
HEAP_ALLOC_BASE = HEAP_BASE + 0x1000


def _load_xbe_sections(path: Path) -> list:
    """Return [(vaddr, bytes)] for every section of an XBE image."""
    data = path.read_bytes()
    base = struct.unpack_from("<I", data, 0x104)[0]
    nsec = struct.unpack_from("<I", data, 0x11C)[0]
    sec_hdr = struct.unpack_from("<I", data, 0x120)[0] - base
    out = []
    for i in range(nsec):
        off = sec_hdr + i * 0x38
        vaddr = struct.unpack_from("<I", data, off + 0x04)[0]
        vsize = struct.unpack_from("<I", data, off + 0x08)[0]
        raw_off = struct.unpack_from("<I", data, off + 0x0C)[0]
        raw_size = struct.unpack_from("<I", data, off + 0x10)[0]
        blob = data[raw_off:raw_off + raw_size]
        if len(blob) < vsize:
            blob = blob + b"\0" * (vsize - len(blob))
        out.append((vaddr, blob[:vsize]))
    return out


class InflateMachine:
    """A Unicorn instance with the patched XBE mapped and inflate drivable."""

    def __init__(self, xbe_path: Path = PATCHED_XBE):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self._map_image(xbe_path)
        self._map_harness()
        self._install_stubs()
        self.heap_top = HEAP_ALLOC_BASE
        self.live = {}  # addr -> size, so a double free is visible
        self.faults = []
        self.executed = set()
        self.exports = self._load_exports()
        self._install_coverage()

    # -- setup ------------------------------------------------------------
    def _map_image(self, xbe_path: Path) -> None:
        if not xbe_path.exists():
            raise FileNotFoundError(
                f"{xbe_path} missing -- run tools/build/build.py first")
        # `build.py --target halo` refreshes only the PE; the XBE is produced by
        # the default target.  Testing a stale image silently scores the previous
        # build, which turns a real regression into a green run.
        pe = _REPO_ROOT / "build" / "halo"
        if pe.exists() and pe.stat().st_mtime > xbe_path.stat().st_mtime + 1:
            raise RuntimeError(
                f"{xbe_path.name} is older than build/halo -- re-run "
                f"`rtk python3 tools/build/build.py -q` (no --target) so the "
                f"patched XBE is regenerated before testing")
        # One flat RWX region covering every section keeps page alignment simple.
        sections = _load_xbe_sections(xbe_path)
        lo = min(v for v, _ in sections)
        hi = max(v + len(b) for v, b in sections)
        lo &= ~0xFFF
        hi = (hi + 0xFFF) & ~0xFFF
        self.uc.mem_map(lo, hi - lo, UC_PROT_ALL)
        for vaddr, blob in sections:
            if blob:
                self.uc.mem_write(vaddr, blob)
        self.image_range = (lo, hi)

    def _map_harness(self) -> None:
        for base, size in ((STACK_BASE, STACK_SIZE), (HEAP_BASE, HEAP_SIZE),
                           (BUF_BASE, BUF_SIZE), (STUB_BASE, STUB_SIZE)):
            self.uc.mem_map(base, size, UC_PROT_ALL)
        self.uc.mem_write(VERSION_STR, b"1.1.3\0")

    def _install_stubs(self) -> None:
        self.uc.hook_add(UC_HOOK_CODE, self._on_code,
                         begin=STUB_BASE, end=STUB_BASE + STUB_SIZE - 1)
        self.uc.hook_add(UC_HOOK_MEM_UNMAPPED, self._on_fault)

    def _install_coverage(self) -> None:
        """Hook the compiled entry of every lifted inflate function.

        The addresses in ``INFLATE_PATH`` are the *original* VAs.  Hooking those
        would under-report: `patch.py` redirects them with `push <impl>; ret`,
        but our own C calls its siblings directly (and `@<reg>` callees like
        huft_build are reached through a thunk), so an intra-module call never
        touches the original VA at all.

        Instead resolve each name in the export table of the compiled PE.  A hit
        there is unambiguous proof that *our* code executed; a name that is
        absent means the function is no longer built, and one that never fires
        means the round trip did not exercise it.
        """
        self.impl_entry = {}
        missing = []
        for va, name in INFLATE_PATH.items():
            # Some of these still carry their kb.json placeholder name; accept
            # either spelling so a later rename does not silently drop coverage.
            addr = self.exports.get(name) or self.exports.get("FUN_%08x" % va)
            if addr is None:
                missing.append(f"{name} (@0x{va:06x}) absent from PE exports "
                               f"-- not built, or renamed in kb.json")
                continue
            self.impl_entry[addr] = name
            self.uc.hook_add(UC_HOOK_CODE, self._on_impl_entry,
                             begin=addr, end=addr)
        if missing:
            raise AssertionError(
                "inflate path is not fully lifted into the patched XBE:\n  "
                + "\n  ".join(missing))

    @staticmethod
    def _load_exports() -> dict:
        """name -> runtime address, from the compiled PE appended to the XBE."""
        sys.path.insert(0, str(_REPO_ROOT / "tools" / "xbox"))
        from symbolize_exception import (  # type: ignore
            load_pe_symbols,
            runtime_base_from_original_xbe,
            normalize_export_name,
        )
        base, _src = runtime_base_from_original_xbe(PRISTINE_XBE)
        if base is None:
            raise RuntimeError("could not derive the appended-PE runtime base")
        index = load_pe_symbols(_REPO_ROOT / "build" / "halo", base)
        if index is None:
            raise RuntimeError("build/halo has no export table -- rebuild")
        return {normalize_export_name(s.name): s.address for s in index.symbols}

    def _on_impl_entry(self, uc, address, size, user_data):
        self.executed.add(self.impl_entry[address])

    # -- stub implementations ---------------------------------------------
    def _pop_cdecl(self, nargs: int) -> tuple:
        """Read a cdecl frame and return (retaddr, args...) without popping."""
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        raw = self.uc.mem_read(esp, 4 * (nargs + 1))
        return struct.unpack("<%dI" % (nargs + 1), raw)

    def _return(self, retaddr: int, value: int) -> None:
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4)  # cdecl: callee pops only RA
        self.uc.reg_write(UC_X86_REG_EAX, value & 0xFFFFFFFF)
        self.uc.reg_write(UC_X86_REG_EIP, retaddr)

    def _on_code(self, uc, address, size, user_data):
        if address == ZALLOC_STUB:
            retaddr, _opaque, items, isize = self._pop_cdecl(3)
            nbytes = items * isize
            addr = (self.heap_top + 0xF) & ~0xF
            self.heap_top = addr + nbytes
            if self.heap_top >= HEAP_BASE + HEAP_SIZE:
                raise MemoryError("harness heap exhausted")
            uc.mem_write(addr, b"\0" * nbytes)
            self.live[addr] = nbytes
            self._return(retaddr, addr)
        elif address == ZFREE_STUB:
            retaddr, _opaque, ptr = self._pop_cdecl(2)
            if ptr and ptr not in self.live:
                raise AssertionError(f"zfree of untracked pointer 0x{ptr:08x}")
            self.live.pop(ptr, None)
            # Poison so a use-after-free shows up as garbage, like the real
            # debug allocator's 0xFD fill.
            self._return(retaddr, 0)

    def _on_fault(self, uc, access, address, size, value, user_data):
        self.faults.append((access, address, size,
                            uc.reg_read(UC_X86_REG_EIP)))
        return False  # let it raise

    # -- calling ----------------------------------------------------------
    def call(self, func: int, *args: int) -> int:
        esp = STACK_BASE + STACK_SIZE - 0x1000
        for arg in reversed(args):
            esp -= 4
            self.uc.mem_write(esp, struct.pack("<I", arg & 0xFFFFFFFF))
        esp -= 4
        self.uc.mem_write(esp, struct.pack("<I", MAGIC_RET))
        self.uc.reg_write(UC_X86_REG_ESP, esp)
        try:
            self.uc.emu_start(func, MAGIC_RET, count=200_000_000)
        except UcError as exc:
            eip = self.uc.reg_read(UC_X86_REG_EIP)
            detail = ""
            if self.faults:
                a, addr, sz, feip = self.faults[-1]
                detail = f" (unmapped access at 0x{addr:08x} from eip 0x{feip:08x})"
            raise RuntimeError(
                f"emulation fault calling 0x{func:08x} at eip 0x{eip:08x}: "
                f"{exc}{detail}") from exc
        return self.uc.reg_read(UC_X86_REG_EAX) & 0xFFFFFFFF

    # -- z_stream accessors -----------------------------------------------
    def _wr(self, off: int, val: int) -> None:
        self.uc.mem_write(ZSTREAM + off, struct.pack("<I", val & 0xFFFFFFFF))

    def _rd(self, off: int) -> int:
        return struct.unpack("<I", self.uc.mem_read(ZSTREAM + off, 4))[0]

    def inflate_stream(self, compressed: bytes, out_cap: int,
                       in_chunk: int = 0, out_chunk: int = 0) -> bytes:
        """Run the full init/inflate-loop/end cycle and return the output.

        ``in_chunk``/``out_chunk`` of 0 mean "hand over everything at once".
        Small values force ``inflate`` through its LEAVE/resume paths, which is
        where bit-buffer and state-machine defects hide.
        """
        in_addr = BUF_BASE
        out_addr = BUF_BASE + 0x400000
        if len(compressed) > 0x400000 or out_cap > BUF_SIZE - 0x400000:
            raise ValueError("payload exceeds harness buffers")
        self.uc.mem_write(in_addr, compressed)
        self.uc.mem_write(out_addr, b"\xCD" * out_cap)

        self.uc.mem_write(ZSTREAM, b"\0" * Z_STREAM_SIZE)
        self._wr(0x20, ZALLOC_STUB)
        self._wr(0x24, ZFREE_STUB)
        self._wr(0x28, 0)  # opaque

        rc = self.call(INFLATE_INIT_, ZSTREAM, VERSION_STR, Z_STREAM_SIZE)
        assert rc == Z_OK, f"inflateInit_ returned {rc:#x}"

        out = self._pump(compressed, in_addr, out_addr, out_cap,
                         in_chunk, out_chunk)
        rc = self.call(INFLATE_END, ZSTREAM)
        assert rc == Z_OK, f"inflateEnd returned {rc:#x}"
        assert not self.live, f"inflateEnd leaked {len(self.live)} allocation(s)"
        return out

    def inflate_two_streams(self, first: bytes, second: bytes,
                            cap_a: int, cap_b: int) -> tuple:
        """init -> inflate(A) -> inflateReset -> inflate(B) -> end.

        This is the only path that reaches ``inflateReset``'s standalone body:
        the call inside ``inflateInit2_`` gets inlined by clang, so without a
        genuine stream reuse the function would be built, redirected, and never
        proven to work.
        """
        in_addr = BUF_BASE
        out_addr = BUF_BASE + 0x400000

        self.uc.mem_write(ZSTREAM, b"\0" * Z_STREAM_SIZE)
        self._wr(0x20, ZALLOC_STUB)
        self._wr(0x24, ZFREE_STUB)
        self._wr(0x28, 0)
        rc = self.call(INFLATE_INIT_, ZSTREAM, VERSION_STR, Z_STREAM_SIZE)
        assert rc == Z_OK, f"inflateInit_ returned {rc:#x}"

        self.uc.mem_write(in_addr, first)
        self.uc.mem_write(out_addr, b"\xCD" * cap_a)
        got_a = self._pump(first, in_addr, out_addr, cap_a, 0, 0)

        rc = self.call(INFLATE_RESET, ZSTREAM)
        assert rc == Z_OK, f"inflateReset returned {rc:#x}"

        self.uc.mem_write(in_addr, second)
        self.uc.mem_write(out_addr, b"\xCD" * cap_b)
        got_b = self._pump(second, in_addr, out_addr, cap_b, 0, 0)

        rc = self.call(INFLATE_END, ZSTREAM)
        assert rc == Z_OK, f"inflateEnd returned {rc:#x}"
        assert not self.live, f"inflateEnd leaked {len(self.live)} allocation(s)"
        return got_a, got_b

    def _pump(self, compressed: bytes, in_addr: int, out_addr: int,
              out_cap: int, in_chunk: int, out_chunk: int) -> bytes:
        in_pos = out_pos = 0
        guard = 0
        while True:
            guard += 1
            assert guard < 100_000, "inflate loop did not converge"
            in_left = len(compressed) - in_pos
            out_left = out_cap - out_pos
            give_in = in_left if in_chunk == 0 else min(in_chunk, in_left)
            give_out = out_left if out_chunk == 0 else min(out_chunk, out_left)
            self._wr(0x00, in_addr + in_pos)
            self._wr(0x04, give_in)
            self._wr(0x0C, out_addr + out_pos)
            self._wr(0x10, give_out)

            rc = self.call(INFLATE, ZSTREAM, Z_NO_FLUSH)
            in_pos = self._rd(0x00) - in_addr
            out_pos = self._rd(0x0C) - out_addr
            if rc == Z_STREAM_END:
                break
            if rc != Z_OK:
                msg_ptr = self._rd(0x18)
                msg = self._read_cstr(msg_ptr) if msg_ptr else "<none>"
                raise AssertionError(
                    f"inflate returned {rc - (1 << 32) if rc >> 31 else rc} "
                    f"({msg!r}) after {out_pos} bytes")
            if give_in == 0 and give_out == 0:
                raise AssertionError("inflate made no progress with no input left")

        return bytes(self.uc.mem_read(out_addr, out_pos))

    def _read_cstr(self, addr: int, limit: int = 128) -> str:
        raw = bytes(self.uc.mem_read(addr, limit))
        return raw.split(b"\0", 1)[0].decode("latin-1")


# --- payloads ---------------------------------------------------------------
def _payloads() -> list:
    """(name, raw, compressed) triples covering all three deflate block types."""
    import random

    rnd = random.Random(0xA10)  # fixed seed: reproducible payloads

    cases = []

    # Dynamic Huffman: structured text, the shape a real .map header takes.
    text = (b"scenario\0bitmap\0sound\0model\0shader_environment\0" * 400)
    cases.append(("dynamic_text", text, zlib.compress(text, 9)))

    # Stored blocks: incompressible random data at level 0.
    noise = bytes(rnd.randrange(256) for _ in range(70_000))
    cases.append(("stored_noise", noise, zlib.compress(noise, 0)))

    # Fixed Huffman: tiny payload, deflate picks the static tree.
    tiny = b"halo"
    cases.append(("fixed_tiny", tiny, zlib.compress(tiny, 9)))

    # Multi-block + window wrap: > 32 KiB of output with long back-references,
    # so inflate_fast's window-wrap copy and inflate_flush both run.
    mixed = bytearray()
    for i in range(300):
        mixed += b"chunk%04d:" % i
        mixed += bytes(rnd.randrange(256) for _ in range(64))
        mixed += b"A" * 300
    mixed = bytes(mixed)
    cases.append(("mixed_wrap", mixed, zlib.compress(mixed, 6)))

    # Distances beyond 32 KiB back and near-zero-length matches.
    runs = (b"\x00" * 40_000) + noise[:8_000] + (b"\x00" * 40_000)
    cases.append(("long_runs", runs, zlib.compress(runs, 9)))

    return cases


# --- the actual checks ------------------------------------------------------
def _check(name: str, raw: bytes, comp: bytes, in_chunk: int,
           out_chunk: int) -> set:
    machine = InflateMachine()
    got = machine.inflate_stream(comp, len(raw) + 64,
                                 in_chunk=in_chunk, out_chunk=out_chunk)
    if got != raw:
        first = next((i for i, (a, b) in enumerate(zip(got, raw)) if a != b),
                     min(len(got), len(raw)))
        raise AssertionError(
            f"{name}: inflate output differs at byte {first} "
            f"(got {len(got)} bytes, want {len(raw)})")
    return machine.executed


CHUNKINGS = [
    (0, 0),        # everything at once
    (1, 0),        # one input byte per call -- every NEEDBITS resume path
    (0, 1),        # one output byte per call -- every NEEDOUT/FLUSH path
    (13, 7),       # awkward co-prime chunking across both sides
    (4096, 1024),  # realistic streaming
]


def _check_reset() -> set:
    """Reuse one z_stream across two payloads, separated by inflateReset."""
    cases = {n: (raw, comp) for n, raw, comp in _payloads()}
    a_raw, a_comp = cases["dynamic_text"]
    b_raw, b_comp = cases["mixed_wrap"]
    machine = InflateMachine()
    got_a, got_b = machine.inflate_two_streams(
        a_comp, b_comp, len(a_raw) + 64, len(b_raw) + 64)
    if got_a != a_raw:
        raise AssertionError("stream_reuse: first stream mismatched")
    if got_b != b_raw:
        raise AssertionError("stream_reuse: second stream mismatched after "
                             "inflateReset")
    return machine.executed


def _cases():
    for name, raw, comp in _payloads():
        for in_chunk, out_chunk in CHUNKINGS:
            # One-byte-at-a-time over 70 KB of noise is needlessly slow and adds
            # no coverage the 13/7 case does not already give.
            if len(raw) > 20_000 and (in_chunk == 1 or out_chunk == 1):
                continue
            yield name, raw, comp, in_chunk, out_chunk


def test_inflate_roundtrip():
    """pytest entry point: every payload against every chunking."""
    covered = set()
    for name, raw, comp, in_chunk, out_chunk in _cases():
        covered |= _check(name, raw, comp, in_chunk, out_chunk)
    covered |= _check_reset()
    unreached = set(INFLATE_PATH.values()) - covered
    assert not unreached, (
        "these lifted functions never executed, so the run proves nothing "
        f"about them: {sorted(unreached)}")


def main() -> int:
    failures = 0
    covered = set()
    for name, raw, comp, in_chunk, out_chunk in _cases():
        label = f"{name} in={in_chunk or 'all'} out={out_chunk or 'all'}"
        try:
            covered |= _check(name, raw, comp, in_chunk, out_chunk)
        except (AssertionError, RuntimeError, MemoryError) as exc:
            failures += 1
            print(f"  FAIL  {label}: {exc}")
        else:
            print(f"  PASS  {label} ({len(raw)} bytes)")

    try:
        covered |= _check_reset()
    except (AssertionError, RuntimeError, MemoryError) as exc:
        failures += 1
        print(f"  FAIL  stream_reuse (inflateReset): {exc}")
    else:
        print("  PASS  stream_reuse (inflateReset)")

    unreached = set(INFLATE_PATH.values()) - covered
    print(f"\nlifted functions executed: {len(covered)}/{len(INFLATE_PATH)}")
    for name in sorted(INFLATE_PATH.values()):
        print(f"  {'HIT ' if name in covered else 'MISS'}  {name}")
    if unreached and not failures:
        failures += 1
        print(f"\n  FAIL  never executed: {sorted(unreached)}")
    print(f"\ninflate round-trip: {'FAILED' if failures else 'OK'} "
          f"({failures} failure(s))")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
