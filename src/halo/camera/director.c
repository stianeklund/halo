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
 *   0x853c0  debug camera (only active for one gamepad player at a time)
 *   0x89270  first-person camera
 *   0x87f20  scripted/editor camera
 *   0x89cd0  dead camera (?)
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
 *        - is_dead && current != debug: install dead/replay state via
 *          0x85b60 + set debug camera (0x85c80) with timer reset.
 *        - !is_dead && current == debug: pick first-person (0x89270) or
 *          dead camera (0x89cd0) based on 0x864b0's verdict and install via
 *          0x865a0 with no top-timer reset, then store the verdict word at
 *          base+0x50.
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
      return; /* already debug — done */

    /* 0x85b60: cdecl(camera_data_ptr, player_index, init_flag=-1). */
    ((void (*)(void *, int16_t, int))0x85b60)((void *)(base + 8),
                                              local_player_index, -1);
    camera_internal_set_camera_fn(local_player_index, (void *)0x85c80, 1);
    return;
  }

  /* is_dead == 0: only switch back to gameplay camera if currently debug. */
  if (current_camera != (void *)0x85c80)
    return;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  /* 0xb6870: lookup player's unit handle. */
  unit_handle = ((int (*)(int16_t))0xb6870)(local_player_index);

  /* 0x864b0: classify camera transition; returns 1 for dead-cam, 0/2/3
   * otherwise; writes a result word to local_word. */
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
 * update, runs the camera function, then commits the result to the observer. */
void director_update(float delta_time)
{
  int i;
  uint8_t mode_flags;
  uint8_t local_98[36]; /* camera input buffer (written by 0x87110) */
  uint8_t local_74[0x68]; /* camera output buffer (written by camera_fn) */
  float local_2c; /* max-timer accumulator (uninit in original) */
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
            /* track max timer seen (local_2c accumulates the peak) */
            if (local_2c <= *timer)
              local_2c = *timer;
          } else {
            /* reset camera when timer underflows threshold with fp camera */
            /* original also wrote dead local fields here (local_20/28/18/26) */
            *timer = 0.0f;
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
