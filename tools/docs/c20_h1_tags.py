#!/usr/bin/env python3
"""Generate local QMD-friendly Halo 1 tag notes from c20 pages."""

from __future__ import annotations

import argparse
import datetime as _dt
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


TAGS = [
    "actor",
    "actor_variant",
    "antenna",
    "bitmap",
    "camera_track",
    "color_table",
    "continuous_damage_effect",
    "contrail",
    "damage_effect",
    "decal",
    "detail_object_collection",
    "dialogue",
    "effect",
    "flag",
    "fog",
    "font",
    "gbxmodel",
    "globals",
    "glow",
    "grenade_hud_interface",
    "hud_globals",
    "hud_message_text",
    "hud_number",
    "input_device_defaults",
    "item_collection",
    "lens_flare",
    "light",
    "light_volume",
    "lightning",
    "material_effects",
    "meter",
    "model",
    "model_animations",
    "model_collision_geometry",
    "multiplayer_scenario_description",
    "object",
    "particle",
    "particle_system",
    "physics",
    "placeholder",
    "point_physics",
    "preferences_network_game",
    "scenario",
    "scenario_structure_bsp",
    "shader",
    "sky",
    "sound",
    "sound_environment",
    "sound_looping",
    "sound_scenery",
    "string_list",
    "tag_collection",
    "ui_widget_collection",
    "ui_widget_definition",
    "unicode_string_list",
    "unit_hud_interface",
    "virtual_keyboard",
    "weapon_hud_interface",
    "weather_particle_system",
    "wind",
]

OUT_DIR = Path("docs/references/h1/tags")
JINA_PREFIX = "https://r.jina.ai/http://c20.reclaimers.net/h1/tags"
C20_PREFIX = "https://c20.reclaimers.net/h1/tags"


def plain_link(match: re.Match[str]) -> str:
    label = match.group(1).strip()
    if label.startswith("!"):
        return ""
    if label == "" or label == "[]":
        return ""
    return label


def simplify_links(markdown: str) -> str:
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", markdown)
    text = re.sub(r"\[([^\]]*)\]\(([^)]*)\)", plain_link, text)
    text = re.sub(r"https?://\S+", "", text)
    text = re.sub(r"\[\?\]", "", text)
    text = text.replace("http://c20.reclaimers.net", "")
    text = text.replace("https://c20.reclaimers.net", "")
    return text


def is_navigation_line(line: str) -> bool:
    stripped = line.strip()
    if not stripped:
        return False
    if stripped in {"c20", "The Reclaimers Library"}:
        return True
    if stripped in {"Page contents", "Workflows:", "Edit with", "* * *"}:
        return True
    if stripped in {"H1-Guerilla", "invader-edit", "invader-edit-qt", "OS_ Guerilla", "Mozzarilla", "reclaimer-python"}:
        return True
    if stripped.startswith("Chat on Discord") or stripped.startswith("This page is incomplete"):
        return True
    if re.match(r"^\d+\.\s+(General information|Halo 1|H1 Editing Kit|Community tools|Halo Custom Edition|Guides|Source data|Tags)\b", stripped):
        return True
    if re.match(r"^\d+\.\s+[a-z0-9_]+\s*$", stripped):
        return True
    if re.match(r"^\d+\.\s+(object|shader) \(abstract tag\)$", stripped):
        return True
    if re.match(r"^\d+\.\s+(Scripting|Maps|Engine systems|Halo [0-9A-Za-z ]+|Contributing to c20|Structure and fields)$", stripped):
        return True
    if stripped.startswith("Navigation") or stripped.startswith("Table of contents"):
        return True
    return False


def flatten_nested_table_line(line: str) -> str:
    if "| | Field |" not in line and "| | Flag |" not in line and "| | Option |" not in line and "| * " not in line:
        return line
    cells = [cell.strip() for cell in line.split("|")]
    rows = []
    headers = []
    header_size = 0
    i = 0
    while i < len(cells):
        cell = cells[i]
        if not cell or cell == "---":
            i += 1
            continue
        if cell in {"Field", "Flag", "Option"} and i + 2 < len(cells):
            headers = cells[i : i + 3]
            header_size = 3
            i += 3
            continue
        if header_size:
            row = cells[i : i + header_size]
            if len(row) == header_size and row[0] not in {"Field", "Flag", "Option", "---", ""}:
                rows.append(row)
                i += header_size
                continue
        i += 1
    if not rows:
        return line

    title = "Nested values"
    if headers:
        title = f"{headers[0]} values"
    output = [f"{title}:"]
    for row in rows:
        name = row[0]
        value = row[1]
        comment = row[2]
        if comment:
            output.append(f"- {name} ({value}): {comment}")
        elif value:
            output.append(f"- {name} ({value})")
        else:
            output.append(f"- {name}")
    return "\n".join(output)


def clean_markdown(markdown: str) -> str:
    text = simplify_links(markdown)
    cleaned_lines = []
    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()
        if is_navigation_line(stripped):
            continue
        if stripped.startswith("Source:") or stripped.startswith("Retrieved:"):
            continue
        if stripped.startswith("Thanks to the following individuals"):
            break
        if stripped.startswith("## Acknowledgements"):
            break
        if stripped.startswith("## [Acknowledgements"):
            break
        line = re.sub(r"^##\s+", "### ", line)
        line = re.sub(r"^#\s+", "## ", line)
        line = re.sub(r"\s+", " ", line) if "|" not in line else line
        line = flatten_nested_table_line(line)
        cleaned_lines.append(line)
    text = "\n".join(cleaned_lines)
    text = re.sub(r"\n{3,}", "\n\n", text.strip())
    return text


def fetch_reader_markdown(tag: str, timeout: int, retries: int) -> str:
    url = f"{JINA_PREFIX}/{tag}/"
    request = urllib.request.Request(url, headers={"User-Agent": "halo-docs-c20-import/1.0"})
    last_error = None
    for attempt in range(retries + 1):
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                data = response.read().decode("utf-8", errors="replace")
            return extract_markdown_content(data)
        except (urllib.error.URLError, TimeoutError) as exc:
            last_error = exc
            if attempt < retries:
                time.sleep(1.0 + attempt)
    raise RuntimeError(f"failed to fetch {tag}: {last_error}")


def extract_markdown_content(reader_text: str) -> str:
    marker = "Markdown Content:"
    if marker in reader_text:
        reader_text = reader_text.split(marker, 1)[1]
    reader_text = reader_text.replace("http://c20.reclaimers.net", "https://c20.reclaimers.net")
    reader_text = re.sub(r"\n{3,}", "\n\n", reader_text.strip())
    return reader_text


def first_paragraph(markdown: str) -> str:
    markdown = simplify_links(markdown)
    for block in markdown.split("\n\n"):
        text = block.strip()
        if not text:
            continue
        if is_navigation_line(text):
            continue
        if text.startswith("|") or text.startswith("#") or text.startswith("*"):
            continue
        if "#tag-field-" in text or "| | ---" in text or len(text) > 800:
            continue
        return simplify_links(text).replace("\n", " ")
    return "This tag page is primarily field documentation. See the tag information below for defined fields, enum values, units, and runtime notes."


def hsc_terms(markdown: str) -> list[str]:
    terms = sorted(set(re.findall(r"`\(([^`)]+)\)`", markdown)))
    filtered = []
    for term in terms:
        term = term.strip()
        if not term or "," in term:
            continue
        if re.match(r"^(?:<[^>]+>\s+)?[A-Za-z_][A-Za-z0-9_]*(?:\s|$)", term):
            filtered.append(term)
    return filtered


def field_terms(markdown: str) -> list[str]:
    terms = sorted(set(re.findall(r"#tag-field-([a-z0-9-]+)", markdown)))
    return terms[:80]


def field_names(markdown: str) -> list[str]:
    names = []
    seen = set()
    for term in field_terms(markdown):
        name = term.replace("-", " ")
        if name not in seen:
            names.append(name)
            seen.add(name)
    return names


def render_doc(tag: str, markdown: str, retrieved: str) -> str:
    summary = first_paragraph(markdown)
    hsc = hsc_terms(markdown)
    fields = field_names(markdown)
    body = clean_markdown(markdown)

    lines = [
        f"# Halo 1 Tag: {tag}",
        "",
        "## Summary",
        "",
        summary,
        "",
        "## Tag Information",
        "",
        f"Tag group: `{tag}`.",
    ]
    if hsc:
        lines.extend(["", "HaloScript entries:", ""])
        lines.extend(f"- `{term}`" for term in hsc)
    if fields:
        lines.extend(["", "Defined fields and enum values mentioned:", ""])
        lines.extend(f"- {field}" for field in fields)
    lines.extend(
        [
            "",
            "## Details",
            "",
            body,
            "",
            "## Provenance",
            "",
            f"Adapted from the Reclaimers Library Halo 1 tag page for `{tag}`. Retrieved {retrieved}. Text is retained locally so this page is readable without following external references. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.",
            "",
        ]
    )
    return "\n".join(lines)


def render_index(generated: set[str], preserved: set[str], failed: dict[str, str], retrieved: str) -> str:
    lines = [
        "# Halo 1 Tag Documentation Index",
        "",
        "This is a local QMD index for readable Halo 1 tag notes. The tag pages keep the useful field, runtime, and scripting text locally instead of requiring agents to follow external links.",
        "",
        f"Last generation attempt: {retrieved}",
        "",
        "The local goal is compact, search-friendly documentation for tag behavior, fields, runtime limits, and HaloScript controls.",
        "",
        "## Coverage",
        "",
        f"- Expected H1 tag pages: {len(TAGS)}",
        f"- Generated this run: {len(generated)}",
        f"- Preserved existing local docs: {len(preserved)}",
        f"- Failed this run: {len(failed)}",
        "",
        "## Tag Pages",
        "",
        "| Tag | Local doc | Status |",
        "| --- | --- | --- |",
    ]
    for tag in TAGS:
        if tag in failed:
            status = f"failed: {failed[tag]}"
        elif tag in generated:
            status = "generated"
        elif tag in preserved:
            status = "preserved"
        else:
            status = "unknown"
        lines.append(f"| `{tag}` | [{tag}.md]({tag}.md) | {status} |")
    lines.extend(
        [
            "",
            "## Suggested Local Page Shape",
            "",
            "Use this shape when hand-refining generated pages:",
            "",
            "| Section | Purpose |",
            "| --- | --- |",
            "| Summary | One-paragraph purpose of the tag and tag group code. |",
            "| Runtime behavior | Engine behavior, limits, debug commands, and special cases. |",
            "| Reverse-engineering relevance | Useful globals, systems, source files, assertions, or likely function areas. |",
            "| Fields | Compact field table with values, units, defaults, and side effects. |",
            "| Script controls | Related HaloScript functions/globals and how they affect runtime state. |",
            "| Open questions | Unknowns from the source page or things worth validating in the Xbox binary. |",
            "| Provenance | Retrieval date and source attribution without link-heavy reference clutter. |",
            "",
        ]
    )
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=OUT_DIR)
    parser.add_argument("--force", action="store_true", help="overwrite existing tag docs")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--delay", type=float, default=0.2)
    parser.add_argument(
        "--preserve",
        action="append",
        default=["decal"],
        help="existing tag doc to preserve even with --force; may be repeated",
    )
    args = parser.parse_args(argv)

    retrieved = _dt.date.today().isoformat()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    generated: set[str] = set()
    preserved: set[str] = set()
    failed: dict[str, str] = {}

    for tag in TAGS:
        out_path = args.out_dir / f"{tag}.md"
        if out_path.exists() and (not args.force or tag in set(args.preserve)):
            preserved.add(tag)
            continue
        try:
            markdown = fetch_reader_markdown(tag, args.timeout, args.retries)
            out_path.write_text(render_doc(tag, markdown, retrieved), encoding="utf-8")
            generated.add(tag)
        except Exception as exc:  # noqa: BLE001 - report per-tag failures and continue.
            failed[tag] = str(exc)
        time.sleep(args.delay)

    index_path = args.out_dir / "index.md"
    index_path.write_text(render_index(generated, preserved, failed, retrieved), encoding="utf-8")

    print(f"expected={len(TAGS)} generated={len(generated)} preserved={len(preserved)} failed={len(failed)}")
    for tag, message in failed.items():
        print(f"failed {tag}: {message}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
