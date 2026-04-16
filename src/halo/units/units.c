/*
 * unit_is_alive (0x1a9a30)
 *
 * Returns whether the given unit handle refers to a unit that is currently
 * alive. Resolves the handle via object_get_and_verify_type with type mask
 * 0x3 (bit 0 = biped, bit 1 = vehicle — accepts any unit object; asserts
 * otherwise) and tests bit 6 of the unit's flag word at offset 0x1B4
 * (unit_data_t.unk_436). The decompiled body is literally:
 *     iVar1 = object_get_and_verify_type(handle, 3);
 *     return (*(uint *)(iVar1 + 0x1B4) >> 6) & 1;
 *
 * Callers: 0x3cff0, 0x95330, 0xbd100.
 */

#include "../../common.h"

bool unit_is_alive(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  return (unit->unk_436 >> 6) & 1;
}
