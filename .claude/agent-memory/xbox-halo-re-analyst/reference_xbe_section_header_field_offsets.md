---
name: xbe-section-header-field-offsets
description: Correct XBE section-header field byte offsets for raw-capstone VA->file-offset mapping (a trap that silently decodes the wrong function)
metadata:
  type: reference
---

When disassembling cachebeta.xbe directly with capstone (no Ghidra), the VA->file
offset MUST use the correct XBE section-header layout. Each section header is 0x38
bytes; the table pointer is at header+0x120 (a VA, so subtract image base 0x10000).

Per-section field offsets (this is the trap — get these wrong and you decode a
DIFFERENT function that still disassembles cleanly, so it looks plausible):
  +0x00  Flags
  +0x04  Virtual Address   (VA)
  +0x08  Virtual Size
  +0x0C  Raw Address       (file offset)   <- map here
  +0x10  Raw Size
Image base at header+0x104 (0x10000). nsec at header+0x11c.

va_to_off(va): for each section, if vaddr <= va < vaddr+vsize: return raw + (va - vaddr).

Validation habit: read the known float constant for the target and confirm it (e.g.
0x28c038 = 0xb58637bd = -1e-6f). If a "string" push decodes to code bytes or a const
reads as code, your field offsets are wrong. I initially used +0x0c/+0x10/+0x14
(wrong) and decoded an unrelated assert-heavy function at "0x106dc0" before fixing.
XBE_PATH used by audit tools: halo-patched/cachebeta.xbe (pristine, unpatched).
