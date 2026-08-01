/*
 * rasterizer_xbox_vertex_shaders_runtime.c
 *
 * Runtime selection of the Xbox rasterizer's vertex shaders, lifted from
 * cachebeta.xbe.
 *
 * Source path (from the binary, every assert pushes the literal at 0x2ae628):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_vertex_shaders_runtime.c
 *
 * Globals (used by address, not in kb.json):
 *   0x325200  vertex_shader_entry[0x43] - vertex_shader_table.  Same table
 *             built by rasterizer_vertex_shaders_initialize; this function
 *             reads +0x8 (handle, `mov (%edx,0x325208),%eax` with the index
 *             already shifted left 4) and +0xc.
 *
 *   0x325630  short - index of the vertex shader currently selected on the
 *             device.  Written unconditionally at the tail; an early return
 *             skips the write only because the value already matches.
 *
 *   0x325634  packed_vertex_shader[3] - the three shaders that are kept
 *             resident in GPU vertex-shader memory.  Stride 8 is proven by
 *             `add $0x8,%edi` in the measure loop and by `shl $0x3` on the
 *             loop index in the search loop.
 *
 *   0x3256ba  short - rasterizer debug/render-mode selector (same word tested
 *             in rasterizer.c; here any non-zero value enables the counter).
 *
 *   0x5a5558  long  - accumulated vertex-shader instruction counter.
 *
 * Notes on the shape of this function:
 *
 *   - The three parameters occupy 4-byte cdecl slots and are loaded as
 *     dwords (`mov 0x8(%ebp),%ebx`) but every comparison is 16-bit
 *     (`test %bx,%bx`, `cmp $0x43,%bx`, `mov 0xc(%ebp),%ax`), and the
 *     write-back at 0x178d99 is a dword store into the first slot.  That is
 *     an `int` parameter used through `(short)` casts, so kb.json keeps the
 *     (int, int, int) prototype its 22 call sites already use.
 *
 *   - `vertex_shader_index` is reassigned from the translation table at
 *     0x178d99 and every later use of slot [EBP+8] is that translated index,
 *     not the caller's argument.
 *
 *   - The "did we measure the packed shader sizes yet" test at 0x178da2 reads
 *     the dword at 0x325640, which is packed_shaders[1].offset (0x325634 +
 *     1*8 + 4).  It is the same storage the measure loop fills, not a
 *     separate flag.
 *
 *   - The size out-parameter of IDirect3DDevice8_GetVertexShaderSize is
 *     `lea 0xc(%ebp),%ecx`, i.e. MSVC coloured the local into the dead
 *     `vertex_type` parameter slot, and `mov 0xc(%ebp),%edx ; add %edx,%esi`
 *     afterwards accumulates that SIZE (not vertex_type).  We use a real
 *     local, which costs one extra frame dword versus the original.
 *
 *   - Both assert tails are `push $-1 ; call 0x0008e2f0` = system_exit(-1),
 *     not halt_and_catch_fire.
 *
 *   - `success` is seeded 1 at 0x178b4f and lives in [EBP-1] for the first
 *     half of the function and in %bl for the second; the final `test %bl,%bl`
 *     gates the one error line.  Every D3D entry point called here is void,
 *     so the `if (success)` re-tests never fail at runtime, but they are the
 *     literal shape of the original call macro.
 *
 *   - The name is binary-proven by the error string at 0x29dffc,
 *     "### ERROR rasterizer_set_vertex_shader failed".
 */

/*
 * One entry of vertex_shader_table.  Mirrors the definition in
 * rasterizer_xbox_vertex_shaders_initialize.c; +0xc is untouched there and is
 * only read here, so it keeps its mechanical name.
 */
typedef struct vertex_shader_entry {
  unsigned long *declaration; /* +0x00 D3D vertex declaration blob    */
  unsigned long *code; /* +0x04 compiled shader microcode      */
  unsigned long handle; /* +0x08 D3D vertex shader handle       */
  long field_c; /* +0x0c added to the 0x5a5558 counter  */
} vertex_shader_entry;

/*
 * One of the three shaders kept resident in GPU vertex-shader memory.
 * +0x00/+0x02/+0x04 are proven by `movswl -0x4(%edi)`, `movb 0x325636(%esi)`
 * and `mov 0x325638(%esi),%edx` against the stride-8 walk.
 */
typedef struct packed_vertex_shader {
  short vertex_shader_index; /* +0x00 index into vertex_shader_table */
  bool loaded; /* +0x02 microcode already uploaded     */
  unsigned char pad_03; /* +0x03                                */
  unsigned long offset; /* +0x04 byte offset in shader memory   */
} packed_vertex_shader;

#define NUMBER_OF_VERTEX_SHADERS 0x43
#define NUMBER_OF_RASTERIZER_VERTEX_TYPES 0xc
#define NUMBER_OF_PACKED_VERTEX_SHADERS 3
#define PACKED_VERTEX_SHADER_MEMORY_SIZE 0x88
#define VERTEX_SHADER_NONE ((short)-1)

#define vertex_shader_table ((vertex_shader_entry *)0x00325200)
#define packed_shaders ((packed_vertex_shader *)0x00325634)
#define current_vertex_shader (*(short *)0x00325630)
#define rasterizer_debug_mode (*(short *)0x003256ba)
#define vertex_shader_instruction_counter (*(long *)0x005a5558)

/*
 * Per-shader translation tables in .rdata, indexed by
 * vertex_type * permutation_count + permutation_index.  They are referenced
 * only as addresses; the entries are shorts and -1 means "no such shader".
 */
#define VERTEX_SHADER_TRANSLATION_TABLE(address) ((const short *)(address))

#define RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE                     \
  "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_vertex_shaders_" \
  "runtime.c"

/*
 * Selects the vertex shader for (vertex_shader_index, vertex_type,
 * permutation_index).  The three shaders in packed_shaders[] live in a single
 * 0x88-byte GPU vertex-shader memory window and are loaded on demand; every
 * other shader is set with IDirect3DDevice8_SetVertexShader, which evicts the
 * packed set.
 */
void FUN_00178b40(int vertex_shader_index, int vertex_type,
                  int permutation_index)
{
  const short *translation_table;
  short permutation_count;
  short packed_shader_index;
  unsigned long handle;
  unsigned long offset;
  uint32_t size;
  int index;
  bool success;

  success = true;
  translation_table = 0;
  permutation_count = 1;

  if ((short)vertex_shader_index < 0 ||
      (short)vertex_shader_index >= NUMBER_OF_VERTEX_SHADERS) {
    display_assert(
      "vertex_shader_index>=0 && vertex_shader_index<NUMBER_OF_VERTEX_SHADERS",
      RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x87, 1);
    system_exit(-1);
  }

  switch ((short)vertex_shader_index) {
  case 0:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002addc8);
    break;
  case 1:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adde0);
    break;
  case 4:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002addf8);
    permutation_count = 2;
    break;
  case 0x26:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ade28);
    break;
  case 0x41:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ade40);
    permutation_count = 4;
    break;
  case 0x38:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adea0);
    break;
  case 0x10:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adeb8);
    break;
  case 0x31:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002aded0);
    break;
  case 0x1d:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adee8);
    break;
  case 0x28:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adf00);
    break;
  case 0x15:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adf18);
    permutation_count = 3;
    break;
  case 0x3a:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adf60);
    break;
  case 0x2a:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adf78);
    permutation_count = 3;
    break;
  case 0x33:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adfc0);
    break;
  case 6:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adfd8);
    break;
  case 8:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002adff0);
    break;
  case 0x25:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae008);
    permutation_count = 6;
    break;
  case 0xa:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae098);
    permutation_count = 4;
    break;
  case 0x40:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae0f8);
    break;
  case 0x27:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae110);
    break;
  case 0xd:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae128);
    break;
  case 5:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae140);
    break;
  case 0x18:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae158);
    permutation_count = 6;
    break;
  case 0x14:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae1e8);
    break;
  case 0x17:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae200);
    break;
  case 0x2e:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae218);
    break;
  case 0x2b:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae230);
    permutation_count = 3;
    break;
  case 0x19:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae278);
    break;
  case 0x16:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae290);
    break;
  case 0xf:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae2a8);
    break;
  case 0x21:
    translation_table = VERTEX_SHADER_TRANSLATION_TABLE(0x002ae2c0);
    permutation_count = 2;
    break;
  default:
    display_assert("### ERROR unsupported vertex shader",
                   RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x36c, 1);
    system_exit(-1);
  }

  if ((short)vertex_type < 0 ||
      (short)vertex_type >= NUMBER_OF_RASTERIZER_VERTEX_TYPES) {
    display_assert(
      "vertex_type>=0 && vertex_type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
      RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x36f, 1);
    system_exit(-1);
  }

  if ((short)permutation_index < 0 ||
      (short)permutation_index >= permutation_count) {
    display_assert(
      "permutation_index>=0 && permutation_index<permutation_count",
      RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x370, 1);
    system_exit(-1);
  }

  index = permutation_count * (short)vertex_type + (short)permutation_index;
  if (translation_table[index] == VERTEX_SHADER_NONE) {
    display_assert("translation_table[vertex_type*permutation_count + "
                   "permutation_index]!=NONE",
                   RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x371, 1);
    system_exit(-1);
  }

  /* from here on the parameter slot holds the TRANSLATED shader index */
  vertex_shader_index = translation_table[index];
  if ((short)vertex_shader_index == current_vertex_shader) {
    return;
  }

  /*
   * First use: measure every packed shader and lay them out back to back in
   * GPU vertex-shader memory.  packed_shaders[1].offset stays zero until this
   * has run, which is what the guard reads.
   */
  if (packed_shaders[1].offset == 0) {
    offset = 0;
    for (packed_shader_index = 0;
         packed_shader_index < NUMBER_OF_PACKED_VERTEX_SHADERS;
         packed_shader_index++) {
      D3DDevice_GetVertexShaderSize(
        vertex_shader_table[packed_shaders[packed_shader_index]
                              .vertex_shader_index]
          .handle,
        &size);
      if (success) {
        success = true;
      } else {
        success = false;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_GetVertexShaderSize(global_d3d_device,"
                     " vertex_shader_table[packed_shaders[packed_shader_index]"
                     ".vertex_shader_index].handle, &size)");
      }
      packed_shaders[packed_shader_index].offset = offset;
      offset += size;
    }

    if (offset > PACKED_VERTEX_SHADER_MEMORY_SIZE) {
      display_assert("### ERROR packed vertex shaders don't fit in GPU memory",
                     RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x3a2, 1);
      system_exit(-1);
    }
  }

  for (packed_shader_index = 0;
       packed_shader_index < NUMBER_OF_PACKED_VERTEX_SHADERS;
       packed_shader_index++) {
    if ((short)vertex_shader_index ==
        packed_shaders[packed_shader_index].vertex_shader_index) {
      break;
    }
  }

  if (packed_shader_index < NUMBER_OF_PACKED_VERTEX_SHADERS) {
    if (packed_shaders[packed_shader_index].loaded) {
      D3DDevice_SelectVertexShader(0,
                                   packed_shaders[packed_shader_index].offset);
      if (success) {
        success = true;
      } else {
        success = false;
        FUN_00167ff0(0, "IDirect3DDevice8_SelectVertexShader(global_d3d_device,"
                        " 0L, (UINT)offset)");
      }
      /* already resident: the instruction counter is not bumped again */
      goto set_current_vertex_shader;
    }

    offset = packed_shaders[packed_shader_index].offset;
    handle = vertex_shader_table[packed_shaders[packed_shader_index]
                                   .vertex_shader_index]
               .handle;
    if (handle == 0) {
      display_assert("### ERROR vertex shader was not valid",
                     RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x3bc, 1);
      system_exit(-1);
    }

    D3DDevice_LoadVertexShader(handle, offset);
    if (success) {
      success = true;
    } else {
      success = false;
      FUN_00167ff0(0, "IDirect3DDevice8_LoadVertexShader(global_d3d_device, "
                      "(DWORD)handle, (UINT)offset)");
    }

    D3DDevice_SelectVertexShader(handle, offset);
    if (success) {
      success = true;
    } else {
      success = false;
      FUN_00167ff0(0, "IDirect3DDevice8_SelectVertexShader(global_d3d_device, "
                      "(DWORD)handle, (UINT)offset)");
    }
    packed_shaders[packed_shader_index].loaded = true;
  } else {
    handle = vertex_shader_table[(short)vertex_shader_index].handle;
    if (handle == 0) {
      display_assert("### ERROR vertex shader was not valid",
                     RASTERIZER_XBOX_VERTEX_SHADERS_RUNTIME_FILE, 0x3c9, 1);
      system_exit(-1);
    }

    D3DDevice_SetVertexShader(handle);
    if (success) {
      success = true;
    } else {
      success = false;
      FUN_00167ff0(
        0,
        "IDirect3DDevice8_SetVertexShader(global_d3d_device, (DWORD)handle)");
    }

    /* setting a non-packed shader evicts everything in the packed window */
    for (packed_shader_index = 0;
         packed_shader_index < NUMBER_OF_PACKED_VERTEX_SHADERS;
         packed_shader_index++) {
      packed_shaders[packed_shader_index].loaded = false;
    }
  }

  if (rasterizer_debug_mode != 0) {
    vertex_shader_instruction_counter +=
      vertex_shader_table[(short)vertex_shader_index].field_c;
  }

set_current_vertex_shader:
  current_vertex_shader = (short)vertex_shader_index;
  if (!success) {
    error(2, "### ERROR rasterizer_set_vertex_shader failed");
  }
}
