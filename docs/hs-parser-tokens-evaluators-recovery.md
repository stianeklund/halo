# HaloScript Evaluators Recovery: Batch 8 (Parser, Tokens, Expressions & Help Evaluators)

## Overview
- **Binary Target**: Halo CE Xbox debug build 2276 (`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).
- **Object**: `hs.obj` (addresses `0xc4010`–`0xc55d0`).
- **Scope**: 19 functions covering HaloScript token enumeration (`hs_tokens_*`), file reference sorting, signature formatting, doc/help generation, math random evaluators, AST condition clause rewriter (`hs_parse_cond_clauses`), and argument chain validator (`hs_syntax_get_arguments`).
- **Decompilation Oracle**: Verified against pristine assembly decompiled with `kuna` (`/data/data/com.termux/files/home/bin/kuna`).
- **Standards**: Strict C89 compliance, explicit terminal `return;` on all void functions, preservation of all `@<reg>` calling conventions, and zero register drift.

## Recovered Functions Summary

| Address | Function Name | HS Builtin Name | Table Index | Return Type | Description |
|---|---|---|---|---|---|
| `0xc4010` | `hs_tokens_compare` | N/A (helper) | N/A | int | Case-insensitive string comparator for `qsort` in `hs_tokens_enumerate`. |
| `0xc4030` | `hs_tokens_add` | N/A (helper) | N/A | void | Appends one matching token string into active enumeration results. |
| `0xc40b0` | `hs_tokens_enumerate_range` | N/A (helper) | N/A | void | Enumerates a half-open range `[start, end)` of candidate names into enumeration. |
| `0xc40f0` | `hs_tokens_enumerate_tag_block` | N/A (helper) | N/A | void | Enumerates name elements embedded in a tag block. |
| `0xc4130` | `hs_tokens_enumerate_scenario_tag_block` | N/A (helper) | N/A | void | Enumerates name elements from scenario-resident tag blocks. |
| `0xc4160` | `hs_tokens_enumerate_fixed_commands` | N/A (helper) | N/A | void | Enumerates fixed command literals into token enumeration. |
| `0xc4180` | `hs_tokens_enumerate_special_forms` | N/A (helper) | N/A | void | Enumerates special forms table at `0x2f156c`. |
| `0xc41b0` | `hs_tokens_enumerate_types` | N/A (helper) | N/A | void | Enumerates 0x2d-entry type names table at `0x2f14b8`. |
| `0xc41e0` | `hs_tokens_enumerate_functions` | N/A (helper) | N/A | void | Enumerates all 418 script functions from the function table at `0x2f1588`. |
| `0xc4770` | `hs_file_reference_compare` | N/A (helper) | N/A | int | Case-insensitive comparator for sorting `file_ref_t` array in `hs_needs_recompile`. |
| `0xc4a40` | `hs_function_format_usage` | N/A (helper) | N/A | void | Formats `(<fn_name> <type1> <type2>)` or usage string into a buffer. |
| `0xc4ae0` | `hs_function_get_help` | N/A (helper) | N/A | void | Copies documentation/help description string of a script function descriptor. |
| `0xc4b40` | `hs_evaluate_script_recompile` | `script_recompile` | 56 | void | Sets global script recompile flag and returns 0 to script thread. |
| `0xc4b60` | `hs_evaluate_random_range` | `random_range` | 59 | int | Evaluates (min, max) and returns pseudo-random integer in `[min, max)`. |
| `0xc4bb0` | `hs_evaluate_real_random_range` | `real_random_range` | 60 | real | Evaluates (min, max) and returns pseudo-random float bits in `[min, max)`. |
| `0xc4ff0` | `hs_evaluate_script_doc` | `script_doc` | 57 | void | Dumps documentation for all 418 script functions to `hs_doc.txt`. |
| `0xc5010` | `hs_evaluate_help` | `help` | 58 | void | Displays usage signature and help text for named script function to console. |
| `0xc5310` | `hs_parse_cond_clauses` | N/A (parser) | N/A | int | Recursively rewrites `cond` clause list into an equivalent nested tree of `if` AST nodes. |
| `0xc55d0` | `hs_syntax_get_arguments` | N/A (parser) | N/A | bool | Validates that a function call has exactly N arguments and extracts their AST node handles. |

## Key Technical Details
1. **Immutable Register Calling Conventions**:
   - `0xc4030` (`hs_tokens_add`): `name@<esi>`
   - `0xc40b0` (`hs_tokens_enumerate_range`): `end_index@<eax>, start_index@<ecx>`
   - `0xc40f0` (`hs_tokens_enumerate_tag_block`): `block@<ebx>`
   - `0xc4a40` (`hs_function_format_usage`): `function_index@<eax>, buffer@<esi>`
   - `0xc4ae0` (`hs_function_get_help`): `function_index@<eax>`
   - `0xc55d0` (`hs_syntax_get_arguments`): `syntax_node@<edi>, expected_count@<bx>`
   - All register conventions were preserved identically and synchronized with `tools/kb_reg_baseline.json`.
2. **Terminal Explicit Returns**:
   - Explicit `return;` verified and added to all void-returning functions.
3. **Cross-TU Call Sites Updated**:
   - `hs_compile.c`: updated call site to `hs_parse_cond_clauses`.
   - `hs_runtime.c`: updated 3 call sites to `hs_syntax_get_arguments`.
   - `test_harness.c`: updated documentation/comments.
