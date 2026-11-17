#ifndef HALO_UNITS_WEAPON_DEFINITIONS_H
#define HALO_UNITS_WEAPON_DEFINITIONS_H

#include "unit_definitions.h"

/* Weapon tag fields read by the unit code. Each accessor is an lvalue of the
 * field at its tag offset. */

#define WEAPON_DEFINITION_FLAGS_OFFSET 0x308
#define WEAPON_DEFINITION_MELEE_DAMAGE_INDEX_OFFSET 0x3a0
#define WEAPON_DEFINITION_FIELD_3B0_OFFSET 0x3b0
#define WEAPON_DEFINITION_ZOOM_LEVELS_OFFSET 0x3da
#define WEAPON_DEFINITION_ZOOM_IN_SOUND_INDEX_OFFSET 0x4ac
#define WEAPON_DEFINITION_ZOOM_OUT_SOUND_INDEX_OFFSET 0x4bc
#define WEAPON_DEFINITION_FIELD_4D0_OFFSET 0x4d0

/* Dword of flag bits; bit 14 is tested for night vision. */
#define weapon_definition_flags(definition) \
  UNIT_DEFINITION_FIELD(definition, uint32_t, WEAPON_DEFINITION_FLAGS_OFFSET)
/* Damage effect used instead of the unit's melee damage. */
#define weapon_definition_melee_damage_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, WEAPON_DEFINITION_MELEE_DAMAGE_INDEX_OFFSET)
#define weapon_definition_field_3b0(definition) \
  UNIT_DEFINITION_FIELD(definition, int, WEAPON_DEFINITION_FIELD_3B0_OFFSET)
/* Number of zoom levels; the zoom fraction divides by this minus one. */
#define weapon_definition_zoom_levels(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, WEAPON_DEFINITION_ZOOM_LEVELS_OFFSET)
/* Sound started when the zoom level changes to a zoomed level. */
#define weapon_definition_zoom_in_sound_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, WEAPON_DEFINITION_ZOOM_IN_SOUND_INDEX_OFFSET)
/* Sound started when the zoom level returns to unzoomed. */
#define weapon_definition_zoom_out_sound_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, WEAPON_DEFINITION_ZOOM_OUT_SOUND_INDEX_OFFSET)
#define weapon_definition_field_4d0(definition) \
  UNIT_DEFINITION_FIELD(definition, real, WEAPON_DEFINITION_FIELD_4D0_OFFSET)

#endif
