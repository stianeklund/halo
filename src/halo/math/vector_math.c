#include "x87_math.h"

/* 0x12090 — action_alert: raise alert/engage flags on an actor.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] (Ghidra: in_stack_00000004).
 *   The kb decl was `(void)`; the disassembly reads [EBP+0x8] and pushes it.
 * Confirmed: PUSH EAX([EBP+8]) / PUSH ECX(*0x6325a4) / CALL 0x119320
 *   -> datum_get(actors_data, actor_handle); result kept in ESI.
 * Confirmed: PUSH EDX([ESI+0x58]) / PUSH 0x61637472 ('actr') / CALL 0x1ba140
 *   -> tag_get(group_tag='actr', tag_index=*(int *)(actor + 0x58)).
 *   ADD ESP,0x10 cleans both cdecl calls (2 + 2 dword args).
 * Confirmed: MOV ECX,1 / MOV word ptr [ESI+0x3fc],CX — 16-bit store of 1.
 * Confirmed: MOV DL,byte ptr [EAX] / TEST DL,0x40 — tests bit 6 of byte 0 of
 *   the 'actr' tag definition; when set, MOV byte [ESI+0x426],CL and
 *   MOV byte [ESI+0x427],CL — two 8-bit stores of 1. */
void FUN_00012090(int actor_handle)
{
  int actor;
  unsigned char *definition;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  definition = (unsigned char *)tag_get(0x61637472, *(int *)(actor + 0x58));
  *(unsigned short *)(actor + 0x3fc) = 1;
  if ((definition[0] & 0x40) != 0) {
    *(unsigned char *)(actor + 0x426) = 1;
    *(unsigned char *)(actor + 0x427) = 1;
  }
}

/* 0x120e0 — action_alert: clear alert state on actor.
 * Sets actor->state_data1 (0xa2) and state_data2 (0xa4) to 0xffff. */
void FUN_000120e0(int actor_handle)
{
  int actor;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(unsigned short *)(actor + 0xa2) = 0xffff;
  *(unsigned short *)(actor + 0xa4) = 0xffff;
}

/* 0x12110 — action_alert: clear another alert/avoid state.
 * Sets actor->field_d0 (short) to 0xffff and field_f4 (int) to -1. */
void FUN_00012110(int actor_handle)
{
  int actor;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  *(unsigned short *)(actor + 0xd0) = 0xffff;
  *(unsigned int *)(actor + 0xf4) = 0xffffffff;
}


/* FUN_00012140 (0x12140) — Subtract two 3D vectors: result = b - a.
 * Confirmed: cdecl, 3 pointer args. Pure FPU leaf.
 * Confirmed: arg1=a [EBP+0x8], arg2=b [EBP+0xc], arg3=result [EBP+0x10].
 * Confirmed: FLD [ECX] / FSUB [EDX] / FSTP [EAX] where ECX=b, EDX=a,
 * EAX=result. */
void FUN_00012140(float *a, float *b, float *result)
{
  result[0] = b[0] - a[0];
  result[1] = b[1] - a[1];
  result[2] = b[2] - a[2];
}

/* 0x12170 — FUN_00012170: squared magnitude of a 3D vector.
 *
 * Computes vector[0]^2 + vector[1]^2 + vector[2]^2 and returns it.
 *
 * Confirmed: loads three floats from [arg0+0], [arg0+4], and [arg0+8].
 * Confirmed: x87 stack sequence squares each component and accumulates with
 *   FADDP; no globals or calls.
 * Confirmed: returns the accumulated sum in ST0.
 */
float FUN_00012170(float *vector)
{
  return vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2];
}

/* 0x121a0 — distance_squared3d: squared distance between two 3D points.
 *
 * Computes (b[0]-a[0])^2 + (b[1]-a[1])^2 + (b[2]-a[2])^2 and returns it.
 *
 * Confirmed: loads from [arg1+0/4/8] and subtracts [arg0+0/4/8].
 * Confirmed: x87 sequence squares each component delta and sums with FADDP.
 * Confirmed: returns in ST0; no globals or calls.
 */
float distance_squared3d(const float *a, const float *b)
{
  float dx = b[0] - a[0];
  float dy = b[1] - a[1];
  float dz = b[2] - a[2];

  return dx * dx + dy * dy + dz * dz;
}

float FUN_000121e0(float min, float max)
{
  int *seed = get_global_random_seed_address();
  return random_real_range(seed, min, max);
}

/* action_avoid_setup (0x128c0)
 * Initialize action avoid state: asserts non-null state pointer, zeroes 4
 * bytes, returns true.
 *
 * Confirmed: cdecl, two stack args. MOV ESI,[EBP+0xc] at 0x128c4 is the
 *   asserted/zeroed pointer, so state_data is the SECOND arg; [EBP+0x8] is
 *   never read by this function (unknown type; named actor_handle after the
 *   identical action_fight_setup twin FUN_00014620 / 0x14620).
 * Confirmed: TEST ESI,ESI / JNZ 0x128e8 at 0x128c7 — assert on NULL only.
 * Confirmed assert args (pushed last-to-first): PUSH 1 (halt), PUSH 0x1e
 *   (line 30), PUSH 0x25339c ("c:\halo\SOURCE\ai\action_avoid.c"),
 *   PUSH 0x25334c ("state_data") / CALL display_assert; then PUSH -1 /
 *   CALL system_exit (noreturn — no ADD ESP on that path).
 * Confirmed: PUSH 4 / PUSH 0 / PUSH ESI / CALL csmemset / ADD ESP,0xc.
 * Confirmed: MOV AL,0x1 at 0x128f5 — byte return, always true. */
char action_avoid_setup(int actor_handle, void *state_data)
{
  if (state_data == NULL) {
    display_assert("state_data", "c:\\halo\\SOURCE\\ai\\action_avoid.c", 0x1e,
                   1);
    system_exit(-1);
  }
  csmemset(state_data, 0, 4);
  return 1;
}

/* action_avoid_perform (0x12920)
 * Run one avoid-action tick: assert the actor is not a swarm actor, and when
 * its timeslice byte is set, evaluate a look target (FUN_00027090) and hand
 * the result to the firing-position selector (FUN_000272d0).  Returns true
 * when actor+0x280 (short) is zero, on both paths.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] kept in EDI (0x12934); the kb
 *   decl was `(void)` but the arg is pushed to all three callees.
 * Confirmed: MOV EAX,0x14740 / CALL _chkstk at 0x12923 — 0x14740-byte frame.
 *   Slots: big_buf 0x1408c @EBP-0x14740, state_buf 0x670 @EBP-0x6b4,
 *   local_48 0x3c @EBP-0x44, local_8 @EBP-0x8, local_4 @EBP-0x4
 *   (4+4+0x3c+0x670+0x1408c == 0x14740).
 * Confirmed: PUSH EDI / PUSH EAX(=[0x6325a4]) / CALL 0x119320 -> datum_get;
 *   result kept in ESI.
 * Confirmed: MOV AL,byte ptr [ESI+0x6] / TEST AL,AL -> display_assert(
 *   "!actor->meta.swarm" @0x253380, "c:\halo\SOURCE\ai\action_avoid.c"
 *   @0x25339c, 0x37, 1) then PUSH -1 / CALL system_exit (noreturn).
 * Confirmed: MOV AL,byte ptr [ESI+0x4c] / TEST AL,AL / JZ 0x129c7 guards the
 *   body.
 * Confirmed: PUSH 0x670 / PUSH 0 / PUSH ECX(EBP-0x6b4) / CALL csmemset, then
 *   MOV word ptr [EBP-0x6b0],0x6 — a 16-bit 6 at state_buf+4.
 * Confirmed FUN_00027090 pushes (last-to-first, 0x12981..0x1299b): &local_4,
 *   big_buf, &local_8, local_48, state_buf, actor_handle.
 * Confirmed FUN_000272d0 pushes (last-to-first, 0x129aa..0x129be): local_4,
 *   big_buf, local_8, local_48, EAX (FUN_00027090 result), actor_handle.
 *   ADD ESP,0x3c at 0x129c4 cleans csmemset (0xc) + both 6-arg calls (0x18
 *   each); the FUN_000272d0 return value is discarded.
 * Confirmed: XOR EAX,EAX / CMP word ptr [ESI+0x280],AX / SETZ AL — byte
 *   return. */
bool action_avoid_perform(int actor_handle)
{
  char *actor;
  char state_buf[0x670];
  char big_buf[0x1408c];
  char local_48[0x3c];
  int local_4;
  int local_8;
  short result;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(char *)(actor + 6) != '\0') {
    display_assert("!actor->meta.swarm", "c:\\halo\\SOURCE\\ai\\action_avoid.c",
                   0x37, 1);
    system_exit(-1);
  }
  if (*(char *)(actor + 0x4c) != '\0') {
    csmemset(state_buf, 0, 0x670);
    *(short *)(state_buf + 4) = 6;
    result = FUN_00027090(actor_handle, state_buf, local_48, &local_8, big_buf,
                          &local_4);
    FUN_000272d0(actor_handle, result, local_48, local_8, (unsigned int)big_buf,
                 (char)local_4);
  }
  return *(short *)(actor + 0x280) == 0;
}

/* 0x12a80 — action_alert: decrement squad-vehicle-passenger counter
 * if actor is in state 4 (vehicle) and target's state is 3. */
void FUN_00012a80(int actor_handle)
{
  int actor;
  int other_actor;

  actor = (int)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(short *)(actor + 0xa0) == 4) {
    other_actor = (int)actor_combat_get_firing_variant_definition(actor_handle);
    if (*(short *)(other_actor + 0x156) == 3 && 0 < *(short *)(actor + 0x5fe)) {
      *(short *)(actor + 0x5fe) = *(short *)(actor + 0x5fe) + -1;
    }
  }
}

/* 0x12be0 — FUN_00012be0: bump the short counter at actor+0xaa when the actor
 * is in state 3 (same state constant guarded by FUN_00012e50) and three gate
 * bytes agree.
 *
 * Confirmed: cdecl, one stack arg at [EBP+0x8] (0x12be3 MOV EAX,[EBP+0x8]);
 *   the kb decl was `(void)` but the dword is pushed to datum_get.
 * Confirmed: MOV ECX,[0x6325a4] / PUSH EAX / PUSH ECX / CALL 0x119320 ->
 *   datum_get(actor_data, actor_handle); ADD ESP,0x8 (cdecl, 2 args).
 * Confirmed: guards are all against the datum_get result in EAX, no base bias:
 *   CMP word ptr [EAX+0xa0],0x3 / JNZ; MOV CL,[EAX+0xa7] / TEST / JZ;
 *   MOV CL,[EAX+0xa2] / TEST / JNZ; MOV CL,[EAX+0x15c] / TEST / JNZ.
 * Confirmed: INC word ptr [EAX+0xaa] — 16-bit increment, no clamp, no return.
 * Unknown: the meanings of the 0xa2, 0x15c gate bytes and the 0xaa counter;
 *   no string or assert evidence in this function, so no semantic names. */
void FUN_00012be0(int actor_handle)
{
  char *actor;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (*(short *)(actor + 0xa0) == 3 && *(char *)(actor + 0xa7) != '\0' &&
      *(char *)(actor + 0xa2) == '\0' && *(char *)(actor + 0x15c) == '\0') {
    *(short *)(actor + 0xaa) = *(short *)(actor + 0xaa) + 1;
  }
}

/* 0x12e50 — FUN_00012e50: check if actor is in a valid 'swarm flying' state
 * and its state timer has not yet expired.
 *
 * Looks up the actor record via actor_data (0x6325a4) and checks:
 *   actor+0xa0 (short): must equal 3 (swarm flying state)
 *   actor+0xa7 (char):  must be non-zero (active flag)
 * If both conditions hold, reads actor+0xac (int, state end time) and
 * returns true iff game_time_get() <= actor+0xac + 0x1e.
 *
 * Confirmed: cdecl, 1 stack arg (actor_handle). Returns bool.
 * Confirmed: ESI = datum_get result + 0x9c; offsets relative to ESI.
 * Confirmed: SETGE AL after CMP EDX,EAX (EDX=*(int*)(ESI+0x10)+0x1e,
 *   AX=game_time_get()), so bVar1 = (EDX >= EAX). */
bool FUN_00012e50(int actor_handle)
{
  char *actor;
  bool result;

  actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle) + 0x9c;
  result = false;
  if (*(short *)(actor + 4) == 3 && *(char *)(actor + 0xb) != '\0') {
    result = *(int *)(actor + 0x10) + 0x1e >= game_time_get();
  }
  return result;
}

/* 0x12ea0 — sqrtf wrapper. */
float FUN_00012ea0(float x)
{
  return sqrtf(x);
}

/* 0x12eb0 — Scale a 2D vector: out = scale * in. */
void FUN_00012eb0(float *in, float scale, float *out)
{
  out[0] = scale * in[0];
  out[1] = scale * in[1];
}

/* 0x12ed0 — Squared magnitude of a 2D vector. */
float FUN_00012ed0(float *v)
{
  return v[0] * v[0] + v[1] * v[1];
}

/* 0x12ef0 — Magnitude of a 2D vector. */
float FUN_00012ef0(float *v)
{
  return sqrtf(v[0] * v[0] + v[1] * v[1]);
}

/* 0x12f10 — Normalize a 2D vector in-place and return its magnitude.
 * Despite the kb.json name "magnitude3d", only operates on v[0] and v[1].
 * If magnitude exceeds epsilon, divides each component by it so v becomes
 * a unit vector. Returns the original magnitude, or 0.0f if too small. */
float magnitude3d(float *v)
{
  float mag;
  float scale;

  mag = sqrtf(v[0] * v[0] + v[1] * v[1]);
  if (fabsf(mag) >= *(double *)0x2533d0) {
    scale = 1.0f / mag;
    v[0] = v[0] * scale;
    v[1] = v[1] * scale;
    return mag;
  }
  return 0.0f;
}

/* 0x12f60 — Dot product of two 2D vectors. */
float FUN_00012f60(float *a, float *b)
{
  return a[0] * b[0] + a[1] * b[1];
}

/* 0x12f80 — Compute out = base + scale * direction (3-component). */
float *vector3d_scale_add(float *base, float *direction, float scale,
                          float *out)
{
  out[0] = scale * direction[0] + base[0];
  out[1] = scale * direction[1] + base[1];
  out[2] = scale * direction[2] + base[2];
  return out;
}

/* 0x12fb0 — Scale a 3D vector: out = scale * in. */
void FUN_00012fb0(float *in, float scale, float *out)
{
  out[0] = scale * in[0];
  out[1] = scale * in[1];
  out[2] = scale * in[2];
}

/* 0x12fe0 — Magnitude of a 3D vector. */
float FUN_00012fe0(float *v)
{
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Normalize a 3D vector in-place.
 * Computes the magnitude (Euclidean length) of v, and if it exceeds a
 * small epsilon threshold (~0.0001), divides each component by the
 * magnitude so v becomes a unit vector. Returns the original magnitude,
 * or 0.0f if the vector was too small to normalize. */
float normalize3d(float *v)
{
  float mag;
  float scale;

  mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  /* Original (0x13010): FCOMP ABS(mag) against the *double* 0.0001 at 0x2533d0
     and normalize only when |mag| >= 0.0001, otherwise return 0.0 (0x2533c0)
     leaving v unchanged.  A prior lift mis-read 0x2533d0 as a float
     (-3.69e19) — making the threshold always-true — and substituted a
     `mag == 0.0f` guard.  That let denormalized / near-zero (but nonzero)
     vectors be divided into a non-unit result that later tripped
     assert_valid_real_normal3d (actor_looking.c:529 via FUN_00028660).  Read
     the threshold as a double to restore the original early-out. */
  if (fabsf(mag) >= *(double *)0x2533d0) {
    scale = 1.0f / mag;
    v[0] = v[0] * scale;
    v[1] = v[1] * scale;
    v[2] = v[2] * scale;
    return mag;
  }
  return 0.0f;
}

/* FUN_00013070 (0x13070) — Dot product of two 3D vectors.
 * Confirmed: cdecl, 2 pointer args. Pure FPU leaf.
 * Confirmed: computes a.z*b.z + a.y*b.y + a.x*b.x (accumulation order). */
float FUN_00013070(float *a, float *b)
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* 0x13090 — Subtract two 3D vectors: out = a - b. */
void FUN_00013090(float *a, float *b, float *out)
{
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

/* 0x130d0 — Ray-cast between two points. Computes the direction vector
 * (point_b - point_a) and delegates to FUN_0014df70 for the actual
 * collision test along that direction from point_a. */
bool FUN_000130d0(uint32_t collision_flags, float *point_a, float *point_b,
                  int max_distance, int16_t *collision_result)
{
  float direction[3];

  direction[0] = point_b[0] - point_a[0];
  direction[1] = point_b[1] - point_a[1];
  direction[2] = point_b[2] - point_a[2];
  return FUN_0014df70(collision_flags, point_a, direction, max_distance,
                      collision_result);
}

/* 0x21370 — Sine of a float (x87 FSIN). */
float FUN_00021370(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)sin(
    (double)x); /* VC71 /Oi inlines as FSIN (matches original) */
#else
  return x87_fsin(x);
#endif
}

/* 0x21380 — Cosine of a float (x87 FCOS). */
float FUN_00021380(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)cos(
    (double)x); /* VC71 /Oi inlines as FCOS (matches original) */
#else
  return x87_fcos(x);
#endif
}

/* 0x21390 — Tangent of a float (x87 FPTAN). */
float FUN_00021390(float x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)tan(
    (double)x); /* VC71 /Oi inlines as FPTAN (matches original) */
#else
  return x87_fsin(x) / x87_fcos(x);
#endif
}

/* 0x213a0 — 2D cross product (z-component): a[0]*b[1] - a[1]*b[0]. */
float FUN_000213a0(float *a, float *b)
{
  return b[1] * a[0] - a[1] * b[0];
}

/* 0x213c0 — Compute out = a + b (3-component). */
void vector3d_add(float *a, float *b, float *out)
{
  out[0] = a[0] + b[0];
  out[1] = a[1] + b[1];
  out[2] = a[2] + b[2];
}

/* 0x21410 — Check if a float is valid (not NaN/Inf). */
int FUN_00021410(uint32_t bits)
{
  return (bits & 0x7f800000) != 0x7f800000;
}

/* 0x21f70 — Float approximate equality check within epsilon. */
int FUN_00021f70(float a, float b)
{
  float diff = a - b;
  if ((*(uint32_t *)&diff & 0x7f800000) == 0x7f800000)
    return 0;
  if (fabsf(diff) < *(double *)0x2549d8)
    return 1;
  return 0;
}

/* 0x21fb0 — valid_real_normal3d: check whether a 3D vector is a valid
 * unit normal (length within epsilon of 1.0).
 *
 * Computes squared_length = dot(v, v) and returns true if
 * |squared_length - 1.0f| < 0.001f.
 *
 * Also rejects NaN/infinity by testing the exponent bits.
 *
 * Confirmed: FLD / FMUL / FADDP computes dot(v, v) on x87 stack.
 * Confirmed: FSUB [0x2533c8] subtracts 1.0f.
 * Confirmed: FABS / FCOMP double ptr [0x2549d8] compares against
 * (double)0.001f. */
bool valid_real_normal3d(float *v)
{
  float sq_len = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  float diff = sq_len - 1.0f;

  if ((*(unsigned int *)&diff & 0x7f800000) == 0x7f800000) {
    return 0;
  }

  return fabsf(diff) < 0.001f;
}

/* 0x28610 — Validate that a 2D vector is a unit normal.
 * Checks that x²+y² is within epsilon of 1.0 and not NaN/Inf. */
int valid_real_normal2d(float *v)
{
  float diff = (v[0] * v[0] + v[1] * v[1]) - 1.0f;
  if ((*(uint32_t *)&diff & 0x7f800000) == 0x7f800000)
    return 0;
  if (fabsf(diff) < *(double *)0x2549d8)
    return 1;
  return 0;
}
