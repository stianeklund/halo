---
description: Goal-mode auto-lift — run until N functions committed at >=90% VC71 or queue exhausted
subtask: false
---

Use `halo-re-lift` for lift rules and `halo-verify-debug` for validation gates.

**Goal:** Lift, verify, and commit as many Halo CE Xbox functions as possible at VC71 >=90%.

Argument: $ARGUMENTS

Parse from $ARGUMENTS (all optional):
- `--goal N` — commit this many functions before stopping (default: 20)
- `--stop-on-fail N` — stop after N consecutive failures (default: 3)
- `--dry-run` — skip commits (leave changes for manual review)

## /goal harness integration

**Immediately after parsing arguments**, register the session with the `/goal` harness so progress appears in the UI:

```
/goal {N} lift functions at >=90% VC71
```

Where `{N}` is the parsed `--goal` value (default 20). This must be the **first** action before any candidate selection or lifting. After each successful commit, call `/goal` again with the current committed count to update the harness progress bar.

**Usage combination:**
- `/goal-lift` — registers `/goal 20 lift functions at >=90% VC71`, then runs
- `/goal-lift --goal 50` — registers `/goal 50 lift functions at >=90% VC71`, then runs

## Stop conditions (first met wins)

1. `--goal N` committed functions have been committed at VC71 >=90%.
2. The selection queue has no remaining viable candidates after screening.
3. The repo cannot build or the verification pipeline is broken in a way unrelated to the current lift.
4. The same infrastructure failure occurs twice after applying the recovery steps below.

**Never lower the >=90% pass threshold. Never keep a failed lift in the tree.**

## Progress log

Keep a compact per-function log at `artifacts/auto_lift/goal_progress.md`:

```
| function | addr | source_file | screen_result | vc71 | action | reason |
```

- `action`: committed / skipped / reverted / infra-blocked
- Update after every function.

## Candidate selection

```bash
rtk python3 tools/llm_auto_lift.py select --limit 60 2>&1 | grep "auto-lift" | grep -v "prior_fail"
```

Preferred target areas (in order): `game_engine.obj`, `lruv_cache.obj`, `hud.obj`, `items.obj`, `input_xbox.obj`.

Avoid: `hs_runtime.c` (C99/VC71 violations unfixed), `prior_fail` candidates unless queue is otherwise empty.

## Pre-screen (skip immediately if any apply)

- Decompile contains `unaff_`, `in_EAX`, `in_ECX`, or similar register-arg artifacts
- Signature uses `__fastcall` or `__thiscall`
- Function is <=5 lines and only wraps one `FUN_` call
- Pattern resembles `condition ? "on" : "off"` / `"true"/"false"` + `error()` + `global_scenario_get()`
- Callees contain `unaff_` or register-arg artifacts
- Known structural cap:
  - Register args (@eax/@esi): ~65–80% cap → skip
  - Trivial tail-call wrappers: ~40% cap → skip
  - MSVC ternary/log scheduling cap: ~87% → skip
  - MSVC loop-unroll vs rep stosd: ~65–70% → skip

Fetch decompilation and callees before screening:

```python
import urllib.request
from urllib.parse import urlencode

addr = "0xADDR"
for endpoint in ["decompile_function", "get_function_callees"]:
    url = f'http://localhost:8089/{endpoint}?{urlencode({"address": addr})}'
    print(urllib.request.urlopen(url, timeout=30).read().decode())
```

## Lift procedure

For each viable candidate:

### Phase 1 — RE analysis + implementation (subagent)

Gather lightweight context in the orchestrator:
```bash
rtk jq '[.. | objects | select(.addr? == "0xADDR")] | .[0]' kb.json
```
Fetch decompilation via Ghidra MCP (`decompile_function`). Then spawn:
```
Agent(subagent_type="xbox-halo-re-analyst", prompt=<brief>)
```
The prompt must include: target address, decompilation output, KB entry JSON,
source file path, and these file-write instructions:
- Write the C89 implementation to the source file at the correct address-ordered position
- Update kb.json declaration if needed (conservatively)
- Update `tools/kb_reg_baseline.json` for any `@<reg>` annotations
- Run `rtk python3 tools/analysis/maintain.py <source_file>`
- Run `rtk python3 tools/audit/check_lift_hazards.py` and fix target-relevant hazards
- Report: RESOLVED_TARGET, Confirmed/Inferred/Uncertain, kb.json updates made

### Phase 2 — build + verify (orchestrator)

After the agent returns, ensure a delinked reference exists (export via
`mcp__ghidra-live__export_delinked_object` if missing), then run:
```bash
rtk python3 tools/lift_pipeline.py --target FUNCNAME --no-metadata-update --verify-policy auto
```

### Pass/fail decision

| VC71 score | Action |
|---|---|
| >=99% | Commit — byte-match sufficient |
| 90–98% | Commit — meets policy |
| 85–89% | Do NOT commit. One permutation attempt only if mapped delinked ref exists |
| 65–85% | Check structural cap. If capped → skip. One focused escalation allowed if not capped |
| <65% | Assume lift bug — revert unless there is a clear, cheap fix |
| No VC71 data | Treat as infra/build issue, not pass |

**Max 3 total attempts per function** (including re-delink, permutation, or focused escalation). After 3 failures → revert and skip.

### Per-function delink fallback (boundary artifact suspicion only)

1. Export per-function `.obj`:
   `mcp__ghidra-live__export_delinked_object` with `selection_mode=range`
2. Add to `objdiff.json`
3. Re-run:
   ```bash
   compare_obj.py <vc71_obj> <per_func_delink_obj> --function FUNCNAME
   ```
4. If corrected VC71 >=90% → commit. Otherwise revert and skip.

## Commit procedure

```bash
rtk git add -- <source_file> kb.json tools/kb_reg_baseline.json
rtk python3 tools/audit/generate_lift_commit.py --batch-name "FUNCNAME" > /tmp/commit_msg.txt
rtk git commit -F /tmp/commit_msg.txt
```

## Revert procedure

```bash
rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json
rtk git status --short
```

## Escalation

Escalate by re-running Phase 1 with `Agent(subagent_type="xbox-halo-re-analyst")`
using the same prompt and the analyst default model.
Revert the failed attempt first: `rtk git checkout -- src/ kb.json tools/kb_reg_baseline.json`

Escalate when:
- VC71 < 65% (control flow / structure wrong)
- ABI audit fails (calling convention reasoning)
- FPU-WARN (operand order)
- Build fails on second attempt (not a simple typo)

Do NOT escalate (revert+skip instead):
- Target has SEH prolog/epilog (not liftable with current tooling)
- Target has >3 register args (disqualified)
- Build fails on an unrelated file (repo state issue)

## Ghidra bridge recovery

If `localhost:8089` health check fails:
1. Restart bridges:
   ```bash
   /mnt/c/Users/stian/scoop/shims/python3.exe tools/ghidra/ghidra_mcp_bridge.py --port=8090 &
   .venv/bin/python3 tools/ghidra_live_mcp/server.py --port=8091 &
   ```
2. Re-run the failed command once.
3. If same failure repeats → stop and report infra-blocked.

## Final report

When stopping, print:
- Number committed
- Number skipped by screen
- Number reverted after failed verification
- Any infra blockers
- Best remaining candidate areas
- Path to progress log (`artifacts/auto_lift/goal_progress.md`)

## Usage

```bash
/goal-lift                    # run until 20 committed or queue exhausted
/goal-lift --goal 50          # run until 50 committed
/goal-lift --goal 10 --dry-run  # trial run, no commits
```
