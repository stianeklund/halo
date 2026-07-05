---
name: ghidra-byte-mask-guard-silent-skip
description: Ghidra `& 0xFFu` mask-chain on an int guard condition is a silent-skip bug when the disasm shows a full-dword CMP; masks 0x8000-class sizes to 0. VC71 % hides it (LCS aligns the byte-test vs CMP away as ~shape gap).
metadata:
  type: reference
---

A repeated `(x & 0xFFu) & 0xFFu ... != 0` chain emitted by Ghidra on a guard
condition is NOT harmless shape. When the underlying variable is an int-sized
value (a size, count, or handle) the low-byte mask can flip the guard:
`0x8000 & 0xFF == 0`, `0x1900 & 0xFF == 0`, `0x100 & 0xFF == 0`. If the original
disassembly guards with a full-width `MOV EAX,[slot]; CMP EAX,EBX; JZ` (EBX==0),
the correct C is `if (x != 0)`. Transcribing the mask verbatim makes the guarded
block silently skipped for any value whose low byte is zero.

**Why VC71 % does not catch it:** both sides are VC71 codegen, but the delinked
reference is the ORIGINAL (full CMP) and our lift compiles the byte test — that
IS part of the structural gap, but LCS aligns most of the surrounding body so it
shows only as a few-percent dip, never as a flagged defect. Not a LOADW/FPU/IMM
warning class. Only a disasm read finds it.

**Reviewer check:** any surviving `& 0xFF` chain on a guard → open the disasm at
that JZ/JNZ and confirm the compare is byte-width (`TEST AL` / `MOVZX`), not
`CMP EAX,...`. Asymmetry is the tell: sibling guard uses plain `!= 0`, this one
has the mask.

**Case — network_connection_new 0x1296b0 (network_connection.obj), 88.8% VC71,
REJECTED 2026-07-05:** clientside-client path sets reliable_size=0x8000; lift's
`if ((reliable_size & 0xFF)... != 0)` → `0x8000 & 0xFF == 0` → the incoming-
reliable circular queue at connection+0x10 is never allocated for a client.
Original @0x12988a does `MOV EAX,[EBP-4]; CMP EAX,EBX; JZ` (full dword) →
allocates it. Server path (reliable_size=0) unaffected, so it hides on the
server oracle. Sibling unreliable guard was correctly written `!= 0`. See also
[[loadw-detector]] (different class: field load width, not guard truncation).

**RESOLVED 2026-07-05 (commit 56e635f0):** network_connection_new (0x1296b0)
re-lifted with the full-dword guard `if (reliable_size != 0)` (no `& 0xFF`),
matching the original `MOV EAX,[EBP-4]; CMP EAX,EBX; JZ` @0x12988a verified in
disasm. Also fixed 4 stale callee decls it needs (create_endpoint_set,
get_next_endpoint_from_set, FUN_00083bd0, FUN_000843a0 — all were void(void),
now carry their real cdecl args/returns). VC71 88.8% (180/194) — the function's
structural ceiling, same % as the rejected attempt but now behaviorally correct.
ported=true, build + hazards + ABI audit clean.
