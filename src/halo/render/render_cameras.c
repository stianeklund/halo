/* Render camera utilities. */

#define MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS 64

static char render_camera_warnings_initialized; /* 0x4d0e18 */
static float
  render_camera_warning_values[MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS]; /* 0x4d0d18
                                                                           */

/* render_camera_check_warning_condition - 0x185770
 * Tracks maximum frustum-integrity violation distances per condition ID.
 * Logs when a condition exceeds its previous worst value. */
void render_camera_check_warning_condition(int16_t id, float value)
{
  assert_halt(id >= 0 && id < MAXIMUM_RENDER_CAMERA_WARNING_CONDITIONS);

  if (!render_camera_warnings_initialized) {
    csmemset(render_camera_warning_values, 0,
             sizeof(render_camera_warning_values));
    render_camera_warnings_initialized = 1;
  }

  if (value > 0.05f && value > render_camera_warning_values[id]) {
    error(2,
          "### ERROR cameras: frustum-integrity condition #%d violated by %f",
          (int)id, (double)value);
    render_camera_warning_values[id] = value;
  }
}

/* Typedefs for math helpers called via hardcoded address. */
typedef float (*normalize_vector3_fn)(float *v);
typedef void (*matrix4x3_inverse_fn)(float *src, float *dst);
typedef void (*matrix4x3_transform_point_fn)(float *mat, float *in, float *out);
typedef void (*matrix4x3_transform_plane_fn)(float *mat, float *in, float *out);
typedef int (*valid_real_matrix4x3_fn)(float *mat);

#define normalize_vector3 ((normalize_vector3_fn)0x13010)
#define matrix4x3_inverse ((matrix4x3_inverse_fn)0x109150)
#define matrix4x3_transform_point ((matrix4x3_transform_point_fn)0x109590)
#define matrix4x3_transform_plane ((matrix4x3_transform_plane_fn)0x10a1c0)
#define valid_real_matrix4x3 ((valid_real_matrix4x3_fn)0xf6d00)

/* Compute the adjusted FOV tangent for the render camera.
 * Uses FPTAN: tan(fov * half_constant) * aspect_ratio */
double render_camera_get_adjusted_field_of_view_tangent(float fov)
{
#ifdef _MSC_VER
  double result;
  __asm {
    fld fov
    fmul dword ptr ds:[253398h]
    fptan
    fstp st(0)
    fmul dword ptr ds:[2b1504h]
    fstp result
  }
  return result;
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

/* Build the full view frustum from a camera, optional viewport bounds, and
 * projection flag.  Populates the frustum structure with:
 *   - viewport bounds (float[4])
 *   - world-to-view / view-to-world matrices (matrix4x3 each)
 *   - 6 clip planes (left, right, bottom, top, near, far)
 *   - z_near / z_far copies
 *   - 4 far-plane frustum corners + camera position + projection center
 *   - AABB (min/max xyz) of the frustum corners
 *   - optional projection matrix and scale factors
 * Assertions guard field-of-view, z ordering, and viewport sanity.
 * Ends with 22 frustum-integrity warning checks (distances of corners
 * and projection center to each clip plane). */
void render_camera_build_frustum(camera_t *camera, float *bounds,
                                 float *frustum, bool do_projection)
{
  float *forward = (float *)&camera->unk_12; /* +0x0c */
  float *up = (float *)&camera->unk_24; /* +0x18 */
  float *pos = (float *)&camera->unk_0; /* +0x00 */
  float *proj_data = (float *)camera->unk_68; /* +0x44 */

  /* Compute viewport pixel dimensions. */
  int width_px =
    (int)camera->viewport_bounds.x1 - (int)camera->viewport_bounds.x0;
  int height_px =
    (int)camera->viewport_bounds.y1 - (int)camera->viewport_bounds.y0;
  float width_f = (float)width_px;
  float height_f = (float)height_px;

  /* Copy or default the viewport bounds (frustum[0..3]). */
  if (bounds == 0) {
    frustum[2] = -1.0f;
    frustum[0] = -1.0f;
    frustum[3] = 1.0f;
    frustum[1] = 1.0f;
  } else {
    frustum[0] = bounds[0];
    frustum[1] = bounds[1];
    frustum[2] = bounds[2];
    frustum[3] = bounds[3];
  }

  /* Compute half-ranges and viewport centers. */
  float half_w_range = (frustum[1] - frustum[0]) * 0.5f;
  float half_h_range = (frustum[3] - frustum[2]) * 0.5f;
  float center_x = (frustum[0] + frustum[1]) / half_w_range * -0.5f;
  float center_y = (frustum[2] + frustum[3]) / half_h_range * -0.5f;

  /* Compute tan(vfov/2) and inverse tangent scale factors.
   * inv_tan_x accounts for the aspect ratio correction. */
  float tan_half_fov;
#ifdef _MSC_VER
  {
    float vfov = camera->vertical_field_of_view;
    __asm {
      fld vfov
      fmul dword ptr ds:[253398h]
      fptan
      fstp st(0)
      fstp tan_half_fov
    }
  }
#else
  asm volatile("flds %[f]\n\t"
               "fmuls 0x253398\n\t"
               "fptan\n\t"
               "fstp %%st(0)"
               : "=t"(tan_half_fov)
               : [f] "m"(camera->vertical_field_of_view)
               : "memory");
#endif

  float inv_tan_x = 1.0f / (half_w_range / height_f * width_f * tan_half_fov);
  float inv_tan_y = 1.0f / (tan_half_fov * half_h_range);

  /* Assertions. */
  assert_halt(camera->vertical_field_of_view <
              *(float *)0x2b16f4); /* _pi - _real_epsilon */
  if (camera->vertical_field_of_view <= *(float *)0x253f44) {
    char *msg = csprintf((char *)0x5ab100,
                         "### FATAL ERROR: field of view set to %f (0x%x)",
                         (double)camera->vertical_field_of_view,
                         *(int *)&camera->vertical_field_of_view);
    display_assert(msg, "c:\\halo\\SOURCE\\render\\render_cameras.c", 0x1b0,
                   true);
    system_exit(-1);
  }
  assert_halt(camera->z_near >= 0.0f);
  assert_halt(camera->z_far > camera->z_near);
  assert_halt(camera->viewport_bounds.x0 < camera->viewport_bounds.x1);
  assert_halt(camera->viewport_bounds.y0 < camera->viewport_bounds.y1);

  /* Build the view-to-world matrix (frustum[0x11..0x1d]).
   * Columns are: right (cross product), up (double cross), -forward,
   * then the camera position as the translation row. */
  float right[3], up2[3], neg_fwd[3];

  /* right = up x forward */
  right[0] = up[2] * forward[1] - up[1] * forward[2];
  right[1] = up[0] * forward[2] - up[2] * forward[0];
  right[2] = up[1] * forward[0] - forward[1] * up[0];

  /* up2 = right x forward  (re-orthogonalized up) */
  up2[0] = right[1] * forward[2] - right[2] * forward[1];
  up2[1] = right[2] * forward[0] - right[0] * forward[2];
  up2[2] = right[0] * forward[1] - right[1] * forward[0];

  /* neg_fwd = -forward */
  neg_fwd[0] = -forward[0];
  neg_fwd[1] = -forward[1];
  neg_fwd[2] = -forward[2];

  normalize_vector3(right);
  normalize_vector3(up2);
  normalize_vector3(neg_fwd);

  /* Store view_to_world matrix at frustum[0x12..0x1d].
   * frustum[0x11] = scale (1.0). */
  frustum[0x12] = right[0];
  frustum[0x13] = right[1];
  frustum[0x14] = right[2];
  frustum[0x15] = up2[0];
  frustum[0x16] = up2[1];
  frustum[0x17] = up2[2];
  frustum[0x18] = neg_fwd[0];
  frustum[0x19] = neg_fwd[1];
  frustum[0x1a] = neg_fwd[2];
  frustum[0x1b] = pos[0];
  frustum[0x1c] = pos[1];
  frustum[0x1d] = pos[2];
  frustum[0x11] = 1.0f;

  /* Compute world_to_view = inverse(view_to_world).
   * frustum[4..0x10] = world_to_view matrix. */
  float *view_to_world = &frustum[0x11];
  float *world_to_view = &frustum[4];
  matrix4x3_inverse(view_to_world, world_to_view);

  assert_halt(valid_real_matrix4x3(world_to_view));
  assert_halt(valid_real_matrix4x3(view_to_world));

  /* Build clip planes.  Each plane is stored as (nx, ny, nz, d).
   * Plane normals are constructed in view space, normalized, then
   * the distance d is computed as dot(normal, global_forward) where
   * global_forward is the vector at **(float**)0x31fc1c. */
  float *global_fwd = *(float **)0x31fc1c;
  float plane_vs[4]; /* view-space plane: (x, y, z, d) */

  /* Left plane (frustum[0x1e..0x21]) */
  plane_vs[0] = -inv_tan_x;
  plane_vs[1] = 0.0f;
  plane_vs[2] = center_x + 1.0f;
  float saved_cx_plus_1 = plane_vs[2];
  normalize_vector3(plane_vs);
  plane_vs[3] = plane_vs[0] * global_fwd[0] + plane_vs[1] * global_fwd[1] +
                plane_vs[2] * global_fwd[2];
  matrix4x3_transform_plane(view_to_world, plane_vs, &frustum[0x1e]);

  /* Right plane (frustum[0x22..0x25]) */
  plane_vs[0] = inv_tan_x;
  plane_vs[1] = 0.0f;
  plane_vs[2] = 1.0f - center_x;
  normalize_vector3(plane_vs);
  plane_vs[3] = plane_vs[0] * global_fwd[0] + plane_vs[1] * global_fwd[1] +
                plane_vs[2] * global_fwd[2];
  matrix4x3_transform_plane(view_to_world, plane_vs, &frustum[0x22]);

  /* Bottom plane (frustum[0x26..0x29]) */
  plane_vs[0] = 0.0f;
  plane_vs[1] = -inv_tan_y;
  plane_vs[2] = center_y + 1.0f;
  float saved_cy_plus_1 = plane_vs[2];
  normalize_vector3(plane_vs);
  plane_vs[3] = plane_vs[0] * global_fwd[0] + plane_vs[1] * global_fwd[1] +
                plane_vs[2] * global_fwd[2];
  matrix4x3_transform_plane(view_to_world, plane_vs, &frustum[0x26]);

  /* Top plane (frustum[0x2a..0x2d]) */
  plane_vs[0] = 0.0f;
  plane_vs[1] = inv_tan_y;
  plane_vs[2] = 1.0f - center_y;
  normalize_vector3(plane_vs);
  plane_vs[3] = plane_vs[0] * global_fwd[0] + plane_vs[1] * global_fwd[1] +
                plane_vs[2] * global_fwd[2];
  matrix4x3_transform_plane(view_to_world, plane_vs, &frustum[0x2a]);

  /* Near plane (frustum[0x2e..0x31]) */
  plane_vs[0] = 0.0f;
  plane_vs[1] = 0.0f;
  plane_vs[2] = 1.0f;
  plane_vs[3] = -camera->z_near;
  matrix4x3_transform_plane(view_to_world, plane_vs, &frustum[0x2e]);

  /* Far plane (frustum[0x32..0x35]) */
  plane_vs[0] = 0.0f;
  plane_vs[1] = 0.0f;
  plane_vs[2] = -1.0f;
  plane_vs[3] = camera->z_far;
  matrix4x3_transform_plane(view_to_world, plane_vs, &frustum[0x32]);

  /* Store z_near and z_far copies. */
  frustum[0x36] = camera->z_near;
  frustum[0x37] = camera->z_far;

  /* Compute scale factors for projection. */
  float scale_x = 1.0f / inv_tan_x;
  float scale_y = 1.0f / inv_tan_y;

  float half_z = (camera->z_far + camera->z_near) * 0.5f;

  /* Compute 4 far-plane frustum corners in world space.
   * Each corner is a view-space direction scaled by -z_far, then
   * transformed by view_to_world into world space. */
  float corner_lx = saved_cx_plus_1 * -(scale_x * camera->z_far);
  float corner_rx = (center_x - 1.0f) * -(scale_x * camera->z_far);
  float corner_by = saved_cy_plus_1 * -(scale_y * camera->z_far);
  float corner_ty = (center_y - 1.0f) * -(scale_y * camera->z_far);

  float corner_vs[3];

  /* Corner 0: left-bottom-far => frustum[0x38..0x3a] */
  corner_vs[0] = corner_lx;
  corner_vs[1] = corner_by;
  corner_vs[2] = -camera->z_far;
  matrix4x3_transform_point(view_to_world, corner_vs, &frustum[0x38]);

  /* Corner 1: right-bottom-far => frustum[0x3b..0x3d] */
  corner_vs[0] = corner_rx;
  corner_vs[1] = corner_by;
  corner_vs[2] = -camera->z_far;
  matrix4x3_transform_point(view_to_world, corner_vs, &frustum[0x3b]);

  /* Corner 2: left-top-far => frustum[0x3e..0x40] */
  corner_vs[0] = corner_lx;
  corner_vs[1] = corner_ty;
  corner_vs[2] = -camera->z_far;
  matrix4x3_transform_point(view_to_world, corner_vs, &frustum[0x3e]);

  /* Corner 3: right-top-far => frustum[0x41..0x43] */
  corner_vs[0] = corner_rx;
  corner_vs[1] = corner_ty;
  corner_vs[2] = -camera->z_far;
  matrix4x3_transform_point(view_to_world, corner_vs, &frustum[0x41]);

  /* Camera position => frustum[0x44..0x46] */
  frustum[0x44] = pos[0];
  frustum[0x45] = pos[1];
  frustum[0x46] = pos[2];

  /* Projection center => frustum[0x47..0x49] */
  float proj_center_vs[3];
  proj_center_vs[0] = -(scale_x * half_z * center_x);
  proj_center_vs[1] = -(scale_y * half_z * center_y);
  proj_center_vs[2] = -half_z;
  matrix4x3_transform_point(view_to_world, proj_center_vs, &frustum[0x47]);

  /* Compute AABB of frustum corners (frustum[0x4a..0x4f]).
   * Initialize from corner 0, then expand with corners 1-3. */
  float *corner0 = &frustum[0x38];
  frustum[0x4b] = corner0[0]; /* max_x = corner0.x */
  frustum[0x4a] = corner0[0]; /* min_x = corner0.x */
  frustum[0x4d] = corner0[1]; /* max_y */
  frustum[0x4c] = corner0[1]; /* min_y */
  frustum[0x4f] = corner0[2]; /* max_z */
  frustum[0x4e] = corner0[2]; /* min_z */

  {
    float *cp = &frustum[0x3c]; /* start at corner1[0] (= 0x3b+1?) */
    int i;
    /* The loop walks 4 iterations starting at frustum[0x3c-1] = 0x3b.
     * ECX starts at ESI+0xF0 = frustum + 0x3c. The comparisons use
     * [ECX-4], [ECX], [ECX+4] => frustum[0x3b], [0x3c], [0x3d] etc.
     * Actually the disasm starts ECX at ESI+0xF0 and accesses
     * [ECX-4], [ECX], [ECX+4]. Let me trace: */
    /* Initial: ECX = &frustum[0x3c] (= ESI + 0xF0).
     * Iteration 0: [ECX-4]=frustum[0x3b], [ECX]=frustum[0x3c],
     *              [ECX+4]=frustum[0x3d]
     * Iteration 1: ECX += 3 => &frustum[0x3f]:
     *              [ECX-4]=frustum[0x3e], etc.
     * ...4 iterations covering corners 1,2,3 and one more. */
    /* Actually: from the disasm, ECX = ESI+0xF0, EDX=4, loop body
     * uses [ECX-4], [ECX], [ECX+4], increments ECX by 0xC (3 floats),
     * decrements EDX, loops while EDX != 0.
     * So 4 iterations at offsets: 0xF0, 0xFC, 0x108, 0x114
     * = frustum[0x3c], [0x3f], [0x42], [0x45]
     * Corner data starts at [0x3b] with stride 3:
     * iter0: [0x3b,0x3c,0x3d] = corner 1 (indices 0x3b-0x3d)
     * iter1: [0x3e,0x3f,0x40] = corner 2
     * iter2: [0x41,0x42,0x43] = corner 3
     * iter3: [0x44,0x45,0x46] = camera position */
    cp = &frustum[0x3c];
    for (i = 4; i != 0; i--) {
      if (frustum[0x4a] > cp[-1])
        frustum[0x4a] = cp[-1];
      if (frustum[0x4c] > cp[0])
        frustum[0x4c] = cp[0];
      if (frustum[0x4e] > cp[1])
        frustum[0x4e] = cp[1];
      if (frustum[0x4b] < cp[-1])
        frustum[0x4b] = cp[-1];
      if (frustum[0x4d] < cp[0])
        frustum[0x4d] = cp[0];
      if (frustum[0x4f] < cp[1])
        frustum[0x4f] = cp[1];
      cp += 3;
    }
  }

  /* Projection matrix block (frustum[0x50..0x62]). */
  if (!do_projection) {
    /* No projection: zero the matrix and scale, clear flag. */
    csmemset(&frustum[0x51], 0, 0x40);
    csmemset(&frustum[0x61], 0, 0x8);
    *(unsigned char *)&frustum[0x50] = 0;
  } else {
    /* Build the projection matrix from the camera's projection
     * data at camera->unk_68 (+0x44, 4 floats). */
    if (camera->z_near == 0.0f) {
      /* z_near == 0: transform the projection data through
       * world_to_view to get the view-space direction. */
      matrix4x3_transform_plane(world_to_view, proj_data, plane_vs);
      /* plane_vs now contains the view-space values. */
    } else {
      /* z_near != 0: use default forward direction. */
      plane_vs[0] = 0.0f;
      plane_vs[2] = 1.0f;
      plane_vs[3] = -camera->z_near;
      plane_vs[1] = 0.0f;
    }

    /* Compute adjusted projection parameters. */
    float inv_z = 1.0f / plane_vs[2];
    float neg_proj_d = -(plane_vs[3] * inv_z);
    float abs_x = inv_z * plane_vs[0];
    float abs_y = inv_z * plane_vs[1];
    if (abs_x < 0.0f)
      abs_x = -abs_x;
    if (abs_y < 0.0f)
      abs_y = -abs_y;
    float denom =
      (camera->z_far - neg_proj_d) * (abs_x + abs_y + *(double *)0x2573d8);
    float proj_scale = camera->z_far / denom;

    plane_vs[0] = inv_z * proj_scale * plane_vs[0];
    plane_vs[1] = inv_z * proj_scale * plane_vs[1];
    plane_vs[3] = -(proj_scale * neg_proj_d);

    /* If the adjusted distance is positive and z_near is zero,
     * flip all signs (face the other way). */
    if (plane_vs[3] > 0.0f && camera->z_near == 0.0f) {
      plane_vs[0] = -plane_vs[0];
      plane_vs[1] = -plane_vs[1];
      proj_scale = -proj_scale;
      plane_vs[3] = -plane_vs[3];
    }

    /* Write the 4x4 projection matrix. */
    csmemset(&frustum[0x51], 0, 0x40);
    frustum[0x53] = -plane_vs[0];
    frustum[0x57] = -plane_vs[1];
    frustum[0x51] = inv_tan_x;
    frustum[0x56] = inv_tan_y;
    frustum[0x5c] = -1.0f;
    frustum[0x59] = -center_x;
    frustum[0x5f] = plane_vs[3];
    *(unsigned char *)&frustum[0x50] = 1;
    frustum[0x5a] = -center_y;
    frustum[0x5b] = -proj_scale;
    frustum[0x61] = inv_tan_x * width_f * 0.5f;
    frustum[0x62] = inv_tan_y * height_f * 0.5f;
  }

  /* Frustum integrity checks: measure signed distances from key
   * points (corners, camera position, projection center) to each
   * clip plane.  The absolute distance is passed to the warning
   * function along with a condition ID (0..0x15). */
  float *left_p = &frustum[0x1e];
  float *right_p = &frustum[0x22];
  float *bottom_p = &frustum[0x26];
  float *top_p = &frustum[0x2a];
  float *near_p = &frustum[0x2e];
  float *far_p = &frustum[0x32];
  float *c0 = &frustum[0x38];
  float *c1 = &frustum[0x3b];
  float *c2 = &frustum[0x3e];
  float *c3 = &frustum[0x41];
  float *cam_pos = &frustum[0x44];
  float *proj_ctr = &frustum[0x47];
  float d;

  /* Corners vs left plane */
  d = c0[0] * left_p[0] + c0[1] * left_p[1] + c0[2] * left_p[2] - left_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(0, d);

  d = c2[0] * left_p[0] + c2[1] * left_p[1] + c2[2] * left_p[2] - left_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(1, d);

  d = cam_pos[0] * left_p[0] + cam_pos[1] * left_p[1] + cam_pos[2] * left_p[2] -
      left_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(2, d);

  /* Corners vs right plane */
  d = c1[0] * right_p[0] + c1[1] * right_p[1] + c1[2] * right_p[2] - right_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(3, d);

  d = c3[0] * right_p[0] + c3[1] * right_p[1] + c3[2] * right_p[2] - right_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(4, d);

  d = cam_pos[0] * right_p[0] + cam_pos[1] * right_p[1] +
      cam_pos[2] * right_p[2] - right_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(5, d);

  /* Corners vs bottom plane */
  d = c0[0] * bottom_p[0] + c0[1] * bottom_p[1] + c0[2] * bottom_p[2] -
      bottom_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(6, d);

  d = c1[0] * bottom_p[0] + c1[1] * bottom_p[1] + c1[2] * bottom_p[2] -
      bottom_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(7, d);

  d = cam_pos[0] * bottom_p[0] + cam_pos[1] * bottom_p[1] +
      cam_pos[2] * bottom_p[2] - bottom_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(8, d);

  /* Corners vs top plane */
  d = c2[0] * top_p[0] + c2[1] * top_p[1] + c2[2] * top_p[2] - top_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(9, d);

  d = c3[0] * top_p[0] + c3[1] * top_p[1] + c3[2] * top_p[2] - top_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(10, d);

  d = cam_pos[0] * top_p[0] + cam_pos[1] * top_p[1] + cam_pos[2] * top_p[2] -
      top_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(11, d);

  /* Corners vs far plane */
  d = c0[0] * far_p[0] + c0[1] * far_p[1] + c0[2] * far_p[2] - far_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(12, d);

  d = c1[0] * far_p[0] + c1[1] * far_p[1] + c1[2] * far_p[2] - far_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(13, d);

  d = c2[0] * far_p[0] + c2[1] * far_p[1] + c2[2] * far_p[2] - far_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(14, d);

  d = c3[0] * far_p[0] + c3[1] * far_p[1] + c3[2] * far_p[2] - far_p[3];
  if (d < 0.0f)
    d = -d;
  render_camera_check_warning_condition(15, d);

  /* Projection center vs all 6 planes (no fabs — signed distance). */
  d = proj_ctr[0] * left_p[0] + proj_ctr[1] * left_p[1] +
      proj_ctr[2] * left_p[2] - left_p[3];
  render_camera_check_warning_condition(16, d);

  d = proj_ctr[0] * right_p[0] + proj_ctr[1] * right_p[1] +
      proj_ctr[2] * right_p[2] - right_p[3];
  render_camera_check_warning_condition(17, d);

  d = proj_ctr[0] * bottom_p[0] + proj_ctr[1] * bottom_p[1] +
      proj_ctr[2] * bottom_p[2] - bottom_p[3];
  render_camera_check_warning_condition(18, d);

  d = proj_ctr[0] * top_p[0] + proj_ctr[1] * top_p[1] + proj_ctr[2] * top_p[2] -
      top_p[3];
  render_camera_check_warning_condition(19, d);

  d = proj_ctr[0] * near_p[0] + proj_ctr[1] * near_p[1] +
      proj_ctr[2] * near_p[2] - near_p[3];
  render_camera_check_warning_condition(20, d);

  d = proj_ctr[0] * far_p[0] + proj_ctr[1] * far_p[1] + proj_ctr[2] * far_p[2] -
      far_p[3];
  render_camera_check_warning_condition(21, d);
}
