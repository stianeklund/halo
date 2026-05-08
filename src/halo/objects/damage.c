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
