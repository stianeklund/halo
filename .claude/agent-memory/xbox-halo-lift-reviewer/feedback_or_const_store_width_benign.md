---
name: or-const-store-width-benign
description: A mem-trace "disjoint write" at the same address with different store widths is benign by construction when the op is OR/AND/bit-set with a constant; do not require snapshot-specific upper-byte reasoning
metadata:
  type: feedback
---

When unicorn_diff `--mem-trace` reports a DISJOINT write pair (1 oracle-only, 1 candidate-only)
at the SAME address, the discriminating question is store WIDTH and OP, not value.

**Case adjudicated: 0x1a2290 obj+0x424 (object base 0x800bf3f8 -> 0x800bf81c).**
- Source: `*(int *)(unit_obj + 0x424) |= 1;` (faithful: original does dword RMW).
- Original disasm @0x1a23f2: `MOV ECX,[ESI+0x424]; OR ECX,1; MOV [ESI+0x424],ECX` = DWORD RMW.
- Candidate (clang .obj) @0x2657: `OR BYTE PTR [esi+0x424],0x1` = BYTE RMW.
- clang legally narrowed the dword `|=1` to a byte OR (high 3 bytes provably unaffected by
  ORing 0x1). mem-trace flags the width difference as a disjoint.

**Verdict: BENIGN BY CONSTRUCTION, holds for ANY upper-byte values (not snapshot-dependent).**
`OR [X],imm` only sets bits and never clears; the dword version round-trips the upper 3 bytes
it just read (single-threaded, no interleaving store between read and write); the byte version
never touches them. Final memory at 0x424..0x427 is byte-identical regardless of pre-existing
0x425/6/7 content. Read side is also byte both sides (orig 0x22af TEST byte; cand 0x24e8 TEST byte).

**Why this matters:** A prior memory justified this as benign "because 0x425/6/7 are zero in the
snapshot" — that is a WEAKER, snapshot-specific claim. The correct reasoning is general. Do not
downgrade a general invariant to a snapshot coincidence.

**Contrast — when width IS a real bug:** if the op were a plain MOV (store-through) and widths
differed, or OR/AND with a value that the narrower store can't represent, the narrow store leaves
stale upper bytes (oracle=dword,cand=byte) or clobbers neighbors (oracle=byte,cand=dword). Then
REJECT/NEEDS-MORE pending a lift fix. Benign requires same-final-value, which OR-with-const gives
for free. Always confirm the OP, not just the width.
