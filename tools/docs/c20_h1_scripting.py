#!/usr/bin/env python3
"""Generate local QMD-friendly Halo 1 scripting notes from c20."""

from __future__ import annotations

import argparse
import datetime as _dt
import re
import sys
import urllib.request
from pathlib import Path


OUT_PATH = Path("docs/references/h1/scripting-reference.md")
JINA_URL = "https://r.jina.ai/http://c20.reclaimers.net/h1/scripting/"

SECTION_LEVELS = {
    "HSC reference": "##",
    "Declaring scripts": "##",
    "Declaring globals": "##",
    "Value types": "##",
    "When to use short vs long": "##",
    "Functions": "##",
    "Control": "###",
    "Math": "###",
    "Logic and comparison": "###",
    "Server functions": "###",
    "Other functions": "###",
    "External globals": "##",
    "Removed": "##",
}

DROP_LINES = {
    "Function",
    "Global",
    "Type",
    "Details",
    "Comments",
    "Example",
    "Page contents",
    "Acknowledgements",
    "Function/global",
    "A · B · C · D · E · F · G · H · I · L · M · N · O · P · Q · R · S · T · U · V (506 total)",
    "A · B · C · D · E · F · G · H · L · M · N · O · P · R · S · T · U · V · W (499 total)",
}


def fetch_page(timeout: int) -> str:
    request = urllib.request.Request(JINA_URL, headers={"User-Agent": "halo-docs-c20-import/1.0"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8", errors="replace")


def source_body(text: str) -> str:
    marker = "Markdown Content:"
    if marker in text:
        text = text.split(marker, 1)[1]
    return text.replace("http://c20.reclaimers.net", "https://c20.reclaimers.net")


def clean_link_text(line: str) -> str:
    line = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", line)
    line = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", line)
    line = re.sub(r"https?://\S+", "", line)
    return line.rstrip()


def normalize_source(text: str) -> list[str]:
    text = source_body(text)
    start = text.find("HSC reference")
    end = text.find("## [Acknowledgements")
    if end < 0:
        end = text.find("Acknowledgements")
    if start < 0:
        raise RuntimeError("could not find HSC reference section")
    if end < start:
        end = len(text)

    body = text[start:end]
    out: list[str] = []
    blank = False
    in_code = False
    for raw in body.splitlines():
        stripped = clean_link_text(raw).strip()
        if not stripped:
            if not blank:
                out.append("")
            blank = True
            continue
        blank = False
        if stripped in DROP_LINES:
            continue
        if stripped.startswith("[](https://"):
            continue
        if stripped.startswith("Thanks to the following"):
            break
        if stripped in SECTION_LEVELS:
            if in_code:
                out.append("```")
                in_code = False
            if out and out[-1] != "":
                out.append("")
            out.append(f"{SECTION_LEVELS[stripped]} {stripped}")
            out.append("")
            continue
        if re.match(r"^\([^)]*\)$", stripped) or stripped.startswith("(script ") or stripped.startswith("(global "):
            if not in_code:
                out.append("```lisp")
                in_code = True
            out.append(stripped)
            continue
        if in_code and (stripped.startswith(";") or stripped.startswith("(") or stripped == ")" or raw.startswith("  ")):
            out.append(stripped)
            continue
        if in_code:
            out.append("```")
            in_code = False
        out.append(stripped)
    if in_code:
        out.append("```")
    while out and out[-1] == "":
        out.pop()
    return out


def render(text: str, retrieved: str) -> str:
    body = normalize_source(text)
    lines = [
        "# Halo 1 Scripting Reference",
        "",
        "## Summary",
        "",
        "This page is a local, cleaned reference extract of the Reclaimers Library Halo 1 scripting page. It preserves the HSC value types, script declaration notes, function listings, external globals, and removed entries for local search. Use `scripting.md` for the concise Xbox-focused overview.",
        "",
        "## Scope Notes",
        "",
        "- This is Reclaimers-derived reference material, not independently verified Xbox binary evidence.",
        "- Entries marked server-only, H1A-only, OpenSauce, Custom Edition, or PC-specific should not be treated as original Xbox behavior without separate binary verification.",
        "- Some examples and descriptions are retained from the source to preserve searchability of function names and usage patterns.",
        "",
    ]
    lines.extend(body)
    lines.extend(
        [
            "",
            "## Provenance",
            "",
            f"Adapted from the Reclaimers Library Halo 1 scripting page. Retrieved {retrieved}. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.",
            "",
        ]
    )
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=OUT_PATH)
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args(argv)

    retrieved = _dt.date.today().isoformat()
    text = fetch_page(args.timeout)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(render(text, retrieved), encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
