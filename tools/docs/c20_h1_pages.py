#!/usr/bin/env python3
"""Generate local QMD-friendly Halo 1 reference pages from c20."""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


TREE_URL = "https://api.github.com/repos/Sigmmma/c20/git/trees/master?recursive=1"
RAW_PREFIX = "src/content/h1/"
JINA_PREFIX = "https://r.jina.ai/http://c20.reclaimers.net/h1"
OUT_ROOT = Path("docs/references/h1")

INCLUDE_PREFIXES = (
    "engine/",
    "h1-ek/",
    "maps/",
    "scripting/",
    "source-data/",
    "tags/",
)

INCLUDE_EXACT = {
    "readme.md",
    "custom-edition/readme.md",
}

EXCLUDE_PREFIXES = (
    "community-tools/",
    "guides/",
)

EXCLUDE_TAG_TOP_LEVEL = {
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
}


def load_tree(timeout: int) -> list[str]:
    request = urllib.request.Request(
        TREE_URL,
        headers={"User-Agent": "halo-docs-c20-import/1.0", "Accept": "application/vnd.github+json"},
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        data = json.load(response)
    return sorted(
        item["path"]
        for item in data.get("tree", [])
        if item.get("path", "").startswith(RAW_PREFIX) and item.get("path", "").endswith("/readme.md")
    )


def rel_page(path: str) -> str:
    return path[len(RAW_PREFIX) :]


def page_url(rel: str) -> str:
    rel = rel.removesuffix("/readme.md")
    if rel == "readme.md":
        return ""
    return rel.strip("/")


def include_page(rel: str) -> bool:
    if rel in INCLUDE_EXACT:
        return True
    if rel.startswith(EXCLUDE_PREFIXES):
        return False
    if not rel.startswith(INCLUDE_PREFIXES):
        return False
    if rel.startswith("tags/"):
        if rel == "tags/readme.md":
            return False
        parts = rel.split("/")
        if len(parts) == 3 and parts[2] == "readme.md" and parts[1] in EXCLUDE_TAG_TOP_LEVEL:
            return False
    return True


def out_path_for(rel: str) -> Path:
    url = page_url(rel)
    if rel.startswith("tags/"):
        tag_path = rel.removesuffix("/readme.md")
        parts = tag_path.split("/")[1:]
        if len(parts) == 1:
            return OUT_ROOT / "tags" / f"{parts[0]}.md"
        return OUT_ROOT / "tags" / Path(*parts).with_suffix(".md")
    if url == "":
        return OUT_ROOT / "pages" / "h1.md"
    return OUT_ROOT / "pages" / Path(url).with_suffix(".md")


def fetch_reader(url_path: str, timeout: int, retries: int) -> str:
    url = JINA_PREFIX + ("/" + url_path.strip("/") if url_path else "") + "/"
    request = urllib.request.Request(url, headers={"User-Agent": "halo-docs-c20-import/1.0"})
    last_error: Exception | None = None
    for attempt in range(retries + 1):
        try:
            with urllib.request.urlopen(request, timeout=timeout) as response:
                return response.read().decode("utf-8", errors="replace")
        except (urllib.error.URLError, TimeoutError) as exc:
            last_error = exc
            if attempt < retries:
                time.sleep(1.0 + attempt)
    raise RuntimeError(f"failed to fetch {url}: {last_error}")


def strip_reader_prefix(text: str) -> str:
    marker = "Markdown Content:"
    if marker in text:
        text = text.split(marker, 1)[1]
    return text.replace("http://c20.reclaimers.net", "https://c20.reclaimers.net")


def plain_link(match: re.Match[str]) -> str:
    label = match.group(1).strip()
    if label.startswith("!"):
        return ""
    return label


def clean_markdown(markdown: str) -> str:
    text = strip_reader_prefix(markdown)
    text = re.sub(r"!\[[^\]]*\]\([^)]*\)", "", text)
    text = re.sub(r"\[([^\]]*)\]\(([^)]*)\)", plain_link, text)
    text = re.sub(r"https?://\S+", "", text)
    lines: list[str] = []
    for raw in text.splitlines():
        line = raw.rstrip()
        stripped = line.strip()
        if not stripped:
            lines.append("")
            continue
        if stripped in {"c20", "The Reclaimers Library", "Page contents", "Workflows", "Workflows:"}:
            continue
        if stripped.startswith("Chat on Discord") or stripped.startswith("Edit with"):
            continue
        if re.match(r"^\d+\.\s+(General information|Halo 1|Halo 2|Halo 3|Halo 3 ODST|Halo Reach|Halo 4|Halo 2 Anniversary MP|Contributing to c20)\b", stripped):
            continue
        if stripped.startswith("## [Acknowledgements") or stripped == "Acknowledgements":
            break
        line = re.sub(r"^#\s+", "## ", line)
        line = re.sub(r"^##\s+", "### ", line)
        lines.append(line)
    text = "\n".join(lines).strip()
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text


def title_from_body(body: str, url_path: str) -> str:
    for line in body.splitlines():
        if line.startswith("## ") or line.startswith("### "):
            return line.lstrip("# ").strip()
    return url_path.strip("/") or "h1"


def render(rel: str, markdown: str, retrieved: str) -> str:
    url = page_url(rel)
    body = clean_markdown(markdown)
    title = title_from_body(body, url)
    source = "https://c20.reclaimers.net/h1/" + (url.strip("/") + "/" if url else "")
    lines = [
        f"# Halo 1 Reference: {title}",
        "",
        "## Source Page",
        "",
        f"- Source: `{source}`",
        f"- Local path: `{rel}`",
        "- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.",
        "",
        "## Content",
        "",
        body,
        "",
        "## Provenance",
        "",
        f"Adapted from the Reclaimers Library Halo 1 page `{source}`. Retrieved {retrieved}. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.",
        "",
    ]
    return "\n".join(lines)


def render_index(rows: list[tuple[str, Path]], skipped: list[str], retrieved: str) -> str:
    lines = [
        "# Halo 1 Imported Page Index",
        "",
        "Generated index for C20 Halo 1 pages imported outside the curated overview pages and top-level tag import.",
        "",
        f"Last generation attempt: {retrieved}",
        "",
        "## Imported Pages",
        "",
        "| Source path | Local doc |",
        "| --- | --- |",
    ]
    for rel, out_path in rows:
        display = out_path.relative_to(OUT_ROOT / "pages") if str(out_path).startswith(str(OUT_ROOT / "pages")) else out_path.relative_to(OUT_ROOT)
        lines.append(f"| `{rel}` | [{display}]({display.as_posix()}) |")
    lines.extend(["", "## Excluded Page Classes", ""])
    lines.append("- `community-tools/`: tool manuals rather than runtime/reference behavior, except information already captured where relevant in runtime pages.")
    lines.append("- `guides/`: authoring tutorials; include specific facts later only when they document engine/runtime behavior not present elsewhere.")
    lines.append("- Top-level tag pages already generated under `tags/` by `tools/docs/c20_h1_tags.py`; nested tag pages are imported here.")
    lines.extend(["", "## Skipped Source Paths", ""])
    lines.extend(f"- `{rel}`" for rel in skipped[:200])
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--retries", type=int, default=2)
    parser.add_argument("--delay", type=float, default=0.1)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args(argv)

    retrieved = _dt.date.today().isoformat()
    paths = [rel_page(path) for path in load_tree(args.timeout)]
    selected = [rel for rel in paths if include_page(rel)]
    skipped = [rel for rel in paths if not include_page(rel)]
    generated: list[tuple[str, Path]] = []
    failed: dict[str, str] = {}
    for rel in selected:
        out_path = out_path_for(rel)
        generated.append((rel, out_path))
        if args.dry_run:
            continue
        try:
            markdown = fetch_reader(page_url(rel), args.timeout, args.retries)
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_text(render(rel, markdown, retrieved), encoding="utf-8")
        except Exception as exc:  # noqa: BLE001 - report per-page failures and continue.
            failed[rel] = str(exc)
        time.sleep(args.delay)
    if not args.dry_run:
        index_path = OUT_ROOT / "pages" / "index.md"
        index_path.parent.mkdir(parents=True, exist_ok=True)
        index_path.write_text(render_index(generated, skipped, retrieved), encoding="utf-8")
    print(f"selected={len(selected)} skipped={len(skipped)} failed={len(failed)}")
    for rel, message in failed.items():
        print(f"failed {rel}: {message}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
