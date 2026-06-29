# Halo 1 Reference: Creating a text file for string lists

## Source Page

- Source: `https://c20.reclaimers.net/h1/source-data/strings-txt/`
- Local path: `source-data/strings-txt/readme.md`
- Scope: Reclaimers-derived Halo 1 reference content retained for local QMD search; verify Xbox runtime behavior against the binary before using as lift evidence.

## Content

The unicode_ string_ list and string_ list tags are imported by Tool from a specially formatted .txt file. This page describes the format Tool expects for these files.

### Creating a text file for string lists

Unlike with HUD message text source files"), the source files for string lists can be anywhere under the HEK's the `data` directory and have any filename ending with ".txt". However, it's still a good idea to keep these files organized with what they're related to so it's easier to find or share your tags later.

A text file can be created on Windows by right clicking in the directory to bring up the context menu and creating a new text file.

The text file can contain multiple strings, each terminated with the line `###END-STRING###`. For example:

```
This is a string.
###END-STRING###
This is also a string.
It also has multiple lines cause Halo is cool like that.
###END-STRING###
We will also be talking about symbols like %jump later in the guide.
###END-STRING###
```

Some features like newlines in strings will not work in certain situations.

### Encoding

When saving the file, **make sure the file is set to encoding UTF-16 LE** for compiling unicode_ string_ list or UTF-8 for string_ list.

### Acknowledgements

Thanks to the following individuals for their research or contributions to this topic:

*   General_101 _(Documenting text format)_

## Provenance

Adapted from the Reclaimers Library Halo 1 page `https://c20.reclaimers.net/h1/source-data/strings-txt/`. Retrieved 2026-06-25. Text is retained locally as a cleaned reference extract for search and reverse-engineering context. See `docs/references/h1/README.md` for source attribution and CC BY-SA 3.0 licensing.
