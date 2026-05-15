void FUN_001365d0(int object_handle, int arg1, int arg2);

/* Initialize a damage_params struct with a damage effect tag index (0x136750).
 * Zeroes 0x54 bytes, sets tag_index at +0x00, and initializes sentinel/default
 * fields to -1 and scale fields at +0x40/+0x44 to 1.0f. */
void damage_data_new(void *damage_params, int tag_index)
{
  csmemset(damage_params, 0, 0x54);
  *(int *)damage_params = tag_index;
  *(int16_t *)((char *)damage_params + 0x4c) = -1;
  *(int *)((char *)damage_params + 0x08) = -1;
  *(int *)((char *)damage_params + 0x0c) = -1;
  *(int16_t *)((char *)damage_params + 0x10) = -1;
  *(int16_t *)((char *)damage_params + 0x18) = -1;
  *(float *)((char *)damage_params + 0x40) = 1.0f;
  *(float *)((char *)damage_params + 0x44) = 1.0f;
}

/* object_restore_body (0x136790) — Check if object body vitality is below full
 * and restore it to 1.0f if so.
 *
 * Returns true (1) if vitality was restored, false (0) if the object has
 * bit 2 of flags byte at +0xb6 set (damage-related flag) or if vitality
 * is already >= 1.0f.
 *
 * Confirmed: object_get_and_verify_type(handle, -1) at 0x136799.
 * Confirmed: TEST AL,0x4 at 0x1367ab checks bit 2 of [obj+0xb6].
 * Confirmed: FLD [ECX+0x90]; FCOMP [0x2533c8] compares vitality with 1.0f.
 * Confirmed: FNSTSW AX; TEST AH,0x5; JP tests "not less than" (>= or NaN).
 * Confirmed: MOV [ECX+0x90],0x3f800000 sets vitality to 1.0f.
 * Confirmed: XOR DL,DL sets default false return before first branch.
 * Confirmed: caller at 0xbd055 tests return with TEST AL,AL (bool).
 */
char object_restore_body(int object_handle)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if ((*(unsigned char *)(obj + 0xb6) & 4) != 0)
    return 0;
  if (*(float *)(obj + 0x90) < 1.0f) {
    *(float *)(obj + 0x90) = 1.0f;
    return 1;
  }
  return 0;
}

/* object_double_charge_shield (0x1367e0) — Check if object shield vitality is
 * at or below full and prepare it for shield regeneration if so.
 *
 * If shield vitality > 1.0f, returns false (0).
 * If shield vitality <= 1.0f:
 *   - Sets bit 4 of flags byte at object+0xb6
 *   - If shield vitality == 0.0f, clamps it to 0.01f
 *   - Clears the shield-damage counter word at object+0xb4
 *   - Returns true (1)
 *
 * Confirmed: object_get_and_verify_type(handle, -1) at 0x1367e9.
 * Confirmed: FLD [ECX+0x94]; FCOMP [0x2533c8] compares shield with 1.0f.
 * Confirmed: TEST AH,0x41; JP tests "above" (> 1.0f) → return 0.
 * Confirmed: OR byte [ECX+0xb6],0x10 sets bit 4 of flags byte.
 * Confirmed: FCOMP [0x2533c0]; TEST AH,0x44; JP tests "not equal to 0.0f".
 * Confirmed: MOV [ECX+0x94],0x3c23d70a sets shield to 0.01f.
 * Confirmed: MOV word [ECX+0xb4],0x0 clears shield-damage counter.
 * Confirmed: caller at 0xbd039 tests return with TEST AL,AL (bool).
 */
char object_double_charge_shield(int object_handle)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if (*(float *)(obj + 0x94) <= 1.0f) {
    *(unsigned char *)(obj + 0xb6) |= 0x10;
    if (*(float *)(obj + 0x94) == 0.0f) {
      *(float *)(obj + 0x94) = 0.01f;
    }
    *(unsigned short *)(obj + 0xb4) = 0;
    return 1;
  }
  return 0;
}

/* FUN_00136840 (0x136840) — Recursively apply FUN_0013c740 to all child
 * objects in the object hierarchy.
 *
 * Starting from the given object, reads the first child handle at +0xC8,
 * then iterates siblings via the link at +0xC4. For each child, calls
 * FUN_0013c740; if it returns false, recurses into that child's subtree.
 *
 * Confirmed: object_get_and_verify_type(handle, -1) at 0x13684a for root.
 * Confirmed: MOV ESI,[EAX+0xc8] at 0x13684f reads first child handle.
 * Confirmed: object_get_and_verify_type(child, -1) at 0x136863 in loop.
 * Confirmed: MOV EDI,[EAX+0xc4] at 0x136868 reads next sibling handle.
 * Confirmed: CALL 0x0013c740 at 0x13686f with child handle in ESI.
 * Confirmed: recursive CALL 0x00136840 at 0x13687c if FUN_0013c740 returns 0.
 * Confirmed: void return, cdecl, 1 param.
 */
void FUN_00136840(int object_handle)
{
  char *obj;
  int child_handle;
  int next_handle;
  char result;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  child_handle = *(int *)(obj + 0xc8);
  while (child_handle != -1) {
    obj = (char *)object_get_and_verify_type(child_handle, -1);
    next_handle = *(int *)(obj + 0xc4);
    result = FUN_0013c740(child_handle);
    if (result == 0) {
      FUN_00136840(child_handle);
    }
    child_handle = next_handle;
  }
}

/* FUN_00136890 (0x136890) — Find the player index that owns a given object.
 *
 * Walks up the object parent chain (via object+0xcc) looking for a unit
 * (biped or vehicle, type_mask=3). When found, returns
 * player_index_from_unit_index for that unit. Returns -1 if no unit
 * ancestor is found.
 *
 * Confirmed: @EAX register arg from caller at 0x137e2c (MOV EAX,EBX; CALL).
 * Confirmed: cdecl callees object_try_and_get_and_verify_type,
 *            object_get_and_verify_type, player_index_from_unit_index.
 */
int FUN_00136890(int object_index)
{
  void *obj;

  while (object_index != -1) {
    if (object_try_and_get_and_verify_type(object_index, 3) != NULL) {
      return player_index_from_unit_index(object_index);
    }
    obj = object_get_and_verify_type(object_index, -1);
    object_index = *(int *)((char *)obj + 0xcc);
  }
  return -1;
}

/* object_can_take_damage (0x1368e0) — Clear bit 3 of object+0xb7 flags byte for
 * all children/widgets of a given parent handle.
 *
 * Iterates using FUN_000ce450 (first) / FUN_000ce320 (next) to enumerate
 * associated objects. For each, clears bit 3 (AND 0xf7) of the flags byte
 * at offset 0xb7. This is the same byte modified by
 * object_set_ranged_attack_inhibited (bit 0) and
 * object_set_melee_attack_inhibited (bit 7).
 *
 * Confirmed: cdecl, single stack param at [EBP+0x8].
 * Confirmed: object_get_and_verify_type(index, -1) at CALL 0x13d680.
 * Confirmed: AND byte [EAX+0xb7],0xf7 at 0x136908.
 * Confirmed: FUN_000ce450 (first child) at CALL 0xce450.
 * Confirmed: FUN_000ce320 (next child) at CALL 0xce320.
 */
void object_can_take_damage(int player_handle)
{
  int iter_state;
  int object_index;
  char *obj;

  object_index = FUN_000ce450(player_handle, &iter_state);
  while (object_index != -1) {
    obj = (char *)object_get_and_verify_type(object_index, -1);
    *(unsigned char *)(obj + 0xb7) &= 0xf7;
    object_index = FUN_000ce320(player_handle, &iter_state);
  }
}

/* object_get_maximum_body_vitality (0x136930) — Set bit 3 of object+0xb7 flags
 * byte for all children/widgets of a given parent handle.
 *
 * Complement of object_can_take_damage which clears the same bit. Iterates
 * using FUN_000ce450 (first) / FUN_000ce320 (next) to enumerate associated
 * objects. For each, sets bit 3 (OR 0x8) of the flags byte at offset 0xb7.
 *
 * Confirmed: cdecl, single stack param at [EBP+0x8].
 * Confirmed: object_get_and_verify_type(index, -1) at CALL 0x13d680.
 * Confirmed: OR byte [EAX+0xb7],0x8 at 0x136958.
 * Confirmed: FUN_000ce450 (first child) at CALL 0xce450.
 * Confirmed: FUN_000ce320 (next child) at CALL 0xce320.
 */
void object_get_maximum_body_vitality(int player_handle)
{
  int iter_state;
  int object_index;
  char *obj;

  object_index = FUN_000ce450(player_handle, &iter_state);
  while (object_index != -1) {
    obj = (char *)object_get_and_verify_type(object_index, -1);
    *(unsigned char *)(obj + 0xb7) |= 0x8;
    object_index = FUN_000ce320(player_handle, &iter_state);
  }
}

/* object_set_ranged_attack_inhibited (0x136980) — Set or clear the
 * damage-invincible bit on an object.
 *
 * If object_handle is valid (!= -1), gets the object and sets or clears
 * bit 0 of byte at object+0xb7 based on the flag parameter.
 *
 * Confirmed: CMP EAX,-1; JZ skip at 0x136986.
 * Confirmed: object_get_and_verify_type(handle, -1) at 0x13698e.
 * Confirmed: OR byte [EAX+0xb7],0x1 (set) at 0x13699d.
 * Confirmed: AND byte [EAX+0xb7],0xfe (clear) at 0x1369a6.
 * Confirmed: flag at [EBP+0xc] tested via TEST CL,CL at 0x136999.
 */
void object_set_ranged_attack_inhibited(int object_handle, char flag)
{
  char *obj;

  if (object_handle != -1) {
    obj = (char *)object_get_and_verify_type(object_handle, -1);
    if (flag != 0) {
      *(unsigned char *)(obj + 0xb7) |= 1;
      return;
    }
    *(unsigned char *)(obj + 0xb7) &= 0xfe;
  }
}

/* object_set_melee_attack_inhibited (0x1369b0) — Set or clear bit 7 of
 * object+0xb6 flags byte.
 *
 * Confirmed: identical structure to object_set_ranged_attack_inhibited but
 * targets offset 0xb6 bit 7. Confirmed: OR byte [EAX+0xb6],0x80 (set) at
 * 0x1369cd. Confirmed: AND byte [EAX+0xb6],0x7f (clear) at 0x1369d6.
 */
void object_set_melee_attack_inhibited(int object_handle, char flag)
{
  char *obj;

  if (object_handle != -1) {
    obj = (char *)object_get_and_verify_type(object_handle, -1);
    if (flag != 0) {
      *(unsigned char *)(obj + 0xb6) |= 0x80;
      return;
    }
    *(unsigned char *)(obj + 0xb6) &= 0x7f;
  }
}

/* FUN_001369e0 (0x1369e0) — Create effect on object (damage-related wrapper).
 *
 * Wrapper around FUN_0009ec30 (effect creation). Passes the object_handle as
 * both object_handle and parent_handle, marker=-1, and zeros for remaining
 * args.
 *
 * Confirmed: @EAX register arg (object_handle) from both callers:
 *   0x136e13: MOV EAX,EDI; CALL 0x1369e0
 *   0x138717: MOV EAX,EDI; CALL 0x1369e0
 * Confirmed: 8 pushes before CALL 0x0009ec30, ADD ESP,0x20.
 * Confirmed: push order: 0,0,0,0,-1,EAX,EAX,[EBP+8].
 * Confirmed: void return (callers ignore EAX after call).
 */
void FUN_001369e0(int object_handle, int effect_tag_index)
{
  FUN_0009ec30(effect_tag_index, object_handle, object_handle, -1, 0, 0, 0,
               0); /* dup-args-ok: confirmed PUSH EAX,EAX */
}

/* FUN_00136a00 (0x136a00) — Set or clear region "cannot be destroyed" byte
 * for all collision model regions that have the "forces object death" flag
 * (bit 4 at region+0x20) and more than one permutation (region+0x48 > 1).
 *
 * When param_1 is 0, writes 1 (true) to obj[0x130 + region_index].
 * When param_1 is non-zero, writes 0 (false).
 *
 * Confirmed: @EAX register arg (object_handle) from both callers:
 *   0x136ba6: MOV EAX,EDI; CALL 0x136a00 (flag=0)
 *   0x138727: MOV EAX,EDI; CALL 0x136a00 (flag=1)
 * Confirmed: PUSH -0x1; PUSH EAX; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: tag_get('obje', obj[0]) then tag_get('coll', obje[0x7c]).
 * Confirmed: LEA EDI,[EAX+0x240] = regions tag_block in collision model.
 * Confirmed: tag_block_get_element(regions, index, 0x54) per region.
 * Confirmed: SETZ AL inverts param_1 for the store.
 * Confirmed: MOVSX ESI,AX = short truncation of loop counter.
 * Confirmed: MOV [ESI+EBX*1+0x130],AL stores at obj + 0x130 + index.
 */
void FUN_00136a00(int object_handle, char param_1)
{
  char *obj;
  char *obje_tag;
  char *coll_tag;
  int *regions;
  short i;
  int index;
  char *element;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *(int *)obj);
  coll_tag = (char *)tag_get(0x636f6c6c, *(int *)(obje_tag + 0x7c));
  regions = (int *)(coll_tag + 0x240);
  i = 0;
  index = 0;
  if (0 < *regions) {
    do {
      element = (char *)tag_block_get_element(regions, index, 0x54);
      if (((*(unsigned char *)(element + 0x20) & 0x10) != 0) &&
          (*(int *)(element + 0x48) > 1)) {
        *(char *)(obj + 0x130 + index) = (param_1 == 0);
      }
      i = (short)(i + 1);
      index = (int)i;
    } while (index < *regions);
  }
}

/* object_get_actual_body_vitality (0x136a80) — Compute scaled body vitality for
 * an object.
 *
 * Returns body_vitality * body_max_vitality, optionally scaled by the
 * difficulty modifier for value_type 1 (body vitality) when param_2 is 0.
 * When param_2 is non-zero, no difficulty scaling is applied.
 *
 * Object offsets:
 *   +0x68: team index (uint16_t), passed to FUN_000b55b0 as team arg
 *   +0x88: body vitality (float)
 *   +0x90: body max vitality (float)
 *
 * Confirmed: PUSH -1; PUSH ESI; CALL 0x13d680 => object_get_and_verify_type x2.
 * Confirmed: MOV EAX,[EAX+0x90] stores body_max_vitality in local [EBP-8].
 * Confirmed: FLD [EAX+0x88] loads body_vitality; FST [EBP-4] copies to local.
 * Confirmed: TEST CL,CL branches on param_2.
 * Confirmed: XOR ECX,ECX; MOV CX,[EAX+0x68] zero-extends team to int.
 * Confirmed: PUSH ECX; PUSH 1; CALL 0xb55b0 => FUN_000b55b0(1, team).
 * Confirmed: FMUL [EBP-4] then FMUL [EBP-8] for final result.
 * Confirmed: caller at 0x52211 pushes [PUSH 0; PUSH ECX] => (handle, 0).
 */
float object_get_actual_body_vitality(int object_handle, char param_2)
{
  char *obj;
  float body_max_vitality;
  float body_vitality;
  float scale;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  body_max_vitality = *(float *)(obj + 0x90);
  obj = (char *)object_get_and_verify_type(object_handle, -1);
  body_vitality = *(float *)(obj + 0x88);
  scale = body_vitality;
  if (param_2 == 0) {
    scale = FUN_000b55b0(1, (int)*(unsigned short *)(obj + 0x68));
    scale = scale * body_vitality;
  }
  return scale * body_max_vitality;
}

/* object_get_actual_shield_vitality (0x136ae0) — Compute scaled shield vitality
 * for an object.
 *
 * Returns shield_vitality * shield_max_vitality, optionally scaled by the
 * difficulty modifier for value_type 2 (shield vitality) when param_2 is 0.
 * When param_2 is non-zero, no difficulty scaling is applied.
 *
 * Object offsets:
 *   +0x68: team index (uint16_t), passed to FUN_000b55b0 as team arg
 *   +0x8c: shield vitality (float)
 *   +0x94: shield max vitality (float)
 *
 * Confirmed: PUSH -1; PUSH ESI; CALL 0x13d680 => object_get_and_verify_type x2.
 * Confirmed: MOV EAX,[EAX+0x94] stores shield_max_vitality in local [EBP-8].
 * Confirmed: FLD [EAX+0x8c] loads shield_vitality; FST [EBP-4] copies to local.
 * Confirmed: TEST CL,CL branches on param_2.
 * Confirmed: XOR ECX,ECX; MOV CX,[EAX+0x68] zero-extends team to int.
 * Confirmed: PUSH ECX; PUSH 2; CALL 0xb55b0 => FUN_000b55b0(2, team).
 * Confirmed: FMUL [EBP-4] then FMUL [EBP-8] for final result.
 * Confirmed: caller at 0x521f5 pushes [PUSH 0; PUSH EDX] => (handle, 0).
 */
float object_get_actual_shield_vitality(int object_handle, char param_2)
{
  char *obj;
  float shield_max_vitality;
  float shield_vitality;
  float scale;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  shield_max_vitality = *(float *)(obj + 0x94);
  obj = (char *)object_get_and_verify_type(object_handle, -1);
  shield_vitality = *(float *)(obj + 0x8c);
  scale = shield_vitality;
  if (param_2 == 0) {
    scale = FUN_000b55b0(2, (int)*(unsigned short *)(obj + 0x68));
    scale = scale * shield_vitality;
  }
  return scale * shield_max_vitality;
}

/* object_deplete_shield (0x136b40) — Trigger initial body-damage effect on an
 * object.
 *
 * If bit 3 of the damage flags byte (obj+0xb6) is not already set:
 *   1. Looks up the object's collision model tag (obje+0x7c -> 'coll')
 *   2. If the collision model has an effect reference at coll+0x1a4 (!= -1),
 *      creates that effect on the object via FUN_0009ec30
 *   3. Sets bit 3 of obj+0xb6
 *   4. Clears obj+0x98 (damage-related counter/timer)
 *   5. Calls FUN_00136a00 to set region "cannot be destroyed" bytes
 *
 * Confirmed: cdecl, 1 stack param (object_handle), void return.
 * Confirmed: PUSH -1; PUSH EDI; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: TEST AL,0x8 at 0x136b5b checks bit 3 of [ESI+0xb6].
 * Confirmed: tag_get('obje', [ESI]) at CALL 0x1ba140.
 * Confirmed: CMP EAX,-1 at 0x136b72 checks collision model index.
 * Confirmed: tag_get('coll', obje[0x7c]) at second CALL 0x1ba140.
 * Confirmed: 8 pushes [0,0,0,0,-1,EDI,EDI,ECX] before CALL 0x9ec30.
 * Confirmed: OR byte [ESI+0xb6],0x8 at 0x136b9d sets bit 3.
 * Confirmed: MOV [ESI+0x98],0x0 at 0x136ba8 clears dword.
 * Confirmed: MOV EAX,EDI; CALL 0x136a00 => FUN_00136a00(@EAX=handle, 0).
 */
void object_deplete_shield(int object_handle)
{
  char *obj;
  char *obje_tag;
  char *coll_tag;
  int coll_index;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if ((*(unsigned char *)(obj + 0xb6) & 8) == 0) {
    obje_tag = (char *)tag_get(0x6f626a65, *(int *)obj);
    coll_index = *(int *)(obje_tag + 0x7c);
    if (coll_index != -1) {
      coll_tag = (char *)tag_get(0x636f6c6c, coll_index);
      FUN_0009ec30(*(int *)(coll_tag + 0x1a4), object_handle, object_handle, -1,
                   0, 0, 0, 0); /* dup-args-ok: confirmed PUSH EDI,EDI */
    }
    *(unsigned char *)(obj + 0xb6) |= 8;
    *(int *)(obj + 0x98) = 0;
    FUN_00136a00(object_handle, 0);
  }
}

/* FUN_00136f40 (0x136f40) — Apply damage velocity and scoring effects to an
 * object based on its type and the damage parameters.
 *
 * Looks up the object's tag ('obje') and the damage effect tag ('jpt!') from
 * damage_data[0]. If the object's radius (obje+0x20) exceeds epsilon (0.0001f),
 * computes a velocity vector from the damage_data position (offset 0x34),
 * adds 0.45f to the Z component, normalizes, then scales by:
 *   jpt_tag+0x1f4 * obje+0x20 * (1/30.0f)
 *
 * Switches on the object type (obj+0x64):
 *   case 0 (biped):   calls FUN_001a4a70 (biped acceleration)
 *   case 1 (vehicle): optionally doubles velocity if jpt+0x1c8 flag set,
 *                     calls vehicle_accelerate (vehicle acceleration)
 *   case 2,3,4 (items): calls item_set_position with flag based on
 *                       damage_data+0x40 > 0.5f && jpt+0x1c8 flag
 *   case 5 (projectile): calls projectile_accelerate (projectile acceleration)
 *
 * After the switch, checks game state via game_engine_can_score:
 *   - If true and damage_data byte +4 >= 0: calls game_statistics_record_damage
 * for scoring, and conditionally FUN_000b56f0 if flags bit 0 is set.
 *   - Otherwise: calls game_engine_player_killed (player death tracking) via
 *     player_index_from_unit_index.
 *
 * Finally, if the object type is 0 or 1 (unit), forwards all parameters
 * to FUN_001b4dc0 for unit-specific damage handling.
 *
 * Register args: EBX = object_handle, ESI = damage_data pointer.
 * Stack args: flags, body_vitality, shield_vitality, param_4, param_5.
 *
 * Confirmed: PUSH -1; PUSH EBX; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: tag_get('obje', obj[0]) and tag_get('jpt!', damage_data[0]).
 * Confirmed: FLD [EDI+0x20]; FCOMP [0x253f44] tests radius > 0.0001f.
 * Confirmed: LEA EDX,[ESI+0x34] copies position vector from damage_data.
 * Confirmed: FADD [0x25614c] adds 0.45f to z component.
 * Confirmed: CALL 0x13010 => normalize3d; FSTP ST0 discards return.
 * Confirmed: FLD [EDX+0x1f4]; FMUL [EDI+0x20]; FMUL [0x2546a4] for scale.
 * Confirmed: jump table at 0x137158 with 6 entries for switch(obj_type).
 * Confirmed: PUSH ECX; FSTP [ESP] pattern for float arg to
 * game_statistics_record_damage. Confirmed: (1 << obj_type) & 3 gates call to
 * FUN_001b4dc0. Confirmed: 7 pushes [EBX,ESI,flags,body,shield,p4,p5] for
 * FUN_001b4dc0.
 */
void FUN_00136f40(int object_handle, void *damage_data, unsigned int flags,
                  float body_vitality, float shield_vitality, int param_4,
                  int param_5)
{
  int *obj;
  char *obje_tag;
  char *jpt_tag;
  char *dd;
  short obj_type;
  float scale;
  float direction[3];
  float velocity[3];
  char game_active;
  int player_handle;

  dd = (char *)damage_data;
  obj = (int *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *obj);
  jpt_tag = (char *)tag_get(0x6a707421, *(int *)dd);

  if (*(float *)(obje_tag + 0x20) > 0.0001f) {
    direction[2] = *(float *)(dd + 0x3c) + 0.45f;
    direction[0] = *(float *)(dd + 0x34);
    direction[1] = *(float *)(dd + 0x38);
    normalize3d(direction);

    scale =
      *(float *)(jpt_tag + 0x1f4) * *(float *)(obje_tag + 0x20) * 0.03333333f;
    obj_type = *(short *)((char *)obj + 0x64);
    velocity[0] = direction[0] * scale;
    velocity[1] = direction[1] * scale;
    velocity[2] = direction[2] * scale;

    switch (obj_type) {
    case 0:
    case 1:
      if (*(float *)(jpt_tag + 0x1f4) > 0.0001f &&
          (*(unsigned int *)((char *)obj + 0x1b4) & 0x800000) == 0) {
        if (obj_type == 0) {
          FUN_001a4a70(object_handle, velocity);
        } else if (obj_type == 1) {
          if ((*(unsigned char *)(jpt_tag + 0x1c8) & 0x20) != 0) {
            velocity[0] = velocity[0] + velocity[0];
            velocity[1] = velocity[1] + velocity[1];
            velocity[2] = velocity[2] + velocity[2];
          }
          vehicle_accelerate(object_handle, velocity);
        }
      }
      break;
    case 2:
    case 3:
    case 4:
      if (*(float *)(dd + 0x40) > 0.5f &&
          (*(unsigned char *)(jpt_tag + 0x1c8) & 0x20) != 0) {
        item_set_position(object_handle, velocity, 1);
      } else {
        item_set_position(object_handle, velocity, 0);
      }
      break;
    case 5:
      projectile_accelerate(object_handle, velocity);
      break;
    default:
      break;
    }
  }

  game_active = game_engine_can_score();
  if (game_active != 0 && *(signed char *)(dd + 0x4) >= 0) {
    game_statistics_record_damage(
      object_handle, body_vitality + shield_vitality, *(int *)(dd + 0x8),
      *(int *)(dd + 0xc), (int)*(unsigned short *)(dd + 0x10));
    if ((flags & 1) != 0) {
      FUN_000b56f0(object_handle, *(int *)(dd + 0x8), *(int *)(dd + 0xc),
                   (int)*(unsigned short *)(dd + 0x10));
    }
  } else {
    game_active = game_engine_can_score();
    if (game_active != 0) {
      player_handle = player_index_from_unit_index(object_handle);
      game_engine_player_killed(
        player_handle, object_handle, player_handle,
        1); /* dup-args-ok: confirmed PUSH EAX,EBX,EAX */
    }
  }

  if ((1 << (*(unsigned char *)((char *)obj + 0x64) & 0x1f)) & 3) {
    FUN_001b4dc0(object_handle, damage_data, flags, body_vitality,
                 shield_vitality, param_4, param_5);
  }
}

/* render_debug_object_damage (0x137370) — Damage debug overlay: display damage
 * vitality info for a targeted object, and handle picking a new target via
 * collision ray.
 *
 * When the damage debug flag (DAT_005a90c0) is set, this function:
 * 1. Formats a debug string with the targeted object's body/shield vitality,
 *    current damage, and recent damage values (offsets 0x90..0xa8).
 *    If no object is targeted (DAT_0046f070 == -1), shows "no object to debug".
 * 2. Sets up text drawing style/color and renders the string on screen.
 * 3. Checks if the spacebar (key 0x48) is held; if so, casts a collision ray
 *    from the camera position along a scaled direction to pick a new target.
 *    If the collision hits an object (type == 3), stores it as the new target.
 *
 * Confirmed: SUB ESP,0x864 for stack frame.
 * Confirmed: DAT_005a90c0 is the damage debug enable flag.
 * Confirmed: DAT_0046f070 is the current debug target object handle.
 * Confirmed: object_try_and_get_and_verify_type(handle, -1) at CALL 0x13d640.
 * Confirmed: tag_get_name(*obj) at CALL 0x1ba1f0.
 * Confirmed: strrchr(path, '\\') at CALL 0x1d9710 to get filename part.
 * Confirmed: _snprintf at CALL 0x1d9179 with 0x800 buffer.
 * Confirmed: draw_string_set_style_justify_flags(-1, 0, 0) at CALL 0x19b800.
 * Confirmed: draw_string_set_color(*(void**)0x2ee6c4) at CALL 0x19b640.
 * Confirmed: rasterizer_text_draw(&rect, NULL, NULL, 0, buf) at CALL 0x183e60.
 * Confirmed: input_key_is_down(0x48) = spacebar at CALL 0xcf560.
 * Confirmed: local_player_get_player_index at CALL 0xba3c0.
 * Confirmed: datum_get(*(data_t**)0x5aa6d4, handle) for player data at
 * 0x119320. Confirmed: Player object handle at player+0x34. Confirmed:
 * FUN_0014df70 collision test with 5 args at CALL 0x14df70. Confirmed:
 * collision type == 3 check at CMP word [EBP-0x64],0x3. Confirmed: collision
 * object handle at [EBP-0x2c] (offset 0x38 in result). Confirmed: assert string
 * "collision.type==_collision_result_object" at 0x29af70. Confirmed:
 * display_assert at CALL 0x8d9f0, system_exit at CALL 0x8e2f0.
 */
void render_debug_object_damage(void)
{
  typedef int(__cdecl * fn_snprintf_t)(char *, int, const char *, ...);
  fn_snprintf_t snprintf_fn;

  char string_buffer[2048];
  int16_t collision_result[40];
  float direction[3];
  int16_t rect[4];

  char *obj;
  char *tag_path;
  char *filename;
  int debug_handle;
  int player_index;
  char *player;
  int object_handle;

  if (*(char *)0x5a90c0 == 0)
    return;

  snprintf_fn = (fn_snprintf_t)0x1d9179;

  /* Copy screen rect from globals and adjust y by +0x140 */
  *(int *)&rect[0] = *(int *)0x506584;
  *(int *)&rect[2] = *(int *)0x506588;
  rect[1] = (int16_t)(rect[1] + 0x140);

  debug_handle = *(int *)0x46f070;
  if (debug_handle == -1) {
    /* No object targeted */
    snprintf_fn(string_buffer, 0x800,
                "no object to debug|n(point and press space)");
  } else {
    obj = (char *)object_try_and_get_and_verify_type(debug_handle, -1);
    if (obj == (char *)0) {
      *(int *)0x46f070 = -1;
    } else {
      /* Get tag path for this object's definition tag, extract filename */
      tag_path = (char *)tag_get_name(*(int *)obj);
      filename = strrchr(tag_path, 0x5c);
      snprintf_fn(
        string_buffer, 0x800,
        "%s|nbody %0.3f|n  current %0.3f|n  recent %0.3f|n"
        "shield %0.3f|n  current %0.3f|n  recent %0.3f|n",
        filename, (double)*(float *)(obj + 0x90),
        (double)*(float *)(obj + 0x9c), (double)*(float *)(obj + 0xa8),
        (double)*(float *)(obj + 0x94), (double)*(float *)(obj + 0x98),
        (double)*(float *)(obj + 0xa4));
    }
  }

  /* Set text drawing style: plain, left-justified, no flags */
  draw_string_set_style_justify_flags(-1, 0, 0);

  /* Set text color from global pointer */
  draw_string_set_color(*(void **)0x2ee6c4);

  /* Draw the debug text on screen */
  rasterizer_text_draw(&rect[0], (short *)0, (void *)0, 0, string_buffer);

  /* Check if spacebar is held (key 0x48) to pick a new damage debug target */
  if (input_key_is_down(0x48) == 0)
    return;

  /* Get local player's object handle, default to -1 */
  {
    uint16_t local_player_index;
    local_player_index = *(uint16_t *)0x506548;
    object_handle = -1;
    if ((int16_t)local_player_index != -1) {
      player_index = local_player_get_player_index(local_player_index);
      player = (char *)datum_get(*(data_t **)0x5aa6d4, player_index);
      object_handle = *(int *)(player + 0x34);
    }
  }

  /* Build direction vector: scale camera direction by global factor */
  direction[0] = *(float *)0x50655c * *(float *)0x25acf0;
  direction[1] = *(float *)0x506560 * *(float *)0x25acf0;
  direction[2] = *(float *)0x506564 * *(float *)0x25acf0;

  /* Cast a collision ray from camera position along the scaled direction */
  if (FUN_0014df70(0x81, (float *)0x506550, direction, object_handle,
                   collision_result) == 0)
    return;

  /* Verify collision result is an object hit */
  if (collision_result[0] != 3) {
    display_assert("collision.type==_collision_result_object",
                   "c:\\halo\\SOURCE\\objects\\damage.c", 0x794, 1);
    system_exit(-1);
  }

  /* Store the hit object as the new debug target */
  /* collision_result offset 0x38 = object handle (EBP-0x2c from EBP-0x64 base)
   */
  *(int *)0x46f070 = *(int *)((char *)collision_result + 0x38);
}

/* object_deplete_body (0x137540) — Set "body depleted" flag and detach child
 * units.
 *
 * If bit 2 of the damage flags byte (obj+0xb6) is not already set:
 *   1. Sets bit 2 of obj+0xb6
 *   2. Looks up the object's collision model tag (obje+0x7c -> 'coll')
 *   3. If the collision model has an effect reference at coll+0xb4 (!= -1),
 *      creates that effect on the object via FUN_0009ec30
 *   4. If the object type (obj+0x64) is 1 (biped), iterates the child object
 *      list (starting at obj+0xc8, next-sibling at child+0xc4). For each child
 *      of type 0 (biped) that meets the activation criteria:
 *        - child+0x1c8 == -1, or the global byte at 0x5aa890 is 0
 *        - child+0x2a0 != -1 (short)
 *      calls unit_set_actively_controlled_flag to set the actively-controlled
 *      flag on that child unit
 *   5. Calls object_deplete_shield to trigger the initial body-damage effect
 *
 * Confirmed: cdecl, 1 stack param (object_handle), void return.
 * Confirmed: PUSH -1; PUSH EDI; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: TEST AL,0x4 at 0x137561 checks bit 2 of [ESI+0xb6].
 * Confirmed: OR EAX,0x4; MOV [ESI+0xb6],AX sets bit 2.
 * Confirmed: tag_get('obje', [ESI]) at CALL 0x1ba140.
 * Confirmed: MOV EAX,[EAX+0x7c] reads collision model index.
 * Confirmed: tag_get('coll', coll_index) at second CALL 0x1ba140.
 * Confirmed: MOV ECX,[EAX+0xb4] reads effect index from coll tag.
 * Confirmed: 8 pushes [0,0,0,0,EBX,EDI,EDI,ECX] before CALL 0x9ec30.
 * Confirmed: CMP word [ESI+0x64],0x1 checks object type == 1.
 * Confirmed: MOV EDI,[ESI+0xc8] reads first child handle.
 * Confirmed: CMP word [ESI+0x64],0x0 checks child type == 0.
 * Confirmed: CMP [ESI+0x1c8],EBX checks child+0x1c8 == -1.
 * Confirmed: MOV AL,[0x5aa890]; TEST AL,AL checks global byte.
 * Confirmed: CMP word [ESI+0x2a0],BX checks child+0x2a0 != -1.
 * Confirmed: PUSH EDI; CALL 0x1a7f80 => unit_set_actively_controlled_flag.
 * Confirmed: MOV EDI,[ESI+0xc4] reads next sibling.
 * Confirmed: MOV EDI,[EBP+0x8] restores param_1 before object_deplete_shield
 * call. Confirmed: PUSH EDI; CALL 0x136b40 =>
 * object_deplete_shield(object_handle).
 */
void object_deplete_body(int object_handle)
{
  char *obj;
  char *obje_tag;
  char *coll_tag;
  int coll_index;
  int child_handle;
  char *child_obj;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  if ((*(unsigned short *)(obj + 0xb6) & 4) == 0) {
    *(unsigned short *)(obj + 0xb6) = *(unsigned short *)(obj + 0xb6) | 4;
    obje_tag = (char *)tag_get(0x6f626a65, *(int *)obj);
    coll_index = *(int *)(obje_tag + 0x7c);
    if (coll_index != -1) {
      coll_tag = (char *)tag_get(0x636f6c6c, coll_index);
      FUN_0009ec30(*(int *)(coll_tag + 0xb4), object_handle, object_handle, -1,
                   0, 0, 0, 0); /* dup-args-ok: confirmed PUSH EDI,EDI */
    }
    if (*(short *)(obj + 0x64) == 1) {
      child_handle = *(int *)(obj + 0xc8);
      while (child_handle != -1) {
        child_obj = (char *)object_get_and_verify_type(child_handle, -1);
        if (*(short *)(child_obj + 0x64) == 0 &&
            (*(int *)(child_obj + 0x1c8) == -1 || *(char *)0x5aa890 == 0) &&
            *(short *)(child_obj + 0x2a0) != -1) {
          unit_set_actively_controlled_flag(child_handle);
        }
        child_handle = *(int *)(child_obj + 0xc4);
      }
    }
    object_deplete_shield(object_handle);
  }
}

/* object_destroy (0x137620) — Object death/destruction handler.
 *
 * Called when an object is destroyed (killed). Performs the death sequence:
 *   1. Gets the object's tag definition ('obje') from the object data
 *   2. Calls object_deplete_body to handle death-related flag setting and child
 *      object detachment
 *   3. If the object has a collision model (obje+0x7c != -1), looks up the
 *      collision tag ('coll') and creates the destroy effect from coll+0xc8
 *      on the object via FUN_0009ec30
 *   4. Calls FUN_00136840 to recursively process child objects
 *   5. Calls FUN_00140cc0 for final destruction cleanup
 *
 * Confirmed: cdecl, 1 stack param (object_handle), void return.
 * Confirmed: PUSH -1; PUSH ESI; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: MOV EAX,[EAX] dereferences first dword of object (tag index).
 * Confirmed: PUSH EAX; PUSH 0x6f626a65; CALL 0x1ba140 => tag_get('obje', ...).
 * Confirmed: MOV EDI,EAX saves obje_tag in EDI.
 * Confirmed: PUSH ESI; CALL 0x137540 => object_deplete_body(object_handle).
 * Confirmed: MOV EAX,[EDI+0x7c] reads collision model index from obje tag.
 * Confirmed: CMP EAX,-1 at 0x13764b checks collision model presence.
 * Confirmed: tag_get('coll', collision_index) at second CALL 0x1ba140.
 * Confirmed: MOV ECX,[EAX+0xc8] reads destroy effect index from coll tag.
 * Confirmed: 8 pushes [0,0,0,0,-1,ESI,ESI,ECX] before CALL 0x9ec30.
 * Confirmed: PUSH ESI; CALL 0x136840 => FUN_00136840(object_handle).
 * Confirmed: PUSH ESI; CALL 0x140cc0 => object_delete(object_handle).
 */
void object_destroy(int object_handle)
{
  int *obj;
  char *obje_tag;
  char *coll_tag;
  int coll_index;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *obj);
  object_deplete_body(object_handle);
  coll_index = *(int *)(obje_tag + 0x7c);
  if (coll_index != -1) {
    coll_tag = (char *)tag_get(0x636f6c6c, coll_index);
    FUN_0009ec30(*(int *)(coll_tag + 0xc8), object_handle, object_handle, -1, 0,
                 0, 0, 0); /* dup-args-ok: confirmed PUSH ESI,ESI */
  }
  FUN_00136840(object_handle);
  object_delete(object_handle);
}

/* FUN_00137690 (0x137690) — Apply region destruction to an object.
 *
 * Called when a region on an object takes enough damage to be destroyed.
 * Looks up the object's collision model tag, validates the region_index,
 * then if the region hasn't already been destroyed:
 *   1. Gets the region element from the collision model's regions tag_block
 *      at coll+0x240 (element size 0x54)
 *   2. Creates the region's destroy effect (region+0x44) via FUN_0009ec30
 *   3. Calls object_permute_region to set the "~damaged" permutation on the
 * object's model for the destroyed region
 *   4. Propagates region flags (region+0x20) into the object's damage flags:
 *        bit 5 (0x20) -> obj+0xb6 |= 0x80
 *        bit 6 (0x40) -> obj+0xb6 |= 0x100
 *        bit 7 (0x80) -> obj+0xb7 |= 0x02
 *        bit 8 (0x100) -> obj+0xb7 |= 0x04
 *   5. If region has "forces body depletion" flag (bit 1 / 0x02), calls
 *      object_deplete_body to deplete the object's body
 *   6. Marks the region as destroyed in obj+0x124 (bitfield)
 *   7. Calls FUN_0013c6e0 to notify region-damage callbacks
 *
 * Confirmed: EDI = object_handle (register arg). Both callers in FUN_001377d0
 *   set EDI from [EBP+0x8] before calling:
 *     0x137a62: MOV EDI,[EBP+0x8]; PUSH EAX; CALL 0x137690
 *     0x137c26: PUSH EBX; CALL 0x137690; MOV EDI,[EBP+0x8] (restore after)
 * Confirmed: PUSH -1; PUSH EDI; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: MOV EAX,[EBX] -> PUSH EAX; PUSH 0x6f626a65 => tag_get('obje').
 * Confirmed: MOV EAX,[EAX+0x7c]; CMP EAX,-1 checks collision model.
 * Confirmed: PUSH EAX; PUSH 0x636f6c6c => tag_get('coll', coll_index).
 * Confirmed: TEST AX,AX; JL assert; CMP AX,0x8; JL skip_assert.
 * Confirmed: PUSH 0x54; PUSH ECX; ADD ESI,0x240; PUSH ESI =>
 * tag_block_get_element. Confirmed: 8 pushes [0,0,0,0,-1,EDI,EDI,[ESI+0x44]]
 * before CALL 0x9ec30. Confirmed: PUSH 1; PUSH EDX; PUSH 0x29b030; PUSH EDI =>
 * object_permute_region(obj,"~damaged",rgn,1). Confirmed: flag tests at
 * 0x20,0x40,0x80,0x100 on region+0x20 byte. Confirmed: TEST byte [ESI+0x20],0x2
 * gates call to object_deplete_body. Confirmed: OR word [EBX+0x124],AX sets
 * region-damaged bit. Confirmed: PUSH [ESI+0x20]; PUSH EDX; PUSH EDI =>
 * FUN_0013c6e0(obj,rgn,flags).
 */
void FUN_00137690(int object_handle, short region_index)
{
  int *obj;
  int obje_tag;
  int coll_tag;
  int coll_index;
  char *region;
  int region_idx;
  unsigned short damaged_regions;
  int bit;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (int)tag_get(0x6f626a65, *obj);
  coll_index = *(int *)(obje_tag + 0x7c);
  if (coll_index == -1) {
    return;
  }

  coll_tag = (int)tag_get(0x636f6c6c, coll_index);
  if (region_index < 0 || region_index >= 8) {
    display_assert("region_index>=0 && region_index<MAXIMUM_REGIONS_PER_OBJECT",
                   "c:\\halo\\SOURCE\\objects\\damage.c", 0x71a, 1);
    system_exit(-1);
  }

  region_idx = (int)region_index;
  damaged_regions = *(unsigned short *)((char *)obj + 0x124);
  bit = 1 << region_idx;
  if ((damaged_regions & bit) != 0) {
    return;
  }

  region =
    (char *)tag_block_get_element((void *)(coll_tag + 0x240), region_idx, 0x54);
  FUN_0009ec30(*(int *)(region + 0x44), object_handle,
               object_handle, /* dup-args-ok: confirmed PUSH EDI,EDI */
               -1, 0, 0, 0, 0);
  object_permute_region(object_handle, "~damaged", region_index, 1);

  if ((*(unsigned char *)(region + 0x20) & 0x20) != 0) {
    *(unsigned short *)((char *)obj + 0xb6) |= 0x80;
  }
  if ((*(unsigned char *)(region + 0x20) & 0x40) != 0) {
    *(unsigned short *)((char *)obj + 0xb6) |= 0x100;
  }
  if ((*(unsigned char *)(region + 0x20) & 0x80) != 0) {
    *(unsigned char *)((char *)obj + 0xb7) |= 0x02;
  }
  if ((*(unsigned int *)(region + 0x20) & 0x100) != 0) {
    *(unsigned char *)((char *)obj + 0xb7) |= 0x04;
  }
  if ((*(unsigned char *)(region + 0x20) & 0x02) != 0) {
    object_deplete_body(object_handle);
  }

  *(unsigned short *)((char *)obj + 0x124) |= (unsigned short)(1 << region_idx);
  FUN_0013c6e0(object_handle, region_index, *(unsigned int *)(region + 0x20));
}

/* object_cause_damage (0x137d20) — Apply damage to an object and its hierarchy.
 *
 * Main damage application entry point. Computes a random damage scale from
 * the damage effect tag, modifies it by game engine / difficulty / team
 * modifiers, iterates the object hierarchy to collect damaged objects, handles
 * unit passengers (recursive self-call), notifies player effects, then enters
 * a damage loop: for each damaged object, resolves collision model and
 * material, applies shield damage (FUN_00136bc0), body damage (FUN_001377d0),
 * commits results (FUN_00136f40), and optionally deletes the object.
 *
 * Confirmed: SUB ESP,0x74 at 0x137d23 → 0x74 bytes of locals.
 * Confirmed: tag_get(0x6a707421, *damage_params) at 0x137d34.
 * Confirmed: EDI = jpt_tag + 0x1c4 at 0x137d3f.
 * Confirmed: get_global_random_seed_address() takes 0 args at 0x137da5.
 * Confirmed: random_real_range(seed, min=*(jpt+0x1d4), max=*(jpt+0x1d8)) at
 * 0x137dab. Confirmed: FUN_00136890(@eax=object_handle) at 0x137e2c, 0x137e35.
 * Confirmed: game_engine_get_damage_multiplier(player_a, player_b) at 0x137e3b,
 * 2 cdecl args. Confirmed: ai_adjust_damage(player_index, damage_params,
 * &scale) at 0x137e19, 3 args. Confirmed: FUN_000a7a30(team_a, team_b) at
 * 0x137e54. Confirmed: FUN_000b5590(0) at 0x137e62, returns float on FPU.
 * Confirmed: object parent chain walk via +0xcc at 0x137ec6. Confirmed:
 * object_get_and_verify_type(handle, -1) at 0x137eec. Confirmed:
 * tag_get(0x6f626a65, *obj) at 0x137efb for object definition. Confirmed:
 * tag_get(0x636f6c6c, collision_ref) at 0x137f11 for collision model.
 * Confirmed: takes_body_damage = ~(coll_flags >> 4) & 1 at 0x137f18-0x137f22.
 * Confirmed: obj+0xa0 (widget handle) added to damaged list at 0x137f54.
 * Confirmed: unit hierarchy walk via +0xc8 at 0x137fa3.
 * Confirmed: recursive self-call at 0x138037 for passenger units.
 * BUG1-FIX: FUN_00136bc0 arg7 = &damage_scale (EBP-0xc) at 0x1382db-0x1382de.
 * BUG2-FIX: FUN_001377d0 arg10 = &body_damage (EBP-0x24) at 0x138354-0x138357.
 * BUG3-FIX: get_global_random_seed_address() is 0-arg; min/max from jpt tag.
 * BUG4-FIX: driver path passes *(unit+0x1c8) to FUN_000a3b80, not player_index.
 * BUG5-FIX: FUN_00136f40 last two args: (int)effect_ptr, material_index.
 */
void object_cause_damage(void *damage_params, int object_handle,
                         short node_index, short region_index,
                         short permutation_index, unsigned int flags)
{
  unsigned int *dp; /* damage_params as uint32 pointer */
  int jpt_tag;
  char *jpt_offset; /* damage effect data at jpt_tag + 0x1c4 */
  char was_modified; /* EBP+0xb: set when damage modifier applied */
  unsigned char takes_body_damage; /* EBP-0x1 */
  char damage_reported; /* EBP-0x2 */
  int damaged_object_count; /* EBP-0x8, used as short in some comparisons */
  float damage_scale; /* EBP-0xc: also serves as body_damage output for
                         FUN_00136bc0 */
  unsigned int damage_flags; /* EBP-0x10 */
  char *object_data; /* EBP-0x14 */
  /* jpt_offset stored at EBP-0x18 */
  void *material_data; /* EBP-0x1c */
  int current_object_handle; /* EBP-0x20 */
  float body_damage; /* EBP-0x24 */
  float shield_damage; /* EBP-0x28 */
  void *collision_model; /* EBP-0x2c */
  int material_index; /* EBP-0x30, only low 16 bits used */
  void *effect_ptr; /* EBP-0x34 */
  int damaged_object_indices[16]; /* EBP-0x74 */

  int i;
  int iter_handle;
  char *obj;
  unsigned int *coll_flags_ptr;
  char *unit_check;
  int child_handle;
  char *child_obj;
  int child_unit;
  unsigned int saved_flags;
  int driver_handle;
  int player_idx;
  short count_short;
  char cVar;
  float fVar;
  float modifier;
  int *seed;
  int obj_tag;
  int coll_ref;

  dp = (unsigned int *)damage_params;

  /* Get damage effect tag data */
  jpt_tag = (int)tag_get(0x6a707421, dp[0]);
  jpt_offset = (char *)jpt_tag + 0x1c4;
  was_modified = 0;
  takes_body_damage = 1;

  /* Assert region_index in valid range */
  if (region_index != -1 && (region_index < 0 || region_index >= 8)) {
    display_assert("region_index==NONE || (region_index>=0 && "
                   "region_index<MAXIMUM_REGIONS_PER_OBJECT)",
                   "c:\\halo\\SOURCE\\objects\\damage.c", 0x338, 1);
    system_exit(-1);
  }

  /* Track last damaged object for debug */
  if (dp[2] != (unsigned int)-1) {
    *(int *)0x46f070 = object_handle;
  }

  /* BUG3-FIX: get_global_random_seed_address takes ZERO args.
   * The min/max values come from the jpt tag at offsets 0x1d4 and 0x1d8
   * (relative to jpt_tag base, which is jpt_offset+0x10 and jpt_offset+0x14).
   */
  seed = get_global_random_seed_address();
  damage_scale = random_real_range(
    seed, *(float *)(jpt_offset + 0x10), /* min = *(jpt_tag + 0x1d4) */
    *(float *)(jpt_offset + 0x14)); /* max = *(jpt_tag + 0x1d8) */

  /* Compute initial damage scale:
   * scale = (random * damage_params[0x40] + (1.0 - damage_params[0x40]) *
   * jpt_offset[0xc])
   *         * damage_params[0x44] */
  damage_scale =
    (damage_scale * *(float *)((char *)dp + 0x40) +
     (1.0f - *(float *)((char *)dp + 0x40)) * *(float *)(jpt_offset + 0xc)) *
    *(float *)((char *)dp + 0x44);

  /* Check if attacker has a player owner; if so, apply AI damage modifier */
  if (dp[3] != (unsigned int)-1) {
    unit_check = (char *)object_try_and_get_and_verify_type(dp[3], 3);
    if (unit_check != (char *)0) {
      if (*(int *)(unit_check + 0x2d8) != -1) {
        unit_check =
          (char *)object_get_and_verify_type(*(int *)(unit_check + 0x2d8), 3);
      }
      i = *(int *)(unit_check + 0x1a8);
      if (i == -1) {
        i = *(int *)(unit_check + 0x1a4);
      }
      if (i != -1) {
        ai_adjust_damage(i, damage_params, &damage_scale);
      }
    }
  }

  /* Apply game engine or team-based damage modifiers */
  cVar = game_engine_running();
  if (cVar != 0) {
    /* Game engine path: get player indices for both sides */
    player_idx = FUN_00136890(object_handle);
    i = FUN_00136890(dp[3]);
    modifier = game_engine_get_damage_multiplier(i, player_idx);
    was_modified = 0;
  } else {
    /* Campaign path: check team allegiance for difficulty scale */
    if (*(short *)((char *)dp + 0x10) == -1)
      goto after_modifier;
    cVar =
      game_allegiance_get_team_is_friendly(*(short *)((char *)dp + 0x10), 1);
    if (cVar == 0)
      goto after_modifier;
    modifier = FUN_000b5590(0);
    was_modified = 1;
  }
  damage_scale = modifier * damage_scale;

after_modifier:
  /* Build list of objects to damage */
  i = 0;
  damaged_object_count = 0;
  damage_reported = 0;

  if ((dp[1] & 5) == 0) {
    /* Walk object parent chain collecting all linked objects */
    iter_handle = object_handle;
    while (iter_handle != -1) {
      if ((unsigned short)i >= 16) {
        display_assert(
          "damaged_object_count<sizeof(damaged_object_indices)/sizeof(long)",
          "c:\\halo\\SOURCE\\objects\\damage.c", 0x37e, 1);
        system_exit(-1);
      }
      damaged_object_indices[(short)(unsigned short)i] = iter_handle;
      i++;
      obj = (char *)object_get_and_verify_type(iter_handle, -1);
      iter_handle = *(int *)(obj + 0xcc);
    }
    damaged_object_count = i;
  } else {
    /* Direct damage: single object */
    damaged_object_count = 1;
    i = damaged_object_count;
    damaged_object_indices[0] = object_handle;
  }

  /* Get the root object's definition tag */
  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obj_tag = (int)tag_get(0x6f626a65, *(int *)obj);

  /* Check collision model for body damage flag */
  coll_ref = *(int *)(obj_tag + 0x7c);
  if (coll_ref != -1) {
    coll_flags_ptr = (unsigned int *)tag_get(0x636f6c6c, coll_ref);
    takes_body_damage = (unsigned char)((~(*coll_flags_ptr >> 4)) & 1);
  }

  /* Add widget object if present */
  if (*(int *)(obj + 0xa0) != -1) {
    if ((unsigned short)i >= 16) {
      display_assert(
        "damaged_object_count<sizeof(damaged_object_indices)/sizeof(long)",
        "c:\\halo\\SOURCE\\objects\\damage.c", 0x397, 1);
      system_exit(-1);
    }
    damaged_object_indices[(short)(unsigned short)i] = *(int *)(obj + 0xa0);
    i++;
    damaged_object_count = i;
  }

  /* Handle unit passengers: recursive damage for seated units */
  if ((*(unsigned char *)((char *)dp + 0x4) & 1) == 0 &&
      *(short *)(obj + 0x64) == 1) {
    char *unit_data;
    int unit_tag;

    unit_data = (char *)object_get_and_verify_type(object_handle, 3);
    unit_tag = (int)tag_get(0x756e6974, *(int *)unit_data);
    /* Modify damage multiplier for seated damage */
    *(float *)((char *)dp + 0x44) =
      (1.0f - *(float *)(jpt_offset + 0x18)) * *(float *)(unit_tag + 0x184);
    child_handle = *(int *)(unit_data + 0xc8);
    while (child_handle != -1) {
      child_obj = (char *)object_get_and_verify_type(child_handle, -1);
      if ((unsigned short)damaged_object_count >= 16) {
        display_assert(
          "damaged_object_count<sizeof(damaged_object_indices)/sizeof(long)",
          "c:\\halo\\SOURCE\\objects\\damage.c", 0x3ab, 1);
        system_exit(-1);
      }
      if (*(short *)(child_obj + 0x64) == 0) {
        child_unit = (int)object_get_and_verify_type(child_handle, 3);
        if (*(int *)(child_unit + 0x1c8) == -1) {
          /* No driver: check if this is the unit's gunner seat */
          if (child_handle != *(int *)(unit_data + 0x2d4))
            goto next_child;
          saved_flags = dp[1] | 0x20;
        } else {
          saved_flags = dp[1] & ~0x20u;
        }
        dp[1] = saved_flags;
        /* Recursive call for passenger */
        object_cause_damage(damage_params, child_handle, (short)-1,
                            (short)-1, /* dup-args-ok: all sentinel -1 */
                            (short)-1, 0);
        dp[1] = dp[1] & ~0x20u;
      }
    next_child:
      child_handle = *(int *)(child_obj + 0xc4);
    }
    i = damaged_object_count;
    *(unsigned int *)((char *)dp + 0x44) = 0x3f800000; /* 1.0f */
  }

  /* Notify player effects for each damaged unit */
  if ((short)(unsigned short)i > 0) {
    int *list_ptr;
    unsigned int list_count;
    list_ptr = damaged_object_indices;
    list_count = (unsigned int)(unsigned short)i;
    do {
      unit_check = (char *)object_try_and_get_and_verify_type(*list_ptr, 3);
      if (unit_check != (char *)0) {
        driver_handle = *(int *)(unit_check + 0x1c8);
        if (driver_handle != -1) {
          /* BUG4-FIX: driver present → pass driver_handle (the value at +0x1c8)
           * directly to FUN_000a3b80, NOT local_player_get_player_index(0).
           * Disasm: MOV EAX,[EAX+0x1c8]; ... JMP 0x1380bb; PUSH EAX. */
          FUN_000a3b80(driver_handle, damage_params, (char *)dp + 0x34,
                       *(float *)((char *)dp + 0x40), damage_scale);
        } else {
          /* No driver: check global flag */
          if (*(char *)0x5aa895 == 0)
            goto skip_player_effect;
          player_idx = local_player_get_player_index(0);
          FUN_000a3b80(player_idx, damage_params, (char *)dp + 0x34,
                       *(float *)((char *)dp + 0x40), damage_scale);
        }
      }
    skip_player_effect:
      list_ptr++;
      list_count--;
    } while (list_count != 0);
  }

  /* Main damage loop: apply damage to each object while scale > 0 */
  if (!(damage_scale > 0.0f))
    return;

  do {
    count_short = (short)damaged_object_count;
    damaged_object_count--;
    if (count_short < 1)
      return;

    current_object_handle = damaged_object_indices[(short)damaged_object_count];
    object_data = (char *)object_get_and_verify_type(current_object_handle, -1);
    obj_tag = (int)tag_get(0x6f626a65, *(int *)object_data);

    shield_damage = 0.0f;
    body_damage = 0.0f;
    effect_ptr = (void *)0;
    damage_flags = 0;
    material_index = -1;

    coll_ref = *(int *)(obj_tag + 0x7c);
    if (coll_ref == -1)
      goto after_collision_block;

    {
      unsigned char *coll_data;
      unsigned char is_forced;

      coll_data = (unsigned char *)tag_get(0x636f6c6c, coll_ref);
      is_forced = (unsigned char)((dp[1] >> 2) & 1);
      collision_model = (void *)coll_data;

      /* Look up node material if valid */
      if (node_index >= 0 && (int)node_index < *(int *)(coll_data + 0x28c)) {
        int node_elem;
        node_elem = (int)tag_block_get_element((int *)(coll_data + 0x28c),
                                               (int)node_index, 0x40);
        material_index = (material_index & 0xffff0000) |
                         (unsigned int)*(unsigned short *)(node_elem + 0x32);
      }

      /* Set modifier flag if applicable */
      if (was_modified != 0) {
        damage_flags = 0x20;
      }

      /* Check team allegiance for this specific object */
      if (*(short *)((char *)dp + 0x10) != -1) {
        cVar = game_allegiance_get_team_is_friendly(
          *(unsigned short *)(object_data + 0x68),
          *(unsigned short *)((char *)dp + 0x10));
        if (cVar == 0) {
          damage_flags |= 0x10;
        }
      }

      /* Resolve permutation/material data */
      if ((unsigned short)damaged_object_count == 0 && permutation_index >= 0 &&
          (int)permutation_index < *(int *)(coll_data + 0x234)) {
        material_data = (void *)tag_block_get_element(
          (int *)(coll_data + 0x234), (int)permutation_index, 0x48);
      } else if (*(short *)(coll_data + 4) >= 0 &&
                 (int)*(short *)(coll_data + 4) < *(int *)(coll_data + 0x234)) {
        material_data = (void *)tag_block_get_element(
          (int *)(coll_data + 0x234), (int)*(short *)(coll_data + 4), 0x48);
      } else {
        material_data = (void *)0x46f028;
      }

      /* Copy material type to damage params */
      *(unsigned short *)((char *)dp + 0x4c) =
        *(unsigned short *)((char *)material_data + 0x24);

      /* Force damage if debug override is active */
      if (*(char *)0x5aa897 != 0 && dp[2] != (unsigned int)-1) {
        is_forced = 1;
      }

      /* Check if object should be destroyed (depleted body) */
      if (((*(short *)jpt_offset == 2 &&
            unit_unsuspecting(current_object_handle, (char *)dp + 0x28) != 0 &&
            (*(unsigned char *)(object_data + 0xb7) & 8) == 0) ||
           is_forced != 0) &&
          (*(unsigned char *)(object_data + 0xb6) & 4) == 0) {
        *(unsigned int *)(object_data + 0x90) = 0;
        object_deplete_body(current_object_handle);
        damage_flags |= 0x41;
      }

      /* Apply shield damage (FUN_00136bc0) */
      if ((*(unsigned char *)((char *)dp + 0x4) & 0x20) == 0 &&
          (*(unsigned int *)(jpt_offset + 0x4) & 0x200) == 0 &&
          *(float *)(object_data + 0x8c) > 0.0f &&
          ((unsigned short)damaged_object_count == 0 ||
           (*coll_data & 1) != 0)) {
        /* BUG1-FIX: arg7 = &damage_scale (EBP-0xc), NOT &body_damage.
         * After the call, damage_scale holds the remaining body damage.
         * Disasm at 0x1382db: LEA ECX,[EBP-0xc]; PUSH ECX → last arg. */
        FUN_00136bc0(current_object_handle, collision_model, (int)material_data,
                     jpt_offset, damage_params, &damage_flags, &shield_damage,
                     &damage_scale);
        coll_data = (unsigned char *)collision_model;
      }

      /* Apply body damage (FUN_001377d0) */
      count_short = (short)damaged_object_count;
      if ((count_short == 0 ||
           (takes_body_damage != 0 && (*coll_data & 2) != 0)) &&
          (*(unsigned int *)(jpt_offset + 0x4) & 0x40) == 0) {
        int body_node;
        int body_region;
        unsigned int body_flags_mask;

        /* Check if shield-only damage should zero the scale */
        if ((*coll_data & 0x20) != 0 &&
            (*(unsigned int *)(jpt_offset + 0x4) & 0x20) == 0) {
          damage_scale = 0.0f;
        }

        if (count_short == 0) {
          body_node = (int)node_index;
          body_region = (int)region_index;
        } else {
          body_node = -1;
          body_region = -1;
        }

        body_flags_mask = (count_short != 0) ? 0 : flags;

        /* BUG2-FIX: arg10 = &body_damage (EBP-0x24), NOT &shield_damage.
         * Disasm at 0x138354: LEA EDX,[EBP-0x24]; PUSH EDX. */
        FUN_001377d0(current_object_handle, body_region, body_node,
                     body_flags_mask, coll_data, (int)material_data, jpt_offset,
                     damage_params, &damage_flags, &body_damage, &effect_ptr,
                     damage_scale);
        damaged_object_count = 0;
      }

      /* Report damage for the first object that actually took damage */
      if (damage_reported == 0 && (shield_damage > *(float *)0x253f44 ||
                                   body_damage > *(float *)0x253f44)) {
        if (shield_damage > body_damage) {
          /* Shield damage dominates: report shield material type */
          *(unsigned short *)((char *)dp + 0x4c) =
            *(unsigned short *)((char *)coll_data + 0xd2);
          *(unsigned int *)((char *)dp + 0x48) =
            *(unsigned int *)(object_data + 0x94);
        } else {
          /* Body damage dominates: clamp body vitality to [0, 1] */
          fVar = 0.0f;
          if (*(float *)(object_data + 0x90) >= 0.0f) {
            fVar = 1.0f;
            if (*(float *)(object_data + 0x90) <= 1.0f) {
              fVar = *(float *)(object_data + 0x90);
            }
          }
          *(float *)((char *)dp + 0x48) = fVar;
        }

        /* Debug logging */
        if (*(char *)0x5a90c0 != 0 &&
            current_object_handle == *(int *)0x46f070) {
          const char *mat_name;
          const char *tag_path;
          const char *filename;
          /* FUN_000b5490 takes 1 arg (material_type) and returns a name string.
           * Confirmed: ADD ESP,0x4 at 0x13845a (1 cdecl arg).
           * The material_data, damage doubles are pre-positioned on the stack
           * for console_printf varargs via MSVC lazy cleanup. */
          mat_name =
            FUN_000b5490(*(unsigned short *)((char *)material_data + 0x24));
          tag_path = tag_get_name(dp[0]);
          filename = strrchr(tag_path, 0x5c);
          console_printf(0, "%s: \"%s\" \"%s\" k=%0.2f S[%3.2f] B[%3.2f]",
                         filename + 1, mat_name, material_data,
                         (double)*(float *)((char *)dp + 0x40),
                         (double)shield_damage, (double)body_damage);
        }
        damage_reported = 1;
      }
    }

  after_collision_block:
    /* BUG5-FIX: FUN_00136f40 register args: EBX=object_handle,
     * ESI=damage_params. Stack args: flags, shield_damage(body_vitality),
     * body_damage(shield_vitality), (int)effect_ptr, material_index. Disasm at
     * 0x138489-0x1384a0: MOV ECX,[EBP-0x30] (material_index) → PUSH ECX
     * (last/arg7) MOV EDX,[EBP-0x34] (effect_ptr) → PUSH EDX (arg6) MOV
     * EAX,[EBP-0x24] (body_damage) → PUSH EAX (arg5) MOV ECX,[EBP-0x28]
     * (shield_damage) → PUSH ECX (arg4) MOV EDX,[EBP-0x10] (damage_flags) →
     * PUSH EDX (arg3) */
    FUN_00136f40(current_object_handle, damage_params, damage_flags,
                 shield_damage, body_damage, (int)effect_ptr, material_index);

    /* Delete object if flagged */
    if ((damage_flags & 4) != 0) {
      object_delete(current_object_handle);
    }
  } while (damage_scale > 0.0f);
}

/* FUN_00137170 (0x137170) — Build damage-effect marker arrays and fire an
 * effect for a damage impact.
 *
 * Sets up 5 forward-direction vectors ("normal", "incident",
 * "negative incident", "reflection", "gravity") and 5 copies of the impact
 * position, then calls either effect_new_attached_from_markers (attached
 * effect) when both the object handle and marker index are valid, or
 * effect_new_unattached_from_markers (unattached effect) otherwise.
 *
 * The incident direction (@EAX) is normalized in-place; if it has zero
 * length, it is replaced by the global forward vector (*(0x31fc3c)).
 * If the surface normal (@ECX/EDI) is NULL, a direction is computed from
 * position minus the object's world position, and if that also has zero
 * length, the object's forward vector (obj+0x24) is used instead.
 *
 * Register args:
 *   @EAX = incident_direction (float[3], normalized in function)
 *   @ECX = surface_normal (float[3], or NULL)
 *   @ESI = object_handle (int)
 * Stack args:
 *   [EBP+0x8]  = effect_tag_index (int, collision model tag index)
 *   [EBP+0xC]  = marker_index (short, -1 for unattached)
 *   [EBP+0x10] = position (float[3], world position of impact)
 *
 * Confirmed: MOV EDI,ECX at 0x13717e saves @ECX (surface_normal) to EDI.
 * Confirmed: TEST EDI,EDI at 0x1371fb branches on NULL surface_normal.
 * Confirmed: MOV EBX,[EBP+0x10] at 0x13717a loads position param.
 * Confirmed: normalize3d at CALL 0x13010 normalizes incident direction.
 * Confirmed: FCOMP [0x2533c0] compares magnitude with 0.0f.
 * Confirmed: FMUL [0x255e94] multiplies by -1.0f for negative incident.
 * Confirmed: FUN_001412f0 at CALL 0x1412f0 = object_get_world_position.
 * Confirmed: FUN_0010c8e0 at CALL 0x10c8e0 = reflect vector.
 * Confirmed: CMP ESI,-1 at 0x1372f9 + CMP AX,0xffff at 0x137303 gate
 *            between effect_new_attached_from_markers and
 * effect_new_unattached_from_markers. Confirmed: 12 pushes + ADD ESP,0x30 for
 * both effect calls. Confirmed: *(0x31fc50) = global gravity/down vector
 * (0,0,-1). Confirmed: *(0x31fc3c) = global forward vector (1,0,0). Confirmed:
 * *(0x31fc38) = translational velocity ptr for
 * effect_new_unattached_from_markers.
 */
/* Unported: only caller (FUN_001377d0) is unported and passes 3 register
 * args (@<eax>, @<ecx>, @<esi>) that our cdecl C function cannot receive.
 * Original XBE code runs and correctly calls our ported callees. */
#if 0
void FUN_00137170(float *incident_direction, float *surface_normal,
                  int object_handle, int effect_tag_index,
                  short marker_index, float *position)
{
  float marker_points[15];
  float forward_vectors[15];
  char *effect_names[5];
  float obj_pos[3];
  float normal[3];
  float direction[3];
  float mag;
  int i;
  float *dst;
  char *obj;

  effect_names[0] = "normal";
  effect_names[1] = "incident";
  effect_names[2] = "negative incident";
  effect_names[3] = "reflection";
  effect_names[4] = "gravity";

  /* Copy gravity vector directly into forward_vectors entry 4 */
  forward_vectors[12] = *(float *)*(int *)0x31fc50;
  forward_vectors[13] = *(float *)(*(int *)0x31fc50 + 4);
  forward_vectors[14] = *(float *)(*(int *)0x31fc50 + 8);

  /* Copy and normalize the incident direction */
  normal[0] = incident_direction[0];
  normal[1] = incident_direction[1];
  normal[2] = incident_direction[2];
  mag = normalize3d(normal);
  if (mag == 0.0f) {
    normal[0] = *(float *)*(int *)0x31fc3c;
    normal[1] = *(float *)(*(int *)0x31fc3c + 4);
    normal[2] = *(float *)(*(int *)0x31fc3c + 8);
  }

  /* Build forward vectors:
   * [1] = incident (normal * -1.0)
   * [2] = negative incident (normal copy)
   */
  forward_vectors[3] = normal[0] * -1.0f;
  forward_vectors[6] = normal[0];
  forward_vectors[7] = normal[1];
  forward_vectors[4] = normal[1] * -1.0f;
  forward_vectors[8] = normal[2];
  forward_vectors[5] = normal[2] * -1.0f;

  if (surface_normal == (float *)0) {
    /* No surface normal provided: compute direction from position */
    object_get_world_position(object_handle, (vector3_t *)obj_pos);
    direction[0] = position[0] - obj_pos[0];
    direction[1] = position[1] - obj_pos[1];
    direction[2] = position[2] - obj_pos[2];
    mag = normalize3d(direction);
    if (mag == 0.0f) {
      obj = (char *)object_get_and_verify_type(object_handle, -1);
      direction[0] = *(float *)(obj + 0x24);
      direction[1] = *(float *)(obj + 0x28);
      direction[2] = *(float *)(obj + 0x2c);
    }
    /* forward[0] = computed direction ("normal") */
    forward_vectors[0] = direction[0];
    forward_vectors[1] = direction[1];
    forward_vectors[2] = direction[2];
    /* Reflect normal about direction */
    FUN_0010c8e0(normal, direction, &forward_vectors[9]);
  } else {
    /* forward[0] = surface normal ("normal") */
    forward_vectors[0] = surface_normal[0];
    forward_vectors[1] = surface_normal[1];
    forward_vectors[2] = surface_normal[2];
    /* Reflect normal about surface normal */
    FUN_0010c8e0(normal, surface_normal, &forward_vectors[9]);
  }

  /* Fill all 5 marker positions with the same impact position */
  dst = marker_points;
  for (i = 5; i != 0; i--) {
    *dst = *position;
    dst[1] = position[1];
    dst[2] = position[2];
    dst += 3;
  }

  if (object_handle != -1 && marker_index != -1) {
    effect_new_attached_from_markers(effect_tag_index, object_handle, object_handle, /* dup-args-ok: confirmed PUSH ESI,ESI */
                 (uint16_t)marker_index, 5, (void *)effect_names,
                 marker_points, forward_vectors, 1.0f, 0.0f, 0.0f, 0.0f);
    return;
  }
  effect_new_unattached_from_markers(effect_tag_index, object_handle,
               *(float **)0x31fc38, 5, (void *)effect_names,
               marker_points, forward_vectors, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}
#endif

/* object_damage_update (0x1384e0) — Per-tick damage state update for an object.
 *
 * Handles three main subsystems each tick:
 * 1. Pending damage application: if damage-pending flags (bits 5,6,13 of +0xb6)
 *    are set and the object is not invincible (bit 2), applies self-damage via
 *    the game difficulty's default damage effect.
 * 2. Shield vitality management:
 *    - Overshield charging (bit 4): increments shield toward 3.0f at 1/30 per
 * tick.
 *    - Overshield drain: if shield > 1.0f and game_engine_running(), drains at
 * a fixed rate (0.00074074074f/tick), notifying the player of lost shield
 * energy.
 *    - Shield recharge: if shield < 1.0f and delay counter is zero, recharges
 * at the collision model's recharge rate scaled by difficulty modifier (type
 * 3).
 * 3. Damage stun counters (+0xb0 body, +0xac shield): after incrementing,
 * decays current and recent damage values by 1/60 per tick once counter
 * thresholds are reached (0 for current, 60 for recent). Resets counter to -1
 * when both current and recent damage reach 0.0f.
 *
 * Confirmed: PUSH -1; PUSH EDI; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: tag_get('obje', obj[0]) then tag_get('coll', obje[0x7c]).
 * Confirmed: FLD [ESI+0x8c]; FCOMP [0x2533c0] checks max_body_vitality > 0.0f.
 * Confirmed: TEST AH,0x41; JNZ skips when body <= 0.0f.
 * Confirmed: TEST CL,0x10 checks overshield charging flag (bit 4).
 * Confirmed: FADD [0x2546a4] adds 1/30 per tick for overshield charge.
 * Confirmed: FCOMP [0x254644] compares with 3.0f overshield cap.
 * Confirmed: TEST AH,0x1; JNZ for shield < 3.0f.
 * Confirmed: TEST AH,0x41; JNZ at 0x138649 for shield <= 1.0f.
 * Confirmed: CALL 0xa8e30 = game_engine_running().
 * Confirmed: CALL 0xba500 = player_index_from_unit_index(object_handle).
 * Confirmed: FLD [0x29b18c]; FCOMP [EBP-4] compares drain_rate with excess.
 * Confirmed: CALL 0xd7cd0 = FUN_000d7cd0(player_index, float_amount).
 * Confirmed: TEST AH,0x5; JP at 0x1386d0 for shield >= 1.0f skip.
 * Confirmed: MOV AX,[ESI+0xb4]; TEST AX,AX; JNZ for delay counter check.
 * Confirmed: DEC AX at 0x138774 for delay counter decrement.
 * Confirmed: CALL 0xb55b0 = FUN_000b55b0(3, team) for difficulty scale.
 * Confirmed: FMUL [EBP-4] scales recharge rate by difficulty.
 * Confirmed: CALL 0x1369e0 with @EAX = object_handle for shield recharge
 * effect. Confirmed: CALL 0x136a00 with @EAX = object_handle, param_1=1.
 * Confirmed: INC EAX; TEST EAX,EAX; JL for stun counter >= 0 check.
 * Confirmed: CMP EAX,0x3c; JL for stun counter >= 60 check.
 * Confirmed: FSUB [0x25634c] subtracts 1/60 per tick for damage decay.
 * Confirmed: TEST AH,0x44; JP for equality-with-zero check on damage values.
 * Confirmed: damage_data_new at 0x136750 and object_cause_damage at 0x137d20.
 * Confirmed: game_globals_get at 0x18e450, tag_block_get_element at 0x19b210.
 */
void object_damage_update(int object_handle)
{
  char *obj;
  char *obje_tag;
  int coll_tag_index;
  char *coll_tag;
  unsigned short flags;
  unsigned int damage_flags;
  char damage_params[0x58];
  float local_8;
  char *difficulty;
  int damage_tag_index;
  int player_index;
  float shield;
  float new_shield;
  int counter;
  float fVar1;
  float fVar3;

  obj = (char *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *(int *)obj);
  coll_tag_index = *(int *)(obje_tag + 0x7c);
  if (coll_tag_index == -1)
    return;
  coll_tag = (char *)tag_get(0x636f6c6c, coll_tag_index);
  if (coll_tag == (char *)0)
    return;

  /* --- Pending damage application --- */
  flags = *(unsigned short *)(obj + 0xb6);
  if ((flags & 0x2000) != 0 || (flags & 0x60) != 0) {
    if ((flags & 4) == 0) {
      difficulty = (char *)tag_block_get_element(
        (char *)game_globals_get() + 0x188, 0, 0x98);
      damage_tag_index = *(int *)(difficulty + 0x1c);
      if (damage_tag_index != -1) {
        damage_data_new(damage_params, damage_tag_index);
        flags = *(unsigned short *)(obj + 0xb6);
        damage_flags = *(unsigned int *)(damage_params + 0x4) | 4;
        *(float *)(damage_params + 0x40) = 1.0f;
        if ((flags & 0x40) != 0) {
          damage_flags = damage_flags | 0x10;
        }
        *(unsigned int *)(damage_params + 0x4) = damage_flags;
        if ((*(unsigned short *)(obj + 0xb6) & 0x2000) != 0) {
          *(unsigned int *)(damage_params + 0x4) =
            *(unsigned int *)(damage_params + 0x4) | 0x80;
        }
        object_cause_damage(damage_params, object_handle, -1, -1, -1, 0);
      }
    }
    *(unsigned short *)(obj + 0xb6) &= 0xdf9f;
  }

  /* --- Shield vitality management --- */
  *(unsigned char *)(obj + 0xb7) &= 0xef;
  flags = *(unsigned short *)(obj + 0xb6);

  if (!(*(float *)(obj + 0x8c) > 0.0f))
    goto stun_body;
  if ((flags & 4) != 0)
    goto stun_body;

  if ((flags & 0x10) != 0) {
    /* Overshield charging: increment shield toward 3.0f */
    shield = *(float *)(obj + 0x94) + 0.033333335f;
    *(float *)(obj + 0x94) = shield;
    if (!(shield < 3.0f)) {
      *(float *)(obj + 0x94) = 3.0f;
      *(unsigned short *)(obj + 0xb6) = flags & 0xffef;
    } else {
      *(unsigned short *)(obj + 0xb6) = flags | 0x1000;
    }
    goto stun_body;
  }

  /* Non-charging path: check overshield drain or recharge */
  if (*(float *)(obj + 0x94) > 1.0f && game_engine_running()) {
    /* Overshield drain */
    player_index = player_index_from_unit_index(object_handle);
    local_8 = *(float *)(obj + 0x94) - 1.0f;
    if (!(0.00074074074f <= local_8)) {
      *(float *)(obj + 0x94) = 1.0f;
      FUN_000d7cd0(player_index, local_8);
    } else {
      *(float *)(obj + 0x94) = *(float *)(obj + 0x94) - 0.00074074074f;
      FUN_000d7cd0(player_index, 0.00074074074f);
    }
    goto stun_body;
  }

  /* Shield recharge (shield < 1.0f) */
  if (!(*(float *)(obj + 0x94) < 1.0f))
    goto stun_body;

  if (*(short *)(obj + 0xb4) == 0) {
    local_8 = *(float *)(coll_tag + 0x1c0);
    local_8 = FUN_000b55b0(3, (int)*(unsigned short *)(obj + 0x68)) * local_8;
    if ((*(unsigned char *)(obj + 0xb6) & 8) != 0) {
      FUN_001369e0(object_handle, *(int *)(coll_tag + 0x1b4));
      *(unsigned char *)(obj + 0xb6) &= 0xf7;
      FUN_00136a00(object_handle, 1);
    }
    *(unsigned char *)(obj + 0xb7) |= 0x10;
    new_shield = local_8 + *(float *)(obj + 0x94);
    flags = *(unsigned short *)(obj + 0xb6);
    *(float *)(obj + 0x94) = new_shield;
    if (new_shield > 1.0f) {
      *(float *)(obj + 0x94) = 1.0f;
      *(unsigned short *)(obj + 0xb6) = flags & 0xefff;
    }
  } else {
    *(short *)(obj + 0xb4) = *(short *)(obj + 0xb4) - 1;
  }

stun_body:
  /* --- Body stun counter decay --- */
  counter = *(int *)(obj + 0xb0);
  if (counter != -1) {
    counter = counter + 1;
    *(int *)(obj + 0xb0) = counter;
    if (counter >= 0) {
      *(float *)(obj + 0x9c) = *(float *)(obj + 0x9c) - 0.016666668f;
    }
    if (counter >= 60) {
      *(float *)(obj + 0xa8) = *(float *)(obj + 0xa8) - 0.016666668f;
    }
    /* Clamp body current damage to >= 0 */
    if (0.0f <= *(float *)(obj + 0x9c)) {
      fVar1 = *(float *)(obj + 0x9c);
    } else {
      fVar1 = 0.0f;
    }
    *(float *)(obj + 0x9c) = fVar1;
    /* Clamp body recent damage to >= 0 */
    if (0.0f <= *(float *)(obj + 0xa8)) {
      fVar3 = *(float *)(obj + 0xa8);
    } else {
      fVar3 = 0.0f;
    }
    local_8 = fVar3;
    *(float *)(obj + 0xa8) = fVar3;
    if (fVar1 == 0.0f && local_8 == 0.0f) {
      *(int *)(obj + 0xb0) = -1;
    }
  }

  /* --- Shield stun counter decay --- */
  counter = *(int *)(obj + 0xac);
  if (counter != -1) {
    counter = counter + 1;
    *(int *)(obj + 0xac) = counter;
    if (counter >= 0) {
      *(float *)(obj + 0x98) = *(float *)(obj + 0x98) - 0.016666668f;
    }
    if (counter >= 60) {
      *(float *)(obj + 0xa4) = *(float *)(obj + 0xa4) - 0.016666668f;
    }
    /* Clamp shield current damage to >= 0 */
    if (0.0f <= *(float *)(obj + 0x98)) {
      fVar1 = *(float *)(obj + 0x98);
    } else {
      fVar1 = 0.0f;
    }
    *(float *)(obj + 0x98) = fVar1;
    /* Clamp shield recent damage to >= 0 */
    if (0.0f <= *(float *)(obj + 0xa4)) {
      fVar3 = *(float *)(obj + 0xa4);
    } else {
      fVar3 = 0.0f;
    }
    local_8 = fVar3;
    *(float *)(obj + 0xa4) = fVar3;
    if (fVar1 == 0.0f && local_8 == 0.0f) {
      *(int *)(obj + 0xac) = -1;
    }
  }
}

/* Apply area-of-effect damage to nearby objects (0x138e30).
 * Resolves the 'jpt!' damage effect tag, searches for objects within the
 * effect radius via object_find_in_radius, applies damage to each via
 * FUN_00138900, then processes breakable surfaces via FUN_00146be0. */
void FUN_00138e30(void *damage_params, int target_index)
{
  char *tag;
  int16_t count;
  int results[64];
  uint16_t i;

  (void)target_index;
  tag = (char *)tag_get(0x6a707421, *(int *)damage_params);
  count = object_find_in_radius(0, 0, (char *)damage_params + 0x14,
                                (float *)((char *)damage_params + 0x1c),
                                *(float *)(tag + 4), results, 0x40);
  if (count > 0) {
    for (i = 0; i < (uint16_t)count; i++)
      FUN_00138900(damage_params, results[i], 0);
  }
  FUN_00146be0(damage_params);
}

/* FUN_00138eb0 — dispatch object deletion callbacks.
 * Iterates through a table of 3 function pointers at 0x3235f0 and calls
 * each with the object handle. These callbacks clean up references to the
 * object in various subsystems (actors, players, AI, etc.) before deletion.
 *
 * Table at 0x3235f0:
 *   [0] = 0x13d8b0 — clears object references in other objects
 *   [1] = 0x40700  — actor cleanup and player notifications
 *   [2] = 0xbb220  — player object reference cleanup
 */
void FUN_00138eb0(int object_handle)
{
  void (**table)(int);
  int i;

  table = (void (**)(int))0x3235f0;
  i = 3;
  do {
    (*table)(object_handle);
    table++;
    i--;
  } while (i != 0);
}

/* FUN_00138ee0 (0x138ee0) — Texture cache hardware format lookup with RDTSC
 * profiling. Wraps xbox_texture_cache_get_hardware_format(hardware_format, 1,
 * 1) between RDTSC start/stop calls for performance measurement.
 *
 * Confirmed: CALL 0x916e0 (RDTSC start) takes no args.
 * Confirmed: PUSH 1 / PUSH 1 / PUSH [EBP+8] then CALL 0x1bf570 (cdecl, 3 args).
 * Confirmed: ADD ESP,0xc after call (cdecl cleanup for 3 args).
 * Confirmed: CALL 0x91710 (RDTSC stop) takes no args.
 * Confirmed: return value in ESI is the result of
 * xbox_texture_cache_get_hardware_format.
 */
int FUN_00138ee0(int hardware_format)
{
  int result;

  profile_texture_start();
  result =
    (int)xbox_texture_cache_get_hardware_format((void *)hardware_format, 1, 1);
  profile_texture_end();
  return result;
}
