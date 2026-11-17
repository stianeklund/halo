/* bipeds.c — biped-specific unit functions.
 *
 * Corresponds to bipeds.obj. Functions sorted by XBE address.
 * Debug assertion path: c:\halo\SOURCE\units\bipeds.c
 */

#include "../../common.h"
#include "../../x87_math.h"
#include "halo/math/rng_trace.h"
#include "bipeds.h"
#include "unit_definitions.h"
#include "units_globals.h"
#include "biped_definitions.h"
#include "game_globals_definitions.h"

/* animation_graph_t::nodes element (stride 0x40). */
typedef struct animation_graph_node_t {
  uint8_t pad_00[0x20];
  int16_t next_sibling_node_index;            /* +0x20 */
  int16_t first_child_node_index;             /* +0x22 */
  int16_t parent_node_index;                  /* +0x24 */
  uint8_t pad_26[2];
  uint32_t flags;                             /* +0x28 */
  uint8_t pad_2c[0x14];
} animation_graph_node_t;
cs(animation_graph_node_t, 0x40);
co(animation_graph_node_t, next_sibling_node_index, 0x20);
co(animation_graph_node_t, parent_node_index, 0x24);
co(animation_graph_node_t, flags, 0x28);

/* 'mode' tag definition; only the nodes block is modelled (size unproven). */
typedef struct model_definition_t {
  uint8_t pad_00[0xb8];
  tag_block nodes;                            /* +0xb8 */
} model_definition_t;
co(model_definition_t, nodes, 0xb8);

/* model_definition_t::nodes element (stride 0x9c). */
typedef struct model_node_t {
  uint8_t pad_00[0x44];
  real node_distance_from_parent;             /* +0x44 */
  uint8_t pad_48[0x54];
} model_node_t;
cs(model_node_t, 0x9c);
co(model_node_t, node_distance_from_parent, 0x44);

/* biped_limp_noodle_move_relax_and_constrain_positions (0x19fa20) — limp-noodle joint relaxation (dormant)
 *
 * Debug assertion path: c:\halo\SOURCE\units\biped_limp_noodle.c (lines
 * 0x13f / 0x212). Pushes collision user 0x12, clamps the animation-graph
 * noodle radius (antr+0x60) to 0.03 when |r| < 1e-4, r < 0 or r > 0.07, then
 * runs four passes of a breadth-first walk over the animation graph nodes.
 * Every non-root node is pushed out of collision along a parent->node ray and
 * pulled back toward its mode-tag rest length (mode node block +0xb8, 0x9c
 * stride, +0x44).
 *
 * Confirmed: early returns at biped+0x47d == 0, biped+0x47d >= 30 and
 * |frac| < 1e-4 skip the collision-user pop (original-binary behaviour; the
 * JBE/JNC/JNP all target the epilogue at 0x1a01bc, not the pop at 0x1a018b).
 * Confirmed: collision_test_sphere callers push 3 dwords (pos, radius, unit) but the
 * callee only reads [EBP+8]/[EBP+0xc]; the unit handle is an ignored extra.
 * Confirmed: 0x19f540 receives the proposed position in ESI (LEA ESI,[-0xc8]).
 * Confirmed: the |cur| >= 1e-4 test is evaluated twice (0x19ffbf FCOM then
 * 0x19fff7 FCOMP of the same ST0) — kept as in the binary.
 */
void biped_limp_noodle_move_relax_and_constrain_positions(int unit_handle, void *node_block) {
  int16_t queue[64];               /* [EBP-0x148] */
  char collisions[0x1b8];          /* [EBP-0x300] */
  real_point3d new_pos;                /* [EBP-0xc8] */
  real_vector3d new_vel;                /* [EBP-0xb4] */
  unsigned long bits[2];           /* [EBP-0xbc] */
  real_point3d ray_start;          /* [EBP-0xa8] */
  struct collision_result collision; /* [EBP-0x9c] */
  real_vector3d delta;             /* [EBP-0x18] */
  float step[3];                   /* [EBP-0x2c] */
  float dist[2];                   /* [EBP-0x38] */
  unsigned char touching[2];       /* [EBP-0x1c] */
  real_matrix4x3 *node_matrices = node_block;
  biped_data_t *biped;
  char *bipd;
  animation_graph_t *animation_graph;
  animation_graph_node_t *node;
  model_definition_t *model;
  model_node_t *model_node;
  float *node_pos;
  float *parent_pos;
  real_plane3d *plane;
  float *p;
  float radius;
  float frac;
  float rest;
  float cur;
  float nn;
  float s;
  int pass;
  int head;
  int tail;
  int k;
  int16_t node_index;
  unsigned char tick;
  unsigned char tick_count;
  union {
    float f;
    int i;
  } radius_bits; /* collision_test_sphere kb decl types the radius as int */

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  bipd = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(bipd));
  radius = animation_graph_definition_field_60(animation_graph);

  if (global_current_collision_user_depth >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\biped_limp_noodle.c", 0x13f, 1);
    system_exit(-1);
  }
  {
    int depth = (int)global_current_collision_user_depth;
    global_current_collision_user_depth += 1;
    collision_user_stack[depth] = 0x12;
  }

  if (fabs(radius) < REAL_EPSILON_DOUBLE_POOL || radius < 0.0f ||
      radius > 0.07f) {
    radius = 0.03f; /* 0x3cf5c28f */
  }

  tick_count = biped->limp_body_max_relaxation_iterations;
  if (tick_count == 0 || tick_count >= 0x1e)
    return;
  tick = biped->limp_body_current_relaxation_iterations;
  frac = (float)((int)tick + 1) / (float)(int)tick_count;
  if (fabs(frac) < REAL_EPSILON_DOUBLE_POOL)
    return;

  radius_bits.f = radius;
  if (tick < tick_count) {
    collision_get_features_in_sphere(0xc0a8, &biped->unit.object.position,
                 biped->unit.object.unk_92 + 0.0625f, 0.0f, radius,
                 unit_handle, (void *)0x4d9de8);
    csmemset(bits, 0, 8);

    for (pass = 0; pass < 4; pass++) {
      head = 0;
      tail = 1;
      queue[0] = 0;
      do {
        node_index = queue[(int16_t)head];
        head++;
        node = tag_block_get_element(&animation_graph->nodes, node_index,
                                     sizeof(animation_graph_node_t));
        if (node_index != 0) {
          step[0] = 0.0f;
          step[1] = 0.0f;
          step[2] = frac * -0.032086614f; /* 0x2b4b7c */
          node_pos = &node_matrices[node_index].position.x;
          parent_pos =
            &node_matrices[node->parent_node_index].position.x;
          delta.i = node_pos[0] - parent_pos[0];
          delta.j = node_pos[1] - parent_pos[1];
          delta.k = node_pos[2] - parent_pos[2];

          if (pass == 0 && !collision_test_sphere((int)node_pos, radius_bits.i)) {
            collision_move_point(node_pos, step, (short *)0x4d9de8, (float *)&new_pos, (float *)&new_vel,
                         3, (int)collisions);
            if (fabs(new_vel.i) < REAL_EPSILON_DOUBLE_POOL &&
                fabs(new_vel.j) < REAL_EPSILON_DOUBLE_POOL) {
              biped_limp_noodle_valid_joint_rotation(
                  (float *)&new_pos, unit_handle, node_index, node_block, node_pos,
                  bits);
            }
          }

          delta.i = node_pos[0] - parent_pos[0];
          delta.j = node_pos[1] - parent_pos[1];
          delta.k = node_pos[2] - parent_pos[2];
          ray_start.x = parent_pos[0] - delta.i * 0.015f; /* 0x25abcc */
          ray_start.y = parent_pos[1] - delta.j * 0.015f;
          ray_start.z = parent_pos[2] - delta.k * 0.015f;
          delta.i *= 1.03f; /* 0x2b4b78 */
          delta.j *= 1.03f;
          delta.k *= 1.03f;
          if (collision_test_vector(0xc0a8, &ray_start, &delta, unit_handle,
                           (int16_t *)&collision)) {
            touching[0] = (unsigned char)collision_test_sphere((int)node_pos,
                                                      0x3cf5c28f);
            touching[1] = (unsigned char)collision_test_sphere((int)parent_pos,
                                                      0x3cf5c28f);
            if ((int)touching[0] + (int)touching[1] != 0) {
              plane = &collision.plane;
              if ((int)touching[0] + (int)touching[1] == 2) {
                nn = plane->normal[1] * plane->normal[1] +
                     plane->normal[2] * plane->normal[2] +
                     plane->normal[0] * plane->normal[0];
                for (k = 0; k < 2; k++) {
                  p = (k == 0) ? node_pos : parent_pos;
                  dist[k] =
                    -((plane->normal[1] * p[1] + plane->normal[2] * p[2] +
                       plane->normal[0] * p[0] - plane->d) /
                      nn);
                  if (dist[k] != 0.0f)
                    dist[k] = radius * 2.5f + dist[k];
                }
              } else {
                for (k = 0; k < 2; k++) {
                  if (touching[k]) {
                    p = (k == 0) ? node_pos : parent_pos;
                    dist[k] =
                      -((plane->normal[1] * p[1] + plane->normal[2] * p[2] +
                         plane->normal[0] * p[0] - plane->d) /
                        (plane->normal[1] * plane->normal[1] +
                         plane->normal[2] * plane->normal[2] +
                         plane->normal[0] * plane->normal[0]));
                    if (dist[k] != 0.0f)
                      dist[k] = radius * 2.5f + dist[k];
                  } else {
                    dist[k] = 0.0f; /* MOV [EBP+ECX*4-0x38],EDX (EDX=0) */
                  }
                }
              }
              for (k = 0; k < 2; k++) {
                if (!(fabs(dist[k]) < REAL_EPSILON_DOUBLE_POOL)) {
                  p = (k == 0) ? node_pos : parent_pos;
                  s = frac * dist[k];
                  p[0] = plane->normal[0] * s + p[0];
                  p[1] = plane->normal[1] * s + p[1];
                  p[2] = plane->normal[2] * s + p[2];
                }
              }
            }
          }

          delta.i = node_pos[0] - parent_pos[0];
          delta.j = node_pos[1] - parent_pos[1];
          delta.k = node_pos[2] - parent_pos[2];
          model = tag_get(TAG_GROUP_MODE, object_definition_model_index(bipd));
          model_node = tag_block_get_element(&model->nodes, node_index,
                                             sizeof(model_node_t));
          rest = model_node->node_distance_from_parent;
          {
            float dx = parent_pos[0] - node_pos[0];
            float dy = parent_pos[1] - node_pos[1];
            float dz = parent_pos[2] - node_pos[2];
            cur = x87_sqrt(dz * dz + dy * dy + dx * dx);
          }
          /* rest <= 0 fails; NaN rest passes (TEST AH,0x41 / JNP). */
          if (!(rest <= 0.0f) && !(rest > 10.0f) && cur >= 0.0f &&
              cur < 20.0f && !(fabs(cur) < REAL_EPSILON_DOUBLE_POOL) &&
              !(fabs(rest) < REAL_EPSILON_DOUBLE_POOL) && rest != cur &&
              !(fabs(cur) < REAL_EPSILON_DOUBLE_POOL)) {
            float f = (rest - cur) / cur;
            if (node->parent_node_index != 0) {
              float h = f * 0.5f; /* 0x253398 */
              step[0] = delta.i * -h;
              step[1] = delta.j * -h;
              step[2] = delta.k * -h;
              collision_move_point(parent_pos, step, (short *)0x4d9de8, parent_pos,
                           step, 3, (int)collisions);
              step[0] = delta.i * h;
              step[1] = delta.j * h;
              step[2] = delta.k * h;
              collision_move_point(node_pos, step, (short *)0x4d9de8, node_pos, step,
                           3, (int)collisions);
            } else {
              step[0] = delta.i * f;
              step[1] = delta.j * f;
              step[2] = delta.k * f;
              if (!collision_test_sphere((int)node_pos, radius_bits.i)) {
                collision_move_point(node_pos, step, (short *)0x4d9de8, node_pos,
                             step, 3, (int)collisions);
              }
            }
          }
        }
        if (node->next_sibling_node_index != -1) {
          queue[(int16_t)tail] = node->next_sibling_node_index;
          tail++;
        }
        if (node->first_child_node_index != -1) {
          queue[(int16_t)tail] = node->first_child_node_index;
          tail++;
        }
      } while ((int16_t)head != (int16_t)tail);
    }
  }

  if (global_current_collision_user_depth <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\biped_limp_noodle.c", 0x212, 1);
    system_exit(-1);
  }
  global_current_collision_user_depth -= 1;
}



/* validate_real_vector3d_axes3 (0x1a01d0)
 *
 * Builds an orthonormal forward/left/up basis (biped_limp_noodle.c:0x217).
 * Each supplied vector is normalized; a zero-length vector is replaced by the
 * matching global world axis. The left and up vectors are then rebuilt via
 * cross products to enforce orthogonality, renormalizing (with axis fallback)
 * after each rebuild.
 *
 * Confirmed: CALL 0x13010 (normalize3d) returns the length; length 0.0 selects
 * the fallback axis. Globals: 0x31fc3c forward, 0x31fc40 left, 0x31fc44 up.
 * Cross products: left = up x forward, up = forward x left, left = up x forward
 * (operand order transcribed from disassembly).
 */
void validate_real_vector3d_axes3(real_vector3d *forward, real_vector3d *left,
                                  real_vector3d *up)
{
  float lc0;
  float lc1;
  float lc2;

  if (forward == NULL) {
    display_assert("forward", "c:\\halo\\SOURCE\\units\\biped_limp_noodle.c",
                   0x217, true);
    system_exit(-1);
  }
  if (left == NULL) {
    display_assert("left", "c:\\halo\\SOURCE\\units\\biped_limp_noodle.c",
                   0x217, true);
    system_exit(-1);
  }
  if (up == NULL) {
    display_assert("up", "c:\\halo\\SOURCE\\units\\biped_limp_noodle.c", 0x217,
                   true);
    system_exit(-1);
  }

  if (normalize3d(forward) == 0.0f) {
    forward->i = global_forward_vector_ptr[0];
    forward->j = global_forward_vector_ptr[1];
    forward->k = global_forward_vector_ptr[2];
  }
  if (normalize3d(up) == 0.0f) {
    up->i = global_up_vector_ptr[0];
    up->j = global_up_vector_ptr[1];
    up->k = global_up_vector_ptr[2];
  }

  /* left = up x forward. All three components are computed before any store
   * (the original cannot assume left[] doesn't alias forward[]/up[], so it
   * batches the cross product onto the x87 stack, then stores [0],[1],[2]). */
  lc2 = forward->j * up->i - up->j * forward->i;
  lc1 = up->k * forward->i - forward->k * up->i;
  lc0 = up->j * forward->k - up->k * forward->j;
  left->i = lc0;
  left->j = lc1;
  left->k = lc2;
  if (normalize3d(left) == 0.0f) {
    left->i = global_left_vector_ptr[0];
    left->j = global_left_vector_ptr[1];
    left->k = global_left_vector_ptr[2];
  }

  /* up = forward x left */
  lc2 = left->j * forward->i - forward->j * left->i;
  lc1 = forward->k * left->i - forward->i * left->k;
  lc0 = forward->j * left->k - left->j * forward->k;
  up->i = lc0;
  up->j = lc1;
  up->k = lc2;
  if (normalize3d(up) == 0.0f) {
    up->i = global_up_vector_ptr[0];
    up->j = global_up_vector_ptr[1];
    up->k = global_up_vector_ptr[2];
  }

  /* left = up x forward */
  lc2 = forward->j * up->i - up->j * forward->i;
  lc1 = up->k * forward->i - forward->k * up->i;
  lc0 = up->j * forward->k - up->k * forward->j;
  left->i = lc0;
  left->j = lc1;
  left->k = lc2;
  if (normalize3d(left) == 0.0f) {
    left->i = global_left_vector_ptr[0];
    left->j = global_left_vector_ptr[1];
    left->k = global_left_vector_ptr[2];
  }
}

/* biped_limp_noodle_adjust_orientations (0x1a03c0)
 *
 * unit_handle arrives in EAX and the node matrices in EDI; node_count is a
 * dead parameter (the body uses the animation graph node count). For each
 * non-root node whose parent is not flagged (flags bit 2), rotates the
 * parent's matrix basis by the angle between the relaxed position delta and
 * the current node delta, then re-validates the basis.
 */
void biped_limp_noodle_adjust_orientations(int unit_handle, int node_count,
                                           float *positions, void *nodes)
{
  biped_data_t *biped;
  void *bipd_tag;
  animation_graph_t *animation_graph;
  tag_block *block;
  int i;
  float *pfx; /* &positions[i].z  (EBX in original, +0x0c/iter)     */
  float *nfx; /* &node_matrices[i].position.z */
  animation_graph_node_t *elem;
  animation_graph_node_t *parent_elem;
  real_vector3d A; /* pos delta  — [EBP-0x38], normalized in-place */
  real_vector3d B; /* node delta — [EBP-0x2c], normalized in-place */
  real_vector3d cross; /* cross(A,B) — [EBP-0x44], normalized in-place */
  float dot_val; /* [EBP-0x4] */
  float angle; /* [EBP-0x14] */
  float sin_a;
  real_matrix4x3 *node_matrices;
  real_matrix4x3 *base;
  char axis_ok;
  float *saved_pfx; /* local_10 = [EBP-0xc] */

  node_matrices = nodes;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  bipd_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(bipd_tag));
  block = &animation_graph->nodes;

  if (block->count <= 0) {
    return;
  }

  /* pfx/nfx point at the z of slot 0 and advance by their strides. */
  pfx = (float *)((char *)positions + 8);
  nfx = &node_matrices[0].position.z;

  i = 0;
  do {
    if (i != 0) {
      float *parent_pos;
      float *parent_nod28;

      saved_pfx = pfx;

      elem = tag_block_get_element(block, i, sizeof(animation_graph_node_t));
      parent_elem = tag_block_get_element(block, elem->parent_node_index,
                                          sizeof(animation_graph_node_t));

      if (((uint8_t)parent_elem->flags & 4) == 0) {
        parent_pos =
          (float *)((char *)positions + elem->parent_node_index * 0x0c);
        parent_nod28 = &node_matrices[elem->parent_node_index].position.x;

        /* pos delta A = positions[i] - positions[parent] */
        A.i = pfx[-2] - parent_pos[0];
        A.j = pfx[-1] - parent_pos[1];
        A.k = *pfx - parent_pos[2];

        /* nod delta B = nodes[i]+0x28 - nodes[parent]+0x28 */
        B.i = nfx[-2] - parent_nod28[0];
        B.j = nfx[-1] - parent_nod28[1];
        B.k = nfx[0] - parent_nod28[2];

        normalize3d(&A);
        normalize3d(&B);

        /* cross(A,B) — operand order verified from FSUBP sequence:
         *   [0]: B.k*A.j - B.j*A.k = cross(A,B)[0]
         *   [1]: A.k*B.i - B.k*A.i = cross(A,B)[1]
         *   [2]: B.j*A.i - A.j*B.i = cross(A,B)[2]   */
        cross.i = B.k * A.j - B.j * A.k;
        cross.j = A.k * B.i - B.k * A.i;
        cross.k = B.j * A.i - A.j * B.i;

        normalize3d(&cross);

        /* dot(A,B) — FPU order: A.k*B.k + A.j*B.j + A.i*B.i */
        dot_val = A.k * B.k + A.j * B.j + A.i * B.i;

        /* if !(fabs(dot - 1.0f) < epsilon) */
        if (!(fabs(dot_val - REAL_ONE_POOL) < REAL_EPSILON_DOUBLE_POOL)) {
          angle = acosf(dot_val);

          /* if !(fabs(angle) < epsilon) && fabs(angle) < max_angle */
          if (!(fabs(angle) < REAL_EPSILON_DOUBLE_POOL) &&
              fabs(angle) < REAL_QUARTER_PI_DOUBLE_POOL) {
            /* The parent index is re-read at every call site. */

            /* First valid_real_vector3d_axes3 / validate_real_vector3d_axes3
             * block */
            base = &node_matrices[elem->parent_node_index];
            axis_ok = valid_real_vector3d_axes3(&base->forward,
                                                &base->left,
                                                &base->up);
            if (!axis_ok) {
              validate_real_vector3d_axes3(&base->forward,
                                           &base->left,
                                           &base->up);
            }

            sin_a = x87_fsin(angle);

            /* rotate forward and up */
            rotate_vector3d_by_sincos(
              &node_matrices[elem->parent_node_index].forward,
              &cross, sin_a, dot_val);
            rotate_vector3d_by_sincos(
              &node_matrices[elem->parent_node_index].up,
              &cross, sin_a, dot_val);

            normalize3d(&node_matrices[elem->parent_node_index].forward);
            normalize3d(&node_matrices[elem->parent_node_index].up);

            /* cross_product3d(up, fwd, left) — uses single base */
            base = &node_matrices[elem->parent_node_index];
            cross_product3d(&base->up, &base->forward,
                            &base->left);

            normalize3d(&node_matrices[elem->parent_node_index].left);

            /* Second valid_real_vector3d_axes3 / validate_real_vector3d_axes3
             * block */
            base = &node_matrices[elem->parent_node_index];
            axis_ok = valid_real_vector3d_axes3(&base->forward,
                                                &base->left,
                                                &base->up);
            if (!axis_ok) {
              validate_real_vector3d_axes3(&base->forward,
                                           &base->left,
                                           &base->up);
              pfx = saved_pfx;
            }
            pfx = saved_pfx;
          }
        }
      }
    }

    i++;
    pfx = pfx + 3; /* +0x0c bytes (3 floats per position) */
    nfx = nfx + sizeof(real_matrix4x3) / sizeof(float);
  } while (i < block->count);
}

/* biped_limp_noodle_relax_nodes_onto_environment (0x1a0680)
 *
 * Updates biped limp-noodle (ragdoll/physics) node positions. Gets the biped
 * object and its 'bipd' tag, then looks up the 'antr' animation tag via the
 * tag reference at bipd+0x44. Fetches the object header block reference at
 * offset object+0x1a0 (the node transform block). Returns 1 if the current
 * node step count (byte at biped_obj+0x47c) >= the maximum step count (byte
 * at biped_obj+0x47d); otherwise copies node positions into the scratch buffer
 * at 0x4e49f0, calls biped_limp_noodle_move_relax_and_constrain_positions to process limp-noodle physics, calls
 * biped_limp_noodle_adjust_orientations to apply the updated positions back,
 * increments the step counter (capped at 0x7f), and returns 0.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) type 1 (biped).
 * Confirmed: CALL 0x1ba140 (tag_get) 'bipd' (0x62697064) then 'antr'
 * (0x616e7472). Confirmed: CALL 0x13dfc0 (object_header_block_reference_get)
 * offset +0x1a0. Confirmed: comparison [ESI+0x47c] vs [ESI+0x47d] (CMP/SBB/INC
 * pattern). Confirmed: node stride 0x34 (13 floats), position offset +0x28
 * within node. Confirmed: scratch buffer at 0x4e49f0 (DAT_004e49f0). Confirmed:
 * antr node count at antr+0x68. Confirmed: step counter cap at 0x7f (JNC =
 * unsigned compare). Inferred: biped_limp_noodle_adjust_orientations
 * @<eax>=unit_handle,
 * @<edi>=node_block_ptr.
 */
char biped_limp_noodle_relax_nodes_onto_environment(int unit_handle)
{
  biped_data_t *biped;
  char *bipd_tag;
  animation_graph_t *animation_graph;
  void *node_block;
  object_data_t *object;
  int count;
  int i;
  int *src_node;
  int *scratch;
  char result;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  bipd_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  animation_graph = tag_get(TAG_GROUP_ANTR, object_definition_animation_graph_index(bipd_tag));
  object = object_get_and_verify_type(unit_handle, _object_mask_all);
  node_block = object_header_block_reference_get(unit_handle, &object->unk_416);

  /* CMP [ESI+0x47c],[ESI+0x47d]; SBB CL,CL; INC CL */
  result = (char)(biped->limp_body_current_relaxation_iterations >=
                  biped->limp_body_max_relaxation_iterations);
  if (result != '\0') {
    return result;
  }

  count = animation_graph->nodes.count;
  if (count > 0) {
    scratch = (int *)0x4e49f0;
    src_node = (int *)((char *)node_block + 0x28);
    i = 0;
    do {
      scratch[0] = src_node[0];
      scratch[1] = src_node[1];
      scratch[2] = src_node[2];
      i++;
      src_node += (0x34 / 4); /* advance by 0x34 bytes (13 ints) */
      scratch += 3;
    } while (i < count);
  }

  biped_limp_noodle_move_relax_and_constrain_positions(unit_handle, node_block);
  /* @<eax>=unit_handle, node_count, positions=scratch buf, @<edi>=node_block */
  biped_limp_noodle_adjust_orientations(unit_handle,
                                        animation_graph->nodes.count,
                                        (float *)0x4e49f0, node_block);

  if (biped->limp_body_current_relaxation_iterations < 0x7f) {
    biped->limp_body_current_relaxation_iterations =
      biped->limp_body_current_relaxation_iterations + 1;
  }
  return result;
}

/* bipeds_initialize_for_new_map (0x1a0790)
 *
 * Confirmed: the body is a single RET (C3).
 * Confirmed: only reference is the biped object_type_definition table at
 *   0x323d48, slot +0x18 (initialize_for_new_map); name from PAL 2342
 *   objects/object_types.c biped_data_definition (T2). */
void bipeds_initialize_for_new_map(void)
{
}

/* bipeds_dispose_from_old_map (0x1a07a0)
 *
 * Confirmed: the body is a single RET (C3).
 * Confirmed: only reference is the biped object_type_definition table at
 *   0x323d48, slot +0x1c (dispose_from_old_map); name from PAL 2342 (T2). */
void bipeds_dispose_from_old_map(void)
{
}

/* biped_place (0x1a07c0)
 *
 * Places a biped unit at a placement location. Calls unit_place with the
 * placement data at offset +0x48 (the unit-level placement struct), then
 * calls object_add_scenario_permutation (no-op stub) with the placement data at
 * offset +0x28.
 *
 * Confirmed: param_2+0x48 passed to unit_place (PUSH ESI+0x48 via LEA).
 * Confirmed: param_2+0x28 passed to object_add_scenario_permutation (ADD
 * ESI,0x28; PUSH ESI). Confirmed: object_add_scenario_permutation is a no-op
 * (single RET). Inferred: param_2 is a biped placement struct (unit placement
 * at +0x48, biped-specific data at +0x28).
 */
void biped_place(int unit_handle, void *placement)
{
  unit_place(unit_handle, (char *)placement + 0x48);
  object_add_scenario_permutation(unit_handle, (char *)placement + 0x28);
}

/* biped_reset (0x1a0800)
 *
 * Resets biped physics/state fields to default values. Clears 0x5c bytes
 * starting at obj+0x424, then restores four default float constants from
 * 0x32513c..0x325148 into obj+0x46c..0x478, and sets obj+0x450 = -1.
 *
 * Confirmed: csmemset(obj+0x424, 0, 0x5c) clears the full state block.
 * Confirmed: store offsets from disasm: obj+0x46c, +0x470, +0x474, +0x478
 *   loaded from globals 0x32513c, 0x325140, 0x325144, 0x325148.
 * Confirmed: obj+0x450 = 0xffffffff (OR ECX,-1 pattern; int -1).
 * Confirmed: MSVC schedules obj+0x450 between the 3rd and 4th const stores;
 *   written naturally here, VC71 reschedules.
 * Inferred: the four constants are default float values (0.0, 0.0, 1.0,
 *   -256.0 from memory at 0x32513c).
 */
void biped_reset(int unit_handle)
{
  biped_data_t *biped;
  int *dst;
  int tmp0;
  int tmp1;
  int tmp2;
  int tmp3;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  csmemset(&biped->flags, 0, 0x5c);
  tmp0 = DEPTHS_OF_HELL_PLANE_NORMAL_I_BITS;
  dst = (int *)&biped->ground_plane;
  tmp1 = DEPTHS_OF_HELL_PLANE_NORMAL_J_BITS;
  dst[0] = tmp0;
  tmp2 = DEPTHS_OF_HELL_PLANE_NORMAL_K_BITS;
  dst[1] = tmp1;
  tmp3 = DEPTHS_OF_HELL_PLANE_D_BITS;
  biped->last_falling_communication_time = NONE;
  dst[2] = tmp2;
  dst[3] = tmp3;
}

/* biped_disconnect_from_structure_bsp (0x1a0860)
 *
 * Clears the biped's BSP leaf/cluster/surface datum handle fields to -1,
 * disconnecting the unit from the current structure BSP.
 *
 * Confirmed: obj+0x430 = -1, obj+0x434 = -1, obj+0x448 = -1 (OR ECX,-1
 * pattern). Confirmed: uses same ECX=-1 value for all three stores (single OR
 * ECX,-1). Uncertain: exact field names at 0x430, 0x434, 0x448.
 */
void biped_disconnect_from_structure_bsp(int unit_handle)
{
  biped_data_t *biped;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped->support_surface_index = NONE;
  biped->pathfinding_surface_index = NONE;
  biped->last_pathfinding_surface_index = NONE;
}

/* biped_get_camera_height_and_offset (0x1a0890)
 *
 * Computes camera height parameters for a biped unit. Gets the world position
 * into out_pos, optionally adds the biped tag's camera height to the Z
 * coordinate, then calculates a height offset based on the biped's crouch
 * state and tag parameters.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type 1 (biped
 * only). Confirmed: CALL 0x1ba140 (tag_get) with 'bipd' signature (0x62697064).
 * Confirmed: CALL 0x1412f0 (object_get_world_position).
 * Confirmed: Tag offsets 0x2f4 (flags), 0x424 (crouch height), 0x428 (stand
 * height), 0x42c (camera height). Confirmed: Unit offset 0x1c8 (unk_456 datum
 * handle), 0x4 (object flags), 0x464 (crouch fraction).
 */
void biped_get_camera_height_and_offset(int unit_handle, vector3_t *out_pos,
                                        float *out_height_offset,
                                        float *out_camera_height)
{
  biped_data_t *biped;
  biped_definition_t *biped_tag;
  float camera_height;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  object_get_world_position(unit_handle, out_pos);

  /* Add camera height to Z position unless flag bit 3 is set */
  if (((uint8_t)biped_tag->biped_flags & 0x8) == 0) {
    out_pos->z = biped_definition_collision_radius(biped_tag) + out_pos->z;
  }

  /* Calculate height offset based on crouch state */
  if (((uint8_t)biped_tag->biped_flags & 0x10) == 0 &&
      (biped->unit.player_index.value != NONE ||
       (biped->unit.object.flags & 0x400000) != 0)) {
    /* Unit has actor or is player-controlled: interpolate based on crouch.
     * Memory-access order matches the original: standing/crouching heights and
     * crouch fraction are read inline; camera_height is cached once so the
     * doubling compiles to FADD ST0,ST0; out_camera_height re-reads [0x42c]. */
    camera_height = biped_definition_collision_radius(biped_tag);
    *out_height_offset = (biped_definition_collision_height_crouching(biped_tag) -
                          biped_definition_collision_height_standing(biped_tag)) *
                           biped->crouch +
                         biped_definition_collision_height_standing(biped_tag) -
                         (camera_height + camera_height);
    *out_camera_height = biped_definition_collision_radius(biped_tag);
  } else {
    /* No actor and not player-controlled: zero offset */
    *out_height_offset = 0.0f;
    *out_camera_height = biped_definition_collision_radius(biped_tag);
  }
}

/* biped_stop_melee_attack (0x1a0950)
 *
 * Clears the melee-attack active flag (obj+0x45d = 0) on the biped's unit
 * object, if the object exists. Uses object_try_and_get_and_verify_type (does
 * not assert on missing object).
 *
 * Confirmed: CALL 0x13d640 (object_try_and_get_and_verify_type) with type 1.
 * Confirmed: MOV byte ptr [EAX+0x45d],0x0 if result != NULL.
 * Inferred: "stop melee" semantics from flag offset matching biped_start_limp
 *   sequence and the clear-vs-set pattern.
 */
void biped_stop_melee_attack(int unit_handle)
{
  biped_data_t *biped;

  biped =
    object_try_and_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  if (biped != NULL) {
    biped->player_melee_ticks = 0;
  }
}

/* biped_start_limp_body_physics (0x1a0970)
 *
 * Begins limp-body (ragdoll-like) physics simulation for a biped. Checks
 * several preconditions against the tag flags and object state before
 * activating: requires the limp-body tag flag (tag+0x2f4 bit 9 set), the
 * biped must have a velocity (obj+0x4 bit 5), must not already be in ragdoll
 * (obj+0x424 bits 0x21), and the global freeze flag must be clear.
 * On entry it clears obj+0x47c (sub-step counter) and stores the unit
 * definition type constant (0x14) in obj+0x47d, then ORs obj+0x4 with
 * 0x800000 and obj+0x424 with 0x20.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) type 1.
 * Confirmed: CALL 0x1ba140 (tag_get) 'bipd'.
 * Confirmed: DAT_004e4cf3 = global freeze flag (byte).
 * Confirmed: tag+0x2f4 >> 9 & 1 = limp-body enabled flag.
 * Confirmed: obj+0x4 bit 0x20 = has-velocity; obj+0x424 bits 0x21 = already
 * ragdoll. Confirmed: CALL 0x19f530 with unit_handle arg → returns 0x14 (unit
 * def type const). Confirmed: stores return value (0x14) in obj+0x47d.
 * Inferred: obj+0x47c is a ragdoll sub-frame counter, cleared on start.
 */
void biped_start_limp_body_physics(int unit_handle)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  uint8_t max_iterations;
  uint32_t object_flags;
  uint32_t biped_flags;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  if (DEBUG_BIPED_LIMP_BODY_DISABLE_BYTE != 0) {
    return;
  }
  if ((definition->biped_flags >> 9 & 1) == 0) {
    return;
  }
  if (((uint8_t)biped->unit.object.flags & 0x20) == 0) {
    return;
  }
  if (((uint8_t)biped->flags & 0x21) != 0) {
    return;
  }
  biped->limp_body_current_relaxation_iterations = 0;
  max_iterations = (uint8_t)biped_limp_noodle_get_max_relaxation_iterations(unit_handle);
  biped->limp_body_max_relaxation_iterations = max_iterations;
  object_flags = biped->unit.object.flags;
  biped_flags = biped->flags;
  object_flags |= 0x800000;
  biped_flags |= 0x20;
  biped->unit.object.flags = object_flags;
  biped->flags = biped_flags;
}

/* biped_stop_limp_body_physics (0x1a09f0)
 *
 * Ends limp-body physics simulation if preconditions are met: the tag must
 * have the limp-body flag (tag+0x2f4 bit 9 = CH bit), and the ragdoll-active
 * flag (obj+0x424 bit 5 = 0x20) must be set. On success, clears both flags.
 *
 * Confirmed: TEST CH,0x2 checks bit 9 of the dword at tag+0x2f4 (CH = byte 1
 *   of dword; bit 1 of CH = bit 9 of dword).
 * Confirmed: AND [ESI+4],0xff7fffff clears obj+0x4 bit 23 (0x800000).
 * Confirmed: AND [ESI+0x424],0xffffffdf clears bit 5 (0x20).
 * Inferred: mirror of biped_start_limp_body_physics.
 */
void biped_stop_limp_body_physics(int unit_handle)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  uint32_t definition_flags;
  uint32_t object_flags;
  uint32_t biped_flags;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  definition_flags = definition->biped_flags;
  if ((definition_flags & 0x200) == 0) {
    return;
  }
  biped_flags = biped->flags;
  if ((biped_flags & 0x20) == 0) {
    return;
  }
  object_flags = biped->unit.object.flags & 0xff7fffff;
  biped_flags = biped_flags & 0xffffffdf;
  biped->unit.object.flags = object_flags;
  biped->flags = biped_flags;
}

/* biped_bumped_object (0x1a0a40)
 *
 * Per-frame melee-contact/bump handler for bipeds. Tracks a timer (obj+0x458)
 * and triggers AI bump notification and optional player-unit reassignment.
 *
 * Register args (confirmed from caller biped_update_moving at 0x1a61ba/0x1a61c4):
 *   @edi  unit_handle  — biped datum handle
 *   @ebx  biped_obj    — pointer to biped object data (pre-fetched by caller)
 * Stack arg:
 *   velocity_ptr  [EBP+0x8]  — pointer to the biped's velocity vector (3
 * floats)
 *
 * Logic:
 *   If timer (obj+0x458) is negative (counting up through negatives):
 *     - If no new contact (EDI == -1): just increment timer and return.
 *     - Else: fall through to common exit (set timer to 0xf1).
 *   If timer >= 0 and no new contact (EDI == -1): early return (nothing to do).
 *   Otherwise:
 *     - Get the contact object (EDI, type -1).
 *     - Call ai_handle_bump(biped_handle, contact_handle, velocity_ptr).
 *     - If vehicle slot (obj+0x1c8) != -1 OR recorded-animation controlling:
 *         If obj+0x454 (last contact) != new contact: store new contact, clear
 * timer, return. Else: increment timer; if timer reaches 4 (> 3): If in
 * multiplayer + has local player: set obj+0x458=0xf1, call
 * players_set_local_player_unit. Fall-through/exit: set obj+0x458 = 0xf1.
 *
 * Confirmed: obj+0x458 = bump_timer (int8_t); obj+0x1c8 = vehicle_datum handle.
 * Confirmed: obj+0x454 = last_contact handle; obj+0x1c8 = vehicle slot.
 * Confirmed: DAT_005aa893 = multiplayer flag.
 * Confirmed: CALL 0x94ff0 (recorded_animation_controlling_unit).
 * Confirmed: CALL 0xb6990 (unit_get_local_player_index), returns int16_t (-1 =
 * none). Confirmed: CALL 0xba5f0 (players_set_local_player_unit(player_idx,
 * unit_handle)). Confirmed: [EDX+0x64] word = some vehicle-seat type field
 * checked == 0. Inferred: 0xf1 timer value = "reset/cleared" sentinel.
 */
void biped_bumped_object(int contact_handle /* @edi */,
                         int unit_handle /* @ebx */, real_vector3d *velocity_ptr)
{
  biped_data_t *biped;
  biped_data_t *contact;
  int8_t timer;
  int16_t player_index;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  timer = biped->bump_ticks;
  if (timer < 0) {
    if (contact_handle == NONE) {
      biped->bump_ticks = timer + 1;
      return;
    }
  } else {
    if (contact_handle == NONE) {
      return;
    }
    /* Any object type; bump_ticks is only written once it is a biped. */
    contact = object_get_and_verify_type(contact_handle, _object_mask_all);
    ai_handle_bump(unit_handle, contact_handle, velocity_ptr);
    if ((biped->unit.player_index.value == NONE) &&
        (recorded_animation_controlling_unit(unit_handle) == 0)) {
      return;
    }
    if (biped->bump_object_index != contact_handle) {
      biped->bump_object_index = contact_handle;
      biped->bump_ticks = 0;
      return;
    }
    timer = biped->bump_ticks;
    timer++;
    biped->bump_ticks = timer;
    if (timer <= 3) {
      return;
    }
    if ((contact->unit.object.type == _object_type_biped) &&
        (CHEAT_BUMP_POSSESSION != 0)) {
      player_index = unit_get_local_player_index(unit_handle);
      if (player_index != NONE) {
        contact->bump_ticks = (int8_t)0xf1;
        players_set_local_player_unit(player_index, unit_handle);
      }
    }
  }
  biped->bump_ticks = (int8_t)0xf1;
}

/* biped_check_discard (0x1a0b30)
 *
 * Checks whether a biped is stuck in an unreachable (bad) position and erases
 * it if so. Triggered once per update frame from the biped update loop.
 *
 * Register arg (confirmed from caller biped_update at 0x1a6727):
 *   @edi  unit_handle  — biped datum handle
 *
 * Conditions to erase (all must hold):
 *   1. game_engine_running() == false (not in a game engine / network game)
 *   2. Either obj+0x4 bit 0x200000 set (flying/physics flag) OR obj+0x4c == -1
 *      (no parent handle)
 *   3. obj+0x14 (velocity Z / speed float) < DAT_002b4d1c (negative-infinity or
 *      very-small threshold — biped is sinking/below world)
 *
 * On erase:
 *   Looks up actor (obj+0x1a8 or fallback obj+0x1a4), calls
 *   ai_debug_describe_actor for the warning string, prints WARNING via error(),
 *   then calls object_delete(unit_handle).
 *
 * Confirmed: FCOMP [0x2b4d1c], TEST AH,0x5, JP = "< with NaN" check.
 * Confirmed: PUSH 0x100 / PUSH 0x5ab100 / PUSH 0x1 / PUSH EDI / PUSH EAX =
 *   ai_debug_describe_actor(actor_handle, unit_handle, 1, buf@0x5ab100, 0x100).
 * Confirmed: PUSH EAX / PUSH ECX / CALL 0x1ba1f0 = tag_get_name(tag_index).
 * Confirmed: warning format string at 0x2b4cd8.
 * Returns 0 (char) always.
 * Inferred: function name from warning message "biped %s (%s) is in a bad
 * place".
 */
char biped_check_discard(int unit_handle /* @edi */)
{
  biped_data_t *biped;
  int actor_handle;
  const char *actor_desc;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  if (game_engine_running() != 0) {
    return 0;
  }
  if ((biped->unit.object.flags & 0x200000) == 0 &&
      biped->unit.object.location.cluster_index != NONE) {
    return 0;
  }
  if (!(biped->unit.object.position.z < REAL_NEG_2000_POOL)) {
    return 0;
  }
  actor_handle = biped->unit.swarm_actor_index.value;
  if (actor_handle == NONE) {
    actor_handle = biped->unit.actor_index.value;
  }
  actor_desc = ai_debug_describe_actor(actor_handle, unit_handle, 1,
                                       (char *)0x5ab100, 0x100);
  /* The binary passes actor_desc after the three coordinates (argument
   * order differs from the format string); reproduced as compiled. */
  error(2, "WARNING: biped %s (%s) is in a bad place (%.1f %.1f %.1f), erasing",
        tag_name_strip_path(tag_get_name(biped->unit.object.tag_index)),
        (double)biped->unit.object.position.x,
        (double)biped->unit.object.position.y,
        (double)biped->unit.object.position.z, actor_desc);
  object_delete(unit_handle);
  return 0;
}

/* Game globals falling_damage block element (globals +0x188, element size
 * 0x98). */
typedef struct game_globals_falling_damage_t {
  uint8_t pad_00[0x10];
  tag_reference falling_damage;               /* +0x10 */
  uint8_t pad_20[0x0c];
  tag_reference maximum_distance_damage;      /* +0x2c */
  uint8_t pad_3c[0x50];
  real runtime_maximum_falling_velocity;      /* +0x8c */
  real runtime_minimum_damage_velocity;       /* +0x90 */
  real runtime_maximum_damage_velocity;       /* +0x94 */
} game_globals_falling_damage_t;
cs(game_globals_falling_damage_t, 0x98);
co(game_globals_falling_damage_t, falling_damage, 0x10);
co(game_globals_falling_damage_t, maximum_distance_damage, 0x2c);
co(game_globals_falling_damage_t, runtime_maximum_falling_velocity, 0x8c);
co(game_globals_falling_damage_t, runtime_minimum_damage_velocity, 0x90);
co(game_globals_falling_damage_t, runtime_maximum_damage_velocity, 0x94);

/* biped_falling_damage (0x1a0be0)
 *
 * unit_handle arrives in EDI; landing_velocity is the landing velocity pushed by
 * biped_update_moving. Above the minimum damage velocity the biped takes
 * scaled falling damage; a biped falling faster than the maximum falling
 * velocity takes maximum-distance damage and, outside a game engine, is
 * erased with a "fell outside world" warning.
 */
void biped_falling_damage(float landing_velocity, int unit_handle /* @edi */)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  game_globals_falling_damage_t *falling_damage;
  struct damage_data damage;
  float t;
  int actor_handle;
  int player_index;
  const char *tag_name;
  const char *actor_desc;
  char no_falling_damage;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  falling_damage =
    tag_block_get_element(&game_globals_falling_damage(game_globals_get()), 0,
                          sizeof(game_globals_falling_damage_t));

  if ((biped->unit.flags & 0x1000) != 0 ||
      (int8_t)definition->biped_flags < 0) {
    no_falling_damage = 1;
  } else {
    no_falling_damage = 0;
  }

  if (CHEAT_JETPACK_BYTE != 0 && biped->unit.player_index.value != NONE) {
    return;
  }

  if (landing_velocity > falling_damage->runtime_minimum_damage_velocity) {
    if (no_falling_damage) {
      return;
    }
    damage_data_new(&damage, falling_damage->falling_damage.tag_index);
    t = (landing_velocity - falling_damage->runtime_minimum_damage_velocity) /
        (falling_damage->runtime_maximum_damage_velocity -
         falling_damage->runtime_minimum_damage_velocity);
    damage.scale = t;
    if (t < 0.0f) {
      damage.scale = 0.0f;
    } else if (t > 1.0f) {
      damage.scale = 1.0f;
    }
    object_cause_damage(&damage, unit_handle, -1, -1, -1, (float *)0);
    return;
  }

  if (((uint8_t)definition->biped_flags & 0x4) != 0) {
    return;
  }
  if (!(biped->unit.object.translational_velocity.k <
        -(falling_damage->runtime_maximum_falling_velocity))) {
    return;
  }
  if (!no_falling_damage && (biped->unit.object.damage_flags & 0x4) == 0) {
    damage_data_new(&damage, falling_damage->maximum_distance_damage.tag_index);
    object_cause_damage(&damage, unit_handle, -1, -1, -1, (float *)0);
  }
  if (game_engine_running() != 0) {
    return;
  }
  if ((biped->unit.object.flags & 0x200000) == 0) {
    return;
  }
  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != NONE) {
    return;
  }
  actor_handle = biped->unit.swarm_actor_index.value;
  if (actor_handle == NONE) {
    actor_handle = biped->unit.actor_index.value;
  }
  actor_desc = ai_debug_describe_actor(actor_handle, unit_handle, 1,
                                       (char *)0x5ab100, 0x100);
  tag_name = tag_get_name(biped->unit.object.tag_index);
  tag_name = tag_name_strip_path(tag_name);
  error(2, "WARNING: biped %s (%s) fell outside world and was erased", tag_name,
        actor_desc);
  object_delete(unit_handle);
}

/* biped_flying_through_air (0x1a0db0)
 *
 * Predicate: returns 1 if the biped is considered airborne (flying through the
 * air) and should run air physics. True when the airborne-frame counter
 * (unit+0x459) exceeds 3 AND either the biped tag does not have the "no
 * airborne" flag (tag+0x2f4 bit 2) or the unit has the override flag
 * (unit+0xb6 bit 2).
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd', ...).
 * unit+0x459 signed-char counter compared > 3; tag+0x2f4 bit2; unit+0xb6 bit2.
 */
int biped_flying_through_air(int unit_handle)
{
  biped_data_t *biped;
  biped_definition_t *definition;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  if (biped->airborne_ticks > 3 &&
      ((((uint8_t)definition->biped_flags & 4) == 0) ||
       ((biped->unit.object.damage_flags & 4) != 0))) {
    return 1;
  }
  return 0;
}

/* biped_start_landing (0x1a0e00)
 *
 * unit_handle arrives in EAX; landing_velocity is the cdecl stack argument.
 * Tag +0x3dc/+0x3e0/+0x3e4 hold the soft/hard/maximum landing velocities
 * (per second) and +0x3d4/+0x3d8 the soft/hard landing times (seconds).
 */
void biped_start_landing(float landing_velocity, int unit_handle)
{
  biped_data_t *biped;
  char *biped_tag;
  float minimum_soft_landing_velocity;
  float minimum_hard_landing_velocity;
  float velocity_range;
  float velocity;
  float recovery_time;
  float scaled_recovery_time;
  float t;
  int landing_type;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  minimum_soft_landing_velocity =
    biped_definition_minimum_soft_landing_velocity(biped_tag) * REAL_ONE_THIRTIETH_POOL;
  minimum_hard_landing_velocity =
    biped_definition_minimum_hard_landing_velocity(biped_tag) * REAL_ONE_THIRTIETH_POOL;

  if (landing_velocity < minimum_soft_landing_velocity) {
    return;
  }

  if (landing_velocity < minimum_hard_landing_velocity) {
    velocity = landing_velocity - minimum_soft_landing_velocity;
    velocity_range =
      minimum_hard_landing_velocity - minimum_soft_landing_velocity;
    recovery_time = biped_definition_maximum_soft_landing_time(biped_tag);
    landing_type = 0;
  } else {
    velocity_range = biped_definition_maximum_hard_landing_velocity(biped_tag) * REAL_ONE_THIRTIETH_POOL -
                     minimum_hard_landing_velocity;
    velocity = landing_velocity;
    recovery_time = biped_definition_maximum_hard_landing_time(biped_tag);
    landing_type = 1;
  }
  scaled_recovery_time = recovery_time * REAL_30_POOL;
  if (velocity_range > REAL_ZERO_POOL) {
    t = velocity / velocity_range;
    if (t < REAL_ZERO_POOL) {
      t = REAL_ZERO_POOL;
    } else if (t > REAL_ONE_POOL) {
      t = REAL_ONE_POOL;
    }
    biped->landing = (int16_t)landing_type;
    biped->landing_recovery_counter = 0;
    biped->landing_recovery_time = (int8_t)(int)(scaled_recovery_time * t);
  }
}

/* Biped definition contact_points element (stride 0x40). */
typedef struct biped_contact_point_t {
  uint8_t pad_00[0x20];
  char marker_name[0x20];                     /* +0x20 */
} biped_contact_point_t;
cs(biped_contact_point_t, 0x40);
co(biped_contact_point_t, marker_name, 0x20);

/* biped_make_footstep (0x1a0f10)
 *
 * Spawns the biped's footstep material effect (tag +0x398) at the marker
 * named by contact point `index` (tag contact_points block at +0x4e8),
 * bracketed by collision user 7. index arrives in BX (MOVSX EBX,BX at
 * 0x1a0f73); param_2 is forwarded unchanged to
 * material_effect_new_from_point (callers pass 3 or 4).
 */
void biped_make_footstep(int unit_handle, int param_2, short index /* @bx */)
{
  biped_data_t *biped;
  char *biped_tag;
  int16_t depth;
  biped_contact_point_t *contact_point;
  object_marker marker;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  if (global_current_collision_user_depth >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf4f, true);
    system_exit(-1);
  }
  depth = global_current_collision_user_depth;
  collision_user_stack[depth] = 7;
  global_current_collision_user_depth = depth + 1;

  if (((int)index < biped_definition_contact_points(biped_tag).count) &&
      (biped_definition_material_effects_index(biped_tag) != -1)) {
    if (material_effect_visible(&biped->unit.object.bounding_sphere_center) !=
        false) {
      contact_point = tag_block_get_element(
        &biped_definition_contact_points(biped_tag), (int)index, sizeof(biped_contact_point_t));
      if (object_get_marker_by_name(unit_handle, contact_point->marker_name,
                                    &marker, 1) != 0) {
        material_effect_new_from_point(biped_definition_material_effects_index(biped_tag), param_2,
                                       &marker.matrix.position, 0);
      }
    }
  }

  if (global_current_collision_user_depth <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf60, true);
    system_exit(-1);
  }
  global_current_collision_user_depth = (int16_t)(global_current_collision_user_depth - 1);
}

/* biped_adjust_placement (0x1a1020)
 *
 * If the biped tag has the "camera offset placement" flag (tag+0x2f4 bit 3)
 * set and NOT the suppress flag (bit 2), shifts the placement's position
 * (placement+0x18..0x20) along the placement's forward axis (placement+0x40..
 * 0x48) by the biped's camera height (tag+0x42c).
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd', ...).
 * camera_height (tag+0x42c) loaded once and reused for all three axes.
 */
void biped_adjust_placement(int unit_handle, char *placement)
{
  object_placement_data *data = (object_placement_data *)placement;
  biped_data_t *biped;
  biped_definition_t *biped_tag;
  unsigned int flags;
  float camera_height;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  flags = biped_tag->biped_flags;
  if ((flags & 8) != 0 && (flags & 4) == 0) {
    camera_height = biped_definition_collision_radius(biped_tag);
    data->position_x = camera_height * data->up.x + data->position_x;
    data->position_y = camera_height * data->up.y + data->position_y;
    data->position_z = camera_height * data->up.z + data->position_z;
  }
}

/* biped_export_function_values (0x1a1080)
 *
 * Exports up to 4 animation "function" input values (unit+0xd4..0xe0) from the
 * biped tag's function-input table (tag+0x30c, 4 int16 type codes). For each
 * slot whose type is 1 ("speed"), the value is the unit's velocity magnitude
 * normalized by the biped's max speed (tag+0x334 ticks/sec * seconds-per-tick),
 * clamped to [0,1]. Type 0 slots are skipped (left unchanged). Other types
 * export 0.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd', ...).
 * Velocity at unit+0x18..0x20; output at unit+0xd4 (stride 4); types at
 * tag+0x30c (stride 2); max-speed denom = tag+0x334 * (1/30).
 */
void biped_export_function_values(int unit_handle)
{
  biped_data_t *biped;
  char *biped_tag;
  short *type_ptr;
  float *out_ptr;
  real_vector3d *vel;
  float val;
  int count;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  type_ptr = biped_definition_function_modes(biped_tag);
  out_ptr = &biped->unit.object.field_d0[1];
  count = 4;
  do {
    if (*type_ptr != 0) {
      val = 0.0f;
      switch (*type_ptr) {
      case 1:
        vel = &biped->unit.object.translational_velocity;
        val = sqrtf(vel->i * vel->i + vel->j * vel->j + vel->k * vel->k) /
              (biped_definition_flying_velocity(biped_tag) * REAL_ONE_THIRTIETH_POOL);
        if (val < 0.0f) {
          val = 0.0f;
        } else if (val > 1.0f) {
          val = 1.0f;
        }
        break;
      }
      *out_ptr = val;
    }
    type_ptr = type_ptr + 1;
    out_ptr = out_ptr + 1;
    count = count + -1;
  } while (count != 0);
}

/* biped_estimate_position (0x1a1140)
 *
 * Estimates the position of a biped based on the estimate mode. Used for
 * camera targeting, AI perception, and gun positioning. Different modes
 * control whether the position is computed from scratch or transformed.
 *
 * Estimate modes:
 *   0 = none: just get world position + height adjustment
 *   1 = crouching: use 0.0 crouch factor for height
 *   2 = standing: use 1.0 crouch factor for height
 *   3 = gun position: apply facing/offset transform, then height
 *   else: use actual unit crouch fraction
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type 1.
 * Confirmed: CALL 0x1ba140 (tag_get) with 'bipd' signature.
 * Confirmed: CALL 0x1412f0 (object_get_world_position) for mode 0.
 * Confirmed: CALL 0x8d9f0 (display_assert) with bipeds.c path.
 * Confirmed: Tag offsets 0x400 (standing height), 0x404 (crouching height).
 * Confirmed: Unit offset 0x464 (crouch fraction).
 * Confirmed: Globals 0x2533c0 (0.0f), 0x2533c8 (1.0f).
 */
void biped_estimate_position(int unit_handle, int16_t estimate_mode,
                             vector3_t *estimated_body_position,
                             vector3_t *desired_facing,
                             vector3_t *desired_gun_offset,
                             vector3_t *out_position)
{
  biped_data_t *biped;
  char *biped_tag;
  float crouch_value;
  float neg_facing_y;
  float facing_x;
  float gun_y;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  /* Assert: if estimate_mode != 0, must have estimated_body_position */
  if (estimate_mode != 0 && estimated_body_position == NULL) {
    display_assert("(estimate_mode == _unit_estimate_none) || "
                   "(estimated_body_position != NULL)",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x2f7, true);
    system_exit(-1);
  }

  /* Assert: if estimate_mode == 3, must have desired_facing */
  if (estimate_mode == 3) {
    if (desired_facing == NULL) {
      display_assert("(estimate_mode != _unit_estimate_gun_position) || "
                     "(desired_facing != NULL)",
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0x2f8, true);
      system_exit(-1);
    }
  } else if (estimate_mode == 0) {
    /* Mode 0: just get world position and apply height */
    object_get_world_position(unit_handle, out_position);
    goto height_adjustment;
  }

  /* Copy estimated body position to output */
  out_position->x = estimated_body_position->x;
  out_position->y = estimated_body_position->y;
  out_position->z = estimated_body_position->z;

  if (estimate_mode == 3) {
    /* Gun position mode: apply facing rotation and gun offset */
    if (desired_facing == NULL || desired_gun_offset == NULL) {
      display_assert("desired_facing && desired_gun_offset",
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0x307, true);
      system_exit(-1);
    }

    /* Transform position by facing direction and gun offset.
     * This applies a 2D rotation in XY plane using the facing vector,
     * then adds the Z component of the gun offset. */
    neg_facing_y = -desired_facing->y;
    facing_x = desired_facing->x;

    /* Apply gun offset X component along facing direction */
    out_position->x =
      desired_gun_offset->x * desired_facing->x + out_position->x;
    out_position->y =
      desired_gun_offset->x * desired_facing->y + out_position->y;
    out_position->z =
      desired_gun_offset->x * desired_facing->z + out_position->z;

    /* Apply gun offset Y component perpendicular to facing (rotated 90 deg) */
    gun_y = desired_gun_offset->y;
    out_position->x = gun_y * neg_facing_y + out_position->x;
    out_position->y = gun_y * facing_x + out_position->y;
    /* Note: gun_y * 0.0 for Z is optimized out, just add gun offset Z */
    out_position->z = gun_y * 0.0f + out_position->z;
    out_position->z = out_position->z + desired_gun_offset->z;
    return;
  }

height_adjustment:
  /* Calculate crouch value based on estimate mode. The switch matches the
   * original's MOVSX/DEC/JZ dispatch (mode 1 -> 0.0, mode 2 -> 1.0, else the
   * unit's actual crouch fraction). */
  switch (estimate_mode) {
  case 1:
    crouch_value = 0.0f; /* Standing */
    break;
  case 2:
    crouch_value = 1.0f; /* Crouching */
    break;
  default:
    crouch_value = biped->crouch;
    break;
  }

  /* Interpolate height between crouching and standing based on crouch value.
   * Order matches the original FPU sequence: (1-cv)*standing + cv*crouching. */
  out_position->z = (1.0f - crouch_value) * biped_definition_standing_camera_height(biped_tag) +
                    crouch_value * biped_definition_crouching_camera_height(biped_tag) +
                    out_position->z;
}

/* biped_get_autoaim_pill (0x1a12e0)
 *
 * Computes the auto-aim capsule (pill) for a biped: base position (out_pos),
 * axis/extent (out_axis), and the biped's autoaim flags value
 * (*out_value = tag+0x458). Three cases by the tag's autoaim marker node
 * indices (tag+0x4e4, +0x4e6):
 *   - Both valid + tag flag bit4 (tag+0x2f4 & 0x10): base = midpoint of the
 *     two marker node positions; axis = zero vector.
 *   - Both valid, no flag: base = marker0 position; axis = marker1 - marker0.
 *   - Either invalid: base/height from biped_get_camera_height_and_offset;
 *     base.z += half the height offset; axis = half-offset along world up.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd', ...);
 * object_get_node_matrix (node matrix +0x28 = position); midpoint3d (0x2a540);
 * 0.5f = REAL_0_5_POOL; axis = marker1 - marker0 (FSUB order from disasm).
 */
void biped_get_autoaim_pill(int unit_handle, real_point3d *out_pos, real_vector3d *out_axis,
                            int *out_value)
{
  biped_data_t *biped;
  biped_definition_t *biped_tag;
  real_matrix4x3 *marker0;
  real_matrix4x3 *marker1;
  float height_offset;
  float camera_height;
  float half;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  if (biped_definition_runtime_pelvis_node_index(biped_tag) != -1 &&
      biped_definition_runtime_head_node_index(biped_tag) != -1) {
    marker0 = object_get_node_matrix(unit_handle,
                                     biped_definition_runtime_pelvis_node_index(biped_tag));
    marker1 = object_get_node_matrix(unit_handle,
                                     biped_definition_runtime_head_node_index(biped_tag));
    if (((uint8_t)biped_tag->biped_flags & 0x10) != 0) {
      midpoint3d(&marker0->position, &marker1->position, out_pos);
      out_axis->i = global_zero_vector_ptr[0];
      out_axis->j = global_zero_vector_ptr[1];
      out_axis->k = global_zero_vector_ptr[2];
      *out_value = *(int *)&biped_definition_autoaim_width(biped_tag);
      return;
    }
    out_pos->x = marker0->position.x;
    out_pos->y = marker0->position.y;
    out_pos->z = marker0->position.z;
    out_axis->i = marker1->position.x - marker0->position.x;
    out_axis->j = marker1->position.y - marker0->position.y;
    out_axis->k = marker1->position.z - marker0->position.z;
    *out_value = *(int *)&biped_definition_autoaim_width(biped_tag);
    return;
  }

  biped_get_camera_height_and_offset(unit_handle, out_pos,
                                     &height_offset, &camera_height);
  half = height_offset * REAL_0_5_POOL;
  out_pos->z = half + out_pos->z;
  out_axis->i = half * global_up_vector_ptr[0];
  out_axis->j = half * global_up_vector_ptr[1];
  out_axis->k = half * global_up_vector_ptr[2];
  *out_value = *(int *)&biped_definition_autoaim_width(biped_tag);
}

/* biped_fix_position (0x1a1430)
 *
 * Tries to find a collision-free world position for a biped (used when
 * teleporting it out of a vehicle seat, respawning, etc.). Starting from an
 * initial position (either supplied via initial_position, or the biped's
 * estimated camera/standing position when NULL), it walks a fixed table of
 * displacement directions at 0x2b4b80 (stride 3 floats), scaling each by
 * `scale`. For each candidate it (1) finds the BSP3D leaf, rejecting points
 * outside the BSP; (2) runs a battery of collision tests (collision_fix_pill sphere
 * fit, collision_test_pill vector-to-surface, and when a seat is involved the
 * collision_model_test_pill / collision_test_line line-of-sight checks against both objects). The
 * first candidate that passes all gates becomes the fixed location: the biped
 * object's position (obj+0xc) is updated, children are re-parented, and the
 * object is translated to the new location (unless dont_teleport). The final
 * world point is optionally written to final_position. Returns 1 on success.
 *
 * The whole search runs inside the collision-user-depth stack (global
 * 0x4761d8 / stack 0x5a8c80, marker 7), asserting depth < 0x20 on entry
 * (line 0x37f) and > 1 on exit (line 0x438).
 *
 * Confirmed (disasm): cdecl, 8 stack params, char return. param_4 is float*
 *   (3-float out-write at 0x1a1891). param_6/7/8 are byte flags (TEST AL,AL).
 *   collision_flags = (biped_tag+0x2f4 & 0x20) ? 0x20c3a0 : (0x20c3a0 +
 *   0xffdfff00) via NEG/SBB/AND/ADD. The direction-search count is branchless:
 *   loop_limit = 0x1b - ((p6!=0)-1 & 9) -> 0x1b when p6, 0x12 when !p6.
 *   The cross product cross = normalize3d(forward_axis x up_axis) is built from
 *   the biped's basis rows at obj+0x24 (right) and obj+0x30 (up); FSUBP order
 *   preserved from disasm. final candidate stored at local_14/10/c.
 * Inferred: the 0x2b4b80 table is a spiral/expanding set of search offsets;
 *   collision_flags low bits select the collision material/group mask.
 * Uncertain: exact field meanings inside the 130d0 collision result buffer
 *   beyond the hit-object-handle word at +0x38 (compared to the two unit
 *   handles to skip self/seat hits).
 */
char biped_fix_position(int unit_handle, int seat_handle,
                        float *initial_position, float *final_position,
                        float scale, char keep_basis, char dont_teleport,
                        char scale_by_height)
{
  unsigned int *biped_obj;
  int biped_tag;
  int depth;
  unsigned char success;
  short i;
  short loop_limit;
  int from_seat;
  int collision_flags;
  int leaf_index;
  void *bsp;
  void *leaf_elem;
  float *src_pos;
  float *basis; /* biped_obj as float[] — basis-matrix rows at +0x24..0x38 */
  real_point3d position; /* local_2c/28/24 */
  float camera_height; /* local_18 ([EBP-0x18], 1a0890 arg4) */
  float height_offset; /* local_1c ([EBP-0x1c], 1a0890 arg3) */
  real_vector3d offset_vec; /* local_4c/48/44 */
  real_vector3d cross; /* local_40/3c/38 */
  float fx, fy;
  real_point3d candidate; /* local_14: candidate.x; local_10/c/8 below */
  real_point3d cand; /* local_10/c/8 — the BSP query point */
  /* Out buffers passed to collision callees. Sized per each callee's writes;
   * preserve sizes to keep the original 0x538 frame layout. */
  real_point3d center; /* local_64 — 1aae0 sphere center / 130d0 args */
  char box[16]; /* local_74 — 14c8e0 out, reused as 14cc80 arg1 */
  location_t location;
  collision_test_result surf_result; /* local_114 — 14e7d0 result */
  collision_test_result los_result; /* local_c4 — 130d0 result; +0x38 = hit handle.
                        * 130d0 delegates to 14df70, which writes an 0x50-byte
                        * result (units.c:9404, projectiles.c collision_result
                        * [0x50]); original frame gap local_c4..local_74 is
                        * exactly 0x50. At 56 bytes it overflowed into box[],
                        * corrupting the 14cc80 context (tag_groups.c:3086
                        * element_size assert on seat exit). */
  static char obstruction[1064]; /* local_53c — 14cc80 working buffer */

  success = 0;
  if ((final_position == (float *)0) && (dont_teleport != '\0')) {
    display_assert("final_position || !dont_teleport",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x37d, true);
    system_exit(-1);
  }
  if (COLLISION_USER_STACK_DEPTH >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x37f, true);
    system_exit(-1);
  }
  depth = COLLISION_USER_STACK_DEPTH;
  COLLISION_USER_STACK_DEPTH = (int16_t)(depth + 1);
  COLLISION_USER_STACK[depth] = 7;

  /* Branch structure preserved from disasm (0x1a14aa): call
   * object_get_bounding_sphere only when a seat is involved, jumping straight
   * to the epilogue when both handles are NONE. */
  if (unit_handle == -1) {
    if (seat_handle == -1) {
      goto epilogue;
    }
    goto call_aae0;
  }
  if (seat_handle != -1) {
  call_aae0:
    object_get_bounding_sphere(seat_handle, (float *)&center,
                               (float *)&from_seat);
  }
  {
    from_seat = (unit_handle == -1);
    if (from_seat) {
      unit_handle = seat_handle;
    }
    biped_obj = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
    biped_tag = (int)tag_get(0x62697064, *biped_obj); /* 'bipd' */
    collision_flags =
      (-(int)((biped_definition_flags(biped_tag) & 0x20) != 0) &
       0xffdfff00) +
      0x20c3a0;

    if (initial_position == (float *)0) {
      src_pos = (float *)&position;
    } else {
      position.x = initial_position[0];
      position.y = initial_position[1];
      position.z = initial_position[2];
      src_pos = (float *)&offset_vec;
    }
    biped_get_camera_height_and_offset(unit_handle, (vector3_t *)src_pos,
                                       &height_offset, &camera_height);
    if (from_seat) {
      unit_handle = -1;
    }
    loop_limit = (short)(0x1b - ((keep_basis != '\0') - 1 & 9));
    if (seat_handle != -1) {
      collision_model_instance_new((int *)box, seat_handle);
    }
    /* &cross = right_axis(obj+0x24) x up_axis(obj+0x30); normalize. The basis
     * rows at obj+0x24..0x38 are floats; load them as floats (FLD), not via
     * int->float conversion. FSUBP order preserved exactly from disasm. */
    basis = (float *)biped_obj;
    cross.i = basis[0xe] * basis[0xa] - basis[0xb] * basis[0xd];
    cross.j = basis[0xb] * basis[0xc] - basis[9] * basis[0xe];
    cross.k = basis[9] * basis[0xd] - basis[0xc] * basis[0xa];
    normalize3d(&cross);
    offset_vec.i = height_offset * global_up_vector_ptr[0];
    offset_vec.j = height_offset * global_up_vector_ptr[1];
    offset_vec.k = height_offset * global_up_vector_ptr[2];
    if (scale_by_height != '\0') {
      scale = camera_height * scale;
    }

    i = 0;
    while ((short)i < loop_limit) {
      leaf_index = (int)(short)i;
      if (keep_basis == '\0') {
        cand.x = scale * FUDGE_VECTORS_I[leaf_index * 3] + position.x;
        cand.y = scale * FUDGE_VECTORS_J[leaf_index * 3] + position.y;
        cand.z = scale * FUDGE_VECTORS_K[leaf_index * 3] + position.z;
      } else {
        fx = scale * FUDGE_VECTORS_I[leaf_index * 3];
        fy = scale * FUDGE_VECTORS_J[leaf_index * 3];
        candidate.z = scale * FUDGE_VECTORS_K[leaf_index * 3];
        cand.x = candidate.z * basis[0xc] + cross.i * fy + fx * basis[9] +
                  position.x;
        cand.y = candidate.z * basis[0xd] + cross.j * fy + fx * basis[0xa] +
                  position.y;
        cand.z = candidate.z * basis[0xe] + cross.k * fy + fx * basis[0xb] +
                  position.z;
      }

      if (scenario_leaf_index_from_point((int)&cand) != -1) {
        leaf_index = scenario_leaf_index_from_point((int)&cand) & 0x7fffffff;
        bsp = scenario_get();
        leaf_elem =
          tag_block_get_element(&structure_bsp_leaves(bsp), leaf_index, 0x10);
        if ((structure_leaf_cluster_index(leaf_elem) != -1) &&
            (collision_fix_pill((uint32_t)collision_flags, (float *)&cand,
                          camera_height + camera_height, height_offset,
                          camera_height, unit_handle, (float *)&cand) != '\0') &&
            (collision_test_pill((uint32_t)collision_flags, (float *)&cand, (float *)&offset_vec,
                          camera_height, unit_handle, &surf_result) == '\0') &&
            ((seat_handle == -1) ||
             ((collision_model_test_pill((int)box, (int)&cand, (int)&offset_vec,
                            camera_height, (int16_t *)obstruction) == '\0') &&
              ((collision_test_line((uint32_t)collision_flags, (float *)&cand,
                             (float *)&center, unit_handle,
                             (int16_t *)&los_result) == 0) ||
               (los_result.object_index == seat_handle)) &&
              ((collision_test_line((uint32_t)collision_flags, (float *)&center,
                             (float *)&cand, seat_handle,
                             (int16_t *)&los_result) == 0) ||
               (los_result.object_index == unit_handle))))) {
          biped_tag = (int)tag_get(0x62697064, *biped_obj);
          scenario_location_from_point(&location, &cand);
          if (location.cluster_index == -1) {
            display_assert("fixed_location.cluster_index!=NONE",
                           "c:\\halo\\SOURCE\\units\\bipeds.c", 0x41e, true);
            system_exit(-1);
          }
          if ((*(unsigned char *)&biped_definition_flags(biped_tag) & 8) == 0) {
            cand.z = cand.z - biped_definition_collision_radius(biped_tag);
          }
          if ((unit_handle != -1) && (dont_teleport == '\0')) {
            biped_obj[3] = REAL_BITS(cand.x);
            biped_obj[4] = REAL_BITS(cand.y);
            biped_obj[5] = REAL_BITS(cand.z);
            object_update_children_recursive(unit_handle);
            object_translate(unit_handle, &cand, &location);
          }
          if (final_position != (float *)0) {
            final_position[0] = cand.x;
            final_position[1] = cand.y;
            final_position[2] = cand.z;
          }
          success = 1;
        }
      }
      i = (short)(i + 1);
      if (success != 0) {
        break;
      }
    }
  }

epilogue:
  if (COLLISION_USER_STACK_DEPTH < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x438, true);
    system_exit(-1);
  }
  COLLISION_USER_STACK_DEPTH = (int16_t)(COLLISION_USER_STACK_DEPTH - 1);
  return (char)success;
}

/* biped_render_debug (0x1a1900)
 *
 * Debug-visualization for a biped's camera/aim geometry, gated by two debug
 * globals. When 0x5054fe is set, draws the camera height/offset: if the height
 * offset is at/below the small threshold (0x2533c0), renders a point
 * (render_debug_sphere); otherwise scales the world-up vector by the height offset and
 * renders a vector arrow (render_debug_pill). When 0x5054fd is set, fetches the
 * autoaim pill (biped_get_autoaim_pill) and renders the axis as an arrow if its
 * squared length exceeds the threshold, else a point. Render context pointers
 * come from [0x2ee6c4] (camera) and [0x2ee6d0] (autoaim).
 *
 * Confirmed: out_pos vec3 at EBP-0x20, scaled vec3 at EBP-0x14, height_offset
 * at EBP-0x8, camera_height/val at EBP-0x4; camera call arg order; up vector
 * from [0x31fc44]; squared-length FPU order is z,y,x; threshold [0x2533c0].
 * Inferred: "render debug" semantics from the two render_debug.c helpers and
 * the debug-global gating.
 */
void biped_render_debug(int unit_handle)
{
  vector3_t out_pos;
  real_vector3d scaled;
  float height_offset;
  float camera_height;

  if (DEBUG_OBJECTS_BIPED_PHYSICS_PILLS != '\0') {
    biped_get_camera_height_and_offset(unit_handle, &out_pos, &height_offset,
                                       &camera_height);
    if (height_offset > REAL_ZERO_POOL) {
      scaled.i = height_offset * global_up_vector_ptr[0];
      scaled.j = height_offset * global_up_vector_ptr[1];
      scaled.k = height_offset * global_up_vector_ptr[2];
      render_debug_pill(1, &out_pos, &scaled, camera_height, GLOBAL_REAL_ARGB_WHITE);
    } else {
      render_debug_sphere(1, &out_pos, camera_height, GLOBAL_REAL_ARGB_WHITE);
    }
  }
  if (DEBUG_OBJECTS_BIPED_AUTOAIM_PILLS != '\0') {
    biped_get_autoaim_pill(unit_handle, &out_pos, &scaled,
                           (int *)&camera_height);
    if (scaled.k * scaled.k + scaled.j * scaled.j + scaled.i * scaled.i >
        REAL_ZERO_POOL) {
      render_debug_pill(1, &out_pos, &scaled, camera_height, GLOBAL_REAL_ARGB_RED);
      return;
    }
    render_debug_sphere(1, &out_pos, camera_height, GLOBAL_REAL_ARGB_RED);
  }
}

/* biped_find_ground_surface (0x1a1a10)
 *
 * Casts a collision ray (vector) from the unit's world position along a caller-
 * supplied direction (scaled) and reports the hit. Computes the ray origin as
 * the unit's world position nudged by a global offset vector ([0x31fc44]) times
 * a global scale ([0x253524]); the ray direction is the caller's direction
 * vector (@eax) times the caller-supplied scale ([EBP+8]). Brackets the work in
 * the collision-user-depth stack (global 0x4761d8 / stack 0x5a8c80, marker 7),
 * asserting depth < 0x20 on entry (line 0x47a) and > 1 on exit (line 0x490).
 * Runs collision_bsp_test_vector against the global collision BSP. On a hit and
 * if out_point != NULL, writes the world-space hit point
 * (out_point[i] = scaled_dir[i] * t + origin[i]); if out_vec != NULL, copies
 * the 3-float surface record pointed to by result+4 into out_vec. Returns the
 * hit surface/leaf index (result+8) on hit, or -1 on miss.
 *
 * Confirmed (disasm): cdecl, returns int in EAX (MOV EAX,ESI; ESI = -1 default
 *   / result+8 on hit). Register args: @eax = direction vector pointer
 *   (MOV ESI,EAX; FMUL [ESI]/[ESI+4]/[ESI+8]); @edi = unit_handle (PUSH EDI ->
 *   object_get_and_verify_type(.,1)). Stack args [EBP+8]=float scale,
 *   [EBP+0xc]=float *out_point, [EBP+0x10]=void *out_vec (caller 0x1a1b90:
 *   PUSH 0x40000000 (2.0f), PUSH out_point, PUSH 0x0; EAX=[0x31fc50],
 *   EDI=unit_handle). collision_bsp_test_vector takes 8 args; its 0x20 cleanup
 *   is combined with object_get_world_position's deferred 8-byte cleanup
 *   (ADD ESP,0x28 = 0x20 + 8). The collision-result buffer is ONE contiguous
 *   region at EBP-0x434: result+0=t (float), result+4=surface-record pointer
 *   (3 floats copied to out_vec), result+8=hit index (return). Ghidra's
 *   local_438/local_434/local_430 are this single buffer, not separate locals.
 * Inferred: origin offset/scale semantics from the FADD-into-world-position
 *   shape; "supporting/forward ray" usage from the 2.0f scale and out params.
 * Uncertain: exact meaning of globals [0x31fc44] (offset vector) and [0x253524]
 *   (scale); full layout of the 0x414-byte collision-result buffer beyond the
 *   three fields read here.
 */
int biped_find_ground_surface(float scale, real_point3d *out_point, void *out_vec,
                              const real_vector3d *direction /* @eax */,
                              int unit_handle /* @edi */)
{
  /* One contiguous collision-result buffer (frame base EBP-0x434). Sized to
   * cover collision_bsp_test_vector's full record (mirrors path.c's
   * local_438[264]); we read t at +0, a surface-record pointer at +4, and the
   * hit index at +8. Do NOT split into separate locals. */
  collision_bsp_test_vector_result_t collision_result;
  int result_index;
  int depth;
  void *bsp;
  float *offset_vec;
  float *surface_record;
  float t;
  real_point3d origin;
  real_vector3d scaled_dir;
  float dir0;

  object_get_and_verify_type(unit_handle, 1);
  bsp = global_collision_bsp_get();
  result_index = -1;
  dir0 = direction->i;

  /* global_current_collision_user_depth by raw address: the named global
   * compiles 6 bytes shorter than the original here. */
  if (COLLISION_USER_STACK_DEPTH >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x47a, true);
    system_exit(-1);
  }
  depth = COLLISION_USER_STACK_DEPTH;
  COLLISION_USER_STACK_DEPTH = (int16_t)(depth + 1);
  COLLISION_USER_STACK[depth] = 7;

  object_get_world_position(unit_handle, &origin);

  offset_vec = global_up_vector_ptr;
  origin.x = offset_vec[0] * REAL_0_4_POOL + origin.x;
  origin.y = offset_vec[1] * REAL_0_4_POOL + origin.y;
  origin.z = offset_vec[2] * REAL_0_4_POOL + origin.z;

  scaled_dir.i = scale * dir0;
  scaled_dir.j = scale * direction->j;
  scaled_dir.k = scale * direction->k;

  if ((char)collision_bsp_test_vector(1, (int)bsp, 0, 0, (int)&origin,
                                      (int)&scaled_dir, 3.4028235e+38f,
                                      (float *)&collision_result) != 0) {
    t = collision_result.t;
    if (out_point != (real_point3d *)0) {
      out_point->x = scaled_dir.i * t + origin.x;
      out_point->y = scaled_dir.j * t + origin.y;
      out_point->z = scaled_dir.k * t + origin.z;
    }
    result_index = collision_result.surface_index;
    if (out_vec != (void *)0) {
      surface_record = (float *)collision_result.plane;
      ((int *)out_vec)[0] = ((int *)surface_record)[0];
      ((int *)out_vec)[1] = ((int *)surface_record)[1];
      ((int *)out_vec)[2] = ((int *)surface_record)[2];
    }
  }

  if (COLLISION_USER_STACK_DEPTH <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x490, true);
    system_exit(-1);
  }
  COLLISION_USER_STACK_DEPTH = (int16_t)(COLLISION_USER_STACK_DEPTH - 1);
  return result_index;
}

/* biped_approximate_surface_index (0x1a1b90)
 *
 * Thin wrapper over the biped collision probe (biped_find_ground_surface):
 * casts a ray of length 2.0 from the biped's world position along the global
 * direction vector at [0x31fc50], with no surface-vector output. The keystone's
 * collision-result index is returned unchanged in EAX (-1 = no hit). out_point,
 * when non-NULL, receives the hit point.
 *
 * Confirmed (disasm 0x1a1b90): keystone direction@<eax> = GLOBAL_DOWN3D;
 * unit_handle@<edi> = param_1; cdecl pushes scale=2.0f (0x40000000),
 * out_point=param_2, out_vec=NULL; ADD ESP,0xc. No MOV EAX after CALL, so the
 * keystone return flows through (caller 0x56c60 tests the result == -1).
 */
int biped_approximate_surface_index(int unit_handle, float *out_point)
{
  return biped_find_ground_surface(2.0f, (real_point3d *)out_point, (void *)0,
                                   GLOBAL_DOWN3D, unit_handle);
}

/* biped_find_pathfinding_surface_index (0x1a1bc0)
 *
 * Resolves the BSP surface index the biped is standing on for pathfinding,
 * caching it on the unit object. If the biped's tag wants the simple path
 * (tag+0x2f4 bit 2 set and unit+0xb6 bit 2 clear) it just clears the cached
 * index and calls object_get_world_position. Otherwise it lazily recomputes:
 * when the cache is stale (unit+0x434 == -1 and game_time advanced past
 * unit+0x444), it snapshots the cached position (unit+0x438..0x440), and tries
 * to re-project onto the previous surface (unit+0x430 last-good, else
 * unit+0x448 last-result) via the collision_surface_* helpers; if that fails it
 * falls back to a fresh 2.0-length collision probe. On success it stores the
 * new position and surface index. Finally it always copies the cached position
 * (unit+0x438) into *pos and returns the cached index (unit+0x434).
 *
 * Confirmed (disasm 0x1a1bc0): object_get_and_verify_type(unit_handle,1);
 * tag_get('bipd', obj[0]); game_time_get; global_collision_bsp_get; keystone
 * direction@<eax>=GLOBAL_DOWN3D, unit_handle@<edi>=unit_handle, push
 * scale=2.0f,&pos_buf,NULL. collision_surface helper arg counts from disasm
 * (test_point2d=5 args ret char, find_closest/project=6 args). Position vec3 at
 * EBP-0x14, 2D scratch at EBP-0x8. display_assert at :0x4e7 if pos==NULL.
 *
 */
int biped_find_pathfinding_surface_index(int unit_handle, vector3_t *pos)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  int game_time;
  void *bsp;
  real_point3d position; /* EBP-0x14: snapshot/output position vec3 */
  real_point2d proj2d; /* EBP-0x8: 2D projected scratch point */
  char projected;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  if ((((uint8_t)definition->biped_flags & 4) != 0) &&
      ((biped->unit.object.damage_flags & 4) == 0)) {
    biped->pathfinding_surface_index = NONE;
    object_get_world_position(unit_handle, pos);
  } else if ((biped->pathfinding_surface_index == NONE) &&
             (game_time = game_time_get(),
              game_time > biped->last_pathfinding_attempt_time)) {
    bsp = global_collision_bsp_get();
    position.x = biped->pathfinding_point.x;
    position.y = biped->pathfinding_point.y;
    position.z = biped->pathfinding_point.z;
    biped->last_pathfinding_attempt_time = game_time;
    if (biped->support_surface_index != NONE) {
      collision_surface_find_closest_point2d(
        (int)bsp, biped->support_surface_index, 2, 1,
        (real_point2d *)&biped->pathfinding_point, &proj2d);
      collision_surface_project_point2d((int)bsp, biped->support_surface_index,
                                        2, 1, &proj2d, &position);
      biped->pathfinding_surface_index = biped->support_surface_index;
    } else if ((biped->last_pathfinding_surface_index != NONE) &&
               (projected = (char)collision_surface_test_point2d(
                  (int)bsp, biped->last_pathfinding_surface_index, 2, 1,
                  (real_point2d *)&biped->pathfinding_point),
                projected != 0)) {
      biped->pathfinding_surface_index = biped->last_pathfinding_surface_index;
      collision_surface_project_point2d(
        (int)bsp, biped->last_pathfinding_surface_index, 2, 1,
        (real_point2d *)&biped->pathfinding_point, &position);
      biped->pathfinding_surface_index = biped->last_pathfinding_surface_index;
    }
    if (biped->pathfinding_surface_index == NONE) {
      biped->pathfinding_surface_index = biped_find_ground_surface(
        2.0f, &position, (void *)0, GLOBAL_DOWN3D, unit_handle);
    }
    if (biped->pathfinding_surface_index != NONE) {
      biped->pathfinding_point.x = position.x;
      biped->pathfinding_point.y = position.y;
      biped->pathfinding_point.z = position.z;
      biped->last_pathfinding_surface_index = biped->pathfinding_surface_index;
    }
  }

  if (pos == (vector3_t *)0) {
    display_assert("pathfinding_point", "c:\\halo\\SOURCE\\units\\bipeds.c",
                   0x4e7, true);
    system_exit(-1);
  }
  *pos = biped->pathfinding_point;
  return biped->pathfinding_surface_index;
}

/* biped_exit_seat_end (0x1a1d80)
 *
 * Finishes a biped exiting a vehicle seat. Flattens the biped's exit forward
 * vector (unit+0x24) to horizontal (zeroes z at unit+0x2c) and renormalizes,
 * falling back to the world forward axis if degenerate; sets the exit up
 * vector (unit+0x30) to world up; sets unit+0x424 bit0. Then tries to fix the
 * biped's position out of the vehicle: first with a fixed 2.0 distance, and if
 * that fails, using the vehicle's bounding sphere; logs a warning if still
 * unable.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd', ...)
 * (result discarded). normalize3d(unit+0x24); fallback = global forward axis;
 * unit+0x30 = global up axis. object_get_bounding_sphere gets vehicle
 * center+radius. 2.0f arg pushed as 0x40000000.
 */
void biped_exit_seat_end(int unit_handle, int seat_handle)
{
  biped_data_t *biped;
  real_point3d center;
  float radius;
  char fixed;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  biped->unit.object.forward.k = 0.0f;
  if (normalize3d(&biped->unit.object.forward) == 0.0f) {
    biped->unit.object.forward.i = global_forward_vector_ptr[0];
    biped->unit.object.forward.j = global_forward_vector_ptr[1];
    biped->unit.object.forward.k = global_forward_vector_ptr[2];
  }
  biped->unit.object.up.i = global_up_vector_ptr[0];
  biped->unit.object.up.j = global_up_vector_ptr[1];
  biped->unit.object.up.k = global_up_vector_ptr[2];
  biped->flags |= 1;

  fixed =
    biped_fix_position(unit_handle, seat_handle, (float *)0, 0, 2.0f, 1, 0, 1);
  if (fixed == 0) {
    object_get_bounding_sphere(seat_handle, (float *)&center, &radius);
    fixed =
      biped_fix_position(unit_handle, seat_handle, (float *)&center, 0, radius, 1, 0, 0);
    if (fixed == 0) {
      error(2, "couldn't teleport the biped out far enough from the vehicle"
               "...");
    }
  }
}

/* biped_falling_danger (0x1a1e70)
 *
 * Per-tick "is this biped stuck falling / off a ledge?" check for AI bipeds.
 * After a battery of eligibility gates (not in a vehicle, tag flags clear,
 * not flying, has a valid path leader, not in scream state 0x1d, settle timer
 * past 0x1e, and a throttled recheck interval of game_time+0xf), it casts a
 * 6.0-length probe from the biped along the global direction at [0x31fc50].
 * If the probe misses, OR (after fetching the biped's world position) the
 * biped's clearance field (obj+0x20) is small and the vertical drop to the
 * probe point exceeds the maximum damage velocity (squared), it
 * triggers the recovery path unit_scream(unit_handle, 0).
 *
 * Confirmed (disasm 0x1a1e70): cdecl unit_handle at [EBP+8] (caller 0x1a6350
 * PUSH EDI; ADD ESP,4); object_get_and_verify_type(unit,1); tag_get('bipd');
 * game_time_get; tag_block_get_element(game_globals_get()+0x188, 0, 0x98);
 * keystone direction@<eax>=GLOBAL_DOWN3D, unit@<edi>, scale=6.0f
 * (0x40c00000); object_get_world_position; unit_scream(unit,0). Return
 * discarded (void).
 *
 * Inferred: obj+0x20 = clearance/height float; DAT_0032512c gravity-related scalar. Probe hit point and world
 * position are separate 3-float stack buffers.
 */
void biped_falling_danger(int unit_handle)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  int game_time;
  game_globals_falling_damage_t *falling_damage;
  float fall_term;
  real_point3d probe_hit;
  real_point3d world_pos;
  real_vector3d *dir_ptr;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  if (((biped->unit.object.damage_flags & 4) == 0) &&
      (((uint8_t)definition->biped_flags & 0x84) == 0) &&
      ((biped->unit.flags & 0x1000) == 0) &&
      (biped->unit.actor_index.value != NONE) &&
      ((char)biped->unit.animation_state != 0x1d)) {
    game_time = game_time_get();
    if ((biped->airborne_ticks > 0x1e) &&
        ((biped->last_falling_communication_time == NONE) ||
         (biped->last_falling_communication_time + 0xf < game_time))) {
      falling_damage =
        tag_block_get_element(&game_globals_falling_damage(game_globals_get()), 0,
                              sizeof(game_globals_falling_damage_t));
      biped->last_falling_communication_time = game_time;
      dir_ptr = GLOBAL_DOWN3D;
      if ((biped_find_ground_surface(6.0f, &probe_hit, (void *)0, dir_ptr,
                                     unit_handle) == -1) ||
          (object_get_world_position(unit_handle, &world_pos),
           !(biped->unit.object.translational_velocity.k > REAL_ZERO_POOL) &&
             (fall_term = (world_pos.z - probe_hit.z) * GLOBAL_GRAVITY,
              !(biped->unit.object.translational_velocity.k *
                    biped->unit.object.translational_velocity.k +
                  (fall_term + fall_term) <
                falling_damage->runtime_maximum_damage_velocity *
                  falling_damage->runtime_maximum_damage_velocity)))) {
        unit_scream(unit_handle, 0);
      }
    }
  }
}

/* biped_vehicle_speech (0x1a1fb0)
 *
 * Vehicle-rider variant of the stuck/ejection check (mirrors biped_falling_danger for a
 * biped riding a vehicle seat). Gates on the vehicle tag flag (vehi+0x17c bit
 * 6), the biped having a seat (obj+0x1a4 != -1), not in scream state 0x1d, a
 * recheck throttle field (obj+0x2d2 > 'x'), the vehicle settle counter
 * (vehi+0x428 > 0x1e), and a game_time+0xf interval on obj+0x450. It first
 * probes straight (8.0 length) along [0x31fc50]; if that misses it computes a
 * scaled velocity vector from the vehicle physics (phys+0x18/0x1c/0x20 times
 * DAT_2b4ee4, with a gravity bias on z), and if that vector's length is large
 * enough re-probes along it. Depending on which probe/threshold fails it logs
 * one of three eject reasons (0x26/0x27/0x28) via
 * ai_communication_event(reason, unit_handle, -1,-1,-1,-1, 0).
 *
 * Confirmed (disasm 0x1a1fb0): register arg unit_handle@<eax> (MOV EDI,EAX at
 * 0x1a1fb9; caller 0x1a6350 MOV EAX,EDI; CALL with no stack arg). vehi =
 * object_get_and_verify_type(object_get_and_verify_type(unit,1)->[0xcc], 2);
 * tag_get('vehi'). keystone: 1st call direction@<eax>=GLOBAL_DOWN3D,
 * scale=8.0f, out_point=0, out_vec=0; 2nd call direction@<eax>=&scaled_vel,
 * scale=8.0f, out_point=0, out_vec=&probe_vec. unit@<edi> on both.
 * ai_communication_event args: 7 pushes (reason, EDI=unit, -1,-1,-1,-1, 0).
 * Void.
 *
 * Inferred: DAT_2b4ee4 velocity scalar, DAT_2b4ee0/DAT_32512c gravity bias;
 * obj fields 0x1a4 seat, 0x253 scream, 0x2d2 throttle byte; vehi+0x428 settle;
 * obj+0x450 recheck timestamp. phys[6..8] = obj+0x18/0x1c/0x20 vehicle
 * velocity.
 */
void biped_vehicle_speech(int unit_handle /* @eax */)
{
  biped_data_t *biped;
  vehicle_data_t *vehicle;
  unit_definition_t *definition;
  int game_time;
  int probe_vec[3]; /* EBP-0x18: 2nd keystone out_vec */
  real_vector3d scaled_vel; /* EBP-0xc: scaled vehicle velocity */

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  vehicle = object_get_and_verify_type(
    biped->unit.object.parent_object_index.value, OBJECT_MASK_VEHICLE);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  if ((((uint8_t)definition->flags & 0x40) != 0) &&
      (biped->unit.actor_index.value != NONE) &&
      ((char)biped->unit.animation_state != 0x1d) &&
      ((char)biped->unit.gunner_inactive_ticks > 0x78)) {
    game_time = game_time_get();
    if ((vehicle->airborne_ticks > 0x1e) &&
        ((biped->last_falling_communication_time == NONE) ||
         (biped->last_falling_communication_time + 0xf < game_time))) {
      biped->last_falling_communication_time = game_time;
      if (biped_find_ground_surface(8.0f, (real_point3d *)0, (void *)0,
                                    GLOBAL_DOWN3D, unit_handle) == -1) {
        scaled_vel.i =
          vehicle->unit.object.translational_velocity.i * REAL_60_POOL;
        scaled_vel.j =
          vehicle->unit.object.translational_velocity.j * REAL_60_POOL;
        scaled_vel.k =
          vehicle->unit.object.translational_velocity.k * REAL_60_POOL -
          GLOBAL_GRAVITY * REAL_1800_POOL;
        if (normalize3d(&scaled_vel) <= REAL_ZERO_POOL) {
          ai_communication_event(0x28, unit_handle, -1, -1, -1, -1, 0);
          return;
        }
        if ((biped_find_ground_surface(8.0f, (real_point3d *)0, probe_vec, &scaled_vel,
                                       unit_handle) == -1) ||
            (*(float *)&probe_vec[2] <= REAL_0_3_POOL)) {
          ai_communication_event(0x28, unit_handle, -1, -1, -1, -1, 0);
          return;
        }
      }
      if (REAL_0_6_POOL < vehicle->unit.object.up.k) {
        if (magnitude3d(&vehicle->unit.object.angular_velocity) <
            REAL_THREE_DEGREES_POOL) {
          ai_communication_event(0x26, unit_handle, -1, -1, -1, -1, 0);
          return;
        }
      }
      ai_communication_event(0x27, unit_handle, -1, -1, -1, -1, 0);
    }
  }
}

/* biped_apply_rotation (0x1a2160)
 *
 * Rotates a biped's forward/up axes (unit+0x24 fwd, unit+0x30 up) about the
 * axis stored at unit+0x3c by the angle equal to that axis vector's length.
 * Normalizes the axis (angle = |axis|), rotates the forward vector by
 * (sin,cos) of the angle and renormalizes it, then rebuilds an orthogonal up
 * vector via two cross products (cross(up_rot, fwd) then cross(temp, fwd)) and
 * renormalizes; on degenerate result, resets to world forward/up.
 *
 * Register arg: unit_handle in EAX. Confirmed: normalize3d (0x13010);
 * rotate_vector3d_by_sincos (0x10b6e0); x87 FCOS/FSIN; globals 0x31fc3c fwd,
 * 0x31fc44 up. Cross operand order transcribed from disassembly.
 */
void biped_apply_rotation(int unit_handle)
{
  object_data_t *object;
  real_vector3d axis;
  real_vector3d up_rot;
  real_vector3d t;
  float angle;
  float cos_a;
  float sin_a;
  real_vector3d *forward;
  real_vector3d *up;

  object = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);

  axis = object->angular_velocity;
  angle = normalize3d(&axis);
  forward = &object->forward;
#if defined(_MSC_VER) && !defined(__clang__)
  cos_a = (float)cos((double)angle);
  sin_a = (float)sin((double)angle);
#else
  cos_a = x87_fcos(angle);
  sin_a = x87_fsin(angle);
#endif
  rotate_vector3d_by_sincos(forward, &axis, sin_a, cos_a);
  normalize3d(forward);

  up = &object->up;
  up_rot = *up;
  rotate_vector3d_by_sincos(&up_rot, &axis, sin_a, cos_a);

  t.i = up_rot.k * forward->j - up_rot.j * forward->k;
  t.j = up_rot.i * forward->k - up_rot.k * forward->i;
  t.k = up_rot.j * forward->i - up_rot.i * forward->j;
  /* up = cross(temp, forward) — FPU LIFO: computed z,y,x, stored x,y,z */
  up->i = t.j * forward->k - t.k * forward->j;
  up->j = t.k * forward->i - t.i * forward->k;
  up->k = t.i * forward->j - t.j * forward->i;
  if (normalize3d(up) == 0.0f) {
    forward->i = global_forward_vector_ptr[0];
    forward->j = global_forward_vector_ptr[1];
    forward->k = global_forward_vector_ptr[2];
    up->i = global_up_vector_ptr[0];
    up->j = global_up_vector_ptr[1];
    up->k = global_up_vector_ptr[2];
  }
}

/* Game globals player_information block element (globals +0x170, element
 * size 0xf4). */
typedef struct game_globals_player_information_t {
  uint8_t pad_00[0x2c];
  real walking_speed;                         /* +0x2c */
  real double_speed_multiplier;               /* +0x30 */
  real run_forward_speed;                     /* +0x34 */
  real run_backward_speed;                    /* +0x38 */
  real run_sideways_speed;                    /* +0x3c */
  real run_acceleration;                      /* +0x40 */
  real sneak_forward_speed;                   /* +0x44 */
  real sneak_backward_speed;                  /* +0x48 */
  real sneak_sideways_speed;                  /* +0x4c */
  real sneak_acceleration;                    /* +0x50 */
  real airborne_acceleration;                 /* +0x54 */
  uint8_t pad_58[0x28];
  real stun_movement_penalty;                 /* +0x80 */
  real field_84;                              /* +0x84 stun penalty on jump speed */
  uint8_t pad_88[0x6c];
} game_globals_player_information_t;
cs(game_globals_player_information_t, 0xf4);
co(game_globals_player_information_t, walking_speed, 0x2c);
co(game_globals_player_information_t, airborne_acceleration, 0x54);
co(game_globals_player_information_t, stun_movement_penalty, 0x80);
co(game_globals_player_information_t, field_84, 0x84);

/* biped_jump (0x1a2290)
 *
 * Attempts to make a biped "jump"/launch along its up axis. Skips if already
 * launched (unit+0x424 bit0) or in a blocking state (unit+0x460 == 1). Computes
 * a launch speed from the biped tag (tag+0x3b4), scaled down by the actor's
 * physics block and (if a global flag is set) a further factor. If the biped's
 * current velocity along up is below the launch speed, boosts it to the launch
 * speed. If actor-controlled, defers to actor_aim_jump (which can veto). On
 * success, writes back the velocity, marks launched (unit+0x424 bit0), clears
 * unit+0x45c, resets unit+0x430, and disconnects two structure-BSP markers.
 *
 * Register arg: unit_handle in EDI. Returns the success flag (char).
 * Confirmed offsets/calls from disassembly; max-speed denom uses arg reuse
 * (game_globals_get(0,0xf4) shares its 0/0xf4 with tag_block_get_element).
 */
char biped_jump(int unit_handle)
{
  biped_data_t *biped;
  char *biped_tag;
  game_globals_player_information_t *player_information;
  real_vector3d *vel_ptr;
  int actor_handle;
  int zero_idx;
  float max_speed;
  float vel[3];
  float *vel0_ptr;
  float dot;
  char success;
  char aim_flag;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  biped_tag = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  zero_idx = 0;
  if (((uint8_t)biped->flags & 1) == 0 && biped->landing != 1) {
    max_speed = biped_definition_jump_velocity(biped_tag);
    vel0_ptr = &vel[0];
    success = 1;
    if (biped->unit.player_index.value != NONE) {
      player_information =
        tag_block_get_element(&game_globals_player_information(game_globals_get()), 0,
                              sizeof(game_globals_player_information_t));
      max_speed = (REAL_ONE_POOL -
                   biped->unit.body_stun * player_information->field_84) *
                  max_speed;
    }
    if (CHEAT_SUPER_JUMP != '\0' && biped->unit.player_index.value != NONE) {
      max_speed = max_speed * REAL_4_0_POOL;
    }

    vel_ptr = &biped->unit.object.translational_velocity;
    vel[0] = vel_ptr->i;
    vel[1] = vel_ptr->j;
    vel[2] = vel_ptr->k;
    dot = vel[1] * biped->unit.object.up.j + vel[2] * biped->unit.object.up.k +
          vel[zero_idx] * biped->unit.object.up.i;
    if (dot < max_speed) {
      dot = max_speed - dot;
      vel[zero_idx] = dot * biped->unit.object.up.i + *vel0_ptr;
      vel[1] = dot * biped->unit.object.up.j + vel[1];
      vel[2] = dot * biped->unit.object.up.k + vel[2];
    }

    actor_handle = biped->unit.swarm_actor_index.value;
    if (actor_handle == NONE) {
      actor_handle = biped->unit.actor_index.value;
    }
    if (actor_handle != NONE) {
      aim_flag = ((char)biped->unit.animation_state == 0x27 ||
                  (char)biped->unit.animation_state == 0x28) ?
                   1 :
                   0;
      success = (char)actor_aim_jump(actor_handle, unit_handle, aim_flag,
                                     max_speed, vel);
      if (success == zero_idx) {
        return success;
      }
    }

    vel_ptr->i = vel[0];
    vel_ptr->j = vel[1];
    vel_ptr->k = vel[2];
    biped->flags |= 1;
    biped->jump_recovery_timer = (int8_t)zero_idx;
    biped->support_surface_index = NONE;
    biped_make_footstep(unit_handle, 4, 0);
    biped_make_footstep(unit_handle, 4, 1);
    return success;
  }
  return (char)zero_idx;
}

/* biped_try_to_make_footsteps (0x1a2440) — per-tick footstep / animation-marker
 * event step
 *
 * Step in the biped update dispatcher (biped_update). Classifies the biped's
 * movement state (object+0x253):
 *   - states 2,3  -> walking (is_walking)
 *   - states 4..7 -> moving fast enough if horizontal velocity squared
 *     (object+0x228..0x230) exceeds threshold 0x25337c (is_fast)
 * If the biped has an active animation (object+0x80 != NONE), looks up the
 * 'antr' animation graph (via object+0x7c) and the current animation element
 * (object+0x80, stride 0xb4 in antr+0x74):
 *   - walking and on the animation's first frame (object+0x82 == 0): fires two
 *     collision-user events (param_2 = 3, indices 0 then 1).
 *   - fast and the current frame matches one of the element's two footstep
 *     marker frames (element+0x40 / element+0x41): fires one event with
 *     index = which marker matched (0 = +0x40, 1 = +0x41) and param_2 = whether
 *     the biped is crouched (object+0x257 == 2).
 * Then runs the slip/recovery counter at object+0x45b based on object+0x42a:
 *   - 0x42a == 0: increment the counter; once it reaches 4 fire two events
 *     (param_2 = 3, indices 0,1) and reset the counter to 0.
 *   - 0x42a == 1: latch the counter to 1 and return.
 *   - otherwise:  reset the counter to 0.
 *
 * unit_handle arrives in EDI (register parameter); no stack arguments.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...)
 * and tag_get('antr', object+0x7c); tag_block_get_element(antr+0x74, idx,
 * 0xb4); jump table at 0x1a25c0 (states 2..7); velocity sum-of-squares vs
 * 0x25337c; biped_make_footstep(unit, param_2, idx) idx routed to BX.
 */
void biped_try_to_make_footsteps(int unit_handle /* @edi */)
{
  biped_data_t *biped;
  animation_graph_t *animation_graph;
  char *animation;
  char is_walking;
  char is_fast;
  char matched_second;
  char counter;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  is_walking = 0;
  is_fast = 0;
  switch ((char)biped->unit.animation_state) {
  case 2:
  case 3:
    is_walking = 1;
    break;
  case 4:
  case 5:
  case 6:
  case 7:
    if (biped->unit.throttle.x * biped->unit.throttle.x +
          biped->unit.throttle.y * biped->unit.throttle.y +
          biped->unit.throttle.z * biped->unit.throttle.z >
        REAL_0_25_POOL) {
      is_fast = 1;
    }
    break;
  default:
    break;
  }

  if (biped->unit.object.unk_128 != NONE) {
    animation_graph = tag_get(TAG_GROUP_ANTR, biped->unit.object.unk_124);
    animation = tag_block_get_element(&animation_graph->animations,
                                      (int)biped->unit.object.unk_128, 0xb4);
    if (is_walking) {
      if (biped->unit.object.unk_130 == 0) {
        biped_make_footstep(unit_handle, 3, 0);
        biped_make_footstep(unit_handle, 3, 1);
      }
    } else if (is_fast && ((animation[0x40] != 0) || (animation[0x41] != 0)) &&
               ((biped->unit.object.unk_130 == (unsigned char)animation[0x40]) ||
                (biped->unit.object.unk_130 ==
                 (unsigned char)animation[0x41]))) {
      matched_second =
        (biped->unit.object.unk_130 != (unsigned char)animation[0x40]);
      biped_make_footstep(unit_handle, ((char)biped->unit.base_seat_index == 2),
                          matched_second);
    }
  }

  switch ((char)biped->state) {
  case 0:
    if (biped->stop_ticks < 1) {
      return;
    }
    counter = (char)(biped->stop_ticks + 1);
    biped->stop_ticks = counter;
    if (counter < 4) {
      return;
    }
    biped_make_footstep(unit_handle, 3, 0);
    biped_make_footstep(unit_handle, 3, 1);
    break;
  case 1:
    biped->stop_ticks = 1;
    return;
  }
  biped->stop_ticks = 0;
}

/* biped_find_nearby_support_surface (0x1a25e0)
 *
 * Finds the BSP surface most directly under the biped's camera point and
 * records it on the unit. Gets the biped camera position, sphere-tests the
 * collision BSP for nearby surfaces (collision_bsp_test_sphere), then for each
 * returned surface index looks up its plane (orienting the normal by the
 * surface's plane-index sign bit) and selects the surface whose signed plane
 * distance to the camera point is smallest. The winning surface index is stored
 * at unit+0x430 and its oriented normal is written to unit+0x46c (4 floats) and
 * unit+0x30 (3 floats). Brackets the work in the collision-user-depth stack
 * (global 0x4761d8 / stack 0x5a8c80), asserting depth < 0x20 on entry and > 1
 * on exit.
 *
 * Confirmed: @ecx = unit_handle (MOV EDI,ECX at 0x1a25ef); 0x1054-byte stack
 *   frame (_chkstk); out_pos vec3 at EBP-0x44; radius = camera_height +
 * [0x2533e8] passed via push-then-fstp; surface index from results[1+i]; plane
 * lookups via tag_block_get_element (element sizes 0xc and 0x10); normal
 * negated when the plane index sign bit is set; dot FPU order z*nz, then +x*nx,
 * +y*ny, -d. Both tag blocks hang off the collision BSP (LEA ECX,[ESI+0x3c] /
 * ADD ESI,0xc with ESI = global_collision_bsp_get()), not off the surface
 * data passed to the sphere test. Inferred: "find supporting surface"
 * semantics from the stores into
 * unit+0x430/ 0x46c/0x30 and the min-distance selection.
 */
void biped_find_nearby_support_surface(int unit_handle /* @ecx */)
{
  int results[1028]; /* 0x1010 bytes: results[0]=count, results[1+i]=surface idx
                      */
  vector3_t cam_pos;
  vector3_t *cam_ptr;
  float height_offset;
  float camera_height;
  biped_data_t *biped;
  char *surface_data;
  void *bsp;
  int16_t depth;
  int count;
  int i;
  int *surface_elem;
  float *plane;
  int plane_index;
  int best_index;
  float best_dist;
  real_plane3d n;
  real_plane3d best_n;
  float dist;
  float radius;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  bsp = global_collision_bsp_get();
  biped_get_camera_height_and_offset(unit_handle, &cam_pos, &height_offset,
                                     &camera_height);

  if (global_current_collision_user_depth >= 0x20) {
    display_assert("collision_user_depth<NUMBER_OF_COLLISION_USERS",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf71, true);
    system_exit(-1);
  }
  depth = global_current_collision_user_depth;
  collision_user_stack[depth] = 7;
  global_current_collision_user_depth = (int16_t)(depth + 1);

  cam_ptr = &cam_pos;
  surface_data = (char *)breakable_surfaces_get_bsp_surface_data();
  radius = camera_height + REAL_0_05_POOL;
  if ((char)collision_bsp_test_sphere((int)bsp, 0x100, (int)surface_data,
                                      (int)cam_ptr, *(int *)&radius,
                                      results) != 0) {
    count = results[0];
    best_index = -1;
    *(int *)&best_dist = 0x7f7fffff;
    i = 0;
    if (count > 0) {
      do {
        surface_elem = (int *)tag_block_get_element(
          (void *)((char *)bsp + 0x3c), results[1 + i], 0xc);
        plane_index = *surface_elem;
        plane = (float *)tag_block_get_element((void *)((char *)bsp + 0xc),
                                               plane_index & 0x7fffffff, 0x10);
        if (plane_index < 0) {
          n.normal[0] = -plane[0];
          n.normal[1] = -plane[1];
          n.normal[2] = -plane[2];
          n.d = -plane[3];
        } else {
          n.normal[0] = plane[0];
          n.normal[1] = plane[1];
          n.normal[2] = plane[2];
          n.d = plane[3];
        }
        dist = (cam_pos.z * n.normal[2] + n.normal[0] * cam_pos.x + cam_pos.y * n.normal[1]) - n.d;
        if (dist < best_dist) {
          best_index = results[1 + i];
          best_n.normal[0] = n.normal[0];
          best_n.normal[1] = n.normal[1];
          best_n.normal[2] = n.normal[2];
          best_n.d = n.d;
          best_dist = dist;
        }
        i = (int)(int16_t)(i + 1);
      } while (i < count);

      if (best_index != -1) {
        biped->support_surface_index = best_index;
        biped->ground_plane.normal[0] = best_n.normal[0];
        biped->ground_plane.normal[1] = best_n.normal[1];
        biped->ground_plane.normal[2] = best_n.normal[2];
        biped->ground_plane.d = best_n.d;
        biped->unit.object.up.i = best_n.normal[0];
        biped->unit.object.up.j = best_n.normal[1];
        biped->unit.object.up.k = best_n.normal[2];
      }
    }
  }

  if (global_current_collision_user_depth <= 1) {
    display_assert("collision_user_depth>0",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf94, true);
    system_exit(-1);
  }
  global_current_collision_user_depth = (int16_t)(global_current_collision_user_depth - 1);
}

/* biped_verify_object_vectors (0x1a2800)
 *
 * Biped vector-failure assert: validates that the biped's forward axis
 * (unit+0x24) and up axis (unit+0x30) are perpendicular unit vectors
 * (valid_real_normal3d_perpendicular). If not, formats a diagnostic naming the
 * biped tag, its physics mode (flying / player-physics / climb / normal from
 * tag flags at +0x2f4), its dead/limping state, the supplied failure-kind
 * string, and the two offending vectors, then asserts and exits.
 *
 * Confirmed: @eax = unit_handle (caller MOV EAX,[EBP+8] then PUSH str; ADD
 * ESP,4); perpendicular check on (unit+0x24, unit+0x30); six floats
 * unit+0x24..+0x38 promoted to double for csprintf; dead bit =
 * byte[unit+0xb6]&4, limp bit = byte[unit+0x424]&0x20; tag flags at tag+0x2f4
 * (&4 flying, &2 player-physics, &0x40 climb); display_assert at
 * bipeds.c:0x55d. Inferred: "vector failure" / mode-string semantics from the
 * format string.
 */
void biped_verify_object_vectors(int unit_handle /* @eax */,
                                 const char *failure_kind)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  const char *limp_str;
  const char *dead_str;
  const char *mode_str;
  uint32_t flags;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  if (valid_real_normal3d_perpendicular(&biped->unit.object.forward,
                                        &biped->unit.object.up) == 0) {
    definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

    limp_str = "/limping";
    if (((uint8_t)biped->flags & 0x20) == 0) {
      limp_str = "";
    }
    dead_str = "/dead";
    if ((biped->unit.object.damage_flags & 4) == 0) {
      dead_str = "";
    }
    flags = definition->biped_flags;
    if ((flags & 4) != 0) {
      mode_str = "flying";
    } else if ((flags & 2) != 0) {
      mode_str = "player-physics";
    } else if ((flags & 0x40) != 0) {
      mode_str = "climb";
    } else {
      mode_str = "normal";
    }

    csprintf(
      (char *)0x5ab100,
      "biped %s (%s%s%s): %s vector failure: (%f, %f, %f) / (%f, %f, %f)",
      tag_name_strip_path(tag_get_name(biped->unit.object.tag_index)), mode_str,
      dead_str, limp_str, failure_kind, (double)biped->unit.object.forward.i,
      (double)biped->unit.object.forward.j, (double)biped->unit.object.forward.k,
      (double)biped->unit.object.up.i, (double)biped->unit.object.up.j,
      (double)biped->unit.object.up.k);
    display_assert((const char *)0x5ab100, "c:\\halo\\SOURCE\\units\\bipeds.c",
                   0x55d, true);
    system_exit(-1);
  }
}

/* biped_update_airborne (0x1a2900) — post-airborne update step
 *
 * Step in the biped update dispatcher (biped_update), reached when the biped
 * is airborne (object+0x424 bit 0). When the biped is flying through the air
 * (biped_flying_through_air) and the tag allows airborne aiming control
 * (tag+0x2f4 bit 8 / 0x100), and the movement state (object+0x253) is neither
 * 0x1f nor 0x29, perturbs the biped's aim/facing:
 *   - Picks a small random pitch in [3deg, 5deg] (random_real_range).
 *   - If the biped's airborne timer (object+0x38) is below threshold 0x2533f0,
 *     OR the horizontal facing (cross of object+0x30 with world up 0x31fc44)
 *     is degenerate (length <= 0x2533c0), uses a fully random yaw in [0, 2pi)
 *     to build the facing axis; otherwise uses the computed horizontal facing.
 *   - Scales the facing by the random pitch and adds it into object+0x3c
 *     (vector3d_scale_add), then runs the airborne aim helper biped_apply_rotation.
 * Asserts the biped is airborne (object+0x424 bit 0). Writes the airborne
 * landing-prep state into *state (0x28 for states 0x27/0x28, 0x14 for state
 * 0x14 or when aiming control was active) and emits the "post-airborne" marker.
 *
 * unit_handle and state are both cdecl stack arguments (caller pushes
 * &update_state then unit_handle).
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...);
 * biped_flying_through_air (0x1a0db0); random_real_range over global seed
 * (0x10b0d0) with min/max 0x3d567750/0x3db2b8c2; cross_product3d(object+0x30,
 * world_up, tmp); normalize3d (0x13010); real_random_range(0, 2pi) random yaw;
 * vector3d_from_angle (0x10cc70); vector3d_scale_add (0x12f80) into
 * object+0x3c.
 */
void biped_update_airborne(int unit_handle, char *state)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  char aim_control;
  char move_state;
  float pitch;
  real_vector3d facing;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  aim_control = 0;
  if ((biped_flying_through_air(unit_handle) != 0) &&
      (aim_control = 1, (definition->biped_flags & 0x100) != 0)) {
    move_state = (char)biped->unit.animation_state;
    if ((move_state != 0x1f) && (move_state != 0x29)) {
      pitch = random_real_range(get_global_random_seed_address(), 0.05235988f,
                                0.08726646f);
      if (biped->unit.object.up.k < REAL_0_8_POOL) {
        cross_product3d(&biped->unit.object.up, (real_vector3d *)global_up_vector_ptr, &facing);
        if (normalize3d(&facing) > REAL_ZERO_POOL) {
          goto have_facing;
        }
      }
      vector3d_from_angle(&facing, real_random_range(0.0f, 2.f * _pi));
    have_facing:
      vector3d_scale_add((real_point3d *)&biped->unit.object.angular_velocity, &facing, pitch,
                         (real_point3d *)&biped->unit.object.angular_velocity);
    }
    biped_apply_rotation(unit_handle);
  }

  if (((uint8_t)biped->flags & 1) == 0) {
    display_assert("TEST_FLAG(biped->biped.flags, _biped_airborne_bit)",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xa61, true);
    system_exit(-1);
  }

  move_state = (char)biped->unit.animation_state;
  if ((move_state == 0x27) || (move_state == 0x28)) {
    *state = 0x28;
  } else if ((move_state == 0x14) || (aim_control)) {
    *state = 0x14;
  }

  biped_verify_object_vectors(unit_handle, "post-airborne");
}

/* biped_update_landing (0x1a2a60) — post-landing update step
 *
 * Step in the biped update dispatcher (biped_update), reached when the biped
 * has a landing-animation index (object+0x460 != NONE). Increments the
 * landing-frame counter (object+0x428); once it reaches the landing frame
 * count (object+0x429) the landing animation index (object+0x460) is cleared
 * to NONE (0xffff). When no cinematic is running and the landing has just
 * begun (counter == 2) or is effectively a one-frame landing (index already
 * cleared and frame count < 2), fires two collision-user events via
 * biped_make_footstep with selector indices 0 then 1 (param_2 = 5). Writes the
 * resulting landing-sound id into *state (0x16 if index == 1, else 0x15) and
 * emits the "post-landing" timing marker via biped_verify_object_vectors.
 *
 * unit_handle arrives in EDI (register parameter); state is the only stack
 * argument (caller pushes &update_state byte).
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...);
 * cinematic_in_progress (0x930a0); biped_make_footstep(unit, 5, idx) idx->BX
 * (0,1).
 */
void biped_update_landing(int unit_handle /* @edi */, char *state)
{
  biped_data_t *biped;
  char counter;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  counter = (char)(biped->landing_recovery_counter + 1);
  biped->landing_recovery_counter = counter;
  if (counter >= biped->landing_recovery_time) {
    biped->landing = NONE;
  }

  if ((cinematic_in_progress() == 0) &&
      ((biped->landing_recovery_counter == 2) ||
       ((biped->landing == NONE) && (biped->landing_recovery_time < 2)))) {
    biped_make_footstep(unit_handle, 5, 0);
    biped_make_footstep(unit_handle, 5, 1);
  }

  *state = (char)((biped->landing == 1) + 0x15);
  biped_verify_object_vectors(unit_handle, "post-landing");
}

/* biped_update_slipping (0x1a2b10) — post-slipping update step
 *
 * Step in the biped update dispatcher (biped_update). If the biped's slipping
 * counter (object+0x45a) has exceeded 3 ticks AND the object's linear velocity
 * magnitude squared (object+0x18..0x20) exceeds the "moving" threshold at
 * 0x25620c (== 1/900), fires two collision-user events via biped_make_footstep
 * with selector indices 0 then 1 (param_2 = 2). Always emits the
 * "post-slipping" timing marker via biped_verify_object_vectors.
 *
 * unit_handle arrives in EDI (register parameter). The caller (biped_update)
 * also pushes a pointer to its update-state byte buffer, but this function
 * never reads it; it is not declared as a parameter because the original is
 * frameless (no EBP frame, no stack-arg load) — the caller's cdecl push and
 * matching ESP cleanup are unaffected.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...);
 * velocity sum-of-squares at +0x18/+0x1c/+0x20 vs threshold 0x25620c;
 * biped_make_footstep(unit, 2, idx) with idx routed to BX (0 then 1).
 */
void biped_update_slipping(int unit_handle /* @edi */)
{
  biped_data_t *biped;
  real_vector3d *velocity;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  velocity = &biped->unit.object.translational_velocity;
  if ((biped->slipping_ticks > 3) &&
      (velocity->k * velocity->k + velocity->j * velocity->j +
         velocity->i * velocity->i >
       REAL_ONE_OVER_900_POOL)) {
    biped_make_footstep(unit_handle, 2, 0);
    biped_make_footstep(unit_handle, 2, 1);
  }

  biped_verify_object_vectors(unit_handle, "post-slipping");
}

/* Rumble definition record passed to rumble_player_impulse (0x3c bytes; the
 * callee copies all of it and reads +0x00/+0x04/+0x14/+0x18/+0x28). */
typedef struct biped_rumble_definition {
  real field_00;
  real field_04;
  int16_t field_08;
  byte pad_0a[0x14 - 0x0a];
  real field_14;
  real field_18;
  byte pad_1c[0x28 - 0x1c];
  real field_28;
  byte pad_2c[0x3c - 0x2c];
} biped_rumble_definition;
cs(biped_rumble_definition, 0x3c);
co(biped_rumble_definition, field_08, 0x08);
co(biped_rumble_definition, field_14, 0x14);
co(biped_rumble_definition, field_28, 0x28);

/* biped_update_jumping (0x1a2b90)
 *
 * While grounded, advances jump_recovery_timer (capped at 0x7f) and jumps once
 * the jump control bit is held and the timer exceeds 5. For a player-driven
 * biped with control bits 0x800 and 0x2000 both set, steers the translational
 * velocity toward the aiming vector, marks the biped airborne and plays a
 * rumble impulse; with only bit 0 set while airborne it damps the velocity.
 *
 * unit_handle arrives in EAX. The caller also pushes its update-state pointer,
 * which this function never reads.
 */
void biped_update_jumping(int unit_handle /* @eax */)
{
  biped_data_t *biped;
  player_data_t *player;
  unsigned int flags;
  char aim_reoriented;
  float angle;
  float weight;
  float scale;
  real_vector3d tmp;
  biped_rumble_definition rumble_def;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  if ((((uint8_t)biped->flags & 1) == 0) && (biped->landing != 1)) {
    if (biped->jump_recovery_timer < 0x7f) {
      biped->jump_recovery_timer = (int8_t)(biped->jump_recovery_timer + 1);
    }
    if ((((uint8_t)biped->unit.control_flags & 2) != 0) &&
        (biped->jump_recovery_timer > 5)) {
      biped_jump(unit_handle);
    }
  }

  if ((CHEAT_JETPACK != 0) && (biped->unit.player_index.value != NONE)) {
    player = datum_get(PLAYER_DATA_VOID, biped->unit.player_index.value);
    flags = biped->unit.control_flags;
    aim_reoriented = 0;
    if (((flags & 0x800) == 0) || ((flags & 0x2000) == 0)) {
      if (((flags & 1) != 0) && (((uint8_t)biped->flags & 1) != 0)) {
        scale_vector3d(&biped->unit.object.translational_velocity, -0.2f,
                     &biped->unit.object.translational_velocity);
      }
    } else {
      /* z,y,x dot order matches the original FLD sequence. */
      if (biped->unit.object.translational_velocity.k *
              biped->unit.aiming_vector.k +
            biped->unit.object.translational_velocity.j *
              biped->unit.aiming_vector.j +
            biped->unit.object.translational_velocity.i *
              biped->unit.aiming_vector.i >=
          REAL_ZERO_POOL) {
        angle = dot_product3d(&biped->unit.object.translational_velocity,
                             &biped->unit.aiming_vector);
      } else {
        angle = REAL_ZERO_POOL;
      }
      weight = REAL_FIVE_SEVENTHS_POOL * angle;
      if (weight < REAL_ZERO_POOL) {
        scale = 0.0f;
      } else if (weight > REAL_ONE_POOL) {
        scale = 1.0f;
      } else {
        scale = weight;
      }
      vector3d_scale_add((real_point3d *)&biped->unit.object.translational_velocity,
                         &biped->unit.aiming_vector, -angle, (real_point3d *)&tmp);
      vector3d_scale_add((real_point3d *)&biped->unit.object.translational_velocity, &tmp,
                         -0.2f, (real_point3d *)&biped->unit.object.translational_velocity);
      vector3d_scale_add((real_point3d *)&biped->unit.object.translational_velocity,
                         &biped->unit.aiming_vector,
                         scale * REAL_0_04_POOL -
                           scale * scale * REAL_0_05_POOL +
                           REAL_0_01_25BB10_POOL,
                         (real_point3d *)&biped->unit.object.translational_velocity);
      biped->flags |= 1;
      aim_reoriented = 1;
    }
    if ((player->local_player_index != -1) && (aim_reoriented)) {
      csmemset(&rumble_def, 0, sizeof(rumble_def));
      rumble_def.field_00 = 1.0f;
      rumble_def.field_04 = 0.3f;
      rumble_def.field_08 = 3;
      rumble_player_impulse(player->local_player_index, &rumble_def.field_00, 1.0f, 1.0f);
    }
  }
}

/* biped_build_flying_axes (0x1a2d90)
 *
 * Builds an orthonormal forward/left/up basis for a flying biped from a given
 * forward vector. Sets up = world up, left = up x forward (renormalized; if
 * degenerate, retries with up = world forward), then up = forward x left
 * (renormalized). Asserts the result is a valid axis triple.
 *
 * Confirmed: normalize3d (0x13010); global up = 0x31fc44, forward = 0x31fc3c;
 * valid_real_vector3d_axes3 (0xf6c40); cross operand order from disassembly.
 * Asserts at bipeds.c:0xb93 (null args) and :0xba2 (invalid axes).
 */
void biped_build_flying_axes(const real_vector3d *forward, real_vector3d *left, real_vector3d *up)
{
  float lc0;
  float lc1;
  float lc2;

  if (forward == NULL || left == NULL || up == NULL) {
    display_assert("forward_vector && left_vector && up_vector",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xb93, true);
    system_exit(-1);
  }

  up->i = global_up_vector_ptr[0];
  up->j = global_up_vector_ptr[1];
  up->k = global_up_vector_ptr[2];

  /* left = up x forward (batched: all components before any store) */
  lc2 = forward->j * up->i - up->j * forward->i;
  lc1 = up->k * forward->i - forward->k * up->i;
  lc0 = up->j * forward->k - up->k * forward->j;
  left->i = lc0;
  left->j = lc1;
  left->k = lc2;
  if (normalize3d(left) == REAL_ZERO_POOL) {
    up->i = global_forward_vector_ptr[0];
    up->j = global_forward_vector_ptr[1];
    up->k = global_forward_vector_ptr[2];
    lc2 = forward->j * up->i - up->j * forward->i;
    lc1 = up->k * forward->i - forward->k * up->i;
    lc0 = up->j * forward->k - up->k * forward->j;
    left->i = lc0;
    left->j = lc1;
    left->k = lc2;
    normalize3d(left);
  }

  /* up = forward x left */
  lc2 = left->j * forward->i - forward->j * left->i;
  lc1 = forward->k * left->i - forward->i * left->k;
  lc0 = forward->j * left->k - left->j * forward->k;
  up->i = lc0;
  up->j = lc1;
  up->k = lc2;
  normalize3d(up);

  if (valid_real_vector3d_axes3(forward, left, up) == 0) {
    display_assert(
      csprintf(error_string_buffer,
               "%s, %s, %s: assert_valid_real_vector3d_axes3(%f, %f, %f / %f, "
               "%f, %f / %f, %f, %f)",
               "forward_vector", "left_vector", "up_vector", (double)forward->i,
               (double)forward->j, (double)forward->k, (double)up->i,
               (double)up->j, (double)up->k, (double)left->i, (double)left->j,
               (double)left->k),
      "c:\\halo\\SOURCE\\units\\bipeds.c", 0xba2, true);
    system_exit(-1);
  }
}

/* Collision/line-of-sight query result entry (0x2c bytes), as filled by
 * collision_move_pill into a 16-element local array (stride 0x2c). The
 * back-half loops walk it with EDI = entry+0x14, so EDI+0x14 is flags (+0x28). */
typedef struct biped_collision_result {
  float point[4]; /* +0x00 .. +0x0c */
  real_plane3d plane; /* +0x10 */
  int object_handle; /* +0x20 */
  int surface_handle; /* +0x24 */
  uint8_t flags; /* +0x28 tested &4 / &8 as a byte */
  uint8_t field_29;
  int16_t field_2a;
} biped_collision_result;
cs(biped_collision_result, 0x2c);
co(biped_collision_result, plane, 0x10);
co(biped_collision_result, flags, 0x28);
co(biped_collision_result, field_29, 0x29);
co(biped_collision_result, field_2a, 0x2a);

/* Movement request built by biped_update_moving at [EBP-0xe4] and handed to
 * biped_update_physics in ESI. */
typedef struct biped_physics {
  int biped_index; /* +0x00  = unit_handle */
  uint16_t in_flags; /* +0x04  OR-built from biped+0x424/+0xb6, tag+0x2f4 */
  uint8_t pad_06[2];
  real_point3d position; /* +0x08  biped_get_camera_height_and_offset out_pos */
  real_vector3d forward; /* +0x14  biped+0x24, or biped+0x1d4 (player path) */
  real_vector3d aiming; /* +0x20  biped+0x24 or unit_get_aiming_vector */
  real_vector3d velocity; /* +0x2c  biped+0x18; passed to biped_bumped_object */
  float crouch_velocity; /* +0x38 */
  real_vector3d movement_desired; /* +0x3c */
  float movement_penalty; /* +0x48 */
  float acceleration_maximum; /* +0x4c */
  float airborne_acceleration_maximum; /* +0x50 */
  float height; /* +0x54  out_height_offset of 0x1a0890 */
  float width; /* +0x58  out_camera_height of 0x1a0890; subtracted from z */
  float ground_tangential_velocity_max; /* +0x5c  FLT_MAX / 0.1 */
  float ground_tangential_angle; /* +0x60  0 / 0.5 */
  float minimum_normal_k; /* +0x64  tag+0x4d0 */
  float downhill_k0; /* +0x68  tag+0x4d4 */
  float downhill_k1; /* +0x6c  tag+0x4d8 */
  float downhill_velocity_scale; /* +0x70  tag+0x364 */
  float uphill_k0; /* +0x74  tag+0x4dc */
  float uphill_k1; /* +0x78  tag+0x4e0 */
  float uphill_velocity_scale; /* +0x7c  tag+0x370 */
  real_plane3d ground_plane; /* +0x80  biped+0x46c in, copied back out */
  int existing_support_surface_index; /* +0x90  biped+0x430 */
  uint8_t pad_94[4];
  int bumped_object_index; /* +0x98  -> biped_bumped_object @<edi> */
  int elevator_object_index; /* +0x9c  -> biped+0x42c / +0x42b ticks */
  uint16_t out_flags; /* +0xa0  bits 1/2/4/0x10 read here */
  uint8_t pad_a2[2];
  int support_surface_index; /* +0xa4  -> biped+0x430 */
  int field_a8; /* +0xa8  written by biped_update_physics, unread here */
  real_point3d new_position; /* +0xac */
  real_vector3d new_velocity; /* +0xb8 */
  float landing_velocity; /* +0xc4 */
  float field_c8; /* +0xc8  written by biped_update_physics, unread here */
} biped_physics;
cs(biped_physics, 0xcc);
co(biped_physics, in_flags, 0x04);
co(biped_physics, movement_desired, 0x3c);
co(biped_physics, ground_plane, 0x80);
co(biped_physics, bumped_object_index, 0x98);
co(biped_physics, out_flags, 0xa0);
co(biped_physics, new_position, 0xac);
co(biped_physics, landing_velocity, 0xc4);

/* biped_update_physics (0x1a2f40) — biped ground/movement physics step.
 *
 * Largest function in bipeds.obj (~5.3 KB, ~1613 instructions). The Ghidra
 * decompiler output for this routine is heavily polluted ("type propagation
 * not settling"; return-address pushes modelled as struct stores; pervasive
 * (char*)/(char**) casts on what are really float scratch slots), so it was
 * used only as a control-flow skeleton and EVERY store offset, FPU operand
 * order, branch sense, and the integer-vs-float nature of each store was
 * cross-checked against the raw disassembly.
 *
 * ABI (Confirmed): single register argument in ESI (read-before-write at the
 *   prologue; caller biped_update_moving does `LEA ESI,[EBP-0xe4]; CALL` with no
 *   pushes and discards EAX). Returns void. Huge _chkstk frame (0xafac) for a
 *   ~44 KB debug-draw scratch buffer passed to collision_get_features_in_sphere.
 *
 * `physics` (ESI) is a biped movement/physics state struct accessed as a flat
 * float array. Field map (index = byte offset / 4), all Confirmed:
 *   +0x04  flags (u16); bit 0x10/0x20/0x01 select movement mode, bit 0x200
 *          selects the surface-walk sub-mode, bits 0x40/0x80/0x100 select the
 *          debug-draw colour.
 *   +0x08  position[3]
 *   +0x14  control / forward basis axis (physics[5..7])
 *   +0x20  ground-plane normal (physics[8..0xa]); +0x2c plane_d at physics[0xb]
 *   +0x2c  velocity[3] (physics[0xb..0xd])  [NOTE: aliases ground-plane d]
 *   +0x38  step-height offset (physics[0xe])
 *   +0x3c  control direction (physics[0xf..0x11])
 *   +0x48  friction (used as 1.0 - friction; 1.0f = REAL_ONE_POOL)
 *   +0x4c  max-speed clamp (mode 0x10, physics[0x13])
 *   +0x50  max-speed clamp (mode 0x01, physics[0x14])
 *   +0x54/0x58  line-of-sight direction/origin args to collision_move_pill
 *   +0x60..0x7c slope-response material curve breakpoints (physics[0x18..0x1f])
 *   +0x80  ground-plane struct (physics+0x80 = physics[0x20..0x23])
 *   +0xa0  result/state word (mixed u16/byte access to the SAME field)
 *   +0xa4  surface datum handle (physics[0x29]); +0xa8 plane reference
 *          (physics[0x2a]) — both 32-bit handles in float slots, "none" = -1
 *   +0xac  new_position (assert string &physics->new_position @0x2b5080)
 *   +0xb8  new_velocity (assert string &physics->new_velocity @0x2b5068)
 *   +0xc4  result point (physics[0x31]); +0xc8 step distance (physics[0x32])
 *
 * NOTE (Confirmed): byte offsets +0xac and +0xb8 are new_position /
 * new_velocity from the binary's own assert strings — the decompiler's field
 * labels for these are inverted; the assert strings are authoritative.
 *
 * NOTE (Confirmed): the ESI[0x26]/[0x27]/[0x29]/[0x2a] "−NAN" stores the
 * decompiler shows are integer `MOV dword ptr [...],0xffffffff` (the "none"
 * datum handle), NOT float NaN loads — verified in the listing. The NaN
 * *checks* in the two final asserts are `AND reg,0x7f800000; CMP 0x7f800000`.
 *
 * Status: dormant (ported=false). This is a faithful full reconstruction kept
 * dead-code while iterating; the orchestrator flips the port toggle.
 */
void biped_update_physics(void *physics_arg /* @esi */)
{
  biped_physics *physics;
  real_vector3d *velocity; /* EBX */
  real_point3d *position; /* EDI */
  unsigned short flags;
  unsigned int flags_word; /* full flags promoted [EBP-0x48] */
  float damp; /* 1.0 - friction (physics->movement_penalty) */
  float length3;
  float clamp;

  /* --- back-half locals --- */
  char debug_scratch[44040]; /* local_afb0: ~44 KB collision_get_features_in_sphere buffer (forces
                              * the _chkstk prologue the original uses) */
  char surface_result[44]; /* local_3a8: collision_features_test_vector out-result */
  biped_collision_result results[16]; /* local_378 base; collision_move_pill array */
  int result_count; /* local_40: u16 count from collision_move_pill */
  int draw_color; /* pcVar10: debug-draw colour code */
  float gx, gy; /* local_5c, local_60: carried planar normal */
  float pos_world[3]; /* local_34/30: position copy */
  real_vector3d new_pos; /* local_9c/98/94: new_position+step */
  real_vector3d los_dir; /* local_7c/78/74: line-of-sight out point */
  real_point3d surf; /* local_6c/68/64: projected surface point */
  real_vector3d disp; /* local_58/54/50: clamp scratch vec3 */
  float best_t; /* local_4c */
  int best_index; /* local_3c */
  int tval;
  float r, t, fdist;
  float z_delta;
  x87_wide_t planar_scale;
  void *obj;
  void *tag;
  real_point3d los_dir2; /* local_18: second LOS out point */
  unsigned int isnan_tmp;
  char material_local; /* [EBP-0x31]: (flags>>9)&1 material flag */
  physics = physics_arg;
  velocity = &physics->velocity;
  position = &physics->position;

#ifdef HALO_RNG_TRACE
  RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_PHYSICS_ENTRY,
               physics->in_flags,
               *(unsigned int *)&physics->biped_index);
#endif

  gx = 0.0f; /* local_5c = 0.0 (prologue) */
  gy = 0.0f; /* local_60 = 0.0 (prologue) */
  material_local = (char)((physics->in_flags >> 9) & 1);

  /* assert_valid_real_point3d(&physics->position) */
  if (valid_real_point3d((float *)position) == 0) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_point3d(%f, %f, %f)",
                            "&physics->position", (double)position->x,
                            (double)position->y, (double)position->z),
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xbae, true);
    system_exit(-1);
  }
  /* assert_valid_real_vector2d(&physics->velocity) */
  if (real_vector3d_valid(velocity) == 0) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_vector2d(%f, %f, %f)",
                            "&physics->velocity", (double)velocity->i,
                            (double)velocity->j, (double)velocity->k),
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xbaf, true);
    system_exit(-1);
  }

  flags = physics->in_flags;
  flags_word = flags;
  physics->out_flags = 0;

  if ((flags & 0x10) != 0) {
    /* ---- mode 0x10 (0x1a3037..0x1a3149): oriented/flying movement ----
     * The original double-stores the displacement into a raw copy (local_30..)
     * and a unit copy (local_54..); normalize3d destroys the unit copy in
     * place; the clamp branch scales the unit, the no-clamp branch reads back
     * the raw. Modelling that as two vec3s reproduces the movl/movl pairs. */
    real_vector3d left;
    real_vector3d up;
    float t0, t1, t2;
    float dx, dy, dz;

    biped_build_flying_axes(&physics->forward, &left, &up);

    t0 = left.i * physics->movement_desired.j +
         up.i * physics->movement_desired.k +
         physics->movement_desired.i * physics->forward.i;
    t1 = physics->forward.j * physics->movement_desired.i +
         left.j * physics->movement_desired.j +
         up.j * physics->movement_desired.k;
    t2 = physics->forward.k * physics->movement_desired.i +
         left.k * physics->movement_desired.j +
         up.k * physics->movement_desired.k;

    damp = REAL_ONE_POOL - physics->movement_penalty;

    dx = damp * t0 - velocity->i;
    dy = damp * t1 - physics->velocity.j;
    dz = damp * t2 - physics->velocity.k;

    disp.i = dx;
    disp.j = dy;
    disp.k = dz;
    length3 = normalize3d(&disp);
    if (length3 > physics->acceleration_maximum) {
      clamp = physics->acceleration_maximum;
      dx = disp.i * clamp;
      dy = disp.j * clamp;
      dz = disp.k * clamp;
    }
    physics->new_velocity.i = dx + velocity->i;
    physics->new_velocity.j = dy + physics->velocity.j;
    physics->new_velocity.k = dz + physics->velocity.k;
    physics->out_flags =
      (unsigned short)((physics->out_flags & 0xfffd) |
                       1);
    goto LAB_001a36a4;
  }

  if ((flags & 0x20) != 0) {
    /* ---- mode 0x20 (0x1a3152..0x1a318b): planar heading carry ----
     * new_velocity.xy = rotate(control.xy) by the forward axis; z carried as a
     * raw 32-bit copy. gx/gy double-store to both new_velocity and the carried
     * planar normal (fsts + fstps). */
    gx = physics->new_velocity.i =
      physics->movement_desired.i * physics->forward.i -
      physics->forward.j * physics->movement_desired.j;
    *(int *)&physics->new_velocity.k =
      *(int *)&physics->movement_desired.k; /* +0xc0 int mov */
    gy = physics->new_velocity.j =
      physics->movement_desired.i * physics->forward.j +
      physics->movement_desired.j * physics->forward.i;
    goto LAB_001a36a4;
  }

  if ((flags & 1) != 0) {
    /* ---- mode 0x01 (0x1a3198..0x1a3264): ground tangent movement ----
     * The original computes both planar cross-products, THEN damp = 1-friction,
     * duplicates damp (fld st0) and multiplies it into each before subtracting
     * velocity. normalize2d normalizes tang in-place, so keep the raw deltas
     * for the original's no-clamp branch at 0x1a3219. */
    float tang[3];
    float c0, c1;
    float raw0, raw1;
    float dc0; /* damp*c0: 0x1a31c5 FSTP dword [ebp-0x44] narrows it and
                * 0x1a31ce reloads it; damp*c1 (0x1a31c8..0x1a31e2) stays
                * in ST(0). */

    c0 = physics->movement_desired.i * physics->forward.i -
         physics->forward.j * physics->movement_desired.j;
    c1 = physics->movement_desired.i * physics->forward.j +
         physics->movement_desired.j * physics->forward.i;
    damp = REAL_ONE_POOL - physics->movement_penalty;

    dc0 = damp * c0;
    HALO_FLT_ROUNDTRIP(dc0);
    raw0 = dc0 - velocity->i;
    raw1 = damp * c1 - physics->velocity.j;
    tang[0] = raw0;
    tang[1] = raw1;
    /* normalize2d at 0x12f10 is the 2D normalize helper; it only reads/writes
     * tang[0]/tang[1]. Keep tang[2] zero for a complete local vec3. */
    tang[2] = 0.0f;

    length3 = normalize2d((real_vector2d *)tang);
    if (length3 > physics->airborne_acceleration_maximum) {
      clamp = physics->airborne_acceleration_maximum;
      tang[0] = tang[0] * clamp;
      tang[1] = tang[1] * clamp;
    } else {
      tang[0] = raw0;
      tang[1] = raw1;
    }
    physics->new_velocity.i = tang[0] + velocity->i;
    physics->new_velocity.j = tang[1] + physics->velocity.j;
    physics->new_velocity.k = physics->velocity.k - GLOBAL_GRAVITY;
    physics->out_flags =
      (unsigned short)((unsigned char)flags_word & 2);
    goto LAB_001a36a4;
  }

  /* ---- (flags & 1): ground tangent already handled above; fall here only for
   * the default surface-walk mode entered at 0x1a3269. The decompiler's
   * mode-0x01 test (0x1a3190 TEST AL,1) gates the planar tangent above; the
   * surface-walk path proper starts at 0x1a3269. ---- */
  {
    float magnitude; /* local_40 (-0x3c): control-direction magnitude */
    short submode; /* local_38: flags & 0x200 */
    real_vector3d plane_left; /* local_30/2c/28 */
    real_vector3d plane_forward; /* local_24/20/1c */
    real_vector3d direction; /* local_18/14/10 */
    float curve_scale; /* fVar1: slope-response result */
    float damp2;
    char curve_flag; /* local_1: secondary state byte */
    real_vector3d *ground_normal; /* EDI */

    magnitude =
      sqrtf(physics->movement_desired.i * physics->movement_desired.i +
            physics->movement_desired.j * physics->movement_desired.j +
            physics->movement_desired.k *
              physics->movement_desired.k); /* 0x1a327d..0x1a328f */
    submode = (short)(flags_word & 0x200);

    if (submode != 0) {
      /* ---- 0x1a329e: in-plane axis recovery via cross products ---- */
      ground_normal = (real_vector3d *)physics->ground_plane.normal;
      plane_forward.i = physics->aiming.i;
      plane_forward.j = physics->aiming.j;
      plane_forward.k = physics->aiming.k;
      cross_product3d(ground_normal, &plane_forward, &plane_left);
      if (normalize3d(&plane_left) == 0.0f) {
        cross_product3d(ground_normal, (real_vector3d *)global_up_vector_ptr, &plane_left);
        if (normalize3d(&plane_left) == 0.0f) {
          cross_product3d(ground_normal, (real_vector3d *)global_forward_vector_ptr, &plane_left);
          normalize3d(&plane_left);
        }
      }
      cross_product3d(&plane_left, ground_normal, &plane_forward);
      normalize3d(&plane_forward);
      direction.i = plane_forward.i * physics->movement_desired.i +
             plane_left.i * physics->movement_desired.j;
      direction.j = plane_forward.j * physics->movement_desired.i +
             plane_left.j * physics->movement_desired.j;
      direction.k = plane_forward.k * physics->movement_desired.i +
             plane_left.k * physics->movement_desired.j +
             physics->movement_desired.k;
      normalize3d(&direction); /* submode normalizes here, JMP 0x34f6 skips below */
    } else {
      /* else-if and else share the normalize at 0x34ea; only else has
       * the material multiply. */
      /* Routing (0x1a3?: flds physics->ground_plane.normal[2]; fcomps threshold; test 0x41/jne
       * 0x1a33ed): the JUMP taken when physics->ground_plane.normal[2] <= threshold goes to the
       * full-projection body (0x1a33ed, else below); the fall-through
       * (physics->ground_plane.normal[2] > threshold) runs the single-component solve at
       * 0x1a339b. The lift had this inverted (`<=`), so steep planes
       * (physics->ground_plane.normal[2] ~ 0) hit the divide-by-physics->ground_plane.normal[2] below -> 0/0 NaN
       * (new_position.z = -1.#IND) and broken slope movement; full projection
       * (cross-product/Gram-Schmidt) is the steep-plane path. */
      if (physics->ground_plane.normal[2] > REAL_0_0001_POOL) {
        /* ---- 0x1a339b: solve direction.k via the dominant (z) normal component ----
         */
        ground_normal = (real_vector3d *)physics->ground_plane.normal;
        direction.i = physics->movement_desired.i * physics->forward.i -
               physics->forward.j * physics->movement_desired.j;
        gx = direction.i;
        direction.j = physics->movement_desired.j * physics->forward.i +
               physics->movement_desired.i * physics->forward.j;
        gy = direction.j;
        direction.k = (direction.j * physics->ground_plane.normal[1] + direction.i * ground_normal->i) /
               physics->ground_plane.normal[2];
        direction.k = physics->movement_desired.k - direction.k;
      } else {
        /* ---- 0x1a33ed: full ground-plane projection ---- */
        ground_normal = (real_vector3d *)physics->ground_plane.normal;
        plane_forward.i = physics->aiming.i;
        plane_forward.j = physics->aiming.j;
        plane_forward.k = physics->aiming.k;
        cross_product3d((real_vector3d *)global_up_vector_ptr, &plane_forward, &plane_left);
        normalize3d(&plane_left);
        vector3d_scale_add(
          (real_point3d *)&plane_forward, ground_normal, -(plane_forward.i * ground_normal->i + plane_forward.j * ground_normal->j + plane_forward.k * ground_normal->k),
          (real_point3d *)&plane_forward);
        vector3d_scale_add(
          (real_point3d *)&plane_left, ground_normal, -(plane_left.i * ground_normal->i + plane_left.j * ground_normal->j + plane_left.k * ground_normal->k),
          (real_point3d *)&plane_left);
        gx = physics->movement_desired.i * physics->forward.i -
             physics->forward.j * physics->movement_desired.j;
        gy = physics->movement_desired.j * physics->forward.i +
             physics->movement_desired.i * physics->forward.j;
        direction.i = plane_forward.i * physics->movement_desired.i +
               plane_left.i * physics->movement_desired.j;
        direction.j = plane_forward.j * physics->movement_desired.i +
               plane_left.j * physics->movement_desired.j;
        direction.k = plane_forward.k * physics->movement_desired.i +
               plane_left.k * physics->movement_desired.j +
               physics->movement_desired.k;
        /* material gate: multiply direction.k when material_local == 0 */
        if (material_local == 0) {
          direction.k = direction.k * REAL_5_0_POOL;
        }
      }
      normalize3d(&direction); /* shared normalize at 0x34ea */
    }

    /* ---- slope-response curve (0x1a34f6..0x1a358f) ---- */
    {
      curve_flag = 0;

      /* slope-response breakpoint curve (0x1a34f6..0x1a358f). Comparison
       * senses transcribed exactly from the FCOMP/TEST/Jcc pairs:
       *   0x1a3503 FCOMP[0x6c] TEST 0x41 JP   -> goto 3518 iff height > [0x1b]
       *   0x1a351b FCOMP[0x68] TEST 0x5  JP   -> goto 3549 iff height >= [0x1a]
       *   0x1a3549 FCOMP[0x78] TEST 0x1  JNZ  -> goto 355b iff height <  [0x1e]
       *   0x1a355e FCOMP[0x74] TEST 0x41 JNZ  -> goto 358c iff height <= [0x1d]
       */
      if (submode != 0) {
        /* 0x1a34fd: submode != 0 -> JNZ 0x358c, curve = magnitude */
        curve_scale = magnitude;
      } else if (!(direction.k > physics->downhill_k1)) {
        curve_scale = magnitude * physics->downhill_velocity_scale;
      } else if (direction.k < physics->downhill_k0) {
        curve_scale = ((physics->downhill_velocity_scale - REAL_ONE_POOL) *
                         (direction.k - physics->downhill_k0) /
                         (physics->downhill_k1 - physics->downhill_k0) +
                       REAL_ONE_POOL) *
                      magnitude;
      } else if (direction.k >= physics->uphill_k1) {
        curve_scale = magnitude * physics->uphill_velocity_scale;
      } else if (direction.k <= physics->uphill_k0) {
        curve_scale = magnitude;
      } else {
        curve_scale = ((physics->uphill_velocity_scale - REAL_ONE_POOL) *
                         (direction.k - physics->uphill_k0) /
                         (physics->uphill_k1 - physics->uphill_k0) +
                       REAL_ONE_POOL) *
                      magnitude;
      }

      damp2 = (REAL_ONE_POOL - physics->movement_penalty) * curve_scale;
      /* Save the pre-normalize displacement before normalize3d() unit-
       * normalizes disp in place: the keep-raw branch below needs the
       * ORIGINAL (un-normalized) vector, not the unit vector. The original
       * computes each component into plane_forward's stack slots (local_24/20/1c,
       * reused here) and copies to the working disp slots (local_54/50/4c).
       * Collapsing the two into one disp[] dropped the save, so the keep-raw
       * branch returned a unit-length (magnitude 1.0) displacement on every
       * sub-max-speed frame -> "slow then suddenly fast" biped movement. */
      plane_forward.i = direction.i * damp2 - velocity->i;
      disp.i = plane_forward.i;
      plane_forward.j = direction.j * damp2 - physics->velocity.j;
      disp.j = plane_forward.j;
      plane_forward.k = direction.k * damp2 - physics->velocity.k;
      disp.k = plane_forward.k;
      length3 = normalize3d(&disp); /* 0x1a35e2; ST0 -> compare */
      if (length3 <= physics->acceleration_maximum) {
        /* 0x1a361f: keep raw (pre-normalize) disp -> restore saved plane_forward */
        disp.i = plane_forward.i;
        disp.j = plane_forward.j;
        disp.k = plane_forward.k;
        curve_flag = 0;
      } else {
        if (submode == 0) {
          curve_flag =
            (char)(((unsigned char)flags_word >> 1) & 1); /* 0x1a35fb */
        }
        clamp = physics->acceleration_maximum;
        disp.i = disp.i * clamp; /* 0x1a360a */
        disp.j = disp.j * clamp;
        disp.k = disp.k * clamp;
      }

      /* new_velocity = disp - ground_normal*0x25f0d0 + velocity (0x1a3637..) */
      ground_normal = (real_vector3d *)physics->ground_plane.normal;
      r = physics->ground_plane.normal[0] * REAL_ONE_OVER_128_POOL; /* ground_normal->i */
      t = physics->ground_plane.normal[1] * REAL_ONE_OVER_128_POOL;
      fdist = physics->ground_plane.normal[2] * REAL_ONE_OVER_128_POOL;
      physics->out_flags =
        (unsigned short)(-(unsigned short)(curve_flag != 0) & 2); /* 0x1a364d */
      physics->new_velocity.i = (disp.i - r) + velocity->i; /* 0x1a3672 */
      physics->new_velocity.j = (disp.j - t) + physics->velocity.j; /* 0x1a3676 */
      z_delta = disp.k - fdist; /* 0x1a366d: FSTP dword [ebp-0x28] */
      HALO_FLT_ROUNDTRIP(z_delta);
      physics->new_velocity.k = z_delta + physics->velocity.k; /* 0x1a3681 */
#ifdef HALO_RNG_TRACE
      RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_PRE_QUERY_Y,
                   RNG_TRACE_BITS(physics->new_velocity.j),
                   *(unsigned int *)&physics->biped_index);
#endif
      if ((*(uint8_t *)&physics->out_flags & 2) == 0) {
        goto LAB_001a36a4;
      }
      physics->new_velocity.k =
        physics->new_velocity.k - GLOBAL_GRAVITY; /* 0x1a3692 step-down */
    }
  }

LAB_001a36a4:
  /* ---- debug-draw colour select (0x1a36a4..0x1a36d2) ---- */
  flags = physics->in_flags;
  if ((flags & 0x40) != 0) {
    draw_color = 0;
  } else if ((char)flags < 0) {
    draw_color = 0xc0a0;
  } else {
    draw_color =
      (int)((-(unsigned int)((flags & 0x100) != 0) & 0xffdfff00) + 0x20c3a0);
  }

  /* position copy (local_34/30/2c) and ray endpoint (local_9c/98/94). The
   * endpoint z carries the step-height offset physics->crouch_velocity. (0x1a36d2..) */
  pos_world[0] = physics->position.x;
  pos_world[1] = physics->position.y;
  pos_world[2] = physics->position.z;
  new_pos.i = physics->new_velocity.i;
  new_pos.j = physics->new_velocity.j;
  new_pos.k = physics->new_velocity.k + physics->crouch_velocity;

  /* ---- collision/line-of-sight query (0x1a36d2..0x1a3803) ---- */
  if (DEBUG_BIPED_SKIP_COLLISION == '\0') {
    /* live path: 10-arg query. Arg order matches the push sequence at
     * 0x1a37ce..0x1a37f6 (last push = first C arg):
     *   draw_color, &pos_world, &new_pos, physics->height, physics->width,
     *   physics->biped_index, &los_dir2, &los_dir, 0x10, results. */
#ifdef HALO_RNG_TRACE
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_QUERY_POS_WORLD_XY,
                 RNG_TRACE_BITS(pos_world[0]), RNG_TRACE_BITS(pos_world[1]));
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_QUERY_POS_WORLD_Z_NEW_POS_X,
                 RNG_TRACE_BITS(pos_world[2]), RNG_TRACE_BITS(new_pos.i));
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_QUERY_NEW_POS_YZ,
                 RNG_TRACE_BITS(new_pos.j), RNG_TRACE_BITS(new_pos.k));
#endif
    result_count = collision_move_pill(
      (void *)draw_color, (real_point3d *)pos_world, &new_pos, physics->height, physics->width,
      physics->biped_index, &los_dir2, &los_dir, 0x10, results);
#ifdef HALO_RNG_TRACE
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_QUERY_OUT_Y, RNG_TRACE_BITS(los_dir2.y),
                 *(unsigned int *)&physics->biped_index);
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_QUERY_OUT_XY, RNG_TRACE_BITS(los_dir2.x),
                 RNG_TRACE_BITS(los_dir2.y));
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_QUERY_OUT_Z_HANDLE,
                 RNG_TRACE_BITS(los_dir2.z),
                 *(unsigned int *)&physics->biped_index);
#endif
  } else {
    /* debug-draw line record (0x1a3721..0x1a37cc): no query runs, count stays
     * 1. Builds results[0] (point/plane/handles) from the position
     * and the basis at *0x31fc44, plus the los_dir endpoints. Dead in normal
     * play (gated on debug global 0x4e4cf2); objdump offsets 0x7e1..0x889
     * show los_dir (later new_velocity) is bare new_pos.xy/0, while los_dir2
     * (later new_position) is pos_world.xy + new_pos.xy and pos_world[2]. */
    results[0].point[1] = physics->position.x;
    results[0].point[2] = physics->position.y;
    results[0].point[3] = physics->position.z;
    results[0].plane.normal[0] = global_up_vector_ptr[0];
    results[0].point[0] = 0.0f;
    results[0].plane.normal[1] = global_up_vector_ptr[1];
    results[0].plane.normal[2] = global_up_vector_ptr[2];
    results[0].plane.d = -physics->position.z;
    los_dir.i = new_pos.i;
    results[0].surface_handle = -1;
    /* 0x1a3796 word +0x2a = -1; 0x1a37b4/0x1a37bb bytes +0x28/+0x29 = 0. */
    results[0].field_2a = -1;
    results[0].flags = 0;
    results[0].field_29 = 0;
    los_dir.j = new_pos.j;
    *(int *)&results[0].object_handle = physics->biped_index;
    los_dir2.x = pos_world[0] + new_pos.i;
    los_dir2.y = pos_world[1] + new_pos.j;
    los_dir2.z = pos_world[2];
    los_dir.k = 0.0f;
    result_count = 1;
  }

  /* ---- debug-draw stub (0x1a3803..0x1a3880) ---- */
  if (DEBUG_BIPED_PHYSICS != '\0') {
    obj = object_get_and_verify_type(physics->biped_index, -1);
    if (((object_data_t *)obj)->owner_player_index != -1) {
      COLLISION_DEBUG_POINT_X = pos_world[0];
      COLLISION_DEBUG_POINT_Y_BITS = *(int *)&pos_world[1];
      COLLISION_DEBUG_POINT_Z_BITS = *(int *)&pos_world[2];
      COLLISION_DEBUG_REPEAT_DWORD = 1;
      COLLISION_DEBUG_VECTOR_I = new_pos.i;
      COLLISION_DEBUG_VECTOR_J = new_pos.j;
      COLLISION_DEBUG_VECTOR_K = new_pos.k;
      COLLISION_DEBUG_LENGTH_BITS = 0x3f800000;
      COLLISION_DEBUG_WIDTH = physics->width;
      COLLISION_DEBUG_HEIGHT = physics->height;
    }
  }

  /* the query count (low 16 bits) drives the result-walk and a state bit */
  if ((unsigned short)result_count < 0x10) {
    *(uint8_t *)&physics->out_flags &= 0xf7;
  } else {
    *(uint8_t *)&physics->out_flags |= 8;
  }

  /* ---- nearest 'mach' surface-plane refinement (0x1a3899..0x1a3d32) ----
   * Only when the count==0 AND a valid mach-surface block (physics->existing_support_surface_index) is
   * present. Walks the surface block's edge list, projecting the new_velocity
   * (physics+0xb8) onto each candidate plane and keeping the nearest in-range
   * hit, then snaps the ground plane / +0xc4 to it. Held FPU values across the
   * named calls (bsp3d_get_plane / distance helpers) are kept in C scalars. */
  physics->field_a8 = 0xffffffff; /* physics->field_a8 = none */
  /* note: the entry test reuses the count==0 ZF from the >0x10 compare */
  if ((unsigned short)result_count == 0 &&
      physics->existing_support_surface_index != -1) {
    int surf_block; /* EDI = global_collision_bsp_get() */
    int surf_index; /* physics->existing_support_surface_index index */
    int best_edge; /* local_38 */
    float best_dist; /* local_48 */
    float nrm_d; /* local_8 */
    float proj[3]; /* local_6c/68/64 */
    float plane0[4]; /* local_8c/88/84/80 (from bsp3d_get_plane) */
    float planeN[4]; /* local_58/54/4c projected plane */
    char edge_swap; /* local_1 */
    real_plane3d bestN; /* local_ac/a8/a4/a0 region: ff58/5c/60/64 */
    int sel_edge;
    int edge0, edge1;
    void *ce; /* current edge element */
    float *pa, *pb;

    surf_block = (int)global_collision_bsp_get();
    surf_index = physics->existing_support_surface_index;
    best_edge = 0xffffffff;
    *(unsigned int *)&best_dist = 0x7f7fffff;
    if (surf_index >= 0 && surf_index < ((collision_bsp_t *)surf_block)->surfaces.count) {
      collision_surface_t *surf =
        tag_block_get_element((void *)(surf_block + 0x3c), surf_index, 0xc);
      edge0 = surf->plane;
      edge1 = surf->first_edge;
      bsp3d_get_plane_from_designator(surf_block, edge0, plane0);
      /* DEVIATION-FIX 2026-06-17: this count==0 mach-edge block operates on the
       * collision-query swept out-points, NOT physics+0xb8. The original keeps
       * THREE distinct stack vec3s; the prior lift collapsed two of them, which
       * silently dropped the edge de-penetration push-outs and corrupted the
       * surviving new_velocity (= los_dir, copied unconditionally at 0x1a4160).
       *   los_dir2 (EBP-0x18) = query out-point-1 -> new_position (+0xac):
       *                         nrm_d, proj base, in-range test, depth,
       * push-out proj     (EBP-0x6c) = projection of los_dir2 onto plane0: t2,
       * distance los_dir  (EBP-0x78) = query out-point-2 -> new_velocity
       * (+0xb8): side dot, face dot, in-place scale_add nrm_d / proj-base read
       * los_dir2 (disasm 0x9cc..0xa14). */
      nrm_d = -(plane0[0] * los_dir2.x + plane0[1] * los_dir2.y +
                plane0[2] * los_dir2.z - plane0[3]);
      proj[0] = plane0[0] * nrm_d + los_dir2.x;
      proj[1] = plane0[1] * nrm_d + los_dir2.y;
      proj[2] = plane0[2] * nrm_d + los_dir2.z;
      ce = (void *)edge1;
      do {
        collision_edge_t *edge =
          tag_block_get_element((void *)(surf_block + 0x48), (int)ce, 0x18);
        surf_index = physics->existing_support_surface_index;
        edge_swap = (char)(surf_index == edge->right_surface);
        sel_edge = COLLISION_EDGE_PAIR_SELECT(edge->left_surface, edge_swap == 0);
        if (sel_edge != -1) {
          collision_surface_t *e2 =
            tag_block_get_element((void *)(surf_block + 0x3c), sel_edge, 0xc);
          /* 0x1a39ab: TEST byte ptr [ESI+5], 2  (bytes f6 46 05 02 -- the only
           * occurrence in the XBE; ESI is the @<esi> physics pointer).  The
           * flag is bit 9 of the flags word at physics+4, sourced from
           * bipd_tag+0x2f4 & 0x40.  This previously read physics+0x14, which is
           * forward.x -- i.e. it tested the low byte of a float, admitting or
           * rejecting mach edges essentially at random. */
          if ((((uint8_t *)&physics->in_flags)[1] & 2) != 0 ||
              (e2->flags & 4) != 0) {
            float side;
            bsp3d_get_plane_from_designator(surf_block, e2->plane, plane0);
            /* side dots plane0 with los_dir (out-point-2), disasm 0xa8b */
            side = plane0[0] * los_dir.i + plane0[1] * los_dir.j +
                   plane0[2] * los_dir.k;
            nrm_d = side;
            if (!(REAL_ZERO_POOL < side) &&
                physics->width * REAL_NEG_0_5_POOL <
                  (plane0[0] * los_dir2.x + plane0[1] * los_dir2.y +
                   plane0[2] * los_dir2.z - plane0[3])) {
              void *v0 = tag_block_get_element((void *)(surf_block + 0x54),
                                               edge->start_vertex, 0x10);
              void *v1 = tag_block_get_element(
                (void *)(surf_block + 0x54), edge->end_vertex, 0x10);
              real_vector3d ev; float t2, dist;
              pa = (float *)v0;
              pb = (float *)v1;
              ev.i = pb[0] - pa[0];
              ev.j = pb[1] - pa[1];
              ev.k = pb[2] - pa[2];
              t2 = ((proj[0] - pa[0]) * ev.i + (proj[1] - pa[1]) * ev.j +
                    (proj[2] - pa[2]) * ev.k) /
                   (ev.i * ev.i + ev.j * ev.j + ev.k * ev.k);
              if (!(REAL_ZERO_POOL < t2)) {
                planeN[0] = pa[0];
                planeN[1] = pa[1];
                planeN[2] = pa[2];
              } else if (t2 <= REAL_ONE_POOL) {
                vector3d_scale_add((real_point3d *)pa, &ev, t2, (real_point3d *)planeN);
              } else {
                planeN[0] = pb[0];
                planeN[1] = pb[1];
                planeN[2] = pb[2];
              }
              dist = distance_squared3d(proj, planeN);
              if (dist < best_dist) {
                best_dist = dist;
                best_edge = sel_edge;
                bestN.normal[0] = plane0[0];
                bestN.normal[1] = plane0[1];
                bestN.normal[2] = plane0[2];
                bestN.d = plane0[3];
                nrm_d = side;
              }
            }
          }
        }
        ce = (void *)COLLISION_EDGE_PAIR_SELECT(edge->forward_edge, edge_swap != 0);
      } while (ce != (void *)edge1);

      if (best_edge != -1 &&
          (physics->width + physics->width) *
              (physics->width + physics->width) >=
            best_dist &&
          nrm_d <= REAL_0_053333_POOL) {
        /* depth dots bestN with los_dir2 (out-point-1), disasm 0xc83 */
        float depth = (bestN.normal[0] * los_dir2.x + bestN.normal[1] * los_dir2.y +
                       bestN.normal[2] * los_dir2.z) -
                      (bestN.d + physics->width);
        if ((depth < 0.0f ? -depth : depth) <=
            physics->width * REAL_0_5_POOL) {
          /* face dots bestN with los_dir (out-point-2), disasm 0xcc8 */
          float face = bestN.normal[0] * los_dir.i + bestN.normal[2] * los_dir.k +
                       bestN.normal[1] * los_dir.j;
          /* push-out de-penetrates los_dir2 (-> new_position), disasm 0xcec */
          float pushv = -best_dist;
          los_dir2.x = bestN.normal[0] * pushv + los_dir2.x;
          los_dir2.y = bestN.normal[1] * pushv + los_dir2.y;
          los_dir2.z = bestN.normal[2] * pushv + los_dir2.z;
          if (REAL_NEG_ONE_THIRTIETH_POOL < face) {
            /* In-place adjust of the swept out-point (new_velocity source):
             * los_dir += -(face+k)*bestN. Original (0x1a3c65..0x1a3c8b) aliases
             * base==out==EBP-0x78 (= los_dir); scale_add is component-wise so
             * in-place is safe. */
            vector3d_scale_add((real_point3d *)&los_dir, (real_vector3d *)&bestN, -(face + REAL_ONE_THIRTIETH_POOL),
                               (real_point3d *)&los_dir);
          }
          /* build a draw record from the plane (0x1a3c8f..0x1a3dce).
           * Original synthetic-contact layout (verified vs disasm):
           *   point[0]=0 (0xd9f movl $0); spatial XYZ in point[1..3]
           *     (0xd8c/0xdd5/0xdec); object_handle=-1 (0xdb1 eax=0xffffffff);
           *   surface_handle=best_edge (0xdb7 ecx); flags=0xffff0000
           *     (0xdc0..0xdce bytes 00,00,ff,ff); result_count=1 (0xd92
           *   movl $0x1,-0x3c) so the refinement loop below actually processes
           *   this synthetic edge contact (prior lift left result_count==0, so
           *   the loop skipped it and the ground-plane snap never ran). */
          pushv = -physics->width;
          results[0].plane.normal[0] = bestN.normal[0];
          results[0].plane.normal[1] = bestN.normal[1];
          results[0].plane.normal[2] = bestN.normal[2];
          results[0].plane.d = bestN.d;
          results[0].point[0] = 0.0f; /* +0x00 */
          results[0].point[1] = bestN.normal[0] * pushv + los_dir2.x; /* +0x04 X */
          results[0].point[2] = bestN.normal[1] * pushv + los_dir2.y; /* +0x08 Y */
          results[0].point[3] = bestN.normal[2] * pushv + los_dir2.z; /* +0x0c Z */
          results[0].object_handle = -1; /* +0x20 */
          results[0].surface_handle = best_edge; /* +0x24 */
          results[0].flags = 0;
          results[0].field_29 = 0;
          results[0].field_2a = -1;
          /* BISECT 2026-06-18: `result_count = 1;` (orig 0xd92) intentionally
           * omitted. Setting it un-gates the edge-snap
           * (physics+0xa4=best_edge), Loop B/C, and metric loop in the count==0
           * path — which correlates with an MP-load decals.c:479 cluster_index
           * crash (biped_update_physics not on the crash stack => persistent/global
           * state via an unverified best_edge). Re-enable only after best_edge
           * is re-derived from the mach-edge fallback disasm. */
          physics->field_a8 = best_edge;
          ((uint8_t *)&physics->new_position.y)[1] = 0; /* LIFT BUG (kept): 0x1a3d07 clears results[0] +0x29/+0x2a */
          ((uint8_t *)&physics->new_position.y)[2] = 0;
        }
      }
    }
  }

  /* ---- planar-normal clamp (0x1a3d34..0x1a3d69) ---- */
  r = gx * gx + gy * gy;
  if (REAL_1E_8_POOL < r) {
    planar_scale = REAL_ONE_DOUBLE_POOL / sqrtf(r);
    gy = gy * planar_scale;
    gx = planar_scale * gx;
  }

  /* ---- result-array refinement (0x1a3d69..) ---- */
  best_index = 0xffffffff;
  {
    char loop_flag9; /* local_9 */
    char loop_flag1; /* local_1 */
    float loop_best; /* local_48 */
    float loop_metric; /* ff74 */
    biped_collision_result *e;
    int n;
    loop_flag9 = 0;
    loop_flag1 = 0;
    *(unsigned int *)&loop_best = 0xff7fffff;
    *(unsigned int *)&loop_metric = 0xff7fffff;
    if ((physics->in_flags & 0x10) != 0) {
      goto LAB_001a401e;
    }
    if ((short)(unsigned short)result_count <= 0) {
      goto LAB_001a401e;
    }
    /* ---- loop A: ground-plane select over the result array ---- */
    for (n = 0; n < (short)(unsigned short)result_count; n++) {
      char want, same;
      float metric;
      e = &results[n];
      if ((char)*(uint8_t *)&physics->in_flags < 0 ||
          (material_local == 0 && (e->flags & 4) == 0)) {
        want = 0;
      } else {
        want = 1;
      }
      /* same-as-cached test: physics+0xa8 (cached surface_handle) vs the best
       * entry's surface_handle (+0x24). Confirmed original 0x1a3dd1
       * cmp [ebp+ecx-0x354],eax with buffer base ebp-0x378 and ecx=n*0x2c =>
       * entry+0x24 = surface_handle. The 2026-06-18 bisect wrongly used .flags
       * (+0x28); restored to match the binary (see +0xa4 fix below). */
      if (physics->field_a8 != -1 &&
          physics->field_a8 ==
            results[(short)best_index].surface_handle) {
        same = 1;
      } else {
        same = 0;
      }
      metric = -(physics->new_velocity.i * e->plane.normal[0] +
                 e->plane.normal[2] * physics->new_velocity.k +
                 physics->new_velocity.j * e->plane.normal[1]);
      if (want == 0) {
        /* 0x1a3f13: FCOMP normal[2] (flds 0x4(%edi)) vs loop_best, test
         * 0x41/jne
         * -> take when normal[2] > loop_best (MAXIMUM: most upward-facing
         * ground plane; Z is the up axis). loop_best inits to -FLT_MAX. The
         * compare/store use normal[2] (Z/up), NOT normal[1] (Y): selecting on Y
         * lets a vertical wall (normal ~= +/-Y, Z~=0) win as "ground", so the
         * biped is projected along/up the wall after a high-velocity contact.
         */
        if (loop_flag9 == 0 && e->plane.normal[2] > loop_best) {
          goto loopA_take;
        }
      } else if ((material_local != 0 ||
                  gx * e->plane.normal[0] + gy * e->plane.normal[1] <=
                    REAL_0_5_POOL) &&
                 (loop_flag9 == 0 || same ||
                  (loop_flag1 == 0 && loop_metric < metric))) {
      loopA_take:
        loop_flag9 = want;
        best_index = n;
        loop_flag1 = same;
        loop_best =
          e->plane.normal[2]; /* 0x1a3f26: mov 0x4(%edi) = normal[2] (Z/up) */
        loop_metric = metric;
      }
      if ((*(uint8_t *)&physics->out_flags & 0x10) == 0) {
        if ((e->flags & 8) == 0) {
          /* 0x1a3e8a-0x1a3e8d: `mov eax,[edi+0xc]; cmp eax,-1; je 0x1a3eb5` --
           * a BSP-surface contact (object_handle == NONE) leaves the
           * cannot-come-to-rest bit CLEAR and continues the loop.  Only an
           * object contact whose type is NOT 6 (scenery) sets the bit.
           * Folding this into the outer condition (as the prior lift did) set
           * the bit on every BSP contact, so object+0x4 bit 0x20 (at-rest)
           * was never set for a biped lying on world geometry, and
           * biped_start_limp_body_physics always early-outed -- dead bipeds
           * never entered limp/ragdoll and lingered in MP. */
          if (e->object_handle == -1) {
            goto loopA_nomark;
          }
          /* DEVIATION: hardened datum_get -> object_try_and_get_and_verify_type
           * to survive stale handles; original calls raw datum_get @0x1a3e97
           * then tests (1 << (od[3] & 0x1f)) & 0x40 (type-6 mask). The
           * verifying accessor (object_try @0x13d640) performs the identical
           * (mask & (1 << (type & 0x1f))) test, but adds salt/generation
           * validation so a stale handle returns NULL instead of dereferencing
           * a freed datum slot. */
          if (e->object_handle == -1) {
            goto loopA_nomark;
          }
          if (((1 << ((char *)datum_get(OBJECT_HEADER_DATA,
                                        e->object_handle))[3]) &
               0x40) != 0) {
            goto loopA_nomark;
          }
        }
        *(uint8_t *)&physics->out_flags |= 0x10;
      loopA_nomark:;
      }
    }
    if ((short)best_index == -1) {
      goto LAB_001a401e;
    }
    e = &results[(short)best_index];
    /* selected entry: compute the result normal dot (local_3c) */
    /* 0x1a3ef6-0x1a3f24: (n1*p1 + n2*p2) + n0*p0 */
    best_t = -(e->plane.normal[1] * new_pos.j + e->plane.normal[2] * new_pos.k +
               e->plane.normal[0] * new_pos.i);
    if (loop_flag9 == 0 && loop_flag1 == 0) {
      /* 0x1a3ff8: flds -0x48(%ebp) = loop_best (the selected entry's normal[2],
       * Z/up), compared against physics->minimum_normal_k (stand-on-slope threshold). */
      if (loop_best < physics->minimum_normal_k) {
        goto LAB_001a401e;
      }
      if ((physics->in_flags & 1) != 0 &&
          physics->ground_tangential_velocity_max < REAL_MAX_POOL) {
        float slen;
        vector3d_scale_add((real_point3d *)&new_pos, (real_vector3d *)&e->plane.normal[0], best_t, &surf);
        slen = magnitude_squared3d((real_vector3d *)&surf);
        if (physics->ground_tangential_velocity_max *
              physics->ground_tangential_velocity_max <
            slen) {
          float md = magnitude3d(&new_pos);
          if (best_t / md < physics->ground_tangential_angle) {
            goto LAB_001a401e;
          }
        }
        (void)slen;
      }
    }
    /* snap ground plane (+0x80 = entry.normal[3]) + surface handle (+0xa4) to
     * the selected entry, then +0xc4 from the normal dot. EDI = &entry.normal
     */
    {
      /* +0xa4 surface index = e->surface_handle (entry+0x24), NOT .flags
       * (+0x28). Confirmed original 0x1a3fb9 mov ebx,[ebp+ebx-0x354] with
       * buffer base ebp-0x378 (edi=entry+0x10) and ebx=best_index*0x2c =>
       * entry+0x24, stored to physics+0xa4 at 0x1a3fe6 -> biped+0x430. The
       * 2026-06-18 bisect wrongly reverted to .flags, writing a flags bitfield
       * (e.g. 0x30000) into the pathfinding last_good_surface field, so
       * biped_find_pathfinding_surface_index passed it to
       * collision_surface_find_closest_point2d and asserted (tag_groups.c:3089)
       * on AI-spawned bipeds. Restored to match the binary. */
      int sel_surface = e->surface_handle;
      *(uint8_t *)&physics->out_flags &= 0xfe;
      *(int *)&physics->ground_plane.normal[0] = *(int *)&e->plane.normal[0];
      *(int *)&physics->ground_plane.normal[1] = *(int *)&e->plane.normal[1];
      *(int *)&physics->ground_plane.normal[2] = *(int *)&e->plane.normal[2];
      *(int *)&physics->ground_plane.d = *(int *)&e->plane.d;
      physics->support_surface_index = sel_surface;
      if (sel_surface != -1 &&
          sel_surface == physics->field_a8) {
        *(int *)&physics->landing_velocity = 0;
        goto LAB_001a4062;
      }
      /* 0x1a3ff6-0x1a4012: (p1*n1 + p2*n2) + p0*n0 */
      physics->landing_velocity =
        -(new_pos.j * physics->ground_plane.normal[1] +
          new_pos.k * physics->ground_plane.normal[2] +
          new_pos.i * physics->ground_plane.normal[0]);
      goto LAB_001a4062;
    }
  }
  goto LAB_001a4062_done;

LAB_001a401e:
  *(uint8_t *)&physics->out_flags |= 1;
  *(int *)&physics->ground_plane.normal[0] = DEPTHS_OF_HELL_PLANE_NORMAL_I_BITS;
  *(int *)&physics->ground_plane.normal[1] = DEPTHS_OF_HELL_PLANE_NORMAL_J_BITS;
  *(int *)&physics->ground_plane.normal[2] = DEPTHS_OF_HELL_PLANE_NORMAL_K_BITS;
  *(int *)&physics->ground_plane.d = DEPTHS_OF_HELL_PLANE_D_BITS;
  physics->support_surface_index = 0xffffffff;
  physics->landing_velocity = 0.0f;

LAB_001a4062:
LAB_001a4062_done:
  /* ---- loop B: nearest object (0x1a4062..0x1a40ec). Pointer-walk over the
   * object_handle field at stride 0x2c (matches the original's add edi,0x2c).
   */
  {
    int near_obj = 0xffffffff;
    int near_type = 0;
    float near_dist;
    int n;
    int *eh;
    *(unsigned int *)&near_dist = 0;
    if ((short)(unsigned short)result_count > 0) {
      eh = &results[0].object_handle;
      n = (unsigned short)result_count;
      do {
        if (*eh != -1) {
          void *o;
          float ddx, ddy, ddz, d2;
          /* Faithful: original CALL 0x13d680 object_get_and_verify_type
           * (asserting, mask -1). Result used directly, NO NULL check
           * (orig 0x1a408a: mov ecx,eax; flds [ecx+0x18]). The inner datum_get
           * asserts/HALTs on a stale handle — which only occurs if collision_move_point
           * returns a count larger than the filled records. Loop C below stays
           * object_try_and_get_and_verify_type (0x13d640) — that IS faithful.
           */
          o = object_get_and_verify_type(*eh, -1);
          ddx = ((object_data_t *)o)->translational_velocity.i - los_dir.i;
          ddy = ((object_data_t *)o)->translational_velocity.j - los_dir.j;
          ddz = ((object_data_t *)o)->translational_velocity.k - los_dir.k;
          d2 = ddx * ddx + ddy * ddy + ddz * ddz;
          if (near_obj == -1) {
            goto loopB_take;
          }
          if ((short)near_type == 1) {
            if (((object_data_t *)o)->type != 1)
              goto loopB_next;
          } else if (((object_data_t *)o)->type == 1) {
            goto loopB_take;
          }
          if (d2 < near_dist) {
            goto loopB_take;
          }
          goto loopB_next;
        loopB_take:
          near_dist = d2;
          near_obj = *eh;
          near_type = (near_type & 0xffff0000) |
                      (unsigned short)((object_data_t *)o)->type;
        }
      loopB_next:
        eh = (int *)((char *)eh + 0x2c);
        n--;
      } while (n != 0);
    }
    physics->bumped_object_index = near_obj;
  }

  /* ---- loop C: 'mach' tag scan (0x1a40f2..0x1a4160). Pointer-walk. ---- */
  physics->elevator_object_index = 0xffffffff;
  if ((short)(unsigned short)result_count > 0) {
    int n = (unsigned short)result_count;
    int *eh = &results[0].object_handle;
    do {
      int handle = *eh;
      if (handle != -1) {
        void *t2obj = object_try_and_get_and_verify_type(handle, 0x80);
        if (t2obj != (void *)0) {
          void *mtag = tag_get(0x6d616368, *(int *)t2obj);
          if ((machine_definition_field_292(mtag) & 4) != 0 &&
              machine_definition_field_2ea(mtag) != -1) {
            physics->elevator_object_index = handle;
          }
        }
      }
      eh = (int *)((char *)eh + 0x2c);
      n--;
    } while (n != 0);
  }

  /* ---- writeback (0x1a4160..0x1a41de): new_position (+0xac) = los_dir2 (the
   * near LOS out-point, raw 32-bit copy); new_velocity (+0xb8) = los_dir (the
   * far out-point); step distance (+0xc8) = |los_dir - new_pos|; then the z
   * step-down.
   * DEVIATION-FIX 2026-06-17: the prior lift wrote new_position = los_dir -
   * los_dir2 and derived the step from that, corrupting the biped position
   * every frame (bipeds float to the ceiling; the camera/PVS derived from the
   * bad position culls all BSP geometry -> split-screen skybox flicker). The
   * original (objdump of delinked bipeds_FUN_001a2f40.obj, 0x1a4160) integer-
   * copies los_dir2 into new_position and takes the step magnitude from
   * (los_dir - new_pos). ---- */
  {
    float d0 = los_dir.i - new_pos.i;
    float d1 = los_dir.j - new_pos.j;
    float d2 = los_dir.k - new_pos.k;
    REAL_BITS(physics->new_position.x) = REAL_BITS(los_dir2.x); /* +0xac new_position */
    REAL_BITS(physics->new_position.y) = REAL_BITS(los_dir2.y);
    REAL_BITS(physics->new_position.z) = REAL_BITS(los_dir2.z);
    REAL_BITS(physics->new_velocity.i) = REAL_BITS(los_dir.i); /* +0xb8 new_velocity */
    REAL_BITS(physics->new_velocity.j) = REAL_BITS(los_dir.j);
    REAL_BITS(physics->new_velocity.k) = REAL_BITS(los_dir.k);
#ifdef HALO_RNG_TRACE
    RNG_TRACE_EX(RNG_TRACE_KIND_BIPED_WRITEBACK_Y,
                 RNG_TRACE_BITS(physics->new_position.y),
                 *(unsigned int *)&physics->biped_index);
#endif
    /* 0x1a4194-0x1a41bf: (d2*d2 + d1*d1) + d0*d0 */
    physics->field_c8 = sqrtf(d2 * d2 + d1 * d1 + d0 * d0); /* +0xc8 step */
  }
  physics->new_velocity.k = physics->new_velocity.k - physics->crouch_velocity;

  /* ---- biped step-down (0x1a41de..0x1a42da) ---- */
  flags = physics->in_flags;
  if ((flags & 4) != 0 && (flags & 8) != 0) {
    obj = object_get_and_verify_type(physics->biped_index, 1);
    if (((unit_data_t *)obj)->player_index.value != -1) {
      tag = tag_get(0x62697064, *(int *)obj);
      if ((*(unsigned char *)&biped_definition_flags(tag) & 0x18) == 0) {
        float half = biped_definition_collision_height_standing(tag) * REAL_0_5_POOL;
        REAL_BITS(surf.x) = REAL_BITS(physics->new_position.x); /* movl copies (0x1a4239) */
        REAL_BITS(surf.y) = REAL_BITS(physics->new_position.y);
        surf.z = physics->new_position.z + half;
        if (collision_get_features_in_sphere(
              (int)((-(unsigned int)((flags & 0x80) != 0) & 0xffdffd00) +
                    0x20c3a0),
              &surf, half, 0.0f, biped_definition_collision_radius(tag),
              physics->biped_index, debug_scratch)) {
          float fr =
            biped_definition_collision_height_standing(tag) -
            (biped_definition_collision_radius(tag) + biped_definition_collision_radius(tag));
          disp.i = fr * global_up_vector_ptr[0];
          disp.j = fr * global_up_vector_ptr[1];
          disp.k = fr * global_up_vector_ptr[2];
          if (collision_features_test_vector((int)debug_scratch,
                                             &physics->new_position, &disp,
                                             surface_result)) {
            *(uint8_t *)&physics->out_flags |= 4;
          }
        }
      }
    }
  }

  /* ---- final NaN asserts (0x1a42dc..0x1a4409) ----
   * The original reads each component through a single pointer to the vector
   * base (EDI=&physics->new_position at +0xac, EAX=&physics->new_velocity at
   * +0xb8) and copies it to a scratch slot ([EBP-0x40]) before the
   * exponent-mask test. Model with pointer-indexed reads so MSVC reproduces
   * the [EDI]/[EDI+4]/[EDI+8] addressing and the per-component temp store. */
  {
    unsigned int *np = (unsigned int *)&physics->new_position;
    isnan_tmp = np[0];
    if ((isnan_tmp & 0x7f800000) == 0x7f800000 ||
        ((isnan_tmp = np[1]), (isnan_tmp & 0x7f800000) == 0x7f800000) ||
        ((isnan_tmp = np[2]), (isnan_tmp & 0x7f800000) == 0x7f800000)) {
      display_assert(
        csprintf(
          error_string_buffer, "%s: assert_valid_real_point3d(%f, %f, %f)",
          "&physics->new_position", (double)physics->new_position.x,
          (double)physics->new_position.y, (double)physics->new_position.z),
        "c:\\halo\\SOURCE\\units\\bipeds.c", 0xee2, true);
      system_exit(-1);
    }
  }
  {
    unsigned int *nv = (unsigned int *)&physics->new_velocity;
    isnan_tmp = nv[0];
    if ((isnan_tmp & 0x7f800000) == 0x7f800000 ||
        ((isnan_tmp = nv[1]), (isnan_tmp & 0x7f800000) == 0x7f800000) ||
        ((isnan_tmp = nv[2]), (isnan_tmp & 0x7f800000) == 0x7f800000)) {
      display_assert(
        csprintf(
          error_string_buffer, "%s: assert_valid_real_vector2d(%f, %f, %f)",
          "&physics->new_velocity", (double)physics->new_velocity.i,
          (double)physics->new_velocity.j, (double)physics->new_velocity.k),
        "c:\\halo\\SOURCE\\units\\bipeds.c", 0xee3, true);
      system_exit(-1);
    }
  }
  (void)flags_word;
  (void)tval;
  (void)fdist;
}

/* biped_snap_facing (0x1a4440)
 *
 * Re-orthogonalise a biped's forward (+0x24) / up (+0x30) vectors for its
 * current movement mode. Name and branch structure from PAL 2342
 * source/units/bipeds.c biped_snap_facing (T2), corroborated by this build's
 * own strings "post-bank-snapfacing", "post-climb-snapfacing",
 * "pre-/post-deadplane-snapfacing", "post-normal-snapfacing" (passed to
 * biped_verify_object_vectors) and the assert "&biped->object.forward" at
 * bipeds.c:0xfa5 (4005).
 *
 * Confirmed (disassembly, SUB ESP,0x38, EBX = biped_index throughout):
 *   - definition = tag_get('bipd', biped->definition_index); flags at
 *     definition+0x2f4: bit 0x4 = flying (bank), bit 0x40 = climbs anything.
 *   - biped+0xb6 bit 0x4 = object dead; biped+0x424 bit 0x1 = airborne.
 *   - bank: forward is assert_valid_real_normal3d'd; left = forward x up3d and
 *     up = left x forward are computed INLINE (no CALL); degenerate up falls
 *     back to global forward/left (dword copies); bank angle at biped+0x468.
 *   - climb: support surface index at biped+0x430 (NONE -> normal = up);
 *     ground plane normal at biped+0x46c; snap angle is the double at
 *     0x281138 (0.17453292012214661 = (double)(float)(10 deg)), FCOS pushed
 *     before FSIN; forward = normal x (forward x normal) is inlined, the
 *     fallback pair calls cross_product3d (0x178d0).
 *   - deadplane: |cos - 1.0f| compared in DOUBLE against the double at
 *     0x2533d0 (9.9999997e-05 = (double)0.0001f); acos is CRT 0x1d94f0.
 *   - x87 dot-product association is (j + k) + i / (k + j) + i per site as
 *     read from the FLD/FADDP order, transcribed below.
 *
 * Call-site verification (first PUSH is the last argument):
 *   0x1a444f object_get_and_verify_type | 1 ; EBX | match
 *   0x1a445e tag_get | [ESI] ; 'bipd' | match
 *   0x1a4486 valid_real_normal3d | EDI=biped+0x24 | match
 *   0x1a44c3 csprintf | f2,f1,f0 (doubles) ; 0x2b5148 ; 0x254a24 ; 0x5ab100
 *   0x1a44cc display_assert | 1 ; 0xfa5 ; 0x2b4d5c ; EAX | match
 *   0x1a4544 normalize3d | &up(-0x20) | match
 *   0x1a45b7 normalize3d | &left(-0x14) | match
 *   0x1a45e9 biped_verify_object_vectors | EAX=EBX ; 0x2b5130 | match
 *   0x1a4649 cross_product3d | &axis(-0x2c) ; &normal(-0x14) ; EDI=up | match
 *   0x1a46d6 rotate_vector3d_by_sincos | cos ; sin ; &axis ; &tilted(-0x38)
 *   0x1a46e7 cross_product3d | &vector(-0x20) ; &normal ; &tilted | match
 *   0x1a47aa cross_product3d | &vector ; EDI=up ; &normal | match
 *   0x1a47bb cross_product3d | &forward(-0x38) ; &vector ; &normal | match
 *   0x1a4823 biped_verify_object_vectors | EAX=[EBP+8] ; 0x2b5118 | match
 *   0x1a484e biped_verify_object_vectors | EAX=EBX ; 0x2b50fc | match
 *   0x1a48b0 cross_product3d | &axis(-0x38) ; EBX=gp ; EDI=up | match
 *   0x1a48eb/0x1a48fd rotate_vector3d_by_sincos | cos ; sin ; &axis ;
 *     up / forward | match
 *   0x1a491d biped_verify_object_vectors | EAX=[EBP+8] ; 0x2b50e0 | match
 *   0x1a4981 biped_verify_object_vectors | EAX=EBX ; 0x2b50c8 | match
 */
void biped_snap_facing(int biped_index)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  real_vector3d *forward;
  real_vector3d *up;

  biped = object_get_and_verify_type(biped_index, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  if ((definition->biped_flags & 0x4) != 0 &&
      (biped->unit.object.damage_flags & 0x4) == 0) {
    real_vector3d left;
    real_vector3d up_vector;
    float sine;
    float cosine;

    forward = &biped->unit.object.forward;
    if (!valid_real_normal3d(forward)) {
      display_assert(csprintf((char *)0x5ab100,
                              "%s: assert_valid_real_normal3d(%f, %f, %f)",
                              "&biped->object.forward", (double)forward->i,
                              (double)forward->j, (double)forward->k),
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0xfa5, 1);
      system_exit(-1);
    }

    left.i = global_up_vector_ptr[2] * forward->j -
             global_up_vector_ptr[1] * forward->k;
    left.j = global_up_vector_ptr[0] * forward->k -
             forward->i * global_up_vector_ptr[2];
    left.k = forward->i * global_up_vector_ptr[1] -
             global_up_vector_ptr[0] * forward->j;
    up_vector.i = left.j * forward->k - left.k * forward->j;
    up_vector.j = left.k * forward->i - left.i * forward->k;
    up_vector.k = left.i * forward->j - left.j * forward->i;
    if (normalize3d(&up_vector) == 0.0f) {
      up_vector = *(real_vector3d *)global_forward_vector_ptr;
      left = *(real_vector3d *)global_left_vector_ptr;
    }

    cosine = x87_fcos(biped->bank);
    sine = x87_fsin(biped->bank);
    up_vector.i = up_vector.i * cosine;
    up_vector.j = up_vector.j * cosine;
    up_vector.k = up_vector.k * cosine;
    normalize3d(&left);
    biped->unit.object.up.i = left.i * sine + up_vector.i;
    biped->unit.object.up.j = left.j * sine + up_vector.j;
    biped->unit.object.up.k = left.k * sine + up_vector.k;

    biped_verify_object_vectors(biped_index, "post-bank-snapfacing");
  } else if ((definition->biped_flags & 0x40) != 0 &&
             (biped->unit.object.damage_flags & 0x4) == 0) {
    real_vector3d normal;
    real_vector3d vector;
    real_vector3d axis;
    real_vector3d new_forward;

    up = &biped->unit.object.up;
    if (biped->support_surface_index == NONE) {
      normal = *up;
    } else {
      bool tilt_toward_surface = true;

      normal = *(real_vector3d *)biped->ground_plane.normal;
      cross_product3d(up, &normal, &axis);
      if (normalize3d(&axis) == 0.0f) {
        if (normal.j * up->j + normal.k * up->k + normal.i * up->i > 0.0f)
          tilt_toward_surface = false;
        else
          axis = biped->unit.object.forward;
      }

      if (tilt_toward_surface) {
        real_vector3d tilted_up = *up;

#if defined(_MSC_VER) && !defined(__clang__)
        rotate_vector3d_by_sincos(&tilted_up, &axis,
                                  (float)sin(0.17453292012214661),
                                  (float)cos(0.17453292012214661));
#else
        rotate_vector3d_by_sincos(&tilted_up, &axis,
                                  (float)x87_fsin_d(0.17453292012214661),
                                  (float)x87_fcos_d(0.17453292012214661));
#endif
        cross_product3d(&tilted_up, &normal,
                        &vector);
        if (vector.k * axis.k + vector.j * axis.j + vector.i * axis.i > 0.0f)
          normal = tilted_up;
      }
    }

    forward = &biped->unit.object.forward;
    vector.i = normal.k * forward->j - normal.j * forward->k;
    vector.j = normal.i * forward->k - normal.k * forward->i;
    vector.k = normal.j * forward->i - normal.i * forward->j;
    new_forward.i = vector.k * normal.j - vector.j * normal.k;
    new_forward.j = normal.k * vector.i - vector.k * normal.i;
    new_forward.k = vector.j * normal.i - normal.j * vector.i;
    if (normalize3d(&new_forward) == 0.0f) {
      cross_product3d(&normal, up, &vector);
      cross_product3d(&normal, &vector,
                      &new_forward);
      if (normalize3d(&new_forward) == 0.0f) {
        normal = *(real_vector3d *)global_up_vector_ptr;
        new_forward = *(real_vector3d *)global_forward_vector_ptr;
      }
    }

    *forward = new_forward;
    *up = normal;

    biped_verify_object_vectors(biped_index, "post-climb-snapfacing");
  } else if ((biped->unit.object.damage_flags & 0x4) != 0 &&
             ((uint8_t)biped->flags & 0x1) == 0) {
    real_vector3d *ground_normal;
    float cosine_of_angle;

    biped_verify_object_vectors(biped_index, "pre-deadplane-snapfacing");

    ground_normal = (real_vector3d *)biped->ground_plane.normal;
    up = &biped->unit.object.up;
    cosine_of_angle = ground_normal->k * up->k + ground_normal->j * up->j +
                      up->i * ground_normal->i;
    if (!((double)x87_fabs(cosine_of_angle - 1.0f) < (double)0.0001f)) {
      float angle;

#if defined(_MSC_VER) && !defined(__clang__)
      {
        double acos(double x);
        angle = (float)acos((double)cosine_of_angle);
      }
#else
      angle = acosf(cosine_of_angle);
#endif
      if (angle != 0.0f) {
        real_vector3d axis;

        cross_product3d(up, ground_normal, &axis);
        if (normalize3d(&axis) != 0.0f) {
          float cosine = x87_fcos(angle);
          float sine = x87_fsin(angle);

          forward = &biped->unit.object.forward;
          rotate_vector3d_by_sincos(up, &axis, sine, cosine);
          rotate_vector3d_by_sincos(forward, &axis, sine, cosine);
          normalize3d(up);
          normalize3d(forward);
        }
      }
    }

    biped_verify_object_vectors(biped_index, "post-deadplane-snapfacing");
  } else {
    forward = &biped->unit.object.forward;
    forward->k = 0.0f;
    if (normalize3d(forward) == 0.0f)
      *forward = *(real_vector3d *)global_forward_vector_ptr;
    biped->unit.object.up = *(real_vector3d *)global_up_vector_ptr;

    biped_verify_object_vectors(biped_index, "post-normal-snapfacing");
  }
}

/* biped_new (0x1a4990)
 *
 * Biped unit callback referenced from a function-pointer table at DATA
 * 0x323d6c. Loads the biped tag, restores four default float constants
 * (obj+0x46c..0x478) from the same globals used by biped_reset
 * (0x32513c..0x325148), sets a state byte and several BSP/contact datum
 * handles to -1, re-samples world position into obj+0x438..0x444,
 * conditionally invokes biped_find_nearby_support_surface (bit 0x40 of the tag flags at +0x2f4
 * -- the same bit referenced at 0x1a39ab, gating a physics "mach edge"
 * flag), then unconditionally calls biped_snap_facing and clears two more
 * state fields. Always returns 1.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) type 1 (biped).
 * Confirmed: CALL 0x1ba140 (tag_get) with 'bipd' signature (0x62697064).
 * Confirmed: obj+0x46c..0x478 loaded from globals 0x32513c, 0x325140,
 *   0x325144, 0x325148 (same constants as biped_reset).
 * Confirmed: obj+0x45c (byte) = 0x7f.
 * Confirmed: obj+0x430, +0x434 = -1 before the world-position call;
 *   obj+0x448, +0x444, +0x44c = -1 after it (single OR EBX,-1 reused for
 *   all five -1 stores).
 * Confirmed: CALL 0x1412f0 (object_get_world_position) writing to
 *   obj+0x438 (vector3_t).
 * Confirmed: bit 0x40 of tag+0x2f4 gates CALL 0x1a25e0(unit_handle@ecx).
 * Confirmed: CALL 0x1a4440 biped_snap_facing(unit_handle) unconditional, cdecl
 * cleanup ADD ESP,0x4. Confirmed: obj+0x42c = -1, obj+0x42b (byte) = 0 after
 * the two calls. Confirmed: MOV AL,0x1 immediately before epilogue --
 * unconditional return value of 1. Uncertain: exact semantics of the
 * +0x42b/+0x42c/+0x444/+0x448/+0x44c fields and of the return value (bool
 * success vs. dispatch-table convention); no strong naming evidence yet, kept
 * as biped_new.
 */
char biped_new(int unit_handle)
{
  biped_data_t *biped;
  biped_definition_t *definition;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);

  *(int *)&biped->ground_plane.normal[0] = DEPTHS_OF_HELL_PLANE_NORMAL_I_BITS;
  *(int *)&biped->ground_plane.normal[1] = DEPTHS_OF_HELL_PLANE_NORMAL_J_BITS;
  *(int *)&biped->ground_plane.normal[2] = DEPTHS_OF_HELL_PLANE_NORMAL_K_BITS;
  *(int *)&biped->ground_plane.d = DEPTHS_OF_HELL_PLANE_D_BITS;

  biped->jump_recovery_timer = 0x7f;
  biped->support_surface_index = NONE;
  biped->pathfinding_surface_index = NONE;

  object_get_world_position(unit_handle, &biped->pathfinding_point);

  biped->last_pathfinding_surface_index = NONE;
  biped->last_pathfinding_attempt_time = NONE;
  biped->impact_target_object_index = NONE;

  if (((uint8_t)definition->biped_flags & 0x40) != 0) {
    biped_find_nearby_support_surface(unit_handle);
  }

  biped_snap_facing(unit_handle);

  biped->elevator_object_index = NONE;
  biped->elevator_ticks = 0;

  return 1;
}

/* biped_preprocess_node_orientations (0x1a4a50) — "preprocess-nodes" timing
 * marker
 *
 * One-instruction-body wrapper: loads its cdecl stack argument into EAX and
 * tail-calls biped_verify_object_vectors(unit_handle@eax, "preprocess-nodes").
 * Same family as the "post-airborne"/"post-landing"/"post-slipping" markers
 * above, but unlike those (which take unit_handle in a register from a
 * frameless caller), this one has a normal EBP frame and reads unit_handle from
 * [EBP+8] -- confirmed by disassembly (PUSH EBP; MOV EBP,ESP; MOV
 * EAX,[EBP+8]; PUSH "preprocess-nodes"; CALL biped_verify_object_vectors). No
 * callers are present in the current evidence bundle (xrefs_to empty);
 * kb.json's prior void(void) decl did not match the observed stack read,
 * corrected here to a single int parameter to match the evidence.
 *
 * Confirmed: MOV EAX,[EBP+8]; PUSH 0x2b5160 ("preprocess-nodes"); CALL
 * biped_verify_object_vectors(unit_handle@eax, failure_kind).
 */
void biped_preprocess_node_orientations(int unit_handle)
{
  biped_verify_object_vectors(unit_handle, "preprocess-nodes");
}

/* biped_accelerate (0x1a4a70..0x1a4c47) — biped_accelerate: apply a velocity
 * impulse to a biped (grenade/explosion launch, point-physics push).
 *
 * cdecl (biped_index, real_vector3d *acceleration); callers: damage.c impulse
 * path and point_physics.c. Early-outs when the definition's unit flags have
 * the special bit set. Brackets the update with the "pre-accel" /
 * "post-accel" biped_verify_object_vectors markers; post-accel runs only
 * while the biped has no parent object and the normalized acceleration is
 * non-zero.
 *
 * Confirmed (disassembly):
 *   - acceleration is halved in place only while the object is not dead; the
 *     damage flags are re-read before deciding on
 *     biped_stop_limp_body_physics (0x1a09f0, cdecl).
 *   - velocity += acceleration; object at-rest bit cleared; biped airborne
 *     and slipping bits set.
 *   - dead / flying / climbs-anything: angular velocity += normalize(up x
 *     acceleration) * random * |acceleration| * (pi/2). |acceleration| sums
 *     (k*k + j*j) + i*i; normalize3d's return value is discarded (FSTP ST0).
 *   - post-accel: forward = normalized acceleration, then biped_snap_facing.
 */
enum {
  _object_at_rest_bit = 5,
  _object_dead_bit = 2,
  _unit_is_special_bit = 20,
  _biped_flying_bit = 2,
  _biped_climbs_anything_bit = 6
};

#ifndef TEST_FLAG
#define TEST_FLAG(flags, bit) (((flags) & (unsigned)FLAG(bit)) != 0)
#endif
#ifndef SET_FLAG
#define SET_FLAG(flags, bit, value)             \
  ((value) ? ((flags) |= (unsigned)FLAG(bit)) : \
             ((flags) &= (unsigned)~FLAG(bit)))
#endif

void biped_accelerate(int biped_index, float *acceleration_vector)
{
  real_vector3d *acceleration = (real_vector3d *)acceleration_vector;
  biped_datum_t *biped;
  biped_definition_t *definition;

  biped = (biped_datum_t *)object_get_and_verify_type(biped_index, 1);
  definition =
    (biped_definition_t *)tag_get(0x62697064, biped->object.definition_index);
  if (TEST_FLAG(definition->unit.flags, _unit_is_special_bit)) {
    return;
  }

  biped_verify_object_vectors(biped_index, "pre-accel");

  if (!TEST_FLAG(biped->object.damage_flags, _object_dead_bit)) {
    acceleration->i = acceleration->i * 0.5f;
    acceleration->j = acceleration->j * 0.5f;
    acceleration->k = acceleration->k * 0.5f;
  }
  if (TEST_FLAG(biped->object.damage_flags, _object_dead_bit)) {
    biped_stop_limp_body_physics(biped_index);
  }

  {
    real_vector3d const *velocity = &biped->object.translational_velocity;
    real_vector3d *result = &biped->object.translational_velocity;

    result->i = velocity->i + acceleration->i;
    result->j = velocity->j + acceleration->j;
    result->k = velocity->k + acceleration->k;
  }
  SET_FLAG(biped->object.flags, _object_at_rest_bit, false);
  SET_FLAG(biped->flags, _biped_airborne_bit, true);
  SET_FLAG(biped->flags, _biped_slipping_bit, true);

  if (TEST_FLAG(biped->object.damage_flags, _object_dead_bit) ||
      TEST_FLAG(definition->biped_flags, _biped_flying_bit) ||
      TEST_FLAG(definition->biped_flags, _biped_climbs_anything_bit)) {
    real_vector3d axis;
    real magnitude;
    real scale;

    axis.i = acceleration->k * global_up_vector_ptr[1] -
             acceleration->j * global_up_vector_ptr[2];
    axis.j = global_up_vector_ptr[2] * acceleration->i -
             acceleration->k * global_up_vector_ptr[0];
    axis.k = acceleration->j * global_up_vector_ptr[0] -
             global_up_vector_ptr[1] * acceleration->i;
    normalize3d(&axis);
    magnitude = x87_sqrt(acceleration->k * acceleration->k +
                         acceleration->j * acceleration->j +
                         acceleration->i * acceleration->i);
    scale = random_math_real((unsigned int *)get_global_random_seed_address());
    scale = scale * magnitude * REAL_HALF_PI_POOL;
    biped->object.angular_velocity.i =
      axis.i * scale + biped->object.angular_velocity.i;
    biped->object.angular_velocity.j =
      axis.j * scale + biped->object.angular_velocity.j;
    biped->object.angular_velocity.k =
      axis.k * scale + biped->object.angular_velocity.k;
  }

  if (biped->object.parent_object_index == NONE) {
    real_vector3d forward;

    forward = *acceleration;
    if (normalize3d(&forward) > 0.0f) {
      biped->object.forward = forward;
      biped_snap_facing(biped_index);
      biped_verify_object_vectors(biped_index, "post-accel");
    }
  }
}

/* biped_update_turning (0x1a4c50..0x1a5300) — biped turning update
 *
 * Only caller: biped_update (0x1a65b3), bracketed by the "pre-turning" /
 * "post-turning" markers and skipped when biped+0xb6 bit 2 (dead) is set.
 * cdecl, two pushes (unit_handle, &state[0]); state is the caller's 2-byte
 * animation pair, only state[0] is written here. Internal markers
 * "post-fly-turn", "post-moving-turn" and "post-standing-turn".
 *
 * Confirmed (disassembly, not decompiler):
 *   - flying arm when tag+0x2f4 bit 2 && !(biped+0xb6 bit 2). Nearly
 *     stationary = |+0x18|^2 < 1/3600 && |+0x3c|^2 < 1.3538552e-6 (both
 *     inlined) && magnitude_squared3d(+0x228) < 0.010000001 && dot(+0x1d4,+0x24)
 *     (dot_product3d) > threshold (0.99 if +0x1b8 bit 0x20, else tag+0x4c8).
 *   - pitch = tag+0x330 * +0x230 stays on the x87 stack; turn axis
 *     forward x up is never stored; turn rate uses (z+y)+x association,
 *     * 10/3 (0x254e6c) * +0x228 - +0x22c, clamped to 1.5.
 *   - velocity/acceleration limits MULTIPLY by 1/30 (0x2546a4) and 1/900
 *     (0x25620c), not divide; bank update divides by time * 30 (0x253394).
 *   - non-flying arm: base seat byte +0x257 == 0 returns with no marker;
 *     == 5 is the "flaming" flag. Climbing (tag flag 0x40) builds the axis
 *     from two inlined crosses; the 2D path normalizes the 3D local in place
 *     with normalize2d (0x12f10) after zeroing z, fallback copies all of
 *     forward.
 *   - the "look don't turn" (+0x1b8 bit 0x100) exit of the moving arm and the
 *     non-idle exit of the standing arm return WITHOUT a marker (the marker
 *     calls at 0x1a52e9/0x1a5193 are only reached from inside the bodies).
 *   - the overshoot snap re-reads tag+0x2f4 bit 0x40 from the tag
 *     (MOV CL,[EAX+0x2f4] at 0x1a5271) instead of the cached flags.
 *   - state[0] = turn_right ? 3 : 2 (SETNE; ADD DL,2 at 0x1a518b).
 */
void biped_update_turning(int unit_handle, unsigned char *state)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  uint32_t flags;
  real_vector3d *forward;
  real_vector3d *up;
  real_vector3d *desired;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  flags = definition->biped_flags;
  forward = &biped->unit.object.forward;
  up = &biped->unit.object.up;
  desired = (real_vector3d *)&biped->unit.desired_facing_vector;

  if ((flags & 0x4) != 0 && (biped->unit.object.damage_flags & 0x4) == 0) {
    char nearly_stationary = 0;
    real_vector3d *velocity = &biped->unit.object.translational_velocity;
    real_vector3d *angular_velocity = &biped->unit.object.angular_velocity;
    real_vector3d target_facing;
    real_rectangle2d aiming_bounds;
    float turn_rate;
    float target_bank;
    float bank_blend;
    float bank_time;
    float angular_velocity_limit;
    float angular_acceleration_limit;

    if (velocity->i * velocity->i + velocity->j * velocity->j +
            velocity->k * velocity->k <
          0.00027777778f &&
        angular_velocity->i * angular_velocity->i +
            angular_velocity->j * angular_velocity->j +
            angular_velocity->k * angular_velocity->k <
          0.0000013538552f &&
        magnitude_squared3d((real_vector3d *)&biped->unit.throttle) < 0.010000001f) {
      float stationary_turning_threshold;

      if (((uint8_t)biped->unit.control_flags & 0x20) != 0)
        stationary_turning_threshold = 0.99000001f;
      else
        stationary_turning_threshold = biped_definition_runtime_cosine_stationary_turning_threshold(definition);

      if (dot_product3d(desired, forward) > stationary_turning_threshold)
        nearly_stationary = 1;
    }

    if (nearly_stationary) {
      target_facing = *forward;
    } else {
      float pitch =
        biped_definition_flying_pitch_amount(definition) * biped->unit.throttle.z;

      target_facing = *desired;
      if (pitch != 0.0f) {
        target_facing.k = target_facing.k + pitch;
        if (normalize3d(&target_facing) == 0.0f)
          target_facing = *desired;
      }
    }

    /* (forward x up) . desired, association (z + y) + x as in the binary */
    turn_rate = ((forward->i * up->j - forward->j * up->i) * desired->k +
                 (forward->k * up->i - up->k * forward->i) * desired->j +
                 (up->k * forward->j - up->j * forward->k) * desired->i) *
                  3.3333333f * biped->unit.throttle.x -
                biped->unit.throttle.y;
    if (turn_rate > 1.5f)
      turn_rate = 1.5f;

    target_bank = turn_rate * biped_definition_flying_bank_amount(definition);
    if (target_bank * biped->bank > 0.0f) {
      float bank_ratio = biped->bank / target_bank;

      if (bank_ratio > 1.0f)
        bank_ratio = 1.0f;
      bank_blend = 1.0f - bank_ratio;
    } else {
      bank_blend = 1.0f;
    }

    bank_time = (1.0f - bank_blend) * biped_definition_flying_bank_decay_time(definition) +
                bank_blend * biped_definition_flying_bank_apply_time(definition);
    if (bank_time > 0.0f) {
      biped->bank =
        (target_bank - biped->bank) / (bank_time * 30.0f) + biped->bank;
    } else {
      biped->bank = target_bank;
    }

    aiming_bounds.x0 = -_pi;
    aiming_bounds.x1 = _pi;
    aiming_bounds.y0 = -(_pi / 2);
    aiming_bounds.y1 = _pi / 2;
    angular_velocity_limit =
      biped_definition_flying_angular_velocity(definition) * 0.033333335f;
    angular_acceleration_limit =
      biped_definition_flying_angular_acceleration(definition) * 0.0011111111f;
    if (angular_acceleration_limit == 0.0f) {
      *forward = target_facing;
    } else {
      unit_euler_aiming_update(
        0, forward, &target_facing, angular_velocity, (float *)&aiming_bounds,
        angular_velocity_limit, angular_acceleration_limit);
    }

    biped_snap_facing(unit_handle);
    biped_verify_object_vectors(unit_handle, "post-fly-turn");
  } else if (biped->unit.base_seat_index != 0) {
    char flaming = 0;
    char turn_right;
    real_vector3d turn_axis;
    float turn_error;
    float facing_alignment;
    uint8_t biped_state;

    if (biped->unit.base_seat_index == 5)
      flaming = 1;

    if ((flags & 0x40) != 0) {
      float cross_x = desired->k * up->j - desired->j * up->k;
      float cross_y = desired->i * up->k - up->i * desired->k;
      float cross_z = up->i * desired->j - desired->i * up->j;

      turn_axis.i = cross_y * up->k - cross_z * up->j;
      turn_axis.j = cross_z * up->i - cross_x * up->k;
      turn_axis.k = cross_x * up->j - cross_y * up->i;
      if (normalize3d(&turn_axis) == 0.0f)
        turn_axis = *forward;

      /* (turn_axis x forward) . up, association (z + y) + x */
      turn_error =
        (turn_axis.i * forward->j - turn_axis.j * forward->i) * up->k +
        (turn_axis.k * forward->i - turn_axis.i * forward->k) * up->j +
        (turn_axis.j * forward->k - turn_axis.k * forward->j) * up->i;
      facing_alignment = turn_axis.j * forward->j + turn_axis.k * forward->k +
                         turn_axis.i * forward->i;
    } else {
      turn_axis = *desired;
      turn_axis.k = 0.0f;
      if (normalize2d((real_vector2d *)&turn_axis) == 0.0f)
        turn_axis = *forward;

      turn_error = turn_axis.i * forward->j - turn_axis.j * forward->i;
      facing_alignment = turn_axis.j * forward->j + turn_axis.i * forward->i;
    }

    turn_right = 1;
    if (!(turn_error > 0.0f))
      turn_right = 0;
    if (facing_alignment < -0.89999998f) {
      if (biped->unit.animation_state == 3)
        turn_right = 1;
      else if (biped->unit.animation_state == 2)
        turn_right = 0;
    }

    biped_state = biped->state;
    if (biped_state == 1 || (flags & 0x1) != 0) {
      if ((biped->unit.control_flags & 0x100) == 0) {
        float cosine =
          x87_fcos_mul(biped_definition_moving_turning_speed(definition), 0.033333335f);
        float sine =
          x87_fsin_mul(biped_definition_moving_turning_speed(definition), 0.033333335f);
        float progress;

        if (turn_right)
          sine = -sine;

        if ((flags & 0x40) != 0) {
          real_vector3d cross;

          rotate_vector3d_by_sincos(forward, up, sine, cosine);
          cross_product3d(&turn_axis, forward, &cross);
          progress = cross.j * up->j + cross.k * up->k + cross.i * up->i;
        } else {
          float forward_x = forward->i;
          float forward_y = forward->j;
          float rotated_y = sine * forward_x + cosine * forward_y;

          forward->i = cosine * forward_x - sine * forward_y;
          forward->j = rotated_y;
          progress = turn_axis.i * forward->j - turn_axis.j * forward->i;
        }

        if (turn_right ? progress < 0.0f : progress > 0.0f) {
          if (((uint8_t)definition->biped_flags & 0x40) != 0) {
            real_vector3d rotation_axis;

            cross_product3d(up, &turn_axis, &rotation_axis);
            if (normalize3d(&rotation_axis) > 0.0f)
              cross_product3d(&rotation_axis, up, forward);
          } else {
            forward->i = turn_axis.i;
            forward->j = turn_axis.j;
            forward->k = 0.0f;
            *up = *(real_vector3d *)global_up_vector_ptr;
          }

          normalize3d(forward);
        }

        biped_verify_object_vectors(unit_handle, "post-moving-turn");
      }
    } else if (biped_state == 0 && !flaming &&
               (biped->unit.flags & 0x4000) == 0 &&
               (biped->unit.control_flags & 0x100) == 0) {
      float stationary_turning_threshold;

      if ((biped->unit.control_flags & 0x20) != 0)
        stationary_turning_threshold = 0.99000001f;
      else
        stationary_turning_threshold = biped_definition_runtime_cosine_stationary_turning_threshold(definition);

      if (facing_alignment < stationary_turning_threshold &&
          (definition->unit.flags & 0x100000) == 0)
        *state = (unsigned char)((turn_right != 0) + 2);

      biped_verify_object_vectors(unit_handle, "post-standing-turn");
    }
  }
}

/* Collision model instance filled by collision_model_instance_new. */
typedef struct collision_model_instance_t {
  uint8_t pad_00[0x0c];
  real_matrix4x3 *node_matrices;              /* +0x0c */
} collision_model_instance_t;
cs(collision_model_instance_t, 0x10);
co(collision_model_instance_t, node_matrices, 0x0c);

/* biped_update_moving (0x1a5300)
 *
 * state[0] is the desired animation state, state[1] the crouching byte.
 *   - MIN(magnitude3d(throttle), 1) evaluates the magnitude twice: an inlined
 *     sqrt for the compare and magnitude3d for the value;
 *   - the default frame-info arm keeps dy = movement_desired.j (the zero
 *     vector copy);
 *   - every per-tick scale is a multiply by 1/30.
 *   - biped_update_jumping gets unit_handle in EAX; the caller still pushes
 *     state, which the callee never reads.
 *   - unit_impact_melee_damage args 3..5 are dword loads at model result
 *     +0x00, +0x02 (misaligned) and +0x1a.
 */
void biped_update_moving(int unit_handle, unsigned char *state)
{
  biped_data_t *biped;
  biped_definition_t *definition;
  real_vector3d *velocity;
  biped_physics physics;
  float movement_scale;
  vector3_t new_position;

  biped = object_get_and_verify_type(unit_handle, 1 << _object_type_biped);
  if (((uint8_t)biped->unit.object.flags & 0x20) != 0 &&
      (biped->unit.object.damage_flags & 0x4) != 0 &&
      (biped->unit.animation_flags & 0x4) != 0)
    return;

  definition = tag_get(TAG_GROUP_BIPD, biped->unit.object.tag_index);
  physics.biped_index = unit_handle;
  physics.in_flags = 0;
  physics.forward = biped->unit.object.forward;
  if ((definition->unit.flags & 0x800) != 0)
    physics.aiming = biped->unit.object.forward;
  else
    unit_get_aiming_vector(unit_handle, &physics.aiming);

  velocity = &biped->unit.object.translational_velocity;
  physics.velocity = *velocity;
  physics.crouch_velocity = 0.0f;
  physics.acceleration_maximum = 0.0053333333f;
  physics.airborne_acceleration_maximum = 0.0f;
  biped_get_camera_height_and_offset(unit_handle, &physics.position,
                                     &physics.height, &physics.width);
  physics.minimum_normal_k = biped_definition_runtime_minimum_normal_k(definition);
  physics.downhill_k0 = biped_definition_runtime_downhill_k0(definition);
  physics.downhill_k1 = biped_definition_runtime_downhill_k1(definition);
  physics.downhill_velocity_scale = biped_definition_downhill_velocity_scale(definition);
  physics.uphill_k0 = biped_definition_runtime_uphill_k0(definition);
  physics.uphill_k1 = biped_definition_runtime_uphill_k1(definition);
  physics.uphill_velocity_scale = biped_definition_uphill_velocity_scale(definition);
  physics.ground_plane.normal[0] = biped->ground_plane.normal[0];
  physics.ground_plane.normal[1] = biped->ground_plane.normal[1];
  physics.ground_plane.normal[2] = biped->ground_plane.normal[2];
  physics.ground_plane.d = biped->ground_plane.d;
  physics.ground_tangential_velocity_max = 3.4028235e38f;
  physics.ground_tangential_angle = 0.0f;
  physics.existing_support_surface_index = biped->support_surface_index;

  if (biped->landing == 1) {
    physics.movement_desired.i = 0.0f;
    physics.movement_desired.j = 0.0f;
    physics.movement_desired.k = 0.0f;
    physics.movement_penalty = 1.0f;
  } else {
    movement_scale = 1.0f;
    if ((definition->biped_flags & 0x800) != 0 &&
        biped->unit.aiming_speed == 0) {
      float difficulty = game_difficulty_get_value(8);
      unsigned long stagger_ordinal = (unsigned long)unit_handle % 137;

      movement_scale =
        difficulty * ((float)stagger_ordinal * 0.00729927f) + 1.0f;
    }

    if ((definition->biped_flags & 0x4) != 0 &&
        (biped->unit.object.damage_flags & 0x4) == 0) {
      state[0] = 0;
    } else if (biped->unit.throttle.x != 0.0f ||
               biped->unit.throttle.y != 0.0f ||
               biped->unit.throttle.z != 0.0f) {
      char stunned;

      if (biped->unit.body_stun > 0.2f)
        stunned = 1;
      else
        stunned = 0;
      if (unit_definition_stunned_movement_threshold(definition) > 0.0f &&
          biped->unit.object.unk_168 > unit_definition_stunned_movement_threshold(definition))
        stunned = 1;

      /* FCOMPP |y| vs |x|; JNE on C0|C3 selects the x arm */
      if (!(x87_fabs(biped->unit.throttle.y) >
            x87_fabs(biped->unit.throttle.x))) {
        if (biped->unit.throttle.x < 0.0f)
          state[0] = (unsigned char)((stunned != 0) * 4 + 5);
        else
          state[0] = (unsigned char)((stunned != 0) * 4 + 4);
      } else {
        if (biped->unit.throttle.y < 0.0f)
          state[0] = (unsigned char)((stunned != 0) * 4 + 7);
        else
          state[0] = (unsigned char)((stunned != 0) * 4 + 6);
      }
    }

    physics.movement_desired = *(real_vector3d *)global_zero_vector_ptr;
    if (biped->unit.object.unk_128 != NONE &&
        ((biped->unit.object.damage_flags & 0x4) != 0 ||
         ((uint8_t)definition->biped_flags & 0x4) == 0) &&
        (biped->unit.animation_flags & 0x4) == 0) {
      animation_graph_t *animation_graph;
      animation_t *animation;
      float dy;
      float dyaw = 0.0f;
      float *frame_info;

      tag_get(TAG_GROUP_MODE, object_definition_model_index(definition));
      animation_graph = tag_get(TAG_GROUP_ANTR, biped->unit.object.unk_124);
      animation = tag_block_get_element(&animation_graph->animations,
                                        biped->unit.object.unk_128, 0xb4);
      switch (animation->field_26) {
      case 1:
        frame_info = animation_get_frame_info(animation, biped->unit.object.unk_130, 8);
        physics.movement_desired.i = frame_info[0];
        dy = frame_info[1];
        break;
      case 2:
        frame_info = animation_get_frame_info(animation, biped->unit.object.unk_130, 0xc);
        physics.movement_desired.i = frame_info[0];
        dy = frame_info[1];
        dyaw = frame_info[2];
        break;
      case 3:
        frame_info = animation_get_frame_info(animation, biped->unit.object.unk_130, 0x10);
        physics.movement_desired.i = frame_info[0];
        dy = frame_info[1];
        physics.movement_desired.k = frame_info[2];
        dyaw = frame_info[3];
        break;
      default:
        dy = physics.movement_desired.j;
        break;
      }

      physics.movement_desired.i =
        physics.movement_desired.i * movement_scale;
      physics.movement_desired.j = dy * movement_scale;
      physics.movement_desired.k =
        physics.movement_desired.k * movement_scale;

      if (!((double)x87_fabs(dyaw) < (double)0.0001f)) {
        real_vector3d rotated_forward;
        real_vector3d *up = &biped->unit.object.up;
        real_vector3d *forward = &biped->unit.object.forward;
        real_vector3d *desired = (real_vector3d *)&biped->unit.desired_facing_vector;

        rotated_forward = *forward;
        rotate_vector3d_by_sincos(&rotated_forward, up, x87_fsin(dyaw),
                                  x87_fcos(dyaw));
        if (((uint8_t)biped->unit.control_flags & 0x20) != 0 &&
            (biped->unit.animation_state == 2 ||
             biped->unit.animation_state == 3) &&
            dot_product3d(desired, forward) > 0.5f) {
          real_vector3d old_cross;
          real_vector3d new_cross;

          cross_product3d(desired, forward, &old_cross);
          cross_product3d(desired, &rotated_forward,
                          &new_cross);
          if ((old_cross.j * up->j + old_cross.k * up->k +
               old_cross.i * up->i) *
                (new_cross.j * up->j + new_cross.k * up->k +
                 new_cross.i * up->i) <=
              0.0f) {
            rotated_forward = *desired;
            unit_abort_animation(unit_handle);
          }
        }

        *forward = rotated_forward;
        biped_snap_facing(unit_handle);
      }
    }

    if ((definition->biped_flags & 0x4) != 0 &&
        (biped->unit.object.damage_flags & 0x4) == 0) {
      real_vector3d *throttle = (real_vector3d *)&biped->unit.throttle;
      float throttle_magnitude;
      float deceleration_scale;
      float crouch_modifier = 1.0f;
      float acceleration;

      if (x87_sqrtd((double)throttle->k * throttle->k +
                    (double)throttle->i * throttle->i +
                    (double)throttle->j * throttle->j) < 1.0)
        throttle_magnitude = magnitude3d(throttle);
      else
        throttle_magnitude = 1.0f;
      deceleration_scale = 1.0f - throttle_magnitude;

      if (biped_definition_flying_crouch_velocity_modifier(definition) > 0.0f) {
        if (biped->crouch == 1.0f)
          crouch_modifier = biped_definition_flying_crouch_velocity_modifier(definition);
        else if (biped->crouch > 0.0f)
          crouch_modifier =
            (biped_definition_flying_crouch_velocity_modifier(definition) - 1.0f) * biped->crouch +
            1.0f;
      }

      physics.movement_desired.i = crouch_modifier *
                                   biped_definition_flying_velocity(definition) *
                                   movement_scale;
      physics.movement_desired.j = movement_scale * crouch_modifier *
                                   biped_definition_flying_sidestep_velocity(definition);
      physics.movement_desired.k = movement_scale * crouch_modifier *
                                   biped_definition_flying_sidestep_velocity(definition);
      acceleration =
        (1.0f - deceleration_scale) * biped_definition_flying_acceleration(definition) +
        deceleration_scale * biped_definition_flying_deceleration(definition);
      acceleration = acceleration * crouch_modifier * movement_scale;
      physics.movement_desired.i =
        physics.movement_desired.i * throttle->i * 0.033333335f;
      physics.movement_desired.j =
        physics.movement_desired.j * biped->unit.throttle.y * 0.033333335f;
      physics.movement_desired.k =
        physics.movement_desired.k * biped->unit.throttle.z * 0.033333335f;
      physics.acceleration_maximum = acceleration * 0.033333335f;
      physics.airborne_acceleration_maximum = physics.acceleration_maximum;
    } else if ((definition->biped_flags & 0x2) != 0 &&
               biped->unit.animation_state != 0x1c) {
      game_globals_player_information_t *player_information;
      float body_stun_scale;
      float stun_scale;
      float crouch;
      float uncrouch;
      float forward_speed;

      player_information = tag_block_get_element(
        &game_globals_player_information(game_globals_get()), 0,
        sizeof(game_globals_player_information_t));
      body_stun_scale = 1.0f;
      if (biped->unit.player_index.value != NONE) {
        player_data_t *player =
          datum_get(player_data, biped->unit.player_index.value);

        body_stun_scale = player->speed_multiplier;
      }

      stun_scale = body_stun_scale *
                   (1.0f - player_information->stun_movement_penalty *
                             biped->unit.body_stun) *
                   movement_scale;
      crouch = biped->crouch;
      uncrouch = 1.0f - crouch;
      if (biped->unit.throttle.x > 0.0f)
        physics.movement_desired.i =
          uncrouch * player_information->run_forward_speed +
          player_information->sneak_forward_speed * crouch;
      else
        physics.movement_desired.i =
          uncrouch * player_information->run_backward_speed +
          player_information->sneak_backward_speed * crouch;
      physics.movement_desired.k = 0.0f;
      physics.movement_desired.j =
        uncrouch * player_information->run_sideways_speed +
        player_information->sneak_sideways_speed * crouch;
      physics.acceleration_maximum =
        uncrouch * player_information->run_acceleration +
        player_information->sneak_acceleration * crouch;

      if (biped->unit.base_seat_index == 1) {
        if (biped->unit.throttle.x > 0.0f)
          forward_speed = player_information->walking_speed;
        else
          forward_speed = 0.0f;
        physics.movement_desired.j = 0.0f;
      } else {
        forward_speed = physics.movement_desired.i;
      }

      physics.movement_desired.i =
        stun_scale * forward_speed * biped->unit.throttle.x * 0.033333335f;
      physics.movement_desired.j = stun_scale * biped->unit.throttle.y *
                                    physics.movement_desired.j * 0.033333335f;
      physics.acceleration_maximum =
        physics.acceleration_maximum * 0.033333335f;
      physics.airborne_acceleration_maximum =
        player_information->airborne_acceleration * 0.033333335f;
      if ((biped->unit.control_flags & 0x100) == 0)
        physics.forward = *(real_vector3d *)&biped->unit.desired_facing_vector;

      if (game_players_are_double_speed()) {
        float double_speed_multiplier =
          player_information->double_speed_multiplier;

        physics.movement_desired.i =
          physics.movement_desired.i * double_speed_multiplier;
        physics.movement_desired.j =
          physics.movement_desired.j * double_speed_multiplier;
        physics.movement_desired.k =
          double_speed_multiplier * physics.movement_desired.k;
      }
    } else if (((uint8_t)biped->flags & 0x3) == 0) {
      velocity->i = physics.movement_desired.i;
      velocity->j = physics.movement_desired.j;
      physics.acceleration_maximum = 3.4028235e38f;
    }

    if (((uint8_t)biped->flags & 0x1) != 0 && biped->airborne_ticks < 0x16 &&
        biped->unit.actor_index.value != NONE &&
        actor_is_leaping(biped->unit.actor_index.value)) {
      physics.ground_tangential_velocity_max = 0.1f;
      physics.ground_tangential_angle = 0.5f;
    }

    physics.movement_penalty = 0.0f;
  }

  {
    float crouch_delta;

    if (biped->unit.base_seat_index == 3) {
      crouch_delta = 1.0f - biped->crouch;
      if (crouch_delta > biped_definition_runtime_crouch_transition_velocity(definition)) {
        biped->crouch = biped->crouch + biped_definition_runtime_crouch_transition_velocity(definition);
        crouch_delta = biped_definition_runtime_crouch_transition_velocity(definition);
      } else {
        biped->crouch = 1.0f;
      }
    } else {
      crouch_delta = -biped->crouch;
      if (crouch_delta < -biped_definition_runtime_crouch_transition_velocity(definition)) {
        biped->crouch = biped->crouch - biped_definition_runtime_crouch_transition_velocity(definition);
        crouch_delta = -biped_definition_runtime_crouch_transition_velocity(definition);
      } else {
        biped->crouch = 0.0f;
      }
    }

    if ((double)x87_fabs(crouch_delta) > (double)0.01f &&
        ((uint8_t)biped->flags & 0x1) != 0)
      physics.crouch_velocity = (biped_definition_collision_height_standing(definition) -
                                 biped_definition_collision_height_crouching(definition)) *
                                crouch_delta;
  }

  if (biped->crouch != 0.0f) {
    physics.in_flags |= 0x4;
    if (state[1] == 0)
      physics.in_flags |= 0x8;
  }
  if ((biped->flags & 0x1) != 0)
    physics.in_flags |= 0x1;
  if ((biped->flags & 0x2) != 0)
    physics.in_flags |= 0x2;
  if ((biped->flags & 0x4) != 0)
    physics.in_flags |= 0x20;
  if ((biped->flags & 0x8) != 0)
    physics.in_flags |= 0x40;
  if ((biped->unit.object.damage_flags & 0x4) != 0)
    physics.in_flags |= 0x80;
  if (((uint8_t)definition->biped_flags & 0x4) != 0 &&
      (biped->unit.object.damage_flags & 0x4) == 0)
    physics.in_flags |= 0x10;
  if (((uint8_t)definition->biped_flags & 0x20) != 0)
    physics.in_flags |= 0x100;
  if (((uint8_t)definition->biped_flags & 0x40) != 0 &&
      (biped->unit.object.damage_flags & 0x4) == 0)
    physics.in_flags |= 0x200;

  biped_update_physics(&physics);

  if (physics.elevator_object_index != NONE) {
    biped->elevator_ticks = 0x3c;
    biped->elevator_object_index = physics.elevator_object_index;
  } else if (biped->elevator_ticks > 0) {
    biped->elevator_ticks = (int8_t)(biped->elevator_ticks - 1);
  } else {
    biped->elevator_object_index = NONE;
  }

  if ((biped->unit.flags & 0x1000000) != 0) {
    physics.new_velocity = *(real_vector3d *)global_zero_vector_ptr;
    physics.out_flags &= ~0x1;
    physics.velocity = physics.new_velocity;
    physics.new_position = physics.position;
  }

  new_position = physics.new_position;
  if (((uint8_t)definition->biped_flags & 0x8) == 0)
    new_position.z = physics.new_position.z - physics.width;
  object_translate(unit_handle, &new_position, 0);

  *velocity = physics.new_velocity;
  biped->support_surface_index = physics.support_surface_index;
  biped->pathfinding_point = new_position;
  biped->pathfinding_surface_index = NONE;
  if (state[1] == 0 && (physics.out_flags & 0x4) != 0)
    state[1] = 1;

  if ((physics.out_flags & 0x1) != 0)
    biped->flags |= 0x1;
  else
    biped->flags &= ~0x1u;
  if ((physics.out_flags & 0x2) != 0)
    biped->flags |= 0x2;
  else
    biped->flags &= ~0x2u;
  if (((uint8_t)definition->biped_flags & 0x20) != 0)
    biped->flags |= 0x10;
  else
    biped->flags &= ~0x10u;

  biped->ground_plane.normal[0] = physics.ground_plane.normal[0];
  biped->ground_plane.normal[1] = physics.ground_plane.normal[1];
  biped->ground_plane.normal[2] = physics.ground_plane.normal[2];
  biped->ground_plane.d = physics.ground_plane.d;
  if (physics.landing_velocity > 0.0f)
    biped_start_landing(physics.landing_velocity, unit_handle);
  if ((physics.out_flags & 0x4) == 0)
    biped_update_jumping(unit_handle);

  if (biped->unit.melee_attack_state == 3 &&
      biped->impact_target_object_index != NONE) {
    object_data_t *target;
    int16_t depth;
    real_vector3d melee_vector;

    target = object_get_and_verify_type(biped->impact_target_object_index,
                                        _object_mask_all);
    if (global_current_collision_user_depth >= 0x20) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0x86d, true);
      system_exit(-1);
    }
    depth = global_current_collision_user_depth;
    global_current_collision_user_depth = (int16_t)(depth + 1);
    collision_user_stack[depth] = 8;

    melee_vector.i = physics.new_position.x - physics.position.x;
    melee_vector.j = physics.new_position.y - physics.position.y;
    melee_vector.k = physics.new_position.z - physics.position.z;
    if (fast_vector_intersects_sphere(
          &physics.position, &melee_vector,
          &target->bounding_sphere_center, target->unk_92)) {
      collision_model_instance_t instance;
      collision_model_vector_result model_result; /* [EBP-0x584] */
      struct collision_result world_collision;
      real_point3d impact_point;
      real_plane3d impact_plane;

      if ((uint8_t)collision_model_instance_new(
            (int *)&instance, biped->impact_target_object_index) &&
          collision_model_test_vector((int)&instance, (void *)3,
                                      &physics.position, &melee_vector,
                                      (int16_t *)&model_result) &&
          !collision_test_vector(0xc2a0, &physics.position,
                                 &melee_vector, unit_handle,
                                 (int16_t *)&world_collision)) {
        vector3d_scale_add(&physics.position, &melee_vector,
                           model_result.t,
                           &impact_point);
        matrix4x3_transform_plane(
          (float *)&instance.node_matrices[model_result.node_index],
                     (float *)model_result.plane, (float *)&impact_plane);
        if (model_result.plane_designator < 0)
          plane_negate((float *)&impact_plane, (float *)&impact_plane);
        unit_impact_melee_damage(
          unit_handle, biped->impact_target_object_index, INT16_DWORD_ARG(model_result, collision_model_vector_result, node_index),
          INT16_DWORD_ARG(model_result, collision_model_vector_result, region_index),
          INT16_DWORD_ARG(model_result, collision_model_vector_result, material_index), (int)&impact_point,
          (float *)&impact_plane, (int)&world_collision.location);
      }
    }

    if (global_current_collision_user_depth <= 1) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0x898, true);
      system_exit(-1);
    }
    global_current_collision_user_depth =
      (int16_t)(global_current_collision_user_depth - 1);
  }

  biped_bumped_object(physics.bumped_object_index, unit_handle,
                      &physics.velocity);
  biped_falling_damage(physics.landing_velocity, unit_handle);

  if ((physics.in_flags & 0x10) == 0 && ((uint8_t)biped->flags & 0x1) == 0)
    biped->unit.object.flags |= 0x2;
  else
    biped->unit.object.flags &= ~0x2u;
  if ((physics.in_flags & 0x10) == 0 && ((uint8_t)biped->flags & 0x1) == 0 &&
      (physics.out_flags & 0x10) == 0 &&
      velocity->i * velocity->i + velocity->j * velocity->j +
          velocity->k * velocity->k <
        0.0001f)
    biped->unit.object.flags |= 0x20;
  else
    biped->unit.object.flags &= ~0x20u;
  if ((biped->unit.object.flags & 0x2) != 0)
    biped->unit.object.angular_velocity =
      *(real_vector3d *)global_zero_vector_ptr;
}