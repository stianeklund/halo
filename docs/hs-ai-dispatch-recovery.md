# HaloScript AI Dispatch Evaluator Recovery Report
**Target:** Halo: Combat Evolved (Original Xbox Debug Build 01.10.12.2276, Oct 12 2001)  
**Binary:** `cachebeta.xbe` (MD5 `c7869590a1c64ad034e49a5ee0c02465`)  
**Branch:** `push09132026`  
**Date:** September 13, 2026  
**Author:** Nick Arcade / Antigravity Pair  

---

## Executive Summary

This document details the recovery, technical analysis, and semantic labeling of **39 consecutive AI and conversation HaloScript dispatch evaluators** in [`src/halo/hs/hs.c`](../src/halo/hs/hs.c) and [`kb.json`](../kb.json).

Every single recovered name is backed by **Tier 1 direct target binary evidence** extracted from the static `hs_function_table` at virtual address `0x002f1588` (file offset `0x002eb0c8`, 418 entries) in the authentic Xbox debug binary `cachebeta.xbe`.

---

## 1. How the Names Were Recovered (Binary Evidence)

In Halo CE Xbox debug build 2276 (`cachebeta.xbe`), all HaloScript functions are registered in a static descriptor table located at VA `0x002f1588`. Each table entry has the following structure:

```c
struct hs_function_definition {
    int16_t return_type;
    int16_t flags;
    const char *name;             // Pointer to script command string in .rdata
    void (*exec_proc)(int16_t fn_idx, int thread_datum, char init); // Dispatch evaluator
    const char *help_string;      // Pointer to developer help string in .rdata
    int16_t param_count;
    int16_t param_types[8];
};
```

Each entry's `exec_proc` function pointer points directly to the compiled dispatch handler in `hs.obj`.
Because Bungie compiled both the command names (`name`) and the documentation strings (`help_string`) into `.rdata`, resolving each entry's `exec_proc` gives **100% authentic, authoritatively named Bungie symbols**.

---

## 2. Complete Catalogue of 39 Recovered Functions

### Batch 1: AI State & Vehicle Encounter Handlers (Commit `8bd97397`)
| VA | Table Index | Script Name | Recovered C Identifier | Bungie Help / Documentation |
| :--- | :---: | :--- | :--- | :--- |
| `0xc0bf0` | 215 | `ai_set_return_state` | `hs_evaluate_ai_set_return_state` | sets the state that a group of actors will return to when they have nothing to do |
| `0xc0c30` | 216 | `ai_set_current_state` | `hs_evaluate_ai_set_current_state` | sets the current state of a group of actors. WARNING: may have unpredictable results on actors that are in combat |
| `0xc0c70` | 217 | `ai_playfight` | `hs_evaluate_ai_playfight` | sets an encounter to be playfighting or not |
| `0xc0cb0` | 219 | `ai_reconnect` | `hs_evaluate_ai_reconnect` | reconnects all AI information to the current structure bsp (use this after you create encounters or command lists in sapien, or |
| `0xc0cd0` | 220 | `ai_vehicle_encounter` | `hs_evaluate_ai_vehicle_encounter` | sets a vehicle to 'belong' to a particular encounter/squad. any actors who get into the vehicle will be placed in this squad. NB |
| `0xc0d10` | 221 | `ai_vehicle_enterable_distance` | `hs_evaluate_ai_vehicle_enterable_distance` | sets a vehicle as being impulsively enterable for actors within a certain distance |
| `0xc0d50` | 222 | `ai_vehicle_enterable_team` | `hs_evaluate_ai_vehicle_enterable_team` | sets a vehicle as being impulsively enterable for actors on a certain team |
| `0xc0d90` | 223 | `ai_vehicle_enterable_actor_type` | `hs_evaluate_ai_vehicle_enterable_actor_type` | sets a vehicle as being impulsively enterable for actors of a certain type (grunt, elite, marine etc) |
| `0xc0dd0` | 224 | `ai_vehicle_enterable_actors` | `hs_evaluate_ai_vehicle_enterable_actors` | sets a vehicle as being impulsively enterable for a certain encounter/squad of actors |
| `0xc0e10` | 225 | `ai_vehicle_enterable_disable` | `hs_evaluate_ai_vehicle_enterable_disable` | disables actors from impulsively getting into a vehicle (this is the default state for newly placed vehicles) |
| `0xc0e50` | 226 | `ai_look_at_object` | `hs_evaluate_ai_look_at_object` | tells an actor to look at an object until further notice |
| `0xc0e90` | 227 | `ai_stop_looking` | `hs_evaluate_ai_stop_looking` | tells an actor to stop looking at whatever it's looking at |
| `0xc0ed0` | 228 | `ai_automatic_migration_target` | `hs_evaluate_ai_automatic_migration_target` | enables or disables a squad as being an automatic migration target |
| `0xc0f10` | 229 | `ai_follow_target_disable` | `hs_evaluate_ai_follow_target_disable` | turns off following for an encounter |

### Batch 2: AI Follow & Behavior Evaluators (Commit `bf8abaa0`)
| VA | Table Index | Script Name | Recovered C Identifier | Bungie Help / Documentation |
| :--- | :---: | :--- | :--- | :--- |
| `0xc0f50` | 230 | `ai_follow_target_players` | `hs_evaluate_ai_follow_target_players` | sets the follow target for an encounter to be the closest player |
| `0xc0f90` | 231 | `ai_follow_target_unit` | `hs_evaluate_ai_follow_target_unit` | sets the follow target for an encounter to be a specific unit |
| `0xc0fd0` | 232 | `ai_follow_target_ai` | `hs_evaluate_ai_follow_target_ai` | sets the follow target for an encounter to be a group of AI (encounter, squad or platoon) |
| `0xc1010` | 233 | `ai_follow_distance` | `hs_evaluate_ai_follow_distance` | sets the distance threshold which will cause squads to migrate when following someone |
| `0xc1050` | 235 | `ai_conversation_stop` | `hs_evaluate_ai_conversation_stop` | stops a conversation from playing or trying to play |
| `0xc1090` | 236 | `ai_conversation_advance` | `hs_evaluate_ai_conversation_advance` | tells a conversation that it may advance |
| `0xc10d0` | 239 | `ai_link_activation` | `hs_evaluate_ai_link_activation` | links the first encounter so that it will be made active whenever it detects that the second encounter is active |
| `0xc1110` | 240 | `ai_berserk` | `hs_evaluate_ai_berserk` | forces a group of actors to start or stop berserking |
| `0xc1150` | 241 | `ai_set_team` | `hs_evaluate_ai_set_team` | makes an encounter change to a new team |
| `0xc1190` | 242 | `ai_allow_charge` | `hs_evaluate_ai_allow_charge` | either enables or disables charging behavior for a group of actors |
| `0xc11d0` | 243 | `ai_allow_dormant` | `hs_evaluate_ai_allow_dormant` | either enables or disables automatic dormancy for a group of actors |

### Batch 3: AI Query, Status & Conversation Evaluators (Commit `e5b564de`)
| VA | Table Index | Script Name | Recovered C Identifier | Bungie Help / Documentation |
| :--- | :---: | :--- | :--- | :--- |
| `0xc1210` | 212 | `ai_is_attacking` | `hs_evaluate_ai_is_attacking` | returns whether a platoon is in the attacking mode (or if an encounter is specified, returns whether any platoon in that encount |
| `0xc1260` | 211 | `ai_command_list_status` | `hs_evaluate_ai_command_list_status` | gets the status of a number of units running command lists: 0 = none, 1 = finished command list, 2 = waiting for stimulus, 3 = r |
| `0xc12b0` | 195 | `ai_going_to_vehicle` | `hs_evaluate_ai_going_to_vehicle` | return the number of actors that are still trying to get into the specified vehicle |
| `0xc1300` | 187 | `ai_living_count` | `hs_evaluate_ai_living_count` | return the number of living actors in the specified encounter and/or squad. |
| `0xc1350` | 188 | `ai_living_fraction` | `hs_evaluate_ai_living_fraction` | return the fraction [0-1] of living actors in the specified encounter and/or squad. |
| `0xc1390` | 189 | `ai_strength` | `hs_evaluate_ai_strength` | return the current strength (average body vitality from 0-1) of the specified encounter and/or squad. |
| `0xc13d0` | 190 | `ai_swarm_count` | `hs_evaluate_ai_swarm_count` | return the number of swarm actors in the specified encounter and/or squad. |
| `0xc1420` | 191 | `ai_nonswarm_count` | `hs_evaluate_ai_nonswarm_count` | return the number of non-swarm actors in the specified encounter and/or squad. |
| `0xc1470` | 192 | `ai_actors` | `hs_evaluate_ai_actors` | converts an ai reference to an object list. |
| `0xc14b0` | 218 | `ai_status` | `hs_evaluate_ai_status` | returns the most severe combat status of a group of actors (0=inactive, 1=noncombat, 2=guarding, 3=search/suspicious, 4=definite |
| `0xc1500` | 234 | `ai_conversation` | `hs_evaluate_ai_conversation` | tries to add an entry to the list of conversations waiting to play. returns FALSE if the required units could not be found to pl |
| `0xc1550` | 237 | `ai_conversation_line` | `hs_evaluate_ai_conversation_line` | returns which line the conversation is currently playing, or 999 if the conversation is not currently playing |
| `0xc15a0` | 238 | `ai_conversation_status` | `hs_evaluate_ai_conversation_status` | returns the status of a conversation (0=none, 1=trying to begin, 2=waiting for guys to get in position, 3=playing, 4=waiting to |
| `0xc15f0` | 244 | `ai_allegiance_broken` | `hs_evaluate_ai_allegiance_broken` | returns whether two teams have an allegiance that is currently broken by traitorous behavior |

---

## 3. What the Functions Do (Technical Analysis)

All 39 functions implement the standard HaloScript evaluator ABI:
```c
void hs_evaluate_<name>(int16_t function_index, int thread_datum, char init);
```

### Execution Flow:
1. **Argument Evaluation:** Each evaluator calls `hs_macro_function_evaluate(function_index, thread_datum, init)`. If arguments are still pending or evaluation fails, it returns `NULL` and the handler yields.
2. **Parameter Extraction:** When evaluation succeeds, a typed memory block is returned:
   - **Encounter / Object handles:** 32-bit integer at `result[0]`.
   - **State / Team codes:** 16-bit word (`result + 1` or `((short*)result)[2]`). Zero-extended (`xor edx,edx; mov dx,[eax+4]`) or sign-extended (`movsx`) per original disassembly.
   - **Booleans / Flags:** 8-bit byte (`*(char*)(result + 1)`).
   - **Distances / Thresholds:** 32-bit float passed via ST(0) or `FLD+FSTP`.
3. **Native Engine Dispatch:** Passes the extracted parameters to internal AI/game subsystems:
   - `ai_set_return_state` $\rightarrow$ `FUN_000579d0(encounter_handle, return_state)`
   - `ai_set_current_state` $\rightarrow$ `FUN_00057aa0(encounter_handle, state)`
   - `ai_playfight` $\rightarrow$ `FUN_00057c70(encounter_handle, value)`
   - `ai_reconnect` $\rightarrow$ `FUN_00057c60()` (reconnects AI graph to structure BSP)
   - `ai_vehicle_encounter` $\rightarrow$ `FUN_00057d00(handle, value)`
   - `ai_vehicle_enterable_*` $\rightarrow$ configuration routines for impulsive vehicle boarding
   - `ai_look_at_object` / `ai_stop_looking` $\rightarrow$ AI gaze controller (`FUN_000581b0` / `FUN_00058220`)
   - `ai_follow_target_*` $\rightarrow$ squad leader tracking routines (`FUN_00058390`, `FUN_00058410`, `FUN_000584a0`)
   - `ai_conversation_*` $\rightarrow$ scripted dialog trigger and status handlers (`FUN_000585d0`, `FUN_00058700`, `FUN_00058710`)
   - `ai_is_attacking` / `ai_status` $\rightarrow$ combat status and behavior queries
   - `ai_living_count` / `ai_living_fraction` / `ai_strength` $\rightarrow$ census and vitality queries
4. **Thread Return:** Commits the result back to the scripting thread via `hs_return(thread_datum, value)`.

---

## 4. Adherence to Repository Rules & Maintainer Guidelines

### A. Authentic Evidence Standard (AGENTS.md & Maintainer Review PR #2)
- **Strict Evidence Gate:** Only authentic Bungie names proven by direct binary evidence (the static descriptor table in `cachebeta.xbe`) are adopted as C identifiers. Speculative or descriptive names without binary proof remain `FUN_...` per Stian Eklund's guidance in PR #2.
- **Zero Invention:** No names, offsets, or types were guessed.

### B. C89 Toolchain Invariants
- In accordance with VC71 C89 requirements, all local variables are declared at the top of their block scope before any statements.

### C. ABI & Calling Conventions
- Register ABI extraction (`extract_reg_args.py --check`) verified: `886 OK, 0 drift`.
- No inline assembly is used in lifted C code (`AGENTS.md` forbids inline asm under Clang due to optimizer clobber bugs). Thunks are managed via `kb.json`.

### D. Minimal Diffs & No Formatting Churn
- Edits to `kb.json` and `src/halo/hs/hs.c` are strictly scoped to the 39 renamed function declarations and definitions. No unrelated formatting or indentation changes were introduced.
