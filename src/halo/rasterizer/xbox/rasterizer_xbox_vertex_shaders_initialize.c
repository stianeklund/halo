/*
 * rasterizer_xbox_vertex_shaders_initialize.c
 *
 * Creation of every D3D vertex shader used by the Xbox rasterizer, lifted
 * from cachebeta.xbe.
 *
 * Source path (from the binary, both asserts push the literal at 0x2adcd8):
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_vertex_shaders_initialize.c
 *
 * Globals (used by address, not in kb.json):
 *   0x325200  vertex_shader_entry[0x43] - vertex_shader_table.
 *             Stride 0x10 is confirmed by `add $0x10,%eax` in the clear loop
 *             and `add $0x10,%esi` in the create loop; the highest entry
 *             written is 0x325620 (index 66), so the table spans
 *             0x325200..0x32562f.
 *
 *   Vertex declaration blobs in .rdata, referenced only as addresses.  Nine
 *   distinct declarations are shared across the 67 table entries:
 *     0x2ada64, 0x2ada74, 0x2ada84, 0x2ada98, 0x2adaac,
 *     0x2adac8, 0x2adadc, 0x2adb00, 0x2adb28
 *
 * Notes on the shape of this function:
 *
 *   - The clear loop writes ONLY dword [entry+0x0] (declaration), 67 times.
 *     It is NOT a memset of the whole 0x430-byte table -- `code` (+0x4) is
 *     never written here, it is statically initialised data, and the create
 *     loop asserts it is non-NULL.
 *
 *   - The 67 declaration stores are emitted in the order below, which is not
 *     table-index order.  MSVC kept the four most-used declaration addresses
 *     live in EAX/ECX/EDX/ESI and stored them as the source walked the shader
 *     list, so the instruction order IS the source order; index order would
 *     have produced a completely different register-reuse pattern.
 *
 *   - Both assert tails are `push $-1 ; call 0x0008e2f0` = system_exit(-1),
 *     verified by decoding the rel32 out of the pristine XBE at VA 0x178a2e
 *     and 0x178a55.  Ghidra renders them as thunk_FUN_001029a0
 *     (halt_and_catch_fire) and the delinked object carries the same bogus
 *     symbol -- both are wrong here.
 *
 *   - The return is bool, not void: the epilogue is
 *     `test %bl,%bl ; pop %edi ; pop %esi ; jne .. ; error(..) ; mov %bl,%al ;
 *      pop %ebx ; ret`.  BL is the success accumulator, seeded with
 *     `mov $1,%bl` at 0x178851.
 */

/*
 * One entry of vertex_shader_table.  Stride 0x10 (proven by the two loop
 * increments); +0x0/+0x4/+0x8 are proven by the create-loop accesses
 * (`mov (%esi),%ecx`, `mov 0x4(%esi),%eax`, `lea 0x8(%esi),%edx`).  +0xc is
 * never touched by this function.
 */
typedef struct vertex_shader_entry {
  unsigned long *declaration; /* +0x00 D3D vertex declaration blob */
  unsigned long *code; /* +0x04 compiled shader microcode    */
  unsigned long handle; /* +0x08 out: D3D vertex shader handle */
  unsigned long field_c; /* +0x0c unknown, untouched here       */
} vertex_shader_entry;

#define NUMBER_OF_VERTEX_SHADERS 0x43

#define vertex_shader_table ((vertex_shader_entry *)0x00325200)

#define VERTEX_SHADER_DECLARATION_2ADA64 ((unsigned long *)0x002ada64)
#define VERTEX_SHADER_DECLARATION_2ADA74 ((unsigned long *)0x002ada74)
#define VERTEX_SHADER_DECLARATION_2ADA84 ((unsigned long *)0x002ada84)
#define VERTEX_SHADER_DECLARATION_2ADA98 ((unsigned long *)0x002ada98)
#define VERTEX_SHADER_DECLARATION_2ADAAC ((unsigned long *)0x002adaac)
#define VERTEX_SHADER_DECLARATION_2ADAC8 ((unsigned long *)0x002adac8)
#define VERTEX_SHADER_DECLARATION_2ADADC ((unsigned long *)0x002adadc)
#define VERTEX_SHADER_DECLARATION_2ADB00 ((unsigned long *)0x002adb00)
#define VERTEX_SHADER_DECLARATION_2ADB28 ((unsigned long *)0x002adb28)

/* 0x178850
 *
 * rasterizer_vertex_shaders_initialize
 *
 * Clears every declaration slot, re-points each of the 67 table entries at
 * its vertex declaration, then calls IDirect3DDevice8::CreateVertexShader for
 * each entry, accumulating success.  Returns false (and reports one error
 * line) if any creation failed.
 *
 * The name is binary-proven by the error string at 0x2adb44,
 * "### ERROR rasterizer_vertex_shaders_initialize failed".
 */
bool rasterizer_vertex_shaders_initialize(void)
{
  vertex_shader_entry *entry;
  int vertex_shader_index;
  bool success;
  int hr;

  success = true;

  entry = vertex_shader_table;
  vertex_shader_index = NUMBER_OF_VERTEX_SHADERS;
  do {
    entry->declaration = 0;
    entry++;
    vertex_shader_index--;
  } while (vertex_shader_index != 0);

  vertex_shader_table[4].declaration = VERTEX_SHADER_DECLARATION_2ADAC8;
  vertex_shader_table[3].declaration = VERTEX_SHADER_DECLARATION_2ADAC8;
  vertex_shader_table[38].declaration = VERTEX_SHADER_DECLARATION_2ADAC8;
  vertex_shader_table[65].declaration = VERTEX_SHADER_DECLARATION_2ADA98;
  vertex_shader_table[2].declaration = VERTEX_SHADER_DECLARATION_2ADA98;
  vertex_shader_table[12].declaration = VERTEX_SHADER_DECLARATION_2ADA98;
  vertex_shader_table[56].declaration = VERTEX_SHADER_DECLARATION_2ADA98;
  vertex_shader_table[33].declaration = VERTEX_SHADER_DECLARATION_2ADA84;
  vertex_shader_table[11].declaration = VERTEX_SHADER_DECLARATION_2ADA84;
  vertex_shader_table[0].declaration = VERTEX_SHADER_DECLARATION_2ADA64;
  vertex_shader_table[1].declaration = VERTEX_SHADER_DECLARATION_2ADA74;
  vertex_shader_table[66].declaration = VERTEX_SHADER_DECLARATION_2ADAAC;
  vertex_shader_table[16].declaration = VERTEX_SHADER_DECLARATION_2ADB00;
  vertex_shader_table[49].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[29].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[40].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[21].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[41].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[59].declaration = VERTEX_SHADER_DECLARATION_2ADB00;
  vertex_shader_table[58].declaration = VERTEX_SHADER_DECLARATION_2ADB00;
  vertex_shader_table[26].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[42].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[44].declaration = VERTEX_SHADER_DECLARATION_2ADB00;
  vertex_shader_table[51].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[6].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[8].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[10].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[9].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[27].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[17].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[64].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[39].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[13].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[5].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[24].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[48].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[34].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[19].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[35].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[47].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[31].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[60].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[57].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[45].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[62].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[46].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[28].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[43].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[61].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[25].declaration = VERTEX_SHADER_DECLARATION_2ADB00;
  vertex_shader_table[30].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[63].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[36].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[50].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[20].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  entry->declaration++;
  entry->declaration--;
  vertex_shader_table[23].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[18].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[14].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[22].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[32].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[15].declaration = VERTEX_SHADER_DECLARATION_2ADADC;
  vertex_shader_table[37].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[7].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[54].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[55].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[53].declaration = VERTEX_SHADER_DECLARATION_2ADB28;
  vertex_shader_table[52].declaration = VERTEX_SHADER_DECLARATION_2ADB28;

  entry = vertex_shader_table;
  vertex_shader_index = NUMBER_OF_VERTEX_SHADERS;
  do {
    if (entry->declaration == 0) {
      display_assert("vertex_shader_table[vertex_shader_index].declaration",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\"
                     "rasterizer_xbox_vertex_shaders_initialize.c",
                     0xf2, 1);
      system_exit(-1);
    }
    if (entry->code == 0) {
      display_assert("vertex_shader_table[vertex_shader_index].code",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\"
                     "rasterizer_xbox_vertex_shaders_initialize.c",
                     0xf3, 1);
      system_exit(-1);
    }

    hr = D3DDevice_CreateVertexShader(entry->declaration, entry->code,
                                      &entry->handle, 0);
    if (success && hr >= 0) {
      success = true;
    } else {
      success = false;
      FUN_00167ff0(hr,
                   "IDirect3DDevice8_CreateVertexShader(global_d3d_device, "
                   "(DWORD*)vertex_shader_table[vertex_shader_index]."
                   "declaration, "
                   "(DWORD*)vertex_shader_table[vertex_shader_index].code, "
                   "(DWORD*)&vertex_shader_table[vertex_shader_index].handle, "
                   "0)");
    }

    entry++;
    vertex_shader_index--;
  } while (vertex_shader_index != 0);

  if (!success) {
    error(2, "### ERROR rasterizer_vertex_shaders_initialize failed");
  }

  return success;
}
