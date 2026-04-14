/* Xbox network transport layer — Winsock/XNet wrapper. */

/* NOTE: transport_initialize (0x82130) is complex Xbox networking init
 * with XNet, WSA, and ethernet link detection. Left to original. */

/* Shut down the network transport layer. */
void transport_dispose(void)
{
  if (*(uint8_t *)0x335090 != 0) {
    ((void (*)(void))0x2232f5)();
    ((void (*)(void))0x2232ed)();
    *(uint8_t *)0x335090 = 0;
  }
}
