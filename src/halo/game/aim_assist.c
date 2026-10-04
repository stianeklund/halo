#include "../../x87_math.h"

void FUN_000a54b0(void)
{
  int16_t local_player_index;
  char *player_data;
  int16_t palette_index;
  int particle_system_tag_index;
  void *scenario;
  char *palette_element;

  if (*(char *)0x32574c == '\0' || *(char *)0x2ef7ee == '\0')
    return;

  local_player_index = *(int16_t *)0x506548;
  if (local_player_index == -1)
    return;

  player_data = (char *)weather_particle_system_get(local_player_index);
  *(int16_t *)(player_data + 0x14) = *(int16_t *)0x506784;
  *(int *)(player_data + 0x10) = *(int *)0x506780;

  *(char *)(player_data + 0x1a) = (char)FUN_0018f3e0(
    player_data + 0x10, (void *)0x506550, (int16_t *)(player_data + 0x18));

  palette_index = *(int16_t *)(player_data + 0x18);
  particle_system_tag_index = -1;

  if (palette_index != -1) {
    scenario = scenario_get();
    palette_element = (char *)tag_block_get_element((char *)scenario + 0x1b4,
                                                    (int)palette_index, 0xf0);
    particle_system_tag_index = *(int *)(palette_element + 0x2c);
  }

  if (*(int *)player_data != particle_system_tag_index) {
    if (*(int *)player_data != -1) {
      weather_particle_system_delete(local_player_index);
    }
    if (particle_system_tag_index != -1) {
      weather_particle_system_new(local_player_index, particle_system_tag_index,
                                  1.0f);
    }
  }

  if (*(int *)player_data != -1) {
    weather_particle_system_render(local_player_index);
  }
}

/* FUN_000a5590 (0xa5590)
 *
 * Linear falloff ramp over a float pair. Both parameters are proven floats by
 * the body itself: FLD float ptr [EBP+0x8] and FLD float ptr [EBP+0xc].
 * Confirmed from disassembly (no calls, no globals other than .rdata):
 *   FLD [EBP+0xc]; FMUL [0x253398]; FSTP [EBP-0x4]   -> half = range * 0.5f
 *   FLD [EBP+0x8]; FCOMP [EBP+0xc]; TEST AH,0x1; JNZ -> C0 set means
 *     value < range, so the FALL-THROUGH (value >= range) returns
 *     FLD [0x2533c0] = 0.0f.
 *   FLD [EBP+0x8]; FCOMP [EBP-0x4]; TEST AH,0x41; JP -> JP is taken only when
 *     neither C0 nor C3 is set (value > half), so the FALL-THROUGH
 *     (value < half or value == half) returns FLD [0x2533c8] = 1.0f.
 *   FLD [EBP+0xc]; FSUB [EBP+0x8]; FLD [EBP+0xc]; FSUB [EBP-0x4]; FDIVP
 *     -> ST(1)/ST(0) = (range - value) / (range - half), in that operand
 *     order.
 * 0x253398 = 0.5f, 0x2533c0 = 0.0f, 0x2533c8 = 1.0f are the shared .rdata
 * constants used throughout the binary; read through their addresses so the
 * reference's FLD/FMUL m32 form is preserved (a literal would let the
 * compiler pick FLDZ/FLD1).
 * The semantic meaning of the two parameters is unknown: the only callers are
 * FUN_000a55e0 (which just forwards its own arguments) and the unported
 * FUN_000a5ac0, so the names stay generic.
 */
float FUN_000a5590(float value, float range)
{
  float half;

  half = range * *(float *)0x253398;
  if (value >= range) {
    return *(float *)0x2533c0;
  }
  if (value <= half) {
    return *(float *)0x2533c8;
  }
  return (range - value) / (range - half);
}

/* FUN_000a55e0 (0xa55e0)
 *
 * Multiplies the results of two calls to FUN_000a5590 (0xa5590, cdecl,
 * 2 float args -> float in ST0; the callee loads both stack slots with
 * FLD float ptr, so these forwarded arguments are floats, not ints).
 * Confirmed from disassembly:
 *   000a55ec CALL FUN_000a5590(arg1, arg2) -> FSTP [EBP-4] (saved)
 *   000a55fc CALL FUN_000a5590(arg3, arg4) -> FMUL [EBP-4] (result * saved)
 * Argument push order at each call site is the standard cdecl
 * right-to-left push (second param pushed first), so callee arg order is
 * NOT swapped: call 1 is FUN_000a5590(arg1, arg2), call 2 is
 * FUN_000a5590(arg3, arg4). No evidence of the semantic meaning of arg1-4
 * beyond their float type -- kept as generic names, and no callers found in
 * this artifact (xrefs_to: none).
 */
float FUN_000a55e0(float arg1, float arg2, float arg3, float arg4)
{
  volatile float saved;

  saved = FUN_000a5590(arg1, arg2);
  return FUN_000a5590(arg3, arg4) * saved;
}

/* unit_get_aim_assist_parameters (0xa5610)
 *
 * Register ABI proven by the body: ESI = EAX (unit handle, compared to -1
 * before any call), EBX is used only as the output float-array base
 * (FSTP [EBX+0x0..0x10]) and is never written, [EBP+8] is the one stack
 * argument: pushed whole to 0xfc780 (PUSH EAX) but tested only as
 * CMP AX,0xffff, so it is kept int and compared through int16_t. Returns AL.
 *   0x13d680(unit, 3); movsx [unit+0x2a2] -> 0x1adeb0(unit, idx)
 *   0x13d680(weapon, 4); tag_get('weap', *weapon_obj)
 *   zoom_level == -1 && (tag[0x308] & 0x20) -> return 0
 *   mag = 0xfc780(weapon, zoom_level); inv = 1.0f / mag  (FDIV ST0,ST1 keeps
 *   mag live in ST1): out[0] = inv*tag[0x3e4], out[1] = mag*tag[0x3e8],
 *   out[2] = inv*tag[0x3ec], out[3] = mag*tag[0x3f0],
 *   out[4] = (tag[0x3f4] > tag[0x3e4] ? tag[0x3f4] : tag[0x3e4]) * inv
 *   (FCOMP + TEST AH,0x41 + JNZ: unordered/<=/== take the 0x3e4 branch).
 * Tag field meanings are unproven; offsets stay raw.
 */
bool unit_get_aim_assist_parameters(int unit_handle, float *out_params,
                                    int zoom_level)
{
  int weapon_handle;
  char *weapon_tag;
  float magnification;
  float inverse;

  if (unit_handle != -1) {
    weapon_handle = unit_inventory_get_weapon(
      unit_handle,
      *(int16_t *)((char *)object_get_and_verify_type(unit_handle, 3) + 0x2a2));
    if (weapon_handle != -1) {
      weapon_tag = (char *)tag_get(
        0x77656170, *(int *)object_get_and_verify_type(weapon_handle, 4));
      if ((int16_t)zoom_level != -1 ||
          (*(uint8_t *)(weapon_tag + 0x308) & 0x20) == 0) {
        magnification =
          weapon_get_zoom_magnification(weapon_handle, zoom_level);
        inverse = *(float *)0x2533c8 / magnification;
        out_params[0] = inverse * *(float *)(weapon_tag + 0x3e4);
        /* 0xa5683 FST [EBP-4]: out_params[0] uses the full-width quotient,
         * every later use reloads the float-narrowed copy (0xa5699/0xa56c8). */
        HALO_FLT_ROUNDTRIP(inverse);
        out_params[1] = magnification * *(float *)(weapon_tag + 0x3e8);
        out_params[2] = inverse * *(float *)(weapon_tag + 0x3ec);
        out_params[3] = magnification * *(float *)(weapon_tag + 0x3f0);
        if (*(float *)(weapon_tag + 0x3f4) > *(float *)(weapon_tag + 0x3e4)) {
          out_params[4] = *(float *)(weapon_tag + 0x3f4) * inverse;
        } else {
          out_params[4] = *(float *)(weapon_tag + 0x3e4) * inverse;
        }
        return true;
      }
    }
  }
  return false;
}

/* compare_targets (0xa5700)
 *
 * qsort comparator over the 0x38-byte candidate-target records built by
 * FUN_000a5f00 and sorted at 0xa60a8 (the only reference to this address is
 * that DATA xref, i.e. the qsort function-pointer argument).
 *
 * Confirmed from the disassembly at 0xa5700 (ECX = param a = [EBP+8],
 * EDX = param b = [EBP+0xc]; each FLD/FCOMP pair loads a's field and compares
 * it against b's, so FNSTSW C0 means a < b and C3 means a == b):
 *   0xa5709 TEST AH,0x41 / JZ   -> a[0x30] >  b[0x30] -> -1
 *   0xa5716 TEST AH,0x05 / JNP  -> a[0x30] <  b[0x30] -> +1
 *   0xa5723 TEST AH,0x41 / JZ   -> a[0x34] >  b[0x34] -> -1
 *   0xa5730 TEST AH,0x05 / JNP  -> a[0x34] <  b[0x34] -> +1
 *   0xa573d TEST AH,0x05 / JNP  -> a[0x28] <  b[0x28] -> -1
 *   0xa574a TEST AH,0x41 / JZ   -> a[0x28] >  b[0x28] -> +1
 *   0xa5757 TEST AH,0x05 / JP   -> fall through when a[0x2c] < b[0x2c] -> -1
 *   0xa576b TEST AH,0x41 / JNZ  -> equal -> tie-break; else +1
 * Two of the four keys therefore sort descending (0x30, 0x34) and two
 * ascending (0x28, 0x2c).  The tie-break at 0xa577f loads the full dword at
 * offset 0 of each record and masks it with 0xffff before subtracting
 * (MOV/AND/AND/SUB, not MOVZX), i.e. the low 16 bits of the datum handle the
 * record carries at offset 0 -- that same dword is passed as the handle
 * argument to FUN_000a5830 at 0xa60ea.  The semantic meaning of the four
 * float keys is unknown from this evidence, so they stay raw offsets.
 */
int compare_targets(const void *a, const void *b)
{
  const char *ra;
  const char *rb;

  ra = (const char *)a;
  rb = (const char *)b;

  if (*(const float *)(ra + 0x30) > *(const float *)(rb + 0x30))
    return -1;
  if (*(const float *)(ra + 0x30) < *(const float *)(rb + 0x30))
    return 1;

  if (*(const float *)(ra + 0x34) > *(const float *)(rb + 0x34))
    return -1;
  if (*(const float *)(ra + 0x34) < *(const float *)(rb + 0x34))
    return 1;

  if (*(const float *)(ra + 0x28) < *(const float *)(rb + 0x28))
    return -1;
  if (*(const float *)(ra + 0x28) > *(const float *)(rb + 0x28))
    return 1;

  if (*(const float *)(ra + 0x2c) < *(const float *)(rb + 0x2c))
    return -1;
  if (*(const float *)(ra + 0x2c) > *(const float *)(rb + 0x2c))
    return 1;

  return (int)(*(const uint32_t *)ra & 0xffff) -
         (int)(*(const uint32_t *)rb & 0xffff);
}

/* reciprocal_square_root (0xa57a0)
 *
 * Whole function, verbatim from the disassembly (7 instructions, no calls):
 *   000a57a0 PUSH EBP
 *   000a57a1 MOV  EBP,ESP
 *   000a57a3 FLD  float ptr [EBP+0x8]   -> the single argument is a float
 *   000a57a6 FSQRT                      -> ST(0) = sqrt(value)
 *   000a57a8 FDIVR float ptr [0x2533c8] -> ST(0) = [0x2533c8] / sqrt(value)
 *   000a57ae POP  EBP
 *   000a57af RET                        -> result returned in ST(0)
 *
 * FDIVR is the reversed form, so the .rdata constant is the dividend, not the
 * divisor.  0x2533c8 is the same .rdata slot FUN_000a5590 (0xa5590) loads as
 * its 1.0f return value, so the constant is 1.0f and the function is
 * 1.0f / sqrt(value).
 *
 * No domain guard: a negative or zero argument is passed straight to FSQRT,
 * matching the original.  The Ghidra decompile for this address is stale (an
 * empty `void __cdecl FUN_000a57a0(void)`); the disassembly above is the
 * evidence used here, and it also proves the float parameter and float return
 * that the kb.json decl previously spelled `void (void)`.
 */
real reciprocal_square_root(real value)
{
  return 1.0f / sqrtf(value);
}

/* limit3d (0xa57b0)
 *
 * Clamps a vector's length to `length`; returns TRUE when it was clamped.
 * Confirmed from disassembly at 0xa57b0:
 *   FLD [ECX+8]; FLD [ECX+4]; FLD [ECX] then i*i + j*j + k*k (in that
 *   association) -> dot.
 *   FLD [EBP+0xc]; FMUL [EBP+0xc]; FLD dot; FCOMPP; TEST AH,0x41; JNZ ->
 *   skip unless dot > length*length.
 *   FSQRT; MOV AL,1; FDIVR [EBP+0xc] -> scale = length / sqrt(dot);
 *   each component multiplied by scale in i, j, k order.
 *   Else arm: XOR AL,AL. Only AL is written, so the return is a byte.
 * Callers: update_alien_scout_physics (0x1b8570) and
 * accelerate_to_velocity3d (real_math.c); both test AL.
 */
boolean limit3d(real_vector3d *vector, real length)
{
  real dot;
  real scale;

  dot = vector->i * vector->i + vector->j * vector->j + vector->k * vector->k;
  if (dot > length * length) {
    scale = length / sqrtf(dot);
    vector->i = scale * vector->i;
    vector->j = scale * vector->j;
    vector->k = scale * vector->k;
    return 1;
  }

  return 0;
}

/* set_real_euler_angles2d (0xa5810)
 *
 * Two-store setter, straight from the disassembly:
 *   0xa5813 MOV EAX,[EBP+0x8]     -> destination pointer (arg 1)
 *   0xa5816 FLD  float ptr [EBP+0xc]  -> arg 2 loaded as a float
 *   0xa5819 MOV ECX,[EBP+0x10]    -> arg 3 copied as a plain dword
 *   0xa581c FSTP float ptr [EAX+0x4]  -> arg 2 stored at destination +0x04
 *   0xa581f MOV  [EAX],ECX        -> arg 3 stored at destination +0x00
 *
 * The argument slots are fixed, so the mapping is unambiguous: arg 2 lands at
 * +0x04 and arg 3 at +0x00.  Which component is yaw and which is pitch is NOT
 * proven -- the function has no callers in this build and no assert string --
 * so the parameters are named after the offsets they write.  Arg 2 is a float
 * by the FLD; arg 3's form (a dword copy) does not prove its type, but the
 * symbol name and the adjacent float make `real` the recovered spelling.
 */
void set_real_euler_angles2d(real *angles, real angle_04, real angle_00)
{
  angles[1] = angle_04;
  angles[0] = angle_00;
}

/* FUN_000a5830 (0xa5830)
 *
 * Line-of-sight test (asserts name c:\halo\SOURCE\game\aim_assist.c).
 * Pushes collision user 6, casts a ray (flags 0xc2ad) from `point` toward
 * the float[3] at `elem_data`, ignoring the root parent of arg3 ([EBP+0x10],
 * used as an object handle). Returns 1 (BL) when nothing is hit, or when
 * the hit type (result +0x00, int16) is 3 and the root parent of the hit
 * object (result +0x38, [EBP-0x24]) equals the root parent of arg4.
 * The collision result buffer spans [EBP-0x5c, EBP-0xc) = 0x50 bytes; the
 * ray delta vector is [EBP-0xc].
 */
char FUN_000a5830(float *point, void *elem_data, float *arg3, int arg4)
{
  char result;
  int ignore_root;
  float direction[3];
  int collision_result[0x14];

  result = 0;
  if (*(int16_t *)0x4761d8 >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\game\\aim_assist.c", 0x15e, 1);
    system_exit(-1);
  }

  {
    int16_t depth = *(int16_t *)0x4761d8;
    *(int16_t *)(0x5a8c80 + (int)depth * 2) = 6;
    *(int16_t *)0x4761d8 = (int16_t)(depth + 1);
  }

  ignore_root = object_get_root_parent((int)arg3);
  direction[0] = ((float *)elem_data)[0] - point[0];
  direction[1] = ((float *)elem_data)[1] - point[1];
  direction[2] = ((float *)elem_data)[2] - point[2];

  if (!FUN_0014df70(0xc2ad, point, direction, ignore_root,
                    (int16_t *)collision_result) ||
      (*(int16_t *)collision_result == 3 &&
       object_get_root_parent(collision_result[0x38 / 4]) ==
         object_get_root_parent(arg4))) {
    result = 1;
  }

  if (*(int16_t *)0x4761d8 <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\game\\aim_assist.c", 0x16f, 1);
    system_exit(-1);
  }
  *(int16_t *)0x4761d8 -= 1;

  return result;
}

/* object_compute_autoaim_target (0xa5920)
 *
 * Computes the point on an object's autoaim pill that the ray (point,
 * direction) should aim at.  It takes the closest point of the pill's axis
 * segment to the ray, clamped to the segment, then moves it toward the ray by
 * at most the pill radius.  Callers: FUN_000a5ac0 (0xa5ad8) and FUN_000a5c60
 * (0xa5c7a).
 *
 * Register ABI (confirmed from the disassembly): EBX = point, ESI = direction,
 * EDI = out_point.  The body reads all three without ever writing them, and
 * both callers load them right before the CALL (0xa5aca/0xa5ace/0xa5ad2 and
 * 0xa5c6a/0xa5c6e/0xa5c72).  object_index is the only stack argument ([EBP+8]).
 *
 * Confirmed from the disassembly at 0xa5920:
 *   - biped_get_autoaim_pill fills base [EBP-0x20], axis [EBP-0x14] and the
 *     radius dword [EBP-8].  The radius is later pushed as limit3d's float
 *     argument (0xa5a45), so it is a float; the callee's decl spells that slot
 *     `int *`.
 *   - cross = axis x direction.  When |cross|^2 > 0 (FCOMP 0.0 / TEST AH,0x41
 *     / JNE), t = dot(offset x direction, cross) / |cross|^2 with
 *     offset = point - base, clamped to [0, 1] (FCOM -> JP / JNE pairs), and
 *     out_point = axis * t + base.  Otherwise out_point = base (dword copy).
 *   - The sums are evaluated z, y, x (0xa5966-0xa5974, 0xa59c5-0xa59d3).
 *   - perpendicular = g - direction * dot(g, direction), g = out_point - point,
 *     with the dot summed x, z, y (0xa5a5e-0xa5a6f).  It is limited to the
 *     radius by limit3d (return ignored) and subtracted from out_point.
 */
void object_compute_autoaim_target(float *point, float *direction,
                                   float *out_point, int object_index)
{
  real_point3d base;
  real_vector3d axis;
  real radius;
  real_vector3d cross;
  real_vector3d offset;
  real_vector3d offset_cross;
  real_vector3d perpendicular;
  real cross_squared;
  real t;
  real projection;
  real gx;
  real gy;
  real gz;

  biped_get_autoaim_pill(object_index, &base.x, &axis.i, (int *)&radius);

  cross.i = axis.j * direction[2] - axis.k * direction[1];
  cross.j = axis.k * direction[0] - axis.i * direction[2];
  cross.k = axis.i * direction[1] - axis.j * direction[0];
  cross_squared = cross.k * cross.k + cross.j * cross.j + cross.i * cross.i;

  if (cross_squared > 0.0f) {
    offset.i = point[0] - base.x;
    offset.j = point[1] - base.y;
    offset.k = point[2] - base.z;
    offset_cross.i = offset.j * direction[2] - offset.k * direction[1];
    offset_cross.j = offset.k * direction[0] - offset.i * direction[2];
    offset_cross.k = offset.i * direction[1] - offset.j * direction[0];
    t = (offset_cross.k * cross.k + offset_cross.j * cross.j +
         offset_cross.i * cross.i) /
        cross_squared;
    if (t < 0.0f) {
      t = 0.0f;
    } else if (t > 1.0f) {
      t = 1.0f;
    }
    out_point[0] = axis.i * t + base.x;
    out_point[1] = axis.j * t + base.y;
    out_point[2] = axis.k * t + base.z;
  } else {
    out_point[0] = base.x;
    out_point[1] = base.y;
    out_point[2] = base.z;
  }

  gx = out_point[0] - point[0];
  gy = out_point[1] - point[1];
  gz = out_point[2] - point[2];
  projection = -(gx * direction[0] + gz * direction[2] + gy * direction[1]);
  perpendicular.i = projection * direction[0] + gx;
  perpendicular.j = projection * direction[1] + gy;
  perpendicular.k = projection * direction[2] + gz;
  limit3d(&perpendicular, radius);

  out_point[0] -= perpendicular.i;
  out_point[1] -= perpendicular.j;
  out_point[2] -= perpendicular.k;
}

/* FUN_000a5ac0 (0xa5ac0)
 *
 * Fills one aim_assist_record_t for `object_index` as seen from `point`
 * along `direction`, and returns 1 when either weight is positive.  Sole
 * caller: FUN_000a5d70 (0xa5e5d), which tests AL.
 *
 * Confirmed from the disassembly at 0xa5ac0:
 *   - object_index is stored at +0x00 before object_compute_autoaim_target is
 *     called with EBX = point, ESI = direction, EDI = &record->field_04.
 *   - field_10 = field_04 - point; field_1c = field_10 (dword copy), then
 *     normalized in place.  The normalize3d return goes to field_28.
 *   - the dot of field_1c and direction (summed z, y, x) is clamped to
 *     [-1, 1].  Its acos (_CIacos, 0x1d94f0) goes to field_2c.
 *   - with a non-NULL cone_spec:
 *       field_30 = FUN_000a5590(angle, cone[0]) * FUN_000a5590(distance,
 * cone[1]) field_34 = FUN_000a5590(angle, cone[2]) * FUN_000a5590(distance,
 * cone[3]) The distance term is computed first each time.  When field_34 > 0
 * and the object's 'unit' tag has bit 0x80000 set at +0x17c, field_34 is scaled
 * by the float at +8 of game_globals +0x110 element 0 (size 0x80). With a NULL
 * cone_spec, both fields are zeroed with integer stores.
 *   - It returns MOV EAX,1 / XOR EAX,EAX, so the return is a full int.
 */
int FUN_000a5ac0(float *cone_spec, int object_index, float *point,
                 float *direction, void *out_record)
{
#if defined(_MSC_VER) && !defined(__clang__)
  /* VC71 /Oi lowers acos() to _CIacos (0x1d94f0), as the original does;
   * block scope so the clang build keeps acosf (see real_math.c). */
  double acos(double x);
#endif
  aim_assist_record_t *record;
  char *unit_tag;
  real distance;
  real dot;
  real angle;
  real distance_weight;

  record = (aim_assist_record_t *)out_record;
  record->object_index = object_index;
  object_compute_autoaim_target(point, direction, &record->field_04.x,
                                object_index);

  record->field_10.i = record->field_04.x - point[0];
  record->field_10.j = record->field_04.y - point[1];
  record->field_10.k = record->field_04.z - point[2];
  record->field_1c = record->field_10;
  distance = normalize3d(&record->field_1c.i);
  record->field_28 = distance;

  dot = record->field_1c.k * direction[2] + record->field_1c.j * direction[1] +
        record->field_1c.i * direction[0];
  if (dot < -1.0f) {
    dot = -1.0f;
  } else if (dot > 1.0f) {
    dot = 1.0f;
  }
#if defined(_MSC_VER) && !defined(__clang__)
  angle = (float)acos((double)dot);
#else
  angle = acosf(dot);
#endif
  record->field_2c = angle;

  if (cone_spec != NULL) {
    distance_weight = FUN_000a5590(distance, cone_spec[1]);
    record->field_30 = FUN_000a5590(angle, cone_spec[0]) * distance_weight;
    distance_weight = FUN_000a5590(distance, cone_spec[3]);
    record->field_34 = FUN_000a5590(angle, cone_spec[2]) * distance_weight;
    if (record->field_34 > 0.0f) {
      unit_tag = (char *)tag_get(
        0x756e6974 /* 'unit' */,
        ((object_datum_t *)object_get_and_verify_type(record->object_index, 3))
          ->definition_index);
      if ((*(uint32_t *)(unit_tag + 0x17c) & 0x80000) != 0) {
        record->field_34 =
          *(real *)((char *)tag_block_get_element(
                      (char *)game_globals_get() + 0x110, 0, 0x80) +
                    8) *
          record->field_34;
      }
    }
  } else {
    record->field_30 = 0.0f;
    record->field_34 = 0.0f;
  }

  if (record->field_30 > 0.0f || record->field_34 > 0.0f)
    return 1;
  return 0;
}

/* FUN_000a5c60 (0xa5c60)
 *
 * Sole caller: FUN_000ac220 (0xac355), which tests AL.  EBX = point,
 * ESI = direction, EDI = out_point for object_compute_autoaim_target;
 * FUN_000a5830 gets (point, out_point, ignore_object_index, object_index).
 * The dot of direction and out_vector is summed z, y, x and clamped to
 * [-1, 1] before _CIacos (0x1d94f0).
 */
char FUN_000a5c60(int object_index, float *point, float *direction,
                  int ignore_object_index, float *out_point, float *out_vector,
                  float *out_distance, float *out_angle)
{
#if defined(_MSC_VER) && !defined(__clang__)
  double acos(double x);
#endif
  char result;
  real distance;
  real dot;

  result = 0;
  object_compute_autoaim_target(point, direction, out_point, object_index);
  if (FUN_000a5830(point, out_point, (float *)ignore_object_index,
                   object_index)) {
    out_vector[0] = out_point[0] - point[0];
    out_vector[1] = out_point[1] - point[1];
    out_vector[2] = out_point[2] - point[2];
    distance = normalize3d(out_vector);
    *out_distance = distance;
    if (distance != 0.0f) {
      dot = CLAMP(direction[2] * out_vector[2] + direction[1] * out_vector[1] +
                      direction[0] * out_vector[0],
                  -1.0f, 1.0f);
#if defined(_MSC_VER) && !defined(__clang__)
      *out_angle = (float)acos((double)dot);
#else
      *out_angle = acosf(dot);
#endif
      return 1;
    }
  }
  return result;
}

/* FUN_000a5d70 (0xa5d70)
 *
 * Recursive per-cluster worker behind FUN_000a5f00 (0xa5f00, unported): walks
 * the linked list of objects rooted at `object_handle` (object field +0xc4 =
 * "next object in this cluster"), filters each object by type mask, deletion
 * flag, unit health fraction, cone containment (FUN_00110210), team
 * allegiance and a "bipd"-tag flag, appends any matching candidate's 0x38-byte
 * record (built by FUN_000a5ac0, unported) into `out_buffer`, then recurses
 * into the object's linked cluster (field +0xc8 = "next cluster", -1
 * terminated) before continuing the object-list walk.
 *
 * Confirmed from disassembly at 0xa5d70:
 *   - The decompiler's `in_stack_XXXXXXXX` stack-slot names are offset -4
 *     from the true [EBP+N] disassembly locations in this function (e.g. its
 *     `in_stack_0000000c` is really [EBP+0x10], `in_stack_00000020` is really
 *     [EBP+0x24]). Every parameter below was derived from the raw [EBP+N]
 *     operands and the recursive self-call's argument marshalling at
 *     0xa5e93-0xa5ecb (which forwards params 3-9 unchanged and only threads
 *     the next handle / remaining capacity / advanced buffer pointer), not
 *     from the decompiler's mislabeled variable names.
 *   - param_1 ([EBP+8]) is never read in this function; it is only forwarded
 *     unchanged as the first pushed arg to FUN_000a5ac0 (0xa5e5c) and to the
 *     recursive self-call (0xa5eca).
 *   - The function returns its running match count in AX only (0xa5eec
 *     `MOV AX,BX`); the caller's `ADD EBX,EAX` (0xa5ed3) only ever executes
 *     right after the recursive CALL itself, so the upper 16 bits of EAX are
 *     always freshly the callee's own (equally AX-only) return and never
 *     carry stale garbage into a live comparison -- every consumer of the
 *     count reads BX/AX, never the high word. int16_t is exact.
 */
int16_t FUN_000a5d70(void *param_1, int object_handle, float *arg_p3,
                     float *arg_p4, float arg_p5, float arg_sine,
                     float arg_cosine, int exclude_handle, int16_t query_team,
                     int16_t max_count, void *out_buffer)
{
  char *obj;
  char *unit_obj;
  char *tag_data;
  char cone_match;
  char record_built;
  int record_buf[14];
  int datum_handle;
  int next_cluster;
  int type_byte;
  int16_t total_count;

  datum_handle = object_handle;
  total_count = 0;

  do {
    obj = (char *)object_get_and_verify_type(datum_handle, -1);
    type_byte = *(unsigned char *)(obj + 0x64) & 0x1f;

    if (((1 << type_byte) & 3) != 0 && (*(unsigned char *)(obj + 4) & 1) == 0) {
      unit_obj = (char *)object_get_and_verify_type(datum_handle, 3);

      if (*(float *)(unit_obj + 0x32c) < *(float *)0x2533c8) {
        cone_match = FUN_00110210((float *)(obj + 0x50), *(float *)(obj + 0x5c),
                                  arg_p3, arg_p4, arg_p5, arg_sine, arg_cosine);

        if (cone_match != 0) {
          if (((1 << type_byte) & 1) != 0 &&
              (*(unsigned char *)(obj + 0xb6) & 4) == 0 &&
              datum_handle != exclude_handle) {
            if (game_allegiance_get_team_is_friendly(
                  query_team, *(int16_t *)(obj + 0x68))) {
              tag_data = (char *)tag_get(0x62697064, *(int *)obj);

              if ((*(unsigned int *)(tag_data + 0x17c) & 0x200000) == 0) {
                record_built = FUN_000a5ac0(param_1, datum_handle, arg_p3,
                                            arg_p4, record_buf);

                if (record_built != 0 && total_count < max_count) {
                  csmemcpy((char *)out_buffer + (int)total_count * 0x38,
                           record_buf, 0x38);
                  total_count = total_count + 1;
                }
              }
            }
          }

          next_cluster = *(int *)(obj + 0xc8);
          if (next_cluster != -1 && total_count < max_count) {
            total_count =
              (int16_t)(total_count +
                        FUN_000a5d70(
                          param_1, next_cluster, arg_p3, arg_p4, arg_p5,
                          arg_sine, arg_cosine, exclude_handle, query_team,
                          (int16_t)(max_count - total_count),
                          (char *)out_buffer + (int)total_count * 0x38));
          }
        }
      }
    }

    datum_handle = *(int *)(obj + 0xc4);
  } while (datum_handle != -1 && total_count < max_count);

  return total_count;
}

/* FUN_000a5f00 (0xa5f00)
 *
 * Gathers aim-assist candidate records for the cone `cone_spec` starting at
 * `starting_cluster`: finds the structure clusters inside the cone, collects
 * the objects in them and runs FUN_000a5d70 on each until `max_count`
 * records are filled.  Returns the record count in AX.  Sole caller:
 * FUN_000a6030 (0xa6099).
 *
 * Confirmed from the disassembly at 0xa5f00:
 *   - cone_spec arrives in EDI (comment-form @<edi> in kb.json) and EDI stays
 *     live to the FUN_000a5d70 calls, where it is pushed as that callee's
 *     first argument.
 *   - length = cone[1] > cone[3] ? cone[1] : cone[3] and
 *     angle = cone[0] > cone[2] ? cone[0] : cone[2] (FCOMP / TEST AH,0x41 /
 *     JNE takes the second operand).  It returns 0 unless length > 0 and
 *     angle > 0.
 *   - sine/cosine are FSIN/FCOS of angle.  structure_clusters_in_cone fills
 *     up to 0x200 int16 cluster indices ([EBP-0x410]).  object_find_in_cluster
 *     (flags 1) fills up to 0x800 object handles ([EBP-0x2410]).
 *   - per object: total += FUN_000a5d70(cone, object, point, direction,
 *     length, sine, cosine, arg4, arg5, max_count - total,
 *     out_buffer + total * 0x38).  The loop stops when the 16-bit total
 *     reaches the 16-bit max_count (CMP SI, word [EBP+0x1c] / JGE) or the
 *     objects run out.
 *   - arg4 and arg5 are forwarded as dwords: an object handle and the 16-bit
 *     team (see FUN_000a6030).  kb_reg_baseline pins their `float *` spelling.
 */
int16_t FUN_000a5f00(float *cone_spec, int16_t starting_cluster, float *point,
                     float *direction, float *arg4, float *arg5, int max_count,
                     void *out_buffer)
{
  int16_t cluster_indices[0x200];
  int object_indices[0x800];
  real length;
  real angle;
  real sine;
  real cosine;
  int16_t object_count;
  int16_t i;
  int16_t total_count;

  total_count = 0;
  if (cone_spec[1] > cone_spec[3]) {
    length = cone_spec[1];
  } else {
    length = cone_spec[3];
  }
  if (cone_spec[0] > cone_spec[2]) {
    angle = cone_spec[0];
  } else {
    angle = cone_spec[2];
  }

  if (length > 0.0f && angle > 0.0f) {
    sine = x87_fsin(angle);
    cosine = x87_fcos(angle);
    object_count = object_find_in_cluster(
      1,
      structure_clusters_in_cone(starting_cluster, point, direction, length,
                                 sine, cosine, 0x200, cluster_indices),
      cluster_indices, 0x800, object_indices);

    for (i = 0; i < object_count; i++) {
      total_count =
        (int16_t)(total_count +
                  FUN_000a5d70(cone_spec, object_indices[i], point, direction,
                               length, sine, cosine, (int)arg4,
                               (int16_t)(int)arg5,
                               (int16_t)(max_count - total_count),
                               (char *)out_buffer + (int)total_count * 0x38));
      if (total_count >= (int16_t)max_count)
        break;
    }
  }

  return total_count;
}

/* FUN_000a6030 (0xa6030)
 *
 * Locate the best candidate record inside the cone described by `cone_spec`,
 * starting from the structure cluster that contains `point`.
 *
 * Confirmed from the disassembly at 0xa6030:
 *   - param2 ([EBP+0xc], held in EBX) is the point: it is the sole argument to
 *     bsp3d_find_leaf_point (FUN_0018e720) at 0xa603f/0xa6053, and is
 *     forwarded unchanged to FUN_000a5f00 (0xa6097) and FUN_000a5830
 *     (0xa60ea) -- so FUN_000a5830's first parameter is that same point, not
 *     an object handle.
 *   - the FIRST stack argument to FUN_000a5f00 is EAX at 0xa6098, i.e. the
 *     cluster index loaded from the bsp leaf element at 0xa6072
 *     (MOV AX, word ptr [EAX+8]) and range-checked against -1 at 0xa6079.
 *     It is NOT param1.
 *   - param1 ([EBP+8]) is loaded into EDI at 0xa6082 and stays live across the
 *     CALL at 0xa6099: it is FUN_000a5f00's implicit @<edi> argument.  That
 *     callee reads four floats from it ([EDI+0]/[EDI+8] = angle,
 *     [EDI+4]/[EDI+0xc] = distance) and derives the cone length/sine/cosine it
 *     hands to structure_clusters_in_cone (0x198ad0).  EDI is reloaded with
 *     the return count at 0xa60a1, which is why the original's live range ends
 *     at the call.
 *   - [EBP+0x14] is an object handle: it is forwarded as a dword to
 *     FUN_000a5f00 (exclude handle for FUN_000a5d70) and to FUN_000a5830
 *     (passed to object_get_root_parent).  [EBP+0x18] is forwarded as a dword
 *     to FUN_000a5f00 and ends up as FUN_000a5d70's int16_t query_team; both
 *     callers load it with a zero-extending word read of player +0x20.  The
 *     (float *) casts below only match FUN_000a5f00's protected baseline decl.
 */
char FUN_000a6030(float *cone_spec, float *point, float *direction,
                  int ignore_object_index, int16_t team_index, void *out_struct)
{
  int16_t cluster_index;
  int16_t count;
  int16_t i;
  aim_assist_record_t candidates[0x40];

  if (FUN_0018e720((int)point) != -1) {
    cluster_index =
      *(int16_t *)((char *)tag_block_get_element(
                     (char *)scenario_get() + 0xe0,
                     FUN_0018e720((int)point) & 0x7fffffff, 0x10) +
                   8);
    if (cluster_index != -1) {
      count = (int16_t)FUN_000a5f00(cone_spec, cluster_index, point, direction,
                                    (float *)ignore_object_index,
                                    (float *)(int)team_index, 0x40, candidates);
      if (count > 0) {
        qsort(candidates, (size_t)count, sizeof(aim_assist_record_t),
              (qsort_compar_proc)compare_targets);
        for (i = 0; i < count; i++) {
          if (FUN_000a5830(point, &candidates[i].field_04,
                           (float *)ignore_object_index,
                           candidates[i].object_index)) {
            *(aim_assist_record_t *)out_struct = candidates[i];
            return 1;
          }
        }
      }
    }
  }

  return 0;
}

/* player_aim_projectile (0xa6130)
 *
 * Bends a player's projectile `forward` vector toward the best aim-assist
 * target and returns that target's object index (NONE when there is none).
 * Asserts name c:\halo\SOURCE\game\aim_assist.c (lines 0x4d, 0x4f, 0x8c).
 * Sole caller: weapon fire at 0xfd8bf (weapons.c), which stores EAX.
 *
 * Confirmed from the disassembly at 0xa6130:
 *   - `player_index` is a full 32-bit player datum handle: the caller pushes
 *     EBX = unit +0x1c8 whole (0xfd6f6/0xfd866) and 0xa6139 forwards the
 *     dword [EBP+8] to datum_get unchanged.  An int16_t parameter would drop
 *     the salt and make datum_get fail.
 *   - `forward` is checked with valid_real_normal3d (0xa6167); on failure the
 *     csprintf return is the display_assert reason (PUSH EAX at 0xa61a9).
 *   - collision user 6 is pushed around the whole body (0xa61b9-0xa61f6) and
 *     popped at 0xa641c-0xa6446 on both paths.
 *   - unit_get_aim_assist_parameters gets the aiming unit in EAX and the
 *     parameter array in EBX (0xa6201/0xa6205); its zoom argument is
 *     unit_get_zoom_level(aiming unit), not the player-control zoom.
 *   - FUN_000a6030 gets player->unit_handle (0xa6249) and the zero-extended
 *     word at player +0x20 (0xa6236 XOR ECX,ECX / MOV CX,[ESI+0x20]) as its
 *     4th/5th arguments.
 *   - the FUN_0014df70 return is not tested; the hit point (+0x18) is read
 *     unconditionally (the callee always fills it).
 *   - player +0x40 = result and player +0x44 = game_time_get() are stored on
 *     every path (0xa6450/0xa6458).
 * In the original the record and the collision result share stack bytes
 * (-0xa4..-0x6c); they are never live together, so separate locals are
 * equivalent. */
int player_aim_projectile(int player_index, float *origin, float *forward)
{
  player_data_t *player;
  object_datum_t *unit;
  int aiming_unit_index;
  int target_object_index;
  real aim_level;
  real distance;
  real dx;
  real dy;
  real dz;
  real aim_assist_parameters[5];
  real_point3d camera_position;
  real_vector3d camera_forward;
  real_vector3d aim_vector;
  real_vector3d offset_vector;
  real_vector3d test_vector;
  real_vector3d hit_vector;
  aim_assist_record_t record;
  struct collision_result collision;

  target_object_index = NONE;
  player = (player_data_t *)datum_get(player_data, player_index);
  aiming_unit_index = unit_get_aiming_unit_index(player->unit_handle);

  if (!valid_real_normal3d(forward)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "direction", (double)forward[0], (double)forward[1],
                            (double)forward[2]),
                   "c:\\halo\\SOURCE\\game\\aim_assist.c", 0x4d, 1);
    system_exit(-1);
  }

  if (global_current_collision_user_depth >= 0x20) {
    display_assert("global_current_collision_user_depth < "
                   "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                   "c:\\halo\\SOURCE\\game\\aim_assist.c", 0x4f, 1);
    system_exit(-1);
  }
  collision_user_stack[global_current_collision_user_depth++] = 6;

  if (unit_get_aim_assist_parameters(aiming_unit_index, aim_assist_parameters,
                                     unit_get_zoom_level(aiming_unit_index))) {
    director_camera_deterministic(player->unit_handle, (int)&camera_position,
                                  (int)&camera_forward);
    aim_vector = *(real_vector3d *)forward;
    aim_level = 0.0f;

    if (FUN_000a6030(aim_assist_parameters, &camera_position.x,
                     &camera_forward.i, player->unit_handle,
                     (int16_t)player->team_index, &record)) {
      aim_vector.i = record.field_04.x - origin[0];
      aim_vector.j = record.field_04.y - origin[1];
      aim_vector.k = record.field_04.z - origin[2];
      if (normalize3d(&aim_vector.i) == 0.0f) {
        aim_vector = *(real_vector3d *)forward;
      }
      aim_level = record.field_30;
      target_object_index = record.object_index;
    }

    unit = (object_datum_t *)object_get_and_verify_type(aiming_unit_index, 3);
    dx = camera_position.x - unit->position.x;
    dy = camera_position.y - unit->position.y;
    dz = camera_position.z - unit->position.z;
    offset_vector = camera_forward;
    distance = sqrtf(dz * dz + dy * dy + dx * dx);
    normalize3d(&offset_vector.i);
    offset_vector.i *= distance;
    offset_vector.j *= distance;
    offset_vector.k *= distance;
    camera_position.x += offset_vector.i;
    camera_position.y += offset_vector.j;
    camera_position.z += offset_vector.k;

    test_vector.i = camera_forward.i * 128.0f;
    test_vector.j = camera_forward.j * 128.0f;
    test_vector.k = camera_forward.k * 128.0f;
    FUN_0014df70(0x1000e9, &camera_position.x, &test_vector.i,
                 player->unit_handle, (int16_t *)&collision);

    hit_vector.i = collision.point.x - origin[0];
    hit_vector.j = collision.point.y - origin[1];
    hit_vector.k = collision.point.z - origin[2];
    if (normalize3d(&hit_vector.i) == 0.0f) {
      hit_vector = *(real_vector3d *)forward;
    }

    FUN_0010c780(&hit_vector.i, &aim_vector.i, aim_level, &test_vector.i);
    /* EDI (forward) is pushed for both arg 2 (0xa6412) and arg 5 (0xa63ff):
     * the pinned vector is written back in place. */
    pin_normal_to_cone3d(&test_vector.i, forward, /* dup-args-ok */
                         x87_fsin(aim_assist_parameters[4]),
                         x87_fcos(aim_assist_parameters[4]), forward);
  }

  if (global_current_collision_user_depth <= 1) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\game\\aim_assist.c", 0x8c, 1);
    system_exit(-1);
  }
  --global_current_collision_user_depth;

  player->field_40 = target_object_index;
  player->field_44 = game_time_get();
  return target_object_index;
}

/* local_player_aim_assist (0xa6470)
 *
 * Per-tick aim-assist query for a local player.  Clears all four outputs,
 * then (only for director perspective 0 or 1) builds the aiming unit's
 * aim-assist parameters, asks FUN_000a6030 for the best candidate seen from
 * the observer camera and, on success, reports the candidate's two weights,
 * the yaw/pitch of its camera-relative vector (vector_to_angles) and the
 * first-order yaw/pitch change of that vector along the offset between the
 * two objects' root locations.  Returns the candidate's object index, or
 * NONE.  Sole caller: player_control.c (0xb754e), which stores EAX.
 *
 * Confirmed from the disassembly at 0xa6470:
 *   - director_get_perspective is called before the six output stores.
 *   - unit_get_aim_assist_parameters gets the aiming unit in EAX and the
 *     parameter array in EBX (0xa64d7/0xa64db); zoom comes from
 *     player_control_get_zoom_level(local_player_index).
 *   - FUN_000a6030 gets the observer camera (+0x00 point, +0x20 vector), the
 *     aiming unit and the zero-extended word at player +0x20.
 *   - x87 sequence 0xa655a-0xa65b6: d = target root - unit root (x, y, z
 *     order); with v = record.field_10, s = v.x*v.x + v.y*v.y, r = sqrt(s):
 *       rates[0] = (v.x*d.y - v.y*d.x) / s
 *       rates[1] = (d.z*r - (v.y*d.y + v.x*d.x) / r * v.z) / (v.z*v.z + s)
 *   - the success path returns record.object_index ([EBP-0x68]); the failure
 *     path returns -1 (OR EAX,0xffffffff). */
int local_player_aim_assist(short local_player_index, real *autoaim_level,
                            real *field_0x30, void *lead_vector, void *arg5)
{
  int16_t perspective;
  player_data_t *player;
  int aiming_unit_index;
  real *camera;
  real *angles;
  real *rates;
  x87_wide_t delta[3];
  real xy_squared;
  real xy_length;
  real aim_assist_parameters[6];
  aim_assist_record_t record;
  real_point3d unit_origin;
  real_point3d target_origin;

  perspective = director_get_perspective(local_player_index);
  angles = (real *)lead_vector;
  rates = (real *)arg5;
  *autoaim_level = 0.0f;
  *field_0x30 = 0.0f;
  angles[1] = 0.0f;
  angles[0] = 0.0f;
  rates[1] = 0.0f;
  rates[0] = 0.0f;

  if (perspective == 0 || perspective == 1) {
    player = (player_data_t *)datum_get(
      player_data, local_player_get_player_index(local_player_index));
    aiming_unit_index = unit_get_aiming_unit_index(player->unit_handle);
    if (unit_get_aim_assist_parameters(
          aiming_unit_index, aim_assist_parameters,
          player_control_get_zoom_level(local_player_index))) {
      camera = (real *)observer_get_camera(local_player_index);
      if (FUN_000a6030(aim_assist_parameters, camera,
                       (real *)((char *)camera + 0x20), aiming_unit_index,
                       (int16_t)player->team_index, &record)) {
        *autoaim_level = record.field_30;
        *field_0x30 = record.field_34;
        vector_to_angles(angles, &record.field_10.i);
        object_get_root_location(player->unit_handle, &unit_origin.x, 0);
        object_get_root_location(record.object_index, &target_origin.x, 0);

        /* The original keeps the three deltas in ST(i) at full width until
         * the end (0xa655a-0xa6570); x87_wide_t stops clang spilling them
         * as dwords. */
        delta[0] = target_origin.x - unit_origin.x;
        delta[1] = target_origin.y - unit_origin.y;
        delta[2] = target_origin.z - unit_origin.z;
        xy_squared = record.field_10.j * record.field_10.j +
                     record.field_10.i * record.field_10.i;
        xy_length = sqrtf(xy_squared);
        rates[0] =
          (record.field_10.i * delta[1] - record.field_10.j * delta[0]) /
          xy_squared;
        rates[1] = (delta[2] * xy_length - (record.field_10.j * delta[1] +
                                            record.field_10.i * delta[0]) /
                                             xy_length * record.field_10.k) /
                   (record.field_10.k * record.field_10.k + xy_squared);
        return record.object_index;
      }
    }
  }

  return NONE;
}
