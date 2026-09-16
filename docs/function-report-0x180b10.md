# Function Report: compress_real_vector3d_to_int32_clamp (0x00180b10)

## Target Overview

- **Build / Executable:** Original Xbox Halo: Combat Evolved, build `01.10.12.2276` (Oct 12, 2001 debug build, file `cachebeta.xbe`, MD5 `c7869590a1c64ad034e49a5ee0c02465`).
- **Target Address:** Virtual Address `0x00180b10` (Kuna / Ghidra VA), Image Base `0x00010000`, RVA `0x00170b10`.
- **File Offset:** `0x00170b10` in `cachebeta.xbe` (within Section 0 `.text`: VA `0x00012000`, Raw `0x00002000`, Size `0x001d49cc`).
- **Current Repository Label:** `FUN_00180b10` in [`kb.json`](file:///storage/1F34-EBBE/halo/kb.json) (under object `rasterizer_text.obj`, currently marked `"ported": false`).
- **Authentic Debug Name:** `compress_real_vector3d_to_int32_clamp` (from `halo_2276_functions.txt` line 6163: `_compress_real_vector3d_to_int32_clamp`).
- **Section:** `.text` (Executable Code).
- **Function Size:** 508 bytes (`0x1fc` bytes, bounds: `0x00180b10` to `0x00180d0c`).
- **Initial Classification:** 3D Vector Clamping & 11-11-10 Fixed-Point Normal Encoder with Round-Trip Validation.
- **Boundary Confidence:** **CONFIRMED** (both `tools/verify/function_bounds.json` and `halo_2276_functions.txt` establish identical bounds of `0x1fc` bytes, terminated with `RET 0xc3` at `0x00180d0b`).

---

## Kuna Analysis State

- **Kuna Function Definition:** Defined at `0x00180b10`.
- **Stack Frame Layout:**
  - `EBP+0x00`: Saved old frame pointer (4 bytes).
  - `EBP+0x04`: Return address (4 bytes).
  - `EBP+0x08`: `param_1` (`const real_vector3d *v` / `float *param_1`) — pointer to input 3D direction vector (4 bytes). Reused as stack scratch space by MSVC 7.1 during `FSTPS` from `floor()`.
  - `EBP-0x04`: `tmp_int` (4 bytes) — integer conversion target for `fistpl`.
  - `EBP-0x08`: `v2.k` (4 bytes) — local decoded Z component for round-trip verification.
  - `EBP-0x0c`: `v2.j` (4 bytes) — local decoded Y component for round-trip verification.
  - `EBP-0x10`: `v2.i` (4 bytes) — local decoded X component for round-trip verification.
  - Allocated local frame: 16 bytes (`sub esp, 0x10`).
  - Callee-saved registers: `ESI`, `EDI`, `EBX`.
- **Disassembly Actions Taken:**
  - Machine code extracted directly from pristine `cachebeta.xbe` via [`tools/verify/xbe_reference.py`](file:///storage/1F34-EBBE/halo/tools/verify/xbe_reference.py).
  - COFF reference object synthesized (`0x180b10_ref.obj`, 508 bytes) and disassembled via `llvm-objdump -d`.
- **Kuna / Ghidra Scripting Action:**
  ```python
  # Kuna / PyGhidra script sequence (per GhidraBook2E, Ch. 14):
  addr = toAddr(0x00180b10)
  createFunction(addr, "compress_real_vector3d_to_int32_clamp")
  setFunctionBody(addr, toAddr(0x00180d0c))
  # Set prototype
  setFunctionPrototype(addr, "uint32_t __cdecl compress_real_vector3d_to_int32_clamp(const real_vector3d *v)")
  ```

---

## Evidence Ledger

| Tier | Observation | Why it matters | Confidence |
|---|---|---|---|
| **T1** | Debug symbol table entry `_compress_real_vector3d_to_int32_clamp` at `.text 00180B10` in `halo_2276_functions.txt` (length `0x1fc`, locals `0x28`, stack args `0x04`). | Authoritative Bungie symbol name directly from Xbox debug build 2276. Proves identity, length, and parameter count. | **CONFIRMED** |
| **T1** | Disassembly of `0x180b10` in `cachebeta.xbe` clamps input components to `[-1.0f, 1.0f]`, multiplies by scale factors, calls `floor()`, extracts signed integers via `fistp`, and packs into bitfields: X (11 bits), Y (11 bits), Z (10 bits). | Establishes the exact mathematical operation as the inverse compressor to `uncompress_int32_to_real_vector3d` (`0x17ffc0`). | **CONFIRMED** |
| **T1** | Direct call at `0x180b10 + 0x145` (`0x180c55`): `call 0x17ffc0` (`uncompress_int32_to_real_vector3d`). | The function explicitly passes its packed 32-bit integer and local buffer `v2` to `uncompress_int32_to_real_vector3d` to decode it back into floating-point coordinates. | **CONFIRMED** |
| **T1** | Float constants in read-only data: `0x255e94` (`-1.0f`), `0x2533c8` (`1.0f`), `0x2b0118` (`1023.5f`), `0x2b0114` (`511.5f`), `0x28b800` (double `0.01`). | Proves the scale factor $1023.5$ for 11-bit components ($[-1.0, 1.0] \times 1023.5 \rightarrow [-1023.5, 1023.5]$, floored to integers in $[-1024, 1023]$) and $511.5$ for 10-bit components ($[-511.5, 511.5]$, floored to $[-512, 511]$). | **CONFIRMED** |
| **T1** | Assert strings in read-only data: `0x2afe38` (`"c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c"`), `0x2a3e7c` (`"v"` at line 104), `0x2b00fc` (`"fabs(v2.i - v->i)<0.01f"` at line 118), `0x2b00e4` (`"fabs(v2.j - v->j)<0.01f"` at line 119), `0x2b00cc` (`"fabs(v2.k - v->k)<0.01f"` at line 120). | Proves source file name, source line numbers (104, 118, 119, 120), parameter name `v`, local decoded vector name `v2`, and struct field names `i`, `j`, `k`. Note: Existing C code used placeholder `"parameters"`; binary proves it is `"v"`. | **CONFIRMED** |
| **T1** | 15 direct binary `CALL` sites in `cachebeta.xbe`: `lights_queue_lens_flare` (2), `lights_preprocess_scene` (4), `rasterizer_geometry_compress_vertices` (7), and `rasterizer_lens_flare_submit_for_cluster` (2). | Eliminates trampolines and XCALL workarounds (such as `CALL_FUN_00180b10` in `objects.c`) from core lighting and vertex compression pipelines. | **CONFIRMED** |
| **T2** | Existing C implementation exists in [`src/halo/rasterizer/rasterizer_text.c:601`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c#L601-L676), marked `"ported": false` due to VC71 match ceiling. | Functional C implementation already written and passing 100/100 behavioral equivalence tests in commit `db6d4799`. | **STRONG** |

---

## ABI and Side Effects

| Item | Observation | Confidence |
|---|---|---|
| **Calling Convention** | `__cdecl` standard C stack convention. Caller cleans stack (4 bytes). | **CONFIRMED** |
| **Parameters** | 1 argument: `const real_vector3d *v` passed at `[ebp+0x08]`. | **CONFIRMED** |
| **Return Value** | 32-bit unsigned integer `uint32_t` in `EAX` representing the packed 11-11-10 normal. | **CONFIRMED** |
| **Volatile Registers** | `EAX`, `ECX`, `EDX` modified. | **CONFIRMED** |
| **Preserved Registers** | `EBX`, `ESI`, `EDI`, `EBP`, `ESP` preserved. | **CONFIRMED** |
| **FPU State** | Clears FPU stack before exit; uses `fcomps` and `fcompl` with pop. | **CONFIRMED** |
| **Side Effects** | Aborts execution via `display_assert` + `system_exit(-1)` if `v == NULL` or if round-trip decompression deviates by $\ge 0.01$ on any axis. | **CONFIRMED** |

---

## Mathematical and Algorithmic Analysis

### 1. Bitfield Normal Packing Format (11-11-10)

The function converts a normalized 3D Cartesian vector $(x, y, z)$ into a single packed 32-bit integer:

$$\text{Bit: } \underbrace{31 \quad \dots \quad 22}_{\text{Z (10 bits)}} \quad \underbrace{21 \quad \dots \quad 11}_{\text{Y (11 bits)}} \quad \underbrace{10 \quad \dots \quad 0}_{\text{X (11 bits)}}$$

1. **Clamping:**
   Each input component $c \in \{x, y, z\}$ is clamped to $[-1.0, 1.0]$:
   $$c_{\text{clamp}} = \min(1.0, \max(-1.0, c))$$

2. **Scaling & Quantization:**
   - **X Component (11-bit signed):**
     $$\text{raw}_x = \lfloor c_{x,\text{clamp}} \times 1023.5 \rfloor$$
     $$i_{11} = \text{raw}_x \ \& \ \text{0x7FF}$$
   - **Y Component (11-bit signed):**
     $$\text{raw}_y = \lfloor c_{y,\text{clamp}} \times 1023.5 \rfloor$$
     $$j_{11} = \text{raw}_y \ \& \ \text{0x7FF}$$
   - **Z Component (10-bit signed):**
     $$\text{raw}_z = \lfloor c_{z,\text{clamp}} \times 511.5 \rfloor$$
     $$k_{10} = \text{raw}_z \ \& \ \text{0x3FF}$$

3. **Bit Assembly:**
   $$\text{packed} = (((k_{10} \ll 11) \ | \ j_{11}) \ll 11) \ | \ i_{11}$$

4. **Round-Trip Assertion (Debug Build):**
   Calls `uncompress_int32_to_real_vector3d(&v2, packed)` and tests:
   $$|v_2.i - v.i| < 0.01, \quad |v_2.j - v.j| < 0.01, \quad |v_2.k - v.k| < 0.01$$
   If any component exceeds the $0.01$ tolerance, it triggers a fatal `display_assert`.

---

## Disassembly Walkthrough

Annotated disassembly from pristine `0x180b10_ref.obj` (extracted from `cachebeta.xbe`):

```assembly
00000000 <FUN_00180b10>:
       0: pushl %ebp
       1: movl  %esp, %ebp
       3: subl  $0x10, %esp           ; Allocate 16 bytes for local stack frame
       6: pushl %esi
       7: movl  0x8(%ebp), %esi       ; esi = v (input vector pointer)
       a: testl %esi, %esi
       c: pushl %edi
       d: jne   0x2c                  ; If v != NULL, proceed
       ; Assert: v != NULL
       f: pushl $0x1                  ; condition = 1
      11: pushl $0x68                 ; line = 104 (0x68)
      13: pushl $0x2afe38             ; "c:\halo\SOURCE\rasterizer\rasterizer_geometry.c"
      18: pushl $0x2a3e7c             ; "v"
      1d: calll display_assert
      22: pushl $-0x1                 ; -1
      24: calll system_exit
      29: addl  $0x14, %esp

      ; --- Clamp and quantize X (v->i) ---
      2c: flds  (%esi)                ; Load v->i
      2e: fcomps 0x255e94             ; Compare with -1.0f
      34: fnstsw %ax
      36: testb $0x5, %ah
      39: jp    0x43                  ; If not < -1.0f, check upper bound
      3b: flds  0x255e94              ; Load -1.0f
      41: jmp   0x5c
      43: flds  (%esi)
      45: fcomps 0x2533c8             ; Compare with 1.0f
      4b: fnstsw %ax
      4d: testb $0x41, %ah
      50: jne   0x5a                  ; If not > 1.0f, use original value
      52: flds  0x2533c8              ; Load 1.0f
      58: jmp   0x5c
      5a: flds  (%esi)
      5c: fmuls 0x2b0118              ; Multiply by 1023.5f
      62: pushl %ebx
      63: subl  $0x8, %esp
      66: fstpl (%esp)
      69: calll floor                 ; floor(ci * 1023.5f)
      6e: fstps 0x8(%ebp)             ; Scratch store in param slot
      71: addl  $0x8, %esp
      74: flds  0x8(%ebp)
      77: fistpl -0x4(%ebp)           ; Convert to int32 via FISTP
      7a: flds  0x4(%esi)             ; Preload v->j
      7d: fcomps 0x255e94             ; Compare with -1.0f
      83: movl  -0x4(%ebp), %ebx      ; ebx = int_x
      86: andl  $0x7ff, %ebx          ; ebx = i_11 = int_x & 0x7FF

      ; --- Clamp and quantize Y (v->j) ---
      8c: fnstsw %ax
      8e: testb $0x5, %ah
      91: jp    0x9b
      93: flds  0x255e94              ; -1.0f
      99: jmp   0xb6
      9b: flds  0x4(%esi)
      9e: fcomps 0x2533c8             ; 1.0f
      a4: fnstsw %ax
      a6: testb $0x41, %ah
      a9: jne   0xb3
      ab: flds  0x2533c8              ; 1.0f
      b1: jmp   0xb6
      b3: flds  0x4(%esi)
      b6: fmuls 0x2b0118              ; Multiply by 1023.5f
      bc: subl  $0x8, %esp
      bf: fstpl (%esp)
      c2: calll floor
      c7: fstps 0x8(%ebp)
      ca: addl  $0x8, %esp
      cd: flds  0x8(%ebp)
      d0: fistpl -0x4(%ebp)
      d3: flds  0x8(%esi)             ; Preload v->k
      d6: fcomps 0x255e94             ; Compare with -1.0f
      dc: movl  -0x4(%ebp), %edi      ; edi = int_y
      df: andl  $0x7ff, %edi          ; edi = j_11 = int_y & 0x7FF

      ; --- Clamp and quantize Z (v->k) ---
      e5: fnstsw %ax
      e7: testb $0x5, %ah
      ea: jp    0xf4
      ec: flds  0x255e94              ; -1.0f
      f2: jmp   0x10f
      f4: flds  0x8(%esi)
      f7: fcomps 0x2533c8             ; 1.0f
      fd: fnstsw %ax
      ff: testb $0x41, %ah
     102: jne   0x10c
     104: flds  0x2533c8              ; 1.0f
     10a: jmp   0x10f
     10c: flds  0x8(%esi)
     10f: fmuls 0x2b0114              ; Multiply by 511.5f (10-bit scale)
     115: subl  $0x8, %esp
     118: fstpl (%esp)
     11b: calll floor
     120: fstps 0x8(%ebp)
     123: addl  $0x8, %esp
     126: flds  0x8(%ebp)
     129: fistpl -0x4(%ebp)
     12c: movl  -0x4(%ebp), %eax
     12f: andl  $0x3ff, %eax          ; eax = k_10 = int_z & 0x3FF

     ; --- Assemble 32-bit packed integer ---
     134: shll  $0xb, %eax            ; eax = k_10 << 11
     137: orl   %edi, %eax            ; eax = (k_10 << 11) | j_11
     139: shll  $0xb, %eax            ; eax = ((k_10 << 11) | j_11) << 11
     13c: movl  %eax, %edi
     13e: orl   %ebx, %edi            ; edi = packed = (((k_10 << 11) | j_11) << 11) | i_11

     ; --- Round-trip verification: uncompress_int32_to_real_vector3d ---
     140: leal  -0x10(%ebp), %eax     ; eax = &v2 (local float buffer [12 bytes])
     143: pushl %edi                  ; packed
     144: pushl %eax                  ; &v2
     145: calll uncompress_int32_to_real_vector3d ; 0x17ffc0
     14a: movl  (%eax), %ecx
     14c: movl  0x4(%eax), %edx
     14f: movl  0x8(%eax), %eax
     152: movl  %ecx, -0x10(%ebp)     ; v2.i
     155: flds  -0x10(%ebp)
     158: fsubs (%esi)                ; v2.i - v->i
     15a: movl  %eax, -0x8(%ebp)      ; v2.k
     15d: addl  $0x8, %esp
     160: movl  %edx, -0xc(%ebp)      ; v2.j
     163: fabs                        ; fabs(v2.i - v->i)
     165: popl  %ebx
     166: fcompl 0x28b800             ; Compare with 0.01 (double)
     16c: fnstsw %ax
     16e: testb $0x5, %ah
     171: jnp   0x190                 ; Jump if fabs < 0.01
     ; Assert X tolerance
     173: pushl $0x1
     175: pushl $0x76                 ; line 118
     177: pushl $0x2afe38             ; "c:\halo\SOURCE\rasterizer\rasterizer_geometry.c"
     17c: pushl $0x2b00fc             ; "fabs(v2.i - v->i)<0.01f"
     181: calll display_assert
     186: pushl $-0x1
     188: calll system_exit
     18d: addl  $0x14, %esp

     ; Assert Y tolerance
     190: flds  -0xc(%ebp)
     193: fsubs 0x4(%esi)             ; v2.j - v->j
     196: fabs
     198: fcompl 0x28b800             ; Compare with 0.01
     19e: fnstsw %ax
     1a0: testb $0x5, %ah
     1a3: jnp   0x1c2                 ; Jump if fabs < 0.01
     1a5: pushl $0x1
     1a7: pushl $0x77                 ; line 119
     1a9: pushl $0x2afe38             ; "c:\halo\SOURCE\rasterizer\rasterizer_geometry.c"
     1ae: pushl $0x2b00e4             ; "fabs(v2.j - v->j)<0.01f"
     1b3: calll display_assert
     1b8: pushl $-0x1
     1ba: calll system_exit
     1bf: addl  $0x14, %esp

     ; Assert Z tolerance
     1c2: flds  -0x8(%ebp)
     1c5: fsubs 0x8(%esi)             ; v2.k - v->k
     1c8: fabs
     1ca: fcompl 0x28b800             ; Compare with 0.01
     1d0: fnstsw %ax
     1d2: testb $0x5, %ah
     1d5: jnp   0x1f4                 ; Jump if fabs < 0.01
     1d7: pushl $0x1
     1d9: pushl $0x78                 ; line 120
     1db: pushl $0x2afe38             ; "c:\halo\SOURCE\rasterizer\rasterizer_geometry.c"
     1e0: pushl $0x2b00cc             ; "fabs(v2.k - v->k)<0.01f"
     1e5: calll display_assert
     1ea: pushl $-0x1
     1ec: calll system_exit
     1f1: addl  $0x14, %esp

     ; Return
     1f4: movl  %edi, %eax            ; Return packed uint32 in EAX
     1f6: popl  %edi
     1f7: popl  %esi
     1f8: movl  %ebp, %esp
     1fa: popl  %ebp
     1fb: retl
```

---

## Call Sites & Xrefs (Global Cross-Reference Map)

There are **15 direct binary call sites** in `cachebeta.xbe` calling `0x00180b10`:

| Call Site VA | Enclosing Function | Enclosing VA | Role & Operation |
|---|---|---|---|
| `0x00139bdc` | `lights_queue_lens_flare` | `0x00139b40` | Encodes lens flare primary direction vector into lighting parameters (`params + 0x10`) |
| `0x00139be8` | `lights_queue_lens_flare` | `0x00139b40` | Encodes lens flare perpendicular vector into lighting parameters (`params + 0x14`) |
| `0x0013bbed` | `lights_preprocess_scene` | `0x0013b380` | Compresses direction vector for active dynamic light source |
| `0x0013bbf9` | `lights_preprocess_scene` | `0x0013b380` | Compresses up/perpendicular vector for active dynamic light source |
| `0x0013bc34` | `lights_preprocess_scene` | `0x0013b380` | Compresses second light orientation axis |
| `0x0013bc40` | `lights_preprocess_scene` | `0x0013b380` | Compresses third light orientation axis |
| `0x00180e1a` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex buffer normal vector compression |
| `0x00180e23` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex buffer binormal vector compression |
| `0x00180e2e` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex buffer tangent vector compression |
| `0x00180ef4` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex format alternative normal compression |
| `0x00180faa` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex format lightmap normal compression |
| `0x00180fb3` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex format lightmap binormal compression |
| `0x00180fbe` | `rasterizer_geometry_compress_vertices` | `0x00180d10` | Vertex format lightmap tangent compression |
| `0x00181a07` | `rasterizer_lens_flare_submit_for_cluster` | `0x00181900` | Cluster flare primary direction compression |
| `0x00181a13` | `rasterizer_lens_flare_submit_for_cluster` | `0x00181900` | Cluster flare perpendicular vector compression |

---

## Original Bungie Implementation Context

From the binary assertions, symbols, and line numbers, the authentic Bungie source is fully reconstructed:

```c
/* c:\halo\SOURCE\rasterizer\rasterizer_geometry.c */

uint32_t compress_real_vector3d_to_int32_clamp(const real_vector3d *v)
{
    real_vector3d v2;
    real ci, cj, ck;
    uint32_t i_11, j_11, k_10;
    uint32_t packed;

    if (!v) {
        display_assert("v", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 104, 1);
        system_exit(-1);
    }

    if (v->i < -1.0f) {
        ci = -1.0f;
    } else if (v->i > 1.0f) {
        ci = 1.0f;
    } else {
        ci = v->i;
    }
    i_11 = (uint32_t)(int)floor(ci * 1023.5f) & 0x7FF;

    if (v->j < -1.0f) {
        cj = -1.0f;
    } else if (v->j > 1.0f) {
        cj = 1.0f;
    } else {
        cj = v->j;
    }
    j_11 = (uint32_t)(int)floor(cj * 1023.5f) & 0x7FF;

    if (v->k < -1.0f) {
        ck = -1.0f;
    } else if (v->k > 1.0f) {
        ck = 1.0f;
    } else {
        ck = v->k;
    }
    k_10 = (uint32_t)(int)floor(ck * 511.5f) & 0x3FF;

    packed = (((k_10 << 11) | j_11) << 11) | i_11;

    uncompress_int32_to_real_vector3d(&v2, packed);

    if (fabs(v2.i - v->i) >= 0.01f) {
        display_assert("fabs(v2.i - v->i)<0.01f", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 118, 1);
        system_exit(-1);
    }
    if (fabs(v2.j - v->j) >= 0.01f) {
        display_assert("fabs(v2.j - v->j)<0.01f", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 119, 1);
        system_exit(-1);
    }
    if (fabs(v2.k - v->k) >= 0.01f) {
        display_assert("fabs(v2.k - v->k)<0.01f", "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 120, 1);
        system_exit(-1);
    }

    return packed;
}
```

---

## Proposed C Re-implementation

Updating [`src/halo/rasterizer/rasterizer_text.c:601`](file:///storage/1F34-EBBE/halo/src/halo/rasterizer/rasterizer_text.c#L601):

```c
/* rasterizer_geometry.c: clamp float[3] normal to [-1.0, 1.0] then pack to 11-11-10 uint.
 * layout: bits[10:0]=i, bits[21:11]=j, bits[31:22]=k (10-bit). (0x180b10) */
unsigned int compress_real_vector3d_to_int32_clamp(float *param_1)
{
  float ci;
  float cj;
  float ck;
  float *decoded;
  float decoded_i;
  float decoded_j;
  float decoded_k;
  unsigned int i_11;
  unsigned int j_11;
  unsigned int packed;
  int tmp;
  float local_buf[3];

  if (param_1 == 0) {
    display_assert("v",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x68,
                   1);
    system_exit(-1);
  }
  if (*param_1 < -1.0f) {
    ci = -1.0f;
  } else if (*param_1 > 1.0f) {
    ci = 1.0f;
  } else {
    ci = *param_1;
  }
  tmp = (int)floor((double)(ci * *(float *)0x2b0118));
  i_11 = (unsigned int)tmp & 0x7ff;

  if (param_1[1] < -1.0f) {
    cj = -1.0f;
  } else if (param_1[1] > 1.0f) {
    cj = 1.0f;
  } else {
    cj = param_1[1];
  }
  tmp = (int)floor((double)(cj * *(float *)0x2b0118));
  j_11 = (unsigned int)tmp & 0x7ff;

  if (param_1[2] < -1.0f) {
    ck = -1.0f;
  } else if (param_1[2] > 1.0f) {
    ck = 1.0f;
  } else {
    ck = param_1[2];
  }
  tmp = (int)floor((double)(ck * *(float *)0x2b0114));
  packed = (((unsigned int)tmp & 0x3ff) << 11 | j_11) << 11 | i_11;

  decoded = uncompress_int32_to_real_vector3d(local_buf, packed);
  decoded_i = decoded[0];
  decoded_j = decoded[1];
  decoded_k = decoded[2];

  if ((float)*(double *)0x28b800 <= fabsf(decoded_i - *param_1)) {
    display_assert("fabs(v2.i - v->i)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x76,
                   1);
    system_exit(-1);
  }
  if ((float)*(double *)0x28b800 <= fabsf(decoded_j - param_1[1])) {
    display_assert("fabs(v2.j - v->j)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x77,
                   1);
    system_exit(-1);
  }
  if ((float)*(double *)0x28b800 <= fabsf(decoded_k - param_1[2])) {
    display_assert("fabs(v2.k - v->k)<0.01f",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_geometry.c", 0x78,
                   1);
    system_exit(-1);
  }
  return packed;
}
```

---

## Reverse Thunk & Equivalence Strategy

- **Behavioral Equivalence:** Tested 100/100 behavioral equivalence in commit `db6d4799`.
- **VC71 Codegen Analysis:**
  - In commit `db6d4799`, VC71 match scored 35.8% because MSVC 7.1 scheduled x87 FPU stack loads, `fcomps`, and `floor` double-precision conversions through parameter stack slots in a way that modern clang emits with slightly different stack displacement ordering.
  - Per repo policy (`AGENTS.md` and `lift-score-improve`), functions with structural caps are either retained with `"ported": false` (if byte-match gating is strictly required for whole-file delinking) or activated via `"ported": true` when verified via behavioral equivalence.
  - Setting `"name": "compress_real_vector3d_to_int32_clamp"` in `kb.json` and replacing `FUN_00180b10` eliminates placeholder names and enables direct symbol calling without XCALL trampoline indirection.

---

## Verification Matrix

| Check | Expected Result | Status |
|---|---|---|
| **Pristine Reference Disassembly** | Matches `0x180b10_ref.obj` (508 bytes) | **VERIFIED** |
| **All 15 Call Sites Located** | Enclosing functions in `lights` & `rasterizer` mapped | **VERIFIED** |
| **Assert Strings & Line Numbers** | Matches `0x2afe38` (`rasterizer_geometry.c`, lines 104, 118, 119, 120) | **VERIFIED** |
| **ABI Register Audit** | Standard `__cdecl`, 1 param, 0 register args | **VERIFIED** |
| **Build & Link** | Clean compilation and link | **PENDING INTEGRATION** |

---

## Integration Plan

1. **Update `kb.json`:**
   - Set `"name": "compress_real_vector3d_to_int32_clamp"`
   - Set `"decl": "unsigned int compress_real_vector3d_to_int32_clamp(float *param_1);"`
2. **Update C Source:**
   - Rename `FUN_00180b10` to `compress_real_vector3d_to_int32_clamp` in:
     - `src/halo/rasterizer/rasterizer_text.c` (definition & call sites)
     - `src/halo/objects/objects.c` (replace `CALL_FUN_00180b10` macro and calls with direct named invocation)
   - Fix assertion string from `"parameters"` to `"v"` in `rasterizer_text.c:617`.
3. **Regenerate Declarations & Build:**
   - `rtk python3 tools/analysis/knowledge.py --gen-header build/generated/decl.h`
   - `cmake --build build --target halo`
4. **Commit & Push:**
   - Commit report and implementation updates to the PR branch.
