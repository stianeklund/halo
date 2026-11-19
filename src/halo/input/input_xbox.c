#include "input.h"

enum {
  MAXIMUM_BUFFERED_KEYSTROKES = 64,
  NUMBER_OF_VIRTUAL_CODES = 256,
  NUMBER_OF_ASCII_CODES = 128,

  GAMEPAD_STICK_DEAD_RANGE = 9000,
  ANALOG_BUTTON_PRESS_THRESHOLD_OFFSET = 32,
  ANALOG_BUTTON_RELEASE_THRESHOLD_OFFSET = 64,
  ANALOG_BUTTON_RELEASE_SATURATION = 191
};

enum {
  _input_device_change_gamepad0_removed = 0,
  _input_device_change_gamepad1_removed,
  _input_device_change_gamepad2_removed,
  _input_device_change_gamepad3_removed,
  _input_device_change_memory_unit0_removed,
  _input_device_change_memory_unit1_removed,
  _input_device_change_memory_unit2_removed,
  _input_device_change_memory_unit3_removed,
  _input_device_change_memory_unit4_removed,
  _input_device_change_memory_unit5_removed,
  _input_device_change_memory_unit6_removed,
  _input_device_change_memory_unit7_removed,
  _input_device_change_gamepad0_inserted,
  _input_device_change_gamepad1_inserted,
  _input_device_change_gamepad2_inserted,
  _input_device_change_gamepad3_inserted,
  _input_device_change_memory_unit0_inserted,
  _input_device_change_memory_unit1_inserted,
  _input_device_change_memory_unit2_inserted,
  _input_device_change_memory_unit3_inserted,
  _input_device_change_memory_unit4_inserted,
  _input_device_change_memory_unit5_inserted,
  _input_device_change_memory_unit6_inserted,
  _input_device_change_memory_unit7_inserted
};

enum {
  _key_modifier_shift = 0,
  _key_modifier_control,
  _key_modifier_alt
};

enum {
  _input_state_mode_record = 3,
  _input_state_mode_playback,
  _input_state_mode_loop
};

#define _error_silent 2

#define XDEVICE_PORT0 0
#define XDEVICE_PORT1 1
#define XDEVICE_PORT2 2
#define XDEVICE_PORT3 3
#define XDEVICE_NO_SLOT 0
#define XDEVICE_PORT0_TOP_MASK 0x00000001
#define XDEVICE_PORT1_TOP_MASK 0x00000002
#define XDEVICE_PORT2_TOP_MASK 0x00000004
#define XDEVICE_PORT3_TOP_MASK 0x00000008
#define XDEVICE_PORT0_BOTTOM_MASK 0x00010000
#define XDEVICE_PORT1_BOTTOM_MASK 0x00020000
#define XDEVICE_PORT2_BOTTOM_MASK 0x00040000
#define XDEVICE_PORT3_BOTTOM_MASK 0x00080000

#define XINPUT_DEBUG_KEYSTROKE_FLAG_CTRL 0x01
#define XINPUT_DEBUG_KEYSTROKE_FLAG_SHIFT 0x02
#define XINPUT_DEBUG_KEYSTROKE_FLAG_ALT 0x04
#define XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP 0x40

#define XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN 0x01
#define XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT 0x02
#define XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP 0x04

#define ERROR_SUCCESS 0
#define ERROR_IO_PENDING 0x3e5
#define INFINITE 0xffffffff
#define CREATE_SUSPENDED 4
#define THREAD_PRIORITY_HIGHEST 2
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_BEGIN 0
#define UNSIGNED_CHAR_MAX 0xff
#define SHORT_MAX 0x7fff
#define SHORT_MIN (-0x8000)
#define LONG_BITS 32
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define XDEVICE_TYPE_MEMORY_UNIT ((void *)0x24b218)
#define XDEVICE_TYPE_GAMEPAD ((void *)0x24b29c)
#define XDEVICE_TYPE_DEBUG_KEYBOARD ((void *)0x24b2a8)

typedef void *(__stdcall *xinput_open_proc)(void *device_type, uint32_t port,
                                            uint32_t slot,
                                            void *polling_parameters);
typedef int(__stdcall *xinput_get_state_proc)(void *device, void *state);
typedef int(__stdcall *xget_device_changes_proc)(void *device_type,
                                                 uint32_t *insertions,
                                                 uint32_t *removals);
typedef void(__stdcall *xinput_close_proc)(void *device);
typedef int(__stdcall *xinput_set_state_proc)(void *device, void *feedback);
#define XInputOpen ((xinput_open_proc)(void *)XInputOpen)
#define XInputClose ((xinput_close_proc)(void *)XInputClose)
#define XInputSetState ((xinput_set_state_proc)XInputSetState)
#define XInputGetState ((xinput_get_state_proc)(void *)XInputGetState)
#define XGetDeviceChanges ((xget_device_changes_proc)(void *)XGetDeviceChanges)

typedef struct {
  uint16_t wButtons;
  byte bAnalogButtons[NUMBER_OF_GAMEPAD_ANALOG_BUTTONS];
  int16_t sThumbLX;
  int16_t sThumbLY;
  int16_t sThumbRX;
  int16_t sThumbRY;
} XINPUT_GAMEPAD;

typedef struct {
  uint32_t dwPacketNumber;
  XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

#pragma pack(push, 1)
typedef struct {
  uint32_t dwStatus;
  uint8_t pad_04[0x3e];
  uint16_t wLeftMotorSpeed;
  uint16_t wRightMotorSpeed;
} XINPUT_FEEDBACK;
#pragma pack(pop)
cs(XINPUT_FEEDBACK, 0x46);

typedef struct {
  byte VirtualKey;
  char Ascii;
  byte Flags;
} XINPUT_DEBUG_KEYSTROKE;

typedef struct {
  uint32_t dwFlags;
  uint32_t dwQueueSize;
  uint32_t dwRepeatDelay;
  uint32_t dwRepeatInterval;
} XINPUT_DEBUG_KEYQUEUE_PARAMETERS;

typedef struct {
  void *DeviceType;
  uint32_t dwPreallocCount;
} XDEVICE_PREALLOC_TYPE;

typedef struct key_stroke {
  byte modifier_flags;
  char ascii_code;
  int16_t key_code;
} key_stroke_t;
cs(key_stroke_t, 0x4);

typedef struct vibrate_data {
  uint16_t left_frequency;
  uint16_t right_frequency;
} vibrate_data_t;

typedef struct raw_gamepad_state {
  point2d sticks[NUMBER_OF_GAMEPAD_STICKS];
} raw_gamepad_state_t;
cs(raw_gamepad_state_t, 0x8);

typedef struct input_globals {
  XINPUT_FEEDBACK gamepad_feedbacks[MAXIMUM_GAMEPADS];
  raw_gamepad_state_t raw_gamepad_states[MAXIMUM_GAMEPADS];
  boolean suppressed;
  boolean update_thread_started;
  uint8_t pad_13a[2];
  void *gamepad_handles[MAXIMUM_GAMEPADS];
  gamepad_state_t gamepad_states[MAXIMUM_GAMEPADS];
  gamepad_state_t suppressed_gamepad_state;
  vibrate_data_t gamepad_rumbler_states[MAXIMUM_GAMEPADS];
  int update_thread_handle;
  void *update_event_handle;
  boolean update_event_pending;
  boolean frame_active;
  uint8_t pad_22e[2];
  int32_t keyboard_queue_state;
  void *keyboard_handle;
  byte key_ticks[NUMBER_OF_KEYS];
  byte key_latches[NUMBER_OF_KEYS];
  int16_t buffered_key_read_index;
  int16_t buffered_key_write_index;
  key_stroke_t buffered_keys[MAXIMUM_BUFFERED_KEYSTROKES];
} input_globals_t;
co(input_globals_t, raw_gamepad_states, 0x118);
co(input_globals_t, suppressed, 0x138);
co(input_globals_t, gamepad_handles, 0x13c);
co(input_globals_t, gamepad_states, 0x14c);
co(input_globals_t, suppressed_gamepad_state, 0x1ec);
co(input_globals_t, gamepad_rumbler_states, 0x214);
co(input_globals_t, update_thread_handle, 0x224);
co(input_globals_t, update_event_pending, 0x22c);
co(input_globals_t, frame_active, 0x22d);
co(input_globals_t, keyboard_queue_state, 0x230);
co(input_globals_t, key_ticks, 0x238);
co(input_globals_t, key_latches, 0x2a0);
co(input_globals_t, buffered_key_read_index, 0x308);
co(input_globals_t, buffered_keys, 0x30c);
cs(input_globals_t, 0x40c);

#define input_globals (*(input_globals_t *)input_globals)


/* The original keeps a real call here; input_dispose does not inline it. */
__declspec(noinline) void input_close_state_file(void)
{
  int handle;

  handle = input_state_file_handle;
  if (handle != 0) {
    CloseHandle(handle);
  }
}

void input_check_state_mode(void)
{
  if (file_get_full_attributes("d:\\write.xts") != -1) {
    input_state_mode = _input_state_mode_record;
    return;
  }
  if (file_get_full_attributes("d:\\read.xts") != -1) {
    input_state_mode = _input_state_mode_playback;
    return;
  }
  if (file_get_full_attributes("d:\\loop.xts") != -1) {
    input_state_mode = _input_state_mode_loop;
  }
}

void input_state_read_packet(void *state)
{
  int file_handle;
  uint32_t bytes_read;

  file_handle = input_state_file_handle;
  bytes_read = 0;
  ReadFile(file_handle, state, sizeof(gamepad_state_t),
           &bytes_read, NULL);
}

void input_state_read_packet_looping(void *state)
{
  int file_handle;
  uint32_t bytes_read;

  file_handle = input_state_file_handle;
  bytes_read = 0;
  ReadFile(file_handle, state, sizeof(gamepad_state_t),
           &bytes_read, NULL);
  if (bytes_read == 0) {
    SetFilePointer(input_state_file_handle, 0, NULL, FILE_BEGIN);
    ReadFile(input_state_file_handle, state, sizeof(gamepad_state_t),
             &bytes_read, NULL);
  }
}

void input_recording_write_packet(void *state)
{
  int file_handle;
  uint32_t bytes_written;

  file_handle = input_state_file_handle;
  bytes_written = 0;
  WriteFile(file_handle, state, sizeof(gamepad_state_t),
            &bytes_written, NULL);
}

void input_open_state_file(void)
{
  input_check_state_mode();

  if (input_state_mode == _input_state_mode_record) {
    input_state_file_handle =
      CreateFileA("d:\\state.data", GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                  FILE_FLAG_SEQUENTIAL_SCAN, 0);
    return;
  }
  if (input_state_mode == _input_state_mode_playback ||
      input_state_mode == _input_state_mode_loop) {
    input_state_file_handle =
      CreateFileA("d:\\state.data", GENERIC_READ, 0, 0, OPEN_EXISTING,
                  FILE_FLAG_SEQUENTIAL_SCAN, 0);
  }
}

void input_state_process_packet(void *state)
{
  int file_handle;
  uint32_t bytes_transferred;

  switch (input_state_mode) {
  case _input_state_mode_record:
    file_handle = input_state_file_handle;
    bytes_transferred = 0;
    WriteFile(file_handle, state, sizeof(gamepad_state_t),
              &bytes_transferred, NULL);
    break;
  case _input_state_mode_playback:
    file_handle = input_state_file_handle;
    bytes_transferred = 0;
    ReadFile(file_handle, state, sizeof(gamepad_state_t),
             &bytes_transferred, NULL);
    break;
  case _input_state_mode_loop:
    input_state_read_packet_looping(state);
    break;
  }
}

short fix_dead_zone(short value, short dead_range)
{
  if (value > dead_range) {
    return (short)((value - dead_range) * SHORT_MAX / (SHORT_MAX - dead_range));
  }
  if (value < -dead_range) {
    return (short)((value + dead_range) * SHORT_MIN / (SHORT_MIN + dead_range));
  }
  return 0;
}

void update_ticks(byte *ticks, boolean down)
{
  int value;

  if (down) {
    value = *ticks + 1;
    if (value > UNSIGNED_CHAR_MAX) {
      value = UNSIGNED_CHAR_MAX;
    }
  } else {
    value = 0;
  }
  *ticks = (byte)value;
}

void input_dispose(void)
{
  short gamepad_index;

  input_close_state_file();
  if (input_globals.keyboard_handle != NULL) {
    XInputClose(input_globals.keyboard_handle);
    input_globals.keyboard_handle = NULL;
  }
  for (gamepad_index = 0; gamepad_index < MAXIMUM_GAMEPADS; gamepad_index++) {
    if (input_globals.gamepad_handles[gamepad_index] != NULL) {
      XInputClose(input_globals.gamepad_handles[gamepad_index]);
      input_globals.gamepad_handles[gamepad_index] = NULL;
    }
  }
}

void input_flush(void)
{
  csmemset(input_globals.gamepad_states, 0,
           sizeof(input_globals.gamepad_states));
  csmemset(input_globals.key_ticks, 0, sizeof(input_globals.key_ticks));
  csmemset(input_globals.key_latches, 0, sizeof(input_globals.key_latches));
  input_globals.buffered_key_read_index = 0;
  input_globals.buffered_key_write_index = 0;
  csmemset(input_globals.buffered_keys, 0,
           sizeof(input_globals.buffered_keys));
}

bool input_key_is_down(uint16_t key_code)
{
  short key;
  bool result;

  key = (short)key_code;
  result = false;
  if (!input_globals.suppressed) {
    switch (key) {
    case _key_shift:
      result = MAX(input_globals.key_ticks[_key_left_shift],
                   input_globals.key_ticks[_key_right_shift]);
      break;
    case _key_control:
      result = MAX(input_globals.key_ticks[_key_left_control],
                   input_globals.key_ticks[_key_right_control]);
      break;
    case _key_windows:
      result = MAX(input_globals.key_ticks[_key_left_windows],
                   input_globals.key_ticks[_key_right_windows]);
      break;
    case _key_alt:
      result = MAX(input_globals.key_ticks[_key_left_alt],
                   input_globals.key_ticks[_key_right_alt]);
      break;
    default:
      assert_halt_msg_at("key_code>=0 && key_code<NUMBER_OF_KEYS",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x13a,
                         key >= 0 && key < NUMBER_OF_KEYS);
      result = input_globals.key_ticks[key];
      break;
    }
  }
  return result;
}

bool input_get_buffered_key(void *out_keystroke)
{
  key_stroke_t *key;
  bool result;

  key = out_keystroke;
  result = false;
  if (input_globals.buffered_key_read_index <
      input_globals.buffered_key_write_index) {
    assert_halt_msg_at("input_globals.buffered_key_read_index>=0 && "
                       "input_globals.buffered_key_read_index<"
                       "MAXIMUM_BUFFERED_KEYSTROKES",
                       "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x14e,
                       input_globals.buffered_key_read_index >= 0 &&
                         input_globals.buffered_key_read_index <
                           MAXIMUM_BUFFERED_KEYSTROKES);
    *key = input_globals
             .buffered_keys[input_globals.buffered_key_read_index++];
    result = true;
  }
  return result;
}

void *input_get_mouse_state(void)
{
  return NULL;
}

bool input_has_gamepad(int16_t gamepad_index)
{
  assert_halt_msg_at("gamepad_index>=0 && gamepad_index<MAXIMUM_GAMEPADS",
                     "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x171,
                     gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  return input_globals.gamepad_handles[gamepad_index] != NULL;
}

void *input_get_gamepad_state(int16_t gamepad_index)
{
  gamepad_state_t *result;

  result = NULL;
  assert_halt_msg_at("gamepad_index>=0 && gamepad_index<MAXIMUM_GAMEPADS",
                     "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x17b,
                     gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  if (input_globals.gamepad_handles[gamepad_index] != NULL) {
    if (input_globals.suppressed)
      result = &input_globals.suppressed_gamepad_state;
    else
      result = &input_globals.gamepad_states[gamepad_index];
  }
  return result;
}

void input_set_rumble(int16_t gamepad_index, uint16_t left, uint16_t right)
{
  assert_halt_msg_at("gamepad_index>=0 && gamepad_index<MAXIMUM_GAMEPADS",
                     "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x198,
                     gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  if (!player_ui_rumble_disabled(gamepad_index)) {
    input_globals.gamepad_rumbler_states[gamepad_index].left_frequency = left;
    input_globals.gamepad_rumbler_states[gamepad_index].right_frequency =
      right;
  }
}

void input_tick(void)
{
  if (input_globals.update_event_pending) {
    SetEvent(input_globals.update_event_handle);
  }
  input_globals.update_event_pending = !input_globals.update_event_pending;
}

static void input_update_analog_button_state(byte *threshold, byte value,
                                             boolean pressed)
{
  byte new_threshold;

  if (pressed) {
    if (value < ANALOG_BUTTON_PRESS_THRESHOLD_OFFSET)
      new_threshold = 0;
    else
      new_threshold = value - ANALOG_BUTTON_PRESS_THRESHOLD_OFFSET;
    if (new_threshold > *threshold)
      *threshold = new_threshold;
  } else {
    if (value > ANALOG_BUTTON_RELEASE_SATURATION)
      new_threshold = UNSIGNED_CHAR_MAX;
    else
      new_threshold = value + ANALOG_BUTTON_RELEASE_THRESHOLD_OFFSET;
    if (new_threshold < *threshold)
      *threshold = new_threshold;
  }
}

void input_get_device_states(void)
{
  uint32_t memory_unit_insertions;
  uint32_t memory_unit_removals;
  uint32_t device_change_flags;
  short gamepad_index;

  device_change_flags = 0;
  {
    uint32_t insertions;
    uint32_t removals;

    if (XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals)) {
      for (gamepad_index = 0; gamepad_index < MAXIMUM_GAMEPADS;
           gamepad_index++) {
        if (removals & FLAG(gamepad_index)) {
          assert_halt_msg_at("input_globals.gamepad_handles[gamepad_index]",
                             "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x217,
                             input_globals.gamepad_handles[gamepad_index] !=
                               NULL);
          XInputClose(input_globals.gamepad_handles[gamepad_index]);
          input_globals.gamepad_handles[gamepad_index] = NULL;
        }
        if (insertions & FLAG(gamepad_index)) {
          assert_halt_msg_at(
            "input_globals.gamepad_handles[gamepad_index]==NULL",
            "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x21e,
            input_globals.gamepad_handles[gamepad_index] == NULL);
          input_globals.gamepad_handles[gamepad_index] =
            XInputOpen(XDEVICE_TYPE_GAMEPAD, gamepad_index, XDEVICE_NO_SLOT,
                       NULL);
          if (input_globals.gamepad_handles[gamepad_index] == NULL) {
            error(_error_silent,
                  "XInputOpen (gamepad) failed (#%d) during input_update()",
                  xapi_GetLastError());
          }
        }
      }

      if (removals & FLAG(XDEVICE_PORT0))
        device_change_flags |= FLAG(_input_device_change_gamepad0_removed);
      if (removals & FLAG(XDEVICE_PORT1))
        device_change_flags |= FLAG(_input_device_change_gamepad1_removed);
      if (removals & FLAG(XDEVICE_PORT2))
        device_change_flags |= FLAG(_input_device_change_gamepad2_removed);
      if (removals & FLAG(XDEVICE_PORT3))
        device_change_flags |= FLAG(_input_device_change_gamepad3_removed);
      if (insertions & FLAG(XDEVICE_PORT0))
        device_change_flags |= FLAG(_input_device_change_gamepad0_inserted);
      if (insertions & FLAG(XDEVICE_PORT1))
        device_change_flags |= FLAG(_input_device_change_gamepad1_inserted);
      if (insertions & FLAG(XDEVICE_PORT2))
        device_change_flags |= FLAG(_input_device_change_gamepad2_inserted);
      if (insertions & FLAG(XDEVICE_PORT3))
        device_change_flags |= FLAG(_input_device_change_gamepad3_inserted);
    }
  }

  if (XGetDeviceChanges(XDEVICE_TYPE_MEMORY_UNIT, &memory_unit_insertions,
                        &memory_unit_removals)) {
    if (memory_unit_removals & XDEVICE_PORT0_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit0_removed);
    if (memory_unit_removals & XDEVICE_PORT0_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit1_removed);
    if (memory_unit_removals & XDEVICE_PORT1_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit2_removed);
    if (memory_unit_removals & XDEVICE_PORT1_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit3_removed);
    if (memory_unit_removals & XDEVICE_PORT2_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit4_removed);
    if (memory_unit_removals & XDEVICE_PORT2_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit5_removed);
    if (memory_unit_removals & XDEVICE_PORT3_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit6_removed);
    if (memory_unit_removals & XDEVICE_PORT3_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit7_removed);
    if (memory_unit_insertions & XDEVICE_PORT0_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit0_inserted);
    if (memory_unit_insertions & XDEVICE_PORT0_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit1_inserted);
    if (memory_unit_insertions & XDEVICE_PORT1_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit2_inserted);
    if (memory_unit_insertions & XDEVICE_PORT1_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit3_inserted);
    if (memory_unit_insertions & XDEVICE_PORT2_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit4_inserted);
    if (memory_unit_insertions & XDEVICE_PORT2_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit5_inserted);
    if (memory_unit_insertions & XDEVICE_PORT3_TOP_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit6_inserted);
    if (memory_unit_insertions & XDEVICE_PORT3_BOTTOM_MASK)
      device_change_flags |= FLAG(_input_device_change_memory_unit7_inserted);
  }

  input_abstraction_update_device_changes(device_change_flags);

  for (gamepad_index = 0; gamepad_index < MAXIMUM_GAMEPADS; gamepad_index++) {
    if (input_globals.gamepad_handles[gamepad_index] != NULL) {
      XINPUT_STATE input_state;
      int result;

      result =
        XInputGetState(input_globals.gamepad_handles[gamepad_index],
                       &input_state);
      if (result >= 0) {
        gamepad_state_t *gamepad_state;
        short button_index;

        gamepad_state = &input_globals.gamepad_states[gamepad_index];
        for (button_index = 0; button_index < NUMBER_OF_GAMEPAD_ANALOG_BUTTONS;
             button_index++) {
          gamepad_state->analog_buttons[button_index] =
            input_state.Gamepad
              .bAnalogButtons[gamepad_analog_button_indices[button_index]];
          update_ticks(&gamepad_state->buttons[button_index],
                       gamepad_state->analog_buttons[button_index] >
                         gamepad_state->analog_button_thresholds[button_index]);
          input_update_analog_button_state(
            &gamepad_state->analog_button_thresholds[button_index],
            gamepad_state->analog_buttons[button_index],
            gamepad_state->buttons[button_index] != 0);
        }
        for (button_index = 0; button_index < NUMBER_OF_GAMEPAD_BINARY_BUTTONS;
             button_index++) {
          update_ticks(
            &gamepad_state->buttons[FIRST_GAMEPAD_BINARY_BUTTON + button_index],
            (input_state.Gamepad.wButtons &
             gamepad_binary_button_masks[button_index]) != 0);
        }

        input_globals.raw_gamepad_states[gamepad_index]
          .sticks[_gamepad_stick_left].x = input_state.Gamepad.sThumbLX;
        input_globals.raw_gamepad_states[gamepad_index]
          .sticks[_gamepad_stick_left].y = input_state.Gamepad.sThumbLY;
        input_globals.raw_gamepad_states[gamepad_index]
          .sticks[_gamepad_stick_right].x = input_state.Gamepad.sThumbRX;
        input_globals.raw_gamepad_states[gamepad_index]
          .sticks[_gamepad_stick_right].y = input_state.Gamepad.sThumbRY;

        gamepad_state->sticks[_gamepad_stick_left].x =
          fix_dead_zone(input_state.Gamepad.sThumbLX, GAMEPAD_STICK_DEAD_RANGE);
        gamepad_state->sticks[_gamepad_stick_left].y =
          fix_dead_zone(input_state.Gamepad.sThumbLY, GAMEPAD_STICK_DEAD_RANGE);
        gamepad_state->sticks[_gamepad_stick_right].x =
          fix_dead_zone(input_state.Gamepad.sThumbRX, GAMEPAD_STICK_DEAD_RANGE);
        gamepad_state->sticks[_gamepad_stick_right].y =
          fix_dead_zone(input_state.Gamepad.sThumbRY, GAMEPAD_STICK_DEAD_RANGE);
      } else {
        error(_error_silent,
              "XGetState (gamepad) failed (#%d) during input_update()", result);
      }
    }
  }
}

void input_flush_rumble(void)
{
  boolean suppress_rumble;
  short gamepad_index;

  suppress_rumble = input_globals.suppressed || console_is_active() ||
                    game_time_get_paused() || !game_in_progress();
  for (gamepad_index = 0; gamepad_index < MAXIMUM_GAMEPADS; gamepad_index++) {
    if (input_globals.gamepad_handles[gamepad_index] != NULL) {
      if (input_globals.gamepad_feedbacks[gamepad_index].dwStatus ==
          ERROR_SUCCESS) {
        input_globals.gamepad_feedbacks[gamepad_index].wLeftMotorSpeed =
          suppress_rumble ?
            0 :
            input_globals.gamepad_rumbler_states[gamepad_index].left_frequency;
        input_globals.gamepad_feedbacks[gamepad_index].wRightMotorSpeed =
          suppress_rumble ?
            0 :
            input_globals.gamepad_rumbler_states[gamepad_index].right_frequency;
        XInputSetState(input_globals.gamepad_handles[gamepad_index],
                       &input_globals.gamepad_feedbacks[gamepad_index]);
      } else if (input_globals.gamepad_feedbacks[gamepad_index].dwStatus !=
                 ERROR_IO_PENDING) {
        input_globals.gamepad_feedbacks[gamepad_index].dwStatus = ERROR_SUCCESS;
      }
    }
  }
}

void input_update_keyboard_devices(void)
{
  {
    uint32_t insertions;
    uint32_t removals;

    if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_KEYBOARD, &insertions,
                          &removals)) {
      int keyboard_index;
      int keyboard_count;

      keyboard_index = 0;
      keyboard_count = LONG_BITS;
      while (keyboard_count) {
        uint32_t keyboard_mask;

        keyboard_mask = FLAG(keyboard_index);
        if (removals & keyboard_mask) {
          assert_halt_msg_at("input_globals.keyboard_handle",
                             "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x2d5,
                             input_globals.keyboard_handle != NULL);
          XInputClose(input_globals.keyboard_handle);
          input_globals.keyboard_handle = NULL;
        }
        if (insertions & keyboard_mask) {
          assert_halt_msg_at("input_globals.keyboard_handle==NULL",
                             "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x2dc,
                             input_globals.keyboard_handle == NULL);
          input_globals.keyboard_handle =
            XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, keyboard_index,
                       XDEVICE_NO_SLOT, NULL);
          if (input_globals.keyboard_handle == NULL) {
            error(_error_silent,
                  "XInputOpen (keyboard) failed (#%d) during input_update()",
                  xapi_GetLastError());
          }
        }
        keyboard_index++;
        keyboard_count--;
      }
    }
  }

  {
    short key_code;

    for (key_code = 0; key_code < NUMBER_OF_KEYS; key_code++) {
      update_ticks(&input_globals.key_ticks[key_code],
                   input_globals.key_latches[key_code]);
    }
  }

  input_globals.buffered_key_read_index = 0;
  input_globals.buffered_key_write_index = 0;

  {
    XINPUT_DEBUG_KEYSTROKE keystroke;

    while (XInputDebugGetKeystroke(&keystroke) == ERROR_SUCCESS) {
      key_stroke_t key;

      key.modifier_flags =
        (keystroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_SHIFT) != 0;
      if (keystroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_CTRL)
        key.modifier_flags |= FLAG(_key_modifier_control);
      else
        key.modifier_flags &= ~FLAG(_key_modifier_control);
      if (keystroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_ALT)
        key.modifier_flags |= FLAG(_key_modifier_alt);
      else
        key.modifier_flags &= ~FLAG(_key_modifier_alt);

      assert_halt_msg_at(
        "keystroke.Ascii>=0 && keystroke.Ascii<NUMBER_OF_ASCII_CODES",
        "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x305,
        keystroke.Ascii >= 0 &&
          (int)keystroke.Ascii < NUMBER_OF_ASCII_CODES);
      key.ascii_code =
        ascii_to_key_code[(int)keystroke.Ascii] != NONE ? keystroke.Ascii : NONE;

      assert_halt_msg_at("keystroke.VirtualKey>=0 && "
                         "keystroke.VirtualKey<NUMBER_OF_VIRTUAL_CODES",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x308,
                         keystroke.VirtualKey >= 0 &&
                           (int)keystroke.VirtualKey < NUMBER_OF_VIRTUAL_CODES);
      key.key_code = virtual_key_to_key_code[keystroke.VirtualKey];
      if (key.key_code != NONE) {
        assert_halt_msg_at("key.key_code>=0 && key.key_code<NUMBER_OF_KEYS",
                           "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x30e,
                           key.key_code >= 0 && key.key_code < NUMBER_OF_KEYS);
        if (keystroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP) {
          input_globals.key_latches[key.key_code] = false;
          if (input_globals.key_ticks[key.key_code] > 1) {
            input_globals.key_ticks[key.key_code] = 0;
          }
        } else {
          if (input_globals.buffered_key_write_index <
              MAXIMUM_BUFFERED_KEYSTROKES) {
            input_globals
              .buffered_keys[input_globals.buffered_key_write_index++] = key;
          }
          input_globals.key_latches[key.key_code] = true;
          if (input_globals.key_ticks[key.key_code] == 0) {
            input_globals.key_ticks[key.key_code] = 1;
          }
        }
      }
    }
  }
}

void input_get_raw_data_string(char *buffer, int16_t size)
{
  assert_halt_msg_at("buffer", "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x32b,
                     buffer != NULL);
  assert_halt_msg_at("size>0", "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x32c,
                     size > 0);
  if (buffer != NULL && size > 0) {
    short buffer_length;
    short gamepad_index;

    buffer_length = (short)snprintf(
      buffer, size, "|n|n|n|ngamepad|tleft stick|tright stick|t|n");
    for (gamepad_index = 0; gamepad_index < MAXIMUM_GAMEPADS;
         gamepad_index++) {
      if (input_globals.gamepad_handles[gamepad_index] != NULL) {
        buffer_length += (short)snprintf(
          buffer + buffer_length, size - buffer_length,
          "gamepad %d|t(%d, %d)|t(%d, %d)|n", gamepad_index,
          input_globals.raw_gamepad_states[gamepad_index]
            .sticks[_gamepad_stick_left].x,
          input_globals.raw_gamepad_states[gamepad_index]
            .sticks[_gamepad_stick_left].y,
          input_globals.raw_gamepad_states[gamepad_index]
            .sticks[_gamepad_stick_right].x,
          input_globals.raw_gamepad_states[gamepad_index]
            .sticks[_gamepad_stick_right].y);
      }
    }
  }
}

void input_update(void)
{
  input_globals.suppressed = false;
  if (!input_globals.update_thread_started) {
    ResumeThread(input_globals.update_thread_handle);
    input_globals.update_thread_started = true;
  }
  input_update_keyboard_devices();
  input_state_process_packet(&input_globals.gamepad_states[0]);
  input_state_process_packet(&input_globals.gamepad_states[1]);
  input_state_process_packet(&input_globals.gamepad_states[2]);
  input_state_process_packet(&input_globals.gamepad_states[3]);
}

void input_frame_begin(void)
{
  input_get_device_states();
  input_globals.frame_active = true;
}

void input_frame_end(void)
{
  input_globals.frame_active = false;
}

uint32_t __stdcall input_keyboard_thread(void *context)
{
  for (;;) {
    WaitForSingleObject((int)input_globals.update_event_handle, INFINITE);
    input_flush_rumble();
  }
}

bool input_initialize(void)
{
  XDEVICE_PREALLOC_TYPE device_types[3];
  XINPUT_DEBUG_KEYQUEUE_PARAMETERS keyboard_queue_parameters;
  int result;

  device_types[0].DeviceType = XDEVICE_TYPE_GAMEPAD;
  device_types[0].dwPreallocCount = MAXIMUM_GAMEPADS;
  device_types[1].DeviceType = XDEVICE_TYPE_DEBUG_KEYBOARD;
  device_types[1].dwPreallocCount = 1;
  device_types[2].DeviceType = XDEVICE_TYPE_MEMORY_UNIT;
  device_types[2].dwPreallocCount = MAXIMUM_MEMORY_UNITS;

  csmemset(&input_globals.suppressed, 0,
           sizeof(input_globals_t) - offsetof(input_globals_t, suppressed));
  thunk_XInitDevices(sizeof(device_types) / sizeof(device_types[0]),
                     device_types);

  input_globals.update_event_pending = true;
  input_globals.update_event_handle = CreateEventA(NULL, false, false, NULL);
  input_globals.update_thread_handle =
    (int)CreateThread(NULL, INPUT_UPDATE_THREAD_STACK_SIZE,
                      input_keyboard_thread, NULL, CREATE_SUSPENDED, NULL);
  SetThreadPriority(input_globals.update_thread_handle,
                    THREAD_PRIORITY_HIGHEST);
  input_globals.update_thread_started = true;
  input_globals.keyboard_queue_state = 0;

  keyboard_queue_parameters.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN |
                                      XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT |
                                      XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP;
  keyboard_queue_parameters.dwQueueSize = MAXIMUM_BUFFERED_KEYSTROKES;
  keyboard_queue_parameters.dwRepeatDelay = KEYBOARD_REPEAT_DELAY_MILLISECONDS;
  keyboard_queue_parameters.dwRepeatInterval = KEYBOARD_REPEAT_INTERVAL_MILLISECONDS;
  result = XInputDebugInitKeyboardQueue(&keyboard_queue_parameters);
  if (result < ERROR_SUCCESS) {
    error(_error_silent,
          "XInputDebugInitKeyboardQueue failed (#%d) during input_initialize()",
          result);
  }

  input_open_state_file();
  input_get_device_states();
  input_update();
  input_globals.update_thread_started = false;
  return true;
}
