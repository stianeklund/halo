/* 0x1249b0 — network_game_server_dispose.
 * Tears down the network game client connection. If the server pointer is
 * non-null, closes its connection handle and clears the in-use flag. */
void network_game_server_dispose(void *server)
{
  if (server != NULL) {
    if (*(int *)((char *)server + 0x82c) != 0)
      FUN_00128d30(*(int *)((char *)server + 0x82c));
    if (*(char *)0x46e8b9 == '\0') {
      display_assert("network_game_client_dont_use_directly_in_use",
                     "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                     0xb2, 1);
      system_exit(-1);
    }
    *(char *)0x46e8b9 = '\0';
  }
  network_game_log("network client disposed");
}

/* FUN_00124c40 (0x124c40)
 *
 * Asserts client is non-null and returns the client's 16-bit value at +0.
 */
uint16_t FUN_00124c40(void *client)
{
  uint16_t *client_words;

  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x1fd, true);
    system_exit(-1);
  }

  client_words = (uint16_t *)client;
  return client_words[0];
}

/* FUN_001257a0 (0x1257a0)
 *
 * Asserts client is non-null and returns client + 0x85c.
 */
void *FUN_001257a0(void *client)
{
  if (client == NULL) {
    display_assert("client",
                   "c:\\halo\\SOURCE\\networking\\network_client_manager.c",
                   0x4cd, true);
    system_exit(-1);
  }

  return (void *)((uint8_t *)client + 0x85c);
}

/* 0x1257e0 — Asserts client is non-null and returns whether the int field at
 * offset 0xc98 is non-zero. */
bool FUN_001257e0(void *server)
{
  assert_halt(server);
  return *(int *)((char *)server + 0xc98) != 0;
}

/* 0x125820 — Asserts client is non-null and returns the uint32_t field at
 * offset 0xc98 (the raw value that FUN_001257e0 tests for non-zero). */
uint32_t FUN_00125820(void *server)
{
  assert_halt(server);
  return *(uint32_t *)((char *)server + 0xc98);
}

/* 0x125860 — Asserts client is non-null and returns the byte field at
 * offset 0xcac. */
bool FUN_00125860(void *server)
{
  assert_halt(server);
  return *(char *)((char *)server + 0xcac);
}
