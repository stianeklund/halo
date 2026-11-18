#ifndef HALO_INPUT_INPUT_H
#define HALO_INPUT_INPUT_H

enum {
  _gamepad_analog_button_a = 0,
  _gamepad_analog_button_b,
  _gamepad_analog_button_x,
  _gamepad_analog_button_y,
  _gamepad_analog_button_black,
  _gamepad_analog_button_white,
  _gamepad_analog_button_left_trigger,
  _gamepad_analog_button_right_trigger,
  NUMBER_OF_GAMEPAD_ANALOG_BUTTONS,

  FIRST_GAMEPAD_BINARY_BUTTON = 8,
  _gamepad_binary_button_dpad_up = 8,
  _gamepad_binary_button_dpad_down,
  _gamepad_binary_button_dpad_left,
  _gamepad_binary_button_dpad_right,
  _gamepad_binary_button_start,
  _gamepad_binary_button_back,
  _gamepad_binary_button_left_thumb,
  _gamepad_binary_button_right_thumb,

  NUMBER_OF_GAMEPAD_BINARY_BUTTONS =
    NUMBER_OF_GAMEPAD_BUTTONS - FIRST_GAMEPAD_BINARY_BUTTON
};

enum {
  _gamepad_stick_left = 0,
  _gamepad_stick_right,
  NUMBER_OF_GAMEPAD_STICKS
};

/* Key codes that input_key_is_down reads by index. The four combined
 * modifier codes past NUMBER_OF_KEYS report the larger of the left and
 * right key tick counts. */
enum {
  _key_left_shift = 0x39,
  _key_right_shift = 0x44,
  _key_left_control = 0x45,
  _key_left_windows = 0x46,
  _key_left_alt = 0x47,
  _key_right_alt = 0x49,
  _key_right_windows = 0x4a,
  _key_right_control = 0x4c,
  NUMBER_OF_KEYS = 0x68,

  _key_shift = NUMBER_OF_KEYS + 1,
  _key_control,
  _key_windows,
  _key_alt
};

typedef struct {
  int16_t x;
  int16_t y;
} point2d;

typedef struct gamepad_state {
  byte analog_buttons[NUMBER_OF_GAMEPAD_ANALOG_BUTTONS];
  byte analog_button_thresholds[NUMBER_OF_GAMEPAD_ANALOG_BUTTONS];
  byte buttons[NUMBER_OF_GAMEPAD_BUTTONS];
  point2d sticks[NUMBER_OF_GAMEPAD_STICKS];
} gamepad_state_t;
cs(gamepad_state_t, 0x28);
co(gamepad_state_t, analog_button_thresholds, 0x08);
co(gamepad_state_t, buttons, 0x10);
co(gamepad_state_t, sticks, 0x20);

#endif
