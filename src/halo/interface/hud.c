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
