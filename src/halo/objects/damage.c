/* FUN_00136790 (0x136790) — Check if object body vitality is below full and
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
char FUN_00136790(int object_handle)
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

/* FUN_001367e0 (0x1367e0) — Check if object shield vitality is at or below full
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
char FUN_001367e0(int object_handle)
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

/* FUN_001368e0 (0x1368e0) — Clear bit 3 of object+0xb7 flags byte for all
 * children/widgets of a given parent handle.
 *
 * Iterates using FUN_000ce450 (first) / FUN_000ce320 (next) to enumerate
 * associated objects. For each, clears bit 3 (AND 0xf7) of the flags byte
 * at offset 0xb7. This is the same byte modified by FUN_00136980 (bit 0)
 * and FUN_001369b0 (bit 7).
 *
 * Confirmed: cdecl, single stack param at [EBP+0x8].
 * Confirmed: object_get_and_verify_type(index, -1) at CALL 0x13d680.
 * Confirmed: AND byte [EAX+0xb7],0xf7 at 0x136908.
 * Confirmed: FUN_000ce450 (first child) at CALL 0xce450.
 * Confirmed: FUN_000ce320 (next child) at CALL 0xce320.
 */
void FUN_001368e0(int player_handle)
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

/* FUN_00136930 (0x136930) — Set bit 3 of object+0xb7 flags byte for all
 * children/widgets of a given parent handle.
 *
 * Complement of FUN_001368e0 which clears the same bit. Iterates using
 * FUN_000ce450 (first) / FUN_000ce320 (next) to enumerate associated
 * objects. For each, sets bit 3 (OR 0x8) of the flags byte at offset 0xb7.
 *
 * Confirmed: cdecl, single stack param at [EBP+0x8].
 * Confirmed: object_get_and_verify_type(index, -1) at CALL 0x13d680.
 * Confirmed: OR byte [EAX+0xb7],0x8 at 0x136958.
 * Confirmed: FUN_000ce450 (first child) at CALL 0xce450.
 * Confirmed: FUN_000ce320 (next child) at CALL 0xce320.
 */
void FUN_00136930(int player_handle)
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

/* FUN_00136980 (0x136980) — Set or clear the damage-invincible bit on an
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
void FUN_00136980(int object_handle, char flag)
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

/* FUN_001369b0 (0x1369b0) — Set or clear bit 7 of object+0xb6 flags byte.
 *
 * Confirmed: identical structure to FUN_00136980 but targets offset 0xb6 bit 7.
 * Confirmed: OR byte [EAX+0xb6],0x80 (set) at 0x1369cd.
 * Confirmed: AND byte [EAX+0xb6],0x7f (clear) at 0x1369d6.
 */
void FUN_001369b0(int object_handle, char flag)
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
  FUN_0009ec30(effect_tag_index, object_handle, object_handle, -1, 0, 0, 0, 0);
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

/* FUN_00136a80 (0x136a80) — Compute scaled body vitality for an object.
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
 * Confirmed: caller at 0x52211 pushes (PUSH 0; PUSH ECX) => (handle, 0).
 */
float FUN_00136a80(int object_handle, char param_2)
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

/* FUN_00136ae0 (0x136ae0) — Compute scaled shield vitality for an object.
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
 * Confirmed: caller at 0x521f5 pushes (PUSH 0; PUSH EDX) => (handle, 0).
 */
float FUN_00136ae0(int object_handle, char param_2)
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

/* FUN_00136b40 (0x136b40) — Trigger initial body-damage effect on an object.
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
 * Confirmed: 8 pushes (0,0,0,0,-1,EDI,EDI,ECX) before CALL 0x9ec30.
 * Confirmed: OR byte [ESI+0xb6],0x8 at 0x136b9d sets bit 3.
 * Confirmed: MOV [ESI+0x98],0x0 at 0x136ba8 clears dword.
 * Confirmed: MOV EAX,EDI; CALL 0x136a00 => FUN_00136a00(@EAX=handle, 0).
 */
void FUN_00136b40(int object_handle)
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
      FUN_0009ec30(*(int *)(coll_tag + 0x1a4), object_handle, object_handle, -1, 0, 0, 0, 0);
    }
    *(unsigned char *)(obj + 0xb6) |= 8;
    *(int *)(obj + 0x98) = 0;
    FUN_00136a00(object_handle, 0);
  }
}

/* FUN_00137540 (0x137540) — Set "body depleted" flag and detach child units.
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
 *   5. Calls FUN_00136b40 to trigger the initial body-damage effect
 *
 * Confirmed: cdecl, 1 stack param (object_handle), void return.
 * Confirmed: PUSH -1; PUSH EDI; CALL 0x13d680 => object_get_and_verify_type.
 * Confirmed: TEST AL,0x4 at 0x137561 checks bit 2 of [ESI+0xb6].
 * Confirmed: OR EAX,0x4; MOV [ESI+0xb6],AX sets bit 2.
 * Confirmed: tag_get('obje', [ESI]) at CALL 0x1ba140.
 * Confirmed: MOV EAX,[EAX+0x7c] reads collision model index.
 * Confirmed: tag_get('coll', coll_index) at second CALL 0x1ba140.
 * Confirmed: MOV ECX,[EAX+0xb4] reads effect index from coll tag.
 * Confirmed: 8 pushes (0,0,0,0,EBX,EDI,EDI,ECX) before CALL 0x9ec30.
 * Confirmed: CMP word [ESI+0x64],0x1 checks object type == 1.
 * Confirmed: MOV EDI,[ESI+0xc8] reads first child handle.
 * Confirmed: CMP word [ESI+0x64],0x0 checks child type == 0.
 * Confirmed: CMP [ESI+0x1c8],EBX checks child+0x1c8 == -1.
 * Confirmed: MOV AL,[0x5aa890]; TEST AL,AL checks global byte.
 * Confirmed: CMP word [ESI+0x2a0],BX checks child+0x2a0 != -1.
 * Confirmed: PUSH EDI; CALL 0x1a7f80 => unit_set_actively_controlled_flag.
 * Confirmed: MOV EDI,[ESI+0xc4] reads next sibling.
 * Confirmed: MOV EDI,[EBP+0x8] restores param_1 before FUN_00136b40 call.
 * Confirmed: PUSH EDI; CALL 0x136b40 => FUN_00136b40(object_handle).
 */
void FUN_00137540(int object_handle)
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
      FUN_0009ec30(*(int *)(coll_tag + 0xb4), object_handle, object_handle, -1, 0, 0, 0, 0);
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
    FUN_00136b40(object_handle);
  }
}

/* FUN_00137620 (0x137620) — Object death/destruction handler.
 *
 * Called when an object is destroyed (killed). Performs the death sequence:
 *   1. Gets the object's tag definition ('obje') from the object data
 *   2. Calls FUN_00137540 to handle death-related flag setting and child
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
 * Confirmed: PUSH ESI; CALL 0x137540 => FUN_00137540(object_handle).
 * Confirmed: MOV EAX,[EDI+0x7c] reads collision model index from obje tag.
 * Confirmed: CMP EAX,-1 at 0x13764b checks collision model presence.
 * Confirmed: tag_get('coll', collision_index) at second CALL 0x1ba140.
 * Confirmed: MOV ECX,[EAX+0xc8] reads destroy effect index from coll tag.
 * Confirmed: 8 pushes (0,0,0,0,-1,ESI,ESI,ECX) before CALL 0x9ec30.
 * Confirmed: PUSH ESI; CALL 0x136840 => FUN_00136840(object_handle).
 * Confirmed: PUSH ESI; CALL 0x140cc0 => object_delete(object_handle).
 */
void FUN_00137620(int object_handle)
{
  int *obj;
  char *obje_tag;
  char *coll_tag;
  int coll_index;

  obj = (int *)object_get_and_verify_type(object_handle, -1);
  obje_tag = (char *)tag_get(0x6f626a65, *obj);
  FUN_00137540(object_handle);
  coll_index = *(int *)(obje_tag + 0x7c);
  if (coll_index != -1) {
    coll_tag = (char *)tag_get(0x636f6c6c, coll_index);
    FUN_0009ec30(*(int *)(coll_tag + 0xc8), object_handle, object_handle, -1, 0, 0, 0, 0);
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
 *   3. Calls FUN_001402c0 to set the "~damaged" permutation on the object's
 *      model for the destroyed region
 *   4. Propagates region flags (region+0x20) into the object's damage flags:
 *        bit 5 (0x20) -> obj+0xb6 |= 0x80
 *        bit 6 (0x40) -> obj+0xb6 |= 0x100
 *        bit 7 (0x80) -> obj+0xb7 |= 0x02
 *        bit 8 (0x100) -> obj+0xb7 |= 0x04
 *   5. If region has "forces body depletion" flag (bit 1 / 0x02), calls
 *      FUN_00137540 to deplete the object's body
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
 * Confirmed: 8 pushes (0,0,0,0,-1,EDI,EDI,[ESI+0x44]) before CALL 0x9ec30.
 * Confirmed: PUSH 1; PUSH EDX; PUSH 0x29b030; PUSH EDI => FUN_001402c0(obj,"~damaged",rgn,1).
 * Confirmed: flag tests at 0x20,0x40,0x80,0x100 on region+0x20 byte.
 * Confirmed: TEST byte [ESI+0x20],0x2 gates call to FUN_00137540.
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
  FUN_0009ec30(*(int *)(region + 0x44), object_handle, object_handle,
               -1, 0, 0, 0, 0);
  FUN_001402c0(object_handle, "~damaged", region_index, 1);

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
    FUN_00137540(object_handle);
  }

  *(unsigned short *)((char *)obj + 0x124) |= (unsigned short)(1 << region_idx);
  FUN_0013c6e0(object_handle, region_index, *(unsigned int *)(region + 0x20));
}
