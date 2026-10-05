#include "x87_math.h"

/* Bungie cseries.h-style flag macros, file-local like
 * game_allegiance.c: SET_FLAG's ?: form is what folds to the SHR/AND and
 * OR/AND-mask sequences in point_physics_update. */
#define FLAG_BIT(b) (1u << (b))
#define TEST_FLAG(f, b) (((f) & FLAG_BIT(b)) != 0)
#define SET_FLAG(f, b, v) ((v) ? ((f) |= FLAG_BIT(b)) : ((f) &= ~FLAG_BIT(b)))

/* point_physics_update flag bits (Bungie point_physics.h names). */
enum {
  _point_physics_ignore_position_bit = 0, /* TEST CL,0x1 @154bc0 */
  _point_physics_ignore_position_under_water_bit = 1, /* SHR EBX,1 @154bd3 */
  _point_physics_force_no_collisions_bit = 2 /* TEST [EBP+8],0x4 */
};

/* point_physics_definition.flags bits. */
enum {
  _point_physics_structure_collisions_bit = 1, /* 0x2 -> collision 0x20 */
  _point_physics_water_collisions_bit = 2, /* 0x4 -> collision 0x40 */
  _point_physics_simple_wind_bit = 3, /* SHR EAX,3 @154ba4 */
  _point_physics_damped_wind_bit = 4, /* TEST CL,0x10 @154bad */
  _point_physics_no_gravity_bit = 5 /* MOV EDX,0x20 @154c3a */
};

/* mass_point_datum.flags bits: OR 0x1/0x2/0x8/0x10
 * in physics_compute_new at 0x151684/0x1516a0/0x1516bc/0x15183d; bit 2
 * (0x4) is set by compute_ground_plane. */
enum {
  _point_at_rest_bit = 0,
  _point_on_ground_bit = 1,
  _point_on_volatile_surface_bit = 2,
  _point_in_water_bit = 3,
  _point_antigraving_bit = 4
};

/* powered_mass_point_definition.flags bits: TEST byte
 * [+0x20] with 0x1..0x40 in physics_compute_new. */
enum {
  _powered_mass_point_ground_friction_bit = 0,
  _powered_mass_point_water_friction_bit = 1,
  _powered_mass_point_air_friction_bit = 2,
  _powered_mass_point_water_lift_bit = 3,
  _powered_mass_point_air_lift_bit = 4,
  _powered_mass_point_thrust_bit = 5,
  _powered_mass_point_antigrav_bit = 6
};

/* point_physics_update result bits. */
enum {
  _point_physics_in_air_bit = 0,
  _point_physics_in_water_bit = 1,
  _point_physics_collided_with_structure_bit = 2,
  _point_physics_collided_with_water_bit = 3
};

/* object_datum_t.flags bit cleared by physics_compute_vehicle_collision
 * (AND ~0x20 at 0x1522b9); Bungie objects.h name. */
enum { _object_at_rest_bit = 5 };

/* damage_data.flags bit set by physics_compute_biped_collision (OR 1 at
 * 0x151d59); Bungie damage.h name. */
enum { _damage_area_of_effect_bit = 0 };

/* File-local copies of Bungie's real_math.h inlines.
 * cross_product3d evaluates k, j, i into temporaries before storing i, j, k,
 * and set_real_point3d takes its components by value (evaluated z, y, x):
 * both orders are visible in physics_instance_new at 0x150a36-0x150a89. */
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

static __inline real_plane3d *plane3d_from_point_and_normal_inline(
  real_plane3d *plane, const real_point3d *point, const real_vector3d *normal)
{
  *(real_vector3d *)plane->normal = *normal;
  plane->d = dot_product3d_inline((const real_vector3d *)point,
                                  (const real_vector3d *)plane->normal);
  return plane;
}

/* render_debug_point_physics (point_physics.c, static): inlined
 * into point_physics_update at 0x154f8b; FUN_00154a20 is its out-of-line
 * copy.  0x2ee6d0/0x2ee6d4 are the red/green debug colour pointers. */
static __inline void render_debug_point_physics_inline(
  const struct point_physics_definition *definition, float *position,
  float radius)
{
  void *color;

  color = *(void **)0x2ee6d0;
  if (!TEST_FLAG(definition->flags, _point_physics_structure_collisions_bit))
    color = *(void **)0x2ee6d4;

  FUN_00189150(1, position, radius, color);
}

static __inline real_vector3d *
scale_vector3d_inline(const real_vector3d *v, real scale, real_vector3d *result)
{
  result->i = scale * v->i;
  result->j = scale * v->j;
  result->k = scale * v->k;
  return result;
}

static __inline real_vector3d *add_vectors3d_inline(const real_vector3d *a,
                                                    const real_vector3d *b,
                                                    real_vector3d *result)
{
  result->i = a->i + b->i;
  result->j = a->j + b->j;
  result->k = a->k + b->k;
  return result;
}

static __inline real_vector3d *
vector_from_points3d_inline(const real_point3d *a, const real_point3d *b,
                            real_vector3d *result)
{
  result->i = b->x - a->x;
  result->j = b->y - a->y;
  result->k = b->z - a->z;
  return result;
}

/* normalize3d as inlined at 0x151ffd-0x15204f: magnitude below the double
 * epsilon at 0x2533d0 yields 0.0f and leaves the vector untouched. */
static __inline real normalize3d_inline(real_vector3d *v)
{
  real magnitude = x87_sqrt(dot_product3d_inline(v, v));

  if (!(x87_fabs(magnitude) < *(const double *)0x2533d0)) {
    real inverse = 1.0f / magnitude;
    v->i *= inverse;
    v->j *= inverse;
    v->k *= inverse;
  } else {
    magnitude = 0.0f;
  }
  return magnitude;
}

static __inline vector3_t *set_real_point3d_inline(vector3_t *p, real x, real y,
                                                   real z)
{
  p->x = x;
  p->y = y;
  p->z = z;
  return p;
}

/* 0x150710 — pin_fraction (physics.c): fraction of the
 * way `value` lies from `begin` to `end`, pinned to [0,1]; a reversed range
 * (begin >= end) measures from `begin` downward. */
float pin_fraction(float value, float begin, float end)
{
  if (begin < end) {
    if (value <= begin)
      return 0.0f;
    else if (value >= end)
      return 1.0f;
    else
      return (value - begin) / (end - begin);
  } else {
    if (value <= end)
      return 1.0f;
    else if (value >= begin)
      return 0.0f;
    else
      return (begin - value) / (begin - end);
  }
}

/* 0x150790 — model-collision sphere pass: walk the collision-sphere tag block
 * of a FUN_001509c0 model context, transform each sphere centre into world
 * space through the context matrix, and feed the transformed point plus the
 * scaled sphere radius to collision_features_from_point.
 *
 * Frame is SUB ESP,0xc: EBP-0x0c is the 3-float transformed point. ESI is
 * only pushed/popped around the loop body (0x1507ac / 0x150812).
 *
 * param_1 is the model context:
 *   +0x00  int    handle forwarded as collision_features_from_point param_4
 *                 (MOV EAX,[EDI] at 0x1507d8)
 *   +0x04  int    base whose +0x74 is the collision-sphere tag_block,
 *                 element size 0x80 (PUSH 0x80 at 0x1507b0)
 *   +0x08  float  matrix; matrix[0] is also the uniform radius scale used by
 *                 FMUL float ptr [EDI+8] at 0x1507d5
 * param_2 (EBP+0x0c) and param_3 (EBP+0x10) are never referenced anywhere in
 * 0x150790-0x15083b; they are kept so the cdecl slots of the sole caller
 * FUN_0014ea10 (call at 0x14eb98) line up.
 * param_4 (EBP+0x14) and param_5 (EBP+0x18) are float dwords carried as int:
 * param_4 is forwarded bit-exact by a plain dword copy (PUSH ECX at
 * 0x1507ea/0x1507f7) and param_5 is the FADD operand at 0x1507ee.
 * collision_features_from_point's param_3 is declared `int` in kb.json but the
 * original stores it with FSTP [ESP] at 0x1507f4 — it is a float dword, so it
 * is forwarded here bit-exact, never through a numeric cast (same note as
 * collision_bsp.c 0x14ea10).
 * Returns 1 when any of the three int16 counters at param_6 is non-zero, else
 * 0 (MOV EAX,0x1 at 0x150832 / XOR EAX,EAX at 0x15082c). The sole caller
 * discards the result.
 */
int FUN_00150790(int param_1, int param_2, float param_3, int param_4,
                 int param_5, int param_6)
{
  void *element;
  int base;
  int element_index;
  short index;
  float *pRadius;
  float radius;
  float point[3];

  base = *(int *)(param_1 + 4);
  index = 0;

  if (*(int *)(base + 0x74) > 0) {
    element_index = 0;
    do {
      element =
        tag_block_get_element((void *)(base + 0x74), element_index, 0x80);
      matrix_transform_point((float *)(param_1 + 8),
                             (float *)((char *)element + 0x38), point);
      radius = *(float *)((char *)element + 0x68) * *(float *)(param_1 + 8) +
               *(float *)&param_5;
      pRadius = &radius;
      collision_features_from_point((int)point, *(float *)&param_4,
                                    *(int *)pRadius, *(int *)param_1, -1, 0,
                                    0xff, -1, (void *)param_6);
      base = *(int *)(param_1 + 4);
      index = (short)(index + 1);
      element_index = index;
    } while (element_index < *(int *)(base + 0x74));
  }

  if (*(short *)param_6 != 0 || *(short *)(param_6 + 2) != 0 ||
      *(short *)(param_6 + 4) != 0) {
    return 1;
  }

  return 0;
}

/* 0x150840 — look up a collision-function attribute by index (compare
 * FUN_0014da80, same 0x234/0x48/+0x24 tag-block shape once the 'coll' tag is
 * resolved). If collision_fn_index == -1, returns -1 (OR AX,0xffff; only the
 * low 16 bits of the return register are set — no MOVSX/MOVZX — matching the
 * `short` return here).
 * If object_index != -1: resolves the object's 'obje' tag, follows +0x7c to
 * its 'coll' tag, indexes the 0x48-byte block at coll_tag+0x234 by
 * collision_fn_index, and returns the int16_t at element+0x24.
 * Else: indexes the 0x14-byte block at scenario_get()+0xa4 by
 * collision_fn_index and returns the int16_t at element+0x12 (scenario-level
 * collision function table, used when there is no object).
 * Confirmed via disassembly 0x150840-0x1508a9. Sole caller:
 * compute_ground_plane at 0x150d48 (unconditional call, not yet ported).
 */
short FUN_00150840(int object_index, short collision_fn_index)
{
  int *obj;
  void *obje_tag;
  void *coll_tag;
  void *elem;

  if (collision_fn_index == -1) {
    return -1;
  }

  if (object_index != -1) {
    obj = (int *)object_get_and_verify_type(object_index, -1);
    obje_tag = tag_get(0x6f626a65 /* 'obje' */, *obj);
    coll_tag =
      tag_get(0x636f6c6c /* 'coll' */, *(int *)((char *)obje_tag + 0x7c));
    elem =
      tag_block_get_element((char *)coll_tag + 0x234, collision_fn_index, 0x48);
    return *(short *)((char *)elem + 0x24);
  }

  elem = tag_block_get_element((char *)scenario_get() + 0xa4,
                               collision_fn_index, 0x14);
  return *(short *)((char *)elem + 0x12);
}

/* 0x1508b0 — render debug geometry for one point-physics instance.
 * state is a small stack/live record: state[0]=object datum handle,
 * state[1]=point-physics definition tag data, state+0x8 = the instance's
 * 4x3 transform matrix (its rows at +0xc and +0x24 are also passed on
 * directly as the forward/up vectors for the origin marker).
 * Resolves the object's 'obje' tag, draws the instance origin marker scaled
 * by obje_tag+0x4, then walks the 0x80-byte tag block at definition+0x74
 * (per-point entries), transforming point +0x38 and vectors +0x44/+0x50 into
 * world space and drawing a sphere of radius +0x68 plus a second marker
 * scaled by +0x68 * 0.5f.
 * Confirmed via disassembly 0x1508b0-0x1508b5. Loop index is a short:
 * INC ECX / MOVSX ECX,CX before the CMP against the reloaded block count,
 * and state[1] is reloaded from the parameter every iteration.
 * 0x2ee6c4 is the debug-draw default color pointer, 0x253398 is 0.5f.
 */
void FUN_001508b0(int *state)
{
  float *matrix;
  int *object;
  void *obje_tag;
  void *elem;
  short i;
  float local_34[3];
  float local_28[3];
  float local_1c[3];
  float local_10[3];

  object = (int *)object_get_and_verify_type(*state, -1);
  obje_tag = tag_get(0x6f626a65 /* 'obje' */, *object);

  matrix = (float *)(state + 2);
  matrix_transform_point(matrix, (float *)(state[1] + 0xc), local_1c);
  FUN_0018a990(1, local_1c, (float *)(state + 3), (float *)(state + 9),
               *(float *)((char *)obje_tag + 0x4));

  for (i = 0; i < *(int *)(state[1] + 0x74); i++) {
    elem = tag_block_get_element((void *)(state[1] + 0x74), i, 0x80);
    matrix_transform_point(matrix, (float *)((char *)elem + 0x38), local_10);
    matrix_transform_vector(matrix, (float *)((char *)elem + 0x44), local_34);
    matrix_transform_vector(matrix, (float *)((char *)elem + 0x50), local_28);
    FUN_00189540(1, local_10, *(float *)((char *)elem + 0x68),
                 *(void **)0x2ee6c4);
    FUN_0018a990(1, local_10, local_34, local_28,
                 *(float *)((char *)elem + 0x68) * *(const float *)0x253398);
  }
}

/* 0x1509c0 — physics_instance_new (physics.c): fill a
 * physics_instance for an object that has a 'phys' tag (obje +0x8c tag index,
 * CMP ECX,-0x1 at 0x1509e8), building its world matrix and moving the matrix
 * origin to the negated centre of mass.  Returns FALSE (XOR AL,AL) when the
 * object has no physics. */
boolean FUN_001509c0(void *instance_data, int object_index)
{
  struct physics_instance *instance;
  object_datum_t *object;
  char *definition;
  vector3_t center_of_mass;

  instance = (struct physics_instance *)instance_data;
  object = (object_datum_t *)object_get_and_verify_type(object_index, -1);
  definition =
    (char *)tag_get(0x6f626a65 /* 'obje' */, object->definition_index);
  if (*(int *)(definition + 0x8c) != -1) {
    instance->object_index = object_index;
    instance->physics = (const struct physics_definition *)tag_get(
      0x70687973 /* 'phys' */, *(int *)(definition + 0x8c));

    instance->world_matrix.scale = 1.0f;
    object_get_world_position(object_index, &instance->world_matrix.position);
    object_get_orientation(object_index, &instance->world_matrix.forward.x,
                           &instance->world_matrix.up.x);
    cross_product3d_inline(
      (const real_vector3d *)&instance->world_matrix.up,
      (const real_vector3d *)&instance->world_matrix.forward,
      (real_vector3d *)&instance->world_matrix.left);

    set_real_point3d_inline(&center_of_mass,
                            -instance->physics->center_of_mass.x,
                            -instance->physics->center_of_mass.y,
                            -instance->physics->center_of_mass.z);
    matrix_transform_point(
      (float *)&instance->world_matrix, &center_of_mass.x,
      &center_of_mass.x); /* dup-args-ok: 0x150a40/0x150a47 */
    instance->world_matrix.position = center_of_mass;

    return true;
  }

  return false;
}

/* 0x150b60 — physics_test_vector (physics.c): cast a
 * ray against every mass-point sphere of the instance (in the instance's
 * local frame) and keep the nearest hit plane, transformed back to world
 * space.  plane3d_from_point_and_normal is inlined here:
 * normal copied as three dwords at 0x150bfa-0x150c19, d = dot(intersection,
 * normal) at 0x150c29-0x150c39. */
boolean FUN_00150b60(void *instance_data, void *point, void *vector,
                     void *result_data)
{
  const struct physics_instance *instance;
  struct physics_test_vector_result *result;
  boolean hit;
  real_point3d local_point;
  vector3_t local_vector;
  real_vector3d normal;
  short mass_point_index;

  instance = (const struct physics_instance *)instance_data;
  result = (struct physics_test_vector_result *)result_data;
  hit = false;
  result->t = 3.4028235e+38f;

  real_matrix3x3_transform_point((void *)&instance->world_matrix,
                                 (float *)point, &local_point.x);
  real_matrix3x3_transform_vector((void *)&instance->world_matrix,
                                  (vector3_t *)vector, &local_vector);

  for (mass_point_index = 0;
       mass_point_index < instance->physics->mass_points.count;
       mass_point_index++) {
    const struct mass_point_definition *mass_point;
    float t;

    mass_point = (const struct mass_point_definition *)tag_block_get_element(
      (void *)&instance->physics->mass_points, mass_point_index, 0x80);
    if (sphere_test_vector3d((float *)&mass_point->position, mass_point->radius,
                             &local_point.x, &local_vector.x, &t, &normal.i) &&
        result->t > t) {
      real_point3d intersection;

      result->t = t;
      intersection.x = local_vector.x * t + local_point.x;
      intersection.y = local_vector.y * t + local_point.y;
      intersection.z = local_vector.z * t + local_point.z;
      plane3d_from_point_and_normal_inline(&result->plane, &intersection,
                                           &normal);
      hit = true;
    }
  }

  if (hit)
    FUN_0010a1c0(
      (float *)&instance->world_matrix, (float *)&result->plane,
      (float *)&result->plane); /* dup-args-ok: PUSH ESI x2 @0x150c5e */

  return hit;
}

/* 0x150c80 — recompute a mass point's ground plane from a downward collision
 * test.  `mass_point` arrives in ECX (MOV EBX,ECX at 0x150c94); the two stack
 * parameters are the collision-test ignore handle ([EBP+8], forwarded as
 * FUN_0014ec30's param6) and the per-point definition element ([EBP+0xc],
 * whose float at +0x68 is the point radius — same field FUN_001508b0 draws the
 * debug sphere with).
 * mass_point layout used here (offsets from disassembly 0x150c80-0x150dcd):
 *   +0x00 dword flags (bit 2 = "no ground"), +0x04 float position[3],
 *   +0x60 float ground_plane[4], +0x70 int16 ground_material_type,
 *   +0x74 float ground plane distance.
 * The +0x70/-1 sentinel and material-type range are named by the assert string
 * at 0x150db3 ("mass_point->ground_material_type==NONE || ...").
 * 0x32513c..0x325148 is the default (flat, downward) plane constant, copied as
 * four dwords exactly as the reference does (MOV EDX,[0x32513c] / MOV
 * [ECX],EDX). x87 addend order at 0x150cc2-0x150ceb is per-site: (k*z + j*y) +
 * i*x, then FSUB plane_d, then FSUBR radius. FUN_0014ec30's search_radius and
 * dist_a are the SAME value in the reference (MOV ECX,[EAX+0x68]; PUSH ECX;
 * PUSH 0; MOV EDX,ECX; PUSH EDX) — the repeated argument is binary-confirmed,
 * not a transcription slip. hit_info (0x2c bytes at EBP-0x30) fields consumed:
 * +0x00 plane distance, +0x10..+0x1c plane, +0x20 object handle, +0x28 flag
 * byte (bit 3), +0x2a collision-function index (loaded into SI for
 * FUN_00150840).
 */
void compute_ground_plane(int ignore_object_handle, void *point_definition,
                          void *mass_point_arg)
{
  unsigned char *mass_point;
  unsigned char *plane;
  float *position;
  int hit_object;
  short material_type;
  unsigned char los_scratch[44040];
  unsigned char hit_info[0x2c];

  mass_point = (unsigned char *)mass_point_arg;
  plane = mass_point + 0x60;
  position = (float *)(mass_point + 4);

  *(int *)(plane + 0x0) = *(int *)0x32513c;
  *(int *)(plane + 0x4) = *(int *)0x325140;
  *(int *)(plane + 0x8) = *(int *)0x325144;
  *(int *)(plane + 0xc) = *(int *)0x325148;
  *(short *)(mass_point + 0x70) = -1;

  *(float *)(mass_point + 0x74) = *(float *)((char *)point_definition + 0x68) -
                                  ((*(float *)(plane + 0x8) * position[2] +
                                    *(float *)(plane + 0x4) * position[1] +
                                    *(float *)(plane + 0x0) * position[0]) -
                                   *(float *)(plane + 0xc));

  if (FUN_0014ec30(0xc0a0, position,
                   *(float *)((char *)point_definition + 0x68), 0.0f,
                   *(float *)((char *)point_definition + 0x68),
                   ignore_object_handle, los_scratch) &&
      collision_features_test_los(los_scratch, position, hit_info)) {
    *(int *)(plane + 0x0) = *(int *)(hit_info + 0x10);
    *(int *)(plane + 0x4) = *(int *)(hit_info + 0x14);
    *(int *)(plane + 0x8) = *(int *)(hit_info + 0x18);
    *(int *)(plane + 0xc) = *(int *)(hit_info + 0x1c);
    *(int *)(mass_point + 0x74) = *(int *)(hit_info + 0x00);

    hit_object = *(int *)(hit_info + 0x20);
    *(short *)(mass_point + 0x70) =
      FUN_00150840(hit_object, *(short *)(hit_info + 0x2a));

    if ((hit_info[0x28] & 8) == 0 &&
        (hit_object == -1 ||
         ((1 << (object_get_type(hit_object) & 0x1f)) & 0x40) != 0)) {
      *(int *)mass_point = *(int *)mass_point & 0xfffffffb;
    } else {
      *(int *)mass_point = *(int *)mass_point | 4;
    }

    if (hit_object != -1) {
      object_deplete_shield(hit_object);
    }
  }

  material_type = *(short *)(mass_point + 0x70);
  if (material_type != -1 && (material_type < 0 || material_type > 0x20)) {
    display_assert("mass_point->ground_material_type==NONE || "
                   "(mass_point->ground_material_type>=0 && "
                   "mass_point->ground_material_type<NUMBER_OF_MATERIAL_TYPES)",
                   "c:\\halo\\SOURCE\\physics\\physics.c", 0x152, 1);
    system_exit(-1);
  }
}

/* 0x150dd0 — friction_evaluate (physics.c, static): split a mass
 * point's friction vector into parallel/perpendicular components along the
 * axis selected by friction_type (0 point, 1 forward, 2 left, 3 up), scale
 * each, and recombine.  Register ABI from the disassembly: friction_type in
 * AX (TEST AX,AX @150dd6), friction in ESI, forward in ECX, up in EDX (the
 * 0x10b8a0 / cross_product3d pushes); the two scales are the stack args.
 * friction is declared last so the ESI arg lands in a callee-save thunk slot.
 * The left axis goes through the out-of-line cross_product3d (CALL 0x178d0).
 * The default case asserts with a NULL message at line 0x17f (383). */
void friction_evaluate(short friction_type, float parallel_scale,
                       float perpendicular_scale, float *forward, float *up,
                       void *friction_data)
{
  struct friction_datum *friction;
  real_vector3d left;

  friction = (struct friction_datum *)friction_data;
  if (friction_type == 0) {
    friction->parallel = friction->friction;
    friction->perpendicular.i = 0.0f;
    friction->perpendicular.j = 0.0f;
    friction->perpendicular.k = 0.0f;
    return;
  }

  switch (friction_type) {
  case 1:
    FUN_0010b8a0(&friction->friction.i, forward, &friction->parallel.i,
                 &friction->perpendicular.i);
    break;
  case 2:
    cross_product3d(up, forward, &left.i);
    FUN_0010b8a0(&friction->friction.i, &left.i, &friction->parallel.i,
                 &friction->perpendicular.i);
    break;
  case 3:
    FUN_0010b8a0(&friction->friction.i, up, &friction->parallel.i,
                 &friction->perpendicular.i);
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\physics\\physics.c", 0x17f, 1);
    system_exit(-1);
    break;
  }

  /* In-place scales: scale_vector3d(v, s, v). */
  scale_vector3d_inline(&friction->parallel, parallel_scale,
                        &friction->parallel); /* dup-args-ok */
  scale_vector3d_inline(&friction->perpendicular, perpendicular_scale,
                        &friction->perpendicular); /* dup-args-ok */
  add_vectors3d_inline(&friction->parallel, &friction->perpendicular,
                       &friction->friction);
}

/* set_real_vector3d (real_math.h inline), components by value. */
static __inline real_vector3d *set_real_vector3d_inline(real_vector3d *v,
                                                        real i, real j, real k)
{
  v->i = i;
  v->j = j;
  v->k = k;
  return v;
}

/* dot_product3d with the addend order the 2276 physics_compute_new
 * sites use: (k + j) + i (e.g. 0x1511bc-0x1511d5, 0x15129b-0x1512b2).
 * The collision-normal dot at 0x1517ce-0x1517eb is (j + k) + i and is
 * spelled out at its site. */
static __inline real dot_product3d_kji_inline(const real_vector3d *a,
                                              const real_vector3d *b)
{
  return a->k * b->k + a->j * b->j + a->i * b->i;
}

/* magnitude_squared3d as inlined at 0x151656-0x15167e: the three
 * components are loaded once (FLD k/j/i) and squared from the FPU stack. */
static __inline real magnitude_squared3d_inline(const real_vector3d *v)
{
  real i = v->i;
  real j = v->j;
  real k = v->k;

  return i * i + j * j + k * k;
}

/* normalize3d as inlined twice in physics_update_old (0x153f3b-0x153f89,
 * 0x153fbf-0x15400a): magnitude_squared3d form (components loaded once),
 * FSQRT, then the double epsilon at 0x2533d0 (1e-4) and inverse * component
 * (FDIVR 1.0f; FMUL [v]).  The magnitude is discarded. */
static __inline void normalize3d_squared_inline(real_vector3d *v)
{
  x87_wide_t magnitude = x87_sqrtd(magnitude_squared3d_inline(v));

  if (!(fabs(magnitude) < *(const double *)0x2533d0)) {
    x87_wide_t inverse = 1.0f / magnitude;

    v->i = inverse * v->i;
    v->j = inverse * v->j;
    v->k = inverse * v->k;
  }
}

/* valid_real / valid_real_vector3d as inlined in physics_update_old
 * (0x152f4e-0x152f8b): each component's bits are spilled and tested
 * against the exponent mask, i then j then k. */
static __inline boolean valid_real_inline(real value)
{
  return (*(uint32_t *)&value & 0x7f800000) != 0x7f800000;
}

static __inline boolean valid_real_vector3d_inline(const real_vector3d *v)
{
  return valid_real_inline(v->i) && valid_real_inline(v->j) &&
         valid_real_inline(v->k);
}

/* assert_valid_real_vector3d_axes2 predicate as inlined at 0x15412f-0x1541fb:
 * |forward|^2 - 1, |up|^2 - 1 (magnitude_squared3d form) and the k,j,i dot
 * must each be a valid real with magnitude below the double 0.001 at
 * 0x2549d8. */
static __inline boolean valid_real_near_zero_inline(real value)
{
  return valid_real_inline(value) && fabs(value) < *(const double *)0x2549d8;
}

static __inline boolean valid_real_axes2_inline(const real_vector3d *forward,
                                                const real_vector3d *up)
{
  return valid_real_near_zero_inline(magnitude_squared3d_inline(forward) -
                                     1.0f) &&
         valid_real_near_zero_inline(magnitude_squared3d_inline(up) - 1.0f) &&
         valid_real_near_zero_inline(dot_product3d_kji_inline(forward, up));
}

/* point_from_line3d (real_math.h inline): result = v*t + p.  The
 * 2276 physics_compute_new builds velocity_relative_to_ground, the powered
 * velocities and every powered_force accumulation with it: each site is a
 * single FMUL-then-FADD of the base component with no intermediate store
 * (e.g. 0x15122b-0x151247, 0x1516dd-0x151708). */
static __inline real_vector3d *point_from_line3d_inline(const real_vector3d *p,
                                                        const real_vector3d *v,
                                                        real t,
                                                        real_vector3d *result)
{
  result->i = v->i * t + p->i;
  result->j = v->j * t + p->j;
  result->k = v->k * t + p->k;
  return result;
}

/* 0x150ed0 — physics_compute_new (physics.c): per-mass-point
 * transform, ground/water/air friction, powered lift/thrust/antigrav, and the
 * summed force and torque about the object's position.
 *
 * Binary details that are easy to get wrong (0x150ed0-0x151a41):
 *  - gravity = global_gravity * gravity_scale (FLD [0x32512c]; FMUL [+0x1c]).
 *  - the powered definition/datum pair is two ternaries: a NULL
 *    tag_block_get_element result also clears the datum (TEST EAX,EAX at
 *    0x150f8a).
 *  - normal_force_magnitude = (ground_depth_mp / ground_depth * gravity -
 *    normal_velocity * damp) * mass (FDIVRP ST2 at 0x1511dd).
 *  - water_pressure_magnitude = water_density / density * mass *
 *    depth_fraction * gravity (0x1513e2-0x1513f0).
 *  - water lift = |dot| * mass * depth_fraction * water_lift_ratio; air lift
 *    = |dot| * air_lift_ratio * mass.
 *  - antigrav probe length (height + radius) is recomputed after the
 *    collision test (0x151774), not kept in a local.
 *  - mass_point->force starts at the memset zero and accumulates all six
 *    contributions (FLD [+0x84]; FADD [+0x118] at 0x151844).
 * 0x32512c global_gravity, 0x31fc50 global_down3d pointer, 0xc0a0 the
 * collision flags pushed to FUN_0014df70 (collision_test_vector). */
void physics_compute_new(
  const struct physics_instance *instance,
  const struct powered_mass_point_datum *powered_mass_points,
  struct mass_point_datum *mass_points, real_vector3d *total_force,
  real_vector3d *total_torque)
{
  object_datum_t *object;
  const struct physics_definition *physics;
  real gravity;
  short mass_point_index;

  object =
    (object_datum_t *)object_get_and_verify_type(instance->object_index, -1);
  physics = instance->physics;
  gravity = *(float *)0x32512c * physics->gravity_scale;

  set_real_vector3d_inline(total_force, 0.0f, 0.0f, -physics->mass * gravity);
  set_real_vector3d_inline(total_torque, 0.0f, 0.0f, 0.0f);

  csmemset(mass_points, 0,
           physics->mass_points.count * sizeof(struct mass_point_datum));

  for (mass_point_index = 0; mass_point_index < physics->mass_points.count;
       mass_point_index++) {
    const struct mass_point_definition *mass_point_definition;
    struct mass_point_datum *mass_point;
    const struct powered_mass_point_definition *powered_mass_point_definition;
    const struct powered_mass_point_datum *powered_mass_point;
    const real_vector3d *ground_normal;

    mass_point_definition =
      (const struct mass_point_definition *)tag_block_get_element(
        (void *)&physics->mass_points, mass_point_index,
        sizeof(struct mass_point_definition));
    mass_point = mass_points + mass_point_index;
    powered_mass_point_definition =
      mass_point_definition->powered_mass_point_index != -1 &&
          powered_mass_points ?
        (const struct powered_mass_point_definition *)tag_block_get_element(
          (void *)&physics->powered_mass_points,
          mass_point_definition->powered_mass_point_index,
          sizeof(struct powered_mass_point_definition)) :
        NULL;
    powered_mass_point =
      powered_mass_point_definition ?
        powered_mass_points + mass_point_definition->powered_mass_point_index :
        NULL;

    mass_point->flags = 0;
    matrix_transform_point((float *)&instance->world_matrix,
                           (float *)&mass_point_definition->position,
                           &mass_point->position.x);
    if (powered_mass_point) {
      real_matrix4x3 powered_matrix;

      matrix4x3_multiply((float *)&instance->world_matrix,
                         (float *)&powered_mass_point->rotation_matrix,
                         (float *)&powered_matrix);
      matrix_transform_vector((float *)&powered_matrix,
                              (float *)&mass_point_definition->forward,
                              &mass_point->forward.i);
      matrix_transform_vector((float *)&powered_matrix,
                              (float *)&mass_point_definition->up,
                              &mass_point->up.i);
    } else {
      matrix_transform_vector((float *)&instance->world_matrix,
                              (float *)&mass_point_definition->forward,
                              &mass_point->forward.i);
      matrix_transform_vector((float *)&instance->world_matrix,
                              (float *)&mass_point_definition->up,
                              &mass_point->up.i);
    }
    scenario_location_from_point(mass_point->location, &mass_point->position);

    vector_from_points3d_inline(&object->position, &mass_point->position,
                                &mass_point->radius);
    cross_product3d_inline(&object->angular_velocity, &mass_point->radius,
                           &mass_point->velocity);
    add_vectors3d_inline(&mass_point->velocity, &object->translational_velocity,
                         &mass_point->velocity); /* dup-args-ok: in-place */

    compute_ground_plane(instance->object_index, (void *)mass_point_definition,
                         mass_point);
    mass_point->water_depth =
      FUN_0018f510(mass_point->location, &mass_point->position);

    ground_normal = (const real_vector3d *)mass_point->ground_plane.normal;
    if (mass_point->ground_depth > 0.0f) {
      const struct material_definition *material;
      x87_wide_t ground_friction; /* ST0-resident @151128..15121e */
      real ground_normal_k1;
      real ground_normal_k0;
      real ground_depth;
      real ground_damp_fraction;
      real normal_velocity;
      real ground_scale;

      material = (const struct material_definition *)FUN_0018e500(
        mass_point->ground_material_type);
      ground_friction = material->physics_ground_friction_scale > 0.0f &&
                            physics->mass <= 7500.0f ?
                          (x87_wide_t)physics->ground_friction *
                            material->physics_ground_friction_scale :
                          physics->ground_friction;
      ground_normal_k1 =
        material->physics_ground_friction_normal_k1_scale > 0.0f ?
          physics->ground_normal_k1 *
            material->physics_ground_friction_normal_k1_scale :
          physics->ground_normal_k1;
      ground_normal_k0 =
        material->physics_ground_friction_normal_k0_scale > 0.0f ?
          physics->ground_normal_k0 *
            material->physics_ground_friction_normal_k0_scale :
          physics->ground_normal_k0;
      ground_depth =
        material->physics_ground_depth_scale > 0.0f ?
          physics->ground_depth * material->physics_ground_depth_scale :
          physics->ground_depth;
      ground_damp_fraction =
        material->physics_ground_damp_fraction_scale > 0.0f ?
          physics->ground_damp_fraction *
            material->physics_ground_damp_fraction_scale :
          physics->ground_damp_fraction;

      normal_velocity =
        dot_product3d_kji_inline(&mass_point->velocity, ground_normal);
      HALO_FLT_ROUNDTRIP(normal_velocity); /* FSTP [EBP-0x14] @1511d7 */
      mass_point->normal_force_magnitude =
        (mass_point->ground_depth / ground_depth * *(float *)0x32512c -
         normal_velocity * ground_damp_fraction) *
        physics->mass;
      scale_vector3d_inline(ground_normal, mass_point->normal_force_magnitude,
                            &mass_point->normal_force);
      ground_scale = -mass_point_definition->mass * ground_friction;
      HALO_FLT_ROUNDTRIP(ground_scale); /* FSTP [EBP-0x4] @151223 */
      point_from_line3d_inline(&mass_point->velocity, ground_normal,
                               -normal_velocity,
                               &mass_point->velocity_relative_to_ground);
      scale_vector3d_inline(&mass_point->velocity_relative_to_ground,
                            ground_scale,
                            &mass_point->ground_friction.friction);

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_ground_friction_bit) &&
          powered_mass_point->ground_friction_velocity != 0.0f) {
        real fraction;
        real alignment;
        real weight;
        real_vector3d powered_velocity;
        real_vector3d projected_velocity;

        fraction =
          pin_fraction(ground_normal->k, ground_normal_k0, ground_normal_k1);
        alignment = dot_product3d_kji_inline(&mass_point->up, ground_normal);
        alignment =
          alignment < 0.0f ? 0.0f : (alignment > 1.0f ? 1.0f : alignment);
        weight = alignment * alignment * fraction * fraction * ground_scale;

        scale_vector3d_inline(&mass_point->forward,
                              -powered_mass_point->ground_friction_velocity,
                              &powered_velocity);
        point_from_line3d_inline(
          &powered_velocity, ground_normal,
          -dot_product3d_kji_inline(&powered_velocity, ground_normal),
          &projected_velocity);
        add_vectors3d_inline(
          &mass_point->velocity_relative_to_ground, &projected_velocity,
          &mass_point->velocity_relative_to_ground); /* dup-args-ok */
        point_from_line3d_inline(
          &mass_point->ground_friction.friction, &projected_velocity, weight,
          &mass_point->ground_friction.friction); /* dup-args-ok */
      }

      friction_evaluate(mass_point_definition->friction_type,
                        mass_point_definition->friction_parallel_scale,
                        mass_point_definition->friction_perpendicular_scale,
                        &mass_point->forward.i, &mass_point->up.i,
                        &mass_point->ground_friction);
    }

    if (mass_point->water_depth > 0.0f) {
      real depth_fraction;

      depth_fraction = mass_point->water_depth < physics->water_depth ?
                         mass_point->water_depth / physics->water_depth :
                         1.0f;
      HALO_FLT_ROUNDTRIP(depth_fraction); /* FSTP [EBP-0x18] @1513b6 */
      if (mass_point_definition->density > 0.0f &&
          physics->water_depth > 0.0f) {
        mass_point->water_pressure_magnitude =
          physics->water_density / mass_point_definition->density *
          mass_point_definition->mass * depth_fraction * gravity;
        set_real_vector3d_inline(&mass_point->water_pressure, 0.0f, 0.0f,
                                 mass_point->water_pressure_magnitude);
      }

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_water_friction_bit) &&
          powered_mass_point->water_friction_velocity != 0.0f) {
        real_vector3d powered_velocity;

        point_from_line3d_inline(&mass_point->velocity, &mass_point->forward,
                                 -powered_mass_point->water_friction_velocity,
                                 &powered_velocity);
        scale_vector3d_inline(&powered_velocity,
                              -mass_point_definition->mass *
                                physics->water_friction,
                              &mass_point->water_friction.friction);
      } else {
        scale_vector3d_inline(&mass_point->velocity,
                              -mass_point_definition->mass *
                                physics->water_friction,
                              &mass_point->water_friction.friction);
      }

      friction_evaluate(mass_point_definition->friction_type,
                        mass_point_definition->friction_parallel_scale,
                        mass_point_definition->friction_perpendicular_scale,
                        &mass_point->forward.i, &mass_point->up.i,
                        &mass_point->water_friction);

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_water_lift_bit) &&
          powered_mass_point->water_lift_ratio != 0.0f) {
        real lift = x87_fabs(dot_product3d_kji_inline(&mass_point->forward,
                                                      &mass_point->velocity)) *
                    physics->mass * depth_fraction *
                    powered_mass_point->water_lift_ratio;

        point_from_line3d_inline(&mass_point->powered_force, &mass_point->up,
                                 lift,
                                 &mass_point->powered_force); /* dup-args-ok */
      }
    }

    {
      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_air_friction_bit) &&
          powered_mass_point->air_friction_velocity != 0.0f) {
        real_vector3d powered_velocity;

        point_from_line3d_inline(&mass_point->velocity, &mass_point->forward,
                                 -powered_mass_point->air_friction_velocity,
                                 &powered_velocity);
        scale_vector3d_inline(&powered_velocity,
                              -mass_point_definition->mass *
                                physics->air_friction,
                              &mass_point->air_friction.friction);
      } else {
        scale_vector3d_inline(&mass_point->velocity,
                              -mass_point_definition->mass *
                                physics->air_friction,
                              &mass_point->air_friction.friction);
      }

      friction_evaluate(mass_point_definition->friction_type,
                        mass_point_definition->friction_parallel_scale,
                        mass_point_definition->friction_perpendicular_scale,
                        &mass_point->forward.i, &mass_point->up.i,
                        &mass_point->air_friction);

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_air_lift_bit) &&
          powered_mass_point->air_lift_ratio != 0.0f) {
        real lift = x87_fabs(dot_product3d_kji_inline(&mass_point->forward,
                                                      &mass_point->velocity)) *
                    powered_mass_point->air_lift_ratio * physics->mass;

        point_from_line3d_inline(&mass_point->powered_force, &mass_point->up,
                                 lift,
                                 &mass_point->powered_force); /* dup-args-ok */
      }
    }

    SET_FLAG(mass_point->flags, _point_at_rest_bit,
             magnitude_squared3d_inline(&mass_point->velocity) < 0.0011111111f);
    SET_FLAG(mass_point->flags, _point_on_ground_bit,
             mass_point->ground_depth > 0.0f);
    SET_FLAG(mass_point->flags, _point_in_water_bit,
             mass_point->water_depth > 0.0f);

    if (powered_mass_point_definition) {
      if (TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_thrust_bit)) {
        point_from_line3d_inline(
          &mass_point->powered_force, &mass_point->forward,
          powered_mass_point->thrust_fraction * physics->mass,
          &mass_point->powered_force); /* dup-args-ok */
      }

      if (TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_antigrav_bit)) {
        real_point3d probe_point;
        real_vector3d probe_vector;
        struct collision_result collision;

        probe_point = mass_point->position;
        scale_vector3d_inline(*(const real_vector3d **)0x31fc50,
                              powered_mass_point_definition->antigrav_height +
                                mass_point_definition->radius,
                              &probe_vector);

        if (FUN_0014df70(0xc0a0, &probe_point.x, &probe_vector.i,
                         instance->object_index, (int16_t *)&collision)) {
          real height;
          real alignment;
          real ground_effect;
          real magnitude;

          height = (powered_mass_point_definition->antigrav_height +
                    mass_point_definition->radius) *
                     collision.t -
                   mass_point_definition->radius;
          HALO_FLT_ROUNDTRIP(height); /* FSTP [EBP-0x14] @15178f */
          alignment = pin_fraction(
            mass_point->up.k, powered_mass_point_definition->antigrav_normal_k0,
            powered_mass_point_definition->antigrav_normal_k1);
          ground_effect =
            height > 0.0f ?
              1.0f - height / powered_mass_point_definition->antigrav_height :
              1.0f;
          magnitude =
            (ground_effect * ground_effect * *(float *)0x32512c -
             (collision.plane.normal[1] * mass_point->velocity.j +
              collision.plane.normal[2] * mass_point->velocity.k +
              collision.plane.normal[0] * mass_point->velocity.i) *
               powered_mass_point_definition->antigrav_damp_fraction) *
            powered_mass_point->antigrav_fraction *
            powered_mass_point_definition->antigrav_strength * physics->mass *
            alignment;

          point_from_line3d_inline(
            &mass_point->powered_force,
            (const real_vector3d *)collision.plane.normal, magnitude,
            &mass_point->powered_force); /* dup-args-ok */
          SET_FLAG(mass_point->flags, _point_antigraving_bit, true);
        }
      }
    }

    add_vectors3d_inline(&mass_point->force, &mass_point->normal_force,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force,
                         &mass_point->ground_friction.friction,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force, &mass_point->water_pressure,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force,
                         &mass_point->water_friction.friction,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force, &mass_point->air_friction.friction,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force, &mass_point->powered_force,
                         &mass_point->force); /* dup-args-ok */
    cross_product3d_inline(&mass_point->radius, &mass_point->force,
                           &mass_point->torque);

    add_vectors3d_inline(total_force, &mass_point->force,
                         total_force); /* dup-args-ok */
    add_vectors3d_inline(total_torque, &mass_point->torque,
                         total_torque); /* dup-args-ok */
  }
}

/* 0x151a50 — physics_compute_biped_collision (physics.c, static):
 * test a biped's physics pill against a vehicle's collision model; on
 * contact shove the biped away from the vehicle, try to depenetrate it, and
 * apply the game-globals falling_damage vehicle_collision_damage (+0x68 index)
 * and vehicle_killed_unit_damage_effect (+0x58 index) effects.
 * 0x1a0890 is biped_get_physics_pill (base, height, width), 0x14c950
 * collision_model_test_point, 0x14cde0 collision_model_get_features_in_sphere
 * (height/width passed as raw dwords, MOV/PUSH at 0x151ae5-0x151af7),
 * 0x14bc10 collision_features_test_point, 0x14f020 collision_fix_pill and
 * 0x1a4a70 biped_accelerate.  Unit fields: last_vehicle_index +0x2dc,
 * game_time_at_last_vehicle_exit +0x2e0, driver_object_index +0x2d4; object
 * owner team/player/object +0x68/+0x70/+0x74; bounding_sphere_center +0x50;
 * unit_definition blip_type +0x298 indexes the static scales[3] at 0x32514c.
 * The assert compares against NUMBEROF(scales), hence the unsigned JC. */
boolean physics_compute_biped_collision(int *instance, int biped_index)
{
  boolean collision;
  real_point3d base;
  real height;
  real width;

  collision = false;
  biped_get_camera_height_and_offset(biped_index, (vector3_t *)&base, &height,
                                     &width);

  if (FUN_0014c950((int)instance, &base)) {
    collision = true;
  } else {
    unsigned char features[0xac08];
    unsigned char collision_plane[0x2c];
    real_point3d center;
    real radius;
    real feature_width;

    collision_features_init(features);
    set_real_point3d_inline((vector3_t *)&center, base.x, base.y,
                            base.z + height * 0.5f);
    radius = height * 0.5f + width;
    feature_width =
      width - 0.015625f > 0.015625f ? width - 0.015625f : 0.015625f;
    FUN_0014cde0((int)instance, (int)&center, radius, *(int *)&height,
                 *(int *)&feature_width, (int)features);
    if (collision_features_test_los(features, &base, collision_plane))
      collision = true;
  }

  if (collision) {
    char *vehicle = (char *)object_get_and_verify_type(*instance, 2);
    char *biped = (char *)object_get_and_verify_type(biped_index, 1);
    real_vector3d *vehicle_velocity = (real_vector3d *)(vehicle + 0x18);
    real vehicle_speed = x87_sqrt(dot_product3d_inline(
      vehicle_velocity, vehicle_velocity)); /* dup-args-ok: in-place */
    real_vector3d acceleration;
    real_point3d new_position;
    boolean cause_damage = true;

    vector_from_points3d_inline((real_point3d *)(vehicle + 0x50),
                                (real_point3d *)(biped + 0x50), &acceleration);
    normalize3d(&acceleration.i);
    acceleration.k += 0.8f;
    normalize3d(&acceleration.i);
    scale_vector3d_inline(&acceleration,
                          vehicle_speed > 0.1f ? vehicle_speed : 0.1f,
                          &acceleration); /* dup-args-ok: in-place */
    add_vectors3d_inline(&acceleration, vehicle_velocity,
                         &acceleration); /* dup-args-ok: in-place */
    scale_vector3d_inline(&acceleration, 0.5f,
                          &acceleration); /* dup-args-ok: in-place */
    FUN_001a4a70(biped_index, &acceleration.i);

    base.x += acceleration.i * 2.0f;
    base.y += acceleration.j * 2.0f;
    base.z += acceleration.k * 2.0f;

    if (FUN_0014f020(0x20c3a0, &base.x, width * 2.0f, height, width,
                     biped_index, &new_position.x)) {
      new_position.z -= width;
      object_translate(biped_index, &new_position.x, NULL);

      cause_damage =
        (*instance != *(int *)(biped + 0x2dc) ||
         game_time_get() > *(int *)(biped + 0x2e0) + 90) &&
        (vehicle_speed > 0.06666667f ||
         distance_squared3d(&vehicle_velocity->i,
                            (const float *)(biped + 0x18)) > 0.0011111111f);
    }

    if (cause_damage) {
      char *falling_damage = (char *)tag_block_get_element(
        (char *)game_globals_get() + 0x188, 0, 0x98);

      if (*(int *)(falling_damage + 0x68) != -1) {
        int responsible_object_index = *instance;
        char *responsible_object = vehicle;
        struct damage_data damage;

        if (*(int *)(vehicle + 0x2d4) != -1) {
          responsible_object_index = *(int *)(vehicle + 0x2d4);
          responsible_object =
            (char *)object_get_and_verify_type(responsible_object_index, -1);
        }

        damage_data_new(&damage, *(int *)(falling_damage + 0x68));
        damage.scale = 1.0f;
        SET_FLAG(damage.flags, _damage_area_of_effect_bit, true);
        damage.owner_player_index = *(int *)(responsible_object + 0x70);
        damage.owner_object_index = *(int *)(responsible_object + 0x74) != -1 ?
                                      *(int *)(responsible_object + 0x74) :
                                      responsible_object_index;
        damage.owner_team_index = *(short *)(responsible_object + 0x68);
        damage.origin = *(real_point3d *)(biped + 0x50);
        damage.epicenter = *(real_point3d *)(vehicle + 0x50);
        damage.direction = acceleration;
        normalize3d(&damage.direction.i);
        object_cause_damage(&damage, biped_index, -1, -1, -1, NULL);
      }

      if (*(int *)(falling_damage + 0x58) != -1) {
        char *unit_definition = (char *)tag_get(0x756e6974, *(int *)biped);
        struct damage_data damage;

        damage_data_new(&damage, *(int *)(falling_damage + 0x58));
        assert_halt_msg_at("unit_definition->unit.blip_type>=0 && "
                           "unit_definition->unit.blip_type<NUMBEROF(scales)",
                           "c:\\halo\\SOURCE\\physics\\physics.c", 0x33e,
                           *(short *)(unit_definition + 0x298) >= 0 &&
                             *(short *)(unit_definition + 0x298) < 3u);
        damage.scale = ((float *)0x32514c)[*(short *)(unit_definition + 0x298)];
        damage.origin = *(real_point3d *)(biped + 0x50);
        scale_vector3d_inline(&acceleration, -1.0f, &damage.direction);
        object_cause_damage(&damage, *instance, -1, -1, -1, NULL);
      }
    }
  }

  return collision;
}

/* 0x151ec0 — physics_compute_vehicle_collision (physics.c, static):
 * sphere-vs-sphere test of every mass point of instance0 against every mass
 * point of instance1; penetrating pairs push the two vehicles apart and the
 * summed force/torque is added to each vehicle's collision_force (+0x460) /
 * collision_torque (+0x46c), clearing _object_at_rest_bit (AND ~0x20).
 * instance1 only receives its share when its physics radius is not > 0.
 * 0x32512c is global_gravity and 0x325138 global_physics_collision_depth
 * (Bungie names); 0x253398 is 0.5f. */
boolean
physics_compute_vehicle_collision(const struct physics_instance *instance0,
                                  const struct physics_instance *instance1)
{
  boolean collision;
  char *object0;
  char *object1;
  real mass_scale;
  real_vector3d force0;
  real_vector3d force1;
  real_vector3d torque0;
  real_vector3d torque1;
  short mass_point0_index;

  collision = false;
  object0 = (char *)object_get_and_verify_type(instance0->object_index, 2);
  object1 = (char *)object_get_and_verify_type(instance1->object_index, 2);
  mass_scale = x87_sqrt(instance0->physics->mass * instance1->physics->mass);
  force0.i = 0.0f;
  force0.j = 0.0f;
  force0.k = 0.0f;
  force1.i = 0.0f;
  force1.j = 0.0f;
  force1.k = 0.0f;
  torque0.i = 0.0f;
  torque0.j = 0.0f;
  torque0.k = 0.0f;
  torque1.i = 0.0f;
  torque1.j = 0.0f;
  torque1.k = 0.0f;

  for (mass_point0_index = 0;
       mass_point0_index < instance0->physics->mass_points.count;
       mass_point0_index++) {
    const struct mass_point_definition *mass_point0 =
      (const struct mass_point_definition *)tag_block_get_element(
        (void *)&instance0->physics->mass_points, mass_point0_index,
        sizeof(struct mass_point_definition));
    real_point3d point0;
    short mass_point1_index;

    matrix_transform_point((float *)&instance0->world_matrix,
                           (float *)&mass_point0->position, &point0.x);

    for (mass_point1_index = 0;
         mass_point1_index < instance1->physics->mass_points.count;
         mass_point1_index++) {
      const struct mass_point_definition *mass_point1 =
        (const struct mass_point_definition *)tag_block_get_element(
          (void *)&instance1->physics->mass_points, mass_point1_index,
          sizeof(struct mass_point_definition));
      real radius = mass_point0->radius + mass_point1->radius;
      real_point3d point1;
      real_vector3d direction;
      real distance;

      matrix_transform_point((float *)&instance1->world_matrix,
                             (float *)&mass_point1->position, &point1.x);
      vector_from_points3d_inline(&point0, &point1, &direction);
      distance = normalize3d_inline(&direction);

      if (distance < radius && distance > 0.0f) {
        real penetration = (radius - distance) * 0.5f;
        real force_magnitude = (*(float *)0x32512c / *(float *)0x325138) *
                               penetration * mass_scale * 2.0f;
        real_vector3d collision_force0;
        real_vector3d collision_force1;
        real_point3d collision_point;
        real_vector3d radius0;
        real_vector3d radius1;
        real_vector3d collision_torque0;
        real_vector3d collision_torque1;

        scale_vector3d_inline(&direction, -force_magnitude, &collision_force0);
        scale_vector3d_inline(&direction, force_magnitude, &collision_force1);
        collision_point.x =
          point0.x + direction.i * (mass_point0->radius - penetration);
        collision_point.y =
          point0.y + direction.j * (mass_point0->radius - penetration);
        collision_point.z =
          point0.z + direction.k * (mass_point0->radius - penetration);
        vector_from_points3d_inline((real_point3d *)(object0 + 0xc),
                                    &collision_point, &radius0);
        vector_from_points3d_inline((real_point3d *)(object1 + 0xc),
                                    &collision_point, &radius1);
        cross_product3d_inline(&radius0, &collision_force0, &collision_torque0);
        collision = true;
        cross_product3d_inline(&radius1, &collision_force1, &collision_torque1);
        add_vectors3d_inline(&force0, &collision_force0,
                             &force0); /* dup-args-ok: in-place */
        add_vectors3d_inline(&force1, &collision_force1,
                             &force1); /* dup-args-ok: in-place */
        add_vectors3d_inline(&torque0, &collision_torque0,
                             &torque0); /* dup-args-ok: in-place */
        add_vectors3d_inline(&torque1, &collision_torque1,
                             &torque1); /* dup-args-ok: in-place */
      }
    }
  }

  if (collision) {
    real_vector3d *collision_force0 = (real_vector3d *)(object0 + 0x460);
    real_vector3d *collision_torque0 = (real_vector3d *)(object0 + 0x46c);

    add_vectors3d_inline(collision_force0, &force0,
                         collision_force0); /* dup-args-ok: in-place */
    add_vectors3d_inline(collision_torque0, &torque0,
                         collision_torque0); /* dup-args-ok: in-place */
    SET_FLAG(((object_datum_t *)object0)->flags, _object_at_rest_bit, false);

    if (!(instance1->physics->radius > 0.0f)) {
      real_vector3d *collision_force1 = (real_vector3d *)(object1 + 0x460);
      real_vector3d *collision_torque1 = (real_vector3d *)(object1 + 0x46c);

      add_vectors3d_inline(collision_force1, &force1,
                           collision_force1); /* dup-args-ok: in-place */
      add_vectors3d_inline(collision_torque1, &torque1,
                           collision_torque1); /* dup-args-ok: in-place */
      SET_FLAG(((object_datum_t *)object1)->flags, _object_at_rest_bit, false);
    }
  }

  return collision;
}

/* 0x152350 — collide one unit against every object inside its bounding
 * radius.  `unit_index` arrives in EBX (never initialized inside the
 * function; sole caller FUN_00154270 at 0x1544be).
 *
 * Stack frame (0x2094 via _chkstk), derived from the EBP displacements at
 * 0x152350-0x1524c2:
 *   EBP-0x2094 (0x2000)  int found[0x800]      object_find_in_radius output
 *   EBP-0x94   (0x3c)    FUN_001509c0 context for this unit  (state_self)
 *   EBP-0x58   (0x3c)    FUN_001509c0 context for the other object
 *   EBP-0x1c   (0x10)    FUN_0014c8e0 collision-bsp context  (a_ctx)
 *   EBP-0xc / -0x8       loop counter / cursor
 *   EBP-0x1              byte result of FUN_0014c8e0 (AL only)
 * The 0x3c / 0x10 context sizes match the same two callees' contexts already
 * documented in collision_bsp.c (EBP-0x64 = 60 bytes, EBP-0x28 = 16 bytes).
 *
 * FUN_0014c8e0's result is consumed as a byte (MOV [EBP-0x1],AL) and reused
 * twice: as object_find_in_radius' type_mask (XOR EAX,EAX / SETNZ AL / ADD
 * EAX,2 -> 2 or 3) and as the "model_instance_valid" assert predicate.
 *
 * object_find_in_radius' pushes at 0x15238d-0x1523b0 (last arg pushed first):
 * 0x800, found, [EAX+0x5c] (radius, plain dword copy of the float),
 * EAX+0x50 (position), EAX+0x48 (cluster info), type_mask, 1.
 * The 9-dword ADD ESP,0x24 also folds in the two pushes of the preceding
 * object_get_and_verify_type call — not extra arguments.
 *
 * Per-object dispatch is on the unsigned byte at header+3 (MOVZX / SUB EAX,0
 * / DEC EAX): 0 = biped, 1 = unit, anything else skipped.
 *
 * The vehicle-side test at 0x152439-0x152449 is FLD [state_other[1]] /
 * FCOMP [0x2533c0] / TEST AH,0x41 / JNZ skip, i.e. fall through to
 * physics_compute_vehicle_collision only when *state_other[1] > 0x2533c0.  The
 * index compare uses both handles masked to their low 16 bits (AND
 * ESI/ECX,0xffff; CMP; JL).
 *
 * physics_compute_vehicle_collision and physics_compute_biped_collision are
 * both called with two stack arguments (LEA/PUSH pairs at 0x15244b-0x152456 and
 * 0x15249a-0x15249f, both cleaned by the shared ADD ESP,0x8 at 0x1524a4);
 * their kb.json `void(void)` declarations were placeholders.
 */
void physics_compute_unit_collisions(int unit_index)
{
  char has_model;
  short found_count;
  unsigned int remaining;
  unsigned int other_index;
  unsigned int object_type;
  int *cursor;
  void *object;
  void *other;
  void *header;
  void *biped;
  int a_ctx[4];
  int state_self[15];
  int state_other[15];
  int found[0x800];

  has_model = (char)FUN_0014c8e0(a_ctx, unit_index);

  if (FUN_001509c0(state_self, unit_index) == 0) {
    return;
  }

  object = object_get_and_verify_type(unit_index, 2);
  found_count = (short)object_find_in_radius(
    1, (unsigned int)((has_model != 0) + 2), (char *)object + 0x48,
    (float *)((char *)object + 0x50), *(float *)((char *)object + 0x5c), found,
    0x800);

  if (found_count <= 0) {
    return;
  }

  remaining = (unsigned int)(unsigned short)found_count;
  cursor = found;

  do {
    other_index = (unsigned int)*cursor;
    header = datum_get(*(data_t **)0x5a8d50, (int)other_index);
    object_type = *(unsigned char *)((char *)header + 3);

    switch (object_type) {
    case 0:
      biped = object_get_and_verify_type((int)other_index, 1);
      if (has_model == 0) {
        display_assert("model_instance_valid",
                       "c:\\halo\\SOURCE\\physics\\physics.c", 0x29d, 1);
        system_exit(-1);
      }
      if ((*(unsigned char *)((char *)biped + 0xb6) & 4) == 0) {
        physics_compute_biped_collision(a_ctx, (int)other_index);
      }
      break;
    case 1:
      if (other_index != (unsigned int)unit_index &&
          FUN_001509c0(state_other, (int)other_index) != 0) {
        other = object_get_and_verify_type((int)other_index, 2);
        if ((int)(other_index & 0xffff) <
              (int)((unsigned int)unit_index & 0xffff) ||
            (*(unsigned char *)((char *)other + 4) & 0x20) != 0 ||
            *(float *)state_other[1] > *(const float *)0x2533c0) {
          physics_compute_vehicle_collision(
            (const struct physics_instance *)state_self,
            (const struct physics_instance *)state_other);
        }
      }
      break;
    default:
      break;
    }

    cursor++;
    remaining--;
  } while (remaining != 0);
}

/* 0x1524d0 — rotate_vectors3d_by_angular_velocity (physics.c,
 * static; kb previously mislabelled it physics_compute_vehicle_collision).
 * Identity proven by its asserts: "forward!=rotated_forward" line 0x3b0,
 * "up!=rotated_up" line 0x3b1, and the vector3d_axes2 check on
 * rotated_forward/rotated_up at line 0x3c5.  Register ABI:
 * angular_velocity in EAX (MOV ECX,[EAX] @1524d6), forward in EBX,
 * rotated_forward in EDI, rotated_up in ESI (CMP EBX,EDI @1524f6); `up` is
 * the only stack argument ([EBP+0x8]).  sine/cosine are inline FSIN/FCOS. */
void rotate_vectors3d_by_angular_velocity(float *forward, float *up,
                                          float *angular_velocity,
                                          float *rotated_forward,
                                          float *rotated_up)
{
  real_vector3d axis;
  float magnitude;

  axis = *(real_vector3d *)angular_velocity;
  magnitude = normalize3d(&axis.i);

  assert_halt_msg_at("forward!=rotated_forward",
                     "c:\\halo\\SOURCE\\physics\\physics.c", 0x3b0,
                     forward != rotated_forward);
  assert_halt_msg_at("up!=rotated_up", "c:\\halo\\SOURCE\\physics\\physics.c",
                     0x3b1, up != rotated_up);

  if (magnitude != 0.0f) {
    real_matrix4x3 rotation;
    float dot;

    FUN_001092d0((float *)&rotation, &axis.i, x87_fsin(magnitude),
                 x87_fcos(magnitude));
    matrix_scale_transform_vector((float *)&rotation, forward, rotated_forward);
    matrix_scale_transform_vector((float *)&rotation, up, rotated_up);
    normalize3d(rotated_forward);

    dot = rotated_up[2] * rotated_forward[2];
    dot += rotated_up[0] * rotated_forward[0];
    dot = -(dot + rotated_up[1] * rotated_forward[1]);
    rotated_up[0] += dot * rotated_forward[0];
    rotated_up[1] += dot * rotated_forward[1];
    rotated_up[2] += dot * rotated_forward[2];
    normalize3d(rotated_up);
  } else {
    *(real_vector3d *)rotated_forward = *(real_vector3d *)forward;
    *(real_vector3d *)rotated_up = *(real_vector3d *)up;
  }

  if (!valid_real_normal3d_perpendicular(rotated_forward, rotated_up)) {
    display_assert(
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, "
               "%f, %f)",
               "rotated_forward", "rotated_up", (double)rotated_forward[0],
               (double)rotated_forward[1], (double)rotated_forward[2],
               (double)rotated_up[0], (double)rotated_up[1],
               (double)rotated_up[2]),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x3c5, 1);
    system_exit(-1);
  }
}

/* 0x152680 — physics_update_new: integrate the summed
 * force and torque from physics_compute_new into the object's velocities,
 * rotate its axes, then push the object out of structure with up to four
 * mass-point penetration sweeps before committing the new position, and
 * refresh the object's at-rest / ground / water flags from the per-point
 * flags.
 *
 * Evidence (0x152680-0x152e37):
 *  - asserts "instance->physics->mass>0.0f" 0x3da and the
 *    assert_valid_real_vector2d format (Bungie typo, sic) for
 *    &linear_acceleration 0x3de, &linear_velocity 0x3e2,
 *    &angular_acceleration 0x3f0 and &angular_velocity 0x3f4.
 *  - powered_mass_points ([EBP+0xc]) is never read.
 *  - world inverse inertia = R * I^-1 * R^T: FUN_0010a2c0 builds R from the
 *    object's forward/up, element 1 of the 0x24-byte block at physics+0x5c
 *    is I^-1, and FUN_001099f0's EAX (the transposed matrix, MOV EAX,[EBP+0xc])
 *    is pushed straight into the second FUN_00109c70 at 0x15284b.
 *  - 0x4761fa is the debug global debug_physics_disable_penetration_freeze
 *    (hs-global table entry at 0x2f2bf8: name 0x27edc4, type 5, address).
 *  - penetration sweep: FUN_0014df70 flags 0xc0a1 with the object itself as
 *    the ignore handle; the earliest hit (smallest t, FCOMP skip when
 *    best.t <= t) is kept by REP MOVSD 0x14.  The escape fraction is
 *    best.t - (dot != 0 ? 0.0078125 / |dot| : 0.03125) (double constants
 *    0x29d870 / 0x29d588), floored at 0, and never stored (ST0-resident from
 *    0x152b7c to 0x152c5c).
 *  - when all four sweeps hit, the loop falls out at 0x152c82 WITHOUT calling
 *    object_set_position; only the no-hit exit (0x152c88) and the debug path
 *    commit the position.  The hit mask goes to vehicle+0x478 on both
 *    non-debug exits.
 *  - object flag 0x4 and 0x8 are BOTH set from the in-water count (TEST DX,DX
 *    at 0x152dec and 0x152dfc); the volatile-surface count only gates the
 *    at-rest bit (CMP word [EBP-0x1c],0 at 0x152d35).
 *  - at-rest thresholds (float, compared <=): |v|^2 0x25620c = (1/30)^2,
 *    |w|^2 0x29d868 = (3 deg)^2, |a|^2 0x29d864 = (1/1800)^2,
 *    |alpha|^2 0x29d860 = (0.1 deg)^2. */
void physics_update_new(
  const struct physics_instance *instance,
  const struct powered_mass_point_datum *powered_mass_points,
  struct mass_point_datum *mass_points, const real_vector3d *total_force,
  const real_vector3d *total_torque)
{
  object_datum_t *object =
    (object_datum_t *)object_get_and_verify_type(instance->object_index, 2);
  real_vector3d linear_acceleration;
  real_vector3d linear_velocity;
  real_point3d position;
  real_vector3d angular_acceleration;
  real_vector3d angular_velocity;
  real_vector3d forward;
  real_vector3d up;
  short mass_point_index;

  (void)powered_mass_points;

  assert_halt_msg_at("instance->physics->mass>0.0f",
                     "c:\\halo\\SOURCE\\physics\\physics.c", 0x3da,
                     instance->physics->mass > 0.0f);

  scale_vector3d_inline(total_force, 1.0f / instance->physics->mass,
                        &linear_acceleration);
  if (!(boolean)real_vector3d_valid(&linear_acceleration.i)) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&linear_acceleration", (double)linear_acceleration.i,
               (double)linear_acceleration.j, (double)linear_acceleration.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x3de, 1);
    system_exit(-1);
  }

  add_vectors3d_inline(&linear_acceleration, &object->translational_velocity,
                       &linear_velocity);
  if (!(boolean)real_vector3d_valid(&linear_velocity.i)) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&linear_velocity", (double)linear_velocity.i,
               (double)linear_velocity.j, (double)linear_velocity.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x3e2, 1);
    system_exit(-1);
  }

  position.x = object->position.x + linear_velocity.i;
  position.y = object->position.y + linear_velocity.j;
  position.z = object->position.z + linear_velocity.k;

  {
    float frame[9];
    float world_inverse_inertia[9];

    FUN_0010a2c0(frame, &object->forward.i, &object->up.i);
    FUN_00109c70(frame,
                 (float *)tag_block_get_element(
                   (void *)&instance->physics->field_5c, 1, sizeof(frame)),
                 world_inverse_inertia);
    FUN_00109c70(world_inverse_inertia, FUN_001099f0(frame, frame),
                 world_inverse_inertia);
    FUN_00109d90(world_inverse_inertia, (float *)&total_torque->i,
                 &angular_acceleration.i);
  }

  if (!(boolean)real_vector3d_valid(&angular_acceleration.i)) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&angular_acceleration", (double)angular_acceleration.i,
               (double)angular_acceleration.j, (double)angular_acceleration.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x3f0, 1);
    system_exit(-1);
  }

  add_vectors3d_inline(&angular_acceleration, &object->angular_velocity,
                       &angular_velocity);
  if (!(boolean)real_vector3d_valid(&angular_velocity.i)) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&angular_velocity", (double)angular_velocity.i,
               (double)angular_velocity.j, (double)angular_velocity.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x3f4, 1);
    system_exit(-1);
  }

  rotate_vectors3d_by_angular_velocity(&object->forward.i, &object->up.i,
                                       &angular_velocity.i, &forward.i, &up.i);

  object->translational_velocity = linear_velocity;
  object->angular_velocity = angular_velocity;

  if (*(boolean *)0x4761fa /* debug_physics_disable_penetration_freeze */) {
    object_set_position(instance->object_index, &position.x, &forward.i, &up.i);
  } else {
    short iterations = 4;
    uint32_t penetration_mask;

    while (iterations-- > 0) {
      boolean found = false;
      real_vector3d best_sweep;
      struct collision_result best_collision;
      real_matrix4x3 world_matrix;
      real_point3d center_of_mass;

      penetration_mask = 0;
      matrix4x3_from_forward_up_position(&world_matrix, &position.x, &forward.i,
                                         &up.i);
      set_real_point3d_inline((vector3_t *)&center_of_mass,
                              -instance->physics->center_of_mass.x,
                              -instance->physics->center_of_mass.y,
                              -instance->physics->center_of_mass.z);
      matrix_transform_point((float *)&world_matrix, &center_of_mass.x,
                             &center_of_mass.x); /* dup-args-ok: in-place */
      world_matrix.position = *(vector3_t *)&center_of_mass;

      for (mass_point_index = 0;
           mass_point_index < instance->physics->mass_points.count;
           mass_point_index++) {
        const struct mass_point_definition *mass_point_definition =
          (const struct mass_point_definition *)tag_block_get_element(
            (void *)&instance->physics->mass_points, mass_point_index,
            sizeof(struct mass_point_definition));
        struct mass_point_datum *mass_point = mass_points + mass_point_index;
        real_point3d target;
        real_vector3d sweep;
        struct collision_result collision;

        matrix_transform_point((float *)&world_matrix,
                               (float *)&mass_point_definition->position,
                               &target.x);
        vector_from_points3d_inline(&mass_point->position, &target, &sweep);

        if (FUN_0014df70(0xc0a1, &mass_point->position.x, &sweep.i,
                         instance->object_index, (int16_t *)&collision)) {
          penetration_mask |= 1u << mass_point_index;

          /* keep the earliest hit (REP MOVSD 0x14 copy of the result) */
          if (!found || best_collision.t > collision.t) {
            found = true;
            best_sweep = sweep;
            best_collision = collision;
          }
        }
      }

      if (!found) {
        object_set_position(instance->object_index, &position.x, &forward.i,
                            &up.i);
        break;
      } else {
        const real_vector3d *normal =
          (const real_vector3d *)best_collision.plane.normal;
        real approach;
        real backoff;
        real fraction;
        real normal_velocity;

        /* (k + j) + i dot of the sweep against the hit normal @152b3e. */
        approach = best_sweep.k * normal->k;
        approach += best_sweep.j * normal->j;
        approach += best_sweep.i * normal->i;
        /* pull-back distance: 1/128 world unit along the sweep, or a flat
         * 1/32 when the sweep grazes the plane (doubles 0x29d870/0x29d588). */
        backoff =
          (real)(approach != 0.0f ? 0.0078125 / fabs(approach) : 0.03125);
        fraction = best_collision.t - backoff > 0.0f
                     ? best_collision.t - backoff
                     : 0.0f;
        normal_velocity = normal->j * linear_velocity.j;
        normal_velocity += normal->k * linear_velocity.k;
        normal_velocity += normal->i * linear_velocity.i;

        if (normal_velocity < 0.0f) {
          /* cancel the into-surface velocity beyond the escape fraction
           * (FMUL/FADD per component at 0x152b97-0x152bce). */
          point_from_line3d_inline(&linear_velocity, normal,
                                   (fraction - 1.0f) * normal_velocity,
                                   &linear_velocity);
          object->translational_velocity = linear_velocity;
          position.x = object->position.x + linear_velocity.i;
          position.y = object->position.y + linear_velocity.j;
          position.z = object->position.z + linear_velocity.k;
        }

        scale_vector3d_inline(&angular_velocity, fraction, &angular_velocity);
        object->angular_velocity = angular_velocity;
        rotate_vectors3d_by_angular_velocity(&object->forward.i, &object->up.i,
                                             &angular_velocity.i, &forward.i,
                                             &up.i);
      }
    }

    *(uint32_t *)((char *)object + 0x478) = penetration_mask;
  }

  {
    short at_rest_count = 0;
    short on_ground_count = 0;
    short on_volatile_surface_count = 0;
    short in_water_count = 0;

    for (mass_point_index = 0;
         mass_point_index < instance->physics->mass_points.count;
         mass_point_index++) {
      const struct mass_point_datum *mass_point =
        mass_points + mass_point_index;

      at_rest_count += TEST_FLAG(mass_point->flags, _point_at_rest_bit);
      on_ground_count += TEST_FLAG(mass_point->flags, _point_on_ground_bit);
      on_volatile_surface_count +=
        TEST_FLAG(mass_point->flags, _point_on_volatile_surface_bit);
      in_water_count += TEST_FLAG(mass_point->flags, _point_in_water_bit);
    }

    SET_FLAG(object->flags, _object_at_rest_bit,
             at_rest_count == instance->physics->mass_points.count &&
               on_ground_count >= 3 && on_volatile_surface_count == 0 &&
               dot_product3d_inline(&linear_velocity, &linear_velocity) <=
                 0.0011111111f &&
               dot_product3d_inline(&angular_velocity, &angular_velocity) <=
                 0.0027415568f &&
               dot_product3d_inline(&linear_acceleration, &linear_acceleration) <=
                 3.0864197e-07f &&
               dot_product3d_inline(&angular_acceleration, &angular_acceleration) <=
                 3.0461742e-06f);
    /* bits 1-4: 0x2 any point on ground, 0x4 and 0x8 any point in water (the
     * binary tests the in-water count for both), 0x10 every point in water. */
    SET_FLAG(object->flags, 1, on_ground_count > 0);
    SET_FLAG(object->flags, 2, in_water_count > 0);
    SET_FLAG(object->flags, 3, in_water_count > 0);
    SET_FLAG(object->flags, 4,
             in_water_count == instance->physics->mass_points.count);
  }
}

/* 0x152e40 — physics_update_old (physics.c): the pre-"instance"
 * rigid-body integrator used by physics definitions with a positive radius.
 * Accumulates the per-mass-point forces (ground/water/air friction, powered
 * lift/thrust/antigrav, gravity, magic force/torque) the same way as
 * physics_compute_new, then integrates them directly into the object:
 * linear acceleration = force / mass, angular acceleration = torque / the
 * scalar moment about the torque axis, translate, rotate the axes by the
 * angular velocity and re-orthonormalize them, and refresh the at-rest /
 * ground / water flags.
 *
 * Differences from physics_compute_new + physics_update_new
 * (0x152e40-0x15426a):
 *  - no material scaling of the ground parameters; the ground normal-force
 *    term is global_gravity / physics.ground_depth * ground_depth (FLD
 *    [0x32512c]; FDIV [+0x24]; FMUL [EDI+0x74] at 0x1532a2) and the normal
 *    velocity is never stored.
 *  - ground friction coefficients are scaled by 0.125f (0x268ed0) when the
 *    GROUND MATERIAL (mass_point+0x70, CMP word [EDI+0x70],0x1f at 0x153441)
 *    is 0x1f.
 *  - mass-point positions are relative to physics.center_of_mass
 *    (0x15311a-0x15314a).
 *  - antigrav does not set the mass point antigraving flag; its magnitude
 *    multiplies alignment before mass (FMULP ST2 at 0x153936).
 *  - point counts are taken inside the force loop, before thrust/antigrav
 *    (0x1537b3-0x1537f0).
 *  - moment = sum((|perp|^2 + radius^2 * 0.4f) * mass * physics+0x04) over
 *    the points, perp = the point radius with its torque-axis component
 *    removed (0x153c33-0x153ccb).  The torque axis magnitude below the double
 *    epsilon 0x2533d0 (1e-4) skips the moment entirely.
 *  - object translation goes through FUN_0018f230 (new location from the old
 *    location at object+0x48 along old->new position) and object_translate.
 *  - assert lines: magic_force 0x4e7, magic_torque 0x4ed,
 *    &translational_acceleration 0x603, &angular_acceleration 0x607,
 *    object forward/up axes 0x63b; the vector-valid and axes checks are
 *    inlined (AND 0x7f800000 / FABS vs double 0.001 at 0x2549d8). */
void physics_update_old(int object_index,
                        struct powered_mass_point_datum *powered_mass_points,
                        struct mass_point_datum *mass_points,
                        const real_vector3d *magic_force,
                        const real_vector3d *magic_torque)
{
  object_datum_t *object;
  char *object_definition;
  const struct physics_definition *physics;
  real_matrix4x3 world_matrix;
  real_vector3d total_force;
  real_vector3d total_torque;
  real_vector3d linear_acceleration;
  real_vector3d angular_acceleration;
  real gravity;
  short mass_point_index;
  short at_rest_count;
  short on_ground_count;
  short on_volatile_surface_count;
  short in_water_count;

  object = (object_datum_t *)object_get_and_verify_type(object_index, -1);
  object_definition =
    (char *)tag_get(0x6f626a65 /* 'obje' */, object->definition_index);
  physics = (const struct physics_definition *)tag_get(
    0x70687973 /* 'phys' */, *(int *)(object_definition + 0x8c));
  gravity = *(float *)0x32512c * physics->gravity_scale;

  at_rest_count = 0;
  on_ground_count = 0;
  on_volatile_surface_count = 0;
  in_water_count = 0;
  matrix4x3_from_forward_up_position(&world_matrix, &object->position.x,
                                     &object->forward.i, &object->up.i);

  set_real_vector3d_inline(&total_force, 0.0f, 0.0f,
                           -(gravity * physics->mass));
  set_real_vector3d_inline(&total_torque, 0.0f, 0.0f, 0.0f);
  set_real_vector3d_inline(&linear_acceleration, 0.0f, 0.0f, 0.0f);
  set_real_vector3d_inline(&angular_acceleration, 0.0f, 0.0f, 0.0f);

  if (powered_mass_points) {
    short powered_mass_point_index;

    for (powered_mass_point_index = 0;
         powered_mass_point_index < physics->powered_mass_points.count;
         powered_mass_point_index++) {
      struct powered_mass_point_datum *powered_mass_point =
        powered_mass_points + powered_mass_point_index;

      FUN_001093b0((float *)&powered_mass_point->rotation_matrix,
                   powered_mass_point->rotation);
      FUN_00109120((float *)&powered_mass_point->rotation_matrix);
    }
  }

  csmemset(mass_points, 0,
           physics->mass_points.count * sizeof(struct mass_point_datum));

  if (magic_force) {
    if (!valid_real_vector3d_inline(magic_force)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "magic_force", (double)magic_force->i,
                              (double)magic_force->j, (double)magic_force->k),
                     "c:\\halo\\SOURCE\\physics\\physics.c", 0x4e7, 1);
      system_exit(-1);
    }
    total_force.i = magic_force->i;
    total_force.j = magic_force->j;
    total_force.k += magic_force->k;
  }
  if (magic_torque) {
    if (!valid_real_vector3d_inline(magic_torque)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "magic_torque", (double)magic_torque->i,
                              (double)magic_torque->j, (double)magic_torque->k),
                     "c:\\halo\\SOURCE\\physics\\physics.c", 0x4ed, 1);
      system_exit(-1);
    }
    total_torque = *magic_torque;
  }

  for (mass_point_index = 0; mass_point_index < physics->mass_points.count;
       mass_point_index++) {
    const struct mass_point_definition *mass_point_definition;
    const struct powered_mass_point_definition *powered_mass_point_definition;
    const struct powered_mass_point_datum *powered_mass_point;
    struct mass_point_datum *mass_point;
    const real_vector3d *ground_normal;
    real_point3d local_position;

    mass_point_definition =
      (const struct mass_point_definition *)tag_block_get_element(
        (void *)&physics->mass_points, mass_point_index,
        sizeof(struct mass_point_definition));
    mass_point = mass_points + mass_point_index;
    powered_mass_point_definition =
      mass_point_definition->powered_mass_point_index != -1 &&
          powered_mass_points ?
        (const struct powered_mass_point_definition *)tag_block_get_element(
          (void *)&physics->powered_mass_points,
          mass_point_definition->powered_mass_point_index,
          sizeof(struct powered_mass_point_definition)) :
        NULL;
    powered_mass_point =
      powered_mass_point_definition ?
        powered_mass_points + mass_point_definition->powered_mass_point_index :
        NULL;

    mass_point->flags = 0;
    vector_from_points3d_inline(&physics->center_of_mass,
                                &mass_point_definition->position,
                                (real_vector3d *)&local_position);
    matrix_transform_point((float *)&world_matrix, &local_position.x,
                           &mass_point->position.x);
    if (powered_mass_point) {
      real_matrix4x3 powered_matrix;

      matrix4x3_multiply((float *)&world_matrix,
                         (float *)&powered_mass_point->rotation_matrix,
                         (float *)&powered_matrix);
      matrix_transform_vector((float *)&powered_matrix,
                              (float *)&mass_point_definition->forward,
                              &mass_point->forward.i);
      matrix_transform_vector((float *)&powered_matrix,
                              (float *)&mass_point_definition->up,
                              &mass_point->up.i);
    } else {
      matrix_transform_vector((float *)&world_matrix,
                              (float *)&mass_point_definition->forward,
                              &mass_point->forward.i);
      matrix_transform_vector((float *)&world_matrix,
                              (float *)&mass_point_definition->up,
                              &mass_point->up.i);
    }
    scenario_location_from_point(mass_point->location, &mass_point->position);

    vector_from_points3d_inline(&object->position, &mass_point->position,
                                &mass_point->radius);
    cross_product3d_inline(&object->angular_velocity, &mass_point->radius,
                           &mass_point->velocity);
    add_vectors3d_inline(&mass_point->velocity, &object->translational_velocity,
                         &mass_point->velocity); /* dup-args-ok: in-place */

    compute_ground_plane(object_index, (void *)mass_point_definition,
                         mass_point);
    mass_point->water_depth =
      FUN_0018f510(mass_point->location, &mass_point->position);

    ground_normal = (const real_vector3d *)mass_point->ground_plane.normal;
    if (mass_point->ground_depth > 0.0f && physics->ground_depth > 0.0f) {
      x87_wide_t normal_velocity; /* ST0-resident @153284..15330b */
      x87_wide_t normal_force_magnitude; /* FST [EDI+0x80] @1532b8 */
      real ground_scale;

      normal_velocity =
        dot_product3d_kji_inline(&mass_point->velocity, ground_normal);
      normal_force_magnitude =
        (*(float *)0x32512c / physics->ground_depth * mass_point->ground_depth -
         normal_velocity * physics->ground_damp_fraction) *
        physics->mass;
      mass_point->normal_force_magnitude = (real)normal_force_magnitude;
      mass_point->normal_force.i = normal_force_magnitude * ground_normal->i;
      mass_point->normal_force.j = normal_force_magnitude * ground_normal->j;
      mass_point->normal_force.k = normal_force_magnitude * ground_normal->k;
      ground_scale = -(mass_point_definition->mass * physics->ground_friction);
      HALO_FLT_ROUNDTRIP(ground_scale); /* FSTP [EBP-0x4] @1532ea */
      mass_point->velocity_relative_to_ground.i =
        -normal_velocity * ground_normal->i + mass_point->velocity.i;
      mass_point->velocity_relative_to_ground.j =
        -normal_velocity * ground_normal->j + mass_point->velocity.j;
      mass_point->velocity_relative_to_ground.k =
        -normal_velocity * ground_normal->k + mass_point->velocity.k;
      scale_vector3d_inline(&mass_point->velocity_relative_to_ground,
                            ground_scale,
                            &mass_point->ground_friction.friction);

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_ground_friction_bit) &&
          powered_mass_point->ground_friction_velocity != 0.0f) {
        real fraction;
        real alignment;
        real weight;
        real_vector3d powered_velocity;
        real_vector3d projected_velocity;

        fraction = pin_fraction(ground_normal->k, physics->ground_normal_k0,
                                physics->ground_normal_k1);
        alignment = dot_product3d_kji_inline(&mass_point->up, ground_normal);
        alignment =
          alignment < 0.0f ? 0.0f : (alignment > 1.0f ? 1.0f : alignment);
        weight = alignment * alignment * fraction * fraction * ground_scale;

        scale_vector3d_inline(&mass_point->forward,
                              -powered_mass_point->ground_friction_velocity,
                              &powered_velocity);
        point_from_line3d_inline(
          &powered_velocity, ground_normal,
          -dot_product3d_kji_inline(&powered_velocity, ground_normal),
          &projected_velocity);
        add_vectors3d_inline(
          &mass_point->velocity_relative_to_ground, &projected_velocity,
          &mass_point->velocity_relative_to_ground); /* dup-args-ok */
        point_from_line3d_inline(
          &mass_point->ground_friction.friction, &projected_velocity, weight,
          &mass_point->ground_friction.friction); /* dup-args-ok */
      }

      if (mass_point->ground_material_type == 0x1f) {
        friction_evaluate(mass_point_definition->friction_type,
                          mass_point_definition->friction_parallel_scale *
                            *(float *)0x268ed0,
                          mass_point_definition->friction_perpendicular_scale *
                            *(float *)0x268ed0,
                          &mass_point->forward.i, &mass_point->up.i,
                          &mass_point->ground_friction);
      } else {
        friction_evaluate(mass_point_definition->friction_type,
                          mass_point_definition->friction_parallel_scale,
                          mass_point_definition->friction_perpendicular_scale,
                          &mass_point->forward.i, &mass_point->up.i,
                          &mass_point->ground_friction);
      }
    }

    if (mass_point->water_depth > 0.0f) {
      real depth_fraction;

      depth_fraction = mass_point->water_depth < physics->water_depth ?
                         mass_point->water_depth / physics->water_depth :
                         1.0f;
      HALO_FLT_ROUNDTRIP(depth_fraction); /* FSTP [EBP-0x40] @1534aa */
      if (mass_point_definition->density > 0.0f &&
          physics->water_depth > 0.0f) {
        mass_point->water_pressure_magnitude =
          physics->water_density / mass_point_definition->density *
          mass_point_definition->mass * depth_fraction * gravity;
        set_real_vector3d_inline(&mass_point->water_pressure, 0.0f, 0.0f,
                                 mass_point->water_pressure_magnitude);
      }

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_water_friction_bit) &&
          powered_mass_point->water_friction_velocity != 0.0f) {
        real_vector3d powered_velocity;

        point_from_line3d_inline(&mass_point->velocity, &mass_point->forward,
                                 -powered_mass_point->water_friction_velocity,
                                 &powered_velocity);
        scale_vector3d_inline(
          &powered_velocity,
          -(mass_point_definition->mass * physics->water_friction),
          &mass_point->water_friction.friction);
      } else {
        scale_vector3d_inline(
          &mass_point->velocity,
          -(mass_point_definition->mass * physics->water_friction),
          &mass_point->water_friction.friction);
      }

      friction_evaluate(mass_point_definition->friction_type,
                        mass_point_definition->friction_parallel_scale,
                        mass_point_definition->friction_perpendicular_scale,
                        &mass_point->forward.i, &mass_point->up.i,
                        &mass_point->water_friction);

      if (powered_mass_point_definition &&
          TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_water_lift_bit) &&
          powered_mass_point->water_lift_ratio != 0.0f) {
        real lift = x87_fabs(dot_product3d_kji_inline(&mass_point->velocity,
                                                      &mass_point->forward)) *
                    depth_fraction * powered_mass_point->water_lift_ratio *
                    physics->mass;

        point_from_line3d_inline(&mass_point->powered_force, &mass_point->up,
                                 lift,
                                 &mass_point->powered_force); /* dup-args-ok */
      }
    }

    if (powered_mass_point_definition &&
        TEST_FLAG(powered_mass_point_definition->flags,
                  _powered_mass_point_air_friction_bit) &&
        powered_mass_point->air_friction_velocity != 0.0f) {
      real_vector3d powered_velocity;

      point_from_line3d_inline(&mass_point->velocity, &mass_point->forward,
                               -powered_mass_point->air_friction_velocity,
                               &powered_velocity);
      scale_vector3d_inline(
        &powered_velocity,
        -(mass_point_definition->mass * physics->air_friction),
        &mass_point->air_friction.friction);
    } else {
      scale_vector3d_inline(
        &mass_point->velocity,
        -(mass_point_definition->mass * physics->air_friction),
        &mass_point->air_friction.friction);
    }

    friction_evaluate(mass_point_definition->friction_type,
                      mass_point_definition->friction_parallel_scale,
                      mass_point_definition->friction_perpendicular_scale,
                      &mass_point->forward.i, &mass_point->up.i,
                      &mass_point->air_friction);

    if (powered_mass_point_definition &&
        TEST_FLAG(powered_mass_point_definition->flags,
                  _powered_mass_point_air_lift_bit) &&
        powered_mass_point->air_lift_ratio != 0.0f) {
      real lift = x87_fabs(dot_product3d_kji_inline(&mass_point->velocity,
                                                    &mass_point->forward)) *
                  powered_mass_point->air_lift_ratio * physics->mass;

      point_from_line3d_inline(&mass_point->powered_force, &mass_point->up,
                               lift,
                               &mass_point->powered_force); /* dup-args-ok */
    }

    SET_FLAG(mass_point->flags, _point_at_rest_bit,
             dot_product3d_kji_inline(&mass_point->velocity,
                                      &mass_point->velocity) < 0.0011111111f);
    SET_FLAG(mass_point->flags, _point_on_ground_bit,
             mass_point->ground_depth > 0.0f);
    SET_FLAG(mass_point->flags, _point_in_water_bit,
             mass_point->water_depth > 0.0f);

    at_rest_count += TEST_FLAG(mass_point->flags, _point_at_rest_bit);
    on_ground_count += TEST_FLAG(mass_point->flags, _point_on_ground_bit);
    on_volatile_surface_count +=
      TEST_FLAG(mass_point->flags, _point_on_volatile_surface_bit);
    in_water_count += TEST_FLAG(mass_point->flags, _point_in_water_bit);

    if (powered_mass_point_definition) {
      if (TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_thrust_bit)) {
        point_from_line3d_inline(
          &mass_point->powered_force, &mass_point->forward,
          powered_mass_point->thrust_fraction * physics->mass,
          &mass_point->powered_force); /* dup-args-ok */
      }

      if (TEST_FLAG(powered_mass_point_definition->flags,
                    _powered_mass_point_antigrav_bit)) {
        real_point3d probe_point;
        real_vector3d probe_vector;
        struct collision_result collision;

        probe_point = mass_point->position;
        scale_vector3d_inline(*(const real_vector3d **)0x31fc50,
                              powered_mass_point_definition->antigrav_height +
                                mass_point_definition->radius,
                              &probe_vector);

        if (FUN_0014df70(0xc0a0, &probe_point.x, &probe_vector.i, object_index,
                         (int16_t *)&collision)) {
          real height;
          real alignment;
          real ground_effect;
          real magnitude;

          height = (powered_mass_point_definition->antigrav_height +
                    mass_point_definition->radius) *
                     collision.t -
                   mass_point_definition->radius;
          HALO_FLT_ROUNDTRIP(height); /* FSTP [EBP+0x18] @1538cd */
          alignment = pin_fraction(
            mass_point->up.k, powered_mass_point_definition->antigrav_normal_k0,
            powered_mass_point_definition->antigrav_normal_k1);
          ground_effect =
            height > 0.0f ?
              1.0f - height / powered_mass_point_definition->antigrav_height :
              1.0f;
          magnitude =
            (ground_effect * ground_effect * *(float *)0x32512c -
             (collision.plane.normal[1] * mass_point->velocity.j +
              collision.plane.normal[2] * mass_point->velocity.k +
              collision.plane.normal[0] * mass_point->velocity.i) *
               powered_mass_point_definition->antigrav_damp_fraction) *
            powered_mass_point->antigrav_fraction *
            powered_mass_point_definition->antigrav_strength * alignment *
            physics->mass;

          point_from_line3d_inline(
            &mass_point->powered_force,
            (const real_vector3d *)collision.plane.normal, magnitude,
            &mass_point->powered_force); /* dup-args-ok */
        }
      }
    }

    add_vectors3d_inline(&mass_point->force, &mass_point->normal_force,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force,
                         &mass_point->ground_friction.friction,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force, &mass_point->water_pressure,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force,
                         &mass_point->water_friction.friction,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force, &mass_point->air_friction.friction,
                         &mass_point->force); /* dup-args-ok */
    add_vectors3d_inline(&mass_point->force, &mass_point->powered_force,
                         &mass_point->force); /* dup-args-ok */
    cross_product3d_inline(&mass_point->radius, &mass_point->force,
                           &mass_point->torque);

    add_vectors3d_inline(&total_force, &mass_point->force,
                         &total_force); /* dup-args-ok */
    add_vectors3d_inline(&total_torque, &mass_point->torque,
                         &total_torque); /* dup-args-ok */
  }

  if (physics->mass != 0.0f) {
    real inverse_mass = 1.0f / physics->mass;

    linear_acceleration.i = total_force.i * inverse_mass;
    linear_acceleration.j = total_force.j * inverse_mass;
    linear_acceleration.k = total_force.k * inverse_mass;
  }

  {
    real_vector3d axis;
    x87_wide_t magnitude; /* ST0-resident @153bcd..153c06 */

    axis = total_torque;
    magnitude = x87_sqrtd(total_torque.k * total_torque.k +
                          total_torque.j * total_torque.j +
                          total_torque.i * total_torque.i);
    if (!(fabs(magnitude) < *(const double *)0x2533d0)) {
      x87_wide_t inverse = 1.0f / magnitude;

      axis.i = axis.i * inverse;
      axis.j = axis.j * inverse;
      axis.k = axis.k * inverse;
      if (magnitude != 0.0f) {
        real moment;

        moment = 0.0f;
        for (mass_point_index = 0;
             mass_point_index < physics->mass_points.count;
             mass_point_index++) {
          const struct mass_point_definition *mass_point_definition;
          const struct mass_point_datum *mass_point;
          x87_wide_t projection; /* ST0-resident @153c6b */
          x87_wide_t perpendicular_i;
          x87_wide_t perpendicular_j;
          x87_wide_t perpendicular_k;

          mass_point_definition =
            (const struct mass_point_definition *)tag_block_get_element(
              (void *)&physics->mass_points, mass_point_index,
              sizeof(struct mass_point_definition));
          mass_point = mass_points + mass_point_index;
          projection =
            -(axis.k * mass_point->radius.k + axis.j * mass_point->radius.j +
              axis.i * mass_point->radius.i);
          perpendicular_i = axis.i * projection + mass_point->radius.i;
          perpendicular_j = axis.j * projection + mass_point->radius.j;
          perpendicular_k = axis.k * projection + mass_point->radius.k;
          moment += (perpendicular_k * perpendicular_k +
                     perpendicular_j * perpendicular_j +
                     perpendicular_i * perpendicular_i +
                     mass_point_definition->radius *
                       mass_point_definition->radius * 0.4f) *
                    mass_point_definition->mass * physics->field_04;
        }

        if (moment != 0.0f) {
          real inverse_moment = 1.0f / moment;

          angular_acceleration.i = total_torque.i * inverse_moment;
          angular_acceleration.j = total_torque.j * inverse_moment;
          angular_acceleration.k = total_torque.k * inverse_moment;
        }
      }
    }
  }

  if (!valid_real_vector3d_inline(&linear_acceleration)) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&translational_acceleration", (double)linear_acceleration.i,
               (double)linear_acceleration.j, (double)linear_acceleration.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x603, 1);
    system_exit(-1);
  }
  add_vectors3d_inline(&linear_acceleration, &object->translational_velocity,
                       &object->translational_velocity); /* dup-args-ok */

  if (!valid_real_vector3d_inline(&angular_acceleration)) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real_vector2d(%f, %f, %f)",
               "&angular_acceleration", (double)angular_acceleration.i,
               (double)angular_acceleration.j, (double)angular_acceleration.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x607, 1);
    system_exit(-1);
  }
  add_vectors3d_inline(&angular_acceleration, &object->angular_velocity,
                       &object->angular_velocity); /* dup-args-ok */

  {
    real_point3d new_position;
    byte new_location[8]; /* same 8-byte scenario location as +0x48 */

    add_vectors3d_inline((const real_vector3d *)&object->position,
                         &object->translational_velocity,
                         (real_vector3d *)&new_position);
    FUN_0018f230(new_location, (char *)object + 0x48 /* location */,
                 &object->position, &new_position);
    object_translate(object_index, &new_position.x, new_location);
  }

  {
    real_vector3d axis;
    x87_wide_t magnitude; /* FSQRT @153eb9, FABS on the unstored ST0 */
    real angle;

    axis = object->angular_velocity;
    magnitude = x87_sqrtd(axis.i * axis.i + axis.k * axis.k + axis.j * axis.j);
    angle = (real)magnitude; /* FST [EBP+0xc] @153ebb */
    if (!(fabs(magnitude) < *(const double *)0x2533d0)) {
      x87_wide_t inverse = 1.0f / angle; /* FDIV [EBP+0xc] @153ed7 */

      axis.i = axis.i * inverse;
      axis.j = axis.j * inverse;
      axis.k = inverse * axis.k;
      if (angle != 0.0f) {
        real sine = x87_fsin(angle);
        real cosine = x87_fcos(angle);
        x87_wide_t dot; /* ST0-resident @153fa1..153fb6 */

        rotate_vector3d_by_sincos(&object->forward.i, &axis.i, sine, cosine);
        rotate_vector3d_by_sincos(&object->up.i, &axis.i, sine, cosine);
        normalize3d_squared_inline(&object->forward);
        dot = -(object->forward.k * object->up.k +
                object->forward.j * object->up.j +
                object->up.i * object->forward.i);
        object->up.i = dot * object->forward.i + object->up.i;
        object->up.j = dot * object->forward.j + object->up.j;
        object->up.k = dot * object->forward.k + object->up.k;
        normalize3d_squared_inline(&object->up);
      }
    }
  }

  SET_FLAG(
    object->flags, _object_at_rest_bit,
    at_rest_count == physics->mass_points.count && on_ground_count >= 3 &&
      on_volatile_surface_count == 0 &&
      magnitude_squared3d_inline(&object->translational_velocity) <=
        0.0011111111f &&
      magnitude_squared3d_inline(&object->angular_velocity) <= 0.0027415568f &&
      dot_product3d_inline(&linear_acceleration, &linear_acceleration) <=
        3.0864197e-07f &&
      dot_product3d_inline(&angular_acceleration, &angular_acceleration) <=
        3.0461742e-06f);
  /* bits 1-4 as in physics_update_new: 0x4 and 0x8 both from the in-water
   * count (TEST AX,AX at 0x1540f7 and 0x154107). */
  SET_FLAG(object->flags, 1, on_ground_count > 0);
  SET_FLAG(object->flags, 2, in_water_count > 0);
  SET_FLAG(object->flags, 3, in_water_count > 0);
  SET_FLAG(object->flags, 4, in_water_count == physics->mass_points.count);

  if (!valid_real_axes2_inline(&object->forward, &object->up)) {
    display_assert(
      csprintf((char *)0x5ab100,
               "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, "
               "%f, %f)",
               "&object->object.forward", "&object->object.up",
               (double)object->forward.i, (double)object->forward.j,
               (double)object->forward.k, (double)object->up.i,
               (double)object->up.j, (double)object->up.k),
      "c:\\halo\\SOURCE\\physics\\physics.c", 0x63b, 1);
    system_exit(-1);
  }
}

/* 0x154270 — physics_update (physics.c): per-tick rigid-body
 * physics for an object.  Definitions with a positive radius take the old
 * integrator (physics_update_old); otherwise build a physics instance,
 * refresh each powered mass point's rotation matrix, accumulate forces
 * (physics_compute_new + the vehicle's pending collision force/torque at
 * +0x460/+0x46c + the optional magic force/torque), integrate
 * (physics_update_new) and resolve unit collisions.  Assert lines
 * 0x10e/0x114; the shared 0x26ae40 format literal really reads
 * "assert_valid_real_vector2d" (Bungie typo, sic). */
void physics_update(int object_index, void *powered_mass_points,
                    void *mass_points, float *magic_force, float *magic_torque)
{
  object_datum_t *object;
  char *object_definition;
  const struct physics_definition *physics;
  struct physics_instance instance;
  real_vector3d total_force;
  real_vector3d total_torque;
  short powered_mass_point_index;

  object = (object_datum_t *)object_get_and_verify_type(object_index, -1);
  object_definition =
    (char *)tag_get(0x6f626a65 /* 'obje' */, object->definition_index);
  physics = (const struct physics_definition *)tag_get(
    0x70687973 /* 'phys' */, *(int *)(object_definition + 0x8c));

  if (physics->radius > 0.0f) {
    physics_update_old(object_index, powered_mass_points, mass_points,
                       (const real_vector3d *)magic_force,
                       (const real_vector3d *)magic_torque);
    return;
  }

  FUN_001509c0(&instance, object_index);
  if (powered_mass_points) {
    for (powered_mass_point_index = 0;
         powered_mass_point_index < physics->powered_mass_points.count;
         powered_mass_point_index++) {
      struct powered_mass_point_datum *powered_mass_point =
        (struct powered_mass_point_datum *)powered_mass_points +
        powered_mass_point_index;

      FUN_001093b0((float *)&powered_mass_point->rotation_matrix,
                   powered_mass_point->rotation);
      FUN_00109120((float *)&powered_mass_point->rotation_matrix);
    }
  }

  physics_compute_new(&instance, powered_mass_points, mass_points, &total_force,
                      &total_torque);

  {
    char *vehicle = (char *)object_get_and_verify_type(object_index, 2);
    real_vector3d *collision_force = (real_vector3d *)(vehicle + 0x460);
    real_vector3d *collision_torque = (real_vector3d *)(vehicle + 0x46c);

    total_force.i += collision_force->i;
    total_force.j += collision_force->j;
    total_force.k += collision_force->k;
    total_torque.i += collision_torque->i;
    total_torque.j += collision_torque->j;
    total_torque.k += collision_torque->k;
    collision_force->i = 0.0f;
    collision_force->j = 0.0f;
    collision_force->k = 0.0f;
    collision_torque->i = 0.0f;
    collision_torque->j = 0.0f;
    collision_torque->k = 0.0f;
  }

  if (magic_force) {
    if (!(boolean)real_vector3d_valid(magic_force)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "magic_force", (double)magic_force[0],
                              (double)magic_force[1], (double)magic_force[2]),
                     "c:\\halo\\SOURCE\\physics\\physics.c", 0x10e, 1);
      system_exit(-1);
    }
    total_force.i += magic_force[0];
    total_force.j += magic_force[1];
    total_force.k += magic_force[2];
  }
  if (magic_torque) {
    if (!(boolean)real_vector3d_valid(magic_torque)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "magic_torque", (double)magic_torque[0],
                              (double)magic_torque[1], (double)magic_torque[2]),
                     "c:\\halo\\SOURCE\\physics\\physics.c", 0x114, 1);
      system_exit(-1);
    }
    total_torque.i += magic_torque[0];
    total_torque.j += magic_torque[1];
    total_torque.k += magic_torque[2];
  }

  physics_update_new(&instance, powered_mass_points, mass_points, &total_force,
                     &total_torque);
  physics_compute_unit_collisions(object_index);
}

/* 0x1544d0 - accumulate float by delta and clamp/wrap within bounds. */
void FUN_001544d0(float *param_1, float *param_2, char param_3, float param_4)
{
  param_4 = param_4 + *param_1;
  *param_1 = param_4;
  if (param_4 < param_2[1]) {
    if (param_3 != '\0') {
      *param_1 = (param_2[0] - param_2[1]) + param_4;
      return;
    }
    *param_1 = param_2[1];
    return;
  }
  if (param_4 > *param_2) {
    if (param_3 != '\0') {
      *param_1 = param_4 - (*param_2 - param_2[1]);
      return;
    }
    *param_1 = *param_2;
  }
}

/* 0x154540 — physics_variable_speed_update (physics_variables.c).
 * Accelerate/decelerate *speed by |delta| scaled by the
 * parameter block, then clamp to +/- |delta| * scale.  Parameter offsets
 * +0x0/+0x4/+0x8/+0xc confirmed by the FMULs at 0x1545aa/0x154607/0x15454e/
 * 0x154556. */
void FUN_00154540(float *speed, void *accel_data, float delta)
{
  struct physics_variable_speed_parameters *parameters;
  float magnitude;
  float acceleration;
  float deceleration;
  float limit;

  parameters = (struct physics_variable_speed_parameters *)accel_data;
  magnitude = (float)fabs(delta);
  acceleration = magnitude * parameters->acceleration;
  deceleration = magnitude * parameters->deceleration;

  if (delta > 0.0f) {
    if (*speed <= -deceleration)
      *speed += deceleration;
    else if (*speed >= 0.0f)
      *speed += acceleration;
    else
      *speed = (*speed / deceleration + 1.0f) * acceleration;

    limit = magnitude * parameters->positive_scale;
    *speed = *speed > limit ? limit : *speed;
    return;
  }

  if (delta < 0.0f) {
    if (*speed >= deceleration)
      *speed -= deceleration;
    else if (*speed <= 0.0f)
      *speed -= acceleration;
    else
      *speed = (*speed / deceleration - 1.0f) * acceleration;

    limit = -magnitude * parameters->negative_scale;
    *speed = limit > *speed ? limit : *speed;
  }
}

/* 0x154630 — seek *speed toward target via FUN_00154540
 * (physics_variables.c).  Returns true (MOV AL,0x1) once the target
 * is reached or already equal; the running result lives in BL. */
boolean physics_variable_speed_update_seek(float *speed, void *parameters,
                                           float target, float delta)
{
  boolean result;

  result = false;
  if (*speed > target) {
    FUN_00154540(speed, parameters, -delta);
    if (*speed <= target) {
      *speed = target;
      result = true;
    }
  } else if (*speed < target) {
    FUN_00154540(speed, parameters, delta);
    if (*speed >= target) {
      *speed = target;
      result = true;
    }
  } else {
    result = true;
  }

  return result;
}

/* 0x1546b0 */
void FUN_001546b0(float *param_1, float *param_2, float *param_3, char param_4,
                  float param_5)
{
  FUN_00154540(param_2, param_3 + 2, param_5);
  FUN_001544d0(param_1, param_3, param_4, *param_2);
}

/* 0x1546f0 — physics_variable_position_get_seek_direction
 * (physics_variables.c): +1/-1 toward target, 0 when already there;
 * with wrap set, the short way round the [limits[1], limits[0]] range.
 * `limits` arrives in ECX (FLD [ECX] / FSUB [ECX+0x4] at 0x154711); `wrap`
 * is the byte at [EBP+0xc] (MOV AL / TEST AL,AL at 0x154706).  The
 * `!= 0.0f` guard is FCOM [0x2533c0] in the reference; VC71 always emits
 * FLD+FUCOMPP for float equality (lift-learnings section 47), a fixed cap. */
float FUN_001546f0(float position, boolean wrap, float target, float *limits)
{
  float direction;

  direction = target - position;
  if (direction != 0.0f) {
    if (wrap && (float)fabs(direction) > (limits[0] - limits[1]) * 0.5f)
      direction = -direction;

    direction = direction > 0.0f ? 1.0f : -1.0f;
  }

  return direction;
}

/* 0x154750 — advance a point-physics scalar toward its target position with
 * no velocity term; returns 1 when the target is snapped to.
 * Confirmed: FST [EBP-4] keeps the first FUN_001546f0 result, FCOMP [0x2533c0]
 * + TEST AH,0x44 / JNP takes the snap path only on equality; the second FCOMP
 * [EBP-4] + JP falls through (returns 0) only when the two results are equal.
 * The [EBP+0x10] slot is forwarded verbatim to both callees (float arg 2 of
 * FUN_001546f0, char param_3 of FUN_001544d0), same as FUN_001547d0. */
char FUN_00154750(float *out_pos, void *point_phys, boolean wrap,
                  float target_pos, float scale)
{
  float remaining;

  remaining = FUN_001546f0(*out_pos, wrap, target_pos, point_phys);
  if (remaining != *(const float *)0x2533c0) {
    FUN_001544d0(out_pos, (float *)point_phys, wrap, remaining * scale);
    if (FUN_001546f0(*out_pos, wrap, target_pos, point_phys) == remaining) {
      return 0;
    }
  }

  *out_pos = target_pos;
  return 1;
}

/* 0x1547d0 — step point physics towards target position; returns 1 if target
 * reached/reset */
char FUN_001547d0(float *out_pos, float *out_vel, void *point_phys,
                  boolean wrap, float target_pos, float accel)
{
  float initial_pos;
  float remaining;

  initial_pos = *out_pos;
  remaining = FUN_001546f0(*out_pos, wrap, target_pos, point_phys);
  if (remaining > 0.0f) {
    FUN_00154540(out_vel, (char *)point_phys + 8, remaining * accel);
    FUN_001544d0(out_pos, (float *)point_phys, wrap, *out_vel);
    if (FUN_001546f0(initial_pos, wrap, target_pos, point_phys) <= remaining) {
      return 0;
    }
  }
  *out_pos = target_pos;
  *out_vel = 0.0f;
  return 1;
}

void point_physics_initialize_for_new_map(void)
{
  *(float *)0x476200 = *(float *)0x325134 * *(float *)0x29d954; /* global_air_mass_over_radius_cubed */
  *(float *)0x4761fc = *(float *)0x325130 * *(float *)0x29d954; /* global_water_mass_over_radius_cubed */
}

void point_physics_dispose_from_old_map(void)
{
}

/* Scale a point-physics density value by volume (scale^3). */
float point_physics_definition_get_mass(int tag_data, float scale)
{
  return scale * *(float *)(tag_data + 4) * scale * scale;
}

/* 0x1548c0 — lerp two point-physics definitions into `result`
 * (point_physics.c).  Assert lines 0x14c-0x14f;
 * flags are taken from physics_a unblended. */
void *point_physics_definition_interpolate(void *physics_a, void *physics_b,
                                           float interpolation, void *out)
{
  const struct point_physics_definition *physics1;
  const struct point_physics_definition *physics2;
  struct point_physics_definition *result;
  float t0;

  physics1 = (const struct point_physics_definition *)physics_a;
  physics2 = (const struct point_physics_definition *)physics_b;
  result = (struct point_physics_definition *)out;
  t0 = 1.0f - interpolation;

  assert_halt_msg_at("physics1", "c:\\halo\\SOURCE\\physics\\point_physics.c",
                     0x14c, physics1);
  assert_halt_msg_at("physics2", "c:\\halo\\SOURCE\\physics\\point_physics.c",
                     0x14d, physics2);
  assert_halt_msg_at("t>=0.f && t<=1.f",
                     "c:\\halo\\SOURCE\\physics\\point_physics.c", 0x14e,
                     interpolation >= 0.0f && interpolation <= 1.0f);
  assert_halt_msg_at("result", "c:\\halo\\SOURCE\\physics\\point_physics.c",
                     0x14f, result);

  result->flags = physics1->flags;

  result->density = t0 * physics1->density + interpolation * physics2->density;
  result->runtime_water_buoyancy_scale =
    t0 * physics1->runtime_water_buoyancy_scale +
    interpolation * physics2->runtime_water_buoyancy_scale;
  result->runtime_air_buoyancy_scale =
    t0 * physics1->runtime_air_buoyancy_scale +
    interpolation * physics2->runtime_air_buoyancy_scale;
  result->runtime_mass_over_radius_cubed =
    t0 * physics1->runtime_mass_over_radius_cubed +
    interpolation * physics2->runtime_mass_over_radius_cubed;
  result->air_friction =
    t0 * physics1->air_friction + interpolation * physics2->air_friction;
  result->water_friction =
    t0 * physics1->water_friction + interpolation * physics2->water_friction;
  result->contact_friction = t0 * physics1->contact_friction +
                             interpolation * physics2->contact_friction;
  result->elasticity =
    t0 * physics1->elasticity + interpolation * physics2->elasticity;

  return result;
}

/* 0x154a20 — render point physics debugging / debug point */
void FUN_00154a20(void *obj, float *point, float val)
{
  void *color;

  color = *(void **)0x2ee6d0;
  if ((*(unsigned char *)obj & 2) == 0) {
    color = *(void **)0x2ee6d4;
  }

  FUN_00189150(1, point, val, color);
}

/* 0x154a50 — point_physics_update (point_physics.c): integrate a
 * point-physics particle for one step — buoyancy/gravity, wind drag, and up
 * to three bounces off collision_test_vector hits.  Assert lines 0xb9-0xbc,
 * 0x10d and 0x138; the 0xba message
 * uses the 0x26ae40 literal, which reads "assert_valid_real_vector2d" (sic).
 * The debug draw
 * at the tail is render_debug_point_physics inlined (its out-of-line copy is
 * FUN_00154a20). */
int point_physics_update(int flags, int physics_tag_data,
                         int *collision_location,
                         int force_weather_palette_index, /* name: PAL 2342 physics/point_physics.c:66 */
                         float *position, float *velocity, float *force,
                         float *collision_normal_out,
                         int16_t *surface_index_out, float radius,
                         float delta_time)
{
  /* physics_tag_data is the point_physics_definition; no local copy (a
   * copy frees the parameter home slot, and 2276 never reuses it). */
  int result;

  result = 0;

  if (!valid_real_point3d(position)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_point3d(%f, %f, %f)",
                            "position", (double)position[0],
                            (double)position[1], (double)position[2]),
                   "c:\\halo\\SOURCE\\physics\\point_physics.c", 0xb9, 1);
    system_exit(-1);
  }
  if (!(boolean)real_vector3d_valid(velocity)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_vector2d(%f, %f, %f)",
                            "translational_velocity", (double)velocity[0],
                            (double)velocity[1], (double)velocity[2]),
                   "c:\\halo\\SOURCE\\physics\\point_physics.c", 0xba, 1);
    system_exit(-1);
  }
  assert_halt_msg_at(
    "!translational_force || valid_real_vector3d(translational_force)",
    "c:\\halo\\SOURCE\\physics\\point_physics.c", 0xbb,
    !force || (boolean)real_vector3d_valid(force));
  assert_halt_msg_at("radius>=0.f",
                     "c:\\halo\\SOURCE\\physics\\point_physics.c", 0xbc,
                     radius >= 0.0f);

  if (delta_time != 0.0f) {
    struct collision_result collision;
    real_vector3d wind_vector;
    real_vector3d delta;
    real_vector3d parallel;
    real_vector3d perpendicular;
    float radius_squared = radius * radius;
    float radius_cubed = radius_squared * radius;
    float mass = ((const struct point_physics_definition *)physics_tag_data)->runtime_mass_over_radius_cubed;
    float buoyancy_scale;
    float friction;
    float dt_over_mass;
    float offset;
    float t;
    uint32_t collision_flags;
    uint32_t wind_flags;
    boolean underwater;
    short i;

    wind_flags = 0;
    SET_FLAG(wind_flags, 0,
             TEST_FLAG(((const struct point_physics_definition *)physics_tag_data)->flags, _point_physics_simple_wind_bit));
    SET_FLAG(wind_flags, 1,
             TEST_FLAG(((const struct point_physics_definition *)physics_tag_data)->flags, _point_physics_damped_wind_bit));

    if (TEST_FLAG(flags, _point_physics_ignore_position_bit)) {
      underwater =
        TEST_FLAG(flags, _point_physics_ignore_position_under_water_bit);
      FUN_00190240(position, &wind_vector.i, wind_flags,
                   (int16_t)force_weather_palette_index);
    } else {
      underwater = FUN_00190550(collision_location, position,
                                (int32_t)&wind_vector, wind_flags);
    }

    if (underwater) {
      mass += global_water_mass_over_radius_cubed;
      buoyancy_scale = ((const struct point_physics_definition *)physics_tag_data)->runtime_water_buoyancy_scale;
      friction = ((const struct point_physics_definition *)physics_tag_data)->water_friction * radius_squared;
      SET_FLAG(result, _point_physics_in_water_bit, true);
    } else {
      mass += global_air_mass_over_radius_cubed;
      buoyancy_scale = ((const struct point_physics_definition *)physics_tag_data)->runtime_air_buoyancy_scale;
      friction = ((const struct point_physics_definition *)physics_tag_data)->air_friction * radius_squared;
      SET_FLAG(result, _point_physics_in_air_bit, true);
    }

    mass = mass * radius_cubed;
    dt_over_mass = delta_time / mass;

    if (TEST_FLAG(((const struct point_physics_definition *)physics_tag_data)->flags, _point_physics_no_gravity_bit))
      buoyancy_scale = 0.0f;

    if (force && mass != 0.0f) {
      velocity[0] += dt_over_mass * force[0];
      velocity[1] += dt_over_mass * force[1];
      velocity[2] += dt_over_mass * force[2];
    }

    /* 0x32512c global_gravity; 30.0f is TICKS_PER_SECOND. */
    velocity[2] =
      buoyancy_scale * (30.0f * (*(float *)0x32512c * 30.0f)) * delta_time +
      velocity[2];

    if (mass == 0.0f) {
      t = (friction == 0.0f) ? 0.0f : 1.0f;
    } else {
      t = dt_over_mass * friction;
      t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }

    velocity[0] += (wind_vector.i - velocity[0]) * t;
    velocity[1] += (wind_vector.j - velocity[1]) * t;
    velocity[2] += (wind_vector.k - velocity[2]) * t;

    collision_flags = FLAG_BIT(0); /* front-facing surfaces */
    SET_FLAG(
      collision_flags, 6, /* media */
      TEST_FLAG(((const struct point_physics_definition *)physics_tag_data)->flags, _point_physics_water_collisions_bit) &&
        !TEST_FLAG(flags, _point_physics_force_no_collisions_bit));
    SET_FLAG(
      collision_flags, 5, /* structure */
      TEST_FLAG(((const struct point_physics_definition *)physics_tag_data)->flags, _point_physics_structure_collisions_bit) &&
        !TEST_FLAG(flags, _point_physics_force_no_collisions_bit));

    assert_halt_msg_at("global_current_collision_user_depth < "
                       "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                       "c:\\halo\\SOURCE\\physics\\point_physics.c", 0x10d,
                       global_current_collision_user_depth < 0x20);
    /* global_current_collision_users[depth++] = 13 (point physics). */
    collision_user_stack[global_current_collision_user_depth++] = 0xd;

    for (i = 0; delta_time != 0.0f && i < 3; i++) {
      delta.i = velocity[0] * delta_time;
      delta.j = velocity[1] * delta_time;
      delta.k = velocity[2] * delta_time;

      if (!FUN_0014df70(collision_flags, position, &delta.i, -1,
                        (int16_t *)&collision)) {
        if (collision.location.leaf_index != -1)
          *(struct collision_location *)collision_location = collision.location;

        *(real_point3d *)position = collision.point;
        break;
      }

      offset = radius > 0.005f ? 0.005f : radius;

      if (collision.type == 0) /* media */
        SET_FLAG(result, _point_physics_collided_with_water_bit, true);
      else if (collision.type == 2) /* structure */
        SET_FLAG(result, _point_physics_collided_with_structure_bit, true);

      if (collision_normal_out)
        *(real_vector3d *)collision_normal_out =
          *(real_vector3d *)collision.plane.normal;

      if (surface_index_out)
        *surface_index_out = collision.material_type;

      FUN_0010b8a0(velocity, collision.plane.normal, &parallel.i,
                   &perpendicular.i);
      velocity[0] = (1.0f - ((const struct point_physics_definition *)physics_tag_data)->contact_friction) * perpendicular.i -
                    parallel.i * ((const struct point_physics_definition *)physics_tag_data)->elasticity;
      velocity[1] = (1.0f - ((const struct point_physics_definition *)physics_tag_data)->contact_friction) * perpendicular.j -
                    parallel.j * ((const struct point_physics_definition *)physics_tag_data)->elasticity;
      velocity[2] = (1.0f - ((const struct point_physics_definition *)physics_tag_data)->contact_friction) * perpendicular.k -
                    parallel.k * ((const struct point_physics_definition *)physics_tag_data)->elasticity;

      if (collision.location.leaf_index != -1)
        *(struct collision_location *)collision_location = collision.location;

      position[0] = collision.plane.normal[0] * offset + collision.point.x;
      position[1] = collision.plane.normal[1] * offset + collision.point.y;
      position[2] = collision.plane.normal[2] * offset + collision.point.z;

      delta_time -= collision.t * delta_time;
    }

    assert_halt_msg_at("global_current_collision_user_depth > 1",
                       "c:\\halo\\SOURCE\\physics\\point_physics.c", 0x138,
                       global_current_collision_user_depth > 1);
    --global_current_collision_user_depth;
  }

  if (*(boolean *)0x5a5e20) /* debug_point_physics */
    render_debug_point_physics_inline((const struct point_physics_definition *)physics_tag_data, position, radius);

  return result;
}
