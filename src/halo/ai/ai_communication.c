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
    for (i = 0; i <= 0x38; i++, out++) {
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
  char *g = *(char **)0x632574;
  int16_t n;
  int i;

  /* set communication-active flag */
  *(char *)(g + 0x10) = 1;

  /* zero the three 8-byte slots */
  csmemset(g + 0x14, 0, 8);
  csmemset(g + 0x1c, 0, 8);
  csmemset(g + 0x24, 0, 8);

  /* clear dialogue status table (DAT_00331f08 * 2 entries, 8 bytes each) */
  n = *(int16_t *)0x331f08;
  for (i = 0; i < (int)n * 2; i++) {
    *(unsigned int *)(*(char **)0x331f0c + i * 8 + 4) = 0xffffffff;
    *(unsigned int *)(*(char **)0x331f0c + i * 8) = 0xffffffff;
  }

  /* clear reply status table (DAT_00331f10 * 2 entries, 8 bytes each) */
  n = *(int16_t *)0x331f10;
  for (i = 0; i < (int)n * 2; i++) {
    *(unsigned int *)(*(char **)0x331f14 + i * 8 + 4) = 0xffffffff;
    *(unsigned int *)(*(char **)0x331f14 + i * 8) = 0xffffffff;
  }

  /* clear conversation state in AI globals */
  *(int16_t *)(g + 0x2c) = 0;
  *(int16_t *)(g + 0x2e) = 0;
  csmemset(g + 0x30, 0, 0x100);

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
