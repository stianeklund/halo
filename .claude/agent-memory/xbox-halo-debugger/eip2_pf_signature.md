---
name: EIP=2 page fault signature
description: How to interpret a "page fault touching 0x00000002" with eip=2 on the Xbox kernel
type: reference
---

When xemu reports "MM: page fault touching 00000002" with eip=2 in a trap frame:

- The CPU literally fetched an instruction at virtual address 0x00000002.
- Xbox runs the entire game in CPL=0 (CS=0x0008 with DPL=0). There is no user mode. So a near-NULL EIP from "user code" presents the same way.
- Common causes (in order of likelihood):
  1. `CALL reg` where `reg` was loaded from a struct slot containing 2 (bad function pointer).
  2. `RET` where the saved return address on the stack was overwritten with 2 (stack corruption / wrong calling convention / arg-count mismatch eating the return address).
  3. `JMP [reg+offset]` indirect jump through a table where the slot contained 2.
- Trap frame in the kernel sits on the kernel stack (above 0xd0000000 in xemu). The user-mode EIP is in KTRAP_FRAME's `Eip` slot. Standard NT layout has `Eip` at offset +0x68, but on Xbox the layout is slightly different — read the raw frame as 32-bit dwords and look for a sane code address near the end (low ~0x6 MB for original XBE; 0x643000+ for our patched section in `.text.patch`).
- The `CR2` value in `info registers` (after the fault) confirms the faulting virtual address. If CR2 == EIP from the trap frame, this is an instruction-fetch fault (not a data-access fault).
- Stack walking: scan kernel stack for known code address ranges. Patched-XBE C section is `0x643000..0x655000`. Original game code is `0x12000..0x1e69e0`. Anything else is data, strings, or kernel pointers.

Related Cxbx/NT trap frame fields to recognize:
- `0xbadb0d00` near +0x08 is the Xbox kernel's "DbgArgMark" sentinel. Confirms you're looking at a real KTRAP_FRAME.
- `DbgEbp` at +0x00 chains into the kernel stack EBP. Useful for walking back to find user-mode return addresses on the stack just below the trap frame.
- `0x0064a836`-style values inside the trap frame are usually saved return addresses captured during fault handling, NOT the user-mode EIP itself.
