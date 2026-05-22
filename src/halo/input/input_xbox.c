/* ---- Stick deadzone constants ---- */
/* The Xbox controller thumbstick range is -32768..32767 (int16_t).
   A deadzone of 9000 (~27.5% of full range) eliminates stick drift.
   Values within [-9000, 9000] map to 0. Values above/below are
   linearly rescaled so that 9001→1, 32767→32767 (positive)
   and -9001→-1, -32768→-32768 (negative). The positive and negative
   sides use slightly different scale factors:
     positive: (v - 9000) * 0x7FFF / 0x5CD7
     negative: uses fixed-point reciprocal multiply equivalent to
              (9000 - v) * 0x8000 / 0x5CD7 with arithmetic-shift rounding */

#define STICK_DEADZONE 9000
#define STICK_POSITIVE_MAX 0x7FFF /* max output for positive side */
#define STICK_NEGATIVE_MAX 0x8000 /* max magnitude for negative side */
#define STICK_POSITIVE_RANGE 0x5CD7 /* 32767 - 9000: usable positive range */
/* 0x4F880E79: fixed-point reciprocal for division by 0x5CD7 in negative path */

#define INPUT_ANALOG_BUTTON_COUNT 8
#define INPUT_DIGITAL_BUTTON_COUNT 8
#define INPUT_KEY_COUNT 0x68

#define INPUT_AXIS_KEY_LEFT_X 0x69
#define INPUT_AXIS_KEY_LEFT_Y 0x6A
#define INPUT_AXIS_KEY_RIGHT_X 0x6B
#define INPUT_AXIS_KEY_RIGHT_Y 0x6C

/* ---- Analog button hysteresis thresholds ---- */
#define ANALOG_BTN_DEADZONE 0x20 /* 32/255 ≈ 12.5% — below this = released */
#define ANALOG_BTN_SATURATE 0xBF /* 191/255 ≈ 75% — above this = full press */
#define ANALOG_BTN_RELEASE_DECAY \
  0x40 /* 64 — added each frame during release fade */

/* ---- XInput function pointer typedefs ---- */
typedef int(__stdcall *xinput_get_changes_fn)(void *, uint32_t *, uint32_t *);
typedef int(__stdcall *xinput_open_fn)(void *, int, int, int);
typedef int(__stdcall *xinput_get_state_fn)(int, void *);
typedef void(__stdcall *xinput_close_fn)(int);
typedef int(__stdcall *xset_event_fn)(int);
typedef int(__stdcall *xinput_get_keystroke_fn)(void *);
typedef int(__stdcall *xinput_debug_init_keyboard_fn)(void *);

typedef struct xinput_gamepad {
  uint16_t wButtons;
  uint8_t bAnalogButtons[8];
  int16_t sThumbLX;
  int16_t sThumbLY;
  int16_t sThumbRX;
  int16_t sThumbRY;
} xinput_gamepad;

typedef struct xinput_state {
  uint32_t dwPacketNumber;
  xinput_gamepad Gamepad;
} xinput_state;

typedef struct input_gamepad_state {
  uint8_t raw_analog[INPUT_ANALOG_BUTTON_COUNT];
  uint8_t smoothed_analog[INPUT_ANALOG_BUTTON_COUNT];
  uint8_t analog_hold[INPUT_ANALOG_BUTTON_COUNT];
  uint8_t digital_hold[INPUT_DIGITAL_BUTTON_COUNT];
  int16_t stick_lx;
  int16_t stick_ly;
  int16_t stick_rx;
  int16_t stick_ry;
} input_gamepad_state;

typedef struct input_raw_stick_state {
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
} input_raw_stick_state;

typedef struct input_rumble_state {
  uint16_t left;
  uint16_t right;
} input_rumble_state;

typedef struct input_change_flag_mapping {
  uint32_t source_mask;
  uint32_t change_flag;
} input_change_flag_mapping;

#pragma pack(push, 1)
typedef struct xinput_feedback {
  uint32_t status;
  uint8_t header[0x3e];
  uint16_t left_motor;
  uint16_t right_motor;
} xinput_feedback;
#pragma pack(pop)

cs(xinput_feedback, 0x46);
co(xinput_feedback, status, 0x0);
co(xinput_feedback, left_motor, 0x42);
co(xinput_feedback, right_motor, 0x44);
cs(input_gamepad_state, 0x28);
cs(input_raw_stick_state, 0x8);
cs(input_rumble_state, 0x4);
co(input_gamepad_state, raw_analog, 0x0);
co(input_gamepad_state, smoothed_analog, 0x8);
co(input_gamepad_state, analog_hold, 0x10);
co(input_gamepad_state, digital_hold, 0x18);
co(input_gamepad_state, stick_lx, 0x20);
co(input_gamepad_state, stick_ly, 0x22);
co(input_gamepad_state, stick_rx, 0x24);
co(input_gamepad_state, stick_ry, 0x26);

static const input_change_flag_mapping k_gamepad_removal_change_flags[4] = {
  { 0x1, 0x1 },
  { 0x2, 0x2 },
  { 0x4, 0x4 },
  { 0x8, 0x8 },
};

static const input_change_flag_mapping k_gamepad_insertion_change_flags[4] = {
  { 0x1, 0x1000 },
  { 0x2, 0x2000 },
  { 0x4, 0x4000 },
  { 0x8, 0x8000 },
};

static const input_change_flag_mapping k_mu_removal_change_flags[8] = {
  { 0x1, 0x10 },  { 0x10000, 0x20 },  { 0x2, 0x40 },  { 0x20000, 0x80 },
  { 0x4, 0x100 }, { 0x40000, 0x200 }, { 0x8, 0x400 }, { 0x80000, 0x800 },
};

static const input_change_flag_mapping k_mu_insertion_change_flags[8] = {
  { 0x1, 0x10000 },     { 0x10000, 0x20000 },  { 0x2, 0x40000 },
  { 0x20000, 0x80000 }, { 0x4, 0x100000 },     { 0x40000, 0x200000 },
  { 0x8, 0x400000 },    { 0x80000, 0x800000 },
};

static char *input_suppressed(void)
{
  return (char *)0x46ba38;
}

static char *input_initialized(void)
{
  return (char *)0x46ba39;
}

static int *input_gamepad_handles(void)
{
  return (int *)0x46ba3c;
}

static input_gamepad_state *input_gamepad_states(void)
{
  return (input_gamepad_state *)0x46ba4c;
}

static input_raw_stick_state *input_raw_stick_states(void)
{
  return (input_raw_stick_state *)0x46ba18;
}

static void *suppressed_gamepad_state(void)
{
  return (void *)0x46baec;
}

static input_rumble_state *input_rumble_states(void)
{
  return (input_rumble_state *)0x46bb14;
}

static int *input_update_callback_arg(void)
{
  return (int *)0x46bb24;
}

static int *input_update_event_handle(void)
{
  return (int *)0x46bb28;
}

static uint8_t *input_update_event_pending(void)
{
  return (uint8_t *)0x46bb2c;
}

static int *input_keyboard_handle(void)
{
  return (int *)0x46bb34;
}

static int *input_state_file_handle(void)
{
  return (int *)0x46b814;
}

static int *input_state_mode(void)
{
  return (int *)0x46b818;
}

static xinput_feedback *input_feedback_states(void)
{
  return (xinput_feedback *)0x46b900;
}

static uint8_t *input_digital_button_states(void)
{
  return (uint8_t *)0x46bb38;
}

/* Stick-axis-as-key state bytes: each axis is decomposed into positive and
   negative halves, stored at fixed offsets within the key state region.
   input_key_is_down returns max(a, b) for each axis pair. */
static uint8_t *input_stick_axis_key_neg_lx(void)
{
  return (uint8_t *)0x46bb71;
}

static uint8_t *input_stick_axis_key_pos_lx(void)
{
  return (uint8_t *)0x46bb7c;
}

static uint8_t *input_stick_axis_key_neg_ly(void)
{
  return (uint8_t *)0x46bb7d;
}

static uint8_t *input_stick_axis_key_neg_rx(void)
{
  return (uint8_t *)0x46bb7e;
}

static uint8_t *input_stick_axis_key_neg_ry(void)
{
  return (uint8_t *)0x46bb7f;
}

static uint8_t *input_stick_axis_key_pos_ry(void)
{
  return (uint8_t *)0x46bb81;
}

static uint8_t *input_stick_axis_key_pos_rx(void)
{
  return (uint8_t *)0x46bb82;
}

static uint8_t *input_stick_axis_key_pos_ly(void)
{
  return (uint8_t *)0x46bb84;
}

static uint8_t *input_analog_button_states(void)
{
  return (uint8_t *)0x46bba0;
}

static const uint8_t *input_analog_button_index_map(void)
{
  return (const uint8_t *)0x281150;
}

static const uint8_t *input_digital_button_bitmask_map(void)
{
  return (const uint8_t *)0x281158;
}

static int16_t *input_vkey_to_keycode_table(void)
{
  return (int16_t *)0x281160;
}

static int16_t *input_ascii_remap_table(void)
{
  return (int16_t *)0x281360;
}

static uint32_t *input_buffered_key_ring(void)
{
  return (uint32_t *)0x46bc0c;
}

static uint8_t input_saturating_increment(uint8_t value)
{
  if (value == 0xFF)
    return value;
  return value + 1;
}

static uint32_t
input_apply_change_flags(uint32_t change_flags, uint32_t source_mask,
                         const input_change_flag_mapping *mappings,
                         int mapping_count)
{
  int i;

  for (i = 0; i < mapping_count; i++) {
    if (source_mask & mappings[i].source_mask)
      change_flags |= mappings[i].change_flag;
  }

  return change_flags;
}

static void input_update_analog_button_state(input_gamepad_state *gamepad,
                                             int button_index, uint8_t raw)
{
  uint8_t smoothed;

  smoothed = gamepad->smoothed_analog[button_index];
  gamepad->raw_analog[button_index] = raw;
  if (raw > smoothed)
    gamepad->analog_hold[button_index] =
      input_saturating_increment(gamepad->analog_hold[button_index]);
  else
    gamepad->analog_hold[button_index] = 0;

  raw = gamepad->raw_analog[button_index];
  if (gamepad->analog_hold[button_index] != 0) {
    if (raw < ANALOG_BTN_DEADZONE)
      raw = 0;
    else
      raw -= ANALOG_BTN_DEADZONE;

    if (raw > smoothed)
      smoothed = raw;
  } else {
    if (raw > ANALOG_BTN_SATURATE)
      raw = 0xFF;
    else
      raw += ANALOG_BTN_RELEASE_DECAY;

    if (raw < smoothed)
      smoothed = raw;
  }

  gamepad->smoothed_analog[button_index] = smoothed;
}

static void input_update_digital_button_state(input_gamepad_state *gamepad,
                                              int button_index,
                                              uint16_t buttons)
{
  if ((input_digital_button_bitmask_map()[button_index] & buttons) != 0)
    gamepad->digital_hold[button_index] =
      input_saturating_increment(gamepad->digital_hold[button_index]);
  else
    gamepad->digital_hold[button_index] = 0;
}

static int16_t input_normalize_stick(int16_t value)
{
  int d;
  int scaled;

  if (value > STICK_DEADZONE)
    return (int16_t)(((int)value - STICK_DEADZONE) * STICK_POSITIVE_MAX /
                     STICK_POSITIVE_RANGE);

  if (value < -STICK_DEADZONE) {
    d = -STICK_DEADZONE - (int)value;
    scaled = (int)((unsigned long long)((long long)(d * STICK_NEGATIVE_MAX) *
                                        0x4f880e79LL) >>
                   32) +
             d * -STICK_NEGATIVE_MAX;
    return (int16_t)((scaled >> 14) - (scaled >> 31));
  }

  return 0;
}

/* input_check_state_mode (0xce4b0)
 * Probe for sentinel files on the D: drive to select input state recording
 * or playback mode.  Sets the global input_state_mode:
 *   d:\write.xts exists  ->  mode 3 (write/record)
 *   d:\read.xts  exists  ->  mode 4 (read/playback)
 *   d:\loop.xts  exists  ->  mode 5 (loop playback)
 * First match wins; if none exist the mode is left unchanged. */
void input_check_state_mode(void)
{
  if (file_get_full_attributes("d:\\write.xts") != -1) {
    *input_state_mode() = 3;
    return;
  }
  if (file_get_full_attributes("d:\\read.xts") != -1) {
    *input_state_mode() = 4;
    return;
  }
  if (file_get_full_attributes("d:\\loop.xts") != -1) {
    *input_state_mode() = 5;
  }
}

/* FUN_000ce500 (0xce500)
 * Reads 0x28 bytes from the state file into the buffer via ReadFile. */
void FUN_000ce500(void *param_1)
{
  void *buf;
  int handle;
  uint32_t local_8;

  buf = param_1;
  handle = *(int *)0x46b814;
  local_8 = 0;
  ReadFile(handle, buf, 0x28, &local_8, 0);
}

/* FUN_000ce530 (0xce530) — read input state packet with seek-and-retry
 *
 * Reads 0x28 bytes from the state file into the provided buffer. If no
 * bytes were read (end-of-file or first read), seeks back to the start
 * of the file and retries once. Used for loop-playback of recorded input. */
void FUN_000ce530(void *state)
{
  uint32_t bytes_read;

  bytes_read = 0;
  ReadFile(*(int *)0x46b814, state, 0x28, &bytes_read, NULL);
  if (bytes_read == 0) {
    FUN_001d1610(*(int *)0x46b814, 0, NULL, 0);
    ReadFile(*(int *)0x46b814, state, 0x28, &bytes_read, NULL);
  }
}

/* input_recording_write_packet (0xce590)
 * Write one input_gamepad_state packet (0x28 bytes) to the input state
 * recording file.  Called unconditionally — the caller is responsible for
 * checking the recording mode before invoking this. */
void input_recording_write_packet(void *state)
{
  uint32_t bytes_written;
  bytes_written = 0;
  WriteFile(*input_state_file_handle(), state, sizeof(input_gamepad_state),
            &bytes_written, NULL);
}

/* input_open_state_file (0xce5c0)
 * Check for sentinel files to select input state recording/playback mode,
 * then open d:\state.data with the appropriate access flags. */
void input_open_state_file(void)
{
  input_check_state_mode();

  if (*input_state_mode() == 3) {
    *input_state_file_handle() =
      CreateFileA("d:\\state.data", 0x40000000, 0, 0, 2, 0x8000000, 0);
    return;
  }
  if (*input_state_mode() == 4 || *input_state_mode() == 5) {
    *input_state_file_handle() =
      CreateFileA("d:\\state.data", 0x80000000, 0, 0, 3, 0x8000000, 0);
  }
}

/* input_state_process_packet (0xce620)
 * Dispatch one input_gamepad_state packet according to the current input
 * state mode:
 *   mode 3 (record)  — write packet to the state file
 *   mode 4 (playback)— read  packet from the state file
 *   mode 5 (loop)    — delegate to FUN_000ce530 for loop-playback handling */
void input_state_process_packet(void *state)
{
  uint32_t bytes_transferred;
  switch (*input_state_mode()) {
  case 3:
    bytes_transferred = 0;
    WriteFile(*input_state_file_handle(), state, sizeof(input_gamepad_state),
              &bytes_transferred, NULL);
    break;
  case 4:
    bytes_transferred = 0;
    ReadFile(*input_state_file_handle(), state, sizeof(input_gamepad_state),
             &bytes_transferred, NULL);
    break;
  case 5:
    FUN_000ce530(state);
    break;
  }
}

/* FUN_000cf3e0 (0xcf3e0)
 * Remaps a short value from range [-param_2..param_2] to
 * [-0x8000..0x7fff], returning the sign bit if in the dead zone. */
int FUN_000cf3e0(short param_1, short param_2)
{
  if (param_1 > param_2) {
    return (((int)param_1 - (int)param_2) * 0x7fff) / (0x7fff - (int)param_2);
  }
  if ((int)param_1 < -(int)param_2) {
    return (((int)param_1 + (int)param_2) * -0x8000) / ((int)param_2 + -0x8000);
  }
  return (int)param_1 & ~0xFFFF;
}

/* FUN_000cf430 (0xcf430)
 * Saturating byte counter update. If increment is zero, clears the
 * counter. Otherwise increments it, clamping at 0xff.
 *
 * Reference layout (je/jle): increment path inline, saturation before
 * shared store; set_to_0 is the je target. */
void FUN_000cf430(uint8_t *counter, char increment)
{
  int new_val;

  if (increment != '\0') {
    new_val = (int)*counter + 1;
    if (new_val <= 0xff) {
      goto store;
    }
    new_val = 0xff;
    *counter = (uint8_t)new_val;
    return;
  }
  new_val = 0;
store:
  *counter = (uint8_t)new_val;
}

/* FUN_000cf490 (0xcf490) */
void FUN_000cf490(void)
{
  int *piVar1;
  int iVar2;
  int handle;

  FUN_000ce4a0();
  if (*input_keyboard_handle() != 0) {
    ((xinput_close_fn)0x24c1b8)(*input_keyboard_handle());
    *input_keyboard_handle() = 0;
  }
  piVar1 = input_gamepad_handles();
  iVar2 = 4;
  do {
    if (*piVar1 != 0) {
      handle = *piVar1;
      ((xinput_close_fn)0x24c1b8)(handle);
      *piVar1 = 0;
    }
    piVar1 = piVar1 + 1;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  return;
}

void input_flush(void)
{
  csmemset(input_gamepad_states(), 0,
           sizeof(input_gamepad_state) * MAXIMUM_GAMEPADS);
  csmemset(input_digital_button_states(), 0, INPUT_KEY_COUNT);
  csmemset(input_analog_button_states(), 0, INPUT_KEY_COUNT);
  word_46BC08 = 0;
  word_46BC0A = 0;
  csmemset(dword_46BC0C, 0, 0x100u);
}

bool input_key_is_down(uint16_t key_code)
{
  int16_t key = (int16_t)key_code;
  uint8_t a, b;

  if (*input_suppressed())
    return false;
  switch (key - INPUT_AXIS_KEY_LEFT_X) {
  case 0: /* left stick X */
    a = *input_stick_axis_key_pos_lx();
    b = *input_stick_axis_key_neg_lx();
    return a < b ? b : a;
  case 1: /* left stick Y */
    a = *input_stick_axis_key_pos_ly();
    b = *input_stick_axis_key_neg_ly();
    return a < b ? b : a;
  case 2: /* right stick X */
    /* The binary uses <= here, unlike the left-stick cases. Preserve that tie
     * behavior. */
    a = *input_stick_axis_key_neg_rx();
    b = *input_stick_axis_key_pos_rx();
    return a <= b ? b : a;
  case 3: /* right stick Y */
    a = *input_stick_axis_key_neg_ry();
    b = *input_stick_axis_key_pos_ry();
    return a <= b ? b : a;
  default:
    assert_halt(key >= 0 && key < INPUT_KEY_COUNT);
    return input_digital_button_states()[key];
  }
}

/* Dequeue the next keystroke from the ring buffer.
 *
 * The buffer holds up to 64 entries (MAXIMUM_BUFFERED_KEYSTROKES = 0x40).
 * word_46BC08 = read index, word_46BC0A = write index.
 * Returns true and copies one dword-sized keystroke into *out_keystroke if a
 * key is available; returns false immediately if the buffer is empty.
 *
 * Note: does NOT check input_suppressed — buffered keys always drain even
 * when input is globally suppressed. */
bool input_get_buffered_key(void *out_keystroke)
{
  int16_t read_idx;

  if (word_46BC08 >= word_46BC0A)
    return false;

  assert_halt(word_46BC08 >= 0 && word_46BC08 < 0x40);

  read_idx = word_46BC08;
  *(int *)out_keystroke = dword_46BC0C[read_idx];
  word_46BC08 = word_46BC08 + 1;
  return true;
}

/* FUN_000cf690 (0xcf690)
 * Returns 0 (stub/always-false check). */
int FUN_000cf690(void)
{
  return 0;
}

bool input_has_gamepad(int16_t gamepad_index)
{
  assert_halt(gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  return input_gamepad_handles()[gamepad_index] != 0;
}

void *input_get_gamepad_state(int gamepad_index)
{
  int16_t index;

  index = (int16_t)gamepad_index;
  assert_halt(index >= 0 && index < MAXIMUM_GAMEPADS);
  if (input_gamepad_handles()[index] != 0) {
    if (*input_suppressed())
      return suppressed_gamepad_state();
    return &input_gamepad_states()[index];
  }
  return NULL;
}

void input_set_rumble(int16_t gamepad_index, uint16_t left, uint16_t right)
{
  assert_halt(gamepad_index >= 0 && gamepad_index < MAXIMUM_GAMEPADS);
  if (!((bool (*)(int16_t))0xe0b00)(gamepad_index)) {
    input_rumble_states()[gamepad_index].left = left;
    input_rumble_states()[gamepad_index].right = right;
  }
}

void input_tick(void)
{
  bool pending_cleared;

  pending_cleared = *input_update_event_pending() == 0;
  if (!pending_cleared) {
    ((xset_event_fn)0x1cfeaa)(*input_update_event_handle());
    pending_cleared = *input_update_event_pending() == 0;
  }

  *input_update_event_pending() = pending_cleared;
}

void input_get_device_states(void)
{
  uint32_t insertions, removals;
  uint32_t mu_insertions, mu_removals;
  uint32_t change_flags;
  int *handles;
  input_gamepad_state *gamepad_states;
  input_raw_stick_state *raw_stick_states;
  const uint8_t *analog_button_index_map;
  uint32_t mask;
  int i, j;
  int result;
  xinput_state state;
  xinput_gamepad *input_state;
  uint8_t raw;

  change_flags = 0;
  result =
    ((xinput_get_changes_fn)0x24c954)((void *)0x24b29c, &insertions, &removals);
  if (result != 0) {
    handles = input_gamepad_handles();
    for (i = 0; i < MAXIMUM_GAMEPADS; i++) {
      mask = 1 << i;
      if (removals & mask) {
        if (handles[i] == 0) {
          display_assert("input_globals.gamepad_handles[gamepad_index]",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x217, 1);
          system_exit(-1);
        }
        ((xinput_close_fn)0x24c1b8)(handles[i]);
        handles[i] = 0;
      }
      if (insertions & mask) {
        if (handles[i] != 0) {
          display_assert("input_globals.gamepad_handles[gamepad_index]==NULL",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x21e, 1);
          system_exit(-1);
        }
        handles[i] = ((xinput_open_fn)0x24c143)((void *)0x24b29c, i, 0, 0);
        if (handles[i] == 0) {
          result = ((int(__stdcall *)(void))0x1d2240)();
          error(2, "XInputOpen (gamepad) failed (#%d) during input_update()",
                result);
        }
      }
    }
    change_flags = input_apply_change_flags(change_flags, removals,
                                            k_gamepad_removal_change_flags, 4);
    change_flags = input_apply_change_flags(
      change_flags, insertions, k_gamepad_insertion_change_flags, 4);
  }

  result = ((xinput_get_changes_fn)0x24c954)((void *)0x24b218, &mu_insertions,
                                             &mu_removals);
  if (result != 0) {
    /* MU (memory unit) insertion/removal flags.
     * XInput packs controller index (bits 0-3) and MU slot (bits 16-19)
     * into a single bitmask. The mapping below translates XInput bit
     * positions to internal change_flags bit positions:
     *
     *   XInput bit     →  change_flags bit   meaning
     *   ─────────────────────────────────────────────────────────
     *   bit 0  (slot A) → 0x010  MU A removed  │  0x010000  MU A inserted
     *   bit 16 (MU A)   → 0x020  MU A port gone │  0x020000  MU A port added
     *   bit 1  (slot B) → 0x040  MU B removed  │  0x040000  MU B inserted
     *   bit 17 (MU B)   → 0x080  MU B port gone │  0x080000  MU B port added
     *   bit 2  (slot C) → 0x100  MU C removed  │  0x100000  MU C inserted
     *   bit 18 (MU C)   → 0x200  MU C port gone │  0x200000  MU C port added
     *   bit 3  (slot D) → 0x400  MU D removed  │  0x400000  MU D inserted
     *   bit 19 (MU D)   → 0x800  MU D port gone │  0x800000  MU D port added
     */
    change_flags = input_apply_change_flags(change_flags, mu_removals,
                                            k_mu_removal_change_flags, 8);
    change_flags = input_apply_change_flags(change_flags, mu_insertions,
                                            k_mu_insertion_change_flags, 8);
  }

  ((void (*)(uint32_t))0xce840)(change_flags);

  handles = input_gamepad_handles();
  gamepad_states = input_gamepad_states();
  raw_stick_states = input_raw_stick_states();
  analog_button_index_map = input_analog_button_index_map();
  for (i = 0; i < MAXIMUM_GAMEPADS; i++) {
    if (handles[i] != 0) {
      result = ((xinput_get_state_fn)0x24c3b6)(handles[i], &state);
      if (result < 0) {
        error(2, "XGetState (gamepad) failed (#%d) during input_update()",
              result);
      } else {
        input_state = &state.Gamepad;

        /* analog buttons (8): smoothing with hysteresis */
        for (j = 0; j < INPUT_ANALOG_BUTTON_COUNT; j++) {
          raw = input_state->bAnalogButtons[analog_button_index_map[j]];
          input_update_analog_button_state(&gamepad_states[i], j, raw);
        }

        /* digital buttons (8): press duration */
        for (j = 0; j < INPUT_DIGITAL_BUTTON_COUNT; j++) {
          input_update_digital_button_state(&gamepad_states[i], j,
                                            input_state->wButtons);
        }

        raw_stick_states[i].lx = input_state->sThumbLX;
        raw_stick_states[i].ly = input_state->sThumbLY;
        raw_stick_states[i].rx = input_state->sThumbRX;
        raw_stick_states[i].ry = input_state->sThumbRY;

        gamepad_states[i].stick_lx =
          input_normalize_stick(input_state->sThumbLX);
        gamepad_states[i].stick_ly =
          input_normalize_stick(input_state->sThumbLY);
        gamepad_states[i].stick_rx =
          input_normalize_stick(input_state->sThumbRX);
        gamepad_states[i].stick_ry =
          input_normalize_stick(input_state->sThumbRY);
      }
    }
  }
}

#define ERROR_IO_PENDING 0x3e5

/* input_flush_rumble (0xcfd00)
 * Send rumble motor commands to all connected gamepads. Called from
 * input_keyboard_thread. Motors are silenced when input is suppressed or
 * game state does not warrant rumble. */
void input_flush_rumble(void)
{
  bool suppress;
  int handle;
  int i;
  xinput_feedback *feedback;

  if (!*input_suppressed() && !console_is_active() && !game_time_get_paused() &&
      game_in_progress()) {
    suppress = false;
  } else {
    suppress = true;
  }

  feedback = input_feedback_states();
  for (i = 0; i < MAXIMUM_GAMEPADS; i++) {
    handle = input_gamepad_handles()[i];
    if (handle != 0) {
      if (feedback->status == 0) {
        feedback->left_motor = suppress ? 0 : input_rumble_states()[i].left;
        feedback->right_motor = suppress ? 0 : input_rumble_states()[i].right;
        XInputSetState(handle, feedback);
      } else if (feedback->status != ERROR_IO_PENDING) {
        feedback->status = 0;
      }
    }
    feedback++;
  }
}

/* input_update_keyboard_devices (0xcfdb0)
 *
 * Hot-plug XDEVICE_TYPE_DEBUG_KEYBOARD handles, age held-key frame counters,
 * and drain the XInput keystroke ring into the digital/analog key state arrays
 * and the 64-entry buffered-key ring at 0x46bc0c.
 *
 * Keystroke packed event layout (dword stored in ring):
 *   bits 31-16: key_code (0x0000-0x0067)
 *   bits 15-8:  remapped ASCII (0xff if not in remap table)
 *   bits  7-0:  modifier flags (bits 0/1 are flags bits 1/0 swapped; bit 2
 *               is flags bit 2) */
void input_update_keyboard_devices(void)
{
  uint32_t insertions, removals;
  uint32_t mask;
  int keyboard_handle;
  int err;
  int i;
  uint8_t *digital;
  uint8_t *analog;
  uint32_t keystroke;
  uint8_t ascii_byte;
  uint8_t flags_byte;
  uint8_t modifier;
  uint8_t ascii_remap;
  int16_t key_code;
  uint32_t event_packed;
  uint32_t hold;

  if (((xinput_get_changes_fn)0x24c954)((void *)0x24b2a8, &insertions,
                                        &removals)) {
    keyboard_handle = *input_keyboard_handle();
    for (i = 0; i < 32; i++) {
      mask = 1u << i;
      if (removals & mask) {
        if (keyboard_handle == 0) {
          display_assert("input_globals.keyboard_handle",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x2d5, 1);
          system_exit(-1);
          keyboard_handle = *input_keyboard_handle();
        }
        ((xinput_close_fn)0x24c1b8)(keyboard_handle);
        keyboard_handle = 0;
        *input_keyboard_handle() = 0;
      }
      if (insertions & mask) {
        if (keyboard_handle != 0) {
          display_assert("input_globals.keyboard_handle==NULL",
                         "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x2dc, 1);
          system_exit(-1);
        }
        *input_keyboard_handle() =
          ((xinput_open_fn)0x24c143)((void *)0x24b2a8, i, 0, 0);
        keyboard_handle = *input_keyboard_handle();
        if (*input_keyboard_handle() == 0) {
          err = ((int(__stdcall *)(void))0x1d2240)();
          error(2, "XInputOpen (keyboard) failed (#%d) during input_update()",
                err);
          keyboard_handle = *input_keyboard_handle();
        }
      }
    }
  }

  /* aging pass: saturating-increment held-key counters, zero released keys */
  digital = input_digital_button_states();
  analog = input_analog_button_states();
  for (i = 0; i < INPUT_KEY_COUNT; i++) {
    hold = analog[i] ? (uint32_t)digital[i] + 1 : 0;
    digital[i] = (uint8_t)(hold > 0xff ? 0xff : hold);
  }

  /* reset buffered-key ring before draining new keystrokes */
  *(uint16_t *)0x46bc08 = 0;
  *(uint16_t *)0x46bc0a = 0;

  while (((xinput_get_keystroke_fn)0x252ad9)(&keystroke) == 0) {
    ascii_byte = (uint8_t)(keystroke >> 8);
    flags_byte = (uint8_t)(keystroke >> 16);
    modifier = (uint8_t)(((flags_byte >> 1) & 1) | ((flags_byte & 1) << 1) |
                         (flags_byte & 4));

    if (ascii_byte >= 0x80) {
      display_assert(
        "keystroke.Ascii>=0 && keystroke.Ascii<NUMBER_OF_ASCII_CODES",
        "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x305, 1);
      system_exit(-1);
    }

    ascii_remap = (input_ascii_remap_table()[ascii_byte] != -1) ? ascii_byte :
                                                                  (uint8_t)0xff;

    /* VirtualKey is 1 byte (0-255), so the < NUMBER_OF_VIRTUAL_CODES (256)
     * assert can never fail; included to mirror the original binary */
    if ((uint32_t)(uint8_t)keystroke >= 0x100u) {
      display_assert("keystroke.VirtualKey>=0 && "
                     "keystroke.VirtualKey<NUMBER_OF_VIRTUAL_CODES",
                     "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x308, 1);
      system_exit(-1);
    }

    key_code = input_vkey_to_keycode_table()[(uint8_t)keystroke];

    if (key_code != -1) {
      if (key_code < 0 || key_code >= INPUT_KEY_COUNT) {
        display_assert("key.key_code>=0 && key.key_code<NUMBER_OF_KEYS",
                       "c:\\halo\\SOURCE\\input\\input_xbox.c", 0x30e, 1);
        system_exit(-1);
      }

      if (flags_byte & 0x40) {
        analog[key_code] = 0;
        if (digital[key_code] > 1)
          digital[key_code] = 0;
      } else {
        event_packed = ((uint32_t)(uint16_t)key_code << 16) |
                       ((uint32_t)ascii_remap << 8) | modifier;
        if (*(uint16_t *)0x46bc0a < 0x40) {
          input_buffered_key_ring()[*(uint16_t *)0x46bc0a] = event_packed;
          (*(uint16_t *)0x46bc0a)++;
        }
        analog[key_code] = 1;
        if (digital[key_code] == 0)
          digital[key_code] = 1;
      }
    }
  }
}

void input_update(void)
{
  int i;

  *input_suppressed() = 0;
  if (!*input_initialized()) {
    ((void(__stdcall *)(int))0x1cfaec)(*input_update_callback_arg());
    *input_initialized() = 1;
  }
  input_update_keyboard_devices();
  for (i = 0; i < MAXIMUM_GAMEPADS; i++)
    input_state_process_packet(&input_gamepad_states()[i]);
}

void input_frame_begin(void)
{
  input_get_device_states();
  input_frame_tick = 1;
}

void input_frame_end(void)
{
  input_frame_tick = 0;
}

/* input_keyboard_thread (0xd01a0) — keyboard event thread.
 * Waits on the keyboard event then flushes rumble state.  Created suspended
 * by input_initialize and resumed on the first call to input_update. */
static uint32_t __stdcall input_keyboard_thread(void *param)
{
  (void)param;
  for (;;) {
    ((void(__stdcall *)(int, uint32_t))0x1d0336)(*input_update_event_handle(),
                                                 0xffffffff);
    input_flush_rumble();
  }
}

/* input_initialize (0xd01c0) */
int input_initialize(void)
{
  struct {
    void *ptype;
    uint32_t max_devices;
  } device_types[3];
  struct {
    uint32_t flags;
    uint32_t queue_size;
    uint32_t repeat_delay;
    uint32_t repeat_period;
  } kbd_params;
  int thread_handle;
  int result;

  device_types[0].ptype = (void *)0x24b29c; /* XDEVICE_TYPE_GAMEPAD */
  device_types[0].max_devices = 4;
  device_types[1].ptype = (void *)0x24b2a8; /* XDEVICE_TYPE_DEBUG_KEYBOARD */
  device_types[1].max_devices = 1;
  device_types[2].ptype = (void *)0x24b218; /* XDEVICE_TYPE_MEMORY_UNIT */
  device_types[2].max_devices = 8;

  csmemset((void *)0x46ba38, 0, 0x2d4);
  ((void(__stdcall *)(int, void *))0x24c92d)(3, device_types);

  *input_update_event_pending() = 1;
  *input_update_event_handle() =
    ((int(__stdcall *)(void *, int, int, const char *))0x1cfded)(NULL, 0, 0,
                                                                 NULL);

  thread_handle =
    ((int(__stdcall *)(void *, int, void *, void *, int, void *))0x1cfd8c)(
      NULL, 0x4000, (void *)input_keyboard_thread, NULL, 4, NULL);
  *input_update_callback_arg() = thread_handle;
  ((void(__stdcall *)(int, int))0x1cf999)(thread_handle, 2);

  *input_initialized() = 1;
  *(uint32_t *)0x46bb30 = 0;

  kbd_params.flags = 7;
  kbd_params.queue_size = 0x40;
  kbd_params.repeat_delay = 500;
  kbd_params.repeat_period = 100;
  result = ((xinput_debug_init_keyboard_fn)0x252d73)(&kbd_params);
  if (result < 0)
    error(2,
          "XInputDebugInitKeyboardQueue failed (#%d) during input_initialize()",
          result);

  input_open_state_file();
  input_get_device_states();
  input_update();

  *input_initialized() = 0;
  return 1;
}
