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

  (void)param_2;

  if (param_3 == -1) {
    return false;
  }
  prop_index = FUN_00064b40(param_3, param_1, 1, 1);
  if (prop_index == -1) {
    return false;
  }
  prop = (char *)datum_get(prop_data, prop_index);
  if (*(float *)(prop + 0x11c) < *(float *)0x254cc4) {
    if (*(int16_t *)(prop + 0x38) == 0 || *(int16_t *)(prop + 0x38) == 1) {
      return true;
    }
  }
  return false;
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

  (void)param_2;

  if (param_3 == -1) {
    return false;
  }
  prop_index = FUN_00064b40(param_3, param_1, 1, 1);
  if (prop_index == -1) {
    return false;
  }
  prop = (char *)datum_get(prop_data, prop_index);
  if (*(float *)(prop + 0x11c) > *(float *)0x254cc4) {
    return true;
  }
  if (*(int16_t *)(prop + 0x38) == 0 || *(int16_t *)(prop + 0x38) == 1) {
    return false;
  }
  return true;
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
