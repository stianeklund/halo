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

/* FUN_00042d80 (0x42d80) — bool predicate: true when the prop keyed by
 * (object_handle, actor_handle) exists, is within a threshold distance, and
 * its field at +0x38 is 0 or 1.
 *
 * Confirmed (disasm 0x42d80-0x42de1):
 *   - No SUB ESP (frame is PUSH EBP; MOV EBP,ESP; PUSH EBX only), so params
 *     are read straight off the incoming stack slots: EBP+8 = param_1
 *     (pushed as ECX, second FUN_00064b40 arg), EBP+0xC (param_2) is never
 *     referenced anywhere in the function body, EBP+0x10 = param_3 (pushed
 *     as EAX, first FUN_00064b40 arg). BL is zeroed once up front (XOR BL,BL)
 *     and is the shared default (false) return value; every early-exit
 *     branch targets 0x42ddd (MOV AL,BL), so the real return is bool in AL,
 *     not the void the stale kb decl showed.
 *   - if (param_3 == -1) return false — CMP EAX,-1 / JZ before the call, so
 *     the -1 sentinel check is on the caller-supplied handle, not on
 *     FUN_00064b40's result.
 *   - FUN_00064b40(param_3, param_1, 1, 1): push order is EAX(param_3) last
 *     = first decl arg (actor_handle), ECX(param_1) = second decl arg
 *     (object_handle), then two literal 1s (create_if_missing, acknowledge).
 *     If the result is -1, return false.
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
 *   or for param_2's role (never read); kept as FUN_00042d80 with param_2
 *   named for its stack position only. */
bool FUN_00042d80(int param_1, int param_2, int param_3)
{
  int prop_index;
  char *prop;
  char result;

  (void)param_2;

  result = 0;
  if (param_3 != -1) {
    prop_index = FUN_00064b40(param_3, param_1, 1, 1);
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

/* FUN_00042df0 (0x42df0) — bool predicate over the same (object_handle,
 * actor_handle)-keyed prop lookup as sibling FUN_00042d80, but with the
 * threshold comparison direction and success/failure roles swapped: true
 * when the prop's distance field is past the threshold, OR its +0x38 field
 * is neither 0 nor 1; false when the prop is missing/unreachable, or the
 * field is within the threshold AND +0x38 is 0 or 1.
 *
 * Confirmed (disasm 0x42df0-0x42e51):
 *   - Same frame/param shape as FUN_00042d80: PUSH EBP; MOV EBP,ESP; PUSH EBX
 *     only (no SUB ESP). EBP+8 = param_1 (pushed as ECX, second
 *     FUN_00064b40 arg), EBP+0xC (param_2) is never referenced anywhere in
 *     the function body, EBP+0x10 = param_3 (pushed as EAX, first
 *     FUN_00064b40 arg). BL is zeroed once (XOR BL,BL) and is the shared
 *     false-return value; both early-exit branches and the final failure
 *     path target 0x42e4d (MOV AL,BL).
 *   - if (param_3 == -1) return false — CMP EAX,-1 / JZ before the call, so
 *     the -1 sentinel check is on the caller-supplied handle, not on
 *     FUN_00064b40's result.
 *   - FUN_00064b40(param_3, param_1, 1, 1): push order is EAX(param_3) last
 *     = first decl arg (actor_handle), ECX(param_1) = second decl arg
 *     (object_handle), then two literal 1s (create_if_missing, acknowledge)
 *     — identical call shape to FUN_00042d80. If the result is -1, return
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
 *     (same offset as FUN_00042d80, not the +0x24 "state" field from
 *     props.c); if it is 0 or 1, return false (JZ 0x42e4d on each compare);
 *     otherwise fall through to the same AL=1 success path at 0x42e48.
 *   - Success path: MOV AL,1 (0x42e48) then POP EBX/POP EBP/RET. Failure
 *     path: MOV AL,BL (0x42e4d, BL==0) then a separate POP EBX/POP EBP/RET
 *     — two distinct epilogues, not a shared one.
 * Uncertain: no evidence for this function's or prop+0x38's semantic name,
 *   or for param_2's role (never read); kept as FUN_00042df0 with param_2
 *   named for its stack position only. */
bool FUN_00042df0(int param_1, int param_2, int param_3)
{
  int prop_index;
  char *prop;
  char result;

  (void)param_2;

  result = 0;
  if (param_3 != -1) {
    prop_index = FUN_00064b40(param_3, param_1, 1, 1);
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

/* FUN_00042f40 (0x42f40) — thin wrapper returning actor_is_fighting for the
 * actor keyed by param_3.
 *
 * Confirmed (disasm 0x42f40-0x42f50):
 *   - No SUB ESP (frame is PUSH EBP; MOV EBP,ESP only); the single param
 *     read is EBP+0x10 (param_3), loaded into EAX and pushed as the sole
 *     arg to actor_is_fighting (0x3b150, in_kb, ported). EBP+8 (param_1)
 *     and EBP+0xC (param_2) are never referenced.
 *   - CALL actor_is_fighting; ADD ESP,0x4; POP EBP; RET — nothing
 *     overwrites EAX between the call and RET, so this function's return
 *     value is actor_is_fighting's bool-in-AL result verbatim.
 *   - Same 3-int-param frame shape as sibling FUN_00042d80/FUN_00042df0 in
 *     this object (both PUSH EBP; MOV EBP,ESP; read only EBP+0x10), kept as
 *     (param_1, param_2, param_3) for consistency.
 * Uncertain: no evidence for this function's semantic name, or for
 *   param_1/param_2's roles (never read); no callers found (xrefs empty),
 *   consistent with the siblings being reached only via an indirect table. */
bool FUN_00042f40(int param_1, int param_2, int param_3)
{
  (void)param_1;
  (void)param_2;

  return (bool)actor_is_fighting(param_3);
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

/* FUN_00042fa0 (0x42fa0) — true when param_1's unit and param_3's actor are
 * both currently targeting (target_target_prop_index) the same object.
 * Gated by sibling predicate FUN_00042d80 on (param_1, param_2, param_3);
 * returns false immediately if that gate fails, or if either actor lookup
 * or target lookup is unresolved (-1).
 *
 * Confirmed (disasm 0x42fa0-0x4304e):
 *   - PUSH ESI(param_3); PUSH EAX(param_2); PUSH EDI(param_1); CALL
 *     FUN_00042d80 — cdecl right-to-left, so the call is
 *     FUN_00042d80(param_1, param_2, param_3), same param order as this
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
 *   param_2's role (forwarded to FUN_00042d80 only, never read directly
 *   here); kept as FUN_00042fa0 with param_2 named for its stack position
 *   only, consistent with siblings FUN_00042d80/FUN_00042df0. */
bool FUN_00042fa0(int param_1, int param_2, int param_3)
{
  char *unit;
  int actor_handle_1;
  actor_t *actor1;
  actor_t *actor2;
  char *prop1;
  char *prop2;

  if (!FUN_00042d80(param_1, param_2, param_3)) {
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

/* FUN_00043050 (0x43050): cdecl predicate with three 32-bit stack
 * arguments.  The binary reads only the third argument at [EBP+0x10]; the
 * first two slots remain unused (same shape as FUN_00042e60 in this file).
 * It resolves that argument through actor_data, then requires
 * actor->field_06a == 3 and actor->field_06e < 4 (signed word compares).
 * Disasm 0x43050-0x43081: CMP EAX,-1/JZ -> false; datum_get(actor_data,
 * param_3); CMP word[EAX+0x6a],3/JNZ -> false; CMP word[EAX+0x6e],4/JL ->
 * true, else false. */
bool FUN_00043050(int param_1, int param_2, int param_3)
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

/* ai_communication_consider_speech (0x430d0) — decide whether a unit should
 * vocalize now, and scale the caller's weight accordingly.
 *
 * ABI (disasm 0x430d0-0x43266): three register arguments plus seven cdecl
 * stack slots. ECX -> ESI = vocalization_type, EAX -> EBX =
 * sound_definition_index_reference, EDX -> EDI = priority (the assert string
 * at 0xc1a names the first two and `weight`; the callee decl of unit_test_speech
 * names unit_handle/priority). Stack: [EBP+0x08] unit_handle, [EBP+0x0c]
 * param_5, [EBP+0x10] param_6, [EBP+0x14] param_7, [EBP+0x18] param_8,
 * [EBP+0x1c] weight, [EBP+0x20] failure_reason. Returns short: both exits do
 * MOV AX,BX where BX holds the play type (kb's earlier `void(void)` decl was
 * a placeholder).
 *
 * Confirmed details:
 *   - 0x4311e..0x43114 pushes seven args (ADD ESP,0x1c) in the order
 *     (unit_handle, priority, param_7, 1, &last_speech_time,
 *      vocalization_type, sound_definition_index_reference).
 *   - 0x43147 ADD ESP,0x10 merges unit_get_speech_priority_name's one-dword cleanup into
 *     crt_sprintf's three, so the name lookup stays nested in the call.
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

  play_type = unit_test_speech(unit_handle, priority, param_7, 1, &last_speech_time,
                           vocalization_type, sound_definition_index_reference);
  if (play_type == 0) {
    if (failure_reason != 0) {
      crt_sprintf(failure_reason, "nospch-%s", unit_get_speech_priority_name(priority));
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
        (float)((int)elapsed - (int)threshold) * *weight * *(float *)0x25634c;
    }
  }

  assert_halt_msg_at(
    "(play_type == _unit_play_speech_none) || (*weight > 0.0f)",
    "c:\\halo\\SOURCE\\ai\\ai_communication.c", 0xc49,
    play_type == 0 || *weight > *(float *)0x2533c0);
  return play_type;
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

/* actor_communication_team (0x43270) — classify an actor's communication
 * team from its actor-type definition flags. Confirmed via disasm
 * 0x43270-0x432ac: datum_get(actor_data, actor_handle) resolves the actor
 * record (no -1 guard on actor_handle, unlike FUN_00043050); the actor's
 * field_004 (int16_t, "meaning unproven") is passed to actor_type_get_race
 * (actor_type_definitions[actor_type]->+0x4 flags word, already ported in
 * actors.c). Bit 0x2 of that flags word (TEST AL,0x2) returns 0; bit 0x4
 * (TEST AL,0x4) returns 1; otherwise returns -1 (OR ECX,0xffffffff / MOV
 * AX,CX at 0x43292/0x432a8). Called through a `(short (*)(int))` function
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

/* FUN_000432b0 (0x432b0) — issue a primary "look at object" request for an
 * actor, reusing a caller-supplied prop handle when valid instead of always
 * looking one up. Called with a register-passed prop handle so a caller that
 * already resolved one (e.g. from an earlier prop_get_active_by_unit_index
 * call) can skip the redundant lookup.
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
 *   - Same look_buf convention as FUN_00014540/FUN_00043360 in this file:
 *     short[8] { int16_t type; int16_t pad; int data[3]; }; only
 *     look_buf[0] and *(int*)&look_buf[2] are ever written.
 *   - FUN_00027a60(EBX, [EBP+8], [EBP+0xc], &look_buf) at 0x43346: args
 *     pushed EDX(&look_buf), EAX([EBP+0xc]=priority), ECX([EBP+8]=
 *     look_type), EBX(actor_handle) — cdecl ADD ESP,0x10 (4 args).
 * Uncertain: no evidence for this function's semantic name; kept as
 *   FUN_000432b0 with params named for their forwarded role, matching the
 *   FUN_00043360 comment convention below. */
void FUN_000432b0(int prop_handle, int actor_handle, int object_handle,
                  short look_type, short priority)
{
  short look_buf[8]; /* [0]=type word, [2..7]=data (int handle or float[3]
                         position) */
  char *prop;
  short state;
  int use_prop_look;

  use_prop_look = 0;

  if (actor_handle == -1)
    return;
  if (priority < 1)
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
  FUN_00027a60(actor_handle, look_type, priority, look_buf);
}

/* FUN_00043360 (0x43360) — issue a secondary "look at object" request
 * (look_buf[0]=6) for an actor, gated on valid actor/object handles and a
 * positive priority. Called unconditionally from FUN_00043ea0 (0x43eef).
 *
 * Confirmed: register-arg gate at 0x43366-0x43373: CMP EDI,-1/JZ;
 *   TEST BX,BX/JLE; CMP ESI,-1/JZ — no incoming register store in this
 *   function's prologue, so EDI/ESI/BX are @<reg> parameters, not locals.
 * Confirmed: object_try_and_get_and_verify_type(ESI, -1) at 0x43378/0x4337d
 *   (cdecl, 2 args); NULL-result branch at 0x43380/0x43382.
 * Confirmed: look_buf layout matches the FUN_00014540 convention (this
 *   file's actor_looking.c, 0x14540): short[8] buffer, [0]=type tag,
 *   *(int*)&buf[2]=data[0]. Here only buf[0]=6 (MOV word [EBP-0x10],0x6 at
 *   0x4338e) and *(int*)&buf[2]=ESI (MOV dword [EBP-0xc],ESI at 0x43394) are
 *   written; buf[4..7] (data[1..2]) are left uninitialized, matching the
 *   original's single-store pattern — do not zero-fill them.
 * Confirmed: FUN_00027a60(EDI, [EBP+8], EBX, &look_buf) at 0x43397, args
 *   pushed EAX(&buf), EBX(priority), ECX([EBP+8]=look_type stack param),
 *   EDI(actor_handle) — cdecl ADD ESP,0x10 (4 args).
 * Uncertain: no evidence for this function's semantic name, nor for the
 *   buf[0]=6 tag's meaning (FUN_00027a60 only special-cases tag==1; tag=6
 *   is opaque here) or for the stack look_type parameter's caller-supplied
 *   value — kept as FUN_00043360 with params named for their forwarded
 *   role in FUN_00027a60's own signature. */
void FUN_00043360(short look_type, int actor_handle, int object_handle,
                  short priority)
{
  short look_buf[8];

  if (actor_handle != -1 && priority > 0 && object_handle != -1) {
    if (object_try_and_get_and_verify_type(object_handle, -1) != NULL) {
      look_buf[0] = 6;
      *(int *)&look_buf[2] = object_handle;
      FUN_00027a60(actor_handle, look_type, priority, look_buf);
    }
  }
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

/* FUN_00043740 (0x43740) — begin (or force-start) a scenario conversation.
 * Tries to allocate a fresh conversation datum; if the pool is full and the
 * caller passed a non-zero param_2, it evicts one running conversation
 * (lowest +0x4 byte, tie-broken by oldest +0xc timestamp), finishes it, and
 * reuses its handle.  The resulting datum records the scenario conversation
 * index at +0x2, an invalid line index (-1) at +0x48, param_2 at +0x4 and the
 * current game time at +0xc.  Returns the datum index, or -1 on failure.
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
int FUN_00043740(int16_t scenario_conversation_index, char param_2)
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
         (1 << (participant_index & 0x1f))) != 0) {
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
        if (*(int16_t *)(line + 4) == 1) {
          *(int32_t *)(conversation + 0x58) = *(int32_t *)(conversation + 0x10);
        } else if (*(int16_t *)(line + 4) == 2) {
          other_index = *(int16_t *)(line + 6);
          if (other_index >= 0 && (int)other_index < *participant_count) {
            actor_handle = *(int32_t *)(conversation + other_index * 4 + 0x28);
            if (actor_handle != -1) {
              actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
              *(int32_t *)(conversation + 0x58) = *(int32_t *)(actor + 0x18);
            }
          }
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
 *     is passed to unit_test_speech through a `short *` cast; EBP-0x8 is likewise
 *     a dword -1.
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
 *   "is fighting" predicate; the vocalization type passed to unit_test_speech is
 *   just that flag widened, and vocalization index 1 is a literal at this
 *   call site.
 * Uncertain: the meaning of the `1`/`0` byte-width literals in arguments 3
 *   and 4 of unit_test_speech is not recoverable from this call site. */
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
      FUN_00043ce0(actor_handle);
    }
    if (actor->field_6ce > 0 && --actor->field_6ce == 0) {
      vocalization_type = (fighting != '\0');
      sound_definition_index = -1;
      communication_count =
        unit_test_speech(actor->field_018, 1, 1, 0, NULL,
                     (short *)&vocalization_type, &sound_definition_index);
      if (communication_count > 0) {
        csmemset(communication, 0, 0x30);
        *(short *)(communication + 0x00) = 1;
        *(short *)(communication + 0x02) = (short)vocalization_type;
        *(int32_t *)(communication + 0x04) = sound_definition_index;
        ai_communication_packet_new(communication + 0x10);
        unit_speak(actor->field_018, communication_count, communication);
      }
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
 *   - TEST AL,0x2 / TEST AL,0x4 on actor_type_get_race's result select team index 0
 *     and 1 respectively; neither bit set returns without touching anything.
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
 *   ever handed to dialogue_get_vocalization_name for the debug line.  Neither meaning is
 *   proven by a string or assert at this call site. */
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
    FUN_00043ce0(*(int32_t *)(unit + 0x1a4));
    actor = datum_get(*(data_t **)0x6325a4, *(int32_t *)(unit + 0x1a4));
    communication_flags =
      actor_type_get_race((int16_t) * (uint16_t *)((char *)actor + 4));
    if ((communication_flags & 2) == 0) {
      if ((communication_flags & 4) == 0) {
        return;
      }
      team_index = 1;
    } else {
      team_index = 0;
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
              *(char **)(0x2c8d68 + (int)team_index * 8), unit_get_speech_priority_name(param_2),
              (int)dialogue_type_index, dialogue_get_vocalization_name(param_3, 1), speech_kind,
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

/* ai_communication_find_global_actor_to_talk (0x458f0) — scan every live actor
 * and return the datum handle of the best-scoring conversation partner.
 *
 * Walks the global encounter/actor iterator, filters candidates by team
 * relationship, scores each survivor with FUN_000454a0, and keeps the highest
 * score (strictly greater). Returns -1 when nothing scores above 0.0f.
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
 * Confirmed: ADD ESP,0xc at 0x4593d cleans encounter_iterator_next(8) +
 *   actor_iterator_next(4) together (cdecl cleanup mis-grouping).
 * Confirmed: game_allegiance_get_team_is_friendly runs before the selector
 *   dispatch (CALL 0x45956, MOVSX 0x4595b), so its side effect happens in
 *   every mode even where mode 0 discards the result.
 * Confirmed selector dispatch at 0x45962-0x4599c:
 *   0 -> match = (actor->field_03e == team)  (CMP word[ESI+0x3e],BX / SETZ)
 *   1 -> match = !friendly                   (TEST AL,AL / SETZ)
 *   2 -> match = friendly                    (falls through to TEST AL,AL)
 *   else -> assert "!\"unreachable\"" line 0xdfd = 3581, then system_exit(-1).
 * Confirmed: ADD ESP,0x2c at 0x459d4 = 11 stack dwords into FUN_000454a0, with
 *   MOV EAX,EDI at 0x459ca supplying its @<eax> register arg. cdecl push order
 *   (0x459a7-0x459c9) reverses to
 *   (iter+0x14, vec_a, param_2, vec_b, param_3..param_9).
 * Confirmed: FCOM [EBP-0x8] / FNSTSW AX / TEST AH,0x41 / JNZ at 0x459d1 keeps
 *   the candidate only when the returned score is strictly greater than
 *   best_score (C3|C0 clear); the reject arm is FSTP ST0 at 0x459e9.
 * Confirmed: FUN_000454a0 prologue (PUSH EBP / MOV EBP,ESP / SUB ESP,0x10 /
 *   PUSH EBX / PUSH ESI / MOV ESI,EAX / PUSH EDI) saves EBX, ESI and EDI, so
 *   EAX is its only register parameter.
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
  encounter_iterator_next(iter, 1);
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
      score = FUN_000454a0(object_handle, *(int *)(iter + 0x14), vec_a, param_2,
                           vec_b, param_3, param_4, param_5, param_6, param_7,
                           param_8, param_9);
      if (score > best_score) {
        best_score = score;
        best_handle = *(int *)(iter + 0x14);
      }
    }
  }
  return best_handle;
}

/* ai_conversation (0x46b60) — script entry point that starts a scenario
 * conversation by index.  Validates the 16-bit index against the scenario
 * tag's conversation block count at +0x468, allocates/force-starts the
 * conversation datum via FUN_00043740, then tries to begin it.  Returns
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
 *   - FUN_00043740 (0x46b8f): pushes are [EBP+0xc] then [EBP+8], i.e.
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
 * Uncertain: the meaning of param_2 beyond FUN_00043740's force-start flag,
 *   and of ai_conversation_finish's ('\1','\0') argument pair here. */
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
    conversation_handle = FUN_00043740((int16_t)param_1, (char)param_2);
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
