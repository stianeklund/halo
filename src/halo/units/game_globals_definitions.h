#ifndef HALO_UNITS_GAME_GLOBALS_DEFINITIONS_H
#define HALO_UNITS_GAME_GLOBALS_DEFINITIONS_H

#include "unit_definitions.h"

/* Game globals tag blocks and material fields read by the unit code. Each
 * accessor is an lvalue of the field at its tag offset. */

#define GAME_GLOBALS_GRENADES_OFFSET 0x128
#define GAME_GLOBALS_PLAYER_INFORMATION_OFFSET 0x170
#define GAME_GLOBALS_FIRST_PERSON_INTERFACE_OFFSET 0x17c
#define GAME_GLOBALS_FALLING_DAMAGE_OFFSET 0x188
#define MATERIAL_DEFINITION_MELEE_HIT_SOUND_INDEX_OFFSET 0x370

/* Block of game_globals_grenade_t, indexed by grenade type. */
#define game_globals_grenades(globals) \
  UNIT_DEFINITION_FIELD(globals, tag_block, GAME_GLOBALS_GRENADES_OFFSET)
/* Block of game_globals_player_information_t. */
#define game_globals_player_information(globals) \
  UNIT_DEFINITION_FIELD(globals, tag_block, GAME_GLOBALS_PLAYER_INFORMATION_OFFSET)
/* Block of game_globals_first_person_interface_t. */
#define game_globals_first_person_interface(globals) \
  UNIT_DEFINITION_FIELD(globals, tag_block, GAME_GLOBALS_FIRST_PERSON_INTERFACE_OFFSET)
/* Block of game_globals_falling_damage_t. */
#define game_globals_falling_damage(globals) \
  UNIT_DEFINITION_FIELD(globals, tag_block, GAME_GLOBALS_FALLING_DAMAGE_OFFSET)

/* Sound started by unit_melee_sound when a melee attack hits the material. */
#define material_definition_melee_hit_sound_index(material) \
  UNIT_DEFINITION_FIELD(material, int, MATERIAL_DEFINITION_MELEE_HIT_SOUND_INDEX_OFFSET)

#endif
