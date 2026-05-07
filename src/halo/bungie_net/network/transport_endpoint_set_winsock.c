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
  link_status = XNetGetEthernetLinkStatus();

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
    winsock_error_report((int)wsa_result);
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

/* Check whether the Xbox ethernet link is currently connected.
 *
 * Calls XNetGetEthernetLinkStatus (0x1d8b76) and returns bit 0,
 * which is the "connected" flag.
 *
 * Confirmed: 3-instruction function — CALL, AND AL,1, RET.
 * Callers include network session management functions.
 */
bool FUN_00082300(void)
{
  return XNetGetEthernetLinkStatus() & 1;
}

/* Remove an endpoint from an endpoint set.
 * Searches the set's endpoint_array for the matching pointer, then finds and
 * removes the endpoint's socket from the fd_array by shifting. Clears the
 * "in set" flag (bit 3) on the endpoint, nulls the array slot, and marks
 * the set dirty. Returns 0 on success, -19 if the endpoint is not found. */
short FUN_00082850(int *endpoint, uint32_t *endpoint_set)
{
  int i = 0;
  uint32_t **ep_array;
  uint32_t j;
  uint32_t *fds;

  assert_halt(endpoint && endpoint_set);
  assert_halt(*(uint8_t *)0x335090);

  if ((int)endpoint_set[0x43] >= 0) {
    ep_array = (uint32_t **)endpoint_set[0x41];
    do {
      if ((int *)ep_array[i] == endpoint) {
        j = 0;
        if (endpoint_set[0] == 0)
          goto clear_entry;
        fds = endpoint_set + 1;
        while (1) {
          if (*fds == (uint32_t)*endpoint) {
            if (j < endpoint_set[0] - 1) {
              uint32_t *p = endpoint_set + j + 1;
              do {
                *p = p[1];
                j++;
                p++;
              } while (j < endpoint_set[0] - 1);
            }
            endpoint_set[0]--;
            goto clear_entry;
          }
          j++;
          fds++;
          if (j >= endpoint_set[0])
            goto clear_entry;
        }
      }
      i++;
    } while (i <= (int)endpoint_set[0x43]);
  }
  return -19;

clear_entry:
  *(uint8_t *)((char *)endpoint + 4) &= 0xf7;
  *(uint32_t *)(endpoint_set[0x41] + i * 4) = 0;
  endpoint_set[0x45] = 1;
  return 0;
}

/* Release the global XNet key and clear associated state.
 *
 * If the "key owned" flag (0x335091) is set, decrements global_key_depth
 * (0x335094) and, if it reaches zero, calls FUN_00222df7 to release the
 * key object at 0x5ab220.  Clears the owned flag.  Then always performs a
 * second decrement-and-release of global_key_depth.  Finally clears the
 * byte at 0x5ab204 via csmemset.
 *
 * Confirmed: display_assert (0x8d9f0); system_exit (0x8e2f0);
 * FUN_00222df7 (0x222df7, __stdcall 1 arg, RET 4);
 * csmemset (0x8db80, cdecl 3 args);
 * assert string "global_key_depth > 0" at 0x2664a8;
 * __FILE__ string at 0x266458; source line 0x66 = 102.
 */
void FUN_00082b30(void)
{
  if (*(uint8_t *)0x335091 != 0) {
    assert_halt(*(int *)0x335094 > 0);
    *(int *)0x335094 -= 1;
    if (*(int *)0x335094 == 0) {
      FUN_00222df7((void *)0x5ab220);
    }
    *(uint8_t *)0x335091 = 0;
  }

  assert_halt(*(int *)0x335094 > 0);
  *(int *)0x335094 -= 1;
  if (*(int *)0x335094 == 0) {
    FUN_00222df7((void *)0x5ab220);
  }

  csmemset((void *)0x5ab204, 0, 1);
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

/* Receive data from a transport endpoint.
 *
 * Calls xnet_recv (0x225bb6) with the socket handle stored at ep[0].
 * On success returns the byte count from recv(); if recv returns 0
 * (graceful close), returns -3 instead.
 * On failure, classifies the Winsock error via xapi_GetLastError (0x2235c4):
 *   WSAECONNRESET (0x2733)                      -> ep status = -4, return -4
 *   WSAECONNABORTED/disconnect-family            -> ep status = -3, return -3
 *     (0x2744/0x2745/0x2746/0x2749/0x274a/0x274c,
 *      also clears bits 0 and 2 of ep flags byte at offset 4)
 *   Any other error                              -> ep status = -2, return -2
 *     (clears bit 2 only of ep flags byte at offset 4)
 *
 * ep struct layout (from disassembly):
 *   [ep+0]  int      socket fd
 *   [ep+4]  uint8_t  flags (bit 0 = connected, bit 2 = ?)
 *   [ep+6]  int16_t  status/error code
 *
 * Confirmed: xnet_recv (0x225bb6, __stdcall 4 args);
 * xapi_GetLastError (0x2235c4); assert strings at 0x26665c, 0x265fe4;
 * switch jump table at 0x82f28; byte redirect table at 0x82f34;
 * source lines 0x322/0x323.
 */
int recv_endpoint(int *ep, void *buffer, int maxlen)
{
  int result;
  int error_code;

  assert_halt(ep && buffer && (maxlen > 0));
  assert_halt(*(uint8_t *)0x335090);

  result = xnet_recv(ep[0], buffer, maxlen, 0);
  if (result == -1) {
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
      /* Disconnect-family errors — clear connected and another flag bit. */
      *(uint8_t *)((char *)ep + 4) &= 0xfa;
      *(int16_t *)((char *)ep + 6) = -3;
      return -3;
    default:
      /* Unknown Winsock error — clear flag bit 2 only. */
      *(uint8_t *)((char *)ep + 4) &= 0xfb;
      *(int16_t *)((char *)ep + 6) = -2;
      return -2;
    }
  }
  if (result == 0)
    result = -3;
  return result;
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

/* Map a WinSock error code to its symbolic name string and report it.
 *
 * Translates the given WinSock/WSA error code into a human-readable
 * constant name (e.g. "WSAECONNRESET"). Stores the result string in a
 * global at 0x335098. If the error code differs from the last reported
 * one (tracked at 0x3352a0), logs it via error(3, ...). Returns the
 * error name string.
 *
 * Confirmed: error (0x8f390, cdecl, variadic);
 * format string "winsock error #%d: %s" at 0x2666a4;
 * global string pointer at 0x335098; last error code at 0x3352a0.
 */
const char *winsock_error_report(int error_code)
{
  const char *name;

  switch (error_code) {
  case -1:
    name = "WSA_WAIT_FAILED";
    break;
  case 0:
    name = "WSA_INVALID_EVENT";
    break;
  case 6:
    name = "WSA_INVALID_HANDLE";
    break;
  case 8:
    name = "WSA_NOT_ENOUGH_MEMORY";
    break;
  case 0x40:
    name = "WSA_MAXIMUM_WAIT_EVENTS";
    break;
  case 0x57:
    name = "WSA_INVALID_PARAMETER";
    break;
  case 0xC0:
    name = "WSA_WAIT_IO_COMPLETION";
    break;
  case 0x102:
    name = "WSA_WAIT_TIMEOUT";
    break;
  case 0x3E3:
    name = "WSA_OPERATION_ABORTED";
    break;
  case 0x3E4:
    name = "WSA_IO_INCOMPLETE";
    break;
  case 0x3E5:
    name = "WSA_IO_PENDING";
    break;

  case 0x2714:
    name = "WSAEINTR";
    break;
  case 0x2719:
    name = "WSAEBADF";
    break;
  case 0x271D:
    name = "WSAEACCES";
    break;
  case 0x271E:
    name = "WSAEFAULT";
    break;
  case 0x2726:
    name = "WSAEINVAL";
    break;
  case 0x2728:
    name = "WSAEMFILE";
    break;
  case 0x2733:
    name = "WSAEWOULDBLOCK";
    break;
  case 0x2734:
    name = "WSAEINPROGRESS";
    break;
  case 0x2735:
    name = "WSAEALREADY";
    break;
  case 0x2736:
    name = "WSAENOTSOCK";
    break;

  case 0x2737:
    name = "WSAEDESTADDRREQ";
    break;

  case 0x2738:
    name = "WSAEMSGSIZE";
    break;
  case 0x2739:
    name = "WSAEPROTOTYPE";
    break;
  case 0x273A:
    name = "WSAENOPROTOOPT";
    break;
  case 0x273B:
    name = "WSAEPROTONOSUPPORT";
    break;
  case 0x273C:
    name = "WSAESOCKTNOSUPPORT";
    break;
  case 0x273D:
    name = "WSAEOPNOTSUPP";
    break;
  case 0x273E:
    name = "WSAEPFNOSUPPORT";
    break;
  case 0x273F:
    name = "WSAEAFNOSUPPORT";
    break;
  case 0x2740:
    name = "WSAEADDRINUSE";
    break;
  case 0x2741:
    name = "WSAEADDRNOTAVAIL";
    break;
  case 0x2742:
    name = "WSAENETDOWN";
    break;
  case 0x2743:
    name = "WSAENETUNREACH";
    break;
  case 0x2744:
    name = "WSAENETRESET";
    break;
  case 0x2745:
    name = "WSAECONNABORTED";
    break;
  case 0x2746:
    name = "WSAECONNRESET";
    break;
  case 0x2747:
    name = "WSAENOBUFS";
    break;
  case 0x2748:
    name = "WSAEISCONN";
    break;
  case 0x2749:
    name = "WSAENOTCONN";
    break;
  case 0x274A:
    name = "WSAESHUTDOWN";
    break;
  case 0x274B:
    name = "WSAETOOMANYREFS";
    break;

  case 0x274C:
    name = "WSAETIMEDOUT";
    break;

  case 0x274D:
    name = "WSAECONNREFUSED";
    break;
  case 0x274E:
    name = "WSAELOOP";
    break;
  case 0x274F:
    name = "WSAENAMETOOLONG";
    break;
  case 0x2750:
    name = "WSAEHOSTDOWN";
    break;
  case 0x2751:
    name = "WSAEHOSTUNREACH";
    break;
  case 0x2752:
    name = "WSAENOTEMPTY";
    break;
  case 0x2753:
    name = "WSAEPROCLIM";
    break;
  case 0x2754:
    name = "WSAEUSERS";
    break;
  case 0x2755:
    name = "WSAEDQUOT";
    break;
  case 0x2756:
    name = "WSAESTALE";
    break;
  case 0x2757:
    name = "WSAEREMOTE";
    break;

  case 0x276B:
    name = "WSASYSNOTREADY";
    break;
  case 0x276C:
    name = "WSAVERNOTSUPPORTED";
    break;
  case 0x276D:
    name = "WSANOTINITIALISED";
    break;

  case 0x2775:
    name = "WSAEDISCON";
    break;
  case 0x2776:
    name = "WSAENOMORE";
    break;
  case 0x2777:
    name = "WSAECANCELLED";
    break;
  case 0x2778:
    name = "WSAEINVALIDPROCTABLE";
    break;
  case 0x2779:
    name = "WSAEINVALIDPROVIDER";
    break;
  case 0x277A:
    name = "WSAEPROVIDERFAILEDINIT";
    break;
  case 0x277B:
    name = "WSASYSCALLFAILURE";
    break;
  case 0x277C:
    name = "WSASERVICE_NOT_FOUND";
    break;
  case 0x277D:
    name = "WSATYPE_NOT_FOUND";
    break;
  case 0x277E:
    name = "WSA_E_NO_MORE";
    break;
  case 0x277F:
    name = "WSA_E_CANCELLED";
    break;
  case 0x2780:
    name = "WSAEREFUSED";
    break;

  case 0x2AF9:
    name = "WSAHOST_NOT_FOUND";
    break;
  case 0x2AFA:
    name = "WSATRY_AGAIN";
    break;
  case 0x2AFB:
    name = "WSANO_RECOVERY";
    break;
  case 0x2AFC:
    name = "WSANO_DATA";
    break;

  case 0x2AFD:
    name = "WSA_QOS_RECEIVERS";
    break;
  case 0x2AFE:
    name = "WSA_QOS_SENDERS";
    break;
  case 0x2AFF:
    name = "WSA_QOS_NO_SENDERS";
    break;
  case 0x2B00:
    name = "WSA_QOS_NO_RECEIVERS";
    break;
  case 0x2B01:
    name = "WSA_QOS_REQUEST_CONFIRMED";
    break;
  case 0x2B02:
    name = "WSA_QOS_ADMISSION_FAILURE";
    break;
  case 0x2B03:
    name = "WSA_QOS_POLICY_FAILURE";
    break;
  case 0x2B04:
    name = "WSA_QOS_BAD_STYLE";
    break;
  case 0x2B05:
    name = "WSA_QOS_BAD_OBJECT";
    break;
  case 0x2B06:
    name = "WSA_QOS_TRAFFIC_CTRL_ERROR";
    break;
  case 0x2B07:
    name = "WSA_QOS_GENERIC_ERROR";
    break;

  default:
    name = "<unknown error>";
    break;
  }

  *(const char **)0x335098 = name;
  if (error_code != *(int *)0x3352a0) {
    error(3, "winsock error #%d: %s", error_code, name);
    *(int *)0x3352a0 = error_code;
  }
  return name;
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
