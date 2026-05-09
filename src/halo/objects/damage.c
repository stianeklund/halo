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
