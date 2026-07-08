---
name: struct-assert
description: "offsetof, static_assert, sizeof check, define struct, struct definition, new struct, struct assert: Turn a struct-recovery evidence table into a conservative C89 struct — explicit padding, exact field widths/signedness, field_XX for unknowns, and cs()/co() sizeof/offsetof static asserts — following the src/types.h house style. Layout is dictated by the binary; the compiler proves it via the asserts."
---

# Struct Definition & Offset Assertion

Input: an evidence table from `struct-recovery` (offset, width, signedness, evidence).
Output: a C89 struct whose layout the compiler *proves* at build time.

## House style (src/types.h)

The repo already has assertion macros — use them, always, for every struct:

```c
#define cs(t, s)    static_assert(sizeof(t) == s)      /* types.h:19 */
#define co(t, f, o) static_assert(offsetof(t, f) == o) /* types.h:20 */
```

Existing conventions (see e.g. `packet_definition`, `packet_entry` in types.h):

```c
typedef struct object_header {
    int16_t  datum_salt;        ///< offset=0x00
    int16_t  field_02;          ///< offset=0x02  always 0, unknown purpose
    uint32_t flags;             ///< offset=0x04  bit5 = at-rest (evidence: FUN_xxxx)
    uint8_t  pad_08[4];         ///< offset=0x08  never observed accessed
    float    position[3];       ///< offset=0x0C
} object_header;
cs(object_header, 0x18);
co(object_header, field_02, 0x02);
co(object_header, position, 0x0C);
```

## Rules

1. **Every field width and signedness comes from the evidence table** (disasm operand
   sizes — MOVSX/MOVZX/word/byte ptr). Never widen "for convenience": an `int16_t` read
   as `int` is exactly the `[LOADW-WARN]` bug class (§24).
2. **Unknowns stay visibly unknown**: `field_XX` (accessed, meaning unknown) or
   `pad_XX[n]` (never observed accessed). Interpolating plausible names is banned —
   naming beyond the evidence goes through `naming-confidence`.
3. **Explicit padding, no compiler discretion.** Spell out every gap byte. Do not use
   `#pragma pack` unless the evidence proves misaligned fields; note it loudly if so.
4. **`co()` for every field, `cs()` for the size.** An offset without an assert is a
   guess; asserts are the whole point. When the total size is unproven, assert only the
   offsets and comment `/* size unproven — largest observed access at 0xNN */`.
5. **C89**: `typedef struct`, declarations before statements. (`static_assert` is
   provided by types.h's compiler shims — follow the existing pattern.)
6. **Unions**: model proven overlays as a named union member with a comment citing the
   discriminator; `co()` asserts apply to the union offset.
7. **Placement**: shared engine structs → `src/types.h` next to their subsystem
   neighbors; single-TU structs → top of that TU. Check first that the struct (or a
   partial version) doesn't already exist: `rtk rg 'offset=0x<key-offset>' src/types.h`
   — **extend the existing definition rather than defining a parallel one**.
8. **Evidence in comments**: each non-obvious field carries its evidence hook
   (`evidence: assert @0x..`, `FUN_xxxx movsx`) per `re-comment-capture` style.

## Verify

```bash
rtk python3 tools/build/build.py -q --target halo   # static asserts fire at compile
```

A failing `co()`/`cs()` is a *finding*, not an annoyance — the evidence table was wrong;
go back to `struct-recovery`, don't bend the struct to compile.

Defining the struct changes no codegen by itself. Rewriting accesses to use it is a
separate step with its own gate: `offset-to-struct`.
