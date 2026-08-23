# Agent + Model Routing Strategy (2026-08)

Design review of Claude Code + OpenCode agent/model routing for the RE/lift
workflows. Optimizes one question: **given the current task state, what is the
cheapest model + context strategy most likely to make useful progress, and when
should it change?**

## 0. Verdict up front

The Claude-side routing in `goal-lift.js` is already evidence-based and close to
optimal — the `M` table (haiku/opus-low/opus-high split), the opus effort ladder
(medium → xhigh → max), the park ledger with `tried_models`, `MAX_ESCALATIONS`,
and the budget floor were each landed after a measurement. The remaining Claude
gains are policy alignment and small trims, not redesign.

The real slack is on the OpenCode side: **everything runs on
`aiolos/gpt-5.6-terra`** — the default model, the `fast` agent, the `deep`
agent, `explore`, and all RE personas. There is no tiering at all. The
Luna/Terra/Sol hypotheses are plausible but **untested in this repo**; they
should be adopted as defaults *with instrumentation*, not as facts.

## 1. Current state (facts, verified 2026-08-07)

### Claude Code
- `goal-lift.js` model policy (single point of control):
  - `mechanical`: haiku-low — tool-run + parse (revert, permute-run, equiv-run,
    redelink, park, report).
  - `commit`: haiku-low — moved from opus after measuring ~4% of session spend
    on a 6-command script.
  - `extract`: opus-low — select + research. Deliberately NOT haiku: the brief's
    `pre_screen` verdict decides whether a target is lifted at all; small
    saving, expensive failure mode. Revisit only with a measured A/B.
  - `reason`: opus-high — lift + review (`xbox-halo-re-analyst`,
    `xbox-halo-lift-reviewer`).
  - `improve`: opus, effort ladder `medium → xhigh → max`, gated on: still
    below bar, not structurally capped, budget ≥ 120k remaining, ≤ 3
    escalations per run. Fable demoted to opt-in (`--improveModel fable`) —
    token-heavy, diversity value didn't justify default cost.
- Measured model facts (memory-backed):
  - Opus-low ≈ Sonnet-low cost with better results; Sonnet-high costs MORE
    than Opus-high (`feedback_opus_low_over_sonnet`).
  - Sonnet lift agents stall-loop under the 180s workflow watchdog
    (`feedback_workflow_sonnet_lift_stall_loop`).
  - Target selection by TU score history is 4x cheaper / 6x faster than queue
    rank; 3 proposed futility-prediction signals measured DEAD
    (`feedback_lift_target_by_score_history`).
- Four Claude agents: `xbox-halo-re-analyst` (first-pass lift),
  `vc71-match-optimizer` (score), `halo-source-recovery` (readability),
  `xbox-halo-lift-reviewer` (fail-closed gate). The score/readability split is
  deliberate and memory-backed (`feedback_readability_separate_from_score_work`)
  — do not merge.
- `auto-lift.md` + `docs/lift-policy.md` still describe a direct Opus → Fable
  escalation; `goal-lift.js` superseded this with the opus effort ladder.
  `lift-policy.md` itself notes goal-lift.js is canonical. Docs are out of sync.

### OpenCode
- `opencode.json`: default model terra; `fast` agent = terra (only
  `textVerbosity: low` — no cheaper tier); `deep` = terra-high; RE personas =
  terra, `variant: xhigh` on the heavy ones. Luna and Sol appear nowhere.
- OpenCode is NOT in the automated pipeline. `goal-lift` / `auto-session` run
  entirely inside Claude Code's Workflow tool and cannot route to OpenCode
  models. OpenCode serves manual sessions. (`/lift-mizuchi` was retired
  2026-08-23 — see `tooling-audit-2026-07-07.md`.)
- `.claude/` is the canonical tree; `.opencode/` is a synced copy
  (`docs/agent-content.md`).

## 2. Hypothesis evaluation

### Claude hypotheses
| Hypothesis | Verdict |
|---|---|
| Haiku: search/exploration/simple extraction | **Confirmed** for tool-run+parse and the commit gate (already landed). **Rejected** for research briefs — pre_screen accuracy is load-bearing; keep opus-low until an A/B measures haiku's pre_screen accuracy. |
| Sonnet medium/high: routine implementation | **Rejected.** Dominated on both axes: opus-low is cheaper-or-equal and better; sonnet stall-loops in the lift lane. Sonnet has no role in this repo's routing. The answer to "is escalating Sonnet reasoning worthwhile vs moving to Opus" is: no — skip Sonnet entirely. |
| Opus low/medium as first escalation | **Confirmed** — this is the current improve ladder's first rung (opus-medium). |
| Opus high: semantic reasoning + hard codegen | **Confirmed** — current `reason` tier. |
| Opus xhigh: very hard | **Confirmed** — ladder rung 2. |
| Fable high/xhigh: frontier cases | **Confirmed but narrow.** Reserve for: (a) fresh-perspective retry on a high-value target after the opus ladder is exhausted (via the improve-pass drain with `--improveModel fable`), (b) `/oracle` second opinions, (c) novel crash-class diagnosis and structural-cap adjudication. Never in the default loop. |

### OpenCode hypotheses (all untested here — adopt as instrumented defaults)
| Hypothesis | Verdict |
|---|---|
| Luna high: routine lifting, cleanup, compile/diff/permuter loops | **Adopt as default** for `fast`, `explore`, and mechanical loop driving. Currently these burn terra for zero reason if luna is adequate — this is the single biggest OpenCode saving available. |
| Luna xhigh/max: harder mechanical | **Adopt** as rung 2 before switching models — mirrors the opus effort ladder (same-model effort climb first, model switch second). |
| Terra high/xhigh: semantic/type/struct recovery, cross-function inference | **Adopt** — terra is today's proven workhorse; keep it on `xbox-halo-re-analyst`, `recovery-category`, `deep`. |
| Sol low/medium: difficult codegen/asm matching | **Adopt** as the OpenCode analog of `vc71-match-optimizer`'s lane. |
| Sol high/max: x87, ABI, pathological compiler behavior | **Adopt** as final rung, same triggers as Claude's xhigh/max rungs (FPU-WARN class, FCOM sense, frame-layout divergence). |
| Routing principle (mechanical: Luna→Luna+→Sol; semantic: Luna→Terra→Luna; codegen: Luna→Sol) | **Endorse with one change:** route by *failure class*, not by a linear ladder. Mechanical stall → same model, higher effort. Semantic uncertainty (unknown struct/typing, wrong control flow) → Terra, which returns compact typed facts, then Luna resumes. Codegen mismatch (score plateau with correct behavior, FPU/frame/imm diff classes) → Sol directly — Terra adds nothing there. |

Instrumentation requirement: the park ledger (`tools/lift/park.py`) already
records `{model, score, notes}` per attempt and drains model-by-model via
`--exclude-model`. Record OpenCode attempts under model ids
(`luna-high`, `terra-xhigh`, `sol-high`, …) in the same ledger. That makes the
ledger the cross-runtime A/B substrate: after ~50 attempts per tier, promote or
demote tiers on measured promote-rate per token, not on vibes.

## 3. Context economics — clean vs shared context rules

Rules, in priority order:

1. **Model switch ⇒ new clean-context subagent, always.** A model switch
   invalidates the prompt cache anyway, so re-reading a long context in the new
   model is strictly worse than a compact brief. Never switch models mid-context.
2. **Spawn a clean-context specialist when the needed input fits a brief**
   (≤ ~10k tokens: decompilation, kb.json entry, disasm window, score-context
   pack, current diff). This is the lift/optimize/review pattern already used —
   keep schema-forced structured returns (`schema:` option), never transcripts.
3. **Continue in the current context when** iterating on the same target, same
   model, same lane, within the 5-minute cache window — e.g. lever-tuning where
   the accumulated diff/score history IS the input.
4. **Orchestration is deterministic code, not model turns.** The Workflow-tool
   JS drivers cost zero model tokens for control flow. Drive campaigns via
   `/goal-lift`-style workflows, not via a long interactive chat on an expensive
   main model — a 1M-context Fable main session is the most expensive possible
   orchestrator.
5. **Return compact findings downward:** a Terra/Opus semantic consult returns
   {struct facts, prototypes, invariants} as structured output; the cheap agent
   (Luna/opus-low) applies them. Escalation is a *consult*, not a handoff of the
   whole task, unless the failure class says the cheap model's work is wrong at
   the structure level (VC71 < 65%).

## 4. Persisted knowledge (anti-rediscovery)

Already in place and load-bearing — protect these: Ghidra context cache
(`artifacts/auto_lift/context_cache/`), score-context packs
(`artifacts/score_context/`), `leaf_cache.json` (equivalence confidence),
park ledger (attempts, tried models, cap hypotheses, warm-start patches),
retrieval index + `prior_fixes.py`, the two-tier skill catalogue (on-demand
loading), delinked references, `kb.json` itself, and the auto-memory directory.

Gaps worth closing:
- **Per-rung outcome stats.** Aggregate the park ledger + failure records into
  win-rate-per-token by (model, effort, failure-class). This turns future
  routing changes into arithmetic. A ~50-line report script over existing JSON.
- **OpenCode results don't feed the ledger.** Fix by convention (record
  attempts with model ids as above) — no new infra needed.
- **Recipe-atlas hit routing.** When score-context classification matches a
  known rule id, the fix is mechanical — apply the lever at the cheapest tier
  (opus-low / luna) without entering the escalation ladder at all.

## 5. Deterministic routing/escalation policy (consolidated)

Per target, one policy for both runtimes (Claude tier / OpenCode tier):

```
attempt 1   lift            opus-high        / terra-xhigh
gate        VC71 >= bar     → commit lane
65–84, not capped, atlas rule matched
            → mechanical lever apply   opus-low  / luna-high   (no escalation charge)
65–84, not capped, no rule
            → optimizer rung 1         opus-medium / sol-medium
rung gain < 1pp → rung 2               opus-xhigh  / sol-high
rung gain < 1pp → rung 3               opus-max    / sol-max   (budget floor 120k, ≤3 targets/run)
still sub-bar   → park with cap hypothesis + warm-start patch
VC71 < 65% OR ABI fail OR FPU-WARN OR 2nd build fail
            → structure is wrong: one fresh-model re-lift
              (fable on Claude; terra→sol never — wrong lane; use the improve
               drain later), else revert+log
semantic uncertainty discovered mid-lift (unknown struct, wrong prototype)
            → pause, spawn semantic consult (opus-high / terra-xhigh),
              apply returned facts at the cheap tier, resume
[85,98] with delinked ref → permuter, not a bigger model
never escalate: SEH targets, >3 reg-args, unrelated build failures
```

Escalation triggers stay *realized-signal only* (zero score delta after a rung,
hard gate failures). Do not add predictive futility heuristics — three were
tested and measured dead.

## 6. Recommended agent sets

### Claude (unchanged set, one trim)
| Agent | Model/effort | Note |
|---|---|---|
| xbox-halo-re-analyst | opus-high | unchanged |
| vc71-match-optimizer | opus, effort from ladder rung | unchanged |
| halo-source-recovery | opus-medium default | readability lane doesn't need high; gate catches errors |
| xbox-halo-lift-reviewer | opus-**medium** (trial) | fail-closed classification, not open-ended reasoning; A/B against high on ~20 reviews before adopting |

No merges: score vs readability separation is memory-backed. No removals. No
additions — generic workflow agents cover mechanical stages.

### OpenCode (retier — the actual change)
| Agent | Now | Recommended |
|---|---|---|
| default model | terra | **luna** (mechanical default; escalate per policy) |
| fast | terra | **luna-low/medium** |
| explore | terra | **luna** |
| deep | terra-high | **remove** — redundant once personas are tiered; or alias to terra-high |
| xbox-halo-re-analyst | terra-xhigh | terra-xhigh (keep) |
| recovery-category | terra-xhigh | terra-**high** (ladder categories are gated; xhigh only for expr/control-flow) |
| vc71-match-optimizer | terra-xhigh | **sol** (medium base, high/max rungs) |
| xbox-halo-lift-reviewer | terra-xhigh | terra-high |
| oracle | terra-xhigh | **sol-max or terra-xhigh** — keep clean-context, briefing-only |

Also delete `opencode.json~` from the tree.

## 7. auto-lift / goal-lift changes

1. **Align docs to one policy.** Rewrite `auto-lift.md` escalation and
   `docs/lift-policy.md` §Escalation-flow to the §5 policy above (opus effort
   ladder first, fable only in the improve drain). Today they contradict
   `goal-lift.js`.
2. **Atlas-rule short-circuit** in goal-lift's [65,85) branch: classification
   rule-id match → `M.extract`-tier lever application before charging an
   escalation slot.
3. **Ledger-driven improve drain** already excludes tried models — extend the
   convention to OpenCode model ids so manual sessions and goal-lift
   share one attempt history.
4. **Reviewer A/B** (opus-medium vs high) behind a `--reviewEffort` flag,
   measured on false-accept/false-reject over one session.

## 8. Cache/token-efficiency improvements

- Keep agent system prompts stable (they are the cacheable prefix per agent
  type); keep skills on-demand — already done.
- Batch same-model stages; the workflow already groups mechanical stages on
  haiku. Don't interleave model switches inside a phase.
- Campaign driving belongs in workflows/cron, not interactive chat on a
  Mythos-class main model. When a main session must supervise, sleep past the
  cache TTL deliberately (≥ 20 min wakeups) rather than polling.
- The single biggest available saving is OpenCode `fast`/`explore`/default on
  luna instead of terra — every mechanical OpenCode turn currently pays the
  flagship-tier price.

## 9. What NOT to do

- Don't merge vc71-match-optimizer + halo-source-recovery (memory-backed ban).
- Don't reintroduce Sonnet anywhere.
- Don't make Fable a default rung anywhere.
- Don't hand-tune OpenCode tiers further without ledger data — the Luna/Sol
  mappings are hypotheses until ~50 recorded attempts per tier exist.
- Don't add predictive futility signals — measured dead; realized signals only.
