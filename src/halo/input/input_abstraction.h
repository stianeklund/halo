#ifndef HALO_INPUT_INPUT_ABSTRACTION_H
#define HALO_INPUT_INPUT_ABSTRACTION_H

#include "input.h"

enum {
  _game_control_jump = 0,
  _game_control_switch_grenades,
  _game_control_action,
  _game_control_switch_weapons,
  _game_control_melee,
  _game_control_flashlight,
  _game_control_grenade,
  _game_control_primary_trigger,
  _game_control_start,
  _game_control_back,
  _game_control_crouch,
  _game_control_zoom,
  NUMBER_OF_GAME_CONTROLS
};

enum {
  _joystick_controls_default = 0,
  _joystick_controls_southpaw,
  _joystick_controls_legacy,
  _joystick_controls_legacy_southpaw,
  NUMBER_OF_JOYSTICK_CONTROLS
};

enum {
  _vehicle_type_human_plane = 3,
  _vehicle_type_alien_fighter = 5
};

typedef struct game_input_preferences {
  real yaw_rate;
  real pitch_rate;
  byte game_control_to_xbox_buttons[NUMBER_OF_GAME_CONTROLS];
  int16_t joystick_controls;
  boolean invert_look;
  boolean invert_look_aircraft_control;
} game_input_preferences_t;
cs(game_input_preferences_t, 0x18);
co(game_input_preferences_t, joystick_controls, 0x14);

typedef struct game_input_state {
  byte buttons[NUMBER_OF_GAME_CONTROLS];
  real forward_movement;
  real strafe;
  real yaw;
  real pitch;
} game_input_state_t;
cs(game_input_state_t, 0x1c);

typedef struct input_abstraction_globals {
  game_input_preferences_t player_control_preferences[MAXIMUM_GAMEPADS];
  game_input_state_t input_states[MAXIMUM_GAMEPADS];
  uint32_t device_enumeration_startup_timer;
  boolean controller_available[MAXIMUM_GAMEPADS];
  boolean initialized;
  uint8_t pad_d9[3];
  uint32_t time_of_first_device_insertion;
} input_abstraction_globals_t;
cs(input_abstraction_globals_t, 0xe0);
co(input_abstraction_globals_t, input_states, 0x60);
co(input_abstraction_globals_t, device_enumeration_startup_timer, 0xd0);
co(input_abstraction_globals_t, controller_available, 0xd4);
co(input_abstraction_globals_t, initialized, 0xd8);
co(input_abstraction_globals_t, time_of_first_device_insertion, 0xdc);

#define INPUT_ABSTRACTION_GLOBALS_ADDRESS 0x46b820
#define input_abstraction_globals \
  (*(input_abstraction_globals_t *)INPUT_ABSTRACTION_GLOBALS_ADDRESS)

#endif
