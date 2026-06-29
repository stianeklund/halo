---
name: bink-decoder-236a4e-attract-crash
description: Crash EIP 0x236a4e in cachebeta.xbe attract mode = RAD Bink macroblock decoder FUN_002362b0 (UNPORTED); dest overrun one page past decode surface in high contiguous/framebuffer RAM
metadata:
  type: project
---

# Bink decoder crash 0x236a4e (attract/idle mode, cachebeta.xbe)

ACCESS_VIOLATION at EIP 0x00236A4E during attract-mode `.bik` playback (idle on main menu plays `bink\attract1.bik`/`attract2`/`attract3`/`credits.bik`).

## Containing function
- **FUN_002362b0** (UNPORTED original; NOT in kb.json — only the 4 BinkXXX import stubs 0x230390/0x230ff0/0x231220/0x231490 are). One large densely-packed function, frame `push ebp;mov ebp,esp;sub esp,0x424` at 0x2362b0; uses EBP as a frame ptr in the outer body but RELOADS `ebp=[ebp+0x18]` (=stride) inside the inner copy loops, so EBP is a general reg there.
- This is **RAD Bink** decode code: copyright string `"Bink\r\nCopyright (C) 1998-1999 RAD Game Tools, Inc."` sits at VA 0x23f4a1, immediately after the function's dispatch jump table.
- Callers: cdecl-with-ECX wrapper region 0x238071+, CALL sites 0x2380c8/0x23811b/0x238196/0x2381c7 (`add esp,0x28` cleanup = 9 stack args + ECX). Wrapper does `test ebp,0x100000` flag gate, rounds dims up to mult-of-8 (`lea eax,[edi+7];and eax,~7`).

## Crash instruction & mechanism
- `0x236a4e: 89 0c 2f   mov dword ptr [edi + ebp], ecx`  (bytes 89 0C 2F 89 confirm)
- Part of an **unrolled 16-byte-per-step block copy** (op0 handler at 0x2369d0): loads 4 dwords from ESI (src) with stride EBP, stores 4 dwords to EDI (dest) with stride EBP, advances both by `EBP*2` via `lea esi,[esi+ebp*2]` / `lea edi,[edi+ebp*2]`.
- Outer loop 0x2369b0 reads a **byte opcode stream** from `[ebp-0x190]` (`movzx ecx,byte[edx];inc edx;cmp ecx,9;ja exit;jmp [ecx*4+0x238028]`). 10 handlers (op0..op9) = Bink macroblock-type dispatch. Loop terminator = `[ebp+0x2c]` counter decremented by 8 at 0x237fb4.
- Fault addr = EDI+EBP = 0x83BC8EC0 + 0x140 = **0x83BC9000** (exactly page-aligned). EBP=0x140=320 = decode-surface row stride. EDI-ESI=0x70800=460800=640*720 (dest-vs-src plane offset).
- Registers EAX=1C1C1E1E EBX=1B1A1A1B ECX=10101010 EDX=43402310 are valid low-entropy luma pixel bytes; all four ESI loads succeeded (we faulted on the STORE). The loop is correctly copying valid data — the **destination ran one page past the end of the decode surface**.

## Why 0x83BC9000 is invalid
- 0x80000000-0x83FFFFFF = Xbox "contiguous memory" (uncached identity alias of physical RAM, `& 0x0FFFFFFF`). 0x83BC9000 -> physical 0x03BC9000 = **59.79 MB** = top-of-RAM region where GPU framebuffer / AGP aperture / video surfaces are carved out.
- Live xemu probe (128MB devkit, mem_limit=128) `xp /1xw 0x83bc9000` => **"Cannot access memory"** — the next page past the surface is genuinely unmapped even on a devkit. (Same probe: `*0x5a8d50`=0xCDCDCDCD uninit-fill => not in active gameplay, consistent with menu/attract.)

## Diagnosis status (HYPOTHESIS, not proven root cause)
- Pristine retail Halo does NOT crash idling at attract for most users, so this is environment/input-dependent, NOT a bug in the copy loop. Most likely upstream **decode params / surface dimensions / stride** or the **xemu-vs-real-devkit framebuffer layout** put the decode surface one page short, or the .bik dimensions don't match the allocated surface.
- Open question if it recurs: confirm which `.bik` (attract1/2/3/credits), the surface alloc size vs (stride 320 x height), and whether this is xemu-specific (framebuffer aperture placement). Need the live trap frame paused AT the fault to read [ebp+8] dest base, [ebp+0x18] stride arg, [ebp+0x2c] block count.

## Tooling note
- No Ghidra MCP this session; disassembled raw XBE directly with capstone (XBE parse: base@0x104, sec count@0x11C, sec hdr VA@0x120, 56-byte headers vaddr/vsize/raw_addr/raw_size at +4/+8/+12/+16). Contiguous phys = VA & 0x0FFFFFFF.
