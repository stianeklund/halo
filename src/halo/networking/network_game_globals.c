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

/* network_game_server_get (0x12a1d0)
 *
 * Returns the global network game server pointer.
 */
void *network_game_server_get(void)
{
  return *(void **)0x46e8bc;
}

/* network_game_client_get (0x12a240)
 *
 * Returns the global network game client pointer.
 */
void *network_game_client_get(void)
{
  return *(void **)0x46e8c0;
}
