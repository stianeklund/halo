---
name: lift-arg-hazards
description: Argument hazard detection for lifted functions — cdecl mis-grouping (ADD ESP tell), NULL register args, and caller-site register order swaps. Invoke when an arg count looks wrong, a 0-arg getter is followed by an outer call, or a @<reg> function produces silent wrong output.
---

# Lift Argument Hazards

**Invoke this skill when:**
- A 0-arg getter is called with extra arguments in the decompile
- `ADD ESP, N` cleanup after a call doesn't match the callee's declared arg count
- A function with `@<reg>` args produces wrong output with no crash
- You are writing a call to a function with 2+ `@<reg>` annotations and aren't sure which value goes to which register
- You see `(float *)0`, `(void *)0`, or `(int *)0` passed as arguments

Source: `docs/lift-learnings.md` §3 + §7 + §10.

---

## Hazard 1 — Ghidra cdecl arg mis-grouping (§7)

MSVC pushes args right-to-left. For `outer(get_x(), a, b, c)` where `get_x` is
0-arg, the pushes for `c, b, a` happen **before** the `CALL get_x`. Ghidra sees
those pushes and attributes them to `get_x`, leaving `outer` with only the
getter's EAX result.

**Detection — the ADD ESP tell:**
```bash
# Disassemble the function. After the outer CALL, count the ADD ESP,N.
# N/4 = total args belonging to outer.
# If N/4 > what Ghidra gave outer, args were stolen by an inner getter.
```

**Example:**
```c
// Wrong (Ghidra's version):
get_x(a, b, c);            // 0-arg getter shown with 3 args
outer(eax);                // outer shown with only 1 arg

// Correct:
outer(get_x(), a, b, c);   // all 4 args belong to outer
```

**Prevention:** For any `outer(getter(), ...)` where kb.json declares `getter`
as 0-arg, STOP and read the `ADD ESP` cleanup after `outer`'s CALL site.

---

## Hazard 2 — NULL placeholder for @<reg> args (§3)

After lifting, grep the source for placeholder nulls:

```bash
grep -n '(float \*)0\|(void \*)0\|(int \*)0\|(char \*)0' src/<file>.c
```

For every hit, find the actual value from the original disassembly:
```bash
# Get the LEA/MOV instructions immediately before the CALL:
rtk python3 tools/audit/dump_caller_regsetup.py 0x<callee_addr>
```

**Never pass NULL for `@<reg>` params.** If you don't know the correct value,
leave the function unported until you determine it.

---

## Hazard 3 — Caller-site register order swap (§10)

A callee declared `@<ecx>, @<eax>, @<ebx>` has its thunk send arg1→ECX,
arg2→EAX, arg3→EBX. But a specific original caller may have loaded them in
a different order (e.g. EBX=arg2, EAX=arg3). When ported, the C call sends
the wrong values.

**Symptom:** No crash. A switch on the first register arg receives a datum
handle (~0x00010000) instead of a small integer → always hits default → feature
silently does nothing.

**Detection:**

```bash
rtk python3 tools/audit/dump_caller_regsetup.py <callee_name_or_addr>
```

This prints every original CALL site's last GPR loads and PUSH sequence.
Compare against the declared thunk convention. Any site that loads EBX before
EAX (or any non-canonical order) needs its C args swapped.

**Rule:** When a §10 swap is found on one caller, audit **all** callers of
that callee before closing. Different callers often use different preparation
sequences.

---

## Quick pre-flight grep (run after writing any lift)

```bash
# NULL register args:
grep -n '(float \*)0\|(void \*)0\|(int \*)0' src/<file>.c

# Float-as-pointer smuggling:
grep -n '(float)(int)' src/<file>.c

# XCALL to targets that might now be ported:
grep -oP 'XCALL\(0x\K[0-9a-f]+' src/<file>.c
```
