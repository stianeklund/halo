#include "x87_math.h" /* x87_fatan2f: inline FPATAN atan2, matches original */

/* MSVC 7.1 FABS intrinsic: declared+pragma here so fabs() inlines to a single
 * FABS instruction instead of a CRT call. */
extern double __cdecl fabs(double);
#if defined(_MSC_VER) && !defined(__clang__)
#pragma intrinsic(fabs)
#else
/* clang builds with -fno-builtin, which ignores the intrinsic pragma above and
 * emits a real CRT call to fabs. The original inlines a single x87 FABS. Force
 * clang to inline via the always-available builtin so the codegen matches the
 * binary (and so equivalence harnesses don't see an external fabs stub). */
#define fabs __builtin_fabs
#endif

/* FUN_00061ca0 (0x61ca0)
 *
 * Per-tick debug path-obstacle-avoidance key handler.  When the debug enable
 * byte (0x3340a9) is set and the developer console is not active, polls debug
 * keys and toggles the mode/enable bytes, optionally (re)builds the obstacle
 * avoidance record and advances it, then draws the current obstacle discs and
 * path steps.  Runs the two draw calls whenever the enable byte is set,
 * regardless of console state.
 *
 * Confirmed from disassembly at 0x61ca0:
 *   - scenario_get() is 0-arg (returns the current scenario tag base; 50+
 *     ported callers).  The 7 pushes Ghidra attributed to scenario_get are
 *     actually FUN_00060ea0's stack args (cdecl arg mis-grouping, §7).
 *   - FUN_00060ea0 is __thiscall + @eax: ECX=0x331f68 (avoidance record),
 *     EAX=0x5ab250 (end point2d), plus 9 cdecl stack args.  ADD ESP,0x24
 *     (36 bytes = 9 args) proves the split is 0 (scenario_get) + 9, not 7+2.
 *   - FUN_000615b0 takes @eax=0x331f68 (the record); its return is discarded.
 *   - FUN_00062960(&0x3334a0 obstacles, 0x5ab240 radius) and
 *     FUN_000609e0(&0x331f68 path) are clean cdecl (ADD ESP,0xc = 2+1).
 */
void FUN_00061ca0(void)
{
  void *scenario;

  if (*(char *)0x3340a9 != 0) {
    if (!console_is_active()) {
      if (input_key_is_down(0x22)) {
        *(char *)0x3340a8 = 1;
        *(char *)0x3340a9 = 0;
      }
      if (input_key_is_down(0x31)) {
        *(char *)0x3340a8 = 0;
        *(char *)0x3340a9 = 0;
      }
      if (input_key_is_down(0x3f)) {
        scenario = scenario_get();
        FUN_00060ea0((void *)0x331f68, (float *)0x5ab250, (void *)0x3334a0,
                     scenario, *(unsigned char *)0x5ab244, *(float *)0x5ab240,
                     (float *)0x5ab260, *(int *)0x5ab25c, *(float *)0x5ab248,
                     *(unsigned char *)0x5ab245, 0);
      }
      if (input_key_is_down(0x26)) {
        FUN_000615b0((void *)0x331f68);
      }
    }
    FUN_00062960((void *)0x3334a0, *(float *)0x5ab240);
    FUN_000609e0((void *)0x331f68);
  }
  return;
}

/* FUN_00061d80 (0x61d80)
 *
 * Zero-clear three consecutive int16 fields (6 bytes) at the pointer arg.
 * Disassembly: XOR ECX,ECX then MOV WORD PTR [EAX+{0,2,4}],CX -- the 0x66
 * operand-size prefix confirms 16-bit store width (LOADW), so the target is
 * a short[3] / small struct of three int16 fields, not int[3].  cdecl, one
 * stack pointer arg; leaf, no calls, no FPU.
 */
void FUN_00061d80(int16_t *out)
{
  out[0] = 0;
  out[1] = 0;
  out[2] = 0;
  return;
}

/* FUN_00061da0 (0x61da0)
 *
 * Store a float/int pair into a structure: value0 (float) -> [out+0x0],
 * value1 (int) -> [out+0x4].  cdecl, three stack args, void return; leaf,
 * no calls.  Disassembly: FLDS 0xc(ebp) / FSTPS (eax) proves the first field
 * is a float copied through the x87 (not a plain MOV); the second field is a
 * plain MOV of a 32-bit int/handle.  The Ghidra "undefined4" for arg0 masked
 * the float store.
 */
void FUN_00061da0(void *out, float value0, int value1)
{
  *(float *)out = value0;
  *((int *)out + 1) = value1;
  return;
}

/* FUN_00061dc0 (0x61dc0)
 *
 * 2D rotation / complex multiply.  Rotates the 2D vector (point[0], point[1])
 * by an angle whose sine is rot_sin and cosine is rot_cos, writing the result
 * to out[0..1]:
 *   out[0] = rot_cos*point[0] - rot_sin*point[1]   (real part, FSUBP)
 *   out[1] = rot_sin*point[0] + rot_cos*point[1]   (imag part, FADDP)
 * cdecl, four stack args, void return; leaf, no calls.  Disassembly verified:
 * each product uses a memory-operand FMUL in the exact binary order, and the
 * real part is evaluated first (FSUBRP st1-st0 = cos*x - sin*y, NOT inverted)
 * then the imag part (FADDP), storing real then imag.  Computing both into
 * named temps (real before imag) before the two stores reproduces that
 * evaluation order exactly -- 100% VC71 (18/18 insns).
 */
void FUN_00061dc0(float *point, float rot_sin, float rot_cos, float *out)
{
  float real;
  float imag;

  real = rot_cos * point[0] - rot_sin * point[1];
  imag = rot_sin * point[0] + rot_cos * point[1];
  out[0] = real;
  out[1] = imag;
  return;
}

/* Projection axis remapping table at 0x28cb10.
 * Indexed as [projection_axis * 2 + projection_sign][component].
 * Maps a 3D projection basis + sign to two axis indices for 2D projection.
 * Component 0 -> out[0], component 1 -> out[1]. */
static const short g_projection3d_mappings[6][2] = {
  { 2, 1 }, { 1, 2 }, { 0, 2 }, { 2, 0 }, { 1, 0 }, { 0, 1 },
};

/* 0x61df0 — Project a 3D point onto a 2D plane.
 * Selects two of the three float lanes of `point` according to the projection
 * axis (0..2) and sign (0/1), writing them to out_projected[0] and [1].
 * real_math.h:0x35b/0x35c assert projection in [_x,_z] and sign in {0,1}. */
void FUN_00061df0(void *point, short projection, unsigned char sign,
                  void *out_projected)
{
  int idx;
  float tmp;

  if ((projection < 0) || (projection > 2)) {
    display_assert("projection>=_x && projection<=_z", "..\\math\\real_math.h",
                   0x35b, 1);
    system_exit(-1);
  }
  if (!(~(sign & ~1))) {
    display_assert("~(sign&~1)", "..\\math\\real_math.h", 0x35c, 1);
    system_exit(-1);
  }
  idx = sign + projection * 2;
  /* out[1] loaded first (FPU), out[0] copied as a raw dword, matching the
   * original's interleaved fld / integer-mov scheduling. */
  tmp = ((float *)point)[g_projection3d_mappings[idx][1]];
  ((unsigned int *)out_projected)[0] =
    ((unsigned int *)point)[g_projection3d_mappings[idx][0]];
  ((float *)out_projected)[1] = tmp;
}

/* 0x61e80 — 2D point-in-radius test.
 * Returns 1 when the squared 2D distance between points p0 and p1 (using the
 * x=[0] and y=[1] lanes only) is <= radius*radius, else 0.  Pure leaf, cdecl,
 * three stack args (two float*, one float).  The y-term is summed before the
 * x-term, matching the decompiler's fld ordering; every product is a
 * self-multiply so there is no operand-order/cross-product hazard. */
int FUN_00061e80(float *p0, float *p1, float radius)
{
  if ((p1[1] - p0[1]) * (p1[1] - p0[1]) + (p1[0] - p0[0]) * (p1[0] - p0[0]) <=
      radius * radius) {
    return 1;
  }
  return 0;
}

/* FUN_00062020 (0x62020)  --  add_obstacle (path_obstacles.c)
 *
 * Append one obstacle record to an obstacle-set.  The set header is a small
 * int16 struct: [+0x00] obstacle_count, [+0x02] disc_count (element count,
 * MAXIMUM_DISC_COUNT == 0x80), [+0x04] a secondary count bumped only when the
 * flags low bit is set.  Records begin at +0x08 with a 24-byte (0xc short)
 * stride; record N lives at (char *)set + N*24 + 8.
 *
 * Two bounds asserts (path_obstacles.c lines 0x68/0x69) then a capacity guard
 * that returns false when the set is full.  On success the new record is
 * filled and true is returned.
 *
 * Field-copy widths are taken from the disassembly (0x62020): rec+0x0c is an
 * x87 float copy (FLD/FSTP) of vector[j]; rec+0x08 and rec+0x14 are raw 32-bit
 * dword copies (integer MOV) of vector[i] and vector[2].  DAT_0028cb24 /
 * DAT_0028cb26 are int16 axis-index selectors (read via MOVSX), i.e. the two
 * projected components of the vector.  cdecl, five stack args, bool in AL.
 */
bool FUN_00062020(int16_t *obstacle_set, uint32_t datum, uint16_t flags,
                  float *vector, uint32_t param_5)
{
  int16_t disc_count;
  int16_t i;
  int16_t j;
  char *rec;

  /* obstacles->disc_count>=0 && obstacles->disc_count<=MAXIMUM_DISC_COUNT */
  if (obstacle_set[1] < 0 || obstacle_set[1] > 0x80) {
    display_assert(
      "obstacles->disc_count>=0 && obstacles->disc_count<=MAXIMUM_DISC_COUNT",
      "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x68, 1);
    system_exit(-1);
  }
  /* obstacles->obstacle_count>=0 &&
   * obstacles->obstacle_count<=obstacles->disc_count */
  if (obstacle_set[0] < 0 || obstacle_set[0] > obstacle_set[1]) {
    display_assert("obstacles->obstacle_count>=0 && "
                   "obstacles->obstacle_count<=obstacles->disc_count",
                   "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x69, 1);
    system_exit(-1);
  }

  disc_count = obstacle_set[1];
  if (disc_count == 0x80)
    return false; /* set is full */

  obstacle_set[1] = disc_count + 1;
  rec = (char *)obstacle_set + disc_count * 24 + 8;
  if ((flags & 1) != 0)
    obstacle_set[2] = obstacle_set[2] + 1;

  *(uint16_t *)(rec + 0x00) = flags;
  *(uint32_t *)(rec + 0x04) = datum;
  *(uint16_t *)(rec + 0x02) = 0xffff;

  i = *(int16_t *)0x28cb24;
  j = *(int16_t *)0x28cb26;
  *(uint32_t *)(rec + 0x08) = *(uint32_t *)&vector[i];
  *(float *)(rec + 0x0c) = vector[j];
  *(uint32_t *)(rec + 0x10) = param_5;
  *(uint32_t *)(rec + 0x14) = *(uint32_t *)&vector[2];

  return true;
}

/* FUN_00198f10 (0x198f10) — resolve planar-fog render parameters
 *
 * Fills the caller's fog-parameter record (`fog`, ~0x4c bytes; the single
 * caller FUN_00185290 passes &global 0x506730) for portal/cluster `index`
 * (uint16 loaded zero-extended from global 0x506784 by the caller).
 *
 * Confirmed from decompile + disassembly:
 *   - NULL `fog` asserts: display_assert("fog", ".../structures.c", 0x1f1, 1)
 *     then system_exit(-1). String at 0x29dc54 = "fog" (byte-verified).
 *   - tag_get(0x666f6720='fog ', fog_index) returns the fog tag definition.
 *   - fog type word at fog+0x1c: 0=none/early-out, 1=planar plane present,
 *     2=atmospheric.  from_object path (fog came via FUN_0018e7d0 marker,
 *     not a portal) instead ORs bit0 into fog+0x02.
 *   - portal element (scenario+0x134, size 0x68): byte+3 & 0x80 == ushort+2
 *     bit15 (== (short)ushort < 0); both tests reproduced as written.
 *   - FLOAT_002533c0 = 0.0f (byte-verified at 0x2533c0); reproduced as the
 *     literal 0.0f (FMUL against a 0.0 rodata constant).
 *   - vec[3] (decomp local_14/local_10/local_c, contiguous EBP-0x14..-0xc) is
 *     declared as a float[3] so &vec[0] passed to FUN_001954e0 (normalize) is
 *     a contiguous buffer; local_c's reuse as the scale temp is mirrored in
 *     vec[2].
 *   - Field-copy store order (fog+0x30..0x44 from fog_tag+0x78/0x7c/0x80/
 *     0x58/0x68/0x60) preserved exactly as MSVC scheduled it (0x3c before
 *     0x44 before 0x40).
 *   - Two tag_block_get_element calls have their results discarded (bounds/
 *     assert side-effect only) — preserved faithfully.
 *   - cdecl, 2 stack args (no ADD ESP shown at the call site; caller cleans 8).
 */
void FUN_00198f10(int index, void *fog)
{
  void *scenario;
  void *marker;
  char *portal;
  char *plane;
  short *fog_tag;
  char *out;
  int fog_index;
  short portal_idx;
  char from_object;
  float vec[3];

  out = (char *)fog;
  scenario = scenario_get();
  from_object = 0;

  if (fog == NULL) {
    display_assert("fog", "c:\\halo\\SOURCE\\structures\\structures.c", 0x1f1,
                   1);
    system_exit(-1);
  }

  *(short *)(out + 0x1c) = 0;
  *(short *)out = 0;
  *(int *)(out + 0x48) = 0;

  fog_index = structure_get_planar_fog_definition_index(scenario, index, 0);
  portal_idx = (short)index;

  if (fog_index == -1) {
    if (portal_idx != -1) {
      tag_block_get_element((char *)scenario + 0x134, (int)portal_idx, 0x68);
      marker = FUN_0018e7d0(0);
      if (marker != NULL) {
        fog_index = *(int *)((char *)marker + 0xa4);
      }
    }
    from_object = 1;
    if (fog_index == -1) {
      return;
    }
  }

  scenario = scenario_get();
  portal = (char *)tag_block_get_element((char *)scenario + 0x134,
                                         (int)portal_idx, 0x68);
  fog_tag = (short *)tag_get(0x666f6720, fog_index);

  if (from_object == 0) {
    if ((*(unsigned char *)(portal + 3) & 0x80) == 0) {
      *(short *)(out + 0x1c) = 2;
    } else {
      *(short *)(out + 0x1c) = 1;
      plane = (char *)tag_block_get_element(
        (char *)scenario + 0x178, *(unsigned short *)(portal + 2) & 0x7fff,
        0x20);
      *(int *)(out + 0x20) = *(int *)(plane + 4);
      *(int *)(out + 0x24) = *(int *)(plane + 8);
      *(int *)(out + 0x28) = *(int *)(plane + 0xc);
      *(int *)(out + 0x2c) = *(int *)(plane + 0x10);
    }
    *(int *)(out + 0x30) = *(int *)((char *)fog_tag + 0x78);
    *(int *)(out + 0x34) = *(int *)((char *)fog_tag + 0x7c);
    *(int *)(out + 0x38) = *(int *)((char *)fog_tag + 0x80);
    *(int *)(out + 0x3c) = *(int *)((char *)fog_tag + 0x58);
    *(int *)(out + 0x44) = *(int *)((char *)fog_tag + 0x68);
    *(int *)(out + 0x40) = *(int *)((char *)fog_tag + 0x60);
    if ((short)*(unsigned short *)(portal + 2) < 0) {
      tag_block_get_element((char *)scenario + 0x178,
                            *(unsigned short *)(portal + 2) & 0x7fff, 0x20);
      vec[2] = *(float *)((char *)fog_tag + 4) * 0.0f; /* FLOAT_002533c0 */
      *(float *)(out + 0x2c) = vec[2] + *(float *)(out + 0x2c);
      vec[0] = vec[2] * *(float *)(out + 0x20);
      vec[1] = vec[2] * *(float *)(out + 0x24);
      vec[2] = vec[2] * *(float *)(out + 0x28);
      FUN_001954e0(vec);
    }
  } else {
    *(unsigned char *)(out + 2) |= 1;
  }

  *(short *)out = *fog_tag;
  *(void **)(out + 0x48) = (char *)fog_tag + 0x84;
}
/* path_obstacles.c — AI path obstacle-disc connectivity.
 *
 * Corresponds to a routine in structures.obj (its sole ported caller,
 * cluster_partition_assign_groups / FUN_000628b0 at 0x628b0, lives in
 * src/halo/structures/structures.c).  __FILE__ evidence for this function is
 * c:\halo\SOURCE\ai\path_obstacles.c (from its display_assert strings at
 * lines 0x183, 0x18c, 0x1a5); two interior asserts cite
 * c:\halo\source\ai\path.h (0x18c) verbatim.
 *
 * Ported: FUN_00062680 (0x62680) — flood-fill of the "obstacle disc" set.
 */

#include "../../common.h"
#include "../../x87_math.h"

/* 0x0062410 — FUN_00062410
 *
 * Obstacle-disc overlap query: given an obstacle-disc set (obstacles), a disc
 * index to skip (disc_index_skip), a 2D query centre (position_xy[0]=x,
 * position_xy[1]=y), and a radius pad, returns the index of the FIRST disc
 * (other than the skipped one) whose inflated circle contains/overlaps the
 * query point, or -1 (NONE) if none do.
 *
 * Per disc, using the obstacle-set layout shared with FUN_00062020 /
 * FUN_00062680 (base+2 = int16 disc_count; disc[] base at +8, stride 0x18;
 * disc +8 = float x, +0xc = float y, +0x10 = float radius):
 *     dx = disc.x - position_xy[0]
 *     dy = disc.y - position_xy[1]
 *     sum_radius = disc.radius + radius
 *   overlap when  NOT( sum_radius^2 < dy^2 + dx^2 )
 * i.e. the disc is returned when its inflated radius squared is >= the squared
 * centre distance.  Loop scans discs in order and returns the first match.
 *
 * FPU order is disassembly-authoritative: sum_radius is disc.radius + radius,
 * the subtractions are disc-field MINUS position, and the distance term is
 * evaluated dy*dy + dx*dx (matching the decompile's fVar4*fVar4 + fVar3*fVar3);
 * the comparison primitive is `<`.  disc_count is re-read from memory each
 * iteration exactly as the original does.
 *
 * Bounds assert (c:\halo\source\ai\path.h:0x18c): disc index in
 * [0, disc_count) and disc_count <= MAXIMUM_DISC_COUNT (0x80); the assert
 * macro is display_assert + system_exit(-1) (do NOT lift as hcf).
 *
 * ABI: cdecl, 4 stack args, short return (disc index or -1).
 */
short FUN_00062410(void *obstacles, short disc_index_skip, float *position_xy,
                   float radius)
{
  char *base;
  short i;
  short disc_count;
  char *disc;
  float sum_radius;
  float dx;
  float dy;

  base = (char *)obstacles;
  disc_count = *(short *)(base + 2);
  i = 0;
  if (disc_count > 0) {
    do {
      if (i != disc_index_skip) {
        if (i < 0 || disc_count <= i || disc_count > 0x80) {
          display_assert("disc_index>=0 && disc_index<obstacles->disc_count && "
                         "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                         "c:\\halo\\source\\ai\\path.h", 0x18c, 1);
          system_exit(-1);
        }
        disc = base + 8 + i * 0x18;
        sum_radius = *(float *)(disc + 0x10) + radius;
        dx = *(float *)(disc + 8) - position_xy[0];
        dy = *(float *)(disc + 0xc) - position_xy[1];
        if (!(sum_radius * sum_radius < dy * dy + dx * dx)) {
          return i;
        }
      }
      disc_count = *(short *)(base + 2);
      i = i + 1;
    } while (i < disc_count);
  }
  return -1;
}

/* 0x0062680 — FUN_00062680
 *
 * Given an obstacle-disc set (obstacles), a shared radius pad (arg2, an
 * IEEE-754 float smuggled through a uint32_t stack slot — the ported caller
 * forwards it as an opaque dword), a seed disc index, and an output bitvector,
 * marks every disc reachable from the seed by "inflated-circle overlap"
 * connectivity.
 *
 * Two discs A and B are connected when the squared centre distance is <= the
 * squared sum of their pad-inflated radii:
 *     dx = B.x - A.x ; dy = B.y - A.y
 *     dx*dx + dy*dy <= ((pad + B.radius) + (pad + A.radius))^2
 * The search is an iterative worklist flood-fill seeded at seed_disc_index; the
 * result is a 1-bit-per-disc membership bitvector (index>>5 dword,
 * 1<<(index&31) bit).
 *
 * Obstacle-set layout (byte offsets from the base pointer, disasm-confirmed):
 *   +0x02  int16  disc_count           (0 <= disc_count <= 0x80)
 *   +0x08  disc[] base, stride 0x18 (24 bytes)
 *     disc +0x08 float x
 *     disc +0x0c float y
 *     disc +0x10 float radius
 * MAXIMUM_DISC_COUNT = 0x80 = 128.
 *
 * FPU order is disassembly-authoritative (0x17a-0x1ad): the inflated-radius sum
 * evaluates the candidate term first, then the current term; the distance
 * squared evaluates dx*dx before dy*dy.  Preserve the parenthesisation for
 * VC71.
 *
 * ABI: cdecl, 4 stack args, void return.
 */
void FUN_00062680(int16_t *partition, uint32_t arg2, int16_t index,
                  uint32_t *out_mask)
{
  uint32_t *mask_word;
  int cand_base;
  float inflated_sum;
  float dx;
  float dy;
  int top_prev;
  int16_t disc_idx;
  int i;
  int16_t disc_count;
  uint32_t bit;
  int16_t stack[128];
  int cur_base;
  int top;
  int base;

  base = (int)partition;

  if ((*(int16_t *)(base + 2) < 0) || (0x80 < *(int16_t *)(base + 2))) {
    display_assert(
      "obstacles->disc_count>=0 && obstacles->disc_count<=MAXIMUM_DISC_COUNT",
      "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x183, 1);
    system_exit(-1);
  }
  csmemset(out_mask, 0, ((*(int16_t *)(base + 2) + 0x1f) >> 5) << 2);
  if (index != -1) {
    if ((index < 0) || (*(int16_t *)(base + 2) <= index)) {
      display_assert(
        "seed_disc_index>=0 && seed_disc_index<obstacles->disc_count",
        "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x18c, 1);
      system_exit(-1);
    }
    mask_word = &out_mask[(int)index >> 5];
    stack[0] = index;
    *mask_word = *mask_word | 1 << ((uint8_t)index & 0x1f);
    i = 1;
    do {
      i = i + -1;
      disc_idx = stack[(int16_t)i];
      top = i;
      if (((disc_idx < 0) || (*(int16_t *)(base + 2) <= disc_idx)) ||
          (0x80 < *(int16_t *)(base + 2))) {
        display_assert("disc_index>=0 && disc_index<obstacles->disc_count && "
                       "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                       "c:\\halo\\source\\ai\\path.h", 0x18c, 1);
        system_exit(-1);
      }
      disc_count = *(int16_t *)(base + 2);
      cur_base = base + 8 + disc_idx * 0x18;
      disc_idx = 0;
      if (0 < disc_count) {
        do {
          i = (int)disc_idx;
          bit = 1 << ((uint8_t)disc_idx & 0x1f);
          mask_word = &out_mask[i >> 5];
          if ((*mask_word & bit) == 0) {
            if (((disc_idx < 0) || (disc_count <= disc_idx)) ||
                (0x80 < disc_count)) {
              display_assert(
                "disc_index>=0 && disc_index<obstacles->disc_count && "
                "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                "c:\\halo\\source\\ai\\path.h", 0x18c, 1);
              system_exit(-1);
            }
            top_prev = top;
            cand_base = base + 8 + i * 0x18;
            inflated_sum =
              (*(float *)&arg2 + *(float *)(base + 0x18 + i * 0x18)) +
              (*(float *)&arg2 + *(float *)(cur_base + 0x10));
            dx = *(float *)(cand_base + 8) - *(float *)(cur_base + 8);
            dy = *(float *)(cand_base + 0xc) - *(float *)(cur_base + 0xc);
            if (dx * dx + dy * dy <= inflated_sum * inflated_sum) {
              disc_count = (int16_t)top;
              *mask_word = *mask_word | bit;
              if (0x7f < disc_count) {
                display_assert("stack_top<MAXIMUM_DISC_COUNT",
                               "c:\\halo\\SOURCE\\ai\\path_obstacles.c", 0x1a5,
                               1);
                system_exit(-1);
              }
              stack[disc_count] = disc_idx;
              top = top_prev + 1;
            }
          }
          disc_count = *(int16_t *)(base + 2);
          disc_idx = disc_idx + 1;
          i = top;
        } while (disc_idx < disc_count);
      }
    } while (0 < (int16_t)i);
  }
  return;
}

/* FUN_000628b0 (0x628b0)  --  cluster_partition_assign_groups
 * (cluster_partitions.c)
 *
 * Partitions the elements of a cluster-partition set into connected groups.
 * The set header is two shorts: [0] = running group-id counter (reset to 0
 * here and bumped once per new group), [1] = element count.  Each element is
 * 0xc shorts (0x18 bytes) wide; the short at element-index 5 (byte 0xa) holds
 * the assigned group id, with -1 meaning "unassigned".
 *
 * Pass 1 marks every element unassigned.  Pass 2 walks the elements; for each
 * still-unassigned element it allocates a new group id, calls FUN_00062680 to
 * produce a 128-bit membership bitset (out param), then stamps the new group id
 * into every element whose bit is set.
 *
 * ABI: cdecl, 2 stack args, void return (RET, no N).  The single call to
 * FUN_00062680 pushes ESI(partition), EDX(arg2), EBX(i), ECX(&mask) and is
 * followed by ADD ESP,0x10 => 4 dword args.  The local frame is only 0x10 bytes
 * (the 4-dword bitset); Ghidra's auStackY_1014[1017] is a hallucination and is
 * omitted.
 */
void FUN_000628b0(int16_t *partition, uint32_t arg2)
{
  int16_t group_id;
  int16_t i;
  int16_t j;
  uint32_t mask[4];

  partition[0] = 0;

  /* Pass 1: mark all elements unassigned. */
  group_id = 0;
  if (partition[1] > 0) {
    do {
      partition[group_id * 0xc + 5] = -1;
      group_id = group_id + 1;
    } while (group_id < partition[1]);
  }

  /* Pass 2: assign a group id to each connected component. */
  i = 0;
  if (partition[1] > 0) {
    do {
      if (partition[i * 0xc + 5] == -1) {
        group_id = partition[0];
        partition[0] = group_id + 1;
        FUN_00062680(partition, arg2, i, mask);
        j = 0;
        if (partition[1] > 0) {
          do {
            if ((mask[(int)j >> 5] & (1 << ((uint8_t)j & 0x1f))) != 0) {
              partition[j * 0xc + 5] = group_id;
            }
            j = j + 1;
          } while (j < partition[1]);
        }
      }
      i = i + 1;
    } while (i < partition[1]);
  }
}

/* FUN_00099070 (0x99070)
 *
 * Debug overlay for the decal render queue and per-cluster decal labels.
 * Runs only when the global debug flag at 0x5aa8b4 is set.
 *
 * Part 1 (decal queue): for each queue in [0, *0x4547da), walk its
 * *0x453fda[queue] elements.  Elements are 0x18-byte records in three
 * parallel arrays: point_a base 0x44dfc0, point_b base 0x44dfd8, and a
 * flag byte base 0x44dfec, all indexed by (element + base)*0x18.  For each
 * element FUN_00189270 draws the edge (running point) -> point_b in the
 * global color at 0x2ee6e0, then FUN_00189150 draws point_b as a 0.0625f
 * point using color 0x2ee6d0 (flag set) or 0x2ee6c4 (flag clear).  After
 * each queue a duplicate-surface-index scan over 0x4547dc reports an error.
 *
 * Part 2 (cluster labels): for each rendered cluster in [0, *0x5137cc), and
 * each of its 5 layers, walk the decal linked list (next handle at
 * record+0x34, -1 terminates) resolved through the decals pool *0x5aa8b8;
 * label each decal with its surface index (record+0x2a, sign-extended and
 * <<2) drawn at record+8.
 *
 * Confirmed from decompile at 0x99070:
 *   - loop bound *0x4547da is re-read after every error() and each inner draw.
 *   - FUN_00189150 scale arg is float 0.0625f (0x3d800000), passed by value.
 *   - sprintf vararg is (int)(short)*(record+0x2a) << 2.
 *   - the linked-list "next" is read from the resolved record pointer +0x34.
 * ABI: cdecl, no args, void return.
 */
void FUN_00099070(void)
{
  char local_50[64];
  int base_element;
  int queue_index;
  short queue_count_g;
  short count;
  short e;
  short dup;
  int byte_off;
  float *point_a;
  float *point_b;
  void *color;
  int ri;
  void *cluster;
  short cluster_id;
  short layer;
  int node;
  char *rec;

  if (*(char *)0x5aa8b4 == 0)
    return;

  base_element = 0;
  queue_index = 0;
  queue_count_g = *(short *)0x4547da;
  if (queue_count_g > 0) {
    do {
      count = ((short *)0x453fda)[(short)queue_index];
      e = 0;
      point_a = (float *)(0x44dfc0 + (count + (short)base_element) * 0x18);
      if (count > 0) {
        do {
          byte_off = (e + (short)base_element) * 0x18;
          point_b = (float *)(0x44dfd8 + byte_off);
          FUN_00189270(1, point_a, point_b, *(void **)0x2ee6e0);
          color = *(void **)0x2ee6d0;
          /* flag byte lives at 0x44dfec + byte_off (== point_b + 0x14); both
           * are byte reads, so the [LOADW-WARN] is a benign addressing-encoding
           * diff. */
          if (*(char *)(0x44dfec + byte_off) == 0)
            color = *(void **)0x2ee6c4;
          FUN_00189150(1, point_b, 0.0625f, color);
          e = e + 1;
          point_a = point_b;
          queue_count_g = *(short *)0x4547da;
        } while (e < ((short *)0x453fda)[(short)queue_index]);
      }
      dup = 0;
      if (queue_count_g > 0) {
        do {
          if (dup != (short)queue_index &&
              ((short *)0x4547dc)[(short)queue_index] ==
                ((short *)0x4547dc)[dup]) {
            error(2, "### ERROR decals: duplicate surface indices in queue -- "
                     "tell Bernie!!");
            queue_count_g = *(short *)0x4547da;
          }
          dup = dup + 1;
        } while (dup < queue_count_g);
      }
      base_element = base_element + count;
      queue_index = queue_index + 1;
    } while ((short)queue_index < queue_count_g);
  }

  ri = 0;
  if (*(short *)0x5137cc > 0) {
    do {
      cluster = rendered_cluster_get(ri);
      cluster_id = *(short *)cluster;
      layer = 0;
      do {
        node = FUN_00098fe0(cluster_id, layer);
        while (node != -1) {
          rec = (char *)datum_get(*(void **)0x5aa8b8, node);
          crt_sprintf(local_50, "%d", (int)*(short *)(rec + 0x2a) << 2);
          FUN_00189cb0(0, rec + 8, local_50, *(int *)0x2ee6d0);
          node = *(int *)(rec + 0x34);
        }
        layer = layer + 1;
      } while ((short)layer < 5);
      ri = ri + 1;
    } while ((short)ri < *(short *)0x5137cc);
  }
}

/* FUN_00099220 (0x99220)
 *
 * Determine the dominant axis of a plane normal.  Returns the index
 * (0=x, 1=y, 2=z) of the component with the largest absolute value.
 */
uint32_t FUN_00099220(float *plane)
{
  float ax = plane[0] < 0.0f ? -plane[0] : plane[0];
  float ay = plane[1] < 0.0f ? -plane[1] : plane[1];
  float az = plane[2] < 0.0f ? -plane[2] : plane[2];

  if (ay <= az && ax <= az)
    return 2;
  if (ay < ax)
    return 0;
  return 1;
}

/* FUN_00099270 (0x99270)
 *
 * Return 1 if the plane normal component at the given projection axis
 * is positive, 0 otherwise.
 */
uint8_t FUN_00099270(float *plane, uint32_t basis)
{
  assert_halt((int16_t)basis >= 0 && (int16_t)basis <= 2);
  if (plane[basis] > 0.0f)
    return 1;
  return 0;
}

/* 0x1056e0 — Dispose of a sphere geometry object.
 * Asserts the sphere handle and its two allocated arrays (vertices at +0x4,
 * triangle_strip_vertex_indices at +0x8) are non-NULL, then frees the two
 * arrays followed by the sphere structure itself.
 * Source: c:\halo\SOURCE\math\geometry.c (lines 0x75-0x7b). */
void FUN_001056e0(void *handle)
{
  if (handle == 0) {
    display_assert("sphere", "c:\\halo\\SOURCE\\math\\geometry.c", 0x75, 1);
    halt_and_catch_fire();
  }
  if (*(int *)((char *)handle + 4) == 0) {
    display_assert("sphere->vertices", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x76, 1);
    halt_and_catch_fire();
  }
  if (*(int *)((char *)handle + 8) == 0) {
    display_assert("sphere->triangle_strip_vertex_indices",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x77, 1);
    halt_and_catch_fire();
  }
  debug_free(*(void **)((char *)handle + 4),
             "c:\\halo\\SOURCE\\math\\geometry.c", 0x79);
  debug_free(*(void **)((char *)handle + 8),
             "c:\\halo\\SOURCE\\math\\geometry.c", 0x7a);
  debug_free(handle, "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b);
}

/* FUN_001057a0 (0x1057a0)
 *
 * Signed-distance evaluation of a 2D point against a plane2d
 * (normal.x, normal.y, distance).  Computes the 2D dot product of the
 * first two components of param_1 and param_2, then subtracts the third
 * component of param_1: (p1[0]*p2[0] + p1[1]*p2[1]) - p1[2].
 * Pure x87 leaf; operand order preserved to match FADD/FSUBP ordering.
 */
float FUN_001057a0(float *param_1, float *param_2)
{
  return (param_1[0] * param_2[0] + param_1[1] * param_2[1]) - param_1[2];
}

/* FUN_001057c0 (0x1057c0)
 *
 * 2D parametric line-intersection solve.  Given plane2d param_1 and param_2
 * (each normal.x, normal.y with param_1 carrying a distance at +0x8) and a
 * shared point/direction param_3 (x at +0x0, y at +0x4, offset at +0x8),
 * returns the negated ratio of two signed 2D evaluations:
 *   num = param_1[0]*param_3[0] + param_1[1]*param_3[1] - param_3[2]
 *   den = param_2[0]*param_3[0] + param_2[1]*param_3[1]
 *   return -(num / den)
 * Pure x87 leaf; single-expression form keeps intermediates in ST(0) to
 * match the FLD/FMUL/FADDP/FSUB/FDIVP/FCHS chain (FSUB not FSUBR: the
 * subtrahend is param_3[2]; FDIVP yields num/den; FCHS negates).
 * EAX holds param_3 throughout; ECX switches from param_1 to param_2.
 */
float FUN_001057c0(float *param_1, float *param_2, float *param_3)
{
  return -(((param_1[0] * param_3[0] + param_1[1] * param_3[1]) - param_3[2]) /
           (param_2[0] * param_3[0] + param_2[1] * param_3[1]));
}

/* FUN_001057f0 (0x1057f0)
 *
 * 3D ray/plane parametric-t solve (3D twin of FUN_001057c0).  param_3 is a
 * plane3d (normal.x/y/z at +0x0/+0x4/+0x8, distance at +0xc); param_1 and
 * param_2 are 3D vectors:
 *   num = param_1[0]*param_3[0] + param_1[1]*param_3[1] + param_1[2]*param_3[2]
 * - param_3[3] den = param_2[0]*param_3[0] + param_2[1]*param_3[1] +
 * param_2[2]*param_3[2] return -(num / den) Pure x87 leaf; single-expression
 * form keeps intermediates in ST(0) to match the FLD/FMUL/FADDP/FSUB/FDIVP/FCHS
 * chain (FSUB not FSUBR: the subtrahend is param_3[3]; FDIVP yields num/den;
 * FCHS negates).  EAX holds param_3 throughout; ECX switches from param_1 to
 * param_2.
 */
float FUN_001057f0(float *param_1, float *param_2, float *param_3)
{
  return -((((param_1[0] * param_3[0] + param_1[1] * param_3[1] +
              param_1[2] * param_3[2]) -
             param_3[3]) /
            (param_2[0] * param_3[0] + param_2[1] * param_3[1] +
             param_2[2] * param_3[2])));
}

/* 0x105980 — Build a ring/cylinder (torus-like) mesh.
 * Sweeps ring_segment_count rings around a cross-section of
 * cylinder_segment_count segments. For each ring the ring angle drives a
 * cos/sin pair (scaled by param_8) that offsets a cross-section built by
 * rotating a base radius (param_10) about the ring normal, then transforms
 * each vertex by the caller's matrix.
 * Outputs: vertex positions (out_positions, stride 3 floats), tex coords
 * (out_texcoords, stride 2 floats), a triangle-strip index buffer
 * (out_indices), the emitted vertex count (*out_vertex_count) and the number
 * of strip index-runs (*out_index_run_count).
 * The ring normal is the cross product of the cross-section direction
 * (cos*param_8, sin*param_8, 0) with the global axis vector at *0x31fc44,
 * normalized when its length is >= the epsilon at 0x2533d0.
 * Constants: 0x255a54 = 6.2831855f (2*pi), 0x2533c0 = 0.0f, 0x2533c8 = 1.0f,
 * 0x2533d0 = double epsilon. Asserts at geometry.c:0x15a/0x15b.
 * Source: c:\halo\SOURCE\math\geometry.c:346 */
void FUN_00105980(float *matrix, short *out_vertex_count,
                  short *out_index_run_count, float *out_positions,
                  float *out_texcoords, short *out_indices,
                  short ring_segment_count, float param_8,
                  int cylinder_segment_count, float param_10)
{
  float fVar1, fVar2, fVar4, angle;
  float fVar9, fVar10, fVar11, fVar12, fVar_sin;
  int iVar5;
  float *pfVar6;
  float *pfVar7;
  short sVar8;
  /* normal[0]=local_38, normal[1]=local_34, normal[2]=local_30; the three
   * must be contiguous+ascending because &normal[0] is passed as the axis
   * argument to rotate_vector3d_by_sincos (stack-aliasing hazard). */
  float normal[3];
  int local_2c;
  float local_28, local_24;
  int local_20, local_1c, local_18, local_14, local_10, local_c, local_8;

  local_1c = 0;
  local_8 = 0;
  if (ring_segment_count <= 2) {
    display_assert("ring_segment_count>2", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x15a, 1);
    system_exit(-1);
  }
  if ((short)cylinder_segment_count <= 2) {
    display_assert("cylinder_segment_count>2",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x15b, 1);
    system_exit(-1);
  }
  local_10 = 0;
  if (ring_segment_count < 0) {
    *out_vertex_count = 0;
    *out_index_run_count = 0;
    return;
  }
  local_20 = (int)ring_segment_count;
  local_18 = 0;
  local_24 = (float)local_20;
  pfVar6 = *(float **)0x0031fc44;
  pfVar7 = out_texcoords;
  do {
    fVar9 = (float)local_18 / local_24;
    angle = *(float *)0x00255a54 * fVar9;
    fVar10 = x87_fcos(angle);
    fVar1 = fVar10 * param_8;
    fVar11 = x87_fsin(angle);
    fVar2 = param_8 * fVar11;
    /* ring normal = (fVar1, fVar2, 0) x axis[]; 0x2533c0 == 0.0f.
     * normal[2]=A, normal[1]=B, normal[0]=C; length sum is (C^2+B^2)+A^2 to
     * match the original's x87 add order. */
    normal[2] = fVar1 * pfVar6[1] - fVar2 * pfVar6[0];
    normal[1] = pfVar6[0] * *(float *)0x002533c0 - fVar1 * pfVar6[2];
    normal[0] = fVar2 * pfVar6[2] - pfVar6[1] * *(float *)0x002533c0;
    fVar4 = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] +
                  normal[2] * normal[2]);
    if (fabsf(fVar4) >= (float)*(double *)0x002533d0) {
      fVar4 = *(float *)0x002533c8 / fVar4;
      normal[0] = normal[0] * fVar4;
      normal[1] = normal[1] * fVar4;
      normal[2] = fVar4 * normal[2];
    }
    sVar8 = 0;
    if (-1 < (short)cylinder_segment_count) {
      local_c = (int)(short)cylinder_segment_count;
      local_14 = (local_8 - cylinder_segment_count) + -1;
      local_28 = (float)(fVar9 + fVar9);
      do {
        pfVar7[1] = local_28;
        if (0 < (short)local_10) {
          if (sVar8 == 0) {
            *out_indices = (short)cylinder_segment_count * 2 + 2;
            out_indices = out_indices + 1;
            local_1c = local_1c + 1;
          }
          *out_indices = (short)local_8;
          out_indices[1] = (short)local_14;
          out_indices = out_indices + 2;
        }
        if ((short)local_10 == ring_segment_count) {
          /* last ring: copy the vertex/texcoord from the first ring */
          iVar5 = (local_c + 1) * local_20;
          pfVar6 = out_positions + iVar5 * -3;
          *out_positions = *pfVar6;
          out_positions[1] = pfVar6[1];
          out_positions[2] = pfVar6[2];
          *out_texcoords = out_texcoords[iVar5 * -2];
          pfVar7 = out_texcoords;
        } else {
          local_2c = (int)sVar8;
          fVar9 = (float)local_2c / (float)local_c;
          *pfVar7 = (float)(fVar9 + fVar9);
          if (sVar8 == (short)cylinder_segment_count) {
            /* seam: copy from the start of this ring */
            pfVar6 = out_positions + local_c * -3;
            *out_positions = *pfVar6;
            out_positions[1] = pfVar6[1];
            out_positions[2] = pfVar6[2];
          } else {
            angle = *(float *)0x00255a54 * fVar9;
            fVar12 = x87_fcos(angle);
            *out_positions = fVar10 * param_10;
            out_positions[1] = fVar11 * param_10;
            out_positions[2] = 0.0f;
            fVar_sin = x87_fsin(angle);
            rotate_vector3d_by_sincos(out_positions, normal, fVar_sin, fVar12);
            *out_positions = fVar1 + *out_positions;
            out_positions[2] = out_positions[2];
            out_positions[1] = fVar2 + out_positions[1];
            matrix_transform_point(matrix, out_positions, out_positions);
          }
        }
        pfVar7 = pfVar7 + 2;
        out_positions = out_positions + 3;
        local_8 = local_8 + 1;
        local_14 = local_14 + 1;
        sVar8 = sVar8 + 1;
        pfVar6 = *(float **)0x0031fc44;
        out_texcoords = pfVar7;
      } while (sVar8 <= (short)cylinder_segment_count);
    }
    local_10 = local_10 + 1;
    local_18 = local_18 + 1;
  } while ((short)local_10 <= ring_segment_count);
  *out_vertex_count = (short)local_8;
  *out_index_run_count = (short)local_1c;
}

/* 0x105d20 — Reduce a 2D point set to its convex hull as an index list.
 * Gift-wrapping (Jarvis march). shell_update (called with the vertex array in
 * EBX) validates that at least three non-collinear points exist (returns 2);
 * otherwise nothing is emitted and 0 is returned.
 *   Phase 1: pick the start vertex (lowest y, then leftmost x) with an epsilon
 *            tie-break (1e-4f) on both axes.
 *   Phase 2: from the current vertex, atan2(dy,dx) angle scan against a running
 *            angle base, wrapping candidate angles into [-1e-4f, ...) by adding
 *            2*pi; keep the minimum-angle vertex, append its index, and stop
 *            when the chosen vertex closes back on the first. A collinear/
 *            degenerate guard uses a double epsilon (=(double)1e-4f) on the
 *            |component delta| between the chosen and first vertices.
 *   Phase 3: reached only when the walk fills all slots (index_count reaches
 *            vertex_count); compacts a trailing duplicate run to the front with
 *            three bounds asserts (geometry.c 0x279,0x27a,0x282).
 * param_1 = vertex_count, param_2 = float[2] vertex array (x,y; 8-byte stride),
 * param_3 = int16 output index list. Returns the emitted index count in AX.
 * Source: c:\halo\SOURCE\math\geometry.c */
int16_t convex_hull2d_reduce(int16_t vertex_count, float *vertices,
                             int16_t *out_indices)
{
  int16_t index_count;

  index_count = 0;
  if (shell_update(vertex_count, vertices) == 2) {
    float base_angle;
    float best_x;
    float best_y;
    int16_t start_index;
    int16_t current_index;
    int16_t next_index;
    float min_angle;
    int16_t collinear_flag;
    int16_t i;
    int16_t first;
    float *p;
    float *ref;

    base_angle = 0.0f; /* FLOAT_002533c0 = 0.0f, running gift-wrap base */
    best_x = 3.4028235e38f; /* FLT_MAX */
    best_y = 3.4028235e38f;
    start_index = -1; /* SI default = low word of FLT_MAX (dead: count>0) */
    collinear_flag = 0;

    /* Phase 1: lowest y, then leftmost x, with epsilon tie-break. */
    if (vertex_count > 0) {
      p = vertices + 1; /* &vertices[0].y */
      for (i = 0; i < vertex_count; i = i + 1) {
        if ((p[0] < best_y - 1e-4f) ||
            ((p[0] < best_y) && (p[-1] < best_x + 1e-4f)) ||
            ((p[0] < best_y + 1e-4f) && (p[-1] < best_x - 1e-4f))) {
          best_x = p[-1];
          best_y = p[0];
          start_index = i;
        }
        p = p + 2;
      }
    }

    current_index = start_index;
    next_index =
      start_index; /* EBX default (dead: inner loop always assigns) */
    for (;;) {
      min_angle = 3.4028235e38f; /* FLT_MAX reset (0x105de9) */
      if (index_count >= vertex_count) {
        goto compaction;
      }
      out_indices[index_count] = current_index;
      index_count = index_count + 1;

      /* Phase 2: min-angle gift-wrap scan. */
      if (vertex_count > 0) {
        ref = vertices + current_index * 2;
        p = vertices;
        for (i = 0; i < vertex_count; i = i + 1) {
          if ((p[0] != ref[0]) || (p[1] != ref[1])) {
            float angle;

            angle = x87_fatan2f(p[1] - ref[1], p[0] - ref[0]) - base_angle;
            if (angle < -1e-4f) {
              do {
                angle = angle + 6.2831855f; /* 2*pi wrap */
              } while (angle < -1e-4f);
            }
            if (angle < min_angle) {
              min_angle = angle;
              next_index = i;
            }
          }
          p = p + 2;
        }
      }

      base_angle = base_angle + min_angle;
      current_index = next_index;

      first = out_indices[0];
      if (collinear_flag == 0) {
        if ((fabs(vertices[next_index * 2] - vertices[first * 2]) >= 1e-4f) ||
            (fabs(vertices[next_index * 2 + 1] - vertices[first * 2 + 1]) >=
             1e-4f)) {
          collinear_flag = 1;
        }
      }

      first = out_indices[0];
      if (next_index == first) {
        return index_count;
      }
      if (collinear_flag == 0) {
        continue;
      }
      if ((fabs(vertices[next_index * 2] - vertices[first * 2]) >= 1e-4f) ||
          (fabs(vertices[next_index * 2 + 1] - vertices[first * 2 + 1]) >=
           1e-4f)) {
        continue;
      }
      return index_count;
    }

  compaction: {
    int16_t last_hull;
    int16_t search;
    int16_t k;

    search = index_count - 2;
    if (search <= 0) {
      goto assert_start_positive;
    }
    last_hull = out_indices[index_count - 1];
    for (;;) {
      if (out_indices[search] == last_hull) {
        int16_t new_count;

        new_count = (index_count - 1) - search;
        index_count = new_count;
        if (new_count > 0) {
          int src;
          int16_t *psrc;
          int16_t *pdst;

          src = search;
          psrc = out_indices + search;
          pdst = out_indices;
          k = 0;
          do {
            if (vertex_count <= k) {
              display_assert("vertex_index<vertex_count",
                             "c:\\halo\\SOURCE\\math\\geometry.c", 0x279, 1);
              system_exit(-1);
            }
            if (vertex_count <= src) {
              display_assert("start_vertex_index+vertex_index<vertex_count",
                             "c:\\halo\\SOURCE\\math\\geometry.c", 0x27a, 1);
              system_exit(-1);
            }
            k = k + 1;
            *pdst = *psrc;
            psrc = psrc + 1;
            pdst = pdst + 1;
            src = src + 1;
          } while (k < new_count);
        }
        if (search > 0) {
          return index_count;
        }
        goto assert_start_positive;
      }
      search = search - 1;
      if (search < 1) {
        goto assert_start_positive;
      }
    }
  }

  assert_start_positive:
    display_assert("start_vertex_index>0", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x282, 1);
    system_exit(-1);
  }
  return index_count;
}

/* FUN_00106030 (0x106030)
 *
 * Validate a 2D polygon (given as an index list into a shared vertex array)
 * for convexity and a closed interior-angle sum.  Vertices are 2D (x, y
 * pairs, stride 8 bytes); param_4 is a short[] index array, param_2 the
 * vertex-array base, param_3 the vertex count.  For each vertex it forms the
 * two incident edge vectors (cur-prev, next-cur), rejects the polygon
 * (return 0) if the signed 2D cross product is below a threshold (reflex /
 * wrong-winding vertex), and otherwise accumulates the angle between the
 * edges (via FUN_0010c440).  Succeeds (return 1) only if the accumulated
 * angle matches the expected total within a tolerance.
 *
 * Confirmed from disassembly at 0x106030:
 *   - cdecl; param_1 (EBP+0x8) is unused but preserved for stack ABI.
 *   - Returns int 0/1 in EAX (the Ghidra CONCAT22 early-return is XOR AL,AL).
 *   - prev index uses a real branch (wrap to count-1); next index uses a
 *     branchless SETcc/SBB/AND wrap to 0 (asymmetry preserved).
 *   - cross = edge1.y*edge0.x - edge1.x*edge0.y (FSUBP ST1-ST0, verified).
 *   - FUN_0010c440(&edge0, &edge1): 2 float* args, angle returned in ST0;
 *     kept in the x87 stack across the counter increment (no round-trip).
 *   - 0x2533c0 threshold (float, '<'); 0x255a54 expected sum (float FSUB);
 *     0x2549d8 tolerance (loaded as DOUBLE via FCOMP m64).
 */
int FUN_00106030(void *param_1, int param_2, short param_3, int param_4)
{
  float edge0[2]; /* local_1c=x, local_18=y : cur - prev  (contiguous) */
  float edge1[2]; /* local_14=x, local_10=y : next - cur  (contiguous) */
  float cross; /* fVar3 */
  float sum; /* local_8 : accumulated interior angle */
  int i; /* local_c : loop counter (16-bit compared) */
  int cur16; /* iVar5 = (short)i */
  int prev_idx; /* iVar4 (prev), reused for next */
  int cur_idx;
  int next_idx;
  float *prev; /* pfVar1 */
  float *cur; /* pfVar2 */
  float *next; /* pfVar1 reused */

  (void)param_1;

  sum = 0.0f;
  i = 0;
  if (param_3 > 0) {
    do {
      cur16 = (int)(short)i;
      prev_idx = cur16 - 1;
      if (prev_idx < 0)
        prev_idx = param_3 - 1;
      prev = (float *)(param_2 + (int)*(short *)(param_4 + prev_idx * 2) * 8);
      cur_idx = (int)*(short *)(param_4 + cur16 * 2);
      cur = (float *)(param_2 + cur_idx * 8);
      edge0[0] = cur[0] - prev[0];
      edge0[1] = cur[1] - prev[1];
      next_idx = ((param_3 <= cur16 + 1) - 1) & (cur16 + 1);
      next = (float *)(param_2 + (int)*(short *)(param_4 + next_idx * 2) * 8);
      edge1[0] = next[0] - cur[0];
      edge1[1] = next[1] - cur[1];
      cross = edge1[1] * edge0[0] - edge1[0] * edge0[1];
      if (cross < *(float *)0x2533c0)
        return 0;
      sum = FUN_0010c440(edge0, edge1) + sum;
      i = i + 1;
    } while ((short)i < param_3);
  }
  if (fabsf(sum - *(float *)0x255a54) < *(double *)0x2549d8)
    return 1;
  return 0;
}

/* FUN_00106130 (0x106130)
 *
 * Test whether a query point lies within a given radius of a 2D convex
 * polygon.  Points are 2D (x, y pairs, stride 8 bytes).  Uses the
 * cross-product sign to check sidedness; if the point is outside any
 * edge beyond the radius, returns false.
 */
bool FUN_00106130(uint16_t point_count, void *points, void *query_point,
                  float radius)
{
  float *pts = (float *)points;
  float *qp = (float *)query_point;
  int16_t i;
  float radius_sq = radius * radius;

  if ((int16_t)point_count <= 0)
    return true;

  for (i = 0; i < (int16_t)point_count; i++) {
    int idx = (int)i;
    int next = (idx + 1 < (int)(int16_t)point_count) ? idx + 1 : 0;
    float ex, ey, edge_len_sq, dx, dy, cross;

    ex = pts[next * 2] - pts[idx * 2];
    ey = pts[next * 2 + 1] - pts[idx * 2 + 1];
    edge_len_sq = ex * ex + ey * ey;

    if (edge_len_sq == 0.0f)
      continue;

    dx = qp[0] - pts[idx * 2];
    dy = qp[1] - pts[idx * 2 + 1];
    cross = dx * ey - dy * ex;

    if (cross <= 0.0f)
      continue;

    if (cross * cross < edge_len_sq * radius_sq)
      continue;

    return false;
  }
  return true;
}

/* FUN_00106200 (0x106200)
 *
 * 2D point-in-polygon winding test with epsilon tolerance.
 * Tests whether a query point lies inside a 2D polygon by checking the
 * cross-product of each edge against the query point.  Points are stored
 * as float[2] pairs (x, y).  For every edge (vert[i] -> vert[next]),
 * computes:  cross = (point.y - vert_i.y) * (next.x - vert_i.x)
 *                  - (point.x - vert_i.x) * (next.y - vert_i.y)
 * If cross < -epsilon for any edge, the point is outside and returns false.
 * The wrap-around index uses: next = (i+1 >= count) ? 0 : i+1.
 */
bool FUN_00106200(int16_t count, void *points, float *query_point,
                  float epsilon)
{
  int16_t i;
  float neg_epsilon;
  float *qp;
  float *pts;

  i = 0;
  if (count <= 0)
    return true;

  neg_epsilon = -epsilon;
  qp = (float *)query_point;
  pts = (float *)points;

  do {
    int idx = (int)i;
    int next = (idx + 1 >= (int)count) ? 0 : idx + 1;
    float ex, ey, dx, dy;

    ex = pts[next * 2] - pts[idx * 2];
    ey = pts[next * 2 + 1] - pts[idx * 2 + 1];
    dx = qp[0] - pts[idx * 2];
    dy = qp[1] - pts[idx * 2 + 1];

    if (dy * ex - dx * ey < neg_epsilon)
      return false;

    i++;
  } while (i < count);

  return true;
}

/* FUN_00106290 (0x106290)
 *
 * Indexed variant of the 2D point-in-polygon winding test (FUN_00106200).
 * Instead of iterating vertices directly, the polygon boundary is described
 * by an index array (int16 indices, stride 2) into a shared vertex pool
 * (float[2] per vertex, x at +0, y at +4).  For every edge
 * (vert[index[i]] -> vert[index[next]]) computes:
 *   cross = (P.y - A.y) * (B.x - A.x) - (P.x - A.x) * (B.y - A.y)
 * and returns 0 as soon as cross < -epsilon for any edge (point is outside
 * that edge beyond the tolerance).  Returns 1 if the point passes every edge.
 * Empty polygon (count <= 0) returns 1.  Wrap-around index uses
 * next = (i+1 >= count) ? 0 : i+1.  Indices are loaded narrow (int16, signed);
 * the cross-product/subtraction order matches FUN_00106200 and disassembly.
 */
int FUN_00106290(int16_t count, void *index_array, void *vertex_base,
                 float *query_point, float epsilon)
{
  int16_t i;
  float neg_epsilon;
  int16_t *indices;
  float *verts;
  float *qp;

  i = 0;
  if (count <= 0)
    return 1;

  neg_epsilon = -epsilon;
  indices = (int16_t *)index_array;
  verts = (float *)vertex_base;
  qp = query_point;

  do {
    int idx = (int)i;
    int next = (idx + 1 >= (int)count) ? 0 : idx + 1;
    int a = (int)indices[idx];
    int b = (int)indices[next];
    float *A = &verts[a * 2];
    float ax = A[0], ay = A[1];
    float bx = verts[b * 2];
    float by = verts[b * 2 + 1];
    float dx = qp[0] - ax;
    float dy = qp[1] - ay;
    float ex = bx - ax;
    float ey = by - ay;

    if (dy * ex - dx * ey < neg_epsilon)
      return 0;

    i++;
  } while (i < count);

  return 1;
}

/* FUN_00106330 (0x106330)
 *
 * 2D polygon signed-area accumulation (triangle-fan / shoelace) returning the
 * absolute area. Points are stored as float[2] pairs (x, y) with stride 2;
 * vertex 0 (points[0], points[1]) is the fan anchor. For each triangle
 * (anchor, cur = points[i], next = points[i+1]), i running over count-2
 * iterations, accumulates half the 2D cross product of the two edge vectors
 * measured from the anchor:
 *   cross = (next.y - anchor.y) * (cur.x - anchor.x)
 *         - (next.x - anchor.x) * (cur.y - anchor.y)
 *   area += 0.5 * cross
 * Returns fabs(area).
 *
 * Confirmed from disasm: seed FLOAT_002533c0 = 0.0f, scale _DAT_00253398 =
 * 0.5f (both single-precision, byte-verified). Cross-product operand order is
 * A*B - C*D (hazard #4, verified against FSUBP direction). x87 extended
 * intermediates are preserved as double; do not reassociate. Loop count is
 * unsigned 16-bit ((unsigned short)(count - 2)); guarded by count > 2. Leaf,
 * cdecl, result left in ST0 with a trailing FABS.
 */
float FUN_00106330(int16_t count, float *points)
{
  float area;
  float *p;
  unsigned int n;

  area = 0.0f; /* FLOAT_002533c0 seed */
  if (count > 2) {
    n = (unsigned int)(unsigned short)(count - 2);
    p = points + 2;
    do {
      n = n - 1;
      /* Signed area of triangle (anchor, cur, next), doubled; scaled by 0.5.
       * Reference computes (next.x-x0)*(cur.y-y0) - (next.y-y0)*(cur.x-x0);
       * result is negated vs the standard fan cross but fabs() absorbs the
       * sign. MSVC schedules this as a pairwise x87 multiply. */
      area = ((p[2] - points[0]) * (p[1] - points[1]) -
              (p[3] - points[1]) * (p[0] - points[0])) *
               0.5f /* _DAT_00253398 */
             + area;
      p = p + 2;
    } while (n != 0);
  }
  return (float)fabs(area); /* FABS */
}

/* FUN_0018e420 (0x18e420)
 *
 * Returns the global BSP3D pointer (DAT_005064d8). Asserts with a halt if
 * the pointer has not been initialized (i.e. is NULL). Called by BSP
 * traversal and portal-intersection code to obtain the current structure
 * BSP3D tag data.
 *
 * Confirmed: no parameters (plain MOV EAX,[global]; TEST; RET).
 * Confirmed: assert string "global_bsp3d", file scenario.c, line 0xd5.
 */
void *FUN_0018e420(void)
{
  if (*(void **)0x5064d8 == NULL) {
    display_assert("global_bsp3d", "c:\\halo\\SOURCE\\scenario\\scenario.c",
                   0xd5, true);
    system_exit(-1);
  }
  return *(void **)0x5064d8;
}

/* Remove a value from a reference list linked through a datum array (0x1913c0).
 * Walks the list starting at *head, finds the datum whose +4 field matches
 * value, calls datum_delete to free it, and unlinks it. */
void reference_list_remove(data_t *data, int *head, int value)
{
  int *current_ptr = head;

  while (*current_ptr != -1) {
    char *datum = (char *)datum_get(data, *current_ptr);
    if (*(int *)(datum + 4) == value) {
      datum_delete(data, *current_ptr);
      *current_ptr = *(int *)(datum + 8);
      return;
    }
    current_ptr = (int *)(datum + 8);
  }

  display_assert(
    csprintf((char *)0x5ab100,
             "attempt to remove invalid element %ld from reference list",
             value),
    "..\\objects\\reference_lists.h", 0x6d, 1);
  system_exit(-1);
}

/* reference_list_copy (0x191440) — Copy a reference_list's entries from source
 * into result. Both lists must have identical size and maximum_count
 * (asserted).
 *
 * For each slot in [0, maximum_count): if the source entry is live (its first
 * word != 0) the 12-byte (3-dword) entry is copied verbatim; otherwise, if the
 * result entry is live, it is removed via datum_delete(result, index).
 *
 * Struct offsets (reference_lists.h):
 *   +0x20 maximum_count (int16)   +0x22 size (int16)   +0x34 entry array ptr
 * Entry stride = 0xc (12 bytes) = 3 dwords; both pointers advance by 0xc/iter.
 *
 * Confirmed (disasm): cdecl(result @EBP+8 = EDI, source @EBP+0xc = EBX);
 * size mismatch asserts at line 0x88 (136), maximum_count mismatch at line 0x89
 * (137), reason strings shown, halt=1, then system_exit(-1). The loop count
 * [result+0x20] is RE-READ from memory each iteration (CMP SI,word[EDI+0x20]),
 * and the index is a signed int16 (JL). datum_delete push order is EDI then
 * MOVSX-of-SI => datum_delete(result, (int)index). No FPU. */
void reference_list_copy(void *result, void *source)
{
  short *result_entry;
  short *source_entry;
  short index;

  if (*(short *)((char *)result + 0x22) != *(short *)((char *)source + 0x22)) {
    display_assert("result->size==source->size",
                   "..\\objects\\reference_lists.h", 0x88, 1);
    system_exit(-1);
  }
  if (*(short *)((char *)result + 0x20) != *(short *)((char *)source + 0x20)) {
    display_assert("result->maximum_count==source->maximum_count",
                   "..\\objects\\reference_lists.h", 0x89, 1);
    system_exit(-1);
  }

  result_entry = *(short **)((char *)result + 0x34);
  source_entry = *(short **)((char *)source + 0x34);
  index = 0;
  if (0 < *(short *)((char *)result + 0x20)) {
    do {
      if (*source_entry == 0) {
        if (*result_entry != 0) {
          datum_delete((data_t *)result, (int)index);
        }
      } else {
        *(int *)result_entry = *(int *)source_entry;
        *(int *)(result_entry + 2) = *(int *)(source_entry + 2);
        *(int *)(result_entry + 4) = *(int *)(source_entry + 4);
      }
      index = index + 1;
      result_entry = result_entry + 6;
      source_entry = source_entry + 6;
    } while (index < *(short *)((char *)result + 0x20));
  }
}

/* Allocate the three cluster-partition globals (0x191500).
 * out[0] = game_state_malloc("<name>"... , "cluster references", 0x800) -- the
 *          per-cluster head block (0x800 bytes).
 * out[1] = game_state_data_new("<name> reference", 0x800, 0xc) built from the
 *          intermediate "cluster <name>" name buffer.
 * out[2] = game_state_data_new("<name> reference", 0x800, 0xc) built from the
 *          intermediate "<name> cluster" name buffer.
 * If any allocation fails, error(0, "couldn't allocate %s cluster partition
 * globals", name). Confirmed (disasm 0x191500-0x1915cf): cdecl(out @EBP+8=ESI,
 * name @EBP+0xc=EDI); two 256-byte scratch buffers (local_204/local_104, frame
 * SUB ESP,0x200, no _chkstk); crt_sprintf/game_state_malloc/game_state_data_new
 * all cdecl (first push = last arg). Failure test order out[0], out[2], out[1]
 * matches the TEST sequence; the last data_new result is held for that test. */
void cluster_partition_globals_new(void **out, const char *name)
{
  char buffer[256];
  char name_buffer[256];
  void *references;

  out[0] = game_state_malloc(name, "cluster references", 0x800);
  crt_sprintf(name_buffer, "cluster %s", name);
  crt_sprintf(buffer, "%s reference", name_buffer);
  out[1] = game_state_data_new(buffer, 0x800, 0xc);
  crt_sprintf(name_buffer, "%s cluster", name);
  crt_sprintf(buffer, "%s reference", name_buffer);
  references = game_state_data_new(buffer, 0x800, 0xc);
  out[2] = references;
  if (out[0] == 0 || references == 0 || out[1] == 0) {
    error(0, "couldn't allocate %s cluster partition globals", name);
  }
}

/* Clear a cluster partition (0x1915d0).
 * Resets the per-cluster head array (partition[0], 0x800 bytes) to the empty
 * sentinel (-1), then empties both datum pools: the per-object cluster
 * references (partition[2]) first, then the per-cluster object references
 * (partition[1]). Callee order and struct offsets confirmed from disassembly.
 */
void cluster_partition_clear(void *partition)
{
  int **part = (int **)partition;

  csmemset(part[0], -1, 0x800);
  data_delete_all((data_t *)part[2]);
  data_delete_all((data_t *)part[1]);
}

/* Dispose both datum pools of a cluster partition (0x191600).
 * Mirrors cluster_partition_clear's pool layout: the per-object cluster
 * references (partition[2]) are disposed first, then the per-cluster object
 * references (partition[1]). Each pool is a data_t whose signature byte at
 * +0x24 is non-zero only while allocated; disposal is skipped otherwise.
 * Callee (data_make_invalid) and disposal order confirmed from disassembly.
 */
void cluster_partition_dispose(void *partition)
{
  data_t **part = (data_t **)partition;

  if (*((char *)part[2] + 0x24) != '\0') {
    data_make_invalid(part[2]);
  }
  if (*((char *)part[1] + 0x24) != '\0') {
    data_make_invalid(part[1]);
  }
}

/* Null a cluster partition's three references (0x191630).
 * Zeroes the head-array pointer (partition[0]), the per-object cluster
 * references pool (partition[2]), then the per-cluster object references pool
 * (partition[1]) -- same [2]-before-[1] pool order as the clear/dispose
 * helpers. Each store is guarded by a test against 0 (if (f != 0) f = 0);
 * this conditional-store shape is preserved verbatim from the original.
 */
void cluster_partition_null_references(int *partition)
{
  if (partition[0] != 0) {
    partition[0] = 0;
  }
  if (partition[2] != 0) {
    partition[2] = 0;
  }
  if (partition[1] != 0) {
    partition[1] = 0;
  }
}

int cluster_partition_iter_next(void *partition, int *state)
{
  if (*state != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)partition + 4), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* Seed-and-advance a cluster iterator (0x191690).
 * Stores cluster_handle into *out_cluster unconditionally; if the handle is
 * valid (!= -1), fetches the cluster reference datum from the pool at
 * cluster_list+8, overwrites *out_cluster with the next link (ref+8), and
 * returns the reference value (ref+4). Returns -1 for an exhausted handle.
 */
int FUN_00191690(void *cluster_list, int *out_cluster, int cluster_handle)
{
  *out_cluster = cluster_handle;
  if (cluster_handle != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)cluster_list + 8), cluster_handle);
    *out_cluster = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* Advance a cluster iterator over the pool at offset +8 (0x1916d0).
 * Advance-only counterpart to FUN_00191690 (which seeds *state first). If the
 * current handle (*state) is exhausted (-1) returns -1; otherwise fetches the
 * reference datum from the pool at partition+8, advances *state to the next
 * link (ref+8), and returns the reference value (ref+4).
 */
int FUN_001916d0(int partition, int *state)
{
  if (*state != -1) {
    char *cluster_reference = datum_get(*(void **)(partition + 8), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* Copy a cluster partition (0x191700).
 * Both operands are 3-pointer structs (see cluster_partition_add_object):
 *   [0] -> cluster head array (one dword per cluster)
 *   [1] -> reference_list (per-cluster object references)
 *   [2] -> reference_list (per-object cluster references)
 * The head array holds scenario->0x134 (cluster tag_block count) dwords, so
 * csmemcpy moves count*4 bytes (the <<2 converts the dword count to bytes).
 * Copy order: element [2] BEFORE [1] -- preserve for codegen match.
 */
void cluster_partition_copy(void **destination, void **source)
{
  void *scenario = scenario_get();

  csmemcpy(destination[0], source[0], *(int *)((char *)scenario + 0x134) << 2);
  reference_list_copy(destination[2], source[2]);
  reference_list_copy(destination[1], source[1]);
}

/* Add an object to a cluster partition (0x1917a0).
 * Finds all clusters overlapping position+radius via structure_find_in_cluster,
 * then for each cluster: allocates a per-object cluster reference
 * (partition[2]) linking into *first_cluster_ref, and a per-cluster object
 * reference (partition[1]) linking into the cluster head array (partition[0]).
 */
void cluster_partition_add_object(void *partition, int object_handle,
                                  void *first_cluster_ref, void *position,
                                  uint32_t radius_fp, void *location)
{
  int **part = (int **)partition;
  int *first_ref = (int *)first_cluster_ref;
  short *pos = (short *)position;
  char *loc = (char *)location;
  short local_clusters[64];
  uint16_t cluster_bsp_index;
  union {
    uint32_t u;
    float f;
  } rad;
  int16_t cluster_count;

  assert_halt(partition);
  assert_halt(first_cluster_ref);
  assert_halt(*first_ref == -1);
  assert_halt(position);
  assert_halt(location);

  cluster_bsp_index = *(uint16_t *)(loc + 4);
  rad.u = radius_fp;

  cluster_count = structure_find_in_cluster(cluster_bsp_index, (float *)pos,
                                            rad.f, 0x40, local_clusters);

  if (cluster_count > 0x40) {
    error(2, "an object or light spanned %d clusters.", (int)cluster_count);
    cluster_count = 0x40;
  }

  {
    int i;
    short *cluster_ptr = local_clusters;
    for (i = 0; i < (int)(uint16_t)cluster_count; i++, cluster_ptr++) {
      short cluster_index = *cluster_ptr;

      {
        data_t *obj_ref_data = (data_t *)part[2];
        int obj_ref_handle = data_new_at_index(obj_ref_data);
        if (obj_ref_handle == -1) {
          error(2, "WARNING: maximum %ss per map (%d) exceeded.", obj_ref_data,
                (int)*(short *)((char *)obj_ref_data + 0x20));
        } else {
          int *obj_ref = (int *)datum_get(obj_ref_data, obj_ref_handle);
          obj_ref[1] = (int)cluster_index;
          obj_ref[2] = *first_ref;
          *first_ref = obj_ref_handle;
        }
      }

      if (cluster_index < 0 ||
          (int)cluster_index >= *(int *)((char *)scenario_get() + 0x134)) {
        display_assert(
          "cluster_index>=0 && "
          "cluster_index<global_structure_bsp_get()->clusters.count",
          "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5, true);
        system_exit(-1);
      }

      {
        int *cluster_head = &part[0][(int)cluster_index];
        data_t *cluster_ref_data = (data_t *)part[1];
        int cluster_ref_handle = data_new_at_index(cluster_ref_data);
        if (cluster_ref_handle == -1) {
          error(2, "WARNING: maximum %ss per map (%d) exceeded.",
                cluster_ref_data,
                (int)*(short *)((char *)cluster_ref_data + 0x20));
        } else {
          int *cluster_ref =
            (int *)datum_get(cluster_ref_data, cluster_ref_handle);
          cluster_ref[1] = object_handle;
          cluster_ref[2] = *cluster_head;
          *cluster_head = cluster_ref_handle;
        }
      }
    }
  }
}

/* Remove an object from a cluster partition (0x1919a0).
 * Walks the per-object cluster reference chain (*first_cluster_ref),
 * and for each entry: reads the cluster index, removes the matching
 * per-cluster object reference via reference_list_remove, frees
 * the per-object datum, then follows the next link. Clears
 * *first_cluster_ref to -1 when done. */
void cluster_partition_remove_object(void *partition, int object_handle,
                                     void *first_cluster_ref)
{
  int **part = (int **)partition;
  int *first_ref = (int *)first_cluster_ref;
  int cursor = *first_ref;

  while (cursor != -1) {
    data_t *obj_ref_data = (data_t *)part[2];
    int *obj_ref = (int *)datum_get(obj_ref_data, cursor);
    short cluster_index = *(short *)((char *)obj_ref + 4);

    datum_delete(obj_ref_data, cursor);

    if (cluster_index < 0 ||
        (int)cluster_index >= *(int *)((char *)scenario_get() + 0x134)) {
      display_assert("cluster_index>=0 && "
                     "cluster_index<global_structure_bsp_get()->clusters.count",
                     "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5,
                     true);
      system_exit(-1);
    }

    {
      int *cluster_head = &part[0][(int)cluster_index];
      reference_list_remove((data_t *)part[1], cluster_head, object_handle);
    }

    cursor = obj_ref[2];
  }

  *first_ref = -1;
}

int cluster_partition_iter_first(void *partition, int *state,
                                 int16_t cluster_idx)
{
  if (cluster_idx < 0 ||
      cluster_idx >= *(int *)((char *)scenario_get() + 0x134)) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<global_structure_bsp_get()->clusters.count",
                   "c:\\halo\\SOURCE\\structures\\cluster_partitions.c", 0xd5,
                   true);
    system_exit(-1);
  }

  *state = *(int *)(*(int *)partition + cluster_idx * 4);
  if (*state != -1) {
    char *cluster_reference =
      datum_get(*(void **)((char *)partition + 4), *state);
    *state = *(int *)(cluster_reference + 8);
    return *(int *)(cluster_reference + 4);
  }

  return -1;
}

/* leaf_map_node_stack_push (FUN_00191ad0, 0x191ad0)
 *
 * Bounds-checked push onto the global leaf-map node stack.  If the stack is
 * already full (count > MAXIMUM_NODE_STACK_COUNT-1 = 0xff), fires the engine
 * assert then system_exit(-1); otherwise stores the node value at
 * node_stack[count] and increments count.
 *
 * Confirmed from disassembly at 0x191ad0:
 *   - node_stack_count : int16 @ 0x4d8e90 (cmpw $0x100 / movswl / incw prove
 *     a 16-bit signed counter, not int32)
 *   - node_stack       : int32[256] @ 0x4d8a90 (0x400 bytes, ends at 0x4d8e90)
 *   - MAXIMUM_NODE_STACK_COUNT = 0x100; guard fires when count >= 0x100.
 *   - display_assert(reason, "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a,
 * 1) then system_exit(-1) (thunk_FUN_001029a0 resolves to system_exit
 * @0x8e2f0).
 *   - Single cdecl stack arg (the pushed node value); kb decl was void(void).
 */
void leaf_map_node_stack_push(int32_t node)
{
  if (*(int16_t *)0x4d8e90 >= 0x100) {
    display_assert("leaf_map_globals.node_stack_count<MAXIMUM_NODE_STACK_COUNT",
                   "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a, 1);
    system_exit(-1);
  }
  *(int32_t *)(0x4d8a90 + *(int16_t *)0x4d8e90 * 4) = node;
  *(int16_t *)0x4d8e90 = *(int16_t *)0x4d8e90 + 1;
}

/* FUN_00191ba0 (0x191ba0)
 *
 * Clears two tag_block members of a structure by resizing each to zero
 * elements.  The structure base is passed as a single cdecl stack argument;
 * the two 0xC-byte tag_blocks (count/address/definition triple) live at
 * base+0x4 and base+0x10.
 *
 * Confirmed from disassembly at 0x191ba0:
 *   - No FPU ops, no branching; two cdecl CALLs to tag_block_resize
 *     (FUN_001b9a90), each: push 0 (count); push block-ptr; call; ADD ESP,8.
 *   - kb decl was void(void); real signature is void f(void *base).
 */
void FUN_00191ba0(void *base)
{
  tag_block_resize((char *)base + 0x4, 0);
  tag_block_resize((char *)base + 0x10, 0);
}

/* leaf_map_mark_portal_designators (FUN_00191cb0, 0x191cb0)
 *
 * structures.obj / c:\halo\SOURCE\structures\leaf_map.c
 *
 * Given a portal index, mark the matching portal-designator entry as visited
 * in each of the portal's two adjacent leaves.
 *
 * The portal record (block at structure+0x10, stride 0x18, index = portal
 * index) carries the two adjacent leaf indices at record offsets +4 and +8
 * (masked with 0x7fffffff to drop the sign/plane bit).  For each of those two
 * leaves (block at structure+0x4, stride 0x18) the leaf's portal_designators
 * block header lives at leaf+0xc (count at [0], stride 4).  That sub-block is
 * scanned for the designator whose (value & 0x7fffffff) equals this portal
 * index; when found, the high bit (0x80000000) is set to mark it.  If no
 * designator references the portal the original asserts
 *   portal_designator_index != leaf->portal_designators.count
 * at leaf_map.c:0x2a1 and halt_and_catch_fire()s.
 *
 * Confirmed from disassembly at 0x191cb0:
 *   - tag_block_get_element is cdecl (block, index, element_size); the two
 *     leaf-block fetches use element_size 0x18, the designator fetch uses 4.
 *   - the inner designator counter is a 16-bit short (sVar5); the (int)cast
 *     truncation is deliberate and preserved for VC71 fidelity.
 *   - the assert-halt path calls halt_and_catch_fire (thunk 0x1029a0); its
 *     0xffffffff argument in the decompile is dead (callee is void(void)).
 * Inferred: field semantics (front/back leaf, "visited" meaning of the high
 * bit) from the leaf_map.c source string; the two-iteration loop and offsets
 * are Confirmed from the disassembly.
 */
void leaf_map_mark_portal_designators(void *structure, uint32_t portal_index)
{
  int *designator_count;
  uint32_t *designator;
  int block_index;
  short designator_index;
  int remaining;
  uint32_t *portal;
  int leaves_block;

  portal = (uint32_t *)tag_block_get_element((char *)structure + 0x10,
                                             portal_index, 0x18);
  leaves_block = (int)structure + 4;
  remaining = 2;
  do {
    portal = portal + 1;
    block_index = (int)tag_block_get_element((void *)leaves_block,
                                             *portal & 0x7fffffff, 0x18);
    designator_count = (int *)(block_index + 0xc);
    designator_index = 0;
    if (0 < *designator_count) {
      block_index = 0;
      do {
        designator =
          (uint32_t *)tag_block_get_element(designator_count, block_index, 4);
        if ((*designator & 0x7fffffff) == portal_index) {
          *designator = *designator | 0x80000000;
          break;
        }
        designator_index = designator_index + 1;
        block_index = (int)designator_index;
      } while (block_index < *designator_count);
    }
    if ((int)designator_index == *designator_count) {
      display_assert("portal_designator_index!=leaf->portal_designators.count",
                     "c:\\halo\\SOURCE\\structures\\leaf_map.c", 0x2a1, 1);
      halt_and_catch_fire();
    }
    remaining = remaining - 1;
  } while (remaining != 0);
}

/* FUN_00191d80 (0x191d80)
 *
 * Fetches an outer tag_block element (block = base+4, index masked to
 * 31 bits, stride 0x18), then scans the nested tag_block located at
 * outer_element+0xc (stride 4). Returns false (0) as soon as any inner
 * element's leading int is non-negative; otherwise returns the low byte
 * of the nested element count.
 *
 * Confirmed from disassembly at 0x191d80:
 *   - two cdecl calls to tag_block_get_element (ADD ESP,0xc each);
 *     push order gives (block, index, element_size).
 *   - the nested block pointer (outer_element+0xc) is held in ESI for
 *     the whole function; *ESI is the element count (int at offset 0).
 *   - inner loop counter is a signed short: INC EDI; MOVSX EAX,DI;
 *     CMP EAX,ECX; JL — kept as `short` so codegen emits the MOVSX.
 *   - both RET sites return AL only: early XOR AL,AL (false), and
 *     fall-through MOV AL,byte ptr [ESI] (low byte of the count).
 */
char FUN_00191d80(int base, unsigned int index)
{
  int *count_block;
  int count;
  int *inner;
  short i;

  count_block = (int *)((int)tag_block_get_element((void *)(base + 4),
                                                   index & 0x7fffffff, 0x18) +
                        0xc);
  count = *count_block;
  i = 0;
  if (0 < count) {
    count = 0;
    do {
      inner = (int *)tag_block_get_element(count_block, count, 4);
      if (-1 < *inner) {
        return 0;
      }
      i = i + 1;
      count = (int)i;
    } while (count < *count_block);
  }
  return *(char *)count_block;
}

/* FUN_00191de0 (0x191de0)
 *
 * Recursive cluster-visibility flood-fill. Given a cluster index, walks that
 * cluster's portal list (nested tag_block at cluster+0xc). For each portal it
 * resolves the connection element (descriptor+0x10) to find the neighbouring
 * cluster: the connection stores its two endpoint cluster indices at +4 and
 * +8; whichever is not the current cluster is the neighbour. It sets the
 * neighbour's bit in the destination bitset and, if that bit was newly set,
 * recurses into the neighbour.
 *
 * Confirmed from disassembly at 0x191de0:
 * - cluster elem = tag_block_get_element(descriptor+4, index&0x7fffffff, 0x18)
 * - portal block header at cluster+0xc; portal element size 4, its *value is
 *   the connection index (masked 0x7fffffff)
 * - connection elem = tag_block_get_element(descriptor+0x10, conn, 0x18)
 * - neighbour = *(conn+4); if neighbour == index, neighbour = *(conn+8)
 * - bit_mask = 1 << (neighbour & 0x1f); word = dst + (neighbour>>5)*4
 * - the loop counter is a signed 16-bit index (MOVSX AX), compared against the
 *   portal count re-read from *(cluster+0xc) each iteration
 * All calls cdecl, args pushed right-to-left; self-recursive.
 */
void FUN_00191de0(int descriptor, int dst, unsigned int cluster_index)
{
  int *portal_block;
  int connection_elem;
  unsigned int *portal;
  unsigned int neighbor;
  unsigned int bit_mask;
  unsigned int *word;
  short i;
  int i_idx;

  portal_block =
    (int *)((char *)tag_block_get_element((void *)(descriptor + 4),
                                          cluster_index & 0x7fffffff, 0x18) +
            0xc);
  i = 0;
  if (0 < *portal_block) {
    i_idx = 0;
    do {
      portal = (unsigned int *)tag_block_get_element(portal_block, i_idx, 4);
      connection_elem = (int)tag_block_get_element((void *)(descriptor + 0x10),
                                                   *portal & 0x7fffffff, 0x18);
      neighbor = *(unsigned int *)(connection_elem + 4);
      if (neighbor == cluster_index) {
        neighbor = *(unsigned int *)(connection_elem + 8);
      }
      bit_mask = 1 << (neighbor & 0x1f);
      word = (unsigned int *)(dst + ((int)neighbor >> 5) * 4);
      if ((bit_mask & *word) == 0) {
        *word = *word | bit_mask;
        FUN_00191de0(descriptor, dst, neighbor);
      }
      i = i + 1;
      i_idx = (int)i;
    } while (i_idx < *portal_block);
  }
  return;
}

/* FUN_00191e90 (0x191e90)
 *
 * Draws a debug triangle-fan outline for one element of a tag block.
 * Fetches the outer tag_block element (block = param_1+0x10, index =
 * low 31 bits of param_2, stride 0x18); the nested tag_block at
 * element+0xc holds the fan vertices (count@0, elements@+4, stride 0xc).
 * The high bit of param_2 selects one of two hard-coded fill colors.
 *
 * For each vertex i in [2, count):
 *   - FUN_00188890 fills the fan triangle (vertex0, vertex(i-1),
 *     vertex(i)) with the selected color.
 *   - FUN_00189270 draws the edge line vertex(i-1)->vertex(i) in the
 *     global debug color pointed to by [0x2ee6d0].
 * Two trailing FUN_00189270 calls close the fan: edge (0,1) and edge
 * (0,count-1).
 *
 * Confirmed from disassembly at 0x191e90:
 *   - Ghidra mis-groups the cdecl args (§7): each tag_block_get_element
 *     cleans only 3 args (ADD ESP,0xc); the surplus pushes belong to the
 *     outer FUN_00188890 (ADD ESP,0x14) / FUN_00189270 (ADD ESP,0x10).
 *   - color select: TEST ESI,0x80000000; SETNZ CL; color = colors+CL*0x10.
 *   - fan index is a signed short widened to int each iteration (INC;MOVSX).
 *   - the global color [0x2ee6d0] is re-read fresh on every call.
 *   - MSVC evaluates call args right-to-left, so the tag_block_get_element
 *     for the higher vertex index fires before the lower one; the arg
 *     order below reproduces that push sequence.
 */
void FUN_00191e90(int param_1, int param_2)
{
  int *verts;
  float colors[8];
  short i;
  int idx;

  verts = (int *)((int)tag_block_get_element((void *)(param_1 + 0x10),
                                             param_2 & 0x7fffffff, 0x18) +
                  0xc);
  colors[0] = 0.1f;
  colors[1] = 0.0f;
  colors[2] = 1.0f;
  colors[3] = 0.0f;
  colors[4] = 0.1f;
  colors[5] = 1.0f;
  colors[6] = 0.0f;
  colors[7] = 0.0f;

  i = 2;
  idx = 2;
  if (2 < *verts) {
    do {
      FUN_00188890(
        1, (float *)tag_block_get_element(verts, 0, 0xc),
        (float *)tag_block_get_element(verts, idx - 1, 0xc),
        (float *)tag_block_get_element(verts, idx, 0xc),
        (void *)((char *)colors +
                 (((unsigned int)param_2 & 0x80000000) != 0) * 0x10));
      FUN_00189270(1, (float *)tag_block_get_element(verts, idx - 1, 0xc),
                   (float *)tag_block_get_element(verts, idx, 0xc),
                   *(void **)0x2ee6d0);
      i = i + 1;
      idx = (int)i;
    } while (idx < *verts);
  }
  FUN_00189270(1, (float *)tag_block_get_element(verts, 0, 0xc),
               (float *)tag_block_get_element(verts, 1, 0xc),
               *(void **)0x2ee6d0);
  FUN_00189270(1, (float *)tag_block_get_element(verts, 0, 0xc),
               (float *)tag_block_get_element(verts, *verts - 1, 0xc),
               *(void **)0x2ee6d0);
}

/* FUN_00191ff0 (0x191ff0)
 *
 * Tag-block iterator. Fetches the tag-block element at
 * (index & 0x7fffffff) with stride 0x18 from the block at base+4,
 * then walks the nested tag-block at element+0xc (count@0, elements@+4),
 * passing each 4-byte element's first dword to FUN_00191e90(base, *elem).
 *
 * Confirmed from disassembly at 0x191ff0:
 * - outer tag_block_get_element(base+4, index & 0x7fffffff, 0x18)
 * - nested block pointer = element + 0xc
 * - inner tag_block_get_element(nested, i, 4)
 * - loop counter is a 16-bit short widened to int each iteration (movsx)
 * All calls cdecl, args pushed right-to-left. No FPU.
 */
void FUN_00191ff0(int base, unsigned int index)
{
  int *count_block;
  int i_idx;
  int *inner;
  short i;

  count_block = (int *)((int)tag_block_get_element((void *)(base + 4),
                                                   index & 0x7fffffff, 0x18) +
                        0xc);
  i = 0;
  if (0 < *count_block) {
    i_idx = 0;
    do {
      inner = (int *)tag_block_get_element(count_block, i_idx, 4);
      FUN_00191e90(base, *inner);
      i = i + 1;
      i_idx = (int)i;
    } while (i_idx < *count_block);
  }
}

/* FUN_001926a0 (0x1926a0)
 *
 * Copies a tag-block bitset from src to dst (word-granular), then for
 * every bit set in the destination bitset invokes FUN_00191de0 to
 * propagate/traverse the corresponding element.
 *
 * Confirmed from disassembly at 0x1926a0:
 * - descriptor at param_1; element count at *(param_1+4)
 * - if src != dst: csmemcpy(dst, src, ((count+0x1f)>>5)<<2) bytes
 *   (word-granular rounded copy of the bitset)
 * - for each bit index i in [0,count) that is set in the dst bitset:
 *     FUN_00191de0(descriptor, dst, i)
 * - count is re-read from *(param_1+4) every iteration (not cached)
 * - returns byte-bool true (AL=1; CONCAT31 high bytes are incidental)
 * All calls cdecl, args pushed right-to-left. No FPU.
 */
unsigned char FUN_001926a0(int descriptor, int src, int dst)
{
  int count;
  int i;

  if (src != dst) {
    csmemcpy((void *)dst, (void *)src,
             (size_t)(((*(int *)(descriptor + 4) + 0x1f) >> 5) << 2));
  }
  count = *(int *)(descriptor + 4);
  i = 0;
  if (0 < count) {
    do {
      if ((*(unsigned int *)(dst + (i >> 5) * 4) & (1 << (i & 0x1f))) != 0) {
        FUN_00191de0(descriptor, dst, i);
      }
      count = *(int *)(descriptor + 4);
      i = i + 1;
    } while (i < count);
  }
  return 1;
}

void structure_detail_objects_initialize(void)
{
  int base;

  base = (int)game_state_malloc("structure detail objects", 0, 0xa430);
  *(int *)(base + 0xa420) = 0;
  *(int *)(base + 0xa424) = 0;
  *(int *)(base + 0xa428) = 0x3f800000; /* 1.0f */
  *(int *)0x4d8ea0 = base;
  *(int *)(base + 0xa42c) = 0;
}

/* FUN_00194360 (0x194360)
 * qsort/bsort comparator. Two cdecl stack args are pointers to records.
 * Reads a signed int16 field at offset +0x10 of each record and orders
 * descending by that field: returns +1 when the second record's field is
 * strictly less than the first's (first sorts earlier), -1 otherwise. The
 * equal case also falls into -1, matching the original (cond*2 - 1) shape.
 */
int FUN_00194360(int param_1, int param_2)
{
  return (unsigned int)(*(short *)(param_2 + 0x10) <
                        *(short *)(param_1 + 0x10)) *
           2 +
         -1;
}

/* FUN_00194380 (0x194380)
 * Collision-BSP point query. Fetches the structure's bsp3d root element
 * (tag_block at param_1+0xb0, element 0, stride 0x60), locates the leaf that
 * contains point param_2 via bsp3d_find_leaf(root, node=0, point), then returns
 * the signed int16 field at +0x8 of the matching leaf element (tag_block at
 * param_1+0xe0, stride 0x10). Returns -1 on the 0xffffffff not-found sentinel.
 *
 * NOTE: Ghidra mis-groups the two calls. MSVC evaluates and pushes
 * bsp3d_find_leaf's 2nd arg (0) and 3rd arg (param_2) BEFORE the inner
 * tag_block_get_element call, so the decompiler folded them into
 * tag_block_get_element as a bogus 5-arg call and showed bsp3d_find_leaf with
 * a single arg. Both callees are proven 3-arg cdecl: 0x19b210 reads only
 * [ebp+8]/[ebp+0xc]/[ebp+0x10], and the third call site is a clean
 * 3-push / add esp,0xc. The nested call form reproduces the push interleave.
 * The leaf index has its high bit stripped (& 0x7fffffff) before use.
 */
int FUN_00194380(int param_1, void *param_2)
{
  void *leaf_element;
  unsigned int leaf;

  leaf = bsp3d_find_leaf(
    tag_block_get_element((void *)(param_1 + 0xb0), 0, 0x60), 0, param_2);
  if (leaf != 0xffffffff) {
    leaf_element = tag_block_get_element((void *)(param_1 + 0xe0),
                                         (int)(leaf & 0x7fffffff), 0x10);
    return (int)*(short *)((char *)leaf_element + 8);
  }
  return -1;
}

/* FUN_00195530 (0x195530)
 * Integer greater-than comparator, likely a qsort/bsearch comparison callback.
 * Two cdecl stack int args, bool/AL return. Returns true only when
 * param_1 > param_2 (the equal case returns false). The two-branch shape
 * (early-return false on param_2 > param_1, then param_2 < param_1) is the
 * original MSVC codegen shape (cmp param_2,param_1 / jle / xor al,al / setl al)
 * and is preserved verbatim; collapsing it to a single param_1 > param_2 would
 * change the emitted branch structure. VC71 recomputes the compare for the
 * trailing setl (clean-bool-via-edx idiom) where the original reused the
 * branch's flags directly, a ~3-insn small-function idiom gap (88.9% VC71).
 */
char FUN_00195530(int param_1, int param_2)
{
  if (param_2 > param_1) {
    return 0;
  }
  return param_2 < param_1;
}

/* 0x195550 - gather structure surfaces selected by a per-32-surface bitmask.
 *
 * Walks the scenario structure-BSP surfaces tag_block at scenario+0xf8 (first
 * int = surface element count). mask is an array of uint bitmask words, one
 * word per 32 surfaces; for each set bit the matching surface index is appended
 * to out_indices and its 6-byte (short[3]) tag element is copied into
 * out_surfaces (stride 6 bytes). surface_count bounds the output write index
 * (asserted).
 *
 * A zero mask word skips an entire block of 0x20 surfaces (surface_index +=
 * 0x20 without touching the tag_block). The loop bound (*count) is re-read
 * every outer iteration - preserved from the original, not cached. The element
 * copy is exactly three 16-bit moves (element_size 6); widths are kept at
 * uint16. */
void FUN_00195550(short surface_count, int *out_indices, uint32_t *mask,
                  int out_surfaces)
{
  int *block;
  int scenario;
  short bit;
  short write_index;
  int surface_index;
  uint16_t *dst;
  uint16_t *elem;

  scenario = (int)scenario_get();
  block = (int *)(scenario + 0xf8);
  write_index = 0;
  surface_index = 0;
  if (0 < *(int *)(scenario + 0xf8)) {
    do {
      if (*mask == 0) {
        surface_index = surface_index + 0x20;
      } else {
        bit = 0;
        do {
          if (*block <= surface_index)
            break;
          if ((*mask & 1 << ((uint8_t)bit & 0x1f)) != 0) {
            elem = (uint16_t *)tag_block_get_element(block, surface_index, 6);
            if ((write_index < 0) || (surface_count <= write_index)) {
              display_assert(
                "surface_index_index>=0 && surface_index_index<surface_count",
                "c:\\halo\\SOURCE\\structures\\structure_render.c", 0x1a5,
                true);
              system_exit(-1);
            }
            *out_indices = surface_index;
            out_indices = out_indices + 1;
            dst = (uint16_t *)(out_surfaces + write_index * 6);
            dst[0] = elem[0];
            dst[1] = elem[1];
            dst[2] = elem[2];
            write_index = write_index + 1;
          }
          bit = bit + 1;
          surface_index = surface_index + 1;
        } while (bit < 0x20);
      }
      mask = mask + 1;
    } while (surface_index < *block);
  }
}

/* FUN_001959f0 (0x1959f0)
 *
 * Structure render-triangle build entry.  Under the profiler gate
 * (0x449ef1 && 0x3275c8), brackets the build call in
 * profile_enter/exit_private("render_structure_build_triangle").  Builds the
 * triangle set via FUN_001956d0(&0x5937d4, &0x5137d0), stashing the returned
 * bsp/structure index at 0x4d8eb4 and a validity flag (index != -1) at
 * 0x4d8eb0.  Then, when the two staged indices (0x3275b8, 0x3275bc) are in
 * range against the scenario tag-block counts at scenario+0x270 / scenario+
 * 0x27c, replays them through the tag-block iterators FUN_00191ff0 /
 * FUN_00191e90 (both operate on scenario+0x26c).  When the rebuild-all byte
 * 0x505703 is set, walks every element of the scenario+0x27c tag-block
 * (stride 0x18, element pointer discarded) and reissues FUN_00191e90 for each.
 * Finally clears 0x4d8eb8 and snapshots the forward vector (3 dwords) from
 * *(0x31fc38) into 0x4d8ebc/ec0/ec4.
 *
 * Confirmed from decompile at 0x1959f0:
 *   - scenario_get() 0-arg; scenario+0x270 guards 0x3275b8, +0x27c guards
 *     0x3275bc and the rebuild loop (element count at *(scenario+0x27c)).
 *   - FUN_001956d0 returns int (EAX -> 0x4d8eb4), two pointer args.
 *   - loop sets index=0 before the count check (preserved), stride 0x18.
 *   - forward-vector snapshot copied as raw dwords (no FPU).
 * All calls cdecl, args pushed right-to-left.
 */
void FUN_001959f0(void)
{
  int scenario;
  int index;
  char *fwd;

  scenario = (int)scenario_get();

  if (*(char *)0x449ef1 != 0 && *(char *)0x3275c8 != 0) {
    profile_enter_private((void *)0x3275c0);
  }
  /* 0x195a18: MOV ESI,[0x5937d0] before the call — count is an implicit
   * @<esi> arg (callee tests SI). */
  *(int *)0x4d8eb4 = FUN_001956d0((void *)0x5937d4, (void *)0x5137d0,
                                  *(int16_t *)0x5937d0);
  if (*(char *)0x449ef1 != 0 && *(char *)0x3275c8 != 0) {
    profile_exit_private((void *)0x3275c0);
  }
  *(char *)0x4d8eb0 = (char)(*(int *)0x4d8eb4 != -1);

  if (-1 < *(int *)0x3275b8 && *(int *)0x3275b8 < *(int *)(scenario + 0x270)) {
    FUN_00191ff0(scenario + 0x26c, *(int *)0x3275b8);
  }
  if (-1 < *(int *)0x3275bc && *(int *)0x3275bc < *(int *)(scenario + 0x27c)) {
    FUN_00191e90(scenario + 0x26c, *(int *)0x3275bc);
  }

  if (*(char *)0x505703 != 0) {
    index = 0;
    if (0 < *(int *)(scenario + 0x27c)) {
      do {
        tag_block_get_element((void *)(scenario + 0x27c), index, 0x18);
        FUN_00191e90(scenario + 0x26c, index);
        index = index + 1;
      } while (index < *(int *)(scenario + 0x27c));
    }
  }

  *(int *)0x4d8eb8 = 0;
  fwd = *(char **)0x31fc38;
  *(int *)0x4d8ebc = *(int *)fwd;
  *(int *)0x4d8ec0 = *(int *)(fwd + 4);
  *(int *)0x4d8ec4 = *(int *)(fwd + 8);
}

/* FUN_00195b10 (0x195b10)
 *
 * render_structure_lightmaps: thin render-orchestration wrapper (string ref
 * "render_structure_lightmaps" @0x327bb8).  Profiles the scope, and when the
 * map has a valid lightmap (byte at 0x4d8eb0 != 0) it briefly forces the 16-bit
 * word at 0x3256b0 to 1 -- only when the scenario has no bsp switch pending
 * (scenario+0xc == -1) and the word is currently 0 -- brackets a rasterizer
 * setup/teardown pair (FUN_0017cc00 / FUN_0017cc40) around the per-surface
 * lightmap draw walk (FUN_00195790), then restores the saved low word.
 *
 * Notes from disasm/decompile:
 *  - 0x3256b0 is a 16-bit word: the save reads the full dword (MOV ESI,dword),
 *    but the conditional set and the restore are word-sized (MOV word,1 /
 *    MOV word,SI), and the "==0" test is a word compare (CMP word,0).  Do NOT
 *    transcribe as a 32-bit store.
 *  - FUN_00195790 takes an @eax pointer (=0x5937d4, surface->material offset
 *    table) plus 6 stack args (confirmed: its decompile uses int *in_EAX as
 *    in_EAX + param_1).  arg1 is the zero-extended uint16 at 0x5937d0.
 */
void FUN_00195b10(void)
{
  int scenario;
  int saved_flag;

  if (*(char *)0x449ef1 != 0 && *(char *)0x327bc0 != 0) {
    profile_enter_private((void *)0x327bb8);
  }

  saved_flag = *(int *)0x3256b0;

  if (*(char *)0x4d8eb0 != 0) {
    scenario = (int)scenario_get();
    if (*(int *)(scenario + 0xc) == -1 && *(short *)0x3256b0 == 0) {
      *(short *)0x3256b0 = 1;
    }
    FUN_0017cc00();
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 (void *)FUN_0017cc10, (void *)FUN_0017cc20, (void *)0x17cc30,
                 0);
    FUN_0017cc40();
    *(short *)0x3256b0 = (short)saved_flag;
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x327bc0 != 0) {
    profile_exit_private((void *)0x327bb8);
  }
}

/* FUN_00195c40 (0x195c40)
 *
 * Sibling of FUN_00195b10: when the map has a valid lightmap pass (byte at
 * 0x4d8eb0 != 0), briefly forces the 16-bit word at 0x3256b0 to 1 -- only when
 * the scenario has no bsp switch pending (scenario+0xc == -1) and the word is
 * currently 0 -- brackets a rasterizer setup/teardown pair (thunks 0x17cda0 /
 * 0x17cde0 -> FUN_00163c40 / FUN_001609a0) around the per-surface draw walk
 * (FUN_00195790), then restores the saved low word.  Unlike FUN_00195b10 this
 * variant has no profiler scope.
 *
 * Confirmed from disassembly at 0x195c40:
 *  - 0x3256b0 is a 16-bit word: the save reads the full dword
 *    (MOV ESI,dword ptr [0x3256b0]), but the conditional set and the restore
 *    are word-sized (MOV word,1 / MOV word,SI), and the "==0" test is a word
 *    compare (CMP word,0).  The save/restore live inside the gate block (the
 *    MOV ESI read is after the JZ), not before it.  Do NOT emit a 32-bit store.
 *  - FUN_00195790 takes an @eax pointer (MOV EAX,0x5937d4 = surface->material
 *    offset table) plus 6 stack args; ADD ESP,0x18 = 6 stack dwords.  Push
 *    order (first push = last C arg): 0 (param_7), 0x17cdd0 (pass_end_cb),
 *    0x17cdc0 (surface_draw_cb), 0x17cdb0 (material_begin_cb), *0x4d8eb4
 *    (lightmap_pass_index), uint16 @0x5937d0 (surface_count).  0x17cdd0 is a
 *    bare label, passed as a raw address.
 */
void FUN_00195c40(void)
{
  int scenario;
  int saved_flag;

  if (*(char *)0x4d8eb0 != 0) {
    saved_flag = *(int *)0x3256b0;
    scenario = (int)scenario_get();
    if (*(int *)(scenario + 0xc) == -1 && *(short *)0x3256b0 == 0) {
      *(short *)0x3256b0 = 1;
    }
    FUN_0017cda0();
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 (void *)FUN_0017cdb0, (void *)FUN_0017cdc0, (void *)0x17cdd0,
                 0);
    FUN_0017cde0();
    *(short *)0x3256b0 = (short)saved_flag;
  }
}

/* FUN_00195d00 (0x195d00)
 *
 * Sibling of FUN_00195d40 (render_structure_reflections): when the map has a
 * valid reflection/lightmap pass (byte at 0x4d8eb0 != 0), brackets a scope
 * enter/exit pair (FUN_0017ce40 / FUN_0017ce60, 1-instr JMP thunks forwarding
 * to FUN_00160bd0 / FUN_00160be0) around the per-surface draw walk
 * (FUN_00195790).  No profiler scope.
 *
 * Confirmed from disassembly at 0x195d00:
 *  - Gate: MOV AL,[0x4d8eb0]; TEST AL,AL; JZ end.  When the byte is 0 the
 *    function does nothing.
 *  - FUN_00195790 takes an @eax pointer (MOV EAX,0x5937d4 = surface->material
 *    offset table -- passed as an ADDRESS, not a deref) plus 6 stack args;
 *    ADD ESP,0x18 = 6 stack dwords.  Push order (first push = last C arg):
 *    0 (param_7), 0 (pass_end_cb), 0x17ce50 (surface_draw_cb), 0
 *    (material_begin_cb), *0x4d8eb4 (lightmap_pass_index), zero-extended uint16
 *    @0x5937d0 (surface_count, XOR ECX,ECX / MOV CX -> unsigned short read).
 *  - Two distinct globals: uint16 count @0x5937d0 vs int[] offsets @0x5937d4.
 *  - The teardown FUN_0017ce60 is reached via a tail-call JMP in the original.
 */
void FUN_00195d00(void)
{
  if (*(char *)0x4d8eb0 != 0) {
    FUN_0017ce40();
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 (void *)0, (void *)FUN_0017ce50, (void *)0, 0);
    FUN_0017ce60();
  }
}

/* FUN_00195d40 (0x195d40)
 *
 * render_structure_reflections: thin render-orchestration wrapper (string ref
 * "render_structure_reflections" @0x3287a8).  Profiles the scope, and when the
 * map has a valid reflection/lightmap pass (byte at 0x4d8eb0 != 0) it brackets
 * a scope enter/exit pair (FUN_00160bf0 / FUN_00160c00, reached in the original
 * via 1-instr JMP thunks at 0x17ce70/0x17ce90) around the per-surface
 * reflection draw walk (FUN_00195790).
 *
 * Notes from disasm/decompile:
 *  - FUN_00195790 takes an @eax pointer (=0x5937d4, surface->material offset
 *    table) plus 6 stack args.  arg1 (surface_count) is the zero-extended
 *    uint16 at 0x5937d0 (MOV CX,word ptr); arg2 (lightmap_pass_index) is the
 *    dword at 0x4d8eb4.  This variant has a single callback: FUN_0017ce80 is
 *    the 5th param (surface_draw_cb); material_begin_cb / pass_end_cb / param_7
 *    are all 0.  (Confirmed via push order: ADD ESP,0x18 = 6 stack dwords.)
 */
void FUN_00195d40(void)
{
  if (*(char *)0x449ef1 != 0 && *(char *)0x3287b0 != 0) {
    profile_enter_private((void *)0x3287a8);
  }

  if (*(char *)0x4d8eb0 != 0) {
    FUN_00160bf0();
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 (void *)0, (void *)FUN_0017ce80, (void *)0, 0);
    FUN_00160c00();
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x3287b0 != 0) {
    profile_exit_private((void *)0x3287a8);
  }
}

/* FUN_00195dc0 (0x195dc0)
 *
 * render_structure_transparent_geo: thin render-orchestration wrapper (string
 * ref "render_structure_transparent_geo" @0x328da0).  Profiles the scope, and
 * when the map has a valid lightmap/material pass (byte at 0x4d8eb0 != 0) it
 * brackets a scope enter/exit pair (thunks 0x17cea0 / 0x17cec0 forwarding to
 * FUN_00160c10 / FUN_00160c20) around the per-surface transparent-geometry draw
 * walk (FUN_00195790).
 *
 * Confirmed from disassembly at 0x195dc0:
 *  - Outer profiler scope: MOV AL,[0x449ef1]; TEST/JZ; MOV AL,[0x328da8];
 *    TEST/JZ around PUSH 0x328da0; CALL profile_enter_private; ADD ESP,4.
 *    Mirrored at the tail with profile_exit_private (PUSH 0x328da0; CALL; POP).
 *  - Middle block gate: MOV AL,[0x4d8eb0]; TEST AL,AL; JZ end.
 *  - FUN_00195790 takes an @eax pointer (MOV EAX,0x5937d4 = surface->material
 *    offset table -- passed as an ADDRESS, not a deref) plus 6 stack args;
 *    ADD ESP,0x18 = 6 stack dwords.  Push order (first push = last C arg):
 *    0x17ceb0 (param_7 -- fn ptr FUN_0017ceb0), 0 (pass_end_cb), 0
 *    (surface_draw_cb), 0 (material_begin_cb), *0x4d8eb4 (lightmap_pass_index),
 *    zero-extended uint16 @0x5937d0 (surface_count, XOR ECX,ECX / MOV CX).
 *  - Two distinct globals: uint16 count @0x5937d0 vs int[] offsets @0x5937d4.
 *    The lone callback (FUN_0017ceb0) rides in the param_7 slot, unlike the
 *    sibling reflection/lightmap passes which use surface_draw_cb.
 * All calls cdecl, args pushed right-to-left.
 */
void FUN_00195dc0(void)
{
  if (*(char *)0x449ef1 != 0 && *(char *)0x328da8 != 0) {
    profile_enter_private((void *)0x328da0);
  }

  if (*(char *)0x4d8eb0 != 0) {
    FUN_0017cea0();
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 (void *)0, (void *)0, (void *)0, (int)FUN_0017ceb0);
    FUN_0017cec0();
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x328da8 != 0) {
    profile_exit_private((void *)0x328da0);
  }
}

/* FUN_00195ec0 (0x195ec0)
 *
 * Two-pass structure lightmap draw driver.  Sibling of FUN_00195b10 /
 * FUN_00195c40 but with no scenario_get / 0x3256b0 save-restore: when the map
 * has a valid lightmap pass (byte at 0x4d8eb0 != 0) it runs the per-surface
 * draw walk (FUN_00195790) twice, once per pass -- FUN_0017cf10(0) then
 * FUN_0017cf10(1) select the pass index -- with a fog emit (FUN_00167920)
 * after each walk.
 *
 * Confirmed from disassembly at 0x195ec0 (delinked/functions/00195ec0.obj):
 *  - Gate: MOV AL,[0x4d8eb0]; TEST AL,AL; JZ end.
 *  - FUN_0017cf10 takes one int arg (PUSH 0 / PUSH 1).  Stack cleanup is
 *    deferred to a single ADD ESP,0x38 at the end (0x38 = 56 = 4 + 24 + 4 + 24:
 *    the two cf10 args plus two 6-stack-arg FUN_00195790 calls).
 *  - FUN_00195790 takes an @eax pointer (MOV EAX,0x5937d4 = surface->material
 *    offset table) plus 6 stack args.  Push order (first push = last C arg):
 *    0 (param_7), 0 (pass_end_cb), 0x17cf20 (surface_draw_cb), 0
 *    (material_begin_cb), *0x4d8eb4 (lightmap_pass_index), zero-extended uint16
 *    @0x5937d0 (surface_count, XOR ECX,ECX / MOV CX read).
 *  - Two distinct globals: uint16 count @0x5937d0 vs int[] offsets @0x5937d4.
 *  - FUN_0017cf20 is passed as a raw callback address, not called here.
 *  - FUN_00167920 (fog emit) takes no args.
 */
void FUN_00195ec0(void)
{
  if (*(char *)0x4d8eb0 != 0) {
    FUN_0017cf10(0);
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 0, (void *)FUN_0017cf20, 0, 0);
    FUN_00167920();
    FUN_0017cf10(1);
    FUN_00195790((int *)0x5937d4, *(unsigned short *)0x5937d0, *(int *)0x4d8eb4,
                 0, (void *)FUN_0017cf20, 0, 0);
    FUN_00167920();
  }
}

/* FUN_00195f30 (0x195f30)
 *
 * render_structure_specular_lights: structure specular-light render entry
 * (string refs "render_structure_specular_lights" @0x329f88 and the outer
 * scope string @0x329990).  Allocates a 0x4000-byte surface-material scratch
 * table on the stack, then either builds a per-gel surface set into it
 * (gel_buffer != 0) or falls back to the globally-built structure surface
 * table at 0x5937d0/0x5937d4 (gel_buffer == 0).  When the resulting
 * lightmap/material index is valid (!= -1), sets up the object, walks the
 * surfaces through FUN_00195790 with a single surface-draw callback
 * (FUN_0017cd70), ends the rasterizer HUD scope, and (only on the gel path)
 * commits the tint factor.  The whole body is bracketed by the outer profiler
 * scope (0x449ef1 && 0x329998); the draw/setup section by the inner scope
 * (0x449ef1 && 0x329f90).
 *
 * Confirmed from disassembly at 0x195f30:
 *   - Frame: _chkstk(0x4004) -> 0x4000-byte scratch @[EBP-0x4004]; its address
 *     is stashed to [EBP-4] in the prologue (used later as the FUN_00195790
 *     @eax argument), and overwritten to the global 0x5937d4 on the fallback
 *     path.  Callee-saved EBX/ESI/EDI hold gel_buffer / surface_count /
 *     material_index across the body.
 *   - gel path: FUN_00197e90(buffer, 0x1000, position, radius, 0, 0, 0,
 *     gel_count, gel_buffer) returns a short (MOVSX AX -> ESI = surface_count);
 *     FUN_001956d0(buffer, 0) returns int (EAX -> EDI = material_index).
 *     Shared ADD ESP,0x2C = 9+2 stack args.
 *   - fallback: material_index = *(int*)0x4d8eb4, surface_count =
 *     (short)*(short*)0x5937d0 (MOVSX), material_offsets = 0x5937d4.
 *   - draw section: FUN_0017cd60(object_handle) [1 arg]; FUN_00195790 takes
 *     @eax = material_offsets ([EBP-4]) plus 6 stack args (surface_count,
 *     material_index, 0, FUN_0017cd70 surface-draw cb, 0, 0) -- shared
 *     ADD ESP,0x1C = 1+6 stack args; _rasterizer_hud_end() via 1-instr JMP
 *     thunk @0x17cd80 -> 0x160970; rasterizer_widget_set_tint_factor gated on
 *     gel_buffer != 0.
 *   - radius is a float passed by value (raw dword push); position is float*.
 * All calls cdecl, args pushed right-to-left.
 */
void FUN_00195f30(int object_handle, float *position, float radius,
                  int gel_count, int gel_buffer)
{
  char buffer[0x4000];
  int *material_offsets;
  short surface_count;
  int material_index;

  material_offsets = (int *)buffer;

  if (*(char *)0x449ef1 != 0 && *(char *)0x329998 != 0) {
    profile_enter_private((void *)0x329990);
  }

  if (gel_buffer != 0) {
    surface_count = FUN_00197e90(buffer, 0x1000, position, radius, 0, 0, 0,
                                 gel_count, gel_buffer);
    material_index = FUN_001956d0(buffer, (void *)0, surface_count);
  } else {
    material_index = *(int *)0x4d8eb4;
    surface_count = *(short *)0x5937d0;
    material_offsets = (int *)0x5937d4;
  }

  if (material_index != -1) {
    if (*(char *)0x449ef1 != 0 && *(char *)0x329f90 != 0) {
      profile_enter_private((void *)0x329f88);
    }
    FUN_0017cd60(object_handle);
    FUN_00195790(material_offsets, surface_count, material_index, 0,
                 (void *)FUN_0017cd70, 0, 0);
    _rasterizer_hud_end();
    if (gel_buffer != 0) {
      rasterizer_widget_set_tint_factor(material_index);
    }
    if (*(char *)0x449ef1 != 0 && *(char *)0x329f90 != 0) {
      profile_exit_private((void *)0x329f88);
    }
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x329998 != 0) {
    profile_exit_private((void *)0x329990);
  }
}

/* FUN_00196060 (0x196060)
 *
 * render_structure_diffuse_lights: structure diffuse-light render entry.  The
 * diffuse sibling of FUN_00195f30 (specular): byte-identical shape, differing
 * only in the diffuse-specific profiler scopes, object-setup/draw callbacks,
 * and HUD-end thunk.  Allocates a 0x4000-byte surface-material scratch table on
 * the stack, then either builds a per-gel surface set into it (gel_buffer != 0)
 * or falls back to the globally-built structure surface table at
 * 0x5937d0/0x5937d4 (gel_buffer == 0).  When the resulting lightmap/material
 * index is valid (!= -1), sets up the object, walks the surfaces through
 * FUN_00195790 with a single surface-draw callback (FUN_0017cc70), commits the
 * tint factor (gel path only), then ends the rasterizer HUD scope.  The whole
 * body is bracketed by the outer profiler scope (0x449ef1 && 0x32a588); the
 * draw/setup section by the inner scope (0x449ef1 && 0x32ab80).
 *
 * Confirmed from disassembly at 0x196060:
 *   - Frame: _chkstk(0x4004) -> 0x4000-byte scratch @[EBP-0x4004]; its address
 *     is stashed to [EBP-4] in the prologue (used later as the FUN_00195790
 *     @eax argument), and overwritten to the global 0x5937d4 on the fallback
 *     path.  Callee-saved EBX/ESI/EDI hold gel_buffer / surface_count /
 *     material_index across the body.
 *   - gel path: FUN_00197e90(buffer, 0x1000, position, radius, 0, 0, 0,
 *     gel_count, gel_buffer) returns a short (MOVSX AX -> ESI = surface_count);
 *     FUN_001956d0(buffer, 0) returns int (EAX -> EDI = material_index).
 *     Shared ADD ESP,0x2C = 9+2 stack args.
 *   - fallback: material_index = *(int*)0x4d8eb4, surface_count =
 *     (short)*(short*)0x5937d0 (MOVSX), material_offsets = 0x5937d4.
 *   - draw section: FUN_0017cc60(object_handle) [1 arg]; FUN_00195790 takes
 *     @eax = material_offsets ([EBP-4]) plus 6 stack args (surface_count,
 *     material_index, 0, FUN_0017cc70 surface-draw cb, 0, 0) -- shared
 *     ADD ESP,0x1C = 1+6 stack args; rasterizer_widget_set_tint_factor
 *     (@0x196139) gated on gel_buffer != 0; FUN_00160930() (HUD end) via
 *     1-instr JMP thunk @0x17cc80.
 *   - radius is a float passed by value (raw dword push); position is float*.
 * All calls cdecl, args pushed right-to-left.
 */
void FUN_00196060(int object_handle, float *position, float radius,
                  int gel_count, int gel_buffer)
{
  char buffer[0x4000];
  int *material_offsets;
  short surface_count;
  int material_index;

  material_offsets = (int *)buffer;

  if (*(char *)0x449ef1 != 0 && *(char *)0x32a588 != 0) {
    profile_enter_private((void *)0x32a580);
  }

  if (gel_buffer != 0) {
    surface_count = FUN_00197e90(buffer, 0x1000, position, radius, 0, 0, 0,
                                 gel_count, gel_buffer);
    material_index = FUN_001956d0(buffer, (void *)0, surface_count);
  } else {
    material_index = *(int *)0x4d8eb4;
    surface_count = *(short *)0x5937d0;
    material_offsets = (int *)0x5937d4;
  }

  if (material_index != -1) {
    if (*(char *)0x449ef1 != 0 && *(char *)0x32ab80 != 0) {
      profile_enter_private((void *)0x32ab78);
    }
    FUN_0017cc60(object_handle);
    FUN_00195790(material_offsets, surface_count, material_index, 0,
                 (void *)FUN_0017cc70, 0, 0);
    if (gel_buffer != 0) {
      rasterizer_widget_set_tint_factor(material_index);
    }
    FUN_00160930();
    if (*(char *)0x449ef1 != 0 && *(char *)0x32ab80 != 0) {
      profile_exit_private((void *)0x32ab78);
    }
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x32a588 != 0) {
    profile_exit_private((void *)0x32a580);
  }
}

/*
 * FUN_00196190 (0x196190) — structures.obj
 *
 * render_structure_shadows: structure shadow render entry.  Sibling of
 * FUN_00196060 (diffuse lights) / FUN_00195f30 (specular): identical shape, but
 * with shadow-specific profiler scopes, NO per-gel branch (always builds the
 * surface set from the passed center/radius/bounds/count/planes), a single
 * surface-draw callback (FUN_0017ccf0), and rasterizer_widget_set_tint_factor
 * as the post-draw thunk (no HUD-end call).  Allocates a 0x4000-byte
 * surface-material scratch table on the stack, builds the shadow surface set
 * via FUN_00197e90, resolves the lightmap/pass index via FUN_001956d0, and when
 * valid (!= -1) walks the surfaces through FUN_00195790.  Outer profiler scope:
 * 0x449ef1 && 0x32b178 ("render_structure_shadows" @0x32b170); draw scope:
 * 0x449ef1 && 0x32b770 ("render_structure_shadows_draw" @0x32b768).
 *
 * Confirmed from disassembly at 0x196190 (delinked obj):
 *   - Frame: _chkstk(0x4000) -> 0x4000-byte scratch @[EBP-0x4000]. Callee-saved
 *     ESI/EDI hold surface_count / pass_index across the body (EBX unused; no
 *     gel branch).  The PUSH ESI / PUSH EDI scheduled after the outer
 *     profile_enter are register SAVES, not call arguments (popped at
 * epilogue); confirmed by ADD ESP,0x2C == 9+2 stack args cleaning both calls
 * below.
 *   - FUN_00197e90(buffer, 0x1000, center, radius, bounds, count, planes, 0, 0)
 *     returns short (MOVSX AX -> ESI = surface_count); FUN_001956d0(buffer, 0)
 *     returns int (EAX -> EDI = pass_index).  Skip draw when pass_index == -1.
 *   - draw section: FUN_00195790 @eax = buffer + 6 stack args (surface_count,
 *     pass_index, 0, FUN_0017ccf0 surface-draw cb, 0, 0) [the 5-byte PUSH at
 *     +0x95 is a reloc-stripped PUSH 0x17ccf0]; then
 *     rasterizer_widget_set_tint_factor(pass_index).  ADD ESP,0x1C == 6+1 args.
 *   - center/bounds/planes are pointers passed through as dwords; radius is a
 *     float by value.  All calls cdecl, args pushed right-to-left.
 */
void FUN_00196190(float *center, float radius_x4, float *bounds6, int count,
                  float *planes6)
{
  char buffer[0x4000];
  short surface_count;
  int pass_index;

  if (*(char *)0x449ef1 != 0 && *(char *)0x32b178 != 0) {
    profile_enter_private((void *)0x32b170);
  }

  surface_count = FUN_00197e90(buffer, 0x1000, center, radius_x4, (int)bounds6,
                               count, (int)planes6, 0, 0);
  pass_index = FUN_001956d0(buffer, (void *)0, surface_count);

  if (pass_index != -1) {
    if (*(char *)0x449ef1 != 0 && *(char *)0x32b770 != 0) {
      profile_enter_private((void *)0x32b768);
    }
    FUN_00195790((int *)buffer, surface_count, pass_index, 0,
                 (void *)FUN_0017ccf0, 0, 0);
    rasterizer_widget_set_tint_factor(pass_index);
    if (*(char *)0x449ef1 != 0 && *(char *)0x32b770 != 0) {
      profile_exit_private((void *)0x32b768);
    }
  }

  if (*(char *)0x449ef1 != 0 && *(char *)0x32b178 != 0) {
    profile_exit_private((void *)0x32b170);
  }
}

void structure_runtime_decals_initialize(void)
{
  *(void **)0x4d8ec8 = game_state_malloc("structure decals", 0, 4);
  if (*(void **)0x4d8ec8 == NULL) {
    display_assert("structure_decals_globals",
                   "c:\\halo\\SOURCE\\structures\\structure_runtime_decals.c",
                   0x1c, true);
    system_exit(-1);
  }
}

/* noinline: original build had this in a separate TU (structure_runtime_decals.c),
 * so callers in structures.c emit real CALLs — keep that shape under VC71 /O2. */
__declspec(noinline) void structure_runtime_decals_initialize_for_new_map(void)
{
  uint8_t *runtime_decal_globals = *(uint8_t **)0x4d8ec8;

  if (runtime_decal_globals == NULL) {
    display_assert("structure_decals_globals",
                   "c:\\halo\\SOURCE\\structures\\structure_runtime_decals.c",
                   0x24, true);
    system_exit(-1);
  }

  *runtime_decal_globals = 0;
}

/*
 * FUN_00196330  (0x196330) — structures.obj
 *
 * Sweeps the current scenario's structure-cluster tag block and deletes
 * permanent decals from clusters flagged for removal.  Guarded by
 * scenario+0x258 (an int) being non-zero.  The cluster tag block has its
 * count word at scenario+0x134 and element stride 0x68.  For each cluster
 * whose int16 field at +0xc is (!= -1) AND whose int16 field at +0xe is
 * (!= 0), decals_delete_permanent_from_cluster is called with the loop index.
 *
 * Confirmed from disassembly 0x196330-0x196391:
 *   - scenario_get() 0-arg (leading PUSH ECX is a local-slot reserve).
 *   - +0x258 is an int guard; early return when zero.
 *   - block pointer = scenario+0x134 passed to tag_block_get_element
 *     (PUSH 0x68; PUSH index; PUSH block; cdecl, ADD ESP,0xc).
 *   - count = uint16 at +0x134; signed compare (MOVSX AX; TEST; JLE).
 *   - element gate: word[elem+0xc] != -1  AND  word[elem+0xe] != 0 (int16).
 *   - decals_delete_permanent_from_cluster((int16_t)index) — cdecl 1 arg;
 *     BX (loop index) compared against count spilled at [EBP-4].
 * All cdecl; no FPU, SEH, or intrinsics.
 */
void FUN_00196330(void)
{
  char *scenario;
  void *block;
  int cluster_count;
  int cluster_index;
  int element_index;

  scenario = (char *)scenario_get();
  if (*(int *)(scenario + 0x258) != 0) {
    block = (void *)(scenario + 0x134);
    cluster_count = (int)*(uint16_t *)block;
    cluster_index = 0;
    if ((int16_t)cluster_count > 0) {
      element_index = 0;
      do {
        char *cluster = tag_block_get_element(block, element_index, 0x68);
        if (*(int16_t *)(cluster + 0xc) != -1 &&
            *(int16_t *)(cluster + 0xe) != 0) {
          decals_delete_permanent_from_cluster((int16_t)cluster_index);
        }
        cluster_index += 1;
        element_index += 1;
      } while ((int16_t)cluster_index < (int16_t)cluster_count);
    }
  }
}

/* structure_runtime_decals_dispose (0x1963b0) — structures.obj
 *
 * Empty no-op in this build: the disassembly is a single RET (C3) with no
 * prologue, stack frame, FPU, memory access, or calls. Preserved as an
 * empty body to keep the address populated and the ABI intact. */
void structure_runtime_decals_dispose(void)
{
}

/* FUN_00198070 (0x198070) — structures.obj
 *
 * Per-frame rebuild of the active cluster's environment sound/geometry list,
 * then for every rendered cluster this frame it evaluates the clipped frustum
 * bounds and builds the camera frustum used to draw that cluster.
 *
 * Runs only when the active-cluster handle at 0x506784 is not -1.
 *
 * Confirmed from disassembly 0x198070-0x19817d:
 *   - scenario_get() called first (result kept in ESI across the whole body);
 *     guard is a dword compare CMP [0x506784],-1 / JZ.
 *   - render_frustum_get_projection_bounds(0x5065a4, proj_bounds) fills a
 *     4-dword buffer whose base is [EBP-0x10] (Ghidra's "local_14".."local_4";
 *     the base is EBP-0x10, NOT EBP-0x14 as the decompiler implied).
 *   - The 4 bounds dwords are scattered into an int16-tagged struct at
 *     EBP-0x864 (word 4 at +0x0, then 8 dwords at +0x4). Scatter map derived
 *     from the raw MOV destination offsets, not the decompiler local names:
 *       +0x04=b[0] +0x08=b[2] +0x0c=b[1] +0x10=b[2]
 *       +0x14=b[1] +0x18=b[3] +0x1c=b[0] +0x20=b[3]
 *   - Global 0x4d8ed8 is set to &cluster_buf (a 0x40-byte stack buffer) BEFORE
 *     csmemset(cluster_buf,0,0x40); tail store order preserved: set global,
 *     word=4, memset, then FUN_00197b00((uint16)*0x506784, sound_list).
 *   - Loop over [0, (int16)*0x5137cc): rendered_cluster_get(i) -> cluster;
 *     tag_block_get_element(scenario+0x134, (int16)*cluster, 0x68) (result
 *     discarded); render_camera_build_clipped_frustum_bounds(0x506550,
 *     cluster+2 shorts, frustum_bounds) (result discarded);
 *     render_camera_build_frustum(0x506550, frustum_bounds, cluster+10 shorts,
 *     1). scenario+0x134 is hoisted into EBX before the loop (ESI is reused as
 *     the cluster pointer) — these are distinct variables in C.
 *   - Signed int16 loop counter/compare (CMP DI,word[0x5137cc] / JL).
 * All cdecl; no FPU subtraction, SEH, or intrinsics.
 */
void FUN_00198070(void)
{
  char *scenario;
  int16_t *cluster;
  int cluster_index;
  float proj_bounds[4];
  float frustum_bounds[4];
  unsigned char cluster_buf[0x40];
  struct {
    int16_t tag;
    int16_t pad;
    float v[8];
  } sound_list;

  scenario = (char *)scenario_get();
  if (*(int *)0x506784 != -1) {
    render_frustum_get_projection_bounds((void *)0x5065a4, proj_bounds);

    sound_list.v[0] = proj_bounds[0]; /* +0x04 */
    sound_list.v[1] = proj_bounds[2]; /* +0x08 */
    sound_list.v[2] = proj_bounds[1]; /* +0x0c */
    sound_list.v[3] = proj_bounds[2]; /* +0x10 */
    sound_list.v[4] = proj_bounds[1]; /* +0x14 */
    sound_list.v[5] = proj_bounds[3]; /* +0x18 */
    sound_list.v[6] = proj_bounds[0]; /* +0x1c */
    sound_list.v[7] = proj_bounds[3]; /* +0x20 */

    *(void **)0x4d8ed8 = cluster_buf;
    sound_list.tag = 4;
    csmemset(cluster_buf, 0, 0x40);
    FUN_00197b00(*(uint16_t *)0x506784, (uint16_t *)&sound_list);

    cluster_index = 0;
    if (*(int16_t *)0x5137cc > 0) {
      do {
        cluster = (int16_t *)rendered_cluster_get(cluster_index);
        tag_block_get_element(scenario + 0x134, *cluster, 0x68);
        render_camera_build_clipped_frustum_bounds(
          (camera_t *)0x506550, (float *)(cluster + 2), frustum_bounds);
        render_camera_build_frustum((camera_t *)0x506550, frustum_bounds,
                                    (float *)(cluster + 10), 1);
        cluster_index += 1;
      } while ((int16_t)cluster_index < *(int16_t *)0x5137cc);
    }
  }
}

/* FUN_00198180 (0x198180) render_structure_visibility:
 *   Per-frame rebuild of the structure BSP visibility bitvectors, followed by
 *   construction of the rendered-cluster list and dispatch to the fast or
 *   legacy cluster-visibility sweep.
 *
 *   scenario = scenario_get(); clusters tag_block at scenario+0x134 (element
 *   size 0x68, count = *(int*)(scenario+0x134)); scenario+0xf8 sizes the
 *   per-portal bitvector.  Per-cluster visibility bitvector at 0x50678c is
 *   memset to 0xffffffff when the active cluster (0x506784) is -1, else 0; the
 *   per-portal bitvector at 0x5137d0 is zeroed.  0x5137cc = rendered-cluster
 *   count (int16), 0x4d8edc = cluster_index -> rendered_cluster_index table
 *   (int16[]).
 *
 *   Register-alias note (verified against disasm 0x198180-0x1983b5): ESI holds
 *   the scenario pointer, but is reused as the loop's bit_index inside the
 *   sweep and reloaded from [EBP-4] afterward.  Kept as two distinct C locals
 *   (scenario / bit_index).  The final structure_bsp record at
 *   tag_block_get_element(clusters,0,0x68)+0x34 selects FUN_001966b0 (!=0, has
 *   precomputed visibility) vs FUN_00196850 (==0, legacy path that emits the
 *   reimport warning).
 *
 *   __FILE__ = c:\halo\SOURCE\structures\structure_visibility.c */
void render_structure_visibility(void)
{
  int scenario;
  int *clusters;
  int bit_index;
  int cluster_index;
  short *cluster_rec;
  void *sound_data;
  char *first_cluster;

  scenario = (int)scenario_get();
  if (*(char *)0x449ef1 != '\0' && *(char *)0x32bd70 != '\0') {
    profile_enter_private((void *)0x32bd68);
  }
  clusters = (int *)(scenario + 0x134);
  csmemset((void *)0x50678c, (*(int *)0x506784 == -1) ? -1 : 0,
           ((*clusters + 0x1f) >> 5) << 2);
  *(short *)0x5937d0 = 0;
  csmemset((void *)0x5137d0, 0, ((*(int *)(scenario + 0xf8) + 0x1f) >> 5) << 2);
  *(short *)0x5137cc = 0;
  FUN_00198070();
  if (*(char *)0x505701 != '\0') {
    *(short *)0x5137cc = 0;
    sound_data = structure_bsp_get_cluster_sound_data(
      (void *)scenario, (int16_t) * (uint16_t *)0x506784);
    csmemcpy((void *)0x50678c, sound_data, ((*clusters + 0x1f) >> 5) << 2);
    if (0 < *clusters) {
      cluster_index = 0;
      bit_index = 0;
      do {
        if ((((unsigned int *)0x50678c)[bit_index >> 5] &
             (1u << (bit_index & 0x1f))) != 0) {
          tag_block_get_element(clusters, bit_index, 0x68);
          if (*(short *)0x5137cc >= 0x80) {
            display_assert(
              "raise MAXIMUM_RENDERED_CLUSTERS",
              "c:\\halo\\SOURCE\\structures\\structure_visibility.c", 0x118, 1);
            system_exit(-1);
          }
          if ((short)cluster_index < 0 || (short)cluster_index >= 0x200) {
            display_assert(
              "cluster_index>=0 && "
              "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
              "c:\\halo\\SOURCE\\structures\\structure_visibility.c", 0x11b, 1);
            system_exit(-1);
          }
          ((short *)0x4d8edc)[bit_index] = *(short *)0x5137cc;
          *(short *)0x5137cc = *(short *)0x5137cc + 1;
          cluster_rec = (short *)rendered_cluster_get(
            ((unsigned short *)0x4d8edc)[bit_index]);
          *cluster_rec = (short)cluster_index;
          render_frustum_get_projection_bounds((void *)0x5065a4,
                                               cluster_rec + 2);
        }
        cluster_index = cluster_index + 1;
        bit_index = (int)(short)cluster_index;
      } while (bit_index < *clusters);
    }
  }
  if (*(char *)0x449ef1 != '\0' && *(char *)0x32bd70 != '\0') {
    profile_exit_private((void *)0x32bd68);
  }
  first_cluster = (char *)tag_block_get_element(clusters, 0, 0x68);
  if (*(int *)(first_cluster + 0x34) == 0) {
    if (*(char *)0x4d8ed0 == '\0') {
      if (0 < *(int *)(scenario + 0xf8)) {
        error(2, "### WARNING: this structure_bsp needs to be reimported for "
                 "new, faster visibility.");
      }
      *(char *)0x4d8ed0 = '\x01';
    }
    FUN_00196850(scenario);
    return;
  }
  FUN_001966b0(scenario);
}

void structures_initialize(void)
{
  structure_detail_objects_initialize();
  structure_runtime_decals_initialize();
}

void structures_initialize_for_new_map(void)
{
  structure_detail_objects_initialize_for_new_map();
  structure_runtime_decals_initialize_for_new_map();
}

/* structures_dispose_from_old_map (0x1983e0): CALL 0x1963a0 + tail-JMP
 * 0x1939c0 in the original — decals first, then detail objects (reverse of
 * the initialize order). Both callees are bare RETs in this build, but the
 * call shape is preserved. */
void structures_dispose_from_old_map(void)
{
  structure_runtime_decals_dispose_from_old_map();
  structure_detail_objects_dispose_from_old_map();
}

/* structures_dispose (0x1983f0): CALL 0x1963b0 + tail-JMP 0x1939d0. */
void structures_dispose(void)
{
  structure_runtime_decals_dispose();
  structure_detail_objects_dispose();
}

/* structures_cluster_marker_begin (0x198400)
 *
 * Asserts that the cluster marker is not already initialized,
 * increments the cluster-marker reference counter, and sets the
 * initialized flag.
 *
 * Confirmed: TEST AL,AL on byte ptr [0x4d92e1].
 * Confirmed: INC dword ptr [0x4d92e4].
 * Confirmed: MOV byte ptr [0x4d92e1], 1.
 */
void structures_cluster_marker_begin(void)
{
  if (*(uint8_t *)0x4d92e1 != 0) {
    display_assert("!structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x103, true);
    system_exit(-1);
  }
  *(uint32_t *)0x4d92e4 += 1;
  *(uint8_t *)0x4d92e1 = 1;
}

bool structure_cluster_unmarked(int16_t cluster_index)
{
  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x10e, true);
    system_exit(-1);
  }

  if (cluster_index < 0 || cluster_index > 0x1ff) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x10f, true);
    system_exit(-1);
  }

  return ((int *)0x4d92e8)[cluster_index] != *(int *)0x4d92e4;
}

int structure_cluster_mark(int16_t cluster_index)
{
  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x11e, true);
    system_exit(-1);
  }

  if (cluster_index < 0 || cluster_index > 0x1ff) {
    display_assert("cluster_index>=0 && "
                   "cluster_index<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x11f, true);
    system_exit(-1);
  }

  if (((int *)0x4d92e8)[cluster_index] != *(int *)0x4d92e4) {
    ((int *)0x4d92e8)[cluster_index] = *(int *)0x4d92e4;
    return 1;
  }

  return 0;
}

/* structures_cluster_marker_end (0x198540)
 *
 * Asserts that the cluster-marker session is currently active (initialized),
 * then clears the initialized flag, ending the session begun by
 * structures_cluster_marker_begin.
 *
 * Confirmed: TEST AL,AL on byte ptr [0x4d92e1] at 0x198545.
 * Confirmed: assert string "structure_globals.cluster_marker_initialized",
 *   __FILE__ "c:\halo\SOURCE\structures\structures.c", line 0x130 (304).
 * Confirmed: MOV byte ptr [0x4d92e1], 0 at 0x198569.
 */
void structures_cluster_marker_end(void)
{
  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x130, true);
    system_exit(-1);
  }
  *(uint8_t *)0x4d92e1 = 0;
}

/* structure_render_surface_from_point_and_leaf (0x198580)
 *
 * Walks the surfaces of one BSP leaf (leaf_index into the scenario's leaf tag
 * block at +0xe0, element 0x10; low 31 bits used as index) looking for one
 * whose material's first field matches material_type.  For each surface whose
 * material reference is valid (!= -1) and matches, it resolves the surface's
 * triangle (indices tag block at +0xf8, element 6 = 3 uint16), finds the
 * geometry material/section (via structure_bsp_find_material_for_surface, which
 * writes out_collection_index / out_geometry_index), fetches the three vertex
 * positions (vertex stride 0x20, vertices base at section+0xf8) into three
 * vec3 buffers, and hands them to FUN_0010d830 (ray/point-vs-triangle test)
 * along with the caller's context and the out_u/out_v pointers.  On the first
 * triangle that passes, it stores the surface index into *out_surface and
 * returns 1; otherwise 0.
 *
 * Confirmed from disassembly at 0x198580:
 *   - param_2 masked with 0x7fffffff before use as a tag-block index (§7
 *     datum/handle low-word extraction).
 *   - CDECL arg mis-group (§7): Ghidra rendered the material lookup as a 5-arg
 *     call + a 1-arg call.  The disasm (6 pushes, two ADD ESP,0xc) proves TWO
 *     3-arg tag_block_get_element calls: first resolves the structure_bsp
 *     reference (scenario+0xb0, index 0, size 0x60); its RESULT is the block
 *     base of the second call (index surface_ref[1], element size 0xc).
 *   - Only surface modes 0 and 1 (short at section+0xb0) proceed to rasterize.
 *   - Return is a bool in AL only (Ghidra's CONCAT31 / (x & 0xffffff00) high
 *     bytes are garbage EAX residue, not part of the value).
 *   - FUN_001935f0 (structure_bsp_find_material_for_surface) is invoked with 4
 *     cdecl args and writes the two short OUT params, which are read back
 *     immediately.
 */
char structure_render_surface_from_point_and_leaf(
  void *render_context, uint32_t leaf_index, int material_type,
  int16_t *out_collection_index, int16_t *out_geometry_index,
  int32_t *out_surface, float *out_u, float *out_v)
{
  void *scenario;
  char *leaf;
  int start;
  int end;
  int surface;
  int *surface_ref;
  void *structure_bsp;
  int *material;
  uint16_t *tri;
  char *collection;
  char *section;
  char *vertices;
  float v0[3];
  float v1[3];
  float v2[3];

  scenario = scenario_get();
  leaf = tag_block_get_element((char *)scenario + 0xe0,
                               (int)(leaf_index & 0x7fffffff), 0x10);
  start = *(int *)(leaf + 0xc);
  end = *(int16_t *)(leaf + 0xa) + start;
  if (end <= start) {
    return 0;
  }

  surface = start;
  do {
    surface_ref = tag_block_get_element((char *)scenario + 0xec, surface, 8);
    if (surface_ref[1] != -1) {
      structure_bsp = tag_block_get_element((char *)scenario + 0xb0, 0, 0x60);
      material = tag_block_get_element(structure_bsp, surface_ref[1], 0xc);
      if (*material == material_type) {
        tri = tag_block_get_element((char *)scenario + 0xf8, *surface_ref, 6);
        structure_bsp_find_material_for_surface(
          scenario, *surface_ref, out_collection_index, out_geometry_index);
        collection = tag_block_get_element((char *)scenario + 0x104,
                                           (int)*out_collection_index, 0x20);
        section = tag_block_get_element(collection + 0x14,
                                        (int)*out_geometry_index, 0x100);
        if (*(int16_t *)(section + 0xb0) == 0 ||
            *(int16_t *)(section + 0xb0) == 1) {
          vertices = *(char **)(section + 0xf8);
          FUN_00180500((float *)(vertices + (uint32_t)tri[0] * 0x20), v0);
          FUN_00180500((float *)(vertices + (uint32_t)tri[1] * 0x20), v1);
          FUN_00180500((float *)(vertices + (uint32_t)tri[2] * 0x20), v2);
          if (FUN_0010d830((float *)render_context, v0, v1, v2, out_u, out_v) !=
              0) {
            *out_surface = *surface_ref;
            return 1;
          }
        }
      }
    }
    surface = surface + 1;
  } while (surface < end);

  return 0;
}

/* Resolve the planar-fog definition index for a BSP cluster/leaf.
 * index == -1 returns the -1 sentinel. When flag == 0 the fog reference is
 * read from the cluster element (+0x134, stride 0x68) at +2; a set top bit
 * indicates indirection through the +0x178 table (stride 0x20). The masked
 * reference indexes the +0x184 block (stride 0x28); its +0x24 field indexes
 * the +0x190 fog-definition block (stride 0x88) whose +0x2c dword is the
 * result. When flag != 0 the global default (FUN_0018e7d0(0)+0xa4) is used. */
int32_t structure_get_planar_fog_definition_index(void *structure_bsp,
                                                  int16_t index, char flag)
{
  uint16_t fog_ref;
  char *element;
  uint16_t *indirect;
  int32_t result = -1;

  if (index == -1) {
    return result;
  }

  element =
    tag_block_get_element((char *)structure_bsp + 0x134, (int)index, 0x68);

  if (flag == '\0') {
    fog_ref = *(uint16_t *)(element + 2);
    if (fog_ref != 0xffff) {
      if ((int16_t)fog_ref < 0) {
        indirect = (uint16_t *)tag_block_get_element(
          (char *)structure_bsp + 0x178, fog_ref & 0x7fff, 0x20);
        fog_ref = *indirect;
      } else {
        fog_ref = fog_ref & 0x7fff;
      }
      if (fog_ref != 0xffff) {
        element = tag_block_get_element((char *)structure_bsp + 0x184,
                                        (int)(int16_t)fog_ref, 0x28);
        if (*(int16_t *)(element + 0x24) != -1) {
          element =
            tag_block_get_element((char *)structure_bsp + 0x190,
                                  (int)*(int16_t *)(element + 0x24), 0x88);
          return *(int32_t *)(element + 0x2c);
        }
      }
    }
  } else {
    element = FUN_0018e7d0(0);
    if (element != 0) {
      return *(int32_t *)(element + 0xa4);
    }
  }
  return result;
}

bool structure_get_planar_fog(void *scenario, int16_t portal_index,
                              float *position, float radius)
{
  uint8_t projected_vertices[1024];
  uint8_t projected_center[8];
  float projected_hit[3];
  char *portal =
    tag_block_get_element((char *)scenario + 0x154, (int)portal_index, 0x40);
  char *structure_bsp = tag_block_get_element((char *)scenario + 0xb0, 0, 0x60);
  float *portal_plane = tag_block_get_element((int *)(structure_bsp + 0xc),
                                              *(int *)(portal + 4), 0x10);
  float plane_distance = position[0] * portal_plane[0] +
                         position[1] * portal_plane[1] +
                         position[2] * portal_plane[2] - portal_plane[3];

  if (fabsf(plane_distance) < radius) {
    float dx = *(float *)(portal + 8) - position[0];
    float dy = *(float *)(portal + 0xc) - position[1];
    float dz = *(float *)(portal + 0x10) - position[2];
    float expanded_radius = radius + *(float *)(portal + 0x14);

    if (dx * dx + dy * dy + dz * dz < expanded_radius * expanded_radius) {
      int portal_plane_index = *(int *)(portal + 4);
      char *bsp3d = FUN_0018e420();
      uint32_t plane_basis;
      uint8_t plane_axis;
      int *portal_vertices = (int *)(portal + 0x34);
      int16_t vertex = 0;

      portal_plane =
        tag_block_get_element((int *)(bsp3d + 0xc), portal_plane_index, 0x10);
      plane_basis = FUN_00099220(portal_plane);
      plane_axis = FUN_00099270(portal_plane, plane_basis);

      projected_hit[0] = -plane_distance * portal_plane[0] + position[0];
      projected_hit[1] = -plane_distance * portal_plane[1] + position[1];
      projected_hit[2] = -plane_distance * portal_plane[2] + position[2];
      FUN_00061df0(projected_hit, plane_basis, plane_axis, projected_center);

      if (*portal_vertices > 0) {
        do {
          FUN_00061df0(tag_block_get_element(portal_vertices, (int)vertex, 0xc),
                       plane_basis, plane_axis,
                       projected_vertices + (int)vertex * 8);
          vertex += 1;
        } while ((int)vertex < *portal_vertices);
      }

      if (FUN_00106130(
            (uint16_t)*portal_vertices, projected_vertices, projected_center,
            sqrtf(radius * radius - plane_distance * plane_distance))) {
        return true;
      }
    }
  }

  return false;
}

int16_t FUN_001989b0(uint16_t cluster_count, float *position, float radius,
                     int max_count, int16_t *out_indices)
{
  void *scenario = scenario_get();
  int16_t current_cluster = (int16_t)cluster_count;
  char *cluster =
    tag_block_get_element((char *)scenario + 0x134, (int)current_cluster, 0x68);
  int remaining_count = max_count - 1;
  int visited_count = 1;

  if ((int16_t)max_count > 0) {
    *out_indices = current_cluster;
    out_indices += 1;
  }

  structure_cluster_mark(cluster_count);

  if (*(int *)(cluster + 0x5c) > 0) {
    int16_t portal_iter = 0;

    do {
      int16_t *portal_index_ptr =
        tag_block_get_element((int *)(cluster + 0x5c), portal_iter, 2);
      int16_t portal_index = *portal_index_ptr;
      int16_t *portal = tag_block_get_element((char *)scenario + 0x154,
                                              (int)portal_index, 0x40);
      int16_t adjacent_cluster = portal[0];

      if (adjacent_cluster == current_cluster) {
        adjacent_cluster = portal[1];
      }

      if (structure_cluster_unmarked(adjacent_cluster) &&
          structure_get_planar_fog(scenario, portal_index, position, radius)) {
        int recurse_count = FUN_001989b0((uint16_t)adjacent_cluster, position,
                                         radius, remaining_count, out_indices);
        visited_count += recurse_count;
        remaining_count -= recurse_count;
        out_indices += (int16_t)recurse_count;
      }

      portal_iter += 1;
    } while ((int)portal_iter < *(int *)(cluster + 0x5c));
  }

  return (int16_t)visited_count;
}

/* structure_clusters_in_cone (0x198ad0)
 *
 * Iterative flood-fill over the scenario's structure clusters, collecting
 * every cluster reachable through portals whose bounding sphere falls inside
 * a view cone, up to max_count output clusters.  This is the cone analog of
 * structure_find_in_cluster / FUN_001989b0 (which flood-fill with a sphere
 * via structure_get_planar_fog); here the portal-vs-cone test is FUN_00110210
 * against the portal bounding sphere (center at portal+8, radius at
 * portal+0x14) and the cone (apex `point`, axis `direction`, `length`, and
 * half-angle sine/cosine).
 *
 * Unlike structure_find_in_cluster, MSVC inlined the cluster-marker begin/end
 * here: the top asserts !cluster_marker_initialized (line 0x103), bumps the
 * marker generation counter (0x4d92e4) and sets the flag; the tail asserts
 * cluster_marker_initialized (line 0x130) and clears it.
 *
 * Confirmed from decompile:
 *   - work_stack[MAXIMUM_CLUSTERS_PER_STRUCTURE=512] on the frame, seeded with
 *     starting_cluster; push bounded by the 0xf5 assert (stack_depth < 512).
 *   - clusters tag block at scenario+0x134 (stride 0x68); each cluster's
 *     portal-index block header at cluster+0x5c (count = *(int*)); index
 *     elements int16 (stride 2).
 *   - cluster_portals tag block at scenario+0x154 (stride 0x40); portal[0]/
 *     portal[1] = front/back cluster (int16); if front == current, take back.
 *   - FUN_00110210 float arg #2 = *(float*)(portal+0x14) (radius), a float,
 *     NOT a pointer.
 * Inferred: point/direction/length param semantics from FUN_00110210's
 *   signature (only forwarded here); return = count of clusters written.
 */
int16_t structure_clusters_in_cone(int16_t starting_cluster, float *point,
                                   float *direction, float length, float sine,
                                   float cosine, int16_t max_count,
                                   int16_t *out_indices)
{
  int16_t work_stack[512];
  void *scenario;
  int stack_depth;
  int output_count;
  int work_depth;
  int next_output;

  if (*(uint8_t *)0x4d92e1 != 0) {
    display_assert("!structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x103, true);
    system_exit(-1);
  }
  *(uint32_t *)0x4d92e4 += 1;
  *(uint8_t *)0x4d92e1 = 1;

  scenario = scenario_get();
  structure_cluster_mark(starting_cluster);
  work_stack[0] = starting_cluster;
  stack_depth = 1;
  output_count = 0;

  do {
    int16_t current_cluster;
    char *cluster;
    int *portal_count;

    if (max_count <= (int16_t)output_count) {
      break;
    }

    stack_depth -= 1;
    current_cluster = work_stack[(int16_t)stack_depth];
    work_depth = stack_depth;
    cluster = tag_block_get_element((char *)scenario + 0x134,
                                    (int)current_cluster, 0x68);
    next_output = output_count + 1;
    portal_count = (int *)(cluster + 0x5c);
    out_indices[(int16_t)output_count] = current_cluster;

    if (*portal_count > 0) {
      int16_t portal_iter = 0;

      do {
        int16_t *portal_index_ptr =
          tag_block_get_element(portal_count, (int)portal_iter, 2);
        int16_t *portal = tag_block_get_element((char *)scenario + 0x154,
                                                (int)*portal_index_ptr, 0x40);
        int16_t adjacent_cluster = portal[0];

        if (adjacent_cluster == current_cluster) {
          adjacent_cluster = portal[1];
        }

        if (structure_cluster_unmarked(adjacent_cluster) &&
            FUN_00110210((float *)(portal + 4), *(float *)(portal + 10), point,
                         direction, length, sine, cosine)) {
          structure_cluster_mark(adjacent_cluster);
          if ((int16_t)work_depth > 0x1ff) {
            display_assert("stack_depth<MAXIMUM_CLUSTERS_PER_STRUCTURE",
                           "c:\\halo\\SOURCE\\structures\\structures.c", 0xf5,
                           true);
            system_exit(-1);
          }
          work_stack[(int16_t)work_depth] = adjacent_cluster;
          work_depth += 1;
        }

        portal_iter += 1;
        stack_depth = work_depth;
      } while ((int)portal_iter < *portal_count);
    }

    output_count = next_output;
  } while (0 < (int16_t)stack_depth);

  if (*(uint8_t *)0x4d92e1 == 0) {
    display_assert("structure_globals.cluster_marker_initialized",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x130, true);
    system_exit(-1);
  }
  *(uint8_t *)0x4d92e1 = 0;

  return (int16_t)output_count;
}

/* structure_test_vector (0x198cb0)
 *
 * Traces a vector (point + direction) through the BSP, iteratively firing a
 * collision raycast (FUN_0014df70, flags 0x21) from the current working point
 * and, on each hit, asking structure_render_surface_from_point_and_leaf whether
 * the hit surface's material matches.  A matching surface whose collection
 * element (scenario+0x104, stride 0x20) is not the -1 sentinel terminates the
 * trace with success (returns 1).  If the collision result requests a continued
 * trace (result byte +0x4c bit0), the working point is nudged forward along the
 * direction by the tiny constant at 0x29ca28 (2^-12) and the loop repeats;
 * otherwise the trace ends with 0.
 *
 * Confirmed from disassembly at 0x198cb0:
 *   - Collision result is an 80-byte (0x50) struct written by FUN_0014df70 via
 *     LEA [EBP-0x54]; read fields: +0x0c leaf/cluster index (dword),
 * +0x18..0x20 hit point (vec3), +0x48 material ref (masked & 0x7fffffff), +0x4c
 * status byte (bit0 = continue-trace).  Sized per hazard #5 / units.c:9411
 * sibling.
 *   - Collision user-stack depth guard (global at 0x4761d8, int16) is pushed/
 *     popped each iteration with the 0xf marker on the stack array at 0x5a8c80.
 *   - FUN_00198580 called cdecl with 8 args (ADD ESP,0x20), char return in AL:
 *     (out_point, leaf, material&0x7fffffff, p4..p8).  p4
 * (out_collection_index) is the only out-pointer the caller itself dereferences
 * (validity check).
 *   - Assert condition strings (single-letter param names): "p", "v",
 *     "material_index", "surface_index", "s", "t" at lines 0x188-0x18d.
 */
char structure_test_vector(float *point, float *direction, float *out_point,
                           int16_t *out_collection_index,
                           int16_t *out_material_index,
                           int32_t *out_surface_index, float *out_u,
                           float *out_v)
{
  char result;
  char terminate;
  int16_t depth;
  void *scenario;
  int16_t *plane;
  char collision_result[80];

  result = 0;
  if (point == NULL) {
    display_assert("p", "c:\\halo\\SOURCE\\structures\\structures.c", 0x188,
                   true);
    system_exit(-1);
  }
  if (direction == NULL) {
    display_assert("v", "c:\\halo\\SOURCE\\structures\\structures.c", 0x189,
                   true);
    system_exit(-1);
  }
  if (out_material_index == NULL) {
    display_assert("material_index",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x18a, true);
    system_exit(-1);
  }
  if (out_surface_index == NULL) {
    display_assert("surface_index",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x18b, true);
    system_exit(-1);
  }
  if (out_u == NULL) {
    display_assert("s", "c:\\halo\\SOURCE\\structures\\structures.c", 0x18c,
                   true);
    system_exit(-1);
  }
  if (out_v == NULL) {
    display_assert("t", "c:\\halo\\SOURCE\\structures\\structures.c", 0x18d,
                   true);
    system_exit(-1);
  }

  out_point[0] = point[0];
  out_point[1] = point[1];
  out_point[2] = point[2];

  do {
    terminate = 1;
    if (*(int16_t *)0x4761d8 >= 32) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\structures\\structures.c", 0x196, true);
      system_exit(-1);
    }
    depth = *(int16_t *)0x4761d8;
    *(int16_t *)0x4761d8 = depth + 1;
    *(int16_t *)(0x5a8c80 + depth * 2) = 0xf;

    if (FUN_0014df70(0x21, out_point, direction, -1,
                     (int16_t *)collision_result) != 0) {
      scenario = scenario_get();
      out_point[0] = *(float *)(collision_result + 0x18);
      out_point[1] = *(float *)(collision_result + 0x1c);
      out_point[2] = *(float *)(collision_result + 0x20);
      if (structure_render_surface_from_point_and_leaf(
            out_point, *(uint32_t *)(collision_result + 0xc),
            *(uint32_t *)(collision_result + 0x48) & 0x7fffffff,
            out_collection_index, out_material_index, out_surface_index, out_u,
            out_v) != 0) {
        plane = (int16_t *)tag_block_get_element(
          (char *)scenario + 0x104, (int)*out_collection_index, 0x20);
        if (*plane != -1) {
          result = 1;
          goto done;
        }
      }
      if ((collision_result[0x4c] & 1) != 0) {
        terminate = 0;
        out_point[0] = direction[0] * *(float *)0x29ca28 + out_point[0];
        out_point[1] = direction[1] * *(float *)0x29ca28 + out_point[1];
        out_point[2] = direction[2] * *(float *)0x29ca28 + out_point[2];
      }
    }

  done:
    if (*(int16_t *)0x4761d8 < 2) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\structures\\structures.c", 0x1aa, true);
      system_exit(-1);
    }
    *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 - 1;
  } while (terminate == 0);

  return result;
}

/*
 * render_debug_fog_planes (0x1990d0) -- structures.obj
 *
 * When fog-plane debug is enabled (byte 0x505700 set, mode word 0x50674c == 1,
 * and structure-index dword 0x506784 valid), fetch the selected fog plane from
 * the scenario structure-BSP tag and draw each of its edges. For every edge the
 * two endpoint vertices are offset inward along the plane normal by the fog
 * distance (float 0x506770, negated) to build a parallel copy; both the
 * original edge and the offset edge are emitted through the debug line writer
 * (0x17eb10) and the two connecting sides through 0x17e5b0, using the pair of
 * view transforms cached at 0x2ee6c4 / 0x2ee6cc.
 *
 * The two 3-float vertex triples live in contiguous stack slots in the original
 * (EBP-0x14 and EBP-0x20) because their base addresses are passed to callees
 * that read 3 floats; they are declared as arrays here to guarantee that
 * contiguity. The (int)(short) narrowing on the edge index / element count is
 * intentional (original reloads them via MOVSX word) and match-sensitive.
 */
void render_debug_fog_planes(void)
{
  float fVar1;
  int iVar2;
  int iVar3;
  float *pfVar4;
  float *pfVar5;
  float vert_b[3]; /* EBP-0x20 offset triple for pfVar5 */
  float vert_a[3]; /* EBP-0x14 offset triple for pfVar4 */
  int local_c;
  int local_8;

  if ((*(char *)0x505700 != 0) && (*(short *)0x50674c == 1) &&
      (*(int *)0x506784 != -1)) {
    iVar2 = (int)scenario_get();
    iVar3 = (int)tag_block_get_element((void *)(iVar2 + 0x134),
                                       *(int *)0x506784, 0x68);
    iVar2 = (int)tag_block_get_element(
      (void *)(iVar2 + 0x178), *(unsigned short *)(iVar3 + 2) & 0x7fff, 0x20);
    local_c = *(int *)(iVar2 + 0x14);
    local_8 = 0;
    if (0 < local_c) {
      iVar3 = 0;
      do {
        local_c = (iVar3 + 1) % local_c;
        pfVar4 =
          (float *)tag_block_get_element((void *)(iVar2 + 0x14), iVar3, 0xc);
        pfVar5 = (float *)tag_block_get_element((void *)(iVar2 + 0x14),
                                                (int)(short)local_c, 0xc);
        fVar1 = -*(float *)0x506770;
        /* Interleaved by component: the original reuses each
         * (fVar1 * normal_component) product for both endpoints, so the
         * six independent stores are grouped in component order. */
        vert_a[0] = fVar1 * *(float *)(iVar2 + 4) + pfVar4[0];
        vert_b[0] = fVar1 * *(float *)(iVar2 + 4) + pfVar5[0];
        vert_a[1] = fVar1 * *(float *)(iVar2 + 8) + pfVar4[1];
        vert_b[1] = fVar1 * *(float *)(iVar2 + 8) + pfVar5[1];
        vert_a[2] = fVar1 * *(float *)(iVar2 + 0xc) + pfVar4[2];
        vert_b[2] = fVar1 * *(float *)(iVar2 + 0xc) + pfVar5[2];
        FUN_0017eb10(pfVar4, pfVar5, *(int *)0x2ee6c4);
        FUN_0017eb10(vert_a, vert_b, *(int *)0x2ee6cc);
        FUN_0017e5b0(pfVar4, vert_a, *(int *)0x2ee6c4, *(int *)0x2ee6cc);
        FUN_0017e5b0(pfVar5, vert_b, *(int *)0x2ee6c4, *(int *)0x2ee6cc);
        local_c = *(int *)(iVar2 + 0x14);
        local_8 = local_8 + 1;
        iVar3 = (int)(short)local_8;
      } while (iVar3 < local_c);
    }
  }
}

int16_t structure_find_in_cluster(uint16_t cluster_count, float *position,
                                  float radius, int max_count,
                                  int16_t *intersected_indices)
{
  if (position == NULL) {
    display_assert("position", "c:\\halo\\SOURCE\\structures\\structures.c",
                   0x86, true);
    system_exit(-1);
  }

  if (radius < 0.f) {
    display_assert("radius>=0.f", "c:\\halo\\SOURCE\\structures\\structures.c",
                   0x87, true);
    system_exit(-1);
  }

  if ((int16_t)max_count <= 0) {
    display_assert("maximum_count>0",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x88, true);
    system_exit(-1);
  }

  if (intersected_indices == NULL) {
    display_assert("intersected_indices",
                   "c:\\halo\\SOURCE\\structures\\structures.c", 0x89, true);
    system_exit(-1);
  }

  if ((int16_t)cluster_count != -1) {
    if (radius > 0.f) {
      int16_t cluster_count_out;

      structures_cluster_marker_begin();
      cluster_count_out = FUN_001989b0(cluster_count, position, radius,
                                       max_count, intersected_indices);
      structures_cluster_marker_end();
      return cluster_count_out;
    }

    if ((int16_t)max_count > 0) {
      *intersected_indices = (int16_t)cluster_count;
      return 1;
    }
  }

  return 0;
}

/*
 * file_location_volume_names (0x505500):
 * char[NUMBER_OF_FILE_REFERENCE_LOCATIONS] [MAXIMUM_FILENAME_LENGTH+1]
 * volume/device-name table, one 256-byte row per file-reference location,
 * indexed by location * 0x100.
 */
#define file_location_volume_names ((char *)0x505500)

/*
 * set_file_location_volume_name (0x199360) - record the volume/device name for
 * a file-reference location.
 *
 * The original asserts require: location is in (0, NUMBER_OF_FILE_REFERENCE_
 * LOCATIONS) i.e. exactly 1; the target row is currently empty; and volume_name
 * fits in MAXIMUM_FILENAME_LENGTH (255) characters. Copies at most 0xff bytes
 * with csstrncpy and explicitly null-terminates byte 255 of the row.
 */
void set_file_location_volume_name(int16_t location, const char *volume_name)
{
  if (location < 1 || location > 1) {
    display_assert("location>0 && location<NUMBER_OF_FILE_REFERENCE_LOCATIONS",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x4b, true);
    system_exit(-1);
  }
  if (csstrlen(file_location_volume_names + location * 0x100) != 0) {
    display_assert("strlen(file_location_volume_names[location])==0",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x4c, true);
    system_exit(-1);
  }
  if ((unsigned int)csstrlen(volume_name) > 0xff) {
    display_assert("strlen(volume_name)<=MAXIMUM_FILENAME_LENGTH",
                   "c:\\halo\\SOURCE\\tag_files\\files.c", 0x4d, true);
    system_exit(-1);
  }
  csstrncpy(file_location_volume_names + location * 0x100, volume_name, 0xff);
  file_location_volume_names[location * 0x100 + 0xff] = '\0';
}

/* structure_detail_objects_initialize_for_new_map @ 0x193bb0
 * Per-map reset of the detail_object_global_runtime_data block. Asserts the
 * block was allocated (sibling structure_detail_objects_initialize stores the
 * base at 0x4d8ea0), then zeroes the whole 0xa430-byte block and clears the
 * byte at base+0x520e. display_assert lineno 0x6d (109), halt=1; on failure
 * the original tail-calls system_exit(-1) (the assert hard-exit).
 * Global 0x4d8ea0 re-read (not cached) to match the original. */
/* noinline: original build had this in a separate TU (structure_detail_objects.c). */
__declspec(noinline) void structure_detail_objects_initialize_for_new_map(void)
{
  if (*(int *)0x4d8ea0 == 0) {
    display_assert("detail_object_global_runtime_data",
                   "c:\\halo\\SOURCE\\structures\\structure_detail_objects.c",
                   0x6d, 1);
    system_exit(-1);
  }
  csmemset(*(void **)0x4d8ea0, 0, 0xa430);
  *(uint8_t *)(*(int *)0x4d8ea0 + 0x520e) = 0;
}
