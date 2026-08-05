---
name: opencode-command-xbdm-getfile
description: "Antigravity/Gemini wrapper for OpenCode /xbdm-getfile. Use when the user asks for /xbdm-getfile, the xbdm-getfile command, or says: Pull a file from xemu or real Xbox over XBDM"
---

# OpenCode Command: /xbdm-getfile

This skill ports the existing OpenCode command `.opencode/commands/xbdm-getfile.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/xbdm-getfile` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Use `xbdm-getfile` and `halo-xbdm`.

Request: $ARGUMENTS

Pull the requested file, usually a `core_save`/`core.bin` dump, using the repo's XBDM/RDCP tooling. Preserve the source path, destination path, and command result in the final report.
