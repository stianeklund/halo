/* Render camera utilities. */

#include "x87_math.h"

#define MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS 64

/* Saved copies of the four frustum projection terms hacked by
 * render_camera_hack_frustum_z; stored and restored as raw dwords, exactly as
 * the original does (MOV, never FLD/FSTP). */
static uint32_t render_camera_saved_frustum_z[4]; /* 0x4d0d08 */

static char render_camera_warnings_initialized; /* 0x4d0e18 */
static float render_camera_warning_values
  [MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS]; /* 0x4d0d18
                                               */

enum {
  _render_frustum_point_flags_left_bit,
  _render_frustum_point_flags_right_bit,
  _render_frustum_point_flags_top_bit,
  _render_frustum_point_flags_bottom_bit,
  _render_frustum_point_flags_near_bit,
  _render_frustum_point_flags_far_bit,
  NUMBER_OF_RENDER_FRUSTUM_POINT_FLAGS
};

#define RENDER_FRUSTUM_POINT_FLAGS_PLANE_MASK \
  (FLAG(NUMBER_OF_RENDER_FRUSTUM_POINT_FLAGS) - 1)

/* Header-inline math helpers.  The original expands these inline (e.g. no
 * calls for them in render_camera_build_frustum 0x187250..0x187f7a); bodies
 * follow the PAL 2342 real_math.h definitions.  Suffixed _inline because
 * kb.json already names the out-of-line copies (0x178d0, 0x99490, 0x99500). */
#define global_origin3d (*(vector3_t **)0x31fc1c)
#define debug_no_frustum_clip (*(bool *)0x4d0e19)
#define render_camera_debug_this_fucking_frustum (*(bool *)0x4d0e1a)
#define global_real_argb_red (*(void **)0x2ee6d0)
#define rasterizer_current_lock_operation (*(int16_t *)0x325652)
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* The engine builds with -fno-builtin, so clang would lower fabs() to a CRT
 * call; the original (and VC71 /Oi) inlines FABS. */
#if !(defined(_MSC_VER) && !defined(__clang__))
#define fabs(x) __builtin_fabs(x)
#endif

static __inline vector3_t *cross_product3d_inline(const vector3_t *a,
                                                  const vector3_t *b,
                                                  vector3_t *result)
{
  real k = a->x * b->y - a->y * b->x;
  real j = a->z * b->x - a->x * b->z;
  real i = a->y * b->z - a->z * b->y;

  result->x = i;
  result->y = j;
  result->z = k;
  return result;
}

static __inline real dot_product3d_inline(const vector3_t *a,
                                          const vector3_t *b)
{
  return a->x * b->x + a->y * b->y + a->z * b->z;
}

static __inline real magnitude3d_inline(const vector3_t *v)
{
  return (real)x87_sqrtd(dot_product3d_inline(v, v));
}

static __inline real_plane3d *plane3d_from_point_and_normal_inline(
  real_plane3d *plane, const vector3_t *point, const vector3_t *normal)
{
  *(vector3_t *)plane->normal = *normal;
  plane->d = dot_product3d_inline(point, (const vector3_t *)plane->normal);
  return plane;
}

static __inline real plane3d_distance_to_point_inline(const real_plane3d *plane,
                                                      const vector3_t *point)
{
  return dot_product3d_inline(point, (const vector3_t *)plane->normal) -
         plane->d;
}

/* render_camera_warn_once - 0x185770
 * Tracks maximum frustum-integrity violation distances per condition ID.
 * Logs when a condition exceeds its previous worst value.
 * render_camera_build_frustum calls this out of line 22 times (id in AX), so
 * it must not be inlined into that caller. */
__declspec(noinline) void render_camera_warn_once(int16_t id,
                                                                float value)
{
  assert_halt(id >= 0 && id < MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS);

  if (!render_camera_warnings_initialized) {
    csmemset(render_camera_warning_values, 0,
             sizeof(render_camera_warning_values));
    render_camera_warnings_initialized = 1;
  }

  /* 001857c9 FCOMP [0.05f] / TEST AH,0x1 / JNZ skip: only strictly-below
   * skips, so the bound is inclusive. */
  if (value >= 0.05f && value > render_camera_warning_values[id]) {
    error(2,
          "### ERROR cameras: frustum-integrity condition #%d violated by %f",
          (int)id, (double)value);
    render_camera_warning_values[id] = value;
  }
}

/* render_camera_new - 0x185810
 * Zero-initializes a render camera block (camera_t, 0x54 bytes). */
void render_camera_new(camera_t *camera)
{
  csmemset(camera, 0, sizeof(camera_t));
}

/* render_camera_hack_frustum_z - 0x185830
 *
 * Overrides the near/far terms of a frustum's projection matrix.  Two sentinel
 * argument pairs select a save/restore mode instead of a recompute:
 *
 * Evidence (0x185830..0x18594a):
 *   TEST ESI,ESI / JZ + MOV AL,[ESI+0x140] / TEST AL,AL / JNZ
 *     => assert(frustum && frustum->projection_valid) at line 0x10f.
 *   FLD [EBP+0xc] / FCOMP [0x00255e94] / FNSTSW AX / TEST AH,0x44 / JP
 *     (twice)  ; 0x255e94 = -1.0f.  TEST AH,0x44 + JP is the MSVC equality
 *     test, the JP taking the not-equal path.  Both args == -1.0f saves the
 *     four terms into 0x4d0d08..0x4d0d14 as plain dword MOVs.
 *   Same shape against 0x002533c0 (0.0f) restores them, also as dword MOVs.
 *   Otherwise:
 *     FLD [EBP+0x10] / FSUB [EBP+0xc]      => denom = far_z - near_z, held in
 *                                             ST1 across both stores
 *     MOV [ESI+0x14c],0 / MOV [ESI+0x15c],0
 *     FLD [EBP+0xc] / FADD [EBP+0x10] / FDIV ST0,ST1 / FCHS / FSTP [ESI+0x16c]
 *     FLD [EBP+0xc] / FMUL [EBP+0x10] / FMUL [0x0025eeac] / FDIV ST0,ST1 /
 *       FSTP [ESI+0x17c]                   ; 0x25eeac = -2.0f
 *   The trailing FSTP ST0 discards the shared denominator.
 *
 * The frustum has no recovered type at this decl, so the projection terms
 * (stride 0x10 from +0x14c) and the validity byte at +0x140 are dereferenced
 * raw, as in the rest of this file.
 */
void render_camera_hack_frustum_z(void *frustum, float near_z, float far_z)
{
  char *f;
  float denom_held;
  float denom;

  f = (char *)frustum;

  assert_halt_msg_at("frustum && frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x10f,
                     frustum != 0 && *(char *)(f + 0x140) != 0);

  if (near_z == *(float *)0x255e94 && far_z == *(float *)0x255e94) {
    render_camera_saved_frustum_z[0] = *(uint32_t *)(f + 0x14c);
    render_camera_saved_frustum_z[1] = *(uint32_t *)(f + 0x15c);
    render_camera_saved_frustum_z[2] = *(uint32_t *)(f + 0x16c);
    render_camera_saved_frustum_z[3] = *(uint32_t *)(f + 0x17c);
    return;
  }

  if (near_z == *(float *)0x2533c0 && far_z == *(float *)0x2533c0) {
    *(uint32_t *)(f + 0x14c) = render_camera_saved_frustum_z[0];
    *(uint32_t *)(f + 0x15c) = render_camera_saved_frustum_z[1];
    *(uint32_t *)(f + 0x16c) = render_camera_saved_frustum_z[2];
    *(uint32_t *)(f + 0x17c) = render_camera_saved_frustum_z[3];
    return;
  }

  denom = far_z - near_z;
  *(uint32_t *)(f + 0x14c) = 0;
  *(uint32_t *)(f + 0x15c) = 0;
  /* The original leaves the single subtraction result on the x87 stack (ST1)
   * across both divides and pops it once at the end; the second reference to
   * the divisor is written through a second local so the divisor stays held
   * rather than being recomputed/reloaded per divide (+3.1pp VC71). */
  denom_held = denom;
  *(float *)(f + 0x16c) = -((near_z + far_z) / denom);
  *(float *)(f + 0x17c) = (near_z * far_z * *(float *)0x25eeac) / denom_held;
}

/* render_camera_build_frustum_bounds - 0x185950
 *
 * Maps the camera viewport into normalized frustum bounds relative to the
 * window bounds (camera + 0x34), scales x by the viewport aspect ratio, and
 * flips y.
 *
 * Evidence (0x185950..0x185a6a): asserts at lines 0x131/0x132; FILD
 * (y1 - y0) / FIDIV (x1 - x0) for the aspect ratio; FILD (wy1 - wy0) /
 * FDIVR 1.0f for the inverse window height; each bound is
 * 2 * viewport - window.x1 - window.x0 (or y0 then y1), the x1/y1 window term
 * subtracted first for x and the y0 term first for y.
 */
void render_camera_build_frustum_bounds(camera_t *camera,
                                        real_rectangle2d *frustum_bounds)
{
  const viewport_bounds_t *viewport_bounds;
  const viewport_bounds_t *window_bounds;
  real aspect_ratio;
  real inverse_window_height;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x131, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x132,
                 frustum_bounds);

  viewport_bounds = &camera->viewport_bounds;
  window_bounds = &camera->unk_52;
  aspect_ratio = (real)(viewport_bounds->y1 - viewport_bounds->y0) /
                 (viewport_bounds->x1 - viewport_bounds->x0);
  inverse_window_height = 1.0f / (window_bounds->y1 - window_bounds->y0);

  frustum_bounds->x0 =
    (2 * viewport_bounds->x0 - window_bounds->x1 - window_bounds->x0) *
    inverse_window_height;
  frustum_bounds->x1 =
    (2 * viewport_bounds->x1 - window_bounds->x1 - window_bounds->x0) *
    inverse_window_height;
  frustum_bounds->y0 =
    (2 * viewport_bounds->y0 - window_bounds->y0 - window_bounds->y1) *
    inverse_window_height;
  frustum_bounds->y1 =
    (2 * viewport_bounds->y1 - window_bounds->y0 - window_bounds->y1) *
    inverse_window_height;

  frustum_bounds->x0 *= aspect_ratio;
  frustum_bounds->x1 *= aspect_ratio;

  {
    real temporary_y0 = frustum_bounds->y0;

    frustum_bounds->y0 = -frustum_bounds->y1;
    frustum_bounds->y1 = -temporary_y0;
  }
}

/* render_frustum_sphere_diameter_in_pixels - 0x185a70
 *
 * Projects a world-space sphere onto the frustum and returns its diameter in
 * pixels.  The depth term is row 3 of the frustum's world-to-view matrix
 * (frustum + 0x1c / +0x28 / +0x34 dotted with the center, plus the
 * translation at +0x40); it is made positive, clamped up to a floor, then
 * divided into the pixel scale at frustum + 0x188 and scaled by the radius.
 * The doubled result is the diameter.
 *
 * Evidence (0x185a70..0x185ac1):
 *   FLD [ECX+0x34] / FMUL [EAX+8] / FLD [ECX+0x28] / FMUL [EAX+4] / FADDP /
 *   FLD [ECX+0x1c] / FMUL [EAX] / FADDP / FADD [ECX+0x40]
 *     => ((m34*c[2] + m28*c[1]) + m1c*c[0]) + m40, in that x87 order.
 *   FCOM [0x002533c0] / TEST AH,0x1 / JZ +2 / FCHS   ; 0x2533c0 = 0.0f
 *   FCOM [0x0025496c] / TEST AH,0x41 / JZ +8 / FSTP ST0 / FLD [0x0025496c]
 *                                                   ; 0x25496c = 0.1f
 *   FDIVR [ECX+0x188] / FMUL [EBP+0x10] / FADD ST0,ST0
 *
 * The frustum has no recovered type at this decl, so the four matrix fields
 * and the pixel scale are dereferenced raw.  TEST AH,0x1 is C0 alone (strictly
 * less than), TEST AH,0x41 is C0|C3 (less than or equal); both senses are
 * written out as the binary tests them.
 */
float render_frustum_sphere_diameter_in_pixels(void *frustum, float *center,
                                               float radius)
{
  char *f;
  float depth;
  float scaled;

  f = (char *)frustum;
  depth = *(float *)(f + 0x34) * center[2] + *(float *)(f + 0x28) * center[1] +
          *(float *)(f + 0x1c) * center[0] + *(float *)(f + 0x40);

  if (depth < *(float *)0x2533c0) {
    depth = -depth;
  }
  if (depth <= *(float *)0x25496c) {
    depth = *(float *)0x25496c;
  }

  scaled = (*(float *)(f + 0x188) / depth) * radius;
  return scaled + scaled;
}

/* render_frustum_cube_view_fraction - 0x185ad0
 *
 * Fraction of the projected [-1, 1] square covered by a view-space box:
 * 0 when the box starts at or behind the eye, 1 when it straddles it,
 * otherwise the clamped projected extent at its near and far depths.
 *
 * Evidence (0x185ad0..0x185f71): asserts at lines 0x36c..0x370, 0x384
 * (projection_valid) and 0x387..0x38e (the projection matrix is a pure
 * perspective); [0][0], [2][0], [1][1], [2][1] are read before the
 * projection_valid assert; MIN/MAX re-evaluate their operands.
 */
float render_frustum_cube_view_fraction(void *frustum, float *bounds)
{
  const render_frustum_t *f = (const render_frustum_t *)frustum;
  const real_rectangle3d *box = (const real_rectangle3d *)bounds;
  real fraction;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x36c, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x36d, bounds);
  assert_halt_msg_at("bounds->x0<=bounds->x1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x36e,
                     box->x0 <= box->x1);
  assert_halt_msg_at("bounds->y0<=bounds->y1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x36f,
                     box->y0 <= box->y1);
  assert_halt_msg_at("bounds->z0<=bounds->z1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x370,
                     box->z0 <= box->z1);

  if (box->z0 >= 0.0f) {
    fraction = 0.0f;
  } else if (box->z1 >= 0.0f) {
    fraction = 1.0f;
  } else {
    real inverse_z0 = 1.0f / box->z0;
    real projection_x = f->field_144[0];
    real projection_offset_x = f->field_144[8];
    real projection_y = f->field_144[5];
    real projection_offset_y = f->field_144[9];
    real inverse_z1 = 1.0f / box->z1;
    real left;
    real bottom;
    real right;
    real top;
    real near_extent;
    real far_extent;

    assert_halt_msg_at("frustum->projection_valid",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x384,
                       f->field_140);
    assert_halt_msg_at("frustum->projection_matrix[1][0]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x387,
                       f->field_144[4] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[3][0]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x388,
                       f->field_144[12] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[0][1]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x389,
                       f->field_144[1] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[3][1]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38a,
                       f->field_144[13] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[0][3]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38b,
                       f->field_144[3] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[1][3]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38c,
                       f->field_144[7] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[2][3]==-1.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38d,
                       f->field_144[11] == -1.0f);
    assert_halt_msg_at("frustum->projection_matrix[3][3]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38e,
                       f->field_144[15] == 0.0f);

    near_extent =
      (box->z0 * projection_offset_x + box->x0 * projection_x) * (-inverse_z0);
    far_extent =
      (box->z1 * projection_offset_x + box->x0 * projection_x) * (-inverse_z1);
    left = MAX(MIN(near_extent, far_extent), -1.0f);
    near_extent =
      (box->z0 * projection_offset_y + box->y0 * projection_y) * (-inverse_z0);
    far_extent =
      (box->z1 * projection_offset_y + box->y0 * projection_y) * (-inverse_z1);
    bottom = MAX(MIN(near_extent, far_extent), -1.0f);
    near_extent =
      (box->z0 * projection_offset_x + box->x1 * projection_x) * (-inverse_z0);
    far_extent =
      (box->z1 * projection_offset_x + box->x1 * projection_x) * (-inverse_z1);
    right = MIN(MAX(near_extent, far_extent), 1.0f);
    near_extent =
      (box->z0 * projection_offset_y + box->y1 * projection_y) * (-inverse_z0);
    far_extent =
      (box->z1 * projection_offset_y + box->y1 * projection_y) * (-inverse_z1);
    top = MIN(MAX(near_extent, far_extent), 1.0f);
    fraction = (right - left) * (top - bottom) * 0.25f;
    if (!(fraction > 0.0f)) {
      fraction = 0.0f;
    }
  }

  return fraction;
}

/* render_frustum_get_projection_bounds - 0x185f80
 *
 * Recovers the view-space bounds at unit depth from the projection matrix
 * scale ([0][0], [1][1]) and offset ([2][0], [2][1]) terms.
 *
 * Evidence (0x185f80..0x186040): assert lines 0x3a7/0x3a8; FLD -1.0f
 * (0x255e94) / FDIV [+0x144] and [+0x158]; x0/x1 = (x -/+ 1) * inverse_x,
 * y0/y1 = (y -/+ 1) * inverse_y.
 */
void render_frustum_get_projection_bounds(void *frustum, void *bounds)
{
  const render_frustum_t *f = (const render_frustum_t *)frustum;
  real_rectangle2d *projection_bounds = (real_rectangle2d *)bounds;
  real x;
  real y;
  real inverse_x_scale;
  real inverse_y_scale;

  assert_halt_msg_at("frustum && frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3a7,
                     f && f->field_140);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3a8, bounds);

  x = -f->field_144[8];
  y = -f->field_144[9];
  inverse_x_scale = -1.0f / f->field_144[0];
  inverse_y_scale = -1.0f / f->field_144[5];

  projection_bounds->x0 = (x - 1.0f) * inverse_x_scale;
  projection_bounds->x1 = (x + 1.0f) * inverse_x_scale;
  projection_bounds->y0 = (y - 1.0f) * inverse_y_scale;
  projection_bounds->y1 = (y + 1.0f) * inverse_y_scale;
}

/* render_camera_view_to_screen - 0x186050
 *
 * Projects a view-space point to viewport pixels; fails for points at or
 * behind the eye (z >= 0) or outside the [-1, 1] projected square.
 *
 * Evidence (0x186050..0x186223): asserts at lines 0x3d2..0x3d5 and the
 * projection_valid byte at +0x140 (line 0x3dc); inverse depth is
 * -1.0f / z; x uses projection [0][0]/[2][0], y the negated [1][1]/[2][1]
 * terms; the viewport height is loaded before the width.
 */
char render_camera_view_to_screen(int *camera, int *frustum, void *view_point,
                                  void *screen_point)
{
  const camera_t *c = (const camera_t *)camera;
  const render_frustum_t *f = (const render_frustum_t *)frustum;
  const vector3_t *point = (const vector3_t *)view_point;
  float *screen = (float *)screen_point;
  char result = 0;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3d2, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3d3, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3d4,
                 view_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3d5,
                 screen_point);

  if (point->z < 0.0f) {
    real inverse_depth = -1.0f / point->z;

    assert_halt_msg_at("frustum->projection_valid",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3dc,
                       f->field_140);

    screen[0] =
      (f->field_144[0] * point->x + f->field_144[8] * point->z) * inverse_depth;
    screen[1] = -(f->field_144[5] * point->y + f->field_144[9] * point->z) *
                inverse_depth;
    if (screen[0] >= -1.0f && screen[0] <= 1.0f && screen[1] >= -1.0f &&
        screen[1] <= 1.0f) {
      real viewport_height =
        (real)(c->viewport_bounds.y1 - c->viewport_bounds.y0);
      real viewport_width =
        (real)(c->viewport_bounds.x1 - c->viewport_bounds.x0);

      screen[0] =
        viewport_width * ((screen[0] + 1.0f) * 0.5f) + c->viewport_bounds.x0;
      screen[1] =
        ((screen[1] + 1.0f) * 0.5f) * viewport_height + c->viewport_bounds.y0;
      result = 1;
    }
  }

  return result;
}

/* render_camera_screen_to_view - 0x186230
 *
 * Unprojects a normalized screen point to a view-space direction at z = -1.
 *
 * Evidence (0x186230..0x18632b): asserts at lines 0x3fa..0x3fd and the
 * projection_valid byte at +0x140 (line 0x404); x = (sx - [2][0]) / [0][0];
 * y = -(([2][1] + sy) / [1][1]); z stored as the dword 0xbf800000.
 */
void render_camera_screen_to_view(void *camera, void *frustum,
                                  float *screen_point, float *view_vector)
{
  const render_frustum_t *f = (const render_frustum_t *)frustum;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fa, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fb, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fc,
                 screen_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fd,
                 view_vector);
  assert_halt_msg_at("frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x404,
                     f->field_140);

  view_vector[0] = (screen_point[0] - f->field_144[8]) / f->field_144[0];
  view_vector[1] = -((f->field_144[9] + screen_point[1]) / f->field_144[5]);
  view_vector[2] = -1.0f;
}

/* render_camera_screen_to_world - 0x186330
 *
 * Builds a world-space ray for a screen point: the ray origin is the camera
 * position (camera + 0x00) copied verbatim, and the direction is the view-space
 * ray from render_camera_screen_to_view rotated into world space by the
 * frustum's view-to-world matrix (frustum + 0x44).
 *
 * Evidence (0x186330..0x18645f): five null-parameter asserts at source lines
 * 0x41c..0x420, then a frustum->projection_valid byte test at +0x140 (line
 * 0x422).  The position copy is three dword MOVs from [ESI]/[ESI+4]/[ESI+8]
 * into [EDI]/[EDI+4]/[EDI+8] (a struct assignment, not three float loads).
 * Both calls are cdecl; the shared ADD ESP,0x1c at 0x186456 retires 4+3 args.
 */
void render_camera_screen_to_world(camera_t *camera, float *frustum,
                                   float *screen_point, vector3_t *world_point,
                                   float *world_vector)
{
  float view_vector[3];

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41c, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41d, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41e,
                 screen_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x41f,
                 world_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x420,
                 world_vector);
  assert_halt_msg_at("frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x422,
                     *((char *)frustum + 0x140) != 0);

  render_camera_screen_to_view(camera, frustum, screen_point, view_vector);

  *world_point = camera->field_00;

  matrix_scale_transform_vector(frustum + 17, view_vector, world_vector);
}

/* Compute the adjusted FOV tangent for the render camera.
 * Uses FPTAN: tan(fov * half_constant) * aspect_ratio */
double render_camera_get_adjusted_field_of_view_tangent(float fov)
{
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines tan as FPTAN (matches original). */
  return tan(fov * *(float *)0x253398) * *(float *)0x2b1504;
#else
  double result;
  asm volatile("flds %[f]\n\t"
               "fmuls 0x253398\n\t"
               "fptan\n\t"
               "fstp %%st(0)\n\t"
               "fmuls 0x2b1504"
               : "=t"(result)
               : [f] "m"(fov)
               : "memory");
  return result;
#endif
}

/* render_camera_build_clipped_frustum_bounds - 0x186480
 *
 * Scales a view-space clip rectangle into frustum bounds; falls back to the
 * full [-1, 1] bounds when clipping is disabled or the rectangle is empty.
 * Returns true when the clipped bounds were used.
 *
 * Evidence (0x186480..0x1865d6): asserts at lines 0x156..0x158;
 * debug_no_frustum_clip byte at 0x4d0e19; FPTAN of fov * 0.5f; aspect =
 * FILD (y1 - y0) / FIDIV (x1 - x0); result is SETZ of the use-full byte.
 */
bool render_camera_build_clipped_frustum_bounds(camera_t *camera,
                                                float *in_bounds,
                                                float *out_bounds)
{
  const real_rectangle2d *clip = (const real_rectangle2d *)in_bounds;
  real_rectangle2d *frustum_bounds = (real_rectangle2d *)out_bounds;
  bool use_full_bounds = 1;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x156, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x157,
                 in_bounds);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x158,
                 out_bounds);

  if (!debug_no_frustum_clip && clip->x0 < clip->x1 && clip->y0 < clip->y1) {
    long viewport_height =
      camera->viewport_bounds.y1 - camera->viewport_bounds.y0;
    long viewport_width =
      camera->viewport_bounds.x1 - camera->viewport_bounds.x0;
    real aspect_ratio = (real)viewport_height / (real)viewport_width;
    real inverse_tangent;
    real horizontal_scale;

#if defined(_MSC_VER) && !defined(__clang__)
    /* VC71 /Oi inlines tan as FPTAN (matches original). */
    inverse_tangent = 1.0f / (real)tan(camera->vertical_field_of_view * 0.5f);
#else
    inverse_tangent = 1.0f / x87_fptan(camera->vertical_field_of_view * 0.5f);
#endif
    horizontal_scale = inverse_tangent * aspect_ratio;

    frustum_bounds->x0 = clip->x0 * horizontal_scale;
    frustum_bounds->x1 = clip->x1 * horizontal_scale;
    frustum_bounds->y0 = inverse_tangent * clip->y0;
    frustum_bounds->y1 = clip->y1 * inverse_tangent;
    use_full_bounds = frustum_bounds->x0 >= frustum_bounds->x1 ||
                      frustum_bounds->y0 >= frustum_bounds->y1;
  }

  if (use_full_bounds) {
    frustum_bounds->y1 = 1.0f;
    frustum_bounds->x1 = 1.0f;
    frustum_bounds->y0 = -1.0f;
    frustum_bounds->x0 = -1.0f;
  }

  return !use_full_bounds;
}

/* render_camera_triangle_frontfacing - 0x1865e0
 *
 * True when the triangle's winding normal faces the camera: the normal of
 * (point1 - point0) x (point2 - point1) dotted with (point0 - camera position)
 * exceeds -_real_epsilon (0x26a810 = -0.0001f; FCOMP / TEST AH,0x41 / JNZ
 * returns 0).  No callers in this build.
 */
bool render_camera_triangle_frontfacing(camera_t *camera, vector3_t *point0,
                                        vector3_t *point1, vector3_t *point2)
{
  vector3_t camera_to_point;
  vector3_t edge0;
  vector3_t edge1;
  vector3_t normal;

  camera_to_point.x = point0->x - camera->field_00.x;
  camera_to_point.y = point0->y - camera->field_00.y;
  camera_to_point.z = point0->z - camera->field_00.z;
  edge0.x = point1->x - point0->x;
  edge0.y = point1->y - point0->y;
  edge0.z = point1->z - point0->z;
  edge1.x = point2->x - point1->x;
  edge1.y = point2->y - point1->y;
  edge1.z = point2->z - point1->z;
  cross_product3d_inline(&edge0, &edge1, &normal);

  return dot_product3d_inline(&normal, &camera_to_point) > -0.0001f;
}

/* render_frustum_build_point_flags - 0x186690
 *
 * Outcode of a world point against the four side planes: a plane whose
 * signed distance is positive (FCOMP 0.0f / TEST AH,0x41 / JNZ -> 0) sets its
 * bit.  Plane order is left, right, bottom, top; the bottom plane sets the
 * bottom bit (0x8) and the top plane the top bit (0x4).
 */
int16_t render_frustum_build_point_flags(void *plane_ctx, void *point)
{
  const render_frustum_t *f = (const render_frustum_t *)plane_ctx;
  const vector3_t *p = (const vector3_t *)point;
  int16_t flags = plane3d_distance_to_point_inline(&f->field_78[0], p) > 0.0f ?
                    FLAG(_render_frustum_point_flags_left_bit) :
                    0;

  flags |= plane3d_distance_to_point_inline(&f->field_78[1], p) > 0.0f ?
             FLAG(_render_frustum_point_flags_right_bit) :
             0;
  flags |= plane3d_distance_to_point_inline(&f->field_78[2], p) > 0.0f ?
             FLAG(_render_frustum_point_flags_bottom_bit) :
             0;
  flags |= plane3d_distance_to_point_inline(&f->field_78[3], p) > 0.0f ?
             FLAG(_render_frustum_point_flags_top_bit) :
             0;
  return flags;
}

/* Frustum-cull a triangle against the clip planes.
 * Builds the outcode flags for each of the three points; a point with no
 * flags is inside every plane, so the triangle is trivially visible.
 * Otherwise the triangle is visible unless all three outcodes share a
 * common plane bit (0x3f mask = the 6 clip planes). */
bool render_frustum_triangle_visible(void *plane_ctx, void *v0, void *v1,
                                     void *v2)
{
  int16_t flags;
  int16_t point_flags;

  point_flags = render_frustum_build_point_flags(plane_ctx, v0);
  if (point_flags == 0) {
    return 1;
  }
  flags = (int16_t)(point_flags & 0x3f);
  point_flags = render_frustum_build_point_flags(plane_ctx, v1);
  if (point_flags == 0) {
    return 1;
  }
  flags &= point_flags;
  point_flags = render_frustum_build_point_flags(plane_ctx, v2);
  if (point_flags == 0) {
    return 1;
  }
  return (int16_t)(point_flags & flags) == 0;
}

/* render_frustum_cube_visible - 0x1867f0
 *
 * 0 = outside, 1 = intersecting, 2 = fully inside.  Rejects on the world
 * AABB (+0x128), classifies the eight box corners against the four side
 * planes, then optionally the five frustum vertices (+0xe0) against the box
 * faces (bits 0x1..0x20).
 */
int16_t render_frustum_cube_visible(void *frustum, float *bounds,
                                    bool test_frustum_against_cube)
{
  const render_frustum_t *f = (const render_frustum_t *)frustum;
  const real_rectangle3d *box = (const real_rectangle3d *)bounds;

  if (f->field_128[1] < box->x0) {
    return 0;
  }
  if (f->field_128[3] < box->y0) {
    return 0;
  }
  if (f->field_128[5] < box->z0) {
    return 0;
  }
  if (f->field_128[0] > box->x1) {
    return 0;
  }
  if (f->field_128[2] > box->y1) {
    return 0;
  }
  if (f->field_128[4] > box->z1) {
    return 0;
  }

  {
    vector3_t cube_vertices[8];
    uint16_t intersection_flags = RENDER_FRUSTUM_POINT_FLAGS_PLANE_MASK;
    uint16_t union_flags = 0;
    int16_t vertex_index;

    cube_vertices[0].x = cube_vertices[2].x = cube_vertices[4].x =
      cube_vertices[6].x = box->x0;
    cube_vertices[1].x = cube_vertices[3].x = cube_vertices[5].x =
      cube_vertices[7].x = box->x1;
    cube_vertices[0].y = cube_vertices[1].y = cube_vertices[4].y =
      cube_vertices[5].y = box->y0;
    cube_vertices[2].y = cube_vertices[3].y = cube_vertices[6].y =
      cube_vertices[7].y = box->y1;
    cube_vertices[0].z = cube_vertices[1].z = cube_vertices[2].z =
      cube_vertices[3].z = box->z0;
    cube_vertices[4].z = cube_vertices[5].z = cube_vertices[6].z =
      cube_vertices[7].z = box->z1;

    for (vertex_index = 0; vertex_index < 8; vertex_index++) {
      const vector3_t *vertex = &cube_vertices[vertex_index];
      uint16_t flags =
        plane3d_distance_to_point_inline(&f->field_78[0], vertex) > 0.0f ?
          FLAG(_render_frustum_point_flags_left_bit) :
          0;

      flags |=
        plane3d_distance_to_point_inline(&f->field_78[1], vertex) > 0.0f ?
          FLAG(_render_frustum_point_flags_right_bit) :
          0;
      flags |=
        plane3d_distance_to_point_inline(&f->field_78[2], vertex) > 0.0f ?
          FLAG(_render_frustum_point_flags_bottom_bit) :
          0;
      flags |=
        plane3d_distance_to_point_inline(&f->field_78[3], vertex) > 0.0f ?
          FLAG(_render_frustum_point_flags_top_bit) :
          0;

      intersection_flags &= flags;
      union_flags |= flags;
    }

    if (!union_flags) {
      return 2;
    }

    if (intersection_flags) {
      return 0;
    }

    if (test_frustum_against_cube) {
      intersection_flags = RENDER_FRUSTUM_POINT_FLAGS_PLANE_MASK;
      for (vertex_index = 0; vertex_index < 5; vertex_index++) {
        const vector3_t *vertex = &f->field_e0[vertex_index];
        uint16_t right_bit;
        uint16_t bottom_bit;
        uint16_t top_bit;
        uint16_t near_bit;
        uint16_t flags =
          vertex->x <= box->x0 ? FLAG(_render_frustum_point_flags_left_bit) : 0;

        right_bit = vertex->x >= box->x1 ?
                      FLAG(_render_frustum_point_flags_right_bit) :
                      0;
        flags |= right_bit;
        bottom_bit = vertex->y <= box->y0 ?
                       FLAG(_render_frustum_point_flags_bottom_bit) :
                       0;
        flags |= bottom_bit;
        top_bit =
          vertex->y >= box->y1 ? FLAG(_render_frustum_point_flags_top_bit) : 0;
        flags |= top_bit;
        near_bit =
          vertex->z <= box->z0 ? FLAG(_render_frustum_point_flags_near_bit) : 0;
        flags |= near_bit;
        flags |=
          vertex->z >= box->z1 ? FLAG(_render_frustum_point_flags_far_bit) : 0;
        intersection_flags &= flags;
      }

      if (intersection_flags) {
        return 0;
      }
    }

    return 1;
  }
}

/* render_frustum_sphere_visible - 0x186ac0
 *
 * 0 = outside, 1 = intersecting, 2 = fully inside.  Rejects on the world
 * AABB (+0x128) first, then on each of the six world planes (+0x78) via the
 * out-of-line plane3d_distance_to_point; the near plane (4) distance is not
 * kept for the containment test.
 */
int16_t render_frustum_sphere_visible(void *frustum, float *center,
                                      float radius)
{
  const render_frustum_t *f = (const render_frustum_t *)frustum;
  real distance0;
  real distance1;
  real distance2;
  real distance3;
  real distance5;
  real negative_radius;

  if (f->field_128[1] < center[0] - radius ||
      f->field_128[3] < center[1] - radius ||
      f->field_128[5] < center[2] - radius ||
      f->field_128[0] > center[0] + radius ||
      f->field_128[2] > center[1] + radius ||
      f->field_128[4] > center[2] + radius) {
    return 0;
  }

  distance0 = plane3d_distance_to_point((float *)&f->field_78[0], center);
  if (distance0 > radius)
    return 0;
  distance1 = plane3d_distance_to_point((float *)&f->field_78[1], center);
  if (distance1 > radius)
    return 0;
  distance2 = plane3d_distance_to_point((float *)&f->field_78[2], center);
  if (distance2 > radius)
    return 0;
  distance3 = plane3d_distance_to_point((float *)&f->field_78[3], center);
  if (distance3 > radius)
    return 0;
  if (plane3d_distance_to_point((float *)&f->field_78[4], center) > radius)
    return 0;
  distance5 = plane3d_distance_to_point((float *)&f->field_78[5], center);
  if (distance5 > radius) {
    return 0;
  } else {
    int16_t result;

    negative_radius = -radius;
    if (distance0 < negative_radius && distance1 < negative_radius &&
        distance2 < negative_radius && distance3 < negative_radius &&
        distance5 < negative_radius) {
      result = 2;
    } else {
      result = 1;
    }

    return result;
  }
}

/* Project a world-space point into screen space.
 * Transforms the point into view space with the frustum's world-to-view
 * matrix (frustum + 0x10) and defers to render_camera_view_to_screen. */
char render_camera_world_to_screen(void *camera, float *frustum,
                                   float *world_point, float *screen_point)
{
  float view_point[3];

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c1, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c2, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c3,
                 world_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c4,
                 screen_point);

  matrix_transform_point(frustum + 4, world_point, view_point);
  return render_camera_view_to_screen((int *)camera, (int *)frustum, view_point,
                                      screen_point);
}

/* render_camera_debug_frustum - 0x186d40
 *
 * Debug overlay: when render_camera_debug_this_fucking_frustum (0x4d0e1a) is
 * set, transforms a 3x3 grid of unit-depth view points to world space and
 * draws the grid lines in red.
 *
 * Evidence (0x186d40..0x186ee3): asserts at lines 0x43b/0x43c; x is
 * FILD (window x1 - x0) * j * tan / (window y1 - y0); y is i * tan; points
 * are transformed through frustum->view_to_world (+0x44) by
 * matrix_transform_point (0x109590); lines via render_debug_line (0x189270)
 * with the global_real_argb_red pointer (0x2ee6d0).
 */
void render_camera_debug_frustum(camera_t *camera, void *frustum)
{
  render_frustum_t *f = (render_frustum_t *)frustum;
  vector3_t points[3][3];
  int16_t i;
  int16_t j;

  if (render_camera_debug_this_fucking_frustum) {
    assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x43b, camera);
    assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x43c,
                   frustum);

    for (i = -1; i < 2; i++) {
      for (j = -1; j < 2; j++) {
        real tangent;

#if defined(_MSC_VER) && !defined(__clang__)
        /* VC71 /Oi inlines tan as FPTAN (matches original). */
        tangent = (real)tan(camera->vertical_field_of_view * 0.5f);
#else
        tangent = x87_fptan(camera->vertical_field_of_view * 0.5f);
#endif
        points[j + 1][i + 1].x = (real)(camera->unk_52.x1 - camera->unk_52.x0) *
                                 j * tangent /
                                 (camera->unk_52.y1 - camera->unk_52.y0);
        points[j + 1][i + 1].y = (real)i * tangent;
        points[j + 1][i + 1].z = -1.0f;
        matrix_transform_point((float *)&f->field_44,
                               (float *)&points[j + 1][i + 1],
                               (float *)&points[j + 1][i + 1]);
      }
    }

    for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) {
        render_debug_line(1, (float *)&points[j][0], (float *)&points[i][2],
                     global_real_argb_red);
        render_debug_line(1, (float *)&points[0][j], (float *)&points[2][i],
                     global_real_argb_red);
      }
    }
  }
}

/* render_camera_mirror - 0x186ef0
 *
 * Builds the camera seen through a mirror (index_of_refraction == 0: reflect
 * forward/up and position across the plane, flip the mirrored byte and
 * negate up) or through a refracting surface (shift the position along the
 * plane normal by the Snell offset for the surface depth).  Always clears
 * z_near and records the mirror plane.
 *
 * Evidence (0x186ef0..0x18724c): plane copied to a local, camera copied by
 * REP MOVSD 0x15 dwords; grazing test fabs(dot) < 0.0125 (double constant
 * 0x2b1580); nudge 0.005859375f (0x2b1578); normalize3d (0x13010); asserts
 * at lines 0xcf (valid_real_plane3d, 0x10a480, TEST AL) and 0xd9/0xda
 * (valid_real_vector3d, 0x84a10, TEST AL); reflect_vector3d (0x10c8e0);
 * -2.0f (0x25eeac); magnitudes are inline FSQRT.
 */
void render_camera_mirror(camera_t *camera, render_mirror_t *mirror,
                          camera_t *result)
{
  real_plane3d plane = mirror->plane;
  real_plane3d adjusted_plane;
  vector3_t adjusted_normal;

  *result = *camera;

  if (mirror->index_of_refraction == 0.0f) {
    if (fabs(dot_product3d_inline((const vector3_t *)plane.normal,
                                  &camera->field_0c)) < 0.0125) {
      vector3_t point_on_plane;
      const vector3_t *forward = &camera->field_0c;
      real distance_to_plane =
        -plane3d_distance_to_point_inline(&plane, &camera->field_00);

      point_on_plane.x =
        plane.normal[0] * distance_to_plane + camera->field_00.x;
      point_on_plane.y =
        plane.normal[1] * distance_to_plane + camera->field_00.y;
      point_on_plane.z =
        plane.normal[2] * distance_to_plane + camera->field_00.z;
      adjusted_normal.x = forward->x * 0.005859375f + plane.normal[0];
      adjusted_normal.y = forward->y * 0.005859375f + plane.normal[1];
      adjusted_normal.z = forward->z * 0.005859375f + plane.normal[2];
      normalize3d((float *)&adjusted_normal);
      plane3d_from_point_and_normal_inline(&adjusted_plane, &point_on_plane,
                                           &adjusted_normal);

      assert_halt_msg_at("valid_real_plane3d(&adjusted_plane)",
                         "c:\\halo\\SOURCE\\render\\render_cameras.c", 0xcf,
                         (char)FUN_0010a480((int)&adjusted_plane));
    } else {
      adjusted_plane = plane;
    }

    FUN_0010c8e0((float *)&camera->field_0c, adjusted_plane.normal,
                 (float *)&result->field_0c);
    FUN_0010c8e0((float *)&camera->field_18, adjusted_plane.normal,
                 (float *)&result->field_18);

    assert_halt_msg_at("valid_real_vector3d(&result->forward)",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0xd9,
                       (char)real_vector3d_valid((float *)&result->field_0c));
    assert_halt_msg_at("valid_real_vector3d(&result->up)",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0xda,
                       (char)real_vector3d_valid((float *)&result->field_18));

    {
      real mirror_scale =
        plane3d_distance_to_point_inline(&adjusted_plane, &camera->field_00) *
        -2.0f;

      result->field_00.x =
        adjusted_plane.normal[0] * mirror_scale + camera->field_00.x;
      result->field_00.y =
        adjusted_plane.normal[1] * mirror_scale + camera->field_00.y;
      result->field_00.z =
        adjusted_plane.normal[2] * mirror_scale + camera->field_00.z;
    }
    result->unk_36 = !camera->unk_36;
    result->field_18.x = -result->field_18.x;
    result->field_18.y = -result->field_18.y;
    result->field_18.z = -result->field_18.z;
  } else {
    vector3_t cross_product;
    real inverse_forward_magnitude =
      1.0f / magnitude3d_inline(&camera->field_0c);
    real sine_of_incidence;
    real refracted_sine;
    real refraction_offset;

    cross_product3d_inline(&camera->field_0c, (const vector3_t *)plane.normal,
                           &cross_product);
    sine_of_incidence =
      magnitude3d_inline(&cross_product) * inverse_forward_magnitude;
    refracted_sine = mirror->index_of_refraction * sine_of_incidence;

    if (sine_of_incidence != 0.0f) {
      real cosine_of_incidence =
        dot_product3d_inline((const vector3_t *)plane.normal,
                             &camera->field_0c) *
        inverse_forward_magnitude;
      real refraction_numerator = cosine_of_incidence * refracted_sine;

      refraction_offset =
        -(refraction_numerator * mirror->depth /
          ((real)x87_sqrtd(1.0f - refracted_sine * refracted_sine) *
           sine_of_incidence));
    } else {
      refraction_offset = 0.0f;
    }

    result->field_00.x =
      plane.normal[0] * refraction_offset + camera->field_00.x;
    result->field_00.y =
      plane.normal[1] * refraction_offset + camera->field_00.y;
    result->field_00.z =
      plane.normal[2] * refraction_offset + camera->field_00.z;
  }

  result->z_near = 0.0f;
  *(real_plane3d *)result->field_44 = plane;
}

/* render_camera_build_frustum - 0x187250
 *
 * Builds the view frustum from a camera, optional frustum bounds, and a
 * projection flag: bounds, world_to_view / view_to_world matrices, the six
 * clip planes (left, right, bottom, top, near, far), z_near / z_far copies, the
 * four far-plane corners plus the camera position, the midpoint, the world
 * AABB of the five vertices, the optional projection matrix, and 22
 * frustum-integrity warning checks.
 *
 * Structure follows the PAL 2342 reconstruction, re-verified against
 * 0x187250..0x187f7a: assert lines 0x1ae..0x1b4 and 0x1ca..0x1cb, the bounds
 * struct copy (dword MOVs), per-vertex MIN/MAX stores (FCOMP / TEST AH,0x41 /
 * unconditional FSTP), and out-of-line warning calls with the id in EAX.
 */
void render_camera_build_frustum(camera_t *camera, float *bounds,
                                 float *frustum, bool do_projection)
{
  render_frustum_t *fr = (render_frustum_t *)frustum;
  int viewport_width_integer =
    camera->viewport_bounds.x1 - camera->viewport_bounds.x0;
  int viewport_height_integer =
    camera->viewport_bounds.y1 - camera->viewport_bounds.y0;
  real viewport_width = (real)viewport_width_integer;
  real viewport_height = (real)viewport_height_integer;
  real half_bounds_width;
  real half_bounds_height;
  real bounds_center_x;
  real bounds_center_y;
  real field_of_view_tangent;
  real projection_x_scale;
  real projection_y_scale;
  vector3_t view_left;
  vector3_t view_up;
  vector3_t view_backward;
  vector3_t plane_normal;
  real_plane3d view_plane;
  real left_plane_z;
  real bottom_plane_z;
  real inverse_projection_x_scale;
  real inverse_projection_y_scale;
  real half_z;
  real far_left;
  real far_right;
  real far_bottom;
  real far_top;
  vector3_t view_point;
  int vertex_index;

  if (bounds) {
    fr->field_00 = *(real_rectangle2d *)bounds;
  } else {
    fr->field_00.y0 = -1.0f;
    fr->field_00.x0 = -1.0f;
    fr->field_00.y1 = 1.0f;
    fr->field_00.x1 = 1.0f;
  }

  half_bounds_width = (fr->field_00.x1 - fr->field_00.x0) * 0.5f;
  half_bounds_height = (fr->field_00.y1 - fr->field_00.y0) * 0.5f;
  bounds_center_x =
    (fr->field_00.x0 + fr->field_00.x1) / half_bounds_width * -0.5f;
  bounds_center_y =
    (fr->field_00.y0 + fr->field_00.y1) / half_bounds_height * -0.5f;
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines tan as FPTAN (matches original). */
  field_of_view_tangent = (real)tan(camera->vertical_field_of_view * 0.5f);
#else
  field_of_view_tangent = x87_fptan(camera->vertical_field_of_view * 0.5f);
#endif
  projection_x_scale = 1.0f / (half_bounds_width / viewport_height *
                               viewport_width * field_of_view_tangent);
  projection_y_scale = 1.0f / (field_of_view_tangent * half_bounds_height);

  assert_halt_msg_at("camera->vertical_field_of_view<_pi - _real_epsilon",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1ae,
                     camera->vertical_field_of_view < *(float *)0x2b16f4);
  if (camera->vertical_field_of_view <= *(float *)0x253f44) {
    display_assert(csprintf((char *)0x5ab100,
                            "### FATAL ERROR: field of view set to %f (0x%x)",
                            (double)camera->vertical_field_of_view,
                            *(int *)&camera->vertical_field_of_view),
                   "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b0, true);
    system_exit(-1);
  }
  assert_halt_msg_at("camera->z_near>=0.0f",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b1,
                     camera->z_near >= 0.0f);
  assert_halt_msg_at("camera->z_far>camera->z_near",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b2,
                     camera->z_far > camera->z_near);
  assert_halt_msg_at("camera->viewport_bounds.x0<camera->viewport_bounds.x1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b3,
                     camera->viewport_bounds.x0 < camera->viewport_bounds.x1);
  assert_halt_msg_at("camera->viewport_bounds.y0<camera->viewport_bounds.y1",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b4,
                     camera->viewport_bounds.y0 < camera->viewport_bounds.y1);

  cross_product3d_inline(&camera->field_0c, &camera->field_18, &view_left);
  cross_product3d_inline(&view_left, &camera->field_0c, &view_up);
  view_backward.x = -camera->field_0c.x;
  view_backward.y = -camera->field_0c.y;
  view_backward.z = -camera->field_0c.z;
  normalize3d(&view_left.x);
  normalize3d(&view_up.x);
  normalize3d(&view_backward.x);
  fr->field_44.forward = view_left;
  fr->field_44.left = view_up;
  fr->field_44.up = view_backward;
  fr->field_44.position = camera->field_00;
  fr->field_44.scale = 1.0f;
  matrix_inverse(&fr->field_44.scale, &fr->field_10.scale);
  assert_halt_msg_at("valid_real_matrix4x3(&frustum->world_to_view)",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1ca,
                     valid_real_matrix4x3(&fr->field_10.scale));
  assert_halt_msg_at("valid_real_matrix4x3(&frustum->view_to_world)",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1cb,
                     valid_real_matrix4x3(&fr->field_44.scale));

  plane_normal.x = -projection_x_scale;
  plane_normal.y = 0.0f;
  left_plane_z = bounds_center_x + 1.0f;
  plane_normal.z = left_plane_z;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal, fr->field_78[0].normal);

  plane_normal.x = projection_x_scale;
  plane_normal.y = 0.0f;
  plane_normal.z = 1.0f - bounds_center_x;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal, fr->field_78[1].normal);

  plane_normal.x = 0.0f;
  plane_normal.y = -projection_y_scale;
  bottom_plane_z = bounds_center_y + 1.0f;
  plane_normal.z = bottom_plane_z;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal, fr->field_78[2].normal);

  plane_normal.x = 0.0f;
  plane_normal.y = projection_y_scale;
  plane_normal.z = 1.0f - bounds_center_y;
  normalize3d(&plane_normal.x);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal, fr->field_78[3].normal);

  view_plane.normal[0] = 0.0f;
  view_plane.normal[1] = 0.0f;
  view_plane.normal[2] = 1.0f;
  view_plane.d = -camera->z_near;
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal, fr->field_78[4].normal);

  view_plane.normal[0] = 0.0f;
  view_plane.normal[1] = 0.0f;
  view_plane.normal[2] = -1.0f;
  view_plane.d = camera->z_far;
  FUN_0010a1c0(&fr->field_44.scale, view_plane.normal, fr->field_78[5].normal);

  fr->field_d8 = camera->z_near;
  fr->field_dc = camera->z_far;
  inverse_projection_x_scale = 1.0f / projection_x_scale;
  inverse_projection_y_scale = 1.0f / projection_y_scale;
  half_z = (camera->z_far + camera->z_near) * 0.5f;
  far_left = left_plane_z * -(inverse_projection_x_scale * camera->z_far);
  far_right =
    (bounds_center_x - 1.0f) * -(inverse_projection_x_scale * camera->z_far);
  far_bottom = bottom_plane_z * -(inverse_projection_y_scale * camera->z_far);
  far_top =
    (bounds_center_y - 1.0f) * -(inverse_projection_y_scale * camera->z_far);

  view_point.x = far_left;
  view_point.y = far_bottom;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[0].x);
  view_point.x = far_right;
  view_point.y = far_bottom;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[1].x);
  view_point.x = far_left;
  view_point.y = far_top;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[2].x);
  view_point.x = far_right;
  view_point.y = far_top;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->field_44.scale, &view_point.x,
                         &fr->field_e0[3].x);

  fr->field_e0[4] = camera->field_00;
  view_point.x = -(inverse_projection_x_scale * half_z * bounds_center_x);
  view_point.y = -(inverse_projection_y_scale * half_z * bounds_center_y);
  view_point.z = -half_z;
  matrix_transform_point(&fr->field_44.scale, &view_point.x, &fr->field_11c.x);

  /* World AABB: x0,x1,y0,y1,z0,z1. */
  fr->field_128[0] = fr->field_128[1] = fr->field_e0[0].x;
  fr->field_128[2] = fr->field_128[3] = fr->field_e0[0].y;
  fr->field_128[4] = fr->field_128[5] = fr->field_e0[0].z;
  for (vertex_index = 1; vertex_index < 5; vertex_index++) {
    const vector3_t *vertex = &fr->field_e0[vertex_index];

    fr->field_128[0] =
      fr->field_128[0] > vertex->x ? vertex->x : fr->field_128[0];
    fr->field_128[2] =
      fr->field_128[2] > vertex->y ? vertex->y : fr->field_128[2];
    fr->field_128[4] =
      fr->field_128[4] > vertex->z ? vertex->z : fr->field_128[4];
    fr->field_128[1] =
      fr->field_128[1] > vertex->x ? fr->field_128[1] : vertex->x;
    fr->field_128[3] =
      fr->field_128[3] > vertex->y ? fr->field_128[3] : vertex->y;
    fr->field_128[5] =
      fr->field_128[5] > vertex->z ? fr->field_128[5] : vertex->z;
  }

  if (do_projection) {
    real inverse_plane_z;
    real clip_offset;
    real projection_scale;

    if (camera->z_near == 0.0f) {
      FUN_0010a1c0(&fr->field_10.scale, camera->field_44, view_plane.normal);
    } else {
      view_plane.normal[0] = 0.0f;
      view_plane.normal[1] = 0.0f;
      view_plane.normal[2] = 1.0f;
      view_plane.d = -camera->z_near;
    }

    inverse_plane_z = 1.0f / view_plane.normal[2];
    clip_offset = -(view_plane.d * inverse_plane_z);
    projection_scale =
      (real)(camera->z_far /
             ((camera->z_far - clip_offset) *
              (fabs(inverse_plane_z * view_plane.normal[0]) +
               fabs(inverse_plane_z * view_plane.normal[1]) + 1.0)));
    view_plane.normal[0] *= inverse_plane_z * projection_scale;
    view_plane.normal[1] *= inverse_plane_z * projection_scale;
    view_plane.normal[2] = projection_scale;
    view_plane.d = -(projection_scale * clip_offset);
    if (view_plane.d > 0.0f && camera->z_near == 0.0f) {
      view_plane.normal[0] = -view_plane.normal[0];
      view_plane.normal[1] = -view_plane.normal[1];
      view_plane.normal[2] = -view_plane.normal[2];
      view_plane.d = -view_plane.d;
    }

    /* 4x4 projection matrix, row-major [row * 4 + column]. */
    csmemset(fr->field_144, 0, sizeof(fr->field_144));
    fr->field_144[0] = projection_x_scale;
    fr->field_144[2] = -view_plane.normal[0];
    fr->field_144[5] = projection_y_scale;
    fr->field_144[6] = -view_plane.normal[1];
    fr->field_144[8] = -bounds_center_x;
    fr->field_144[9] = -bounds_center_y;
    fr->field_144[10] = -view_plane.normal[2];
    fr->field_144[11] = -1.0f;
    fr->field_144[14] = view_plane.d;
    fr->field_140 = 1;
    fr->field_184[0] = projection_x_scale * viewport_width * 0.5f;
    fr->field_184[1] = projection_y_scale * viewport_height * 0.5f;
  } else {
    csmemset(fr->field_144, 0, sizeof(fr->field_144));
    csmemset(fr->field_184, 0, sizeof(fr->field_184));
    fr->field_140 = 0;
  }

  /* Planes: 0 left, 1 right, 2 bottom, 3 top, 4 near, 5 far.
   * Vertices: 0 bottom-left, 1 bottom-right, 2 top-left, 3 top-right,
   * 4 apex (camera position). */
  render_camera_warn_once(
    0, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[0], &fr->field_e0[0])));
  render_camera_warn_once(
    1, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[0], &fr->field_e0[2])));
  render_camera_warn_once(
    2, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[0], &fr->field_e0[4])));
  render_camera_warn_once(
    3, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[1], &fr->field_e0[1])));
  render_camera_warn_once(
    4, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[1], &fr->field_e0[3])));
  render_camera_warn_once(
    5, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[1], &fr->field_e0[4])));
  render_camera_warn_once(
    6, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[2], &fr->field_e0[0])));
  render_camera_warn_once(
    7, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[2], &fr->field_e0[1])));
  render_camera_warn_once(
    8, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[2], &fr->field_e0[4])));
  render_camera_warn_once(
    9, (real)fabs(
         plane3d_distance_to_point_inline(&fr->field_78[3], &fr->field_e0[2])));
  render_camera_warn_once(
    10, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[3],
                                                    &fr->field_e0[3])));
  render_camera_warn_once(
    11, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[3],
                                                    &fr->field_e0[4])));
  render_camera_warn_once(
    12, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                                    &fr->field_e0[0])));
  render_camera_warn_once(
    13, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                                    &fr->field_e0[1])));
  render_camera_warn_once(
    14, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                                    &fr->field_e0[2])));
  render_camera_warn_once(
    15, (real)fabs(plane3d_distance_to_point_inline(&fr->field_78[5],
                                                    &fr->field_e0[3])));
  render_camera_warn_once(
    16, plane3d_distance_to_point_inline(&fr->field_78[0], &fr->field_11c));
  render_camera_warn_once(
    17, plane3d_distance_to_point_inline(&fr->field_78[1], &fr->field_11c));
  render_camera_warn_once(
    18, plane3d_distance_to_point_inline(&fr->field_78[2], &fr->field_11c));
  render_camera_warn_once(
    19, plane3d_distance_to_point_inline(&fr->field_78[3], &fr->field_11c));
  render_camera_warn_once(
    20, plane3d_distance_to_point_inline(&fr->field_78[4], &fr->field_11c));
  render_camera_warn_once(
    21, plane3d_distance_to_point_inline(&fr->field_78[5], &fr->field_11c));
}

/* contrail_fade - 0x187f80
 *
 * Angle fade for a contrail point: |normal . to_camera| / |to_camera|,
 * optionally eased (definition flag 0x40, transition function 2) and
 * inverted for fade mode 2.  fade_mode 0 returns 1.0f.
 *
 * Evidence (0x187f80..0x18800e): camera position read from
 * unknown_global_camera (0x506550); inline FSQRT magnitude; FABS; TEST byte
 * [definition],0x40; transition_function_evaluate (0x10a710); FSUBR 1.0f.
 */
float contrail_fade(contrail_definition_t *definition, int16_t fade_mode,
                    vector3_t *world_point, vector3_t *world_normal)
{
  real result = 1.0f;

  if (fade_mode) {
    vector3_t to_camera;

    to_camera.x = unknown_global_camera.field_00.x - world_point->x;
    to_camera.y = unknown_global_camera.field_00.y - world_point->y;
    to_camera.z = unknown_global_camera.field_00.z - world_point->z;
    result = (real)fabs(dot_product3d_inline(world_normal, &to_camera) /
                        magnitude3d_inline(&to_camera));

    if (definition->flags & 0x40) {
      result = transition_function_evaluate(2, result);
    }
    if (fade_mode == 2) {
      result = 1.0f - result;
    }
  }

  return result;
}

/* render_contrail - 0x188010
 *
 * Builds and draws one contrail instance as a triangle strip: two vertices
 * per point (color/width interpolated from the point's state, tinted by the
 * attached object's change color), orientation per render type, alpha faded
 * by contrail_fade and clamped to [0, 1].
 *
 * Evidence (0x188010..0x18878f): lock operation word 0x325652 set to 0xf and
 * cleared on exit; assert "triangles && vertices" at render_contrails.c line
 * 0x88; vertex stride 0x18; five-entry jump table at 0x188790 on
 * definition->render_type (0 vertical, 1/2 horizontal/media, 4 viewer;
 * 3 and out of range report an error).  The error arm returns without
 * releasing the buffers or clearing the lock operation (original bug,
 * preserved).  PAL 2342 names 0x17cf60 (kb: rasterizer_dynamic_unlit_geometry_draw)
 * rasterizer_dynamic_unlit_geometry_draw.
 */
typedef struct {
  vector3_t point; /* +0x00 */
  real texture[2]; /* +0x0c */
  uint32_t color; /* +0x14 */
} contrail_vertex_t;

void render_contrail(void *contrail_datum, void *contrail_definition,
                     int16_t point_index)
{
  contrail_datum_t *contrail = (contrail_datum_t *)contrail_datum;
  contrail_definition_t *definition =
    (contrail_definition_t *)contrail_definition;
  int16_t instance_index = point_index;
  void *bitmap;
  int16_t segment_count;
  int16_t triangle_count;
  int16_t vertex_count;
  int triangle_buffer_index;
  int vertex_buffer_index;

  bitmap = bitmap_group_get_bitmap_from_sequence(definition->bitmap_index, contrail->sequence_index,
                        contrail->frame_index);
  rasterizer_current_lock_operation = 0xf;
  if (xbox_texture_cache_get_hardware_format(bitmap, 0, 1)) {
    segment_count = contrail->contrail_point_counts[instance_index] - 1;
    triangle_count = segment_count + segment_count;
    vertex_count = triangle_count + 2;
    triangle_buffer_index = rasterizer_dynamic_triangles_new(triangle_count);
    vertex_buffer_index = rasterizer_dynamic_vertices_new(6, vertex_count);
    if (triangle_buffer_index != -1 && vertex_buffer_index != -1) {
      int16_t *triangles;
      contrail_vertex_t *vertices;
      contrail_vertex_t *first_vertex;
      contrail_shader_t *shader;
      bool apply_orientation;
      real texture_u_step;
      real texture_offset_v;
      real texture_v_far;
      vector3_t average_position;
      contrail_point_datum_t *previous_point;
      real texture_u;
      int point_datum_index;

      triangles = (int16_t *)rasterizer_dynamic_triangles_lock(triangle_buffer_index);
      vertices = (contrail_vertex_t *)rasterizer_dynamic_vertices_lock(
        vertex_buffer_index);
      average_position = *global_origin3d;
      shader = &definition->shader;
      apply_orientation = shader->framebuffer_fade_mode != 0;
      assert_halt_msg_at("triangles && vertices",
                         "c:\\halo\\SOURCE\\render\\render_contrails.c", 0x88,
                         triangles && vertices);

      texture_u = contrail->texture_offset_u;
      texture_u_step = definition->texture_repeats_u;
      if (definition->scale_flags & 0x40) {
        texture_u_step *= contrail->density;
      }
      texture_u_step = -texture_u_step;
      texture_offset_v = contrail->texture_offset_v;
      texture_v_far = definition->texture_repeats_v;
      if (definition->scale_flags & 0x80) {
        texture_v_far *= contrail->density;
      }
      texture_v_far += texture_offset_v;
      previous_point = NULL;
      point_datum_index =
        contrail->first_contrail_point_indices[instance_index];

      while (point_datum_index != -1) {
        contrail_point_datum_t *point;
        contrail_point_state_t *state;
        real color_scale;
        real width;
        real_argb_color color;
        real half_width;
        vector3_t orientation;
        vector3_t facing_normal;

        point = (contrail_point_datum_t *)datum_get(contrail_point_data,
                                                    point_datum_index);
        state = (contrail_point_state_t *)tag_block_get_element(
          &definition->states, point->state_index, 0x68);
        color_scale = 1.0f;
        if (state->scale_flags & 0x20) {
          color_scale = point->density;
        }
        width = state->width;
        if (state->scale_flags & 0x10) {
          width *= point->density;
        }
        half_width = width;
        color.alpha =
          (state->color_upper_bound.alpha - state->color_lower_bound.alpha) *
            color_scale +
          state->color_lower_bound.alpha;
        color.red =
          (state->color_upper_bound.red - state->color_lower_bound.red) *
            color_scale +
          state->color_lower_bound.red;
        color.green =
          (state->color_upper_bound.green - state->color_lower_bound.green) *
            color_scale +
          state->color_lower_bound.green;
        color.blue =
          (state->color_upper_bound.blue - state->color_lower_bound.blue) *
            color_scale +
          state->color_lower_bound.blue;

        if (point->flags & 0x2) {
          contrail_point_state_t *next_state;
          real transition;
          real next_color_scale;
          real next_width;
          real_argb_color next_color;

          next_state = (contrail_point_state_t *)tag_block_get_element(
            &definition->states, point->state_index + 1, 0x68);
          transition = point->time;
          next_color_scale = 1.0f;
          if (next_state->scale_flags & 0x20) {
            next_color_scale = point->density;
          }
          next_width = next_state->width;
          if (next_state->scale_flags & 0x10) {
            next_width *= point->density;
          }
          next_color.alpha = (next_state->color_upper_bound.alpha -
                              next_state->color_lower_bound.alpha) *
                               next_color_scale +
                             next_state->color_lower_bound.alpha;
          next_color.red = (next_state->color_upper_bound.red -
                            next_state->color_lower_bound.red) *
                             next_color_scale +
                           next_state->color_lower_bound.red;
          next_color.green = (next_state->color_upper_bound.green -
                              next_state->color_lower_bound.green) *
                               next_color_scale +
                             next_state->color_lower_bound.green;
          next_color.blue = (next_state->color_upper_bound.blue -
                             next_state->color_lower_bound.blue) *
                              next_color_scale +
                            next_state->color_lower_bound.blue;
          half_width = (next_width - width) * transition + width;
          color.alpha =
            (next_color.alpha - color.alpha) * transition + color.alpha;
          color.red = (next_color.red - color.red) * transition + color.red;
          color.green =
            (next_color.green - color.green) * transition + color.green;
          color.blue = (next_color.blue - color.blue) * transition + color.blue;
        }

        if (contrail->object_index != -1) {
          char *object =
            (char *)object_get_and_verify_type(contrail->object_index, -1);
          char *object_definition = (char *)tag_get(0x6f626a65, *(int *)object);
          /* object definition attachments block at +0x140 (element 0x48);
           * change_color_reference is the short at attachment +0x34. */
          char *attachment = (char *)tag_block_get_element(
            object_definition + 0x140, contrail->attachment_index, 0x48);
          int16_t change_color_index = *(int16_t *)(attachment + 0x34) - 1;

          if (change_color_index != -1) {
            /* object outgoing change colors: real_rgb_color[] at +0x168. */
            const real *change_color =
              (const real *)(object + 0x168) + change_color_index * 3;

            color.red *= change_color[0];
            color.green *= change_color[1];
            color.blue *= change_color[2];
          }
        }

        half_width *= 0.5f;
        vertices[0].texture[0] = texture_u;
        vertices[0].texture[1] = texture_v_far;
        vertices[1].texture[0] = texture_u;
        vertices[1].texture[1] = texture_offset_v;
        average_position.x += point->position.x;
        average_position.y += point->position.y;
        average_position.z += point->position.z;

        switch (definition->render_type) {
        case 0:
          vertices[0].point.x = point->position.x;
          vertices[0].point.y = point->position.y;
          vertices[0].point.z = point->position.z - half_width;
          vertices[1].point.x = point->position.x;
          vertices[1].point.y = point->position.y;
          vertices[1].point.z = point->position.z + half_width;
          if (apply_orientation) {
            contrail_point_datum_t *next_point;

            if (previous_point) {
              orientation.x = previous_point->position.y - point->position.y;
              orientation.y = point->position.x - previous_point->position.x;
            } else {
              next_point = (contrail_point_datum_t *)datum_get(
                contrail_point_data, point->next_contrail_point_index);
              orientation.x = point->position.y - next_point->position.y;
              orientation.y = next_point->position.x - point->position.x;
            }
            orientation.z = 0.0f;
            normalize3d((float *)&orientation);
          }
          break;

        case 1:
        case 2: {
          real perpendicular[2];
          contrail_point_datum_t *next_point;

          if (previous_point) {
            perpendicular[0] = previous_point->position.y - point->position.y;
            perpendicular[1] = point->position.x - previous_point->position.x;
          } else {
            next_point = (contrail_point_datum_t *)datum_get(
              contrail_point_data, point->next_contrail_point_index);
            perpendicular[0] = point->position.y - next_point->position.y;
            perpendicular[1] = next_point->position.x - point->position.x;
          }
          normalize2d(perpendicular);
          vertices[0].point.x =
            point->position.x - perpendicular[0] * half_width;
          vertices[0].point.y =
            point->position.y - perpendicular[1] * half_width;
          vertices[0].point.z = point->position.z;
          vertices[1].point.x =
            point->position.x + perpendicular[0] * half_width;
          vertices[1].point.y =
            point->position.y + perpendicular[1] * half_width;
          vertices[1].point.z = point->position.z;
          if (apply_orientation) {
            orientation = *(vector3_t *)global_up_vector_ptr;
          }
          break;
        }

        case 4: {
          vector3_t tangent;
          real eye_i;
          real eye_j;
          real eye_k;
          contrail_point_datum_t *next_point;

          if (previous_point) {
            eye_i =
              unknown_global_camera.field_00.x - previous_point->position.x;
            eye_j =
              unknown_global_camera.field_00.y - previous_point->position.y;
            eye_k =
              unknown_global_camera.field_00.z - previous_point->position.z;
            tangent.x = point->position.x - previous_point->position.x;
            tangent.y = point->position.y - previous_point->position.y;
            tangent.z = point->position.z - previous_point->position.z;
          } else {
            next_point = (contrail_point_datum_t *)datum_get(
              contrail_point_data, point->next_contrail_point_index);
            eye_i = unknown_global_camera.field_00.x - point->position.x;
            eye_j = unknown_global_camera.field_00.y - point->position.y;
            eye_k = unknown_global_camera.field_00.z - point->position.z;
            tangent.x = next_point->position.x - point->position.x;
            tangent.y = next_point->position.y - point->position.y;
            tangent.z = next_point->position.z - point->position.z;
          }
          facing_normal.x = tangent.z * eye_j - tangent.y * eye_k;
          facing_normal.y = tangent.x * eye_k - tangent.z * eye_i;
          facing_normal.z = tangent.y * eye_i - tangent.x * eye_j;
          normalize3d((float *)&facing_normal);
          vertices[0].point.x =
            point->position.x - facing_normal.x * half_width;
          vertices[0].point.y =
            point->position.y - facing_normal.y * half_width;
          vertices[0].point.z =
            point->position.z - facing_normal.z * half_width;
          vertices[1].point.x =
            point->position.x + facing_normal.x * half_width;
          vertices[1].point.y =
            point->position.y + facing_normal.y * half_width;
          vertices[1].point.z =
            point->position.z + facing_normal.z * half_width;
          if (apply_orientation) {
            orientation.x =
              facing_normal.z * tangent.y - facing_normal.y * tangent.z;
            orientation.y =
              facing_normal.x * tangent.z - facing_normal.z * tangent.x;
            orientation.z =
              facing_normal.y * tangent.x - facing_normal.x * tangent.y;
            normalize3d((float *)&orientation);
          }
          break;
        }

        case 3:
        default:
          error(2, "contrail %s uses an unsupported render type.",
                tag_get_name(contrail->definition_index));
          /* Original bug preserved: skips the buffer unlock/delete calls and
           * leaves the lock operation set. */
          return;
        }

        color.alpha *= contrail_fade(definition, shader->framebuffer_fade_mode,
                                     &point->position, &orientation);
        if (color.alpha < 0.0f) {
          color.alpha = 0.0f;
        } else if (color.alpha > 1.0f) {
          color.alpha = 1.0f;
        }
        vertices[0].color = vertices[1].color =
          real_argb_color_to_pixel32((float *)&color);
        texture_u += texture_u_step;
        previous_point = point;
        vertices += 2;
        point_datum_index = point->next_contrail_point_index;
      }

      vertices -= vertex_count;
      first_vertex = vertices;
      if (!(definition->flags & 0x1)) {
        ((uint8_t *)&first_vertex[0].color)[3] = 0;
        ((uint8_t *)&first_vertex[1].color)[3] = 0;
      }
      if (!(definition->flags & 0x2)) {
        ((uint8_t *)&vertices[vertex_count - 1].color)[3] = 0;
        ((uint8_t *)&vertices[vertex_count - 2].color)[3] = 0;
      }
      if (segment_count > 0) {
        int16_t segment_index;

        for (segment_index = 0; segment_index < segment_count;
             segment_index++) {
          triangles[0] = 2 * segment_index;
          triangles[1] = 2 * segment_index + 1;
          triangles[2] = 2 * segment_index + 2;
          triangles += 3;
          triangles[0] = 2 * segment_index + 2;
          triangles[1] = 2 * segment_index + 1;
          triangles[2] = 2 * segment_index + 3;
          triangles += 3;
        }
      }
      {
        real one_over_point_count =
          1.0f / contrail->contrail_point_counts[instance_index];

        average_position.x *= one_over_point_count;
        average_position.y *= one_over_point_count;
        average_position.z *= one_over_point_count;
      }
      rasterizer_dynamic_triangles_unlock(triangle_buffer_index);
      rasterizer_dynamic_vertices_unlock(vertex_buffer_index);
      rasterizer_dynamic_unlit_geometry_draw((uint32_t)shader, (uint32_t)bitmap, 0, triangle_buffer_index,
                   (uint32_t)vertex_buffer_index, triangle_count,
                   (float *)&average_position, 0);
      rasterizer_dynamic_triangles_delete(triangle_buffer_index);
      rasterizer_dynamic_vertices_delete(vertex_buffer_index);
    }
  }
  rasterizer_current_lock_operation = 0;
}

/* render_contrails - 0x1887b0
 *
 * Walks the live contrail pool and draws each contrail sub-trail whose
 * definition selects one of the render passes named by the incoming mask.
 *
 * The gate byte at 0x32574a is one of the four effect-enable bytes that
 * render_effects (0x184b60) writes together.
 *
 * Evidence (0x1887b0..0x18885f):
 *   - 001887b6 MOV AL,[0x0032574a] / TEST AL,AL / JZ end.
 *   - 001887c3/001887e7/00188842 MOV from [0x005aa8c0] = contrail_data.
 *   - 001887f6 MOV EDX,[EDI+4] = contrail definition_index, passed to
 *     tag_get with group 'cont' (0x636f6e74).
 *   - 0018880c LEA EBX,[EDI+0x2c] = contrail_point_counts[4]; the loop
 *     counter is 16-bit (00188839 CMP SI,0x4 / JL).
 *   - 00188810 MOV CL,[EAX+0x18] is reloaded from [EBP-8] every iteration
 *     (0018882f), so the shift is recomputed inside the inner loop.
 *   - 00188827 PUSH ESI / PUSH EAX / PUSH EDI / CALL 0x00188010 /
 *     ADD ESP,0xc => render_contrail(contrail, definition, index) cdecl.
 *
 * The contrail element type lives in effects/contrails.c and is not visible
 * here, so the two touched offsets are dereferenced raw; the definition tag
 * has no recovered type at all (+0x18 is the only field observed).
 */
void render_contrails(uint32_t render_pass_mask)
{
  int contrail_index;
  char *contrail;
  char *definition;
  int16_t *point_counts;
  int16_t index;

  if (*(char *)0x32574a == 0) {
    return;
  }

  for (contrail_index = data_next_index(contrail_data, -1);
       contrail_index != -1;
       contrail_index = data_next_index(contrail_data, contrail_index)) {
    contrail = (char *)datum_get(contrail_data, contrail_index);
    definition = (char *)tag_get(0x636f6e74, *(int *)(contrail + 4));

    point_counts = (int16_t *)(contrail + 0x2c);
    index = 0;
    do {
      /* 00188818 SHL EDX,CL with no preceding AND: the reference relies on
       * the x86 implicit shift-count mask, so no & 0x1f is written here. */
      if ((render_pass_mask &
           ((uint32_t)1 << *(uint8_t *)(definition + 0x18))) != 0 &&
          *point_counts > 1) {
        render_contrail(contrail, definition, index);
      }
      index++;
      point_counts++;
    } while (index < 4);
  }
}

/* render_contrails_normal - 0x188880
 *
 * Trivial wrapper: selects the "normal" contrail render passes and tail-calls
 * render_contrails.
 *
 * Evidence (0x188880..0x188888):
 *   00188880 PUSH -0xd       ; render_pass_mask = 0xfffffff3
 *   00188882 CALL 0x001887b0 ; render_contrails
 *   00188887 POP ECX         ; cdecl cleanup of the single dword arg
 *   00188888 RET
 *
 * The mask is the one's complement of 0x0000000c, i.e. every pass except the
 * two selected by bits 2 and 3. The pass-bit meanings are not recovered, so
 * the constant is written as the literal the binary pushes.
 */
void render_contrails_normal(void)
{
  render_contrails(0xfffffff3);
}
