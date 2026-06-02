#!/usr/bin/env python3
"""PostToolUse hook: inject cached context-enrichment after Ghidra decompile.

Wired in .claude/settings.json on the same matcher as decompile_hook.py:
  mcp__ghidra__decompile_function|mcp__ghidra__batch_decompile|mcp__ghidra__force_decompile

When the agent decompiles a function, this hook checks whether cache-context
(llm_auto_lift.py cache-context subcommand) ran for that target beforehand.
If a cache file exists with enrichment data, it formats the trustworthy fields
into a compact markdown block and injects it as a [context-enrichment] system
message.  If no cache file exists, or the cache has no enrichment, it exits
silently (no systemMessage output).

Enrichment fields covered:
  - callee_details: callee signature table (name, decl, ported, @reg status)
  - call_site_audit: FPU/FSTP hazards ONLY (ARG_COUNT and REGISTER_ALIASING
    are filtered out as unreliable)
  - struct_offsets: verified field offsets for ESI/EDI/EBX only (other regs
    filtered out because non-pointer registers are unreliable evidence)
  - buffer_alias: HIGH-RISK buffer-alias reads only
  - buffer_warnings: undersized-buffer warnings

NOTE: The render_enrichment_markdown function below is intentionally inlined
rather than imported from llm_auto_lift.py, because that file is under active
concurrent development.  When both files stabilize, the maintainer should move
the canonical implementation into llm_auto_lift.py and import it here.

Exit codes: 0 always (errors are logged, not fatal).
Output: {"systemMessage": "..."} on stdout when enrichment is found,
        empty stdout when there is nothing to inject.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CONTEXT_CACHE = REPO_ROOT / "artifacts" / "auto_lift" / "context_cache"

# Max chars of enrichment markdown to inject into systemMessage
MAX_INJECT_CHARS = 4000


# ---------------------------------------------------------------------------
# Address → cache-file resolver
# ---------------------------------------------------------------------------

def _cache_file_for_address(address: str) -> Path | None:
    """Return the context-cache Path for a hex address, or None if absent.

    Cache files are named  FUN_<08x>.json  (zero-padded 8-digit hex).
    The address from the Ghidra tool input may carry a space prefix (e.g.
    "mem:0x1234ab") and/or a 0x prefix.  We strip both before formatting.

    If the padded FUN_ path doesn't exist we also try the function name
    by scanning for files whose JSON address field matches — but that scan
    is expensive, so we only do it as a last-resort and cap at 200 files.
    """
    if not address:
        return None

    # Strip optional "space:" prefix
    if ":" in address:
        address = address.split(":", 1)[1]

    # Normalise hex
    try:
        addr_int = int(address, 16)
    except ValueError:
        return None

    name = f"FUN_{addr_int:08x}"
    candidate = CONTEXT_CACHE / f"{name}.json"
    if candidate.exists():
        return candidate

    # Fallback: caller may have named the target (e.g. "object_update").
    # We cannot recover the name from address alone without kb.json, so stop.
    return None


def _cache_file_for_batch(functions_arg: str) -> list[Path]:
    """Return existing cache files for a comma-separated function-name list."""
    results: list[Path] = []
    for name in functions_arg.split(","):
        name = name.strip()
        if not name:
            continue
        candidate = CONTEXT_CACHE / f"{name}.json"
        if candidate.exists():
            results.append(candidate)
    return results


# ---------------------------------------------------------------------------
# Renderer (inline copy — see module docstring for deduplication note)
# ---------------------------------------------------------------------------

def render_enrichment_markdown(ghidra_ctx: dict) -> str:
    """Format trustworthy enrichment fields into compact markdown.

    Filters aggressively:
      - Only FPU_ARG / FSTP hazards from call_site_audit (no ARG_COUNT,
        no REGISTER_ALIASING).
      - Only ESI / EDI / EBX from struct_offsets (pointer-holding regs).
      - Only high_risk entries from buffer_alias.details.
      - Never emits annotated_decompile (can be 20 KB).
    """
    out: list[str] = []

    # --- Callee signature table ---
    cd: dict = ghidra_ctx.get("callee_details") or {}
    if cd:
        rows: list[str] = []
        for name, e in cd.items():
            if e.get("not_in_kb"):
                rows.append(f"| {name} | — | NOT IN KB | — |")
            else:
                reg_flag = "@reg" if e.get("has_reg_args") else ""
                decl = (e.get("decl") or "").replace("|", "\\|")
                ported = str(bool(e.get("ported")))
                rows.append(
                    f"| {e.get('name', name)} | `{decl}` | {ported} | {reg_flag} |"
                )
        if rows:
            header = "| name | decl | ported | reg-args |"
            sep = "|---|---|---|---|"
            out.append(
                "**Callees**\n\n" + header + "\n" + sep + "\n" + "\n".join(rows)
            )

    # --- FPU / FSTP call-site hazards only ---
    fpu_lines: list[str] = []
    seen: set[str] = set()
    for cs in ghidra_ctx.get("call_site_audit") or []:
        for h in cs.get("hazards") or []:
            if "FPU" not in h.upper() and "FSTP" not in h.upper():
                continue
            callee_label = cs.get("callee_kb") or cs.get("callee_addr") or "?"
            line = f"- {cs.get('call_addr', '?')} → {callee_label}: {h}"
            if line not in seen:
                seen.add(line)
                fpu_lines.append(line)
    if fpu_lines:
        out.append("**FPU/FSTP arg hazards**\n" + "\n".join(fpu_lines))

    # --- Struct offsets (pointer-holding registers only) ---
    POINTER_REGS = {"ESI", "EDI", "EBX"}
    so: dict = ghidra_ctx.get("struct_offsets") or {}
    filtered_so = {r: v for r, v in so.items() if r.upper() in POINTER_REGS}
    if filtered_so:
        lines = [
            f"- {r}: {', '.join(v)}" for r, v in sorted(filtered_so.items())
        ]
        out.append("**Verified struct offsets (ptr regs)**\n" + "\n".join(lines))

    # --- HIGH-RISK buffer-alias reads ---
    ba: dict = ghidra_ctx.get("buffer_alias") or {}
    hi_risk = [d for d in (ba.get("details") or []) if d.get("high_risk")]
    if hi_risk:
        lines = [
            f"- line {d['line']}: `{d['local']}` is `{d['buffer']}`+{d['offset_into_buffer']}"
            for d in hi_risk
        ]
        out.append("**HIGH-RISK buffer-alias reads**\n" + "\n".join(lines))

    # --- Undersized-buffer warnings ---
    bw: list = ghidra_ctx.get("buffer_warnings") or []
    if bw:
        out.append("**Undersized-buffer warnings**\n" + "\n".join(f"- {w}" for w in bw))

    if not out:
        return ""

    caveat = (
        "_Evidence-derived from cached Ghidra context; verify against disasm "
        "before relying._"
    )
    return caveat + "\n\n" + "\n\n".join(out)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def _load_and_render(cache_path: Path) -> str:
    """Load a cache JSON and render its enrichment.  Returns "" on any error."""
    try:
        ctx = json.loads(cache_path.read_text(encoding="utf-8"))
        return render_enrichment_markdown(ctx)
    except Exception:
        return ""


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read())
    except (json.JSONDecodeError, OSError):
        return 0

    tool_name: str = payload.get("tool_name", "")
    if tool_name not in (
        "mcp__ghidra__decompile_function",
        "mcp__ghidra__batch_decompile",
        "mcp__ghidra__force_decompile",
    ):
        return 0

    tool_input: dict = payload.get("tool_input") or {}
    cache_paths: list[Path] = []

    if tool_name == "mcp__ghidra__batch_decompile":
        # batch_decompile takes "functions" (comma-separated names)
        functions_arg = tool_input.get("functions", "")
        cache_paths = _cache_file_for_batch(functions_arg)
    else:
        # decompile_function / force_decompile take "address"
        address = tool_input.get("address", "")
        p = _cache_file_for_address(address)
        if p:
            cache_paths = [p]

    if not cache_paths:
        # No cache file for this target → silent no-op
        return 0

    sections: list[str] = []
    for cp in cache_paths:
        md = _load_and_render(cp)
        if md:
            sections.append(md)

    if not sections:
        # Cache exists but has no enrichment → silent no-op
        return 0

    combined = "\n\n---\n\n".join(sections)
    if len(combined) > MAX_INJECT_CHARS:
        combined = combined[:MAX_INJECT_CHARS] + "\n\n... (truncated; full enrichment in context cache)"

    msg = "[context-enrichment] Pre-computed Ghidra enrichment for this target:\n\n" + combined
    print(json.dumps({"systemMessage": msg}), flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
