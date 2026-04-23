---
name: halo-page-fault
description: Investigate page faults during lift/reimplementation, typically caused by ABI/signature mismatches in kb.json
---

# Halo Page Fault Investigation

Use this skill when investigating page faults that occur during lift/reimplementation
work. Page faults in this context almost always indicate a kb.json signature
mismatch between what callers expect and what the ported implementation provides.

## When to use this skill

- XBDM/xemu reports a page fault (e.g., `MM: page fault touching E6D40564`)
- EIP points outside valid game code
- Crash occurs after recently porting a function or modifying kb.json
- The fault address looks like garbage/uninitialized data

## The root cause pattern

The most common cause: a function was changed in kb.json from a simple declaration
to a ported function with a **different signature**, especially:

- Added `@<reg>` register arguments that callers don't know about
- Changed return type width (e.g., `void` → `uint`)
- Changed parameter count or types
- Missing `HDATA` indirection adjustment

Callers compiled against the old signature pass garbage in registers/stack slots
that the new implementation interprets as valid pointers.

## Investigation workflow

### 1. Capture the crash context

From XBDM/xemu output, extract:
- Fault address (e.g., `touching E6D40564`)
- Trap frame location (e.g., `trap frame D009B378`)
- EIP at crash (e.g., `eip 000923F6`)

### 2. Identify the crashing instruction

Use Ghidra MCP to inspect the EIP:
```
ghidra_get_function_by_address(address="0x000923F6")
ghidra_disassemble_bytes(start_address="0x000923F6", length=16)
```

**Key question**: Is the EIP inside a lifted/port function or still in original XBE code?

### 3. Trace back to find the caller

If EIP is in original XBE code, the fault is likely due to calling a ported
function with wrong arguments:

```
# Get the call stack from trap frame or walk backwards
ghidra_get_xrefs_to(address="0x<faulting_function>")
```

### 4. Check kb.json for signature drift

Inspect the suspect function in kb.json:

```bash
# Find the function at or near the crash address
jq '.symbols[] | select(.address | startswith("000923"))' kb.json

# Check if it has @<reg> annotations
jq '.symbols[] | select(.name == "<function_name>") | {name, address, type, calling_convention}' kb.json
```

**Red flags to look for:**
- Recently added `@<eax>`, `@<ecx>`, `@<edx>` annotations
- Changed from `type: "function_declaration"` to `type: "function_implementation"`
- Calling convention mismatch with actual usage sites
- Return type that doesn't match how callers use the result

### 5. Cross-check against callers

Find all callers and verify they match the kb.json signature:

```
ghidra_get_function_callers(name="<function_name>", limit=20)
```

For each caller, check:
- Do they load values into the expected register args before CALL?
- Do they use the return value correctly?
- Does the stack cleanup match the parameter count?

### 6. Common fix patterns

**Pattern A: Wrong @<reg> annotation**
```json
// Wrong - forces register arg that callers don't provide
{
  "name": "FUN_0009a5a0",
  "address": "0009a5a0",
  "type": "function_implementation",
  "parameters": [{"name": "pData", "type": "void*", "@<eax>": true}]
}

// Correct - match what callers actually do
{
  "name": "FUN_0009a5a0",
  "address": "0009a5a0",
  "type": "function_declaration"
  // Let the original XBE implementation handle it
}
```

**Pattern B: Missing HDATA indirection**
- Check if the function returns a handle that needs `HDATA` dereferencing
- Verify callers expect the right indirection level

**Pattern C: Wrong return type width**
- 32-bit vs 64-bit return values affect register usage
- Check `eax` vs `eax:edx` patterns in disassembly

## Evidence policy

Follow `halo-xbox-re` doctrine:
- **Confirmed**: Disassembly showing caller register/stack setup matches kb.json
- **Inferred**: Signature mismatch is the likely cause based on timeline
- **Uncertain**: Whether the fix is annotation removal vs caller updates

## Validation

After fixing kb.json:
1. Rebuild the project
2. Re-test on Xbox/xemu
3. Verify the page fault no longer occurs at the same location

## Example session

**Symptom:**
```
MM: page fault touching E6D40564, trap frame D009B378, eip 000923F6
```

**Investigation:**
- EIP 000923F6 is inside FUN_000923f0 (ported function)
- FUN_000923f0 calls FUN_0009a5a0
- kb.json shows FUN_0009a5a0 recently changed to have `@<eax>` parameter
- Callers of FUN_0009a5a0 don't load anything into eax before call

**Root cause:** FUN_0009a5a0 signature mismatch - callers expect cdecl,
kb.json declares custom register calling convention.

**Fix:** Revert FUN_0009a5a0 to function_declaration or fix all callers
and verify with disassembly.

## Output report

When reporting findings:
- Fault address and EIP
- Function at fault location
- kb.json signature vs actual caller behavior
- Confirmed/Inferred/Uncertain classification
- Fix applied
- Validation result
