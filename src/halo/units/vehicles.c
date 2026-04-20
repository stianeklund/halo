/*
 * vehicle_moving_near_any_player (0x1b7ee0)
 *
 * Scans all local players. For each local player whose unit is on foot (not
 * currently inside a vehicle, checked via object_data+0xcc == NONE), collects
 * up to MAXIMUM_NUMBER_OF_LOCAL_PLAYERS unit handles and their bounding-sphere
 * centres. Then iterates every vehicle object in the world. For each vehicle,
 * checks each on-foot player:
 *   1. The player's unit is not already sitting inside this vehicle
 *      (object_data[unit]+0xcc != vehicle_datum_handle).
 *   2. Squared distance between vehicle position (+0x50) and the cached
 *      player bounding-sphere centre is < 100.0 (within 10 world units).
 *   3. Squared velocity magnitude (+0x18) is >= ~1/900 (speed >= ~1/30 u/tick).
 * Returns true if any qualifying vehicle is found.
 *
 * Called from game_safe_to_save (0xa7530) to block saving while a moving
 * vehicle is close to a player on foot.
 */

#include "../../common.h"

/* Squared proximity threshold: 10.0^2 = 100.0 world units. */
#define VEHICLE_NEAR_PLAYER_DIST_SQ 100.0f
/* Squared velocity threshold: (1/30)^2 = 1/900. Vehicle must exceed this to
 * be considered "moving". Value from binary at 0x25620c. */
#define VEHICLE_MIN_SPEED_SQ 0.001111111138f

bool vehicle_moving_near_any_player(void)
{
  /* Object iterator buffer: 0x10-byte struct identical in layout to
   * data_iter_t. object_iterator_next writes the current datum handle at
   * byte offset 0x08 (iter_buf[2] as an int array). */
  int iter_buf[4];
  int unit_handles[4]; /* handles of on-foot player units */
  float player_pos[12]; /* 3-float bounding-sphere centre per player */
  float radius_scratch; /* radius out-param, not used here */
  int16_t lpi; /* current local_player_index */
  int16_t n; /* count of on-foot player units collected */
  int16_t i;
  int player_handle;
  char *player;
  int unit_handle;
  void *unit_obj;
  void *veh_obj;
  int vehicle_datum_handle;
  float dx, dy, dz;
  float vx, vy, vz;
  char found; /* 1 = no vehicle found yet, 0 = found; matches local_5 */

  n = 0;
  found = 1;

  /* Phase 1: collect on-foot local player units and their positions.
   * local_player_get_next(-1) returns the first valid local_player_index. */
  lpi = ((int16_t(*)(int16_t))0xba4c0)((int16_t)-1);
  if (lpi == (int16_t)-1)
    goto done;

  /* Push ESI before inner loop (matches disasm 001b7f07: PUSH ESI). */
  do {
    /* First call: validate player index. */
    player_handle = ((int (*)(int16_t))0xba3c0)(lpi);
    if (player_handle != -1) {
      /* Second call: get handle for datum_get (matches disasm 001b7f16). */
      player_handle = ((int (*)(int16_t))0xba3c0)(lpi);
      player = (char *)((void *(*)(void *, int))0x119320)(*(void **)0x5aa6d4,
                                                          player_handle);
      unit_handle = *(int *)(player + 0x34);
      if (unit_handle != -1) {
        /* object_get_and_verify_type(handle, 3): accepts biped or vehicle. */
        unit_obj = ((void *(*)(int, int))0x13d680)(unit_handle, 3);
        /* +0xcc = parent_object_index; NONE (-1) means unit is on foot. */
        if (*(int *)((char *)unit_obj + 0xcc) == -1) {
          unit_handles[n] = unit_handle;
          /* object_get_bounding_sphere: writes centre to &player_pos[n*3],
           * radius to &radius_scratch. Centre is at object_data+0x50. */
          ((void (*)(int, float *, float *))0x1aae0)(
            unit_handle, &player_pos[n * 3], &radius_scratch);
          n++;
        }
      }
    }
    lpi = ((int16_t(*)(int16_t))0xba4c0)(lpi);
  } while (lpi != (int16_t)-1);

  if (n == 0)
    goto done;

  /* Phase 2: iterate all vehicle objects (type_mask=2 = bit 1 = vehicle). */
  object_iterator_new(iter_buf, 2, 0);

  while ((veh_obj = object_iterator_next(iter_buf)) != NULL) {
    /* iter_buf[2] holds the datum handle of the current vehicle object,
     * written by object_iterator_next at offset 0x08 in the iter buffer. */
    vehicle_datum_handle = iter_buf[2];

    i = 0;
    if (n <= 0)
      continue;

    do {
      unit_obj = ((void *(*)(int, int))0x13d680)(unit_handles[i], 3);
      /* Skip player whose unit is already inside this vehicle. */
      if (*(int *)((char *)unit_obj + 0xcc) == vehicle_datum_handle) {
        i++;
        continue;
      }

      /* Squared distance: vehicle pos (+0x50) vs player bounding centre. */
      dx = *(float *)((char *)veh_obj + 0x50) - player_pos[i * 3];
      dy = *(float *)((char *)veh_obj + 0x54) - player_pos[i * 3 + 1];
      dz = *(float *)((char *)veh_obj + 0x58) - player_pos[i * 3 + 2];
      if (dx * dx + dy * dy + dz * dz >= VEHICLE_NEAR_PLAYER_DIST_SQ) {
        i++;
        continue;
      }

      /* Squared velocity: vehicle velocity (+0x18) must exceed threshold. */
      vx = *(float *)((char *)veh_obj + 0x18);
      vy = *(float *)((char *)veh_obj + 0x1c);
      vz = *(float *)((char *)veh_obj + 0x20);
      if (vx * vx + vy * vy + vz * vz >= VEHICLE_MIN_SPEED_SQ) {
        found = 0;
        goto done;
      }
      i++;
    } while (i < n);
  }

done:
  return found == 0;
}
