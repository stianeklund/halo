extern float normalize3d(float *v);
extern void vector3d_scale_add(float *base, float *direction, float scale,
                               float *out);
extern void matrix_transform_point(float *matrix, float *in, float *out);
extern void perpendicular3d(float *in, float *out);
extern void angles_to_vector(float *out, float *angles);

extern void object_placement_data_new(void *placement, int tag_index,
                                      int parent_handle);
extern void matrix_inverse(float *src, float *dst);
extern void matrix4x3_multiply(float *a, float *b, float *out);
extern void matrix_from_forward_and_up(float *out, float *forward, float *up);
extern int16_t convex_polygon2d_clip_to_plane(int16_t count, float *points,
                                              float *line, int16_t max_count,
                                              float *out_points,
                                              uint32_t *out_bitmask,
                                              uint8_t *changed, float epsilon);

extern void *csstrncpy(char *destination, const char *source, size_t size);
extern char *csstrtok(char *string, const char *delimiters);
extern void set_random_seed(int seed);
extern float random_math_real(unsigned int *seed);

extern float magnitude3d(float *v);
extern float FUN_00013070(float *a, float *b);
extern void points_interpolate(float *a, float *b, float blend, float *out);
extern void scalars_interpolate(float a, float b, float blend, float *out);
extern void scalars_interpolate_and_clamp_0_to_1(float a, float b, float t,
                                                 float *out);
extern uint32_t FUN_000d1c90(float *color);
extern void FUN_001bd5f0(void);
extern void *csmemset(void *buffer, int c, size_t size);
extern void crc_new(uint32_t *checksum);
extern void crc_checksum_buffer(uint32_t *checksum, void *data, int size);


static int check(const char *name, uint32_t got, uint32_t expected, char *buf)
{
  if (got == expected) {
    crt_sprintf(buf, "ASSERT|PASS|%s|got=%08X|expected=%08X\n", name, got,
                expected);
    debug_string_to_display(buf, 0);
    return 1;
  }
  crt_sprintf(buf, "ASSERT|FAIL|%s|got=%08X|expected=%08X\n", name, got,
              expected);
  debug_string_to_display(buf, 0);
  return 0;
}

static void dump_clip_case(const char *name, int16_t ret, uint32_t mask,
                           uint8_t changed, float *points, char *buf)
{
  int i;
  int point_count;

  crt_sprintf(buf, "CASE|BEGIN|clip|%s\n", name);
  debug_string_to_display(buf, 0);

  crt_sprintf(buf, "VALUE|%s.ret|value=%04X|changed=%02X|mask=%08X\n", name,
              (uint16_t)ret, changed, mask);
  debug_string_to_display(buf, 0);

  point_count = ret;
  if (point_count < 0) {
    point_count = 0;
  }

  for (i = 0; i < point_count; i++) {
    crt_sprintf(buf, "VALUE|%s.p%d|x=%08X|y=%08X\n", name, i,
                *(uint32_t *)&points[i * 2], *(uint32_t *)&points[i * 2 + 1]);
    debug_string_to_display(buf, 0);
  }

  crt_sprintf(buf, "CASE|END|clip|%s|PASS\n", name);
  debug_string_to_display(buf, 0);
}

static void dump_float_case(const char *group, const char *name, float *values,
                            int value_count, char *buf)
{
  int i;

  crt_sprintf(buf, "CASE|BEGIN|%s|%s\n", group, name);
  debug_string_to_display(buf, 0);

  for (i = 0; i < value_count; i++) {
    crt_sprintf(buf, "VALUE|%s.v%d|value=%08X\n", name, i,
                *(uint32_t *)&values[i]);
    debug_string_to_display(buf, 0);
  }

  crt_sprintf(buf, "CASE|END|%s|%s|PASS\n", group, name);
  debug_string_to_display(buf, 0);
}

static void dump_u32_case(const char *group, const char *name,
                          uint32_t *values, int value_count, char *buf)
{
  int i;

  crt_sprintf(buf, "CASE|BEGIN|%s|%s\n", group, name);
  debug_string_to_display(buf, 0);

  for (i = 0; i < value_count; i++) {
    crt_sprintf(buf, "VALUE|%s.v%d|value=%08X\n", name, i, values[i]);
    debug_string_to_display(buf, 0);
  }

  crt_sprintf(buf, "CASE|END|%s|%s|PASS\n", group, name);
  debug_string_to_display(buf, 0);
}

void run_tests(void)
{
  char buf[128];
  int passed = 0, total = 0;

  debug_string_to_display("RUN|BEGIN|suite=xbox_harness\n", 0);

  /* normalize3d: magnitude and resulting unit vector */
  {
    float v[3] = { 10.5f, -4.2f, 3.14f };
    float mag = normalize3d(v);

    total += 4;
    passed += check("normalize3d mag", *(uint32_t *)&mag, 0x413BC96E, buf);
    passed += check("normalize3d x", *(uint32_t *)&v[0], 0x3F650690, buf);
    passed += check("normalize3d y", *(uint32_t *)&v[1], 0xBEB73873, buf);
    passed += check("normalize3d z", *(uint32_t *)&v[2], 0x3E88FAA9, buf);
  }

  {
    float v[3] = { -100.0f, 0.25f, 50.0f };
    float dump_values[4];
    float mag = normalize3d(v);

    dump_values[0] = mag;
    dump_values[1] = v[0];
    dump_values[2] = v[1];
    dump_values[3] = v[2];
    dump_float_case("normalize3d", "normalize3d_large", dump_values, 4, buf);
  }

  /* vector3d_scale_add */
  {
    float base[3] = { 1.5f, 2.0f, -3.2f };
    float dir[3] = { 0.5f, -1.0f, 2.0f };
    float out[3];

    vector3d_scale_add(base, dir, 2.5f, out);

    total += 3;
    passed += check("v3d_scale_add x", *(uint32_t *)&out[0], 0x40300000, buf);
    passed += check("v3d_scale_add y", *(uint32_t *)&out[1], 0xBF000000, buf);
    passed += check("v3d_scale_add z", *(uint32_t *)&out[2], 0x3FE66666, buf);
  }

  /* matrix_transform_point: layout is [scale, 3x3-rotation-row-major, tx, ty, tz] = 13 floats */
  {
    float mat[13] = { 1.0f,               /* scale (1 = no scaling) */
                      1.0f, 0.0f, 0.0f,   /* row 0 of 3x3 rotation */
                      0.0f, 1.0f, 0.0f,   /* row 1 */
                      0.0f, 0.0f, 1.0f,   /* row 2 */
                      10.0f, 20.0f, 30.0f /* translation */};
    float in_point[3] = { 5.0f, 5.0f, 5.0f };
    float out_point[3];

    matrix_transform_point(mat, in_point, out_point);

    total += 3;
    /* identity rotation + translation [10,20,30] applied to [5,5,5] = [15,25,35] */
    passed +=
      check("mat_transform_pt x", *(uint32_t *)&out_point[0], 0x41700000, buf);
    passed +=
      check("mat_transform_pt y", *(uint32_t *)&out_point[1], 0x41C80000, buf);
    passed +=
      check("mat_transform_pt z", *(uint32_t *)&out_point[2], 0x420C0000, buf);
  }

  {
    float mat[13] = { 2.0f,
                      0.0f, -1.0f, 0.0f,
                      1.0f,  0.0f, 0.0f,
                      0.0f,  0.0f, 1.0f,
                      -3.0f, 4.0f, 5.0f };
    float in_point[3] = { 2.0f, -1.0f, 0.5f };
    float out_point[3];

    matrix_transform_point(mat, in_point, out_point);
    dump_float_case("matrix_transform_point", "mat_transform_rot_scale",
                    out_point, 3, buf);
  }

  /* perpendicular3d */
  {
    float in_vec[3] = { 1.0f, 0.0f, 0.0f };
    float out_vec[3];

    perpendicular3d(in_vec, out_vec);

    total += 3;
    passed += check("perp3d x", *(uint32_t *)&out_vec[0], 0x80000000, buf);
    passed += check("perp3d y", *(uint32_t *)&out_vec[1], 0x00000000, buf);
    passed += check("perp3d z", *(uint32_t *)&out_vec[2], 0x3F800000, buf);
  }

  /* angles_to_vector */
  {
    float angles[2] = { 0.785398f, 0.785398f };
    float out_vec[3];

    angles_to_vector(out_vec, angles);

    total += 3;
    passed += check("angles2vec x", *(uint32_t *)&out_vec[0], 0x3F000003, buf);
    passed += check("angles2vec y", *(uint32_t *)&out_vec[1], 0x3F000000, buf);
    passed += check("angles2vec z", *(uint32_t *)&out_vec[2], 0x3F3504F1, buf);
  }

  /* object_placement_data_new: Object Placement Init */
  {
    uint8_t placement[0x88];
    int i;
    for (i = 0; i < sizeof(placement); i++) {
      placement[i] = 0xCC;
    }

    object_placement_data_new(placement, 0x1234, -1);

    total += 3;
    passed +=
      check("placement tag", *(uint32_t *)&placement[0x00], 0x00001234, buf);
    passed += check("placement scale x", *(uint32_t *)&placement[0x58],
                    0x3F800000, buf);
    passed += check("placement tail", placement[0x87], 0x0000003F, buf);
  }

  /* Parented placement: only safe when the object data table is initialized
   * (game_initialize → objects_initialize). In TEST_HARNESS mode the table
   * lives at 0x5a8d50 and is NULL until objects_initialize runs; calling
   * datum_absolute_index_to_index with a non-NONE parent on a NULL table
   * causes an ACCESS_VIOLATION. Skip the dump when the table is absent. */
  if (*(void **)0x5a8d50 != 0) {
    uint8_t placement[0x88];
    uint32_t dump_values[5];
    int i;

    for (i = 0; i < sizeof(placement); i++) {
      placement[i] = 0xAA;
    }

    object_placement_data_new(placement, 0x55AA, 0x10203);

    dump_values[0] = *(uint32_t *)&placement[0x00];
    dump_values[1] = *(uint32_t *)&placement[0x04];
    dump_values[2] = *(uint32_t *)&placement[0x58];
    dump_values[3] = *(uint32_t *)&placement[0x7C];
    dump_values[4] = *(uint32_t *)&placement[0x84];
    dump_u32_case("object_placement", "placement_parented", dump_values, 5,
                  buf);
  }

  /* matrix_inverse */
  {
    float src_mat[12] = { 0.0f, -1.0f, 0.0f, 1.0f,  0.0f,  0.0f,
                          0.0f, 0.0f,  1.0f, 10.0f, 20.0f, 30.0f };
    float dst_mat[12];
    int i;
    for (i = 0; i < 12; i++)
      dst_mat[i] = 0.0f;

    matrix_inverse(src_mat, dst_mat);

    total += 3;
    passed += check("mat_inv m00", *(uint32_t *)&dst_mat[0], 0x00000000, buf);
    passed += check("mat_inv m10", *(uint32_t *)&dst_mat[3], 0x00000000, buf);
    passed +=
      check("mat_inv trans_x", *(uint32_t *)&dst_mat[9], 0x00000000, buf);
  }

  {
    float src_mat[12] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f,
                          0.0f, 1.0f, 0.0f, -4.0f, 8.0f, 12.0f };
    float dst_mat[12];

    matrix_inverse(src_mat, dst_mat);
    dump_float_case("matrix_inverse", "mat_inv_rot_trans", dst_mat, 12, buf);
  }

  /* matrix4x3_multiply */
  {
    float mat_a[12] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 1.0f, 2.0f, 3.0f };
    float mat_b[12] = { 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 5.0f,  5.0f, 5.0f };
    float out_mat[12];

    matrix4x3_multiply(mat_a, mat_b, out_mat);

    total += 3;
    passed += check("mat_mul m01", *(uint32_t *)&out_mat[1], 0x00000000, buf);
    passed +=
      check("mat_mul trans_y", *(uint32_t *)&out_mat[10], 0x40E00000, buf);
    passed +=
      check("mat_mul trans_z", *(uint32_t *)&out_mat[11], 0x40400000, buf);
  }

  /* matrix_from_forward_and_up */
  {
    float forward[3] = { 1.0f, 1.0f, 0.0f }; // Will be normalized implicitly?
    float up[3] = { 0.0f, 0.0f, 1.0f };
    float out_mat[12];

    matrix_from_forward_and_up(out_mat, forward, up);

    total += 3;
    passed +=
      check("mat_fwd_up m00", *(uint32_t *)&out_mat[0], 0x3F800000, buf);
    passed +=
      check("mat_fwd_up m01", *(uint32_t *)&out_mat[1], 0x3F800000, buf);
    passed +=
      check("mat_fwd_up m22", *(uint32_t *)&out_mat[8], 0x00000000, buf);
  }

  /* csstrncpy */
  {
    char dst[16];
    int i;
    for (i = 0; i < sizeof(dst); i++)
      dst[i] = 0xCC;

    csstrncpy(dst, "hello world",
              8); // Should copy 8 chars and not null terminate?

    total += 2;
    passed += check("csstrncpy last char", dst[7], 0x0000006F, buf);
    passed += check("csstrncpy next char", (uint8_t)dst[8], 0x000000CC, buf);
  }

  /* csstrtok */
  {
    char str[] = "this,is;a\ttest";
    char *tok1 = csstrtok(str, ",;\t");
    char *tok2 = csstrtok(NULL, ",;\t");

    total += 2;
    passed += check("csstrtok t1", tok1 ? tok1[0] : 0, 0x00000074, buf);
    passed += check("csstrtok t2", tok2 ? tok2[0] : 0, 0x00000069, buf);
  }

  /* random_math_real */
  {
    unsigned int local_seed = 1337;
    set_random_seed(local_seed);

    float r1 = random_math_real(&local_seed);
    float r2 = random_math_real(&local_seed);

    total += 2;
    passed += check("rand r1", *(uint32_t *)&r1, 0x3F4114C1, buf);
    passed += check("rand r2", *(uint32_t *)&r2, 0x3F0CAC8D, buf);
  }

  /* magnitude3d (normalize2d) */
  {
    float v[2] = { 3.0f, 4.0f };
    float mag = magnitude3d(v);
    total += 3;
    passed += check("magnitude3d mag", *(uint32_t *)&mag, 0x40A00000, buf);
    passed += check("magnitude3d x", *(uint32_t *)&v[0], 0x3F19999A, buf);
    passed += check("magnitude3d y", *(uint32_t *)&v[1], 0x3F4CCCCD, buf);
  }

  /* FUN_00013070 (dot_product3d) */
  {
    float a[3] = { 1.5f, -2.0f, 3.0f };
    float b[3] = { -0.5f, 1.5f, 2.0f };
    float dot = FUN_00013070(a, b);
    total += 1;
    passed += check("FUN_00013070 dot", *(uint32_t *)&dot, 0x40100000, buf);
  }

  /* points_interpolate (interpolate_vector3d) */
  {
    float a[3] = { 10.0f, 20.0f, 30.0f };
    float b[3] = { 20.0f, -10.0f, 0.0f };
    float out[3];
    points_interpolate(a, b, 0.25f, out);
    total += 3;
    passed +=
      check("points_interpolate x", *(uint32_t *)&out[0], 0x41480000, buf);
    passed +=
      check("points_interpolate y", *(uint32_t *)&out[1], 0x41480000, buf);
    passed +=
      check("points_interpolate z", *(uint32_t *)&out[2], 0x41B40000, buf);
  }

  {
    float a[3] = { -2.0f, 4.0f, 10.0f };
    float b[3] = { 8.0f, -8.0f, -2.0f };
    float out[3];

    points_interpolate(a, b, 1.25f, out);
    dump_float_case("points_interpolate", "points_interpolate_extrap", out, 3,
                    buf);
  }

  /* scalars_interpolate (interpolate_float) */
  {
    float out;
    scalars_interpolate(10.0f, 20.0f, 0.75f, &out);
    total += 1;
    passed +=
      check("scalars_interpolate out", *(uint32_t *)&out, 0x418C0000, buf);
  }

  {
    float dump_values[2];
    float out_low;
    float out_high;

    scalars_interpolate_and_clamp_0_to_1(-5.0f, 5.0f, -0.25f, &out_low);
    scalars_interpolate_and_clamp_0_to_1(-5.0f, 5.0f, 1.25f, &out_high);
    dump_values[0] = out_low;
    dump_values[1] = out_high;
    dump_float_case("scalars_interpolate", "scalars_interpolate_clamp",
                    dump_values, 2, buf);
  }

  /* FUN_000d1c90 (real_argb_color_to_pixel32) */
  {
    float color[4] = { 0.5f, 1.0f, 0.0f, 0.8f }; /* alpha, red, green, blue */
    uint32_t pixel = FUN_000d1c90(color);
    total += 1;
    passed += check("FUN_000d1c90 pixel", pixel, 0x80FF00CC, buf);
  }

  /* convex_polygon2d_clip_to_plane: 2D polygon clip against a line.
   * Golden/candidate comparison is done by run_golden_tests.py on the raw
   * output below, so these cases print exact hex results instead of PASS/FAIL.
   */
  {
    float line_x_ge_0[3] = { 1.0f, 0.0f, 0.0f };
    float line_y_ge_0[3] = { 0.0f, 1.0f, 0.0f };

    {
      float in_points[8] = {
        -1.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f
      };
      float out_points[0x18];
      uint32_t out_mask = 0x0000000f;
      uint8_t changed = 0xcc;
      int16_t ret;

      ret = convex_polygon2d_clip_to_plane(
        4, in_points, line_x_ge_0, 0xc, out_points, &out_mask, &changed, 0.0f);
      dump_clip_case("clip_cross", ret, out_mask, changed, out_points, buf);
    }

    {
      float alias_points[0x18] = { -2.0f, 2.0f,  2.0f,  2.0f,
                                   2.0f,  -2.0f, -2.0f, -2.0f };
      uint32_t out_mask = 0x00000005;
      uint8_t changed = 0xcc;
      int16_t ret;

      ret = convex_polygon2d_clip_to_plane(
        4, alias_points, line_y_ge_0, 0xc,
        alias_points, /* dup-args-ok: in-place clipping test */
        &out_mask, &changed, 0.0f);
      dump_clip_case("clip_alias", ret, out_mask, changed, alias_points, buf);
    }

    {
      float in_points[6] = { 1.0f, 0.0f, 2.0f, 1.0f, 1.0f, 2.0f };
      float out_points[0x18];
      uint8_t changed = 0xcc;
      int16_t ret;

      ret = convex_polygon2d_clip_to_plane(3, in_points, line_x_ge_0, 0xc,
                                           out_points, NULL, &changed, 0.0f);
      dump_clip_case("clip_inside", ret, 0, changed, out_points, buf);
    }

    {
      float in_points[6] = { -3.0f, -1.0f, -2.0f, -2.0f, -1.0f, -3.0f };
      float out_points[0x18];
      uint8_t changed = 0xcc;
      int16_t ret;

      ret = convex_polygon2d_clip_to_plane(3, in_points, line_x_ge_0, 0xc,
                                           out_points, NULL, &changed, 0.0f);
      dump_clip_case("clip_outside", ret, 0, changed, out_points, buf);
    }

    {
      float in_points[6] = { 0.0f, 0.0f, 2.0f, 0.0f, -1.0f, 1.0f };
      float out_points[0x18];
      uint32_t out_mask = 0x00000007;
      uint8_t changed = 0xcc;
      int16_t ret;

      ret = convex_polygon2d_clip_to_plane(3, in_points, line_x_ge_0, 0xc,
                                           out_points, &out_mask, &changed,
                                           0.0001f);
      dump_clip_case("clip_duplicate", ret, out_mask, changed, out_points, buf);
    }
  }

  /* inflate round-trip: exercises FUN_00116010 / FUN_001160c0 which call
   * FUN_00115ba0 with @<eax>=bb.  A missing @<eax> annotation causes the
   * Huffman tables to be built with a garbage bit-count pointer, corrupting
   * s->trees.bb and s->trees.tb and freezing the game on map selection. */
  {
    /* "HaloInflateTest" compressed with zlib -9 (23 bytes, includes header) */
    static const unsigned char compressed[23] = {
      0x78, 0xda, 0xf3, 0x48, 0xcc, 0xc9, 0xf7, 0xcc,
      0x4b, 0xcb, 0x49, 0x2c, 0x49, 0x0d, 0x49, 0x2d,
      0x2e, 0x01, 0x00, 0x2d, 0xdb, 0x05, 0xe8
    };
    unsigned char out_buf[32];
    int zs[14]; /* z_stream = 0x38 bytes */
    int init_ret, inflate_ret;

    csmemset(zs, 0, sizeof(zs));
    init_ret = FUN_001155c0((int)zs, "1.1.3", 0x38);

    inflate_ret = (int)0xfffffffe; /* Z_STREAM_ERROR fallback */
    if (init_ret == 0) {
      zs[0] = (int)compressed;    /* next_in  */
      zs[1] = sizeof(compressed); /* avail_in */
      zs[3] = (int)out_buf;       /* next_out */
      zs[4] = sizeof(out_buf);    /* avail_out */
      inflate_ret = FUN_001155e0((int)zs, 4 /* Z_FINISH */);
      FUN_00115430((int)zs);
    }

    total += 4;
    passed += check("inflate_init", (uint32_t)init_ret, 0, buf);
    passed += check("inflate_z_stream_end", (uint32_t)inflate_ret, 1, buf);
    /* "Halo" in little-endian = 0x6F6C6148 */
    passed += check("inflate_out[0..3]", *(uint32_t *)&out_buf[0], 0x6F6C6148, buf);
    /* "Infl" in little-endian = 0x6C666E49 */
    passed += check("inflate_out[4..7]", *(uint32_t *)&out_buf[4], 0x6C666E49, buf);
  }

  {
    static const unsigned char compressed[23] = {
      0x78, 0xda, 0xf3, 0x48, 0xcc, 0xc9, 0xf7, 0xcc,
      0x4b, 0xcb, 0x49, 0x2c, 0x49, 0x0d, 0x49, 0x2d,
      0x2e, 0x01, 0x00, 0x2d, 0xdb, 0x05, 0xe8
    };
    unsigned char out_buf[4];
    uint32_t dump_values[4];
    int zs[14];
    int init_ret;
    int inflate_ret;

    csmemset(zs, 0, sizeof(zs));
    csmemset(out_buf, 0xCC, sizeof(out_buf));
    init_ret = FUN_001155c0((int)zs, "1.1.3", 0x38);
    inflate_ret = (int)0xfffffffe;
    if (init_ret == 0) {
      zs[0] = (int)compressed;
      zs[1] = sizeof(compressed);
      zs[3] = (int)out_buf;
      zs[4] = sizeof(out_buf);
      inflate_ret = FUN_001155e0((int)zs, 4);
      FUN_00115430((int)zs);
    }

    dump_values[0] = (uint32_t)init_ret;
    dump_values[1] = (uint32_t)inflate_ret;
    dump_values[2] = *(uint32_t *)&out_buf[0];
    dump_values[3] = (uint32_t)zs[4];
    dump_u32_case("inflate", "inflate_tiny_outbuf", dump_values, 4, buf);
  }

  {
    static const unsigned char compressed[23] = {
      0x78, 0xda, 0xf3, 0x48, 0xcc, 0xc9, 0xf7, 0xcc,
      0x4b, 0xcb, 0x49, 0x2c, 0x49, 0x0d, 0x49, 0x2d,
      0x2e, 0x01, 0x00, 0x2d, 0xdb, 0x05, 0xe8
    };
    unsigned char out_buf[16];
    uint32_t dump_values[5];
    int zs[14];
    int init_ret;
    int inflate_ret;

    csmemset(zs, 0, sizeof(zs));
    csmemset(out_buf, 0xCC, sizeof(out_buf));
    init_ret = FUN_001155c0((int)zs, "1.1.3", 0x38);
    inflate_ret = (int)0xfffffffe;
    if (init_ret == 0) {
      zs[0] = (int)compressed;
      zs[1] = 8;
      zs[3] = (int)out_buf;
      zs[4] = sizeof(out_buf);
      inflate_ret = FUN_001155e0((int)zs, 4);
      FUN_00115430((int)zs);
    }

    dump_values[0] = (uint32_t)init_ret;
    dump_values[1] = (uint32_t)inflate_ret;
    dump_values[2] = *(uint32_t *)&out_buf[0];
    dump_values[3] = *(uint32_t *)&out_buf[4];
    dump_values[4] = (uint32_t)zs[1];
    dump_u32_case("inflate", "inflate_truncated_input", dump_values, 5, buf);
  }

  /* cache_file_init: verify FUN_001bd5f0 ran correctly at boot.
   * Dumps each slot's file handle, 'head' marker (+0x0C), map name (+0x2C),
   * build string (+0x4C), and file-time CRC (+0x70). Compared between
   * oracle (original) and candidate (ported) builds by run_golden_tests.py. */
  {
    uint32_t slot_state[18];
    char *slot_base = (char *)0x4e61d8;
    int si;

    for (si = 0; si < 6; si++) {
      char *entry = slot_base + si * 0x80c;
      slot_state[si * 3]     = *(uint32_t *)(entry);
      slot_state[si * 3 + 1] = *(uint32_t *)(entry + 0x0c);
      slot_state[si * 3 + 2] = *(uint32_t *)(entry + 0x2c);
    }

    dump_u32_case("cache_file_init", "slot_state_at_boot", slot_state, 18, buf);

    total += 6;
    for (si = 0; si < 6; si++) {
      char *entry = slot_base + si * 0x80c;
      int handle = *(int *)entry;
      passed += check("cache_slot_handle_valid",
                      (uint32_t)(handle != 0 && handle != -1),
                      1, buf);
    }
  }

  /* CRC round-trip: exercises the write→read checksum invariant that broke
   * when game_state_test_persistent_storage passed a separate scratch variable
   * instead of a pointer into header+0x148.  Uses the real crc_new /
   * crc_checksum_buffer from the engine. */
  {
    uint8_t header[0x14c];
    uint32_t write_crc;
    uint32_t read_saved;
    uint32_t read_computed;
    int i;

    for (i = 0; i < 0x14c; i++)
      header[i] = (uint8_t)(i * 7 + 0x13);

    /* --- write side: zero checksum field, compute, store --- */
    *(uint32_t *)(header + 0x148) = 0;
    crc_new(&write_crc);
    crc_checksum_buffer(&write_crc, header, 0x14c);
    *(uint32_t *)(header + 0x148) = write_crc;

    /* --- read side: pointer-into-header pattern (the correct way) --- */
    read_saved = *(uint32_t *)(header + 0x148);
    *(uint32_t *)(header + 0x148) = 0;
    crc_new(&read_computed);
    crc_checksum_buffer(&read_computed, header, 0x14c);

    total += 2;
    passed += check("crc_roundtrip_match",
                    (uint32_t)(read_computed == read_saved), 1, buf);
    passed += check("crc_roundtrip_value", read_computed, write_crc, buf);
  }

  crt_sprintf(buf, "RUN|END|passed=%d|failed=%d|total=%d\n", passed,
              total - passed, total);
  debug_string_to_display(buf, 0);
}
