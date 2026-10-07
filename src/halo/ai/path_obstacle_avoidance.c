/* path_obstacle_avoidance.c — AI path obstacle-avoidance helpers.
 *
 * Corresponds to path_obstacle_avoidance.obj.
 *
 * Recovered by lifting from cachebeta.xbe (v01.10.12.2276).
 */
#include "../../common.h"

/*
 * FUN_00060c40 -- valid_real_point2d: returns true when both components of a
 * real_point2d (x, y) are finite (neither NaN nor +/-Inf).
 *
 * A float is non-finite iff its IEEE-754 exponent field is all ones, i.e.
 * (bits & 0x7f800000) == 0x7f800000. The original materializes the boolean
 * in full EAX (MOV EAX,1 / XOR EAX,EAX); Ghidra collapsed the two returns to
 * void because callers discarded the result. Same 0x7f800000 mask idiom as
 * valid_real_rgb_color.
 *
 * ABI: cdecl, one stack pointer arg (real_point2d*), pure integer leaf.
 *
 * Shape (delinked 00060c40.obj): each component is copied into a float local
 * first, then bit-tested through the local — VC71 spills the local into the
 * dead param home slot ([EBP+8], MOV [EBP+8],ECX / MOV [EBP+8],EAX), keeping
 * the frame at zero locals. Testing point[N]'s bits directly loses those
 * stores (59.5%); the local recovers them. The tests are spelled as a nested
 * valid-chain (`!= mask` guarding inward, shared `return 0` tail) so both
 * branches compile to JE into the trailing XOR EAX block — goto/early-return
 * spellings made VC71 flip the second branch (85.7%). 100.0% VC71.
 */
int valid_real_point2d(float *point)
{
  float v;

  v = point[0];
  if ((*(uint32_t *)&v & 0x7f800000) != 0x7f800000) {
    v = point[1];
    if ((*(uint32_t *)&v & 0x7f800000) != 0x7f800000)
      return 1;
  }
  return 0;
}

/*
 * 0x00060c80 -- path_add_step (name: CEA PDB line containment).
 *
 * ABI (disassembly): path arrives in EDI (read before any write, never
 * pushed); six cdecl stack args at [EBP+8..0x1c]; plain RET. Returns the new
 * step index in AX (MOV AX,BX), or -1 (OR EAX,-1) when the step list is full
 * or when a matching step is rejected.
 *
 * Step records (0x28 bytes at path+0x30, see FUN_000600f0): +0x00..+0x08
 * point (x,y,z), +0x0c..+0x10 2D direction normalized in place, +0x14 length
 * returned by normalize2d, +0x18 int16, +0x1a byte, +0x1c int16[2] links
 * indexed by the byte arg, +0x20 float, +0x24 int16 step index (followed as
 * the chain link). Path fields touched: +0x10/+0x14 floats, +0x1c int16,
 * +0x20 int16, +0x24 float, +0x2c int16 step_count.
 *
 * The chain-walk bounds check is inlined (same assert text and line 0x28 as
 * FUN_000600f0); the three other step lookups call FUN_000600f0.
 * FUN_00060910 takes path@<eax> and the new step index @<bx>; the assert on
 * its result passes a NULL reason (PUSH 0) at line 0x1a4.
 */
int16_t path_add_step(void *path, float *point, float z, int16_t param_3,
                      unsigned char param_4, float param_5,
                      int16_t link_step_index)
{
  char *base;
  char *step;
  float *new_step;
  float *a;
  float *b;
  float dx;
  float dy;
  int16_t step_index;
  int16_t new_index;
  int16_t *link;
  char flag;

  base = (char *)path;
  if (*(int16_t *)(base + 0x2c) >= 0x80)
    return -1;
  dx = *(float *)(base + 0x10) - point[0];
  flag = 0;
  dy = *(float *)(base + 0x14) - point[1];
  step_index = link_step_index;
  while (step_index != -1) {
    if (step_index < 0 || step_index >= *(int16_t *)(base + 0x2c) ||
        *(int16_t *)(base + 0x2c) > 0x80) {
      display_assert("step_index>=0 && step_index<path->step_count && "
                     "path->step_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                     "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x28,
                     1);
      system_exit(-1);
    }
    step = base + 0x30 + step_index * 0x28;
    if (*(int16_t *)(step + 0x18) != param_3) {
      if (param_3 == *(int16_t *)(base + 0x1c) &&
          *(int16_t *)(base + 0x1c) != -1) {
        flag = 1;
        if (((int16_t *)(step + 0x1c))[param_4 == 0] != -1) {
          a = (float *)FUN_000600f0(path,
                                    ((int16_t *)(step + 0x1c))[param_4 == 0]);
          b = (float *)FUN_000600f0(path, link_step_index);
          if (dy * a[4] + dx * a[3] > REAL_ZERO_POOL &&
              (b[4] * a[3] - a[4] * b[3]) * (dy * a[3] - dx * a[4]) <
                REAL_ZERO_POOL)
            return -1;
        }
        link = &((int16_t *)(step + 0x1c))[param_4];
        if (*link == link_step_index || *link == -1)
          *link = *(int16_t *)(base + 0x2c);
      }
      break;
    }
    if (*(unsigned char *)(step + 0x1a) != param_4)
      return -1;
    step_index = *(int16_t *)(step + 0x24);
  }

  new_index = *(int16_t *)(base + 0x2c);
  *(int16_t *)(base + 0x2c) = new_index + 1;
  new_step = (float *)FUN_000600f0(path, new_index);
  new_step[0] = point[0];
  new_step[1] = point[1];
  new_step[2] = z;
  new_step[3] = dx;
  new_step[4] = dy;
  new_step[5] = normalize2d(new_step + 3);
  new_step[8] = new_step[5] + param_5;
  *(int16_t *)((char *)new_step + 0x18) = param_3;
  *((unsigned char *)new_step + 0x1a) = param_4;
  *(int16_t *)((char *)new_step + 0x24) = link_step_index;
  csmemset(new_step + 7, -1, 4);
  if (flag && new_step[5] < *(float *)(base + 0x24)) {
    *(float *)(base + 0x24) = new_step[5];
    *(int16_t *)(base + 0x20) = new_index;
  }
  if (!FUN_00060910(path, new_index)) {
    display_assert(0, "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x1a4,
                   1);
    system_exit(-1);
  }
  return new_index;
}

/*
 * 0x00060ea0 -- initializes an avoidance record and adds its start step.
 *
 * ABI (disassembly): avoidance_record in ECX (copied to EDI), end_point in
 * EAX (copied to ESI); nine cdecl stack args; plain RET.
 *
 * param_7 is forwarded as a raw dword (MOV EDX,[EBP+0x1c] / PUSH EDX) into
 * path_add_step's float z slot, so it is reinterpreted, not converted.
 * +0x24 receives the bit pattern 0x7f7fffff.
 */
void FUN_00060ea0(void *avoidance_record, float *end_point, void *param_2,
                  void *scenario, unsigned char param_4, float radius,
                  float *start_point, int param_7, float param_8,
                  unsigned char param_9, unsigned char param_10)
{
  float v;
  char *record;
  int16_t disc_index;
  int16_t disc_value;

  record = (char *)avoidance_record;
  v = radius;
  if ((*(uint32_t *)&v & 0x7f800000) == 0x7f800000) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real(0x%08X %f)", "radius",
                            *(uint32_t *)&v, (double)radius),
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x1b8, 1);
    system_exit(-1);
  }
  v = start_point[0];
  if ((*(uint32_t *)&v & 0x7f800000) == 0x7f800000 ||
      (v = start_point[1], (*(uint32_t *)&v & 0x7f800000) == 0x7f800000)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_point2d(%f, %f)", "start",
                            (double)start_point[0], (double)start_point[1]),
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x1b9, 1);
    system_exit(-1);
  }
  v = end_point[0];
  if ((*(uint32_t *)&v & 0x7f800000) == 0x7f800000 ||
      (v = end_point[1], (*(uint32_t *)&v & 0x7f800000) == 0x7f800000)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_point2d(%f, %f)", "goal",
                            (double)end_point[0], (double)end_point[1]),
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x1ba, 1);
    system_exit(-1);
  }
  *(float *)(record + 0x00) = radius;
  *(void **)(record + 0x0c) = scenario;
  *(unsigned char *)(record + 0x04) = param_4;
  *(void **)(record + 0x08) = param_2;
  *(unsigned char *)(record + 0x28) = 0;
  *(float *)(record + 0x10) = end_point[0];
  *(float *)(record + 0x14) = end_point[1];
  *(float *)(record + 0x18) = param_8;
  disc_index = FUN_00062410(param_2, -1, end_point, radius);
  if (disc_index == -1) {
    disc_value = -1;
  } else {
    disc_value = *(int16_t *)((char *)FUN_00060070(param_2, disc_index) + 2);
  }
  *(int16_t *)(record + 0x1c) = disc_value;
  *(unsigned char *)(record + 0x29) = param_9;
  *(int16_t *)(record + 0x1e) = -1;
  *(uint32_t *)(record + 0x24) = 0x7f7fffff;
  *(int16_t *)(record + 0x20) = -1;
  *(unsigned char *)(record + 0x2a) = param_10;
  *(int16_t *)(record + 0x2c) = 0;
  *(int16_t *)(record + 0x1430) = 0;
  path_add_step(avoidance_record, start_point, *(float *)&param_7, -1, 0, 0.0f,
                -1);
}

/*
 * 0x00061280 -- path_add_steps (name: CEA PDB line containment).
 *
 * ABI (disassembly): seed_disc_index arrives in AX (MOV BX,AX before any
 * write to EAX), path in ESI (read, never saved or written); one cdecl stack
 * arg step_index at [EBP+8]; plain RET, no return value.
 *
 * Flood-fills the obstacle discs reachable from the seed disc using an
 * explicit int16 stack (MAXIMUM_DISC_COUNT=0x80 entries at [EBP-0x160]) and a
 * visited bit vector (4 dwords at [EBP-0x54]). For each popped disc,
 * FUN_000625a0 produces two 2D directions ([EBP-0x44], [EBP-0x3c]) and a
 * distance clamped to at least path+0x00. Each direction is swept with
 * path_test_pill2d (step point in EDI, direction in EBX); a newly hit disc is
 * pushed, and when the hit lies beyond the distance and its +0x0e int16
 * differs from the popped disc's +0x0a int16, a step is added at the
 * midpoint scale (*0x253398) with z from structure_test_ray2d's result +4.
 *
 * Path fields: +0x00 float, +0x04 byte, +0x08 obstacles (disc_count int16
 * at +0x02, 0x18-byte discs from +0x00 with int16 at +0x0a), +0x0c structure
 * bsp, +0x2a byte, +0x2c step_count, steps at +0x30 (0x28 bytes). Step +0x08
 * is forwarded as a raw dword (callee slot named surface_index).
 */
void path_add_steps(int16_t seed_disc_index, void *path, int16_t step_index)
{
  struct {
    float t;
    int32_t field_04;
    int32_t field_08;
    int16_t disc_index;
    int16_t field_0e;
  } result;
  char *base;
  char *step;
  char *obstacles;
  int16_t disc_stack[0x80];
  float ray_result[3];
  uint32_t visited[4];
  float directions[2][2];
  float point[2];
  float distance;
  float t;
  int16_t stack_top;
  int16_t disc_index;
  int16_t disc_word;
  int16_t j;
  uint32_t bit;

  base = (char *)path;
  if (step_index < 0 || step_index >= *(int16_t *)(base + 0x2c) ||
      *(int16_t *)(base + 0x2c) > 0x80) {
    display_assert("step_index>=0 && step_index<path->step_count && "
                   "path->step_count<=MAXIMUM_OBSTACLE_AVOIDANCE_STEPS",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x28, 1);
    system_exit(-1);
  }
  step = base + 0x30 + step_index * 0x28;
  obstacles = *(char **)(base + 8);
  if (*(int16_t *)(obstacles + 2) < 0 || *(int16_t *)(obstacles + 2) > 0x80) {
    display_assert("path->obstacles->disc_count>=0 && "
                   "path->obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x252, 1);
    system_exit(-1);
  }
  csmemset(visited, 0,
           ((*(int16_t *)(*(char **)(base + 8) + 2) + 0x1f) >> 5) << 2);
  if (seed_disc_index < 0 ||
      seed_disc_index >= *(int16_t *)(*(char **)(base + 8) + 2)) {
    display_assert("seed_disc_index>=0 && "
                   "seed_disc_index<path->obstacles->disc_count",
                   "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c", 0x255, 1);
    system_exit(-1);
  }
  visited[seed_disc_index >> 5] |= 1u << (seed_disc_index & 0x1f);
  disc_stack[0] = seed_disc_index;
  stack_top = 1;
  do {
    obstacles = *(char **)(base + 8);
    stack_top--;
    disc_index = disc_stack[stack_top];
    if (disc_index == -1) {
      disc_word = -1;
    } else {
      if (disc_index < 0 || disc_index >= *(int16_t *)(obstacles + 2) ||
          *(int16_t *)(obstacles + 2) > 0x80) {
        display_assert("disc_index>=0 && disc_index<obstacles->disc_count && "
                       "obstacles->disc_count<=MAXIMUM_DISC_COUNT",
                       "c:\\halo\\source\\ai\\path.h", 0x18c, 1);
        system_exit(-1);
      }
      disc_word = *(int16_t *)(obstacles + disc_index * 0x18 + 0xa);
    }
    FUN_000625a0(*(void **)(base + 8), disc_index, (float *)step,
                 *(float *)base, directions[0], directions[1], &distance);
    if (distance < *(float *)base)
      distance = *(float *)base;
    for (j = 0; j < 2; j++) {
      path_test_pill2d((float *)step, directions[j], *(void **)(base + 0xc),
                       *(unsigned char *)(base + 4), *(void **)(base + 8),
                       disc_index, *(int32_t *)(step + 8), *(float *)base,
                       *(float *)base + *(float *)base + distance, 0, 0,
                       *(unsigned char *)(base + 0x2a), &result);
      if (result.disc_index != -1) {
        if (result.disc_index < 0 ||
            result.disc_index >= *(int16_t *)(*(char **)(base + 8) + 2)) {
          display_assert("result.disc_index>=0 && "
                         "result.disc_index<path->obstacles->disc_count",
                         "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c",
                         0x271, 1);
          system_exit(-1);
        }
        bit = 1u << (result.disc_index & 0x1f);
        if ((visited[result.disc_index >> 5] & bit) == 0) {
          visited[result.disc_index >> 5] |= bit;
          if (stack_top >= 0x80) {
            display_assert("stack_top<MAXIMUM_DISC_COUNT",
                           "c:\\halo\\SOURCE\\ai\\path_obstacle_avoidance.c",
                           0x277, 1);
            system_exit(-1);
          }
          disc_stack[stack_top] = result.disc_index;
          stack_top++;
        }
      }
      if (result.t > distance && result.field_0e != disc_word) {
        t = (result.t + distance) * *(const float *)0x253398;
        structure_test_ray2d(
          *(void **)(base + 0xc), *(unsigned char *)(base + 4), (float *)step,
          *(int32_t *)(step + 8), directions[j], t, ray_result);
        point[0] = t * directions[j][0] + ((float *)step)[0];
        point[1] = t * directions[j][1] + ((float *)step)[1];
        path_add_step(path, point, ray_result[1], disc_word, (unsigned char)j,
                      ((float *)step)[8] - ((float *)step)[5] + t, step_index);
      }
    }
  } while (stack_top > 0);
}

/*
 * FUN_000616e0 -- path_find (name: 2276 symbol dump, T1).
 *
 * ABI (binary evidence, caller FUN_00061750 @ 0x619d3..0x619f1 and this
 * function's prologue): avoidance_record arrives in ESI; three more values
 * arrive in EDX/ECX/EAX and are pushed through unchanged as FUN_00060ea0's
 * last three stack args (PUSH EAX / PUSH ECX / PUSH EDX before any other
 * push). Six cdecl stack params (caller ADD ESP,0x18). The caller loads EDX
 * from a dword local, ECX from a byte (XOR ECX,ECX / MOV CL), and AL=1, and
 * tests only AL of the result. Register params are declared first
 * (EAX, ECX, EDX, then ESI) so the reverse thunk stages them in scratch
 * slots; the C parameter order is not source-order evidence. Return is
 * materialized in full EAX (XOR EAX,EAX / SETNZ AL).
 *
 * FUN_00060ea0 argument map (ADD ESP,0x24 = 9 stack args):
 *   ECX=avoidance_record, EAX=[EBP+0x1c], then [EBP+0xc], scenario_get(),
 *   [EBP+0x8], [EBP+0x10], [EBP+0x14], [EBP+0x18], EDX, ECX, EAX.
 * FUN_000615b0 is repeated while it returns nonzero in AL.
 * +0x1e/+0x20 are int16 fields (compared against -1); +0x28 is a byte flag.
 */
int path_find(unsigned char param_10, unsigned char param_9, float param_8,
              void *avoidance_record, unsigned char param_4, void *param_2,
              float radius, float *start_point, int param_7, float *end_point)
{
  char *record;

  record = (char *)avoidance_record;
  FUN_00060ea0(avoidance_record, end_point, param_2, scenario_get(), param_4,
               radius, start_point, param_7, param_8, param_9, param_10);
  do {
  } while ((char)FUN_000615b0(avoidance_record) != 0);
  if (*(int16_t *)(record + 0x1e) != -1) {
    *(unsigned char *)(record + 0x28) = 1;
  } else if (*(int16_t *)(record + 0x20) != -1) {
    *(int16_t *)(record + 0x1e) = *(int16_t *)(record + 0x20);
  }
  return *(int16_t *)(record + 0x1e) != -1;
}
