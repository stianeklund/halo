/* object_restore_body (0x136790) — Check if object body vitality is below full and
 * restore it to 1.0f if so.
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

/* object_double_charge_shield (0x1367e0) — Check if object shield vitality is at or below full
 * and prepare it for shield regeneration if so.
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

/* object_can_take_damage (0x1368e0) — Clear bit 3 of object+0xb7 flags byte for all
 * children/widgets of a given parent handle.
 *
 * Iterates using FUN_000ce450 (first) / FUN_000ce320 (next) to enumerate
 * associated objects. For each, clears bit 3 (AND 0xf7) of the flags byte
 * at offset 0xb7. This is the same byte modified by object_set_ranged_attack_inhibited (bit 0)
 * and object_set_melee_attack_inhibited (bit 7).
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

/* object_get_maximum_body_vitality (0x136930) — Set bit 3 of object+0xb7 flags byte for all
 * children/widgets of a given parent handle.
 *
 * Complement of object_can_take_damage which clears the same bit. Iterates using
 * FUN_000ce450 (first) / FUN_000ce320 (next) to enumerate associated
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

/* object_set_ranged_attack_inhibited (0x136980) — Set or clear the damage-invincible bit on an
 * object.
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

/* object_set_melee_attack_inhibited (0x1369b0) — Set or clear bit 7 of object+0xb6 flags byte.
 *
 * Confirmed: identical structure to object_set_ranged_attack_inhibited but targets offset 0xb6 bit 7.
 * Confirmed: OR byte [EAX+0xb6],0x80 (set) at 0x1369cd.
 * Confirmed: AND byte [EAX+0xb6],0x7f (clear) at 0x1369d6.
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
 * both object_handle and parent_handle, marker=-1, and zeros for remaining args.
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
  FUN_0009ec30(effect_tag_index, object_handle, object_handle, -1, 0, 0, 0, 0); /* dup-args-ok: confirmed PUSH EAX,EAX */
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

/* object_get_actual_body_vitality (0x136a80) — Compute scaled body vitality for an object.
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

/* object_get_actual_shield_vitality (0x136ae0) — Compute scaled shield vitality for an object.
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

/* object_deplete_shield (0x136b40) — Trigger initial body-damage effect on an object.
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
      FUN_0009ec30(*(int *)(coll_tag + 0x1a4), object_handle, object_handle, -1, 0, 0, 0, 0); /* dup-args-ok: confirmed PUSH EDI,EDI */
    }
    *(unsigned char *)(obj + 0xb6) |= 8;
    *(int *)(obj + 0x98) = 0;
    FUN_00136a00(object_handle, 0);
  }
}

/* object_deplete_body (0x137540) — Set "body depleted" flag and detach child units.
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
 * Confirmed: MOV EDI,[EBP+0x8] restores param_1 before object_deplete_shield call.
 * Confirmed: PUSH EDI; CALL 0x136b40 => object_deplete_shield(object_handle).
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
      FUN_0009ec30(*(int *)(coll_tag + 0xb4), object_handle, object_handle, -1, 0, 0, 0, 0); /* dup-args-ok: confirmed PUSH EDI,EDI */
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
    FUN_0009ec30(*(int *)(coll_tag + 0xc8), object_handle, object_handle, -1, 0, 0, 0, 0); /* dup-args-ok: confirmed PUSH ESI,ESI */
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
 *   3. Calls object_permute_region to set the "~damaged" permutation on the object's
 *      model for the destroyed region
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
 * Confirmed: PUSH 0x54; PUSH ECX; ADD ESI,0x240; PUSH ESI => tag_block_get_element.
 * Confirmed: 8 pushes [0,0,0,0,-1,EDI,EDI,[ESI+0x44]] before CALL 0x9ec30.
 * Confirmed: PUSH 1; PUSH EDX; PUSH 0x29b030; PUSH EDI => object_permute_region(obj,"~damaged",rgn,1).
 * Confirmed: flag tests at 0x20,0x40,0x80,0x100 on region+0x20 byte.
 * Confirmed: TEST byte [ESI+0x20],0x2 gates call to object_deplete_body.
 * Confirmed: OR word [EBX+0x124],AX sets region-damaged bit.
 * Confirmed: PUSH [ESI+0x20]; PUSH EDX; PUSH EDI => FUN_0013c6e0(obj,rgn,flags).
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
    display_assert(
      "region_index>=0 && region_index<MAXIMUM_REGIONS_PER_OBJECT",
      "c:\\halo\\SOURCE\\objects\\damage.c", 0x71a, 1);
    system_exit(-1);
  }

  region_idx = (int)region_index;
  damaged_regions = *(unsigned short *)((char *)obj + 0x124);
  bit = 1 << region_idx;
  if ((damaged_regions & bit) != 0) {
    return;
  }

  region = (char *)tag_block_get_element((void *)(coll_tag + 0x240),
                                          region_idx, 0x54);
  FUN_0009ec30(*(int *)(region + 0x44), object_handle, object_handle, /* dup-args-ok: confirmed PUSH EDI,EDI */
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
 *                     calls FUN_001b5c90 (vehicle acceleration)
 *   case 2,3,4 (items): calls item_set_position with flag based on
 *                       damage_data+0x40 > 0.5f && jpt+0x1c8 flag
 *   case 5 (projectile): calls projectile_accelerate (projectile acceleration)
 *
 * After the switch, checks game state via FUN_000a8e40:
 *   - If true and damage_data byte +4 >= 0: calls FUN_000b56e0 for scoring,
 *     and conditionally FUN_000b56f0 if flags bit 0 is set.
 *   - Otherwise: calls FUN_000af660 (player death tracking) via
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
 * Confirmed: PUSH ECX; FSTP [ESP] pattern for float arg to FUN_000b56e0.
 * Confirmed: (1 << obj_type) & 3 gates call to FUN_001b4dc0.
 * Confirmed: 7 pushes [EBX,ESI,flags,body,shield,p4,p5] for FUN_001b4dc0.
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

    scale = *(float *)(jpt_tag + 0x1f4) * *(float *)(obje_tag + 0x20)
            * 0.03333333f;
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
          FUN_001b5c90(object_handle, velocity);
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

  game_active = FUN_000a8e40();
  if (game_active != 0 && *(signed char *)(dd + 0x4) >= 0) {
    FUN_000b56e0(object_handle, body_vitality + shield_vitality,
                 *(int *)(dd + 0x8), *(int *)(dd + 0xc),
                 (int)*(unsigned short *)(dd + 0x10));
    if ((flags & 1) != 0) {
      FUN_000b56f0(object_handle, *(int *)(dd + 0x8),
                   *(int *)(dd + 0xc),
                   (int)*(unsigned short *)(dd + 0x10));
    }
  } else {
    game_active = FUN_000a8e40();
    if (game_active != 0) {
      player_handle = player_index_from_unit_index(object_handle);
      FUN_000af660(player_handle, object_handle, player_handle, 1); /* dup-args-ok: confirmed PUSH EAX,EBX,EAX */
    }
  }

  if ((1 << (*(unsigned char *)((char *)obj + 0x64) & 0x1f)) & 3) {
    FUN_001b4dc0(object_handle, damage_data, flags, body_vitality,
                 shield_vitality, param_4, param_5);
  }
}

/* FUN_00137170 (0x137170) — Build damage-effect marker arrays and fire an
 * effect for a damage impact.
 *
 * Sets up 5 forward-direction vectors ("normal", "incident",
 * "negative incident", "reflection", "gravity") and 5 copies of the impact
 * position, then calls either effect_new_attached_from_markers (attached effect) when both the
 * object handle and marker index are valid, or effect_new_unattached_from_markers (unattached
 * effect) otherwise.
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
 *            between effect_new_attached_from_markers and effect_new_unattached_from_markers.
 * Confirmed: 12 pushes + ADD ESP,0x30 for both effect calls.
 * Confirmed: *(0x31fc50) = global gravity/down vector (0,0,-1).
 * Confirmed: *(0x31fc3c) = global forward vector (1,0,0).
 * Confirmed: *(0x31fc38) = translational velocity ptr for effect_new_unattached_from_markers.
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

/* render_debug_object_damage (0x137370) — Damage debug overlay: display damage vitality info
 * for a targeted object, and handle picking a new target via collision ray.
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
 * Confirmed: datum_get(*(data_t**)0x5aa6d4, handle) for player data at 0x119320.
 * Confirmed: Player object handle at player+0x34.
 * Confirmed: FUN_0014df70 collision test with 5 args at CALL 0x14df70.
 * Confirmed: collision type == 3 check at CMP word [EBP-0x64],0x3.
 * Confirmed: collision object handle at [EBP-0x2c] (offset 0x38 in result).
 * Confirmed: assert string "collision.type==_collision_result_object" at 0x29af70.
 * Confirmed: display_assert at CALL 0x8d9f0, system_exit at CALL 0x8e2f0.
 */
void render_debug_object_damage(void)
{
  typedef int(__cdecl *fn_snprintf_t)(char *, int, const char *, ...);
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
      snprintf_fn(string_buffer, 0x800,
          "%s|nbody %0.3f|n  current %0.3f|n  recent %0.3f|n"
          "shield %0.3f|n  current %0.3f|n  recent %0.3f|n",
          filename,
          (double)*(float *)(obj + 0x90),
          (double)*(float *)(obj + 0x9c),
          (double)*(float *)(obj + 0xa8),
          (double)*(float *)(obj + 0x94),
          (double)*(float *)(obj + 0x98),
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
  /* collision_result offset 0x38 = object handle (EBP-0x2c from EBP-0x64 base) */
  *(int *)0x46f070 = *(int *)((char *)collision_result + 0x38);
}

/* FUN_00138ee0 (0x138ee0) — Texture cache hardware format lookup with RDTSC
 * profiling. Wraps xbox_texture_cache_get_hardware_format(hardware_format, 1, 1)
 * between RDTSC start/stop calls for performance measurement.
 *
 * Confirmed: CALL 0x916e0 (RDTSC start) takes no args.
 * Confirmed: PUSH 1 / PUSH 1 / PUSH [EBP+8] then CALL 0x1bf570 (cdecl, 3 args).
 * Confirmed: ADD ESP,0xc after call (cdecl cleanup for 3 args).
 * Confirmed: CALL 0x91710 (RDTSC stop) takes no args.
 * Confirmed: return value in ESI is the result of xbox_texture_cache_get_hardware_format.
 */
int FUN_00138ee0(int hardware_format)
{
  int result;

  profile_texture_start();
  result = (int)xbox_texture_cache_get_hardware_format((void *)hardware_format, 1, 1);
  profile_texture_end();
  return result;
}
