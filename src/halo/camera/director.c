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

/*
 * FUN_00085a40 (0x85a40) — is there another player whose player-record dword
 * at +0x20 equals this player's?
 *
 * player_handle arrives in EDI (the decompiler reports it as `unaff_EDI`:
 * EDI is read at 0x85a4d/0x85a80 and never written in this function).
 * Returns in AL: 1 as soon as an iterated player other than player_handle has
 * the same dword at player+0x20; otherwise the BL zeroed at 0x85a62 (0).
 *
 * Call sites traced from the disassembly:
 *   PUSH EDI / PUSH EAX(=[0x5aa6d4]) / CALL 0x119320
 *     -> datum_get(player_data, player_handle)
 *   PUSH ECX(=[0x5aa6d4]) / PUSH EDX(=EBP-0x10) / CALL 0x1197b0
 *     -> data_iterator_new(&iter, player_data)
 *   PUSH EBP-0x10 / CALL 0x119810 -> data_iterator_next(&iter)
 * The single ADD ESP,0x14 at 0x85a72 cleans all three cdecl arg groups
 * (2 + 2 + 1 dwords) at once; the earlier PUSH EBX/PUSH ESI are callee saves
 * matched by the POP ESI/POP EBX at both exits.
 *
 * The iterator local is at EBP-0x10, so CMP [EBP-0x8],EDI compares
 * iter.datum_handle (data_iter_t +0x8). Both [0x5aa6d4] loads are kept
 * separate, matching the two MOVs at 0x85a46 and 0x85a54.
 *
 * The meaning of the player field at +0x20 is unproven here; it is only ever
 * compared for equality between two player records.
 *
 * 0x85a40 / director.obj
 */
bool FUN_00085a40(int player_handle)
{
  data_iter_t iter;
  char *player;
  char *other;
  int value;
  bool result;

  player = (char *)datum_get(player_data, player_handle);
  value = *(int *)(player + 0x20);
  result = 0;

  data_iterator_new(&iter, player_data);
  other = (char *)data_iterator_next(&iter);
  while (other != NULL) {
    if (iter.datum_handle != (uint32_t)player_handle &&
        *(int *)(other + 0x20) == value) {
      return 1;
    }
    other = (char *)data_iterator_next(&iter);
  }

  return result;
}

/*
 * FUN_00085ab0 (0x85ab0) — pick the next eligible player handle after
 * `current_handle`, falling back to `current_handle` when none is found.
 *
 * Arguments (0x85ab0..0x85b5d):
 *   BL          restrict_flag; tested at 0x85ab6 and again at 0x85b0e, never
 *               written in this function (decompiler reports `unaff_BL`).
 *   [EBP+0x8]   player_handle  — the player to exclude (CMP at 0x85b03) and,
 *               when restrict_flag is set, the source of the +0x20 dword.
 *   [EBP+0xc]   current_handle — compared against each candidate's low word
 *               (0x85b20..0x85b32) and returned unchanged at 0x85b4d.
 *   Result in EAX (0x85b57 MOV EAX,ESI / 0x85b4d MOV EAX,[EBP+0xc]).
 *
 * Call sites traced from the disassembly:
 *   0x85ac7  PUSH [EBP+8] / PUSH [0x5aa6d4] -> datum_get(player_data, handle)
 *   0x85ae5  PUSH [0x5aa6d4] / PUSH EBP-0x10 -> data_iterator_new(&iter, ...)
 *   0x85aee, 0x85b38  PUSH EBP-0x10 -> data_iterator_next(&iter)
 * The ADD ESP,0xc at 0x85af3 cleans the iterator_new + first iterator_next arg
 * groups together; the datum_get group is cleaned separately at 0x85acf.
 *
 * The iterator local sits at EBP-0x10, so MOV ECX,[EBP-0x8] at 0x85b00 reads
 * iter.datum_handle (data_iter_t +0x8).
 *
 * Candidate filter (0x85b00..0x85b15): handle differs from player_handle, the
 * player dword at +0x34 is not -1, and — only when restrict_flag is set — the
 * dword at +0x20 matches the caller player's. The meaning of both fields is
 * unproven; +0x20 is only ever compared for equality (same as FUN_00085a40)
 * and +0x34 only against -1.
 *
 * Selection (0x85b17..0x85b46): the first candidate is remembered in ESI; a
 * later candidate whose low 16 bits exceed those of current_handle replaces it
 * and ends the scan (JG at 0x85b32 -> 0x85b46, which falls into the exit
 * check). Both operands are masked to 16 bits before the signed compare, so
 * signedness cannot matter.
 *
 * 0x85ab0 / director.obj
 */
uint32_t FUN_00085ab0(char restrict_flag, uint32_t player_handle,
                      uint32_t current_handle)
{
  data_iter_t iter;
  char *player;
  int value;
  uint32_t result;

  if (restrict_flag == 0) {
    value = -1;
  } else {
    player = (char *)datum_get(player_data, (int)player_handle);
    value = *(int *)(player + 0x20);
  }

  result = 0xffffffff;
  data_iterator_new(&iter, player_data);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    if (iter.datum_handle != player_handle && *(int *)(player + 0x34) != -1 &&
        (restrict_flag == 0 || *(int *)(player + 0x20) == value)) {
      if (result == 0xffffffff) {
        result = iter.datum_handle;
      } else if ((int)(iter.datum_handle & 0xffff) >
                 (int)(current_handle & 0xffff)) {
        result = iter.datum_handle;
        break;
      }
    }
    player = (char *)data_iterator_next(&iter);
  }

  if (result == 0xffffffff) {
    return current_handle;
  }
  return result;
}

/*
 * dead_camera_new (0x85b60) — initialize the dead/third-person follow camera
 * state block for a local player.
 *
 * cdecl(camera, local_player_index, handle). The assert at 0x85b7b passes
 * "camera" as the reason string with file c:\halo\SOURCE\camera\dead_camera.c
 * line 0x17 (23), which names the first parameter.
 *
 * Layout written into the block (field meanings beyond the first vector are
 * unproven; the offsets below are exactly the stores at 0x85b98..0x85c6e):
 *   +0x00..0x0b  three dwords copied verbatim from observer_get_camera()
 *                (MOV dword, not FLD/FSTP) — the observer camera position
 *   +0x0c        random_real_range(seed, 0.0f, 2*pi)
 *   +0x10        -random_real_range(seed, 0.15*pi, 0.35*pi)   (FCHS at 0x85c02)
 *   +0x14        random_real_range(seed, 2.0f, 6.0f)
 *   +0x18        1.2217305f (0x3f9c61aa, 70 degrees in radians)
 *   +0x1c        dword copied from [0x266f38]
 *   +0x20        local_player_get_player_index(local_player_index)
 *   +0x24        copy of the dword just written at +0x20 (reloaded from the
 *                block at 0x85c54/0x85c65, not held in a register)
 *   +0x28        `handle` when it is not -1, otherwise player_data[+0x38]
 *   +0x2c        float selected at 0x85c0a..0x85c32
 *
 * Store order matters: +0x18 is written before the first random draw, and the
 * three random_real_range() calls happen in the order 0x14, 0x0c, 0x10 — each
 * one re-reads the local seed address (three separate CALL 0x10b120).
 *
 * The +0x2c selector reads three distinct float globals and only calls
 * game_engine_running() on the handle == -1 path (JZ at 0x85c10).
 *
 * 0x85b60 / director.obj
 */
void dead_camera_new(void *camera, int16_t local_player_index, int handle)
{
  char *dst;
  uint32_t *src;
  float selected;
  float drawn;
  uint32_t copied;
  int player_handle;
  char *player;

  src = (uint32_t *)observer_get_camera((unsigned short)local_player_index);

  assert_halt_at("c:\\halo\\SOURCE\\camera\\dead_camera.c", 23, camera);

  dst = (char *)camera;
  *(uint32_t *)(dst + 0x00) = src[0];
  *(uint32_t *)(dst + 0x04) = src[1];
  *(uint32_t *)(dst + 0x08) = src[2];
  *(float *)(dst + 0x18) = 1.2217305f;
  *(float *)(dst + 0x14) =
    random_real_range((int *)random_math_get_local_seed_address(), 2.0f, 6.0f);
  *(float *)(dst + 0x0c) = random_real_range(
    (int *)random_math_get_local_seed_address(), 0.0f, 6.2831855f);
  drawn = random_real_range((int *)random_math_get_local_seed_address(),
                            0.47123894f, 1.0995574f);
  /* [0x266f38] is loaded at 0x85bfc, ahead of the FCHS/FSTP pair at
   * 0x85c02/0x85c07 — keep the read in front of the negated store. */
  copied = *(uint32_t *)0x266f38;
  *(float *)(dst + 0x10) = -drawn;
  *(uint32_t *)(dst + 0x1c) = copied;

  if (handle != -1) {
    selected = *(float *)0x2548fc;
  } else if (game_engine_running()) {
    selected = *(float *)0x266f3c;
  } else {
    selected = *(float *)0x266f40;
  }
  *(float *)(dst + 0x2c) = selected;

  player_handle = local_player_get_player_index(local_player_index);
  *(int *)(dst + 0x20) = player_handle;

  /* JNE at 0x85c43 makes the handle == -1 case the fall-through; both tails
   * duplicate the +0x28/+0x24 stores and reload +0x20 from the block. */
  if (handle == -1) {
    player = (char *)datum_get(player_data, player_handle);
    *(int *)(dst + 0x28) = *(int *)(player + 0x38);
    *(int *)(dst + 0x24) = *(int *)(dst + 0x20);
    return;
  }

  *(int *)(dst + 0x28) = handle;
  *(int *)(dst + 0x24) = *(int *)(dst + 0x20);
}

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

/* Set the per-player director "inhibit facing" flag (0x861d0).
 * Writes 1 to the per-player director state byte at struct offset +0x4d
 * (base 0x3352b4 + local_player_index * 0xf8). */
void director_inhibit_facing(int16_t local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  ((char *)0x335301)[(int)local_player_index * 0xf8] = 1;
}

/* Set active local-player context used by hs/console during cheat dispatch.
 * Writes 1 to the per-player director state byte at struct offset +0x4e. */
void director_set_local_player_context(int16_t player_index)
{
  assert_halt(player_index >= 0 &&
              player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  ((char *)0x335302)[(int)player_index * 0xf8] = 1;
}

/* director_inhibited_facing (0x86270) — read the per-player "facing inhibited"
 * flag byte. Base 0x335301 = per-player struct base 0x3352b4 + 0x4d, stride
 * 0xf8 (same array director_set_local_player_context writes at +0x4e).
 * Reference returns the raw byte in AL (no test), so the load is typed bool. */
bool director_inhibited_facing(short local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return ((bool *)0x335301)[(int)local_player_index * 0xf8];
}

/* director_inhibited_input (0x862c0) — read the per-player "input inhibited"
 * flag byte. Base 0x335302 = per-player struct base 0x3352b4 + 0x4e, stride
 * 0xf8 — the same byte director_set_local_player_context writes 1 to.
 * Reference returns the raw byte in AL (no test), so the load is typed bool. */
bool director_inhibited_input(short local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return ((bool *)0x335302)[(int)local_player_index * 0xf8];
}

/* Number of director game modes — the assert bound is CMP SI,0x5 at 0x8631d. */
#define NUMBER_OF_DIRECTOR_GAME_MODES 5

/* director_set_mode (0x86310) — set the global director mode word at
 * 0x3352ac. Only writes when the mode actually changes, and then raises the
 * flag byte at 0x3352ae so the next update re-dispatches the camera. */
void director_set_mode(int16_t mode)
{
  assert_halt(mode >= 0 && mode < NUMBER_OF_DIRECTOR_GAME_MODES);
  if (*(int16_t *)0x3352ac != mode) {
    *(int16_t *)0x3352ac = mode;
    *(uint8_t *)0x3352ae = 1;
  }
}

/* director_save_camera (0x86360) — debug helper that dumps the local player 0
 * observer camera to "d:\camera.txt". Field offsets are read straight off the
 * camera block returned by observer_get_camera:
 *   +0x00..0x08  position xyz
 *   +0x20..0x28  forward xyz
 *   +0x2c..0x34  up xyz
 *   +0x38        field of view
 * Each float is widened to double for the varargs fprintf (FLD dword /
 * FSTP qword in the reference). Nothing is written when the fopen fails. */
void director_save_camera(void)
{
  void *file;
  float *camera;

  file = crt_fopen("d:\\camera.txt", "w");
  if (file) {
    camera = (float *)observer_get_camera(0);
    crt_fprintf(file, "%f %f %f\n", (double)camera[0], (double)camera[1],
                (double)camera[2]);
    crt_fprintf(file, "%f %f %f\n", (double)camera[8], (double)camera[9],
                (double)camera[10]);
    crt_fprintf(file, "%f %f %f\n", (double)camera[11], (double)camera[12],
                (double)camera[13]);
    crt_fprintf(file, "%f\n", (double)camera[14]);
    crt_fclose(file);
  }
}

/*
 * director_get_perspective (0x86410) — return the cached camera
 * "perspective" class for a local player, refreshing the cache from the
 * currently active camera-mode function pointer at base+0x8.
 *   base+0x4  float  camera timer
 *   base+0x8  void*  active camera-mode function pointer
 *   base+0x56 int16  cached perspective: 0 = first-person (0x89270),
 *             1 = following/transition (0x89cd0), 2 = free/debug camera
 *             (0x853c0), 3 = other
 * Base = 0x3352b0 + local_player_index * 0xf8.
 *
 * Mirrors the disassembly's three separate return points exactly: the
 * first-person branch only refreshes and returns the cache when the timer
 * equals 0.0f (constant at 0x2533c0, confirmed 0.0f elsewhere in-tree),
 * otherwise it falls through and returns the stale cached value unchanged.
 */
int16_t director_get_perspective(int16_t local_player_index)
{
  char *base;
  void *camera_fn;

  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\camera\\director.c", 0xb3, 1);
    system_exit(-1);
  }

  base = (char *)0x3352b0 + (int)local_player_index * 0xf8;
  camera_fn = *(void **)(base + 0x8);

  if (camera_fn == (void *)0x89270) {
    if (*(float *)(base + 0x4) == 0.0f) {
      *(int16_t *)(base + 0x56) = 0;
      return *(int16_t *)(base + 0x56);
    }
  } else if (camera_fn == (void *)0x89cd0) {
    *(int16_t *)(base + 0x56) = 1;
    return *(int16_t *)(base + 0x56);
  } else {
    *(int16_t *)(base + 0x56) = (int16_t)((camera_fn != (void *)0x853c0) + 2);
  }

  return *(int16_t *)(base + 0x56);
}

/*
 * director_desired_perspective — classify the perspective a unit wants.
 *
 * Writes a perspective word through *perspective (0 = none, 1, 2, 3) and
 * returns a 16-bit flag in AX. The seat flags come from the parent unit's
 * unit tag seat block (group tag 'unit', block at +0x2e4, element size 0x11c).
 *
 * Binary notes (0x864b0):
 *  - *perspective is zeroed first, so the parent_handle == -1 arm re-reads the
 *    value it just stored; the (==1 || ==3) test is preserved verbatim.
 *  - TEST DL,0x3 after SHL EDX,CL is reproduced as (1 << (n & 0x1f)) & 3.
 *  - EBX (the returned flag) is set by seat flag bit 4 BEFORE the bit-6 test.
 *
 * 0x864b0 / director.obj
 */
int16_t director_desired_perspective(int unit_handle, int16_t *perspective)
{
  char *unit;
  int *unit_definition;
  int parent_handle;
  int seat_index;
  unsigned int *seat;
  unsigned int seat_flags;
  unsigned char wants_perspective;
  char animation_state;
  int16_t result;

  result = 0;
  *perspective = 0;

  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    parent_handle = *(int *)(unit + 0xcc);

    if (parent_handle != -1) {
      unit_definition = (int *)object_get_and_verify_type(parent_handle, -1);

      if (((1 << *(unsigned char *)((char *)unit_definition + 0x64)) & 3) !=
          0) {
        seat_index = (int)*(short *)(unit + 0x2a0);
        seat = (unsigned int *)tag_block_get_element(
          (void *)((char *)tag_get(0x756e6974, *unit_definition) + 0x2e4),
          seat_index, 0x11c);

        seat_flags = *seat;
        wants_perspective = (unsigned char)((seat_flags >> 6) & 1);

        if ((seat_flags & 0x10) != 0)
          result = 1;

        if (wants_perspective != 0) {
          animation_state = *(char *)(unit + 0x253);
          if (animation_state == 0x1a) {
            *perspective = 1;
            return 1;
          }
          if (animation_state == 0x1b) {
            *perspective = 3;
            return 1;
          }
        }
      }

      *perspective = 2;
      return result;
    }

    goto no_parent;
  }

  goto ret_zero;

  /* Cold tail: the reference lays this arm out after the epilogue (0x8657b),
   * reached only by the forward JZ at 0x864e0. */
no_parent:
  if (*perspective == 1 || *perspective == 3)
    return 1;

ret_zero:
  return 0;
}

/*
 * FUN_000865a0 — set player director mode entry fields.
 * Writes param_1 to [base+0x8], 1.0f to [base+0xc4], clears [base+0xc0],
 * and if param_2 is true writes 1.0f to [base+0x4].
 * Base = 0x3352b0 + local_player_index * 0xf8.
 * local_player_index passed in SI.
 *
 * 0x865a0 / director.obj
 */
void FUN_000865a0(int16_t local_player_index, int param_1, bool param_2)
{
  char *base;

  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\camera\\director.c", 0xb3, 1);
    system_exit(-1);
  }
  base = (char *)0x3352b0 + (int)local_player_index * 0xf8;
  *(int *)(base + 0x8) = param_1;
  *(int *)(base + 0xc4) = 0x3f800000;
  *(unsigned char *)(base + 0xc0) = 0;
  if (param_2) {
    *(int *)(base + 0x4) = 0x3f800000;
  }
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
  dst = (char *)0x3352b0 + (int)local_player_index * 0xf8 + 0xd0;

  i = 4;
  do {
    *(uint32_t *)(dst - 8) = *src;
    *(uint32_t *)dst = 0;
    *(uint32_t *)(dst - 4) = 0;
    src = (uint32_t *)((char *)src + 0x1c);
    dst += 0xc;
    i--;
  } while (i != 0);
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

/* director_camera_deterministic (0x86b80) — deterministic camera dispatch.
 *
 * Classifies the unit's desired perspective via director_desired_perspective
 * (0x864b0), then forwards the same three stack arguments to one of two
 * camera routines: 0x88c80 (first-person family, director.obj, adjacent to
 * first_person_camera_new) when the classification word is zero, otherwise
 * 0x89c00 (following-camera family, adjacent to following_camera_update).
 *
 * The perspective word written back through the local is scratch here — the
 * original only keeps the 16-bit return value in SI and returns it (MOV AX,SI
 * on both exit paths). param_2/param_3 are opaque dword pass-throughs: the
 * original copies them straight from the stack frame to the callee's argument
 * slots with no float handling, so their types are unproven. */
int16_t director_camera_deterministic(int unit_handle, int param_2, int param_3)
{
  int16_t perspective;
  int16_t result;

  result = director_desired_perspective(unit_handle, &perspective);

  if (result == 0) {
    /* 0x88c80 proves both forwarded arguments are float[3] out-pointers (it
     * hands param_2 to unit_set_seat_state's `float *position`); the casts
     * are type-only and do not change the pushed dwords. */
    FUN_00088c80(unit_handle, (float *)param_2, (float *)param_3);
  } else {
    FUN_00089c00(unit_handle, param_2, param_3);
  }

  return result;
}

/* director_script_camera (0x86cb0) — enable or disable scripted camera control
 * for every local player.
 *
 * Stores the low byte of the argument into the director scripting state byte
 * (*(char**)0x5ab200), then walks the four per-player entries (base 0x3352b4,
 * stride 0xf8; the reference walks them through EDI seeded at 0x335374 =
 * base + 0xc0):
 *
 *   script_control != 0: install the debug/free camera fn (0x853c0) at +0x4,
 *     prime the timer at +0xc0 to 1.0f, clear the byte flag at +0xbc.
 *   script_control == 0: restore gameplay — classify the player's unit via
 *     director_desired_perspective (0x864b0) and either re-init the following
 *     camera data (0x89850) + install 0x89cd0, or the first-person camera data
 *     (0x88c40) + install 0x89270. The classification word is kept at +0x50.
 *
 * Either way FUN_00084fe0 (0x84fe0, bored-camera enable flag) is called once
 * per iteration with the same byte.
 *
 * The reference keeps the perspective out-slot in the upper half of the
 * incoming argument slot ([EBP+0xa]) rather than allocating a frame local;
 * we use a plain local, which is behaviourally identical. */
void director_script_camera(int value)
{
  unsigned char script_control;
  int16_t i;
  char *base;
  int unit_handle;
  int16_t perspective;
  void *camera_fn;

  script_control = (unsigned char)value;
  **(char **)0x5ab200 = (char)script_control;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++) {
    assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    base = (char *)0x3352b4 + (int)i * 0xf8;

    if (script_control != 0) {
      assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

      *(uint32_t *)(base + 4) = 0x853c0;
      *(float *)(base + 0xc0) = 1.0f;
      *(uint8_t *)(base + 0xbc) = 0;
    } else {
      assert_halt(i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

      unit_handle = player_control_get_unit_index(i);
      if (director_desired_perspective(unit_handle, &perspective) == 1) {
        /* 0x89850: following-camera data init; cdecl(camera_data_ptr). */
        following_camera_new((void *)(base + 8));
        camera_fn = (void *)0x89cd0;
      } else {
        /* 0x88c40: first-person camera data init; cdecl(camera_data_ptr). */
        first_person_camera_new((void *)(base + 8));
        camera_fn = (void *)0x89270;
      }

      camera_internal_set_camera_fn(i, camera_fn, 0);
      *(int16_t *)(base + 0x50) = perspective;
    }

    scripted_camera_enable(script_control);
  }
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
    camera_internal_cycle_mode(local_player_index, (int16_t *)0x266f68, 3);

  /* If a scripted camera is active (script_state[0] != 0), nothing else. */
  if (**(char **)0x5ab200 != 0)
    return;

  camera_internal_reevaluate(local_player_index, 0);

  current_camera = *(void **)(base + 4);

  if (is_dead != 0) {
    if (current_camera == (void *)0x85c80)
      return; /* already in the third-person follow camera */

    dead_camera_new((void *)(base + 8), local_player_index, -1);
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
  result = director_desired_perspective(unit_handle, &local_word);

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
    camera_internal_cycle_mode(local_player_index, (int16_t *)0x266f70, 4);
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

/*
 * FUN_000874d0 — dispatch per-player camera update based on director mode.
 *
 * Reads the global director mode from 0x3352ac (short) and calls the
 * appropriate per-player camera function:
 *   mode 0, 1 → director_set_player_camera_normal(local_player_index,
 * reset_flag, mode_flags) mode 2    →
 * director_set_player_camera_scripted(local_player_index, reset_flag) mode 4 →
 * director_apply_replay_mode_for_player(reset_flag, local_player_index,
 * mode_flags) other     → no-op
 *
 * local_player_index@<ecx>, reset_flag@<eax>, mode_flags@<edx>.
 *
 * 0x874d0 / director.obj
 */
void FUN_000874d0(int16_t local_player_index, char reset_flag, char mode_flags)
{
  switch (*(int16_t *)0x3352ac) {
  case 0:
  case 1:
    director_set_player_camera_normal(local_player_index, reset_flag,
                                      mode_flags);
    return;
  case 2:
    director_set_player_camera_scripted(local_player_index, reset_flag);
    return;
  case 4:
    director_apply_replay_mode_for_player(reset_flag, local_player_index,
                                          mode_flags);
    return;
  default:
    return;
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

/* editor_camera_initialize (0x87800) — initialize the scripted/editor camera
 * for one local player.
 *
 * On the first call after a map load (byte flag at 0x33569a still 0) the
 * editor-camera defaults are pulled out of the scenario tag: the tag_block at
 * scenario+0x354 (element size 0x34) supplies four dwords copied to the
 * globals at 0x33569c (position xyz at +0x0/+0x4/+0x8, angles at +0xc). If
 * either the block count (scenario+0x354) or its address (scenario+0x358) is
 * zero, the 0x14-byte global block at 0x33569c is zeroed instead.
 *
 * Afterwards the cached angles at 0x3356a8 are converted to a forward vector
 * and handed, together with the position globals, to the camera-data setup
 * helper at 0x89350. For local player 0 the camera-data pointer is latched in
 * the global at 0x3356b0, and when the mode word at 0x3356c4 is non-zero the
 * matching entry of the 8-byte-stride dispatch table at 0x2ee680 is called
 * with the camera-data pointer. */
void editor_camera_initialize(void *camera_data, int16_t local_player_index)
{
  void *scenario;
  uint32_t *element;
  float forward[3];

  if (*(char *)0x33569a == 0) {
    scenario = global_scenario_get();
    if (*(int *)((char *)scenario + 0x354) == 0 ||
        (scenario = global_scenario_get(),
         *(int *)((char *)scenario + 0x358) == 0)) {
      csmemset((void *)0x33569c, 0, 0x14);
    } else {
      scenario = global_scenario_get();
      element = (uint32_t *)tag_block_get_element(
        (void *)((char *)scenario + 0x354), 0, 0x34);
      *(uint32_t *)0x33569c = element[0];
      *(uint32_t *)0x3356a0 = element[1];
      *(uint32_t *)0x3356a4 = element[2];
      *(uint32_t *)0x3356a8 = element[3];
    }
  }
  *(char *)0x33569a = 1;

  angles_to_vector(forward, (float *)0x3356a8);

  /* 0x89350: cdecl(camera_data, position globals, forward vector) — three
   * pushes before the call, with the shared ADD ESP,0x14 also retiring
   * angles_to_vector's two arguments. */
  FUN_00089350(camera_data, (float *)0x33569c, forward);

  if (local_player_index == 0) {
    *(void **)0x3356b0 = camera_data;
  }

  if (*(int16_t *)0x3356c4 != 0) {
    (*(void (**)(void *))(0x2ee680 + (int)*(int16_t *)0x3356c4 * 8))(
      camera_data);
  }
}

/* editor_camera_get_focus (0x878d0) — read back the cached editor/flying
 * camera focus: the position xyz dwords at 0x33569c/0x3356a0/0x3356a4 and the
 * two angle dwords at 0x3356a8/0x3356ac.
 *
 * Both out-pointers are asserted non-NULL first; the assert strings and line
 * numbers (0x78, 0x79) come from editor_flying_camera.c, so they are spelled
 * explicitly rather than via assert_halt (which would stamp director.c).
 * The reference copies all five fields as plain dword MOVs, so the copy is
 * written on uint32_t lvalues rather than float ones. */
void editor_camera_get_focus(uint32_t *position, uint32_t *angles)
{
  if (position == 0) {
    display_assert("position",
                   "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x78, 1);
    system_exit(-1);
  }

  if (angles == 0) {
    display_assert("angles", "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                   0x79, 1);
    system_exit(-1);
  }

  position[0] = *(uint32_t *)0x33569c;
  position[1] = *(uint32_t *)0x3356a0;
  position[2] = *(uint32_t *)0x3356a4;
  angles[0] = *(uint32_t *)0x3356a8;
  angles[1] = *(uint32_t *)0x3356ac;
}

/* editor_camera_set_focus (0x87950) — store the editor/flying camera focus:
 * the position xyz dwords into 0x33569c/0x3356a0/0x3356a4 and the two angle
 * dwords into 0x3356a8/0x3356ac.  Mirror of editor_camera_get_focus.
 *
 * Both in-pointers are asserted non-NULL first; the assert strings and line
 * numbers (0x81, 0x82) come from editor_flying_camera.c, so they are spelled
 * explicitly rather than via assert_halt (which would stamp director.c).
 * The reference copies all five fields as plain dword MOVs, so the copy is
 * written on uint32_t lvalues rather than float ones. */
void editor_camera_set_focus(const uint32_t *position, const uint32_t *angles)
{
  if (position == 0) {
    display_assert("position",
                   "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x81, 1);
    system_exit(-1);
  }

  if (angles == 0) {
    display_assert("angles", "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                   0x82, 1);
    system_exit(-1);
  }

  *(uint32_t *)0x33569c = position[0];
  *(uint32_t *)0x3356a0 = position[1];
  *(uint32_t *)0x3356a4 = position[2];
  *(uint32_t *)0x3356a8 = angles[0];
  *(uint32_t *)0x3356ac = angles[1];
}

/* editor_camera_set_position (0x879d0) — push a new focus into the editor/
 * flying camera.  When the live camera-data pointer at 0x3356b0 is NULL the
 * values are parked in the cached focus globals via editor_camera_set_focus
 * and the "focus initialized" byte at 0x33569a is raised; otherwise the three
 * position dwords and two angle dwords are written straight into the live
 * camera data at +0x00..+0x08 and +0x0c/+0x10.
 *
 * Both in-pointers are asserted non-NULL first; the assert strings and line
 * numbers (0x94, 0x95) come from editor_flying_camera.c, so they are spelled
 * explicitly rather than via assert_halt (which would stamp director.c).
 * The reference copies every field as a plain dword MOV, so the copy is
 * written on uint32_t lvalues rather than float ones. */
void editor_camera_set_position(const uint32_t *point, const uint32_t *angles)
{
  uint32_t *camera_data;

  if (point == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                   0x94, 1);
    system_exit(-1);
  }

  if (angles == 0) {
    display_assert("angles", "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                   0x95, 1);
    system_exit(-1);
  }

  camera_data = *(uint32_t **)0x3356b0;
  if (camera_data == 0) {
    editor_camera_set_focus(point, angles);
    *(char *)0x33569a = 1;
    return;
  }

  camera_data[0] = point[0];
  camera_data[1] = point[1];
  camera_data[2] = point[2];
  camera_data[3] = angles[0];
  camera_data[4] = angles[1];
}

/* FUN_00087ac0 (0x87ac0) — set the editor/flying-camera enable byte at
 * 0x335699 and return its previous value.
 *
 * The reference loads the old byte into AL (MOV AL,[0x335699]) before storing
 * the incoming stack byte, and nothing on either path clobbers AL before the
 * RET, so the old value is the return value.  The name and the meaning of the
 * byte are unproven — there is no string or assert evidence in this function —
 * so the raw FUN_ name and a mechanical parameter name are kept.
 *
 * When the new value is zero and the live camera-data pointer at 0x3356b0 is
 * non-NULL, the dword at camera_data+0x14 is cleared. */
char FUN_00087ac0(char enabled)
{
  char previous;
  uint32_t *camera_data;

  previous = *(char *)0x335699;
  *(char *)0x335699 = enabled;

  if (enabled == 0) {
    camera_data = *(uint32_t **)0x3356b0;
    if (camera_data != 0) {
      camera_data[5] = 0;
    }
  }

  return previous;
}

/* editor_camera_set_mode (0x87b00) — switch the editor/flying camera mode
 * word at 0x3356c4, running the per-mode translation callbacks on the way out
 * of the old mode and into the new one.
 *
 * Two parallel dispatch tables share an 8-byte-stride array of mode records:
 * the _translate_from entry lives at 0x2ee67c and the _translate_to entry at
 * 0x2ee680 (the same record, +0 and +4).  Both are indexed by the sign-
 * extended mode word (MOVSX ECX,AX / MOVSX ESI,DI) and are called cdecl with
 * the live camera-data pointer re-loaded from 0x3356b0 at each call site
 * (MOV EAX,[0x3356b0] at 0x87b60, MOV ECX,[0x3356b0] at 0x87ba4).
 *
 * The translation callbacks only run when the camera-data pointer is non-NULL
 * and the mode actually changes; the mode word store and the console_printf
 * of the mode name (dword-stride name table at 0x2ee68c) happen on every path.
 * The assert strings and line numbers (0x12e, 0x134) come from
 * editor_flying_camera.c, so they are spelled explicitly rather than via
 * assert_halt (which would stamp director.c).
 *
 * The stack parameter is a 16-bit word: the reference reads it with
 * MOV DI,word ptr [EBP+0x8] and stores it back with
 * MOV word ptr [0x3356c4],DI. */
void editor_camera_set_mode(int16_t mode)
{
  int16_t current;
  const char *mode_name;

  if (*(void **)0x3356b0 != 0) {
    current = *(int16_t *)0x3356c4;
    if (current != mode) {
      if (current != 0) {
        if (*(void **)(0x2ee67c + (int)current * 8) == 0) {
          display_assert("translate_funcs[camera_mode][_translate_from]",
                         "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                         0x12e, 1);
          system_exit(-1);
        }
        (*(void (**)(void *))(0x2ee67c + (int)*(volatile int16_t *)0x3356c4 *
                                           8))(*(void **)0x3356b0);
      }

      if (mode != 0) {
        if (*(void **)(0x2ee680 + (int)mode * 8) == 0) {
          display_assert("translate_funcs[mode][_translate_to]",
                         "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                         0x134, 1);
          system_exit(-1);
        }
        (*(void (**)(void *))(0x2ee680 + (int)mode * 8))(*(void **)0x3356b0);
      }
    }
  }

  mode_name = *(const char **)(0x2ee68c + (int)mode * 4);
  *(int16_t *)0x3356c4 = mode;
  console_printf(0, mode_name);
}

/* 28-byte camera state block exchanged by FUN_00087c00 (0x87c00).  The
 * reference only ever moves it as seven dwords (MOV ECX,0x7 / REP MOVSD) or
 * touches individual dwords, so every member is typed uint32_t; +0xc is the
 * angle pair written by vector_to_angles, and +0x14/+0x18 are only ever
 * copied wholesale, never read or written individually. */
typedef struct camera_capture_state {
  uint32_t field_00;
  uint32_t field_04;
  uint32_t field_08;
  uint32_t field_0c;
  uint32_t field_10;
  uint32_t field_14;
  uint32_t field_18;
} camera_capture_state;

/* FUN_00087c00 (0x87c00) — record the caller's camera state into the capture
 * globals and hand back either the parked override or a default view.
 *
 * The caller's 28-byte block (stack parameter at [EBP+8], held in EAX) is
 * first copied into the globals at 0x3356d0 with MOV ECX,0x7 / REP MOVSD, so
 * it is written here as a whole-struct assignment rather than field stores.
 *
 * If the override byte at 0x33570c is non-zero, the seven dwords parked at
 * 0x3356f0 are copied back over the caller's block (second REP MOVSD) and the
 * function returns.  Otherwise the first three dwords are reset to
 * 0 / [0x2670d8] / 0 — the reference loads [0x2670d8] into ECX at 0x87c32,
 * before any of the three stores — and the angle pair at +0xc is recomputed by
 * vector_to_angles from the vector at *(0x2ee670) + 0x1c.  Dwords +0x14 and
 * +0x18 keep whatever the caller passed in on that path.
 *
 * Call site 0x87c56 is cdecl with ADD ESP,0x8: PUSH EDX (= *(0x2ee670) + 0x1c)
 * is the last argument (in_vector), PUSH EAX (= block + 0xc) the first
 * (out_angles).
 *
 * The parameter is declared void * so the kb.json declaration needs no
 * file-local type; the original reads it only as this seven-dword block. */
void FUN_00087c00(void *state)
{
  camera_capture_state *block = (camera_capture_state *)state;

  *(camera_capture_state *)0x3356d0 = *block;

  if (*(char *)0x33570c != 0) {
    *block = *(camera_capture_state *)0x3356f0;
    return;
  }

  block->field_00 = 0;
  block->field_04 = *(uint32_t *)0x2670d8;
  block->field_08 = 0;
  vector_to_angles((float *)&block->field_0c,
                   (float *)(*(char **)0x2ee670 + 0x1c));
}

/* editor_camera_update (0x87f20) — per-mode editor camera update entry stored
 * in the update_funcs table (the only xref is the DATA store at 0x87026).
 *
 * Ghidra types this void(void) and reports the three stack parameters as
 * in_stack_00000004/8/c; the disassembly reads [EBP+8], [EBP+0xc] (EDI) and
 * [EBP+0x10] (ESI), so the function really takes three cdecl parameters and
 * forwards all three to the dispatch entry at 0x87024.  param_2/param_3 keep
 * the types FUN_000853c0 already declares for the same two pointers.
 *
 * update_funcs is the 4-byte-stride table at 0x2ee674; translate_funcs is the
 * 8-byte-stride record pair at 0x2ee67c (_translate_from) / 0x2ee680
 * (_translate_to), both indexed by the sign-extended mode word at 0x3356c4.
 * The second assert checks the _translate_from slot at 0x2ee67c but the call
 * at 0x8800a dispatches through the _translate_to slot at 0x2ee680 — that
 * mismatch is in the original and is preserved here.
 *
 * [0x2ee670] is re-loaded for the +0x1c vector (MOV EDX at 0x87f81, MOV ECX
 * at 0x87fa1), so the reload is kept.  FUN_00087eb0 takes one argument: the
 * PUSH EDX at 0x87fba is covered by the single ADD ESP,0xc at 0x87fc6 that
 * also cleans the two vector_to_angles pushes. */
void editor_camera_update(int param_1, unsigned short *param_2,
                          unsigned int *param_3)
{
  uint32_t *camera_data;
  char *globals;

  if (*(void **)(0x2ee674 + (int)*(int16_t *)0x3356c4 * 4) == 0) {
    display_assert("update_funcs[camera_mode]",
                   "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x154,
                   1);
    system_exit(-1);
  }

  if (*(char *)0x335698 != 0) {
    if (*((char *)param_2 + 2) == 0) {
      scripted_camera_update(0, param_2, param_3);
      return;
    }

    camera_data = *(uint32_t **)0x3356b0;
    globals = *(char **)0x2ee670;
    camera_data[0] = *(uint32_t *)(globals + 0x10);
    camera_data[1] = *(uint32_t *)(globals + 0x14);
    camera_data[2] = *(uint32_t *)(globals + 0x18);
    vector_to_angles((float *)(camera_data + 3),
                     (float *)(*(char **)0x2ee670 + 0x1c));
    FUN_00087eb0(*(void **)0x2ee66c);

    if (*(int16_t *)0x3356c4 != 0) {
      if (*(void **)(0x2ee67c + (int)*(int16_t *)0x3356c4 * 8) == 0) {
        display_assert("translate_funcs[camera_mode][_translate_from]",
                       "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                       0x164, 1);
        system_exit(-1);
      }
      (*(void (**)(void *))(0x2ee680 + (int)*(int16_t *)0x3356c4 * 8))(
        *(void **)0x3356b0);
    }
  }

  (*(void (**)(int, unsigned short *, unsigned int *))(
    0x2ee674 + (int)*(int16_t *)0x3356c4 * 4))(param_1, param_2, param_3);

  if (*(char *)0x335698 != 0) {
    param_3[0x12] = 0;
    param_3[0] |= 9;
  }
}

/* editor_camera_set_scripted (0x88050) — enter or leave scripted camera mode.
 *
 * Ghidra types this void(void) and reports the parameter as
 * in_stack_00000004; the disassembly reads it at [EBP+8] into BL (TEST BL,BL
 * at 0x8805a, MOVZX EDX,BL at 0x881d1), so it is a single cdecl byte
 * parameter.  MOVZX is what makes it unsigned.
 *
 * Entering (enable != 0): run the _translate_from callback for the current
 * mode, derive the angle pair from the live look vector, hand both the
 * position (*(0x2ee670) + 0x10) and those angles to editor_camera_set_position,
 * then start a camera interpolation through FUN_00085280.  Leaving
 * (enable == 0): copy the live position/angles back into the camera-data
 * block at 0x3356b0, notify FUN_00087eb0, and run the mode callback.
 *
 * Both asserts test the _translate_from slot at 0x2ee67c, but the entering
 * path dispatches through 0x2ee67c while the leaving path dispatches through
 * 0x2ee680 (CALL dword ptr [EDX*8 + 0x2ee680] at 0x881c1) — the leaving-path
 * mismatch is in the original (same quirk as editor_camera_update) and is
 * preserved.
 *
 * Call sites:
 *  - 0x880c2 vector_to_angles: PUSH ECX (= *(0x2ee670) + 0x1c) is in_vector,
 *    PUSH EDX (= EBP-8) is out_angles.  [0x2ee670] is re-loaded at 0x880c7
 *    for the +0x10 position, so the reload is kept.
 *  - 0x880d5 editor_camera_set_position: PUSH EAX (= EBP-8) is the last
 *    argument (angles), PUSH ECX (= globals + 0x10) the first (point); the
 *    single ADD ESP,0x10 at 0x880df cleans both calls' four pushes.
 *  - 0x88101 / 0x88128 FUN_00085280 (ADD ESP,0x18, six dwords): first PUSH is
 *    param_1, so param_6 is the dword at [0x2ee66c] (-1 on the 0x8810e path),
 *    param_5 is 0, param_4 the float immediate 0x3f9c61aa = 1.2217305f.
 *    The two paths differ only in param_1 (0x3356b8 vs globals + 0x10) and
 *    param_6.
 *  - 0x88170 FUN_00087eb0 takes the dword at [0x2ee66c]; ADD ESP,0xc at
 *    0x8817b cleans it together with the two vector_to_angles pushes.
 *  - 0x881ef console_printf: the message selector is the char * table at
 *    0x2ee694 indexed by the zero-extended parameter.
 *
 * The old flag at 0x335698 is read into CL before the stores at 0x881e3 /
 * 0x881e9, so the save-then-overwrite order is preserved. */
void editor_camera_set_scripted(unsigned char enable)
{
  float local_angles[2];
  char *globals;
  uint32_t *src;
  uint32_t *camera_data;
  const char *message;
  char previous;
  int16_t mode;
  int target;

  if (enable != 0) {
    /* MOV AX,[0x3356c4] at 0x88062 is the only load; both MOVSX at 0x8806d
     * and 0x880a1 re-use AX, so the mode word is read once. */
    mode = *(int16_t *)0x3356c4;
    if (mode != 0) {
      if (*(void **)(0x2ee67c + (int)mode * 8) == 0) {
        display_assert("translate_funcs[camera_mode][_translate_from]",
                       "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                       0x183, 1);
        system_exit(-1);
      }
      (*(void (**)(void *))(0x2ee67c + (int)mode * 8))(*(void **)0x3356b0);
    }

    vector_to_angles(local_angles, (float *)(*(char **)0x2ee670 + 0x1c));
    globals = *(char **)0x2ee670;
    editor_camera_set_position((const uint32_t *)(globals + 0x10),
                               (const uint32_t *)local_angles);

    /* JZ 0x8810e at 0x880e5: the fall-through (target != -1) is the
     * 0x3356b8 path, so the != test comes first. */
    target = *(int *)0x2ee66c;
    if (target != -1) {
      globals = *(char **)0x2ee670;
      scripted_camera_set_camera_point_relative((float *)0x3356b8, (float *)(globals + 0x1c),
                   (float *)(globals + 0x28), 1.2217305f, 0, target);
    } else {
      globals = *(char **)0x2ee670;
      scripted_camera_set_camera_point_relative((float *)(globals + 0x10), (float *)(globals + 0x1c),
                   (float *)(globals + 0x28), 1.2217305f, 0, -1);
    }
  } else {
    /* ADD EAX,0x10 / MOV ECX,EAX at 0x8813a: globals + 0x10 is held in one
     * register and the three dwords are read through it. */
    src = (uint32_t *)(*(char **)0x2ee670 + 0x10);
    camera_data = *(uint32_t **)0x3356b0;
    camera_data[0] = src[0];
    camera_data[1] = src[1];
    camera_data[2] = src[2];
    vector_to_angles((float *)(camera_data + 3),
                     (float *)(*(char **)0x2ee670 + 0x1c));
    FUN_00087eb0(*(void **)0x2ee66c);

    mode = *(int16_t *)0x3356c4;
    if (mode != 0) {
      if (*(void **)(0x2ee67c + (int)mode * 8) == 0) {
        display_assert("translate_funcs[camera_mode][_translate_from]",
                       "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                       0x19e, 1);
        system_exit(-1);
      }
      (*(void (**)(void *))(0x2ee680 + (int)mode * 8))(*(void **)0x3356b0);
    }
  }

  /* The reference loads CL from 0x335698 and the message pointer before it
   * writes 0x3356ca / 0x335698 (stores at 0x881e3 / 0x881e9 sit after the
   * three console_printf pushes). */
  previous = *(char *)0x335698;
  message = *(const char **)(0x2ee694 + (unsigned int)enable * 4);
  *(char *)0x3356ca = previous;
  *(char *)0x335698 = (char)enable;
  console_printf(0, "%s scripted camera mode", message);
}

/* FUN_00088200 (0x88200) — park the caller's camera block as the override
 * state, then refill the caller's block from the live camera globals.
 *
 * Ghidra types this void(void) and reports the stack parameter as
 * in_stack_00000004; the disassembly reads it at [EBP+8] into EAX, so it is a
 * single cdecl pointer parameter.
 *
 * The 28-byte block is copied into the override globals at 0x3356f0 with
 * MOV ECX,0x7 / REP MOVSD (whole-struct assignment here, same shape as
 * FUN_00087c00), and the override flag byte at 0x33570c is then set to 1.
 *
 * The block is refilled from *(0x2ee670): dwords +0x10/+0x14/+0x18 go to
 * block[0..2] (the reference holds globals+0x10 in ECX across all three
 * loads), and the angle pair at +0xc is recomputed by vector_to_angles from
 * the vector at *(0x2ee670) + 0x1c.  [0x2ee670] is re-loaded for that vector
 * (MOV EDX at 0x88238), so the reload is kept.  Dwords +0x14 and +0x18 of the
 * block keep whatever the caller passed in.
 *
 * Call site 0x88246 is cdecl: PUSH EDX (= *(0x2ee670) + 0x1c) is the last
 * argument (in_vector), PUSH EAX (= block + 0xc) the first (out_angles).
 * FUN_00087eb0 takes one argument, the dword at [0x2ee66c] (PUSH EAX at
 * 0x88250); the single ADD ESP,0xc at 0x88256 cleans all three pushes. */
void FUN_00088200(void *state)
{
  camera_capture_state *block = (camera_capture_state *)state;
  char *globals;

  *(camera_capture_state *)0x3356f0 = *block;
  *(char *)0x33570c = 1;

  globals = *(char **)0x2ee670;
  block->field_00 = *(uint32_t *)(globals + 0x10);
  block->field_04 = *(uint32_t *)(globals + 0x14);
  block->field_08 = *(uint32_t *)(globals + 0x18);
  vector_to_angles((float *)&block->field_0c,
                   (float *)(*(char **)0x2ee670 + 0x1c));
  FUN_00087eb0(*(void **)0x2ee66c);
}

/* first_person_camera_new (0x88c40) — reset the first-person camera data
 * block.  Ghidra types this void(void) and reports the stack parameter as
 * in_stack_00000004; the disassembly loads it at [EBP+8] into ESI
 * (MOV ESI,[EBP+8] at 0x88c44), so it is a single cdecl pointer parameter.
 *
 * The null check at 0x88c47 (TEST ESI,ESI / JNZ 0x88c68) is an assert: the
 * pushed arguments at 0x88c4b-0x88c54 are display_assert("camera",
 * "c:\halo\SOURCE\camera\first_person_camera.c", 0x18, true) followed by
 * system_exit(-1) at 0x88c60.  The .rdata reason string is "camera", so the
 * original condition was written on a parameter of that name; the assert is
 * stamped with the first_person_camera.c TU, not director.c, so the file and
 * line are pinned with assert_halt_at.
 *
 * The body is a single dword store of 0 (MOV [ESI],0x0 at 0x88c68); the width
 * is a dword, and nothing else in the block is touched here. */
void first_person_camera_new(void *camera)
{
  assert_halt_at("c:\\halo\\SOURCE\\camera\\first_person_camera.c", 0x18,
                 camera);

  *(uint32_t *)camera = 0;
}

/* FUN_00088c80 (0x88c80) — produce the first-person camera's eye position and
 * forward vector for a unit.
 *
 * Ghidra types this void(void) and reports the three cdecl arguments as
 * in_stack_00000004/8/c; the disassembly loads them at [EBP+8] (EDI, then
 * ESI after the first call), [EBP+0xc] (EBX) and [EBP+0x10] (EDI) at
 * 0x88c89/0x88c94/0x88ca0, so it is a three-parameter cdecl function.
 *
 * Baseline: unit_set_seat_state fills the caller's position vector, and the
 * unit's own aiming vector at +0x1ec..+0x1f4 is copied out as the forward
 * vector.  Both copies are plain dword moves in the reference
 * (MOV EDX,[EAX] / MOV [ECX],EDX at 0x88ca9..0x88cb8), so they are spelled as
 * dword copies here rather than float assignments.
 *
 * Override: if the unit is riding something (+0xcc is a valid object handle,
 * type mask 2), the vehicle's seat definition is fetched from the 'vehi'
 * definition's seat block at +0x2e4, indexed by the unit's seat index at
 * +0x2a0 with element size 0x11c.  The reference reads only the low byte of
 * the seat definition and branches on its sign (MOV CL,[EAX] / TEST CL,CL /
 * JNS at 0x88cfd..0x88d04) — a seat flag whose bit 7 selects the marker-driven
 * camera.  In that case the "primary trigger" marker on the vehicle supplies
 * both vectors: the marker record's forward vector at +0x3c and its position
 * at +0x60, matching the marker layout used by player_control (0x6c-byte
 * record, one marker requested).
 *
 * Note the two halves are written to opposite parameters: the marker position
 * (+0x60, read at [EBP-0xc]) goes to the EBX parameter that unit_set_seat_state
 * filled, and the marker forward (+0x3c, read at [EBP-0x30]) goes to the EDI
 * parameter that received the unit's aiming vector. */
void FUN_00088c80(int unit_handle, float *out_position, float *out_forward)
{
  char *unit;
  char *vehicle;
  char *seat;
  char marker_buf[0x6c]; /* object_get_markers_by_string_id output */

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_set_seat_state(unit_handle, out_position);

  ((uint32_t *)out_forward)[0] = *(uint32_t *)(unit + 0x1ec);
  ((uint32_t *)out_forward)[1] = *(uint32_t *)(unit + 0x1f0);
  ((uint32_t *)out_forward)[2] = *(uint32_t *)(unit + 0x1f4);

  if (*(int *)(unit + 0xcc) != NONE) {
    vehicle =
      (char *)object_try_and_get_and_verify_type(*(int *)(unit + 0xcc), 2);
    if (vehicle != NULL) {
      /* one nested expression: the original cleans both calls with a single
       * ADD ESP,0x14 at 0x88cff */
      seat = (char *)tag_block_get_element(
        (char *)tag_get(0x76656869 /* 'vehi' */, *(int *)vehicle) + 0x2e4,
        *(int16_t *)(unit + 0x2a0), 0x11c);

      if (*seat < 0) {
        if (object_get_markers_by_string_id(*(int *)(unit + 0xcc),
                                            (void *)"primary trigger",
                                            marker_buf, 1) != 0) {
          ((uint32_t *)out_position)[0] = *(uint32_t *)(marker_buf + 0x60);
          ((uint32_t *)out_position)[1] = *(uint32_t *)(marker_buf + 0x64);
          ((uint32_t *)out_position)[2] = *(uint32_t *)(marker_buf + 0x68);
          ((uint32_t *)out_forward)[0] = *(uint32_t *)(marker_buf + 0x3c);
          ((uint32_t *)out_forward)[1] = *(uint32_t *)(marker_buf + 0x40);
          ((uint32_t *)out_forward)[2] = *(uint32_t *)(marker_buf + 0x44);
        }
      }
    }
  }
}
