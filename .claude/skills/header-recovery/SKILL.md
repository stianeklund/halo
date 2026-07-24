---
name: header-recovery
tier: agent
triggers: ["header file", "create header", ".h file", "new header", "where does this struct go", "split types.h", "types.h is huge", "include guard", "header layout"]
description: Recover the original Bungie header files and place recovered types in them, instead of piling every struct into src/types.h. Header paths are binary-proven from __FILE__ strings stamped by asserts inside header-resident inline functions. Headers carry types/macros/inlines only — function declarations come from kb.json via build/generated/decl.h. Zero codegen risk, gated on build + unchanged VC71.
---

# Header Recovery

The original source had per-subsystem headers. Our lift has almost none: as of
2026-07-24, **8 `.h` files for 166 `.c` files**, and `src/types.h` is 1080 lines
holding ~40 unrelated structs. Every recovered struct lands there by default,
so the monolith grows with every lift and never shrinks.

This is the *un-recovered* shape, exactly like raw offset arithmetic is. The
faithful shape is the header the binary proves existed.

## 1. Never invent a header name

**A header may only be created if the binary names it.** Asserts inside inline
functions that live in a header stamp that header's path into `__FILE__`, so
recovered headers appear in the XBE as literal strings.

Extract them:

```bash
rtk python3 -c "
import importlib.util, re
from pathlib import Path
spec=importlib.util.spec_from_file_location('c','tools/audit/check_callee_reg_args.py')
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
data,_=m.load_xbe(Path('halo-patched/cachebeta.xbe'))
for mo in sorted({mo.group().rstrip(b'\x00').decode('ascii','replace')
                  for mo in re.finditer(rb'[A-Za-z]:\\\\?[A-Za-z0-9_\\\\.]{4,80}\.h\x00', data)}):
    print(mo)
"
```

A hit proves three things: the header existed, its **exact directory**, and that
it contained at least one inline function with an assert.

### Known headers (2026-07-24 sweep, 13 total)

| Original path | Our path |
|---|---|
| `c:\halo\source\ai\actor_type_definitions.h` | `src/halo/ai/actor_type_definitions.h` |
| `c:\halo\source\ai\encounters.h` | `src/halo/ai/encounters.h` |
| `c:\halo\source\ai\path.h` | `src/halo/ai/path.h` |
| `c:\halo\source\hs\hs_library_internal_compile.h` | `src/halo/hs/…` |
| `c:\halo\source\hs\hs_library_internal_runtime.h` | `src/halo/hs/…` |
| `c:\halo\source\objects\objects.h` | `src/halo/objects/objects.h` |
| `c:\halo\source\objects\widgets\widget_types.h` | `src/halo/objects/widgets/…` |
| `c:\halo\source\sound\sound_classes.h` | `src/halo/sound/sound_classes.h` |
| `c:\halo\source\sound\sound_definitions.h` | `src/halo/sound/…` |
| `c:\halo\source\sound\sound_dsound.h` | `src/halo/sound/sound_dsound.h` |

Plus `bitmaps_inlines.h`, `real_math.h`, `reference_lists.h` (short-name hits;
re-run the sweep to pin their directories before creating them).

If a subsystem has **no** proven header, its types stay in `types.h`. Waiting is
correct; a plausible-sounding `units.h` we made up is not recovery.

## 2. The decl.h boundary

Function declarations are **generated from kb.json** into
`build/generated/decl.h`. A recovered header must never hand-declare a function
that kb.json owns — you would create a second source of truth that silently
drifts from the `@<reg>` annotations.

A recovered header carries only:
- `typedef`/`struct` definitions **and their `cs()`/`co()` asserts** (the asserts
  move with the struct — they are its proof, not decoration)
- `#define`s and enums (see `const-enum-recovery`)
- `static inline` functions, if the binary shows one lived there

## 3. Placement: which header does a type belong in?

| Tier | Evidence | Action |
|---|---|---|
| A | An assert string or the type's own name ties it to that subsystem, **or** only TUs under that header's directory use it | Move it into the header |
| B | Used across unrelated subsystems | Leave in `types.h` — it is genuinely shared |
| C | Subsystem has no binary-proven header | Leave in `types.h`, do not invent one |

Worked example: `encounter_definition`'s recovered name comes verbatim from the
assert `"encounter_definition->squads.count <= MAXIMUM_SQUADS_PER_ENCOUNTER"` in
`encounters.c`, and `c:\halo\source\ai\encounters.h` is binary-proven — Tier A,
so it belongs in `src/halo/ai/encounters.h`, not `types.h`.

## 4. Include discipline

- Guard macro mirrors the recovered path: `HALO_AI_ENCOUNTERS_H`.
- Each header is **self-contained**: it includes what it uses (usually just
  `types.h` for the scalar typedefs and the `cs`/`co` macros) and compiles alone.
- `types.h` must not include the new headers — dependency points *inward*, from
  subsystem headers to `types.h`. A cycle here will surface as a confusing
  redefinition error, not a clean diagnostic.
- The `.c` files that used the type include the new header.
- **Hazard:** moving file-scope declarations is exactly the case where
  `maintain.py` strands `#include`/`#define`/`typedef` lines (see memory
  `project_maintain_reorder_strands_file_scope_decls`); `--check` does not catch
  it. Re-read the head of any file `maintain.py` reorders after a header move.

## 5. Gates

Headers emit no code, so the bar is absolute:

1. `tools/build/build.py -q --target halo` — clean.
2. **VC71 match unchanged for every affected TU.** Not "no big drop" — *unchanged*.
   A header move that shifts a single instruction means something moved
   semantically (a type width changed, a struct gained padding, a macro now
   resolves differently). Investigate; do not accept it.
3. The `cs()`/`co()` asserts still compile in their new home — that is what proves
   the layout survived the move.
4. `tools/audit/check_readability.py --changed-only` for the touched `.c` files.

## 6. Commit discipline

**One header per commit** (`Separation` rule). A reviewer must be able to read
"commit 4 = encounters.h" and skim it. Never mix a header move with a lift, a
rename, or an offset→field rewrite — those are `/lift` and the `cleanup` ladder.

Related: [`struct-assert`](../struct-assert/SKILL.md) (defines the struct and its
asserts), [`struct-recovery`](../struct-recovery/SKILL.md) (produces the evidence
table), [`naming-confidence`](../naming-confidence/SKILL.md) (what may be named).
