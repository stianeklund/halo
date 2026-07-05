---
name: opencode-command-analyze-screenshot
description: "Antigravity/Gemini wrapper for OpenCode /analyze_screenshot. Use when the user asks for /analyze_screenshot, the analyze_screenshot command, or says: OCR a screenshot with local Gemma 4 and summarize what is on screen"
---

# OpenCode Command: /analyze_screenshot

This skill ports the existing OpenCode command `.opencode/commands/analyze_screenshot.md`
to Antigravity/Gemini.

When invoked:

- Treat the user's text after `/analyze_screenshot` or after the skill request as `$ARGUMENTS`.
- Follow the command prompt below as the task instructions.
- Keep using the repo's normal `AGENTS.md` and skill doctrine.

## Command Prompt

Analyze a screenshot image with the local Gemma 4 vision model, with OCR as the
primary goal.

Argument: $ARGUMENTS (`<image-path> [question]`)

Behavior:
1. Treat the first argument as the screenshot path when provided.
2. If no path is provided, capture PNG screenshots with
   `rtk python3 tools/xbox/xdbm_screenshot.py --host 127.0.0.1 --images 5 --png`, then use the newest PNG output.
3. Read the image directly and use the configured local model
   `lmstudio/gemma-4-e2b` for OCR and screen understanding.
4. Output the visible text first, as faithfully as possible:
   - preserve wording, capitalization, and line breaks when clear
   - mark uncertain text as `[unclear]`
   - do not invent missing text
5. After the OCR block, answer any extra user question from `$ARGUMENTS` using
   only what is visible in the screenshot.
6. If there is no extra question, give a short summary of the screen state,
   including obvious menu items, HUD text, prompts, dialog text, or errors.
7. If no screenshot can be found, say exactly that and ask for a PNG path.

Output format:
- `Image:` resolved path
- `OCR:` faithful transcription
- `Summary:` concise interpretation of what is on screen
- `Answer:` only if the user asked a follow-up question
