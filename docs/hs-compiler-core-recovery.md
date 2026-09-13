# HaloScript Compiler Core Recovery: 5 AST & Lexer Routines (`hs_compile.obj`)

## Overview
- **Binary Target**: Halo CE Xbox debug build 2276 (`halo-patched/cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).
- **Object**: `hs_compile.obj` (addresses `0xc5960`, `0xc71c0`, `0xc72b0`, `0xc7b10`, `0xc7ca0`).
- **Scope**: 5 core HaloScript compilation, parsing, lexing, and AST management routines:
  1. `0xc5960` — `hs_compile_resolve_predicate`: Resolves expression predicate name into function/script index and updates call node flags.
  2. `0xc71c0` — `hs_parse_atom`: Parses quoted string literals and unquoted identifiers, converting to lowercase and checking termination.
  3. `0xc72b0` — `hs_skip_whitespace`: Lexer routine skipping whitespace, newlines, line comments (`;...`), and block comments (`;*...*;`).
  4. `0xc7b10` — `hs_syntax_node_reintern_strings`: Traverses syntax nodes/trees to re-intern string constants during script recompilation.
  5. `0xc7ca0` — `hs_parse_parenthesized_expression`: Parses parenthesized compound expressions, sub-expression chaining, and parenthesis matching.
- **Decompilation Oracle**: Verified against pristine assembly extracted and decompiled via `kuna` (`/data/data/com.termux/files/home/bin/kuna`) from synthesized reference COFF objects.
- **Standards**: Strict C89 compliance, explicit terminal `return;` on all void functions (Rule 3), parameter formatting (Rules 1 & 2), preservation of all `@<reg>` calling conventions, and zero register drift.

---

## Recovered Functions Summary

| Address | Authentic Name | Calling Convention / Regs | Return Type | Role / Subsystem | Description |
|---|---|---|---|---|---|
| `0xc5960` | `hs_compile_resolve_predicate` | `int datum_index@<eax>` | `void` | Compiler AST Resolver | Resolves function or script index from predicate node name; sets `is_script` flag and asserts `predicate->function_index!=NONE`. |
| `0xc71c0` | `hs_parse_atom` | `int datum_index@<eax>, char **cursor@<esi>` | `void` | Lexer / Atom Parser | Parses identifier or quoted string literal, handles unterminated quotes, and converts result to lowercase via `csstr_tolower`. |
| `0xc72b0` | `hs_skip_whitespace` | `char **cursor@<esi>` | `void` | Lexer / Comment Stripper | Advances source cursor past spaces, tabs, newlines, single-line comments (`;`), and block comments (`;*...*;`). |
| `0xc7b10` | `hs_syntax_node_reintern_strings` | `int datum_index` (`__cdecl`) | `void` | Recompiler AST Cleaner | Traverses AST nodes, marks dirty flag bit 3 (`0x8`), and re-interns string constants for scenario recompilation. |
| `0xc7ca0` | `hs_parse_parenthesized_expression` | `char **cursor, int datum_index` (`__cdecl`) | `void` | Parser / Compound Expr | Parses parenthesized list, skipping whitespace and chaining child syntax nodes via `next_node` field. |

---

## Technical Details & Byte-Matching Verification

### 1. Register Calling Conventions & Baseline Preservation
Three of the five routines utilize custom x86 register arguments in Halo CE debug build 2276:
- `0xc5960` (`hs_compile_resolve_predicate`): `datum_index` is passed in `%eax`.
- `0xc71c0` (`hs_parse_atom`): `datum_index` in `%eax`, `cursor` pointer in `%esi`.
- `0xc72b0` (`hs_skip_whitespace`): `cursor` pointer in `%esi`.
All register assignments were verified with `extract_reg_args.py --check` and synchronized with `tools/kb_reg_baseline.json`.

### 2. Decompilation & Logic Correspondence (via `kuna`)

#### `0xc5960` — `hs_compile_resolve_predicate`
Reference decompilation:
```c
void FUN_000c5960(void)
{
  short v1; // ax
  int v2; // eax
  int v3; // eax
  int v4; // eax
  
  v2 = sub_4539c0(dat_5aa6c8);
  v3 = sub_4539c0(dat_5aa6c8);
  v3 = sub_4539c0(dat_5aa6c8,*(unsigned int *)(v3 + 0x10));
  if (*(short *)(v3 + 4) != 2) {
    v4 = *(int *)(v3 + 0xc) + dat_46b6e8;
    *(unsigned short *)(v2 + 2) = sub_3fe660(v4);
    *(unsigned short *)(v3 + 4) = 2;
    if (*(short *)(v2 + 2) == -1) {
      v1 = sub_3fe3f0(*(int *)(v3 + 0xc) + dat_46b6e8);
      *(short *)(v2 + 2) = v1;
      if (v1 != -1)
        *(unsigned char *)(v2 + 6) = *(unsigned char *)(v2 + 6) | 2;
    }
    *(unsigned short *)(v3 + 2) = *(unsigned short *)(v2 + 2);
    return;
  }
  if (*(short *)(v3 + 2) == -1) {
    sub_3c8090(0x27be04,0x27bd0c,0x520,1);
    sub_3c8990(0xffffffff);
  }
  *(unsigned short *)(v2 + 2) = *(unsigned short *)(v3 + 2);
}
```
- Strings and line asserts:
  - `0x27be04`: `"predicate->function_index!=NONE"`
  - `0x27bd0c`: `"c:\\halo\\SOURCE\\hs\\hs_compile.c"`
  - `0x520`: Line 1312

#### `0xc71c0` — `hs_parse_atom`
Reference decompilation:
```c
void FUN_000c71c0(void)
{
  char v1;
  int v2; // eax
  short v3; // cx
  char *v4;
  int *v5; // esi
  
  v2 = sub_452160(dat_5aa6c8);
  v4 = (char *)*v5;
  if (*v4 == '\"') {
    *v5 = (int)&v4[1];
    *(int *)(v2 + 0xc) = (int)&v4[1] - dat_46b6e8;
    v1 = *(char *)*v5;
    while ((v1 && (*(char *)*v5 != '\"'))) {
      v4 = &((char *)*v5)[1];
      *v5 = (int)v4;
      v1 = *v4;
    }
    if (!*(char *)*v5) {
      dat_46b6fc = 0x27c7d8;
      dat_46b700 = *(int *)(v2 + 0xc) + -1;
    }
    *(char *)*v5 = 0;
    *v5 = *v5 + 1;
    sub_3c67e0(*(int *)(v2 + 0xc) + dat_46b6e8);
    return;
  }
  *(int *)(v2 + 0xc) = (int)v4 - dat_46b6e8;
  if (*(char *)*v5) {
    while( true ) {
      v1 = *(char *)*v5;
      if ((v1 == ')') || (v1 == ';')) break;
      v3 = 0;
      do {
        if (v1 == *(char *)(v3 + 0x27bb78)) {
          sub_3c67e0(*(int *)(v2 + 0xc) + dat_46b6e8);
          return;
        }
        v3 += 1;
      } while (v3 < 2);
      v3 = 0;
      do {
        if (v1 == *(char *)(v3 + 0x27bb7c)) {
          sub_3c67e0(*(int *)(v2 + 0xc) + dat_46b6e8);
          return;
        }
        v3 += 1;
      } while (v3 < 2);
      v4 = &((char *)*v5)[1];
      *v5 = (int)v4;
      if (!*v4) break;
    }
  }
  sub_3c67e0(*(int *)(v2 + 0xc) + dat_46b6e8);
  return;
}
```
- Strings:
  - `0x27c7d8`: `"this quoted constant is unterminated."`
  - `0x27bb78`: Whitespace delimiters (`" \t"`)
  - `0x27bb7c`: Newline delimiters (`"\r\n"`)

#### `0xc72b0` — `hs_skip_whitespace`
Reference state machine matches the four states `(0 = scan, 1 = line comment, 2 = block comment, default = assert unreachable)`.
- Strings and line asserts:
  - `0x27c800`: `"unterminated comment."`
  - `0x255ee8`: `"!\"unreachable\""` at line `0x46c` (1132) of `"c:\\halo\\SOURCE\\hs\\hs_compile.c"`.

#### `0xc7b10` — `hs_syntax_node_reintern_strings`
Re-interns string constants via `FUN_000c6a70` (`hs_compile_intern_string`) and recurses down child node chains (`node->child_node` and `child_node->next_node`).

#### `0xc7ca0` — `hs_parse_parenthesized_expression`
Reference error messages:
- `0x27cc0c`: `"this left parenthesis is unmatched."`
- `0x27cbf0`: `"this expression is empty."`

---

## Verification Summary
- **Register Argument Drift**: `886 OK, 0 drift, 0 missing, 0 stale` via `python3 tools/audit/extract_reg_args.py --check`.
- **Ported Deactivations**: `46 allowlisted, 0 unallowlisted` via `python3 tools/audit/check_ported_deactivations.py --check`.
- **Compiler Build**: `cmake --build build --target halo` succeeded with 0 errors.
- **XBE Patching**: `cmake --build build --target patched_xbe` completed with valid redirect generation.
- **Symbol Exports**: `llvm-nm` confirms `_hs_compile_resolve_predicate`, `_hs_parse_atom`, `_hs_skip_whitespace`, `_hs_syntax_node_reintern_strings`, and `_hs_parse_parenthesized_expression` exported as `T` symbols in `hs_compile.c.obj`.
