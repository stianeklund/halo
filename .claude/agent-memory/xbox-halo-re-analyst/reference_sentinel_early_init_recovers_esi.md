---
name: sentinel-early-init-recovers-esi
description: Early-init `result=-1` before the branch DOES force the callee-saved-ESI -1 sentinel (corrects "not forceable"); per-fn delinked ref must be zero-padded 8-hex
metadata:
  type: reference
---

Two findings from lifting network_game_client_get_local_machine_index (0x12a690,
network_game_globals.obj), a trivial getter that returns `(short)*(char*)(machine+0x40)`
or -1 on null.

## Early-init sentinel DOES recover the callee-saved-ESI pattern (corrects prior memory)

The [[nested-call-interleave-recovery]] note claims the "callee-saved-reg sentinel
(OR ESI,-1)" is "NOT forceable from C, accept ~2-3 insn cap." That is WRONG for the
common early-return-sentinel case. When the original materializes the -1 return in a
callee-saved reg held across a CALL (`push esi; or esi,-1; ...call...; mov ax,si; pop esi`),
writing the C as an **early-init local overwritten in the success path** reproduces it exactly:

```c
short f(void) {
  short result;
  result = -1;                       /* init BEFORE the branch */
  if (client != NULL) {
    void *m = get_machine(client);   /* CALL between init and use */
    if (m != NULL) result = (short)*(char*)((char*)m + 0x40);
  }
  return result;                     /* held in esi across the call */
}
```

vs. the naive `return -1;` at the tail (materializes -1 in AX at the return site).
This took VC71 78.6% (12/16) -> 100.0% (16/16). Clang allocates `result` to ESI across
the call precisely because it is live across a CALL and reused after. The cap only holds
when the sentinel is NOT a value live across a call (e.g. a scratch OR CL,1 flag).
Lever: whenever the reference has `push esi/or esi,-1` and you `return -1` at the tail,
switch to an early-init result variable.

## Per-function delinked ref filename MUST be zero-padded 8-hex

`vc71_verify._per_function_ref` matches `FUN_([0-9a-f]{8})` and looks for
`delinked/functions/<hex8>.obj` — e.g. `0012a690.obj`, NOT `12a690.obj`. The prefetch
stage and `export_delinked_object` write the 6-digit form `12a690.obj`, which vc71_verify
silently ignores ("No existing delinked reference contains: FUN_..."). Fix: `cp
delinked/functions/12a690.obj delinked/functions/0012a690.obj`. Also: after adding a NEW
function to a TU, run vc71_verify with `--no-cache` once — the SQLite compile cache holds
the pre-add object and reports "not found in both objects".
