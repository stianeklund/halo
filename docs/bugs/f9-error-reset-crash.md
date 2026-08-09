# F9 (Error Log Reset) Crash

**Status:** Root-caused and fixed — 2026-08-09
**Severity:** Low — debug-only key, but the two defects it exposed are a general class
**Regression:** Confirmed — original cachebeta.xbe handles F9 fine
**Discovered:** 2026-05-16
**See also:** [Object data corruption bug](nan-object-lighting-crash.md)

## Symptoms

Pressing F9 on the debug keyboard during gameplay froze the game. With XBDM
attached the freeze was silent: no assert in debug.txt, all threads alive, the
box still answering RDCP. `isstopped thread=N` was the only thing that named it.

## Why it looked like a deadlock

XBDM takes first-chance exceptions before the title. It halted the faulting
thread at the faulting instruction, so the title's own handler
(`_write_to_error_file`, 0x8f230) never ran — no `EXCEPTION halt` line, no
flush. The other three threads sat in `KeWait` at `0x8001f48c` and the CPU idled
at `0x8001e023`, which reads exactly like a soft-deadlock.

**Diagnostic:** on any silent freeze with XBDM attached, run `threads` then
`isstopped thread=N` for each. It reports the exception code and fault address
that the log never receives.

## Why F9 specifically

F9 (debug key entry [8], key_code 0x09) calls `FUN_000ffe90` → `FUN_0008f630`
(errors.obj, unported). Among the error-log globals it resets, it sets
`profile_global_enable` (`0x449ef1`) to 1.

Every profiling call site is guarded:

```c
if (profile_global_enable && *(char *)(section + 8) /* section->active */)
    profile_enter_private(section);
```

Until F9 sets that flag, none of the profiling calls execute. Both defects
below sat dormant behind that gate — which is why F9 was the only trigger, and
why VC71 match said nothing about either.

The earlier hypothesis on this page — that our ported `error()` and the
unported reset disagreed about the errors.obj global layout — was wrong. The
reset function is faithful; the bugs were in the profiling path it enables.

## Defect 1 — `find_profile_section` declared with no parameters

`kb.json` had `void find_profile_section(void);` for 0x8f8e0. The function
takes a `section` pointer:

```
0008f8e5  mov esi, dword ptr [ebp + 8]     ; section
0008f8ea  cmp esi, ebx                     ; assert(section)          profile.c:559
0008f90e  cmp byte ptr [esi + 8], bl       ; assert(section->active)  profile.c:560
```

and the original callers pass it:

```
0008fa47  mov esi, dword ptr [ebp + 8]     ; profile_enter_private(section)
0008fa4a  push esi
0008fa4b  call 0x8f8e0
0008fa50  add esp, 4
```

Our `profile_enter_private` / `profile_exit_private` called it as
`find_profile_section()`, pushing nothing, so the callee read stale stack as
`section`. Live fault: `0xc0000005 address=0x0008f90e read=0x00000014` with
`section = 0xe57e0000`.

Fixed by correcting the kb.json decl and both call sites in
`src/halo/cseries/profile.c`.

## Defect 2 — `objects_update` passed the section's name, not the section

Exposed only after defect 1 was fixed. `0x324638` **is** a `profile_section`
struct, not a pointer to one:

| Offset | Field | Value at 0x324638 |
|--------|-------|-------------------|
| +0x00 | `const char *name` | `0x29b880` = `"objects_update"` |
| +0x04 | `int index` | `-1` when unregistered |
| +0x08 | `char active` | the byte the guard reads as `0x324640` |
| +0x0a | `int16 stack_depth` | `-1` when not entered |

The original pushes the struct address:

```
00145199  mov al, byte ptr [0x324640]     ; section->active
001451a2  push 0x324638                   ; section
001451a7  call 0x8fa40                    ; profile_enter_private
```

Our `objects.c` had `profile_enter_private(*(void *volatile *)0x324638)` —
dereferencing the struct and passing its `name` pointer. `0x8f8e0` then read
`"objects_update"`'s bytes as `index`, which is neither `-1` nor a valid slot,
so it hit its third assert:

```
EXCEPTION halt in c:\halo\SOURCE\cseries\profile.c,#566:
don't call profile_enter_private(), call profile_enter()
```

That message is misleading — the guard at profile.c:566 fires for any section
whose `index` is neither `-1` nor a live entry in `profile_sections`
(`0x3361b4[]`, count at `0x3361b0`), not only for a genuine direct call.

Fixed both call sites in `src/halo/objects/objects.c`. The other profile call
sites were audited mechanically (argument address + 8 must equal the guard's
address) and are correct.

## What 0x8f8e0 actually does

Despite the kb.json name `find_profile_section`, it registers or validates:

- `index == -1` → assign `index = profile_globals.count++`, store
  `profile_sections[index] = section`, zero the counters, set
  `stack_depth = -1`. Asserts at profile.c:570 if count would reach 0x100.
- otherwise → require `0 <= index < count` and
  `profile_sections[index] == section`, else assert profile.c:566.

Renaming it is left to the naming lane; the behaviour is recorded here.

## Detector

`tools/audit/check_lift_hazards.py::check_noparam_decl_args` now fails the
build for defect 1's class: any unported kb.json entry declared with no
parameters that our C calls as `f()`, where the original opens with a standard
`PUSH EBP; MOV EBP,ESP` frame and reads `[EBP+8]` or higher. It found 13 more
instances the same day; all were fixed.

Defect 2's class (passing `*(T *)ADDR` where the original pushes `ADDR`) is not
covered generically — the profile-specific audit above was a one-off script,
and a general rule would need to know which globals are structs.

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
| F9  | 0x09 | 0x0ffe90   | Reset error log — enables profiling |

Modifier keys: 0x69 = left Ctrl, 0x6a = (second modifier).
Modifier combos: 0=none, 1=Ctrl only, 2=mod2 only, 3=both.
