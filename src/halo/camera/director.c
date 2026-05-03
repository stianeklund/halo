/* Camera director — controls camera mode per local player.
 *
 * Per-player director state lives in a 0xf8-byte array of 4 entries.
 * There are two "views" into the array used by different functions:
 *   - base 0x3352b4: float timer [+0], void* camera_fn [+4], camera data [+8]
 *   - base 0x3352fc: unknown dword [+0], unknown byte [+4] (= 0x3352b4 + 0x48)
 *
 * Globals:
 *   0x3352a8  float  delta_time (written by director_update)
 *   0x3352ac  int16  director_mode (0=normal, 1=?, 2=editor/scripted, 4=replay)
 *   0x3352ae  uint8  first-update flag (cleared after first camera dispatch)
 *
 * Camera mode functions (pointers stored at player_struct+4):
 *   0x853c0  debug/free camera (only active for one gamepad player at a time)
 *   0x89270  first-person gameplay camera
 *   0x87f20  scripted/editor camera
 *   0x89cd0  transition camera used when returning from a third-person style
 *            view back to gameplay; binary comparison suggests this is the
 *            vehicle-exit blend path, not a pure dead camera
 *
 * Register-arg callees are reached through shims in camera_internal.h. */
#include "camera_internal.h"

/* Allocate director scripting state. */
void director_initialize(void)
{
  *(char **)0x5ab200 = (char *)game_state_malloc("director scripting", 0, 4);
  **(char **)0x5ab200 = 0;
}

/* Dispose — nothing to clean up. */
void director_dispose(void)
{
}

/* Set active local-player context used by hs/console during cheat dispatch.
 * Writes 1 to the per-player director state byte at struct offset +0x4e. */
void director_set_local_player_context(int16_t player_index)
{
  assert_halt(player_index >= 0 &&
              player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  ((char *)0x335302)[(int)player_index * 0xf8] = 1;
}

/* Per-player default-state init (0x86600). Fills four 12-byte slots at
 * struct offset 0x194/0x1a0/0x1ac/0x1b8 (relative to 0x3352b4 + player*0xf8).
 * Each slot's first dword is seeded from a const table at 0x2ee604 (0x1c
 * stride); the remaining two dwords are zeroed.
 *
 * Original prototype passes the player index in AX — reached via the
 * reverse-thunk codegen in tools/patch.py when the original binary calls
 * 0x86600. Our C signature is plain cdecl. */
void director_init_player_cameras(int16_t local_player_index)
{
  uint32_t *src;
  char *dst;
  int i;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  src = (uint32_t *)0x2ee604;
  dst = (char *)0x335380 + (int)local_player_index * 0xf8 + 0xd0;

  for (i = 0; i < 4; i++) {
    *(uint32_t *)(dst - 8) = *src;
    *(uint32_t *)dst = 0;
    *(uint32_t *)(dst - 4) = 0;
    src += 7;
    dst += 0xc;
  }
}

/* Reset director state for all 4 players when disposing old map.
 * Per-player data starts at 0x335374 with stride 0xF8 bytes.
 * Offsets verified against disassembly (ESI-relative). */
void director_dispose_from_old_map(void)
{
  int16_t i;
  char *entry = (char *)0x335374;

  for (i = 0; i < 4; i++) {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    *(int *)(entry - 0xbc) = 0;
    *(float *)entry = 1.0f;
    *(uint8_t *)(entry - 4) = 0;
    entry += 0xf8;
  }
  **(char **)0x5ab200 = 0;
}

/* Normal-play camera dispatch for one player (mode-0/1, 0x86de0).
 *
 * Three behaviors selected by reset_flag and the player's death state:
 *
 *   1. reset_flag != 0 (forced reset):
 *      Re-init the camera-data block via 0x88c40, install the first-person
 *      camera fn (0x89270), prime the +0xc0 timer to 1.0f, clear the +0xbc
 *      byte. Returns immediately.
 *
 *   2. reset_flag == 0:
 *      Compute is_dead from player_data: dead iff player_data[+0x34] == -1
 *      AND player_data[+0xaa] (signed short) > 0. Then optionally cycle
 *      camera mode (mode_flags), then if no scripted camera state is active,
 *      re-evaluate the active camera and either:
 *        - is_dead && current != vehicle/death-follow camera: install
 *          third-person follow state via 0x85b60 + set camera 0x85c80 with
 *          timer reset.
 *        - !is_dead && current == 0x85c80: call 0x864b0 to classify whether
 *          gameplay should resume directly in first person (0x89270) or
 *          through the transition camera (0x89cd0). The helper also writes a
 *          small state word stored at base+0x50.
 *
 * Ghidra comparison against the original Xbox binary suggests our earlier
 * comments were too death-camera-centric here: 0x864b0 is vehicle/seat-state
 * sensitive, and 0x89cd0 appears to drive the smooth third-person-to-first-
 * person blend seen when exiting a vehicle.
 *
 * Original receives player index in AX, plus two byte stack args —
 * reverse-thunked to plain cdecl by tools/patch.py. */
void director_set_player_camera_normal(int16_t local_player_index,
                                       char reset_flag, char mode_flags)
{
  char *base;
  uint8_t is_dead;
  void *current_camera;
  int player_handle;
  char *player_data;
  int unit_handle;
  int16_t local_word;
  int16_t result;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  base = (char *)0x3352b4 + (int)local_player_index * 0xf8;

  if (reset_flag != 0) {
    /* 0x88c40: zero camera-data first dword. */
    ((void (*)(void *))0x88c40)((void *)(base + 8));

    assert_halt(local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    *(uint8_t *)(base + 0xbc) = 0;
    *(uint32_t *)(base + 4) = 0x89270;
    *(float *)(base + 0xc0) = 1.0f;
    return;
  }

  /* Compute is_dead from the player's data record. */
  player_handle = local_player_get_player_index(local_player_index);
  player_data = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  if (*(int *)(player_data + 0x34) != -1) {
    is_dead = 0;
  } else if (*(int16_t *)(player_data + 0xaa) > 0) {
    is_dead = 1;
  } else {
    is_dead = 0;
  }

  if (mode_flags != 0)
    CAMERA_INTERNAL_CYCLE_MODE(local_player_index, 0x266f68, 3);

  /* If a scripted camera is active (script_state[0] != 0), nothing else. */
  if (**(char **)0x5ab200 != 0)
    return;

  camera_internal_reevaluate(local_player_index, 0);

  current_camera = *(void **)(base + 4);

  if (is_dead != 0) {
    if (current_camera == (void *)0x85c80)
      return; /* already in the third-person follow camera */

    /* 0x85b60: cdecl(camera_data_ptr, player_index, init_flag=-1). */
    ((void (*)(void *, int16_t, int))0x85b60)((void *)(base + 8),
                                              local_player_index, -1);
    camera_internal_set_camera_fn(local_player_index, (void *)0x85c80, 1);
    return;
  }

  /* is_dead == 0: only switch back to gameplay camera if we're still in the
   * third-person follow camera. */
  if (current_camera != (void *)0x85c80)
    return;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  /* 0xb6870: lookup player's unit handle. */
  unit_handle = ((int (*)(int16_t))0xb6870)(local_player_index);

  /* 0x864b0: classify how gameplay should resume from 0x85c80. Binary
   * evidence says this is not just a dead-camera test: it is sensitive to
   * unit/seat state and selects whether we go through 0x89cd0 (transition
   * camera) or straight to 0x89270. It also writes a small state word that we
   * preserve at base+0x50 for the camera pipeline. */
  result = ((int16_t(*)(int, int16_t *))0x864b0)(unit_handle, &local_word);

  if (result == 1) {
    ((void (*)(void *))0x89850)((void *)(base + 8));
    camera_internal_set_camera_fn(local_player_index, (void *)0x89cd0, 0);
  } else {
    ((void (*)(void *))0x88c40)((void *)(base + 8));
    camera_internal_set_camera_fn(local_player_index, (void *)0x89270, 0);
  }

  *(int16_t *)(base + 0x50) = local_word;
}

/* Switch player to scripted/editor camera (mode 2 dispatch, 0x86fa0).
 * If reset_flag is set, or the current camera fn is not already the scripted
 * one (0x87f20), call the scripted-camera initializer at 0x87800 with the
 * per-player camera-data pointer, then install 0x87f20 as the active camera
 * fn, prime the timer at +0xc0 to 1.0f, and clear the byte flag at +0xbc.
 *
 * Original receives player index in SI and reset_flag as a byte stack arg —
 * reverse-thunked to plain cdecl by tools/patch.py. */
void director_set_player_camera_scripted(int16_t local_player_index,
                                         char reset_flag)
{
  char *base;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  base = (char *)0x3352b4 + (int)local_player_index * 0xf8;

  if (reset_flag != 0 || *(uint32_t *)(base + 4) != 0x87f20) {
    /* 0x87800: scripted-camera init; cdecl(camera_data_ptr, player_index). */
    ((void (*)(void *, int16_t))0x87800)((void *)(base + 8),
                                         local_player_index);

    assert_halt(local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    *(uint32_t *)(base + 4) = 0x87f20;
    *(float *)(base + 0xc0) = 1.0f;
    *(uint8_t *)(base + 0xbc) = 0;
  }
}

/* Replay-mode dispatch for one player (0x87050).
 * Two cases:
 *   - reset_flag != 0: forcibly install the first-person camera fn at base+4
 *     (using the camera-data init helper at 0x88c40), prime the timer, clear
 *     the reset byte at +0xbc.
 *   - reset_flag == 0 && mode_flags != 0: cycle through replay camera modes
 *     by calling 0x86a50, which expects @eax=player_index, @ebx=mode-table
 *     pointer, and a stack arg = entry count. The mode table at 0x266f70 has
 *     4 short entries.
 *
 * Original receives reset_flag in AL and player index in SI; reverse-thunked
 * to plain cdecl by tools/patch.py. */
void director_apply_replay_mode_for_player(char reset_flag,
                                           int16_t local_player_index,
                                           char mode_flags)
{
  char *base;

  if (reset_flag != 0) {
    assert_halt(local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    base = (char *)0x3352b4 + (int)local_player_index * 0xf8;

    /* 0x88c40: camera-data init helper (cdecl: camera_data_ptr). */
    ((void (*)(void *))0x88c40)((void *)(base + 8));

    assert_halt(local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    *(uint32_t *)(base + 4) = 0x89270;
    *(float *)(base + 0xc0) = 1.0f;
    *(uint8_t *)(base + 0xbc) = 0;
    return;
  }

  if (mode_flags != 0)
    CAMERA_INTERNAL_CYCLE_MODE(local_player_index, 0x266f70, 4);
}

/* Build per-player camera input snapshot for one tick (0x87110).
 *
 * Output buffer layout (32-bit fields unless noted):
 *   +0x00 int16  player_index
 *   +0x02 uint8  valid
 *   +0x04 float  delta_time
 *   +0x08 float  forward
 *   +0x0c float  side
 *   +0x10 float  pitch_accumulator
 *   +0x14 float  yaw_accumulator
 *   +0x18 float  roll_accumulator
 *   +0x1c float  zoom_accumulator
 *   +0x20 float  trigger
 *
 * Branches on whether the player has a controllable unit:
 *   - Has unit: derive look/strafe from unit's control state, scaled by
 *     per-player sensitivity (base+0xc0). Returns "valid" derived from the
 *     unit's cinematic byte (+0x14).
 *   - No unit: fall back to gamepad direct-input. The "no unit" branch
 *     calls 0xcf690 which is stubbed to 0 on Xbox, so the fallback body is
 *     dead code on retail — kept faithful to the binary anyway.
 *
 * Original receives the output-buffer pointer in EAX; reverse-thunked to
 * plain cdecl by tools/patch.py. */
bool director_compute_camera_input(short *out_buf, int local_player_index)
{
  int16_t player16 = (int16_t)local_player_index;
  int player_handle;
  char *player_data;
  int unit_handle;
  void *current_camera;
  char *base;

  assert_halt(player16 >= 0 && player16 < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  csmemset(out_buf, 0, 0x24);
  *out_buf = player16;
  *(float *)(out_buf + 2) = *(float *)0x3352a8;

  base = (char *)0x3352b4 + (int)player16 * 0xf8;

  player_handle = local_player_get_player_index(player16);
  player_data = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  unit_handle = (int)*(int16_t *)(player_data + 2);

  if (unit_handle == -1 || input_has_gamepad((int16_t)unit_handle) == 0) {
    /* No unit / unit not playable: gamepad-direct fallback. On Xbox 0xcf690
     * is stubbed to 0, so the inner body never runs. */
    int *gamepad_state = (int *)((void *(*)(void))0xcf690)();
    if (gamepad_state != NULL) {
      bool back_pressed;

      gamepad_state = (int *)((void *(*)(void))0xcf690)();
      back_pressed = input_key_is_down(0x1d);

      if ((current_camera = *(void **)(base + 4),
           current_camera != (void *)0x89270 &&
             *((char *)gamepad_state + 0xd) != 0) ||
          input_key_is_down(0x1e) != 0) {
        uint8_t mode_flags = (input_key_is_down(0x20) != 0) ? 1 : 0;
        if (input_key_is_down(0x2e))
          mode_flags |= 2;
        if (input_key_is_down(0x2d))
          mode_flags |= 4;
        if (input_key_is_down(0x2f))
          mode_flags |= 8;
        if (input_key_is_down(0x22))
          mode_flags |= 0x10;
        if (input_key_is_down(0x30))
          mode_flags |= 0x20;
        if (input_key_is_down(0x23))
          mode_flags |= 0x40;
        if (input_key_is_down(0x31))
          mode_flags |= 0x80;

        camera_internal_integrate(player16, mode_flags,
                                  (float)gamepad_state[2]);

        *(float *)(out_buf + 4) = (float)gamepad_state[0] * *(float *)0x2670a0;
        *(float *)(out_buf + 6) = (float)gamepad_state[1] * *(float *)0x2670a0;
        *(float *)(out_buf + 8) += *(float *)(base + 0xd8);
        *(float *)(out_buf + 0x10) = (float)gamepad_state[2];
        *(float *)(out_buf + 10) += *(float *)(base + 0xe4);
        *(float *)(out_buf + 0xc) += *(float *)(base + 0xf0);
        *(float *)(out_buf + 0xe) += *(float *)(base + 0xcc);
        *((uint8_t *)out_buf + 2) = 1;

        ((void (*)(int))0x86220)(local_player_index);
        ((void (*)(int))0x861d0)(local_player_index);
      }
      return back_pressed == 1;
    }
    return false;
  }

  /* Has unit: pull look/strafe from the unit's control struct. */
  {
    char *unit_data = (char *)input_get_gamepad_state(unit_handle);
    bool valid_unit;

    if (*(char *)0x335690 != 0) {
      valid_unit = (*(char *)(unit_data + 0x14) == 1);
    } else {
      uint8_t cb = *(uint8_t *)(unit_data + 0x14);
      if (cb == 0) {
        valid_unit = false;
      } else if ((unsigned)cb % 0x1e != 0) {
        valid_unit = false;
      } else {
        valid_unit = true;
      }
    }

    current_camera = *(void **)(base + 4);
    if (current_camera == (void *)0x89270 || current_camera == (void *)0x89cd0)
      return valid_unit;

    if (*(char *)(unit_data + 0x1f) == 1)
      *(char *)(base + 0xbc) = (*(char *)(base + 0xbc) == 0);

    if (*(char *)(base + 0xbc) == 0)
      return valid_unit;

    {
      uint8_t mode_flags = (*(char *)(unit_data + 0x17) != 0) ? 0x10 : 0;
      float trigger;

      if (*(char *)(unit_data + 0x16) != 0)
        mode_flags |= 0x20;

      trigger = (float)((int)((unsigned)(1 < *(uint8_t *)(unit_data + 0x18)) -
                              (unsigned)(1 < *(uint8_t *)(unit_data + 0x19)))) *
                *(float *)0x253524;

      *(float *)(out_buf + 0x10) = trigger;
      camera_internal_integrate(player16, mode_flags, trigger);

      *(float *)(out_buf + 4) = (float)(int)*(int16_t *)(unit_data + 0x24) *
                                *(float *)0x3352a8 * *(float *)0x2670b0;
      *(float *)(out_buf + 6) = (float)(int)*(int16_t *)(unit_data + 0x26) *
                                *(float *)0x3352a8 * *(float *)0x2670ac;
      *(float *)(out_buf + 10) = (float)(int)*(int16_t *)(unit_data + 0x22) *
                                 *(float *)(base + 0xc0) * *(float *)0x3352a8 *
                                 *(float *)0x2670a8;
      *(float *)(out_buf + 0xc) = (float)(int)*(int16_t *)(unit_data + 0x20) *
                                  *(float *)(base + 0xc0) * *(float *)0x3352a8 *
                                  *(float *)0x2670a4;

      *(float *)(out_buf + 0xe) += *(float *)(base + 0xcc);
      *((uint8_t *)out_buf + 2) = 1;

      ((void (*)(int))0x86220)(local_player_index);
      ((void (*)(int))0x861d0)(local_player_index);
    }

    return valid_unit;
  }
}

/* Set director mode and reset per-player camera state for a new map.
 * Director mode is 2 (scripted) in editor, 0 (normal) otherwise.
 * For each player: zeros timer, two unknown fields, then dispatches to the
 * mode-specific camera initializer and runs the per-player data init. */
void director_initialize_for_new_map(void)
{
  int16_t i;
  char *p;

  /* director mode: 0 = normal gameplay, 2 = editor (scripted camera) */
  *(int16_t *)0x3352ac = game_in_editor() ? 2 : 0;
  *(uint8_t *)0x3352ae = 0;

  /* p points into the per-player array at offset 0x48 from the struct base */
  p = (char *)0x3352fc;
  for (i = 0; i < 4; i++) {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    /* zero offset-0x48 field (dword), offset-0x4c field (byte), and the
     * timer at offset-0x00 (= p - 0x48) */
    *(uint32_t *)p = 0;
    *(uint8_t *)(p + 4) = 0;
    *(uint32_t *)(p - 0x48) = 0; /* timer field at struct base */

    switch (*(int16_t *)0x3352ac) {
    case 0:
    case 1:
      camera_internal_set_mode_0_1(i, 1, 0);
      break;
    case 2:
      camera_internal_set_mode_2(i, 1, 0);
      break;
    case 4:
      camera_internal_set_mode_4(1, i, 0);
      break;
    }

    camera_internal_init_player(i);

    p += 0xf8;
  }
}

/* Update all active local players' cameras for this tick.
 * Stores delta_time, polls input via 0x87110, dispatches per-mode camera
 * update, runs the active camera function, then commits the result to the
 * observer.
 *
 * The active camera function is where the transition-camera path starts, but
 * not where it finishes: the camera output block copied to ps+0x54 is later
 * consumed by observer-side code which seeds and integrates several per-axis
 * blend timers. That observer stage is what makes the vehicle-exit return to
 * first person feel smooth in the original binary. */
void director_update(float delta_time)
{
  int i;
  uint8_t mode_flags;
  uint8_t local_98[36]; /* camera input buffer (written by 0x87110) */
  uint8_t local_74[0x68]; /* camera output buffer (written by camera_fn) */
  char *ps; /* per-player struct base (0xf8-byte stride) */

  *(float *)0x3352a8 = delta_time;
  i = 0;
  ps = (char *)0x3352b4;

  do {
    if (local_player_get_player_index((int16_t)i) != -1) {
      assert_halt((int16_t)i >= 0 &&
                  (int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

      ps[0x4d] = 0;
      ps[0x4e] = 0;

      mode_flags = camera_internal_poll_input(local_98, i);

      {
        uint8_t rst = *(uint8_t *)0x3352ae;
        switch (*(int16_t *)0x3352ac) {
        case 0:
        case 1:
          camera_internal_set_mode_0_1((int16_t)i, rst, mode_flags);
          break;
        case 2:
          camera_internal_set_mode_2((int16_t)i, rst, mode_flags);
          break;
        case 4:
          camera_internal_set_mode_4(rst, (int16_t)i, mode_flags);
          break;
        }
      }

      /* clear first-update flag after camera mode dispatch */
      *(uint8_t *)0x3352ae = 0;

      csmemset(local_74, 0, 0x68);

      /* call the per-player camera update function if set */
      {
        void (*camera_fn)(void *, void *, void *) =
          *(void (**)(void *, void *, void *))(ps + 4);
        if (camera_fn != NULL) {
          /* the debug camera (0x853c0) only runs for the active gamepad player
           */
          if ((void *)camera_fn != (void *)0x853c0 ||
              (int16_t)i == local_player_get_next(-1)) {
            camera_fn(ps + 8, local_98, local_74);
          }
        }
      }

      if (!(local_74[0] & 1)) {
        /* camera produced no valid output — clear the valid bit */
        *(uint32_t *)(ps + 0x54) &= ~1U;
      } else {
        float *timer = (float *)ps; /* timer at struct offset 0 */
        if (*timer != 0.0f) {
          if (*(float *)0x2549d4 <= *timer ||
              *(void **)(ps + 4) != (void *)0x89270) {
            /* track max timer in local_74[0x48] — this lives inside the
             * camera output buffer so it's passed to the observer via
             * the qmemcpy into ps+0x54. The observer uses it to drive
             * the smooth camera blend. */
            if (*(float *)(local_74 + 0x48) <= *timer)
              *(float *)(local_74 + 0x48) = *timer;
          } else {
            /* reset camera when timer underflows threshold with fp camera.
             * The writes below seed transition flags and timers in the camera
             * output buffer (local_74) at offsets 0x4c/0x4e (flag bytes = 3)
             * and 0x54/0x5c (timer floats = 0). The observer consumes these
             * to drive the smooth third-person-to-first-person blend. */
            *timer = 0.0f;
            *(uint32_t *)(local_74 + 0x54) = 0;
            *(uint8_t *)(local_74 + 0x4c) = 3;
            *(uint32_t *)(local_74 + 0x5c) = 0;
            *(uint8_t *)(local_74 + 0x4e) = 3;
          }
          /* count down timer, clamped to zero */
          {
            float next = *timer - delta_time;
            *timer = next < 0.0f ? 0.0f : next;
          }
        }
        /* copy 0x68 bytes of camera output into the per-player camera slot */
        qmemcpy(ps + 0x54, local_74, 0x68);
      }

      /* commit camera data to the observer subsystem */
      ((void (*)(int16_t, void *))0x8acb0)((int16_t)i, ps + 0x54);
    }

    i++;
    ps += 0xf8;
  } while ((int16_t)i < 4);
}
