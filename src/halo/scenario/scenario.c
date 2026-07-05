#include "x87_math.h"

/* 0x18aef0 — object bounding-sphere accessor: fetch an object's world-space
 * center (object_data + 0x50/0x54/0x58, three contiguous floats) and the
 * bounding radius from its 'obje' definition tag (+0x104). The object handle
 * is resolved with no type restriction (0xffffffff). Asserts that both output
 * pointers are non-NULL. cdecl: handle [EBP+8], center [EBP+0xc] (-> ESI),
 * radius [EBP+0x10] (-> EBX); plain MOV dword copies reinterpreted as float. */
void FUN_0018aef0(int object_handle, float *center, float *radius)
{
  char *obj;
  char *def_tag;

  obj = (char *)object_get_and_verify_type(object_handle, 0xffffffff);
  if (center == 0) {
    display_assert("center", "..\\objects\\objects.h", 0x227, 1);
    system_exit(-1);
  }
  if (radius == 0) {
    display_assert("radius", "..\\objects\\objects.h", 0x228, 1);
    system_exit(-1);
  }
  center[0] = *(float *)(obj + 0x50);
  center[1] = *(float *)(obj + 0x54);
  center[2] = *(float *)(obj + 0x58);
  def_tag = (char *)tag_get(0x6f626a65 /* 'obje' */, *(int *)obj);
  *radius = *(float *)(def_tag + 0x104);
}

/* 0x18af90 — cached object render-states pool: allocate the game-state
 * data array (0x100 entries x 0x100 bytes) and store it at the global
 * pool pointer; assert on allocation failure. */
void FUN_0018AF90(void)
{
  *(data_t **)0x50652c =
    game_state_data_new("cached object render states", 0x100, 0x100);
  if (*(data_t **)0x50652c == 0) {
    display_assert("cached_object_render_states",
                   "c:\\halo\\SOURCE\\render\\render_objects.c", 0x7d, 1);
    system_exit(-1);
  }
}

/* 0x18afd0 — cached object render-states pool free: delete all entries in the
 * cached_object_render_states data pool. */
void FUN_0018afd0(void)
{
  data_delete_all(*(data_t **)0x50652c);
}

/* 0x18afe0 — cached object render-states pool dispose: if the pool exists
 * and is initialized (+0x24 valid flag set), invalidate its data array. */
void FUN_0018afe0(void)
{
  if (*(data_t **)0x50652c != 0 &&
      *(char *)((char *)*(data_t **)0x50652c + 0x24) != 0) {
    data_make_invalid(*(data_t **)0x50652c);
  }
}

/* 0x18b000 — cached object render-states pool clear: reset the global pool
 * pointer to NULL. */
void FUN_0018B000(void)
{
  *(data_t **)0x50652c = 0;
}

/* 0x18b010 — predicate: should the given object be treated as the local
 * player's first-person view subject? Resolves the local player's player
 * index (local_player_get_player_index of the local-player index global at
 * 0x506548). If valid, fetches that player's controlled unit handle from the
 * players data pool (*(int*)0x5aa6d4) at player_data+0x34. The object qualifies
 * (returns 1) when it is that unit AND the director perspective for the local
 * player (director_get_perspective) is 0 (first-person). Otherwise it falls
 * back to FUN_00085150(object_handle) (observer/cinematic check) and returns 1
 * if that is set, else 0.
 * ABI: object_handle in ESI (@<esi>), frameless. Returns int bool. Sole caller
 * FUN_0018c100 sets ESI = *buf (the object handle). */
int FUN_0018b010(int object_handle)
{
  int unit_handle;

  if (local_player_get_player_index(*(int16_t *)0x506548) == -1) {
    unit_handle = -1;
  } else {
    unit_handle = *(
      int *)((int)datum_get(*(data_t **)0x5aa6d4, local_player_get_player_index(
                                                    *(int16_t *)0x506548)) +
             0x34);
  }

  if (unit_handle == object_handle &&
      director_get_perspective(*(int16_t *)0x506548) == 0) {
    return 1;
  }

  if (FUN_00085150(object_handle)) {
    return 1;
  }
  return 0;
}

/* 0x18b080 — rebuild the per-frame rendered-object list. Resets the object
 * marker pass, gathers up to 0x100 collideable then noncollideable objects
 * across all clusters into the int[256] table at 0x4d82d4 (count tracked in
 * the int16 counter at 0x4d82d0), ends the marker pass, then errors once if
 * the table filled to MAXIMUM_RENDERED_OBJECTS (0x100). cdecl, void(void). */
void FUN_0018b080(void)
{
  short count;

  object_reset_markers();
  count = (short)FUN_00196c90(
    (int)0x4d82d4, 0x100, (void *)cluster_partition_object_iter_first,
    (void *)cluster_partition_object_iter_next, (void *)FUN_0018aef0,
    (void *)object_markers_need_update, (void *)object_mark);
  *(short *)0x4d82d0 = count;
  count = (short)FUN_00196c90(
    0x4d82d4 + (int)count * 4, 0x100 - *(unsigned short *)0x4d82d0,
    (void *)cluster_get_first_noncollideable_object,
    (void *)cluster_get_next_noncollideable_object, (void *)FUN_0018aef0,
    (void *)object_markers_need_update, (void *)object_mark);
  *(short *)0x4d82d0 = (short)(*(short *)0x4d82d0 + count);
  object_marker_end();
  if (*(unsigned short *)0x4d82d0 == 0x100 && *(char *)0x4d86d5 == 0) {
    error(2, "MAXIMUM_RENDERED_OBJECTS exceeded.");
    *(char *)0x4d86d5 = 1;
  }
}

/* 0x18b130 — screen-space diameter (in pixels) of an object's bounding
 * sphere against the render frustum globals at 0x5065a4. During a cinematic,
 * objects whose flag bit 0x400000 (object+0x4) is set report FLT_MAX
 * (0x2548fc) — always-visible override. Otherwise fetches the bounding
 * sphere (FUN_0001aae0) and tail-returns
 * render_frustum_sphere_diameter_in_pixels(). Register ABI: object handle in
 * ESI; float return in ST(0). Callers (3, all in FUN_0018c100) load ESI from
 * the PVS record's first dword. kb.json previously misnamed this
 * global_structure_bsp_tag_index_get. */
float FUN_0018b130(int object_handle)
{
  float center[3];
  float radius;
  char *object;

  if (cinematic_in_progress()) {
    object = (char *)object_get_and_verify_type(object_handle, -1);
    if ((*(int *)(object + 4) & 0x400000) != 0) {
      return *(float *)0x2548fc; /* FLT_MAX */
    }
  }
  FUN_0001aae0(object_handle, center, &radius);
  return render_frustum_sphere_diameter_in_pixels((void *)0x5065a4, center,
                                                  radius);
}

/* parent_model_effect — 40-byte (0x28) render-effect descriptor threaded down
 * the object hierarchy by FUN_0018b190. Copied verbatim (10 dwords) from the
 * parent's record, then selected fields are overwritten per object state.
 * Field bit-copies of object float positions are done as raw dwords (matching
 * the MSVC integer MOV stores, not FLD/FSTP). */
typedef struct render_model_effect {
  int16_t type; /* +0x00 */
  int16_t pad_02; /* +0x02 */
  int field_04; /* +0x04 */
  int field_08; /* +0x08 */
  int object_index; /* +0x0C */
  int position[3]; /* +0x10 (x,y,z float bits) */
  int modifier_shader; /* +0x1C ('shdr' tag data ptr) */
  void *node_transforms; /* +0x20 (object_data + 0x168) */
  void *node_matrices; /* +0x24 (object_data + 0xe4) */
} render_model_effect_t;

/* 0x18b190 — recursive per-object render walker (render_objects.c). Iterates
 * an object's sibling chain (object_data+0xc4) and recurses into each object's
 * first-child chain (object_data+0xc8), submitting each visible object's model
 * for rendering. render_data->pass byte (+8) selects the normal pass (0) vs a
 * shadow pass (non-zero); render_data->lighting (+4) is the cluster lighting.
 *
 * NOTE: each of the two model-submit sites is a single 13-argument cdecl call
 * to FUN_00123ed0 (add $0x34 = 13 dwords of stack cleanup). MSVC evaluates the
 * cdecl arguments right-to-left, so object_get_node_matrices (0x13fe70), which
 * supplies arg 3, is called in the middle of the push sequence — after args
 * 4..13 and before args 1..2. Passing only three of these arguments would feed
 * the callee stack garbage for the remaining ten, so all thirteen are supplied.
 *
 * Object misattribution note: assert __FILE__ strings identify this TU as
 * render_objects.c, not scenario.c. */
void FUN_0018b190(void *render_data, void *parent_model_effect,
                  int object_handle)
{
  char *rd = (char *)render_data;
  char *obj;
  char *obj3;
  int obje;
  int fp_unit;
  int obj_datum;
  float dist;
  float *fwd;
  float center[3];
  float radius;
  float marker[3];
  void *pair[2];
  render_model_effect_t record;
  char text[512];

  while (object_handle != -1) {
    obj = (char *)object_get_and_verify_type(object_handle, -1);

    /* First-person view subject: the local player's controlled unit. */
    if (local_player_get_player_index(*(unsigned short *)0x506548) == -1) {
      fp_unit = -1;
    } else {
      fp_unit = *(int *)((char *)datum_get(*(data_t **)0x5aa6d4,
                                           local_player_get_player_index(
                                             *(unsigned short *)0x506548)) +
                         0x34);
    }

    /* Render this object unless it is the local first-person unit rendered in
     * first-person perspective (unless observed) — or the debug override. */
    if ((((fp_unit != object_handle) ||
          (director_get_perspective(*(unsigned short *)0x506548) != 0)) &&
         (FUN_00085150(object_handle) == 0)) ||
        (*(char *)0x506574 != 0)) {
      if (*(char *)(rd + 8) == 0) {
        if (parent_model_effect == 0) {
          display_assert("parent_model_effect",
                         "c:\\halo\\SOURCE\\render\\render_objects.c", 0x186,
                         1);
          system_exit(-1);
        }
        record = *(render_model_effect_t *)parent_model_effect;
        if (*(short *)parent_model_effect == 2) {
          record.type = 0;
          record.modifier_shader = 0;
          record.node_transforms = 0;
          record.node_matrices = 0;
        }
      }

      if ((*(unsigned char *)(obj + 4) & 1) == 0) {
        obje = (int)tag_get(0x6f626a65, *(int *)obj);

        /* Level-of-detail: pixel diameter of the object's bounding sphere,
         * clamped to +inf while cinematic-hidden. */
        if ((cinematic_in_progress() == 0) ||
            ((*(unsigned int *)((char *)object_get_and_verify_type(
                                  object_handle, -1) +
                                4) &
              0x400000) == 0)) {
          FUN_0001aae0(object_handle, center, &radius);
          dist = render_frustum_sphere_diameter_in_pixels((void *)0x5065a4,
                                                          center, radius);
        } else {
          dist = 3.4028235e+38f;
        }

        if (*(int *)(rd + 4) == 0) {
          display_assert("data->lighting",
                         "c:\\halo\\SOURCE\\render\\render_objects.c", 0x19f,
                         1);
          system_exit(-1);
        }

        if (*(char *)(rd + 8) == 0) {
          /* Modifier shader validation. */
          if (*(int *)(obje + 0x9c) != -1) {
            record.modifier_shader =
              (int)tag_get(0x73686472, *(int *)(obje + 0x9c));
            if (shader_type_is_valid_for_modifier(
                  *(int16_t *)(record.modifier_shader + 0x24)) == 0) {
              error(2, "### ERROR invalid modifier shader",
                    (int)*(short *)(record.modifier_shader + 0x24));
              record.modifier_shader = 0;
            } else {
              record.node_transforms = obj + 0x168;
              record.node_matrices = obj + 0xe4;
            }
          }

          /* Early-fade effect type 1 (LOD-driven). */
          if (((1 << (*(unsigned char *)(obj + 0x64) & 0x1f)) & 3) != 0) {
            obj3 = (char *)object_get_and_verify_type(object_handle, 3);
            if (*(float *)(obj3 + 0x32c) > *(float *)0x2533c0) {
              record.position[0] = *(int *)(obj + 0x50);
              record.type = 1;
              record.object_index = object_handle;
              record.position[1] = *(int *)(obj + 0x54);
              record.position[2] = *(int *)(obj + 0x58);
              record.field_04 = *(int *)(obj3 + 0x32c);
              record.field_08 = *(int *)(obj3 + 0x330);
            }
          }

          /* Effect type 2 (object-definition flagged). */
          if ((*(unsigned char *)(obje + 2) & 2) != 0) {
            record.position[0] = *(int *)(obj + 0x50);
            record.type = 2;
            record.object_index = object_handle;
            record.position[1] = *(int *)(obj + 0x54);
            record.position[2] = *(int *)(obj + 0x58);
          }

          /* Debug marker for inactive objects. */
          obj_datum = (int)datum_get(*(data_t **)0x5a8d50, object_handle);
          if ((*(char *)0x4d86d4 != 0) && (*(int *)(obj + 0xcc) == -1) &&
              ((*(unsigned char *)(obj_datum + 2) & 1) == 0) &&
              (*(int *)(obje + 0x34) != -1)) {
            fwd = *(float **)0x31fc44;
            /* Reload the marker-scale global per component (matches MSVC's
             * per-use FLD [0x2549d4]; caching it in a local loses ~0.6pp VC71). */
            marker[0] = fwd[0] * *(float *)0x2549d4 + *(float *)(obj + 0x50);
            marker[1] = fwd[1] * *(float *)0x2549d4 + *(float *)(obj + 0x54);
            marker[2] = fwd[2] * *(float *)0x2549d4 + *(float *)(obj + 0x58);
            crt_sprintf(text, "inactive %s",
                        tag_name_strip_path(tag_get_name(*(int *)obj)));
            FUN_00189150(0, (float *)(obj + 0x50), *(float *)(obj + 0x5c),
                         *(void **)0x2ee6d8);
            FUN_00189cb0(0, marker, text, *(int *)0x2ee6d8);
          }

          /* Single 13-argument model-submit. MSVC evaluates the cdecl args
           * right-to-left, so the object_get_node_matrices call (arg 3) is
           * emitted after the trailing ten pushes and before the first two —
           * matching the interleaved getter call in the original. */
          FUN_00123ed0(*(int *)(obje + 0x34), dist,
                       object_get_node_matrices(object_handle), obj + 0x130,
                       obj + 0x168, obj + 0xe4, *(int *)(rd + 4), obj + 0x50,
                       *(int *)(obj + 0x5c), &record, object_handle,
                       *(unsigned short *)(obj + 0x126),
                       (*(char *)(rd + 9) != 0) ? 4 : 0);
          if (*(char *)0x506528 != 0) {
            FUN_0013c920(object_handle);
          }
        } else {
          /* Shadow pass: no record built (arg10 NULL, arg13 constant 2). */
          FUN_00123ed0(*(int *)(obje + 0x34), dist * *(float *)0x2533e4,
                       object_get_node_matrices(object_handle), obj + 0x130,
                       obj + 0x168, obj + 0xe4, *(int *)(rd + 4), obj + 0x50,
                       *(int *)(obj + 0x5c), 0, object_handle,
                       *(unsigned short *)(obj + 0x126), 2);
        }
      }

      /* Attached widgets (normal pass only). */
      if ((*(char *)(rd + 8) == 0) && (*(int *)(obj + 0x11c) != -1)) {
        pair[0] = obj + 0x168;
        pair[1] = obj + 0xe4;
        widgets_render_object_widgets(object_handle, *(int *)(rd + 4), pair);
      }

      /* Recurse into the first-child chain. */
      if (*(int *)(obj + 0xc8) != -1) {
        FUN_0018b190(render_data,
                     (*(char *)(rd + 8) != 0) ? (void *)0 : &record,
                     *(int *)(obj + 0xc8));
      }
    }

    object_handle = *(int *)(obj + 0xc4);
  }
}

/* 0x18b830 — render an object's blob shadow. ctx is a small shadow
 * descriptor: +0x0 object handle, +0x4 render-state pointer (forward vector
 * at +0x5c, ambient rgb at +0x68), +0xc receives the 4x3 shadow basis
 * matrix, +0x40 is forwarded as the final output block. Builds the object
 * bounding sphere, derives a basis from a vector perpendicular to the render
 * forward, fades the shadow color toward white by `fade` (active-camouflage
 * power at unit+0x32c reduces the fade for units), then hands off to the
 * decals shadow renderer FUN_0017ccb0. Register arg: ctx @<esi>. */
void FUN_0018b830(void *ctx, float fade)
{
  float center[3];
  float perp[3];
  float color[3];
  float radius;
  float fade_local;
  float one_minus;
  int state;
  char *obj;

  fade_local = fade;
  FUN_0001aae0(*(int *)ctx, center, &radius);
  perpendicular3d((float *)(*(int *)((char *)ctx + 4) + 0x5c), perp);
  /* length result discarded (FSTP ST0 in the original) */
  normalize3d(perp);
  matrix4x3_from_forward_up_position((char *)ctx + 0xc, center, perp,
                                     (float *)(*(int *)((char *)ctx + 4) +
                                               0x5c));
  state = *(int *)((char *)ctx + 4);
  color[0] = *(float *)(state + 0x68);
  color[1] = *(float *)(state + 0x6c);
  color[2] = *(float *)(state + 0x70);
  obj = (char *)object_get_and_verify_type(*(int *)ctx, -1);
  if ((1 << (*(uint8_t *)(obj + 0x64) & 0x1f) & 3u) != 0) {
    obj = (char *)object_get_and_verify_type(*(int *)ctx, 3);
    if (*(float *)(obj + 0x32c) > *(const float *)0x2533c0) {
      fade_local =
        (*(const float *)0x2533c8 - *(float *)(obj + 0x32c)) * fade;
    }
  }
  one_minus = *(const float *)0x2533c8 - fade_local;
  color[0] = color[0] * fade_local + one_minus;
  color[1] = color[1] * fade_local + one_minus;
  color[2] = color[2] * fade_local + one_minus;
  FUN_0017ccb0(*(int *)ctx, (float *)((char *)ctx + 0xc), color, radius,
               (float *)((char *)ctx + 0x40));
}

/* 0x18b930 — build a plane from a normal and a point on the plane, plus the
 * flipped (negated) plane. plane = {n.x, n.y, n.z, dot(n, point)}; flipped =
 * -plane. The normal components are copied as raw dwords (integer moves in
 * the original). No callers in the binary (dead helper). Register args:
 * plane @<eax>, flipped @<ecx>, normal @<edx>. */
void FUN_0018b930(float *plane, float *flipped, float *normal, float *point)
{
  *(int32_t *)plane = *(int32_t *)normal;
  *(int32_t *)(plane + 1) = *(int32_t *)(normal + 1);
  *(int32_t *)(plane + 2) = *(int32_t *)(normal + 2);
  plane[3] =
    normal[2] * point[2] + normal[1] * point[1] + normal[0] * point[0];
  flipped[0] = -plane[0];
  flipped[1] = -plane[1];
  flipped[2] = -plane[2];
  flipped[3] = -plane[3];
}

/* 0x18b990 — build an oriented-box clip volume for a visibility/portal query.
 *
 * The ECX descriptor holds three axis vectors A (+0x10), B (+0x1c) and a
 * direction axis C (+0x28), a center point P (+0x34) and a scalar extent r
 * (+0x40). Two products are produced and forwarded to FUN_00196190:
 *   - a 6-plane array {nx,ny,nz,d} (24 floats): +C/-C (asymmetric extents
 *     r*0.5 in front, r*4.0 behind), +A/-A and +B/-B (extent r each). d is the
 *     signed plane offset dot(axis,P) - extent.
 *   - a 6-scalar axis-expanded bound around P. For x/y the direction-axis
 *     contribution uses a sign-selected multiplier (component<=0 -> *4.0f,
 *     component>0 -> *-0.5f); z uses the fixed 4.0f/0.5f pair. The absolute
 *     sums |A.i|+|B.i| widen each bound.
 * Then runs the profile-exit pair FUN_0017cd00 (0-arg thunk).
 * Fastcall: descriptor pointer in ECX. */
void FUN_0018b990(void *volume)
{
  float *A;
  float *B;
  float *Cv;
  float *P;
  float r;
  float r4;
  float dot;
  float ax, bx, ay, by, az, bz; /* |A.x|,|B.x|,|A.y|,|B.y|,|A.z|,|B.z| */
  float mlx, mhx, mly, mhy; /* sign-selected multipliers for x/y bounds */
  float planes[24]; /* 6 planes {nx,ny,nz,d}, EBP-0x80..-0x24 */
  float scalars[6]; /* expanded bound, EBP-0x20..-0xc */

  A = (float *)((char *)volume + 0x10);
  B = (float *)((char *)volume + 0x1c);
  Cv = (float *)((char *)volume + 0x28);
  P = (float *)((char *)volume + 0x34);
  r = *(float *)((char *)volume + 0x40);
  r4 = r * 4.0f;

  /* plane 0: +C, d = dot(C,P) - r*0.5 (dot accumulates z,y,x per the original) */
  dot = Cv[2] * P[2] + Cv[1] * P[1] + Cv[0] * P[0];
  planes[0] = Cv[0];
  planes[1] = Cv[1];
  planes[2] = Cv[2];
  planes[3] = dot - r * 0.5f;
  /* plane 1: -C, d = -dot(C,P) - r*4.0 */
  planes[4] = -Cv[0];
  planes[5] = -Cv[1];
  planes[6] = -Cv[2];
  planes[7] = -dot - r4;

  /* plane 2: +A, d = dot(A,P) - r */
  dot = A[2] * P[2] + A[1] * P[1] + A[0] * P[0];
  planes[8] = A[0];
  planes[9] = A[1];
  planes[10] = A[2];
  planes[11] = dot - r;
  /* plane 3: -A, d = -dot(A,P) - r */
  planes[12] = -A[0];
  planes[13] = -A[1];
  planes[14] = -A[2];
  planes[15] = -dot - r;

  /* plane 4: +B, d = dot(B,P) - r */
  dot = B[2] * P[2] + B[1] * P[1] + B[0] * P[0];
  planes[16] = B[0];
  planes[17] = B[1];
  planes[18] = B[2];
  planes[19] = dot - r;
  /* plane 5: -B, d = -dot(B,P) - r */
  planes[20] = -B[0];
  planes[21] = -B[1];
  planes[22] = -B[2];
  planes[23] = -dot - r;

  /* per-component absolute values of the A and B axes (FCOM 0 + FCHS) */
  ax = A[0];
  if (ax < 0.0f)
    ax = -ax;
  bx = B[0];
  if (bx < 0.0f)
    bx = -bx;
  ay = A[1];
  if (ay < 0.0f)
    ay = -ay;
  by = B[1];
  if (by < 0.0f)
    by = -by;
  az = A[2];
  if (az < 0.0f)
    az = -az;
  bz = B[2];
  if (bz < 0.0f)
    bz = -bz;

  /* direction-axis sign-selected multipliers for the x/y expanded bounds */
  mlx = -0.5f;
  if (Cv[0] <= 0.0f)
    mlx = 4.0f;
  mhx = -0.5f;
  if (Cv[0] > 0.0f)
    mhx = 4.0f;
  mly = -0.5f;
  if (Cv[1] <= 0.0f)
    mly = 4.0f;
  mhy = -0.5f;
  if (Cv[1] > 0.0f)
    mhy = 4.0f;

  scalars[0] = (Cv[0] * mlx + -(bx + ax)) * r + P[0];
  scalars[1] = (Cv[0] * mhx + bx + ax) * r + P[0];
  scalars[2] = (Cv[1] * mly + -(by + ay)) * r + P[1];
  scalars[3] = (Cv[1] * mhy + by + ay) * r + P[1];
  scalars[4] = (Cv[2] * 4.0f + -(bz + az)) * r + P[2];
  scalars[5] = ((bz + az) - Cv[2] * 0.5f) * r + P[2];

  FUN_00196190(P, r4, scalars, 6, planes);
  FUN_0017cd00();
}

/* The original FSTPs each FSUB delta to a 32-bit local and reloads it for
 * the clamp compares, so they see the float-rounded value; clang keeps the
 * delta in x87 ST and compares at 80-bit extended precision, which can take
 * a different branch on overflow/NaN edges (rasterizer.c:1248 precedent).
 * Force the same store+reload rounding. No-op under VC71, which spills. */
#if !defined(_MSC_VER) || defined(__clang__)
#define FADE_ROUND32(x) asm volatile("" : "+m"(x))
#else
#define FADE_ROUND32(x) ((void)0)
#endif

/* 0x18b610 — fade a 3-float lighting vector toward its desired values: each
 * component steps by its delta clamped to [-step, +step]. Used by
 * FUN_0018bc60 smooth lighting for the ambient/distant color triples.
 * Register args: cur @<ecx>, des @<edx>; step on the stack. */
void FUN_0018b610(float *cur, float *des, float step)
{
  float delta;

  delta = des[0] - cur[0];
  FADE_ROUND32(delta);
  cur[0] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[0];
  delta = des[1] - cur[1];
  FADE_ROUND32(delta);
  cur[1] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[1];
  delta = des[2] - cur[2];
  FADE_ROUND32(delta);
  cur[2] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[2];
}

/* 0x18b6b0 — 4-float variant of FUN_0018b610 (delta clamped to
 * [-step, +step] per component). Register args: cur @<ecx>, des @<edx>. */
void FUN_0018b6b0(float *cur, float *des, float step)
{
  float delta;

  delta = des[0] - cur[0];
  FADE_ROUND32(delta);
  cur[0] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[0];
  delta = des[1] - cur[1];
  FADE_ROUND32(delta);
  cur[1] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[1];
  delta = des[2] - cur[2];
  FADE_ROUND32(delta);
  cur[2] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[2];
  delta = des[3] - cur[3];
  FADE_ROUND32(delta);
  cur[3] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[3];
}

/* 0x18b780 — direction variant of FUN_0018b610: fades a 3-float direction
 * toward its desired values (delta clamped to [-step, +step] per component),
 * then renormalizes the result via normalize3d (float length return
 * discarded — FSTP ST0 at 0x18b818). Register args: cur @<ecx>,
 * des @<edx>. */
void FUN_0018b780(float *cur, float *des, float step)
{
  float delta;

  delta = des[0] - cur[0];
  FADE_ROUND32(delta);
  cur[0] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[0];
  delta = des[1] - cur[1];
  FADE_ROUND32(delta);
  cur[1] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[1];
  delta = des[2] - cur[2];
  FADE_ROUND32(delta);
  cur[2] = ((delta < -step) ? -step : ((delta > step) ? step : delta)) +
           cur[2];
  normalize3d(cur);
}

/* 0x18bc60 — refresh a cached object render-state's lighting
 * (render\render_objects.c, asserts 0x27b/0x2b2). The render-state datum is
 * fetched from the pool at 0x50652c by index (@<eax>). Staleness is judged
 * from the frames since the last desired-lighting submit (state+0x8 vs the
 * frame counter 0x506540) with a LOD-dependent budget (every frame above
 * 0x254f90, every 3 above 0x253f00, else every 10), but only for
 * dynamically-lit objects (object flags bit14). A stale state whose
 * transform also moved (>1 frame since state+0x10) forces a full rebuild.
 * Rebuild recomputes the desired lighting (FUN_0013bce0 into state+0x88)
 * and stamps lod/submit-frame; when the tick counter advanced
 * (0x506544 vs state+0xc) the point lights are re-gathered (FUN_0013aa10).
 * A forced rebuild copies desired -> current (state+0x14) wholesale
 * (0x74 bytes); a stale state with smooth lighting on (0x323c04) instead
 * fades current toward desired per field (ambient/distant colors and
 * directions via the @ecx/@edx helpers FUN_0018b610/b6b0/b780 at rate
 * 0.03, the point-light intensities at 0.012) — skipped entirely when the
 * object sits at the world origin and has no attached 0x80-type check
 * (object_try...(handle, 0x80) == NULL). Otherwise only the point-light
 * count/indices trio (+0xc8..+0xd0 -> +0x54..+0x5c) is refreshed when the
 * tick advanced. Ends validating every point-light index against the
 * rasterizer light count (signed short 0x5a8d5a) and stamping the
 * tick/frame counters. Register arg: render_state_index @<eax>. */
void FUN_0018bc60(int render_state_index, int object_handle, float lod,
                  char force_rebuild)
{
  char *state;
  char *obj;
  float *cur;
  float *des;
  float pos[3]; /* EBP-0x14 */
  int ticks;
  int frames_xform;
  int frames_submit;
  int idx;
  int16_t i;
  char stale;

  state = (char *)datum_get(*(data_t **)0x50652c, render_state_index);
  ticks = *(int *)0x506544 - *(int *)(state + 0xc);
  frames_xform = *(int *)0x506540 - *(int *)(state + 0x10);
  frames_submit = *(int *)0x506540 - *(int *)(state + 8);
  stale = 0;
  if (frames_submit < 0 || ticks < 0) {
    ticks = 1;
    frames_submit = 1;
  }
  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if ((*(uint32_t *)(obj + 4) & 0x4000) != 0) {
    if (lod > *(const float *)0x254f90) {
      stale = frames_submit > 0;
    } else if (lod > *(const float *)0x253f00) {
      stale = frames_submit > 3;
    } else {
      stale = frames_submit > 10;
    }
    if (stale != 0 && frames_xform > 1) {
      obj = (char *)object_get_and_verify_type(object_handle, -1);
      if ((*(uint32_t *)(obj + 4) & 0x4000) != 0) {
        force_rebuild = 1;
        goto rebuild;
      }
    }
  }
  if (force_rebuild != 0 || stale != 0) {
  rebuild:
    *(int *)(state + 4) = object_handle;
    FUN_0013bce0(object_handle, (float *)(state + 0x88));
    *(float *)(state + 0xfc) = lod;
    *(int *)(state + 8) = *(int *)0x506540;
    if (force_rebuild != 0)
      goto update_points;
  }
  if (ticks > 0) {
  update_points:
    FUN_0013aa10(object_handle, (int)(state + 0x88));
    if (force_rebuild != 0)
      goto copy_all;
  }
  if (stale == 0) {
    if (ticks > 0) {
      *(int16_t *)(state + 0x54) = *(int16_t *)(state + 0xc8);
      *(int32_t *)(state + 0x58) = *(int32_t *)(state + 0xcc);
      *(int32_t *)(state + 0x5c) = *(int32_t *)(state + 0xd0);
    }
  } else if (*(char *)0x323c04 != 0) {
    cur = (float *)(state + 0x14);
    des = (float *)(state + 0x88);
    if (*(int16_t *)(state + 0x94) != 2) {
      display_assert("state->desired_lighting.distant_light_count==2",
                     "c:\\halo\\SOURCE\\render\\render_objects.c", 0x27b, 1);
      system_exit(-1);
    }
    object_get_root_location(object_handle, pos, 0);
    if (pos[0] != *(const float *)0x2533c0 ||
        pos[1] != *(const float *)0x2533c0 ||
        pos[2] != *(const float *)0x2533c0 ||
        object_try_and_get_and_verify_type(object_handle, 0x80) != NULL) {
      FUN_0018b610(cur, des, 0.03f);
      FUN_0018b6b0(cur + 19, des + 19, 0.03f);
      FUN_0018b610(cur + 4, des + 4, 0.03f);
      FUN_0018b780(cur + 7, des + 7, 0.03f);
      FUN_0018b610(cur + 10, des + 10, 0.03f);
      FUN_0018b780(cur + 13, des + 13, 0.03f);
      FUN_0018b780(cur + 23, des + 23, 0.012f);
      FUN_0018b610(cur + 26, des + 26, 0.03f);
    }
    *(int16_t *)(state + 0x54) = *(int16_t *)(state + 0xc8);
    *(int32_t *)(state + 0x58) = *(int32_t *)(state + 0xcc);
    *(int32_t *)(state + 0x5c) = *(int32_t *)(state + 0xd0);
  } else {
  copy_all:
    qmemcpy(state + 0x14, state + 0x88, 0x74);
  }
  for (i = 0; i < *(int16_t *)(state + 0x54); i++) {
    idx = *(int *)(state + 0x58 + (int)i * 4);
    if (idx < 0 || idx >= (int)*(int16_t *)0x5a8d5a) {
      display_assert(
        "state->lighting.point_light_indices[point_light_index]>=0 && "
        "state->lighting.point_light_indices[point_light_index]<"
        "debug_rasterizer_light_count",
        "c:\\halo\\SOURCE\\render\\render_objects.c", 0x2b2, 1);
      system_exit(-1);
    }
  }
  *(int *)(state + 0xc) = *(int *)0x506544;
  *(int *)(state + 0x10) = *(int *)0x506540;
}

/* 0x18bf80 — resolve/allocate the cached object render-state datum index for
 * an object. The object stores its render-state index at object_data+0x120.
 *   1. If that index is valid (!= -1) and the pooled datum at that index still
 *      references this object (datum+0x4 == object_handle) and the object's
 *      stored index is still valid, refresh that render-state (FUN_0018bc60,
 *      force_rebuild=0) and return the existing index.
 *   2. Otherwise allocate a new datum slot (data_new_at_index). If the pool is
 *      full (-1), evict the entry with the greatest "age" score: for each
 * active datum, age = (float)(DAT_00506544 - datum+0xc); a negative age is
 * clamped to 1000.0f (0x254cb8). The slot with the maximum age wins; start the
 *      search at -FLT_MAX (0xff7fffff). If nothing is found, return -1.
 *   3. Build the chosen render-state (FUN_0018bc60, force_rebuild=1) and store
 *      the index at object_data+0x120.
 * cdecl: object_handle [EBP+8], lod [EBP+0xc] (float, forwarded bitwise to
 * FUN_0018bc60). Pool is *(data_t**)0x50652c (cached object render states).
 * Confirmed: 0x2533c0 = 0.0f, 0x254cb8 = 1000.0f, -FLT_MAX init = 0xff7fffff.
 */
int FUN_0018bf80(int object_handle, float lod)
{
  char *obj;
  int existing_index;
  char *datum;
  int new_index;
  int iter_index;
  float best_age;
  float age;

  obj = (char *)object_get_and_verify_type(object_handle, 0xffffffff);
  existing_index = *(int *)(obj + 0x120);
  if (existing_index != -1) {
    datum = (char *)datum_get(*(data_t **)0x50652c, existing_index);
    if (*(int *)(datum + 4) == object_handle) {
      existing_index = *(int *)(obj + 0x120);
      if (existing_index != -1) {
        /* Original loads the datum index into EAX (MOV EAX,ESI @ 0x18c091)
         * — bc60 takes it as a hidden @<eax> arg the old decl dropped. */
        FUN_0018bc60(existing_index, object_handle, lod, 0);
        return existing_index;
      }
    }
  }

  new_index = data_new_at_index(*(data_t **)0x50652c);
  if (new_index == -1) {
    best_age = -3.4028235e+38f;
    for (iter_index = data_next_index(*(data_t **)0x50652c, 0xffffffff);
         iter_index != -1;
         iter_index = data_next_index(*(data_t **)0x50652c, iter_index)) {
      datum = (char *)datum_get(*(data_t **)0x50652c, iter_index);
      age = (float)(*(int *)0x506544 - *(int *)(datum + 0xc));
      if (age < *(float *)0x2533c0) {
        age = *(float *)0x254cb8;
      }
      if (best_age < age) {
        new_index = iter_index;
        best_age = age;
      }
    }
    if (new_index == -1) {
      return -1;
    }
  }

  /* @<eax> = the newly allocated/evicted slot (MOV EAX,EDI @ 0x18c06e). */
  FUN_0018bc60(new_index, object_handle, lod, 1);
  *(int *)(obj + 0x120) = new_index;
  return new_index;
}

/* 0x18c0b0 — fetch an object's cached cluster-lighting datum data pointer.
 * Resolves the object's cluster-lighting record index via FUN_0018bf80
 * (object_handle, lod). On success returns the datum's data + 0x14 from the
 * cluster-lighting pool (*(data_t**)0x50652c). On the NONE (-1) path, falls
 * back to the global lighting-state buffer at 0x4d8258: rebuilds its baseline
 * lighting (FUN_0013bce0) and its dynamic light markers (FUN_0013aa10), then
 * returns &DAT_004d8258. cdecl: object_handle [EBP+8], lod [EBP+0xc] (float,
 * forwarded bitwise to FUN_0018bf80). NOTE: kb name 'scenario_leaf_index_from
 * _point' appears to be a misnomer (object-handle arg, pointer return). */
void *scenario_leaf_index_from_point(int object_handle, float lod)
{
  int index;

  index = FUN_0018bf80(object_handle, lod);
  if (index != -1) {
    return (void *)((int)datum_get(*(data_t **)0x50652c, index) + 0x14);
  }
  FUN_0013bce0(object_handle, (float *)0x4d8258);
  FUN_0013aa10(object_handle, 0x4d8258);
  return (void *)0x4d8258;
}

/* 0x18c3a0 — scenario_test_pvs: per-frame potentially-visible-set debug draw.
 * Optionally brackets the work in a 'render_objects' profile section (when both
 * the global debug-enable byte 0x449ef1 and the section-enable byte 0x325810
 * are set). Resets the sprite/decal draw state (FUN_0017d1a0(0)) and primes the
 * object iteration list (FUN_0018b080), then runs the body loop exactly twice:
 * the first pass (BL toggle 0->1) and the second. On the pass whose toggle byte
 * BL equals the global char at 0x3256c6 it walks the visible-object table at
 * 0x4d82d4 (count is the signed short at 0x4d82d0), seeding a 0x48-byte stack
 * record per entry (record[0] = table[i]) and processing it via FUN_0018c100
 * (record passed in EDI); on the other pass it draws first-person weapons
 * (first_person_weapon_draw). Finishes via FUN_0016b240 (reloc-verified
 * against the delinked reference; an earlier draft wrongly called
 * FUN_0017cbf0 here). void(void). */
void scenario_test_pvs(void)
{
  char pass;
  short i;
  bool again;
  char record[0x48];

  if (*(char *)0x449ef1 != 0 && *(char *)0x325810 != 0) {
    profile_enter_private((void *)0x325808);
  }
  FUN_0017d1a0(0);
  FUN_0018b080();
  pass = 0;
  record[8] = 0;
  do {
    if (pass == *(char *)0x3256c6) {
      i = 0;
      if (0 < *(short *)0x4d82d0) {
        do {
          *(int *)record = *(int *)(0x4d82d4 + (int)i * 4);
          FUN_0018c100(record);
          i = i + 1;
        } while (i < *(short *)0x4d82d0);
      }
    } else {
      first_person_weapon_draw();
    }
    again = (pass == 0);
    pass = 1;
  } while (again);
  FUN_0016b240();
  if (*(char *)0x449ef1 != 0 && *(char *)0x325810 != 0) {
    profile_exit_private((void *)0x325808);
  }
}

/* 0x18c460 — scenario_test_pas: per-frame potentially-audible-set debug draw.
 * Optionally brackets the work in a 'render_object_shadows' profile section
 * (when both the global debug-enable byte 0x449ef1 and the section-enable byte
 * 0x325e08 are set). When the master enable byte 0x325800 is set, it brackets
 * a sky/object iteration pass between FUN_00172520 and FUN_00172720
 * (reloc-verified against the delinked reference; an earlier draft wrongly
 * called the decal helpers FUN_0017cca0/FUN_0017cd10 here): a 0x48-byte stack
 * record is primed (record[8] = 1) and passed in EAX to scenario_get_sky,
 * which drives the per-object iteration loop and processing internally.
 * void(void). */
void scenario_test_pas(void)
{
  char record[0x48];

  if (*(char *)0x449ef1 != 0 && *(char *)0x325e08 != 0) {
    profile_enter_private((void *)0x325e00);
  }
  if (*(char *)0x325800 != 0) {
    FUN_00172520();
    record[8] = 1;
    scenario_get_sky(record);
    rasterizer_window_get_fog();
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x325e08 != 0) {
    profile_exit_private((void *)0x325e00);
  }
}

/* 0x18c580 — 3-key record comparator: compares two records by signed int16 at
 * +2, then signed int16 at +4, then unsigned byte at +6; returns the first
 * non-zero difference (qsort/bsearch-style ordering). */
int FUN_0018c580(int param_1, int param_2)
{
  int diff;

  diff = (int)*(short *)(param_1 + 2) - (int)*(short *)(param_2 + 2);
  if (diff == 0) {
    diff = (int)*(short *)(param_1 + 4) - (int)*(short *)(param_2 + 4);
    if (diff == 0)
      diff = (int)*(unsigned char *)(param_1 + 6) -
             (int)*(unsigned char *)(param_2 + 6);
  }
  return diff;
}

/* 0x18ca40 — render the sky model (render\render_sky.c, assert line 0x26).
 * Gated on render.visible_sky_model (0x506789); the sky record comes from
 * FUN_0018e7d0(render.visible_sky_index @0x50678a). Decompresses the base
 * node pose from the 'mode' tag (FUN_00123aa0), advances each overlay
 * animation (sky+0xb8 block, 0x24/elem): phase accumulators live in the
 * static array 0x4d86d8, stepped by frame-seconds (0x50654c) / period
 * (elem+0x4) mod 1.0 (double 0x2573d8), applied via FUN_00122690 when the
 * 'antr' sequence's node count matches the model. Builds world node
 * matrices (FUN_00123c70 from the camera basis ptrs 0x31fc1c/3c/44), then
 * for each sky light (sky+0xc4 block, 0x74/elem): direction from the named
 * model marker's world position minus the camera (0x506550) — the marker
 * query fills a 0x6c record whose +0x60 position the original read through
 * overlapping locals — or from the euler angles at elem+0x68 when the
 * marker name is empty; the light is placed far along that direction
 * (0x2b1b50) facing back (perpendicular basis), FUN_00139b40. Finally the
 * node matrices are pulled into a scaled view space (scale 2^-10, position
 * * 0x2b1b4c), FUN_0017d1a0(1) selects the sky rasterizer mode, and the
 * model is drawn with unit region scales via the 13-arg FUN_00123ed0,
 * flushed through FUN_0016b240 (the 0x17cbf0 thunk's target, matching the
 * scenario_test_pvs reloc lesson). cdecl, void(void), 0x1658-byte frame via
 * _chkstk. */
void FUN_0018ca40(void)
{
  float node_matrices[832]; /* EBP-0x1658: 64 x 0x34-byte node matrices */
  float node_buf[512];      /* EBP-0x958: 64 x 0x20-byte node transforms */
  char record[0x74];        /* EBP-0x158 render-model record */
  float scales[8];          /* EBP-0xE4 per-region scales */
  float view_matrix[13];    /* EBP-0xC4 */
  char marker[0x6c];        /* EBP-0x90; pos2/negdir/perp overlapped its
                             * dead world matrix in the original frame */
  float pos2[3];
  float negdir[3];
  float perp[3];
  float delta[3];
  float phase;
  float frame;
  float len;
  float *defcol;
  char *rec;
  char *mode_tag;
  char *antr_tag;
  char *elem;
  char *seq;
  int counter;
  int i;

  if (*(char *)0x506789 != 0) {
    if (FUN_0018e7d0((int)*(uint16_t *)0x50678a) == NULL) {
      display_assert("!render.visible_sky_model || "
                     "scenario_get_sky(render.visible_sky_index)",
                     "c:\\halo\\SOURCE\\render\\render_sky.c", 0x26, 1);
      system_exit(-1);
    }
    if (*(char *)0x506789 != 0) {
      rec = (char *)FUN_0018e7d0((int)*(uint16_t *)0x50678a);
      mode_tag = (char *)tag_get(0x6d6f6465, *(int *)(rec + 0xc));
      FUN_00123aa0(mode_tag, node_buf);
      if (*(int *)(rec + 0x1c) != -1) {
        antr_tag = (char *)tag_get(0x616e7472, *(int *)(rec + 0x1c));
        if (*(int *)(rec + 0xb8) > 0) {
          counter = 0;
          i = 0;
          do {
            elem = (char *)tag_block_get_element(rec + 0xb8, i, 0x24);
            if (*(int16_t *)elem >= 0 &&
                (int)*(int16_t *)elem < *(int *)(antr_tag + 0x74) &&
                *(float *)(elem + 4) != *(const float *)0x2533c0) {
              seq = (char *)tag_block_get_element(
                antr_tag + 0x74, (int)*(int16_t *)elem, 0xb4);
              if ((int)*(int16_t *)(seq + 0x2c) ==
                  *(int *)(mode_tag + 0xb8)) {
#if defined(_MSC_VER) && !defined(__clang__)
                /* VC71 /Oi lowers fmod to _CIfmod, matching the original;
                 * clang takes the x87_fmod (FPREM) branch below. */
                phase = (float)fmod(
                  (double)(*(float *)0x50654c / *(float *)(elem + 4) +
                           ((float *)0x4d86d8)[i]),
                  *(const double *)0x2573d8);
#else
                phase =
                  x87_fmod(*(float *)0x50654c / *(float *)(elem + 4) +
                             ((float *)0x4d86d8)[i],
                           *(const double *)0x2573d8);
#endif
                ((float *)0x4d86d8)[i] = phase;
                frame = (float)(int)*(int16_t *)(seq + 0x22) * phase;
                FUN_00122690(seq, frame, node_buf);
              }
            }
            counter++;
            i = (int)(int16_t)counter;
          } while (i < *(int *)(rec + 0xb8));
        }
      }
      FUN_00123c70(mode_tag, node_matrices, node_buf, *(float **)0x31fc1c,
                   *(float **)0x31fc3c, *(float **)0x31fc44);
      if (*(int *)(rec + 0xac) > 0) {
        counter = 0;
        i = 0;
        do {
          tag_block_get_element(rec + 0xac, i, 0x24); /* validation only */
          counter++;
          scales[i] = 1.0f;
          i = (int)(int16_t)counter;
        } while (i < *(int *)(rec + 0xac));
      }
      if (*(int *)(rec + 0xc4) > 0) {
        counter = 0;
        i = 0;
        do {
          elem = (char *)tag_block_get_element(rec + 0xc4, i, 0x74);
          if (*(int *)(elem + 0xc) != -1) {
            if (csstrlen(elem + 0x10) == 0) {
              angles_to_vector(delta, (float *)(elem + 0x68));
            } else {
              if (FUN_00124730(*(int *)(rec + 0xc), elem + 0x10, 0, 0, -1,
                               node_matrices, 0, marker, 1) == 0)
                goto next_light;
              delta[0] = *(float *)(marker + 0x60) - *(float *)0x506550;
              delta[1] = *(float *)(marker + 0x64) - *(float *)0x506554;
              delta[2] = *(float *)(marker + 0x68) - *(float *)0x506558;
              len = sqrtf(delta[2] * delta[2] + delta[0] * delta[0] +
                          delta[1] * delta[1]);
              if (fabs((double)len) >= *(const double *)0x2533d0) {
                len = *(const float *)0x2533c8 / len;
                delta[0] = delta[0] * len;
                delta[1] = delta[1] * len;
                delta[2] = delta[2] * len;
              }
            }
            pos2[0] =
              delta[0] * *(const float *)0x2b1b50 + *(float *)0x506550;
            pos2[1] =
              delta[1] * *(const float *)0x2b1b50 + *(float *)0x506554;
            pos2[2] =
              delta[2] * *(const float *)0x2b1b50 + *(float *)0x506558;
            negdir[0] = -delta[0];
            negdir[1] = -delta[1];
            negdir[2] = -delta[2];
            perpendicular3d(negdir, perp);
            len = sqrtf(perp[2] * perp[2] + perp[1] * perp[1] +
                        perp[0] * perp[0]);
            if (fabs((double)len) >= *(const double *)0x2533d0) {
              len = *(const float *)0x2533c8 / len;
              perp[0] = perp[0] * len;
              perp[1] = perp[1] * len;
              perp[2] = perp[2] * len;
            }
            FUN_00139b40(*(int *)(elem + 0xc), (int *)pos2, (int)negdir,
                         (int)perp, *(float **)0x2ee708, 1.0f);
          }
        next_light:
          counter++;
          i = (int)(int16_t)counter;
        } while (i < *(int *)(rec + 0xc4));
      }
      qmemcpy(view_matrix, *(void **)0x31fc60, 0x34);
      view_matrix[10] = *(float *)0x506550 * *(const float *)0x2b1b4c;
      view_matrix[11] = *(float *)0x506554 * *(const float *)0x2b1b4c;
      view_matrix[12] = *(float *)0x506558 * *(const float *)0x2b1b4c;
      view_matrix[0] = 0.0009765625f; /* 2^-10, immediate store */
      if (*(int *)(mode_tag + 0xb8) > 0) {
        counter = 0;
        i = 0;
        do {
          matrix4x3_multiply(view_matrix, node_matrices + i * 13,
                             node_matrices + i * 13); /* dup-args-ok */
          counter++;
          i = (int)(int16_t)counter;
        } while (i < *(int *)(mode_tag + 0xb8));
      }
      FUN_0017d1a0(1);
      csmemset(record, 0, 0x74);
      defcol = *(float **)0x2ee708;
      *(int32_t *)record = *(int32_t *)defcol;
      *(int32_t *)(record + 4) = *(int32_t *)(defcol + 1);
      *(int32_t *)(record + 8) = *(int32_t *)(defcol + 2);
      FUN_00123ed0(*(int *)(rec + 0xc), 0.0f, node_matrices, 0, 0, scales,
                   (int)record, (void *)0x506550, 0, 0, 0, 0, 1);
      FUN_0016b240();
    }
  }
}

/* Sort record built by FUN_0018c5b0's gather pass; FUN_0018c580 is the
 * matching qsort comparator (tag, then sort key, then first-person flag). */
typedef struct particle_sort_record {
  int16_t datum_index;
  int16_t tag_index;
  int16_t sort_key;
  char first_person;
  char pad;
} particle_sort_record_t;

/* 0x18c5b0 — particles_render: gather -> sort -> batched sprite emission.
 * Bracketed by profile_enter/exit_private(0x3263f8) when profiling is on
 * (0x449ef1 && 0x326400); gated on the particles-enabled byte 0x32574b.
 * Pass 1 walks the particle pool (0x5aa8a0) and gathers renderable
 * particles: the location (particle+0x28) must be in a visible cluster and
 * the first-person filter bits honored (+0x2 bit4 = hide in first person,
 * bit5 = only in first person; the local player's first-person state comes
 * from 0x506548 via FUN_0018c4d0, falling back to window index 4). Records
 * hold {datum, definition tag index (+0x4), sort key (+0x2c), fp flag}.
 * Pass 2 qsorts (CRT qsort, comparator FUN_0018c580) and run-length encodes
 * runs of identical (tag, sort, fp) into up to 0x200 counts. Pass 3 builds
 * one sprite batch per run: FUN_0018d2c0 begin (shader from 'part' tag+0x10,
 * geometry tag+0xb0, flags 2 for first-person), per particle resolve the
 * world position/direction — already-detached particles (+0x8 == -1) copy
 * +0x30/+0x3c directly (with the original's redundant -1 self-store);
 * attached ones transform through the first-person weapon node matrix
 * (flags bit6) or the object node matrix (deleting the particle when the
 * object is gone) — cull by projected pixel diameter vs tag+0x90, scale by
 * near-fade (tag+0x68), birth ramp (tag+0x40) and death ramp (tag+0x44),
 * then FUN_0018d6e0. The batch ends by storing the average particle radius
 * into the animation record (+0x8 -> +0x98, divides by the emitted count
 * even when zero, matching the original) and FUN_0018d360. cdecl,
 * void(void), 0x24e8-byte frame via _chkstk. */
void FUN_0018c5b0(void)
{
  particle_sort_record_t gather[0x400]; /* EBP-0x24E8 */
  int16_t runs[0x200];                  /* EBP-0x4E8 */
  char record[0xa4];                    /* EBP-0xE8 build_sprites record */
  float direction[3];                   /* EBP-0x44 */
  float position[3];                    /* EBP-0x38 */
  float life_delta;                     /* EBP-0x2C */
  float radius;                         /* EBP-0x28 */
  float radius_accum;                   /* EBP-0x24 */
  float pixels;                         /* EBP-0x20 */
  float size;
  float scale;
  int emitted;                          /* EBP-0x1C */
  int count;                            /* EBP-0x4 */
  int fp_raw;
  int fp_index;
  int index;
  int inner;
  int remaining_runs;
  int16_t run_count;
  int16_t prev_tag;
  int16_t prev_sort;
  int16_t tag_index;
  char prev_fp;                         /* EBP-0x5 */
  char is_fp;
  char *part;
  char *tag;
  float *marker;
  int16_t *runp;
  particle_sort_record_t *rec;
  uint32_t flags3;

  count = 0;
  if (*(char *)0x449ef1 != 0 && *(char *)0x326400 != 0) {
    profile_enter_private((void *)0x3263f8);
  }
  if (*(char *)0x32574b != 0) {
    fp_raw = *(int *)0x506548;
    if ((int16_t)fp_raw == -1 || FUN_0018c4d0((int16_t)fp_raw) == 0) {
      fp_raw = 4;
    }
    index = data_next_index(*(data_t **)0x5aa8a0, -1);
    if (index != -1) {
      fp_index = (int)(int16_t)fp_raw;
      do {
        part = (char *)datum_get(*(data_t **)0x5aa8a0, index);
        is_fp = (int)*(uint8_t *)(part + 0xf) == fp_index;
        if (render_location_visible(part + 0x28) != 0 &&
            ((*(uint8_t *)(part + 2) & 0x10) == 0 || !is_fp) &&
            ((*(uint8_t *)(part + 2) & 0x20) == 0 || is_fp)) {
          rec = &gather[(int16_t)count];
          count++;
          rec->datum_index = (int16_t)index;
          rec->tag_index = *(int16_t *)(part + 4);
          rec->sort_key = *(int16_t *)(part + 0x2c);
          if (is_fp && (*(uint8_t *)(part + 2) & 0x20) != 0) {
            rec->first_person = 1;
          } else {
            rec->first_person = 0;
          }
        }
        index = data_next_index(*(data_t **)0x5aa8a0, index);
      } while (index != -1);
      if ((int16_t)count > 0) {
        qsort(gather, (size_t)(int16_t)count, 8,
              (int (*)(const void *, const void *))FUN_0018c580);
        prev_tag = -1;
        prev_sort = -1;
        prev_fp = 0;
        run_count = 0;
        runp = 0;
        rec = gather;
        do {
          count--;
          tag_index = rec->tag_index;
          if (tag_index == prev_tag && rec->sort_key == prev_sort &&
              rec->first_person == prev_fp) {
            *runp = *runp + 1;
          } else {
            if (run_count >= 0x200)
              break;
            prev_tag = tag_index;
            prev_sort = rec->sort_key;
            prev_fp = rec->first_person;
            runp = &runs[run_count];
            run_count++;
            *runp = 1;
          }
          rec++;
        } while ((int16_t)count > 0);
        rec = gather;
        if (run_count > 0) {
          runp = runs;
          remaining_runs = (int)(uint16_t)run_count;
          do {
            tag = (char *)tag_get(0x70617274, (int)rec->tag_index);
            radius_accum = 0.0f;
            emitted = 0;
            FUN_0018d2c0((uint32_t *)record, *runp, *(uint32_t *)(tag + 0x10),
                         (int)(tag + 0xb0),
                         rec->first_person != 0 ? 2u : 0u);
            if (*runp > 0) {
              inner = (int)(uint16_t)*runp;
              do {
                part = (char *)datum_get(*(data_t **)0x5aa8a0,
                                         (int)rec->datum_index);
                radius = particle_get_radius((int)rec->datum_index);
                if (*(int *)(part + 8) == -1) {
                  *(int32_t *)position = *(int32_t *)(part + 0x30);
                  *(int32_t *)(position + 1) = *(int32_t *)(part + 0x34);
                  *(int32_t *)(position + 2) = *(int32_t *)(part + 0x38);
                  *(int32_t *)direction = *(int32_t *)(part + 0x3c);
                  *(int32_t *)(direction + 1) = *(int32_t *)(part + 0x40);
                  *(int32_t *)(direction + 2) = *(int32_t *)(part + 0x44);
                  /* redundant self-store kept from the original */
                  *(int *)(part + 8) = -1;
                } else {
                  if ((*(uint8_t *)(part + 2) & 0x40) != 0) {
                    marker = (float *)first_person_weapon_get_node_matrix(
                      (int)*(uint8_t *)(part + 0xf),
                      (int)*(uint16_t *)(part + 0xc));
                  } else {
                    if (object_try_and_get_and_verify_type(
                          *(int *)(part + 8), -1) == NULL) {
                      particle_delete((int)rec->datum_index);
                      goto next_particle;
                    }
                    marker = (float *)object_get_node_matrix(
                      *(int *)(part + 8), *(int16_t *)(part + 0xc));
                  }
                  matrix_transform_point(marker, (float *)(part + 0x30),
                                         position);
                  matrix_transform_vector(marker, (float *)(part + 0x3c),
                                          direction);
                }
                pixels = render_frustum_sphere_diameter_in_pixels(
                  (void *)0x5065a4, position, radius);
                if (pixels > *(float *)(tag + 0x90)) {
                  emitted++;
                  scale = 1.0f;
                  size = (radius + radius) * *(float *)(tag + 0xa8);
                  life_delta =
                    *(float *)(part + 0x18) - *(float *)(part + 0x14);
                  radius_accum = radius + radius_accum;
                  if (pixels < *(float *)(tag + 0x68)) {
                    size = (*(float *)(tag + 0x68) / pixels) * size;
                  }
                  if (*(float *)(tag + 0x40) > *(const float *)0x2533c0 &&
                      *(float *)(part + 0x14) < *(float *)(tag + 0x40)) {
                    scale = *(float *)(part + 0x14) / *(float *)(tag + 0x40);
                  }
                  if (*(float *)(tag + 0x44) > *(const float *)0x2533c0 &&
                      life_delta < *(float *)(tag + 0x44)) {
                    scale = (life_delta / *(float *)(tag + 0x44)) * scale;
                  }
                  flags3 = (uint32_t)((*(uint8_t *)(part + 2) >> 1) & 2);
                  if ((*(uint8_t *)(part + 2) & 8) != 0) {
                    flags3 |= 4;
                  } else {
                    flags3 &= ~4u; /* redundant AND kept from the original */
                  }
                  FUN_0018d6e0(record, (int16_t)*(uint16_t *)(tag + 0xac),
                               (int16_t)*(uint16_t *)(part + 0x24),
                               (int16_t)*(uint16_t *)(part + 0x26), position,
                               direction, *(float *)(part + 0x54), size,
                               (float *)(part + 0x60), scale, flags3);
                  *(int *)(part + 0x10) = *(int *)0x506540;
                }
              next_particle:
                rec++;
                inner--;
              } while (inner != 0);
            }
            /* average radius into the animation record — divides by the
             * emitted count even when it is zero, as the original does */
            *(float *)(*(int *)(record + 8) + 0x98) =
              radius_accum / (float)(int16_t)emitted;
            FUN_0018d360(record);
            runp++;
            remaining_runs--;
          } while (remaining_runs != 0);
        }
      }
    }
  }
  if (*(char *)0x449ef1 != 0 && *(char *)0x326400 != 0) {
    profile_exit_private((void *)0x3263f8);
  }
}

/* 0x18cf10 — render_sprite.c transform helper (asserts at lines 0x4a-0x54):
 * transform a sprite's untransformed origin/direction into view space.
 * data->flags (+0x10) bit0 = screen-space sprite: run
 * render_camera_screen_to_view into a scratch vector (the result is
 * discarded; MSVC placed the scratch in the dead param slots) and
 * hard-assert that no direction was supplied. Otherwise the `flags` byte
 * bit0 selects raw copy (already view-space) vs
 * matrix_transform_point/_vector through the view matrix at 0x5065b4.
 * direction is optional (NULL skips). Register args: data @<ebx>,
 * untransformed_origin @<esi>, untransformed_direction @<edi>. */
void FUN_0018cf10(void *data, float *untransformed_origin,
                  float *untransformed_direction, uint8_t flags,
                  float *transformed_origin, float *transformed_direction)
{
  float view_vector[3];

  if (data == NULL) {
    display_assert("data", "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x4a,
                   1);
    system_exit(-1);
  }
  if (untransformed_origin == NULL) {
    display_assert("untransformed_origin",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x4b, 1);
    system_exit(-1);
  }
  if (transformed_origin == NULL) {
    display_assert("transformed_origin",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x4c, 1);
    system_exit(-1);
  }
  if (transformed_direction == NULL) {
    display_assert("transformed_direction",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x4d, 1);
    system_exit(-1);
  }
  if ((*(uint8_t *)((char *)data + 0x10) & 1) != 0) {
    render_camera_screen_to_view((void *)0x506550, (void *)0x5065a4,
                                 untransformed_origin, view_vector);
    if (untransformed_direction != NULL) {
      display_assert("!untransformed_direction",
                     "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x54, 1);
      system_exit(-1);
    }
  } else if ((flags & 1) != 0) {
    *(int32_t *)transformed_origin = *(int32_t *)untransformed_origin;
    *(int32_t *)(transformed_origin + 1) =
      *(int32_t *)(untransformed_origin + 1);
    *(int32_t *)(transformed_origin + 2) =
      *(int32_t *)(untransformed_origin + 2);
    if (untransformed_direction != NULL) {
      *(int32_t *)transformed_direction =
        *(int32_t *)untransformed_direction;
      *(int32_t *)(transformed_direction + 1) =
        *(int32_t *)(untransformed_direction + 1);
      *(int32_t *)(transformed_direction + 2) =
        *(int32_t *)(untransformed_direction + 2);
    }
  } else {
    matrix_transform_point((float *)0x5065b4, untransformed_origin,
                           transformed_origin);
    if (untransformed_direction != NULL) {
      matrix_transform_vector((float *)0x5065b4, untransformed_direction,
                              transformed_direction);
    }
  }
}

/* 0x18d140 — render_sprite.c: find or allocate the sprite group slot for a
 * bitmap. data (@<esi>) is the build_sprites record: +0x4 sprite capacity
 * (short), +0x10 flags (bit0 = screen-space -> vertex type 8 else 6), +0x20
 * group count (short), +0x24 group array (8 entries x 0x10: +0x0 vertex
 * buffer handle, +0x4 vertices pointer, +0x8 used count, +0xc bitmap).
 * Scans for an existing group with this bitmap; if absent, asserts the 8-slot
 * cap (render_sprite.c:0x113, csprintf into the 0x5ab100 scratch), then
 * allocates: registers the bitmap with the texture cache and creates a
 * dynamic vertex buffer (capacity*4 vertices) with the pointer resolved via
 * 0x17c9d0. Returns the group index, or -1 when the table is full or the
 * group has no vertices (+0x28 NULL). Global 0x325652 (short) brackets the
 * allocation; 0x4d86f8 is the warn-once latch. */
int16_t FUN_0018d140(void *data, int bitmap)
{
  int16_t count;
  int16_t index;
  char *group;
  void *fmt;
  int vb;
  int verts;

  count = *(int16_t *)((char *)data + 0x20);
  index = 0;
  if (count > 0) {
    do {
      if (*(int *)((char *)data + ((int)index + 3) * 0x10) == bitmap)
        break;
      index++;
    } while (index < *(int16_t *)((char *)data + 0x20));
  }
  if (index >= count && count > 7) {
    display_assert(
      csprintf((char *)0x5ab100,
               "a build_sprites_begin call can accomodate at most %d bitmaps",
               8),
      "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x113, 1);
    system_exit(-1);
  }
  count = *(int16_t *)((char *)data + 0x20);
  if (index >= count) {
    if (count > 7)
      return -1;
    if (index >= count) {
      group = (char *)data + (int)index * 0x10 + 0x24;
      *(int16_t *)((char *)data + 0x20) = count + 1;
      *(int *)(group + 0xc) = bitmap;
      fmt = xbox_texture_cache_get_hardware_format((void *)bitmap, 0, 1);
      if (fmt != NULL) {
        *(uint16_t *)0x325652 = 0x10;
        vb = rasterizer_widget_set_zbuffer_enable(
          (*(uint32_t *)((char *)data + 0x10) & 1) != 0 ? 8 : 6,
          (int)*(int16_t *)((char *)data + 4) << 2);
        *(int *)group = vb;
        if (vb != -1) {
          verts = rasterizer_widget_draw_sprite3d(vb);
          *(int *)(group + 4) = verts;
          if (verts == 0) {
            display_assert("group->vertices",
                           "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x126,
                           1);
            system_exit(-1);
            /* the original assert path stores its own bracket-close then
             * jumps past the shared store */
            *(uint16_t *)0x325652 = 0;
            goto reset_count;
          }
        } else {
          if (*(char *)0x4d86f8 == 0) {
            error(2, "build_sprite failed to allocate dynamic vertices");
            *(char *)0x4d86f8 = 1;
          }
          *(int *)(group + 4) = 0;
        }
        *(uint16_t *)0x325652 = 0;
      } else {
        *(int *)(group + 4) = 0;
      }
    reset_count:
      *(int16_t *)(group + 8) = 0;
    }
  }
  if (index != -1 && *(int *)((char *)data + (int)index * 0x10 + 0x28) == 0)
    return -1;
  return index;
}

/* 0x18d040 — resolve a sprite record's default scale then apply the
 * per-record count multiplier (kb name scenario_object_name_index_from_string
 * was a misattribution; the body is float scale math, no string handling).
 * state+0x10 bit0 selects the mode: when set, a sentinel scale
 * (*value == *(float *)0x2533c0) becomes 1.0 before the multiply; when clear
 * and p1 == 0, the sentinel becomes -(src+0x8 / global 0x506728). Finally
 * *value *= (short)(info+0x4). p2 is on the stack but unused (kept for ABI).
 * Register args: state @<eax>, value @<ecx>. Only caller: FUN_0018d6e0
 * (build_sprite). */
void FUN_0018d040(void *state, float *value, int16_t p1, int p2, void *src,
                  void *info)
{
  if ((*(uint8_t *)((char *)state + 0x10) & 1) != 0) {
    if (*value == *(const float *)0x2533c0) {
      *value = 1.0f;
      *value = (float)*(int16_t *)((char *)info + 4) * *value;
      return;
    }
  } else if (p1 == 0 && *value == *(const float *)0x2533c0) {
    *value = -(*(float *)((char *)src + 8) / *(float *)0x506728);
  }
  *value = (float)*(int16_t *)((char *)info + 4) * *value;
}

/* 0x18d0b0 — per-frame coverage/big-sprite stats reset (kb name is a
 * misnomer). If the debug flag at 0x5064e8 is set, format the accumulated
 * coverage (float @0x506504) and big-sprite count (short @0x506508) into a
 * line and emit it via FUN_00189c40(0, ...). Then zero both accumulators and
 * run two matrix_transform_vector passes over the 0x5065b4 matrix using the
 * pointers at *0x31fc44 / *0x31fc40 into the 0x506510 / 0x50651c buffers.
 * cdecl, void(void). */
void scenario_fog_region_get_fog_index(void)
{
  char line[512];

  if (*(char *)0x5064e8 != 0) {
    crt_sprintf(line, "   coverage: %.1f big sprites: %d",
                (double)*(float *)0x506504, (int)*(short *)0x506508);
    FUN_00189c40(0, line);
  }
  *(float *)0x506504 = 0.0f;
  *(short *)0x506508 = 0;
  matrix_transform_vector((float *)0x5065b4, *(float **)0x31fc44,
                          (float *)0x506510);
  matrix_transform_vector((float *)0x5065b4, *(float **)0x31fc40,
                          (float *)0x50651c);
}

/* 0x18d2c0 — render_sprite builder init: populate a sprite-build record
 * (param_1) from a shader/geometry source. Asserts the shader pointer is
 * non-NULL (render_sprite.c:0x14f) and that the _build_sprites_valid_bit
 * (flags & 4) is clear on entry (render_sprite.c:0x150). Stores fields then
 * snapshots the current view origin from *0x31fc1c (3 dwords) into
 * +0x14..+0x1c, and finally re-sets +0x10 to flags | _build_sprites_valid_bit.
 * The +0x10 store happens twice (param_5 then param_5|4), matching the
 * original. cdecl. */
void FUN_0018d2c0(uint32_t *param_1, int16_t param_2, uint32_t param_3,
                  int param_4, uint32_t param_5)
{
  uint32_t *view;

  if (param_4 == 0) {
    display_assert("shader", "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x14f,
                   1);
    system_exit(-1);
  }
  if ((param_5 & 4) != 0) {
    display_assert("!TEST_FLAG(flags, _build_sprites_valid_bit)",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x150, 1);
    system_exit(-1);
  }
  param_1[0] = param_3; /* +0x00 */
  param_1[4] = param_5; /* +0x10 (overwritten below) */
  param_1[2] = (uint32_t)param_4; /* +0x08 */
  *(int16_t *)((char *)param_1 + 0x20) = 0;
  *(int16_t *)((char *)param_1 + 0x0c) = 0;
  *(int16_t *)((char *)param_1 + 0x04) = param_2;
  view = *(uint32_t **)0x31fc1c;
  param_1[5] = view[0]; /* +0x14 */
  param_1[6] = view[1]; /* +0x18 */
  param_1[4] = param_5 | 4; /* +0x10 set _build_sprites_valid_bit */
  param_1[7] = view[2]; /* +0x1c */
}

/* 0x18d360 — render_sprite emit (render/render_sprite.c). Scales the sprite-
 * build record's cached origin by (global 0x2533c8 / count) [reversed divide:
 * the int16 at +0xc is the divisor, the global is the numerator], transforms it
 * into screen space in-place, then walks the per-sprite record array handing
 * each live entry to the rasterizer widget path (widget_end -> emit ->
 * submit_occlusion_test). Asserts _build_sprites_valid_bit (flags&4) is set on
 * entry (render_sprite.c:0x166) and rejects screen-geometry sprites
 * (render_sprite.c:0x179). Clears _build_sprites_valid_bit on exit.
 *
 * data layout: +0x08 dword source; +0x0c int16 count/divisor; +0x10 dword flags
 * (bit2 = _build_sprites_valid_bit, bit0 = screen-geometry, bit1 -> widget flag
 * 0x800); +0x14..0x1c float3 origin; +0x20 int16 record count; +0x24 array of
 * 0x10-byte records {+0x0 dword handle, +0x8 int16 subcount, +0xc dword param}.
 *
 * cdecl, one stack arg. Load widths preserved: entry flag test is a byte read,
 * the exit clear is a dword AND; the counts at +0xc/+0x20 and each record's
 * +0x8 subcount are int16 (MOVSX). matrix_transform_point is called in-place
 * (src == dst), matching the original. system_exit(-1) (not
 * halt_and_catch_fire) follows each failed assert; the trailing cf80 call in
 * the screen-geometry branch is dead code after system_exit but is emitted by
 * the original. */
void FUN_0018d360(void *sprite_build_data)
{
  char *data;
  float scale;
  float *origin;
  int16_t i;
  uint32_t *rec;

  data = (char *)sprite_build_data;
  scale = *(float *)0x2533c8 / (float)(int)*(int16_t *)(data + 0xc);

  if ((*(uint8_t *)(data + 0x10) & 4) == 0) {
    display_assert("TEST_FLAG(data->flags, _build_sprites_valid_bit)",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x166, 1);
    system_exit(-1);
  }

  origin = (float *)(data + 0x14);
  origin[0] *= scale;
  origin[1] *= scale;
  origin[2] *= scale;
  matrix_transform_point((float *)0x5065e8, origin, origin);

  for (i = 0; i < *(int16_t *)(data + 0x20); i++) {
    rec = (uint32_t *)(data + 0x24 + (int)i * 0x10);
    if (*(int16_t *)((char *)rec + 0x8) != 0) {
      rasterizer_widget_end((int)rec[0]);
      if ((*(uint32_t *)(data + 0x10) & 1) != 0) {
        display_assert(
          "### ERROR sprites rendered with screen geometry -- tell Bernie!!",
          "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x179, 1);
        system_exit(-1);
        FUN_0017cf80(0, -4, rec[0], (int)*(int16_t *)((char *)rec + 0x8) << 1);
      } else {
        FUN_0017cf60(*(uint32_t *)(data + 8), rec[3], 0, -4, rec[0],
                     (int)*(int16_t *)((char *)rec + 0x8) << 1, origin,
                     ((*(uint32_t *)(data + 0x10) & 2) << 6) | 0x20);
      }
      rasterizer_widget_submit_occlusion_test((int)rec[0]);
    }
  }

  *(uint32_t *)(data + 0x10) &= ~4u;
}

/* 0x18d490 — render_sprite.c: build the 2-vector sprite basis for a sprite
 * type (asserts at lines 0x74-0x76, 0x7a, 0xac). basis receives right
 * (+0x4) and up (+0x10) vectors; +0x1c is scratch for the normalized
 * direction in type 2. Screen-space records (data+0x10 bit0) only allow
 * type 0 (no basis needed). Type 0: identity basis {1,0,0}/{0,1,0}.
 * Type 1 (oriented): right = normalize(direction), up =
 * cross(origin, right) normalized. Type 2 (parallel/beam): pick the view
 * forward (0x506510) or up (0x50651c) axis — whichever is less parallel to
 * the direction (threshold const 0x28ace8 vs squared dot) — then right =
 * cross(direction, axis) normalized, up = right rotated -90 deg around the
 * normalized direction. The original reuses the dead origin param slot for
 * the axis pointer and the up-vector pointer; the lift reassigns the origin
 * param to match. Register args: basis @<eax>, data @<ecx>,
 * direction @<ebx>. */
void FUN_0018d490(float *basis, void *data, float *direction,
                  int16_t sprite_type, int unused, float *origin)
{
  float dot;
  float dot_sq;

  if (data == NULL) {
    display_assert("data", "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x74,
                   1);
    system_exit(-1);
  }
  if (origin == NULL) {
    display_assert("origin", "c:\\halo\\SOURCE\\render\\render_sprite.c",
                   0x75, 1);
    system_exit(-1);
  }
  if (basis == NULL) {
    display_assert("basis", "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x76,
                   1);
    system_exit(-1);
  }
  if ((*(uint8_t *)((char *)data + 0x10) & 1) != 0) {
    if (sprite_type == 0)
      return;
    display_assert("build_sprite only supports normal sprites in screen "
                   "space.",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x7a, 1);
    system_exit(-1);
    return;
  }
  if (sprite_type == 0) {
    basis[1] = 1.0f;
    basis[2] = 0.0f;
    basis[3] = 0.0f;
    basis[4] = 0.0f;
    basis[5] = 1.0f;
    basis[6] = 0.0f;
    return;
  }
  if (sprite_type == 1) {
    *(int32_t *)(basis + 1) = *(int32_t *)direction;
    *(int32_t *)(basis + 2) = *(int32_t *)(direction + 1);
    *(int32_t *)(basis + 3) = *(int32_t *)(direction + 2);
    /* length results discarded (FSTP ST0 in the original) */
    normalize3d(basis + 1);
    cross_product3d(origin, basis + 1, basis + 4);
    normalize3d(basis + 4);
    return;
  }
  if (sprite_type == 2) {
    dot = *(float *)0x506514 * direction[1] +
          *(float *)0x506518 * direction[2] +
          *(float *)0x506510 * direction[0];
    dot_sq = dot * dot;
    origin = (float *)0x506510;
    if (FUN_00012170(direction) * *(const float *)0x28ace8 < dot_sq)
      origin = (float *)0x50651c;
    cross_product3d(direction, origin, basis + 1);
    normalize3d(basis + 1);
    origin = basis + 4;
    *(int32_t *)origin = *(int32_t *)(basis + 1);
    *(int32_t *)(origin + 1) = *(int32_t *)(basis + 2);
    *(int32_t *)(origin + 2) = *(int32_t *)(basis + 3);
    *(int32_t *)(basis + 7) = *(int32_t *)direction;
    *(int32_t *)(basis + 8) = *(int32_t *)(direction + 1);
    *(int32_t *)(basis + 9) = *(int32_t *)(direction + 2);
    normalize3d(basis + 7);
    rotate_vector3d_by_sincos(origin, basis + 7, -1.0f, 0.0f);
    return;
  }
  display_assert(0, "c:\\halo\\SOURCE\\render\\render_sprite.c", 0xac, 1);
  system_exit(-1);
}

/* 0x18d670 — projection-cosine helper (pure FPU leaf, no callees). For mode 0
 * returns 1.0f. For non-zero mode, returns the magnitude of the normalized
 * projection of v2 onto v1's direction: |dot(v1, v2) / |v1||. For mode 2 the
 * result is complemented as (1.0f - that). Used to score how aligned two
 * direction vectors are. NOTE: the kb name "scenario_reload_structure_bsp_if_
 * necessary" is a mis-attribution; this is a vector-angle helper.
 * cdecl: mode [EBP+8] (int16), v1 [EBP+0xc] (float*), v2 [EBP+0x10] (float*).
 * Returns float. Confirmed: 0x2533c8 = 1.0f; FSQRT over v1 only; FABS; FSUBR
 * for the mode-2 complement. */
float FUN_0018d670(short mode, float *v1, float *v2)
{
  float result;

  result = *(float *)0x2533c8;
  if (mode != 0) {
#if defined(_MSC_VER) && !defined(__clang__)
    /* VC71 /Oi inlines fabs((double)x) as the x87 FABS instruction, matching
     * the original's inline FABS on ST. clang (shipped build) takes the #else
     * branch below, so the binary is unchanged. Analog of lift-score-improve
     * technique 1 (cos/sin intrinsification) applied to fabsf. */
    result = (float)fabs(
      (double)((v1[0] * v2[0] + v2[1] * v1[1] + v2[2] * v1[2]) /
               sqrtf(v1[2] * v1[2] + v1[1] * v1[1] + v1[0] * v1[0])));
#else
    result = fabsf((v1[0] * v2[0] + v2[1] * v1[1] + v2[2] * v1[2]) /
                   sqrtf(v1[2] * v1[2] + v1[1] * v1[1] + v1[0] * v1[0]));
#endif
    if (mode == 2) {
      result = *(float *)0x2533c8 - result;
    }
  }
  return result;
}

/* 0x18d6e0 — build_sprite (render_sprite.c, asserts 0x1a2/0x1a3/0x1b3): emit
 * one 4-corner sprite into the build_sprites record. Resolves the 'bitm' tag,
 * validates sequence/sprite indices, finds/allocates the vertex group
 * (FUN_0018d140), transforms origin/direction into view space
 * (FUN_0018cf10), builds the 2-vector basis (FUN_0018d490), resolves the
 * default scale (FUN_0018d040 writes through the scale param slot), fades
 * intensity by the animation view-angle term (FUN_0018d670), packs the
 * 32-bit color (alpha from intensity or the fixed 0x2602c8 factor), then
 * writes 4 vertices (screen-space stride 0x14: x,y,u,v,color; world stride
 * 0x18: x,y,z,u,v,color with rotation, mirroring, and bounds tracking).
 * Afterwards accumulates the view origin, bumps counts, adds the screen
 * coverage fraction (render_frustum_cube_view_fraction) with the big-sprite
 * clamp (>10 oversized sprites get dropped again), and draws the debug quad
 * outline when 0x5064e8 is set. cdecl, 11 stack args. */
void FUN_0018d6e0(void *data, int16_t mode, int16_t sequence_index,
                  int16_t sprite_index, float *untransformed_origin,
                  float *untransformed_direction, float angle, float scale,
                  float *color, float intensity, uint32_t flags)
{
  float transformed_direction[3]; /* EBP-0xB0 */
  float basis[10];                /* EBP-0xA4; [1..3] right, [4..6] up,
                                   * [7..9] forward scratch */
  float dbg_b[3];                 /* EBP-0x70 */
  float dbg_d[3];                 /* EBP-0x64 */
  float dbg_c[3];                 /* EBP-0x58 */
  uint32_t mirror_y;              /* EBP-0x4C */
  float bounds[6];                /* EBP-0x48: minx,maxx,miny,maxy,minz,maxz */
  float sin_a;                    /* EBP-0x30 */
  float sin_dx;                   /* EBP-0x2C (after elem ptr dies) */
  float cos_a;                    /* EBP-0x28 */
  float transformed_origin[3];    /* EBP-0x24 */
  char *elem;                     /* EBP-0x18 */
  char *bitm;                     /* EBP-0x14, then group ptr */
  int vertex_index;               /* EBP-0x10 (read back as int16) */
  float wpoint[3];                /* EBP-0xC: world vertex / debug p0 */
  char *seq_elem;
  char *bitmap_elem;
  char *group;
  char *anim;
  uint32_t pixel;
  uint32_t final_color;
  uint32_t mirror_x;
  float alpha_f;
  float area;
  int16_t group_index;
  int16_t old_count;
  int corner;
  float u;
  float v;
  float dx;
  float dy;
  float rot_x;
  float rot_y;
  float *vtx;
  int verts;
  int idx;

  bitm = (char *)tag_get(0x6269746d, *(int *)data);
  if (untransformed_origin == NULL) {
    display_assert("untransformed_origin",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x1a2, 1);
    system_exit(-1);
  }
  if (mode != 0 &&
      (untransformed_direction == NULL ||
       untransformed_direction[0] * untransformed_direction[0] +
           untransformed_direction[1] * untransformed_direction[1] +
           untransformed_direction[2] * untransformed_direction[2] ==
         *(const float *)0x2533c0)) {
    display_assert("mode==_build_sprite_normal || (untransformed_direction "
                   "&& magnitude_squared3d(untransformed_direction))",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x1a3, 1);
    system_exit(-1);
  }
  if (color == NULL) {
    color = *(float **)0x2ee6c4;
  }
  if (*(int16_t *)((char *)data + 0xc) < *(int16_t *)((char *)data + 4)) {
    if (sequence_index >= 0 && (int)sequence_index < *(int *)(bitm + 0x54)) {
      seq_elem = (char *)tag_block_get_element(bitm + 0x54,
                                               (int)sequence_index, 0x40);
      if (*(int16_t *)(seq_elem + 0x20) != -1 && sprite_index >= 0 &&
          (int)sprite_index < *(int *)(seq_elem + 0x34)) {
        elem = (char *)tag_block_get_element(seq_elem + 0x34,
                                             (int)sprite_index, 0x20);
        if (*(int16_t *)elem == -1) {
          display_assert(
            csprintf((char *)0x5ab100,
                     "the bitmap group %s sequence %d sprite %d references "
                     "bitmap -1 (tell matt).",
                     tag_get_name(*(int *)data), sequence_index,
                     sprite_index),
            "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x1b3, 1);
          system_exit(-1);
        }
        bitmap_elem = (char *)tag_block_get_element(
          bitm + 0x60, (int)*(int16_t *)elem, 0x30);
        group_index = FUN_0018d140(data, (int)bitmap_elem);
        if (group_index != -1) {
          group = (char *)data + (int)group_index * 0x10 + 0x24;
          if (*(int16_t *)(group + 8) < *(int16_t *)((char *)data + 4)) {
            vertex_index = (int)*(int16_t *)(group + 8) * 4;
            sin_a = 0.0f;
            cos_a = 1.0f;
            qmemcpy(bounds, *(void **)0x31fc6c, 0x18);
            if (angle != *(const float *)0x2533c0) {
              sin_a = x87_fsin(angle);
              cos_a = x87_fcos(angle);
            }
            FUN_0018cf10(data, untransformed_origin,
                         untransformed_direction, (uint8_t)flags,
                         transformed_origin, transformed_direction);
            FUN_0018d490(basis, data, transformed_direction, mode,
                         (int)flags, transformed_origin);
            FUN_0018d040(data, &scale, mode, (int)flags, transformed_origin,
                         bitmap_elem);
            anim = (char *)*(int *)((char *)data + 8);
            if (anim != NULL && *(int16_t *)(anim + 0x2c) != 0 &&
                mode != 0) {
              cross_product3d(basis + 1, basis + 4, basis + 7);
              intensity = FUN_0018d670(*(uint16_t *)(anim + 0x2c),
                                       transformed_origin, basis + 7) *
                          intensity;
            }
            pixel = FUN_000d1c90(color);
            anim = (char *)*(int *)((char *)data + 8);
            if (anim != NULL && *(int16_t *)(anim + 0x2a) != 0 &&
                (*(uint8_t *)(anim + 0x28) & 2) == 0) {
              alpha_f = intensity * *(const float *)0x2602c8;
            } else {
              alpha_f = (float)(pixel >> 24) * intensity;
            }
            final_color = ((uint32_t)(uint8_t)(int)alpha_f << 24) |
                          (pixel & 0xffffff);
            mirror_x = flags & 2;
            mirror_y = flags & 4;
            corner = 0;
            do {
              if ((((corner >> 1) ^ corner) & 1) == 0) {
                u = *(float *)(elem + 8);
              } else {
                u = *(float *)(elem + 0xc);
              }
              if ((corner & 2) == 0) {
                v = *(float *)(elem + 0x14);
              } else {
                v = *(float *)(elem + 0x10);
              }
              dx = u - (*(float *)(elem + 8) + *(float *)(elem + 0x18));
              dy = (*(float *)(elem + 0x1c) + *(float *)(elem + 0x10)) - v;
              sin_dx = sin_a * dx;
              rot_x = dx * cos_a - dy * sin_a;
              rot_y = dy * cos_a + sin_dx;
              if (mirror_x != 0) {
                rot_x = -rot_x;
              }
              if (mirror_y != 0) {
                rot_y = -rot_y;
              }
              if ((*(uint8_t *)((char *)data + 0x10) & 1) != 0) {
                vtx = (float *)(*(int *)(group + 4) +
                                (int)(int16_t)vertex_index * 0x14);
                vtx[0] = rot_x * scale + transformed_origin[0];
                vtx[1] = rot_y * scale + transformed_origin[1];
                vtx[2] = u;
                vtx[3] = v;
                *(uint32_t *)(vtx + 4) = final_color;
              } else {
                vtx = (float *)(*(int *)(group + 4) +
                                (int)(int16_t)vertex_index * 0x18);
                wpoint[0] = (basis[4] * rot_y + basis[1] * rot_x) * scale +
                            transformed_origin[0];
                wpoint[1] = (basis[5] * rot_y + basis[2] * rot_x) * scale +
                            transformed_origin[1];
                wpoint[2] = (basis[6] * rot_y + basis[3] * rot_x) * scale +
                            transformed_origin[2];
                if (wpoint[0] < bounds[0]) {
                  bounds[0] = wpoint[0];
                }
                if (bounds[1] < wpoint[0]) {
                  bounds[1] = wpoint[0];
                }
                if (wpoint[1] < bounds[2]) {
                  bounds[2] = wpoint[1];
                }
                if (bounds[3] < wpoint[1]) {
                  bounds[3] = wpoint[1];
                }
                if (wpoint[2] < bounds[4]) {
                  bounds[4] = wpoint[2];
                }
                if (bounds[5] < wpoint[2]) {
                  bounds[5] = wpoint[2];
                }
                vtx[3] = u;
                vtx[0] = wpoint[0];
                vtx[4] = v;
                vtx[1] = wpoint[1];
                vtx[2] = wpoint[2];
                *(uint32_t *)(vtx + 5) = final_color;
              }
              vertex_index = vertex_index + 1;
              corner = corner + 1;
            } while ((int16_t)corner < 4);
            *(float *)((char *)data + 0x14) =
              transformed_origin[0] + *(float *)((char *)data + 0x14);
            *(float *)((char *)data + 0x18) =
              transformed_origin[1] + *(float *)((char *)data + 0x18);
            *(float *)((char *)data + 0x1c) =
              transformed_origin[2] + *(float *)((char *)data + 0x1c);
            *(int16_t *)(group + 8) = *(int16_t *)(group + 8) + 1;
            *(int16_t *)((char *)data + 0xc) =
              *(int16_t *)((char *)data + 0xc) + 1;
            if ((*(uint8_t *)((char *)data + 0x10) & 1) == 0) {
              area = render_frustum_cube_view_fraction((void *)0x5065a4,
                                                       bounds);
              *(float *)0x506504 = *(float *)0x506504 + area;
              if (area > *(const float *)0x253398) {
                old_count = *(int16_t *)0x506508;
                *(int16_t *)0x506508 = old_count + 1;
                if (old_count > 10) {
                  *(int16_t *)(group + 8) = *(int16_t *)(group + 8) - 1;
                  *(int16_t *)((char *)data + 0xc) =
                    *(int16_t *)((char *)data + 0xc) - 1;
                }
              }
              if (*(char *)0x5064e8 != 0) {
                idx = (int)(int16_t)vertex_index;
                verts = *(int *)(group + 4);
                matrix_transform_point((float *)0x5065e8,
                                       (float *)(verts +
                                                 (idx * 3 - 12) * 8),
                                       wpoint);
                matrix_transform_point((float *)0x5065e8,
                                       (float *)(verts + (idx * 3 - 9) * 8),
                                       dbg_b);
                matrix_transform_point((float *)0x5065e8,
                                       (float *)(verts + (idx * 3 - 6) * 8),
                                       dbg_c);
                matrix_transform_point((float *)0x5065e8,
                                       (float *)(verts + idx * 0x18 - 0x18),
                                       dbg_d);
                FUN_0017eb10(wpoint, dbg_b, *(int *)0x2ee6c4);
                FUN_0017eb10(wpoint, dbg_c, *(int *)0x2ee6c4);
                FUN_0017eb10(dbg_c, dbg_d, *(int *)0x2ee6c4);
                FUN_0017eb10(dbg_d, dbg_b, *(int *)0x2ee6c4);
              }
            }
          }
        }
      }
    }
  } else {
    error(2, "build_sprite sprite count exceeded.");
  }
}

/* 0x18dcf0 — build_sprite dispatcher (render/render_sprite.c). Projects the
 * sprite into screen space (FUN_0018cf10 fills two scratch vectors from
 * camera state), scores the view angle against pi/2 to derive a fade factor,
 * and emits up to two rasterizer sprite records via build_sprite
 * (FUN_0018d6e0):
 *   - A "primary" record when the fade lands inside (0.05, 1.0]. Its
 *     sequence frame is either the caller's frame (param_4) or, when flags
 *     bit1 is set, an animated frame stepped by param_7 through the bitmap
 *     sequence's frame count (fmod), with rotation forced to 0. mode
 *     defaults to 1, becomes 3 when the view distance is negative (mode-2
 *     style back-face).
 *   - A "complement" record when (1.0 - fade) > 0.05, always animated with
 *     a rotation taken from atan2(vec_b[1], vec_b[0]) and mode 1.
 * Asserts data/untransformed_origin/untransformed_axis_of_rotation at
 * render_sprite.c:0x286-0x288; param_9 defaults to *(void **)0x2ee6c4.
 * distance = FUN_0010c510(&vec_a, &vec_b) - pi/2 (0x2568bc); fade =
 * distance^2 * 0x2b1ef8, clamped to [0, 1] and gated at 0.05 (0x2533e8).
 * The animated frame is (int)fmod(count * 0x2b1bc4 * param_7 + 0.5, count).
 * fmod uses the x87_fmod helper (FPREM) to avoid clang's FPREM1
 * mis-lowering; atan2 matches VC71's inline FPATAN. cdecl, 10 stack args. */
void FUN_0018dcf0(void *param_1, unsigned int param_2, int param_3,
                  int param_4, void *param_5, void *param_6, float param_7,
                  float param_8, void *param_9, float param_10)
{
  float vec_a[3];
  float vec_b[3];
  float rotation_pre; /* permuter: pre-computed atan2 spill (+0.7pp VC71) */
  float distance;
  float fade;
  float fade2;
  float rotation;
  int count;
  int frame;
  int mode;
  void *tag;
  unsigned char zero_byte; /* permuter: materialized 0 (+0.7pp VC71) */
  char *elem;

  if (param_1 == (void *)0) {
    display_assert("data", "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x286,
                   1);
    system_exit(-1);
  }
  if (param_5 == (void *)0) {
    display_assert("untransformed_origin",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x287, 1);
    system_exit(-1);
  }
  if (param_6 == (void *)0) {
    display_assert("untransformed_axis_of_rotation",
                   "c:\\halo\\SOURCE\\render\\render_sprite.c", 0x288, 1);
    system_exit(-1);
  }
  if (param_9 == (void *)0) {
    param_9 = *(void **)0x2ee6c4;
  }

  FUN_0018cf10(param_1, (float *)param_5, (float *)param_6,
               (uint8_t)(param_2 & 1), vec_a, vec_b);
  distance = FUN_0010c510(vec_a, vec_b) - *(float *)0x2568bc;
  fade = distance * distance * *(float *)0x2b1ef8;

  if (fade >= *(float *)0x2533c0) {
    if (fade > *(float *)0x2533c8) {
      fade = 1.0f; /* immediate store (movl $0x3f800000); 0x2533c8 == 1.0f */
    } else if (fade <= *(float *)0x2533e8) {
      goto LAB_0018ded5;
    }

    tag = tag_get(0x6269746d, *(int *)param_1);
    elem = (char *)tag_block_get_element((char *)tag + 0x54,
                                         (short)param_3 + 1, 0x40);
    count = *(int16_t *)(elem + 0x34);
    zero_byte = 0;
    mode = 1;
    if ((param_2 & 2) != zero_byte) {
#if defined(_MSC_VER) && !defined(__clang__)
      /* VC71 /Oi lowers fmod to `call _CIfmod`, matching the original's
       * FUN_001daf7e dispatch. clang (-mno-sse) mis-lowers fmod to FPREM1,
       * so the shipping build takes the #else x87_fmod (FPREM) branch; the
       * binary is unchanged. See feedback_fprem1_fmod. */
      frame = (int)(fmod((float)count * *(float *)0x2b1bc4 * param_7 +
                           *(float *)0x253398,
                         (double)count) +
                    (short)param_4);
#else
      frame = (int)(x87_fmod((float)count * *(float *)0x2b1bc4 * param_7 +
                               *(float *)0x253398,
                             (double)count) +
                    (float)(short)param_4);
#endif
      rotation = 0.0f;
      if (distance < *(float *)0x2533c0) {
        frame = count - param_4;
      }
    } else {
      rotation = param_7;
      frame = param_4;
      if (distance < *(float *)0x2533c0) {
        mode = 3;
      }
    }
    FUN_0018d6e0(param_1, zero_byte, (int16_t)(param_3 + 1), (int16_t)frame,
                 vec_a, 0, rotation, param_8, (float *)param_9,
                 fade * param_10, mode);
  } else {
    fade = 0.0f; /* immediate store (MOV [fade],0x0), not a 0x2533c0 load */
  }

LAB_0018ded5:
  fade2 = *(float *)0x2533c8 - fade;
  if (fade2 > *(float *)0x2533e8) {
    tag = tag_get(0x6269746d, *(int *)param_1);
    rotation_pre = (float)atan2((double)vec_b[1], (double)vec_b[0]);
    elem = (char *)tag_block_get_element((char *)tag + 0x54, (short)param_3,
                                         0x40);
    count = *(int16_t *)(elem + 0x34);
    rotation = rotation_pre;
#if defined(_MSC_VER) && !defined(__clang__)
    frame = (int)fmod((float)count * *(float *)0x2b1bc4 * param_7 +
                        *(float *)0x253398,
                      (double)count);
#else
    frame = (int)x87_fmod((float)count * *(float *)0x2b1bc4 * param_7 +
                            *(float *)0x253398,
                          (double)count);
#endif
    FUN_0018d6e0(param_1, 0, (int16_t)param_3, (int16_t)frame, vec_a, 0,
                 rotation, param_8, (float *)param_9, fade2 * param_10, 1);
  }
}

/* 0x18df70 — triangle_strip iterator init (render/triangle_strips.c:0x16-0x17).
 * Asserts both the iterator (param_1) and the vertex-index buffer (param_2) are
 * non-NULL, then stores: +0x4 = index buffer ptr, +0x0 = vertex/index count
 * (int16 param_3), +0x2 = 0 (current position), +0x9 = 0x73 ('s' state tag).
 * cdecl, void. */
void FUN_0018df70(short *iterator, int index_buffer, short count)
{
  if (iterator == (short *)0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\render\\triangle_strips.c",
                   0x16, 1);
    system_exit(-1);
  }
  if (index_buffer == 0) {
    display_assert("triangle_strip_vertex_indices",
                   "c:\\halo\\SOURCE\\render\\triangle_strips.c", 0x17, 1);
    system_exit(-1);
  }
  *(int *)(iterator + 2) = index_buffer;
  *iterator = count;
  iterator[1] = 0;
  *((char *)iterator + 9) = 0x73;
}

/* 0x18e140 — look up the multiplayer scenario-description tag and return its
 * scenario count. Asserts the out-param is non-NULL, then resolves the
 * 'mply' tag "ui\multiplayer_scenarios". When loaded, writes the tag's int16
 * at +0x0 into *out_short and returns the int32 count at +0x4. When the tag
 * is not loaded, writes 0 to *out_short and returns 0. cdecl: out_short
 * [EBP+8] (-> EDI). */
int FUN_0018e140(short *out_short)
{
  int count;
  void *tag;

  if (out_short == 0) {
    display_assert(
      "count", "c:\\halo\\SOURCE\\scenario\\multiplayer_scenario_description.c",
      0x3d, 1);
    system_exit(-1);
  }
  count = tag_loaded(0x6d706c79, "ui\\multiplayer_scenarios");
  if (count != -1) {
    tag = tag_get(0x6d706c79, count);
    if (tag == 0) {
      display_assert(
        "scenario_list",
        "c:\\halo\\SOURCE\\scenario\\multiplayer_scenario_description.c", 0x43,
        1);
      system_exit(-1);
    }
    count = *(int *)((char *)tag + 4);
    *out_short = *(short *)tag;
    return count;
  }
  *out_short = 0;
  return 0;
}

/* 0x18c4d0 — predicate: should the local player's view be drawn in
 * first-person? Returns 1 when the director perspective for the given local
 * player is 0 (first-person view). When the perspective is not first-person,
 * the function still forces first-person if the player's unit is seated in a
 * vehicle whose unit-tag seat definition has the "force first-person" flag set
 * (unit seats block at tag+0x2e4, element size 0x11c, byte0 bit 3). Resolution
 * path: player
 * -> controlled unit handle (player_data+0x34) verified as a unit (type mask
 * 3); require a valid parent/vehicle handle (unit+0xcc) and a valid seat index
 * (unit+0x2a0, int16); resolve the vehicle object (also type mask 3), read its
 * tag index (vehicle+0), fetch the 'unit' tag, and index its seats block by the
 * seat index.
 * cdecl: local_player_index [EBP+8] (int16, passed in a dword slot). Returns
 * char bool in AL. player_data global at 0x5aa6d4 (data_t *). */
char FUN_0018c4d0(int16_t local_player_index)
{
  char first_person;
  char *player;
  char *unit;
  char *vehicle;
  char *unit_tag;
  char *seat;

  first_person = (director_get_perspective(local_player_index) == 0) ? 1 : 0;
  if (first_person == 0) {
    player = (char *)datum_get(
      *(data_t **)0x5aa6d4, local_player_get_player_index(local_player_index));
    if (*(int *)(player + 0x34) != -1) {
      unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
      if (*(int *)(unit + 0xcc) != -1 && *(int16_t *)(unit + 0x2a0) != -1) {
        vehicle = (char *)object_get_and_verify_type(*(int *)(unit + 0xcc), 3);
        unit_tag = (char *)tag_get(0x756e6974, *(int *)vehicle);
        seat = (char *)tag_block_get_element(
          unit_tag + 0x2e4, (int)*(int16_t *)(unit + 0x2a0), 0x11c);
        if ((*seat & 8) != 0) {
          return 1;
        }
      }
    }
  }
  return first_person;
}

/* 0x18e1d0 — scenario_debug_to_file: split a scenario's name (at scenario+0x20)
 * on its last backslash and format "<name>\<tail>" into the caller's output
 * buffer. Asserts the scenario pointer (param_1) is non-NULL. Walks backward
 * from the end of the name (strlen via csstrlen) to the last '\\'; if none is
 * found returns 1 with the buffer left empty. On success formats via snprintf
 * (out, size, "%s\\%s", name, tail) where tail = name + (offset_of_backslash +
 * 1). Always returns 1. cdecl: scenario [EBP+8], out_buf [EBP+0xc], buf_size
 * [EBP+0x10]. From multiplayer_scenario_description.c:0x59. */
char scenario_debug_to_file(int param_1, char *param_2, unsigned int param_3)
{
  int name;
  int idx;

  if (param_1 == 0) {
    display_assert(
      "item", "c:\\halo\\SOURCE\\scenario\\multiplayer_scenario_description.c",
      0x59, 1);
    system_exit(-1);
  }
  name = param_1 + 0x20;
  *param_2 = 0;
  idx = csstrlen((const char *)name);
  if (-1 < idx) {
    while (*(char *)(name + idx) != '\\') {
      idx = idx - 1;
      if (idx < 0) {
        return 1;
      }
    }
    snprintf(param_2, param_3, "%s\\%s", (char *)name,
             (char *)(idx + 0x21 + param_1));
  }
  return 1;
}

void scenario_initialize(void)
{
  *(void **)0x5064d0 = game_state_malloc("scenario globals", 0, 0x100);
}

/* Initialize scenario globals for a new map: clear game globals data,
 * copy default gravity/speed from constant table, reset a flag. */
void scenario_initialize_for_new_map(void)
{
  char *globals;

  ((void (*)(void))0x190500)();
  globals = *(char **)0x5064d0;
  csmemset(globals + 4, 0, 0xb0);
  qmemcpy(globals + 0xb8, (void *)0x2c1220, 0x48);
  *(uint8_t *)(globals + 0xb4) = 0;
}

void scenario_dispose_from_old_map(void)
{
  *(char *)0x5057c0 = 0;
}

/* scenario_frame_update is a direct trampoline to the wind update at
 * 0x18ffe0. The original binary has a single JMP instruction here.
 * wind_update takes no arguments; delta_time is unused (harmless under
 * cdecl — the callee never reads a stack argument). */
void scenario_frame_update(float delta_time)
{
  (void)delta_time;
  wind_update();
}

void scenario_unload(void)
{
  assert_halt(!((bool (*)())0x1c5940)());
  ((void (*)())0x1b9890)();
  *(int *)0x326a08 = NONE;
  *(int16_t *)0x326a0c = NONE;
  *(int16_t *)(*(char **)0x5064d0) = NONE;
  *(int *)0x5064e4 = 0;
  *(int *)0x5064e0 = 0;
  *(int *)0x5064dc = 0;
  *(int *)0x5064d8 = 0;
  *(int *)0x5064d4 = 0;
}

scenario_t *global_scenario_get(void)
{
  assert_halt(*(void **)0x5064e4);
  return *(scenario_t **)0x5064e4;
}

/* Return the global scenario pointer without asserting (0x18e3b0).
 * Sibling of global_scenario_get; used at call sites that must tolerate
 * a NULL scenario (e.g. early boot, between map switches). Compiles to
 * a 2-instruction MOV EAX, [global]; RET. */
void *FUN_0018e3b0(void)
{
  return *(void **)0x5064e4;
}

void *scenario_get(void)
{
  assert_halt(global_structure_bsp);
  return global_structure_bsp;
}

void *global_collision_bsp_get(void)
{
  assert_halt(global_collision_bsp);
  return global_collision_bsp;
}

/* Return the game globals tag data pointer. Asserts if not loaded. */
void *game_globals_get(void)
{
  if (!*(void **)0x5064d4) {
    display_assert("global_game_globals",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xdd, 1);
    system_exit(-1);
  }
  return *(void **)0x5064d4;
}

/* Reset a scenario location by setting its cluster_index (offset +6) to
 * NONE (-1), marking it as unresolved/invalid. */
void scenario_location_reset(int *location)
{
  *(int16_t *)((char *)location + 6) = NONE;
}

/* FUN_0018e500 (0x18e500) — look up a material type entry from game globals.
 *
 * Given a material_type index (int16_t), validate it and return a pointer to
 * the corresponding element from the game_globals material_types tag block at
 * offset 0x194. Each element is 0x374 bytes.
 *
 * If game_globals is not loaded, asserts. If material_type is out of range
 * (not NONE and not in [0, NUMBER_OF_MATERIAL_TYPES-1] where
 * NUMBER_OF_MATERIAL_TYPES==33), asserts. If material_type is NONE (-1) or the
 * index is >= the block count, returns a pointer to a static fallback buffer at
 * 0x4d8700, initialising it with a -1 sentinel on the first call.
 *
 * Confirmed: assert "global_game_globals" at line 0xdd (221).
 * Confirmed: assert string "material_type==NONE || ..." at line 0x11e (286).
 * Confirmed: block at game_globals+0x194, element size 0x374.
 * Confirmed: fallback initialises dword at 0x4d8a70 to -1, byte 0x4d8a74 to 1.
 * Confirmed: returns &DAT_004d8700 when index is NONE or out of range.
 */
void *FUN_0018e500(int16_t material_type)
{
  char *game_globals;
  int index;

  if (!*(void **)0x5064d4) {
    display_assert("global_game_globals",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xdd, 1);
    system_exit(-1);
  }
  game_globals = *(char **)0x5064d4;

  if (material_type != (int16_t)NONE &&
      (material_type < 0 || material_type >= 33)) {
    display_assert("material_type==NONE || (material_type>=0 && "
                   "material_type<NUMBER_OF_MATERIAL_TYPES)",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x11e, 1);
    system_exit(-1);
  }

  if (material_type >= 0) {
    index = (int)material_type;
    if (index < *(int *)(game_globals + 0x194)) {
      return tag_block_get_element(game_globals + 0x194, index, 0x374);
    }
  }

  if (!*(uint8_t *)0x4d8a74) {
    *(int *)0x4d8a70 = NONE;
    *(uint8_t *)0x4d8a74 = 1;
  }
  return (void *)0x4d8700;
}

/* 0x18e5c0 — does the BSP location's cluster have a "stop" background-sound
 * (sound environment) flag set? Looks up the cluster element
 * (global_structure_bsp + 0x134, element size 0x68) by location->cluster_index
 * (location+4), reads the cluster's background-sound reference (cluster+4,
 * int16). If valid (!= -1) and in range of the BSP background-sound block
 * (bsp + 0x1fc, element size 0x74), reads that block element's sound tag index
 * (+0x2c); if valid, resolves the 'lsnd' tag (tag_get 0x6c736e64) and returns
 * its first byte's bit 0. Returns 0 on any missing reference.
 * cdecl: location [EBP+8]. Returns char (bool). */
char FUN_0018e5c0(int location)
{
  char *cluster;
  char *sound_element;
  int tag_index;
  char result;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  cluster =
    (char *)tag_block_get_element((char *)global_structure_bsp + 0x134,
                                  (int)*(int16_t *)(location + 4), 0x68);

  result = 0;
  if (*(int16_t *)(cluster + 4) != -1) {
    if (!global_structure_bsp) {
      display_assert("global_structure_bsp",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
      system_exit(-1);
    }

    if ((int)*(int16_t *)(cluster + 4) <
        *(int *)((char *)global_structure_bsp + 0x1fc)) {
      sound_element =
        (char *)tag_block_get_element((char *)global_structure_bsp + 0x1fc,
                                      (int)*(int16_t *)(cluster + 4), 0x74);
      tag_index = *(int *)(sound_element + 0x2c);
      if (tag_index != -1) {
        return *(char *)tag_get(0x6c736e64 /* 'lsnd' */, tag_index) & 1;
      }
    }
  }
  return result;
}

/* 0x18e690 — returns a constant float (0.0f at 0x2533c0). */
float FUN_0018e690(void)
{
  return 0.0f;
}

/* 0x18e6a0 — copy three world basis/origin vectors into up to four caller
 * buffers. The globals are POINTERS to 3-float vectors (double-deref): up
 * vector (*0x31fc44), left vector (*0x31fc40), and a third vector (*0x2ee708)
 * delivered to BOTH out_d and out_e. Each out-param is optional (skipped when
 * NULL). Always returns 1. param_1 [EBP+8] is unused. cdecl. */
char FUN_0018e6a0(int unused, float *out_up, float *out_left, float *out_d,
                  float *out_e)
{
  float *src;

  src = *(float **)0x0031fc44;
  if (out_up != 0) {
    out_up[0] = src[0];
    out_up[1] = src[1];
    out_up[2] = src[2];
  }
  src = *(float **)0x0031fc40;
  if (out_left != 0) {
    out_left[0] = src[0];
    out_left[1] = src[1];
    out_left[2] = src[2];
  }
  src = *(float **)0x002ee708;
  if (out_d != 0) {
    out_d[0] = src[0];
    out_d[1] = src[1];
    out_d[2] = src[2];
  }
  src = *(float **)0x002ee708;
  if (out_e != 0) {
    out_e[0] = src[0];
    out_e[1] = src[1];
    out_e[2] = src[2];
  }
  return 1;
}

/* Triangle strip iterator (c:\halo\SOURCE\render\triangle_strips.c — the
 * kb.json name scenario_trigger_volume_test_point was a misattribution;
 * corrected from the assert strings). Iterates an indexed triangle strip
 * set, emitting one triangle (3 uint16 vertex indices) per call.
 *   +0 strip_count        strips remaining
 *   +2 vertex_count       vertices remaining in the current strip
 *   +4 current            pointer into the strip index stream
 *   +8 parity             winding flag, toggles every emitted triangle
 *   +9 signature          's' (asserted, triangle_strips.c:0x29)
 */
typedef struct triangle_strip_iterator {
  int16_t strip_count;
  int16_t vertex_count;
  uint16_t *current;
  char parity;
  char signature;
} triangle_strip_iterator;

/* 0x18dfe0 — triangle_strip_iterator_next: emit the next triangle of the
 * strip set into vertices[0..2], returning 1 while triangles remain, else 0.
 * Mid-strip (vertex_count != 0) the triangle is the previous two indices
 * plus the next one, with the first two swapped on odd parity to keep the
 * winding consistent; at a strip boundary the next strip's header (vertex
 * count, asserted >= 3 at line 0x41) is consumed and its first three
 * indices are emitted with parity reset to 1.
 * cdecl: iterator [EBP+8], vertices [EBP+0xc]; AL-only return (char).
 * Confirmed: asserts "iterator" 0x27, "vertices" 0x28, signature 0x29;
 * field offsets from MOV word/byte disasm; per-access current re-loads
 * (vertices may alias the stream, so no local pointer cache). */
char triangle_strip_iterator_next(void *iterator_ptr, uint16_t *vertices)
{
  triangle_strip_iterator *iterator;

  iterator = (triangle_strip_iterator *)iterator_ptr;
  if (iterator == (triangle_strip_iterator *)0) {
    display_assert("iterator", "c:\\halo\\SOURCE\\render\\triangle_strips.c",
                   0x27, 1);
    system_exit(-1);
  }
  if (vertices == (uint16_t *)0) {
    display_assert("vertices", "c:\\halo\\SOURCE\\render\\triangle_strips.c",
                   0x28, 1);
    system_exit(-1);
  }
  if (iterator->signature != 's') {
    display_assert("iterator->signature==_valid_strip_iterator_signature",
                   "c:\\halo\\SOURCE\\render\\triangle_strips.c", 0x29, 1);
    system_exit(-1);
  }

  if (iterator->vertex_count != 0) {
    if (iterator->parity != 0) {
      vertices[0] = iterator->current[-1];
      vertices[1] = iterator->current[-2];
    } else {
      vertices[0] = iterator->current[-2];
      vertices[1] = iterator->current[-1];
    }
    vertices[2] = iterator->current[0];
    iterator->vertex_count--;
    iterator->current += 1;
    iterator->parity = (char)(iterator->parity == 0);
    return 1;
  }

  if (iterator->strip_count != 0) {
    /* strip header: index count; the strip yields count-2 triangles, so
     * count-3 remain after the first triangle below */
    iterator->vertex_count = (int16_t)(*iterator->current - 3);
    iterator->current += 1;
    if (iterator->vertex_count < 0) {
      display_assert("iterator->vertex_count>=0",
                     "c:\\halo\\SOURCE\\render\\triangle_strips.c", 0x41, 1);
      system_exit(-1);
    }
    iterator->parity = 1;
    vertices[0] = *iterator->current;
    iterator->current += 1;
    vertices[1] = *iterator->current;
    iterator->current += 1;
    vertices[2] = *iterator->current;
    iterator->strip_count--;
    iterator->current += 1;
    return 1;
  }

  return 0;
}

/* FUN_0018e720 (0x18e720) — bsp3d_find_leaf_point
 *
 * Looks up the BSP3D leaf containing a given 3D point by traversing the
 * bsp3d tree from the root (node 0). Asserts that the global bsp3d tree
 * pointer is non-null before calling.
 *
 * Confirmed: assert "global_bsp3d" at 0xd5 (213).
 * Confirmed: CALL 0x146db0 (bsp3d_find_leaf) with args (bsp3d, 0, point).
 * Confirmed: single cdecl param (point pointer) at [EBP+8].
 * Confirmed: return value is bsp3d_find_leaf result in EAX.
 */
int FUN_0018e720(int point)
{
  if (*(void **)0x5064d8 == NULL) {
    display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xd5, 1);
    system_exit(-1);
  }
  return (int)bsp3d_find_leaf(*(void **)0x5064d8, 0, (void *)point);
}

/* 0x18e770 — Look up the sky tag ref (tag index) for a given sky list index.
 * Returns the 4-byte tag ref from the scenario sky block, or -1 if invalid. */
int FUN_0018e770(int16_t sky_index)
{
  int result;
  char *element;

  if (*(int *)0x5064e4 == 0) {
    display_assert("global_scenario", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xb7, 1);
    system_exit(-1);
  }
  result = -1;
  if ((int)sky_index >= 0) {
    if ((int)sky_index < *(int *)(*(int *)0x5064e4 + 0x30)) {
      element = (char *)tag_block_get_element((int *)(*(int *)0x5064e4 + 0x30),
                                              sky_index, 0x10);
      result = *(int *)(element + 0xc);
    }
  }
  return result;
}

/* 0x18e7d0 — Get sky tag pointer for a given sky list index.
 * Returns tag_get('sky ', tag_ref) or NULL if index is invalid. */
void *FUN_0018e7d0(int param_1)
{
  int tag_ref;

  tag_ref = FUN_0018e770((int16_t)param_1);
  if (tag_ref != -1)
    return tag_get(0x736b7920, tag_ref); /* 'sky ' */
  return NULL;
}

/* 0x18e800 — scenario_ensure_point_within_world: test whether a cluster is
 * audible/visible by checking bit cluster_index1 in the current structure
 * BSP's per-cluster sound-data bitvector for source cluster_index. Asserts the
 * BSP is loaded (*0x5064e0, scenario.c:0xc5) and that cluster_index1 is in
 * range [0, structure_bsp->clusters.count) (bsp+0x134, scenario.c:0x1d4).
 * Returns the tested bit as 0/1. cdecl: cluster_index [EBP+8] (int, passed as
 * the callee's int16_t arg), cluster_index1 [EBP+0xc] (MOVSX int16_t). */
bool scenario_ensure_point_within_world(int cluster_index,
                                        int16_t cluster_index1)
{
  void *bsp;
  uint32_t *sound_data;

  if (*(int *)0x5064e0 == 0) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }
  bsp = *(void **)0x5064e0;
  sound_data =
    structure_bsp_get_cluster_sound_data(bsp, (int16_t)cluster_index);
  if (cluster_index1 < 0 ||
      (int)cluster_index1 >= *(int *)((char *)bsp + 0x134)) {
    display_assert(
      "cluster_index1>=0 && cluster_index1<structure_bsp->clusters.count",
      "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x1d4, 1);
    system_exit(-1);
  }
  return (sound_data[(int)cluster_index1 >> 5] &
          (1u << ((int)cluster_index1 & 0x1f))) != 0;
}

/* 0x18e8a0 — AND the two clusters' PVS (potentially-visible-set) bit vectors.
 * Asserts the structure BSP global (0x5064e0) is loaded, fetches each region's
 * cluster sound-data PVS bitfield via structure_bsp_get_cluster_sound_data,
 * then bit_vector_and's them, size = cluster count (uint16 @bsp+0x134), into a
 * NULL result buffer (in-place / dry-run, result discarded). cdecl, void. */
void scenario_get_fog_region_index(int region_a, int region_b)
{
  int bsp;
  int pvs_a;
  int pvs_b;

  if (*(int *)0x5064e0 == 0) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }
  bsp = *(int *)0x5064e0;
  pvs_a = (int)structure_bsp_get_cluster_sound_data((void *)bsp, region_a);
  pvs_b = (int)structure_bsp_get_cluster_sound_data((void *)bsp, region_b);
  bit_vector_and(*(unsigned short *)(bsp + 0x134), pvs_a, pvs_b, 0);
}

bool scenario_location_potentially_visible_local(void *location)
{
  int16_t cluster_index;
  void *pvs;

  if (*(int16_t *)((char *)location + 4) >= 0) {
    if (!global_structure_bsp) {
      display_assert("global_structure_bsp",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
      system_exit(-1);
    }
    if ((int)*(int16_t *)((char *)location + 4) <
        *(int *)((char *)global_structure_bsp + 0x134))
      goto valid;
  }
  display_assert(
    "location->cluster_index>=0 && "
    "location->cluster_index<global_structure_bsp_get()->clusters.count",
    "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x1e7, 1);
  system_exit(-1);

valid:
  cluster_index = *(int16_t *)((char *)location + 4);
  pvs = players_get_combined_pvs_local();
  return (*(uint32_t *)((char *)pvs + ((int)cluster_index >> 5) * 4) &
          (1u << (cluster_index & 0x1f))) != 0;
}

/* 0x18e9b0 — test whether a BSP location's cluster is in the combined
 * potentially-visible set for all players. Asserts the location's cluster_index
 * (location+4, int16) is in [0, global_structure_bsp->clusters.count). Then
 * fetches the combined PVS bit vector (players_get_combined_pvs) and returns
 * the cluster's bit. This is the all-players variant of
 * scenario_location_potentially_visible_local (which uses the local-player
 * PVS); its assert line is 0x1ef vs 0x1e7. cdecl: location [EBP+8]. Returns
 * bool. */
bool scenario_location_potentially_visible(void *location)
{
  int16_t cluster_index;
  void *pvs;

  if (*(int16_t *)((char *)location + 4) >= 0) {
    if (!global_structure_bsp) {
      display_assert("global_structure_bsp",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
      system_exit(-1);
    }
    if ((int)*(int16_t *)((char *)location + 4) <
        *(int *)((char *)global_structure_bsp + 0x134))
      goto valid;
  }
  display_assert(
    "location->cluster_index>=0 && "
    "location->cluster_index<global_structure_bsp_get()->clusters.count",
    "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x1ef, 1);
  system_exit(-1);

valid:
  cluster_index = *(int16_t *)((char *)location + 4);
  pvs = players_get_combined_pvs();
  return (*(uint32_t *)((char *)pvs + ((int)cluster_index >> 5) * 4) &
          (1u << (cluster_index & 0x1f))) != 0;
}

/* 0x18ea50 — find a named entry in a scenario tag's name block (param_1+0x204,
 * element size 0x24) by csstrcmp against `name`; returns the 0-based index or
 * -1 (NONE) if no match. Block count is re-read from *(int*)(param_1+0x204)
 * each iteration. */
int16_t FUN_0018ea50(void *param_1, const char *name)
{
  int16_t i;
  void *element;

  i = 0;
  if (*(int *)((char *)param_1 + 0x204) > 0) {
    do {
      element = tag_block_get_element((char *)param_1 + 0x204, i, 0x24);
      if (csstrcmp(element, name) == 0)
        return i;
      i = i + 1;
    } while ((int)i < *(int *)((char *)param_1 + 0x204));
  }
  return -1;
}

/*
 * scenario_get_structure_reference_index_from_tag_index — resolve a BSP3D node
 * index to its fog tag index.
 *
 * Given a bsp3d_node_index, looks up the node element (bsp+0x184, size 0x28)
 * and reads the fog palette index at offset +0x24 (int16_t). If valid, looks
 * up the fog palette entry (bsp+0x190, size 0x88) and returns the fog tag
 * index at offset +0x2c. Returns -1 if any step fails.
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: tag_block_get_element(bsp+0x184, node_index, 0x28) for BSP3D node.
 * Confirmed: tag_block_get_element(bsp+0x190, palette_index, 0x88) for fog
 * palette. Confirmed: returns *(int*)(palette_entry + 0x2c) or -1.
 */
int scenario_get_structure_reference_index_from_tag_index(
  int16_t bsp3d_node_index)
{
  char *bsp;
  char *node_element;
  int16_t fog_palette_index;
  char *palette_entry;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;

  if (bsp3d_node_index != NONE) {
    node_element =
      (char *)tag_block_get_element(bsp + 0x184, (int)bsp3d_node_index, 0x28);
    fog_palette_index = *(int16_t *)(node_element + 0x24);
    if (fog_palette_index != NONE) {
      palette_entry = (char *)tag_block_get_element(
        bsp + 0x190, (int)fog_palette_index, 0x88);
      if (*(int *)(palette_entry + 0x2c) != -1) {
        return *(int *)(palette_entry + 0x2c);
      }
    }
  }

  return -1;
}

/* Switch the active structure BSP. Calls dispose callbacks on the old BSP,
 * loads the new one from the scenario tag, and calls initialize callbacks.
 * Returns false if the BSP index is invalid or the BSP fails to load. */
bool scenario_switch_structure_bsp(__int16 bsp_index)
{
  char *scenario_tag;
  char *bsp_ref;
  bool had_old_bsp;
  bool result = 0;

  if (bsp_index == *(int16_t *)0x326a0c)
    return result;
  if (bsp_index < 0)
    return result;

  scenario_tag = *(char **)0x5064e4;
  if ((int)bsp_index >= *(int *)(scenario_tag + 0x5a4))
    return result;

  bsp_ref =
    (char *)tag_block_get_element(scenario_tag + 0x5a4, (int)bsp_index, 0x20);

  assert_halt(*(char **)0x5064e4);

  /* stop rendering during BSP switch */
  ((void (*)(void))0x101c90)();
  ((void (*)(int))0x14d070)(0);

  had_old_bsp = 0;
  if (*(int16_t *)0x326a0c != -1) {
    /* call 10 dispose-from-old-map callbacks */
    int *dispose_table = (int *)0x326a44;
    int i;
    for (i = 0; i < 10; i++)
      ((void (*)(void))dispose_table[i])();

    /* unload old BSP tag */
    {
      char *old_bsp = (char *)tag_block_get_element(
        *(char **)0x5064e4 + 0x5a4, (int)*(int16_t *)0x326a0c, 0x20);
      ((void (*)(void *))0x1ba0c0)(old_bsp);
    }

    *(int16_t *)(*(char **)0x5064d0) = -1;
    *(int16_t *)0x326a0c = -1;
    had_old_bsp = 1;
  }

  /* load new BSP */
  if (((bool (*)(void *))0x1b9fa0)(bsp_ref)) {
    char *bsp_tag = (char *)tag_get(0x73627370, *(int *)(bsp_ref + 0x1c));
    *(char **)0x5064e0 = bsp_tag;

    *(char **)0x5064dc = (char *)tag_block_get_element(bsp_tag + 0xb0, 0, 0x60);
    *(char **)0x5064d8 = (char *)tag_block_get_element(bsp_tag + 0xb0, 0, 0x60);

    *(int16_t *)(*(char **)0x5064d0) = bsp_index;
    *(int16_t *)0x326a0c = bsp_index;

    if (had_old_bsp) {
      /* call 13 initialize-for-new-map callbacks */
      int *init_table = (int *)0x326a10;
      int i;
      for (i = 0; i < 13; i++)
        ((void (*)(void))init_table[i])();
    }
    result = 1;
  } else {
    /* BSP load failed */
    error(0, "%s", *(char **)(bsp_ref + 0x14));
  }

  /* resume rendering */
  ((void (*)(int))0x14d070)(1);
  ((void (*)(void))0x101ca0)();

  return result;
}

/* 0x18ecd0 — unload the currently-loaded structure BSP if one is active.
 * If the cached BSP index (*(int16_t*)0x326a0c) differs from the index stored
 * at *(int16_t*)(*0x5064d0), frees the active BSP's structure-references block
 * element (scenario+0x5a4, size 0x20) via FUN_001ba0c0, marks the cached index
 * NONE, and re-switches to the index held at *0x5064d0. */
void FUN_0018ecd0(void)
{
  void *element;

  if (*(int16_t *)(*(char **)0x5064d0) != *(int16_t *)0x326a0c) {
    element = tag_block_get_element(*(char **)0x5064e4 + 0x5a4,
                                    (int)*(int16_t *)0x326a0c, 0x20);
    FUN_001ba0c0(element);
    *(int16_t *)0x326a0c = -1;
    scenario_switch_structure_bsp(*(int16_t *)(*(char **)0x5064d0));
  }
}

/* 0x18ed20 — find a scenario structure-BSP reference index by tag name.
 * Resolves param_2 (a tag index) to its name via tag_get_name, then linearly
 * scans the structure_bsp block (scenario_tag+0x5a4, 0x20-byte elements),
 * csstrcmp'ing each element's tag-name pointer (+0x14) against it. Returns the
 * 0-based index of the first match, or -1 if none. Count is re-read from
 * [param_1+0x5a4] each iteration, matching the original. cdecl; returns short
 * (only AX is written on the match path). */
short FUN_0018ed20(int param_1, int param_2)
{
  const char *target_name;
  char *element;
  short i;

  target_name = tag_get_name(param_2);
  for (i = 0; i < *(int *)(param_1 + 0x5a4); i++) {
    element = (char *)tag_block_get_element((void *)(param_1 + 0x5a4), i, 0x20);
    if (csstrcmp(target_name, *(const char **)(element + 0x14)) == 0) {
      return i;
    }
  }
  return -1;
}

/* 0x18ed90 — test whether a world-space point lies inside a scenario cluster's
 * shape volume. The cluster element (0x60 bytes) comes from the global scenario
 * tag's cluster block (global_scenario + 0x360, element size 0x60), indexed by
 * cluster_index. The shape type is the int16 at offset +0:
 *   - type 0 (axis-aligned box): the point is inside iff each component lies
 *     strictly within the half-open bounds box[+0x48..+0x4c] (x),
 *     box[+0x50..+0x54] (y), box[+0x58..+0x5c] (z); the low bound uses '<='
 *     (must be strictly greater) and the high bound uses '>=' (must be strictly
 *     less). Any out-of-range component returns 0.
 *   - type 1 (oriented box): build a 4x3 transform from the box basis
 *     (position box+0x48, forward box+0x30, up box+0x3c), transform the point
 *     into box-local space, then require each local component to be in
 *     (0, box+0x54), (0, box+0x58), (0, box+0x5c) respectively.
 * Any other type asserts. Returns 1 if inside, 0 otherwise.
 * cdecl: cluster_index [EBP+8] (int16, MOVSX), point [EBP+0xc] (float*). */
char FUN_0018ed90(short cluster_index, float *point)
{
  char *box;
  float local[16];
  float transformed[3];
  float z;

  if (*(int *)0x5064e4 == 0) {
    display_assert("global_scenario", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xb7, 1);
    system_exit(-1);
  }

  box = (char *)tag_block_get_element((void *)(*(int *)0x5064e4 + 0x360),
                                      (int)cluster_index, 0x60);

  switch (*(int16_t *)box) {
  case 0:
    if (point[0] <= *(float *)(box + 0x48)) {
      return 0;
    }
    if (point[1] <= *(float *)(box + 0x50)) {
      return 0;
    }
    if (point[2] <= *(float *)(box + 0x58)) {
      return 0;
    }
    if (!(point[0] < *(float *)(box + 0x4c))) {
      return 0;
    }
    if (!(point[1] < *(float *)(box + 0x54))) {
      return 0;
    }
    z = point[2];
    break;
  case 1:
    matrix4x3_from_forward_up_position(local, (float *)(box + 0x48),
                                       (float *)(box + 0x30),
                                       (float *)(box + 0x3c));
    real_matrix3x3_transform_point(local, point, transformed);
    if (transformed[0] <= *(float *)0x2533c0) {
      return 0;
    }
    if (transformed[1] <= *(float *)0x2533c0) {
      return 0;
    }
    if (transformed[2] <= *(float *)0x2533c0) {
      return 0;
    }
    if (!(transformed[0] < *(float *)(box + 0x54))) {
      return 0;
    }
    if (!(transformed[1] < *(float *)(box + 0x58))) {
      return 0;
    }
    z = transformed[2];
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\scenario\\scenario.c", 0x331, 1);
    system_exit(-1);
    z = 0.0f;
    break;
  }

  if (!(z < *(float *)(box + 0x5c))) {
    return 0;
  }
  return 1;
}

/* 0x18ef00 — test an object's physics position against cluster volume
 * cluster_index. Resolves the object handle (no type restriction) and forwards
 * the object's physics-position vector (object_data + 0x50) to the cluster
 * point-in-volume test FUN_0018ed90. Returns false (AL=0) when the handle is
 * -1. cdecl: cluster_index [EBP+8], object_handle [EBP+0xc]; byte (AL) return.
 */
char FUN_0018ef00(int cluster_index, int object_handle)
{
  char result;
  int obj;

  result = 0;
  if (object_handle != -1) {
    obj = (int)object_get_and_verify_type(object_handle, 0xffffffff);
    result = FUN_0018ed90((short)cluster_index, (float *)(obj + 0x50));
  }
  return result;
}

/* 0x18ef30 — scenario_debug_dump_status: print the loaded scenario/current BSP
 * names and every player's world position to the supplied stream.
 *
 * cdecl, one stack arg: stream = [EBP+8] (held in EDI across the loop, ECX in
 * the no-scenario branch). Returns void.
 *
 * Confirmed control flow / ABI (disasm 0x18ef30-0x18f071):
 *  - scenario tag index at 0x326a08; when -1, print "<no scenario loaded>\n"
 *    and return.
 *  - global_scenario pointer at 0x5064e4; when null, assert "global_scenario"
 *    at scenario.c line 0xb7 then system_exit(-1) (re-read separately at the
 *    +0x5a4 use, matching the original's two distinct loads).
 *  - current structure_bsp index is a MOVSX word at 0x326a0c (int16 -> int),
 *    used both as the tag_block_get_element index AND as the "#%d" vararg.
 *  - MSVC deferred-vararg scheduling: the bsp_index push (0x18ef75) precedes
 *    tag_block_get_element's 3 real args and survives its cleanup to become
 *    the fprintf %d; likewise the first tag_get_name result (0x18ef9b) is a
 *    pre-pushed %s. tag_block_get_element takes 3 args, tag_get_name takes 1.
 *  - players iterated with data_iterator_new/next over the data_t at 0x5aa6d4;
 *    the "player 0x%08x" handle is iter+0x8 (data_iter_t.datum_handle), NOT the
 *    returned element (buffer-alias §5).
 *  - player object handle at player+0x34; when -1 print " dead\n", else resolve
 *    with object_get_and_verify_type(handle, 3) and print physics position
 *    (floats at obj+0x50/0x54/0x58 promoted to double), leaf (dword obj+0x48)
 *    and cluster (MOVSX word obj+0x4c). */
void FUN_0018ef30(void *stream)
{
  data_iter_t iter;
  void *player;
  void *obj;
  void *element;
  int bsp_index;
  int handle;
  const char *bsp_name;
  const char *scen_name;

  if (*(int32_t *)0x326a08 != -1) {

  if (*(void **)0x5064e4 == 0) {
    display_assert("global_scenario", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xb7, 1);
    system_exit(-1);
  }

  bsp_index = *(int16_t *)0x326a0c;
  element =
    tag_block_get_element((char *)*(void **)0x5064e4 + 0x5a4, bsp_index, 0x20);
  bsp_name = tag_get_name(*(int32_t *)((char *)element + 0x1c));
  scen_name = tag_get_name(*(int32_t *)0x326a08);
  crt_fprintf(stream, "\"%s\" bsp \"%s\" (#%d)\n", scen_name, bsp_name,
              bsp_index);

  data_iterator_new(&iter, *(data_t **)0x5aa6d4);
  for (player = data_iterator_next(&iter); player != 0;
       player = data_iterator_next(&iter)) {
    crt_fprintf(stream, "player 0x%08x", iter.datum_handle);
    handle = *(int32_t *)((char *)player + 0x34);
    if (handle != -1) {
      obj = object_get_and_verify_type(handle, 3);
      crt_fprintf(
        stream, " at (%.2f,%.2f,%.2f) (leaf#%d,cluster#%d)\n",
        *(float *)((char *)obj + 0x50), *(float *)((char *)obj + 0x54),
        *(float *)((char *)obj + 0x58), *(int32_t *)((char *)obj + 0x48),
        (int)*(int16_t *)((char *)obj + 0x4c));
    } else {
      crt_fprintf(stream, " dead\n");
    }
  }

  } else {
    crt_fprintf(stream, "<no scenario loaded>\n");
  }
}

/* Return the current structure BSP index.
 * Confirmed: MOV AX,[0x326a0c]; RET — 2-instruction getter. */
short global_structure_bsp_index_get(void)
{
  return *(int16_t *)0x326a0c;
}

/* Load a scenario from the map cache. Opens the cache file, loads the
 * scenario and game globals tags, and switches to BSP 0. */
bool scenario_load(const char *map_name)
{
  int tag_index;
  int matg_index;
  char *scenario_tag;
  bool result = 0;

  ((void (*)(void *, const char *))0x8e770)((void *)0x326a6c, "scenario_load");
  tag_index = ((int (*)(const char *))0x1b9e70)(map_name);
  *(int *)0x326a08 = tag_index;

  if (tag_index == -1) {
    /* map not found — print error with map path line by line */
    char *path = (char *)0x25386f;
    char *nl;
    error(1, "couldn't open map file");
    do {
      nl = ((char *(*)(const char *, int))0x1d95d0)(path, '\n');
      if (nl)
        *nl = 0;
      error(1, "%s", path);
      if (!nl)
        break;
      *nl = '\n';
      path = nl + 1;
    } while (path);
    return result;
  }

  scenario_tag = (char *)tag_get(0x73636e72, tag_index);
  *(char **)0x5064e4 = scenario_tag;

  if (*(int *)(scenario_tag + 0x5a4) < 1) {
    error(1, "scenario has no structure bsps");
    return result;
  }

  /* load game globals tag ("matg") */
  matg_index = tag_loaded(0x6d617467, "globals\\globals");
  *(char **)0x5064d4 = (char *)tag_get(0x6d617467, matg_index);

  if (scenario_switch_structure_bsp(0))
    return 1;

  return result;
}

/*
 * scenario_location_from_point — resolve a 3D position to a BSP location.
 *
 * Looks up the BSP leaf containing the point via the bsp3d tree, then
 * reads the cluster index from the leaf entry in the structure BSP tag.
 * Writes {leaf_index, cluster_index} into the 8-byte location_out struct.
 *
 * Confirmed: asserts "global_bsp3d" at line 0xd5, "global_structure_bsp"
 *            at line 0xc5.
 * Confirmed: CALL 0x146db0 with 3 args (bsp3d, 0, point) — cdecl.
 * Confirmed: leaf result masked with 0x7fffffff before indexing.
 * Confirmed: tag_block_get_element(bsp+0xe0, leaf & 0x7fffffff, 0x10).
 * Confirmed: cluster_index read as int16 at element+8.
 */
void scenario_location_from_point(void *location_out, void *point)
{
  uint32_t *loc = (uint32_t *)location_out;
  char *element;
  uint32_t leaf;

  if (*(void **)0x5064d8 == 0) {
    display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xd5, 1);
    system_exit(-1);
  }

  leaf = bsp3d_find_leaf(*(void **)0x5064d8, 0, point);
  loc[0] = leaf;

  if (leaf == 0xffffffff) {
    *(int16_t *)&loc[1] = -1;
    return;
  }

  if (*(void **)0x5064e0 == 0) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  element = (char *)tag_block_get_element((char *)*(void **)0x5064e0 + 0xe0,
                                          leaf & 0x7fffffff, 0x10);
  *(int16_t *)&loc[1] = *(int16_t *)(element + 8);
}

/* 0x18f230 — thin wrapper that forwards to scenario_location_from_point using
 * its 1st and 4th arguments (location_out=param_1, point=param_4); params 2
 * and 3 are received but unused. */
void FUN_0018f230(void *param_1, void *param_2, void *param_3, void *param_4)
{
  scenario_location_from_point(param_1, param_4);
}

/* Probes whether the global BSP3D leaf at the given position is solid (no leaf
   found). Steps the position up in Z by 0.05 up to 0x96 times until a leaf is
   located. Returns 1 if a leaf was found on the very first probe (position was
   immediately above geometry / "underwater"), 0 otherwise. */
char scenario_location_underwater(float *position)
{
  short count;
  uint32_t leaf;

  count = 0;
  while (1) {
    if (*(int *)0x5064d8 == 0) {
      display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                     0xd5, 1);
      system_exit(-1);
    }
    leaf = bsp3d_find_leaf(*(void **)0x5064d8, 0, (void *)position);
    if (leaf != 0xffffffff) {
      break;
    }
    if (count >= 0x96) {
      break;
    }
    position[2] += 0.05f;
    count = count + 1;
  }
  return count == 0;
}

/*
 * FUN_0018f2d0 — resolve a BSP location to a fog palette index.
 *
 * Given a location (with cluster_index at offset +4) and a 3D position,
 * looks up the cluster's fog reference from the structure BSP clusters
 * tag block (bsp+0x134, element size 0x68). The fog reference short at
 * offset +2 in the cluster element can be:
 *   - NONE (-1): return NONE.
 *   - Non-negative (bit 15 clear): direct fog palette index (& 0x7fff).
 *   - Negative (bit 15 set): index into the fog planes tag block
 *     (bsp+0x178, element size 0x20). The plane entry contains a fog
 *     palette index at offset +0 and a plane normal+d at offset +4..+0x10.
 *     If the position is on the near side of the plane (dot - d + threshold
 *     < 0.0), returns the fog palette index; otherwise returns NONE.
 *
 * The fog threshold is read from the fog tag (0x666f6720 'fog ') at
 * offset +0x74, but only if the fog tag's first byte has bit 0 set.
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: tag_block_get_element(bsp+0x134, cluster_index, 0x68).
 * Confirmed: tag_block_get_element(bsp+0x178, plane_index, 0x20).
 * Confirmed: FCOMP against 0.0f at [0x2533c0]; TEST AH,5 / JP pattern.
 * Confirmed: calls scenario_get_structure_reference_index_from_tag_index and
 * tag_get('fog ', ...).
 */
int16_t FUN_0018f2d0(void *location, void *position)
{
  char *bsp;
  char *cluster;
  int16_t fog_ref;
  char *plane_entry;
  int16_t fog_palette_index;
  int fog_tag_index;
  char *fog_tag;
  float fog_threshold;

  if (*(int16_t *)((char *)location + 4) == NONE)
    return NONE;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;
  cluster = (char *)tag_block_get_element(
    bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);

  fog_ref = *(int16_t *)(cluster + 2);
  if (fog_ref == NONE)
    return NONE;

  if (fog_ref >= 0)
    return fog_ref & 0x7fff;

  plane_entry =
    (char *)tag_block_get_element(bsp + 0x178, (int)(fog_ref & 0x7fff), 0x20);

  fog_palette_index = *(int16_t *)plane_entry;

  fog_tag_index =
    scenario_get_structure_reference_index_from_tag_index(fog_palette_index);
  fog_threshold = 0.0f;
  if (fog_tag_index != -1) {
    fog_tag = (char *)tag_get(0x666f6720, fog_tag_index);
    if (*(uint8_t *)fog_tag & 1) {
      fog_threshold = *(float *)(fog_tag + 0x74);
    }
  }

  if (position == NULL ||
      (*(float *)(plane_entry + 4) * *(float *)position +
       *(float *)(plane_entry + 8) * *((float *)position + 1) +
       *(float *)(plane_entry + 0xc) * *((float *)position + 2)) -
          *(float *)(plane_entry + 0x10) + fog_threshold <
        *(float *)0x2533c0) {
    return fog_palette_index;
  }

  return NONE;
}

/*
 * FUN_0018f3e0 -- determine whether a position is under indoor fog.
 *
 * Given a BSP location and a 3D position, finds the BSP3D node via
 * FUN_0018f2d0, then looks up the corresponding fog tag. Returns true
 * (1) if the fog tag's first byte has bit 0 set (indoor fog flag).
 *
 * Optionally writes a sky index to *out_sky_index: either from the
 * BSP3D node element (offset +0x26), or as a fallback from the cluster
 * element (offset +0x08 in the clusters tag_block at bsp+0x134).
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: asserts "location" at line 0x258 (600).
 * Confirmed: asserts "position" at line 0x259 (601).
 * Confirmed: tag_get('fog ', fog_index) to read indoor flag.
 * Confirmed: tag_block_get_element(bsp+0x184, node, 0x28) for BSP3D node.
 * Confirmed: tag_block_get_element(bsp+0x134, cluster, 0x68) fallback.
 */
bool FUN_0018f3e0(void *location, void *position, int16_t *out_sky_index)
{
  char *bsp;
  int16_t node_index;
  char *node_element;
  char *cluster_element;
  int fog_index;
  char *fog_tag;
  bool is_indoor;
  int16_t sky_index;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;
  node_index = FUN_0018f2d0(location, position);
  sky_index = NONE;
  is_indoor = false;

  assert_halt(location);
  assert_halt(position);

  if (node_index == NONE) {
    /* no BSP3D node found -- fall through to cluster fallback */
  } else {
    node_element =
      (char *)tag_block_get_element(bsp + 0x184, (int)node_index, 0x28);
    fog_index =
      scenario_get_structure_reference_index_from_tag_index(node_index);
    if (fog_index == -1) {
      is_indoor = false;
    } else {
      fog_tag = (char *)tag_get(0x666f6720, fog_index);
      is_indoor = *(uint8_t *)fog_tag & 1;
    }
    sky_index = *(int16_t *)(node_element + 0x26);
    if (sky_index != NONE)
      goto done;
  }

  /* fallback: read sky index from the cluster */
  if (*(int16_t *)((char *)location + 4) != NONE) {
    cluster_element = (char *)tag_block_get_element(
      bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);
    sky_index = *(int16_t *)(cluster_element + 0x8);
  }

done:
  if (out_sky_index != NULL) {
    *out_sky_index = sky_index;
  }
  return is_indoor;
}

/*
 * FUN_0018f510 -- compute signed fog distance at a BSP location.
 *
 * Given a BSP location (with cluster_index at offset +4) and a 3D position,
 * looks up the cluster's fog reference from the structure BSP clusters tag
 * block (bsp+0x134, element size 0x68). The fog reference short at offset +2
 * in the cluster element can be:
 *   - NONE (-1): return -FLT_MAX sentinel.
 *   - Negative (bit 15 set): index into the fog planes tag block
 *     (bsp+0x178, element size 0x20) with a plane at offset +4 and a
 *     fog palette index at offset +0.
 *   - Non-negative: direct fog palette index (no plane).
 *
 * The fog palette index is resolved to a fog tag via
 * scenario_get_structure_reference_index_from_tag_index. If the fog tag's first
 * byte has bit 0 set (indoor fog), the function:
 *   - With a plane: returns -(dot(plane, position) + fog_tag[0x74]).
 *   - Without a plane: returns FLT_MAX.
 * Otherwise returns -FLT_MAX.
 *
 * Confirmed: asserts "global_structure_bsp" at line 0xc5.
 * Confirmed: tag_get('fog ', tag_index) at CALL 0x1ba140.
 * Confirmed: plane3d_distance_to_point(plane, position) is plane_test_point
 * (dot - d). Confirmed: FCHS negates the sum before return. Confirmed: 0x2548fc
 * = FLT_MAX (0x7f7fffff). Confirmed: default return = -FLT_MAX (0xff7fffff).
 */
float FUN_0018f510(void *location, void *position)
{
  char *bsp;
  char *cluster;
  int16_t fog_ref;
  int16_t fog_palette_index;
  float *plane;
  int tag_index;
  char *fog_tag;
  float d;

  if (*(int16_t *)((char *)location + 4) == NONE)
    return -3.4028235e+38f;

  if (!global_structure_bsp) {
    display_assert("global_structure_bsp",
                   "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
    system_exit(-1);
  }

  bsp = (char *)global_structure_bsp;
  cluster = (char *)tag_block_get_element(
    bsp + 0x134, (int)*(int16_t *)((char *)location + 4), 0x68);

  fog_ref = *(int16_t *)(cluster + 2);
  if (fog_ref == NONE)
    return -3.4028235e+38f;

  if (fog_ref < 0) {
    char *palette_entry =
      (char *)tag_block_get_element(bsp + 0x178, (int)(fog_ref & 0x7fff), 0x20);
    fog_palette_index = *(int16_t *)palette_entry;
    plane = (float *)(palette_entry + 4);
  } else {
    fog_palette_index = fog_ref & 0x7fff;
    plane = NULL;
  }

  tag_index =
    scenario_get_structure_reference_index_from_tag_index(fog_palette_index);
  if (tag_index == -1)
    return -3.4028235e+38f;

  fog_tag = (char *)tag_get(0x666f6720, tag_index);
  if (!(*(uint8_t *)fog_tag & 1))
    return -3.4028235e+38f;

  if (plane != NULL) {
    d = plane3d_distance_to_point(plane, position);
    return -(d + *(float *)(fog_tag + 0x74));
  }

  return 3.4028235e+38f;
}

/* 0x18f600 — determine the active sound environment for the local players.
 *
 * Scans local players 0..3. For each player whose camera is in a valid BSP
 * cluster, examines two candidate 'snde' (sound environment) tag sources and
 * keeps the one with the highest priority (snde tag + 4, int16):
 *   A) the fog palette of the cluster's structure reference — fog tag + 0x110
 *      is the snde reference, fog tag + 0x100 the environment datum, and fog
 *      byte0 bit 0 the "changed" flag;
 *   B) the cluster's own sound-environment reference (cluster+6 -> structure
 *      BSP block +0x208, size 0x50); its environment datum comes from
 *      cluster+4 -> background-sound block +0x1fc (size 0x74), element +0x2c,
 *      or -1 when out of range.
 * The winning snde datum drives the sound manager block at *0x5064d0 + 0xb8:
 * when the resolved changed-flag differs from the stored flag (mgr+0xb4) the
 * whole 0x48-byte source block is copied in and *out_changed is set to 1;
 * otherwise the 12 environment floats (mgr+0xc0..+0xec) are moved toward the
 * source by a per-field min/max clamped step and *out_changed is set to 0.
 * Emits a debug string when the module debug flag at 0x5064cc is non-zero.
 * Both paths write the environment datum to *out_datum and the manager env
 * block pointer (mgr+0xb8) to *out_env.
 * cdecl: out_datum [EBP+8], out_env [EBP+0xc], out_changed [EBP+0x10]. */
void scenario_get_sound_environment(int32_t *out_datum, void **out_env,
                                    uint8_t *out_changed)
{
  int player;
  int player_index;
  void *camera;
  char *bsp;
  char *cluster;
  int16_t node;
  volatile unsigned int
    ref_index; /* volatile: force MSVC stack spill (permuter 89.4% shape) */
  char *fog_tag;
  int fog_snde;
  char *env_element;
  int cluster_snde;
  int16_t bg_ref;
  char *bg_element;

  int32_t env_datum;
  int snde_datum;
  int16_t best_prio;
  uint8_t flag;

  const char *env_name;
  int *env_src;
  char *mgr;
  int *mgr_env;
  char *env_block;
  float delta;

  flag = 0;
  snde_datum = -1;
  env_datum = -1;
  best_prio = -0x8000;

  player = 0;
  do {
    player_index = local_player_get_player_index((int16_t)player);
    if (player_index != -1) {
      camera = observer_get_camera((unsigned short)player);
      if (*(int16_t *)((char *)camera + 0x10) != -1) {
        if (!global_structure_bsp) {
          display_assert("global_structure_bsp",
                         "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xc5, 1);
          system_exit(-1);
        }
        bsp = (char *)global_structure_bsp;
        cluster = (char *)tag_block_get_element(
          (char *)global_structure_bsp + 0x134,
          (int)*(int16_t *)((char *)camera + 0x10), 0x68);
        node = FUN_0018f2d0((char *)camera + 0xc, camera);
        ref_index = scenario_get_structure_reference_index_from_tag_index(node);

        /* Branch A: fog-palette sound environment. */
        if (ref_index != -1) {
          fog_tag = (char *)tag_get(0x666f6720 /* 'fog ' */, ref_index);
          fog_snde = *(int *)(fog_tag + 0x110);
          if (fog_snde != -1 &&
              *(int16_t *)((char *)tag_get(0x736e6465 /* 'snde' */, fog_snde) +
                           4) > best_prio) {
            best_prio = *(int16_t *)((char *)tag_get(0x736e6465, fog_snde) + 4);
            env_datum = *(int32_t *)(fog_tag + 0x100);
            flag = (uint8_t)(*(uint8_t *)tag_get(0x666f6720, ref_index) & 1);
            snde_datum = fog_snde;
          }
        }

        /* Branch B: cluster sound-environment reference. */
        if (*(int16_t *)(cluster + 6) != -1) {
          env_element = (char *)tag_block_get_element(
            bsp + 0x208, (int)*(int16_t *)(cluster + 6), 0x50);
          cluster_snde = *(int *)(env_element + 0x2c);
          if (cluster_snde != -1 &&
              *(int16_t *)((char *)tag_get(0x736e6465, cluster_snde) + 4) >
                best_prio) {
            best_prio =
              *(int16_t *)((char *)tag_get(0x736e6465, cluster_snde) + 4);
            bg_ref = *(int16_t *)(cluster + 4);
            flag = 0;
            snde_datum = cluster_snde;
            if (bg_ref != -1 && (int)bg_ref < *(int *)(bsp + 0x1fc)) {
              bg_element =
                (char *)tag_block_get_element(bsp + 0x1fc, (int)bg_ref, 0x74);
              env_datum = *(int32_t *)(bg_element + 0x2c);
            } else {
              env_datum = -1;
            }
          }
        }
      }
    }
    player = player + 1;
  } while ((int16_t)player < 4);

  /* Optional debug print of the resolved environment name. */
  if (*(char *)0x5064cc != '\0') {
    if (snde_datum == -1) {
      env_name = "no sound environment";
    } else {
      env_name = tag_get_name(snde_datum);
    }
    crt_sprintf((char *)0x5ab100, "|n|n|n|n%s", env_name);
    FUN_00189c40(0, (char *)0x5ab100);
  }

  /* Source environment block: the resolved 'snde' tag, or module default. */
  if (snde_datum == -1) {
    env_src = (int *)0x2c1220;
  } else {
    env_src = (int *)tag_get(0x736e6465, snde_datum);
  }

  mgr = *(char **)0x5064d0;
  mgr_env = (int *)(mgr + 0xb8);

  if (flag != *(uint8_t *)(mgr + 0xb4)) {
    /* Environment changed: copy the whole 0x48-byte source block (18 dwords,
     * REP MOVSD in the original). */
    qmemcpy(mgr_env, env_src, 0x48);
    *(uint8_t *)(mgr + 0xb4) = flag;
    *out_changed = 1;
    *out_datum = env_datum;
    *out_env = (void *)(mgr + 0xb8);
  } else {
    /* Environment unchanged: ease the 12 env floats toward the source with
     * per-field min/max step clamps.  delta = source - current.  Both the
     * source (env_src) and destination (mgr_env) blocks are addressed by the
     * same dword index k (floats occupy dwords 2..13, i.e. block +0x8..+0x34).
     */
    env_block = mgr + 0xb8;
    delta = ((float *)env_src)[2] - ((float *)mgr_env)[2];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)env_block)[2] = delta + ((float *)env_block)[2];

    delta = ((float *)env_src)[3] - ((float *)mgr_env)[3];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)env_block)[3] = delta + ((float *)env_block)[3];

    delta = ((float *)env_src)[4] - ((float *)mgr_env)[4];
    if (delta < *(const float *)0x2546a0)
      delta = *(const float *)0x2546a0;
    else if (*(const float *)0x2533e4 < delta)
      delta = *(const float *)0x2533e4;
    ((float *)env_block)[4] = delta + ((float *)env_block)[4];

    delta = ((float *)env_src)[5] - ((float *)mgr_env)[5];
    if (delta < *(const float *)0x25e884)
      delta = *(const float *)0x25e884;
    else if (*(const float *)0x25496c < delta)
      delta = *(const float *)0x25496c;
    ((float *)mgr_env)[5] = delta + ((float *)mgr_env)[5];

    delta = ((float *)env_src)[6] - ((float *)mgr_env)[6];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)mgr_env)[6] = delta + ((float *)mgr_env)[6];

    delta = ((float *)env_src)[7] - ((float *)mgr_env)[7];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)mgr_env)[7] = delta + ((float *)mgr_env)[7];

    delta = ((float *)env_src)[8] - ((float *)mgr_env)[8];
    if (delta < *(const float *)0x2b2270)
      delta = *(const float *)0x2b2270;
    else if (*(const float *)0x2b226c < delta)
      delta = *(const float *)0x2b226c;
    ((float *)mgr_env)[8] = delta + ((float *)mgr_env)[8];

    delta = ((float *)env_src)[9] - ((float *)mgr_env)[9];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)mgr_env)[9] = delta + ((float *)mgr_env)[9];

    delta = ((float *)env_src)[10] - ((float *)mgr_env)[10];
    if (delta < *(const float *)0x2b2268)
      delta = *(const float *)0x2b2268;
    else if (*(const float *)0x2b2264 < delta)
      delta = *(const float *)0x2b2264;
    ((float *)mgr_env)[10] = delta + ((float *)mgr_env)[10];

    delta = ((float *)env_src)[11] - ((float *)mgr_env)[11];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)mgr_env)[11] = delta + ((float *)mgr_env)[11];

    delta = ((float *)env_src)[12] - ((float *)mgr_env)[12];
    if (delta < *(const float *)0x2b2274)
      delta = *(const float *)0x2b2274;
    else if (*(const float *)0x25bc08 < delta)
      delta = *(const float *)0x25bc08;
    ((float *)env_block)[12] = delta + ((float *)env_block)[12];

    delta = ((float *)env_src)[13] - ((float *)mgr_env)[13];
    if (delta < *(const float *)0x2b2260)
      delta = *(const float *)0x2b2260;
    else if (*(const float *)0x2b225c < delta)
      delta = *(const float *)0x2b225c;
    ((float *)env_block)[13] = delta + ((float *)env_block)[13];

    *out_changed = 0;
    *out_datum = env_datum;
    *out_env = env_block;
  }
}

/* 0x18fb20 — component-wise move-towards for a 3-float point. For each of the
 * three components, advance position toward target by at most max_delta:
 *   position[k] += clamp(target[k] - position[k], -max_delta, +max_delta)
 * Pure x87 leaf, no globals, no calls. Register ABI: position in ECX, target
 * in EDX, max_delta pushed on the stack (caller-cleaned). */
void FUN_0018fb20(float *position, const float *target, float max_delta)
{
  float delta;
  float neg_max;
  float clamped;

  neg_max = -max_delta;

  /* Branch polarity matches the original (TEST AH,0x5; JP = `delta <
   * neg_max` with the jump path taken on >= or unordered): NaN deltas take
   * the else path and propagate, exactly like the x87 code. */
  delta = target[0] - position[0];
  if (delta < neg_max) {
    clamped = neg_max;
  } else {
    clamped = delta;
    if (max_delta < delta) {
      clamped = max_delta;
    }
  }
  position[0] = clamped + position[0];

  delta = target[1] - position[1];
  if (delta < neg_max) {
    clamped = neg_max;
  } else {
    clamped = delta;
    if (max_delta < delta) {
      clamped = max_delta;
    }
  }
  position[1] = clamped + position[1];

  delta = target[2] - position[2];
  if (delta < neg_max) {
    clamped = neg_max;
  } else {
    clamped = delta;
    if (max_delta < delta) {
      clamped = max_delta;
    }
  }
  position[2] = clamped + position[2];
}

/* 0x18fbc0 — per-render-window atmospheric fog update. Resolves the 'sky '
 * tag for the current structure BSP (structure_bsp_index != -1 via
 * FUN_0018e770) or the scenario's first sky palette entry (index == -1,
 * resolve inlined), then smooths the per-window fog record at
 * *0x5064d0 + window_index*0x2c + 4 toward the sky's fog block:
 *   indoor (-1):  fog block = sky + 0x78, target opacity scale = 1.0 when
 *                 the sky's indoor-fog screen tag ref (+0xa4) is set, else 0
 *   outdoor:      fog block = sky + 0x58, target scale = 0
 * Record fields: +0 valid, +4 position[3] (last camera pos), +0x10/+0x14
 * max densities, +0x18 opaque distance, +0x1c color[3], +0x28 scale.
 * When the camera moved less than DAT_00254cc0 units and the record is
 * valid (and neither max density is 0), values are interpolated via
 * interpolate_scalar / FUN_0018fb20 at a rate proportional to the camera
 * distance; otherwise they snap. Finally publishes color, distances, the
 * blended max density, and the clamped [0,1] scale into the render fog
 * output block (out + 4 / +0x10 / +0x14 / +0x18 / +0x4c).
 * cdecl: window_index [EBP+8] (word read), structure_bsp_index [EBP+0xc]
 * (dword read, compared as DI), camera_position [EBP+0x10], out [EBP+0x14].
 * Both spare arg slots are reused as float spills by the original.
 * Confirmed: assert scenario.c:0xb7; 'sky ' = 0x736b7920; snap condition
 * chain from TEST AH,0x5/0x44 decode; smooth rate = dist, then
 * dist * DAT_002533e8 for distance/color/scale. Sole caller
 * render_window (render.c). */
void FUN_0018fbc0(int16_t window_index, int structure_bsp_index,
                  const float *camera_position, char *out)
{
  int sky_tag;
  int tag_ref;
  char *scnr;
  char *element;
  char *rec;
  float *fog;
  float target_scale;
  float dist;
  float dx;
  float dy;
  float dz;
  float blended;
  char local_rec[0x2c];

  if (*(int *)0x5064e4 == 0) {
    display_assert("global_scenario", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xb7, 1);
    system_exit(-1);
  }
  if ((int16_t)structure_bsp_index == -1) {
    /* default sky: first entry of the scenario sky palette (block +0x30) */
    if (*(int *)0x5064e4 == 0) {
      display_assert("global_scenario",
                     "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xb7, 1);
      system_exit(-1);
    }
    scnr = *(char **)0x5064e4;
    tag_ref = -1;
    if (0 < *(int *)(scnr + 0x30)) {
      element =
          (char *)tag_block_get_element((int *)(scnr + 0x30), 0, 0x10);
      tag_ref = *(int *)(element + 0xc);
    }
    sky_tag = 0;
    if (tag_ref != -1) {
      sky_tag = (int)tag_get(0x736b7920, tag_ref); /* 'sky ' */
    }
  } else {
    /* matches the inlined FUN_0018e7d0 dataflow: result accumulated in a
     * separate pointer (ECX in the reference), then handed to sky_tag (EAX) */
    void *sky_ptr;

    tag_ref = FUN_0018e770((int16_t)structure_bsp_index);
    sky_ptr = (void *)0;
    if (tag_ref != -1) {
      sky_ptr = tag_get(0x736b7920, tag_ref); /* 'sky ' */
    }
    sky_tag = (int)sky_ptr;
  }

  if (window_index != -1) {
    rec = *(char **)0x5064d0 + window_index * 0x2c + 4;
  } else {
    rec = local_rec;
  }

  if (sky_tag != 0) {
    if ((int16_t)structure_bsp_index == -1) {
      fog = (float *)(sky_tag + 0x78); /* indoor fog block */
      /* re-resolve the default sky to test its indoor-fog screen ref */
      if (*(int *)0x5064e4 == 0) {
        display_assert("global_scenario",
                       "c:\\halo\\SOURCE\\scenario\\scenario.c", 0xb7, 1);
        system_exit(-1);
      }
      scnr = *(char **)0x5064e4;
      tag_ref = -1;
      if (0 < *(int *)(scnr + 0x30)) {
        element =
            (char *)tag_block_get_element((int *)(scnr + 0x30), 0, 0x10);
        tag_ref = *(int *)(element + 0xc);
      }
      sky_tag = 0;
      if (tag_ref != -1) {
        sky_tag = (int)tag_get(0x736b7920, tag_ref); /* 'sky ' */
      }
      target_scale = 1.0f;
      if (*(int *)(sky_tag + 0xa4) == -1) {
        target_scale = 0.0f;
      }
    } else {
      fog = (float *)(sky_tag + 0x58); /* outdoor fog block */
      target_scale = 0.0f;
    }

    dx = camera_position[0] - *(float *)(rec + 4);
    dy = camera_position[1] - *(float *)(rec + 8);
    dz = camera_position[2] - *(float *)(rec + 0xc);
    dist = sqrtf(dx * dx + dy * dy + dz * dz);

    if (window_index != -1 && dist < *(const float *)0x254cc0 &&
        *rec != 0 && fog[7] != *(const float *)0x2533c0 &&
        *(float *)(rec + 0x14) != *(const float *)0x2533c0) {
      /* smooth: move each record field toward the sky's fog block */
      interpolate_scalar((float *)(rec + 0x10), fog[6], dist);
      interpolate_scalar((float *)(rec + 0x14), fog[7], dist);
      dist = dist * *(const float *)0x2533e8;
      interpolate_scalar((float *)(rec + 0x18), fog[5], dist);
      FUN_0018fb20((float *)(rec + 0x1c), fog, dist);
      interpolate_scalar((float *)(rec + 0x28), target_scale, dist);
    } else {
      /* snap */
      *(float *)(rec + 0x10) = fog[6];
      *(float *)(rec + 0x14) = fog[7];
      *(float *)(rec + 0x18) = fog[5];
      *(float *)(rec + 0x1c) = fog[0];
      *(float *)(rec + 0x20) = fog[1];
      *(float *)(rec + 0x24) = fog[2];
      *(float *)(rec + 0x28) = target_scale;
      *rec = 1;
    }
    *(float *)(rec + 4) = camera_position[0];
    *(float *)(rec + 8) = camera_position[1];
    *(float *)(rec + 0xc) = camera_position[2];
  }

  /* publish the record into the render fog output block */
  *(int *)(out + 4) = *(int *)(rec + 0x1c);
  *(int *)(out + 8) = *(int *)(rec + 0x20);
  *(int *)(out + 0xc) = *(int *)(rec + 0x24);
  *(int *)(out + 0x10) = *(int *)(rec + 0x18);
  *(int *)(out + 0x14) = *(int *)(rec + 0x10);

  if (*(float *)(rec + 0x14) != *(const float *)0x2533c0) {
    blended = *(float *)(rec + 0x10) + *(const float *)0x253f44;
    if (blended < *(float *)(rec + 0x14)) {
      blended = *(float *)(rec + 0x14);
    }
  } else {
    blended = *(const float *)0x2533c0;
  }
  *(float *)(out + 0x18) = blended;

  if (*(float *)(rec + 0x28) < *(const float *)0x2533c0) {
    *(int *)(out + 0x4c) = 0;
    return;
  }
  /* NaN scale must take the raw-copy path (original: TEST AH,0x41; JNZ →
   * unordered jumps to the copy). `1.0f < x` keeps that under clang; the
   * reference's JNZ vs our JP costs one VC71 diff insn. */
  if (*(const float *)0x2533c8 < *(float *)(rec + 0x28)) {
    *(float *)(out + 0x4c) = 1.0f;
    return;
  }
  *(int *)(out + 0x4c) = *(int *)(rec + 0x28);
}

/* 0x18fef0 — reset a scenario module's global byte flag at 0x5057c0 to 0. */
void FUN_0018fef0(void)
{
  *(char *)0x5057c0 = 0;
}
