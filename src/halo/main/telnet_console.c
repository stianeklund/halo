/*
 * telnet_console.c — Debug telnet server for runtime console access.
 *
 * Listens on TCP port 23 (telnet). Accepts exactly one client connection at a
 * time. Each process tick checks for new connections and forwards received data
 * to the HS (HaloScript) command processor via FUN_00130b70.
 *
 * Globals layout at 0x46eee0 (0x8c bytes, zeroed by initialize/dispose):
 *   [+0x00] int*  listening_endpoint  — TCP listening socket wrapper
 *   [+0x04] int*  clients[0].ep       — accepted client endpoint (one slot)
 *   [+0x08] char  clients[0].buf[128] — telnet line input buffer
 *   [+0x88] char  initialized         — non-zero when subsystem is live
 */

/*
 * Transport endpoint helpers — not in kb.json (only used here).
 * All calls go through hardcoded function-pointer casts.
 *
 *   0x82d70  create_transport_endpoint(int type) -> int* (NULL on failure)
 *   0x83ce0  bind_endpoint(int *ep, void *addr)  -> int16_t (0=ok)
 *   0x843a0  listen_endpoint(int *ep)             -> int16_t (0=ok)
 *   0x848c0  destroy_endpoint(int *ep)            -> void
 *   0x83040  endpoint_readable(int *ep, uint16_t timeout_ms) -> char (bool)
 *   0x84450  accept_endpoint(int *ep)             -> int* (NULL if none)
 *   0x82f50  send_endpoint(int *ep, char *buf, int len) -> int (<=0 = error)
 *   0x82e50  recv_endpoint(int *ep, char *buf, int maxlen) -> int (<=0 = error)
 *   0x81c80  transport_error_string(int16_t code) -> char* (description)
 *   0x130b70 process_telnet_client_input(char *buf, int len)
 *            -> char (bool: 1=ok, 0=close client); ESI = &clients[0] (implicit)
 */

/* telnet_console_globals layout (as addressed from 0x46eee0): */
#define tc_listening_ep (*(int **)0x46eee0)
#define tc_client0_ep (*(int **)0x46eee4)
#define tc_initialized (*(char *)0x46ef68)

/* Maximum number of simultaneous telnet clients. */
#define TELNET_CONSOLE_MAX_CLIENTS 1

/* TCP transport type code. */
#define TRANSPORT_TYPE_TCP 0x12

/*
 * telnet_console_initialize — create listening TCP socket on port 23.
 *
 * Allocates a TCP transport endpoint, binds it to INADDR_ANY:23, and sets
 * the socket to listen.  Sets the initialized flag on success; logs and
 * tears down on any failure.
 */
void telnet_console_initialize(void)
{
  int16_t result;
  int *ep;

  /* Transport address struct (0x18 bytes).  Layout inferred from bind_endpoint
   * internals: ip at [+0], unknown word at [+0x10], port (host order) at
   * [+0x12].  All other bytes are zero (INADDR_ANY, no options). */
  struct {
    uint32_t ip; /* [+0x00] INADDR_ANY = 0 */
    uint8_t pad[0x10]; /* [+0x04..+0x0f] zeroed */
    uint16_t unk_10; /* [+0x10] = 0x0004 (observed, unknown field) */
    uint16_t port; /* [+0x12] = 23 (telnet), host byte order */
    uint32_t pad2; /* [+0x14] = 0 */
  } addr;

  /* Zero the globals block before populating it. */
  csmemset((void *)0x46eee0, 0, 0x8c);

  /* Allocate a TCP endpoint. */
  ep = ((int *(*)(int))0x82d70)(TRANSPORT_TYPE_TCP);
  if (ep == 0) {
    error(2, "create_transport_endpoint() failed on telnet console endpoint");
    return;
  }
  tc_listening_ep = ep;

  /* Build the bind address: INADDR_ANY on port 23. */
  csmemset(&addr, 0, sizeof(addr));
  addr.unk_10 = 0x0004;
  addr.port = 0x0017; /* 23 decimal = telnet */

  result = ((int16_t(*)(int *, void *))0x83ce0)(ep, &addr);
  if (result != 0) {
    error(2, "bind_endpoint() failed on telnet console endpoint");
    goto fail;
  }

  result = ((int16_t(*)(int *))0x843a0)(ep);
  if (result != 0) {
    error(2, "listen_endpoint() failed on telnet console endpoint");
    goto fail;
  }

  /* Mark subsystem active. */
  tc_initialized = 1;
  return;

fail:
  ((void (*)(int *))0x848c0)(ep);
  tc_listening_ep = 0;
}

/*
 * telnet_console_dispose — shut down the telnet server.
 *
 * Closes the listening endpoint and any connected client, then zeros the
 * entire globals block so the subsystem is in a clean uninitialized state.
 */
void telnet_console_dispose(void)
{
  if (tc_initialized) {
    if (tc_listening_ep != 0) {
      ((void (*)(int *))0x848c0)(tc_listening_ep);
    }
    if (tc_client0_ep != 0) {
      ((void (*)(int *))0x848c0)(tc_client0_ep);
    }
  }
  csmemset((void *)0x46eee0, 0, 0x8c);
}

/*
 * telnet_console_process — per-tick I/O pump for the telnet console.
 *
 * Called once per game tick.  Two independent sections:
 *
 * 1. Accept section: if the listening endpoint is readable, attempt to
 *    accept a new connection.  If accepted and a free client slot exists,
 *    send a greeting and record the endpoint.  If all slots are occupied,
 *    send a rejection message and close the new connection.
 *
 * 2. Receive section: if the connected client's endpoint is readable,
 *    recv up to 0x20 bytes and dispatch to FUN_00130b70 (the per-character
 *    input handler, which echoes chars and processes CR/LF as commands).
 *    On recv error or handler failure, log and close the client.
 *
 * Note: FUN_00130b70 (0x130b70) reads ESI as an implicit register argument
 * pointing to the client slot struct at 0x46eee4.  It cannot be expressed
 * as a plain C function pointer.  The call uses inline asm to load ESI
 * before the call and pass the two stack arguments via "r" constraints
 * (safe — no "m" constraints with pushl, per project convention).
 */
void telnet_console_process(void)
{
  int slot;
  int *new_ep;
  int *slot_ep;
  char readable;
  int recv_result;
  char recv_buf[0x20];
  char input_ok;
  char *error_str;

  if (!tc_initialized) {
    return;
  }

  /* --- Accept section --- */
  readable = ((char (*)(int *, uint16_t))0x83040)(tc_listening_ep, 0);
  if (readable) {
    new_ep = ((int *(*)(int *))0x84450)(tc_listening_ep);
    if (new_ep != 0) {
      /* Search for a free client slot (max 1). */
      for (slot = 0; slot < TELNET_CONSOLE_MAX_CLIENTS; slot++) {
        if (*(int *)(0x46eee4 + slot * 0x84) == 0) {
          /* Found a free slot — send greeting. */
          const char *greeting = "Would you like to play a game?\r\n";
          int greet_len = csstrlen(greeting);
          int sent = ((int (*)(int *, const char *, int))0x82f50)(
            new_ep, greeting, greet_len);
          if (sent < 1) {
            /* Send failed; reject the connection. */
            ((void (*)(int *))0x848c0)(new_ep);
          } else {
            /* Store the accepted endpoint and clear the input buffer. */
            *(int *)(0x46eee4 + slot * 0x84) = (int)new_ep;
            *(char *)(0x46eee8 + slot * 0x84) = 0;
          }
          goto accept_done;
        }
      }
      /* No free slot — send rejection and close. */
      {
        const char *full_msg =
          "sorry - the maximum number of clients are already connected."
          " goodbye!\r\n";
        int full_len = csstrlen(full_msg);
        ((int (*)(int *, const char *, int))0x82f50)(new_ep, full_msg,
                                                     full_len);
        ((void (*)(int *))0x848c0)(new_ep);
      }
    accept_done:;
    }
  }

  /* --- Receive section (client slot 0 only) --- */
  slot_ep = tc_client0_ep;
  if (slot_ep == 0) {
    return;
  }
  readable = ((char (*)(int *, uint16_t))0x83040)(slot_ep, 0);
  if (!readable) {
    return;
  }

  recv_result = ((int (*)(int *, char *, int))0x82e50)(slot_ep, recv_buf, 0x20);
  if (recv_result > 0) {
    /* Dispatch received bytes to the telnet input handler.
     * FUN_00130b70 reads ESI as a pointer to the client slot struct
     * (0x46eee4).  Use inline asm to set ESI before the call. */
    int _buf = (int)recv_buf;
    int _len = recv_result;
    int _esi = 0x46eee4;
    asm volatile("pushl %[len]\n\t"
                 "pushl %[buf]\n\t"
                 "call *%[fn]\n\t"
                 "addl $8, %%esp"
                 : "=a"(input_ok)
                 : [fn] "r"((void *)0x130b70), [buf] "r"(_buf), [len] "r"(_len),
                   [esi] "S"(_esi)
                 : "ecx", "edx", "memory", "cc");
    if (input_ok) {
      return;
    }
    error(2, "error processing telnet client");
  } else {
    /* recv returned <= 0 — connection lost or error. */
    error_str = ((char *(*)(int16_t))0x81c80)((int16_t)recv_result);
    error(2, "connection lost to telnet client ('%s')", error_str);
  }

  /* Close and nullify the client on any error or graceful disconnect. */
  if (tc_client0_ep != 0) {
    ((void (*)(int *))0x848c0)(tc_client0_ep);
    tc_client0_ep = 0;
  }
}
