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
