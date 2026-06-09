---
name: crash-symbolization-method
description: How to symbolize patched-XBE crash backtrace addresses (0x006xxxxx) to function names via the build PE export table; the stale-halo.map trap
metadata:
  type: reference
---

Crash backtraces from the running game use the PATCHED XBE address space. Three address ranges:
- `0x006xxxxx`–`0x007xxxxx` = OUR lifted clang code. The EXE (`build/halo`, ImageBase 0x400000) is rebased wholesale to **0x642000** and appended as `.text.patch` (XBE VA 0x643000). Mapping: **EXE_VMA = crash_addr - 0x642000 + 0x400000**.
- `0x001xxxxx`–`0x003xxxxx` = ORIGINAL unported XBE code (original .text at VA 0x12000). Still runs at its original VA.
- `0x800xxxxx` = Xbox kernel.

**NEVER use build/halo.map (stale, never regenerated).** Symbolize from the fresh build PE export table:
```python
import pefile, bisect
pe = pefile.PE('build/halo', fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
b = pe.OPTIONAL_HEADER.ImageBase  # 0x400000
exps = sorted((e.address+b, e.name.decode()) for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name)
# for a crash addr in 0x006xxxxx: vma = addr - 0x642000 + b; greatest export <= vma
```
~2973 exports. Saved a reusable copy at `/tmp/symbolize.py` (takes hex args).

**Unwinder lies — validate each frame is a real return-address.** A genuine return address is the instruction immediately AFTER a `CALL`. Disassemble `build/halo` at the EXE_VMA and check the preceding bytes are `e8 xx xx xx xx` (call rel32) or `ff` (call indirect). If the "frame" lands on a `js`/`test`/mid-instruction, the frame-pointer walk picked up a stale stack slot — that frame is bogus. (Seen: two captures of the SAME crash differed only in a bogus [1] frame; the real one was a return-after-CALL.)

**Export-boundary attribution skips non-exported functions.** A function with no export entry (e.g. `FUN_0003f5f0` follows `ai_update`) gets mis-attributed to the preceding export. Cross-check by reading the source/disasm at the resolved offset; if the body doesn't match, the real function is a non-exported one after the named export.

Ported-vs-original check per frame: look up greatest kb.json addr (hex-parsed) <= the original VA; read its `ported` flag. A frame in `0x001xxxxx` is original; one in `0x006xxxxx` is lifted. The lifted↔original call boundaries are where ABI bugs bite. See [[crash-reg-arg-shift-use-after-free]].
