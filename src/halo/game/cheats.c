#include "../../x87_math.h"

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
       * sinks the add into the scaled-index register (`add esi,0`); a second
       * use makes it CSE the value and emit the reference's `lea esi,(esi)`. */
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
__declspec(noinline) int FUN_000a67c0(void)
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
  ((terminal_output_2_t)terminal_printf)(
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

/* FUN_000a6930 -- spawn one object per non-NONE tag index in a 0x10-byte
 * stride record array, arranged on a circle around the first armed player.
 *
 * records (param_1) -- base of the record array; each 0x10-byte element holds
 *                      a tag index at +0xC (NONE (-1) skips that slot).
 * count   (param_2) -- element count, re-read as a signed 16-bit word from the
 *                      stack slot on every iteration (MOV AX,[EBP+0xC]).
 *
 * Bails out when no player has a weapon equipped (FUN_000a67c0 == -1).
 * Otherwise takes that player's object (player+0x34) world position and
 * orientation, then for each populated slot places an object at
 *   spacing = min(*(float *)0x255a54 / count, *(float *)0x26b164)
 *   angle   = (i - count/2) * spacing + atan2(forward.x, forward.y)
 *   x = cos(angle) * *(float *)0x2533ec + position.x
 *   y = sin(angle) * *(float *)0x2533ec + position.y
 *   z = position.z + *(float *)0x2533f0
 * with the player's forward/up copied into the placement descriptor.
 * The constants keep their raw addresses because their values are not proven
 * here beyond 0x255a54 = 6.2831855f (2*pi, named in structures.c).
 *
 * Call-site verification (raw disassembly at 0xa6930):
 *   0xa694f datum_get: PUSH EAX(*(data_t **)0x5aa6d4), PUSH EAX(handle)
 *   0xa695c object_get_and_verify_type: PUSH 3, PUSH [ESI+0x34]; result unused
 *   0xa6969 object_get_world_position: PUSH EDX(&position), PUSH [ESI+0x34]
 *   0xa697a object_get_orientation: PUSH ECX(&up), PUSH EDX(&forward),
 *           PUSH EAX([ESI+0x34]).  ADD ESP,0x24 merges the cleanup of all
 *           four calls, so the ARG_COUNT=9 hazard on this site is that merge
 *           and not a 9-argument call.
 *   0xa69fb object_placement_data_new: PUSH -1, PUSH ECX(*record),
 *           PUSH EAX(&placement)
 *   0xa6a62 object_new: PUSH ECX(&placement); ADD ESP,0x10 covers both.
 * FPATAN operand order: FLD [EBP-0x10] (forward.x) then FLD [EBP-0x0c]
 * (forward.y), so ST(1) = forward.x is the atan2 numerator.
 * The placement buffer is 0x88 bytes ([EBP-0xB0 .. EBP-0x29]), matching the
 * 0x88 bytes object_placement_data_new writes.
 */
void FUN_000a6930(int records, unsigned short count)
{
  object_placement_data placement;
  vector3_t position;
  vector3_t forward;
  vector3_t up;
  int player_handle;
  char *player;
  int i;
  int *record;
  unsigned int remaining;
  float spacing;
  float angle;

  player_handle = FUN_000a67c0();
  if (player_handle == -1)
    return;
  player = (char *)datum_get(player_data, player_handle);
  object_get_and_verify_type(*(int *)(player + 0x34), 3);
  object_get_world_position(*(int *)(player + 0x34), &position);
  object_get_orientation(*(int *)(player + 0x34), (float *)&forward,
                         (float *)&up);
  if ((short)count <= 0)
    return;
  i = 0;
  record = (int *)((char *)records + 0xc);
  remaining = count;
  do {
    if (*record != -1) {
      spacing = *(float *)0x00255a54 / (float)(short)count;
      if (spacing > *(float *)0x0026b164)
        spacing = *(float *)0x0026b164;
      angle = (float)(i - (short)count / 2) * spacing +
              (float)atan2((double)forward.x, (double)forward.y);
      object_placement_data_new(&placement, *record, -1);
      placement.forward = forward;
      placement.up = up;
      placement.position_x =
        x87_fcos(angle) * *(float *)0x002533ec + position.x;
      placement.position_y =
        x87_fsin(angle) * *(float *)0x002533ec + position.y;
      placement.position_z = position.z + *(float *)0x002533f0;
      object_new(&placement);
    }
    i++;
    record += 4;
    remaining--;
  } while (remaining != 0);
}
