# kb update policy

Propose conservative kb.json updates only.

Allowed when evidence is strong:
- function name improvements
- parameter count narrowing
- calling convention narrowing
- high-confidence global meaning notes
- confirmed struct/field offset notes
- import or subsystem mapping supported by evidence

Disallowed or discouraged:
- broad semantic renames based on vibes
- speculative struct repacking
- aggressive type invention
- @<reg> entries for functions that are not implemented
- project-wide naming changes from weak local evidence

When proposing kb changes, separate:
- High-confidence deltas
- Tentative notes
- Things that should remain unknown
