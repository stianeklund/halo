# Halo 1 Reference Notes

This directory contains local, QMD-searchable reference notes for Halo 1. The material is adapted from the Reclaimers Library and narrowed for this repository's Halo CE Xbox reverse-engineering work.

## Contents

| Path                                             | Scope                                                                                                                                                                           |
|--------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [scripting.md](scripting.md)                     | Concise notes on HaloScript execution, value types, limits, function families, and Xbox-relevant runtime behavior.                                                              |
| [scripting-reference.md](scripting-reference.md) | Cleaned local extract of the full Reclaimers HSC reference, including functions, external globals, and removed entries.                                                         |
| [engine.md](engine.md)                           | Concise notes on Blam engine systems and limits explicitly relevant to Halo CE for Xbox.                                                                                        |
| [code-audit.md](code-audit.md)                   | Initial cross-reference between imported C20 facts and this repository's Xbox reimplementation evidence.                                                                        |
| [pages/](pages/)                                 | Generated local extracts for selected runtime/reference C20 Halo 1 pages, including maps, engine systems, scripting limits, H1 Tool, source-data formats, and nested tag pages. |
| [tags/](tags/)                                   | Generated local notes for Halo 1 tag pages, including fields, runtime notes, script controls, and provenance.                                                                   |

## Import Scope

The generated importer currently includes `/h1/` pages that document runtime behavior, file/map formats, engine systems, scripting, source-data formats, H1 Editing Kit behavior that affects tags/maps, and nested tag pages. It intentionally excludes most `community-tools/` and tutorial-style `guides/` pages because they are primarily tool manuals or authoring walkthroughs rather than Halo CE Xbox runtime reference material.

The generated import index is [pages/index.md](pages/index.md). That file lists imported source paths and skipped page classes so coverage gaps are explicit.

## Source and License

These documents adapt content from the Reclaimers Library at `https://c20.reclaimers.net/`. The Reclaimers Library page footer states that its text is available under the Creative Commons Attribution-ShareAlike 3.0 license.

Adapted Reclaimers-derived content in this directory is therefore provided under Creative Commons Attribution-ShareAlike 3.0:

- License: `https://creativecommons.org/licenses/by-sa/3.0/`
- Source: `https://c20.reclaimers.net/h1/`
- Attribution: Reclaimers Library contributors
- Changes: content has been reformatted, summarized, filtered for Halo CE Xbox relevance, and organized into local Markdown pages for search and reverse-engineering reference.

When editing these files, preserve source attribution, keep derived content under CC BY-SA 3.0, and clearly distinguish verified Xbox binary facts from Reclaimers-derived notes or unresolved questions.

## Page Shape

| Section                       | Purpose                                                              |
|-------------------------------|----------------------------------------------------------------------|
| Summary                       | One-paragraph purpose and scope.                                     |
| Runtime behavior              | Engine behavior, limits, caches, synchronization, and special cases. |
| Reverse-engineering relevance | Systems, globals, tables, or binary areas worth searching.           |
| Xbox-specific notes           | Details that differ on or directly affect Halo CE for Xbox.          |
| Open questions                | Unknowns worth validating in the Xbox binary.                        |
| Provenance                    | Retrieval date and source attribution.                               |
