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

float FUN_000d1690(int split_screen)
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

/* Draw weapon/equipment name and status text for the local player's HUD.
 * Builds a multi-line string in the shared HUD text buffer (0x5ab100) from
 * the local player's held weapon/equipment tags, then renders it using the
 * rasterizer text pipeline.  Covers:
 *   - Equipment name (or "|n" if none)
 *   - Grenade count + type name
 *   - Weapon name + ammo counts + heat + age (if magazines present) or just name
 *   - Active-camouflage / full-spectrum-vision status strings
 *   - hud_set_element_digital calls for zoom ratio and ammo fill
 * Screen position is loaded from 0x506584 (x|y packed), y offset by +100.
 * Color pointer from 0x2ee6c4; style/justify/flags set to (-1,0,0). */
void FUN_000d1090(void)
{
  int player_handle;
  char *player;
  int weapon_obj_handle;
  void *unit_obj;
  void *weapon_obj;
  void *weapon_tag;
  const char *name_ptr;
  const char *base_name;
  int equip_handle;
  void *mag_block_elem;
  int rounds_loaded_curr;
  int rounds_loaded_max;
  int rounds_total;
  int rounds_loaded_max2;
  float heat;
  float age;
  float fvar;
  const char *fsv_str;
  const char *ac_str;
  float autoaim;
  int16_t zoom_level;
  float magnification;
  unsigned int flags_dword;
  unsigned int *tag_block_flags;
  int screen_pos[2]; /* EBP-0xc: packed as short x, short y, then [1]=color */

  player_handle = local_player_get_player_index(
      (int16_t)*(int *)0x506548);
  if (player_handle == -1)
    return;

  player = (char *)datum_get(*(data_t **)0x5aa6d4, player_handle);
  if (*(int *)(player + 0x34) == -1)
    return;

  /* Get unit object (type 3) and its tag ('unit') */
  unit_obj = object_get_and_verify_type(*(int *)(player + 0x34), 3);
  tag_get(0x756e6974, *(int *)unit_obj);

  /* Second call to get unit_obj again for weapon slot read (matches disasm) */
  {
    void *tmp_unit;
    tmp_unit = object_get_and_verify_type(*(int *)(player + 0x34), 3);
    weapon_obj_handle = unit_get_weapon(
        *(int *)(player + 0x34),
        *(int16_t *)((char *)tmp_unit + 0x2a2));
  }

  /* Get equipment handle */
  equip_handle = unit_get_equipment(*(int *)(player + 0x34));

  /* --- Equipment block: first (and only) write from offset 0 in buf --- */
  if (equip_handle != -1) {
    void *equip_obj;
    equip_obj = object_get_and_verify_type(equip_handle, -1);
    name_ptr = tag_get_name(*(int *)equip_obj);
    base_name = strrchr(name_ptr, 0x5c);
    crt_sprintf((char *)0x5ab100,
                "%s (press WHITE to use)|n", base_name + 1);
  } else {
    /* No equipment: just a newline */
    crt_sprintf((char *)0x5ab100, "|n");
  }

  /* --- Grenade block --- */
  if (*(char *)((char *)unit_obj + 0x2cc) != (char)-1) {
    void *game_globals;
    void *grenade_elem;
    const char *grenade_name;
    int gren_idx;
    int grenade_count;

    gren_idx = (int)*(signed char *)((char *)unit_obj + 0x2cc);
    game_globals = game_globals_get();
    grenade_elem = tag_block_get_element(
        (char *)game_globals + 0x128, gren_idx, 0x44);
    name_ptr = tag_get_name(*(int *)((char *)grenade_elem + 0x40));
    grenade_name = tag_name_strip_path(name_ptr);
    /* grenade count = [unit_obj + 0x2ce + grenade_type_index] */
    grenade_count = (int)*(signed char *)((char *)unit_obj
                   + 0x2ce + gren_idx);
    crt_sprintf((char *)0x5ab100 + csstrlen((char *)0x5ab100),
                "%d %s|n", grenade_count, grenade_name);
  }

  /* --- Weapon block --- */
  if (weapon_obj_handle != -1) {
    void *mag_block;

    weapon_obj = object_get_and_verify_type(weapon_obj_handle, 4);
    weapon_tag = tag_get(0x77656170, *(int *)weapon_obj);

    mag_block = (char *)weapon_tag + 0x4f0;

    if (*(int *)mag_block > 0) {
      /* Has magazines: full ammo display */
      mag_block_elem = tag_block_get_element(
          mag_block, 0, 0x70);

      heat = *(float *)((char *)weapon_obj + 0x1ec);
      age  = *(float *)((char *)weapon_obj + 0x1f0);
      rounds_loaded_curr = (int)*(int16_t *)((char *)weapon_obj + 0x25e);
      rounds_total       = (int)*(int16_t *)((char *)weapon_obj + 0x260);
      rounds_loaded_max  = (int)*(int16_t *)((char *)mag_block_elem + 0x8);
      rounds_loaded_max2 = (int)*(int16_t *)((char *)mag_block_elem + 0xa);

      name_ptr  = tag_get_name(*(int *)weapon_obj);
      base_name = strrchr(name_ptr, 0x5c);
      crt_sprintf((char *)0x5ab100 + csstrlen((char *)0x5ab100),
                  "%s|ntotal %d/%d|nloaded %d/%d|nheat %3.2f|nage %3.2f|n",
                  base_name + 1,
                  rounds_loaded_curr, rounds_loaded_max,
                  rounds_total, rounds_loaded_max2,
                  (double)heat, (double)age);
    } else {
      /* No magazines: weapon name via tag_name_strip_path (original uses
         tag_name_strip_path which returns strrchr_result+1; caller adds
         another +1 in the INC EAX at 0xd1282) */
      name_ptr  = tag_get_name(*(int *)weapon_obj);
      base_name = tag_name_strip_path(name_ptr);
      crt_sprintf((char *)0x5ab100 + csstrlen((char *)0x5ab100),
                  "%s|n", base_name + 1);
    }

    /* Cloak / FSV status */
    {
      int scratch;
      float fsv_f, ac_f;
      /* original: MOVSX+store to [EBP-4], FILD from [EBP-4] */
      scratch = (int)*(int16_t *)(player + 0x6a); /* fsv_timer */
      fsv_f = (float)scratch;
      scratch = (int)*(int16_t *)(player + 0x68); /* ac_timer */
      ac_f = (float)scratch;
      fsv_str = "FULL-SPECTRUM VISION ";
      if (!(fsv_f > 0.0f)) fsv_str = "";
      ac_str = "ACTIVE-CAMOUFLAGE ";
      if (!(ac_f > 0.0f)) ac_str = "";

      crt_sprintf((char *)0x5ab100 + csstrlen((char *)0x5ab100),
                  "%s%s", ac_str, fsv_str);
    }

    /* Autoaim level → select draw pointer */
    {
      const void *draw_ptr;
      autoaim = player_control_get_autoaim_level(
          (int16_t)*(int *)0x506548);
      draw_ptr = (autoaim > 0.0f)
                 ? *(const void **)0x2ee6d0
                 : *(const void **)0x2ee6d8;

      zoom_level  = (int16_t)player_control_get_zoom_level(
          (int16_t)*(int *)0x506548, draw_ptr);
      magnification = weapon_get_zoom_magnification(
          weapon_obj_handle, zoom_level);

      hud_set_element_digital(
          *(float *)((char *)weapon_tag + 0x3e4) / magnification,
          draw_ptr);
    }

    /* Ammo fill indicator */
    {
      tag_block_flags = (unsigned int *)tag_block_get_element(
          (char *)weapon_tag + 0x4fc, 0, 0x114);
      flags_dword = tag_block_flags[0];

      if (flags_dword & 0x200) {
        fvar = *(float *)((char *)weapon_obj + 0x1e4);
      } else {
        fvar = *(float *)((char *)weapon_obj + 0x22c);
      }

      hud_set_element_digital(
          fvar * *(float *)((char *)tag_block_flags + 0x80) +
          (1.0f - fvar) * *(float *)((char *)tag_block_flags + 0x7c),
          *(const void **)0x2ee6e0);
    }
  }

  /* --- Draw block (always) --- */
  screen_pos[0] = *(int *)0x506584;
  ((int16_t *)screen_pos)[1] += 100;   /* y += 100 (hi-word of screen_pos[0]) */
  screen_pos[1] = *(int *)0x506588;

  draw_string_set_style_justify_flags(-1, 0, 0);
  draw_string_set_color(*(const void **)0x2ee6c4);
  rasterizer_text_draw(screen_pos, 0, 0, 0, (char *)0x5ab100);
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
