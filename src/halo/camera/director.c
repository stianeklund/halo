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
 *   0x89cd0  following camera for third-person and vehicle views
 *
 * Register-arg callees are reached through shims in camera_internal.h. */
#include "camera_internal.h"
#include "director.h"
#include "observer.h"

/*
 * FUN_00085a40 (0x85a40) — is there another player on this player's team?
 *
 * player_handle arrives in EDI (the decompiler reports it as `unaff_EDI`:
 * EDI is read at 0x85a4d/0x85a80 and never written in this function).
 * Returns in AL: 1 as soon as an iterated player other than player_handle has
 * the same team_index; otherwise the BL zeroed at 0x85a62 (0).
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
 * 0x85a40 / director.obj
 */
bool FUN_00085a40(int player_handle)
{
  data_iter_t iter;
  player_data_t *player;
  player_data_t *other;
  int32_t team_index;
  bool result;

  player = datum_get(player_data, player_handle);
  team_index = player->team_index;
  result = 0;

  data_iterator_new(&iter, player_data);
  other = data_iterator_next(&iter);
  while (other != NULL) {
    if (iter.datum_handle != (uint32_t)player_handle &&
        other->team_index == team_index) {
      return 1;
    }
    other = data_iterator_next(&iter);
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
 *               when restrict_flag is set, the source of the team_index.
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
 * player has a unit, and — only when restrict_flag is set — it is on the
 * caller player's team.
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
  player_data_t *player;
  int32_t team_index;
  uint32_t result;

  if (restrict_flag == 0) {
    team_index = -1;
  } else {
    player = datum_get(player_data, (int)player_handle);
    team_index = player->team_index;
  }

  result = 0xffffffff;
  data_iterator_new(&iter, player_data);
  player = data_iterator_next(&iter);
  while (player != NULL) {
    if (iter.datum_handle != player_handle && player->unit_handle != NONE &&
        (restrict_flag == 0 || player->team_index == team_index)) {
      if (result == 0xffffffff) {
        result = iter.datum_handle;
      } else if ((int)(iter.datum_handle & 0xffff) >
                 (int)(current_handle & 0xffff)) {
        result = iter.datum_handle;
        break;
      }
    }
    player = data_iterator_next(&iter);
  }

  if (result == 0xffffffff) {
    return current_handle;
  }
  return result;
}

/*
 * dead_camera_new (0x85b60) — initialize the dead camera for a local player.
 *
 * cdecl(camera, local_player_index, unit_index). The assert at 0x85b7b passes
 * "camera" as the reason string with file c:\halo\SOURCE\camera\dead_camera.c
 * line 0x17 (23), which names the first parameter.
 *
 * The position is copied as three dwords from observer_get_camera(); the
 * timer is a dword copy of the pool constant.  The three random_real_range()
 * calls happen in the order distance, yaw, pitch, each re-reading the local
 * seed address.  unit_index NONE follows the player's previous unit and only
 * then calls game_engine_running() for the switch delay.
 *
 * 0x85b60 / director.obj
 */
void dead_camera_new(void *camera, int16_t local_player_index, int unit_index)
{
  dead_camera_t *dead_camera;
  real_point3d *observer_position;
  real switch_timer;
  real pitch;
  real timer;
  int32_t player_index;
  player_data_t *player;

  observer_position =
    observer_get_camera((unsigned short)local_player_index);

  assert_halt_at("c:\\halo\\SOURCE\\camera\\dead_camera.c", 23, camera);

  dead_camera = camera;
  dead_camera->position = *observer_position;
  dead_camera->field_of_view = 1.2217305f; /* 70 degrees */
  dead_camera->distance =
    random_real_range(random_math_get_local_seed_address(), 2.0f, 6.0f);
  dead_camera->facing.yaw = random_real_range(
    random_math_get_local_seed_address(), 0.0f, 6.2831855f);
  pitch = random_real_range(random_math_get_local_seed_address(),
                            0.47123894f, 1.0995574f);
  /* The timer constant is loaded at 0x85bfc, ahead of the FCHS/FSTP pair at
   * 0x85c02/0x85c07 — keep the read in front of the negated store. */
  timer = 3.0f;
  dead_camera->facing.pitch = -pitch;
  dead_camera->timer = timer;

  if (unit_index != NONE) {
    switch_timer = 3.4028235e+38f;
  } else if (game_engine_running()) {
    switch_timer = 15.0f;
  } else {
    switch_timer = 3.0f;
  }
  dead_camera->switch_timer = switch_timer;

  player_index = local_player_get_player_index(local_player_index);
  dead_camera->player_index = player_index;

  /* JNE at 0x85c43 makes the unit_index == NONE case the fall-through; both
   * tails duplicate the unit_index/current_player_index stores and reload
   * player_index from the block. */
  if (unit_index == NONE) {
    player = datum_get(player_data, player_index);
    dead_camera->unit_index = player->previous_unit_handle;
    dead_camera->current_player_index = dead_camera->player_index;
    return;
  }

  dead_camera->unit_index = unit_index;
  dead_camera->current_player_index = dead_camera->player_index;
}

/* camera_director_t, director_globals_t, director_scripting_t and
 * editor_camera_globals_t live in types.h (kb.json data symbols).
 *
 * Original entry addresses of the camera update procs held in
 * camera_director_t.camera_proc.  The slot is written and compared by
 * director code that still runs as original code (director_choose_game_
 * perspective and director_script_camera are dormant lifts, and
 * director_load_camera is unported); that code stores these ORIGINAL
 * addresses.  Taking the address of the C function would instead yield the
 * address of our implementation (the original entry only holds the
 * redirect), so an identity test against the symbol would never match a slot
 * the original code wrote — the lip-sync class of bug in lift-learnings
 * section 54.  Every camera_proc store and test in director.obj must switch
 * to the function symbols together, once all of them are active lifts. */
enum {
  SCRIPTED_CAMERA_UPDATE_ENTRY = 0x853c0, /* scripted_camera_update */
  DEAD_CAMERA_UPDATE_ENTRY = 0x85c80, /* dead_camera_update */
  EDITOR_CAMERA_UPDATE_ENTRY = 0x87f20, /* editor_camera_update */
  FIRST_PERSON_CAMERA_UPDATE_ENTRY = 0x89270, /* first_person_camera_update */
  FLYING_CAMERA_UPDATE_ENTRY = 0x893a0, /* flying_camera_update */
  FOLLOWING_CAMERA_UPDATE_ENTRY = 0x89cd0, /* following_camera_update */
  ORBITING_CAMERA_UPDATE_ENTRY = 0x8cf30 /* orbiting_camera_update */
};


/* dead_camera_update (0x85c80).  With no player to follow the next unit is
 * the command pointer itself, as in the original. */
void dead_camera_update(void *camera_data, void *control_data,
                        void *result_data)
{
  dead_camera_t *camera;
  camera_action_t *controls;
  camera_command_t *result;
  object_data_t *unit;
  player_data_t *player;
  int32_t next_player;
  int32_t next_unit;
  bool match_team;

  camera = camera_data;
  controls = control_data;
  result = result_data;
  unit = camera->unit_index == NONE ?
           NULL :
           object_try_and_get_and_verify_type(
             camera->unit_index, -1);
  result->position =
    unit != NULL ? unit->bounding_sphere_center : camera->position;
  result->depth = camera->distance;
  angles_to_vector(&result->forward.i, &camera->facing.yaw);
  observer_up_from_forward(&result->forward, &result->up);
  result->field_of_view = camera->field_of_view;
  result->offset = *global_zero_vector3d;
  result->velocity = *global_zero_vector3d;
  result->flags = FLAG(_observer_command_valid_bit);
  result->timer = 0.0f > camera->timer ? 0.0f : camera->timer;
  result->field_54[0] = 0.0f;
  result->field_4c[0] = 3;
  if (camera->timer == dead_camera_initial_timer) {
    result->depth = 0.5f;
    result->field_54[2] = 0.0f;
    result->field_4c[2] = 3;
  }
  camera->timer -= controls->seconds_elapsed;
  camera->switch_timer -= controls->seconds_elapsed;
  camera->switch_timer =
    0.0f > camera->switch_timer ? 0.0f : camera->switch_timer;
  if (camera->switch_timer == 0.0f && !game_time_get_paused()) {
    match_team = FUN_00085a40(camera->player_index);
    next_player = FUN_00085ab0((char)match_team, camera->player_index,
                               camera->current_player_index);
    camera->current_player_index = next_player;
    if (next_player != NONE) {
      player = datum_get(player_data, next_player);
      next_unit = player->unit_handle;
    } else {
      next_unit = (int32_t)result_data;
    }
    if (next_unit != camera->unit_index && next_unit != NONE) {
      camera->timer = dead_camera_initial_timer;
      camera->unit_index = next_unit;
    }
    camera->switch_timer = game_engine_running() ?
                             15.0f :
                             3.0f;
  }
  if (!director_camera_command_valid(result, result->field_of_view,
                                     result->timer)) {
    display_assert(csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
                            CAMERA_COMMAND_INVALID_ARGUMENTS(result)),
                   "c:\\halo\\SOURCE\\camera\\dead_camera.c", 0x9e, true);
    system_exit(-1);
  }
}

/* Allocate director scripting state. */
void director_initialize(void)
{
  director_camera_scripted = game_state_malloc(
    "director scripting", 0, sizeof(director_scripting_t));
  director_camera_scripted->camera_scripted = false;
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
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  director_globals.local_players[local_player_index].inhibited_facing = true;
}

/* Set active local-player context used by hs/console during cheat dispatch.
 * Writes 1 to the per-player director state byte at struct offset +0x4e. */
void director_set_local_player_context(int16_t player_index)
{
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     player_index >= 0 &&
                       player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  director_globals.local_players[player_index].inhibited_input = true;
}

/* director_inhibited_facing (0x86270) — read the per-player "facing inhibited"
 * flag byte. Base 0x335301 = per-player struct base 0x3352b4 + 0x4d, stride
 * 0xf8 (same array director_set_local_player_context writes at +0x4e).
 * Reference returns the raw byte in AL (no test), so the load is typed bool. */
bool director_inhibited_facing(short local_player_index)
{
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return director_globals.local_players[local_player_index].inhibited_facing;
}

/* director_inhibited_input (0x862c0) — read the per-player "input inhibited"
 * flag byte. Base 0x335302 = per-player struct base 0x3352b4 + 0x4e, stride
 * 0xf8 — the same byte director_set_local_player_context writes 1 to.
 * Reference returns the raw byte in AL (no test), so the load is typed bool. */
bool director_inhibited_input(short local_player_index)
{
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  return director_globals.local_players[local_player_index].inhibited_input;
}

/* Number of director game modes — the assert bound is CMP SI,0x5 at 0x8631d. */
#define NUMBER_OF_DIRECTOR_GAME_MODES 5

/* director_set_mode (0x86310) — set the global director mode word at
 * 0x3352ac. Only writes when the mode actually changes, and then raises the
 * flag byte at 0x3352ae so the next update re-dispatches the camera. */
void director_set_mode(int16_t mode)
{
  assert_halt_msg_at("mode>=0 && mode<NUMBER_OF_DIRECTOR_GAME_MODES",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0x180,
                     mode >= 0 && mode < NUMBER_OF_DIRECTOR_GAME_MODES);
  if (director_globals.game_mode != mode) {
    director_globals.game_mode = mode;
    director_globals.initialize_camera = true;
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
  observer_result_t *camera;

  file = crt_fopen("d:\\camera.txt", "w");
  if (file) {
    camera = observer_get_camera(0);
    crt_fprintf(file, "%f %f %f\n", (double)camera->position.x,
                (double)camera->position.y, (double)camera->position.z);
    crt_fprintf(file, "%f %f %f\n", (double)camera->forward.i,
                (double)camera->forward.j, (double)camera->forward.k);
    crt_fprintf(file, "%f %f %f\n", (double)camera->up.i, (double)camera->up.j,
                (double)camera->up.k);
    crt_fprintf(file, "%f\n", (double)camera->field_of_view);
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
 * Single return of the cached word (the compiler duplicates it per arm): the
 * first-person branch only refreshes the cache when the timer
 * equals 0.0f (constant at 0x2533c0, confirmed 0.0f elsewhere in-tree),
 * otherwise it falls through and returns the stale cached value unchanged.
 */
int16_t director_get_perspective(int16_t local_player_index)
{
  camera_director_t *director;


  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("local_player_index>=0 && "
                   "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                   "c:\\halo\\SOURCE\\camera\\director.c", 0xb3, 1);
    system_exit(-1);
  }

  director = &director_globals.local_players[local_player_index];

  if (director->camera_proc == FIRST_PERSON_CAMERA_UPDATE_ENTRY) {
    if (director->camera_change_pause == 0.0f) {
      director->perspective = 0;
    }
  } else if (director->camera_proc == FOLLOWING_CAMERA_UPDATE_ENTRY) {
    director->perspective = 1;

  } else {
    director->perspective =
      (int16_t)((director->camera_proc != SCRIPTED_CAMERA_UPDATE_ENTRY) + 2);
  }

  return director->perspective;
}

/*
 * director_desired_perspective (0x864b0) — classify the perspective a unit
 * wants and whether the director should use the following camera.
 *
 * Writes a perspective word through *perspective (0 = none, 1 = entering a
 * seat, 2 = seated, 3 = exiting) and returns the 16-bit "following" flag.
 * The seat flags come from the parent unit's 'unit' tag seat block.
 *
 * Single exit: every perspective arm falls through to the (== 1 || == 3)
 * test, which raises the flag for the enter/exit-seat states.  When the unit
 * has no parent that test re-reads the zero stored at entry, as in the
 * reference.
 */
int16_t director_desired_perspective(int unit_handle, int16_t *perspective)
{
  int16_t following = 0;

  *perspective = 0;
  if (unit_handle != -1) {
    unit_data_t *unit =
      object_get_and_verify_type(unit_handle, _object_mask_unit);

    if (unit->object.parent_object_index.value != -1) {
      object_data_t *parent_object =
        object_get_and_verify_type(
          unit->object.parent_object_index.value, _object_mask_all);

      if ((FLAG(parent_object->type) & _object_mask_unit) != 0) {
        bool third_person_on_enter;
        unit_seat_t *seat = tag_block_get_element(
          &unit_definition_get(parent_object->tag_index)->seats,
          (short)unit->parent_seat_index, sizeof(unit_seat_t));

        third_person_on_enter =
          (seat->flags & FLAG(_unit_seat_third_person_on_enter_bit)) != 0;
        if ((seat->flags & FLAG(_unit_seat_third_person_camera_bit)) != 0)
          following = 1;
        if (third_person_on_enter) {
          /* unk_595 = animation state */
          if ((char)unit->unk_595 == _unit_state_entering_seat)
            *perspective = 1;
          else if ((char)unit->unk_595 == _unit_state_exiting_seat)
            *perspective = 3;
          else
            *perspective = 2;
        } else {
          *perspective = 2;
        }
      } else {
        *perspective = 2;
      }
    }

    if (*perspective == 1 || *perspective == 3)
      following = 1;
  }

  return following;
}


/*
 * FUN_000865a0 — install a camera proc for a local player (index in SI),
 * resetting the debug controls; pause_camera_change primes the camera
 * change pause.
 *
 * 0x865a0 / director.obj
 */
void FUN_000865a0(int16_t local_player_index, int camera_proc,
                  bool pause_camera_change)
{
  camera_director_t *director;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  director = &director_globals.local_players[local_player_index];
  director->camera_proc = camera_proc;
  director->debug_input_scale = 1.0f;
  director->debug_controls = false;
  if (pause_camera_change) {
    director->camera_change_pause = 1.0f;
  }
}


/* Per-player default-state init (0x86600). Resets the four debug_variables
 * {value, velocity, delta} of a local player: value from the definitions'
 * initial_value (0x2ee604 = 0x2ee5f8 + 0xc, 0x1c stride), the other two to
 * zero.  The index form (rather than a walking instance pointer) is what
 * makes VC71 split the address into base + 0xd0 and count the loop down
 * with DEC, as the original does.
 *
 * Original prototype passes the player index in AX — reached via the
 * reverse-thunk codegen in tools/patch.py when the original binary calls
 * 0x86600. Our C signature is plain cdecl. */
void director_init_player_cameras(int16_t local_player_index)
{
  camera_director_t *director;
  director_variable_definition_t *definition;
  director_variable_instance_t *instance;
  int i;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  director = &director_globals.local_players[local_player_index];
  definition = &director_variable_definitions[0];

  i = NUMBER_OF_DIRECTOR_VARIABLES;
  do {
    instance = &director->debug_variables[NUMBER_OF_DIRECTOR_VARIABLES - i];
    instance->value = definition->initial_value;
    instance->velocity = 0.0f;
    instance->delta = 0.0f;
    definition++;
    i--;
  } while (i != 0);
}

/* FUN_00086670: input-variable integration used by the flying camera.
 * 2276 tests the first definition's has_hyper_scale byte (0x2ee610) for
 * every variable; this intentionally preserves that original behavior. */
void FUN_00086670(int16_t local_player_index, int mode_flags, float fwd)
{
  camera_director_t *director;
  director_variable_definition_t *definition;
  director_variable_instance_t *instance;
  real hyper_scale;
  real velocity_scale;
  real delta;
  bool negative;
  bool positive;
  bool reset;
  int i;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  director = &director_globals.local_players[local_player_index];
  director->debug_input_scale *= (real)pow(director_input_scale_step, fwd);
  director->debug_input_scale =
    director->debug_input_scale < 0.01f ?
      0.01f :
      (director->debug_input_scale > 50.0f ?
         50.0f :
         director->debug_input_scale);
  for (i = 0; i < NUMBER_OF_DIRECTOR_VARIABLES; ++i) {
    definition = &director_variable_definitions[i];
    instance = &director->debug_variables[i];
    hyper_scale = director_variable_definitions[0].has_hyper_scale ?
                    director->debug_input_scale :
                    1.0f;
    velocity_scale = director_globals.dtime * 5.0f;
    velocity_scale = velocity_scale < 0.0f ?
                       0.0f :
                       (velocity_scale > 1.0f ? 1.0f : velocity_scale);
    velocity_scale = 1.0f - velocity_scale;
    negative = definition->negative_bit != -1 &&
               (mode_flags & (1U << definition->negative_bit)) != 0;
    positive = definition->positive_bit != -1 &&
               (mode_flags & (1U << definition->positive_bit)) != 0;
    reset = definition->reset_bit != -1 &&
            (mode_flags & (1U << definition->reset_bit)) != 0;
    instance->velocity *= velocity_scale;
    if (negative && !positive)
      instance->velocity -= director_variable_acceleration *
                            definition->scale * director_globals.dtime *
                            hyper_scale;
    else if (positive && !negative)
      instance->velocity += director_variable_acceleration *
                            definition->scale * director_globals.dtime *
                            hyper_scale;
    else if (game_in_editor())
      instance->velocity = 0.0f;
    delta = director_globals.dtime * instance->velocity;
    instance->delta = delta;
    if (reset)
      instance->value = definition->initial_value;
    else
      instance->value += delta;
    instance->value =
      instance->value < definition->minimum ?
        definition->minimum :
        (instance->value > definition->maximum ? definition->maximum :
                                                 instance->value);
  }
}

/* Clear every local player's camera proc and debug controls when the old
 * map is disposed, and drop scripted camera control. */
void director_dispose_from_old_map(void)
{
  int16_t i;
  camera_director_t *director = director_globals.local_players;

  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++) {
    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
    director->camera_proc = 0;
    director->debug_input_scale = 1.0f;
    director->debug_controls = false;
    director++;
  }
  director_camera_scripted->camera_scripted = false;
}

/* director_load_camera (0x86900) — debug helper that reads "d:\camera.txt"
 * (the director_save_camera format) back into local player 0's director as a
 * flying camera. The cross/dot product is inline: the reference keeps it on
 * the x87 stack (0x34-byte frame holds only the 13 scanned/derived floats).
 * camera_data +0x14/+0x18 are flying-camera fields (unnamed). The proc slot
 * gets the ORIGINAL entry address (see the enum above). */
void director_load_camera(void)
{
  void *file;
  real_point3d position;
  real_vector3d forward;
  real_vector3d derived_up;
  real_vector3d stored_up;
  real field_of_view;
  camera_director_t *director;
  flying_camera_t *camera;

  file = crt_fopen("d:\\camera.txt", "r");
  if (file) {
    _fscanf(file, "%f %f %f\n", &position.x, &position.y, &position.z);
    _fscanf(file, "%f %f %f\n", &forward.i, &forward.j, &forward.k);
    _fscanf(file, "%f %f %f\n", &stored_up.i, &stored_up.j, &stored_up.k);
    _fscanf(file, "%f\n", &field_of_view);
    crt_fclose(file);
    director = &director_globals.local_players[0];
    camera = director_flying_camera(director);
    FUN_00089350(camera, &position, &forward);
    observer_up_from_forward(&forward, &derived_up);
    camera->roll = FUN_0010c510(&stored_up.i, &derived_up.i);
    if ((derived_up.k * stored_up.j - stored_up.k * derived_up.j) *
            forward.i +
          (forward.j *
             (stored_up.k * derived_up.i - derived_up.k * stored_up.i) +
           forward.k *
             (stored_up.i * derived_up.j - derived_up.i * stored_up.j)) >
        0.0f) {
      camera->roll = -camera->roll;
    }
    camera->field_of_view = field_of_view;
    director->camera_proc = FLYING_CAMERA_UPDATE_ENTRY;
    director->debug_input_scale = 1.0f;
    director->debug_controls = false;
    director->camera_mode_index = 2;
  }
}

/* FUN_00086a50: AX/EBX inputs and signed word table/count are retained in
 * kb.json. Callback slots retain original VAs for director comparisons. */
void FUN_00086a50(int local_player_index, int16_t *mode_table, int16_t count)
{
  camera_director_t *director;
  int16_t camera_mode;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     (int16_t)local_player_index >= 0 &&
                       (int16_t)local_player_index <
                         MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  director = &director_globals.local_players[(int16_t)local_player_index];
  director->camera_mode_index = (director->camera_mode_index + 1) % count;
  camera_mode = mode_table[director->camera_mode_index];
  switch (camera_mode) {
  case _director_camera_mode_following:
    following_camera_new(director->camera_data);
    FUN_000865a0((int16_t)local_player_index, FOLLOWING_CAMERA_UPDATE_ENTRY,
                 true);
    break;
  case _director_camera_mode_orbiting:
    orbiting_camera_new(director->camera_data, director->command.depth,
                        &director->command.forward);
    FUN_000865a0((int16_t)local_player_index, ORBITING_CAMERA_UPDATE_ENTRY,
                 true);
    break;
  case _director_camera_mode_flying:
    FUN_00089350(director->camera_data, &director->command.position,
                 &director->command.forward);
    FUN_000865a0((int16_t)local_player_index, FLYING_CAMERA_UPDATE_ENTRY,
                 true);
    break;
  case _director_camera_mode_editor:
    break;
  case _director_camera_mode_first_person:
    first_person_camera_new(director->camera_data);
    FUN_000865a0((int16_t)local_player_index, FIRST_PERSON_CAMERA_UPDATE_ENTRY,
                 true);
    break;
  default:
    display_assert(NULL, "c:\\halo\\SOURCE\\camera\\director.c", 0x200, 1);
    system_exit(-1);
    break;
  }
  console_printf(
    0, "%s camera",
    director_camera_mode_names[mode_table[director->camera_mode_index]]);
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
 * on both exit paths). */
int16_t director_camera_deterministic(int unit_handle,
                                      real_point3d *out_position,
                                      real_vector3d *out_forward)
{
  int16_t perspective;
  int16_t result;

  result = director_desired_perspective(unit_handle, &perspective);

  if (result == 0) {
    FUN_00088c80(unit_handle, out_position, out_forward);
  } else {
    FUN_00089c00(unit_handle, out_position, out_forward);
  }

  return result;
}

/* director_choose_game_perspective (0x86be0) — pick the first-person or
 * following camera for a local player's unit.
 *
 * Register ABI: local_player_index @<eax> (kept in
 * SI, 16-bit TEST/CMP), force @<bl> (read by five TEST BL,BL, never written).
 * Sole caller: director_set_player_camera_normal (0x86ed5).
 *
 * The director slot is director_globals.local_players[index]: camera_proc,
 * camera_data and the last perspective word (seat_state). The camera procs
 * are stored as their original entry addresses (the *_UPDATE_ENTRY enum)
 * because other director code compares the slot against those values; see
 * the note on the enum. */
void director_choose_game_perspective(int16_t local_player_index, char force)
{
  camera_director_t *director;
  int16_t following;
  int16_t perspective;
  int unit_index;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  director = &director_globals.local_players[local_player_index];
  unit_index = player_control_get_unit_index(local_player_index);
  following = director_desired_perspective(unit_index, &perspective);
  if (force || director->seat_state != perspective) {
    if (following == 1) {
      if (force || director->camera_proc == FIRST_PERSON_CAMERA_UPDATE_ENTRY) {
        following_camera_new(director->camera_data);
        FUN_000865a0(local_player_index, FOLLOWING_CAMERA_UPDATE_ENTRY, !force);
      }
    } else if (force ||
               director->camera_proc == FOLLOWING_CAMERA_UPDATE_ENTRY) {
      first_person_camera_new(director->camera_data);
      FUN_000865a0(local_player_index, FIRST_PERSON_CAMERA_UPDATE_ENTRY,
                   !force);
    }
    director->seat_state = perspective;
  }
}

/* director_script_camera (0x86cb0) — enable or disable scripted camera control
 * for every local player.
 *
 * Stores the low byte of the argument into
 * director_camera_scripted->camera_scripted, then walks the four
 * director_globals.local_players slots. Each iteration asserts the player index
 * (director.c line 0xb3, the director_get check) and then:
 *
 *   value != 0: install the scripted camera fn (0x853c0) through
 *     FUN_000865a0 with the top-timer flag clear. The reference inlines that
 *     helper: fn at slot +0x08, 1.0f at +0xc4, byte clear at +0xc0, each
 *     addressed off EDI = slot + 0xc4 (0x335374 for player 0).
 *
 *   value == 0: forced re-evaluation of the gameplay camera, i.e.
 *     director_choose_game_perspective(i, TRUE), which the reference also
 *     inlines (its own index assert is the third 0xb3 site). With force set
 *     every gate in that function is taken, so the player's unit is
 *     classified by director_desired_perspective (0x864b0) and either the
 *     following camera (0x89850 data init, 0x89cd0 update fn) or the
 *     first-person camera (0x88c40 data init, 0x89270 update fn) is
 *     installed with the top-timer flag clear; the perspective word is
 *     stored at slot +0x54.
 *
 * Either way scripted_camera_enable (0x84fe0) is called once per iteration
 * with the same byte; the reference pushes the whole incoming dword (PUSH
 * EBX), which is the same low byte the callee reads.
 *
 * Residual shape gaps (measured, not behavioural):
 *  - the reference keeps the perspective out-slot of the inlined
 *    classification in the upper half of the incoming argument slot
 *    ([EBP+0xa]); our compile allocates a frame local instead;
 *  - the reference calls first_person_camera_new (0x88c40) out of line,
 *    while our compile inlines it into the classification arm;
 *  - the reference reuses its strength-reduced slot pointer (EDI) for the
 *    inlined classification; ours recomputes index * 0xf8 there.
 *
 * Earlier revisions spelled the slot writes out by hand at +0x04/+0xbc/+0xc0
 * of 0x3352b4 and duplicated the classification inline. Those are the same
 * addresses as the helper calls above (0x3352b4 + n == 0x3352b0 + n + 4);
 * calling the helpers reproduces the reference's three index asserts and its
 * call sequence. Behaviour is unchanged: the same stores, the same callees.
 */
/* The scripted camera proc is installed by its original entry address,
 * SCRIPTED_CAMERA_UPDATE_ENTRY, for the reason given on the enum. */
void director_script_camera(unsigned char value)
{
  int16_t i;

  director_camera_scripted->camera_scripted = value;
  for (i = 0; i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; i++) {
    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    if (value != 0) {
      /* Install the scripted camera without priming the top timer. */
      FUN_000865a0(i, SCRIPTED_CAMERA_UPDATE_ENTRY, 0);
    } else {
      /* Forced re-evaluation of the gameplay camera for this player. */
      director_choose_game_perspective(i, 1);
    }

    scripted_camera_enable(value);
  }
}

/* Normal-play camera dispatch for one player (mode-0/1, 0x86de0).
 *
 * reset_flag installs a fresh first-person camera (the inlined body of
 * FUN_000865a0 without the change pause).  Otherwise mode_flags cycles the
 * game camera modes, and unless a script owns the camera the gameplay
 * perspective is re-evaluated; a player with no unit whose +0xaa word is
 * positive gets the dead camera, and leaving the dead camera re-selects the
 * following or first-person camera from director_desired_perspective.
 *
 * Original receives player index in AX, plus two byte stack args —
 * reverse-thunked to plain cdecl by tools/patch.py. */
void director_set_player_camera_normal(int16_t local_player_index,
                                       char reset_flag, char mode_flags)
{
  camera_director_t *director;
  uint8_t dead;
  director_player_view_t *player;
  int32_t unit_index;
  int16_t perspective;
  int16_t following;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  director = &director_globals.local_players[local_player_index];

  if (reset_flag != 0) {
    first_person_camera_new(director->camera_data);

    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       local_player_index >= 0 &&
                         local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    director->debug_controls = false;
    director->camera_proc = FIRST_PERSON_CAMERA_UPDATE_ENTRY;
    director->debug_input_scale = 1.0f;
    return;
  }

  player = datum_get(
    player_data, local_player_get_player_index(local_player_index));
  if (player->unit_handle != NONE) {
    dead = false;
  } else if (player->field_aa > 0) {
    dead = true;
  } else {
    dead = false;
  }

  if (mode_flags != 0)
    FUN_00086a50(local_player_index, director_game_camera_modes, 3);

  if (director_camera_scripted->camera_scripted)
    return;

  director_choose_game_perspective(local_player_index, false);

  if (dead) {
    if (director->camera_proc == DEAD_CAMERA_UPDATE_ENTRY)
      return;

    dead_camera_new(director->camera_data, local_player_index, NONE);
    FUN_000865a0(local_player_index, DEAD_CAMERA_UPDATE_ENTRY, true);
    return;
  }

  if (director->camera_proc != DEAD_CAMERA_UPDATE_ENTRY)
    return;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  unit_index = player_control_get_unit_index(local_player_index);
  following = director_desired_perspective(unit_index, &perspective);

  if (following == 1) {
    following_camera_new(director->camera_data);
    FUN_000865a0(local_player_index, FOLLOWING_CAMERA_UPDATE_ENTRY, false);
  } else {
    first_person_camera_new(director->camera_data);
    FUN_000865a0(local_player_index, FIRST_PERSON_CAMERA_UPDATE_ENTRY, false);
  }

  director->seat_state = perspective;
}

/* Switch player to the editor camera (mode 2 dispatch, 0x86fa0).  Unless it
 * is already installed (or reset_flag forces it), initialize the editor
 * camera and install editor_camera_update with the inlined FUN_000865a0
 * stores.
 *
 * Original receives player index in SI and reset_flag as a byte stack arg —
 * reverse-thunked to plain cdecl by tools/patch.py. */
void director_set_player_camera_scripted(int16_t local_player_index,
                                         char reset_flag)
{
  camera_director_t *director;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  director = &director_globals.local_players[local_player_index];

  if (reset_flag != 0 || director->camera_proc != EDITOR_CAMERA_UPDATE_ENTRY) {
    editor_camera_initialize(director->camera_data, local_player_index);

    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       local_player_index >= 0 &&
                         local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    director->camera_proc = EDITOR_CAMERA_UPDATE_ENTRY;
    director->debug_input_scale = 1.0f;
    director->debug_controls = false;
  }
}

/* Replay-mode dispatch for one player (0x87050).  reset_flag installs a fresh
 * first-person camera (inlined FUN_000865a0 stores); otherwise mode_flags
 * cycles the replay camera modes.
 *
 * Original receives reset_flag in AL and player index in SI; reverse-thunked
 * to plain cdecl by tools/patch.py. */
void director_apply_replay_mode_for_player(char reset_flag,
                                           int16_t local_player_index,
                                           char mode_flags)
{
  camera_director_t *director;

  if (reset_flag != 0) {
    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       local_player_index >= 0 &&
                         local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    director = &director_globals.local_players[local_player_index];
    first_person_camera_new(director->camera_data);

    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       local_player_index >= 0 &&
                         local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    director->camera_proc = FIRST_PERSON_CAMERA_UPDATE_ENTRY;
    director->debug_input_scale = 1.0f;
    director->debug_controls = false;
    return;
  }

  if (mode_flags != 0)
    FUN_00086a50(local_player_index, director_replay_camera_modes, 4);
}

/* Build the per-player camera_action_t for one tick (0x87110).
 *
 * Branches on whether the player's gamepad is connected:
 *   - Gamepad: the sticks drive facing and translation when the debug
 *     controls are toggled on (digital button 7); the return value comes
 *     from analog button 4's hold count.
 *   - No gamepad: fall back to the debug input state from 0xcf690, which is
 *     stubbed to 0 on Xbox, so that body is dead code on retail — kept
 *     faithful to the binary anyway.
 *
 * Original receives the output-buffer pointer in EAX; reverse-thunked to
 * plain cdecl by tools/patch.py. */
bool director_compute_camera_input(camera_action_t *action,
                                   int local_player_index)
{
  int16_t player16 = (int16_t)local_player_index;
  camera_director_t *director;
  player_data_t *player;
  int16_t gamepad_index;

  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                     player16 >= 0 &&
                       player16 < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  csmemset(action, 0, sizeof(camera_action_t));
  action->local_player_index = player16;
  action->seconds_elapsed = director_globals.dtime;

  director = &director_globals.local_players[player16];

  player = datum_get(
    player_data, local_player_get_player_index(player16));
  gamepad_index = player->local_player_index;

  if (gamepad_index == NONE || input_has_gamepad(gamepad_index) == 0) {
    director_mouse_state_t *mouse = FUN_000cf690();
    if (mouse != NULL) {
      bool back_pressed;

      mouse = FUN_000cf690();
      back_pressed = input_key_is_down(0x1d);

      if ((director->camera_proc != FIRST_PERSON_CAMERA_UPDATE_ENTRY &&
           mouse->field_0d != 0) ||
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

        FUN_00086670(player16, mode_flags, (float)mouse->wheel);

        action->facing_delta.yaw = (float)mouse->delta_x * -0.0031415927f;
        action->facing_delta.pitch = (float)mouse->delta_y * -0.0031415927f;
        action->facing_delta.roll += director->debug_variables[1].delta;
        action->wheel_delta = (float)mouse->wheel;
        action->translation.i += director->debug_variables[2].delta;
        action->translation.j += director->debug_variables[3].delta;
        action->translation.k += director->debug_variables[0].delta;
        action->active = true;

        director_set_local_player_context((int16_t)local_player_index);
        director_inhibit_facing((int16_t)local_player_index);
      }
      return back_pressed == 1;
    }
    return false;
  }

  {
    input_gamepad_state *gamepad =
      input_get_gamepad_state(gamepad_index);
    bool pressed;

    if (director_camera_switch_fast != 0) {
      pressed = (gamepad->analog_hold[4] == 1);
    } else {
      uint8_t held = gamepad->analog_hold[4];
      if (held == 0) {
        pressed = false;
      } else if ((unsigned)held % 30 != 0) {
        pressed = false;
      } else {
        pressed = true;
      }
    }

    if (director->camera_proc == FIRST_PERSON_CAMERA_UPDATE_ENTRY ||
        director->camera_proc == FOLLOWING_CAMERA_UPDATE_ENTRY)
      return pressed;

    if (gamepad->digital_hold[7] == 1)
      director->debug_controls = (director->debug_controls == 0);

    if (director->debug_controls == 0)
      return pressed;

    {
      uint8_t mode_flags = (gamepad->analog_hold[7] != 0) ? 0x10 : 0;
      float wheel;

      if (gamepad->analog_hold[6] != 0)
        mode_flags |= 0x20;

      wheel = (float)((int)((unsigned)(1 < gamepad->digital_hold[0]) -
                            (unsigned)(1 < gamepad->digital_hold[1]))) *
              0.4f;

      action->wheel_delta = wheel;
      FUN_00086670(player16, mode_flags, wheel);

      action->facing_delta.yaw = director_stick_yaw_scale *
        ((float)(int)gamepad->stick_rx * director_globals.dtime);
      action->facing_delta.pitch = director_stick_pitch_scale *
        ((float)(int)gamepad->stick_ry * director_globals.dtime);
      action->translation.i = director_globals.dtime *
        ((float)(int)gamepad->stick_ly * director->debug_input_scale) * 5e-05f;
      action->translation.j = director_globals.dtime *
        ((float)(int)gamepad->stick_lx * director->debug_input_scale) * -5e-05f;

      action->translation.k += director->debug_variables[0].delta;
      action->active = true;

      director_set_local_player_context((int16_t)local_player_index);
      director_inhibit_facing((int16_t)local_player_index);
    }

    return pressed;
  }
}

/*
 * FUN_000874d0 — dispatch the per-player camera setup for the director game
 * mode: 0/1 normal play, 2 editor camera, 4 replay, anything else a no-op.
 *
 * local_player_index@<ecx>, reset_flag@<eax>, mode_flags@<edx>.
 *
 * 0x874d0 / director.obj
 */
void FUN_000874d0(int16_t local_player_index, char reset_flag, char mode_flags)
{
  switch (director_globals.game_mode) {
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
  camera_director_t *director;

  /* director mode: 0 = normal gameplay, 2 = editor (scripted camera) */
  director_globals.game_mode = game_in_editor() ? 2 : 0;
  director_globals.initialize_camera = 0;

  for (i = 0; i < 4; i++) {
    assert_halt_msg_at("local_player_index>=0 && "
                       "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                       "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                       i >= 0 && i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    director = &director_globals.local_players[i];
    director->bored = 0;
    director->bored_time = 0;
    director->camera_change_pause = 0.0f;

    switch (director_globals.game_mode) {
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

  }
}

/* Update all active local players' cameras for this tick: build the camera
 * action, run the mode dispatch, call the active camera proc into a fresh
 * command, then hand the command to the observer.
 *
 * While camera_change_pause runs the command's timer is raised to it; with
 * the first-person camera and less than 0.2s left the pause ends at once and
 * two of the command's per-axis transition slots are reset. */
void director_update(float delta_time)
{
  int i;
  uint8_t mode_flags;
  camera_action_t action;
  camera_command_t command;
  camera_director_t *director;

  director_globals.dtime = delta_time;
  i = 0;
  director = director_globals.local_players;

  do {
    if (local_player_get_player_index((int16_t)i) != NONE) {
      assert_halt_msg_at("local_player_index>=0 && "
                         "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                         "c:\\halo\\SOURCE\\camera\\director.c", 0xb3,
                         (int16_t)i >= 0 &&
                           (int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

      director->inhibited_facing = false;
      director->inhibited_input = false;

      mode_flags =
        (uint8_t)director_compute_camera_input(&action, i);

      {
        uint8_t initialize = director_globals.initialize_camera;
        switch (director_globals.game_mode) {
        case 0:
        case 1:
          director_set_player_camera_normal((int16_t)i, initialize,
                                            mode_flags);
          break;
        case 2:
          director_set_player_camera_scripted((int16_t)i, initialize);
          break;
        case 4:
          director_apply_replay_mode_for_player(initialize, (int16_t)i,
                                                mode_flags);
          break;
        }
      }

      director_globals.initialize_camera = false;

      csmemset(&command, 0, sizeof(camera_command_t));

      {
        director_camera_proc camera_proc =
          (director_camera_proc)director->camera_proc;
        if (camera_proc != NULL) {
          /* The scripted camera only runs for the next active local player. */
          if (director->camera_proc != SCRIPTED_CAMERA_UPDATE_ENTRY ||
              (int16_t)i == local_player_get_next(-1)) {
            camera_proc(director->camera_data, &action, &command);
          }
        }
      }

      if (!(command.flags & FLAG(_observer_command_valid_bit))) {
        director->command.flags &= ~FLAG(_observer_command_valid_bit);
      } else {
        if (director->camera_change_pause != 0.0f) {
          if (0.2f <=
                director->camera_change_pause ||
              director->camera_proc != FIRST_PERSON_CAMERA_UPDATE_ENTRY) {
            if (command.timer <= director->camera_change_pause)
              command.timer = director->camera_change_pause;
          } else {
            director->camera_change_pause = 0.0f;
            command.field_54[0] = 0.0f;
            command.field_4c[0] = 3;
            command.field_54[2] = 0.0f;
            command.field_4c[2] = 3;
          }
          {
            float next = director->camera_change_pause - delta_time;
            director->camera_change_pause = next < 0.0f ? 0.0f : next;
          }
        }
        qmemcpy(&director->command, &command, sizeof(camera_command_t));
      }

      observer_set_camera((int16_t)i, &director->command);
    }

    i++;
    director++;
  } while ((int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
}

/* director_initialize_for_saved_game (0x877e0) — after-load callback in the
 * game_state_revert table at 0x32eaa8.
 *
 * Re-runs the per-map director reset, then re-applies the saved scripted-
 * camera byte (director_camera_scripted) to every local player through
 * director_script_camera. The byte is zero-extended
 * (XOR ECX,ECX; MOV CL,[EAX]) before the single cdecl PUSH. */
void director_initialize_for_saved_game(void)
{
  director_initialize_for_new_map();
  director_script_camera(director_camera_scripted->camera_scripted);
}

/* editor_camera_initialize (0x87800) — initialize the editor camera for one
 * local player.
 *
 * On the first call after a map load the focus is seeded from the first
 * element of the scenario block at +0x354 (zeroed when the block is empty);
 * the scenario is re-fetched for every access, as in the original.  The
 * camera data then gets the focus position and the forward vector of the
 * focus angles.  Local player 0's camera data becomes the live editor
 * camera, and the current mode's _translate_to callback runs on it. */
void editor_camera_initialize(void *camera_data, int16_t local_player_index)
{
  director_scenario_start_t *element;
  real_vector3d forward;

  if (!editor_camera_globals.initialized) {
    if (scenario_block_354(global_scenario_get())->count == 0 ||
        scenario_block_354(global_scenario_get())->address == NULL) {
      csmemset(&editor_camera_globals.focus_position, 0,
               sizeof(real_point3d) + sizeof(real_euler_angles2d));
    } else {
      element = tag_block_get_element(
        scenario_block_354(global_scenario_get()), 0,
        SCENARIO_BLOCK_354_ELEMENT_SIZE);
      editor_camera_globals.focus_position = element->position;
      editor_camera_globals.focus_angles.yaw = element->angles.yaw;
    }
  }
  editor_camera_globals.initialized = true;

  angles_to_vector(&forward.i,
                   &editor_camera_globals.focus_angles.yaw);

  /* 0x89350: cdecl(camera_data, position, forward) — the shared
   * ADD ESP,0x14 also retires angles_to_vector's two arguments. */
  FUN_00089350(camera_data, &editor_camera_globals.focus_position, &forward);

  if (local_player_index == 0) {
    editor_camera_globals.camera = camera_data;
  }

  if (editor_camera_globals.mode != 0) {
    translate_funcs[editor_camera_globals.mode][_translate_to](
      camera_data);
  }
}

/* editor_camera_get_focus (0x878d0) — read back the cached editor/flying
 * camera focus position and angles.
 *
 * Both out-pointers are asserted non-NULL first; the assert strings and line
 * numbers (0x78, 0x79) come from editor_flying_camera.c, so they are spelled
 * explicitly rather than via assert_halt (which would stamp director.c).
 * The reference copies all five dwords with plain MOVs, which the struct
 * assignments produce. */
void editor_camera_get_focus(real_point3d *position, real_euler_angles2d *angles)
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

  *position = editor_camera_globals.focus_position;
  *angles = editor_camera_globals.focus_angles;
}

/* editor_camera_set_focus (0x87950) — store the editor/flying camera focus
 * position and angles.  Mirror of editor_camera_get_focus.
 *
 * Both in-pointers are asserted non-NULL first; the assert strings and line
 * numbers (0x81, 0x82) come from editor_flying_camera.c, so they are spelled
 * explicitly rather than via assert_halt (which would stamp director.c).
 * The reference copies all five dwords with plain MOVs, which the struct
 * assignments produce; the uint32_t pointers are the kb.json prototype. */
void editor_camera_set_focus(const real_point3d *position,
                             const real_euler_angles2d *angles)
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

  editor_camera_globals.focus_position = *position;
  editor_camera_globals.focus_angles = *angles;
}

/* editor_camera_set_position (0x879d0) — push a new focus into the editor/
 * flying camera.  When there is no live editor camera
 * (editor_camera_globals.camera == NULL) the values are parked in the cached
 * focus via editor_camera_set_focus and editor_camera_globals.initialized is
 * raised; otherwise position and facing are written straight into the live
 * camera.
 *
 * Both in-pointers are asserted non-NULL first; the assert strings and line
 * numbers (0x94, 0x95) come from editor_flying_camera.c, so they use
 * assert_halt_at rather than assert_halt (which would stamp director.c).
 *
 * The reference copies every field as a plain dword MOV: the position is one
 * 12-byte struct copy (through a second pointer register) and the facing an
 * 8-byte one, so both are written as struct assignments, never as float
 * loads/stores.  The in-pointers stay uint32_t (kb.json prototype); they
 * point at a real_point3d and a real_euler_angles2d.
 *
 * The live camera pointer is read once into a local and tested; the reference
 * keeps it in EAX for the stores and copies it to ECX for the position copy,
 * which is what the struct assignment through `camera` produces.
 *
 * editor_camera_globals is the kb.json data symbol for the editor camera
 * state block; no raw addresses are used here.
 */
void editor_camera_set_position(const real_point3d *point,
                                const real_euler_angles2d *angles)
{
  flying_camera_t *camera;

  assert_halt_at("c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x94,
                 point);
  assert_halt_at("c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x95,
                 angles);

  camera = editor_camera_globals.camera;
  if (camera == 0) {
    editor_camera_set_focus(point, angles);
    editor_camera_globals.initialized = 1;
    return;
  }

  camera->position = *point;
  camera->facing = *angles;
}

/* FUN_00087ac0 (0x87ac0) — set editor_camera_globals.use_roll and return its
 * previous value (left in AL by the original).  Turning it off levels the
 * live editor camera's roll. */
char FUN_00087ac0(char enabled)
{
  char previous;
  flying_camera_t *camera;

  previous = editor_camera_globals.use_roll;
  editor_camera_globals.use_roll = enabled;

  if (enabled == 0) {
    camera = editor_camera_globals.camera;
    if (camera != NULL) {
      camera->roll = 0.0f;
    }
  }

  return previous;
}

/* editor_camera_set_mode (0x87b00) — switch the editor/flying camera mode,
 * running the per-mode translation callbacks on the way out of the old mode
 * and into the new one.
 *
 * translate_funcs is a [mode][_translate_from/_translate_to] table of
 * callbacks taking the live editor camera.  The callbacks only run when
 * there is a live camera and the mode actually changes; the mode store and
 * the console_printf of the mode name happen on every path.
 *
 * The assert strings and line numbers (0x12e, 0x134) come from
 * editor_flying_camera.c, so they use assert_halt_msg_at rather than
 * assert_halt (which would stamp director.c).
 *
 * The stack parameter is a 16-bit word: the reference reads it with
 * MOV DI,word ptr [EBP+0x8] and stores it back with a word MOV. */
void editor_camera_set_mode(int16_t mode)
{
  if (editor_camera_globals.camera && editor_camera_globals.mode != mode) {
    if (editor_camera_globals.mode) {
      assert_halt_msg_at(
        "translate_funcs[camera_mode][_translate_from]",
        "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x12e,
        translate_funcs[editor_camera_globals.mode][_translate_from]);
      translate_funcs[editor_camera_globals.mode][_translate_from](
        editor_camera_globals.camera);
    }
    if (mode) {
      assert_halt_msg_at("translate_funcs[mode][_translate_to]",
                         "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                         0x134, translate_funcs[mode][_translate_to]);
      translate_funcs[mode][_translate_to](editor_camera_globals.camera);
    }
  }

  editor_camera_globals.mode = mode;
  console_printf(0, editor_camera_mode_names[mode]);
}


/* FUN_00087c00 (0x87c00) — translate_funcs[1][_translate_to]: entering
 * editor camera mode 1, park the caller's flying camera in
 * persisted_cameras[0] and either restore persisted_cameras[1] (when it was
 * saved by FUN_00088200) or reset the camera to a default view.
 *
 * Both whole-camera copies are MOV ECX,0x7 / REP MOVSD struct assignments.
 * On the default path the position y constant is loaded into ECX at 0x87c32
 * before any of the three stores, and the facing is recomputed from the
 * source forward vector; roll and field of view keep the caller's values.
 *
 * The parameter is declared void * to match the kb.json declaration. */
void FUN_00087c00(void *state)
{
  flying_camera_t *camera = state;
  real position_y;

  editor_camera_globals.persisted_cameras[0].camera = *camera;

  if (editor_camera_globals.persisted_cameras[1].saved) {
    *camera = editor_camera_globals.persisted_cameras[1].camera;
    return;
  }

  position_y = editor_camera_default_position_y;
  set_real_point3d_inline(&camera->position, 0.0f, position_y, 0.0f);
  vector_to_angles(&camera->facing.yaw,
                   &editor_camera_source->forward.i);
}

/* editor_camera_move_to_point (0x87c80) — place the editor camera 2.5 units
 * back from `point` along its current facing.  The live camera pointer is
 * re-loaded after angles_to_vector, and each component is
 * point - forward * 2.5 (FMUL then FSUBR, the point is the minuend). */
void editor_camera_move_to_point(const real_point3d *point)
{
  real_vector3d forward;
  flying_camera_t *camera;

  if (point == 0) {
    display_assert("point", "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                   0x8b, 1);
    system_exit(-1);
  }

  angles_to_vector(&forward.i,
                   &editor_camera_globals.camera->facing.yaw);
  camera = editor_camera_globals.camera;
  point_from_line_back3d(point, &forward, 2.5f, &camera->position);
}

/* editor_camera_set_position_and_roll's angles: a facing it hands on as a
 * real_euler_angles2d, then a roll. */
struct editor_camera_angles {
  real_euler_angles2d facing;
  real roll;
};
cs(struct editor_camera_angles, 0x0c);
co(struct editor_camera_angles, roll, 0x08);

/* editor_camera_set_position_and_roll (0x87d00) — like
 * editor_camera_set_position, but angles is {yaw, pitch, roll}: the facing is
 * re-derived from the yaw/pitch/roll matrix and the roll is recomputed from
 * the matrix up vector. */
void editor_camera_set_position_and_roll(const real_point3d *point,
                                         const struct editor_camera_angles *angles)
{
  real_matrix4x3 matrix;
  real_vector3d forward;
  real_vector3d cross;
  real_vector3d pitched;
  real_euler_angles2d facing;

  assert_halt_at("c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0xa9,
                 point);
  assert_halt_at("c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0xaa,
                 angles);

  if (editor_camera_globals.camera == 0) {
    editor_camera_set_focus(point, &angles->facing);
    editor_camera_globals.initialized = 1;
    editor_camera_globals.reset_all = 1;
    return;
  }

  editor_camera_globals.camera->position = *point;
  FUN_00109e90(&matrix.scale, angles->facing.yaw, angles->facing.pitch,
               angles->roll);
  vector_to_angles(&editor_camera_globals.camera->facing.yaw,
                   &matrix.forward.i);
  angles_to_vector(&forward.i,
                   &editor_camera_globals.camera->facing.yaw);
  facing = editor_camera_globals.camera->facing;
  facing.pitch = facing.pitch + k_half_pi;
  angles_to_vector(&pitched.i, &facing.yaw);
  normalize3d(&forward.i);
  normalize3d(&pitched.i);
  cross_product3d_inline(&pitched, &matrix.up, &cross);
  normalize3d(&cross.i);
  editor_camera_globals.camera->roll =
    FUN_0010c510(&pitched.i, &matrix.up.i) * dot_product3d(&cross, &forward);
  if (editor_camera_relative_object_index != NONE) {
    set_real_vector3d_inline(&editor_camera_globals.unit_offset, point->x,
                             point->y, point->z);
  }
  editor_camera_globals.reset_all = 1;
}

/* FUN_00087eb0 (0x87eb0) — record the editor camera relative object and
 * derive editor_camera_globals.unit_offset.
 *
 * param_1 is stored unconditionally (MOV [0x2ee66c],EAX at 0x87ebf).  With a
 * live editor camera and a handle other than -1, the offset is the camera
 * position minus the object's bounding_sphere_center (FLD [ESI],
 * FSUB [EAX+0x50]).  For a -1 handle the zero vector is copied as three
 * dwords.  The object is reached through a real_point3d pointer to its
 * bounding_sphere_center; caching the object pointer instead loses 7 bytes. */
void FUN_00087eb0(int32_t object_index)
{
  flying_camera_t *camera;
  object_data_t *object;
  const real_vector3d *source;

  camera = editor_camera_globals.camera;
  editor_camera_relative_object_index = object_index;
  if (camera != NULL) {
    if (object_index != NONE) {
      object = object_get_and_verify_type(object_index, -1);
      vector_from_points3d(&object->bounding_sphere_center, &camera->position,
                           &editor_camera_globals.unit_offset);
      return;
    }
    source = global_zero_vector3d;
    editor_camera_globals.unit_offset = *source;
  }
}

/* editor_camera_update (0x87f20) — per-mode editor camera update entry stored
 * in the update_funcs table (the only xref is the DATA store at 0x87026).
 *
 * Ghidra types this void(void) and reports the three stack parameters as
 * in_stack_00000004/8/c; the disassembly reads [EBP+8], [EBP+0xc] (EDI) and
 * [EBP+0x10] (ESI), so the function really takes three cdecl parameters and
 * forwards all three to the current mode's update_funcs entry.
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
void editor_camera_update(void *camera_data, camera_action_t *action,
                          camera_command_t *command)
{
  flying_camera_t *camera;
  editor_camera_source_t *source;

  if (update_funcs[editor_camera_globals.mode] == NULL) {
    display_assert("update_funcs[camera_mode]",
                   "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x154,
                   1);
    system_exit(-1);
  }

  if (editor_camera_globals.scripted) {
    if (!action->active) {
      scripted_camera_update(0, action, command);
      return;
    }

    camera = editor_camera_globals.camera;
    source = editor_camera_source;
    camera->position = source->position;
    vector_to_angles(&camera->facing.yaw,
                     &editor_camera_source->forward.i);
    FUN_00087eb0(editor_camera_relative_object_index);

    if (editor_camera_globals.mode != 0) {
      if (translate_funcs[editor_camera_globals.mode][_translate_from] ==
          NULL) {
        display_assert("translate_funcs[camera_mode][_translate_from]",
                       "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                       0x164, 1);
        system_exit(-1);
      }
      translate_funcs[editor_camera_globals.mode][_translate_to](
        editor_camera_globals.camera);
    }
  }

  update_funcs[editor_camera_globals.mode](camera_data, action,
                                           command);

  if (editor_camera_globals.scripted) {
    command->timer = 0.0f;
    command->flags |= FLAG(_observer_command_valid_bit) |
                      FLAG(_observer_command_force_time_bit);
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
  real_euler_angles2d facing;
  editor_camera_source_t *source;
  flying_camera_t *camera;
  const real_point3d *position;
  const char *message;
  char previous;
  int32_t relative_object_index;

  if (enable != 0) {
    /* The mode word is read at each use: the original reloads AX after the
     * assert call and sign-extends it again for the dispatch. */
    if (editor_camera_globals.mode != 0) {
      if (translate_funcs[editor_camera_globals.mode][_translate_from] ==
          NULL) {
        display_assert("translate_funcs[camera_mode][_translate_from]",
                       "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                       0x183, 1);
        system_exit(-1);
      }
      translate_funcs[editor_camera_globals.mode][_translate_from](
        editor_camera_globals.camera);
    }

    vector_to_angles(&facing.yaw,
                     &editor_camera_source->forward.i);
    source = editor_camera_source;
    editor_camera_set_position(&source->position, &facing);

    /* JZ 0x8810e at 0x880e5: the fall-through (relative object present) is
     * the unit_offset path, so the != test comes first. */
    relative_object_index = editor_camera_relative_object_index;
    if (relative_object_index != NONE) {
      source = editor_camera_source;
      scripted_camera_set_camera_point_relative(
        &editor_camera_globals.unit_offset.i, &source->forward.i,
        &source->up.i, 1.2217305f, 0, relative_object_index);
    } else {
      source = editor_camera_source;
      scripted_camera_set_camera_point_relative(
        &source->position.x, &source->forward.i,
        &source->up.i, 1.2217305f, 0, NONE);
    }
  } else {
    /* ADD EAX,0x10 / MOV ECX,EAX at 0x8813a: source->position is held in
     * one register and the three dwords are read through it. */
    camera = editor_camera_globals.camera;
    position = &editor_camera_source->position;
    camera->position = *position;

    vector_to_angles(&camera->facing.yaw,
                     &editor_camera_source->forward.i);
    FUN_00087eb0(editor_camera_relative_object_index);

    if (editor_camera_globals.mode != 0) {
      if (translate_funcs[editor_camera_globals.mode][_translate_from] ==
          NULL) {
        display_assert("translate_funcs[camera_mode][_translate_from]",
                       "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c",
                       0x19e, 1);
        system_exit(-1);
      }
      translate_funcs[editor_camera_globals.mode][_translate_to](
        editor_camera_globals.camera);
    }
  }

  /* The original loads the old flag and the message pointer before it
   * writes last_scripted / scripted (stores at 0x881e3 / 0x881e9 sit after
   * the three console_printf pushes). */
  previous = editor_camera_globals.scripted;
  message = editor_camera_scripted_messages[enable];
  editor_camera_globals.last_scripted = previous;
  editor_camera_globals.scripted = (char)enable;
  console_printf(0, "%s scripted camera mode", message);
}

/* FUN_00088200 (0x88200) — translate_funcs[1][_translate_from]: leaving
 * editor camera mode 1, save the caller's flying camera in
 * persisted_cameras[1] (marking it saved), then move the camera to the
 * source position and forward vector.
 *
 * The save is a MOV ECX,0x7 / REP MOVSD struct assignment (same shape as
 * FUN_00087c00).  The original holds source+0x10 in ECX for the three
 * position dwords and re-loads the source pointer for the forward vector;
 * roll and field of view keep the caller's values.  One ADD ESP,0xc cleans
 * the vector_to_angles and FUN_00087eb0 pushes. */
void FUN_00088200(void *state)
{
  flying_camera_t *camera = state;
  editor_camera_source_t *source;

  editor_camera_globals.persisted_cameras[1].camera = *camera;
  editor_camera_globals.persisted_cameras[1].saved = true;

  source = editor_camera_source;
  camera->position = source->position;
  vector_to_angles(&camera->facing.yaw,
                   &editor_camera_source->forward.i);
  FUN_00087eb0(editor_camera_relative_object_index);
}

/* editor_camera_orbiting_update (0x887e0) — orbiting-mode entry of the
 * editor camera update_funcs table; cdecl (camera_data [EBP+8],
 * control_data [EBP+0xc], result_data [EBP+0x10]).
 *
 * The orbit reuses the editor flying camera: its facing is the orbit angle
 * pair and its position.y holds the orbit distance (FUN_00087c00 seeds it).
 *
 * The validator reads field_of_view and timer back from the constant pools
 * (FLD [0x2670c8] at 0x88b0c, [0x2670d0] at 0x88b49), not from the result
 * block, so those reads are kept on the pools.  The distance clamp (JZ at
 * 0x88882) also clamps NaN; the pitch clamp (JP at 0x88839, JNZ at 0x88850)
 * leaves NaN unclamped. */
void editor_camera_orbiting_update(void *camera_data, void *control_data,
                                   void *result_data)
{
  flying_camera_t *camera;
  camera_action_t *action;
  camera_command_t *result;
  player_control_unit_camera_info_t info;
  real pitch;
  real distance;

  camera = camera_data;
  action = control_data;
  result = result_data;

  player_control_get_unit_camera_info(action->local_player_index, &info);
  result->position = info.position;
  if (action->active) {
    camera->facing.yaw = action->facing_delta.yaw + camera->facing.yaw;
    pitch = action->facing_delta.pitch + camera->facing.pitch;
    if (pitch < -1.2566371f) {
      pitch = -1.2566371f;
    } else if (pitch > 1.2566371f) {
      pitch = 1.2566371f;
    }
    camera->facing.pitch = pitch;
    director_set_local_player_context(action->local_player_index);
  }

  distance =
    camera->position.y - action->wheel_delta * 0.33333334f;
  if (!(distance > 0.6f)) {
    distance = 0.6f;
  }
  camera->position.y = distance;

  if (info.unit_index != NONE) {
    angles_to_vector(&result->forward.i, &camera->facing.yaw);
    observer_up_from_forward(&result->forward, &result->up);
    object_get_root_location(info.unit_index, &result->velocity.i, NULL);
    result->flags = FLAG(_observer_command_valid_bit);
  }

  result->offset = *global_zero_vector3d;
  result->depth = camera->position.y;
  result->field_of_view = editor_orbit_field_of_view;
  result->timer = editor_orbit_timer;

  if (!director_camera_command_valid(result, editor_orbit_field_of_view,
                                     editor_orbit_timer)) {
    display_assert(
      csprintf(error_string_buffer, CAMERA_COMMAND_INVALID_FORMAT,
               CAMERA_COMMAND_INVALID_ARGUMENTS_AT(
                 result, editor_orbit_field_of_view,
                 editor_orbit_timer)),
      "c:\\halo\\SOURCE\\camera\\editor_flying_camera.c", 0x23b, true);
    system_exit(-1);
  }
}
