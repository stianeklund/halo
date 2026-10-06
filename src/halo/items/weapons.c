#include "x87_math.h"
double pow(double x, double y);

/* 0xfad20 — weapons_initialize */
void weapons_initialize(void)
{
}

/* 0xfad30 — weapons_initialize_for_new_map */
void weapons_initialize_for_new_map(void)
{
}

/* 0xfad40 — weapons_dispose_from_old_map */
void weapons_dispose_from_old_map(void)
{
}

/* 0xfad50 — weapons_dispose */
void weapons_dispose(void)
{
}

/* 0xfad60 — weapon_place
 *
 * Applies a scenario weapon placement record to a freshly created weapon
 * object: clamps the placement's round counts against the first trigger
 * definition's magazine limits, applies the placement flags, and (when the
 * placement is not "initially at rest") nudges object field 0x14 by a fixed
 * float constant.
 *
 * Confirmed: cdecl, 2 stack args — [EBP+0x8] weapon_handle (EBX),
 * [EBP+0xc] placement (EDI). Confirmed: MOV EAX,EBX at 0xfae0e returns the
 * weapon handle; EBX is saved/restored solely to hold it across the calls.
 * Only reference is the object-placement table at 0x323f50 (no C callers).
 * Confirmed: PUSH 0x4 / PUSH EBX → object_get_and_verify_type at 0xfad6c.
 * Confirmed: MOV EAX,[ESI] / PUSH EAX / PUSH 0x77656170 → tag_get at 0xfad7b;
 * ADD ESP,0x10 cleans both cdecl calls at 0xfad8e.
 * Confirmed: MOV ECX,[EAX+0x4f0] / ADD EAX,0x4f0 / TEST ECX,ECX / JLE — the
 * trigger tag_block count gate at 0xfad80–0xfad93.
 * Confirmed: PUSH 0x70 / PUSH 0x0 / PUSH EAX → tag_block_get_element at
 * 0xfad9a (element size 0x70).
 * Confirmed: CMP CX,DX / JLE with MOVSX on both arms at 0xfadaa–0xfadb7 =
 * signed min(placement+0x48, trigger+0x8) stored to word [ESI+0x25e].
 * Confirmed: CMP CX,AX / JG at 0xfadc6–0xfadd1 = signed
 * min(placement+0x4a, trigger+0xa) stored to word [ESI+0x260].
 * Confirmed: TEST byte [EDI+0x4c],1 → OR/AND 0x20 on dword [ESI+0x4], then
 * OR byte [ESI+0x6],0x2 (same dword, byte 2) in that order.
 * Confirmed: TEST byte [EDI+0x4c],4 → JNZ clears 0x20 in [ESI+0x1a4],
 * else sets it.
 * Confirmed: TEST byte [EDI+0x4c],1 / JNZ skip → FLD [ESI+0x14] /
 * FADD [0x2533e8] / FSTP [ESI+0x14].
 */

/* Forward declaration for unported function at 0x21fb0 — valid_real_normal3d */
extern bool valid_real_normal3d(float *vec);

int weapon_place(int weapon_handle, void *placement)
{
  char *weapon;
  char *place;
  char *weap_tag;
  char *trigger;
  short placed;
  short limit;

  place = (char *)placement;
  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  weap_tag = (char *)tag_get(0x77656170, *(int *)weapon);

  if (*(int *)(weap_tag + 0x4f0) > 0) {
    trigger =
      (char *)tag_block_get_element((void *)(weap_tag + 0x4f0), 0, 0x70);

    placed = *(int16_t *)(place + 0x48);
    limit = *(int16_t *)(trigger + 0x8);
    *(int16_t *)(weapon + 0x25e) = (int16_t)(placed > limit ? limit : placed);

    placed = *(int16_t *)(place + 0x4a);
    limit = *(int16_t *)(trigger + 0xa);
    *(int16_t *)(weapon + 0x260) = (int16_t)(placed > limit ? limit : placed);
  }

  if ((*(uint8_t *)(place + 0x4c) & 1) != 0) {
    *(int *)(weapon + 0x4) |= 0x20;
  } else {
    *(int *)(weapon + 0x4) &= ~0x20;
  }
  *(uint8_t *)(weapon + 0x6) |= 2;

  if ((*(uint8_t *)(place + 0x4c) & 4) == 0) {
    *(int *)(weapon + 0x1a4) |= 0x20;
  } else {
    *(int *)(weapon + 0x1a4) &= ~0x20;
  }

  if ((*(uint8_t *)(place + 0x4c) & 1) == 0) {
    *(float *)(weapon + 0x14) =
      *(float *)(weapon + 0x14) + *(const float *)0x2533e8;
  }
  return weapon_handle;
}

/* 0xfae30 — weapon_preprocess_node_orientations
 *
 * Prefetches the weapon's animation graph block element for node orientation
 * processing. Resolves the weapon tag, finds the 'antr' tag, and calls
 * tag_block_get_element on the first animation element if the block is
 * non-empty. The result is discarded; the call likely primes an internal cache.
 *
 * Confirmed: cdecl, 1 stack arg (weapon_handle).
 * Confirmed: CALL object_get_and_verify_type(weapon_handle, 4).
 * Confirmed: CALL tag_get(0x77656170, *obj) → weap_tag.
 * Confirmed: CALL tag_get(0x616e7472, *(weap_tag+0x44)) → antr.
 * Confirmed: CMP *(int *)(antr+0x18), 0; JZ exit.
 * Confirmed: CALL tag_block_get_element(antr+0x18, 0, 0x1c) (result unused).
 */
void weapon_preprocess_node_orientations(int weapon_handle)
{
  int *obj;
  int tag;
  int antr;

  obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  tag = (int)tag_get(0x77656170, *obj);
  antr = (int)tag_get(0x616e7472, *(int *)(tag + 0x44));
  if (*(int *)(antr + 0x18) != 0) {
    tag_block_get_element((void *)(antr + 0x18), 0, 0x1c);
  }
}

/* 0xfae80 — weapon_get_label */
char *weapon_get_label(int weapon_handle)
{
  char *result = (char *)0x25386f;
  int *obj;
  if (weapon_handle != -1) {
    obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    result = (char *)tag_get(0x77656170, *obj) + 0x30c;
  }
  return result;
}

/* 0xfaeb0 — weapon_set_integrated_light_power */
void weapon_set_integrated_light_power(int weapon_handle, int light_power)
{
  char *weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  *(int *)(weapon_obj + 0x1f8) = light_power;
}

/* 0xfaed0 — weapon_estimate_time_to_target
 *
 * Given a weapon object handle, a trigger index, and a float value, returns
 * the result of evaluating the projectile charge function for that trigger.
 *
 * Steps:
 *   1. Verify weapon object and get its 'weap' tag (first dword of object
 *      data is the tag index).
 *   2. Validate trigger_index against the triggers block at tag+0x4fc.
 *   3. Get the trigger element (element size 0x114) at the given index.
 *   4. Read the 'proj' (projectile) tag reference from trigger+0xa0.
 *   5. Call projectile_estimate_time_to_target(proj_tag, param_3) — returns a
 * float ratio.
 *   6. Default return value is 0.0f (loaded from 0x2533c0 which holds 0.0f).
 *
 * Confirmed: PUSH 0x4 / PUSH EAX → object_get_and_verify_type at 0xfaed6.
 * Confirmed: MOV ECX,[EAX] / PUSH ECX / PUSH 0x77656170 → tag_get at 0xfaedf.
 * Confirmed: FLD [0x2533c0] (0.0f default) at 0xfaeeb.
 * Confirmed: MOV CX,[EBP+0xc] (trigger_index) at 0xfaef1.
 * Confirmed: ADD ESP,0x10 cleans 4 cdecl args (both calls) at 0xfaef5.
 * Confirmed: MOV EDX,[EAX+0x4fc] / ADD EAX,0x4fc block count/ptr at 0xfaefd.
 * Confirmed: PUSH 0x114 / PUSH ECX / PUSH EAX → tag_block_get_element at
 * 0xfaf0f. Confirmed: MOV EDX,[EBP+0x10] (param_3) at 0xfaf1d. Confirmed: MOV
 * EAX,[EAX+0xa0] (proj tag ref) at 0xfaf20. Confirmed: PUSH EDX / PUSH EAX /
 * PUSH 0x70726f6a → tag_get('proj') at 0xfaf29. ADD ESP,0x8 cleans 2 args; EDX
 * (param_3) already on stack for projectile_estimate_time_to_target. Confirmed:
 * PUSH EAX / CALL projectile_estimate_time_to_target at 0xfaf38; ADD ESP,0x8
 * cleans 2.
 */
float weapon_estimate_time_to_target(int weapon_handle, int16_t trigger_index,
                                     float param_3)
{
  int *weapon_data = (int *)object_get_and_verify_type(weapon_handle, 4);
  void *weap_tag = tag_get(0x77656170, weapon_data[0]);
  float result = 0.0f;

  if (trigger_index >= 0) {
    int *trig_block = (int *)((char *)weap_tag + 0x4fc);
    int trigger_count = trig_block[0];
    int trigger_idx = (int)trigger_index;
    if (trigger_idx < trigger_count) {
      void *trig_elem = tag_block_get_element(trig_block, trigger_idx, 0x114);
      int proj_ref = *(int *)((char *)trig_elem + 0xa0);
      void *proj_tag = tag_get(0x70726f6a, proj_ref);
      result = projectile_estimate_time_to_target(proj_tag, param_3);
    }
  }

  return result;
}

/* 0xfaf50 — weapon_can_be_fired
 *
 * Returns 0 when the weapon is at/over full heat, or (in a running game
 * engine) when the first trigger's field at +0xa is positive while both
 * ammo-ish counters at obj+0x260 and obj+0x25e are zero. Otherwise 1.
 *
 * Confirmed: cdecl, 1 stack arg [EBP+0x8].
 * Confirmed: PUSH 0x4 / PUSH EAX → object_get_and_verify_type at 0xfaf5b.
 * Confirmed: MOV ECX,[ESI] / PUSH ECX / PUSH 0x77656170 → tag_get at 0xfaf6a;
 * ADD ESP,0x10 at 0xfaf7d cleans both cdecl calls (2 + 2 args), so tag_get
 * takes 2 args.
 * Confirmed: tag_get is called before the heat gate, even though its result
 * is only consumed on the second path.
 * Confirmed: FLD [ESI+0x1f0] / FCOMP [0x2533c8] / FNSTSW / TEST AH,0x1 / JNZ
 * at 0xfaf6f–0xfaf85 — C0 set (heat < 1.0f) continues, otherwise XOR AL,AL.
 * Confirmed: CALL 0x000a8e30 (game_engine_running) / TEST AL,AL at 0xfaf8d.
 * Confirmed: MOV ECX,[EDI+0x4f0] / TEST ECX,ECX / JLE — signed count > 0.
 * Confirmed: LEA EAX,[EDI+0x4f0] / PUSH 0x70 / PUSH 0x0 / PUSH EAX →
 * tag_block_get_element(block, 0, 0x70) at 0xfafab; ADD ESP,0xc.
 * Confirmed: CMP word ptr [EAX+0xa],0x0 / JLE — signed int16 > 0.
 * Confirmed: CMP word ptr [ESI+0x260],0x0 / JNZ then CMP word ptr
 * [ESI+0x25e],0x0 / JZ — offset 0x260 tested first.
 */
char weapon_can_be_fired(int weapon_handle)
{
  int *obj;
  void *weap_tag;
  void *trigger;

  obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  weap_tag = tag_get(0x77656170, obj[0]);

  if (*(float *)((char *)obj + 0x1f0) >= 1.0f) {
    return 0;
  }

  if (game_engine_running() && *(int *)((char *)weap_tag + 0x4f0) > 0) {
    trigger = tag_block_get_element((char *)weap_tag + 0x4f0, 0, 0x70);
    if (*(int16_t *)((char *)trigger + 0xa) > 0 &&
        *(int16_t *)((char *)obj + 0x260) == 0 &&
        *(int16_t *)((char *)obj + 0x25e) == 0) {
      return 0;
    }
  }

  return 1;
  if (weap_tag) {
  }
}

/* 0xfafe0 — weapon_useful
 *
 * Returns 1 while the weapon object's float at +0x1f0 is below 1.0f.
 *
 * Confirmed: cdecl, 1 stack arg ([EBP+8]); no callers found in binary.
 * Confirmed: PUSH 0x4 / PUSH EAX → object_get_and_verify_type at 0xfafe9.
 * Confirmed: FLD [EAX+0x1f0]; FCOMP [0x2533c8] (1.0f); TEST AH,1; JNZ → AL=1.
 * Confirmed: returns in AL only (XOR AL,AL / MOV AL,1).
 */
bool weapon_useful(int weapon_index)
{
  void *obj = object_get_and_verify_type(weapon_index, 4);
  if (*(float *)((char *)obj + 0x1f0) >= 1.0f) {
    return 0;
  }
  return 1;
}

/* 0xfb010 — weapon_compute_movement_penalty
 *
 * Selects one of two tag floats (tag+0x400 when param_2 is nonzero, else
 * tag+0x404), then, depending on the int16 selector at tag+0x3fc, replaces it
 * with 0.0f when param_3 is zero. Parameter meanings are unproven.
 *
 * Confirmed: cdecl, 3 stack args; [EBP+0xc] and [EBP+0x10] read as bytes.
 * Confirmed: PUSH 0x4 / PUSH EAX → object_get_and_verify_type at 0xfb01a.
 * Confirmed: MOV ECX,[ESI] / PUSH ECX / PUSH 0x77656170 → tag_get at 0xfb029;
 * ADD ESP,0x10 at 0xfb031 cleans both calls (2 + 2 args).
 * Confirmed: MOVSX [tag+0x3fc]; case 1 → zero check; case 2 → zero check only
 * when int16 obj+0x258 == 1 or obj+0x264 == 1 (0x258 tested first).
 * Confirmed: FSTP ST0 / FLD [0x2533c0] (0.0f) when param_3 == 0.
 */
float weapon_compute_movement_penalty(int weapon_handle, char param_2,
                                      char param_3)
{
  int *obj;
  void *weap_tag;
  float result;

  obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  weap_tag = tag_get(0x77656170, obj[0]);

  if (param_2) {
    result = *(float *)((char *)weap_tag + 0x400);
  } else {
    result = *(float *)((char *)weap_tag + 0x404);
  }

  switch (*(int16_t *)((char *)weap_tag + 0x3fc)) {
  case 1:
    break;
  case 2:
    if (*(int16_t *)((char *)obj + 0x258) == 1 ||
        *(int16_t *)((char *)obj + 0x264) == 1) {
      break;
    }
    return result;
  default:
    return result;
  }

  if (!param_3) {
    result = *(float *)0x2533c0;
  }
  return result;
}

/* 0xfb080 — weapon_melee_attack */
void weapon_melee_attack(void)
{
}

/* 0xfb090 — weapon_must_be_readied
 *
 * Returns non-zero if the weapon's 'must be readied' flag is set
 * (bit 3 of the weapon definition flags at tag+0x308).
 *
 * Confirmed: cdecl, 1 stack arg (weapon_handle).
 * Confirmed: CALL object_get_and_verify_type(weapon_handle, 4).
 * Confirmed: CALL tag_get(0x77656170, *obj).
 * Confirmed: SHR EAX,3; AND EAX,1 on *(uint *)(tag+0x308).
 */
int weapon_must_be_readied(int weapon_handle)
{
  int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  int tag = (int)tag_get(0x77656170, *obj);
  return (*(uint32_t *)(tag + 0x308) >> 3) & 1;
}

/* 0xfb0c0 — weapon_is_flag */
bool weapon_is_flag(int object_index)
{
  int *obj = (int *)object_get_and_verify_type(object_index, 4);
  uint32_t *tag = (uint32_t *)tag_get(0x77656170, *obj);
  return (tag[0x308 / 4] >> 3) & 1;
}

/* 0xfb0f0 — weapon_prevents_grenade_throwing
 *
 * Returns 1 if weapon_handle == -1 (no weapon), 1 if the weapon's animation
 * state at +0x1e8 is in range (4, 11), or the flag bit 6 of tag+0x308 is set.
 *
 * Confirmed: cdecl, 1 stack arg (param_1 = weapon_handle).
 * Confirmed: returns 1 immediately if param_1 == -1.
 * Confirmed: CALL object_get_and_verify_type(param_1, 4).
 * Confirmed: CALL tag_get(0x77656170, *obj).
 * Confirmed: SHR EAX,6; AND EAX,1 on *(uint *)(tag+0x308).
 * Confirmed: range check on *(char *)(obj+0x1e8): returns 1 if in (4,11).
 */
int weapon_prevents_grenade_throwing(int weapon_handle)
{
  int *obj;
  int tag;
  char result;

  result = 1;
  if (weapon_handle != -1) {
    obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    tag = (int)tag_get(0x77656170, *obj);
    result = (char)((*(uint32_t *)(tag + 0x308) >> 6) & 1);
    if (*(char *)((char *)obj + 0x1e8) >= 5 &&
        *(char *)((char *)obj + 0x1e8) <= 10) {
      result = 1;
    }
  }
  return result;
}

/* 0xfb140 — weapon_get_first_person_animation_time
 *
 * Looks up a weapon's animation graph and returns a frame count for
 * the requested animation slot. param_2 selects the field (0 -> offset
 * 0x22, 1 -> offset 0x34), param_3 is the animation index used to look
 * up into the first tag block element, param_4 selects a variant when
 * the weapon type at tag+0x4e2 is 1 (dual-wield).
 *
 * Confirmed: cdecl, 4 stack args.
 * Confirmed: CALL object_get_and_verify_type(weapon_handle, 4) at 0xfb14c.
 * Confirmed: CALL tag_get(0x77656170, *obj) at 0xfb159.
 * Confirmed: tag+0x478 is antr tag_index, tag+0x4e2 is weapon type.
 * Confirmed: tag_block at antr+0x48 element size 0x1c.
 * Confirmed: tag_block at antr+0x74 element size 0xb4.
 * Confirmed: switch on param_2: case 0 reads +0x22, case 1 reads +0x34.
 * Confirmed: assert at weapons.c line 0x634 for invalid param_2.
 * Confirmed: dual-wield branch reads indices 0x17(+0x2e), 0x18(+0x30),
 *   0x19(+0x32) from first element's index array.
 */
int16_t weapon_get_first_person_animation_time(int weapon_handle,
                                               int16_t param_2, int16_t param_3,
                                               int16_t param_4)
{
  uint32_t *weapon_data;
  int weap_tag;
  int16_t result;
  int antr;
  int elem0;
  int16_t anim_index;
  void *antr_block;
  int anim_elem;
  weapon_data = (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  weap_tag = (int)tag_get(0x77656170, weapon_data[0]);
  result = 0;

  if (*(int *)(weap_tag + 0x478) == -1)
    return result;

  antr = (int)tag_get(0x616e7472, *(int *)(weap_tag + 0x478));

  if (*(int *)(antr + 0x48) == 0)
    return result;

  elem0 = (int)tag_block_get_element((void *)(antr + 0x48), 0, 0x1c);
  if (elem0 == 0)
    return result;

  if (param_3 < 0 || (int)param_3 >= *(int *)(elem0 + 0x10))
    return result;

  anim_index = *(int16_t *)(*(int *)(elem0 + 0x14) + param_3 * 2);
  if (anim_index == -1)
    return result;

  antr_block = (void *)(antr + 0x74);
  anim_elem = (int)tag_block_get_element(antr_block, (int)anim_index, 0xb4);

  switch (param_2) {
  case 0:
    result = *(int16_t *)(anim_elem + 0x22);
    break;
  case 1:
    result = *(int16_t *)(anim_elem + 0x34);
    break;
  default:
    display_assert(0, "c:\\halo\\SOURCE\\items\\weapons.c", 0x634, 1);
    system_exit(-1);
    break;
  }

  /* Dual-wield variant override: weapon type == 1 and param_2 == 0 */
  if (param_2 == 0 && *(int16_t *)(weap_tag + 0x4e2) == 1) {
    int idx_a;
    int elem_a;
    int idx_b;
    int idx_c;
    if (*(int *)(elem0 + 0x10) <= 0x17) {
      idx_a = -1;
    } else {
      idx_a = (int)*(int16_t *)(*(int *)(elem0 + 0x14) + 0x2e);
    }
    elem_a = (int)tag_block_get_element(antr_block, idx_a, 0xb4);

    if (*(int *)(elem0 + 0x10) <= 0x18) {
      idx_b = -1;
    } else {
      idx_b = (int)*(int16_t *)(*(int *)(elem0 + 0x14) + 0x30);
    }
    tag_block_get_element(antr_block, idx_b, 0xb4);

    if (*(int *)(elem0 + 0x10) <= 0x19) {
      idx_c = -1;
    } else {
      idx_c = (int)*(int16_t *)(*(int *)(elem0 + 0x14) + 0x32);
    }
    tag_block_get_element(antr_block, idx_c, 0xb4);

    switch (param_4) {
    case 0:
      result = *(int16_t *)(elem_a + 0x22);
      break;
    case 2:
      result = *(int16_t *)(elem_a + 0x22);
      return result;
    }
  }

  return result;
}

/* 0xfb2f0 — weapon_overcharged
 *
 * Returns 1 if the weapon's trigger state at +0x211 is 2 (overcharge) or
 * 3 (overcharge-releasing), 0 otherwise.
 *
 * Confirmed: cdecl, 1 stack arg (weapon_handle).
 * Confirmed: CALL object_get_and_verify_type(weapon_handle, 4).
 * Confirmed: CMP BYTE PTR [EAX+0x211], 2 / JE; CMP ..., 3 / JE.
 * Confirmed: returns 1 if state == 2 or 3, else 0.
 */
int weapon_overcharged(int weapon_handle)
{
  int obj = (int)object_get_and_verify_type(weapon_handle, 4);
  if (*(char *)(obj + 0x211) != 2 && *(char *)(obj + 0x211) != 3)
    return 0;
  return 1;
}

__declspec(noinline) void *weapon_trigger_get(void *weapon_obj,
                                            int16_t trigger_index)
{
  int *tag_data = (int *)tag_get(0x77656170, *(int *)weapon_obj);

  assert_halt_msg_at("trigger_index>=0 && trigger_index<weapon_definition->weapon.triggers.count", "c:\\halo\\SOURCE\\items\\weapons.c",
      0x667,
      trigger_index >= 0 &&
                    trigger_index < *(int *)((char *)tag_data + 0x4fc));

  return (void *)((char *)weapon_obj + 0x210 + trigger_index * 36);
}

void *weapon_magazine_get(void *weapon_obj, int16_t magazine_index)
{
  int *tag_data = (int *)tag_get(0x77656170, *(int *)weapon_obj);

  assert_halt_msg_at("magazine_index>=0 && magazine_index<weapon_definition->weapon.magazines.count", "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, magazine_index >= 0 &&
              magazine_index < *(int *)((char *)tag_data + 0x4f0));

  return (void *)((char *)weapon_obj + (magazine_index + 50) * 12);
}

/* 0xfb3c0 — weapon_busy
 *
 * Returns true if the weapon has any active triggers, magazines, or
 * pending activity. Checks five fields in the weapon data.
 *
 * Confirmed: regparm, weapon_handle in EAX.
 * Confirmed: PUSH 4 / PUSH EAX / CALL object_get_and_verify_type.
 * Confirmed: checks offsets 0x211, 0x235, 0x258, 0x264, 0x1e8.
 * Confirmed: returns 0 (false) only if ALL five are zero/null.
 */
bool weapon_busy(int weapon_handle)
{
  char *weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);

  if (*(char *)(weapon_data + 0x211) != 0 ||
      *(char *)(weapon_data + 0x235) != 0 ||
      *(int16_t *)(weapon_data + 0x258) != 0 ||
      *(int16_t *)(weapon_data + 0x264) != 0 ||
      *(char *)(weapon_data + 0x1e8) != 0) {
    return true;
  }

  return false;
}

/* 0xfb410 — weapon_magazine_state_change_ok */
/* Confirmed: weapon_handle in EAX. Returns 1 only when the bytes at
 *   +0x211, +0x235 (trigger 0/1 state bytes; trigger stride 36 from +0x210)
 *   and +0x1e8 (weapon state byte) are all zero. */
boolean weapon_magazine_state_change_ok(int weapon_handle)
{
  char *weapon;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  if (*(char *)(weapon + 0x211) != 0 || *(char *)(weapon + 0x235) != 0 ||
      *(char *)(weapon + 0x1e8) != 0) {
    return false;
  }
  return true;
}

/* 0xfb450 — weapon_get_effect_object_index
 * Confirmed: weapon_handle in ESI. Returns the owner handle (+0xcc) when
 *   bit 0 of the byte at +0x4 is set and owner != -1, else weapon_handle. */
int weapon_get_effect_object_index(int weapon_handle)
{
  char *weapon;
  int owner_handle;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  if (*(unsigned char *)(weapon + 4) & 1) {
    owner_handle = *(int *)(weapon + 0xcc);
    if (owner_handle != -1) {
      return owner_handle;
    }
  }
  return weapon_handle;
}

/* 0xfb480 — weapon_get_owner_object_index
 * Confirmed: weapon_handle in EAX. Returns +0xcc when it is not -1 and
 *   object_try_and_get_and_verify_type(owner, 3) succeeds, else -1. */
int weapon_get_owner_object_index(int weapon_handle)
{
  char *weapon;
  int owner_handle;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  owner_handle = *(int *)(weapon + 0xcc);
  if (owner_handle != -1 &&
      object_try_and_get_and_verify_type(owner_handle, 3) != 0) {
    return *(int *)(weapon + 0xcc);
  }
  return -1;
}

/* 0xfb4c0 — weapon_get_projectile_owner_object_index
 * Confirmed: weapon_handle in EAX. -1 if owner (+0xcc) is -1 or
 *   object_try_and_get_and_verify_type(owner, 3) fails; otherwise the
 *   owner object's dword at +0x2d8, or the owner handle when that is -1.
 * Unknown: meaning of owner+0x2d8. */
int weapon_get_projectile_owner_object_index(int weapon_handle)
{
  char *weapon;
  int owner_handle;
  char *owner;
  int result;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  owner_handle = *(int *)(weapon + 0xcc);
  if (owner_handle == -1) {
    return -1;
  }
  owner = (char *)object_try_and_get_and_verify_type(owner_handle, 3);
  if (owner == 0) {
    return -1;
  }
  result = *(int *)(owner + 0x2d8);
  if (result == -1) {
    return *(int *)(weapon + 0xcc);
  }
  return result;
}

/* 0xfb510 — weapon_trigger_get_charged_fraction
 *
 * Returns a float in ST(0) describing the trigger's charge state.
 *
 * Name evidence (T2, behaviour + state machine): returns
 *   1.0f - elapsed/trigger_definition[+0x48] in trigger state 2, exactly
 *   1.0f in state 3 and 0.0f otherwise. weapon_update (0xfe910) counts
 *   state 2 down and enters state 3 when it reaches zero, so state 2 is
 *   "charging" and state 3 is "fully charged" — the return value is the
 *   charged fraction. Sole caller is weapon_export_function_values
 *   (0xfbf00), the tag function-value exporter.
 *
 * Confirmed: register args — weapon_handle in EAX (PUSH 0x4 / PUSH EAX /
 *   CALL object_get_and_verify_type), trigger_index in CX
 *   (MOV ESI,ECX at 0xfb51a; later MOVSX EDX,SI).
 * Confirmed: call order is object_get_and_verify_type(handle, 4) ->
 *   weapon_trigger_get(EDI=object, SI=trigger_index) -> tag_get(0x77656170,
 *   *(int *)object) -> tag_block_get_element(tag_data+0x4fc,
 *   (int)(int16_t)trigger_index, 0x114). All four calls run
 *   unconditionally before the state test (ADD ESP,0x1c at 0xfb54f).
 * Confirmed: MOVSX ECX,byte ptr [EBX+0x1] / SUB ECX,2 / JZ (state 2)
 *   / DEC ECX / JZ (state 3); EBX is the weapon_trigger_get trigger pointer,
 *   so the byte is the trigger state at weapon_object+0x211.
 * Confirmed state 2: MOVSX ECX,word ptr [EBX+0x2] / FILD dword /
 *   FMUL [0x2546a4] / FDIV [EAX+0x48] / FSUBR [0x2533c8]
 *   => *(float *)0x2533c8 - (ticks * *(float *)0x2546a4) /
 *      *(float *)(trigger_definition + 0x48).
 *   EAX at the FDIV is the tag_block_get_element result.
 * Confirmed state 3: FLD [0x2533c8]. Default: FLD [0x2533c0].
 * Unknown: the 0x114-byte trigger definition layout (+0x48) and the
 *   weapon-object trigger layout (+0x1/+0x2) are not modelled.
 */
float weapon_trigger_get_charged_fraction(int weapon_handle,
                                          int16_t trigger_index)
{
  int *weapon_obj;
  char *trigger;
  char *trigger_defn;
  float charge;
  int weapon_defn;

  weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon_obj, trigger_index);
  weapon_defn = (int)tag_get(0x77656170, *weapon_obj);
  trigger_defn = (char *)tag_block_get_element(
    (void *)((char *)weapon_defn + 0x4fc), trigger_index, 0x114);

  switch (trigger[1]) {
  case 2:
    charge = (float)*(int16_t *)(trigger + 2) * *(float *)0x2546a4;
    return *(float *)0x2533c8 - charge / *(float *)(trigger_defn + 0x48);
  case 3:
    return *(float *)0x2533c8;
  }
  return *(float *)0x2533c0;
}

/* 0xfb5a0 — weapon_trigger_can_fire_again
 *
 * Name evidence: 2276 symbol dump (T1). Sole caller weapon_update (0xfe910).
 *
 * Confirmed: register args — weapon_handle in EAX (PUSH 0x4 / PUSH EAX /
 *   CALL object_get_and_verify_type), trigger_index in CX (MOV ESI,ECX at
 *   0xfb5aa; MOVSX EDX,SI for tag_block_get_element). Returns bool in AL.
 * Confirmed: call order object_get_and_verify_type -> weapon_trigger_get
 *   (EDI=object, SI=trigger_index) -> tag_get(0x77656170, *object) ->
 *   tag_block_get_element(tag_data+0x4fc, trigger_index, 0x114).
 * Confirmed x87: rate source is object+0x1e4 when definition flags bit 0x200
 *   is set, else trigger+0x10; rate = defn[+4] + rate*(defn[+8]-defn[+4]).
 *   period = [0x253394]/rate when rate > [0x253f44], else 0.0f.
 *   If defn[+0x444] > 0.0f, period *= object[+0x1f0]*defn[+0x444] + 1.0f.
 *   Result = ((float)(signed char)trigger[0] + 1.0f >= period) (FCOMP, TEST
 *   AH,1 — unordered yields false).
 * Confirmed: flags bit 0x8 && (object[+0x1a4] & 2) && !(trigger[4] & 1)
 *   forces false; tested after the period comparison.
 * Unknown: meanings of object+0x1e4/+0x1f0/+0x1a4, trigger+0/+4/+0x10 and
 *   definition +4/+8/+0x444; the value at 0x253f44.
 */
bool weapon_trigger_can_fire_again(int weapon_handle, int16_t trigger_index)
{
  char *weapon_obj;
  char *trigger;
  char *weapon_defn;
  char *trigger_defn;
  unsigned int flags;
  float rate;
  float period;
  bool can_fire;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon_obj, trigger_index);
  weapon_defn = (char *)tag_get(0x77656170, *(int *)weapon_obj);
  trigger_defn = (char *)tag_block_get_element((void *)(weapon_defn + 0x4fc),
                                               trigger_index, 0x114);
  flags = *(unsigned int *)trigger_defn;
  can_fire = false;

  if (flags & 0x200)
    rate = *(float *)(weapon_obj + 0x1e4);
  else
    rate = *(float *)(trigger + 0x10);
  rate = rate * (*(float *)(trigger_defn + 8) - *(float *)(trigger_defn + 4)) +
         *(float *)(trigger_defn + 4);

  if (rate > *(float *)0x253f44)
    period = *(float *)0x253394 / rate;
  else
    period = *(float *)0x2533c0;

  if (*(float *)(weapon_defn + 0x444) > *(float *)0x2533c0)
    period *= *(float *)(weapon_obj + 0x1f0) * *(float *)(weapon_defn + 0x444) +
              *(float *)0x2533c8;

  if ((float)*(signed char *)trigger + *(float *)0x2533c8 >= period)
    can_fire = true;

  if ((flags & 8) && (*(unsigned char *)(weapon_obj + 0x1a4) & 2) &&
      !(*(unsigned char *)(trigger + 4) & 1))
    return false;

  return can_fire;
}

/* 0xfb690 — weapon_magazine_idle
 * Confirmed: weapon_handle in EAX, magazine_index in CX (MOV ESI,ECX). */
void weapon_magazine_idle(int weapon_handle, int16_t magazine_index)
{
  char *weapon;
  int16_t *mag_state;
  char *tag;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  mag_state = (int16_t *)weapon_magazine_get(weapon, magazine_index);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  tag_block_get_element((char *)tag + 0x4f0, (int)magazine_index, 0x70);
  mag_state[0] = 0;
  mag_state[1] = 0;
}

/* 0xfb6e0 — weapon_effect_new
 *
 * Starts an effect or sound associated with a weapon trigger. Resolves
 * the parent object, determines the tag group of the trigger effect
 * (0x65666665='effe' or 0x736e6421='snd!'), and dispatches accordingly.
 *
 * Confirmed: regparm, weapon_handle in EAX, 3 stack args.
 * Confirmed: MOV ESI, EAX at 0xfb6e8 saves weapon_handle.
 * Confirmed: if trigger_effect == -1, returns -1 immediately.
 * Confirmed: object byte+4 bit 0 checked; if set and +0xcc != -1,
 *   parent_handle = object+0xcc.
 * Confirmed: second object_get_and_verify_type call to get unit handle.
 * Confirmed: tag_get_group_tag returns tag group; dispatches on effe/snd!.
 * Confirmed: assert at weapons.c line 0x9d2 for unknown tag group.
 * Confirmed: snd! branch reads globals [0x31fc1c] and [0x31fc3c].
 * Confirmed: effe branch calls effect_new_from_object with 8 args.
 */
int weapon_effect_new(int trigger_effect, float scale, float param_3,
                      int weapon_handle)
{
  char *weapon_data;
  int parent_handle;
  char *weapon_data2;
  int object_handle;
  int tag_group;

  if (trigger_effect != -1) {
    weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
    parent_handle = weapon_handle;
    if ((*(uint8_t *)(weapon_data + 4) & 1) != 0 &&
        *(int *)(weapon_data + 0xcc) != -1) {
      parent_handle = *(int *)(weapon_data + 0xcc);
    }

    weapon_data2 = (char *)object_get_and_verify_type(weapon_handle, 4);
    object_handle = -1;
    if (*(int *)(weapon_data2 + 0xcc) != -1) {
      if (object_try_and_get_and_verify_type(*(int *)(weapon_data2 + 0xcc),
                                             3) != 0) {
        object_handle = *(int *)(weapon_data2 + 0xcc);
      }
    }

    tag_group = tag_get_group_tag(trigger_effect);
    switch (tag_group) {
    case 0x65666665:
      return (int)effect_new_from_object(
        trigger_effect, object_handle, parent_handle, -1, scale, param_3, 0, 0);
    case 0x736e6421: {
      float *position = *(float **)0x31fc1c;
      float *forward = *(float **)0x31fc3c;
      object_impulse_sound_new(object_handle, trigger_effect, -1, position,
                               forward, scale);
      return -1;
    }
    default:
      display_assert(0, "c:\\halo\\SOURCE\\items\\weapons.c", 0x9d2, 1);
      system_exit(-1);
      break;
    }
  }

  return -1;
}

/* 0xfb7d0 — weapon_effect_looping_new
 *
 * Sibling of weapon_effect_new (0xfb6e0): same parent-resolution
 * preamble, but dispatches to effect_new_looping (0x9eb40) with three
 * -1 shorts.
 *
 * Name evidence (T2, callee + caller use): the tail call is
 *   effect_new_looping (0x9eb40, named in kb.json) and weapon_update
 *   (0xfe910) stores this function's EAX return into weapon object
 *   +0x274, later passing it to effect_stop (0x9ceb0). So the return is
 *   a looping-effect handle that is created here and stopped later —
 *   the looping counterpart of weapon_effect_new.
 *
 * Confirmed: no prologue; EBX and ESI are live on entry (only EDI is
 *   saved/restored via PUSH EDI / POP EDI at 0xfb7d6 / 0xfb831).
 * Confirmed: OR EAX,0xffffffff at 0xfb7d0 and 0xfb833 => int return of -1
 *   on both early-exit paths; the taken path returns FUN_0009eb40's EAX.
 * Confirmed: CMP EBX,-0x1 / JZ 0xfb836 guards the whole body.
 * Confirmed: PUSH 0x4 / PUSH ESI / CALL 0x13d680 (twice) =>
 *   object_get_and_verify_type(esi, 4); ESI is the weapon handle.
 * Confirmed: MOV EDI,ESI then TEST CL,0x1 on byte [EAX+4]; if set and
 *   [EAX+0xcc] != -1, EDI = [EAX+0xcc] (parent handle).
 * Confirmed: second lookup's [EAX+0xcc], if != -1, is passed as
 *   object_try_and_get_and_verify_type(handle, 3) with the result
 *   discarded (no test of EAX after ADD ESP,0x8 at 0xfb819).
 * Confirmed: PUSH -1 / -1 / -1 / EDI / EBX / CALL 0x9eb40 => args
 *   (ebx, parent_handle, -1, -1, -1); ADD ESP,0x14.
 * Unknown: meaning of the EBX argument and of FUN_0009eb40; the
 *   object fields +0x4 (flags byte) and +0xcc (parent handle) follow the
 *   0xfb6e0 sibling's usage.
 */
int weapon_effect_looping_new(int param_1, int weapon_handle)
{
  char *weapon_data;
  int parent_handle;
  char *weapon_data2;

  if (param_1 != -1) {
    weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
    parent_handle = weapon_handle;
    if ((*(uint8_t *)(weapon_data + 4) & 1) != 0 &&
        *(int *)(weapon_data + 0xcc) != -1) {
      parent_handle = *(int *)(weapon_data + 0xcc);
    }

    weapon_data2 = (char *)object_get_and_verify_type(weapon_handle, 4);
    if (*(int *)(weapon_data2 + 0xcc) != -1) {
      object_try_and_get_and_verify_type(*(int *)(weapon_data2 + 0xcc), 3);
    }

    if (parent_handle != -1) {
      return effect_new_looping(param_1, parent_handle, -1, -1, -1);
    }
  }

  return -1;
}

/* 0xfb840 — weapon_detonate
 * Confirmed: weapon_handle in ESI. */
void weapon_detonate(int weapon_handle)
{
  char *weapon;
  char *tag;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  weapon_effect_new(*(int *)(tag + 0x390), 0.0f, 0.0f, weapon_handle);
  object_delete(weapon_handle);
}

/* 0xfb880 — weapon_trigger_change_state
 *
 * Sets a weapon trigger's state byte and its accompanying 16-bit charge
 * value. Resolves the weapon object, bounds-checks the trigger index and
 * the new state, then writes both fields of the trigger record.
 *
 * Confirmed: register args — weapon_handle in EAX (PUSH 4 / PUSH EAX /
 *   CALL object_get_and_verify_type / ADD ESP,8 at 0xfb887), trigger_index
 *   in SI (TEST SI,SI / CMP SI,0x2), new_state in BX (TEST BX,BX /
 *   CMP BX,0x9). One 16-bit stack arg at [EBP+8] (MOV DX,[EBP+8]).
 * Confirmed: object lookup happens BEFORE both asserts (result kept in EDI).
 * Confirmed: assert paths PUSH 1 / PUSH 0xa11 (resp. 0xa12) / PUSH filepath
 *   / PUSH reason / CALL display_assert then PUSH -1 / CALL system_exit.
 * Confirmed: address form MOVSX EAX,SI / LEA ECX,[EAX+EAX*8] /
 *   LEA EAX,[EDI+ECX*4] => weapon_data + trigger_index * 36.
 * Confirmed: store order — byte BL to +0x211 first, then word DX to +0x212.
 * Confirmed: trigger stride 36 and base +0x210 agree with weapon_trigger_get;
 *   +0x211 is the state byte read as trigger[1] by
 * weapon_trigger_get_charged_fraction and +0x212 the int16 read as *(int16_t
 * *)(trigger + 2). Unknown: the meaning of the stack-passed 16-bit value beyond
 * its use as the trigger's charge/tick field; raw offsets retained to match the
 *   sibling accessors.
 */
void weapon_trigger_change_state(int16_t charge_ticks, int weapon_handle,
                                 int16_t trigger_index, int16_t new_state)
{
  char *weapon_data;

  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);

  assert_halt_msg_at("trigger_index>=0 && "
                     "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11,
                     trigger_index >= 0 &&
                       trigger_index < MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON);
  assert_halt_msg_at("new_state>=0 && new_state<NUMBER_OF_TRIGGER_STATES",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0xa12,
                     new_state >= 0 && new_state < NUMBER_OF_TRIGGER_STATES);

  *(char *)(weapon_data + trigger_index * 36 + 0x211) = (char)new_state;
  *(int16_t *)(weapon_data + trigger_index * 36 + 0x212) = charge_ticks;
}

/* 0xfb910 — weapon_trigger_start_ejection_port
 *
 * Name evidence (T2, caller + decay pairing): the only caller is
 *   weapon_state_key_frame (0xfb990), which dispatches here on weapon
 *   animation states 3 and 4 with trigger_index 0 and 1 respectively
 *   (0xfb9bb-0xfb9d3). States 3/4 are the "chamber-1"/"chamber-2"
 *   animations (weapon_magazine_start_chamber sets magazine_index+3,
 *   weapon_magazine_start_reload sets magazine_index+5 for
 *   "reload-1"/"reload-2"; the name strings live at 0x290c08-0x290c54).
 *   Chambering is the shell-ejection key frame. The value stored here,
 *   trigger record +0x14, is decayed every tick by weapon_update using
 *   trigger_definition[+0xf4], i.e. it is a recovery ramp started at
 *   1.0f — an ejection-port recovery, not a charge latch.
 *
 * Resolves the weapon object and its trigger definition, then — when the
 * definition's +0xa4 float exceeds *(float *)0x2533c0 and the definition's
 * 0x80 flag agrees with the boolean stack argument — stores 1.0f into the
 * trigger record at +0x14.
 *
 * Confirmed: register args — weapon_handle in EAX (PUSH 0x4 / PUSH EAX /
 *   CALL object_get_and_verify_type at 0xfb91b), trigger_index in CX
 *   (MOV ESI,ECX at 0xfb919, later MOVSX EDX,SI at 0xfb936). One byte
 *   stack arg at [EBP+8] (MOV AL,byte ptr [EBP+8]).
 * Confirmed: call order object_get_and_verify_type(handle, 4) ->
 *   tag_get(0x77656170, *(int *)object) -> weapon_trigger_get(EDI=object,
 *   SI=trigger_index) -> tag_block_get_element(tag_data+0x4fc,
 *   (int)(int16_t)trigger_index, 0x114). Note this differs from
 *   weapon_trigger_get_charged_fraction, which calls weapon_trigger_get before
 * tag_get. All four run unconditionally; one ADD ESP,0x1c at 0xfb94f cleans the
 * 7 pushed dwords of all three cdecl calls. Confirmed: FLD [ECX+0xa4] / FCOMP
 * [0x2533c0] / FNSTSW AX / TEST AH,0x41 / JNZ end => continue only when
 * definition+0xa4 >
 *   *(float *)0x2533c0 (C3|C0 set means <= or unordered).
 * Confirmed: MOV ECX,[ECX] / AND ECX,0x80 then the two-arm test at
 *   0xfb96d..0xfb97f stores only when (flag != 0 && arg != 0) or
 *   (flag == 0 && arg == 0).
 * Confirmed: store target is EDI, the weapon_trigger_get return (trigger
 *   record), offset +0x14, immediate 0x3f800000 = 1.0f.
 * Unknown: the 0x114-byte trigger definition layout (+0x0 flags, +0xa4
 *   float) and the trigger record field at +0x14 are not modelled;
 *   the meaning of the byte stack argument beyond its boolean use.
 */
void weapon_trigger_start_ejection_port(char param_1, int weapon_handle,
                                        int16_t trigger_index)
{
  int *weapon_obj;
  int weapon_defn;
  char *trigger;
  char *trigger_defn;

  weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  weapon_defn = (int)tag_get(0x77656170, *weapon_obj);
  trigger = (char *)weapon_trigger_get(weapon_obj, trigger_index);
  trigger_defn = (char *)tag_block_get_element(
    (void *)((char *)weapon_defn + 0x4fc), trigger_index, 0x114);

  if (*(float *)(trigger_defn + 0xa4) > *(float *)0x2533c0) {
    if (((*(uint32_t *)trigger_defn & 0x80) != 0 && param_1 != 0) ||
        ((*(uint32_t *)trigger_defn & 0x80) == 0 && param_1 == 0)) {
      *(float *)(trigger + 0x14) = 1.0f;
    }
  }
}

/* 0xfb990 — weapon_state_key_frame
 * Confirmed: weapon_handle in EDI; byte +0x1e8 == 3/4 -> trigger 0/1. */
void weapon_state_key_frame(int weapon_handle)
{
  char *weapon;
  int8_t state;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  tag_get(0x77656170, *(uint32_t *)weapon);
  state = *(int8_t *)(weapon + 0x1e8);
  if (state == 3) {
    weapon_trigger_start_ejection_port(1, weapon_handle, 0);
  } else if (state == 4) {
    weapon_trigger_start_ejection_port(1, weapon_handle, 1);
  }
}

/* 0xfb9e0 — weapon_magazine_state_interruptable
 * Confirmed: state in CX (MOVSX ECX,CX); returns AL. */
boolean weapon_magazine_state_interruptable(int16_t state)
{
  if (state == 0 || state == 2) {
    return true;
  }
  return false;
}

/* 0xfba00 — weapon_state_interruptable
 *
 * Leaf predicate (no calls). Decides whether a weapon animation state may
 * be interrupted by comparing it against a second state value.
 *
 * Confirmed: register arg — state arrives in DX (MOVSX ECX,DX at 0xfba03);
 *   one 16-bit stack arg at [EBP+8] (CMP word ptr [EBP+8],DX at 0xfba13).
 *   Plain RET (no imm) => cdecl, caller cleans the stack arg.
 * Confirmed: byte return in AL — XOR AL,AL at 0xfba06, MOV AL,1 at
 *   0xfba1c, SETGE AL at 0xfba17.
 * Confirmed: branch order TEST ECX,ECX / JZ (state == 0 => return 1),
 *   JLE (state < 0 => return 0), CMP ECX,2 / JG (state > 2 => return 0),
 *   otherwise SETGE on the signed 16-bit compare [EBP+8] - DX, i.e.
 *   return param_1 >= state.
 * Unknown: the weapon animation-state enumeration behind the 0..2 range
 *   and the meaning of the stack operand relative to the DX operand; both
 *   parameters are only proven to be signed 16-bit state values.
 */
boolean weapon_state_interruptable(int16_t param_1, int16_t state)
{
  int state_value;

  /* MOVSX ECX,DX at 0xfba03: the state tests run sign-extended to 32 bits,
   * while the final compare stays 16-bit (CMP word ptr [EBP+8],DX). */
  state_value = state;
  if (state_value != 0) {
    if (state_value > 0 && state_value <= 2) {
      return (boolean)(param_1 >= state);
    }
    return 0;
  }
  return 1;
}

/* 0xfba20 — weapon_set_state
 *
 * Sets the weapon's animation state by looking up the animation graph
 * and choosing a random animation for the given state. The state
 * parameter arrives in BX.
 *
 * Confirmed: regparm, state in BX. 2 stack args (weapon_handle, param_2).
 * Confirmed: PUSH 4 / PUSH [EBP+8] / CALL object_get_and_verify_type.
 * Confirmed: tag_get(0x77656170, *obj) for weapon tag.
 * Confirmed: param_2 == 0 && weapon_data+0x1e8 != 0 => priority check.
 * Confirmed: tag+0x44 is the animation graph tag_index (antr).
 * Confirmed: tag_block at antr+0x18, element size 0x1c.
 * Confirmed: switch on state (0..10) maps to animation indices.
 * Confirmed: model_animation_choose_random(1, tag+0x44, anim_index).
 * Confirmed: stores to weapon_data+0x80, +0x82, +0x1e8.
 * Confirmed: tail section resolves unit handle and calls
 * unit_handle_weapon_state_change. Confirmed: returns AL=1 on success, AL=0 on
 * early exit.
 */
int weapon_set_state(int weapon_handle, char param_2, int16_t state)
{
  uint32_t *weapon_data =
    (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  int weap_tag = (int)tag_get(0x77656170, weapon_data[0]);
  int antr;
  int elem;
  int16_t anim_slot;
  uint16_t raw_index;
  int16_t chosen;

  /* Priority check: if param_2 is 0 and weapon has a current state,
   * only allow transitions from equal or higher priority */
  if (param_2 == 0) {
    int16_t current_state = (int16_t) * (char *)((char *)weapon_data + 0x1e8);
    if (current_state != 0) {
      if (current_state < 1)
        return 0;
      if (current_state > 2)
        return 0;
      if (state < current_state)
        return 0;
    }
  }

  /* Look up animation graph */
  if (*(int *)(weap_tag + 0x44) == -1)
    goto tail;

  antr = (int)tag_get(0x616e7472, *(int *)(weap_tag + 0x44));
  if (*(int *)(antr + 0x18) == 0)
    goto tail;

  elem = (int)tag_block_get_element((void *)(antr + 0x18), 0, 0x1c);
  if (elem == 0)
    goto tail;

  /* Map weapon animation state to animation block index */
  switch (state) {
  case 0:
    anim_slot = 0;
    break;
  case 1:
    anim_slot = 9;
    break;
  case 2:
    anim_slot = 10;
    break;
  case 3:
    anim_slot = 5;
    break;
  case 4:
    anim_slot = 6;
    break;
  case 5:
  case 6:
    anim_slot = 3;
    break;
  case 7:
  case 8:
    anim_slot = 8;
    break;
  case 9:
    anim_slot = 1;
    break;
  case 10:
    anim_slot = 2;
    break;
  default:
    goto tail;
  }

  /* Resolve animation index from the lookup table */
  if ((int)anim_slot < *(int *)(elem + 0x10)) {
    raw_index = *(uint16_t *)(*(int *)(elem + 0x14) + anim_slot * 2);
    if (raw_index == 0xffff) {
      if (state != 0)
        goto tail;
    }
  } else {
    raw_index = 0xffff;
    if (state != 0)
      goto tail;
  }

  /* Choose a random animation and set the weapon state */
  chosen = (int16_t)model_animation_choose_random(1, *(int *)(weap_tag + 0x44),
                                                  (int16_t)raw_index);
  *(int16_t *)((char *)weapon_data + 0x80) = chosen;
  *(int16_t *)((char *)weapon_data + 0x82) = 0;
  *(char *)((char *)weapon_data + 0x1e8) = (char)state;

tail:
  /* Resolve unit handle and notify sound system */
  {
    int unit_data;
    int unit_handle;
    int check;
    int check2;
    unit_data = (int)object_get_and_verify_type(weapon_handle, 4);
    unit_handle = -1;
    if (*(int *)(unit_data + 0xcc) != -1) {
      check =
        (int)object_try_and_get_and_verify_type(*(int *)(unit_data + 0xcc), 3);
      if (check != 0) {
        unit_handle = *(int *)(unit_data + 0xcc);
      }
    }
    check2 = (int)object_try_and_get_and_verify_type(unit_handle, 3);
    if (check2 != 0) {
      unit_handle_weapon_state_change(unit_handle, state);
    }
  }
  return 1;
}

/* weapon_set_total_rounds (0xfbbd0)
 *
 * Sets weapon magazine rounds from an input array. For each magazine,
 * clamps the input value to the magazine's maximum capacity, then clamps
 * the loaded rounds to not exceed the new total.
 */
void weapon_set_total_rounds(int weapon_handle, int16_t *rounds_array)
{
  int *weapon = (int *)object_get_and_verify_type(weapon_handle, 4);
  int tag_data = (int)tag_get(0x77656170, *weapon);
  int magazine_count;
  int16_t i;

  if (rounds_array == NULL) {
    display_assert("rounds_array", "c:\\halo\\SOURCE\\items\\weapons.c", 0xc0a,
                   1);
    system_exit(-1);
  }

  magazine_count = *(int *)(tag_data + 0x4f0);
  for (i = 0; (int)i < magazine_count; i++) {
    int check_tag;
    char *mag_def;
    int16_t max_rounds;
    int16_t input_rounds;
    int16_t new_total;
    char *magazine;
    int16_t current_loaded;
    int16_t new_loaded;
    check_tag = (int)tag_get(0x77656170, *weapon);
    if ((int16_t)i < 0 || (int)i >= *(int *)(check_tag + 0x4f0)) {
      display_assert("magazine_index>=0 && "
                     "magazine_index<weapon_definition->weapon.magazines.count",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
      system_exit(-1);
    }
    mag_def =
      (char *)tag_block_get_element((int *)(tag_data + 0x4f0), (int)i, 0x70);
    max_rounds = *(int16_t *)(mag_def + 8);
    input_rounds = rounds_array[i];
    new_total = (input_rounds < max_rounds) ? input_rounds : max_rounds;
    magazine = (char *)weapon + ((int)i * 3 + 0x96) * 4;
    current_loaded = *(int16_t *)(magazine + 8);
    *(int16_t *)(magazine + 6) = new_total;
    new_loaded = (current_loaded <= new_total) ? current_loaded : new_total;
    *(int16_t *)(magazine + 8) = new_loaded;
  }
}

/* 0xfbcf0 — power
 *
 * Confirmed: cdecl, 2 float stack args; FLD [EBP+8] then FLD [EBP+0xc]
 * (base in ST1, exponent in ST0), then tail-JMP 0x1d9e70 (_CIpow), whose
 * ST0 result is returned unchanged.  No xrefs to 0xfbcf0 in the artifact.
 */
float power(float base, float exponent)
{
  return (float)pow((double)base, (double)exponent);
}

/* 0xfbd00 — random
 * Body confirmed (get_global_random_seed_address -> random_seed_step):
 *   a file-local copy of the global-seed random() helper, emitted in
 *   weapons.obj between power (0xfbcf0) and weapon_new. Same step used
 *   inline by weapon_trigger_fire for the random firing-rate pick. */
uint16_t random(void)
{
  return random_seed_step((unsigned int *)get_global_random_seed_address());
}

/* 0xfbd10 — weapon_new
 *
 * Object-type "new" callback (only xref is DATA 0x323f4c). cdecl, 1 stack arg
 * at [EBP+8]; returns MOV AL,1.
 * Confirmed: byte [weapon+0x1e8]=0, dword [weapon+0x274]=-1 after tag_get.
 * Magazines loop (tag +0x4f0, element 0x70, assert line 0x672): per-weapon
 *   slot at weapon+(i*3+0x96)*4; +8 = min(elem+6, elem+0xa) (CMP CX,DX /
 *   JLE keeps elem+6), then +6 = elem+6 - that value.
 * Triggers loop (tag +0x4fc, element 0x114, assert line 0x667): slot at
 *   weapon+i*0x24+0x210; element pointer unused; dword +0x20=-1, byte +0=0x7f.
 * Field meanings are unproven; raw offsets retained.
 */
bool weapon_new(int weapon_handle)
{
  int *weapon = (int *)object_get_and_verify_type(weapon_handle, 4);
  int tag_data = (int)tag_get(0x77656170, *weapon);
  int16_t i;

  *((char *)weapon + 0x1e8) = 0;
  weapon[0x274 / 4] = -1;

  for (i = 0; (int)i < *(int *)(tag_data + 0x4f0); i++) {
    int check_tag;
    char *mag_def;
    char *magazine;
    int16_t loaded;
    check_tag = (int)tag_get(0x77656170, *weapon);
    if ((int16_t)i < 0 || (int)i >= *(int *)(check_tag + 0x4f0)) {
      display_assert("magazine_index>=0 && "
                     "magazine_index<weapon_definition->weapon.magazines.count",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
      system_exit(-1);
    }
    magazine = (char *)weapon + ((int)i * 3 + 0x96) * 4;
    mag_def =
      (char *)tag_block_get_element((int *)(tag_data + 0x4f0), (int)i, 0x70);
    loaded = (*(int16_t *)(mag_def + 6) > *(int16_t *)(mag_def + 0xa)) ?
               *(int16_t *)(mag_def + 0xa) :
               *(int16_t *)(mag_def + 6);
    *(int16_t *)(magazine + 8) = loaded;
    *(int16_t *)(magazine + 6) = *(int16_t *)(mag_def + 6) - loaded;
  }

  for (i = 0; (int)i < *(int *)(tag_data + 0x4fc); i++) {
    int check_tag;
    char *trigger;
    check_tag = (int)tag_get(0x77656170, *weapon);
    if ((int16_t)i < 0 || (int)i >= *(int *)(check_tag + 0x4fc)) {
      display_assert("trigger_index>=0 && "
                     "trigger_index<weapon_definition->weapon.triggers.count",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0x667, 1);
      system_exit(-1);
    }
    trigger = (char *)weapon + (int)i * 0x24 + 0x210;
    tag_block_get_element((int *)(tag_data + 0x4fc), (int)i, 0x114);
    *(int *)(trigger + 0x20) = -1;
    *trigger = 0x7f;
  }

  return true;
}

/* 0xfbea0 — weapon_delete
 *
 * Debug-build guard: when the game engine is running, a weapon that is a
 * flag (CTF flag / oddball style carried object) must never be deleted.
 * The whole body is the assert; there is no other work in this function.
 *
 * Confirmed: cdecl, 1 stack arg at [EBP+8] (weapon_index).
 * Confirmed: CALL game_engine_running (0xa8e30); TEST AL,AL; JZ exit.
 * Confirmed: PUSH 4 / PUSH [EBP+8] / CALL object_get_and_verify_type.
 * Confirmed: MOV ECX,[EAX] / PUSH ECX / PUSH 0x77656170 / CALL tag_get,
 *   then ADD ESP,0x10 clears both cdecl calls (2+2 stack args).
 * Confirmed: MOV EDX,[EAX+0x308]; SHR EDX,3; TEST DL,1 — the weapon_is_flag
 *   test is INLINED here (no CALL to 0xfb0c0), so it is spelled inline.
 * Confirmed: assert text "!weapon_is_flag(weapon_index)" at line 0xea,
 *   PUSH 1 (halt) / CALL display_assert then PUSH -1 / CALL system_exit.
 */
void weapon_delete(int weapon_index)
{
  int *obj;
  uint32_t *weap_tag;

  if (game_engine_running()) {
    obj = (int *)object_get_and_verify_type(weapon_index, 4);
    weap_tag = (uint32_t *)tag_get(0x77656170, *obj);
    assert_halt_msg_at("!weapon_is_flag(weapon_index)",
                       "c:\\halo\\SOURCE\\items\\weapons.c", 0xea,
                       ((weap_tag[0x308 / 4] >> 3) & 1) == 0);
  }
}

/* 0xfbf00 — weapon_export_function_values
 *
 * Evaluates the weapon definition's four exported function sources
 * (int16 array at weapon_definition+0x330) and writes one float per slot to
 * the outermost parent object's function-value array (+0xd4).
 *
 * Confirmed: cdecl, 1 stack arg at [EBP+8] (weapon_handle); referenced only
 *   from data (0x323f5c). Prior kb decl (void) omitted the stack arg.
 * Confirmed: object_get_and_verify_type(weapon_handle, 4) -> tag_get(
 *   0x77656170, *weapon) (one ADD ESP,0x10), then parent walk while
 *   (obj[+4] & 1) && obj[+0xcc] != -1 via object_get_and_verify_type(.., -1).
 * Confirmed: MOVSX/DEC/CMP 0xf jump table over sources 1..16; value is
 *   zeroed before the switch and stored for every non-zero source.
 * Confirmed: source 10/11 and 12 call tag_block_get_element(+0x4fc, i,
 *   0x114) -> weapon_trigger_get(EDI=weapon, SI=i) ->
 *   weapon_trigger_get_charged_fraction(EAX=weapon_handle, CX=i); the
 *   source 10/11 weapon_trigger_get result is discarded in the binary.
 * Confirmed: source 9 compares weapon_definition+0x34c to 0x3f800000 with an
 *   integer CMP; 0x2533c8 is the 1.0f constant, 0x2533c0 the 0.0f constant.
 * Unknown: meanings of the source enum values and of weapon object fields
 *   +0x1dc/+0x1ec/+0x1f0/+0x1f4/+0x1f8/+0x278 and definition +0x360.
 */
void weapon_export_function_values(int weapon_handle)
{
  char *weapon_obj;
  char *weapon_defn;
  char *obj;
  float *function_values; /* name: PAL 2342 items/weapons.c:807 */
  int16_t *function_mode; /* PAL 2342 items/weapons.c:808 function_modes */
  int count;
  float function_value; /* name: PAL 2342 items/weapons.c:822 */
  float t;
  int16_t i;
  char *trigger_defn;
  char *trigger;
  char *magazine_defn;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  weapon_defn = (char *)tag_get(0x77656170, *(int *)weapon_obj);
  obj = weapon_obj;
  while ((*(uint8_t *)(obj + 4) & 1) != 0 && *(int *)(obj + 0xcc) != -1) {
    obj = (char *)object_get_and_verify_type(*(int *)(obj + 0xcc), -1);
  }
  function_values = (float *)(obj + 0xd4);
  function_mode = (int16_t *)(weapon_defn + 0x330);
  count = 4;
  do {
    if (*function_mode != 0) {
      function_value = 0.0f;
      switch (*function_mode) {
      case 6:
        function_value = 1.0f;
        break;
      case 1:
        function_value = *(float *)(weapon_obj + 0x1ec);
        break;
      case 9:
        if ((*(uint8_t *)(weapon_obj + 0x1dc) & 1) != 0 &&
            *(int *)(weapon_defn + 0x34c) != 0x3f800000) {
          function_value =
            (*(float *)(weapon_obj + 0x1ec) - *(float *)(weapon_defn + 0x34c)) /
            (*(float *)0x2533c8 - *(float *)(weapon_defn + 0x34c));
        }
        break;
      case 12:
        for (i = 0; (int)i < *(int *)(weapon_defn + 0x4fc); i++) {
          trigger_defn =
            (char *)tag_block_get_element(weapon_defn + 0x4fc, (int)i, 0x114);
          trigger = (char *)weapon_trigger_get(weapon_obj, i);
          if (*(float *)(trigger_defn + 0x48) > *(float *)0x2533c0) {
            t = weapon_trigger_get_charged_fraction(weapon_handle, i) *
                *(float *)(trigger_defn + 0x54);
            if (!(function_value > t)) {
              function_value = t;
            }
          }
          if (trigger[1] == 3) {
            t = (*(float *)0x2533c8 - *(float *)(trigger_defn + 0x54)) *
                  *(float *)(weapon_obj + 0x1f4) +
                *(float *)(trigger_defn + 0x54);
            if (!(function_value > t)) {
              function_value = t;
            }
          }
          if (!(function_value > *(float *)(trigger + 0x18))) {
            function_value = *(float *)(trigger + 0x18);
          }
          *(float *)(trigger + 0x18) = function_value;
        }
        t = *(float *)(weapon_defn + 0x360) * *(float *)(weapon_obj + 0x1ec);
        if (!(function_value > t)) {
          function_value = t;
        }
        break;
      case 2:
      case 3:
        i = *function_mode - 2;
        if ((int)i < *(int *)(weapon_defn + 0x4f0)) {
          magazine_defn =
            (char *)tag_block_get_element(weapon_defn + 0x4f0, (int)i, 0x70);
          if (*(int16_t *)(magazine_defn + 10) != 0) {
            function_value =
              (float)(int)*(int16_t *)(weapon_obj + 0x260 + i * 12) /
              (float)(int)*(int16_t *)(magazine_defn + 10);
          }
        }
        break;
      case 7:
      case 8:
        i = *function_mode - 7;
        if ((int)i < *(int *)(weapon_defn + 0x4fc)) {
          function_value = *(float *)(weapon_obj + 0x224 + i * 36);
        }
        break;
      case 4:
      case 5:
        i = *function_mode - 4;
        if ((int)i < *(int *)(weapon_defn + 0x4fc)) {
          function_value = *(float *)(weapon_obj + 0x220 + i * 36);
        }
        break;
      case 15:
      case 16:
        i = *function_mode - 15;
        if ((int)i < *(int *)(weapon_defn + 0x4fc)) {
          function_value = *(float *)(weapon_obj + 0x220 + i * 36);
          if (game_time_get() - *(int *)(weapon_obj + 0x278) > 1) {
            function_value = 0.0f;
          }
        }
        break;
      case 10:
      case 11:
        i = *function_mode - 10;
        if ((int)i < *(int *)(weapon_defn + 0x4fc)) {
          tag_block_get_element(weapon_defn + 0x4fc, (int)i, 0x114);
          weapon_trigger_get(weapon_obj, i);
          function_value =
            weapon_trigger_get_charged_fraction(weapon_handle, i);
        }
        break;
      case 14:
        function_value = *(float *)(weapon_obj + 0x1f8);
        break;
      case 13:
        function_value = *(float *)(weapon_obj + 0x1f0);
        break;
      }
      *function_values = function_value;
    }
    function_mode++;
    function_values++;
    count--;
  } while (count != 0);
}

/* Transfer ammunition from a source object into a weapon's magazines (0xfc290).
 * For each magazine that's below initial capacity, tries to transfer rounds
 * from either the same weapon type or matching equipment. Deletes the source if
 * fully depleted. Returns true if any ammo source was matched. */
bool weapon_handle_potential_inventory_item(int weapon_handle,
                                            int source_handle,
                                            uint16_t local_player_index,
                                            int16_t *rounds_out)
{
  int *weapon_data = (int *)object_get_and_verify_type(weapon_handle, 4);
  int tag_data = (int)tag_get(0x77656170, *weapon_data);
  int *source_data = (int *)object_get_and_verify_type(source_handle, 0x1c);
  int source_tag = *source_data;
  bool found = false;

  {
    int16_t i;
    for (i = 0; (int)i < *(int *)(tag_data + 0x4f0); i++) {
      int check_tag;
      int mag_offset;
      char *mag_def;
      int16_t *mag_rounds;
      int16_t transfer;
      check_tag = (int)tag_get(0x77656170, *weapon_data);
      if ((int16_t)i < 0 || (int)i >= *(int *)(check_tag + 0x4f0)) {
        display_assert(
          "magazine_index>=0 && "
          "magazine_index<weapon_definition->weapon.magazines.count",
          "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
        system_exit(-1);
      }
      mag_offset = ((int)i * 3 + 0x96) * 4;
      mag_def =
        (char *)tag_block_get_element((char *)tag_data + 0x4f0, (int)i, 0x70);
      mag_rounds = (int16_t *)((char *)weapon_data + mag_offset + 6);
      transfer = 0;
      if (*mag_rounds < *(int16_t *)(mag_def + 8)) {
        int16_t need;
        need = *(int16_t *)(mag_def + 8) - *mag_rounds;
        if (*weapon_data == source_tag) {
          int *src_weap;
          int src_tag2;
          int16_t *src_rounds;
          src_weap = (int *)object_get_and_verify_type(source_handle, 4);
          src_tag2 = (int)tag_get(0x77656170, *src_weap);
          if ((int16_t)i < 0 || (int)i >= *(int *)(src_tag2 + 0x4f0)) {
            display_assert(
              "magazine_index>=0 && "
              "magazine_index<weapon_definition->weapon.magazines.count",
              "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
            system_exit(-1);
          }
          src_rounds = (int16_t *)((char *)src_weap + mag_offset + 6);
          transfer = need;
          if (*src_rounds <= need) {
            transfer = *src_rounds;
          }
          if (transfer > 0) {
            *src_rounds = *src_rounds - transfer;
            if (*(int *)(tag_data + 0x49c) != -1 &&
                (int16_t)local_player_index != -1) {
              sound_impulse_start(*(int *)(tag_data + 0x49c), 1.0f);
            }
            if (*src_rounds == 0) {
              object_delete(source_handle);
            }
          }
          found = true;
        } else {
          int *equip_block;
          int16_t j;
          equip_block = (int *)(mag_def + 0x64);
          for (j = 0; (int)j < *equip_block; j++) {
            int16_t *entry;
            entry = (int16_t *)tag_block_get_element(equip_block, (int)j, 0x1c);
            if (*(int *)(entry + 0xc) == source_tag) {
              transfer = need;
              if (*entry <= need) {
                transfer = *entry;
              }
              if (transfer > 0) {
                if ((int16_t)local_player_index != -1) {
                  equipment_definition_handle_pickup(*(int *)(entry + 0xc));
                }
                object_delete(source_handle);
                found = true;
                break;
              }
            }
          }
        }
        *mag_rounds = *mag_rounds + transfer;
        *rounds_out = transfer;
      }
    }
  }
  return found;
}

/* weapon_owner_update (0xfc4b0)
 *
 * Validates the weapon object (type 4) and resolves its 'weap' tag, stores the
 * low 16 bits of a1 at weapon+0x1e0, then evaluates transition function type 4
 * over a2 and stores the result as the float at weapon+0x1e4
 * (weapon->weapon.primary_trigger, per the assert string). Asserts the stored
 * value is a valid real (exponent != 0xff).
 *
 * Confirmed: PUSH 4 / PUSH handle -> object_get_and_verify_type (0x13d680).
 * Confirmed: PUSH weapon[0] / PUSH 'weap' -> tag_get (0x1ba140), result unused.
 * Confirmed: MOV DX,[EBP+0xc]; MOV [ESI+0x1e0],DX — 16-bit store of a1.
 * Confirmed: PUSH [EBP+0x10] / PUSH 4 -> transition_function_evaluate
 * (0x10a710) = (function_type=4, t=a2); FST [ESI+0x1e4].
 * Confirmed: assert reads the int bits back from [ESI+0x1e4] for %08X and
 * passes the still-live ST0 as the %f double; line 0x4af. */
void weapon_owner_update(int weapon_handle, int a1, float a2)
{
  uint32_t *weapon_data;
  float value;

  weapon_data = (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  tag_get(0x77656170, weapon_data[0]);

  *(int16_t *)((int)weapon_data + 0x1e0) = (int16_t)a1;

  value = (*(float *)((int)weapon_data + 0x1e4) =
             transition_function_evaluate(4, a2));

  if ((*(uint32_t *)&value & 0x7f800000) == 0x7f800000) {
    display_assert(
      csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
               "weapon->weapon.primary_trigger",
               *(uint32_t *)((int)weapon_data + 0x1e4), (double)value),
      "c:\\halo\\SOURCE\\items\\weapons.c", 0x4af, 1);
    system_exit(-1);
  }
}

/* weapon_build_weapon_interface_state (0xfc550)
 *
 * Fills the caller's weapon-interface state record (state; name: PAL 2342
 * items/weapons.c:1141) from a weapon
 * object and its 'weap' tag: two dwords copied from weapon+0x1ec/0x1f0,
 * a one-bit flag from weapon+0x1dc, the magazine count, then a 10-byte
 * record per magazine.
 *
 * Confirmed: PUSH 4 / PUSH handle -> object_get_and_verify_type (0x13d680).
 * Confirmed: PUSH weapon[0] / PUSH 'weap' -> tag_get (0x1ba140) both times.
 * Confirmed: magazines tag_block is at weapon_definition+0x4f0; its first
 * dword is the element count, stored as a 16-bit field at state+0xa.
 * Confirmed: loop counter is a 16-bit value re-sign-extended each iteration
 * (INC EAX / MOVSX ESI,AX), the assert tests the 16-bit form for < 0.
 * Confirmed: PUSH 0x70 / PUSH index / PUSH block -> tag_block_get_element
 * (0x19b210), element size 0x70.
 * Confirmed: per-magazine source fields are weapon+0x258+index*0xc (+0, +6,
 * +8) and magazine_definition+8/+0xa; destinations are state+index*10 at
 * +0xc, +0xd, +0xe, +0x10, +0x12 and +0x14.
 */
void weapon_build_weapon_interface_state(int weapon_handle, int state)
{
  int *weapon_data;
  void *weapon_definition;
  int *magazines_block;
  void *magazine_definition;
  int *magazine;
  int out_base;
  int index;
  int reloading; /* name: PAL 2342 items/weapons.c:1157 */
  int16_t magazine_index;

  weapon_data = (int *)object_get_and_verify_type(weapon_handle, 4);
  weapon_definition = tag_get(0x77656170, weapon_data[0]);

  *(int *)state = weapon_data[0x7b];
  *(int *)(state + 4) = weapon_data[0x7c];

  *(uint8_t *)(state + 8) =
    (uint8_t)(*(uint8_t *)((int)weapon_data + 0x1dc) & 1);

  magazines_block = (int *)((int)weapon_definition + 0x4f0);
  *(int16_t *)(state + 10) = *(int16_t *)magazines_block;

  index = 0;
  magazine_index = 0;

  if (*magazines_block > 0) {
    do {
      weapon_definition = tag_get(0x77656170, weapon_data[0]);
      if ((magazine_index < 0) ||
          (index >= *(int *)((int)weapon_definition + 0x4f0))) {
        display_assert(
          "magazine_index>=0 && "
          "magazine_index<weapon_definition->weapon.magazines.count",
          "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
        system_exit(-1);
      }

      magazine = weapon_data + index * 3 + 0x96;
      magazine_definition = tag_block_get_element(magazines_block, index, 0x70);

      reloading = *(int16_t *)magazine == 1 || *(int16_t *)magazine == 3;
      /* Magazine state 1 or 3 counts as reloading; the bool form, not an
       * if/else ladder, is what emits the reference's CMP CX,1 / JE /
       * CMP CX,3 sequence with immediate stores to the flag slot
       * (PAL 2342 weapons.c:1157 spells it the same way).
       */

      out_base = state + index * 10;
      *(uint8_t *)(out_base + 0xc) = (uint8_t)reloading;
      *(uint8_t *)(out_base + 0xd) = (uint8_t)(*(int16_t *)magazine == 0);
      *(int16_t *)(out_base + 0xe) = *(int16_t *)((int)magazine + 8);
      *(int16_t *)(out_base + 0x10) =
        *(int16_t *)((int)magazine_definition + 0xa);
      *(int16_t *)(out_base + 0x12) = *(int16_t *)((int)magazine + 6);
      *(int16_t *)(state + (index * 5 + 10) * 2) =
        *(int16_t *)((int)magazine_definition + 8);

      magazine_index = (int16_t)(magazine_index + 1);
      index = magazine_index;
    } while (index < *magazines_block);
  }
}

/* weapon_reloading (0xfc690)
 *
 * True when the weapon has at least one magazine and the weapon object's
 * magazine state word at +0x258 equals 1 (reloading).
 *
 * Confirmed: PUSH 4 / PUSH [EBP+8] / CALL object_get_and_verify_type
 * (0x13d680); result kept in ESI for the whole body.
 * Confirmed: PUSH [ESI] / PUSH 0x77656170 / CALL tag_get (0x1ba140) twice —
 * cdecl, tag_index is the first dword of the weapon object.
 * Confirmed: MOV ECX,[EAX+0x4f0] / TEST ECX,ECX / JLE exit — the magazines
 * tag_block element count gates the whole body; on that path AL = BL = 0.
 * Confirmed: the second tag_get + TEST/JG pair is the inlined magazine
 * accessor's bounds assert for index 0 (only the `0 < count` half survives
 * constant folding); its returned pointer is unused.
 * Confirmed: assert path PUSH 1 / PUSH 0x672 (line 1650) / PUSH filepath /
 * PUSH reason / CALL display_assert (0x8d9f0), then PUSH -1 / CALL
 * system_exit (0x8e2f0).
 * Confirmed: CMP word ptr [ESI+0x258],1 / MOV AL,1 / JZ — 16-bit compare,
 * bool return in AL.
 * Unknown: the weapon-object field at +0x258 and the tag-data layout at
 * +0x4f0 beyond the leading count are not modelled; raw offsets retained.
 */
bool weapon_reloading(int weapon_handle)
{
  char *weapon_data;
  bool result;

  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
  result = false;

  if (*(int *)((char *)tag_get(0x77656170, *(int *)weapon_data) + 0x4f0) > 0) {
    assert_halt_msg_at(
      "magazine_index>=0 && "
      "magazine_index<weapon_definition->weapon.magazines."
      "count",
      "c:\\halo\\SOURCE\\items\\weapons.c", 0x672,
      *(int *)((char *)tag_get(0x77656170, *(int *)weapon_data) + 0x4f0) > 0);
    result = (*(int16_t *)(weapon_data + 0x258) == 1);
  }

  return result;
}

/* weapon_rotate_zoom_level (0xfc710)
 *
 * Advances the weapon's zoom level one step and wraps back to the unzoomed
 * state (-1) after the last level. Returns the new zoom level.
 *
 * Confirmed: MOV EDI,[EBP+8] / PUSH 4 / PUSH EDI / CALL
 * object_get_and_verify_type (0x13d680); PUSH [EAX] / PUSH 0x77656170 /
 * CALL tag_get (0x1ba140), result kept in ESI.
 * Confirmed: PUSH EDI / CALL weapon_reloading (0xfc690) — the single ADD
 * ESP,0x14 after it cleans all five pushes, so the weapon handle is
 * weapon_reloading's only argument.
 * Confirmed: TEST AL,AL / JNZ -> MOV AX,[EBP+0xc] / RET — while reloading the
 * current level is returned unchanged.
 * Confirmed: TEST AX,AX / JL and MOVSX ECX,word ptr [ESI+0x3da] / DEC ECX /
 * MOVSX EDX,AX / CMP EDX,ECX / JGE — sign-extended 16-bit compares against
 * (level count - 1).
 * Confirmed: in-range path INC EAX (level + 1); out-of-range path XOR EAX,EAX
 * / SETNZ AL / DEC EAX, i.e. -1 when the level equals count-1 (wrap to
 * unzoomed) and 0 otherwise (a negative level enters the first zoom level).
 * Confirmed: return value is 16-bit (AX).
 * Unknown: the weapon tag field at +0x3da is the zoom-level count; the rest of
 * the tag layout is not modelled, so the raw offset is retained.
 */
int16_t weapon_rotate_zoom_level(int weapon_datum, int16_t current_index)
{
  char *weapon_data;
  char *weapon_definition;

  weapon_data = (char *)object_get_and_verify_type(weapon_datum, 4);
  weapon_definition = (char *)tag_get(0x77656170, *(int *)weapon_data);

  if (!weapon_reloading(weapon_datum)) {
    if (current_index >= 0 &&
        (int)current_index < (int)*(int16_t *)(weapon_definition + 0x3da) - 1) {
      return (int16_t)(current_index + 1);
    }

    return (int16_t)(((int)current_index !=
                      (int)*(int16_t *)(weapon_definition + 0x3da) - 1) -
                     1);
  }

  return current_index;
}

/* weapon_get_zoom_magnification (0xfc780)
 *
 * Returns the magnification for a zoom level: 1.0 when the level is out of
 * range, otherwise min * pow(max / min, level / (count - 1)).
 *
 * Confirmed: PUSH 4 / PUSH handle -> object_get_and_verify_type (0x13d680);
 * PUSH [EAX] / PUSH 0x77656170 -> tag_get (0x1ba140), one ADD ESP,0x10.
 * Confirmed: [EBP-4] = 1.0f before the calls; it is the out-of-range result.
 * Confirmed: MOV DX,[EBP+0xc] / TEST DX,DX / JL and CMP DX,[tag+0x3da] / JGE
 * are 16-bit signed range checks.
 * Confirmed: count > 1 -> FILD level / FIDIV (count - 1) stored as float;
 * otherwise the ratio is 0.0f.
 * Confirmed: tag+0x3dc and tag+0x3e0 each fall back to 1.0f unless > 0.0f
 * (FCOMP [0x2533c0]; TEST AH,0x41; JNZ).
 * Confirmed: FLD max / FDIV min / FLD ratio / CALL 0x1d9e70 (_CIpow, ST1^ST0)
 * then FMUL min.
 * Confirmed: assert_valid_real at line 0x5a2 passes the int bits (%08X) and a
 * double (%f); "magnification>0.0f" at line 0x5a3.
 * Unknown: tag fields +0x3da/+0x3dc/+0x3e0 are not modelled; the names
 * count/min/max are behaviour-only.
 */
float weapon_get_zoom_magnification(int weapon_handle, int zoom_level)
{
  float magnification;
  float ratio;
  float minimum;
  float maximum;
  char *weapon_definition;
  int16_t level;
  int16_t count;

  magnification = 1.0f;
  weapon_definition = (char *)tag_get(
    0x77656170, *(int *)object_get_and_verify_type(weapon_handle, 4));
  level = (int16_t)zoom_level;

  if (level >= 0) {
    count = *(int16_t *)(weapon_definition + 0x3da);
    if (level < count) {
      if (count > 1) {
        ratio = (float)(int)level / (float)(int)(count - 1);
      } else {
        ratio = 0.0f;
      }

      if (*(float *)(weapon_definition + 0x3dc) > 0.0f) {
        minimum = *(float *)(weapon_definition + 0x3dc);
      } else {
        minimum = 1.0f;
      }

      if (*(float *)(weapon_definition + 0x3e0) > 0.0f) {
        maximum = *(float *)(weapon_definition + 0x3e0);
      } else {
        maximum = 1.0f;
      }

      magnification =
        (float)(pow((double)(maximum / minimum), (double)ratio) * minimum);

      if ((*(uint32_t *)&magnification & 0x7f800000) == 0x7f800000) {
        display_assert(csprintf((char *)0x5ab100,
                                "%s: assert_valid_real(0x%08X %f)",
                                "magnification", *(uint32_t *)&magnification,
                                (double)magnification),
                       "c:\\halo\\SOURCE\\items\\weapons.c", 0x5a2, 1);
        system_exit(-1);
      }

      assert_halt_msg_at("magnification>0.0f",
                         "c:\\halo\\SOURCE\\items\\weapons.c", 0x5a3,
                         magnification > 0.0f);
    }
  }

  return magnification;
}

/* weapon_get_field_of_view (0xfc8e0)
 *
 * Divides the base field of view by the zoom magnification; the zoomed value
 * is used only when it lies strictly between the two limits, otherwise the
 * base value is returned unchanged.
 *
 * Confirmed: PUSH [EBP+0x10] / PUSH [EBP+0x8] / CALL 0xfc780 / ADD ESP,0x8 --
 * cdecl (weapon_handle, zoom_level); [EBP+0xc] is copied to [EBP-4] first.
 * Confirmed: FCOM [0x2533c8] (1.0f); TEST AH,0x44 / JNP -> magnification ==
 * 1.0f returns the base value.
 * Confirmed: FDIVR [EBP+0xc] -> base / magnification.
 * Confirmed: FCOM [0x28af00] (0.031415928f); TEST AH,0x41 / JNZ, then
 * FCOM [0x28aefc] (3.1101768f); TEST AH,0x5 / JNP -> strict range test.
 */
real weapon_get_field_of_view(int weapon_handle, real base_field_of_view,
                              uint16_t zoom_level)
{
  real field_of_view;
  real magnification;

  field_of_view = base_field_of_view;
  magnification = weapon_get_zoom_magnification(weapon_handle, zoom_level);
  if (magnification != 1.0f) {
    magnification = base_field_of_view / magnification;
    if (magnification > 0.031415928f && magnification < 3.1101768f) {
      field_of_view = magnification;
    }
  }

  return field_of_view;
}

/* weapon_prevents_melee_attack (0xfc930)
 *
 * True when the weapon blocks a melee attack: either the weapon tag sets the
 * "prevents melee attack" flag, or the weapon object is in one of two states
 * (2, 3) at +0x211.
 *
 * Confirmed: CMP ESI,-0x1 / MOV AL,0x1 / JZ exit — a null weapon handle
 * (-1) returns 1 before any call is made.
 * Confirmed: PUSH 0x4 / PUSH ESI / CALL object_get_and_verify_type
 * (0x13d680) — cdecl, (weapon_handle, 4).
 * Confirmed: MOV EAX,[EAX] / PUSH EAX / PUSH 0x77656170 / CALL tag_get
 * (0x1ba140) — cdecl, ('weap', first dword of the weapon object).
 * Confirmed: MOV EBX,[EAX+0x308] / SHR EBX,0x9 / AND BL,0x1 — bit 9 of the
 * dword flags field at tag+0x308, computed BEFORE the second call.
 * Confirmed: PUSH 0x4 / PUSH ESI / CALL object_get_and_verify_type again —
 * the single ADD ESP,0x18 cleans all six pushes of the three calls.
 * Confirmed: MOV AL,byte ptr [EAX+0x211] / CMP AL,0x2 / JZ / CMP AL,0x3 /
 * JNZ — byte compare; on either match MOV AL,0x1 and return, otherwise
 * MOV AL,BL returns the tag flag.
 * Unknown: the weapon-object byte at +0x211 and the tag flags word at +0x308
 * are not modelled; raw offsets retained.
 */
char weapon_prevents_melee_attack(int weapon_handle)
{
  char *weapon_data;
  char tag_flag;
  char state;
  char result;

  result = 1;
  if (weapon_handle != -1) {
    weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
    tag_flag = (char)((*(unsigned int *)((char *)tag_get(0x77656170,
                                                         *(int *)weapon_data) +
                                         0x308) >>
                       9) &
                      1);

    weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
    state = *(weapon_data + 0x211);

    if (((state == 2) || (state == 3)) != 0) {
      return 1;
    }

    result = tag_flag;
  }

  return result;
}

/* Begin a magazine reload cycle (0xfc990).
 * If the magazine state is idle (0) or post-reload (2), and the weapon is
 * not in an animation, starts the reload animation and effect. For dual-wield
 * weapons (tag+0x4e2 == 1), computes the animation variant from whether the
 * magazine is one round short of full.
 *
 * Confirmed: magazine_index in AX (register arg), 2 stack args (weapon_handle,
 * param_2). Confirmed: calls object_get_and_verify_type(weapon_handle, 4)
 * twice. Confirmed: calls weapon_magazine_get(weapon_obj@<edi>,
 * magazine_index@<si>). Confirmed: calls weapon_set_state(weapon_handle, 0,
 * magazine_index+5 @<bx>). Confirmed: calls weapon_effect_new(mag_def[0x44],
 * 0, 0, weapon_handle@<eax>). Confirmed: calls
 * first_person_weapon_message_from_weapon(weapon_handle, 9 or 10). Confirmed:
 * calls weapon_get_first_person_animation_time(weapon_handle, 0, 7, iVar6).
 * Confirmed: clears bit 3 of weapon_obj[0x1dc] on non-early-exit path.
 */
void weapon_magazine_start_reload(int16_t magazine_index, int weapon_handle,
                                  int param_2)
{
  char *weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  int16_t *magazine_state =
    (int16_t *)weapon_magazine_get((void *)weapon_obj, magazine_index);
  int tag_data = (int)tag_get(0x77656170, *(int *)weapon_obj);
  char *mag_def = (char *)tag_block_get_element((char *)tag_data + 0x4f0,
                                                (int)magazine_index, 0x70);

  if (*magazine_state == 0 || *magazine_state == 2) {
    int iVar6 = (int)object_get_and_verify_type(weapon_handle, 4);
    if (*(char *)(iVar6 + 0x211) == 0 && *(char *)(iVar6 + 0x235) == 0 &&
        *(char *)(iVar6 + 0x1e8) == 0) {
      if (magazine_state[3] > 0 &&
          magazine_state[4] < *(int16_t *)(mag_def + 0xa)) {
        int16_t anim_variant = -1;
        int16_t frame;
        weapon_set_state(weapon_handle, 0, (int16_t)(magazine_index + 5));
        weapon_effect_new(*(int *)(mag_def + 0x44), 0, 0, weapon_handle);
        first_person_weapon_message_from_weapon(weapon_handle,
                                                (magazine_state[4] != 0) + 9);

        if (*(int16_t *)(tag_data + 0x4e2) == 1) {
          int diff_is_one =
            ((int)*(int16_t *)(mag_def + 0xa) - (int)magazine_state[4]) == 1;
          if (param_2 == 0) {
            anim_variant = diff_is_one ? 1 : -1;
          } else {
            anim_variant = diff_is_one ? 2 : 0;
          }
        }

        *magazine_state = 1;
        frame = weapon_get_first_person_animation_time(weapon_handle, 0, 7,
                                                       anim_variant);
        magazine_state[1] = frame;
        magazine_state[2] = frame;
      }
      *(uint32_t *)(weapon_obj + 0x1dc) &= ~0x8u;
    }
  }
}

/* Complete a magazine reload cycle (0xfcaf0).
 * Transfers rounds from unloaded reserve to the loaded count, capped by
 * the tag's rounds-per-reload and maximum-rounds fields. Adjusts reserve
 * for dual-wield. Optionally starts the next reload cycle if rounds remain. */
void weapon_magazine_finish_reload(int weapon_handle, int magazine_index)
{
  char *weapon_obj;
  int16_t *magazine;
  void *tag_data;
  char *mag_def;
  int16_t rounds_unloaded;
  int16_t rounds_reload;
  int rounds_to_load;
  int total;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  magazine =
    (int16_t *)weapon_magazine_get((void *)weapon_obj, (int16_t)magazine_index);
  tag_data = tag_get(0x77656170, *(int *)weapon_obj);
  mag_def = (char *)tag_block_get_element((char *)tag_data + 0x4f0,
                                          (int)(int16_t)magazine_index, 0x70);

  if ((*mag_def & 1) != 0) {
    magazine[4] = 0;
  }

  rounds_unloaded = magazine[3];
  rounds_reload = *(int16_t *)(mag_def + 0x18);
  rounds_to_load = rounds_unloaded;
  if (rounds_reload <= rounds_unloaded) {
    rounds_to_load = rounds_reload;
  }

  total = (int16_t)(magazine[4] + rounds_to_load);
  if (total > *(uint16_t *)(mag_def + 0xa)) {
    total = *(uint16_t *)(mag_def + 0xa);
  }

  if (*(char *)0x5aa892 == 0 && (*(uint8_t *)(weapon_obj + 0x1a4) & 2) != 0) {
    magazine[3] = (int16_t)(rounds_unloaded - total + magazine[4]);
  }

  magazine[4] = (int16_t)total;
  magazine[0] = 2;
  magazine[1] = 0;

  if (magazine[3] > 0 && total < *(int16_t *)(mag_def + 0xa) &&
      (*mag_def & 1) == 0 && (*(uint8_t *)(weapon_obj + 0x1e0) & 0x26) == 0) {
    weapon_magazine_start_reload((int16_t)magazine_index, weapon_handle, 0);
  }
}

/* weapon_magazine_start_chamber — begin a magazine chambering cycle (0xfcbd0).
 *
 * Name evidence (T2, magazine state machine): weapon_update (0xfe910)
 *   drives magazine state 1 (reloading) -> weapon_magazine_finish_reload,
 *   state 2 -> this function, state 3 -> weapon_magazine_finish_chamber
 *   (0xfcc90). This function sets state 3, plays mag_def[+0x54] (the
 *   tag reference one slot after the reload effect at +0x44) and sets
 *   weapon animation state magazine_index+3, where +5 is the reload
 *   animation pair — so +3 is the "chamber-1"/"chamber-2" pair.
 *
 * Confirmed: magazine_index arrives in EAX (MOV EBX,EAX at 0xfcbd6, used as
 * MOVSX ECX,BX for the tag-block index and as the @<bx> argument after
 * ADD EBX,3); weapon_handle is the single stack argument at [EBP+8].
 * Confirmed: calls object_get_and_verify_type(weapon_handle, 4) twice —
 * the first result (EDI) supplies the tag index, the second (EAX) is the
 * object the three state bytes are read from.
 * Confirmed: calls FUN_000fb370(weapon_obj@<edi>, magazine_index@<si>) and
 * the returned pointer (ESI) is the magazine state block.
 * Confirmed: guard is *state == 0 || *state == 2, then bytes +0x211, +0x235
 * and +0x1e8 must all be zero; tag_get/tag_block_get_element run only inside
 * that guard (unlike the sibling at 0xfc990, which hoists them).
 * Confirmed: weapon_set_state(weapon_handle, 0,
 * magazine_index+3 @<bx>) precedes weapon_start_effect(mag_def[0x54], 0, 0,
 * weapon_handle@<eax>); the two float arguments are PUSH 0x0 slots.
 * Confirmed: *state = 3 (MOV word ptr [ESI],0x3), then
 * FLD [EDI+0x1c] / FMUL [0x253394] / CALL _ftol2 / MOV [ESI+2],AX —
 * 0x253394 is TICKS_PER_SECOND (30.0f), stored as int16_t.
 * Unknown: the semantic meaning of magazine state 3 and of mag_def+0x1c.
 */
void weapon_magazine_start_chamber(int16_t magazine_index, int weapon_handle)
{
  char *weapon_obj;
  int16_t *magazine_state;
  char *object;
  char *tag_data;
  char *mag_def;
  int state;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  magazine_state =
    (int16_t *)weapon_magazine_get((void *)weapon_obj, magazine_index);

  state = *magazine_state;
  switch (state) {
  case 0:
  case 2: {
    object =
      (char *)object_get_and_verify_type(*(volatile int *)&weapon_handle, 4);
    if (*(object + 0x211) == 0 && *(object + 0x235) == 0 &&
        *(object + 0x1e8) == 0) {
      tag_data = (char *)tag_get(0x77656170, *(int *)weapon_obj);
      mag_def = (char *)tag_block_get_element(tag_data + 0x4f0,
                                              (int)magazine_index, 0x70);
      weapon_set_state(weapon_handle, 0, (int16_t)(magazine_index + 3));
      weapon_effect_new(*(int *)(mag_def + 0x54), 0, 0, weapon_handle);
      *magazine_state = 3;
      magazine_state[1] =
        (int16_t)(int)(*(float *)(mag_def + 0x1c) * TICKS_PER_SECOND);
    }
    break;
  }
  default:
    break;
  }
}

/* 0xfcc90 — weapon_magazine_finish_chamber */
void weapon_magazine_finish_chamber(int16_t magazine_index, int weapon_handle)
{
  char *weapon;
  int16_t *mag_state;
  char *tag;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  mag_state = (int16_t *)weapon_magazine_get(weapon, magazine_index);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  tag_block_get_element(tag + 0x4f0, (int)magazine_index, 0x70);

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  mag_state = (int16_t *)weapon_magazine_get(weapon, magazine_index);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  tag_block_get_element(tag + 0x4f0, (int)magazine_index, 0x70);

  mag_state[0] = 0;
  mag_state[1] = 0;
}

/* weapon_trigger_fully_charged — the trigger has finished charging (0xfcd10).
 *
 * Name evidence (T2, trigger state machine): weapon_update (0xfe910)
 *   calls this from trigger state 2 (charging) exactly when the tick
 *   counter reaches 0. It sets state 3, for which
 *   weapon_trigger_get_charged_fraction returns exactly 1.0f, and sets
 *   weapon animation state trigger_index+7 — the "charged-1"/"charged-2"
 *   pair (strings at 0x290c08/0x290c14).
 *
 * Confirmed: trigger_index arrives in AX (MOV ESI,EAX at 0xfcd1d, then
 *   MOVSX EDI,SI); weapon_handle is the single stack arg at [EBP+8].
 * Confirmed: object_get_and_verify_type(weapon_handle, 4) at 0xfcd1f, then
 *   weapon_trigger_get(EDI=object, SI=trigger_index) at 0xfcd26 — its return
 * value is discarded (EAX is immediately reloaded with MOV EAX,[EDI]); the call
 * is made for its bounds assert. Confirmed: tag_get(0x77656170, *(int *)object)
 * then tag_block_get_element(tag+0x4fc, trigger_index, 0x114) — the trigger
 *   definition block (stride 0x114), same block used by weapon_reset_state.
 * Confirmed: FLD [def+0x4c] / FMUL [0x253394] / CALL _ftol2 at 0xfcd4c-0xfcd55;
 *   the int result is spilled to [EBP-4] and only its low word is stored
 *   (MOV DX,word ptr [EBP-4] at 0xfcd95).
 * Confirmed: a SECOND object_get_and_verify_type(weapon_handle, 4) at 0xfcd60
 *   supplies the base for the store, and it happens BEFORE the bounds check.
 * Confirmed: bounds test TEST SI,SI / JL and CMP SI,0x2 / JL; failure path is
 *   display_assert(..., weapons.c, 0xa11, 1) then system_exit(-1).
 * Confirmed: LEA ECX,[EDI+EDI*8] / LEA EAX,[EBX+ECX*4] => weapon_data +
 *   trigger_index*36; stores byte [+0x211] = 3 and word [+0x212] = counter.
 * Confirmed: weapon_set_animation_state(weapon_handle, 1,
 *   trigger_index+7 @<bx>) (LEA EBX,[ESI+0x7] at 0xfcd9f), then
 *   first_person_weapon_message_from_weapon(weapon_handle, 0xe).
 * Unknown: the semantic meaning of trigger state 3 and of trigger_def+0x4c
 *   (a duration in seconds); raw offsets retained.
 */
void weapon_trigger_fully_charged(int16_t trigger_index, int weapon_handle)
{
  char *weapon_obj;
  char *tag_data;
  char *trigger_def;
  char *weapon_data;
  char *trigger_entry;
  int16_t animation_state;
  int counter;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  weapon_trigger_get((void *)weapon_obj, trigger_index);

  tag_data = (char *)tag_get(0x77656170, *(int *)weapon_obj);
  trigger_def =
    (char *)tag_block_get_element(tag_data + 0x4fc, (int)trigger_index, 0x114);
  counter = (int)(*(float *)(trigger_def + 0x4c) * TICKS_PER_SECOND);

  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
  /* Hoisted: LEA EBX,[ESI+0x7] is a pure computation on the register-held
   * trigger_index; computing it before the bounds check matches the
   * reference's register allocation without changing behaviour. */
  animation_state = (int16_t)(trigger_index + 7);

  if (trigger_index < 0 || trigger_index >= 2) {
    display_assert("trigger_index>=0 && "
                   "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                   "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11, 1);
    system_exit(-1);
  }

  trigger_entry = weapon_data + (int)trigger_index * 36 + 0x210;
  *(char *)(trigger_entry + 1) = 3;
  *(int16_t *)(trigger_entry + 2) = (int16_t)counter;

  weapon_set_state(weapon_handle, 1, animation_state);
  first_person_weapon_message_from_weapon(weapon_handle, 0xe);
}

/* weapon_trigger_idle — return one weapon trigger to the idle state (0xfcdd0).
 *
 * Name evidence (T2, trigger state machine): sets state 0 with a zero
 *   tick counter. State 0 is weapon_update's not-firing branch (it just
 *   increments a saturating idle counter at trigger+0x00). This is the
 *   state weapon_trigger_recover (0xfcec0) and the state 7/8 exits fall
 *   back to.
 *
 * Confirmed: both args are register args and there are no stack args —
 *   MOV EBX,ECX at 0xfcdd3 (weapon_handle in ECX) and MOV ESI,EAX at 0xfcdd8
 *   (trigger_index in AX); the function ends in a bare RET.
 * Confirmed: object_get_and_verify_type(weapon_handle, 4) at 0xfcdda, result
 *   kept in EDI, then weapon_trigger_get(EDI=object, SI=trigger_index) at
 * 0xfcde1 — its return value is discarded (EAX is reloaded with MOV EAX,[EDI]);
 * the call is made for its bounds assert, same as in
 * weapon_trigger_fully_charged. Confirmed: tag_get(0x77656170, *(int *)object)
 * then tag_block_get_element(tag+0x4fc, trigger_index, 0x114) at 0xfce02 — the
 *   trigger definition block (stride 0x114); its result is discarded here.
 * Confirmed: MOVSX EDI,SI at 0xfcdf3 supplies the sign-extended index to both
 *   tag_block_get_element and the entry-address LEA.
 * Confirmed: a SECOND object_get_and_verify_type(weapon_handle, 4) at 0xfce0a
 *   supplies the store base (EBX), and it happens BEFORE the bounds check;
 *   ADD ESP,0x24 at 0xfce0f is the coalesced cdecl cleanup for all 9 pushed
 *   dwords, not a 9-argument call.
 * Confirmed: bounds test TEST SI,SI / JL and CMP SI,0x2 / JL; failure path is
 *   display_assert(..., weapons.c, 0xa11, 1) then system_exit(-1).
 * Confirmed: LEA ECX,[EDI+EDI*8] / LEA EAX,[EBX+ECX*4] => weapon_data +
 *   trigger_index*36; stores byte [+0x211] = 0 then word [+0x212] = 0.
 * Unknown: the semantic meaning of trigger state 0
 * (weapon_trigger_fully_charged uses 3, weapon_trigger_locked uses 7,
 * weapon_reset_state uses 8); raw offsets retained.
 */
void weapon_trigger_idle(int16_t trigger_index, int weapon_handle)
{
  char *weapon_obj;
  char *tag_data;
  char *weapon_data;
  char *trigger_entry;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  weapon_trigger_get((void *)weapon_obj, trigger_index);

  tag_data = (char *)tag_get(0x77656170, *(int *)weapon_obj);
  tag_block_get_element(tag_data + 0x4fc, (int)trigger_index, 0x114);

  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);

  if (trigger_index < 0 || trigger_index >= 2) {
    display_assert("trigger_index>=0 && "
                   "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                   "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11, 1);
    system_exit(-1);
  }

  trigger_entry = weapon_data + (int)trigger_index * 36 + 0x210;
  *(char *)(trigger_entry + 1) = 0;
  *(int16_t *)(trigger_entry + 2) = 0;
}

/* weapon_trigger_locked — lock a trigger out until it is released (0xfce60).
 *
 * Name evidence (T2, trigger state machine): sets state 7 with a tick
 *   counter of -1, so the counter never expires. weapon_update's state 7
 *   arm has exactly one exit — weapon_trigger_idle, taken only once the
 *   trigger is no longer held. Entered from state 4 when trigger
 *   definition flag 0x8 is set. The observable contract is a hold-time
 *   lockout.
 *
 * Confirmed: weapon_handle in EAX, trigger_index in SI (register args, no
 * stack args; RET with no immediate).
 * Confirmed: CALL object_get_and_verify_type(weapon_handle, 4) happens BEFORE
 * the bounds check (PUSH 4 / PUSH EAX / CALL / ADD ESP,8, result kept in EDI).
 * Confirmed: bounds test is TEST SI,SI / JL and CMP SI,0x2 / JL — the literal
 * 2 is MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON, not a tag count.
 * Confirmed: failure path is display_assert(..., weapons.c, 0xa11, 1) then
 * system_exit(-1) (PUSH -1).
 * Confirmed: entry address is MOVSX EAX,SI / LEA ECX,[EAX+EAX*8] /
 * LEA EAX,[EDI+ECX*4] — i.e. weapon_data + trigger_index*36 + 0x210 base.
 * Confirmed: stores byte [entry+0x211] = 7 and word [entry+0x212] = 0xffff.
 * Unknown: the semantic meaning of trigger state 7 (weapon_reset_state uses 8
 * for the idle/reset state).
 */
void weapon_trigger_locked(int weapon_handle, int16_t trigger_index)
{
  char *weapon_data;
  char *trigger_entry;

  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);

  if (trigger_index < 0 || trigger_index >= 2) {
    display_assert("trigger_index>=0 && "
                   "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                   "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11, 1);
    system_exit(-1);
  }

  trigger_entry = weapon_data + (int)trigger_index * 36 + 0x210;
  *(char *)(trigger_entry + 1) = 7;
  *(int16_t *)(trigger_entry + 2) = -1;
}

/* 0xfcec0 — weapon_trigger_recover
 *
 * Confirmed: EAX=trigger_index (copied to ESI, used as SI / MOVSX ECX,SI),
 * EBX=weapon_handle.
 * Confirmed call order: object_get_and_verify_type(handle, 4) ->
 * weapon_trigger_get(EDI=object, SI=index) -> tag_get('weap', *object) ->
 * tag_block_get_element(tag + 0x4fc, (short)index, 0x114) (result unused) ->
 * byte [trigger + 0] = 0 -> weapon_trigger_idle(AX=index, ECX=handle).
 */
void weapon_trigger_recover(int trigger_index, int weapon_handle)
{
  void *weapon_data;
  char *trigger;
  char *weapon_tag;

  weapon_data = object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon_data, (int16_t)trigger_index);
  weapon_tag = (char *)tag_get(0x77656170, *(int *)weapon_data);
  tag_block_get_element(weapon_tag + 0x4fc, (int16_t)trigger_index, 0x114);
  *trigger = 0;
  weapon_trigger_idle((int16_t)trigger_index, weapon_handle);
}

/* 0xfcf20 — weapon_reset
 *
 * Resets all trigger and magazine states on a weapon. Iterates over
 * trigger entries and sets each trigger state byte to 8 and counter
 * to 0. Then iterates over magazine entries, checks for auto-reload
 * condition, and resets magazine state/counter fields.
 *
 * Confirmed: cdecl, 1 stack arg (weapon_handle).
 * Confirmed: CALL object_get_and_verify_type(weapon_handle, 4).
 * Confirmed: CALL tag_get(0x77656170, *obj) for weapon tag.
 * Confirmed: trigger loop: tag+0x4fc count, stride 0x24 (36 bytes)
 *   per trigger entry in weapon data starting at +0x210.
 * Confirmed: assert at weapons.c:0x667 for trigger_index bounds.
 * Confirmed: tag_block_get_element(tag+0x4fc, index, 0x114).
 * Confirmed: stores +0x211 = 8, +0x212 = 0 per trigger.
 * Confirmed: magazine loop: tag+0x4f0 count, stride 12 bytes per
 *   magazine entry starting at weapon_data + 0x258.
 * Confirmed: assert at weapons.c:0x672 for magazine_index bounds.
 * Confirmed: tag_block_get_element(tag+0x4f0, index, 0x70).
 * Confirmed: auto-reload check: magazine[0]==1 and frame*2 < count.
 * Confirmed: calls weapon_get_first_person_animation_time(handle, 0, 7, -1).
 * Confirmed: calls weapon_magazine_finish_reload(handle, magazine_index).
 * Confirmed: resets magazine[0] and magazine[1] to 0.
 */
void weapon_reset(int weapon_handle)
{
  uint32_t *weapon_data =
    (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  int weap_tag = (int)tag_get(0x77656170, weapon_data[0]);
  int magazine_int;
  int mag_tag_ptr;

  /* Reset trigger states */
  int16_t trigger_index = 0;
  if (0 < *(int *)(weap_tag + 0x4fc)) {
    int trigger_count_index = 0;
    do {
      int weap_tag2 = (int)tag_get(0x77656170, weapon_data[0]);
      char *trigger_entry;
      if (trigger_index < 0 ||
          trigger_count_index >= *(int *)(weap_tag2 + 0x4fc)) {
        display_assert("trigger_index>=0 && trigger_index<weapon_definition->"
                       "weapon.triggers.count",
                       "c:\\halo\\SOURCE\\items\\weapons.c", 0x667, 1);
        system_exit(-1);
      }

      /* Compute trigger entry pointer:
       * base + trigger_index * 9 * 4 + 0x210 */
      trigger_entry = (char *)weapon_data + trigger_count_index * 36 + 0x210;

      tag_block_get_element((void *)(weap_tag + 0x4fc), trigger_count_index,
                            0x114);

      trigger_index = trigger_index + 1;
      trigger_count_index = (int)trigger_index;

      *(char *)(trigger_entry + 1) = 8;
      *(int16_t *)(trigger_entry + 2) = 0;
    } while (trigger_count_index < *(int *)(weap_tag + 0x4fc));
  }

  /* Reset magazine states */
  magazine_int = 0;
  mag_tag_ptr = weap_tag + 0x4f0;
  if (0 < *(int *)(weap_tag + 0x4f0)) {
    int mag_count_index = 0;
    do {
      int weap_tag3 = (int)tag_get(0x77656170, weapon_data[0]);
      int16_t *mag_entry;
      if ((int16_t)magazine_int < 0 ||
          mag_count_index >= *(int *)(weap_tag3 + 0x4f0)) {
        display_assert("magazine_index>=0 && magazine_index<weapon_definition->"
                       "weapon.magazines.count",
                       "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
        system_exit(-1);
      }

      /* Compute magazine entry pointer:
       * base + (magazine_index * 3 + 0x96) * 4 */
      mag_entry =
        (int16_t *)((char *)weapon_data + (mag_count_index * 3 + 0x96) * 4);

      tag_block_get_element((void *)mag_tag_ptr, mag_count_index, 0x70);

      if (mag_entry[0] == 1) {
        int16_t frame =
          weapon_get_first_person_animation_time(weapon_handle, 0, 7, -1);
        if (mag_entry[1] * 2 < (int)frame) {
          weapon_magazine_finish_reload(weapon_handle, magazine_int);
        }
      }

      magazine_int = magazine_int + 1;
      mag_count_index = (int)(int16_t)magazine_int;
      mag_entry[0] = 0;
      mag_entry[1] = 0;
    } while (mag_count_index < *(int *)(mag_tag_ptr));
  }
}

#include "x87_math.h"

/* 0xfd0b0 — projectile_distribute
 *
 * Confirmed from disassembly: the index arrives in AX (TEST AX,AX at 0xfd0bc,
 *   MOV DX,AX at 0xfd0fa with no prior definition), five cdecl stack args at
 *   [EBP+0x8..0x18]; single caller FUN_000fd570 at 0xfdb9f.
 * Confirmed: bit 0 of the byte at [EBP+0x18] selects the offset scheme:
 *   set   -> index 0 gives 0.0f (0x2533c0); else t = index-1, offset is
 *            (short)(t>>1) when t is odd, (short)-(t>>1) when t is even.
 *   clear -> offset = (float)(short)(index>>1) - 0.5f (0x253398), negated
 *            (FCHS) when index is odd.
 * Confirmed: offset is multiplied by the float at [EBP+0x14] before the
 *   short at [EBP+0x10] is compared with 1 (MOVSX/DEC/JNZ at 0xfd115-0xfd11d).
 * Confirmed: when it equals 1, FCOS then FSIN of the product; CALL
 *   rotate_vector3d_by_sincos([EBP+0x8], [EBP+0xc], sin, cos) at 0xfd137.
 * Unknown: meaning of param_4 values other than 1 and of other flag bits.
 */
void projectile_distribute(int16_t projectile_index, float *vector, float *axis,
                           int16_t param_4, float angle, uint8_t flags)
{
  float offset;
  int16_t prior_index;

  if (flags & 1) {
    if (projectile_index == 0) {
      offset = 0.0f;
    } else {
      prior_index = (int16_t)(projectile_index - 1);
      if (prior_index & 1) {
        offset = (float)(int16_t)(prior_index >> 1);
      } else {
        offset = (float)(int16_t) - (prior_index >> 1);
      }
    }
  } else {
    offset = (float)(int16_t)(projectile_index >> 1) - 0.5f;
    if (projectile_index & 1) {
      offset = -offset;
    }
  }
  offset = offset * angle;
  if (param_4 == 1) {
    rotate_vector3d_by_sincos(vector, axis, x87_fsin(offset), x87_fcos(offset));
  }
}

/* weapon_state_next (0xfd150)
 *
 * Name evidence (T2, animation-event pairing): weapon_update (0xfe910)
 *   calls animation_update_internal and dispatches result 1 to
 *   weapon_state_key_frame (0xfb990) and result 2 — animation finished —
 *   here. This picks the state that follows: everything returns to
 *   animation state 0 (idle) except the holding/looping states 7, 8
 *   ("charged-1"/"charged-2") and 10.
 *
 * Confirmed from disassembly at 0xfd150: the weapon handle arrives in ESI
 * (PUSH ESI at 0xfd152 with no prior definition in the function), so the
 * kb.json decl carries "@<esi>" on the first parameter.
 *
 * Reads the weapon animation-state byte at weapon+0x1e8 and, unless that
 * state is 7, 8 or 10, forces animation state 0 via
 * weapon_set_animation_state(handle, 1, 0). The third argument travels in
 * BX (XOR EBX,EBX at 0xfd171), matching that callee's "@<bx>" annotation.
 * Compares are signed (JL/JLE), so the state is read as a signed char.
 */
void weapon_state_next(int weapon_handle)
{
  char animation_state;

  animation_state =
    *(char *)((int)object_get_and_verify_type(weapon_handle, 4) + 0x1e8);

  if ((animation_state < 7) ||
      ((animation_state > 8) && (animation_state != 10))) {
    weapon_set_state(weapon_handle, 1, 0);
  }
}

/* weapon_set_current_amount (0xfd180)
 *
 * Sets weapon ammo level based on a fraction. For battery-based weapons
 * (no magazines or has triggers with charging threshold), stores the charge
 * level at weapon+0x1f0. For magazine-based weapons, sets the loaded rounds
 * in the first magazine and adjusts the total accordingly.
 */
void weapon_set_current_amount(int weapon_handle, float ammo_fraction)
{
  int *weapon = (int *)object_get_and_verify_type(weapon_handle, 4);
  int tag_data = (int)tag_get(0x77656170, *weapon);

  bool is_battery = false;
  int magazine_count = *(int *)(tag_data + 0x4f0);

  if (magazine_count == 0) {
    is_battery = true;
  } else {
    int16_t i;
    int trigger_count = *(int *)(tag_data + 0x4fc);
    for (i = 0; (int)i < trigger_count; i++) {
      char *trigger =
        (char *)tag_block_get_element((int *)(tag_data + 0x4fc), (int)i, 0x114);
      if (*(float *)(trigger + 0xbc) > 0.0f) {
        is_battery = true;
        break;
      }
    }
  }

  if (ammo_fraction < 0.0f) {
    ammo_fraction = 0.0f;
  } else if (ammo_fraction > 1.0f) {
    ammo_fraction = 1.0f;
  }

  if (is_battery) {
    *(float *)((char *)weapon + 0x1f0) = 1.0f - ammo_fraction;
    return;
  }

  if (magazine_count > 0) {
    char *mag_def =
      (char *)tag_block_get_element((int *)(tag_data + 0x4f0), 0, 0x70);
    int check_tag = (int)tag_get(0x77656170, *weapon);
    int16_t max_rounds;
    int16_t new_loaded;
    int16_t current_loaded;
    if (*(int *)(check_tag + 0x4f0) < 1) {
      display_assert("magazine_index>=0 && "
                     "magazine_index<weapon_definition->weapon.magazines.count",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
      system_exit(-1);
    }

    max_rounds = *(int16_t *)(mag_def + 0xa);
    new_loaded = (int16_t)(int)((float)max_rounds * ammo_fraction);
    current_loaded = *(int16_t *)((char *)weapon + 0x260);
    *(int16_t *)((char *)weapon + 0x260) = new_loaded;
    *(int16_t *)((char *)weapon + 0x25e) += (new_loaded - current_loaded);
  }
}

/* weapon_ready — no binary address assigned.
 * Initializes a weapon after it becomes the active weapon for a unit.
 * Resets trigger/magazine state, sets the ready animation (state 9),
 * fires the initial effect from the weapon triggers tag block, and
 * stores the ready animation frame count into the weapon data. */
void weapon_ready(int weapon_handle)
{
  uint32_t *weapon_data =
    (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  int tag_data = (int)tag_get(0x77656170, weapon_data[0]);
  int16_t frame;

  weapon_reset(weapon_handle);
  weapon_set_state(weapon_handle, 1, 9);
  first_person_weapon_message_from_weapon(weapon_handle, 0xc);
  weapon_effect_new(*(int *)(tag_data + 0x348), 0, 0, weapon_handle);

  frame = weapon_get_first_person_animation_time(weapon_handle, 0, 10, -1);
  *(int16_t *)((int)weapon_data + 0x1ea) = frame;
}

/* weapon_put_away — no binary address assigned.
 * Attempts to place (holster/put-away) the current weapon. If flag is
 * zero and the weapon has active triggers or animations, the placement
 * is rejected. On success, sets the put-away animation (state 10),
 * resets trigger/magazine state, disposes any attached effect, and
 * starts the put-away effect sequence. */
bool weapon_put_away(int weapon_handle, int flag)
{
  volatile char result;
  uint32_t *weapon_data;

  weapon_data = (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  tag_get(0x77656170, weapon_data[0]);

  result = 0;
  if ((char)flag != 0 || !(char)weapon_busy(weapon_handle)) {
    if ((char)weapon_set_state(weapon_handle, flag, 10)) {
      *(int16_t *)((int)weapon_data + 0x1e0) = 0;
      weapon_reset(weapon_handle);

      if (*(int *)((int)weapon_data + 0x274) != -1) {
        effect_delete(*(int *)((int)weapon_data + 0x274));
        *(int *)((int)weapon_data + 0x274) = -1;
      }

      first_person_weapon_message_from_weapon(weapon_handle, 0xb);
      return true;
    }
  }

  return result;
}

/* weapon_aim (0xfd400) — weapon_try_and_fire_projectile
 *
 * Attempts to fire a projectile for a given weapon trigger. Validates the
 * weapon object (type 4), resolves the 'weap' tag, then validates the
 * trigger_index is in range [0, triggers.count). If in range, fetches the
 * trigger block element (0x114 bytes each) at [weap_tag+0x4fc], reads the
 * 'proj' tag reference from [trigger_elem+0xa0], and resolves it via
 * tag_get('proj',...). Passes the resolved projectile tag along with the
 * remaining parameters to projectile_aim (projectile fire dispatcher), then
 * validates param_6 as a valid 3D unit normal using valid_real_normal3d. If
 * the vector is invalid, formats an assert message via csprintf and calls
 * display_assert + system_exit. Returns true (1) on success, false (0) if
 * trigger_index is out of range.
 *
 * Line number evidence: assert at line 0x515 (1301) in weapons.c.
 *
 * Confirmed: object_get_and_verify_type(weapon_handle, 4) at 0xfd40c.
 * Confirmed: tag_get(0x77656170, weapon_data[0]) at 0xfd41b; ADD ESP,0x10
 *   cleans both preceding calls (object_get + tag_get = 4 args).
 * Confirmed: TEST SI,SI / JL at 0xfd42a–0xfd42d guards trigger_index < 0.
 * Confirmed: MOV EDX,[EBX+0x4fc] / ADD EBX,0x4fc at 0xfd433–fd439 =
 *   trigger block count and pointer.
 * Confirmed: MOVSX ECX,SI / CMP ECX,EDX / JGE at 0xfd43f–0xfd447 guards
 *   trigger_index >= triggers.count.
 * Confirmed: CALL 0xfb320 at 0xfd44d is a debug assertion using caller ESI/EDI;
 *   not representable as a plain C call — elided (bounds already checked
 * above). Confirmed: tag_block_get_element(EBX, ECX, 0x114) at 0xfd45c where
 * EBX= &triggers_block ([weap_tag+0x4fc] after ADD EBX,0x4fc). Confirmed: MOV
 * EAX,[EAX+0xa0] at 0xfd475 reads proj tag reference from trigger element.
 * Confirmed: tag_get(0x70726f6a, proj_ref, ...) at 0xfd496 with 14 pushes;
 *   ADD ESP,0x8 cleans only tag_get's own 2 args.
 * Confirmed: projectile_aim called with proj_tag + 12 stale stack args at
 * 0xfd49f; receives proj_tag as arg1, then param_3..param_9 interleaved with
 * 0-padding. Confirmed: valid_real_normal3d(param_6) at 0xfd4a5 (PUSH ESI where
 * ESI=[EBP+0x1c]). Confirmed: csprintf(&DAT_005ab100, ...) assert-format at
 * 0xfd4e2; float args loaded FLD+FSTP double via MSVC push-then-fstp pattern.
 * Confirmed: CALL display_assert at 0xfd4eb; CALL system_exit at 0xfd4f2.
 * Confirmed: return 1 at 0xfd4fa; return 0 (XOR AL,AL) at 0xfd428 on early
 * exit.
 */
bool weapon_aim(int weapon_handle, int16_t trigger_index, void *param_3,
                void *param_4, int param_5, float *param_6, int param_7,
                void *param_8, void *param_9)
{
  int *weapon_data = (int *)object_get_and_verify_type(weapon_handle, 4);
  char *weap_tag = (char *)tag_get(0x77656170, weapon_data[0]);
  int trigger_count;
  int trig_idx;
  char *trigger_elem;
  int proj_ref;
  void *proj_tag;

  if (trigger_index < 0)
    return false;

  trigger_count = *(int *)(weap_tag + 0x4fc);
  trig_idx = (int)trigger_index;
  if (trig_idx >= trigger_count)
    return false;

  /* weapon_trigger_get assertion (trigger bounds) elided: reads caller ESI/EDI,
   * not representable as a plain C call. Bounds already checked above. */

  trigger_elem =
    (char *)tag_block_get_element((void *)(weap_tag + 0x4fc), trig_idx, 0x114);
  proj_ref = *(int *)(trigger_elem + 0xa0);

  /* tag_get + projectile_aim share a single stack cleanup.
   * Disassembly-verified push sequence at 0xfd46d–0xfd495 (14 pushes,
   * right-to-left): [P1] ECX=param_9, [P2] EDX=param_8, [P3] ECX=param_7, [P4]
   * 0, [P5] ESI=param_6, [P6] EDX=param_5, [P7-P10] 0,0,0,0, [P11] ECX=param_4
   * (ECX reloaded via MOV ECX,[EBP+0x14] at 0xfd47c), [P12] EDX=param_3, [P13]
   * EAX=proj_ref, [P14] 'proj' tag_get (at 0xfd496) uses P14+'proj' and
   * P13=proj_ref; ADD ESP,8 cleans them. PUSH EAX (proj_tag) at 0xfd49e, then
   * CALL projectile_aim at 0xfd49f: arg2=P12=param_3(origin),
   * arg3=P11=param_4(target), arg8=P6=param_5, arg9=P5=param_6(aim_vector),
   * arg11=P3=param_7, arg12=P2=param_8, arg13=P1=param_9. */
  proj_tag = tag_get(0x70726f6a, proj_ref);
  ((void (*)(void *, void *, void *, int, int, int, int, int, float *, int, int,
             void *, void *))0xf84d0)(proj_tag, param_3, param_4, 0, 0, 0, 0,
                                      param_5, param_6, 0, param_7, param_8,
                                      param_9);

  if (!valid_real_normal3d(param_6)) {
    display_assert(csprintf((char *)0x5ab100,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "result_aim_vector", (double)param_6[0],
                            (double)param_6[1], (double)param_6[2]),
                   "c:\\halo\\SOURCE\\items\\weapons.c", 0x515, 1);
    system_exit(-1);
  }

  return true;
}

/* 0xfd510 — weapon_stop_reload
 *
 * Confirmed from disassembly at 0xfd510: the whole body is
 *   PUSH EBP; MOV EBP,ESP; POP EBP; JMP 0xfcf20
 * i.e. a cdecl frame set up and torn down, then an unconditional tail-call to
 * weapon_reset_state(0xfcf20) with the incoming stack argument untouched.
 * No other callee, no struct access, no side effect of its own.
 *
 * Unknown: why the wrapper exists separately from weapon_reset_state; the name
 * comes from the kb.json decl, not from a string in this function.
 */
void weapon_stop_reload(int weapon_handle)
{
  weapon_reset(weapon_handle);
}

/* 0xfd520 — weapon_trigger_finish_tracking
 * Confirmed: weapon_handle in ECX, trigger_index in EAX. */
void weapon_trigger_finish_tracking(int weapon_handle, int trigger_index)
{
  char *weapon;
  char *tag;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  (void)weapon_trigger_get(weapon, (int16_t)trigger_index); /* hazard-ok: intentional-discard */
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  tag_block_get_element((char *)tag + 0x4fc, (int16_t)trigger_index, 0x114);

  *(int *)(weapon + 0x200) = -1;
  weapon_trigger_recover(trigger_index, weapon_handle);
}

/* 0xfd570 — trigger_create_projectiles
 *
 * Rewritten 2026-09-29 against the 0xfd570-0xfdc8e disassembly; the batch-4
 * lift read the wrong marker fields, wrong seat-unit field, wrong aim-call
 * arguments and swapped projectile_distribute arguments.
 *
 * Confirmed: markers come from object_get_marker_by_name into a 64 x 0x6c
 *   buffer (EBP-0x1c20); per marker the origin is matrix_position (+0x60) and
 *   the forward vector is matrix_forward (+0x3c) (0xfd66e-0xfd6a1, 0xfd8e8).
 * Confirmed: a zero marker count becomes 1, and trigger-definition flag 0x20
 *   clear forces 1 (0xfd648-0xfd65f).
 * Confirmed: the seated/vehicle unit is unit+0x2d8 (0xfd705, 0xfd75c); its
 *   player (+0x1c8) and actor (+0x1a4) replace the parent's.
 * Confirmed: unit_adjust_projectile_ray writes a scalar speed (EBP-0x40)
 *   which later scales placement+0x28 (0xfdba4-0xfdbd3).
 * Confirmed: player path builds left = up x forward (global_up_vector_ptr),
 *   falls back to global_left_vector_ptr when normalize3d returns 0.0, builds
 *   up = forward x left, offsets origin by trig_def+0x84/+0x88/+0x8c along
 *   forward/left/up, then CALL player_aim_projectile(player, origin, forward)
 *   (0xfd795-0xfd8bf; the second normalize3d argument stays on the stack and
 *   is removed by the shared ADD ESP,0x10 at 0xfd8dd).
 * Confirmed: actor path is actor_aim_projectile(actor, origin, forward,
 *   &error_angle) (0xfd8cb-0xfd8d8); the callee stores a float there.
 * Confirmed: object_placement_data_new receives trig_def+0xa0 on every
 *   projectile, even when the charged-shot path took trigger 1's projectile
 *   tag for the -1 test (0xfd9d6).
 * Confirmed: error_angle persists across projectiles; it is only computed
 *   while it is still 0.0 (0xfda5d-0xfda94).
 * Confirmed: cone arg 3 is trig_def+0x78 (0xfdaaa), not 0.0.
 * Confirmed: trig_def flag 0x1000 makes every projectile reuse projectile 0's
 *   forward vector (0xfdace-0xfdb13).
 * Confirmed: projectile_distribute(index@ax, forward, up, word trig_def+0x6c,
 *   float trig_def+0x70, projectile count) (0xfdb80-0xfdb9f).
 * Confirmed: placement flag 2 is set only when the parent unit has a player
 *   (+0x1c8 != -1); those projectiles are then moved to the unit camera.
 * Uncertain: meaning of placement+0x04 bit 2 and of placement+0x28 (written
 *   as forward * speed, so likely a velocity).
 */
void trigger_create_projectiles(int weapon_handle, int16_t trigger_index)
{
  char *weapon;
  char *trigger;
  char *weapon_tag;
  char *trig_def;
  char *w;
  char *unit;
  char *unit_tag;
  char *seat_unit;
  char *owner_unit;
  char *trigger1_def;
  const char *marker_names[2];
  object_marker markers[64];
  object_marker *marker;
  int16_t marker_count;
  unsigned int markers_left;
  int marker_object;
  int parent_unit_handle;
  int player_index;
  int actor_index;
  int target_object;
  int owner;
  int projectile_tag_index;
  int projectile_count;
  int projectile;
  int16_t i;
  uint16_t charge;
  char adjust_origin;
  char use_unit_forward;
  char tracer;
  char place_at_camera;
  float origin[3];
  float forward[3];
  float left[3];
  float up[3];
  float first_forward[3];
  float camera_position[3];
  float *offset;
  float x;
  float y;
  float speed;
  float error_angle;
  float spread;
  float len;
  float inv_len;
  object_placement_data placement;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon, trigger_index);
  weapon_tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  trig_def = (char *)tag_block_get_element(weapon_tag + 0x4fc, (int)trigger_index, 0x114);

  w = (char *)object_get_and_verify_type(weapon_handle, 4);
  parent_unit_handle = -1;
  if (*(int *)(w + 0xcc) != -1 &&
      object_try_and_get_and_verify_type(*(int *)(w + 0xcc), 3) != NULL) {
    parent_unit_handle = *(int *)(w + 0xcc);
  }

  marker_names[0] = "primary trigger";   /* 0x267238 */
  marker_names[1] = "secondary trigger"; /* 0x28af04 */
  w = (char *)object_get_and_verify_type(weapon_handle, 4);
  marker_object = weapon_handle;
  if ((*(uint8_t *)(w + 4) & 1) && *(int *)(w + 0xcc) != -1) {
    marker_object = *(int *)(w + 0xcc);
  }
  marker_count = object_get_marker_by_name(marker_object, (void *)marker_names[trigger_index],
                                           markers, 64);
  if (marker_count == 0) {
    marker_count = 1;
  }
  if (!(*(uint8_t *)trig_def & 0x20)) {
    marker_count = 1;
  }
  if (marker_count <= 0) {
    return;
  }

  marker = markers;
  markers_left = (uint16_t)marker_count;
  do {
    origin[0] = marker->matrix.position.x;
    origin[1] = marker->matrix.position.y;
    origin[2] = marker->matrix.position.z;
    forward[0] = marker->matrix.forward.x;
    forward[1] = marker->matrix.forward.y;
    forward[2] = marker->matrix.forward.z;
    speed = 0.0f;
    error_angle = 0.0f;

    unit = (char *)object_try_and_get_and_verify_type(parent_unit_handle, 3);
    target_object = -1;
    if (!(*(uint32_t *)trig_def & 0x800) && unit != NULL && !(*(uint8_t *)(unit + 0xb6) & 4)) {
      unit_tag = (char *)tag_get(0x756e6974, *(uint32_t *)unit);
      player_index = *(int *)(unit + 0x1c8);
      actor_index = *(int *)(unit + 0x1a4);
      if (*(int *)(unit + 0x2d8) != -1) {
        seat_unit = (char *)object_get_and_verify_type(*(int *)(unit + 0x2d8), 3);
        player_index = *(int *)(seat_unit + 0x1c8);
        actor_index = *(int *)(seat_unit + 0x1a4);
      }
      adjust_origin = (char)((*(uint32_t *)(unit_tag + 0x17c) >> 3) & 1);
      use_unit_forward = 1;
      if (actor_index != -1 && actor_firing_blindly(actor_index)) {
        use_unit_forward = 0;
      }
      if (*(int *)(unit + 0x2d8) != -1) {
        use_unit_forward = 0;
      }
      unit_adjust_projectile_ray(parent_unit_handle, origin, forward, &speed,
                                 adjust_origin, use_unit_forward);

      if (player_index != -1) {
        left[0] = forward[2] * global_up_vector_ptr[1] - forward[1] * global_up_vector_ptr[2];
        left[1] = forward[0] * global_up_vector_ptr[2] - forward[2] * global_up_vector_ptr[0];
        left[2] = forward[1] * global_up_vector_ptr[0] - forward[0] * global_up_vector_ptr[1];
        if (normalize3d(left) == 0.0f) {
          left[0] = global_left_vector_ptr[0];
          left[1] = global_left_vector_ptr[1];
          left[2] = global_left_vector_ptr[2];
        }
        up[0] = left[2] * forward[1] - left[1] * forward[2];
        up[1] = forward[2] * left[0] - left[2] * forward[0];
        up[2] = left[1] * forward[0] - forward[1] * left[0];
        normalize3d(up);

        /* origin += forward*o[0] + left*o[1] + up*o[2], same per-axis
         * accumulation order as 0xfd839-0xfd8bc. */
        offset = (float *)(trig_def + 0x84);
        x = forward[0] * offset[0] + origin[0];
        origin[1] = forward[1] * offset[0] + origin[1];
        origin[2] = forward[2] * offset[0] + origin[2];
        x = left[0] * offset[1] + x;
        y = left[1] * offset[1] + origin[1];
        origin[2] = left[2] * offset[1] + origin[2];
        origin[0] = up[0] * offset[2] + x;
        origin[1] = up[1] * offset[2] + y;
        origin[2] = up[2] * offset[2] + origin[2];

        target_object = player_aim_projectile(player_index, origin, forward);
      } else if (actor_index != -1) {
        target_object = actor_aim_projectile(actor_index, origin, forward, &error_angle);
      }
    }

    if (*(uint8_t *)trig_def & 0x20) {
      origin[0] = marker->matrix.position.x;
      origin[1] = marker->matrix.position.y;
      origin[2] = marker->matrix.position.z;
    }

    if (trigger_index == 0 && *(int16_t *)(weapon + 0x20c) > 0) {
      trigger1_def = (char *)tag_block_get_element(weapon_tag + 0x4fc, 1, 0x114);
      projectile_tag_index = *(int *)(trigger1_def + 0xa0);
      charge = *(uint16_t *)(weapon + 0x20c);
      if (*(int16_t *)(weapon_tag + 0x32c) == 4) {
        charge++;
      }
      projectile_count = (uint16_t)(*(uint16_t *)(trig_def + 0x6e) * charge);
      *(int16_t *)(weapon + 0x20c) = 0;
    } else {
      projectile_count = *(int16_t *)(trig_def + 0x6e);
      projectile_tag_index = *(int *)(trig_def + 0xa0);
    }

    if (projectile_tag_index != -1) {
      w = (char *)object_get_and_verify_type(weapon_handle, 4);
      owner = -1;
      if (*(int *)(w + 0xcc) != -1) {
        owner_unit = (char *)object_try_and_get_and_verify_type(*(int *)(w + 0xcc), 3);
        if (owner_unit != NULL) {
          owner = *(int *)(w + 0xcc);
          if (*(int *)(owner_unit + 0x2d8) != -1) {
            owner = *(int *)(owner_unit + 0x2d8);
          }
        }
      }

      for (i = 0; i < (int16_t)projectile_count; i++) {
        tracer = 0;
        object_placement_data_new(&placement, *(int *)(trig_def + 0xa0), owner);
        placement.position_x = origin[0];
        placement.position_y = origin[1];
        placement.position_z = origin[2];
        placement.forward.x = forward[0];
        placement.forward.y = forward[1];
        placement.forward.z = forward[2];

        if (*(float *)(trigger + 0x10) == 0.0f ||
            *(int16_t *)(trigger + 0xe) >= *(int16_t *)(trig_def + 0x26)) {
          tracer = 1;
          *(int16_t *)(trigger + 0xe) = 0;
        } else {
          (*(int16_t *)(trigger + 0xe))++;
        }

        if (error_angle == 0.0f) {
          spread = (*(uint32_t *)trig_def & 0x200) ? *(float *)(weapon + 0x1e4)
                                                   : *(float *)(trigger + 0x1c);
          error_angle = (1.0f - spread) * *(float *)(trig_def + 0x7c) +
                        spread * *(float *)(trig_def + 0x80);
        }

        if (!(*(uint32_t *)trig_def & 0x400) || !(*(uint8_t *)(weapon + 0x1e0) & 0x40)) {
          /* hazard-ok: in-place cone perturbation, both pointers are &placement.forward at 0xfdaad/0xfdab6 */
          seed_random_vector_in_cone3d(get_global_random_seed_address(), &placement.forward.x,
                                       *(float *)(trig_def + 0x78), error_angle,
                                       &placement.forward.x); /* dup-args-ok */
        }

        if (i == 0) {
          first_forward[0] = placement.forward.x;
          first_forward[1] = placement.forward.y;
          first_forward[2] = placement.forward.z;
        }
        if (*(uint32_t *)trig_def & 0x1000) {
          placement.forward.x = first_forward[0];
          placement.forward.y = first_forward[1];
          placement.forward.z = first_forward[2];
        }

        /* perpendicular3d returns its out pointer; 0xfdb2c normalizes through it. */
        perpendicular3d(&placement.forward.x, &placement.up.x);
        len = sqrtf(placement.up.x * placement.up.x + placement.up.y * placement.up.y +
                    placement.up.z * placement.up.z);
        if (!(fabs((double)len) < 0.0001f)) {
          inv_len = 1.0f / len;
          placement.up.x = inv_len * placement.up.x;
          placement.up.y = inv_len * placement.up.y;
          placement.up.z = inv_len * placement.up.z;
        }

        projectile_distribute(i, &placement.forward.x, &placement.up.x,
                              *(int16_t *)(trig_def + 0x6c), *(float *)(trig_def + 0x70),
                              (uint8_t)projectile_count);

        placement.field_28[0] = placement.forward.x * speed;
        placement.field_28[1] = placement.forward.y * speed;
        placement.field_28[2] = placement.forward.z * speed;

        place_at_camera = 0;
        if (unit != NULL && *(int *)(unit + 0x1c8) != -1) {
          placement.field_04 |= 2;
          place_at_camera = 1;
        }

        projectile = object_new(&placement);
        if (projectile != -1) {
          if (place_at_camera) {
            unit_get_camera_position(parent_unit_handle, camera_position);
            object_try_place(projectile, camera_position);
          }
          if (target_object != -1) {
            projectile_set_target_object_index(projectile, target_object);
          }
          if (!tracer) {
            projectile_kill_tracer(projectile);
          }
        }
      }
    }

    marker++;
  } while (--markers_left);
}

/* 0xfdc90 — weapon_trigger_fire
 *
 * Rewritten 2026-09-29 against the 0xfdc90-0xfe441 disassembly.
 *
 * Confirmed: firing-rate element (0x84 bytes) holds effect/damage tag pairs
 *   per outcome: slot 0 fired (+0x30/+0x60), slot 1 overheated (+0x40/+0x70),
 *   slot 2 not fired (+0x50/+0x80) (0xfe03e-0xfe056).
 * Confirmed: the final weapon_effect_new scale is trigger+0x10 when fired,
 *   1.0 when not fired, 0.0 when the trigger has no firing rates; param_3 is
 *   weapon+0x1ec / tag+0x350 only on a normal shot (0xfdfdc-0xfe03b).
 * Confirmed: the firing-rate search stops when the rolled count is positive
 *   or the index wraps back to the value trigger+0xa held on entry
 *   (EBP-0x24, 0xfde74 / 0xfdf1f).
 * Confirmed: weapon_set_state(handle, 0, state@bx = (trigger_index != 0) + 1)
 *   (0xfe1d5-0xfe1e5).
 * Confirmed: the firing damage (owner != -1 && damage tag != -1) sets damage
 *   flag 8, direction = -(unit+0x1ec..0x1f4), origin = epicenter = unit+0x50
 *   (0xfe219-0xfe287).
 * Confirmed: the overheat-explosion branch deletes the weapon and then falls
 *   through to the trigger-state update and final effect (JMP 0xfe343); it
 *   does not return early.
 */
void weapon_trigger_fire(int weapon_handle, int trigger_index)
{
  char *weapon;
  char *trigger;
  char *weapon_tag;
  char *trig_def;
  char *w;
  char *refetch_trigger;
  char *refetch_trig_def;
  char *magazine_def;
  int16_t *magazine;
  char *rate_elem;
  char *owner;
  int *rates;
  int owner_unit;
  int firing_effect;
  int firing_damage;
  int effect_slot;
  int player_index;
  int16_t rounds;
  int16_t rounds_per_shot;
  int16_t rate;
  int16_t start_rate;
  char fired;
  char overheated;
  char charge_fire;
  float effect_scale;
  float effect_param;
  float t;
  struct damage_data damage;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon, (int16_t)trigger_index);
  weapon_tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  trig_def = (char *)tag_block_get_element(weapon_tag + 0x4fc, (int16_t)trigger_index, 0x114);

  w = (char *)object_get_and_verify_type(weapon_handle, 4);
  owner_unit = -1;
  if (*(int *)(w + 0xcc) != -1 &&
      object_try_and_get_and_verify_type(*(int *)(w + 0xcc), 3) != NULL) {
    owner_unit = *(int *)(w + 0xcc);
  }

  firing_damage = -1;
  firing_effect = -1;
  effect_param = 0.0f;
  effect_scale = 0.0f;
  fired = 0;
  overheated = 0;
  charge_fire = 0;

  if ((int16_t)trigger_index == 1 &&
      (*(int16_t *)(weapon_tag + 0x32c) == 3 || *(int16_t *)(weapon_tag + 0x32c) == 4)) {
    charge_fire = 1;
  }

  if (*(int16_t *)(trig_def + 0x20) != -1) {
    magazine_def = (char *)tag_block_get_element(weapon_tag + 0x4f0, *(int16_t *)(trig_def + 0x20), 0x70);
    magazine = (int16_t *)weapon_magazine_get(weapon, *(int16_t *)(trig_def + 0x20));
    if (!charge_fire || *(int16_t *)(weapon + 0x20c) < *(int16_t *)(weapon_tag + 0x32e)) {
      rounds = magazine[4];
      rounds_per_shot = *(int16_t *)(trig_def + 0x22);
      if ((rounds >= rounds_per_shot || (*(uint8_t *)trig_def & 4)) &&
          (!(*(uint32_t *)(weapon_tag + 0x308) & 0x800) || *(float *)(weapon + 0x1f0) < 1.0f) &&
          (rounds >= *(int16_t *)(trig_def + 0x24) || !(*(uint8_t *)(trigger + 4) & 1))) {
        if (!*(uint8_t *)0x5aa899 &&
            (magazine[4] = (int16_t)(rounds - rounds_per_shot)) <= 0) {
          magazine[4] = 0;
        } else if (*(uint8_t *)magazine_def & 2) {
          magazine[0] = 2;
          magazine[1] = 0;
        }
        fired = 1;
      }
    }
  } else {
    fired = 1;
  }

  if (*(uint8_t *)0x5aa899) {
    fired = 1;
  }

  rates = (int *)(trig_def + 0x108);
  if (*rates > 0) {
    if (*(int16_t *)(trigger + 0xc) <= 0) {
      start_rate = *(int16_t *)(trigger + 0xa);
      rate = start_rate;
      if (*(uint8_t *)trig_def & 2) {
        rate = (int16_t)((int)random_seed_step((unsigned int *)get_global_random_seed_address()) % *rates);
      }
      do {
        if ((int)*(uint16_t *)(trigger + 8) == (1 << *rates) - 1) {
          *(uint16_t *)(trigger + 8) = 0;
        }
        do {
          rate++;
          if (rate >= *rates) {
            rate = 0;
          }
        } while (*(uint16_t *)(trigger + 8) & (1 << rate));

        rate_elem = (char *)tag_block_get_element(rates, rate, 0x84);
        *(int16_t *)(trigger + 0xa) = rate;
        *(uint16_t *)(trigger + 8) |= (uint16_t)(1 << rate);
        *(int16_t *)(trigger + 0xc) = seed_random_range(
          (unsigned int *)get_global_random_seed_address(),
          *(int16_t *)rate_elem, *(int16_t *)(rate_elem + 2));
      } while (*(int16_t *)(trigger + 0xc) <= 0 && rate != start_rate);
    }

    (*(int16_t *)(trigger + 0xc))--;
    rate_elem = (char *)tag_block_get_element(rates, *(int16_t *)(trigger + 0xa), 0x84);

    if (*(float *)(weapon_tag + 0x448) > 0.0f && *(float *)(weapon_tag + 0x448) < 1.0f &&
        *(float *)(weapon + 0x1f0) > *(float *)(weapon_tag + 0x448)) {
      t = ((*(float *)(weapon + 0x1f0) - *(float *)(weapon_tag + 0x448)) *
           *(float *)(weapon_tag + 0x44c)) / (1.0f - *(float *)(weapon_tag + 0x448));
      if (*(char *)(trigger + 1) == 6) {
        t = t + t;
      }
      if (random_math_real((unsigned int *)get_global_random_seed_address()) < t) {
        overheated = 1;
      }
    }

    if (!fired) {
      effect_slot = 2;
      effect_scale = 1.0f;
      effect_param = 0.0f;
    } else if (overheated) {
      effect_slot = 1;
      effect_scale = *(float *)(trigger + 0x10);
      effect_param = 0.0f;
    } else {
      effect_slot = 0;
      effect_scale = *(float *)(trigger + 0x10);
      if (*(float *)(weapon_tag + 0x350) == 0.0f) {
        effect_param = 0.0f;
      } else {
        effect_param = *(float *)(weapon + 0x1ec) / *(float *)(weapon_tag + 0x350);
      }
    }
    firing_effect = *(int *)(rate_elem + (effect_slot + 3) * 0x10);
    firing_damage = *(int *)(rate_elem + (effect_slot + 6) * 0x10);
  }

  if (fired) {
    if ((*(uint8_t *)(weapon + 0x1a4) & 2) && game_engine_running()) {
      player_index = player_index_from_unit_index(owner_unit);
      if (player_index != -1) {
        game_engine_weapon_fired(player_index);
      }
    }
    *(int *)(weapon + 0x278) = game_time_get();
    first_person_weapon_message_from_weapon(
      weapon_handle, overheated ? ((int16_t)trigger_index != 0) + 2 : ((int16_t)trigger_index != 0));

    /* The binary re-fetches weapon/trigger/definition here (0xfe0c7-0xfe0f7). */
    w = (char *)object_get_and_verify_type(weapon_handle, 4);
    refetch_trigger = (char *)weapon_trigger_get(w, (int16_t)trigger_index);
    refetch_trig_def = (char *)tag_block_get_element(
      (char *)tag_get(0x77656170, *(uint32_t *)w) + 0x4fc, (int16_t)trigger_index, 0x114);
    if (*(float *)(refetch_trig_def + 0xa4) > 0.0f && *(int8_t *)refetch_trig_def >= 0) {
      *(float *)(refetch_trigger + 0x14) = 1.0f;
    }
    if (*(float *)(trig_def + 0xa8) > 0.0f) {
      *(float *)(trigger + 0x18) = 1.0f;
    }

    if (!*(uint8_t *)0x5aa899) {
      *(float *)(weapon + 0x1ec) = *(float *)(trig_def + 0xb8) + *(float *)(weapon + 0x1ec);
    }
    if (!(*(uint32_t *)(weapon + 0x1a4) & 2) &&
        *(float *)(weapon + 0x1ec) > *(float *)(weapon_tag + 0x350)) {
      *(float *)(weapon + 0x1ec) = *(float *)(weapon_tag + 0x350);
    } else if (*(float *)(weapon + 0x1ec) > 1.0f) {
      *(float *)(weapon + 0x1ec) = 1.0f;
    }

    if ((*(uint32_t *)(weapon + 0x1a4) & 2) && !*(uint8_t *)0x5aa892) {
      *(float *)(weapon + 0x1f0) = *(float *)(trig_def + 0xbc) + *(float *)(weapon + 0x1f0);
      if (*(float *)(weapon + 0x1f0) > 1.0f) {
        *(float *)(weapon + 0x1f0) = 1.0f;
      }
    }

    weapon_set_state(weapon_handle, 0, (int16_t)(((int16_t)trigger_index != 0) + 1));

    if (!overheated) {
      if (charge_fire) {
        (*(int16_t *)(weapon + 0x20c))++;
      } else {
        trigger_create_projectiles(weapon_handle, (int16_t)trigger_index);
        ai_handle_unit_effect(owner_unit, 1, *(uint16_t *)(trig_def + 0x2e));
      }
    }

    if (owner_unit != -1 && firing_damage != -1) {
      owner = (char *)object_get_and_verify_type(owner_unit, 3);
      damage_data_new(&damage, firing_damage);
      damage.flags |= 8;
      damage.direction.i = -*(float *)(owner + 0x1ec);
      damage.direction.j = -*(float *)(owner + 0x1f0);
      damage.direction.k = -*(float *)(owner + 0x1f4);
      damage.epicenter.x = *(float *)(owner + 0x50);
      damage.epicenter.y = *(float *)(owner + 0x54);
      damage.epicenter.z = *(float *)(owner + 0x58);
      damage.origin.x = *(float *)(owner + 0x50);
      damage.origin.y = *(float *)(owner + 0x54);
      damage.origin.z = *(float *)(owner + 0x58);
      object_cause_damage(&damage, owner_unit, -1, -1, -1, NULL);
    }

    if (*(int16_t *)(weapon_tag + 0x4e2) == 3 && (int16_t)trigger_index == 1) {
      *(uint32_t *)(weapon + 0x1dc) |= 4;
    }
  }

  if (*(float *)(weapon + 0x1ec) > *(float *)(weapon_tag + 0x354) &&
      random_math_real((unsigned int *)get_global_random_seed_address()) < *(float *)(weapon_tag + 0x358)) {
    w = (char *)object_get_and_verify_type(weapon_handle, 4);
    weapon_effect_new(*(int *)((char *)tag_get(0x77656170, *(uint32_t *)w) + 0x390), 0.0f, 0.0f,
                      weapon_handle);
    object_delete(weapon_handle);
  }

  if (!fired) {
    w = (char *)object_get_and_verify_type(weapon_handle, 4);
    if ((int16_t)trigger_index < 0 || (int16_t)trigger_index >= 2) {
      display_assert("trigger_index>=0 && trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11, 1);
      system_exit(-1);
    }
    *(uint8_t *)(w + 0x211 + (int16_t)trigger_index * 36) = 7;
    *(int16_t *)(w + 0x212 + (int16_t)trigger_index * 36) = -1;
  } else if (*(char *)(trigger + 1) != 6 || overheated) {
    if (*(uint8_t *)trig_def & 1) {
      w = (char *)object_get_and_verify_type(weapon_handle, 4);
      if ((int16_t)trigger_index < 0 || (int16_t)trigger_index >= 2) {
        display_assert("trigger_index>=0 && trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                       "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11, 1);
        system_exit(-1);
      }
      *(uint8_t *)(w + 0x211 + (int16_t)trigger_index * 36) = 5;
      *(int16_t *)(w + 0x212 + (int16_t)trigger_index * 36) = -1;
    } else {
      weapon_trigger_recover(trigger_index, weapon_handle);
    }
  }

  *(uint32_t *)(trigger + 4) &= ~1u;
  weapon_effect_new(firing_effect, effect_scale, effect_param, weapon_handle);
}

/* 0xfe450 — weapon_trigger_begin_firing
 *
 * Name evidence: 2276 symbol dump (T1). Callers: weapon_update (0xfe910).
 *
 * Confirmed: cdecl, 3 stack args, plain RET — weapon_handle [EBP+8],
 *   trigger_index [EBP+0xc] (MOVSX ECX,BX / TEST AX,AX / CMP AX,2, so int16),
 *   param_3 [EBP+0x10] (MOV AL / TEST AL,AL, so a byte flag).
 * Confirmed: call order object_get_and_verify_type(handle, 4) ->
 *   weapon_trigger_get(EDI=object, SI=trigger_index) -> tag_get(0x77656170,
 *   *object) -> tag_block_get_element(tag+0x4fc, trigger_index, 0x114).
 * Confirmed: if trigger_defn int16 +0x20 != -1:
 * tag_block_get_element(tag+0x4f0, that index, 0x70) then
 * weapon_magazine_get(EDI=object, SI=re-read +0x20); a nonzero int16 at the
 * magazine clears the ready flag. Object byte +0x1dc bit 0 also clears it.
 * Confirmed: FUN_0018f3e0(object+0x48, object+0xc, NULL) true -> return; ready
 *   false -> return; param_3 nonzero -> weapon_trigger_fire(handle, index).
 * Confirmed x87: trigger_defn +0x48 > [0x2533c0] (TEST AH,0x41) selects the
 *   state-2 path; else trigger_defn +0xc4 > [0x2533c0] selects the state-1
 *   path; else weapon_trigger_fire. In the state-2 path, tag flags +0x308 bit
 *   0x800 with object +0x1f0 >= [0x2533c8] (TEST AH,1) fires and returns.
 * Confirmed: tag triggers count (+0x4fc) > 1 -> trigger +0x20 =
 *   weapon_effect_new(trigger_defn +0x68, 0, 0, handle@<eax>); else trigger
 *   float +0x10 > [0x2533c0] sets trigger +4 bit 0x20 then fires, otherwise
 *   clears bit 0x20.
 * Confirmed: ticks = _ftol2(duration * [0x253394]) BEFORE the second object
 *   lookup; store word +0x212 then byte state +0x211 (0xfe60f/0xfe617).
 * Unknown: meanings of param_3, object +0x1dc/+0x1f0, trigger +4/+0x10/+0x20,
 *   trigger_defn +0x20/+0x48/+0x68/+0xc4, tag +0x308 bit 0x800. The kb decl
 *   of weapon_trigger_fire takes int; the reference pushes the raw dword
 *   [EBP+0xc], here it receives the sign-extended int16 (same low 16 bits).
 */
void weapon_trigger_begin_firing(int weapon_handle, int16_t trigger_index,
                                 bool param_3)
{
  char *weapon_obj;
  char *trigger;
  char *weapon_defn;
  char *trigger_defn;
  char *weapon_data;
  int16_t charge_ticks;
  bool ready;

  weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon_obj, trigger_index);
  weapon_defn = (char *)tag_get(0x77656170, *(int *)weapon_obj);
  trigger_defn = (char *)tag_block_get_element((void *)(weapon_defn + 0x4fc),
                                               trigger_index, 0x114);
  ready = true;
  if (*(int16_t *)(trigger_defn + 0x20) != -1) {
    tag_block_get_element((void *)(weapon_defn + 0x4f0),
                          *(int16_t *)(trigger_defn + 0x20), 0x70);
    if (*(int16_t *)weapon_magazine_get(weapon_obj,
                                        *(int16_t *)(trigger_defn + 0x20)) != 0)
      ready = false;
  }
  if (*(unsigned char *)(weapon_obj + 0x1dc) & 1)
    ready = false;

  if (FUN_0018f3e0(weapon_obj + 0x48, weapon_obj + 0xc, (int16_t *)0))
    return;
  if (!ready)
    return;

  if (!param_3) {
    if (*(float *)(trigger_defn + 0x48) > *(float *)0x2533c0) {
      if ((*(unsigned int *)(weapon_defn + 0x308) & 0x800) &&
          *(float *)(weapon_obj + 0x1f0) >= *(float *)0x2533c8) {
        weapon_trigger_fire(weapon_handle, trigger_index);
        return;
      }
      if (*(int *)(weapon_defn + 0x4fc) > 1) {
        *(int *)(trigger + 0x20) = weapon_effect_new(
          *(int *)(trigger_defn + 0x68), 0.0f, 0.0f, weapon_handle);
      } else if (*(float *)(trigger + 0x10) > *(float *)0x2533c0) {
        *(unsigned int *)(trigger + 4) |= 0x20;
        weapon_trigger_fire(weapon_handle, trigger_index);
      } else {
        *(unsigned int *)(trigger + 4) &= ~0x20u;
      }
      charge_ticks =
        (int16_t)(int)(*(float *)(trigger_defn + 0x48) * TICKS_PER_SECOND);
      weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
      assert_halt_msg_at("trigger_index>=0 && "
                         "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                         "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11,
                         trigger_index >= 0 &&
                           trigger_index <
                             MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON);
      *(int16_t *)(weapon_data + trigger_index * 36 + 0x212) = charge_ticks;
      *(char *)(weapon_data + trigger_index * 36 + 0x211) = 2;
      return;
    }
    if (*(float *)(trigger_defn + 0xc4) > *(float *)0x2533c0) {
      charge_ticks =
        (int16_t)(int)(*(float *)(trigger_defn + 0xc4) * TICKS_PER_SECOND);
      weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
      assert_halt_msg_at("trigger_index>=0 && "
                         "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                         "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11,
                         trigger_index >= 0 &&
                           trigger_index <
                             MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON);
      *(int16_t *)(weapon_data + trigger_index * 36 + 0x212) = charge_ticks;
      *(char *)(weapon_data + trigger_index * 36 + 0x211) = 1;
      return;
    }
  }
  weapon_trigger_fire(weapon_handle, trigger_index);
}

/* 0xfe6c0 — weapon trigger charge start (raw offsets retained)
 *
 * Resolves the weapon object, runs the weapon_trigger_get trigger-bounds debug
 * assertion, fetches the trigger definition out of the weapon tag's 0x4fc
 * block, optionally notifies the next trigger, then latches trigger state 1
 * with a tick count derived from the definition's +0xc4 float.
 *
 * Confirmed: register args — trigger_index in EAX (MOV ESI,EAX at 0xfe6cc,
 *   later TEST SI,SI / CMP SI,0x2 / MOVSX EAX,SI), weapon_handle in ECX
 *   (MOV EBX,ECX at 0xfe6c7, pushed to object_get_and_verify_type twice).
 *   No stack args (RET with no immediate at 0xfe780).
 * Confirmed: call order object_get_and_verify_type(handle, 4) at 0xfe6ce ->
 *   weapon_trigger_get(EDI=object, SI=trigger_index) at 0xfe6d5 (EDI survives
 * it and is re-read at 0xfe6da) -> tag_get(0x77656170, *(int *)object) at
 *   0xfe6e2 -> tag_block_get_element(tag_data+0x4fc, trigger_index, 0x114)
 *   at 0xfe6f4. One ADD ESP,0x1c at 0xfe701 cleans all 7 pushed dwords.
 * Confirmed: MOV EAX,[EDI] / LEA ECX,[ESI+1] / CMP ECX,EAX / JGE at
 *   0xfe6fc–0xfe706 — EDI is tag_data+0x4fc, so the guard is
 *   trigger_index + 1 < block count, using the full 32-bit trigger_index.
 * Confirmed: the guarded call at 0xfe70d pushes EDX=trigger_index+1 then
 *   EBX=weapon_handle and cleans 8 bytes (ADD ESP,0x8 at 0xfe712), so
 *   FUN_000fdc90 is cdecl with arg1=weapon_handle, arg2=trigger_index+1.
 *   The kb.json decl was void(void); it is corrected from this call site.
 * Confirmed: FLD [EAX+0xc4] / FMUL [0x253394] / CALL _ftol2 at
 *   0xfe715–0xfe724 where EAX is the tag_block_get_element result saved at
 *   [EBP-0x4]; 0x253394 is TICKS_PER_SECOND (30.0f). The int result is kept
 *   in EDI and stored as a word, so it is truncated to int16.
 * Confirmed: the object is looked up a SECOND time at 0xfe72e (PUSH 0x4 /
 *   PUSH EBX) after the conversion and before the assert.
 * Confirmed: assert path PUSH 1 / PUSH 0xa11 / PUSH filepath / PUSH reason
 *   / CALL display_assert then PUSH -1 / CALL system_exit at 0xfe743–0xfe75b;
 *   same reason string and line as weapon_trigger_change_state (0xfb880).
 * Confirmed: address form MOVSX EAX,SI / LEA ECX,[EAX+EAX*8] /
 *   LEA EAX,[EBX+ECX*4] => weapon_data + (int16_t)trigger_index * 36.
 * Confirmed: store order — word DI to +0x212 FIRST (0xfe76c), then byte
 *   immediate 1 to +0x211 (0xfe775); this is the reverse of
 *   weapon_trigger_change_state (0xfb880).
 * Unknown: the trigger definition field at +0xc4 (a duration in seconds) and
 *   the meaning of trigger state 1; raw offsets retained to match the
 *   sibling accessors. The role of FUN_000fdc90 is not established.
 */
void weapon_trigger_overload(int trigger_index, int weapon_handle)
{
  int *weapon_obj;
  char *weapon_defn;
  char *trigger_defn;
  char *weapon_data;
  int16_t charge_ticks;
  int16_t tindex;

  weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  weapon_trigger_get(weapon_obj, (int16_t)trigger_index);
  weapon_defn = (char *)tag_get(0x77656170, *weapon_obj);
  trigger_defn = (char *)tag_block_get_element((void *)(weapon_defn + 0x4fc),
                                               trigger_index, 0x114);

  if (trigger_index + 1 < *(int *)(weapon_defn + 0x4fc)) {
    weapon_trigger_fire(weapon_handle, trigger_index + 1);
  }

  charge_ticks =
    (int16_t)(int)(*(float *)(trigger_defn + 0xc4) * TICKS_PER_SECOND);

  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);

  tindex = (int16_t)trigger_index;
  assert_halt_msg_at("trigger_index>=0 && "
                     "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11,
                     tindex >= 0 &&
                       tindex < MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON);

  *(int16_t *)(weapon_data + tindex * 36 + 0x212) = charge_ticks;
  *(char *)(weapon_data + tindex * 36 + 0x211) = 1;
}

/* 0xfe790 — weapon_trigger_release_charge (raw offsets retained)
 *
 * Name evidence (T2, trigger state machine): weapon_update (0xfe910)
 *   calls this from trigger state 3 (fully charged) exactly when the
 *   trigger is no longer being held, and again from state 3 when the
 *   magazine has run dry. It moves to state 6 for
 *   trigger_definition[+0x58] seconds, or — when that duration is zero —
 *   fires immediately (weapon_trigger_fire) and recovers
 *   (weapon_trigger_recover). That is the release of a held charge.
 *   The name previously sat on 0xfb880, which is weapon_trigger_change_state.
 *
 * Same prologue family as weapon_trigger_overload: resolve the weapon object,
 * run the weapon_trigger_get trigger-bounds debug helper (whose return is a
 * trigger record pointer), fetch the trigger definition from the weapon tag's
 * 0x4fc block, then either latch trigger state 6 with a tick count derived from
 * the definition's +0x58 float, or take the zero-duration path.
 *
 * Confirmed: register args — trigger_index arrives in EAX (MOV ESI,EAX at
 *   0xfe79e, later TEST SI,SI / CMP SI,0x2 / MOVSX EDI,SI), weapon_handle in
 *   ECX (MOV EBX,ECX at 0xfe799, pushed to object_get_and_verify_type twice).
 *   RET with no immediate at 0xfe859 / 0xfe884, so no stack args.
 * Confirmed: call order object_get_and_verify_type(handle, 4) at 0xfe7a0 ->
 *   weapon_trigger_get(EDI=object, SI=trigger_index) at 0xfe7a7 (its EAX return
 * is saved to [EBP-0x4]) -> tag_get(0x77656170, *(int *)object) at 0xfe7b7 ->
 *   tag_block_get_element(tag_data+0x4fc, (int16_t)trigger_index, 0x114) at
 *   0xfe7ce. One ADD ESP,0x1c at 0xfe7d5 cleans all 7 pushed dwords; the
 *   block pointer tag_data+0x4fc is also kept at [EBP-0x8].
 * Confirmed: FLD [ECX+0x58] / FCOMP [0x2533c0] / FNSTSW AX / TEST AH,0x41 /
 *   JNZ 0xfe85a at 0xfe7d8–0xfe7e6 — 0x2533c0 is 0.0f and the jump is taken
 *   on C3|C0, i.e. the +0x58 duration <= 0.0f path.
 * Confirmed: zero-duration path — MOV ECX,[EBP-0x8] / CMP [ECX],0x1 / JLE at
 *   0xfe85a–0xfe860 tests the trigger block count > 1; the guarded call at
 *   0xfe865 pushes 0x1 then EBX=weapon_handle and cleans 8 bytes, so it is
 *   FUN_000fdc90(weapon_handle, 1) — the literal 1, not trigger_index + 1.
 * Confirmed: MOV EAX,ESI / CALL 0xfcec0 at 0xfe86d — FUN_000fcec0 reads SI
 *   (MOV ESI,EAX at 0xfcec9) and EBX (PUSH EBX at 0xfcec8 into
 *   object_get_and_verify_type), so it is (trigger_index@<eax>,
 *   weapon_handle@<ebx>); EBX still holds weapon_handle here.
 * Confirmed: charged path — FLD [ECX+0x58] / FMUL [0x253394] / CALL _ftol2 at
 *   0xfe7e8–0xfe7f1 (0x253394 = TICKS_PER_SECOND, 30.0f); the result is kept
 *   at [EBP-0x8] and later read as a word (MOV DX,[EBP-0x8]), so it is
 *   truncated to int16.
 * Confirmed: the object is looked up a SECOND time at 0xfe7fc after the
 *   conversion and before the assert; assert path PUSH 1 / PUSH 0xa11 /
 *   PUSH filepath / PUSH reason / CALL display_assert then PUSH -1 /
 *   CALL system_exit at 0xfe811–0xfe829.
 * Confirmed: address form MOVSX EDI,SI / LEA ECX,[EDI+EDI*8] /
 *   LEA EAX,[EBX+ECX*4] => weapon_data + (int16_t)trigger_index * 36; store
 *   order is byte 6 to +0x211 FIRST (0xfe83c), then word to +0x212 (0xfe843).
 * Confirmed: both exits clear *(int *)(FUN_000fb320_result + 0x10) to 0
 *   (0xfe84e, 0xfe879).
 * Unknown: the trigger definition field at +0x58 (a duration in seconds), the
 *   meaning of trigger state 6, the trigger-record field at +0x10, and the
 *   role of FUN_000fdc90 / FUN_000fcec0; raw offsets retained.
 */
void weapon_trigger_release_charge(int trigger_index, int weapon_handle)
{
  int *weapon_obj;
  char *trigger;
  char *weapon_defn;
  char *trigger_defn;
  char *weapon_data;
  int16_t charge_ticks;
  int16_t tindex;

  weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  trigger = (char *)weapon_trigger_get(weapon_obj, (int16_t)trigger_index);
  weapon_defn = (char *)tag_get(0x77656170, *weapon_obj);
  tindex = (int16_t)trigger_index;
  trigger_defn =
    (char *)tag_block_get_element((void *)(weapon_defn + 0x4fc), tindex, 0x114);

  if (*(float *)(trigger_defn + 0x58) > *(float *)0x2533c0) {
    charge_ticks = (int16_t)(int)(*(float *)(trigger_defn + 0x58) *
                                  30.0f); /* TICKS_PER_SECOND */

    weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);

    assert_halt_msg_at("trigger_index>=0 && "
                       "trigger_index<MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON",
                       "c:\\halo\\SOURCE\\items\\weapons.c", 0xa11,
                       tindex >= 0 &&
                         tindex < MAXIMUM_NUMBER_OF_TRIGGERS_PER_WEAPON);

    *(char *)(weapon_data + tindex * 36 + 0x211) = 6;
    *(int16_t *)(weapon_data + tindex * 36 + 0x212) = charge_ticks;
    *(int *)(trigger + 0x10) = 0;
    return;
  }

  if (*(int *)(weapon_defn + 0x4fc) > 1) {
    weapon_trigger_fire(weapon_handle, 1);
  }
  weapon_trigger_recover(trigger_index, weapon_handle);
  *(int *)(trigger + 0x10) = 0;
}

/* 0xfe890 — weapon_trigger_overcharged */
void weapon_trigger_overcharged(int trigger_index, int weapon_handle)
{
  char *weapon;
  char *tag;
  char *trigger_def;
  int16_t action;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  weapon_trigger_get(weapon, (int16_t)trigger_index);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  trigger_def = (char *)tag_block_get_element((char *)tag + 0x4fc, (int16_t)trigger_index, 0x114);

  action = *(int16_t *)(trigger_def + 0x50);
  if (action == 1) {
    weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
    tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
    weapon_effect_new(*(int *)(tag + 0x390), 0.0f, 0.0f, weapon_handle);
    object_delete(weapon_handle);
  } else if (action == 2) {
    weapon_trigger_release_charge((int16_t)trigger_index, weapon_handle);
  }
}

/* 0xfe910 — weapon_update
 *
 * Re-verified against cachebeta.xbe 0xfe910-0xff3cc (jump table 0xff3d0).
 * Prologue block 0xfe95d-0xfec79 (tracked-object validation, animation
 * update, detonate-when-unheld, age decay, heat/overheat, per-trigger
 * initial "active" bytes at [EBP-8]/[EBP-7], auto-reload flag 8) was
 * missing from the previous lift. trigger_active is a per-trigger byte
 * array seeded by that block, not a constant false.
 */
boolean weapon_update(int weapon_handle)
{
  char *weapon;
  char *tag;
  char *unit;
  char *unit_tag;
  char *mag;
  char *mag_def;
  char *trig;
  char *trig_def;
  int i;
  int obj_h;
  int16_t rounds_per_second;
  int16_t magazine_state;
  int16_t rounds;
  int16_t anim_result;
  uint32_t flags;
  uint8_t control_flags;
  boolean skip_age;
  boolean overheat_message;
  char trigger_active_init[2];
  char trigger_active;
  float heat_loss;

  weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  if (*(uint8_t *)0x449ef1 && *(uint8_t *)0x31f3c8) {
    profile_enter_private((void *)0x31f3c0);
  }

  /* 0xfe95d: drop a stale tracked object handle. */
  if (*(int *)(weapon + 0x200) != -1 &&
      object_try_and_get_and_verify_type(*(int *)(weapon + 0x200), -1) == 0) {
    *(int *)(weapon + 0x200) = -1;
  }

  /* 0xfe981: animation update; result 1 = key frame, 2 = next state. */
  if (*(int *)(tag + 0x44) != -1 && *(int16_t *)(weapon + 0x80) != -1) {
    anim_result = (int16_t)animation_update_internal(1, *(int *)(tag + 0x44),
                                                     (short *)(weapon + 0x80), 0);
    if (anim_result == 1) {
      weapon_state_key_frame(weapon_handle);
    } else if (anim_result == 2) {
      weapon_state_next(weapon_handle);
    }
  }

  /* 0xfe9c1: tag flag 0x400 detonates an unheld weapon. */
  if ((*(uint32_t *)(tag + 0x308) & 0x400) && *(int *)(weapon + 0xcc) == -1) {
    item_detonate(weapon_handle);
  }

  /* 0xfe9de: +0x1f8 decays by 1/24 per tick unless the holding unit's tag
   * has flag 0x800000 at +0x17c. */
  if (*(float *)(weapon + 0x1f8) > 0.0f) {
    skip_age = false;
    if (*(int *)(weapon + 0xcc) != -1) {
      unit = (char *)object_try_and_get_and_verify_type(*(int *)(weapon + 0xcc), 3);
      if (unit != 0 && *(int *)unit != -1) {
        unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
        if (*(uint32_t *)(unit_tag + 0x17c) & 0x800000) {
          skip_age = true;
        }
      }
    }
    if (!skip_age) {
      *(float *)(weapon + 0x1f8) -= *(float *)0x28af18;
      if (*(float *)(weapon + 0x1f8) < 0.0f) {
        *(float *)(weapon + 0x1f8) = 0.0f;
      }
    }
  }

  /* 0xfea57: heat (+0x1ec) / overheat flags (+0x1dc bits 0,1,2). */
  if (*(float *)(weapon + 0x1ec) > 0.0f) {
    if (!(*(float *)(weapon + 0x1ec) < *(float *)(tag + 0x350)) &&
        !(*(uint32_t *)(weapon + 0x1dc) & 1)) {
      flags = *(uint32_t *)(weapon + 0x1dc) | 1;
      *(uint32_t *)(weapon + 0x1dc) = flags;
      overheat_message = false;
      if (*(int16_t *)(tag + 0x4e2) == 3 && (flags & 4)) {
        *(uint32_t *)(weapon + 0x1dc) = flags & ~4u;
        overheat_message = true;
      }
      first_person_weapon_message_from_weapon(weapon_handle, overheat_message ? 0x10 : 0xf);
      *(int *)(weapon + 0x274) = weapon_effect_looping_new(*(int *)(tag + 0x380), weapon_handle);
    }
    /* 0xfead9: FCOMP 0 / TEST AH,0x44 / JP skip => runs only when +0x1f4 == 0. */
    if (*(float *)(weapon + 0x1f4) == 0.0f) {
      heat_loss = *(float *)(tag + 0x35c) * *(float *)0x2546a4;
      if (*(float *)(tag + 0x440) > 0.0f) {
        heat_loss *= 1.0f - *(float *)(weapon + 0x1f0) * *(float *)(tag + 0x440);
      }
      *(float *)(weapon + 0x1ec) -= heat_loss;
      if (*(float *)(weapon + 0x1ec) < 0.0f) {
        *(float *)(weapon + 0x1ec) = 0.0f;
      }
      flags = *(uint32_t *)(weapon + 0x1dc);
      if ((flags & 1) && !(flags & 2)) {
        if ((*(float *)(weapon + 0x1ec) - *(float *)(tag + 0x34c)) / heat_loss <= 1.0f) {
          *(uint32_t *)(weapon + 0x1dc) = flags | 2;
        }
      }
    }
    flags = *(uint32_t *)(weapon + 0x1dc);
    if ((flags & 1) && *(float *)(weapon + 0x1ec) < *(float *)(tag + 0x34c)) {
      *(uint32_t *)(weapon + 0x1dc) = flags & ~3u;
      if (*(int *)(weapon + 0x274) != -1) {
        effect_stop(*(int *)(weapon + 0x274), 1);
      }
    }
  }

  /* 0xfebbf */
  *(float *)(weapon + 0x1f4) = 0.0f;
  if (*(int16_t *)(weapon + 0x1ea) > 0) {
    (*(int16_t *)(weapon + 0x1ea))--;
  }

  /* 0xfebdf: per-trigger initial active bytes from control flags +0x1e0. */
  control_flags = *(uint8_t *)(weapon + 0x1e0);
  if (!(control_flags & 0x10) && *(int16_t *)(weapon + 0x1ea) <= 0) {
    trigger_active_init[0] = (char)((control_flags >> 1) & 1);
    trigger_active_init[1] = ((*(uint32_t *)(tag + 0x308) & 0x1000) && (control_flags & 4)) ? 1 : 0;
  } else {
    trigger_active_init[0] = 0;
    trigger_active_init[1] = 0;
  }
  /* 0xfec18: tag+0x32c secondary-trigger mode. The +0x220 test is an
   * integer compare of trigger[0]+0x10 against 0x3f800000 (1.0f). */
  switch (*(int16_t *)(tag + 0x32c)) {
  case 1:
    if (trigger_active_init[1] && *(int *)(tag + 0x4fc) > 0 &&
        *(uint32_t *)(weapon + 0x220) != 0x3f800000) {
      trigger_active_init[1] = 0;
    }
    break;
  case 2:
    if (trigger_active_init[1]) {
      trigger_active_init[0] = 0;
    }
    break;
  }
  if ((control_flags & 8) && *(int *)(tag + 0x4f0) > 0) {
    *(uint32_t *)(weapon + 0x1dc) |= 8;
  }
  if (*(uint32_t *)(weapon + 0x1dc) & 8) {
    weapon_magazine_start_reload(0, weapon_handle, 1);
  }

  /* 0xfec7c: magazines */
  for (i = 0; i < *(int *)(tag + 0x4f0); i = (int16_t)(i + 1)) {
    if ((int16_t)i < 0 ||
        i >= *(int *)((char *)tag_get(0x77656170, *(uint32_t *)weapon) + 0x4f0)) {
      display_assert("magazine_index>=0 && magazine_index<weapon_definition->weapon.magazines.count",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0x672, 1);
      system_exit(-1);
    }
    mag = weapon + 0x258 + i * 12;
    mag_def = (char *)tag_block_get_element(tag + 0x4f0, i, 0x70);
    rounds_per_second = *(int16_t *)(mag_def + 4);
    if (rounds_per_second > 0 && *(int16_t *)(mag + 8) < *(int16_t *)(mag_def + 10)) {
      *(int16_t *)(mag + 10) += rounds_per_second % 30;
      *(int16_t *)(mag + 8) += rounds_per_second / 30;
      if (*(int16_t *)(mag + 10) >= 30) {
        (*(int16_t *)(mag + 8))++;
        *(int16_t *)(mag + 10) -= 30;
      }
      if (*(int16_t *)(mag + 8) > *(int16_t *)(mag_def + 10)) {
        *(int16_t *)(mag + 8) = *(int16_t *)(mag_def + 10);
      }
    }
    if (*(int16_t *)(mag + 2) != 0) {
      (*(int16_t *)(mag + 2))--;
    }
    magazine_state = *(int16_t *)mag;
    if (magazine_state == 1) {
      if (*(int16_t *)(mag + 2) - 1 <= 0) {
        weapon_magazine_finish_reload(weapon_handle, i);
      }
    } else if (magazine_state == 2) {
      weapon_magazine_start_chamber((int16_t)i, weapon_handle);
    } else if (magazine_state == 3) {
      if (*(int16_t *)(mag + 2) == 0) {
        weapon_magazine_finish_chamber((int16_t)i, weapon_handle);
      }
    }
  }

  /* 0xfedc6: triggers */
  for (i = 0; i < *(int *)(tag + 0x4fc); i = (int16_t)(i + 1)) {
    if ((int16_t)i < 0 ||
        i >= *(int *)((char *)tag_get(0x77656170, *(uint32_t *)weapon) + 0x4fc)) {
      display_assert("trigger_index>=0 && trigger_index<weapon_definition->weapon.triggers.count",
                     "c:\\halo\\SOURCE\\items\\weapons.c", 0x667, 1);
      system_exit(-1);
    }
    trig = weapon + 0x210 + i * 36;
    trig_def = (char *)tag_block_get_element(tag + 0x4fc, i, 0x114);

    if ((*(uint32_t *)trig_def & 0x200) && (*(uint8_t *)(weapon + 0x1a4) & 2)) {
      trigger_active_init[i] = *(float *)(weapon + 0x1e4) > *(float *)0x2533e8;
    }
    if ((*(uint32_t *)trig_def & 0x40) && *(int *)(weapon + 0xcc) == -1) {
      trigger_active_init[i] = 1;
    }
    if (*(int16_t *)(trig + 2) != 0) {
      (*(int16_t *)(trig + 2))--;
    }
    if (*(uint32_t *)trig_def & 0x10) {
      if (!(*(uint32_t *)(trig + 4) & 2) && trigger_active_init[i]) {
        *(uint32_t *)(trig + 4) ^= 4;
      }
      if (trigger_active_init[i]) {
        *(uint32_t *)(trig + 4) |= 2;
      } else {
        *(uint32_t *)(trig + 4) &= ~2u;
      }
      trigger_active_init[i] = (char)((*(uint32_t *)(trig + 4) >> 2) & 1);
    }
    trigger_active = trigger_active_init[i];
    if (!trigger_active) {
      *(uint32_t *)(trig + 4) |= 1;
    }

    if (*(float *)(trig + 0x14) > 0.0f) {
      *(float *)(trig + 0x14) -= *(float *)(trig_def + 0xf4);
      if (*(float *)(trig + 0x14) <= 0.0f) *(float *)(trig + 0x14) = 0.0f;
    }
    if (*(float *)(trig + 0x18) > 0.0f) {
      *(float *)(trig + 0x18) -= *(float *)(trig_def + 0xf0);
      if (*(float *)(trig + 0x18) <= 0.0f) *(float *)(trig + 0x18) = 0.0f;
    }

    switch (*(int8_t *)(trig + 1)) {
    case 0:
      if (!(*(uint8_t *)(weapon + 0x1e0) & 0x10) && *(int *)(weapon + 0xcc) != -1 &&
          *(int16_t *)(trig_def + 0x20) != -1) {
        rounds = *(int16_t *)((char *)weapon_magazine_get(weapon, *(int16_t *)(trig_def + 0x20)) + 8);
        if ((rounds < *(int16_t *)(trig_def + 0x22) && !(*(uint32_t *)trig_def & 4)) ||
            rounds < *(int16_t *)(trig_def + 0x24) || rounds == 0) {
          weapon_magazine_start_reload(*(int16_t *)(trig_def + 0x20), weapon_handle, 1);
        }
      }
      if (trigger_active && weapon_trigger_can_fire_again(weapon_handle, (int16_t)i)) {
        weapon_trigger_begin_firing(weapon_handle, (int16_t)i, 0);
      } else if (*(int8_t *)trig < 0x7f) {
        (*(int8_t *)trig)++;
      }
      break;
    case 1:
      if (!trigger_active) {
        weapon_trigger_begin_firing(weapon_handle, (int16_t)i, 1);
      } else if (*(int16_t *)(trig + 2) == 0 &&
                 *(int16_t *)(weapon + 0x20c) < *(int16_t *)(tag + 0x32e)) {
        weapon_trigger_overload(i, weapon_handle);
      }
      break;
    case 2:
      if (*(int16_t *)(trig + 2) != 0) {
        if (!trigger_active) {
          if ((int16_t)i == 0 && *(int *)(tag + 0x4fc) > 1 && !(*(uint32_t *)(trig + 4) & 0x20)) {
            weapon_trigger_begin_firing(weapon_handle, 0, 1);
          } else {
            weapon_trigger_idle((int16_t)i, weapon_handle);
          }
          if (*(int *)(trig + 0x20) != -1) {
            effect_stop(*(int *)(trig + 0x20), 1);
            *(int *)(trig + 0x20) = -1;
          }
        }
      } else {
        weapon_trigger_fully_charged((int16_t)i, weapon_handle);
      }
      break;
    case 3:
      if (trigger_active) {
        *(float *)(weapon + 0x1f4) = 1.0f - ((float)*(int16_t *)(trig + 2) * *(float *)0x2546a4) /
                                               *(float *)(trig_def + 0x4c);
        if (*(int16_t *)(trig + 2) != 0) {
          if (*(int16_t *)((char *)weapon_magazine_get(weapon, *(int16_t *)(trig_def + 0x20)) + 8) <
                  *(int16_t *)(trig_def + 0x22) &&
              !(*(uint32_t *)trig_def & 4)) {
            weapon_trigger_release_charge(i, weapon_handle);
          }
        } else {
          weapon_trigger_overcharged(i, weapon_handle);
        }
      } else {
        weapon_trigger_release_charge(i, weapon_handle);
      }
      break;
    case 4:
      if (*(int16_t *)(trig + 2) == 0) {
        if ((*(uint32_t *)trig_def & 8) && (*(uint8_t *)(weapon + 0x1a4) & 2) &&
            !(*(uint32_t *)(trig + 4) & 1)) {
          weapon_trigger_locked(weapon_handle, (int16_t)i);
        } else {
          weapon_trigger_idle((int16_t)i, weapon_handle);
        }
      }
      break;
    case 5:
      if (!trigger_active || *(int *)(weapon + 0x200) == -1) {
        weapon_trigger_finish_tracking(weapon_handle, i);
      }
      break;
    case 6:
      if (*(int16_t *)(trig + 2) != 0) {
        weapon_trigger_begin_firing(weapon_handle, (int16_t)i, 1);
      } else {
        weapon_trigger_recover(i, weapon_handle);
      }
      break;
    case 7:
      if (!trigger_active) {
        weapon_trigger_idle((int16_t)i, weapon_handle);
      }
      break;
    case 8:
      if (*(int16_t *)(trig + 2) == 0) {
        weapon_trigger_idle((int16_t)i, weapon_handle);
      }
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\items\\weapons.c", 0x30a, 1);
      system_exit(-1);
    }

    if (trigger_active) {
      *(float *)(trig + 0x10) = *(float *)(trig_def + 0xf8) + *(float *)(trig + 0x10);
      if (*(float *)(trig + 0x10) > 1.0f) *(float *)(trig + 0x10) = 1.0f;
      if (*(float *)(trig_def + 0x14) != 0.0f && !(*(uint32_t *)(trig + 4) & 0x10) &&
          *(float *)(trig + 0x10) > *(float *)(trig_def + 0x14)) {
        weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
        obj_h = weapon_handle;
        if ((*(uint8_t *)(weapon + 4) & 1) && *(int *)(weapon + 0xcc) != -1) {
          obj_h = *(int *)(weapon + 0xcc);
        }
        object_permute_region(obj_h, *(const char **)((char *)0x31f3b8 + i * 4), -1, 1);
        *(uint32_t *)(trig + 4) |= 0x10;
      }
    } else {
      *(float *)(trig + 0x10) -= *(float *)(trig_def + 0xfc);
      if (*(float *)(trig + 0x10) < 0.0f) *(float *)(trig + 0x10) = 0.0f;
      if ((*(uint32_t *)(trig + 4) & 0x10) && *(float *)(trig + 0x10) < *(float *)(trig_def + 0x14)) {
        weapon = (char *)object_get_and_verify_type(weapon_handle, 4);
        obj_h = weapon_handle;
        if ((*(uint8_t *)(weapon + 4) & 1) && *(int *)(weapon + 0xcc) != -1) {
          obj_h = *(int *)(weapon + 0xcc);
        }
        object_permute_region(obj_h, *(const char **)((char *)0x31f3b8 + i * 4), -1, 0);
        *(uint32_t *)(trig + 4) &= ~0x10u;
      }
    }

    if (*(uint8_t *)(trig + 1) != 6 && *(uint8_t *)(trig + 1) != 4 && !trigger_active) {
      *(float *)(trig + 0x1c) -= *(float *)(trig_def + 0x104);
      if (*(float *)(trig + 0x1c) < 0.0f) *(float *)(trig + 0x1c) = 0.0f;
    } else {
      *(float *)(trig + 0x1c) = *(float *)(trig_def + 0x100) + *(float *)(trig + 0x1c);
      if (*(float *)(trig + 0x1c) > 1.0f) *(float *)(trig + 0x1c) = 1.0f;
    }
  }

  if (*(uint8_t *)0x449ef1 && *(uint8_t *)0x31f3c8) {
    profile_exit_private((void *)0x31f3c0);
  }

  return 1;
}
