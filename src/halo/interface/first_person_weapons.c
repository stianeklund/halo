/* Allocate the first-person weapons game state block (0xdc750).
 * Reserves 0x7a80 bytes (4 slots of 0x1ea0 each) via game_state_malloc.
 * Asserts on allocation failure. */
void FUN_000dc750(void)
{
  *(void **)0x46bea8 = game_state_malloc("first person weapons", 0, 0x7a80);
  if (*(void **)0x46bea8 == 0) {
    display_assert("first_person_weapons",
                   "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0xf0,
                   1);
    system_exit(-1);
  }
}

/* 0xdc790 — first_person_weapons_dispose */
void first_person_weapons_dispose(void)
{
}

/* Initialize (clear) all 4 first-person weapon slots (0xdc7a0).
 * Each slot is 0x1ea0 bytes. After zeroing, sets sentinel values:
 *   slot+0x04 = -1 (0xffffffff)
 *   slot+0x1e98 = -1 (0xffffffff)
 *   slot+0x1e9c = -1 (0xffff, 16-bit) */
void first_person_weapons_initialize_for_new_map(void)
{
  int i;
  int offset;
  int base;

  offset = 0;
  i = 4;
  base = *(int *)0x46bea8;
  do {
    csmemset((void *)(base + offset), 0, 0x1ea0);
    base = *(int *)0x46bea8;
    *(int *)(offset + 4 + base) = -1;
    *(int *)(offset + 0x1e98 + base) = -1;
    *(short *)(offset + 0x1e9c + base) = -1;
    offset += 0x1ea0;
    i--;
  } while (i != 0);
}

/* Dispose first-person weapons from old map (0xdc7f0, stub). */
void FUN_000dc7f0(void)
{
}

/* Map a first-person weapon state to an animation graph index (0xdc8c0).
 * Pure lookup table: 24 states (0..23) map to animation indices; any
 * out-of-range state returns -1. The return is 32-bit (every arm is
 * `mov eax, imm`, the default `or eax, -1`; PAL-2342 declares it long), and
 * the cases follow the original's case-body order behind the jump table at
 * 0xdc964, not numeric order. */
int first_person_animation_type_from_weapon_state(int16_t state)
{
  switch (state) {
  case 0:
    return 0;
  case 3:
    return 9;
  case 4:
    return 0xc;
  case 5:
    return 1;
  case 6:
    return 2;
  case 7:
    return 0xe;
  case 8:
    return 0x12;
  case 9:
    return 0x13;
  case 10:
    return 0xd;
  case 11:
    return 5;
  case 12:
    return 6;
  case 13:
    return 7;
  case 14:
    return 8;
  case 18:
    return 0xb;
  case 19:
    return 0xa;
  case 20:
    return 0x10;
  case 21:
    return 0x14;
  case 1:
    return 0x15;
  case 2:
    return 0x16;
  case 15:
    return 0x17;
  case 16:
    return 0x18;
  case 17:
    return 0x19;
  case 22:
    return 0x1a;
  case 23:
    return 0x1b;
  default:
    return -1;
  }
}

/* Try to play a third-person weapon sound for an object event (0xdc9d0).
 * When no local player owns the weapon, this function looks up the weapon's
 * animation graph tag, maps the event through two state-translation tables
 * (FUN_000dc800 and first_person_animation_type_from_weapon_state), resolves
 * the animation's sound effect tag reference, and plays it at the global origin
 * with default forward. */
void weapon_play_first_person_weapon_sound(int param_2, int object_handle)
{
  int16_t state;
  int16_t anim_index;
  char *weapon_tag;
  int anim_graph_tag_index;
  char *antr_tag;
  char *block_element;
  int16_t lookup_result;
  char *anim_element;
  char *sound_element;
  int sound_tag_index;

  if (object_handle == -1)
    return;
  if ((int16_t)param_2 == -1)
    return;
  if (!object_try_and_get_and_verify_type(object_handle, 4))
    return;

  {
    int *weapon_obj = (int *)object_get_and_verify_type(object_handle, 4);
    weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  }

  anim_graph_tag_index = *(int *)(weapon_tag + 0x478);
  if (anim_graph_tag_index == -1)
    return;

  state = FUN_000dc800(param_2);
  if (state == -1)
    return;

  anim_index = first_person_animation_type_from_weapon_state(state);
  if (anim_index == -1)
    return;

  antr_tag = (char *)tag_get(0x616e7472, anim_graph_tag_index);

  if (*(int *)(antr_tag + 0x48) == 0) {
    block_element = NULL;
  } else {
    block_element = (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
  }

  if (anim_index < 0 || (int)anim_index >= *(int *)(block_element + 0x10))
    return;

  lookup_result =
    *(int16_t *)(*(int *)(block_element + 0x14) + (int)anim_index * 2);
  if (lookup_result == -1)
    return;

  anim_element =
    (char *)tag_block_get_element(antr_tag + 0x74, (int)lookup_result, 0xb4);
  if (*(int16_t *)(anim_element + 0x3c) == -1)
    return;

  sound_element = (char *)tag_block_get_element(
    antr_tag + 0x54, (int)*(int16_t *)(anim_element + 0x3c), 0x14);
  sound_tag_index = *(int *)(sound_element + 0xc);
  if (sound_tag_index == -1)
    return;

  {
    float *position = *(float **)0x31fc1c;
    float *forward = *(float **)0x31fc3c;
    object_impulse_sound_new(object_handle, sound_tag_index, -1, position,
                             forward, 1.0f);
  }
}

/* Return the first-person weapon state block for a local player (0xdcaf0).
 * local_player_index arrives in SI (register argument); the result is returned
 * in EAX as fp_base + local_player_index * 0x1ea0. */
void *first_person_weapon_get(int16_t local_player_index)
{
  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  return (void *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
}

/* Toggle the first-person weapon activation state for a local player (0xdcb30).
 * When activating (activate != 0): asserts weapon_index != NONE, then calls
 * effects_start_on_first_person_weapon to start effects. When deactivating:
 * calls effects_stop_on_first_person_weapon to stop effects and
 * particles_stop_on_first_person_weapon to stop sounds. Only acts if the state
 * changes. */
void first_person_weapon_set_visibility(int16_t local_player_index,
                                        uint8_t activate)
{
  char *fp;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);

  if (activate != *(uint8_t *)fp) {
    if (activate != 0) {
      assert_halt(*(int *)(fp + 8) != -1);
      effects_start_on_first_person_weapon((int)local_player_index,
                                           *(int *)(fp + 8));
      *(uint8_t *)fp = activate;
      return;
    }
    effects_stop_on_first_person_weapon((int)local_player_index);
    particles_stop_on_first_person_weapon((int)local_player_index);
    *(uint8_t *)fp = 0;
  }
}

/* Copy animation node transforms from the animation graph into the
 * first-person node buffer using a node remap table (0xdcbd0).
 * For each model node i, node_remap[i] gives the source index in the
 * animation graph node array. Copies 0x34 bytes (13 dwords) per node.
 * Asserts that every remap index is within [0, antr->nodes.count). */
void model_remap_node_matrices_to_match_animation_graph(int mode_tag_index,
                                                        int fp_nodes,
                                                        int antr_tag_index,
                                                        int anim_nodes,
                                                        int16_t *node_remap)
{
  char *mode_tag;
  char *antr_tag;
  int node_count;
  int16_t i;
  int16_t remap_idx;
  int src_off;
  int dst_off;
  int j;
  unsigned int *src;
  unsigned int *dst;

  mode_tag = (char *)tag_get(0x6d6f6465, mode_tag_index);
  antr_tag = (char *)tag_get(0x616e7472, antr_tag_index);
  node_count = *(int *)(mode_tag + 0xb8);
  if (node_count <= 0)
    return;

  i = 0;
  do {
    remap_idx = node_remap[(int)i];
    assert_halt_msg_at("animation_graph_node_index>=0 && animation_graph_node_index<animation_graph->nodes.count", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x633, remap_idx >= 0 && (int)remap_idx < *(int *)(antr_tag + 0x68));
    src_off = (int)remap_idx * 0x34;
    dst_off = (int)i * 0x34;
    src = (unsigned int *)(anim_nodes + src_off);
    dst = (unsigned int *)(fp_nodes + dst_off);
    for (j = 0xd; j != 0; j--) {
      *dst = *src;
      src++;
      dst++;
    }
    i++;
  } while ((int)i < *(int *)(mode_tag + 0xb8));
}

/* Match animation node labels between a model tag and an animation graph tag
 * (0xdcc80). For each node in the model's node block, searches the animation
 * graph's node block for a matching string label via csstrcmp. Stores the
 * matching index in the output array. Returns 1 if all nodes matched, 0 if
 * any node had no match. */
uint8_t model_build_remapping_table_for_animation_graph(int mode_tag_index,
                                                        int antr_tag_index,
                                                        int16_t *output)
{
  char *mode_tag;
  char *antr_tag;
  uint8_t success;
  int16_t outer;
  int outer_int;
  void *antr_nodes; /* pointer to antr tag + 0x68 (node block) */

  mode_tag = (char *)tag_get(0x6d6f6465, mode_tag_index);
  antr_tag = (char *)tag_get(0x616e7472, antr_tag_index);

  success = 1;
  outer = 0;
  outer_int = 0;

  if (*(int *)(mode_tag + 0xb8) < 1)
    return 1;

  antr_nodes = (void *)(antr_tag + 0x68);

  do {
    char *mode_element;
    int16_t inner;
    int inner_int;

    mode_element =
      (char *)tag_block_get_element(mode_tag + 0xb8, outer_int, 0x9c);
    inner = 0;

    if (*(int *)antr_nodes > 0) {
      inner_int = 0;
      do {
        char *antr_element;
        antr_element =
          (char *)tag_block_get_element(antr_nodes, inner_int, 0x40);
        if (csstrcmp(mode_element, antr_element) == 0) {
          if (inner != -1) {
            output[outer_int] = inner;
            goto next_outer;
          }
          break;
        }
        inner = inner + 1;
        inner_int = (int)inner;
      } while (inner_int < *(int *)antr_nodes);
    }

    success = 0;

  next_outer:
    outer = outer + 1;
    outer_int = (int)outer;
    if (outer_int >= *(int *)(mode_tag + 0xb8))
      return success;
  } while (1);
}

/* Find the local player index (0..3) whose unit currently holds the given
 * weapon object. Iterates all local players, resolves each player's
 * controlled unit, and checks if the unit's active weapon slot matches the
 * given object handle. Returns the local player index or -1 if not found. */
int16_t first_person_weapon_index_from_weapon_index(int object_handle)
{
  int16_t i;

  for (i = 0; i < 4; i++) {
    int player_handle;
    char *player;
    int unit_handle;
    char *unit;
    int16_t weapon_index;

    player_handle = local_player_get_player_index(i);
    if (player_handle == -1)
      continue;

    player = (char *)datum_get(player_data, player_handle);
    unit_handle = *(int *)(player + 0x34);
    if (unit_handle == -1)
      continue;

    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    weapon_index = *(int16_t *)(unit + 0x2a2);
    if (weapon_index == -1)
      continue;

    if (object_handle == *(int *)(unit + 0x2a8 + (int)weapon_index * 4))
      return i;
  }

  return (int16_t)-1;
}

/* Find the local player index (0..3) whose player record controls the given
 * unit object handle (0xdcdc0). Iterates all local players, resolves each
 * player datum, and compares the player's controlled-unit handle at +0x34
 * against the handle passed in EDI. Returns the local player index or -1. */
int16_t first_person_weapon_index_from_unit_index(int unit_handle)
{
  int16_t i;

  for (i = 0; i < 4; i++) {
    int player_handle;
    char *player;

    player_handle = local_player_get_player_index(i);
    if (player_handle == -1)
      continue;

    player = (char *)datum_get(player_data, player_handle);
    if (*(int *)(player + 0x34) == unit_handle)
      return i;
  }

  return (int16_t)-1;
}

/* Precache the weapon's predicted resources and set the reload timer (0xdce00).
 * If the player has a valid weapon, resolves the weapon tag and calls
 * predicted_resources_precache on the resource block at weapon_tag + 0x4e4.
 * Always sets the timer at fp + 0x12 to 0x1e (30 ticks). */
void first_person_weapon_predict(int16_t local_player_index)
{
  char *fp;
  int weapon_handle;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_handle = *(int *)(fp + 8);

  if (weapon_handle != -1) {
    int *weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
    predicted_resources_precache((int *)(weapon_tag + 0x4e4));
  }

  *(int16_t *)(fp + 0x12) = 0x1e;
}

/* Render the current local player's first-person weapon and hands (0xdce80).
 * Reads the active local player index at 0x506548; bails on -1. Resolves the
 * player's unit (player+0x34), the fp slot's weapon (fp+0x8), the weapon tag
 * and its animation graph (weap+0x478). Builds an on-stack record at
 * ebp-0x38 that render_model receives as its effect_record argument:
 * a flag word (1 when unit+0x1b4 bit 0x10 is set or unit+0x32c > 0.0f),
 * then unit+0x32c/+0x330, the unit handle and the 3 dwords at 0x506550.
 * Then remaps and renders the weapon model (weap+0x468, gated by fp+0x1d8c)
 * and the hands model (game globals block +0x17c element 0, +0xc, gated by
 * fp+0x1e0e). Field meanings beyond the offsets are unproven. */
void first_person_weapon_draw(void)
{
  struct {
    int16_t field_00;
    int16_t pad_02;
    uint32_t field_04;
    uint32_t field_08;
    int field_0c;
    uint32_t field_10;
    uint32_t field_14;
    uint32_t field_18;
    uint32_t field_1c;
    uint32_t pad_20[2]; /* ref frame is 0xd38: model_effect starts at ebp-0x38 */
  } model_effect; /* name: PAL 2342 first_person_weapons.c:496 */
  char node_matrices[0xd00];
  char *fp;
  char *unit;
  int *weapon;
  char *weapon_tag;
  char *first_person_interface; /* name: PAL 2342 first_person_weapons.c:497 */
  void *lighting;
  int unit_handle;

  if (*(int16_t *)0x506548 == -1)
    return;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, *(int16_t *)0x506548 >= 0 &&
              *(int16_t *)0x506548 < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)*(int16_t *)0x506548 * 0x1ea0);
  if (local_player_get_player_index(*(int16_t *)0x506548) == -1)
    return;

  unit_handle =
    *(int *)((char *)datum_get(player_data, local_player_get_player_index(
                                              *(int16_t *)0x506548)) +
             0x34);
  if (unit_handle == -1 || *fp == 0 || *(int *)(fp + 4) == -1 ||
      *(int *)(fp + 8) == -1)
    return;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  weapon_tag = (char *)tag_get(0x77656170, *weapon);
  if (*(int *)(weapon_tag + 0x478) == -1)
    return;

  first_person_interface =
    (char *)tag_block_get_element((char *)game_globals_get() + 0x17c, 0, 0xc0);
  tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));
  lighting = scenario_leaf_index_from_point(unit_handle, 3.4028235e+38f);

  model_effect.field_1c = 0;
  if ((*(uint8_t *)(unit + 0x1b4) & 0x10) != 0 ||
      *(float *)(unit + 0x32c) > *(float *)0x2533c0) {
    model_effect.field_04 = *(uint32_t *)(unit + 0x32c);
    model_effect.field_08 = *(uint32_t *)(unit + 0x330);
    model_effect.field_0c = unit_handle;
    model_effect.field_00 = 1;
    model_effect.field_10 = *(uint32_t *)0x506550;
    model_effect.field_14 = *(uint32_t *)0x506554;
    model_effect.field_18 = *(uint32_t *)0x506558;
  } else {
    model_effect.field_00 = 0;
  }

  if (*(uint8_t *)(fp + 0x1d8c) != 0 && *(int *)(weapon_tag + 0x468) != -1) {
    model_remap_node_matrices_to_match_animation_graph(
      *(int *)(weapon_tag + 0x468), (int)node_matrices,
      *(int *)(weapon_tag + 0x478), (int)(fp + 0x108c),
      (int16_t *)(fp + 0x1d8e));
    render_model(*(int *)(weapon_tag + 0x468), 0.0f, node_matrices, 0,
                 (char *)weapon + 0x168, (char *)weapon + 0xe4, (int)lighting,
                 (void *)0x506550, 0, &model_effect, *(int *)(fp + 8), 0, 8);
  }

  if (*(uint8_t *)(fp + 0x1e0e) != 0 && *(int *)(first_person_interface + 0xc) != -1) {
    model_remap_node_matrices_to_match_animation_graph(
      *(int *)(first_person_interface + 0xc), (int)node_matrices,
      *(int *)(weapon_tag + 0x478), (int)(fp + 0x108c),
      (int16_t *)(fp + 0x1e10));
    render_model(*(int *)(first_person_interface + 0xc), 0.0f, node_matrices, 0,
                 unit + 0x168, unit + 0xe4, (int)lighting, (void *)0x506550, 0,
                 &model_effect, *(int *)(fp + 8), 0, 8);
  }
}

/* Search the 4 first-person weapon slots for the one owning object_handle.
 * Returns the local player index (0-3), or -1 if not found (0xdd110). */
int first_person_weapon_get_local_index(int object_handle)
{
  char *base;
  int16_t i;

  i = 0;
  do {
    if (i < 0 || i >= 4) {
      display_assert("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\interface\\first_person_weapons.c",
                     0x599, 1);
      system_exit(-1);
    }
    base = *(char **)0x46bea8 + (int)i * 0x1ea0;
    if (*(int *)(base + 8) == object_handle && *base != 0)
      break;
    i++;
  } while (i < 4);
  if (i == 4)
    return -1;
  return (int)i;
}

/* 0xdd190 — first_person_weapon_get_marker_by_name */
int16_t first_person_weapon_get_marker_by_name(int object_handle, void *marker_name, void *out_markers, int max_count)
{
  char *weapon;
  int16_t local_player_index;
  char *fp;
  char *tag;
  char *antr_tag;
  int antr_tag_index;
  int mode_tag_index;

  weapon = (char *)object_try_and_get_and_verify_type(object_handle, 4);
  if (!weapon) {
    return 0;
  }

  local_player_index = first_person_weapon_index_from_weapon_index(object_handle);
  if (local_player_index == -1 || director_get_perspective(local_player_index) != 0) {
    return 0;
  }

  fp = (char *)first_person_weapon_get(local_player_index);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);

  if (!*(uint8_t *)(fp + 0x1d8c)) {
    return 0;
  }

  mode_tag_index = *(int *)(tag + 0x468);
  if (mode_tag_index == -1) {
    return 0;
  }

  antr_tag_index = *(int *)(tag + 0x478);
  if (antr_tag_index == -1) {
    return 0;
  }

  antr_tag = (char *)tag_get(0x616e7472, antr_tag_index);

  return model_get_marker_by_name(
    mode_tag_index,
    (const char *)marker_name,
    0,
    (int)(uint32_t)(fp + 0x1d8e),
    *(int16_t *)(antr_tag + 0x68),
    fp + 0x108c,
    0,
    out_markers,
    (int16_t)max_count
  );
}

/* 0xdd260 — first_person_weapon_center_flashlight */
void first_person_weapon_center_flashlight(int object_handle, float *out_position, float *out_forward, void *out_up)
{
  int16_t local_player_index;
  char *fp;
  object_marker marker;




  local_player_index = first_person_weapon_index_from_unit_index(object_handle);
  if (local_player_index == -1) {
    return;
  }

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);



  fp = (char *)*(int *)0x46bea8 + (int)local_player_index * 0x1ea0;
  if (!*(uint8_t *)fp) {
    return;
  }

  if (first_person_weapon_get_marker_by_name(*(int *)(fp + 8), (void *)"flashlight", &marker, 1) <= 0) {
    return;
  }




  out_position[0] = marker.matrix.position.x - marker.matrix.forward.x * 0.5f;
  out_position[1] = marker.matrix.position.y - marker.matrix.forward.y * 0.5f;
  out_position[2] = marker.matrix.position.z - marker.matrix.forward.z * 0.5f;

  *(vector3_t *)out_forward = marker.matrix.forward;



  /* 0xdd31b-0xdd32f: out_up is written unconditionally (no NULL test). */
  *(vector3_t *)out_up = marker.matrix.up;


}

/* Fetch the first-person marker of a weapon held by the unit of the local
 * player currently being rendered (0xdd340). Resolves weapon (type 4) ->
 * owner unit at +0xcc (type 3) -> player handle at unit+0x1c8; requires the
 * player's local index (+0x2) to equal the render local player (0x506548)
 * and first_person_weapon_get()'s leading byte to be nonzero. Looks up one
 * marker named by marker_result and copies its position (+0x60), forward
 * (+0x3c) and up (+0x54) vectors out. Returns 1 on success, else 0. */
char first_person_weapon_adjust_light(int object_handle, int marker_result,
                                      void *out_position, void *out_forward,
                                      void *out_up)
{
  object_marker marker;
  char *weapon;
  char *unit;
  char *player;
  int player_handle;
  int16_t local_player_index;
  char result;

  weapon = (char *)object_get_and_verify_type(object_handle, 4);
  unit = (char *)object_get_and_verify_type(*(int *)(weapon + 0xcc), 3);
  player_handle = *(int *)(unit + 0x1c8);
  result = 0;
  if (player_handle != -1) {
    player = (char *)datum_get(player_data, player_handle);
    local_player_index = *(int16_t *)(player + 2);
    if (local_player_index != -1 &&
        local_player_index == *(int16_t *)0x506548 &&
        *(char *)first_person_weapon_get(local_player_index) != 0 &&
        first_person_weapon_get_marker_by_name(
          object_handle, (void *)marker_result, &marker, 1) > 0) {
      ((int *)out_position)[0] = ((int *)&marker.matrix.position)[0];
      ((int *)out_position)[1] = ((int *)&marker.matrix.position)[1];
      ((int *)out_position)[2] = ((int *)&marker.matrix.position)[2];
      ((int *)out_forward)[0] = ((int *)&marker.matrix.forward)[0];
      ((int *)out_forward)[1] = ((int *)&marker.matrix.forward)[1];
      ((int *)out_forward)[2] = ((int *)&marker.matrix.forward)[2];
      ((int *)out_up)[0] = ((int *)&marker.matrix.up)[0];
      ((int *)out_up)[1] = ((int *)&marker.matrix.up)[1];
      ((int *)out_up)[2] = ((int *)&marker.matrix.up)[2];
      return 1;
    }
  }
  return result;
}

/* Return a pointer to the node transform for a given node in the local
 * player's first-person weapon animation state (0xdd410).
 * Validates the local_player_index (0..3) and node_index against the
 * animation graph node count. Returns fp_base + 0x108c + node_index * 0x34. */
void *first_person_weapon_get_node_matrix(int16_t local_player_index,
                                          int16_t node_index)
{
  char *fp;
  int *weapon_obj;
  char *weapon_tag;
  char *antr_tag;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_obj = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

  assert_halt_msg_at("node_index>=0 && node_index<animation_graph->nodes.count", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x2ce, node_index >= 0 && (int)node_index < *(int *)(antr_tag + 0x68));

  return (void *)((int)node_index * 0x34 + 0x108c + (int)fp);
}

/* Copy animation node data from the current buffer to the blend buffer and
 * update blend timing (0xdd4d0). Copies node_count * 32 bytes from fp + 0x8c
 * to fp + 0x88c. If blend_ticks >= (fp[0x8a] - fp[0x88]), resets the blend
 * origin to 0 and sets the blend target to blend_ticks. */
void first_person_weapon_start_interpolation(int16_t local_player_index,
                                             int16_t blend_ticks)
{
  char *fp;
  int *weapon_obj;
  char *weapon_tag;
  char *antr_tag;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_obj = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

  csmemcpy(fp + 0x88c, fp + 0x8c, *(int *)(antr_tag + 0x68) << 5);

  if ((int)blend_ticks >=
      (int)*(int16_t *)(fp + 0x8a) - (int)*(int16_t *)(fp + 0x88)) {
    *(int16_t *)(fp + 0x88) = 0;
    *(int16_t *)(fp + 0x8a) = blend_ticks;
  }
}

/* 0xdd580 — first_person_weapon_build_node_matrices */
void first_person_weapon_build_node_matrices(int16_t local_player_index)
{
  char *fp;
  char *weapon;
  char *tag;
  char *model_tag;
  char *antr_tag;
  char *unit_anim;
  int16_t node_index;
  char *node_elem;
  char *matrices;
  char *anim_node;
  char *node;
  int frame;
  int16_t diff;
  char *mag_def;
  float frac;
  int16_t max_rounds;
  int16_t rounds;

  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + (int)local_player_index * 0x1ea0;
  if (!*(uint8_t *)(fp + 0x50)) {
    vector_to_angles((float *)(fp + 0x60), (float *)0x50655c);
    *(uint32_t *)(fp + 0x70) = *(uint32_t *)0x506550;
    *(uint32_t *)(fp + 0x74) = *(uint32_t *)0x506554;
    *(uint32_t *)(fp + 0x78) = *(uint32_t *)0x506558;
  }

  *(uint32_t *)(fp + 0x6c) = *(uint32_t *)(fp + 0x64);
  *(uint32_t *)(fp + 0x68) = *(uint32_t *)(fp + 0x60);
  *(uint32_t *)(fp + 0x7c) = *(uint32_t *)(fp + 0x70);
  *(uint32_t *)(fp + 0x80) = *(uint32_t *)(fp + 0x74);
  *(uint32_t *)(fp + 0x84) = *(uint32_t *)(fp + 0x78);

  vector_to_angles((float *)(fp + 0x60), (float *)0x50655c);
  *(uint32_t *)(fp + 0x70) = *(uint32_t *)0x506550;
  *(uint32_t *)(fp + 0x74) = *(uint32_t *)0x506554;
  *(uint32_t *)(fp + 0x78) = *(uint32_t *)0x506558;

  *(uint32_t *)(fp + 0x54) = *(uint32_t *)0x50655c;
  *(uint32_t *)(fp + 0x58) = *(uint32_t *)0x506560;
  *(uint32_t *)(fp + 0x5c) = *(uint32_t *)0x506564;

  *(uint8_t *)(fp + 0x50) = 1;

  if (*(int *)(fp + 8) != -1 && !object_get_and_verify_type(*(int *)(fp + 8), 4)) {
    error(3, "local player %d, weapon (0x%x), deleted unexpectedly", (int)local_player_index, *(int *)(fp + 8));
    *(int *)(fp + 8) = -1;
  }

  if (*(int *)(fp + 8) == -1) {
    return;
  }

  weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  model_tag = (char *)tag_get(0x6d6f6465, *(uint32_t *)(tag + 0x468));
  antr_tag = (char *)tag_get(0x616e7472, *(uint32_t *)(tag + 0x478));

  if (*(int *)(antr_tag + 0x48) == 0 || !(unit_anim = (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c))) {
    animation_graph_node_matrices_from_orientations(*(int *)(tag + 0x478), (float *)(fp + 0x108c), (float *)(fp + 0x8c), (float *)0x506550, (float *)0x50655c, (float *)0x506568);
    return;
  }

  if (*(int16_t *)(fp + 0x16) != -1) {
    anim_node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x16), 0xb4);
    FUN_00121d60(0, anim_node, *(int16_t *)(fp + 0x18), fp + 0x8c);
  } else {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x50e, 1);
    system_exit(-1);
    FUN_00123aa0(model_tag, fp + 0x8c);
  }

  matrices = fp + 0x8c;

  if (*(int *)(unit_anim + 0x10) >= 0x12) {
    node_index = *(int16_t *)(*(int *)(unit_anim + 0x14) + 0x22);
    if (node_index != -1) {
      node_elem = (char *)tag_block_get_element(antr_tag + 0x74, (int)node_index, 0xb4);
      if (*(int16_t *)(tag + 0x4e2) != 2 || (*(int16_t *)(*(char **)0x46bea8 + 0xc) != 0xd && *(int16_t *)(*(char **)0x46bea8 + 0xc) != 0xe)) {
        if (*(int16_t *)(weapon + 0x260) < *(int16_t *)(node_elem + 0x22)) {
          overlay_animation_apply(node_elem, (int)*(int16_t *)(weapon + 0x260), matrices);
        }
      } else {
        frame = (int)*(int16_t *)(weapon + 0x260);
        diff = *(int16_t *)(weapon + 0x25c) - *(int16_t *)(weapon + 0x25a);
        if (diff >= 0x2c) {
          mag_def = (char *)tag_block_get_element(tag + 0x4f0, 0, 0x70);
          frac = (float)(diff - 0x2c) * 0.2f;
          max_rounds = *(int16_t *)(mag_def + 0xa);
          rounds = *(int16_t *)(weapon + 0x25e);
          if (frac > 1.0f) frac = 1.0f;
          if (rounds > max_rounds) rounds = max_rounds;
          frame += (int)((float)(rounds - frame) * frac);
        }
        overlay_animation_apply(node_elem, frame, matrices);
      }
    }
  }

  if (*(int16_t *)(fp + 0x1a) != -1) {
    node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x1a), 0xb4);
    overlay_animation_apply(node, (int)*(int16_t *)(fp + 0x1c), matrices);
  }

  if (*(int16_t *)(fp + 0x20) != -1) {
    node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x20), 0xb4);
    /* 0xdd90e: the 0xb4 animation element itself (EAX from
     * tag_block_get_element) is arg 1 -- the callee reads its frame count
     * at +0x22.  Passing *(node + 0x20) read a bogus frame count and fired
     * "frame index out of bounds B" every tick of the charge overlay. */
    overlay_animation_apply_continuous_scaled((int)node, *(float *)(fp + 0x24), *(float *)(weapon + 0x1f4) + *(float *)0x253398, (int)(uint32_t)matrices);
  }

  if (*(int *)(unit_anim + 0x10) >= 5) {
    node_index = *(int16_t *)(*(int *)(unit_anim + 0x14) + 8);
    if (node_index != -1) {
      node = (char *)tag_block_get_element(antr_tag + 0x74, (int)node_index, 0xb4);
      if (*(int16_t *)(node + 0x22) >= 9) {
        if (*(float *)(fp + 0x30) <= 0.0f) {
          if (*(float *)(fp + 0x30) < 0.0f) {
            ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 1, -*(float *)(fp + 0x30), matrices);
          }
        } else {
          ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 0, *(float *)(fp + 0x30), matrices);
        }

        if (*(float *)(fp + 0x34) <= 0.0f) {
          if (*(float *)(fp + 0x34) < 0.0f) {
            ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 2, -*(float *)(fp + 0x34), matrices);
          }
        } else {
          ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 3, *(float *)(fp + 0x34), matrices);
        }

        if (*(float *)(fp + 0x40) <= 0.0f) {
          if (*(float *)(fp + 0x40) < 0.0f) {
            ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 5, -*(float *)(fp + 0x40), matrices);
          }
        } else {
          ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 4, *(float *)(fp + 0x40), matrices);
        }

        if (*(float *)(fp + 0x44) <= 0.0f) {
          if (*(float *)(fp + 0x44) < 0.0f) {
            ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 6, -*(float *)(fp + 0x44), matrices);
          }
        } else {
          ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 7, *(float *)(fp + 0x44), matrices);
        }

        if (*(float *)(fp + 0x28) > 0.0f) {
          ((void (*)(void *, int, float, void *))overlay_animation_apply_scaled)(node, 8, *(float *)(fp + 0x28), matrices);
        }
      }
    }
  }

  if (*(int16_t *)(fp + 0x8a) >= 1) {
    interpolate_node_orientations(*(int16_t *)(antr_tag + 0x68), fp + 0x88c, matrices, *(int16_t *)(fp + 0x88), *(int16_t *)(fp + 0x8a));
  }

  animation_graph_node_matrices_from_orientations(*(int *)(tag + 0x478), (float *)(fp + 0x108c), (float *)(fp + 0x8c), (float *)0x506550, (float *)0x50655c, (float *)0x506568);
}

/* 0xddae0 — first_person_weapon_render_update */
void first_person_weapon_render_update(void)
{
  int16_t current_player;
  char *fp;
  uint8_t activate;

  current_player = *(int16_t *)0x506548;
  if (current_player == -1) {
    return;
  }
  if (current_player < 0 || current_player >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + (int)current_player * 0x1ea0;
  if (*(int *)(fp + 4) == -1 || *(int *)(fp + 8) == -1) {
    return;
  }

  if (director_get_perspective(current_player) != 0 || player_control_get_zoom_level(current_player) != -1) {
    activate = 0;
  } else {
    activate = 1;
  }

  first_person_weapon_set_visibility(current_player, activate);
  if (*fp != 0) {
    first_person_weapon_build_node_matrices(current_player);
  }
}

/* Get first-person weapon markers by name, but only for the local player
 * currently being rendered (0xddb90). The weapon's holding local player is
 * resolved via FUN_000dcd60 and compared against the current render local
 * player index (global 0x506548); on a mismatch the function returns 0
 * markers. Otherwise it forwards all four arguments to
 * first_person_weapon_get_marker_by_name (0xdd190) and returns its count. */
int16_t first_person_weapon_get_marker_by_name_render(int object_handle,
                                                      void *marker_name,
                                                      void *out_markers,
                                                      int max_count)
{
  if (*(int16_t *)0x506548 ==
      first_person_weapon_index_from_weapon_index(object_handle)) {
    return first_person_weapon_get_marker_by_name(object_handle, marker_name,
                                                  out_markers, max_count);
  }

  return 0;
}

/* Set the first-person weapon animation state for a local player (0xddbd0).
 * Applies state-transition filtering: certain incoming states are rejected
 * depending on the current state. For dual-wielding weapons (type 3), maps
 * state 3 to state 0 unless the weapon has a specific flag. Looks up the
 * animation index via first_person_animation_type_from_weapon_state and the
 * animation graph to validate the transition. If param_3 is nonzero, stops any
 * pending sound. */
void first_person_weapon_set_state(int16_t local_player_index, int param_2,
                                   int param_3)
{
  int16_t state = (int16_t)param_2;
  char *fp;
  int weapon_handle;
  int16_t sVar1;
  int16_t blend_ticks;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_handle = *(int *)(fp + 8);

  /* If the unit's weapon has the dual-wield flag (byte 0x1dc bit 0),
   * remap certain states. */
  if (weapon_handle != -1) {
    char *weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
    if ((*(uint8_t *)(weapon_obj + 0x1dc) & 1) != 0) {
      if (state == 0x13) {
        state = 2;
      } else if (state == 0x14) {
        state = 0x15;
      }
    }
  }

  /* First switch: filter incoming states based on current state.
   * Binary switch table at 0xdde40/0xdde50 maps:
   *   6,7,8,9 → handler 0 (melee filter)
   *   0xb,0xc → handler 1 (grenade filter)
   *   0x13    → handler 2
   *   0xa,0xd-0x12 → handler 3 (default, no filter) */
  switch (state) {
  case 6:
  case 7:
  case 8:
  case 9: {
    int16_t cur = *(int16_t *)(fp + 0xc);
    if (cur != 0 && cur != 5 && cur != 6 && cur != 4 && cur != 0xf &&
        cur != 0x16 && cur != 0x10 && cur != 0x11 && cur != 0xd && cur != 0xe) {
      return;
    }
    break;
  }
  case 0xb:
  case 0xc:
    if (*(int16_t *)(fp + 0xc) != 0 && *(int16_t *)(fp + 0xc) != 5) {
      return;
    }
    break;
  case 0x13:
    if (*(int16_t *)(fp + 0xc) == 0x13)
      return;
    break;
  default:
    break;
  }

  if (state == -1)
    return;
  if (*(int *)(fp + 8) == -1)
    return;

  {
    int *weapon_obj2 = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
    char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj2);

    /* For weapon type 3, if state is also 3 and the dual-wield flag is
     * not set, reset state to 0. */
    if (*(int16_t *)(weapon_tag + 0x4e2) == 3 && state == 3 &&
        (*(uint8_t *)((char *)weapon_obj2 + 0x1dc) & 1) == 0) {
      state = 0;
    }

    sVar1 = first_person_animation_type_from_weapon_state(state);

    /* If weapon type is 1 and current state is 0x10, use blend_ticks = 0. */
    if (*(int16_t *)(weapon_tag + 0x4e2) == 1 &&
        *(int16_t *)(fp + 0xc) == 0x10) {
      blend_ticks = 0;
    } else {
      /* Second switch: determine blend tick count. */
      switch (state) {
      case 3:
      case 10:
      case 0x13:
        blend_ticks = 0;
        break;
      case 6:
      case 7:
      case 8:
      case 9:
        blend_ticks = 3;
        break;
      default:
        blend_ticks = 6;
        break;
      }
    }

    if (*(int *)(fp + 4) == -1)
      return;
    if (*(int *)(fp + 8) == -1)
      return;

    {
      int *weapon_obj3 = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
      char *weapon_tag2 = (char *)tag_get(0x77656170, *weapon_obj3);
      char *antr_tag =
        (char *)tag_get(0x616e7472, *(int *)(weapon_tag2 + 0x478));

      if (*(int *)(antr_tag + 0x48) != 0) {
        char *anim_block =
          (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
        if (anim_block != NULL && sVar1 >= 0 &&
            (int)sVar1 < *(int *)(anim_block + 0x10)) {
          int16_t anim_index =
            *(int16_t *)(*(int *)(anim_block + 0x14) + (int)sVar1 * 2);
          if (anim_index != -1) {
            if ((char)param_3 != 0 && *(int *)(fp + 0x1e98) != -1 &&
                *(int16_t *)(fp + 0x1e9c) != 1) {
              sound_stop_impulse(*(int *)(fp + 0x1e98));
              *(int *)(fp + 0x1e98) = -1;
              *(int16_t *)(fp + 0x1e9c) = -1;
            }
            if (blend_ticks > 0) {
              first_person_weapon_start_interpolation(local_player_index,
                                                      blend_ticks);
            }
            *(int16_t *)(fp + 0xc) = state;
            *(int16_t *)(fp + 0x16) = anim_index;
            *(int16_t *)(fp + 0x18) = 0;
          }
        }
      }
    }
  }
}

/* Initialize or reinitialize a local player's first-person weapon (0xdde80).
 * Clears the current weapon reference, deactivates visual/sound state if
 * previously active, then resolves the unit's current weapon and sets up
 * the animation graph, idle animation index, and initial weapon state. */
void first_person_weapon_switch_weapons(int param_1)
{
  int16_t local_player_index = (int16_t)param_1;
  char *fp;
  uint8_t was_active;
  int weapon_handle;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  was_active = *(uint8_t *)fp;

  /* Clear weapon handle. */
  *(int *)(fp + 8) = -1;

  /* If previously active, deactivate effects and sounds. */
  if (was_active != 0) {
    assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    {
      char *fp2 = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
      if (*(uint8_t *)fp2 != 0) {
        effects_stop_on_first_person_weapon(param_1);
        particles_stop_on_first_person_weapon(param_1);
        *(uint8_t *)fp2 = 0;
      }
    }
  }

  /* If no unit is assigned, skip weapon setup. */
  if (*(int *)(fp + 4) == -1)
    goto done;

  {
    char *unit_obj = (char *)object_get_and_verify_type(*(int *)(fp + 4), 3);
    int16_t weapon_index = *(int16_t *)(unit_obj + 0x2a2);

    weapon_handle = unit_inventory_get_weapon(*(int *)(fp + 4), weapon_index);
    if (weapon_handle == -1)
      goto done;

    {
      int *weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);

      if (*(int *)(weapon_tag + 0x468) == -1)
        goto done;
      if (*(int *)(weapon_tag + 0x478) == -1)
        goto done;

      {
        char *antr_tag =
          (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

        if (*(int *)(antr_tag + 0x48) == 0)
          goto done;

        {
          char *anim_block =
            (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
          if (anim_block == NULL)
            goto done;

          /* Set idle animation index from the animation lookup table. */
          *(int16_t *)(fp + 0x14) = -1;
          if (*(int *)(anim_block + 0x10) > 4) {
            int16_t anim_lookup = *(int16_t *)(*(int *)(anim_block + 0x14) + 8);
            if (anim_lookup != -1) {
              char *anim_entry = (char *)tag_block_get_element(
                antr_tag + 0x74, (int)anim_lookup, 0xb4);
              if (*(int16_t *)(anim_entry + 0x22) >= 9) {
                *(int16_t *)(fp + 0x14) = anim_lookup;
              }
            }
          }

          /* Check game globals for the global fp animation model. */
          {
            char *game_globals = (char *)game_globals_get();
            char *gg_element =
              (char *)tag_block_get_element(game_globals + 0x17c, 0, 0xc0);

            if (*(int *)(gg_element + 0xc) != -1) {
              *(uint8_t *)(fp + 0x1e0e) =
                model_build_remapping_table_for_animation_graph(
                  *(int *)(gg_element + 0xc), *(int *)(weapon_tag + 0x478),
                  (int16_t *)(fp + 0x1e10));
            }
          }

          *(uint8_t *)(fp + 0x1d8c) =
            model_build_remapping_table_for_animation_graph(
              *(int *)(weapon_tag + 0x468), *(int *)(weapon_tag + 0x478),
              (int16_t *)(fp + 0x1d8e));

          if (*(uint8_t *)(fp + 0x1d8c) == 0)
            goto done;
          if (*(uint8_t *)(fp + 0x1e0e) == 0)
            goto done;

          /* Set weapon handle and clear animation state. */
          *(int *)(fp + 0x8) = weapon_handle;
          *(int16_t *)(fp + 0xc) = -1;
          *(int16_t *)(fp + 0x16) = -1;
          *(int16_t *)(fp + 0x1a) = -1;
          *(int16_t *)(fp + 0x20) = -1;
          *(int *)(fp + 0x28) = 0;
          *(int *)(fp + 0x2c) = 0;
          *(int16_t *)(fp + 0x10) = 0;
          *(int *)(fp + 0x1e98) = -1;
          *(int16_t *)(fp + 0x1e9c) = -1;

          first_person_weapon_set_state(param_1, 0, 1);

          *(int16_t *)(fp + 0x8a) = 0;

          if (was_active != 0) {
            first_person_weapon_set_visibility(local_player_index, 1);
          }
        }
      }
    }
  }

done:
  first_person_weapon_predict(local_player_index);
}

/* Bind a first-person weapon state block to a player and reset it (0xde0e0).
 * local_player_index arrives in ESI (register argument); param_2 arrives on the
 * stack and is stored at fp+4 (the value later passed to
 * player_clear_aim_assist by FUN_000de140). Clears the byte at fp+0x50, then
 * runs the weapon-state reset in FUN_000dde80. */
void first_person_weapon_new_unit(int local_player_index, int param_2)
{
  char *fp;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, (int16_t)local_player_index >= 0 &&
              (int16_t)local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)(int16_t)local_player_index * 0x1ea0);

  *(uint8_t *)(fp + 0x50) = 0;
  *(int *)(fp + 4) = param_2;

  first_person_weapon_switch_weapons(local_player_index);
}

/* Process a weapon event for a local player's first-person weapon (0xde140).
 * Handles reload initiation, weapon put-away, aim-assist clearing, and state
 * transitions. Computes reload count from trigger data and weapon ammo state,
 * then selects the appropriate animation state. */
void first_person_weapon_message(int param_1, int param_2)
{
  char *fp;
  int saved_event;
  int new_state;

  if ((int16_t)param_1 == -1)
    return;

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, (int16_t)param_1 >= 0 &&
              (int16_t)param_1 < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)(int16_t)param_1 * 0x1ea0);
  saved_event = (int)(int16_t)param_2;

  switch ((int16_t)param_2) {
  case 0:
    *(float *)(fp + 0x2c) += 0.05f;
    break;
  case 9:
  case 10:
    player_control_unzoom(*(int *)(fp + 4));
    break;
  case 0xc:
    first_person_weapon_switch_weapons(param_1);
    break;
  case 0xd:
    *(int *)(fp + 0x8) = -1;
    break;
  }

  if (*(int *)(fp + 0x8) == -1)
    goto default_handler;

  {
    int *weapon;
    char *tag;

    weapon = (int *)object_get_and_verify_type(*(int *)(fp + 0x8), 4);
    if (*weapon == -1)
      goto default_handler;

    tag = (char *)tag_get(0x77656170, *weapon);
    if (*(int16_t *)(tag + 0x4e2) != 1)
      goto default_handler;

    if ((int16_t)param_2 != 9 && (int16_t)param_2 != 10)
      goto default_handler;

    {
      char *trigger;
      int16_t current_state;
      int16_t rounds_loaded;
      int16_t ammo_remaining;
      int reload_count;
      int16_t reload_type;

      trigger = (char *)tag_block_get_element(tag + 0x4f0, 0, 0x70);
      current_state = *(int16_t *)(fp + 0xc);
      rounds_loaded = *(int16_t *)((char *)weapon + 0x260);
      ammo_remaining = *(int16_t *)((char *)weapon + 0x25e);

      reload_count = (int)*(int16_t *)(trigger + 0xa) - (int)rounds_loaded;
      if (reload_count > (int)ammo_remaining)
        reload_count = (int)ammo_remaining;

      if (current_state == 0xf || current_state == 0x16 ||
          current_state == 0x10 || current_state == 0x11 ||
          current_state == 0xd || current_state == 0xe ||
          *(int16_t *)((char *)weapon + 0x258) != 0) {
        if (reload_count == 1)
          *(int16_t *)(fp + 0x1e94) = 1;
        else
          *(int16_t *)(fp + 0x1e94) = -1;
      } else {
        *(int16_t *)(fp + 0x1e92) = (int16_t)reload_count;
        *(uint8_t *)(fp + 0x1e90) = (rounds_loaded == 0);
        *(uint16_t *)(fp + 0x1e94) = (uint16_t)(((reload_count != 1) - 1) & 2);
      }

      reload_type = *(int16_t *)(fp + 0x1e94);
      if (reload_type == -1) {
        new_state = 0xd;
      } else if (reload_type == 0 || reload_type == 2) {
        new_state = 0xf;
      } else {
        goto default_handler;
      }
      goto apply_state;
    }
  }

default_handler:
  new_state = (int)FUN_000dc800(param_2);
  if ((int16_t)new_state == -1)
    goto cleanup;

apply_state:
  first_person_weapon_set_state(param_1, new_state, 1);

cleanup:
  if (saved_event == 0xc)
    *(int16_t *)(fp + 0x8a) = 0;
}

/* Notify the first-person weapon system that a unit generated an event
 * (0xde360). Finds the local player controlling the unit (0xdcdc0) and
 * processes the event for that player. If no local player controls the
 * unit, falls back to the third-person path for the unit's currently
 * equipped weapon index at unit+0x2a2. */
void first_person_weapon_message_from_unit(int unit_handle, int message_type)
{
  int16_t local_player;

  local_player = first_person_weapon_index_from_unit_index(unit_handle);
  first_person_weapon_message(local_player, message_type);
  if (local_player == -1) {
    char *unit;
    int16_t weapon_index;

    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    weapon_index = *(int16_t *)(unit + 0x2a2);
    if (weapon_index != -1) {
      weapon_play_first_person_weapon_sound(message_type, (int)weapon_index);
    }
  }
}

/* Notify the first-person weapon system of an object event (0xde3b0).
 * Finds the local player holding the weapon, processes the event, and if
 * no local player owns it, attempts third-person sound playback. */
void first_person_weapon_message_from_weapon(int object_handle, int param_2)
{
  int16_t local_player =
    first_person_weapon_index_from_weapon_index(object_handle);
  first_person_weapon_message(local_player, param_2);
  if (local_player == -1) {
    weapon_play_first_person_weapon_sound(param_2, object_handle);
  }
}

/* 0xde3f0 — first_person_weapon_next_state */
void first_person_weapon_next_state(int local_player_index)
{
  char *fp;
  int16_t current_state;

  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + local_player_index * 0x1ea0;
  current_state = *(int16_t *)(fp + 0xc);
  if ((uint16_t)current_state > 23) {
    return;
  }

  switch (current_state) {
  case 0:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 16:
  case 17:
  case 19:
  case 20:
  case 22:
    first_person_weapon_set_state(local_player_index, 0, 0);
    break;

  case 1:
  case 2:
  case 21:
  case 23:
    first_person_weapon_set_state(local_player_index, 3, 0);
    break;

  case 3:
  case 4:
    break;

  case 18:
    (*(int16_t *)(fp + 0x18))--;
    break;

  case 13:
  case 14:
    {
      char *weapon;
      char *tag;
      int16_t anim;

      weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
      tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
      if (*(int16_t *)(tag + 0x4e2) == 1) {
        anim = *(int16_t *)(fp + 0x1e94);
        if (anim != 0 && anim != -1) {
          if (anim != 2 && anim != 1) {
            display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 799, 1);
            system_exit(-1);
          }
          if (*(uint8_t *)(fp + 0x1e90) != 0) {
            first_person_weapon_set_state(local_player_index, 0x10, 0);
          } else {
            first_person_weapon_set_state(local_player_index, 0x11, 0);
          }
          return;
        }
      }
      first_person_weapon_set_state(local_player_index, 0, 0);
    }
    break;

  case 15:
    {
      char *weapon;
      char *tag;

      weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
      tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
      if (*(int16_t *)(tag + 0x4e2) == 1 && *(int16_t *)(fp + 0x1e94) == 2) {
        if (*(uint8_t *)(fp + 0x1e90) != 0) {
          first_person_weapon_set_state(local_player_index, 0x10, 0);
        } else {
          first_person_weapon_set_state(local_player_index, 0x11, 0);
        }
        return;
      }
      first_person_weapon_set_state(local_player_index, 0, 0);
    }
    break;
  }
}

#include "x87_math.h"
/* Per-tick first-person weapon update for one local player (0xde560).
 * Validates the held weapon handle, drives the state and moving-overlay
 * animations, springs the sway/aim offsets toward their targets, rolls the
 * idle-fidget timer, and schedules prediction. Field names are unknown;
 * offsets are relative to the 0x1ea0-byte per-player block. */
void first_person_weapon_update(int16_t local_player_index)
{
  char *fp;
  char *unit;
  char *weapon;
  char *weapon_tag;
  char *antr_tag;
  char *element;
  int triggered_sound_index; /* name: PAL 2342 first_person_weapons.c:1673 */
  float magnitude;
  float turning_i; /* name: PAL 2342 first_person_weapons.c:1828 */
  float turning_j; /* name: PAL 2342 first_person_weapons.c:1831 */
  uint8_t moving;
  int16_t animation_update_result;
#if defined(_MSC_VER) && !defined(__clang__)
  double __cdecl fmod(double, double);
#endif

  assert_halt_msg_at("local_player_index>=0 && local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS", "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)*(int *)0x46bea8 + (int)local_player_index * 0x1ea0;
  if (*(int *)(fp + 8) != NONE &&
      object_try_and_get_and_verify_type(*(int *)(fp + 8), 4) == NULL) {
    error(3, "local player %d, weapon (0x%x), deleted unexpectedly",
          (int)local_player_index, *(int *)(fp + 8));
    *(int *)(fp + 8) = NONE;
  }

  if (*(int *)(fp + 4) != NONE && *(int *)(fp + 8) != NONE) {
    unit = (char *)object_get_and_verify_type(*(int *)(fp + 4), 3);
    weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
    weapon_tag = (char *)tag_get(0x77656170, *(int *)weapon);
    tag_get(0x6d6f6465, *(int *)(weapon_tag + 0x468));
    antr_tag = (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

    if (*(int16_t *)(fp + 0xc) == 3 || *(int16_t *)(fp + 0xc) == 1) {
      if ((*(uint8_t *)(weapon + 0x1dc) & 2) != 0) {
        first_person_weapon_set_state(local_player_index, 0x16, 1);
      }
      if ((*(uint8_t *)(weapon + 0x1dc) & 1) == 0) {
        first_person_weapon_set_state(local_player_index, 0, 1);
      }
    }

    animation_update_result = (int16_t)animation_update_internal(
      0, *(int *)(weapon_tag + 0x478), (short *)(fp + 0x16),
      &triggered_sound_index);
    if (animation_update_result != 1 &&
        animation_update_result == 2) {
      first_person_weapon_next_state(local_player_index);
    }




    if (triggered_sound_index != NONE &&
        director_get_perspective(local_player_index) == 0) {
      *(int *)(fp + 0x1e98) = object_impulse_sound_new(
        *(int *)(fp + 8), triggered_sound_index, -1, *(float **)0x31fc1c,
        *(float **)0x31fc3c, 1.0f);
      *(int16_t *)(fp + 0x1e9c) = *(int16_t *)(fp + 0xc);
    }

    /* moving = |unit+0x228 vector| > [0x25496c] and not flying */
    magnitude =
      sqrtf(*(float *)(unit + 0x228) * *(float *)(unit + 0x228) +
                  *(float *)(unit + 0x22c) * *(float *)(unit + 0x22c) +
                  *(float *)(unit + 0x230) * *(float *)(unit + 0x230));
    moving = magnitude > *(float *)0x25496c ? 1 : 0;
    if (unit_flying_through_air(*(int *)(fp + 4)) != 0) {
      moving = 0;
    }

    if (*(int16_t *)(fp + 0x1a) != -1) {
      animation_update_internal(0, *(int *)(weapon_tag + 0x478),
                                (short *)(fp + 0x1a), NULL);
      if (!moving) {
        if (*(int16_t *)(fp + 0xc) == 0) {
          first_person_weapon_start_interpolation(local_player_index, 6);
        }
        *(int16_t *)(fp + 0x1a) = -1;
      }
    } else if (moving) {
      element = *(int *)(antr_tag + 0x48) == 0
                  ? NULL
                  : (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
      *(int16_t *)(fp + 0x1c) = 0;
      *(int16_t *)(fp + 0x1a) =
        *(int *)(element + 0x10) > 3
          ? *(int16_t *)(*(int *)(element + 0x14) + 6)
          : (int16_t)-1;
    }

    if (*(int16_t *)(fp + 0x20) == -1) {
      if (*(int16_t *)(fp + 0xc) == 4) {
        element = *(int *)(antr_tag + 0x48) == 0
                    ? NULL
                    : (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c);
        *(float *)(fp + 0x24) = 0.0f;
        *(int16_t *)(fp + 0x20) =
          *(int *)(element + 0x10) > 0xf
            ? *(int16_t *)(*(int *)(element + 0x14) + 0x1e)
            : (int16_t)-1;
      }
    } else if (*(int16_t *)(fp + 0xc) == 4) {
      int frame_count;

      element = (char *)tag_block_get_element(
        antr_tag + 0x74, (int)*(int16_t *)(fp + 0x20), 0xb4);
      frame_count = (int)*(int16_t *)(element + 0x22);
      /* FUN_001daf7e == _CIfmod (FPREM loop) */
#if defined(_MSC_VER) && !defined(__clang__)
      *(float *)(fp + 0x24) = (float)fmod(
        (double)((*(float *)(weapon + 0x1f4) + *(float *)0x2533c8) * 2.0f +
                 *(float *)(fp + 0x24)),
        (double)frame_count);
#else
      *(float *)(fp + 0x24) =
        x87_fmod((*(float *)(weapon + 0x1f4) + *(float *)0x2533c8) * 2.0f +
                   *(float *)(fp + 0x24),
                 (double)frame_count);
#endif
    } else {
      *(int16_t *)(fp + 0x20) = -1;
    }

    if (*(uint8_t *)(fp + 0x50) != 0) {
      accelerate_to_position((float *)(fp + 0x30), (float *)(fp + 0x38),
                             *(float *)(unit + 0x228), 0.08f, 0.5f, -1.0f, 1.0f,
                             0);
      accelerate_to_position((float *)(fp + 0x34), (float *)(fp + 0x3c),
                             *(float *)(unit + 0x22c), 0.08f, 0.5f, -1.0f, 1.0f,
                             0);
      turning_i = signed_angular_difference(*(float *)(fp + 0x68),
                                        *(float *)(fp + 0x60)) *
              *(float *)0x253394;
      turning_j = signed_angular_difference(*(float *)(fp + 0x6c),
                                        *(float *)(fp + 0x64)) *
              *(float *)0x282490;
      if (turning_i < *(float *)0x255e94) {
        turning_i = -1.0f;
      } else if (turning_i > *(float *)0x2533c8) {
        turning_i = 1.0f;
      }
      if (turning_j < *(float *)0x255e94) {
        turning_j = -1.0f;
      } else if (turning_j > *(float *)0x2533c8) {
        turning_j = 1.0f;
      }
      accelerate_to_position((float *)(fp + 0x40), (float *)(fp + 0x48), turning_i,
                             0.03f, 0.2f, -1.0f, 1.0f, 0);
      accelerate_to_position((float *)(fp + 0x44), (float *)(fp + 0x4c), turning_j,
                             0.03f, 0.2f, -1.0f, 1.0f, 0);
    }

    accelerate_to_position((float *)(fp + 0x28), (float *)(fp + 0x2c), 0.0f,
                           0.01f, 0.2f, 0.0f, 1.0f, 0);
    /* binary compares the raw dword against 0x3f800000 (1.0f) */
    if (*(int *)(fp + 0x28) == 0x3f800000) {
      *(float *)(fp + 0x2c) = 0.0f;
    }

    if (*(int16_t *)(fp + 0x8a) > 0) {
      (*(int16_t *)(fp + 0x88))++;
      if (*(int16_t *)(fp + 0x88) >= *(int16_t *)(fp + 0x8a)) {
        *(int16_t *)(fp + 0x8a) = 0;
      }
    }

    if (player_control_get_autoaim_level(local_player_index) ==
          *(float *)0x2533c0 &&
        player_control_get_zoom_level(local_player_index) == -1 &&
        *(float *)(fp + 0x28) == *(float *)0x2533c0 &&
        *(float *)(fp + 0x30) == *(float *)0x2533c0 &&
        *(float *)(fp + 0x34) == *(float *)0x2533c0 &&
        *(float *)(fp + 0x40) == *(float *)0x2533c0 &&
        *(float *)(fp + 0x44) == *(float *)0x2533c0) {
      if (*(int16_t *)(fp + 0xc) == 0) {
        char *globals_element;

        globals_element = (char *)tag_block_get_element(
          (char *)game_globals_get() + 0x170, 0, 0xf4);
        if (*(int16_t *)(fp + 0xe) == 0) {
          *(int16_t *)(fp + 0xe) =
            (int16_t)(int)(FUN_000849f0(*(float *)(globals_element + 0x9c),
                                        *(float *)(globals_element + 0xa0)) *
                           *(float *)0x253394);
        }
        (*(int16_t *)(fp + 0x10))++;
        if (*(int16_t *)(fp + 0x10) > *(int16_t *)(fp + 0xe)) {
          *(int16_t *)(fp + 0xe) = 0;
          if (real_local_random() >= *(float *)(globals_element + 0xa4)) {
            first_person_weapon_set_state(local_player_index, 5, 1);
          }
        }
      } else {
        *(int16_t *)(fp + 0x10) = 0;
      }
    } else {
      *(int16_t *)(fp + 0x10) = 0;
      if (*(int16_t *)(fp + 0xc) == 5) {
        first_person_weapon_set_state(local_player_index, 0, 1);
      }
    }
  }

  (*(int16_t *)(fp + 0x12))--;
  if (*(int16_t *)(fp + 0x12) <= 0) {
    first_person_weapon_predict(local_player_index);
  }
}

/* Update first-person weapon state for all local players. Detects when
 * a player's controlled unit changes and reinitializes their weapon
 * rendering state. Calls per-player weapon update each frame. */
void first_person_weapons_update(void)
{
  int i;
  int offset;

  offset = 0;
  for (i = 0; (int16_t)i < 4; i++, offset += 0x1ea0) {
    int player_handle;
    char *fp;
    void *player;
    int unit;

    player_handle = local_player_get_player_index(i);
    if (player_handle == NONE)
      continue;

    assert_halt((int16_t)i >= 0 &&
                (int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    fp = (char *)*(int *)0x46bea8 + offset;
    player = datum_get(player_data, player_handle);
    unit = *(int *)((char *)player + 0x34);

    if (*(int *)(fp + 4) != unit) {
      assert_halt((int16_t)i >= 0 &&
                  (int16_t)i < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
      *(uint8_t *)(fp + 0x50) = 0;
      *(int *)(fp + 4) = unit;
      first_person_weapon_switch_weapons(i);
    }
    if (*(int *)(fp + 8) == NONE)
      first_person_weapon_switch_weapons(i);
    first_person_weapon_update(i);
  }
}
