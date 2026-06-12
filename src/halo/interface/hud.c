#include "x87_math.h"

void hud_dispose(void)
{
  FUN_000db140();
  hud_messaging_dispose();
  FUN_000d8b80();
  FUN_000d7430();
  FUN_000d46e0();
}

void hud_initialize_for_new_map(void)
{
  csmemset(*(void **)0x46bd10, 0, 4);
  *(char *)*(void **)0x46bd10 = 1;
  if (interface_get_tag_index(6) == -1) {
    display_assert("interface_get_tag_index(_interface_hud_globals)!=NONE",
                   "c:\\halo\\SOURCE\\interface\\hud.c", 0x71, 1);
    system_exit(-1);
  }
  hud_globals = tag_get(0x68756467, interface_get_tag_index(6));
  FUN_000d46a0();
  FUN_000d7330();
  FUN_000d8b30();
  hud_messaging_initialize_for_new_map();
  FUN_000db150();
}

void hud_dispose_from_old_map(void)
{
  FUN_000db1b0();
  hud_messaging_dispose_from_old_map();
  FUN_000d8b70();
  FUN_000d7420();
  FUN_000d46d0();
}

/* HaloScript: set whether the HUD is shown. Writes param to HUD control
 * block byte 0 and returns the updated value (double-reads the global
 * pointer to match MSVC register allocation). */
char scripted_show_hud(char visible)
{
  **(char **)0x0046bd10 = visible;
  return **(char **)0x0046bd10;
}

/* HaloScript: set whether HUD help text is shown. Same pattern as
 * scripted_show_hud but targets HUD control block byte 1. */
char scripted_show_hud_help_text(char visible)
{
  (*(char **)0x0046bd10)[1] = visible;
  return (*(char **)0x0046bd10)[1];
}

void hud_update(void)
{
  int i;
  int player_handle;
  void *player;

  ((void (*)(void))0xda980)();
  ((void (*)(void))0xd7d10)();
  ((void (*)(void))0xd7080)();
  ((void (*)(void))0xd51b0)();
  if (game_engine_force_single_screen()) {
    for (i = 0; (int16_t)i < 4; i++) {
      player_handle = local_player_get_player_index(i);
      if (player_handle != -1) {
        player = datum_get(player_data, player_handle);
        ((void (*)(void *, int))0xd7560)(player, 0);
      }
    }
  }
}

wchar_t *hud_get_item_string(int index)
{
  int tag_handle;
  int *tag;

  tag_handle = *(int *)((char *)hud_globals + 0xa0);
  if (tag_handle != -1) {
    tag = (int *)tag_get(0x75737472, tag_handle);
    if (tag != NULL && index >= 0 && index < *tag)
      return (wchar_t *)((int (*)(int, int))0x19d420)(tag_handle, index);
  }
  return *(wchar_t **)0x2f66bc;
}

/* Notify the HUD that a local player has picked up equipment.
 * If the local player index is valid (not NONE), forwards to the HUD
 * messaging handler at 0xd5240 with param_3=1 and param_4=-1. */
void hud_player_set_equipment(unsigned __int16 local_player_index,
                              int equipment_tag_handle)
{
  if ((int16_t)local_player_index != NONE)
    hud_messaging_set_vehicle_notification(local_player_index,
                                           equipment_tag_handle, 1, -1);
}

/* Notify the HUD that a local player has entered a vehicle seat.
 * If the local player index is valid (not NONE), forwards to the HUD
 * messaging handler at 0xd5240 with the seat index and param_4=1. */
void hud_player_enter_vehicle(unsigned __int16 local_player_index,
                              int tag_handle, int16_t seat_index)
{
  if ((int16_t)local_player_index != NONE)
    hud_messaging_set_vehicle_notification(local_player_index, tag_handle,
                                           seat_index, 1);
}

/* Notify the HUD that a local player has entered a vehicle.
 * If the local player index is valid (not NONE), forwards to the HUD
 * vehicle notification handler at 0xd5240 with zeroed extra params. */
void hud_player_set_vehicle(unsigned __int16 local_player_index,
                            int vehicle_tag_handle)
{
  if ((int16_t)local_player_index != NONE)
    hud_messaging_set_vehicle_notification(local_player_index,
                                           vehicle_tag_handle, 0, 0);
}

/* Notify the HUD that a local player has changed vehicle seat.
 * If the local player index is valid (not NONE), forwards to the HUD
 * vehicle notification handler at 0xd5240 with zeroed extra params. */
void hud_player_set_vehicle_seat(unsigned __int16 local_player_index,
                                 int vehicle_tag_handle)
{
  if ((int16_t)local_player_index != NONE)
    hud_messaging_set_vehicle_notification(local_player_index,
                                           vehicle_tag_handle, 0, 0);
}

void hud_load(bool a1)
{
  __int16 v1;

  if (a1) {
    v1 = *((_WORD *)hud_globals + 492);
  } else {
    v1 = *((_WORD *)hud_globals + 493);
  }
  scripted_hud_messages_clear();
  if (v1 != -1) {
    hud_print_message(local_player_get_next(-1), hud_get_item_string(v1));
  }
}

void hud_autosave(int16_t param)
{
  int16_t string_index;
  int i;
  int player_handle;
  wchar_t *message;

  if (param == 0)
    string_index = *(int16_t *)((char *)hud_globals + 0x3de);
  else
    string_index = *(int16_t *)((char *)hud_globals + 0x3dc);
  ((void (*)(void))0xd5120)();
  if (param != 0 && *(int *)((char *)hud_globals + 0x3ec) != -1)
    ((void (*)(int, int))0x1c7480)(*(int *)((char *)hud_globals + 0x3ec),
                                   0x3f800000);
  if (string_index != -1) {
    for (i = 0; (int16_t)i < 4; i++) {
      player_handle = local_player_get_player_index(i);
      if (player_handle != -1) {
        message = hud_get_item_string(string_index);
        ((void (*)(int, wchar_t *))0xd51c0)(i, message);
      }
    }
  }
}

/* Scan int array backwards from index 127, return first index where element
 * is not the sentinel 0x62626262 ("bbbb"). Returns -1 if all are sentinel.
 * Loop counter is short (16-bit); OR AX,0xffff sign-extends -1 to int. */
int FUN_000d1550(int param_1)
{
  short iVar1;

  iVar1 = 0x7f;
  do {
    if (*(int *)(param_1 + (int)iVar1 * 4) != 0x62626262)
      return (int)iVar1;
    iVar1--;
  } while (iVar1 >= 0);
  return iVar1;
}

/* Looks up a HUD bitmap-widget element: indexes the 'bitm' tag's widget block
 * by hud_widget_index, then its sub-block by (param_1 % count), returning the
 * element pointer + 8 (or NULL if any index is invalid).  Stack-guard
 * instrumented (hud_draw.c). */
void *FUN_000d1580(int tag_index, short hud_widget_index, short param_1)
{
  int return_addr;
  int guard[128];
  void *block;
  void *inner_block;
  void *elem;
  int block_count;
  int inner_block_count;
  int widget_idx;
  int param1_mod;
  short i;
  short corrupt_index;

  elem = NULL;
  return_addr = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);

  if (tag_index == -1 || hud_widget_index == -1 || param_1 == -1) {
    goto done_scan;
  }

  block = tag_get(0x6269746d, tag_index);
  block_count = *(int *)((char *)block + 0x54);
  block = (char *)block + 0x54;
  widget_idx = (int)hud_widget_index;
  if (widget_idx >= block_count) {
    goto done_scan;
  }

  inner_block = tag_block_get_element(block, widget_idx, 0x40);
  inner_block_count = *(int *)((char *)inner_block + 0x34);
  inner_block = (char *)inner_block + 0x34;
  if (inner_block_count == 0) {
    goto done_scan;
  }

  param1_mod = (int)param_1 % inner_block_count;
  elem = (char *)tag_block_get_element(inner_block, param1_mod, 0x20) + 8;

done_scan:
  corrupt_index = -1;
  for (i = 0x7f; i >= 0; i--) {
    if (guard[(int)i] != 0x62626262) {
      corrupt_index = i;
      break;
    }
  }

  if (FUN_000d1540() != return_addr) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 100, 1);
    system_exit(-1);
  }

  if (corrupt_index != -1) {
    display_assert(
        csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)corrupt_index),
        "c:\\halo\\SOURCE\\interface\\hud_draw.c", 100, 1);
    system_exit(-1);
  }

  return elem;
}

/* Predicate: returns 1 when a weapon-interface-state buffer is "displayable"
 * — either it has a non-zero count field (+0x10) with both +0xe and +0x12
 * clear, or its scalar field (+0x4) equals 1.0f.  EAX = state buffer. */
int FUN_000d02c0(void *state_buf)
{
  if (*(short *)((char *)state_buf + 0x10) != 0 &&
      *(short *)((char *)state_buf + 0xe) == 0 &&
      *(short *)((char *)state_buf + 0x12) == 0) {
    return 1;
  }
  if (*(unsigned int *)((char *)state_buf + 4) == 0x3f800000) {
    return 1;
  }
  return 0;
}

/* Updates the player's weapon/equipment HUD state message: handles respawn
 * failure prompts, then a weapon-HUD-state machine (held weapon, vehicle seat,
 * equipment, pickups, seat weapon, custom string, and weapon-switch search).
 * EAX = local player index. */
void FUN_000d04d0(int local_player_index)
{
  void *player_rec;
  short respawn_failure;
  int item_handle;
  void *item_obj;
  int item_wphi_tag_index;
  short weapon_hud_state;
  int unit_handle;
  void *unit_obj;
  int vehicle_unit_handle;
  short vehicle_seat_index;
  void *vehicle_obj;
  void *seat_elem;
  int initial_slot;
  int initial_weapon_handle;
  int current_weapon_handle;
  int next_slot;
  int total_weapons;
  int is_valid;
  void *obj_result;
  void *weap_tag;
  void *wphi_tag;
  int wphi_index;
  short *wphi_ptr;
  void *unit_obj89;
  short weapon_slot_89;
  void *seat_block89;
  short slot_indicator;
  void *obj10;
  short custom_idx;
  int d04a0_result;
  unsigned char wif_buf[0x20];
  wchar_t wchar_buf[0x400];

  player_rec = datum_get(*(data_t **)0x5aa6d4, local_player_index);
  respawn_failure = players_get_respawn_failure();

  if (respawn_failure != 0 && *(int *)((char *)player_rec + 0x34) == -1) {
    switch ((int)respawn_failure) {
    case 1:
      hud_set_state_message(*(short *)0x506548, 0xb);
      return;
    case 2:
      hud_set_state_message(*(short *)0x506548, 0xa);
      return;
    case 3:
      hud_set_state_message(*(short *)0x506548, 0x9);
      return;
    case 4:
      hud_set_state_message(*(short *)0x506548, 0xc);
      return;
    default:
      display_assert("!\"unreachable\"", "c:\\halo\\SOURCE\\interface\\hud.c",
                     0x1a8, 1);
      system_exit(-1);
      return;
    }
  }

  item_handle = *(int *)((char *)player_rec + 0x24);
  item_wphi_tag_index = -1;
  if (item_handle != -1) {
    item_obj = object_get_and_verify_type(item_handle, -1);
    item_wphi_tag_index =
        *(short *)((char *)tag_get(0x6f626a65, *(int *)item_obj) + 0x13c);
  }

  weapon_hud_state = *(short *)((char *)player_rec + 0x28);
  unit_handle = *(int *)((char *)player_rec + 0x34);

  switch ((int)weapon_hud_state) {
  case 1:
  case 2:
    hud_set_state_message(*(short *)0x506548, 0);
    hud_set_state_message_text(*(short *)0x506548, 0, (short)item_wphi_tag_index,
                               0);
    return;

  case 3:
    unit_obj = object_get_and_verify_type(unit_handle, 3);
    hud_set_state_message(*(short *)0x506548, 7);
    d04a0_result = FUN_000d04a0(*(int *)((char *)unit_obj + 0xcc));
    hud_set_state_message_text(*(short *)0x506548, 0, (short)d04a0_result, 0);
    return;

  case 5:
    hud_set_state_message(*(short *)0x506548, 1);
    d04a0_result = FUN_000d04a0(unit_get_equipment(unit_handle));
    hud_set_state_message_text(*(short *)0x506548, 0, (short)d04a0_result, 0);
    hud_set_state_message_text(*(short *)0x506548, 1, (short)item_wphi_tag_index,
                               0);
    return;

  case 6:
    obj_result =
        object_try_and_get_and_verify_type(*(int *)((char *)player_rec + 0x24), 4);
    if (obj_result == NULL) {
      break;
    }
    weap_tag = tag_get(0x77656170, *(int *)obj_result);
    wphi_index = *(int *)((char *)weap_tag + 0x48c);
    wphi_ptr = NULL;
    if (wphi_index != -1) {
      wphi_tag = tag_get(0x77706869, wphi_index);
      wphi_ptr = (short *)((char *)wphi_tag + 0x13c);
      if (*wphi_ptr == -1) {
        wphi_ptr = NULL;
      }
    }
    hud_set_state_message(*(short *)0x506548, 4);
    if (wphi_ptr != NULL) {
      hud_set_state_message_icon(*(short *)0x506548, 0, (int)wphi_ptr);
      return;
    }
    hud_set_state_message_text(*(short *)0x506548, 0, (short)item_wphi_tag_index,
                               0);
    return;

  case 7:
    obj_result =
        object_try_and_get_and_verify_type(*(int *)((char *)player_rec + 0x24), 4);
    if (obj_result == NULL) {
      break;
    }
    weap_tag = tag_get(0x77656170, *(int *)obj_result);
    wphi_index = *(int *)((char *)weap_tag + 0x48c);
    wphi_ptr = NULL;
    if (wphi_index != -1) {
      wphi_tag = tag_get(0x77706869, wphi_index);
      wphi_ptr = (short *)((char *)wphi_tag + 0x13c);
      if (*wphi_ptr == -1) {
        wphi_ptr = NULL;
      }
    }
    hud_set_state_message(*(short *)0x506548, 0);
    if (wphi_ptr != NULL) {
      hud_set_state_message_icon(*(short *)0x506548, 0, (int)wphi_ptr);
      return;
    }
    hud_set_state_message_text(*(short *)0x506548, 0, (short)item_wphi_tag_index,
                               0);
    return;

  case 8:
  case 9:
    unit_obj89 =
        object_get_and_verify_type(*(int *)((char *)player_rec + 0x24), 3);
    weapon_slot_89 = *(short *)((char *)unit_obj89 + 0x2a);
    hud_set_state_message(*(short *)0x506548, 6);
    seat_block89 = tag_block_get_element(
        (char *)tag_get(0x756e6974, *(int *)unit_obj89) + 0x2e4,
        (int)weapon_slot_89, 0x11c);
    slot_indicator = *(short *)((char *)seat_block89 + 0xec);
    hud_set_state_message_text(*(short *)0x506548, 0, slot_indicator, 0);
    hud_set_state_message_text(*(short *)0x506548, 1, (short)item_wphi_tag_index,
                               0);
    return;

  case 10:
    obj10 = object_get_and_verify_type(*(int *)((char *)player_rec + 0x24), 0x100);
    custom_idx = *(short *)((char *)obj10 + 0x1c8);
    if (custom_idx != -1) {
      hud_set_state_message(*(short *)0x506548, 3);
      hud_set_state_message_text(*(short *)0x506548, 0, custom_idx, 1);
      return;
    }
    hud_set_state_message(*(short *)0x506548, 2);
    hud_set_state_message_text(*(short *)0x506548, 0, (short)item_wphi_tag_index,
                               0);
    return;

  case 11:
    hud_set_state_message(*(short *)0x506548, 8);
    d04a0_result = FUN_000d04a0(*(int *)((char *)player_rec + 0x24));
    hud_set_state_message_text(*(short *)0x506548, 0, (short)d04a0_result, 0);
    return;

  default:
    if (FUN_000ae110(local_player_index, (int)wchar_buf, 0x400)) {
      hud_enable_custom_state_message(*(short *)0x506548, 1);
      hud_set_state_text(*(short *)0x506548, wchar_buf);
      return;
    }
    if (unit_handle == -1) {
      hud_enable_custom_state_message(*(short *)0x506548, 0);
      return;
    }

    unit_obj = object_get_and_verify_type(unit_handle, 3);
    initial_slot = (int)*(short *)((char *)unit_obj + 0x2a2);
    initial_weapon_handle = unit_get_weapon(unit_handle, (short)initial_slot);

    unit_obj = object_get_and_verify_type(unit_handle, 3);
    vehicle_unit_handle = *(int *)((char *)unit_obj + 0xcc);
    vehicle_seat_index = *(short *)((char *)unit_obj + 0x2a0);

    is_valid = 1;
    if (vehicle_unit_handle != -1 && vehicle_seat_index != -1) {
      vehicle_obj = object_get_and_verify_type(vehicle_unit_handle, 3);
      seat_elem = tag_block_get_element(
          (char *)tag_get(0x756e6974, *(int *)vehicle_obj) + 0x2e4,
          (int)vehicle_seat_index, 0x11c);
      if ((*(unsigned char *)seat_elem & 0xc) != 0) {
        is_valid = 0;
      } else {
        is_valid = 1;
      }
    }

    if (initial_weapon_handle == -1 || !is_valid) {
      goto disable;
    }

    weapon_build_weapon_interface_state(initial_weapon_handle, (int)wif_buf);
    if (!FUN_000d02c0(wif_buf)) {
      goto disable;
    }

    total_weapons = unit_count_weapons(unit_handle);
    next_slot = initial_slot;
    do {
      next_slot = unit_inventory_next_weapon(unit_handle, next_slot, 1);
      current_weapon_handle = unit_get_weapon(unit_handle, (short)next_slot);
      weapon_build_weapon_interface_state(current_weapon_handle, (int)wif_buf);
      if (!(*(short *)(wif_buf + 0x10) != 0 && *(short *)(wif_buf + 0xe) == 0 &&
            *(short *)(wif_buf + 0x12) == 0)) {
        if (*(float *)(wif_buf + 4) == *(float *)0x2533c8) {
          break;
        }
      }
      if (current_weapon_handle == initial_weapon_handle) {
        break;
      }
      total_weapons--;
      if ((short)total_weapons == 0) {
        continue;
      }
      break;
    } while (1);

    if (FUN_000d02c0(wif_buf) || current_weapon_handle == initial_weapon_handle) {
      goto disable;
    }

    hud_set_state_message(*(short *)0x506548, 5);
    obj_result = object_try_and_get_and_verify_type(current_weapon_handle, 4);
    if (obj_result == NULL) {
      goto disable;
    }
    weap_tag = tag_get(0x77656170, *(int *)obj_result);
    wphi_index = *(int *)((char *)weap_tag + 0x48c);
    if (wphi_index != -1) {
      wphi_tag = tag_get(0x77706869, wphi_index);
      if (*(short *)((char *)wphi_tag + 0x13c) != -1) {
        hud_set_state_message_icon(*(short *)0x506548, 0,
                                   (int)((char *)wphi_tag + 0x13c));
        return;
      }
    }
    hud_set_state_message_text(*(short *)0x506548, 0, (short)item_wphi_tag_index,
                               0);
    return;

  disable:
    hud_enable_custom_state_message(*(short *)0x506548, 0);
    return;
  }
}

/* Projects a player's biped head position to screen space and draws a rotating
 * crosshair/reticle sprite there.  The reticle's rotation angle is derived from
 * the head's camera-space depth (mapped through a near/far range and clamped).
 * EAX = local player handle. */
void FUN_000d0e90(int player_handle)
{
  void *player;
  float head_pos[3];
  float cam_pos[3];
  float screen_xy[2];
  int screen_int;
  short offset_xy[2];
  int sprite_handle;
  float depth_ratio;
  float t;
  float color_struct[4];

  player = datum_get(*(data_t **)0x5aa6d4, player_handle);
  unit_get_head_position(*(int *)((char *)player + 0x34), head_pos);
  head_pos[2] += *(float *)0x2533e4;
  matrix_transform_point((float *)0x5065b4, head_pos, cam_pos);
  if (render_camera_view_to_screen((int *)0x506550, (int *)0x5065a4, cam_pos,
                                   screen_xy) == '\0') {
    return;
  }

  sprite_handle = interface_get_tag_index(9);
  sprite_handle = (int)FUN_00077040(sprite_handle, 0, 0);
  if (xbox_texture_cache_get_hardware_format((void *)sprite_handle, 0, 1) ==
      NULL) {
    return;
  }

  screen_int = (int)(screen_xy[0] + 0.5f);
  offset_xy[0] = (short)(screen_int - *(short *)0x50657e);

  screen_int = (int)(screen_xy[1] + 0.5f);
  depth_ratio = (-cam_pos[2] - *(float *)0x50667c) * *(float *)0x253f00 /
                (*(float *)0x506680 - *(float *)0x50667c);
  offset_xy[1] = (short)(screen_int - *(int *)0x50657c);

  t = *(float *)0x2533c8 - depth_ratio;
  if (*(float *)0x256988 <= t) {
    if (*(float *)0x2533c8 < t) {
      t = *(float *)0x2533c8;
    }
  } else {
    t = *(float *)0x256988;
  }

  color_struct[0] = 0.0f;
  color_struct[1] = 1.0f;
  color_struct[2] = (*(float *)0x2533c8 - t) * *(float *)0x253398;
  color_struct[3] = 1.0f;

  interface_draw_bitmap(sprite_handle, offset_xy, color_struct, 1.0f, 0, 1.0f);
}

/* Draws the rotating crosshair (FUN_000d0e90) for every OTHER live player on
 * the local player's team (same team, valid unit, different handle). */
void FUN_000d0ff0(void)
{
  int player_index;
  void *player_datum;
  int team;
  int count;
  int handles[16];
  data_iter_t iter;
  void *datum;
  int handle;
  int i;

  player_index = local_player_get_player_index((int16_t) * (short *)0x506548);
  player_datum = datum_get(*(data_t **)0x5aa6d4, player_index);
  team = *(int *)((char *)player_datum + 0x20);
  if (player_index == -1) {
    return;
  }
  count = 0;
  data_iterator_new(&iter, *(data_t **)0x5aa6d4);
  datum = data_iterator_next(&iter);
  while (datum != NULL) {
    handle = *((int *)&iter + 2); /* iter.datum_handle at iter+8 */
    if (player_index != handle && *(int *)((char *)datum + 0x20) == team &&
        *(int *)((char *)datum + 0x34) != -1) {
      handles[count] = handle;
      count++;
    }
    datum = data_iterator_next(&iter);
  }
  for (i = 0; i < count; i++) {
    FUN_000d0e90(handles[i]);
  }
}

/* Draws a 16-segment HUD ring (e.g. a charge/cooldown indicator).  param_1
 * sets the ring radius via tan(); each of 16 vertices is placed on the circle
 * at depth -0.0625, transformed by the HUD matrix at 0x5065e8, then drawn as
 * connected segments with style param_2. */
void FUN_000d0c80(float param_1, int param_2)
{
  float vertices[16][3];
  float radius;
  float angle;
  int i;

  radius = x87_fptan(param_1 * *(float *)0x253398) * *(float *)0x255d90;

  angle = 0.0f;
  for (i = 0; i < 16; i++) {
    vertices[i][2] = -0.0625f;
    vertices[i][0] = x87_fcos(angle) * radius;
    vertices[i][1] = x87_fsin(angle) * radius;
    matrix_transform_point((float *)0x5065e8, vertices[i], vertices[i]);
    angle += *(float *)0x26b164;
  }

  for (i = 1; i <= 16; i++) {
    FUN_0017eb10(vertices[i - 1], vertices[i % 16], param_2);
  }
}

/* Returns a weapon's HUD interface index (int16 at the weapon's 'obje' tag
 * +0x13c), or -1 if the handle is NONE.  Frameless leaf; EAX = weapon handle. */
int FUN_000d04a0(int weapon_handle)
{
  void *obj;
  void *tag;

  if (weapon_handle == -1) {
    return -1;
  }
  obj = object_get_and_verify_type(weapon_handle, -1);
  tag = tag_get(0x6f626a65, *(int *)obj);
  return (int)*(short *)((char *)tag + 0x13c);
}

/* Resolves a bitmap data element (and optional sprite element) from a 'bitm'
 * tag given a sequence and frame index, writing the results to *out_bitmap and
 * *out_sprite.  Stack-guard instrumented (hud_draw.c). */
void FUN_000d16a0(int bitmap_tag, short sequence_index, unsigned int frame_index,
                  int *out_bitmap, int *out_sprite)
{
  int return_addr;
  int guard[128];
  void *tag_data;
  void *seq_elem;
  void *sprite_elem;
  int sprite_count;
  int bitmap_index;
  short corrupt;

  return_addr = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);

  if (out_bitmap == NULL) {
    display_assert("bitmap", "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0xc1, 1);
    system_exit(-1);
  }
  if (out_sprite == NULL) {
    display_assert("clip", "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0xc2, 1);
    system_exit(-1);
  }

  if (bitmap_tag != -1) {
    tag_data = tag_get(0x6269746d, bitmap_tag);
    if ((int)(short)sequence_index < *(int *)((char *)tag_data + 0x54)) {
      seq_elem = tag_block_get_element((char *)tag_data + 0x54,
                                       (int)(short)sequence_index, 0x40);
      frame_index = frame_index & 0x7fff;
      if ((short)frame_index < 0) {
        display_assert("frame_index >= 0",
                       "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0xcd, 1);
        system_exit(-1);
      }
      sprite_count = *(int *)((char *)seq_elem + 0x34);
      if (sprite_count == 0) {
        *out_bitmap =
            (int)FUN_00077040(bitmap_tag, sequence_index, (short)frame_index);
      } else {
        sprite_elem = tag_block_get_element(
            (char *)seq_elem + 0x34, (int)(short)frame_index % sprite_count, 0x20);
        bitmap_index = (int)*(short *)sprite_elem;
        *out_bitmap =
            (int)tag_block_get_element((char *)tag_data + 0x60, bitmap_index, 0x30);
      }
    }
  }

  if (*out_bitmap == 0) {
    *out_sprite = 0;
  } else {
    *out_sprite =
        (int)FUN_000d1580(bitmap_tag, sequence_index, (short)frame_index);
  }

  corrupt = 0x7f;
  do {
    if (guard[(int)corrupt] != 0x62626262) {
      goto found_corrupt;
    }
    corrupt--;
  } while (corrupt >= 0);
  corrupt = -1;
found_corrupt:
  if (FUN_000d1540() != return_addr) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0xe4, 1);
    system_exit(-1);
  }
  if (corrupt != -1) {
    display_assert(
        csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)corrupt),
        "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0xe4, 1);
    system_exit(-1);
  }
}

float FUN_000d1690(int split_screen)
{
  return *(float *)0x002533c8;
}

/* HUD weapon-interface draw helper (hud_draw.c). EAX = render/player state.
 * Resolves the player's current weapon; if the player has none, falls back to
 * the parent unit's weapon (gated by a 'unit' tag flag bit).  When a weapon is
 * found, builds its HUD interface state.  Returns true if a weapon was drawn.
 * Wrapped in the hud_draw debug instrumentation: a return-address capture plus
 * a 0x200-byte 0x62 stack canary, both re-checked on exit. */
char FUN_000d1a70(int render, int param_1)
{
  int return_address;
  int guard[128];
  void *obj;
  void *parent_unit;
  unsigned char *flags_elem;
  void *weapon_tag;
  int weapon;
  char drew;
  short corrupt_index;
  short i;

  drew = 0;
  return_address = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);

  obj = object_get_and_verify_type(*(int *)(render + 0x34), 3);
  weapon = unit_get_weapon(*(int *)(render + 0x34),
                           *(short *)((char *)obj + 0x2a2));
  if (weapon == -1) {
    obj = object_get_and_verify_type(*(int *)(render + 0x34), 3);
    if (*(int *)((char *)obj + 0xcc) != -1 &&
        *(short *)((char *)obj + 0x2a0) != -1) {
      parent_unit = object_get_and_verify_type(*(int *)((char *)obj + 0xcc), 3);
      weapon_tag = tag_get(0x756e6974, *(int *)parent_unit);
      flags_elem = (unsigned char *)tag_block_get_element(
          (char *)weapon_tag + 0x2e4, *(short *)((char *)obj + 0x2a0), 0x11c);
      if ((*flags_elem & 8) != 0) {
        parent_unit =
            object_get_and_verify_type(*(int *)((char *)obj + 0xcc), 3);
        weapon = unit_get_weapon(*(int *)((char *)obj + 0xcc),
                                 *(short *)((char *)parent_unit + 0x2a2));
      }
    }
  }

  if (*(short *)(render + 2) != *(short *)0x506548) {
    display_assert("player->local_player_index==render.local_player_index",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x3fd, 1);
    system_exit(-1);
  }

  if (weapon != -1) {
    weapon_tag = object_get_and_verify_type(weapon, 4);
    tag_get(0x77656170, *(int *)weapon_tag);
    weapon_build_weapon_interface_state(weapon, param_1);
    drew = 1;
  }

  corrupt_index = -1;
  for (i = 0x7f; i >= 0; i--) {
    if (guard[i] != 0x62626262) {
      corrupt_index = i;
      break;
    }
  }
  if (return_address != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x408, 1);
    system_exit(-1);
  }
  if (corrupt_index != -1) {
    display_assert(csprintf((char *)0x5ab100, "corrupt stack at %d!",
                            (int)corrupt_index),
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x408, 1);
    system_exit(-1);
  }
  return drew;
}

/* Truncate a float toward zero to a 32-bit int (C cast semantics).
 * The original is an inline /QIfist truncation helper: it FISTs (round to
 * nearest), then corrects back toward zero via an integer-bits SBB/SETG of the
 * remainder.  Verified equivalent to (int)x by exhaustive sweep. */
int FUN_000d1c50(float param_1)
{
  return (int)param_1;
}

/* Scales the float at struct offset +8 by a global factor and rounds to the
 * nearest int (original uses x87 FISTP round-to-nearest). */
int FUN_000d2300(int param_1)
{
  float v;

  v = *(float *)(param_1 + 8) * *(float *)0x253394;
  return (int)(v < 0.0f ? v - 0.5f : v + 0.5f);
}

/* Resolves the animated HUD meter color: decompresses two packed colors,
 * computes a cosine-eased blend factor for the current point in the fade
 * cycle, and returns the packed ARGB result.  param_1 -> color-cycle
 * descriptor (floats at [2]=cycle duration, [3]+[5]=segment span, [5]=fade-in
 * span; int16 at +0x10 = segment count; byte at +0x12 bit0 = reverse flag).
 * param_2 = animation base time (0 selects an endpoint by flag, no blend).
 * Stack-guard instrumented (hud_draw.c). */
uint32_t FUN_000d2320(int *param_1, int param_2)
{
  int return_address;
  int guard[128];
  int elapsed_ticks;
  float color_a[4];
  float color_b[4];
  float out_color[4];
  float elapsed_f;
  float period_total;
  int period_count_i;
  short period_count;
  float elapsed_mod;
  float t;
  float t_sqrt;
  short corrupt_index;
  short i;

  return_address = FUN_000d1540();
  csmemset(guard, 0x62, 0x200);

  elapsed_ticks = game_time_get() - param_2;
  elapsed_f = x87_fmod((float)elapsed_ticks * *(float *)0x2546a4,
                       (double)((float *)param_1)[2]);

  pixel32_to_real_argb_color((unsigned int)param_1[0], color_a);
  pixel32_to_real_argb_color((unsigned int)param_1[1], color_b);

  period_total = ((float *)param_1)[3] + ((float *)param_1)[5];
  period_count = *(short *)((char *)param_1 + 0x10);
  period_count_i = (int)period_count;

  if ((float)period_count_i * period_total <= elapsed_f) {
    out_color[0] = color_a[0];
    out_color[1] = color_a[1];
    out_color[2] = color_a[2];
    out_color[3] = color_a[3];
    goto check_guard;
  }

  elapsed_mod = x87_fmod(elapsed_f, (double)period_total);

  if (param_2 == 0) {
    if (*(char *)((char *)param_1 + 0x12) & 1) {
      out_color[0] = color_a[0];
      out_color[1] = color_a[1];
      out_color[2] = color_a[2];
      out_color[3] = color_a[3];
    } else {
      out_color[0] = color_b[0];
      out_color[1] = color_b[1];
      out_color[2] = color_b[2];
      out_color[3] = color_b[3];
    }
    goto check_guard;
  }

  if (elapsed_mod < ((float *)param_1)[5]) {
    t = (float)(1.0 - ((double)x87_fcos_mul(elapsed_mod / ((float *)param_1)[5],
                                            *(float *)0x281a78) + 1.0) * 0.5);
    if (*(float *)0x2533c0 <= t) {
      if (*(float *)0x2533c8 < t) {
        t = *(float *)0x2533c8;
      }
    } else {
      t = *(float *)0x2533c0;
    }
    t_sqrt = sqrtf(t);
    if (*(char *)((char *)param_1 + 0x12) & 1) {
      vectors_interpolate(color_b, color_a, t_sqrt, out_color);
      scalars_interpolate(color_b[3], color_a[3], t_sqrt, &out_color[3]);
    } else {
      vectors_interpolate(color_a, color_b, t_sqrt, out_color);
      scalars_interpolate(color_a[3], color_b[3], t_sqrt, &out_color[3]);
    }
  } else {
    if (*(char *)((char *)param_1 + 0x12) & 1) {
      out_color[0] = color_b[0];
      out_color[1] = color_b[1];
      out_color[2] = color_b[2];
      out_color[3] = color_b[3];
    } else {
      out_color[0] = color_a[0];
      out_color[1] = color_a[1];
      out_color[2] = color_a[2];
      out_color[3] = color_a[3];
    }
  }

check_guard:
  corrupt_index = -1;
  for (i = 0x7f; i >= 0; i--) {
    if (guard[i] != 0x62626262) {
      corrupt_index = i;
      break;
    }
  }
  if (return_address != FUN_000d1540()) {
    display_assert("corrupt return address!",
                   "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x137, 1);
    system_exit(-1);
  }
  if (corrupt_index != -1) {
    display_assert(
        csprintf((char *)0x5ab100, "corrupt stack at %d!", (int)corrupt_index),
        "c:\\halo\\SOURCE\\interface\\hud_draw.c", 0x137, 1);
    system_exit(-1);
  }
  return FUN_000d1c90(out_color);
}

uint32_t FUN_000d1c90(float *color)
{
  int a;
  int r;
  int g;
  int b;
  uint32_t result;
  float ca;
  float cr;
  float cg;
  float cb;

  if (!valid_real_argb_color(color)) {
    /* clamp overbright tag colors instead of halting — cachebeta contrails
     * store HDR color bounds > 1.0 that would crash the original assert. */
    ca = color[0] < 0.0f ? 0.0f : (color[0] > 1.0f ? 1.0f : color[0]);
    cr = color[1] < 0.0f ? 0.0f : (color[1] > 1.0f ? 1.0f : color[1]);
    cg = color[2] < 0.0f ? 0.0f : (color[2] > 1.0f ? 1.0f : color[2]);
    cb = color[3] < 0.0f ? 0.0f : (color[3] > 1.0f ? 1.0f : color[3]);
    a = (int)(ca * 255.0f + 0.5f);
    r = (int)(cr * 255.0f + 0.5f);
    g = (int)(cg * 255.0f + 0.5f);
    b = (int)(cb * 255.0f + 0.5f);
    return (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16) |
           ((uint32_t)a << 24);
  }

  a = (int)(color[0] * 255.0f + 0.5f);
  r = (int)(color[1] * 255.0f + 0.5f);
  g = (int)(color[2] * 255.0f + 0.5f);
  b = (int)(color[3] * 255.0f + 0.5f);
  result = (uint32_t)b | ((uint32_t)g << 8) | ((uint32_t)r << 16) |
           ((uint32_t)a << 24);

  if (((uint32_t)(b & 0xff) | ((uint32_t)(g & 0xff) << 8) |
       ((uint32_t)(r & 0xff) << 16) | ((uint32_t)a << 24)) != result) {
    display_assert("verify == result", "..\\bitmaps\\bitmaps_inlines.h", 0xbc,
                   true);
    system_exit(-1);
  }

  return result;
}

void FUN_000d1e90(float alpha, float intensity)
{
  float color[4];

  if (alpha < 0.0f || alpha > 1.0f) {
    display_assert("alpha>=0.0f && alpha<=1.0f",
                   "..\\bitmaps\\bitmaps_inlines.h", 0x13f, true);
    system_exit(-1);
  }
  if (intensity < 0.0f || intensity > 1.0f) {
    display_assert("intensity>=0.0f && intensity<=1.0f",
                   "..\\bitmaps\\bitmaps_inlines.h", 0x140, true);
    system_exit(-1);
  }
  color[0] = alpha;
  color[1] = intensity;
  color[2] = intensity;
  color[3] = intensity;
  FUN_000d1c90(color);
}

/* Packs a 3-component real RGB color (each 0.0..1.0) into a 0x00RRGGBB
 * 32-bit pixel.  Validates the color via valid_real_rgb_color() and, on
 * failure, formats an assert message (bitmaps_inlines.h:0xc9) and halts.
 * Sibling of FUN_000d1c90 (the 4-component ARGB variant). */
unsigned int FUN_000d1dd0(float *color)
{
  float scale;
  int packed;
  char *msg;

  scale = 255.0f;
  if (!valid_real_rgb_color(color)) {
    msg = csprintf((char *)0x5ab100,
                   "%s: assert_valid_real_rgb_color(%f, %f, %f)", "color",
                   (double)color[0], (double)color[1], (double)color[2]);
    display_assert(msg, "..\\bitmaps\\bitmaps_inlines.h", 0xc9, 1);
    system_exit(-1);
  }

  /* Original rounds via x87 FISTP (round-to-nearest); the products are always
   * positive (color is validated to [0,1]), so the +0.5f truncation idiom is
   * behaviorally equivalent under our clang/-mno-sse build. */
  packed = (int)(color[2] * scale + 0.5f) & 0xff;
  packed |= ((int)(color[1] * scale + 0.5f) & 0xff) << 8;
  packed |= ((int)(color[0] * scale + 0.5f) & 0xff) << 16;
  return (unsigned int)packed;
}
