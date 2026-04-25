/* Initializes a weapon after it becomes the active weapon for a unit.
 * Resets trigger/magazine state, sets the ready animation (state 9),
 * fires the initial effect from the weapon triggers tag block, and
 * stores the ready animation frame count into the weapon data. */
void weapon_activate(int weapon_handle)
{
  uint32_t *weapon_data =
    (uint32_t *)object_get_and_verify_type(weapon_handle, 4);
  int tag_data = (int)tag_get(0x77656170, weapon_data[0]);

  weapon_reset_state(weapon_handle);
  weapon_set_animation_state(weapon_handle, 1, 9);
  FUN_000de3b0(weapon_handle, 0xc);
  weapon_start_effect(*(int *)(tag_data + 0x348), 0, 0, weapon_handle);

  int16_t frame = weapon_get_animation_frame(weapon_handle, 0, 10, -1);
  *(int16_t *)((int)weapon_data + 0x1ea) = frame;
}
