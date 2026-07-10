/* path_obstacles.c — AI path obstacle debug rendering.
 *
 * Corresponds to path_smoothing.obj (this TU is the path_obstacles.c
 * translation unit within it). Source path confirmed via __FILE__ assert
 * xref: c:\halo\SOURCE\ai\path_obstacles.c
 *
 * Ported:
 *   FUN_00062960 (0x62960) — path_obstacles_debug_render: iterate the
 *     obstacle-disc array, raycast each disc's point, and draw either a
 *     projected disc (on hit) or a 3D sphere (on miss), tinted by the disc's
 *     owning obstacle index.
 *
 * Key layout (obstacles record):
 *   +0x00  int16  obstacle_count
 *   +0x02  int16  disc_count            (both reloaded each iteration)
 * Per-disc record (stride 0x18, base = obstacles + disc_index*0x18):
 *   +0x0a  int16  obstacle_index
 *   +0x10  float  point.x
 *   +0x14  float  point.y
 *   +0x18  float  radius
 *   +0x1c  float  point.z
 * MAXIMUM_DISC_COUNT = 0x80.
 *
 * Globals:
 *   0x2533c0  float  zero/threshold constant (radius > this => extra draw)
 *   0x25eeac  float  ray-direction Z scale factor
 *   0x2c8fb8  color table, 0x10-byte entries indexed by obstacle_index
 *             (byte-offset pointer add, NOT float-element indexing).
 */

#include "../../common.h"

/* 0x00062960 — path_obstacles_debug_render
 *
 * Note the hit/miss asymmetry, preserved from the original:
 *   - On raycast HIT, the projected-disc call (FUN_0018a860) receives
 *     &disc->point (pfVar1, pointing into the obstacles buffer, where the
 *     three contiguous floats are x, y, radius — z lives at +0x1c and is not
 *     part of that view; the disc renderer only uses x/y + the plane).
 *   - On raycast MISS, the sphere call (FUN_00189540) receives the local
 *     {x,y,z} point copy, which reads z from disc+0x1c.
 */
void path_obstacles_debug_render(void *obstacles, float radius)
{
  short disc_index;
  short
    count; /* reused: disc_count, then disc->obstacle_index (matches orig) */
  int disc_off;
  float *disc_point;
  bool hit;
  short collision_result[18]; /* 36-byte buffer for FUN_0014df70 */
  float plane_scratch[11]; /* scratch/output buffer for FUN_0018a860 */
  float ray_dir[3]; /* {0, 0, dir.z} */
  float point[3]; /* contiguous {x, y, z} */

  count = *(short *)((char *)obstacles + 2); /* disc_count */
  disc_index = 0;
  if (0 < count) {
    do {
      if ((disc_index < 0) || (count <= disc_index) || (0x80 < count)) {
        display_assert("disc_index>=0 && disc_index<obstacles->disc_count && "
                       "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                       "c:\\halo\\source\\ai\\path.h", 0x18c, true);
        halt_and_catch_fire();
      }
      disc_off = disc_index * 0x18;
      count =
        *(short *)((char *)obstacles + disc_off + 10); /* obstacle_index */
      if ((count < 0) || (*(short *)obstacles <= count)) {
        display_assert("disc->obstacle_index>=0 && "
                       "disc->obstacle_index<obstacles->obstacle_count",
                       "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x211, true);
        halt_and_catch_fire();
      }
      point[1] = *(float *)((char *)obstacles + disc_off + 0x14); /* y */
      point[0] = *(float *)((char *)obstacles + disc_off + 0x10); /* x */
      point[2] = *(float *)((char *)obstacles + disc_off + 0x1c); /* z */
      disc_point = (float *)((char *)obstacles + disc_off + 0x10);
      ray_dir[2] = (radius + *(float *)((char *)obstacles + disc_off + 0x18)) *
                   *(float *)0x25eeac;
      ray_dir[0] = 0.0f;
      ray_dir[1] = 0.0f;
      hit = FUN_0014df70(0x21, point, ray_dir, -1, collision_result);
      if (hit) {
        FUN_0018a860(1, plane_scratch, 2, 1, disc_point,
                     *(float *)((char *)obstacles + disc_off + 0x18),
                     (char *)0x2c8fb8 +
                       *(short *)((char *)obstacles + disc_off + 10) * 0x10,
                     0.015625f);
        if (*(float *)0x2533c0 < radius) {
          FUN_0018a860(1, plane_scratch, 2, 1, disc_point,
                       radius + *(float *)((char *)obstacles + disc_off + 0x18),
                       (char *)0x2c8fb8 +
                         *(short *)((char *)obstacles + disc_off + 10) * 0x10,
                       0.015625f);
        }
      } else {
        FUN_00189540(1, point, *(float *)((char *)obstacles + disc_off + 0x18),
                     (char *)0x2c8fb8 +
                       *(short *)((char *)obstacles + disc_off + 10) * 0x10);
        if (*(float *)0x2533c0 < radius) {
          FUN_00189540(1, point,
                       radius + *(float *)((char *)obstacles + disc_off + 0x18),
                       (char *)0x2c8fb8 +
                         *(short *)((char *)obstacles + disc_off + 10) * 0x10);
        }
      }
      count = *(short *)((char *)obstacles + 2); /* reload disc_count */
      disc_index = disc_index + 1;
    } while (disc_index < count);
  }
  return;
}

/* 0x00063710 — structure_test_ray2d  (TU:
 * c:\halo\SOURCE\ai\path_structure_bsp.c)
 *
 * March a 2D line query through the structure-bsp surface set, skipping
 * surfaces flagged in the per-surface skip byte-array and (unless
 * ignore_breakable) surfaces whose breakable bit is set in the runtime
 * breakable bitmap. Returns true (1) if the line is blocked by a surface
 * within the query, writing the hit into out_result as
 * {float t; int surface_index; int leaf/pass}; returns false (0) on miss,
 * writing {t, surface_index, -1}.
 *
 * ABI (cdecl, all stack params; returns bool in AL):
 *   +0x08 structure_bsp     (uses +0xb0 surface tag_block, +0x1e8 skip-flag
 * ptr) +0x0c ignore_breakable  (nonzero => skip the breakable test) +0x10 point
 * (forwarded to collision_surface_test_line2d) +0x14 surface_index     (initial
 * surf; reused as scratch/out surface index) +0x18 direction (forwarded) +0x1c
 * t                 (LHS of every FCOMP) +0x20 out_result        (3 dwords)
 *
 * out_result layout mirrors the 6-float scratch buffer filled by
 * collision_surface_test_line2d: two records {t, aux, surface_index}, the
 * "enter" record at buf[0..2] and the "exit" record at buf[3..5].
 */
char structure_test_ray2d(void *structure_bsp, char ignore_breakable,
                          float *point, int surface_index, float *direction,
                          float t, void *out_result)
{
  void *bsp_surfaces; /* EDI: surface tag_block element base */
  unsigned int *breakable_bitmap; /* EBX: per-surface breakable dwords */
  char *skip_flags; /* [structure_bsp+0x1e8]: skip byte-array */
  int cur_surf;
  char flagbyte;
  char surf_blocked;
  char *coll_surface;
  volatile unsigned int
    word; /* store/reload shape lever to match original codegen */
  int *out;
  float buf[6]; /* out_result of collision_surface_test_line2d:
                   enter{t,aux,surf}, exit{t,aux,surf} */

  bsp_surfaces = tag_block_get_element((char *)structure_bsp + 0xb0, 0, 0x60);
  breakable_bitmap = (unsigned int *)breakable_surfaces_get_bsp_surface_data();
  skip_flags = *(char **)((char *)structure_bsp + 0x1e8);

  collision_surface_test_line2d((int)bsp_surfaces, surface_index, 2, 1, point,
                                direction, buf);

  while (1) {
    /* enter-surface handling (only when the query point is before enter_t) */
    if (t < buf[0]) {
      cur_surf = *(int *)&buf[2]; /* enter surface index */
      flagbyte = skip_flags[cur_surf];
      if (flagbyte != 0) {
        if (ignore_breakable == 0 && flagbyte < 0) {
          coll_surface = (char *)tag_block_get_element(
            (char *)bsp_surfaces + 0x3c, cur_surf, 0xc);
          if ((coll_surface[8] & 8) == 0) {
            display_assert("TEST_FLAG(collision_surface->flags, "
                           "_collision_surface_breakable_bit)",
                           "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0x69,
                           true);
            system_exit(-1);
          }
          word = (unsigned char)coll_surface[9];
          surf_blocked =
            (breakable_bitmap[word >> 5] & (1u << (word & 0x1f))) != 0;
          if (surf_blocked == 0)
            goto exit_check; /* breakable & broken => not blocking here */
          cur_surf = *(int *)&buf[2];
        }
        if (cur_surf != -1)
          goto march;
      }
    }

  exit_check:
    /* exit-surface handling; writeout when the query point is at/before exit_t
     */
    if (t <= buf[3])
      break;
    cur_surf = *(int *)&buf[5]; /* exit surface index */
    flagbyte = skip_flags[cur_surf];
    if (flagbyte == 0)
      break;
    if (ignore_breakable == 0 && flagbyte < 0) {
      coll_surface = (char *)tag_block_get_element((char *)bsp_surfaces + 0x3c,
                                                   cur_surf, 0xc);
      if ((coll_surface[8] & 8) == 0) {
        display_assert("TEST_FLAG(collision_surface->flags, "
                       "_collision_surface_breakable_bit)",
                       "c:\\halo\\SOURCE\\ai\\path_structure_bsp.c", 0x7e,
                       true);
        system_exit(-1);
      }
      word = (unsigned char)coll_surface[9];
      surf_blocked = (breakable_bitmap[word >> 5] & (1u << (word & 0x1f))) != 0;
      if (surf_blocked == 0)
        break;
      cur_surf = *(int *)&buf[5];
    }
    if (cur_surf == -1)
      break;

  march:
    surface_index = cur_surf;
    collision_surface_test_line2d((int)bsp_surfaces, cur_surf, 2, 1, point,
                                  direction, buf);
  }

  /* writeout (0x63890) */
  out = (int *)out_result;
  if (t < buf[0]) { /* Path A: blocked at enter surface */
    *(float *)out = buf[0];
    out[1] = surface_index;
    out[2] = *(int *)&buf[1];
    return 1;
  }
  out[1] = surface_index;
  if (t <= buf[3]) { /* Path C: miss */
    out[2] = -1;
    *(float *)out = t;
    return 0;
  }
  /* Path B: blocked at exit surface */
  *(float *)out = buf[3];
  out[2] = *(int *)&buf[4];
  return 1;
}

/* 0x000638f0 — path_smoothing surface-height coplanarity test.
 *
 * Projects `point` onto two collision surfaces (surf_a, surf_b) of the
 * structure-BSP block at def+0xB0 and compares the projected height
 * (third component of each projection output). Returns false when either
 * surface index is -1, or when the two heights differ by less than the
 * global tolerance (i.e. the two projections are coplanar/close enough);
 * returns true when the height difference exceeds the tolerance.
 *
 * ABI (disasm): 4 cdecl stack args, BYTE-BOOL return in AL. Default AL=0;
 * MOV AL,1 only on the fall-through where ABS(diff) is NOT < threshold.
 *
 * Projection-output-size (§5): collision_surface_project_point2d writes 3
 * floats to out_point (Ghidra sized the buffers float[2] — wrong). The two
 * output buffers are contiguous in the 0x18-byte frame; the compare reads
 * the THIRD component of each (out1[2] @ EBP-0x4, out2[2] @ EBP-0x10).
 *
 * Threshold at 0x25f0c8 is a DOUBLE (FCOMP double ptr): the x87 ABS of the
 * float difference is promoted to double for the compare. */
bool FUN_000638f0(int def, float *point, int surf_a, int surf_b)
{
  void *block_elem;
  float out1[3];
  float out2[3];

  block_elem = tag_block_get_element((void *)(def + 0xb0), 0, 0x60);
  if ((surf_a != -1) && (surf_b != -1)) {
    collision_surface_project_point2d((int)block_elem, surf_a, 2, 1, point,
                                      out1);
    collision_surface_project_point2d((int)block_elem, surf_b, 2, 1, point,
                                      out2);
    if (fabs(out1[2] - out2[2]) < *(double *)0x25f0c8)
      return 0;
    return 1;
  }
  return 0;
}
