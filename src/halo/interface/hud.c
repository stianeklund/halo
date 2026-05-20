void hud_dispose(void)
{
  FUN_000db140();
  FUN_000d6020();
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
  FUN_000d5ff0();
  FUN_000db150();
}

void hud_dispose_from_old_map(void)
{
  FUN_000db1b0();
  FUN_000d6010();
  FUN_000d8b70();
  FUN_000d7420();
  FUN_000d46d0();
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

float FUN_000d1690(void)
{
  return *(float *)0x002533c8;
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
