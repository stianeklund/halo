#include "x87_math.h"

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

/* Antenna debug-data pool global (0x5a90d4). Read/written by the
 * antenna_debug_data_* cluster below (0x130ec0-0x131840ish) — a game_state
 * data pool exposed for live tuning, unrelated to the telnet transport
 * layer above but linked into the same object file. Not registered in
 * kb.json's globals table; addressed directly like the tc_* macros above.
 * "antenna" comes from game_state_data_new's own name-string argument;
 * the functions' own original identifiers are not otherwise evidenced. */
#define g_antenna_data (*(data_t **)0x5a90d4)

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
  int *ep;

  csmemset((void *)0x46eee0, 0, 0x8c);

  ep = (int *)get_next_endpoint_from_set(TRANSPORT_TYPE_TCP);
  tc_listening_ep = ep;
  if (ep != NULL) {
    struct {
      uint32_t address[4];
      uint16_t address_length;
      uint16_t port;
      uint32_t pad;
    } addr = { 0 };

    addr.address_length = 4;
    addr.port = 0x0017;

    if (FUN_00083ce0(ep, &addr) == 0) {
      if (FUN_000843a0((int)tc_listening_ep) == 0) {
        tc_initialized = 1;
        return;
      }
      error(2, "listen_endpoint() failed on telnet console endpoint");
      destroy_endpoint(tc_listening_ep);
      tc_listening_ep = 0;
    } else {
      error(2, "bind_endpoint() failed on telnet console endpoint");
      destroy_endpoint(tc_listening_ep);
      tc_listening_ep = 0;
    }
  } else {
    error(2, "create_transport_endpoint() failed on telnet console endpoint");
  }
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
 * pointing to the client slot struct at 0x46eee4.  kb.json declares that as
 * `void *client@<esi>`, so the build system generates the thunk and the call
 * site is plain C — no inline asm, which also lets this TU compile under
 * VC71 and be byte-scored.
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
    /* Dispatch received bytes to the telnet input handler.  FUN_00130b70
     * reads ESI as a pointer to the client slot struct (0x46eee4); kb.json
     * carries that as `void *client@<esi>`, so the build system emits the
     * thunk and this is a plain call. */
    input_ok = FUN_00130b70((void *)0x46eee4, recv_buf, recv_result);
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

/*
 * antenna_debug_data_new (0x130ec0) — allocate the antenna debug-data pool.
 *
 * game_state_data_new("antenna", 12, 700) — up to 12 elements, 700 bytes
 * each. Logs and returns without setting the global on allocation failure
 * (error(0, ...), a non-fatal severity — matches disasm's PUSH EAX where
 * EAX is the failed (NULL) result, not a fixed severity constant).
 */
void antenna_debug_data_new(void)
{
  g_antenna_data = game_state_data_new("antenna", 12, 700);
  if (g_antenna_data == NULL) {
    error(0, "couldn't allocate antenna globals");
  }
}

/*
 * antenna_debug_data_dispose (0x130ef0) — free all antenna debug-data
 * elements via data_delete_all(g_antenna_data).
 */
void antenna_debug_data_dispose(void)
{
  data_delete_all(g_antenna_data);
}

/*
 * antenna_debug_data_invalidate (0x130f00) — mark the antenna pool's
 * datums invalid via data_make_invalid(g_antenna_data).
 */
void antenna_debug_data_invalidate(void)
{
  data_make_invalid(g_antenna_data);
}

/*
 * antenna_debug_data_clear (0x130f10) — null the global pool pointer if
 * set. Disasm guards the store with a TEST/JZ (mirrors the source's
 * `if (g_antenna_data != 0) g_antenna_data = 0;` shape exactly rather than
 * an unconditional assignment).
 */
void antenna_debug_data_clear(void)
{
  if (g_antenna_data != NULL) {
    g_antenna_data = NULL;
  }
}

/*
 * antenna_debug_data_add (0x130f30) — allocate a new antenna debug-data
 * datum for a tag and populate its per-marker table.
 *
 * tag_id == -1 (no tag) returns -1 without touching the pool. Otherwise
 * looks up the 'ant!' tag, allocates a datum via data_new_at_index, and for
 * each element of the tag's marker block (+0xc4) writes a 0x20-byte record
 * at rec+0x1c+i*0x20: a running position total (elem+0x00/04/08, accumulated
 * from marker[+0x74/0x78/0x7c] AFTER being stored — so each record holds the
 * cumulative total up to but excluding the current marker), zeroed fields
 * at +0xc/+0x10/+0x14/+0x18/+0x1c, and — only when the tag has a bitmap
 * group (field_2c != -1) with a valid sequence — a scale factor at +0x18
 * computed from the first bitmap sequence's UV range, sprite lookup width,
 * and bitmap group's field_50, divided into marker[+0x24].
 *
 * A final trailing record is written one past the last marker
 * (rec+0x1c+count*0x20) holding the final accumulated total — but only 3 of
 * the 5 fields the per-marker records zero (+0xc/+0x10/+0x14); +0x18/+0x1c
 * are left untouched. This asymmetry is disasm-confirmed (0x1310fc-0x131127
 * has no writes to the trailing record's +0x18/+0x1c), not an oversight to
 * "fix".
 *
 * field_2c's tag group ('bitm') and the divide-by expression's exact FPU
 * association order are confirmed against the byte-identical Ghidra
 * decompile: element_field4/bitmap_field_50 are read as int16_t then
 * widened; the "* 2" is FADD ST0,ST0, not a separate multiply.
 */
int antenna_debug_data_add(int tag_id)
{
  char *tag_def;
  char *rec;
  int datum_handle;
  int count;
  float acc0;
  float acc1;
  float acc2;
  int i;
  char *elem;

  if (tag_id == -1) {
    return -1;
  }

  tag_def = (char *)tag_get(0x616e7421, tag_id);

  datum_handle = data_new_at_index(g_antenna_data);
  if (datum_handle == -1) {
    return -1;
  }

  rec = (char *)datum_get(g_antenna_data, datum_handle);

  rec[4] = 0;
  count = *(int *)(tag_def + 0xc4);
  *(int *)(rec + 8) = tag_id;
  rec[5] = (count < 2) ? 1 : 0;
  *(int *)(rec + 0xc) = -1;
  *(int16_t *)(rec + 6) = 0;
  *(int *)(rec + 0x18) = 0;
  *(int *)(rec + 0x14) = 0;
  *(int *)(rec + 0x10) = 0;
  acc1 = 0.0f;
  acc0 = 0.0f;
  acc2 = 0.0f;

  for (i = 0; i < count; i++) {
    char *te;
    int bitmap_group_tag;

    te = (char *)tag_block_get_element(tag_def + 0xc4, i, 0x80);
    elem = rec + 0x1c + i * 0x20;

    *(float *)(elem + 0x00) = acc0;
    *(float *)(elem + 0x04) = acc1;
    *(float *)(elem + 0x08) = acc2;
    *(int *)(elem + 0x14) = 0;
    *(int *)(elem + 0x10) = 0;
    *(int *)(elem + 0xc) = 0;
    *(int16_t *)(elem + 0x1c) = 0;
    *(int *)(elem + 0x18) = 0;

    bitmap_group_tag = *(int *)(tag_def + 0x2c);
    if (bitmap_group_tag != -1) {
      char *bitmap_tag_def;
      int16_t bitmap_group_index;

      bitmap_tag_def = (char *)tag_get(0x6269746d, bitmap_group_tag);
      bitmap_group_index = *(int16_t *)(te + 0x28);

      if (bitmap_group_index >= 0 &&
          bitmap_group_index < *(int *)(bitmap_tag_def + 0x54)) {
        char *bitmap_group_element;

        bitmap_group_element = (char *)tag_block_get_element(
          bitmap_tag_def + 0x54, bitmap_group_index, 0x40);

        if (*(int *)(bitmap_group_element + 0x34) != 0) {
          char *first_seq;
          void *lookup_result;

          first_seq =
            (char *)tag_block_get_element(bitmap_group_element + 0x34, 0, 0x20);
          lookup_result =
            bitmap_group_try_and_get_bitmap(*(int *)(tag_def + 0x2c), *(uint16_t *)first_seq);

          if (lookup_result != NULL) {
            int result_field4;
            int bitmap_field_50;
            float uv_range;

            result_field4 = *(int16_t *)((char *)lookup_result + 4);
            bitmap_field_50 = *(int16_t *)(bitmap_tag_def + 0x50);
            uv_range = *(float *)(first_seq + 0xc) - *(float *)(first_seq + 8);

            *(float *)(elem + 0x18) =
              *(float *)(te + 0x24) /
              (uv_range * (float)result_field4 -
               ((float)bitmap_field_50 + (float)bitmap_field_50) -
               *(float *)0x2533c8);
          }
        }
      }
    }

    acc0 = acc0 + *(float *)(te + 0x74);
    acc1 = acc1 + *(float *)(te + 0x78);
    acc2 = acc2 + *(float *)(te + 0x7c);
  }

  elem = rec + 0x1c + i * 0x20;
  *(float *)(elem + 0x00) = acc0;
  *(int *)(elem + 0x14) = 0;
  *(int *)(elem + 0x10) = 0;
  *(int *)(elem + 0xc) = 0;
  *(float *)(elem + 0x04) = acc1;
  *(float *)(elem + 0x08) = acc2;

  return datum_handle;
}

/*
 * antenna_debug_data_remove (0x131130) — free one antenna debug-data datum
 * via datum_delete(g_antenna_data, datum_handle).
 */
void antenna_debug_data_remove(int datum_handle)
{
  datum_delete(g_antenna_data, datum_handle);
}

/*
 * antenna_debug_data_relocate_marker (0x131150) — refresh one antenna
 * record's tracked marker position, sliding all its per-marker/total
 * points (built by antenna_debug_data_add) by the movement delta.
 *
 * rec (@<esi>) is an antenna_debug_data_add record: [+0xc] holds the
 * object handle passed to object_get_markers_by_string_id, [+0x10/14/18]
 * the last-synced marker world position, [+0x1c+i*0x20] the per-marker
 * (and, at i==count, trailing total) point table.
 *
 * marker_pos (@<edi>, in/out) receives the fetched marker's translation
 * (out_markers+0x60/64/68); world_pos_out (@<eax>) receives a second
 * translation from the same fetch (out_markers+0x3c/40/44) — both are
 * caller-owned buffers, not touched beyond these 3 floats each.
 *
 * Movement gate: each axis delta is FTOL-truncated to int, abs'd, and
 * compared against the 0x2533c8 epsilon as a float — so deltas under 1.0
 * unit always compare as 0 and never trip the gate on their own; this
 * quantization is disasm-confirmed (FILD of the truncated int, not the
 * raw float delta) and preserved as-is, not "fixed" to compare floats
 * directly. If every axis is within the epsilon the update loop is
 * skipped entirely; the last-synced-position writeback at the end always
 * runs either way.
 */
void antenna_debug_data_relocate_marker(void *world_pos_out, void *rec,
                                        void *marker_pos, void *tag_def,
                                        void *location_out)
{
  unsigned char buf[0x80];
  float dx;
  float dy;
  float dz;
  int abs_delta;

  object_get_markers_by_string_id(*(int *)((char *)rec + 0xc), tag_def, buf, 1);

  *(int *)((char *)marker_pos + 0x00) = *(int *)(buf + 0x60);
  *(int *)((char *)marker_pos + 0x04) = *(int *)(buf + 0x64);
  *(int *)((char *)marker_pos + 0x08) = *(int *)(buf + 0x68);

  *(int *)((char *)world_pos_out + 0x00) = *(int *)(buf + 0x3c);
  *(int *)((char *)world_pos_out + 0x04) = *(int *)(buf + 0x40);
  *(int *)((char *)world_pos_out + 0x08) = *(int *)(buf + 0x44);

  scenario_location_from_point(location_out, buf + 0x60);

  dx = *(float *)(buf + 0x60) - *(float *)((char *)rec + 0x10);
  dy = *(float *)(buf + 0x64) - *(float *)((char *)rec + 0x14);
  dz = *(float *)(buf + 0x68) - *(float *)((char *)rec + 0x18);

  abs_delta = (int)dx;
  abs_delta = (abs_delta ^ (abs_delta >> 0x1f)) - (abs_delta >> 0x1f);
  if ((float)abs_delta > *(float *)0x2533c8)
    goto relocate;

  abs_delta = (int)dy;
  abs_delta = (abs_delta ^ (abs_delta >> 0x1f)) - (abs_delta >> 0x1f);
  if ((float)abs_delta > *(float *)0x2533c8)
    goto relocate;

  abs_delta = (int)dz;
  abs_delta = (abs_delta ^ (abs_delta >> 0x1f)) - (abs_delta >> 0x1f);
  if (!((float)abs_delta > *(float *)0x2533c8))
    goto no_relocate;

relocate: {
  int count;

  count = *(int *)((char *)tag_def + 0xc4) + 1;
  if (count > 0) {
    int i;

    for (i = 0; i < count; i++) {
      float *elem;

      elem = (float *)((char *)rec + 0x1c + i * 0x20);
      elem[0] = dx + elem[0];
      elem[1] = dy + elem[1];
      elem[2] = dz + elem[2];
    }
  }
}

no_relocate:

  *(float *)((char *)rec + 0x10) = *(float *)(buf + 0x60);
  *(float *)((char *)rec + 0x14) = *(float *)(buf + 0x64);
  *(float *)((char *)rec + 0x18) = *(float *)(buf + 0x68);
}

/*
 * antenna_debug_data_draw_beams (0x131280) — build sprite-beam segments
 * between consecutive points of one antenna record's marker table.
 *
 * tag_def (@<ecx>) is the 'ant!' tag; rec is the antenna_debug_data_add
 * record whose table this draws (elem[i] at rec+0x1c+i*0x20, matching
 * antenna_debug_data_add/antenna_debug_data_relocate_marker's layout:
 * elem[i]+0x00/04/08 = point, elem[i]+0x18 = per-marker scale factor).
 *
 * t is a 0..1 reveal fraction lerped from a global fade-in window
 * (tag_def+0x98..+0x94) against the 0x253f00 clock, clamped to 0 below
 * the window and 1 past the 0x2533c8 epsilon — the pass-through band
 * between is deliberate (disasm-confirmed FCOMP/FNSTSW clamp, not a
 * "fix"). Segment i..i+1 is only drawn when its scale factor is nonzero
 * and t > 0 — both gates confirmed against the compound FNSTSW/TEST
 * branch in disasm and cross-checked against the Ghidra decompile twice.
 */
void antenna_debug_data_draw_beams(void *tag_def, void *rec)
{
  unsigned char sprite_build_data[0xa4];
  int *tag_block;
  int count;
  float t;
  int i;

  tag_block = (int *)((char *)tag_def + 0xc4);
  count = *tag_block;
  if (count != 0) {
    t =
      (*(float *)0x253f00 - *(float *)((char *)tag_def + 0x98)) /
      (*(float *)((char *)tag_def + 0x94) - *(float *)((char *)tag_def + 0x98));
    if (t < 0.0f) {
      t = 0.0f;
    } else if (t > *(float *)0x2533c8) {
      t = 1.0f;
    }

    build_sprites_begin((uint32_t *)sprite_build_data, (int16_t)*tag_block,
                        *(uint32_t *)((char *)tag_def + 0x2c), 0x326b30, 0);

    for (i = 0; i < *tag_block; i++) {
      char *elem;
      char *te;
      float delta[3];
      float scale;
      float color[4];

      elem = (char *)rec + i * 0x20 + 0x1c;
      te = (char *)tag_block_get_element(tag_block, i, 0x80);

      delta[0] = *(float *)(elem + 0x20) - *(float *)elem;
      delta[1] = *(float *)(elem + 0x24) - *(float *)(elem + 4);
      delta[2] = *(float *)(elem + 0x28) - *(float *)(elem + 8);

      color[0] = *(float *)(te + 0x2c);
      color[1] = *(float *)(te + 0x30);
      color[2] = *(float *)(te + 0x34);
      color[3] = *(float *)(te + 0x38);

      scale = *(float *)(elem + 0x18);

      if (scale != 0.0f && t > 0.0f) {
        int16_t bitmap_idx;

        bitmap_idx = *(int16_t *)(te + 0x28);
        FUN_0018d6e0((void *)sprite_build_data, 1, bitmap_idx, 0, (float *)elem,
                     delta, 0.0f, scale, color, t, 0);
      }
    }

    FUN_0018d360((void *)sprite_build_data);
  }
}

/*
 * antenna_debug_data_simulate_rope (0x1313f0) -- per-tick rope-physics
 * simulation for one antenna debug-data record. Reseeds marker[0] from
 * antenna_debug_data_relocate_marker's world_pos/marker_pos, then walks
 * the tag marker chain (count+1 elements, last iteration reuses the final
 * marker element for its te lookup) running point_physics_update + a
 * SQRT-based segment-length constraint blend, then rebuilds each segment's
 * rotation axis (cross(delta,+Z), falling back to global_left_vector_ptr
 * when degenerate) and applies rotate_vector3d_by_sincos to get the next
 * marker's offset. Writes back position + (pos-old_pos)*inv_dt velocity.
 */
void antenna_debug_data_simulate_rope(void *rec, void *tag_def,
                                      float delta_time)
{
  float world_pos[3];
  float marker_pos[3];
  int location_out[2]; /* {leaf_index, cluster_index@+4}; point_physics_update
                        * aliases this SAME stack slot on every loop
                        * iteration in the original disasm (LEA EBP-0x8c
                        * reused each call) -- must persist across the loop,
                        * not be a fresh per-iteration local. */
  int count;

  antenna_debug_data_relocate_marker(world_pos, rec, marker_pos, tag_def,
                                     location_out);

  if (*((char *)rec + 5) == 0 && delta_time > 0.0f) {
    count = *(int *)((char *)tag_def + 0xc4);

    if (count != -1 && count + 1 >= 0) {
      int n;
      int i;
      float inv_dt;
      float pos[3];
      float delta_vec[3];
      float prev_pos[3];
      float prev_rotated[3];

      n = 0;
      inv_dt = 1.0f / delta_time;
      i = 0;

      do {
        char *elem;
        char *te;
        int te_index;
        float weight;
        float axis[3];
        float axis_len;
        float angle;
        float sin_a;
        float cos_a;
        float offset[3];
        float z_axis[3];

        elem = (char *)rec + i * 0x20 + 0x1c;
        te_index = i;
        if (i == count) {
          te_index = count - 1;
        }
        te =
          (char *)tag_block_get_element((char *)tag_def + 0xc4, te_index, 0x80);

        weight = *(float *)te * *(float *)((char *)tag_def + 0x90);
        *(int16_t *)(elem + 0x1c) = *(int16_t *)(elem + 0x1c) + 1;

        if (n == 0) {
          pos[0] = marker_pos[0];
          pos[1] = marker_pos[1];
          pos[2] = marker_pos[2];
          delta_vec[0] = world_pos[0];
          delta_vec[1] = world_pos[1];
          delta_vec[2] = world_pos[2];
        } else {
          float dist;
          float ratio;
          float blend;
          void *physics_tag;

          pos[0] = *(float *)elem;
          pos[1] = *(float *)(elem + 4);
          pos[2] = *(float *)(elem + 8);

          physics_tag = tag_get(0x70706879, *(int *)((char *)tag_def + 0x3c));
          point_physics_update(0, (int)physics_tag, location_out, -1, pos,
                               (float *)(elem + 0xc), NULL, NULL, NULL, 0.02f,
                               delta_time);

          pos[0] = pos[0] - prev_pos[0];
          pos[1] = pos[1] - prev_pos[1];
          pos[2] = pos[2] - prev_pos[2];
          dist = x87_sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
          ratio = *(float *)(te + 0x24) / dist;

          blend = 1.0f - weight;
          pos[0] =
            prev_rotated[0] * weight + (ratio * pos[0] + prev_pos[0]) * blend;
          pos[1] =
            prev_rotated[1] * weight + blend * (ratio * pos[1] + prev_pos[1]);
          pos[2] =
            prev_rotated[2] * weight + blend * (ratio * pos[2] + prev_pos[2]);

          delta_vec[0] = pos[0] - prev_pos[0];
          delta_vec[1] = pos[1] - prev_pos[1];
          delta_vec[2] = pos[2] - prev_pos[2];
        }

        /* cross(delta_vec, (0,0,-1)); the 0.0f multiplies mirror the
         * disasm's FMUL against the shared 0.0f constant at 0x2533c0. */
        axis[0] = delta_vec[2] * 0.0f - delta_vec[1];
        axis[1] = delta_vec[0] - delta_vec[2] * 0.0f;
        axis[2] = delta_vec[1] * 0.0f - delta_vec[0] * 0.0f;

        z_axis[0] = 0.0f;
        z_axis[1] = 0.0f;
        z_axis[2] = 1.0f;

        axis_len = normalize3d(axis);
        if (axis_len == 0.0f) {
          axis[0] = global_left_vector_ptr[0];
          axis[1] = global_left_vector_ptr[1];
          axis[2] = global_left_vector_ptr[2];
        }

        offset[0] = *(float *)(te + 0x74);
        offset[1] = *(float *)(te + 0x78);
        offset[2] = *(float *)(te + 0x7c);

        angle = FUN_0010c510(z_axis, delta_vec);
        cos_a = x87_fcos(angle);
        sin_a = x87_fsin(angle);
        rotate_vector3d_by_sincos(offset, axis, sin_a, cos_a);

        prev_rotated[0] = offset[0] + pos[0];
        prev_rotated[1] = offset[1] + pos[1];
        prev_rotated[2] = offset[2] + pos[2];

        prev_pos[0] = pos[0];
        prev_pos[1] = pos[1];
        prev_pos[2] = pos[2];

        n = n + 1;

        *(float *)(elem + 0xc) = (pos[0] - *(float *)elem) * inv_dt;
        *(float *)(elem + 0x10) = (pos[1] - *(float *)(elem + 4)) * inv_dt;
        *(float *)(elem + 0x14) = (pos[2] - *(float *)(elem + 8)) * inv_dt;
        *(float *)elem = pos[0];
        *(float *)(elem + 4) = pos[1];
        *(float *)(elem + 8) = pos[2];

        count = *(int *)((char *)tag_def + 0xc4);
        i = n;
      } while (i < count + 1);
    }
  }
}

/*
 * antenna_debug_data_set_object (0x131700) -- attach an object to an
 * antenna debug-data record and force a simulation catch-up.
 *
 * Validates the object handle (discarded result, assert-only), fetches the
 * record and its tag def, then if the record is active (rec[5]==0): stores
 * the object handle at rec+0xc, and if the per-tick counter at rec+6 has
 * drifted past 5 ticks without an update, runs 3 extra rope-simulation
 * steps at a fixed 0.05f timestep to catch the chain back up before
 * resetting the counter and redrawing the beams.
 */
void antenna_debug_data_set_object(int object_handle, int datum_handle)
{
  void *rec;
  void *tag_def;

  object_get_and_verify_type(object_handle, -1);
  rec = datum_get(g_antenna_data, datum_handle);
  tag_def = tag_get(0x616e7421, *(int *)((char *)rec + 8));

  if (*((char *)rec + 5) == 0) {
    *(int *)((char *)rec + 0xc) = object_handle;

    if (*(int16_t *)((char *)rec + 6) > 5) {
      antenna_debug_data_simulate_rope(rec, tag_def, 0.05f);
      antenna_debug_data_simulate_rope(rec, tag_def, 0.05f);
      antenna_debug_data_simulate_rope(rec, tag_def, 0.05f);
    }

    *(int16_t *)((char *)rec + 6) = 0;
    antenna_debug_data_draw_beams(tag_def, rec);
  }
}

/*
 * antenna_debug_data_update_all (0x131790) -- per-tick driver for every
 * live antenna debug-data record.
 *
 * Walks g_antenna_data via data_next_index. For each active record
 * (rec[5]==0) with an attached object (rec+0xc != -1) whose tick counter
 * (rec+6) hasn't yet hit the 5-tick threshold that
 * antenna_debug_data_set_object catches up, bumps the counter and runs one
 * rope-simulation step, clamping delta_time to 1/15s (0x3d888889) when the
 * caller's delta_time exceeds it -- a frame-hitch guard so the chain solve
 * doesn't blow up on a slow tick.
 */
void antenna_debug_data_update_all(float delta_time)
{
  int index;
  float clamped_dt;

  for (index = data_next_index(g_antenna_data, -1); index != -1;
       index = data_next_index(g_antenna_data, index)) {
    void *rec;
    void *tag_def;

    rec = datum_get(g_antenna_data, index);
    tag_def = tag_get(0x616e7421, *(int *)((char *)rec + 8));

    if (*((char *)rec + 5) == 0) {
      *(int16_t *)((char *)rec + 6) = *(int16_t *)((char *)rec + 6) + 1;

      if (*(int *)((char *)rec + 0xc) != -1 &&
          *(int16_t *)((char *)rec + 6) < 5) {
        clamped_dt = (0.06666667f >= delta_time) ? delta_time : 0.06666667f;
        antenna_debug_data_simulate_rope(rec, tag_def, clamped_dt);
      }
    }
  }
}

/*
 * FUN_00131840 (0x131840) -- validate a flag coordinate and return its
 * 24-byte cell at flag+0x1c+(x*definition->height+y)*24.
 * Definition offsets +0xc/+0xe are binary-observed int16_t width/height.
 */
void *FUN_00131840(void *flag, void *definition, int16_t x, int16_t y)
{
  int index;

  if (flag == NULL || definition == NULL) {
    display_assert("flag && definition",
                   "c:\\halo\\SOURCE\\objects\\widgets\\flags.c", 0x60, true);
    system_exit(-1);
  }

  if (x < 0 || x >= *(int16_t *)((char *)definition + 0xc)) {
    display_assert("x>=0 && x<definition->width",
                   "c:\\halo\\SOURCE\\objects\\widgets\\flags.c", 0x61, true);
    system_exit(-1);
  }

  if (y < 0 || y >= *(int16_t *)((char *)definition + 0xe)) {
    display_assert("y>=0 && y<definition->height",
                   "c:\\halo\\SOURCE\\objects\\widgets\\flags.c", 0x62, true);
    system_exit(-1);
  }

  index = (int)x * (int)*(int16_t *)((char *)definition + 0xe) + (int)y;
  return (char *)flag + index * 24 + 0x1c;
}

/*
 * telnet_console_print (0x1318f0) -- validate an interior flag coordinate
 * and return its int16_t cell at flag+0x1534+(x*(height-1)+y)*2.
 * Definition offsets +0xc/+0xe are binary-observed signed int16_t values.
 */
int16_t *telnet_console_print(void *flag, void *definition, int16_t x,
                              int16_t y)
{
  int index;

  if (flag == NULL || definition == NULL) {
    display_assert("flag && definition",
                   "c:\\halo\\SOURCE\\objects\\widgets\\flags.c", 0x6d, true);
    system_exit(-1);
  }

  if (x < 0 || x >= *(int16_t *)((char *)definition + 0xc) - 1) {
    display_assert("x>=0 && x<definition->width-1",
                   "c:\\halo\\SOURCE\\objects\\widgets\\flags.c", 0x6e, true);
    system_exit(-1);
  }

  if (y < 0 || y >= *(int16_t *)((char *)definition + 0xe) - 1) {
    display_assert("y>=0 && y<definition->height-1",
                   "c:\\halo\\SOURCE\\objects\\widgets\\flags.c", 0x6f, true);
    system_exit(-1);
  }

  index = (int)x * ((int)*(int16_t *)((char *)definition + 0xe) - 1) + (int)y;
  return (int16_t *)((char *)flag + index * 2 + 0x1534);
}

/*
 * FUN_001319b0 (0x1319b0) -- allocate the flag globals data pool.
 * The allocation failure path preserves the failed EAX value as error's
 * first argument; it is necessarily zero after the TEST/JNZ guard.
 */
void FUN_001319b0(void)
{
  *(data_t **)0x5a90d0 = game_state_data_new("flag", 2, 0x16bc);
  if (*(data_t **)0x5a90d0 == NULL) {
    error(0, "couldn't allocate flag globals");
  }
}

/*
 * FUN_001319e0 (0x1319e0) -- delete every flag globals data-pool element.
 */
void FUN_001319e0(void)
{
  data_delete_all(*(data_t **)0x5a90d0);
}

/*
 * FUN_001319f0 (0x1319f0) -- mark every flag globals datum invalid.
 */
void FUN_001319f0(void)
{
  data_make_invalid(*(data_t **)0x5a90d0);
}

/*
 * FUN_00131a00 (0x131a00) -- clear the flag globals data-pool pointer.
 */
void FUN_00131a00(void)
{
  if (*(data_t **)0x5a90d0 != NULL) {
    *(data_t **)0x5a90d0 = NULL;
  }
}

/* FUN_00131b40 (0x131b40) -- delete an antenna debug-data datum. */
void FUN_00131b40(int datum_handle)
{
  datum_delete(g_antenna_data, datum_handle);
}

/*
 * FUN_00131e00 (0x131e00) -- walk the flag definition's tag_block at +0x54
 * (0x34-byte elements) and, for each element, split its run length across the
 * remaining row extent at +0xe into two halves, emitting one FUN_00131a20 pass
 * per half (modes 4 and 5).  Runs are rounded down to an even length
 * (AND 0xfffffffe at 0x131e6e) before halving.  Offsets +0x08, +0x0e and +0x54
 * are disasm-observed; nothing else about the definition is touched.
 */
void FUN_00131e00(void *definition, void *flag)
{
  int row;
  int index;
  int run;
  int half;
  int16_t count;
  int16_t *element;

  row = 0;
  index = 0;
  if (*(int16_t *)((char *)definition + 8) == 0) {
    return;
  }
  if (*(int *)((char *)definition + 0x54) <= 0) {
    return;
  }

  do {
    if ((int)(int16_t)row >= (int)*(int16_t *)((char *)definition + 0xe)) {
      return;
    }

    element = (int16_t *)tag_block_get_element((char *)definition + 0x54,
                                               (int)(int16_t)index, 0x34);
    count = *element;
    if (count < 0) {
      run = 0;
    } else {
      run = (int)*(int16_t *)((char *)definition + 0xe) - (int)(int16_t)row;
      if ((int)count <= run) {
        run = (int)count;
      }
    }

    run &= ~1;
    half = run >> 1;
    FUN_00131a20(definition, flag, 0, row, half, 4);
    FUN_00131a20(definition, flag, 0, half + row, half, 5);

    row += run;
    index++;
  } while ((int)(int16_t)index < *(int *)((char *)definition + 0x54));
}

/* FUN_00131ed0 (0x131ed0) -- initialize edge flag cells. */
void FUN_00131ed0(void *definition, void *flag)
{
  int16_t type;
  int16_t extent;
  int index;

  type = *(int16_t *)((char *)definition + 4);
  if (type == 0) {
    return;
  }

  if (type == 3 || type == 4) {
    extent = *(int16_t *)((char *)definition + 0xe) - 1;
  } else {
    extent = *(int16_t *)((char *)definition + 0xe) >> 1;
  }

  index = (int)*(int16_t *)((char *)definition + 6) +
          (int)*(int16_t *)((char *)definition + 0xc) - (int)extent - 1;
  if (index < 0) {
    index = 0;
  }

  if (type == 3) {
    FUN_00131a20(definition, flag, index, 0, extent, 3);
    return;
  }
  if (type == 4) {
    FUN_00131a20(definition, flag, index, 0, extent, 2);
    return;
  }
  if (type == 1) {
    FUN_00131a20(definition, flag, index, 0, extent, 2);
    FUN_00131a20(definition, flag, index, extent, extent, 3);
    return;
  }
  if (type == 2) {
    FUN_00131a20(definition, flag, index, 0, extent, 3);
    FUN_00131a20(definition, flag, index, extent, extent, 2);
  }
}

/*
 * FUN_00132ca0 (0x132ca0) -- create and initialize a flag globals datum for
 * a scenario flag definition. Definition offsets +0xc/+0xe/+0x50 and datum
 * offsets +0x02..+0x18 are binary-observed.
 */
void FUN_00132ca0(int flag_definition_index)
{
  void *definition;
  void *flag;
  void *cell;
  int datum_handle;
  int x;
  int y;
  int16_t *interior_cell;

  global_scenario_get();
  if (flag_definition_index == -1) {
    return;
  }

  definition = tag_get(0x666c6167, flag_definition_index);
  datum_handle = data_new_at_index(*(data_t **)0x5a90d0);
  if (datum_handle == -1) {
    return;
  }

  flag = datum_get(*(data_t **)0x5a90d0, datum_handle);
  if ((int)*(int16_t *)((char *)definition + 0xe) *
          (int)*(int16_t *)((char *)definition + 0xc) >=
        0xe1 ||
      *(int16_t *)((char *)definition + 0xc) >= 0x28 ||
      *(int *)((char *)definition + 0x50) == -1) {
    *((char *)flag + 2) = 1;
    return;
  }

  *((char *)flag + 2) = 0;
  *((char *)flag + 3) = 0;
  *(int *)((char *)flag + 8) = -1;
  *(int *)((char *)flag + 0xc) = flag_definition_index;
  *(int *)((char *)flag + 0x10) = 0;
  *(int *)((char *)flag + 0x14) = 0;
  *(int *)((char *)flag + 0x18) = 0;

  for (x = 0; x < *(int16_t *)((char *)definition + 0xc); x++) {
    for (y = 0; y < *(int16_t *)((char *)definition + 0xe); y++) {
      cell = FUN_00131840(flag, definition, (int16_t)x, (int16_t)y);
      *(int *)cell = *(int *)*(void **)0x31fc1c;
      *(int *)((char *)cell + 4) = *(int *)((char *)*(void **)0x31fc1c + 4);
      *(int *)((char *)cell + 8) = *(int *)((char *)*(void **)0x31fc1c + 8);
      *(int *)((char *)cell + 0xc) = *(int *)*(void **)0x31fc38;
      *(int *)((char *)cell + 0x10) = *(int *)((char *)*(void **)0x31fc38 + 4);
      *(int *)((char *)cell + 0x14) = *(int *)((char *)*(void **)0x31fc38 + 8);

      if (x < *(int16_t *)((char *)definition + 0xc) - 1 &&
          y < *(int16_t *)((char *)definition + 0xe) - 1) {
        interior_cell =
          telnet_console_print(flag, definition, (int16_t)x, (int16_t)y);
        *interior_cell = 0;
      }
    }
  }

  FUN_00131e00(definition, flag);
  FUN_00131ed0(definition, flag);
}

/* FUN_00132e20 (0x132e20) -- update one flag datum and render it when valid. */
void FUN_00132e20(int object_handle, int flag_datum_handle, int param_3,
                  int param_4)
{
  void *flag;
  void *definition;

  object_get_and_verify_type(object_handle, -1);
  flag = datum_get(*(data_t **)0x5a90d0, flag_datum_handle);
  definition = tag_get(0x666c6167, *(int *)((char *)flag + 0xc));
  *(int *)((char *)flag + 8) = object_handle;

  if (*(int16_t *)((char *)flag + 6) > 5 || *((char *)flag + 3) == 0) {
    FUN_00131fc0(flag, definition, 5.0f);
    *((char *)flag + 3) = 1;
  }

  *(int16_t *)((char *)flag + 6) = 0;
  if (*((char *)flag + 2) == 0) {
    flag_render_proper(flag, definition, param_3, param_4);
  }
}

/* FUN_00132ea0 (0x132ea0) -- per-tick update over every live flag datum.
 * Each iteration fetches the flag's "flag" definition tag, bumps the idle
 * counter at +0x6, and simulates the cloth (FUN_00131fc0) while the flag is
 * still attached to an object (+0x8 != NONE), the counter is under 5, and the
 * elapsed time is non-zero.  The compare at 0x132efb is FCOMP against the
 * 0.0f constant at 0x2533c0 with TEST AH,0x44 / JNP, i.e. skip when equal. */
void FUN_00132ea0(float dt)
{
  int datum_handle;
  void *flag;
  void *definition;

  for (datum_handle = data_next_index(*(data_t **)0x5a90d0, -1);
       datum_handle != -1;
       datum_handle = data_next_index(*(data_t **)0x5a90d0, datum_handle)) {
    flag = datum_get(*(data_t **)0x5a90d0, datum_handle);
    definition = tag_get(0x666c6167, *(int *)((char *)flag + 0xc));
    *(int16_t *)((char *)flag + 6) =
      (int16_t)(*(int16_t *)((char *)flag + 6) + 1);

    if (*(int *)((char *)flag + 8) != -1 &&
        *(int16_t *)((char *)flag + 6) < 5 && dt != *(float *)0x2533c0) {
      FUN_00131fc0(flag, definition, dt);
    }
  }
}
