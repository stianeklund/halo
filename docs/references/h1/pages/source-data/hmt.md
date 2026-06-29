# Halo 1 Reference: Creating a text file for HUD messages

## Source Page

- Source: `https://c20.reclaimers.net/h1/source-data/hmt/`
- Local path: `source-data/hmt/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The **.hmt** (HUD message text) file format contains source text content which, when imported by Tool, becomes hud_ message_ text tags.

### Creating a text file for HUD messages

Start by going to the root directory of your level and creating a text file. The contents of the text file **differs** from the formatting used for string_ list. Instead you will define a variable and set the value for the string. Here is an example:

```
string1=Well, what do we have here?
string2=If I'm not mistaken this looks like a list of strings.
string3=A lot of neat things can be done with strings.
```

Once you've set the contents of the text file make sure to save the file with the encoding set to **UTF-16 LE**. Make sure the file is named the following:

`hud messages.hmt`

Tool looks for a file with this name and extension specifically when compiling _hud\_ message\_ text_ tags. On top of needing a specific name, it is also required for the .hmt file to reside in the root directory of the scenario the tag is intended for. For example, for the "tutorial" scenario we need to save the file at the following location:

`(HEK Install Path)\data\levels\test\tutorial\hud messages.hmt`

The command requires this setup in order to complete the task successfully. You can now use the command `hud-messages` to import your source file.

If you're using hud_ message_ text for scripting with _display\_ scenario\_ help_, then be aware that only default singleplayer paths are considered valid.

### Button types

HMT can contain special keywords which are shown as key bindings or controller buttons ingame:

*   `%a-button`
*   `%b-button`
*   `%x-button`
*   `%y-button`
*   `%black-button`
*   `%white-button`
*   `%left-trigger`
*   `%right-trigger`
*   `%dpad-up`
*   `%dpad-down`
*   `%dpad-left`
*   `%dpad-right`
*   `%start-button`
*   `%back-button`
*   `%left-thumb`
*   `%right-thumb`
*   `%left-stick`
*   `%right-stick`
*   `%action`
*   `%throw-grenade`
*   `%primary-trigger`
*   `%integrated-light`
*   `%jump`
*   `%use-equipment`
*   `%rotate-weapons`
*   `%rotate-grenades`
*   `%zoom`
*   `%crouch`
*   `%accept`
*   `%back`
*   `%move`
*   `%look`
*   `%custom-1`
*   `%custom-2`
*   `%custom-3`
*   `%custom-4`
*   `%custom-5`
*   `%custom-6`
*   `%custom-7`
*   `%custom-8`

### Known issues

Some button types do not show the correct bindings ingame in MCC, but do work in Standalone. This is known to affect at least `%back`, but does NOT affect:

*   `%primary-trigger`
*   `%throw-grenade`
*   `%accept`
*   `%zoom`

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   Aerocatia _(Button keywords in Invader source)_
*   General_101 _(Documenting HMT format and text symbols)_
*   Photolysis _(Documenting MCC button display issue)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/source-data/hmt/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
