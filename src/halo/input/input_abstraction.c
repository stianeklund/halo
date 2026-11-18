#include "x87_math.h"
#include "input_abstraction.h"
#include "../units/game_globals_definitions.h"

#define _error_silent 2

enum {
  _error_controller_unplugged_start_to_continue = 0xc,
  _error_controller_unplugged = 0xd
};

#define INPUT_DEVICE_SETTLE_MILLISECONDS 2000
#define INPUT_DEVICE_INSERTION_FLAGS 0xfff000

/* The single-precision 45 and 135 degree angles are promoted before the
 * double arithmetic below, so they are written as float literals. */
#define GAMEPAD_AXIS_NORMALIZATION_SCALE (1.f / 32767.f)
#define STICK_DIAGONAL_ANGLE 0.7853981852531433f
#define STICK_SECOND_QUADRANT_DIAGONAL_ANGLE 2.356194496154785f
#define RIGHT_STICK_DIAGONAL_SNAP_ANGLE ((double)0.1745329201221466f)
#define LEFT_STICK_DIAGONAL_SNAP_ANGLE \
  ((double)STICK_DIAGONAL_ANGLE - RIGHT_STICK_DIAGONAL_SNAP_ANGLE)
#define STICK_DIAGONAL_BLEND_SCALE (1.0 / LEFT_STICK_DIAGONAL_SNAP_ANGLE)

/* .rdata table of the four quadrant diagonals: 45, 135, -45, -135 degrees. */
#define STICK_DIRECTION_ANGLES_ADDRESS 0x280f84
#define stick_direction_angles ((real const *)STICK_DIRECTION_ANGLES_ADDRESS)

void input_abstraction_dispose(void)
{
  csmemset(&input_abstraction_globals, 0,
           offsetof(input_abstraction_globals_t,
                    time_of_first_device_insertion));
}

void input_abstraction_mark_time(void)
{
  input_abstraction_globals.device_enumeration_startup_timer =
    system_milliseconds();
}

void input_abstraction_get_local_player_preferences(short local_player_index,
                                                    void *preferences_out)
{
  if (local_player_index < 0 || local_player_index >= MAXIMUM_GAMEPADS) {
    display_assert(
      "(local_player_index>=0) && (local_player_index<MAXIMUM_GAMEPADS)",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1ef, 1);
    system_exit(-1);
  }
  if (preferences_out == NULL) {
    display_assert("preferences",
                   "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1f0, 1);
    system_exit(-1);
  }
  csmemcpy(preferences_out,
           &input_abstraction_globals
              .player_control_preferences[local_player_index],
           sizeof(game_input_preferences_t));
}

void input_abstraction_update_local_player_preferences(short controller_index,
                                                       void *preferences)
{
  game_input_preferences_t *new_preferences;

  new_preferences = preferences;
  if (controller_index < 0 || controller_index >= MAXIMUM_GAMEPADS) {
    display_assert(
      "(controller_index>=0) && (controller_index<MAXIMUM_GAMEPADS)",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1fb, 1);
    system_exit(-1);
  }
  if (new_preferences == NULL) {
    display_assert("preferences",
                   "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1fc, 1);
    system_exit(-1);
  }
  if (new_preferences->game_control_to_xbox_buttons[_game_control_start] !=
        _gamepad_binary_button_start ||
      new_preferences->game_control_to_xbox_buttons[_game_control_back] !=
        _gamepad_binary_button_back) {
    display_assert(
      "invalid controller preferences; can't remap start & back buttons",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x1ff, 1);
    system_exit(-1);
  }
  csmemcpy(
    &input_abstraction_globals.player_control_preferences[controller_index],
    new_preferences, sizeof(game_input_preferences_t));
}

void *input_abstraction_get_input_state(short local_player_index)
{
  if (local_player_index < 0 || local_player_index >= MAXIMUM_GAMEPADS) {
    display_assert(
      "(local_player_index>=0) && (local_player_index<MAXIMUM_GAMEPADS)",
      "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x209, 1);
    system_exit(-1);
  }
  return &input_abstraction_globals.input_states[local_player_index];
}

/* The message text, typo included, is verbatim from the binary. */
void input_abstraction_update_device_changes(unsigned int device_change_flags)
{
  if (!input_abstraction_globals.initialized) {
    return;
  }
  if (device_change_flags != 0 &&
      (system_milliseconds() -
           input_abstraction_globals.device_enumeration_startup_timer >=
         INPUT_DEVICE_SETTLE_MILLISECONDS ||
       (input_abstraction_globals.time_of_first_device_insertion != 0 &&
        system_milliseconds() -
            input_abstraction_globals.time_of_first_device_insertion >=
          INPUT_DEVICE_SETTLE_MILLISECONDS))) {
    error(_error_silent,
          "stopping bink playback to due to change in input devices");
    bink_playback_stop();
  }
  if ((device_change_flags & INPUT_DEVICE_INSERTION_FLAGS) != 0 &&
      input_abstraction_globals.time_of_first_device_insertion == 0) {
    input_abstraction_globals.time_of_first_device_insertion =
      system_milliseconds();
  }
}

/* Whether the local player drives a human plane or alien fighter. The sole
 * caller keeps the result in AL, so it returns char. */
char input_abstraction_print_config_control(int controller_index)
{
  int player_index;
  player_data_t *player;
  unit_data_t *unit;
  object_data_t *vehicle;
  vehicle_definition_t *definition;
  unit_seat_t *seat;

  if ((int16_t)controller_index < 0 ||
      (int16_t)controller_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("(controller_index>=0) && "
                   "(controller_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\input\\input_abstraction.c", 0x239, 1);
    system_exit(-1);
  }

  player_index = local_player_get_player_index(controller_index);
  if (player_index == NONE) {
    return false;
  }

  player =
    (player_data_t *)datum_absolute_index_to_index(player_data, player_index);
  if (player == NULL) {
    return false;
  }

  unit = object_try_and_get_and_verify_type(player->unit_handle,
                                            _object_mask_unit);
  if (unit == NULL) {
    return false;
  }

  if (unit->object.parent_object_index.value == NONE ||
      unit->parent_seat_index == NONE) {
    return false;
  }

  vehicle = object_get_and_verify_type(
    (int)unit->object.parent_object_index.value, _object_mask_vehicle);
  definition = tag_get(TAG_GROUP_VEHI, vehicle->tag_index);
  if (definition->vehicle_type == _vehicle_type_human_plane ||
      definition->vehicle_type == _vehicle_type_alien_fighter) {
    seat = tag_block_get_element(&((unit_definition_t *)definition)->seats,
                                 (int)unit->parent_seat_index,
                                 sizeof(unit_seat_t));
    return (char)((seat->flags & FLAG(_unit_seat_driver_bit)) != 0);
  }
  return false;
}

void input_abstraction_initialize(void)
{
  int controller_index;
  game_input_preferences_t *preferences;

  csmemset(&input_abstraction_globals, 0,
           offsetof(input_abstraction_globals_t,
                    time_of_first_device_insertion));
  for (controller_index = 0; controller_index < MAXIMUM_GAMEPADS;
       controller_index++) {
    preferences =
      &input_abstraction_globals.player_control_preferences[controller_index];
    preferences->pitch_rate = 60.f;
    preferences->yaw_rate = 120.f;
    preferences->game_control_to_xbox_buttons[_game_control_jump] =
      _gamepad_analog_button_a;
    preferences->game_control_to_xbox_buttons[_game_control_switch_grenades] =
      _gamepad_analog_button_black;
    preferences->game_control_to_xbox_buttons[_game_control_action] =
      _gamepad_analog_button_x;
    preferences->game_control_to_xbox_buttons[_game_control_switch_weapons] =
      _gamepad_analog_button_y;
    preferences->game_control_to_xbox_buttons[_game_control_melee] =
      _gamepad_analog_button_b;
    preferences->game_control_to_xbox_buttons[_game_control_flashlight] =
      _gamepad_analog_button_white;
    preferences->game_control_to_xbox_buttons[_game_control_grenade] =
      _gamepad_analog_button_left_trigger;
    preferences->game_control_to_xbox_buttons[_game_control_primary_trigger] =
      _gamepad_analog_button_right_trigger;
    preferences->game_control_to_xbox_buttons[_game_control_start] =
      _gamepad_binary_button_start;
    preferences->game_control_to_xbox_buttons[_game_control_back] =
      _gamepad_binary_button_back;
    preferences->game_control_to_xbox_buttons[_game_control_crouch] =
      _gamepad_binary_button_left_thumb;
    preferences->game_control_to_xbox_buttons[_game_control_zoom] =
      _gamepad_binary_button_right_thumb;
    preferences->joystick_controls = _joystick_controls_default;
    preferences->invert_look = false;
    preferences->invert_look_aircraft_control = false;
    input_abstraction_globals.controller_available[controller_index] =
      input_has_gamepad((int16_t)controller_index);
  }
  input_abstraction_globals.device_enumeration_startup_timer =
    system_milliseconds();
  input_abstraction_globals.initialized = true;
}

void input_abstraction_update(void)
{
  real stick_x;
  real stick_y;
  real left_magnitude;
  real right_magnitude;
  char invert_look;
  int16_t left_quadrant;
  int16_t right_quadrant;
  int16_t joystick_controls;
  gamepad_state_t *gamepad;
  int control_index;
  real_point2d left_stick;
  real_point2d right_stick;
  real absolute_angle;
  int controller_index;
  real left_angle, right_angle;
  real absolute_sine, absolute_cosine;
  double scale;
  real left_difference, right_difference;
  game_input_preferences_t *preferences;
  game_input_state_t *state;
  int16_t error_controller;
  int16_t error_code;
  boolean pause_game;
  boolean show_error;
  int index, available_controllers;

  tag_block_get_element(&game_globals_player_control(game_globals_get()), 0,
                        GAME_GLOBALS_PLAYER_CONTROL_SIZE);

  for (controller_index = 0; controller_index < MAXIMUM_GAMEPADS;
       controller_index++) {
    gamepad = input_get_gamepad_state(controller_index);
    if (gamepad != NULL) {
      preferences =
        &input_abstraction_globals.player_control_preferences[controller_index];
      state = &input_abstraction_globals.input_states[controller_index];

      player_look_yaw_rate[controller_index] =
        input_abstraction_globals.player_control_preferences[controller_index]
          .yaw_rate;
      player_look_pitch_rate[controller_index] =
        input_abstraction_globals.player_control_preferences[controller_index]
          .pitch_rate;

      stick_y = (real)gamepad->sticks[_gamepad_stick_left].y;
      stick_x = (real)gamepad->sticks[_gamepad_stick_left].x;

      left_angle = (real)x87_atan2((double)stick_y, (double)stick_x);
      absolute_sine = x87_fabs(x87_fsin(left_angle));
      absolute_cosine = x87_fabs(x87_fcos(left_angle));
      scale = 1.0 / (absolute_sine <= absolute_cosine ? absolute_cosine
                                                      : absolute_sine);

      stick_x = stick_x * GAMEPAD_AXIS_NORMALIZATION_SCALE * scale;
      left_stick.x = CLAMP(stick_x, -1.0f, 1.0f);

      stick_y = stick_y * GAMEPAD_AXIS_NORMALIZATION_SCALE * scale;
      left_stick.y = CLAMP(stick_y, -1.0f, 1.0f);

      stick_y = (real)gamepad->sticks[_gamepad_stick_right].y;
      stick_x = (real)gamepad->sticks[_gamepad_stick_right].x;

      right_angle = (real)x87_atan2((double)stick_y, (double)stick_x);
      absolute_sine = x87_fabs(x87_fsin(right_angle));
      absolute_cosine = x87_fabs(x87_fcos(right_angle));
      scale = 1.0 / (absolute_sine <= absolute_cosine ? absolute_cosine
                                                      : absolute_sine);

      stick_x = stick_x * GAMEPAD_AXIS_NORMALIZATION_SCALE * scale;
      right_stick.x = CLAMP(stick_x, -1.0f, 1.0f);

      stick_y = stick_y * GAMEPAD_AXIS_NORMALIZATION_SCALE * scale;
      right_stick.y = CLAMP(stick_y, -1.0f, 1.0f);

      for (control_index = 0; control_index < NUMBER_OF_GAME_CONTROLS;
           control_index++) {
        state->buttons[control_index] =
          gamepad->buttons[preferences
                             ->game_control_to_xbox_buttons[control_index]];
      }

      joystick_controls = preferences->joystick_controls;
      if (joystick_controls == _joystick_controls_legacy ||
          joystick_controls == _joystick_controls_legacy_southpaw) {
        left_quadrant =
          (short)((left_stick.x < 0.0f ? 1 : 0) | (left_stick.y < 0.0f ? 2 : 0));
        right_quadrant =
          (short)((right_stick.x < 0.0f ? 1 : 0) | (right_stick.y < 0.0f ? 2 : 0));

        left_difference = left_angle - stick_direction_angles[left_quadrant];
        right_difference = right_angle - stick_direction_angles[right_quadrant];

        left_magnitude = x87_sqrt(left_stick.y * left_stick.y + left_stick.x * left_stick.x);
        right_magnitude = x87_sqrt(right_stick.y * right_stick.y + right_stick.x * right_stick.x);

        if (fabs(left_difference) < LEFT_STICK_DIAGONAL_SNAP_ANGLE) {
          absolute_angle = fabs(left_angle);
          if (absolute_angle < STICK_DIAGONAL_ANGLE ||
              absolute_angle > STICK_SECOND_QUADRANT_DIAGONAL_ANGLE) {
            left_stick.x = (left_stick.x < 0.f ? -1 : 1) * left_magnitude;
            left_stick.y = (left_stick.y < 0.f ? -1 : 1) * left_magnitude *
                     (1.0 - fabs(left_difference) * STICK_DIAGONAL_BLEND_SCALE);
          } else {
            left_stick.y = (left_stick.y < 0.f ? -1 : 1) * left_magnitude;
            left_stick.x = (left_stick.x < 0.f ? -1 : 1) * left_magnitude *
                     (1.0 - fabs(left_difference) * STICK_DIAGONAL_BLEND_SCALE);
          }
        } else {
          if (fabs(left_stick.x) > fabs(left_stick.y)) {
            left_stick.x = (left_stick.x < 0.f ? -1 : 1) * left_magnitude;
            left_stick.y = 0.f;
          } else {
            left_stick.y = (left_stick.y < 0.f ? -1 : 1) * left_magnitude;
            left_stick.x = 0.f;
          }
        }

        if (fabs(right_difference) < RIGHT_STICK_DIAGONAL_SNAP_ANGLE) {
          absolute_angle = fabs(right_angle);
          if (absolute_angle < STICK_DIAGONAL_ANGLE ||
              absolute_angle > STICK_SECOND_QUADRANT_DIAGONAL_ANGLE) {
            right_stick.x = (right_stick.x < 0.f ? -1 : 1) * right_magnitude;
            right_stick.y = (right_stick.y < 0.f ? -1 : 1) * right_magnitude *
                     (1.0 - fabs(right_difference) * STICK_DIAGONAL_BLEND_SCALE);
          } else {
            right_stick.y = (right_stick.y < 0.f ? -1 : 1) * right_magnitude;
            right_stick.x = (right_stick.x < 0.f ? -1 : 1) * right_magnitude *
                     (1.0 - fabs(right_difference) * STICK_DIAGONAL_BLEND_SCALE);
          }
        } else {
          if (fabs(right_stick.x) > fabs(right_stick.y)) {
            right_stick.x = (right_stick.x < 0.f ? -1 : 1) * right_magnitude;
            right_stick.y = 0.f;
          } else {
            right_stick.y = (right_stick.y < 0.f ? -1 : 1) * right_magnitude;
            right_stick.x = 0.f;
          }
        }
      }

      invert_look = preferences->invert_look;
      if (invert_look == false && preferences->invert_look_aircraft_control) {
        invert_look = input_abstraction_print_config_control(controller_index);
      }

      switch (preferences->joystick_controls) {
      case _joystick_controls_default:
        if (gamepad->buttons[_gamepad_binary_button_dpad_left]) {
          state->strafe = 1.0f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_right]) {
          state->strafe = -1.0f;
        } else {
          state->strafe = -left_stick.x;
        }
        if (gamepad->buttons[_gamepad_binary_button_dpad_up]) {
          state->forward_movement = 1.0f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_down]) {
          state->forward_movement = -1.0f;
        } else {
          state->forward_movement = left_stick.y;
        }
        state->yaw = -right_stick.x;
        state->pitch = (invert_look ? -1.f : 1.f) * right_stick.y;
        break;

      case _joystick_controls_southpaw:
        if (gamepad->buttons[_gamepad_binary_button_dpad_left]) {
          state->yaw = 1.0f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_right]) {
          state->yaw = -1.0f;
        } else {
          state->yaw = -left_stick.x;
        }
        if (gamepad->buttons[_gamepad_binary_button_dpad_up]) {
          state->pitch = invert_look ? -1.f : 1.f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_down]) {
          state->pitch = invert_look ? 1.f : -1.f;
        } else {
          state->pitch = (invert_look ? -1.f : 1.f) * left_stick.y;
        }
        state->forward_movement = right_stick.y;
        state->strafe = -right_stick.x;
        break;

      case _joystick_controls_legacy:
        if (gamepad->buttons[_gamepad_binary_button_dpad_left]) {
          state->yaw = 1.0f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_right]) {
          state->yaw = -1.0f;
        } else {
          state->yaw = -left_stick.x;
        }
        if (gamepad->buttons[_gamepad_binary_button_dpad_up]) {
          state->forward_movement = 1.0f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_down]) {
          state->forward_movement = -1.0f;
        } else {
          state->forward_movement = left_stick.y;
        }
        state->strafe = -right_stick.x;
        state->pitch = (invert_look ? -1.f : 1.f) * right_stick.y;
        break;

      case _joystick_controls_legacy_southpaw:
        if (gamepad->buttons[_gamepad_binary_button_dpad_left]) {
          state->strafe = 1.0f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_right]) {
          state->strafe = -1.0f;
        } else {
          state->strafe = -left_stick.x;
        }
        if (gamepad->buttons[_gamepad_binary_button_dpad_up]) {
          state->pitch = invert_look ? -1.f : 1.f;
        } else if (gamepad->buttons[_gamepad_binary_button_dpad_down]) {
          state->pitch = invert_look ? 1.f : -1.f;
        } else {
          state->pitch = (invert_look ? -1.f : 1.f) * left_stick.y;
        }
        state->forward_movement = right_stick.y;
        state->yaw = -right_stick.x;
        break;

      default:
        error(_error_silent, "unknown joystick preset");
        break;
      }
      input_abstraction_globals.controller_available[controller_index] = true;
    } else {
      if (input_abstraction_globals.controller_available[controller_index]) {
        error_controller = (int16_t)controller_index;
        show_error = true;
        if (main_menu_is_active()) {
          available_controllers = 0;
          index = 0;
          do {
            if (input_abstraction_globals.controller_available[index])
              available_controllers++;
            index++;
          } while (index < MAXIMUM_GAMEPADS);

          pause_game = false;
          error_code = _error_controller_unplugged;
          if (available_controllers >= 2) {
            if (player_ui_get_single_player_local_player_controller(0) !=
                  controller_index &&
                player_ui_get_single_player_local_player_controller(1) !=
                  controller_index &&
                player_ui_get_single_player_local_player_controller(2) !=
                  controller_index &&
                player_ui_get_single_player_local_player_controller(3) !=
                  controller_index &&
                !player_ui_local_player_wants_to_play_multiplayer(
                  controller_index)) {
              show_error = false;
            }
          } else {
            if (player_ui_get_single_player_local_player_controller(0) !=
                  controller_index &&
                player_ui_get_single_player_local_player_controller(1) !=
                  controller_index &&
                player_ui_get_single_player_local_player_controller(2) !=
                  controller_index &&
                player_ui_get_single_player_local_player_controller(3) !=
                  controller_index &&
                !player_ui_local_player_wants_to_play_multiplayer(
                  controller_index)) {
              error_controller = NONE;
            }
          }
        } else if (global_network_game_client_get()) {
          pause_game = false;
          error_code = _error_controller_unplugged;
          show_error = local_player_exists((int16_t)controller_index);
        } else {
          pause_game = true;
          error_code = _error_controller_unplugged_start_to_continue;
          show_error = local_player_exists((int16_t)controller_index);
        }
        if (show_error == true) {
          if (virtual_keyboard_active())
            virtual_keyboard_close();
          display_error_deferred(error_code, error_controller, pause_game, pause_game); /* dup-args-ok: pause_game pushed twice in pristine at 0x000cf3a8 */
        }
      }
      input_abstraction_globals.controller_available[controller_index] = false;
    }
  }
}
