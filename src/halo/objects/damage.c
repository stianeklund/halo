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
