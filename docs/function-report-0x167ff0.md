# Function Report: rasterizer_error (0x00167ff0)

## Target Overview

- **Build / Executable:** Original Xbox Halo: Combat Evolved, build `01.10.12.2276` (Oct 12, 2001 debug build, file `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).
- **Target Address:** Virtual Address `0x00167ff0` (Kuna / Ghidra VA), Image Base `0x00010000`, RVA `0x00157ff0`.
- **File Offset:** `0x00157ff0` in `cachebeta.xbe` (within Section 0 `.text`: VA `0x00012000`, Raw `0x00002000`, Size `0x001d49cc`).
- **Current Repository Label:** `FUN_00167ff0` in [`kb.json`](file:///storage/1F34-EBBE/halo/kb.json) (under object `rasterizer_xbox_environment_fog.obj`, currently unported).
- **Authentic Debug Name:** `rasterizer_error` (from `halo_2276_functions.txt` line 5756: `_rasterizer_error`).
- **Section:** `.text` (Executable Code).
- **Function Size:** 429 bytes (`0x1ad` bytes, bounds: `0x00167ff0` to `0x0016819d`).
- **Initial Classification:** Direct3D 8 Error Handler, HRESULT Decoder, and System Error Logger.
- **Boundary Confidence:** **CONFIRMED** (both `tools/verify/function_bounds.json` and `halo_2276_functions.txt` establish identical bounds of `0x1ad` bytes, terminated with `RET 0xc3` at `0x0016819c`).

---

## Kuna Analysis State

- **Kuna Function Definition:** Defined at `0x00167ff0`.
- **Stack Frame Layout:**
  - `EBP+0x00`: Saved old frame pointer (4 bytes).
  - `EBP+0x04`: Return address (4 bytes).
  - `EBP+0x08`: `param_1` (`int hr` / `HRESULT hr`) — Direct3D status/error code (4 bytes).
  - `EBP+0x0c`: `param_2` (`const char *format`) — printf-style format string describing the failed D3D call (4 bytes).
  - `EBP+0x10`: `param_3` / `...` — variable argument list (varargs).
  - `EBP-0x400` to `EBP-0x001`: `error_desc[1024]` (1024 bytes) — buffer holding D3DX/D3D error description string.
  - `EBP-0x800` to `EBP-0x401`: `call_text[1024]` (1024 bytes) — buffer holding formatted call site text.
  - Allocated local frame: 2048 bytes (`sub esp, 0x800`).
  - Callee-saved registers: `ESI`, `EDI`.
- **Disassembly Actions Taken:**
  - Machine code extracted directly from pristine `cachebeta.xbe` via [`tools/verify/xbe_reference.py`](file:///storage/1F34-EBBE/halo/tools/verify/xbe_reference.py).
  - COFF reference object synthesized (`0x167ff0_ref.obj`, 429 bytes) and disassembled via `llvm-objdump -d`.
- **Kuna / Ghidra Scripting Action:**
  ```python
  # Kuna / PyGhidra script sequence (per GhidraBook2E, Ch. 14):
  addr = toAddr(0x00167ff0)
  createFunction(addr, "rasterizer_error")
  setFunctionBody(addr, toAddr(0x0016819d))
  # Set prototype
  setFunctionPrototype(addr, "void __cdecl rasterizer_error(int hr, const char *format, ...)")
  ```

---

## Evidence Ledger

| Tier | Observation | Why it matters | Confidence |
|---|---|---|---|
| **T1** | Debug symbol table entry `_rasterizer_error` at `.text 00167FF0` in `halo_2276_functions.txt` (length `0x1ad`, locals `0x80c`, stack args `0x09`). | Authoritative Bungie symbol name directly from Xbox debug build 2276. Proves identity, length, and signature. | **CONFIRMED** |
| **T1** | Disassembly of `0x167ff0` in `cachebeta.xbe` formats varargs via `vsnprintf`/`vsprintf`, retrieves D3D error description via helper at `0x99c58`, decodes HRESULT via dense jump table into error names (`D3DERR_INVALIDCALL`, `D3DERR_DEVICELOST`, `E_OUTOFMEMORY`, etc.), and calls `error(2, "%s in %s (code=%d, error=%s)", ...)` at `0x8f534`. | Proves the exact error formatting and dispatch behavior. | **CONFIRMED** |
| **T1** | String constants in read-only data: `0x2a21a8` (`"%s in %s (code=%d, error=%s)"`), `0x2666bc` (`"<unknown error>"`), `0x2a240c` (`"<can't get description>"`), and 23 DirectX HRESULT strings (`D3DERR_OUTOFVIDEOMEMORY`, `E_INVALIDARG`, etc.). | Identifies every possible branch and error code decoded by the rasterizer. | **CONFIRMED** |
| **T1** | 99 call sites in repository across 7 key files: `rasterizer.c` (23), `rasterizer_xbox_lights.c` (22), `rasterizer_xbox.c` (30), `rasterizer_xbox_decals.c` (16), `rasterizer_xbox_vertex_shaders_initialize.c` (2), `rasterizer_xbox_vertex_shaders_runtime.c` (5), and `rasterizer_xbox_widgets.c` (1). | Largest unported callee in the graphics subsystem; resolving this eliminates unported trampolines across the entire rasterizer. | **CONFIRMED** |

---

## ABI and Side Effects

| Item | Observation | Confidence |
|---|---|---|
| **Calling Convention** | `__cdecl` standard C varargs convention. Caller cleans stack. | **CONFIRMED** |
| **Parameters** | `int hr` (HRESULT), `const char *format`, `...` (variable arguments). | **CONFIRMED** |
| **Return Value** | `void`. | **CONFIRMED** |
| **Volatile Registers** | `EAX`, `ECX`, `EDX` modified. | **CONFIRMED** |
| **Preserved Registers** | `EBX`, `ESI`, `EDI`, `EBP`, `ESP` preserved. | **CONFIRMED** |
| **Side Effects** | Formats error message and invokes the engine error system (`error(2, ...)`), which logs to console/debugger and halts execution if in fail-fast mode. | **CONFIRMED** |

---

## Mathematical and Algorithmic Analysis

### HRESULT Decoding Logic

The function checks `hr` against standard Direct3D 8 and COM return codes:

1. **COM Status Codes:**
   - `0x80004005`: `E_FAIL`
   - `0x8007000e`: `E_OUTOFMEMORY`
   - `0x80070057`: `E_INVALIDARG`

2. **D3D 8 Error Codes (0x88760000 base):**
   - `0x8876017c`: `D3DERR_OUTOFVIDEOMEMORY`
   - `0x88760818`: `D3DERR_WRONGTEXTUREFORMAT`
   - `0x88760819`: `D3DERR_UNSUPPORTEDCOLOROPERATION`
   - `0x8876081a`: `D3DERR_UNSUPPORTEDCOLORARG`
   - `0x8876081b`: `D3DERR_UNSUPPORTEDALPHAOPERATION`
   - `0x8876081c`: `D3DERR_UNSUPPORTEDALPHAARG`
   - `0x8876081d`: `D3DERR_TOOMANYOPERATIONS`
   - `0x8876081e`: `D3DERR_CONFLICTINGTEXTUREFILTER`
   - `0x8876081f`: `D3DERR_UNSUPPORTEDFACTORVALUE`
   - `0x88760820`: `D3DERR_CONFLICTINGRENDERSTATE`
   - `0x88760821`: `D3DERR_UNSUPPORTEDTEXTUREFILTER`
   - `0x88760822`: `D3DERR_CONFLICTINGTEXTUREPALETTE`
   - `0x88760866`: `D3DERR_DRIVERINTERNALERROR`
   - `0x88760868`: `D3DERR_NOTFOUND`
   - `0x88760869`: `D3DERR_MOREDATA`
   - `0x8876086a`: `D3DERR_DEVICELOST`
   - `0x8876086b`: `D3DERR_DEVICENOTRESET`
   - `0x8876086c`: `D3DERR_NOTAVAILABLE`
   - `0x8876086d`: `D3DERR_INVALIDDEVICE`
   - `0x8876086e`: `D3DERR_INVALIDCALL`
   - Any other value: fallback to string `"<unknown error>"`.

3. **Message Formatting & Dispatch:**
   - Calls `vsnprintf(call_text, sizeof(call_text), format, args)`.
   - Calls `D3DXGetErrorString(hr, error_desc, sizeof(error_desc))`. If lookup fails ($< 0$), copies `"<can't get description>"`.
   - Dispatches formatted alert:
     ```c
     error(2, "%s in %s (code=%d, error=%s)", error_name, call_text, hr, error_desc);
     ```

---

## Disassembly Walkthrough

Annotated machine code from pristine `0x167ff0_ref.obj`:

```assembly
00000000 <FUN_00167ff0>:
       0: pushl %ebp
       1: movl  %esp, %ebp
       3: subl  $0x800, %esp          ; 2048 bytes on stack: two 1024-byte buffers
       9: movl  0xc(%ebp), %ecx       ; format string
       c: pushl %esi
       d: pushl %edi
       e: leal  0x10(%ebp), %eax      ; &varargs
      11: pushl %eax                  ; args
      12: pushl %ecx                  ; format
      13: leal  -0x800(%ebp), %edx    ; call_text buffer
      19: pushl %edx
      1a: movl  $0x2666bc, %esi       ; default esi = "<unknown error>"
      1f: calll vsprintf              ; Format call text into call_text
      24: movl  0x8(%ebp), %edi       ; edi = hr
      27: addl  $0xc, %esp
      2a: pushl $0x3ff                ; max_len = 1023
      2f: leal  -0x400(%ebp), %eax    ; error_desc buffer
      35: pushl %eax
      36: pushl %edi                  ; hr
      37: calll D3DXGetErrorStringA   ; Helper at 0x99c58
      3c: testl %eax, %eax
      3e: jge   0x54                  ; If succeeded, proceed to switch
      40: leal  -0x400(%ebp), %ecx
      46: pushl $0x2a240c             ; "<can't get description>"
      4b: pushl %ecx
      4c: calll strcpy                ; Copy fallback description
      51: addl  $0x8, %esp

      ; --- HRESULT Switch Table ---
      54: cmpl  $0x8876081f, %edi
      5a: jg    0x124
      60: je    0x11d                 ; D3DERR_UNSUPPORTEDFACTORVALUE
      66: cmpl  $0x88760819, %edi
      6c: jg    0xde
      6e: je    0xd4                  ; D3DERR_UNSUPPORTEDCOLOROPERATION
      70: cmpl  $0x80070057, %edi
      76: jg    0xac
      78: je    0xa2                  ; E_INVALIDARG
      7a: cmpl  $0x80004005, %edi
      80: je    0x98                  ; E_FAIL
      82: cmpl  $0x8007000e, %edi
      88: jne   0x188                 ; Unknown -> jump to dispatch
      8e: movl  $0x2a23fc, %esi       ; "E_OUTOFMEMORY"
      93: jmp   0x188
      98: movl  $0x2a23f4, %esi       ; "E_FAIL"
      9d: jmp   0x188
      a2: movl  $0x2a23e4, %esi       ; "E_INVALIDARG"
      a7: jmp   0x188
      ac: cmpl  $0x8876017c, %edi
      b2: je    0xca                  ; D3DERR_OUTOFVIDEOMEMORY
      b4: cmpl  $0x88760818, %edi
      ba: jne   0x188
      c0: movl  $0x2a23c8, %esi       ; "D3DERR_WRONGTEXTUREFORMAT"
      c5: jmp   0x188
      ca: movl  $0x2a23b0, %esi       ; "D3DERR_OUTOFVIDEOMEMORY"
      cf: jmp   0x188
      d4: movl  $0x2a238c, %esi       ; "D3DERR_UNSUPPORTEDCOLOROPERATION"
      d9: jmp   0x188
      de: leal  0x7789f7e6(%edi), %eax ; Dense range: 0x8876081a - 0x8876081e
      e4: cmpl  $0x4, %eax
      e7: ja    0x188
      ed: jmpl  *0x1681a0(,%eax,4)     ; Jump table dispatch for renderstate errors
     ...
     188: leal  -0x400(%ebp), %eax    ; error_desc
     18e: pushl %eax
     18f: pushl %edi                  ; hr
     190: leal  -0x800(%ebp), %ecx    ; call_text
     196: pushl %ecx
     197: pushl %esi                  ; error_name string
     198: pushl $0x2a21a8             ; "%s in %s (code=%d, error=%s)"
     19d: pushl $0x2                  ; error severity = 2
     19f: calll error                 ; Engine error logger at 0x8f534
     1a4: addl  $0x18, %esp
     1a7: popl  %edi
     1a8: popl  %esi
     1a9: movl  %ebp, %esp
     1ab: popl  %ebp
     1ac: retl
```

---

## Proposed C Re-implementation

```c
/* rasterizer_error.c / rasterizer.c */

void rasterizer_error(int hr, const char *format, ...)
{
    va_list args;
    char call_text[1024];
    char error_desc[1024];
    const char *error_name = "<unknown error>";

    va_start(args, format);
    vsprintf(call_text, format, args);
    va_end(args);

    if (D3DXGetErrorStringA(hr, error_desc, sizeof(error_desc) - 1) < 0) {
        strcpy(error_desc, "<can't get description>");
    }

    switch ((unsigned int)hr) {
    case 0x8007000e: error_name = "E_OUTOFMEMORY"; break;
    case 0x80004005: error_name = "E_FAIL"; break;
    case 0x80070057: error_name = "E_INVALIDARG"; break;
    case 0x8876017c: error_name = "D3DERR_OUTOFVIDEOMEMORY"; break;
    case 0x88760818: error_name = "D3DERR_WRONGTEXTUREFORMAT"; break;
    case 0x88760819: error_name = "D3DERR_UNSUPPORTEDCOLOROPERATION"; break;
    case 0x8876081a: error_name = "D3DERR_UNSUPPORTEDCOLORARG"; break;
    case 0x8876081b: error_name = "D3DERR_UNSUPPORTEDALPHAOPERATION"; break;
    case 0x8876081c: error_name = "D3DERR_UNSUPPORTEDALPHAARG"; break;
    case 0x8876081d: error_name = "D3DERR_TOOMANYOPERATIONS"; break;
    case 0x8876081e: error_name = "D3DERR_CONFLICTINGTEXTUREFILTER"; break;
    case 0x8876081f: error_name = "D3DERR_UNSUPPORTEDFACTORVALUE"; break;
    case 0x88760820: error_name = "D3DERR_CONFLICTINGRENDERSTATE"; break;
    case 0x88760821: error_name = "D3DERR_UNSUPPORTEDTEXTUREFILTER"; break;
    case 0x88760822: error_name = "D3DERR_CONFLICTINGTEXTUREPALETTE"; break;
    case 0x88760866: error_name = "D3DERR_DRIVERINTERNALERROR"; break;
    case 0x88760868: error_name = "D3DERR_NOTFOUND"; break;
    case 0x88760869: error_name = "D3DERR_MOREDATA"; break;
    case 0x8876086a: error_name = "D3DERR_DEVICELOST"; break;
    case 0x8876086b: error_name = "D3DERR_DEVICENOTRESET"; break;
    case 0x8876086c: error_name = "D3DERR_NOTAVAILABLE"; break;
    case 0x8876086d: error_name = "D3DERR_INVALIDDEVICE"; break;
    case 0x8876086e: error_name = "D3DERR_INVALIDCALL"; break;
    default: break;
    }

    error(2, "%s in %s (code=%d, error=%s)", error_name, call_text, hr, error_desc);
}
```

---

## Integration and Next Steps

1. **Update `kb.json`:**
   - Rename `FUN_00167ff0` $\rightarrow$ `rasterizer_error`.
   - Update declaration: `void rasterizer_error(int hr, const char *format, ...);`.
2. **Rename across `src/`:**
   - Update all 99 call sites in:
     - `src/halo/rasterizer/rasterizer.c`
     - `src/halo/rasterizer/xbox/rasterizer_xbox_lights.c`
     - `src/halo/rasterizer/xbox/rasterizer_xbox.c`
     - `src/halo/rasterizer/xbox/rasterizer_xbox_decals.c`
     - `src/halo/rasterizer/xbox/rasterizer_xbox_vertex_shaders_initialize.c`
     - `src/halo/rasterizer/xbox/rasterizer_xbox_vertex_shaders_runtime.c`
     - `src/halo/rasterizer/xbox/rasterizer_xbox_widgets.c`
3. **Linkage:**
   - Eliminates 99 instances of `FUN_00167ff0` placeholder symbol usage project-wide.
