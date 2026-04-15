---
description: OCR a screenshot with local Gemma 4 and summarize what is on screen
agent: fast
model: lmstudio/gemma-4-e2b
subtask: true
---

Analyze a screenshot image with the local Gemma 4 vision model, with OCR as the
primary goal.

Argument: $ARGUMENTS (`<image-path> [question]`)

Behavior:
1. Treat the first argument as the screenshot path when provided.
2. If no path is provided, look for the most recent file matching `xemu*.png` in:
   - the repo root
   - `/mnt/c/Users/*/AppData/Local/Temp/`
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
