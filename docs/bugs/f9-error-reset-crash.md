# F9 (Error Log Reset) Crash

**Status:** Open — not yet investigated (avoid F9 for now)
**Severity:** Low — debug-only function, easy to avoid
**Regression:** Confirmed — original cachebeta.xbe handles F9 fine
**Discovered:** 2026-05-16
**See also:** [Object data corruption bug](nan-object-lighting-crash.md)

## Symptoms

- Pressing F9 on the debug keyboard during gameplay freezes/crashes the game
- Stack trace shows patched code addresses (0x0065aa10, 0x006946d8)

## Mechanism

F9 is mapped to debug key entry [8] (key_code=0x09), which calls
`FUN_000ffe90` → `FUN_0008f630` (errors.obj, unported).

`FUN_0008f630` resets error log globals:
- `DAT_003361a0` = 0x2bb5c755 (magic/seed)
- `DAT_003361a4` = 0
- Iterates `DAT_003361b4[i]+4 = -1` for i in 0..DAT_003361b0
- Resets `DAT_003361b0`, `DAT_003361a8`, `DAT_003361aa`, `DAT_003361ac`
- Resets `DAT_003365c2`, `DAT_003365c4`, `DAT_003365bc`, `DAT_003365b4`
- Sets `DAT_00449ef1 = 1`

The crash likely occurs because our ported `error()` function (0x8f390,
errors.obj, ported=true) writes to these same globals with a different
layout or semantics than the original. When the unported reset function
clears them based on the original layout, it corrupts state that the
ported error system depends on.

## Fix Approach

- Port `FUN_0008f630` so it matches the ported error system's expectations
- OR: verify that the ported `error()` function uses the same globals at
  the same offsets as the original, and fix any discrepancies

## Debug Keyboard Map (for reference)

| Key | Code | Callback   | Action                              |
|-----|------|------------|-------------------------------------|
| Esc | 0x01 | 0x0ffdd0   | Set debug break flag                |
| F2  | 0x02 | 0x0ffe10   | Select previous AI encounter        |
| F3  | 0x03 | 0x0ffdf0   | Select next AI encounter            |
| F4  | 0x04 | 0x0ffe30   | Select next actor in encounter      |
| F4  | 0x04 | 0x0ffe50   | +Ctrl: select previous actor        |
| F5  | 0x05 | 0x0ffe70   | Cycle AI line-spray debug viz       |
| F6  | 0x06 | 0x0ffeb0   | ai_erase_all (delete all AI actors) |
| .   | 0x34 | 0x0ffed0   | (unknown)                           |
| F9  | 0x09 | 0x0ffe90   | Reset error log (CRASHES)           |

Modifier keys: 0x69 = left Ctrl, 0x6a = (second modifier).
Modifier combos: 0=none, 1=Ctrl only, 2=mod2 only, 3=both.
