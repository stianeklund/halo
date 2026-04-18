//
// AUTOMATICALLY GENERATED. DO NOT EDIT.
//

#define HFUNC __declspec(dllexport)
#define HDATA __declspec(dllimport)

// obj:<common> src:?
HDATA char *breakable_surface_globals;
HDATA data_t * cached_object_render_states;
HDATA char cheats_globals[0xc80];
HDATA data_t *contrail_data;
HDATA data_t *contrail_point_data;
HDATA void *current_game_engine;
HDATA int16_t decal_counts_0;
HDATA int16_t decal_counts_1;
HDATA char *decal_globals;
HDATA data_t *effect_data;
HDATA data_t *effect_location_data;
HDATA char error_string_buffer[256];
HDATA char event_manager_globals[0x108];
HDATA char *game_allegiance_globals;
HDATA int16_t game_engine_variant_index;
HDATA game_state_globals_t game_state_globals;
HDATA game_variant_t game_variant_global;
HDATA data_t *global_decal_data;
HDATA void *global_structure_bsp;
HDATA data_t *hs_syntax_data;
HDATA char input_frame_tick;
HDATA char local_player_network_indices[64];
HDATA data_t * object_header_data;
HDATA data_t *particle_data;
HDATA data_t *particle_system_data;
HDATA data_t *particle_system_header_data;
HDATA data_t * player_data;
HDATA char *player_effect_globals;
HDATA char player_ui_globals[0x230];
HDATA players_globals_t * players_globals;
HDATA int render;
HDATA char *rumble_globals;
HDATA data_t * team_data;
HDATA int16_t weather_particle_system_count;
HDATA data_t *weather_particle_system_data;

// obj:LIBCMT:_control87.obj src:?
HFUNC unsigned int __control87(unsigned int new_value, unsigned int mask);

// obj:LIBCMT:qsort.obj src:?
HFUNC void __cdecl qsort(void *base, size_t nmemb, size_t size, int (__cdecl *compar)(const void *, const void *));

// obj:LIBCMT:strncpy.obj src:?
HFUNC char *strncpy(char *destination, const char *source, size_t count);

// obj:LIBCMT:vsprintf.obj src:?
HFUNC int vsprintf(char *buffer, const char *format, char *arglist);

// obj:LIBCMT:wcsncpy.obj src:?
HFUNC wchar_t *_wcsncpy(wchar_t *dest, const wchar_t *src, size_t count);

// obj:ai.obj src:ai/ai.c
HFUNC void ai_dispose(void);
HFUNC void ai_dispose_from_old_map(void);
HFUNC bool ai_enemies_can_see_player(void);
HFUNC void ai_initialize(void);
HFUNC void ai_initialize_for_new_map(void);
HFUNC void ai_place(void);
HFUNC void ai_update(void);

// obj:bink_playback.obj src:bink/bink_playback.c
HFUNC void bink_playback_dispose(void);
HFUNC void bink_playback_initialize(void);
HFUNC void bink_playback_render(void);
HFUNC void bink_playback_update(void);

// obj:bitmaps.obj src:bitmaps/bitmaps.c
HFUNC void bitmap_delete(void *);

// obj:breakable_surfaces.obj src:physics/breakable_surfaces.c
HFUNC void breakable_surfaces_dispose(void);
HFUNC void breakable_surfaces_dispose_from_old_map(void);
HFUNC void breakable_surfaces_initialize(void);
HFUNC void breakable_surfaces_initialize_for_new_map(void);

// obj:cache_files_windows.obj src:cache/cache_files_windows.c
HFUNC bool cache_files_precache_in_progress(void);
HFUNC bool cache_files_precache_is_copying_map(char *map_name);
HFUNC bool cache_files_precache_map_begin(char *map_name, bool);
HFUNC void cache_files_precache_map_end(void);
HFUNC bool cache_files_precache_map_loaded(char *map_name);
HFUNC void cache_files_precache_map_queue_end(void);
HFUNC __int16 cache_files_precache_map_status(float *);
HFUNC void cache_files_precache_set_priority(bool);

// obj:cheats.obj src:game/cheats.c
HFUNC void cheats_dispose(void);
HFUNC void cheats_dispose_from_old_map(void);
HFUNC void cheats_initialize(void);
HFUNC void cheats_initialize_for_new_map(void);
HFUNC void cheats_update(void);

// obj:cinematics.obj src:cutscene/cinematics.c
HDATA cinematic_globals_t * cinematic_globals;
HFUNC bool cinematic_can_be_skipped(void);
HFUNC void cinematic_dispose(void);
HFUNC void cinematic_dispose_from_old_map(void);
HFUNC bool cinematic_in_progress(void);
HFUNC void cinematic_initialize(void);
HFUNC void cinematic_initialize_for_new_map(void);

// obj:collision_usage.obj src:physics/collision_usage.c
HFUNC void collision_log_begin_period(__int16 time_period);
HFUNC void collision_log_continue_period(__int16 time_period);
HFUNC void collision_log_end_period(void);
HFUNC void collision_log_initialize(void);

// obj:console.obj src:main/console.c
HFUNC void console_dispose(void);
HFUNC void console_initialize(void);
HFUNC void console_initialize_for_new_map(void);
HFUNC void console_startup(void);
HFUNC bool console_update(void);
HFUNC void console_warning(const char* format, ...);

// obj:contrails.obj src:effects/contrails.c
HFUNC void contrails_dispose(void);
HFUNC void contrails_dispose_from_old_map(void);
HFUNC void contrails_initialize(void);
HFUNC void contrails_initialize_for_new_map(void);
HFUNC void contrails_update(float);

// obj:crc.obj src:memory/crc.c
HFUNC void crc_checksum_buffer(uint32_t *checksum, void *data, int size);

// obj:cseries.obj src:cseries/cseries.c
HFUNC void *csmemcpy(void *destination, void *source, size_t size);
HFUNC void *csmemset(void *buffer, int c, size_t size);
HFUNC char *csprintf(char *buffer, const char *format, ...);
HFUNC int csstrlen(const char *s1);
HFUNC void *csstrncpy(char *destination, const char *source, size_t size);
HFUNC void display_assert(const char *reason, const char *filepath, int lineno, bool halt);

// obj:cseries_windows.obj src:cseries/cseries_windows.c
HFUNC void __noreturn system_exit(int a2);
HFUNC unsigned int system_milliseconds(void);

// obj:d3d_intimacy.obj src:?
HFUNC int *d3d_find_flipcount(void);

// obj:data.obj src:memory/data.c
HFUNC int data_allocation_size(__int16 count, __int16 size);
HFUNC void data_delete_all(data_t *data);
HFUNC void data_dispose(data_t *data);
HFUNC void data_initialize(data_t *data, char *name, __int16 maximum_count, __int16 size);
HFUNC void data_iterator_new(data_iter_t *iter, data_t *data);
HFUNC void *data_iterator_next(data_iter_t *iterator);
HFUNC void data_make_invalid(data_t *data);
HFUNC void data_make_valid(data_t *data);
HFUNC data_t *data_new(char *name, int16_t maximum_count, int16_t size);
HFUNC int data_new_at_index(data_t *data);
HFUNC int data_new_datum(data_t *data, int handle);
HFUNC int data_next_index(data_t *data, int prev_index);
HFUNC void data_verify(data_t *data);
HFUNC int datum_absolute_index_to_index(data_t *data, int absolute_index);
HFUNC void datum_delete(data_t *data, int datum_handle);
HFUNC void *datum_get(data_t *data, int datum_handle);

// obj:debug_keys.obj src:main/debug_keys.c
HFUNC void debug_keys_dispose(void);
HFUNC void debug_keys_initialize(void);
HFUNC void debug_keys_update(void);

// obj:debug_memory.obj src:cseries/debug_memory.c
HFUNC void debug_free(void *ptr, const char *file, int line);
HFUNC void *debug_malloc(uint32_t size, bool zero, const char *file, int line);

// obj:decals.obj src:effects/decals.c
HFUNC void decals_dispose(void);
HFUNC void decals_dispose_from_old_map(void);
HFUNC void decals_initialize(void);
HFUNC void decals_initialize_for_new_map(void);

// obj:director.obj src:camera/director.c
HFUNC void director_apply_replay_mode_for_player(char reset_flag, int16_t local_player_index, char mode_flags);
HFUNC bool director_compute_camera_input(short *out_buf, int local_player_index);
HFUNC void director_dispose(void);
HFUNC void director_dispose_from_old_map(void);
HFUNC void director_init_player_cameras(int16_t local_player_index);
HFUNC void director_initialize(void);
HFUNC void director_initialize_for_new_map(void);
HFUNC void director_set_player_camera_normal(int16_t local_player_index, char reset_flag, char mode_flags);
HFUNC void director_set_player_camera_scripted(int16_t local_player_index, char reset_flag);
HFUNC void director_update(float);

// obj:editor_stubs.obj src:game/editor_stubs.c
HFUNC void editor_dispose(void);
HFUNC void editor_dispose_from_old_map(void);
HFUNC void editor_initialize(void);
HFUNC void editor_initialize_for_new_map(void);
HFUNC bool editor_should_exit(void);
HFUNC void editor_update(void);
HFUNC bool game_in_editor(void);

// obj:effects.obj src:effects/effects.c
HFUNC bool dangerous_effects_near_player(void);
HFUNC void effect_update(int effect_index, float elapsed);
HFUNC void effects_dispose(void);
HFUNC void effects_dispose_from_old_map(void);
HFUNC void effects_initialize(void);
HFUNC void effects_initialize_for_new_map(void);
HFUNC bool effects_update(float);

// obj:errors.obj src:cseries/errors.c
HFUNC void error(unsigned __int16 a1, const char *a2, ...);

// obj:event_manager.obj src:interface/event_manager.c
HFUNC void event_manager_dispatch(int16_t *event, int16_t player_index);
HFUNC void event_manager_dispose(void);
HFUNC void event_manager_initialize(void);
HFUNC void event_manager_update(void);

// obj:files.obj src:tag_files/files.c
HFUNC file_ref_t *file_reference_create_from_path(file_ref_t *info, const char *directory, bool a4);

// obj:first_person_weapons.obj src:interface/first_person_weapons.c
HFUNC void first_person_weapons_update(void);

// obj:game.obj src:game/game.c
HDATA bool byte_2EF808;
HDATA void* off_2EF800;
HFUNC bool game_all_quiet(void);
HFUNC int16_t game_difficulty_level_get(void);
HFUNC int game_difficulty_level_get_ignore_easy(void);
HFUNC void game_difficulty_level_set(int16_t);
HFUNC void game_dispose(void);
HFUNC void game_dispose_from_old_map(void);
HFUNC void game_frame(float);
HFUNC void game_initial_pulse(void);
HFUNC void game_initialize(void);
HFUNC void game_initialize_for_new_map(void);
HFUNC bool game_is_cooperative(void);
HFUNC bool game_load(game_options_t *options);
HFUNC bool game_map_loading_in_progress(float *progress);
HFUNC void game_options_new(game_options_t *game_options);
HFUNC bool game_options_verify(game_options_t *game_options);
HFUNC bool game_players_are_double_speed(void);
HFUNC void game_precache_new_map(char *map_name, bool a2);
HFUNC bool game_safe_to_save(void);
HFUNC bool game_safe_to_speak(void);
HFUNC void game_set_game_engine_index(void);
HFUNC void game_set_game_variant(game_variant_t *variant);
HFUNC void game_set_game_variant_from_name(const char*);
HFUNC void game_set_players_are_double_speed(bool);
HFUNC void game_tick(void);
HFUNC void game_unload(void);
HFUNC void remove_quitting_players_from_game(void);
HFUNC void set_random_seed(int);

// obj:game_allegiance.obj src:game/game_allegiance.c
HFUNC void game_allegiance_dispose(void);
HFUNC void game_allegiance_dispose_from_old_map(void);
HFUNC void game_allegiance_initialize(void);
HFUNC void game_allegiance_initialize_for_new_map(void);
HFUNC void game_allegiance_update(void);

// obj:game_engine.obj src:game/game_engine.c
HFUNC void game_engine_dispose(void);
HFUNC void game_engine_dispose_from_old_map(void);
HFUNC void game_engine_evaluate_game_complexity(void);
HFUNC bool game_engine_force_single_screen(void);
HFUNC int game_engine_game_starting(void);
HFUNC game_variant_t *game_engine_get_variant_by_name(game_variant_t *variant, const char *name);
HFUNC void game_engine_initialize(game_variant_t *variant);
HFUNC void game_engine_initialize_for_new_map(void);
HFUNC void game_engine_player_added(int player_data_handle);
HFUNC bool game_engine_running(void);
HFUNC void game_engine_update(void);
HFUNC void game_engine_update_non_deterministic(float);

// obj:game_sound.obj src:sound/game_sound.c
HFUNC void game_sound_dispose(void);
HFUNC void game_sound_dispose_from_old_map(void);
HFUNC void game_sound_initialize(void);
HFUNC void game_sound_initialize_for_new_map(void);
HFUNC void game_sound_update(float);

// obj:game_state.obj src:saved_games/game_state.c
HFUNC data_t *game_state_data_new(char *name, __int16 maximum_count, __int16 size);
HFUNC void game_state_dispose(void);
HFUNC void game_state_dispose_from_old_map(void);
HFUNC void game_state_initialize_for_new_map(void);
HFUNC void game_state_load_core(const char *);
HFUNC void *game_state_malloc(const char *name, const char *a2, int size);
HFUNC void game_state_revert(void);
HFUNC void game_state_save(void);
HFUNC void game_state_save_core(const char *);

// obj:game_state_xbox.obj src:?
HFUNC void xbox_game_state_close_file(void);
HFUNC void xbox_game_state_dispose_buffer(void);

// obj:game_statistics.obj src:game/game_statistics.c
HFUNC void game_statistics_start(void);

// obj:game_time.obj src:game/game_time.c
HFUNC bool game_in_progress(void);
HFUNC bool game_predicting(void);
HFUNC void game_time_dispose(void);
HFUNC void game_time_dispose_from_old_map(void);
HFUNC void game_time_end(void);
HFUNC int game_time_get(void);
HFUNC int16_t game_time_get_elapsed(void);
HFUNC bool game_time_get_paused(void);
HFUNC float game_time_get_speed(void);
HFUNC void game_time_initialize(void);
HFUNC void game_time_initialize_for_new_map(void);
HFUNC bool game_time_initialized(void);
HFUNC void game_time_set_paused(bool);
HFUNC void game_time_set_speed(float);
HFUNC void game_time_start(void);
HFUNC void game_time_statistics_frame(int16_t, int16_t, int16_t);
HFUNC void game_time_statistics_new(void);
HFUNC void game_time_update(float);
HFUNC int local_time_get(void);
HFUNC int16_t local_time_get_elapsed(void);

// obj:hs.obj src:hs/hs.c
HFUNC void hs_dispose(void);
HFUNC void hs_dispose_from_old_map(void);
HFUNC void hs_initialize(void);
HFUNC void hs_initialize_for_new_map(void);
HFUNC void hs_update(void);

// obj:hs_runtime.obj src:hs/hs_runtime.c
HFUNC void hs_runtime_dispose_from_old_map(void);
HFUNC void hs_runtime_initialize_for_new_map(void);

// obj:hud.obj src:interface/hud.c
HDATA void * hud_globals;
HFUNC void hud_autosave(int16_t);
HFUNC wchar_t *hud_get_item_string(int a1);
HFUNC void hud_load(bool a1);
HFUNC void hud_update(void);

// obj:hud_messaging.obj src:interface/hud_messaging.c
HFUNC void hud_print_message(__int16 player, wchar_t *message);
HFUNC _BYTE *scripted_hud_messages_clear(void);

// obj:input_abstraction.obj src:input/input_abstraction.c
HFUNC void input_abstraction_dispose(void);
HFUNC void input_abstraction_initialize(void);
HFUNC void input_abstraction_update(void);

// obj:input_xbox.obj src:input/input_xbox.c
HFUNC void input_flush(void);
HFUNC void input_frame_begin(void);
HFUNC void input_frame_end(void);
HFUNC void input_get_device_states(void);
HFUNC void *input_get_gamepad_state(int gamepad_index);
HFUNC bool input_has_gamepad(int16_t gamepad_index);
HFUNC bool input_key_is_down(uint16_t);
HFUNC void input_set_rumble(int16_t gamepad_index, uint16_t left, uint16_t right);
HFUNC void input_update(void);

// obj:interface.obj src:interface/interface.c
HFUNC void interface_dispose(void);
HFUNC void interface_dispose_from_old_map(void);
HFUNC void interface_initialize(void);
HFUNC void interface_initialize_for_new_map(void);

// obj:items.obj src:items/items.c
HFUNC bool dangerous_items_near_player(void);

// obj:main.obj src:main/main.c
HDATA bool debug_game_save;
HDATA bool debug_no_drawing;
HDATA short global_difficulty_level;
HDATA __int16 global_screenshot_count;
HDATA short player_spawn_count;
HFUNC void compute_window_bounds(int player_index, int num_players, viewport_bounds_t *a3, viewport_bounds_t *a4);
HFUNC void create_local_players(void);
HFUNC void display_error_damaged_media(void);
HFUNC short game_connection(void);
HFUNC void main_change_map_name(void);
HFUNC void main_frame_rate_debug(void);
HFUNC void main_game_render(double a2);
HFUNC void main_initialize_time(void);
HFUNC void main_load_last_solo_map(void);
HFUNC void main_load_ui_scenario(bool a1);
HFUNC void main_loop(void);
HFUNC void main_menu_load(void);
HFUNC void main_menu_precache_resources(void);
HFUNC void main_new_map(game_options_t *game_options);
HFUNC void main_pregame_render(void);
HFUNC void main_present_frame(void);
HFUNC void main_queue_map_name(char *map_name);
HFUNC void main_rasterizer_throttle(void);
HFUNC void main_save_current_solo_map(char *map_name);
HFUNC void main_save_map_private(void);
HFUNC void main_setup_connection(void);
HFUNC void main_skip_private(void);
HFUNC void main_update_time(void);
HFUNC void main_vertical_blank_interrupt_handler(void);
HFUNC void main_won_map_private(void);
HFUNC void screenshot_render(void *a1);
HFUNC void set_window_camera_values(void *window, float *a3);
HFUNC int __cdecl sort_desired_local_player_controllers(const void *a1, const void *a2);

// obj:marketing_and_strategic_business_development.obj src:interface/marketing_and_strategic_business_development.c
HFUNC void xbox_demos_launch(void);

// obj:network_game_globals.obj src:networking/network_game_globals.c
HFUNC void dispose_global_network_game_client(void);
HFUNC void dispose_global_network_game_server(void);
HFUNC void network_game_abort(void);
HFUNC bool network_game_client_end_frame(void);
HFUNC bool network_game_client_start_frame(void);
HFUNC bool network_game_server_start_frame(void);

// obj:network_messages.obj src:networking/network_messages.c
HFUNC void initialize_network_game_packets(void);

// obj:objects.obj src:objects/objects.c
HDATA object_globals_t * object_globals;
HDATA void * object_name_list;
HDATA void * objects;
HFUNC void *object_get_and_verify_type(int datum_handle, int type_mask);
HFUNC void objects_dispose(void);
HFUNC void objects_dispose_from_old_map(void);
HFUNC void objects_initialize(void);
HFUNC void objects_initialize_for_new_map(void);
HFUNC void objects_place(void);
HFUNC void objects_update(void);

// obj:observer.obj src:camera/observer.c
HFUNC void observer_dispose_from_old_map(void);
HFUNC void *observer_get_camera(unsigned __int16 local_player_index);
HFUNC void observer_initialize(void);
HFUNC int observer_initialize_for_new_map(void);
HFUNC void observer_update(float);

// obj:particle_systems.obj src:effects/particle_systems.c
HFUNC void particle_system_delete(int particle_system_handle);
HFUNC void particle_systems_dispose(void);
HFUNC void particle_systems_dispose_from_old_map(void);
HFUNC void particle_systems_initialize(void);
HFUNC void particle_systems_initialize_for_new_map(void);
HFUNC void particle_systems_update(float);

// obj:particles.obj src:effects/particles.c
HFUNC void particles_dispose(void);
HFUNC void particles_dispose_from_old_map(void);
HFUNC void particles_initialize(void);
HFUNC void particles_initialize_for_new_map(void);
HFUNC void particles_update(float);

// obj:physical_memory_map.obj src:cache/physical_memory_map.c
HDATA physical_memory_map_globals_t physical_memory_map_globals;
HFUNC void physical_memory_allocate(void);

// obj:player_control.obj src:game/player_control.c
HFUNC void player_control_dispose(void);
HFUNC void player_control_get_facing(int16_t local_player_index, float delta_time);
HFUNC void player_control_initialize_for_new_map(void);
HFUNC void player_control_new_unit(uint16_t local_player_index, int player_index);
HFUNC void player_control_update(float);

// obj:player_effects.obj src:effects/player_effects.c
HFUNC void player_effect_dispose(void);
HFUNC void player_effect_dispose_from_old_map(void);
HFUNC char *player_effect_get(int16_t local_player_index);
HFUNC void player_effect_initialize(void);
HFUNC void player_effect_initialize_for_new_map(void);
HFUNC void player_effect_update(void);

// obj:player_queues_new.obj src:game/player_queues_new.c
HFUNC bool player_control_get_current_actions(void *action_buf);
HFUNC void update_client_apply_actions(int16_t ticks);
HFUNC void update_client_start(void);
HFUNC int update_get_game_time(void);
HFUNC int update_get_maximum_actions(void);
HFUNC void update_server_delete(void);
HFUNC void update_server_new(void);
HFUNC void update_server_start(void);

// obj:player_rumble.obj src:game/player_rumble.c
HFUNC uint32_t rumble_calculate(char *slot);
HFUNC void rumble_clear_for_local_player(int16_t local_player_index);
HFUNC void rumble_dispose(void);
HFUNC void rumble_dispose_from_old_map(void);
HFUNC void rumble_initialize(void);
HFUNC void rumble_initialize_for_new_map(void);
HFUNC void rumble_update(void);

// obj:player_ui.obj src:interface/player_ui.c
HFUNC void player_ui_dispose(void);
HFUNC __int16 player_ui_get_single_player_local_player_controller(__int16 a1);
HFUNC void player_ui_initialize(void);
HFUNC void player_ui_remember_player1_profile(bool);

// obj:players.obj src:game/players.c
HFUNC bool any_player_is_dead(void);
HFUNC bool any_player_is_in_the_air(void);
HFUNC int find_unused_local_player_index(void);
HFUNC __int16 local_player_count(void);
HFUNC bool local_player_exists(int16_t);
HFUNC __int16 local_player_get_next(__int16 a1);
HFUNC int local_player_get_player_index(int16_t local_player_index);
HFUNC int local_player_set_player_index(unsigned __int16 a1, int a2);
HFUNC void *machine_get_player_list(int16_t);
HFUNC void player_build_action_update(int player_handle, void *control_out, void *action);
HFUNC void player_delete(int);
HFUNC void player_died(int);
HFUNC bool player_examine_nearby_unit(int player_unit_handle, int nearby_unit_handle);
HFUNC int player_index_from_unit_index(int);
HFUNC void player_input_enable(bool);
HFUNC bool player_input_enabled(void);
HFUNC int player_new(unsigned __int16 local_player_index, int player_handle_hint, unsigned __int16 local_player_index2, char *player_name);
HFUNC void player_register_machine(unsigned __int16 local_player_index, int player_handle);
HFUNC void player_reset_action_result(int player_handle);
HFUNC void player_set_action_result_for_vehicle(int player_handle, int vehicle_handle);
HFUNC void player_spawn(int player_handle);
HFUNC int player_try_to_enter_vehicle(void);
HFUNC bool player_try_to_spawn_in_vehicle(int player_handle);
HFUNC bool players_are_all_dead(void);
HFUNC int players_compute_local_player_count(void);
HFUNC void players_dispose(void);
HFUNC void players_dispose_from_old_map(void);
HFUNC void *players_get_combined_pvs(void);
HFUNC void *players_get_combined_pvs_local(void);
HFUNC int16_t players_get_respawn_failure(void);
HFUNC void players_initialize(void);
HFUNC void players_initialize_for_new_map(void);
HFUNC bool players_respawn_coop(void);
HFUNC void players_update_after_game(void);
HFUNC void players_update_before_game(void);
HFUNC void players_update_pvs(bool local_player_only);

// obj:point_physics.obj src:physics/point_physics.c
HFUNC void point_physics_dispose_from_old_map(void);
HFUNC void point_physics_initialize_for_new_map(void);

// obj:predicted_resources.obj src:?
HFUNC void predicted_resources_precache(int *a1);

// obj:profile.obj src:cseries/profile.c
HDATA bool profile_global_enable;
HFUNC void profile_enter_private(void*);
HFUNC void profile_exit_private(void*);
HFUNC void profile_frame_end(void);
HFUNC void profile_frame_start(void);
HFUNC void profile_render_end(void);
HFUNC void profile_render_start(void);
HFUNC void profile_render_window_end(void);
HFUNC void profile_render_window_start(char a1);
HFUNC void profile_tick_end(void);
HFUNC void profile_tick_start(void);

// obj:progress_bar.obj src:interface/progress_bar.c
HFUNC void progress_bar_begin(bool);
HFUNC void progress_bar_display(float progress);
HFUNC void progress_bar_dispose(void);
HFUNC void progress_bar_end(void);
HFUNC void progress_bar_initialize(void);

// obj:projectiles.obj src:items/projectiles.c
HFUNC bool dangerous_projectiles_near_player(void);

// obj:random_math.obj src:math/random_math.c
HFUNC int *get_global_random_seed_address(void);
HFUNC void lock_global_random_seed(void);
HFUNC void random_seed_debug_log(bool a1);
HFUNC void unlock_global_random_seed(void);

// obj:rasterizer.obj src:rasterizer/rasterizer.c
HFUNC void rasterizer_frame_begin(float *a1);
HFUNC void rasterizer_frame_end(void);
HFUNC void rasterizer_set_vblank_callback(void *);
HFUNC int rasterizer_window_begin(window_parameters_t *a1);
HFUNC void rasterizer_window_end(void);
HFUNC int rasterizer_windows_begin(void);
HFUNC void rasterizer_windows_end(void);

// obj:rasterizer_common.obj src:rasterizer/common/rasterizer_common.c
HFUNC void rasterizer_dispose_from_old_map(void);
HFUNC void rasterizer_frame_update(float);
HFUNC void rasterizer_initialize_for_new_map(void);

// obj:rasterizer_decals.obj src:?
HFUNC void rasterizer_decals_dispose(void);
HFUNC void rasterizer_decals_dispose_from_old_map(void);
HFUNC void rasterizer_decals_initialize(void);
HFUNC void rasterizer_decals_initialize_for_new_map(void);

// obj:rasterizer_xbox.obj src:rasterizer/xbox/rasterizer_xbox.c
HFUNC void rasterizer_preinitialize(void);

// obj:real_math.obj src:math/real_math.c
HFUNC void real_math_reset_precision(void);

// obj:recorded_animations.obj src:cutscene/recorded_animations.c
HFUNC void recorded_animations_dispose(void);
HFUNC void recorded_animations_dispose_from_old_map(void);
HFUNC void recorded_animations_initialize(void);
HFUNC void recorded_animations_initialize_for_new_map(void);
HFUNC void recorded_animations_update(void);

// obj:render.obj src:render/render.c
HFUNC void render_dispose(void);
HFUNC void render_frame(void *a2, __int16 a3, _WORD *a4, _WORD *a5, void *a6, float a7);
HFUNC void render_frame_pregame(pregame_render_info_t *pregame_info, void *bitmap);
HFUNC void render_frame_present(_WORD *a1, void *a2);
HFUNC void render_initialize(void);
HFUNC void render_initialize_for_new_map(void);
HFUNC void render_window(int16_t *win, void *offset_or_null);
HFUNC void render_window_pregame(int window_type, int16_t *win);

// obj:render_cameras.obj src:render/render_cameras.c
HFUNC void render_camera_build_frustum(camera_t *camera, _DWORD *a2, float *frustum, bool a4);
HFUNC double render_camera_get_adjusted_field_of_view_tangent(float a1);

// obj:saved_game_files.obj src:saved games/saved_game_files.c
HFUNC void saved_game_files_dispose(void);
HFUNC void saved_game_files_initialize(void);

// obj:scenario.obj src:scenario/scenario.c
HDATA int global_scenario_index;
HFUNC void *game_globals_get(void);
HFUNC scenario_t *global_scenario_get(void);
HFUNC void scenario_dispose_from_old_map(void);
HFUNC void scenario_frame_update(float);
HFUNC void *scenario_get(void);
HFUNC void *scenario_get(void);
HFUNC void scenario_initialize(void);
HFUNC void scenario_initialize_for_new_map(void);
HFUNC bool scenario_load(const char *map_name);
HFUNC bool scenario_switch_structure_bsp(__int16 a1);
HFUNC void scenario_unload(void);

// obj:shaders.obj src:shaders/shaders.c
HFUNC void numeric_countdown_timer_update(void);

// obj:shell.obj src:?
HFUNC bool shell_application_is_paused(void);
HFUNC void shell_dispose(void);
HFUNC int shell_initialize(void);

// obj:shell_xbox.obj src:shell_xbox.c
HFUNC int __cdecl main(int argc, const char **argv, const char **envp);
HFUNC void shell_idle(void);
HFUNC void update_loaded_module_section_attributes(void);

// obj:sound_classes.obj src:sound/sound_classes.c
HFUNC void sound_classes_dispose(void);
HFUNC void sound_classes_dispose_from_old_map(void);
HFUNC void sound_classes_initialize(void);
HFUNC void sound_classes_initialize_for_new_map(void);

// obj:sound_manager.obj src:sound/sound_manager.c
HFUNC void sound_dispose_from_old_map(void);
HFUNC void sound_initialize_for_new_map(void);
HFUNC void sound_render(void);

// obj:stack_walk_windows.obj src:cseries/stack_walk_windows.c
HFUNC char stack_walk(__int16 a1);

// obj:structures.obj src:structures/structures.c
HFUNC void structures_dispose(void);
HFUNC void structures_dispose_from_old_map(void);
HFUNC void structures_initialize(void);
HFUNC void structures_initialize_for_new_map(void);

// obj:tag_groups.obj src:?
HFUNC void *tag_block_get_element(void *block, int index, int element_size);

// obj:tags.obj src:cache/tags.c
HFUNC void *tag_get(int group_tag, int tag_index);

// obj:telnet_console.obj src:?
HFUNC void telnet_console_dispose(void);
HFUNC void telnet_console_initialize(void);
HFUNC void telnet_console_process(void);

// obj:terminal.obj src:interface/terminal.c
HFUNC void terminal_update(void);

// obj:tiff_file.obj src:bitmaps/tiff_file.c
HFUNC const char *tiff_export(file_ref_t *info, __int16 *bitmap);

// obj:transport_endpoint_set_winsock.obj src:bungie_net/network/transport_endpoint_set_winsock.c
HFUNC void transport_dispose(void);
HFUNC void transport_initialize(void);

// obj:ui_widget.obj src:interface/ui_widget.c
HFUNC void display_error_when_main_menu_loaded(int16_t);
HFUNC void main_screen_shell_load(void);
HFUNC void process_ui_widgets(void);
HFUNC void render_ui_widgets(__int16 a1, viewport_bounds_t *window_bounds);
HFUNC void ui_widget_load_progress_widget(void);
HFUNC void *ui_widget_realloc(int a1, unsigned short a2, const char *a3, unsigned int a4);
HFUNC void ui_widgets_close_all(void);
HFUNC void ui_widgets_disable_pause_game(int);
HFUNC void ui_widgets_dispose(void);
HFUNC void ui_widgets_initialize(void);
HFUNC void ui_widgets_safe_to_load(bool a1);

// obj:ui_widget_game_data_input_functions.obj src:interface/ui_widget_game_data_input_functions.c
HFUNC void ui_widget_game_data_function_invoke(void *widget, unsigned short game_data_input_reference_function);

// obj:unicode.obj src:text/unicode.c
HFUNC wchar_t *ascii_to_wide(const char *ascii, wchar_t *unicode, size_t length);
HFUNC wchar_t *ustrncpy(wchar_t *dest, wchar_t *src, size_t count);

// obj:units.obj src:units/units.c
HFUNC bool any_unit_is_dangerous(void);
HFUNC void unit_clear_seat_tag(int unit_handle);
HFUNC void unit_delete(int datum_handle);
HFUNC int unit_get_weapon(int unit_handle, int16_t weapon_index);
HFUNC bool unit_is_alive(int unit_handle);
HFUNC void unit_set_control(int unit_handle, void *unit_control);
HFUNC bool unit_set_in_vehicle(int unit_handle, int weapon_index);
HFUNC void units_update(void);

// obj:vehicles.obj src:units/vehicles.c
HFUNC bool vehicle_moving_near_any_player(void);

// obj:weather_particle_systems.obj src:effects/weather_particle_systems.c
HFUNC void weather_particle_systems_dispose(void);
HFUNC void weather_particle_systems_dispose_from_old_map(void);
HFUNC void weather_particle_systems_initialize(void);
HFUNC void weather_particle_systems_initialize_for_new_map(void);

// obj:widgets.obj src:objects/widgets/widgets.c
HFUNC void widgets_update(float);

// obj:? src:?
HDATA char byte_325714;
HDATA char byte_457068;
HDATA char byte_457069;
HDATA char byte_46BA4C[40];
HDATA char byte_46BB38[57];
HDATA char byte_46BBA0[104];
HDATA char byte_46DA28;
HDATA char byte_46DA3B;
HDATA char byte_46DA3C;
HDATA char byte_46DA45;
HDATA char byte_46DA46;
HDATA char byte_46DA47;
HDATA char byte_46DA50;
HDATA char byte_46DB54;
HDATA char byte_46DC55;
HDATA char core_name[];
HDATA int dword_46BC0C[64];
HDATA int dword_46CC44;
HDATA int *flip_count_ptr;
HDATA float flt_46DA08;
HDATA game_globals_t * game_globals;
HDATA bool game_reset_pending;
HDATA bool game_state_load_core_pending;
HDATA bool game_state_revert_pending;
HDATA bool game_state_save_core_pending;
HDATA bool game_state_save_pending;
HDATA game_time_globals_t * game_time_globals;
HDATA float global_frustum[4];
HDATA bool main_change_map_name_pending;
HDATA main_globals_t main_globals;
HDATA __int16 * main_globals_movie;
HDATA bool main_load_last_solo_map_pending;
HDATA bool main_menu_load_pending;
HDATA bool main_skip_private_pending;
HDATA bool main_won_map_private_pending;
HDATA char map_name[255];
HDATA int movie_frame_count;
HDATA player_control_globals_t * player_control_globals;
HDATA pregame_render_info_t pregame_render_info;
HDATA volatile int64_t qword_325678;
HDATA bool should_skip_cinematic;
HDATA wchar_t ui_widget_game_data_build_version_wide_str[128];
HDATA void (*ui_widget_game_data_function_table[])(void *widget);
HDATA unk_time_globals_t unk_time_globals;
HDATA camera_t unknown_global_camera;
HDATA window_t window[5];
HDATA short word_46BC08;
HDATA short word_46BC0A;
HDATA __int16 word_46BFC4[4];
HDATA __int16 word_46DA0C;
HDATA short word_46DA40;
HDATA short word_46DA4C;
HDATA short word_46DA4E;
HDATA short word_46DDDC;
HDATA short word_46DDDE[15];
HDATA bool xbox_demos_launch_pending;
HFUNC void *XPhysicalAlloc(size_t size, unsigned int addr, unsigned int alignment, unsigned int protect);
HFUNC void j__render_dispose_from_old_map(void);
HFUNC int snprintf(char *str, size_t size, const char *format, ...);
HFUNC void ui_widget_game_data_build_version(int a2);


#undef HFUNC
#undef HDATA

//
// AUTOMATICALLY GENERATED. DO NOT EDIT.
//
