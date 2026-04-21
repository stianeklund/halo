/* Xbox network transport layer — Winsock/XNet wrapper. */

/* Initialize the Xbox network transport layer.
 *
 * Queries ethernet link status, optionally enables XNet security bypass
 * (if d:\bypass_security.txt exists), starts XNet and WinSock, then
 * polls for ethernet link with a 10-second timeout. On success, sets
 * the transport_initialized flag. On any failure, cleans up and returns.
 *
 * Confirmed: XNetGetEthernetLinkStatus (0x1d8b76) returns link flags;
 * XNetStartup (0x2231f8, stdcall 1 arg) and WSAStartup (0x223206,
 * stdcall 2 args); XNetCleanup (0x2232f5) and WSACleanup (0x2232ed);
 * XNetGetEthernetLinkStatus poll wrapper (0x222ecf, stdcall 1 arg);
 * setsockopt-like (0x222e0e, stdcall 2 args); fopen (0x1d9e59, cdecl);
 * fclose (0x1d9dac, cdecl); winsock_error_report (0x83310, cdecl).
 */
void transport_initialize(void)
{
  uint8_t xnet_params[11];
  uint8_t wsadata[0x190];
  uint32_t link_status;
  const char *connected_str;
  const char *speed100_str;
  const char *speed10_str;
  const char *fullduplex_str;
  const char *halfduplex_str;
  void *file;
  int xnet_result;
  int16_t wsa_result;
  unsigned int start_time;
  unsigned int deadline;
  int link_result;

  /* Early out if already initialized. */
  if (*(uint8_t *)0x335090 != 0)
    return;

  /* Zero the WSADATA buffer. */
  csmemset(wsadata, 0, sizeof(wsadata));

  /* Build XNetStartupParams structure (11 bytes). */
  xnet_params[0] = 0x0B; /* cfgSizeOfStruct */
  xnet_params[1] = 0x00; /* cfgFlags */
  xnet_params[2] = 0x18; /* cfgSockMaxDgramSockets */
  xnet_params[3] = 0x08; /* cfgSockMaxStreamSockets */
  xnet_params[4] = 0x04; /* cfgSockDefaultRecvBufsizeInK */
  xnet_params[5] = 0x08; /* cfgSockDefaultSendBufsizeInK */
  xnet_params[6] = 0x80; /* cfgKeyRegMax */
  xnet_params[7] = 0x00; /* cfgSecRegMax */
  xnet_params[8] = 0x00; /* cfgQosDataLimitDiv4 */
  xnet_params[9] = 0x01; /* cfgQosProbeMinWait */
  xnet_params[10] = 0x20; /* cfgQosProbeMaxWait */

  /* Query ethernet link status and log it. */
  link_status = ((uint32_t(*)(void))0x1d8b76)();

  halfduplex_str = (link_status & 0x10) ? " in half-duplex mode" : "";
  fullduplex_str = (link_status & 0x08) ? " in full-duplex mode" : "";
  speed10_str = (link_status & 0x04) ? " at 10 Mbps" : "";
  speed100_str = (link_status & 0x02) ? " at 100 Mbps" : "";
  connected_str = (link_status & 0x01) ? "connected" : "not connected";

  error(3, "xbox ethernet link is %s%s%s%s%s", connected_str, speed100_str,
        speed10_str, fullduplex_str, halfduplex_str);

  /* Re-init cfgSizeOfStruct and cfgFlags before checking bypass. */
  xnet_params[0] = 0x0B;
  xnet_params[1] = 0x00;

  /* Check for security bypass file. */
  file = ((void *(*)(const char *, const char *))0x1d9e59)(
    "d:\\bypass_security.txt", "r");
  if (file != 0) {
    error(2, "XNET_STARTUP_BYPASS_SECURITY [ON]");
    xnet_params[1] |= 0x01;
    ((void (*)(void *))0x1d9dac)(file);
  }

  /* Start XNet. */
  xnet_result = ((int(__stdcall *)(uint8_t *))0x2231f8)(xnet_params);
  if (xnet_result != 0)
    return;

  /* Start WinSock 2.2. */
  wsa_result = ((int16_t(__stdcall *)(int16_t, uint8_t *))0x223206)(2, wsadata);
  if (wsa_result != 0) {
    /* Cleanup: WSACleanup then report error. */
    ((void (*)(void))0x2232ed)();
    ((void (*)(int))0x83310)((int)wsa_result);
    return;
  }

  /* Poll for ethernet link with 10-second timeout. */
  start_time = system_milliseconds();
  deadline = start_time + 10000;

  for (;;) {
    link_result = ((int(__stdcall *)(void *))0x222ecf)((void *)0x5ab230);
    if (system_milliseconds() > deadline)
      break;
    if (link_result == 0)
      continue;
    if (link_result != 1) {
      /* Link detected — configure socket options and mark initialized. */
      ((int(__stdcall *)(void *, int))0x222e0e)((void *)0x5ab228, 8);
      *(uint8_t *)0x335090 = 1;
      return;
    }
    break;
  }

  /* Timeout or error — shut down XNet and WSACleanup. */
  ((void (*)(void))0x2232f5)();
  ((void (*)(void))0x2232ed)();
}

/* Shut down the network transport layer. */
void transport_dispose(void)
{
  if (*(uint8_t *)0x335090 != 0) {
    ((void (*)(void))0x2232f5)();
    ((void (*)(void))0x2232ed)();
    *(uint8_t *)0x335090 = 0;
  }
}

/* Clean up the endpoint pool. Iterates 64 entries (8 bytes each) at
 * 0x3350a0. For each entry with a non-zero thread handle and cleanup
 * flag set, closes the thread and clears the entry. */
void endpoint_pool_cleanup(void)
{
  int *entry = (int *)0x3350a0;

  do {
    if (entry[0] != 0 && *(char *)(entry + 1) != 0) {
      thread_close((void *)entry[0]);
      entry[0] = 0;
      *(char *)(entry + 1) = 0;
    }
    entry += 2;
  } while ((int)entry < 0x3352a0);
}

/* Send data over a transport endpoint.
 *
 * Calls xnet_send (0x225c20) with the socket handle stored at ep[0].
 * On success returns the byte count from send().
 * On failure, classifies the Winsock error via xapi_GetLastError (0x2235c4):
 *   WSAECONNRESET (0x2733)                      -> ep status = -4, return -4
 *   WSAECONNABORTED/disconnect-family            -> ep status = -3, return -3
 *     (0x2744/0x2745/0x2746/0x2749/0x274a/0x274c,
 *      also clears connected bit (bit 0) of ep flags byte at offset 4)
 *   Any other error                              -> ep status = -2, return -2
 *
 * ep struct layout (from disassembly):
 *   [ep+0]  int      socket fd
 *   [ep+4]  uint8_t  flags (bit 0 = connected)
 *   [ep+6]  int16_t  status/error code
 *
 * Confirmed: xnet_send (0x225c20, __stdcall 4 args, RET 0x10);
 * xapi_GetLastError (0x2235c4 thunk -> 0x1d2240);
 * switch jump table at 0x83010; byte redirect table at 0x8301c;
 * assert strings at 0x26665c, 0x265fe4; source line 0x350/0x351.
 */
int send_endpoint(int *ep, const char *buf, int len)
{
  int result;
  int error_code;

  assert_halt(ep && buf && (len > 0));
  assert_halt(*(uint8_t *)0x335090);

  result = xnet_send(ep[0], buf, len, 0);
  if (result != -1)
    return result;

  error_code = xapi_GetLastError();
  switch (error_code) {
  case 0x2733:
    /* WSAECONNRESET — connection reset by peer. */
    *(int16_t *)((char *)ep + 6) = -4;
    return -4;
  case 0x2744:
  case 0x2745:
  case 0x2746:
  case 0x2749:
  case 0x274a:
  case 0x274c:
    /* Various disconnect/abort errors — mark endpoint not connected. */
    *(uint8_t *)((char *)ep + 4) &= 0xfe;
    *(int16_t *)((char *)ep + 6) = -3;
    return -3;
  default:
    /* Unknown Winsock error. */
    *(int16_t *)((char *)ep + 6) = -2;
    return -2;
  }
}

/* Close a transport endpoint's socket and clear its connected flag.
 *
 * If the endpoint's socket handle is not INVALID_SOCKET (-1), calls
 * xnet_closesocket to close it. On failure, reports the Winsock error
 * via winsock_error_report. Then sets the socket handle to -1.
 * Always clears bit 0 (connected) of the flags byte at ep+4.
 *
 * ep struct layout (from disassembly):
 *   [ep+0]  int      socket fd (-1 = invalid)
 *   [ep+4]  uint8_t  flags (bit 0 = connected)
 *
 * Confirmed: xnet_closesocket (0x225cc6, __stdcall 1 arg, RET 4);
 * xapi_GetLastError (0x2235c4 thunk -> 0x1d2240);
 * winsock_error_report (0x83310, cdecl 1 arg);
 * assert strings at 0x266658, 0x265fe4; source lines 0x221/0x222.
 */
void close_endpoint(int *ep)
{
  int result;
  int err;

  assert_halt(ep != NULL);
  assert_halt(*(uint8_t *)0x335090);

  if (*ep != -1) {
    result = xnet_closesocket(*ep);
    if (result != 0) {
      err = xapi_GetLastError();
      winsock_error_report(err);
    }
    *ep = -1;
  }
  *(uint8_t *)((char *)ep + 4) &= 0xfe;
}

/* Destroy a transport endpoint: close its socket, free memory, cleanup pool.
 *
 * Calls close_endpoint (0x84000) to close the underlying socket and clear
 * the socket handle.  Then frees the endpoint allocation via debug_free
 * (0x8ef70) with original XBE source path and line.  Finally tail-calls
 * endpoint_pool_cleanup (0x82d30) to remove the entry from the active table.
 *
 * Confirmed: close_endpoint (0x84000, cdecl 1 arg: int *ep);
 * debug_free (0x8ef70, 3 args); endpoint_pool_cleanup (0x82d30, 0 args);
 * assert strings at 0x266658, 0x265fe4; source line 0xe4/0xe5/0xe8.
 */
void destroy_endpoint(int *ep)
{
  assert_halt(ep != NULL);
  assert_halt(*(uint8_t *)0x335090);

  /* Close the underlying socket and clear handle/flags. */
  close_endpoint(ep);

  /* Free the endpoint allocation using original XBE source path and line. */
  debug_free(
    ep, "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0xe8);

  /* Remove from active endpoint pool. */
  endpoint_pool_cleanup();
}
