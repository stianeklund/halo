/* Xbox network transport layer — Winsock/XNet wrapper. */
#include "../../../common.h"

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

/* Compare two 8-byte transport nonces for equality.
 *
 * Both pointers are asserted non-NULL, then the two blobs are compared with
 * csmemcmp over the nonce size; the function returns true only when they are
 * byte-identical.
 *
 * Confirmed (0x81f30-0x81f98): src is [EBP+8] (ESI), dst is [EBP+0xc] (EDI);
 * display_assert (0x8d9f0, cdecl 4 args) with "src != NULL" at 0x2664ec line
 * 0xa3 = 163 and "dst != NULL" at 0x2664e0 line 0xa4 = 164, __FILE__ at
 * 0x266458; system_exit (0x8e2f0, PUSH -1); csmemcmp (0x8da40, cdecl 3 args —
 * PUSH 8 / PUSH EDI / PUSH ESI, ADD ESP,0xc at 0x81f8c).  The tail
 * NEG EAX / SBB AL,AL / INC AL at 0x81f8f-0x81f94 is the MSVC lowering of a
 * boolean `csmemcmp(...) == 0`, so the result is a bool in AL, not the raw
 * memcmp value.
 *
 * Uncertain: the nonce blobs are untyped here; no field of either is read by
 * this function, so no struct is invented for them.  The 8 is the literal
 * pushed at 0x81f83 and equals sizeof(global_nonce).
 */
/* noinline (VC71 verification only): the original build emits this out of
 * line — its sole in-TU caller, transport_nonce_is_equal_to_global (0x81fa0),
 * reaches it through a real CALL at 0x81ff7.  /O2's implied /Ob2 otherwise
 * inlines the body into that caller and scrambles both functions' shape. */
__declspec(noinline) bool transport_nonce_is_equal(const void *src,
                                                   const void *dst)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    163, src != NULL);
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    164, dst != NULL);

  return csmemcmp(src, dst, sizeof(global_nonce)) == 0;
}

/* Test whether the caller's nonce matches this machine's global nonce.
 *
 * Both parameters are asserted first, then the whole comparison is delegated
 * to transport_nonce_is_equal with the global nonce as the second operand.
 *
 * Confirmed (0x81fa0-0x82001): src is [EBP+8] (loaded into ESI at 0x81fa4),
 * bytes is [EBP+0xc] (compared against the literal 8 at 0x81fcb);
 * display_assert (0x8d9f0, cdecl 4 args) with "src != NULL" at 0x2664ec line
 * 0xaf = 175 and "bytes == sizeof(global_nonce)" at 0x2664c0 line 0xb0 = 176,
 * __FILE__ at 0x266458; system_exit (0x8e2f0, PUSH -1).  The tail call is
 * transport_nonce_is_equal (0x81f30, cdecl 2 args — PUSH 0x5ab228 at 0x81ff1
 * then PUSH ESI at 0x81ff6, so src is arg1 and global_nonce is arg2, ADD
 * ESP,8 at 0x81ffc).
 *
 * The result is RETURNED, not discarded: nothing between the CALL and the RET
 * touches EAX, and the sole caller (0x1272c1 in FUN_00127260) does TEST AL,AL
 * / JE at 0x1272c9 on the value, so this is a bool-in-AL return.
 *
 * Uncertain: the name is behavioural only — no string or symbol in the binary
 * names this function; the nonce blobs stay untyped 8-byte globals.
 */
bool transport_nonce_is_equal_to_global(const void *src, int bytes)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    175, src != NULL);
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    176, bytes == sizeof(global_nonce));

  return transport_nonce_is_equal(src, global_nonce);
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

/* Three-way comparison of two dwords by NULL-ness only, used as a sort
 * predicate: a zero slot orders after a non-zero slot, and two slots that are
 * both zero or both non-zero compare equal.  Sorting an array with this
 * compacts the live entries to the front while leaving their relative order
 * otherwise undecided.
 *
 * Confirmed from disassembly at 0x824a0: a plain `push ebp / mov ebp,esp`
 * frame with no locals and no calls; both arguments arrive on the stack
 * (EBP+0x8, EBP+0xc) and are dereferenced as dwords into EAX and ECX before
 * any branch.  `test eax,eax / jnz` selects the *a != 0 arm, each arm then
 * tests ECX; the three exits are `mov eax,1` (0x824b5), `or eax,-1` (0x824c0)
 * and `xor eax,eax` (0x824c5, shared by both arms), so the return value is a
 * full 32-bit int of 1 / -1 / 0.
 *
 * Uncertain: the only reference is a DATA xref from 0x8255b inside
 * poll_endpoint_set (0x824d0), i.e. the address is taken and handed to
 * something else rather than called directly, so the original parameter types
 * are unproven.  The dereferences are dword-wide, and nothing here reveals
 * whether the compared dwords are pointers, handles or counts, so the
 * parameters stay `const int *`.  No name evidence exists for this function
 * (no assert string, no __FILE__ line of its own), so it keeps FUN_ naming. */
int FUN_000824a0(const int *a, const int *b)
{
  /* Both dwords are loaded before the first branch, not re-read per arm: the
   * reference does MOV EAX,[EAX] / TEST EAX,EAX / MOV ECX,[EBP+0xc] /
   * MOV ECX,[ECX] ahead of the JNZ at 0x824af, and both arms then reuse ECX
   * with a bare TEST (0x824b1, 0x824bc).  Reading through the parameters
   * inside the branches instead makes VC71 reload EBP+0xc twice and emit
   * CMP [reg],0 in place of the MOV/TEST pair (measured: 73.2%). */
  int a_value;
  int b_value;

  a_value = *a;
  b_value = *b;

  if (a_value == 0) {
    if (b_value != 0) {
      return 1;
    }
  } else if (b_value == 0) {
    return -1;
  }

  return 0;
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

/* Reset an endpoint set's iteration cursor.
 *
 * Asserts the set pointer is non-NULL and that the transport is initialized,
 * then stores 0 to the dword at set+0x110.  That dword is the only field
 * touched; it is the cursor the get-next-endpoint walk advances (0x82940 is
 * the "rewind" half of that pair).  No other state is read or written.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with message "set" at
 * 0x266450 line 0x26d and "transport_initialized" at 0x265fe4 line 0x26e,
 * __FILE__ string at 0x266458; system_exit (0x8e2f0, PUSH -1) after each.
 * Both asserts are single-condition two-branch tests in the reference
 * (TEST ESI,ESI / JNZ at 0x82949; MOV AL,[0x335090] / TEST AL,AL / JNZ at
 * 0x8296b), so each maps to one assert macro.  The store is
 * MOV dword ptr [ESI + 0x110],0x0 at 0x82994.
 *
 * Uncertain: the meaning of the field at 0x110 beyond "cursor reset to 0" is
 * not proven by this function alone; nothing here names it. */
void rewind_endpoint_set(int endpoint_set)
{
  assert_halt_msg_at(
    "set",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x26d, endpoint_set);
  assert_halt_msg_at(
    "transport_initialized",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x26e, *(uint8_t *)0x335090);

  *(uint32_t *)(endpoint_set + 0x110) = 0;
}

/* Fetch the next entry from an endpoint set's iteration walk.
 *
 * This is the "next" half of the pair whose "rewind" half is 0x82940: it
 * reads the cursor dword at set+0x110, and while the cursor is <= the count
 * dword at set+0x10c (signed compare, CMP/JG at 0x82a0d) it loads element
 * [cursor] from the dword array whose base pointer lives at set+0x104,
 * post-increments the cursor, and returns the element.  Once the cursor
 * passes the count it returns 0 without touching the cursor.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with message "set" at
 * 0x266450 line 0x27a and "transport_initialized" at 0x265fe4 line 0x27b,
 * __FILE__ string at 0x266458; system_exit (0x8e2f0, PUSH -1) after each.
 * Both asserts are single-condition two-branch tests in the reference
 * (TEST ESI,ESI / JNZ at 0x829ba; MOV AL,[0x335090] / TEST AL,AL / JNZ at
 * 0x829de), so each maps to one assert macro.  The zero return is
 * materialized from a pre-zeroed EDI (XOR EDI,EDI at 0x829b8; MOV EAX,EDI
 * at 0x82a29).
 *
 * Uncertain: the element type behind set+0x104 is only ever read as a dword
 * here, and the field at 0x10c is proven to be an inclusive bound but is not
 * named by anything in this function.  The base pointer at 0x104 is the same
 * array delete_endpoint_set indexes as endpoint_set[0x41]. */
int FUN_000829b0(int endpoint_set)
{
  int cursor;
  int entry;

  assert_halt_msg_at(
    "set",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x27a, endpoint_set);
  assert_halt_msg_at(
    "transport_initialized",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x27b, *(uint8_t *)0x335090);

  cursor = *(int *)(endpoint_set + 0x110);
  if (cursor <= *(int *)(endpoint_set + 0x10c)) {
    entry = *(int *)(*(int *)(endpoint_set + 0x104) + cursor * 4);
    *(int *)(endpoint_set + 0x110) = cursor + 1;
    return entry;
  }

  return 0;
}

/* Report the number of entries an endpoint set holds.
 *
 * Asserts the set pointer is non-NULL and that the transport is initialized,
 * then returns the dword at set+0x10c plus one.  Nothing is written; the
 * cursor at set+0x110 is not touched.  The +1 is consistent with 0x10c being
 * the inclusive high index the iteration walk in FUN_000829b0 compares the
 * cursor against (CMP/JG at 0x82a0d), so the returned value is a count rather
 * than the stored bound.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with message "set" at
 * 0x266450 line 0x289 and "transport_initialized" at 0x265fe4 line 0x28a,
 * __FILE__ string at 0x266458; system_exit (0x8e2f0, PUSH -1) after each.
 * Both asserts are single-condition two-branch tests in the reference
 * (TEST ESI,ESI / JNZ at 0x82a39; MOV AL,[0x335090] / TEST AL,AL / JNZ at
 * 0x82a62), so each maps to one assert macro.  The return is
 * MOV EAX,dword ptr [ESI + 0x10c] / INC EAX at 0x82a84.
 *
 * Uncertain: whether 0x10c is stored as a count-minus-one or as a last-index
 * is not decidable from this function; only the +1 relation is proven. */
int FUN_00082a30(int endpoint_set)
{
  assert_halt_msg_at(
    "set",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x289, endpoint_set);
  assert_halt_msg_at(
    "transport_initialized",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x28a, *(uint8_t *)0x335090);

  return *(int *)(endpoint_set + 0x10c) + 1;
}

/* Drop any owned global XNet key, then mint and publish a fresh one.
 *
 * If the "key owned" flag (0x335091) is set, releases that reference:
 * decrements global_key_depth (0x335094) and, when it reaches zero, calls
 * FUN_00222df7 on the key-id blob at 0x5ab220; the owned flag is then
 * cleared.  global_key_depth must be zero at that point (second assert), so
 * this runs only when no other holder remains.  The byte at 0x5ab204 is set
 * to 1, a new (id, key) pair is generated into locals by FUN_00222da0, and
 * FUN_00081e00 publishes the pair into the 0x5ab210/0x5ab220 globals and
 * re-registers it.  FUN_00082b30 is the matching teardown, which clears
 * 0x5ab204 again.
 *
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with "global_key_depth
 * > 0" at 0x2664a8 line 0x66 and "0 == global_key_depth" at 0x2665f8 line
 * 0x79, __FILE__ string at 0x266458; system_exit (0x8e2f0, PUSH -1) after
 * each.  DEC dword ptr [0x335094] / JNZ at 0x82ac5 is a decrement-then-test.
 * MOV byte ptr [0x5ab204],0x1 at 0x82b0c is a one-byte store, not a word.
 * FUN_00222da0 (0x222da0) is __stdcall with 2 args: the CALL at 0x82b13 is
 * followed directly by LEA with no ADD ESP, and the pushes at 0x82b07/0x82b0b
 * put the 8-byte local (EBP-0x8) in arg1 and the 16-byte local (EBP-0x18) in
 * arg2.  FUN_00081e00 (0x81e00) is cdecl 2 args (ADD ESP,0x8 at 0x82b25) and
 * the pushes at 0x82b1b/0x82b1f pass the two locals in the OPPOSITE order:
 * the 16-byte blob is arg1 (key) and the 8-byte blob is arg2 (id), which
 * agrees with FUN_00081e00's own 4-dword/2-dword copies.
 *
 * Uncertain: the reference ends with XOR AX,AX before the epilogue, i.e. it
 * materializes a 16-bit zero in the return register.  The single caller
 * (0x12ef51 in FUN_0012eef0) ignores it and no other evidence types the
 * result, so the kb.json declaration is left as void and that one instruction
 * is not reproduced.  The two locals are untyped blobs; no field of either is
 * read here, so no struct is invented for them.
 */
void FUN_00082a90(void)
{
  uint32_t id[2];
  uint32_t key[4];

  if (*(uint8_t *)0x335091 != 0) {
    assert_halt_msg_at(
      "global_key_depth > 0",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      0x66, *(int *)0x335094 > 0);
    *(int *)0x335094 -= 1;
    if (*(int *)0x335094 == 0) {
      FUN_00222df7((void *)0x5ab220);
    }
    *(uint8_t *)0x335091 = 0;
  }

  assert_halt_msg_at(
    "0 == global_key_depth",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
    0x79, 0 == *(int *)0x335094);

  *(uint8_t *)0x5ab204 = 1;
  FUN_00222da0(id, key);
  FUN_00081e00(key, id);
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

/* Register a key/id pair and fill in an address record from an XNet address.
 *
 * First runs the inlined release-global-key helper (source line 0x66, the same
 * block that opens FUN_00082a90 and FUN_00082b30): if the owned flag at
 * 0x335091 is set, decrement global_key_depth (0x335094) and release the key
 * object at 0x5ab220 when it reaches zero, then clear the flag.  Registers the
 * caller's key/id with FUN_00081e00, translates param_1 plus the id into a
 * 32-bit address with FUN_00222e31, byte-reverses that value into out[0], and
 * writes a type tag, the caller's 16-bit field, and a zero.  Finally re-arms
 * the owned flag at 0x335091.
 *
 * Confirmed (0x82bd0-0x82c87): display_assert (0x8d9f0, cdecl 4 args, msg
 * 0x2664a8, __FILE__ 0x266458, line 0x66); system_exit (0x8e2f0);
 * FUN_00222df7 (0x222df7, __stdcall 1 arg);
 * FUN_00081e00 (0x81e00, cdecl 2 args, ADD ESP,8 at 0x82c2d) -- pushes are
 * ESI=[EBP+0x10] then EAX=[EBP+0xc], so args are (key=[EBP+0xc],
 * id=[EBP+0x10]), matching that callee's own kb.json declaration;
 * FUN_00222e31 (0x222e31, __stdcall 3 args, no ADD ESP after the CALL) --
 * pushes are ECX=&local, ESI, EDX=[EBP+0x8], so args are ([EBP+0x8],
 * [EBP+0x10], &local); ESI is callee-saved and still holds [EBP+0x10].
 * Stores: dword [out+0x00] = byte-reverse(local), word [out+0x10] = 4,
 * word [out+0x12] = [EBP+0x14] (MOV CX, so 16-bit), dword [out+0x14] = 0.
 *
 * Uncertain: the meaning of param_1 and of the constant 4 stored at out+0x10
 * are not evidenced by any string, so both stay mechanical.  Bytes 0x04-0x0f
 * of the output record are never accessed here, so no struct is invented for
 * it and the caller is assumed to own those bytes.
 */
void FUN_00082bd0(void *param_1, const uint32_t *key, const uint32_t *id,
                  uint16_t param_4, uint32_t *out)
{
  uint32_t address;

  if (*(uint8_t *)0x335091 != 0) {
    assert_halt_msg_at(
      "global_key_depth > 0",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_set_winsock.c",
      0x66, *(int *)0x335094 > 0);
    *(int *)0x335094 -= 1;
    if (*(int *)0x335094 == 0) {
      FUN_00222df7((void *)0x5ab220);
    }
    *(uint8_t *)0x335091 = 0;
  }

  FUN_00081e00(key, id);
  FUN_00222e31(param_1, id, &address);

  out[0] = (((address & 0xff0000) | (address >> 16)) >> 8) |
           (((address & 0xff00) | (address << 16)) << 8);
  *(uint16_t *)(out + 4) = 4;
  *(uint16_t *)((char *)out + 0x12) = param_4;
  out[5] = 0;
  *(uint8_t *)0x335091 = 1;
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

/* Return the signed byte stored at endpoint offset 5.
 *
 * Guards on a non-NULL endpoint and on the transport_initialized flag, then
 * sign-extends the byte at ep+5 into the 32-bit return register.
 *
 * Confirmed (0x82df0-0x82e4a): the incoming pointer is loaded once
 * (MOV ESI,dword ptr [EBP+0x8] at 0x82df4) and both the NULL guard
 * (TEST ESI,ESI / JNZ at 0x82df7) and the returned load use it.
 * display_assert (0x8d9f0, cdecl 4 args) is called with msg "ep" at 0x266658
 * line 0x12c and msg "transport_initialized" at 0x265fe4 line 0x12d; both
 * pushes of the __FILE__ argument are 0x266618, which is
 * "c:\halo\SOURCE\bungie_net\network\transport_endpoint_winsock.c" -- NOT the
 * _set_ file string at 0x266458 used by the endpoint-set functions above.
 * Each assert is followed by PUSH -1 / CALL system_exit (0x8e2f0) with no
 * stack cleanup, so each maps to one assert macro.  The return is
 * MOVSX EAX,byte ptr [ESI + 0x5] at 0x82e44: a sign-extending byte load into
 * a 32-bit result, so the field is a signed byte and the return type is int.
 *
 * Uncertain: the kb.json name count_endpoints_in_set is not supported by this
 * function's own evidence -- the asserted parameter is spelled "ep" and the
 * __FILE__ string is the per-endpoint TU, not the endpoint-set TU, and no
 * iteration or accumulation occurs.  The name is left unchanged because it is
 * pre-existing kb.json state, not because it is proven.  There are no xrefs to
 * 0x82df0 in the binary, so no caller constrains the parameter type; it is
 * declared int * to match the ep parameter of the other endpoint functions in
 * this TU.  The meaning of the byte at ep+5 is unknown (recv_endpoint documents
 * ep+4 as a flags byte and ep+6 as a 16-bit status, leaving ep+5 unclaimed). */
int count_endpoints_in_set(int *ep)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x12c, ep);
  assert_halt_msg_at(
    "transport_initialized",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x12d, *(uint8_t *)0x335090);

  return *(signed char *)((char *)ep + 5);
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

/* Wait until a Winsock endpoint's socket becomes writable, up to a timeout.
 *
 * Builds a single-entry fd_set holding ep->socket and passes it as select()'s
 * WRITE set (arg 3) with a timeval of {0 sec, timeout_msec * 1000 usec}.
 * Returns true only when select() reports at least one ready descriptor AND
 * __WSAFDIsSet confirms our socket is the one that became ready; otherwise
 * false (timeout, select error, or a set that does not contain the socket).
 *
 * The kb.json name transport_server_initialize is retained (it is the name in
 * tools/verify/function_bounds.json and the pipeline target), but nothing in
 * this function initializes a server: the only observable behavior is the
 * writability poll described above.  Treat the name as unproven.
 *
 * Local layout (from the disassembly's SUB ESP,0x10c):
 *   [EBP-0x10c] 0x104-byte fd_set   — dword 0 = fd_count, dwords 1.. = fd_array
 *                                     (0x104 = 4 + 64*4, i.e. FD_SETSIZE 64)
 *   [EBP-0x008] 8-byte timeval      — dword 0 = tv_sec, dword 1 = tv_usec
 * fd_count and fd_array[0] are stored directly; there is no FD_ZERO/memset.
 *
 * Confirmed: assert message "ep && (ep->socket != INVALID_SOCKET)" at
 * 0x26667c, __FILE__ at 0x266618, line 0x417; display_assert (0x8d9f0,
 * cdecl 4 args) + system_exit (0x8e2f0) at 0x83127/0x8312e.
 * Confirmed: both calls are __stdcall — CALL 0x2251b8 at 0x8316d and
 * CALL 0x2235f3 at 0x83180 are each followed directly by TEST EAX,EAX with
 * no ADD ESP, so the callee cleans up its 5 / 2 dword arguments.
 * Confirmed argument order at 0x83145..0x83156 (first PUSH is the last arg):
 * (1, NULL, &fd_set, NULL, &timeval) and (*ep, &fd_set).
 * Confirmed: MOVZX EAX,word [EBP+0xc] then IMUL EAX,EAX,0x3e8 — the second
 * parameter is a 16-bit unsigned millisecond count scaled to microseconds.
 * Confirmed: MOV AL,1 / XOR AL,AL exits, so the return is a bool in AL.
 *
 * Inferred (from the argument shape only, no string or import names the
 * callees): 0x2251b8 has select()'s exact 5-argument Winsock ABI over an
 * FD_SETSIZE-64 fd_set and a timeval, and 0x2235f3 has __WSAFDIsSet's
 * (SOCKET, fd_set *) shape; they are registered as xnet_select /
 * xnet_wsafdisset alongside the other xnet_* wsock.obj thunks.
 *
 * Uncertain: whether select's nfds argument of 1 is meaningful here (Winsock
 * ignores nfds); it is reproduced verbatim.
 */
bool transport_server_initialize(int *ep, unsigned short timeout_msec)
{
  uint32_t write_set[65];
  int32_t timeout[2];

  assert_halt_msg_at(
    "ep && (ep->socket != INVALID_SOCKET)",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x417, ep && (*ep != -1));

  write_set[0] = 1;
  write_set[1] = (uint32_t)*ep;
  timeout[0] = 0;
  timeout[1] = timeout_msec * 1000;

  if (xnet_select(1, NULL, write_set, NULL, timeout) > 0) {
    if (xnet_wsafdisset(*ep, write_set) != 0)
      return true;
  }

  return false;
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

/* Report bit 1 of a Winsock endpoint's flag byte at ep+4.
 *
 * Asserts that ep is non-null, then returns bit 1 of the byte at ep+4 —
 * the same flag byte whose bit 0 FUN_000831a0 reports.
 *
 * Confirmed: PUSH EBP / MOV EBP,ESP / MOV ESI,[EBP+8] / TEST ESI,ESI /
 * JNZ — a single pointer parameter and a null-check assert.
 * Confirmed: display_assert (0x8d9f0, cdecl 4 args) with message "ep" at
 * 0x266658, __FILE__ at 0x266618, line 0x42e = 1070; system_exit (0x8e2f0)
 * with -1 at 0x83201/0x83203.
 * Confirmed return shape at 0x8320b..0x83214: XOR EAX,EAX / MOV AL,
 * byte ptr [ESI+4] / AND EAX,0x2 / SHR EAX,0x1 — a zero-extended byte load
 * from offset 4, masked to bit 1 and normalized to 0/1, i.e. a bool in AL.
 *
 * Uncertain: what bit 1 means.  Nothing in this function or its assert
 * string names the flag, and the binary has no xrefs to 0x831e0 (the
 * callers are through data/vtable or were inlined away), so no semantic
 * name is invented for either the function or the bit.  The sibling
 * getters at 0x83220/0x83260/0x832a0 are the remaining unlifted bits of
 * the same byte.
 */
bool FUN_000831e0(int *ep)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x42e, ep);

  return (*((const uint8_t *)ep + 4) & 2) >> 1;
}

/* Report the COMPLEMENT of bit 4 of a Winsock endpoint's flag byte at ep+4.
 *
 * Asserts that ep is non-null, then returns the inverse of bit 4 of the byte
 * at ep+4 — the same flag byte whose bit 0 FUN_000831a0 and whose bit 1
 * FUN_000831e0 report.
 *
 * Confirmed frame/param at 0x83220..0x83229: PUSH EBP / MOV EBP,ESP /
 * PUSH ESI / MOV ESI,[EBP+8] / TEST ESI,ESI / JNZ — a single pointer
 * parameter and a null-check assert.
 * Confirmed assert at 0x8322b..0x83243 (first PUSH is the last arg):
 * display_assert("ep" @0x266658, __FILE__ @0x266618, 0x436, true) — cdecl,
 * 4 args — followed by system_exit(-1) (PUSH -0x1 / CALL 0x8e2f0).
 * 0x436 = line 1078.
 * Confirmed return shape at 0x8324b..0x83256: XOR EAX,EAX / MOV AL,
 * byte ptr [ESI+4] / SHR EAX,0x4 / NOT EAX / AND EAX,0x1 — zero-extended
 * byte load from offset 4, shifted down by 4, complemented, masked to bit 0.
 * The shift-then-NOT-then-AND order (vs. the sibling's AND-then-SHR at
 * 0x831e0) is preserved verbatim in the expression below.
 *
 * Uncertain: what bit 4 means, and whether the original source spelled this
 * as a negated bitfield read.  Nothing here or in the assert string names
 * the flag, and the artifact records no xrefs to 0x83220 (callers are
 * through data/vtable or were inlined away), so no semantic name is
 * invented for the function or the bit.  The sibling getters at 0x83260 and
 * 0x832a0 are the remaining unlifted bits of the same byte.
 */
bool FUN_00083220(int *ep)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x436, ep);

  return ~(*((const uint8_t *)ep + 4) >> 4) & 1;
}

/* Report the 16-bit field at ep+6 of a Winsock endpoint.
 *
 * Asserts that ep is non-null, then returns the halfword at ep+6.  This is
 * NOT another bit of the flag byte at ep+4 that the 0x831a0/0x831e0/0x83220
 * siblings read: the load here is a full word from a different offset.
 *
 * Confirmed frame/param at 0x83260..0x83269: PUSH EBP / MOV EBP,ESP /
 * PUSH ESI / MOV ESI,dword ptr [EBP+8] / TEST ESI,ESI / JNZ 0x8328b — a
 * single pointer parameter and a null-check assert.
 * Confirmed assert at 0x8326b..0x83283 (first PUSH is the last arg):
 * display_assert("ep" @0x266658, __FILE__ @0x266618, 0x43e, true) — cdecl,
 * 4 args — followed by system_exit(-1) (PUSH -0x1 / CALL 0x8e2f0).
 * 0x43e = line 1086.
 * Confirmed return shape at 0x8328b: MOV AX,word ptr [ESI + 0x6] — a
 * 16-bit load into AX only; the upper half of EAX is never written, so the
 * return type is 16-bit wide, not int.
 *
 * Uncertain: the meaning of the field at ep+6, and its signedness.  A bare
 * MOV AX carries no sign information and the artifact records no xrefs to
 * 0x83260 (callers are through data/vtable or were inlined away), so the
 * only proven fact is the 16-bit width; unsigned is the conservative
 * spelling for a raw field read and produces the same load.  Ghidra's
 * decompile of this function shows `void (void)` and drops the return
 * entirely — that is an artifact of the stale kb.json prototype, not
 * evidence that the value is unused.
 */
unsigned short FUN_00083260(int *ep)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x43e, ep);

  return *(const uint16_t *)((const uint8_t *)ep + 6);
}

/* Report whether two Winsock endpoints hold the same live socket handle.
 *
 * Asserts that both pointers are non-null, then returns true only when the
 * dword at offset 0 of `a` is not -1 AND equals the dword at offset 0 of `b`.
 * A pair of endpoints that both hold -1 therefore compares UNEQUAL — the
 * -1 check is not redundant with the equality test.
 *
 * Confirmed frame/params at 0x832a0..0x832aa: PUSH EBP / MOV EBP,ESP /
 * PUSH ESI / MOV ESI,dword ptr [EBP+8] / TEST ESI,ESI / PUSH EDI / JNZ
 * 0x832cc, then MOV EDI,dword ptr [EBP+0xc] / TEST EDI,EDI / JNZ 0x832f3 —
 * two pointer parameters, each with its own null-check assert.  (Ghidra's
 * decompile reports `void (void)` with `in_stack_00000004`/`in_stack_00000008`
 * and drops the return value entirely; that is an artifact of the stale
 * kb.json `void FUN_000832a0(void)` prototype, not evidence about the
 * signature.)
 * Confirmed asserts (first PUSH is the last arg, cdecl 4 args):
 *   0x832ac..0x832bd display_assert("a" @0x266090, __FILE__ @0x266618,
 *     0x447, true) then system_exit(-1) (PUSH -0x1 / CALL 0x8e2f0);
 *   0x832d3..0x832e4 display_assert("b" @0x26608c, __FILE__ @0x266618,
 *     0x448, true) then system_exit(-1).
 * 0x447 = line 1095, 0x448 = line 1096 — consecutive lines, i.e. two
 * separate one-argument asserts, and the messages are the bare parameter
 * names, so the original parameters were spelled `a` and `b`.
 * Confirmed compare/return shape at 0x832f3..0x8330c: MOV EAX,dword ptr
 * [ESI] / CMP EAX,-0x1 / JZ false / CMP EAX,dword ptr [EDI] / JNZ false /
 * MOV EAX,0x1 / RET; false: XOR EAX,EAX / RET — a bool in AL/EAX, with the
 * short-circuit order (-1 test first, then the equality test) preserved
 * below.  Only offset 0 of either endpoint is read.
 *
 * Confirmed by the assert string at 0x83200's sibling in this same TU
 * ("ep && (ep->socket != INVALID_SOCKET)", used at line 989 for the same
 * dword at offset 0): offset 0 is the socket handle and -1 is
 * INVALID_SOCKET.  The literal is spelled -1 here to match the rest of the
 * TU and the CMP immediate.
 *
 * Uncertain: what the caller does with the result, and whether `a`/`b` are
 * ordered (e.g. incoming vs. stored).  The artifact records no xrefs to
 * 0x832a0 — callers are through data/vtable or were inlined away — so no
 * semantic name is invented for the function or its parameters beyond the
 * two the assert strings prove.
 */
bool FUN_000832a0(int *a, int *b)
{
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x447, a);
  assert_halt_at(
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x448, b);

  return *a != -1 && *a == *b;
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

/* Set or clear non-blocking mode on a Winsock endpoint's socket.
 *
 * Reads the endpoint's current state from bit 4 of the flag byte at
 * endpoint+4 — the same bit whose complement FUN_00083220 reports.  The
 * `(~(*(byte*)(endpoint+4) >> 4) & 1) == 0` test below is FUN_00083220's
 * own return expression compared against zero: the compiler inlined that
 * getter's body here verbatim, carrying over its assert's original source
 * line (0x436) into this function too.
 *
 * If bit 4 is set and `flag` is nonzero, calls ioctlsocket(FIONBIO, 0) to
 * put the socket back into blocking mode and clears bit 4 on success.  If
 * bit 4 is clear and `flag` is zero, calls ioctlsocket(FIONBIO, 1) to put
 * the socket into non-blocking mode and sets bit 4 on success.  If `flag`
 * already matches the bit-4 state, no ioctlsocket call is made and the
 * function returns 0 unchanged. On ioctlsocket failure, reports the
 * Winsock error via xapi_GetLastError()/winsock_error_report() and
 * returns -0x12.  The 16-bit result is always stored at endpoint+6.
 *
 * Confirmed: assert_halt_msg_at "ep" at line 0x139, "transport_initialized"
 * at line 0x13a, "ep" again at line 0x436 (file
 * c:\halo\SOURCE\bungie_net\network\transport_endpoint_winsock.c).
 * FIONBIO == 0x8004667e is the standard Winsock ioctl code for
 * non-blocking mode. Call-site audit confirms FUN_00224633 is a plain
 * cdecl-shaped 3-arg call (no register args) with no ESP cleanup after
 * either call site (0x83c73/0x83ca5), i.e. __stdcall — matching the
 * xnet_bind/xnet_getsockname/xnet_closesocket wrapper family already
 * registered in the same object (XNET:wsock.obj, 0x224633 falls inside
 * that object's address range). Registered here as xnet_ioctlsocket.
 *
 * Uncertain: no source-level name is recovered for bit 4 of the flag
 * byte; kept as the same raw offset the sibling getters use.
 */
short FUN_00083bd0(int endpoint, int flag)
{
  short status;
  int sock;
  uint32_t mode;
  int error_code;

  status = 0;

  assert_halt_msg_at(
    "ep", "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x139, endpoint != 0);
  assert_halt_msg_at(
    "transport_initialized",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x13a, *(uint8_t *)0x335090 != 0);
  assert_halt_msg_at(
    "ep", "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x436, endpoint != 0);

  if ((~(*((uint8_t *)endpoint + 4) >> 4) & 1) == 0) {
    if (flag == 0)
      goto store_status;
    sock = *(int *)endpoint;
    mode = 0;
    status = xnet_ioctlsocket(sock, 0x8004667e, &mode);
    if (status == 0) {
      *((uint8_t *)endpoint + 4) &= 0xef;
      *(int16_t *)((char *)endpoint + 6) = 0;
      return 0;
    }
  } else {
    if (flag != 0)
      goto store_status;
    sock = *(int *)endpoint;
    mode = 1;
    status = xnet_ioctlsocket(sock, 0x8004667e, &mode);
    if (status == 0) {
      *((uint8_t *)endpoint + 4) |= 0x10;
      *(int16_t *)((char *)endpoint + 6) = 0;
      return 0;
    }
  }

  error_code = xapi_GetLastError();
  winsock_error_report(error_code);
  status = -0x12;

store_status:
  *(int16_t *)((char *)endpoint + 6) = status;
  return status;
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

/* Worker-thread body for one asynchronous endpoint connect request.
 *
 * The address of this function is taken as data at 0x84261 inside
 * connect_endpoint_async (FUN_000841b0), so it is the thread routine that
 * services a pending connect; the request block arrives as the single
 * thread parameter and is freed here.  It performs the blocking connect via
 * FUN_00083e20, then acquires the request mutex with a 1000 ms timeout.
 * With the mutex held: if the cancel flag (byte at +0x24) is set the socket
 * is closed via close_endpoint, and the mutex/thread references are latched
 * for the teardown below.  If the mutex cannot be acquired the reported
 * status becomes -1 and nothing is torn down.  The status is always written
 * to the endpoint's status word at ep+6 and returned.
 *
 * request block layout (int[] offsets):
 *   [0]    int *  endpoint (socket handle at ep[0], status word at ep+6)
 *   [1..6] —      address blob, passed by pointer to FUN_00083e20
 *   [7]    int    thread reference (asserted non-zero)
 *   [8]    int *  mutex reference
 *   [9]    char   cancel flag (byte at +0x24)
 *
 * Confirmed: __stdcall with one stack arg (RET 4 at 0x841a3); the result is
 * the status sign-extended from BX (MOVSX EAX,BX at 0x8419e);
 * FUN_00083e20 (0x83e20, cdecl 2 args — PUSH LEA[EDI+4] then PUSH [EDI], so
 * arg1 = input->ep and arg2 = &input[1]);
 * take_mutex (0x81870, cdecl 2 args, timeout 0x3e8);
 * close_endpoint (0x84000, cdecl 1 arg);
 * debug_free (0x8ef70, cdecl 3 args, line 0x252);
 * release_mutex (0x818d0, cdecl 1 arg);
 * FUN_00081910 (0x81910, cdecl 1 arg — the PUSH ESI at 0x84187 is covered by
 * the ADD ESP,0x14 at 0x8418d that also cleans the debug_free and
 * release_mutex pushes, so the callee does not pop its argument);
 * FUN_00082cf0 (0x82cf0, no arguments, no stack cleanup);
 * assert strings at 0x266c9c/0x266c90/0x266c80/0x265fe4 with source lines
 * 0x239-0x23c; transport_initialized flag at 0x335090.
 *
 * Uncertain: MSVC reuses the parameter home slot [EBP+8] for the latched
 * thread reference while the request pointer itself stays in EDI for the
 * rest of the function (MOV [EBP+8],ECX at 0x8415f, PUSH EDI at 0x8417b).
 * On the mutex-failure path that slot therefore still holds the incoming
 * request pointer, which is non-NULL, so FUN_00082cf0 still runs.  Modelled
 * here by reassigning the parameter and keeping the request pointer in a
 * separate local, which reproduces that behaviour exactly.
 */
int __stdcall FUN_00084080(int *input)
{
  int *request;
  int *mutex;
  short status;

  request = input;
  mutex = NULL;

  if (request == NULL) {
    display_assert(
      "input",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
      0x239, 1);
    system_exit(-1);
  }
  if (request[0] == 0) {
    display_assert(
      "input->ep",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
      0x23a, 1);
    system_exit(-1);
  }
  if (request[7] == 0) {
    display_assert(
      "input->thread",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
      0x23b, 1);
    system_exit(-1);
  }
  if (*(uint8_t *)0x335090 == 0) {
    display_assert(
      "transport_initialized",
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
      0x23c, 1);
    system_exit(-1);
  }

  status = FUN_00083e20(request[0], (int)(request + 1));

  if (take_mutex((int *)request[8], 1000)) {
    if (*(uint8_t *)((char *)request + 0x24) != 0) {
      close_endpoint((int *)request[0]);
    }
    mutex = (int *)request[8];
    input = (int *)request[7];
  } else {
    status = -1;
  }

  *(short *)((char *)(int *)request[0] + 6) = status;

  if (mutex != NULL) {
    debug_free(
      request,
      "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
      0x252);
    release_mutex(mutex);
    FUN_00081910(mutex);
  }

  if (input != NULL) {
    FUN_00082cf0();
  }

  return status;
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

/* Accept a pending connection on a listening endpoint.
 *
 * Calls xnet_accept (0x2251ad, stdcall 3 args) on the listening endpoint's
 * socket (field +0x0) with a 16-byte sockaddr scratch buffer and a length
 * in/out of 0x10.  The accepted address is written into that stack buffer
 * and never read again — only the returned socket is used.
 *
 * On accept failure (-1) the Winsock error is fetched and reported via
 * winsock_error_report, the endpoint's status word at +6 is set to 0xffff,
 * and 0 is returned.  On success a fresh endpoint is obtained from
 * get_next_endpoint_from_set (0x82d70, cdecl 1 arg) keyed on the listening
 * endpoint's protocol byte at +5 (read sign-extended, MOVSX at 0x844c9); if
 * the pool is exhausted the status word at +6 is set to 0xfff7 and 0 is
 * returned.  Otherwise the accepted socket is stored at the new endpoint's
 * +0x0, bit 0 of the flag byte at +0x4 is set, and the new endpoint is
 * returned.
 *
 * Confirmed: xnet_accept (0x2251ad, stdcall — PUSH EAX/ECX/EDX at
 * 0x844b7-0x844bc then CALL with no ADD ESP); xapi_GetLastError (0x2235c4
 * thunk -> 0x1d2240) whose EAX is pushed straight into winsock_error_report
 * (0x83310, cdecl 1 arg, ADD ESP,4 at 0x84504); get_next_endpoint_from_set
 * (0x82d70, cdecl 1 arg, ADD ESP,4 at 0x844d3); display_assert (0x8d9f0,
 * cdecl 4 args) with message "listening_endpoint && (listening_endpoint->
 * socket >= 0)" at 0x266d20 line 0x2d1 and "transport_initialized" at
 * 0x265fe4 line 0x2d2, __FILE__ string 0x266618 =
 * transport_endpoint_winsock.c; system_exit (0x8e2f0, PUSH -1) after each.
 * transport_initialized at 0x335090.  Frame: SUB ESP,0x14 = the 16-byte
 * sockaddr at EBP-0x14 plus the length dword at EBP-0x4, which is
 * initialized to 0x10 at 0x84460 BEFORE the first assert branch.
 *
 * Uncertain: only the null half of the first assert's condition is compiled
 * (TEST ESI,ESI / JNZ at 0x8445d-0x84467); the "socket >= 0" half of the
 * recovered message string is not emitted, so assert_halt_msg_at carries the
 * original text with the condition the binary actually tests.
 *
 * Shape: the result is a single NULL-initialized local returned once at the
 * end (XOR EBX,EBX at 0x8445b with EBX callee-saved across both calls, and
 * MOV EAX,EBX at 0x8450f on the accept-failure exit; the pool-exhausted exit
 * at 0x844ec returns the already-zero EAX from the allocator).  Both failure
 * blocks are out-of-line after the success epilogue (JZ at 0x844c7/0x844d8),
 * so the tests are written positively here.
 *
 * Inferred: 0x2251ad is named xnet_accept from its call shape
 * (listening socket, sockaddr out, addrlen in/out, returns a new socket or
 * -1) and its adjacency to the xnet_bind thunk at 0x225197; nothing in the
 * binary names the import.
 */
int FUN_00084450(int listening_endpoint)
{
  int addr_len;
  uint8_t remote_addr[16];
  int accepted_socket;
  int *endpoint;

  endpoint = NULL;
  addr_len = 0x10;

  assert_halt_msg_at(
    "listening_endpoint && (listening_endpoint->socket >= 0)",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x2d1, listening_endpoint);
  assert_halt_msg_at(
    "transport_initialized",
    "c:\\halo\\SOURCE\\bungie_net\\network\\transport_endpoint_winsock.c",
    0x2d2, *(uint8_t *)0x335090);

  accepted_socket =
    xnet_accept(*(int *)listening_endpoint, remote_addr, &addr_len);
  if (accepted_socket != -1) {
    /* Protocol byte at +5 selects the endpoint flavour to allocate. */
    endpoint = (int *)get_next_endpoint_from_set(
      (int)*(signed char *)(listening_endpoint + 5));
    if (endpoint != NULL) {
      endpoint[0] = accepted_socket;
      *(uint8_t *)((char *)endpoint + 4) |= 1;
    } else {
      *(uint16_t *)(listening_endpoint + 6) = 0xfff7;
    }
  } else {
    winsock_error_report(xapi_GetLastError());
    *(uint16_t *)(listening_endpoint + 6) = 0xffff;
  }

  return (int)endpoint;
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

/* Reject a pending connection on a listening endpoint.
 *
 * Accepts the queued connection via FUN_00084450 (0x84450) and, when one was
 * produced, immediately destroys it with destroy_endpoint (0x848c0).  The net
 * effect is to drain and silently drop one pending connection.
 *
 * Confirmed: cdecl, one stack arg at [EBP+8] forwarded as the sole argument to
 * both calls (PUSH EAX / ADD ESP,4 at 0x84947 and 0x84954); the accepted
 * handle is tested with TEST EAX,EAX / JZ, so the destroy call is skipped on
 * a NULL result.  Return is 16-bit: the single exit does XOR AX,AX (operand
 * size 16 at 0x8495c), i.e. the function always returns 0 in AX and leaves the
 * upper half of EAX undefined -- the caller at 0x129c18 (network_connection.c)
 * stores the result into a short.
 *
 * Unknown: whether the original spelled the return type short or another
 * 16-bit type; nothing in the binary names it.  No asserts are present in this
 * function, unlike its two callees.
 */
short FUN_00084940(int listening_endpoint)
{
  int *endpoint;

  endpoint = (int *)FUN_00084450(listening_endpoint);
  if (endpoint != NULL) {
    destroy_endpoint(endpoint);
  }

  return 0;
}

/* Initialize a transport endpoint record: stamp offset+0 with the current
 * time and zero offsets+4 and +8.
 *
 * Confirmed (0x84970-0x8498e): cdecl, one stack arg (int *ep, [EBP+8]).
 * Store order in the binary is offset+8 first, then the call, then
 * offset+0, then offset+4 (MOV [ESI+8],0 / CALL system_milliseconds /
 * MOV [ESI],EAX / MOV [ESI+4],0); reproduced here in the same order.
 * system_milliseconds is 0x8e370 (confirmed by call_site_audit), matching
 * the disassembly's direct CALL target -- the cached decompile text names
 * an unrelated "thunk_FUN_001d0581" and is not trusted here.
 *
 * Unknown: semantic names of ep[1]/ep[2] (offsets+4/+8); no caller is
 * present in this binary (xrefs_to reports none), so field roles beyond
 * "zeroed by this initializer" are unproven. No return value.
 */
void FUN_00084970(int *ep)
{
  ep[2] = 0;
  ep[0] = (int)system_milliseconds();
  ep[1] = 0;
}
