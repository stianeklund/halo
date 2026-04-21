/* Compute the cross product of two 3D vectors.
 *
 * out = a × b
 *
 * Confirmed: pure x87 arithmetic, no assertions, no globals.
 * Confirmed: FLD [ECX] / FMUL [EAX+0x4] / FLD [ECX+0x4] / FMUL [EAX] / FSUBP
 *   computes out[2] = a[0]*b[1] - a[1]*b[0] (z component).
 * Confirmed: three-component store at [EAX], [EAX+0x4], [EAX+0x8].
 */
void cross_product3d(float *a, float *b, float *out)
{
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

/* 0xfae80 — weapon_get_label: resolve a weapon handle to its label string.
 *
 * If the handle is NONE (-1), returns a pointer to an empty string
 * (global at 0x25386f, which starts with a null byte).
 * Otherwise resolves the object via object_get_and_verify_type with
 * type mask 4 (weapon), reads the tag index from the object header,
 * looks up the 'weap' tag, and returns a pointer to offset 0x30c
 * within the tag definition (the weapon label field).
 *
 * Confirmed: CMP ECX,-1 / MOV EAX,0x25386f / JZ return.
 * Confirmed: PUSH 0x4 / PUSH ECX / CALL object_get_and_verify_type.
 * Confirmed: MOV EAX,[EAX] — reads tag_index from object header.
 * Confirmed: PUSH EAX / PUSH 0x77656170 / CALL tag_get.
 * Confirmed: ADD EAX,0x30c — label field offset.
 */
char *weapon_get_label(int weapon_handle)
{
  if (weapon_handle == -1) {
    return (char *)0x25386f;
  }
  int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  return (char *)tag_get(0x77656170, *obj) + 0x30c;
}

/* 0xfb0c0 — weapon_is_flag: test whether a weapon object is a flag.
 *
 * Resolves the object via object_get_and_verify_type with type mask 4,
 * reads the tag index, looks up the 'weap' tag, and tests bit 3 of
 * the dword at offset 0x308 in the tag definition.
 *
 * Confirmed: PUSH 0x4 / PUSH EAX / CALL object_get_and_verify_type.
 * Confirmed: MOV ECX,[EAX] — reads tag_index.
 * Confirmed: PUSH ECX / PUSH 0x77656170 / CALL tag_get.
 * Confirmed: MOV EAX,[EAX+0x308] / SHR EAX,0x3 / AND EAX,0x1.
 */
bool weapon_is_flag(int object_index)
{
  int *obj = (int *)object_get_and_verify_type(object_index, 4);
  uint32_t *tag = (uint32_t *)tag_get(0x77656170, *obj);
  return (tag[0x308 / 4] >> 3) & 1;
}
