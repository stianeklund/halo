/* Render camera utilities. */

#include "x87_math.h"

#define k_real_zero (*(const float *)0x2533c0)
#define k_real_negative_one (*(const float *)0x255e94)
#define k_minimum_projected_depth (*(const float *)0x25496c)
#define k_real_half (*(const float *)0x253398)
#define k_field_of_view_tangent_scale (*(const float *)0x2b1504)
#define k_maximum_vertical_field_of_view (*(const float *)0x2b16f4)
#define MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS 64
#define temporary ((char *)0x5ab100)

/* keep: copied as dwords (MOV, not FLD/FSTP) */
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

#define global_origin3d (*(real_point3d **)0x31fc1c)
#define debug_no_frustum_clip (*(bool *)0x4d0e19)
#define render_camera_debug_this_fucking_frustum (*(bool *)0x4d0e1a)
#define global_real_argb_red (*(void **)0x2ee6d0)
#define rasterizer_current_lock_operation (*(int16_t *)0x325652)
#define render_contrails_enabled (*(boolean *)0x32574a)
#define TAG_GROUP_CONT 0x636f6e74
#define NUMBER_OF_OBJECT_CHANGE_COLORS 4

/* Object datum view: definition index and the outgoing change colors. */
typedef struct contrail_object {
  int32_t definition_index;
  char pad_04[0x168 - 0x04];
  real_rgb_color change_colors[NUMBER_OF_OBJECT_CHANGE_COLORS];
} contrail_object_t;

/* Object definition view: the attachments block. */
typedef struct contrail_object_definition {
  char pad_00[0x140];
  tag_block attachments;
} contrail_object_definition_t;

/* Object definition attachment element. */
typedef struct contrail_object_attachment {
  char pad_00[0x34];
  int16_t change_color;
  char pad_36[0x48 - 0x36];
} contrail_object_attachment_t;
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* The engine builds with -fno-builtin, so clang would lower fabs() to a CRT
 * call; the original (and VC71 /Oi) inlines FABS. */
#if !(defined(_MSC_VER) && !defined(__clang__))
#define fabs(x) __builtin_fabs(x)
#endif

/* Inline expansions of the out-of-line math helpers kb.json already names. */
static __inline real_vector3d *cross_product3d_inline(const real_vector3d *a,
                                                      const real_vector3d *b,
                                                      real_vector3d *result)
{
  real k = a->i * b->j - a->j * b->i;
  real j = a->k * b->i - a->i * b->k;
  real i = a->j * b->k - a->k * b->j;

  result->i = i;
  result->j = j;
  result->k = k;
  return result;
}

static __inline real dot_product3d_inline(const real_vector3d *a,
                                          const real_vector3d *b)
{
  return a->i * b->i + a->j * b->j + a->k * b->k;
}

static __inline real point_dot_vector3d_inline(const real_point3d *point,
                                               const real_vector3d *vector)
{
  return point->x * vector->i + point->y * vector->j + point->z * vector->k;
}

static __inline real magnitude3d_inline(const real_vector3d *v)
{
  return (real)x87_sqrtd(dot_product3d_inline(v, v));
}

static __inline real_plane3d *plane3d_from_point_and_normal_inline(
  real_plane3d *plane, const real_point3d *point, const real_vector3d *normal)
{
  plane->normal = *normal;
  plane->d = point_dot_vector3d_inline(point, normal);
  return plane;
}

static __inline real plane3d_distance_to_point_inline(const real_plane3d *plane,
                                                      const real_point3d *point)
{
  return point_dot_vector3d_inline(point, &plane->normal) - plane->d;
}

__declspec(noinline) void render_camera_check_warning_condition(int16_t id,
                                                                float value)
{
  assert_halt_msg_at("id>=0 && id<MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS", "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x87, id >= 0 && id < MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS);

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

void render_camera_new(camera_t *camera)
{
  csmemset(camera, 0, sizeof(camera_t));
}

void render_camera_hack_frustum_z(render_frustum_t *frustum, float near_z,
                                  float far_z)
{
  float denom_held;
  float denom;

  assert_halt_msg_at("frustum && frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x10f,
                     frustum != 0 && frustum->projection_valid != 0);

  if (near_z == k_real_negative_one && far_z == k_real_negative_one) {
    render_camera_saved_frustum_z[0] = *(uint32_t *)&frustum->projection_matrix[0][2];
    render_camera_saved_frustum_z[1] = *(uint32_t *)&frustum->projection_matrix[1][2];
    render_camera_saved_frustum_z[2] = *(uint32_t *)&frustum->projection_matrix[2][2];
    render_camera_saved_frustum_z[3] = *(uint32_t *)&frustum->projection_matrix[3][2];
    return;
  }

  if (near_z == k_real_zero && far_z == k_real_zero) {
    *(uint32_t *)&frustum->projection_matrix[0][2] = render_camera_saved_frustum_z[0];
    *(uint32_t *)&frustum->projection_matrix[1][2] = render_camera_saved_frustum_z[1];
    *(uint32_t *)&frustum->projection_matrix[2][2] = render_camera_saved_frustum_z[2];
    *(uint32_t *)&frustum->projection_matrix[3][2] = render_camera_saved_frustum_z[3];
    return;
  }

  denom = far_z - near_z;
  *(uint32_t *)&frustum->projection_matrix[0][2] = 0;
  *(uint32_t *)&frustum->projection_matrix[1][2] = 0;
  /* The original leaves the single subtraction result on the x87 stack (ST1)
   * across both divides and pops it once at the end; the second reference to
   * the divisor is written through a second local so the divisor stays held
   * rather than being recomputed/reloaded per divide (+3.1pp VC71). */
  denom_held = denom;
  frustum->projection_matrix[2][2] = -((near_z + far_z) / denom);
  frustum->projection_matrix[3][2] = (near_z * far_z * -2.0f) / denom_held;
}

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
  window_bounds = &camera->window_bounds;
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

float render_frustum_sphere_diameter_in_pixels(const render_frustum_t *frustum,
                                               const real_point3d *center,
                                               float radius)
{
  float depth;
  float scaled;

  depth = frustum->world_to_view.up.k * center->z +
          frustum->world_to_view.left.k * center->y +
          frustum->world_to_view.forward.k * center->x +
          frustum->world_to_view.position.z;

  if (!(depth >= k_real_zero)) {
    depth = -depth;
  }
  if (!(depth > k_minimum_projected_depth)) {
    depth = k_minimum_projected_depth;
  }

  scaled = (frustum->projection_world_to_screen[1] / depth) * radius;
  return scaled + scaled;
}

float render_frustum_cube_view_fraction(const render_frustum_t *frustum,
                                        const real_rectangle3d *bounds)
{
  const render_frustum_t *f = frustum;
  const real_rectangle3d *box = bounds;
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
    real projection_x = f->projection_matrix[0][0];
    real projection_offset_x = f->projection_matrix[2][0];
    real projection_y = f->projection_matrix[1][1];
    real projection_offset_y = f->projection_matrix[2][1];
    real inverse_z1 = 1.0f / box->z1;
    real left;
    real bottom;
    real right;
    real top;
    real near_extent;
    real far_extent;

    assert_halt_msg_at("frustum->projection_valid",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x384,
                       f->projection_valid);
    assert_halt_msg_at("frustum->projection_matrix[1][0]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x387,
                       f->projection_matrix[1][0] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[3][0]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x388,
                       f->projection_matrix[3][0] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[0][1]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x389,
                       f->projection_matrix[0][1] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[3][1]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38a,
                       f->projection_matrix[3][1] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[0][3]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38b,
                       f->projection_matrix[0][3] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[1][3]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38c,
                       f->projection_matrix[1][3] == 0.0f);
    assert_halt_msg_at("frustum->projection_matrix[2][3]==-1.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38d,
                       f->projection_matrix[2][3] == -1.0f);
    assert_halt_msg_at("frustum->projection_matrix[3][3]==0.0f",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x38e,
                       f->projection_matrix[3][3] == 0.0f);

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

void render_frustum_get_projection_bounds(const render_frustum_t *frustum,
                                          real_rectangle2d *bounds)
{
  const render_frustum_t *f = frustum;
  real_rectangle2d *projection_bounds = bounds;
  real x;
  real y;
  real inverse_x_scale;
  real inverse_y_scale;

  assert_halt_msg_at("frustum && frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3a7,
                     f && f->projection_valid);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3a8, bounds);

  x = -f->projection_matrix[2][0];
  y = -f->projection_matrix[2][1];
  inverse_x_scale = -1.0f / f->projection_matrix[0][0];
  inverse_y_scale = -1.0f / f->projection_matrix[1][1];

  projection_bounds->x0 = (x - 1.0f) * inverse_x_scale;
  projection_bounds->x1 = (x + 1.0f) * inverse_x_scale;
  projection_bounds->y0 = (y - 1.0f) * inverse_y_scale;
  projection_bounds->y1 = (y + 1.0f) * inverse_y_scale;
}

char render_camera_view_to_screen(const camera_t *camera,
                                  const render_frustum_t *frustum,
                                  const real_point3d *view_point,
                                  real_point2d *screen_point)
{
  const camera_t *c = camera;
  const render_frustum_t *f = frustum;
  const real_point3d *point = view_point;
  real_point2d *screen = screen_point;
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
                       f->projection_valid);

    screen->x =
      (f->projection_matrix[0][0] * point->x + f->projection_matrix[2][0] * point->z) * inverse_depth;
    screen->y = -(f->projection_matrix[1][1] * point->y + f->projection_matrix[2][1] * point->z) *
                inverse_depth;
    if (screen->x >= -1.0f && screen->x <= 1.0f && screen->y >= -1.0f &&
        screen->y <= 1.0f) {
      real viewport_height =
        (real)(c->viewport_bounds.y1 - c->viewport_bounds.y0);
      real viewport_width =
        (real)(c->viewport_bounds.x1 - c->viewport_bounds.x0);

      screen->x =
        viewport_width * ((screen->x + 1.0f) * 0.5f) + c->viewport_bounds.x0;
      screen->y =
        ((screen->y + 1.0f) * 0.5f) * viewport_height + c->viewport_bounds.y0;
      result = 1;
    }
  }

  return result;
}

void render_camera_screen_to_view(const camera_t *camera,
                                  const render_frustum_t *frustum,
                                  const real_point2d *screen_point,
                                  real_vector3d *view_vector)
{
  const render_frustum_t *f = frustum;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fa, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fb, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fc,
                 screen_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3fd,
                 view_vector);
  assert_halt_msg_at("frustum->projection_valid",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x404,
                     f->projection_valid);

  view_vector->i = (screen_point->x - f->projection_matrix[2][0]) /
                   f->projection_matrix[0][0];
  view_vector->j = -((f->projection_matrix[2][1] + screen_point->y) /
                     f->projection_matrix[1][1]);
  view_vector->k = -1.0f;
}

void render_camera_screen_to_world(const camera_t *camera,
                                   const render_frustum_t *frustum,
                                   const real_point2d *screen_point,
                                   real_point3d *world_point,
                                   real_vector3d *world_vector)
{
  real_vector3d view_vector;

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
                     frustum->projection_valid != 0);

  render_camera_screen_to_view(camera, frustum, screen_point, &view_vector);

  *world_point = camera->position;

  matrix_scale_transform_vector(&frustum->view_to_world, &view_vector,
                                world_vector);
}

double render_camera_get_adjusted_field_of_view_tangent(float fov)
{
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi inlines tan as FPTAN (matches original). */
  return tan(fov * k_real_half) * k_field_of_view_tangent_scale;
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

bool render_camera_build_clipped_frustum_bounds(const camera_t *camera,
                                                const real_rectangle2d *clip,
                                                real_rectangle2d *frustum_bounds)
{
  bool use_full_bounds = 1;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x156, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x157, clip);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x158,
                 frustum_bounds);

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

bool render_camera_triangle_frontfacing(camera_t *camera, real_point3d *point0,
                                        real_point3d *point1, real_point3d *point2)
{
  real_vector3d camera_to_point;
  real_vector3d edge0;
  real_vector3d edge1;
  real_vector3d normal;

  camera_to_point.i = point0->x - camera->position.x;
  camera_to_point.j = point0->y - camera->position.y;
  camera_to_point.k = point0->z - camera->position.z;
  edge0.i = point1->x - point0->x;
  edge0.j = point1->y - point0->y;
  edge0.k = point1->z - point0->z;
  edge1.i = point2->x - point1->x;
  edge1.j = point2->y - point1->y;
  edge1.k = point2->z - point1->z;
  cross_product3d_inline(&edge0, &edge1, &normal);

  /* Summed z, y, then x: the order the x87 code accumulates the dot in. */
  return normal.k * camera_to_point.k + normal.j * camera_to_point.j +
           normal.i * camera_to_point.i >
         -0.0001f;
}

int16_t render_frustum_build_point_flags(const render_frustum_t *frustum,
                                         const real_point3d *point)
{
  const render_frustum_t *f = frustum;
  const real_point3d *p = point;
  /* Hand-summed: the x87 code adds the y and z products first, x last. */
  const real_plane3d *plane = f->world_planes;
  real distance;
  int16_t flags;

  distance = p->y * plane[0].normal.j;
  distance += p->z * plane[0].normal.k;
  distance += p->x * plane[0].normal.i;
  flags = distance - plane[0].d > 0.0f ?
            FLAG(_render_frustum_point_flags_left_bit) :
            0;
  distance = p->y * plane[1].normal.j;
  distance += p->z * plane[1].normal.k;
  distance += p->x * plane[1].normal.i;
  flags |= distance - plane[1].d > 0.0f ?
             FLAG(_render_frustum_point_flags_right_bit) :
             0;
  distance = p->y * plane[2].normal.j;
  distance += p->z * plane[2].normal.k;
  distance += p->x * plane[2].normal.i;
  flags |= distance - plane[2].d > 0.0f ?
             FLAG(_render_frustum_point_flags_bottom_bit) :
             0;
  distance = p->y * plane[3].normal.j;
  distance += p->z * plane[3].normal.k;
  distance += p->x * plane[3].normal.i;
  flags |= distance - plane[3].d > 0.0f ?
             FLAG(_render_frustum_point_flags_top_bit) :
             0;
  return flags;
}

bool render_frustum_triangle_visible(const render_frustum_t *frustum,
                                     const real_point3d *point0,
                                     const real_point3d *point1,
                                     const real_point3d *point2)
{
  int16_t flags;
  int16_t point_flags;

  point_flags = render_frustum_build_point_flags(frustum, point0);
  if (point_flags == 0) {
    return 1;
  }
  flags = (int16_t)(point_flags & 0x3f);
  point_flags = render_frustum_build_point_flags(frustum, point1);
  if (point_flags == 0) {
    return 1;
  }
  flags &= point_flags;
  point_flags = render_frustum_build_point_flags(frustum, point2);
  if (point_flags == 0) {
    return 1;
  }
  return (int16_t)(point_flags & flags) == 0;
}

int16_t render_frustum_cube_visible(const render_frustum_t *frustum,
                                    const real_rectangle3d *bounds,
                                    bool test_frustum_against_cube)
{
  const render_frustum_t *f = frustum;
  const real_rectangle3d *box = bounds;

  if (f->world_bounds.x1 < box->x0) {
    return 0;
  }
  if (f->world_bounds.y1 < box->y0) {
    return 0;
  }
  if (f->world_bounds.z1 < box->z0) {
    return 0;
  }
  if (f->world_bounds.x0 > box->x1) {
    return 0;
  }
  if (f->world_bounds.y0 > box->y1) {
    return 0;
  }
  if (f->world_bounds.z0 > box->z1) {
    return 0;
  }

  {
    real_point3d cube_vertices[8];
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
      const real_point3d *vertex = &cube_vertices[vertex_index];
      /* Per-plane x87 sum order: x,y,z; x,z,y; z,y,x; z,y,x. */
      const real_plane3d *plane = f->world_planes;
      real distance;
      uint16_t flags;

      distance = vertex->x * plane[0].normal.i;
      distance += vertex->y * plane[0].normal.j;
      distance += plane[0].normal.k * vertex->z;
      flags = distance - plane[0].d > 0.0f ?
                FLAG(_render_frustum_point_flags_left_bit) :
                0;
      distance = plane[1].normal.i * vertex->x;
      distance += plane[1].normal.k * vertex->z;
      distance += vertex->y * plane[1].normal.j;
      flags |= distance - plane[1].d > 0.0f ?
                 FLAG(_render_frustum_point_flags_right_bit) :
                 0;
      distance = plane[2].normal.k * vertex->z;
      distance += vertex->y * plane[2].normal.j;
      distance += plane[2].normal.i * vertex->x;
      flags |= distance - plane[2].d > 0.0f ?
                 FLAG(_render_frustum_point_flags_bottom_bit) :
                 0;
      distance = plane[3].normal.k * vertex->z;
      distance += vertex->y * plane[3].normal.j;
      distance += plane[3].normal.i * vertex->x;
      flags |= distance - plane[3].d > 0.0f ?
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
        const real_point3d *vertex = &f->world_vertices[vertex_index];
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

int16_t render_frustum_sphere_visible(const render_frustum_t *frustum,
                                      const real_point3d *center, float radius)
{
  const render_frustum_t *f = frustum;
  real distance0;
  real distance1;
  real distance2;
  real distance3;
  real distance5;
  real negative_radius;
  int16_t result;

  if (f->world_bounds.x1 < center->x - radius ||
      f->world_bounds.y1 < center->y - radius ||
      f->world_bounds.z1 < center->z - radius ||
      f->world_bounds.x0 > center->x + radius ||
      f->world_bounds.y0 > center->y + radius ||
      f->world_bounds.z0 > center->z + radius) {
    return 0;
  }

  distance0 = plane3d_distance_to_point(&f->world_planes[0], center);
  if (distance0 > radius)
    return 0;
  distance1 = plane3d_distance_to_point(&f->world_planes[1], center);
  if (distance1 > radius)
    return 0;
  distance2 = plane3d_distance_to_point(&f->world_planes[2], center);
  if (distance2 > radius)
    return 0;
  distance3 = plane3d_distance_to_point(&f->world_planes[3], center);
  if (distance3 > radius)
    return 0;
  if (plane3d_distance_to_point(&f->world_planes[4], center) > radius)
    return 0;
  distance5 = plane3d_distance_to_point(&f->world_planes[5], center);
  if (distance5 > radius) {
    result = 0;
  } else {
    negative_radius = -radius;
    if (distance0 < negative_radius && distance1 < negative_radius &&
        distance2 < negative_radius && distance3 < negative_radius &&
        distance5 < negative_radius) {
      result = 2;
    } else {
      result = 1;
    }
  }

  return result;
}

char render_camera_world_to_screen(const camera_t *camera,
                                   const render_frustum_t *frustum,
                                   const real_point3d *world_point,
                                   real_point2d *screen_point)
{
  real_point3d view_point;

  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c1, camera);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c2, frustum);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c3,
                 world_point);
  assert_halt_at("c:\\halo\\SOURCE\\render\\render_cameras.c", 0x3c4,
                 screen_point);

  matrix_transform_point(&frustum->world_to_view, world_point,
                         &view_point);
  return render_camera_view_to_screen(camera, frustum, &view_point,
                                      screen_point);
}

void render_camera_debug_frustum(const camera_t *camera,
                                 const render_frustum_t *frustum)
{
  real_point3d points[3][3];
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
        points[j + 1][i + 1].x = (real)(camera->window_bounds.x1 - camera->window_bounds.x0) *
                                 j * tangent /
                                 (camera->window_bounds.y1 - camera->window_bounds.y0);
        points[j + 1][i + 1].y = (real)i * tangent;
        points[j + 1][i + 1].z = -1.0f;
        matrix_transform_point(&frustum->view_to_world,
                               &points[j + 1][i + 1],
                               &points[j + 1][i + 1]);
      }
    }

    for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) {
        render_debug_line(1, &points[j][0], &points[i][2],
                     global_real_argb_red);
        render_debug_line(1, &points[0][j], &points[2][i],
                     global_real_argb_red);
      }
    }
  }
}

void render_camera_mirror(camera_t *camera, render_mirror_t *mirror,
                          camera_t *result)
{
  real_plane3d plane = mirror->plane;
  real_plane3d adjusted_plane;
  real_vector3d adjusted_normal;

  *result = *camera;

  if (mirror->index_of_refraction == 0.0f) {
    if (fabs(dot_product3d_inline(&plane.normal,
                                  &camera->forward)) < 0.0125f) {
      real_point3d point_on_plane;
      const real_vector3d *forward = &camera->forward;
      real distance_to_plane =
        -plane3d_distance_to_point_inline(&plane, &camera->position);

      point_on_plane.x =
        plane.normal.i * distance_to_plane + camera->position.x;
      point_on_plane.y =
        plane.normal.j * distance_to_plane + camera->position.y;
      point_on_plane.z =
        plane.normal.k * distance_to_plane + camera->position.z;
      adjusted_normal.i = forward->i * 0.005859375f + plane.normal.i;
      adjusted_normal.j = forward->j * 0.005859375f + plane.normal.j;
      adjusted_normal.k = forward->k * 0.005859375f + plane.normal.k;
      normalize3d(&adjusted_normal);
      plane3d_from_point_and_normal_inline(&adjusted_plane, &point_on_plane,
                                           &adjusted_normal);

      assert_halt_msg_at("valid_real_plane3d(&adjusted_plane)",
                         "c:\\halo\\SOURCE\\render\\render_cameras.c", 0xcf,
                         (char)FUN_0010a480((int)&adjusted_plane));
    } else {
      adjusted_plane = plane;
    }

    reflect_vector3d(&camera->forward, &adjusted_plane.normal,
                 &result->forward);
    reflect_vector3d(&camera->up, &adjusted_plane.normal,
                 &result->up);

    assert_halt_msg_at("valid_real_vector3d(&result->forward)",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0xd9,
                       (char)real_vector3d_valid(&result->forward));
    assert_halt_msg_at("valid_real_vector3d(&result->up)",
                       "c:\\halo\\SOURCE\\render\\render_cameras.c", 0xda,
                       (char)real_vector3d_valid(&result->up));

    {
      real mirror_scale =
        plane3d_distance_to_point_inline(&adjusted_plane, &camera->position) *
        -2.0f;

      result->position.x =
        adjusted_plane.normal.i * mirror_scale + camera->position.x;
      result->position.y =
        adjusted_plane.normal.j * mirror_scale + camera->position.y;
      result->position.z =
        adjusted_plane.normal.k * mirror_scale + camera->position.z;
    }
    result->mirrored = !camera->mirrored;
    {
      real_vector3d *up = &result->up;

      up->i = -up->i;
      up->j = -up->j;
      up->k = -up->k;
    }
  } else {
    real_vector3d cross_product;
    real inverse_forward_magnitude =
      1.0f / magnitude3d_inline(&camera->forward);
    real sine_of_incidence;
    real refracted_sine;
    real refraction_offset;

    cross_product3d_inline(&camera->forward, &plane.normal,
                           &cross_product);
    sine_of_incidence =
      magnitude3d_inline(&cross_product) * inverse_forward_magnitude;
    refracted_sine = mirror->index_of_refraction * sine_of_incidence;

    if (sine_of_incidence != 0.0f) {
      real cosine_of_incidence =
        dot_product3d_inline(&plane.normal,
                             &camera->forward) *
        inverse_forward_magnitude;
      real refraction_numerator = cosine_of_incidence * refracted_sine;

      refraction_offset =
        -(refraction_numerator * mirror->depth /
          ((real)x87_sqrtd(1.0f - refracted_sine * refracted_sine) *
           sine_of_incidence));
    } else {
      refraction_offset = 0.0f;
    }

    result->position.x =
      plane.normal.i * refraction_offset + camera->position.x;
    result->position.y =
      plane.normal.j * refraction_offset + camera->position.y;
    result->position.z =
      plane.normal.k * refraction_offset + camera->position.z;
  }

  result->z_near = 0.0f;
  result->mirror_plane = plane;
}

void render_camera_build_frustum(const camera_t *camera,
                                 const real_rectangle2d *bounds,
                                 render_frustum_t *frustum, bool do_projection)
{
  render_frustum_t *fr = frustum;
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
  real_vector3d view_left;
  real_vector3d view_up;
  real_vector3d view_backward;
  real_vector3d plane_normal;
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
  real_point3d view_point;
  int vertex_index;

  if (bounds) {
    fr->frustum_bounds = *bounds;
  } else {
    fr->frustum_bounds.y0 = -1.0f;
    fr->frustum_bounds.x0 = -1.0f;
    fr->frustum_bounds.y1 = 1.0f;
    fr->frustum_bounds.x1 = 1.0f;
  }

  half_bounds_width = (fr->frustum_bounds.x1 - fr->frustum_bounds.x0) * 0.5f;
  half_bounds_height = (fr->frustum_bounds.y1 - fr->frustum_bounds.y0) * 0.5f;
  bounds_center_x =
    (fr->frustum_bounds.x0 + fr->frustum_bounds.x1) / half_bounds_width * -0.5f;
  bounds_center_y =
    (fr->frustum_bounds.y0 + fr->frustum_bounds.y1) / half_bounds_height * -0.5f;
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
                     camera->vertical_field_of_view < k_maximum_vertical_field_of_view);
  if (camera->vertical_field_of_view <= 0.0001f) {
    display_assert(csprintf(temporary,
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

  cross_product3d_inline(&camera->forward, &camera->up, &view_left);
  cross_product3d_inline(&view_left, &camera->forward, &view_up);
  view_backward.i = -camera->forward.i;
  view_backward.j = -camera->forward.j;
  view_backward.k = -camera->forward.k;
  normalize3d(&view_left);
  normalize3d(&view_up);
  normalize3d(&view_backward);
  fr->view_to_world.forward = view_left;
  fr->view_to_world.left = view_up;
  fr->view_to_world.up = view_backward;
  fr->view_to_world.position = camera->position;
  fr->view_to_world.scale = 1.0f;
  matrix_inverse(&fr->view_to_world.scale, &fr->world_to_view.scale);
  assert_halt_msg_at("valid_real_matrix4x3(&frustum->world_to_view)",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1ca,
                     valid_real_matrix4x3(&fr->world_to_view.scale));
  assert_halt_msg_at("valid_real_matrix4x3(&frustum->view_to_world)",
                     "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1cb,
                     valid_real_matrix4x3(&fr->view_to_world.scale));

  plane_normal.i = -projection_x_scale;
  plane_normal.j = 0.0f;
  left_plane_z = bounds_center_x + 1.0f;
  plane_normal.k = left_plane_z;
  normalize3d(&plane_normal);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  matrix4x3_transform_plane(&fr->view_to_world, &view_plane, &fr->world_planes[0]);

  plane_normal.i = projection_x_scale;
  plane_normal.j = 0.0f;
  plane_normal.k = 1.0f - bounds_center_x;
  normalize3d(&plane_normal);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  matrix4x3_transform_plane(&fr->view_to_world, &view_plane, &fr->world_planes[1]);

  plane_normal.i = 0.0f;
  plane_normal.j = -projection_y_scale;
  bottom_plane_z = bounds_center_y + 1.0f;
  plane_normal.k = bottom_plane_z;
  normalize3d(&plane_normal);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  matrix4x3_transform_plane(&fr->view_to_world, &view_plane, &fr->world_planes[2]);

  plane_normal.i = 0.0f;
  plane_normal.j = projection_y_scale;
  plane_normal.k = 1.0f - bounds_center_y;
  normalize3d(&plane_normal);
  plane3d_from_point_and_normal_inline(&view_plane, global_origin3d,
                                       &plane_normal);
  matrix4x3_transform_plane(&fr->view_to_world, &view_plane, &fr->world_planes[3]);

  view_plane.normal.i = 0.0f;
  view_plane.normal.j = 0.0f;
  view_plane.normal.k = 1.0f;
  view_plane.d = -camera->z_near;
  matrix4x3_transform_plane(&fr->view_to_world, &view_plane, &fr->world_planes[4]);

  view_plane.normal.i = 0.0f;
  view_plane.normal.j = 0.0f;
  view_plane.normal.k = -1.0f;
  view_plane.d = camera->z_far;
  matrix4x3_transform_plane(&fr->view_to_world, &view_plane, &fr->world_planes[5]);

  fr->z_near = camera->z_near;
  fr->z_far = camera->z_far;
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
  matrix_transform_point(&fr->view_to_world, &view_point,
                         &fr->world_vertices[0]);
  view_point.x = far_right;
  view_point.y = far_bottom;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->view_to_world, &view_point,
                         &fr->world_vertices[1]);
  view_point.x = far_left;
  view_point.y = far_top;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->view_to_world, &view_point,
                         &fr->world_vertices[2]);
  view_point.x = far_right;
  view_point.y = far_top;
  view_point.z = -camera->z_far;
  matrix_transform_point(&fr->view_to_world, &view_point,
                         &fr->world_vertices[3]);

  fr->world_vertices[4] = camera->position;
  view_point.x = -(inverse_projection_x_scale * half_z * bounds_center_x);
  view_point.y = -(inverse_projection_y_scale * half_z * bounds_center_y);
  view_point.z = -half_z;
  matrix_transform_point(&fr->view_to_world, &view_point, &fr->world_midpoint);

  /* World AABB: x0,x1,y0,y1,z0,z1. */
  fr->world_bounds.x0 = fr->world_bounds.x1 = fr->world_vertices[0].x;
  fr->world_bounds.y0 = fr->world_bounds.y1 = fr->world_vertices[0].y;
  fr->world_bounds.z0 = fr->world_bounds.z1 = fr->world_vertices[0].z;
  for (vertex_index = 1; vertex_index < 5; vertex_index++) {
    const real_point3d *vertex = &fr->world_vertices[vertex_index];

    fr->world_bounds.x0 =
      fr->world_bounds.x0 > vertex->x ? vertex->x : fr->world_bounds.x0;
    fr->world_bounds.y0 =
      fr->world_bounds.y0 > vertex->y ? vertex->y : fr->world_bounds.y0;
    fr->world_bounds.z0 =
      fr->world_bounds.z0 > vertex->z ? vertex->z : fr->world_bounds.z0;
    fr->world_bounds.x1 =
      fr->world_bounds.x1 > vertex->x ? fr->world_bounds.x1 : vertex->x;
    fr->world_bounds.y1 =
      fr->world_bounds.y1 > vertex->y ? fr->world_bounds.y1 : vertex->y;
    fr->world_bounds.z1 =
      fr->world_bounds.z1 > vertex->z ? fr->world_bounds.z1 : vertex->z;
  }

  if (do_projection) {
    real inverse_plane_z;
    real clip_offset;
    real projection_scale;

    if (camera->z_near == 0.0f) {
      matrix4x3_transform_plane(&fr->world_to_view, &camera->mirror_plane, &view_plane);
    } else {
      view_plane.normal.i = 0.0f;
      view_plane.normal.j = 0.0f;
      view_plane.normal.k = 1.0f;
      view_plane.d = -camera->z_near;
    }

    inverse_plane_z = 1.0f / view_plane.normal.k;
    clip_offset = -(view_plane.d * inverse_plane_z);
    projection_scale =
      (real)(camera->z_far /
             ((camera->z_far - clip_offset) *
              (fabs(inverse_plane_z * view_plane.normal.i) +
               fabs(inverse_plane_z * view_plane.normal.j) + 1.0)));
    view_plane.normal.i *= inverse_plane_z * projection_scale;
    view_plane.normal.j *= inverse_plane_z * projection_scale;
    view_plane.normal.k = projection_scale;
    view_plane.d = -(projection_scale * clip_offset);
    if (view_plane.d > 0.0f && camera->z_near == 0.0f) {
      view_plane.normal.i = -view_plane.normal.i;
      view_plane.normal.j = -view_plane.normal.j;
      view_plane.normal.k = -view_plane.normal.k;
      view_plane.d = -view_plane.d;
    }

    /* 4x4 projection matrix, row-major [row * 4 + column]. */
    csmemset(fr->projection_matrix, 0, sizeof(fr->projection_matrix));
    fr->projection_matrix[0][0] = projection_x_scale;
    fr->projection_matrix[0][2] = -view_plane.normal.i;
    fr->projection_matrix[1][1] = projection_y_scale;
    fr->projection_matrix[1][2] = -view_plane.normal.j;
    fr->projection_matrix[2][0] = -bounds_center_x;
    fr->projection_matrix[2][1] = -bounds_center_y;
    fr->projection_matrix[2][2] = -view_plane.normal.k;
    fr->projection_matrix[2][3] = -1.0f;
    fr->projection_matrix[3][2] = view_plane.d;
    fr->projection_valid = 1;
    fr->projection_world_to_screen[0] = projection_x_scale * viewport_width * 0.5f;
    fr->projection_world_to_screen[1] = projection_y_scale * viewport_height * 0.5f;
  } else {
    csmemset(fr->projection_matrix, 0, sizeof(fr->projection_matrix));
    csmemset(fr->projection_world_to_screen, 0, sizeof(fr->projection_world_to_screen));
    fr->projection_valid = 0;
  }

  /* Planes: 0 left, 1 right, 2 bottom, 3 top, 4 near, 5 far.
   * Vertices: 0 bottom-left, 1 bottom-right, 2 top-left, 3 top-right,
   * 4 apex (camera position). */
  render_camera_check_warning_condition(
    0, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[0], &fr->world_vertices[0])));
  render_camera_check_warning_condition(
    1, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[0], &fr->world_vertices[2])));
  render_camera_check_warning_condition(
    2, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[0], &fr->world_vertices[4])));
  render_camera_check_warning_condition(
    3, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[1], &fr->world_vertices[1])));
  render_camera_check_warning_condition(
    4, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[1], &fr->world_vertices[3])));
  render_camera_check_warning_condition(
    5, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[1], &fr->world_vertices[4])));
  render_camera_check_warning_condition(
    6, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[2], &fr->world_vertices[0])));
  render_camera_check_warning_condition(
    7, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[2], &fr->world_vertices[1])));
  render_camera_check_warning_condition(
    8, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[2], &fr->world_vertices[4])));
  render_camera_check_warning_condition(
    9, (real)fabs(
         plane3d_distance_to_point_inline(&fr->world_planes[3], &fr->world_vertices[2])));
  render_camera_check_warning_condition(
    10, (real)fabs(plane3d_distance_to_point_inline(&fr->world_planes[3],
                                                    &fr->world_vertices[3])));
  render_camera_check_warning_condition(
    11, (real)fabs(plane3d_distance_to_point_inline(&fr->world_planes[3],
                                                    &fr->world_vertices[4])));
  render_camera_check_warning_condition(
    12, (real)fabs(plane3d_distance_to_point_inline(&fr->world_planes[5],
                                                    &fr->world_vertices[0])));
  render_camera_check_warning_condition(
    13, (real)fabs(plane3d_distance_to_point_inline(&fr->world_planes[5],
                                                    &fr->world_vertices[1])));
  render_camera_check_warning_condition(
    14, (real)fabs(plane3d_distance_to_point_inline(&fr->world_planes[5],
                                                    &fr->world_vertices[2])));
  render_camera_check_warning_condition(
    15, (real)fabs(plane3d_distance_to_point_inline(&fr->world_planes[5],
                                                    &fr->world_vertices[3])));
  render_camera_check_warning_condition(
    16, plane3d_distance_to_point_inline(&fr->world_planes[0], &fr->world_midpoint));
  render_camera_check_warning_condition(
    17, plane3d_distance_to_point_inline(&fr->world_planes[1], &fr->world_midpoint));
  render_camera_check_warning_condition(
    18, plane3d_distance_to_point_inline(&fr->world_planes[2], &fr->world_midpoint));
  render_camera_check_warning_condition(
    19, plane3d_distance_to_point_inline(&fr->world_planes[3], &fr->world_midpoint));
  render_camera_check_warning_condition(
    20, plane3d_distance_to_point_inline(&fr->world_planes[4], &fr->world_midpoint));
  render_camera_check_warning_condition(
    21, plane3d_distance_to_point_inline(&fr->world_planes[5], &fr->world_midpoint));
}

float contrail_fade(contrail_definition_t *definition, int16_t fade_mode,
                    real_point3d *world_point, real_vector3d *world_normal)
{
  real result = 1.0f;

  if (fade_mode) {
    real_vector3d to_camera;

    to_camera.i = unknown_global_camera.position.x - world_point->x;
    to_camera.j = unknown_global_camera.position.y - world_point->y;
    to_camera.k = unknown_global_camera.position.z - world_point->z;
    /* x87 sum order: dot z,y,x; squared magnitude (z+x)+y. */
    result = (real)fabs(
      (to_camera.k * world_normal->k + to_camera.j * world_normal->j +
       to_camera.i * world_normal->i) /
      (real)x87_sqrtd(to_camera.j * to_camera.j +
                      (to_camera.k * to_camera.k + to_camera.i * to_camera.i)));

    if (definition->flags & 0x40) {
      result = transition_function_evaluate(2, result);
    }
    if (fade_mode == 2) {
      result = 1.0f - result;
    }
  }

  return result;
}

typedef struct {
  real_point3d point; /* +0x00 */
  real_point2d texture; /* +0x0c */
  union {
    uint32_t pixel;
    struct {
      uint8_t blue;
      uint8_t green;
      uint8_t red;
      uint8_t alpha;
    } argb;
  } color; /* +0x14 */
} contrail_vertex_t;

void render_contrail(contrail_datum_t *contrail,
                     contrail_definition_t *definition, int16_t instance_index)
{
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
      real_point3d average_position;
      contrail_point_datum_t *previous_point;
      real texture_u;
      int point_index;

      triangles = rasterizer_dynamic_triangles_lock(triangle_buffer_index);
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
      point_index =
        contrail->first_contrail_point_indices[instance_index];

      while (point_index != -1) {
        contrail_point_datum_t *point;
        contrail_point_state_t *state;
        real color_scale;
        real width;
        real_argb_color color;
        real half_width;
        real_vector3d orientation;
        real_vector3d facing_normal;

        point = datum_get(contrail_point_data,
                                                    point_index);
        state = tag_block_get_element(
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

          next_state = tag_block_get_element(
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
          contrail_object_t *object =
            object_get_and_verify_type(contrail->object_index, -1);
          contrail_object_definition_t *object_definition =
            tag_get(TAG_GROUP_OBJE, object->definition_index);
          contrail_object_attachment_t *attachment = tag_block_get_element(
            &object_definition->attachments, contrail->attachment_index,
            sizeof(contrail_object_attachment_t));
          int16_t change_color_index = attachment->change_color - 1;

          if (change_color_index != NONE) {
            const real_rgb_color *change_color =
              &object->change_colors[change_color_index];

            color.red *= change_color->red;
            color.green *= change_color->green;
            color.blue *= change_color->blue;
          }
        }

        half_width *= 0.5f;
        vertices[0].texture.x = texture_u;
        vertices[0].texture.y = texture_v_far;
        vertices[1].texture.x = texture_u;
        vertices[1].texture.y = texture_offset_v;
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
              orientation.i = previous_point->position.y - point->position.y;
              orientation.j = point->position.x - previous_point->position.x;
            } else {
              next_point = datum_get(
                contrail_point_data, point->next_contrail_point_index);
              orientation.i = point->position.y - next_point->position.y;
              orientation.j = next_point->position.x - point->position.x;
            }
            orientation.k = 0.0f;
            normalize3d(&orientation);
          }
          break;

        case 1:
        case 2: {
          real_vector2d perpendicular;
          contrail_point_datum_t *next_point;

          if (previous_point) {
            perpendicular.i = previous_point->position.y - point->position.y;
            perpendicular.j = point->position.x - previous_point->position.x;
          } else {
            next_point = datum_get(
              contrail_point_data, point->next_contrail_point_index);
            perpendicular.i = point->position.y - next_point->position.y;
            perpendicular.j = next_point->position.x - point->position.x;
          }
          normalize2d(&perpendicular);
          vertices[0].point.x =
            point->position.x - perpendicular.i * half_width;
          vertices[0].point.y =
            point->position.y - perpendicular.j * half_width;
          vertices[0].point.z = point->position.z;
          vertices[1].point.x =
            point->position.x + perpendicular.i * half_width;
          vertices[1].point.y =
            point->position.y + perpendicular.j * half_width;
          vertices[1].point.z = point->position.z;
          if (apply_orientation) {
            orientation = *(real_vector3d *)global_up_vector_ptr;
          }
          break;
        }

        case 4: {
          real_vector3d tangent;
          real eye_i;
          real eye_j;
          real eye_k;
          contrail_point_datum_t *next_point;

          if (previous_point) {
            eye_i =
              unknown_global_camera.position.x - previous_point->position.x;
            eye_j =
              unknown_global_camera.position.y - previous_point->position.y;
            eye_k =
              unknown_global_camera.position.z - previous_point->position.z;
            tangent.i = point->position.x - previous_point->position.x;
            tangent.j = point->position.y - previous_point->position.y;
            tangent.k = point->position.z - previous_point->position.z;
          } else {
            next_point = datum_get(
              contrail_point_data, point->next_contrail_point_index);
            eye_i = unknown_global_camera.position.x - point->position.x;
            eye_j = unknown_global_camera.position.y - point->position.y;
            eye_k = unknown_global_camera.position.z - point->position.z;
            tangent.i = next_point->position.x - point->position.x;
            tangent.j = next_point->position.y - point->position.y;
            tangent.k = next_point->position.z - point->position.z;
          }
          facing_normal.i = tangent.k * eye_j - tangent.j * eye_k;
          facing_normal.j = tangent.i * eye_k - tangent.k * eye_i;
          facing_normal.k = tangent.j * eye_i - tangent.i * eye_j;
          normalize3d(&facing_normal);
          vertices[0].point.x =
            point->position.x - facing_normal.i * half_width;
          vertices[0].point.y =
            point->position.y - facing_normal.j * half_width;
          vertices[0].point.z =
            point->position.z - facing_normal.k * half_width;
          vertices[1].point.x =
            point->position.x + facing_normal.i * half_width;
          vertices[1].point.y =
            point->position.y + facing_normal.j * half_width;
          vertices[1].point.z =
            point->position.z + facing_normal.k * half_width;
          if (apply_orientation) {
            orientation.i =
              facing_normal.k * tangent.j - facing_normal.j * tangent.k;
            orientation.j =
              facing_normal.i * tangent.k - facing_normal.k * tangent.i;
            orientation.k =
              facing_normal.j * tangent.i - facing_normal.i * tangent.j;
            normalize3d(&orientation);
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
        vertices[0].color.pixel = vertices[1].color.pixel =
          real_argb_color_to_pixel32(&color);
        texture_u += texture_u_step;
        previous_point = point;
        vertices += 2;
        point_index = point->next_contrail_point_index;
      }

      vertices -= vertex_count;
      first_vertex = vertices;
      if (!(definition->flags & 0x1)) {
        first_vertex[0].color.argb.alpha = 0;
        first_vertex[1].color.argb.alpha = 0;
      }
      if (!(definition->flags & 0x2)) {
        vertices[vertex_count - 1].color.argb.alpha = 0;
        vertices[vertex_count - 2].color.argb.alpha = 0;
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
                   &average_position, 0);
      rasterizer_dynamic_triangles_delete(triangle_buffer_index);
      rasterizer_dynamic_vertices_delete(vertex_buffer_index);
    }
  }
  rasterizer_current_lock_operation = 0;
}

void render_contrails(uint32_t render_pass_mask)
{
  int contrail_index;
  contrail_datum_t *contrail;
  contrail_definition_t *definition;
  int16_t *point_counts;
  int16_t index;

  if (render_contrails_enabled == 0) {
    return;
  }

  for (contrail_index = data_next_index(contrail_data, -1);
       contrail_index != -1;
       contrail_index = data_next_index(contrail_data, contrail_index)) {
    contrail = datum_get(contrail_data, contrail_index);
    definition = tag_get(TAG_GROUP_CONT, contrail->definition_index);

    point_counts = contrail->contrail_point_counts;
    index = 0;
    do {
      /* 00188818 SHL EDX,CL with no preceding AND: the reference relies on
       * the x86 implicit shift-count mask, so no & 0x1f is written here. */
      if ((render_pass_mask &
           ((uint32_t)1 << definition->render_type)) != 0 &&
          *point_counts > 1) {
        render_contrail(contrail, definition, index);
      }
      index++;
      point_counts++;
    } while (index < 4);
  }
}

void render_contrails_normal(void)
{
  render_contrails(0xfffffff3);
}
