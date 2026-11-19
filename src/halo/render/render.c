#include "../../shader_model.h"

#define k_real_zero (*(const float *)0x2533c0)
#define k_real_one (*(const float *)0x2533c8)
#define render_contrails_enabled (*(boolean *)0x32574a)
#define render_particles_enabled (*(boolean *)0x32574b)
#define render_particle_systems_enabled (*(boolean *)0x32574c)
#define render_weather_particle_systems_enabled (*(boolean *)0x32574d)
#define transparent_geometry_group_index (*(int16_t *)0x4d0cb8)
#define transparent_geometry_group_pending_flags ((uint32_t *)0x4d0cbc)
#define transparent_geometry_groups \
  (*(struct transparent_geometry_group **)0x4d0cec)
#define transparent_geometry_groups2 \
  (*(struct transparent_geometry_group **)0x4d0cf0)
#define transparent_geometry_group_count (*(int32_t *)0x4d0cf4)
#define transparent_geometry_group_count2 (*(int32_t *)0x4d0cf8)
#define transparent_geometry_group_sorted_indices (*(int16_t **)0x4d0cfc)
/* Absolute form: the kb extern leaves an unpaired relocation. */
#define global_window_parameters \
  (*(rasterizer_window_begin_parameters *)0x5a5bc0)
#define rasterizer_transparent_pixel_counter_active (*(boolean *)0x325740)
#define rasterizer_first_person_weapon_near_clip_distance (*(real *)0x32569c)
#define rasterizer_first_person_weapon_far_clip_distance (*(real *)0x3256a0)

#define RASTERIZER_TRANSPARENT_GEOMETRY_FILE \
  "c:\\halo\\SOURCE\\rasterizer\\rasterizer_transparent_geometry.c"

/* rasterizer_profile_begin/end indices; NUMBER_OF_RASTERIZER_PROFILES (0x1d)
 * bounds the profile table. */
enum {
  _rasterizer_profile_water = 0x13,
  _rasterizer_profile_queued_transparents = 0x16,
  NUMBER_OF_RASTERIZER_PROFILES = 0x1d
};

enum {
  _shader_type_transparent_water = 7
};

enum {
  _rasterizer_geometry_first_person_bit = 7
};

/* The 2276 assert strings name the struct, transparent_geometry_groups and
 * sorted_index; the element stride is 0xa0. */
struct transparent_geometry_group {
  dword geometry_flags;                 ///< offset=0x00
  byte pad_04[4];                       ///< offset=0x04
  int32_t source_object_index;          ///< offset=0x08
  shader_base *shader;                  ///< offset=0x0c
  byte pad_10[0x44];                    ///< offset=0x10
  int32_t dynamic_vertex_buffer_index;  ///< offset=0x54
  vertex_buffer *vertex_buffer;         ///< offset=0x58
  byte pad_5c[0x14];                    ///< offset=0x5c
  real z_sort;                          ///< offset=0x70
  byte pad_74[0x1c];                    ///< offset=0x74
  int32_t sorted_index;                 ///< offset=0x90
  byte pad_94[9];                       ///< offset=0x94
  boolean field_9d;                     ///< offset=0x9d
  byte pad_9e[2];                       ///< offset=0x9e
};
cs(struct transparent_geometry_group, 0xa0);
co(struct transparent_geometry_group, source_object_index, 0x08);
co(struct transparent_geometry_group, shader, 0x0c);
co(struct transparent_geometry_group, dynamic_vertex_buffer_index, 0x54);
co(struct transparent_geometry_group, vertex_buffer, 0x58);
co(struct transparent_geometry_group, z_sort, 0x70);
co(struct transparent_geometry_group, sorted_index, 0x90);
co(struct transparent_geometry_group, field_9d, 0x9d);

#define RASTERIZER_TARGET_RENDER_PRIMARY_WIDTH 640
#define RASTERIZER_TARGET_RENDER_PRIMARY_HEIGHT 480

enum {
  _render_planar_fog_mode_fully_fogged = 2
};

enum {
  _decal_layer_primary,
  _decal_layer_secondary,
  _decal_layer_light,
  _decal_layer_alpha_tested,
  _decal_layer_water
};

struct render_fog {
  byte pad_00[2];                     ///< offset=0x00
  word runtime_flags;                 ///< offset=0x02
  real_rgb_color atmospheric_color;   ///< offset=0x04
  real atmospheric_maximum_density;   ///< offset=0x10
  byte pad_14[4];                     ///< offset=0x14
  real atmospheric_maximum_distance;  ///< offset=0x18
  int16_t planar_mode;                ///< offset=0x1c
  byte pad_1e[0x22];                  ///< offset=0x1e
  real planar_maximum_distance;       ///< offset=0x40
  byte pad_44[0xc];                   ///< offset=0x44
};
cs(struct render_fog, 0x50);
co(struct render_fog, runtime_flags, 0x02);
co(struct render_fog, atmospheric_maximum_density, 0x10);
co(struct render_fog, atmospheric_maximum_distance, 0x18);
co(struct render_fog, planar_mode, 0x1c);
co(struct render_fog, planar_maximum_distance, 0x40);

/* Typed view of window_parameters_t, the block rasterizer_window_begin copies
 * to global_window_parameters. */
typedef struct render_window_parameters {
  int16_t rasterizer_target;
  int16_t window_index;
  boolean has_mirror;
  boolean field_05;
  int16_t pad_06;
  camera_t camera;
  render_frustum_t frustum;
  struct render_fog fog;
  byte screen_flash[0x20];
} render_window_parameters_t;
cs(render_window_parameters_t, 0x258);

/* structure_visibility_find_mirror output: the mirror and its cluster. */
typedef struct render_reflection {
  render_mirror_t mirror;
  int16_t cluster_index;
  int16_t pad_1a;
} render_reflection_t;
cs(render_reflection_t, 0x1c);

/* The 2276 asserts name window->render_camera and window->rasterizer_camera;
 * render_frame steps windows by 0xac. */
struct render_window {
  int16_t local_player_index;  ///< offset=0x00
  boolean console_window;      ///< offset=0x02
  byte pad_03;                 ///< offset=0x03
  camera_t render_camera;      ///< offset=0x04
  camera_t rasterizer_camera;  ///< offset=0x58
};
cs(struct render_window, 0xac);
co(struct render_window, render_camera, 0x04);
co(struct render_window, rasterizer_camera, 0x58);

/* Fields of render (0x506540); the absolute form keeps the relocations
 * paired. */
#define render_frame_index (*(int32_t *)0x506540)
#define render_scene_index (*(int32_t *)0x506544)
#define render_window_index (*(int16_t *)0x50654a)
#define render_time_delta_since_tick (*(real *)0x50654c)
#define render_fog_parameters (*(struct render_fog *)0x506730)
#define render_cluster_index (*(int32_t *)0x506784)
#define render_visible_sky_index (*(int16_t *)0x50678a)
#define render_visible_cluster_bits ((unsigned int *)0x50678c)
#define render_rendered_clusters ((char *)0x5067cc)
#define RENDERED_CLUSTER_SIZE 0x1a0
#define render_rendered_cluster_count (*(int16_t *)0x5137cc)
#define render_invalid_fog_warning_displayed (*(boolean *)0x4d0d02)
#define global_screenshot_count (*(int16_t *)0x31fa98)
#define global_screenshot_size (*(int16_t *)0x46e008)

char rasterizer_transparent_geometry_get_group_pending_status(void *group)
{
  short presorted_index;

  presorted_index = rasterizer_transparent_geometry_group_to_presorted_index(
    (unsigned int)group);
  if (presorted_index != NONE) {
    return (char)((FLAG(presorted_index & 0x1f) &
                   transparent_geometry_group_pending_flags[presorted_index >>
                                                            5]) == 0);
  }
  return 1;
}

void rasterizer_transparent_geometry_set_group_pending_status(void *group,
                                                              int pending)
{
  short group_presorted_index;
  int index;

  group_presorted_index =
    rasterizer_transparent_geometry_group_to_presorted_index(
      (unsigned int)group);
  if (group_presorted_index == NONE) {
    return;
  }
  index = group_presorted_index;
  if ((char)pending == 0) {
    transparent_geometry_group_pending_flags[index >> 5] =
      transparent_geometry_group_pending_flags[index >> 5] |
      FLAG(index & 0x1f);
    return;
  }
  transparent_geometry_group_pending_flags[index >> 5] =
    transparent_geometry_group_pending_flags[index >> 5] & ~FLAG(index & 0x1f);
}

short rasterizer_transparent_geometry_get_primary_vertex_type(
  void *group_pointer)
{
  struct transparent_geometry_group *group;
  vertex_buffer *primary_vertex_buffer;
  int dynamic_vertex_buffer_index;

  group = group_pointer;
  if (group == NULL) {
    display_assert("group", RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0xf4, 1);
    system_exit(-1);
  }
  primary_vertex_buffer = group->vertex_buffer;
  if (primary_vertex_buffer != NULL) {
    return primary_vertex_buffer->type;
  }
  dynamic_vertex_buffer_index = group->dynamic_vertex_buffer_index;
  if (dynamic_vertex_buffer_index != NONE) {
    return rasterizer_dynamic_vertices_get_type(dynamic_vertex_buffer_index);
  }
  error(2, "### ERROR transparent geometry group has no vertices");
  return NONE;
}

void rasterizer_transparent_geometry_end(void)
{
}

void rasterizer_transparent_geometry_dispose(void)
{
  rasterizer_transparent_geometry_dispose_aux_buffer();
  if (transparent_geometry_groups != NULL) {
    debug_free(transparent_geometry_groups,
               RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x111);
  }
  transparent_geometry_groups = NULL;
  if (transparent_geometry_group_sorted_indices != NULL) {
    debug_free(transparent_geometry_group_sorted_indices,
               RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x114);
  }
  transparent_geometry_group_sorted_indices = NULL;
  if (transparent_geometry_groups2 != NULL) {
    debug_free(transparent_geometry_groups2,
               RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x118);
  }
  transparent_geometry_groups2 = NULL;
  transparent_geometry_group_count2 = 0;
  transparent_geometry_group_count = 0;
}

void rasterizer_transparent_geometry_stop(void)
{
  rasterizer_set_stencil_mode(0);
}

int group_sorted_indices_cmpfn(const void *a, const void *b)
{
  const int16_t *group_index1;
  const int16_t *group_index2;
  struct transparent_geometry_group *group1;
  struct transparent_geometry_group *group2;
  int result;
  int first_person1;
  char last1;
  char last2;

  group_index1 = a;
  group_index2 = b;
  result = 0;
  if (group_index1 == NULL || *group_index1 < 0 ||
      *group_index1 >= transparent_geometry_group_count) {
    display_assert("group_index1 && (*group_index1)>=0 && "
                   "(*group_index1)<transparent_geometry_group_count",
                   RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x1aa, 1);
    system_exit(-1);
  }
  if (group_index2 == NULL || *group_index2 < 0 ||
      *group_index2 >= transparent_geometry_group_count) {
    display_assert("group_index2 && (*group_index2)>=0 && "
                   "(*group_index2)<transparent_geometry_group_count",
                   RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x1ab, 1);
    system_exit(-1);
  }
  group1 = transparent_geometry_groups + *group_index1;
  group2 = transparent_geometry_groups + *group_index2;
  if (shader_is_water_decal(group1->shader)) {
    result = -1;
  } else if (shader_is_water_decal(group2->shader)) {
    result = 1;
  } else if (group1->shader != NULL &&
             group1->shader->type == _shader_type_transparent_water) {
    result = -1;
  } else if (group2->shader != NULL &&
             group2->shader->type == _shader_type_transparent_water) {
    result = 1;
  } else {
    first_person1 = group1->geometry_flags &
                    FLAG(_rasterizer_geometry_first_person_bit);
    if (first_person1 != 0 && (group2->geometry_flags & FLAG(_rasterizer_geometry_first_person_bit)) == 0) {
      result = 1;
    } else if (first_person1 == 0 &&
               (group2->geometry_flags &
                FLAG(_rasterizer_geometry_first_person_bit)) != 0) {
      result = -1;
    } else if (group1->z_sort > group2->z_sort) {
      result = 1;
    } else if (group1->z_sort < group2->z_sort) {
      result = -1;
    } else if (group1->source_object_index > group2->source_object_index) {
      result = 1;
    } else if (group1->source_object_index < group2->source_object_index) {
      result = -1;
    }
  }
  last1 = group1->field_9d;
  last2 = group2->field_9d;
  if (last1 != 0 && last2 == 0)
    return 1;
  if (last1 == 0 && last2 != 0)
    return -1;
  return result;
}

void rasterizer_sort_internal(void)
{
  int count;
  int16_t index;
  int16_t *sorted_indices;
  struct transparent_geometry_group *groups;

  count = transparent_geometry_group_count;
  index = 0;
  if (count > 0) {
    do {
      if (transparent_geometry_groups + index == NULL) {
        display_assert("group", RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x192,
                       1);
        system_exit(-1);
      }
      transparent_geometry_group_sorted_indices[index] = index;
      index++;
    } while (index < count);
  }
  qsort(transparent_geometry_group_sorted_indices, (size_t)count,
        sizeof(*transparent_geometry_group_sorted_indices),
        group_sorted_indices_cmpfn);
  count = transparent_geometry_group_count;
  index = 0;
  if (count > 0) {
    groups = transparent_geometry_groups;
    sorted_indices = transparent_geometry_group_sorted_indices;
    do {
      groups[sorted_indices[index]].sorted_index = index;
      index++;
    } while (index < count);
  }
}

void rasterizer_transparent_geometry_draw(char water)
{
  char first_person_flag;
  struct transparent_geometry_group *group;
  int group_index;
  int profile;
  shader_base *shader;

  profile = water != 0 ? _rasterizer_profile_water
                       : _rasterizer_profile_queued_transparents;
  rasterizer_profile_begin(profile);
  if (transparent_geometry_group_count > 0) {
    first_person_flag = 0;
    if (water != 0) {
      rasterizer_sort_internal();
      transparent_geometry_group_index = 0;
      if (global_window_parameters.window_index != NONE) {
        rasterizer_transparent_pixel_counter_active = 1;
      }
    }
    rasterizer_transparent_geometry_groups_begin();
    rasterizer_transparent_pixel_counter_active = 0;
    while ((int)transparent_geometry_group_index <
           transparent_geometry_group_count) {
      group_index = transparent_geometry_group_sorted_indices
        [transparent_geometry_group_index];
      group = transparent_geometry_groups + group_index;
      if (water != 0) {
        shader = group->shader;
        if (shader == NULL ||
            (shader->type != _shader_type_transparent_water &&
             shader_is_water_decal(shader) == 0)) {
          break;
        }
      }
      if ((group->geometry_flags &
           FLAG(_rasterizer_geometry_first_person_bit)) != 0) {
        if (water != 0) {
          display_assert("!water", RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x154,
                         1);
          system_exit(-1);
        }
        if (global_window_parameters.rasterizer_target !=
            _rasterizer_target_render_primary) {
          display_assert(
            "global_window_parameters.rasterizer_target==_rasterizer_target_"
            "render_primary",
            RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x155, 1);
          system_exit(-1);
        }
        if (first_person_flag == 0) {
          rasterizer_set_stencil_mode(0);
          rasterizer_set_frustum_z(
            rasterizer_first_person_weapon_near_clip_distance,
            rasterizer_first_person_weapon_far_clip_distance);
          first_person_flag = 1;
        }
      } else if (first_person_flag != 0) {
        display_assert("!first_person_flag",
                       RASTERIZER_TRANSPARENT_GEOMETRY_FILE, 0x163, 1);
        system_exit(-1);
      }
      rasterizer_transparent_geometry_group_draw(group, 0);
      transparent_geometry_group_index = transparent_geometry_group_index + 1;
    }
    if (water == 0 && global_window_parameters.window_index != NONE) {
      rasterizer_transparent_pixel_counter_active = 1;
    }
    rasterizer_transparent_geometry_groups_end();
    rasterizer_transparent_pixel_counter_active = 0;
    if (first_person_flag != 0) {
      rasterizer_set_frustum_z(0.0f, 0.0f);
    }
  }
  rasterizer_profile_end(profile);
}

void render_effects(int enable)
{
  render_weather_particle_systems_enabled = (boolean)enable;
  render_particle_systems_enabled = (boolean)enable;
  render_particles_enabled = (boolean)enable;
  render_contrails_enabled = (boolean)enable;
}

void render_initialize(void)
{
  render_objects_initialize();
}

void render_initialize_for_new_map(void)
{
  render_objects_initialize_for_new_map();
}

void j__render_dispose_from_old_map(void)
{
  render_objects_dispose_from_old_map();
}

void render_dispose(void)
{
  render_objects_dispose();
}

void render_window_pregame(int window_type, int16_t *win)
{
  render_window_parameters_t window_params;
  struct render_window *window;

  window = (struct render_window *)win;
  profile_render_window_start(0);
  csmemset(&window_params, 0, sizeof(window_params));

  unknown_global_camera = window->render_camera;
  render_camera_build_frustum(&unknown_global_camera, 0, &global_frustum, 1);

  window_params.camera = window->rasterizer_camera;
  render_camera_build_frustum(&window_params.camera, 0, &window_params.frustum,
                              1);

  /* set up scene parameters */
  window_params.rasterizer_target = 0;
  window_params.window_index = NONE;
  window_params.field_05 = (window_type == 0);

  rasterizer_window_begin((window_parameters_t *)&window_params);

  switch (window_type) {
  case 0:
    interface_draw_fullscreen_overlays();
    rasterizer_debug_draw();
    break;
  case 1:
    game_engine_nonplayer_post_rasterize();
    break;
  default:
    display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\render\\render.c",
                   0x11f, 1);
    system_exit(-1);
    break;
  }

  rasterizer_window_end();
  profile_render_window_end();
}

void render_frame_pregame(pregame_render_info_t *pregame_info,
                          void *main_globals_movie)
{
  render_window_parameters_t window_parameters;
  float elapsed[2];
  float progress;

  ++render.frame_index;
  rasterizer_frame_begin(elapsed);
  rasterizer_windows_begin();
  profile_render_window_start(0);
  csmemset(&window_parameters, 0, sizeof(window_parameters));

  unknown_global_camera = pregame_info->cam0;
  render_camera_build_frustum(&unknown_global_camera, 0, &global_frustum, 1);

  window_parameters.camera = pregame_info->cam1;
  render_camera_build_frustum(&window_parameters.camera, 0,
                              &window_parameters.frustum, 1);
  window_parameters.rasterizer_target = 0;
  rasterizer_window_begin((window_parameters_t *)&window_parameters);
  render_ui_widgets(0, &pregame_info->cam1.viewport_bounds);
  bink_playback_render();
  if (game_map_loading_in_progress(&progress)) {
    progress_bar_display(progress);
  }
  rasterizer_window_end();
  profile_render_window_end();
  rasterizer_windows_end();
  rasterizer_frame_end();
}

void render_frame_present(_WORD *a1, void *a2)
{
  rasterizer_present(a2, a1);
}

/* keep: an int-returning test (NEG/SBB/NEG); a char `!= 0` compiles to
 * TEST/SETNE. */
static __inline int render_bit_vector_test(const unsigned int *vector,
                                           int bit)
{
  return ((1 << (bit & 0x1f)) & vector[bit >> 5]) != 0;
}

char render_location_visible(const location_t *location)
{
  if (location->cluster_index < 0 ||
      location->cluster_index >= global_structure_bsp_get()->clusters.count) {
    display_assert("location->cluster_index>=0 && "
                   "location->cluster_index<global_structure_bsp_get()->"
                   "clusters.count",
                   "c:\\halo\\SOURCE\\render\\render.c", 0x248, 1);
    system_exit(-1);
  }
  return (char)render_bit_vector_test(render_visible_cluster_bits,
                                      location->cluster_index);
}

void *rendered_cluster_get(int rendered_cluster_index)
{
  int16_t index;

  index = (int16_t)rendered_cluster_index;
  if (index < 0 || index >= render_rendered_cluster_count) {
    display_assert("rendered_cluster_index>=0 && "
                   "rendered_cluster_index<render.rendered_cluster_count",
                   "c:\\halo\\SOURCE\\render\\render.c", 0x250, 1);
    system_exit(-1);
  }
  return render_rendered_clusters + index * RENDERED_CLUSTER_SIZE;
}

void render_scene(int16_t player_index, camera_t *render_cam,
                  render_frustum_t *render_frustum, camera_t *rasterizer_cam,
                  render_frustum_t *rasterizer_frustum, int16_t pass_type,
                  char reflected)
{
  render_window_parameters_t window_params;
  int16_t i;

  profile_render_window_start(1);
  render_scene_index = render_scene_index + 1;
  csmemset(&window_params, 0, sizeof(window_params));
  render.local_player_index = player_index;
  unknown_global_camera = *render_cam;
  global_frustum = *render_frustum;
  window_params.camera = *rasterizer_cam;
  window_params.frustum = *rasterizer_frustum;
  window_params.rasterizer_target = pass_type;
  window_params.has_mirror = reflected;
  window_params.window_index = render_window_index;
  window_params.fog = render_fog_parameters;
  render_structure_visibility();
  player_effect_get_screen_flash(player_index,
                                 window_params.screen_flash);
  rasterizer_window_begin((window_parameters_t *)&window_params);

  if (!bink_playback_has_video()) {
    scenario_fog_region_get_fog_index();
    render_sky();
    first_person_weapon_render_update();
    lights_preprocess_scene();
    render_objects();
    structure_render_preprocess();
    structure_render_lightmaps();
    rasterizer_lens_flares_submit_occlusion_tests();
    scenario_test_pas();
    lights_render_diffuse();

    rasterizer_decals_begin(_decal_layer_light);
    for (i = 0; i < render_rendered_cluster_count; i++) {
      rasterizer_decals_draw(*(uint16_t *)rendered_cluster_get(i));
    }
    rasterizer_decals_end();

    rasterizer_decals_begin(_decal_layer_alpha_tested);
    for (i = 0; i < render_rendered_cluster_count; i++) {
      rasterizer_decals_draw(*(uint16_t *)rendered_cluster_get(i));
    }
    rasterizer_decals_end();

    structure_render_diffuse_texture();

    rasterizer_decals_begin(_decal_layer_primary);
    for (i = 0; i < render_rendered_cluster_count; i++) {
      rasterizer_decals_draw(*(uint16_t *)rendered_cluster_get(i));
    }
    rasterizer_decals_end();

    rasterizer_decals_begin(_decal_layer_secondary);
    for (i = 0; i < render_rendered_cluster_count; i++) {
      rasterizer_decals_draw(*(uint16_t *)rendered_cluster_get(i));
    }
    rasterizer_decals_end();

    lights_render_specular();
    structure_render_specular_lightmaps();
    structure_render_reflection_lightmap_masks();
    structure_render_reflection_mirrors();
    structure_render_reflections();
    structure_render_transparent_geometry();
    structure_render_fog();
    get_postgame_hilite_colors();
    weather_particle_systems_render();
    render_particles();
    particle_systems_render();
    render_contrails_normal();
    rasterizer_transparent_geometry_draw(1);

    rasterizer_decals_begin(_decal_layer_water);
    for (i = 0; i < render_rendered_cluster_count; i++) {
      rasterizer_decals_draw(*(uint16_t *)rendered_cluster_get(i));
    }
    rasterizer_decals_end();

    structure_render_detail_objects();
    rasterizer_transparent_geometry_draw(0);
    rasterizer_transparent_geometry_stop();
    structure_render_fog_screen();
    rasterizer_lens_flares_draw();
    interface_draw_screen();
    rasterizer_screen_flash();
    render_ui_widgets(player_index,
                      &rasterizer_cam->viewport_bounds);
  }

  bink_playback_render();
  render_camera_debug_frustum(&unknown_global_camera, &global_frustum);
  render_debug();
  editor_render();
  rasterizer_debug_draw();
  rasterizer_window_end();
  profile_render_window_end();
}

void render_window(int16_t *win, void *offset_or_null)
{
  struct render_window *window = (struct render_window *)win;
  camera_t *render_cam = &window->render_camera;
  camera_t *rasterizer_cam = &window->rasterizer_camera;
  real_rectangle2d frustum_bounds;
  char rendered_reflection = 0;
  render_reflection_t reflection;
  camera_t reflection_cam;
  render_frustum_t render_frustum;
  render_frustum_t rasterizer_frustum;
  render_frustum_t reflection_frustum;

  /* initialize render pass for this camera */
  structure_visibility_find_camera(render_cam);

  /* update render globals from scene */
  render_fog_parameters.runtime_flags = 0;
  scenario_get_atmospheric_fog(
    window->local_player_index, (int)(uint16_t)render_visible_sky_index,
    &render_cam->position, &render_fog_parameters);
  FUN_00198f10((int)(uint16_t)render_cluster_index, &render_fog_parameters);

  /* track closest fog distance when BSP index is unknown */
  if (render_fog_parameters.atmospheric_maximum_distance !=
      k_real_zero) {
    if (render_visible_sky_index == NONE &&
        render_fog_parameters.planar_maximum_distance >
          render_fog_parameters.atmospheric_maximum_distance) {
      render_fog_parameters.planar_maximum_distance =
        render_fog_parameters.atmospheric_maximum_distance;
    }
  }

  /* clamp z_far to fog distance when fog type matches */
  if (render_fog_parameters.atmospheric_maximum_density == k_real_one &&
      render_fog_parameters.atmospheric_maximum_distance !=
        k_real_zero) {
    window->render_camera.z_far =
      window->render_camera.z_far >
          render_fog_parameters.atmospheric_maximum_distance ?
        render_fog_parameters.atmospheric_maximum_distance :
        window->render_camera.z_far;
  }

  /* clamp z_far to closest fog in mode 2 */
  if (render_fog_parameters.planar_mode ==
        _render_planar_fog_mode_fully_fogged &&
      render_fog_parameters.planar_maximum_distance != k_real_zero) {
    window->render_camera.z_far =
      window->render_camera.z_far >
          render_fog_parameters.planar_maximum_distance ?
        render_fog_parameters.planar_maximum_distance :
        window->render_camera.z_far;
  }

  /* fog sanity: z_far must exceed z_near */
  if (window->render_camera.z_far <= window->render_camera.z_near) {
    if (!render_invalid_fog_warning_displayed) {
      error(2, "### ERROR something is wrong with the fog in the "
               "sky tag or the fog tag");
      render_invalid_fog_warning_displayed = 1;
    }
    window->render_camera.z_far = window->render_camera.z_near + 0.01f;
  }

  /* assert viewport and window bounds match between cameras */
  if (csmemcmp(&window->render_camera.viewport_bounds,
               &window->rasterizer_camera.viewport_bounds,
               sizeof(viewport_bounds_t)) != 0) {
    display_assert("!memcmp(&window->render_camera.viewport_bounds, "
                   "&window->rasterizer_camera.viewport_bounds, "
                   "sizeof(rectangle2d))",
                   "c:\\halo\\SOURCE\\render\\render.c", 0xbb, 1);
    system_exit(-1);
  }
  if (csmemcmp(&window->render_camera.window_bounds,
               &window->rasterizer_camera.window_bounds,
               sizeof(viewport_bounds_t)) != 0) {
    display_assert("!memcmp(&window->render_camera.window_bounds, "
                   "&window->rasterizer_camera.window_bounds, "
                   "sizeof(rectangle2d))",
                   "c:\\halo\\SOURCE\\render\\render.c", 0xbc, 1);
    system_exit(-1);
  }

  render_camera_build_frustum_bounds(render_cam, &frustum_bounds);

  if (offset_or_null != NULL) {
    const int16_t *screenshot_combined_index = offset_or_null;
    int tile_count;

    tile_count = global_screenshot_count * global_screenshot_size;
    if (tile_count > 0) {
      real tile_width;
      real tile_height;
      real_rectangle2d adjusted_bounds;

      tile_width = (frustum_bounds.x1 - frustum_bounds.x0) / (real)tile_count;
      tile_height = (frustum_bounds.y1 - frustum_bounds.y0) / (real)tile_count;
      adjusted_bounds.x0 =
        (real)screenshot_combined_index[0] * tile_width + frustum_bounds.x0;
      adjusted_bounds.y0 =
        (real)(tile_count - screenshot_combined_index[1] - 1) * tile_height +
        frustum_bounds.y0;
      adjusted_bounds.x1 = adjusted_bounds.x0 + tile_width;
      adjusted_bounds.y1 = adjusted_bounds.y0 + tile_height;
      frustum_bounds = adjusted_bounds;
    }
  }

  /* build projection frustums for both cameras */
  render_camera_build_frustum(render_cam, &frustum_bounds, &render_frustum, 1);
  render_camera_build_frustum(rasterizer_cam, &frustum_bounds,
                              &rasterizer_frustum, 1);

  /* reflection rendering (single player local only) */
  if (game_connection() == 1) {
    char has_refl = structure_visibility_find_mirror(render_cam, &render_frustum,
                                                     &reflection);
    if (has_refl) {
      int saved_bsp = render_cluster_index;

      /* reflection requires full-screen viewport */
      if (window->render_camera.viewport_bounds.x0 != 0) {
        display_assert("window->render_camera.viewport_bounds.x0==0",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe1, 1);
        system_exit(-1);
      }
      if (window->render_camera.viewport_bounds.y0 != 0) {
        display_assert("window->render_camera.viewport_bounds.y0==0",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe2, 1);
        system_exit(-1);
      }
      if (window->render_camera.viewport_bounds.x1 !=
          RASTERIZER_TARGET_RENDER_PRIMARY_WIDTH) {
        display_assert("window->render_camera.viewport_bounds.x1=="
                       "RASTERIZER_TARGET_RENDER_PRIMARY_WIDTH",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe3, 1);
        system_exit(-1);
      }
      if (window->render_camera.viewport_bounds.y1 !=
          RASTERIZER_TARGET_RENDER_PRIMARY_HEIGHT) {
        display_assert("window->render_camera.viewport_bounds.y1=="
                       "RASTERIZER_TARGET_RENDER_PRIMARY_HEIGHT",
                       "c:\\halo\\SOURCE\\render\\render.c", 0xe4, 1);
        system_exit(-1);
      }

      /* build reflection camera and frustum */
      render_camera_mirror(render_cam, &reflection.mirror,
                           &reflection_cam);
      render_camera_build_frustum(&reflection_cam, &frustum_bounds, &reflection_frustum,
                                  1);

      /* render reflection to secondary target */
      rasterizer_profile_enable(0);
      render_cluster_index = reflection.cluster_index;

      render_scene(-1, &reflection_cam, &reflection_frustum, &reflection_cam,
                   &reflection_frustum, 1, 0);

      /* restore BSP and switch back to main render target */
      render_cluster_index = saved_bsp;
      rasterizer_profile_enable(1);

      rendered_reflection = 1;
    }
  }

  render_scene(window->local_player_index, render_cam, &render_frustum,
               rasterizer_cam, &rasterizer_frustum, 0,
               (char)rendered_reflection);
}

void render_frame(void *a2, int16_t a3, const int16_t *a4, const int16_t *a5, void *a6,
                  float a7)
{
  int16_t i;
  float elapsed[2];
  int16_t *win;
  int tick;
  int16_t offset[2];

  render_frame_index += 1;
  render_time_delta_since_tick = a7;
  csmemset(elapsed, 0, 8);
  tick = game_time_get();
  elapsed[0] = (float)tick * (1.0f / 30.0f);
  rasterizer_frame_begin(elapsed);
  rasterizer_windows_begin();
  win = a2;
  for (i = 0; i < a3; i++) {
    render_window_index = i;
    if ((char)win[1] != '\0') {
      render_window_pregame(0, win);
    } else if (win[0] == -1) {
      render_window_pregame(1, win);
    } else {
      if (a5 != NULL && a4 != NULL) {
        offset[0] =
          (int16_t)(a4[0] * global_screenshot_count + a5[0]);
        offset[1] = (int16_t)(a4[1] * global_screenshot_count + a5[1]);
      }
      render_window(win, a5 != NULL ? offset : NULL);
    }
    win += 0x56;
  }
  progress_bar_eachframe();
  rasterizer_windows_end();
  rasterizer_frame_end();
}
