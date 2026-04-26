/* Assert server is non-null and return the connection pointer at offset +8
 * (0x12d570). */
int FUN_0012d570(void *server)
{
  if (!server) {
    display_assert("server",
                   "c:\\halo\\SOURCE\\networking\\network_server_manager.c",
                   0x769, 1);
    system_exit(-1);
  }
  return (int)((char *)server + 8);
}
