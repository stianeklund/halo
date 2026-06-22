---
name: consult-oracles
description: >-
  Consult a clean-context oracle for expert second opinions when the user says
  oracle, consult oracles, ask the experts, or needs hard debugging,
  architecture, performance, or validation help.
---

# Consult Oracle

Use this skill when the user asks to consult an oracle, ask the experts, get a
second opinion, validate an approach, or solve a difficult debugging,
architecture, security, or performance problem.

The oracle must get an independent, evidence-first briefing. Do not ask it to
rubber-stamp the current session's theory.

## Oracle Choice

- Prefer the project `oracle` subagent when available. It is configured as a
  clean-context oracle in `.opencode/agents/oracle.md`.
- If the `oracle` subagent is not available in the running session, use the
  most independent available subagent, usually `general`, and provide a fully
  self-contained prompt.

After adding or changing this skill or agent definition, the user must quit and
restart OpenCode before the running session can see newly added subagents or
skill metadata.

## Workflow

1. Gather narrow evidence relevant to the user's question.
2. Write a neutral briefing document and show it to the user for correction.
3. Wait for explicit user approval.
4. Send the exact briefing text to the oracle subagent.
5. Relay the oracle verdict verbatim, then add a short evidence-based synthesis
   only if useful.

## Evidence Collection

For git repositories, collect the smallest useful current-state evidence:

```bash
rtk git status --short
rtk git log --oneline -20
rtk git diff HEAD -- <relevant paths>
```

For this repo, prefer scoped paths such as `src/`, `kb.json`, and
`CMakeLists.txt`. Do not include generated artifacts, logs, secrets, or broad
unrelated diffs.

Collect targeted additional evidence based on the question:

- Function or symbol mentioned: search for it and read the definition plus key callers.
- Build or test failure: include the exact command and failure output.
- `kb.json` entry: query it with `rtk jq`; do not read the whole file.
- Specific file mentioned: read only the implicated line range.
- Architecture question: include relevant modules, entry points, constraints, and file paths.
- Security/performance question: include inputs, assumptions, measured output, and affected code paths.

Redact credentials, tokens, private keys, and unrelated proprietary material.
If redaction would change the technical question, ask the user how to proceed.

## Briefing Template

Show this briefing to the user before sending it:

```markdown
# Oracle Briefing

## Project context
<2-3 factual sentences about the project, language, toolchain, and relevant subsystem.>

## The question
<Paste the user's question verbatim. Do not rephrase.>

## Current situation
<Observed facts only: symptom, failing command, regression range, decision point, or behavior seen.>

## Constraints
<Hard constraints: ABI, compatibility, build system, performance budget, security boundary, review policy, deadline. If none are known, say so.>

## Evidence

### Git status
<raw output or "not applicable">

### Recent commits
<raw output or "not applicable">

### Current diff
<raw output, scoped to relevant paths, or "no uncommitted relevant changes">

### Additional evidence
<One labeled section per item, with raw output and file/line references.>

## Oracle instructions
You are an independent expert evaluator with no prior context for this investigation.

1. Read all evidence before forming a view.
2. Answer the question directly and specifically.
3. Challenge unsupported assumptions explicitly.
4. Cite file paths, line numbers, diff hunks, commands, or field offsets for claims.
5. Separate confirmed facts, inferences, and uncertainties.
6. State confidence and what evidence would change the conclusion.
7. Recommend next steps, risks, and alternatives where relevant.
```

After showing it, say:

`Oracle briefing ready. Review it above; correct anything biased or incomplete before I send it. Reply 'send' to proceed, or tell me what to change.`

## Sending The Briefing

After user confirmation, launch a subagent with a prompt containing exactly the
briefing text. Prefer:

```text
subagent_type: oracle
description: Oracle: <one-line summary>
prompt: <briefing text verbatim>
```

If `oracle` is unavailable in the current session, use `general` and start the
prompt with:

```text
You are acting as an independent expert oracle. Do not assume any prior context beyond what is in this prompt.
```

Then include the briefing text verbatim.

## Handling The Verdict

Present the oracle's full response verbatim under:

```markdown
## Oracle Verdict
```

Then add a short synthesis only when useful:

- Immediate action the oracle flagged.
- Any constraints the oracle missed.
- Whether you accept or reject the recommendation, grounded in evidence.

Do not silently override the oracle. If you disagree, state the disagreement and
the concrete evidence that decides it.
