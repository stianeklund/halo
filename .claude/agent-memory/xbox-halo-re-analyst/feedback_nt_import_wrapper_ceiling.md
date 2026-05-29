---
name: nt-import-wrapper-ceiling
description: Small NT/kernel-import wrappers (RtlInitAnsiString/NtOpenFile/NtClose etc.) hit a permanent ~45-65% VC71 ceiling; instruction diff, not score, decides correctness
metadata:
  type: feedback
---

Small XAPILIB/CRT functions that are thin wrappers around Xbox NT kernel imports
(RtlInitAnsiString, NtOpenFile, NtClose, NtQueryDirectoryFile, ...) have a permanent
VC71 structural ceiling well below 90% — observed 45% (reg-normalized) on the
~30-instruction FUN_001d7cb4 delete-tree open wrapper.

**Why:** three unavoidable, no-C-lever divergences:
1. Import call shape: the original XBE calls kernel imports via the indirect IAT
   thunk `CALL [0x00253xxx]` (objdump shows `call *0x0` with a reloc). VC71's
   standalone .obj emits a direct relocatable `call`. Every import call mismatches.
2. MSVC zero-init idiom: `oa.RootDirectory = 0` compiles to `AND $0x0, mem`
   (`andl $0x0,-0xc(%ebp)`) in the original, but clang/VC71-candidate emits
   `MOV $0, mem`. One mismatch per zeroed field.
3. Load/push scheduling: `MOV [EBP+8],%eax; PUSH %eax` vs `PUSH [EBP+8]` for the
   same argument; MSVC reorders OBJECT_ATTRIBUTES field stores for pipeline
   scheduling. Cosmetic.

**How to apply:** For these wrappers, do NOT chase the score by reshaping control
flow — it trades a faithful lift for a byte-match illusion. The discriminating
check is a localized instruction diff (`compare_obj.py -f <fn> --show-diffs
--reg-normalize`) against the real binary disassembly. If every *offset, argument,
store, and control-flow edge* matches and only call-shape / zero-init-idiom /
scheduling differ, it is a ceiling, not a bug — accept and route confidence
through equivalence (same accepted pattern as the SEH wrappers in CLAUDE.md).
Report the score with "source unchanged; ceiling not regression" so a lower
reg-normalized number isn't misread as a backslide.

**Param-slot reuse pattern (faithful lift technique):** MSVC reuses the single
`[EBP+8]` stack param slot — on entry it is the filename pointer (passed to
RtlInitAnsiString), then NtOpenFile receives `&param` (LEA EAX,[EBP+8]; PUSH EAX)
as the FileHandle OUT arg, overwriting the slot with the opened handle; later
calls PUSH [EBP+8] read the handle back. Lift this by taking `&name` (forces clang
to spill the param to one addressable slot) and reusing `name` after the open —
a separate HANDLE local adds a frame slot and diverges from the original layout.

Related: [[feedback_thunk_signature_propagation]] for the decals.c thunk-conflict
class of pre-existing build breakage.
