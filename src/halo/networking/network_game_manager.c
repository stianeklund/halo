#include <stdarg.h>
#define network_machine_is_valid(m)                     \
  ((m) != NULL && *(char *)((char *)(m) + 0x40) >= 0 && \
   *(char *)((char *)(m) + 0x40) < 4)


bool network_game_add_machine(void *game, void *machine)
{
  char *g = (char *)game;
  char *m = (char *)machine;
  int i;
  char *mach_ptr;

  if (g == NULL || m == NULL || !network_machine_is_valid(m)) {
    display_assert("game && machine && network_machine_is_valid(machine)",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x6a,
                   1);
    system_exit(-1);
  }

  for (i = 0; i < 4; i++) {
    if (!network_machine_is_valid(g + 0x114 + i * 0x44)) {
      break;
    }
  }

  if (i >= 4) {
    return false;
  }

  /* Original computes the target slot from the loop index (imul i,0x44 +
   * lea 0x114) rather than a running pointer carried out of the loop. */
  mach_ptr = g + 0x114 + i * 0x44;
  csmemcpy(mach_ptr, m, 0x44);
  *(short *)(g + 0x112) += 1;
  return true;
}

bool network_game_update_machine(void *game, void *machine)
{
  char *g = (char *)game;
  char *m = (char *)machine;
  int i;
  char *mach_ptr;

  if (g == NULL || m == NULL || !network_machine_is_valid(m)) {
    display_assert("game && machine && network_machine_is_valid(machine)",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x81,
                   1);
    system_exit(-1);
  }

  mach_ptr = g + 0x154;
  for (i = 0; i < 4; i++) {
    if (*mach_ptr == m[0x40]) {
      csmemcpy(g + 0x114 + i * 0x44, m, 0x44);
      return true;
    }
    mach_ptr += 0x44;
  }
  return false;
}

void xbox_set_machine_name(const char *name)
{
  wchar_t local_44[32];

  if (name != NULL && *name != '\0') {
    /* Success path is the inline fall-through; MSVC places the ascii_to_wide
     * failure branch out of line after the return, so test != NULL and keep
     * the error() call in the else. */
    if (ascii_to_wide(name, local_44, 0x40) != NULL) {
      local_44[31] = 0;
      if (!XSetNicknameW(local_44, 1)) {
        error(2, "XSetNickname() failed");
      }
    } else {
      error(
        2, "'%s' is not a valid machine name (max. name length= %d characters)",
        (const char *)local_44, 0x1f);
    }
  }
}

void network_game_generate_local_machine_name(void *name_buffer)
{
  int iVar1;
  char local_24[32];
  wchar_t *name;

  name = (wchar_t *)name_buffer;
  iVar1 = FUN_001d29eb(0, name, 0x20);
  if (iVar1 == -1) {
    ustrncpy(name, network_game_get_random_player_name(), 0x20);
    name[31] = 0;
    if (XSetNicknameW(name, 1) != 0) {
      error(2, "system nickname set to '%s'", wide_to_ascii(name, local_24, 0x20));
      name[31] = 0;
      return;
    }
    error(2, "XSetNickname() failed to set system nickname");
    name[31] = 0;
    return;
  }
  FUN_001d33a2(iVar1);
  name[31] = 0;
}

void network_game_end_and_load_ui(void *game)
{
  char *g = (char *)game;
  if (g[0x430] != '\0') {
    main_load_ui_scenario(true);
  }
  csmemset(g + 0x430, 0, 4);
}

void network_game_reset_for_next_round(void *game, bool flag)
{
  char *g = (char *)game;

  if (g == NULL) {
    display_assert(
      "game", "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x22d, 1);
    system_exit(-1);
  }

  if (!flag || g[0x430] == '\0') {
    csmemset(g + 0x430, 0, 4);
  } else {
    main_load_ui_scenario(true);
    csmemset(g + 0x430, 0, 4);
    if (global_network_game_server_get() != NULL) {
      set_game_connection(2);
      game_time_end();
      return;
    }
    if (global_network_game_client_get() != NULL) {
      set_game_connection(1);
      game_time_end();
      return;
    }
  }
  game_time_end();
}


bool network_player_is_valid(void *client)
{
  char *c = (char *)client;
  if (c != NULL && c[0x1d] >= 0 && c[0x1d] < 4 && c[0x1c] >= 0 && c[0x1c] < 4) {
    return true;
  }
  return false;
}

void network_game_invalidate_machine(void *game, uint16_t machine_index)
{
  char *g = (char *)game;
  int i;
  char *controller_ptr;
  uint16_t m = machine_index;

  if (g == NULL || m >= 4) {
    display_assert("game && (machine_index<MAXIMUM_NETWORK_MACHINE_COUNT)",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x40,
                   1);
    system_exit(-1);
  }

  g[0x154 + m * 0x44] = (char)0xff;
  *(short *)(g + 0x114 + m * 0x44) = 0;

  controller_ptr = g + 0x226 + 0x1d;
  for (i = 0; i < 16; i++) {
    if ((signed char)controller_ptr[-1] == (int)m) {
      if (controller_ptr - 0x1d == NULL) {
        display_assert("player",
                       "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                       0x58, 1);
        system_exit(-1);
      }
      controller_ptr[-1] = (char)0xff;
      controller_ptr[0] = (char)0xff;
      controller_ptr[1] = (char)0xff;
      controller_ptr[2] = (char)0xff;
      *(short *)(controller_ptr - 0x1d) = 0;
    }
    controller_ptr += 0x20;
  }
}

bool network_game_add_player(void *game, void *player)
{
  char *g = (char *)game;
  char *p = (char *)player;
  char *pcVar4;
  char *slot_ptr;
  int i;
  int empty_slot;
  char machine_index;
  char controller_index;
  bool result = false;

  if (g == NULL || p == NULL) {
    display_assert("game && player",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0xbb,
                   true);
    system_exit(-1);
  }

  if (*(short *)(g + 0x224) < (short)g[0x10e]) {
    machine_index = p[0x1c];
    if (machine_index >= 0 && machine_index < 4) {
      controller_index = p[0x1d];
      if (controller_index >= 0 && controller_index < 4) {
        /* Check for duplicates */
        i = 0;
        pcVar4 = g + 0x243;
        do {
          if (pcVar4[-1] == machine_index && *pcVar4 == controller_index) break;
          if (pcVar4[0x1f] == machine_index && pcVar4[0x20] == controller_index) {
            i += 1;
            break;
          }
          if (pcVar4[0x3f] == machine_index && pcVar4[0x40] == controller_index) {
            i += 2;
            break;
          }
          if (pcVar4[0x5f] == machine_index && pcVar4[0x60] == controller_index) {
            i += 3;
            break;
          }
          if (pcVar4[0x7f] == machine_index && pcVar4[0x80] == controller_index) {
            i += 4;
            break;
          }
          if (pcVar4[0x9f] == machine_index && pcVar4[0xa0] == controller_index) {
            i += 5;
            break;
          }
          if (pcVar4[0xbf] == machine_index && pcVar4[0xc0] == controller_index) {
            i += 6;
            break;
          }
          if (pcVar4[0xdf] == machine_index && pcVar4[0xe0] == controller_index) {
            i += 7;
            break;
          }
          i += 8;
          pcVar4 += 0x100;
        } while (i < 16);

        if (i == 16 && network_player_is_valid(p)) {
          /* Find empty slot (player_index == -1) */
          empty_slot = -1;
          slot_ptr = g + 0x245;
          for (i = 0; i < 16; i++, slot_ptr += 0x20) {
            if (*slot_ptr == (char)-1) {
              empty_slot = i;
              break;
            }
          }

          if ((p[0x1f] == (char)-1 || empty_slot == (int)(signed char)p[0x1f]) &&
              empty_slot != -1) {
            p[0x1f] = (char)empty_slot;
            csmemcpy(g + 0x226 + empty_slot * 0x20, p, 0x20);
            *(short *)(g + 0x224) += 1;
            result = true;
          }
        }
      }
    }
  } else {
    error(2, "game is already at maximum players; can't add new player");
  }
  return result;
}

int sort_network_players(void *p1, void *p2)
{
  char *player1 = (char *)p1;
  char *player2 = (char *)p2;

  if (player1 == NULL || player2 == NULL) {
    display_assert("p1 && p2",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                   0x142, 1);
    system_exit(-1);
  }

  if (!network_player_is_valid(player1) && !network_player_is_valid(player2)) {
    return 0;
  }
  if (!network_player_is_valid(player1) && network_player_is_valid(player2)) {
    return 1;
  }
  if (network_player_is_valid(player1) && !network_player_is_valid(player2)) {
    return -1;
  }

  /* Both are valid, compare machine indices first */
  if (player1[0x1c] <= player2[0x1c]) {
    if (player2[0x1c] <= player1[0x1c]) {
      if (player2[0x1d] < player1[0x1d]) {
        return 1;
      }
      if (player2[0x1d] <= player1[0x1d]) {
        display_assert("multiple players on the same machine cannot have the same "
                       "controller index",
                       "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x165,
                       true);
        system_exit(-1);
      }
    }
    return -1;
  }
  return 1;
}

bool network_game_spawn_player(void *player)
{
  char *p = (char *)player;
  bool is_local;
  int16_t controller;
  int player_index;

  if (!network_player_is_valid(p)) {
    display_assert("network_player_is_valid(player)",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                   0x1bc, 1);
    system_exit(-1);
  }

  is_local = network_game_player_is_local(p);
  if (is_local) {
    controller = (int16_t)p[0x1d];
  } else {
    controller = -1;
  }

  player_index = player_new((int)p[0x1c], 0xffffffff, controller, p);
  if (player_index != -1) {
    p[0x1f] = (char)player_index;
    return true;
  }
  return false;
}

bool network_game_player_is_valid(void *player, void *game)
{
  char *p = (char *)player;
  char *g = (char *)game;
  char machine_index;
  char controller_index;
  int i;
  char *controller_ptr;
  bool result;

  if (p == NULL || g == NULL) {
    display_assert("player && game",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                   0x247, 1);
    system_exit(-1);
  }

  result = false;
  if (p != NULL) {
    controller_index = p[0x1d];
    if (controller_index >= 0 && controller_index < 4) {
      machine_index = p[0x1c];
      if (machine_index >= 0 && machine_index < 4) {
        i = 0;
        controller_ptr = g + 0x226 + 0x1d;
        while (controller_ptr[-1] != machine_index ||
               *controller_ptr != controller_index) {
          i++;
          controller_ptr += 0x20;
          if (i >= 16) {
            goto done;
          }
        }
        result = true;
      }
    }
  }
done:
  return result;
}

void network_game_invalidate(void *game)
{
  char *g = (char *)game;
  int i;

  if (g == NULL) {
    display_assert(
      "game", "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x23, 1);
    system_exit(-1);
  }

  csmemset(g, 0, 0x434);
  csmemset(g + 0x20, 0, 0x84);
  *(short *)(g + 0x112) = 0;
  *(short *)(g + 0x224) = 0;

  for (i = 0; i < 4; i++) {
    network_game_invalidate_machine(g, i);
  }

  csmemset(g + 0x226, 0xff, 0x200);
  g[0x10d] = 2;
  g[0x10e] = 16;
  g[0x430] = 0;
}

bool network_game_update_player(void *game, void *player)
{
  char *g = (char *)game;
  char *p = (char *)player;
  int player_index;
  char *player_slot;

  if (g == NULL || p == NULL) {
    display_assert("game && player",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                   0x101, 1);
    system_exit(-1);
  }

  if (network_game_player_is_valid(p, g)) {
    player_index = (int)p[0x1f];
    player_slot = g + 0x226 + player_index * 0x20;
    if (player_slot[0x1d] == p[0x1d] && player_slot[0x1c] == p[0x1c]) {
      csmemcpy(player_slot, p, 0x20);
      return true;
    }
  }

  error(2, "tried to update a player with indvalid data");
  return false;
}

bool network_game_remove_player(void *game, void *player)
{
  char *g = (char *)game;
  char *p = (char *)player;
  int i;
  char *player_slot;

  if (g == NULL || p == NULL) {
    display_assert("game && player",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                   0x120, 1);
    system_exit(-1);
  }

  if (network_game_player_is_valid(p, g)) {
    player_slot = g + 0x226;
    for (i = 0; i < 16; i++) {
      if (player_slot != NULL && player_slot[0x1d] >= 0 &&
          player_slot[0x1d] < 4) {
        if (player_slot[0x1c] >= 0 && player_slot[0x1c] < 4) {
          if (player_slot[0x1c] == p[0x1c] && player_slot[0x1d] == p[0x1d]) {
            network_game_invalidate_player((uint8_t *)player_slot);
            *(short *)(g + 0x224) -= 1;
            return true;
          }
        }
      }
      player_slot += 0x20;
    }
    return false;
  }

  error(2, "tried to remove a player with indvalid data");
  return false;
}

bool network_game_create_game_objects(void *game)
{
  char *g = (char *)game;
  int i;
  char *player;
  game_options_t options;
  int conn;

  if (g == NULL) {
    display_assert(
      "game", "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x170, 1);
    system_exit(-1);
  }

  /* Initialize game options. 0x12b350 calls game_options_new (the
   * game_options_t constructor), not a plain memset — it seeds non-zero option
   * defaults. */
  game_options_new(&options);
  csstrncpy(options.map_name, g + 0x24, 0x7f);
  options.difficulty = *(int16_t *)(g + 0x110);

  conn = game_connection();
  if (conn > 0) {
    if (conn < 3) {
      options.random_seed = network_game_get_random_seed();
    } else {
      if (conn != 3) {
        goto bad_connection;
      }
      options.random_seed = *(uint32_t *)(g + 0x428);
    }

    game_precache_new_map(options.map_name, true);
    main_menu_unload();
    if (game_in_progress()) {
      game_dispose_from_old_map();
      game_unload();
    }

    if (*(int *)(g + 0xbc) != 0) {
      game_set_game_variant((game_variant_t *)(g + 0xa4));
    }

    if (!game_load(&options)) {
      error(0, "game_load() failed.");
      return (bool)g[0x430];
    }

    g[0x430] = 1;
    game_initialize_for_new_map();
    qsort(g + 0x226, 16, 32, (int (*)(const void *, const void *))sort_network_players);

    player = g + 0x226;
    for (i = 0; i < 16; i++, player += 0x20) {
      if (!network_player_is_valid(player)) {
        break;
      }
      if (!network_game_spawn_player(player)) {
        volatile char *valid_flag = (volatile char *)(g + 0x430);
        *valid_flag = 0;
        return (bool)*valid_flag;
      }
    }

    return (bool)g[0x430];
  }

bad_connection:
  display_assert("!\"bad game connection\"",
                 "c:\\halo\\SOURCE\\networking\\network_game_manager.c",
                 0x17f, true);
  system_exit(-1);
  return false;
}

bool network_game_remove_machine(void *game, void *machine)
{
  char *g = (char *)game;
  char *m = (char *)machine;
  char machine_index;
  int i;

  if (g == NULL || m == NULL) {
    display_assert("game && machine",
                   "c:\\halo\\SOURCE\\networking\\network_game_manager.c", 0x97,
                   1);
    system_exit(-1);
  }

  machine_index = m[0x40];
  if (machine_index >= 0 && machine_index < 4) {
    char *mach_ptr = g + 0x154;
    for (i = 0; i < 4; i++) {
      if (*mach_ptr == machine_index) {
        char *player = g + 0x226;
        int p;
        for (p = 0; p < 16; p++) {
          /* Check if player slot is valid and matches our machine (inlined
           * player_is_valid) */
          if (player != NULL && player[0x1d] >= 0 && player[0x1d] < 4 &&
              player[0x1c] >= 0 && player[0x1c] < 4 &&
              player[0x1c] == machine_index) {
            if (!network_game_remove_player(g, player)) {
              error(2, "failed to remove a machine's player");
            }
          }
          player += 0x20;
        }
        network_game_invalidate_machine(g, (short)machine_index);
        *(short *)(g + 0x112) -= 1;
        return true;
      }
      mach_ptr += 0x44;
    }
  }
  return false;
}

wchar_t *network_game_get_random_player_name(void)
{
  int iVar1;
  short *psVar2;
  int uVar3;
  wchar_t *puVar4;

  iVar1 = tag_loaded(0x75737472, "ui\\random_player_names");
  if (iVar1 != -1) {
    psVar2 = (short *)tag_get(0x75737472, iVar1);
    if (psVar2 != NULL) {
      uVar3 =
        seed_random_range(random_math_get_local_seed_address(), 0, *psVar2 - 1);
      puVar4 = (wchar_t *)FUN_0019d420(iVar1, uVar3);
      return puVar4;
    }
  }
  return (wchar_t *)0x26cdf0;
}

void network_event(const char *format, ...)
{
  va_list args;

  if (format == NULL) {
    display_assert("format", "c:\\halo\\SOURCE\\networking\\network_messages.c",
                   0x14b, 1);
    system_exit(-1);
  }

  va_start(args, format);
  crt_vsnprintf(error_string_buffer, 0xff, format, args);
  va_end(args);

  error(3, error_string_buffer);
}

bool encode_network_game_message(void *message_struct, char *encoded_message,
                                 int16_t *encoded_message_size, int16_t type,
                                 int one)
{
  if (message_struct == NULL || encoded_message == NULL ||
      encoded_message_size == NULL || !(*encoded_message_size > 0)) {
    display_assert("message_struct && encoded_message && encoded_message_size "
                   "&& (*encoded_message_size>0)",
                   "c:\\halo\\SOURCE\\networking\\network_messages.c", 0x161,
                   1);
    system_exit(-1);
  }

  return encode_packet_group(&s_network_game_messages_group, message_struct,
                             encoded_message, encoded_message_size, type,
                             (short)one);
}
