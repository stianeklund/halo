---
name: aw-escalator-300b0-prop24-gate
description: actor+0x268 awareness escalator = FUN_000300b0 actor_situation_update_target_status (UNPORTED, actor_perception.c); awareness is a pure switch on perception-prop+0x24 (target_type); the a10 cap is INDIRECT (prop+0x24 stuck 0, all direct +0x24 writers unported), root = ported ai.obj driver feeding the unported classifier.
metadata:
  type: reference
---

# actor+0x268 awareness escalator and its prop+0x24 gate (2026-06-27)

Settles deliverable: WHO writes actor+0x268 (awareness 0..10), and what gates it past 3/5.

## The escalator (CONFIRMED, binary)
- **FUN_000300b0 = `actor_situation_update_target_status`** (decl in kb.json). UNPORTED (ported=null/false).
- Source TU = `c:\halo\SOURCE\ai\actor_perception.c` (string reloc at obj off 0x5dc0).
- Resolves: `edi(actor) = datum_get(DAT_006325a4, [ebp+8] actor_handle)`; `esi(target/perception prop) = datum_get(DAT_005ab23c, [edi+0x270])`. So **esi+0xNN are perception-prop fields at *0x5ab23c**; the prop index lives at **actor+0x270** (and a paired handle at actor+0x26c, cleared to -1 when no target).
- THE store: VMA **0x30239** (obj off 0x5ed9) `mov WORD PTR [edi+0x268], ax`. +0x268 is a 16-bit WORD. The clear is 0x300d5 `mov word[edi+0x268],0`.

## The 3->5 (firing) gate is prop+0x24 (target_type), a pure switch (CONFIRMED)
`ax` written to +0x268 is computed by `switch(WORD prop+0x24)`, range 0..5 (table at obj 0x5f38; case 2 and case 3 SHARE the rich branch). Awareness by target_type:
- type 0 -> aw 0 (also clears actor+0x270/+0x26c = -1)
- type 1 -> aw 1
- type 2 OR 3 -> rich branch: prop+0x127!=0 ->2; elif prop+0x74!=0 ->11(0xb); elif prop+0x32>=2 ->10(0xa); elif prop+0x38 not in {0,1} ->7; elif (prop+0x122>2 OR fcomp prop+0x11c) ->8; else ->9
- type 4 -> 5 if prop+0xb8!=0 else 5 (setne+5; =>5 or 6)  [aw>=5 -> combat]
- type 5 -> prop+0x127!=0 ->2; else **prop+0xbb!=0 -> 3, ==0 -> 4** (neg/sbb/+4 idiom at 0x5e98-0x5ea5)
- default -> keeps [ebp-4] (a prior datum_get-derived value)

So **aw>=5 (actor_combat firing gate) REQUIRES prop+0x24 in {2,3,4}**. Types {0,1,5} cap below firing (5 maxes at aw 3/4). The escalator itself produces aw=3 ONLY via type5+prop+0xbb!=0 — but per live capture the broken state is type 0, upstream of that.

## Direct prop+0x24 writers — ALL UNPORTED (so bug is INDIRECT)
capstone/objdump on actors.obj, all bases verified = DAT_005ab23c (prop) via datum_get:
- 0x32c10 -> writes 5
- **0x33330** `actor_perception_become_acknowledged` -> writes **3** (combat promote) at 0x3341d; sole-region ported caller via ai.obj sweep
- 0x33440 -> writes 0
- 0x34970 -> writes 4
- **0x355f0** per-tick dispatcher -> writes `di` (variable) at 0x36295
All ported=null. Conclusion: the unported classifiers run ORIGINAL code; a divergent prop+0x24 outcome means a PORTED ai.obj fn fed bad input or mis-drove their invocation. Consistent with [[a10-passive-culprit-ai-obj]] single-object toggle-bisect (ai.obj is the culprit object).

## Live discriminator (from [[a10-passive-culprit-ai-obj]], same capture gen)
Player-tracking prop (trk=prop+0x18==player): broken **prop+0x24=0**, actor+0x268=0 (never promoted); stock prop+0x24=3-4, +0x268=6-10. Prompt's "caps at aw=3" is imprecise vs live 0 — both sit below aw>=5; re-pin live before quoting an exact number.

## Ranked ported, non-exonerated suspects feeding the gate (ai.obj)
Chain: 0x41180 ai_update -> 0x3f5f0 sweep -> 0x3ec80 perception tick (actors.obj, bisect-EXONERATED) -> 0x355f0/0x33330 (unported).
1. **0x3f5f0** (88.4% VC71, ai.c) — sole ported driver/gatekeeper of perception tick; decides WHICH actors perceive + passes the handle.
2. **0x413c0** (75.3%, lowest; @esi/@edi reg-args).
3. **0x40570** (86.3%) / **0x41420** (89.8%).
4. **0x40010** (100%) — consumes perception state (queriers 0x2fc20/0x2fd10).
EXCLUDED from suspects: 0x40280 and 0x3b410 (task-exonerated 10); 0x3ec80 (bisect-exonerated). actor_looking/LOS is EMPIRICALLY EXCLUDED — it was on the lift in both bisect arms; reverting ai.obj ALONE restored engagement.
Decisive next test (read-only-blocked for analyst): function-bisect the ~35 ai.obj ports, start 0x3f5f0, find the one flip that toggles player prop+0x24 0->3.
