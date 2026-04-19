# Prototype inference

Goal: infer the narrowest defensible prototype.

Prefer:
- unknown pointer over invented struct
- integer width over speculative enum
- raw offset evidence over semantic field naming
- unnamed or generic parameter names over misleading names

Use:
- PUSH count
- stack cleanup
- callsite repetition
- return-value use
- register setup
- import signatures
- repeated access patterns across callers

Do not:
- infer rich semantics from one caller
- promote a guessed type to confirmed
- invent out-params without evidence
- introduce register annotations in kb for unimplemented functions
