/* 0x160910 — empty body.
 * Binary: bare RET (C3). */
void _rasterizer_environment_lightmap_end(void)
{
}

/* 0x160920 — end rasterizer profile section 3.
 * Binary: PUSH 0x3 / CALL FUN_0016fa40 / POP ECX / RET.
 * FUN_0016fa40 takes an int16_t profile index; the semantic role of the
 * index is unproven. */
void _rasterizer_environment_lightmaps_end(void)
{
  FUN_0016fa40(3);
}

/* 0x160930 — empty body.
 * Binary: bare RET (C3). */
void _rasterizer_environment_diffuse_light_end(void)
{
}

/* 0x160940 — begin the HUD rasterizer profile section.
 * Binary: PUSH 0x5 / CALL 0x16fa40 / POP ECX / RET.
 * FUN_0016fa40 takes an int16_t profile index; the semantic role of the
 * callee and of index 5 is unproven (no assert or string evidence). */
void _rasterizer_hud_begin(void)
{
  FUN_0016fa40(5);
}

/* 0x160950 — end diffuse-texture scope.
 * Binary: PUSH 2 / CALL FUN_00158ae0 / PUSH 8 / CALL FUN_0016fa40 /
 * ADD ESP,8 / RET. Call order and profile index are binary-fixed; their
 * semantic roles are unproven. */
void _rasterizer_environment_diffuse_textures_end(void)
{
  FUN_00158ae0(2);
  FUN_0016fa40(8);
}

/* 0x160970 — empty body.
 * Binary: bare RET (C3). */
void _rasterizer_hud_end(void)
{
}

/* 0x160980 — end rasterizer profile section 0xb.
 * Binary: PUSH 0xb / CALL FUN_0016fa40 / POP ECX / RET.
 * The profile index semantic role is unproven. */
void _rasterizer_environment_specular_lights_end(void)
{
  FUN_0016fa40(0xb);
}

/* 0x160990 — empty body.
 * Binary: bare RET (C3). */
void FUN_00160990(void)
{
}

/* 0x1609a0 — end rasterizer profile section 0xc.
 * Binary: PUSH 0xc / CALL FUN_0016fa40 / POP ECX / RET.
 * FUN_0016fa40 takes an int16_t profile index; the semantic role of the
 * index is unproven. */
void _rasterizer_dynamic_lit_geometry_draw(void)
{
  FUN_0016fa40(0xc);
}

/* 0x1609b0 — draw one environment geometry batch through the shared
 * texture-animation vertex-shader constant block.
 *
 * Signature: cdecl, bare RET (caller cleans). Six dword parameter slots are
 * read from the frame: [EBP+8] shader, [EBP+0x10], [EBP+0x14], [EBP+0x18] and
 * [EBP+0x1c]. The slot at [EBP+0xc] is never read by this function, but it
 * occupies a parameter position, so it is declared and left unused.
 * The last three forwarded slots land in rasterizer_draw_dynamic_triangles_static_vertices2's kb decl positions
 * vertices_per_primitive / a2 / triangle_count, and in
 * rasterizer_frame_statistics_count_static_vertices' identically named slots;
 * the names here are taken from those decls, not from independent evidence.
 * [EBP+0x1c] is read as a uint16 at offset 0 (the vertex type passed to
 * FUN_00178b40) and forwarded both as-is and as +0x14.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x873)
 *   0x3256bc  uint16  mode selector; this path requires exactly 0
 *   0x3256d0  uint8   enable flag (must be non-zero)
 *   0x3256d2  uint8   second enable flag (must be non-zero)
 *   0x3256b0  uint16  must be 0
 *   0x47dca4  uint8   must be 0 (set by FUN_00163fe0/FUN_00164590 when a
 *                     stage bitmap was missing)
 *   0x3256ba  uint16  statistics mode; the counters below run only when it
 *                     is 2. NOTE: this is 0x3256ba, not the 0x3256bc used by
 *                     the guard above — both appear in this function.
 *   0x5a5e18  dword   value forwarded unchanged to
 *                     shader_environment_texture_animation_evaluate; the
 *                     reference copies it with MOV EDX / PUSH EDX, so its
 *                     type is unproven.
 *   0x1fb6e0  dword   shadow copy of render state 0x40a80, stored after the
 *                     matching D3DDevice_SetRenderState_Simple call.
 *   0x5a548c / 0x5a5488 / 0x5a5484
 *                     frame statistic counters: batch count, triangle_count
 *                     accumulator, and the accumulator fed by
 *                     rasterizer_frame_statistics_count_static_vertices.
 *
 * Shader-data offsets (from FUN_001906b0(shader, 3)); all four are read
 * directly in the disassembly:
 *   +0x138 / +0x13c  the two animated values written into constant row 0
 *   +0x2d4           float compared against 1.0f and passed to FUN_00159070
 *   +0x2f4 / +0x2f8  floats compared against 0.0f
 *
 * Branch senses are decoded from the FNSTSW forms, not from the decompiler:
 *   TEST AH,0x41 / JZ  after FCOMP  =>  operand > memory
 *   TEST AH,0x5  / JP  after FCOMP  =>  jump unless operand < memory
 *
 * The 12-dword block is one contiguous local (SUB ESP,0x30; every slot from
 * EBP-0x30 to EBP-0x4 is written) uploaded as 3 vertex-shader constants at
 * register -0x54. It is typed uint32_t because the reference copies the two
 * shader-data values with GPR moves (MOV ECX/EDX + MOV [EBP-N]) rather than
 * x87 loads, and stores the 1.0f entries as the immediate 0x3f800000.
 * Slots 7 and 11 are the trailing components of constant rows 1 and 2; their
 * addresses are handed to the texture-animation evaluator as out-parameters.
 *
 * Call-site arity note: the binary emits a single ADD ESP,0x1c (7 dwords)
 * after CALL 0x190a90. FUN_00178b40's 3 cdecl arguments are left uncleaned
 * before it, so 3 + 4 = 7: shader_environment_texture_animation_evaluate
 * takes 4 arguments, not the 0 its previous kb decl claimed.
 *
 * Call order, the guard compare order, and the counter update order (both
 * accumulator stores precede the count_static_vertices call) are binary-fixed.
 */
void FUN_001609b0(void *shader, int a2, int vertices_per_primitive, int a4,
                  int triangle_count, void *geometry)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  uint32_t render_state;
  int static_vertices;
  int batch_count;
  int triangle_total;

  (void)a2;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x873, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256d0 != 0 &&
      *(uint8_t *)0x3256d2 != 0 && *(uint16_t *)0x3256b0 == 0 &&
      *(uint8_t *)0x47dca4 == 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x87d, true);
      system_exit(-1);
    }

    shader_data = FUN_001906b0(shader, 3);

    if ((*(float *)((char *)shader_data + 0x2f4) > *(float *)0x2533c0 ||
         *(float *)((char *)shader_data + 0x2f8) > *(float *)0x2533c0) &&
        *(float *)((char *)shader_data + 0x2d4) < *(float *)0x2533c8) {
      FUN_00178b40(0x3a, *(uint16_t *)geometry, 0);

      /* [0]/[1] are copied as raw dwords: the reference moves them through
       * GPRs (MOV ECX,[ESI+0x138] / MOV [EBP-0x30],ECX), not the x87 stack.
       * They must stay ahead of the literal stores — emitting them last
       * costs 5.1pp (measured). */
      *(uint32_t *)&constants[0] = *(uint32_t *)((char *)shader_data + 0x138);
      *(uint32_t *)&constants[1] = *(uint32_t *)((char *)shader_data + 0x13c);
      /* The reference loads 0x5a5e18 before the literal stores (MOV EDX,
       * [0x5a5e18] precedes MOV [EBP-0x28],0x3f800000), which lets it push
       * all four arguments ahead of the block. */
      texture_globals = *(void **)0x5a5e18;
      constants[2] = 1.0f;
      constants[3] = 1.0f;
      constants[4] = 1.0f;
      constants[5] = 0.0f;
      constants[6] = 0.0f;
      constants[7] = 0.0f;
      constants[8] = 0.0f;
      constants[9] = 1.0f;
      constants[10] = 0.0f;
      constants[11] = 0.0f;

      shader_environment_texture_animation_evaluate(
        shader, texture_globals, &constants[7], &constants[11]);

      D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

      render_state = FUN_00159070(*(float *)((char *)shader_data + 0x2d4));
      D3DDevice_SetRenderState_Simple(0x40a80, render_state);
      *(uint32_t *)0x1fb6e0 = render_state;

      rasterizer_draw_dynamic_triangles_static_vertices2(vertices_per_primitive, a4, triangle_count, (const vertex_buffer *)geometry,
                   (const vertex_buffer *)((char *)geometry + 0x14));

      if (*(uint16_t *)0x3256ba == 2) {
        batch_count = *(int *)0x5a548c;
        triangle_total = *(int *)0x5a5488;
        *(int *)0x5a548c = batch_count + 1;
        *(int *)0x5a5488 = triangle_total + triangle_count;
        /* The reference loads 0x5a5484 only after the call returns, so the
         * call must not be sequenced against a pending read of it. */
        static_vertices = rasterizer_frame_statistics_count_static_vertices(
          vertices_per_primitive, a4, triangle_count);
        *(int *)0x5a5484 = *(int *)0x5a5484 + static_vertices;
      }
    }
  }
}

/* 0x160bb0 — empty body.
 * Binary: bare RET (C3). */
void FUN_00160bb0(void)
{
}

/* 0x160bc0 — begin rasterizer profile section 0xd.
 * Binary: PUSH 0xd / CALL 0x16fa40 / POP ECX / RET.
 * Profile index 0xd is unnamed (no assert or string evidence). */
void FUN_00160bc0(void)
{
  FUN_0016fa40(0xd);
}

/* 0x160bd0 — begin rasterizer profile section 0xe.
 * Binary: PUSH 0xe / CALL 0x16f910 / POP ECX / RET.
 * Profile index 0xe is unnamed (no assert or string evidence). */
void FUN_00160bd0(void)
{
  FUN_0016f910(0xe);
}

/* 0x160be0 — begin rasterizer profile section 0xe.
 * Binary: PUSH 0xe / CALL 0x16fa40 / POP ECX / RET.
 * Profile index 0xe is unnamed (no assert or string evidence). */
void FUN_00160be0(void)
{
  FUN_0016fa40(0xe);
}

/* 0x160bf0 — one-arg wrapper.
 * Binary: PUSH 0xf / CALL 0x16f910 / POP ECX / RET.
 * Callee FUN_0016f910 takes an int16_t profile index; the role of the callee
 * and the meaning of index 0xf are unproven (no assert or string evidence). */
void FUN_00160bf0(void)
{
  FUN_0016f910(0xf);
}

/* 0x160c00 — one-arg wrapper.
 * Binary: PUSH 0xf / CALL 0x16fa40 / POP ECX / RET.
 * Callee FUN_0016fa40 takes an int16_t profile index; the role of the callee
 * and the meaning of index 0xf are unproven (no assert or string evidence). */
void FUN_00160c00(void)
{
  FUN_0016fa40(0xf);
}

/* 0x160c10 — one-arg wrapper followed by a tail call.
 * Binary: PUSH 0x10 / CALL 0x16f910 / ADD ESP,0x4 / JMP 0x174ce0.
 * FUN_0016f910 takes an int16_t profile index; the role of both callees and
 * the meaning of index 0x10 are unproven (no assert or string evidence). */
void FUN_00160c10(void)
{
  FUN_0016f910(0x10);
  FUN_00174ce0();
}

/* 0x160c20 — a no-arg call followed by a one-arg wrapper.
 * Binary: CALL 0x1749b0 / PUSH 0x10 / CALL 0x16fa40 / POP ECX / RET.
 * FUN_0016fa40 takes an int16_t profile index; the role of both callees and
 * the meaning of index 0x10 are unproven (no assert or string evidence).
 * Call order is binary-fixed: FUN_001749b0 runs before FUN_0016fa40. */
void FUN_00160c20(void)
{
  FUN_001749b0();
  FUN_0016fa40(0x10);
}

/* 0x160c30 — begin rasterizer profile section 3 and, for a set of accepted
 * modes, bind texture stage 3 and program its stage/render state.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x11)
 *   0x3256bc  uint16    mode selector; accepted values 0,2,6,3,4,7,5,8
 *                       (compare order is binary-fixed; MOV AX loads once)
 *   0x3256c9  uint8     enable flag (must be non-zero)
 *   0x476204  ptr       rasterizer globals; +0x1c is a bitmap tag index
 *   0x1fb7a4 / 0x1fb784 / 0x1fb78c / 0x1fb77c / 0x1fb798
 *                       render-state shadow copies, each stored next to
 * the matching D3DDevice_SetRenderState_Simple call. Call order and the
 * interleaving of the shadow stores are binary-fixed. */
void FUN_00160c30(void)
{
  uint16_t mode;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c", 0x11,
      true);
    system_exit(-1);
  }

  FUN_0016f910(3);

  mode = *(uint16_t *)0x3256bc;
  if ((mode == 0 || mode == 2 || mode == 6 || mode == 3 || mode == 4 ||
       mode == 7 || mode == 5 || mode == 8) &&
      *(uint8_t *)0x3256c9 != 0) {
    rasterizer_set_texture_direct(3, *(int *)(*(int *)0x476204 + 0x1c), 0);

    D3DDevice_SetTextureStageState(3, 10, 3);
    D3DDevice_SetTextureStageState(3, 0xb, 3);
    D3DDevice_SetTextureStageState(3, 0xc, 3);
    D3DDevice_SetTextureStageState(3, 0xd, 2);
    D3DDevice_SetTextureStageState(3, 0xe, 2);
    D3DDevice_SetTextureStageState(3, 0xf, 2);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGBA);
    *(unsigned long *)0x1fb7a4 = NV097_COLOR_MASK_RGBA;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(unsigned long *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(unsigned long *)0x1fb78c = 0x7f;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x203);
    *(unsigned long *)0x1fb77c = 0x203;
    D3DDevice_SetRenderState_Simple(0x4035c, 1);
    *(unsigned long *)0x1fb798 = 1;
    D3DDevice_SetRenderState_ZBias(0);
  }
}

/* 0x160dc0 — bind a bitmap to texture stage 2 and program its stage states,
 * then refresh the three floats at 0x47dc98/0x47dc9c/0x47dca0.
 *
 * Signature: the reference reads its argument at [EBP+8] and returns with a
 * bare RET (no stack cleanup), so this is cdecl with one parameter. It is
 * forwarded unchanged to rasterizer_set_texture_bitmap_data, whose kb decl
 * names that slot bitmap_data. The kb decl types it int; the sole caller
 * (FUN_0017cc10, 0x17cc10) passes an int through, so the type is left as-is.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x44)
 *   0x3256bc  uint16    mode selector; accepted values 0,2,6,3,4,7,5,8
 *                       (compare order is binary-fixed; a single MOV AX loads
 *                       it once, so it is cached in a local here)
 *   0x3256c9  uint8     enable flag (must be non-zero)
 *   0x3256ed  uint8     flag selecting stage-state value 1 vs 2 for states
 *                       0xd/0xe/0xf. The reference re-loads it before each of
 *                       the three calls (into CL, DL, AL) rather than caching,
 *                       so each use below is a direct read.
 *   0x47dca4  uint8     byte flag: cleared on the bound path, set when the
 *                       supplied pointer is null. Both arms store an
 *                       immediate; unlike the 0x163fe0/0x164590 twins, NEITHER
 *                       arm returns — the bound path JMPs to 0x160edb and the
 *                       null path falls through to it, so both reach the
 *                       0x3256b0 block below.
 *   0x3256b0  int16     signed selector for the trailing block: > 0 to run it
 *                       at all (TEST AX,AX / JLE), == 2 selects the constant
 *                       fill. Loaded once (MOV AX) and reused, so cached.
 *   0x3256e4  float     constant source for the == 2 fill; one FLD feeds
 *                       FST 0x47dca0, FST 0x47dc9c, FSTP 0x47dc98.
 *   0x47dc98/0x47dc9c/0x47dca0  float  three outputs. In the == 2 arm all
 *                       three take the same value; otherwise each takes a
 *                       separate random_math_real draw, stored in the
 *                       binary's order 0x47dc98, 0x47dc9c, 0x47dca0.
 *
 * The random seed is binary-fixed but its role is unproven: ESI holds the
 * parameter from 0x160e31 and is callee-saved across the intervening calls,
 * so MOV [EBP-4],ESI at 0x160f0d seeds the generator with the argument value.
 * Ghidra leaves local_8 uninitialized here; that is a decompiler bug.
 * Call order is binary-fixed. */
void FUN_00160dc0(int bitmap_data)
{
  uint16_t mode;
  int16_t select_3256b0;
  unsigned int seed;
  float value;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c", 0x44,
      true);
    system_exit(-1);
  }

  mode = *(uint16_t *)0x3256bc;
  if ((mode == 0 || mode == 2 || mode == 6 || mode == 3 || mode == 4 ||
       mode == 7 || mode == 5 || mode == 8) &&
      *(uint8_t *)0x3256c9 != 0) {
    if (bitmap_data != 0) {
      rasterizer_set_texture_bitmap_data(2, (void *)bitmap_data);

      D3DDevice_SetTextureStageState(2, 10, 3);
      D3DDevice_SetTextureStageState(2, 0xb, 3);
      D3DDevice_SetTextureStageState(2, 0xd, (*(uint8_t *)0x3256ed != 0) + 1);
      D3DDevice_SetTextureStageState(2, 0xe, (*(uint8_t *)0x3256ed != 0) + 1);
      D3DDevice_SetTextureStageState(2, 0xf, (*(uint8_t *)0x3256ed != 0) + 1);

      *(uint8_t *)0x47dca4 = 0;
    } else {
      D3DDevice_SetTexture(2, (void *)0);
      *(uint8_t *)0x47dca4 = 1;
    }

    select_3256b0 = *(int16_t *)0x3256b0;
    if (select_3256b0 > 0) {
      if (select_3256b0 == 2) {
        value = *(float *)0x3256e4;
        *(float *)0x47dca0 = value;
        *(float *)0x47dc9c = value;
        *(float *)0x47dc98 = value;
        return;
      }
      seed = (unsigned int)bitmap_data;
      *(float *)0x47dc98 = random_math_real(&seed);
      *(float *)0x47dc9c = random_math_real(&seed);
      *(float *)0x47dca0 = random_math_real(&seed);
    }
  }
}

/* 0x160f50 — draw one environment lightmap geometry batch. Two disjoint
 * drawing paths selected by *(int16_t *)0x3256bc:
 *   mode 9                     debug lightmap-index pass. A 51-entry constant
 *                              table maps *(int16_t *)0x3256ea (split by /1000
 *                              and %1000) to a {vertex-shader kind, pixel-
 *                              shader constant} pair. The table is built on the
 *                              stack; the reference emits its stores in
 *                              register-scheduling order, they are written here
 *                              in index order.
 *   modes 0,2,6,3,4,7,5,8      normal environment lightmap pass. The compare
 *                              order of that chain is binary-fixed, as is the
 *                              jump-table case grouping further down.
 * Globals (identities unproven except where noted):
 *   0x3256bc  int16   drawing mode. Cached in AX for the entry chain but
 *                     re-read (MOVSX) for the switch, so both spellings below
 *                     are deliberate.
 *   0x3256ba  int16   frame-statistics enable (== 2)
 *   0x3256c9  uint8   normal-pass enable
 *   0x3256ea  int16   packed debug lightmap selector (quotient = frame index,
 *                     remainder = table index)
 *   0x3256ec  uint8   / 0x3256f4 uint8   render toggles
 *   0x3256b0  int16   lighting variant selector
 *   0x325724  uint32  raw dword copied into vertex-shader constant slot 0
 *   0x476204  ptr     bitmap/texture globals; dereferenced, so this is a
 *                     double indirection
 *   0x47dca4  uint8   vertex stride selector: 0 => second stream at +0x14,
 *                     otherwise at +0
 *   0x47dc98  float3  precomputed color used by lighting variants 2 and 3
 *   0x2533c0  float   0.0f literal pool
 *   0x2533c8  float   1.0f literal pool. The two 1.0f-scaled FUN_00012fb0
 *                     calls at the end of the illumination block push an
 *                     immediate 0x3f800000 instead, hence the plain 1.0f
 *                     literals there.
 *   0x5a5ac0  -       0xf0-byte pixel-shader state block (shared with
 *                     rasterizer_xbox_widgets/shadows/models/screen_effect)
 *   0x5a5e18  -       texture-animation globals. Read as a pointer for
 *                     shader_environment_texture_animation_evaluate and as a
 *                     float (an animation time) for the three FUN_0010a5e0
 *                     inputs; both spellings are in the disassembly.
 *   0x1fb788 / 0x1fb7a4 / 0x1fb784 / 0x1fb77c / 0x1fb798
 *                     render-state shadow copies, each paired with the call it
 *                     mirrors. ZEnable and ZBias have no shadow store.
 *   0x5a5424 / 0x5a5428 / 0x5a542c   frame-statistics accumulators
 * The three FUN_0010a5e0 function-type loads are MOVZX (XOR reg,reg; MOV
 * rx,word), not MOVSX, so they are unsigned; the table kind load and the
 * 0x3256ea load are MOVSX and stay signed.
 * Call order, guard compare order, the 0x5a5ac0 store order, and the
 * statistics-block shape are binary-fixed. */
void FUN_00160f50(void *shader, int frame_index, int vertices_per_primitive,
                  int a4, int triangle_count, void *vertex_buffer)
{
  struct {
    int16_t kind;
    int32_t value;
  } table[51];
  float plasma_a[3];
  float plasma_b[3];
  float vector_secondary[3];
  float vector_primary[3];
  float scale_plasma;
  float constants[16];
  float scale_primary;
  float scale_secondary;
  void *shader_data;
  void *texture_globals;
  void *bitmap_globals;
  int selector;
  int lightmap_frame;
  short table_index;
  char is_last;
  uint32_t alpha_enable;
  uint32_t stage1_filter;
  uint32_t tag_index;
  int batch_count;
  int triangle_total;
  int static_vertices;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c", 0x87,
      true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 9) {
    if (*(uint16_t *)vertex_buffer != 1) {
      return;
    }

    table[0].kind = 0;
    table[0].value = 8;
    table[1].kind = 0;
    table[1].value = 9;
    table[2].kind = 0;
    table[2].value = 0xa;
    table[3].kind = 0;
    table[3].value = 0xb;
    table[4].kind = 1;
    table[4].value = 8;
    table[5].kind = 1;
    table[5].value = 9;
    table[6].kind = 1;
    table[6].value = 0xa;
    table[7].kind = 1;
    table[7].value = 0x14;
    table[8].kind = 1;
    table[8].value = 0xb;
    table[9].kind = -1;
    table[9].value = -1;
    table[10].kind = 0;
    table[10].value = 4;
    table[11].kind = 0;
    table[11].value = 5;
    table[12].kind = 1;
    table[12].value = 4;
    table[13].kind = 1;
    table[13].value = 5;
    table[14].kind = 2;
    table[14].value = 4;
    table[15].kind = 2;
    table[15].value = 5;
    table[16].kind = 3;
    table[16].value = 4;
    table[17].kind = -1;
    table[17].value = -1;
    table[18].kind = -1;
    table[18].value = -1;
    table[19].kind = -1;
    table[19].value = -1;
    table[20].kind = 5;
    table[20].value = 5;
    table[21].kind = 2;
    table[21].value = 0x14;
    table[22].kind = 2;
    table[22].value = 0x15;
    table[23].kind = 2;
    table[23].value = 0x13;
    table[24].kind = -1;
    table[24].value = -1;
    table[25].kind = -1;
    table[25].value = -1;
    table[26].kind = -1;
    table[26].value = -1;
    table[27].kind = -1;
    table[27].value = -1;
    table[28].kind = -1;
    table[28].value = -1;
    table[29].kind = -1;
    table[29].value = -1;
    table[30].kind = 2;
    table[30].value = 8;
    table[31].kind = 2;
    table[31].value = 9;
    table[32].kind = 2;
    table[32].value = 0xa;
    table[33].kind = 2;
    table[33].value = 0xb;
    table[34].kind = -1;
    table[34].value = -1;
    table[35].kind = -1;
    table[35].value = -1;
    table[36].kind = -1;
    table[36].value = -1;
    table[37].kind = -1;
    table[37].value = -1;
    table[38].kind = -1;
    table[38].value = -1;
    table[39].kind = -1;
    table[39].value = -1;
    table[40].kind = 3;
    table[40].value = 5;
    table[41].kind = 4;
    table[41].value = 4;
    table[42].kind = 4;
    table[42].value = 5;
    table[43].kind = 5;
    table[43].value = 4;
    table[44].kind = -1;
    table[44].value = -1;
    table[45].kind = -1;
    table[45].value = -1;
    table[46].kind = -1;
    table[46].value = -1;
    table[47].kind = -1;
    table[47].value = -1;
    table[48].kind = -1;
    table[48].value = -1;
    table[49].kind = -1;
    table[49].value = -1;
    table[50].kind = 3;
    table[50].value = 0xc;

    selector = *(int16_t *)0x3256ea;
    lightmap_frame = selector / 1000;
    table_index = (short)(selector % 1000);
    if (table_index < 0 || table_index >= 0x33) {
      return;
    }
    if (table[table_index].kind == -1) {
      return;
    }

    bitmap_globals = *(void **)0x476204;
    is_last = (char)(table_index == 0x32);
    if (is_last != 0 && *(int *)((char *)bitmap_globals + 0x118) != -1) {
      rasterizer_set_texture_direct(0, *(int *)((char *)bitmap_globals + 0x118),
                                    0);
      D3DDevice_SetTextureStageState(0, 0xa, 1);
      D3DDevice_SetTextureStageState(0, 0xb, 1);
    } else {
      rasterizer_set_texture_direct(0, *(int *)((char *)bitmap_globals + 0x1c),
                                    (short)lightmap_frame);
      D3DDevice_SetTextureStageState(0, 0xa, 3);
      D3DDevice_SetTextureStageState(0, 0xb, 3);
      D3DDevice_SetTextureStageState(0, 0xc, 3);
    }
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);

    rasterizer_set_texture_direct(
      1, *(int *)((char *)*(void **)0x476204 + 0x1c), (short)lightmap_frame);
    D3DDevice_SetTextureStageState(1, 0xa, 3);
    D3DDevice_SetTextureStageState(1, 0xb, 3);
    D3DDevice_SetTextureStageState(1, 0xc, 3);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);

    rasterizer_set_texture_direct(
      2, *(int *)((char *)*(void **)0x476204 + 0x1c), (short)lightmap_frame);
    D3DDevice_SetTextureStageState(2, 0xa, 3);
    D3DDevice_SetTextureStageState(2, 0xb, 3);
    D3DDevice_SetTextureStageState(2, 0xc, 3);
    D3DDevice_SetTextureStageState(2, 0xd, 2);
    D3DDevice_SetTextureStageState(2, 0xe, 2);
    D3DDevice_SetTextureStageState(2, 0xf, 2);

    rasterizer_set_texture_direct(
      3, *(int *)((char *)*(void **)0x476204 + 0x1c), (short)lightmap_frame);
    D3DDevice_SetTextureStageState(3, 0xa, 3);
    D3DDevice_SetTextureStageState(3, 0xb, 3);
    D3DDevice_SetTextureStageState(3, 0xc, 3);
    D3DDevice_SetTextureStageState(3, 0xd, 2);
    D3DDevice_SetTextureStageState(3, 0xe, 2);
    D3DDevice_SetTextureStageState(3, 0xf, 2);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
    *(uint32_t *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(uint32_t *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(uint32_t *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x203);
    *(uint32_t *)0x1fb77c = 0x203;
    D3DDevice_SetRenderState_Simple(0x4035c, 1);
    *(uint32_t *)0x1fb798 = 1;
    D3DDevice_SetRenderState_ZBias(0);

    FUN_00178b40(0x25, *(uint16_t *)vertex_buffer, table[table_index].kind);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b98 = (2 * (uint32_t)(is_last == 0) + 1) | 0x18c60;
    if (is_last != 0) {
      *(uint32_t *)&constants[4] = *(uint32_t *)0x325724;
      constants[5] = 1.0f;
      constants[6] = 1.0f;
      constants[7] = 1.0f;
      constants[8] = 1.0f;
      constants[9] = 0.0f;
      constants[10] = 0.0f;
      constants[11] = 0.0f;
      constants[12] = 0.0f;
      constants[13] = 1.0f;
      constants[14] = 0.0f;
      constants[15] = 0.0f;
      D3DDevice_SetVertexShaderConstant(-0x54, &constants[4], 3);
      *(uint32_t *)0x5a5b94 = 3;
      *(uint32_t *)0x5a5ae8 = 0xff0000;
      *(uint32_t *)0x5a5b08 = 0xff;
      *(uint32_t *)0x5a5b48 = 0x4849484a;
      *(uint32_t *)0x5a5b74 = 0x30cd;
      *(uint32_t *)0x5a5b4c = 0xc0c0d0d;
      *(uint32_t *)0x5a5b78 = 0xcd;
      *(uint32_t *)0x5a5b50 = 0xc010d02;
      *(uint32_t *)0x5a5b7c = 0xc00;
      *(uint32_t *)0x5a5ae0 = (uint32_t)table[table_index].value | 0x18200000;
    } else {
      *(uint32_t *)0x5a5b94 = 1;
      *(uint32_t *)0x5a5ae0 = (uint32_t)table[table_index].value;
    }
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    rasterizer_draw_dynamic_triangles_static_vertices2(vertices_per_primitive, a4, triangle_count, (const struct vertex_buffer *)vertex_buffer,
                 (const struct vertex_buffer *)((char *)vertex_buffer + 20 * (*(uint8_t *)0x47dca4 == 0)));
    return;
  }

  if (*(uint16_t *)0x3256bc != 0 && *(uint16_t *)0x3256bc != 2 &&
      *(uint16_t *)0x3256bc != 6 && *(uint16_t *)0x3256bc != 3 &&
      *(uint16_t *)0x3256bc != 4 && *(uint16_t *)0x3256bc != 7 &&
      *(uint16_t *)0x3256bc != 5 && *(uint16_t *)0x3256bc != 8) {
    return;
  }
  if (*(uint8_t *)0x3256c9 == 0) {
    return;
  }

  if (shader == 0) {
    display_assert(
      "shader",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x15b, true);
    system_exit(-1);
  }
  if (vertex_buffer == 0) {
    display_assert(
      "vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x15c, true);
    system_exit(-1);
  }

  shader_data = FUN_001906b0(shader, 3);
  FUN_00178b40(0x10, *(uint16_t *)vertex_buffer,
               shader_get_vertex_shader_permutation(shader));

  if ((*(uint8_t *)((char *)shader_data + 0x28) & 1) == 0 ||
      (alpha_enable = 1, *(uint8_t *)0x3256f4 == 0)) {
    alpha_enable = 0;
  }
  D3DDevice_SetRenderState_Simple(0x40300, alpha_enable);
  *(uint32_t *)0x1fb788 = alpha_enable;

  tag_index = 0xffffffff;
  if ((*(uint8_t *)((char *)shader_data + 0x28) & 2) == 0) {
    tag_index = *(uint32_t *)((char *)shader_data + 0x134);
  }
  rasterizer_set_texture(0, 0, 3, (int)tag_index, (short)frame_index);
  D3DDevice_SetTextureStageState(0, 0xa, 1);
  D3DDevice_SetTextureStageState(0, 0xb, 1);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);

  rasterizer_set_texture(1, 0, 0, *(int *)((char *)shader_data + 0x260),
                         (short)frame_index);
  D3DDevice_SetTextureStageState(1, 0xa, 1);
  D3DDevice_SetTextureStageState(1, 0xb, 1);
  D3DDevice_SetTextureStageState(1, 0xd, 2);
  D3DDevice_SetTextureStageState(1, 0xe, 2);
  D3DDevice_SetTextureStageState(1, 0xf, 2);

  if ((*(uint8_t *)((char *)shader_data + 0x180) & 1) != 0) {
    SetTextureStageStateSmart(1, 0xd, 1);
    SetTextureStageStateSmart(1, 0xe, 1);
    stage1_filter = 1;
  } else {
    SetTextureStageStateSmart(1, 0xd, 2);
    SetTextureStageStateSmart(1, 0xe, 2);
    stage1_filter = 2;
  }
  SetTextureStageStateSmart(1, 0xf, (int)stage1_filter);

  *(uint32_t *)&constants[4] = *(uint32_t *)((char *)shader_data + 0x138);
  *(uint32_t *)&constants[5] = *(uint32_t *)((char *)shader_data + 0x13c);
  *(uint32_t *)&constants[6] = *(uint32_t *)((char *)shader_data + 0x250);
  texture_globals = *(void **)0x5a5e18;
  constants[7] = 1.0f;
  constants[8] = 1.0f;
  constants[9] = 0.0f;
  constants[10] = 0.0f;
  constants[11] = 0.0f;
  constants[12] = 0.0f;
  constants[13] = 1.0f;
  constants[14] = 0.0f;
  constants[15] = 0.0f;
  shader_environment_texture_animation_evaluate(shader, texture_globals,
                                                &constants[11], &constants[15]);
  D3DDevice_SetVertexShaderConstant(-0x54, &constants[4], 3);

  if (*(int *)((char *)shader_data + 0x260) == -1) {
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b94 = 2;
    *(uint32_t *)0x5a5b74 = 0x208c;
    *(uint32_t *)0x5a5ac4 = 0x34201408;
    *(uint32_t *)0x5a5b2c = 0xc00;
    *(uint32_t *)0x5a5ae0 = 0xa0f000c;
    *(uint32_t *)0x5a5ae4 = 0x1c011800;
    *(uint32_t *)0x5a5b98 =
      ((uint32_t)(*(uint8_t *)0x47dca4 == 0) << 10) | 0x18001;
    *(uint32_t *)0x5a5b48 =
      (-(uint32_t)(*(uint8_t *)0x3256ec != 0) & 0x282b0a01) + 0x20200000;
  } else {
    if (*(float *)((char *)shader_data + 0x1b8) == *(float *)0x2533c0) {
      display_assert(
        "illumination->primary_animation_period!=0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x1c0, true);
      system_exit(-1);
    }
    if (*(float *)((char *)shader_data + 0x1f4) == *(float *)0x2533c0) {
      display_assert(
        "illumination->secondary_animation_period!=0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x1c1, true);
      system_exit(-1);
    }
    if (*(float *)((char *)shader_data + 0x230) == *(float *)0x2533c0) {
      display_assert(
        "illumination->plasma_animation_period!=0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x1c2, true);
      system_exit(-1);
    }

    scale_primary = FUN_0010a5e0(
      *(uint16_t *)((char *)shader_data + 0x1b4),
      (*(float *)0x5a5e18 + *(float *)((char *)shader_data + 0x1bc)) /
        *(float *)((char *)shader_data + 0x1b8));
    scale_secondary = FUN_0010a5e0(
      *(uint16_t *)((char *)shader_data + 0x1f0),
      (*(float *)0x5a5e18 + *(float *)((char *)shader_data + 0x1f8)) /
        *(float *)((char *)shader_data + 0x1f4));
    scale_plasma = FUN_0010a5e0(
      *(uint16_t *)((char *)shader_data + 0x22c),
      (*(float *)0x5a5e18 + *(float *)((char *)shader_data + 0x234)) /
        *(float *)((char *)shader_data + 0x230));

    FUN_00012fb0((float *)((char *)shader_data + 0x1a8),
                 *(float *)0x2533c8 - scale_primary, vector_primary);
    FUN_00012fb0((float *)((char *)shader_data + 0x1e4),
                 *(float *)0x2533c8 - scale_secondary, vector_secondary);
    vector3d_scale_add(vector_primary, (float *)((char *)shader_data + 0x19c),
                       scale_primary, vector_primary);
    vector3d_scale_add(vector_secondary, (float *)((char *)shader_data + 0x1d8),
                       scale_secondary, vector_secondary);
    FUN_00012fb0((float *)((char *)shader_data + 0x214), 1.0f, plasma_a);
    FUN_00012fb0((float *)((char *)shader_data + 0x220), 1.0f, plasma_b);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b28 = 0xc00;
    *(uint32_t *)0x5a5b74 = 0xc00;
    *(uint32_t *)0x5a5b38 = 0xc00;
    *(uint32_t *)0x5a5b84 = 0xc00;
    *(uint32_t *)0x5a5b88 = 0xc00;
    *(uint32_t *)0x5a5b98 =
      ((uint32_t)(*(uint8_t *)0x47dca4 == 0) << 10) | 0x18021;
    *(uint32_t *)0x5a5b94 = 0x11106;
    *(uint32_t *)0x5a5ac0 = 0x1120b920;
    *(uint32_t *)0x5a5b48 = 0x1920b120;
    *(uint32_t *)0x5a5ac4 = 0xdcdccccc;
    *(uint32_t *)0x5a5b2c = 0x24c00;
    *(uint32_t *)0x5a5b4c =
      (-(uint32_t)(*(uint8_t *)0x3256ec != 0) & 0x282b0a01) + 0x20200000;
    *(uint32_t *)0x5a5b78 = 0x2080;
    *(uint32_t *)0x5a5af0 = 0xff0000;
    *(uint32_t *)0x5a5b10 = 0xff00;
    *(uint32_t *)0x5a5ac8 = 0x1c1c0920;
    *(uint32_t *)0x5a5b30 = 0xc9;
    *(uint32_t *)0x5a5b50 = 0x9010902;
    *(uint32_t *)0x5a5b7c = 0x30cd;
    *(uint32_t *)0x5a5acc = 0x5c5c;
    *(uint32_t *)0x5a5b34 = 0x4c00;
    *(uint32_t *)0x5a5b54 = 0xc010d02;
    *(uint32_t *)0x5a5b80 = 0xd00;
    *(uint32_t *)0x5a5ad0 = 0x34201408;
    *(uint32_t *)0x5a5b58 = 0x11c0220;
    *(uint32_t *)0x5a5b5c = 0xc190d20;
    *(uint32_t *)0x5a5ae0 = 0xa0f000c;
    *(uint32_t *)0x5a5ae4 = 0x1c011800;
    *(uint32_t *)0x5a5ae8 = FUN_00159070(scale_plasma);
    *(uint32_t *)0x5a5af4 = real_rgb_color_to_pixel32(vector_primary);
    *(uint32_t *)0x5a5b14 = real_rgb_color_to_pixel32(vector_secondary);
    *(uint32_t *)0x5a5af8 = real_rgb_color_to_pixel32(plasma_a);
    *(uint32_t *)0x5a5b18 = real_rgb_color_to_pixel32(plasma_b);
  }

  *(uint32_t *)0x5a5b6c = real_rgb_color_to_pixel32((float *)((char *)shader_data + 0x10c));

  if (*(uint16_t *)0x3256b0 == 1) {
    constants[0] = 0.5f;
    constants[1] = 0.6f;
    constants[2] = 0.6f;
    constants[3] = 1.0f;
    constants[4] = 0.0f;
    constants[5] = 0.0f;
    constants[6] = 1.0f;
    constants[7] = 0.1f;
    constants[8] = 0.9f;
    constants[9] = 0.9f;
    constants[10] = 0.8f;
    constants[11] = 0.0f;
    constants[12] = 0.1f;
    constants[13] = 0.1f;
    constants[14] = 0.3f;
    constants[15] = 0.0f;
    D3DDevice_SetVertexShaderConstant(-0x51, &constants[0], 4);
    *(uint32_t *)0x5a5ae0 = 0x2004000c;
  } else if (*(uint16_t *)0x3256b0 == 2 || *(uint16_t *)0x3256b0 == 3) {
    *(uint32_t *)0x5a5b70 = real_rgb_color_to_pixel32((float *)0x47dc98);
    *(uint32_t *)0x5a5ae0 = 0x2002000c;
  }

  switch ((int)*(int16_t *)0x3256bc) {
  case 0:
    break;
  case 2:
  case 4:
  case 5:
    *(uint32_t *)0x5a5b94 = 1;
    *(uint32_t *)0x5a5ac0 = 0;
    *(uint32_t *)0x5a5b28 = 0;
    *(uint32_t *)0x5a5b48 = 0;
    *(uint32_t *)0x5a5b74 = 0;
    *(uint32_t *)0x5a5ae0 = 8;
    break;
  case 6:
  case 7:
  case 8:
    *(uint32_t *)0x5a5b94 = 1;
    *(uint32_t *)0x5a5ac0 = 0x48402020;
    *(uint32_t *)0x5a5b28 = 0x20d00;
    *(uint32_t *)0x5a5b48 = 0;
    *(uint32_t *)0x5a5b74 = 0;
    *(uint32_t *)0x5a5ae0 = 0x1d;
    break;
  case 3:
    *(uint32_t *)0x5a5b94 = 1;
    *(uint32_t *)0x5a5ac0 = 0;
    *(uint32_t *)0x5a5b28 = 0;
    *(uint32_t *)0x5a5b48 = 0;
    *(uint32_t *)0x5a5b74 = 0;
    *(uint32_t *)0x5a5ae0 = 0x20;
    break;
  default:
    display_assert(
      "### ERROR unsupported drawing mode in environment lightmap pass",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x23f, true);
    system_exit(-1);
    break;
  }

  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  rasterizer_draw_dynamic_triangles_static_vertices2(vertices_per_primitive, a4, triangle_count, (const struct vertex_buffer *)vertex_buffer,
               (const struct vertex_buffer *)((char *)vertex_buffer + 20 * (*(uint8_t *)0x47dca4 == 0)));

  if (*(uint16_t *)0x3256ba == 2) {
    batch_count = *(int *)0x5a542c;
    triangle_total = *(int *)0x5a5428;
    *(int *)0x5a542c = batch_count + 1;
    *(int *)0x5a5428 = triangle_total + triangle_count;
    static_vertices = rasterizer_frame_statistics_count_static_vertices(
      vertices_per_primitive, a4, triangle_count);
    *(int *)0x5a5424 = *(int *)0x5a5424 + static_vertices;
  }
}

/* 0x161f00 — begin rasterizer profile section 5 and, on one narrow mode, bind
 * two lightmap/environment textures to stages 2 and 3, program their stage
 * state, then program blend/depth/color-mask render state and bind the shared
 * pixel-shader state block.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x273)
 *   0x3256bc  uint16    mode selector; this path requires exactly 0
 *   0x3256cb  uint8     enable flag (must be non-zero). NOTE: 0x3256cb, not
 *                       the 0x3256cc/0x3256cf/0x3256d0 used by the neighbours.
 *   0x476204  void *    pointer to a structure whose dword fields at +0xc and
 *                       +0x1c are the bitmap tag indices passed to
 *                       rasterizer_set_texture_direct. The binary loads the
 *                       global then dereferences it (MOV EAX,[0x476204];
 *                       MOV ECX,[EAX+0xc]), so this is a double indirection.
 *                       The identity of the structure is unproven.
 *   0x1fb7a4 / 0x1fb784 / 0x1fb790 / 0x1fb794 / 0x1fb7c0 / 0x1fb788 /
 *   0x1fb78c / 0x1fb77c / 0x1fb798
 *                       render-state shadow copies. MSVC rotates several of
 *                       these stores past the ECX/EDX loads of the NEXT call;
 *                       each store below is paired with the call it mirrors,
 *                       per the disassembly. ZEnable and ZBias have no shadow
 *                       store. 0x1fb78c (the 0x40340 shadow) is unique to this
 *                       function within this TU.
 *   0x5a5ac0  -         0xf0-byte pixel-shader state block (shared with
 *                       rasterizer_xbox_widgets/shadows/models/screen_effect)
 *
 * Call order, the guard compare order, and the 0x5a5ac0 store order are
 * binary-fixed. The single ADD ESP,0x10 after the last call is MSVC coalescing
 * csmemset's three stack args with rasterizer_set_pixel_shader's one; the
 * pixel-shader call takes one argument. */
void FUN_00161f00(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x273, true);
    system_exit(-1);
  }

  FUN_0016f910(5);

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cb != 0) {
    rasterizer_set_texture_direct(2, *(int *)(*(char **)0x476204 + 0xc), 0);
    D3DDevice_SetTextureStageState(2, 10, 4);
    D3DDevice_SetTextureStageState(2, 0xb, 4);
    D3DDevice_SetTextureStageState(2, 0xc, 4);
    D3DDevice_SetTextureStageState(2, 0xd, 2);
    D3DDevice_SetTextureStageState(2, 0xe, 1);
    D3DDevice_SetTextureStageState(2, 0xf, 1);

    rasterizer_set_texture_direct(3, *(int *)(*(char **)0x476204 + 0x1c), 0);
    D3DDevice_SetTextureStageState(3, 10, 3);
    D3DDevice_SetTextureStageState(3, 0xb, 3);
    D3DDevice_SetTextureStageState(3, 0xc, 3);
    D3DDevice_SetTextureStageState(3, 0xd, 2);
    D3DDevice_SetTextureStageState(3, 0xe, 1);
    D3DDevice_SetTextureStageState(3, 0xf, 1);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    *(unsigned long *)0x1fb7a4 = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(unsigned long *)0x1fb784 = 1;
    D3DDevice_SetRenderState_Simple(0x40344, 1);
    *(unsigned long *)0x1fb790 = 1;
    D3DDevice_SetRenderState_Simple(0x40348, 1);
    *(unsigned long *)0x1fb794 = 1;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(unsigned long *)0x1fb7c0 = 0x8006;
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(unsigned long *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0);
    *(unsigned long *)0x1fb78c = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x202);
    *(unsigned long *)0x1fb77c = 0x202;
    D3DDevice_SetRenderState_Simple(0x4035c, 0);
    *(unsigned long *)0x1fb798 = 0;
    D3DDevice_SetRenderState_ZBias(0);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned long *)0x5a5b98 = 0x18861;
    *(unsigned long *)0x5a5b94 = 4;
    *(unsigned long *)0x5a5ac0 = 0x4b204b20;
    *(unsigned long *)0x5a5b28 = 0x20c00;
    *(unsigned long *)0x5a5b48 = 0x90a484b;
    *(unsigned long *)0x5a5b74 = 0x10cd;
    *(unsigned long *)0x5a5b4c = 0xc0d0000;
    *(unsigned long *)0x5a5b78 = 0xc0;
    *(unsigned long *)0x5a5b50 = 0xc1c0000;
    *(unsigned long *)0x5a5b7c = 0xc0;
    *(unsigned long *)0x5a5b54 = 0xc010c01;
    *(unsigned long *)0x5a5b80 = 0x10cd;
    *(unsigned long *)0x5a5ae0 = 0xc010000;
    *(unsigned long *)0x5a5ae4 = 0xd00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
  }
}

/* Light definition fields read by 0x1621c0.  The three period names come
 * from the assert strings at __FILE__ lines 0x2ec..0x2ee
 * ("light->definition->gel.yaw_period>0.0f" and the pitch/roll forms).  Each
 * *_function word is the short passed to FUN_0010a5e0 together with its
 * period, at -2 from that period.  The enclosing gel sub-struct start is
 * unproven, so the fields are flat here.
 *   +0x70  dword passed to rasterizer_set_texture as bitmap_tag_index */
typedef struct {
  char pad_00[0x70];
  int32_t field_70;       /* offset=0x70 */
  char pad_74[0x1a];
  int16_t yaw_function;   /* offset=0x8e */
  real yaw_period;        /* offset=0x90 */
  char pad_94[2];
  int16_t roll_function;  /* offset=0x96 */
  real roll_period;       /* offset=0x98 */
  char pad_9c[2];
  int16_t pitch_function; /* offset=0x9e */
  real pitch_period;      /* offset=0xa0 */
} rasterizer_light_definition_t;
co(rasterizer_light_definition_t, field_70, 0x70);
co(rasterizer_light_definition_t, yaw_function, 0x8e);
co(rasterizer_light_definition_t, yaw_period, 0x90);
co(rasterizer_light_definition_t, roll_function, 0x96);
co(rasterizer_light_definition_t, roll_period, 0x98);
co(rasterizer_light_definition_t, pitch_function, 0x9e);
co(rasterizer_light_definition_t, pitch_period, 0xa0);

/* One rasterizer_lights entry: base 0x5a37e4, stride 0x38
 * (IMUL ESI,ESI,0x38 at 0x16223d), count at 0x5a37e0.
 *   +0x00  definition  (assert text "light->definition->gel...")
 *   +0x04  position    three dwords copied raw into vs constant 0 .xyz
 *   +0x10  forward     rotated into the 3x3 frame's forward row
 *   +0x1c  up          rotated into the 3x3 frame's up row
 *   +0x28  color       passed to real_rgb_color_to_pixel32
 *   +0x34  radius      (assert text "light->radius>0.0f") */
typedef struct {
  rasterizer_light_definition_t *definition; /* offset=0x00 */
  real_point3d position;                     /* offset=0x04 */
  real_vector3d forward;                     /* offset=0x10 */
  real_vector3d up;                          /* offset=0x1c */
  real_rgb_color color;                      /* offset=0x28 */
  real radius;                               /* offset=0x34 */
} rasterizer_light_t;
cs(rasterizer_light_t, 0x38);
co(rasterizer_light_t, position, 0x04);
co(rasterizer_light_t, forward, 0x10);
co(rasterizer_light_t, up, 0x1c);
co(rasterizer_light_t, color, 0x28);
co(rasterizer_light_t, radius, 0x34);

/* 0x1621c0 — set up the environment diffuse-light gel pass for one rasterizer
 * light: bind the light definition's gel bitmap to stage 1 (clamp, linear),
 * rotate the light's forward/up by yaw/pitch/roll taken from three periodic
 * functions of game time, upload position + 0.5/radius and the negated basis
 * as five vertex-shader constants at -0x51, and set render state 0x40a60
 * (shadowed at 0x1fb6c0) to the light color as a pixel32.
 *
 * Signature: sole caller 0x17cc60 is a tail-jump thunk forwarding its own
 * stack argument; the body reads [EBP+8] (MOV ESI,[EBP+8] at 0x16220e) and
 * returns with a bare RET, so cdecl with one int.  Assert text names it
 * light_index.
 *
 * Frame: 0x80 bytes.  The 3x3 frame lives at EBP-0x30 (forward -0x30, left
 * -0x24, up -0x18), the angles at -0xc/-0x8/-0x4 and the 4x3 rotation at
 * -0x64; the 20-float constant buffer at -0x80 reuses the rotation's slots
 * after it dies, so the two live in disjoint inner blocks.
 *
 * The cross product is inlined (no CALL): left = forward x up, each component
 * stored straight into the frame's left row, then normalize3d (result
 * discarded, FSTP ST0 at 0x162484).
 *
 * Globals:
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x2d3)
 *   0x3256bc  uint16  mode selector; only value 0 is accepted
 *   0x3256cb  uint8   enable flag (must be non-zero)
 *   0x5a5e18  float   game time in seconds (also read by
 *                     rasterizer_xbox_environment_fog.c)
 *   0x2533c0 = 0.0f, 0x255a54 = 2*pi, 0x253398 = 0.5f */
void FUN_001621c0(int light_index)
{
  rasterizer_light_t *light;
  real_matrix3x3 gel_matrix;
  unsigned int color;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x2d3, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cb != 0) {
    if (light_index < 0 || light_index >= *(int *)0x5a37e0) {
      display_assert(
        "light_index>=0 && light_index<rasterizer_lights.light_count",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x2d9, true);
      system_exit(-1);
    }

    light = (rasterizer_light_t *)0x5a37e4 + light_index;

    if (!(light->radius > *(float *)0x2533c0)) {
      display_assert(
        "light->radius>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x2dc, true);
      system_exit(-1);
    }

    rasterizer_set_texture(1, 2, 1, light->definition->field_70, 0);
    D3DDevice_SetTextureStageState(1, 10, 3);
    D3DDevice_SetTextureStageState(1, 0xb, 3);
    D3DDevice_SetTextureStageState(1, 0xc, 3);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);

    if (!(light->definition->yaw_period > *(float *)0x2533c0)) {
      display_assert(
        "light->definition->gel.yaw_period>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x2ec, true);
      system_exit(-1);
    }
    if (!(light->definition->pitch_period > *(float *)0x2533c0)) {
      display_assert(
        "light->definition->gel.pitch_period>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x2ed, true);
      system_exit(-1);
    }
    if (!(light->definition->roll_period > *(float *)0x2533c0)) {
      display_assert(
        "light->definition->gel.roll_period>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x2ee, true);
      system_exit(-1);
    }

    {
      {
        real yaw = FUN_0010a5e0(light->definition->yaw_function,
                                *(float *)0x5a5e18 /
                                  light->definition->yaw_period) *
                   *(float *)0x255a54;
        real pitch = FUN_0010a5e0(light->definition->pitch_function,
                                  *(float *)0x5a5e18 /
                                    light->definition->pitch_period) *
                     *(float *)0x255a54;
        real roll = FUN_0010a5e0(light->definition->roll_function,
                                 *(float *)0x5a5e18 /
                                   light->definition->roll_period) *
                    *(float *)0x255a54;
        real_matrix4x3 rotation;

        matrix4x3_rotation_from_angles((float *)&rotation, yaw, pitch, roll);
        matrix_transform_vector((float *)&rotation, &light->forward,
                                &gel_matrix.forward);
        matrix_transform_vector((float *)&rotation, &light->up,
                                &gel_matrix.up);
      }

      gel_matrix.left.i = gel_matrix.up.k * gel_matrix.forward.j -
                          gel_matrix.up.j * gel_matrix.forward.k;
      gel_matrix.left.j = gel_matrix.up.i * gel_matrix.forward.k -
                          gel_matrix.up.k * gel_matrix.forward.i;
      gel_matrix.left.k = gel_matrix.up.j * gel_matrix.forward.i -
                          gel_matrix.up.i * gel_matrix.forward.j;
      normalize3d(&gel_matrix.left);

      {
        real vertex_constants[20];

        vertex_constants[0] = light->position.x;
        vertex_constants[1] = light->position.y;
        vertex_constants[2] = light->position.z;
        vertex_constants[3] = 0.5f / light->radius;
        vertex_constants[4] = -gel_matrix.forward.i;
        vertex_constants[5] = -gel_matrix.forward.j;
        vertex_constants[6] = -gel_matrix.forward.k;
        vertex_constants[7] = 1.0f;
        vertex_constants[8] = -gel_matrix.left.i;
        vertex_constants[9] = -gel_matrix.left.j;
        vertex_constants[10] = -gel_matrix.left.k;
        vertex_constants[11] = 1.0f;
        vertex_constants[12] = -gel_matrix.up.i;
        vertex_constants[13] = -gel_matrix.up.j;
        vertex_constants[14] = -gel_matrix.up.k;
        vertex_constants[15] = 1.0f;
        vertex_constants[16] = 0.0f;
        vertex_constants[17] = 0.0f;
        vertex_constants[18] = 0.0f;
        vertex_constants[19] = 1.0f;
        D3DDevice_SetVertexShaderConstant(-0x51, vertex_constants, 5);
      }
    }

    color = real_rgb_color_to_pixel32((float *)&light->color);
    D3DDevice_SetRenderState_Simple(0x40a60, color);
    *(unsigned long *)0x1fb6c0 = color;
  }
}

/* 0x162560 — draw one environment geometry batch with the shader's lightmap
 * bitmap (or no bitmap) bound to texture stage 0.
 *
 * Signature: cdecl, bare RET (caller cleans). Six dword parameter slots are
 * read from the frame: [EBP+8] shader, [EBP+0xc] (forwarded to
 * rasterizer_set_texture's frame_index slot), [EBP+0x10], [EBP+0x14],
 * [EBP+0x18] and [EBP+0x1c]. The last three forwarded slots land in
 * rasterizer_draw_dynamic_triangles_static_vertices's kb decl positions vertices_per_primitive / a2 /
 * triangle_count and in rasterizer_frame_statistics_count_static_vertices'
 * identically named slots; the names here are taken from those decls, not
 * from independent evidence. [EBP+0x1c] is asserted non-NULL under the string
 * "vertex_buffer", is read as a uint16 at offset 0 (the vertex type passed to
 * FUN_00178b40) and is forwarded as-is (no +0x14 form here, unlike
 * FUN_001609b0/FUN_001640d0). [EBP+0xc] is loaded with MOV ECX,dword ptr
 * [EBP+0xc], so the slot is a 32-bit type; it is declared int and passed
 * straight through.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x333)
 *   0x3256bc  uint16  mode selector; this path requires exactly 0
 *   0x3256cb  uint8   single enable flag (must be non-zero). This function
 *                     tests only these two — there is no 0x3256b0 or
 *                     0x47dca4 test here.
 *   0x3256ba  uint16  statistics mode; the counters below run only when it
 *                     is 2. NOTE: 0x3256ba, not the 0x3256bc of the guard.
 *   0x5a5e18  dword   value forwarded unchanged to
 *                     shader_environment_texture_animation_evaluate; the
 *                     reference copies it with MOV EAX / PUSH EAX, so its
 *                     type is unproven.
 *   0x1fb744  dword   shadow copy of render state 0x41e20, stored after the
 *                     matching D3DDevice_SetRenderState_Simple call.
 *   0x5a5448 / 0x5a5444 / 0x5a5440
 *                     frame statistic counters: batch count, triangle_count
 *                     accumulator, and the accumulator fed by
 *                     rasterizer_frame_statistics_count_static_vertices.
 *
 * Shader-data offsets (from FUN_001906b0(shader, 3)); all read directly in
 * the disassembly:
 *   +0x28            flag byte; bit 1 set means "no bitmap" (tag index -1)
 *   +0x134           bitmap tag index bound to stage 0 when bit 1 is clear
 *   +0x138 / +0x13c  the two animated values written into constant row 0
 *   +0x10c           color vector passed to real_rgb_color_to_pixel32
 *
 * There is no float compare anywhere in this function (no FCOM in the
 * reference): the draw is gated only by the two global tests.
 *
 * The 12-dword block is one contiguous local (SUB ESP,0x30; every slot from
 * EBP-0x30 to EBP-0x4 is written) uploaded as 3 vertex-shader constants at
 * register -0x54. Slots 7 and 11 are the trailing components of constant
 * rows 1 and 2; their addresses are handed to the texture-animation
 * evaluator as out-parameters.
 *
 * Call-site arity note: the reference emits one ADD ESP,0x10 after
 * CALL 0x178b40 that also cleans the PUSH EDI belonging to the preceding
 * CALL 0x190710 — FUN_00178b40 still takes 3 cdecl arguments.
 *
 * Call order, the guard compare order, and the counter update order (both
 * accumulator stores precede the count_static_vertices call) are binary-fixed.
 */
void FUN_00162560(void *shader, int frame_index, int vertices_per_primitive,
                  int a4, int triangle_count, void *vertex_buffer)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  int permutation;
  int bitmap_tag_index;
  uint32_t render_state;
  int static_vertices;
  int batch_count;
  int triangle_total;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x333, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cb != 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x33a, true);
      system_exit(-1);
    }

    shader_data = FUN_001906b0(shader, 3);

    /* The vertex_buffer assert follows the FUN_001906b0 call in the
     * reference (MOV EBX,[EBP+0x1c] is issued after CALL 0x1906b0). */
    if (vertex_buffer == 0) {
      display_assert(
        "vertex_buffer",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x33f, true);
      system_exit(-1);
    }

    permutation = shader_get_vertex_shader_permutation(shader);
    FUN_00178b40(0x31, *(uint16_t *)vertex_buffer, permutation);

    /* MOV EAX,-1 then a conditional MOV EAX,[ESI+0x134]; written as an
     * init-then-overwrite, not a ternary, to keep the reference's shape. */
    bitmap_tag_index = -1;
    if ((*(uint8_t *)((char *)shader_data + 0x28) & 2) == 0) {
      bitmap_tag_index = *(int *)((char *)shader_data + 0x134);
    }

    rasterizer_set_texture(0, 0, 3, bitmap_tag_index, frame_index);

    D3DDevice_SetTextureStageState(0, 10, 1);
    D3DDevice_SetTextureStageState(0, 0xb, 1);
    D3DDevice_SetTextureStageState(0, 0xd, 2);
    D3DDevice_SetTextureStageState(0, 0xe, 2);
    D3DDevice_SetTextureStageState(0, 0xf, 2);

    /* [0]/[1] are copied as raw dwords: the reference moves them through
     * GPRs (MOV EDX,[ESI+0x138] / MOV [EBP-0x30],EDX), not the x87 stack,
     * and they precede the literal stores. */
    *(uint32_t *)&constants[0] = *(uint32_t *)((char *)shader_data + 0x138);
    *(uint32_t *)&constants[1] = *(uint32_t *)((char *)shader_data + 0x13c);
    /* MOV EAX,[0x5a5e18] precedes MOV [EBP-0x28],0x3f800000, which lets the
     * reference push all four arguments ahead of the block. */
    texture_globals = *(void **)0x5a5e18;
    constants[2] = 1.0f;
    constants[3] = 1.0f;
    constants[4] = 1.0f;
    constants[5] = 0.0f;
    constants[6] = 0.0f;
    constants[7] = 0.0f;
    constants[8] = 0.0f;
    constants[9] = 1.0f;
    constants[10] = 0.0f;
    constants[11] = 0.0f;

    shader_environment_texture_animation_evaluate(
      shader, texture_globals, &constants[7], &constants[11]);

    D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

    render_state = real_rgb_color_to_pixel32((float *)((char *)shader_data + 0x10c));
    D3DDevice_SetRenderState_Simple(0x41e20, render_state);
    *(uint32_t *)0x1fb744 = render_state;

    rasterizer_draw_dynamic_triangles_static_vertices(vertices_per_primitive, a4, triangle_count, vertex_buffer);

    if (*(uint16_t *)0x3256ba == 2) {
      batch_count = *(int *)0x5a5448;
      triangle_total = *(int *)0x5a5444;
      *(int *)0x5a5448 = batch_count + 1;
      *(int *)0x5a5444 = triangle_total + triangle_count;
      /* The reference loads 0x5a5440 only after the call returns, so the
       * call must not be sequenced against a pending read of it. */
      static_vertices = rasterizer_frame_statistics_count_static_vertices(
        vertices_per_primitive, a4, triangle_count);
      *(int *)0x5a5440 = *(int *)0x5a5440 + static_vertices;
    }
  }
}

/* 0x162790 — begin rasterizer profile section 8 and, for a set of accepted
 * modes, program blend/depth/color-mask render state.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x399)
 *   0x3256bc  uint16    mode selector; accepted values 0,1,3,4,7,5,8
 *                       (compare order is binary-fixed). The reference
 *                       re-reads this global after each D3D call rather than
 *                       caching it, so every use below is a direct read.
 *   0x3256cc  uint8     enable flag (must be non-zero)
 *   0x3256f5  uint8     alpha-write flag; selects RGBA vs RGB color mask
 *                       (binary: NEG AL / SBB EAX,EAX / AND 0x1000000 / ADD
 * 0x10101) 0x1fb7a4 / 0x1fb784 / 0x1fb790 / 0x1fb794 / 0x1fb7c0 / 0x1fb788 /
 *   0x1fb77c / 0x1fb798
 *                       render-state shadow copies. MSVC rotates these stores
 *                       ahead of the following call; each store below is paired
 *                       with the call it mirrors, per the disassembly.
 * Call order is binary-fixed. */
void FUN_00162790(void)
{
  unsigned long value;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x399, true);
    system_exit(-1);
  }

  FUN_0016f910(8);

  if ((*(uint16_t *)0x3256bc == 0 || *(uint16_t *)0x3256bc == 1 ||
       *(uint16_t *)0x3256bc == 3 || *(uint16_t *)0x3256bc == 4 ||
       *(uint16_t *)0x3256bc == 7 || *(uint16_t *)0x3256bc == 5 ||
       *(uint16_t *)0x3256bc == 8) &&
      *(uint8_t *)0x3256cc != 0) {
    D3DDevice_SetRenderState_CullMode(0x901);

    value = (*(uint8_t *)0x3256f5 != 0) ? NV097_COLOR_MASK_RGBA :
                                          NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, value);
    *(unsigned long *)0x1fb7a4 = value;

    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(unsigned long *)0x1fb784 = 1;

    value = (*(uint16_t *)0x3256bc != 1) ? 0x306 : 1;
    D3DDevice_SetRenderState_Simple(0x40344, value);
    *(unsigned long *)0x1fb790 = value;

    value = (*(uint16_t *)0x3256bc == 1) ? 1 : 0;
    D3DDevice_SetRenderState_Simple(0x40348, value);
    *(unsigned long *)0x1fb794 = value;

    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(unsigned long *)0x1fb7c0 = 0x8006;

    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(unsigned long *)0x1fb788 = 0;

    D3DDevice_SetRenderState_ZEnable(*(uint16_t *)0x3256bc != 1);

    D3DDevice_SetRenderState_Simple(0x40354, 0x202);
    *(unsigned long *)0x1fb77c = 0x202;

    D3DDevice_SetRenderState_Simple(0x4035c, 0);
    *(unsigned long *)0x1fb798 = 0;

    D3DDevice_SetRenderState_ZBias(0);

    FUN_00158ae0(5);
  }
}

/* 0x162920 — draw one environment geometry batch for the environment texture
 * pass: bind four shader bitmaps to stages 0-3, upload the per-stage texture
 * scale rows, program the 0x5a5ac0 pixel-shader block from three shader enums
 * and the drawing mode, then draw.
 *
 * Signature: cdecl, bare RET, six dword stack slots; the only caller is the
 * tail-call wrapper FUN_0017cd30 (PUSH EBP / MOV EBP,ESP / POP EBP /
 * JMP 0x162920), so the arguments are forwarded unchanged. The slot roles
 * mirror the sibling FUN_00162560: [EBP+8] shader (asserted "shader",
 * handed to FUN_001906b0 and shader_get_vertex_shader_permutation),
 * [EBP+0xc] frame_index (every rasterizer_set_texture call),
 * [EBP+0x10]/[EBP+0x14]/[EBP+0x18] forwarded to
 * rasterizer_draw_dynamic_triangles_static_vertices and
 * rasterizer_frame_statistics_count_static_vertices; [EBP+0x18] is also the
 * triangle accumulator added to 0x5a5464; [EBP+0x1c] vertex_buffer
 * (asserted, uint16 vertex type at +0). The kb decl keeps them as `int`,
 * which is ABI-identical; they are cast here.
 *
 * Globals (roles unproven beyond the assert strings):
 *   0x476ab0  global_d3d_device (assert line 0x3c8)
 *   0x3256bc  uint16 drawing mode; accepted 0,1,3,4,7,5,8 (binary compare
 *             order); the final switch uses the byte map at 0x162f78:
 *             0,5,8 -> 0xc; 1 -> 0xc110000 plus FUN_00159070(0.33f) into
 *             0x5a5b6c; 3,4,7 -> 0x1c; 2,6,>8 -> assert line 0x4b6.
 *   0x3256cc  uint8 enable flag
 *   0x3256ba  uint16 statistics mode (counters only when 2)
 *   0x5a5468 / 0x5a5464 / 0x5a5460  batch / triangle / static-vertex
 *             counters
 *
 * Shader-data offsets (FUN_001906b0(shader, 3)):
 *   +0x2a short  shader type (0/1/2, else assert 0x479)
 *   +0x6c bit 0  scale stages 1-3 by stage-0 bitmap size ratios
 *   +0x94 / +0xc4 / +0xd8 / +0x108  bitmap tag indices, stages 0..3
 *   +0xb0 / +0xf4 short  detail functions (0/1/2, else assert 0x48b / 0x4a0)
 *   +0xb4 / +0xc8 / +0xf8 float  per-stage scales
 * rasterizer_set_texture returns the bitmap data; its first dword is read
 * as two int16 halves (MOVSX word [x], MOVSX word [x+2]) and every stage-N
 * ratio is stage0 / stageN per half (FILD / FDIVR sequence at 0x162bbb).
 * The x87 keeps the last ratio (stage-3 high half) in ST0; the else arm
 * loads 1.0 there.
 *
 * Constant block: 12 contiguous floats at EBP-0x54 uploaded as 3 rows to
 * register -0x54. &constants[7] (EBP-0x38) and &constants[11] (EBP-0x28)
 * go to shader_environment_texture_animation_evaluate as out-params.
 */
void FUN_00162920(int shader_arg, int frame_index_arg,
                  int vertices_per_primitive, int a4, int triangle_count,
                  int vertex_buffer_arg)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  /* bitmap size: two int16 halves read back with MOVSX word */
  struct fun_00162920_size {
    int16_t x;
    int16_t y;
  } size0, size1, size2, size3; /* EBP-0x04 / -0x08 / -0x10 / -0x1c */
  real_vector2d stage1; /* EBP-0x24 */
  real_vector2d stage2; /* EBP-0x0c */
  real_vector2d stage3; /* EBP-0x18; .j stays in ST0 */
  int permutation;
  int static_vertices;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x3c8, true);
    system_exit(-1);
  }

  if (!(*(uint16_t *)0x3256bc == 0 || *(uint16_t *)0x3256bc == 1 ||
        *(uint16_t *)0x3256bc == 3 || *(uint16_t *)0x3256bc == 4 ||
        *(uint16_t *)0x3256bc == 7 || *(uint16_t *)0x3256bc == 5 ||
        *(uint16_t *)0x3256bc == 8)) {
    return;
  }
  if (*(uint8_t *)0x3256cc == 0) {
    return;
  }

  if ((void *)shader_arg == 0) {
    display_assert(
      "shader",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x3d5, true);
    system_exit(-1);
  }
  shader_data = FUN_001906b0((void *)shader_arg, 3);
  if ((void *)vertex_buffer_arg == 0) {
    display_assert(
      "vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x3e3, true);
    system_exit(-1);
  }

  permutation = shader_get_vertex_shader_permutation((void *)shader_arg);
  FUN_00178b40(0x28, *(uint16_t *)vertex_buffer_arg, permutation);

  size0 = *(struct fun_00162920_size *)rasterizer_set_texture(
    0, 0, 1, *(int *)((char *)shader_data + 0x94), frame_index_arg);
  D3DDevice_SetTextureStageState(0, 10, 1);
  D3DDevice_SetTextureStageState(0, 0xb, 1);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);

  size1 = *(struct fun_00162920_size *)rasterizer_set_texture(
    1, 0, 2, *(int *)((char *)shader_data + 0xc4), frame_index_arg);
  D3DDevice_SetTextureStageState(1, 10, 1);
  D3DDevice_SetTextureStageState(1, 0xb, 1);
  D3DDevice_SetTextureStageState(1, 0xd, 2);
  D3DDevice_SetTextureStageState(1, 0xe, 2);
  D3DDevice_SetTextureStageState(1, 0xf, 2);

  size2 = *(struct fun_00162920_size *)rasterizer_set_texture(
    2, 0, 2, *(int *)((char *)shader_data + 0xd8), frame_index_arg);
  D3DDevice_SetTextureStageState(2, 10, 1);
  D3DDevice_SetTextureStageState(2, 0xb, 1);
  D3DDevice_SetTextureStageState(2, 0xd, 2);
  D3DDevice_SetTextureStageState(2, 0xe, 2);
  D3DDevice_SetTextureStageState(2, 0xf, 2);

  size3 = *(struct fun_00162920_size *)rasterizer_set_texture(
    3, 0, 2, *(int *)((char *)shader_data + 0x108), frame_index_arg);
  D3DDevice_SetTextureStageState(3, 10, 1);
  D3DDevice_SetTextureStageState(3, 0xb, 1);
  D3DDevice_SetTextureStageState(3, 0xd, 2);
  D3DDevice_SetTextureStageState(3, 0xe, 2);
  D3DDevice_SetTextureStageState(3, 0xf, 2);

  if (*(uint8_t *)((char *)shader_data + 0x6c) & 1) {
    stage1.i = (float)size0.x / (float)size1.x;
    stage1.j = (float)size0.y / (float)size1.y;
    stage2.i = (float)size0.x / (float)size2.x;
    stage2.j = (float)size0.y / (float)size2.y;
    stage3.i = (float)size0.x / (float)size3.x;
    stage3.j = (float)size0.y / (float)size3.y;
  } else {
    stage3.j = 1.0f;
    stage3.i = 1.0f;
    stage2.j = 1.0f;
    stage2.i = 1.0f;
    stage1.j = 1.0f;
    stage1.i = 1.0f;
  }

  texture_globals = *(void **)0x5a5e18;
  constants[0] = stage1.i * *(float *)((char *)shader_data + 0xb4);
  constants[1] = stage1.j * *(float *)((char *)shader_data + 0xb4);
  constants[4] = 1.0f;
  constants[5] = 0.0f;
  constants[7] = 0.0f;
  constants[8] = 0.0f;
  constants[2] = stage2.i * *(float *)((char *)shader_data + 0xc8);
  constants[9] = 1.0f;
  constants[11] = 0.0f;
  constants[3] = stage2.j * *(float *)((char *)shader_data + 0xc8);
  constants[6] = stage3.i * *(float *)((char *)shader_data + 0xf8);
  constants[10] = stage3.j * *(float *)((char *)shader_data + 0xf8);
  shader_environment_texture_animation_evaluate(
    (void *)shader_arg, texture_globals, &constants[7], &constants[11]);

  D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0x8421;
  *(uint32_t *)0x5a5b94 = 3;

  switch (*(int16_t *)((char *)shader_data + 0x2a)) {
  case 0:
    *(uint32_t *)0x5a5ac0 = 0x3a1a1a19;
    *(uint32_t *)0x5a5b48 = 0x3a0a1a09;
    *(uint32_t *)0x5a5ac4 = 0x181c0000;
    *(uint32_t *)0x5a5b2c = 0xc0;
    *(uint32_t *)0x5a5b28 = 0xc00;
    *(uint32_t *)0x5a5b74 = 0xc00;
    break;
  case 1:
    *(uint32_t *)0x5a5ac0 = 0x381a1819;
    *(uint32_t *)0x5a5b48 = 0x380a1809;
    *(uint32_t *)0x5a5b2c = 0;
    *(uint32_t *)0x5a5b28 = 0xc00;
    *(uint32_t *)0x5a5b74 = 0xc00;
    break;
  case 2:
    *(uint32_t *)0x5a5ac0 = 0x18200000;
    *(uint32_t *)0x5a5b28 = 0xc0;
    *(uint32_t *)0x5a5b48 = 0x380a1809;
    *(uint32_t *)0x5a5b2c = 0;
    *(uint32_t *)0x5a5b74 = 0xc00;
    break;
  default:
    display_assert(
      "### ERROR unsupported environment shader type",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x479, true);
    system_exit(-1);
    break;
  }

  switch (*(int16_t *)((char *)shader_data + 0xb0)) {
  case 0:
    *(uint32_t *)0x5a5b4c = 0x80c080c;
    *(uint32_t *)0x5a5b78 = 0xc00;
    break;
  case 1:
    *(uint32_t *)0x5a5b4c = 0x80c0000;
    *(uint32_t *)0x5a5b78 = 0xc00;
    break;
  case 2:
    *(uint32_t *)0x5a5b4c = 0x8204c20;
    *(uint32_t *)0x5a5b78 = 0xc00;
    break;
  default:
    display_assert(
      "### ERROR unsupported environment shader detail function",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x48b, true);
    system_exit(-1);
    break;
  }
  *(uint32_t *)0x5a5ac8 = 0x1c1b0000;
  *(uint32_t *)0x5a5b30 = 0xc0;

  switch (*(int16_t *)((char *)shader_data + 0xf4)) {
  case 0:
    *(uint32_t *)0x5a5b50 = 0xc0b0c0b;
    *(uint32_t *)0x5a5b7c = 0xc00;
    break;
  case 1:
    *(uint32_t *)0x5a5b50 = 0xc0b0000;
    *(uint32_t *)0x5a5b7c = 0xc00;
    break;
  case 2:
    *(uint32_t *)0x5a5b50 = 0xc204b20;
    *(uint32_t *)0x5a5b7c = 0xc00;
    break;
  default:
    display_assert(
      "### ERROR unsupported environment shader detail function",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x4a0, true);
    system_exit(-1);
    break;
  }

  *(uint32_t *)0x5a5ae4 = 0x1c00;
  switch (*(int16_t *)0x3256bc) {
  case 0:
  case 5:
  case 8:
    *(uint32_t *)0x5a5ae0 = 0xc;
    break;
  case 1:
    *(uint32_t *)0x5a5b6c = FUN_00159070(0.33f);
    *(uint32_t *)0x5a5ae0 = 0xc110000;
    break;
  case 3:
  case 4:
  case 7:
    *(uint32_t *)0x5a5ae0 = 0x1c;
    break;
  default:
    display_assert(
      "### ERROR unsupported drawing mode in environment texture pass",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x4b6, true);
    system_exit(-1);
  }

  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  rasterizer_draw_dynamic_triangles_static_vertices(
    vertices_per_primitive, a4, triangle_count,
    (void *)vertex_buffer_arg);

  if (*(uint16_t *)0x3256ba == 2) {
    *(int *)0x5a5468 = *(int *)0x5a5468 + 1;
    *(int *)0x5a5464 = *(int *)0x5a5464 + triangle_count;
    static_vertices = rasterizer_frame_statistics_count_static_vertices(
      vertices_per_primitive, a4, triangle_count);
    *(int *)0x5a5460 = *(int *)0x5a5460 + static_vertices;
  }
}

/* 0x162f90 — begin rasterizer profile section 0xb and, on the accepted mode,
 * bind the same bitmap to texture stages 2 and 3 and program their stage
 * states plus the shared render state.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x4e5)
 *   0x3256bc  uint16  mode selector; only value 0 is accepted (a direct
 *                     CMP word ptr — the reference never loads it into a
 *                     register, so it is not cached in a local here)
 *   0x3256ce  uint8   enable flag (must be non-zero)
 *   0x476204  ptr     rasterizer globals; +0x1c is a bitmap tag index. The
 *                     reference re-loads the global before each of the two
 *                     rasterizer_set_texture_direct calls (EAX then EDX), so
 *                     it is read twice below rather than hoisted.
 *   0x1fb7a4 / 0x1fb784 / 0x1fb790 / 0x1fb794 / 0x1fb7c0 / 0x1fb788 /
 *   0x1fb78c / 0x1fb77c / 0x1fb798
 *                     render-state shadow copies, each stored next to the
 *                     matching D3DDevice_SetRenderState_Simple call.
 * Call order and the interleaving of the shadow stores are binary-fixed. */
void FUN_00162f90(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x4e5, true);
    system_exit(-1);
  }

  FUN_0016f910(0xb);

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256ce != 0) {
    rasterizer_set_texture_direct(2, *(int *)(*(int *)0x476204 + 0x1c), 0);

    D3DDevice_SetTextureStageState(2, 10, 3);
    D3DDevice_SetTextureStageState(2, 0xb, 3);
    D3DDevice_SetTextureStageState(2, 0xc, 3);
    D3DDevice_SetTextureStageState(2, 0xd, 2);
    D3DDevice_SetTextureStageState(2, 0xe, 2);
    D3DDevice_SetTextureStageState(2, 0xf, 2);

    rasterizer_set_texture_direct(3, *(int *)(*(int *)0x476204 + 0x1c), 0);

    D3DDevice_SetTextureStageState(3, 10, 3);
    D3DDevice_SetTextureStageState(3, 0xb, 3);
    D3DDevice_SetTextureStageState(3, 0xc, 3);
    D3DDevice_SetTextureStageState(3, 0xd, 2);
    D3DDevice_SetTextureStageState(3, 0xe, 2);
    D3DDevice_SetTextureStageState(3, 0xf, 2);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    *(unsigned long *)0x1fb7a4 = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(unsigned long *)0x1fb784 = 1;
    D3DDevice_SetRenderState_Simple(0x40344, 0x304);
    *(unsigned long *)0x1fb790 = 0x304;
    D3DDevice_SetRenderState_Simple(0x40348, 1);
    *(unsigned long *)0x1fb794 = 1;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(unsigned long *)0x1fb7c0 = 0x8006;
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(unsigned long *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0);
    *(unsigned long *)0x1fb78c = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x202);
    *(unsigned long *)0x1fb77c = 0x202;
    D3DDevice_SetRenderState_Simple(0x4035c, 0);
    *(unsigned long *)0x1fb798 = 0;
    D3DDevice_SetRenderState_ZBias(0);
  }
}

/* 0x1631d0 — set up the gel-mapped (spot) specular lighting pass for one
 * rasterizer light: upload five vertex-shader constants built from the
 * light's position and orientation basis, bind the gel bitmap to texture
 * stage 1, program that stage, and install the pixel shader built in the
 * shared 0x5a5ac0 block.  Only caller: FUN_00163590, on its deferred path.
 *
 * Signature: MOV ESI,EAX at 0x1631da is the only read of the argument
 * (@<eax>); the function ends in a bare RET.  The assert string at 0x525
 * names it light_index.
 *
 * Light record: base 0x5a37e4, stride 0x38 (IMUL ESI,ESI,0x38 at 0x16324c),
 * count at 0x5a37e0.  Offsets read here:
 *   +0x00  owner light definition (char *, see below)
 *   +0x04..+0x0c  three dwords copied verbatim into vs constant 0 (raw MOVs)
 *   +0x10  vector copied as raw dwords (forward), negated into constant 1 and
 *          scaled into constant 4
 *   +0x1c  vector copied as raw dwords (up), negated into constant 3
 *   +0x34  radius (assert "light->radius>0.0f" at 0x528:
 *          FLD/FCOMP [0x2533c0] / TEST AH,0x41 / JZ)
 * Owner fields: +0x70 bitmap tag index, replaced by +0x88 when it is -1
 * (CMP EDI,-1 at 0x16328c); +0x24 is light_definition_t's
 * specular_radius_multiplier (FLD [EAX+0x24] at 0x1632ff).  The owner is
 * re-read from the record for that FLD (MOV EAX,[ESI] at 0x1632fd).
 *
 * The side vector is the cross product forward x up, written inline (the
 * three FSUBP blocks at 0x163297..0x1632f3 store i, j, k in order), and is
 * then normalized in place (CALL 0x13010; the result is discarded with
 * FSTP ST0).  The cross product reads the record through two pointers
 * (LEA EAX,[ESI+0x10] / LEA ECX,[ESI+0x1c]), not the stack copies.
 *
 * Stack: a real_matrix4x3 local spans EBP-0x34..-0x1 (SUB ESP,0x84 =
 * 0x50 vertex constants + 0x34 matrix).  Only its forward (-0x30), left
 * (-0x24) and up (-0x18) rows are written; scale (-0x34) and position
 * (-0xc) are never touched.
 *
 * Constants: 0x2533c0 = 0.0f, 0x253398 = 0.5f, 0x2533c8 = 1.0f.
 * The 0x5a5ac0 store order and the call order are binary-fixed. */
void FUN_001631d0(int light_index)
{
  /* Same 0x38-byte record as rasterizer_light_t; vector3_t rows here so the
   * forward/up copies assign straight into real_matrix4x3's rows. */
  struct point_light_t {
    char *owner;
    real_point3d position;
    vector3_t forward;
    vector3_t up;
    float field_28[3];
    float radius;
  };
  const struct point_light_t *light;
  int gel_bitmap_index;
  real_matrix4x3 matrix;
  float vertex_constants[20];
  float radius;
  float cone_scale;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x51f, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256ce != 0) {
    if (light_index < 0 || light_index >= *(int *)0x5a37e0) {
      display_assert(
        "light_index>=0 && light_index<rasterizer_lights.light_count",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x525, true);
      system_exit(-1);
    }

    light = (const struct point_light_t *)0x5a37e4 + light_index;

    if (!(light->radius > 0.0f)) {
      display_assert(
        "light->radius>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x528, true);
      system_exit(-1);
    }

    gel_bitmap_index = *(int *)(light->owner + 0x70);
    if (gel_bitmap_index == NONE) {
      gel_bitmap_index = *(int *)(light->owner + 0x88);
    }

    matrix.forward = *(const real_vector3d *)&light->forward;
    matrix.up = *(const real_vector3d *)&light->up;
    {
      const vector3_t *a = &light->forward;
      const vector3_t *b = &light->up;
      matrix.left.i = a->y * b->z - a->z * b->y;
      matrix.left.j = a->z * b->x - a->x * b->z;
      matrix.left.k = a->x * b->y - a->y * b->x;
    }
    normalize3d(&matrix.left);

    radius = *(float *)(light->owner + 0x24) * light->radius;
    cone_scale = 1.0f / (radius - radius * 0.5f);

    *(real_point3d *)&vertex_constants[0] = light->position;
    vertex_constants[3] = 0.5f / radius;
    vertex_constants[4] = -matrix.forward.i;
    vertex_constants[5] = -matrix.forward.j;
    vertex_constants[6] = -matrix.forward.k;
    vertex_constants[7] = 1.0f;
    vertex_constants[8] = -matrix.left.i;
    vertex_constants[9] = -matrix.left.j;
    vertex_constants[10] = -matrix.left.k;
    vertex_constants[11] = 1.0f;
    vertex_constants[12] = -matrix.up.i;
    vertex_constants[13] = -matrix.up.j;
    vertex_constants[14] = -matrix.up.k;
    vertex_constants[15] = 1.0f;
    vertex_constants[16] = matrix.forward.i * cone_scale;
    vertex_constants[17] = matrix.forward.j * cone_scale;
    vertex_constants[18] = matrix.forward.k * cone_scale;
    vertex_constants[19] = -(cone_scale * (radius * 0.5f));

    D3DDevice_SetVertexShaderConstant(-0x51, vertex_constants, 5);

    rasterizer_set_texture(1, 2, 1, gel_bitmap_index, 0);

    D3DDevice_SetTextureStageState(1, 10, 3);
    D3DDevice_SetTextureStageState(1, 0xb, 3);
    D3DDevice_SetTextureStageState(1, 0xc, 3);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned long *)0x5a5b98 = 0x18c61;
    *(unsigned long *)0x5a5b94 = 0x11006;
    *(unsigned long *)0x5a5ae8 = 0xff;
    *(unsigned long *)0x5a5ac0 = 0x4b204b20;
    *(unsigned long *)0x5a5b28 = 0x20400;
    *(unsigned long *)0x5a5b48 = 0x484a0000;
    *(unsigned long *)0x5a5b74 = 0x20c0;
    *(unsigned long *)0x5a5aec = 0xff;
    *(unsigned long *)0x5a5ac4 = 0x4a204a20;
    *(unsigned long *)0x5a5b2c = 0x20500;
    *(unsigned long *)0x5a5b4c = 0x48cc8a40;
    *(unsigned long *)0x5a5b78 = 0x10d00;
    *(unsigned long *)0x5a5ac8 = 0x2c120c11;
    *(unsigned long *)0x5a5b30 = 0xc00;
    *(unsigned long *)0x5a5b50 = 0xcd4b0809;
    *(unsigned long *)0x5a5b7c = 0x20d0;
    *(unsigned long *)0x5a5acc = 0xd0d1415;
    *(unsigned long *)0x5a5b34 = 0xd5;
    *(unsigned long *)0x5a5b54 = 0x2c020c01;
    *(unsigned long *)0x5a5b80 = 0xc00;
    *(unsigned long *)0x5a5ad0 = 0x1d1d151c;
    *(unsigned long *)0x5a5b38 = 0xd5;
    *(unsigned long *)0x5a5b58 = 0xc091c09;
    *(unsigned long *)0x5a5b84 = 0x110cd;
    *(unsigned long *)0x5a5ad4 = 0x1d1d0000;
    *(unsigned long *)0x5a5b3c = 0xd0;
    *(unsigned long *)0x5a5b5c = 0xc150d1d;
    *(unsigned long *)0x5a5b88 = 0x10cd;
    *(unsigned long *)0x5a5ae0 = 0xc0f0000;
    *(unsigned long *)0x5a5ae4 = 0x1d330d00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
  }
}

/* 0x163590 — set up the "no lens-flare / simple" lighting pass for one
 * rasterizer light: upload five vertex-shader constants, bind the default
 * bitmap to texture stage 1, program that stage, and install the pixel shader
 * built in the shared 0x5a5ac0 block.  When the light's owner definition says
 * a richer path applies, this defers to FUN_001631d0 instead.
 *
 * Signature: the reference reads its argument at [EBP+8] (MOV EDI,[EBP+8] at
 * 0x1635dc) and returns with a bare RET, so cdecl with one int parameter.  The
 * assert string at 0x5c6 names the slot light_index.
 *
 * Light record: base 0x5a37e4, stride 0x38 (IMUL ESI,ESI,0x38 at 0x16360d),
 * count at 0x5a37e0.  Offsets touched here, all raw because their meaning is
 * unproven in this function:
 *   +0x00  pointer to the owning light definition
 *   +0x04..+0x0c  three dwords copied verbatim into vs constant 0 (raw MOVs)
 *   +0x28  float triple passed to real_rgb_color_brightness
 *   +0x34  radius
 * Owner fields: +0x1c compared as an integer against the bit pattern
 * 0xbf800000 (CMP dword ptr [EAX+0x1c],0xbf800000 at 0x163648 — an integer
 * compare in the reference, not an FCOM), +0x70 and +0x88 compared to -1, and
 * +0x24 read as a float.  The owner is re-read from the record at the FLD site
 * (MOV ECX,[ESI] at 0x1636a0), so the reload is written out.
 *
 * The radius assert at 0x5c9 is the float form of ASSERT(light->radius): the
 * reference is FLD/FCOMP [0x2533c0] / FNSTSW / TEST AH,0x44 / JP, which skips
 * the assert when the radius differs from the 0x2533c0 constant.
 *
 * Globals:
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x5c0)
 *   0x3256bc  uint16  mode selector; only value 0 is accepted
 *   0x3256ce  uint8   enable flag (must be non-zero)
 *   0x325170  uint16  written 1 on the deferred path, 0 on the local path
 *                     (MOV word ptr, so a 16-bit store)
 *   0x47dca8  float   brightness of the light's +0x28 triple
 *   0x476204  ptr     rasterizer globals; +0x0c is a bitmap tag index
 *   0x2533c0/0x2533c8/0x253398  float constants
 *   0x5a5ac0  -       0xf0-byte pixel-shader state block (shared with the
 *                     other rasterizer_xbox_* pixel-shader builders)
 * The 0x5a5ac0 store order and the call order are binary-fixed. */
void FUN_00163590(int light_index)
{
  struct vs_vec3 {
    float x, y, z;
  };
  struct point_light_t {
    char *owner;
    float position[3];
    float color[3];
    char pad_1c[0xc];
    float field_28[3];
    float radius;
  };
  const struct point_light_t *light;
  float vs_const[20];

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x5c0, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256ce != 0) {
    if (light_index < 0 || light_index >= *(int *)0x5a37e0) {
      display_assert(
        "light_index>=0 && light_index<rasterizer_lights.light_count",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x5c6, true);
      system_exit(-1);
    }

    light = (const struct point_light_t *)0x5a37e4 + light_index;

    if (light->radius == 0.0f) {
      display_assert(
        "light->radius",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x5c9, true);
      system_exit(-1);
    }

    if (*(uint32_t *)(light->owner + 0x1c) != 0xbf800000 &&
        (*(int *)(light->owner + 0x70) != -1 ||
         *(int *)(light->owner + 0x88) != -1)) {
      *(uint16_t *)0x325170 = 1;
      *(float *)0x47dca8 = real_rgb_color_brightness((float *)light->field_28);
      FUN_001631d0(light_index);
      return;
    }

    *(uint16_t *)0x325170 = 0;
    *(float *)0x47dca8 = real_rgb_color_brightness((float *)light->field_28);

    /* Constant 0 is the light position (copied as raw dwords, as the reference
     * does with MOV EDX/EAX/ECX) plus a scale in .w; constants 1..4 are the
     * rows of an identity-like basis. */
    vs_const[3] =
      (*(float *)0x2533c8 / (*(float *)(light->owner + 0x24) * light->radius)) *
      *(float *)0x253398;
    *(struct vs_vec3 *)&vs_const[0] = *(const struct vs_vec3 *)light->position;
    vs_const[4] = 0.0f;
    vs_const[5] = 0.0f;
    vs_const[6] = 0.0f;
    vs_const[7] = 1.0f;
    vs_const[8] = 0.0f;
    vs_const[9] = 0.0f;
    vs_const[10] = 0.0f;
    vs_const[11] = 1.0f;
    vs_const[12] = 0.0f;
    vs_const[13] = 0.0f;
    vs_const[14] = 0.0f;
    vs_const[15] = 1.0f;
    vs_const[16] = 0.0f;
    vs_const[17] = 0.0f;
    vs_const[18] = 0.0f;
    vs_const[19] = 1.0f;

    D3DDevice_SetVertexShaderConstant(-0x51, vs_const, 5);

    rasterizer_set_texture_direct(1, *(int *)(*(char **)0x476204 + 0xc), 0);

    D3DDevice_SetTextureStageState(1, 10, 4);
    D3DDevice_SetTextureStageState(1, 0xb, 4);
    D3DDevice_SetTextureStageState(1, 0xc, 4);
    D3DDevice_SetTextureStageState(1, 0xd, 2);
    D3DDevice_SetTextureStageState(1, 0xe, 2);
    D3DDevice_SetTextureStageState(1, 0xf, 2);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned long *)0x5a5b98 = 0x18c41;
    *(unsigned long *)0x5a5b94 = 0x11006;
    *(unsigned long *)0x5a5ae8 = 0xff;
    *(unsigned long *)0x5a5ac0 = 0x4b204b20;
    *(unsigned long *)0x5a5b28 = 0x20400;
    *(unsigned long *)0x5a5aec = 0xff;
    *(unsigned long *)0x5a5b48 = 0x484a0000;
    *(unsigned long *)0x5a5b74 = 0x20c0;
    *(unsigned long *)0x5a5ac4 = 0x4a204a20;
    *(unsigned long *)0x5a5b2c = 0x20500;
    *(unsigned long *)0x5a5b4c = 0x48cc8a40;
    *(unsigned long *)0x5a5b78 = 0x10d00;
    *(unsigned long *)0x5a5ac8 = 0x2c120c11;
    *(unsigned long *)0x5a5b30 = 0xc00;
    *(unsigned long *)0x5a5b50 = 0xcd4b0809;
    *(unsigned long *)0x5a5b7c = 0x20d0;
    *(unsigned long *)0x5a5acc = 0xd0d1415;
    *(unsigned long *)0x5a5b34 = 0xd5;
    *(unsigned long *)0x5a5b54 = 0x2c020c01;
    *(unsigned long *)0x5a5b80 = 0xc00;
    *(unsigned long *)0x5a5ad0 = 0x1d1d151c;
    *(unsigned long *)0x5a5b38 = 0xd5;
    *(unsigned long *)0x5a5b58 = 0xc091c09;
    *(unsigned long *)0x5a5b84 = 0x110cd;
    *(unsigned long *)0x5a5ad4 = 0x1d1d0000;
    *(unsigned long *)0x5a5b3c = 0xd0;
    *(unsigned long *)0x5a5b5c = 0xc150d1d;
    *(unsigned long *)0x5a5b88 = 0x10cd;
    *(unsigned long *)0x5a5ae0 = 0xc0f0000;
    *(unsigned long *)0x5a5ae4 = 0x1d200d00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
  }
}

/* 0x163910 — draw one environment lightmap/self-illumination batch: bind the
 * shader's bitmap to stage 0, program stage states, upload a 3-register
 * vertex-shader constant block, fill the shared pixel-shader state block from
 * the shader's fade/color fields, and issue the draw.
 *
 * Signature: parameter slots EBP+0x8..EBP+0x1c are all read, so cdecl with six
 * dword parameters. Names for slots 1 and 6 come from the assert strings
 * ("shader" at line 0x667, "vertex_buffer" at line 0x66d); slots 3/4/5 are
 * named after the rasterizer_draw_dynamic_triangles_static_vertices decl they are forwarded to. Only one caller
 * (0x17cd74 in FUN_0017cd70), which is not ported.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x660)
 *   0x3256bc  uint16    mode selector; this path requires exactly 0
 *   0x3256ce  uint8     enable flag (must be non-zero). NOTE: 0x3256ce, one
 *                       byte below the 0x3256cf used by FUN_001640d0.
 *   0x47dca8  float     global fade/intensity scalar; must be > 0.0f and is
 *                       multiplied into the shader's +0x290 fade at both
 *                       FUN_00159070 sites.
 *   0x325170  uint16    permutation index forwarded to FUN_00178b40
 *   0x5a5e18  void *    texture-animation globals handed to the evaluator
 *   0x2533c0  float     0.0f pool constant
 *   0x5a5ac0  -         0xf0-byte pixel-shader state block (shared with
 *                       rasterizer_xbox_widgets/shadows/models/screen_effect)
 *   0x5a5474 / 0x5a5470 / 0x5a546c
 *                       profile-mode counters (batch count, triangle total,
 *                       static-vertex total). Uniformly 0xc below the
 *                       counters FUN_001640d0 uses.
 *
 * The 12-dword block is one contiguous local (SUB ESP,0x30; every slot from
 * EBP-0x30 to EBP-0x4 is written) uploaded as 3 vertex-shader constants at
 * register -0x54. Slots 7 and 11 are handed to the texture-animation
 * evaluator as out-parameters (LEA ECX,[EBP-0x14] / LEA EAX,[EBP-0x4]).
 *
 * The reference reloads 0x47dca8 and re-multiplies by +0x290 for each of the
 * two FUN_00159070 calls (0x163ad8 and 0x163aed), so the product is written
 * out at both sites rather than hoisted into a temporary.
 *
 * Call order, the guard compare order, and the counter update order (both
 * accumulator stores precede the count_static_vertices call) are binary-fixed.
 */
void FUN_00163910(void *shader, int frame_index, int vertices_per_primitive,
                  int a4, int triangle_count, void *vertex_buffer)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  int stage_value;
  int static_vertices;
  int batch_count;
  int triangle_total;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x660, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256ce != 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x667, true);
      system_exit(-1);
    }

    shader_data = FUN_001906b0(shader, 3);

    if (*(float *)((char *)shader_data + 0x290) > *(float *)0x2533c0 &&
        *(float *)0x47dca8 > *(float *)0x2533c0) {
      if (vertex_buffer == 0) {
        display_assert(
          "vertex_buffer",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
          0x66d, true);
        system_exit(-1);
      }

      FUN_00178b40(0x15, *(uint16_t *)vertex_buffer, *(uint16_t *)0x325170);

      rasterizer_set_texture(0, 0, 3, *(int *)((char *)shader_data + 0x134),
                             frame_index);

      D3DDevice_SetTextureStageState(0, 10, 1);
      D3DDevice_SetTextureStageState(0, 0xb, 1);
      D3DDevice_SetTextureStageState(0, 0xd, 2);
      D3DDevice_SetTextureStageState(0, 0xe, 2);
      D3DDevice_SetTextureStageState(0, 0xf, 2);

      /* [0]/[1] are copied as raw dwords: the reference moves them through
       * GPRs (MOV ECX,[ESI+0x138] / MOV [EBP-0x30],ECX), not the x87 stack,
       * and they precede the literal stores. */
      *(uint32_t *)&constants[0] = *(uint32_t *)((char *)shader_data + 0x138);
      *(uint32_t *)&constants[1] = *(uint32_t *)((char *)shader_data + 0x13c);
      /* MOV EDX,[0x5a5e18] at 0x163a74 precedes MOV [EBP-0x28],0x3f800000,
       * which lets the reference push all four arguments ahead of the block. */
      texture_globals = *(void **)0x5a5e18;
      constants[2] = 1.0f;
      constants[3] = 1.0f;
      constants[4] = 1.0f;
      constants[5] = 0.0f;
      constants[6] = 0.0f;
      constants[7] = 0.0f;
      constants[8] = 0.0f;
      constants[9] = 1.0f;
      constants[10] = 0.0f;
      constants[11] = 0.0f;

      shader_environment_texture_animation_evaluate(
        shader, texture_globals, &constants[7], &constants[11]);

      D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

      *(unsigned long *)0x5a5af0 = FUN_00159070(
        *(float *)0x47dca8 * *(float *)((char *)shader_data + 0x290));
      *(unsigned long *)0x5a5b10 = FUN_00159070(
        *(float *)0x47dca8 * *(float *)((char *)shader_data + 0x290));
      *(unsigned long *)0x5a5af4 =
        real_rgb_color_to_pixel32((float *)((char *)shader_data + 0x2a8));
      *(unsigned long *)0x5a5b14 =
        real_rgb_color_to_pixel32((float *)((char *)shader_data + 0x2b4));

      if ((*(uint8_t *)((char *)shader_data + 0x28) & 2) != 0) {
        *(unsigned long *)0x5a5b48 = 0x14a0000;
        *(unsigned long *)0x5a5b4c = 0x1cc8a40;
        *(unsigned long *)0x5a5b7c = 0x20d9;
      } else {
        *(unsigned long *)0x5a5b48 = 0x484a0000;
        *(unsigned long *)0x5a5b4c = 0x48cc8a40;
        *(unsigned long *)0x5a5b7c = 0x20d0;
      }

      *(unsigned long *)0x5a5b84 =
        0x110cd +
        (((*(uint8_t *)((char *)shader_data + 0x27c) & 1) != 0) ? 0x10000 : 0);

      if ((*(uint8_t *)((char *)shader_data + 0x27c) & 2) != 0) {
        *(unsigned long *)0x5a5b94 = 0x11008;
        *(unsigned long *)0x5a5ad8 = 0x1d1d0000;
        *(unsigned long *)0x5a5adc = 0x1d1d0000;
        stage_value = 0xd0;
      } else {
        *(unsigned long *)0x5a5b94 = 0x11006;
        *(unsigned long *)0x5a5ad8 = 0;
        *(unsigned long *)0x5a5adc = 0;
        stage_value = 0;
      }
      /* The reference stores 0x5a5b40/0x5a5b44 from EAX after the branch
       * joins, not inside either arm. */
      *(unsigned long *)0x5a5b40 = stage_value;
      *(unsigned long *)0x5a5b44 = stage_value;

      rasterizer_set_pixel_shader((void *)0x5a5ac0);

      rasterizer_draw_dynamic_triangles_static_vertices(vertices_per_primitive, a4, triangle_count, vertex_buffer);

      if (*(uint16_t *)0x3256ba == 2) {
        batch_count = *(int *)0x5a5474;
        triangle_total = *(int *)0x5a5470;
        *(int *)0x5a5474 = batch_count + 1;
        *(int *)0x5a5470 = triangle_total + triangle_count;
        /* The reference loads 0x5a546c only after the call returns, so the
         * call must not be sequenced against a pending read of it. */
        static_vertices = rasterizer_frame_statistics_count_static_vertices(
          vertices_per_primitive, a4, triangle_count);
        *(int *)0x5a546c = *(int *)0x5a546c + static_vertices;
      }
    }
  }
}

/* 0x163c40 — begin rasterizer profile section 0xc and, on one narrow mode,
 * bind the shared lightmap bitmap to texture stages 2 and 3, program their
 * stage states, set the blend/depth render state, and bind the pixel-shader
 * state block.
 *
 * Signature: no parameter slot is read and the reference ends in a bare RET,
 * so cdecl void(void).
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x6f9)
 *   0x3256bc  uint16    mode selector; this path requires exactly 0
 *   0x3256cf  uint8     enable flag (must be non-zero). NOTE: this is
 *                       0x3256cf, not the 0x3256d0/0x3256d2 pair used by the
 *                       four-condition guard in FUN_001643e0/FUN_00164590 —
 *                       this guard has only three conditions.
 *   0x3256b0  uint16    must be 0
 *   0x476204  char *    pointer to a record whose +0x1c dword is the bitmap
 *                       tag index bound to both stages (same field
 *                       rasterizer_xbox_shadows.c:386 binds to stage 2).
 *   0x1fb7a4 / 0x1fb784 / 0x1fb790 / 0x1fb794 / 0x1fb7c0 / 0x1fb788 /
 *   0x1fb78c / 0x1fb77c / 0x1fb798
 *                       render-state shadow copies. Each store follows the
 *                       D3DDevice_SetRenderState_Simple call it mirrors: the
 *                       binary emits the store after the ECX/EDX loads of the
 *                       NEXT call, but it belongs to the preceding one.
 *                       ZEnable and ZBias have no shadow store. Unlike
 *                       FUN_001643e0 this path also programs 0x40340 (shadow
 *                       0x1fb78c) and passes 1 (not 0) to 0x40348 and 0x40300.
 *   0x5a5ac0  -         0xf0-byte pixel-shader state block (shared with
 *                       rasterizer_xbox_widgets/shadows/models/screen_effect).
 *                       Highest store offset is 0x5a5b98 - 0x5a5ac0 = 0xd8, so
 *                       every store lands inside the 0xf0 csmemset.
 *
 * The decompiler renders three of the block immediates as address-of forms
 * (&DAT_00011006 / &DAT_00010d00 / &DAT_000110cd); the disassembly stores the
 * plain integers 0x11006 / 0x10d00 / 0x110cd, which is what is written here.
 * MSVC hoists the repeated 0xc00 and 0xd5 immediates into ECX/EAX before the
 * block; that is scheduling, so the immediates are written out directly.
 * The reference emits one combined ADD ESP,0x10 covering csmemset's three
 * arguments and rasterizer_set_pixel_shader's one.
 *
 * Call order, the guard compare order, and the 0x5a5ac0 store order are
 * binary-fixed. */
void FUN_00163c40(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x6f9, true);
    system_exit(-1);
  }

  FUN_0016f910(0xc);

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cf != 0 &&
      *(uint16_t *)0x3256b0 == 0) {
    rasterizer_set_texture_direct(2, *(int *)(*(char **)0x476204 + 0x1c), 0);
    D3DDevice_SetTextureStageState(2, 10, 3);
    D3DDevice_SetTextureStageState(2, 0xb, 3);
    D3DDevice_SetTextureStageState(2, 0xc, 3);
    D3DDevice_SetTextureStageState(2, 0xd, 2);
    D3DDevice_SetTextureStageState(2, 0xe, 2);
    D3DDevice_SetTextureStageState(2, 0xf, 2);
    rasterizer_set_texture_direct(3, *(int *)(*(char **)0x476204 + 0x1c), 0);
    D3DDevice_SetTextureStageState(3, 10, 3);
    D3DDevice_SetTextureStageState(3, 0xb, 3);
    D3DDevice_SetTextureStageState(3, 0xc, 3);
    D3DDevice_SetTextureStageState(3, 0xd, 2);
    D3DDevice_SetTextureStageState(3, 0xe, 2);
    D3DDevice_SetTextureStageState(3, 0xf, 2);

    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD,
                                    NV097_COLOR_MASK_RGB);
    *(unsigned long *)0x1fb7a4 = NV097_COLOR_MASK_RGB;
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(unsigned long *)0x1fb784 = 1;
    D3DDevice_SetRenderState_Simple(0x40344, 0x304);
    *(unsigned long *)0x1fb790 = 0x304;
    D3DDevice_SetRenderState_Simple(0x40348, 1);
    *(unsigned long *)0x1fb794 = 1;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(unsigned long *)0x1fb7c0 = 0x8006;
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(unsigned long *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0);
    *(unsigned long *)0x1fb78c = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x202);
    *(unsigned long *)0x1fb77c = 0x202;
    D3DDevice_SetRenderState_Simple(0x4035c, 0);
    *(unsigned long *)0x1fb798 = 0;
    D3DDevice_SetRenderState_ZBias(0);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned long *)0x5a5b98 = 0x18c21;
    *(unsigned long *)0x5a5b94 = 0x11006;
    *(unsigned long *)0x5a5ae8 = 0x800000ff;
    *(unsigned long *)0x5a5ac0 = 0x4b204b20;
    *(unsigned long *)0x5a5b28 = 0x20400;
    *(unsigned long *)0x5a5b48 = 0x484a0911;
    *(unsigned long *)0x5a5b74 = 0x30c9;
    *(unsigned long *)0x5a5aec = 0xff;
    *(unsigned long *)0x5a5ac4 = 0x4a204a20;
    *(unsigned long *)0x5a5b2c = 0x20500;
    *(unsigned long *)0x5a5b4c = 0x48cc8a40;
    *(unsigned long *)0x5a5b78 = 0x10d00;
    *(unsigned long *)0x5a5ac8 = 0x2c120c11;
    *(unsigned long *)0x5a5b30 = 0xc00;
    *(unsigned long *)0x5a5b50 = 0xcd4b0809;
    *(unsigned long *)0x5a5b7c = 0x20d0;
    *(unsigned long *)0x5a5acc = 0xd0d1415;
    *(unsigned long *)0x5a5b34 = 0xd5;
    *(unsigned long *)0x5a5b54 = 0x2c020c01;
    *(unsigned long *)0x5a5b80 = 0xc00;
    *(unsigned long *)0x5a5ad0 = 0x1d1d151c;
    *(unsigned long *)0x5a5b38 = 0xd5;
    *(unsigned long *)0x5a5b58 = 0xc091c09;
    *(unsigned long *)0x5a5b84 = 0x110cd;
    *(unsigned long *)0x5a5ad4 = 0x1d1d0000;
    *(unsigned long *)0x5a5b3c = 0xd0;
    *(unsigned long *)0x5a5b5c = 0xc150d1d;
    *(unsigned long *)0x5a5b88 = 0x10cd;
    *(unsigned long *)0x5a5b6c = 0x80000000;
    *(unsigned long *)0x5a5ae0 = 0xc0f0000;
    *(unsigned long *)0x5a5ae4 = 0x1d110d00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
  }
}

/* 0x163fe0 — bind a bitmap to texture stage 1 and program its stage state,
 * or record that no bitmap was supplied.
 *
 * Signature: the reference reads its argument at [EBP+8] and returns with a
 * bare RET (no stack cleanup), so this is cdecl with one pointer parameter.
 * The parameter is forwarded unchanged to rasterizer_set_texture_bitmap_data,
 * whose kb decl names that slot bitmap_data.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x752)
 *   0x3256bc  uint16    mode selector; only value 0 is accepted here
 *                       (binary: a single CMP word ptr [0x3256bc],0x0)
 *   0x3256cf  uint8     enable flag (must be non-zero)
 *   0x3256ed  uint8     flag selecting stage-state value 1 vs 2 for states
 *                       0xd/0xe/0xf. The reference re-loads it before each of
 *                       the three calls (into CL, DL, AL) rather than caching,
 *                       so each use below is a direct read.
 *   0x47dca4  uint8     byte flag: cleared on the bound path, set when the
 *                       supplied pointer is null. It is written only inside
 *                       the accepted-mode guard; both early exits skip it.
 * Call order is binary-fixed. */
void FUN_00163fe0(void *bitmap_data)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x752, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cf != 0) {
    if (bitmap_data != 0) {
      rasterizer_set_texture_bitmap_data(1, bitmap_data);

      D3DDevice_SetTextureStageState(1, 10, 3);
      D3DDevice_SetTextureStageState(1, 0xb, 3);
      D3DDevice_SetTextureStageState(1, 0xd, (*(uint8_t *)0x3256ed != 0) + 1);
      D3DDevice_SetTextureStageState(1, 0xe, (*(uint8_t *)0x3256ed != 0) + 1);
      D3DDevice_SetTextureStageState(1, 0xf, (*(uint8_t *)0x3256ed != 0) + 1);

      *(uint8_t *)0x47dca4 = 0;
      return;
    }
    *(uint8_t *)0x47dca4 = 1;
  }
}

/* 0x1640d0 — draw one environment geometry batch with a bitmap bound to
 * texture stage 0 and the shared pixel-shader state block reprogrammed from
 * the shader-data flag bytes.
 *
 * Signature: cdecl, bare RET (caller cleans). Six dword parameter slots are
 * read from the frame: [EBP+8] shader, [EBP+0xc] (forwarded to
 * rasterizer_set_texture's frame_index slot), [EBP+0x10], [EBP+0x14],
 * [EBP+0x18] and [EBP+0x1c]. The last three forwarded slots land in
 * rasterizer_draw_dynamic_triangles_static_vertices2's kb decl positions vertices_per_primitive / a2 /
 * triangle_count and in rasterizer_frame_statistics_count_static_vertices'
 * identically named slots; the names here are taken from those decls, not
 * from independent evidence. [EBP+0x1c] is read as a uint16 at offset 0 (the
 * vertex type passed to FUN_00178b40) and forwarded both as-is and as +0x14.
 * [EBP+0xc] is loaded with MOV ECX,dword ptr [EBP+0xc], so the slot is a
 * 32-bit type; it is declared int and passed straight through.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x77b)
 *   0x3256bc  uint16  mode selector; this path requires exactly 0
 *   0x3256cf  uint8   single enable flag (must be non-zero) — this function
 *                     tests only this one byte, unlike FUN_001609b0 which
 *                     tests 0x3256d0 and 0x3256d2
 *   0x3256b0  uint16  must be 0
 *   0x47dca4  uint8   must be 0 (set by FUN_00163fe0/FUN_00164590 when a
 *                     stage bitmap was missing)
 *   0x3256ba  uint16  statistics mode; the counters below run only when it
 *                     is 2. NOTE: 0x3256ba, not the 0x3256bc of the guard.
 *   0x5a5e18  dword   value forwarded unchanged to
 *                     shader_environment_texture_animation_evaluate; the
 *                     reference copies it with MOV ECX / PUSH ECX, so its
 *                     type is unproven.
 *   0x5a5ac0  the 0xf0-byte pixel-shader state block (see FUN_00163590,
 *                     which memsets and fully populates it); the fields
 *                     written here are 0x5a5ad8/0x5a5adc, 0x5a5af0/0x5a5af4,
 *                     0x5a5b10/0x5a5b14, 0x5a5b40/0x5a5b44, 0x5a5b48/0x5a5b4c,
 *                     0x5a5b7c, 0x5a5b84 and 0x5a5b94.
 *   0x5a5480 / 0x5a547c / 0x5a5478
 *                     frame statistic counters: batch count, triangle_count
 *                     accumulator, and the accumulator fed by
 *                     rasterizer_frame_statistics_count_static_vertices.
 *
 * Shader-data offsets (from FUN_001906b0(shader, 3)); all read directly in
 * the disassembly:
 *   +0x28            flag byte, bit 1 selects the 0x5a5b48/0x5a5b4c/0x5a5b7c
 *                    triple
 *   +0x134           bitmap tag index bound to stage 0
 *   +0x138 / +0x13c  the two animated values written into constant row 0
 *   +0x27c           flag byte; bit 2 gates the whole draw, bit 0 adds
 *                    0x10000 to the 0x5a5b84 word, bit 1 selects the
 *                    0x5a5b94/0x5a5ad8/0x5a5adc/0x5a5b40 set
 *   +0x290           float compared against 0.0f and passed twice to
 *                    FUN_00159070
 *   +0x2a8 / +0x2b4  color vectors passed to real_rgb_color_to_pixel32
 *
 * Branch senses are decoded from the FNSTSW form, not from the decompiler:
 *   FCOMP [0x2533c0] / TEST AH,0x41 / JNZ-to-skip  =>  operand > 0.0f
 *
 * The 12-dword block is one contiguous local (SUB ESP,0x30; every slot from
 * EBP-0x30 to EBP-0x4 is written) uploaded as 3 vertex-shader constants at
 * register -0x54. Slots 7 and 11 are the trailing components of constant
 * rows 1 and 2; their addresses are handed to the texture-animation
 * evaluator as out-parameters.
 *
 * Call order, the guard compare order, and the counter update order (both
 * accumulator stores precede the count_static_vertices call) are binary-fixed.
 */
void FUN_001640d0(void *shader, int frame_index, int vertices_per_primitive,
                  int a4, int triangle_count, void *geometry)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  int stage_value;
  int static_vertices;
  int batch_count;
  int triangle_total;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x77b, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256cf != 0 &&
      *(uint16_t *)0x3256b0 == 0 && *(uint8_t *)0x47dca4 == 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x784, true);
      system_exit(-1);
    }

    shader_data = FUN_001906b0(shader, 3);

    if (*(float *)((char *)shader_data + 0x290) > *(float *)0x2533c0 &&
        (*(uint8_t *)((char *)shader_data + 0x27c) & 4) != 0) {
      FUN_00178b40(0x15, *(uint16_t *)geometry, 2);

      rasterizer_set_texture(0, 0, 3, *(int *)((char *)shader_data + 0x134),
                             frame_index);

      D3DDevice_SetTextureStageState(0, 10, 1);
      D3DDevice_SetTextureStageState(0, 0xb, 1);
      D3DDevice_SetTextureStageState(0, 0xd, 2);
      D3DDevice_SetTextureStageState(0, 0xe, 2);
      D3DDevice_SetTextureStageState(0, 0xf, 2);

      /* [0]/[1] are copied as raw dwords: the reference moves them through
       * GPRs (MOV EAX,[ESI+0x138] / MOV [EBP-0x30],EAX), not the x87 stack,
       * and they precede the literal stores. */
      *(uint32_t *)&constants[0] = *(uint32_t *)((char *)shader_data + 0x138);
      *(uint32_t *)&constants[1] = *(uint32_t *)((char *)shader_data + 0x13c);
      /* MOV ECX,[0x5a5e18] precedes MOV [EBP-0x28],0x3f800000, which lets
       * the reference push all four arguments ahead of the block. */
      texture_globals = *(void **)0x5a5e18;
      constants[2] = 1.0f;
      constants[3] = 1.0f;
      constants[4] = 1.0f;
      constants[5] = 0.0f;
      constants[6] = 0.0f;
      constants[7] = 0.0f;
      constants[8] = 0.0f;
      constants[9] = 1.0f;
      constants[10] = 0.0f;
      constants[11] = 0.0f;

      shader_environment_texture_animation_evaluate(
        shader, texture_globals, &constants[7], &constants[11]);

      D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

      *(unsigned long *)0x5a5af0 =
        FUN_00159070(*(float *)((char *)shader_data + 0x290));
      *(unsigned long *)0x5a5b10 =
        FUN_00159070(*(float *)((char *)shader_data + 0x290));
      *(unsigned long *)0x5a5af4 =
        real_rgb_color_to_pixel32((float *)((char *)shader_data + 0x2a8));
      *(unsigned long *)0x5a5b14 =
        real_rgb_color_to_pixel32((float *)((char *)shader_data + 0x2b4));

      if ((*(uint8_t *)((char *)shader_data + 0x28) & 2) != 0) {
        *(unsigned long *)0x5a5b48 = 0x14a0911;
        *(unsigned long *)0x5a5b4c = 0x1cc8a40;
        *(unsigned long *)0x5a5b7c = 0x20d9;
      } else {
        *(unsigned long *)0x5a5b48 = 0x484a0911;
        *(unsigned long *)0x5a5b4c = 0x48cc8a40;
        *(unsigned long *)0x5a5b7c = 0x20d0;
      }

      *(unsigned long *)0x5a5b84 =
        0x110cd +
        (((*(uint8_t *)((char *)shader_data + 0x27c) & 1) != 0) ? 0x10000 : 0);

      if ((*(uint8_t *)((char *)shader_data + 0x27c) & 2) != 0) {
        *(unsigned long *)0x5a5b94 = 0x11008;
        *(unsigned long *)0x5a5ad8 = 0x1d1d0000;
        *(unsigned long *)0x5a5adc = 0x1d1d0000;
        stage_value = 0xd0;
      } else {
        *(unsigned long *)0x5a5b94 = 0x11006;
        *(unsigned long *)0x5a5ad8 = 0;
        *(unsigned long *)0x5a5adc = 0;
        stage_value = 0;
      }
      /* The reference stores 0x5a5b40/0x5a5b44 from EAX after the branch
       * joins, not inside either arm. */
      *(unsigned long *)0x5a5b40 = stage_value;
      *(unsigned long *)0x5a5b44 = stage_value;

      rasterizer_set_pixel_shader((void *)0x5a5ac0);

      rasterizer_draw_dynamic_triangles_static_vertices2(vertices_per_primitive, a4, triangle_count, (const vertex_buffer *)geometry,
                   (const vertex_buffer *)((char *)geometry + 0x14));

      if (*(uint16_t *)0x3256ba == 2) {
        batch_count = *(int *)0x5a5480;
        triangle_total = *(int *)0x5a547c;
        *(int *)0x5a5480 = batch_count + 1;
        *(int *)0x5a547c = triangle_total + triangle_count;
        /* The reference loads 0x5a5478 only after the call returns, so the
         * call must not be sequenced against a pending read of it. */
        static_vertices = rasterizer_frame_statistics_count_static_vertices(
          vertices_per_primitive, a4, triangle_count);
        *(int *)0x5a5478 = *(int *)0x5a5478 + static_vertices;
      }
    }
  }
}

/* 0x1643e0 — begin rasterizer profile section 0xd and, on one narrow mode,
 * program alpha-only color-mask blend/depth render state and bind the shared
 * pixel-shader state block.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x815)
 *   0x3256bc  uint16    mode selector; this path requires exactly 0
 *   0x3256d0  uint8     enable flag (must be non-zero)
 *   0x3256d2  uint8     second enable flag (must be non-zero)
 *   0x3256b0  uint16    must be 0
 *   0x1fb7a4 / 0x1fb784 / 0x1fb790 / 0x1fb794 / 0x1fb7c0 / 0x1fb788 /
 *   0x1fb77c / 0x1fb798
 *                       render-state shadow copies. Each store follows the
 *                       D3DDevice_SetRenderState_Simple call it mirrors: the
 *                       binary emits the store after the ECX/EDX loads of the
 *                       NEXT call, but it belongs to the preceding one.
 *                       ZEnable and ZBias have no shadow store.
 *   0x5a5ac0  -         0xf0-byte pixel-shader state block (shared with
 *                       rasterizer_xbox_widgets/shadows/models/screen_effect)
 *
 * The color mask 0x1000000 is alpha-only (bit 24); it is not one of the
 * canonical NV097_COLOR_MASK_* values, so it stays a literal.
 * Call order, the guard compare order, and the 0x5a5ac0 store order are
 * binary-fixed. */
void FUN_001643e0(void)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x815, true);
    system_exit(-1);
  }

  FUN_0016f910(0xd);

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256d0 != 0 &&
      *(uint8_t *)0x3256d2 != 0 && *(uint16_t *)0x3256b0 == 0) {
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(NV097_SET_COLOR_MASK_CMD, 0x1000000);
    *(unsigned long *)0x1fb7a4 = 0x1000000;
    D3DDevice_SetRenderState_Simple(0x40304, 1);
    *(unsigned long *)0x1fb784 = 1;
    D3DDevice_SetRenderState_Simple(0x40344, 0x304);
    *(unsigned long *)0x1fb790 = 0x304;
    D3DDevice_SetRenderState_Simple(0x40348, 0);
    *(unsigned long *)0x1fb794 = 0;
    D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
    *(unsigned long *)0x1fb7c0 = 0x8006;
    D3DDevice_SetRenderState_Simple(0x40300, 0);
    *(unsigned long *)0x1fb788 = 0;
    D3DDevice_SetRenderState_ZEnable(1);
    D3DDevice_SetRenderState_Simple(0x40354, 0x202);
    *(unsigned long *)0x1fb77c = 0x202;
    D3DDevice_SetRenderState_Simple(0x4035c, 0);
    *(unsigned long *)0x1fb798 = 0;
    D3DDevice_SetRenderState_ZBias(0);

    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(unsigned long *)0x5a5b98 = 1;
    *(unsigned long *)0x5a5b94 = 2;
    *(unsigned long *)0x5a5ae8 = 0x80b050;
    *(unsigned long *)0x5a5b48 = 0x8010000;
    *(unsigned long *)0x5a5b74 = 0x20c0;
    *(unsigned long *)0x5a5ac4 = 0x2c120c20;
    *(unsigned long *)0x5a5b2c = 0xc00;
    *(unsigned long *)0x5a5ae0 = 0;
    *(unsigned long *)0x5a5ae4 = 0x1c00;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
  }
}

/* 0x164590 — bind a bitmap to texture stage 0 and program its stage states,
 * on one narrow mode. Structurally the stage-0 twin of FUN_00163fe0 (0x163fe0),
 * with the four-condition guard used by FUN_001643e0 (0x1643e0).
 *
 * Signature: the reference reads its argument at [EBP+8] and returns with a
 * bare RET (no stack cleanup), so this is cdecl with one pointer parameter.
 * The parameter is forwarded unchanged to rasterizer_set_texture_bitmap_data,
 * whose kb decl names that slot bitmap_data.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *    global_d3d_device (assert at __FILE__ line 0x848)
 *   0x3256bc  uint16    mode selector; only value 0 is accepted here
 *                       (binary: CMP word ptr [0x3256bc],0x0)
 *   0x3256d0  uint8     enable flag (must be non-zero)
 *   0x3256d2  uint8     second enable flag (must be non-zero)
 *   0x3256b0  uint16    must be 0 (CMP word ptr [0x3256b0],0x0)
 *   0x3256ed  uint8     flag selecting stage-state value 1 vs 2 for states
 *                       0xd/0xe/0xf. The reference re-loads it before each of
 *                       the three calls (into CL, DL, AL) rather than caching,
 *                       so each use below is a direct read.
 *   0x47dca4  uint8     byte flag: cleared on the bound path, set when the
 *                       supplied pointer is null. It is written only inside
 *                       the accepted-mode guard; both early exits skip it.
 * Stage index 0 is binary-proven: every stage-state call reaches 0x1e9410 with
 * XOR ECX,ECX, and the bitmap bind pushes 0 as its first (stage) argument.
 * Call order is binary-fixed. */
void FUN_00164590(void *bitmap_data)
{
  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x848, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256d0 != 0 &&
      *(uint8_t *)0x3256d2 != 0 && *(uint16_t *)0x3256b0 == 0) {
    if (bitmap_data != 0) {
      rasterizer_set_texture_bitmap_data(0, bitmap_data);

      D3DDevice_SetTextureStageState(0, 10, 3);
      D3DDevice_SetTextureStageState(0, 0xb, 3);
      D3DDevice_SetTextureStageState(0, 0xd, (*(uint8_t *)0x3256ed != 0) + 1);
      D3DDevice_SetTextureStageState(0, 0xe, (*(uint8_t *)0x3256ed != 0) + 1);
      D3DDevice_SetTextureStageState(0, 0xf, (*(uint8_t *)0x3256ed != 0) + 1);

      *(uint8_t *)0x47dca4 = 0;
      return;
    }
    *(uint8_t *)0x47dca4 = 1;
  }
}

/* 0x164690 — draw one environment "distant/fog plane" batch: bind the
 * shader's stage-0 bitmap plus the shared stage-1/2 bitmap, program four
 * texture stages and the fixed-function render states, upload the shared
 * texture-animation vertex-shader constant block, then build and set the
 * 0xf0-byte pixel-shader state block before emitting the primitives.
 *
 * Signature: cdecl, bare RET (caller cleans). Six dword parameter slots are
 * read from the frame: [EBP+8] shader, [EBP+0xc] (forwarded to
 * rasterizer_set_texture's frame_index slot), [EBP+0x10], [EBP+0x14],
 * [EBP+0x18] and [EBP+0x1c]. The middle three land in rasterizer_draw_dynamic_triangles_static_vertices's kb decl
 * positions vertices_per_primitive / a2 / triangle_count and in
 * rasterizer_frame_statistics_count_static_vertices' identically named slots;
 * the names here are taken from those decls, not from independent evidence.
 * [EBP+0x1c] is the vertex buffer: read as a uint16 at offset 0 (the vertex
 * type passed to FUN_00178b40) and forwarded whole to rasterizer_draw_dynamic_triangles_static_vertices. Unlike
 * the FUN_001609b0 / FUN_001640d0 siblings there is no "+0x14" second
 * forward — the callee here is the 4-argument rasterizer_draw_dynamic_triangles_static_vertices, not rasterizer_draw_dynamic_triangles_static_vertices2.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0x8df)
 *   0x3256bc  uint16  mode selector; this path requires exactly 0
 *   0x3256d1  uint8   enable flag (must be non-zero); read as a byte
 *                     (MOV AL / TEST AL,AL). Not the 0x3256d0/0x3256d2 pair
 *                     that FUN_001609b0 tests.
 *   0x5a5bc4  uint8   second enable flag (must be non-zero)
 *   0x5a5bc0  uint16  must be 0 (CMP word ptr)
 *   0x476204  ptr     rasterizer globals; +0x1c is the bitmap tag index bound
 *                     to texture stages 1 and 2
 *   0x5a5e18  dword   value forwarded unchanged to
 *                     shader_environment_texture_animation_evaluate; the
 *                     reference copies it with MOV EDX / PUSH EDX, so its
 *                     type is unproven.
 *   0x1fb7a4 / 0x1fb784 / 0x1fb790 / 0x1fb794 / 0x1fb7c0 / 0x1fb788 /
 *   0x1fb77c / 0x1fb798
 *                     shadow copies of the render states, each stored right
 *                     after its D3DDevice_SetRenderState_Simple call.
 *   0x5a5ac0  -       the 0xf0-byte pixel-shader state block (see
 *                     FUN_00163590, which also memsets and populates it).
 *   0x5a5bd4 / 0x5a5bd8 / 0x5a5bdc
 *                     three scalars folded into the clamped color fed to
 *                     real_rgb_color_to_pixel32 on the "no stage-0 bitmap" path.
 *   0x3256ba  uint16  statistics mode; the counters below run only when it
 *                     is 2. NOTE: 0x3256ba, not the 0x3256bc of the guard.
 *   0x5a5498 / 0x5a5494 / 0x5a5490
 *                     frame statistic counters: batch count, triangle_count
 *                     accumulator, and the accumulator fed by
 *                     rasterizer_frame_statistics_count_static_vertices.
 *                     These are a different triple from either sibling's.
 *   0x253398 / 0x2533c0 / 0x2533c8
 *                     float constants (1.0f-class scale, low clamp, high
 *                     clamp); addressed as memory operands, matching the
 *                     reference's FMUL/FSUBR/FCOM memory forms.
 *
 * Shader-data offsets (from FUN_001906b0(shader, 3)); all read directly in
 * the disassembly:
 *   +0x28            flag byte, bit 1 selects the 0x5a5ae4 word
 *   +0x134           bitmap tag index bound to stage 0; the value -1 (no
 *                    bitmap) selects the clamped-color path
 *   +0x138 / +0x13c  the two animated values written into constant row 0
 *   +0x2a8 / +0x2b4  color vectors passed to real_a_rgb_color_to_pixel32
 *   +0x2d0           flag byte; bit 0 gates the whole draw
 *   +0x2f4 / +0x2f8  floats compared against 0.0f and used as the alpha of
 *                    the two packed colors
 *
 * Branch senses are decoded from the FNSTSW forms, not from the decompiler:
 *   FCOMP [0x2533c0] / TEST AH,0x41 / JZ   =>  operand > memory
 *   FCOM  [0x2533c0] / TEST AH,0x5  / JP   =>  jump unless operand < memory
 *   FCOM  [0x2533c8] / TEST AH,0x41 / JNZ  =>  jump unless operand > memory
 * The gate order is binary-fixed as flag-first: TEST AL,0x1 on +0x2d0 runs
 * before either float compare.
 *
 * FSUBR against a memory operand computes memory - ST0, so each clamped
 * component is scale - (factor * scale), not the other way round.
 *
 * The 12-dword constant block is one contiguous local (SUB ESP,0x3c covers it
 * plus the 3-float color) uploaded as 3 vertex-shader constants at register
 * -0x54. Slots 7 and 11 are the trailing components of constant rows 1 and 2;
 * their addresses are handed to the texture-animation evaluator as
 * out-parameters. The three clamped floats are one contiguous group: the
 * reference takes the address of the first (LEA ECX,[EBP-0xc]) and passes it
 * to real_rgb_color_to_pixel32, whose kb decl is float *.
 *
 * Call-site arity note: the binary emits merged cdecl cleanups. ADD ESP,0x1c
 * after CALL 0x190a90 is 4 arguments plus FUN_00178b40's 3 uncleaned;
 * ADD ESP,0x24 after CALL 0x15dc10 is 4 arguments plus the uncleaned
 * rasterizer_set_pixel_shader 1 and two real_a_rgb_color_to_pixel32 2+2.
 *
 * Call order, the guard compare order, the render-state/shadow-store
 * interleave, and the counter update order (both accumulator stores precede
 * the count_static_vertices call) are binary-fixed.
 */
void FUN_00164690(void *shader, int frame_index, int vertices_per_primitive,
                  int a4, int triangle_count, void *vertex_buffer)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  float color[3];
  int static_vertices;
  int batch_count;
  int triangle_total;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x8df, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc == 0 && *(uint8_t *)0x3256d1 != 0 &&
      *(uint8_t *)0x5a5bc4 != 0 && *(uint16_t *)0x5a5bc0 == 0) {
    if (shader == 0) {
      display_assert(
        "shader",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
        0x8e8, true);
      system_exit(-1);
    }

    shader_data = FUN_001906b0(shader, 3);

    if ((*(uint8_t *)((char *)shader_data + 0x2d0) & 1) != 0 &&
        (*(float *)((char *)shader_data + 0x2f4) > *(float *)0x2533c0 ||
         *(float *)((char *)shader_data + 0x2f8) > *(float *)0x2533c0)) {
      if (vertex_buffer == 0) {
        display_assert(
          "vertex_buffer",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
          0x8ef, true);
        system_exit(-1);
      }

      rasterizer_set_texture(0, 0, 3, *(int *)((char *)shader_data + 0x134),
                             frame_index);

      D3DDevice_SetTextureStageState(0, 10, 1);
      D3DDevice_SetTextureStageState(0, 0xb, 1);
      D3DDevice_SetTextureStageState(0, 0xd, 2);
      D3DDevice_SetTextureStageState(0, 0xe, 2);
      D3DDevice_SetTextureStageState(0, 0xf, 2);

      rasterizer_set_texture_direct(1, *(int *)(*(char **)0x476204 + 0x1c), 0);

      D3DDevice_SetTextureStageState(1, 10, 3);
      D3DDevice_SetTextureStageState(1, 0xb, 3);
      D3DDevice_SetTextureStageState(1, 0xc, 3);
      D3DDevice_SetTextureStageState(1, 0xd, 2);
      D3DDevice_SetTextureStageState(1, 0xe, 1);
      D3DDevice_SetTextureStageState(1, 0xf, 1);

      rasterizer_set_texture_direct(2, *(int *)(*(char **)0x476204 + 0x1c), 0);

      D3DDevice_SetTextureStageState(2, 10, 3);
      D3DDevice_SetTextureStageState(2, 0xb, 3);
      D3DDevice_SetTextureStageState(2, 0xc, 3);
      D3DDevice_SetTextureStageState(2, 0xd, 2);
      D3DDevice_SetTextureStageState(2, 0xe, 1);
      D3DDevice_SetTextureStageState(2, 0xf, 1);

      FUN_001584f0(3, 1, 0);

      /* Stage 3 has no 0xc entry; stages 1 and 2 do. Binary-fixed. */
      D3DDevice_SetTextureStageState(3, 10, 3);
      D3DDevice_SetTextureStageState(3, 0xb, 3);
      D3DDevice_SetTextureStageState(3, 0xd, 2);
      D3DDevice_SetTextureStageState(3, 0xe, 2);
      D3DDevice_SetTextureStageState(3, 0xf, 2);

      D3DDevice_SetRenderState_CullMode(0x901);

      D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
      *(uint32_t *)0x1fb7a4 = 0x10101;
      D3DDevice_SetRenderState_Simple(0x40304, 1);
      *(uint32_t *)0x1fb784 = 1;
      D3DDevice_SetRenderState_Simple(0x40344, 0x304);
      *(uint32_t *)0x1fb790 = 0x304;
      D3DDevice_SetRenderState_Simple(0x40348, 1);
      *(uint32_t *)0x1fb794 = 1;
      D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
      *(uint32_t *)0x1fb7c0 = 0x8006;
      D3DDevice_SetRenderState_Simple(0x40300, 0);
      *(uint32_t *)0x1fb788 = 0;
      D3DDevice_SetRenderState_ZEnable(1);
      D3DDevice_SetRenderState_Simple(0x40354, 0x202);
      *(uint32_t *)0x1fb77c = 0x202;
      D3DDevice_SetRenderState_Simple(0x4035c, 0);
      *(uint32_t *)0x1fb798 = 0;
      D3DDevice_SetRenderState_ZBias(0);

      FUN_00178b40(0x33, *(uint16_t *)vertex_buffer, 0);

      /* [0]/[1] are copied as raw dwords: the reference moves them through
       * GPRs (MOV ECX,[ESI+0x138] / MOV [EBP-0x3c],ECX), not the x87 stack,
       * and emits both stores ahead of the literal ones. */
      *(uint32_t *)&constants[0] = *(uint32_t *)((char *)shader_data + 0x138);
      *(uint32_t *)&constants[1] = *(uint32_t *)((char *)shader_data + 0x13c);
      /* The reference loads 0x5a5e18 before the literal stores (MOV EDX,
       * [0x5a5e18] precedes MOV [EBP-0x34],0x43a00000), which lets it push
       * all four arguments ahead of the block. */
      texture_globals = *(void **)0x5a5e18;
      constants[2] = 320.0f;
      constants[3] = 240.0f;
      constants[4] = 1.0f;
      constants[5] = 0.0f;
      constants[6] = 0.0f;
      constants[7] = 0.0f;
      constants[8] = 0.0f;
      constants[9] = 1.0f;
      constants[10] = 0.0f;
      constants[11] = 0.0f;

      shader_environment_texture_animation_evaluate(
        shader, texture_globals, &constants[7], &constants[11]);

      D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

      csmemset((void *)0x5a5ac0, 0, 0xf0);
      *(uint32_t *)0x5a5b98 = 0x8c61;
      *(uint32_t *)0x5a5b94 = 0x11005;

      if (*(int *)((char *)shader_data + 0x134) == -1) {
        color[0] = *(float *)0x253398 - *(float *)0x5a5bd4 * *(float *)0x253398;
        if (color[0] < *(float *)0x2533c0) {
          color[0] = 0.0f;
        } else if (color[0] > *(float *)0x2533c8) {
          color[0] = 1.0f;
        }
        color[1] = *(float *)0x253398 - *(float *)0x5a5bd8 * *(float *)0x253398;
        if (color[1] < *(float *)0x2533c0) {
          color[1] = 0.0f;
        } else if (color[1] > *(float *)0x2533c8) {
          color[1] = 1.0f;
        }
        color[2] = *(float *)0x253398 - *(float *)0x5a5bdc * *(float *)0x253398;
        if (color[2] < *(float *)0x2533c0) {
          color[2] = 0.0f;
        } else if (color[2] > *(float *)0x2533c8) {
          color[2] = 1.0f;
        }
        *(uint32_t *)0x5a5ae8 = real_rgb_color_to_pixel32(&color[0]);
        *(uint32_t *)0x5a5b48 = 0x4a410b0b;
      } else {
        *(uint32_t *)0x5a5b48 = 0x49480b0b;
      }

      *(uint32_t *)0x5a5b74 = 0x20cd;
      *(uint32_t *)0x5a5b4c = 0xc0c0d0d;
      *(uint32_t *)0x5a5b78 = 0xcd;
      *(uint32_t *)0x5a5b50 = 0xc0c0d0d;
      *(uint32_t *)0x5a5b7c = 0xd;

      *(uint32_t *)0x5a5af4 =
        real_a_rgb_color_to_pixel32(*(float *)((char *)shader_data + 0x2f4),
                                    (float *)((char *)shader_data + 0x2a8));
      *(uint32_t *)0x5a5b14 =
        real_a_rgb_color_to_pixel32(*(float *)((char *)shader_data + 0x2f8),
                                    (float *)((char *)shader_data + 0x2b4));

      *(uint32_t *)0x5a5b34 = 0xc00;
      *(uint32_t *)0x5a5b80 = 0xc00;
      *(uint32_t *)0x5a5b84 = 0xc00;
      *(uint32_t *)0x5a5acc = 0x2c120c11;
      *(uint32_t *)0x5a5b54 = 0x2c020c01;
      *(uint32_t *)0x5a5b58 = 0x2c0d0c0b;
      *(uint32_t *)0x5a5ae0 = 0xc0f0000;
      /* Reference is AND AL,2 / NEG AL / SBB EAX,EAX / AND EAX,0xffffffe8 /
       * ADD EAX,0x20 / OR EAX,0x1c00 / SHL EAX,0x10 — a branchless mask,
       * kept in that form rather than a ternary. */
      *(uint32_t *)0x5a5ae4 =
        ((uint32_t)((-(uint32_t)((*(uint8_t *)((char *)shader_data + 0x28) &
                                  2) != 0) &
                     0xffffffe8u) +
                    0x20u) |
         0x1c00u)
        << 0x10;

      rasterizer_set_pixel_shader((void *)0x5a5ac0);

      rasterizer_draw_dynamic_triangles_static_vertices(vertices_per_primitive, a4, triangle_count, vertex_buffer);

      if (*(uint16_t *)0x3256ba == 2) {
        batch_count = *(int *)0x5a5498;
        triangle_total = *(int *)0x5a5494;
        *(int *)0x5a5498 = batch_count + 1;
        *(int *)0x5a5494 = triangle_total + triangle_count;
        /* The reference loads 0x5a5490 only after the call returns, so the
         * call must not be sequenced against a pending read of it. */
        static_vertices = rasterizer_frame_statistics_count_static_vertices(
          vertices_per_primitive, a4, triangle_count);
        *(int *)0x5a5490 = *(int *)0x5a5490 + static_vertices;
      }
    }
  }
}

void FUN_00164cf0(void *shader, int frame_index, int vertices_per_primitive,
                  int a4, int triangle_count, void *vertex_buffer)
{
  void *shader_data;
  void *texture_globals;
  float constants[12];
  float color[3];
  int reflection_type;
  int lightmap_pass;
  int static_vertices;
  int batch_count;
  int triangle_total;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x99e, true);
    system_exit(-1);
  }

  if (*(uint16_t *)0x3256bc != 0) {
    return;
  }
  if (*(uint8_t *)0x3256d2 == 0) {
    return;
  }

  if (shader == 0) {
    display_assert(
      "shader",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x9a6, true);
    system_exit(-1);
  }

  shader_data = FUN_001906b0(shader, 3);

  reflection_type = *(int16_t *)((char *)shader_data + 0x2d2);
  if (reflection_type == 0 || reflection_type == 2) {
    if ((*(uint8_t *)((char *)shader_data + 0x28) & 2) != 0) {
      reflection_type = 1;
    }
    if (*(int *)((char *)shader_data + 0x134) == -1) {
      reflection_type = 1;
    }
  }

  if (*(float *)((char *)shader_data + 0x2f4) <= *(float *)0x2533c0 &&
      *(float *)((char *)shader_data + 0x2f8) <= *(float *)0x2533c0) {
    return;
  }
  if (*(int *)((char *)shader_data + 0x330) == -1) {
    return;
  }

  if (vertex_buffer == 0) {
    display_assert(
      "vertex_buffer",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x9bf, true);
    system_exit(-1);
  }
  if (reflection_type < 0 || reflection_type >= 3) {
    display_assert(
      "reflection_type>=0 && "
      "reflection_type<NUMBER_OF_SHADER_ENVIRONMENT_REFLECTION_TYPES",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0x9c0, true);
    system_exit(-1);
  }

  rasterizer_set_texture(0, 0, 3, *(int *)((char *)shader_data + 0x134),
                         frame_index);

  D3DDevice_SetTextureStageState(0, 10, 1);
  D3DDevice_SetTextureStageState(0, 0xb, 1);
  D3DDevice_SetTextureStageState(0, 0xd, 2);
  D3DDevice_SetTextureStageState(0, 0xe, 2);
  D3DDevice_SetTextureStageState(0, 0xf, 2);

  rasterizer_set_texture_direct(1, *(int *)(*(char **)0x476204 + 0x1c), 0);

  D3DDevice_SetTextureStageState(1, 10, 3);
  D3DDevice_SetTextureStageState(1, 0xb, 3);
  D3DDevice_SetTextureStageState(1, 0xc, 3);
  D3DDevice_SetTextureStageState(1, 0xd, 2);
  D3DDevice_SetTextureStageState(1, 0xe, 1);
  D3DDevice_SetTextureStageState(1, 0xf, 1);

  rasterizer_set_texture_direct(2, *(int *)(*(char **)0x476204 + 0x1c), 0);

  D3DDevice_SetTextureStageState(2, 10, 3);
  D3DDevice_SetTextureStageState(2, 0xb, 3);
  D3DDevice_SetTextureStageState(2, 0xc, 3);
  D3DDevice_SetTextureStageState(2, 0xd, 2);
  D3DDevice_SetTextureStageState(2, 0xe, 1);
  D3DDevice_SetTextureStageState(2, 0xf, 1);

  rasterizer_set_texture(3, 2, 0, *(int *)((char *)shader_data + 0x330),
                         frame_index);

  /* Stage 3 here DOES carry the 0xc entry (0x164fb4) — unlike FUN_00164690. */
  D3DDevice_SetTextureStageState(3, 10, 3);
  D3DDevice_SetTextureStageState(3, 0xb, 3);
  D3DDevice_SetTextureStageState(3, 0xc, 3);
  D3DDevice_SetTextureStageState(3, 0xd, 2);
  D3DDevice_SetTextureStageState(3, 0xe, 2);
  D3DDevice_SetTextureStageState(3, 0xf, 2);

  D3DDevice_SetRenderState_CullMode(0x901);

  D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
  *(uint32_t *)0x1fb7a4 = 0x10101;
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_Simple(0x40344, 0x304);
  *(uint32_t *)0x1fb790 = 0x304;
  D3DDevice_SetRenderState_Simple(0x40348, 1);
  *(uint32_t *)0x1fb794 = 1;
  D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
  *(uint32_t *)0x1fb7c0 = 0x8006;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x40354, 0x202);
  *(uint32_t *)0x1fb77c = 0x202;
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_ZBias(0);

  FUN_00178b40(0x2a, *(uint16_t *)vertex_buffer, reflection_type);

  /* [0]/[1] move through GPRs (MOV ECX,[ESI+0x138] / MOV [EBP-0x3c],ECX),
   * not the x87 stack, and are stored ahead of the literal ones. */
  *(uint32_t *)&constants[0] = *(uint32_t *)((char *)shader_data + 0x138);
  *(uint32_t *)&constants[1] = *(uint32_t *)((char *)shader_data + 0x13c);
  texture_globals = *(void **)0x5a5e18;
  constants[2] = 320.0f;
  constants[3] = 240.0f;
  constants[4] = 1.0f;
  constants[5] = 0.0f;
  constants[6] = 0.0f;
  constants[7] = 0.0f;
  constants[8] = 0.0f;
  constants[9] = 1.0f;
  constants[10] = 0.0f;
  constants[11] = 0.0f;

  shader_environment_texture_animation_evaluate(shader, texture_globals,
                                                &constants[7], &constants[11]);

  D3DDevice_SetVertexShaderConstant(-0x54, &constants[0], 3);

  csmemset((void *)0x5a5ac0, 0, 0xf0);

  switch (reflection_type) {
  case 1:
    *(uint32_t *)0x5a5b98 = 0x18c61;
    break;
  case 0:
  case 2:
    *(uint32_t *)0x5a5b98 = 0x62e21;
    *(uint32_t *)0x5a5ba0 = 0;
    *(uint32_t *)0x5a5b9c = 0x111;
    break;
  default:
    display_assert(
      "### ERROR unsupported reflection type",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0xa20, true);
    system_exit(-1);
    break;
  }

  *(uint32_t *)0x5a5b94 = 0x11005;

  if (reflection_type == 0 || reflection_type == 2 ||
      *(int *)((char *)shader_data + 0x134) == -1) {
    color[0] = *(float *)0x253398 - *(float *)0x5a5bd4 * *(float *)0x253398;
    if (color[0] < *(float *)0x2533c0) {
      color[0] = 0.0f;
    } else if (color[0] > *(float *)0x2533c8) {
      color[0] = 1.0f;
    }
    color[1] = *(float *)0x253398 - *(float *)0x5a5bd8 * *(float *)0x253398;
    if (color[1] < *(float *)0x2533c0) {
      color[1] = 0.0f;
    } else if (color[1] > *(float *)0x2533c8) {
      color[1] = 1.0f;
    }
    color[2] = *(float *)0x253398 - *(float *)0x5a5bdc * *(float *)0x253398;
    if (color[2] < *(float *)0x2533c0) {
      color[2] = 0.0f;
    } else if (color[2] > *(float *)0x2533c8) {
      color[2] = 1.0f;
    }
    *(uint32_t *)0x5a5ae8 = real_rgb_color_to_pixel32(&color[0]);
    *(uint32_t *)0x5a5b48 = 0x4a410b0b;
  } else {
    *(uint32_t *)0x5a5b48 = 0x49480b0b;
  }

  /* 0x1652df-0x165308: one EAX feeds both 0xc0c0d0d stores, and the 0x20cd/
   * 0xcd/0xd trio is emitted between them. */
  *(uint32_t *)0x5a5b4c = 0xc0c0d0d;
  *(uint32_t *)0x5a5b50 = 0xc0c0d0d;
  *(uint32_t *)0x5a5b74 = 0x20cd;
  *(uint32_t *)0x5a5b78 = 0xcd;
  *(uint32_t *)0x5a5b7c = 0xd;

  *(uint32_t *)0x5a5af4 =
    real_a_rgb_color_to_pixel32(*(float *)((char *)shader_data + 0x2f4),
                                (float *)((char *)shader_data + 0x2a8));
  *(uint32_t *)0x5a5b14 =
    real_a_rgb_color_to_pixel32(*(float *)((char *)shader_data + 0x2f8),
                                (float *)((char *)shader_data + 0x2b4));

  /* 0x16533c-0x16536e: the 0xc00 stores are interleaved with the combiner
   * words (one EAX holds 0xc00 across all three). */
  *(uint32_t *)0x5a5acc = 0x2c120c11;
  *(uint32_t *)0x5a5b34 = 0xc00;
  *(uint32_t *)0x5a5b54 = 0x2c020c01;
  *(uint32_t *)0x5a5b80 = 0xc00;
  *(uint32_t *)0x5a5b58 = 0x2c0d0c0b;
  *(uint32_t *)0x5a5b84 = 0xc00;
  *(uint32_t *)0x5a5ae0 = 0xc0f0000;

  /* Reference is AND CL,2 / NEG CL / SBB ECX,ECX / AND ECX,0xffffffe8 /
   * ADD ECX,0x20 / OR ECX,0x1c00 / SHL ECX,0x10 — a branchless mask. */
  *(uint32_t *)0x5a5ae4 =
    ((uint32_t)((-(uint32_t)((*(uint8_t *)((char *)shader_data + 0x28) & 2) !=
                             0) &
                 0xffffffe8u) +
                0x20u) |
     0x1c00u)
    << 0x10;

  rasterizer_set_pixel_shader((void *)0x5a5ac0);

  if (*(uint16_t *)0x3256b0 != 0 && reflection_type == 2) {
    lightmap_pass = 1;
  } else {
    lightmap_pass = 0;
  }

  rasterizer_draw_dynamic_triangles_static_vertices2(vertices_per_primitive, a4, triangle_count, (const struct vertex_buffer *)vertex_buffer,
               (const struct vertex_buffer *)((char *)vertex_buffer + lightmap_pass * 20));

  if (*(uint16_t *)0x3256ba == 2) {
    batch_count = *(int *)0x5a5498;
    triangle_total = *(int *)0x5a5494;
    *(int *)0x5a5498 = batch_count + 1;
    *(int *)0x5a5494 = triangle_total + triangle_count;
    static_vertices = rasterizer_frame_statistics_count_static_vertices(
      vertices_per_primitive, a4, triangle_count);
    *(int *)0x5a5490 = *(int *)0x5a5490 + static_vertices;
  }
}

/* 12- and 16-byte opaque blocks copied wholesale into the transparent
 * geometry group. The reference lowers each as a run of dword load/store
 * pairs through a LEA'd base pointer (LEA EDX,[ESI+0x74] / LEA EDX,[ESI+0x80]),
 * which is what a struct assignment of each size produces. No field-level
 * evidence exists for the 16-byte block's contents, so it stays opaque; the
 * 12-byte block is the caller's centroid. */
typedef struct {
  uint32_t d[3];
} rasterizer_env_block12_t;

typedef struct {
  uint32_t d[4];
} rasterizer_env_block16_t;

/* 0x165420 — queue one environment geometry batch as a transparent geometry
 * group.
 *
 * Signature: cdecl, bare RET (caller cleans). kb.json previously declared this
 * `(void)`; the reference reads twelve dword parameter slots from [EBP+8] to
 * [EBP+0x34], so the decl is widened to twelve positional parameters. The only
 * caller is FUN_0017ceb0 (0x17ceb4), which is not ported, so no C call site
 * changes. Slot [EBP+0x2c] is never read but occupies a parameter position, so
 * it is declared and left unused (same treatment as FUN_001609b0's a2).
 *
 * Parameter names: `shader`, `centroid` and `geometry_flags` come from the
 * assert strings at TU lines 0xa92/0xa95/0xade. `vertices_per_primitive`, `a5`
 * and `triangle_count` are taken from the slot names of
 * rasterizer_frame_statistics_count_static_vertices' kb decl, which receives
 * them in that order — not from independent evidence. The rest stay positional.
 *
 * geometry_flags is a by-value slot mutated in the frame
 * (OR dword ptr [EBP+0x34],0x1 / ...,0x7), not an out-parameter.
 *
 * Globals (roles unproven beyond the assert string):
 *   0x476ab0  void *  global_d3d_device (assert at __FILE__ line 0xa8b)
 *   0x3256d3  uint8   enable flag; the whole body runs only when non-zero
 *   0x5a5bc8 / 0x5a5bcc / 0x5a5bd0
 *                     camera-relative origin subtracted from the centroid
 *   0x5a5bd4 / 0x5a5bd8 / 0x5a5bdc
 *                     the three coefficients dotted with that difference; the
 *                     negated dot lands at group+0x70 (a sort key)
 *   0x47dbf8  the fixed group record used when the no-queue bit is set
 *   0x47dc88  dword   set to -1 alongside that fixed record
 *   0x47dcac  uint8   latch so the "too many groups" error prints once
 *   0x3256ba  uint16  statistics mode; the counters below run only when it
 *                     is 2. NOTE: 0x3256ba, not the 0x3256bc used elsewhere
 *                     in this TU as a mode guard.
 *   0x5a54a8 / 0x5a54a0 / 0x5a54a4 / 0x5a549c
 *                     frame statistic counters: batch count, triangle
 *                     accumulator, running triangle maximum, and the
 *                     accumulator fed by count_static_vertices.
 *
 * Order-critical, all binary-fixed:
 *   - dx/dy/dz are computed from the *parameter* centroid immediately after
 *     its assert, before the group+0x70 store and before the group+0x74 copy.
 *   - The dot product association is (0x5a5bdc*dz + 0x5a5bd8*dy) + 0x5a5bd4*dx
 *     (FLD 0x5a5bdc first, FADDP, then FLD 0x5a5bd4, FADDP, FCHS).
 *   - shader->base.type is re-read after FUN_001792C0(1): the reference emits
 *     CMP word ptr [EDI+0x24],0x7 twice around that call.
 *   - The statistics counters are pre-read into locals before their stores,
 *     and 0x5a549c is loaded only after count_static_vertices returns.
 *   - The 1.0f pair is stored as the immediate 0x3f800000 at group+0x40 then
 *     group+0x3c (one MOV EAX, two stores).
 *
 * Assert at line 0xa93 fires when shader->base.type == 3; the string reads
 * "shader->base.type!=_shader_type_environment", so type 3 is rejected here.
 */
void FUN_00165420(void *shader, int16_t a2, int a3, int vertices_per_primitive,
                  int a5, int triangle_count, int a7, float *centroid,
                  uint32_t *colors, int a10, int geometry_data,
                  int geometry_flags)
{
  char *group;
  void *shader_data;
  uint32_t *color_source;
  rasterizer_env_block16_t zero;
  float dx;
  float dy;
  float dz;
  int no_queue;
  int batch_count;
  int triangle_total;
  int max_triangles;
  int static_vertices;

  (void)a10;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0xa8b, true);
    system_exit(-1);
  }

  if (*(uint8_t *)0x3256d3 == 0) {
    return;
  }

  if (shader == NULL) {
    display_assert(
      "shader",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0xa92, true);
    system_exit(-1);
  }

  if (*(int16_t *)((char *)shader + 0x24) == 3) {
    display_assert(
      "shader->base.type!=_shader_type_environment",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0xa93, true);
    system_exit(-1);
  }

  if (shader_type_is_valid_for_environment(
        *(uint16_t *)((char *)shader + 0x24)) == 0) {
    display_assert(
      "shader_type_is_valid_for_environment(shader->base.type)",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0xa94, true);
    system_exit(-1);
  }

  if (centroid == NULL) {
    display_assert(
      "centroid",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
      0xa95, true);
    system_exit(-1);
  }

  dx = centroid[0] - *(float *)0x5a5bc8;
  dy = centroid[1] - *(float *)0x5a5bcc;
  dz = centroid[2] - *(float *)0x5a5bd0;

  if (colors != NULL) {
    geometry_flags = geometry_flags | 1;
  }

  if (shader_is_decal(shader) != 0) {
    geometry_flags = geometry_flags | 7;
  }

  no_queue = geometry_flags & 2;
  if (no_queue != 0) {
    group = (char *)0x47dbf8;
    *(uint32_t *)0x47dc88 = 0xffffffff;
  } else {
    group = (char *)rasterizer_transparent_geometry_group_new();
    if (group == NULL) {
      if (*(uint8_t *)0x47dcac != 0) {
        return;
      }
      error(2, "### ERROR too many transparent geometry groups");
      *(uint8_t *)0x47dcac = 1;
      return;
    }
  }

  *(uint32_t *)group = (uint32_t)geometry_flags;
  *(void **)(group + 0xc) = shader;
  *(int16_t *)(group + 0x10) = a2;
  *(int *)(group + 0x44) = vertices_per_primitive;
  *(int *)(group + 0x4c) = a5;
  *(int *)(group + 0x50) = triangle_count;
  *(int *)(group + 0x58) = a7;
  *(int *)(group + 0x5c) = a3;
  *(uint32_t *)(group + 4) = 0;
  *(uint32_t *)(group + 8) = 0;
  *(int16_t *)(group + 0x14) = 0;
  *(uint32_t *)(group + 0x48) = 0;
  *(uint32_t *)(group + 0x54) = 0xffffffff;

  zero.d[0] = 0;
  zero.d[1] = 0;
  zero.d[2] = 0;
  zero.d[3] = 0;

  *(float *)(group + 0x70) =
    -(*(float *)0x5a5bdc * dz + *(float *)0x5a5bd8 * dy +
      *(float *)0x5a5bd4 * dx);

  *(rasterizer_env_block12_t *)(group + 0x74) =
    *(rasterizer_env_block12_t *)centroid;

  if (colors == NULL) {
    color_source = &zero.d[0];
  } else {
    color_source = colors;
  }
  *(rasterizer_env_block16_t *)(group + 0x80) =
    *(rasterizer_env_block16_t *)color_source;

  *(int16_t *)(group + 0x94) = -1;
  *(int16_t *)(group + 0x96) = -1;
  *(uint32_t *)(group + 0x40) = 0x3f800000;
  *(uint32_t *)(group + 0x3c) = 0x3f800000;
  *(uint32_t *)(group + 0x98) = 0;
  *(char *)(group + 0x9d) = 0;
  *(uint32_t *)(group + 0x60) = 0;
  *(int16_t *)(group + 0x64) = 0;

  *(int *)(group + 0x68) = rasterizer_memory_pool_copy(geometry_data, 0x74);
  *(uint32_t *)(group + 0x6c) = 0;

  if (*(int16_t *)((char *)shader + 0x24) == 7) {
    FUN_001792C0(1);
    if (*(int16_t *)((char *)shader + 0x24) == 7) {
      shader_data = FUN_001906b0(shader, 7);
      if ((*(uint8_t *)((char *)shader_data + 0x28) & 8) != 0) {
        if (no_queue != 0) {
          display_assert(
            "!TEST_FLAG(geometry_flags, _rasterizer_geometry_no_queue_bit)",
            "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_environment.c",
            0xade, true);
          system_exit(-1);
        }
        *(uint32_t *)group = *(uint32_t *)group | 2;
        rasterizer_transparent_geometry_group_draw(group, 0);
        FUN_001845b0(group, 1);
        *(uint32_t *)group = *(uint32_t *)group & 0xfffffffd;
        goto drawn;
      }
    }
  }

  if (no_queue != 0) {
    rasterizer_transparent_geometry_group_draw(group, 0);
  }

drawn:

  if (*(uint16_t *)0x3256ba == 2) {
    batch_count = *(int *)0x5a54a8;
    triangle_total = *(int *)0x5a54a0;
    max_triangles = *(int *)0x5a54a4;
    *(int *)0x5a54a8 = batch_count + 1;
    *(int *)0x5a54a0 = triangle_total + triangle_count;
    if (max_triangles < triangle_count) {
      *(int *)0x5a54a4 = triangle_count;
    }
    static_vertices = rasterizer_frame_statistics_count_static_vertices(
      vertices_per_primitive, a5, triangle_count);
    *(int *)0x5a549c = *(int *)0x5a549c + static_vertices;
  }
}
