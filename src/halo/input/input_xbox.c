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

/* ---- Per-gamepad state layout (0x28 bytes each, 4 total = 0xA0) ----
 *   offset  size  field
 *   0x00    8     raw_analog[8]       latest analog button readings
 *   0x08    8     smoothed_analog[8]  hysteresis-smoothed analog values
 *   0x10    8     analog_hold[8]      press-and-hold counter for analog buttons
 *   0x18    8     digital_hold[8]     press duration counter for digital buttons
 *   0x20    2     stick_lx            normalized left thumb X
 *   0x22    2     stick_ly            normalized left thumb Y
 *   0x24    2     stick_rx            normalized right thumb X
 *   0x26    2     stick_ry            normalized right thumb Y
 *   0x28    -     (next gamepad)
 */

/* ---- Input globals address map ----
 *   0x46BA38  char    input_suppressed
 *   0x46BA39  char    input_initialized
 *   0x46BA3C  int[4]  gamepad_handles
 *   0x46BA4C  char[]  gamepad_states[4][0x28]
 *   0x46BA1A  int16[] raw_stick_values[4][4]  (LX,LY,RX,RY per pad)
 *   0x46BAEC  void*   suppressed_gamepad_state  (returned when input is
 *                    suppressed)
 *   0x46BB14  struct  rumble[4] { uint16_t left; uint16_t right; }
 *   0x46BB24  int     input_update_callback_arg
 *   0x46BB2D  char    input_frame_tick
 *   0x46BB38  char[]  digital_button_states[0x68]
 *   0x46BBA0  char[]  analog_button_states[0x68]
 *   0x46BB70  uint8   left_trigger_a
 *   0x46BB71  uint8   left_trigger_b
 *   0x46BB7C  uint8   right_trigger_a
 *   0x46BB7D  uint8   right_trigger_b
 *   0x46BB7E  uint8   left_shoulder_a
 *   0x46BB7F  uint8   left_shoulder_b
 *   0x46BB81  uint8   right_shoulder_b
 *   0x46BB82  uint8   right_shoulder_a(?)
 *   0x46BB84  uint8   another_trigger_a
 *   0x46BC08  int16   mu_insertion_count
 *   0x46BC0A  int16   mu_removal_count
 *   0x46BC0C  int32   mu_change_events[64]
 */

/* ---- XInput function pointer typedefs ---- */
typedef int(__stdcall *xinput_get_changes_fn)(void *, uint32_t *, uint32_t *);
typedef int(__stdcall *xinput_open_fn)(void *, int, int, int);
typedef int(__stdcall *xinput_get_state_fn)(int, void *);
typedef void(__stdcall *xinput_close_fn)(int);

/* ---- Key codes for thumbstick axes in input_key_is_down ----
 *   0x69 = left stick X    0x6A = left stick Y
 *   0x6B = right stick X   0x6C = right stick Y                        */

/* ---- Read-only mapping tables ----
 *   0x281150  uint8[8]  analog_button_index_map
 *            maps logical analog channel to XInput bAnalogButtons index
 *   0x281158  uint8[8]  digital_button_bitmask_map
 *            maps logical digital channel to XInput wButtons mask            */

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
  return (input_raw_stick_state *)0x46ba1a;
}

static input_rumble_state *input_rumble_states(void)
{
  return (input_rumble_state *)0x46bb14;
}

static uint8_t *input_digital_button_states(void)
{
  return (uint8_t *)0x46bb38;
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

  if (*(char *)0x46ba38) /* input_suppressed */
    return false;
  switch (key - INPUT_AXIS_KEY_LEFT_X) {
  case 0: /* left stick X */
    a = *(uint8_t *)0x46bb7c;
    b = *(uint8_t *)0x46bb71;
    return a < b ? b : a;
  case 1: /* left stick Y */
    a = *(uint8_t *)0x46bb84;
    b = *(uint8_t *)0x46bb7d;
    return a < b ? b : a;
  case 2: /* right stick X */
    /* The binary uses <= here, unlike the left-stick cases. Preserve that tie
     * behavior. */
    a = *(uint8_t *)0x46bb7e;
    b = *(uint8_t *)0x46bb82;
    return a <= b ? b : a;
  case 3: /* right stick Y */
    a = *(uint8_t *)0x46bb7f;
    b = *(uint8_t *)0x46bb81;
    return a <= b ? b : a;
  default:
    assert_halt(key >= 0 && key < INPUT_KEY_COUNT);
    return input_digital_button_states()[key];
  }
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
    if (*(char *)0x46ba38) /* input_suppressed */
      return (void *)0x46baec; /* suppressed_gamepad_state */
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

void input_update(void)
{
  int i;

  *(char *)0x46ba38 = 0; /* clear input_suppressed */
  if (!*(char *)0x46ba39) { /* if not yet initialized */
    ((void(__stdcall *)(int))0x1cfaec)(*(int *)0x46bb24); /* init callback */
    *(char *)0x46ba39 = 1; /* mark initialized */
  }
  ((void (*)(void))0xcfdb0)(); /* poll/update all gamepads */
  for (i = 0; i < MAXIMUM_GAMEPADS; i++)
    ((void (*)(void *))0xce620)(&input_gamepad_states()[i]);
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
