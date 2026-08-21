/* Xbox network transport layer — Winsock/XNet wrapper. */
#include "../../common.h"

/* Publish the global XNet key pair and register it on the first use.
 *
 * Copies the caller's 16-byte blob to the global at 0x5ab210 and the
 * caller's 8-byte blob to the global at 0x5ab220, then — only while the
 * reference count at 0x335094 is still zero — registers the pair via
 * FUN_00222de0 and asserts the call succeeded.  The reference count is
 * incremented unconditionally on every call, including the ones that skip
 * the registration; FUN_00082b30 performs the matching decrement and calls
 * FUN_00222df7 on the 0x5ab220 blob when it drops back to zero.
 *
 * Confirmed: FUN_00222de0 (0x222de0, __stdcall 2 args — CALL at 0x81e50 is
 * followed directly by TEST EAX,EAX with no ADD ESP, so the callee cleans);
 * push order at 0x81e46/0x81e4b puts 0x5ab220 in arg1 and 0x5ab210 in arg2;
 * display_assert (0x8d9f0, cdecl 4 args) with message "0 == error" at
 * 0x26649c, __FILE__ at 0x266458, line 0x5c = 92; system_exit (0x8e2f0).
 *
 * Inferred (not named for it): the 8-byte/16-byte pair and the sibling
 * single-argument release FUN_00222df7 match the XNetRegisterKey /
 * XNetUnregisterKey (XNKID, XNKEY) shape, but nothing in the binary names
 * the import, so the thunks keep their FUN_ names.
 *
 * Uncertain: the two globals are untyped blobs here; no field of either is
 * read by this function, so no struct is invented for them.
 */
void FUN_00081e00(const uint32_t *key, const uint32_t *id)
{
  int error;

  *(uint32_t *)0x5ab210 = key[0];
  *(uint32_t *)0x5ab214 = key[1];
  *(uint32_t *)0x5ab218 = key[2];
  *(uint32_t *)0x5ab21c = key[3];
  *(uint32_t *)0x5ab220 = id[0];
  *(uint32_t *)0x5ab224 = id[1];

  if (*(int *)0x335094 == 0) {
    error = FUN_00222de0((void *)0x5ab220, (void *)0x5ab210);
    assert_halt_at(
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      92, 0 == error);
  }

  *(int *)0x335094 += 1;
}

/* The 8-byte transport nonce global at 0x5ab228, filled by
 * transport_initialize.  Named from the assert text "bytes ==
 * sizeof(global_nonce)" at 0x2664c0; the 8-byte size is proven by the
 * literal 8 the assert compares against and by the csmemcpy length. */
#define global_nonce (*(uint8_t(*)[8])0x5ab228)

/* Copy the 8-byte global transport nonce to the caller's buffer.
 *
 * The nonce lives at 0x5ab228 and is filled during transport_initialize
 * (the 0x222e0e call at 0x5ab228/8).  Both parameters are asserted before
 * the copy; the size assert is an exact `== 8` compare against the nonce's
 * size, so the function only ever copies the whole blob.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with "dst != NULL" at
 * 0x2664e0 line 0x97 = 151 and "bytes == sizeof(global_nonce)" at 0x2664c0
 * line 0x98 = 152, __FILE__ at 0x266458; system_exit (0x8e2f0, PUSH -1);
 * csmemcpy (0x8e0b0, cdecl 3 args — PUSH 8 / PUSH 0x5ab228 / PUSH ESI,
 * ADD ESP,0xc at 0x81f1e).
 *
 * Uncertain: the nonce blob is untyped here; no field of it is read by this
 * function, so no struct is invented for it.
 */
void transport_get_nonce(void *dst, int bytes)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    151, dst != NULL);
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    152, bytes == sizeof(global_nonce));

  csmemcpy(dst, global_nonce, sizeof(global_nonce));
}

/* Copy the local transport address blob to the caller's buffer and return it.
 *
 * Confirmed (0x82060-0x82083): three dword loads from 0x5ab230, 0x5ab234 and
 * 0x5ab238 stored to [dst+0], [dst+4] and [dst+8]; the parameter is returned
 * unchanged in EAX (MOV EAX,[EBP+8] at 0x82069, never reloaded).  Exactly 12
 * bytes are copied — no loop, no csmemcpy, no assert, no stack frame beyond
 * PUSH EBP.  The lone caller (network_server_manager advertise builder) reads
 * back only dword 0..2 from its scratch buffer, matching the 12-byte size.
 *
 * Uncertain: the blob at 0x5ab230 is an untyped 12-byte global here.  Only
 * whole dwords are moved, so no field layout is proven and no struct is
 * invented; the "xnaddr" in the kb.json name is not corroborated by any
 * string or field access inside this function.
 */
void *transport_get_xnaddr(void *dst)
{
  ((uint32_t *)dst)[0] = *(uint32_t *)0x5ab230;
  ((uint32_t *)dst)[1] = *(uint32_t *)0x5ab234;
  ((uint32_t *)dst)[2] = *(uint32_t *)0x5ab238;

  return dst;
}

/* Return the 64-bit transport key ID from the global at 0x5ab220. */
int64_t transport_get_key_id(void)
{
  if (*(int *)0x335094 <= 0) {
    display_assert(
      "global_key_depth > 0",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      0xe0, 1);
    system_exit(-1);
  }

  return *(int64_t *)0x5ab220;
}

/* Copy the 16-byte transport key from the global at 0x5ab210 into the
   destination buffer and return it. */
void *transport_get_key(void *dst)
{
  uint32_t *out = (uint32_t *)dst;

  if (*(int *)0x335094 <= 0) {
    display_assert(
      "global_key_depth > 0",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      0xe7, 1);
    system_exit(-1);
  }

  out[0] = *(uint32_t *)0x5ab210;
  out[1] = *(uint32_t *)0x5ab214;
  out[2] = *(uint32_t *)0x5ab218;
  out[3] = *(uint32_t *)0x5ab21c;

  return dst;
}

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
  xnet_result =
    ((int(__stdcall *)(uint8_t *))0x2231f8)(/* hazard-ok: fnptr-conv */
                                            xnet_params);
  if (xnet_result != 0)
    return;

  /* Start WinSock 2.2. */
  /* hazard-ok: fnptr-conv */
  wsa_result = ((int16_t(__stdcall *)(
    int16_t, uint8_t *))0x223206)(
                                  2, wsadata);
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
    link_result =
      ((int(__stdcall *)(void *))0x222ecf)(/* hazard-ok: fnptr-conv */
                                           (void *)0x5ab230);
    if (system_milliseconds() > deadline)
      break;
    if (link_result == 0)
      continue;
    if (link_result != 1) {
      /* Link detected — configure socket options and mark initialized. */
      ((int(__stdcall *)(void *, int))0x222e0e)(/* hazard-ok: fnptr-conv */
                                                (void *)0x5ab228, 8);
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
bool transport_network_available(void)
{
  return XNetGetEthernetLinkStatus() & 1;
}

/* Destroy an endpoint set: free its endpoint array, then the set itself.
 *
 * Asserts the set and its endpoint-array pointer are both non-NULL (one
 * short-circuited && producing a single shared failure block in the
 * reference), then asserts the transport is initialized, then releases the
 * array at set+0x104 followed by the set allocation itself.  Both releases
 * go through debug_free with the original source path and line, so the
 * allocator's leak bookkeeping blames the original Bungie call sites.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with message
 * "set && set->ep_array" at 0x2665d4 line 0x1bb and "transport_initialized"
 * at 0x265fe4 line 0x1bc, __FILE__ at 0x266458; system_exit (0x8e2f0,
 * PUSH -1); debug_free (0x8ef70, cdecl 3 args) at lines 0x1be and 0x1bf.
 * Offset 0x104 is the same endpoint-array pointer remove_endpoint_from_set
 * reads as endpoint_set[0x41].  The array pointer is re-loaded from the set
 * at 0x8246e after the second assert, not kept live from the first test.
 *
 * Uncertain: the reference tail is XOR AX,AX before the epilogue, which is
 * the MSVC shape for a 16-bit return value, but kb.json declares this void
 * and no caller evidence names a returned value, so the void decl is kept
 * and the zeroing is left unmodelled. */
void delete_endpoint_set(int set)
{
  /* Both asserts are written De Morgan'd rather than via assert_halt_msg_at:
   * the macro's `!(a && b)` makes VC71 materialize the condition as a 0/1
   * value in EAX instead of emitting the original's two-branch
   * `test esi,esi / jz` + `test eax,eax / jnz` (measured; same effect
   * documented at players.c set_player_powerup). */
  if (set == 0 || *(int *)(set + 0x104) == 0) {
    display_assert(
      "set && set->ep_array",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      0x1bb, 1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x335090 == 0) {
    display_assert(
      "transport_initialized",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      0x1bc, 1);
    system_exit(-1);
  }

  debug_free(
    *(void **)(set + 0x104),
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x1be);
  debug_free(
    (void *)set,
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x1bf);
}

/* Remove an endpoint from an endpoint set.
 * Searches the set's endpoint_array for the matching pointer, then finds and
 * removes the endpoint's socket from the fd_array by shifting. Clears the
 * "in set" flag (bit 3) on the endpoint, nulls the array slot, and marks
 * the set dirty. Returns 0 on success, -19 if the endpoint is not found. */
short remove_endpoint_from_set(int *endpoint, uint32_t *endpoint_set)
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

/* Test whether a Winsock endpoint is currently connected.
 *
 * Asserts that endpoint is non-null, then returns the state of the
 * connected flag (bit 0 of the byte at endpoint+4). This flag is cleared
 * by send_endpoint when it receives disconnect/abort errors from Winsock.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl, 4 args); system_exit (0x8e2f0).
 * Confirmed: bit 0 of *(byte*)(endpoint+4) is the connected flag.
 */
bool FUN_000831a0(int endpoint)
{
  assert_halt(endpoint);
  return *(uint8_t *)(endpoint + 4) & 1;
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

/* Get the socket address for an endpoint.
 *
 * Tries getsockname (0x224876) first; if that fails, tries getpeername
 * (0x22486b).  Both are XNet thunks with signature
 * __stdcall(int socket, void *name, int *namelen) RET 0xc.  The local
 * sockaddr buffer is 16 bytes (AF_INET: family=2, port, sin_addr).
 *
 * On success: stores ntohl(sin_addr) at addr[0], stores 4 as a uint16_t
 * at byte offset 0x10, stores ntohs(sin_port) as a uint16_t at byte
 * offset 0x12; clears ep[6] (status) to 0; returns 0.
 * On failure: calls xapi_GetLastError and reports via winsock_error_report;
 * sets ep[6] to 0xfff1; returns 0xfff1 (short -15).
 *
 * ep struct layout (from disassembly):
 *   [ep+0]  int      socket fd (-1 = invalid)
 *   [ep+6]  int16_t  status/error code
 *
 * Confirmed: xnet_getsockname (0x224876, __stdcall 3 args, RET 0xc);
 * xnet_getpeername (0x22486b, __stdcall 3 args, RET 0xc);
 * xapi_GetLastError (0x2235c4 thunk -> 0x1d2240);
 * winsock_error_report (0x83310, cdecl 1 arg);
 * transport_initialized flag at 0x335090;
 * assert strings: "ep && address" at 0x266c70, file at 0x266618;
 * source lines 0xf7/0xf8.
 */
short FUN_00083a60(int *ep, void *addr)
{
  int result;
  int err;
  uint32_t ip;
  uint32_t ip_host;
  uint16_t port;
  uint16_t port_host;
  int16_t sa_buf[8]; /* 16-byte sockaddr_in buffer */
  int sa_len;

  sa_len = 0x10;

  assert_halt(ep && addr);
  assert_halt(*(uint8_t *)0x335090);

  if (*ep != -1) {
    result = xnet_getsockname(*ep, sa_buf, &sa_len);
    if (result == 0) {
      if (sa_buf[0] == 2) {
        /* AF_INET: extract and byte-swap IP and port. */
        port = (uint16_t)sa_buf[1];
        ip = *(uint32_t *)((char *)sa_buf + 4);
        /* ntohl(ip): reorder bytes from network order to host order. */
        ip_host = (((ip & 0xff0000u) | (ip >> 16)) >> 8) |
                  (((ip & 0xff00u) | (ip << 16)) << 8);
        /* ntohs(port): swap port bytes. */
        port_host =
          (uint16_t)(((uint16_t)(port << 8)) | ((uint16_t)(port >> 8)));
        *(uint32_t *)addr = ip_host;
        *(uint16_t *)((char *)addr + 0x10) = 4;
        *(uint16_t *)((char *)addr + 0x12) = port_host;
        *(int16_t *)((char *)ep + 6) = 0;
        return 0;
      }
    } else {
      result = xnet_getpeername(*ep, sa_buf, &sa_len);
      if (result == 0 && sa_buf[0] == 2) {
        port = (uint16_t)sa_buf[1];
        ip = *(uint32_t *)((char *)sa_buf + 4);
        ip_host = (((ip & 0xff0000u) | (ip >> 16)) >> 8) |
                  (((ip & 0xff00u) | (ip << 16)) << 8);
        port_host =
          (uint16_t)(((uint16_t)(port << 8)) | ((uint16_t)(port >> 8)));
        *(uint32_t *)addr = ip_host;
        *(uint16_t *)((char *)addr + 0x10) = 4;
        *(uint16_t *)((char *)addr + 0x12) = port_host;
        *(int16_t *)((char *)ep + 6) = 0;
        return 0;
      }
    }
    err = xapi_GetLastError();
    winsock_error_report(err);
  }
  *(int16_t *)((char *)ep + 6) = (int16_t)0xfff1;
  return (short)0xfff1;
}

/* Bind a transport endpoint to an address.
 *
 * If the socket is not yet created (== -1), creates one via FUN_00083930
 * (regarg: ECX=af, EDX=type, EAX=protocol) using SOCK_STREAM=1 for TCP
 * (ep->type==0x12) or SOCK_DGRAM=2 for UDP (ep->type==0x11). Converts
 * the custom address format (host-order IP at addr[0], type at addr+0x10,
 * host-order port at addr+0x12) into a sockaddr_in and calls xnet_bind
 * (0x225197, stdcall 3 args).
 *
 * Returns 0 on success, -1 if socket creation fails or type is unknown,
 * -14 (0xfff2) if bind fails.
 *
 * Confirmed: FUN_00083930 (0x83930, regarg ECX/EDX/EAX);
 * xnet_bind (0x225197, stdcall 3 args);
 * xapi_GetLastError (0x2235c4); winsock_error_report (0x83310, cdecl);
 * transport_initialized at 0x335090; source lines 0x16c/0x16d.
 */
short FUN_00083ce0(int *ep, void *addr)
{
  int socket_result;
  int bind_result;
  int error_code;
  short status;
  uint32_t ip;
  uint16_t port;
  uint8_t sa[16];

  status = 0;

  assert_halt(ep && addr);
  assert_halt(*(uint8_t *)0x335090);

  if (*ep == -1) {
    if (*(uint8_t *)((char *)ep + 5) == 0x12) {
      socket_result = FUN_00083930(2, 1, 0);
      *ep = socket_result;
      if (socket_result != -1)
        goto do_bind;
      status = -1;
    } else if (*(uint8_t *)((char *)ep + 5) == 0x11) {
      socket_result = FUN_00083930(2, 2, 0);
      *ep = socket_result;
      if (socket_result != -1)
        goto do_bind;
      status = -1;
    } else {
      status = -12;
    }
    if (*ep == -1 || status != 0) {
      *(uint16_t *)((char *)ep + 6) = 0xffff;
      return -1;
    }
  }

do_bind:
  ip = *(uint32_t *)addr;
  *(uint32_t *)(sa + 4) = (((ip & 0xff0000u) | (ip >> 16)) >> 8) |
                          (((ip & 0xff00u) | (ip << 16)) << 8);
  port = *(uint16_t *)((char *)addr + 0x12);
  *(uint16_t *)(sa + 2) =
    (uint16_t)(((uint16_t)(port << 8)) | ((uint16_t)(port >> 8)));
  *(uint16_t *)sa = 2;

  bind_result = xnet_bind(*ep, sa, 0x10);
  if (bind_result == 0) {
    *(int16_t *)((char *)ep + 6) = status;
    return status;
  }

  error_code = xapi_GetLastError();
  winsock_error_report(error_code);
  *(int16_t *)((char *)ep + 6) = (int16_t)0xfff2;
  return (short)0xfff2;
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

/* Cancel an in-progress connection attempt.
 *
 * Validates the connect_handle struct (non-null, has ep, has thread), then
 * calls endpoint_pool_cleanup() before attempting to acquire the connection
 * mutex with a 1000 ms timeout. On success: closes the underlying socket via
 * close_endpoint(), clears all endpoint flags (word at ep+6), marks the
 * handle as cancelled (byte at connect_handle+0x24), and releases the mutex.
 * On failure to acquire the mutex: asserts and halts.
 *
 * connect_handle layout (int[] offsets):
 *   [0]  int * — pointer to the endpoint struct (socket handle at ep[0])
 *   [7]  int   — thread reference (must be non-zero)
 *   [8]  int * — pointer to the mutex HANDLE for this connection
 *   [9]  char  — cancel flag (set to 1 on successful cancel, byte at +0x24)
 *
 * Confirmed: assert "input && input->ep && input->thread" at line 0x298;
 *   assert "!\"unable to get mutex in cancel_connect_process()!\"" at 0x2a5;
 *   source file
 * "c:\halo\SOURCE\bungie_net\network\transport_endpoint_winsock.c";
 *   take_mutex (mutex_acquire, 0x81870, cdecl 2 args: mutex_ref, timeout_ms);
 *   release_mutex (mutex_release, 0x818d0, cdecl 1 arg: mutex_ref);
 *   close_endpoint (0x84000); endpoint_pool_cleanup (0x82d30).
 *   endpoint flags word cleared at ep+6 after close; cancel flag at ESI+0x24.
 */
void transport_server_terminate(int *connect_handle)
{
  if (connect_handle == NULL || connect_handle[0] == 0 ||
      connect_handle[7] == 0) {
    display_assert(
      "input && input->ep && input->thread",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
      0x298, 1);
    system_exit(-1);
  }

  endpoint_pool_cleanup();

  if (take_mutex((int *)connect_handle[8], 1000)) {
    close_endpoint((int *)connect_handle[0]);
    *(uint16_t *)((char *)(int *)connect_handle[0] + 6) = 0;
    *(uint8_t *)((char *)connect_handle + 0x24) = 1;
    release_mutex((int *)connect_handle[8]);
    return;
  }

  display_assert(
    "!\"unable to get mutex in cancel_connect_process()!\"",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x2a5, 1);
  system_exit(-1);
}

/* Receive a UDP datagram and return the sender's address.
 *
 * If the endpoint's socket is not yet created (== -1), creates a UDP socket
 * via FUN_00083930 (regarg: ECX=af, EDX=type, EAX=protocol) and binds to
 * any address/port via FUN_00083ce0. Then calls xnet_recvfrom (0x225cd1,
 * stdcall 6 args). On success, converts the sender's sockaddr_in to the
 * custom address format: addr[0]=ntohl(ip), addr+0x10=4, addr+0x12=ntohs(port).
 * On failure, classifies the Winsock error:
 *   WSAEWOULDBLOCK (0x2733) -> return -4
 *   Disconnect family (0x2744-0x274c) -> clear bits 0,2 of flags, return -3
 *   Other -> clear bit 2 of flags, return -2
 *
 * Confirmed: FUN_00083930 (0x83930, regarg ECX/EDX/EAX, creates socket);
 * FUN_00083ce0 (0x83ce0, cdecl 2 args, binds endpoint);
 * xnet_recvfrom (0x225cd1, stdcall 6 args);
 * xapi_GetLastError (0x2235c4); transport_initialized at 0x335090.
 * Assert strings at 0x266db0/0x266618; source lines 0x377-0x38b.
 */
int FUN_00084520(int *ep, void *buffer, int length, void *addr)
{
  int socket_result;
  short bind_result;
  int recv_result;
  int error_code;
  uint32_t bind_addr[6];
  uint8_t from_addr[16];
  int from_len;
  uint32_t ip;
  uint16_t port;

  from_len = 0x10;

  assert_halt(ep && buffer && addr && (length > 0));
  assert_halt(*(uint8_t *)0x335090);

  if (*ep == -1) {
    assert_halt(*(uint8_t *)((char *)ep + 5) == 0x11);

    socket_result = FUN_00083930(2, 2, 0);
    *ep = socket_result;
    if (socket_result != -1) {
      bind_addr[0] = 0;
      bind_addr[1] = 0;
      bind_addr[2] = 0;
      bind_addr[3] = 0;
      bind_addr[4] = 0;
      bind_addr[5] = 0;
      *(uint16_t *)&bind_addr[4] = 4;

      bind_result = FUN_00083ce0(ep, (void *)bind_addr);
      assert_halt(bind_result == 0);

      if (*ep != -1)
        goto do_recvfrom;
    }
    *(uint16_t *)((char *)ep + 6) = 0xffff;
  } else {
  do_recvfrom:
    assert_halt(!(*(uint8_t *)((char *)ep + 4) & 1));

    recv_result = xnet_recvfrom(*ep, buffer, length, 0, from_addr, &from_len);
    if (recv_result != -1) {
      if (recv_result >= 0) {
        ip = *(uint32_t *)(from_addr + 4);
        *(uint32_t *)addr = (((ip & 0xff0000u) | (ip >> 16)) >> 8) |
                            (((ip & 0xff00u) | (ip << 16)) << 8);
        *(uint16_t *)((char *)addr + 0x10) = 4;
        port = *(uint16_t *)(from_addr + 2);
        *(uint16_t *)((char *)addr + 0x12) =
          (uint16_t)(((uint16_t)(port << 8)) | ((uint16_t)(port >> 8)));
      }
      return recv_result;
    }
  }

  error_code = xapi_GetLastError();
  switch (error_code) {
  case 0x2733:
    return -4;
  case 0x2744:
  case 0x2745:
  case 0x2746:
  case 0x2749:
  case 0x274a:
  case 0x274c:
    *(uint8_t *)((char *)ep + 4) &= 0xfa;
    return -3;
  default:
    *(uint8_t *)((char *)ep + 4) &= 0xfb;
    return -2;
  }
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
