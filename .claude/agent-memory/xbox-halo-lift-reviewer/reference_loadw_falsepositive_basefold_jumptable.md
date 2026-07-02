---
name: loadw-falsepositive-basefold-jumptable
description: Two LOADW-WARN false-positive classes in vc71 objdiff — base-register pointer-fold (disp differs by N because one side did ADD reg,N) and switch jump-table byte read — neither is a field-width bug
metadata:
  type: reference
---

`vc71_verify --loadw-only` compares the *displacement literal* of narrow (movzbl/movswl) loads
between our obj and the delinked ref. Two recurring FALSE-POSITIVE classes, both non-blocking once
the effective address is reconciled against the pristine disasm:

**1. Base-register pointer-fold.** The original folds a `+N` pointer adjustment into the base register
(`ADD ESI,0x2`) and then reads `[ESI+disp]`; our clang build keeps the base un-adjusted and folds `+N`
into the displacement. Result: ref shows `movzbl 0x80(%esi)` and ours shows `movzbl 0x82(%esi)` — the
tool flags a disp mismatch, but the **effective address is identical** (buffer+0x82). Confirm by reading
the pristine disasm for the base-register write before the load. Seen on network_server_message_handler
0x130270/0x130580 error-code read: pristine `ADD ESI,0x2; MOVZX EAX,byte [ESI+0x80]` = buffer+0x82,
matching source `((unsigned char*)(msg+1))[0x80]`. Sign also matched (movzbl = unsigned).

**2. Switch jump-table index read.** A dense `switch` lowered by MSVC as a jump table emits
`MOVZX reg,byte [index+<table_addr>]` (e.g. `movzbl 0x130968(%eax)`) to read the per-case table entry,
then `JMP [reg*4+<table2>]`. clang may lower the same switch as an if-chain or a differently-based table,
so the ref's table-read has no counterpart in our obj. This is switch-lowering codegen, NOT a field read —
ignore it.

**Rule:** a LOADW-WARN only survives as a real concern if, after reconciling base-register value and
excluding jump-table reads, ours and ref read *different effective addresses* or *different signedness*
(movsbl vs movzbl) of an actual struct/message field. Otherwise it is display noise. See [[project_informative_sub90_needs_runtime_bar]] for how a fully-resolved LOADW still does not by itself clear an informative <90% VC71 lift.
