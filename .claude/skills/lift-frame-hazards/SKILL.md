---
name: lift-frame-hazards
description: Buffer sizing from _chkstk frames, stack aliasing detection, and contiguous-buffer rules. Invoke when sizing a local buffer, seeing _chkstk in a frame, or passing &local to a callee that indexes param[N].
---

# Lift Frame and Buffer Hazards

**Invoke this skill when:**
- The decompile mentions `_chkstk` and you need to size a buffer from the frame
- You see `&local_XX` passed to a callee that indexes `param[N]` where N > 0
- A `static` buffer exists with a comment about avoiding `_chkstk`
- Callee performs a `csmemset` or `memcpy` and you aren't sure how large its buffer is

Source: `docs/lift-learnings.md` §2 + §20.

---

## Rule 1 — Derive buffer size from the _chkstk frame, not Ghidra's guess (§2)

When a function's first instruction is `_chkstk(EAX = N)`:

```
true_frame_bytes = N
subtract known named locals (iterators, scalars, small structs)
remainder = the large buffer size
```

**Example:** `_chkstk 0x1028` = `0x1010` buffer + `0x10` iterator + `0x8` bsp_data
→ `int buf[1028]` (not Ghidra's guess of `[1024]`).

**Why it matters:** Under-sizing the buffer means a callee writing near its tail
corrupts your stack. Ghidra guesses conservatively and is often wrong by 4–16 entries.

Always verify the buffer's last-read index against `EBP-N` in disassembly:
```bash
# In Ghidra: disassemble_function at address, then scan for [EBP-N] accesses
# near the buffer region to find the true high-water mark
```

---

## Rule 2 — &local_XX passed to a callee = must be contiguous (§2)

When any `&local_XX` is passed to a callee, decompile that callee and check if
it reads `param[N]` where N > 0, or does `memset(param, 0, K)`.

If yes: the caller's separate `local_XX` variables **must** be replaced by a
single array of at least `K` bytes / N+1 elements.

```bash
# After lifting, grep for address-of-local in callsites:
grep -n '&local_' src/<file>.c
# For each hit, check the callee:
rtk python3 -c "
import urllib.request; from urllib.parse import urlencode
url='http://localhost:8089/decompile_function?' + urlencode({'address':'0xADDR'})
print(urllib.request.urlopen(url, timeout=30).read().decode())
" | grep -E 'param_1\[[0-9]\]|memset|memcpy'
```

**Silent-failure variant:** The aliased buffer can be as small as 3 floats.
If you see three separate `float local_40 = 0; float local_3c = 0; float local_38 = 0;`
and any of them have their address taken, they must be `float local_40[3]`.
A lift-added `= 0` initializer on address-taken locals is the tell — the lifter
sensed they weren't fully written but missed the contiguity requirement.

---

## Rule 3 — static buffers are a _chkstk workaround (§20)

`_chkstk` is now stubbed in `xbox_crt.c` (`_chkstk` → no-op). The linker error
is gone. Any `static` buffer added to avoid it should be converted back:

```bash
grep -rn 'static.*avoid.*_chkstk\|static.*_chkstk\|chkstk linker' src/
```

**Before converting:** rebuild and verify the VC71 score improves. Typical gain
is 5–15pp because stack buffers use EBP-relative addressing (matches original)
while static buffers use absolute symbol addresses.

---

## Rule 4 — Re-derive every instruction when re-lifting a deactivated function (§2)

When a function was deactivated (ported=false) and you're fixing it:
- The existing dormant C is a **draft**, not a baseline.
- The flagged call is rarely the only defect.
- Re-read every `MOV [EBP-N], EAX` in the original disassembly around the changed
  region — stores the decompiler dropped often hide next to the one you found.
