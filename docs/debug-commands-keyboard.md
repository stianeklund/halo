# Debug Commands, Cheats, and Keyboard Input

## 1. Quick Reference: The Best of Debug & Cheats

If you want to know how to fly around and spawn things, here are the most useful commands:

| Action | Command / Key | How to use |
|--------|---------------|------------|
| **Open Console** | <kbd>~</kbd> (Tilde) | Press `~` or \` to open the console and type commands. |
| **Teleport to camera** | `(cheat_teleport_to_camera)` | While flying in freecam, type this to move your player to the camera. |
| **Give all weapons** | `(cheat_all_weapons)` | Drops every weapon in the game at your feet. |
| **Give all vehicles** | `(cheat_all_vehicles)` | Spawns one of every vehicle type nearby. |
| **Give all powerups** | `(cheat_all_powerups)` | Drops overshields and active camo near you. |
| **Active Camo** | `(cheat_active_camouflage)` | Grants you active camo instantly. |
| **Erase all AI** | <kbd>F6</kbd> | Deletes every AI actor in the level immediately. |
| **Cycle AI line-spray** | <kbd>F5</kbd> | Cycles through AI debug drawing modes. |
| **Free Camera** | `(camera_control 1)` | Detaches the camera from the player. `0` to reattach. |

---

## 2. Debug Keyboard Keys

The debug keyboard dispatcher is `debug_keys_update()` in `src/halo/main/debug_keys.c`.
It walks a definition table loaded from the binary — pointer at `0x31f9c8`, table at
`0x31f9cc`. Each entry is 0x14 (20) bytes.

### Entry Layout

| Offset | Type | Field |
|--------|------|-------|
| `+0x00` | int16_t | `key_code` — internal key code (see [Internal Key Codes](#internal-key-codes)) |
| `+0x02` | int16_t | `modifier_index` — which modifier keys are required (see below) |
| `+0x04` | void (\*)(uint32_t) | `callback` — called with `1` on key-down, `0` on key-up |
| `+0x08` | uint8_t | `state_mode` — `0` = mirror key state, `1` = toggle on release |
| `+0x09` | uint8_t[3] | padding |
| `+0x0c` | char \* | `state` — optional byte written by dispatcher |
| `+0x10` | char \* | `description` — human-readable name string (NULL = end of table) |

### Modifier Index

The modifier keys are key codes `0x69` (axis L_X) and `0x6a` (axis L_Y). The index selects:

| Index | Key 0x69 | Key 0x6a | Notes |
|-------|----------|----------|-------|
| `0` | not down | not down | No modifier |
| `1` | down | not down | |
| `2` | not down | down | |
| `3` | down | down | Both modifiers |

### Full Key Table (extracted from binary at 0x31f9cc)

| Key | Code | Modifier | Callback | Description | Action |
|-----|------|----------|----------|-------------|--------|
| Esc | `0x01` | 0 | 0x000ffdd0 | "Select Prev Encounter" | Cycles to previous AI encounter display |
| F2 | `0x02` | 0 | 0x000ffe10 | "Select Next Encounter" | Cycles to next AI encounter display |
| F3 | `0x03` | 0 | 0x000ffdf0 | "Select Next Encounter" | Cycles to next AI encounter display |
| F4 | `0x04` | 0 | 0x000ffe30 | "Select This Actor" | Selects next actor in current encounter |
| Ctrl+F4 | `0x04` | 1 | 0x000ffe50 | "Select Prev Actor" | Selects previous actor in current encounter |
| F5 | `0x05` | 0 | 0x000ffe70 | "Cycle Line-Spray Debug" | Cycles AI line-spray debug visualization mode |
| F6 | `0x06` | 0 | 0x000ffeb0 | "Erase All Actors" | Calls `ai_erase_all`; deletes all AI actors |
| `.` | `0x34` | 0 | 0x000ffed0 | "(unknown)" | Unknown — not yet identified |
| F9 | `0x09` | 0 | 0x000ffe90 | NULL (terminator) | Resets error log (crashes in patched builds) |

### Debug Key Callback Source Details

| Address | Name | Behavior |
|---------|------|----------|
| `0x000ffdd0` | — | Sets global debug break flag when called with non-zero arg |
| `0x000ffdf0` | — | Calls `ai_debug_change_selected_encounter(1)` (next) |
| `0x000ffe10` | — | Calls `ai_debug_change_selected_encounter(-1)` (prev) |
| `0x000ffe30` | — | Selects next actor in current encounter |
| `0x000ffe50` | — | Selects previous actor in current encounter |
| `0x000ffe70` | — | Cycles AI line-spray debug visualization mode |
| `0x000ffeb0` | — | Sets `*(char *)0x5ac9c1 = 1` (ai_erase_all trigger) |
| `0x000ffed0` | — | Unknown — needs analysis |
| `0x000ffe90` | — | Calls error-log reset function (crashes in patched builds) |

The F9 crash is suspected to be a mismatch between the unported reset helper
and the currently ported error system.

---

## 3. Internal Key Codes

The Xbox keyboard driver (XInput) maps hardware scancodes to internal key codes
via `input_vkey_to_keycode_table()` at binary address `0x281160`. The table is
256 `int16_t` entries (512 bytes) indexed by Xbox virtual key code. A value of
`-1` (0xffff) means unmapped.

### VKey → Keycode Mapping (extracted from binary at 0x281160)

| Xbox VKey | Internal Code | Notes |
|-----------|---------------|-------|
| 0x00–0x0F (unmapped) | — | Xbox gamepad/misc keys |
| 0x10 (Shift) | `0x1d` (29) | |
| 0x11 (Ctrl) | `0x1e` (30) | |
| 0x12 (Alt) | `0x1f` (31) | |
| 0x14 (Caps) | `0x38` (56) | |
| 0x15–0x1F (IME) | — | |
| 0x20 (Space) | `0x44` (68) | |
| 0x21 (PgUp) | `0x2c` (44) | |
| 0x22 (PgDn) | `0x2d` (45) | |
| 0x23 (End) | `0x2e` (46) | |
| 0x24 (Home) | `0x2f` (47) | |
| 0x25 (Left) | `0x30` (48) | |
| 0x26 (Up) | `0x31` (49) | |
| 0x27 (Right) | `0x32` (50) | |
| 0x28 (Down) | `0x33` (51) | |
| 0x29 (Select) | `0x3a` (58) | |
| 0x2A (Print) | `0x3b` (59) | |
| 0x2B (Exec) | `0x3c` (60) | |
| 0x2C (PrnScrn) | `0x3d` (61) | |
| 0x2D (Ins) | `0x3e` (62) | |
| 0x2E (Del) | `0x3f` (63) | |
| 0x2F (Help) | `0x40` (64) | |
| 0x30–0x39 (0–9) | `0x0d–0x16` (13–22) | Top-row digits |
| 0x3A–0x40 | — | Unmapped |
| 0x41–0x5A (A–Z) | `0x1a–0x33` (26–51) | Uppercase only (lowercase passed via flags) |
| 0x5B (LWin) | `0x2a` (42) | |
| 0x5C (RWin) | `0x2b` (43) | |
| 0x5D (Apps) | `0x31` (49) | |
| 0x5E–0x5F | — | |
| 0x60 (Num0) | `0x34` (52) | Numpad keys |
| 0x61 (Num1) | `0x1c` (28) | |
| 0x62 (Num2) | `0x41` (65) | |
| 0x63 (Num3) | `0x1b` (27) | |
| 0x64 (Num4) | `0x42` (66) | |
| 0x65 (Num5) | `0x43` (67) | |
| 0x66 (Num6) | `0x10` (16) | |
| 0x67–0x69 | — | |
| 0x6A (Num*) | `0x37` (55) | |
| 0x6B (Num+) | `0x29` (41) | |
| 0x6C (NumEnter) | — | |
| 0x6D (Num-) | `0x2b` (43) | |
| 0x6E (Num.) | `0x2a` (42) | |
| 0x6F (Num/) | `0x37` (55) | |
| 0x70–0x78 (F1–F9) | `0x01–0x09` (1–9) | Function keys 1–9 |
| 0x79–0x7F | — | F10–F16 generally unmapped |
| 0x80–0x87 | — | Unmapped |
| 0x90 (NumLk) | `0x36` (54) | |
| 0x91 (Scroll) | `0x38` (56) | |
| 0xA0–0xA5 | — | Left/Right modifiers (remapped elsewhere) |
| 0xBA (;) | `0x27` (39) | US semicolon |
| 0xBB (=) | `0x0c` (12) | Equals |
| 0xBC (,) | `0x23` (35) | Comma |
| 0xBD (-) | `0x0b` (11) | Minus |
| 0xBE (.) | `0x34` (52) | Period |
| 0xBF (/) | `0x25` (37) | Forward slash |
| 0xC0 (`) | `0x10` (16) | Backtick/tilde |
| 0xDB ([) | `0x21` (33) | Left bracket |
| 0xDC (\) | `0x38` (56) | Backslash |
| 0xDD (]) | `0x22` (34) | Right bracket |
| 0xDE (') | `0x28` (40) | Apostrophe |
| 0xDF–0xFF | — | Unmapped |

### Known Important Internal Codes (for debug and console)

| Code | VKey(s) | Usage |
|------|---------|-------|
| `0x01` | F1 | Debug key Esc binding |
| `0x02` | F2 | Debug key F2 binding |
| `0x03` | F3 | Debug key F3 binding |
| `0x04` | F4 | Debug key F4 binding |
| `0x05` | F5 | Debug key F5 binding |
| `0x06` | F6 | Debug key F6 binding |
| `0x09` | F9 | Debug key F9 binding |
| `0x10` | Num6, ` (backtick) | **Console toggle** (tilde/backtick) |
| `0x1e` | Ctrl | **Console enter/submit** |
| `0x34` | Numpad0, Period | Debug key `.` binding, also period on main keyboard |
| `0x38` | Caps, Scroll, \ | **Console escape/close** |
| `0x4d` | (navigation) | **Console history up** (up arrow) |
| `0x4e` | (navigation) | **Console history down** (down arrow) |
| `0x66` | Num6 | **Console cancel/close** (backspace) |
| `0x69` | (axis) | **Modifier key A** — left stick X-axis as key |
| `0x6a` | (axis) | **Modifier key B** — left stick Y-axis as key |

### Digit key mapping (top row vs numpad)

| Digit | Top row code | Numpad code |
|-------|-------------|-------------|
| 0 | `0x0d` (13) | `0x34` (52) |
| 1 | `0x0e` (14) | `0x1c` (28) |
| 2 | `0x0f` (15) | `0x41` (65) |
| 3 | `0x10` (16) | `0x1b` (27) |
| 4 | `0x11` (17) | `0x42` (66) |
| 5 | `0x12` (18) | `0x43` (67) |
| 6 | `0x13` (19) | `0x10` (16) |
| 7 | `0x14` (20) | — |
| 8 | `0x15` (21) | — |
| 9 | `0x16` (22) | — |

### Letter key mapping (A–Z)

| Letter | Code | Letter | Code | Letter | Code |
|--------|------|--------|------|--------|------|
| A | `0x1a` (26) | J | `0x1a+9` (35) | S | `0x1a+18` (44) |
| B | `0x1b` (27) | K | `0x1a+10` (36) | T | `0x1a+19` (45) |
| C | `0x1c` (28) | L | `0x1a+11` (37) | U | `0x1a+20` (46) |
| D | `0x1d` (29) | M | `0x1a+12` (38) | V | `0x1a+21` (47) |
| E | `0x1e` (30) | N | `0x1a+13` (39) | W | `0x1a+22` (48) |
| F | `0x1f` (31) | O | `0x1a+14` (40) | X | `0x1a+23` (49) |
| G | `0x20` (32) | P | `0x1a+15` (41) | Y | `0x1a+24` (50) |
| H | `0x21` (33) | Q | `0x1a+16` (42) | Z | `0x1a+25` (51) |
| I | `0x22` (34) | R | `0x1a+17` (43) | | |

Note: Key code `0x1a` = A, code `0x33` = Z. The console `hs_console_evaluate()`
path uses ASCII, not internal codes, for command text.

---

## 4. Console Controls

The debug console is handled by `console_update()` in `src/halo/main/console.c`.

### Opening the Console

| Action | Internal Code | VKey |
|--------|---------------|------|
| Toggle open/close | `0x10` | Backtick/tilde (`` ` ``) or Numpad 6 |

### Console Key Bindings (when open)

| Key/Event | Internal Code | Behavior |
|-----------|---------------|----------|
| Tilde/backtick | `0x10` | Close the console |
| Enter | `0x1e` | Submit command — wraps in S-expression and evaluates via `hs_console_evaluate()` |
| Escape | `0x38` | If input buffer non-empty, submit command then close; if empty, close immediately |
| Backspace | `0x66` | Same behavior as Escape |
| Up arrow | `0x4d` | Browse older command history |
| Down arrow | `0x4e` | Browse newer command history |

### Console History

| Property | Value |
|----------|-------|
| Ring size | 8 slots |
| Slot size | 255 bytes each |
| Total history | 2040 bytes at `0x46d124` |
| Navigation | `(head - browse + 8) % 8 * 255` |

### Input Buffer

| Property | Value |
|----------|-------|
| Buffer address | `0x46d018` |
| Max length | 1024 tokens |
| Cursor state | At `0x46d118` via `edit_text_set_cursor_to_end` |

### Console Startup

`console_startup()` reads one of:

| Mode | Path |
|------|------|
| Retail/game | `d:\init.txt` |
| Editor mode | `editor_d:\init.txt` |

Each non-empty line is stored in the history ring, then evaluated with
`hs_console_evaluate()`. Lines starting with `;` are treated as comments.

### Tab Completion

`console_process_enter()` on Tab tab-completes the current token by enumerating
all matching HaloScript function names via `hs_tokens_enumerate()`. If more than
16 matches are found, they're printed in batches of 4 per line.

---

## 5. Cheat Controller (`d:\cheats.txt`)

Handled by `cheats_load_from_file()` and `cheats_update()` in
`src/halo/game/cheats.c`.

### File Format

```
d:\cheats.txt
```
Up to 16 lines, each up to 199 characters. Lines are stripped of `\r\n\t;`.
Lines attached to slots 12 (back) and 13 (start) are rejected with:
```
Cannot execute cheats attached to the back or start button
```

### Cheat Slot → Gamepad Button Mapping

| Slot | Gamepad byte offset `+0x10` | Button | Notes |
|------|------------------------------|--------|-------|
| 0 | `+0x10` byte 0 | A | |
| 1 | `+0x10` byte 1 | B | |
| 2 | `+0x10` byte 2 | X | |
| 3 | `+0x10` byte 3 | Y | |
| 4 | `+0x10` byte 4 | Black | |
| 5 | `+0x10` byte 5 | White | |
| 6 | `+0x10` byte 6 | Left Trigger | |
| 7 | `+0x10` byte 7 | Right Trigger | |
| 8 | `+0x10` byte 8 | D-Pad Up | |
| 9 | `+0x10` byte 9 | D-Pad Down | |
| 10 | `+0x10` byte 10 | D-Pad Left | |
| 11 | `+0x10` byte 11 | D-Pad Right | |
| 12 | `+0x10` byte 12 | Back | **Rejected** — cannot bind |
| 13 | `+0x10` byte 13 | Start | **Rejected** — cannot bind |
| 14 | `+0x10` byte 14 | Left Thumb | |
| 15 | `+0x10` byte 15 | Right Thumb | |

### Pre-defined Cheat Commands

HaloScript has 6 built-in `cheat_` commands. You can type these into the console or bind them in `cheats.txt`:

* `cheat_all_powerups` : drops all powerups near player
* `cheat_all_weapons` : drops all weapons near player
* `cheat_all_vehicles` : drops all vehicles on player
* `cheat_teleport_to_camera` : teleports player to camera location
* `cheat_active_camouflage` : gives the player active camouflage
* `cheat_active_camouflage_local_player` : gives the player active camouflage

### Execution Flow

1. Global `cheat_controller` flag must be set (byte at some global)
2. For each local player, get their gamepad state
3. If gamepad connected (`+0x1d` byte non-zero), iterate 16 button bytes at `+0x10`
4. If a button is pressed AND its cheat slot string is non-empty:
   - Set director context to this player
   - Log the cheat command to console
   - Run `hs_console_evaluate(cheat_string)`
   - If evaluation fails, clear that cheat slot
5. `cheats_load_from_file()` is called during `cheats_initialize_for_new_map()`

---

## 6. Input State Recording (Playback/Automation)

The input system supports recording and playback of controller state for testing
via sentinel files on the `D:` drive. This effectively allows driving the game using
prerecorded input.

| Sentinel file | Mode | Behavior |
|---------------|------|----------|
| `d:\write.xts` | Mode 3 (Record) | Writes `input_gamepad_state` packets (0x28 bytes each) to `d:\state.data` every tick |
| `d:\read.xts` | Mode 4 (Playback) | Reads packets from `d:\state.data` every tick, driving the game |
| `d:\loop.xts` | Mode 5 (Loop) | Reads packets, seeks to start on EOF for continuous loop playback |

Handled by `input_check_state_mode()` in `src/halo/input/input_xbox.c`. This is fully functional and can be used for automation or reproducing precise state.

---

## 7. HaloScript Command Table

The built-in HaloScript function table is at `0x2f1588` and has `0x1a2`
(418) entries. `hs_function_table_get()` returns a descriptor by index, and
`hs_find_function_by_name()` searches by name (case-insensitive).

### Descriptor Layout (0x1c bytes each)

| Offset | Size | Field | Source Evidence |
|--------|------|-------|-----------------|
| `+0x00` | u32 | `flags` | Return type / classification |
| `+0x04` | u32 | `name_ptr` | Pointer to function name string |
| `+0x08` | u32 | `type_check_cb` | Compile-time type-check callback |
| `+0x0c` | u32 | `eval_cb` | Runtime evaluator callback |
| `+0x10` | u32 | `help_ptr` | Help/documentation string |
| `+0x14` | u32 | `syntax_ptr` | Usage/syntax prototype string |
| `+0x18` | i16 | `arg_count` | Number of typed arguments |
| `+0x1a` | u8[] | `arg_types` | Variable-length argument type bytes |

Confirmed via reverse-engineering of `hs_find_function_by_name` (name at +4),
`hs_doc` (help at +0x10), `hs_help` (help at +0x10), `hs_macro_function_evaluate`
(arg count at +0x18 as i16, arg types at +0x1a), and direct memory reads of the
table at 0x2f1588 and descriptor data at 0x26f448–0x272790.

### Flag Values (Return Type / Classification)

| Flag | Count | Meaning | Examples |
|------|-------|---------|---------|
| `3` | 5 | Control flow construct | `begin`, `begin_random`, `if`, `cond`, `set` |
| `4` | 310 | Statement (void return) | `sleep`, `wake`, `print`, `object_create`, `unit_kill` |
| `5` | 57 | Boolean predicate | `and`, `or`, `=`, `>`, `<`, `volume_test_object` |
| `6` | 15 | Arithmetic / numeric | `+`, `-`, `*`, `/`, `min`, `max`, `real_random_range` |
| `7` | 20 | Mixed / derived | `list_count`, `random_range`, `recording_time` |
| `8` | 2 | Time-interval | `game_time`, `sound_impulse_time` |
| `23` | 3 | Object list | `players`, `vehicle_riders`, `ai_actors` |
| `32` | 2 | Difficulty | `game_difficulty_get`, `game_difficulty_get_real` |
| `37` | 1 | List element | `list_get` |
| `38` | 3 | Unit cast | `unit`, `vehicle_driver`, `vehicle_gunner` |

### Console Evaluate Path

`hs_console_evaluate()` in `src/halo/hs/hs.c` handles command input from
console, cheats, and init files:

| Input starts with | Behavior |
|-------------------|----------|
| `(` | Pass through as S-expression |
| Global name + arguments | Wrap as `(set <name> <args>)` |
| Global name only | Evaluate as global read |
| Anything else | Wrap as `(<command>)` |

Comments after `;` blank the command before evaluation.

### Console Help Handlers

| Handler | Evaluator of | Behavior |
|---------|-------------|----------|
| `FUN_000c4ff0` (0xc4ff0) | `script_doc` | Calls `hs_doc()` (0xc4e90) to write all command docs to `hs_doc.txt` |
| `FUN_000c5010` (0xc5010) | `help` | Calls `hs_macro_function_evaluate` then `hs_help()` (0xc4e20) to print help for a named function |

### Command Quick Reference by Category

#### Control Flow (indices 0–4)

```
(begin <expr1> [<expr2> ...])       — evaluate sequence, return last
(begin_random <expr1> [<expr2> ...]) — evaluate in random order
(if <bool> <then> [<else>])         — conditional
(cond (<bool1> <val1>) [(<bool2> <val2>) ...]) — first-true match
(set <name> <value>)                 — assign global
```

#### Boolean Logic (indices 5–18)

```
(and <bool1> [<bool2> ...])  — true if all true
(or <bool1> [<bool2> ...])   — true if any true
(not <bool>)                 — logical NOT
(= <a> <b>)                  — equality
(!= <a> <b>)                 — inequality
(> <a> <b>)                  — greater than
(< <a> <b>)                  — less than
(>= <a> <b>)                 — greater or equal
(<= <a> <b>)                 — less or equal
```

#### Arithmetic (indices 7–12)

```
(+ <num1> [<num2> ...])    — sum
(- <num1> <num2>)          — difference
(* <num1> [<num2> ...])    — product
(/ <num1> <num2>)          — quotient
(min <num1> [<num2> ...])  — minimum
(max <num1> [<num2> ...])  — maximum
```

#### Script Control (indices 19–22)

```
(sleep <ticks> [<script>])             — pause execution
(sleep_until <bool> [<check_interval>]) — pause until condition true
(wake <script_name>)                    — wake a sleeping script
(inspect <expr>)                        — print value for debugging
```

#### Type Conversion (indices 23, 45–46)

```
(unit <object>)                         — cast object to unit
(list_get <list> <index>)               — get item from object list
(list_count <list>)                     — count items in list
```

#### Object Operations (indices 30–44, 71–87)

```
(volume_teleport_players_not_inside <volume> <flag>)
(volume_test_object <volume> <object>)            → bool
(volume_test_objects <volume> <objects>)           → bool
(volume_test_objects_all <volume> <objects>)       → bool
(object_teleport <object> <flag>)
(object_set_facing <object> <flag>)
(object_set_shield <object> <real>)
(object_set_permutation <object> <region> <perm>)
(object_create <name>)
(object_destroy <name>)
(object_create_anew <name>)
(object_create_containing <substring>)
(object_create_anew_containing <substring>)
(object_destroy_containing <substring>)
(object_destroy_all)
(object_set_ranged_attack_inhibited <object> <bool>)
(object_set_melee_attack_inhibited <object> <bool>)
(object_set_collideable <object> <bool>)
(object_set_scale <object> <real> <ticks>)
(objects_attach <parent> <parent_marker> <child> <child_marker>)
(objects_detach <parent> <child>)
(garbage_collect_now)
(object_cannot_take_damage <object>)
(object_can_take_damage <object>)
(object_beautify <object> <bool>)
(objects_predict <name>)
(object_type_predict <type_name>)
(object_pvs_set_object <object>)
(object_pvs_set_camera <cutscene_camera_point>)
(object_pvs_clear)
(object_pvs_activate <name>)      — alias for object_pvs_set_object
(objects_dump_memory)
```

#### Effects (indices 47–55, 88–92)

```
(effect_new <effect> <flag>)
(effect_new_on_object_marker <effect> <object> <marker>)
(damage_new <damage> <flag>)
(damage_object <damage> <object>)
(objects_can_see_object <units> <degrees> <object>)       → bool
(objects_can_see_flag <units> <degrees> <flag>)            → bool
(objects_delete_by_definition <definition>)
(sound_set_gain <gain>)              — "absolutely do not use this"
(sound_get_gain)                     — "absolutely do not use this either"
(render_lights <bool>)
(render_effects <bool>)
(scenery_get_animation_time <scenery>)                      → ticks
(scenery_animation_start <scenery> <anim> <ticks>)
(scenery_animation_start_at_frame <scenery> <anim> <ticks> <frame>)
```

#### Unit Operations (indices 93–147)

```
(unit_can_blink <unit> <bool>)
(unit_open <unit>)
(unit_close <unit>)
(unit_kill <unit>)
(unit_kill_silent <unit>)
(unit_get_custom_animation_time <unit>)                  → ticks
(unit_stop_custom_animation <unit>)
(unit_does_target <unit>)
(unit_has_weapon <unit>)                                 → bool
(unit_has_weapons <unit>)                                → bool
(unit_set_weapon <unit> <weapon>)
(unit_lower_weapon <unit>)
(unit_raise_weapon <unit>)
(unit_reload <unit>)
(unit_command <unit> <command_type> <target>)
(unit_set_skin <unit> <skin>)
(unit_bump_visibility <unit> <bool>)
(unit_set_ranged_attack_inhibited <unit> <bool>)
(unit_set_melee_attack_inhibited <unit> <bool>)
(unit_targeting_priorities_enable <unit>)
(unit_targeting_priorities_disable <unit>)
(unit_set_emotion_animation <unit> <emotion>)
(unit_drop_weapon <unit>)
(unit_drop_weapon_arbitrary <unit>)
(unit_make_invincible <unit>)
(unit_forward <unit>)                                     → bool
(unit_current_health <unit>)                              → real
(unit_max_health <unit>)                                  → real
(unit_has_max_fragile_health <unit>)                      → bool
(unit_set_parasite_target <unit> <target>)
(unit_get_parasite_target <unit>)
(unit_set_health_bar <unit> <bool>)
(unit_set_health_bar_size <unit> <size>)
(unit_set_health_bar_screen <unit> <screen>)
(unit_lock <unit>)
(unit_unlock <unit>)
(unit_test_coop_progress <unit>)
```

#### AI Operations (indices 156–244)

```
(ai_free <encounter>)
(ai_free_units <encounter>)
(ai_place <encounter>)
(ai_kill <encounter>)
(ai_kill_silent <encounter>)
(ai_erase <encounter>)
(ai_erase_all)
(ai_select <encounter>)
(ai_deselect <encounter>)
(ai_set_blind <encounter> <bool>)
(ai_set_deaf <encounter> <bool>)
(ai_set_respawn <encounter> <bool>)
(ai_respawn)                                             — re-enable respawn
(ai_magically_see_encounter <encounter> <encounter>)
(ai_magically_see_players <encounter>)
(ai_magically_see_unit <encounter> <unit>)
(ai_attach <encounter> <leader_encounter>)
(ai_attach_free <encounter> <leader_encounter>)
(ai_detach <encounter>)
(ai_retreat <encounter> [<bool>])
(ai_maneuver <encounter>)
(ai_maneuver_enable <encounter> <bool>)
(ai_migrate <encounter> <destination_encounter>)
(ai_migrate_and_speak <encounter> <destination_encounter>)
(ai_migrate_by_unit <encounter> <destination>)
(ai_allegiance <team1> <team2> <type>)
(ai_allegiance_broken <team1> <team2>)
(ai_allegiance_remove <team1> <team2>)
(ai_living_count <encounter>)                            → short
(ai_living_fraction <encounter>)                         → real
(ai_nonswarm_count <encounter>)                          → short
(ai_actors <encounter>)                                  → list
(ai_go_to_vehicle <actor> <vehicle>)
(ai_go_to_vehicle_override <actor> <vehicle>)
(ai_going_to_vehicle <actor>)                            → bool
(ai_exit_vehicle <actor>)
(ai_braindead <actor> <bool>)
(ai_braindead_by_unit <unit> <bool>)
(ai_disregard <encounter> <bool>)
(ai_prefer_target <actor> <target>)
(ai_renew <encounter>)
(ai_set_current_state <encounter> <state>)
(ai_set_return_state <encounter> <state>)
(ai_force_active <encounter> <bool>)
(ai_force_active_by_unit <unit> <bool>)
(ai_command_list <encounter> <script>)
(ai_command_list_by_unit <unit> <script>)
(ai_command_list_advance <encounter>)
(ai_command_list_advance_by_unit <encounter> <unit>)
(ai_command_list_status <encounter>)                    → short
(ai_is_attacking <actor>)                                → bool
(ai_follow_target_disable <actor>)
(ai_follow_target_players <actor>)
(ai_follow_target_unit <actor> <unit>)
(ai_follow_distance <actor> <real>)
(ai_conversation <conversation> <actor1> [<actor2> ...])
(ai_conversation_stop <conversation>)
(ai_conversation_advance <conversation>)
(ai_conversation_line <conversation>)                    → short
(ai_conversation_status <conversation>)                  → short
(ai_berserk <actor> <bool>)
(ai_set_team <actor> <team>)
(ai_allow_charge <actor> <bool>)
(ai_allow_dormant <actor> <bool>)
(ai_link_activation <encounter> <link_encounter>)
(ai_reconnect <actor> <new_encounter>)
(ai_playfight <actor> <bool>)
(ai_automatic_migration_target <encounter> <target>)
(ai_look_at_object <actor> <object> <bool>)
(ai_attack <actor> [<target>])
(ai_defend <actor> [<target>])
```

#### AI Debug (indices 284–291)

```
(ai_dialogue_triggers <encounter> <bool>)
(ai_grenades <encounter> <type>)
(ai_lines <encounter> <bool>)
(ai_debug_sound_point_set <point> <radius>)
(ai_debug_teleport_to <encounter> <flag>)
(ai_debug_speak <string>)
(ai_debug_speak_list <list>)
(ai_debug_vocalize <encounter> <vocalization>)
```

#### Cinematic / Camera (indices 245–253, 294–302)

```
(cinematic_start)
(cinematic_stop)
(cinematic_skip_start)
(cinematic_skip_stop)
(cinematic_show_letterbox <bool>)
(cinematic_set_title <title>)
(cinematic_set_title_delayed <title> <real_seconds>)
(cinematic_suppress_bsp_object_creation <bool>)
(camera_control <bool>)
(camera_set <camera_point> <ticks>)
(camera_set_relative <camera_point> <ticks> <relative_to>)
(camera_set_animation <camera_point> <anim>)
(camera_set_animation_relative <...>)
(camera_set_first_person <unit>)
(camera_set_follow_style <follow>)
(camera_set_follow_offset <x> <y> <z>)
(camera_set_focus <object>)
(camera_set_focus_relative <camera_point> <offset>)
(camera_set_rotation <yaw> <pitch> <roll>)
(camera_set_position <x> <y> <z>)
(camera_set_fov <fov>)
(camera_field_of_view)                                    → real
(attract_mode_start)
(recording_play <unit> <recording>)
(recording_play_and_delete <unit> <recording>)
(recording_play_and_hover <vehicle> <recording>)
(recording_kill <unit>)
(recording_time <unit>)                                   → real
```

#### Game State (indices 303–322)

```
(game_won)
(game_lost)
(game_safe_to_save)                    → bool
(game_all_quiet)                        → bool
(game_safe_to_speak)                    → bool
(game_is_cooperative)                   → bool
(game_save)
(game_save_cancel)
(game_save_no_timeout)
(game_save_totally_unsafe)
(game_saving)                            → bool
(game_revert)
(game_reverted)                          → bool
(game_skip_ticks <short>)
(core_save)
(core_save_name <path>)
(core_load)
(core_load_at_startup)
(core_load_name <path>)
(core_load_name_at_startup <path>)
(delete_save_game_files)
```

#### Sound (indices 323–335)

```
(sound_impulse_start <sound> <source> <scale>)
(sound_impulse_time <sound>)             → real
(sound_impulse_stop <sound>)
(sound_looping_start <sound> <source> <scale>)
(sound_looping_stop <sound>)
(sound_looping_set_scale <sound> <scale>)
(sound_looping_set_alternate <sound> <bool>)
(debug_sounds_enable <substring> <bool>)
(debug_sounds_distances <substring> <min> <max>)
(debug_sounds_wet <substring> <level>)
(sound_enable <bool>)
(sound_class_set_gain <class> <gain> <ticks>)
(vehicle_hover <vehicle> <bool>)
```

#### Player / HUD (indices 337–395)

```
(players)                                → list
(players_unzoom_all)
(player_enable_input <bool>)
(player_camera_control <bool>)
(player_action_test_reset)
(player_action_test_jump)                    → bool
(player_action_test_primary_trigger)         → bool
(player_action_test_grenade_trigger)         → bool
(player_action_test_zoom)                    → bool
(player_action_test_action)                  → bool
(player_action_test_accept)                  → bool
(player_action_test_back)                    → bool
(player_action_test_look_relative_up)        → bool
(player_action_test_look_relative_down)      → bool
(player_action_test_look_relative_left)      → bool
(player_action_test_look_relative_right)     → bool
(player_action_test_look_relative_all)       → bool
(player_action_test_move_relative_all)       → bool
(debug_teleport_player <x> <y>)
(player_add_equipment <profile> <bool>)
(show_hud <bool>)
(show_hud_help_text <bool>)
(enable_hud_help_flash <bool>)
(hud_help_flash_restart)
(activate_nav_point_flag <type> <unit> <flag> <offset>)
(activate_nav_point_object <type> <unit> <object> <offset>)
(activate_team_nav_point_flag <type> <team> <flag> <offset>)
(activate_team_nav_point_object <type> <team> <object> <offset>)
(deactivate_nav_point_flag <unit> <flag>)
(deactivate_nav_point_object <unit> <object>)
(deactivate_team_nav_point_flag <team> <flag>)
(deactivate_team_nav_point_object <team> <object>)
(hud_show_health <bool>)
(hud_blink_health <bool>)
(hud_show_shield <bool>)
(hud_blink_shield <bool>)
(hud_show_motion_sensor <bool>)
(hud_blink_motion_sensor <bool>)
(hud_show_crosshair <bool>)
(hud_clear_messages)
(hud_set_help_text <string>)
(hud_set_objective_text <string>)
(hud_set_timer_time <minutes> <seconds>)
(hud_set_timer_warning_time <minutes> <seconds>)
(hud_set_timer_position <x> <y> <corner>)
(show_hud_timer <bool>)
(pause_hud_timer <bool>)
(hud_get_timer_ticks)                     → short
(time_code_show <bool>)
(time_code_start <bool>)
(time_code_reset)
```

#### Screen Effects (indices 399–406)

```
(script_screen_effect_set_value <index> <value>)
(cinematic_screen_effect_start <bool>)
(cinematic_screen_effect_set_convolution <a> <b> <c> <d> <e>)
(cinematic_screen_effect_set_filter <a> <b> <c> <d> <e> <f>)
(cinematic_screen_effect_set_filter_desaturation_tint <r> <g> <b>)
(cinematic_screen_effect_set_video <noise> <overbright>)
(cinematic_screen_effect_stop)
(cinematic_set_near_clip_distance <real>)
```

#### Player Effects (indices 371–375)

```
(player_effect_set_max_translation <x> <y> <z>)
(player_effect_set_max_rotation <yaw> <pitch> <roll>)
(player_effect_set_max_rumble <left> <right>)
(player_effect_start <intensity> <attack>)
(player_effect_stop <decay>)
```

#### Miscellaneous / Debug (indices 56–65, 368–370, 396–417)

```
(script_recompile)
(script_doc)                              — writes hs_doc.txt
(help <function_name>)
(random_range <min> <max>)                — short random
(real_random_range <min> <max>)           — real random
(game_time)                                → ticks
(print <string>)
(cls)
(error_overflow_suppression <bool>)
(numeric_countdown_timer_set <ms> <auto_start>)
(numeric_countdown_timer_get <index>)     → short
(numeric_countdown_timer_stop)
(numeric_countdown_timer_restart)
(breakable_surfaces_enable <bool>)
(structure_lens_flares_place)
(rasterizer_decals_flush)
(rasterizer_fps_accumulate)
(rasterizer_model_ambient_reflection_tint <r> <g> <b> <a>)
(rasterizer_lights_reset_for_new_map)
(enumerate_memory_units)
(fast_setup_network_server)               — "for zach's multiplayer testing"
(profile_unlock_solo_levels)
(player0_look_invert_pitch <bool>)
(player0_look_pitch_is_inverted)          → bool
(player0_joystick_set_is_normal)          → bool
(ui_widget_show_path <path>)
(display_scenario_help <help_id>)
(network_game_start_now)                  — "another one for zach"
(xbox_set_machine_name <name>)
(game_difficulty_get)                     → short
(game_difficulty_get_real)                → real
```

### Full Data Export

The complete 418-entry table (index, name, help text, syntax, flags, callback
addresses, argument count) is available as JSON at:
- `/tmp/haloscript_table.json`

---

## 8. Source Files

| File | Purpose |
|------|---------|
| `src/halo/main/debug_keys.c` | Debug keyboard dispatcher and definition table |
| `src/halo/main/console.c` | Debug console startup, update, and keyboard handling |
| `src/halo/game/cheats.c` | `d:\cheats.txt` loading and gamepad-button execution |
| `src/halo/hs/hs.c` | HaloScript console evaluation and function table lookup |
| `src/halo/input/input_xbox.c` | Xbox input, keyboard device polling, vkey→keycode mapping |
