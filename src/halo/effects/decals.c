#include "x87_math.h"

/*
 * decal_check — consistency check for a doubly-linked decal entry.
 * Verifies that the decal's prev (0x30) and next (0x34) neighbors share the
 * same cluster_index (+4) and, if layer_check is true, the same layer (+6).
 * Calls datum_get twice per neighbor to read each field independently,
 * matching the original MSVC codegen.
 *
 * 0x98970 / decals.obj
 */
void decal_check(int handle, bool layer_check)
{
  void *decal;
  void *other;
  int16_t cluster;
  int16_t lyr;

  decal = datum_get(global_decal_data, handle);

  if (*(int *)((char *)decal + 0x30) != -1) {
    other = datum_get(global_decal_data, *(int *)((char *)decal + 0x30));
    cluster = *(int16_t *)((char *)other + 4);
    other = datum_get(global_decal_data, *(int *)((char *)decal + 0x30));
    lyr = *(int16_t *)((char *)other + 6);
    if (cluster != *(int16_t *)((char *)decal + 4)) {
      display_assert("cluster_index==decal->cluster_index",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xc2, 1);
      system_exit(-1);
    }
    if (layer_check && lyr != *(int16_t *)((char *)decal + 6)) {
      display_assert("!layer_check || layer==decal->layer",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xc3, 1);
      system_exit(-1);
    }
  }

  if (*(int *)((char *)decal + 0x34) != -1) {
    other = datum_get(global_decal_data, *(int *)((char *)decal + 0x34));
    cluster = *(int16_t *)((char *)other + 4);
    other = datum_get(global_decal_data, *(int *)((char *)decal + 0x34));
    lyr = *(int16_t *)((char *)other + 6);
    if (cluster != *(int16_t *)((char *)decal + 4)) {
      display_assert("cluster_index==decal->cluster_index",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xcb, 1);
      system_exit(-1);
    }
    if (layer_check && lyr != *(int16_t *)((char *)decal + 6)) {
      display_assert("!layer_check || layer==decal->layer",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0xcc, 1);
      system_exit(-1);
    }
  }
}

/*
 * decal_set_first_decal_index — set the first decal datum index for a cluster/layer slot.
 * Validates cluster_index in [0, 512) and layer in [0, 5), then writes
 * param_1 into decal_globals at [layer * 512 + cluster_index].
 * Counterpart to decal_get_first_decal_index (getter). Takes cluster_index in SI, layer in DI.
 *
 * 0x98aa0 / decals.obj
 */
void decal_set_first_decal_index(int16_t cluster_index, int16_t layer, int param_1)
{
  if (cluster_index < 0 || cluster_index >= 0x200) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0xd8, 1);
    system_exit(-1);
  }
  if (layer < 0 || layer >= 5) {
    display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0xd9, 1);
    system_exit(-1);
  }
  decal_globals->first_decal_index[layer][cluster_index] = param_1;
}

void decal_sprite_get_bounds(float *sprite_bounds, void *definition,
                  int16_t sequence_index, int16_t sprite_index, float extent,
                  float *out_extent)
{
  char *definition_data;
  char *bitmap_tag;
  char *sequence;
  int16_t *sprite;
  char *bitmap;
  float ratio;
  float x_scale;
  float y_scale;

  ratio = 1.0f;
  if (definition == NULL) {
    display_assert("definition", "c:\\halo\\SOURCE\\effects\\decals.c", 0x107,
                   true);
    system_exit(-1);
  }

  if (sprite_bounds == NULL) {
    display_assert("sprite_bounds", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x108, true);
    system_exit(-1);
  }

  if (out_extent == NULL) {
    display_assert("extent", "c:\\halo\\SOURCE\\effects\\decals.c", 0x109,
                   true);
    system_exit(-1);
  }

  definition_data = (char *)definition;

  bitmap_tag = (char *)tag_get(0x6269746d, *(int *)(definition_data + 0xe4));
  sequence =
    (char *)tag_block_get_element(bitmap_tag + 0x54, (int)sequence_index, 0x40);
  sprite =
    (int16_t *)tag_block_get_element(sequence + 0x34, (int)sprite_index, 0x20);
  bitmap =
    (char *)tag_block_get_element(bitmap_tag + 0x60, (int)sprite[0], 0x30);

  sprite_bounds[0] = *(float *)(sprite + 4);
  sprite_bounds[1] = *(float *)(sprite + 6);
  sprite_bounds[2] = *(float *)(sprite + 8);
  sprite_bounds[3] = *(float *)(sprite + 10);

  if ((definition_data[1] & 1) != 0) {
    ratio = ((float)(int)*(int16_t *)(bitmap + 6) /
             (float)(int)*(int16_t *)(bitmap + 4)) *
            ((*(float *)(sprite + 6) - *(float *)(sprite + 4)) /
             (*(float *)(sprite + 10) - *(float *)(sprite + 8)));
  }

  extent = extent / *(float *)(definition_data + 0xfc);
  x_scale = (float)(int)*(int16_t *)(bitmap + 4) * extent;
  y_scale = (float)(int)*(int16_t *)(bitmap + 6) * extent * ratio;

  out_extent[0] = -*(float *)(sprite + 0xc) * x_scale;
  out_extent[1] = ((*(float *)(sprite + 6) - *(float *)(sprite + 0xc)) -
                   *(float *)(sprite + 4)) *
                  x_scale;
  out_extent[2] = -*(float *)(sprite + 0xe) * y_scale;
  out_extent[3] = ((*(float *)(sprite + 10) - *(float *)(sprite + 0xe)) -
                   *(float *)(sprite + 8)) *
                  y_scale;
}

void decals_initialize(void)
{
  global_decal_data = game_state_data_new("decals", 0x800, 0x38);
  assert_halt(global_decal_data);
  global_decal_data->identifier_zero_invalid = 1;
  decal_globals = game_state_malloc("decal globals", 0, 0x280c);
  assert_halt(decal_globals);
  __rasterizer_decals_initialize();
  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

void decals_initialize_for_new_map(void)
{
  assert_halt(global_decal_data);
  assert_halt(decal_globals);
  csmemset(decal_globals, 0xFF, 0x2800);
  decal_globals->first_disconnected_decal_index = -1;
  decal_globals->locked_count = 0;
  decal_globals->permanent_count = 0;
  data_delete_all(global_decal_data);
  __rasterizer_decals_initialize_for_new_map();
  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

void decals_dispose_from_old_map(void)
{
  assert_halt(global_decal_data);
  assert_halt(decal_globals);
  __rasterizer_decals_dispose_from_old_map();
  data_make_invalid(global_decal_data);
}

void decals_dispose(void)
{
  global_decal_data = 0;
  rasterizer_decals_dispose();
}

/* decals_unlock (0x98e70)
 *
 * Scans every live decal entry and clears transient-lifetime flags:
 *   bit 0 (0x1) = "locked"   — always cleared, decrements locked count
 *   bit 1 (0x2) = "permanent" — only cleared when full_reset is true,
 *                               decrements permanent count
 *
 * After the scan, if either count is non-zero an error is logged once
 * (per-static error-once flag at 0x4557de / 0x4557df) and the count is
 * reset to zero.  Finally, the two decal counters (decal_counts_0,
 * decal_counts_1) are zeroed.
 *
 * The bit fields are in the uint16_t at decal_entry+0x2; the counts
 * live in decal_globals+0x2804 (locked) and decal_globals+0x2808 (permanent).
 */
void decals_unlock(bool full_reset)
{
  assert_halt(global_decal_data);

  if (*(uint8_t *)((char *)global_decal_data + 0x24) != 0) {
    data_iter_t iter;
    int16_t *entry;
    decal_globals_t *dg;

    assert_halt(decal_globals);

    data_iterator_new(&iter, global_decal_data);
    while ((entry = (int16_t *)data_iterator_next(&iter)) != NULL) {
      dg = decal_globals;
      if (entry[1] & 1) {
        /* Clear locked flag and decrement locked count. */
        entry[1] &= ~1;
        dg->locked_count -= 1;
      }
      if (full_reset && (entry[1] & 2)) {
        /* Clear permanent flag and decrement permanent count. */
        entry[1] &= ~2;
        dg->permanent_count -= 1;
      }
    }

    dg = decal_globals;

    /* Sanity-check locked count. */
    if (dg->locked_count != 0) {
      if (!*(uint8_t *)0x4557de) {
        error(
          2, "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
          dg->locked_count);
        dg = decal_globals;
        *(uint8_t *)0x4557de = 1;
      }
      dg->locked_count = 0;
    }

    /* Sanity-check permanent count (only meaningful on full reset). */
    if (full_reset && dg->permanent_count != 0) {
      if (!*(uint8_t *)0x4557df) {
        error(
          2,
          "### ERROR decals: permanent count is invalid (#%d) -- tell Bernie!!",
          dg->permanent_count);
        dg = decal_globals;
        *(uint8_t *)0x4557df = 1;
      }
      dg->permanent_count = 0;
    }
  }

  decal_counts_0 = 0;
  decal_counts_1 = 0;
}

/* decal_cluster_get_first_index (0x98fe0)
 *
 * Returns the first decal datum index for the given cluster and layer.
 * Validates that cluster_index is in [0, 512) and layer is in [0, 5).
 * Indexes into the decal_globals array: [layer * 512 + cluster_index]. */
int decal_get_first_decal_index(int16_t cluster_index, int16_t layer)
{
  assert_halt(cluster_index >= 0 && cluster_index < 0x200);
  assert_halt(layer >= 0 && layer < 5);

  return decal_globals->first_decal_index[layer][cluster_index];
}

/* Projection axis remapping table at 0x28cb10. */
static const int16_t g_projection_axes[6][2] = {
  { 2, 1 }, { 1, 2 }, { 0, 2 }, { 2, 0 }, { 1, 0 }, { 0, 1 },
};

/* project_point2d (0x992d0)
 *
 * Unprojects a 2D point back to 3D using a plane equation and a projection
 * axis.  The projection axis (0=x, 1=y, 2=z) selects which coordinate to
 * solve for; the sign bit picks one of two axis-remapping orders from
 * g_projection_axes.  The two known 2D coordinates are placed into
 * out_point at the remapped axis slots, and the third is computed from
 * the plane equation  (d - a*u - b*v) / c.  If the plane's coefficient
 * along the projection axis is near zero, a large sentinel value is stored
 * instead.
 *
 * Inlined from ..\\math\\real_math.h (line 0x36f).
 */
void project_point2d(float *point_2d, float *plane, int16_t projection,
                     uint8_t sign, float *out_point)
{
  int proj_i;
  int table_index;
  int16_t axis_a;
  int16_t axis_b;

  proj_i = (int)projection;
  table_index = (uint32_t)sign + proj_i * 2;
  axis_a = g_projection_axes[table_index][0];
  axis_b = g_projection_axes[table_index][1];

  if (projection < 0 || projection > 2) {
    display_assert("projection>=_x && projection<=_z", "..\\math\\real_math.h",
                   0x36f, true);
    system_exit(-1);
  }

  if (~(sign & ~1) == 0) {
    display_assert("~(sign&~1)", "..\\math\\real_math.h", 0x370, true);
    system_exit(-1);
  }

  out_point[(int)axis_a] = point_2d[0];
  out_point[(int)axis_b] = point_2d[1];

  if (x87_fabs(plane[proj_i]) < *(double *)0x2533d0) {
    out_point[proj_i] = *(float *)0x2533c0;
    return;
  }

  out_point[proj_i] = (plane[3] - plane[(int)axis_a] * point_2d[0] -
                       plane[(int)axis_b] * point_2d[1]) /
                      plane[proj_i];
}

/* triple_product3d (0x993b0)
 *
 * Computes the scalar triple product: dot(cross(p, q), r).
 * Equivalent to the signed volume of the parallelepiped formed by vectors
 * p, q, r. Returns a float. */
float triple_product3d(float *p, float *q, float *r)
{
  float cross[3];

  cross[0] = p[1] * q[2] - p[2] * q[1];
  cross[1] = p[2] * q[0] - p[0] * q[2];
  cross[2] = p[0] * q[1] - q[0] * p[1];

  return cross[0] * r[0] + cross[1] * r[1] + cross[2] * r[2];
}

/* plane2d_from_points (0x99400)
 *
 * Computes a 2D line equation (normal + distance) from two 2D points.
 * The line normal is the perpendicular of (point_b - point_a), normalized.
 * out_line[0..1] = unit normal, out_line[2] = signed distance from origin.
 * Returns out_line on success, or NULL if the two points are coincident
 * (length below epsilon 0x2533d0) or the length equals 0.0f (0x2533c0).
 *
 * The original inlines Bungie's normalize2d: the FSQRT result stays in ST
 * for the whole function (FDIV ST(1), FCOMP against 0.0f) and is never
 * rounded to float, so `length` is typed double here. A float local makes
 * cl.exe spill (and round) it; the (float) cast on the zero test mirrors the
 * original's float-typed compare and cannot change the outcome (length is
 * already known to be >= 1e-4 on that path).
 */
float *plane2d_from_points(float *out_line, float *point_a, float *point_b)
{
  double length;
  float inv_length;

  out_line[0] = point_b[1] - point_a[1];
  out_line[1] = point_a[0] - point_b[0];

  length = x87_sqrtd(out_line[0] * out_line[0] + out_line[1] * out_line[1]);

  if (!(fabs(length) < *(double *)0x2533d0)) {
    inv_length = *(float *)0x2533c8 / length;
    out_line[0] = inv_length * out_line[0];
    out_line[1] = inv_length * out_line[1];

    if ((float)length != *(float *)0x2533c0) {
      out_line[2] = out_line[0] * point_a[0] + out_line[1] * point_a[1];
      return out_line;
    }
  }

  out_line[2] = 0.0f;
  return NULL;
}

/*
 * plane3d_from_point_and_normal — build a 3D plane from a point on the plane and its normal.
 *
 * plane_out[0..2] = normal[0..2], copied as one 12-byte aggregate
 * (Bungie: `plane->n = *normal;`) -- the original's MOV EDX,EAX + three
 * dword MOVs through ESI is cl.exe's struct-assignment shape, which three
 * element-wise float stores do not reproduce. plane_out[3] =
 * dot(plane_out[0..2], point).
 *
 * NOTE (binary fidelity): the dot product's left operand is re-read out of
 * plane_out (EAX), not out of the normal argument, even though the two hold
 * equal values after the copy. Preserve that shape — substituting normal[i]
 * changes the emitted loads. Terms are written x + y + z in source order; the
 * x87 push order in the original (z, then y, then x with FADDPs) is exactly
 * what MSVC emits for left-to-right evaluation of that expression.
 *
 * 0x99490 / decals.obj
 */
void plane3d_from_point_and_normal(float *plane_out, float *point, float *normal)
{
  *(vector3_t *)plane_out = *(vector3_t *)normal;
  plane_out[3] =
    plane_out[0] * point[0] + plane_out[1] * point[1] + plane_out[2] * point[2];
}

/* Signed distance from a point to a plane (normal·point - d). */
float plane3d_distance_to_point(float *plane, float *point)
{
  return plane[0] * point[0] + plane[1] * point[1] + plane[2] * point[2] -
         plane[3];
}

uint32_t real_a_rgb_color_to_pixel32(float alpha, float *color)
{
  float scale;
  uint32_t result;

  scale = 255.0f;
  if (!(alpha >= 0.0f && alpha <= 1.0f)) {
    display_assert("alpha>=0.0f && alpha<=1.0f",
                   "..\\bitmaps\\bitmaps_inlines.h", 0xf3, true);
    system_exit(-1);
  }

  if (!valid_real_rgb_color(color)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_rgb_color(%f, %f, %f)",
                            "color", (double)color[0], (double)color[1],
                            (double)color[2]),
                   "..\\bitmaps\\bitmaps_inlines.h", 0xf4, true);
    system_exit(-1);
  }

  /* The original is an inline __asm block (Bungie's bitmaps_inlines.h form):
   * all four channels are scaled in ST and converted with FISTP, i.e. rounded
   * to nearest under the default FPU control word -- not truncated the way a
   * C (int) cast / _ftol2 would. Blue lands in bits 0-7, alpha in 24-31. */
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    mov   edx, color
    fld   alpha
    fld   dword ptr [edx]
    fld   dword ptr [edx+4]
    fld   dword ptr [edx+8]
    fld   scale
    fmul  st(4), st
    fmul  st(3), st
    fmul  st(2), st
    fmulp st(1), st
    fistp result
    and   result, 0FFh
    mov   edx, result
    fistp result
    and   result, 0FFh
    shl   result, 8
    or    edx, result
    fistp result
    and   result, 0FFh
    shl   result, 16
    or    edx, result
    fistp result
    shl   result, 24
    or    edx, result
    mov   result, edx
  }
#else
  __asm__ __volatile__("flds %[alpha]\n\t"
                       "flds (%[color])\n\t"
                       "flds 4(%[color])\n\t"
                       "flds 8(%[color])\n\t"
                       "flds %[scale]\n\t"
                       "fmul %%st, %%st(4)\n\t"
                       "fmul %%st, %%st(3)\n\t"
                       "fmul %%st, %%st(2)\n\t"
                       "fmulp %%st, %%st(1)\n\t"
                       "fistpl %[result]\n\t"
                       "andl $0xff, %[result]\n\t"
                       "movl %[result], %%edx\n\t"
                       "fistpl %[result]\n\t"
                       "andl $0xff, %[result]\n\t"
                       "shll $8, %[result]\n\t"
                       "orl %[result], %%edx\n\t"
                       "fistpl %[result]\n\t"
                       "andl $0xff, %[result]\n\t"
                       "shll $16, %[result]\n\t"
                       "orl %[result], %%edx\n\t"
                       "fistpl %[result]\n\t"
                       "shll $24, %[result]\n\t"
                       "orl %[result], %%edx\n\t"
                       "movl %%edx, %[result]"
                       : [result] "=m"(result)
                       : [alpha] "m"(alpha), [color] "r"(color),
                         [scale] "m"(scale)
                       : "edx", "cc", "memory");
#endif

  return result;
}

/* bsp3d_get_plane_from_designator (0x99640)
 *
 * Extracts a plane equation from a BSP's plane tag block.  The plane_reference
 * encodes both the plane index (low 31 bits) and a sign-flip flag (bit 31).
 * When the sign bit is set, all four plane components (normal xyz + d) are
 * negated, effectively flipping the plane to face the opposite direction.
 * Each plane element is 0x10 bytes (four floats: i, j, k, d).
 */
void bsp3d_get_plane_from_designator(int structure_bsp,
                                     uint32_t plane_reference, float *out_plane)
{
  float *plane_data;

  plane_data = (float *)tag_block_get_element(
    (char *)structure_bsp + 0xc, (int)(plane_reference & 0x7fffffff), 0x10);

  if (plane_reference & 0x80000000) {
    out_plane[0] = -plane_data[0];
    out_plane[1] = -plane_data[1];
    out_plane[2] = -plane_data[2];
    out_plane[3] = -plane_data[3];
  } else {
    *(real_plane3d *)out_plane = *(real_plane3d *)plane_data;
  }
}

/*
 * decal_update — age one decal, fading or retiring it.
 *
 * Computes the decal's age in seconds from the game clock, then either
 * retires it (age has reached its lifetime) or sets its alpha from the
 * fade-out ramp. Decals flagged permanent (bit 1) stay at full alpha.
 *
 * Retirement releases the decal's claim on the locked-decal budget if it held
 * one (flag bit 0), then calls the rasterizer-side free FUN_0017cb10. The
 * "tell Bernie" underflow warning is the same one-shot pattern as in
 * decals_unlock above, with its own latch byte (0x4557dc here,
 * 0x4557dd there) so the two sites report independently.
 *
 * Decal fields used:
 *   +0x02 int16_t  flags: bit 0 = counted against the locked budget,
 *                         bit 1 = permanent (never fades or expires)
 *   +0x14 int      birth game time, in ticks
 *   +0x1c float    lifetime, in seconds
 *   +0x20 float    fade-out duration, in seconds
 *   +0x28 uint8_t  alpha
 *   +0x2c int      definition_index ('deca' tag index)
 *
 * ABI: one register param, no stack params (plain RET, and the single call
 * site @00099fb3 in decals_update does no cleanup). `decal_index` arrives in
 * EDI (@<edi>): PUSH EDI @000996bc forwards it as datum_get's second argument
 * with no prior write to EDI in this function, and EDI is never popped --
 * ADD ESP,8 @000996c3 after three pushes proves PUSH ESI @000996bb is the
 * callee-save (ESI is popped at all three exits) while EDI and EAX are the two
 * arguments. It is forwarded again to FUN_0017cb10 @0009978b. Ghidra typed the
 * function void(void).
 *
 * Confirmed: FCOMP branch polarities, decoded from the FNSTSW mask bits
 *            (AH bit0 = C0/less, bit2 = C2/unordered, bit6 = C3/equal) rather
 *            than from an idiom table:
 *              TEST AH,0x44 + JNP @00099732 -> taken iff equal
 *              TEST AH,0x05 + JNP @0009973f -> taken iff age < lifetime
 *              TEST AH,0x41 + JNE @000997a3 -> taken iff lifetime <= 0
 *              TEST AH,0x05 + JP  @000997ca -> taken iff remaining >= fade
 *              TEST AH,0x01 + JNE @000997da -> taken iff f < 0
 *              TEST AH,0x41 + JNP @000997ea -> taken iff f <= 1
 * Confirmed: constants read out of the XBE -- 0x2533c0 = 0.0f,
 *            0x2533c8 = 1.0f, 0x269dc0 = 0.033333335f (one tick in seconds),
 *            0x2602c8 = 255.0f. Assert strings at 0x26a030 and 0x26a01c,
 *            file string 0x269e0c; both tails call system_exit(-1), not
 *            halt_and_catch_fire.
 * Confirmed: the alpha=0xff store @0009971d sits between the TEST and the Jcc
 *            of the permanent-flag branch, so it runs unconditionally.
 * Uncertain: the tag_get result is loaded into EAX and never read. The call is
 *            kept because it is a load-bearing side effect (it faults on an
 *            invalid tag index), but the original source presumably bound it to
 *            a variable this path does not use.
 * Uncertain: the final conversion is a bare FLD/FISTP @0009981b with no
 *            control-word change, so it uses the FPU's round-to-nearest mode --
 *            NOT a C (int) cast, which must truncate. x87_round_to_int
 *            reproduces the instruction; a plain cast would differ by one for
 *            fractional products.
 *
 * 0x996b0 / decals.obj
 */
void decal_update(int decal_index)
{
  char *decal;
  int16_t flags;
  float age;
  float f;

  decal = (char *)datum_get(global_decal_data, decal_index);
  age = (float)(game_time_get() - *(int *)(decal + 0x14)) * 0.033333335f;

  if (*(int *)(decal + 0x2c) == NONE) {
    display_assert("decal->definition_index!=NONE",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0x133, true);
    system_exit(-1);
  }
  tag_get(0x64656361 /* 'deca' */, *(int *)(decal + 0x2c));

  flags = *(int16_t *)(decal + 2);
  *(uint8_t *)(decal + 0x28) = 0xff;
  if ((flags & 2) != 0)
    return;

  if (*(float *)(decal + 0x1c) != 0.0f && age >= *(float *)(decal + 0x1c)) {
    if ((flags & 1) != 0) {
      *(int16_t *)(decal + 2) = (int16_t)(flags & 0xfffe);
      decal_globals->locked_count -= 1;
      if (decal_globals->locked_count < 0 && *(uint8_t *)0x4557dc == 0) {
        error(
          2, "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
          decal_globals->locked_count);
        *(uint8_t *)0x4557dc = 1;
      }
    }
    FUN_0017cb10(decal_index);
    return;
  }

  if (*(float *)(decal + 0x1c) <= 0.0f)
    return;
  if (*(float *)(decal + 0x20) <= 0.0f)
    return;
  if (*(float *)(decal + 0x1c) - age >= *(float *)(decal + 0x20))
    return;

  f = (*(float *)(decal + 0x1c) - age) / *(float *)(decal + 0x20);
  if (!(f >= 0.0f && f <= 1.0f)) {
    display_assert("f>=0.0f && f<=1.0f", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x142, true);
    system_exit(-1);
  }
  *(uint8_t *)(decal + 0x28) = (uint8_t)x87_round_to_int(f * 255.0f);
}

/*
 * decal_reinsert — prepend a decal to the cluster/layer linked list.
 *
 * Reads the current list head via decal_get_first_decal_index, then initialises the decal's
 * link fields (prev=-1, next=old_head, cluster_index, layer) via datum_get on
 * global_decal_data. If the old head exists it back-links its prev to the new
 * decal. Finally calls decal_set_first_decal_index to update the list head.
 *
 * cluster_index@<ecx>, layer@<ax> are register args; decal_handle is on the
 * stack. ESI=cluster_index, EDI=layer are preserved throughout for the
 * decal_set_first_decal_index call.
 *
 * 0x99840 / decals.obj
 */
void decal_reinsert(int16_t cluster_index, int16_t layer, int decal_handle)
{
  int old_head;
  char *decal;
  char *old_head_decal;

  old_head = decal_get_first_decal_index(cluster_index, layer);
  decal = (char *)datum_get(global_decal_data, decal_handle);
  *(int *)(decal + 0x30) = -1;
  *(int *)(decal + 0x34) = old_head;
  *(int16_t *)(decal + 4) = cluster_index;
  *(int16_t *)(decal + 6) = layer;
  if (old_head != -1) {
    old_head_decal = (char *)datum_get(global_decal_data, old_head);
    *(int *)(old_head_decal + 0x30) = decal_handle;
  }
  decal_set_first_decal_index(cluster_index, layer, decal_handle);
}

static void decals_log_invalid_decal_type_once(
  int16_t decal_type, int decal_tag_index, const char *decal_name,
  int bitmap_tag_index, const char *bitmap_name, const char *context);

int decal_insert(int new_index_hint, int16_t cluster_index, int16_t layer,
                 int old_index, bool randomize)
{
  data_iter_t iter;
  int decal_index;

  decal_index = data_new_datum(global_decal_data, new_index_hint);

  if (cluster_index < 0 || cluster_index >= 0x200) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x1df, true);
    system_exit(-1);
  }
  if (layer < 0 || layer >= 5) {
    display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                   "c:\\halo\\SOURCE\\effects\\decals.c", 0x1e0, true);
    system_exit(-1);
  }

  if (decal_index != -1) {
    char *new_decal = (char *)datum_get(global_decal_data, decal_index);

    if (randomize) {
      *(int16_t *)(new_decal + 2) = 2;
      decal_globals->permanent_count++;
    } else if (100 * random_seed_step(random_math_get_local_seed_address()) < 655350) {
      *(int16_t *)(new_decal + 2) = 1;
      decal_globals->locked_count++;

      if (decal_globals->locked_count > 512) {
        int16_t restart_count = 0;

        data_iterator_new(&iter, global_decal_data);
        while (decal_globals->locked_count > 256) {
          char *decal = (char *)data_iterator_next(&iter);

          if (decal) {
            if ((*(uint8_t *)(decal + 2) & 1) &&
                (100 * random_seed_step(random_math_get_local_seed_address()) < 2686935 ||
                 *(int16_t *)(decal + 4) == -1)) {
              *(uint8_t *)(decal + 2) &= ~1;
              decal_globals->locked_count--;
            }
          } else {
            data_iterator_new(&iter, global_decal_data);
            if (++restart_count >= 100) {
              error(2, "### ERROR decals: failed to unlock decals during "
                       "insert -- tell Bernie!!");
              return -1;
            }
          }
        }

        if (decal_globals->locked_count < 0 && *(uint8_t *)0x4557dd == 0) {
          error(
            2,
            "### ERROR decals: locked count is invalid (#%d) -- tell Bernie!!",
            decal_globals->locked_count);
          *(uint8_t *)0x4557dd = 1;
        }
      }
    } else {
      *(int16_t *)(new_decal + 2) = 0;
    }

    if (old_index != -1) {
      char *next = (char *)datum_get(global_decal_data, old_index);

      if (*(int16_t *)(next + 4) != cluster_index) {
        display_assert("next->cluster_index==cluster_index",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x22c, true);
        system_exit(-1);
      }

      if (*(int *)(next + 0x30) != -1) {
        *(int *)((char *)datum_get(global_decal_data, *(int *)(next + 0x30)) + 0x34) =
          decal_index;
      } else {
        decal_set_first_decal_index(cluster_index, layer, decal_index);
      }

      *(int *)(next + 0x30) = decal_index;
      *(int *)(new_decal + 0x30) = decal_index;
      *(int *)(new_decal + 0x34) = old_index;
      *(int16_t *)(new_decal + 4) = cluster_index;
      *(int16_t *)(new_decal + 6) = layer;
    } else {
      decal_reinsert(cluster_index, layer, decal_index);
    }
  } else {
    error(2, "### ERROR failed to insert decal");
  }

  return decal_index;
}

/*
 * decals_reconnect_to_structure_bsp — walk the disconnected-decal list
 * (decal_globals->first_disconnected_decal_index at +0x2800) and reattach
 * each decal to its structure-BSP cluster. For every decal on the list the
 * next handle (+0x34) is cached BEFORE any relinking, the decal is
 * consistency-checked (decal_check), its cluster is resolved from the decal
 * position (+8) via scenario_location_from_point, and when a valid cluster
 * is found the decal is unlinked from the disconnected list (repairing
 * neighbour prev/next at +0x30/+0x34, or the list head at +0x2800 when it is
 * the first entry) and prepended to the cluster/layer list via decal_reinsert.
 * A post-incremented guard counter aborts with an error after 0x801
 * iterations.
 *
 * 0x99b70 / decals.obj
 */
void decals_reconnect_to_structure_bsp(void)
{
  int guard;
  int location[2]; /* scenario_location, 6 bytes; cluster_index at +4 */
  int decal_index;
  int next;
  char *decal;
  char *other;

  if (global_decal_data == NULL) {
    display_assert("global_decal_data", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x279, true);
    system_exit(-1);
  }

  if (global_decal_data->valid) {
    guard = 0;
    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x281, true);
      system_exit(-1);
    }
    decal_index = decal_globals->first_disconnected_decal_index;
    if (decal_index != -1) {
      do {
        decal = (char *)datum_get(global_decal_data, decal_index);
        /* cache next BEFORE the unlink below rewrites neighbour links */
        next = *(int *)(decal + 0x34);
        if (guard++ > 0x800) {
          error(2, "### ERROR decals: infinite loop -- tell Bernie!!");
          break;
        }
        if (*(int16_t *)(decal + 4) != -1) {
          display_assert("decal->cluster_index==NONE",
                         "c:\\halo\\SOURCE\\effects\\decals.c", 0x294, true);
          system_exit(-1);
        }
        if (*(int16_t *)(decal + 6) < 0 || *(int16_t *)(decal + 6) >= 5) {
          display_assert(
            "decal->layer>=0 && decal->layer<NUMBER_OF_DECAL_LAYERS",
            "c:\\halo\\SOURCE\\effects\\decals.c", 0x295, true);
          system_exit(-1);
        }
        decal_check(decal_index, false);
        scenario_location_from_point(location, decal + 8);
        if (*(int16_t *)((char *)location + 4) != -1) {
          if (*(int *)(decal + 0x34) != -1) {
            other =
              (char *)datum_get(global_decal_data, *(int *)(decal + 0x34));
            *(int *)(other + 0x30) = *(int *)(decal + 0x30);
          }
          if (*(int *)(decal + 0x30) != -1) {
            other =
              (char *)datum_get(global_decal_data, *(int *)(decal + 0x30));
            *(int *)(other + 0x34) = *(int *)(decal + 0x34);
          } else {
            if (decal_globals->first_disconnected_decal_index != decal_index) {
              display_assert(
                "decal_globals->first_disconnected_decal_index==decal_index",
                "c:\\halo\\SOURCE\\effects\\decals.c", 0x2aa, true);
              system_exit(-1);
            }
            decal_globals->first_disconnected_decal_index = *(int *)(decal + 0x34);
          }
          decal_reinsert(*(int16_t *)((char *)location + 4),
                       *(int16_t *)(decal + 6), decal_index);
        }
        decal_check(decal_index, false);
        decal_index = next;
      } while (next != -1);
    }
  }
}

/*
 * decals_disconnect_from_structure_bsp — move every clustered decal onto the
 * disconnected list.
 *
 * Walks all [cluster][layer] lists (layer inner, cluster outer). Every decal
 * visited has its cluster_index (+4, int16_t) cleared to NONE. When the walk
 * reaches the tail of a list (next at +0x34 == NONE) the whole list is spliced
 * onto the front of the disconnected list: the tail's next takes the old
 * disconnected head (decal_globals + 0x2800), that old head's prev (+0x30) is
 * repaired to point at the tail, the disconnected head becomes the list's
 * ORIGINAL first index (the decal_get_first_decal_index result, not the current node), and
 * the [layer][cluster] slot is cleared to NONE. The trailing bound asserts
 * (source lines 0xd8/0xd9) come from decal_set_first_decal_index being inlined
 * here. A post-incremented guard aborts a list after 0x801 iterations.
 *
 * 0x99d60 / decals.obj
 */
void decals_disconnect_from_structure_bsp(void)
{
  int16_t cluster_index;
  int16_t layer;
  int first;
  int guard;
  int decal_index;
  int current;
  char *decal;
  char *other;

  if (global_decal_data == NULL) {
    display_assert("global_decal_data", "c:\\halo\\SOURCE\\effects\\decals.c",
                   0x2c9, true);
    system_exit(-1);
  }

  if (global_decal_data->valid) {
    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x2cf, true);
      system_exit(-1);
    }

    for (cluster_index = 0; cluster_index < 0x200; ++cluster_index) {
      for (layer = 0; layer < 5; ++layer) {
        first = decal_get_first_decal_index(cluster_index, layer);
        guard = 0;
        decal_index = first;

        while (decal_index != -1) {
          current = decal_index;
          decal = (char *)datum_get(global_decal_data, current);
          /* latch next BEFORE the splice below rewrites +0x34 */
          decal_index = *(int *)(decal + 0x34);

          if (guard++ > 0x800) {
            error(2, "### ERROR decals: infinite loop -- tell Bernie!!");
            break;
          }

          if (*(int16_t *)(decal + 4) != cluster_index) {
            display_assert("decal->cluster_index==cluster_index",
                           "c:\\halo\\SOURCE\\effects\\decals.c", 0x2ea, true);
            system_exit(-1);
          }
          *(int16_t *)(decal + 4) = -1;

          if (*(int *)(decal + 0x34) == -1) {
            *(int *)(decal + 0x34) = decal_globals->first_disconnected_decal_index;
            if (decal_globals->first_disconnected_decal_index != -1) {
              other = (char *)datum_get(global_decal_data,
                                        decal_globals->first_disconnected_decal_index);
              *(int *)(other + 0x30) = current;
            }
            decal_globals->first_disconnected_decal_index = first;

            if (cluster_index < 0 || cluster_index >= 0x200) {
              display_assert("cluster_index>=0 && "
                             "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                             "c:\\halo\\SOURCE\\effects\\decals.c", 0xd8, true);
              system_exit(-1);
            }
            if (layer < 0 || layer >= 5) {
              display_assert("layer>=0 && layer<NUMBER_OF_DECAL_LAYERS",
                             "c:\\halo\\SOURCE\\effects\\decals.c", 0xd9, true);
              system_exit(-1);
            }
            decal_globals->first_decal_index[layer][cluster_index] = -1;
          }
        }
      }
    }
  }
}

/*
 * decals_update — age every live decal once per tick.
 *
 * Walks the decal pool with the standard data_iterator pair and calls
 * decal_update for each element. The whole pass is skipped when the pool's
 * "valid" byte at +0x24 is clear (pool not initialised for the current map),
 * matching decals_unlock above -- but note there is no
 * assert_halt on the pool pointer here: the original loads [0x005aa8b8]
 * straight into EAX @00099f86 and dereferences +0x24 with no null check.
 *
 * The pool pointer is read ONCE and reused as data_iterator_new's argument
 * (PUSH EAX @00099f92 re-uses the same EAX the +0x24 test loaded), so the
 * local `data` here is deliberate, not a caching optimisation.
 *
 * decal_update takes its decal handle in EDI (@<edi>); the original sources
 * it from the iterator's datum_handle field at iter+0x8
 * (MOV EDI,[EBP-0x8] @00099fb0 with iter based at EBP-0x10), not from
 * data_iterator_next's returned element pointer.
 */
void decals_update(void)
{
  data_t *data;
  data_iter_t iter;
  void *elem;

  data = global_decal_data;
  if (*(uint8_t *)((char *)data + 0x24) != 0) {
    data_iterator_new(&iter, data);
    elem = data_iterator_next(&iter);
    while (elem != NULL) {
      decal_update((int)iter.datum_handle);
      elem = data_iterator_next(&iter);
    }
  }
}

void decals_delete_permanent_from_cluster(int16_t cluster_index)
{
  if (cluster_index < 0 || cluster_index >= 0x200) {
    display_assert(
      "cluster_index>=0 && cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x377, true);
    system_exit(-1);
  }

  if (*(uint8_t *)((char *)global_decal_data + 0x24) != 0) {
    int16_t layer;

    if (decal_globals == NULL) {
      display_assert("decal_globals", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x37d, true);
      system_exit(-1);
    }

    for (layer = 0; layer < 5; ++layer) {
      int decal_index = -1;

      if (cluster_index == -1) {
        if (layer == 0) {
          decal_index = decal_globals->first_disconnected_decal_index;
        }
      } else {
        decal_index = decal_globals->first_decal_index[layer][cluster_index];
      }

      while (decal_index != -1) {
        int current_decal_index = decal_index;
        char *decal = (char *)datum_get(global_decal_data, current_decal_index);
        decal_index = *(int *)(decal + 0x34);

        if (*(int16_t *)(decal + 4) != cluster_index) {
          display_assert("decal->cluster_index==cluster_index",
                         "c:\\halo\\SOURCE\\effects\\decals.c", 0x398, true);
          system_exit(-1);
        }

        if ((*(uint16_t *)(decal + 2) & 2) != 0) {
          *(uint16_t *)(decal + 2) &= (uint16_t)~2;
          decal_globals->permanent_count -= 1;

          if ((*(uint8_t *)(decal + 2) & 1) != 0) {
            display_assert("!TEST_FLAG(decal->flags, _decal_locked_bit)",
                           "c:\\halo\\SOURCE\\effects\\decals.c", 0x39f, true);
            system_exit(-1);
          }

          FUN_0017cb10(current_decal_index);
        }
      }
    }

    if (decal_globals->permanent_count < 0) {
      display_assert("decal_globals->permanent_count>=0",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x3a8, true);
      system_exit(-1);
    }
  }
}

/*
 * decal_delete — unlink a decal from its list and release its datum.
 *
 * Warns once per session (two independent byte latches) when a locked
 * (+2 bit 0) or permanent (+2 bit 1) decal is deleted, repairs the
 * doubly-linked neighbours (prev at +0x30, next at +0x34), then updates the
 * list head: decal_globals->first_disconnected_decal_index (+0x2800) when
 * cluster_index (+4) is -1, otherwise the [layer][cluster] slot via
 * decal_set_first_decal_index. Every path ends in datum_delete.
 *
 * 0x9a160 / decals.obj
 */
void decal_delete(int decal_index)
{
  void *decal;
  void *other;
  int first;

  decal = datum_get(global_decal_data, decal_index);
  if (decal == NULL) {
    display_assert("decal", "c:\\halo\\SOURCE\\effects\\decals.c", 0x3b3, true);
    system_exit(-1);
  }

  if ((*(uint8_t *)((char *)decal + 2) & 1) != 0 &&
      decals_reported_locked_delete == 0) {
    error(2, "### ERROR decals: deleting locked decal (#%d) -- tell Bernie!!",
          decal_index);
    decals_reported_locked_delete = 1;
  }

  if ((*(uint8_t *)((char *)decal + 2) & 2) != 0 &&
      decals_reported_permanent_delete == 0) {
    error(2,
          "### ERROR decals: deleting permanent decal (#%d) -- tell Bernie!!",
          decal_index);
    decals_reported_permanent_delete = 1;
  }

  if (*(int *)((char *)decal + 0x34) != -1) {
    other = datum_get(global_decal_data, *(int *)((char *)decal + 0x34));
    *(int *)((char *)other + 0x30) = *(int *)((char *)decal + 0x30);
  }

  if (*(int *)((char *)decal + 0x30) != -1) {
    other = datum_get(global_decal_data, *(int *)((char *)decal + 0x30));
    *(int *)((char *)other + 0x34) = *(int *)((char *)decal + 0x34);
    datum_delete(global_decal_data, decal_index);
    return;
  }

  if (*(int16_t *)((char *)decal + 4) == -1) {
    if (decal_globals->first_disconnected_decal_index != decal_index) {
      display_assert(
        "decal_globals->first_disconnected_decal_index==decal_index",
        "c:\\halo\\SOURCE\\effects\\decals.c", 0x3db, true);
      system_exit(-1);
    }
    decal_globals->first_disconnected_decal_index = *(int *)((char *)decal + 0x34);
    datum_delete(global_decal_data, decal_index);
    return;
  }

  first = decal_get_first_decal_index(*(int16_t *)((char *)decal + 4),
                       *(int16_t *)((char *)decal + 6));
  if (first != decal_index) {
    display_assert(
      "decal_get_first_decal_index(decal->cluster_index, decal->layer)"
      "==decal_index",
      "c:\\halo\\SOURCE\\effects\\decals.c", 0x3e0, true);
    system_exit(-1);
  }

  decal_set_first_decal_index(*(int16_t *)((char *)decal + 4), *(int16_t *)((char *)decal + 6),
               *(int *)((char *)decal + 0x34));
  datum_delete(global_decal_data, decal_index);
}

void decal_projection_create(float *bounds, float *projection, float *basis)
{
  typedef struct { float v[4]; } decal_extent_copy_t;
  typedef struct { float v[3]; } decal_vector_copy_t;
  float projected[3];
  float *plane;
  float abs_i;
  float abs_j;
  float abs_k;
  int16_t projection_axis;
  float *u_axis;
  float *v_axis;

  if (basis == NULL) {
    display_assert("basis", "c:\\halo\\SOURCE\\effects\\decals.c", 0x410, true);
    system_exit(-1);
  }

  if (projection == NULL) {
    display_assert("projection", "c:\\halo\\SOURCE\\effects\\decals.c", 0x411,
                   true);
    system_exit(-1);
  }

  qmemcpy(projection, basis, 13 * sizeof(float));

  *(decal_extent_copy_t *)(projection + 0xd) = *(decal_extent_copy_t *)bounds;
  *(decal_vector_copy_t *)(projection + 0x11) =
      *(decal_vector_copy_t *)(basis + 7);
  plane = projection + 0x11;
  plane[3] = plane[2] * basis[12] + plane[1] * basis[11] + plane[0] * basis[10];

  abs_i = x87_fabs(plane[0]);
  abs_j = x87_fabs(plane[1]);
  abs_k = x87_fabs(plane[2]);
  if (abs_k >= abs_j && abs_k >= abs_i) {
    projection_axis = 2;
  } else if (abs_j >= abs_i) {
    projection_axis = 1;
  } else {
    projection_axis = 0;
  }
  *(int16_t *)((char *)projection + 0x54) = projection_axis;

  *(uint8_t *)((char *)projection + 0x56) =
      (uint8_t)projection_sign_from_vector3d(plane, projection_axis);

  projected[0] = bounds[0] * basis[1] + bounds[2] * basis[4] + basis[10];
  projected[1] = bounds[2] * basis[5] + bounds[0] * basis[2] + basis[11];
  projected[2] = bounds[2] * basis[6] + bounds[0] * basis[3] + basis[12];
  project_point3d(projected, *(int16_t *)((char *)projection + 0x54),
               *(uint8_t *)((char *)projection + 0x56), projection + 0x16);

  projected[0] = bounds[1] * basis[1] + bounds[2] * basis[4] + basis[10];
  projected[1] = bounds[2] * basis[5] + bounds[1] * basis[2] + basis[11];
  projected[2] = bounds[2] * basis[6] + bounds[1] * basis[3] + basis[12];
  project_point3d(projected, *(int16_t *)((char *)projection + 0x54),
               *(uint8_t *)((char *)projection + 0x56), projection + 0x18);

  projected[0] = bounds[1] * basis[1] + bounds[3] * basis[4] + basis[10];
  projected[1] = bounds[3] * basis[5] + bounds[1] * basis[2] + basis[11];
  projected[2] = bounds[1] * basis[3] + bounds[3] * basis[6] + basis[12];
  project_point3d(projected, *(int16_t *)((char *)projection + 0x54),
               *(uint8_t *)((char *)projection + 0x56), projection + 0x1a);

  projected[0] = bounds[3] * basis[4] + bounds[0] * basis[1] + basis[10];
  projected[1] = bounds[3] * basis[5] + bounds[0] * basis[2] + basis[11];
  projected[2] = bounds[3] * basis[6] + bounds[0] * basis[3] + basis[12];
  project_point3d(projected, *(int16_t *)((char *)projection + 0x54),
               *(uint8_t *)((char *)projection + 0x56), projection + 0x1c);

  u_axis = projection + 0x1e;
  v_axis = projection + 0x20;
  projection[0x1e] = projection[0x18] - projection[0x16];
  projection[0x1f] = projection[0x19] - projection[0x17];
  projection[0x20] = projection[0x1c] - projection[0x16];
  projection[0x21] = projection[0x1d] - projection[0x17];
  projection[0x22] = *(float *)0x2533c8 /
                     (v_axis[1] * u_axis[0] - u_axis[1] * v_axis[0]);
}

void decal_clip_to_surface(void *geometry, float *projection, int surface_index,
                  bool allow_deviants, float scale, int16_t type,
                  int *surface_queue, int16_t *surface_queue_write_index,
                  int *deviant_surface_list, int16_t *deviant_surface_count)
{
  char *geometry_data;
  int structure_bsp;
  int *surface;
  float plane[4];
  float angle;
  int16_t queue_write_index;
  int16_t deviant_count;

  if (type < 0 || type >= 4) {
    decals_log_invalid_decal_type_once(type, -1, NULL, -1, NULL,
                                       "decal_surface_add");
    return;
  }

  if (surface_index == -1) {
    return;
  }

  structure_bsp = (int)global_collision_bsp_get();
  surface = (int *)tag_block_get_element((char *)structure_bsp + 0x3c,
                                         surface_index, 0xc);

  if (projection == NULL) {
    display_assert("projection", "c:\\halo\\SOURCE\\effects\\decals.c", 0x488,
                   true);
    system_exit(-1);
  }

  if (geometry == NULL) {
    display_assert("geometry", "c:\\halo\\SOURCE\\effects\\decals.c", 0x489,
                   true);
    system_exit(-1);
  }

  geometry_data = (char *)geometry;

  if (allow_deviants) {
    if (surface_queue == NULL) {
      display_assert("surface_queue", "c:\\halo\\SOURCE\\effects\\decals.c",
                     0x48d, true);
      system_exit(-1);
    }

    if (surface_queue_write_index == NULL || *surface_queue_write_index < 0 ||
        *surface_queue_write_index > 0x400) {
      display_assert(
        "surface_queue_write_index && *surface_queue_write_index>=0 "
        "&& *surface_queue_write_index<=MAXIMUM_DECAL_SURFACE_QUE"
        "UE_SIZE",
        "c:\\halo\\SOURCE\\effects\\decals.c", 0x48e, true);
      system_exit(-1);
    }

    if (deviant_surface_list == NULL) {
      display_assert("deviant_surface_list",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x48f, true);
      system_exit(-1);
    }

    if (deviant_surface_count == NULL || *deviant_surface_count < 0 ||
        *deviant_surface_count > 0x400) {
      display_assert("deviant_surface_count && *deviant_surface_count>=0 && "
                     "*deviant_surface_count<=MAXIMUM_DECAL_SURFACE_QUEUE_SIZE",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x490, true);
      system_exit(-1);
    }

    queue_write_index = *surface_queue_write_index;
    deviant_count = *deviant_surface_count;
  }

  bsp3d_get_plane_from_designator(structure_bsp, (uint32_t)surface[0], plane);
  angle = angle_between_normals3d(plane, projection + 0x11);

  /* Original (PAL 2342 source): wrap into neighbours when the surface bends
   * past minimum_wrap_angle, otherwise clip the decal polygon to it. */
  if (allow_deviants &&
      !(angle <= *(float *)(0x269d80 + type * 0x10) * *(float *)0x253d4c)) {
    int edge_index = surface[1];

    do {
      int *edge = (int *)tag_block_get_element((char *)structure_bsp + 0x48,
                                               edge_index, 0x18);
      bool surface_on_right = edge[5] == surface_index;
      float *edge_start = (float *)tag_block_get_element(
        (char *)structure_bsp + 0x54, edge[!surface_on_right], 0x10);

      if (queue_write_index < 0x400) {
        float *edge_end = (float *)tag_block_get_element(
          (char *)structure_bsp + 0x54, edge[surface_on_right], 0x10);
        float edge_vector[3];

        edge_vector[0] = edge_end[0] - edge_start[0];
        edge_vector[1] = edge_end[1] - edge_start[1];
        edge_vector[2] = edge_end[2] - edge_start[2];
        if (fast_vector_intersects_sphere(
              edge_start, edge_vector, projection + 0xa,
              scale * *(float *)(0x269d88 + type * 0x10))) {
          int adjacent_surface_index = edge[4 + !surface_on_right];
          int16_t queue_index = 0;

          while (adjacent_surface_index != -1 &&
                 queue_index < queue_write_index) {
            if (surface_queue[queue_index] == adjacent_surface_index) {
              adjacent_surface_index = -1;
            }

            queue_index++;
          }

          if (adjacent_surface_index != -1) {
            surface_queue[queue_write_index++] = adjacent_surface_index;
          }
        }
      }

      edge_index = edge[2 + surface_on_right];
    } while (edge_index != surface[1]);

    if (angle <= *(float *)(0x269d84 + type * 0x10) * *(float *)0x253d4c &&
        deviant_count < 0x400) {
      deviant_surface_list[deviant_count++] = surface_index;
    }
  } else {
    float *input_points = projection + 0x16;
    int edge_index = surface[1];
    int16_t edge_iteration = 0;
    int16_t decal_point_count = 4;
    uint32_t clipped_flags = 0;
    float *output_points;
    struct {
      float x;
      float y;
    } previous_point, current_point;
    float texture_y;

    do {
      int *edge = (int *)tag_block_get_element((char *)structure_bsp + 0x48,
                                               edge_index, 0x18);
      bool surface_on_right = edge[5] == surface_index;
      float *edge_start = (float *)tag_block_get_element(
        (char *)structure_bsp + 0x54, edge[!surface_on_right], 0x10);

      output_points = (float *)(0x44df10 + (edge_iteration & 1) * 0x60);
      if (edge_iteration == 0) {
        float *edge_end = (float *)tag_block_get_element(
          (char *)structure_bsp + 0x54, edge[surface_on_right], 0x10);

        project_point3d(edge_end, *(int16_t *)((char *)projection + 0x54),
                     *(uint8_t *)((char *)projection + 0x56), &previous_point);
      }

      project_point3d(edge_start, *(int16_t *)((char *)projection + 0x54),
                   *(uint8_t *)((char *)projection + 0x56), &current_point);

      {
        float clipping_plane[3];

        if (plane2d_from_points(clipping_plane, &current_point.x,
                                &previous_point.x)) {
          uint8_t clipped;

          decal_point_count = convex_polygon2d_clip_to_plane(
            decal_point_count, input_points, clipping_plane, 0xc,
            output_points, &clipped_flags, &clipped, 0.0f);

          if (allow_deviants && clipped && queue_write_index < 0x400) {
            float *edge_end = (float *)tag_block_get_element(
              (char *)structure_bsp + 0x54, edge[surface_on_right], 0x10);
            float edge_vector[3];

            edge_vector[0] = edge_end[0] - edge_start[0];
            edge_vector[1] = edge_end[1] - edge_start[1];
            edge_vector[2] = edge_end[2] - edge_start[2];
            if (fast_vector_intersects_sphere(
                  edge_start, edge_vector, projection + 0xa,
                  scale * *(float *)(0x269d88 + type * 0x10))) {
              int adjacent_surface_index = edge[4 + !surface_on_right];
              int16_t queue_index = 0;

              while (adjacent_surface_index != -1 &&
                     queue_index < queue_write_index) {
                if (surface_queue[queue_index] == adjacent_surface_index) {
                  adjacent_surface_index = -1;
                }

                queue_index++;
              }

              if (adjacent_surface_index != -1) {
                surface_queue[queue_write_index++] = adjacent_surface_index;
              }
            }
          }
        } else {
          decal_point_count = 0;
        }
      }

      edge_index = edge[2 + surface_on_right];
      edge_iteration++;
      previous_point = current_point;
      input_points = output_points;
    } while (edge_index != surface[1] && decal_point_count > 0);

    if (decal_point_count >= 3 &&
        decal_point_count <= 0x400 - *(int16_t *)(geometry_data + 0x6000) &&
        ((*(uint8_t *)(surface + 2) & 0xb) == 0)) {
      int16_t decal_point_index;

      if (!(*(int16_t *)(geometry_data + 0x6802) < 0x400)) {
        display_assert(
          "geometry->decal_surface_count<MAXIMUM_DECAL_SURFACE_QUEU"
          "E_SIZE",
          "c:\\halo\\SOURCE\\effects\\decals.c", 0x555, true);
        system_exit(-1);
      }

      *(int *)(geometry_data +
               *(int16_t *)(geometry_data + 0x6802) * 4 + 0x6804) =
        surface_index;
      *(int16_t *)(geometry_data +
                   *(int16_t *)(geometry_data + 0x6802) * 2 + 0x6002) =
        decal_point_count;
      (*(int16_t *)(geometry_data + 0x6802))++;

      for (decal_point_index = 0; decal_point_index < decal_point_count;
           decal_point_index++) {
        float *point = output_points + decal_point_index * 2;
        float offset[2];

        offset[0] = point[0] - projection[0x16];
        offset[1] = point[1] - projection[0x17];
        texture_y = -((offset[0] * projection[0x1f] -
                       offset[1] * projection[0x1e]) * projection[0x22]);
        *(float *)(geometry_data +
                   *(int16_t *)(geometry_data + 0x6000) * 0x18 + 0xc) =
          (offset[0] * projection[0x21] - offset[1] * projection[0x20]) *
          projection[0x22];
        *(float *)(geometry_data +
                   *(int16_t *)(geometry_data + 0x6000) * 0x18 + 0x10) =
          texture_y;
        *(bool *)(geometry_data +
                  *(int16_t *)(geometry_data + 0x6000) * 0x18 + 0x14) =
          (clipped_flags & (1u << decal_point_index)) != 0;

        project_point2d(point, plane, *(int16_t *)((char *)projection + 0x54),
                        *(uint8_t *)((char *)projection + 0x56),
                        (float *)(geometry_data +
                                  *(int16_t *)(geometry_data + 0x6000) *
                                    0x18));

        if (!(clipped_flags & (1u << decal_point_index))) {
          float *vertex = (float *)(geometry_data +
                                    *(int16_t *)(geometry_data + 0x6000) *
                                      0x18);
          float zoffset = *(float *)0x325710;

          vertex[0] += plane[0] * *(float *)0x325710;
          vertex[1] += plane[1] * zoffset;
          vertex[2] += plane[2] * zoffset;
        }

        (*(int16_t *)(geometry_data + 0x6000))++;
      }
    }
  }

  if (allow_deviants) {
    *surface_queue_write_index = queue_write_index;
    *deviant_surface_count = deviant_count;
  }
}

static void decals_assert_or_exit(const char *condition, int line)
{
  display_assert(condition, "c:\\halo\\SOURCE\\effects\\decals.c", line, true);
  system_exit(-1);
}

static float decals_dot3(const float *a, const float *b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float decals_random_real(float min, float max)
{
  return random_real_range((int *)random_math_get_local_seed_address(), min,
                           max);
}

static int16_t decals_random_short(int16_t min, int16_t max)
{
  return seed_random_range(random_math_get_local_seed_address(), min, max);
}

static void
decals_log_invalid_decal_type_once(int16_t decal_type, int decal_tag_index,
                                   const char *decal_name, int bitmap_tag_index,
                                   const char *bitmap_name, const char *context)
{
  static uint32_t reported_mask;

  if (decal_type >= 0 && decal_type < 32) {
    uint32_t bit = 1u << decal_type;
    if ((reported_mask & bit) != 0) {
      return;
    }
    reported_mask |= bit;
  }

  error(2,
        "### ERROR decals: invalid decal type %d in %s (decal=%d '%s' bitm=%d "
        "'%s') -- skipping",
        decal_type, context, decal_tag_index,
        decal_name ? tag_name_strip_path((char *)decal_name) : "<null>",
        bitmap_tag_index,
        bitmap_name ? tag_name_strip_path((char *)bitmap_name) : "<null>");
}

typedef struct s_decal_geometry_vertex {
  float position[3];
  float uv[2];
  uint8_t clipped;
  uint8_t pad[3];
} s_decal_geometry_vertex;

typedef struct s_decal_geometry_scratch {
  s_decal_geometry_vertex vertices[0x400];
  int16_t vertex_count;
  int16_t surface_vertex_counts[0x400];
  int16_t surface_count;
  int surfaces[0x400];
} s_decal_geometry_scratch;

typedef struct s_decal_staged_vertex {
  float position[3];
  int16_t uv[2];
} s_decal_staged_vertex;

typedef struct s_decal_cached_quad {
  s_decal_staged_vertex vertices[4];
} s_decal_cached_quad;

static bool g_warned_decal_vertex_overflow;
static bool g_warned_decal_quad_overflow;

void decal_new_from_collision(int decal_tag_index, int16_t *collision_result,
                              void *direction, float scale, bool randomize,
                              int16_t color_index, int flags)
{
  int structure_bsp = (int)global_collision_bsp_get();
  bool reuse_previous = false;
  float basis[13];
  float extent[4];
  float sprite_bounds[4];
  float radius;
  int16_t sequence_index;
  int16_t sprite_index;
  int16_t bitmap_index;

  if (collision_result == NULL) {
    decals_assert_or_exit("collision_result", 0x7f1);
  }

  if (direction == NULL) {
    decals_assert_or_exit("direction", 0x7f2);
  }

  if (*(uint8_t *)0x2eebd0 == 0) {
    decals_assert_or_exit((char *)0x26a828, 0x7f3);
  }

  if (flags != 0) {
    decals_assert_or_exit((char *)0x26a814, 0x7f5);
  }

  while (decal_tag_index != -1) {
    int16_t queue_read_index = 0;
    int16_t deviant_surface_count = 0;
    char *decal_tag = (char *)tag_get(0x64656361, decal_tag_index);
    char *bitmap_tag = (char *)tag_get(0x6269746d, *(int *)(decal_tag + 0xe4));

    if (*(int16_t *)(decal_tag + 2) >= 0 &&
        *(int16_t *)(decal_tag + 2) < 4) {

      if (!reuse_previous) {
        float axis_vector[3];
        float tangent[3];
        float bitangent[3];
        float cosine;
        float sine;
        float *normal = (float *)(collision_result + 0x12);
        float *velocity = (float *)direction;

        if ((*(uint16_t *)decal_tag & 8) != 0 &&
            decals_dot3(normal, velocity) < *(float *)0x26a810) {
          cosine = -1.0f;
          sine = 0.0f;

          if ((*(uint16_t *)decal_tag & 0x20) != 0) {
            int16_t projection = projection_from_vector3d(velocity);
            float sign = (uint8_t)projection_sign_from_vector3d(velocity, projection) ? 1.0f : -1.0f;

            switch (projection) {
            case 0:
              axis_vector[0] = sign;
              axis_vector[1] = 0.0f;
              axis_vector[2] = 0.0f;
              break;
            case 1:
              axis_vector[0] = 0.0f;
              axis_vector[1] = sign;
              axis_vector[2] = 0.0f;
              break;
            case 2:
              axis_vector[0] = 0.0f;
              axis_vector[1] = 0.0f;
              axis_vector[2] = sign;
              break;
            default:
              display_assert((char *)0x26a7e4, "c:\\halo\\SOURCE\\effects\\decals.c", 0x848, true);

              system_exit(-1);
              break;
            }

            if (decals_dot3(normal, velocity) > *(float *)0x2533c0) {
              axis_vector[0] = axis_vector[0] + normal[0];
              axis_vector[1] = axis_vector[1] + normal[1];
              axis_vector[2] = axis_vector[2] + normal[2];
            } else {
              axis_vector[0] = axis_vector[0] - normal[0];
              axis_vector[1] = axis_vector[1] - normal[1];
              axis_vector[2] = axis_vector[2] - normal[2];
            }

            normalize3d(axis_vector);
            {
              float k = normal[0] * axis_vector[1] - normal[1] * axis_vector[0];
              float j = normal[2] * axis_vector[0] - normal[0] * axis_vector[2];
              float i = normal[1] * axis_vector[2] - normal[2] * axis_vector[1];

              tangent[0] = i;
              tangent[1] = j;
              tangent[2] = k;
            }
            {
              float k = normal[0] * tangent[1] - normal[1] * tangent[0];
              float j = normal[2] * tangent[0] - normal[0] * tangent[2];
              float i = normal[1] * tangent[2] - normal[2] * tangent[1];

              bitangent[0] = i;
              bitangent[1] = j;
              bitangent[2] = k;
            }

            if (decals_dot3(tangent, tangent) < *(float *)0x253f44 || /* dup-args-ok */
                decals_dot3(bitangent, bitangent) < *(float *)0x253f44) { /* dup-args-ok */
              float reflection_scale = -decals_dot3(velocity, normal);

              axis_vector[0] = reflection_scale * normal[0] + velocity[0];
              axis_vector[1] = reflection_scale * normal[1] + velocity[1];
              axis_vector[2] = reflection_scale * normal[2] + velocity[2];

              projection = projection_from_vector3d(axis_vector);
              sign = (uint8_t)projection_sign_from_vector3d(axis_vector, projection) ? 1.0f : -1.0f;

              switch (projection) {
              case 0:
                axis_vector[0] = sign;
                axis_vector[1] = 0.0f;
                axis_vector[2] = 0.0f;
                break;
              case 1:
                axis_vector[0] = 0.0f;
                axis_vector[1] = sign;
                axis_vector[2] = 0.0f;
                break;
              case 2:
                axis_vector[0] = 0.0f;
                axis_vector[1] = 0.0f;
                axis_vector[2] = sign;
                break;
              default:
                display_assert((char *)0x26a7e4, "c:\\halo\\SOURCE\\effects\\decals.c", 0x868, true);

                system_exit(-1);
                break;
              }

              if (decals_dot3(axis_vector, normal) > *(float *)0x2533c0) {
                axis_vector[0] = axis_vector[0] + normal[0];
                axis_vector[1] = axis_vector[1] + normal[1];
                axis_vector[2] = axis_vector[2] + normal[2];
              } else {
                axis_vector[0] = axis_vector[0] - normal[0];
                axis_vector[1] = axis_vector[1] - normal[1];
                axis_vector[2] = axis_vector[2] - normal[2];
              }

              normalize3d(axis_vector);
              {
                float k = normal[0] * axis_vector[1] - normal[1] * axis_vector[0];
                float j = normal[2] * axis_vector[0] - normal[0] * axis_vector[2];
                float i = normal[1] * axis_vector[2] - normal[2] * axis_vector[1];

                tangent[0] = i;
                tangent[1] = j;
                tangent[2] = k;
              }
              {
                float k = normal[0] * tangent[1] - normal[1] * tangent[0];
                float j = normal[2] * tangent[0] - normal[0] * tangent[2];
                float i = normal[1] * tangent[2] - normal[2] * tangent[1];

                bitangent[0] = i;
                bitangent[1] = j;
                bitangent[2] = k;
              }
            }
          } else {
            {
              float k = normal[0] * velocity[1] - normal[1] * velocity[0];
              float j = normal[2] * velocity[0] - normal[0] * velocity[2];
              float i = normal[1] * velocity[2] - normal[2] * velocity[1];

              tangent[0] = i;
              tangent[1] = j;
              tangent[2] = k;
            }
            {
              float k = normal[0] * tangent[1] - normal[1] * tangent[0];
              float j = normal[2] * tangent[0] - normal[0] * tangent[2];
              float i = normal[1] * tangent[2] - normal[2] * tangent[1];

              bitangent[0] = i;
              bitangent[1] = j;
              bitangent[2] = k;
            }
          }
        } else {
          float angle = decals_random_real(0.0f, 6.2831855f);

          cosine = x87_fcos(angle);
          sine = x87_fsin(angle);
          perpendicular3d(normal, tangent);
          {
            float k = normal[0] * tangent[1] - normal[1] * tangent[0];
            float j = normal[2] * tangent[0] - normal[0] * tangent[2];
            float i = normal[1] * tangent[2] - normal[2] * tangent[1];

            bitangent[0] = i;
            bitangent[1] = j;
            bitangent[2] = k;
          }
        }

        {
          float magnitude = sqrtf(tangent[0] * tangent[0] +
                                  tangent[1] * tangent[1] +
                                  tangent[2] * tangent[2]);

          if (!(*(double *)0x2533d0 > x87_fabs(magnitude))) {
            float inverse = 1.0f / magnitude;

            tangent[0] = inverse * tangent[0];
            tangent[1] = inverse * tangent[1];
            tangent[2] = inverse * tangent[2];
          }
        }

        {
          float magnitude = sqrtf(bitangent[0] * bitangent[0] +
                                  bitangent[1] * bitangent[1] +
                                  bitangent[2] * bitangent[2]);

          if (!(*(double *)0x2533d0 > x87_fabs(magnitude))) {
            float inverse = 1.0f / magnitude;

            bitangent[0] = inverse * bitangent[0];
            bitangent[1] = inverse * bitangent[1];
            bitangent[2] = inverse * bitangent[2];
          }
        }

        basis[1] = cosine * bitangent[0] - tangent[0] * sine;
        basis[2] = cosine * bitangent[1] - tangent[1] * sine;
        basis[3] = cosine * bitangent[2] - tangent[2] * sine;
        basis[4] = tangent[0] * cosine + bitangent[0] * sine;
        basis[5] = tangent[1] * cosine + bitangent[1] * sine;
        basis[6] = tangent[2] * cosine + bitangent[2] * sine;
        *(vector3_t *)&basis[7] = *(vector3_t *)normal;
        *(vector3_t *)&basis[10] = *(vector3_t *)(collision_result + 0xc);

        if (color_index == -1) {
          sequence_index =
            decals_random_short(0, *(int16_t *)(bitmap_tag + 0x54));

          if (sequence_index >= *(int *)(bitmap_tag + 0x54)) {
            error(2, (char *)0x26a788);
            sequence_index = (int16_t)(*(int *)(bitmap_tag + 0x54) - 1);
          }
        } else {
          sequence_index = color_index;
        }

        sprite_index = 0;

        if (scale == *(float *)0x2533c0) {
          scale = 1.0f;
        }

        radius = decals_random_real(*(float *)(decal_tag + 0x18),
                                    *(float *)(decal_tag + 0x1c)) *
                 scale;
      }

      if (*(int16_t *)bitmap_tag == 3) {
        char *sequence = (char *)tag_block_get_element(bitmap_tag + 0x54,
                                                       sequence_index, 0x40);
        int16_t *sprite = (int16_t *)tag_block_get_element(sequence + 0x34,
                                                           sprite_index, 0x20);

        bitmap_index = sprite[0];
        decal_sprite_get_bounds(sprite_bounds, decal_tag, sequence_index, 0,
                                radius, extent);
      } else {
        float aspect = 1.0f;

        bitmap_index = 0;

        if ((*(uint16_t *)decal_tag & 0x100) != 0) {
          char *bitmap =
            (char *)tag_block_get_element(bitmap_tag + 0x60, 0, 0x30);

          aspect = (float)(int)*(int16_t *)(bitmap + 6) /
                   (float)(int)*(int16_t *)(bitmap + 4);
        }

        extent[0] = -radius;
        extent[1] = radius;
        extent[2] = -(aspect * radius);
        extent[3] = aspect * radius;
        sprite_bounds[0] = sprite_bounds[2] = 0.0f;
        sprite_bounds[1] = sprite_bounds[3] = 1.0f;
      }

      if (!randomize) {
        if (!xbox_texture_cache_get_hardware_format(
              tag_block_get_element(bitmap_tag + 0x60, bitmap_index, 0x30), 0,
              true)) {
          return;
        }
      }

      {
        float projection[35];
        int surface_queue[0x400];
        int deviant_surface_list[0x400];
        int deviant_surface_bunch[0x400];
        s_decal_staged_vertex render_vertices[0x400];
        int16_t queue_write_index;
        float normal_bounds[6];

        decal_projection_create(extent, projection, basis);
        normal_bounds[0] = basis[7];
        normal_bounds[1] = basis[7];
        normal_bounds[2] = basis[8];
        normal_bounds[3] = basis[8];
        normal_bounds[4] = basis[9];
        normal_bounds[5] = basis[9];
        *(int16_t *)0x4547da = 0;
        *(int16_t *)0x453fd8 = 0;
        surface_queue[0] = *(int *)((char *)collision_result + 0x44);
        queue_write_index = 1;

        while (queue_read_index < queue_write_index) {
          if (queue_read_index >= 0x400) {
            decals_assert_or_exit((char *)0x26a74c, 0x931);
          }

          if (queue_write_index > 0x400) {
            decals_assert_or_exit((char *)0x26a710, 0x932);
          }

          decal_clip_to_surface((void *)0x44dfd8, projection,
                                surface_queue[queue_read_index++], true, radius,
                                *(int16_t *)(decal_tag + 2), surface_queue,
                                &queue_write_index, deviant_surface_list,
                                &deviant_surface_count);
        }

        if (*(uint8_t *)(0x269d8c + ((int)*(int16_t *)(decal_tag + 2) << 4)) !=
              0 &&
            deviant_surface_count > 0) {
          int16_t remaining_deviant_surface_count = deviant_surface_count;

          while (remaining_deviant_surface_count > 0) {
            int16_t bunch_size = 0;
            int16_t deviant_surface_index;

            for (deviant_surface_index = 0;
                 !bunch_size && deviant_surface_index < deviant_surface_count;
                 deviant_surface_index++) {
              int deviant_surface = deviant_surface_list[deviant_surface_index];

              if (deviant_surface != -1) {
                int *surface = (int *)tag_block_get_element(
                  (char *)structure_bsp + 0x3c, deviant_surface, 0xc);
                float surface_plane[4];
                int16_t next_index;

                {
                  int designator = surface[0];
                  real_plane3d *plane = (real_plane3d *)tag_block_get_element(
                    (char *)structure_bsp + 0xc, designator & 0x7fffffff, 0x10);

                  if (designator < 0) {
                    surface_plane[0] = -plane->normal[0];
                    surface_plane[1] = -plane->normal[1];
                    surface_plane[2] = -plane->normal[2];
                    surface_plane[3] = -plane->d;
                  } else {
                    *(real_plane3d *)surface_plane = *plane;
                  }
                }

                if (bunch_size >= 0x400) {
                  decals_assert_or_exit((char *)0x26a6d4, 0x95f);
                }

                deviant_surface_bunch[bunch_size++] = deviant_surface;
                deviant_surface_list[deviant_surface_index] = -1;

                for (next_index = (int16_t)(deviant_surface_index + 1);
                     next_index < deviant_surface_count; next_index++) {
                  int next_surface_index = deviant_surface_list[next_index];

                  if (next_surface_index != -1) {
                    int *next_surface = (int *)tag_block_get_element(
                      (char *)structure_bsp + 0x3c, next_surface_index, 0xc);
                    float next_plane[4];
                    float angle;

                    bsp3d_get_plane_from_designator(structure_bsp,
                                                    (uint32_t)next_surface[0],
                                                    next_plane);

                    angle = angle_between_normals3d(surface_plane, next_plane);
                    if (angle <= *(float *)(0x269d80 +
                                            ((int)*(int16_t *)(decal_tag + 2)
                                             << 4)) *
                                   *(float *)0x253d4c) {
                      if (bunch_size >= 0x400) {
                        decals_assert_or_exit((char *)0x26a6d4, 0x976);
                      }

                      deviant_surface_bunch[bunch_size++] = next_surface_index;
                      deviant_surface_list[next_index] = -1;
                    }
                  }
                }

                {
                  int closest_surface_index = -1;
                  float closest_minimum_distance;
                  float closest_maximum_distance;
                  float closest_plane[4];
                  vector3_t closest_edge_start;
                  vector3_t closest_edge_end;
                  int16_t bunch_index;

                  for (bunch_index = 0; bunch_index < bunch_size;
                       bunch_index++) {
                    int bunch_surface_index = deviant_surface_bunch[bunch_index];
                    int *bunch_surface = (int *)tag_block_get_element(
                      (char *)structure_bsp + 0x3c, bunch_surface_index, 0xc);
                    int edge_index = bunch_surface[1];

                    do {
                      int *edge = (int *)tag_block_get_element(
                        (char *)structure_bsp + 0x48, edge_index, 0x18);
                      bool surface_on_right = edge[5] == bunch_surface_index;
                      float *edge_start = (float *)tag_block_get_element(
                        (char *)structure_bsp + 0x54, edge[!surface_on_right],
                        0x10);
                      float *edge_end = (float *)tag_block_get_element(
                        (char *)structure_bsp + 0x54, edge[surface_on_right],
                        0x10);
                      float minimum_distance = x87_fabs(
                        decals_dot3(edge_start, &projection[17]) - projection[20]);
                      float maximum_distance = x87_fabs(
                        decals_dot3(edge_end, &projection[17]) - projection[20]);

                      if (minimum_distance > maximum_distance) {
                        float swap_distance = minimum_distance;

                        minimum_distance = maximum_distance;
                        maximum_distance = swap_distance;
                      }

                      if (closest_surface_index == -1 ||
                          (minimum_distance <= closest_minimum_distance &&
                           maximum_distance <= closest_maximum_distance)) {
                        bsp3d_get_plane_from_designator(
                          structure_bsp, (uint32_t)bunch_surface[0],
                          closest_plane);
                        closest_minimum_distance = minimum_distance;
                        closest_maximum_distance = maximum_distance;
                        closest_edge_start = *(vector3_t *)edge_start;
                        closest_edge_end = *(vector3_t *)edge_end;
                        closest_surface_index = bunch_surface_index;
                      }

                      edge_index = edge[2 + surface_on_right];
                    } while (edge_index != bunch_surface[1]);
                  }

                  if (closest_surface_index == -1) {
                    decals_assert_or_exit((char *)0x26a6b8, 0x9bd);
                  }

                  {
                    float wrapped_projection[35];
                    float edge_axis[3];
                    float magnitude;

                    edge_axis[0] = closest_edge_end.x - closest_edge_start.x;
                    edge_axis[1] = closest_edge_end.y - closest_edge_start.y;
                    edge_axis[2] = closest_edge_end.z - closest_edge_start.z;

                    magnitude = sqrtf(edge_axis[0] * edge_axis[0] +
                                      edge_axis[1] * edge_axis[1] +
                                      edge_axis[2] * edge_axis[2]);

                    if (!(*(double *)0x2533d0 > x87_fabs(magnitude))) {
                      float inverse = 1.0f / magnitude;

                      edge_axis[0] = inverse * edge_axis[0];
                      edge_axis[1] = inverse * edge_axis[1];
                      edge_axis[2] = inverse * edge_axis[2];
                    } else {
                      magnitude = 0.0f;
                    }

                    if (magnitude > *(float *)0x2533c0) {
                      float sign;
                      float angle;
                      float rotation[13];
                      float wrapped_basis[13];

                      {
                        float cross[3];
                        float k = closest_plane[0] * projection[18] -
                                  closest_plane[1] * projection[17];
                        float j = closest_plane[2] * projection[17] -
                                  closest_plane[0] * projection[19];
                        float i = closest_plane[1] * projection[19] -
                                  closest_plane[2] * projection[18];

                        cross[0] = i;
                        cross[1] = j;
                        cross[2] = k;
                        sign = decals_dot3(cross, edge_axis) < *(float *)0x2533c0 ?
                                 1.0f :
                                 -1.0f;
                      }

                      angle = angle_between_normals3d(closest_plane,
                                                      &projection[17]) *
                              sign;
                      FUN_001092d0(rotation, edge_axis, x87_fsin(angle),
                                   x87_fcos(angle));

                      wrapped_basis[10] = basis[10] - closest_edge_start.x;
                      wrapped_basis[11] = basis[11] - closest_edge_start.y;
                      wrapped_basis[12] = basis[12] - closest_edge_start.z;
                      matrix_transform_point(rotation, &wrapped_basis[10],
                                             &wrapped_basis[10]); /* dup-args-ok */
                      matrix_transform_vector(rotation, &basis[1],
                                              &wrapped_basis[1]);
                      matrix_transform_vector(rotation, &basis[4],
                                              &wrapped_basis[4]);
                      matrix_transform_vector(rotation, &basis[7],
                                              &wrapped_basis[7]);
                      wrapped_basis[0] = 1.0f;
                      wrapped_basis[10] += closest_edge_start.x;
                      wrapped_basis[11] += closest_edge_start.y;
                      wrapped_basis[12] += closest_edge_start.z;

                      decal_projection_create(extent, wrapped_projection,
                                              wrapped_basis);
                      normal_bounds[0] = wrapped_basis[7] > normal_bounds[0] ?
                                           normal_bounds[0] :
                                           wrapped_basis[7];
                      normal_bounds[1] = wrapped_basis[7] > normal_bounds[1] ?
                                           wrapped_basis[7] :
                                           normal_bounds[1];
                      normal_bounds[2] = wrapped_basis[8] > normal_bounds[2] ?
                                           normal_bounds[2] :
                                           wrapped_basis[8];
                      normal_bounds[3] = wrapped_basis[8] > normal_bounds[3] ?
                                           wrapped_basis[8] :
                                           normal_bounds[3];
                      normal_bounds[4] = wrapped_basis[9] > normal_bounds[4] ?
                                           normal_bounds[4] :
                                           wrapped_basis[9];
                      normal_bounds[5] = wrapped_basis[9] > normal_bounds[5] ?
                                           wrapped_basis[9] :
                                           normal_bounds[5];
                    } else {
                      error(2, (char *)0x26a670);
                      memcpy(wrapped_projection, projection,
                             sizeof(wrapped_projection));
                    }

                    for (bunch_index = 0; bunch_index < bunch_size;
                         bunch_index++) {
                      decal_clip_to_surface((void *)0x44dfd8, wrapped_projection,
                                            deviant_surface_bunch[bunch_index],
                                            false, radius,
                                            *(int16_t *)(decal_tag + 2), NULL,
                                            NULL, NULL, NULL);
                    }
                  }
                }

                remaining_deviant_surface_count -= bunch_size;
              }
            }
          }
        }

        if (*(int16_t *)0x4547da <= 0 || *(int16_t *)0x453fd8 <= 0) {
          return;
        }

        if (*(int16_t *)0x453fd8 > 0x400) {
          if (!g_warned_decal_vertex_overflow) {
            error(2,
                  "### ERROR decals: vertex overflow (count=%d) -- skipping decal",
                  *(int16_t *)0x453fd8);
            g_warned_decal_vertex_overflow = true;
          }
          return;
        }

        {
          float offset[3];
          int16_t decal_quad_count = 0;
          int16_t decal_surface_index;
          int cache_size;
          int cache_index;

          offset[0] = 0.0f;
          offset[1] = 0.0f;
          offset[2] = 0.0f;

          if (normal_bounds[1] - normal_bounds[0] <= *(float *)0x253398 &&
              normal_bounds[3] - normal_bounds[2] <= *(float *)0x253398 &&
              normal_bounds[5] - normal_bounds[4] <= *(float *)0x253398) {
            offset[0] = normal_bounds[0] + normal_bounds[1];
            offset[1] = normal_bounds[2] + normal_bounds[3];
            offset[2] = normal_bounds[4] + normal_bounds[5];
            normalize3d(offset);
            offset[0] = offset[0] * *(float *)0x325710;
            offset[1] = offset[1] * *(float *)0x325710;
            offset[2] = offset[2] * *(float *)0x325710;
          }

          for (decal_surface_index = 0;
               decal_surface_index < *(int16_t *)0x4547da;
               decal_surface_index++) {
            if (((int16_t *)0x453fda)[decal_surface_index] < 3) {
              decals_assert_or_exit((char *)0x26a608, 0xa5a);
            }

            decal_quad_count +=
              (int16_t)((((int16_t *)0x453fda)[decal_surface_index] - 1) / 2);
          }

          if (decal_quad_count < 0 || decal_quad_count > 0x400) {
            if (!g_warned_decal_quad_overflow) {
              error(2,
                    "### ERROR decals: quad overflow (count=%d) -- skipping decal",
                    decal_quad_count);
              g_warned_decal_quad_overflow = true;
            }
            return;
          }

          cache_size = decal_quad_count << 6;
          cache_index = FUN_0017cae0(cache_size);

          if (cache_index != -1) {
            int decal_index = decal_insert(cache_index, collision_result[8],
                                           *(int16_t *)(decal_tag + 4), -1,
                                           randomize);

            if (decal_index != -1) {
              char *decal = (char *)datum_get(global_decal_data, decal_index);
              s_decal_cached_quad *quads =
                (s_decal_cached_quad *)FUN_0017caf0(cache_index, cache_size);

              if (quads != NULL) {
                int16_t decal_vertex_index;

                for (decal_vertex_index = 0;
                     decal_vertex_index < *(int16_t *)0x453fd8;
                     decal_vertex_index++) {
                  s_decal_geometry_vertex *decal_vertex =
                    &((s_decal_geometry_vertex *)0x44dfd8)[decal_vertex_index];
                  float texcoord[2];
                  float texcoord_raw;
                  int u;
                  int v;

                  texcoord_raw =
                    (sprite_bounds[1] - sprite_bounds[0]) * decal_vertex->uv[0] +
                    sprite_bounds[0];
                  texcoord[0] = texcoord_raw < *(float *)0x2533c0 ?
                                  *(float *)0x2533c0 :
                                texcoord_raw > *(float *)0x2533c8 ?
                                  *(float *)0x2533c8 :
                                  texcoord_raw;
                  texcoord_raw =
                    (sprite_bounds[3] - sprite_bounds[2]) * decal_vertex->uv[1] +
                    sprite_bounds[2];
                  texcoord[1] = texcoord_raw < *(float *)0x2533c0 ?
                                  *(float *)0x2533c0 :
                                texcoord_raw > *(float *)0x2533c8 ?
                                  *(float *)0x2533c8 :
                                  texcoord_raw;
                  texcoord[0] *= *(float *)0x26a604;
                  texcoord[0] = texcoord[0] < *(float *)0x2533c0 ?
                                  *(float *)0x2533c0 :
                                texcoord[0] > *(float *)0x26a600 ?
                                  *(float *)0x26a600 :
                                  texcoord[0];
                  u = x87_round_to_int(
                    (float)floor(texcoord[0] + *(float *)0x253398));
                  texcoord[1] *= *(float *)0x26a604;
                  texcoord[1] = texcoord[1] < *(float *)0x2533c0 ?
                                  *(float *)0x2533c0 :
                                texcoord[1] > *(float *)0x26a600 ?
                                  *(float *)0x26a600 :
                                  texcoord[1];
                  v = x87_round_to_int(
                    (float)floor(texcoord[1] + *(float *)0x253398));

                  if ((u & 0x8000) != 0 || (v & 0x8000) != 0) {
                    decals_assert_or_exit((char *)0x26a5e0, 0xa88);
                  }

                  render_vertices[decal_vertex_index].uv[0] = (int16_t)u;
                  render_vertices[decal_vertex_index].uv[1] = (int16_t)v;
                  render_vertices[decal_vertex_index].position[0] =
                    offset[0] + decal_vertex->position[0];
                  render_vertices[decal_vertex_index].position[1] =
                    offset[1] + decal_vertex->position[1];
                  render_vertices[decal_vertex_index].position[2] =
                    offset[2] + decal_vertex->position[2];
                }

                *(vector3_t *)(decal + 8) =
                  *(vector3_t *)(collision_result + 0xc);
                *(int *)(decal + 0x14) = game_time_get();
                *(uint8_t *)(decal + 0x18) = (uint8_t)sequence_index;
                *(uint8_t *)(decal + 0x1a) = 0;
                *(uint8_t *)(decal + 0x1b) = (uint8_t)bitmap_index;
                *(float *)(decal + 0x1c) =
                  decals_random_real(*(float *)(decal_tag + 0x78),
                                     *(float *)(decal_tag + 0x7c));
                *(float *)(decal + 0x20) =
                  decals_random_real(*(float *)(decal_tag + 0x80),
                                     *(float *)(decal_tag + 0x84));
                *(int *)(decal + 0x2c) = decal_tag_index;
                *(int16_t *)(decal + 0x2a) = decal_quad_count;

                {
                  float intensity =
                    decals_random_real(*(float *)(decal_tag + 0x2c),
                                       *(float *)(decal_tag + 0x30));
                  float color[3];
                  float interpolation = decals_random_real(0.0f, 1.0f);

                  rgb_colors_interpolate(color, (*(uint8_t *)decal_tag >> 1) & 3,
                               (float *)(decal_tag + 0x34),
                               (float *)(decal_tag + 0x40), interpolation);
                  *(uint32_t *)(decal + 0x24) =
                    real_a_rgb_color_to_pixel32(intensity, color);
                  *(uint8_t *)(decal + 0x28) = 0xff;
                }

                {
                  s_decal_cached_quad *quad = quads;
                  int16_t quad_index = 0;
                  int16_t vertex_base = 0;

                  for (decal_surface_index = 0;
                       decal_surface_index < *(int16_t *)0x4547da;
                       decal_surface_index++) {
                    int16_t surface_vertex_count =
                      ((int16_t *)0x453fda)[decal_surface_index];
                    int16_t surface_vertex_index;

                    if (surface_vertex_count < 3) {
                      decals_assert_or_exit((char *)0x26a5a4, 0xab8);
                    }

                    for (surface_vertex_index = 1;
                         surface_vertex_index + 1 < surface_vertex_count;
                         surface_vertex_index += 2) {
                      if (!(surface_vertex_index + 1 < surface_vertex_count)) {
                        decals_assert_or_exit((char *)0x26a56c, 0xabc);
                      }

                      quad->vertices[0] = render_vertices[vertex_base];
                      quad->vertices[1] =
                        render_vertices[vertex_base + surface_vertex_index];
                      quad->vertices[2] =
                        render_vertices[vertex_base + surface_vertex_index + 1];
                      quad->vertices[3] =
                        render_vertices[surface_vertex_index + 2 >=
                                            surface_vertex_count ?
                                          vertex_base :
                                          vertex_base + surface_vertex_index + 2];
                      quad++;
                      quad_index++;
                    }

                    vertex_base += surface_vertex_count;
                  }

                  if (quad_index != decal_quad_count) {
                    decals_assert_or_exit((char *)0x26a54c, 0xacc);
                  }
                }

                thunk_FUN_0015b960();
              } else {
                FUN_0017cb10(cache_index);

                if (*(uint8_t *)0x5aa8b4 != 0) {
                  error(2, (char *)0x26a524);
                }

                return;
              }
            } else {
              FUN_0017cb10(cache_index);

              if (*(uint8_t *)0x5aa8b4 != 0) {
                error(2, (char *)0x26a4e0, decal_globals->locked_count,
                      decal_globals->permanent_count);
              }

              return;
            }
          } else {
            if (*(uint8_t *)0x5aa8b4 != 0) {
              error(2, (char *)0x26a498, decal_globals->locked_count,
                    decal_globals->permanent_count);
            }

            return;
          }
        }
      }
    } else {
      int bitmap_tag_index = *(int *)(decal_tag + 0xe4);
      const char *decal_name = tag_get_name(decal_tag_index);
      const char *bitmap_name = tag_get_name(bitmap_tag_index);
      decals_log_invalid_decal_type_once(*(int16_t *)(decal_tag + 2),
                                         decal_tag_index, decal_name,
                                         bitmap_tag_index, bitmap_name,
                                         "decal_new");
      /* Project guard (not in the binary): the skipped tag computed no
       * radius/sprite/basis, so the next tag must not reuse them. */
      reuse_previous = false;
      decal_tag_index = *(int *)(decal_tag + 0x14);
      continue;
    }

    reuse_previous = (*(uint8_t *)decal_tag & 1) != 0;
    decal_tag_index = *(int *)(decal_tag + 0x14);
  }
}

void decal_new(int decal_tag_index, void *origin, void *direction,
                  float scale, bool randomize, int16_t color_index, int flags)
{
  if (*(uint8_t *)0x2eebd0 != 0) {
    uint32_t *local_random_seed_address = random_math_get_local_seed_address();
    uint32_t local_seed = 0;
    int16_t collision_result[40];

    if (randomize) {
      if (local_random_seed_address == NULL) {
        display_assert("local_random_seed_address",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x5fd, true);
        system_exit(-1);
      }

      if (origin == NULL) {
        display_assert("origin", "c:\\halo\\SOURCE\\effects\\decals.c", 0x5fe,
                       true);
        system_exit(-1);
      }

      local_seed = *local_random_seed_address;
      *local_random_seed_address = ((uint32_t *)origin)[2] ^
                                   ((uint32_t *)origin)[1] ^
                                   ((uint32_t *)origin)[0] ^ 0xdeadc0de;
    }

    if (*(int16_t *)0x4761d8 >= 0x20) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x60e, true);
      system_exit(-1);
    }

    {
      short depth = *(short *)0x4761d8;
      (*(short *)0x4761d8)++;
      *(short *)(0x5a8c80 + (int)depth * 2) = 9;
    }

    if (FUN_0014df70(0x100061, (float *)origin, (float *)direction, -1,
                     collision_result) &&
        collision_result[0] != 0 && collision_result[0] == 2) {
      uint8_t *decal_tag = (uint8_t *)tag_get(0x64656361, decal_tag_index);
      if ((decal_tag[0] & 0x10) == 0) {
        decal_new_from_collision(decal_tag_index, collision_result, direction,
                                 scale, randomize, color_index, flags);
      }
    }

    if (*(short *)0x4761d8 <= 1) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\effects\\decals.c", 0x632, true);
      system_exit(-1);
    }

    --*(short *)0x4761d8;

    if (randomize) {
      if (local_random_seed_address == NULL) {
        display_assert("local_random_seed_address",
                       "c:\\halo\\SOURCE\\effects\\decals.c", 0x636, true);
        system_exit(-1);
      }

      *local_random_seed_address = local_seed;
    }
  }
}

/* Tail-call thunk to rasterizer decal initialization (__rasterizer_debug_immediate_line_screenspace).
 * Inherits the caller's pushed args and forwards them unchanged (cdecl). */
void FUN_0017ca50(short *p0, short *p1, float *color0, float *color1)
{
  __rasterizer_debug_immediate_line_screenspace(p0, p1, color0, color1);
}

/* Tail-call thunk to rasterizer debug 2D polyline drawer (__rasterizer_debug_immediate_linestrip_screenspace).
 * 0x17ca60: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x15acc0 — forwards 3 stack
 * args: [EBP+8]=points (short[2] array), [EBP+C]=point_count (int16_t),
 * [EBP+10]=color (real_rgb_color *). */
void FUN_0017ca60(short *points, int16_t point_count, float *color)
{
  __rasterizer_debug_immediate_linestrip_screenspace(points, point_count, color);
}

/* Tail-call thunk to rasterizer decal setup (__rasterizer_debug_immediate_end_screenspace). */
void FUN_0017ca70(void)
{
  __rasterizer_debug_immediate_end_screenspace();
}

/* Tail-call thunk to __rasterizer_decals_initialize (0x15b6d0). */
void thunk_rasterizer_decals_initialize(void)
{
  __rasterizer_decals_initialize();
}

/* Tail-call thunk to __rasterizer_decals_initialize_for_new_map (0x15b190). */
void thunk_rasterizer_decals_initialize_for_new_map(void)
{
  __rasterizer_decals_initialize_for_new_map();
}

/* Tail-call thunk to __rasterizer_decals_dispose_from_old_map (0x15b1a0). */
void thunk_rasterizer_decals_dispose_from_old_map(void)
{
  __rasterizer_decals_dispose_from_old_map();
}

/* Tail-call thunk to rasterizer decal (__rasterizer_decals_flush). */
void FUN_0017cac0(void)
{
  __rasterizer_decals_flush();
}

/* Tail-call thunk to rasterizer_decals_dispose (0x15b7e0). */
void thunk_rasterizer_decals_dispose(void)
{
  rasterizer_decals_dispose();
}

int FUN_0017cae0(uint32_t cache_size)
{
  return __rasterizer_decal_vertices_new(cache_size);
}

void *FUN_0017caf0(int cache_index, uint32_t cache_size)
{
  return __rasterizer_decal_vertices_lock(cache_index, cache_size);
}

void thunk_FUN_0015b960(void)
{
  __rasterizer_decal_vertices_unlock();
}

void FUN_0017cb10(int decal_index)
{
  __rasterizer_decal_vertices_delete(decal_index);
}

/* Tail-call thunk to decal rendering pass setup (__rasterizer_decals_begin).
 * pass_index selects the rendering pass type. */
void FUN_0017cb20(short pass_index)
{
  __rasterizer_decals_begin(pass_index);
}

/* Tail-call thunk to per-cluster decal rendering (__rasterizer_decals_draw).
 * rendered_cluster_data is a pointer to the cluster render data. */
void FUN_0017cb30(int rendered_cluster_data)
{
  __rasterizer_decals_draw(rendered_cluster_data);
}

/* Tail-call thunk to rasterizer decal geometry (__rasterizer_decals_end). */
void FUN_0017cb40(void)
{
  __rasterizer_decals_end();
}

/* Tail-call thunk to rasterizer decal geometry (__rasterizer_detail_objects_begin). */
void FUN_0017cb50(void)
{
  __rasterizer_detail_objects_begin();
}

/* Tail-call thunk to rasterizer decal geometry initialization (__rasterizer_detail_objects_rebuild_vertices).
 * 0x17cb60: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x15c980 — forwards 1 stack
 * arg: [EBP+8]=decal_group ptr (MOV ESI,[EBP+8] at 0x15c9aa). */
void FUN_0017cb60(void *decal_group)
{
  __rasterizer_detail_objects_rebuild_vertices(decal_group);
}

/* Tail-call thunk to rasterizer decal geometry disposal (__rasterizer_detail_objects_draw).
 * 0x17cb70: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x15cbb0 — forwards 1 stack
 * arg: [EBP+8]=decal_group ptr (MOV ESI,[EBP+8] at 0x15cbe1). */
void FUN_0017cb70(void *decal_group)
{
  __rasterizer_detail_objects_draw(decal_group);
}

/* Tail-call thunk to rasterizer decal geometry (__rasterizer_detail_objects_end). */
void FUN_0017cb80(void)
{
  __rasterizer_detail_objects_end();
}

/* Tail-call thunk to rasterizer decal rendering (__rasterizer_screen_effect).
 * 0x17cb90: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x170c90 — forwards 1 stack
 * arg: [EBP+8]=decal ptr (MOV EAX,[EBP+8] at 0x170ccb; passed to 0x17dc70). */
void FUN_0017cb90(void *decal)
{
  __rasterizer_screen_effect(decal);
}

/* Tail-call thunk to dynamic vertex geometry decal flush (__rasterizer_model_begin). */
void FUN_0017cbb0(void *param_1, int param_2)
{
  __rasterizer_model_begin(param_1, param_2);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (__rasterizer_model_draw).
 */
void FUN_0017cbc0(int shader, int p2, int p3, int widget_handle, int p5, int p6,
                  int zbuf_handle)
{
  __rasterizer_model_draw(shader, p2, p3, widget_handle, p5, p6, zbuf_handle);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (__rasterizer_model_transparent_geometry_submit).
 */
void FUN_0017cbd0(void *shader, short p2, int p3, int widget_handle, int p5,
                  int p6, int zbuf_handle, float *position, void *p9)
{
  __rasterizer_model_transparent_geometry_submit(shader, p2, p3, widget_handle, p5, p6, zbuf_handle, position,
               p9);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (__rasterizer_environment_lightmap_begin).
 */
void FUN_0017cc10(int param_1)
{
  __rasterizer_environment_lightmap_begin(param_1);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (__rasterizer_environment_lightmap_draw).
 */
void FUN_0017cc20(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  __rasterizer_environment_lightmap_draw((void *)param_1, param_2, param_3, param_4, param_5,
               (void *)param_6);
}

/* Tail-call thunk to rasterizer_xbox_environment gel-light setup
 * (__rasterizer_environment_diffuse_light_begin).
 * 0x17cc60: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x1621c0 — ESP is unchanged
 * at the JMP, so the caller's single pushed dword stays in place and becomes
 * __rasterizer_environment_diffuse_light_begin's first cdecl argument ([ESP+4] / in_stack_00000004). That
 * callee asserts on it with
 * "light_index>=0 && light_index<rasterizer_lights.light_count"
 * (rasterizer_xbox_environment.c:0x2d9) and indexes rasterizer_lights with
 * light_index*0x38, so the forwarded dword is a light index, not a handle.
 * Sole XBE caller: structure_render_diffuse_light @0x196117. */
void FUN_0017cc60(int light_index)
{
  __rasterizer_environment_diffuse_light_begin(light_index);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (__rasterizer_environment_diffuse_light_draw).
 */
void FUN_0017cc70(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  __rasterizer_environment_diffuse_light_draw((void *)param_1, param_2, param_3, param_4, param_5,
               (void *)param_6);
}

/* Tail-call thunk to rasterizer shadow-pass begin (__rasterizer_environment_shadow_begin).
 * 0x17ccb0: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x172a30 — forwards 5 stack
 * args (ADD ESP,0x14 at 0x18b928): [EBP+8]=param_1 (unused), [EBP+C]=shadow
 * matrix ptr (MOV ESI,[EBP+C] at 0x172a81), [EBP+10]=shadow color ptr
 * (MOV EBX,[EBP+10]), [EBP+14]=object_bounding_radius (FLD [EBP+14] at
 * 0x172b88), [EBP+18]=out_radius. Returns __rasterizer_environment_shadow_begin's char (drawn/visible
 * flag) — the caller chain FUN_0018b830 -> FUN_0018c100 tests AL after the
 * call (implicit-EAX propagation in the original; made explicit here). */
char FUN_0017ccb0(int param_1, const float *shadow_matrix,
                  const float *shadow_color, float object_bounding_radius,
                  float *out_radius)
{
  return __rasterizer_environment_shadow_begin(param_1, shadow_matrix, shadow_color,
                      object_bounding_radius, out_radius);
}

/* Tail-call thunk to rasterizer decal rendering (__rasterizer_environment_shadow_model_begin).
 * Inherits the caller's pushed arg and forwards it unchanged (cdecl). */
void FUN_0017ccc0(int param_1)
{
  __rasterizer_environment_shadow_model_begin(param_1);
}

/* Tail-call thunk to rasterizer decal rendering (__rasterizer_environment_shadow_model_draw).
 * 0x17ccd0: PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0x172de0 — forwards 4 stack
 * args (ADD ESP,0x10 at 0x173041): [EBP+8]=decal ptr (MOV ESI,[EBP+8]),
 * [EBP+C]=param_2 (pushed @0x172f20), [EBP+10]=param_3 (MOV EBX,[EBP+10]),
 * [EBP+14]=param_4 (MOV EDI,[EBP+14]). */
void FUN_0017ccd0(void *decal, int param_2, void *param_3, void *param_4)
{
  __rasterizer_environment_shadow_model_draw(decal, param_2, param_3, param_4);
}

/* Tail-call thunk to rasterizer decal rendering (__rasterizer_environment_shadow_draw). */
void FUN_0017ccf0(void *shader, int param_2, int vertices_per_primitive, int a2,
                  int triangle_count, void *vertex_buffer)
{
  __rasterizer_environment_shadow_draw(shader, param_2, vertices_per_primitive, a2, triangle_count,
               vertex_buffer);
}

/* Tail-call thunk to rasterizer dynamic vertex geometry decal (__rasterizer_environment_diffuse_texture_draw).
 */
void FUN_0017cd30(int param_1, int param_2, int param_3, int param_4,
                  int param_5, int param_6)
{
  __rasterizer_environment_diffuse_texture_draw(param_1, param_2, param_3, param_4, param_5, param_6);
}
