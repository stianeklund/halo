/* Shader tag base.
 *
 * The assert at c:\halo\SOURCE\shaders\shader_definitions.c:0x85d inside
 * FUN_001906b0 stringizes "shader->base.type==shader_type", which names the
 * member (`base`) and the field (`type`); the disassembly proves the offset and
 * width (MOV AX, word ptr [ESI+0x24]). Nothing past +0x24 is proven from this
 * function, so nothing else is declared here. */
typedef struct shader_base_definition {
  /* Bit 2 (0x4) is tested by shader_get_vertex_shader_permutation
   * (TEST byte ptr [EDI],0x4 at 0x1907f6); nothing else about this byte is
   * proven, so it keeps a mechanical name. */
  uint8_t field_00;
  uint8_t pad_01[0x23];
  int16_t type;
} shader_base_definition;

typedef struct shader_definition {
  shader_base_definition base;
} shader_definition;

#define SHADER_DEFINITIONS_FILE \
  "c:\\halo\\SOURCE\\shaders\\shader_definitions.c"

/* 0x190550 — resolve the sky index (and the indoor-fog answer) for a BSP
 * location, then hand the result to FUN_00190240.
 *
 * The base pointer comes from 0x18e3c0 (kb: scenario_get). The three tag blocks
 * read here have exactly the offsets and strides that scenario.c's fog helpers
 * read off `global_structure_bsp`: clusters at +0x134 (stride 0x68), BSP3D
 * nodes at +0x184 (stride 0x28), and a +0x190 block (stride 0x88) whose +0x2c
 * dword is a 'fog ' tag index. Those layouts are unrecovered, so the field
 * reads stay raw: location+0x4 is the int16 cluster index, cluster+0x8 the
 * fallback sky index, node+0x24 an int16 reference index, node+0x26 the int16
 * sky index.
 *
 * flags bit 2 (0x4) suppresses the position argument to FUN_0018f2d0 and also
 * gates the non-indoor sky override; bit 3 (0x8) suppresses the indoor answer.
 * Confirmed: TEST CL,1 on the fog tag's first byte — the same indoor flag
 * scenario.c's FUN_0018f3e0 reads.
 *
 * The `(x != 0) - 1` mask at 0x190583 (MOV EAX,0 / SETNZ AL / DEC EAX / AND
 * EAX,EDX) is a branchless select of `position` or NULL; it is written as the
 * same mask expression rather than an if/else to keep the codegen.
 */
bool FUN_00190550(void *location, void *position, int32_t param_3,
                  uint32_t flags)
{
  char *bsp;
  int16_t node_index;
  char *cluster_element;
  char *node_element;
  char *reference_element;
  int16_t reference_index;
  int fog_tag_index;
  char *fog_tag;
  int32_t suppress_position;
  int16_t sky_index;
  bool is_indoor;

  is_indoor = false;
  sky_index = NONE;
  if (*(int16_t *)((char *)location + 4) != NONE) {
    bsp = (char *)scenario_get();
    suppress_position = flags & 4;
    node_index = FUN_0018f2d0(
      location, (void *)(((suppress_position != 0) - 1) & (int32_t)position));
    cluster_element = (char *)tag_block_get_element(
      bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);
    sky_index = *(int16_t *)(cluster_element + 8);
    if (node_index != NONE) {
      node_element =
        (char *)tag_block_get_element(bsp + 0x184, (int)node_index, 0x28);
      reference_index = *(int16_t *)(node_element + 0x24);
      if (reference_index != NONE &&
          *(int16_t *)(node_element + 0x26) != NONE) {
        reference_element = (char *)tag_block_get_element(
          bsp + 0x190, (int)reference_index, 0x88);
        fog_tag_index = *(int32_t *)(reference_element + 0x2c);
        if (fog_tag_index != NONE) {
          fog_tag = (char *)tag_get(0x666f6720, fog_tag_index);
          if (*(uint8_t *)fog_tag & 1) {
            if ((flags & 8) == 0) {
              sky_index = *(int16_t *)(node_element + 0x26);
              is_indoor = true;
            }
          } else if (suppress_position == 0) {
            sky_index = *(int16_t *)(node_element + 0x26);
          }
        }
      }
    }
  }
  FUN_00190240(position, param_3, flags, sky_index);
  return is_indoor;
}

/* 0x190670 — the "ignore indoor fog" entry point for the 0x190550 sky lookup:
 * forwards all four arguments and ORs bit 3 (0x8) into `flags`.
 *
 * The whole function is 15 instructions (0x190670-0x19068f): PUSH EBP / MOV
 * EBP,ESP, no locals and no SUB ESP, then MOV EAX,[EBP+0x14] / OR EAX,0x8 as
 * the only logic, four right-to-left pushes, CALL 0x190550, ADD ESP,0x10 (which
 * is what proves four cdecl stack arguments), POP EBP / RET. Argument types are
 * taken from FUN_00190550's recovered signature — the wrapper only moves them
 * through GPRs and dereferences nothing, so it carries no type evidence itself.
 *
 * Declared void even though FUN_00190550 returns bool and the wrapper leaves AL
 * untouched across its epilogue: with bit 3 forced on, 0x190550's `is_indoor`
 * can never become true (its only assignment sits behind `(flags & 8) == 0`),
 * so the value reaching the caller is the constant false. Both spellings emit
 * identical bytes, so the binary does not arbitrate; the constant return is why
 * void is the narrower reading. No XBE caller exists to settle it.
 */
void FUN_00190670(void *location, void *position, int32_t param_3,
                  uint32_t flags)
{
  FUN_00190550(location, position, param_3, flags | 8);
}

/* 0x190690 — the "ignore position" entry point for the 0x190550 sky lookup:
 * forwards all four arguments and ORs bit 2 (0x4) into `flags`.
 *
 * Byte-identical to FUN_00190670 apart from the OR immediate and the CALL
 * displacement (0x190670 is `83 c8 08`, 0x190690 is `83 c8 04`); both spans are
 * 32 bytes. PUSH EBP / MOV EBP,ESP, no locals and no SUB ESP, then
 * MOV EAX,[EBP+0x14] / OR EAX,0x4 as the only logic, four right-to-left pushes,
 * CALL 0x190550, ADD ESP,0x10 (which is what proves four cdecl stack
 * arguments), POP EBP / RET. Argument types are taken from FUN_00190550's
 * recovered signature — the wrapper only moves them through GPRs and
 * dereferences nothing, so it carries no type evidence of its own.
 *
 * Inside 0x190550 bit 2 is `suppress_position`: it NULLs the position argument
 * to FUN_0018f2d0 and closes the non-indoor sky override. Unlike bit 3 (see
 * FUN_00190670, which forces the constant false), bit 2 leaves the
 * `(flags & 8) == 0` indoor path open, so `is_indoor` stays reachable and the
 * value EAX carries across this wrapper's epilogue is genuinely variable —
 * which is why this one is spelled bool and 0x190670 is spelled void. The
 * binary itself does not arbitrate: nothing touches EAX between the CALL and
 * the RET, so both spellings emit identical bytes, and a scan of every E8
 * rel32 in the image finds no direct caller of 0x190690 to settle it.
 */
bool FUN_00190690(void *location, void *position, int32_t param_3,
                  uint32_t flags)
{
  return FUN_00190550(location, position, param_3, flags | 4);
}

/* 0x1906b0 — assert that `shader` is a non-NULL shader tag of the expected
 * type, then hand it straight back. Callers use it as a checked downcast (see
 * rasterizer.c, which adds the per-shader-type field offset to the result). */
void *FUN_001906b0(void *shader_pointer, int16_t shader_type)
{
  shader_definition *shader = (shader_definition *)shader_pointer;

  assert_halt_at(SHADER_DEFINITIONS_FILE, 0x85c, shader);
  /* Original assert text has no spaces around `==`; see assert_halt_msg_at. */
  assert_halt_msg_at("shader->base.type==shader_type", SHADER_DEFINITIONS_FILE,
                     0x85d, shader->base.type == shader_type);
  return shader;
}

#define SHADERS_FILE "c:\\halo\\SOURCE\\shaders\\shaders.c"

/* 0x190710 — pick the vertex-shader permutation index for a shader tag.
 *
 * Only four of the shader types contribute a non-zero permutation; every other
 * type (and the -1 sentinel) yields 0.  The per-type field offsets below are
 * raw because each switch arm looks at a *different* shader definition struct
 * (FUN_001906b0 is a checked downcast) and none of those layouts is recovered
 * yet.  Field widths are taken from the disassembly: +0x58 is a dword compared
 * against -1, +0x5c and +0x2a are words, +0x29 is a byte tested against 0x8.
 *
 * The two FUN_001906b0 calls per arm are genuine — the original re-casts the
 * shader instead of caching the result. */
int shader_get_vertex_shader_permutation(void *shader_pointer)
{
  shader_definition *shader = (shader_definition *)shader_pointer;
  char *data;
  int16_t permutation;

  assert_halt_at(SHADERS_FILE, 20, shader);

  if (shader == (shader_definition *)0xffffffff) {
    permutation = 0;
  } else {
    switch (shader->base.type) {
    /* Arm order follows the original's code layout (the type-4 arm precedes the
     * type-1 arm at 0x190755), which MSVC emits in source order. */
    case 4:
      data = (char *)FUN_001906b0(shader, 4);
      /* FLD [EAX+0x38]; FCOMP 0.0f; TEST AH,0x41; JNZ -> strictly greater. */
      if (*(float *)(data + 0x38) > 0.0f)
        permutation = 1;
      else
        permutation = 0;
      break;
    case 1:
      data = (char *)FUN_001906b0(shader, 1);
      if (*(int32_t *)(data + 0x58) != -1) {
        data = (char *)FUN_001906b0(shader, 1);
        permutation = (int16_t)(*(int16_t *)(data + 0x5c) + 1);
      } else {
        permutation = 0;
      }
      break;
    case 5:
      data = (char *)FUN_001906b0(shader, 5);
      permutation = (int16_t)(*(int16_t *)(data + 0x2a) + 1);
      if (permutation == 1) {
        data = (char *)FUN_001906b0(shader, 5);
        if ((*(uint8_t *)(data + 0x29) & 8) == 0)
          permutation = 0;
      }
      if ((shader->base.field_00 & 4) != 0)
        permutation = 5;
      break;
    case 6:
      data = (char *)FUN_001906b0(shader, 6);
      permutation = (int16_t)(*(int16_t *)(data + 0x2a) + 1);
      if (permutation == 1) {
        data = (char *)FUN_001906b0(shader, 6);
        if ((*(uint8_t *)(data + 0x29) & 8) == 0)
          permutation = 0;
      }
      if ((shader->base.field_00 & 4) != 0)
        permutation = 5;
      break;
    default:
      permutation = 0;
      break;
    }
  }
  return permutation;
}

/* 0x190830 — does this shader tag describe a mirror?
 *
 * Only two shader types can answer yes, and they answer from different fields:
 *   type 3:  MOV AL,byte ptr [EAX+0x2d0]; AND AL,1   (bit 0 of a flags byte)
 *   type 8:  CMP word ptr [EAX+0x8a],2; SETZ AL      (a signed 16-bit enum ==
 * 2) Every other type, and NULL, answers 0.
 *
 * The offsets stay raw for the same reason as in shader_is_decal: each arm
 * looks at a *different* shader definition struct behind the FUN_001906b0
 * checked downcast, and neither of those layouts is recovered.  Widths come
 * from the disassembly: +0x2d0 is a byte (MOV AL, byte ptr) and +0x8a is a word
 * (CMP word ptr) — using `int` for either would be a field-width bug.
 *
 * Two arms again, but the case values are not adjacent, so MSVC dispatched with
 * a compare chain off one signed 16-bit load (MOVSX EDX, word ptr [ECX+0x24];
 * CMP EDX,3; JZ; CMP EDX,8; JNZ) rather than the subtract chain it built for
 * shader_is_water_decal or the jump table it built for shader_is_decal.
 *
 * As in shader_is_water_decal, the arms accumulate into a byte the prologue
 * clears (XOR AL,AL at 0x190836, i.e. before the NULL test), so the NULL test
 * and the default arm share an epilogue.  MSVC placed the type-8 block first
 * (inline at 0x19084a) and gave it its own POP EBP/RET at 0x19085f, while the
 * out-of-line type-3 block at 0x190861 falls into the shared epilogue at
 * 0x190874 — block placement and tail duplication, not two different return
 * paths in the source.
 *
 * The frame is PUSH EBP/MOV EBP,ESP with no `sub esp`: no spilled locals. */
char shader_is_mirror(void *shader_pointer)
{
  shader_definition *shader = (shader_definition *)shader_pointer;
  unsigned char mirror;
  char *data;

  mirror = 0;
  if (shader != (shader_definition *)0) {
    /* MOVSX EDX, word ptr [ECX+0x24] -- the selector is a signed 16-bit field.
     */
    switch (shader->base.type) {
    case 3:
      data = (char *)FUN_001906b0(shader, 3);
      mirror = *(uint8_t *)(data + 0x2d0);
      mirror &= 1;
      break;
    case 8:
      data = (char *)FUN_001906b0(shader, 8);
      mirror = (unsigned char)(*(int16_t *)(data + 0x8a) == 2);
      break;
    }
  }
  return (char)mirror;
}

/* 0x1908a0 — does this shader tag carry the "decal" flag?
 *
 * Only four shader types have the flag; every other type (and NULL) answers 0.
 * Selector 7 is present in the jump table at 0x190910 but routes to the default
 * arm, so it is not spelled out here.
 *
 * The offsets are raw for the same reason as in
 * shader_get_vertex_shader_permutation: each arm looks at a *different* shader
 * definition struct behind the FUN_001906b0 checked downcast, and none of those
 * layouts is recovered.  Widths and bit positions are read off the disassembly:
 *   types 5 and 6:  MOV AL,[EAX+0x29]; SHR AL,1; AND AL,1
 *   type 8:         MOV AL,[EAX+0x28]; SHR AL,1; AND AL,1
 *   type 9:         MOV AL,[EAX+0x28]; AND AL,1      (no shift)
 * Types 8 and 9 read the same byte but different bits -- do not merge them.
 *
 * Every arm stays in 8-bit arithmetic (MOVB/SHRB/ANDB, never MOVZX + 32-bit
 * ops), which is what a `: 1` bitfield in a one-byte flags word compiles to;
 * the shift count is the bit index.  The result is accumulated into a byte that
 * the prologue clears (XOR AL,AL at 0x1908a5) rather than returned from each
 * arm, so the NULL test and the default arm share the type-9 epilogue. */
char shader_is_decal(void *shader_pointer)
{
  shader_definition *shader = (shader_definition *)shader_pointer;
  unsigned char decal;
  char *data;

  decal = 0;
  if (shader != (shader_definition *)0) {
    /* MOVSX EDX, word ptr [ECX+0x24] -- the selector is a signed 16-bit field.
     */
    switch (shader->base.type) {
    case 5:
      data = (char *)FUN_001906b0(shader, 5);
      decal = *(uint8_t *)(data + 0x29);
      decal >>= 1;
      decal &= 1;
      break;
    case 6:
      data = (char *)FUN_001906b0(shader, 6);
      decal = *(uint8_t *)(data + 0x29);
      decal >>= 1;
      decal &= 1;
      break;
    case 8:
      data = (char *)FUN_001906b0(shader, 8);
      decal = *(uint8_t *)(data + 0x28);
      decal >>= 1;
      decal &= 1;
      break;
    case 9:
      data = (char *)FUN_001906b0(shader, 9);
      decal = *(uint8_t *)(data + 0x28);
      decal &= 1;
      break;
    }
  }
  return (char)decal;
}

/* 0x190930 — does this shader tag carry the "water decal" flag?
 *
 * The same shape as shader_is_decal, restricted to shader types 5 and 6 and
 * reading a different bit of the same flags byte:
 *   types 5 and 6:  MOV AL,[EAX+0x29]; SHR AL,4; AND AL,1
 * Only two arms, so MSVC dispatched with a subtract chain off one signed 16-bit
 * load (MOVSX EDX, word ptr [ECX+0x24]; SUB EDX,5; JZ; DEC EDX; JNZ) instead of
 * the jump table it built for shader_is_decal's four arms; the chain starts at
 * 5, which fixes the arm order.
 *
 * The offsets stay raw for the same reason as in shader_is_decal: each arm
 * looks at a *different* shader definition struct behind the FUN_001906b0
 * checked downcast, and none of those layouts is recovered.
 *
 * As in shader_is_decal, the arms accumulate into a byte the prologue clears
 * (XOR AL,AL at 0x190934), so the NULL test and the default arm share an
 * epilogue.  MSVC placed the type-6 block first and gave it its own POP EBP/RET
 * at 0x19095c while the type-5 block falls into the shared epilogue at 0x190970
 * — block placement, not two different return paths in the source. */
char shader_is_water_decal(void *shader_pointer)
{
  shader_definition *shader = (shader_definition *)shader_pointer;
  unsigned char water_decal;
  char *data;

  water_decal = 0;
  if (shader != (shader_definition *)0) {
    /* MOVSX EDX, word ptr [ECX+0x24] -- the selector is a signed 16-bit field.
     */
    switch (shader->base.type) {
    case 5:
      data = (char *)FUN_001906b0(shader, 5);
      water_decal = *(uint8_t *)(data + 0x29);
      water_decal >>= 4;
      water_decal &= 1;
      break;
    case 6:
      data = (char *)FUN_001906b0(shader, 6);
      water_decal = *(uint8_t *)(data + 0x29);
      water_decal >>= 4;
      water_decal &= 1;
      break;
    }
  }
  return (char)water_decal;
}

/* 0x190980 — does this shader tag opt out of the effect pass?
 *
 * Structurally identical to shader_is_water_decal: restricted to shader types 5
 * and 6, reading a third bit of the same flags byte:
 *   types 5 and 6:  MOV AL,[EAX+0x29]; SHR AL,5; AND AL,1
 * Two arms again, so MSVC dispatched with a subtract chain off one signed
 * 16-bit load (MOVSX EDX, word ptr [ECX+0x24]; SUB EDX,5; JZ; DEC EDX; JNZ)
 * rather than a jump table; the chain starting at 5 fixes the arm order.
 *
 * The offsets stay raw for the same reason as in shader_is_decal: each arm
 * looks at a *different* shader definition struct behind the FUN_001906b0
 * checked downcast, and none of those layouts is recovered.
 *
 * As in shader_is_water_decal, the arms accumulate into a byte the prologue
 * clears (XOR AL,AL at 0x190984), so the NULL test and the default arm share an
 * epilogue.  MSVC placed the type-6 block first (inline at 0x190998) and gave
 * it its own ADD ESP,8/AND AL,1/POP EBP/RET at 0x1909ab, while the out-of-line
 * type-5 block at 0x1909ad falls into the shared epilogue at 0x1909c0 — block
 * placement and tail duplication, not two different return paths in the source.
 *
 * The frame is PUSH EBP/MOV EBP,ESP with no `sub esp`: no spilled locals. */
char shader_ignores_effect(void *shader_pointer)
{
  shader_definition *shader = (shader_definition *)shader_pointer;
  unsigned char ignores_effect;
  char *data;

  ignores_effect = 0;
  if (shader != (shader_definition *)0) {
    /* MOVSX EDX, word ptr [ECX+0x24] -- the selector is a signed 16-bit field.
     */
    switch (shader->base.type) {
    case 5:
      data = (char *)FUN_001906b0(shader, 5);
      ignores_effect = *(uint8_t *)(data + 0x29);
      ignores_effect >>= 5;
      ignores_effect &= 1;
      break;
    case 6:
      data = (char *)FUN_001906b0(shader, 6);
      ignores_effect = *(uint8_t *)(data + 0x29);
      ignores_effect >>= 5;
      ignores_effect &= 1;
      break;
    }
  }
  return (char)ignores_effect;
}

/* 0x1909d0 — is this shader *type* one of the transparent shader classes?
 *
 * Unlike its neighbours this takes the shader type directly rather than a tag
 * pointer, so there is no NULL test, no FUN_001906b0 downcast and no frame
 * beyond PUSH EBP/MOV EBP,ESP (no `sub esp`: no spilled locals).
 *
 * The selector is loaded once and sign-extended (MOVSX ECX, word ptr [EBP+8]),
 * which is what proves the parameter is a *signed* 16-bit value; every
 * comparison against it is then a full 32-bit signed compare.
 * - CMP ECX,1 / JZ takes it.
 * - CMP ECX,4 / JLE rejects, so the range starts at 5.
 * - CMP ECX,0xa / JG rejects, so the range ends at 10 inclusive.
 * That is: transparent iff type == 1 || 5 <= type <= 10. Types 5..10 are the
 * same band the flag readers above dispatch on (shader_is_water_decal and
 * shader_ignores_effect only accept 5 and 6), so the two disjoint tests are one
 * `if` in the source, not two.
 * - `type` is a widened copy rather than the parameter itself: comparing the
 *   int16_t directly lets MSVC keep the whole function 16-bit (MOVW/CMPW on
 * CX), while the original widens once up front and compares in ECX.
 * - The upper bound is spelled `<= 10` and not `< 11` for the same reason: the
 *   original's immediate is 0xa with JG, and `< 11` compiles to CMP 0xb / JGE.
 *
 * The result is the byte the prologue clears (XOR AL,AL at 0x1909d7); the taken
 * arm materialises it with MOV AL,1 and falls into the shared POP EBP/RET, so
 * this is the assign-a-flag shape, not a ternary or a returned expression. */
char shader_type_is_transparent(int16_t shader_type)
{
  int type;
  char transparent;

  type = shader_type;
  transparent = 0;
  if (type == 1 || (type > 4 && type <= 10)) {
    transparent = 1;
  }
  return transparent;
}

/* 0x1909f0 — is this shader *type* one of the lightmapped shader classes?
 *
 * A mechanical twin of shader_type_is_transparent above: the two functions are
 * byte-identical apart from their compare immediates and the Jcc opcodes, so
 * the same source shape applies.
 *   55 8b ec               PUSH EBP / MOV EBP,ESP      (no `sub esp`: no
 * spills) 0f bf 4d 08            MOVSX ECX, word ptr [EBP+8] (signed 16-bit
 * selector, widened once up front) 32 c0                  XOR AL,AL (flag
 * cleared) 83 f9 03 / 7c 0c       CMP ECX,3  / JL  -> epilogue (reject below 3)
 *   83 f9 04 / 7e 05       CMP ECX,4  / JLE -> MOV AL,1 (accept 3..4)
 *   83 f9 08 / 75 02       CMP ECX,8  / JNZ -> epilogue (accept only 8)
 *   b0 01                  MOV AL,1
 *   5d c3                  POP EBP / RET
 * That is: lightmapped iff type == 3 || type == 4 || type == 8.
 *
 * Spelling notes, all forced by the immediates:
 * - `type` is a widened `int` copy, not the int16_t parameter itself; comparing
 *   the parameter directly lets MSVC stay 16-bit (CMPW on CX) instead of
 *   sign-extending once into ECX.
 * - `>= 3` (not `> 2`) picks CMP 3 / JL; `<= 4` (not `< 5`) picks CMP 4 / JLE.
 * - The result is the byte the prologue cleared, materialised by MOV AL,1 in
 * the taken arm which falls into the shared POP EBP/RET — the assign-a-flag
 * shape, not a returned expression or a ternary. Both accepting arms converge
 * on that single MOV AL,1. */
char shader_type_is_lightmapped(int16_t shader_type)
{
  int type;
  char lightmapped;

  type = shader_type;
  lightmapped = 0;
  if (type >= 3 && (type <= 4 || type == 8)) {
    lightmapped = 1;
  }
  return lightmapped;
}

/* 0x190a10 — is this shader *type* one of the vertex-lit shader classes?
 *
 * The third member of the same mechanical family as the two predicates above;
 * only the compare immediates and the Jcc opcodes differ.
 *   55 8b ec               PUSH EBP / MOV EBP,ESP      (no `sub esp`)
 *   0f bf 4d 08            MOVSX ECX, word ptr [EBP+8] (signed 16-bit selector,
 *                                                       widened once up front)
 *   32 c0                  XOR AL,AL                   (flag cleared)
 *   83 f9 04 / 74 05       CMP ECX,4 / JZ  -> MOV AL,1  (accept 4)
 *   83 f9 08 / 75 02       CMP ECX,8 / JNZ -> epilogue  (accept only 8)
 *   b0 01                  MOV AL,1
 *   5d c3                  POP EBP / RET
 * That is: vertex-lit iff type == 4 || type == 8. Note type 4 and type 8 are
 * also both accepted by shader_type_is_lightmapped, so this is a narrower test
 * over the same pair, not a disjoint band.
 *
 * Spelling notes, same as the twins: `type` is a widened `int` copy rather than
 * the int16_t parameter (comparing the parameter directly keeps MSVC 16-bit on
 * CX instead of sign-extending once into ECX), and the result is the byte the
 * prologue cleared, materialised by a single MOV AL,1 that both accepting arms
 * converge on before the shared POP EBP/RET — the assign-a-flag shape, not a
 * returned expression or a ternary. */
char shader_type_is_vertex_lit(int16_t shader_type)
{
  int type;
  char vertex_lit;

  type = shader_type;
  vertex_lit = 0;
  if (type == 4 || type == 8) {
    vertex_lit = 1;
  }
  return vertex_lit;
}

/* 0x190a30 — is this shader *type* legal on environment (BSP) geometry?
 *
 * The fourth member of the same mechanical family as the three predicates
 * above, and a byte-for-byte twin of shader_type_is_transparent: the two
 * functions differ only in their three compare immediates (1 -> 3 and 0xa ->
 * 9), so the identical source shape applies.
 *   55 8b ec               PUSH EBP / MOV EBP,ESP      (no `sub esp`: no
 *                                                       spills, no locals)
 *   0f bf 4d 08            MOVSX ECX, word ptr [EBP+8] (signed 16-bit selector,
 *                                                       widened once up front)
 *   32 c0                  XOR AL,AL                   (flag cleared)
 *   83 f9 03 / 74 0a       CMP ECX,3 / JZ  -> MOV AL,1  (accept 3)
 *   83 f9 04 / 7e 07       CMP ECX,4 / JLE -> epilogue  (reject 4 and below)
 *   83 f9 09 / 7f 02       CMP ECX,9 / JG  -> epilogue  (reject above 9)
 *   b0 01                  MOV AL,1                     (fallthrough: 5..9)
 *   5d c3                  POP EBP / RET
 * That is: valid iff type == 3 || (type > 4 && type <= 9), i.e. the accepted
 * set is {3, 5, 6, 7, 8, 9}. Note type 4 is explicitly *rejected* — it is
 * skipped over by the JLE that follows the `== 3` test, so it is a hole in the
 * band rather than the low end of a 3..9 range.
 *
 * Spelling notes, all forced by the encodings:
 * - `type` is a widened `int` copy, not the int16_t parameter itself; comparing
 *   the parameter directly lets MSVC stay 16-bit (CMPW on CX) instead of
 *   sign-extending once into ECX.
 * - `> 4` (not `>= 5`) picks CMP 4 / JLE; `<= 9` (not `< 10`) picks CMP 9 / JG.
 * - The result is the byte the prologue cleared, materialised by the single
 *   MOV AL,1 that the accepting arms converge on before the shared POP
 *   EBP/RET — the assign-a-flag shape, not a returned expression or a
 *   ternary. */
char shader_type_is_valid_for_environment(int16_t shader_type)
{
  int type;
  char valid;

  type = shader_type;
  valid = 0;
  if (type == 3 || (type > 4 && type <= 9)) {
    valid = 1;
  }
  return valid;
}

/* 0x190a50 — is this shader *type* legal on model (rendered object) geometry?
 *
 * The fifth member of the same mechanical family as the four predicates above;
 * only the compare immediates and the Jcc opcodes differ. The accepted set here
 * is a single contiguous signed band, so it is the simplest of the family:
 *   55 8b ec               PUSH EBP / MOV EBP,ESP      (no `sub esp`: no
 * spills) 0f bf 4d 08            MOVSX ECX, word ptr [EBP+8] (signed 16-bit
 * selector, widened once up front) 32 c0                  XOR AL,AL (flag
 * cleared) 83 f9 03 / 7c 07       CMP ECX,3 / JL  -> epilogue  (reject < 3) 83
 * f9 0a / 7f 02       CMP ECX,0a / JG -> epilogue  (reject > 10) b0 01 MOV AL,1
 *   5d c3                  POP EBP / RET
 * That is: valid on models iff type is in the closed signed interval [3, 10].
 * Both Jcc are signed on a sign-extended value, so the band is exactly that.
 * Note this is a superset of shader_type_is_valid_for_environment's [3, 9] plus
 * type 10, and of shader_type_is_lightmapped — models accept more types than
 * BSP geometry does.
 *
 * Spelling notes, same as the twins: `type` is a widened `int` copy rather than
 * the int16_t parameter (comparing the parameter directly keeps MSVC 16-bit on
 * CX instead of sign-extending once into ECX); `>= 3` (not `> 2`) picks
 * CMP 3 / JL and `<= 10` (not `< 11`) picks CMP 0a / JG (`< 11` would compile
 * to CMP 0b / JGE); and the result is the byte the prologue cleared,
 * materialised by MOV AL,1 in the accepting arm which falls into the shared
 * POP EBP/RET — the assign-a-flag shape, not a returned expression or a
 * ternary. The return stays `char`: the original never zero-extends past AL. */
char shader_type_is_valid_for_model(int16_t shader_type)
{
  int type;
  char valid;

  type = shader_type;
  valid = 0;
  if (type >= 3 && type <= 10) {
    valid = 1;
  }
  return valid;
}

/* 0x190a70 — may this shader *type* carry a shader modifier?
 *
 * The sixth and last member of the mechanical predicate family above, and an
 * *exact byte-for-byte duplicate* of shader_type_is_transparent (0x1909d0):
 * both functions are the same 28 bytes,
 *   558bec 0fbf4d08 32c0 83f901 740a 83f904 7e07 83f90a 7f02 b001 5dc3
 * i.e. the compiler emitted the same code twice from two separately-written
 * (but identically-shaped) source predicates, rather than folding them.
 *   55 8b ec               PUSH EBP / MOV EBP,ESP      (no `sub esp`: no
 *                                                       spills, no locals)
 *   0f bf 4d 08            MOVSX ECX, word ptr [EBP+8] (signed 16-bit selector,
 *                                                       widened once up front)
 *   32 c0                  XOR AL,AL                   (flag cleared)
 *   83 f9 01 / 74 0a       CMP ECX,1 / JZ  -> MOV AL,1  (accept 1)
 *   83 f9 04 / 7e 07       CMP ECX,4 / JLE -> epilogue  (reject 4 and below)
 *   83 f9 0a / 7f 02       CMP ECX,0a / JG -> epilogue  (reject above 10)
 *   b0 01                  MOV AL,1                     (fallthrough: 5..10)
 *   5d c3                  POP EBP / RET
 * That is: modifiers are legal iff type == 1 || (type > 4 && type <= 10), the
 * accepted set {1, 5, 6, 7, 8, 9, 10}. Types 2, 3 and 4 are holes: 2 is skipped
 * by the JLE after the `== 1` test, and so are 3 and 4. Because the byte image
 * is identical to shader_type_is_transparent, the accepted set is *the same*
 * set — the two tests are separate engine questions that happen to coincide on
 * this type enumeration, not one function reached by two names.
 *
 * Spelling notes, all forced by the encodings (identical to the twin):
 * - `type` is a widened `int` copy, not the int16_t parameter itself; comparing
 *   the parameter directly lets MSVC stay 16-bit (CMPW on CX) instead of
 *   sign-extending once into ECX. Both Jcc are signed, so a `uint16_t` selector
 *   would change behaviour for negative types.
 * - `> 4` (not `>= 5`) picks CMP 4 / JLE; `<= 10` (not `< 11`) picks CMP 0a /
 *   JG (`< 11` would compile to CMP 0b / JGE).
 * - The result is the byte the prologue cleared, materialised by the single
 *   MOV AL,1 that both accepting arms converge on before the shared POP
 *   EBP/RET — the assign-a-flag shape, not a returned expression or a ternary.
 *   The return stays `char`: the original never zero-extends past AL. */
char shader_type_is_valid_for_modifier(int16_t shader_type)
{
  int type;
  char valid;

  type = shader_type;
  valid = 0;
  if (type == 1 || (type > 4 && type <= 10)) {
    valid = 1;
  }
  return valid;
}

/* numeric_countdown_timer_set @ 0x00190be0
 *
 * Seeds the numeric countdown timer globals shared with
 * numeric_countdown_timer_update(): the remaining-time value at 0x4d8a78 and
 * the enable flag at 0x4d8a7c.
 *
 * Plain cdecl frame (PUSH EBP; MOV EBP,ESP), two stack params:
 *   time     int   [EBP+0x08]  -> MOV EAX,[EBP+8] ; MOV [0x4d8a78],EAX
 *   enabled  char  [EBP+0x0c]  -> MOV CL,[EBP+0xc] ; MOV byte [0x4d8a7c],CL
 *
 * The second store is a BYTE store through CL, so `enabled` must stay `char`;
 * widening it to int would clobber 0x4d8a7d-0x4d8a7f. The last-update time at
 * 0x4d8a80 is deliberately left untouched here (only update() writes it).
 * kb decl was previously void(void). */
void numeric_countdown_timer_set(int time, char enabled)
{
  *(int *)0x4d8a78 = time;
  *(char *)0x4d8a7c = enabled;
}

/* numeric_countdown_timer_get @ 0x00190c00
 *
 * Returns one digit of the countdown timer's remaining-time value (the signed
 * millisecond counter at 0x4d8a78, written by numeric_countdown_timer_set()
 * and decremented by numeric_countdown_timer_update()), selected by a1.
 *
 * Plain cdecl frame (PUSH EBP; MOV EBP,ESP), no locals, no _chkstk. The
 * selector is read SIGNED and NARROW:
 *   MOVSX EAX,word ptr [EBP+0x08] ; INC EAX ; CMP EAX,9 ; JA <tail>
 *   JMP dword ptr [EAX*4 + 0x190d68]      (10-entry jump table)
 * so the effective switch value is (short)a1 + 1, and a1 == -1 legitimately
 * selects case 0. An out-of-range selector falls to the shared tail at
 * 0x190d62, which returns the pre-initialised 0.
 *
 * Digit chain (all constant divides are MSVC magic-multiply sequences --
 * IMUL 0x66666667 / 0x51eb851f / 0x10624dd3 / 0x68db8bad / 0x45e7b273 /
 * 0x6fd91d85 / 0x4a90be59 / 0x774dfd5b, each followed by SAR + SHR 31 + ADD,
 * i.e. plain SIGNED integer division; the trailing %10 / %6 is a real IDIV
 * ECX). Case 1 uses a straight CDQ/IDIV 10 with no magic multiply since it
 * only needs the raw remainder. Writing these as ordinary signed C / and %
 * regenerates the same sequences; using unsigned would change both codegen
 * and behaviour for a negative counter.
 *
 * Case 0 is the odd one out: it returns the raw counter's low 16 bits, not a
 * digit. The reference loads the FULL dword (MOV EDX,[0x4d8a78]) and lets the
 * 16-bit return truncate it; MSVC 7.1 narrows this to MOV DX,word ptr [...]
 * for us regardless of whether the conversion is written implicitly or with an
 * explicit (int16_t) cast -- both forms were measured and score the same, so
 * this one instruction is a codegen choice we cannot steer from source.
 *
 * Every arm ends MOV AX,DX / POP EBP / RET, i.e. only AX is written and the
 * upper half of EAX is whatever the divide left behind -- which is what Ghidra
 * renders as CONCAT22(...) ten times over. That is a return-WIDTH fact, not a
 * CONCAT hazard: the return type is 16-bit, so the garbage high half is never
 * reproduced. Confirmed from the caller side -- FUN_000be6a0's reference does
 * MOV word ptr [EBP-4],AX into a zero-initialised int slot before reading it
 * back, i.e. it consumes a 16-bit return. The kb decl was previously int. */
int16_t numeric_countdown_timer_get(int a1)
{
  int16_t digit;

  digit = 0;
  switch ((short)a1 + 1) {
  case 0:
    digit = *(int *)0x4d8a78;
    break;
  case 1:
    digit = *(int *)0x4d8a78 % 10;
    break;
  case 2:
    digit = *(int *)0x4d8a78 / 10 % 10;
    break;
  case 3:
    digit = *(int *)0x4d8a78 / 100 % 10;
    break;
  case 4:
    digit = *(int *)0x4d8a78 / 1000 % 10;
    break;
  case 5:
    digit = *(int *)0x4d8a78 / 10000 % 6;
    break;
  case 6:
    digit = *(int *)0x4d8a78 / 60000 % 10;
    break;
  case 7:
    digit = *(int *)0x4d8a78 / 600000 % 6;
    break;
  case 8:
    digit = *(int *)0x4d8a78 / 3600000 % 10;
    break;
  case 9:
    digit = *(int *)0x4d8a78 / 36000000 % 10;
    break;
  }
  return digit;
}

/* numeric_countdown_timer_stop @ 0x00190d90
 *
 * Clears the countdown timer's enable flag. Two instructions, no frame:
 *   MOV byte ptr [0x004d8a7c],0x0
 *   RET
 *
 * The store is BYTE-width, matching the `char` flag written by
 * numeric_countdown_timer_set() and tested by
 * numeric_countdown_timer_update(); an int store would clobber
 * 0x4d8a7d-0x4d8a7f. The remaining-time (0x4d8a78) and last-update
 * (0x4d8a80) values are deliberately left untouched. */
void numeric_countdown_timer_stop(void)
{
  *(char *)0x4d8a7c = 0;
}

/* numeric_countdown_timer_restart @ 0x00190da0
 *
 * Sets the countdown timer's enable flag. Two instructions, no frame:
 *   MOV byte ptr [0x004d8a7c],0x1
 *   RET
 *
 * Byte-for-byte the mirror of numeric_countdown_timer_stop() above, with the
 * immediate 1 instead of 0, so the store must stay BYTE-width (`char`); an int
 * store would clobber 0x4d8a7d-0x4d8a7f. The remaining-time (0x4d8a78) and
 * last-update (0x4d8a80) values are deliberately left untouched -- only
 * numeric_countdown_timer_set()/update() write those. */
void numeric_countdown_timer_restart(void)
{
  *(char *)0x4d8a7c = 1;
}

void numeric_countdown_timer_update(void)
{
  int current_time;

  current_time = *(int *)0x4d8a80;
  if (*(char *)0x4d8a7c) {
    current_time = (game_time_get() * 1000) / 30;
    if (*(int *)0x4d8a80 <= current_time) {
      *(int *)0x4d8a78 += *(int *)0x4d8a80 - current_time;
      if (*(int *)0x4d8a78 < 0)
        *(int *)0x4d8a78 = 0;
    }
  }
  *(int *)0x4d8a80 = current_time;
}
