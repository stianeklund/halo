---
name: a10-ai-obj-pertick-writeset-clean
description: 2026-06-28 Argument/write-set diff of ALL ported ai.obj per-tick functions vs disasm — ZERO perception-gate/LOS writes anywhere in ai.obj; the 3 per-tick drivers (0x3f5f0/0x40570/0x413c0) are argument-faithful; this CONTRADICTS the ai.obj bisect and means the next step is bisect-liveness verification, NOT another upstream report
metadata:
  type: project
---

Task: data/argument-level diff (not control flow) of ported ai.obj per-tick functions vs 0x____
disasm, hunting any write/arg that would zero perception promotion (prop+0x24 stuck 0, prop+0x30=0).

## Scope correction (authoritative obj-by-narrowest-range from objects.csv)
ai.obj = 0x3F350-0x427C0. Of the coordinator's 4 targets:
- 0x3f5f0 ai.obj (per-tick driver), 0x40570 ai.obj, 0x413c0 ai.obj -> IN scope.
- 0x5de80 -> encounters.obj, 0x53680 -> ai_debug.obj (and ported=null) -> OUT of scope (identical
  in both bisect arms; controlled-for; NOT diffed).
- actor_activate 0x3ec80 -> actors.obj; dispatcher 0x355f0 + refresh 0x33440 -> actor_perception.obj.
  ALL controlled-for by the ai.obj-only bisect.

## RESULT: ai.obj per-tick code is argument/write FAITHFUL and writes NO perception field
- 0x3f5f0 (full disasm verified): only 3 stores, all to AI-globals header (g+4=g+6, g+6=0, g+3=0).
  GATE faithful: `CMP word [record+0x6a],0; JLE skip` == our `(int16_t)record[0x6a] > 0` (signed).
  HANDLE faithful: passes [EBP-0x8] = iter+0x14 to 0x3ec80 (== our `iter+0x14`); dead path
  actor_erase(iter+0x14,0) faithful. record (0x59b50 ret) vs iter+0x14 correctly distinct.
- 0x40570: deferred actor-CREATION queue (AI-globals+0x8b8 count, +0x8bc[] handles); creates actors
  via 0x3f030 + registers via 0x1b2b80; writes only [EBP-N] locals and +0x8b8 (count clear). NO
  perception/LOS/alertness write.
- 0x413c0: firing-position-entry builder (entry@<esi>); writes only entry struct fields
  [ESI+0x1/0x10/0x14/0x18/0x1c/0x20/0x24]; caller is only 0x41420 (firing path, not perception).
- SWEEP of ALL 35 ported ai.obj funcs in range for non-stack stores to hot offsets
  {0x38,0x66,0x63,0x26,0x6a,0x6e,0x268,0x9e,0x4e,0x270,0x610,0x60c,0x9c}: ZERO hits. No scaled/
  indexed/computed-offset stores missed either.

## CONTRADICTION (do not file a 4th "upstream" report)
Bisect says: revert ai.obj only -> grunts engage. But no ported ai.obj function writes any
perception-gate/LOS field, and the per-tick drivers are argument-faithful. The only remaining
ai.obj-local mechanisms are: (a) an ARGUMENT to an unported callee that the perception machine
consumes (distance/flags/mode) passed wrong — but the per-tick drivers pass only handle+gate, both
faithful; (b) the bisect was NOT live (toggle didn't take effect). Per "prove toggles live before
trusting verdict" + "deploys self-verify running build", the DECISIVE next step is to re-run the
ai.obj toggle-bisect WITH live verification (verify_toggles_live.py byte-classify after deploy,
confirm the ai.obj redirects are actually flipped), NOT another static upstream audit. If the
bisect IS live and reproduces, the mechanism is an ai.obj argument to an unported perception callee
that escaped the handle/gate check — re-examine 0x3f5f0's siblings that pass DISTANCE/POSITION args
(not just handles) into 0x416e0/0x33440 inputs. Cross-ref [[pertick-promotion-gate-355f0-33440]],
[[stageB-rate-actrtag-static-table]], [[a10-passive-culprit-ai-obj]].
