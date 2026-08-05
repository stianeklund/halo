
/* ai_debug_dispose: free actor_debug_array and actor_path_debug_array.
 *
 * Confirmed: __FILE__ = "c:\halo\SOURCE\ai\ai_debug.c"
 *   line 0xa0 (160) — actor_debug_array free
 *   line 0xa6 (166) — actor_path_debug_array free
 * Called from ai_dispose (0x3f6f0). */
void ai_debug_dispose(void)
{
  if (*(void **)0x331f58 != NULL) {
    debug_free(*(void **)0x331f58, "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0xa0);
    *(void **)0x331f58 = NULL;
  }
  if (*(void **)0x331f5c != NULL) {
    debug_free(*(void **)0x331f5c, "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0xa6);
    *(void **)0x331f5c = NULL;
  }
}

/* ai_debug_dispose_from_old_map: if a valid scenario is loaded and a current
 * encounter is selected (DAT_005ac9f4 != -1), copy the encounter name from the
 * scenario tag block into DAT_005ac9d2 and clear the dirty flag. Otherwise zero
 * the name buffer via csstrcpy with empty string.
 *
 * No __FILE__ string.  Called from ai_dispose_from_old_map (0x3f720) and
 * ai_handle_editing. */
void ai_debug_dispose_from_old_map(void)
{
  void *scenario;
  void *encounter;

  scenario = FUN_0018e3b0();
  if (scenario != NULL && *(int32_t *)0x5ac9f4 != -1) {
    encounter =
      tag_block_get_element((void *)((char *)scenario + 0x42c),
                            (int)(*(uint32_t *)0x5ac9f4 & 0xffff), 0xb0);
    csstrncpy((char *)0x5ac9d2, encounter, 0x20);
    *(uint8_t *)0x5ac9f1 = 0;
    return;
  }
  csstrcpy((char *)0x5ac9d2, (const char *)0x25386f);
}

/* ai_debug_clear_storage: assert that both debug arrays are allocated, then
 * zero them. Asserts actor_debug_array != NULL (line 0xd0 = 208) and
 * actor_path_debug_array != NULL (line 0xd3 = 211) before zeroing each.
 *
 * Confirmed: __FILE__ = "c:\halo\SOURCE\ai\ai_debug.c" (0x25ab74)
 *   line 0xd0 (208) — actor_debug_array assert
 *   line 0xd3 (211) — actor_path_debug_array assert
 * Called from ai_debug_initialize_for_new_map (ai_debug.obj, 0x4c0f0).
 *
 * Note: decompiler showed csmemset size for path array as &DAT_00394f80
 * (treating immediate as address dereference).  Disassembly confirms
 * PUSH 0x394f80 — it is a literal immediate size, not a pointer. */
void ai_debug_clear_storage(void)
{
  if (*(void **)0x331f58 == NULL) {
    display_assert("actor_debug_array", "c:\\halo\\SOURCE\\ai\\ai_debug.c",
                   0xd0, 1);
    system_exit(-1);
  }
  csmemset(*(void **)0x331f58, 0, 0x657c00);
  if (*(void **)0x331f5c == NULL) {
    display_assert("actor_path_debug_array", "c:\\halo\\SOURCE\\ai\\ai_debug.c",
                   0xd3, 1);
    system_exit(-1);
  }
  csmemset(*(void **)0x331f5c, 0, 0x394f80);
}

/* ai_debug_actor_deleted: scan actor_path_debug_array (0x20 entries, stride
 * 0x1ca7c) and clear the active flag (offset +0xc) for any entry whose actor
 * handle (offset +0x0) matches actor_handle.
 *
 * No __FILE__ string.  Called from actor_delete (actors.obj, 0x3cc10). */
void ai_debug_actor_deleted(int actor_handle)
{
  char *base;
  int off;
  int i;

  base = *(char **)0x331f5c;
  off = 0;
  for (i = 0x20; i != 0; i--) {
    if (*(char *)(base + off + 0xc) != '\0' &&
        *(int *)(base + off) == actor_handle) {
      *(char *)(base + off + 0xc) = '\0';
      base = *(char **)0x331f5c;
    }
    off += 0x1ca7c;
  }
}

/* ai_debug_get_newest_path_storage (0x490c0) — scan the 0x20-entry
 * actor_path_debug_array (base *(char**)0x331f5c, stride 0x1ca7c) for the valid
 * entry belonging to actor_handle with the largest value at +0x4, and return a
 * pointer to it.  Returns NULL when no entry matches.
 *
 * Record fields used (matching ai_debug_get_path_storage at 0x49120, which
 * writes them):
 *   +0x0  int   actor handle key
 *   +0x4  int   creation stamp from game_time_get() — larger == newer
 *   +0xc  char  valid flag
 *
 * No __FILE__ string, no asserts, no calls (pure leaf, 0x490c0..0x49117).
 *
 * Binary shape notes (disassembly is authoritative — Ghidra declared this
 * void(void) and dropped the EAX return):
 *   EBX = *(int *)0x331f5c is loaded ONCE before the loop, and the found path
 *   returns EBX + best_slot * 0x1ca7c, so the base is cached in a local.
 *   EAX walks the table as a cursor initialised with LEA EAX,[EBX+4], so all
 *   field accesses are cursor-relative: [EAX-4]=+0x0, [EAX]=+0x4, [EAX+8]=+0xc.
 *   Score test is CMP EDX,ESI / JLE skip — strict greater-than with the loaded
 *   value first, so ties keep the FIRST maximum encountered.
 *   Loop counter (CX) and slot index (DI) are 16-bit: CMP CX,0x20 / CMP DI,-1 /
 *   MOVSX EAX,DI before IMUL EAX,EAX,0x1ca7c. */
void *ai_debug_get_newest_path_storage(int actor_handle)
{
  char *base;
  char *cursor;
  int best_stamp;
  short best_slot;
  short i;

  best_slot = -1;
  best_stamp = -1;
  i = 0;
  base = *(char **)0x331f5c;
  cursor = base + 4;
  do {
    if (*(char *)(cursor + 8) != '\0' &&
        *(int *)(cursor - 4) == actor_handle &&
        *(int *)cursor > best_stamp) {
      best_stamp = *(int *)cursor;
      best_slot = i;
    }
    i++;
    cursor += 0x1ca7c;
  } while (i < 0x20);

  if (best_slot == (short)-1) {
    return 0;
  }
  return base + (int)best_slot * 0x1ca7c;
}

/* ai_debug_get_path_storage (0x49120) — find or allocate a path debug storage
 * slot for actor_handle. Searches 0x20 entries (stride 0x1ca7c) in the
 * actor_path_debug_array. Returns an exact match, first inactive slot, or
 * evicts the oldest entry. Returns NULL if eviction finds no slot. */
void *ai_debug_get_path_storage(int actor_handle)
{
  char *base;
  char *entry;
  short best_slot;
  short i;
  short oldest_slot;
  int oldest_time;
  int off;

  best_slot = -1;
  i = 0;
  do {
    base = *(char **)0x331f5c;
    entry = base + (int)i * 0x1ca7c;
    if (*(int *)entry == actor_handle && *(char *)(entry + 0xd) == '\0') {
      best_slot = i;
      goto found;
    }
    if (best_slot == (short)-1 && *(char *)(entry + 0xc) == '\0') {
      best_slot = i;
    }
    i++;
  } while (i < 0x20);

  if (best_slot == (short)-1) {
    oldest_slot = -1;
    oldest_time = 0x7fffffff;
    off = 0;
    i = 0;
    do {
      base = *(char **)0x331f5c;
      entry = base + off;
      if (*(char *)(entry + 0xc) == '\0') {
        display_assert("path->valid", "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0x123,
                       1);
        system_exit(-1);
      }
      if (*(int *)(entry + 4) < oldest_time) {
        oldest_time = *(int *)(entry + 4);
        oldest_slot = i;
      }
      off += 0x1ca7c;
      i++;
    } while (i < 0x20);
    best_slot = oldest_slot;
    if (best_slot == (short)-1) {
      return 0;
    }
  }

found:
  entry = *(char **)0x331f5c + (int)best_slot * 0x1ca7c;
  csmemset(entry, 0, 0x1ca7c);
  *(char *)(entry + 0xc) = 1;
  *(int *)entry = actor_handle;
  *(int *)(entry + 4) = game_time_get();
  return entry;
}

/* ai_debug_select_encounter: reset debug encounter state when encounter_idx
 * changes. Checks if the current encounter index (0x5ac9f4) differs from
 * encounter_idx; if so, updates the index, clears the debug-state byte at
 * 0x629d40, zeroes the 0x670-byte block at 0x629d44 and the 0x8000-byte block
 * at 0x62a3b4, then calls ai_debug_select_actor(encounter_idx, -1) to
 * reinitialize secondary state.
 *
 * No __FILE__ string.  Called from ai_debug_select_actor,
 * ai_debug_initialize_for_new_map, ai_debug_change_selected_encounter,
 * FUN_00054e40.
 *
 * Calling convention verified (ADD ESP,0x20 at 0x49267 covers 8 dwords):
 *   3 args to csmemset(0x629d44,...) + 3 args to csmemset(0x62a3b4,...) +
 *   2 args to ai_debug_select_actor = 8 dwords. ai_debug_select_actor is cdecl.
 *
 * Call-site verification:
 *   ai_debug_initialize_for_new_map @ 0x4c116: PUSH ESI (enc_idx) ->
 * encounter_idx [match] ai_debug_select_actor @ 0x4b1ca: PUSH EAX (param_1) ->
 * encounter_idx [match] */
void ai_debug_select_actor(int encounter_idx, int param_2);

void ai_debug_select_encounter(int encounter_idx)
{
  if (*(int32_t *)0x5ac9f4 != encounter_idx) {
    *(int32_t *)0x5ac9f4 = encounter_idx;
    *(uint8_t *)0x629d40 = 0;
    csmemset((void *)0x629d44, 0, 0x670);
    csmemset((void *)0x62a3b4, 0, 0x8000);
    ai_debug_select_actor(encounter_idx, -1);
  }
}

/* FUN_000494d0: set debug ray-test success flag.
 *
 * No __FILE__ string. Called from ai_debug_get_last_path (ray setup) and
 * FUN_000494e0 (ray render). */
void FUN_000494d0(char success)
{
  *(uint8_t *)0x5acab9 = success;
}

/* ai_debug_highlight_cluster (0x496c0): report the debug highlight color for a
 * BSP cluster.
 *
 * Confirmed: __FILE__ = "c:\halo\SOURCE\ai\ai_debug.c", line 0x1025 (4133) —
 * the "highlight_color" NULL check on the out parameter.
 *
 * Returns 0 (and writes nothing) unless the highlight-cluster debug flag
 * (0x5aca6c) is set and an encounter is selected (0x5ac9f4 != -1).  The
 * 0x200-byte cluster bit vector at 0x331f18 is rebuilt via FUN_00058fd0
 * whenever the cached game time (0x2c8e90) or cached encounter index
 * (0x2c8e8c) is stale.  If the queried cluster's bit is set the color depends
 * on byte +0xd of the selected encounter datum; otherwise the third color
 * constant is used.
 *
 * Call-site verification (all cdecl, caller-cleaned):
 *   0x4970a FUN_00058fd0, ADD ESP,0x14 (5 dwords).  Pushes, in reverse order,
 *     0x331f18, 0, 0x200, 0, EDX(=[0x5ac9f4]) -> C order
 *     (encounter_index, 0, 0x200, 0, (char *)0x331f18)  [match]
 *   0x49741 display_assert, no ADD ESP (noreturn).  Pushes 1, 0x1025,
 *     0x25ab74 (file), 0x25abec (reason)  [match]
 *   0x49748 system_exit, PUSH -1  [match]
 *   0x4977b datum_get, ADD ESP,8.  PUSH EDX(=[0x5ac9f4]) then PUSH
 *     EAX(=[0x5ab270]) -> datum_get(*(data_t **)0x5ab270, [0x5ac9f4]) [match]
 *   EDX is reloaded from [0x5ac9f4] at 0x49717 after the FUN_00058fd0 call
 *   (the frame has no `sub esp`, so no stack local exists to spill into), so
 *   the global is re-read rather than cached across the call.
 *
 * Store-offset table (out is a single 4-byte slot, held in ESI):
 *   out+0x00 <- [0x2ee6e0]   bit set, encounter byte +0xd != 0
 *   out+0x00 <- [0x2ee6d8]   bit set, encounter byte +0xd == 0
 *   out+0x00 <- [0x2ee6c8]   bit clear
 *
 * Inferred: the early-out path falls into POP EBP at 0x497ae *without* popping
 * ESI (ESI is pushed at 0x49728, after the rebuild block), so the flag/index
 * test is a plain early `return 0;` ahead of any use of `out`.  The three
 * success epilogues (0x49794, 0x497a1, 0x497ad) each MOV AL,1 and POP ESI.
 *
 * Bit test (0x4975a-0x49771): MOVSX EAX,word[EBP+8]; ECX=EAX&0x1f;
 * EDI=1<<CL; SAR EAX,5; TEST dword[EAX*4+0x331f18],EDI — the table is
 * uint32[] indexed by the *sign-extended* cluster index >> 5 (arithmetic). */
char ai_debug_highlight_cluster(int16_t cluster_index, void *out)
{
  int time;
  char *encounter;

  if (*(uint8_t *)0x5aca6c == 0 || *(int32_t *)0x5ac9f4 == -1) {
    return 0;
  }
  time = game_time_get();
  if (*(int32_t *)0x2c8e90 != time ||
      *(int32_t *)0x2c8e8c != *(int32_t *)0x5ac9f4) {
    FUN_00058fd0(*(int32_t *)0x5ac9f4, 0, 0x200, 0, (char *)0x331f18);
    *(int32_t *)0x2c8e90 = game_time_get();
    *(int32_t *)0x2c8e8c = *(int32_t *)0x5ac9f4;
  }
  if (out == NULL) {
    display_assert("highlight_color", "c:\\halo\\SOURCE\\ai\\ai_debug.c",
                   0x1025, 1);
    system_exit(-1);
  }
  if ((((uint32_t *)0x331f18)[(int)cluster_index >> 5] &
       (1u << (cluster_index & 0x1f))) != 0) {
    encounter = (char *)datum_get(*(data_t **)0x5ab270, *(int32_t *)0x5ac9f4);
    if (encounter[0xd] != 0) {
      *(void **)out = *(void **)0x2ee6e0;
      return 1;
    }
    *(void **)out = *(void **)0x2ee6d8;
    return 1;
  }
  *(void **)out = *(void **)0x2ee6c8;
  return 1;
}

/* ai_debug_idle_look_clear: reset the idle-look debug block at 0x6323d4 to
 * track a single actor handle.  Sets the "valid" byte flag from
 * (actor_handle != -1), stores the handle itself as a dword, and clears the
 * 16-bit property count.  This is the 1-argument variant of the same three
 * stores performed by ai_debug_select_actor (0x4b1b0) and
 * ai_debug_initialize_for_new_map (0x4c0f0).
 *
 * No __FILE__ string.  No CALLs, no FPU, no locals.  Called from
 * actor_looking.c (actor idle-look update) with the actor handle.
 *
 * Confirmed: cdecl, 1 stack arg at [EBP+0x8]; caller does the cleanup.
 *
 * Store-offset table (absolute addresses, 0x4a6e6..0x4a6fe):
 *   [0x6323d4] <- (actor_handle != -1)  byte   (CMP EAX,-1 / SETNZ CL /
 *                                               MOV byte ptr [0x6323d4],CL)
 *   [0x6323d8] <- EAX (actor_handle)    dword  (MOV [0x6323d8],EAX)
 *   [0x6323dc] <- 0                     word   (MOV word ptr [0x6323dc],0x0)
 *
 * Store widths are load-bearing: 0x6323d4 is a byte and 0x6323dc is a 16-bit
 * word.  Writing either as a dword would clobber the neighbouring fields. */
void ai_debug_idle_look_clear(int actor_handle)
{
  *(uint8_t *)0x6323d4 = (actor_handle != -1);
  *(int32_t *)0x6323d8 = actor_handle;
  *(uint16_t *)0x6323dc = 0;
}

/* ai_debug_idle_look_addprop: append one (index, score) pair to the idle-look
 * debug proposal list set up by ai_debug_idle_look_clear (0x4a6e0).  Asserts
 * the "valid" flag first, then appends only while the count is below the
 * 0x20-entry capacity (no lower-bound check; the compare is signed JGE).
 *
 * __FILE__ assert xref confirms the TU: c:\halo\SOURCE\ai\ai_debug.c, line
 * 0x13b1 (5041), reason "ai_debug.idle_look_valid".  Assert push order
 * (last push = first arg): PUSH 1 / PUSH 0x13b1 / PUSH 0x25ab74 (file) /
 * PUSH 0x25aeac (reason) -> display_assert; then PUSH -1 -> system_exit
 * (noreturn, no stack cleanup follows).
 *
 * Confirmed: cdecl, PUSH EBP / MOV EBP,ESP, no sub esp, no locals.
 * [EBP+0x8] = int index, [EBP+0xC] = float value (single FLD/FSTP
 * passthrough, genuinely a float — not a smuggled pointer).
 *
 * Store-offset table (absolute addresses):
 *   [0x6323e0 + count*4] <- index   dword (MOVSX EAX,AX; MOV [..EAX*4],..)
 *   [0x632460 + count*4] <- value   float (MOVSX EDX,word [0x6323dc] — the
 *                                   counter is RE-LOADED from memory between
 *                                   the two stores; FLD [EBP+0xC] / FSTP)
 *   [0x6323dc]           <- count+1 word  (INC word ptr [0x6323dc])
 *
 * Widths are load-bearing: 0x6323d4 is a byte flag and 0x6323dc is a SIGNED
 * 16-bit count (MOV AX / CMP AX,0x20 / JGE).  0x632460 == 0x6323e0 + 0x80,
 * i.e. the score array begins exactly one 32-entry dword array later. */
void ai_debug_idle_look_addprop(int index, float value)
{
  if (*(uint8_t *)0x6323d4 == 0) {
    display_assert("ai_debug.idle_look_valid",
                   "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0x13b1, 1);
    system_exit(-1);
  }
  if (*(int16_t *)0x6323dc < 0x20) {
    ((int32_t *)0x6323e0)[*(int16_t *)0x6323dc] = index;
    ((float *)0x632460)[*(int16_t *)0x6323dc] = value;
    (*(int16_t *)0x6323dc)++;
  }
}

/* ai_debug_update: per-tick AI debug update.  Three independent debug actions:
 *
 *   1. Camera-follow (0x5ac9fc):  acquire actor or LOS-hit target, then
 *      build follow-camera state via the 0x5dfc0-0x5ff70 family.
 *
 *   2. Guard-position update (0x5ac9c2):  shift guard position history
 *      arrays forward in all scenario squads; print count and clear.
 *
 *   3. Actor-variant reset (0x5ac9c3):  reset all starting-location actor
 *      variant fields to 0xffff; print count and clear.
 *
 * No __FILE__ string.  Called from ai_update (0x41180).
 *
 * Inferred: push-then-fstp float args at 0x4abf3-0x4abf8 (FSTP replaces
 * pushed dummy values with FPU-computed float values).
 * Register aliasing verified: EBX=1 set at 0x4ab44, used as arg to
 * object_try_and_get_and_verify_type at 0x4ab5d and as byte value 1 for flag
 * stores. */
void ai_debug_update(void)
{
  /* camera-reset flag */
  if (*(uint8_t *)0x5aca6a != '\0') {
    *(int32_t *)0x5accac = 0;
    *(int32_t *)0x5eccb0 = 0;
  }

  if (*(uint8_t *)0x5ac9fc != '\0') {
    /* camera-follow: actor-position path */
    if (*(uint8_t *)0x5ac9fd == '\0') {
      int actor = player_control_get_unit_index(0);
      if (actor != -1 && object_try_and_get_and_verify_type(actor, 1) != NULL) {
        float pos[3];
        int bone =
          biped_find_pathfinding_surface_index(actor, (vector3_t *)pos);
        if (bone != -1) {
          *(float *)0x5f91ac = pos[0];
          *(float *)0x5f91b0 = pos[1];
          *(float *)0x5f91b4 = pos[2];
          *(uint8_t *)0x5f91a8 = 1;
          *(int32_t *)0x5f91b8 = bone;
          *(int32_t *)0x5f91bc = actor;
        }
      }
    }

    /* camera-follow: LOS-hit path */
    if (*(uint8_t *)0x5ac9fe == '\0') {
      void *cam = observer_get_camera(0);
      if (cam != NULL) {
        float *fwd = *(float **)0x31fc50;
        float scale = *(float *)0x254cb8;
        float dir[3];
        char hitbuf[8];

        *(uint16_t *)0x5ac5d4 += 1;
        dir[0] = fwd[0] * scale;
        dir[1] = fwd[1] * scale;
        dir[2] = fwd[2] * scale;
        if (FUN_0014df70(0x21, (float *)cam, dir, -1, (int16_t *)hitbuf) != 0) {
          /* hitbuf offsets relative to local_20 (EBP-0x1c):
           * local_58 = EBP-0x58 = local_20 - 0x3c (+0x3c back from
           * local_20); Ghidra shows local_58/54/50/2c.
           * Confirmed from disasm: MOV EAX,[EBP-0x54] etc. */
          *(int32_t *)0x5f91c4 = *(int32_t *)(hitbuf + 0); /* slot 0 */
          *(uint8_t *)0x5f91c0 = 1;
          *(int32_t *)0x5f91c8 = *(int32_t *)(hitbuf + 4);
          *(int32_t *)0x5f91cc = *(int32_t *)(hitbuf - 4); /* Uncertain */
          *(int32_t *)0x5f91d0 = *(int32_t *)(hitbuf - 8); /* Uncertain */
          *(int32_t *)0x5f91d4 = 0;
        }
      }
    }

    /* build follow-camera if a target was acquired */
    if (*(uint8_t *)0x5f91a8 != '\0') {
      char cam_state[0x48];
      path_input_new(cam_state, 0x3e4ccccd, 0, *(int32_t *)0x5f91bc);
      path_input_set_start(cam_state, (void *)0x5f91ac, *(int32_t *)0x5f91b8);
      if (*(float *)0x2533c0 < *(float *)0x5aca00) {
        path_input_set_search_bounds(cam_state, *(int32_t *)0x5aca00);
      }
      if (*(uint8_t *)0x5aca04 != '\0') {
        int actor2 = player_control_get_unit_index(0);
        if (actor2 != -1) {
          vector3_t vpos;
          float outer, inner;
          object_get_and_verify_type(actor2, 3);
          object_get_world_position(actor2, &vpos);
          outer = (*(float *)0x5aca0c == *(float *)0x2533c0) ?
                    20.0f :
                    *(float *)0x5aca0c;
          inner = (*(float *)0x5aca08 == *(float *)0x2533c0) ?
                    8.0f :
                    *(float *)0x5aca08;
          path_input_set_attractor(cam_state, (float *)&vpos, inner, -1, outer);
        }
      }
      path_state_new(cam_state, (void *)0x5f91dc, (void *)0x60d2c4);
      if (*(uint8_t *)0x5f91c0 != '\0' && *(uint8_t *)0x5ac9ff == '\0') {
        FUN_0005e0d0((void *)0x5f91dc, (void *)0x5f91c4, *(int32_t *)0x5f91d0,
                     *(int32_t *)0x5aca10);
      }
      FUN_0005ff70((void *)0x5f91dc);
      if (*(uint8_t *)0x5f91c0 != '\0' && *(uint8_t *)0x5ac9ff != '\0') {
        FUN_0005e0d0((void *)0x5f91dc, (void *)0x5f91c4, *(int32_t *)0x5f91d0,
                     *(int32_t *)0x5aca10);
      }
      path_state_build_path((unsigned int)0x5f91dc, (unsigned int *)0x60d268);
      *(uint8_t *)0x5f91d8 = 1;
      *(uint8_t *)0x60d2d0 = 1;
      *(int32_t *)0x60d2c8 = game_time_get();
      *(int32_t *)0x60d2c4 = -1;
    }
  }

  /* guard-position update */
  if (*(uint8_t *)0x5ac9c2 != '\0' && game_in_editor() != 0) {
    int scenario = (int)global_scenario_get();
    int *squads = (int *)(scenario + 0x42c);
    float total = 0.0f;
    float si = 0.0f;
    if (*squads > 0) {
      int s = 0;
      do {
        int squad = (int)tag_block_get_element((void *)squads, s, 0xb0);
        int *firing = (int *)(squad + 0x80);
        if (*firing > 0) {
          int f = 0;
          do {
            int fp = (int)tag_block_get_element((void *)firing, f, 0xe8);
            int32_t *hist = (int32_t *)(fp + 0x6c);
            int n = 4;
            do {
              *hist = *(hist - 1);
              hist--;
              n--;
            } while (n != 0);
            *(int32_t *)(fp + 0x5c) = *(int32_t *)(fp + 0x54);
            total = (float)((int)total + 1);
            f++;
          } while (f < *firing);
        }
        si = (float)((int)si + 1);
        s = (int)(int16_t)si;
      } while (s < *squads);
    }
    console_printf(0, "updated all %d squads' guard positions. glory!",
                   (int)total);
    *(uint8_t *)0x5ac9c2 = '\0';
  }

  /* actor-variant reset */
  if (*(uint8_t *)0x5ac9c3 != '\0' && game_in_editor() != 0) {
    int scenario = (int)global_scenario_get();
    int *squads = (int *)(scenario + 0x42c);
    float total = 0.0f;
    int *lsq = squads;
    float si = 0.0f;
    if (*squads > 0) {
      int s = 0;
      do {
        int squad = (int)tag_block_get_element((void *)squads, s, 0xb0);
        int *firing = (int *)(squad + 0x80);
        float fi = 0.0f;
        if (*firing > 0) {
          int f = 0;
          do {
            int fp = (int)tag_block_get_element((void *)firing, f, 0xe8);
            int *starts = (int *)(fp + 0xd0);
            int k = 0;
            if (*starts > 0) {
              do {
                int sl = (int)tag_block_get_element((void *)starts, k, 0x1c);
                *(uint16_t *)(sl + 0x18) = 0xffff;
                total = (float)((int)total + 1);
                k++;
              } while (k < *starts);
            }
            fi = (float)((int)fi + 1);
            f = (int)(int16_t)fi;
            squads = lsq;
          } while (f < *firing);
        }
        si = (float)((int)si + 1);
        s = (int)(int16_t)si;
      } while (s < *squads);
    }
    console_printf(
      0, "reset the actor variant in all %d starting locations. glory!",
      (int)total);
    *(uint8_t *)0x5ac9c3 = '\0';
  }

  FUN_0004a030();
  FUN_0004a9f0();
}

/* ai_debug_select_encounter: reset debug encounter state when encounter_idx
 * changes. Checks if the current encounter index (0x5ac9f4) differs from
 * encounter_idx; if so, updates the index, clears the debug-state byte at
 * 0x629d40, zeroes the 0x670-byte block at 0x629d44 and the 0x8000-byte block
 * at 0x62a3b4, then calls ai_debug_select_actor(encounter_idx, -1) to
 * reinitialize secondary state.
 *
 * No __FILE__ string.  Called from ai_debug_select_actor,
 * ai_debug_initialize_for_new_map, ai_debug_change_selected_encounter,
 * FUN_00054e40.
 *
 * Call-site verification:
 *   ai_debug_initialize_for_new_map @ 0x4c116: PUSH ESI (enc_idx, int) ->
 * encounter_idx [match] ai_debug_select_actor @ 0x4b1ca: PUSH EAX (param_1,
 * int) -> encounter_idx [match]
 *
 * Stack cleanup: ADD ESP,0x20 (0x49267) covers 8 dwords:
 *   3 args to csmemset(0x629d44,...) + 3 args to csmemset(0x62a3b4,...) +
 *   2 args to ai_debug_select_actor = 8 dwords = 0x20 bytes. */

/* ai_debug_select_actor: reinitialize secondary encounter debug state when
 * either the encounter index or param_2 changes.  Calls
 * ai_debug_select_encounter(encounter_idx) to reset the primary per-encounter
 * debug block, then updates the secondary encounter index (0x5ac9f8), clears
 * the stride-loop byte array at 0x62a3b5 (0x200 entries, stride 0x40), and
 * stores param_2 into the 0x6323d8 globals block (with 0x6323d4 as a non-(-1)
 * boolean and 0x6323dc zeroed as a word).
 *
 * No __FILE__ string.  Called from ai_debug_select_encounter (0x49220),
 * FUN_0004b7a0, ai_debug_change_selected_actor, FUN_00054e20.
 *
 * Call-site verification (only one CALL):
 *   0x4b1ca: PUSH EAX — EAX set from [EBP+0x8] at 0x4b1b3 = encounter_idx
 *   -> ai_debug_select_encounter(encounter_idx)  [match]
 *   ADD ESP,0x4 confirms cdecl 1-arg cleanup.
 *
 * Store-offset table (absolute addresses):
 *   [0x5ac9f8] <- ESI (param_2)      dword
 *   [0x629d40] <- DL=0               byte  (XOR EDX,EDX)
 *   [0x62a3b5 + n*0x40] <- DL=0      byte  loop n=0..0x1ff
 *   [0x6323d4] <- (param_2 != -1)    byte  (SETNZ AL)
 *   [0x6323d8] <- ESI (param_2)      dword
 *   [0x6323dc] <- DX=0               word  (MOV word ptr [0x6323dc],DX) */
void ai_debug_select_actor(int encounter_idx, int param_2)
{
  uint8_t *p;
  int n;

  if (*(int32_t *)0x5ac9f4 != encounter_idx ||
      *(int32_t *)0x5ac9f8 != param_2) {
    ai_debug_select_encounter(encounter_idx);
    *(int32_t *)0x5ac9f8 = param_2;
    *(uint8_t *)0x629d40 = 0;
    p = (uint8_t *)0x62a3b5;
    n = 0x200;
    do {
      *p = 0;
      p += 0x40;
      n--;
    } while (n != 0);
    *(uint8_t *)0x6323d4 = (param_2 != -1);
    *(int32_t *)0x6323d8 = param_2;
    *(uint16_t *)0x6323dc = 0;
  }
}

/* ai_debug_initialize_for_new_map: look up the encounter named DAT_005ac9d2 in
 * the scenario encounter list, reset debug encounter state, then if the
 * selected encounter or secondary index changed, reinitialize via
 * ai_debug_select_encounter.
 *
 * No __FILE__ string.  Called from ai_initialize_for_new_map (0x41090).
 *
 * Store-offset table (0x4c116..0x4c15f):
 *   0x629d40       <- 0 (byte, XOR EDX,EDX)
 *   0x62a3b5+n*0x40 (n=0..0x1ff) <- 0 (byte, loop)
 *   0x6323d4       <- 0 (byte)
 *   0x6323d8       <- 0xffffffff (dword)
 *   0x6323dc       <- 0 (word, MOV word ptr) */
void ai_debug_initialize_for_new_map(void)
{
  int enc_idx;
  uint8_t *p;
  int n;

  enc_idx = encounter_get_by_name((char *)0x5ac9d2);
  ai_debug_clear_storage();
  if (*(int32_t *)0x5ac9f4 != enc_idx || *(int32_t *)0x5ac9f8 != -1) {
    ai_debug_select_encounter(enc_idx);
    *(int32_t *)0x5ac9f8 = -1;
    *(uint8_t *)0x629d40 = 0;
    p = (uint8_t *)0x62a3b5;
    n = 0x200;
    do {
      *p = 0;
      p += 0x40;
      n--;
    } while (n != 0);
    *(uint8_t *)0x6323d4 = 0;
    *(int32_t *)0x6323d8 = -1;
    *(uint16_t *)0x6323dc = 0;
  }
}