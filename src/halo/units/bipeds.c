/* bipeds.c — biped-specific unit functions.
 *
 * Corresponds to bipeds.obj. Functions sorted by XBE address.
 * Debug assertion path: c:\halo\SOURCE\units\bipeds.c
 */

#include "../../common.h"
#include "../../x87_math.h"

/* FUN_001a01d0 (0x1a01d0)
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
void FUN_001a01d0(float *forward, float *left, float *up)
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
    forward[0] = global_forward_vector_ptr[0];
    forward[1] = global_forward_vector_ptr[1];
    forward[2] = global_forward_vector_ptr[2];
  }
  if (normalize3d(up) == 0.0f) {
    up[0] = global_up_vector_ptr[0];
    up[1] = global_up_vector_ptr[1];
    up[2] = global_up_vector_ptr[2];
  }

  /* left = up x forward. All three components are computed before any store
   * (the original cannot assume left[] doesn't alias forward[]/up[], so it
   * batches the cross product onto the x87 stack, then stores [0],[1],[2]). */
  lc2 = forward[1] * up[0] - up[1] * forward[0];
  lc1 = up[2] * forward[0] - forward[2] * up[0];
  lc0 = up[1] * forward[2] - up[2] * forward[1];
  left[0] = lc0;
  left[1] = lc1;
  left[2] = lc2;
  if (normalize3d(left) == 0.0f) {
    left[0] = global_left_vector_ptr[0];
    left[1] = global_left_vector_ptr[1];
    left[2] = global_left_vector_ptr[2];
  }

  /* up = forward x left */
  lc2 = left[1] * forward[0] - forward[1] * left[0];
  lc1 = forward[2] * left[0] - forward[0] * left[2];
  lc0 = forward[1] * left[2] - left[1] * forward[2];
  up[0] = lc0;
  up[1] = lc1;
  up[2] = lc2;
  if (normalize3d(up) == 0.0f) {
    up[0] = global_up_vector_ptr[0];
    up[1] = global_up_vector_ptr[1];
    up[2] = global_up_vector_ptr[2];
  }

  /* left = up x forward */
  lc2 = forward[1] * up[0] - up[1] * forward[0];
  lc1 = up[2] * forward[0] - forward[2] * up[0];
  lc0 = up[1] * forward[2] - up[2] * forward[1];
  left[0] = lc0;
  left[1] = lc1;
  left[2] = lc2;
  if (normalize3d(left) == 0.0f) {
    left[0] = global_left_vector_ptr[0];
    left[1] = global_left_vector_ptr[1];
    left[2] = global_left_vector_ptr[2];
  }
}

/* FUN_001a03c0 (0x1a03c0)
 *
 * Biped limp-noodle (ragdoll) node orientation updater.
 * Called once per frame to update the orientation (forward/left/up basis) of
 * each biped antenna/hair node using the current world-space positions.
 *
 * Register args (confirmed from caller 0x1a0680):
 *   unit_handle  EAX  — datum handle of the biped unit
 *   nodes        EDI  — pointer to node-state array; each entry is 0x34 bytes:
 *                         +0x00  position (vec3, 3 floats)
 *                         +0x04 .. +0x0f  (gaps/unused in this function)
 *                         +0x10  up vector (vec3, 3 floats)
 *                         +0x1c  ??? (vec3, 3 floats)
 *                         +0x28  position mirror? (vec3, 3 floats)
 *                         +0x30  (float*) pointer to current pos [EBP-0x10]
 *
 * Stack args:
 *   node_count   [EBP+0x08]  — dead param (body uses tag count instead)
 *   positions    [EBP+0x0c]  — pointer to position array; each entry 0x0c bytes
 *                              (vec3 stride 0x0c)
 *
 * For each antenna node i (skipping i==0):
 *   - Computes delta vectors A (positions delta) and B (nodes delta) from the
 *     parent node.
 *   - Normalizes both deltas.
 *   - Computes cross_axis = cross(A, B) and normalizes it.
 *   - Computes dot = dot(A, B).
 *   - If |dot - 1.0f| >= epsilon (not co-linear), computes angle = acos(dot).
 *   - If epsilon <= |angle| < max_angle, rotates the node's forward/up vectors
 *     around cross_axis by (sin(angle), cos(angle)), rebuilding the basis.
 *
 * Confirmed offsets (from disassembly):
 *   node stride:       0x34
 *   node +0x04:        forward vector (rotated by rotate_vector3d_by_sincos)
 *   node +0x10:        left   vector
 *   node +0x1c:        up     vector (rotated by rotate_vector3d_by_sincos)
 *   node +0x28:        position delta base (3 floats)
 *   positions stride:  0x0c
 *   tag block @[tag+0x68]: antenna block count+ptr
 *   element[i]+0x24:   short parent_node_index
 *   element[i]+0x28:   flags byte (bit 2 = skip-rotation)
 *
 * Globals: 0x2533c8 = 1.0f, 0x2533d0 = epsilon (~1e-6), 0x25b3f0 = max_angle
 *
 * Inferred: positions[0] is unused (loop body guarded by local_1c != 0).
 * Uncertain: exact semantics of node +0x10 and +0x1c fields.
 */
void FUN_001a03c0(int unit_handle, int node_count, float *positions,
                  void *nodes)
{
  /* unit_handle passed in EAX; nodes passed in EDI — see kb.json decl.
   * node_count ([EBP+8]) is a dead parameter; the body uses the tag count. */
  void *unit_data;
  void *bipd_tag;
  void *antr_tag;
  int *block; /* antenna tag_block: [0]=count                       */
  int i;
  float *pfx; /* &positions[i].z  (EBX in original, +0x0c/iter)     */
  float *nfx; /* &nodes[i]+0x30   (local_14, +0x34/iter)            */
  char *elem;
  char *parent_elem;
  float A[3]; /* pos delta  — [EBP-0x38], normalized in-place */
  float B[3]; /* node delta — [EBP-0x2c], normalized in-place */
  float cross[3]; /* cross(A,B) — [EBP-0x44], normalized in-place */
  float dot_val; /* [EBP-0x4] */
  float angle; /* [EBP-0x14] */
  float sin_a;
  char *nodes_bytes;
  char *base; /* nodes + parent_idx*0x34 */
  char axis_ok;
  float *saved_pfx; /* local_10 = [EBP-0xc] */

  nodes_bytes = (char *)nodes;

  unit_data = object_get_and_verify_type(unit_handle, 1);
  bipd_tag = tag_get(0x62697064, *(int *)unit_data);
  antr_tag = tag_get(0x616e7472, *(int *)((char *)bipd_tag + 0x44));
  block = (int *)((char *)antr_tag + 0x68);

  if (block[0] <= 0) {
    return;
  }

  /* pfx = positions+8 (points to .z of slot 0, pfx[-2]=.x pfx[-1]=.y)
   * nfx = nodes+0x30  (nfx[-2]=.x nfx[-1]=.y nfx[0]=.z of pos-mirror at +0x28)
   * Both advance by their respective strides each iteration. */
  pfx = (float *)((char *)positions + 8);
  nfx = (float *)(nodes_bytes + 0x30);

  i = 0;
  do {
    if (i != 0) {
      float *parent_pos;
      float *parent_nod28;

      saved_pfx = pfx;

      elem = (char *)tag_block_get_element(block, i, 0x40);
      parent_elem = (char *)tag_block_get_element(
        block, (int)(*(short *)(elem + 0x24)), 0x40);

      if ((*(unsigned char *)(parent_elem + 0x28) & 4) == 0) {
        parent_pos =
          (float *)((char *)positions + (int)(*(short *)(elem + 0x24)) * 0x0c);
        parent_nod28 =
          (float *)(nodes_bytes + (int)(*(short *)(elem + 0x24)) * 0x34 + 0x28);

        /* pos delta A = positions[i] - positions[parent] */
        A[0] = pfx[-2] - parent_pos[0];
        A[1] = pfx[-1] - parent_pos[1];
        A[2] = *pfx - parent_pos[2];

        /* nod delta B = nodes[i]+0x28 - nodes[parent]+0x28 */
        B[0] = nfx[-2] - parent_nod28[0];
        B[1] = nfx[-1] - parent_nod28[1];
        B[2] = nfx[0] - parent_nod28[2];

        normalize3d(A);
        normalize3d(B);

        /* cross(A,B) — operand order verified from FSUBP sequence:
         *   [0]: B[2]*A[1] - B[1]*A[2] = cross(A,B)[0]
         *   [1]: A[2]*B[0] - B[2]*A[0] = cross(A,B)[1]
         *   [2]: B[1]*A[0] - A[1]*B[0] = cross(A,B)[2]   */
        cross[0] = B[2] * A[1] - B[1] * A[2];
        cross[1] = A[2] * B[0] - B[2] * A[0];
        cross[2] = B[1] * A[0] - A[1] * B[0];

        normalize3d(cross);

        /* dot(A,B) — FPU order: A[2]*B[2] + A[1]*B[1] + A[0]*B[0] */
        dot_val = A[2] * B[2] + A[1] * B[1] + A[0] * B[0];

        /* if |dot - 1.0f| >= epsilon */
        if (*(double *)0x2533d0 <= fabs(dot_val - *(float *)0x2533c8)) {
          angle = acosf(dot_val);

          /* if epsilon <= |angle| < max_angle */
          if (*(double *)0x2533d0 <= fabs(angle) &&
              fabs(angle) < *(double *)0x25b3f0) {
            /* Original re-reads *(short*)(elem+0x24) and recomputes
             * parent_idx*0x34 + nodes + offset at EVERY call site.
             * Reproduce that re-read pattern faithfully. */

            /* First valid_real_vector3d_axes3 / FUN_001a01d0 block */
            base = nodes_bytes + (int)(*(short *)(elem + 0x24)) * 0x34;
            axis_ok = valid_real_vector3d_axes3((float *)(base + 0x4),
                                                (float *)(base + 0x10),
                                                (float *)(base + 0x1c));
            if (!axis_ok) {
              FUN_001a01d0((float *)(base + 0x4), (float *)(base + 0x10),
                           (float *)(base + 0x1c));
            }

            sin_a = x87_fsin(angle);

            /* rotate fwd and up — each re-reads *(short*)(elem+0x24) */
            rotate_vector3d_by_sincos(
              (float *)(nodes_bytes + (int)(*(short *)(elem + 0x24)) * 0x34 +
                        0x4),
              cross, sin_a, dot_val);
            rotate_vector3d_by_sincos(
              (float *)(nodes_bytes + (int)(*(short *)(elem + 0x24)) * 0x34 +
                        0x1c),
              cross, sin_a, dot_val);

            normalize3d((float *)(nodes_bytes +
                                  (int)(*(short *)(elem + 0x24)) * 0x34 + 0x4));
            normalize3d((float *)(nodes_bytes +
                                  (int)(*(short *)(elem + 0x24)) * 0x34 +
                                  0x1c));

            /* cross_product3d(up, fwd, left) — uses single base */
            base = nodes_bytes + (int)(*(short *)(elem + 0x24)) * 0x34;
            cross_product3d((float *)(base + 0x1c), (float *)(base + 0x4),
                            (float *)(base + 0x10));

            normalize3d((float *)(nodes_bytes +
                                  (int)(*(short *)(elem + 0x24)) * 0x34 +
                                  0x10));

            /* Second valid_real_vector3d_axes3 / FUN_001a01d0 block */
            base = nodes_bytes + (int)(*(short *)(elem + 0x24)) * 0x34;
            axis_ok = valid_real_vector3d_axes3((float *)(base + 0x4),
                                                (float *)(base + 0x10),
                                                (float *)(base + 0x1c));
            if (!axis_ok) {
              FUN_001a01d0((float *)(base + 0x4), (float *)(base + 0x10),
                           (float *)(base + 0x1c));
              pfx = saved_pfx;
            }
            pfx = saved_pfx;
          }
        }
      }
    }

    i++;
    pfx = pfx + 3; /* +0x0c bytes (3 floats per position) */
    nfx = nfx + (0x34 / 4); /* +0x34 bytes (13 floats per node)    */
  } while (i < block[0]);
}

/* FUN_001a0680 (0x1a0680)
 *
 * Updates biped limp-noodle (ragdoll/physics) node positions. Gets the biped
 * object and its 'bipd' tag, then looks up the 'antr' animation tag via the
 * tag reference at bipd+0x44. Fetches the object header block reference at
 * offset object+0x1a0 (the node transform block). Returns 1 if the current
 * node step count (byte at biped_obj+0x47c) >= the maximum step count (byte
 * at biped_obj+0x47d); otherwise copies node positions into the scratch buffer
 * at 0x4e49f0, calls FUN_0019fa20 to process limp-noodle physics, calls
 * FUN_001a03c0 to apply the updated positions back, increments the step
 * counter (capped at 0x7f), and returns 0.
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) type 1 (biped).
 * Confirmed: CALL 0x1ba140 (tag_get) 'bipd' (0x62697064) then 'antr'
 * (0x616e7472). Confirmed: CALL 0x13dfc0 (object_header_block_reference_get)
 * offset +0x1a0. Confirmed: comparison [ESI+0x47c] vs [ESI+0x47d] (CMP/SBB/INC
 * pattern). Confirmed: node stride 0x34 (13 floats), position offset +0x28
 * within node. Confirmed: scratch buffer at 0x4e49f0 (DAT_004e49f0). Confirmed:
 * antr node count at antr+0x68. Confirmed: step counter cap at 0x7f (JNC =
 * unsigned compare). Inferred: FUN_001a03c0 @<eax>=unit_handle,
 * @<edi>=node_block_ptr.
 */
char FUN_001a0680(int unit_handle)
{
  char *biped_obj;
  char *bipd_tag;
  char *antr_tag;
  void *node_block;
  char *obj_base;
  int count;
  int i;
  int *src_node;
  int *scratch;
  char result;

  biped_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  bipd_tag = (char *)tag_get(0x62697064, *(int *)biped_obj);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(bipd_tag + 0x44));
  obj_base = (char *)object_get_and_verify_type(unit_handle, -1);
  node_block = object_header_block_reference_get(unit_handle, obj_base + 0x1a0);

  /* CMP [ESI+0x47c],[ESI+0x47d]; SBB CL,CL; INC CL */
  result = (char)(*(unsigned char *)(biped_obj + 0x47c) >=
                  *(unsigned char *)(biped_obj + 0x47d));
  if (result != '\0') {
    return result;
  }

  count = *(int *)((char *)antr_tag + 0x68);
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

  FUN_0019fa20(unit_handle, node_block);
  /* @<eax>=unit_handle, node_count, positions=scratch buf, @<edi>=node_block */
  FUN_001a03c0(unit_handle, *(int *)((char *)antr_tag + 0x68),
               (float *)0x4e49f0, node_block);

  if (*(unsigned char *)(biped_obj + 0x47c) < 0x7f) {
    *(unsigned char *)(biped_obj + 0x47c) =
      *(unsigned char *)(biped_obj + 0x47c) + 1;
  }
  return result;
}

/* biped_place (0x1a07c0)
 *
 * Places a biped unit at a placement location. Calls unit_place with the
 * placement data at offset +0x48 (the unit-level placement struct), then
 * calls FUN_0013d870 (no-op stub) with the placement data at offset +0x28.
 *
 * Confirmed: param_2+0x48 passed to unit_place (PUSH ESI+0x48 via LEA).
 * Confirmed: param_2+0x28 passed to FUN_0013d870 (ADD ESI,0x28; PUSH ESI).
 * Confirmed: FUN_0013d870 is a no-op (single RET).
 * Inferred: param_2 is a biped placement struct (unit placement at +0x48,
 *   biped-specific data at +0x28).
 */
void biped_place(int unit_handle, void *placement)
{
  unit_place(unit_handle, (char *)placement + 0x48);
  FUN_0013d870(unit_handle, (char *)placement + 0x28);
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
  char *obj;
  int *dst;
  int tmp0;
  int tmp1;
  int tmp2;
  int tmp3;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  csmemset(obj + 0x424, 0, 0x5c);
  tmp0 = *(int *)0x32513c;
  dst = (int *)(obj + 0x46c);
  tmp1 = *(int *)0x325140;
  dst[0] = tmp0;
  tmp2 = *(int *)0x325144;
  dst[1] = tmp1;
  tmp3 = *(int *)0x325148;
  *(int *)(obj + 0x450) = -1;
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
  char *obj;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  *(int *)(obj + 0x430) = -1;
  *(int *)(obj + 0x434) = -1;
  *(int *)(obj + 0x448) = -1;
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
  char *unit_obj;
  char *biped_tag;
  float camera_height;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  object_get_world_position(unit_handle, out_pos);

  /* Add camera height to Z position unless flag bit 3 is set */
  if ((*(uint8_t *)(biped_tag + 0x2f4) & 0x8) == 0) {
    out_pos->z = *(float *)(biped_tag + 0x42c) + out_pos->z;
  }

  /* Calculate height offset based on crouch state */
  if ((*(uint8_t *)(biped_tag + 0x2f4) & 0x10) == 0 &&
      (*(int *)(unit_obj + 0x1c8) != -1 ||
       (*(uint32_t *)(unit_obj + 0x4) & 0x400000) != 0)) {
    /* Unit has actor or is player-controlled: interpolate based on crouch.
     * Memory-access order matches the original: standing/crouching heights and
     * crouch fraction are read inline; camera_height is cached once so the
     * doubling compiles to FADD ST0,ST0; out_camera_height re-reads [0x42c]. */
    camera_height = *(float *)(biped_tag + 0x42c);
    *out_height_offset =
      (*(float *)(biped_tag + 0x428) - *(float *)(biped_tag + 0x424)) *
        *(float *)(unit_obj + 0x464) +
      *(float *)(biped_tag + 0x424) - (camera_height + camera_height);
    *out_camera_height = *(float *)(biped_tag + 0x42c);
  } else {
    /* No actor and not player-controlled: zero offset */
    *out_height_offset = 0.0f;
    *out_camera_height = *(float *)(biped_tag + 0x42c);
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
  char *obj;

  obj = (char *)object_try_and_get_and_verify_type(unit_handle, 1);
  if (obj != NULL) {
    *(uint8_t *)(obj + 0x45d) = 0;
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
  char *obj;
  char *biped_tag;
  uint8_t type_const;
  uint32_t flags4;
  uint32_t flags424;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)obj);
  if (*(uint8_t *)0x4e4cf3 != 0) {
    return;
  }
  if ((*(uint32_t *)(biped_tag + 0x2f4) >> 9 & 1) == 0) {
    return;
  }
  if ((*(uint8_t *)(obj + 0x4) & 0x20) == 0) {
    return;
  }
  if ((*(uint8_t *)(obj + 0x424) & 0x21) != 0) {
    return;
  }
  *(uint8_t *)(obj + 0x47c) = 0;
  type_const = (uint8_t)FUN_0019f530(unit_handle);
  *(uint8_t *)(obj + 0x47d) = type_const;
  flags4 = *(uint32_t *)(obj + 0x4);
  flags424 = *(uint32_t *)(obj + 0x424);
  flags4 |= 0x800000;
  flags424 |= 0x20;
  *(uint32_t *)(obj + 0x4) = flags4;
  *(uint32_t *)(obj + 0x424) = flags424;
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
  char *obj;
  char *biped_tag;
  uint32_t tag_flags;
  uint32_t flags4;
  uint32_t flags424;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)obj);
  tag_flags = *(uint32_t *)(biped_tag + 0x2f4);
  if ((tag_flags & 0x200) == 0) {
    return;
  }
  flags424 = *(uint32_t *)(obj + 0x424);
  if ((flags424 & 0x20) == 0) {
    return;
  }
  flags4 = *(uint32_t *)(obj + 0x4) & 0xff7fffff;
  flags424 = flags424 & 0xffffffdf;
  *(uint32_t *)(obj + 0x4) = flags4;
  *(uint32_t *)(obj + 0x424) = flags424;
}

/* FUN_001a0a40 (0x1a0a40)
 *
 * Per-frame melee-contact/bump handler for bipeds. Tracks a timer (obj+0x458)
 * and triggers AI bump notification and optional player-unit reassignment.
 *
 * Register args (confirmed from caller FUN_001a5300 at 0x1a61ba/0x1a61c4):
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
void FUN_001a0a40(int contact_handle /* @edi */, int unit_handle /* @ebx */,
                  float *velocity_ptr)
{
  char *obj;
  char *contact_obj;
  int8_t timer;
  int16_t player_idx;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  tag_get(0x62697064, *(int *)obj);

  timer = *(int8_t *)(obj + 0x458);
  if (timer < 0) {
    if (contact_handle == -1) {
      *(int8_t *)(obj + 0x458) = timer + 1;
      return;
    }
  } else {
    if (contact_handle == -1) {
      return;
    }
    contact_obj = (char *)object_get_and_verify_type(contact_handle, -1);
    ai_handle_bump(unit_handle, contact_handle, velocity_ptr);
    if ((*(int *)(obj + 0x1c8) == -1) &&
        (recorded_animation_controlling_unit(unit_handle) == 0)) {
      return;
    }
    if (*(int *)(obj + 0x454) != contact_handle) {
      *(int *)(obj + 0x454) = contact_handle;
      *(int8_t *)(obj + 0x458) = 0;
      return;
    }
    timer = *(int8_t *)(obj + 0x458);
    timer++;
    *(int8_t *)(obj + 0x458) = timer;
    if (timer <= 3) {
      return;
    }
    if ((*(int16_t *)(contact_obj + 0x64) == 0) &&
        (*(uint8_t *)0x5aa893 != 0)) {
      player_idx = unit_get_local_player_index(unit_handle);
      if (player_idx != -1) {
        *(uint8_t *)(contact_obj + 0x458) = 0xf1;
        players_set_local_player_unit(player_idx, unit_handle);
      }
    }
  }
  *(uint8_t *)(obj + 0x458) = 0xf1;
}

/* FUN_001a0b30 (0x1a0b30)
 *
 * Checks whether a biped is stuck in an unreachable (bad) position and erases
 * it if so. Triggered once per update frame from the biped update loop.
 *
 * Register arg (confirmed from caller FUN_001a6350 at 0x1a6727):
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
char FUN_001a0b30(int unit_handle /* @edi */)
{
  char *obj;
  int actor_handle;
  const char *tag_name;
  const char *actor_desc;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  if (game_engine_running() != 0) {
    return 0;
  }
  if ((*(uint32_t *)(obj + 0x4) & 0x200000) == 0 &&
      *(int16_t *)(obj + 0x4c) != -1) {
    return 0;
  }
  if (!(*(float *)(obj + 0x14) < *(float *)0x2b4d1c)) {
    return 0;
  }
  actor_handle = *(int *)(obj + 0x1a8);
  if (actor_handle == -1) {
    actor_handle = *(int *)(obj + 0x1a4);
  }
  actor_desc = ai_debug_describe_actor(actor_handle, unit_handle, 1,
                                       (char *)0x5ab100, 0x100);
  tag_name = tag_get_name(*(int *)obj);
  tag_name = tag_name_strip_path(tag_name);
  error(2, "WARNING: biped %s (%s) is in a bad place (%.1f %.1f %.1f), erasing",
        tag_name, actor_desc, (double)*(float *)(obj + 0xc),
        (double)*(float *)(obj + 0x10), (double)*(float *)(obj + 0x14));
  object_delete(unit_handle);
  return 0;
}

/* FUN_001a0be0 (0x1a0be0)
 *
 * Per-frame biped world-boundary check. If the biped falls outside the world
 * or below the kill-volume threshold, applies damage and/or erases the biped.
 *
 * Register arg (confirmed from caller FUN_001a5300 at 0x1a61d1):
 *   @edi  unit_handle  — biped datum handle
 * Stack arg:
 *   vertical_speed  [EBP+0x8]  — current vertical speed (float, from local_24
 *     in caller = camera Z delta or velocity component)
 *
 * Logic summary:
 *   1. Get biped object and tag. Get game_globals physics block element [0].
 *   2. Compute bVar1 (= "is_protected"): set true if obj+0x1b4 bit 0x1000 set
 *      OR tag+0x2f4 bit 7 (0x80) set.
 *   3. If multiplayer-engine running (DAT_5aa891) AND obj+0x1c8 != -1: skip.
 *   4. If vertical_speed > physics[0x90] (kill-height threshold):
 *      - If not protected: build damage params from physics[0x1c] tag ref,
 *        call damage_data_new + object_cause_damage.
 *      - Compute t = clamp01((speed - low) / (high - low)) and return.
 *   5. Else (below threshold):
 *      - Check tag flag bit 2 (0x4) at tag+0x2f4 and vertical speed.
 *      - If not flying and falling fast: possibly apply damage, then erase.
 *
 * Confirmed: tag_block_get_element(game_globals+0x188, 0, 0x98).
 * Confirmed: TEST AH,0x10 checks obj+0x1b4 bit 0x1000.
 * Confirmed: physics block offsets 0x8c (fall speed), 0x90 (jump height),
 *   0x94 (climb height), 0x38/0x1c (damage tag refs).
 * Confirmed: damage_data_new, object_cause_damage call arg counts from disasm.
 * Confirmed: clamp01 via FCOMP+FNSTSW+TEST pattern.
 * Confirmed: erasing path = player_index_from_unit_index +
 * ai_debug_describe_actor
 *   + error("fell outside world") + object_delete.
 * Inferred: vertical_speed param meaning (from call site local_24 in 0x1a5300).
 * Uncertain: exact semantics of obj+0x1b4 bit 0x1000 (possibly "in vehicle" or
 *   "has shield").
 */
void FUN_001a0be0(float vertical_speed, int unit_handle /* @edi */)
{
  char *obj;
  char *biped_tag;
  char *physics;
  char damage_params[0x40];
  float t;
  int actor_handle;
  int player_idx;
  const char *tag_name;
  const char *actor_desc;
  char is_protected;

  obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)obj);
  physics =
    (char *)tag_block_get_element((char *)game_globals_get() + 0x188, 0, 0x98);

  /* is_protected = obj has special flag OR tag marks it protected */
  if ((*(uint32_t *)(obj + 0x1b4) & 0x1000) != 0 ||
      *(int8_t *)(biped_tag + 0x2f4) < 0) {
    is_protected = 1;
  } else {
    is_protected = 0;
  }

  /* In multiplayer with a vehicle: skip all checks */
  if (*(uint8_t *)0x5aa891 != 0 && *(int *)(obj + 0x1c8) != -1) {
    return;
  }

  if (vertical_speed > *(float *)(physics + 0x90)) {
    /* Above jump-height threshold: if protected return early, else damage +
     * return */
    if (is_protected) {
      return;
    }
    damage_data_new(damage_params, *(int *)(physics + 0x1c));
    /* Compute lerp t = clamp01((speed - low) / (high - low)) */
    t = (vertical_speed - *(float *)(physics + 0x90)) /
        (*(float *)(physics + 0x94) - *(float *)(physics + 0x90));
    if (t < *(float *)0x2533c0) {
      t = 0.0f;
    } else if (!(t <= *(float *)0x2533c8)) {
      t = 1.0f;
    }
    object_cause_damage(damage_params, unit_handle, -1, -1, -1, 0);
    return;
  }

  /* Below threshold — check for fall/erase conditions */
  if ((*(uint8_t *)(biped_tag + 0x2f4) & 0x4) != 0) {
    return;
  }
  if (!(*(float *)(obj + 0x20) < -(*(float *)(physics + 0x8c)))) {
    return;
  }
  if (!is_protected && (*(uint8_t *)(obj + 0xb6) & 0x4) == 0) {
    damage_data_new(damage_params, *(int *)(physics + 0x38));
    object_cause_damage(damage_params, unit_handle, -1, -1, -1, 0);
  }
  if (game_engine_running() != 0) {
    return;
  }
  if ((*(uint32_t *)(obj + 0x4) & 0x200000) == 0) {
    return;
  }
  player_idx = player_index_from_unit_index(unit_handle);
  if (player_idx != -1) {
    return;
  }
  actor_handle = *(int *)(obj + 0x1a8);
  if (actor_handle == -1) {
    actor_handle = *(int *)(obj + 0x1a4);
  }
  actor_desc = ai_debug_describe_actor(actor_handle, unit_handle, 1,
                                       (char *)0x5ab100, 0x100);
  tag_name = tag_get_name(*(int *)obj);
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
  char *unit_obj;
  char *biped_tag;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  if (*(char *)(unit_obj + 0x459) > 3 &&
      (((*(unsigned char *)(biped_tag + 0x2f4) & 4) == 0) ||
       ((*(unsigned char *)(unit_obj + 0xb6) & 4) != 0))) {
    return 1;
  }
  return 0;
}

/* FUN_001a0e00 (0x1a0e00)
 *
 * Advances a biped animation/transition phase based on an elapsed-time
 * threshold. The biped tag stores three phase boundaries (in ticks) at
 * tag+0x3dc/0x3e0/0x3e4, converted to seconds via *(1/30). Given the elapsed
 * time `threshold`, selects which phase the biped is in, computes a normalized
 * progress t (clamped to [0,1]) across that phase, and writes:
 *   unit+0x460 (uint16) = phase flag (0 = first phase, 1 = second phase)
 *   unit+0x428 (byte)   = 0
 *   unit+0x429 (byte)   = (char)(int)(phase_rate * t)
 * where phase_rate = tag[0x3d4 or 0x3d8] * TICKS_PER_SECOND.
 *
 * unit_handle is passed in EAX (register arg); `threshold` is the cdecl stack
 * arg. _ftol2 (0x1d9068) is the MSVC float->int intrinsic — written as (int).
 * Constant 0x2546a4 = seconds-per-tick (1/30); 0x253394 = TICKS_PER_SECOND.
 */
void FUN_001a0e00(float threshold, int unit_handle)
{
  char *unit_obj;
  char *biped_tag;
  float fVar1;
  float fVar2;
  float range;
  float offset;
  float base;
  float scaled;
  float t;
  int flag;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  fVar1 = *(float *)(biped_tag + 0x3dc) * *(float *)0x2546a4;
  fVar2 = *(float *)(biped_tag + 0x3e0) * *(float *)0x2546a4;

  /* Branch primitives transcribed from the disassembly: the original tests with
   * `<` (FCOMP; TEST AH,0x5) and inverts the jump, so mirror those `<` forms
   * and the early-exit shape rather than the algebraically-equivalent `<=`
   * nesting. */
  if (threshold < fVar1) {
    return;
  }
  if (threshold < fVar2) {
    offset = threshold - fVar1;
    range = fVar2 - fVar1;
    base = *(float *)(biped_tag + 0x3d4);
    flag = 0;
  } else {
    range = *(float *)(biped_tag + 0x3e4) * *(float *)0x2546a4 - fVar2;
    offset = threshold;
    base = *(float *)(biped_tag + 0x3d8);
    flag = 1;
  }
  scaled = base * TICKS_PER_SECOND;
  if (range <= 0.0f) {
    return;
  }
  t = offset / range;
  if (t < 0.0f) {
    t = 0.0f;
  } else if (1.0f < t) {
    t = 1.0f;
  }
  *(uint16_t *)(unit_obj + 0x460) = (uint16_t)flag;
  *(unsigned char *)(unit_obj + 0x428) = 0;
  *(unsigned char *)(unit_obj + 0x429) = (unsigned char)(int)(scaled * t);
}

/* FUN_001a0f10 (0x1a0f10)
 *
 * Spawns a biped contact/footstep effect from one entry of the biped tag's
 * contact-point block. Looks up the biped tag ('bipd') from the unit, brackets
 * the work in the collision-user-depth stack (global 0x4761d8 / stack 0x5a8c80,
 * marker 7), asserting depth < 0x20 on entry (line 0xf4f) and > 1 on exit
 * (line 0xf60). If the requested contact index (register BX) is in range of the
 * tag's contact-point block at tag+0x4e8 AND the effect tag reference at
 * tag+0x398 is valid (!= -1), and the object's animation/contact gate
 * FUN_0009f3b0(object+0x50) passes, it fetches contact-point element BX
 * (element size 0x40), resolves the named marker (name at element+0x20) on the
 * object via object_get_markers_by_string_id (one marker, into a 108-byte
 * result buffer), and on success spawns the effect (tag+0x398) at the marker's
 * world position (buffer+0x60) via FUN_0009f570.
 *
 * Confirmed (disasm): cdecl, 2 stack params [EBP+8]=unit_handle, [EBP+0xc];
 *   index is register-passed in BX (MOVSX EBX,BX at 0x1a0f73 reads BX before
 *   any write; callers 0x1a2440 load EBX immediately before each CALL). void
 *   return. The marker-result buffer is one contiguous region: Ghidra split it
 *   into local_74[96]+local_14[12], but object_get_markers_by_string_id writes
 *   to offset 0x6c (108 bytes) and FUN_0009f570 reads the position at +0x60
 *   (LEA [EBP-0x70] vs LEA [EBP-0x10] differ by exactly 0x60).
 * Inferred: 'bipd' contact-point footstep-effect spawn semantics from the
 *   tag-block index + effect-tag + marker-position spawn shape.
 * Uncertain: precise meaning of param_2 (forwarded unchanged to FUN_0009f570);
 *   callers pass 3 or 4 (region/permutation selector). Layout of the 108-byte
 *   marker-result buffer beyond "transform copy at +0x38..0x6c, position at
 *   +0x60" is opaque (no named struct in headers yet).
 */
void FUN_001a0f10(int unit_handle, int param_2, short index /* @bx */)
{
  unsigned int *object;
  int biped_tag;
  int depth;
  void *contact_elem;
  /* One contiguous marker-result buffer. object_get_markers_by_string_id
   * writes up to offset 0x6c (108 bytes); FUN_0009f570 reads the marker
   * world position at +0x60. Sized so the MSVC frame totals 0x70 with the
   * 4-byte object pointer (do not split into separate locals). */
  char marker_buf[0x6c];

  object = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (int)tag_get(0x62697064, *object); /* 'bipd' */

  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf4f, true);
    system_exit(-1);
  }
  depth = *(int16_t *)0x4761d8;
  *(int16_t *)0x4761d8 = (int16_t)(depth + 1);
  *(int16_t *)(0x5a8c80 + depth * 2) = 7;

  if (((int)index < *(int *)(biped_tag + 0x4e8)) &&
      (*(int *)(biped_tag + 0x398) != -1)) {
    if (FUN_0009f3b0((char *)object + 0x50) != false) {
      contact_elem =
        tag_block_get_element((void *)(biped_tag + 0x4e8), (int)index, 0x40);
      if (object_get_markers_by_string_id(
            unit_handle, (char *)contact_elem + 0x20, marker_buf, 1) != 0) {
        FUN_0009f570(*(int *)(biped_tag + 0x398), param_2, marker_buf + 0x60,
                     0);
      }
    }
  }

  if (*(int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf60, true);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 = (int16_t)(*(int16_t *)0x4761d8 - 1);
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
  char *unit_obj;
  char *biped_tag;
  float camera_height;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  if ((*(unsigned int *)(biped_tag + 0x2f4) & 8) != 0 &&
      (*(unsigned int *)(biped_tag + 0x2f4) & 4) == 0) {
    camera_height = *(float *)(biped_tag + 0x42c);
    *(float *)(placement + 0x18) =
      camera_height * *(float *)(placement + 0x40) +
      *(float *)(placement + 0x18);
    *(float *)(placement + 0x1c) =
      camera_height * *(float *)(placement + 0x44) +
      *(float *)(placement + 0x1c);
    *(float *)(placement + 0x20) =
      camera_height * *(float *)(placement + 0x48) +
      *(float *)(placement + 0x20);
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
  char *unit_obj;
  char *biped_tag;
  short *type_ptr;
  float *out_ptr;
  float *vel;
  float ratio;
  float val;
  int count;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  type_ptr = (short *)(biped_tag + 0x30c);
  out_ptr = (float *)(unit_obj + 0xd4);
  count = 4;
  do {
    if (*type_ptr != 0) {
      val = 0.0f;
      if (*type_ptr == 1) {
        vel = (float *)(unit_obj + 0x18);
        ratio = sqrtf(vel[0] * vel[0] + vel[1] * vel[1] + vel[2] * vel[2]) /
                (*(float *)(biped_tag + 0x334) * *(float *)0x2546a4);
        if (0.0f <= ratio) {
          val = ratio;
          if (1.0f < ratio) {
            val = 1.0f;
          }
        }
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
 * Confirmed: Tag offsets 0x400 (crouching height), 0x404 (standing height).
 * Confirmed: Unit offset 0x464 (crouch fraction).
 * Confirmed: Globals 0x2533c0 (0.0f), 0x2533c8 (1.0f).
 */
void biped_estimate_position(int unit_handle, int16_t estimate_mode,
                             vector3_t *estimated_body_position,
                             vector3_t *desired_facing,
                             vector3_t *desired_gun_offset,
                             vector3_t *out_position)
{
  char *unit_obj;
  char *biped_tag;
  float crouch_value;
  float neg_facing_y;
  float facing_x;
  float gun_y;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

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
    crouch_value = 0.0f; /* Crouching */
    break;
  case 2:
    crouch_value = 1.0f; /* Standing */
    break;
  default:
    crouch_value = *(float *)(unit_obj + 0x464); /* Actual crouch fraction */
    break;
  }

  /* Interpolate height between crouching and standing based on crouch value.
   * Order matches the original FPU sequence: (1-cv)*crouching + cv*standing. */
  out_position->z = (1.0f - crouch_value) * *(float *)(biped_tag + 0x400) +
                    crouch_value * *(float *)(biped_tag + 0x404) +
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
 * 0.5f = *(float*)0x253398; axis = marker1 - marker0 (FSUB order from disasm).
 */
void biped_get_autoaim_pill(int unit_handle, float *out_pos, float *out_axis,
                            int *out_value)
{
  char *unit_obj;
  char *biped_tag;
  char *marker0;
  char *marker1;
  float height_offset;
  float camera_height;
  float half;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  if (*(short *)(biped_tag + 0x4e4) != -1 &&
      *(short *)(biped_tag + 0x4e6) != -1) {
    marker0 = (char *)object_get_node_matrix(unit_handle,
                                             *(short *)(biped_tag + 0x4e4));
    marker1 = (char *)object_get_node_matrix(unit_handle,
                                             *(short *)(biped_tag + 0x4e6));
    if ((*(unsigned char *)(biped_tag + 0x2f4) & 0x10) != 0) {
      midpoint3d((float *)(marker0 + 0x28), (float *)(marker1 + 0x28), out_pos);
      out_axis[0] = global_zero_vector_ptr[0];
      out_axis[1] = global_zero_vector_ptr[1];
      out_axis[2] = global_zero_vector_ptr[2];
      *out_value = *(int *)(biped_tag + 0x458);
      return;
    }
    out_pos[0] = *(float *)(marker0 + 0x28);
    out_pos[1] = *(float *)(marker0 + 0x2c);
    out_pos[2] = *(float *)(marker0 + 0x30);
    out_axis[0] = *(float *)(marker1 + 0x28) - *(float *)(marker0 + 0x28);
    out_axis[1] = *(float *)(marker1 + 0x2c) - *(float *)(marker0 + 0x2c);
    out_axis[2] = *(float *)(marker1 + 0x30) - *(float *)(marker0 + 0x30);
    *out_value = *(int *)(biped_tag + 0x458);
    return;
  }

  biped_get_camera_height_and_offset(unit_handle, (vector3_t *)out_pos,
                                     &height_offset, &camera_height);
  half = height_offset * *(float *)0x253398;
  out_pos[2] = half + out_pos[2];
  out_axis[0] = half * global_up_vector_ptr[0];
  out_axis[1] = half * global_up_vector_ptr[1];
  out_axis[2] = half * global_up_vector_ptr[2];
  *out_value = *(int *)(biped_tag + 0x458);
}

/* biped_fix_position (0x1a1430)
 *
 * Tries to find a collision-free world position for a biped (used when
 * teleporting it out of a vehicle seat, respawning, etc.). Starting from an
 * initial position (either supplied via initial_position, or the biped's
 * estimated camera/standing position when NULL), it walks a fixed table of
 * displacement directions at 0x2b4b80 (stride 3 floats), scaling each by
 * `scale`. For each candidate it (1) finds the BSP3D leaf, rejecting points
 * outside the BSP; (2) runs a battery of collision tests (FUN_0014f020 sphere
 * fit, FUN_0014e7d0 vector-to-surface, and when a seat is involved the
 * FUN_0014cc80 / FUN_000130d0 line-of-sight checks against both objects). The
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
  int bsp;
  int leaf_elem;
  float *src_pos;
  float *basis; /* biped_obj as float[] — basis-matrix rows at +0x24..0x38 */
  float position[3]; /* local_2c/28/24 */
  float camera_height; /* local_18 ([EBP-0x18], 1a0890 arg4) */
  float height_offset; /* local_1c ([EBP-0x1c], 1a0890 arg3) */
  float offset_vec[3]; /* local_4c/48/44 */
  float cross[3]; /* local_40/3c/38 */
  float fx, fy;
  float candidate[3]; /* local_14: candidate.x; local_10/c/8 below */
  float cand[3]; /* local_10/c/8 — the BSP query point */
  /* Out buffers passed to collision callees. Sized per each callee's writes;
   * preserve sizes to keep the original 0x538 frame layout. */
  char center[12]; /* local_64 — 1aae0 sphere center / 130d0 args */
  char box[16]; /* local_74 — 14c8e0 out, reused as 14cc80 arg1 */
  char location[16]; /* local_58/54/50 — scenario_location_from_point */
  char surf_result[80]; /* local_114 — 14e7d0 result */
  char los_result[56]; /* local_c4 — 130d0 result; +0x38 = hit handle */
  static char obstruction[1064]; /* local_53c — 14cc80 working buffer */

  success = 0;
  if ((final_position == (float *)0) && (dont_teleport != '\0')) {
    display_assert("final_position || !dont_teleport",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x37d, true);
    system_exit(-1);
  }
  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x37f, true);
    system_exit(-1);
  }
  depth = *(int16_t *)0x4761d8;
  *(int16_t *)0x4761d8 = (int16_t)(depth + 1);
  *(int16_t *)(0x5a8c80 + depth * 2) = 7;

  /* Branch structure preserved from disasm (0x1a14aa): call FUN_0001aae0 only
   * when a seat is involved, jumping straight to the epilogue when both handles
   * are NONE. */
  if (unit_handle == -1) {
    if (seat_handle == -1) {
      goto epilogue;
    }
    goto call_aae0;
  }
  if (seat_handle != -1) {
  call_aae0:
    FUN_0001aae0(seat_handle, (float *)center, (float *)&from_seat);
  }
  {
    from_seat = (unit_handle == -1);
    if (from_seat) {
      unit_handle = seat_handle;
    }
    biped_obj = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
    biped_tag = (int)tag_get(0x62697064, *biped_obj); /* 'bipd' */
    collision_flags =
      (-(int)((*(unsigned int *)(biped_tag + 0x2f4) & 0x20) != 0) &
       0xffdfff00) +
      0x20c3a0;

    if (initial_position == (float *)0) {
      src_pos = &position[0];
    } else {
      position[0] = initial_position[0];
      position[1] = initial_position[1];
      position[2] = initial_position[2];
      src_pos = &offset_vec[0];
    }
    biped_get_camera_height_and_offset(unit_handle, (vector3_t *)src_pos,
                                       &height_offset, &camera_height);
    if (from_seat) {
      unit_handle = -1;
    }
    loop_limit = (short)(0x1b - ((keep_basis != '\0') - 1 & 9));
    if (seat_handle != -1) {
      FUN_0014c8e0((int *)box, seat_handle);
    }
    /* cross = right_axis(obj+0x24) x up_axis(obj+0x30); normalize. The basis
     * rows at obj+0x24..0x38 are floats; load them as floats (FLD), not via
     * int->float conversion. FSUBP order preserved exactly from disasm. */
    basis = (float *)biped_obj;
    cross[0] = basis[0xe] * basis[0xa] - basis[0xb] * basis[0xd];
    cross[1] = basis[0xb] * basis[0xc] - basis[9] * basis[0xe];
    cross[2] = basis[9] * basis[0xd] - basis[0xc] * basis[0xa];
    normalize3d(&cross[0]);
    offset_vec[0] = height_offset * global_up_vector_ptr[0];
    offset_vec[1] = height_offset * global_up_vector_ptr[1];
    offset_vec[2] = height_offset * global_up_vector_ptr[2];
    if (scale_by_height != '\0') {
      scale = camera_height * scale;
    }

    i = 0;
    while ((short)i < loop_limit) {
      leaf_index = (int)(short)i;
      if (keep_basis == '\0') {
        cand[0] = scale * ((float *)0x2b4b80)[leaf_index * 3] + position[0];
        cand[1] = scale * ((float *)0x2b4b84)[leaf_index * 3] + position[1];
        cand[2] = scale * ((float *)0x2b4b88)[leaf_index * 3] + position[2];
      } else {
        fx = scale * ((float *)0x2b4b80)[leaf_index * 3];
        fy = scale * ((float *)0x2b4b84)[leaf_index * 3];
        candidate[2] = scale * ((float *)0x2b4b88)[leaf_index * 3];
        cand[0] = candidate[2] * basis[0xc] + cross[0] * fy + fx * basis[9] +
                  position[0];
        cand[1] = candidate[2] * basis[0xd] + cross[1] * fy + fx * basis[0xa] +
                  position[1];
        cand[2] = candidate[2] * basis[0xe] + cross[2] * fy + fx * basis[0xb] +
                  position[2];
      }

      if (FUN_0018e720((int)&cand[0]) != -1) {
        leaf_index = FUN_0018e720((int)&cand[0]) & 0x7fffffff;
        bsp = (int)scenario_get();
        leaf_elem =
          (int)tag_block_get_element((void *)(bsp + 0xe0), leaf_index, 0x10);
        if ((*(short *)(leaf_elem + 8) != -1) &&
            (FUN_0014f020((uint32_t)collision_flags, &cand[0],
                          camera_height + camera_height, height_offset,
                          camera_height, unit_handle, &cand[0]) != '\0') &&
            (FUN_0014e7d0((uint32_t)collision_flags, &cand[0], &offset_vec[0],
                          camera_height, unit_handle, surf_result) == '\0') &&
            ((seat_handle == -1) ||
             ((FUN_0014cc80((int)box, (int)&cand[0], (int)&offset_vec[0],
                            camera_height, (int16_t *)obstruction) == '\0') &&
              ((FUN_000130d0((uint32_t)collision_flags, &cand[0],
                             (float *)center, unit_handle,
                             (int16_t *)los_result) == 0) ||
               (*(int *)(los_result + 0x38) == seat_handle)) &&
              ((FUN_000130d0((uint32_t)collision_flags, (float *)center,
                             &cand[0], seat_handle,
                             (int16_t *)los_result) == 0) ||
               (*(int *)(los_result + 0x38) == unit_handle))))) {
          biped_tag = (int)tag_get(0x62697064, *biped_obj);
          scenario_location_from_point(location, &cand[0]);
          if (*(short *)(location + 4) == -1) {
            display_assert("fixed_location.cluster_index!=NONE",
                           "c:\\halo\\SOURCE\\units\\bipeds.c", 0x41e, true);
            system_exit(-1);
          }
          if ((*(unsigned char *)(biped_tag + 0x2f4) & 8) == 0) {
            cand[2] = cand[2] - *(float *)(biped_tag + 0x42c);
          }
          if ((unit_handle != -1) && (dont_teleport == '\0')) {
            biped_obj[3] = *(unsigned int *)&cand[0];
            biped_obj[4] = *(unsigned int *)&cand[1];
            biped_obj[5] = *(unsigned int *)&cand[2];
            object_update_children_recursive(unit_handle);
            object_translate(unit_handle, &cand[0], location);
          }
          if (final_position != (float *)0) {
            final_position[0] = cand[0];
            final_position[1] = cand[1];
            final_position[2] = cand[2];
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
  if (*(int16_t *)0x4761d8 < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x438, true);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 = (int16_t)(*(int16_t *)0x4761d8 - 1);
  return (char)success;
}

/* biped_render_debug (0x1a1900)
 *
 * Debug-visualization for a biped's camera/aim geometry, gated by two debug
 * globals. When 0x5054fe is set, draws the camera height/offset: if the height
 * offset is at/below the small threshold (0x2533c0), renders a point
 * (FUN_00189540); otherwise scales the world-up vector by the height offset and
 * renders a vector arrow (FUN_00189860). When 0x5054fd is set, fetches the
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
  float scaled[3];
  float height_offset;
  float camera_height;

  if (*(char *)0x5054fe != '\0') {
    biped_get_camera_height_and_offset(unit_handle, &out_pos, &height_offset,
                                       &camera_height);
    if (*(float *)0x2533c0 < height_offset) {
      scaled[0] = height_offset * global_up_vector_ptr[0];
      scaled[1] = height_offset * global_up_vector_ptr[1];
      scaled[2] = height_offset * global_up_vector_ptr[2];
      FUN_00189860(1, &out_pos, scaled, camera_height, *(void **)0x2ee6c4);
    } else {
      FUN_00189540(1, &out_pos, camera_height, *(void **)0x2ee6c4);
    }
  }
  if (*(char *)0x5054fd != '\0') {
    biped_get_autoaim_pill(unit_handle, (float *)&out_pos, scaled,
                           (int *)&camera_height);
    if (*(float *)0x2533c0 <
        scaled[2] * scaled[2] + scaled[1] * scaled[1] + scaled[0] * scaled[0]) {
      FUN_00189860(1, &out_pos, scaled, camera_height, *(void **)0x2ee6d0);
      return;
    }
    FUN_00189540(1, &out_pos, camera_height, *(void **)0x2ee6d0);
  }
}

/* FUN_001a1a10 (0x1a1a10)
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
int FUN_001a1a10(float scale, float *out_point, void *out_vec,
                 float *direction /* @eax */, int unit_handle /* @edi */)
{
  /* One contiguous collision-result buffer (frame base EBP-0x434). Sized to
   * cover collision_bsp_test_vector's full record (mirrors path.c's
   * local_438[264]); we read t at +0, a surface-record pointer at +4, and the
   * hit index at +8. Do NOT split into separate locals. */
  float collision_result[264];
  int result_index;
  int depth;
  void *bsp;
  float *offset_vec;
  float *surface_record;
  float t;
  float origin[3];
  float scaled_dir[3];
  float dir0;

  object_get_and_verify_type(unit_handle, 1);
  bsp = global_collision_bsp_get();
  result_index = -1;
  dir0 = direction[0];

  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x47a, true);
    system_exit(-1);
  }
  depth = *(int16_t *)0x4761d8;
  *(int16_t *)0x4761d8 = (int16_t)(depth + 1);
  *(int16_t *)(0x5a8c80 + depth * 2) = 7;

  object_get_world_position(unit_handle, (vector3_t *)origin);

  offset_vec = *(float **)0x31fc44;
  origin[0] = offset_vec[0] * *(float *)0x253524 + origin[0];
  origin[1] = offset_vec[1] * *(float *)0x253524 + origin[1];
  origin[2] = offset_vec[2] * *(float *)0x253524 + origin[2];

  scaled_dir[0] = scale * dir0;
  scaled_dir[1] = scale * direction[1];
  scaled_dir[2] = scale * direction[2];

  if ((char)collision_bsp_test_vector(1, (int)bsp, 0, 0, (int)origin,
                                      (int)scaled_dir, 3.4028235e+38f,
                                      collision_result) != 0) {
    t = collision_result[0];
    if (out_point != (float *)0) {
      out_point[0] = scaled_dir[0] * t + origin[0];
      out_point[1] = scaled_dir[1] * t + origin[1];
      out_point[2] = scaled_dir[2] * t + origin[2];
    }
    result_index = *(int *)((char *)collision_result + 8);
    if (out_vec != (void *)0) {
      surface_record = *(float **)((char *)collision_result + 4);
      ((int *)out_vec)[0] = ((int *)surface_record)[0];
      ((int *)out_vec)[1] = ((int *)surface_record)[1];
      ((int *)out_vec)[2] = ((int *)surface_record)[2];
    }
  }

  if (*(int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0x490, true);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 = (int16_t)(*(int16_t *)0x4761d8 - 1);
  return result_index;
}

/* biped_approximate_surface_index (0x1a1b90)
 *
 * Thin wrapper over the biped collision probe (FUN_001a1a10): casts a ray of
 * length 2.0 from the biped's world position along the global direction vector
 * at [0x31fc50], with no surface-vector output. The keystone's collision-result
 * index is returned unchanged in EAX (-1 = no hit). out_point, when non-NULL,
 * receives the hit point.
 *
 * Confirmed (disasm 0x1a1b90): keystone direction@<eax> = *(float**)0x31fc50;
 * unit_handle@<edi> = param_1; cdecl pushes scale=2.0f (0x40000000),
 * out_point=param_2, out_vec=NULL; ADD ESP,0xc. No MOV EAX after CALL, so the
 * keystone return flows through (caller 0x56c60 tests the result == -1).
 */
int biped_approximate_surface_index(int unit_handle, float *out_point)
{
  return FUN_001a1a10(2.0f, out_point, (void *)0, *(float **)0x31fc50,
                      unit_handle);
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
 * direction@<eax>=*(float**)0x31fc50, unit_handle@<edi>=unit_handle, push
 * scale=2.0f,&pos_buf,NULL. collision_surface helper arg counts from disasm
 * (test_point2d=5 args ret char, find_closest/project=6 args). Position vec3 at
 * EBP-0x14, 2D scratch at EBP-0x8. display_assert at :0x4e7 if pos==NULL.
 *
 * Unit field offsets (dword index in the disasm): 0x430=last_good_surface
 * (0x10c), 0x434=cached_surface_index (0x10d), 0x438=cached_position[3]
 * (0x10e/0x10f/0x110), 0x444=cache_timestamp (0x111), 0x448=last_result_surface
 * (0x112).
 */
int biped_find_pathfinding_surface_index(int unit_handle, vector3_t *pos)
{
  int *unit_obj;
  int tag;
  int game_time;
  void *bsp;
  float position[3]; /* EBP-0x14: snapshot/output position vec3 */
  float proj2d[2]; /* EBP-0x8: 2D projected scratch point */
  char projected;

  unit_obj = (int *)object_get_and_verify_type(unit_handle, 1);
  tag = (int)tag_get(0x62697064, *unit_obj);
  if (((*(unsigned char *)(tag + 0x2f4) & 4) != 0) &&
      ((*(unsigned char *)((char *)unit_obj + 0xb6) & 4) == 0)) {
    unit_obj[0x10d] = -1;
    object_get_world_position(unit_handle, pos);
  } else if ((unit_obj[0x10d] == -1) &&
             (game_time = game_time_get(), game_time > unit_obj[0x111])) {
    bsp = global_collision_bsp_get();
    position[0] = ((float *)(unit_obj + 0x10e))[0];
    position[1] = ((float *)(unit_obj + 0x10e))[1];
    position[2] = ((float *)(unit_obj + 0x10e))[2];
    unit_obj[0x111] = game_time;
    if (unit_obj[0x10c] != -1) {
      collision_surface_find_closest_point2d(
        (int)bsp, unit_obj[0x10c], 2, 1, (float *)(unit_obj + 0x10e), proj2d);
      collision_surface_project_point2d((int)bsp, unit_obj[0x10c], 2, 1, proj2d,
                                        position);
      unit_obj[0x10d] = unit_obj[0x10c];
    } else if ((unit_obj[0x112] != -1) &&
               (projected = (char)collision_surface_test_point2d(
                  (int)bsp, unit_obj[0x112], 2, 1, (float *)(unit_obj + 0x10e)),
                projected != 0)) {
      unit_obj[0x10d] = unit_obj[0x112];
      collision_surface_project_point2d((int)bsp, unit_obj[0x112], 2, 1,
                                        (float *)(unit_obj + 0x10e), position);
      unit_obj[0x10d] = unit_obj[0x112];
    }
    if (unit_obj[0x10d] == -1) {
      unit_obj[0x10d] = FUN_001a1a10(2.0f, position, (void *)0,
                                     *(float **)0x31fc50, unit_handle);
    }
    if (unit_obj[0x10d] != -1) {
      ((float *)(unit_obj + 0x10e))[0] = position[0];
      ((float *)(unit_obj + 0x10e))[1] = position[1];
      ((float *)(unit_obj + 0x10e))[2] = position[2];
      unit_obj[0x112] = unit_obj[0x10d];
    }
  }

  if (pos == (vector3_t *)0) {
    display_assert("pathfinding_point", "c:\\halo\\SOURCE\\units\\bipeds.c",
                   0x4e7, true);
    system_exit(-1);
  }
  ((int *)pos)[0] = unit_obj[0x10e];
  ((int *)pos)[1] = unit_obj[0x10f];
  ((int *)pos)[2] = unit_obj[0x110];
  return unit_obj[0x10d];
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
 * unit+0x30 = global up axis. FUN_0001aae0 gets vehicle center+radius.
 * 2.0f arg pushed as 0x40000000.
 */
void biped_exit_seat_end(int unit_handle, int seat_handle)
{
  char *unit_obj;
  float center[3];
  float radius;
  char fixed;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  tag_get(0x62697064, *(int *)unit_obj);

  *(int *)(unit_obj + 0x2c) = 0;
  if (normalize3d((float *)(unit_obj + 0x24)) == 0.0f) {
    *(float *)(unit_obj + 0x24) = global_forward_vector_ptr[0];
    *(float *)(unit_obj + 0x28) = global_forward_vector_ptr[1];
    *(float *)(unit_obj + 0x2c) = global_forward_vector_ptr[2];
  }
  *(float *)(unit_obj + 0x30) = global_up_vector_ptr[0];
  *(float *)(unit_obj + 0x34) = global_up_vector_ptr[1];
  *(float *)(unit_obj + 0x38) = global_up_vector_ptr[2];
  *(int *)(unit_obj + 0x424) |= 1;

  fixed =
    biped_fix_position(unit_handle, seat_handle, (float *)0, 0, 2.0f, 1, 0, 1);
  if (fixed == 0) {
    FUN_0001aae0(seat_handle, center, &radius);
    fixed =
      biped_fix_position(unit_handle, seat_handle, center, 0, radius, 1, 0, 0);
    if (fixed == 0) {
      error(2, "couldn't teleport the biped out far enough from the vehicle"
               "...");
    }
  }
}

/* FUN_001a1e70 (0x1a1e70)
 *
 * Per-tick "is this biped stuck falling / off a ledge?" check for AI bipeds.
 * After a battery of eligibility gates (not in a vehicle, tag flags clear,
 * not flying, has a valid path leader, not in scream state 0x1d, settle timer
 * past 0x1e, and a throttled recheck interval of game_time+0xf), it casts a
 * 6.0-length probe from the biped along the global direction at [0x31fc50].
 * If the probe misses, OR (after fetching the biped's world position) the
 * biped's clearance field (obj+0x20) is small and the vertical drop to the
 * probe point exceeds the physics fall threshold (physics+0x94 squared), it
 * triggers the recovery path FUN_001a74d0(unit_handle, 0).
 *
 * Confirmed (disasm 0x1a1e70): cdecl unit_handle at [EBP+8] (caller 0x1a6350
 * PUSH EDI; ADD ESP,4); object_get_and_verify_type(unit,1); tag_get('bipd');
 * game_time_get; tag_block_get_element(game_globals_get()+0x188, 0, 0x98);
 * keystone direction@<eax>=*(float**)0x31fc50, unit@<edi>, scale=6.0f
 * (0x40c00000); object_get_world_position; FUN_001a74d0(unit,0). Return
 * discarded (void).
 *
 * Inferred: obj+0x20 = clearance/height float; physics+0x94 = fall-distance
 * threshold; DAT_0032512c gravity-related scalar. Probe hit point and world
 * position are separate 3-float stack buffers.
 */
void FUN_001a1e70(int unit_handle)
{
  int *unit_obj;
  int tag;
  int game_time;
  int physics;
  float fall_term;
  float probe_hit[3];
  float world_pos[3];
  float *dir_ptr;

  unit_obj = (int *)object_get_and_verify_type(unit_handle, 1);
  tag = (int)tag_get(0x62697064, *unit_obj);
  if (((*(unsigned char *)((char *)unit_obj + 0xb6) & 4) == 0) &&
      ((*(unsigned char *)(tag + 0x2f4) & 0x84) == 0) &&
      ((unit_obj[0x6d] & 0x1000) == 0) && (unit_obj[0x69] != -1) &&
      (*(char *)((char *)unit_obj + 0x253) != 0x1d)) {
    game_time = game_time_get();
    if ((*(char *)((char *)unit_obj + 0x459) > 0x1e) &&
        ((unit_obj[0x114] == -1) || (unit_obj[0x114] + 0xf < game_time))) {
      physics =
        (int)tag_block_get_element((char *)game_globals_get() + 0x188, 0, 0x98);
      unit_obj[0x114] = game_time;
      dir_ptr = *(float **)0x31fc50;
      if ((FUN_001a1a10(6.0f, probe_hit, (void *)0, dir_ptr,
                        unit_handle) == -1) ||
          (object_get_world_position(unit_handle, (vector3_t *)world_pos),
           *(float *)((char *)unit_obj + 0x20) <= *(float *)0x2533c0 &&
             (fall_term = (world_pos[2] - probe_hit[2]) * *(float *)0x32512c,
              *(float *)(physics + 0x94) * *(float *)(physics + 0x94) <=
                *(float *)((char *)unit_obj + 0x20) *
                    *(float *)((char *)unit_obj + 0x20) +
                  fall_term + fall_term))) {
        FUN_001a74d0(unit_handle, 0);
      }
    }
  }
}

/* FUN_001a1fb0 (0x1a1fb0)
 *
 * Vehicle-rider variant of the stuck/ejection check (mirrors FUN_001a1e70 for a
 * biped riding a vehicle seat). Gates on the vehicle tag flag (vehi+0x17c bit
 * 6), the biped having a seat (obj+0x1a4 != -1), not in scream state 0x1d, a
 * recheck throttle field (obj+0x2d2 > 'x'), the vehicle settle counter
 * (vehi+0x428 > 0x1e), and a game_time+0xf interval on obj+0x450. It first
 * probes straight (8.0 length) along [0x31fc50]; if that misses it computes a
 * scaled velocity vector from the vehicle physics (phys+0x18/0x1c/0x20 times
 * DAT_2b4ee4, with a gravity bias on z), and if that vector's length is large
 * enough re-probes along it. Depending on which probe/threshold fails it logs
 * one of three eject reasons (0x26/0x27/0x28) via FUN_00046f10(reason,
 * unit_handle, -1,-1,-1,-1, 0).
 *
 * Confirmed (disasm 0x1a1fb0): register arg unit_handle@<eax> (MOV EDI,EAX at
 * 0x1a1fb9; caller 0x1a6350 MOV EAX,EDI; CALL with no stack arg). vehi =
 * object_get_and_verify_type(object_get_and_verify_type(unit,1)->[0xcc], 2);
 * tag_get('vehi'). keystone: 1st call direction@<eax>=*(float**)0x31fc50,
 * scale=8.0f, out_point=0, out_vec=0; 2nd call direction@<eax>=&scaled_vel,
 * scale=8.0f, out_point=0, out_vec=&probe_vec. unit@<edi> on both.
 * FUN_00046f10 args: 7 pushes (reason, EDI=unit, -1,-1,-1,-1, 0). Void.
 *
 * Inferred: DAT_2b4ee4 velocity scalar, DAT_2b4ee0/DAT_32512c gravity bias;
 * obj fields 0x1a4 seat, 0x253 scream, 0x2d2 throttle byte; vehi+0x428 settle;
 * obj+0x450 recheck timestamp. phys[6..8] = obj+0x18/0x1c/0x20 vehicle
 * velocity.
 */
void FUN_001a1fb0(int unit_handle /* @eax */)
{
  int *unit_obj;
  int *vehi_obj;
  int tag;
  int game_time;
  int probe_vec[3]; /* EBP-0x18: 2nd keystone out_vec */
  float scaled_vel[3]; /* EBP-0xc: scaled vehicle velocity */

  unit_obj = (int *)object_get_and_verify_type(unit_handle, 1);
  vehi_obj = (int *)object_get_and_verify_type(unit_obj[0x33], 2);
  tag = (int)tag_get(0x76656869, *vehi_obj);
  if (((*(unsigned char *)(tag + 0x17c) & 0x40) != 0) &&
      (unit_obj[0x69] != -1) && (*(char *)((char *)unit_obj + 0x253) != 0x1d) &&
      (*(char *)((char *)unit_obj + 0x2d2) > 0x78)) {
    game_time = game_time_get();
    if ((*(unsigned char *)((char *)vehi_obj + 0x428) > 0x1e) &&
        ((unit_obj[0x114] == -1) || (unit_obj[0x114] + 0xf < game_time))) {
      unit_obj[0x114] = game_time;
      if (FUN_001a1a10(8.0f, (float *)0, (void *)0, *(float **)0x31fc50,
                       unit_handle) == -1) {
        scaled_vel[0] =
          *(float *)((char *)vehi_obj + 0x18) * *(float *)0x2b4ee4;
        scaled_vel[1] =
          *(float *)((char *)vehi_obj + 0x1c) * *(float *)0x2b4ee4;
        scaled_vel[2] =
          *(float *)((char *)vehi_obj + 0x20) * *(float *)0x2b4ee4 -
          *(float *)0x32512c * *(float *)0x2b4ee0;
        if (normalize3d(scaled_vel) <= *(float *)0x2533c0) {
          FUN_00046f10(0x28, unit_handle, -1, -1, -1, -1, 0);
          return;
        }
        if ((FUN_001a1a10(8.0f, (float *)0, probe_vec, scaled_vel,
                          unit_handle) == -1) ||
            (*(float *)&probe_vec[2] <= *(float *)0x2533e4)) {
          FUN_00046f10(0x28, unit_handle, -1, -1, -1, -1, 0);
          return;
        }
      }
      if (*(float *)0x253f3c < *(float *)((char *)vehi_obj + 0x38)) {
        if (FUN_00012fe0((float *)(vehi_obj + 0xf)) < *(float *)0x26e2ec) {
          FUN_00046f10(0x26, unit_handle, -1, -1, -1, -1, 0);
          return;
        }
      }
      FUN_00046f10(0x27, unit_handle, -1, -1, -1, -1, 0);
    }
  }
}

/* FUN_001a2160 (0x1a2160)
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
void FUN_001a2160(int unit_handle)
{
  char *unit_obj;
  float axis[3];
  float up_rot[3];
  float t0;
  float t1;
  float t2;
  float angle;
  float cos_a;
  float sin_a;
  float *fwd;
  float *up_ptr;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);

  axis[0] = *(float *)(unit_obj + 0x3c);
  axis[1] = *(float *)(unit_obj + 0x40);
  axis[2] = *(float *)(unit_obj + 0x44);
  angle = normalize3d(axis);
  fwd = (float *)(unit_obj + 0x24);
#if defined(_MSC_VER) && !defined(__clang__)
  cos_a = (float)cos((double)angle);
  sin_a = (float)sin((double)angle);
#else
  cos_a = x87_fcos(angle);
  sin_a = x87_fsin(angle);
#endif
  rotate_vector3d_by_sincos(fwd, axis, sin_a, cos_a);
  normalize3d(fwd);

  up_ptr = (float *)(unit_obj + 0x30);
  up_rot[0] = up_ptr[0];
  up_rot[1] = up_ptr[1];
  up_rot[2] = up_ptr[2];
  rotate_vector3d_by_sincos(up_rot, axis, sin_a, cos_a);

  t0 = up_rot[2] * fwd[1] - up_rot[1] * fwd[2];
  t1 = up_rot[0] * fwd[2] - up_rot[2] * fwd[0];
  t2 = up_rot[1] * fwd[0] - up_rot[0] * fwd[1];
  /* up = cross(temp, fwd) — FPU LIFO: computed z,y,x, stored x,y,z */
  up_ptr[0] = t1 * fwd[2] - t2 * fwd[1];
  up_ptr[1] = t2 * fwd[0] - t0 * fwd[2];
  up_ptr[2] = t0 * fwd[1] - t1 * fwd[0];
  if (normalize3d(up_ptr) == 0.0f) {
    fwd[0] = global_forward_vector_ptr[0];
    fwd[1] = global_forward_vector_ptr[1];
    fwd[2] = global_forward_vector_ptr[2];
    up_ptr[0] = global_up_vector_ptr[0];
    up_ptr[1] = global_up_vector_ptr[1];
    up_ptr[2] = global_up_vector_ptr[2];
  }
}

/* FUN_001a2290 (0x1a2290)
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
char FUN_001a2290(int unit_handle)
{
  char *unit_obj;
  char *biped_tag;
  char *physics;
  float *vel_ptr;
  int actor_handle;
  int zero_idx;
  float max_speed;
  float vel[3];
  float *vel0_ptr;
  float dot;
  char success;
  char aim_flag;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

  if ((*(unsigned char *)(unit_obj + 0x424) & 1) != 0) {
    return 0;
  }
  zero_idx = 0;
  if (*(short *)(unit_obj + 0x460) == 1) {
    return 0;
  }

  max_speed = *(float *)(biped_tag + 0x3b4);
  vel0_ptr = &vel[0];
  success = 1;
  if (*(int *)(unit_obj + 0x1c8) != -1) {
    physics = (char *)tag_block_get_element((char *)game_globals_get() + 0x170,
                                            0, 0xf4);
    max_speed = (*(float *)0x2533c8 -
                 *(float *)(unit_obj + 0x3d4) * *(float *)(physics + 0x84)) *
                max_speed;
  }
  if (*(char *)0x5aa894 != '\0' && *(int *)(unit_obj + 0x1c8) != -1) {
    max_speed = max_speed * *(float *)0x2533d8;
  }

  vel_ptr = (float *)(unit_obj + 0x18);
  vel[0] = vel_ptr[0];
  vel[1] = vel_ptr[1];
  vel[2] = vel_ptr[2];
  dot = vel[1] * *(float *)(unit_obj + 0x34) +
        vel[2] * *(float *)(unit_obj + 0x38) +
        vel[zero_idx] * *(float *)(unit_obj + 0x30);
  if (dot < max_speed) {
    dot = max_speed - dot;
    vel[zero_idx] = dot * *(float *)(unit_obj + 0x30) + *vel0_ptr;
    vel[1] = dot * *(float *)(unit_obj + 0x34) + vel[1];
    vel[2] = dot * *(float *)(unit_obj + 0x38) + vel[2];
  }

  actor_handle = *(int *)(unit_obj + 0x1a8);
  if (actor_handle == -1) {
    actor_handle = *(int *)(unit_obj + 0x1a4);
  }
  if (actor_handle != -1) {
    aim_flag = (*(char *)(unit_obj + 0x253) == 0x27 ||
                *(char *)(unit_obj + 0x253) == 0x28) ?
                 1 :
                 0;
    success =
      (char)actor_aim_jump(actor_handle, unit_handle, aim_flag, max_speed, vel);
    if (success == zero_idx) {
      return success;
    }
  }

  vel_ptr[0] = vel[0];
  vel_ptr[1] = vel[1];
  vel_ptr[2] = vel[2];
  *(int *)(unit_obj + 0x424) |= 1;
  *(unsigned char *)(unit_obj + 0x45c) = zero_idx;
  *(int *)(unit_obj + 0x430) = -1;
  FUN_001a0f10(unit_handle, 4, 0);
  FUN_001a0f10(unit_handle, 4, 1);
  return success;
}

/* FUN_001a2440 (0x1a2440) — per-tick footstep / animation-marker event step
 *
 * Step in the biped update dispatcher (FUN_001a6350). Classifies the biped's
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
 * 0x25337c; FUN_001a0f10(unit, param_2, idx) idx routed to BX.
 */
void FUN_001a2440(int unit_handle /* @edi */)
{
  unsigned int *object;
  char *anim_elem;
  char is_walking;
  char is_fast;
  char matched_second;
  char counter;

  object = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
  tag_get(0x62697064, (int)object[0]); /* 'bipd' */

  is_walking = 0;
  is_fast = 0;
  switch (*(char *)((int)object + 0x253)) {
  case 2:
  case 3:
    is_walking = 1;
    break;
  case 4:
  case 5:
  case 6:
  case 7:
    if (*(float *)(object + 0x8a) * *(float *)(object + 0x8a) +
          *(float *)(object + 0x8b) * *(float *)(object + 0x8b) +
          *(float *)(object + 0x8c) * *(float *)(object + 0x8c) >
        *(float *)0x25337c) {
      is_fast = 1;
    }
    break;
  default:
    break;
  }

  if (*(short *)((int)object + 0x80) != -1) {
    anim_elem = (char *)tag_get(0x616e7472, (int)object[0x1f]); /* 'antr' */
    anim_elem = (char *)tag_block_get_element(
      anim_elem + 0x74, (int)*(short *)((int)object + 0x80), 0xb4);
    if (is_walking) {
      if (*(short *)((int)object + 0x82) == 0) {
        FUN_001a0f10(unit_handle, 3, 0);
        FUN_001a0f10(unit_handle, 3, 1);
      }
    } else if (is_fast && ((anim_elem[0x40] != 0) || (anim_elem[0x41] != 0)) &&
               ((*(short *)((int)object + 0x82) ==
                 (unsigned char)anim_elem[0x40]) ||
                (*(short *)((int)object + 0x82) ==
                 (unsigned char)anim_elem[0x41]))) {
      matched_second =
        (*(short *)((int)object + 0x82) != (unsigned char)anim_elem[0x40]);
      FUN_001a0f10(unit_handle, (*(char *)((int)object + 0x257) == 2),
                   matched_second);
    }
  }

  switch (*(char *)((int)object + 0x42a)) {
  case 0:
    if (*(char *)((int)object + 0x45b) < 1) {
      return;
    }
    counter = (char)(*(char *)((int)object + 0x45b) + 1);
    *(char *)((int)object + 0x45b) = counter;
    if (counter < 4) {
      return;
    }
    FUN_001a0f10(unit_handle, 3, 0);
    FUN_001a0f10(unit_handle, 3, 1);
    break;
  case 1:
    *(char *)((int)object + 0x45b) = 1;
    return;
  }
  *(char *)((int)object + 0x45b) = 0;
}

/* FUN_001a25e0 (0x1a25e0)
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
 * +y*ny, -d. Inferred: "find supporting surface" semantics from the stores into
 * unit+0x430/ 0x46c/0x30 and the min-distance selection.
 */
void FUN_001a25e0(int unit_handle /* @ecx */)
{
  int results[1028]; /* 0x1010 bytes: results[0]=count, results[1+i]=surface idx
                      */
  vector3_t cam_pos;
  vector3_t *cam_ptr;
  float height_offset;
  float camera_height;
  char *unit_obj;
  char *surface_data;
  void *bsp;
  int16_t depth;
  int count;
  int i;
  int idx2;
  int *surface_elem;
  float *plane;
  int plane_index;
  int best_index;
  float best_dist;
  float n[4];
  float best_n[4];
  float dist;
  float radius;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  bsp = global_collision_bsp_get();
  biped_get_camera_height_and_offset(unit_handle, &cam_pos, &height_offset,
                                     &camera_height);

  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("collision_user_depth<NUMBER_OF_COLLISION_USERS",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf71, true);
    system_exit(-1);
  }
  depth = *(int16_t *)0x4761d8;
  *(int16_t *)0x4761d8 = (int16_t)(depth + 1);
  *(int16_t *)(0x5a8c80 + depth * 2) = 7;

  cam_ptr = &cam_pos;
  surface_data = (char *)breakable_surfaces_get_bsp_surface_data();
  radius = camera_height + *(float *)0x2533e8;
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
          (void *)(surface_data + 0x3c), results[1 + i], 0xc);
        plane_index = *surface_elem;
        plane = (float *)tag_block_get_element((void *)(surface_data + 0xc),
                                               plane_index & 0x7fffffff, 0x10);
        if (plane_index < 0) {
          n[0] = -plane[0];
          n[1] = -plane[1];
          idx2 = 2;
          n[idx2] = -plane[idx2];
          n[3] = -plane[3];
        } else {
          n[0] = plane[0];
          n[1] = plane[1];
          n[idx2] = plane[idx2];
          n[3] = plane[3];
        }
        dist = (cam_pos.z * n[idx2] + n[0] * cam_pos.x + cam_pos.y * n[1]) -
               n[3];
        if (dist < best_dist) {
          best_index = results[1 + i];
          best_n[0] = n[0];
          best_n[1] = n[1];
          best_n[idx2] = n[idx2];
          best_n[3] = n[3];
          best_dist = dist;
        }
        i = (int)(int16_t)(i + 1);
      } while (i < count);

      if (best_index != -1) {
        *(int *)(unit_obj + 0x430) = best_index;
        *(float *)(unit_obj + 0x46c) = best_n[0];
        *(float *)(unit_obj + 0x470) = best_n[1];
        *(float *)(unit_obj + 0x474) = best_n[idx2];
        *(float *)(unit_obj + 0x478) = best_n[3];
        *(float *)(unit_obj + 0x30) = best_n[0];
        *(float *)(unit_obj + 0x34) = best_n[1];
        *(float *)(unit_obj + 0x38) = best_n[idx2];
      }
    }
  }

  if (*(int16_t *)0x4761d8 <= 1) {
    display_assert("collision_user_depth>0",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xf94, true);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 = (int16_t)(*(int16_t *)0x4761d8 - 1);
}

/* FUN_001a2800 (0x1a2800)
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
void FUN_001a2800(int unit_handle /* @eax */, const char *failure_kind)
{
  char *unit_obj;
  char *biped_tag;
  const char *limp_str;
  const char *dead_str;
  const char *mode_str;
  uint32_t flags;

  unit_obj = (char *)object_get_and_verify_type(unit_handle, 1);
  if (valid_real_normal3d_perpendicular((float *)(unit_obj + 0x24),
                                        (float *)(unit_obj + 0x30)) == 0) {
    biped_tag = (char *)tag_get(0x62697064, *(int *)unit_obj);

    limp_str = "/limping";
    if ((*(unsigned char *)(unit_obj + 0x424) & 0x20) == 0) {
      limp_str = "";
    }
    dead_str = "/dead";
    if ((*(unsigned char *)(unit_obj + 0xb6) & 4) == 0) {
      dead_str = "";
    }
    flags = *(uint32_t *)(biped_tag + 0x2f4);
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
      tag_name_strip_path(tag_get_name(*(int *)unit_obj)), mode_str, dead_str,
      limp_str, failure_kind, (double)*(float *)(unit_obj + 0x24),
      (double)*(float *)(unit_obj + 0x28), (double)*(float *)(unit_obj + 0x2c),
      (double)*(float *)(unit_obj + 0x30), (double)*(float *)(unit_obj + 0x34),
      (double)*(float *)(unit_obj + 0x38));
    display_assert((const char *)0x5ab100, "c:\\halo\\SOURCE\\units\\bipeds.c",
                   0x55d, true);
    system_exit(-1);
  }
}

/* FUN_001a2900 (0x1a2900) — post-airborne update step
 *
 * Step in the biped update dispatcher (FUN_001a6350), reached when the biped
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
 *     (vector3d_scale_add), then runs the airborne aim helper FUN_001a2160.
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
 * world_up, tmp); normalize3d (0x13010); FUN_000121e0(0, 2pi) random yaw;
 * vector3d_from_angle (0x10cc70); vector3d_scale_add (0x12f80) into
 * object+0x3c.
 */
void FUN_001a2900(int unit_handle, char *state)
{
  unsigned int *object;
  int biped_tag;
  char aim_control;
  char move_state;
  float pitch;
  float facing[3];

  object = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (int)tag_get(0x62697064, (int)object[0]); /* 'bipd' */

  aim_control = 0;
  if ((biped_flying_through_air(unit_handle) != 0) &&
      (aim_control = 1, (*(unsigned int *)(biped_tag + 0x2f4) & 0x100) != 0)) {
    move_state = *(char *)((int)object + 0x253);
    if ((move_state != 0x1f) && (move_state != 0x29)) {
      pitch = random_real_range(get_global_random_seed_address(), 0.05235988f,
                                0.08726646f);
      if (*(float *)(object + 0xe) /* +0x38 */ >= *(float *)0x2533f0) {
        cross_product3d((float *)(object + 0xc) /* +0x30 */,
                        *(float **)0x31fc44 /* world up */, facing);
        if (normalize3d(facing) > *(float *)0x2533c0) {
          goto have_facing;
        }
      }
      vector3d_from_angle(facing, FUN_000121e0(0.0f, 6.2831855f));
    have_facing:
      vector3d_scale_add((float *)(object + 0xf) /* +0x3c */, facing, pitch,
                         (float *)(object + 0xf));
    }
    FUN_001a2160(unit_handle);
  }

  if ((*(unsigned char *)(object + 0x109) /* +0x424 */ & 1) == 0) {
    display_assert("TEST_FLAG(biped->biped.flags, _biped_airborne_bit)",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xa61, true);
    system_exit(-1);
  }

  move_state = *(char *)((int)object + 0x253);
  if ((move_state == 0x27) || (move_state == 0x28)) {
    *state = 0x28;
  } else if ((move_state == 0x14) || (aim_control)) {
    *state = 0x14;
  }

  FUN_001a2800(unit_handle, "post-airborne");
}

/* FUN_001a2a60 (0x1a2a60) — post-landing update step
 *
 * Step in the biped update dispatcher (FUN_001a6350), reached when the biped
 * has a landing-animation index (object+0x460 != NONE). Increments the
 * landing-frame counter (object+0x428); once it reaches the landing frame
 * count (object+0x429) the landing animation index (object+0x460) is cleared
 * to NONE (0xffff). When no cinematic is running and the landing has just
 * begun (counter == 2) or is effectively a one-frame landing (index already
 * cleared and frame count < 2), fires two collision-user events via
 * FUN_001a0f10 with selector indices 0 then 1 (param_2 = 5). Writes the
 * resulting landing-sound id into *state (0x16 if index == 1, else 0x15) and
 * emits the "post-landing" timing marker via FUN_001a2800.
 *
 * unit_handle arrives in EDI (register parameter); state is the only stack
 * argument (caller pushes &update_state byte).
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...);
 * cinematic_in_progress (0x930a0); FUN_001a0f10(unit, 5, idx) idx->BX (0,1).
 */
void FUN_001a2a60(int unit_handle /* @edi */, char *state)
{
  unsigned char *object;
  char counter;

  object = (unsigned char *)object_get_and_verify_type(unit_handle, 1);
  tag_get(0x62697064, *(int *)object); /* 'bipd' */

  counter = (char)(object[0x428] + 1);
  object[0x428] = (unsigned char)counter;
  if (counter >= (char)object[0x429]) {
    *(short *)(object + 0x460) = -1;
  }

  if ((cinematic_in_progress() == 0) &&
      (((char)object[0x428] == 2) ||
       ((*(short *)(object + 0x460) == -1) && ((char)object[0x429] < 2)))) {
    FUN_001a0f10(unit_handle, 5, 0);
    FUN_001a0f10(unit_handle, 5, 1);
  }

  *state = (char)((*(short *)(object + 0x460) == 1) + 0x15);
  FUN_001a2800(unit_handle, "post-landing");
}

/* FUN_001a2b10 (0x1a2b10) — post-slipping update step
 *
 * Step in the biped update dispatcher (FUN_001a6350). If the biped's slipping
 * counter (object+0x45a) has exceeded 3 ticks AND the object's linear velocity
 * magnitude squared (object+0x18..0x20) exceeds the "moving" threshold at
 * 0x25620c (== 1/900), fires two collision-user events via FUN_001a0f10 with
 * selector indices 0 then 1 (param_2 = 2). Always emits the "post-slipping"
 * timing marker via FUN_001a2800.
 *
 * unit_handle arrives in EDI (register parameter). The caller (FUN_001a6350)
 * also pushes a pointer to its update-state byte buffer, but this function
 * never reads it; it is not declared as a parameter because the original is
 * frameless (no EBP frame, no stack-arg load) — the caller's cdecl push and
 * matching ESP cleanup are unaffected.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...);
 * velocity sum-of-squares at +0x18/+0x1c/+0x20 vs threshold 0x25620c;
 * FUN_001a0f10(unit, 2, idx) with idx routed to BX (0 then 1).
 */
void FUN_001a2b10(int unit_handle /* @edi */)
{
  unsigned int *object;
  float *velocity;

  object = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
  tag_get(0x62697064, (int)object[0]); /* 'bipd' */

  velocity = (float *)(object + 6); /* +0x18: linear velocity vec3 */
  if ((*(char *)((int)object + 0x45a) > 3) &&
      (velocity[2] * velocity[2] + velocity[1] * velocity[1] +
         velocity[0] * velocity[0] >
       *(float *)0x25620c)) {
    FUN_001a0f10(unit_handle, 2, 0);
    FUN_001a0f10(unit_handle, 2, 1);
  }

  FUN_001a2800(unit_handle, "post-slipping");
}

/* FUN_001a2b90 (0x1a2b90) — airborne aim / landing-rumble update step
 *
 * Step in the biped update dispatcher (FUN_001a5300). When the biped is not
 * yet airborne (object+0x424 bit 0 clear) and its landing index (object+0x460)
 * isn't 1, advances the airborne-frame counter (object+0x45c, capped at 0x7f);
 * if the biped has the relevant control flag (object+0x1b8 bit 1) and the
 * counter exceeds 5, runs FUN_001a2290.
 *
 * If a controlling unit (player) is bound (global 0x5aa891 set, object+0x1c8 is
 * a valid datum) it fetches the player record (datum_get) and, depending on
 * object+0x1b8 flags:
 *   - bits 0x800 AND 0x2000 set: re-orients the biped's aim vector
 *     (object+0x18). Computes the dot of the aim (object+0x18) with the desired
 *     aim (object+0x1ec); when that dot is below epsilon (0x2533c0) it derives
 *     an angle via FUN_00013070, otherwise uses epsilon. A blend weight is
 *     formed (0x2b4fbc * angle) and saturated to [0,1]. The aim is rotated
 *     toward the desired aim via three vector3d_scale_add steps and the
 *     airborne flag (object+0x424 bit 0) is set.
 *   - only bit 1 (and already airborne): damps the aim vector by -0.2
 *     (FUN_00012fb0).
 * Finally, if the aim was re-oriented this tick and the player has a valid
 * controller index (player+2), triggers a landing rumble (rumble_player_impulse
 * with a 0x3c-byte rumble definition: scale 1.0, 0.3, type 3).
 *
 * unit_handle arrives in EAX (register parameter). The caller also pushes its
 * update-state pointer, but this function never reads it.
 *
 * Confirmed: object_get_and_verify_type(unit_handle, 1); tag_get('bipd',...);
 * FUN_001a2290 (@edi=unit_handle); datum_get(0x5aa6d4, object+0x1c8);
 * FUN_00013070 dot/angle of object+0x18 and object+0x1ec; vector3d_scale_add
 * (0x12f80) x3; FUN_00012fb0 damp; csmemset+rumble_player_impulse (0xb9bc0).
 */
void FUN_001a2b90(int unit_handle /* @eax */)
{
  unsigned int *object;
  int player;
  unsigned int flags;
  char aim_reoriented;
  float angle;
  float weight;
  float scale;
  float tmp[3];
  float rumble_def[15]; /* 0x3c bytes */

  object = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
  tag_get(0x62697064, (int)object[0]); /* 'bipd' */

  if (((*(unsigned char *)((int)object + 0x424) & 1) == 0) &&
      (*(short *)((int)object + 0x460) != 1)) {
    if (*(char *)((int)object + 0x45c) < 0x7f) {
      *(char *)((int)object + 0x45c) =
        (char)(*(char *)((int)object + 0x45c) + 1);
    }
    if (((*(unsigned char *)((int)object + 0x1b8) & 2) != 0) &&
        (*(char *)((int)object + 0x45c) > 5)) {
      FUN_001a2290(unit_handle);
    }
  }

  if ((*(char *)0x5aa891 != 0) && ((int)object[0x72] != -1)) {
    player = (int)datum_get(*(void **)0x5aa6d4, (int)object[0x72]);
    flags = object[0x6e]; /* +0x1b8 */
    aim_reoriented = 0;
    if (((flags & 0x800) == 0) || ((flags & 0x2000) == 0)) {
      if (((flags & 1) != 0) &&
          ((*(unsigned char *)((int)object + 0x424) & 1) != 0)) {
        FUN_00012fb0((float *)(object + 6), -0.2f, (float *)(object + 6));
      }
    } else {
      /* Dot of aim (object+0x18) with desired aim (object+0x1ec); z,y,x order
       * matches the original FLD sequence. The FLD/FMUL operand assignment for
       * each commutative product is an MSVC scheduling choice (a*b == b*a, so
       * bit-identical); vc71 flags it as an FPU operand-order diff but the
       * value is provably the same. */
      if (*(float *)(object + 8) * *(float *)(object + 0x7d) +
            *(float *)(object + 7) * *(float *)(object + 0x7c) +
            *(float *)(object + 6) * *(float *)(object + 0x7b) <
          *(float *)0x2533c0) {
        angle = FUN_00013070((float *)(object + 6), (float *)(object + 0x7b));
      } else {
        angle = *(float *)0x2533c0;
      }
      weight = *(float *)0x2b4fbc * angle;
      if (weight < *(float *)0x2533c0) {
        scale = 0.0f; /* clamp below epsilon -> 0 */
      } else if (weight > *(float *)0x2533c8) {
        scale = 1.0f; /* clamp above 1.0 -> 1.0 */
      } else {
        scale = weight;
      }
      vector3d_scale_add((float *)(object + 6), (float *)(object + 0x7b),
                         -angle, tmp);
      vector3d_scale_add((float *)(object + 6), tmp, -0.2f,
                         (float *)(object + 6));
      vector3d_scale_add((float *)(object + 6), (float *)(object + 0x7b),
                         scale * *(float *)0x25bb0c -
                           scale * scale * *(float *)0x2533e8 +
                           *(float *)0x25bb10,
                         (float *)(object + 6));
      object[0x109] |= 1; /* +0x424 bit 0 */
      aim_reoriented = 1;
    }
    if ((*(short *)(player + 2) != -1) && (aim_reoriented)) {
      csmemset(rumble_def, 0, 0x3c);
      *(unsigned int *)&rumble_def[0] = 0x3f800000; /* 1.0 */
      *(unsigned int *)&rumble_def[1] = 0x3e99999a; /* 0.3 */
      *(short *)&rumble_def[2] = 3;
      rumble_player_impulse(*(short *)(player + 2), rumble_def, 1.0f, 1.0f);
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
void biped_build_flying_axes(float *forward, float *left, float *up)
{
  float lc0;
  float lc1;
  float lc2;

  if (forward == NULL || left == NULL || up == NULL) {
    display_assert("forward_vector && left_vector && up_vector",
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xb93, true);
    system_exit(-1);
  }

  up[0] = global_up_vector_ptr[0];
  up[1] = global_up_vector_ptr[1];
  up[2] = global_up_vector_ptr[2];

  /* left = up x forward (batched: all components before any store) */
  lc2 = forward[1] * up[0] - up[1] * forward[0];
  lc1 = up[2] * forward[0] - forward[2] * up[0];
  lc0 = up[1] * forward[2] - up[2] * forward[1];
  left[0] = lc0;
  left[1] = lc1;
  left[2] = lc2;
  if (normalize3d(left) == 0.0f) {
    up[0] = global_forward_vector_ptr[0];
    up[1] = global_forward_vector_ptr[1];
    up[2] = global_forward_vector_ptr[2];
    lc2 = forward[1] * up[0] - up[1] * forward[0];
    lc1 = up[2] * forward[0] - forward[2] * up[0];
    lc0 = up[1] * forward[2] - up[2] * forward[1];
    left[0] = lc0;
    left[1] = lc1;
    left[2] = lc2;
    normalize3d(left);
  }

  /* up = forward x left */
  lc2 = left[1] * forward[0] - forward[1] * left[0];
  lc1 = forward[2] * left[0] - forward[0] * left[2];
  lc0 = forward[1] * left[2] - left[1] * forward[2];
  up[0] = lc0;
  up[1] = lc1;
  up[2] = lc2;
  normalize3d(up);

  if (valid_real_vector3d_axes3(forward, left, up) == 0) {
    display_assert(
      csprintf(error_string_buffer,
               "%s, %s, %s: assert_valid_real_vector3d_axes3(%f, %f, %f / %f, "
               "%f, %f / %f, %f, %f)",
               "forward_vector", "left_vector", "up_vector", (double)forward[0],
               (double)forward[1], (double)forward[2], (double)up[0],
               (double)up[1], (double)up[2], (double)left[0], (double)left[1],
               (double)left[2]),
      "c:\\halo\\SOURCE\\units\\bipeds.c", 0xba2, true);
    system_exit(-1);
  }
}

/* Collision/line-of-sight query result entry (0x2c bytes), as filled by
 * FUN_00150550 into a 16-element local array. Offsets confirmed from the
 * back-half access patterns (EDI walks the array at stride 0x2c):
 *   +0x00 point[3] (also used as plane normal source)
 *   +0x04 normal.y (EDI+0x4, used in the ground-plane height test)
 *   +0x0c object datum handle (EDI+0xc, compared to -1, fed to datum_get)
 *   +0x14 flags byte (EDI+0x14, tested &8 / &4) and +0x28 plane reference.
 * The decompiler models the array as scattered local_358/local_35c/local_36c
 * char* locals at stride 0xb; this struct preserves the same 0x2c layout. */
typedef struct biped_collision_result {
  float point[4]; /* +0x00 .. +0x0c */
  float normal[3]; /* +0x10 .. +0x18 */
  float plane_d; /* +0x1c */
  int object_handle; /* +0x20 */
  int surface_handle; /* +0x24 */
  int flags; /* +0x28 */
} biped_collision_result;

/* FUN_001a2f40 (0x1a2f40) — biped ground/movement physics step.
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
 *   prologue; caller FUN_001a5300 does `LEA ESI,[EBP-0xe4]; CALL` with no
 *   pushes and discards EAX). Returns void. Huge _chkstk frame (0xafac) for a
 *   ~44 KB debug-draw scratch buffer passed to FUN_0014ec30.
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
 *   +0x48  friction (used as 1.0 - friction; 1.0f = *(float*)0x2533c8)
 *   +0x4c  max-speed clamp (mode 0x10, physics[0x13])
 *   +0x50  max-speed clamp (mode 0x01, physics[0x14])
 *   +0x54/0x58  line-of-sight direction/origin args to FUN_00150550
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
void FUN_001a2f40(void *physics_arg /* @esi */)
{
  float *physics;
  float *velocity; /* physics + 0xb (EBX = ESI+0x2c) */
  float *position; /* physics + 2  (EDI = ESI+0x8)  */
  unsigned short flags;
  unsigned int flags_word; /* full flags promoted [EBP-0x48] */
  float damp; /* 1.0 - friction (physics[0x12]) */
  float length3;
  float clamp;

  /* --- back-half locals --- */
  char debug_scratch[44040]; /* local_afb0: ~44 KB FUN_0014ec30 buffer (forces
                              * the _chkstk prologue the original uses) */
  char surface_result[44]; /* local_3a8: FUN_0014c4b0 out-result */
  biped_collision_result results[16]; /* local_378 base; FUN_00150550 array */
  int result_count; /* local_40: u16 count from FUN_00150550 */
  int draw_color; /* pcVar10: debug-draw colour code */
  float gx, gy; /* local_5c, local_60: carried planar normal */
  float pos_world[3]; /* local_34/30: position copy */
  float new_pos[3]; /* local_9c/98/94: new_position+step */
  float los_dir[3]; /* local_7c/78/74: line-of-sight out point */
  float surf[3]; /* local_6c/68/64: projected surface point */
  float disp[3]; /* local_58/54/50: clamp scratch vec3 */
  float best_t; /* local_4c */
  int best_index; /* local_3c */
  int tval;
  float r, t, fdist;
  void *obj;
  void *tag;
  float proj_seg[3]; /* scale_add scratch in the LOS gate */
  float los_dir2[3]; /* local_18: second LOS out point */
  unsigned int isnan_tmp;
  char material_local; /* [EBP-0x31]: (flags>>9)&1 material flag */

  physics = (float *)physics_arg;
  velocity = physics + 0xb; /* +0x2c */
  position = physics + 2; /* +0x08 */

  gx = 0.0f; /* local_5c = 0.0 (prologue) */
  gy = 0.0f; /* local_60 = 0.0 (prologue) */
  material_local = (char)((*(unsigned short *)((char *)physics + 4) >> 9) & 1);

  /* assert_valid_real_point3d(&physics->position) */
  if (valid_real_point3d(position) == 0) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_point3d(%f, %f, %f)",
                            "&physics->position", (double)position[0],
                            (double)position[1], (double)position[2]),
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xbae, true);
    system_exit(-1);
  }
  /* assert_valid_real_vector2d(&physics->velocity) */
  if (real_vector3d_valid(velocity) == 0) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_vector2d(%f, %f, %f)",
                            "&physics->velocity", (double)velocity[0],
                            (double)velocity[1], (double)velocity[2]),
                   "c:\\halo\\SOURCE\\units\\bipeds.c", 0xbaf, true);
    system_exit(-1);
  }

  flags = *(unsigned short *)((char *)physics + 4);
  flags_word = flags;
  *(unsigned short *)((char *)physics + 0xa0) = 0;

  if ((flags & 0x10) != 0) {
    /* ---- mode 0x10 (0x1a3037..0x1a3149): oriented/flying movement ----
     * The original double-stores the displacement into a raw copy (local_30..)
     * and a unit copy (local_54..); normalize3d destroys the unit copy in
     * place; the clamp branch scales the unit, the no-clamp branch reads back
     * the raw. Modelling that as two vec3s reproduces the movl/movl pairs. */
    float left[3];
    float up[3];
    float t0, t1, t2;
    float dx, dy, dz;

    biped_build_flying_axes(physics + 5, left, up);

    t0 = left[0] * physics[0x10] + up[0] * physics[0x11] +
         physics[0xf] * physics[5];
    t1 = physics[6] * physics[0xf] + left[1] * physics[0x10] +
         up[1] * physics[0x11];
    t2 = physics[7] * physics[0xf] + left[2] * physics[0x10] +
         up[2] * physics[0x11];

    damp = *(float *)0x2533c8 - physics[0x12];

    dx = damp * t0 - velocity[0];
    dy = damp * t1 - physics[0xc];
    dz = damp * t2 - physics[0xd];

    disp[0] = dx;
    disp[1] = dy;
    disp[2] = dz;
    length3 = normalize3d(disp);
    if (length3 > physics[0x13]) {
      clamp = physics[0x13];
      dx = disp[0] * clamp;
      dy = disp[1] * clamp;
      dz = disp[2] * clamp;
    }
    physics[0x2e] = dx + velocity[0];
    physics[0x2f] = dy + physics[0xc];
    physics[0x30] = dz + physics[0xd];
    *(unsigned short *)((char *)physics + 0xa0) =
      (unsigned short)((*(unsigned short *)((char *)physics + 0xa0) & 0xfffd) |
                       1);
    goto LAB_001a36a4;
  }

  if ((flags & 0x20) != 0) {
    /* ---- mode 0x20 (0x1a3152..0x1a318b): planar heading carry ----
     * new_velocity.xy = rotate(control.xy) by the forward axis; z carried as a
     * raw 32-bit copy. gx/gy double-store to both new_velocity and the carried
     * planar normal (fsts + fstps). */
    gx = physics[0x2e] = physics[0xf] * physics[5] - physics[6] * physics[0x10];
    *(int *)((char *)physics + 0xc0) =
      *(int *)&physics[0x11]; /* +0xc0 int mov */
    gy = physics[0x2f] = physics[0xf] * physics[6] + physics[0x10] * physics[5];
    goto LAB_001a36a4;
  }

  if ((flags & 1) != 0) {
    /* ---- mode 0x01 (0x1a3198..0x1a3264): ground tangent movement ----
     * The original computes both planar cross-products, THEN damp = 1-friction,
     * duplicates damp (fld st0) and multiplies it into each before subtracting
     * velocity. Compute the crosses into temps first to match that order. */
    float tang[3];
    float c0, c1;

    c0 = physics[0xf] * physics[5] - physics[6] * physics[0x10];
    c1 = physics[0xf] * physics[6] + physics[0x10] * physics[5];
    damp = *(float *)0x2533c8 - physics[0x12];

    tang[0] = damp * c0 - velocity[0];
    tang[1] = damp * c1 - physics[0xc];
    /* tang[2] (local_3c) is read stale by magnitude3d in the original; fixed
     * to 0 here. Only tang[0]/tang[1] feed outputs, so this affects only the
     * clamp's length test, never a stored field. */
    tang[2] = 0.0f;

    length3 = magnitude3d(tang);
    if (length3 > physics[0x14]) {
      clamp = physics[0x14];
      tang[0] = tang[0] * clamp;
      tang[1] = tang[1] * clamp;
    }
    physics[0x2e] = tang[0] + velocity[0];
    physics[0x2f] = tang[1] + physics[0xc];
    physics[0x30] = physics[0xd] - *(float *)0x32512c;
    *(unsigned short *)((char *)physics + 0xa0) =
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
    float vecA[3]; /* local_30/2c/28 */
    float vecB[3]; /* local_24/20/1c */
    float d[3]; /* local_18/14/10: must be contiguous for normalize3d(&d[0]) */
    float curve_scale; /* fVar1: slope-response result */
    float damp2;
    char curve_flag; /* local_1: secondary state byte */
    float *gp; /* EDI = physics + 0x20 ground-plane normal */

    magnitude =
      sqrtf(physics[0xf] * physics[0xf] + physics[0x10] * physics[0x10] +
           physics[0x11] * physics[0x11]); /* 0x1a327d..0x1a328f */
    submode = (short)(flags_word & 0x200);

    if (submode != 0) {
      /* ---- 0x1a329e: in-plane axis recovery via cross products ---- */
      gp = physics + 0x20;
      vecB[0] = physics[8];
      vecB[1] = physics[9];
      vecB[2] = physics[10];
      cross_product3d(gp, vecB, vecA);
      if (normalize3d(vecA) == *(float *)0x2533c0) {
        cross_product3d(gp, (float *)(*(int *)0x31fc44), vecA);
        if (normalize3d(vecA) == *(float *)0x2533c0) {
          cross_product3d(gp, (float *)(*(int *)0x31fc3c), vecA);
          normalize3d(vecA);
        }
      }
      cross_product3d(vecA, gp, vecB);
      normalize3d(vecB);
      d[0] = vecB[0] * physics[0xf] + vecA[0] * physics[0x10];
      d[1] = vecB[1] * physics[0xf] + vecA[1] * physics[0x10];
      d[2] = vecB[2] * physics[0xf] + vecA[2] * physics[0x10] +
             physics[0x11];
      normalize3d(d); /* submode normalizes here, JMP 0x34f6 skips below */
    } else {
      /* else-if and else share the normalize at 0x34ea; only else has
       * the material multiply. */
      if (physics[0x22] <= *(float *)0x253f44) {
        /* ---- 0x1a339b: ground normal nearly horizontal ---- */
        gp = physics + 0x20;
        d[0] =
          physics[0xf] * physics[5] - physics[6] * physics[0x10];
        gx = d[0];
        d[1] =
          physics[0x10] * physics[5] + physics[0xf] * physics[6];
        gy = d[1];
        d[2] = (d[1] * physics[0x21] + d[0] * gp[0]) / physics[0x22];
        d[2] = physics[0x11] - d[2];
      } else {
        /* ---- 0x1a33ed: full ground-plane projection ---- */
        gp = physics + 0x20;
        vecB[0] = physics[8];
        vecB[1] = physics[9];
        vecB[2] = physics[10];
        cross_product3d((float *)(*(int *)0x31fc44), vecB, vecA);
        normalize3d(vecA);
        vector3d_scale_add(vecB, gp,
                           -(vecB[0] * gp[0] + vecB[1] * gp[1] + vecB[2] * gp[2]),
                           vecB);
        vector3d_scale_add(vecA, gp,
                           -(vecA[0] * gp[0] + vecA[1] * gp[1] + vecA[2] * gp[2]),
                           vecA);
        gx = physics[0xf] * physics[5] - physics[6] * physics[0x10];
        gy = physics[0x10] * physics[5] + physics[0xf] * physics[6];
        d[0] = vecB[0] * physics[0xf] + vecA[0] * physics[0x10];
        d[1] = vecB[1] * physics[0xf] + vecA[1] * physics[0x10];
        d[2] = vecB[2] * physics[0xf] + vecA[2] * physics[0x10] +
               physics[0x11];
        /* material gate: multiply d[2] when material_local == 0 */
        if (material_local == 0) {
          d[2] = d[2] * *(float *)0x254cc4;
        }
      }
      normalize3d(d); /* shared normalize at 0x34ea */
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
      } else if (!(d[2] > physics[0x1b])) {
        curve_scale = magnitude * physics[0x1c];
      } else if (d[2] < physics[0x1a]) {
        curve_scale =
          ((physics[0x1c] - *(float *)0x2533c8) * (d[2] - physics[0x1a]) /
             (physics[0x1b] - physics[0x1a]) +
           *(float *)0x2533c8) *
          magnitude;
      } else if (d[2] < physics[0x1e]) {
        if (d[2] <= physics[0x1d]) {
          curve_scale = magnitude;
        } else {
          curve_scale =
            ((physics[0x1f] - *(float *)0x2533c8) * (d[2] - physics[0x1d]) /
               (physics[0x1e] - physics[0x1d]) +
             *(float *)0x2533c8) *
            magnitude;
        }
      } else {
        curve_scale = magnitude * physics[0x1f];
      }

      damp2 = (*(float *)0x2533c8 - physics[0x12]) * curve_scale;
      disp[0] = d[0] * damp2 - velocity[0];
      disp[1] = d[1] * damp2 - physics[0xc];
      disp[2] = d[2] * damp2 - physics[0xd];
      length3 = normalize3d(disp); /* 0x1a35e2; ST0 -> compare */
      if (length3 < physics[0x13] || length3 == physics[0x13]) {
        /* 0x1a35f2 JNZ 0x1a361f: keep raw disp */
        disp[0] = disp[0];
        disp[1] = disp[1];
        disp[2] = disp[2];
        curve_flag = 0;
      } else {
        if (submode == 0) {
          curve_flag =
            (char)(((unsigned char)flags_word >> 1) & 1); /* 0x1a35fb */
        }
        clamp = physics[0x13];
        disp[0] = disp[0] * clamp; /* 0x1a360a */
        disp[1] = disp[1] * clamp;
        disp[2] = disp[2] * clamp;
      }

      /* new_velocity = disp - ground_normal*0x25f0d0 + velocity (0x1a3637..) */
      gp = physics + 0x20;
      r = physics[0x20] * *(float *)0x25f0d0; /* gp[0] */
      t = physics[0x21] * *(float *)0x25f0d0;
      fdist = physics[0x22] * *(float *)0x25f0d0;
      *(unsigned short *)((char *)physics + 0xa0) =
        (unsigned short)(-(unsigned short)(curve_flag != 0) & 2); /* 0x1a364d */
      physics[0x2e] = (disp[0] - r) + velocity[0]; /* 0x1a3672 */
      physics[0x2f] = (disp[1] - t) + physics[0xc]; /* 0x1a3676 */
      physics[0x30] = (disp[2] - fdist) + physics[0xd]; /* 0x1a3681 */
      if ((*(unsigned char *)((char *)physics + 0xa0) & 2) == 0) {
        goto LAB_001a36a4;
      }
      physics[0x30] =
        physics[0x30] - *(float *)0x32512c; /* 0x1a3692 step-down */
    }
  }

LAB_001a36a4:
  /* ---- debug-draw colour select (0x1a36a4..0x1a36d2) ---- */
  flags = *(unsigned short *)((char *)physics + 4);
  if ((flags & 0x40) != 0) {
    draw_color = 0;
  } else if ((char)flags < 0) {
    draw_color = 0xc0a0;
  } else {
    draw_color =
      (int)((-(unsigned int)((flags & 0x100) != 0) & 0xffdfff00) + 0x20c3a0);
  }

  /* position copy (local_34/30/2c) and ray endpoint (local_9c/98/94). The
   * endpoint z carries the step-height offset physics[0xe]. (0x1a36d2..) */
  pos_world[0] = physics[2];
  pos_world[1] = physics[3];
  pos_world[2] = physics[4];
  new_pos[0] = physics[0x2e];
  new_pos[1] = physics[0x2f];
  new_pos[2] = physics[0x30] + physics[0xe];

  /* ---- collision/line-of-sight query (0x1a36d2..0x1a3803) ---- */
  if (*(char *)0x4e4cf2 == '\0') {
    /* live path: 10-arg query. Arg order matches the push sequence at
     * 0x1a37ce..0x1a37f6 (last push = first C arg):
     *   draw_color, &pos_world, &new_pos, physics[0x15], physics[0x16],
     *   physics[0], &los_dir2, &los_dir, 0x10, results. */
    result_count = FUN_00150550(
      (void *)draw_color, pos_world, new_pos, *(int *)&physics[0x15],
      *(int *)&physics[0x16], (int)physics[0], &los_dir2[0], &los_dir[0], 0x10,
      results);
  } else {
    /* debug-draw line record (0x1a3721..0x1a37cc): no query runs, count stays
     * 1. Builds results[0] (point/normal/plane_d/handles) from the position
     * and the basis at *0x31fc44, plus the los_dir endpoints. Dead in normal
     * play (gated on debug global 0x4e4cf2); transcribed for fidelity. */
    results[0].point[1] = physics[2];
    results[0].point[2] = physics[3];
    results[0].point[3] = physics[4];
    results[0].normal[0] = *(float *)(*(int *)0x31fc44);
    results[0].point[0] = 0.0f;
    results[0].normal[1] = *(float *)(*(int *)0x31fc44 + 4);
    results[0].normal[2] = *(float *)(*(int *)0x31fc44 + 8);
    results[0].plane_d = -physics[4];
    los_dir[0] = pos_world[0] + new_pos[0];
    results[0].surface_handle = -1;
    results[0].flags = -1; /* word 0xffff into +0x2a */
    los_dir[1] = physics[3] + new_pos[1];
    *(int *)&results[0].object_handle = *(int *)&physics[0];
    los_dir2[0] = new_pos[0];
    los_dir2[1] = new_pos[1];
    los_dir2[2] = physics[4];
    result_count = 1;
  }

  /* ---- debug-draw stub (0x1a3803..0x1a3880) ---- */
  if (*(char *)0x4e4cf0 != '\0') {
    obj = object_get_and_verify_type((int)physics[0], -1);
    if (*(int *)((char *)obj + 0x70) != -1) {
      *(float *)0x5a8d00 = pos_world[0];
      *(int *)0x5a8d04 = *(int *)&pos_world[1];
      *(int *)0x5a8d08 = *(int *)&pos_world[2];
      *(int *)0x5a8d1c = 1;
      *(float *)0x5a8cf0 = new_pos[0];
      *(float *)0x5a8cf4 = new_pos[1];
      *(float *)0x5a8cf8 = new_pos[2];
      *(int *)0x324fc4 = 0x3f800000;
      *(float *)0x4761b8 = physics[0x16];
      *(float *)0x4761bc = physics[0x15];
    }
  }

  /* the query count (low 16 bits) drives the result-walk and a state bit */
  if ((unsigned short)result_count < 0x10) {
    *(unsigned char *)((char *)physics + 0xa0) &= 0xf7;
  } else {
    *(unsigned char *)((char *)physics + 0xa0) |= 8;
  }

  /* ---- nearest 'mach' surface-plane refinement (0x1a3899..0x1a3d32) ----
   * Only when the count==0 AND a valid mach-surface block (physics[0x24]) is
   * present. Walks the surface block's edge list, projecting the new_velocity
   * (physics+0xb8) onto each candidate plane and keeping the nearest in-range
   * hit, then snaps the ground plane / +0xc4 to it. Held FPU values across the
   * named calls (bsp3d_get_plane / distance helpers) are kept in C scalars. */
  *(int *)((char *)physics + 0xa8) = 0xffffffff; /* physics[0x2a] = none */
  /* note: the entry test reuses the count==0 ZF from the >0x10 compare */
  if ((unsigned short)result_count == 0 &&
      *(int *)((char *)physics + 0x90) != -1) {
    int surf_block; /* EDI = global_collision_bsp_get() */
    int surf_index; /* physics[0x24] index */
    int best_edge; /* local_38 */
    float best_dist; /* local_48 */
    float nrm_d; /* local_8 */
    float proj[3]; /* local_6c/68/64 */
    float plane0[4]; /* local_8c/88/84/80 (from bsp3d_get_plane) */
    float planeN[4]; /* local_58/54/4c projected plane */
    char edge_swap; /* local_1 */
    float bestN[4]; /* local_ac/a8/a4/a0 region: ff58/5c/60/64 */
    int sel_edge;
    int edge0, edge1;
    void *ce; /* current edge element */
    float *pa, *pb;

    surf_block = (int)global_collision_bsp_get();
    surf_index = *(int *)((char *)physics + 0x90);
    best_edge = 0xffffffff;
    *(unsigned int *)&best_dist = 0x7f7fffff;
    if (surf_index >= 0 && surf_index < *(int *)(surf_block + 0x3c)) {
      void *surf =
        tag_block_get_element((void *)(surf_block + 0x3c), surf_index, 0xc);
      edge0 = *(int *)surf;
      edge1 = *(int *)((char *)surf + 4);
      bsp3d_get_plane_from_designator(surf_block, edge0, plane0);
      nrm_d = -(plane0[0] * physics[0x2e] + plane0[1] * physics[0x2f] +
                plane0[2] * physics[0x30] - plane0[3]);
      proj[0] = plane0[0] * nrm_d + physics[0x2e];
      proj[1] = plane0[1] * nrm_d + physics[0x2f];
      proj[2] = plane0[2] * nrm_d + physics[0x30];
      ce = (void *)edge1;
      do {
        void *edge =
          tag_block_get_element((void *)(surf_block + 0x48), (int)ce, 0x18);
        surf_index = *(int *)((char *)physics + 0x90);
        edge_swap = (char)(surf_index == *(int *)((char *)edge + 0x14));
        sel_edge = *(int *)((char *)edge + 0x10 + (edge_swap == 0 ? 4 : 0));
        if (sel_edge != -1) {
          void *e2 =
            tag_block_get_element((void *)(surf_block + 0x3c), sel_edge, 0xc);
          if ((*(unsigned char *)((char *)physics + 0x14) & 2) != 0 ||
              (*(unsigned char *)((char *)e2 + 8) & 4) != 0) {
            float side;
            bsp3d_get_plane_from_designator(surf_block, *(int *)e2, plane0);
            side =
              plane0[0] * proj[0] + plane0[1] * proj[1] + plane0[2] * proj[2];
            nrm_d = side;
            if (!(*(float *)0x2533c0 < side) &&
                physics[0x16] * *(float *)0x255964 <
                  (plane0[0] * physics[0x2e] + plane0[1] * physics[0x2f] +
                   plane0[2] * physics[0x30] - plane0[3])) {
              void *v0 = tag_block_get_element((void *)(surf_block + 0x54),
                                               *(int *)edge, 0x10);
              void *v1 = tag_block_get_element(
                (void *)(surf_block + 0x54), *(int *)((char *)edge + 4), 0x10);
              float ev[3], t2;
              pa = (float *)v0;
              pb = (float *)v1;
              ev[0] = pb[0] - pa[0];
              ev[1] = pb[1] - pa[1];
              ev[2] = pb[2] - pa[2];
              t2 = ((proj[0] - pa[0]) * ev[0] + (proj[1] - pa[1]) * ev[1] +
                    (proj[2] - pa[2]) * ev[2]) /
                   (ev[0] * ev[0] + ev[1] * ev[1] + ev[2] * ev[2]);
              if (!(*(float *)0x2533c0 < t2)) {
                planeN[0] = pa[0];
                planeN[1] = pa[1];
                planeN[2] = pa[2];
              } else if (t2 <= *(float *)0x2533c8) {
                vector3d_scale_add(pa, ev, t2, planeN);
              } else {
                planeN[0] = pb[0];
                planeN[1] = pb[1];
                planeN[2] = pb[2];
              }
              if (distance_squared3d(proj, planeN) < best_dist) {
                best_dist = distance_squared3d(proj, planeN);
                best_edge = sel_edge;
                bestN[0] = plane0[0];
                bestN[1] = plane0[1];
                bestN[2] = plane0[2];
                bestN[3] = plane0[3];
                nrm_d = side;
              }
            }
          }
        }
        ce = (void *)*(int *)((char *)edge + 8 + (edge_swap != 0 ? 4 : 0));
      } while (ce != (void *)edge1);

      if (best_edge != -1 &&
          (physics[0x16] + physics[0x16]) * (physics[0x16] + physics[0x16]) >=
            best_dist &&
          nrm_d <= *(float *)0x2b509c) {
        float depth = (bestN[0] * physics[0x2e] + bestN[1] * physics[0x2f] +
                       bestN[2] * physics[0x30]) -
                      (bestN[3] + physics[0x16]);
        if ((depth < 0.0f ? -depth : depth) <=
            physics[0x16] * *(float *)0x253398) {
          float face =
            bestN[0] * proj[0] + bestN[2] * proj[2] + bestN[1] * proj[1];
          float pushv = -depth;
          physics[0x2e] = bestN[0] * pushv + physics[0x2e];
          physics[0x2f] = bestN[1] * pushv + physics[0x2f];
          physics[0x30] = bestN[2] * pushv + physics[0x30];
          if (*(float *)0x256348 < face) {
            /* In-place adjust of the projected point: proj += -(face+k)*bestN.
             * Original (0x1a3c65..0x1a3c8b) aliases base==out==proj (EBP-0x78);
             * scale_add is component-wise so in-place is safe. */
            vector3d_scale_add(proj, bestN, -(face + *(float *)0x2546a4), proj);
          }
          /* build a draw record from the plane (0x1a3c8f..) */
          pushv = -physics[0x16];
          results[0].normal[0] = bestN[0];
          results[0].normal[1] = bestN[1];
          results[0].normal[2] = bestN[2];
          results[0].plane_d = bestN[3];
          results[0].point[0] = bestN[0] * pushv + physics[0x2e];
          results[0].flags = 1;
          results[0].object_handle = 0;
          results[0].point[2] = bestN[2] * pushv + physics[0x30];
          results[0].surface_handle = -1;
          results[0].point[1] = bestN[1] * pushv + physics[0x2f];
          *(int *)((char *)physics + 0xa8) = best_edge;
          *(unsigned char *)((char *)physics + 0xb1) = 0;
          *(unsigned char *)((char *)physics + 0xb2) = 0;
        }
      }
    }
  }

  /* ---- planar-normal clamp (0x1a3d34..0x1a3d69) ---- */
  r = gx * gx + gy * gy;
  if (*(float *)0x2b5098 < r) {
    r = (float)(*(double *)0x2573d8 / sqrtf(r));
    gy = gy * r;
    gx = r * gx;
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
    if ((*(unsigned short *)((char *)physics + 4) & 0x10) != 0) {
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
      if ((char)*(unsigned char *)((char *)physics + 4) < 0 ||
          (material_local == 0 && (e->flags & 4) == 0)) {
        want = 0;
      } else {
        want = 1;
      }
      if (*(int *)((char *)physics + 0xa8) != -1 &&
          *(int *)((char *)physics + 0xa8) == e->flags) {
        same = 1;
      } else {
        same = 0;
      }
      metric = -(physics[0x2e] * e->normal[0] + e->normal[2] * physics[0x30] +
                 physics[0x2f] * e->normal[1]);
      if (want == 0) {
        if (loop_flag9 == 0 && e->normal[1] < loop_best) {
          goto loopA_take;
        }
      } else if ((material_local != 0 ||
                  gx * e->normal[0] + gy * e->normal[1] <=
                    *(float *)0x253398) &&
                 (loop_flag9 == 0 || same ||
                  (loop_flag1 == 0 && loop_metric < metric))) {
      loopA_take:
        loop_flag9 = want;
        best_index = n;
        loop_flag1 = same;
        loop_best = e->normal[1];
        loop_metric = metric;
      }
      if ((*(unsigned char *)((char *)physics + 0xa0) & 0x10) == 0) {
        if ((e->flags & 8) == 0 && e->object_handle != -1) {
          void *od = datum_get((data_t *)*(int *)0x5a8d50, e->object_handle);
          if ((1 << (*(unsigned char *)((char *)od + 3) & 0x1f) & 0x40) != 0) {
            goto loopA_nomark;
          }
        }
        *(unsigned char *)((char *)physics + 0xa0) |= 0x10;
      loopA_nomark:;
      }
    }
    if ((short)best_index == -1) {
      goto LAB_001a401e;
    }
    e = &results[(short)best_index];
    /* selected entry: compute the result normal dot (local_3c) */
    best_t = -(e->normal[0] * new_pos[0] + e->normal[2] * new_pos[2] +
               e->normal[1] * new_pos[1]);
    if (loop_flag9 == 0 && loop_flag1 == 0) {
      if (e->normal[1] < physics[0x19]) {
        goto LAB_001a401e;
      }
      if ((*(unsigned short *)((char *)physics + 4) & 1) != 0 &&
          physics[0x17] < *(float *)0x2548fc) {
        float seg[3], slen;
        vector3d_scale_add(&new_pos[0], &e->normal[0], best_t, proj_seg);
        slen = FUN_00012170(proj_seg);
        if (physics[0x17] * physics[0x17] < slen) {
          float md = FUN_00012fe0(&new_pos[0]);
          if (best_t / md < physics[0x18]) {
            goto LAB_001a401e;
          }
        }
        (void)seg;
        (void)slen;
      }
    }
    /* snap ground plane (+0x80 = entry.normal[3]) + surface handle (+0xa4) to
     * the selected entry, then +0xc4 from the normal dot. EDI = &entry.normal
     */
    {
      int sel_surface = e->flags;
      *(unsigned char *)((char *)physics + 0xa0) &= 0xfe;
      *(int *)((char *)physics + 0x80) = *(int *)&e->normal[0];
      *(int *)((char *)physics + 0x84) = *(int *)&e->normal[1];
      *(int *)((char *)physics + 0x88) = *(int *)&e->normal[2];
      *(int *)((char *)physics + 0x8c) = *(int *)&e->plane_d;
      *(int *)((char *)physics + 0xa4) = sel_surface;
      if (sel_surface != -1 &&
          sel_surface == *(int *)((char *)physics + 0xa8)) {
        *(int *)((char *)physics + 0xc4) = 0;
        goto LAB_001a4062;
      }
      physics[0x31] =
        -(new_pos[0] * physics[0x21] + new_pos[2] * physics[0x22] +
          new_pos[1] * physics[0x20]);
      goto LAB_001a4062;
    }
  }
  goto LAB_001a4062_done;

LAB_001a401e:
  *(unsigned char *)((char *)physics + 0xa0) |= 1;
  *(int *)((char *)physics + 0x80) = *(int *)0x32513c;
  *(int *)((char *)physics + 0x84) = *(int *)0x325140;
  *(int *)((char *)physics + 0x88) = *(int *)0x325144;
  *(int *)((char *)physics + 0x8c) = *(int *)0x325148;
  *(int *)((char *)physics + 0xa4) = 0xffffffff;
  physics[0x31] = 0.0f;

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
          void *o = object_get_and_verify_type(*eh, -1);
          float ddx = *(float *)((char *)o + 0x18) - los_dir[0];
          float ddy = *(float *)((char *)o + 0x1c) - los_dir[1];
          float ddz = *(float *)((char *)o + 0x20) - los_dir[2];
          float d2 = ddx * ddx + ddy * ddy + ddz * ddz;
          if (near_obj != -1) {
            if ((short)near_type == 1) {
              if (*(short *)((char *)o + 0x64) != 1)
                goto loopB_next;
            } else if (*(short *)((char *)o + 0x64) == 1) {
              goto loopB_take;
            }
            if (d2 <= near_dist)
              goto loopB_next;
          }
        loopB_take:
          near_dist = d2;
          near_obj = *eh;
          near_type = (near_type & 0xffff0000) |
                      (unsigned short)*(short *)((char *)o + 0x64);
        }
      loopB_next:
        eh = (int *)((char *)eh + 0x2c);
        n--;
      } while (n != 0);
    }
    *(int *)((char *)physics + 0x98) = near_obj;
  }

  /* ---- loop C: 'mach' tag scan (0x1a40f2..0x1a4160). Pointer-walk. ---- */
  *(int *)((char *)physics + 0x9c) = 0xffffffff;
  if ((short)(unsigned short)result_count > 0) {
    int n = (unsigned short)result_count;
    int *eh = &results[0].object_handle;
    do {
      int handle = *eh;
      if (handle != -1) {
        void *t2obj = object_try_and_get_and_verify_type(handle, 0x80);
        if (t2obj != (void *)0) {
          void *mtag = tag_get(0x6d616368, *(int *)t2obj);
          if ((*(unsigned char *)((char *)mtag + 0x292) & 4) != 0 &&
              *(short *)((char *)mtag + 0x2ea) != -1) {
            *(int *)((char *)physics + 0x9c) = handle;
          }
        }
      }
      eh = (int *)((char *)eh + 0x2c);
      n--;
    } while (n != 0);
  }

  /* ---- writeback: new_position (+0xac), new_velocity (+0xb8), step distance
   * (+0xc8) and the z step-down (0x1a4160..0x1a41de). new_position is the
   * displacement between the two query out-points (los_dir - los_dir2). ---- */
  physics[0x2b] = los_dir[0] - los_dir2[0]; /* +0xac */
  physics[0x2c] = los_dir[1] - los_dir2[1];
  physics[0x2d] = los_dir[2] - los_dir2[2];
  physics[0x2e] = los_dir[0];
  physics[0x2f] = los_dir[1];
  physics[0x30] = los_dir[2];
  physics[0x32] =
    sqrtf(physics[0x2b] * physics[0x2b] + physics[0x2c] * physics[0x2c] +
         physics[0x2d] * physics[0x2d]);
  physics[0x30] = physics[0x30] - physics[0xe];

  /* ---- biped step-down (0x1a41de..0x1a42da) ---- */
  flags = *(unsigned short *)((char *)physics + 4);
  if ((flags & 4) != 0 && (flags & 8) != 0) {
    obj = object_get_and_verify_type((int)physics[0], 1);
    if (*(int *)((char *)obj + 0x1c8) != -1) {
      tag = tag_get(0x62697064, *(int *)obj);
      if ((*(unsigned char *)((char *)tag + 0x2f4) & 0x18) == 0) {
        float half = *(float *)((char *)tag + 0x424) * *(float *)0x253398;
        *(int *)&surf[0] = *(int *)&physics[0x2b]; /* movl copies (0x1a4239) */
        *(int *)&surf[1] = *(int *)&physics[0x2c];
        surf[2] = physics[0x2d] + half;
        if (FUN_0014ec30(
              (int)((-(unsigned int)((flags & 0x80) != 0) & 0xffdffd00) +
                    0x20c3a0),
              surf, *(float *)((char *)tag + 0x42c), 0.0f, (int)physics[0], 0,
              debug_scratch)) {
          float fr =
            *(float *)((char *)tag + 0x424) -
            (*(float *)((char *)tag + 0x42c) + *(float *)((char *)tag + 0x42c));
          disp[0] = fr * *(float *)(*(int *)0x31fc44);
          disp[1] = fr * *(float *)(*(int *)0x31fc44 + 4);
          disp[2] = fr * *(float *)(*(int *)0x31fc44 + 8);
          if (FUN_0014c4b0((int)debug_scratch, &physics[0x2b], disp,
                           surface_result)) {
            *(unsigned char *)((char *)physics + 0xa0) |= 4;
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
    unsigned int *np = (unsigned int *)&physics[0x2b];
    isnan_tmp = np[0];
    if ((isnan_tmp & 0x7f800000) == 0x7f800000 ||
        ((isnan_tmp = np[1]), (isnan_tmp & 0x7f800000) == 0x7f800000) ||
        ((isnan_tmp = np[2]), (isnan_tmp & 0x7f800000) == 0x7f800000)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real_point3d(%f, %f, %f)",
                              "&physics->new_position", (double)physics[0x2b],
                              (double)physics[0x2c], (double)physics[0x2d]),
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0xee2, true);
      system_exit(-1);
    }
  }
  {
    unsigned int *nv = (unsigned int *)&physics[0x2e];
    isnan_tmp = nv[0];
    if ((isnan_tmp & 0x7f800000) == 0x7f800000 ||
        ((isnan_tmp = nv[1]), (isnan_tmp & 0x7f800000) == 0x7f800000) ||
        ((isnan_tmp = nv[2]), (isnan_tmp & 0x7f800000) == 0x7f800000)) {
      display_assert(csprintf(error_string_buffer,
                              "%s: assert_valid_real_vector2d(%f, %f, %f)",
                              "&physics->new_velocity", (double)physics[0x2e],
                              (double)physics[0x2f], (double)physics[0x30]),
                     "c:\\halo\\SOURCE\\units\\bipeds.c", 0xee3, true);
      system_exit(-1);
    }
  }
  (void)flags_word;
  (void)tval;
  (void)fdist;
}
