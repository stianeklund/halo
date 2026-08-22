void FUN_000a54b0(void)
{
  int16_t local_player_index;
  char *player_data;
  int16_t palette_index;
  int particle_system_tag_index;
  void *scenario;
  char *palette_element;

  if (*(char *)0x32574c == '\0' || *(char *)0x2ef7ee == '\0')
    return;

  local_player_index = *(int16_t *)0x506548;
  if (local_player_index == -1)
    return;

  player_data = (char *)FUN_000a3e60(local_player_index);
  *(int16_t *)(player_data + 0x14) = *(int16_t *)0x506784;
  *(int *)(player_data + 0x10) = *(int *)0x506780;

  *(char *)(player_data + 0x1a) = (char)FUN_0018f3e0(
    player_data + 0x10, (void *)0x506550, (int16_t *)(player_data + 0x18));

  palette_index = *(int16_t *)(player_data + 0x18);
  particle_system_tag_index = -1;

  if (palette_index != -1) {
    scenario = scenario_get();
    palette_element = (char *)tag_block_get_element((char *)scenario + 0x1b4,
                                                    (int)palette_index, 0xf0);
    particle_system_tag_index = *(int *)(palette_element + 0x2c);
  }

  if (*(int *)player_data != particle_system_tag_index) {
    if (*(int *)player_data != -1) {
      FUN_000a4200(local_player_index);
    }
    if (particle_system_tag_index != -1) {
      FUN_000a40a0(local_player_index, particle_system_tag_index, 1.0f);
    }
  }

  if (*(int *)player_data != -1) {
    weather_particle_system_render(local_player_index);
  }
}

/* FUN_000a6030 (0xa6030)
 *
 * Locate the best candidate record inside the cone described by `cone_spec`,
 * starting from the structure cluster that contains `point`.
 *
 * Confirmed from the disassembly at 0xa6030:
 *   - param2 ([EBP+0xc], held in EBX) is the point: it is the sole argument to
 *     bsp3d_find_leaf_point (FUN_0018e720) at 0xa603f/0xa6053, and is
 *     forwarded unchanged to FUN_000a5f00 (0xa6097) and FUN_000a5830
 *     (0xa60ea) -- so FUN_000a5830's first parameter is that same point, not
 *     an object handle.
 *   - the FIRST stack argument to FUN_000a5f00 is EAX at 0xa6098, i.e. the
 *     cluster index loaded from the bsp leaf element at 0xa6072
 *     (MOV AX, word ptr [EAX+8]) and range-checked against -1 at 0xa6079.
 *     It is NOT param1.
 *   - param1 ([EBP+8]) is loaded into EDI at 0xa6082 and stays live across the
 *     CALL at 0xa6099: it is FUN_000a5f00's implicit @<edi> argument.  That
 *     callee reads four floats from it ([EDI+0]/[EDI+8] = angle,
 *     [EDI+4]/[EDI+0xc] = distance) and derives the cone length/sine/cosine it
 *     hands to structure_clusters_in_cone (0x198ad0).  EDI is reloaded with
 *     the return count at 0xa60a1, which is why the original's live range ends
 *     at the call.
 */
char FUN_000a6030(float *cone_spec, float *point, float *direction,
                  float *arg4, float *arg5, void *out_struct)
{
  void *scenario;
  void *leaf_element;
  int leaf_index;
  int16_t cluster_index;
  int16_t count;
  int16_t i;
  char local_buffer[0xe00];
  char *elem;

  if (FUN_0018e720((int)point) == -1)
    return 0;

  leaf_index = FUN_0018e720((int)point) & 0x7fffffff;
  scenario = scenario_get();
  leaf_element =
    tag_block_get_element((char *)scenario + 0xe0, leaf_index, 0x10);
  cluster_index = *(int16_t *)((char *)leaf_element + 8);

  if (cluster_index == -1)
    return 0;

  count = (int16_t)FUN_000a5f00(cone_spec, cluster_index, point, direction,
                                arg4, arg5, 0x40, local_buffer);

  if (count <= 0)
    return 0;

  qsort(local_buffer, (size_t)count, 0x38, (qsort_compar_proc)0x000a5700);

  for (i = 0; i < count; i++) {
    elem = local_buffer + (int)i * 0x38;
    if (FUN_000a5830(point, elem + 4, arg4, *(int *)elem)) {
      csmemcpy(out_struct, elem, 0x38);
      return 1;
    }
  }

  return 0;
}

void cheats_initialize(void)
{
  csmemset(cheats_globals, 0, sizeof(cheats_globals));
}

void cheats_dispose(void)
{
}

void cheats_dispose_from_old_map(void)
{
}

void cheats_update(void)
{
  int16_t player_index;
  void *gamepad;
  char *cheat;
  char *btn;
  int cnt;

  if (!cheat_controller)
    return;

  player_index = (int16_t)local_player_get_next(-1);
  while (player_index != -1) {
    gamepad = input_get_gamepad_state(player_index);
    if (gamepad != NULL && *(char *)((char *)gamepad + 0x1d)) {
      cheat = cheats_globals;
      btn = (char *)gamepad + 0x10;
      cnt = 0x10;
      do {
        if (*cheat != '\0' && *btn != '\0') {
          director_set_local_player_context(player_index);
          if (*btn == '\x01') {
            console_printf(0, cheat);
            if (!hs_console_evaluate(cheat))
              *cheat = '\0';
          }
        }
        btn++;
        cheat += 0xc8;
        cnt--;
      } while (cnt != 0);
    }
    player_index = (int16_t)local_player_get_next(player_index);
  }
}

void cheats_load_from_file(void)
{
  void *stream;
  int16_t slot;
  char *entry;

  stream = crt_fopen("d:\\cheats.txt", "r");
  if (stream == NULL)
    return;

  for (slot = 0; slot < 16; slot++) {
    entry = cheats_globals + (int)slot * 200;
    if (crt_fgets(entry, 199, stream) == NULL)
      break;
    csstrtok(entry, "\r\n\t;");
    if ((slot == 12 || slot == 13) && *entry != '\0') {
      /* Second textual use of the address expression: with only one use cl.exe
       * sinks the add into the scaled-index register (`add esi,0`); a second use
       * makes it CSE the value and emit the reference's `lea esi,(esi)`. */
      *(cheats_globals + (int)slot * 200) = '\0';
      error(2, "Cannot execute cheats attached to the back or start button");
    }
  }
  crt_fclose(stream);
}

/* cheat_active_camouflage_local_player — give weapon infinite ammo cheat for
 * one local player. Finds the player's primary weapon, sets its vitality
 * to 1.0f (full), and sets bit 4 (+optionally bit 5) in the weapon's flags at
 * +0x1b4. Source: cheats.c, local_player_index in [0,3].
 */
void cheat_active_camouflage_local_player(int local_player_index)
{
  int player_handle;
  char *player;
  char *weapon;
  unsigned int flags;

  if ((short)local_player_index < 0 || (short)local_player_index >= 4)
    return;
  player_handle = local_player_get_player_index((int16_t)local_player_index);
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  weapon = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  *(unsigned int *)(weapon + 0x32c) = 0x3f800000;
  flags = *(unsigned int *)(weapon + 0x1b4);
  if (flags & 0x10)
    *(unsigned int *)(weapon + 0x1b4) = flags | 0x20;
  *(unsigned int *)(weapon + 0x1b4) |= 0x10;
}

/* FUN_000a67c0 — find the first player datum that has a weapon equipped.
 * Returns the datum handle of the player, or -1 if none found.
 * The datum handle is stored in the iterator at offset 8 (data_iter_t.datum).
 */
int FUN_000a67c0(void)
{
  data_iter_t iter;
  char *player;

  data_iterator_new(&iter, player_data);
  player = (char *)data_iterator_next(&iter);
  while (player != NULL) {
    if (*(int *)(player + 0x34) != -1)
      return *(int *)((char *)&iter + 8);
    player = (char *)data_iterator_next(&iter);
  }
  return -1;
}

void cheats_initialize_for_new_map(void)
{
  cheats_load_from_file();
}

/* cheat_teleport_to_camera — teleport cheat: move a player's vehicle/weapon to
 * the camera. Finds a player with a weapon (via FUN_000a67c0), then teleports
 * the vehicle (or weapon if not in a vehicle) to the camera position using
 * object_set_position. Frameless in the original binary.
 */
typedef void (*terminal_output_2_t)(void *, const char *);

void cheat_teleport_to_camera(void)
{
  int player_handle;
  char *player;
  short local_player_idx;
  char *camera;
  char *weapon_obj;
  int object_handle;

  player_handle = FUN_000a67c0();
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  local_player_idx = *(short *)(player + 2);
  if (local_player_idx == (short)-1)
    return;
  camera = (char *)observer_get_camera((unsigned short)local_player_idx);
  if (!camera) {
    display_assert("result", "c:\\halo\\SOURCE\\game\\cheats.c", 0x100, 1);
    system_exit(-1);
  }
  if (*(short *)(camera + 0x10) != (short)-1) {
    weapon_obj = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
    object_handle = *(int *)(weapon_obj + 0xcc);
    if (object_handle == -1)
      object_handle = *(int *)(player + 0x34);
    object_set_position(object_handle, (float *)camera, NULL, NULL);
    return;
  }
  ((terminal_output_2_t)terminal_output)(
    *(void **)0x2ee6f0,
    "Camera is outside BSP... cannot initiate teleportation...");
}

/* cheat_all_powerups — give weapon infinite ammo for the first armed player.
 * Same weapon modification logic as cheat_active_camouflage_local_player but
 * targets the first player that has a weapon equipped (via FUN_000a67c0) rather
 * than a specific local index. Frameless in the original binary.
 */
void cheat_all_powerups(void)
{
  int player_handle;
  char *player;
  char *weapon;
  unsigned int flags;

  player_handle = FUN_000a67c0();
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  weapon = (char *)object_get_and_verify_type(*(int *)(player + 0x34), 3);
  *(unsigned int *)(weapon + 0x32c) = 0x3f800000;
  flags = *(unsigned int *)(weapon + 0x1b4);
  if (flags & 0x10)
    *(unsigned int *)(weapon + 0x1b4) = flags | 0x20;
  *(unsigned int *)(weapon + 0x1b4) |= 0x10;
}
