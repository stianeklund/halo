/* ai_communication.c — AI communication dialogue/reply subsystem lifecycle.
 *
 * Corresponds to addresses 0x42a30–0x42ce0 in the XBE.
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

#include "../../x87_math.h"
#include "../math/real_math.h"
#include "encounters.h"
/* ---------- TU-local types and globals ----------
 * Names are inferred;
 * the layouts are confirmed against 2276 code where noted. */

/* dialogue_usage_t, reply_usage_t and dialogue_event_status_t live in
 * types.h; global_dialogue_table, global_reply_table,
 * global_dialogue_event_count, global_dialogue_events, global_reply_events,
 * communication_speech_priorities, communication_notification_delays,
 * communication_protagonist_default_look_priorities and
 * global_communication_team_names (two names per team; the code always
 * reads [2 * team + 1]) are kb.json data symbols. */


























































/* ai_get_player_rating tunables: communication_player_absolute_range,
 * communication_player_ideal_range_min/_max, communication_player_ideal_fov
 * and communication_player_rating_low_priority are kb.json data symbols
 * (const reals in .rdata), as is communication_player_speaking_priorities.
 */

/* ai_globals (ai_globals_t *), the ai_print_* / ai_debug_communication_*
 * hs-global bytes and the communication bit vectors are kb.json data
 * symbols. */








/* SAR 5 / AND 0x1f / SHL / TEST dword — signed index, as in the binary. */
#define ai_debug_communication_focused(type)                                \
  ((ai_debug_communication_focus_vector[(type) >> 5] &                      \
    (1u << ((type) & 0x1f))) != 0)

/* Partial conversation layouts. Widths/offsets are from 2276
 * ai_conversation_find_participant (0x447d0); names are inferred.
 * Unobserved fields remain padding. See
 * recovery/evidence/ai_conversation_*.json. */
typedef struct {
  char pad_00[2];
  uint8_t flags; /* +0x02: TEST byte [participant+2],2 */
  char pad_03;
  int16_t selection_type; /* +0x04: MOVSX / switch */
  int16_t actor_type; /* +0x06: CMP word */
  int16_t preexisting_name_index; /* +0x08: MOV AX; NONE */
  int16_t new_name_index; /* +0x0a: debug name fallback */
  char pad_0c[0xc];
  int16_t dialogue_variants[6]; /* +0x18: six word reads */
  char pad_24[0x20];
  int32_t ai_index; /* +0x44: iterator source; NONE */
  char pad_48[0xc];
} ai_conversation_participant_t;
cs(ai_conversation_participant_t, 0x54);
co(ai_conversation_participant_t, flags, 0x02);
co(ai_conversation_participant_t, selection_type, 0x04);
co(ai_conversation_participant_t, actor_type, 0x06);
co(ai_conversation_participant_t, preexisting_name_index, 0x08);
co(ai_conversation_participant_t, new_name_index, 0x0a);
co(ai_conversation_participant_t, dialogue_variants, 0x18);
co(ai_conversation_participant_t, ai_index, 0x44);

typedef struct {
  char pad_00[0x28];
  real run_to_player_distance; /* +0x28: FCOMP 0.0 */
  char pad_2c[0x24];
  tag_block participants; /* +0x50: tag_block_get_element, stride 0x54 */
  char pad_5c[0x18];
} ai_conversation_definition_t;
cs(ai_conversation_definition_t, 0x74);
co(ai_conversation_definition_t, run_to_player_distance, 0x28);
co(ai_conversation_definition_t, participants, 0x50);

/* Only the prefix used here is recovered; complete datum size unproven here. */
typedef struct {
  char pad_00[2];
  int16_t definition_index;
  char pad_04[0x10];
  uint32_t participant_mask;
  int16_t dialogue_indices[8];
  int32_t actor_indices[8];
} ai_conversation_datum_t;
co(ai_conversation_datum_t, definition_index, 0x02);
co(ai_conversation_datum_t, participant_mask, 0x14);
co(ai_conversation_datum_t, dialogue_indices, 0x18);
co(ai_conversation_datum_t, actor_indices, 0x28);

/* conversation_data and ai_print_conversations: kb.json data symbols. */


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
  int count;

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

/* ai_communication_get_type_name (0x42cb0): bounds-checked lookup into the
 * 57-entry communication-type name table at 0x2c8d78.
 *
 * Binary: MOV CX,[EBP+8]; TEST CX,CX; MOV EAX,0x253b58 ("<error>"); JL out;
 * CMP CX,0x39; JGE out; MOVSX EAX,CX; MOV EAX,[EAX*4+0x2c8d78]; out: RET.
 * Both compares are signed (JL/JGE). */
const char *ai_communication_get_type_name(int16_t type)
{
  const char *result;

  result = (const char *)0x253b58; /* "<error>" */
  if (type >= 0 && type < 0x39) {
    result = ((const char **)0x2c8d78)[type];
  }
  return result;
}

/* ai_communication_get_type_by_name (0x42ce0): case-sensitive search of the
 * 57-entry communication-type name table at 0x2c8d78 (an array of
 * `const char *`, one per row) for `name`. Returns the matching row index,
 * or -1 if never matched.
 *
 * The table size (0x39 = 57) is independently confirmed by two callers in
 * ai_debug.c (ai_debug_communication_suppress/ignore at 0x4a650/0x4a680,
 * which pass 0x39 as this lookup's companion vector_size argument).
 *
 * Confirmed via disassembly: OR EBX,0xffffffff (result = -1); loop:
 * MOV ECX,[EDI] (table[i]); PUSH EAX([EBP+8]=name); PUSH ECX; CALL
 * csstrcmp; TEST EAX,EAX; JNZ skip; MOV EBX,ESI (result = i) — no early
 * exit on match, the loop always runs all 57 iterations, so a later
 * matching row overwrites an earlier one. INC ESI; ADD EDI,4; CMP SI,0x39;
 * JL loop. MOV AX,BX at the end truncates the 32-bit accumulator to the
 * int16_t return. */
int16_t ai_communication_get_type_by_name(const char *name)
{
  int16_t i;
  int16_t result;

  result = -1;
  for (i = 0; i < 0x39; i++) {
    if (csstrcmp(((const char **)0x2c8d78)[i], name) == 0) {
      result = i;
    }
  }
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

/* reply_filter_close (0x42d80) — bool predicate: true when the prop keyed by
 * (object_handle, actor_handle) exists, is within a threshold distance, and
 * its field at +0x38 is 0 or 1.
 *
 * Confirmed (disasm 0x42d80-0x42de1):
 *   - No SUB ESP (frame is PUSH EBP; MOV EBP,ESP; PUSH EBX only), so params
 *     are read straight off the incoming stack slots: EBP+8 = param_1
 *     (pushed as ECX, second prop_get_base_by_unit_index arg), EBP+0xC
 * (param_2) is never referenced anywhere in the function body, EBP+0x10 =
 * param_3 (pushed as EAX, first prop_get_base_by_unit_index arg). BL is zeroed
 * once up front (XOR BL,BL) and is the shared default (false) return value;
 * every early-exit branch targets 0x42ddd (MOV AL,BL), so the real return is
 * bool in AL, not the void the stale kb decl showed.
 *   - if (param_3 == -1) return false — CMP EAX,-1 / JZ before the call, so
 *     the -1 sentinel check is on the caller-supplied handle, not on
 *     prop_get_base_by_unit_index's result.
 *   - prop_get_base_by_unit_index(param_3, param_1, 1, 1): push order is
 * EAX(param_3) last = first decl arg (actor_handle), ECX(param_1) = second decl
 * arg (object_handle), then two literal 1s (create_if_missing, acknowledge). If
 * the result is -1, return false.
 *   - datum_get(prop_data, result): MOV EDX,[0x5ab23c] (prop_data, the same
 *     global documented in props.c); PUSH EAX(handle); PUSH EDX(prop_data)
 *     — cdecl right-to-left, so prop_data is arg1.
 *   - FLD [prop+0x11c]; FCOMP [0x254cc4]; FNSTSW AX; TEST AH,5; JP <false>.
 *     Same idiom documented in prop_new_unacknowledged (props.c): mask 0x05
 *     is C0|C2, and for ordered operands the fall-through (JP not taken) is
 *     `prop+0x11c < accumulator`; unordered also takes JP (false), so the
 *     surviving condition is `prop+0x11c < *(float *)0x254cc4`.
 *   - MOV AX,word[prop+0x38]; this offset is NOT the +0x24 "state" field
 *     documented in props.c's prop struct notes — it is a distinct,
 *     previously-unobserved offset (props.c's prop struct is not modelled
 *     in types.h), kept as a raw field access. Accepts 0 or 1; anything else
 *     falls through to the false return.
 *   - Success path: MOV AL,1 (0x42dd8) then POP EBX/POP EBP/RET. Failure
 *     path: MOV AL,BL (0x42ddd, BL==0) then a separate POP EBX/POP EBP/RET
 *     — two distinct epilogues, not a shared one.
 * Uncertain: no evidence for this function's or prop+0x38's semantic name,
 *   or for param_2's role (never read); kept as reply_filter_close with param_2
 *   named for its stack position only. */
bool reply_filter_close(int param_1, int param_2, int param_3)
{
  int prop_index;
  char *prop;
  char result;

  (void)param_2;

  result = 0;
  if (param_3 != -1) {
    prop_index = prop_get_base_by_unit_index(param_3, param_1, 1, 1);
    if (prop_index != -1) {
      prop = (char *)datum_get(prop_data, prop_index);
      if (*(float *)(prop + 0x11c) < *(float *)0x254cc4) {
        if (*(int16_t *)(prop + 0x38) == 0 || *(int16_t *)(prop + 0x38) == 1) {
          result = 1;
        }
      }
    }
  }
  return result;
}

/* reply_filter_not_close (0x42df0) — bool predicate over the same
 * (object_handle, actor_handle)-keyed prop lookup as sibling
 * reply_filter_close, but with the threshold comparison direction and
 * success/failure roles swapped: true when the prop's distance field is past
 * the threshold, OR its +0x38 field is neither 0 nor 1; false when the prop is
 * missing/unreachable, or the field is within the threshold AND +0x38 is 0
 * or 1.
 *
 * Confirmed (disasm 0x42df0-0x42e51):
 *   - Same frame/param shape as reply_filter_close: PUSH EBP; MOV EBP,ESP; PUSH
 * EBX only (no SUB ESP). EBP+8 = param_1 (pushed as ECX, second
 *     prop_get_base_by_unit_index arg), EBP+0xC (param_2) is never referenced
 * anywhere in the function body, EBP+0x10 = param_3 (pushed as EAX, first
 *     prop_get_base_by_unit_index arg). BL is zeroed once (XOR BL,BL) and is
 * the shared false-return value; both early-exit branches and the final failure
 *     path target 0x42e4d (MOV AL,BL).
 *   - if (param_3 == -1) return false — CMP EAX,-1 / JZ before the call, so
 *     the -1 sentinel check is on the caller-supplied handle, not on
 *     prop_get_base_by_unit_index's result.
 *   - prop_get_base_by_unit_index(param_3, param_1, 1, 1): push order is
 * EAX(param_3) last = first decl arg (actor_handle), ECX(param_1) = second decl
 * arg (object_handle), then two literal 1s (create_if_missing, acknowledge) —
 * identical call shape to reply_filter_close. If the result is -1, return
 *     false.
 *   - datum_get(prop_data, result): MOV EDX,[0x5ab23c] (prop_data, same
 *     global documented in props.c); PUSH EAX(handle); PUSH EDX(prop_data)
 *     — cdecl right-to-left, so prop_data is arg1.
 *   - FLD [prop+0x11c]; FCOMP [0x254cc4]; FNSTSW AX; TEST AH,0x41; JZ 0x42e48.
 *     Mask 0x41 is C3(0x40)|C0(0x01); FCOMP's condition-code table maps
 *     C3=0,C0=0 uniquely to ST(0) > source (C3=0,C0=1 is less-than; C3=1,C0=0
 *     is equal; C3=1,C2=1,C0=1 is unordered), so JZ (both bits clear) taken
 *     means `prop+0x11c > *(float *)0x254cc4`. Taking this branch jumps
 *     straight to the AL=1 success path, skipping the +0x38 check entirely.
 *   - Not taken (prop+0x11c <= *(float *)0x254cc4): MOV AX,word[prop+0x38]
 *     (same offset as reply_filter_close, not the +0x24 "state" field from
 *     props.c); if it is 0 or 1, return false (JZ 0x42e4d on each compare);
 *     otherwise fall through to the same AL=1 success path at 0x42e48.
 *   - Success path: MOV AL,1 (0x42e48) then POP EBX/POP EBP/RET. Failure
 *     path: MOV AL,BL (0x42e4d, BL==0) then a separate POP EBX/POP EBP/RET
 *     — two distinct epilogues, not a shared one.
 * Uncertain: no evidence for this function's or prop+0x38's semantic name,
 *   or for param_2's role (never read); kept as reply_filter_not_close with
 * param_2 named for its stack position only. */
bool reply_filter_not_close(int param_1, int param_2, int param_3)
{
  int prop_index;
  char *prop;
  char result;

  (void)param_2;

  result = 0;
  if (param_3 != -1) {
    prop_index = prop_get_base_by_unit_index(param_3, param_1, 1, 1);
    if (prop_index != -1) {
      prop = (char *)datum_get(prop_data, prop_index);
      if (*(float *)(prop + 0x11c) > *(float *)0x254cc4 ||
          (*(int16_t *)(prop + 0x38) != 0 && *(int16_t *)(prop + 0x38) != 1)) {
        result = 1;
      }
    }
  }
  return result;
}

/* reply_filter_searching (0x42e60): cdecl predicate with three 32-bit stack
 * arguments.  The binary reads only the third argument at [EBP+0x10]; the
 * first two slots remain unused.  It resolves that argument through
 * actor_data, then accepts actor state 7 or state 5 with the signed word at
 * actor+0xa4 equal to 1.  The +0xa4 access stays raw: types.h currently models
 * that region as byte fields, while this function proves a word comparison. */
bool reply_filter_searching(int param_1, int param_2, int param_3)
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

/* reply_filter_same_platoon (0x42eb0): cdecl predicate with three 32-bit stack
 * arguments.  The second argument is forwarded to reply_filter_close but is not
 * read by that callee's current binary body.  On its true path, this function
 * resolves a type-3 object from param_1, then compares actor fields at +0x34
 * (dword) and +0x3c (signed word) for the object's actor and param_3. */
bool reply_filter_same_platoon(int param_1, int param_2, int param_3)
{
  unit_data_t *unit;
  actor_t *actor_a;
  actor_t *actor_b;

  if (reply_filter_close(param_1, param_2, param_3)) {
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

/* reply_filter_fighting (0x42f40) — thin wrapper returning actor_is_fighting
 * for the actor keyed by param_3.
 *
 * Confirmed (disasm 0x42f40-0x42f50):
 *   - No SUB ESP (frame is PUSH EBP; MOV EBP,ESP only); the single param
 *     read is EBP+0x10 (param_3), loaded into EAX and pushed as the sole
 *     arg to actor_is_fighting (0x3b150, in_kb, ported). EBP+8 (param_1)
 *     and EBP+0xC (param_2) are never referenced.
 *   - CALL actor_is_fighting; ADD ESP,0x4; POP EBP; RET — nothing
 *     overwrites EAX between the call and RET, so this function's return
 *     value is actor_is_fighting's bool-in-AL result verbatim.
 *   - Same 3-int-param frame shape as sibling
 * reply_filter_close/reply_filter_not_close in this object (both PUSH EBP; MOV
 * EBP,ESP; read only EBP+0x10), kept as (param_1, param_2, param_3) for
 * consistency. Uncertain: no evidence for this function's semantic name, or for
 *   param_1/param_2's roles (never read); no callers found (xrefs empty),
 *   consistent with the siblings being reached only via an indirect table. */
bool reply_filter_fighting(int param_1, int param_2, int param_3)
{
  (void)param_1;
  (void)param_2;

  return (bool)actor_is_fighting(param_3);
}

/* reply_filter_fighting_close (0x42f60): cdecl predicate with three 32-bit
 * stack arguments.  The binary reads [EBP+0x8], [EBP+0xc], and [EBP+0x10],
 * forwards all three to reply_filter_close, and returns a byte in AL.  When
 * reply_filter_close returns nonzero, the third argument is passed to
 * actor_is_fighting; the result is 1 only when both calls return nonzero. */
char reply_filter_fighting_close(int param_1, int param_2, int param_3)
{
  char result;

  result = 0;
  if (reply_filter_close(param_1, param_2, param_3)) {
    if (actor_is_fighting(param_3))
      result = 1;
  }
  return result;
}

/* reply_filter_same_target (0x42fa0) — true when param_1's unit and param_3's
 * actor are both currently targeting (target_target_prop_index) the same
 * object. Gated by sibling predicate reply_filter_close on (param_1, param_2,
 * param_3); returns false immediately if that gate fails, or if either actor
 * lookup or target lookup is unresolved (-1).
 *
 * Confirmed (disasm 0x42fa0-0x4304e):
 *   - PUSH ESI(param_3); PUSH EAX(param_2); PUSH EDI(param_1); CALL
 *     reply_filter_close — cdecl right-to-left, so the call is
 *     reply_filter_close(param_1, param_2, param_3), same param order as this
 *     function's own signature. XOR BL,BL up front and every early-exit
 *     branch (target 0x43048) converges on MOV AL,BL — the shared false
 *     return.
 *   - object_get_and_verify_type(param_1, 3): PUSH 0x3; PUSH EDI(param_1);
 *     result+0x1a4 is read immediately after — the same "unit's actor
 *     handle" offset used throughout units.c/bipeds.c (e.g. units.c:5085,
 *     bipeds.c:748), so this is param_1's actor handle. It is NOT a field
 *     of actor_t: object_get_and_verify_type(_,3) returns a unit object,
 *     not an actor pool entry, so the +0x1a4 here is kept as a raw offset
 *     read rather than tied to actor_t's unrelated field_1a4.
 *   - CMP EAX,-1/JZ then CMP ESI,-1/JZ, both targeting 0x43048: if
 *     (actor_handle_1 == -1) return false; if (param_3 == -1) return
 *     false.
 *   - datum_get(actor_data, actor_handle_1) then datum_get(actor_data,
 *     param_3): both loads of [0x6325a4] (actor_data, the global
 *     documented in props.c) immediately precede their call; EDI holds
 *     the first result across the second call.
 *   - actor1->target_target_prop_index (+0x270, the co()-anchored field
 *     also used across actors.c/actor_perception.c/props.c) and
 *     actor2->target_target_prop_index are each checked against -1 before
 *     use; either -1 returns false.
 *   - datum_get(prop_data, actor1->target_target_prop_index) then
 *     datum_get(prop_data, actor2->target_target_prop_index): both loads
 *     of [0x5ab23c] (prop_data), EDI holds the first prop pointer across
 *     the second call — same interleave shape as the actor pair above.
 *   - Final compare is prop1+0x18 vs prop2+0x18 — the prop struct's
 *     object_handle field, established across props.c (compared against
 *     object_handle at 771/912) and ai.c/actor_perception.c/ai_script.c:
 *     SETZ AL, so the return is `prop1->object_handle ==
 *     prop2->object_handle`.
 * Uncertain: no evidence for this function's semantic name or for
 *   param_2's role (forwarded to reply_filter_close only, never read directly
 *   here); kept as reply_filter_same_target with param_2 named for its stack
 * position only, consistent with siblings
 * reply_filter_close/reply_filter_not_close. */
bool reply_filter_same_target(int param_1, int param_2, int param_3)
{
  char *unit;
  int actor_handle_1;
  actor_t *actor1;
  actor_t *actor2;
  char *prop1;
  char *prop2;

  if (!reply_filter_close(param_1, param_2, param_3)) {
    return false;
  }
  unit = (char *)object_get_and_verify_type(param_1, 3);
  actor_handle_1 = *(int *)(unit + 0x1a4);
  if (actor_handle_1 == -1 || param_3 == -1) {
    return false;
  }
  actor1 = (actor_t *)datum_get(actor_data, actor_handle_1);
  actor2 = (actor_t *)datum_get(actor_data, param_3);
  if (actor1->target_target_prop_index == -1 ||
      actor2->target_target_prop_index == -1) {
    return false;
  }
  prop1 = (char *)datum_get(prop_data, actor1->target_target_prop_index);
  prop2 = (char *)datum_get(prop_data, actor2->target_target_prop_index);
  return *(int *)(prop1 + 0x18) == *(int *)(prop2 + 0x18);
}

/* reply_filter_no_certain_target (0x43050): cdecl predicate with three 32-bit
 * stack arguments.  The binary reads only the third argument at [EBP+0x10]; the
 * first two slots remain unused (same shape as reply_filter_searching in this
 * file). It resolves that argument through actor_data, then requires
 * actor->field_06a == 3 and actor->field_06e < 4 (signed word compares).
 * Disasm 0x43050-0x43081: CMP EAX,-1/JZ -> false; datum_get(actor_data,
 * param_3); CMP word[EAX+0x6a],3/JNZ -> false; CMP word[EAX+0x6e],4/JL ->
 * true, else false. */
bool reply_filter_no_certain_target(int param_1, int param_2, int param_3)
{
  actor_t *actor;
  bool result;

  result = false;
  if (param_3 != -1) {
    actor = (actor_t *)datum_get(actor_data, param_3);
    if (actor->field_06a == 3 && actor->field_06e < 4) {
      result = true;
    }
  }
  return result;
}

/* FUN_00043090 (0x43090): cdecl predicate with three 32-bit stack
 * arguments.  The binary uses only param_3 at [EBP+0x10].  It calls
 * actor_is_fighting(param_3); when that succeeds, datum_get(actor_data,
 * param_3) is retained because its returned record is read at +0x04.  The
 * predicate returns 1 only when that signed word is zero.
 *
 * Not "flee_leader": actor_is_fighting() explicitly suppresses its own
 * true result while state_action == _actor_action_flee (actions.c), so
 * gating on actor_is_fighting()==true is the opposite of "flee". field_004
 * is independently confirmed elsewhere (actors.c, ai_communication.c) as
 * the actor's type-palette index, not the leader flag (that's field_01c).
 * No named/enum meaning ties field_004==0 to anything recoverable, so this
 * stays FUN_-named per naming-confidence doctrine (explicit unknown over
 * guess). Reviewed PR #12, reverted from disputed name; see PR review. */
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

/* ai_communication_consider_speech (0x430d0) — decide whether a unit should
 * vocalize now, and scale the caller's weight accordingly.
 *
 * ABI (disasm 0x430d0-0x43266): three register arguments plus seven cdecl
 * stack slots. ECX -> ESI = vocalization_type, EAX -> EBX =
 * sound_definition_index_reference, EDX -> EDI = priority (the assert string
 * at 0xc1a names the first two and `weight`; the callee decl of
 * unit_test_speech names unit_handle/priority). Stack: [EBP+0x08] unit_handle,
 * [EBP+0x0c] param_5, [EBP+0x10] param_6, [EBP+0x14] param_7, [EBP+0x18]
 * param_8, [EBP+0x1c] weight, [EBP+0x20] failure_reason. Returns short: both
 * exits do MOV AX,BX where BX holds the play type (kb's earlier `void(void)`
 * decl was a placeholder).
 *
 * Confirmed details:
 *   - 0x4311e..0x43114 pushes seven args (ADD ESP,0x1c) in the order
 *     (unit_handle, priority, param_7, 1, &last_speech_time,
 *      vocalization_type, sound_definition_index_reference).
 *   - 0x43147 ADD ESP,0x10 merges unit_get_speech_priority_name's one-dword
 * cleanup into crt_sprintf's three, so the name lookup stays nested in the
 * call.
 *   - 0x431a2 MOV ECX,0 / SETS CL / DEC ECX / AND ECX,EAX clamps the elapsed
 *     tick delta at zero (branchless `delta < 0 ? 0 : delta`).
 *   - 0x431b9 indexes a 0x28-byte-stride table at 0x257cd8 by param_5 and
 *     reads its leading float; FIADD of the spilled sign-extended param_6
 *     confirms the tolerance is added as an int.
 *   - 0x431d8 stores the zeroed EBX through weight, so suppressing speech
 *     also clears the returned play type and skips the trailing assert.
 *   - 0x43230 FCOMP [0x2533c0] / TEST AH,0x41 / JZ is `*weight > 0.0f`.
 * Unknown: param_5 (table selector, must be < 5), param_6 (tick tolerance),
 * param_7 (forwarded char flag), param_8 (enable flag). */
short ai_communication_consider_speech(int *sound_definition_index_reference,
                                       short *vocalization_type, short priority,
                                       int unit_handle, short param_5,
                                       short param_6, char param_7,
                                       char param_8, float *weight,
                                       char *failure_reason)
{
  short play_type;
  short elapsed;
  short threshold;
  int last_speech_time;
  int delta;
  int tolerance;

  assert_halt_at("c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xc1a,
                 vocalization_type && sound_definition_index_reference &&
                   weight);

  play_type =
    unit_test_speech(unit_handle, priority, param_7, 1, &last_speech_time,
                     vocalization_type, sound_definition_index_reference);
  if (play_type == 0) {
    if (failure_reason != 0) {
      crt_sprintf(failure_reason, "nospch-%s",
                  unit_get_speech_priority_name(priority));
    }
  } else if (play_type == 1) {
    *weight = *weight * *(float *)0x2533e4;
  }

  if ((game_connection() != 0 || *(char *)0x5aca47 == 0) && param_8 != 0 &&
      param_5 < 5 && last_speech_time != -1) {
    delta = game_time_get() - last_speech_time;
    elapsed = (short)(delta < 0 ? 0 : delta);
    tolerance = param_6;
    threshold =
      (short)(*(float *)(0x257cd8 + param_5 * 0x28) * *(float *)0x253394 +
              tolerance);
    if (elapsed <= threshold) {
      play_type = 0;
      *weight = 0.0f;
      if (failure_reason != 0) {
        crt_sprintf(failure_reason, "spk%d<tol%d+%d", (int)elapsed, tolerance,
                    (int)threshold - tolerance);
      }
      return play_type;
    }
    if ((int)elapsed < (int)threshold + 60) {
      *weight =
        *(float *)0x25634c * ((float)((int)elapsed - (int)threshold) * *weight);
    }
  }

  assert_halt_msg_at(
    "(play_type == _unit_play_speech_none) || (*weight > 0.0f)",
    "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xc49,
    play_type == 0 || *weight > *(float *)0x2533c0);
  return play_type;
}

/* actor_communication_team (0x43270) — classify an actor's communication
 * team from its actor-type definition flags. Confirmed via disasm
 * 0x43270-0x432ac: datum_get(actor_data, actor_handle) resolves the actor
 * record (no -1 guard on actor_handle, unlike reply_filter_no_certain_target);
 * the actor's field_004 (int16_t, "meaning unproven") is passed to
 * actor_type_get_race (actor_type_definitions[actor_type]->+0x4 flags word,
 * already ported in actors.c). Bit 0x2 of that flags word (TEST AL,0x2) returns
 * 0; bit 0x4 (TEST AL,0x4) returns 1; otherwise returns -1 (OR ECX,0xffffffff /
 * MOV AX,CX at 0x43292/0x432a8). Called through a `(short (*)(int))` function
 * pointer cast at encounters.c:6018 with an actor index, confirming the
 * (int actor_handle) -> int16_t signature; the prior kb.json `void(void)`
 * decl was a placeholder. Ghidra's decompile_c mis-typed this as void with
 * bare `return;` on every path — disassembly is authoritative here. */
int16_t actor_communication_team(int actor_handle)
{
  actor_t *actor;
  int16_t flags;
  int16_t result;

  actor = (actor_t *)datum_get(actor_data, actor_handle);
  flags = actor_type_get_race(actor->field_004);
  result = -1;
  if ((flags & 2) != 0) {
    return 0;
  }
  if ((flags & 4) != 0) {
    result = 1;
  }
  return result;
}

/* ai_communication_look_secondary_at_unit (0x432b0) — issue a primary "look at
 * object" request for an actor, reusing a caller-supplied prop handle when
 * valid instead of always looking one up. Called with a register-passed prop
 * handle so a caller that already resolved one (e.g. from an earlier
 * prop_get_active_by_unit_index call) can skip the redundant lookup.
 *
 * Confirmed via disasm 0x432b0-0x43352:
 *   - No incoming register store to EAX/EBX/EDI in the prologue (only
 *     `MOV ESI,EAX` at 0x432ba to preserve the value across the
 *     prop_get_active_by_unit_index call) -> EAX/EBX/EDI are @<reg>
 *     parameters, not locals.
 *   - Gate order (0x432b6-0x432df): EBX(actor_handle)==-1 -> return;
 *     word[EBP+0xc](priority)<=0 -> return; EDI(object_handle)==-1 ->
 *     return; object_try_and_get_and_verify_type(EDI,3)==NULL -> return.
 *   - EAX(prop_handle): if ==-1 (0x432e1), call
 *     prop_get_active_by_unit_index(EBX,EDI) and use its result (0x432e6);
 *     if the (possibly updated) handle ==-1 (0x432f2), skip straight to the
 *     position-look fallback at 0x43326.
 *   - datum_get(prop_data, prop_handle) (0x432f7-0x432fe); word
 *     [result+0x24] read into AX (0x43303); range-gated 2<=AX<=3 (signed
 *     CMP/JL/JG at 0x4330a-0x43314) before the type=1 path at 0x4331b; the
 *     trailing `CMP ESI,-1;JZ` at 0x43316 is provably dead on this path
 *     (only reachable here with ESI!=-1) but is kept as a literal condition
 *     rather than silently dropped.
 *   - Same look_buf convention as actor_conversation_control/FUN_00043360 in
 * this file: short[8] { int16_t type; int16_t pad; int data[3]; }; only
 *     look_buf[0] and *(int*)&look_buf[2] are ever written.
 *   - actor_look_secondary(EBX, [EBP+8], [EBP+0xc], &look_buf) at 0x43346: args
 *     pushed EDX(&look_buf), EAX([EBP+0xc]=priority), ECX([EBP+8]=
 *     look_type), EBX(actor_handle) — cdecl ADD ESP,0x10 (4 args).
 * Uncertain: no evidence for this function's semantic name; kept as
 *   ai_communication_look_secondary_at_unit with params named for their
 * forwarded role, matching the ai_communication_look_secondary_at_object
 * comment convention below. */
void ai_communication_look_secondary_at_unit(int prop_handle, int actor_handle,
                                             int object_handle, short look_type,
                                             short priority)
{
  short look_buf[8]; /* [0]=type word, [2..7]=data (int handle or float[3]
                         position) */
  char *prop;
  short state;
  int use_prop_look;

  use_prop_look = 0;

  if (actor_handle == -1)
    return;
  if (priority <= 0)
    return;
  if (object_handle == -1)
    return;
  if (object_try_and_get_and_verify_type(object_handle, 3) == NULL)
    return;

  if (prop_handle == -1) {
    prop_handle = prop_get_active_by_unit_index(actor_handle, object_handle);
  }
  if (prop_handle != -1) {
    prop = (char *)datum_get(prop_data, prop_handle);
    state = *(short *)(prop + 0x24);
    if (state >= 2 && state <= 3 && prop_handle != -1) {
      use_prop_look = 1;
    }
  }

  if (use_prop_look) {
    look_buf[0] = 1;
    *(int *)&look_buf[2] = prop_handle;
  } else {
    look_buf[0] = 3;
    unit_get_head_position(object_handle, (float *)&look_buf[2]);
  }
  actor_look_secondary(actor_handle, look_type, priority, look_buf);
}

/* ai_communication_look_secondary_at_object (0x43360) — issue a secondary "look
 * at object" request (look_buf[0]=6) for an actor, gated on valid actor/object
 * handles and a positive priority. Called unconditionally from
 * ai_communication_handle_received_looking (0x43eef).
 *
 * Confirmed: register-arg gate at 0x43366-0x43373: CMP EDI,-1/JZ;
 *   TEST BX,BX/JLE; CMP ESI,-1/JZ — no incoming register store in this
 *   function's prologue, so EDI/ESI/BX are @<reg> parameters, not locals.
 * Confirmed: object_try_and_get_and_verify_type(ESI, -1) at 0x43378/0x4337d
 *   (cdecl, 2 args); NULL-result branch at 0x43380/0x43382.
 * Confirmed: look_buf layout matches the actor_conversation_control convention
 * (this file's actor_looking.c, 0x14540): short[8] buffer, [0]=type tag,
 *   *(int*)&buf[2]=data[0]. Here only buf[0]=6 (MOV word [EBP-0x10],0x6 at
 *   0x4338e) and *(int*)&buf[2]=ESI (MOV dword [EBP-0xc],ESI at 0x43394) are
 *   written; buf[4..7] (data[1..2]) are left uninitialized, matching the
 *   original's single-store pattern — do not zero-fill them.
 * Confirmed: actor_look_secondary(EDI, [EBP+8], EBX, &look_buf) at 0x43397,
 * args pushed EAX(&buf), EBX(priority), ECX([EBP+8]=look_type stack param),
 *   EDI(actor_handle) — cdecl ADD ESP,0x10 (4 args).
 * Uncertain: no evidence for this function's semantic name, nor for the
 *   buf[0]=6 tag's meaning (actor_look_secondary only special-cases tag==1;
 * tag=6 is opaque here) or for the stack look_type parameter's caller-supplied
 *   value — kept as ai_communication_look_secondary_at_object with params named
 * for their forwarded role in actor_look_secondary's own signature. */
void ai_communication_look_secondary_at_object(short look_type,
                                               int actor_handle,
                                               int object_handle,
                                               short priority)
{
  short look_buf[8];

  if (actor_handle != -1 && priority > 0 && object_handle != -1) {
    if (object_try_and_get_and_verify_type(object_handle, -1) != NULL) {
      look_buf[0] = 6;
      *(int *)&look_buf[2] = object_handle;
      actor_look_secondary(actor_handle, look_type, priority, look_buf);
    }
  }
}

/* ai_conversation_status (0x433b0) — status of the scenario conversation
 * param_1. Walks the live conversation table (*(data_t **)0x6324ec); for each
 * entry whose index field (+0x2) matches, derives a status from bytes +0x6,
 * +0x5 and +0x8 (1 / 2 / 3 / 4) and keeps the maximum (signed 16-bit CMP
 * SI,AX; JG). A nonzero maximum is returned. Otherwise scans the ai globals
 * (*(char **)0x632574) record array at +0x30 (stride 0x10, count word +0x2c)
 * for the entry with matching word +0x0 and greatest dword +0x4 (strict >,
 * starting at -1); if one is found returns 5 when its byte +0x2 is set, else
 * 6 when byte +0x3 is set, else 7. Falls through to the (zero) status.
 * Field meanings are unproven; offsets only (disasm 0x433b0-0x434bf). */
int16_t ai_conversation_status(int16_t param_1)
{
  data_iter_t iter;
  char *conversation;
  char *record;
  int16_t status;
  int16_t current;
  int16_t index;
  int16_t best_index;
  int best_value;

  status = 0;
  data_iterator_new(&iter, *(data_t **)0x6324ec);
  conversation = (char *)data_iterator_next(&iter);
  if (conversation != 0) {
    do {
      if (*(int16_t *)(conversation + 2) == param_1) {
        if (conversation[6] == '\0') {
          current = 1;
        } else if (conversation[5] == '\0') {
          current = 2;
        } else {
          current = (int16_t)((conversation[8] != '\0') + 3);
        }
        status = status > current ? status : current;
      }
      conversation = (char *)data_iterator_next(&iter);
    } while (conversation != 0);
  }
  if (status == 0) {
    best_value = -1;
    best_index = -1;
    record = *(char **)0x632574;
    for (index = 0; index < *(int16_t *)(record + 0x2c); index++) {
      if (*(int16_t *)(record + 0x30 + index * 0x10) == param_1 &&
          *(int *)(record + 0x34 + index * 0x10) > best_value) {
        best_index = index;
        best_value = *(int *)(record + 0x34 + index * 0x10);
      }
    }
    if (best_index != -1) {
      record = *(char **)0x632574 + (best_index + 3) * 0x10;
      if (record[2] != '\0') {
        status = 5;
      } else {
        status = record[3] != '\0' ? 6 : 7;
      }
    }
  }
  return status;
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

/* ai_conversation_finish (0x435b0) — mark a conversation as finished: log it
 * to the debug console when the AI debug flag is set, record it in the AI
 * globals ring-history buffer at 0x632574+0x2e/0x2c (16-byte slots starting
 * at slot 3), invalidate the aim-target fields of every actor still listed
 * as a speaker (conversation+0x14 bitmask / +0x28+i*4 handle array — same
 * layout confirmed by ai_conversation_actor_deleted, 0x44590), then delete
 * the conversation datum. Called from ai_conversation_stop (0x44500) and
 * ai_conversation_actor_deleted (0x44590).
 *
 * Confirmed (disasm 0x435b0-0x4373a):
 *   - Early-out MOV ESI,[EBP+8]; CMP ESI,-1; JZ 0x43736 — conversation_handle
 *     == -1 skips the whole body (no console log, no ring write, no
 *     actor scrub, no datum_delete) and jumps straight to the epilogue that
 *     only pops ESI (EBX/EDI are not yet pushed on that path).
 *   - datum_get(*(data_t**)0x6324ec, handle) is called TWICE (0x435ca and
 *     0x43636) with identical arguments; MSVC does not common the two calls.
 *     The first result ([EBP-4], saved at 0x435d9) feeds the scenario index
 *     lookup and is the one read again after the printf for the actor-scrub
 *     loop (0x436d2 `MOV EAX,[EBP-4]`); the second result (a fresh EAX, not
 *     spilled) feeds only the ring-buffer +0x2 copy at 0x43686.
 *   - tag_block_get_element(scenario+0x468, index, 0x74) push order (0x74
 *     first, then index, then the block pointer immediately before the
 *     call) matches the decl (block, index, element_size).
 *   - console_printf argument order from the push sequence (0x4361c-0x43626):
 *     PUSH ECX(begin-status, keyed off param_2) / PUSH EAX(finish-status,
 *     keyed off param_3) / PUSH EBX(scenario_conversation) / PUSH format /
 *     PUSH channel(0) — console_printf(0, fmt, scenario_conversation,
 *     finish_status, begin_status). The ARG_COUNT hazard (cleanup=5 vs decl
 *     3+varargs) is a false positive: 2 of the 5 cleaned dwords are varargs.
 *   - *(char*)0x632574 (the shared AI globals block, same global as
 *     actor_communication_update/ai_conversation_stop) is reloaded from the
 *     global pointer at every single use (7 separate `MOV reg,[0x632574]`)
 *     rather than cached in a register — mirrored here by never caching it
 *     to a local, matching the rest of this TU.
 *   - Ring index math: SI = (int16_t)(counter+1) is stored back, then
 *     re-read and reduced via `AND 0x8000000f` + negative-correction — the
 *     standard MSVC codegen for signed `% 16` — written here as `% 16` so
 *     VC71 regenerates the identical mask-and-correct sequence.
 *   - The high-water mark update reads the ORIGINAL pre-increment counter
 *     (CX from 0x43643, sign-extended again at 0x43676) for the `counter+1`
 *     comparison/store, not the post-wrap value, so `index` below is always
 *     the pre-increment, un-wrapped counter.
 *   - Ring slot addressing is byte-offset from the AI globals base:
 *     (index+3)*0x10 for the int16 copy of conversation+0x2, then a second,
 *     independently recomputed index*0x10 base for the +0x32/+0x33/+0x34
 *     byte/byte/dword fields — matches the two separate LEA/SHL sequences at
 *     0x4368e and 0x4369d.
 *   - game_time_get() (FUN_000b5aa0) takes no arguments (no PUSH before
 *     0x436b8) and its EAX return is stored as a plain dword at
 *     ai_globals+index*0x10+0x34.
 *   - Actor scrub loop reads the bitmask/handle array from the FIRST
 *     datum_get result ([EBP-4]), not the second — proven by 0x436d2
 *     `MOV EAX,dword ptr [EBP-4]` immediately before the +0x14/+0x28 reads.
 *   - `i` is reused: first as the bit-test/array index (0..count-1), then
 *     reassigned to the array's actor-handle value inside the same
 *     `&&`-guarded comma expression (0x436e3 `MOV ECX,[EAX+ECX*4+0x28]`
 *     overwrites the index register) — mirrored with a single `i` and a
 *     comma expression to match the reuse exactly.
 *   - actor+0x1dc/+0x1e0 are stored unconditionally (EDI, the loop's -1
 *     sentinel from `OR EDI,0xffffffff`, reused as the write value);
 *     actor+0x9c is stored only when actor->state_action == 0xc (CMP
 *     word[EAX+0x6c],0xc at 0x436fd, matching the co() offset for
 *     state_action). 0x9c is left as a raw offset cast, not a named actor_t
 *     field: a dword write there would span the existing
 *     pad_09a[0x3]/field_09d/field_09e/field_09f bytes in src/types.h,
 *     which is evidence for a future struct-recovery pass, not something to
 *     resolve by guessing a field name here.
 * Uncertain: semantic names for the AI-globals ring fields (+0x2c/+0x2e
 *   counters, +0x10-stride slot layout) and for conversation+0x14/+0x28 are
 *   not recoverable from this call site alone — same "no named struct yet"
 *   situation as the other conversation functions in this TU. */
void ai_conversation_finish(int conversation_handle, char param_2, char param_3)
{
  char *conversation;
  char *conversation2;
  char *scenario_conversation;
  const char *finish_status;
  const char *begin_status;
  int16_t counter;
  int16_t high_water;
  int16_t wrapped;
  int16_t line_index;
  int index;
  int i;
  int time;
  actor_t *actor;

  if (conversation_handle == -1) {
    return;
  }
  conversation = (char *)datum_get(*(data_t **)0x6324ec, conversation_handle);
  scenario_conversation =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                  *(int16_t *)(conversation + 2), 0x74);
  if (*(char *)0x5aca5f != '\0') {
    begin_status = " (unable to begin)";
    if (param_2 == '\0') {
      begin_status = "";
    }
    finish_status = "successfully";
    if (param_3 == '\0') {
      finish_status = "prematurely";
    }
    console_printf(0, "%s: finished %s%s", scenario_conversation, finish_status,
                   begin_status);
  }
  conversation2 = (char *)datum_get(*(data_t **)0x6324ec, conversation_handle);
  counter = *(int16_t *)(*(char **)0x632574 + 0x2e);
  *(int16_t *)(*(char **)0x632574 + 0x2e) = counter + 1;
  wrapped = (int16_t)((int)*(int16_t *)(*(char **)0x632574 + 0x2e) % 16);
  *(int16_t *)(*(char **)0x632574 + 0x2e) = wrapped;
  high_water = *(int16_t *)(*(char **)0x632574 + 0x2c);
  index = (int)counter;
  if (high_water <= index + 1) {
    high_water = (int16_t)(index + 1);
  }
  *(int16_t *)(*(char **)0x632574 + 0x2c) = high_water;
  *(int16_t *)(*(char **)0x632574 + (index + 3) * 0x10) =
    *(int16_t *)(conversation2 + 2);
  index = index * 0x10;
  *(char *)(*(char **)0x632574 + index + 0x32) = param_2;
  *(char *)(*(char **)0x632574 + index + 0x33) = param_3;
  time = game_time_get();
  *(int32_t *)(*(char **)0x632574 + index + 0x34) = time;
  line_index = 0;
  if (0 < *(int32_t *)(scenario_conversation + 0x50)) {
    i = 0;
    do {
      if (((*(uint32_t *)(conversation + 0x14) & (1 << (i & 0x1f))) != 0) &&
          (i = *(int32_t *)(conversation + i * 4 + 0x28), i != -1)) {
        actor = (actor_t *)datum_get(*(data_t **)0x6325a4, i);
        actor->field_1dc = -1;
        actor->field_1e0 = -1;
        if (actor->state_action == 0xc) {
          *(int32_t *)((char *)actor + 0x9c) = -1;
        }
      }
      line_index = line_index + 1;
      i = (int)line_index;
    } while (i < *(int32_t *)(scenario_conversation + 0x50));
  }
  datum_delete(*(data_t **)0x6324ec, conversation_handle);
}

/* ai_conversation_new (0x43740) — begin (or force-start) a scenario
 * conversation. Tries to allocate a fresh conversation datum; if the pool is
 * full and the caller passed a non-zero param_2, it evicts one running
 * conversation (lowest +0x4 byte, tie-broken by oldest +0xc timestamp),
 * finishes it, and reuses its handle.  The resulting datum records the scenario
 * conversation index at +0x2, an invalid line index (-1) at +0x48, param_2 at
 * +0x4 and the current game time at +0xc.  Returns the datum index, or -1 on
 * failure.
 *
 * Confirmed (disasm 0x43740-0x43860):
 *   - Signature: `MOV DX,word ptr [EBP+8]` / `MOV AL,byte ptr [EBP+0xc]` —
 *     two cdecl stack args, a 16-bit and an 8-bit one; Ghidra's
 *     `void (void)` prototype is wrong.  The return value is real: two
 *     distinct epilogues load EAX from two different sources (`MOV EAX,EDI`
 *     at 0x4384e, `MOV EAX,[EBP-4]` at 0x43857), which is return-value
 *     codegen, not a dead EAX.  Both sources hold the same index value.
 *   - Sentinel init order is BL=1 (priority), EDI=0x7fffffff (time),
 *     ESI=-1 (handle) at 0x4377b..0x43782 — all three BEFORE the
 *     data_iterator_new call at 0x43785, so they are written before it here.
 *   - `OR ESI,0xffffffff` is the usual -1 sentinel, matching the rest of
 *     this TU.
 *   - Eviction test is a mixed-signedness pair: `CMP CL,BL` + `JC` is an
 *     UNSIGNED byte compare on +0x4, while `CMP dword ptr [EAX+0xc],EDI` +
 *     `JGE` is a SIGNED dword compare on +0xc.  The update block stores in
 *     the order time, handle, priority (0x437ac..0x437b2).
 *   - The first data_iterator_next call is peeled (0x4378e), the loop body
 *     re-enters at 0x437a0, so a do/while after a leading call is the
 *     matching shape.
 *   - iter.datum_handle is read from EBP-0xc with the iterator based at
 *     EBP-0x14, i.e. iterator offset +0x8 — data_iter_t.datum_handle.
 *   - tag_block_get_element(scenario+0x468, index, 0x74): pushes are 0x74
 *     first, then the sign-extended [EBP+8] index, then the block pointer
 *     (0x437da..0x437e7), matching the kb decl (block, index, element_size).
 *     NOTE: ai_conversation_advance (0x43520) in this TU passes these two
 *     swapped; that is a separate pre-existing issue, not touched here.
 *   - All three ARG_COUNT hazards are merged-ADD-ESP false positives:
 *     `ADD ESP,0xc` @0x43793 = data_iterator_new's 2 + data_iterator_next's
 *     1; `ADD ESP,0x18` @0x437fa = tag_block_get_element's 3 +
 *     console_printf's 3; `ADD ESP,0x14` @0x43813 =
 *     ai_conversation_finish's 3 + data_new_datum's 2.
 *   - game_time_get (0xb5aa0) is called with no pushes and its EAX stored as
 *     a plain dword at conversation+0xc.
 * Uncertain: semantics of conversation+0x4 (used both as the caller's flag
 *   and as the eviction key) and of +0x48; no string or struct evidence at
 *   this call site, so both stay raw offsets and param_2 keeps a mechanical
 *   name. */
int ai_conversation_new(int16_t scenario_conversation_index, char param_2)
{
  data_iter_t iter;
  char *conversation;
  char *cur;
  int index;
  int best_handle;
  int best_time;
  unsigned char best_priority;

  index = data_new_at_index(*(data_t **)0x6324ec);
  if (index == -1) {
    if (param_2 == '\0') {
      return index;
    }
    best_priority = 1;
    best_time = 0x7fffffff;
    best_handle = -1;
    data_iterator_new(&iter, *(data_t **)0x6324ec);
    cur = (char *)data_iterator_next(&iter);
    if (cur == 0) {
      return index;
    }
    do {
      if (*(unsigned char *)(cur + 4) < best_priority ||
          *(int32_t *)(cur + 0xc) < best_time) {
        best_time = *(int32_t *)(cur + 0xc);
        best_handle = (int)iter.datum_handle;
        best_priority = *(unsigned char *)(cur + 4);
      }
      cur = (char *)data_iterator_next(&iter);
    } while (cur != 0);
    if (best_handle == -1) {
      return index;
    }
    if (*(char *)0x5aca5f != '\0') {
      console_printf(
        0,
        "%s: this conversation is already running or trying to run, overwrite "
        "it",
        tag_block_get_element((char *)global_scenario_get() + 0x468,
                              (int)scenario_conversation_index, 0x74));
    }
    ai_conversation_finish(best_handle, '\0', '\0');
    index = data_new_datum(*(data_t **)0x6324ec, best_handle);
    if (index == -1) {
      return index;
    }
  }
  conversation = (char *)datum_get(*(data_t **)0x6324ec, index);
  *(int16_t *)(conversation + 2) = scenario_conversation_index;
  *(int16_t *)(conversation + 0x48) = -1;
  *(char *)(conversation + 4) = param_2;
  *(int32_t *)(conversation + 0xc) = game_time_get();
  return index;
}

/* ai_conversation_line_begin (0x43870) — arm the currently-selected line of one
 * running scenario conversation: resolve the speaking participant, cache the
 * speaker/listener object handles and the sound tag reference, and set the line
 * countdown.  Returns true when the line was armed, false when the participant
 * index is out of range or that participant is not present in the conversation.
 *
 * Confirmed (disasm 0x43870-0x43a1b):
 *   - Conversation handle arrives in EAX (PUSH EAX at 0x4387f feeds
 *     datum_get(*(data_t **)0x6324ec, handle) with no prior def of EAX), and
 *     the result is a bool in AL (XOR AL,AL at 0x438bd, MOV AL,1 at 0x43a13,
 *     single exit at 0x43a15).
 *   - tag_block_get_element(scenario+0x468, conversation->scenario_index, 0x74)
 *     then (conv_tag+0x5c, conversation[0x48], 0x7c) -> the line element.  The
 *     ADD ESP,0x18 at 0x438ba is MSVC merging the two 3-arg cleanups, not a
 *     6-arg call.
 *   - word[line+2] (participant index) is RELOADED at 0x438f7, 0x439a7 and
 *     0x439db rather than cached across the whole body.
 *   - Presence test: MOV EBX,1; SHL EBX,CL; TEST [conversation+0x14],EBX.
 *   - line+0x5c source: MOVSX word[conversation+idx*2+0x18]; SHL EAX,4;
 *     MOV ECX,[EAX+EDI+0x28] -> line + dialogue_index*0x10 + 0x28.
 *   - FLD [line+0xc]; FMUL [0x253394] (=30.0f); _ftol2 -> word[conv+0x4c].
 *   - Trailing zero stores are descending: 0x63, 0x62, 0x61. */
bool ai_conversation_line_begin(int conversation_handle)
{
  char *conversation;
  char *conv_tag;
  char *line;
  char *participant;
  char *actor;
  int *participant_count;
  short participant_index;
  short other_index;
  int actor_handle;
  bool result;

  conversation = (char *)datum_get(*(data_t **)0x6324ec, conversation_handle);
  conv_tag =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                  (int)*(int16_t *)(conversation + 2), 0x74);
  line = (char *)tag_block_get_element(
    conv_tag + 0x5c, (int)*(int16_t *)(conversation + 0x48), 0x7c);
  participant_index = *(int16_t *)(line + 2);
  result = 0;
  if (participant_index >= 0) {
    participant_count = (int *)(conv_tag + 0x50);
    if ((int)participant_index < *participant_count &&
        (*(uint32_t *)(conversation + 0x14) &
         (1 << participant_index)) != 0) {
      participant = (char *)tag_block_get_element(participant_count,
                                                  (int)participant_index, 0x54);
      actor_handle =
        *(int32_t *)(conversation + *(int16_t *)(line + 2) * 4 + 0x28);
      *(int16_t *)(conversation + 0x4a) = *(int16_t *)(line + 2);
      if (actor_handle == -1) {
        *(int32_t *)(conversation + 0x50) = -1;
        *(int32_t *)(conversation + 0x54) = -1;
        *(int32_t *)(conversation + 0x58) = -1;
        *(char *)(conversation + 0x60) = 1;
      } else {
        actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
        *(int32_t *)(conversation + 0x50) = actor_handle;
        *(int32_t *)(conversation + 0x54) = *(int32_t *)(actor + 0x18);
        *(int32_t *)(conversation + 0x58) = -1;
        switch (*(int16_t *)(line + 4)) {
        case 1:
          *(int32_t *)(conversation + 0x58) = *(int32_t *)(conversation + 0x10);
          break;
        case 2:
          other_index = *(int16_t *)(line + 6);
          if (other_index >= 0 && (int)other_index < *participant_count) {
            actor_handle = *(int32_t *)(conversation + other_index * 4 + 0x28);
            if (actor_handle != -1) {
              actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
              *(int32_t *)(conversation + 0x58) = *(int32_t *)(actor + 0x18);
            }
          }
          break;
        }
        *(char *)(conversation + 0x60) = (*(int16_t *)(participant + 4) == 6 ||
                                          *(int16_t *)(participant + 4) == 7);
      }
      assert_halt_msg_at(
        "(conversation->dialogue_indices[line->participant_index] >= 0) && "
        "(conversation->dialogue_indices[line->participant_index] < "
        "MAXIMUM_DIALOGUE_VARIANTS_PER_CONVERSATION_PARTICIPANT)",
        "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x146b,
        *(int16_t *)(conversation + *(int16_t *)(line + 2) * 2 + 0x18) >= 0 &&
          *(int16_t *)(conversation + *(int16_t *)(line + 2) * 2 + 0x18) < 6);
      *(int32_t *)(conversation + 0x5c) =
        *(int32_t *)(line +
                     *(int16_t *)(conversation + *(int16_t *)(line + 2) * 2 +
                                  0x18) *
                       0x10 +
                     0x28);
      *(int16_t *)(conversation + 0x4c) =
        (int16_t)(int)(*(float *)(line + 0xc) * *(float *)0x253394);
      *(int16_t *)(conversation + 0x4e) = *(int16_t *)line;
      *(char *)(conversation + 0x63) = 0;
      *(char *)(conversation + 0x62) = 0;
      *(char *)(conversation + 0x61) = 0;
      result = 1;
    }
  }
  return result;
}

/* ai_conversation_line_perform (0x43a20) — per-tick driver for the currently
 * armed line of one running conversation: start the line's sound (unit speech
 * or a scripted sound) once nothing blocks it, then wait for it to finish and
 * for the line's countdown to expire, and report whether the line is done.
 *
 * Confirmed (disasm 0x43a20-0x43ca8):
 *   - Conversation handle arrives in EAX (PUSH EAX at 0x43a2d with no prior
 *     def of EAX); every exit loads AL from byte [conversation+0x63]
 *     (0x43c69, 0x43c91, 0x43ca1), so the result is that byte.
 *   - tag_block_get_element(scenario+0x468, word[conversation+2], 0x74) is
 *     kept in [EBP-0x8] and pushed as the first %s of console_printf.
 *   - Participant loop: short counter in BX, bound dword [conv_tag+0x50]
 *     reloaded each pass; flags word [conversation+0x4e] reloaded each pass.
 *     Actor tests: word +0x6c == 0xc, DWORD +0xa8 != -1 (the actor_t field
 *     is typed int16, so raw access is used), bytes +0xa1 / +0xa0 == 0.
 *   - unit_test_speech pushes (last arg first): &[EBP-0xc] (sound index,
 *     preset from +0x5c), &[EBP-0x10] (dword -1), 0, EBX=1, 0, 6, unit handle
 *     (+0x54); ADD ESP,0x1c = 7 stack dwords.
 *     The type local is a dword store, so it is int passed via `short *`.
 *   - Result 1 skips to the +0x61 re-check without marking the line started;
 *     results <= 0 fall through to the marking stores.
 *   - 0x30-byte record at EBP-0x40: +0x00=6, +0x02=-1, +0x04=conv+0x5c,
 *     +0x10=conv+0x58, +0x14/+0x16/+0x18=-1, +0x1c/+0x1e=1, +0x20=conv+0x54,
 *     +0x24=0; unit_speak(conv+0x54 reloaded, count, record).
 *   - scripted_sound_new(conv+0x5c, -1, 1.0f) (PUSH 0x3f800000).
 *   - +0x62: object_get_and_verify_type(conv+0x54, 3), word +0x338 != 6.
 * Uncertain: meaning of the +0x4e flag bits 0x08/0x10/0x20, of actor
 *   state_action 0xc, and of bytes +0x05/+0x08/+0x09/+0x60. */
bool ai_conversation_line_perform(int conversation_handle)
{
  char *conversation;
  char *conv_tag;
  char *actor;
  char communication[0x30];
  int sound_definition_index;
  int vocalization_type;
  int actor_handle;
  int speaking_unit_index;
  int line_sound_index;
  short index;
  short communication_count;
  char blocked;

  conversation = (char *)datum_get(*(data_t **)0x6324ec, conversation_handle);
  conv_tag =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                  (int)*(int16_t *)(conversation + 2), 0x74);
  if (*(char *)(conversation + 0x63) != 0) {
    return *(char *)(conversation + 0x63);
  }
  if (*(char *)(conversation + 0x61) == 0) {
    blocked = 0;
    if (*(int32_t *)(conversation + 0x5c) != -1) {
      if ((*(uint16_t *)(conversation + 0x4e) & 0x30) != 0) {
        for (index = 0; (int)index < *(int32_t *)(conv_tag + 0x50); index++) {
          actor_handle = *(int32_t *)(conversation + index * 4 + 0x28);
          if (actor_handle != -1) {
            actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
            if (((*(uint16_t *)(conversation + 0x4e) & 0x20) != 0 ||
                 ((*(uint16_t *)(conversation + 0x4e) & 0x10) != 0 &&
                  actor_handle == *(int32_t *)(conversation + 0x50))) &&
                *(int16_t *)(actor + 0x6c) == 0xc &&
                *(int32_t *)(actor + 0xa8) != -1 &&
                *(char *)(actor + 0xa1) == 0 && *(char *)(actor + 0xa0) == 0) {
              blocked = 1;
            }
          }
        }
      }
      if (sound_scripted_dialog_is_playing() || blocked != 0) {
        goto check_started;
      }
      speaking_unit_index = *(int32_t *)(conversation + 0x54);
      if (speaking_unit_index == -1 || *(char *)(conversation + 0x60) != 0) {
        scripted_sound_new(*(int32_t *)(conversation + 0x5c), -1, 1.0f);
      } else {
        sound_definition_index = *(int32_t *)(conversation + 0x5c);
        vocalization_type = -1;
        communication_count = unit_test_speech(
          speaking_unit_index, 6, 0, 1, NULL, (short *)&vocalization_type,
          &sound_definition_index);
        if (communication_count == 1) {
          goto check_started;
        }
        if (communication_count > 0) {
          csmemset(communication, 0, 0x30);
          line_sound_index = *(int32_t *)(conversation + 0x5c);
          *(short *)(communication + 0x02) = -1;
          *(short *)(communication + 0x14) = -1;
          *(short *)(communication + 0x18) = -1;
          *(short *)(communication + 0x16) = -1;
          *(int32_t *)(communication + 0x10) =
            *(int32_t *)(conversation + 0x58);
          *(short *)(communication + 0x00) = 6;
          *(int32_t *)(communication + 0x04) = line_sound_index;
          *(short *)(communication + 0x1c) = 1;
          *(short *)(communication + 0x1e) = 1;
          *(int32_t *)(communication + 0x20) =
            *(int32_t *)(conversation + 0x54);
          *(short *)(communication + 0x24) = 0;
          if (*(char *)0x5aca5f != '\0') {
            console_printf(0, "%s: speak %s", conv_tag,
                           tag_get_name(line_sound_index));
          }
          unit_speak(*(int32_t *)(conversation + 0x54), communication_count,
                     communication);
        }
      }
    }
    *(char *)(conversation + 0x61) = 1;
    *(char *)(conversation + 5) = 1;
  check_started:
    if (*(char *)(conversation + 0x61) == 0) {
      return *(char *)(conversation + 0x63);
    }
  }
  if (*(char *)(conversation + 0x62) == 0) {
    if (*(int32_t *)(conversation + 0x54) == -1) {
      line_sound_index = *(int32_t *)(conversation + 0x5c);
      *(char *)(conversation + 0x62) =
        line_sound_index == -1 || scripted_sound_time(line_sound_index) == 0;
    } else {
      *(char *)(conversation + 0x62) =
        *(int16_t *)((char *)object_get_and_verify_type(
                       *(int32_t *)(conversation + 0x54), 3) +
                     0x338) != 6;
    }
    if (*(char *)(conversation + 0x62) == 0) {
      return *(char *)(conversation + 0x63);
    }
  }
  if (*(int16_t *)(conversation + 0x4c) > 0) {
    *(int16_t *)(conversation + 0x4c) -= 1;
    return *(char *)(conversation + 0x63);
  }
  *(char *)(conversation + 0x63) = 1;
  if ((*(uint16_t *)(conversation + 0x4e) & 8) != 0) {
    if (*(char *)(conversation + 8) == 0) {
      *(char *)(conversation + 8) = 1;
      *(char *)(conversation + 9) = 0;
    }
    if (*(char *)(conversation + 9) != 0) {
      *(char *)(conversation + 8) = 0;
      return *(char *)(conversation + 0x63);
    }
    *(char *)(conversation + 0x63) = 0;
  }
  return *(char *)(conversation + 0x63);
}

/* actor_reset_idle_vocalization_timer (0x43ce0) — arm the idle/fighting
 * vocalization countdown for an actor: pick a random delay from the 'actr'
 * tag's idle-vocalization range (a separate min/max pair for in-combat vs
 * not), scale seconds to ticks, add a per-unit speech-queue bonus when the
 * actor's controlled unit is still speaking, and stash both the countdown
 * and the actor's current "fighting" flag.
 *
 * Confirmed (disasm 0x43ce0-0x43da8):
 *   - PUSH EBP; MOV EBP,ESP; SUB ESP,0x8 — two dword locals at EBP-0x4 and
 *     EBP-0x8; PUSH EBX/ESI/EDI at entry.  MOV EDI,EAX at 0x43ce9 captures
 *     the actor handle from the register-arg convention (@<eax>).
 *   - datum_get(g_actors_data, actor_handle) -> ESI (actor); tag_get
 *     (0x61637472 'actr', actor->field_058) -> [EBP-0x4] (actr_tag);
 *     actor_in_combat(actor_handle) -> BL (fighting).  These three calls are
 *     EDI's (the actor handle's) last use; ADD ESP,0x14 at 0x43d15 cleans
 *     all three calls' pushes (2+2+1 dwords) at once.
 *   - XOR EDI,EDI immediately re-purposes EDI as an int16 "bonus"
 *     accumulator (default 0), unrelated to the actor handle it displaced.
 *     If actor->field_018 (the actor's unit handle, matches the field
 *     actor_communication_update below passes to unit_test_speech/
 *     unit_speak) != -1, object_get_and_verify_type(actor->field_018, 3)
 *     resolves the unit; if the unit's speech_count field (+0x338, int16 —
 *     the same field unit_is_speaking in units.c tests `> 0`) is > 0,
 *     `bonus` becomes the unit's raw field at +0x3aa (int16; the same
 *     un-named offset ai_communication_update_speech_timers below reads as
 *     `*(int16_t *)(unit + 0x3aa)`).
 *     THE DECOMPILER DROPS THIS ENTIRE BLOCK: its pseudocode never mentions
 *     EDI, +0x338, +0x3aa, or a "bonus" value at all — confirmed only by
 *     reading the raw disassembly (0x43d18-0x43d38).
 *   - `fighting` selects the actr tag's delay range: not-in-combat reads
 *     actr_tag+0x3f8 (min) / +0x3fc (max); in-combat reads actr_tag+0x400
 *     (min) / +0x404 (max) — both float pairs, pushed min-then-max (closer
 *     to the CALL = earlier cdecl argument) after the branch, then
 *     get_global_random_seed_address()'s result is pushed last (closest to
 *     the CALL, so the first argument) onto random_real_range.
 *   - random_real_range(seed, min, max) returns the delay in seconds in
 *     ST(0); FMUL float ptr [0x253394] (TICKS_PER_SECOND, =30.0f, confirmed
 *     by reading the bytes 00 00 F0 41 at that address) converts seconds to
 *     ticks — the same scale constant ai_communication_update_speech_timers
 *     uses for its own FMUL/FIADD/_ftol2 idiom two functions below.
 *   - MOVSX EDX,DI; MOV [EBP-8],EDX stages `bonus` as a spilled dword, then
 *     FIADD dword ptr [EBP-8] adds it (an FPU integer add, not a memory
 *     float load) to the ticks value before _ftol2 truncates to an int16
 *     tick count.  THE DECOMPILER ALSO DROPS THE FMUL AND FIADD — its
 *     pseudocode shows a bare, argument-less `_ftol2()` call, as if
 *     converting whatever ST(0) happened to already hold.
 *   - Final stores: actor->field_6cc = fighting (matches
 *     actor_communication_update's read of the same field below);
 *     actor->field_6ce = the truncated tick count (matches the countdown
 *     that actor_communication_update decrements).
 */
void actor_reset_idle_vocalization_timer(int actor_handle)
{
  actor_t *actor;
  char *actr_tag;
  char *unit;
  char fighting;
  float delay_min;
  float delay_max;
  int16_t bonus;
  int *seed;
  float delay_seconds;

  actor = (actor_t *)datum_get(*(data_t **)0x6325a4, actor_handle);
  actr_tag = (char *)tag_get(0x61637472, actor->field_058);
  fighting = actor_in_combat(actor_handle);

  bonus = 0;
  if (actor->field_018 != -1) {
    unit = (char *)object_get_and_verify_type(actor->field_018, 3);
    if (*(int16_t *)(unit + 0x338) > 0) {
      bonus = *(int16_t *)(unit + 0x3aa);
    }
  }

  if (fighting == 0) {
    delay_min = *(float *)(actr_tag + 0x3f8);
    delay_max = *(float *)(actr_tag + 0x3fc);
  } else {
    delay_min = *(float *)(actr_tag + 0x400);
    delay_max = *(float *)(actr_tag + 0x404);
  }

  seed = get_global_random_seed_address();
  delay_seconds = random_real_range(seed, delay_min, delay_max);
  actor->field_6cc = fighting;
  actor->field_6ce = (int16_t)(int)(delay_seconds * TICKS_PER_SECOND + bonus);
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
 *   - actor_reset_idle_vocalization_timer is called with MOV EAX,EDI at
 * 0x43e06, i.e. the actor handle in EAX; 0x43ce9 (MOV EDI,EAX) proves the
 * callee consumes it, so the kb.json decl carries `@<eax>`.  It is the routine
 * that writes both +0x6cc (the cached fighting flag) and +0x6ce (the
 * countdown).
 *   - The countdown is read once into EAX (XOR EAX,EAX; MOV AX,[ESI+0x6ce]),
 *     tested `> 0` (TEST AX,AX / JLE), decremented, stored back and tested
 *     `== 0` (DEC EAX; TEST AX,AX; MOV [ESI+0x6ce],AX; JNZ) — one load, hence
 *     the `> 0 && --field == 0` form rather than three separate reads.
 *   - EBP-0x4 is written with a full dword (MOV dword ptr [EBP-0x4],EDX after
 *     XOR EDX,EDX / SETNZ DL), so the vocalization-type local is int-width and
 *     is passed to unit_test_speech through a `short *` cast; EBP-0x8 is
 * likewise a dword -1.
 *   - unit_test_speech's pushes at 0x43e2d..0x43e45 are EAX(=&[EBP-0x8]),
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
 *   - The single ADD ESP,0x1c at 0x43e94 cleans unit_speak's three pushes
 *     plus csmemset's three and ai_communication_packet_new's one — MSVC
 *     coalesced the cleanups, so the ARG_COUNT hazard on unit_speak
 *     (cleanup=7 vs decl=3) is a false positive.
 *   - MOV EDI,EAX; TEST DI,DI; JLE — the communication count is a signed
 *     16-bit `> 0` test.
 * Inferred: actor_in_combat (returns char, +0x6cc is a byte) is the actor
 *   "is fighting" predicate; the vocalization type passed to unit_test_speech
 * is just that flag widened, and vocalization index 1 is a literal at this call
 * site. Uncertain: the meaning of the `1`/`0` byte-width literals in arguments
 * 3 and 4 of unit_test_speech is not recoverable from this call site. */
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
    fighting = actor_in_combat(actor_handle);
    if (actor->field_6ce == 0 || actor->field_6cc != fighting) {
      actor_reset_idle_vocalization_timer(actor_handle);
    }
    if (actor->field_6ce > 0 && --actor->field_6ce == 0) {
      vocalization_type = (fighting != '\0');
      sound_definition_index = -1;
      communication_count =
        unit_test_speech(actor->field_018, 1, 1, 0, NULL,
                         (short *)&vocalization_type, &sound_definition_index);
      if (communication_count > 0) {
        csmemset(communication, 0, 0x30);
        *(short *)(communication + 0x02) = (short)vocalization_type;
        *(int32_t *)(communication + 0x04) = sound_definition_index;
        *(short *)(communication + 0x00) = 1;
        ai_communication_packet_new(communication + 0x10);
        unit_speak(actor->field_018, communication_count, communication);
      }
    }
  }
}

/* ai_communication_handle_received_looking (0x43ea0) — react to an incoming
 * communication record that requests a "look at" response: dispatch either
 * a secondary look-at-unit or look-at-object request depending on the
 * record's type field, escalating the look_type/priority passed to the
 * secondary-look helper when the record's object handle matches the actor's
 * already-active prop. Called unconditionally from ai_communication_notify
 * (0x45290, not yet ported) at 0x45474.
 *
 * Confirmed via disasm 0x43ea0-0x43f1b:
 *   - No incoming register store to ECX/EAX in the prologue (only
 *     `MOV EDI,ECX` at 0x43ea4 to preserve the pointer across the datum_get
 *     call) -> ECX/EAX are @<reg> parameters, not locals; [EBP+0x8] is a
 *     genuine caller-pushed stack argument (actor_handle), reused unchanged
 *     by both downstream calls.
 *   - Gate: word [ECX+0xe] (record's type field) <= 0 -> return (0x43eab).
 *   - datum_get(prop_data, EAX) at 0x43eb7 (cdecl, args PUSH EAX then
 *     PUSH ECX(=prop_data global @0x5ab23c) so ECX=data,EAX=datum_handle
 *     per datum_get's own decl) -> EAX holds the resolved prop record
 *     pointer for the rest of the function.
 *   - look_type default is 9 (MOV EDX,0x9 at 0x43ec7, unconditional);
 *     escalated to 8 only when record type==1 AND record's object handle
 *     (dword [EDI+0x10]) equals the resolved prop's dword [EAX+0x18]
 *     (0x43ec3-0x43ed6) — both conditions gate the single EDX,0x8 store.
 *   - MOVSX EAX,CX; DEC EAX; JZ / DEC EAX; JNZ (0x43edb-0x43ee2) is a
 *     type==1 / type==2 / else dispatch: type==2 falls through to
 *     ai_communication_look_secondary_at_object; type==1 jumps to the
 *     ai_communication_look_secondary_at_unit call; any other type value
 *     falls straight to the epilogue.
 *   - type==2 path (0x43ee4-0x43f14): ESI=dword[EDI+0x10](object_handle),
 *     BX=word[EDI+0xc](priority), EDI reloaded from [EBP+8](actor_handle),
 *     PUSH EDX(look_type) -> CALL ai_communication_look_secondary_at_object
 *     matches that callee's kb.json @<edi>/@<esi>/@<bx> + stack look_type.
 *   - type==1 path (0x43efc-0x43f14): EBX=[EBP+8](actor_handle),
 *     EAX=zero-extended word[EDI+0xc](priority) pushed first, EDX(look_type)
 *     pushed second (last before call, so it is the first cdecl param),
 *     EDI reloaded from dword[EDI_old+0x10](object_handle), EAX=-1
 *     (prop_handle, forces the callee to re-resolve its own prop) ->
 *     CALL ai_communication_look_secondary_at_unit matches that callee's
 *     kb.json @<eax>/@<ebx>/@<edi> + stack look_type,priority order.
 * Uncertain: the incoming record pointer has no recovered struct (no other
 *   site in this file names its fields), so it is dereferenced as a raw
 *   char* with field_0c/field_0e/field_10 offsets, matching the untyped
 *   `char *prop` convention used by the two callees in this same file;
 *   no evidence for what the type field's values 1/2 mean, nor for the
 *   8-vs-9 look_type semantics, beyond the binary comparison recovered
 *   above. */
void ai_communication_handle_received_looking(int actor_handle, void *record,
                                              int prop_handle)
{
  char *data;
  char *prop;
  short type;
  short look_type;

  data = (char *)record;
  if (*(short *)(data + 0x0e) > 0) {
    prop = (char *)datum_get(prop_data, prop_handle);
    type = *(short *)(data + 0x0e);
    look_type = 9;
    if (type == 1 && *(int32_t *)(data + 0x10) == *(int32_t *)(prop + 0x18)) {
      look_type = 8;
    }
    if (type == 1) {
      ai_communication_look_secondary_at_unit(
        -1, actor_handle, *(int32_t *)(data + 0x10), look_type,
        *(short *)(data + 0x0c));
    } else if (type == 2) {
      ai_communication_look_secondary_at_object(look_type, actor_handle,
                                                *(int32_t *)(data + 0x10),
                                                *(short *)(data + 0x0c));
    }
  }
}

/* ai_communication_update_speech_timers (0x43f20) — record that a unit just
 * spoke: stamp the unit's speech timestamp, raise the per-team "someone is
 * talking" high-water marks in the AI globals block, and stamp/arm the
 * dialogue and reply cooldown entries for this speech.
 *
 * Confirmed (disasm 0x43f20-0x441b4):
 *   - Frame PUSH EBP; MOV EBP,ESP; SUB ESP,0xc; PUSH EBX/ESI/EDI.  Locals are
 *     EBP-0x4 (team index, see below), EBP-0x8 (base_ticks) and EBP-0xc
 *     (ticks).  Plain RET, so the four stack parameters are cdecl.
 *   - The unit handle arrives in EAX: PUSH 0x3; PUSH EAX; CALL
 *     object_get_and_verify_type at 0x43f2b with EAX never written in this
 *     function beforehand — hence the `@<eax>` annotation in kb.json.
 *   - Stack parameters are EBP+0x8 (param_2, read as DI/CX — 16-bit),
 *     EBP+0xc (param_3), EBP+0x10 (dialogue_type_index) and EBP+0x14
 *     (reply_table_index).  The last two names are recovered verbatim from
 *     the assert strings at 0x440a6 and 0x44138.
 *   - EBP-0x8 is stored at 0x43f6f (before ADD ESI,EAX) and EBP-0xc at
 *     0x43f79 (after), so EBP-0x8 is game_time_get()'s raw result and
 *     EBP-0xc is that result plus the clamped delay.  EBX is reloaded from
 *     EBP-0x8 on both paths (0x44043 and 0x44087), so the cooldown entries'
 *     first dword receives base_ticks, not ticks.
 *   - MOVSX EAX,[EDI+0x3aa]; ADD EAX,-0x2d; XOR EDX,EDX; TEST EAX,EAX;
 *     SETL DL; DEC EDX; AND EAX,EDX — a branchless max(0, field-0x2d).
 *   - XOR ECX,ECX; MOV CX,word ptr [EAX+4] — the actor field at +0x4 is
 *     zero-extended, so it is read through an unsigned short.
 *   - TEST AL,0x2 / TEST AL,0x4 on actor_type_get_race's result select team
 * index 0 and 1 respectively; neither bit set returns without touching
 * anything.
 *   - The AI globals pointer at 0x632574 is re-loaded for each of the three
 *     high-water updates (0x43fe1, 0x43ffb, 0x44015), and EBP-0x4 is
 *     re-read with MOVSX at 0x43fdd, 0x440c6 and 0x44158 — kept as separate
 *     reads here rather than hoisted into one local.
 *   - CMP DI,0x5/JG, CMP DI,0x3/JL, CMP DI,0x5/JL are signed 16-bit tests,
 *     i.e. `param_2 <= 5`, `param_2 >= 3`, `param_2 >= 5`.
 *   - Dialogue entry address: LEA ESI,[ECX + (team + index*2)*8] off the
 *     table at 0x331f0c; its definition is LEA [EAX+EAX*4] then
 *     [EDI*8+0x257e48] = index*0x28, matching the 0x28 stride already
 *     documented for the comm dialogue table.  Reply entry is off 0x331f14
 *     with LEA [EAX+EAX*8] then [EDI*4+0x258eb0] = index*0x24.
 *   - FNSTSW AX; TEST AH,0x41; JNZ skip after FLD f / FCOMP [0x2533c0]
 *     proceeds only when f is strictly greater than the threshold.
 *   - The float at +0x14 (dialogue) / +0x1c (reply) is loaded twice (FLD at
 *     0x440f8 and 0x44108; 0x4418a and 0x4419a) — once to compare, once to
 *     scale — so it is read from memory twice rather than cached.
 *   - FLD f; FMUL [0x253394]; FIADD dword ptr [EBP-0xc]; _ftol2 is
 *     (int)(f * scale + ticks); _ftol2 is written as a plain cast.
 *   - Hazard ARG_COUNT on actor_type_get_race (cleanup=3, decl=1) is a false
 *     positive: the ADD ESP,0xc at 0x43fb1 is MSVC coalescing datum_get's
 *     two pushes with this call's single push.
 *   - Hazard ARG_COUNT on error (cleanup=8, decl=3) is likewise expected:
 *     error is varargs and ADD ESP,0x20 at 0x44082 covers level + format
 *     + six varargs.
 * Uncertain: param_2 and param_3 keep mechanical names.  param_2 indexes the
 *   three high-water slots and picks "talk" vs "chatter"; param_3 is only
 *   ever handed to dialogue_get_vocalization_name for the debug line.  Neither
 * meaning is proven by a string or assert at this call site. */
void ai_communication_update_speech_timers(int unit_handle, int16_t param_2,
                                           int16_t param_3,
                                           int16_t dialogue_type_index,
                                           int16_t reply_table_index)
{
  char *unit;
  void *actor;
  int base_ticks;
  int ticks;
  int delay;
  short team_index;
  int16_t communication_flags;
  char *ai_globals;
  int high_water;
  int32_t *entry;
  char *definition;
  const char *speech_kind;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(int32_t *)(unit + 0x1a4) == -1) {
    actor = NULL;
  } else {
    actor = datum_get(*(data_t **)0x6325a4, *(int32_t *)(unit + 0x1a4));
  }
  base_ticks = game_time_get();
  delay = (int)*(int16_t *)(unit + 0x3aa) - 0x2d;
  if (delay < 0) {
    delay = 0;
  }
  ticks = base_ticks + delay;
  *(int32_t *)(unit + 0x3a0) = ticks;
  if (actor != NULL) {
    actor_reset_idle_vocalization_timer(*(int32_t *)(unit + 0x1a4));
    actor = datum_get(*(data_t **)0x6325a4, *(int32_t *)(unit + 0x1a4));
    communication_flags =
      actor_type_get_race((int16_t) * (uint16_t *)((char *)actor + 4));
    if ((communication_flags & 2) != 0) {
      team_index = 0;
    } else if ((communication_flags & 4) != 0) {
      team_index = 1;
    } else {
      return;
    }
    if (param_2 <= 5) {
      ai_globals = *(char **)0x632574;
      high_water = *(int32_t *)(ai_globals + (int)team_index * 4 + 0x14);
      if (high_water <= ticks) {
        high_water = ticks;
      }
      *(int32_t *)(ai_globals + (int)team_index * 4 + 0x14) = high_water;
      if (param_2 >= 3) {
        ai_globals = *(char **)0x632574;
        high_water = *(int32_t *)(ai_globals + (int)team_index * 4 + 0x1c);
        if (high_water <= ticks) {
          high_water = ticks;
        }
        *(int32_t *)(ai_globals + (int)team_index * 4 + 0x1c) = high_water;
      }
      if (param_2 >= 5) {
        ai_globals = *(char **)0x632574;
        high_water = *(int32_t *)(ai_globals + (int)team_index * 4 + 0x24);
        if (high_water <= ticks) {
          high_water = ticks;
        }
        *(int32_t *)(ai_globals + (int)team_index * 4 + 0x24) = high_water;
      }
      if (*(char *)0x5aca54 != '\0') {
        speech_kind = "talk";
        if (param_2 < 3) {
          speech_kind = "chatter";
        }
        error(2, "%s %s %d/%s: %s %d",
              *(char **)(0x2c8d68 + (int)team_index * 8),
              unit_get_speech_priority_name(param_2), (int)dialogue_type_index,
              dialogue_get_vocalization_name(param_3, 1), speech_kind,
              ticks - base_ticks);
      }
    }
    if (dialogue_type_index != -1) {
      if (dialogue_type_index < 0 ||
          dialogue_type_index >= *(int16_t *)0x331f08) {
        display_assert("(dialogue_type_index >= 0) && (dialogue_type_index < "
                       "global_dialogue_event_count)",
                       "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xc9c, 1);
        system_exit(-1);
      }
      entry = (int32_t *)(*(char **)0x331f0c +
                          ((int)team_index + (int)dialogue_type_index * 2) * 8);
      definition = (char *)0x257e48 + (int)dialogue_type_index * 0x28;
      entry[0] = base_ticks;
      if ((game_connection() != 0 || *(char *)0x5aca46 == '\0') &&
          *(float *)(definition + 0x14) > *(float *)0x2533c0) {
        entry[1] =
          (int)(*(float *)(definition + 0x14) * *(float *)0x253394 + ticks);
      }
    }
    if (reply_table_index != -1) {
      if (reply_table_index < 0 || reply_table_index >= *(int16_t *)0x331f10) {
        display_assert("(reply_table_index >= 0) && (reply_table_index < "
                       "global_reply_event_count)",
                       "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xcaf, 1);
        system_exit(-1);
      }
      entry = (int32_t *)(*(char **)0x331f14 +
                          ((int)team_index + (int)reply_table_index * 2) * 8);
      definition = (char *)0x258eb0 + (int)reply_table_index * 0x24;
      entry[0] = base_ticks;
      if ((game_connection() != 0 || *(char *)0x5aca46 == '\0') &&
          *(float *)(definition + 0x1c) > *(float *)0x2533c0) {
        entry[1] =
          (int)(*(float *)(definition + 0x1c) * *(float *)0x253394 + ticks);
      }
    }
  }
}

/* ai_communication_get_player_rating (0x441c0) — rate how well-placed the
 * nearest "audience" player is to hear unit_handle speak.  DORMANT
 * (kb ported:false).
 *
 * Confirmed (disasm 0x441c0-0x444f9):
 *   - cdecl, 4 stack args, plain RET; returns float in ST(0) (every caller
 *     consumes ST(0) with FSTP/FCOM right after the CALL).
 *   - arg 2 is a byte (MOV AL,[EBP+0xc]); args 3/4 are optional out-pointers
 *     (dword player unit, float distance), each NULL-tested before the store.
 *   - player_data (0x5aa6d4) iteration; player datum +0x34 is the unit index.
 *   - vector = own head - player head; |v|^2 is summed z,x,y; dot product
 *     with the player's aiming vector is summed z,y,x (binary order kept).
 *   - Tunables read from .data: 0x257e34 = 30.0 absolute range,
 *     0x257e38 = 3.0 ideal min, 0x257e3c = 15.0 ideal max, 0x257e40 =
 *     0.7071 ideal fov, 0x253f44 = 1e-4 epsilon; 0x2533c8 = 1.0,
 *     0x253f40 = 2.0, 0x253398 = 0.5, 0x2533c4 = 0.7, 0x259cd0 = 0.35.
 *   - LOS branch: object +0x4c cluster words of both root parents, PVS test
 *     (0x18e800) only when both are != -1; a failed PVS skips the player.
 *     Collision-user push of type 3 around collision_test_vector
 *     (0x14df70, flags 0x27) with asserts at lines 0xe91/0xe97.
 *   - No player with a unit -> returns 1.0, else the best rating (starts
 *     at 0.0, so an in-range player always wins over none).
 * Inferred names. */
float ai_communication_get_player_rating(int unit_handle, char test_line_of_sight,
                                         int *out_unit_handle,
                                         float *out_distance)
{
  int16_t collision[40];
  data_iter_t iter;
  real_vector3d line_of_sight_vector;
  real_vector3d aiming_vector;
  real_point3d position;
  real_point3d player_position;
  int closest_unit;
  float best_rating;
  float closest_distance;
  char any_players;
  player_data_t *player;
  real_vector3d vector;
  float distance_squared;
  float distance;
  float rating;
  float facing;
  char clear_line_of_sight;
  char blocked;
  short cluster_index;
  short player_cluster_index;

  closest_unit = -1;
  best_rating = 0.0f;
  closest_distance = 3.4028235e+38f;
  any_players = 0;
  unit_get_head_position(unit_handle, (float *)&position);
  data_iterator_new(&iter, player_data);
  player = (player_data_t *)data_iterator_next(&iter);
  while (player != NULL) {
    if ((int)player->unit_handle != -1) {
      any_players = 1;
      unit_get_head_position(player->unit_handle, (float *)&player_position);
      vector_from_points3d(&player_position, &position, &vector);
      distance_squared = magnitude_squared3d(&vector);




      if (distance_squared < communication_player_absolute_range *
                               communication_player_absolute_range) {
        clear_line_of_sight = 0;
        if (test_line_of_sight) {
          /* object +0x4c low word = location.cluster_index */
          cluster_index =
            ((object_data_t *)object_get_and_verify_type(
               object_get_root_parent(unit_handle), -1))
              ->unk_76.index;
          player_cluster_index =
            ((object_data_t *)object_get_and_verify_type(
               object_get_root_parent(player->unit_handle), -1))
              ->unk_76.index;
          if (cluster_index != -1 && player_cluster_index != -1 &&
              !scenario_ensure_point_within_world(cluster_index,
                                                  player_cluster_index)) {
            goto next;
          }
          ai_profile.meters[_ai_meter_collisions].accumulator++;
          if (global_current_collision_user_depth >= 0x20) {
            display_assert("global_current_collision_user_depth < "
                           "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                           "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xe91, 1);
            system_exit(-1);
          }
          /* Push _collision_user_ai_comms (3); the depth is post-incremented
           * in place here and pre-decremented after the test, both directly
           * on the kb data symbols (the reference's INC/DEC word ptr forms
           * need the named global, not an absolute-address cast). */
          collision_user_stack[global_current_collision_user_depth++] = 3;
          vector_from_points3d(&player_position, &position,
                               &line_of_sight_vector);

          blocked = FUN_0014df70(0x27, (float *)&player_position,
                                 (float *)&line_of_sight_vector, -1, collision);
          if (global_current_collision_user_depth <= 1) {
            display_assert("global_current_collision_user_depth > 1",
                           "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xe97, 1);
            system_exit(-1);
          }
          --global_current_collision_user_depth;
          if (distance_squared < communication_player_ideal_range_min *
                                   communication_player_ideal_range_min ||
              !blocked) {
            clear_line_of_sight = 1;
          } else {
            clear_line_of_sight = 0;
          }
        }
        rating = 1.0f;
        distance = (float)x87_sqrtd(distance_squared);
        if (distance < communication_player_ideal_range_max) {
          if (distance < communication_player_ideal_range_min) {
            rating = 2.0f;
          } else {
            rating = (communication_player_ideal_range_max - distance) /
                       (communication_player_ideal_range_max -
                        communication_player_ideal_range_min) +
                     1.0f;
          }
          if (clear_line_of_sight) {
            rating = rating + 0.5f;
          }
          if (distance > 0.0001f /* _real_epsilon, 0x253f44 */) {
            unit_get_aiming_vector(player->unit_handle, &aiming_vector);
            facing = dot_product3d(&aiming_vector, &vector) / distance;



            if (facing > communication_player_ideal_fov) {
              rating = (0.7f - (1.0f - facing) /
                                 (1.0f - communication_player_ideal_fov) *
                                 0.35f) +
                       rating;
            }
          }
        }
        if (rating > best_rating) {
          best_rating = rating;
          closest_unit = player->unit_handle;
          closest_distance = distance;
        }
      }
    }
  next:
    player = (player_data_t *)data_iterator_next(&iter);
  }
  if (!any_players) {
    rating = 1.0f;
  } else {
    rating = best_rating;
  }
  if (out_distance != NULL) {
    *out_distance = closest_distance;
  }
  if (out_unit_handle != NULL) {
    *out_unit_handle = closest_unit;
  }
  return rating;
}

/* ai_conversation_stop (0x44500) — iterate all conversations and finish every
 * one whose index field (+0x2) matches param_1. When the AI debug flag at
 * 0x5aca5f is set, logs the stop via console_printf with the conversation
 * name from the scenario tag block at offset 0x468, same idiom as the
 * neighbouring ai_conversation_advance (0x43520).
 *
 * Confirmed (disasm 0x44500-0x44589):
 *   - param read once into SI (MOV SI,word[EBP+8] at 0x44526) — 16-bit, so
 *     the parameter is a short, not the placeholder `int` in kb.json.
 *   - LEA EBX,[EBX] at 0x4452a is a 3-byte filler NOP (self-referential LEA),
 *     not real code — alignment padding before the loop top.
 *   - Loop shape matches ai_conversation_advance: first data_iterator_next()
 *     result tested before entering a do/while, back-edge (JNZ -> 0x44530)
 *     targets CMP word[EAX+2],SI, single epilogue at 0x44586.
 *   - tag_block_get_element argument order confirmed by push sequence: PUSH
 *     0x74 (element_size, pushed first = rightmost decl arg) then PUSH EAX
 *     (index = MOVSX of param_1, pushed second) then, after
 *     global_scenario_get() returns and +0x468 is added, PUSH EAX (block,
 *     pushed last = first decl arg) immediately before the CALL — i.e.
 *     tag_block_get_element(block, param_1, 0x74).
 *   - ECX at 0x44565 (MOV ECX,dword[EBP-0x8]) is local_14 (data_iter_t at
 *     EBP-0x10) + 0x8 = the datum_handle field, matching data_iter_t's
 *     confirmed layout in types.h.
 *   - FUN_000435b0 (ai_conversation_finish) is called with
 *     (datum_handle, 0, 0) — PUSH 0x0; PUSH 0x0; PUSH ECX, cdecl right-to-
 *     left, so param_2/param_3 are both 0. */
void ai_conversation_stop(int16_t param_1)
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
        console_printf(0, "%s: told to stop by scripting",
                       tag_block_get_element(
                         (char *)global_scenario_get() + 0x468, param_1, 0x74));
      }
      ai_conversation_finish(iter.datum_handle, 0, 0);
    }
    conversation = (char *)data_iterator_next(&iter);
  } while (conversation != 0);
}

/* ai_conversation_actor_deleted (0x44590) — called when an actor datum is
 * deleted; scrubs that actor's handle out of every conversation's speaker
 * line list, or finishes the conversation outright if the scenario
 * conversation definition is flagged "stop when speaker deleted".
 *
 * Confirmed (disasm 0x44590-0x44651):
 *   - Outer loop is the same iterate-all-conversations shape as
 *     ai_conversation_stop/ai_conversation_advance: first
 *     data_iterator_next() checked before the loop body (TEST ESI,ESI;
 *     JZ -> pop esi; ret at 0x4464d), back-edge (JNZ -> 0x445c3) after a
 *     second data_iterator_next() at the bottom.
 *   - tag_block_get_element argument order confirmed by push sequence
 *     (PUSH 0x74; PUSH EAX(=index, MOVSX of conversation+0x2); ADD EAX,0x468
 *     on global_scenario_get()'s result; PUSH EAX(=block) immediately before
 *     the CALL) — same (block, index, 0x74) order as ai_conversation_stop.
 *   - Inner loop count is NOT cached: MOV ECX,[EAX+0x50] guards the initial
 *     JLE, and MOV EDI,[EAX+0x50] re-reads the same field at the bottom of
 *     the inner do-while — two separate loads of the scenario conversation
 *     definition's +0x50 count, matching Ghidra's decompile which
 *     recomputes rather than caching, so the C mirrors that (no local
 *     `count` variable).
 *   - Inner loop scans conversation+0x28+i*4 (an array of actor handles,
 *     one per speaker line) for a match against the deleted actor's handle
 *     (CMP EDI,[EBP+8]).
 *   - On match, TEST byte[EAX+0x20],1 (scenario conversation flags bit 0)
 *     selects between two outcomes:
 *       - set: FUN_000435b0/ai_conversation_finish(iter.datum_handle,0,0)
 *         then BREAK out of the inner loop (falls straight to the next
 *         data_iterator_next() at 0x44635, same target as normal inner-loop
 *         exit at 0x44623).
 *       - clear: clears bit i in conversation+0x14 (AND with the negated,
 *         CL-masked 1<<i — SHL EDI,CL; NOT EDI; AND), sets
 *         conversation+0x28+i*4 to -1 (0xffffffff), and if
 *         conversation+0x4a (word) equals the pre-increment loop counter,
 *         sets conversation+0x63 (byte 99) to 1.
 *   - Loop counter is 16-bit (line_index, DX) sign-extended into the 32-bit
 *     array/compare index (MOVSX ECX,DX at 0x4461c) each iteration, matching
 *     the `short` narrow-then-widen shape used by the neighbouring
 *     conversation functions in this file.
 * Uncertain: semantic names for conversation+0x14 (bitmask), +0x28 (speaker
 *   handle array), +0x4a (word), +0x63 (byte) are not recoverable from this
 *   call site alone — no named conversation struct exists yet in this TU
 *   (ai_conversation_line/advance/stop all use the same raw offset-cast
 *   idiom), so this function follows that convention rather than inventing
 *   one. */
void ai_conversation_actor_deleted(int actor_handle)
{
  data_iter_t iter;
  char *conversation;
  void *scenario_conversation;
  int i;
  int16_t line_index;

  data_iterator_new(&iter, *(data_t **)0x6324ec);
  conversation = (char *)data_iterator_next(&iter);
  if (conversation == 0) {
    return;
  }
  do {
    scenario_conversation =
      tag_block_get_element((char *)global_scenario_get() + 0x468,
                            *(int16_t *)(conversation + 2), 0x74);
    line_index = 0;
    if (0 < *(int32_t *)((char *)scenario_conversation + 0x50)) {
      i = 0;
      do {
        if (*(int32_t *)(conversation + i * 4 + 0x28) == actor_handle) {
          if ((*(uint8_t *)((char *)scenario_conversation + 0x20) & 1) != 0) {
            ai_conversation_finish(iter.datum_handle, 0, 0);
            break;
          }
          *(uint32_t *)(conversation + 0x14) &= ~(1 << (i & 0x1f));
          *(int32_t *)(conversation + i * 4 + 0x28) = -1;
          if (*(int16_t *)(conversation + 0x4a) == line_index) {
            *(uint8_t *)(conversation + 0x63) = 1;
          }
        }
        line_index = line_index + 1;
        i = (int)line_index;
      } while (i < *(int32_t *)((char *)scenario_conversation + 0x50));
    }
    conversation = (char *)data_iterator_next(&iter);
  } while (conversation != 0);
}

/* ai_conversation_unit_died (0x44660).
 *
 * Confirmed from disassembly:
 *   - Iterates the conversation data pool and resolves each conversation's
 *     scenario definition from scenario+0x468 with element size 0x74.
 *   - Clears matching unit handles at conversation offsets +0x54, +0x58,
 *     and +0x10; a +0x54 match also sets byte +0x63.
 *   - When param_2 is nonzero, clears matching actor fields at +0xa8 and
 *     +0x1e0 when actor word +0x6c is 0xc.
 *   - Finishes and returns on a match, logging the fixed string when the
 *     trace byte at 0x5aca5f is nonzero.
 *
 * Record offsets remain raw because their semantic field names are not
 * established by this function. */
void ai_conversation_unit_died(int unit_handle, char param_2)
{
  data_iter_t iterator;
  char *conversation;
  char *definition;
  char *actor;
  int conversation_index;
  int actor_handle;
  char unit_matches;

  data_iterator_new(&iterator, *(data_t **)0x6324ec);
  conversation = (char *)data_iterator_next(&iterator);
  if (conversation != (char *)0) {
    do {
      definition = (char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x468,
        (int)*(int16_t *)(conversation + 0x02), 0x74);

      unit_matches = 0;
      if (*(int32_t *)(conversation + 0x54) == unit_handle) {
        unit_matches = 1;
        *(uint8_t *)(conversation + 0x63) = 1;
        *(int32_t *)(conversation + 0x54) = -1;
      }
      if (*(int32_t *)(conversation + 0x58) == unit_handle) {
        unit_matches = 1;
        *(int32_t *)(conversation + 0x58) = -1;
      }
      if (*(int32_t *)(conversation + 0x10) == unit_handle) {
        unit_matches = 1;
        *(int32_t *)(conversation + 0x10) = -1;
      }

      if (param_2 != 0 || (*(uint8_t *)(definition + 0x20) & 1) != 0) {
        conversation_index = 0;
        if (*(int32_t *)(definition + 0x50) > 0) {
          do {
            if ((*(uint32_t *)(conversation + 0x14) &
                 (1u << conversation_index)) != 0) {
              actor_handle =
                *(int32_t *)(conversation + 0x28 + conversation_index * 4);
              if (actor_handle != -1) {
                actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
                if (*(int32_t *)(actor + 0x18) == unit_handle) {
                  unit_matches = 1;
                }
                if (param_2 != 0) {
                  if (*(int16_t *)(actor + 0x6c) == 0xc &&
                      *(int32_t *)(actor + 0xa8) == unit_handle) {
                    *(int32_t *)(actor + 0xa8) = -1;
                  }
                  if (*(int32_t *)(actor + 0x1e0) == unit_handle) {
                    *(int32_t *)(actor + 0x1e0) = -1;
                  }
                }
              }
            }
            conversation_index = conversation_index + 1;
          } while ((int16_t)conversation_index <
                   *(int32_t *)(definition + 0x50));
        }
        if (unit_matches != 0) {
          if (*(uint8_t *)0x5aca5f != 0) {
            console_printf(0, "%s: unit died, aborting", definition);
          }
          ai_conversation_finish(iterator.datum_handle, 0, 0);
          return;
        }
      }
      conversation = (char *)data_iterator_next(&iterator);
    } while (conversation != (char *)0);
  }
}

/* ai_conversation_find_participant (0x447d0).
 * EAX carries conversation_index; five stack arguments, AL boolean return.
 * Iterator indices are at +0x14 (all actors) / +0x10 (AI index).
 * Candidate scoring, variant fallback, RNG and optional outputs follow the
 * original listing, including the distinction between assigned and nearby. */
char ai_conversation_find_participant(int16_t participant_index,
                                      char *found_actor, char *try_alternate,
                                      char *better_player_rating,
                                      float *best_distance_reference,
                                      int conversation_index)
{
  ai_conversation_datum_t *conversation;
  ai_conversation_definition_t *definition;
  ai_conversation_participant_t *participant;
  actor_t *actor;
  actor_t *placed_actor;
  unit_data_t *unit;
  unit_data_t *player_unit;
  real_point3d nearby_positions[8];
  char actor_iterator[0x1c];
  char ai_iterator[0x18];
  int16_t change_variant_indices[6];
  int16_t rejection_counts[7];
  const char *rejection_names[7];
  char actor_string[256];
  char reason_string[512];
  const char *object_name;
  int selected_actor_index;
  int actor_index;
  int object_index;
  int player_index;
  int possible_actor_count;
  int16_t selected_variant_index;
  int16_t nearby_count;
  int16_t slot;
  int16_t variant_index;
  int16_t found_variant_index;
  int16_t change_variant_count;
  int16_t unit_variant;
  int16_t name_index;
  char assigned;
  char better_rating_seen;
  char radio_selection;
  char use_object;
  char use_ai_iterator;
  char first_participant;
  char found_variant;
  char selection_valid;
  real best_score;
  real best_distance;
  real candidate_score;
  real candidate_distance;
  real player_rating;
  real nearest_distance_squared;
  x87_wide_t dx, dy, dz, distance_squared;

  conversation =
    (ai_conversation_datum_t *)datum_get(conversation_data, conversation_index);
  definition = (ai_conversation_definition_t *)tag_block_get_element(
    (char *)global_scenario_get() + 0x468, conversation->definition_index,
    0x74);
  participant = (ai_conversation_participant_t *)tag_block_get_element(
    &definition->participants, participant_index, 0x54);
  selected_variant_index = -1;
  selected_actor_index = -1;
  best_distance = 3.402823466e38f;
  better_rating_seen = 0;
  assigned = 0;
  /* original text names both pointers; the binary tests only definition */
  assert_halt_msg_at("conversation_definition && conversation",
                     "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x127a,
                     definition);

  if (participant->selection_type == 1) {
    assigned = 1;
    selected_variant_index = 0;
    goto finalize;
  }

  radio_selection = 0;
  use_object = 0;
  use_ai_iterator = 0;
  object_index = -1;
  best_score = 0.0f;
  possible_actor_count = 0;
  csmemset(rejection_counts, 0, sizeof(rejection_counts));
  if (participant->selection_type == 6 || participant->selection_type == 7) {
    radio_selection = 1;
  }
  nearby_count = 0;
  for (slot = 0; slot < definition->participants.count; slot++) {
    if (conversation->actor_indices[slot] != -1) {
      placed_actor =
        (actor_t *)datum_get(actor_data, conversation->actor_indices[slot]);
      assert_halt_msg_at(
        "nearby_unit_count < MAXIMUM_PARTICIPANTS_PER_CONVERSATION",
        "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x12ad, nearby_count < 8);
      nearby_positions[nearby_count] = placed_actor->body_position;
      nearby_count++;
    }
  }
  first_participant = nearby_count == 0;
  if (participant->preexisting_name_index != -1) {
    object_index =
      object_name_list_get_handle(participant->preexisting_name_index);
    use_object = 1;
  } else if (participant->ai_index != -1) {
    ai_index_actor_iterator_new(participant->ai_index, ai_iterator);
    use_ai_iterator = 1;
  } else {
    actor_iterator_new(actor_iterator, 1);
  }

  for (;;) {
    candidate_score = 0.0f;
    candidate_distance = 3.402823466e38f;
    player_unit = NULL;
    selection_valid = 1;
    if (use_object) {
      unit = (unit_data_t *)object_try_and_get_and_verify_type(object_index, 3);
      actor = NULL;
      actor_index = -1;
      if (unit != NULL && unit->actor_index.value != -1) {
        actor_index = unit->actor_index.value;
        actor = (actor_t *)datum_get(actor_data, actor_index);
      }
      object_index = -1;
    } else if (use_ai_iterator) {
      actor = (actor_t *)ai_index_actor_iterator_next(ai_iterator);
      actor_index = *(int *)(ai_iterator + 0x10);
    } else {
      actor = (actor_t *)actor_iterator_next(actor_iterator);
      actor_index = *(int *)(actor_iterator + 0x14);
    }
    if (actor == NULL) {
      break;
    }
    possible_actor_count++;
    if (actor->meta_unit_index == -1) {
      rejection_counts[0]++;
      continue;
    }
    if (actor->meta_type != participant->actor_type) {
      rejection_counts[1]++;
      continue;
    }
    for (slot = 0; slot < definition->participants.count; slot++) {
      if (actor_index == conversation->actor_indices[slot]) {
        break;
      }
    }
    if (slot < definition->participants.count) {
      rejection_counts[2]++;
      continue;
    }
    player_rating = ai_communication_get_player_rating(
      actor->meta_unit_index, first_participant, &player_index,
      &candidate_distance);
    HALO_FLT_ROUNDTRIP(player_rating);
    if (player_index == -1) {
      if (!radio_selection) {
        rejection_counts[3]++;
        better_rating_seen = 1;
        continue;
      }
    } else {
      candidate_score = player_rating;
      player_unit = (unit_data_t *)object_get_and_verify_type(player_index, 3);
    }
    switch (participant->selection_type) {
    case 0:
    case 6:
      /* 0xa7a30 is named friendly in kb; the binary rejects a true AL. */
      if (player_unit != NULL &&
          game_allegiance_get_team_is_friendly(
            actor->meta_team_index, player_unit->object.owner_team_index)) {
        selection_valid = 0;
      }
      break;
    case 2:
      if (player_unit == NULL ||
          player_unit->object.parent_object_index.value == -1 ||
          actor->vehicle_index !=
            player_unit->object.parent_object_index.value) {
        selection_valid = 0;
      } else if (actor->field_161) {
        candidate_score += 1.0f;
        HALO_FLT_ROUNDTRIP(candidate_score);
      }
      break;
    case 3:
      if (actor->vehicle_index != -1) {
        selection_valid = 0;
      }
      break;
    case 4:
    case 7:
      if (actor->field_01c) {
        candidate_score += 1.5f;
        HALO_FLT_ROUNDTRIP(candidate_score);
      }
      break;
    }
    if (!selection_valid) {
      rejection_counts[4]++;
      continue;
    }
    if (first_participant && !radio_selection && player_rating < 2.0f &&
        definition->run_to_player_distance == 0.0f) {
      rejection_counts[5]++;
      better_rating_seen = 1;
      continue;
    }
    if (nearby_count > 0) {
      nearest_distance_squared = 3.402823466e38f;
      for (slot = 0; slot < nearby_count; slot++) {
        dx = (x87_wide_t)nearby_positions[slot].x - actor->body_position.x;
        dy = (x87_wide_t)nearby_positions[slot].y - actor->body_position.y;
        dz = (x87_wide_t)nearby_positions[slot].z - actor->body_position.z;
        distance_squared = dz * dz + dx * dx + dy * dy;
        if (distance_squared < nearest_distance_squared) {
          nearest_distance_squared = HALO_NARROW(distance_squared);
          HALO_FLT_ROUNDTRIP(nearest_distance_squared);
        }
      }
      if (nearest_distance_squared < 20.25f) {
        candidate_score = (1.0f - (x87_sqrtd(nearest_distance_squared) - 1.5f) *
                                    0.3333333433f) +
                          candidate_score;
        HALO_FLT_ROUNDTRIP(candidate_score);
      }
    }
    unit = (unit_data_t *)object_get_and_verify_type(actor->meta_unit_index, 3);
    unit_variant = unit->object.unk_110;
    change_variant_count = 0;
    found_variant_index = -1;
    found_variant = 0;
    for (variant_index = 0; variant_index < 6; variant_index++) {
      if (participant->dialogue_variants[variant_index] != -1) {
        if (participant->dialogue_variants[variant_index] == unit_variant) {
          found_variant_index = variant_index;
          goto matching_variant;
        }
        if (participant->dialogue_variants[variant_index] == 0) {
          found_variant_index = variant_index;
          found_variant = 1;
        } else if (unit_variant < 100 &&
                   participant->dialogue_variants[variant_index] < 100) {
          assert_halt_msg_at("change_variant_indices_count < "
                             "MAXIMUM_DIALOGUE_VARIANTS_PER_CONVERSATION_"
                             "PARTICIPANT",
                             "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x13af,
                             change_variant_count < 6);
          change_variant_indices[change_variant_count++] = variant_index;
        }
      }
    }
    if (found_variant) {
    matching_variant:
      assert_halt_msg_at("found_variant_index != NONE",
                         "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x13b8,
                         found_variant_index != -1);
      variant_index = found_variant_index;
      candidate_score += 0.7f;
      HALO_FLT_ROUNDTRIP(candidate_score);
    } else {
      if (change_variant_count <= 0) {
        rejection_counts[6]++;
        continue;
      }
      if (change_variant_count == 1) {
        variant_index = change_variant_indices[0];
      } else {
        variant_index = change_variant_indices[seed_random_range(
          (unsigned int *)get_global_random_seed_address(), 0,
          change_variant_count)];
      }
      assert_halt_msg_at("(actor_variant_index >= 0) && (actor_variant_index < "
                         "MAXIMUM_DIALOGUE_VARIANTS_PER_CONVERSATION_PARTICIPANT)",
                         "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x13c7,
                         variant_index >= 0 && variant_index < 6);
    }
    if (candidate_score > best_score) {
      selected_actor_index = actor_index;
      best_score = candidate_score;
      best_distance = candidate_distance;
      selected_variant_index = variant_index;
      assigned = 1;
    }
  }

  if (!assigned && ai_print_conversations) {
    name_index = participant->preexisting_name_index;
    if (name_index == -1) {
      name_index = participant->new_name_index;
    }
    object_name = (const char *)0x254384;
    if (name_index >= 0 &&
        name_index < *(int *)((char *)global_scenario_get() + 0x204)) {
      object_name = (const char *)tag_block_get_element(
        (char *)global_scenario_get() + 0x204, name_index, 0x24);
    }
    if (use_object) {
      csstrcpy(actor_string, "<specific unit>");
    } else if (use_ai_iterator) {
      ai_index_to_string(participant->ai_index, global_scenario_get(),
                         actor_string, sizeof(actor_string));
    } else {
      csstrcpy(actor_string, "<everyone>");
    }
    console_printf(0, "%s: didn't find %d/%s in %s (%d possible actors)",
                   (char *)definition, participant_index, object_name,
                   actor_string, (int)(int16_t)possible_actor_count);
    if ((int16_t)possible_actor_count > 0) {
      rejection_names[0] = "swarm";
      rejection_names[1] = "wrong-type";
      rejection_names[2] = "already-conversing";
      rejection_names[3] = "nowhere-near-player";
      rejection_names[4] = "selection";
      rejection_names[5] = "not-near-player";
      rejection_names[6] = "no-dialogue-match";
      csstrcpy(reason_string, "  reasons: ");
      for (slot = 0; slot < 7; slot++) {
        if (rejection_counts[slot] > 0) {
          crt_sprintf(reason_string + csstrlen(reason_string), "%s(%d) ",
                      rejection_names[slot], (int)rejection_counts[slot]);
        }
      }
      console_printf(0, reason_string);
    }
  }

finalize:
  if (assigned) {
    conversation->participant_mask |= 1u << participant_index;
    conversation->actor_indices[participant_index] = selected_actor_index;
    conversation->dialogue_indices[participant_index] = selected_variant_index;
    if (found_actor != NULL && selected_actor_index != -1) {
      *found_actor = 1;
    }
  } else if ((participant->flags & 2) && try_alternate != NULL) {
    *try_alternate = 1;
  }
  if (better_rating_seen && better_player_rating != NULL) {
    *better_player_rating = 1;
  }
  if (best_distance_reference != NULL &&
      best_distance < *best_distance_reference) {
    *best_distance_reference = best_distance;
  }
  return assigned;
}
/* ai_communication_started (0x44fd0) — a unit began speaking: optional debug
 * logging, then record the speech in the dialogue timers.  DORMANT
 * (kb ported:false).
 *
 * Confirmed (disasm 0x44fd0-0x4526e):
 *   - MOVSX priority; byte table 0x45278 / jump table 0x45270: priorities
 *     0,1,2,7,10 return immediately, every other value (incl. negatives and
 *     >10) falls into the body.
 *   - Debug print gated on byte 0x5aca51, speech-state dump on 0x5aca53.
 *   - Label: actor (unit+0x1a4) -> "<encounter>/<squad>" or
 *     "<no encounter>" + actor type name; else object name (unit+0x6a, scenario
 *     block +0x204, stride 0x24); else player index (unit+0x1c8); else the
 *     unit handle.  Encounter block scenario+0x42c stride 0xb0, squads at
 *     encounter+0x80 stride 0xe8, index = MOVSX actor+0x3a.
 *   - Suffix built in the shared scratch buffer 0x5ab100 then csstrcat'ed.
 *   - comm_data is NULL-tested only for the dialogue suffix; the final
 *     byte +0xa (timers already updated) read is unguarded, as in the binary.
 *   - update_speech_timers: EAX=unit_handle, then (priority, type,
 *     zero-extended word comm_data+6, -1).
 *   - unit+0x338 is compared signed (CMP word,0 / JLE). */
void ai_communication_started(int unit_handle, uint16_t priority, uint16_t type,
                              void *comm_data)
{
  unit_data_t *unit;
  actor_t *actor;
  encounter_definition *encounter;
  ai_information_packet_t *information;
  const char *vocalization_name;

  information = (ai_information_packet_t *)comm_data;
  switch ((short)priority) {
  case 0:
  case 1:
  case 2:
  case 7:
  case 10:
    break;
  default:
    if (ai_print_vocalizations) {
      char string[1024];
      char name[256];

      unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
      if (unit->actor_index.value != -1) {
        actor = (actor_t *)datum_get(actor_data, unit->actor_index.value);
        if (actor->field_034 == (uint32_t)-1) {
          csstrcpy(name, "<no encounter>");
        } else {
          /* field_034 = encounter index, field_03a = squad index */
          encounter = (encounter_definition *)tag_block_get_element(
            &global_scenario_get()->ai_encounters, actor->field_034 & 0xffff,
            sizeof(encounter_definition));
          crt_sprintf(name, "%s/%s", encounter->name,
                      ((squad_definition_t *)tag_block_get_element(
                         &encounter->squads, actor->field_03a,
                         sizeof(squad_definition_t)))->name);
        }
        crt_sprintf(string, "%s/%s: ", name,
                    actor_type_get_name(actor->meta_type));
      } else if (unit->object.unk_106 != -1) {
        /* unk_106 = scenario object name index */
        crt_sprintf(string, "%s: ",
                    ((scenario_object_name_t *)tag_block_get_element(
                       &global_scenario_get()->object_names,
                       unit->object.unk_106, sizeof(scenario_object_name_t)))
                      ->name);
      } else if (unit->unk_456.value != -1) {
        crt_sprintf(string, "player %d: ", unit->unk_456.value & 0xffff);
      } else {
        crt_sprintf(string, "unit %04X: ", unit_handle & 0xffff);
      }
      if (type == 0xffff) {
        vocalization_name = "non-voc";
      } else {
        vocalization_name = dialogue_get_vocalization_name(type, 0);
      }
      crt_sprintf(error_string_buffer, "%s %s",
                  unit_get_speech_priority_name(priority), vocalization_name);
      csstrcat(string, error_string_buffer);
      if (information != NULL && information->dialogue_type_index != -1) {
        crt_sprintf(
          error_string_buffer, " [%d/%s]", (int)information->dialogue_type_index,
          ai_communication_get_type_name(
            global_dialogue_table[information->dialogue_type_index]
              .communication_type));
        csstrcat(string, error_string_buffer);
      }
      console_printf(0, string);
    }
    if (ai_print_speech) {
      char actor_string[512];
      char speech_string[512];

      unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
      /* unit +0x338 = current speech item priority, compared signed */
      if ((int16_t)unit->unk_824 > 0) {
        error(2, "%s: %s",
              ai_debug_describe_actor(unit->actor_index.value, unit_handle, 0,
                                      actor_string, 0x200),
              unit_describe_speech(unit_handle, 1, 0x200, speech_string));
      }
    }
    /* unguarded dereference, as in the binary */
    if (!information->updated_dialogue_timers) {
      ai_communication_update_speech_timers(unit_handle, priority, type,
                                            information->dialogue_type_index,
                                            -1);
    }
    break;
  }
}

/* ai_communication_notify (0x45290) — broadcast a unit's communication
 * record to every actor that can hear it.
 *
 * Confirmed (disasm 0x45290-0x45498):
 *   - cdecl, plain RET; both callers (0x48b57 via 0x44fd0's caller shape,
 *     0x1a7a25 in units.c) push 4 dwords.  Only EBP+0x8 (unit_handle) and
 *     EBP+0x14 (comm_data) are read; priority/type are unused here.
 *   - comm_data+0x14 is a signed word: MOVSX/DEC/JNE selects the ==1 case,
 *     which forwards the zero-extended words +0x18/+0x1a and byte +0x1c to
 *     ai_handle_allegiance_broken_notification (0x40150).
 *   - The broadcast runs when word +0x14 != 0 or signed word +0x0c > 0.
 *   - comm_data+0x06 is `ai_information->dialogue_type_index` per the assert
 *     string at 0x259f78 (line 0x894); when it is not -1 and the dialogue
 *     table entry's word at +0x2 (table 0x257e48, stride 0x28) is >= 4 the
 *     audibility volume argument is raised from 1 to 3.
 *   - The sound location defaults to unit+0x48; when unit+0xcc != -1 it is
 *     the root parent object's +0x48 instead.
 *   - Per actor (actor_iterator, handle at iter+0x14, reloaded after every
 *     call): skip the actor driving this unit, skip friendly teams
 *     (actor+0x3e vs unit+0x68), and skip when the squared distance from the
 *     unit head position to actor+0x120 exceeds 900.0 (0x254e00; FCOMP /
 *     TEST AH,0x41 / JE skip).  Otherwise the actor's prop for this unit is
 *     fetched and, if actor_audibility_at_point returns >= 2 (CMP AX,2 — a
 *     16-bit result), actor_handle_communication and
 *     ai_communication_handle_received_looking (@<ecx>=comm_data,
 *     @<eax>=prop_handle) are called.
 *   - actor_audibility_at_point's 6th argument is PUSH 0x3f800000 (1.0f);
 *     the callee (0x31850) never reads that slot. */
void ai_communication_notify(int unit_handle, uint16_t priority, uint16_t type,
                             void *comm_data)
{
  ai_information_packet_t *information;
  unit_data_t *unit;
  actor_t *actor;
  prop_t *prop;
  int prop_handle;
  short dialogue_type_index;
  float sense_block[14];
  actor_iterator_t actors;
  real_point3d speaker_head;
  short team;
  void *location;
  short volume;
  int parent_object_index;

  information = (ai_information_packet_t *)comm_data;
  switch (information->information_type) {
  case 1: /* allegiance information */
    ai_handle_allegiance_broken_notification(
      (uint16_t)information->information_data.allegiance.team1_index,
      (uint16_t)information->information_data.allegiance.team2_index,
      (uint8_t)information->information_data.allegiance.broken);
    break;
  }
  if (information->information_type != 0 || information->look_priority > 0) {
    unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
    location = &unit->object.unk_72; /* object location, +0x48 */
    team = unit->object.owner_team_index;
    volume = 1;
    unit_get_head_position(unit_handle, (float *)&speaker_head);
    dialogue_type_index = information->dialogue_type_index;
    if (dialogue_type_index != -1) {
      assert_halt_msg_at(
        "(ai_information->dialogue_type_index >= 0) && "
        "(ai_information->dialogue_type_index < global_dialogue_event_count)",
        "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x894,
        dialogue_type_index >= 0 &&
          dialogue_type_index < global_dialogue_event_count);
      if (global_dialogue_table[information->dialogue_type_index]
            .communication_priority >= 4) {
        volume = 3;
      }
    }
    if (unit->object.parent_object_index.value != -1) {
      parent_object_index = object_get_root_parent(unit_handle);
      location = &((object_data_t *)object_get_and_verify_type(
                     parent_object_index, -1))->unk_72;
    }
    actor_iterator_new(&actors, 1);
    actor = (actor_t *)actor_iterator_next(&actors);
    while (actor != NULL) {
      real_vector3d vector;

      if (actor->meta_unit_index != unit_handle &&
          !game_allegiance_get_team_is_friendly(actor->field_03e, team) &&
          (vector_from_points3d(&actor->head_position, &speaker_head,
                                &vector),
           !(vector.j * vector.j + vector.i * vector.i +
               vector.k * vector.k > 900.0f))) {
        prop_handle =
          prop_get_base_by_unit_index(actors.index, unit_handle, 1, 1);
        if (prop_handle != -1) {
          prop = (prop_t *)datum_get(prop_data, prop_handle);
          actor_perception_find_sense_position(
            actors.index, (float *)&speaker_head, prop_handle, sense_block);
          if ((short)actor_audibility_at_point(
                actors.index, sense_block, (float *)&speaker_head, location,
                volume,
                0x3f800000 /* the 1.0f bit pattern; kb types this slot int */,
                (uint16_t)prop->line_of_sight) >= 2) {
            actor_handle_communication(actors.index, prop_handle, (int)comm_data);
            ai_communication_handle_received_looking(actors.index, comm_data,
                                                     prop_handle);
          }
        }
      }
      actor = (actor_t *)actor_iterator_next(&actors);
    }
  }
}

/* ai_communication_actor_talk_weight (0x454a0).
 * subject_index is in EAX; return is ST(0), including all zero exits.
 * Existing callers forward stimulus_range_bits as a raw dword. Preserve that
 * ABI and reinterpret its IEEE float payload rather than numerically cast it.
 * The remaining stack carriers are narrowed exactly at the original word/byte
 * reads. consider_speech receives EAX=&sound, ECX=&vocalization, DX=priority.
 */
float ai_communication_actor_talk_weight(
  int subject_index, int actor_index, float *subject_point, int cause_index,
  float *cause_point, int stimulus_range_bits, int communication_type,
  int communication_priority, int speech_priority, int vocalization_type,
  int animation_type, int flags)
{
  actor_t *actor;
  unit_data_t *subject;
  prop_t *prop;
  union {
    int bits;
    real value;
  } range;
  real weight;
  real player_rating;
  x87_wide_t dx, dy, dz;
  int unit_index;
  int prop_index;
  int sound_index;
  int16_t speech_type;
  int16_t visibility;
  char lighting;
  char have_target;
  char passes;
  char subject_ok;
  char cause_ok;

  actor = (actor_t *)datum_get(actor_data, actor_index);
  range.bits = stimulus_range_bits;
  have_target = subject_index != -1 || cause_index != -1;
  unit_index = actor->meta_unit_index;
  passes = unit_index != -1;
  weight = 10.0f;
  if (actor->field_06a <= 1 || !passes) {
    return 0.0f;
  }
  if (have_target) {
    if (subject_index != -1) {
      dx = (x87_wide_t)subject_point[0] - actor->head_position.x;
      dy = (x87_wide_t)subject_point[1] - actor->head_position.y;
      dz = (x87_wide_t)subject_point[2] - actor->head_position.z;
      if ((x87_wide_t)range.value * range.value > dx * dx + dy * dy + dz * dz) {
        goto in_range;
      }
    }
    if (cause_index == -1 ||
        !((x87_wide_t)range.value * range.value >
          distance_squared3d((float *)&actor->head_position, cause_point))) {
      return 0.0f;
    }
  in_range:
    passes = 1;
  }
  if (flags & 2) {
    player_rating =
      ai_communication_get_player_rating(unit_index, 0, NULL, NULL);
    if (player_rating == 0.0f) {
      return 0.0f;
    }
    weight = player_rating * 5.0f + 10.0f;
    HALO_FLT_ROUNDTRIP(weight);
  }
  if ((flags & 4) && subject_index != -1) {
    subject = (unit_data_t *)object_get_and_verify_type(subject_index, 3);
    if (subject->object.parent_object_index.value != actor->vehicle_index) {
      return 0.0f;
    }
  }
  if ((int16_t)animation_type != -1 &&
      (uint8_t)unit_test_animation_impulse(unit_index, animation_type)) {
    weight += 5.0f;
    HALO_FLT_ROUNDTRIP(weight);
  }
  if ((int16_t)vocalization_type != -1) {
    speech_type = (int16_t)vocalization_type;
    sound_index = -1;
    if (ai_communication_consider_speech(
          &sound_index, &speech_type, (short)speech_priority, unit_index,
          (short)communication_priority, 0, (char)(flags & 1), 1, &weight,
          NULL) == 0) {
      return 0.0f;
    }
  }
  if (have_target) {
    subject_ok = 0;
    cause_ok = 0;
    if (subject_index != -1) {
      if (actor->meta_unit_index == subject_index) {
        if (flags & 8) {
          subject_ok = 1;
        } else {
          passes = 0;
        }
      } else {
        prop_index =
          prop_get_base_by_unit_index(actor_index, subject_index, 1, 0);
        if (prop_index != -1) {
          prop = (prop_t *)datum_get(prop_data, prop_index);
          /* The x87 TEST AH,0x41 also admits unordered comparisons here. */
          if (!(prop->distance > range.value)) {
            if (prop->state < 2 || prop->state > 3) {
              if (prop->enemy) {
                goto check_cause;
              }
              if ((int16_t)communication_type == 0 && prop->audibility < 2 &&
                  prop->ineffability < 2) {
                lighting = prop->flashlight ? 2 : prop->lighting;
                visibility = actor_visibility_at_point(
                  actor_index, &actor->head_position,
                  (float *)&prop->head_position, lighting, prop->line_of_sight,
                  1, 0,
                  actor_get_perception_knowledge(actor_index, prop_index));
                if (visibility < 2) {
                  goto check_cause;
                }
              }
            }
            weight = (1.0f - (x87_wide_t)prop->distance / range.value) * 10.0f +
                     weight;
            HALO_FLT_ROUNDTRIP(weight);
            subject_ok = 1;
          }
        }
      }
    }
  check_cause:
    if (cause_index != -1) {
      if (actor->meta_unit_index == cause_index) {
        if (!(flags & 0x10)) {
          return 0.0f;
        }
        cause_ok = 1;
      } else {
        prop_index = prop_get_active_by_unit_index(actor_index, cause_index);
        if (prop_index != -1) {
          prop = (prop_t *)datum_get(prop_data, prop_index);
          if (!(prop->distance > range.value) &&
              ((prop->state >= 2 && prop->state <= 3) || !prop->enemy)) {
            weight = (1.0f - (x87_wide_t)prop->distance / range.value) * 10.0f +
                     weight;
            HALO_FLT_ROUNDTRIP(weight);
            cause_ok = 1;
          }
        }
      }
    }
    if (!passes) {
      return 0.0f;
    }
    passes = subject_ok | cause_ok;
  }
  return passes ? weight : 0.0f;
}

/* ai_communication_find_specific_actor_to_talk (0x45830) — scan the actors
 * selected by one ai_index reference and return the datum handle of the
 * best-scoring conversation partner.
 *
 * Same scoring shape as ai_communication_find_global_actor_to_talk (0x458f0)
 * but driven by the ai_index actor iterator instead of the global
 * encounter/actor walk, and with no team filtering.
 *
 * Confirmed (disasm 0x45830-0x458ee):
 *   Register params: EAX = ai_index (MOV EBX,EAX at 0x45837, then CMP EBX,-1
 *   and PUSH EBX into ai_index_actor_iterator_new at 0x45877); EDI and ESI are
 *   never saved by the prologue (only PUSH EBX at 0x45836), so both are
 *   inbound parameters. EDI feeds unit_get_head_position at 0x45859 and the
 *   @<eax> arg of FUN_000454a0 (MOV EAX,EDI at 0x458b6); ESI feeds
 *   unit_get_head_position at 0x4586b and callee arg 4 (PUSH ESI at 0x458b0).
 *   Stack params: [EBP+0x08] param_1 .. [EBP+0x20] param_7, forwarded
 *   unchanged as FUN_000454a0 args 6..12.
 *   Return: EAX = [EBP-0x4] (MOV EAX,[EBP-0x4] at 0x458e7). The early-out at
 *   0x45849 jumps past that load to 0x458ea with EAX still holding the
 *   OR EAX,0xffffffff from 0x45839 — same -1 value, so a single C return of
 *   best_handle is faithful.
 * Confirmed frame (SUB ESP,0x38 = 56 bytes):
 *   [EBP-0x38] iter        0x18 bytes — ai_index actor iterator, layout B
 *                          (6 ints); current actor handle at iter+0x10,
 *                          read at 0x458a8 and reloaded at 0x458ca.
 *   [EBP-0x20] vec_a       float[3] — head position of the EDI object.
 *   [EBP-0x14] vec_b       float[3] — head position of the ESI object.
 *   [EBP-0x08] best_score  float
 *   [EBP-0x04] best_handle int
 * Confirmed: ADD ESP,0xc at 0x45886 cleans ai_index_actor_iterator_new(8) plus
 *   the first ai_index_actor_iterator_next(4) together — cdecl cleanup
 *   mis-grouping, not a 3-arg call (the artifact's ARG_COUNT hazard on
 *   0x45881 is this same mis-grouping; the second call site at 0x458db has a
 *   plain ADD ESP,0x4).
 * Confirmed: ADD ESP,0x2c at 0x458c0 = 11 stack dwords into FUN_000454a0; the
 *   push sequence 0x45899-0x458b5 reverses to
 *   (iter+0x10, vec_a, ESI, vec_b, param_1..param_7).
 * Confirmed: FCOM [EBP-0x8] / FNSTSW AX / TEST AH,0x41 / JNZ at 0x458c8 keeps
 *   the candidate only when the returned score is strictly greater than
 *   best_score (C3 and C0 both clear); the reject arm is FSTP ST0 at 0x458d5.
 * Confirmed: the iterator's returned record pointer is only tested for
 *   non-zero (TEST EAX,EAX at 0x45889 / 0x458e3); it is never dereferenced.
 * Unknown: the meaning of param_1..param_7 — they are forwarded verbatim into
 *   the unlifted scorer FUN_000454a0 and never inspected here.
 */
int ai_communication_find_specific_actor_to_talk(
  int param_1, int param_2, int param_3, int param_4, int param_5, int param_6,
  int param_7, unsigned int ai_index, int other_object_handle,
  int object_handle)
{
  char iter[0x18];
  float vec_a[3];
  float vec_b[3];
  float best_score;
  int best_handle;
  float score;

  best_handle = -1;
  best_score = 0.0f;
  if (ai_index != 0xffffffff) {
    if (object_handle != -1) {
      unit_get_head_position(object_handle, vec_a);
    }
    if (other_object_handle != -1) {
      unit_get_head_position(other_object_handle, vec_b);
    }
    ai_index_actor_iterator_new(ai_index, iter);
    while (ai_index_actor_iterator_next(iter) != 0) {
      score = ai_communication_actor_talk_weight(
        object_handle, *(int *)(iter + 0x10), vec_a, other_object_handle, vec_b,
        param_1, param_2, param_3, param_4, param_5, param_6, param_7);
      if (score > best_score) {
        best_score = score;
        best_handle = *(int *)(iter + 0x10);
      }
    }
  }
  return best_handle;
}

/* ai_communication_find_global_actor_to_talk (0x458f0) — scan every live actor
 * and return the datum handle of the best-scoring conversation partner.
 *
 * Walks the global encounter/actor iterator, filters candidates by team
 * relationship, scores each survivor with ai_communication_actor_talk_weight,
 * and keeps the highest score (strictly greater). Returns -1 when nothing
 * scores above 0.0f.
 *
 * Confirmed (disasm 0x458f0-0x45a08):
 *   Register params: EDI = object_handle (never saved in the prologue),
 *   BX = team (CMP BX,-0x1 at 0x45948). Only ESI is saved (PUSH ESI 0x458f6),
 *   so EDI/EBX are inbound parameters.
 *   Stack params: [EBP+0x08] param_1 int16_t (MOVSX at 0x4595b, 0/1/2
 * selector), [EBP+0x0c] param_2 .. [EBP+0x28] param_9. Return: EAX = [EBP-0x4]
 * (MOV EAX,[EBP-0x4] at 0x45a01). Confirmed frame (SUB ESP,0x3c = 60 bytes):
 *   [EBP-0x3c] iter        0x1c bytes (actor handle at iter+0x14 = [EBP-0x28],
 *                          same convention as actors_move_randomly)
 *   [EBP-0x20] vec_b       float[3]  (address-only; never written here)
 *   [EBP-0x14] vec_a       float[3]  (unit_get_head_position destination)
 *   [EBP-0x08] best_score  float
 *   [EBP-0x04] best_handle int
 * Confirmed: both unit_get_head_position calls push EDI and LEA [EBP-0x14]
 *   (0x4590b-0x4590d and 0x4591d-0x4591f) — the second is guarded on
 *   param_2 != -1 yet still passes object_handle into the same buffer. That
 *   duplicate is what the binary does; it is preserved deliberately.
 * Confirmed: ADD ESP,0xc at 0x4593d cleans actor_iterator_new(8) +
 *   actor_iterator_next(4) together (cdecl cleanup mis-grouping).
 * Confirmed: game_allegiance_get_team_is_friendly runs before the selector
 *   dispatch (CALL 0x45956, MOVSX 0x4595b), so its side effect happens in
 *   every mode even where mode 0 discards the result.
 * Confirmed selector dispatch at 0x45962-0x4599c:
 *   0 -> match = (actor->field_03e == team)  (CMP word[ESI+0x3e],BX / SETZ)
 *   1 -> match = !friendly                   (TEST AL,AL / SETZ)
 *   2 -> match = friendly                    (falls through to TEST AL,AL)
 *   else -> assert "!\"unreachable\"" line 0xdfd = 3581, then system_exit(-1).
 * Confirmed: ADD ESP,0x2c at 0x459d4 = 11 stack dwords into
 * ai_communication_actor_talk_weight, with MOV EAX,EDI at 0x459ca supplying its
 * @<eax> register arg. cdecl push order (0x459a7-0x459c9) reverses to
 *   (iter+0x14, vec_a, param_2, vec_b, param_3..param_9).
 * Confirmed: FCOM [EBP-0x8] / FNSTSW AX / TEST AH,0x41 / JNZ at 0x459d1 keeps
 *   the candidate only when the returned score is strictly greater than
 *   best_score (C3|C0 clear); the reject arm is FSTP ST0 at 0x459e9.
 * Confirmed: ai_communication_actor_talk_weight prologue (PUSH EBP / MOV
 * EBP,ESP / SUB ESP,0x10 / PUSH EBX / PUSH ESI / MOV ESI,EAX / PUSH EDI) saves
 * EBX, ESI and EDI, so EAX is its only register parameter.
 */
int ai_communication_find_global_actor_to_talk(int16_t param_1, int param_2,
                                               int param_3, int param_4,
                                               int param_5, int param_6,
                                               int param_7, int param_8,
                                               int param_9, int object_handle,
                                               int16_t team)
{
  char iter[0x1c];
  float vec_b[3];
  float vec_a[3];
  float best_score;
  volatile int best_handle;
  char *actor_record;
  bool match;
  float score;

  best_handle = -1;
  best_score = 0.0f;
  if (object_handle != -1) {
    unit_get_head_position(object_handle, vec_a);
  }
  if (param_2 != -1) {
    /* Binary-faithful: the guard tests param_2 but the call still passes
     * object_handle into vec_a, exactly as at 0x4591a-0x4591f. */
    unit_get_head_position(object_handle, vec_a);
  }
  actor_iterator_new(iter, 1);
  while ((actor_record = (char *)actor_iterator_next(iter)) != NULL) {
    match = 1;
    if (team != -1) {
      match = game_allegiance_get_team_is_friendly(
        team, ((actor_t *)actor_record)->field_03e);
      switch (param_1) {
      case 0:
        match = (((actor_t *)actor_record)->field_03e == team);
        break;
      case 1:
        match = !match;
        break;
      case 2:
        break;
      default:
        display_assert("!\"unreachable\"",
                       "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xdfd, 1);
        system_exit(-1);
        break;
      }
    }
    if (match) {
      score = ai_communication_actor_talk_weight(
        object_handle, *(int *)(iter + 0x14), vec_a, param_2, vec_b, param_3,
        param_4, param_5, param_6, param_7, param_8, param_9);
      if (score > best_score) {
        best_score = score;
        best_handle = *(int *)(iter + 0x14);
      }
    }
  }
  return best_handle;
}

/* ai_conversation_begin (0x45a10) — try to start a scenario conversation.
 * Resolves every participant through ai_conversation_find_participant,
 * rejects the start when a required participant is missing (can_begin = 0),
 * not yet ready, or (definition flag 0x40) outside the trigger distance
 * (keep_trying = 1).  Definition flag 0x10 requires some participant to see
 * a player; flag 0x80 requires a player to look at a participant.  On success
 * every assigned participant's unit gets its dialogue variant (+0x6e) set and
 * conversation[6] = 1.  On failure *out_keep_trying receives
 * keep_trying && (definition flag 0x40).  Returns can_begin.
 *
 * Confirmed (live Ghidra disasm 0x45a10-0x460d6; bundle artifact held only
 * connection-error payloads):
 *   - The participant finder at 0x447d0 takes the conversation handle
 *     in EAX (MOV EAX,[EBP+8] before each CALL; callee PUSH EAX at 0x447e2 into
 *     datum_get) plus 5 cdecl args (ADD ESP,0x14): index, &any_assigned,
 *     &saw_flag2 (NULL in the second pass, PUSH 0 at 0x45b55),
 *     &participant_flag, &min_distance.  Return (AL) is only tested in the
 *     second pass (0x45b68).
 *   - Out-flag byte [EBP-2] is reused by the third loop as "missing"; float
 *     [EBP-0x1c] and dword [EBP-0x10] are reused per block (block locals).
 *   - FCOMP/TEST AH,0x41/JNZ at 0x45cbf and 0x45cd3: def+0x24 > 0.0f and
 *     min_distance > def+0x24; 0x45e1c TEST AH,5/JP: dist < best.
 *   - console_printf 0x45ce4: (0, fmt, definition, (double)min_distance,
 *     (double)def+0x24) from FSTP [ESP] / FSTP [ESP+8].
 *   - unit_can_see_point third arg is PUSH 0x3f060a92 (0.5235988f).
 *   - assert string 0x259ff0 / line 0x1216: conv+0x18 is dialogue_indices
 *     (int16, < 6).
 * Uncertain: meaning of find_participant out-flags 2/3/4 beyond the observed
 * writes, definition flags 0x10/0x20/0x40/0x80, unit +0x6e / +0x1b4 bit 8. */
char ai_conversation_begin(int conversation_handle, char *out_keep_trying)
{
  int state_data[33];
  data_iter_t iter;
  unsigned int ready_mask;
  char retry_flag;
  float min_distance;
  char found_not_ready;
  char *conversation;
  int16_t i;
  char keep_trying;
  char *definition;
  char any_assigned;
  char saw_flag2;
  char participant_flag;
  char can_begin;
  int *participants;
  char *participant;
  unsigned int bit;
  uint16_t flags;

  conversation = (char *)datum_get(*(data_t **)0x6324ec, conversation_handle);
  definition =
    (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                  *(int16_t *)(conversation + 2), 0x74);
  ready_mask = 0;
  saw_flag2 = '\0';
  retry_flag = '\0';
  any_assigned = '\0';
  can_begin = '\1';
  keep_trying = '\0';
  min_distance = 3.4028235e38f;
  *(uint32_t *)(conversation + 0x14) = 0;
  csmemset(conversation + 0x28, -1, 0x20);
  csmemset(conversation + 0x18, -1, 0x10);
  participants = (int *)(definition + 0x50);
  if (*participants > 0) {
    i = 0;
    do {
      participant = (char *)tag_block_get_element(participants, i, 0x54);
      if ((participant[2] & 4) == 0) {
        participant_flag = '\0';
        ai_conversation_find_participant(i, &any_assigned, &saw_flag2,
                                         &participant_flag, &min_distance,
                                         conversation_handle);
        if (participant_flag != '\0') {
          ready_mask |= 1 << i;
        } else {
          ready_mask &= ~(1 << i);
        }
      }
      i++;
    } while (i < *participants);
    if (saw_flag2 != '\0') {
      for (i = 0; i < *participants; i++) {
        participant = (char *)tag_block_get_element(participants, i, 0x54);
        bit = 1 << i;
        if ((*(uint32_t *)(conversation + 0x14) & bit) == 0 &&
            (participant[2] & 4) != 0) {
          participant_flag = '\0';
          if (ai_conversation_find_participant(i, &any_assigned, NULL,
                                               &participant_flag, &min_distance,
                                               conversation_handle) != '\0') {
            retry_flag = '\1';
          } else if (participant_flag != '\0') {
            ready_mask |= bit;
          } else {
            ready_mask &= ~bit;
          }
        }
      }
    }
  }
  found_not_ready = '\0';
  participant_flag = '\0';
  for (i = 0; i < *participants; i++) {
    participant = (char *)tag_block_get_element(participants, i, 0x54);
    flags = *(uint16_t *)(participant + 2);
    if ((flags & 1) == 0 &&
        (*(uint32_t *)(conversation + 0x14) & (1 << i)) == 0 &&
        ((flags & 2) == 0 || retry_flag == '\0') &&
        ((flags & 4) == 0 || saw_flag2 != '\0')) {
      bit = (1 << i) & ready_mask;
      if (bit != 0) {
        found_not_ready = '\1';
      } else {
        participant_flag = '\1';
      }
      if (*(char *)0x5aca5f != '\0') {
        const char *name;
        int16_t name_index;
        int index;

        name = (const char *)0x254384;
        name_index = *(int16_t *)(participant + 8);
        if (name_index == -1) {
          name_index = *(int16_t *)(participant + 0xa);
        }
        if (name_index >= 0) {
          index = name_index;
          if (index < *(int *)((char *)global_scenario_get() + 0x204)) {
            name = (const char *)tag_block_get_element(
              (char *)global_scenario_get() + 0x204, index, 0x24);
          }
        }
        console_printf(0,
                       bit != 0 ?
                         "%s: found participant %d/%s but not ready to "
                         "talk yet" :
                         "%s: could not find participant %d/%s",
                       definition, (int)i, name);
      }
      break;
    }
  }
  if (participant_flag != '\0') {
    can_begin = '\0';
  } else if (found_not_ready != '\0') {
    keep_trying = '\1';
    can_begin = '\0';
  } else if ((definition[0x20] & 0x40) != 0 &&
             *(float *)(definition + 0x24) > 0.0f && any_assigned != '\0' &&
             min_distance > *(float *)(definition + 0x24)) {
    if (*(char *)0x5aca5f != '\0') {
      console_printf(0,
                     "%s: participants currently outside trigger-dist %.1f > "
                     "%.1f, must wait",
                     definition, (double)min_distance,
                     (double)*(float *)(definition + 0x24));
    }
    keep_trying = '\1';
    can_begin = '\0';
  }
  *(int32_t *)(conversation + 0x10) = -1;
  if (can_begin != '\0') {
    if ((definition[0x20] & 0x10) != 0) {
      if (any_assigned == '\0') {
        can_begin = '\0';
      } else {
        float best_distance;
        char *player;

        best_distance = 3.4028235e38f;
        data_iterator_new(&iter, player_data);
        player = (char *)data_iterator_next(&iter);
        while (player != NULL) {
          if (*(int *)(player + 0x34) != -1) {
            float distance;
            int16_t j;
            int prop_handle;
            char *prop;

            distance = 3.4028235e38f;
            for (j = 0; j < *(int *)(definition + 0x50); j++) {
              prop_handle = *(int *)(conversation + j * 4 + 0x28);
              if (prop_handle != -1 &&
                  (prop_handle = prop_get_active_by_unit_index(
                     prop_handle, *(int *)(player + 0x34))) != -1) {
                prop = (char *)datum_get(prop_data, prop_handle);
                if (*(int16_t *)(prop + 0x24) >= 2 &&
                    *(int16_t *)(prop + 0x24) <= 3 &&
                    distance > *(float *)(prop + 0x11c)) {
                  distance = *(float *)(prop + 0x11c);
                }
              }
            }
            if (distance < best_distance) {
              *(int *)(conversation + 0x10) = *(int *)(player + 0x34);
              best_distance = distance;
            }
          }
          player = (char *)data_iterator_next(&iter);
        }
        if (*(int *)(conversation + 0x10) == -1) {
          if (*(char *)0x5aca5f != '\0') {
            console_printf(0, "%s: cannot start, nobody can see player",
                           definition);
          }
          if ((definition[0x20] & 0x40) != 0) {
            keep_trying = '\1';
          }
          can_begin = '\0';
        }
      }
    }
    if (can_begin != '\0' && (definition[0x20] & 0x80) != 0 &&
        any_assigned != '\0') {
      char *player;
      char looking;

      looking = '\0';
      data_iterator_new(&iter, player_data);
      player = (char *)data_iterator_next(&iter);
      while (player != NULL && looking == '\0') {
        if (*(int *)(player + 0x34) != -1) {
          int16_t j;
          int actor_handle;

          for (j = 0; j < *(int *)(definition + 0x50); j++) {
            actor_handle = *(int *)(conversation + j * 4 + 0x28);
            if (actor_handle != -1 &&
                unit_can_see_point(
                  *(int *)(player + 0x34),
                  (float *)((char *)datum_get(*(data_t **)0x6325a4,
                                              actor_handle) +
                            0x120),
                  0.5235988f) != '\0') {
              looking = '\1';
              break;
            }
          }
        }
        player = (char *)data_iterator_next(&iter);
      }
      if (looking == '\0') {
        if (*(char *)0x5aca5f != '\0') {
          console_printf(0, "%s: cannot start, players are not looking at us",
                         definition);
        }
        if ((definition[0x20] & 0x40) != 0) {
          keep_trying = '\1';
        }
        can_begin = '\0';
      }
    }
    if (can_begin != '\0') {
      int16_t j;

      for (j = 0; j < *(int *)(definition + 0x50); j++) {
        if ((*(uint32_t *)(conversation + 0x14) & (1 << j)) != 0 &&
            *(int *)(conversation + j * 4 + 0x28) != -1) {
          char *actor;
          char *unit;
          int16_t variant;

          participant =
            (char *)tag_block_get_element(definition + 0x50, j, 0x54);
          actor = (char *)datum_get(*(data_t **)0x6325a4,
                                    *(int *)(conversation + j * 4 + 0x28));
          unit = (char *)object_get_and_verify_type(*(int *)(actor + 0x18), 3);
          if (*(int16_t *)(participant + 0xa) != -1) {
            object_name_list_set_handle(*(int16_t *)(participant + 0xa),
                                        *(int *)(actor + 0x18));
          }
          if ((definition[0x20] & 0x20) != 0 &&
              action_converse_setup(*(int *)(conversation + j * 4 + 0x28),
                                    conversation_handle, state_data) != '\0') {
            actor_action_change(*(int *)(conversation + j * 4 + 0x28), 0xc,
                                (int)state_data);
          }
          if (!(*(int16_t *)(conversation + j * 2 + 0x18) >= 0 &&
                *(int16_t *)(conversation + j * 2 + 0x18) < 6)) {
            display_assert(
              "(conversation->dialogue_indices[index] >= 0) && "
              "(conversation->dialogue_indices[index] < "
              "MAXIMUM_DIALOGUE_VARIANTS_PER_CONVERSATION_PARTICIPANT)",
              "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x1216, 1);
            system_exit(-1);
          }
          variant =
            *(int16_t *)(participant +
                         *(int16_t *)(conversation + j * 2 + 0x18) * 2 + 0x18);
          if (*(int16_t *)(unit + 0x6e) != variant) {
            *(int16_t *)(unit + 0x6e) = variant;
            *(uint32_t *)(unit + 0x1b4) &= 0xfffffeff;
          }
        }
      }
      conversation[6] = '\1';
      return can_begin;
    }
  }
  if (keep_trying != '\0' && (definition[0x20] & 0x40) != 0) {
    *out_keep_trying = '\1';
  } else {
    *out_keep_trying = '\0';
  }
  return can_begin;
}

/* ai_communication_find_actor_to_reply_to_player (0x460e0) — walk the reply
 * table at 0x258eb0 (0x24-byte entries, sentinel field_00 == -1) for entries
 * whose field_00 equals param_3 and whose field_02 is -1 or equals param_4.
 * Each matching entry rolls its chance (+0x14) and then picks a replier by
 * entry type (+0x04): 2/4 -> ai_communication_find_global_actor_to_talk with
 * mode 1/2 around unit_handle; 3 -> the actor of param_2's unit (+0x1a4).
 * A found actor is rejected while its team's reply timer (+4 of the
 * 0x331f14 slot) is still in the future.  The first accepted actor stops the
 * walk.  *out_scale (when non-NULL) receives the time-since-last-reply
 * scale clamped to [0,1] (default 1.0).  Returns the actor handle or -1.
 *
 * Confirmed (live Ghidra disasm 0x460e0-0x46521; bundle artifact held only
 * connection-error payloads):
 *   - Five cdecl stack args: [EBP+8] int (object_get_and_verify_type(.,3)),
 *     [EBP+0xc] int (object_try_and_get_and_verify_type(.,3)), [EBP+0x10]
 *     and [EBP+0x14] compared with CMP word, [EBP+0x18] float * written at
 *     0x46519.  EAX = [EBP-8] at 0x4651b is the returned actor handle.
 *   - find_global_actor_to_talk pushes (0x462ad-0x462df): mode, -1,
 *     0x41100000 (9.0f bits; callee param is int), -1, zero-extended
 *     entry+0x0a, the short read from 0x257c68[entry+0x0a] (EBX), zero-
 *     extended entry+0x06, zero-extended entry+0x08, 0; EDI = unit_handle,
 *     BX = unit+0x68.  ADD ESP,0x2c also covers object_get_and_verify_type.
 *   - chance test FCOMP [0x2533c0]/TEST AH,0x41/JNZ: proceed only when
 *     chance > 0.0f.  Roll FLD r/FCOMP chance/TEST AH,5/JP: accept only
 *     when r < chance (NaN rejects).
 *   - scale: FILD (now - slot[0]) * [0x25620c]; TEST AH,5/JP: < 0 -> 0.0f;
 *     TEST AH,0x41/JNZ: > [0x2533c8] -> 1.0f.
 *   - Final error(2, buffer) passes the accumulated buffer as the format.
 * Uncertain: meanings of param_2..param_4 and entry fields +0x06/+0x08/+0x0a
 *   beyond their observed uses; the 0x257c68 table; global 0x5aca44 bit
 *   vector semantics (suppresses the final log). */
int ai_communication_find_actor_to_reply_to_player(int unit_handle, int param_2,
                                                   int16_t param_3,
                                                   int16_t param_4,
                                                   float *out_scale)
{
  char buffer[1024];
  int now;
  int reply_index;
  float roll;
  float scale;
  int actor_handle;
  char print_log;
  char *entry;
  int16_t vocalization_value;
  int16_t team;
  int32_t *slot;
  void *unit;
  int bit;

  actor_handle = -1;
  scale = 1.0f;
  if (*(char *)(*(char **)0x632574 + 0x10) != '\0' && param_3 != -1) {
    print_log = *(char *)0x5aca44;
    if (*(char *)0x5aca4f != '\0') {
      crt_sprintf(buffer, "PLAYER-REPLY %s ",
                  dialogue_get_vocalization_name(param_3, 1));
    }
    if (*(char *)0x5aca44 != '\0') {
      bit = (int)param_3;
      if ((((int *)0x5aca24)[bit >> 5] & (1 << (bit & 0x1f))) != 0) {
        print_log = '\0';
      }
    }
    reply_index = 0;
    entry = (char *)0x258eb0;
    do {
      if (*(int16_t *)(entry + 0x00) == param_3 &&
          (*(int16_t *)(entry + 0x02) == -1 ||
           *(int16_t *)(entry + 0x02) == param_4)) {
        vocalization_value =
          *(int16_t *)(0x257c68 + (int)*(int16_t *)(entry + 0x0a) * 2);
        if (*(char *)0x5aca44 != '\0') {
          bit = (int)*(int16_t *)(entry + 0x06);
          if ((((int *)0x5aca24)[bit >> 5] & (1 << (bit & 0x1f))) != 0) {
            print_log = '\0';
          }
        }
        if (sound_scripted_dialog_is_playing() &&
            (*(uint8_t *)(entry + 0x0c) & 1) == 0) {
          if (*(char *)0x5aca4f != '\0') {
            csstrcat(buffer, "[scripted-override] ");
          }
        } else if (*(float *)(entry + 0x14) > *(float *)0x2533c0) {
          roll =
            random_math_real((unsigned int *)get_global_random_seed_address());
          if ((game_connection() == 0 && *(char *)0x5aca45 != '\0') ||
              roll < *(float *)(entry + 0x14)) {
            switch (*(int16_t *)(entry + 0x04)) {
            case 2:
              unit = object_get_and_verify_type(unit_handle, 3);
              actor_handle = ai_communication_find_global_actor_to_talk(
                1, -1, 0x41100000, -1, *(uint16_t *)(entry + 0x0a),
                vocalization_value, *(uint16_t *)(entry + 0x06),
                *(uint16_t *)(entry + 0x08), 0, unit_handle,
                *(int16_t *)((char *)unit + 0x68));
              break;
            case 3:
              unit = object_try_and_get_and_verify_type(param_2, 3);
              if (unit != NULL) {
                actor_handle = *(int *)((char *)unit + 0x1a4);
              }
              break;
            case 4:
              unit = object_get_and_verify_type(unit_handle, 3);
              actor_handle = ai_communication_find_global_actor_to_talk(
                2, -1, 0x41100000, -1, *(uint16_t *)(entry + 0x0a),
                vocalization_value, *(uint16_t *)(entry + 0x06),
                *(uint16_t *)(entry + 0x08), 0, unit_handle,
                *(int16_t *)((char *)unit + 0x68));
              break;
            }
            if (actor_handle == -1) {
              if (*(char *)0x5aca4f != '\0') {
                csstrcat(buffer, csprintf((char *)0x5ab100, "[%s nobody] ",
                                          dialogue_get_vocalization_name(
                                            *(uint16_t *)(entry + 0x06), 1)));
              }
              goto next_entry;
            }
            team = actor_communication_team(actor_handle);
            if (team != -1) {
              slot =
                (int32_t *)(*(char **)0x331f14 +
                            ((int)team + (int)(int16_t)reply_index * 2) * 8);
              now = game_time_get();
              if (slot[0] != -1) {
                scale = (float)(now - slot[0]) * *(float *)0x25620c;
                if (scale < *(float *)0x2533c0) {
                  scale = 0.0f;
                } else if (scale > *(float *)0x2533c8) {
                  scale = 1.0f;
                }
              }
              if ((game_connection() != 0 || *(char *)0x5aca46 == '\0') &&
                  slot[1] != -1 && slot[1] - now > 0) {
                if (*(char *)0x5aca4f != '\0') {
                  csstrcat(buffer,
                           csprintf((char *)0x5ab100, "[%s %s-d-dis/%d] ",
                                    dialogue_get_vocalization_name(
                                      *(uint16_t *)(entry + 0x06), 1),
                                    *(char **)(0x2c8d6c + (int)team * 8),
                                    slot[1] - now));
                }
                actor_handle = -1;
                goto next_entry;
              }
            }
            if (*(char *)0x5aca4f != '\0') {
              csstrcat(buffer, csprintf((char *)0x5ab100, "[%s found-actor] ",
                                        dialogue_get_vocalization_name(
                                          *(uint16_t *)(entry + 0x06), 1)));
            }
          } else {
            if (*(char *)0x5aca4f != '\0') {
              csstrcat(buffer, csprintf((char *)0x5ab100, "[%s rand%.2f>%.2f] ",
                                        dialogue_get_vocalization_name(
                                          *(uint16_t *)(entry + 0x06), 1),
                                        (double)roll,
                                        (double)*(float *)(entry + 0x14)));
            }
          }
        } else {
          if (*(char *)0x5aca4f != '\0') {
            csstrcat(buffer, csprintf((char *)0x5ab100, "[%s 0-player-chance] ",
                                      dialogue_get_vocalization_name(
                                        *(uint16_t *)(entry + 0x06), 1)));
          }
        }
        if (actor_handle != -1) {
          break;
        }
      }
    next_entry:
      reply_index++;
      entry += 0x24;
    } while (*(int16_t *)(entry + 0x00) != -1);
    if (*(char *)0x5aca4f != '\0' && print_log == '\0') {
      error(2, buffer);
    }
  }
  if (out_scale != NULL) {
    *out_scale = scale;
  }
  return actor_handle;
}

/* ai_communication_finished (0x46530) — a unit finished speaking: walk the
 * reply table for replies to its vocalization and make the first eligible
 * unit speak the reply.  DORMANT (kb ported:false).
 *
 * Confirmed (disasm 0x46530-0x46b5c):
 *   - cdecl, 6 stack args, plain RET.  [EBP+0xc] (priority) is never read;
 *     [EBP+0x10] is the 16-bit vocalization type, [EBP+0x14] a byte
 *     (reply_to_player), [EBP+0x18] a preselected actor handle (-1 = none),
 *     [EBP+0x1c] the ai information packet (+0 target unit, +8 damage
 *     category word).
 *   - Same reply table / debug globals as 0x460e0.  Reply entry +0x10 float
 *     chance, +0x18 float delay seconds, +0x20 filter callback
 *     (bool fn(unit, info, actor), TEST AL).
 *   - not_focused starts as !(focus bit of the voc type) when focus is on
 *     (NEG/SBB/INC), else 0; a focused reply voc clears it.
 *   - The loop enters its body without a sentinel test (do/while shape).
 *   - Protagonist 2: speaker actor with encounter (+0x34 != -1) ->
 *     find_specific (EAX = encounter & 0xffff, ESI = -1, EDI = unit;
 *     stack 9.0f, -1, priority, speech priority, voc, animation, 0), else
 *     find_global mode 1; 3: info target unit if it is a live unit; 4:
 *     find_global mode 2.  Reply unit = actor +0x18.
 *   - consider_speech: EAX=&sound index(-1), ECX=&voc, DX=speech priority,
 *     stack (reply unit, priority, delay ticks, 0, 0, &weight(1.0),
 *     consider string).  delay = (int)(delay seconds * 30.0).
 *   - Speech record stores (EBP-0x50 base): +0 speech priority, +2 voc,
 *     +4 sound index, +8 delay, +0xa (int)(0x257c78[priority] * 30.0),
 *     +0xc 0x18, +0x10 unit_handle, +0x14/+0x16/+0x18 -1, +0x1a byte 1,
 *     +0x1c/+0x1e/+0x24 0, csmemset(+0x28, 0, 8).  +0xe and +0x20 are not
 *     written by the binary.
 *   - look_secondary_at_unit: EAX=-1, EBX=reply unit actor (+0x1a4),
 *     EDI=unit_handle, stack (8, word 0x257c98[priority]). */
void ai_communication_finished(int unit_handle, short priority,
                               short vocalization_type, char reply_to_player,
                               int preselected_reply_actor, void *information)
{
  ai_information_packet_t *packet = (ai_information_packet_t *)information;

  (void)priority; /* [EBP+0xc] is never read */
  if (ai_globals->dialogue_triggers_enabled && vocalization_type != -1) {
    char consider_string[512];
    char debug_string[1024];
    const reply_usage_t *reply;
    char not_focused = 0;
    char any_replies = 0;
    int reply_index;

    if (ai_print_communication) {
      crt_sprintf(debug_string, "REPLY %s: ",
                  dialogue_get_vocalization_name(vocalization_type, 0));
    }
    if (ai_debug_communication_focus_enable) {
      not_focused = !ai_debug_communication_focused(vocalization_type);
    }

    reply = global_reply_table;
    reply_index = 0;
    /* the binary enters the body without a sentinel test: the original
     * reply table was initialized in this TU, so entry 0 was known valid */
    do {
      if (reply->original_vocalization_type == vocalization_type) {
        unit_data_t *unit =
          (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
        actor_t *speaker_actor =
          unit->actor_index.value == -1
            ? NULL
            : (actor_t *)datum_get(actor_data, unit->actor_index.value);
        int speech_priority = (uint16_t)
          communication_speech_priorities[reply->communication_priority];

        any_replies = 1;
        if (ai_print_communication) {
          csstrcat(debug_string,
                   csprintf(error_string_buffer, "%s:",
                            dialogue_get_vocalization_name(
                              reply->vocalization_type, 0)));
        }
        if (ai_debug_communication_focus_enable &&
            ai_debug_communication_focused(reply->vocalization_type)) {
          not_focused = 0;
        }

        if (reply->original_damage_category != -1 &&
            reply->original_damage_category != packet->damage_category) {
          if (ai_print_communication) {
            csstrcat(debug_string, "wrong-dmg ");
          }
        } else if (sound_scripted_dialog_is_playing() &&
                   (reply->flags & 1) == 0) {
          if (ai_print_communication) {
            csstrcat(debug_string, "override-scripted ");
          }
        } else {
          int reply_unit_handle = -1;

          if (preselected_reply_actor == -1) {
            int reply_actor = -1;

            assert_halt_at("c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x9e5,
                           !reply_to_player);
            switch (reply->protagonist_type) {
            case 2:
              if (speaker_actor != NULL &&
                  speaker_actor->field_034 != (uint32_t)-1) {
                /* EAX = encounter & 0xffff, ESI = -1, EDI = unit_handle;
                 * stack arg 1 is 9.0f, forwarded as bits by the int-typed
                 * callee */
                reply_actor = ai_communication_find_specific_actor_to_talk(
                  0x41100000 /* 9.0f */, -1,
                  (uint16_t)reply->communication_priority, speech_priority,
                  (uint16_t)reply->vocalization_type,
                  (uint16_t)reply->animation_type, 0,
                  speaker_actor->field_034 & 0xffff, -1, unit_handle);
              } else {
                reply_actor = ai_communication_find_global_actor_to_talk(
                  1, -1, 0x41100000 /* 9.0f */, -1,
                  (uint16_t)reply->communication_priority, speech_priority,
                  (uint16_t)reply->vocalization_type,
                  (uint16_t)reply->animation_type, 0, unit_handle,
                  unit->object.owner_team_index);
              }
              break;
            case 3:
              if (object_try_and_get_and_verify_type(packet->target_unit_index,
                                                     3)) {
                reply_unit_handle = packet->target_unit_index;
              }
              break;
            case 4:
              reply_actor = ai_communication_find_global_actor_to_talk(
                2, -1, 0x41100000 /* 9.0f */, -1,
                (uint16_t)reply->communication_priority, speech_priority,
                (uint16_t)reply->vocalization_type,
                (uint16_t)reply->animation_type, 0, unit_handle,
                unit->object.owner_team_index);
              break;
            }
            if (reply_actor != -1) {
              reply_unit_handle =
                ((actor_t *)datum_get(actor_data, reply_actor))
                  ->meta_unit_index;
            }
          } else {
            reply_unit_handle =
              ((actor_t *)datum_get(actor_data, preselected_reply_actor))
                ->meta_unit_index;
          }

          if (reply_unit_handle != -1) {
            unit_data_t *reply_unit =
              (unit_data_t *)object_get_and_verify_type(reply_unit_handle, 3);

            if (reply_unit->unk_456.value == -1) { /* unit +0x1c8 player */
              char play_reply = 1;

              if (!reply_to_player) {
                if (reply->chance > 0.0f) {
                  float random_value = random_math_real(
                    (unsigned int *)get_global_random_seed_address());

                  if ((game_connection() != 0 ||
                       !ai_debug_communication_random_disabled) &&
                      !(random_value < reply->chance)) {
                    if (ai_print_communication) {
                      csstrcat(debug_string,
                               csprintf(error_string_buffer, "rand %.2f>%.2f ",
                                        (double)random_value,
                                        (double)reply->chance));
                    }
                    play_reply = 0;
                  }
                } else {
                  if (ai_print_communication) {
                    csstrcat(debug_string, "0-chance ");
                  }
                  play_reply = 0;
                }
              }

              if (!play_reply) {
                if (ai_print_communication) {
                  csstrcat(debug_string, "rand-failed ");
                }
              } else if (reply->reply_filter != NULL &&
                         !reply->reply_filter(unit_handle, information,
                                              reply_unit->actor_index.value)) {
                if (ai_print_communication) {
                  csstrcat(debug_string, "filter ");
                }
              } else {
                short reply_voc = reply->vocalization_type;
                int sound_definition_index = -1;
                float weight = 1.0f;
                short delay_ticks = (short)(int)(reply->delay_time * 30.0f);
                short play_type = ai_communication_consider_speech(
                  &sound_definition_index, &reply_voc, (short)speech_priority,
                  reply_unit_handle, reply->communication_priority,
                  delay_ticks, 0, 0, &weight, consider_string);

                if (play_type > 0) {
                  unit_speech_item_t speech;

                  /* +0x0e and packet +0x10 are left unwritten, as in the
                   * binary */
                  speech.priority = (int16_t)speech_priority;
                  speech.vocalization_type = reply_voc;
                  speech.sound_definition_index = sound_definition_index;
                  speech.delay_time = delay_ticks;
                  speech.ai_notification_delay = (int16_t)(int)(
                    communication_notification_delays
                      [reply->communication_priority] *
                    30.0f);
                  speech.pause_time = 0x18;
                  speech.ai.target_unit_index = unit_handle;
                  speech.ai.communication_type = -1;
                  speech.ai.dialogue_type_index = -1;
                  speech.ai.damage_category = -1;
                  speech.ai.updated_dialogue_timers = 1;
                  speech.ai.look_priority = 0;
                  speech.ai.look_type = 0;
                  speech.ai.information_type = 0;
                  csmemset(&speech.ai.information_data, 0,
                           sizeof(speech.ai.information_data));
                  unit_speak(reply_unit_handle, play_type, &speech);
                  ai_communication_update_speech_timers(
                    reply_unit_handle, (int16_t)speech_priority, reply_voc,
                    -1, (int16_t)reply_index);
                  /* EAX = -1 prop, EBX = reply unit's actor, EDI =
                   * unit_handle; look type 8 = communicating prop */
                  ai_communication_look_secondary_at_unit(
                    -1, reply_unit->actor_index.value, unit_handle, 8,
                    communication_protagonist_default_look_priorities
                      [reply->communication_priority]);
                  if (ai_print_communication) {
                    csstrcat(debug_string,
                             strupr(csprintf(error_string_buffer, ">>%s<<",
                                             dialogue_get_vocalization_name(
                                               speech.vocalization_type, 1))));
                  }
                  break;
                }

                if (ai_print_communication) {
                  csstrcat(debug_string,
                           csprintf(error_string_buffer, "u-%s-%s ",
                                    weight > 0.0f ? "dis" : "n/a",
                                    consider_string));
                }
              }
            } else {
              if (ai_print_communication) {
                csstrcat(debug_string, "playercant ");
              }
            }
          } else {
            if (ai_print_communication) {
              csstrcat(debug_string, "nobody ");
            }
          }
        }
      }
      reply++;
      reply_index++;
    } while (reply->original_vocalization_type != -1);

    if (any_replies && ai_print_communication && !not_focused) {
      error(2, debug_string);
    }
  }
}

/* ai_conversation (0x46b60) — script entry point that starts a scenario
 * conversation by index.  Validates the 16-bit index against the scenario
 * tag's conversation block count at +0x468, allocates/force-starts the
 * conversation datum via ai_conversation_new, then tries to begin it.  Returns
 * true when the conversation is running or has been queued to keep trying,
 * false when the index is out of range or the conversation pool is full.
 *
 * Confirmed (disasm 0x46b60-0x46ca1):
 *   - Prologue is PUSH EBP / MOV EBP,ESP / PUSH EBX/ESI/EDI with NO
 *     `sub esp`: MSVC parks the ai_conversation_begin out-flag in the dead
 *     high byte of param_1's incoming slot ([EBP+0xb]), since only CX is
 *     ever read from that dword.  A normal C local is used here instead;
 *     the resulting `sub esp` is a permanent frame-shape difference.
 *   - `TEST CX,CX` / `JL` then `MOVSX ESI,CX` / `CMP ESI,[EAX+0x468]` /
 *     `JGE`: the range test is on the SIGNED low 16 bits, and the
 *     sign-extended index in ESI stays live across all four print sites.
 *   - global_scenario_get() is called once up front (0x46b66) and again at
 *     EACH print site (0x46ba5, 0x46c06, 0x46c40, 0x46c6e) — four separate
 *     calls, not a cached pointer.
 *   - ai_conversation_new (0x46b8f): pushes are [EBP+0xc] then [EBP+8], i.e.
 *     (param_1, param_2) cdecl.  Ghidra's `void (void)` prototype swallowed
 *     both args (§7_GETTER_SWALLOWED); `ADD ESP,0x8` proves the 2 args.
 *   - ai_conversation_begin (0x46bee): pushes are LEA ECX,[EBP+0xb] then
 *     EDI, i.e. (conversation_handle, &keep_trying) cdecl, with
 *     `MOV byte ptr [EBP+0xb],0` zeroing the flag before the call and
 *     `TEST AL,AL` consuming a bool return.  Same §7 swallow; `ADD ESP,0x8`
 *     proves 2 args.  kb decl corrected from `void (void)`.
 *   - The 0x46c39..0x46c66 arm ("can't begin yet but will remember and keep
 *     trying it", string 0x25a308) is REAL; Ghidra dropped it as
 *     unreachable.  It is taken when begin returned false but set the
 *     keep-trying flag, and it returns true.
 *   - The debug flag at 0x5aca5f is loaded once at 0x46c32, above the
 *     keep_trying branch, and both remaining arms test that same AL.
 *   - tag_block_get_element(scenario+0x468, index, 0x74): pushes are 0x74,
 *     then ESI, then the block pointer — (block, index, element_size), the
 *     kb order.  (ai_conversation_advance at 0x43520 in this TU passes the
 *     last two swapped; that pre-existing issue is not touched here.)
 *   - The four `ARG_COUNT: cleanup=6 vs decl=3` hazards on console_printf
 *     are merged-`ADD ESP,0x18` false positives: tag_block_get_element's 3
 *     args plus console_printf's 3.
 *   - Both false exits are `MOV AL,BL` with BL zeroed at 0x46b6e — a bool
 *     return, not a status variable.
 * Uncertain: the meaning of param_2 beyond ai_conversation_new's force-start
 * flag, and of ai_conversation_finish's ('\1','\0') argument pair here. */
int ai_conversation(int param_1, int param_2)
{
  char *scenario;
  int index;
  int conversation_handle;
  char keep_trying;
  char debug_enabled;

  scenario = (char *)global_scenario_get();
  index = (int)(short)param_1;
  if ((short)param_1 >= 0 && index < *(int *)(scenario + 0x468)) {
    conversation_handle = ai_conversation_new((int16_t)param_1, (char)param_2);
    if (*(char *)0x5aca5f != '\0') {
      console_printf(0, "%s: script tried to start conversation",
                     tag_block_get_element(
                       (char *)global_scenario_get() + 0x468, index, 0x74));
    }
    if (conversation_handle == -1) {
      error(2,
            "WARNING: too many executing conversations (ran out of "
            "MAXIMUM_CONVERSATIONS_PER_MAP %d)",
            0x80);
      return 0;
    }
    keep_trying = '\0';
    if (ai_conversation_begin(conversation_handle, &keep_trying) != '\0') {
      if (*(char *)0x5aca5f != '\0') {
        console_printf(0, "%s: begun successfully",
                       tag_block_get_element(
                         (char *)global_scenario_get() + 0x468, index, 0x74));
      }
      return 1;
    }
    debug_enabled = *(char *)0x5aca5f;
    if (keep_trying != '\0') {
      if (debug_enabled != '\0') {
        console_printf(
          0, "%s: can't begin yet but will remember and keep trying it",
          tag_block_get_element((char *)global_scenario_get() + 0x468, index,
                                0x74));
      }
      return 1;
    }
    if (debug_enabled != '\0') {
      console_printf(
        0,
        "%s: could not start, and not set to keep trying... aborting "
        "(status 5)",
        tag_block_get_element((char *)global_scenario_get() + 0x468, index,
                              0x74));
    }
    ai_conversation_finish(conversation_handle, '\1', '\0');
  }
  return 0;
}

/* ai_conversation_update (0x46cb0) — per-tick driver for every live
 * conversation datum in the "ai conversation" pool (0x6324ec).  Not-yet-begun
 * conversations retry ai_conversation_begin every 30 ticks from their +0xc
 * timestamp and are finished ('\1','\0') once begin clears the keep-trying
 * flag; begun ones advance their line index (+0x48) through
 * ai_conversation_line_perform / ai_conversation_line_begin until the scenario
 * definition's +0x5c count is exhausted, which sets +0x7; +0x7 set finishes
 * the conversation ('\0','\1'); otherwise participants get their
 * actor +0x1dc/+0x1e0 fields refreshed.
 *
 * Confirmed (disasm 0x46cb0-0x46f0d, live Ghidra; bundle artifact held only
 * connection-error payloads):
 *   - game_time_get() is called once, before data_iterator_new.
 *   - tag_block_get_element(global_scenario_get() + 0x468, *(short*)(c+2),
 *     0x74): pushes 0x74, index, block — (block, index, element_size).
 *   - ai_conversation_begin (0x45a10): pushes LEA [EBP-1] then iter handle
 *     [EBP-0x14] (iter at EBP-0x1c, datum_handle at +8), ADD ESP,8; the
 *     keep_trying byte is set to 1 before the modulo test; return unused.
 *   - console_printf channel is EDX (the zero remainder) at the first site,
 *     PUSH 0 elsewhere; the scenario definition pointer is the %s arg.
 *   - ai_conversation_line_perform / _line_begin take the handle in EAX; the
 *     begin return (AL) becomes the next iteration's line-ready flag.
 *   - After a successful perform, datum_get(conversation pool, handle) and
 *     tag_block_get_element(...) are called again and the result discarded
 *     (EDI keeps the original definition pointer).
 *   - Actor loop: 16-bit counter (BX), bit test of +0x14, handle at
 *     +0x28 + i*4, actor pool 0x6325a4; the +0x4e flags byte test for bit 0
 *     and word test for bits 1/2.
 * Uncertain: semantics of conversation +0x6/+0x7/+0x14/+0x4e/+0x54/+0x58 and
 * actor +0x18/+0x1dc/+0x1e0 beyond the observed accesses. */
void ai_conversation_update(void)
{
  data_iter_t iter;
  char *conversation;
  char *definition;
  actor_t *actor;
  int time;
  int actor_handle;
  int16_t member;
  char keep_trying;
  char line_ready;

  time = game_time_get();
  data_iterator_new(&iter, *(data_t **)0x6324ec);
  conversation = (char *)data_iterator_next(&iter);
  while (conversation != 0) {
    definition =
      (char *)tag_block_get_element((char *)global_scenario_get() + 0x468,
                                    *(int16_t *)(conversation + 2), 0x74);
    if (conversation[6] == '\0') {
      keep_trying = '\1';
      if ((time - *(int32_t *)(conversation + 0xc)) % 0x1e == 0) {
        if (*(char *)0x5aca5f != '\0') {
          console_printf(0, "%s: trying to begin", definition);
        }
        ai_conversation_begin((int)iter.datum_handle, &keep_trying);
      }
      if (conversation[6] == '\0' && keep_trying == '\0') {
        if (*(char *)0x5aca5f != '\0') {
          console_printf(0, "%s: unable to begin, and no point in continuing",
                         definition);
        }
        ai_conversation_finish((int)iter.datum_handle, '\1', '\0');
      }
    }
    if (conversation[6] != '\0' && conversation[7] == '\0') {
      line_ready =
        (*(int16_t *)(conversation + 0x48) >= 0 &&
         *(int16_t *)(conversation + 0x48) < *(int32_t *)(definition + 0x5c));
      for (;;) {
        if (line_ready != '\0') {
          char *line_conversation;

          if (!ai_conversation_line_perform((int)iter.datum_handle)) {
            goto line_pending;
          }
          line_conversation =
            (char *)datum_get(*(data_t **)0x6324ec, (int)iter.datum_handle);
          tag_block_get_element((char *)global_scenario_get() + 0x468,
                                *(int16_t *)(line_conversation + 2), 0x74);
        }
        *(int16_t *)(conversation + 0x48) =
          *(int16_t *)(conversation + 0x48) + 1;
        if (*(int16_t *)(conversation + 0x48) >=
            *(int32_t *)(definition + 0x5c)) {
          break;
        }
        line_ready = ai_conversation_line_begin((int)iter.datum_handle);
      }
      if (*(char *)0x5aca5f != '\0') {
        console_printf(0, "%s: no more lines to play", definition);
      }
      conversation[7] = '\1';
    }
  line_pending:
    if (conversation[7] != '\0') {
      ai_conversation_finish((int)iter.datum_handle, '\0', '\1');
    } else if (conversation[6] != '\0') {
      for (member = 0; member < *(int32_t *)(definition + 0x50); member++) {
        if ((*(uint32_t *)(conversation + 0x14) & (1 << member)) != 0 &&
            (actor_handle = *(int32_t *)(conversation + member * 4 + 0x28),
             actor_handle != -1)) {
          actor = (actor_t *)datum_get(*(data_t **)0x6325a4, actor_handle);
          actor->field_1dc = (int32_t)iter.datum_handle;
          actor->field_1e0 = -1;
          if (actor->field_018 == *(int32_t *)(conversation + 0x54)) {
            actor->field_1e0 = *(int32_t *)(conversation + 0x58);
          } else if (actor->field_018 == *(int32_t *)(conversation + 0x58) &&
                     (*(uint16_t *)(conversation + 0x4e) & 1) != 0) {
            actor->field_1e0 = *(int32_t *)(conversation + 0x54);
          } else if ((*(uint16_t *)(conversation + 0x4e) & 2) != 0) {
            actor->field_1e0 = *(int32_t *)(conversation + 0x54);
          } else if ((*(uint16_t *)(conversation + 0x4e) & 4) != 0) {
            actor->field_1e0 = *(int32_t *)(conversation + 0x58);
          }
        }
      }
    }
    conversation = (char *)data_iterator_next(&iter);
  }
}

/* ---------- ai_communication_event support (TU-local) ----------
 * Names are inferred; every offset and
 * table base below is confirmed by the 0x46f10 disassembly. */

/* Selected fields of an encounter datum (encounter_data pool, 0x5ab270).
 * Offsets from ai_communication_event: +0x44/+0x45/+0x46 bytes and the
 * +0x50 dword timer compared against -1, 75, 180 and 270. */
typedef struct {
  char pad_00[0x44];
  boolean enemy_alive;                /* +0x44 */
  boolean enemy_visible;              /* +0x45 */
  boolean enemy_traitor;              /* +0x46 */
  char pad_47[0x9];
  int32_t enemy_visible_timer;        /* +0x50 */
} ai_communication_encounter_view_t;
co(ai_communication_encounter_view_t, enemy_visible_timer, 0x50);

/* One candidate line (EBP-0x56c array, stride 0x38 per IMUL EAX,EAX,0x38). */
typedef struct {
  real weight;                          /* +0x00 */
  boolean interrupts;                   /* +0x04 */
  boolean is_reply;                     /* +0x05 */
  int16_t vocalization_type;            /* +0x06 */
  int16_t priority;                     /* +0x08 */
  int16_t animation_type;               /* +0x0a */
  int16_t play_type;                    /* +0x0c */
  int16_t delay_time;                   /* +0x0e */
  int16_t ai_delay_time;                /* +0x10 */
  char pad_12[0x2];                     /* +0x12 */
  int32_t protagonist_unit_index;       /* +0x14 */
  int32_t protagonist_actor_index;      /* +0x18 */
  int32_t recipient_unit_index;         /* +0x1c */
  int32_t preselected_reply_actor_index; /* +0x20 */
  int16_t protagonist_look_priority;    /* +0x24 */
  int16_t recipient_look_priority;      /* +0x26 */
  int16_t look_type;                    /* +0x28 */
  char pad_2a[0x2];                     /* +0x2a */
  int32_t look_unit_index;              /* +0x2c */
  int32_t sound_definition_index;       /* +0x30 */
  int16_t dialogue_type_index;          /* +0x34 */
  char pad_36[0x2];                     /* +0x36 */
} ai_communication_possibility_t;
cs(ai_communication_possibility_t, 0x38);

/* Secondary-look request handed to actor_look_secondary (0x27a60): a type
 * word at +0 (1 = prop, 3 = point) and the target at +4 (EBP-0xf0 record). */
typedef struct {
  int16_t type;                       /* +0x00 */
  char pad_02[0x2];
  union {
    int32_t prop_index;
    real_point3d point;
  } data;                             /* +0x04 */
} ai_communication_secondary_look_t;
cs(ai_communication_secondary_look_t, 0x10);

#define MAXIMUM_COMMUNICATION_POSSIBILITIES 16
/* global_communication_table_indices, global_communication_type_names and
 * global_communication_priority_names are kb.json data symbols. */
/* communication_player_speaking_priorities: kb.json data symbol. */
/* communication_recipient_default_look_priorities,
 * communication_timer_tolerances and communication_play_delays are kb.json
 * data symbols. */
/* communication_player_rating_low_priority: kb.json data symbol. */
#ifndef TEST_FLAG
#define TEST_FLAG(flags, bit) (((flags) & (unsigned)FLAG(bit)) != 0)
#endif
#ifndef SET_FLAG
#define SET_FLAG(f, b, v) \
  ((v) ? ((f) |= (unsigned)FLAG(b)) : ((f) &= (unsigned)~FLAG(b)))
#endif
#define ai_debug_bit_test(vector, index) \
  (((vector)[(index) >> 5] & (1u << ((index) & 0x1f))) != 0)

/* ai_communication_event (0x46f10) — something happened (death, damage,
 * sighting, ...) between a subject and a cause unit: update allegiance
 * incidents, then score every dialogue usage for this communication type
 * and have the winning protagonist speak (or hand off to the player-reply
 * path).
 *
 * Confirmed (disassembly):
 *   - args are [EBP+8] word type, +0xc subject unit, +0x10 cause unit,
 *     +0x14 word hostility, +0x18 word damage type, +0x1c word information
 *     type, +0x20 information data pointer (NULL -> csmemset of 8 bytes);
 *   - 2276 derives the communication team inline from
 *     actor_type_get_race (race & 2 -> 0, race & 4 -> 1, else NONE), and
 *     the post-speech look goes through prop_get_active_by_unit_index +
 *     actor_look_secondary(actor, 9, ...);
 *   - enemy_status[5] without an encounter is target_type >= 10 AND the
 *     target is really alive (the alive test is not negated).
 * Uncertain: play_type and look_unit_index are not reset per usage in the
 * binary (they keep stale stack values for reply / no-look usages); they
 * are initialized once here and only reach consumers that ignore them. */
void ai_communication_event(short communication_type, int subject_unit_index,
                            int cause_unit_index, short hostility,
                            short damage_type, short information_type,
                            ai_information_data_t *information_data)
{
  char speech_string[512];
  char requirements_string[1024];
  char timer_string[512];
  char debug_string[1024];
  char team_string[256];
  ai_communication_possibility_t possibilities[
    MAXIMUM_COMMUNICATION_POSSIBILITIES];
  int16_t speech_disabled_reason[2 * 2 * 8];
  int16_t speech_delay[2 * 2 * 8];
  boolean speech_disabled[2 * 2 * 8];
  real chatter_seconds[2];
  real talk_seconds[2];
  real shout_seconds[2];
  int16_t chatter_ticks[2];
  int16_t talk_ticks[2];
  int16_t shout_ticks[2];
  boolean hostility_matches[5];
  boolean enemy_status[6];
  boolean subject_groups[2];
  boolean cause_groups[2];
  int event_time;
  short possibility_count;
  real total_possibility_weight;
  boolean any_forced_possibility;
  unit_data_t *subject_unit;
  unit_data_t *cause_unit;
  int subject_encounter_index;
  ai_communication_encounter_view_t *subject_encounter;
  int subject_actor_index;
  actor_t *subject_actor;
  int cause_actor_index;
  actor_t *cause_actor;
  int friend_actor_index;
  int other_actor_index;
  short subject_team;
  short cause_team;
  short subject_race;
  short cause_race;
  boolean find_friend_actor;
  boolean find_other_actor;
  boolean player_involved;
  boolean suppress_output;
  boolean any_requirements_failed;
  boolean any_protagonist_considered;
  const dialogue_usage_t *usage;
  short dialogue_index;
  short communication_team;
  short priority;
  short distance_group;
  short play_type;
  int look_unit_index;
  ai_communication_possibility_t *selected_possibility;
  short vocalization_type;

  event_time = game_time_get();
  possibility_count = 0;
  total_possibility_weight = 0.0f;
  any_forced_possibility = false;
  subject_unit = NULL;
  cause_unit = NULL;
  subject_encounter_index = -1;
  subject_encounter = NULL;
  subject_actor_index = -1;
  subject_actor = NULL;
  cause_actor_index = -1;
  cause_actor = NULL;
  friend_actor_index = -1;
  other_actor_index = -1;
  subject_team = -1;
  cause_team = -1;
  subject_race = 0;
  cause_race = 0;
  find_friend_actor = true;
  find_other_actor = true;
  player_involved = false;
  any_requirements_failed = false;
  any_protagonist_considered = false;
  play_type = 0;
  look_unit_index = -1;

  assert_halt_msg_at("(communication_type >= 0) && (communication_type < "
                     "NUMBER_OF_AI_COMMUNICATION_TYPES)",
                     "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x34e,
                     communication_type >= 0 && communication_type < 0x39);

  if (hostility == -1) {
    hostility = 0; /* _comm_hostility_none */
  }
  if (damage_type == -1) {
    damage_type = 0;
  }

  for (distance_group = 0; distance_group < 2; distance_group++) {
    subject_groups[distance_group] = false;
    cause_groups[distance_group] = false;
  }

  if (subject_unit_index != -1) {
    subject_unit = (unit_data_t *)object_get_and_verify_type(
      subject_unit_index, 3);
    subject_actor_index = subject_unit->actor_index.value;
    subject_team = subject_unit->object.owner_team_index;
    subject_race = (short)ai_get_race_from_team_index(subject_team);
    if (subject_actor_index != -1) {
      subject_actor = (actor_t *)datum_get(actor_data, subject_actor_index);
      subject_encounter_index = subject_actor->field_034;
      subject_race = actor_type_get_race(subject_actor->meta_type);
      if (subject_actor->field_245 > 0) { /* situation.close_friends */
        subject_groups[1] = true;
        subject_groups[0] = true;
      } else if (subject_actor->field_200 > 0) { /* situation.area_friends */
        subject_groups[1] = false;
        subject_groups[0] = true;
      }
      if (subject_encounter_index != -1) {
        subject_encounter = (ai_communication_encounter_view_t *)datum_get(
          encounter_data, subject_encounter_index);
      }
    } else if (subject_unit->unk_456.value != -1) { /* unit.player_index */
      subject_race = 1; /* _race_player */
    }
  }

  if (cause_unit_index != -1) {
    cause_unit = (unit_data_t *)object_get_and_verify_type(cause_unit_index,
                                                            3);
    cause_actor_index = cause_unit->actor_index.value;
    cause_team = cause_unit->object.owner_team_index;
    cause_race = (short)ai_get_race_from_team_index(cause_team);
    if (cause_actor_index != -1) {
      cause_actor = (actor_t *)datum_get(actor_data, cause_actor_index);
      cause_race = actor_type_get_race(cause_actor->meta_type);
      if (cause_actor->field_245 > 0) {
        cause_groups[0] = true;
        cause_groups[1] = true;
      } else if (cause_actor->field_200 > 0) {
        cause_groups[0] = true;
        cause_groups[1] = false;
      }
    } else if (cause_unit->unk_456.value != -1) {
      cause_race = 1; /* _race_player */
    }
  }

  if (subject_unit && cause_unit && subject_team != cause_team &&
      game_team_is_ally(subject_team, cause_team)) {
    boolean betrayal = false;
    boolean observed = false;

    if (communication_type == 0) { /* _ai_communication_death */
      if (hostility == 3) {        /* _comm_hostility_enemy */
        betrayal = true;
        observed = true;
      } else {
        if (subject_encounter) {
          betrayal = subject_encounter->enemy_traitor ||
                     subject_encounter->enemy_visible_timer == -1 ||
                     subject_encounter->enemy_visible_timer >= 270;
        }
        /* EDI = subject unit, BX = subject team; 18.0f pushed as bits */
        friend_actor_index = ai_communication_find_global_actor_to_talk(
          0 /* _find_actor_mode_same_team */, cause_unit_index,
          0x41900000 /* 18.0f */, 0 /* death */, 6 /* yell */, -1, -1, -1, 0,
          subject_unit_index, subject_team);
        if (friend_actor_index != -1) {
          find_friend_actor = false;
          observed = true;
        }
        switch (damage_type) {
        case 3:
        case 4:
        case 9:
          if (!betrayal) {
            observed = false;
          }
          break;
        }
        if (damage_type == 3) {
          betrayal = false;
        }
      }

      if (betrayal) {
        hostility = 4; /* _comm_hostility_traitor */
      }

      if (ai_print_allegiance) {
        console_printf(0, "incident between teams %s and %s: %s, %s",
                       global_game_team_names[subject_team],
                       global_game_team_names[cause_team],
                       betrayal ? "betrayal" : "accident",
                       observed ? "observed" : "unobserved");
      }

      if (observed) {
        bool notify_immediately = false;
        boolean broken = game_allegiance_bump(cause_team, subject_team,
                                              betrayal != 0,
                                              &notify_immediately);

        if (notify_immediately) {
          ai_handle_allegiance_broken_notification(cause_team, subject_team,
                                                   broken);
        }
        if (ai_print_allegiance && !broken) {
          int16_t incident_threshold;
          int16_t incidents = game_allegiance_get_incidents(
            cause_team, subject_team, &incident_threshold);

          console_printf(0, "allegiance %s, %d incidents (threshold %d)",
                         "still holds", (int)incidents,
                         incident_threshold == -1 ? 999
                                                  : (int)incident_threshold);
        }
      }
    }

    /* Team allegiance test. */
    if (game_allegiance_get_team_is_friendly(subject_team, cause_team)) {
      hostility = 4; /* _comm_hostility_traitor */
    }
  }

  if (ai_print_communication) {
    char hostility_characters[5];

    hostility_characters[0] = 'n';
    hostility_characters[1] = 's';
    hostility_characters[2] = 'f';
    hostility_characters[3] = 'e';
    hostility_characters[4] = 't';
    crt_sprintf(debug_string, "%s-%c ",
                global_communication_type_names[communication_type],
                hostility_characters[hostility]);
    strupr(debug_string);
    csstrcpy(requirements_string, "");
  }

  if (!subject_actor) {
    short index;

    for (index = 0; index < 6; index++) {
      enemy_status[index] = true;
    }
  } else if (!subject_encounter) {
    /* +0x274 any_target_ever, +0x278 since_any_target_visible_timer,
     * +0x27c target_really_alive, +0x6e combat_status (inferred names) */
    enemy_status[0] = !subject_actor->field_274;
    enemy_status[1] = !subject_actor->field_27c &&
                      subject_actor->field_278 != -1;
    enemy_status[2] = subject_actor->target_target_prop_index == -1 ||
                      subject_actor->field_278 == -1 ||
                      subject_actor->field_278 >= 180;
    enemy_status[3] = subject_actor->field_06e < 3 &&
                      (subject_actor->field_278 == -1 ||
                       subject_actor->field_278 >= 75) &&
                      (subject_actor->field_27c ||
                       subject_actor->field_06e > 0);
    enemy_status[4] = subject_actor->field_06e < 6;
    enemy_status[5] = subject_actor->target_target_type >= 10 &&
                      subject_actor->field_27c;
  } else {
    enemy_status[0] = !subject_actor->field_274;
    enemy_status[1] = subject_encounter->enemy_visible_timer != -1 &&
                      !subject_encounter->enemy_alive;
    enemy_status[2] = (subject_encounter->enemy_visible_timer == -1 ||
                       subject_encounter->enemy_visible_timer >= 180) &&
                      subject_encounter->enemy_alive;
    enemy_status[3] = subject_actor->field_06e < 3 &&
                      (subject_encounter->enemy_visible_timer == -1 ||
                       subject_encounter->enemy_visible_timer >= 75) &&
                      (subject_encounter->enemy_alive ||
                       subject_actor->field_06e > 0);
    enemy_status[4] = subject_actor->field_06e < 6 &&
                      (subject_encounter->enemy_visible_timer == -1 ||
                       subject_encounter->enemy_visible_timer >= 75);
    enemy_status[5] = subject_encounter->enemy_visible &&
                      subject_encounter->enemy_alive;
  }

  csmemset(hostility_matches, 0, sizeof(hostility_matches));
  if (hostility != -1) {
    hostility_matches[hostility] = true;
    if (hostility == 4) {
      hostility_matches[3] = true;
    }
  }

  csmemset(speech_disabled_reason, -1, sizeof(speech_disabled_reason));
  csmemset(speech_disabled, 0, sizeof(speech_disabled));
  csmemset(speech_delay, 0, sizeof(speech_delay));

  for (communication_team = 0; communication_team < 2; communication_team++) {
    int ticks;

    ticks = event_time -
            ai_globals->last_shout_time[communication_team];
    shout_ticks[communication_team] = (int16_t)(ticks < 0 ? 0 : ticks);
    ticks = event_time -
            ai_globals->last_talk_time[communication_team];
    talk_ticks[communication_team] = (int16_t)(ticks < 0 ? 0 : ticks);
    ticks = event_time -
            ai_globals->last_chatter_time[communication_team];
    chatter_ticks[communication_team] = (int16_t)(ticks < 0 ? 0 : ticks);
    /* 0x2546a4 = 1/30 */
    shout_seconds[communication_team] =
      (real)shout_ticks[communication_team] * 0.033333335f;
    talk_seconds[communication_team] =
      (real)talk_ticks[communication_team] * 0.033333335f;
    chatter_seconds[communication_team] =
      (real)chatter_ticks[communication_team] * 0.033333335f;

    /* nothing may be spoken at priority none */
    speech_disabled[2 * (8 * communication_team)] = true;
    speech_disabled[2 * (8 * communication_team) + 1] = true;

    for (priority = 1; priority < 6; priority++) {
      for (distance_group = 0; distance_group < 2; distance_group++) {
        const real *tolerance =
          communication_timer_tolerances[priority][distance_group];
        int slot = 2 * (8 * communication_team + priority) + distance_group;
        boolean disabled = false;
        short delay = 0;
        short reason = -1;
        short remaining;

        if (tolerance[0] > 0.0f) {
          remaining = (short)(int)(tolerance[0] * 30.0f -
                                   (real)chatter_ticks[communication_team]);
          if (remaining > 0) {
            reason = 0;
            disabled = true;
            delay = remaining < 0 ? 0 : remaining;
          }
        }
        if (tolerance[1] > 0.0f) {
          remaining = (short)(int)(tolerance[1] * 30.0f -
                                   (real)talk_ticks[communication_team]);
          if (remaining > 0) {
            disabled = true;
            reason = 1;
            delay = delay > remaining ? delay : remaining;
          }
        }
        if (tolerance[3] > 0.0f) {
          remaining = (short)(int)(tolerance[3] * 30.0f -
                                   (real)shout_ticks[communication_team]);
          if (remaining > 0) {
            disabled = true;
            reason = 2;
            delay = delay > remaining ? delay : remaining;
          }
        }
        /* Reads this slot's stored delay, which is still zero from the
         * csmemset above. */
        if (disabled && tolerance[4] > 0.0f &&
            (real)speech_delay[slot] < tolerance[4] * 30.0f) {
          disabled = false;
        }
        speech_disabled_reason[slot] = reason;
        speech_delay[slot] = delay;
        speech_disabled[slot] = disabled;
      }
    }
  }

  if (!ai_globals->dialogue_triggers_enabled) {
    if (ai_print_communication) {
      csstrcat(debug_string, "DISABLED");
      error(2, debug_string);
    }
    return;
  }

  dialogue_index = global_communication_table_indices[communication_type];
  if (game_connection() == 0 &&
      ai_debug_bit_test(ai_debug_communication_suppress_vector,
                        (int)communication_type)) {
    dialogue_index = -1;
    suppress_output = true;
  } else {
    suppress_output = ai_debug_bit_test(ai_debug_communication_ignore_vector,
                                        (int)communication_type);
  }
  if (ai_debug_communication_focus_enable) {
    suppress_output = true;
  }

  if (dialogue_index != -1) {
    assert_halt_msg_at("(dialogue_index >= 0) && (dialogue_index < "
                       "global_dialogue_event_count)",
                       "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x4c3,
                       dialogue_index >= 0 &&
                         dialogue_index < global_dialogue_event_count);

    for (usage = &global_dialogue_table[dialogue_index];
         usage->communication_type == communication_type;
         usage++, dialogue_index++) {
      short communication_priority = usage->communication_priority;

      if (ai_debug_communication_focus_enable &&
          ai_debug_communication_focused(usage->vocalization_type)) {
        suppress_output = false;
      }

      if (usage->required_hostility != -1 &&
          !hostility_matches[usage->required_hostility]) {
        if (ai_print_communication) {
          const char *hostility_names[5];

          hostility_names[0] = "none";
          hostility_names[1] = "self";
          hostility_names[2] = "friend";
          hostility_names[3] = "enemy";
          hostility_names[4] = "traitor";
          csstrcat(requirements_string,
                   csprintf(error_string_buffer, "[%s/%d host-%s] ",
                            dialogue_get_vocalization_name(
                              usage->vocalization_type, 1),
                            (int)dialogue_index,
                            hostility_names[usage->required_hostility]));
        }
        any_requirements_failed = true;
      } else if (sound_scripted_dialog_is_playing() &&
                 usage->communication_priority < 6 &&
                 (usage->flags & 0x40) == 0) { /* override-scripted bit */
        if (ai_print_communication) {
          csstrcat(requirements_string, "[scripted-override] ");
        }
        any_requirements_failed = true;
      } else if (usage->required_enemy_status != -1 &&
                 !enemy_status[usage->required_enemy_status]) {
        if (ai_print_communication) {
          const char *enemy_status_names[6];

          enemy_status_names[0] = "never";
          enemy_status_names[1] = "dead";
          enemy_status_names[2] = "lost";
          enemy_status_names[3] = "notvis";
          enemy_status_names[4] = "nodanger";
          enemy_status_names[5] = "vis";
          csstrcat(requirements_string,
                   csprintf(error_string_buffer, "[%s/%d status-%s] ",
                            dialogue_get_vocalization_name(
                              usage->vocalization_type, 1),
                            (int)dialogue_index,
                            enemy_status_names[usage->required_enemy_status]));
        }
        any_requirements_failed = true;
      } else if (usage->required_subject_race != -1 &&
                 (subject_unit_index == -1 ||
                  (short)(usage->required_subject_race & subject_race) ==
                    0)) {
        if (ai_print_communication) {
          csstrcat(requirements_string,
                   csprintf(error_string_buffer, "[%s/%d nosubrace] ",
                            dialogue_get_vocalization_name(
                              usage->vocalization_type, 1),
                            (int)dialogue_index));
        }
        any_requirements_failed = true;
      } else if (usage->required_cause_race != -1 &&
                 (cause_unit_index == -1 ||
                  (short)(usage->required_cause_race & cause_race) == 0)) {
        if (ai_print_communication) {
          csstrcat(requirements_string,
                   csprintf(error_string_buffer, "[%s/%d nocausrace] ",
                            dialogue_get_vocalization_name(
                              usage->vocalization_type, 1),
                            (int)dialogue_index));
        }
        any_requirements_failed = true;
      } else if (usage->required_damage != -1 &&
                 usage->required_damage != damage_type) {
        if (ai_print_communication) {
          csstrcat(requirements_string,
                   csprintf(error_string_buffer, "[%s/%d nodmg] ",
                            dialogue_get_vocalization_name(
                              usage->vocalization_type, 1),
                            (int)dialogue_index));
        }
        any_requirements_failed = true;
      } else {
        short speech_priority =
          communication_speech_priorities[communication_priority];
        actor_t *protagonist_actor = NULL;
        short look_type = 0; /* _ai_information_none */
        short recipient_look_priority = 0;
        boolean *protagonist_groups = NULL;
        boolean is_reply = false;
        int protagonist_actor_index = -1;
        short candidate_delay = 0;
        int protagonist_unit_index = -1;
        int recipient_unit_index = -1;
        int reply_actor_index = -1;
        real repeat_rating = 1.0f;
        real reply_rating = 1.0f;
        short protagonist_look_priority;
        real player_rating;
        boolean near_player;
        boolean valid = true;

        csstrcpy(team_string, "<err>");

        switch (usage->protagonist_type) {
        case 0: /* _comm_protagonist_subject */
          protagonist_groups = subject_groups;
          protagonist_unit_index = subject_unit_index;
          protagonist_actor_index = subject_actor_index;
          protagonist_actor = subject_actor;
          recipient_unit_index = cause_unit_index;
          break;

        case 1: /* _comm_protagonist_cause */
          protagonist_groups = cause_groups;
          protagonist_unit_index = cause_unit_index;
          protagonist_actor_index = cause_actor_index;
          protagonist_actor = cause_actor;
          recipient_unit_index = subject_unit_index;
          break;

        case 2: /* _comm_protagonist_friend */
          recipient_unit_index = cause_unit_index;
          if (find_friend_actor) {
            short find_actor_flags;

            /* bit0 allow lookup (usage bit0), bit1 near to players,
             * bit2 same vehicle (usage 0x10), bit3 allow subject
             * (usage 0x20), bit4 allow cause */
            find_actor_flags = 0;
            SET_FLAG(find_actor_flags, 0, TEST_FLAG(usage->flags, 0));
            SET_FLAG(find_actor_flags, 1, true);
            SET_FLAG(find_actor_flags, 2, TEST_FLAG(usage->flags, 4));
            SET_FLAG(find_actor_flags, 3, TEST_FLAG(usage->flags, 5));
            SET_FLAG(find_actor_flags, 4, true);

            if (subject_encounter_index != -1) {
              /* EAX = encounter absolute index, ESI = cause unit,
               * EDI = subject unit; 10.0f pushed as bits */
              friend_actor_index = ai_communication_find_specific_actor_to_talk(
                0x41200000 /* 10.0f */, communication_type,
                communication_priority, speech_priority,
                (uint16_t)usage->vocalization_type,
                (uint16_t)usage->animation_type, find_actor_flags,
                subject_encounter_index & 0xffff, cause_unit_index,
                subject_unit_index);
            } else {
              friend_actor_index = ai_communication_find_global_actor_to_talk(
                1 /* _find_actor_mode_friend */, cause_unit_index,
                0x41200000 /* 10.0f */, communication_type,
                communication_priority, speech_priority,
                (uint16_t)usage->vocalization_type,
                (uint16_t)usage->animation_type, find_actor_flags,
                subject_unit_index, subject_team);
            }
            find_friend_actor = false;
          }
          protagonist_actor_index = friend_actor_index;
          if (protagonist_actor_index != -1) {
            protagonist_actor =
              (actor_t *)datum_get(actor_data, protagonist_actor_index);
            protagonist_unit_index = protagonist_actor->meta_unit_index;
          }
          break;

        case 4: /* _comm_protagonist_enemy */
          recipient_unit_index = cause_unit_index;
          if (find_other_actor) {
            short find_actor_flags;

            find_actor_flags = 0;
            SET_FLAG(find_actor_flags, 0, TEST_FLAG(usage->flags, 0));
            SET_FLAG(find_actor_flags, 1, true);
            SET_FLAG(find_actor_flags, 2, TEST_FLAG(usage->flags, 4));
            SET_FLAG(find_actor_flags, 3, TEST_FLAG(usage->flags, 5));
            other_actor_index = ai_communication_find_global_actor_to_talk(
              2 /* _find_actor_mode_enemy */, cause_unit_index,
              0x41400000 /* 12.0f */, communication_type,
              communication_priority, speech_priority,
              (uint16_t)usage->vocalization_type,
              (uint16_t)usage->animation_type, find_actor_flags,
              subject_unit_index, subject_team);
            find_other_actor = false;
          }
          protagonist_actor_index = other_actor_index;
          if (protagonist_actor_index != -1) {
            protagonist_actor =
              (actor_t *)datum_get(actor_data, protagonist_actor_index);
            protagonist_unit_index = protagonist_actor->meta_unit_index;
          }
          break;

        default:
          display_assert(NULL, "c:\\halo\\SOURCE\\ai\\ai_communication.c",
                         0x594, true);
          system_exit(-1);
          break;
        }

        if (protagonist_unit_index != -1) {
          unit_data_t *protagonist_unit = (unit_data_t *)
            object_get_and_verify_type(protagonist_unit_index, 3);

          if ((protagonist_unit->object.damage_flags & 4) != 0 || /* dead */
              protagonist_unit->object.type == 1) {  /* vehicle */
            valid = false;
          } else if (protagonist_unit->unk_456.value != -1 &&
                     protagonist_unit->actor_index.value == -1) {
            if (usage->flags & 8) { /* _dialogue_usage_player_bit */
              is_reply = true;
              player_involved = true;
            } else {
              valid = false;
            }
          }
        } else {
          valid = false;
        }

        /* +0x6a state.mode (0 = braindead), +0x6c state.action (0xb = obey),
         * +0xa0 obey allow_communication (inferred names) */
        if ((protagonist_actor &&
             (protagonist_actor->field_06a == 0 ||
              (protagonist_actor->state_action == 0xb &&
               !protagonist_actor->field_0a0))) ||
            !valid) {
          if (ai_print_communication) {
            char protagonist_code =
              usage->protagonist_type == 0   ? 's'
              : usage->protagonist_type == 1 ? 'c'
              : usage->protagonist_type == 2 ? 'f'
              : usage->protagonist_type == 4 ? 'e'
                                             : '?';

            csstrcat(debug_string,
                     csprintf(error_string_buffer, "[%s/%d nounit-%c] ",
                              dialogue_get_vocalization_name(
                                usage->vocalization_type, 1),
                              (int)dialogue_index, (int)protagonist_code));
          }
          valid = false;
        }

        if (valid && is_reply) {
          reply_actor_index = ai_communication_find_actor_to_reply_to_player(
            protagonist_unit_index, recipient_unit_index,
            usage->vocalization_type, damage_type, &reply_rating);
          if (protagonist_unit_index == subject_unit_index) {
            find_friend_actor = false;
            friend_actor_index = reply_actor_index;
          }
          if (reply_actor_index == -1) {
            if (ai_print_communication) {
              csstrcat(debug_string,
                       csprintf(error_string_buffer, "[%s/%d noplyreply] ",
                                dialogue_get_vocalization_name(
                                  usage->vocalization_type, 1),
                                (int)dialogue_index));
            }
            valid = false;
          }
        }

        if (valid && usage->required_group != -1 && protagonist_groups &&
            !protagonist_groups[usage->required_group]) {
          if (ai_print_communication) {
            char group_code = usage->required_group == 0   ? 'e'
                              : usage->required_group == 1 ? 't'
                                                           : '?';

            csstrcat(debug_string,
                     csprintf(error_string_buffer, "[%s/%d nogrp-%c] ",
                              dialogue_get_vocalization_name(
                                usage->vocalization_type, 1),
                              (int)dialogue_index, (int)group_code));
          }
          valid = false;
        }

        if (valid) {
          if (is_reply) {
            communication_priority =
              communication_player_speaking_priorities[communication_priority];
            player_rating = 2.0f;
            csstrcpy(team_string, "player");
          } else {
            player_rating = ai_communication_get_player_rating(
              protagonist_unit_index, 1, NULL, NULL);
            if (player_rating == 0.0f) {
              if (ai_print_communication) {
                csstrcat(debug_string,
                         csprintf(error_string_buffer, "[%s/%d 0-playrat] ",
                                  dialogue_get_vocalization_name(
                                    usage->vocalization_type, 1),
                                  (int)dialogue_index));
              }
              valid = false;
            } else {
              near_player =
                player_rating < communication_player_rating_low_priority;
              communication_team = -1;
              if (protagonist_actor_index != -1) {
                int16_t race = actor_type_get_race(
                  ((actor_t *)datum_get(actor_data, protagonist_actor_index))
                    ->meta_type);

                if (race & 2) {
                  communication_team = 0;
                } else if (race & 4) {
                  communication_team = 1;
                }
              }

              if (communication_team == -1) {
                csstrcpy(team_string, "unteamed");
              } else {
                int slot;

                assert_halt_msg_at(
                  "(communication_team >= 0) && (communication_team < "
                  "NUMBER_OF_AI_COMMUNICATION_TEAMS)",
                  "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x62b,
                  communication_team >= 0 && communication_team < 2);
                assert_halt_msg_at(
                  "(communication_priority > _ai_communication_priority_none) "
                  "&& (communication_priority < "
                  "NUMBER_OF_AI_COMMUNICATION_PRIORITIES)",
                  "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x62e,
                  communication_priority > 0 && communication_priority < 8);

                crt_sprintf(
                  team_string, "%s-%c%c%c%s",
                  global_communication_team_names[2 * communication_team + 1],
                  (int)global_communication_priority_names
                    [communication_priority][0],
                  (int)global_communication_priority_names
                    [communication_priority][1],
                  (int)global_communication_priority_names
                    [communication_priority][2],
                  near_player ? "-lo" : "-hi");

                slot = 2 * (8 * communication_team + communication_priority) +
                       near_player;
                if (speech_disabled[slot]) {
                  assert_halt_msg_at(
                    "communication_priority < _ai_communication_priority_yell",
                    "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x639,
                    communication_priority < 6);

                  if (ai_print_communication) {
                    const real *tolerance = communication_timer_tolerances
                      [communication_priority][near_player];

                    if (speech_disabled_reason[slot] == 0) {
                      crt_sprintf(timer_string, "chat:%.1f<%.1f",
                                  (double)chatter_seconds[communication_team],
                                  (double)tolerance[0]);
                    } else if (speech_disabled_reason[slot] == 1) {
                      crt_sprintf(timer_string, "talk:%.1f<%.1f",
                                  (double)talk_seconds[communication_team],
                                  (double)tolerance[1]);
                    } else if (speech_disabled_reason[slot] == 2) {
                      crt_sprintf(timer_string, "shout:%.1f<%.1f",
                                  (double)shout_seconds[communication_team],
                                  (double)tolerance[3]);
                    } else {
                      crt_sprintf(timer_string, "<err>");
                    }
                    csstrcat(debug_string,
                             csprintf(error_string_buffer, "[%s/%d %s %s] ",
                                      dialogue_get_vocalization_name(
                                        usage->vocalization_type, 1),
                                      (int)dialogue_index, team_string,
                                      timer_string));
                  }
                  valid = false;
                } else {
                  candidate_delay = speech_delay[slot];

                  if (communication_priority < 7) { /* < exclaim */
                    dialogue_event_status_t *event =
                      &global_dialogue_events[dialogue_index * 2 +
                                              communication_team];

                    if (event->last_time_spoken != -1) {
                      repeat_rating =
                        (real)(event_time - event->last_time_spoken) *
                        0.0011111111f;
                      if (repeat_rating < 0.0f) {
                        repeat_rating = 0.0f;
                      } else if (repeat_rating > 1.0f) {
                        repeat_rating = 1.0f;
                      }
                    }

                    if ((game_connection() != 0 ||
                         !ai_debug_communication_timeout_disabled) &&
                        event->disable_until_time != -1) {
                      int remaining = event->disable_until_time - event_time;

                      if (near_player) {
                        remaining += 30;
                      }
                      if (remaining > 0) {
                        if (ai_print_communication) {
                          csstrcat(
                            debug_string,
                            csprintf(error_string_buffer,
                                     "[%s/%d %s-d-dis/%d] ",
                                     dialogue_get_vocalization_name(
                                       usage->vocalization_type, 1),
                                     (int)dialogue_index,
                                     global_communication_team_names
                                       [2 * communication_team + 1],
                                     remaining));
                        }
                        valid = false;
                      }
                    }
                  }
                }
              }
            }
          }
        }

        if (valid) {
          short play_delay = (short)(int)(
            communication_play_delays[usage->protagonist_type] * 30.0f);
          short delay_time;
          short ai_delay_time;

          if (subject_race == 1 && !is_reply) { /* _race_player */
            play_delay = (short)(play_delay + 30);
          }
          delay_time = (short)(play_delay + candidate_delay);
          ai_delay_time = (short)((int)((usage->flags & 4) /* immediate */
                                          ? 0.0f
                                          : communication_notification_delays
                                                [communication_priority] *
                                              30.0f) +
                                  candidate_delay);

          switch (usage->recipient_look_direction) {
          case 1: /* subject */
            if (subject_unit_index != -1) {
              look_type = 1; /* _ai_information_look_unit */
              look_unit_index = subject_unit_index;
            }
            break;

          case 2: /* protagonist */
            if (protagonist_unit_index != -1) {
              look_type = 1;
              look_unit_index = protagonist_unit_index;
            }
            break;

          case 3: /* target */
            if (recipient_unit_index != -1) {
              look_type = 1;
              look_unit_index = recipient_unit_index;
            }
            break;

          case 4: /* danger */
            if (subject_actor_index != -1) {
              actor_t *danger_actor =
                (actor_t *)datum_get(actor_data, subject_actor_index);

              if (danger_actor->danger_zone_danger_type > 0) {
                assert_halt_msg_at(
                  "subject_actor->danger_zone.object_index != NONE",
                  "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x6b0,
                  danger_actor->danger_zone_object_index != -1);
                look_type = 2; /* _ai_information_look_object */
                look_unit_index = danger_actor->danger_zone_object_index;
              }
            }
            break;
          }

          if (look_type != 0) {
            recipient_look_priority = usage->recipient_look_priority;
            if (recipient_look_priority == -1 ||
                recipient_look_priority == 1) {
              recipient_look_priority =
                communication_recipient_default_look_priorities
                  [communication_priority];
            }
          }

          protagonist_look_priority = usage->protagonist_look_priority;
          if (protagonist_look_priority == -1 ||
              protagonist_look_priority == 1) {
            protagonist_look_priority =
              communication_protagonist_default_look_priorities
                [communication_priority];
          }

          {
            short candidate_vocalization_type = usage->vocalization_type;
            short animation_type = usage->animation_type;
            int sound_definition_index = -1;
            real speech_weight = 1.0f;
            real animation_weight = 1.0f;

            if (!is_reply) {
              play_type = ai_communication_consider_speech(
                &sound_definition_index, &candidate_vocalization_type,
                speech_priority, protagonist_unit_index,
                communication_priority, delay_time,
                (char)(usage->flags & 1), 0, &speech_weight, speech_string);
              if (play_type == 0) { /* _unit_play_speech_none */
                if (ai_print_communication) {
                  csstrcat(debug_string,
                           csprintf(error_string_buffer, "[%s/%d u-%s-%s] ",
                                    dialogue_get_vocalization_name(
                                      usage->vocalization_type, 1),
                                    (int)dialogue_index,
                                    speech_weight > 0.0f ? "dis" : "n/a",
                                    speech_string));
                }
                valid = false;
              } else if (animation_type != -1 &&
                         (unsigned char)unit_test_animation_impulse(
                           protagonist_unit_index, animation_type) != 0) {
                if (protagonist_actor_index != -1) {
                  assert_halt_at("c:\\halo\\SOURCE\\ai\\ai_communication.c",
                                 0x6f2, protagonist_actor);
                  /* 2 = _action_class_transitory; +0x6a mode 1 = asleep */
                  if (actor_get_action_priority_flag(
                        protagonist_actor_index) != 2 &&
                      protagonist_actor->field_06a != 1) {
                    animation_weight = 2.0f;
                  }
                } else {
                  animation_weight = 2.0f;
                }
              }
            }

            if (valid) {
              real weight = animation_weight * speech_weight * player_rating *
                            usage->weight * reply_rating * repeat_rating;

              if (weight > 0.0f) {
                ai_communication_possibility_t *possibility;

                if (possibility_count >= MAXIMUM_COMMUNICATION_POSSIBILITIES) {
                  error(2,
                        "ai_communication_event: type %d (%s) overflowed "
                        "MAXIMUM_COMMUNICATION_POSSIBILITIES (%d)",
                        (int)communication_type,
                        global_communication_type_names[communication_type],
                        MAXIMUM_COMMUNICATION_POSSIBILITIES);
                  break;
                }

                possibility = &possibilities[possibility_count];
                possibility->dialogue_type_index = dialogue_index;
                possibility->is_reply = is_reply;
                possibility->weight = weight;
                possibility->protagonist_unit_index = protagonist_unit_index;
                possibility->protagonist_actor_index = protagonist_actor_index;
                possibility->animation_type = usage->animation_type;
                possibility->recipient_unit_index = recipient_unit_index;
                possibility->preselected_reply_actor_index = reply_actor_index;
                possibility->priority = speech_priority;
                possibility->delay_time = delay_time;
                possibility->ai_delay_time = ai_delay_time;
                possibility->vocalization_type = candidate_vocalization_type;
                possibility->sound_definition_index = sound_definition_index;
                possibility->recipient_look_priority = recipient_look_priority;
                possibility->look_type = look_type;
                possibility->look_unit_index = look_unit_index;
                possibility->play_type = play_type;
                possibility->protagonist_look_priority =
                  protagonist_look_priority;
                possibility->interrupts =
                  (boolean)(((uint8_t)usage->flags >> 1) & 1); /* force */
                if (possibility->interrupts) {
                  any_forced_possibility = true;
                }
                possibility_count++;

                if (ai_print_communication) {
                  csstrcat(
                    debug_string,
                    csprintf(error_string_buffer,
                             "[%s/%d %s del%d w:%.1f%s s%.1f p%.1f%s a%.1f "
                             "rc%.1f rp%.1f t%.1f] ",
                             dialogue_get_vocalization_name(
                               usage->vocalization_type, 1),
                             (int)dialogue_index, team_string,
                             (int)candidate_delay, (double)usage->weight,
                             possibilities[possibility_count - 1].interrupts
                               ? "F"
                               : "",
                             (double)speech_weight, (double)player_rating,
                             is_reply ? "PLAYER" : "",
                             (double)animation_weight, (double)repeat_rating,
                             (double)reply_rating,
                             (double)possibilities[possibility_count - 1]
                               .weight));
                }
                total_possibility_weight += weight;
                any_protagonist_considered = true;
              }
            }
          }
        }

        if (!valid) {
          any_protagonist_considered = true;
        }
      }
    }
  }

  if (ai_print_communication_player && !player_involved) {
    suppress_output = true;
  }

  if (possibility_count > 0) {
    ai_information_packet_t information;

    selected_possibility = possibilities;
    if (any_forced_possibility) {
      real original_weight = total_possibility_weight;
      short forced_count = 0;
      short index;

      total_possibility_weight = 0.0f;
      for (index = 0; index < possibility_count; index++) {
        if (possibilities[index].interrupts) {
          forced_count++;
        } else {
          possibilities[index].weight = 0.0f;
        }
        total_possibility_weight += possibilities[index].weight;
      }
      assert_halt_msg_at("total_possibility_weight > 0.0f",
                         "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x76e,
                         total_possibility_weight > 0.0f);
      if (ai_print_communication) {
        csstrcat(debug_string,
                 csprintf(error_string_buffer, "[%d/%.1f force %d/%.1f] ",
                          (int)possibility_count, (double)original_weight,
                          (int)forced_count,
                          (double)total_possibility_weight));
      }
    }

    if (possibility_count > 1) {
      real cumulative_weight = 0.0f;
      real random_weight =
        random_math_real((unsigned int *)get_global_random_seed_address()) *
        total_possibility_weight;
      short index;

      for (index = 0; index < possibility_count - 1; index++) {
        cumulative_weight += possibilities[index].weight;
        if (cumulative_weight >= random_weight) {
          break;
        }
      }
      selected_possibility = &possibilities[index];
      if (ai_print_communication) {
        csstrcat(debug_string,
                 csprintf(error_string_buffer, "[rnd%.1f tot%.1f cum%.1f@%d] ",
                          (double)random_weight,
                          (double)total_possibility_weight,
                          (double)cumulative_weight, (int)index));
      }
    }

    information.communication_type = communication_type;
    information.target_unit_index = selected_possibility->recipient_unit_index;
    information.damage_category = damage_type;
    information.dialogue_type_index = selected_possibility->dialogue_type_index;
    information.updated_dialogue_timers = true;
    information.look_priority = selected_possibility->recipient_look_priority;
    information.look_type = selected_possibility->look_type;
    information.look_unit_index = selected_possibility->look_unit_index;
    information.information_type =
      information_type == -1 ? 0 : information_type;
    if (!information_data) {
      csmemset(&information.information_data, 0,
               sizeof(information.information_data));
    } else {
      information.information_data = *information_data;
    }

    if (selected_possibility->is_reply) {
      assert_halt_msg_at(
        "selected_possibility->preselected_reply_actor_index != NONE",
        "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0x7a7,
        selected_possibility->preselected_reply_actor_index != -1);
      vocalization_type = selected_possibility->vocalization_type;
      ai_communication_started(
        selected_possibility->protagonist_unit_index,
        (uint16_t)selected_possibility->priority,
        (uint16_t)vocalization_type, &information);
      ai_communication_notify(
        selected_possibility->protagonist_unit_index,
        (uint16_t)selected_possibility->priority,
        (uint16_t)vocalization_type, &information);
      ai_communication_finished(
        selected_possibility->protagonist_unit_index,
        selected_possibility->priority, vocalization_type, true,
        selected_possibility->preselected_reply_actor_index, &information);
    } else {
      unit_speech_item_t speech;
      int speaker_unit_index;
      int recipient_unit_index = selected_possibility->recipient_unit_index;
      int protagonist_actor_index;
      short speech_priority = selected_possibility->priority;
      short dialogue_type_index = selected_possibility->dialogue_type_index;
      short protagonist_look_priority;

      speech.priority = speech_priority;
      speech.delay_time = selected_possibility->delay_time;
      speech.sound_definition_index =
        selected_possibility->sound_definition_index;
      speech.pause_time = 24;
      vocalization_type = selected_possibility->vocalization_type;
      speech.vocalization_type = vocalization_type;
      speech.ai_notification_delay = selected_possibility->ai_delay_time;
      speech.ai = information;

      speaker_unit_index = selected_possibility->protagonist_unit_index;
      unit_speak(speaker_unit_index,
                 (short)(uint16_t)selected_possibility->play_type, &speech);

      if (selected_possibility->animation_type != -1) {
        unit_data_t *speaker_unit =
          (unit_data_t *)object_get_and_verify_type(speaker_unit_index, 3);
        real_vector2d alignment;

        alignment.i = speaker_unit->object.forward.x; /* object.forward */
        alignment.j = speaker_unit->object.forward.y;
        if (recipient_unit_index != -1) {
          real_point3d speaker_head;
          real_point3d target_head;
          real magnitude;

          unit_get_head_position(speaker_unit_index, &speaker_head.x);
          unit_get_head_position(recipient_unit_index, &target_head.x);
          alignment.i = target_head.x - speaker_head.x;
          alignment.j = target_head.y - speaker_head.y;
          /* inlined normalize2d: 0x2533d0 = _real_epsilon (double) */
          magnitude = (real)x87_sqrtd(alignment.i * alignment.i +
                                      alignment.j * alignment.j);
          if (x87_fabs(magnitude) < 0.0001f) {
            magnitude = 0.0f;
          } else {
            real inverse = 1.0f / magnitude;

            alignment.i *= inverse;
            alignment.j *= inverse;
          }
          if (magnitude == 0.0f) {
            alignment.i = speaker_unit->object.forward.x;
            alignment.j = speaker_unit->object.forward.y;
          }
        }
        unit_apply_animation_impulse(
          speaker_unit_index,
          (uint16_t)selected_possibility->animation_type, &alignment);
      }

      protagonist_actor_index = selected_possibility->protagonist_actor_index;
      protagonist_look_priority =
        selected_possibility->protagonist_look_priority;
      if (protagonist_actor_index != -1 && protagonist_look_priority > 0 &&
          recipient_unit_index != -1 &&
          object_try_and_get_and_verify_type(recipient_unit_index, 3) !=
            NULL) {
        ai_communication_secondary_look_t look;
        int prop_index = prop_get_active_by_unit_index(protagonist_actor_index,
                                                       recipient_unit_index);
        int16_t prop_state = 0;

        if (prop_index != -1) {
          prop_state = ((prop_t *)datum_get(prop_data, prop_index))->state;
        }
        if (prop_index != -1 && prop_state >= 2 && prop_state <= 3) {
          look.type = 1;
          look.data.prop_index = prop_index;
        } else {
          look.type = 3;
          unit_get_head_position(recipient_unit_index, &look.data.point.x);
        }
        actor_look_secondary(protagonist_actor_index, 9,
                             protagonist_look_priority, (short *)&look);
      }

      ai_communication_update_speech_timers(speaker_unit_index,
                                            speech_priority, vocalization_type,
                                            dialogue_type_index, -1);
    }

    if (ai_print_communication) {
      csstrcat(debug_string,
               strupr(csprintf(error_string_buffer, ">>%s<<",
                               dialogue_get_vocalization_name(
                                 vocalization_type, 1))));
      if (!suppress_output) {
        error(2, debug_string);
      }
    }
  } else if (ai_print_communication) {
    if (any_requirements_failed && !any_protagonist_considered) {
      csstrcat(debug_string, requirements_string);
    }
    if (ai_debug_communication_focus_enable || !any_requirements_failed ||
        any_protagonist_considered) {
      csstrcat(debug_string, "NONE");
      if (!suppress_output) {
        error(2, debug_string);
      }
    }
  }
}
