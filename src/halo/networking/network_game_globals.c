/* network_game_in_progress (0x12a000)
 *
 * Returns true when either global network game endpoint pointer is non-null.
 */
bool network_game_in_progress(void)
{
  if (*(int *)0x46e8c0 == 0 && *(int *)0x46e8bc == 0) {
    return false;
  }

  return true;
}
