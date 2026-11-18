# Agent Instructions

These rules apply to every coding agent in this repository, including Claude,
OpenCode, and subagents. A local, untracked `CLAUDE.md` may symlink to this file.

## Mission

Recover Halo CE Xbox behavior faithfully and incrementally.

- **Binary is source of truth:** Target Halo Xbox debug build 2276 at
  `halo-patched/cachebeta.xbe` (MD5 `c7869590a1c64ad034e49a5ee0c02465`, version
  `01.10.12.2276`, Oct 12, 2001). `kb.json` addresses are absolute VAs for this
  binary. Never substitute a retail `default.xbe`.
- **Functional reimplementation:** Build C implementations and patch redirects
  into the original binary. Require equivalent logic, operations, ABI, layout,
  side effects, and control flow; byte identity is not always required.
- **Explicit unknowns:** Prefer an explicit unknown over a plausible guess.
  `field_<hex>` means an accessed but unproven field; `pad_<hex>[n]` means bytes
  never observed accessed. Use lowercase hex, no `0x`, at least two digits.
  Legacy `unk_` names may remain until their struct is otherwise edited.
- **Verified layouts:** Check field offsets in `src/types.h`; never guess. Split
  unknown arrays when evidence proves accessed offsets. Follow skill
  `naming-confidence`.
- **Small, faithful changes:** Keep changes reviewable. Separate logic changes
  from cleanup. Register every new `.c` file in `src/CMakeLists.txt`.

## Required Instruction Index

The linked files are binding repository instructions, not optional background.
Before acting, load every row matching the task. Start with the index when task
scope is unclear.

| Task | Required instructions |
|------|-----------------------|
| Any repository task | [`research-and-tooling.md`](docs/agent-instructions/research-and-tooling.md) |
| C source, `kb.json`, RE, lift, ABI, types, structs, or Ghidra | [`lift-implementation.md`](docs/agent-instructions/lift-implementation.md) |
| Source edit, build, test, hazard review, VC71, equivalence, or deployment | [`build-and-verification.md`](docs/agent-instructions/build-and-verification.md) |
| Commit, auto-lift, skill routing, subagent, or final report | [`commits-skills-and-reporting.md`](docs/agent-instructions/commits-skills-and-reporting.md) |

## Always-On Rules

1. Prefix every shell command with `rtk`, including each command in a chain.
2. Never read the same file twice in one task. Track files already read. After a
   successful edit, trust the edit result; after failure, reread only the
   affected range, at most 20 lines.
3. Never read `kb.json` directly. Query it with `rtk jq` only.
4. Never read or search generated/noisy directories unless explicitly asked:
   `build/`, `build_debug/`, `node_modules/`, `.git/`, `halo-patched/`,
   `__pycache__/`, and `dist/`.
5. Never read `*.log`; rerun the producing command with narrow output.
6. Scope first: identify exact paths, symbols, and line ranges. For source edits,
   inspect callers, target code, `kb.json` ABI, and current scoped diff first.
7. Before debugging any regression, crash, hang, assert, visual bug, wrong
   behavior, or build/deploy failure, run
   `rtk python3 tools/memory/prior_fixes.py "<symptom or target>"`.
8. Before any Ghidra MCP call, run
   `rtk python3 tools/audit/check_ghidra_mcp.py`; stop and alert the user if it
   fails.
9. Lifted code is C89. Declarations precede statements in each block.
10. Do not invent behavior, names, fields, types, or source. Preserve ABI,
    especially immutable `@<reg>` annotations. Never fake a match with undefined
    behavior, volatile shaping, dead stores, arbitrary barriers, or pragmas.
11. Run the narrowest meaningful validation. Source edits require the hazard
    scan described in `build-and-verification.md`; lift work requires the lift
    pipeline and ABI/type gates.
12. Use `/lift` for every new function port. Do not manually implement and
    commit new lifts outside that workflow.
13. Never name or path-reference external reference projects (other decomps,
    other builds) in code, comments, commit messages, skills, or docs. Required
    provenance disclosures live only in the evidence records `PROVENANCE.md`
    names. Tooling that needs them explicitly lives outside the repository.

## Skills

Skills are doctrine agents self-invoke. Treat `skill_router_hook.py`
recommendations as required; if routing misses a relevant skill, find it in
`.claude/skills/SKILLS.md`, and name loaded skills in subagent briefs. Tool
selection: skill `tool-reference`.
