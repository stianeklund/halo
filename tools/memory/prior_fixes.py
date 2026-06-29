#!/usr/bin/env python3
"""Lookup prior fixes, regressions, and relevant skills for a symptom.

This is a cheap preflight for agents before debugging a regression.  It searches
durable local evidence sources that are easy to forget manually:

- source-controlled docs, skills, and commands
- repo-local agent memory under .claude/agent-memory (excluding token logs)
- recent git commit subjects/bodies/files
- retrieval outcome metadata in tools/retrieval/index.duckdb when available

The output is a set of leads, not proof.  Confirm any hypothesis against the
binary, disassembly, or runtime oracle before fixing code.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parent.parent.parent

STOPWORDS = {
    "about", "after", "again", "against", "all", "and", "are", "because",
    "been", "before", "being", "bug", "but", "can", "case", "does", "fix",
    "fixed", "for", "from", "have", "how", "into", "issue", "like", "not",
    "our", "problem", "regression", "same", "that", "the", "this", "was",
    "when", "with", "wrong", "xbox", "halo",
}

SOURCE_WEIGHTS = {
    "agent-memory": 2.2,
    "git": 1.9,
    "retrieval": 1.7,
    "lift-learnings": 1.6,
    "qmd": 1.5,
    "docs": 1.3,
    "skills": 1.1,
    "commands": 1.0,
}


@dataclass
class Match:
    source: str
    title: str
    location: str
    score: float
    snippet: str


def tokenize(text: str) -> list[str]:
    tokens = []
    for raw in re.findall(r"[a-zA-Z0-9_]{3,}", text.lower()):
        if raw not in STOPWORDS:
            tokens.append(raw)
    return tokens


def score_text(text: str, terms: list[str], phrases: list[str], target: str) -> float:
    if not text or not terms:
        return 0.0
    lower = text.lower()
    score = 0.0
    for term in terms:
        count = lower.count(term)
        if count:
            score += min(count, 8) * (2.0 if len(term) >= 8 else 1.0)
    for phrase in phrases:
        if phrase and phrase in lower:
            score += 8.0
    if target and target.lower() in lower:
        score += 14.0
    return score


def best_snippet(text: str, terms: list[str], max_chars: int = 360) -> str:
    lines = text.splitlines()
    if not lines:
        return ""
    best_idx = 0
    best_score = -1
    for idx, line in enumerate(lines):
        lower = line.lower()
        score = sum(lower.count(t) for t in terms)
        if score > best_score:
            best_idx = idx
            best_score = score
    start = max(0, best_idx - 1)
    end = min(len(lines), best_idx + 3)
    snippet = " ".join(l.strip() for l in lines[start:end] if l.strip())
    snippet = re.sub(r"\s+", " ", snippet).strip()
    if len(snippet) > max_chars:
        snippet = snippet[: max_chars - 3].rstrip() + "..."
    return snippet


def source_kind(path: Path) -> str:
    rel = path.relative_to(REPO_ROOT).as_posix()
    if rel == "docs/lift-learnings.md":
        return "lift-learnings"
    if rel.startswith(".claude/agent-memory/"):
        return "agent-memory"
    if "/skills/" in rel or rel.endswith("/SKILL.md"):
        return "skills"
    if "/commands/" in rel:
        return "commands"
    return "docs"


def iter_markdown_files() -> Iterable[Path]:
    roots = [
        REPO_ROOT / "docs",
        REPO_ROOT / ".claude" / "agent-memory",
        REPO_ROOT / ".claude" / "skills",
        REPO_ROOT / ".claude" / "commands",
        REPO_ROOT / ".opencode" / "skills",
        REPO_ROOT / ".opencode" / "commands",
    ]
    for root in roots:
        if not root.exists():
            continue
        for path in root.rglob("*.md"):
            rel = path.relative_to(REPO_ROOT).as_posix()
            if "/token_discipline/" in rel:
                continue
            yield path


def markdown_matches(terms: list[str], phrases: list[str], target: str) -> list[Match]:
    matches: list[Match] = []
    for path in iter_markdown_files():
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        kind = source_kind(path)
        score = score_text(text, terms, phrases, target) * SOURCE_WEIGHTS[kind]
        if score <= 0.0:
            continue
        rel = path.relative_to(REPO_ROOT).as_posix()
        title = rel
        first_heading = re.search(r"^#\s+(.+)$", text, re.MULTILINE)
        if first_heading:
            title = first_heading.group(1).strip()
        matches.append(Match(kind, title, rel, round(score, 2), best_snippet(text, terms)))
    return matches


def run_git(args: list[str]) -> str:
    try:
        proc = subprocess.run(
            ["git", *args],
            cwd=REPO_ROOT,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
    except OSError:
        return ""
    return proc.stdout


def git_matches(terms: list[str], phrases: list[str], target: str, max_commits: int) -> list[Match]:
    raw = run_git([
        "log", "--all", f"-n{max_commits}", "--date=short",
        "--format=%H%x1f%ad%x1f%s", "--",
        "src", "kb.json", "docs", "tools", ".claude/agent-memory",
    ])
    candidates: list[tuple[float, str, str, str]] = []
    for line in raw.splitlines():
        parts = line.split("\x1f")
        if len(parts) != 3:
            continue
        sha, date, subject = parts
        score = score_text(subject, terms, phrases, target) * SOURCE_WEIGHTS["git"]
        if score > 0.0:
            candidates.append((score, sha, date, subject))

    candidates.sort(reverse=True, key=lambda item: item[0])
    matches: list[Match] = []
    for score, sha, date, subject in candidates[:12]:
        detail = run_git([
            "show", "--name-only", "--format=%B", "--no-renames", "-n", "1", sha,
            "--", "src", "kb.json", "docs", "tools", ".claude/agent-memory",
        ])
        detail_score = score + score_text(detail, terms, phrases, target) * 0.3
        snippet = best_snippet(detail or subject, terms)
        matches.append(Match(
            "git",
            subject,
            f"{sha[:12]} {date}",
            round(detail_score, 2),
            snippet or subject,
        ))
    return matches


def _extract_json_array(text: str) -> list[dict]:
    start = text.find("[")
    end = text.rfind("]")
    if start < 0 or end < start:
        return []
    try:
        data = json.loads(text[start:end + 1])
    except json.JSONDecodeError:
        return []
    return data if isinstance(data, list) else []


def qmd_matches(query: str, target: str, limit: int) -> list[Match]:
    """Query QMD's indexed markdown with a cheap explicit lex query.

    Use a typed `lex:` query to avoid query-expansion/rerank model startup.  The
    direct markdown scan below remains the fallback and covers agent-memory,
    which QMD does not currently index.
    """
    q = " ".join(part for part in (query, target) if part).strip()
    if not q:
        return []
    try:
        proc = subprocess.run(
            [
                "qmd", "query", f"lex: {q}", "--format", "json",
                "--no-rerank", "-n", str(limit),
            ],
            cwd=REPO_ROOT,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=12,
        )
    except (OSError, subprocess.TimeoutExpired):
        return []
    if proc.returncode != 0:
        return []

    matches: list[Match] = []
    for item in _extract_json_array(proc.stdout):
        file_name = str(item.get("file") or "")
        line = item.get("line")
        location = file_name
        if line:
            location += f":{line}"
        score = float(item.get("score") or 0.0) * 20.0 * SOURCE_WEIGHTS["qmd"]
        matches.append(Match(
            "qmd",
            str(item.get("title") or file_name),
            location,
            round(score, 2),
            re.sub(r"\s+", " ", str(item.get("snippet") or "")).strip()[:360],
        ))
    return matches


def retrieval_matches(terms: list[str], phrases: list[str], target: str) -> list[Match]:
    try:
        sys.path.insert(0, str(REPO_ROOT))
        from tools.retrieval import db as retrieval_db  # pylint: disable=import-outside-toplevel
    except Exception:
        return []

    try:
        con = retrieval_db.connect(read_only=True)
        rows = con.execute(
            """
            SELECT addr, name, obj_name, source_path, decl, vc71_score, verdict,
                   failure_reason, hazard_flags
            FROM functions
            """
        ).fetchall()
        cols = [d[0] for d in con.description]
        con.close()
    except Exception:
        return []

    matches: list[Match] = []
    for row in rows:
        rec = dict(zip(cols, row))
        text = " ".join(str(rec.get(k) or "") for k in cols)
        score = score_text(text, terms, phrases, target) * SOURCE_WEIGHTS["retrieval"]
        if score <= 0.0:
            continue
        quality = []
        if rec.get("vc71_score") is not None:
            quality.append(f"VC71 {rec['vc71_score']:.1f}%")
        if rec.get("verdict"):
            quality.append(str(rec["verdict"]))
        title = f"{rec.get('name')} @ {rec.get('addr')}"
        if quality:
            title += f" ({', '.join(quality)})"
        matches.append(Match(
            "retrieval",
            title,
            str(rec.get("source_path") or rec.get("obj_name") or "tools/retrieval/index.duckdb"),
            round(score, 2),
            best_snippet(text, terms),
        ))
    return matches


def skill_recommendations(query: str, target: str) -> list[str]:
    text = f"{query} {target}".lower()
    recs: list[str] = []

    def add(name: str, why: str) -> None:
        item = f"{name}: {why}"
        if item not in recs:
            recs.append(item)

    if re.search(r"regression|wrong|broken|crash|assert|hang|freeze|fault|access_violation|eip|cr2|visual|tint|invisible|missing", text):
        add("debug", "single entry point for runtime/debug symptoms")
        add("bug-hunt", "run automated hazard and silent-bug scans after edits")
    if re.search(r"crash|fault|access_violation|cr2|eip|assert|trap", text):
        add("crash-triage", "match registers/asserts against known crash signals")
    if re.search(r"page fault|access_violation|cr2|eip", text):
        add("halo-page-fault", "deep ABI/signature investigation")
    if re.search(r"visual|color|tint|invisible|geometry|lighting|raster|cull|draw|spawn|position", text):
        add("lift-silent-bugs", "non-crashing correctness and visual regressions")
        add("lift-crash-signals", "toggle-bisect and runtime localization")
    if re.search(r"buffer|stack|frame|chkstk|local_|alias|overflow", text):
        add("lift-frame-hazards", "buffer sizing and stack alias checks")
        add("lift-decompiler-traps", "buffer-alias and call-site decompiler traps")
    if re.search(r"arg|cdecl|stdcall|register|@<|eax|ebx|ecx|edx|esi|edi|prototype|abi", text):
        add("lift-arg-hazards", "argument count/order and register-arg checks")
        add("check-callee-regs", "unported callees with implicit register args")
    if re.search(r"lift|decomp|ghidra|vc71|objdiff|fpu|x87", text):
        add("halo-re-lift", "binary-backed lift workflow")
        add("halo-verify-debug", "verification ladder and delink/equivalence routing")
    if re.search(r"xemu|qmp|screenshot|gdb", text):
        add("debug-xemu", "xemu probing, QMP, screenshots, GDB")
    if re.search(r"xbdm|xbox|rdcp|devkit", text):
        add("halo-xbdm", "real Xbox probing and RDCP commands")
    return recs[:8]


def print_text_report(query: str, target: str, matches: list[Match], skills: list[str], limit: int) -> None:
    print("Prior-fix lookup")
    print(f"query : {query or '-'}")
    if target:
        print(f"target: {target}")
    print("note  : leads only; confirm against binary/disassembly/runtime before fixing")
    print()

    if skills:
        print("Recommended skills")
        for item in skills:
            print(f"- {item}")
        print()

    if not matches:
        print("No local prior-fix matches found.")
        return

    print("Top local matches")
    for idx, match in enumerate(matches[:limit], 1):
        print(f"{idx}. [{match.source}] {match.title}")
        print(f"   location: {match.location}")
        print(f"   score   : {match.score}")
        if match.snippet:
            print(f"   snippet : {match.snippet}")


def normalized_location(match: Match) -> str:
    loc = re.sub(r":\d+$", "", match.location)
    if loc.startswith("qmd://halo-docs/"):
        return "docs/" + loc[len("qmd://halo-docs/"):]
    if loc.startswith("qmd://halo-commands/"):
        return ".claude/commands/" + loc[len("qmd://halo-commands/"):]
    if loc.startswith("qmd://halo-skills/"):
        return ".claude/skills/" + loc[len("qmd://halo-skills/"):]
    if loc.startswith("qmd://halo-agent-skills/"):
        return ".agents/skills/" + loc[len("qmd://halo-agent-skills/"):]
    return loc


def dedupe_matches(matches: list[Match]) -> list[Match]:
    best_by_key: dict[str, Match] = {}
    for match in matches:
        key = normalized_location(match)
        prev = best_by_key.get(key)
        if prev is None or match.score > prev.score:
            best_by_key[key] = match
    return sorted(best_by_key.values(), key=lambda match: match.score, reverse=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Search prior fixes, agent memory, commits, and retrieval outcomes."
    )
    parser.add_argument("query", nargs="*", help="symptom, bug, target, or subsystem text")
    parser.add_argument("--target", help="optional function/address/subsystem to boost")
    parser.add_argument("--limit", type=int, default=8, help="number of matches to print")
    parser.add_argument("--max-commits", type=int, default=500, help="recent commits to scan")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()

    query = " ".join(args.query).strip()
    target = (args.target or "").strip()
    combined = " ".join(part for part in (query, target) if part)
    if not combined:
        parser.error("provide a query or --target")

    phrases = [p.lower() for p in re.findall(r'"([^"]+)"', combined)]
    terms = tokenize(combined)
    if not terms:
        terms = tokenize(re.sub(r'"[^"]+"', " ", combined))

    matches: list[Match] = []
    matches.extend(qmd_matches(query, target, max(args.limit, 8)))
    matches.extend(markdown_matches(terms, phrases, target))
    matches.extend(git_matches(terms, phrases, target, args.max_commits))
    matches.extend(retrieval_matches(terms, phrases, target))
    matches = dedupe_matches(matches)

    skills = skill_recommendations(query, target)
    if args.json:
        print(json.dumps({
            "query": query,
            "target": target,
            "recommended_skills": skills,
            "matches": [asdict(m) for m in matches[: args.limit]],
        }, indent=2))
    else:
        print_text_report(query, target, matches, skills, args.limit)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
