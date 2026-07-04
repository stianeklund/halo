/* MSVC CRT pow(): compiles to the _CIpow intrinsic (0x1d9e70 dispatcher,
 * body at 0x1d9e94 uses fyl2x). Not in decl.h; declared locally as in
 * objects.c so the compiler emits the intrinsic. */
double pow(double x, double y);

/* rasterizer_plasma_energy_draw (FUN_0016eef0): emit the plasma-energy
 * transparent shader (shader type 10) for one geometry group. Binds the two
 * noise-map textures (primary at shader+0xe0, secondary at shader+0x128),
 * sets fixed-function / render state, computes the two animation scroll
 * matrices (vs const block A: 6 vec4) and the perpendicular/parallel tint
 * colour rows (block B: 3 vec4) into vertex-shader constants, then installs
 * the plasma pixel shader and draws.
 * Original TU: c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_plasma_energy.c
 */
void FUN_0016eef0(void *group)
{
  char *grp = (char *)group;
  char *p; /* plasma shader params base = FUN_001906b0(..)+0x28 */
  int *params; /* grp+0x6c: {color_table, pow_table} pointer pair */
  float *color_table; /* params[0]: 12-byte-stride colour entries */
  float *pow_table; /* params[1]: exponent-table floats [EBP-0xc] */
  float *tint; /* [EBX] tint rgb triple */
  float alpha_c; /* [EBP-8] default 1.0 */
  float alpha_s; /* [EBP-4] default 0.0 */
  float t; /* DAT_005a5e18 scroll time */
  float adiv; /* t / primary_noise_map_animation_period */
  float bdiv; /* t / secondary_noise_map_animation_period */
  float dupA; /* shader+0xa8 diagonal duplicate (primary) */
  float dupB; /* shader+0xf0 diagonal duplicate (secondary) */
  float vsA[24]; /* [EBP-0x9c] vertex-shader const block A (6 vec4) */
  float vsB[12]; /* [EBP-0x3c] vertex-shader const block B (3 vec4) */
  short idx;

  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "plasma_energy.c",
                   0x15, 1);
    system_exit(-1);
  }
  if (*(char *)0x3256fb == 0) {
    return;
  }

  p = (char *)FUN_001906b0(*(void **)(grp + 0xc), 10) + 0x28;
  tint = *(float **)0x2ee708;
  alpha_c = 1.0f;
  alpha_s = 0.0f;

  params = *(int **)(grp + 0x6c);
  if (params != 0) {
    color_table = *(float **)params;
    if (color_table != 0) {
      idx = *(short *)(p + 0x58);
      if (idx >= 1 && idx <= 4) {
        tint = (float *)((char *)color_table + (idx - 1) * 12);
      }
    }
    pow_table = *(float **)(params + 1);
    if (pow_table != 0) {
      idx = *(short *)(p + 4);
      if (idx >= 1 && idx <= 4) {
        alpha_c =
          (float)pow((double)pow_table[idx - 1], (double)*(float *)(p + 8));
      }
      idx = *(short *)(p + 0xc);
      if (idx >= 1 && idx <= 4) {
        alpha_s =
          (float)pow((double)pow_table[idx - 1], (double)*(float *)(p + 0x14)) *
          *(float *)(p + 0x10);
      }
    }
  }

  /* Bind the two noise-map textures and their fixed-function stage state. */
  rasterizer_set_texture(0, 1, 0, *(int *)(p + 0xb8),
                         *(unsigned short *)(grp + 0x10));
  SetTextureStageStateSmart(0, 0xa, 1);
  SetTextureStageStateSmart(0, 0xb, 1);
  SetTextureStageStateSmart(0, 0xc, 1);
  SetTextureStageStateSmart(0, 0xd, 2);
  SetTextureStageStateSmart(0, 0xe, 2);
  SetTextureStageStateSmart(0, 0xf, 2);
  rasterizer_set_texture(1, 1, 0, *(int *)(p + 0x100),
                         *(unsigned short *)(grp + 0x10));
  SetTextureStageStateSmart(1, 0xa, 1);
  SetTextureStageStateSmart(1, 0xb, 1);
  SetTextureStageStateSmart(1, 0xc, 1);
  SetTextureStageStateSmart(1, 0xd, 2);
  SetTextureStageStateSmart(1, 0xe, 2);
  SetTextureStageStateSmart(1, 0xf, 2);

  D3DDevice_SetRenderState_CullMode(0);
  /* 0x10101 is correct despite the delinked disasm showing $0x101: the
   * imm carries a dir32 reloc to XBE_FILE_HEADER_00010000, so the real
   * value is 0x10000 (XBE base) + 0x101 addend = 0x10101. */
  D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
  *(uint32_t *)0x1fb7a4 = 0x10101;
  D3DDevice_SetRenderState_Simple(0x40304, 1);
  *(uint32_t *)0x1fb784 = 1;
  D3DDevice_SetRenderState_Simple(0x40344, 0x302);
  *(uint32_t *)0x1fb790 = 0x302;
  D3DDevice_SetRenderState_Simple(0x40348, 1);
  *(uint32_t *)0x1fb794 = 1;
  D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
  *(uint32_t *)0x1fb7c0 = 0x8006;
  D3DDevice_SetRenderState_Simple(0x40300, 0);
  *(uint32_t *)0x1fb788 = 0;
  D3DDevice_SetRenderState_ZEnable(1);
  D3DDevice_SetRenderState_Simple(0x4035c, 0);
  *(uint32_t *)0x1fb798 = 0;
  D3DDevice_SetRenderState_Simple(0x40354, 0x203);
  *(uint32_t *)0x1fb77c = 0x203;
  D3DDevice_SetRenderState_ZBias(0);

  FUN_00178b40(0xf, FUN_00184610(grp), 0);

  if (!(*(float *)(p + 0x98) != 0.0f)) {
    display_assert("plasma->primary_noise_map_animation_period!=0.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "plasma_energy.c",
                   0x69, 1);
    system_exit(-1);
  }
  if (!(*(float *)(p + 0xe0) != 0.0f)) {
    display_assert("plasma->secondary_noise_map_animation_period!=0.0f",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "plasma_energy.c",
                   0x6a, 1);
    system_exit(-1);
  }

  if (alpha_s < *(float *)0x2a39e0) {
    alpha_s = 0.0f;
  }

  /* Vertex-shader const block A (register -0x51): two scroll matrices.
   * Rows 0-2 scale t/primary_period by shader+0xc4/0xc8/0xcc with the
   * shader+0xd0 duplicate on the diagonal; rows 3-5 use t/secondary_period
   * with shader+0x10c/0x110/0x114 and the shader+0x118 duplicate. alpha_s
   * rides in A[2]. */
  t = *(float *)0x5a5e18;
  adiv = t / *(float *)(p + 0x98);
  bdiv = t / *(float *)(p + 0xe0);
  dupA = *(float *)(p + 0xa8);
  dupB = *(float *)(p + 0xf0);
  vsA[0] = dupA;
  vsA[1] = 0.0f;
  vsA[2] = alpha_s;
  vsA[3] = adiv * *(float *)(p + 0x9c);
  vsA[4] = 0.0f;
  vsA[5] = dupA;
  vsA[6] = 0.0f;
  vsA[7] = adiv * *(float *)(p + 0xa0);
  vsA[8] = 0.0f;
  vsA[9] = 0.0f;
  vsA[10] = dupA;
  vsA[11] = adiv * *(float *)(p + 0xa4);
  vsA[12] = dupB;
  vsA[13] = 0.0f;
  vsA[14] = 0.0f;
  vsA[15] = bdiv * *(float *)(p + 0xe4);
  vsA[16] = 0.0f;
  vsA[17] = dupB;
  vsA[18] = 0.0f;
  vsA[19] = bdiv * *(float *)(p + 0xe8);
  vsA[20] = 0.0f;
  vsA[21] = 0.0f;
  vsA[22] = dupB;
  vsA[23] = bdiv * *(float *)(p + 0xec);

  /* Block B (register -0x54): identity row, (perp-parallel)*colour row, and
   * parallel*colour row.  perp rgba = shader+0x64/0x68/0x6c/0x60,
   * parallel rgba = shader+0x74/0x78/0x7c/0x70; alpha uses alpha_c. */
  vsB[0] = 1.0f;
  vsB[1] = 1.0f;
  vsB[2] = 1.0f;
  vsB[3] = 1.0f;
  vsB[4] = (*(float *)(p + 0x3c) - *(float *)(p + 0x4c)) * tint[0];
  vsB[5] = (*(float *)(p + 0x40) - *(float *)(p + 0x50)) * tint[1];
  vsB[6] = (*(float *)(p + 0x44) - *(float *)(p + 0x54)) * tint[2];
  vsB[7] = (*(float *)(p + 0x38) - *(float *)(p + 0x48)) * alpha_c;
  vsB[8] = *(float *)(p + 0x4c) * tint[0];
  vsB[9] = *(float *)(p + 0x50) * tint[1];
  vsB[10] = *(float *)(p + 0x54) * tint[2];
  vsB[11] = alpha_c * *(float *)(p + 0x48);

  D3DDevice_SetVertexShaderConstant(-0x51, vsA, 6);
  D3DDevice_SetVertexShaderConstant(-0x54, vsB, 3);

  /* Pixel-shader state block for the plasma-energy shader. */
  csmemset((void *)0x5a5ac0, 0, 0xf0);
  *(uint32_t *)0x5a5b98 = 0x42;
  *(uint32_t *)0x5a5b94 = 0x104;
  *(uint32_t *)0x5a5ac0 = 0x820a920;
  *(uint32_t *)0x5a5b28 = 0xc00;
  *(uint32_t *)0x5a5b48 = 0x1920b820;
  *(uint32_t *)0x5a5b74 = 0xc00;
  *(uint32_t *)0x5a5ac4 = 0x1c1c0c0c;
  *(uint32_t *)0x5a5b2c = 0x24c00;
  *(uint32_t *)0x5a5b4c = 0;
  *(uint32_t *)0x5a5b78 = 0;
  *(uint32_t *)0x5a5ac8 = 0x5c5c;
  *(uint32_t *)0x5a5b30 = 0x4d00;
  *(uint32_t *)0x5a5b50 = 0;
  *(uint32_t *)0x5a5b7c = 0;
  *(uint32_t *)0x5a5acc = 0x14150000;
  *(uint32_t *)0x5a5b34 = 0x40;
  *(uint32_t *)0x5a5b54 = 0x1c051da0;
  *(uint32_t *)0x5a5b80 = 0xc00;
  *(uint32_t *)0x5a5ae0 = 0xc0f0000;
  *(uint32_t *)0x5a5ae4 = 0x1c1c1400;
  rasterizer_set_pixel_shader((void *)0x5a5ac0);
  FUN_00174510(grp, 0);
}

/* rasterizer_transparent_geometry_group_draw: draw one sorted transparent
 * geometry group, dispatching per shader type (generic/chicago/glass/meter/
 * plasma/water), handling extra layers via self-recursion, predicted shader
 * pre-pass, debug tint mode, and secondary (dirty) group passes (0x174d10) */
void rasterizer_transparent_geometry_group_draw(void *group, int dirty)
{
  char *grp = (char *)group;
  char success; /* [EBP-0x1] draw success accumulator */
  char draw_secondary; /* [EBP-0x2d] draw dirty secondary groups after */
  char *sh;
  int vertex_type; /* [EBP-0x38] */
  int permutation; /* [EBP-0x40] */
  int pass; /* [EBP-0x80] two-pass layer loop */
  char has_multi; /* [EBP+0xb] case-1 secondary-map flag */
  short sec_count; /* [EBP+0xa] secondary group count */
  char *sec;
  short si;
  char *rec;
  char *next;

  success = 1;
  draw_secondary = 0;
  if (grp == (char *)0) {
    display_assert("group",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "transparent_geometry.c",
                   0xe8, 1);
    system_exit(-1);
  }
  if (*(int *)0x476ab0 == 0) {
    display_assert("global_d3d_device",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                   "transparent_geometry.c",
                   0xe9, 1);
    system_exit(-1);
  }
  if (*(int *)(grp + 0x98) != 0 && (char)dirty == 0) {
    return;
  }
  if (FUN_00184570(grp) == 0) {
    return;
  }
  FUN_001845b0(grp, 0);
  if (*(short *)(grp + 0x94) != -1) {
    rasterizer_transparent_geometry_group_draw(
      rasterizer_transparent_geometry_group_get(*(short *)(grp + 0x94)), dirty);
  }

  /* rasterizer_debug_transparents: draw with random per-group tint */
  if (*(char *)0x3256c2 != 0) {
    if ((*grp & 2) == 0 && *(int *)(grp + 0xc) != 0 &&
        *(int *)(grp + 0x90) != -1) {
      short vertex_shader_table[12];
      char solid_color; /* [EBP+0xb] debug value forces solid color */
      unsigned int seed;
      float argb[4]; /* [EBP-0x2c] alpha,red,green,blue */
      float blue;
      float minimum;
      float maximum;
      float range_scale;
      float tint;
      float dim;
      float skin_xform[12]; /* [EBP-0xb0] */
      struct {
        void *matrices;
        short node_count;
      } skinning; /* [EBP-0x1c] */
      char text_buffer[96]; /* [EBP-0x550] */

      vertex_shader_table[0] = 6;
      vertex_shader_table[1] = 6;
      vertex_shader_table[2] = 6;
      vertex_shader_table[3] = 6;
      vertex_shader_table[4] = 0xd;
      vertex_shader_table[5] = 0xd;
      vertex_shader_table[6] = 0x41;
      vertex_shader_table[7] = 0x41;
      vertex_shader_table[8] = -1;
      vertex_shader_table[9] = -1;
      vertex_shader_table[10] = -1;
      vertex_shader_table[11] = -1;
      vertex_type = (short)FUN_00184610(grp);
      if (*(short *)0x3256ea >= 1000 || *(short *)0x3256ea < 0) {
        solid_color = 1;
      } else {
        solid_color = 0;
      }
      if ((short)vertex_type < 0 || (short)vertex_type >= 0xc) {
        display_assert(
          "vertex_type>=0 && vertex_type<NUMBER_OF_RASTERIZER_VERTEX_TYPES",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
          "geometry.c",
          0x118, 1);
        system_exit(-1);
      }
      if (vertex_shader_table[(short)vertex_type] == -1) {
        display_assert("vertex_shader_table[vertex_type]!=NONE",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x119, 1);
        system_exit(-1);
      }
      FUN_00178b40(
        (int)(0xffff0000u |
              (unsigned short)vertex_shader_table[(short)vertex_type]),
        vertex_type, 0);
      D3DDevice_SetRenderState_CullMode(0);
      D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
      *(uint32_t *)0x1fb7a4 = 0x10101;
      D3DDevice_SetRenderState_Simple(0x40304, (unsigned char)solid_color);
      *(uint32_t *)0x1fb784 = (unsigned char)solid_color;
      D3DDevice_SetRenderState_Simple(0x40344, 1);
      *(uint32_t *)0x1fb790 = 1;
      D3DDevice_SetRenderState_Simple(0x40348, 1);
      *(uint32_t *)0x1fb794 = 1;
      D3DDevice_SetRenderState_Simple(0x40350, 0x8006);
      *(uint32_t *)0x1fb7c0 = 0x8006;
      D3DDevice_SetRenderState_Simple(0x40300, 0);
      *(uint32_t *)0x1fb788 = 0;
      D3DDevice_SetRenderState_ZEnable(1);
      D3DDevice_SetRenderState_Simple(0x4035c, 0);
      *(uint32_t *)0x1fb798 = 0;
      D3DDevice_SetRenderState_Simple(0x40354, 0x203);
      *(uint32_t *)0x1fb77c = 0x203;
      D3DDevice_SetRenderState_ZBias(0);
      csmemset((void *)0x5a5ac0, 0, 0xf0);
      seed = (unsigned int)((int)*(short *)0x3256ea + *(int *)(grp + 0x90));
      argb[0] = 1.0f;
      argb[1] = random_math_real(&seed);
      argb[2] = random_math_real(&seed);
      blue = random_math_real(&seed);
      /* normalize color so channels span [0.15, 0.33]; MIN/MAX macros
       * re-evaluate their arguments as in the original */
      minimum = (argb[1] <= ((argb[2] <= blue) ? argb[2] : blue)) ?
                  argb[1] :
                  ((argb[2] <= blue) ? argb[2] : blue);
      maximum = (argb[1] <= ((blue < argb[2]) ? argb[2] : blue)) ?
                  ((blue < argb[2]) ? argb[2] : blue) :
                  argb[1];
      range_scale = *(float *)0x2a52b4 / (maximum - minimum);
      argb[1] = (argb[1] - minimum) * range_scale + *(float *)0x256140;
      argb[2] = (argb[2] - minimum) * range_scale + *(float *)0x256140;
      argb[3] = (blue - minimum) * range_scale + *(float *)0x256140;
      if (solid_color != 0) {
        tint = *(float *)0x325724;
        if (tint < *(float *)0x2533c0) {
          tint = *(float *)0x29d598;
        } else if (tint > *(float *)0x2533c8) {
          tint = 1.0f;
        } else if (tint == *(float *)0x2533c0) {
          tint = *(float *)0x29d598;
        }
        if (*(short *)0x3256ea >= 1000) {
          argb[1] = argb[1] * tint;
          argb[2] = argb[2] * tint;
          argb[3] = tint * argb[3];
        } else {
          argb[1] = tint;
          argb[2] = tint;
          argb[3] = tint;
        }
      }
      if (!(argb[1] >= *(float *)0x2533c0 && argb[1] <= *(float *)0x2533c8)) {
        display_assert("color.red >=0.0f && color.red <=1.0f",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x156, 1);
        system_exit(-1);
      }
      if (!(argb[2] >= *(float *)0x2533c0 && argb[2] <= *(float *)0x2533c8)) {
        display_assert("color.green>=0.0f && color.green<=1.0f",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x157, 1);
        system_exit(-1);
      }
      if (!(argb[3] >= *(float *)0x2533c0 && argb[3] <= *(float *)0x2533c8)) {
        display_assert("color.blue >=0.0f && color.blue <=1.0f",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x158, 1);
        system_exit(-1);
      }
      *(uint32_t *)0x5a5b6c = FUN_000d1dd0(&argb[1]);
      *(uint32_t *)0x5a5b94 = 1;
      *(uint32_t *)0x5a5ae0 = 1;
      rasterizer_set_pixel_shader((void *)0x5a5ac0);
      if (*(void **)(grp + 0x60) != (void *)0 && *(short *)(grp + 0x64) != 0) {
        skinning.matrices = *(void **)(grp + 0x60);
        skinning.node_count = *(short *)(grp + 0x64);
      } else {
        skinning.matrices = *(void **)0x31fc60;
        skinning.node_count = 1;
      }
      rasterizer_set_model_skinning(&skinning);
      skin_xform[0] = 1.0f;
      skin_xform[1] = 0.0f;
      skin_xform[2] = 0.0f;
      skin_xform[3] = 0.0f;
      skin_xform[4] = 0.0f;
      skin_xform[5] = 1.0f;
      skin_xform[6] = 0.0f;
      skin_xform[7] = 0.0f;
      skin_xform[8] = 0.0f;
      skin_xform[9] = 0.0f;
      skin_xform[10] = 1.0f;
      skin_xform[11] = 0.0f;
      if ((*grp & 0x20) != 0) {
        skin_xform[0] = *(float *)0x5a5c64;
        skin_xform[1] = *(float *)0x5a5c70;
        skin_xform[2] = *(float *)0x5a5c7c;
        skin_xform[3] = *(float *)0x5a5bc8;
        skin_xform[4] = *(float *)0x5a5c68;
        skin_xform[5] = *(float *)0x5a5c74;
        skin_xform[6] = *(float *)0x5a5c80;
        skin_xform[7] = *(float *)0x5a5bcc;
        skin_xform[8] = *(float *)0x5a5c6c;
        skin_xform[9] = *(float *)0x5a5c78;
        skin_xform[10] = *(float *)0x5a5c84;
        skin_xform[11] = *(float *)0x5a5bd0;
      }
      D3DDevice_SetVertexShaderConstant(0x58, skin_xform, 3);
      success = 1;
      FUN_00174510(grp, 0);
      if (solid_color == 0) {
        crt_sprintf(text_buffer, "%.03f", (double)*(float *)(grp + 0x70));
        argb[1] = argb[1] * *(float *)0x254644;
        if (argb[1] < *(float *)0x2533c0) {
          argb[1] = 0.0f;
        } else if (argb[1] > *(float *)0x2533c8) {
          argb[1] = 1.0f;
        }
        dim = argb[1] * *(float *)0x254644;
        if (dim < *(float *)0x2533c0) {
          argb[2] = 0.0f;
          argb[3] = 0.0f;
        } else if (dim > *(float *)0x2533c8) {
          argb[2] = 1.0f;
          argb[3] = 1.0f;
        } else {
          argb[2] = dim;
          argb[3] = dim;
        }
        FUN_00189cb0(0, grp + 0x74, text_buffer, (int)argb);
      }
    }
    goto tail;
  }

  /* predicted shaders: pre-set state for run of type-2 groups w/ same tag */
  if (*(short *)(grp + 0x14) == 2 && *(int *)(grp + 8) != *(int *)0x47e4b8 &&
      (char)dirty == 0) {
    char *g2;
    int first_tag;
    struct {
      void *matrices;
      short node_count;
    } skinning2;

    first_tag = *(int *)(grp + 8);
    g2 = grp;
    vertex_type = FUN_00184610(grp);
    FUN_00178b40(0xd, vertex_type, 0);
    SetRenderStateSmart(0x7f, 0);
    SetRenderStateSmart(0x43, 0);
    SetRenderStateSmart(0x3b, 0);
    SetRenderStateSmart(0x3c, 0);
    SetRenderStateSmart(0x7b, 1);
    SetRenderStateSmart(0x40, 1);
    SetRenderStateSmart(0x39, 0x203);
    D3DDevice_SetRenderState_ZBias(0);
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(uint32_t *)0x5a5b94 = 1;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    do {
      if (*(int *)(g2 + 8) != first_tag || *(short *)(g2 + 0x14) != 2) {
        break;
      }
      if (shader_ignores_effect(*(void **)(g2 + 0xc)) == 0) {
        if (*(void **)(g2 + 0x60) != (void *)0 && *(short *)(g2 + 0x64) != 0) {
          skinning2.node_count = *(short *)(g2 + 0x64);
          skinning2.matrices = *(void **)(g2 + 0x60);
        } else {
          skinning2.node_count = 1;
          skinning2.matrices = *(void **)0x31fc60;
        }
        rasterizer_set_model_skinning(&skinning2);
        if (*(int *)(grp + 0x68) != 0) {
          rasterizer_set_model_lighting(*(void **)(g2 + 0x68));
        }
        FUN_00174510(g2, 0);
      }
      g2 = (char *)rasterizer_transparent_geometry_next_group(g2);
    } while (g2 != (char *)0);
  }

  if ((*grp & 2) == 0) {
    if (*(short *)0x5a5bc0 == 0 && (char)dirty == 0) {
      sh = *(char **)(grp + 0xc);
      if (*(char *)0x3256fa == 0) {
        if (sh == (char *)0 ||
            (*(short *)(sh + 0x24) != 7 && shader_is_water_decal(sh) == 0)) {
          FUN_001595c0();
        }
      } else if (sh != (char *)0 && *(short *)(sh + 0x24) == 4 &&
                 *(short *)(grp + 0x14) == 1 &&
                 *(int *)(grp + 8) != *(int *)0x47e4b8) {
        FUN_001595c0();
      }
    }
    if ((*grp & 2) == 0 && *(short *)0x5a5bc0 == 0 &&
        *(short *)(grp + 0x14) == 1 && *(int *)(grp + 0xc) != 0 &&
        *(short *)(*(char **)(grp + 0xc) + 0x24) == 4 && (char)dirty == 0) {
      next = (char *)rasterizer_transparent_geometry_next_group(grp);
      if (next == (char *)0 || *(short *)(next + 0x14) != 1 ||
          *(int *)(next + 8) != *(int *)(grp + 8) ||
          *(int *)(next + 0xc) == 0 ||
          *(short *)(*(char **)(next + 0xc) + 0x24) != 4) {
        draw_secondary = 1;
      }
    }
  }

  if (*(int *)(grp + 0xc) == 0) {
    /* group with no shader: invoke user callback stored in the record */
    (*(void (**)(int, int))(grp + 0x48))(*(int *)(grp + 0x4c),
                                         *(int *)(grp + 0x50));
    goto tail;
  }

  permutation = shader_get_vertex_shader_permutation(*(void **)(grp + 0xc));
  vertex_type = FUN_00184610(grp);
  if ((*grp & 2) == 0) {
    struct {
      void *matrices;
      short node_count;
    } skinning3;
    if (*(void **)(grp + 0x60) != (void *)0 && *(short *)(grp + 0x64) != 0) {
      skinning3.node_count = *(short *)(grp + 0x64);
      skinning3.matrices = *(void **)(grp + 0x60);
    } else {
      skinning3.node_count = 1;
      skinning3.matrices = *(void **)0x31fc60;
    }
    rasterizer_set_model_skinning(&skinning3);
    if (*(int *)(grp + 0x68) != 0) {
      rasterizer_set_model_lighting(*(void **)(grp + 0x68));
    }
  }
  if ((*grp & 8) != 0) {
    if (*(short *)0x5a5bc0 == 0) {
      rasterizer_set_frustum_z(0.00390625f, 1024.0f);
    }
    SetRenderStateSmart(0x7b, 0);
    SetRenderStateSmart(0x81, 0);
  } else {
    SetRenderStateSmart(0x7b, 1);
    SetRenderStateSmart(0x40, 0);
    SetRenderStateSmart(0x39, 0x203);
    SetRenderStateSmart(0x81,
                        -(int)(shader_is_decal(*(void **)(grp + 0xc)) != 0) &
                          *(int *)0x32570c);
  }

  pass = 0;
  do {
    if ((char)*grp < 0) {
      if (*(short *)(grp + 0x14) == 1) {
        if ((short)pass > 0) {
          break;
        }
        rasterizer_set_frustum_z(*(float *)0x32569c, *(float *)0x3256a0);
      } else if ((short)pass != 0) {
        FUN_00158ae0(2);
        SetRenderStateSmart(0x7b, 0);
      } else {
        sh = *(char **)(grp + 0xc);
        if (sh != (char *)0 && *(short *)(sh + 0x24) == 1) {
          char *senv = (char *)FUN_001906b0(sh, 1);
          if ((*(unsigned char *)(senv + 0x28) & 4) != 0) {
            goto next_pass;
          }
        }
        FUN_00158ae0(3);
      }
    } else if ((short)pass > 0) {
      break;
    }

    sh = *(char **)(grp + 0xc);
    switch (*(short *)(sh + 0x24)) {
    case 1: {
      /* shader_environment-style multitexture path */
      char *env;
      char env_flags_bit1; /* BL: (shader->flags >> 1) & 1 */
      float skin_xform1[12]; /* [EBP-0x108] */
      float texanim1[16]; /* [EBP-0x1f0] rows 2,3 written by texture anim */
      float fog_consts[8]; /* [EBP-0xa0] */
      float opacity;
      float fog_scale;
      int stage_count;
      int idx;

      env = (char *)FUN_001906b0(sh, 1);
      /* has_multi = secondary map used as a regular multitexture stage.
       * A z-sprite secondary map (env+0x5c == 2) is NOT a multitexture
       * stage — it goes through the dedicated z-sprite final-combiner
       * path instead (original 0x175830: jne keeps 1, i.e. != 2). */
      if (*(int *)(env + 0x58) != -1 && *(short *)(env + 0x5c) != 2) {
        has_multi = 1;
      } else {
        has_multi = 0;
      }
      env_flags_bit1 = (char)((*(unsigned char *)(env + 0x28) >> 1) & 1);
      rasterizer_set_texture_bitmap_data(0, *(void **)(grp + 0x5c));
      SetTextureStageStateSmart(0, 0xa,
                                (*(unsigned char *)(env + 0x2e) & 2) | 1);
      SetTextureStageStateSmart(
        0, 0xb, ((*(unsigned char *)(env + 0x2e) & 4) | 2) >> 1);
      SetTextureStageStateSmart(
        0, 0xd, 2 - (int)((*(unsigned char *)(env + 0x2e) & 1) != 0));
      SetTextureStageStateSmart(
        0, 0xe, 2 - (int)((*(unsigned char *)(env + 0x2e) & 1) != 0));
      SetTextureStageStateSmart(
        0, 0xf, 2 - (int)((*(unsigned char *)(env + 0x2e) & 1) != 0));
      if (*(int *)(env + 0x58) != -1) {
        rasterizer_set_texture(1, 0, 1, *(int *)(env + 0x58),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(1, 0xa,
                                  (*(unsigned char *)(env + 0x5e) & 2) | 1);
        SetTextureStageStateSmart(
          1, 0xb, ((*(unsigned char *)(env + 0x5e) & 4) | 2) >> 1);
        SetTextureStageStateSmart(
          1, 0xd, 2 - (int)((*(unsigned char *)(env + 0x5e) & 1) != 0));
        SetTextureStageStateSmart(
          1, 0xe, 2 - (int)((*(unsigned char *)(env + 0x5e) & 1) != 0));
        SetTextureStageStateSmart(
          1, 0xf, 2 - (int)((*(unsigned char *)(env + 0x5e) & 1) != 0));
      }
      SetRenderStateSmart(0x7f, 0);
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(0x3c, 0);
      FUN_001580b0(*(unsigned short *)(env + 0x2a));
      FUN_00178b40(0x41, vertex_type, permutation);
      skin_xform1[0] = 1.0f;
      skin_xform1[1] = 0.0f;
      skin_xform1[2] = 0.0f;
      skin_xform1[3] = 0.0f;
      skin_xform1[4] = 0.0f;
      skin_xform1[5] = 1.0f;
      skin_xform1[6] = 0.0f;
      skin_xform1[7] = 0.0f;
      skin_xform1[8] = 0.0f;
      skin_xform1[9] = 0.0f;
      skin_xform1[10] = 1.0f;
      skin_xform1[11] = 0.0f;
      texanim1[0] = 1.0f;
      texanim1[1] = 0.0f;
      texanim1[2] = 0.0f;
      texanim1[3] = 0.0f;
      texanim1[4] = 0.0f;
      texanim1[5] = 1.0f;
      texanim1[6] = 0.0f;
      texanim1[7] = 0.0f;
      texanim1[8] = 0.0f;
      texanim1[9] = 0.0f;
      texanim1[10] = 0.0f;
      texanim1[11] = 0.0f;
      texanim1[12] = 0.0f;
      texanim1[13] = 0.0f;
      texanim1[14] = 0.0f;
      texanim1[15] = 0.0f;
      if ((*grp & 0x20) != 0) {
        skin_xform1[0] = *(float *)0x5a5c64;
        skin_xform1[1] = *(float *)0x5a5c70;
        skin_xform1[2] = *(float *)0x5a5c7c;
        skin_xform1[3] = *(float *)0x5a5bc8;
        skin_xform1[4] = *(float *)0x5a5c68;
        skin_xform1[5] = *(float *)0x5a5c74;
        skin_xform1[6] = *(float *)0x5a5c80;
        skin_xform1[7] = *(float *)0x5a5bcc;
        skin_xform1[8] = *(float *)0x5a5c6c;
        skin_xform1[9] = *(float *)0x5a5c78;
        skin_xform1[10] = *(float *)0x5a5c84;
        skin_xform1[11] = *(float *)0x5a5bd0;
      }
      if (has_multi != 0) {
        FUN_00190e10(env + 0x60, *(void **)(grp + 0x6c), *(float *)(grp + 0x3c),
                     *(float *)(grp + 0x40), 0.0f, 0.0f, 0.0f,
                     *(float *)0x5a5e18, &texanim1[8], &texanim1[12]);
      }
      D3DDevice_SetVertexShaderConstant(0x58, skin_xform1, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__INVERSE_OFFSET, "
                     "vsh_constants__inverse, VSH_CONSTANTS__INVERSE_COUNT)");
      }
      D3DDevice_SetVertexShaderConstant(-0x51, texanim1, 4);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(
          0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
             "VSH_CONSTANTS__TEXANIM_OFFSET, vsh_constants__texanim, 4)");
      }
      if (*(char *)0x325718 != 0 && *(short *)(env + 0x5c) == 2 &&
          *(int *)(env + 0x58) != -1 && (char)*grp >= 0) {
        /* z-sprite fog constants */
        opacity = 1.0f;
        if (*(float *)(env + 0x9c) != *(float *)0x2533c0) {
          opacity = *(float *)(env + 0x9c);
        }
        fog_scale = *(float *)0x2a50dc;
        if (*(char *)0x32568c != 0) {
          fog_scale = *(float *)0x2a50e0;
        }
        fog_consts[4] = *(float *)0x5a5bd4;
        fog_consts[5] = *(float *)0x5a5bd8;
        fog_consts[6] = *(float *)0x5a5bdc;
        fog_consts[0] = (*(float *)0x5a5c08 * fog_scale) /
                        (*(float *)0x5a5c08 - *(float *)0x5a5c04);
        fog_consts[1] = -(fog_consts[0] * *(float *)0x5a5c04);
        fog_consts[2] = opacity * *(float *)(env + 0x98);
        fog_consts[3] = *(float *)0x5a5c04 + *(float *)0x25bb10;
        fog_consts[7] = -FUN_00013070((float *)0x5a5bd4, (float *)0x5a5bc8);
        D3DDevice_SetVertexShaderConstant(-0x3f, fog_consts, 2);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(0,
                       "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                       "device, VSH_CONSTANTS__ZSPRITE_OFFSET, "
                       "vsh_constants__zsprite, VSH_CONSTANTS__ZSPRITE_COUNT)");
        }
      }
      csmemset((void *)0x5a5ac0, 0, 0xf0);
      *(uint32_t *)0x5a5ac0 = 0x18201415;
      *(uint32_t *)0x5a5b28 = 0xc4;
      *(uint32_t *)0x5a5ae0 = 0xc;
      *(uint32_t *)0x5a5ae4 = 0x1c00;
      if (env_flags_bit1 != 0) {
        *(uint32_t *)0x5a5b48 = 0x8080000;
        *(uint32_t *)0x5a5b74 = 0xc0;
        *(uint32_t *)0x5a5b4c = 0xc0c0000;
        *(uint32_t *)0x5a5b78 = 0xc0;
        *(uint32_t *)0x5a5b50 = 0x250c0508;
        *(uint32_t *)0x5a5b7c = 0xc00;
        stage_count = 3;
      } else {
        *(uint32_t *)0x5a5b48 = 0x8050000;
        *(uint32_t *)0x5a5b74 = 0xc0;
        stage_count = 1;
      }
      if (has_multi != 0) {
        idx = (short)stage_count * 4;
        *(uint32_t *)(0x5a5ac0 + idx) = 0x1c190000;
        *(uint32_t *)(0x5a5b28 + idx) = 0xc0;
        *(uint32_t *)(0x5a5b48 + idx) = 0xc090000;
        *(uint32_t *)(0x5a5b74 + idx) = 0xc0;
        stage_count = stage_count + 1;
      }
      if (*(char *)0x325718 == 0 || *(short *)(env + 0x5c) != 2 ||
          *(int *)(env + 0x58) == -1 || (char)*grp < 0) {
        *(uint32_t *)0x5a5b98 = ((unsigned int)(has_multi != 0) << 5) | 1;
      } else {
        *(uint32_t *)0x5a5b98 = 0x54421;
        *(uint32_t *)0x5a5ba0 = 0x110000;
        *(uint32_t *)0x5a5b9c = 0;
        SetTextureStageStateSmart(1, 0xa, 1);
        SetTextureStageStateSmart(1, 0xb, 1);
        SetTextureStageStateSmart(1, 0xd, 2);
        SetTextureStageStateSmart(1, 0xe, 2);
        SetTextureStageStateSmart(1, 0xf, 2);
        D3DDevice_SetStreamSource(1, *(void **)0x47e4bc, 2);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(0,
                       "IDirect3DDevice8_SetStreamSource(global_d3d_device, 1, "
                       "rasterizer_xbox_transparent_geometry_texcoord_stream, "
                       "2*sizeof(byte))");
        }
        rasterizer_set_texture(1, 0, 0, *(int *)(env + 0x58),
                               *(unsigned short *)(grp + 0x10));
      }
      if (*(char *)0x3256d4 != 0 && (*grp & 4) == 0) {
        switch (*(short *)(env + 0x2a)) {
        case 0:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5ac0 + idx) = 0x1c140000;
          *(uint32_t *)(0x5a5b28 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 1:
        case 5:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc142034;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 2:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc14a034;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 3:
        case 4:
        case 6:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc140000;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        case 7:
          idx = (short)stage_count * 4;
          *(uint32_t *)(0x5a5ac0 + idx) = 0x1c140000;
          *(uint32_t *)(0x5a5b28 + idx) = 0xc00;
          *(uint32_t *)(0x5a5b48 + idx) = 0xc140000;
          *(uint32_t *)(0x5a5b74 + idx) = 0xc00;
          stage_count = stage_count + 1;
          *(int *)0x5a5b94 = (short)stage_count;
          break;
        default:
          display_assert("### ERROR unsupported framebuffer blend function",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x3a2, 1);
          system_exit(-1);
        }
      } else {
        *(int *)0x5a5b94 = (short)stage_count;
      }
      goto set_shader_and_draw;
    }

    case 4:
      /* model effect: only valid for object groups; drawn via decal path */
      if (*(short *)(grp + 0x14) != 1) {
        display_assert(
          "### ERROR unsupported model effect type in transparent group",
          "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
          "geometry.c",
          0x2ab, 1);
        system_exit(-1);
      }
      if (*(char *)0x47e4c0 != 0) {
        return;
      }
      FUN_00159900(grp);
      break;
    case 5: {
      /* shader_transparent_generic */
      char *gen;
      char *layers;
      char *map;
      char *stage;
      char *bitm;
      int frame_index; /* [EBP-0x34] */
      int n;
      int m;
      int j;
      short gtype;
      short first_map_type; /* [EBP-0x14] */
      short first_map_type_table[4]; /* [EBP-0xd8] */
      int op_table[4]; /* [EBP-0x134] */
      int colorop;
      int alphaop;
      float u;
      float v;
      float anim_out[32]; /* [EBP-0x330] 4 stages x 8 floats */
      char sub_group[0xa0]; /* [EBP-0x450] */
      int nstages;
      unsigned int fade_mode_value; /* [EBP-0x6c] */
      float fade_consts[12]; /* [EBP-0x180] */
      float t;
      float c[4]; /* [EBP-0x64] stage argb color */
      float da;
      float dr;
      float dg;
      float db;
      float *pf;
      char ok;
      int bcount;
      int limit;
      int eidx;
      int fvi;
      int k;
      float x;
      unsigned int blendrow;

      gen = (char *)FUN_001906b0(sh, 5);
      frame_index = *(unsigned short *)(grp + 0x10);
      layers = gen + 0x48;
      if (*(int *)layers > 0) {
        n = 0;
        do {
          csmemcpy(sub_group, grp, 0xa0);
          *(int *)(sub_group + 0x90) = -1;
          map = (char *)tag_block_get_element(layers, (short)n, 0x10);
          *(void **)(sub_group + 0xc) =
            tag_get(0x73686472, *(int *)(map + 0xc));
          rasterizer_transparent_geometry_group_draw(sub_group, dirty);
          n = n + 1;
        } while ((int)(short)n < *(int *)layers);
      }
      FUN_00178b40(0x18, vertex_type, permutation);
      SetRenderStateSmart(
        0x7f,
        (int)((-(unsigned int)((*(unsigned char *)(gen + 0x29) & 4) != 0) &
               0xfffff6ff) +
              0x901));
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(0x3c, *(unsigned char *)(gen + 0x29) & 1);
      SetRenderStateSmart(0x3d, 0x7f);
      FUN_001580b0(*(unsigned short *)(gen + 0x2c));
      if ((char)*(char *)(gen + 0x29) < 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(gen + 0x54) > 0) {
        /* numeric-counter driven first map index */
        map = (char *)tag_block_get_element(gen + 0x54, 0, 0x64);
        bitm = (char *)tag_get(0x6269746d, *(int *)(map + 0x28));
        bcount = *(short *)(bitm + 0x60);
        limit = (short)*(unsigned char *)(gen + 0x28);
        eidx = ((bcount != 8) - 1 & 3);
        x = (float)limit *
              *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) + eidx * 4) +
            *(float *)0x253398;
        /* PIN(FLOOR(...)) re-evaluates the floor expression per compare */
        if ((int)floor((double)x) < 0) {
          fvi = 0;
        } else if ((int)floor(
                     (double)((float)limit *
                                *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                           eidx * 4) +
                              *(float *)0x253398)) > limit) {
          fvi = limit;
        } else {
          fvi = (int)floor(
            (double)((float)limit *
                       *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                  eidx * 4) +
                     *(float *)0x253398));
        }
        for (k = *(short *)(grp + 0x10); k > 0; k--) {
          fvi = (int)(short)fvi / (int)(short)bcount;
        }
        frame_index = (int)(short)fvi % (int)(short)bcount;
      }
      m = 0;
      do {
        if ((int)(short)m < *(int *)(gen + 0x54)) {
          map = (char *)tag_block_get_element(gen + 0x54, (short)m, 0x64);
          gtype = *(short *)(gen + 0x2a);
          first_map_type_table[0] = 0;
          first_map_type_table[1] = 2;
          first_map_type_table[2] = 2;
          first_map_type_table[3] = 2;
          op_table[0] = 1;
          op_table[1] = 3;
          op_table[2] = 3;
          op_table[3] = 3;
          if ((short)m == 0) {
            first_map_type = first_map_type_table[gtype];
          } else {
            first_map_type = 0;
          }
          if ((*gen & 4) != 0 && gtype != 0) {
            display_assert(
              "!TEST_FLAG(shader_transparent_generic->shader.radiosity.flags, "
              "_shader_radiosity_FILTHY_transparent_lit_bit) || "
              "shader_transparent_generic->generic.type==_shader_transparent_"
              "generic_type_2d_map",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x406, 1);
            system_exit(-1);
          }
          if (gtype < 0 || gtype > 3) {
            display_assert(
              "type>=0 && type<NUMBER_OF_SHADER_TRANSPARENT_GENERIC_TYPES",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x407, 1);
            system_exit(-1);
          }
          rasterizer_set_texture((short)m, first_map_type, 0,
                                 *(int *)(map + 0x28), frame_index);
          if (first_map_type == 0 && (*map & 2) != 0) {
            colorop = 3;
          } else if ((short)m != 0) {
            colorop = 1;
          } else {
            colorop = op_table[gtype];
          }
          if (first_map_type == 0 && (*map & 4) != 0) {
            alphaop = 3;
          } else if ((short)m != 0) {
            alphaop = 1;
          } else {
            alphaop = op_table[gtype];
          }
          SetTextureStageStateSmart((short)m, 0xa, colorop);
          SetTextureStageStateSmart((short)m, 0xb, alphaop);
          SetTextureStageStateSmart((short)m, 0xc,
                                    ((short)m != 0) ? 1 : op_table[gtype]);
          D3DDevice_SetTextureStageState((short)m, 0xd, 2);
          SetTextureStageStateSmart((short)m, 0xe, 2 - (int)((*map & 1) != 0));
          SetTextureStageStateSmart((short)m, 0xf, 2 - (int)((*map & 1) != 0));
        }
        if ((int)(short)m < *(int *)(gen + 0x54) &&
            ((short)m > 0 || *(short *)(gen + 0x2a) == 0)) {
          map = (char *)tag_block_get_element(gen + 0x54, (short)m, 0x64);
          u = *(float *)(map + 4);
          v = *(float *)(map + 8);
          if ((short)m == 0) {
            if ((*(unsigned char *)(gen + 0x29) & 0x40) != 0) {
              u = -(u * *(float *)(grp + 0x70));
              v = -(v * *(float *)(grp + 0x70));
            }
            if ((*(unsigned char *)(gen + 0x29) & 8) == 0) {
              u = u * *(float *)(grp + 0x3c);
              v = v * *(float *)(grp + 0x40);
            }
          } else {
            u = u * *(float *)(grp + 0x3c);
            v = v * *(float *)(grp + 0x40);
          }
          FUN_00190e10(map + 0x2c, *(void **)(grp + 0x6c), u, v,
                       *(float *)(map + 0xc), *(float *)(map + 0x10),
                       *(float *)(map + 0x14), *(float *)0x5a5e18,
                       &anim_out[(short)m * 8], &anim_out[(short)m * 8 + 4]);
        } else if ((int)(short)m < *(int *)(gen + 0x54) &&
                   (*(unsigned char *)(gen + 0x29) & 8) != 0) {
          anim_out[(short)m * 8] = *(float *)0x5a5c64;
          anim_out[(short)m * 8 + 1] = *(float *)0x5a5c68;
          anim_out[(short)m * 8 + 2] = *(float *)0x5a5c6c;
          anim_out[(short)m * 8 + 4] = *(float *)0x5a5c70;
          anim_out[(short)m * 8 + 5] = *(float *)0x5a5c74;
          anim_out[(short)m * 8 + 6] = *(float *)0x5a5c78;
          anim_out[(short)m * 8 + 3] = 0.0f;
          anim_out[(short)m * 8 + 7] = 0.0f;
        } else {
          anim_out[(short)m * 8] = 1.0f;
          anim_out[(short)m * 8 + 1] = 0.0f;
          anim_out[(short)m * 8 + 2] = 0.0f;
          anim_out[(short)m * 8 + 4] = 0.0f;
          anim_out[(short)m * 8 + 5] = 1.0f;
          anim_out[(short)m * 8 + 6] = 0.0f;
          anim_out[(short)m * 8 + 3] = 0.0f;
          anim_out[(short)m * 8 + 7] = 0.0f;
        }
        m = m + 1;
      } while ((short)m < 4);
      D3DDevice_SetVertexShaderConstant(-0x51, anim_out, 8);
      if (success != 0) {
        ok = FUN_0017c2f0(*(void **)(grp + 0xc), (void *)0x5a5ac0);
        if (ok == 0) {
          success = 0;
        } else {
          success = 1;
        }
      } else {
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXANIM_OFFSET, "
                     "vsh_constants__texanim, VSH_CONSTANTS__TEXANIM_COUNT)");
        success = 0;
      }
      if (*(char *)0x3256d4 == 0) {
        goto generic_stage_colors;
      }
      nstages = *(int *)(gen + 0x60);
      if (nstages < 1) {
        nstages = 1;
      }
      if ((*grp & 0x10) != 0 && *(short *)(gen + 0x2c) == 0) {
        /* fog-plane driven fade into an extra combiner stage */
        t = -(plane3d_distance_to_point((float *)0x5a5dc8, (float *)0x5a5bc8) /
              *(float *)0x5a5dec);
        if (t < *(float *)0x2533c0) {
          t = 0.0f;
        } else if (t > *(float *)0x2533c8) {
          t = 1.0f;
        }
        ((uint32_t *)0x5a5ae8)[(short)nstages] = real_a_rgb_color_to_pixel32(
          t * *(float *)0x5a5de4, (float *)0x5a5dd8);
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = 0x310c1101;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        goto generic_stage_colors;
      }
      fade_consts[0] = 0.0f;
      fade_consts[1] = 0.0f;
      fade_consts[2] = 0.0f;
      fade_consts[3] = 0.0f;
      fade_consts[4] = 0.0f;
      fade_consts[5] = 0.0f;
      fade_consts[6] = 0.0f;
      fade_consts[7] = 0.0f;
      fade_consts[8] = 0.0f;
      fade_consts[9] = 0.0f;
      fade_consts[10] = 1.0f;
      fade_consts[11] = 0.0f;
      if (*(short *)(grp + 0x14) == 1) {
        t = *(float *)0x2533c8 - *(float *)(grp + 0x18);
        if (t < *(float *)0x2533c0) {
          fade_consts[10] = 0.0f;
        } else if (t > *(float *)0x2533c8) {
          fade_consts[10] = 1.0f;
        } else {
          fade_consts[10] = t;
        }
      }
      if (*(short *)(gen + 0x30) > 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(*(int *)(grp + 0x6c) + 4) != 0) {
        fade_consts[10] =
          fade_consts[10] * *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) - 4 +
                                       *(short *)(gen + 0x30) * 4);
      }
      D3DDevice_SetVertexShaderConstant(-0x54, fade_consts, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXSCALE_OFFSET, "
                     "vsh_constants__texscale, VSH_CONSTANTS__TEXSCALE_COUNT)");
      }
      if (*(short *)(gen + 0x2e) == 0) {
        fade_mode_value = 0x14;
      } else if (*(short *)(gen + 0x2e) == 1) {
        fade_mode_value = 0x15;
      } else {
        if (*(short *)(gen + 0x2e) != 2) {
          display_assert("### ERROR unsupported framebuffer fade mode",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x4a1, 1);
          system_exit(-1);
        }
        fade_mode_value = 5;
      }
      switch (*(short *)(gen + 0x2c)) {
      case 0:
        *(uint32_t *)(0x5a5ac0 + (short)nstages * 4) =
          (fade_mode_value | 0x1c00) << 0x10;
        *(uint32_t *)(0x5a5b28 + (short)nstages * 4) = 0xc00;
        break;
      case 1:
      case 5:
        blendrow =
          (fade_mode_value ^ 0x20) | fade_mode_value << 0x10 | 0xc002000;
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = blendrow;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      case 2:
        blendrow =
          (fade_mode_value ^ 0x20) | fade_mode_value << 0x10 | 0xc00a000;
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = blendrow;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      case 3:
      case 4:
      case 6:
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = (fade_mode_value | 0xc00)
                                                       << 0x10;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      case 7:
        *(uint32_t *)(0x5a5ac0 + (short)nstages * 4) =
          (fade_mode_value | 0x1c00) << 0x10;
        *(uint32_t *)(0x5a5b28 + (short)nstages * 4) = 0xc00;
        *(uint32_t *)(0x5a5b48 + (short)nstages * 4) = (fade_mode_value | 0xc00)
                                                       << 0x10;
        *(uint32_t *)(0x5a5b74 + (short)nstages * 4) = 0xc00;
        break;
      default:
        display_assert("### ERROR unsupported framebuffer blend function",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x4c0, 1);
        system_exit(-1);
      }
    generic_stage_colors:
      j = 0;
      if (*(int *)(gen + 0x60) > 0) {
        do {
          stage = (char *)tag_block_get_element(gen + 0x60, (short)j, 0x70);
          if (*(float *)(stage + 8) == *(float *)0x2533c0) {
            display_assert("stage->constant_color0_animation_period!=0.0f",
                           "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_"
                           "xbox_transparent_geometry.c",
                           0x4d3, 1);
            system_exit(-1);
          }
          if (*(int *)(grp + 0x6c) != 0 && (*stage & 4) != 0) {
            t = **(float **)(*(int *)(grp + 0x6c) + 4);
          } else {
            t = FUN_0010a5e0(*(short *)(stage + 6),
                             *(float *)0x5a5e18 / *(float *)(stage + 8));
          }
          da = *(float *)(stage + 0x1c) - *(float *)(stage + 0xc);
          dr = *(float *)(stage + 0x20) - *(float *)(stage + 0x10);
          dg = *(float *)(stage + 0x24) - *(float *)(stage + 0x14);
          db = *(float *)(stage + 0x28) - *(float *)(stage + 0x18);
          c[0] = t * da + *(float *)(stage + 0xc);
          c[1] = t * dr + *(float *)(stage + 0x10);
          c[2] = dg * t + *(float *)(stage + 0x14);
          c[3] = t * db + *(float *)(stage + 0x18);
#if !defined(_MSC_VER) || defined(__clang__)
          /* The original stores each channel to a 32-bit float (FSTP) and the
           * range asserts below reload the rounded value; clang keeps the
           * channels in x87 registers (FST + FUCOMI) and compares at 80-bit
           * extended precision.  When t == 1.0 the lerp lo + round32(hi-lo)*t
           * can land half a ULP above 1.0 (e.g. needler core stage 6 with the
           * weapon A-out pegged at 1.0), which passes the original's rounded
           * compare but trips the extended-precision one.  Force the same
           * store+reload rounding before comparing. */
          asm volatile("" : "+m"(c[0]), "+m"(c[1]), "+m"(c[2]), "+m"(c[3]));
#endif
          if (!(c[1] >= *(float *)0x2533c0 && c[1] <= *(float *)0x2533c8)) {
            display_assert(
              "constant_color0.red >=0.0f && constant_color0.red <=1.0f",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x4e8, 1);
            system_exit(-1);
          }
          if (!(c[2] >= *(float *)0x2533c0 && c[2] <= *(float *)0x2533c8)) {
            display_assert(
              "constant_color0.green>=0.0f && constant_color0.green<=1.0f",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x4e9, 1);
            system_exit(-1);
          }
          if (!(c[3] >= *(float *)0x2533c0 && c[3] <= *(float *)0x2533c8)) {
            display_assert(
              "constant_color0.blue >=0.0f && constant_color0.blue <=1.0f",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x4ea, 1);
            system_exit(-1);
          }
          if (*(short *)(stage + 4) > 0 && *(short *)(stage + 4) < 5 &&
              *(int **)(grp + 0x6c) != (int *)0 &&
              **(int **)(grp + 0x6c) != 0) {
            pf = (float *)(**(int **)(grp + 0x6c) - 0xc +
                           *(short *)(stage + 4) * 0xc);
            if (!(pf[0] >= *(float *)0x2533c0 && pf[0] <= *(float *)0x2533c8)) {
              display_assert(
                "external_color->red >=0.0f && external_color->red <=1.0f",
                "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                "transparent_geometry.c",
                0x4f5, 1);
              system_exit(-1);
            }
            if (!(pf[1] >= *(float *)0x2533c0 && pf[1] <= *(float *)0x2533c8)) {
              display_assert(
                "external_color->green>=0.0f && external_color->green<=1.0f",
                "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                "transparent_geometry.c",
                0x4f6, 1);
              system_exit(-1);
            }
            if (!(pf[2] >= *(float *)0x2533c0 && pf[2] <= *(float *)0x2533c8)) {
              display_assert(
                "external_color->blue >=0.0f && external_color->blue <=1.0f",
                "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                "transparent_geometry.c",
                0x4f7, 1);
              system_exit(-1);
            }
            c[1] = c[1] * pf[0];
            c[2] = c[2] * pf[1];
            c[3] = c[3] * pf[2];
          }
          ((uint32_t *)0x5a5ae8)[(short)j] = FUN_000d1c90(c);
          j = j + 1;
        } while ((int)(short)j < *(int *)(gen + 0x60));
      }
      goto set_shader_and_draw;
    }
    case 6: {
      /* shader_transparent_chicago */
      char *chi;
      char *layers2;
      char *map2;
      char *bitm2;
      int frame_index2; /* [EBP-0x14] */
      int m2; /* loop counter (param slot reuse in original) */
      short ctype;
      short first_map_type2; /* [EBP-0xc] */
      short first_map_type_table2[4]; /* [EBP-0xd0] */
      int op_table2[4]; /* [EBP-0x144] */
      int colorop2;
      int alphaop2;
      float u2;
      float v2;
      float anim_out2[32]; /* [EBP-0x3b0] */
      char sub_group2[0xa0]; /* [EBP-0x4f0] */
      short nstages2;
      unsigned int fade_mode_value2; /* [EBP-0x70] */
      float fade_consts2[12]; /* [EBP-0x1b0] */
      float t2;
      char ok2;
      int bcount2;
      int limit2;
      int eidx2;
      int fvi2;
      int k2;
      float x2;
      unsigned int blendrow2;

      chi = (char *)FUN_001906b0(sh, 6);
      frame_index2 = *(unsigned short *)(grp + 0x10);
      layers2 = chi + 0x48;
      /* NOTE: original re-reads the layer count each iteration and always
       * fetches element 0 -- faithful reproduction of the binary */
      while (*(int *)layers2 > 0) {
        csmemcpy(sub_group2, grp, 0xa0);
        *(int *)(sub_group2 + 0x90) = -1;
        map2 = (char *)tag_block_get_element(layers2, 0, 0x10);
        *(void **)(sub_group2 + 0xc) =
          tag_get(0x73686472, *(int *)(map2 + 0xc));
        rasterizer_transparent_geometry_group_draw(sub_group2, dirty);
      }
      FUN_00178b40(0x18, vertex_type, permutation);
      SetRenderStateSmart(
        0x7f,
        (int)((-(unsigned int)((*(unsigned char *)(chi + 0x29) & 4) != 0) &
               0xfffff6ff) +
              0x901));
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(0x3c, *(unsigned char *)(chi + 0x29) & 1);
      SetRenderStateSmart(0x3d, 0x7f);
      FUN_001580b0(*(unsigned short *)(chi + 0x2c));
      if ((char)*(char *)(chi + 0x29) < 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(chi + 0x54) > 0) {
        map2 = (char *)tag_block_get_element(chi + 0x54, 0, 0xdc);
        bitm2 = (char *)tag_get(0x6269746d, *(int *)(map2 + 0x78));
        bcount2 = *(short *)(bitm2 + 0x60);
        frame_index2 = (short)bcount2;
        if ((*(unsigned char *)(chi + 0x60) & 2) != 0) {
          frame_index2 =
            numeric_countdown_timer_get(*(unsigned short *)(grp + 0x10));
        } else {
          limit2 = (short)*(unsigned char *)(chi + 0x28);
          eidx2 = ((bcount2 != 8) - 1 & 3);
          x2 = (float)limit2 *
                 *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) + eidx2 * 4) +
               *(float *)0x253398;
          /* PIN(FLOOR(...)) re-evaluates the floor expression per compare */
          if ((int)floor((double)x2) < 0) {
            fvi2 = 0;
          } else if ((int)floor((
                       double)((float)limit2 *
                                 *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                            eidx2 * 4) +
                               *(float *)0x253398)) > limit2) {
            fvi2 = limit2;
          } else {
            fvi2 = (int)floor(
              (double)((float)limit2 *
                         *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) +
                                    eidx2 * 4) +
                       *(float *)0x253398));
          }
          for (k2 = *(short *)(grp + 0x10); k2 > 0; k2--) {
            fvi2 = (int)(short)fvi2 / (int)(short)frame_index2;
          }
          frame_index2 = (int)(short)fvi2 % (int)(short)frame_index2;
        }
      }
      m2 = 0;
      do {
        if ((int)(short)m2 < *(int *)(chi + 0x54)) {
          map2 = (char *)tag_block_get_element(chi + 0x54, (short)m2, 0xdc);
          ctype = *(short *)(chi + 0x2a);
          first_map_type_table2[0] = 0;
          first_map_type_table2[1] = 2;
          first_map_type_table2[2] = 2;
          first_map_type_table2[3] = 2;
          op_table2[0] = 1;
          op_table2[1] = 3;
          op_table2[2] = 3;
          op_table2[3] = 3;
          if ((short)m2 == 0) {
            first_map_type2 = first_map_type_table2[ctype];
          } else {
            first_map_type2 = 0;
          }
          if ((*chi & 4) != 0 && ctype != 0) {
            display_assert(
              "!TEST_FLAG(shader_transparent_chicago->shader.radiosity.flags, "
              "_shader_radiosity_FILTHY_transparent_lit_bit) || "
              "shader_transparent_chicago->chicago.type==_shader_transparent_"
              "chicago_type_2d_map",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x567, 1);
            system_exit(-1);
          }
          if (ctype < 0 || ctype > 3) {
            display_assert(
              "type>=0 && type<NUMBER_OF_SHADER_TRANSPARENT_CHICAGO_TYPES",
              "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_transparent_"
              "geometry.c",
              0x568, 1);
            system_exit(-1);
          }
          rasterizer_set_texture((short)m2, first_map_type2, 0,
                                 *(int *)(map2 + 0x78), frame_index2);
          if (first_map_type2 == 0 && (*map2 & 4) != 0) {
            colorop2 = 3;
          } else if ((short)m2 != 0) {
            colorop2 = 1;
          } else {
            colorop2 = op_table2[ctype];
          }
          if (first_map_type2 == 0 && (*map2 & 8) != 0) {
            alphaop2 = 3;
          } else if ((short)m2 != 0) {
            alphaop2 = 1;
          } else {
            alphaop2 = op_table2[ctype];
          }
          D3DDevice_SetTextureStageState((short)m2, 0xa, colorop2);
          D3DDevice_SetTextureStageState((short)m2, 0xb, alphaop2);
          D3DDevice_SetTextureStageState(
            (short)m2, 0xc, ((short)m2 != 0) ? 1 : op_table2[ctype]);
          D3DDevice_SetTextureStageState((short)m2, 0xd, 2);
          D3DDevice_SetTextureStageState((short)m2, 0xe,
                                         2 - (int)((*map2 & 1) != 0));
          D3DDevice_SetTextureStageState((short)m2, 0xf,
                                         2 - (int)((*map2 & 1) != 0));
        }
        if ((int)(short)m2 < *(int *)(chi + 0x54) &&
            ((short)m2 > 0 || *(short *)(chi + 0x2a) == 0)) {
          map2 = (char *)tag_block_get_element(chi + 0x54, (short)m2, 0xdc);
          u2 = *(float *)(map2 + 0x54);
          v2 = *(float *)(map2 + 0x58);
          if ((short)m2 == 0) {
            if ((*(unsigned char *)(chi + 0x29) & 0x40) != 0) {
              u2 = -(u2 * *(float *)(grp + 0x70));
              v2 = -(v2 * *(float *)(grp + 0x70));
            }
            if ((*(unsigned char *)(chi + 0x29) & 8) == 0) {
              u2 = u2 * *(float *)(grp + 0x3c);
              v2 = v2 * *(float *)(grp + 0x40);
            }
          } else {
            u2 = u2 * *(float *)(grp + 0x3c);
            v2 = v2 * *(float *)(grp + 0x40);
          }
          FUN_00190e10(map2 + 0xa4, *(void **)(grp + 0x6c), u2, v2,
                       *(float *)(map2 + 0x5c), *(float *)(map2 + 0x60),
                       *(float *)(map2 + 0x64), *(float *)0x5a5e18,
                       &anim_out2[(short)m2 * 8],
                       &anim_out2[(short)m2 * 8 + 4]);
        } else if ((int)(short)m2 < *(int *)(chi + 0x54) &&
                   (*(unsigned char *)(chi + 0x29) & 8) != 0) {
          anim_out2[(short)m2 * 8] = *(float *)0x5a5c64;
          anim_out2[(short)m2 * 8 + 1] = *(float *)0x5a5c68;
          anim_out2[(short)m2 * 8 + 2] = *(float *)0x5a5c6c;
          anim_out2[(short)m2 * 8 + 4] = *(float *)0x5a5c70;
          anim_out2[(short)m2 * 8 + 5] = *(float *)0x5a5c74;
          anim_out2[(short)m2 * 8 + 6] = *(float *)0x5a5c78;
          anim_out2[(short)m2 * 8 + 3] = 0.0f;
          anim_out2[(short)m2 * 8 + 7] = 0.0f;
        } else {
          anim_out2[(short)m2 * 8] = 1.0f;
          anim_out2[(short)m2 * 8 + 1] = 0.0f;
          anim_out2[(short)m2 * 8 + 2] = 0.0f;
          anim_out2[(short)m2 * 8 + 4] = 0.0f;
          anim_out2[(short)m2 * 8 + 5] = 1.0f;
          anim_out2[(short)m2 * 8 + 6] = 0.0f;
          anim_out2[(short)m2 * 8 + 3] = 0.0f;
          anim_out2[(short)m2 * 8 + 7] = 0.0f;
        }
        m2 = m2 + 1;
      } while ((short)m2 < 4);
      D3DDevice_SetVertexShaderConstant(-0x51, anim_out2, 8);
      if (success == 0) {
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXANIM_OFFSET, "
                     "vsh_constants__texanim, VSH_CONSTANTS__TEXANIM_COUNT)");
        success = 0;
      } else {
        ok2 = FUN_0017bca0(*(void **)(grp + 0xc), (void *)0x5a5ac0);
        success = 1;
        if (ok2 == 0) {
          success = 0;
        }
      }
      if (*(char *)0x3256d4 == 0) {
        goto set_shader_and_draw;
      }
      nstages2 = *(short *)(chi + 0x54);
      if ((*grp & 0x10) != 0 && *(short *)(chi + 0x2c) == 0) {
        t2 = -(plane3d_distance_to_point((float *)0x5a5dc8, (float *)0x5a5bc8) /
               *(float *)0x5a5dec);
        if (t2 < *(float *)0x2533c0) {
          t2 = 0.0f;
        } else if (t2 > *(float *)0x2533c8) {
          t2 = 1.0f;
        }
        ((uint32_t *)0x5a5ae8)[nstages2] = real_a_rgb_color_to_pixel32(
          *(float *)0x5a5de4 * t2, (float *)0x5a5dd8);
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = 0x310c1101;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        goto set_shader_and_draw;
      }
      fade_consts2[0] = 0.0f;
      fade_consts2[1] = 0.0f;
      fade_consts2[2] = 0.0f;
      fade_consts2[3] = 0.0f;
      fade_consts2[4] = 0.0f;
      fade_consts2[5] = 0.0f;
      fade_consts2[6] = 0.0f;
      fade_consts2[7] = 0.0f;
      fade_consts2[8] = 0.0f;
      fade_consts2[9] = 0.0f;
      fade_consts2[10] = 1.0f;
      fade_consts2[11] = 0.0f;
      if (*(short *)(grp + 0x14) == 1 &&
          (*(unsigned char *)(chi + 0x60) & 1) == 0) {
        t2 = *(float *)0x2533c8 - *(float *)(grp + 0x18);
        if (t2 < *(float *)0x2533c0) {
          fade_consts2[10] = 0.0f;
        } else if (t2 > *(float *)0x2533c8) {
          fade_consts2[10] = 1.0f;
        } else {
          fade_consts2[10] = t2;
        }
      }
      if (*(short *)(chi + 0x30) > 0 && *(int *)(grp + 0x6c) != 0 &&
          *(int *)(*(int *)(grp + 0x6c) + 4) != 0) {
        fade_consts2[10] =
          fade_consts2[10] * *(float *)(*(int *)(*(int *)(grp + 0x6c) + 4) - 4 +
                                        *(short *)(chi + 0x30) * 4);
      }
      D3DDevice_SetVertexShaderConstant(-0x54, fade_consts2, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXSCALE_OFFSET, "
                     "vsh_constants__texscale, VSH_CONSTANTS__TEXSCALE_COUNT)");
      }
      if (*(short *)(chi + 0x2e) == 0) {
        fade_mode_value2 = 0x14;
      } else if (*(short *)(chi + 0x2e) == 1) {
        fade_mode_value2 = 0x15;
      } else {
        if (*(short *)(chi + 0x2e) != 2) {
          display_assert("### ERROR unsupported framebuffer fade mode",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x603, 1);
          system_exit(-1);
        }
        fade_mode_value2 = 5;
      }
      switch (*(short *)(chi + 0x2c)) {
      case 0:
        *(uint32_t *)(0x5a5ac0 + nstages2 * 4) = (fade_mode_value2 | 0x1c00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b28 + nstages2 * 4) = 0xc00;
        break;
      case 1:
      case 5:
        blendrow2 =
          (fade_mode_value2 ^ 0x20) | fade_mode_value2 << 0x10 | 0xc002000;
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = blendrow2;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      case 2:
        blendrow2 =
          (fade_mode_value2 ^ 0x20) | fade_mode_value2 << 0x10 | 0xc00a000;
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = blendrow2;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      case 3:
      case 4:
      case 6:
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = (fade_mode_value2 | 0xc00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      case 7:
        *(uint32_t *)(0x5a5ac0 + nstages2 * 4) = (fade_mode_value2 | 0x1c00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b28 + nstages2 * 4) = 0xc00;
        *(uint32_t *)(0x5a5b48 + nstages2 * 4) = (fade_mode_value2 | 0xc00)
                                                 << 0x10;
        *(uint32_t *)(0x5a5b74 + nstages2 * 4) = 0xc00;
        break;
      default:
        display_assert("### ERROR unsupported framebuffer blend function",
                       "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                       "transparent_geometry.c",
                       0x622, 1);
        system_exit(-1);
      }
      goto set_shader_and_draw;
    }

    case 7:
      FUN_00179de0(grp);
      break;
    case 8: {
      /* shader_transparent_glass */
      char *gls;
      short reflection_type;
      float glass_consts[12]; /* diffuse [EBP-0x280] */
      float refl_consts[12]; /* reflection [EBP-0x2b0] */
      float bump_consts[12]; /* bump/specular [EBP-0x220] */
      float bump_color[3]; /* [EBP-0x7c] */
      float bc;

      gls = (char *)FUN_001906b0(sh, 8);
      reflection_type = *(short *)(gls + 0x8a);
      if (reflection_type == 2) {
        if (*(char *)0x5a5bc4 == 0 || *(short *)0x5a5bc0 != 0) {
          break;
        }
      } else if (reflection_type == 0 &&
                 ((*(unsigned char *)(gls + 0x28) & 8) != 0 ||
                  *(int *)(gls + 0xcc) == -1)) {
        reflection_type = 1;
      }
      if (*(int *)(gls + 0x70) != -1 ||
          *(float *)(gls + 0x54) != *(float *)0x2533c0 ||
          *(float *)(gls + 0x58) != *(float *)0x2533c0 ||
          *(float *)(gls + 0x5c) != *(float *)0x2533c0) {
        /* diffuse pass */
        rasterizer_set_texture(0, 0, 1, *(int *)(gls + 0x70),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(0, 0xa, 1);
        SetTextureStageStateSmart(0, 0xb, 1);
        SetTextureStageStateSmart(0, 0xd, 2);
        SetTextureStageStateSmart(0, 0xe, 2);
        SetTextureStageStateSmart(0, 0xf, 2);
        SetRenderStateSmart(
          0x7f,
          (int)((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 4) != 0) &
                 0xfffff6ff) +
                0x901));
        SetRenderStateSmart(0x43, 0x10101);
        SetRenderStateSmart(0x3b, 1);
        SetRenderStateSmart(0x3e, 0);
        SetRenderStateSmart(0x3f, 0x300);
        SetRenderStateSmart(0x4a, 0x8006);
        SetRenderStateSmart(0x3c, 1);
        SetRenderStateSmart(0x3d, 0);
        FUN_00178b40(0x2e, vertex_type, permutation);
        glass_consts[0] = *(float *)(grp + 0x3c) * *(float *)(gls + 0x60);
        glass_consts[1] = *(float *)(grp + 0x40) * *(float *)(gls + 0x60);
        glass_consts[2] = 1.0f;
        glass_consts[3] = 1.0f;
        glass_consts[4] = 0.0f;
        glass_consts[5] = 0.0f;
        glass_consts[6] = 0.0f;
        glass_consts[7] = 0.0f;
        glass_consts[8] = 0.0f;
        glass_consts[9] = 0.0f;
        glass_consts[10] = 0.0f;
        glass_consts[11] = 0.0f;
        D3DDevice_SetVertexShaderConstant(-0x54, glass_consts, 3);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
               "VSH_CONSTANTS__TEXSCALE_OFFSET, vsh_constants__texscale, "
               "VSH_CONSTANTS__TEXSCALE_COUNT)");
        }
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        *(uint32_t *)0x5a5b98 = 1;
        *(uint32_t *)0x5a5b94 = 1;
        *(uint32_t *)0x5a5ae8 = FUN_000d1dd0((float *)(gls + 0x54));
        *(uint32_t *)0x5a5b48 = 0x8010000;
        *(uint32_t *)0x5a5b74 = 0xc0;
        if (*(short *)(grp + 0x14) == 1) {
          *(uint32_t *)0x5a5b08 = FUN_00159070(*(float *)(grp + 0x18));
          *(uint32_t *)0x5a5ac0 = 0x14320000;
          *(uint32_t *)0x5a5b28 = 0x40;
        }
        *(uint32_t *)0x5a5ae0 = 0x140c2000;
        *(uint32_t *)0x5a5ae4 = 0x1400;
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        FUN_00174510(grp, 0);
      }
      if ((*(float *)(gls + 0x8c) > *(float *)0x2533c0 ||
           *(float *)(gls + 0x9c) > *(float *)0x2533c0) &&
          (*(int *)(gls + 0xb8) != -1 || reflection_type == 2)) {
        /* reflection pass */
        if (reflection_type < 0 || reflection_type > 2) {
          display_assert("reflection_type>=0 && "
                         "reflection_type<NUMBER_OF_SHADER_TRANSPARENT_GLASS_"
                         "REFLECTION_TYPES",
                         "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                         "transparent_geometry.c",
                         0x69e, 1);
          system_exit(-1);
        }
        rasterizer_set_texture(0, 0, 3, *(int *)(gls + 0xcc),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(0, 0xa, 1);
        SetTextureStageStateSmart(0, 0xb, 1);
        SetTextureStageStateSmart(0, 0xd, 2);
        SetTextureStageStateSmart(0, 0xe, 2);
        SetTextureStageStateSmart(0, 0xf, 2);
        rasterizer_set_texture_direct(1, *(int *)(*(int *)0x476204 + 0x1c), 0);
        SetTextureStageStateSmart(1, 0xa, 3);
        SetTextureStageStateSmart(1, 0xb, 3);
        SetTextureStageStateSmart(1, 0xc, 3);
        SetTextureStageStateSmart(1, 0xd, 2);
        SetTextureStageStateSmart(1, 0xe, 1);
        SetTextureStageStateSmart(1, 0xf, 1);
        rasterizer_set_texture_direct(2, *(int *)(*(int *)0x476204 + 0x1c), 0);
        SetTextureStageStateSmart(2, 0xa, 3);
        SetTextureStageStateSmart(2, 0xb, 3);
        SetTextureStageStateSmart(2, 0xc, 3);
        SetTextureStageStateSmart(2, 0xd, 2);
        SetTextureStageStateSmart(2, 0xe, 1);
        SetTextureStageStateSmart(2, 0xf, 1);
        if (reflection_type == 2) {
          FUN_001584f0(3, 1, 0);
          SetTextureStageStateSmart(3, 0xa, 3);
          SetTextureStageStateSmart(3, 0xb, 3);
          SetTextureStageStateSmart(3, 0xd, 2);
          SetTextureStageStateSmart(3, 0xe, 2);
          SetTextureStageStateSmart(3, 0xf, 1);
        } else {
          rasterizer_set_texture(3, 2, 0, *(int *)(gls + 0xb8),
                                 *(unsigned short *)(grp + 0x10));
          SetTextureStageStateSmart(3, 0xa, 3);
          SetTextureStageStateSmart(3, 0xb, 3);
          SetTextureStageStateSmart(3, 0xc, 3);
          SetTextureStageStateSmart(3, 0xd, 2);
          SetTextureStageStateSmart(3, 0xe, 2);
          SetTextureStageStateSmart(3, 0xf, 2);
        }
        SetRenderStateSmart(
          0x7f,
          (int)((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 4) != 0) &
                 0xfffff6ff) +
                0x901));
        SetRenderStateSmart(0x43, 0x10101);
        SetRenderStateSmart(0x3b, 1);
        SetRenderStateSmart(0x3e, 0x302);
        SetRenderStateSmart(0x3f, 1);
        SetRenderStateSmart(0x4a, 0x8006);
        SetRenderStateSmart(0x3c, 0);
        FUN_00178b40(0x2b, vertex_type, reflection_type);
        refl_consts[0] = *(float *)(grp + 0x3c) * *(float *)(gls + 0xbc);
        refl_consts[1] = *(float *)(grp + 0x40) * *(float *)(gls + 0xbc);
        refl_consts[2] = 320.0f;
        refl_consts[3] = 240.0f;
        refl_consts[4] = 0.0f;
        refl_consts[5] = 0.0f;
        refl_consts[6] = 0.0f;
        refl_consts[7] = 0.0f;
        refl_consts[8] = 0.0f;
        refl_consts[9] = 0.0f;
        refl_consts[10] = 0.0f;
        refl_consts[11] = 0.0f;
        D3DDevice_SetVertexShaderConstant(-0x54, refl_consts, 3);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
               "VSH_CONSTANTS__TEXSCALE_OFFSET, vsh_constants__texscale, "
               "VSH_CONSTANTS__TEXSCALE_COUNT)");
        }
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        if (reflection_type == 0) {
          *(uint32_t *)0x5a5b98 = 0x62e21;
          *(uint32_t *)0x5a5ba0 = 0;
          *(uint32_t *)0x5a5b9c = 0x111;
        } else if (reflection_type == 1) {
          *(uint32_t *)0x5a5b98 = 0x18c61;
        } else {
          if (reflection_type != 2) {
            display_assert("### ERROR unsupported reflection type",
                           "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_"
                           "xbox_transparent_geometry.c",
                           0x707, 1);
            system_exit(-1);
          }
          *(uint32_t *)0x5a5b98 = 0x8c61;
        }
        *(uint32_t *)0x5a5b94 = 0x11005;
        if (reflection_type != 0 && *(int *)(gls + 0xcc) != -1) {
          *(uint32_t *)0x5a5b48 = 0x49480b0b;
        } else {
          bc = *(float *)0x253398 - *(float *)0x5a5bd4 * *(float *)0x253398;
          if (bc < *(float *)0x2533c0) {
            bump_color[0] = 0.0f;
          } else if (bc > *(float *)0x2533c8) {
            bump_color[0] = 1.0f;
          } else {
            bump_color[0] = bc;
          }
          bc = *(float *)0x253398 - *(float *)0x5a5bd8 * *(float *)0x253398;
          if (bc < *(float *)0x2533c0) {
            bump_color[1] = 0.0f;
          } else if (bc > *(float *)0x2533c8) {
            bump_color[1] = 1.0f;
          } else {
            bump_color[1] = bc;
          }
          bc = *(float *)0x253398 - *(float *)0x5a5bdc * *(float *)0x253398;
          if (bc < *(float *)0x2533c0) {
            bump_color[2] = 0.0f;
          } else if (bc > *(float *)0x2533c8) {
            bump_color[2] = 1.0f;
          } else {
            bump_color[2] = bc;
          }
          *(uint32_t *)0x5a5ae8 = FUN_000d1dd0(bump_color);
          *(uint32_t *)0x5a5b48 = 0x4a410b0b;
        }
        *(uint32_t *)0x5a5b74 = 0x20cd;
        *(uint32_t *)0x5a5b4c = 0xc0c0d0d;
        *(uint32_t *)0x5a5b78 = 0xcd;
        if (*(short *)(grp + 0x14) == 1) {
          *(uint32_t *)0x5a5b0c = FUN_00159070(*(float *)(grp + 0x18));
          *(uint32_t *)0x5a5ac4 = 0x14320000;
          *(uint32_t *)0x5a5b2c = 0x40;
        }
        *(uint32_t *)0x5a5b50 = 0xc0c0d0d;
        *(uint32_t *)0x5a5b7c = 0xd;
        *(uint32_t *)0x5a5af4 = FUN_000d1c90((float *)(gls + 0x8c));
        *(uint32_t *)0x5a5b14 = FUN_000d1c90((float *)(gls + 0x9c));
        *(uint32_t *)0x5a5b34 = 0xc00;
        *(uint32_t *)0x5a5b80 = 0xc00;
        *(uint32_t *)0x5a5b84 = 0xc00;
        *(uint32_t *)0x5a5acc = 0x2c120c11;
        *(uint32_t *)0x5a5b54 = 0x2c020c01;
        *(uint32_t *)0x5a5b58 = 0x2c0d0c0b;
        *(uint32_t *)0x5a5ae0 = 0xc0f0000;
        *(uint32_t *)0x5a5ae4 =
          ((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 8) != 0) &
            0xfffffff4) +
           0x14) *
            0x10000 |
          0x1c002000;
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        FUN_00174510(grp, 0);
      }
      if (*(int *)(gls + 0x164) != -1 || *(int *)(gls + 0x178) != -1) {
        /* bump/specular pass */
        rasterizer_set_texture(0, 0, 1, *(int *)(gls + 0x164),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(0, 0xa, 1);
        SetTextureStageStateSmart(0, 0xb, 1);
        SetTextureStageStateSmart(0, 0xd, 2);
        SetTextureStageStateSmart(0, 0xe, 2);
        SetTextureStageStateSmart(0, 0xf, 2);
        rasterizer_set_texture(1, 0, 2, *(int *)(gls + 0x178),
                               *(unsigned short *)(grp + 0x10));
        SetTextureStageStateSmart(1, 0xa, 1);
        SetTextureStageStateSmart(1, 0xb, 1);
        SetTextureStageStateSmart(1, 0xd, 2);
        SetTextureStageStateSmart(1, 0xe, 2);
        SetTextureStageStateSmart(1, 0xf, 2);
        SetRenderStateSmart(
          0x7f,
          (int)((-(unsigned int)((*(unsigned char *)(gls + 0x28) & 4) != 0) &
                 0xfffff6ff) +
                0x901));
        SetRenderStateSmart(0x43, 0x10101);
        SetRenderStateSmart(0x3b, 1);
        SetRenderStateSmart(0x3e, 0x302);
        SetRenderStateSmart(0x3f, 0x303);
        SetRenderStateSmart(0x4a, 0x8006);
        SetRenderStateSmart(0x3c, 1);
        SetRenderStateSmart(0x3d, 0);
        if (*(int *)(grp + 0x5c) == 0) {
          rasterizer_set_texture(2, 0, 0, -1, 0);
          SetTextureStageStateSmart(2, 0xa, 3);
          SetTextureStageStateSmart(2, 0xb, 3);
          SetTextureStageStateSmart(2, 0xd, 2);
          SetTextureStageStateSmart(2, 0xe, 2);
          SetTextureStageStateSmart(2, 0xf, 2);
        } else {
          rasterizer_set_texture_bitmap_data(2, *(void **)(grp + 0x5c));
          SetTextureStageStateSmart(2, 0xa, 3);
          SetTextureStageStateSmart(2, 0xb, 3);
          SetTextureStageStateSmart(2, 0xd, 2);
          SetTextureStageStateSmart(2, 0xe, 2);
          SetTextureStageStateSmart(2, 0xf, 2);
        }
        FUN_00178b40(0x19, vertex_type, permutation);
        bump_consts[0] = *(float *)(gls + 0x154) * *(float *)(grp + 0x3c);
        bump_consts[1] = *(float *)(gls + 0x154) * *(float *)(grp + 0x40);
        bump_consts[2] = *(float *)(gls + 0x168) * *(float *)(grp + 0x3c);
        bump_consts[3] = *(float *)(gls + 0x168) * *(float *)(grp + 0x40);
        bump_consts[4] = 0.0f;
        bump_consts[5] = 0.0f;
        bump_consts[6] = 0.0f;
        bump_consts[7] = 0.0f;
        bump_consts[8] = 0.0f;
        bump_consts[9] = 0.0f;
        bump_consts[10] = 0.0f;
        bump_consts[11] = 0.0f;
        D3DDevice_SetVertexShaderConstant(-0x54, bump_consts, 3);
        if (success != 0) {
          success = 1;
        } else {
          success = 0;
          FUN_00167ff0(
            0, "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_device, "
               "VSH_CONSTANTS__TEXSCALE_OFFSET, vsh_constants__texscale, "
               "VSH_CONSTANTS__TEXSCALE_COUNT)");
        }
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        *(uint32_t *)0x5a5b98 = 0x421;
        *(uint32_t *)0x5a5b94 = 3;
        *(uint32_t *)0x5a5ac0 = 0x18190000;
        *(uint32_t *)0x5a5b28 = 0xc0;
        *(uint32_t *)0x5a5b48 = 0x8090000;
        *(uint32_t *)0x5a5b74 = 0x100c0;
        *(uint32_t *)0x5a5ac4 = 0x1c140000;
        *(uint32_t *)0x5a5b2c = 0xc0;
        *(uint32_t *)0x5a5b4c = 0xa200420;
        *(uint32_t *)0x5a5b78 = 0xd00;
        *(uint32_t *)0x5a5b50 = 0xc0d0000;
        *(uint32_t *)0x5a5b7c = 0xc0;
        *(uint32_t *)0x5a5ae0 = 0xc;
        *(uint32_t *)0x5a5ae4 = 0x1c00;
        rasterizer_set_pixel_shader((void *)0x5a5ac0);
        FUN_00174510(grp, *(int *)(grp + 0x5c) != 0);
      }
      break;
    }

    case 9: {
      /* shader_transparent_meter */
      char *met;
      float brightness; /* [EBP-0x3c] */
      float power; /* [EBP-0x44] */
      float gradient; /* [EBP-0x4c] */
      float met_alpha; /* [EBP-0x48] */
      float flash; /* rides FPU stack in original */
      float tint[3]; /* [EBP-0x150] */
      float flash_color[3]; /* [EBP-0x28] */
      float inv_flash;
      float x9;
      float t9;
      short src;
      int *ext9;
      uint32_t px1;
      uint32_t px2;
      uint32_t px3;
      uint32_t px4;
      uint32_t px_final;
      float px_final_alpha;
      float *px_final_color;
      float px3_alpha;
      float meter_consts[12]; /* [EBP-0x250] */

      met = (char *)FUN_001906b0(sh, 9);
      brightness = 1.0f;
      power = 1.0f;
      gradient = 1.0f;
      met_alpha = 1.0f;
      flash = 1.0f;
      if (*(int *)(grp + 0x6c) != 0 &&
          *(int *)(*(int *)(grp + 0x6c) + 4) != 0) {
        ext9 = (int *)*(int *)(*(int *)(grp + 0x6c) + 4);
        src = *(short *)(met + 0xd8);
        if (src > 0 && src < 5) {
          brightness = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xda);
        if (src > 0 && src < 5) {
          power = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xdc);
        if (src > 0 && src < 5) {
          gradient = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xde);
        if (src > 0 && src < 5) {
          flash = *((float *)ext9 + (src - 1));
        }
        src = *(short *)(met + 0xe0);
        if (src > 0 && src < 5) {
          met_alpha = *((float *)ext9 + (src - 1));
        }
      }
      if (*(char *)0x3256c3 != 0) {
        /* rasterizer_debug_meters override */
        t9 = FUN_0010a5e0(2, *(float *)0x5a5e18 / *(float *)0x325724);
        if (*(float *)0x325728 >= *(float *)0x2533c0) {
          brightness = *(float *)0x325728;
        } else {
          brightness = t9;
        }
        if (*(float *)0x32572c >= *(float *)0x2533c0) {
          power = *(float *)0x32572c;
        } else {
          power = t9;
        }
        if (*(float *)0x325730 >= *(float *)0x2533c0) {
          gradient = *(float *)0x325730;
        } else {
          gradient = t9;
        }
        if (*(float *)0x325734 >= *(float *)0x2533c0) {
          flash = *(float *)0x325734;
        } else {
          flash = t9;
        }
        if (*(float *)0x325738 >= *(float *)0x2533c0) {
          met_alpha = *(float *)0x325738;
        } else {
          met_alpha = t9;
        }
      }
      tint[0] = power * *(float *)(met + 0xa0);
      tint[1] = power * *(float *)(met + 0xa4);
      tint[2] = power * *(float *)(met + 0xa8);
      x9 = flash * *(float *)0x253f78;
      if (x9 <= *(float *)0x2533c8) {
        x9 = 1.0f;
      }
      inv_flash = *(float *)0x2533c8 / x9;
      if ((*(unsigned char *)(met + 0x28) & 8) != 0) {
        flash_color[0] = brightness * *(float *)(met + 0xac);
        flash_color[1] = brightness * *(float *)(met + 0xb0);
        flash_color[2] = brightness * *(float *)(met + 0xb4);
        px3_alpha = *(float *)(met + 0xbc);
        px1 = real_a_rgb_color_to_pixel32(gradient, (float *)(met + 0x7c));
        px2 = real_a_rgb_color_to_pixel32(inv_flash, (float *)(met + 0x88));
        px3 = real_a_rgb_color_to_pixel32(px3_alpha, (float *)(met + 0x94));
        px4 = real_a_rgb_color_to_pixel32(met_alpha, tint);
        px_final_alpha = *(float *)(met + 0xb8);
        px_final_color = flash_color;
      } else {
        px1 = real_a_rgb_color_to_pixel32(gradient, (float *)(met + 0x7c));
        px2 = real_a_rgb_color_to_pixel32(inv_flash, (float *)(met + 0x88));
        px3 = real_a_rgb_color_to_pixel32(*(float *)0x2533c0,
                                          (float *)(met + 0x94));
        px4 = real_a_rgb_color_to_pixel32(met_alpha, tint);
        px_final_alpha = brightness;
        px_final_color = (float *)(met + 0xac);
      }
      px_final = real_a_rgb_color_to_pixel32(px_final_alpha, px_final_color);
      rasterizer_set_texture(0, 0, 1, *(int *)(met + 0x58),
                             *(unsigned short *)(grp + 0x10));
      SetTextureStageStateSmart(0, 0xa, 1);
      SetTextureStageStateSmart(0, 0xb, 1);
      SetTextureStageStateSmart(
        0, 0xd, 2 - (int)((*(unsigned char *)(met + 0x28) & 0x10) != 0));
      SetTextureStageStateSmart(
        0, 0xe, 2 - (int)((*(unsigned char *)(met + 0x28) & 0x10) != 0));
      SetTextureStageStateSmart(0, 0xf, 2);
      SetRenderStateSmart(
        0x7f,
        (int)((-(unsigned int)((*(unsigned char *)(met + 0x28) & 2) != 0) &
               0xfffff6ff) +
              0x901));
      SetRenderStateSmart(0x43, 0x10101);
      SetRenderStateSmart(0x3b, 1);
      SetRenderStateSmart(
        0x3e, (int)((~((unsigned int)*(unsigned char *)(met + 0x28) >> 2) & 2) |
                    0x8001));
      SetRenderStateSmart(
        0x3f,
        (int)((-(unsigned int)((*(unsigned char *)(met + 0x28) & 8) != 0) &
               0xffff8301) +
              0x8001));
      SetRenderStateSmart(0x4b, px_final);
      SetRenderStateSmart(0x4a, 0x8006);
      SetRenderStateSmart(0x3c, 0);
      FUN_00178b40(0x16, vertex_type, permutation);
      meter_consts[0] = 1.0f;
      meter_consts[1] = 1.0f;
      meter_consts[2] = 1.0f;
      meter_consts[3] = 1.0f;
      meter_consts[4] = *(float *)(grp + 0x3c);
      meter_consts[5] = 0.0f;
      meter_consts[6] = 0.0f;
      meter_consts[7] = 0.0f;
      meter_consts[8] = 0.0f;
      meter_consts[9] = *(float *)(grp + 0x40);
      meter_consts[10] = 0.0f;
      meter_consts[11] = 0.0f;
      D3DDevice_SetVertexShaderConstant(-0x54, meter_consts, 3);
      if (success != 0) {
        success = 1;
      } else {
        success = 0;
        FUN_00167ff0(0,
                     "IDirect3DDevice8_SetVertexShaderConstant(global_d3d_"
                     "device, VSH_CONSTANTS__TEXSCALE_OFFSET, "
                     "vsh_constants__texscale, VSH_CONSTANTS__TEXSCALE_COUNT)");
      }
      csmemset((void *)0x5a5ac0, 0, 0xf0);
      SetTextureStageStateSmart(0, 0x15, 4);
      *(uint32_t *)0x5a5b28 = 0x20c00;
      *(uint32_t *)0x5a5b74 = 0x20c00;
      *(uint32_t *)0x5a5ae8 = px4;
      *(uint32_t *)0x5a5b10 = px4;
      *(uint32_t *)0x5a5b08 = px2;
      *(uint32_t *)0x5a5b0c = px2;
      *(uint32_t *)0x5a5aec = px1;
      *(uint32_t *)0x5a5af0 = px1;
      *(uint32_t *)0x5a5b98 = 1;
      *(uint32_t *)0x5a5b94 = 0x11104;
      *(uint32_t *)0x5a5ac0 = 0x12081208;
      *(uint32_t *)0x5a5b48 = 0x1120e820;
      *(uint32_t *)0x5a5ac4 = 0x6c200000;
      *(uint32_t *)0x5a5b2c = 0xc0;
      *(uint32_t *)0x5a5b4c = 0x3c011c02;
      *(uint32_t *)0x5a5b78 = 0xc00;
      *(uint32_t *)0x5a5ac8 = 0x820b120;
      *(uint32_t *)0x5a5b30 = 0xc00;
      *(uint32_t *)0x5a5af4 = px3;
      *(uint32_t *)0x5a5b34 = 0x4c00;
      *(uint32_t *)0x5a5b80 = 0x4c00;
      *(uint32_t *)0x5a5b7c = 0xc00;
      *(uint32_t *)0x5a5b50 =
        ((-(unsigned int)((*(unsigned char *)(met + 0x28) & 4) != 0) & 0xe0) +
         2) |
        0xc201c00;
      *(uint32_t *)0x5a5b14 = px_final;
      *(uint32_t *)0x5a5acc = 0x12201120;
      *(uint32_t *)0x5a5b54 = 0xc200120;
      *(uint32_t *)0x5a5ae0 = 0xc180000;
      *(uint32_t *)0x5a5ae4 = 0x1c00;
      if (*(char *)0x3256c3 != 0 && *(short *)0x3256ea != 0) {
        csmemset((void *)0x5a5ac0, 0, 0xf0);
        SetTextureStageStateSmart(0, 0x15, 0);
        SetRenderStateSmart(0x3b, 0);
        *(uint32_t *)0x5a5b98 = 1;
        *(uint32_t *)0x5a5b94 = 1;
        *(uint32_t *)0x5a5ae0 = ((*(short *)0x3256ea < 2) - 1 & 0x10) + 8;
      }
      rasterizer_set_pixel_shader((void *)0x5a5ac0);
      FUN_00174510(grp, 0);
      SetTextureStageStateSmart(0, 0x15, 0);
      break;
    }

    case 10:
      FUN_0016eef0(grp);
      break;

    default:
      error(2, "### ERROR unsupported shader type");
      success = 0;
      break;
    }

    goto next_pass;

  set_shader_and_draw:
    /* shared tail for shader types 1/5/6 (0x17744d) */
    rasterizer_set_pixel_shader((void *)0x5a5ac0);
    FUN_00174510(grp, 0);

  next_pass:
    pass = pass + 1;
  } while ((short)pass < 2);

  if ((*grp & 8) != 0 && *(short *)0x5a5bc0 == 0) {
    rasterizer_set_frustum_z(*(float *)0x2533c0, *(float *)0x2533c0);
  }
  if ((char)*grp < 0 && *(short *)(grp + 0x14) == 1) {
    rasterizer_set_frustum_z(*(float *)0x2533c0, *(float *)0x2533c0);
  }

tail:
  if ((char)dirty == 0) {
    *(int *)0x47e4b8 = *(int *)(grp + 8);
  }
  if (*(short *)(grp + 0x96) != -1) {
    rasterizer_transparent_geometry_group_draw(
      rasterizer_transparent_geometry_group_get(*(short *)(grp + 0x96)), dirty);
  }
  if (draw_secondary != 0) {
    sec = (char *)rasterizer_secondary_geometry_groups_get(&sec_count);
    if ((char)dirty != 0) {
      display_assert("!dirty",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "transparent_geometry.c",
                     0x8e0, 1);
      system_exit(-1);
    }
    for (si = 0; si < sec_count; si++) {
      rec = sec + (int)si * 0xa0;
      if (*(int *)(rec + 0x98) == *(int *)(grp + 8) &&
          *(short *)(rec + 0x14) == 1) {
        rasterizer_transparent_geometry_group_draw(rec, 1);
        if (*(short *)0x3256ea != 0) {
          *(char *)0x47e4c0 = 1;
        }
      }
    }
  }
  if (success == 0) {
    error(2, "### ERROR rasterizer_transparent_geometry_group_draw failed");
  }
}

void rasterizer_frame_begin(float *elapsed)
{
  char val;

  val = *(char *)0x3256c8;
  if (val < 2) {
    *(char *)0x3256d4 = val;
    *(char *)0x3256d3 = val;
    *(char *)0x3256d2 = val;
    *(char *)0x3256d1 = val;
    *(char *)0x3256d0 = val;
    *(char *)0x3256cf = val;
    *(char *)0x3256ce = val;
    *(char *)0x3256cd = val;
    *(char *)0x3256cc = val;
    *(char *)0x3256ca = val;
    *(char *)0x3256cb = val;
    *(char *)0x3256c9 = val;
    *(char *)0x3256d5 = val;
    *(char *)0x3256c8 = 2;
  }
  if (*(float *)0x325694 == *(float *)0x2533c0)
    *(int *)0x325694 = *(int *)0x2af1ac;
  if (*(float *)0x325698 == *(float *)0x2533c0)
    *(int *)0x325698 = *(int *)0x2af1b0;
  if (*(float *)0x32569c == *(float *)0x2533c0)
    *(int *)0x32569c = *(int *)0x2af1b4;
  if (*(float *)0x3256a0 == *(float *)0x2533c0)
    *(int *)0x3256a0 = *(int *)0x2af1b8;
  ((void (*)(float *))0x157940)(elapsed);
}

int rasterizer_windows_begin(void)
{
  return ((int (*)(void))0x1559d0)();
}

static void sanitize_window_screen_flash(window_parameters_t *parameters)
{
  int32_t *flash_type = (int32_t *)((char *)parameters + 0x238);
  float *flash_scale = (float *)((char *)parameters + 0x23c);
  float *flash_color = (float *)((char *)parameters + 0x240);

  if (*flash_type == 0) {
    return;
  }

  if (!(*flash_scale >= 0.0f && *flash_scale <= 1.0f)) {
    *flash_scale = 0.0f;
    *flash_type = 0;
    return;
  }

  if (!(flash_color[0] >= 0.0f && flash_color[0] <= 1.0f &&
        flash_color[1] >= 0.0f && flash_color[1] <= 1.0f &&
        flash_color[2] >= 0.0f && flash_color[2] <= 1.0f &&
        flash_color[3] >= 0.0f && flash_color[3] <= 1.0f)) {
    *flash_type = 0;
    *flash_scale = 0.0f;
    flash_color[0] = 0.0f;
    flash_color[1] = 0.0f;
    flash_color[2] = 0.0f;
    flash_color[3] = 0.0f;
  }
}

int rasterizer_window_begin(window_parameters_t *a1)
{
  sanitize_window_screen_flash(a1);
  return ((int (*)(window_parameters_t *))0x158df0)(a1);
}

void rasterizer_window_end(void)
{
  ((void (*)(void))0x158f90)();
}

void rasterizer_windows_end(void)
{
  ((void (*)(void))0x155a40)();
}

void rasterizer_frame_end(void)
{
  ((void (*)(void))0x155a70)();
}

void rasterizer_set_vblank_callback(void *cb)
{
  ((void (*)(void *))0x155c10)(cb);
}

/* 0x172a30
 *
 * FUN_00172a30
 *
 * Shadow-pass begin / shadow-generate setup. Programs the D3D render
 * states, pixel shader, and vertex-shader constants for the shadow
 * generation pass, then stashes the shadow projection matrix, RGB color,
 * and object bounding radius into the module-global shadow parameter block
 * (0x47e46c..).
 *
 * Asserts the D3D device exists. When rendering is enabled
 * (*(short *)0x5a5bc0 == 0) and the shadow feature flag is set
 * (*(char *)0x3256ca != 0):
 *   1. Validates the matrix/color pointers, each RGB component (in [0,1]),
 *      and the object bounding radius (> 0).
 *   2. Sets cull mode, four "simple" render states (each mirrored into a
 *      module global at 0x1fb7a4/784/788/78c), disables Z test and Z bias.
 *   3. Clears and programs the 0xf0-byte pixel-shader state block at
 *      0x5a5ac0, then binds it.
 *   4. Builds five vertex-shader constant registers - a shadow-projection
 *      transform scaled by 1/radius - and uploads them at register -0x44.
 *   5. Stashes the 13-dword matrix, RGB color, and radius into the shadow
 *      parameter block, and clears the associated state bytes.
 *   6. Optionally writes the radius back through out_radius.
 *   7. If the render-mode word (*(short *)0x3256ba) == 2, bumps the
 *      per-frame counter at 0x5a5430.
 *
 * param_1:                unused (present for the cdecl caller ABI).
 * shadow_matrix:          shadow projection matrix (13 dwords / 4x3-ish).
 * shadow_color:           RGB shadow color (3 floats, each in [0,1]).
 * object_bounding_radius: bounding radius (> 0); its reciprocal scales the
 *                         projection transform.
 * out_radius:             optional; receives object_bounding_radius.
 *
 * Returns 1 (AL).
 */
char FUN_00172a30(int param_1, const float *shadow_matrix,
                  const float *shadow_color, float object_bounding_radius,
                  float *out_radius)
{
  float vs_const[20];
  float inv_r;
  const unsigned long *src;
  unsigned long *dst;
  int i;

  (void)param_1;

  if (*(void **)0x476ab0 == 0) {
    display_assert(
      "global_d3d_device",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x93, 1);
    system_exit(-1);
  }
  if (*(short *)0x5a5bc0 == 0 && *(char *)0x3256ca != 0) {
    if (shadow_matrix == 0) {
      display_assert(
        "shadow_matrix",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x99,
        1);
      system_exit(-1);
    }
    if (shadow_color == 0) {
      display_assert(
        "shadow_color",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9a,
        1);
      system_exit(-1);
    }
    if (!(shadow_color[0] >= 0.0f) || !(shadow_color[0] <= 1.0f)) {
      display_assert(
        "shadow_color->red >=0.0f && shadow_color->red <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9b,
        1);
      system_exit(-1);
    }
    if (!(shadow_color[1] >= 0.0f) || !(shadow_color[1] <= 1.0f)) {
      display_assert(
        "shadow_color->green>=0.0f && shadow_color->green<=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9c,
        1);
      system_exit(-1);
    }
    if (!(shadow_color[2] >= 0.0f) || !(shadow_color[2] <= 1.0f)) {
      display_assert(
        "shadow_color->blue >=0.0f && shadow_color->blue <=1.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9d,
        1);
      system_exit(-1);
    }
    if (!(object_bounding_radius > 0.0f)) {
      display_assert(
        "object_bounding_radius>0.0f",
        "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_shadows.c", 0x9e,
        1);
      system_exit(-1);
    }

    /* Render state: cull, four "simple" states (mirrored to module globals),
     * Z test/bias off. Each mirror store is paired with its state by value;
     * MSVC schedules the store into the following call's setup window. */
    D3DDevice_SetRenderState_CullMode(0x901);
    D3DDevice_SetRenderState_Simple(0x40358, 0x10101);
    *(unsigned long *)0x1fb7a4 = 0x10101;
    D3DDevice_SetRenderState_Simple(0x40304, 0);
    *(unsigned long *)0x1fb784 = 0;
    D3DDevice_SetRenderState_Simple(0x40300, 1);
    *(unsigned long *)0x1fb788 = 1;
    D3DDevice_SetRenderState_Simple(0x40340, 0x7f);
    *(unsigned long *)0x1fb78c = 0x7f;
    D3DDevice_SetRenderState_ZEnable(0);
    D3DDevice_SetRenderState_ZBias(0);

    /* Program and bind the shadow-generation pixel-shader state block. */
    csmemset((void *)0x5a5ac0, 0, 0xf0);
    *(int *)0x5a5b98 = 1;
    *(int *)0x5a5b94 = 1;
    *(int *)0x5a5ae0 = 0x20;
    *(int *)0x5a5ae4 = 0x1800;
    rasterizer_set_pixel_shader((void *)0x5a5ac0);

    /* Vertex-shader constants: rows 0/1 are the shadow projection scaled by
     * 1/radius; the trailing constants are fixed. All 20 floats form one
     * contiguous buffer that SetVertexShaderConstant uploads (5 registers). */
    inv_r = 1.0f / object_bounding_radius;
    vs_const[8] = 0.0f;
    vs_const[9] = 0.0f;
    vs_const[10] = 0.0f;
    vs_const[11] = 0.5f;
    vs_const[12] = 0.0f;
    vs_const[0] = inv_r * shadow_matrix[1];
    vs_const[1] = inv_r * shadow_matrix[2];
    vs_const[2] = inv_r * shadow_matrix[3];
    vs_const[3] = -((shadow_matrix[10] * shadow_matrix[1] +
                     shadow_matrix[11] * shadow_matrix[2] +
                     shadow_matrix[12] * shadow_matrix[3]) *
                    inv_r);
    vs_const[4] = inv_r * shadow_matrix[4];
    vs_const[5] = inv_r * shadow_matrix[5];
    vs_const[6] = inv_r * shadow_matrix[6];
    vs_const[7] = -((shadow_matrix[10] * shadow_matrix[4] +
                     shadow_matrix[11] * shadow_matrix[5] +
                     shadow_matrix[12] * shadow_matrix[6]) *
                    inv_r);
    vs_const[13] = 0.0f;
    vs_const[14] = 0.0f;
    vs_const[15] = 1.0f;
    vs_const[16] = 0.0f;
    vs_const[17] = 0.0f;
    vs_const[18] = 0.0f;
    vs_const[19] = 0.0f;
    D3DDevice_SetVertexShaderConstant(-0x44, vs_const, 5);

    FUN_00158140(2, 0,
                 (*(unsigned char *)0x3256f7 != 0) ? 0x88888888u : 0u, 1, 0);
    FUN_00158ae0(0);

    /* Stash the 13-dword matrix, then the RGB color, then the radius. */
    src = (const unsigned long *)shadow_matrix;
    dst = (unsigned long *)0x47e47c;
    for (i = 0xd; i != 0; i--) {
      *dst = *src;
      src++;
      dst++;
    }
    *(float *)0x47e46c = shadow_color[0];
    *(float *)0x47e470 = shadow_color[1];
    *(float *)0x47e474 = shadow_color[2];
    *(float *)0x47e478 = object_bounding_radius;
    if (out_radius != 0) {
      *out_radius = object_bounding_radius;
    }
    *(int *)0x47e4b0 = 0;
    *(char *)0x47e4b4 = 0;
    *(char *)0x47e4b5 = 0;
    *(char *)0x3251fc = 0;
    if (*(short *)0x3256ba == 2) {
      *(int *)0x5a5430 = *(int *)0x5a5430 + 1;
    }
  }
  return 1;
}
