---
name: source-recovery
description: "Faithful source recovery for already-lifted Halo Xbox code — manifest-driven readability recovery in a mandatory risk order, with byte-match gates, parked failures, and one commit per category."
---

# Source Recovery

Use this skill only for already implemented (`ported: true`) and committed code.
The default contract is that compiled bytes do not change. Corrective fidelity
changes are opt-in and require binary evidence plus a behavioral oracle.

Usage: `/recover-source <src file | kb.json object> [--allow-risky]`

The manifest created by `tools/recovery/source_recovery.py` is the machine state.
Do not work from an untracked baseline or silently rewrite source files.

## Required sequence

```bash
R=tools/recovery/source_recovery.py; M=recovery/<file>.c.json
rtk python3 $R plan --source src/halo/<path>/<file>.c -o $M
rtk python3 $R capture $M --object build/<...>/<tu>.obj
rtk python3 $R ladder $M
rtk python3 $R set-status $M <item-id> applied
rtk python3 $R check $M --object <obj>
rtk python3 $R report $M
```

Add `--allow-risky` to `plan` only when expression or control-flow recovery is
explicitly authorized. Use `parked --reason <why>` for failed items.

1. Confirm the exact source path and clean-tree precondition, then plan and capture.
2. Work small, line-numbered debt items from the manifest.
3. Obtain binary evidence before changing behavior or assigning semantic names.
4. Work the ladder below in order, one category at a time.
5. Gate every small change before marking it applied.
6. Report all applied, parked, skipped, and failed work.

## Mandatory ladder

| # | Category | Skill | Gate |
|---|---|---|---|
| 1 | `comments` | `re-comment-capture` | byte-identical |
| 2 | `local-renames` | `local-var-cleanup` | byte-identical |
| 3 | `symbol-names` | `naming-confidence` | byte-identical |
| 4 | `const-enum` | `const-enum-recovery` | byte-identical and no new `[IMM-WARN]` |
| 5 | `struct-define` | `struct-recovery` + `struct-assert` | byte-identical and build passes |
| 6 | `offset-to-field` | `offset-to-struct` | VC71 gate and hazard scan |
| 7 | `expr-simplify` | `expr-simplify` | opt-in plus behavioral oracle |
| 8 | `control-flow` | `control-flow-cleanup` | opt-in plus behavioral oracle |

Categories may be skipped when inapplicable, never reordered. Without
`--allow-risky`, categories 7 and 8 remain untouched.

## Gates and session rules

- Neutral work uses `source_recovery.py check --mode neutral`; assertion metadata and COFF neutrality must remain unchanged.
- Near-neutral work also uses `vc71_regression.py check --threshold 0` and its category-specific detector.
- Risky work may accept a codegen delta only with `--mode corrective`, explicit opt-in, and a green behavioral oracle.
- Start from a clean tree; never mix recovery with an in-flight lift.
- Use one commit per category. Before each commit run `rtk python3 tools/recovery/check_category_purity.py <category> --staged`; exit 0 or the documented exit 2 is acceptable, exit 1 is not.
- Never change `@<reg>` annotations, `ported` flags, kb.json signatures, or build configuration.
- On a gate failure, revert only that unit and park it with a specific reason; continue independent items.
- Ratchet VC71 floors upward after productive work with `vc71_regression.py update --source <file>`.
- Preserve ABI, layout, evaluation order, side effects, and control-flow shape unless risky recovery is explicitly authorized.

## Delegation

OpenCode category workers must be sequential. Each worker receives one category,
the manifest path, the target leaf skill, and an explicit same-worktree rule.
Never run two category workers for the same TU in parallel because each category
commit is the next category's base.
