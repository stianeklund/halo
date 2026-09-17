#include "x87_math.h"

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
void FUN_000dc7a0(void)
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
 * out-of-range state returns -1. */
int FUN_000dc8c0(int16_t state)
{
  switch (state) {
  case 0:
    return 0;
  case 1:
    return 0x15;
  case 2:
    return 0x16;
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
  case 15:
    return 0x17;
  case 16:
    return 0x18;
  case 17:
    return 0x19;
  case 18:
    return 0xb;
  case 19:
    return 0xa;
  case 20:
    return 0x10;
  case 21:
    return 0x14;
  case 22:
    return 0x1a;
  case 23:
    return 0x1b;
  default:
    return (int16_t)-1;
  }
}

/* Try to play a third-person weapon sound for an object event (0xdc9d0).
 * When no local player owns the weapon, this function looks up the weapon's
 * animation graph tag, maps the event through two state-translation tables
 * (FUN_000dc800 and FUN_000dc8c0), resolves the animation's sound effect
 * tag reference, and plays it at the global origin with default forward. */
void FUN_000dc9d0(int param_2, int object_handle)
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

  anim_index = FUN_000dc8c0(state);
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
void *FUN_000dcaf0(int16_t local_player_index)
{
  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  return (void *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
}

/* Toggle the first-person weapon activation state for a local player (0xdcb30).
 * When activating (activate != 0): asserts weapon_index != NONE, then calls
 * effects_start_on_first_person_weapon to start effects. When deactivating:
 * calls effects_stop_on_first_person_weapon to stop effects and FUN_000a1510 to
 * stop sounds. Only acts if the state changes. */
void FUN_000dcb30(int16_t local_player_index, uint8_t activate)
{
  char *fp;

  assert_halt(local_player_index >= 0 &&
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
    FUN_000a1510((int)local_player_index);
    *(uint8_t *)fp = 0;
  }
}

/* Copy animation node transforms from the animation graph into the
 * first-person node buffer using a node remap table (0xdcbd0).
 * For each model node i, node_remap[i] gives the source index in the
 * animation graph node array. Copies 0x34 bytes (13 dwords) per node.
 * Asserts that every remap index is within [0, antr->nodes.count). */
void fp_anim_apply_node_remap(int mode_tag_index, int fp_nodes,
                              int antr_tag_index, int anim_nodes,
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
    assert_halt(remap_idx >= 0 && (int)remap_idx < *(int *)(antr_tag + 0x68));
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
uint8_t FUN_000dcc80(int mode_tag_index, int antr_tag_index, int16_t *output)
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
int16_t FUN_000dcd60(int object_handle)
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
int16_t FUN_000dcdc0(int unit_handle)
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
void FUN_000dce00(int16_t local_player_index)
{
  char *fp;
  int weapon_handle;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  weapon_handle =
    *(int *)((int)local_player_index * 0x1ea0 + 8 + *(int *)0x46bea8);
  fp = (char *)((int)local_player_index * 0x1ea0 + *(int *)0x46bea8);

  if (weapon_handle != -1) {
    int *weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    char *weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
    predicted_resources_precache((int *)(weapon_tag + 0x4e4));
  }

  *(int16_t *)(fp + 0x12) = 0x1e;
}

/* 0xdce80 — first_person_weapon_draw */
void first_person_weapon_draw(void)
{
  int16_t current_player;
  char *fp;
  int player_index;
  char *player;
  int unit_handle;
  char *unit;
  char *weapon;
  char *tag;
  int antr_tag_index;
  int light_index;
  uint16_t light_flags[2];
  char node_matrices[3328];
  char *antr_data;

  current_player = *(int16_t *)0x506548;
  if (current_player == -1) {
    return;
  }
  if (current_player < 0 || current_player >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + (int)current_player * 0x1ea0;
  player_index = player_index_from_unit_index((int)current_player);
  if (player_index == -1) {
    return;
  }

  player = (char *)datum_get(player_data, player_index);
  unit_handle = *(int *)(player + 0x34);
  if (unit_handle == -1 || !*fp || *(int *)(fp + 4) == -1 || *(int *)(fp + 8) == -1) {
    return;
  }

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  antr_tag_index = *(int *)(tag + 0x478);
  if (antr_tag_index == -1) {
    return;
  }

  tag_get(0x616e7472, antr_tag_index);
  light_index = (int)scenario_leaf_index_from_point(unit_handle, 3.4028235e+38f);
  if ((*(uint8_t *)(unit + 0x1b4) & 0x10) || *(float *)(unit + 0x32c) > 0.0f) {
    light_flags[0] = 1;
  } else {
    light_flags[0] = 0;
  }

  if (*(uint8_t *)(fp + 0x1d8c) && *(int *)(tag + 0x468) != -1) {
    fp_anim_apply_node_remap(*(int *)(tag + 0x468), (int)node_matrices, antr_tag_index, (int)(fp + 0x108c), (int16_t *)(fp + 0x1d8e));
    render_model(*(int *)(tag + 0x468), 0.0f, (void *)node_matrices, 0, (void *)(weapon + 0x168), (void *)(weapon + 0xe4), light_index, (void *)0x506550, 0, (void *)light_flags, *(int *)(fp + 8), 0, 8);
  }

  if (*(uint8_t *)(fp + 0x1e0e)) {
    antr_data = (char *)tag_block_get_element((char *)game_globals_get() + 0x17c, 0, 0xc0);
    if (*(int *)(antr_data + 0xc) != -1) {
      fp_anim_apply_node_remap(*(int *)(antr_data + 0xc), (int)node_matrices, antr_tag_index, (int)(fp + 0x108c), (int16_t *)(fp + 0x1e10));
      render_model(*(int *)(antr_data + 0xc), 0.0f, (void *)node_matrices, 0, (void *)(unit + 0x168), (void *)(unit + 0xe4), light_index, (void *)0x506550, 0, (void *)light_flags, *(int *)(fp + 8), 0, 8);
    }
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

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  weapon_obj = (int *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(weapon_tag + 0x478));

  assert_halt(node_index >= 0 && (int)node_index < *(int *)(antr_tag + 0x68));

  return (void *)((int)node_index * 0x34 + 0x108c + (int)fp);
}

/* Copy animation node data from the current buffer to the blend buffer and
 * update blend timing (0xdd4d0). Copies node_count * 32 bytes from fp + 0x8c
 * to fp + 0x88c. If blend_ticks >= (fp[0x8a] - fp[0x88]), resets the blend
 * origin to 0 and sets the blend target to blend_ticks. */
void FUN_000dd4d0(int16_t local_player_index, int16_t blend_ticks)
{
  char *fp;
  int *weapon_obj;
  char *weapon_tag;
  char *antr_tag;

  assert_halt(local_player_index >= 0 &&
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

  weapon = (char *)object_get_and_verify_type(object_handle, 4);
  if (!weapon) {
    return 0;
  }

  local_player_index = FUN_000dcd60(object_handle);
  if (local_player_index == -1 || director_get_perspective(local_player_index) != 0) {
    return 0;
  }

  fp = (char *)FUN_000dcaf0(local_player_index);
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

  return FUN_00124730(
    mode_tag_index,
    (const char *)marker_name,
    0,
    (int)(fp + 0x1d8e),
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
  char marker_buf[0x6c];
  float *marker_forward;
  float *marker_position;
  float *marker_up;

  local_player_index = FUN_000dcdc0(object_handle);
  if (local_player_index == -1) {
    return;
  }

  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + (int)local_player_index * 0x1ea0;
  if (!*(uint8_t *)fp) {
    return;
  }

  if (first_person_weapon_get_marker_by_name(*(int *)(fp + 8), (void *)"flashlight", marker_buf, 1) <= 0) {
    return;
  }

  marker_forward = (float *)(marker_buf + 0x3c);
  marker_up = (float *)(marker_buf + 0x54);
  marker_position = (float *)(marker_buf + 0x60);

  out_position[0] = marker_position[0] - marker_forward[0] * 0.5f;
  out_position[1] = marker_position[1] - marker_forward[1] * 0.5f;
  out_position[2] = marker_position[2] - marker_forward[2] * 0.5f;

  out_forward[0] = marker_forward[0];
  out_forward[1] = marker_forward[1];
  out_forward[2] = marker_forward[2];

  if (out_up) {
    ((float *)out_up)[0] = marker_up[0];
    ((float *)out_up)[1] = marker_up[1];
    ((float *)out_up)[2] = marker_up[2];
  }
}

/* 0xdd340 — first_person_weapon_adjust_light */
char first_person_weapon_adjust_light(int object_handle, int marker_result, void *out_position, void *out_forward, void *out_up)
{
  char *weapon;
  char *unit;
  int player_datum_index;
  char *player_datum;
  int16_t local_player_index;
  char *fp;
  char marker_buf[0x6c];
  float *marker_forward;
  float *marker_up;
  float *marker_position;

  weapon = (char *)object_get_and_verify_type(object_handle, 4);
  unit = (char *)object_get_and_verify_type(*(int *)(weapon + 0xcc), 3);
  player_datum_index = *(int *)(unit + 0x1c8);
  if (player_datum_index == -1) {
    return 0;
  }

  player_datum = (char *)datum_get(*(void **)0x5aa6d4, player_datum_index);
  local_player_index = *(int16_t *)(player_datum + 2);
  if (local_player_index == -1 || local_player_index != *(int16_t *)0x506548) {
    return 0;
  }

  fp = (char *)FUN_000dcaf0(local_player_index);
  if (!*(uint8_t *)fp) {
    return 0;
  }

  if (first_person_weapon_get_marker_by_name(object_handle, (void *)marker_result, marker_buf, 1) <= 0) {
    return 0;
  }

  marker_forward = (float *)(marker_buf + 0x3c);
  marker_up = (float *)(marker_buf + 0x54);
  marker_position = (float *)(marker_buf + 0x60);

  if (out_position) {
    ((float *)out_position)[0] = marker_position[0];
    ((float *)out_position)[1] = marker_position[1];
    ((float *)out_position)[2] = marker_position[2];
  }

  if (out_forward) {
    ((float *)out_forward)[0] = marker_forward[0];
    ((float *)out_forward)[1] = marker_forward[1];
    ((float *)out_forward)[2] = marker_forward[2];
  }

  if (out_up) {
    ((float *)out_up)[0] = marker_up[0];
    ((float *)out_up)[1] = marker_up[1];
    ((float *)out_up)[2] = marker_up[2];
  }

  return 1;
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
  if (*(int16_t *)0x506548 == FUN_000dcd60(object_handle)) {
    return first_person_weapon_get_marker_by_name(object_handle, marker_name,
                                                  out_markers, max_count);
  }

  return 0;
}

/* Set the first-person weapon animation state for a local player (0xddbd0).
 * Applies state-transition filtering: certain incoming states are rejected
 * depending on the current state. For dual-wielding weapons (type 3), maps
 * state 3 to state 0 unless the weapon has a specific flag. Looks up the
 * animation index via FUN_000dc8c0 and the animation graph to validate the
 * transition. If param_3 is nonzero, stops any pending sound. */
void FUN_000ddbd0(int param_1, int param_2, int param_3)
{
  int16_t local_player_index = (int16_t)param_1;
  int16_t state = (int16_t)param_2;
  char *fp;
  int weapon_handle;
  int16_t sVar1;
  int16_t blend_ticks;

  assert_halt(local_player_index >= 0 &&
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

    sVar1 = FUN_000dc8c0(state);

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
              FUN_000dd4d0(local_player_index, blend_ticks);
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
void FUN_000dde80(int param_1)
{
  int16_t local_player_index = (int16_t)param_1;
  char *fp;
  uint8_t was_active;
  int weapon_handle;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
  was_active = *(uint8_t *)fp;

  /* Clear weapon handle. */
  *(int *)(fp + 8) = -1;

  /* If previously active, deactivate effects and sounds. */
  if (was_active != 0) {
    assert_halt(local_player_index >= 0 &&
                local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

    {
      char *fp2 = (char *)(*(int *)0x46bea8 + (int)local_player_index * 0x1ea0);
      if (*(uint8_t *)fp2 != 0) {
        effects_stop_on_first_person_weapon(param_1);
        FUN_000a1510(param_1);
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

    weapon_handle = unit_get_weapon(*(int *)(fp + 4), weapon_index);
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
              *(uint8_t *)(fp + 0x1e0e) = FUN_000dcc80(
                *(int *)(gg_element + 0xc), *(int *)(weapon_tag + 0x478),
                (int16_t *)(fp + 0x1e10));
            }
          }

          *(uint8_t *)(fp + 0x1d8c) = FUN_000dcc80(*(int *)(weapon_tag + 0x468),
                                                   *(int *)(weapon_tag + 0x478),
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

          FUN_000ddbd0(param_1, 0, 1);

          *(int16_t *)(fp + 0x8a) = 0;

          if (was_active != 0) {
            FUN_000dcb30(local_player_index, 1);
          }
        }
      }
    }
  }

done:
  FUN_000dce00(local_player_index);
}

/* Bind a first-person weapon state block to a player and reset it (0xde0e0).
 * local_player_index arrives in ESI (register argument); param_2 arrives on the
 * stack and is stored at fp+4 (the value later passed to
 * player_clear_aim_assist by FUN_000de140). Clears the byte at fp+0x50, then
 * runs the weapon-state reset in FUN_000dde80. */
void FUN_000de0e0(int local_player_index, int param_2)
{
  char *fp;

  assert_halt((int16_t)local_player_index >= 0 &&
              (int16_t)local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)(int16_t)local_player_index * 0x1ea0);

  *(uint8_t *)(fp + 0x50) = 0;
  *(int *)(fp + 4) = param_2;

  FUN_000dde80(local_player_index);
}

/* Process a weapon event for a local player's first-person weapon (0xde140).
 * Handles reload initiation, weapon put-away, aim-assist clearing, and state
 * transitions. Computes reload count from trigger data and weapon ammo state,
 * then selects the appropriate animation state. */
void FUN_000de140(int param_1, int param_2)
{
  char *fp;
  int saved_event;
  int new_state;

  if ((int16_t)param_1 == -1)
    return;

  assert_halt((int16_t)param_1 >= 0 &&
              (int16_t)param_1 < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);

  fp = (char *)(*(int *)0x46bea8 + (int)(int16_t)param_1 * 0x1ea0);
  saved_event = (int)(int16_t)param_2;

  switch ((int16_t)param_2) {
  case 0:
    *(float *)(fp + 0x2c) += 0.05f;
    break;
  case 9:
  case 10:
    player_clear_aim_assist(*(int *)(fp + 4));
    break;
  case 0xc:
    FUN_000dde80(param_1);
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
  FUN_000ddbd0(param_1, new_state, 1);

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

  local_player = FUN_000dcdc0(unit_handle);
  FUN_000de140(local_player, message_type);
  if (local_player == -1) {
    char *unit;
    int16_t weapon_index;

    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    weapon_index = *(int16_t *)(unit + 0x2a2);
    if (weapon_index != -1) {
      FUN_000dc9d0(message_type, (int)weapon_index);
    }
  }
}

/* Notify the first-person weapon system of an object event (0xde3b0).
 * Finds the local player holding the weapon, processes the event, and if
 * no local player owns it, attempts third-person sound playback. */
void first_person_weapon_message_from_weapon(int object_handle, int param_2)
{
  int16_t local_player = FUN_000dcd60(object_handle);
  FUN_000de140(local_player, param_2);
  if (local_player == -1) {
    FUN_000dc9d0(param_2, object_handle);
  }
}

/* 0xdd580 — first_person_weapon_update */
void first_person_weapon_update(int16_t local_player_index)
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
    char *anim_node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x16), 0xb4);
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
        int frame = (int)*(int16_t *)(weapon + 0x260);
        int16_t diff = *(int16_t *)(weapon + 0x25c) - *(int16_t *)(weapon + 0x25a);
        if (diff >= 0x2c) {
          char *mag_def = (char *)tag_block_get_element(tag + 0x4f0, 0, 0x70);
          float frac = (float)(diff - 0x2c) * 0.2f;
          int16_t max_rounds = *(int16_t *)(mag_def + 0xa);
          int16_t rounds = *(int16_t *)(weapon + 0x25e);
          if (frac > 1.0f) frac = 1.0f;
          if (rounds > max_rounds) rounds = max_rounds;
          frame += (int)((float)(rounds - frame) * frac);
        }
        overlay_animation_apply(node_elem, frame, matrices);
      }
    }
  }

  if (*(int16_t *)(fp + 0x1a) != -1) {
    char *node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x1a), 0xb4);
    overlay_animation_apply(node, (int)*(int16_t *)(fp + 0x1c), matrices);
  }

  if (*(int16_t *)(fp + 0x20) != -1) {
    char *node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x20), 0xb4);
    FUN_00122a50(*(int *)(node + 0x20), *(float *)(fp + 0x24), *(float *)(weapon + 0x1f4) + *(float *)0x253398, (int)matrices);
  }

  if (*(int *)(unit_anim + 0x10) >= 5) {
    node_index = *(int16_t *)(*(int *)(unit_anim + 0x14) + 8);
    if (node_index != -1) {
      char *node = (char *)tag_block_get_element(antr_tag + 0x74, (int)node_index, 0xb4);
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

  FUN_000dcb30(current_player, activate);
  if (*fp != 0) {
    first_person_weapon_update(current_player);
  }
}

/* 0xde3f0 — first_person_weapon_next_state */
void first_person_weapon_next_state(int16_t local_player_index)
{
  char *fp;
  int16_t current_state;

  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + (int)local_player_index * 0x1ea0;
  current_state = *(int16_t *)(fp + 0xc);
  if ((uint16_t)current_state > 23) {
    return;
  }

  switch (current_state) {
  case 0:
  case 1:
  case 2:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
  case 15:
  case 16:
  case 19:
  case 20:
  case 21:
  case 22:
  case 23:
    FUN_000ddbd0((int)local_player_index, 0, 0);
    break;
  case 3:
    FUN_000ddbd0((int)local_player_index, 3, 0);
    break;
  case 4:
    (*(int16_t *)(fp + 0x18))--;
    break;
  case 13:
  case 14:
    {
      char *weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
      char *tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
      if (*(int16_t *)(tag + 0x4e2) == 1 && *(int16_t *)(fp + 0x1e94) == 2) {
        int next_state = (*(uint8_t *)(fp + 0x1e90) != 0) ? 0x10 : 0x11;
        FUN_000ddbd0((int)local_player_index, next_state, 0);
      } else {
        FUN_000ddbd0((int)local_player_index, 0, 0);
      }
    }
    break;
  case 17:
  case 18:
    {
      char *weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
      char *tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
      if (*(int16_t *)(tag + 0x4e2) == 1) {
        int16_t anim = *(int16_t *)(fp + 0x1e94);
        if (anim == 1 || anim == 2) {
          int next_state = (*(uint8_t *)(fp + 0x1e90) != 0) ? 0x10 : 0x11;
          FUN_000ddbd0((int)local_player_index, next_state, 0);
        } else if (anim != 0 && anim != -1) {
          display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x31f, 1);
          system_exit(-1);
        } else {
          FUN_000ddbd0((int)local_player_index, 0, 0);
        }
      } else {
        FUN_000ddbd0((int)local_player_index, 0, 0);
      }
    }
    break;
  }
}

/* 0xde560 — FUN_000de560 */
void FUN_000de560(int16_t local_player_index)
{
  char *fp;
  char *unit;
  char *weapon;
  char *tag;
  char *antr_tag;
  int anim_result;
  int anim_sound;
  float speed;
  boolean in_air;
  int sound_handle;
  float vel_x, vel_y, vel_z;
  float yaw_delta, pitch_delta;
  int new_state;

  if (local_player_index < 0 || local_player_index >= 4) {
    display_assert(0, "c:\\halo\\SOURCE\\interface\\first_person_weapons.c", 0x599, 1);
    system_exit(-1);
  }

  fp = (char *)*(int *)0x46bea8 + (int)local_player_index * 0x1ea0;

  if (*(int *)(fp + 8) != -1 && !object_get_and_verify_type(*(int *)(fp + 8), 4)) {
    error(3, "local player %d, weapon (0x%x), deleted unexpectedly", (int)local_player_index, *(int *)(fp + 8));
    *(int *)(fp + 8) = -1;
  }

  if (*(int *)(fp + 4) == -1 || *(int *)(fp + 8) == -1) {
    goto update_delay;
  }

  unit = (char *)object_get_and_verify_type(*(int *)(fp + 4), 3);
  weapon = (char *)object_get_and_verify_type(*(int *)(fp + 8), 4);
  tag = (char *)tag_get(0x77656170, *(uint32_t *)weapon);
  (void)tag_get(0x6d6f6465, *(uint32_t *)(tag + 0x468));
  antr_tag = (char *)tag_get(0x616e7472, *(uint32_t *)(tag + 0x478));

  if (*(int16_t *)(fp + 0xc) == 3 || *(int16_t *)(fp + 0xc) == 1) {
    if (*(uint8_t *)(weapon + 0x1dc) & 2) {
      FUN_000ddbd0((int)local_player_index, 0x16, 1);
    }
    if (!(*(uint8_t *)(weapon + 0x1dc) & 1)) {
      FUN_000ddbd0((int)local_player_index, 0, 1);
    }
  }

  anim_sound = -1;
  anim_result = animation_update_internal(0, *(int *)(tag + 0x478), (short *)(fp + 0x16), &anim_sound);
  if (anim_result != 1 && anim_result == 2) {
    first_person_weapon_next_state(local_player_index);
  }

  if (anim_sound != -1 && player_control_get_zoom_level(local_player_index) == -1) {
    sound_handle = object_impulse_sound_new(*(uint32_t *)(fp + 8), anim_sound, -1, (float *)0x31fc1c, (float *)0x31fc3c, 1.0f);
    *(uint32_t *)(fp + 0x1e98) = sound_handle;
    *(int16_t *)(fp + 0x1e9c) = *(int16_t *)(fp + 0xc);
  }

  vel_x = *(float *)(unit + 0x228);
  vel_y = *(float *)(unit + 0x22c);
  vel_z = *(float *)(unit + 0x230);
  speed = sqrtf(vel_x * vel_x + vel_y * vel_y + vel_z * vel_z);
  in_air = (boolean)biped_flying_through_air(*(int *)(fp + 4));

  if (*(int16_t *)(fp + 0x1a) != -1) {
    animation_update_internal(0, *(int *)(tag + 0x478), (short *)(fp + 0x1a), NULL);
    if (in_air || speed <= *(float *)0x25496c) {
      if (*(int16_t *)(fp + 0xc) == 0) {
        FUN_000dd4d0(local_player_index, 6);
      }
      *(int16_t *)(fp + 0x1a) = -1;
    }
  } else if (!in_air && speed > *(float *)0x25496c) {
    char *elem = (*(int *)(antr_tag + 0x48)) ? (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c) : NULL;
    *(int16_t *)(fp + 0x1c) = 0;
    *(int16_t *)(fp + 0x1a) = (elem && *(int *)(elem + 0x10) >= 4) ? *(int16_t *)(*(int *)(elem + 0x14) + 6) : -1;
  }

  if (*(int16_t *)(fp + 0x20) != -1) {
    if (*(int16_t *)(fp + 0xc) != 4) {
      *(int16_t *)(fp + 0x20) = -1;
    } else {
      char *node = (char *)tag_block_get_element(antr_tag + 0x74, (int)*(int16_t *)(fp + 0x20), 0xb4);
      *(float *)(fp + 0x24) = x87_fmod(*(float *)(fp + 0x24) + (*(float *)(weapon + 0x1f4) + 1.0f) * 2.0f, (double)*(int16_t *)(node + 0x22));
    }
  } else if (*(int16_t *)(fp + 0xc) == 4) {
    char *elem = (*(int *)(antr_tag + 0x48)) ? (char *)tag_block_get_element(antr_tag + 0x48, 0, 0x1c) : NULL;
    *(float *)(fp + 0x24) = 0.0f;
    *(int16_t *)(fp + 0x20) = (elem && *(int *)(elem + 0x10) >= 0x10) ? *(int16_t *)(*(int *)(elem + 0x14) + 0x1e) : -1;
  }

  if (*(uint8_t *)(fp + 0x50)) {
    accelerate_to_position((float *)(fp + 0x30), (float *)(fp + 0x38), *(float *)(unit + 0x228), 0.02f, 0.5f, -1.0f, 1.0f, 0);
    accelerate_to_position((float *)(fp + 0x34), (float *)(fp + 0x3c), *(float *)(unit + 0x22c), 0.02f, 0.5f, -1.0f, 1.0f, 0);

    yaw_delta = signed_angular_difference(*(float *)(fp + 0x68), *(float *)(fp + 0x60)) * *(float *)0x253394;
    pitch_delta = signed_angular_difference(*(float *)(fp + 0x6c), *(float *)(fp + 0x64)) * *(float *)0x282490;

    if (yaw_delta < *(float *)0x255e94) {
      yaw_delta = -1.0f;
    } else if (yaw_delta > 1.0f) {
      yaw_delta = 1.0f;
    }

    if (pitch_delta < *(float *)0x255e94) {
      pitch_delta = -1.0f;
    } else if (pitch_delta > 1.0f) {
      pitch_delta = 1.0f;
    }

    accelerate_to_position((float *)(fp + 0x40), (float *)(fp + 0x48), yaw_delta, 0.03f, 0.2f, -1.0f, 1.0f, 0);
    accelerate_to_position((float *)(fp + 0x44), (float *)(fp + 0x4c), pitch_delta, 0.03f, 0.2f, -1.0f, 1.0f, 0);
  }

  accelerate_to_position((float *)(fp + 0x28), (float *)(fp + 0x2c), 0.0f, 0.01f, 0.2f, 0.0f, 1.0f, 0);
  if (*(float *)(fp + 0x28) == 1.0f) {
    *(float *)(fp + 0x2c) = 0.0f;
  }

  if (*(int16_t *)(fp + 0x8a) >= 1) {
    (*(int16_t *)(fp + 0x88))++;
    if (*(int16_t *)(fp + 0x88) >= *(int16_t *)(fp + 0x8a)) {
      *(int16_t *)(fp + 0x8a) = 0;
    }
  }

  if (player_control_get_autoaim_level(local_player_index) != 0.0f ||
      player_control_get_zoom_level(local_player_index) != -1 ||
      *(float *)(fp + 0x28) != 0.0f ||
      *(float *)(fp + 0x30) != 0.0f || *(float *)(fp + 0x34) != 0.0f ||
      *(float *)(fp + 0x40) != 0.0f || *(float *)(fp + 0x44) != 0.0f) {
    *(int16_t *)(fp + 0x10) = 0;
    if (*(int16_t *)(fp + 0xc) == 5) {
      new_state = 0;
      FUN_000ddbd0((int)local_player_index, new_state, 1);
    }
  } else {
    if (*(int16_t *)(fp + 0xc) != 0) {
      *(int16_t *)(fp + 0x10) = 0;
    } else {
      char *hud_def = (char *)tag_block_get_element((char *)game_globals_get() + 0x170, 0, 0xb8);
      if (*(int16_t *)(fp + 0xe) == 0) {
        *(int16_t *)(fp + 0xe) = (int16_t)(int)(FUN_000849f0(*(float *)(hud_def + 0x9c), *(float *)(hud_def + 0xa0)) * *(float *)0x253394);
      }
      (*(int16_t *)(fp + 0x10))++;
      if (*(int16_t *)(fp + 0x10) > *(int16_t *)(fp + 0xe)) {
        *(int16_t *)(fp + 0xe) = 0;
        if (real_local_random() >= *(float *)(hud_def + 0xa4)) {
          new_state = 5;
          FUN_000ddbd0((int)local_player_index, new_state, 1);
        }
      }
    }
  }

update_delay:
  (*(int16_t *)(fp + 0x12))--;
  if (*(int16_t *)(fp + 0x12) < 1) {
    FUN_000dce00(local_player_index);
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
      FUN_000dde80(i);
    }
    if (*(int *)(fp + 8) == NONE)
      FUN_000dde80(i);
    FUN_000de560((int16_t)i);
  }
}
