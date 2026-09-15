#ifdef HALO_RNG_TRACE
#include "halo/math/rng_trace.h"
#endif

/* memset as a compiler intrinsic (not the csmemset helper): the 0xb4490
 * reference fills flag_indices with an inline store sequence, not a call. */
extern void *__cdecl memset(void *, int, unsigned int);
#if defined(_MSC_VER) && !defined(__clang__)
#pragma intrinsic(memset)
#else
#define memset __builtin_memset
#endif
#line 1
void FUN_000a6a80(void)
{
  /* One contiguous placement buffer (matches original frame at EBP-0x114).
     FUN_000a6930 reads tag indices from base+0xC with a 0x10-byte stride, so
     the header (12 bytes) and the entry array must share one allocation.
     Ghidra split this into local_118[12] + aiStack_10c[61]; declaring them as
     two separate locals let clang place them non-adjacently, so the helper read
     uninitialized stack (garbage tag index 0xf3c90060 -> cache_files.c:475
     assert on the cheat_all_weapons path). */
  char local_118[256];
  char local_18[20];
  int *piVar1;
  int iVar2;
  void *uVar3;
  int iVar4;

  piVar1 = (int *)((char *)game_globals_get() + 0x14c);
  if (*piVar1 != 0) {
    iVar2 = (int)tag_block_get_element(piVar1, 0, 0x10);
    if (iVar2 != 0) {
      if (*piVar1 == 0) {
        FUN_000a6930(0, *(unsigned short *)piVar1);
        return;
      }
      uVar3 = tag_block_get_element(piVar1, 0, 0x10);
      FUN_000a6930((int)uVar3, *(unsigned short *)piVar1);
      return;
    }
  }
  iVar2 = 0;
  FUN_001b9b60((int)local_18, 0x77656170);
  iVar4 = FUN_001b9b80((int)local_18);
  while (iVar4 != -1 && (unsigned short)iVar2 < 0x10) {
    *(int *)(local_118 + 0xc + (short)(unsigned short)iVar2 * 0x10) = iVar4;
    iVar2 = iVar2 + 1;
    iVar4 = FUN_001b9b80((int)local_18);
  }
  FUN_000a6930((int)local_118, iVar2);
  return;
}

void FUN_000a6b50(void)
{
  int *block;
  void *element;

  block = (int *)((char *)game_globals_get() + 0x158);
  if (*block != 0) {
    element = tag_block_get_element(block, 0, 0x10);
    FUN_000a6930((int)element, *(unsigned short *)block);
    return;
  }
  FUN_000a6930(0, *(unsigned short *)block);
}

void FUN_000a6ba0(void)
{
  int *block;
  int index;

  block = (int *)((char *)game_globals_get() + 0x164);
  if (*block != 0) {
    index =
      (int)*(short *)((char *)tag_block_get_element(block, 0, 0xa0) + 0x20);
    FUN_000a6930(*(int *)((char *)tag_block_get_element(block, 0, 0xa0) + 0x24),
                 (unsigned short)index);
  }
}

void game_initialize(void)
{
  game_globals = (game_globals_t *)game_state_malloc("game globals", 0,
                                                     sizeof(game_globals_t));
  csmemset(game_globals, 0, sizeof(game_globals_t));
  csmemset(&game_variant_global, 0, sizeof(game_variant_global));
  real_math_reset_precision();
  game_time_initialize();
  game_engine_initialize(&game_variant_global);
  game_allegiance_initialize();
  interface_initialize();
  scenario_initialize();
  director_initialize();
  observer_initialize();
  render_initialize();
  objects_initialize();
  structures_initialize();
  breakable_surfaces_initialize();
  decals_initialize();
  collision_log_initialize();
  players_initialize();
  contrails_initialize();
  particles_initialize();
  effects_initialize();
  weather_particle_systems_initialize();
  particle_systems_initialize();
  sound_classes_initialize();
  game_sound_initialize();
  rumble_initialize();
  player_effect_initialize();
  ai_initialize();
  editor_initialize();
  ui_widgets_initialize();
  hs_initialize();
  recorded_animations_initialize();
  cheats_initialize();
  transport_initialize();
  telnet_console_initialize();
  initialize_network_game_packets();
  cinematic_initialize();
  saved_game_files_initialize();
  event_manager_initialize();
  input_abstraction_initialize();
  player_ui_initialize();
  bink_playback_initialize();
  progress_bar_initialize();
}

void game_dispose(void)
{
  recorded_animations_dispose();
  cinematic_dispose();
  hs_dispose();
  cheats_dispose();
  ui_widgets_dispose();
  editor_dispose();
  ai_dispose();
  player_effect_dispose();
  rumble_dispose();
  game_sound_dispose();
  sound_classes_dispose();
  game_engine_dispose();
  particle_systems_dispose();
  weather_particle_systems_dispose();
  effects_dispose();
  particles_dispose();
  contrails_dispose();
  players_dispose();
  decals_dispose();
  breakable_surfaces_dispose();
  structures_dispose();
  render_dispose();
  objects_dispose();
  director_dispose();
  interface_dispose();
  game_allegiance_dispose();
  game_time_dispose();
  saved_game_files_dispose();
  event_manager_dispose();
  input_abstraction_dispose();
  player_ui_dispose();
  game_state_dispose();
  telnet_console_dispose();
  transport_dispose();
  bink_playback_dispose();
  progress_bar_dispose();
}

void game_precache_new_map(char *map_name, bool a2)
{
  __int16 map_status;

  if (cache_files_precache_map_loaded(map_name)) {
  LABEL_22:
    if (a2) {
      assert_halt(cache_files_precache_map_loaded(map_name));
      main_save_current_solo_map(map_name);
      main_queue_map_name(NULL);

      if (cache_files_precache_in_progress())
        cache_files_precache_map_end();

      if (player_spawn_count == 1)
        player_ui_remember_player1_profile(true);
    }
    return;
  }

  if (cache_files_precache_in_progress() &&
      !cache_files_precache_is_copying_map(map_name)) {
    if (a2) {
      cache_files_precache_map_end();
    } else {
      cache_files_precache_map_queue_end();
      main_queue_map_name(map_name);
    }
  }

  if (!cache_files_precache_in_progress() &&
      !cache_files_precache_map_begin(map_name, a2)) {
    error(2, "shouldn't be here... map '%s' doesn't exist", map_name);
    if (a2) {
      display_assert("read the last error message for which map failed to load",
                     __FILE__, __LINE__, true);
      system_exit(-1);
    }
  }

  cache_files_precache_set_priority(a2);

  if (a2) {
    game_globals->map_loading = true;
    game_globals->map_load_progress = 0.0f;
    assert_halt(cache_files_precache_in_progress() &&
                cache_files_precache_is_copying_map(map_name));
    ui_widget_load_progress_widget();
    progress_bar_begin(global_scenario_index != -1);

    do {
      map_status =
        cache_files_precache_map_status(&game_globals->map_load_progress);
      main_pregame_render();
      main_rasterizer_throttle();
      main_present_frame();
    } while (!map_status);

    progress_bar_end();
    ui_widgets_close_all();

    if (map_status == 2)
      display_error_damaged_media();

    cache_files_precache_map_end();
    assert_halt(cache_files_precache_map_loaded(map_name));

    game_globals->map_loading = false;
    game_globals->map_load_progress = 1.0f;

    goto LABEL_22;
  }
}

bool game_map_loading_in_progress(float *progress)
{
  if (progress) {
    *progress = game_globals->map_load_progress;
  }
  return game_globals->map_loading;
}

void game_unload(void)
{
  __int16 map_status;

  if (cache_files_precache_in_progress()) {
    game_globals->map_loading = true;
    ui_widget_load_progress_widget();

    do {
      map_status =
        cache_files_precache_map_status(&game_globals->map_load_progress);
      main_pregame_render();
      main_rasterizer_throttle();
      main_present_frame();
    } while (!map_status);

    ui_widgets_close_all();
    if (map_status == 2)
      display_error_damaged_media();
    cache_files_precache_map_end();
  }
  if (game_globals->map_loaded) {
    scenario_unload();
    random_seed_debug_log(0);
    game_globals->map_loaded = false;
  }
}

void game_dispose_from_old_map()
{
  rasterizer_dispose_from_old_map();
  game_state_dispose_from_old_map();
  cheats_dispose_from_old_map();
  recorded_animations_dispose_from_old_map();
  hs_dispose_from_old_map();
  cinematic_dispose_from_old_map();
  editor_dispose_from_old_map();
  ai_dispose_from_old_map();
  player_effect_dispose_from_old_map();
  rumble_dispose_from_old_map();
  point_physics_dispose_from_old_map();
  particle_systems_dispose_from_old_map();
  weather_particle_systems_dispose_from_old_map();
  decals_dispose_from_old_map();
  breakable_surfaces_dispose_from_old_map();
  structures_dispose_from_old_map();
  j__render_dispose_from_old_map();
  objects_dispose_from_old_map();
  director_dispose_from_old_map();
  observer_dispose_from_old_map();
  interface_dispose_from_old_map();
  players_dispose_from_old_map();
  contrails_dispose_from_old_map();
  particles_dispose_from_old_map();
  effects_dispose_from_old_map();
  game_sound_dispose_from_old_map();
  sound_classes_dispose_from_old_map();
  sound_dispose_from_old_map();
  game_allegiance_dispose_from_old_map();
  update_server_delete();
  game_engine_dispose_from_old_map();
  scenario_dispose_from_old_map();
  game_time_dispose_from_old_map();
  ui_widgets_close_all();
  ui_widgets_safe_to_load(0);
  game_globals->active = 0;
}

void game_frame(float elapsed)
{
  if (game_globals->players_double_speed)
    elapsed *= 0.5f;

  assert_halt(game_globals->active);

  collision_log_begin_period(1);
  particles_update(elapsed);
  contrails_update(elapsed);
  particle_systems_update(elapsed);
  widgets_update(elapsed);
  game_sound_update(elapsed);
  scenario_frame_update(elapsed);
  rasterizer_frame_update(elapsed);
  numeric_countdown_timer_update();
  collision_log_end_period();
}

void remove_quitting_players_from_game(void)
{
  data_iter_t iter;
  char *player;
  int quit_tick;
  int quit_at;

  if (!game_engine_running())
    return;

  quit_tick = game_time_get();
  data_iterator_new(&iter, player_data);
  while ((player = (char *)data_iterator_next(&iter)) != NULL) {
    quit_at = *(int *)(player + 0xCC);
    if (quit_at != -1 && *(char *)(player + 0xD1) == 0) {
      if (quit_tick == quit_at) {
        *(char *)(player + 0xD1) = 1;
        if (*(int *)(player + 0x34) != -1) {
          object_get_and_verify_type(*(int *)(player + 0x34), 3);
          unit_delete(*(int *)(player + 0x34));
        }
      } else if (quit_at < quit_tick) {
        error(2, "player %x failed to quit, wanted %d is %d", iter.datum_handle,
              quit_at, quit_tick);
      }
    }
  }
}

void game_tick(void)
{
  float seconds_per_tick;

  profile_tick_start();
  collision_log_begin_period(0);
  real_math_reset_precision();
  if (profile_global_enable && byte_2EF808)
    profile_enter_private(&off_2EF800);

  assert_halt(game_globals->active);

  remove_quitting_players_from_game();
  game_allegiance_update();
  units_update();
  ai_update();
  players_update_before_game();

  seconds_per_tick = 1 / 30.0f;
  if (game_globals->players_double_speed)
    seconds_per_tick = 1 / 60.0f;

  effects_update(seconds_per_tick);
  lock_global_random_seed();
  rumble_update();
  first_person_weapons_update();
  unlock_global_random_seed();
  game_engine_update();
  editor_update();
  hs_update();
  recorded_animations_update();
  objects_update();
  players_update_after_game();
  hud_update();
  player_effect_update();
  if (profile_global_enable && byte_2EF808)
    profile_exit_private(&off_2EF800);
  collision_log_end_period();
  profile_tick_end();
}

void game_options_new(game_options_t *options)
{
  csmemset(options, 0, sizeof(*options));
  options->unk_4 = 0;
  options->difficulty = 1;
  options->random_seed = 0xDEADBEEF;
}

bool game_options_verify(game_options_t *options)
{
  if (options->difficulty >= 0 && options->difficulty < 4)
    return 1;
  return 0;
}

void game_initial_pulse()
{
  data_iter_t iter; // [esp+0h] [ebp-10h] BYREF

  data_iterator_new(&iter, player_data);
  while (data_iterator_next(&iter))
    game_engine_player_added(iter.datum_handle);
  game_engine_game_starting();
}

void game_set_players_are_double_speed(bool is_double_speed)
{
  game_globals->players_double_speed = is_double_speed;
}

bool game_players_are_double_speed(void)
{
  return game_globals->players_double_speed;
}

void game_difficulty_level_set(int16_t difficulty)
{
  game_globals->game_options.difficulty = difficulty;
}

int16_t game_difficulty_level_get(void)
{
  return game_globals->game_options.difficulty;
}

/* 0xa7470 — difficulty level with "easy" folded up to "normal".
 *
 * Reference (0xa7470-0xa7487, 7 instructions, single exit):
 *   MOV EAX,[0x004566ec]      game_globals
 *   MOV AX,word ptr [EAX+0xe] game_options.difficulty (0x8 + 0x6), int16
 *   CMP AX,0x1                16-bit SIGNED compare against 1
 *   MOVSX EAX,AX              widen the field for the taken path (flags kept)
 *   JG  0x000a7487            difficulty > 1 -> return it as-is
 *   MOV EAX,0x1               else clamp to 1; full 32-bit write => int return
 *   RET
 *
 * The MOVSX/MOV EAX,0x1 pair is why the return type is int, not int16_t: the
 * value is materialised at full width in EAX on both paths.
 */
int game_difficulty_level_get_ignore_easy(void)
{
  int16_t difficulty;

  difficulty = game_globals->game_options.difficulty;

  return difficulty > 1 ? difficulty : 1;
}

void game_set_game_variant(game_variant_t *variant)
{
  if (!variant)
    csmemset(&game_variant_global, 0, sizeof(game_variant_t));
  else
    qmemcpy(&game_variant_global, variant, sizeof(game_variant_t));
}

void game_set_game_engine_index(void)
{
  display_assert(
    "this is broken and should get updated for the variants, ask michael",
    __FILE__, __LINE__, true);
  system_exit(-1);
}

bool game_all_quiet(void)
{
  return (char)!(dangerous_projectiles_near_player() ||
                 dangerous_items_near_player() ||
                 dangerous_effects_near_player() ||
                 any_unit_is_dangerous() ||
                 ai_enemies_can_see_player());
}

bool game_safe_to_save(void)
{
  bool safe = true;

  if (ai_enemies_can_see_player()) {
    if (debug_game_save) {
      console_warning("not safe to save: ai_enemies_can_see_player");
    }
    safe = false;
  } else if (dangerous_projectiles_near_player()) {
    if (debug_game_save) {
      console_warning("not safe to save: dangerous_projectiles_near_player");
    }
    safe = false;
  } else if (dangerous_items_near_player()) {
    if (debug_game_save) {
      console_warning("not safe to save: dangerous_items_near_player");
    }
    safe = false;
  } else if (dangerous_effects_near_player()) {
    if (debug_game_save) {
      console_warning("not safe to save: dangerous_effects_near_player");
    }
    safe = false;
  } else if (any_unit_is_dangerous()) {
    if (debug_game_save) {
      console_warning("not safe to save: any_unit_is_dangerous");
    }
    safe = false;
  } else if (any_player_is_in_the_air()) {
    if (debug_game_save) {
      console_warning("not safe to save: any_player_is_in_the_air");
    }
    safe = false;
  } else if (any_player_is_dead()) {
    if (debug_game_save) {
      console_warning("not safe to save: any_player_is_dead");
    }
    safe = false;
  } else if (vehicle_moving_near_any_player()) {
    if (debug_game_save) {
      console_warning("not safe to save: vehicle_moving_near_any_player");
    }
    safe = false;
  }

  return safe;
}

bool game_safe_to_speak(void)
{
  return !dangerous_projectiles_near_player() && !any_player_is_dead();
}

bool game_is_cooperative(void)
{
  return player_spawn_count > 1;
}
void set_random_seed(int seed)
{
#ifdef HALO_RNG_TRACE
  RNG_TRACE(RNG_TRACE_GLOBAL_SEED_ADDR, RNG_TRACE_KIND_SET_SEED, seed);
#endif
#line 560
  *get_global_random_seed_address() = seed;
}
#line 562
bool game_load(game_options_t *options)
{
  game_globals_t *globals;
  bool loaded;

  assert_halt(!game_globals->active);
  assert_halt(!game_globals->map_loaded);
  assert_halt(game_options_verify(options));

  random_seed_debug_log(1);
  csmemcpy(&game_globals->game_options, options, sizeof(*options));

  loaded = scenario_load(options->map_name);
  globals = game_globals;

  if (loaded) {
    globals->map_loaded = true;
  }

  return globals->map_loaded;
}

void game_initialize_for_new_map(void)
{
  int random_seed;

  assert_halt(game_globals->map_loaded);
  assert_halt(!game_globals->active);

  random_seed = game_globals->game_options.random_seed;
#ifdef HALO_RNG_TRACE
  RNG_TRACE(RNG_TRACE_GLOBAL_SEED_ADDR, RNG_TRACE_KIND_MAP_SEED, random_seed);
#endif
#line 592
  *get_global_random_seed_address() = random_seed;
  game_engine_dispose();
  game_engine_initialize(&game_variant_global);
  real_math_reset_precision();
  rasterizer_initialize_for_new_map();
  game_state_initialize_for_new_map();
  game_time_initialize_for_new_map();
  interface_initialize_for_new_map();
  game_allegiance_initialize_for_new_map();
  players_initialize_for_new_map();
  scenario_initialize_for_new_map();
  objects_initialize_for_new_map();
  render_initialize_for_new_map();
  structures_initialize_for_new_map();
  breakable_surfaces_initialize_for_new_map();
  decals_initialize_for_new_map();
  director_initialize_for_new_map();
  observer_initialize_for_new_map();
  contrails_initialize_for_new_map();
  particles_initialize_for_new_map();
  effects_initialize_for_new_map();
  particle_systems_initialize_for_new_map();
  sound_initialize_for_new_map();
  sound_classes_initialize_for_new_map();
  game_sound_initialize_for_new_map();
  weather_particle_systems_initialize_for_new_map();
  point_physics_initialize_for_new_map();
  game_engine_initialize_for_new_map();
  game_statistics_start();
  update_server_new();
  player_control_initialize_for_new_map();
  rumble_initialize_for_new_map();
  player_effect_initialize_for_new_map();
  ai_initialize_for_new_map();
  console_initialize_for_new_map();
  editor_initialize_for_new_map();
  cinematic_initialize_for_new_map();
  hs_initialize_for_new_map();
  recorded_animations_initialize_for_new_map();
  cheats_initialize_for_new_map();
  game_globals->active = 1;
  objects_place();
  if (!game_in_editor()) {
    ai_place();
  }
  ui_widgets_safe_to_load(1);
}

void game_set_game_variant_from_name(const char *name)
{
  game_variant_t variant;
  game_variant_t variant_copy;

  // The original at 0xa78e0 has a dead `if (!&variant_copy)` branch from a
  // LEA+TEST+JNZ pattern on a stack address — unreachable in practice, and
  // inexpressible in C. We preserve the two-step copy via variant_copy but
  // skip the dead zero-out branch.
  qmemcpy(&variant_copy, game_engine_get_variant_by_name(&variant, name),
          sizeof(game_variant_t));
  qmemcpy(&game_variant_global, &variant_copy, sizeof(game_variant_t));
}

/* FUN_000b4170 (0xb4170) — race/team score computation.
 * Given a player handle and a mode flag, computes a score:
 *   mode 1: returns a per-team value from the table at 0x456f98.
 *   otherwise: counts set bits in the per-team bitmask at 0x456f54
 *              (8 bits per loop iteration, 4 iterations for all 32 bits),
 *              then returns player->c2 * 0x21 + bit_count. */
int FUN_000b4170(int player_handle, int param_2)
{
  int iVar1;
  unsigned int uVar2;
  unsigned char bVar3;
  int iVar5;
  int iVar6;
  char *player;
  int team_index;

  player = (char *)datum_get(player_data, player_handle);
  team_index = *(int *)(player + 0x20);
  if (param_2 == 1) {
    return ((int *)0x456f98)[team_index];
  }
  uVar2 = ((unsigned int *)0x456f54)[team_index];
  iVar6 = 0;
  iVar5 = 2;
  do {
    bVar3 = (unsigned char)iVar5;
    if ((uVar2 & (1 << (bVar3 - 2))) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << (bVar3 - 1))) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << bVar3)) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << (bVar3 + 1))) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << (bVar3 + 2))) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << (bVar3 + 3))) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << (bVar3 + 4))) != 0)
      iVar6 = iVar6 + 1;
    if ((uVar2 & (1 << (bVar3 + 5))) != 0)
      iVar6 = iVar6 + 1;
    iVar1 = iVar5 + 6;
    iVar5 = iVar5 + 8;
  } while (iVar1 < 0x20);
  return (int)*(int16_t *)(player + 0xc2) * 0x21 + iVar6;
}

/* FUN_000b4250 (0xb4250) — race score string
 *
 * Formats the player's race score (lap/flag count) into a wide string buffer.
 * Looks up the player datum by handle, reads the int16 score at offset 0xc2,
 * and formats it via usprintf with the format string at 0x26c118 (likely
 * L"%d"). */
wchar_t *FUN_000b4250(int player_handle, wchar_t *dst)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  usprintf(dst, (const wchar_t *)0x26c118, (int)*(int16_t *)(player + 0xc2));
  return dst;
}

/* FUN_000b4290 (0xb4290) — race score header string
 *
 * Returns the header label for the race score column.
 * If the game variant modifier (offset 0x4c) is 2 (flags mode), returns
 * L"Flags". Otherwise returns L"Laps". */
wchar_t *FUN_000b4290(wchar_t *dst)
{
  void *variant;

  variant = game_engine_get_variant();
  if (*(int *)((char *)variant + 0x4c) == 2) {
    usprintf(dst, L"Flags");
    return dst;
  }
  usprintf(dst, L"Laps");
  return dst;
}

/* FUN_000b42d0 (0xb42d0) — race score format by position index
 *
 * Formats the race score at 0x456f98[param_1] into dst using the
 * format string pointer at 0x26c118. Returns dst. */
wchar_t *FUN_000b42d0(int param_1, wchar_t *dst)
{
  usprintf(dst, (const wchar_t *)0x26c118, ((int *)0x456f98)[param_1]);
  return dst;
}

/* FUN_000b4490 (0xb4490) — race engine: place the race flag objects.
 *
 * Walks the player table (0x5aa6d4). For each player it fetches the player's
 * unit (player+0x34, type mask 3) and asks FUN_000b43b0 for the next race
 * flag index, passing the unit's position (unit+0xc) or NULL and the array of
 * flags already chosen in this pass. Collection stops after 8 entries or when
 * FUN_000b43b0 returns -1. Each collected index then names a netgame flag in
 * the scenario block at scenario+0x378 (0x94-byte elements), and a flag object
 * is spawned at that flag's position with a facing vector built from the
 * flag's facing angle. 0x456fdc (cleared by FUN_000b4960) is set to 1 for
 * every object actually created.
 *
 * FUN_000b43b0 signature: two stack pushes at 0xb44f7/0xb4501 (position-or-
 * NULL, then &flag_indices) and the result is compared against -1 at 0xb450f.
 *
 * Source: c:\halo\SOURCE\game\game_engine_race.c */
void FUN_000b4490(void)
{
  char *scenario;
  char *player;
  char *unit;
  char *volatile flags_block;
  netgame_flag *flag;
  data_iter_t iter;
  char placement[0x88];
  int flag_indices[8];
  int count;
  int flag_index;
  int tag_index;
  int i;

  scenario = (char *)global_scenario_get();

  flag_indices[0] = -1;
  memset(&flag_indices[1], 0, 7 * sizeof(int));

  count = 0;
  data_iterator_new(&iter, player_data);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    if (*(int *)(player + 0x34) != -1)
      unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
    else
      unit = (char *)0;

    if (count == 8)
      break;

    if (unit != (char *)0)
      flag_index = FUN_000b43b0(unit + 0xc, flag_indices);
    else
      flag_index = FUN_000b43b0((void *)0, flag_indices);

    if (flag_index == -1)
      break;

    flag_indices[count] = flag_index;
    count++;
    player = (char *)data_iterator_next(&iter);
  }

  i = 0;
  if (count > 0) {
    flags_block = scenario + 0x378;
    do {
      flag = (netgame_flag *)tag_block_get_element(flags_block, flag_indices[i],
                                                   0x94);
      tag_index = FUN_000b3770(i);
      if (tag_index != -1) {
        object_placement_data_new(placement, tag_index, -1);
        *(int *)(placement + 0x18) = *(int *)&flag->position_x;
        *(int *)(placement + 0x1c) = *(int *)&flag->position_y;
        *(int *)(placement + 0x20) = *(int *)&flag->position_z;
        vector3d_from_angle((float *)(placement + 0x34), flag->facing);
        object_new(placement);
        *(char *)0x456fdc = 1;
      }
      i++;
    } while (i < count);
  }
}

/* FUN_000b45c0 (0xb45c0) — race engine: pick random flag.
 *
 * Counts the number of valid race flags from the bitmask at 0x456f10.
 * If param_1 != -1, subtracts 1 (excluding the current flag).
 * Then picks a random index from that count and iterates through the
 * scenario netgame flags (type == 3, i.e. race) to find the nth matching
 * flag whose team != param_1.
 *
 * Source: c:\halo\SOURCE\game\game_engine_race.c */
int FUN_000b45c0(int param_1)
{
  short sVar1;
  int iVar2;
  int count;
  int iVar4;
  int iVar5;
  int *piVar6;
  int local_8;
  int flag_element;

  local_8 = -1;
  iVar2 = (int)global_scenario_get();
  count = 0;
  iVar4 = 0;
  do {
    if ((*(int *)0x456f10 & (1 << iVar4)) != 0) {
      count = count + 1;
    }
    iVar4 = iVar4 + 1;
  } while (iVar4 < 0x20);
  if (param_1 != -1) {
    count = count + -1;
  }
  if (count < 1) {
    display_assert("count > 0", "c:\\halo\\SOURCE\\game\\game_engine_race.c",
                   0x2a7, 1);
    system_exit(-1);
  }
  sVar1 = seed_random_range((unsigned int *)get_global_random_seed_address(), 0,
                       (short)count);
  piVar6 = (int *)(iVar2 + 0x378);
  iVar5 = (int)sVar1;
  iVar4 = 0;
  if (0 < *piVar6) {
    do {
      flag_element = (int)tag_block_get_element(piVar6, iVar4, 0x94);
      if ((*(short *)(flag_element + 0x10) == 3) &&
          (*(short *)(flag_element + 0x12) != param_1)) {
        if (iVar5 == 0) {
          local_8 = (int)*(short *)(flag_element + 0x12);
          if (local_8 != -1) {
            return local_8;
          }
          break;
        }
        iVar5 = iVar5 + -1;
      }
      iVar4 = iVar4 + 1;
    } while (iVar4 < *piVar6);
  }
  display_assert("new_flag != NONE",
                 "c:\\halo\\SOURCE\\game\\game_engine_race.c", 700, 1);
  system_exit(-1);
  return local_8;
}

/* FUN_000B4800 (0xb4800) — race engine: per-player race update.
 *
 * Reached through the game-engine variant function table (data xref from
 * 0x2f00a4), so the single stack parameter is a player handle.
 *
 * Announces state message 0x16 for the player, then — if the player has a
 * unit (player+0x34 != NONE) and scoring is enabled — tests the unit
 * against the race netgame flags. If the unit is riding something
 * (unit+0xcc != NONE) the parent object's position is used with a wider
 * radius (2.5) and zero height; otherwise the unit's own position is used
 * with radius 1.5 / height 0.6. Both probes ask for netgame flag type 3
 * (race) with index -1 (any).
 *
 * A hit (flag index != NONE) forwards to race_update_team_score, which
 * takes the player handle in EAX and the flag index on the stack
 * (000b488c PUSH EAX / 000b488d MOV EAX,EDI / 000b488f CALL 0xb46b0 /
 * ADD ESP,4).
 *
 * Position is read at object+0x50 (physics position), not +0x0c.
 *
 * Source: c:\halo\SOURCE\game\game_engine_race.c */
void FUN_000B4800(int player_handle)
{
  char *player;
  char *unit;
  char *parent;
  int flag_index;

  player = (char *)datum_get(player_data, player_handle);
  game_engine_state_message(player_handle, 0x16, player_handle);
  if (*(int *)(player + 0x34) != -1) {
    if (game_engine_can_score()) {
      unit = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
      if (*(int *)(unit + 0xcc) == -1) {
        flag_index =
          find_netgame_flag((float *)(unit + 0x50), 1.5f, 0.6f, 3, -1);
      } else {
        parent = (char *)object_get_and_verify_type(*(int *)(unit + 0xcc), 3);
        flag_index =
          find_netgame_flag((float *)(parent + 0x50), 2.5f, 0.0f, 3, -1);
      }
      if (flag_index != -1) {
        race_update_team_score(player_handle, flag_index);
      }
    }
  }
}

/* FUN_000b4960 (0xb4960) — race engine: initialize for new map.
 *
 * Sets up race-mode state for a new map. Clears the race globals region
 * (0x456f10..+0xd0), iterates scenario netgame flags of type 3 (race
 * flags), sets valid-flag bits in DAT_00456f10, and calls
 * game_engine_set_goal_position for each flag.
 *
 * Post-init depends on the game variant modifier (offset 0x4c):
 *   mode 2 (flags): picks a random starting flag via FUN_000b45c0(-1).
 *   mode 0 (normal): fills per-team flag indices with the minimum flag index.
 *   other: fills per-team flag indices with -1, returns 0xffffff01.
 *
 * Source: c:\halo\SOURCE\game\game_engine_race.c */
int FUN_000b4960(void)
{
  short sVar1;
  int iVar2;
  int iVar3;
  int min_flag;
  int *piVar5;

  min_flag = 0x20;
  iVar2 = (int)global_scenario_get();
  FUN_000b3860();
  *(char *)0x456fdc = 0;
  csmemset((void *)0x456f10, 0, 0xd0);
  piVar5 = (int *)(iVar2 + 0x378);
  *(int *)0x5aa744 = 0x1e;
  iVar2 = 0;
  if (0 < *piVar5) {
    do {
      iVar3 = (int)tag_block_get_element(piVar5, iVar2, 0x94);
      if (*(short *)(iVar3 + 0x10) == 3) {
        sVar1 = *(short *)(iVar3 + 0x12);
        if (sVar1 >= 0x20) {
          error(2,
                "one of the netgameflags that defines the track was out of "
                "the legal range 0..%d",
                0x20);
        } else {
          if (min_flag > (int)sVar1) {
            min_flag = (int)sVar1;
          }
          *(int *)0x456f10 |= (1 << ((unsigned char)sVar1 & 0x1f));
          game_engine_set_goal_position((int)sVar1, (void *)iVar3, 0.0f,
                                        "flag_blue", -1, -1, -1);
        }
      }
      iVar2 = iVar2 + 1;
    } while (iVar2 < *piVar5);
  }
  iVar2 = (int)game_engine_get_variant();
  if (*(int *)(iVar2 + 0x4c) == 2) {
    *(int *)0x456f94 = FUN_000b45c0(-1);
    return 1;
  }
  iVar2 = (int)game_engine_get_variant();
  {
    int *dest = (int *)0x456f14;
    int count = 16;
    if (*(int *)(iVar2 + 0x4c) == 0) {
      while (count--) {
        *dest++ = min_flag;
      }
      return 1;
    }
    while (count--) {
      *dest++ = -1;
    }
    return 1;
  }
}

/* FUN_000b4b10 (0xb4b10) — invalidate a player's race timestamp
 *
 * Looks up the player record for the given handle and stores -1 into the
 * dword at player+0x88. Neighboring race-engine code treats that dword as a
 * game_time stamp (game_engine.c writes game_time_get() there and subtracts
 * it to get an elapsed time), so -1 marks it invalid/unset.
 *
 * Disassembly (0xb4b10): MOV EAX,[EBP+8]; MOV ECX,[0x5aa6d4];
 * PUSH EAX; PUSH ECX; CALL datum_get (0x119320); ADD ESP,8;
 * MOV dword ptr [EAX+0x88],0xffffffff. */
void FUN_000b4b10(unsigned int player_handle)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  *(int *)(player + 0x88) = -1;
}

/* FUN_000b4d50 (0xb4d50) — race score lookup
 *
 * Looks up a player's race score value. If param_2 == 1, reads score from
 * the 0x456fe0 table indexed by the player's int at offset 0x20. Otherwise
 * reads from the 0x457020 table indexed by the player handle's low 16 bits. */
int FUN_000b4d50(unsigned int player_handle, int param_2)
{
  char *player;

  player = (char *)datum_get(player_data, player_handle);
  if (param_2 == 1) {
    return *(int *)(0x456fe0 + *(int *)(player + 0x20) * 4);
  }
  return *(int *)(0x457020 + (player_handle & 0xffff) * 4);
}

/* FUN_000b4d90 (0xb4d90) — check race state == 1 */
bool FUN_000b4d90(int param_1)
{
  bool result;
  result = 0;
  if (--param_1 == 0) {
    result = 1;
  }
  return result;
}

/* FUN_000b4da0 (0xb4da0) — race score format by handle
 *
 * Formats a race score string from the 0x457020 table, indexed by the
 * low 16 bits of the player handle. Uses the format string at 0x26c118. */
wchar_t *FUN_000b4da0(unsigned int player_handle, wchar_t *dst)
{
  usprintf(dst, (const wchar_t *)0x26c118,
           *(int *)(0x457020 + (player_handle & 0xffff) * 4));
  return dst;
}

/* FUN_000b4dd0 (0xb4dd0) — race score header "Score"
 *
 * Formats the static header string L"Score" into the destination buffer. */
wchar_t *FUN_000b4dd0(wchar_t *dst)
{
  usprintf(dst, L"Score");
  return dst;
}

/* FUN_000b4df0 (0xb4df0) — race score format by index
 *
 * Formats a race score string from the 0x456fe0 table, indexed directly
 * by param_1. Uses the format string at 0x26c118. */
wchar_t *FUN_000b4df0(int index, wchar_t *dst)
{
  usprintf(dst, (const wchar_t *)0x26c118, *(int *)(0x456fe0 + index * 4));
  return dst;
}

/* 0xb4e20 — find_next_target
 *
 * Slayer/oddball "next target" selection: counts the eligible players
 * (not us, not our previous target, different team, alive), picks a random
 * one of them with seed_random_range(seed, 0, count), then walks the player data
 * again to find that Nth eligible player and reports it.
 *
 * Register arg: @EDI = player datum handle (read uninitialized at 0xb4e2b
 * PUSH EDI, never written inside the function).
 *
 * Source file: c:\halo\SOURCE\game\game_engine_slayer.c, assert line 0xc2.
 *
 * Confirmed 0xb4e7b: PUSH EDI / PUSH g_players_data / CALL datum_get -> EBX
 *   (self), then PUSH ESI(iterator handle) / PUSH g_players_data ->
 *   EAX (other); the +0x20 team compare is other vs self, and the +0x34
 *   != -1 aliveness test is on `other`.
 * Confirmed 0xb4ec8: PUSH EAX(count) / PUSH 0x0 / CALL
 *   get_global_random_seed_address / PUSH EAX -> seed_random_range(seed, 0, count).
 * Confirmed 0xb4f56: on count exhaustion the chosen handle is re-read from
 *   the iterator ([EBP-0x18] == data_iter_t.datum_handle) and the assert
 *   fires when it is NONE.
 */
void find_next_target(int player_index)
{
  data_iter_t iterator;
  void *player;
  void *self;
  void *other;
  int last_target;
  int next_target;
  int count;
  unsigned int handle;

  player = datum_get(player_data, player_index);
  last_target = *(int *)((char *)player + 0x88);
  next_target = -1;
  count = 0;
  data_iterator_new(&iterator, player_data);
  if (data_iterator_next(&iterator) != 0) {
    do {
      handle = iterator.datum_handle;
      self = datum_get(player_data, player_index);
      other = datum_get(player_data, (int)handle);
      if (handle != (unsigned int)player_index &&
          handle != (unsigned int)last_target &&
          *(int *)((char *)other + 0x20) != *(int *)((char *)self + 0x20) &&
          *(int *)((char *)other + 0x34) != -1) {
        count = count + 1;
      }
    } while (data_iterator_next(&iterator) != 0);
    if (count > 0) {
      count = seed_random_range((unsigned int *)get_global_random_seed_address(), 0,
                           (int16_t)count);
      data_iterator_new(&iterator, player_data);
      while (data_iterator_next(&iterator) != 0) {
        handle = iterator.datum_handle;
        self = datum_get(player_data, player_index);
        other = datum_get(player_data, (int)handle);
        if (handle != (unsigned int)player_index &&
            handle != (unsigned int)last_target &&
            *(int *)((char *)other + 0x20) != *(int *)((char *)self + 0x20) &&
            *(int *)((char *)other + 0x34) != -1) {
          if (count == 0) {
            next_target = (int)iterator.datum_handle;
            if (next_target != -1)
              goto have_target;
            break;
          }
          count = count - 1;
        }
      }
      display_assert("next_target != NONE",
                     "c:\\halo\\SOURCE\\game\\game_engine_slayer.c", 0xc2, 1);
      system_exit(-1);
    }
  }
have_target:
  *(int *)((char *)player + 0x88) = next_target;
  if (next_target != -1) {
    game_engine_player_event(player_index, 0x1e, next_target);
  }
}

/* 0xb5040 — FUN_000b5040 — player-event message formatter
 *
 * Formats the
 * HUD/status message for a player event.  Referenced only as
 * data from the
 * handler table at 0x2f015c (no code callers), so the cdecl
 * signature comes
 * from the frame: [EBP+0x8] player handle, [EBP+0xc] event
 * type, [EBP+0x10]
 * target player handle, [EBP+0x14] destination buffer,
 * [EBP+0x18] buffer
 * size (wchar count).  Returns AL (MOV BL,1 at 0xb5056,
 * MOV AL,BL on every
 * formatted exit; XOR AL,AL on the unhandled-event exit).
 *
 * Confirmed
 * 0xb5049/0xb5058: MOV EAX,[0x5aa6d4] / PUSH ESI / PUSH EAX /
 *   CALL
 * 0x119320 — datum_get(player_data, player_handle), result discarded.
 *
 * Confirmed 0xb5065: CMP ECX,0x1e / JNZ 0xb512e, then 0xb512e CMP ECX,0x16 /
 *
 * JZ, CMP ECX,0x1e / JZ (the second 0x1e compare is unreachable), else
 *   XOR
 * AL,AL / RET.
 * Confirmed 0xb506e/0xb5073: CALL 0xa9350
 * (game_engine_get_variant) /
 *   MOV CL,[EAX+0x1c] selects the team-scoring
 * spelling.
 * Confirmed 0xb508f: MOV EDI,[EDX*4+0x456fe0] with EDX =
 * player+0x20 (team),
 *   and 0xb50a4: MOV ECX,[ESI*4+0x457020] with ESI =
 * handle & 0xffff — the
 *   same two score tables FUN_000b4da0 /
 * FUN_000b4df0 read.
 * Confirmed 0xb50bd: PUSH EDI / PUSH ECX / PUSH 0x26dd64
 * / PUSH 0x80 /
 *   PUSH EDX(local buffer) / CALL 0x19e9f0, ADD ESP,0x24; the
 * 128-wchar
 * local (SUB ESP,0x100) is formatted but never read afterwards —
 * kept
 * because the call order and side effects must be preserved.
 *
 * Confirmed 0xb50f1: the variant+0x1c == 0 arm uses the shared format at
 *
 * 0x26c118 with only the 0x457020 score.
 * Confirmed 0xb5104/0xb511d:
 * datum_get(player_data, target_handle), then
 *   PUSH EAX+4 / PUSH 0x26dd48 /
 * PUSH size / PUSH buffer — the target name
 *   lives at +4 in the player
 * datum.
 * Confirmed 0xb5150/0xb5156: PUSH 0x1 / PUSH ESI / CALL 0xa9e20
 *
 * (game_engine_get_place(handle, 1)) then CALL 0xa9af0
 *
 * (game_engine_place_to_string).
 * Confirmed 0xb51ad: PUSH EDX(variant+0x40) /
 * PUSH EAX(0x456fe0 team score) /
 *   PUSH ESI(0x457020 player score) / PUSH
 * EDI(place string) / PUSH 0x26dd14 /
 *   PUSH size / PUSH buffer, ADD
 * ESP,0x38.
 * Confirmed 0xb51f9: the non-team arm pushes variant+0x40, the
 * 0x456fe0 team
 *   score and the place string with the format at 0x26dcf0,
 * ADD ESP,0x2c.
 */
bool FUN_000b5040(unsigned int player_handle, int event_type, int target_handle,
                  wchar_t *buffer, int buffer_size)
{
  wchar_t local_buffer[128];
  void *variant;
  void *player;
  void *target;
  wchar_t *place_string;
  int table_score;
  int player_score;

  datum_get(player_data, player_handle);
  if (event_type == 0x1e) {
    variant = game_engine_get_variant();
    if (*((char *)variant + 0x1c) != 0) {
      player = datum_get(player_data, player_handle);
      table_score = *(int *)(0x456fe0 + *(int *)((char *)player + 0x20) * 4);
      datum_get(player_data, player_handle);
      unicode_sprintf(local_buffer, 0x80, L"%d team %d",
                      *(int *)(0x457020 + (player_handle & 0xffff) * 4),
                      table_score);
    } else {
      datum_get(player_data, player_handle);
      unicode_sprintf(local_buffer, 0x80, (const wchar_t *)0x26c118,
                      *(int *)(0x457020 + (player_handle & 0xffff) * 4));
    }
    target = datum_get(player_data, target_handle);
    unicode_sprintf(buffer, buffer_size, L"New Target %s",
                    (wchar_t *)((char *)target + 4));
    return true;
  }
  if (event_type == 0x16) {
    variant = game_engine_get_variant();
    if (*((char *)variant + 0x1c) != 0) {
      place_string =
        game_engine_place_to_string(game_engine_get_place(player_handle, 1));
      player = datum_get(player_data, player_handle);
      table_score = *(int *)(0x456fe0 + *(int *)((char *)player + 0x20) * 4);
      datum_get(player_data, player_handle);
      player_score = *(int *)(0x457020 + (player_handle & 0xffff) * 4);
      variant = game_engine_get_variant();
      unicode_sprintf(buffer, buffer_size, L"%s kills %d team %d of %d",
                      place_string, player_score, table_score,
                      *(int *)((char *)variant + 0x40));
      return true;
    }
    place_string =
      game_engine_place_to_string(game_engine_get_place(player_handle, 1));
    player = datum_get(player_data, player_handle);
    table_score = *(int *)(0x456fe0 + *(int *)((char *)player + 0x20) * 4);
    variant = game_engine_get_variant();
    unicode_sprintf(buffer, buffer_size, L"%s kills %d of %d", place_string,
                    table_score, *(int *)((char *)variant + 0x40));
    return true;
  }
  return false;
}

/* 0xb5490 — FUN_000b5490
 *
 * Returns the name string for a given material type index.
 * If material_type == -1, returns "NONE" (at 0x253a04).
 * Otherwise asserts material_type is in [0, NUMBER_OF_MATERIAL_TYPES) where
 * NUMBER_OF_MATERIAL_TYPES == 0x21 (33), then indexes into the static pointer
 * table at 0x2f0208 (const char *[33]).
 *
 * Confirmed: PUSH 0x1 / PUSH 0x389 / PUSH 0x26dfc4 / PUSH 0x26df88 /
 *   CALL display_assert / PUSH -0x1 / CALL system_exit / ADD ESP,0x14 at
 *   0xb54a9-0xb54c6 (lazy cdecl stack cleanup covers both calls).
 * Confirmed: MOVSX EAX,SI / MOV EAX,[EAX*4+0x2f0208] table lookup at 0xb54c9.
 * Confirmed: CMP SI,-0x1 / JZ → MOV EAX,0x253a04 / RET at 0xb54d6.
 * Source file: c:\halo\SOURCE\game\game_globals.c, assert line 0x389 (905).
 */
const char *FUN_000b5490(short material_type)
{
  if (material_type == -1)
    return (const char *)0x253a04;
  if (material_type < 0 || material_type >= 0x21) {
    display_assert("material_type>=0 && material_type<NUMBER_OF_MATERIAL_TYPES",
                   "c:\\halo\\SOURCE\\game\\game_globals.c", 0x389, 1);
    system_exit(-1);
  }
  return ((const char **)0x2f0208)[material_type];
}

/* 0xb54e0 — game_globals_difficulty_scale
 *
 * Looks up a difficulty scaling factor from the game globals matg tag's
 * difficulty block.  The block stores an array of floats arranged as
 * value_type rows of 4 difficulty columns (Easy/Normal/Heroic/Legendary).
 *
 * Register args: @BX = value_type (0..0x22), @DI = difficulty (-1 = raw
 * per-value_type default, 0..3 = clamped to [0, 3]).
 *
 * If difficulty < 0 the function indexes the table as value_type*16 bytes
 * from the element base (i.e. column 0, raw float[value_type][0]).
 * If difficulty >= 0 it clamps to min(difficulty, 3) and indexes as
 * (difficulty + value_type*4)*4 bytes, i.e. float[value_type][difficulty].
 *
 * Falls back to 1.0f if game_globals_get() returns NULL, the difficulty
 * block count is zero, or tag_block_get_element returns NULL.
 *
 * Confirmed: CALL 0x18e450 (game_globals_get) at 0xb54ec.
 * Confirmed: TEST BX,BX / CMP BX,0x23 range assert at 0xb54f1/0xb54f8.
 * Confirmed: MOV ECX,[ESI+0x11c] / LEA EAX,[ESI+0x11c] at 0xb5522/0xb552a.
 * Confirmed: PUSH 0x284 / PUSH 0x0 / PUSH EAX to tag_block_get_element at
 *   0xb5532. Confirmed: TEST DI,DI / JGE 0xb555e selects raw vs clamped path.
 * Confirmed: XOR ECX,ECX / MOVSX EDX,BX / MOVSX ECX,CX / LEA EDX,[ECX+EDX*4]
 *   then FLD [EAX+EDX*4] for the raw (DI<0) path at 0xb554b.
 * Confirmed: CMP DI,3 / MOV ECX,3 / JG / MOV ECX,EDI clamping at 0xb555e.
 */
__declspec(noinline) float game_globals_difficulty_scale(int16_t value_type, int16_t difficulty)
{
  float default_val = 1.0f;
  void *globals;
  void *element;
  int16_t clamped;
  int idx;

  assert_halt(value_type >= 0 && value_type < 0x23);

  globals = game_globals_get();
  if (!globals)
    return default_val;

  if (*(int *)((char *)globals + 0x11c) == 0)
    return default_val;

  element = tag_block_get_element((char *)globals + 0x11c, 0, 0x284);
  if (!element)
    return default_val;

  if (difficulty < 0) {
    idx = (int)value_type * 4;
    return *(float *)((char *)element + idx * 4);
  }

  clamped = difficulty > 3 ? 3 : difficulty;
  idx = (int)clamped + (int)value_type * 4;
  return *(float *)((char *)element + idx * 4);
}

/* 0xb5590 — FUN_000b5590
 *
 * Convenience wrapper: fetches the current game difficulty level and
 * returns game_globals_difficulty_scale(value_type, difficulty).
 *
 * Confirmed: CALL 0xa7460 (game_difficulty_level_get) at 0xb5595.
 * Confirmed: MOV EBX,[EBP+0x8] (value_type cdecl arg) at 0xb559a.
 * Confirmed: MOV EDI,EAX (difficulty ← return AX of game_difficulty_level_get)
 *   at 0xb559d. Confirmed: CALL 0xb54e0 (game_globals_difficulty_scale,
 *   @bx=value_type, @di=difficulty) at 0xb559f.
 */
float FUN_000b5590(int16_t value_type)
{
  int16_t difficulty = game_difficulty_level_get();
  return game_globals_difficulty_scale(value_type, difficulty);
}

/* FUN_000b55b0 (0xb55b0) — game_globals_difficulty_scale_get
 *
 * Returns the difficulty scaling factor for a given game-globals value type
 * and team. The lookup strategy depends on game mode:
 *   - If the game engine is running (multiplayer/co-op): force difficulty=1
 *     (Normal) regardless of the current level setting.
 *   - Else if team is friendly (allegiance_get_team_is_friendly(1, team)):
 *     use the actual difficulty level from game_difficulty_level_get.
 *   - Else: look up value_type in an override table at 0x26ddc8
 *     (int16_t[0x23]). If the override is NONE (-1, meaning no override),
 *     force difficulty=1. Otherwise substitute the override value_type and
 *     use the actual difficulty level.
 *
 * The underlying scale is fetched by game_globals_difficulty_scale
 * (@BX=value_type,
 * @DI=difficulty) from the game globals tag (matg) difficulty block.
 *
 * Confirmed: CALL 0xa7460 (game_difficulty_level_get) → EDI at 0xb55ba.
 * Confirmed: CALL 0xa8e30 (game_engine_running) → AL at 0xb55bc.
 * Confirmed: MOV EDI,1 when running at 0xb55c5 (force Normal).
 * Confirmed: CALL 0xa7a30 (game_allegiance_get_team_is_friendly(1, team)) at
 * 0xb55dc. Confirmed: assert value_type in [0, 0x22] at line 0x3bd (957),
 *   __FILE__ "c:\halo\SOURCE\game\game_globals.c".
 * Confirmed: override table word lookup at [ECX*2 + 0x26ddc8] at 0xb5619.
 * Confirmed: CMP AX,0xffff selects between override=NONE (EDI=1) and
 *   override != NONE (EBX=override, EDI=actual difficulty) at 0xb5621.
 * Confirmed: CALL 0xb54e0 with BX=value_type and DI=difficulty in all paths.
 */
float FUN_000b55b0(short value_type, int team)
{
  int16_t difficulty = game_difficulty_level_get();
  if (game_engine_running()) {
    return game_globals_difficulty_scale(value_type, 1);
  }
  if (game_allegiance_get_team_is_friendly(1, team)) {
    return game_globals_difficulty_scale(value_type, difficulty);
  }
  assert_halt(value_type >= 0 && value_type < 0x23);
  {
    int16_t override = *(int16_t *)(0x26ddc8 + (int)value_type * 2);
    if (override == (int16_t)0xffff) {
      return game_globals_difficulty_scale(value_type, 1);
    }
    return game_globals_difficulty_scale(override, difficulty);
  }
}
