# Output schema

Use this format unless the caller explicitly requests another format.

## Target
- address / symbol / object / hypothesis

## Scope
- what was examined
- what was not examined

## Confirmed
- binary-backed facts only

## Inferred
- best narrow interpretation supported by evidence

## Uncertain
- unresolved possibilities, conflicts, or weakly supported guesses

## Evidence
- callsites
- globals
- imports
- strings
- stack behavior
- register behavior
- operand widths
- decompiler/disassembly mismatches

## Proposed code
- faithful lift or prototype only if requested or useful

## Proposed kb deltas
- conservative changes only

## Validation
- what would most efficiently confirm or falsify the current interpretation

## Open questions
- the smallest next checks worth doing
