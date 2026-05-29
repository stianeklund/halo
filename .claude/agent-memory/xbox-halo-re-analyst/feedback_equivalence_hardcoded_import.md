---
name: equivalence-hardcoded-import-artifact
description: Unicorn equivalence diverges 100% when a lift calls an XAPILIB/kernel import via a hardcoded-address fn-ptr cast; this is a harness artifact, not a lift bug
metadata:
  type: feedback
---

When a lifted function calls an XAPILIB/Xbox-kernel import via a hardcoded-address
function-pointer cast (e.g. `((int(__stdcall *)(...))0x1d2518)(...)` for XGetLaunchInfo),
Unicorn equivalence (`unicorn_diff.py`) will diverge on 100% of seeds even with
`--allow-stubs`.

**Why:** The oracle's call to the import is a resolved relocation the harness knows how
to stub. The candidate emits a literal `call 0x1d2518` to an address that is unmapped in
the function-scoped Unicorn run, so `--allow-stubs` can't intercept it (it stubs by
recognized symbol name, not by literal address). The lifted side runs off into garbage:
the telltale signature is a huge `INSN_count` (e.g. 92021 vs oracle's 37), a corrupted
`ESP_delta` (≈ -buffer_size, the local frame never cleaned), and EAX equal to a stack
pointer instead of the real return.

**How to apply:** For non-leaf functions whose only divergence cause is a hardcoded-import
call, trust VC71 byte-match over equivalence. Equivalence is the *weak* signal for this
function class. Classify the diverged seed: if the mismatch is the INSN_count/ESP blowup
described above (harness mechanism) rather than the lift's own return value or memory
writes, it is spurious. The commit tooling gates on ABI audit, not equivalence, so this
does not block `needs_commit`. Confirmed on FUN_001911b0 (shell.obj, demo-disc launch
check) — VC71 95.1%, equivalence 100/100 diverged spuriously.

Related: [[reference_msvc_intrinsics]] (other harness-hostile call patterns).
