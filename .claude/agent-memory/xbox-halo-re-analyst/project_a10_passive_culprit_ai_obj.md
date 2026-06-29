---
name: a10-passive-culprit-ai-obj
description: a10 grunt passive bug LOCALIZED to ai.obj (ai.c) by coordinator single-object toggle-bisect; supersedes the stale "no defect in the 13 / root is upstream LOS" conclusion. Direct perception writers are unported; mechanism is indirect via ai.obj's per-tick perception driver/feeders.
metadata:
  type: project
---

# a10 passive-engage: culprit narrowed to ai.obj (2026-06-26)

**CORRECTION to prior memory.** [[a10-passive-alertness-root-traced]] and [[a10-prop-deletion-path-faithful]] concluded "no static mis-lift in the 13 AI objects; root is upstream LOS input (biped aim)". That conclusion is now SUPERSEDED. A fresh coordinator toggle-bisect narrowed the regression to a SINGLE object: **ai.obj (src/halo/ai/ai.c)**. With all 12 other AI objects running our lift and only ai.obj reverted to original, grunts engage; with ai.obj on our lift, they stay passive. So the defect IS in ported ai.obj code, not upstream of AI scope. The two prior exhaustive body-audits failed to localize because they audited the wrong objects (perception/actor_looking/actors/props) and trusted a non-discriminating bisect metric (max_phantom_props read 3 in probe_allon, probe_A, probe_B alike).

**Empirical discriminator (CONFIRMED, same 256-record capture generation):** player-tracking prop's perception state `prop+0x24`:
- unpatched (original = behaviorally ALLOFF): prop+0x24 = **3-4**, actor+0x268 = 6-10, +0x6e = 3-7, +0x6a = 3 (engaged). bud+0x3a=900.
- patched / now (full ports incl ai.obj): prop+0x24 = **0** (never leaves initial state), +0x268=0, +0x6e=0, +0x6a=2 (passive). bud+0x3a=0.
- `prop+0x66` (acknowledge level) = **-1 in BOTH** builds -> the prior "+0x66=-1 = acknowledge never fired" lead is REFUTED as the discriminator. The prop never even advances from state 0; it's not an acknowledge/promote-to-3 race, it's that nothing drives 0->3.
- `prop+0x9c` (visibility) = 0 in BOTH (not the discriminator, consistent with prior).
Tool: inline python over artifacts/ai_regression/{actors,props}_{unpatched,patched,now}.bin, match player prop by trk(prop+0x18)==player, NOT by index. (focus_probe.py player_handle() gives the player handle.)

**Direct writers of the bug fields are UNPORTED (store instructions confirmed via capstone on halo-patched/cachebeta.xbe, XBE base 0x10000):**
- actor+0x268: `0x300d5 mov word[edi+0x268],0` and `0x30239 mov word[edi+0x268],ax` in 0x300b0 actor_situation_update_target_status (UNPORTED).
- prop+0x24 promote: `0x3341d mov word[esi+0x24],3` in 0x33330 actor_perception_become_acknowledged (UNPORTED); `0x36295 mov word[esi+0x24],di` in the per-tick dispatcher 0x355f0 (UNPORTED).
- (Base-pool trap avoided: 0x158cb/0x15973/... `mov word[esi+0x24]` in ported actor_looking 0x15880/0x15900/0x159d0 write a 0x44-byte LOOK-SPEC descriptor (param_2, memset 0x44), NOT the 0x138 perception prop. Disp 0x24 is ubiquitous — always trace the base.)
So the mechanism is INDIRECT: an ai.obj function feeds bad INPUT to, or mis-drives the invocation of, the unported perception state machine.

**Per-tick perception call chain (mapped):**
`0x41180 ai_update(void)` (ai.obj) -> `0x3f5f0` per-actor activation sweep (ai.obj, 88.4% VC71) -> `0x3ec80` perception tick (actors.obj, 86.4% — EXONERATED by single-obj bisect) -> `0x355f0` dispatcher (UNPORTED) + 0x303f0 situation (UNPORTED) + 0x32cb0 emotion (UNPORTED).
- 0x3f5f0 is the SOLE ported caller of dispatcher-owner 0x3ec80. Its gate: iterate actors via 0x59b10/0x59b50; if record+0xb!=0 -> actor_erase; else if `actor+0x6a > 0` -> FUN_0003ec80(actor_handle@<esi> = iter+0x14). **Body verified FAITHFUL vs disasm** (ai.c:25-54) — iterator, 0x6a gate, handle iter+0x14=ebp-8 all match. Not the bug at the control-flow level.

**RANKED ai.obj suspects (of the 35) given to coordinator for function-bisect:**
1. **0x3f5f0** (88.4%) — sole ai.obj driver of the perception tick; gatekeeper. Body faithful but 88.4% = residual codegen diff worth a live A/B (it decides WHICH actors get perception + passes the handle).
2. **0x413c0** (75.3%, lowest score on an AI-logic path; @esi/@edi reg-args ai_firing_pos_entry) — structural imperfection candidate.
3. **0x40570** (86.3%) and **0x41420** (89.8%) — sub-bar ai.c logic.
4. **0x40010 / 0x40280** (both 100%) — call 0x2fc20 get_perception_knowledge + 0x2fd10 compute_prop_weight (perception queriers); 100% so unlikely, but they directly consume perception state.
- 0x41590 ai_test_line_of_fire is **ported=false** and called only by grenade-aim 0x22ba0 — RED HERRING, not on perception path (line-of-FIRE not line-of-SIGHT).
- 0x40360 (100%) broadcasts unit_effect=**0** (not 3) via 0x3d430 — tangential intel, not the enemy-acknowledge promote.

**DECISIVE next experiment (read-only-blocked for analyst; hand to user):** function-bisect the 35 ai.obj ports — revert half, find the single function whose original-vs-lift flip toggles prop+0x24 0->3 on the player grunt. Start with 0x3f5f0. OR live-diff at the perception tick: compare the inputs 0x3ec80/0x355f0 receive (actor+0x9e LOS result, distance, the iterated handle) patched vs unpatched on a committed grunt. The unported dispatcher is identical code both builds, so a divergent prop+0x24 outcome means divergent INPUT produced by ported ai.obj.

**Tooling:** artifacts/ai_regression/disasm_range.py (capstone, VA-direct). vc71_scores.json keyed by FUN_xxxxxxxx under .scores. kb_meta.json .symbols[addr].status='ported' is authoritative (kb.json object grouping + per-fn 'object' field are unreliable/null). owner()-by-nearest-known-fn MISLABELS callsites in the 0x30xxx-0x36xxx unported region as 0x2fd10 — discount those.
