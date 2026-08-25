/* ai_communication.c — AI communication dialogue/reply subsystem lifecycle.
 *
 * Corresponds to addresses 0x42a30–0x42ca0 in the XBE.
 * Source path confirmed via __FILE__ string:
 *   c:\halo\SOURCE\ai\ai_communication.c
 *
 * Subsystem roles:
 *   ai_communication_initialize             (0x42a30) — allocate comm tables
 *                                                        and conversation data
 *   ai_communication_dispose                (0x42b80) — no-op stub
 *   ai_communication_initialize_for_new_map (0x42b90) — reset comm state for
 *                                                        a new map load
 *   ai_communication_dispose_from_old_map   (0x42ca0) — invalidate
 *                                                        conversation data
 *
 * Key globals (all raw addresses — no named headers exist yet):
 *   0x331f08  int16_t: count of comm dialogue entries (stride 0x28)
 *   0x331f0c  void *:  allocated comm dialogue status table
 *                      (DAT_00331f08 * 2 entries, each 8 bytes)
 *   0x331f10  int16_t: count of comm reply entries (stride 0x24)
 *   0x331f14  void *:  allocated comm reply status table
 *                      (DAT_00331f10 * 2 entries, each 8 bytes)
 *   0x6324ec  data_t *: "ai conversation" data table
 *   0x632574  void *:  AI globals block (shared with ai.c)
 *
 * Static tables (read-only data):
 *   0x257e48  comm dialogue table; each entry is 0x28 bytes; sentinel = -1
 *             at entry[0] (a short).
 *   0x258eb0  comm reply table; each entry is 0x24 bytes; sentinel = -1
 *             at entry[0] (a short).
 *   0x632500  int16_t[0x39]: index map built during initialize
 */

/* ai_communication_initialize: count comm dialogue/reply table entries,
 * allocate per-entry status tables via game_state_malloc, build a dialogue
 * index map into 0x632500[], and allocate the "ai conversation" data table.
 *
 * Confirmed: __FILE__ = "c:\halo\SOURCE\ai\ai_communication.c"
 *   line 0x286 (646) -> dialogue alloc assert
 *   line 0x293 (659) -> reply alloc assert
 *   line 0x2a8 (680) -> conversation data assert
 * Called from ai_initialize (0x3f670). */
void ai_communication_initialize(void)
{
  int16_t i;
  int16_t count;

  /* --- count comm dialogue entries (stride 0x28, sentinel = -1 at [0]) */
  count = 0;
  {
    int16_t *p = (int16_t *)0x257e48;
    do {
      p += 0x14; /* advance by 0x28 bytes (stride = 0x28) */
      count++;
    } while (*p != -1);
  }
  *(int16_t *)0x331f08 = count;

  /* allocate dialogue status table if not already present */
  if (*(void **)0x331f0c == 0) {
    *(void **)0x331f0c =
      game_state_malloc("ai communication dialogue", 0, (int)count << 4);
    if (*(void **)0x331f0c == 0) {
      display_assert("ai_communication_initialize: unable to allocate comm "
                     "dialogue status table",
                     "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x286, 1);
      system_exit(-1);
    }
  }

  /* --- count comm reply entries (stride 0x24, sentinel = -1 at [0]) */
  count = 0;
  {
    int16_t *p = (int16_t *)0x258eb0;
    do {
      p += 0x12; /* advance by 0x24 bytes (stride = 0x24) */
      count++;
    } while (*p != -1);
  }
  *(int16_t *)0x331f10 = count;

  /* allocate reply status table if not already present */
  if (*(void **)0x331f14 == 0) {
    *(void **)0x331f14 =
      game_state_malloc("ai communication replies", 0, (int)count << 4);
    if (*(void **)0x331f14 == 0) {
      display_assert("ai_communication_initialize: unable to allocate comm "
                     "reply status table",
                     "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x293, 1);
      system_exit(-1);
    }
  }

  /* --- build dialogue index map into 0x632500[0..0x38].
   * For each slot i (0..0x38), walk the dialogue table and store the
   * sequential index of the entry whose sentinel-short equals i, or -1
   * if not found. Confirmed: CMP DI,0x39 / JL loop in disassembly. */
  {
    int16_t *out = (int16_t *)0x632500;
    for (i = 0; i < 0x39; i++, out++) {
      int16_t j = 0;
      int16_t *entry = (int16_t *)0x257e48;
      int16_t cur_sentinel;
      *out = -1;
      cur_sentinel = 0;
      do {
        if (cur_sentinel == i) {
          *out = j;
          break;
        }
        cur_sentinel = entry[0x14]; /* next sentinel at stride offset */
        entry += 0x14;
        j++;
      } while (cur_sentinel != -1);
    }
  }

  /* allocate "ai conversation" data table: max 8 entries, each 100 bytes.
   * Confirmed: PUSH 0x64; PUSH 0x8; PUSH name ->
   * game_state_data_new(name,8,100) */
  *(void **)0x6324ec = game_state_data_new("ai conversation", 8, 100);
  if (*(void **)0x6324ec == 0) {
    display_assert("conversation_data",
                   "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x2a8, 1);
    system_exit(-1);
  }
}

/* ai_communication_dispose: no-op stub.
 * Called from ai_dispose (0x3f6f0). Binary is a single RET instruction. */
void ai_communication_dispose(void)
{
}

/* ai_communication_initialize_for_new_map: reset communication state for a
 * new map load.
 *
 * Confirmed via caller: ai_initialize_for_new_map (0x41090).
 * Sets the communication-active flag at AI globals +0x10, zeroes the three
 * 8-byte slots at +0x14/+0x1c/+0x24, clears both dialogue and reply status
 * tables (each entry is 8 bytes: two uint32_t fields both set to 0xffffffff),
 * clears the conversation counter shorts at +0x2c/+0x2e, zeroes the 256-byte
 * conversation scratch buffer at +0x30, and calls data_delete_all on the
 * conversation data table.
 *
 * Store-offset table (offsets into AI globals block via DAT_00632574):
 *   +0x10  <- 1 (byte, communication-active flag)
 *   +0x14  <- csmemset 0, 8 bytes
 *   +0x1c  <- csmemset 0, 8 bytes
 *   +0x24  <- csmemset 0, 8 bytes
 *   dialogue table[i*8+0] <- 0xffffffff (uint32_t)
 *   dialogue table[i*8+4] <- 0xffffffff (uint32_t)
 *   reply table[i*8+0]    <- 0xffffffff (uint32_t)
 *   reply table[i*8+4]    <- 0xffffffff (uint32_t)
 *   +0x2c  <- 0 (int16_t)
 *   +0x2e  <- 0 (int16_t)
 *   +0x30  <- csmemset 0, 0x100 bytes */
void ai_communication_initialize_for_new_map(void)
{
  int n;
  int i;

  *(uint8_t *)(*(uintptr_t *)0x632574 + 0x10) = 1;
  csmemset((void *)(*(uintptr_t *)0x632574 + 0x14), 0, 8);
  csmemset((void *)(*(uintptr_t *)0x632574 + 0x1c), 0, 8);
  csmemset((void *)(*(uintptr_t *)0x632574 + 0x24), 0, 8);

  n = (int)(*(int16_t *)0x331f08) << 1;
  i = 0;
  if (n > 0) {
    do {
      *(unsigned int *)(*(char **)0x331f0c + i * 8 + 4) = ~0u;
      *(unsigned int *)(*(char **)0x331f0c + i * 8) = ~0u;
      i = (int16_t)(i + 1);
    } while (i < (int)(*(int16_t *)0x331f08) << 1);
  }

  n = (int)(*(int16_t *)0x331f10) << 1;
  i = 0;
  if (n > 0) {
    do {
      *(unsigned int *)(*(char **)0x331f14 + i * 8 + 4) = ~0u;
      *(unsigned int *)(*(char **)0x331f14 + i * 8) = ~0u;
      i = (int16_t)(i + 1);
    } while (i < (int)(*(int16_t *)0x331f10) << 1);
  }

  *(int16_t *)(*(char **)0x632574 + 0x2c) = 0;
  *(int16_t *)(*(char **)0x632574 + 0x2e) = 0;
  csmemset((void *)(*(char **)0x632574 + 0x30), 0, 0x100);

  data_delete_all(*(void **)0x6324ec);
}

/* ai_communication_dispose_from_old_map: invalidate the conversation data
 * table when leaving a map.
 *
 * Confirmed via callers: ai_dispose_from_old_map (0x3f720) and
 * ai_handle_editing (0x41e80). Binary: MOV EAX,[0x6324ec]; PUSH EAX;
 * CALL data_make_invalid; POP ECX; RET. */
void ai_communication_dispose_from_old_map(void)
{
  data_make_invalid(*(void **)0x6324ec);
}

/* ai_communication_get_type_by_name (0x42ce0): search the 57-entry
 * communication-type name table at 0x2c8d78 and return the matching index.
 *
 * Confirmed from disassembly: EBX starts at -1, ESI starts at 0, and the loop
 * visits entries 0 through 0x38.  Each csstrcmp result is tested; a zero
 * result stores the current index, so a later duplicate would replace an
 * earlier match.  The low 16 bits of EBX are returned.  A missing match
 * therefore returns -1. */
int16_t ai_communication_get_type_by_name(const char *name)
{
  int16_t result;
  int16_t index;
  const char **type_names;

  result = -1;
  index = 0;
  type_names = (const char **)0x2c8d78;
  do {
    if (csstrcmp(*type_names, name) == 0)
      result = index;
    index = (int16_t)(index + 1);
    type_names++;
  } while (index < 0x39);

  return result;
}

/* ai_communication_packet_new (0x42d20): initialize 0x20-byte packet. */
void ai_communication_packet_new(void *packet)
{
  if (packet == NULL) {
    display_assert("information", "c:\\halo\\SOURCE\\ai\\ai_communication.c",
                   0x300, 1);
    system_exit(-1);
  }

  csmemset(packet, 0, 0x20);
  *(int32_t *)packet = -1;
  *(int16_t *)((char *)packet + 4) = -1;
  *(int16_t *)((char *)packet + 6) = -1;
  *(int16_t *)((char *)packet + 8) = -1;
}

/* FUN_00042d80 (0x42d80): cdecl with three 32-bit stack arguments.
 * The binary reads [EBP+0x8] and [EBP+0x10]; [EBP+0xc] is not read.
 * Return value is a boolean in AL. */
bool FUN_00042d80(int param_1, int param_2, int param_3)
{
  int prop_handle;
  int16_t prop_state;
  char *prop;
  bool result;

  result = false;

  if (param_3 != -1) {
    prop_handle = FUN_00064b40(param_3, param_1, 1, 1);
    if (prop_handle != -1) {
      prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
      if (*(float *)(prop + 0x11c) < *(float *)0x254cc4) {
        prop_state = *(int16_t *)(prop + 0x38);
        if (prop_state == 0 || prop_state == 1)
          result = true;
      }
    }
  }

  return result;
}

/* FUN_00042df0 (0x42df0): cdecl with three 32-bit stack arguments.
 * The binary reads [EBP+0x8] and [EBP+0x10]; [EBP+0xc] is not read.
 * Return value is a boolean in AL.  The prop field at +0x38 remains
 * mechanically named because no prop structure is modelled in types.h. */
bool FUN_00042df0(int param_1, int param_2, int param_3)
{
  int prop_handle;
  int16_t prop_field_38;
  char *prop;
  bool result;

  result = false;

  if (param_3 != -1) {
    prop_handle = FUN_00064b40(param_3, param_1, 1, 1);
    if (prop_handle != -1) {
      prop = (char *)datum_get(*(data_t **)0x5ab23c, prop_handle);
      if (!(*(float *)(prop + 0x11c) > *(float *)0x254cc4)) {
        prop_field_38 = *(int16_t *)(prop + 0x38);
        if (prop_field_38 != 0 && prop_field_38 != 1)
          result = true;
      } else {
        result = true;
      }
    }
  }

  return result;
}

/* FUN_00042e60 (0x42e60): cdecl predicate with three 32-bit stack
 * arguments.  The binary reads only the third argument at [EBP+0x10]; the
 * first two slots remain unused.  It resolves that argument through
 * actor_data, then accepts actor state 7 or state 5 with the signed word at
 * actor+0xa4 equal to 1.  The +0xa4 access stays raw: types.h currently models
 * that region as byte fields, while this function proves a word comparison. */
bool FUN_00042e60(int param_1, int param_2, int param_3)
{
  char *actor;
  int16_t action;
  bool result;

  result = false;
  if (param_3 != -1) {
    actor = (char *)datum_get(actor_data, param_3);
    action = *(int16_t *)(actor + 0x6c);
    switch (action) {
    case 5:
      result = *(int16_t *)(actor + 0xa4) == 1;
      break;
    case 7:
      result = true;
      break;
    }
  }
  return result;
}

/* FUN_00042eb0 (0x42eb0): cdecl predicate with three 32-bit stack
 * arguments.  The second argument is forwarded to FUN_00042d80 but is not
 * read by that callee's current binary body.  On its true path, this function
 * resolves a type-3 object from param_1, then compares actor fields at +0x34
 * (dword) and +0x3c (signed word) for the object's actor and param_3. */
bool FUN_00042eb0(int param_1, int param_2, int param_3)
{
  unit_data_t *unit;
  actor_t *actor_a;
  actor_t *actor_b;

  if (FUN_00042d80(param_1, param_2, param_3)) {
    unit = (unit_data_t *)object_get_and_verify_type(param_1, 3);
    if (unit->actor_index.value != -1 && param_3 != -1) {
      actor_a = (actor_t *)datum_get(actor_data, unit->actor_index.value);
      actor_b = (actor_t *)datum_get(actor_data, param_3);
      if (actor_a->field_034 != (uint32_t)-1 &&
          actor_a->field_034 == actor_b->field_034 &&
          actor_a->field_03c == actor_b->field_03c) {
        return true;
      }
      return false;
    }
  }
  return false;
}

/* FUN_00042f60 (0x42f60): cdecl predicate with three 32-bit stack
 * arguments.  The binary reads [EBP+0x8], [EBP+0xc], and [EBP+0x10],
 * forwards all three to FUN_00042d80, and returns a byte in AL.  When
 * FUN_00042d80 returns nonzero, the third argument is passed to
 * actor_is_fighting; the result is 1 only when both calls return nonzero. */
char FUN_00042f60(int param_1, int param_2, int param_3)
{
  char result;

  result = 0;
  if (FUN_00042d80(param_1, param_2, param_3)) {
    if (actor_is_fighting(param_3))
      result = 1;
  }
  return result;
}

/* FUN_00042fa0 (0x42fa0): cdecl with three 32-bit stack arguments and a
 * boolean result in AL.  The call at 0x42fb4 pushes param_3, param_2, and
 * param_1 in that order before calling FUN_00042d80; the decompiler omitted
 * these arguments from its draft signature.
 *
 * Confirmed from disassembly: after the helper succeeds, the type-3 object
 * at param_1 supplies its unit actor handle at +0x1a4.  The two actor records
 * are resolved through actor_data, and their +0x270 handles are resolved
 * through prop_data.  The final result is SETZ AL after comparing the dwords
 * at +0x18 in those two prop records.  The +0x18 prop access remains raw:
 * types.h has no recovered prop structure for this record. */
bool FUN_00042fa0(int param_1, int param_2, int param_3)
{
  unit_data_t *unit;
  actor_t *actor_a;
  actor_t *actor_b;
  char *prop_a;
  char *prop_b;

  if (FUN_00042d80(param_1, param_2, param_3)) {
    unit = (unit_data_t *)object_get_and_verify_type(param_1, 3);
    if (unit->actor_index.value != -1 && param_3 != -1) {
      actor_a = (actor_t *)datum_get(actor_data, unit->actor_index.value);
      actor_b = (actor_t *)datum_get(actor_data, param_3);
      if (actor_a->target_target_prop_index != -1 &&
          actor_b->target_target_prop_index != -1) {
        prop_a =
          (char *)datum_get(prop_data, actor_a->target_target_prop_index);
        prop_b =
          (char *)datum_get(prop_data, actor_b->target_target_prop_index);
        return *(int32_t *)(prop_a + 0x18) == *(int32_t *)(prop_b + 0x18);
      }
    }
  }
  return false;
}

/* FUN_00043090 (0x43090): cdecl predicate with three 32-bit stack
 * arguments.  The binary uses only param_3 at [EBP+0x10].  It calls
 * actor_is_fighting(param_3); when that succeeds, datum_get(actor_data,
 * param_3) is retained because its returned record is read at +0x04.  The
 * predicate returns 1 only when that signed word is zero. */
char FUN_00043090(int param_1, int param_2, int param_3)
{
  actor_t *actor;
  char result;

  result = 0;
  if (actor_is_fighting(param_3)) {
    actor = (actor_t *)datum_get(actor_data, param_3);
    if (actor->field_004 == 0)
      result = 1;
  }
  return result;
}

/* ai_conversation_line (0x434c0) — look up the current line index of the
 * conversation whose index field (+0x2) matches param_1. Returns the first
 * match's 16-bit field at +0x48; returns 999 (NONE) when no conversation
 * matches or the conversation list is empty.
 *
 * Confirmed (disasm 0x434c0-0x43519):
 *   - Frame PUSH EBP; MOV EBP,ESP; SUB ESP,0x10 — the single 0x10 local is the
 *     data_iter_t at EBP-0x10; ESI/EDI pushed after the SUB.
 *   - EDI = 0x3e7 loaded at 0x434d2 (before the first CALL) and read only at
 *     the not-found exit (MOV AX,DI at 0x43506): a callee-saved register-
 *     allocated local holding the NONE result across the loop, hence `line`.
 *     The load is 32-bit (MOV EDI,0x3e7) and the return truncates (MOV AX,DI),
 *     so the local is int-width and narrowed at the return, not a short.
 *   - data_iterator_new(&iter, *(data_t **)0x6324ec) — the global is loaded by
 *     value (MOV EAX,[0x6324ec]; PUSH EAX), then LEA ECX,[EBP-0x10]; PUSH ECX,
 *     so the stack order is (iter, data).
 *   - param read once, hoisted out of the loop (MOV SI,word[EBP+8] at
 *     0x434ec) — 16-bit, so the parameter is a short, not an int.
 *   - Loop back-edge (JNZ -> 0x434f0) targets the CMP word[EAX+2],SI, i.e. the
 *     rotated form of a top-tested while loop: the initial TEST/JE at 0x434ea
 *     and the loop-exhausted fallthrough converge on ONE not-found exit at
 *     0x43506, so the 999 return is NOT duplicated (unlike the neighbour
 *     ai_conversation_advance, which early-returns before a do/while).
 *   - Both fields are word ptr reads: +0x2 index, +0x48 returned line.
 *   - Two RETs, no shared epilogue: found at 0x4350f (MOV AX,word[EAX+0x48]),
 *     not-found at 0x43506 (MOV AX,DI). Return is 16-bit in AX. */
int16_t ai_conversation_line(int16_t param_1)
{
  data_iter_t iter;
  char *conversation;
  int line;

  line = 999;
  data_iterator_new(&iter, *(data_t **)0x6324ec);
  while ((conversation = (char *)data_iterator_next(&iter)) != 0) {
    if (*(int16_t *)(conversation + 2) == param_1) {
      return *(int16_t *)(conversation + 0x48);
    }
  }
  return (int16_t)line;
}

/* ai_conversation_advance (0x43520) — iterate all conversations and mark
 * matching entries as advanced. For each conversation whose index field
 * (+0x2) matches param_1, sets byte +0x9 to 1. When the AI debug flag
 * at 0x5aca5f is set, logs the advance via console_printf with the
 * conversation name from the scenario tag block at offset 0x468. */
void ai_conversation_advance(short param_1)
{
  data_iter_t iter;
  char *conversation;

  data_iterator_new(&iter, *(data_t **)0x6324ec);
  conversation = (char *)data_iterator_next(&iter);
  if (conversation == 0) {
    return;
  }
  do {
    if (*(short *)(conversation + 2) == param_1) {
      if (*(char *)0x5aca5f != '\0') {
        console_printf(
          0, "%s: told to advance by scripting",
          tag_block_get_element((char *)global_scenario_get() + 0x468, 0x74,
                                (int)param_1));
      }
      conversation[9] = 1;
    }
    conversation = (char *)data_iterator_next(&iter);
  } while (conversation != 0);
}

/* actor_communication_update (0x43db0) — per-tick idle/ambient speech tick for
 * one actor.  While the actor is at least state 2 and the AI globals' speech
 * enable byte is set, it (re)arms the countdown whenever the actor's cached
 * "fighting" flag went stale, decrements the countdown, and on the tick it
 * reaches zero asks the unit for a matching communication and dispatches it.
 *
 * Confirmed (disasm 0x43db0-0x43e9d):
 *   - Frame PUSH EBP; MOV EBP,ESP; SUB ESP,0x38 — 0x30-byte communication
 *     record at EBP-0x38..EBP-0x09, int at EBP-0x8, int at EBP-0x4.  ESI/EDI
 *     are pushed at entry, EBX only on the non-early-exit path (0x43de8),
 *     which is why the body is nested ifs rather than early returns.
 *   - datum_get(actor_data, handle): MOV EAX,[0x6325a4]; PUSH EDI(handle);
 *     PUSH EAX; ADD ESP,8 at 0x43dc9 — so the pool is the first argument.
 *   - CMP word ptr [ESI+0x6a],0x2 / JL — signed 16-bit `>= 2`.
 *   - MOV ECX,[0x632574]; MOV AL,byte ptr [ECX+0x10]; TEST AL,AL — the AI
 *     globals block is loaded by value, the gate is its byte at +0x10.
 *   - FUN_00043ce0 is called with MOV EAX,EDI at 0x43e06, i.e. the actor
 *     handle in EAX; 0x43ce9 (MOV EDI,EAX) proves the callee consumes it, so
 *     the kb.json decl carries `@<eax>`.  It is the routine that writes both
 *     +0x6cc (the cached fighting flag) and +0x6ce (the countdown).
 *   - The countdown is read once into EAX (XOR EAX,EAX; MOV AX,[ESI+0x6ce]),
 *     tested `> 0` (TEST AX,AX / JLE), decremented, stored back and tested
 *     `== 0` (DEC EAX; TEST AX,AX; MOV [ESI+0x6ce],AX; JNZ) — one load, hence
 *     the `> 0 && --field == 0` form rather than three separate reads.
 *   - EBP-0x4 is written with a full dword (MOV dword ptr [EBP-0x4],EDX after
 *     XOR EDX,EDX / SETNZ DL), so the vocalization-type local is int-width and
 *     is passed to FUN_001a68d0 through a `short *` cast; EBP-0x8 is likewise
 *     a dword -1.
 *   - FUN_001a68d0's pushes at 0x43e2d..0x43e45 are EAX(=&[EBP-0x8]),
 *     ECX(=&[EBP-0x4]), 0, 0, 1, 1, EDX(=[ESI+0x18]); cdecl, so left-to-right
 *     the arguments are (unit handle, 1, 1, 0, NULL, &type, &sound index) and
 *     ADD ESP,0x1c confirms 7 stack dwords.
 *   - Ghidra's `local_2c[32]` at EBP-0x28 is NOT an independent local: it is
 *     record + 0x10, so ai_communication_packet_new receives an interior
 *     pointer into the same 0x30-byte record (same shape as
 *     ai_debug_vocalize).
 *   - The record stores are word [EBP-0x38]=1, word [EBP-0x36]=CX and dword
 *     [EBP-0x34]=EDX, i.e. record+0x00/+0x02/+0x04; MSVC sank the constant
 *     +0x00 store below the other two, the source order is ascending.
 *   - The single ADD ESP,0x1c at 0x43e94 cleans FUN_001a6ef0's three pushes
 *     plus csmemset's three and ai_communication_packet_new's one — MSVC
 *     coalesced the cleanups, so the ARG_COUNT hazard on FUN_001a6ef0
 *     (cleanup=7 vs decl=3) is a false positive.
 *   - MOV EDI,EAX; TEST DI,DI; JLE — the communication count is a signed
 *     16-bit `> 0` test.
 * Inferred: FUN_0003b120 (returns char, +0x6cc is a byte) is the actor
 *   "is fighting" predicate; the vocalization type passed to FUN_001a68d0 is
 *   just that flag widened, and vocalization index 1 is a literal at this
 *   call site.
 * Uncertain: the meaning of the `1`/`0` byte-width literals in arguments 3
 *   and 4 of FUN_001a68d0 is not recoverable from this call site. */
void actor_communication_update(int actor_handle)
{
  char communication[0x30];
  int sound_definition_index;
  int vocalization_type;
  actor_t *actor;
  char fighting;
  short communication_count;

  actor = (actor_t *)datum_get(*(data_t **)0x6325a4, actor_handle);
  if (actor->field_06a >= 2 && *(char *)(*(char **)0x632574 + 0x10) != '\0') {
    fighting = FUN_0003b120(actor_handle);
    if (actor->field_6ce == 0 || actor->field_6cc != fighting) {
      FUN_00043ce0(actor_handle);
    }
    if (actor->field_6ce > 0 && --actor->field_6ce == 0) {
      vocalization_type = (fighting != '\0');
      sound_definition_index = -1;
      communication_count =
        FUN_001a68d0(actor->field_018, 1, 1, 0, NULL,
                     (short *)&vocalization_type, &sound_definition_index);
      if (communication_count > 0) {
        csmemset(communication, 0, 0x30);
        *(short *)(communication + 0x00) = 1;
        *(short *)(communication + 0x02) = (short)vocalization_type;
        *(int32_t *)(communication + 0x04) = sound_definition_index;
        ai_communication_packet_new(communication + 0x10);
        FUN_001a6ef0(actor->field_018, communication_count, communication);
      }
    }
  }
}

/* ai_conversation_stop (0x44500) — stop every conversation whose index field
 * at +0x02 matches param_1.  When the AI trace byte at 0x5aca5f is set, the
 * matching conversation name is resolved from the scenario block at +0x468
 * and logged before the conversation is finished.
 *
 * Confirmed by disassembly 0x44500-0x44589:
 *   - The 0x10-byte local at EBP-0x10 is data_iter_t.  data_iterator_new
 *     receives &iterator and the data pointer loaded from 0x6324ec.
 *   - The conversation index comparison is a 16-bit load at item+0x02.
 *     The parameter is loaded as a 16-bit value from [EBP+0x08].
 *   - tag_block_get_element receives scenario+0x468, (short)param_1 widened
 *     to int, and element size 0x74, in that order.
 *   - ai_conversation_finish receives iterator.datum_handle at iterator+0x08,
 *     followed by two zero-valued char arguments.
 *   - Stack cleanup is coalesced across nested calls: ADD ESP,0x0c after the
 *     first iterator-next also cleans data_iterator_new; ADD ESP,0x18 after
 *     console_printf cleans the staged tag-block and console arguments.
 */
void ai_conversation_stop(int param_1)
{
  void *item;
  data_iter_t iterator;

  data_iterator_new(&iterator, *(data_t **)0x6324ec);
  item = data_iterator_next(&iterator);
  if (item != (void *)0) {
    do {
      if (*(short *)((char *)item + 2) == (short)param_1) {
        if (*(char *)0x5aca5f != '\0') {
          item = global_scenario_get();
          item = tag_block_get_element((char *)item + 0x468,
                                       (int)(short)param_1, 0x74);
          console_printf(0, "%s: told to stop by scripting", item);
        }
        ai_conversation_finish(iterator.datum_handle, 0, 0);
      }
      item = data_iterator_next(&iterator);
    } while (item != (void *)0);
  }
}

/* ai_conversation_actor_deleted (0x44590).
 *
 * The item and tag-block element offsets below are kept raw: this lift does
 * not establish semantic field names for either record. */
void ai_conversation_actor_deleted(int actor_handle)
{
  void *item;
  void *definition;
  int16_t comparison_index;
  int item_index;
  data_iter_t iterator;

  data_iterator_new(&iterator, *(data_t **)0x6324ec);
  item = data_iterator_next(&iterator);
  while (item != (void *)0) {
    definition =
      tag_block_get_element((char *)global_scenario_get() + 0x468,
                            (int)*(int16_t *)((char *)item + 0x02), 0x74);
    comparison_index = 0;
    if (*(int *)((char *)definition + 0x50) > 0) {
      item_index = 0;
      do {
        if (*(int *)((char *)item + item_index * 4 + 0x28) == actor_handle) {
          if ((*(uint8_t *)((char *)definition + 0x20) & 1) != 0) {
            ai_conversation_finish(iterator.datum_handle, 0, 0);
            break;
          }
          *(uint32_t *)((char *)item + 0x14) &= ~(1u << item_index);
          *(int32_t *)((char *)item + item_index * 4 + 0x28) = -1;
          if (*(int16_t *)((char *)item + 0x4a) == comparison_index)
            *(uint8_t *)((char *)item + 0x63) = 1;
        }
        comparison_index = comparison_index + 1;
        item_index = (int)comparison_index;
      } while (item_index < *(int *)((char *)definition + 0x50));
    }
    item = data_iterator_next(&iterator);
  }
}
