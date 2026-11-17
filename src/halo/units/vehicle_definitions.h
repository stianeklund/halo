#ifndef HALO_UNITS_VEHICLE_DEFINITIONS_H
#define HALO_UNITS_VEHICLE_DEFINITIONS_H

/* Vehicle tag fields that vehicle_definition_t does not declare. Each accessor
 * is an lvalue of the field at its tag offset. */

#define VEHICLE_DEFINITION_BLUR_SPEED_OFFSET 0x318
#define VEHICLE_DEFINITION_FIELD_31C_OFFSET 0x31c
#define VEHICLE_DEFINITION_FIELD_330_OFFSET 0x330
#define VEHICLE_DEFINITION_FIELD_334_OFFSET 0x334
#define VEHICLE_DEFINITION_FIELD_340_OFFSET 0x340
#define VEHICLE_DEFINITION_FIELD_344_OFFSET 0x344
#define VEHICLE_DEFINITION_SUSPENSION_SOUND_INDEX_OFFSET 0x3bc
#define VEHICLE_DEFINITION_CRASH_SOUND_INDEX_OFFSET 0x3cc
#define VEHICLE_DEFINITION_MATERIAL_EFFECTS_INDEX_OFFSET 0x3dc
#define MASS_POINT_DEFINITION_FIELD_24_OFFSET 0x24
#define SCENARIO_FIELD_10_OFFSET 0x10
#define SCENARIO_FIELD_14_OFFSET 0x14

/* Speed at or above which vehicle_update shows the "~blur" region. */
#define vehicle_definition_blur_speed(definition) \
  UNIT_DEFINITION_FIELD(definition, real, VEHICLE_DEFINITION_BLUR_SPEED_OFFSET)
/* First of four int16 export function sources. */
#define vehicle_definition_field_31c(definition) \
  UNIT_DEFINITION_FIELD(definition, int16_t, VEHICLE_DEFINITION_FIELD_31C_OFFSET)
#define vehicle_definition_field_330(definition) \
  UNIT_DEFINITION_FIELD(definition, real, VEHICLE_DEFINITION_FIELD_330_OFFSET)
#define vehicle_definition_field_334(definition) \
  UNIT_DEFINITION_FIELD(definition, real, VEHICLE_DEFINITION_FIELD_334_OFFSET)
#define vehicle_definition_field_340(definition) \
  UNIT_DEFINITION_FIELD(definition, real, VEHICLE_DEFINITION_FIELD_340_OFFSET)
#define vehicle_definition_field_344(definition) \
  UNIT_DEFINITION_FIELD(definition, real, VEHICLE_DEFINITION_FIELD_344_OFFSET)
/* Sound played by update_suspension. */
#define vehicle_definition_suspension_sound_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, VEHICLE_DEFINITION_SUSPENSION_SOUND_INDEX_OFFSET)
/* Sound played by create_crashing_effects. */
#define vehicle_definition_crash_sound_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, VEHICLE_DEFINITION_CRASH_SOUND_INDEX_OFFSET)
/* Effect passed to material_effect_new by create_slipping_effects. */
#define vehicle_definition_material_effects_index(definition) \
  UNIT_DEFINITION_FIELD(definition, int, VEHICLE_DEFINITION_MATERIAL_EFFECTS_INDEX_OFFSET)

/* Bit 0 picks material effect type 10 instead of 9 for a slipping point. */
#define mass_point_definition_field_24(mass_point) \
  UNIT_DEFINITION_FIELD(mass_point, uint32_t, MASS_POINT_DEFINITION_FIELD_24_OFFSET)

/* Scenario z floor and ceiling for some vehicle types; 0 means none. */
#define scenario_field_10(scenario) \
  UNIT_DEFINITION_FIELD(scenario, real, SCENARIO_FIELD_10_OFFSET)
#define scenario_field_14(scenario) \
  UNIT_DEFINITION_FIELD(scenario, real, SCENARIO_FIELD_14_OFFSET)

#endif
