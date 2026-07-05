#!/usr/bin/env python3
"""Synthetic, copyright-free Unicorn repro of the effects.c:585 assert.

Runs the REAL delinked `effects_start_on_first_person_weapon` (FUN_0009e0d0,
delinked/effects.obj) in Unicorn against a *hand-crafted* minimal state — no
core.bin, no game data — and proves that the cross-slot double-claim reaches the
engine assert:

    EXCEPTION halt in ...effects.c,#585: effect->local_player_index==NONE

Mechanism (see tools/xbox/fixtures/a10/effects-585-fp-slot-mismatch/README.md):
an effect attached to the first-person weapon still carries a non-NONE
local_player_index (claimed by an orphaned local slot) when a different slot
activates FP effects on the same weapon object.

The function's pre-assert callees are stubbed (all cdecl — caller cleans):
  data_next_index -> 0 then -1   (one effect in the pool)
  datum_get       -> &effect     (our synthetic record)
  tag_get         -> ignored     (result discarded before the +0x3c check)
  display_assert  -> OBSERVED     (assert reached iff line==0x249 && halt==1)
  system_exit/0x1029a0 -> OBSERVED (push -1; the fatal halt)
  FUN_0009d4e0    -> stubbed      (only reached on the non-asserting control path)

Default: bug case (effect->local_player_index = 0) => asserts.
--control: effect->local_player_index = -1 (NONE) => does NOT assert (clean ret).

Exit 0 = behaved as expected for the chosen mode; non-zero = unexpected.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import (
    UC_X86_REG_ESP, UC_X86_REG_EBP, UC_X86_REG_EIP, UC_X86_REG_EAX,
)

ROOT = Path(__file__).resolve().parent.parent.parent
OBJ = ROOT / "delinked" / "effects.obj"

# --- COFF constants for delinked/effects.obj (.text) -----------------------
TEXT_FILE_OFF = 0x64      # objdump -h: .text raw data file offset
FUNC_OFF = 0x1a80         # FUN_0009e0d0 section offset (symbol value)

# --- Unicorn memory layout -------------------------------------------------
CODE_BASE = 0x00400000
CODE_SIZE = 0x00010000
STACK_BASE = 0x00100000
STACK_SIZE = 0x00100000
STACK_TOP = STACK_BASE + STACK_SIZE
GLOB_PAGE = 0x005aa000    # holds the effect_data pointer @ 0x5aa8b0
EFFECT_DATA_PTR_ADDR = 0x005aa8b0
DUMMY_POOL = 0x00900000   # value stored at effect_data (stubs ignore it)
EFFECT_ADDR = 0x00800000  # our synthetic effect record
EFFECT_PAGE = 0x00800000
SENT_PAGE = 0x00700000    # sentinel page for stubbed callees + fake ret
FAKE_RET = SENT_PAGE + 0x60

# callee sentinels (each REL32 call is redirected here, then hook-dispatched)
S_DATA_NEXT = SENT_PAGE + 0x00
S_DATUM_GET = SENT_PAGE + 0x10
S_TAG_GET = SENT_PAGE + 0x20
S_ASSERT = SENT_PAGE + 0x30
S_HALT = SENT_PAGE + 0x40
S_D4E0 = SENT_PAGE + 0x50

# REL32 call sites (section offset -> sentinel target)
REL32 = {
    0x1a8d: S_DATA_NEXT,   # call data_next_index (initial)
    0x1b10: S_DATA_NEXT,   # call data_next_index (loop continuation, @0x1b0f)
    0x1aad: S_DATUM_GET,   # call datum_get
    0x1abd: S_TAG_GET,     # call tag_get
    0x1ae4: S_ASSERT,      # call display_assert
    0x1aeb: S_HALT,        # call system_exit/halt_and_catch_fire (0x1029a0)
    0x1b00: S_D4E0,        # disp of `call FUN_0009d4e0` at 0x1aff
}
# DIR32 sites (section offset -> absolute value)
DIR32 = {
    0x1a84: EFFECT_DATA_PTR_ADDR,  # mov eax, ds:effect_data (initial)
    0x1aa6: EFFECT_DATA_PTR_ADDR,  # mov ecx, ds:effect_data (loop body)
    0x1b09: EFFECT_DATA_PTR_ADDR,  # mov ecx, ds:effect_data (loop continuation)
}

WEAPON = 0xE5720165  # arbitrary FP-weapon object handle (just a number)


def read_text_section() -> bytearray:
    raw = OBJ.read_bytes()
    # section vsize from objdump: 0x31e1
    return bytearray(raw[TEXT_FILE_OFF:TEXT_FILE_OFF + 0x31e1])


def patch(code: bytearray) -> None:
    for off, tgt in REL32.items():
        disp = (tgt - (CODE_BASE + off + 4)) & 0xFFFFFFFF
        struct.pack_into("<I", code, off, disp)
    for off, val in DIR32.items():
        struct.pack_into("<I", code, off, val & 0xFFFFFFFF)


def run(lpi: int, verbose: bool) -> dict:
    code = read_text_section()
    patch(code)

    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(CODE_BASE, CODE_SIZE)
    uc.mem_map(STACK_BASE, STACK_SIZE)
    uc.mem_map(GLOB_PAGE, 0x1000)
    uc.mem_map(EFFECT_PAGE, 0x1000)
    uc.mem_map(SENT_PAGE, 0x1000)
    uc.mem_map(DUMMY_POOL & ~0xFFF, 0x1000)

    uc.mem_write(CODE_BASE, bytes(code))
    uc.mem_write(SENT_PAGE, b"\xCC" * 0x1000)
    uc.mem_write(EFFECT_DATA_PTR_ADDR, struct.pack("<I", DUMMY_POOL))

    # synthetic effect record: salt!=0, +4 tag id, +0x3c attached==WEAPON,
    # +0x4c local_player_index (int16)
    uc.mem_write(EFFECT_ADDR, b"\x00" * 0x100)
    uc.mem_write(EFFECT_ADDR + 0x00, struct.pack("<H", 0xE572))   # salt (in-use)
    uc.mem_write(EFFECT_ADDR + 0x04, struct.pack("<I", 0x12345678))  # tag id
    uc.mem_write(EFFECT_ADDR + 0x3c, struct.pack("<I", WEAPON))   # attached object
    uc.mem_write(EFFECT_ADDR + 0x4c, struct.pack("<h", lpi))      # local_player_index

    # cdecl args: [ret][arg1=local_player_index][arg2=weapon]
    esp = STACK_TOP - 0x40
    uc.mem_write(esp + 0x00, struct.pack("<I", FAKE_RET))
    uc.mem_write(esp + 0x04, struct.pack("<I", 1))         # activating slot = 1
    uc.mem_write(esp + 0x08, struct.pack("<I", WEAPON))
    uc.reg_write(UC_X86_REG_ESP, esp)
    uc.reg_write(UC_X86_REG_EBP, esp)

    st = {"dni": 0, "assert": None, "halt": None, "clean_ret": False, "trace": []}

    def do_ret(uc, eax=None):
        sp = uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", uc.mem_read(sp, 4))[0]
        uc.reg_write(UC_X86_REG_ESP, sp + 4)
        if eax is not None:
            uc.reg_write(UC_X86_REG_EAX, eax & 0xFFFFFFFF)
        uc.reg_write(UC_X86_REG_EIP, ret)

    def rd(uc, off):
        sp = uc.reg_read(UC_X86_REG_ESP)
        return struct.unpack("<I", uc.mem_read(sp + off, 4))[0]

    def hook_code(uc, addr, size, ud):
        if addr == S_DATA_NEXT:
            val = 0 if st["dni"] == 0 else 0xFFFFFFFF
            st["dni"] += 1
            if verbose: st["trace"].append(f"data_next_index -> {val:#x}")
            do_ret(uc, val)
        elif addr == S_DATUM_GET:
            if verbose: st["trace"].append(f"datum_get -> {EFFECT_ADDR:#x}")
            do_ret(uc, EFFECT_ADDR)
        elif addr == S_TAG_GET:
            if verbose: st["trace"].append("tag_get -> 0 (discarded)")
            do_ret(uc, 0)
        elif addr == S_D4E0:
            if verbose: st["trace"].append("FUN_0009d4e0 (claim path) -> void")
            do_ret(uc, 0)
        elif addr == S_ASSERT:
            line = rd(uc, 0x0c)
            halt = rd(uc, 0x10)
            st["assert"] = {"line": line, "halt": halt}
            if verbose: st["trace"].append(f"display_assert(line={line:#x}, halt={halt})")
            do_ret(uc)
        elif addr == S_HALT:
            st["halt"] = {"arg": rd(uc, 0x04)}
            if verbose: st["trace"].append(f"system_exit/halt(arg={st['halt']['arg']:#x})")
            uc.emu_stop()
        elif addr == FAKE_RET:
            st["clean_ret"] = True
            if verbose: st["trace"].append("clean ret (no assert)")
            uc.emu_stop()

    def hook_inval(uc, access, addr, size, value, ud):
        eip = uc.reg_read(UC_X86_REG_EIP)
        st["fault"] = (f"unmapped addr={addr:#x} size={size} access={access} "
                       f"eip={eip:#x} (sec_off={eip - CODE_BASE:#x})")
        uc.emu_stop()
        return False

    uc.hook_add(UC_HOOK_CODE, hook_code)
    uc.hook_add(UC_HOOK_MEM_INVALID, hook_inval)

    try:
        uc.emu_start(CODE_BASE + FUNC_OFF, 0, count=100000)
    except unicorn.UcError as e:
        st["error"] = str(e)
    return st


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--control", action="store_true",
                    help="effect->local_player_index = NONE (should NOT assert)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if not OBJ.exists():
        print(f"ERROR: missing {OBJ} (export the delinked effects.obj first)")
        return 2

    lpi = -1 if args.control else 0
    mode = "CONTROL (lpi=NONE)" if args.control else "BUG (lpi=0, claimed by slot 0)"
    st = run(lpi, args.verbose)

    print(f"== effects.c:585 repro — {mode} ==")
    if args.verbose:
        for t in st["trace"]:
            print(f"   {t}")
    if st.get("fault"):
        print(f"   memory fault: {st['fault']}")
    if st.get("error"):
        print(f"   emu error: {st['error']}")

    asserted = st["assert"] is not None
    if asserted:
        a = st["assert"]
        print(f"   display_assert reached: line={a['line']:#x} halt={a['halt']}"
              f"  halt-call={'yes' if st['halt'] else 'no'}")

    if args.control:
        ok = (not asserted) and st["clean_ret"]
        print(f"RESULT: {'PASS' if ok else 'FAIL'} — "
              f"{'no assert, clean return (expected)' if ok else 'unexpected assert/behavior'}")
        return 0 if ok else 1
    else:
        ok = asserted and st["assert"]["line"] == 0x249 and st["assert"]["halt"] == 1 \
            and st["halt"] is not None and (st["halt"]["arg"] & 0xFFFFFFFF) == 0xFFFFFFFF
        print(f"RESULT: {'PASS' if ok else 'FAIL'} — "
              f"{'reached effects.c#585 assert + system_exit(-1) (repro confirmed)' if ok else 'did not reproduce'}")
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
