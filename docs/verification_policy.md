# Verification Policy: Accepting Low Match %

This policy defines when a lift may be accepted with low structural match
percentages, using:

- structural match (`xdk_verify.py` / `compare_obj.py`)
- FPU warning signal (`[FPU-WARN]`)
- behavior checks (reference behavior harness + runtime checks)

Behavioral fidelity to Xbox is the primary goal. Match % is a signal, not the
goal by itself.

## Scope

Use this policy for lifted functions when:

- `xdk_verify.py` reports low match %
- there is uncertainty whether mismatch is scheduling noise vs real behavior drift
- `ENABLE_XDK_HYBRID` is being considered as a fallback

## Required Inputs

For each target function, capture:

1. Structural score: `P` from `compare_obj.py` / `xdk_verify.py`
2. FPU warnings: `W` from `xdk_verify.py --fpu-only`
3. Reference behavior test: `G` from an automated harness comparison
   (pass/fail)
4. Runtime behavior: `R` from Option 3 lane or equivalent targeted scenario
5. Build/ABI status: `B` from build + ABI audit gates

If any required input is missing, verdict is `UNDECIDED` (do not mark verified).

## Hard-Fail Gates

Reject (or keep unverified) regardless of match % when any are true:

- `B = FAIL` (build or ABI gate fails)
- FPU-sensitive function and `W > 0`
- Reference behavior test exists for the function and `G = FAIL`
- Runtime scenario is required for the subsystem and `R = FAIL`

FPU-sensitive means geometry/math/projection/normalization/camera/physics math
or any function with non-trivial x87 instruction blocks.

## Low-Match Acceptance Table

Define low match as `P < 50`.

| Range | Default verdict | Required conditions to accept |
|---|---|---|
| `P >= 50` | Acceptable | Hard-fail gates clear |
| `40 <= P < 50` | Conditional | Hard-fail gates clear + (`G=PASS` or `R=PASS`) |
| `25 <= P < 40` | Strict conditional | Hard-fail gates clear + `G=PASS` + `R=PASS` + callsite/disasm review note |
| `P < 25` | Reject by default | Only accept with explicit maintainer sign-off and documented binary-evidence rationale |

For `P < 25`, acceptance requires all of:

1. `W = 0` (or function is non-FPU)
2. `G = PASS` and `R = PASS`
3. Written explanation of why mismatch is structural noise (not behavior)
4. Explicit reviewer/maintainer approval in commit/PR notes

Without all four, keep function unverified.

## Distinguishing Noise vs Real Drift

Treat low score as likely **noise** only when most of these are true:

- same callee set (or semantically equivalent renamed callees)
- same branch structure intent (no missing/extra behavior branches)
- stack/local layout differences are explainable (allocator/scheduling only)
- no FPU operand-order warnings

Treat low score as likely **real drift** when any are true:

- early divergence in parameter/register usage that changes data flow
- altered call ordering with side effects
- changed boolean materialization that affects control flow
- FPU subtraction/cross-product/division operand direction warnings

## Standard Decision Procedure

For each target function:

1. Run structural compare and record `P`.
2. Run `--fpu-only` and record `W`.
3. Run/refresh reference behavior test for the function (`G`).
4. Run targeted runtime scenario (`R`) in Option 3 lane or equivalent.
5. Apply table + hard-fail gates.
6. Record one-line verdict in notes:
   - `ACCEPTED_LOW_MATCH`
   - `REJECTED_LOW_MATCH`
   - `UNDECIDED_MISSING_SIGNAL`

## Commands (RTK)

```bash
rtk python3 tools/verify/xdk_verify.py src/path/to/file.c --function FUN_XXXXXXXX
rtk python3 tools/verify/xdk_verify.py src/path/to/file.c --function FUN_XXXXXXXX --fpu-only
rtk <automated reference-behavior command>
rtk python3 tools/verify/verify_option3.py --target FUN_XXXXXXXX --skip-iso
```

## Automated Enforcement in lift_pipeline.py

`tools/lift_pipeline.py` now enforces this policy with:

- `--low-match-policy` (`strict` by default)
- `--low-match-threshold` (default `50`)
- `--low-match-behavior-both-below` (default `40`)
- `--low-match-reject-below` (default `25`)
- `--behavior-check-cmd` (non-interactive reference behavior command)

In strict mode, the run fails if `xdk_verify` data is unavailable.

## Policy on XDK_HYBRID

`ENABLE_XDK_HYBRID` is allowed as a tactical tool, not a verification bypass.

- Do not use hybrid compilation alone as acceptance evidence.
- Hybrid is acceptable when it improves codegen stability for known hard-match
  TUs, but function verification still requires this policy's signals.
- Prefer micro-scoped hybrid (small TU set) over broad hybrid rollout.

## Minimal Evidence Block (for commit/PR notes)

Use this template:

```text
Target: FUN_XXXXXXXX
P(match): NN.N%
W(FPU warnings): 0|N
G(reference behavior): PASS|FAIL|N/A
R(runtime): PASS|FAIL|N/A
Verdict: ACCEPTED_LOW_MATCH | REJECTED_LOW_MATCH | UNDECIDED_MISSING_SIGNAL
Rationale: <1-3 lines, binary-evidence grounded>
```
