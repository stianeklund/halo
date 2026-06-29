# Halo 1 Reference: Types of data

## Source Page

- Source: `https://c20.reclaimers.net/h1/source-data/`
- Local path: `source-data/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

**Source data files** are the files found in your editing kit's `data` folder. These are the assets which have not yet been converted into tag format by Tool (or Sapien in the case of scripts).

### Types of data

Halo 1 makes use of the following source data formats:

*   .JMS models which are imported as gbxmodel, scenario_ structure_ bsp and others.
*   .WRL files which contain _error geometry_ when the above fails. Note that the location and name of the generated WRL file depends on the Tool verb being used and the version of Tool (HEK vs H1A-EK). See the Tool page.
*   .JMA, .JMM, etc files which are imported as model_ animations.
*   .tif textures which are imported as bitmap.
*   .hsc scenario scripts, most commonly used to orchestrate singleplayer levels (e.g. AI encounters, cutscenes).
*   .txt files imported as unicode_ string_ list.
*   .hmt") files imported as hud_ message_ text.

Instructions for importing these formats can be found on Tool's page. There are also community-made tools like Invader and Mozzarilla which cover some of these operations, and offer a different user experience or features.

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/source-data/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
