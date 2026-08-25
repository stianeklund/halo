
/* ai_debug_initialize (0x48e90): clear AI debug state, reset selections, and
 * allocate the actor and path debug arrays when they are not already present.
 *
 * Confirmed from disassembly:
 *   csmemset(0x5ac9c0, 0, 0x85b2c)
 *   actor_debug_array allocation: 0x657c00 bytes, source line 0x93
 *   actor_path_debug_array allocation: 0x394f80 bytes, source line 0x94
 *   display_assert(..., line 0x96, true) followed by system_exit(-1).
 * The global addresses remain raw because no corresponding declarations exist
 * in types.h. */
void ai_debug_initialize(void)
{
  csmemset((void *)0x5ac9c0, 0, 0x85b2c);
  *(int32_t *)0x5ac9f8 = -1;
  *(int32_t *)0x5ac9f4 = -1;
  *(int32_t *)0x5acab4 = 1;
  *(uint8_t *)0x5aca65 = 1;

  if (*(void **)0x331f58 == NULL) {
    *(void **)0x331f58 =
      debug_malloc(0x657c00, false, "c:\\halo\\SOURCE\\ai\\ai_debug.c",
                   0x93);
  }
  if (*(void **)0x331f5c == NULL) {
    *(void **)0x331f5c =
      debug_malloc(0x394f80, false, "c:\\halo\\SOURCE\\ai\\ai_debug.c",
                   0x94);
  }
  if (*(void **)0x331f58 != NULL && *(void **)0x331f5c != NULL) {
    return;
  }
  display_assert("actor_debug_array && actor_path_debug_array",
                 "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0x96, true);
  system_exit(-1);
}

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
    if (*(char *)(cursor + 8) != '\0' && *(int *)(cursor - 4) == actor_handle &&
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

/* FUN_00049280 (0x49280): draw a debug polyline through `count` entries plus
 * a per-entry "up" marker.
 *
 * No __FILE__ string. 2 register args (point@<ecx>, color@<ebx>) + 2 stack
 * args ([EBP+0x8] count (short), [EBP+0xc] entries); caller cleans (ADD
 * ESP after each CALL, not RET N).  Callees FUN_00189450 (line between two
 * points) and FUN_001893e0 (point + direction marker), both no-reg-arg,
 * already ported.
 *
 * Called 3x from FUN_0004c560 (0x4c774/0x4c79f/0x4c7ca; static disasm via
 * tools/audit/dump_caller_regsetup.py, ported=false so not decompiled here).
 * At every call site `point`(@<ecx>) is `lea ecx,[esi+0x28]` -- the same
 * anchor point across all 3 calls -- while `color`(@<ebx>) is one of three
 * table slots ([0x2ee6d0]/[0x2ee6d4]/[0x2ee6d8]) and (count, entries) are a
 * per-call (word count, dword pointer) pair read out of the caller's record.
 *
 * `entries` points 4 bytes before the first array element (confirmed by the
 * disassembly's `ADD EAX,0x4` on the incoming pointer before the first use);
 * each array element is 0x10 bytes (4 floats) further (`pfVar1 = pfVar1 + 4`
 * in float units, `ADD ESI,0x10` in the caller). With `entries` typed
 * `float *`, `entries + 1` reproduces the +4-byte skip exactly.
 *
 * Body, matching the two independent `if (0 < count)` tests in the
 * disassembly (0004928c and 000492ab) rather than merging them:
 *   if count > 0: line(point, entries+1, color, scale=0.1)
 *   for i in [0, count):
 *     if i > 0: line(entries+1+4*(i-1), entries+1+4*i, color, scale=0.1)
 *     marker(entries+1+4*i, dir=global_up_vector_ptr, scale=0.02, color)
 * 0x3dcccccd == 0.1f, 0x3ca3d70a == 0.02f (both confirmed float bit patterns
 * from the disassembly's PUSH immediates). PTR_DAT_0031fc44 is the known
 * global_up_vector_ptr (kb.json: `float *global_up_vector_ptr;`). */
void FUN_00049280(float *point, void *color, int16_t count, float *entries)
{
  float *pfVar1;
  int16_t i;

  if (0 < count) {
    FUN_00189450(1, point, entries + 1, color, 0.1f);
  }
  i = 0;
  if (0 < count) {
    pfVar1 = entries + 1;
    do {
      if (0 < i) {
        FUN_00189450(1, pfVar1 - 4, pfVar1, color, 0.1f);
      }
      FUN_001893e0(1, pfVar1, global_up_vector_ptr, 0.02f, color);
      i = i + 1;
      pfVar1 = pfVar1 + 4;
    } while (i < count);
  }
}

/* ai_debug_point3d_set: store three reals into a 3-float point.
 *
 * No __FILE__ string, no callees, no locals (the original has no `sub esp`).
 * 4 cdecl stack args at [EBP+0x8]=dst, +0xc=x, +0x10=y, +0x14=z; caller cleans.
 *
 * Store-offset table (derived from disassembly, EAX = [EBP+0x8] = dst):
 *   +0x00  <- FSTP from FLD [EBP+0xc]   (x)
 *   +0x04  <- ECX = MOV [EBP+0x10]      (y, moved as a raw dword)
 *   +0x08  <- EDX = MOV [EBP+0x14]      (z, moved as a raw dword)
 * Only the first component goes through the x87 stack; y/z are integer moves,
 * an MSVC scheduling artifact of the natural three-assignment source form.
 * The interleaved MOV ECX / MOV EDX between the stores is scheduling too.
 *
 * Name is descriptive (object-prefixed), not recovered from a string. */
void ai_debug_point3d_set(float *point, float x, float y, float z)
{
  point[0] = x;
  point[1] = y;
  point[2] = z;
}

/* ai_debug_get_last_path (0x493d0): arm the debug line-of-fire ray with a new
 * pair of endpoints.
 *
 * No __FILE__ string, no callees (pure leaf, zero CALLs), no FPU instructions,
 * and no locals -- the original frame is PUSH EBP / MOV EBP,ESP / ... / POP EBP
 * / RET with no `sub esp`.  2 cdecl stack args at [EBP+0x8] (loaded into ECX)
 * and [EBP+0xc] (loaded into EDX); caller cleans.
 *
 * Despite the kb name this is a setter: it publishes the two endpoints into the
 * debug ray block and resets the ray state.  Called from ai.c's line-of-fire
 * rendering block (guarded by the 0x5aca69 debug flag) with two float[3]s.
 *
 * Debug ray block layout (0x5acab8, 0x20 bytes; widths from the disassembly --
 * only the first two stores are byte-sized, everything from 0x5acabc on is
 * dword):
 *   +0x00  0x5acab8  uint8   armed flag        <- 1
 *   +0x01  0x5acab9  uint8   ray-test success  <- 0   (also written by
 *                                                     FUN_000494d0)
 *   +0x02           2 bytes padding
 *   +0x04  0x5acabc  float[3] endpoint A       <- vec_a[0..2]
 *   +0x10  0x5acac8  float[3] endpoint B       <- vec_b[0..2]
 *   +0x1c  0x5acad4  int32    counter/index    <- 0
 *
 * Store-offset table (derived from the disassembly, ECX = vec_a, EDX = vec_b):
 *   0x5acab8 <- MOV byte ptr, 1
 *   0x5acab9 <- XOR EAX,EAX ; MOV AL           (EAX=0 is kept live)
 *   0x5acabc <- [ECX+0x0]   dword move, no FLD/FSTP
 *   0x5acac0 <- [ECX+0x4]   dword move
 *   0x5acac4 <- [ECX+0x8]   dword move
 *   0x5acac8 <- [EDX+0x0]   dword move
 *   0x5acacc <- [EDX+0x4]   dword move
 *   0x5acad0 <- [EDX+0x8]   dword move
 *   0x5acad4 <- MOV EAX     dword zero, reusing the XOR-cleared EAX
 * EDX is reloaded from [EBP+0xc] between the [ECX+8] read and its store --
 * pure MSVC scheduling, no semantic content.
 *
 * Inferred: the two flag bytes are stored inline rather than through
 * FUN_000494d0 (there is no CALL in this function at all). */
void ai_debug_get_last_path(float *vec_a, float *vec_b)
{
  *(uint8_t *)0x5acab8 = 1;
  *(uint8_t *)0x5acab9 = 0;
  *(float *)0x5acabc = vec_a[0];
  *(float *)0x5acac0 = vec_a[1];
  *(float *)0x5acac4 = vec_a[2];
  *(float *)0x5acac8 = vec_b[0];
  *(float *)0x5acacc = vec_b[1];
  *(float *)0x5acad0 = vec_b[2];
  *(int32_t *)0x5acad4 = 0;
}

/* ai_debug_lineoffire_addpill (0x49430): append one pill (capsule) to the
 * debug line-of-fire overlay list.
 *
 * TU anchored by the __FILE__ xref "c:\halo\SOURCE\ai\ai_debug.c" in the
 * assert at 0x49441 (line 0xfc4 = 4036).
 *
 * 4 cdecl stack args, caller cleans; frame is PUSH EBP / MOV EBP,ESP with no
 * `sub esp` (no spilled locals).
 *
 * Param 3 is a FLOAT, not an int: 0x49469 does `FLD dword ptr [EBP+0x10]` and
 * 0x494bc does `FSTP dword ptr [EDX*4 + 0x5acc68]` -- the value travels
 * through the x87 stack into a float[16].  The kb decl previously said `int`
 * and the sole caller (ai.c) bit-punned a float through it; both are corrected
 * in this change.
 *
 * Pill list layout (parallel arrays, all indexed by the count at 0x5acad4,
 * which ai_debug_get_last_path resets to 0):
 *   0x5acab8  uint8         valid/armed flag (assert guard)
 *   0x5acad4  int32         pill count, bound 0x10
 *   0x5acad8  char[16]      per-pill flag
 *   0x5acae8  float[16][3]  endpoint A, stride 0xc  (LEA EAX+EAX*2, then *4)
 *   0x5acba8  float[16][3]  endpoint B, stride 0xc
 *   0x5acc68  float[16]     radius,     stride 4
 *
 * The count is re-read from memory five times (0x4945c bounds test + char
 * index, 0x49472 endpoint A, 0x49494 endpoint B, 0x494b6 radius, 0x494c3
 * increment); it is deliberately not hoisted into a local here.
 *
 * Store-offset table (from the disassembly; ECX = source vector, EAX = the
 * LEA'd destination base -- NOT from the decompiler's field labels):
 *   0x5acad8 + count      <- CL             from [EBP+0x14]  (byte store)
 *   0x5acae8 + count*0xc  <- [ECX+0x0/4/8]  dword moves, no x87
 *   0x5acba8 + count*0xc  <- [ECX+0x0/4/8]  dword moves, no x87
 *   0x5acc68 + count*4    <- FSTP           x87, FLD'd at 0x49469
 *   0x5acad4              <- INC EAX
 *
 * Uncertain: "pill" is from the kb name, not a recovered string; the two
 * endpoints plus a radius describe a capsule, which matches the sphere and
 * segment line-of-fire tests that feed this from ai.c. */
void ai_debug_lineoffire_addpill(float *vec_a, float *vec_b, float radius,
                                 char flag)
{
  float *dst;

  if (*(uint8_t *)0x5acab8 == 0) {
    display_assert("ai_debug.lineoffire_valid",
                   "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0xfc4, 1);
    system_exit(-1);
  }
  if (*(int32_t *)0x5acad4 < 0x10) {
    ((char *)0x5acad8)[*(int32_t *)0x5acad4] = flag;

    dst = (float *)0x5acae8 + *(int32_t *)0x5acad4 * 3;
    dst[0] = vec_a[0];
    dst[1] = vec_a[1];
    dst[2] = vec_a[2];

    dst = (float *)0x5acba8 + *(int32_t *)0x5acad4 * 3;
    dst[0] = vec_b[0];
    dst[1] = vec_b[1];
    dst[2] = vec_b[2];

    ((float *)0x5acc68)[*(int32_t *)0x5acad4] = radius;
    *(int32_t *)0x5acad4 = *(int32_t *)0x5acad4 + 1;
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

/* FUN_000494e0: render the stored debug line-of-sight ray.
 *
 * No __FILE__ string; the name is left as FUN_000494e0.  Behaviour: draws the
 * stored debug ray as one line, then one sphere per recorded hit.  Does
 * nothing unless the ray block armed flag (0x5acab8) is set.
 *
 * Debug ray block (see ai_debug_set_last_ray and FUN_000494d0 above):
 *   0x5acab8  uint8    armed flag
 *   0x5acab9  uint8    ray-test success flag (written by FUN_000494d0)
 *   0x5acabc  float[3] ray start
 *   0x5acac8  float[3] ray delta (start + delta = ray end)
 *   0x5acad4  int32    hit count
 *   0x5acad8  uint8[]  per-hit flag,    stride 1
 *   0x5acae8  float[3] per-hit point A, stride 0xc
 *   0x5acba8  float[3] per-hit point B, stride 0xc  (= point A array + 0xc0)
 *   0x5acc68  float    per-hit radius,  stride 4
 * The 0xc0 gap between the two point arrays is 16 entries of stride 0xc, so
 * the parallel arrays hold 16 hits.  Point B is addressed in the original as
 * EDI+0xc0 off the same walking pointer (the delinked reference has a single
 * relocation against 0x5acae8), not as a separate absolute base.
 *
 * Call-site verification (both cdecl, caller-cleaned):
 *   0x49543 FUN_00189270, ADD ESP,0x10 (4 dwords).  Pushes, in reverse order:
 *     color, LEA EBP-0xc (endpoint), 0x5acabc (start), 1 -> C order
 *     (1, (float *)0x5acabc, endpoint, color)  [match]
 *   0x49581 FUN_00189860, ADD ESP,0x14 (5 dwords).  Pushes, in reverse order:
 *     color, [ESI*4+0x5acc68], EDI+0xc0, EDI, 1 -> C order
 *     (1, point, point + 0xc0, radius, color)  [match]
 *   The radius push is a plain dword MOV of a float slot.  Ghidra prints a
 *   `(float)` cast on an int array there, which would be an FILD conversion;
 *   the disassembly has no FILD, so it is a raw float load.
 *
 * Store-offset table (endpoint is the only buffer: 3 floats at EBP-0xc):
 *   endpoint+0x0 (EBP-0xc) <- FLD [0x5acabc]; FADD [0x5acac8]
 *   endpoint+0x4 (EBP-0x8) <- FLD [0x5acac0]; FADD [0x5acacc]
 *   endpoint+0x8 (EBP-0x4) <- FLD [0x5acac4]; FADD [0x5acad0]
 *   All three chains are FADD (never FSUB), so there is no operand-order
 *   hazard.  The FSTPs are interleaved with the colour select purely by MSVC
 *   scheduling.
 *
 * The two colour selects use different pointer-global pairs and opposite
 * polarity: the line takes [0x2ee6d4] when the success flag is set and
 * [0x2ee6d0] otherwise, while each sphere takes [0x2ee6d0] when its own hit
 * flag is set and [0x2ee6d8] otherwise.  All three are pointer globals
 * (MOV reg,[imm32]), not addresses of colour constants.
 *
 * The hit count at 0x5acad4 is re-read from memory on every iteration (two
 * relocations against it in the delinked reference), so it stays in the loop
 * condition rather than being cached in a local. */
void FUN_000494e0(void)
{
  float endpoint[3];
  void *color;
  float *point;
  int i;

  if (*(uint8_t *)0x5acab8 != 0) {
    endpoint[0] = *(float *)0x5acabc + *(float *)0x5acac8;
    endpoint[1] = *(float *)0x5acac0 + *(float *)0x5acacc;
    endpoint[2] = *(float *)0x5acac4 + *(float *)0x5acad0;
    color = *(void **)0x2ee6d4;
    if (*(uint8_t *)0x5acab9 == 0) {
      color = *(void **)0x2ee6d0;
    }
    FUN_00189270(1, (float *)0x5acabc, endpoint, color);
    i = 0;
    if (0 < *(int32_t *)0x5acad4) {
      point = (float *)0x5acae8;
      do {
        color = *(void **)0x2ee6d0;
        if (((uint8_t *)0x5acad8)[i] == 0) {
          color = *(void **)0x2ee6d8;
        }
        FUN_00189860(1, point, point + 48, ((float *)0x5acc68)[i], color);
        i++;
        point += 3;
      } while (i < *(int32_t *)0x5acad4);
    }
  }
}

/* FUN_000495b0 (0x495b0): render the second AI debug ray/sphere/path block.
 *
 * Gated on the armed flag at 0x5f8cb4.  Draws a point marker at 0x5f8cb8, a
 * vector from 0x5f8cb8 along 0x5f8cc4, then a list of spheres and a polyline.
 * This is a distinct global block from the 0x5acab8 one used by FUN_000494e0:
 *   0x5f8cb4  uint8    armed flag
 *   0x5f8cb5  uint8    secondary flag (selects the polyline colour polarity)
 *   0x5f8cb8  float[3] point A
 *   0x5f8cc4  float[3] point B / vector
 *   0x5f8cd0  int32    sphere count
 *   0x5f8cd4  float[3] sphere centre,   stride 0xc (16 entries)
 *   0x5f8d94  float[3] sphere endpoint, stride 0xc (= 0x5f8cd4 + 0xc0)
 *   0x5f8e54  float    sphere radius,   stride 4
 *   0x5f8e94  int32    polyline point count
 *   0x5f8e98  float[3] polyline points, stride 0xc (line i joins
 * pt[i]..pt[i+1])
 *
 * Call-site verification (all cdecl, caller-cleaned; first PUSH = last C arg):
 *   0x495d0 FUN_00189150.  PUSH EAX([0x2ee6e0]); PUSH 0x3dcccccd (0.1f as a
 *     raw dword, no FLD); PUSH 0x5f8cb8; PUSH 1 -> (1, 0x5f8cb8, 0.1f, colour)
 *     [match]
 *   0x495ed FUN_00189320.  PUSH ECX([0x2ee6e0]); PUSH 0x3f800000 (1.0f);
 *     PUSH 0x5f8cc4; PUSH 0x5f8cb8; PUSH 1 ->
 *     (1, 0x5f8cb8, 0x5f8cc4, 1.0f, colour)  [match]
 *     ADD ESP,0x24 at 0x495f7 is the *combined* deferred cleanup for both
 *     calls (4 + 5 = 9 dwords); it is not a nine-argument call.
 *   0x49635 FUN_00189860, ADD ESP,0x14.  PUSH EDX([0x2ee6d8]); PUSH
 *     EAX([EAX*4+0x5f8e54]); PUSH EDX(ECX+0x5f8d94); PUSH EAX(ECX+0x5f8cd4);
 *     PUSH 1 -> (1, centre+i*12, endpoint+i*12, radius[i], colour)  [match]
 *   0x4969c FUN_00189270, ADD ESP,0x10.  PUSH ECX(colour); PUSH
 *     ECX(EAX+0x5f8ea4); PUSH EDX(EAX+0x5f8e98); PUSH 1 ->
 *     (1, pt+i*12, pt+i*12+0xc, colour)  [match]
 *   The two point bases in each of the last two calls are separate LEAs
 *   against separate imm32 bases, so they stay separate expressions here
 *   rather than being folded into one base plus +0xc0 / +0xc.
 *
 * Confirmed: the radius push at 0x49630 is a plain dword MOV of a float slot.
 * Ghidra prints a `(float)` cast on an int array there, which would require an
 * FILD; there is no FPU instruction anywhere in this function.
 *
 * Confirmed: the element stride is 12 bytes, not the 3 that Ghidra's
 * undefined-byte pointer maths implies.  The address computation is
 * LEA ECX,[EAX+EAX*2]; SHL ECX,2 recomputed from the loop index each
 * iteration, so the lift indexes `(float *)base + i * 3` instead of walking a
 * pointer.
 *
 * Confirmed: both counts are re-read from memory inside the loop (MOV
 * ECX,[0x5f8cd0] / MOV ECX,[0x5f8e94] in the loop tail), and the polyline's
 * `count - 2` comparison is recomputed from a fresh load at 0x4964b each
 * iteration, so neither is cached in a local.
 *
 * Confirmed: the loop counter lives in SI as an int16 and is widened with
 * MOVSX EAX,SI for every use, transcribed here as a short counter plus an int
 * copy.  The polyline guard is LEA EDX,[ECX-1]; TEST EDX,EDX; JLE — a plain
 * `count - 1 > 0` test, not Ghidra's `count != 1 && -1 < count - 1`.
 *
 * cdecl void(void); no stack frame, ESI is the only saved register. */
void FUN_000495b0(void)
{
  void *color;
  short counter;
  int i;
  int off;
  int32_t *new_var;

  new_var = (int32_t *)0x5f8e94;
  if (*(uint8_t *)0x5f8cb4 != 0) {
    FUN_00189150(1, (float *)0x5f8cb8, 0.1f, *(void **)0x2ee6e0);
    FUN_00189320(1, (float *)0x5f8cb8, (float *)0x5f8cc4, 1.0f,
                 *(void **)0x2ee6e0);

    counter = 0;
    if (0 < *(int32_t *)0x5f8cd0) {
      i = 0;
      do {
        off = i * 12;
        FUN_00189860(1, (char *)0x5f8cd4 + off, (char *)0x5f8d94 + off,
                     ((float *)0x5f8e54)[i], *(void **)0x2ee6d8);
        counter++;
        i = counter;
      } while (i < *(int32_t *)0x5f8cd0);
    }

    counter = 0;
    if (0 < ((*new_var) - 1)) {
      i = 0;
      do {
        if (*(uint8_t *)0x5f8cb5 != 0) {
          color = *(void **)0x2ee6d4;
        } else {
          color = *(void **)0x2ee6f0;
          if (i != ((*new_var) - 2)) {
            color = *(void **)0x2ee6d0;
          }
        }
        off = i * 12;
        FUN_00189270(1, (float *)((char *)0x5f8e98 + off),
                     (float *)((char *)0x5f8ea4 + off), color);
        counter++;
        i = counter;
        if (counter) {
        }
      } while (i < ((*new_var) - 1));
    }
  }
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

/* ai_debug_render_points_and_lines: flush the queued AI debug point and line
 * lists to the debug renderer.
 *
 * Each queued point (3-float / 12-byte record array at 0x5accb0, count at
 * 0x5accac) is drawn as its own list index, formatted with "%d" into the
 * shared scratch string buffer at 0x5ab100, at the point's position.  Each
 * queued line (6-byte record: int16 point_a, int16 point_b, int16 color;
 * count at 0x5eccb0, the first record's colour field at 0x5eccb8) is drawn
 * between two of those points.
 *
 * Both loops pick a colour from a 13-entry local table of *addresses* of
 * global colour pointers.  The double dereference is load-bearing: the disasm
 * does MOV ECX,[EBP+EAX*4-0x34] then MOV EDX,[ECX], so the argument is the
 * value stored AT 0x2ee6cc etc., not 0x2ee6cc itself.  MSVC emits the table
 * as 13 separate `MOV dword ptr [EBP-N],imm32` stores at 0x499a1..0x499f5,
 * so it must stay a local array and not be hoisted to file-scope data.
 *
 * No __FILE__ string.  No FPU instructions anywhere in the function (points
 * are passed by pointer only).  cdecl void(void); frame is SUB ESP,0x34 with
 * ESI and EDI saved.
 *
 * Colour-table store-offset table (0x499a1..0x499f5), indexed later as
 * [EBP+EAX*4-0x34] so colors[0] lives at EBP-0x34:
 *   [EBP-0x34] colors[ 0] <- 0x2ee6cc     [EBP-0x18] colors[ 7] <- 0x2ee6e8
 *   [EBP-0x30] colors[ 1] <- 0x2ee6d8     [EBP-0x14] colors[ 8] <- 0x2ee6e4
 *   [EBP-0x2c] colors[ 2] <- 0x2ee6ec     [EBP-0x10] colors[ 9] <- 0x2ee6d0
 *   [EBP-0x28] colors[ 3] <- 0x2ee6dc     [EBP-0x0c] colors[10] <- 0x2ee6f0
 *   [EBP-0x24] colors[ 4] <- 0x2ee6d4     [EBP-0x08] colors[11] <- 0x2ee6e0
 *   [EBP-0x20] colors[ 5] <- 0x2ee6f4     [EBP-0x04] colors[12] <- 0x2ee6c4
 *   [EBP-0x1c] colors[ 6] <- 0x2ee700
 *
 * The clamp is upper-bound only (CMP against 0xc / JLE).  MOVSX makes the
 * index signed, but the original emits no lower-bound guard so none is added
 * here.  Both loop counts are re-read from their globals in the loop tail
 * (MOV EAX,[0x5accac] at 0x49a51, MOV EAX,[0x5eccb0] at 0x49aaa) rather than
 * cached in a register, so the conditions re-read them too. */
void ai_debug_render_points_and_lines(void)
{
  void **colors[13];
  float *point;
  int16_t *line;
  int color_index;
  int i;

  colors[0] = (void **)0x2ee6cc;
  colors[1] = (void **)0x2ee6d8;
  colors[2] = (void **)0x2ee6ec;
  colors[3] = (void **)0x2ee6dc;
  colors[4] = (void **)0x2ee6d4;
  colors[5] = (void **)0x2ee6f4;
  colors[6] = (void **)0x2ee700;
  colors[7] = (void **)0x2ee6e8;
  colors[8] = (void **)0x2ee6e4;
  colors[9] = (void **)0x2ee6d0;
  colors[10] = (void **)0x2ee6f0;
  colors[11] = (void **)0x2ee6e0;
  colors[12] = (void **)0x2ee6c4;

  i = 0;
  if (0 < *(int32_t *)0x5accac) {
    point = (float *)0x5accb0;
    do {
      crt_sprintf((char *)0x5ab100, "%d", (int)((int16_t *)0x5dccb0)[i]);
      color_index = (int)((int16_t *)0x5dccb0)[i];
      if (0xc < color_index) {
        color_index = 0xc;
      }
      FUN_00189cb0(1, point, (void *)0x5ab100, (int)*colors[color_index]);
      i = i + 1;
      point = point + 3;
    } while (i < *(int32_t *)0x5accac);
  }

  i = 0;
  if (0 < *(int32_t *)0x5eccb0) {
    line = (int16_t *)0x5eccb8;
    do {
      color_index = (int)line[0];
      if (0xc < color_index) {
        color_index = 0xc;
      }
      FUN_00189270(1, (float *)0x5accb0 + line[-2] * 3,
                   (float *)0x5accb0 + line[-1] * 3, *colors[color_index]);
      i = i + 1;
      line = line + 3;
    } while (i < *(int32_t *)0x5eccb0);
  }
}

/* ai_debug_describe_actor: format a human-readable description of an actor
 * and/or an object into the caller's buffer, and return that buffer.
 *
 * The result is the concatenation of three independently built pieces:
 *   1. an "encounter/squad " (or "encounter/(platoon) squad ") prefix taken
 *      from the actor's encounter membership, or "encounterless " when the
 *      actor has no encounter;
 *   2. the actor's/object's unit definition tag name with its directory path
 *      stripped;
 *   3. a " (name)" suffix naming the 0x24-byte scenario block element selected
 *      by object field +0x6a.
 *
 * Confirmed: cdecl, five stack arguments, frame is PUSH EBP / MOV EBP,ESP /
 *   SUB ESP,0x200 / PUSH ESI / PUSH EDI.  There is no _chkstk call, so the two
 *   scratch buffers are declared normally.  EBX is pushed at 0x49b2f and
 *   popped at 0x49bbd, i.e. only inside the encounter branch, where it holds
 *   the squad element pointer across the platoon lookup.
 * Confirmed: the two scratch buffers are char[256] each -- local_104 at
 *   EBP-0x100 and local_204 at EBP-0x200, exactly filling the 0x200 frame.
 * Confirmed: object_handle is written back to its own stack slot ([EBP+0xC])
 *   at the top of the with_actor branch, from actor +0x18.  Every later read
 *   of the parameter therefore sees the actor's unit handle, so the parameter
 *   is reassigned in place rather than copied to a local.
 * Confirmed: the encounter index is the low 16 bits of actor +0x34 (AND with
 *   0xffff after the dword is tested against -1), while the squad and platoon
 *   indices at actor +0x3a / +0x3c are loaded with MOVSX from word operands,
 *   so they are signed int16 fields (lift-learnings §24).  Object +0x6a is
 *   likewise MOVSX from a word.
 * Confirmed: the tag block element sizes are 0xB0 (scenario +0x42c,
 *   encounters), 0xE8 (encounter +0x80, squads), 0xAC (encounter +0x8c,
 *   platoons) and 0x24 (scenario +0x204).
 * Confirmed: the names printed for those elements are the element pointers
 *   themselves -- the pushes at 0x49b9c / 0x49bb5 / 0x49c2d push the pointers
 *   tag_block_get_element returned, with no field offset added, so each
 *   element begins with its name string.
 * Confirmed: crt_sprintf argument order at 0x49b9c is PUSH EBX(squad),
 *   PUSH EDI(encounter), PUSH fmt, PUSH EDX(buffer) -- last push first, so it
 *   is (buffer, "%s/%s ", encounter, squad): encounter first, squad second.
 *   At 0x49bb5 the order is (buffer, "%s/(%s) %s ", encounter, platoon,
 *   squad).
 * Confirmed: the final formatter at 0x49c51 is the 6-push snprintf
 *   (buf, buf_size, "%s%s%s", encounter_text, type_name, variant_text), and
 *   the return value is MOV EAX,ESI at 0x49c5a where ESI was loaded from
 *   [EBP+0x14] -- the function returns its own buf argument.
 *
 * Inferred: object type mask 3 for object_get_and_verify_type, matching the
 *   two verify calls in FUN_00049c70 in this same TU.
 * Inferred: the unit definition tag name is read from tag +0x2c, the standard
 *   tag-header name pointer used by the other tag_name_strip_path callers.
 *
 * Uncertain: the meaning of the 0x24-byte scenario block at scenario +0x204
 *   selected by object +0x6a.  It is printed parenthesised after the unit
 *   name, so it is a per-object variant/palette name, but no assert string
 *   names it, so no name is applied.
 *
 * Call-site note: the ARG_COUNT advisories on this function are all merged or
 *   variadic cleanups -- ADD ESP,0x18 at the squad lookup covers two
 *   tag_block_get_element calls (3 dwords each), ADD ESP,0x14 after
 *   tag_name_strip_path covers the preceding tag_get pushes as well, and the
 *   crt_sprintf/snprintf cleanups vary with the variadic argument count.
 *
 * Called from FUN_0004b7a0 (0x4b7a0) with the 0x100-byte global scratch
 * buffer at 0x5ab100. */
char *ai_debug_describe_actor(int actor_handle, int object_handle,
                              char with_actor, char *buf, int buf_size)
{
  char encounter_text[256];
  char variant_text[256];
  const char *type_name;
  char *actor;
  void *scenario;
  void *encounter;
  void *squad;
  void *platoon;
  void *object;
  void *definition;
  void *variant;
  int encounter_index;

  csstrcpy(encounter_text, "");
  if (with_actor != 0 && actor_handle != -1) {
    actor = (char *)datum_get(actor_data, actor_handle);
    object_handle = *(int32_t *)(actor + 0x18);
    if (*(uint32_t *)(actor + 0x34) == 0xffffffff) {
      csstrcpy(encounter_text, "encounterless ");
    } else {
      encounter_index = (int)(*(uint32_t *)(actor + 0x34) & 0xffff);
      scenario = global_scenario_get();
      encounter = tag_block_get_element((void *)((char *)scenario + 0x42c),
                                        encounter_index, 0xb0);
      squad = tag_block_get_element((void *)((char *)encounter + 0x80),
                                    (int)*(int16_t *)(actor + 0x3a), 0xe8);
      if (*(int16_t *)(actor + 0x3c) == -1 ||
          (platoon = tag_block_get_element((void *)((char *)encounter + 0x8c),
                                           (int)*(int16_t *)(actor + 0x3c),
                                           0xac)) == NULL) {
        crt_sprintf(encounter_text, "%s/%s ", (char *)encounter, (char *)squad);
      } else {
        crt_sprintf(encounter_text, "%s/(%s) %s ", (char *)encounter,
                    (char *)platoon, (char *)squad);
      }
    }
  }
  type_name = "";
  csstrcpy(variant_text, "");
  if (object_handle != -1) {
    object = object_get_and_verify_type(object_handle, 3);
    definition = tag_get(0x756e6974 /* 'unit' */, *(int32_t *)object);
    type_name =
      tag_name_strip_path(*(const char **)((char *)definition + 0x2c));
    if (*(int16_t *)((char *)object + 0x6a) != -1) {
      scenario = global_scenario_get();
      variant =
        tag_block_get_element((void *)((char *)scenario + 0x204),
                              (int)*(int16_t *)((char *)object + 0x6a), 0x24);
      crt_sprintf(variant_text, " (%s)", (char *)variant);
    }
  }
  snprintf(buf, buf_size, "%s%s%s", encounter_text, type_name, variant_text);
  return buf;
}

/* FUN_00049c70: resolve the object the debug camera is currently looking at,
 * returning a datum handle (or -1 when nothing usable is under the crosshair).
 *
 * Fires a 50.0-world-unit ray from the observer camera along the camera's own
 * forward vector (camera+0x20..+0x28), ignoring the local player's own unit,
 * then walks the hit object's two handle fields (+0x1a8 preferred, +0x1a4
 * fallback) and, if both are empty, repeats the walk one level down through
 * the object's +0x2d4 handle.
 *
 * Confirmed (XBE 0x49c70-0x49d5f, 80 instructions): frame is PUSH EBP /
 *   MOV EBP,ESP / SUB ESP,0x5c / PUSH EBX / PUSH ESI.  EDI is pushed at
 *   0x49c8f and popped at 0x49cf8 -- it is live only inside the
 *   camera != NULL branch.  Locals: the 0x50-byte collision result at
 *   EBP-0x5c and the float[3] ray delta at EBP-0xc..EBP-0x4.
 * Confirmed: this function RETURNS a value.  EBX is seeded -1 by
 *   OR EBX,0xffffffff at 0x49c7a, is the destination of every success path,
 *   and the epilogue is POP ESI / MOV EAX,EBX / POP EBX / MOV ESP,EBP /
 *   POP EBP / RET.  The kb declaration previously typed it void, which is
 *   wrong and would have dropped the whole 0x49d19-0x49d56 tail.
 * Confirmed: the perspective test is TEST AX,AX (16-bit) on
 *   director_get_perspective's return, so only perspective 0 (first person)
 *   resolves a player unit to exclude from the ray.
 * Confirmed: datum_get is called (player_data, index) -- PUSH EAX (the index
 *   from local_player_get_player_index) at 0x49cb0 then PUSH [0x5aa6d4].
 *   The player unit handle is read from player record +0x34.
 * Confirmed: the shared ADD ESP,0xc at 0x49cb8 covers datum_get's 2 dwords
 *   plus the PUSH 0 for local_player_get_player_index at 0x49ca2 -- MSVC
 *   coalesced the two cleanups.  The ARG_COUNT hazard on datum_get is a false
 *   positive; it really takes 2 arguments.
 * Confirmed: FUN_0014df70's pushes at 0x49cbb-0x49ceb are ECX(=EBP-0x5c),
 *   EDI(player unit), EDX(=EBP-0xc), ESI(camera), 0x81, so left-to-right the
 *   arguments are (0x81, camera, delta, player_unit, collision_result), and
 *   ADD ESP,0x14 confirms 5 stack args.  Result is tested TEST AL,AL.
 * Confirmed: the ray delta is three independent FLD [ESI+0x20/0x24/0x28] /
 *   FMUL [0x25acf0] / FSTP [EBP-0xc/-0x8/-0x4] sequences.  Multiplication
 *   only -- no FSUB, so there is no operand-order hazard.  The multiplier at
 *   0x25acf0 reads 0x42480000 = 50.0f in the XBE.
 * Confirmed: the collision-result reads are buffer fields, not independent
 *   locals.  CMP word ptr [EBP-0x5c],3 is result+0x00 (the hit-type tag) and
 *   MOV EAX,[EBP-0x24] is result+0x38 (the hit object handle).  Ghidra sized
 *   the buffer as short[28] (0x38 bytes), which is exactly why it reported
 *   the +0x38 read as a separate "local_28".
 * Confirmed: the tail at 0x49d43-0x49d56 consumes the second
 *   object_get_and_verify_type result (MOV ECX,[EAX+0x1a8] ... MOV EBX,
 *   [EAX+0x1a4]).  Ghidra dropped this block and showed the call's result as
 *   discarded.
 *
 * Inferred: the 0x81 flag word selects the collision mask used for debug
 *   picking; the sibling debug raycast in ai_debug_update uses 0x21.
 * Inferred: object type mask 3 for both verify calls (biped|vehicle in the
 *   masks used elsewhere in this TU).
 *
 * Uncertain: the meaning of object fields +0x1a4, +0x1a8 and +0x2d4.  All
 *   three are compared against -1 and returned directly, so all three are
 *   datum handles, but no name is applied without further evidence.
 * Uncertain: whether the two later -1 comparisons were written as literals.
 *   The binary emits CMP EAX,EBX / CMP ECX,EBX there because EBX is provably
 *   -1 at both points; that is a register-reuse optimisation over the same
 *   source-level "== -1" test. */
int FUN_00049c70(void)
{
  char collision_result[0x50];
  float delta[3];
  float *camera;
  int player_unit;
  int handle;
  int result;

  result = -1;
  camera = (float *)observer_get_camera(0);
  if (camera == NULL) {
    return result;
  }

  player_unit = -1;
  if (director_get_perspective(0) == 0) {
    player_unit =
      *(int32_t *)((char *)datum_get(player_data,
                                     local_player_get_player_index(0)) +
                   0x34);
  }

  delta[0] = camera[8] * *(float *)0x25acf0;
  delta[1] = camera[9] * *(float *)0x25acf0;
  delta[2] = camera[10] * *(float *)0x25acf0;

  if (!FUN_0014df70(0x81, camera, delta, player_unit,
                    (int16_t *)collision_result)) {
    return result;
  }
  if (*(int16_t *)collision_result != 3) {
    return result;
  }
  handle = *(int32_t *)(collision_result + 0x38);
  if (handle == -1) {
    return result;
  }
  {
    char *object = (char *)object_try_and_get_and_verify_type(handle, 3);
    if (object == NULL) {
      return result;
    }

    result = *(int32_t *)(object + 0x1a8);
    if (result == -1) {
      result = *(int32_t *)(object + 0x1a4);
    }
    if (result != -1) {
      return result;
    }

    handle = *(int32_t *)(object + 0x2d4);
    if (handle == -1) {
      return result;
    }
    object = (char *)object_get_and_verify_type(handle, 3);
    result = *(int32_t *)(object + 0x1a8);
    if (result == -1) {
      result = *(int32_t *)(object + 0x1a4);
    }
  }
  return result;
}

/* ai_debug_vocalize (0x49f60): debug console command that makes the currently
 * selected debug actor speak a named vocalization.  Looks up the vocalization
 * name and the vocalization-type name, asks the actor's unit for a matching
 * communication, and if one is produced, fills a 0x30-byte AI communication
 * record and hands it to the unit's speech dispatcher.
 *
 * Confirmed: two stack parameters at [EBP+8] and [EBP+0xC] (Ghidra surfaced
 *   them as in_stack_00000004 / in_stack_00000008).  Both are passed straight
 *   through to the name->index lookups FUN_001a6cd0 / FUN_001a67e0, whose kb
 *   declarations take `const char *`.  The old kb declaration of `(void)` was
 *   wrong and would have produced a caller/callee stack mismatch.
 * Confirmed: frame is PUSH EBP / MOV EBP,ESP / SUB ESP,0x38.  EBX and EDI are
 *   only saved on the non-early-exit path (PUSH EBX/EDI at 0x49f77/0x49f78),
 *   which is why the body is written as nested ifs rather than early returns.
 * Confirmed: ESI is materialised once as -1 (OR ESI,0xffffffff at 0x49f6c) and
 *   serves all three sentinel compares (the debug actor handle, actor +0x18,
 *   and CMP AX,SI at 0x49fbb) as well as the initial -1 stored to [EBP-0x8].
 * Confirmed: the 0x30-byte communication record lives at EBP-0x38..EBP-0x09
 *   (csmemset(EBP-0x38, 0, 0x30) at 0x49fe5-0x49fed; PUSH 0x30 / PUSH 0 /
 *   PUSH LEA [EBP-0x38], cdecl so the first PUSH is the last argument).
 *   Ghidra's `local_2c[32]` at EBP-0x28 is NOT an independent local: it is
 *   record + 0x10, so ai_communication_packet_new receives an interior
 *   pointer into the same buffer.
 * Confirmed: the field stores are, in instruction order,
 *   0x49ffd  MOV [EBP-0x38],BX   record+0x00 <- vocalization index
 *   0x4a001  MOV [EBP-0x36],CX   record+0x02 <- vocalization type
 *   0x4a005  MOV [EBP-0x34],EDX  record+0x04 <- sound definition index
 * Confirmed: datum_get is (actor_data, handle) -- PUSH EAX (the handle read
 *   from 0x5ac9f8) then PUSH [0x6325a4], ADD ESP,8 at 0x49f89.
 * Confirmed: FUN_001a68d0's pushes at 0x49fd6 are EAX(=&[EBP-0x8]),
 *   ECX(=&[EBP-0x4]), 0, 1, 1, EBX, EDX, so left-to-right the arguments are
 *   (unit handle, vocalization index, 1, 1, NULL, &type, &sound index) and
 *   ADD ESP,0x1c confirms 7 stack dwords.
 * Confirmed: the single ADD ESP,0x8 at 0x49fb0 cleans both name-lookup pushes
 *   and the single ADD ESP,0x1c at 0x4a01b cleans FUN_001a6ef0's three pushes
 *   plus csmemset's three and ai_communication_packet_new's one -- MSVC
 *   coalesced the cleanups, so the ARG_COUNT hazards on FUN_001a67e0 and
 *   FUN_001a6ef0 are false positives.
 * Confirmed: the width of every compare is 16-bit -- TEST BX,BX / JLE (signed
 *   `> 0`), CMP AX,SI (`== -1`), TEST SI,SI -- so the three intermediates are
 *   `short`, not `int`.
 * Inferred: 0x5aca89 is a "debug speech requested" byte flag; it is set to 1
 *   unconditionally once the debug actor handle resolves, before the unit
 *   handle is even validated.
 * Uncertain: the two `1` arguments to FUN_001a68d0 are byte-width literals
 *   (PUSH 1 twice) whose meaning is not recoverable from this call site. */
void ai_debug_vocalize(const char *vocalization_name,
                       const char *vocalization_type_name)
{
  char communication[0x30];
  int sound_definition_index;
  short vocalization_type;
  void *actor;
  short vocalization_index;
  short communication_count;

  if (*(int32_t *)0x5ac9f8 != -1) {
    actor = datum_get(*(data_t **)0x6325a4, *(int32_t *)0x5ac9f8);
    *(uint8_t *)0x5aca89 = 1;
    if (*(int32_t *)((char *)actor + 0x18) != -1) {
      vocalization_index = FUN_001a6cd0(vocalization_name);
      vocalization_type = FUN_001a67e0(vocalization_type_name);
      if (vocalization_index > 0 && vocalization_type != -1) {
        sound_definition_index = -1;
        communication_count =
          FUN_001a68d0(*(int32_t *)((char *)actor + 0x18), vocalization_index,
                       1, 1, NULL, &vocalization_type, &sound_definition_index);
        if (communication_count != 0) {
          csmemset(communication, 0, 0x30);
          *(short *)(communication + 0x00) = vocalization_index;
          *(short *)(communication + 0x02) = vocalization_type;
          *(int32_t *)(communication + 0x04) = sound_definition_index;
          ai_communication_packet_new(communication + 0x10);
          FUN_001a6ef0(*(int32_t *)((char *)actor + 0x18), communication_count,
                       communication);
        }
      }
    }
  }
}

/* FUN_0004a030 (0x4a030): per-tick service routine for the debug "speak"
 * request block that ai_debug_speak (0x4a220) and ai_debug_speak_list
 * (0x4a290) arm.  While the block is active it waits for the unit to stop
 * talking, waits out a countdown, asks the unit for the communication that
 * matches the current vocalization index, dispatches it, prints
 * "<vocalization name>: <sound tag name>" to the console, and then either
 * advances to the next non-"unused" vocalization or shuts the block down.
 *
 * Confirmed ABI: __cdecl, no parameters, no return value.  Frame is PUSH EBP /
 *   MOV EBP,ESP / SUB ESP,0x38 (56 bytes of locals) with a single PUSH ESI
 *   mid-function; ESI carries the communication count and later the printed
 *   tag-name pointer.  No FPU instructions, no SEH, no register arguments.
 * Confirmed: the 0x30-byte AI communication record occupies EBP-0x38..EBP-0x09
 *   and is zeroed by csmemset(LEA [EBP-0x38], 0, 0x30) at 0x4a0ef.  Ghidra's
 *   `local_2c[32]` at EBP-0x28 is NOT an independent local: EBP-0x28 is
 *   record + 0x10, so ai_communication_packet_new receives an interior pointer
 *   into the same buffer.  Declaring it as a second array would grow the frame
 *   past 0x38 and hand FUN_001a6ef0 the wrong bytes.  This is the same layout
 *   ai_debug_vocalize (0x49f60) uses.
 * Confirmed: [EBP-0x8] is a dword sound-definition index preset to -1 by
 *   MOV dword ptr [EBP-0x8],0xffffffff before the lookup, and [EBP-0x4] is a
 *   word vocalization type seeded from the 16-bit global at 0x6324ea.  Both
 *   are out-parameters of FUN_001a68d0 and are read back afterwards.
 * Confirmed global widths (all accesses are of the stated size; widening any
 *   of them changes the emitted load/store and the 0xd1 wraparound):
 *   0x6324e0  byte   speak-block active flag
 *   0x6324e1  byte   auto-advance flag
 *   0x6324e2  byte   skip-"unused" flag
 *   0x6324e4  dword  unit datum handle
 *   0x6324e8  word   countdown in ticks
 *   0x6324ea  word   current vocalization index, signed, bounded by 0xd1
 *   The same block is written by ai_debug_speak (0x4a220), which confirms the
 *   byte/word split independently.
 *
 * Call-site verification (cdecl: the first PUSH is the last C argument):
 *   arg# | binary source                    | C expression              | match
 *   object_try_and_get_and_verify_type (0x13d640) @ 0x4a054, ADD ESP,8
 *     1  | PUSH EAX (= [0x6324e4])          | *(int32_t *)0x6324e4      | yes
 *     2  | PUSH 3 (pushed first)            | 3                         | yes
 *   FUN_001a68d0 (0x1a68d0) @ 0x4a0d7, ADD ESP,0x1c (7 dwords)
 *     1  | PUSH EDX (= [0x6324e4])          | *(int32_t *)0x6324e4      | yes
 *     2  | PUSH 3                           | 3                         | yes
 *     3  | PUSH 0                           | 0                         | yes
 *     4  | PUSH 0                           | 0                         | yes
 *     5  | PUSH 0                           | NULL                      | yes
 *     6  | PUSH ECX (= LEA [EBP-0x4])       | &vocalization_type        | yes
 *     7  | PUSH EAX (= LEA [EBP-0x8]), 1st  | &sound_definition_index   | yes
 *   csmemset (0x8db80) @ 0x4a0ef
 *     1  | PUSH EAX (= LEA [EBP-0x38])      | communication             | yes
 *     2  | PUSH 0                           | 0                         | yes
 *     3  | PUSH 0x30 (pushed first)         | 0x30                      | yes
 *   ai_communication_packet_new (0x42d20) @ 0x4a112
 *     1  | PUSH EAX (= LEA [EBP-0x28])      | communication + 0x10      | yes
 *   FUN_001a6ef0 (0x1a6ef0) @ 0x4a123
 *     1  | PUSH EDX (= [0x6324e4])          | *(int32_t *)0x6324e4      | yes
 *     2  | PUSH ESI (communication count)   | communication_count       | yes
 *     3  | PUSH ECX (= LEA [EBP-0x38]), 1st | communication             | yes
 *   crt_strstr (0x1d9690) @ 0x4a141
 *     1  | PUSH ESI (tag_get_name result)   | name                      | yes
 *     2  | PUSH 0x25ad10 (pushed first)     | "conditional"             | yes
 *   crt_strchr (0x1d95d0) @ 0x4a150
 *     1  | PUSH EAX (strstr result)         | p                         | yes
 *     2  | PUSH 0x5c (pushed first)         | '\\'                      | yes
 *   FUN_001a67b0 (0x1a67b0) @ 0x4a174 and @ 0x4a1b7, ADD ESP,8
 *     1  | PUSH EAX (= word [0x6324ea])     | *(int16_t *)0x6324ea      | yes
 *     2  | PUSH 0 (pushed first)            | 0                         | yes
 *   console_printf (0xff4d0) @ 0x4a184
 *     1  | PUSH 0 (pushed last)             | 0                         | yes
 *     2  | PUSH 0x259f2c                    | "%s: %s"                  | yes
 *     3  | PUSH EAX (FUN_001a67b0 result)   | FUN_001a67b0(index, 0)    | yes
 *     4  | PUSH ESI (pushed first)          | name                      | yes
 *   csstrcmp (0x8dcb0) @ 0x4a1c0
 *     1  | PUSH EAX (FUN_001a67b0 result)   | FUN_001a67b0(index, 0)    | yes
 *     2  | PUSH 0x25ad00 (pushed first)     | "unused"                  | yes
 *   The ADD ESP,0x1c at 0x4a12b is a single coalesced cleanup covering
 *   csmemset's three pushes, ai_communication_packet_new's one and
 *   FUN_001a6ef0's three, so the enrichment ARG_COUNT warnings on
 *   FUN_001a6ef0 ("cleanup=7, decl=3") and crt_strstr ("cleanup=3, decl=2")
 *   are false positives; the raw push counts above are 3 and 2.
 *
 * Store-offset table for the communication record (derived from the raw MOV
 * instructions, not from the decompiler's field labels; base = EBP-0x38):
 *   offset | width | source                       | notes
 *   +0x00  | word  | 4                            | literal, not an index
 *   +0x02  | word  | [EBP-0x4]                    | vocalization type readback
 *   +0x04  | dword | [EBP-0x8]                    | sound definition index
 *   +0x0c  | word  | 0xf                          | literal
 *   +0x10  | -     | passed out                   | packet sub-record
 *   Offsets +0x06..+0x0b and +0x0e..+0x0f stay zero from the csmemset; the
 *   record layout is not otherwise known, so it is kept as a raw byte buffer
 *   exactly as in ai_debug_vocalize rather than invented as a struct.
 *
 * Confirmed block layout: both `"<none>"` stores are tail-merged into one
 *   block at LAB_0004a163, reached by `jl` from the count test and by `je`
 *   from the sound-index test, and the tag-name path ends with an explicit
 *   `jmp LAB_0004a168` over it.  That places the "<none>" store AFTER the
 *   tag-name path, so both tests must be written with the positive (tag-name)
 *   arm first and the failure arm as `else`; writing them the other way round
 *   emits the store first and inverts both branch mnemonics.
 * Confirmed control flow: `INC EAX / JZ` at 0x4a15c is a real test of
 *   strchr_result + 1 against NULL and is preserved verbatim; folding it away
 *   would drop two instructions.  The advance loop at 0x4a1a7 is a do/while
 *   whose bottom test re-reads the 16-bit global.
 *
 * Inferred: FUN_001a68d0's second argument 3 is a priority/importance selector
 *   (ai_debug_vocalize passes the looked-up vocalization index there instead),
 *   and a returned count below 2 means "nothing to say", which is why the
 *   printed tag name degenerates to "<none>".
 * Uncertain: the meaning of the literals 4 (record+0x00) and 0xf (record+0x0c)
 *   is not recoverable from this call site; 0xf is also the countdown reload
 *   value written to 0x6324e8, but the two uses may be unrelated.
 * Uncertain: the "conditional" / backslash trimming shortens a tag path such
 *   as "...conditional\\<leaf>" to its leaf name; the exact tag-name shape it
 *   targets is not observable here. */
void FUN_0004a030(void)
{
  char communication[0x30];
  int sound_definition_index;
  int16_t vocalization_type;
  void *unit;
  int16_t communication_count;
  const char *name;
  char *p;

  if (*(uint8_t *)0x6324e0 == 0) {
    return;
  }
  if (*(int32_t *)0x6324e4 == -1) {
    return;
  }

  unit = object_try_and_get_and_verify_type(*(int32_t *)0x6324e4, 3);
  if (unit == NULL) {
    *(uint8_t *)0x6324e0 = 0;
    return;
  }
  if ((*(uint8_t *)((char *)unit + 0xb6) & 4) != 0) {
    *(uint8_t *)0x6324e0 = 0;
    return;
  }
  if (*(int16_t *)((char *)unit + 0x338) != 0) {
    return;
  }

  if (*(int16_t *)0x6324e8 > 0) {
    *(int16_t *)0x6324e8 = (int16_t)(*(int16_t *)0x6324e8 - 1);
  }
  if (*(int16_t *)0x6324e8 != 0) {
    return;
  }

  if (*(int16_t *)0x6324ea >= 0 && *(int16_t *)0x6324ea < 0xd1) {
    vocalization_type = *(int16_t *)0x6324ea;
    sound_definition_index = -1;
    communication_count =
      FUN_001a68d0(*(int32_t *)0x6324e4, 3, 0, 0, NULL, &vocalization_type,
                   &sound_definition_index);
    if (communication_count >= 2) {
      csmemset(communication, 0, 0x30);
      *(int16_t *)(communication + 0x02) = vocalization_type;
      *(int32_t *)(communication + 0x04) = sound_definition_index;
      *(int16_t *)(communication + 0x00) = 4;
      *(int16_t *)(communication + 0x0c) = 0xf;
      ai_communication_packet_new(communication + 0x10);
      FUN_001a6ef0(*(int32_t *)0x6324e4, communication_count, communication);
      if (sound_definition_index != -1) {
        name = tag_get_name(sound_definition_index);
        p = crt_strstr(name, "conditional");
        if (p != NULL) {
          p = crt_strchr(p, '\\');
          if (p != NULL) {
            p = p + 1;
            if (p != NULL) {
              name = p;
            }
          }
        }
      } else {
        name = "<none>";
      }
    } else {
      name = "<none>";
    }
    console_printf(0, "%s: %s", FUN_001a67b0(*(int16_t *)0x6324ea, 0), name);
    if (*(uint8_t *)0x6324e1 == 0) {
      *(int16_t *)0x6324ea = -1;
    } else {
      *(int16_t *)0x6324e8 = 0xf;
      do {
        *(int16_t *)0x6324ea = (int16_t)(*(int16_t *)0x6324ea + 1);
        if (csstrcmp(FUN_001a67b0(*(int16_t *)0x6324ea, 0), "unused") != 0) {
          break;
        }
        if (*(uint8_t *)0x6324e2 == 0) {
          *(int16_t *)0x6324ea = -1;
          break;
        }
      } while (*(int16_t *)0x6324ea < 0xd1);
    }
  }

  if (*(int16_t *)0x6324ea >= 0 && *(int16_t *)0x6324ea < 0xd1) {
    return;
  }
  console_printf(0, "speech done");
  *(uint8_t *)0x6324e0 = 0;
}

/* ai_debug_speak (0x4a220): arm the debug "speak" request block for the
 * currently selected debug actor, using a named vocalization type.
 *
 * Confirmed ABI: __cdecl, ONE dword stack argument.  `MOV ECX,dword ptr
 *   [EBP+0x8]` at 0x4a23a reads the parameter and pushes it as the single
 *   argument of FUN_001a67e0(const char *), so the parameter is a name
 *   string.  The terminator is a plain RET (no RET n), and the kb.json
 *   declaration previously read "void ai_debug_speak(void);", which is wrong;
 *   it is corrected as part of this lift.
 * Confirmed frame: PUSH EBP / MOV EBP,ESP with no SUB ESP and no locals.  ESI
 *   is pushed only inside the `!= -1` branch (0x4a22d) and popped at 0x4a282;
 *   it carries the datum_get result across the second call.
 *
 * Call-site verification (cdecl: the first PUSH is the last C argument):
 *   arg# | binary source                     | C expression         | match
 *   datum_get (0x119320)
 *     1  | PUSH EAX (= [0x6325a4])           | *(data_t **)0x6325a4 | yes
 *     2  | PUSH EAX (= [0x5ac9f8]), pushed 1st| *(int32_t *)0x5ac9f8| yes
 *   FUN_001a67e0 (0x1a67e0)
 *     1  | PUSH ECX (= [EBP+0x8])            | name                 | yes
 *   The single ADD ESP,0xc at 0x4a248 cleans BOTH calls (8 + 4); MSVC
 *   coalesced the cleanups, so the ARG_COUNT hazard on FUN_001a67e0
 *   ("cleanup=3 stack args, decl=1") is a false positive.
 *
 * Confirmed store widths and order (LOADW-sensitive; derived from the raw
 * MOV instructions, not from the decompiler's field labels):
 *   offset     | width | source                   | notes
 *   0x5aca89   | byte  | 1                        | shared with MOV CL,1
 *   0x6324e0   | byte  | 1                        | same CL
 *   0x6324e8   | word  | 0                        | MOV word ptr [...],CX
 *   0x6324e1   | byte  | 0                        | same XOR-zeroed ECX
 *   0x6324e4   | dword | actor[+0x18]             | unit handle
 *   0x6324ea   | word  | FUN_001a67e0 result      | vocalization type index
 * Declaring 0x6324e8 / 0x6324ea as 32-bit or 0x5aca89 / 0x6324e0 / 0x6324e1
 * as anything wider than a byte changes the emitted store size.
 *
 * Confirmed compare widths: CMP ECX,-1 on the 32-bit actor field at +0x18 and
 *   CMP AX,0xffff on the 16-bit lookup result, so the intermediate is a
 *   `short`, not an `int`.  Both tests happen AFTER both calls, so the C `&&`
 *   short-circuit costs nothing and matches the emitted order.
 *
 * Inferred: 0x5aca89 is the same "debug speech requested" byte flag that
 *   ai_debug_vocalize (0x49f60) sets; 0x6324e0..0x6324ea is a small pending-
 *   speech record (byte flag, byte flag, dword unit handle, word, word).
 * Uncertain: the individual field meanings inside 0x6324e0..0x6324ea are not
 *   recoverable from this call site alone, so they are left as raw addresses.
 *
 * No FPU instructions.  Only two CALLs, both cdecl and both already in
 * kb.json; no register arguments are involved. */
void ai_debug_speak(const char *name)
{
  void *actor;
  int16_t vocalization_type;

  if (*(int32_t *)0x5ac9f8 != -1) {
    actor = datum_get(*(data_t **)0x6325a4, *(int32_t *)0x5ac9f8);
    vocalization_type = FUN_001a67e0(name);
    if (*(int32_t *)((char *)actor + 0x18) != -1 && vocalization_type != -1) {
      *(uint8_t *)0x5aca89 = 1;
      *(uint8_t *)0x6324e0 = 1;
      *(int16_t *)0x6324e8 = 0;
      *(uint8_t *)0x6324e1 = 0;
      *(int32_t *)0x6324e4 = *(int32_t *)((char *)actor + 0x18);
      *(int16_t *)0x6324ea = vocalization_type;
    }
  }
}

/* ai_debug_speak_list (0x4a290): arm the debug "speak" request block for the
 * currently selected debug actor, selecting the vocalization by naming one of
 * the engine's fixed speech-list categories instead of a single vocalization.
 *
 * Confirmed ABI: __cdecl, ONE dword stack argument.  `MOV ECX,dword ptr
 *   [EBP+0x8]` at 0x4a3b0 and `MOV EDX,dword ptr [EBP+0x8]` at 0x4a3d1 read
 *   the parameter; it is passed to crt_stricmp and printed with a "%s"
 *   conversion, so it is a name string.  The terminator is a plain RET.  The
 *   kb.json declaration previously read "void ai_debug_speak_list(void);",
 *   which is wrong; it is corrected as part of this lift.  Ghidra surfaces
 *   the parameter as `in_stack_00000004` because of that bad declaration.
 *
 * Confirmed frame: PUSH EBP / MOV EBP,ESP / SUB ESP,0x78, then the early-out
 *   `CMP EAX,-1 / JE 0x4a457` runs BEFORE `PUSH EBX / PUSH ESI / PUSH EDI`
 *   (0x4a2a4..0x4a2a8), so the not-selected path skips the callee-saved
 *   pushes entirely and its epilogue at 0x4a457 is just MOV ESP,EBP / POP EBP
 *   / RET.  The early return therefore has to be the first statement, ahead of
 *   any local initialization.  0x78 = 15 * 8, exactly the table below.
 *
 * Confirmed table layout (derived from the raw MOV instructions at
 * 0x4a2b0..0x4a396, not from the decompiler's field labels).  Fifteen 8-byte
 * entries at [EBP-0x78] stepping +8; each entry stores a dword name pointer at
 * +0, a word index at +4 and a byte flag at +6.  The pad byte at +7 is never
 * written, which is what a member-wise brace initializer of an automatic
 * aggregate emits (a .rdata template copy would have written it), so the
 * original declared this as a local array, not a static one.
 *   ebp-  | name literal            | index | flag
 *   0x78  | 0x25ae38 "all"          | 0     | 1
 *   0x70  | 0x25ae30 "idle"         | 0     | 0
 *   0x68  | 0x25ae24 "involuntary"  | 6     | 0
 *   0x60  | 0x25ae14 "hurting people"        | 0x15 | 0
 *   0x58  | 0x25ae08 "being hurt"            | 0x1d | 0
 *   0x50  | 0x25adf8 "killing people"        | 0x31 | 0
 *   0x48  | 0x25ade0 "player kill comments"  | 0x50 | 0
 *   0x40  | 0x25add0 "friends dying"         | 0x60 | 0
 *   0x38  | 0x25adc4 "shouting"              | 0x6c | 0
 *   0x30  | 0x25adb0 "group communication"   | 0x7b | 0
 *   0x28  | 0x25ada8 "actions"               | 0x94 | 0
 *   0x20  | 0x25ad98 "exclamations"          | 0xb1 | 0
 *   0x18  | 0x25ad84 "post-combat actions"   | 0xbc | 0
 *   0x10  | 0x25ad70 "post-combat chatter"   | 0xc5 | 0
 *   0x08  | 0 (EBX)                          | 0xffff | 0   <- terminator
 * Every zero in the block is stored from EBX, which is XOR-zeroed once at
 * 0x4a2a5 and reused as the literal 0 and as the NULL compare operand for the
 * rest of the function.
 *
 * Loop shape: both loops are rotated `while (entry->name != NULL)` forms.  In
 * each, the first entry's name is materialized as the literal 0x25ae38 in the
 * preheader (`MOV EAX,0x25ae38 / JMP body` at 0x4a3a6 and 0x4a3e6) instead of
 * being reloaded from the frame - MSVC const-propagated table[0].name through
 * the brace initializer and proved the leading test redundant.  The search
 * loop's `break` on a stricmp match lands on a re-test of `entry->name`
 * (`CMP dword ptr [ESI],EBX` at 0x4a3cd), while the bottom-of-loop exit jumps
 * straight to the error path at 0x4a3d1; that is the classic lowering of a
 * `while` search followed by a post-loop `if (entry->name != NULL)`, not a
 * do/while.
 *
 * Call-site verification (cdecl: the first PUSH is the last C argument):
 *   arg# | binary source                       | C expression       | match
 *   datum_get (0x119320), CALL 0x4a399, ADD ESP,8 at 0x4a3a0
 *     1  | PUSH EAX (= [0x6325a4]) at 0x4a2af  | *(data_t **)0x6325a4 | yes
 *     2  | PUSH EAX (= [0x5ac9f8]) at 0x4a2a9  | *(int32_t *)0x5ac9f8 | yes
 *     Both pushes were hoisted above the table stores by the scheduler; the
 *     CALL itself follows the whole block.  Result lives in EDI afterwards.
 *   crt_stricmp (0x1dd801), CALL 0x4a3b5, ADD ESP,8
 *     1  | PUSH EAX (current entry name)       | entry->name        | yes
 *     2  | PUSH ECX (= [EBP+0x8]), pushed 1st  | list_name          | yes
 *   console_printf (0xff4d0), CALL 0x4a3db, ADD ESP,0xc
 *     1  | PUSH EBX (= 0), pushed last         | 0                  | yes
 *     2  | PUSH 0x25ad28                       | format literal     | yes
 *     3  | PUSH EDX (= [EBP+0x8]), pushed 1st  | list_name          | yes
 *   console_printf (0xff4d0), CALL 0x4a3f7, ADD ESP,0xc
 *     1  | PUSH EBX (= 0), pushed last         | 0                  | yes
 *     2  | PUSH 0x25ad1c                       | "    %s"           | yes
 *     3  | PUSH EAX (current entry name)       | entry->name        | yes
 * All three callees are cdecl and already in kb.json; none takes register
 * arguments, so no @<reg> annotations are involved.
 *
 * Confirmed store widths and order in the found path (LOADW-sensitive):
 *   address  | width | source                | instruction
 *   0x5aca89 | byte  | 1                     | 0x4a423
 *   0x6324e0 | byte  | 1                     | 0x4a42a
 *   0x6324e8 | word  | 0 (BX)                | 0x4a431
 *   0x6324e1 | byte  | 1                     | 0x4a438
 *   0x6324e2 | byte  | entry->flag (CL)      | 0x4a43f
 *   0x6324e4 | dword | actor[+0x18]          | 0x4a448
 *   0x6324ea | word  | entry->index (AX)     | 0x4a44e
 * This is the same pending-speech record ai_debug_speak (0x4a220) fills, with
 * two extra bytes set: 0x6324e1 is 1 here (0 there) and 0x6324e2 carries the
 * table's per-entry flag byte, which is 1 only for the "all" entry.
 *
 * Confirmed widths: `MOV AX,word ptr [ESI+4] / CMP AX,0xffff` makes the index
 *   a 16-bit field; `MOV CL,byte ptr [ESI+6]` makes the flag an 8-bit field.
 *   Declaring either as int would add movswl/movzbl.  The flag byte is loaded
 *   into a register before the store block starts, so it is kept in a local.
 *   `actor[+0x18]` is read twice (CMP at 0x4a410, MOV at 0x4a445) - the
 *   original does not cache it, so neither do we.
 *
 * Inferred: the index column is an index into the engine's vocalization table
 *   (the same space ai_debug_speak's FUN_001a67e0 lookup returns), and the
 *   values are the first vocalization of each named category.
 * Uncertain: the meaning of the individual bytes in 0x6324e0..0x6324e2 is not
 *   recoverable from these two call sites, so they are left as raw addresses;
 *   likewise the flag column's semantics beyond "set only for all".
 *
 * No FPU instructions, no SEH, no _chkstk (0x78 is under the probe
 * threshold). */
void ai_debug_speak_list(const char *list_name)
{
  struct ai_speak_list_entry {
    const char *name;
    int16_t index;
    uint8_t flag;
  };

  if (*(int32_t *)0x5ac9f8 == -1)
    return;

  {
    struct ai_speak_list_entry speak_lists[15] = {
      { "all", 0, 1 },
      { "idle", 0, 0 },
      { "involuntary", 6, 0 },
      { "hurting people", 0x15, 0 },
      { "being hurt", 0x1d, 0 },
      { "killing people", 0x31, 0 },
      { "player kill comments", 0x50, 0 },
      { "friends dying", 0x60, 0 },
      { "shouting", 0x6c, 0 },
      { "group communication", 0x7b, 0 },
      { "actions", 0x94, 0 },
      { "exclamations", 0xb1, 0 },
      { "post-combat actions", 0xbc, 0 },
      { "post-combat chatter", 0xc5, 0 },
      { NULL, -1, 0 }
    };
    struct ai_speak_list_entry *entry;
    void *actor;
    int16_t index;
    uint8_t flag;

    actor = datum_get(*(data_t **)0x6325a4, *(int32_t *)0x5ac9f8);

    entry = speak_lists;
    while (entry->name != NULL) {
      if (crt_stricmp(entry->name, list_name) == 0)
        break;
      entry++;
    }

    if (entry->name != NULL) {
      if (*(int32_t *)((char *)actor + 0x18) != -1) {
        index = entry->index;
        if (index != -1) {
          flag = entry->flag;
          *(uint8_t *)0x5aca89 = 1;
          *(uint8_t *)0x6324e0 = 1;
          *(int16_t *)0x6324e8 = 0;
          *(uint8_t *)0x6324e1 = 1;
          *(uint8_t *)0x6324e2 = flag;
          *(int32_t *)0x6324e4 = *(int32_t *)((char *)actor + 0x18);
          *(int16_t *)0x6324ea = index;
        }
      }
    } else {
      console_printf(0,
                     "ai_speak_list: couldn't find the list '%s'... here are "
                     "the known lists:",
                     list_name);
      for (entry = speak_lists; entry->name != NULL; entry++)
        console_printf(0, "    %s", entry->name);
    }
  }
}

/* ai_debug_toggle_flags (0x4a460): parse a list of debug-flag names into a
 * temporary bit vector and toggle those bits in a caller-supplied vector.
 *
 * __FILE__ assert xref confirms the TU: c:\halo\SOURCE\ai\ai_debug.c, lines
 * 0x1353 ("lookup"), 0x1354 ("vector_size <= 2048") and 0x135d
 * ("(comm_type >= 0) && (comm_type < vector_size)").  The third assert names
 * the lookup result "comm_type", so the primary caller is an AI
 * communication-type console command; the routine itself is generic over the
 * name->index lookup function passed in [EBP+0x18].
 *
 * Semantics: build a mask from the supplied names (the literal name "all"
 * sets every bit).  Then count, across the whole vector, how many mask bits
 * are NOT yet set in the destination ("set" count) and how many already are
 * ("cleared" count).  If anything is missing, OR the mask in and report
 * "set N flags"; otherwise complement the mask and AND it in, reporting
 * "cleared N flags".  The command therefore behaves as a toggle: naming flags
 * turns them on unless they were all on already, in which case it turns them
 * off.
 *
 * Confirmed ABI: __cdecl, 5 dword stack args, no register args.
 *   [EBP+0x08] int       count        (JLE/JL - signed)
 *   [EBP+0x0C] char **   names
 *   [EBP+0x10] int       dest_vector  (bit vector base; re-loaded from the
 *                                       parameter slot at every use in the
 *                                       reference, so it is an int, not a
 *                                       cached typed pointer)
 *   [EBP+0x14] uint32_t  vector_size  (CMP 0x800/JBE, JC - unsigned)
 *   [EBP+0x18] int16_t (__cdecl *lookup)(const char *)
 *              CALL dword ptr [EBP+0x18] with one PUSH and ADD ESP,4 -> the
 *              callee is cdecl and its result is read 16-bit (CMP SI,-1).
 * The kb.json declaration previously read "void FUN_0004a460(void);", which
 * is wrong for a 5-argument cdecl; it is corrected as part of this lift.
 *
 * Frame: SUB ESP,0x108 = a 0x100-byte mask buffer at [EBP-0x108] (64 dwords =
 * 2048 bits, exactly the "vector_size <= 2048" bound) plus two int counters
 * at [EBP-0x8] ("set") and [EBP-0x4] ("cleared").
 *
 * Call-site verification (cdecl: the first PUSH is the last C argument):
 *   arg# | binary source                | C expression        | match
 *   display_assert x3 -- PUSH 1 / PUSH line / PUSH 0x25ab74 / PUSH reason
 *     1  | last PUSH (reason string)    | reason literal      | yes
 *     2  | PUSH 0x25ab74                | __FILE__ literal    | yes
 *     3  | PUSH imm line                | 0x1353/0x1354/0x135d| yes
 *     4  | PUSH 1                       | 1                   | yes
 *     no ADD ESP follows: PUSH -1 / CALL 0x8e2f0 (system_exit, noreturn).
 *   csmemset -- PUSH EBX / PUSH EDI or -1 / PUSH buf; ADD ESP,0xc
 *     1  | last PUSH = LEA [EBP-0x108]  | mask                | yes
 *     2  | PUSH 0 or PUSH -1            | 0 / -1              | yes
 *     3  | PUSH EBX (rounded byte len)  | mask_bytes          | yes
 *   csstrcmp -- PUSH 0x25ae38 / PUSH names[i]; ADD ESP,8
 *     1  | last PUSH = names[i]         | names[i]            | yes
 *     2  | PUSH 0x25ae38                | "all"               | yes
 *   bit_vector_or (0x108f00) -- PUSH EAX / PUSH EAX / PUSH buf / PUSH EDI
 *     1  | last PUSH = EDI (size16)     | size16              | yes
 *     2  | PUSH LEA [EBP-0x108]         | (int)mask           | yes
 *     3  | PUSH EAX (dest_vector)       | dest_vector         | yes
 *     4  | PUSH EAX (dest_vector)       | dest_vector         | yes
 *     dest_vector is pushed twice because the OR is in-place (v1 ==
 *     result_out).  This is legitimate, not a duplicate-argument bug.
 *   FUN_00108fa0 -- PUSH buf / PUSH buf / PUSH EDI  (in-place complement)
 *     1  | last PUSH = EDI (size16)     | size16              | yes
 *     2  | PUSH LEA [EBP-0x108]         | (int)mask           | yes
 *     3  | PUSH LEA [EBP-0x108]         | (int)mask           | yes
 *   bit_vector_and (0x108e70) -- same shape as bit_vector_or
 *     1..4 | EDI, buf, EAX, EAX         | size16, mask, dest, dest | yes
 *   console_printf -- PUSH MOVSX count / PUSH fmt / PUSH 0
 *     1  | last PUSH = 0                | 0                   | yes
 *     2  | PUSH 0x25ae50 / 0x25ae3c     | format literal      | yes
 *     3  | PUSH MOVSX of the 16-bit cnt | (int)count16        | yes
 * All eight direct callees are cdecl and already in kb.json; the ninth call
 * is the indirect lookup through the parameter.  No callee takes register
 * arguments, so no @<reg> annotations are involved.
 *
 * Width notes (load-bearing): the lookup result is compared as 16 bits
 * (CMP SI,-1) and both counters are truncated to 16 bits (TEST SI,SI on
 * (short)counter) before their zero tests, then MOVSX-extended for
 * console_printf.  Dropping either narrowing changes behaviour for counts
 * that are exact multiples of 0x10000.  The byte length handed to csmemset is
 * ((vector_size + 0x1f) >> 5) << 2 - round up to whole 32-bit words, then
 * scale to bytes - not vector_size / 8.  Word indexing uses a signed shift
 * (SAR) on both the mask and the destination.
 *
 * No FPU instructions, no struct field access. */
void ai_debug_toggle_flags(int count, char **names, int dest_vector,
                           uint32_t vector_size,
                           int16_t(__cdecl *lookup)(const char *))
{
  int cleared_count;
  int set_count;
  uint32_t mask[64];
  uint32_t mask_bytes;
  uint32_t bit_index;
  uint32_t bit;
  int i;
  int16_t comm_type;
  int16_t size16;
  int16_t count16;

  cleared_count = 0;
  set_count = 0;

  if (lookup == 0) {
    display_assert("lookup", "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0x1353, 1);
    system_exit(-1);
  }
  if (vector_size > 0x800) {
    display_assert("vector_size <= 2048", "c:\\halo\\SOURCE\\ai\\ai_debug.c",
                   0x1354, 1);
    system_exit(-1);
  }

  mask_bytes = ((vector_size + 0x1f) >> 5) << 2;
  csmemset(mask, 0, mask_bytes);

  for (i = 0; i < count; i++) {
    comm_type = (*lookup)(names[i]);
    /* Dispatch shape recovered from the reference lowering: the -1 arm is
     * entered by a taken JE and the normal arm by an unconditional JMP, so
     * NEITHER arm is a fall-through.  A plain if/else always leaves one arm
     * as the fall-through, so the source was a switch. */
    switch (comm_type) {
    case -1:
      /* Unrecognised name: only the literal "all" is accepted, and it sets
       * every bit in the mask. */
      if (csstrcmp(names[i], "all") == 0) {
        csmemset(mask, -1, mask_bytes);
      }
      break;
    default:
      if ((comm_type < 0) || ((uint32_t)(int)comm_type >= vector_size)) {
        display_assert("(comm_type >= 0) && (comm_type < vector_size)",
                       "c:\\halo\\SOURCE\\ai\\ai_debug.c", 0x135d, 1);
        system_exit(-1);
      }
      mask[(int)comm_type >> 5] |= 1 << (comm_type & 0x1f);
      break;
    }
  }

  for (bit_index = 0; bit_index < vector_size; bit_index++) {
    bit = 1u << (bit_index & 0x1f);
    if ((mask[(int)bit_index >> 5] & bit) != 0) {
      if ((*(uint32_t *)((((int)bit_index >> 5) * 4) + dest_vector) & bit) ==
          0) {
        set_count++;
      } else {
        cleared_count++;
      }
    }
  }

  size16 = (int16_t)vector_size;

  count16 = (int16_t)set_count;
  if (count16 != 0) {
    bit_vector_or(size16, (int)mask, dest_vector, dest_vector);
    console_printf(0, "set %d flags", (int)count16);
    return;
  }

  count16 = (int16_t)cleared_count;
  if (count16 != 0) {
    FUN_00108fa0(size16, (int)mask, (int)mask);
    bit_vector_and(size16, (int)mask, dest_vector, dest_vector);
    console_printf(0, "cleared %d flags", (int)count16);
  }
}

/* ai_debug_communication_suppress (0x4a650): console/script command that
 * toggles bits in the AI communication-suppression bit vector at 0x5aca14.
 *
 * A pure forwarder to ai_debug_toggle_flags (0x4a460) with three constants
 * baked in: the destination bit vector, its size in bits, and the
 * name->type lookup used to resolve each argument string.
 *
 * Confirmed: cdecl.  PUSH EBP / MOV EBP,ESP, no SUB ESP, no locals, no FPU,
 * no struct access, a single CALL, then POP EBP / RET (no RET n, so cdecl
 * and not stdcall).  Thirteen instructions total.
 *
 * The two incoming arguments are loaded into registers BEFORE the pushes
 * (`MOV EAX,dword ptr [EBP+0xC]` and `MOV ECX,dword ptr [EBP+0x8]`), which
 * is what proves this takes two stack parameters; the previous kb.json
 * declaration of `void (void)` is why the decompiler emitted an
 * argument-less wrapper around a bare FUN_0004a460() call.
 *
 * Call-site table for ai_debug_toggle_flags (last push = first arg, cdecl):
 *   arg5 lookup      PUSH 0x42ce0  -> ai_communication_get_type_by_name
 *   arg4 vector_size PUSH 0x39     -> 57 communication types
 *   arg3 dest_vector PUSH 0x5aca14 -> suppression bit vector, passed as an
 *                                     integer address (the callee's
 *                                     dest_vector parameter is `int`)
 *   arg2 names       PUSH EAX      -> [EBP+0xC]
 *   arg1 count       PUSH ECX      -> [EBP+0x8]
 *   CALL 0x4a460 ; ADD ESP,0x14    (5 dwords, caller cleanup -> cdecl)
 *
 * vector_size 0x39 is far under the callee's "vector_size <= 2048" assert
 * (ai_debug.c line 0x1354), so that path can never fire from here.  The
 * callee allocates its own 64-dword scratch mask, so nothing is allocated
 * on this frame.  No __FILE__ string of its own.
 *
 * Reached from the hs script-command dispatch table at function_index 0x18
 * (see src/halo/hs/hs_library_internal_runtime.h:928), which supplies the
 * argument count and the argument string vector. */
void ai_debug_communication_suppress(int count, char **names)
{
  ai_debug_toggle_flags(count, names, 0x5aca14, 0x39,
                        ai_communication_get_type_by_name);
}

/* ai_debug_communication_ignore (0x4a680): console/script command that
 * toggles bits in the AI communication-ignore bit vector at 0x5aca1c.
 *
 * Structurally identical to ai_debug_communication_suppress (0x4a650) eight
 * bytes earlier; the ONLY difference is the destination bit vector constant
 * (0x5aca1c here vs 0x5aca14 there).  Both are pure forwarders to
 * ai_debug_toggle_flags (0x4a460) with three constants baked in: the
 * destination bit vector, its size in bits, and the name->type lookup used
 * to resolve each argument string.
 *
 * Confirmed: cdecl.  PUSH EBP / MOV EBP,ESP, no SUB ESP, no locals, no FPU,
 * no struct access, a single CALL, then POP EBP / RET (no RET n, so cdecl
 * and not stdcall).  Thirteen instructions total.
 *
 * The two incoming arguments are loaded into registers BEFORE the pushes
 * (`MOV EAX,dword ptr [EBP+0xC]` and `MOV ECX,dword ptr [EBP+0x8]`), which
 * is what proves this takes two stack parameters; the previous kb.json
 * declaration of `void (void)` is why the decompiler emitted an
 * argument-less wrapper around a bare FUN_0004a460() call.
 *
 * Call-site table for ai_debug_toggle_flags (last push = first arg, cdecl):
 *   arg5 lookup      PUSH 0x42ce0  -> ai_communication_get_type_by_name
 *   arg4 vector_size PUSH 0x39     -> 57 communication types
 *   arg3 dest_vector PUSH 0x5aca1c -> ignore bit vector, passed as an
 *                                     integer address (the callee's
 *                                     dest_vector parameter is `int`)
 *   arg2 names       PUSH EAX      -> [EBP+0xC]
 *   arg1 count       PUSH ECX      -> [EBP+0x8]
 *   CALL 0x4a460 ; ADD ESP,0x14    (5 dwords, caller cleanup -> cdecl)
 *
 * vector_size 0x39 is far under the callee's "vector_size <= 2048" assert,
 * so that path can never fire from here.  The callee allocates its own
 * 64-dword scratch mask, so nothing is allocated on this frame.  No
 * __FILE__ string of its own.
 *
 * Reached from the hs script-command dispatch table (see
 * src/halo/hs/hs_library_internal_runtime.h:929), which supplies the
 * argument count and the argument string vector. */
void ai_debug_communication_ignore(int count, char **names)
{
  ai_debug_toggle_flags(count, names, 0x5aca1c, 0x39,
                        ai_communication_get_type_by_name);
}

/* ai_debug_communication_focus (0x4a6b0): console/script command that
 * toggles bits in a debug bit vector at 0x5aca24.
 *
 * Structurally identical to ai_debug_communication_suppress (0x4a650) and
 * ai_debug_communication_ignore (0x4a680); all three are pure forwarders to
 * ai_debug_toggle_flags (0x4a460) with three constants baked in: the
 * destination bit vector, its size in bits, and the name->index lookup used
 * to resolve each argument string.  This one differs in ALL THREE constants,
 * not just the vector address.
 *
 * Confirmed: cdecl.  PUSH EBP / MOV EBP,ESP, no SUB ESP, no locals, no FPU,
 * no struct access, a single CALL, then POP EBP / RET (no RET n, so cdecl
 * and not stdcall).  Thirteen instructions total.
 *
 * The two incoming arguments are loaded into registers BEFORE the pushes
 * (`MOV EAX,dword ptr [EBP+0xC]` and `MOV ECX,dword ptr [EBP+0x8]`), which
 * is what proves this takes two stack parameters; the previous kb.json
 * declaration of `void (void)` is why the decompiler emitted an
 * argument-less wrapper around a bare FUN_0004a460() call.
 *
 * Call-site table for ai_debug_toggle_flags (last push = first arg, cdecl):
 *   arg5 lookup      PUSH 0x1a67e0  -> FUN_001a67e0, the same name->index
 *                                      lookup ai_debug_speak uses for the
 *                                      vocalization type name
 *   arg4 vector_size PUSH 0xd1      -> 209 entries
 *   arg3 dest_vector PUSH 0x5aca24  -> bit vector, passed as an integer
 *                                      address (the callee's dest_vector
 *                                      parameter is `int`)
 *   arg2 names       PUSH EAX       -> [EBP+0xC]
 *   arg1 count       PUSH ECX       -> [EBP+0x8]
 *   CALL 0x4a460 ; ADD ESP,0x14     (5 dwords, caller cleanup -> cdecl)
 *
 * UNCERTAIN — the name is from kb.json and is corroborated by the hs
 * debug_string dispatch table (function_index 0x1a; see
 * src/halo/hs/hs_library_internal_runtime.h:989), but the two constants
 * disagree with the "communication" reading of that name: the sibling
 * suppress/ignore commands use the 57-entry communication-type table via
 * ai_communication_get_type_by_name (0x42ce0), whereas this one uses the
 * 209-entry vocalization-type lookup FUN_001a67e0 (0x1a67e0).  So the bit
 * vector at 0x5aca24 is indexed by vocalization type, not communication
 * type.  Neither the vector nor the lookup has an independent name string,
 * so the kb name is kept as-is and the discrepancy is recorded here.
 *
 * vector_size 0xd1 is far under the callee's "vector_size <= 2048" assert,
 * so that path can never fire from here.  The callee allocates its own
 * 64-dword scratch mask, so nothing is allocated on this frame.  No
 * __FILE__ string of its own. */
void ai_debug_communication_focus(int count, char **names)
{
  ai_debug_toggle_flags(count, names, 0x5aca24, 0xd1, FUN_001a67e0);
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

/* FUN_0004a8c0 (0x4a8c0) — render the AI "communication" debug ring buffer.
 * Walks the 32-entry ring stored in the AI globals block (*(char**)0x632574)
 * from head (+0x130) to tail (+0x132), and for every live entry draws a debug
 * sphere at the entry's position plus a text label "c<count> t<age>" offset
 * 0.3 units along the world up/forward vector.
 *
 * Ring layout (confirmed from `lea eax,[eax+eax*4]` / `lea
 * edi,[edx+eax*4+0x134]` — stride 0x14, base +0x134): +0x00  int16   type   (-1
 * == empty slot; also indexes the colour table when in [0,3)) +0x02  int16
 * count  (MOVSX -> int, printed as %d) +0x04  float[3] position +0x10  int32
 * timestamp (game ticks; age = game_time - timestamp) Header int16s: +0x130
 * head (zero-extended: `xor ecx,ecx; mov cx,[edx+0x130]`), +0x132 tail (16-bit
 * compare `cmp ax,[edx+0x132]`).
 *
 * Colour selection: the fallback pointer is loaded FIRST
 * (`mov ebx,[0x2ee6c4]`) and only then conditionally overwritten by an
 * indexed lookup through a 3-entry stack table of colour-pointer ADDRESSES
 * (0x2ee6d8 / 0x2ee6e0 / 0x2ee6d0), which is dereferenced once more
 * (`mov edx,[ebp+ecx*4-0x14]; mov ebx,[edx]`).  The 0x2ee6xx globals hold
 * pointers into the 0x2677xx real_argb_color pool — same idiom already used
 * by actors.c:3555 and actor_looking.c:835.
 *
 * Call-site verification (all cdecl, ADD ESP,0x10 after each):
 *   0x4a955 FUN_00189540: PUSH EBX(color) / PUSH 0x3e4ccccd(0.2f) /
 *           PUSH ESI(=EDI+4, position) / PUSH 1  -> (1, pos, 0.2f, color)
 *   0x4a9a6 csprintf:     PUSH EAX(game_time-timestamp) / PUSH ECX(count) /
 *           PUSH 0x25aed0("c%d t%d") / PUSH 0x5ab100(static buffer)
 *   0x4a9b5 FUN_00189cb0: PUSH EBX(color) at 0x4a96e — BEFORE csprintf's own
 *           pushes — then PUSH EAX(csprintf result) / PUSH EDX(&text_position)
 *           / PUSH 1.  The hoisted colour push proves the csprintf call is
 *           NESTED as argument 3 of FUN_00189cb0, not assigned to a temp.
 *           EBX is reloaded with [EDI+0x10] (timestamp) right after its push.
 *
 * Store-offset table (text_position, EBP-0x20 .. EBP-0x18, passed by
 * `lea edx,[ebp-0x20]` so the three floats must be contiguous):
 *   +0x00 <- FLD [EAX]   ; FMUL [0x2533e4]=0.3f ; FADD [ESI] (0x4a95f..0x4a974)
 *   +0x04 <- FLD [EAX+4] ; FMUL [0x2533e4]      ; FADD [ESI+4]
 * (0x4a977..0x4a983) +0x08 <- FLD [EAX+8] ; FMUL [0x2533e4]      ; FADD [ESI+8]
 * (0x4a986..0x4a9a3) Every component is load-mul-add in the same operand order;
 * do not fold into an fma-shaped expression.  EAX = *(float**)0x31fc44 (world
 * up/forward constant, same global as actors.c:4596).
 *
 * The AI-globals pointer is re-read from 0x632574 only at the bottom of the
 * live-entry branch (`mov edx,[0x632574]` at 0x4a9ba); the empty-slot path
 * keeps the stale pointer, so the reload must be the last statement of the
 * `type != -1` block.  Ring advance is `INC CL; AND ECX,0x1f` — a 32-entry
 * wrap.  Confirmed: cdecl, no args, PUSH EBP / MOV EBP,ESP / SUB ESP,0x20;
 * EBX/ESI/EDI are saved only on the non-empty path (MSVC sank the saves past
 * the initial JE). */
void FUN_0004a8c0(void)
{
  short index;
  int game_time;
  void **color_table[3];
  float text_position[3];
  char *ai_globals;
  char *entry;
  float *pos;
  float *up;
  void *color;
  short type;

  game_time = game_time_get();
  ai_globals = *(char **)0x632574;
  index = (short)*(uint16_t *)(ai_globals + 0x130);

  while (index != *(int16_t *)(ai_globals + 0x132)) {
    entry = ai_globals + 0x134 + (int)index * 0x14;
    type = *(int16_t *)entry;
    if (type != -1) {
      pos = (float *)(entry + 4);
      color_table[0] = (void **)0x2ee6d8;
      color_table[1] = (void **)0x2ee6e0;
      color_table[2] = (void **)0x2ee6d0;
      color = *(void **)0x2ee6c4;
      if (type >= 0 && type < 3) {
        color = *color_table[type];
      }
      FUN_00189540(1, pos, 0.2f, color);
      up = *(float **)0x31fc44;
      text_position[0] = up[0] * 0.3f + pos[0];
      text_position[1] = up[1] * 0.3f + pos[1];
      text_position[2] = up[2] * 0.3f + pos[2];
      FUN_00189cb0(1, text_position,
                   csprintf((char *)0x5ab100, "c%d t%d",
                            (int)*(int16_t *)(entry + 2),
                            game_time - *(int32_t *)(entry + 0x10)),
                   (int)color);
      ai_globals = *(char **)0x632574;
    }
    index = (short)((index + 1) & 0x1f);
  }
}

/* FUN_0004a9f0 (0x4a9f0) — O(n^2) pairwise suppression pass over the
 * actor_path_debug_array (base *(char **)0x331f5c, stride 0x1ca7c, 0x20
 * slots).  For every ordered pair (outer, inner) with inner > outer, when both
 * records are live and they describe the "same" debug event, the older of the
 * two is retired by clearing its valid flag at +0xc.  When the OUTER record is
 * the one retired the inner scan stops immediately (the outer record is now
 * dead, so no further pairing against it is meaningful).
 *
 * Record fields used (same layout as ai_debug_get_path_storage at 0x49120):
 *   +0x00 int        key (actor handle / event id) — must be equal
 *   +0x04 int        creation stamp — larger == newer; the SMALLER one dies
 *   +0x0c char       valid flag (cleared to 0 to suppress)
 *   +0x0d char       second gate flag (both records must have it set)
 *   +0x28 float[3]   primary point (x,y,z)
 *   +0x60 char       has-direction flag — must be equal on both records
 *   +0x64 float[3]   secondary point, only compared when +0x60 != 0
 *
 * Confirmed ABI: __cdecl, no arguments, no return value.
 * Frame: PUSH EBP / MOV EBP,ESP / SUB ESP,0xc / PUSH EBX,ESI,EDI — three
 * dword locals: the byte-offset accumulator, the 16-bit outer index and the
 * 0x20 down-counter that drives the outer do/while.  The outer loop is a
 * counted do/while with a separate offset accumulator (offset += 0x1ca7c),
 * NOT an index*stride multiply; only the INNER loop multiplies (MOVSX ESI,BX /
 * IMUL ESI,ESI,0x1ca7c).  Both loops re-load the table base from 0x331f5c on
 * every iteration (0x4aa10 and 0x4aa40), so it is not cached in a local.
 *
 * Loop induction is 16-bit: CMP BX,0x20 / JGE for the outer gate and
 * CMP BX,0x20 / JL for the inner bottom test, with MOVSX before the IMUL.
 *
 * Distance block (0x4aa66..0x4aa99): three FLD/FSUB pairs in ASCENDING offset
 * order 0x28, 0x2c, 0x30, each `FLD [ESI+off]; FSUB [EDI+off]` — the delta is
 * inner MINUS outer.  The three deltas stay resident on the x87 stack and are
 * then squared in the order z, x, y with two FADDPs, i.e. the sum is
 * (dz*dz + dx*dx) + dy*dy.  Squaring makes the subtraction direction
 * behaviourally irrelevant but both the direction and the summation order are
 * transcribed literally for codegen fidelity.  VC71 schedules the second and
 * third squares itself: writing the sum as (dz,dy,dx) instead of (dz,dx,dy)
 * produces byte-identical output, so the residual [FPU-WARN] on the two
 * `fld %st(N)` / `fmul %st(N+1),%st` pairs is a scheduling artifact of the
 * verify compiler, not an operand-order bug (all three terms are squares, so
 * the sum is order-independent).
 *
 * Both threshold tests are `FCOMP [0x25337c]; FNSTSW AX; TEST AH,0x5; JP` —
 * STRICT less-than (JP is taken for greater-or-equal and for equal), not <=.
 * [0x25337c] holds 0.25f; it is written as a literal so VC71 pools it the way
 * the original did instead of emitting an absolute operand.
 *
 * Call-site verification (cdecl: the first PUSH is the LAST C argument):
 *   distance_squared3d (0x121a0) at 0x4aaa7 — LEA EDX,[ESI+0x64] / PUSH EDX /
 *   LEA EAX,[EDI+0x64] / PUSH EAX / CALL / (FCOMP) / ADD ESP,0x8
 *     arg# | binary source           | C expression   | match
 *       1  | last PUSH = EAX = EDI+0x64 (outer) | outer + 0x64 | yes
 *       2  | first PUSH = EDX = ESI+0x64 (inner) | inner + 0x64 | yes
 *   Result is consumed from ST(0) (float return) — the ADD ESP,0x8 lands
 *   between the FCOMP and the FNSTSW, which is pure scheduling.
 *   This is the ONLY call in the function; no buffers are allocated or passed,
 *   so there is no store-offset table to derive. */
void FUN_0004a9f0(void)
{
  char *inner;
  char *outer;
  float dx;
  float dy;
  float dz;
  int offset;
  int remaining;
  short j;
  short outer_index;

  outer_index = 1;
  offset = 0;
  remaining = 0x20;
  do {
    outer = *(char **)0x331f5c + offset;
    if (outer[0xc] != '\0' && outer[0xd] != '\0') {
      for (j = outer_index; j < 0x20; j++) {
        inner = *(char **)0x331f5c + (int)j * 0x1ca7c;
        if (inner[0xc] != '\0' && inner[0xd] != '\0' &&
            *(int *)inner == *(int *)outer) {
          dx = *(float *)(inner + 0x28) - *(float *)(outer + 0x28);
          dy = *(float *)(inner + 0x2c) - *(float *)(outer + 0x2c);
          dz = *(float *)(inner + 0x30) - *(float *)(outer + 0x30);
          if (dz * dz + dx * dx + dy * dy < 0.25f &&
              outer[0x60] == inner[0x60] &&
              (outer[0x60] == '\0' ||
               distance_squared3d((const float *)(outer + 0x64),
                                  (const float *)(inner + 0x64)) < 0.25f)) {
            if (*(int *)(outer + 4) < *(int *)(inner + 4)) {
              outer[0xc] = 0;
              break;
            }
            inner[0xc] = 0;
          }
        }
      }
    }
    offset += 0x1ca7c;
    outer_index++;
    remaining--;
  } while (remaining != 0);
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

/* ai_debug_change_selected_encounter: step the debug encounter selection to
 * the next (param non-zero) or previous (param zero) allocated encounter datum
 * and echo a one-line description of it to the console.
 *
 * When the datum walk runs off the end of the data array, print
 * "no more encounters" and deselect (ai_debug_select_encounter(-1)).
 * Otherwise fetch the matching scenario encounter tag-block element
 * (scenario + 0x42c, element size 0xb0) and format:
 *   "encounter <name> [<active|inactive> <description>] (<n> actors)"
 * where <name> is the char name[32] at the head of the encounter element,
 * <description> is either "3d-positions" (flags bit 0x20) or
 * "<manual|auto>-bsp <bsp>" with <bsp> the int16 at +0x7e printed as "%d" or
 * "NONE" when it is -1, and flags bit 0x40 selects "manual" over "auto".
 *
 * No __FILE__ string.  Globals: 0x5ab270 = encounter data_t*,
 * 0x5ac9f4 = currently selected encounter datum index.
 *
 * Call-site verification (cdecl, first PUSH is last C arg):
 *   0x4afd3 data_prev_index: PUSH EDX([0x5ac9f4]); PUSH EAX([0x5ab270])
 *     -> data_prev_index(encounter_data, selected)                 [match]
 *   0x4afbf data_next_index: PUSH EAX([0x5ac9f4]); PUSH ECX([0x5ab270])
 *     -> data_next_index(encounter_data, selected)                 [match]
 *   0x4afe5 datum_absolute_index_to_index: PUSH EBX(index); PUSH ECX(data)
 *     -> datum_absolute_index_to_index(encounter_data, index)      [match]
 *     Result kept in EDI and NULL-tested; it is a datum POINTER even though
 *     the kb declaration types it int (ABI-immutable, so cast at the site).
 *   early-return: PUSH "no more encounters"; PUSH EAX(the 0 just returned,
 *     reused as the literal channel 0) -> console_printf(0, ...)   [match]
 *     then PUSH -1; ai_debug_select_encounter(-1); one ADD ESP,0xC covers both.
 *   0x4b01d/0x4b028: PUSH 0xB0; PUSH EDX(index & 0xffff); CALL
 *     global_scenario_get; ADD EAX,0x42C; PUSH EAX; CALL tag_block_get_element
 *     -> tag_block_get_element(scenario + 0x42c, index & 0xffff, 0xb0) [match]
 *     (args are pushed before global_scenario_get runs — right-to-left cdecl
 *     evaluation of the C call reproduces that order).
 *   0x4b07f crt_sprintf: PUSH EDX(MOVSX int16 @+0x7e); PUSH "%d";
 *     PUSH ECX(EBP-0x100) -> crt_sprintf(bsp_text, "%d", bsp)      [match]
 *   0x4b0ab crt_sprintf: PUSH ECX(EBP-0x100); PUSH EAX("manual"/"auto");
 *     PUSH "%s-bsp %s"; PUSH EDX(EBP-0x200); ADD ESP,0x10
 *     -> crt_sprintf(description, "%s-bsp %s", mode, bsp_text)     [match]
 *   0x4b0d9 console_printf: PUSH ECX(MOVSX int16 @EDI+0x2a); PUSH
 * EDX(EBP-0x200); PUSH EAX("active"/"inactive"); PUSH ESI(encounter element,
 * printed as the %s name); PUSH "encounter %s [%s %s] (%d actors)"; PUSH 0,
 * then PUSH EBX; CALL ai_debug_select_encounter; one ADD ESP,0x1C covers both.
 * [match]
 *
 * Frame: SUB ESP,0x200 — exactly two char[256] buffers, EBP-0x200
 * (description) and EBP-0x100 (bsp_text).  No FPU instructions.
 * The parameter test is a BYTE test (MOV AL,[EBP+8]; TEST AL,AL). */
void ai_debug_change_selected_encounter(int next)
{
  unsigned int index;
  char *datum;
  void *encounter;
  const char *text;
  char description[256];
  char bsp_text[256];

  if ((char)next == 0) {
    index = data_prev_index(*(data_t **)0x5ab270, *(int32_t *)0x5ac9f4);
  } else {
    index =
      (unsigned int)data_next_index(*(data_t **)0x5ab270, *(int32_t *)0x5ac9f4);
  }
  datum =
    (char *)datum_absolute_index_to_index(*(data_t **)0x5ab270, (int)index);
  if (datum == NULL) {
    console_printf((int)datum, "no more encounters");
    ai_debug_select_encounter(-1);
    return;
  }
  encounter = tag_block_get_element((char *)global_scenario_get() + 0x42c,
                                    (int)(index & 0xffff), 0xb0);
  if ((*((uint8_t *)encounter + 0x20) & 0x20) == 0) {
    if (*(int16_t *)((char *)encounter + 0x7e) == -1) {
      csstrcpy(bsp_text, "NONE");
    } else {
      crt_sprintf(bsp_text, "%d", (int)*(int16_t *)((char *)encounter + 0x7e));
    }
    text = (*((uint8_t *)encounter + 0x20) & 0x40) ? "manual" : "auto";
    crt_sprintf(description, "%s-bsp %s", text, bsp_text);
  } else {
    csstrcpy(description, "3d-positions");
  }
  text = (datum[0xd] != 0) ? "active" : "inactive";
  console_printf(0, "encounter %s [%s %s] (%d actors)", encounter, text,
                 description, (int)*(int16_t *)(datum + 0x2a));
  ai_debug_select_encounter((int)index);
}

/* ai_debug_teleport_to (0x4b0f0) — debug command: teleport every player unit
 * onto the starting locations of the given scenario encounter.
 *
 * Walks the encounter's nested "starting locations" tag_block (encounter
 * element + 0xa4, element size 0x34) and drops each live player unit onto a
 * location in turn, wrapping with a modulo when there are more players than
 * locations.  The unit's facing is derived from the location's yaw field
 * (element + 0xc) as {cos(yaw), sin(yaw), 0.0f}; the up vector is NULL so
 * object_set_position keeps the object's current up.
 *
 * Confirmed (disassembly 0x4b0f0-0x4b1a5):
 *   The kb declaration was "void ai_debug_teleport_to(void)", which is wrong:
 *   MOV EAX,[EBP+0x8] at 0x4b0f6 reads one stack argument.  The early-out
 *   CMP EAX,-1 / JZ at 0x4b0f9 tests the FULL 32-bit value; the AND
 *   EAX,0xffff mask is applied only afterwards, on the tag_block index, so
 *   the parameter is an int, not a short.
 *   The frame is PUSH EBP / MOV EBP,ESP / SUB ESP,0x1c, and EBX/ESI/EDI are
 *   pushed at 0x4b102 — after the early-out test.  The param == -1 path
 *   branches straight to MOV ESP,EBP, so the source shape is a single early
 *   "return" before any other work.
 *   0x4b10a/0x4b10f PUSH 0xb0 / PUSH EAX are pre-staged arguments for the
 *   tag_block_get_element that follows global_scenario_get (which takes no
 *   arguments); ADD ESP,0xc at 0x4b128 is that call's cdecl cleanup.
 *   EDI = element + 0xa4 is a nested tag_block header; the loop guard is
 *   CMP DWORD PTR [EDI],0 / JLE, i.e. "count > 0".
 *   The counter EBX is a signed 16-bit value: MOVSX EAX,BX / CDQ / IDIV at
 *   0x4b153-0x4b15a performs the signed modulo against the location count.
 *   INC EBX is inside the "unit handle != -1" arm — the continue path jumps
 *   to 0x4b192 (the next data_iterator_next) and bypasses it, so players
 *   without a unit do not consume a starting location.
 *   The facing is two reloads of the SAME source: FLD [EAX+0xc] / FCOS /
 *   FSTP [EBP-0xc] then FLD [EAX+0xc] / FSIN / FSTP [EBP-0x8], followed by
 *   MOV DWORD PTR [EBP-0x4],0.  No subtraction and no cross product, so
 *   there is no operand-order hazard.
 *   [EBP-0xc]..[EBP-0x4] is one float[3]; LEA ECX,[EBP-0xc] is what gets
 *   pushed.  Ghidra's local_10/local_c/local_8 are that array's three slots
 *   (its local_10 name is off by one frame slot), not independent variables.
 *
 * Call-site verification (all cdecl, first PUSH = last argument):
 *   0x4b110 global_scenario_get()                                    [match]
 *   0x4b123 tag_block_get_element: PUSH 0xb0, PUSH EAX(idx & 0xffff),
 *           PUSH EAX(scenario + 0x42c) -> (block, index, 0xb0)       [match]
 *   0x4b13b data_iterator_new: PUSH [0x5aa6d4], PUSH ECX(&iter)      [match]
 *   0x4b144 data_iterator_next: PUSH &iter.  The ADD ESP,0xc here merges
 *           this call's 1 dword with data_iterator_new's 2; the second call
 *           site at 0x4b196 shows the true ADD ESP,0x4, so the ARG_COUNT
 *           hazard on this site is a false positive.
 *   0x4b162 tag_block_get_element: PUSH 0x34, PUSH EAX(index % count),
 *           PUSH EDI(locations)                                      [match]
 *   0x4b189 object_set_position: PUSH 0, PUSH ECX(&forward[0]),
 *           PUSH EAX(location), PUSH EDX(*(int *)(player + 0x34)).
 *           ADD ESP,0x1c cleans these 4 dwords plus the 3 from the
 *           tag_block_get_element above.                             [match]
 *
 * Store-offset table (frame, from the raw disassembly):
 *   [EBP-0x1c] .. [EBP-0x0d]  data_iter_t iter (0x10 bytes, LEA ECX,[EBP-0x1c])
 *   [EBP-0x0c] <- FSTP after FCOS   forward[0] = cos(yaw)
 *   [EBP-0x08] <- FSTP after FSIN   forward[1] = sin(yaw)
 *   [EBP-0x04] <- MOV imm 0         forward[2] = 0.0f
 *
 * Uncertain: the exact meaning of the 0x34-byte starting-location element
 *   beyond position at +0x00..+0x08 and yaw at +0x0c. */
void ai_debug_teleport_to(int encounter_index)
{
  data_iter_t iter;
  float forward[3];
  char *player;
  int *locations;
  float *location;
  short index;

  if (encounter_index == -1) {
    return;
  }
  locations = (int *)((char *)tag_block_get_element(
                        (char *)global_scenario_get() + 0x42c,
                        (int)((unsigned int)encounter_index & 0xffff), 0xb0) +
                      0xa4);
  if (*locations > 0) {
    index = 0;
    data_iterator_new(&iter, *(data_t **)0x5aa6d4);
    player = (char *)data_iterator_next(&iter);
    while (player != (char *)0) {
      if (*(int *)(player + 0x34) != -1) {
        location = (float *)tag_block_get_element(
          locations, (int)index % *locations, 0x34);
        forward[0] = (float)cos((double)location[3]);
        forward[1] = (float)sin((double)location[3]);
        forward[2] = 0.0f;
        object_set_position(*(int *)(player + 0x34), location, forward,
                            (float *)0);
        index = (short)(index + 1);
      }
      player = (char *)data_iterator_next(&iter);
    }
  }
}

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

/* FUN_0004b2b0: advance the AI debug text cursor by one line.
 *
 * Saves the current cursor position (vec3 @0x5ac9b0) into the previous-position
 * slot (vec3 @0x5ac9a0), then steps the cursor along the world up vector:
 *   current += line_spacing(0x5ac990) * up[0..2]
 * and returns a pointer to the SAVED previous position, so the caller renders
 * its string at the old spot while the cursor already points at the next line.
 *
 * Confirmed (cachebeta.xbe 0x4b2b0..0x4b31a, 8 x87/GPR ops, no CALLs):
 *   - Implicit-EAX return.  `MOV EAX,0x5ac9a0` at 0x4b309 sits between the last
 *     FMUL and the last FADD, i.e. it is a deliberate return value, not dead
 *     code.  All 63 call sites in the XBE (0x4bc24 .. 0x53486) follow the call
 *     with `PUSH EAX; PUSH 1; CALL 0x189cb0`, so the pointer IS consumed.  The
 *     kb decl was `void (void)`; corrected to `float *(void)` — leaving it void
 *     would have handed every caller an undefined EAX.
 *   - The three-dword save is a GPR copy in the original: three loads into
 *     EAX/ECX/EDX from 0x5ac9b0/b4/b8, then three stores to 0x5ac9a0/a4/a8,
 *     with the first FLD and the up-pointer load scheduled into the gaps.
 *     Transcribed with three dword temps so the loads precede the stores;
 *     writing it as three plain assignments makes the compiler emit
 *     interleaved load/store pairs instead.
 *   - The up-vector pointer at 0x31fc44 is loaded ONCE into EAX at 0x4b2cc,
 *     i.e. after the first store frees EAX; all three FMULs index [EAX],
 *     [EAX+4], [EAX+8].  Cached in `up`, read after the copy.
 *   - FPU operand order per component is FLD [0x5ac990]; FMUL [EAX+n];
 *     FADD <pos>; FSTP — so the C expression must be `scale * up[n] + pos`,
 *     scale first.  All adds; no FSUB, so no subtraction-direction hazard.
 *   - Asymmetric addend, preserved deliberately: component 0 FADDs from
 *     0x5ac9b0 itself (0x4b2df) while components 1 and 2 FADD from the
 *     just-saved copies at 0x5ac9a4 / 0x5ac9a8 (0x4b2f4 / 0x4b30e).  The values
 *     are identical only because the save happens first — do not reorder.
 *
 *   - The FIRST read of the line spacing is hoisted into a local, because the
 *     reference schedules its `flds 0x5ac990` into the middle of the copy
 *     block rather than after it.  Components 1 and 2 must keep reading the
 *     global directly so their `fmuls 0x5ac990` memory operands survive.
 *     Nothing in this function writes 0x5ac990, so the hoist is value-safe.
 *
 * Uncertain: no __FILE__ string and no assert anchor, so the original symbol
 * name is unknown; kept as FUN_0004b2b0. */
float *FUN_0004b2b0(void)
{
  float *up;
  uint32_t x;
  float line_spacing;
  uint32_t y;
  uint32_t z;

  line_spacing = *(float *)0x5ac990;
  x = *(uint32_t *)0x5ac9b0;
  y = *(uint32_t *)0x5ac9b4;
  z = *(uint32_t *)0x5ac9b8;
  *(uint32_t *)0x5ac9a0 = x;
  *(uint32_t *)0x5ac9a4 = y;
  *(uint32_t *)0x5ac9a8 = z;

  up = *(float **)0x31fc44;

  *(float *)0x5ac9b0 = line_spacing * up[0] + *(float *)0x5ac9b0;
  *(float *)0x5ac9b4 = *(float *)0x5ac990 * up[1] + *(float *)0x5ac9a4;
  *(float *)0x5ac9b8 = *(float *)0x5ac990 * up[2] + *(float *)0x5ac9a8;

  return (float *)0x5ac9a0;
}

/* FUN_0004b670 (0x4b670): draws the AI debug "unit position" marker for a
 * unit passed in EDI, with a caller-supplied colour in EBX and a draw-extras
 * flag on the stack (`draw_flag`, [EBP+8]).  Register args confirmed from
 * disasm: EDI/EBX are read without being defined inside the function (no
 * prologue PUSH for either, only ESI is saved/restored), and both are
 * forwarded unchanged into callees whose kb.json decls fix their meaning
 * (EDI -> object_try_and_get_and_verify_type's datum_handle and
 * biped_get_camera_height_and_offset's unit_handle; EBX -> the `color`
 * argument of every FUN_001898xx draw call).
 *
 * Verified against raw disasm (not the Ghidra decompile's pseudo-C, whose
 * local_c/local_8 stack-slot names are swapped from their real EBP offsets):
 *   0x4b67a object_try_and_get_and_verify_type(object_handle, 1) -> unit.
 *           NULL -> return (whole function is a no-op).
 *   0x4b695 object_try_and_get_and_verify_type(*(int*)(unit+0xcc), 3) ->
 *           weapon.  unit+0xcc is read-only evidence (struct_offsets: ESI
 *           0xcc); kept as a raw offset, matching this file's existing
 *           convention for object-pointer fields with no recovered struct
 *           (see the object+0x2d4 "owner handle" cast earlier in this file).
 *   0x4b6a1 weapon != NULL && *(int*)(weapon+0x2d4) == object_handle (i.e.
 *           the weapon's owner is this unit) selects the FUN_0001aae0 path;
 *           otherwise biped_get_camera_height_and_offset.
 *   0x4b6b8 FUN_0001aae0(weapon_handle, center, &camera_height); the
 *           height_offset out-param is not touched by this callee and is set
 *           to 0.0f explicitly right after (0x4b6c0), matching decompile.
 *   0x4b6d6 biped_get_camera_height_and_offset(object_handle, (vector3_t*)
 *           center, &height_offset, &camera_height) — argument order fixed
 *           by push order (EDI pushed last = arg1).
 *   0x4b6e3/0x4b6f3 FCOMP+FNSTSW+TEST AH,0x41/JNZ: branch-B (FUN_00189540) is
 *           taken when draw_flag==0 OR height_offset<=*(float*)0x2533c0;
 *           branch-A (FUN_00189860) only when draw_flag!=0 AND
 *           height_offset>that threshold. This matches the decompile's
 *           `(flag=='\0') || (height_offset<=FLOAT_002533c0)` predicate.
 *   0x4b718 FUN_00189860(1, center, height_vec, camera_height, color) where
 *           height_vec = {0.0f, 0.0f, height_offset} is built in-place
 *           (EBP-0x20/-0x1c/-0x18) right before the call; radius arg is the
 *           unscaled camera_height (no FMUL on this path).
 *   0x4b736 FUN_00189540(1, center, camera_height * *(float*)0x25afcc,
 *           color) — PUSH ECX is a dummy slot immediately overwritten by
 *           FSTP [ESP] with the FMUL result (FPU_ARG hazard already
 *           resolved: the real float arg is the FSTP value, not the pushed
 *           dummy register).
 *   0x4b759 (only if draw_flag!=0) FUN_00189150(1, center, camera_height *
 *           *(float*)0x255154, color) — same FSTP-over-dummy shape.
 *
 * Uncertain: no __FILE__ string/assert anchor for this function; kept as
 * FUN_0004b670. Confirmed callers: FUN_0004c920 (0x4caaa, 0x4cad6), not yet
 * ported, so the caller-side register setup is not cross-checked here. */
void FUN_0004b670(int object_handle, void *color, char draw_flag)
{
  void *unit;
  void *weapon;
  int weapon_handle;
  float center[3];
  float height_offset;
  float camera_height;
  float height_vec[3];

  unit = object_try_and_get_and_verify_type(object_handle, 1);
  if (unit == NULL) {
    return;
  }

  weapon_handle = *(int32_t *)((char *)unit + 0xcc);
  weapon = object_try_and_get_and_verify_type(weapon_handle, 3);
  if (weapon != NULL && *(int32_t *)((char *)weapon + 0x2d4) == object_handle) {
    FUN_0001aae0(weapon_handle, center, &camera_height);
    height_offset = 0.0f;
  } else {
    biped_get_camera_height_and_offset(object_handle, (vector3_t *)center,
                                       &height_offset, &camera_height);
  }

  if (draw_flag == 0 || height_offset <= *(float *)0x2533c0) {
    FUN_00189540(1, center, camera_height * *(float *)0x25afcc, color);
  } else {
    height_vec[0] = 0.0f;
    height_vec[1] = 0.0f;
    height_vec[2] = height_offset;
    FUN_00189860(1, center, height_vec, camera_height, color);
  }

  if (draw_flag != 0) {
    FUN_00189150(1, center, camera_height * *(float *)0x255154, color);
  }
}

/* FUN_0004b7a0: service the pending "select actor" debug-key request.  Asks
 * FUN_00049c70 for a candidate actor handle; when one exists, describes it into
 * the shared error/description buffer at 0x5ab100, echoes "selected %s" to the
 * console, and points the debug encounter/actor selection at that actor's
 * encounter (actor + 0x34) and handle.  When no actor is available the
 * selection is reset with ai_debug_select_actor(-1, -1).  Either way the
 * request flag at 0x5ac9c1 (raised by debug_key_erase_all_actors, 0xffdc0) is
 * cleared.
 *
 * No __FILE__ string.  38 instructions, two-branch, no FPU, no loops, no stack
 * locals (no `sub esp`): ESI holds the handle, EDI the datum_get result.
 *
 * FUN_00049c70's kb declaration was `void (void)`; the disassembly does
 * MOV ESI,EAX immediately after the CALL, so it really returns an int handle
 * (-1 = none).  Ghidra models this as `extraout_EAX`.  The kb decl has been
 * corrected to `int FUN_00049c70(void);` (implicit-EAX return, not a register
 * argument).
 *
 * Branch: CMP ESI,-1 / JZ 0x4b7f8 — equality against -1, so the positive
 * (handle != -1) path is the fall-through and the reset is the else arm.
 *
 * Call-site verification (cdecl, first PUSH = last arg):
 *   0x4b7b5 datum_get: PUSH ESI (handle), PUSH EAX (=[0x6325a4] actor_data)
 *     -> datum_get(actor_data, actor_handle)                       [match]
 *   0x4b7cb ai_debug_describe_actor: PUSH 0x100, PUSH 0x5ab100, PUSH 0x1,
 *     PUSH -0x1, PUSH ESI
 *     -> ai_debug_describe_actor(handle, -1, 1, 0x5ab100, 0x100)   [match]
 *     (MOV EDI,EAX at 0x4b7c9 captures datum_get's result BEFORE this call,
 *      interleaved among the pushes — it is not this call's return.)
 *   0x4b7dc console_printf: PUSH 0x5ab100, PUSH 0x25afd0 ("selected %s"),
 *     PUSH 0x0 -> console_printf(0, "selected %s", 0x5ab100)       [match]
 *     Return value discarded.
 *   0x4b7e6 ai_debug_select_actor: PUSH ESI, PUSH ECX (=[EDI+0x34])
 *     -> ai_debug_select_actor(*(int32_t *)(actor + 0x34), handle) [match]
 *   0x4b7fc ai_debug_select_actor: PUSH -0x1, PUSH -0x1
 *     -> ai_debug_select_actor(-1, -1)                             [match]
 *
 * Stack cleanup: one shared ADD ESP,0x30 at 0x4b7eb covers all four calls in
 * the taken branch = 2 + 5 + 3 + 2 = 12 dwords = 0x30 bytes (the per-call
 * "cleanup=12 stack args" hazard report is a false positive).  The else arm
 * has its own ADD ESP,0x8 for its single 2-arg call.
 *
 * Store-offset table (absolute addresses):
 *   [0x5ab100] <- description text (written by ai_debug_describe_actor; a
 *                 0x100-byte global scratch buffer, not a stack local — the
 *                 0x100 size argument confirms it)
 *   [0x5ac9c1] <- 0   byte  (MOV byte ptr [0x005ac9c1],0x0; emitted once per
 *                 branch because the two epilogues differ — the taken branch
 *                 POPs EDI as well as ESI) */
void FUN_0004b7a0(void)
{
  int actor_handle;
  char *actor;

  actor_handle = FUN_00049c70();
  if (actor_handle != -1) {
    actor = (char *)datum_get(actor_data, actor_handle);
    ai_debug_describe_actor(actor_handle, -1, 1, (char *)0x5ab100, 0x100);
    console_printf(0, "selected %s", (char *)0x5ab100);
    ai_debug_select_actor(*(int32_t *)(actor + 0x34), actor_handle);
  } else {
    ai_debug_select_actor(-1, -1);
  }
  *(uint8_t *)0x5ac9c1 = 0;
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

/* ai_debug_change_selected_actor: step the debug actor selection forward or
 * backward within the currently selected encounter.  Walks the encounter's
 * actor iterator to the position of the currently selected actor, moves one
 * step in the requested direction, then reports the new actor on the console
 * and commits it via ai_debug_select_actor.
 *
 * Confirmed (XBE 0x4c170): PUSH EBP / MOV EBP,ESP / SUB ESP,0xc.  The 12 bytes
 *   of frame are consumed entirely by the iterator passed as LEA [EBP-0xc] to
 *   encounter_actor_iterator_new/_next/_prev, so Ghidra's "local_c" (EBP-0x8)
 *   is iterator dword 1, not an independent local.  It is the current actor
 *   handle.  EDI holds the encounter datum, ESI the 16-bit position counter;
 *   ESI is pushed only after the early-exit branch, so that path pops EDI only.
 * Confirmed: only AL of the parameter is consulted (MOV AL,[EBP+8] / TEST
 *   AL,AL / JZ), hence the (char) test.
 * Confirmed: the position counter is 16-bit -- it reaches console_printf via
 *   MOVSX EDX,SI followed by INC EDX, i.e. widen-then-increment, so the
 *   argument is the promoted "idx + 1" and not a truncated (short)(idx + 1).
 * Confirmed: encounter + 0x2a is a int16_t actor count (MOVSX word ptr).
 * Confirmed: the buffer handed to ai_debug_describe_actor is the shared
 *   256-byte scratch at 0x5ab100 with an explicit 0x100 size.
 *
 * Call-site verification (cdecl, first PUSH is the last argument):
 *   0x4c184  PUSH EAX([0x5ac9f4]) / PUSH ECX(*(void**)0x5ab270)
 *            -> datum_absolute_index_to_index(*(data_t**)0x5ab270,
 *                                             *(int32_t*)0x5ac9f4)  [match]
 *   0x4c198  PUSH 0x25b0d0 / PUSH EAX -- EAX is the NULL result of the call
 *            above, reused as the channel argument.  Written as
 *            (int)encounter to reproduce the register reuse; it is provably
 *            NULL on this path.                                      [match]
 *   0x4c1a1  ai_debug_select_actor(-1,-1).  ADD ESP,0x10 folds the cleanup for
 *            this call and the console_printf above (2 + 2 dwords) -- the
 *            ARG_COUNT hazard on this site is a folded-cleanup false positive.
 *   0x4c1bc  PUSH EDX([0x5ac9f4]) / PUSH EAX(LEA [EBP-0xc])
 *            -> encounter_actor_iterator_new(iter, *(int32_t*)0x5ac9f4) [match]
 *   0x4c235  PUSH 0x100 / PUSH 0x5ab100 / PUSH 1 / PUSH -1 / PUSH EAX([EBP-8])
 *            -> ai_debug_describe_actor(iter[1], -1, 1, (char*)0x5ab100,
 *                                       0x100)                       [match]
 *   0x4c250  PUSH 0x5ab100 / PUSH MOVSX ECX,word[EDI+0x2a] /
 *            PUSH (MOVSX EDX,SI; INC EDX) / PUSH 0x25b0c0 / PUSH 0
 *            -> console_printf(0, "actor %d/%d: %s", idx + 1,
 *                              (int)*(int16_t*)(encounter + 0x2a),
 *                              (char*)0x5ab100)                       [match]
 *   0x4c260  ai_debug_select_actor(*(int32_t*)0x5ac9f4, iter[1]).
 *            ADD ESP,0x30 folds describe_actor(5) + console_printf(5) +
 *            ai_debug_select_actor(2) = 12 dwords.
 *   0x4c275  console_printf(0, "no more actors"); 0x4c283
 *            ai_debug_select_actor(*(int32_t*)0x5ac9f4, -1); ADD ESP,0x10.
 *
 * Note: MOV EDI,EDI at 0x4c1de is two-byte loop-head alignment padding, not
 *   code.  No FPU operations anywhere in this function.
 *
 * Uncertain: the remaining iterator dwords (0 and 2) are not touched here, so
 *   their meaning is left opaque; only dword 1 (the current actor handle) is
 *   read.
 *
 * Called from the ai_debug console/key handlers alongside
 * ai_debug_change_selected_encounter. */
void ai_debug_change_selected_actor(int param)
{
  int32_t iter[3];
  char *encounter;
  void *actor;
  int16_t idx;
  int more;

  encounter = (char *)datum_absolute_index_to_index(*(data_t **)0x5ab270,
                                                    *(int32_t *)0x5ac9f4);
  if (encounter == NULL) {
    console_printf((int)encounter, "no encounter selected (use F2/F3)");
    ai_debug_select_actor(-1, -1);
    return;
  }
  idx = 0;
  encounter_actor_iterator_new(iter, *(int32_t *)0x5ac9f4);
  if (*(int32_t *)0x5ac9f8 != -1) {
    more = encounter_actor_iterator_next(iter);
    while (more != 0 && iter[1] != *(int32_t *)0x5ac9f8) {
      idx++;
      more = encounter_actor_iterator_next(iter);
    }
  }
  if ((char)param == 0) {
    actor = encounter_actor_iterator_prev(iter);
    idx--;
  } else {
    actor = (void *)encounter_actor_iterator_next(iter);
    idx++;
  }
  if (actor != NULL) {
    ai_debug_describe_actor(iter[1], -1, 1, (char *)0x5ab100, 0x100);
    console_printf(0, "actor %d/%d: %s", idx + 1,
                   (int)*(int16_t *)(encounter + 0x2a), (char *)0x5ab100);
    ai_debug_select_actor(*(int32_t *)0x5ac9f4, iter[1]);
  } else {
    console_printf(0, "no more actors");
    ai_debug_select_actor(*(int32_t *)0x5ac9f4, -1);
  }
}

/* FUN_0004c890: draw the camera-follow LOS-hit debug line, then continue the
 * queued path-follow build.
 *
 * Guard: runs only when a target position was captured (DAT_5f91a8, set by
 * ai_debug_update's actor-position path) and a LOS hit was recorded
 * (DAT_5f91c0, set by ai_debug_update's LOS-hit path) and the path build has
 * NOT already failed (DAT_60d268 == 0, written by path_state_build_path).
 *
 * Color select (from the 13-entry debug color table documented in
 * ai_debug_render_points_and_lines):
 *   - colors[1] (0x2ee6d8) if neither DAT_5ac9ff nor DAT_5f9228 is set.
 *   - colors[4] (0x2ee6d4) if either is set and DAT_5f925c (int16) == 0.
 *   - colors[11] (0x2ee6e0) if DAT_5f925c (signed int16) >= 0x400.
 *   - colors[7] (0x2ee6e8) otherwise.
 * Draws a line from DAT_5f91ac (captured position) to DAT_5f91c4 (LOS-hit
 * slot 0) via FUN_00189270.
 *
 * Confirmed from disassembly (0x4c890-0x4c91a): the color selection is not
 * the nested-if the decompiler's comma-operator reconstruction suggests.
 * Traced instruction-by-instruction:
 *   JNZ [0x5ac9ff]!=0 -> L1(0x4c8c4) directly (0x5f9228 is tested only when
 *     0x5ac9ff is 0); JNZ [0x5f9228]!=0 -> L1; else EAX=colors[1], jump to
 *     the call site (0x4c8e6).
 *   L1 (0x4c8c4): CMP AX([0x5f925c]),0 ; JNZ L2(0x4c8d6); else EAX=colors[4],
 *     jump to the call site.
 *   L2 (0x4c8d6): EAX=colors[11] unconditionally, then CMP AX,0x400 (signed);
 *     JGE keeps colors[11] and falls into the call; else EAX=colors[7].
 *
 * Second half: if DAT_60d2d0 (path-ready flag, set by ai_debug_update) is
 * set, continue the path with FUN_0004b220(&DAT_60d2ec) and
 * FUN_0004c560(&DAT_60d2c4).  Both pass the *address* of the global (MOV
 * EAX/ESI, imm32 -- no brackets), not its value, matching their @<eax>/@<esi>
 * float-pointer / void-pointer parameter types already recorded in kb.json.
 *
 * No __FILE__ string.  Called from FUN_000534d0 (0x5359e, unconditional). */
void FUN_0004c890(void)
{
  void *color;

  if (*(uint8_t *)0x5f91a8 != 0 && *(uint8_t *)0x5f91c0 != 0 &&
      *(uint8_t *)0x60d268 == 0) {
    if (*(uint8_t *)0x5ac9ff == 0 && *(uint8_t *)0x5f9228 == 0) {
      color = *(void **)0x2ee6d8;
    } else if (*(int16_t *)0x5f925c == 0) {
      color = *(void **)0x2ee6d4;
    } else if (*(int16_t *)0x5f925c >= 0x400) {
      color = *(void **)0x2ee6e0;
    } else {
      color = *(void **)0x2ee6e8;
    }
    FUN_00189270(1, (float *)0x5f91ac, (float *)0x5f91c4, color);
  }

  if (*(uint8_t *)0x60d2d0 != 0) {
    FUN_0004b220((float *)0x60d2ec);
    FUN_0004c560((void *)0x60d2c4);
  }
}

/* FUN_00052ab0: debug-render one text label per active entry of the 32-entry
 * table at [0x331f5c] (stride 0x1ca7c).  For each entry whose two enable bytes
 * at +0x0c and +0x0d are both non-zero, it offsets the entry's world position
 * by the global up vector, pushes that as the debug-text anchor
 * (FUN_0004b220), formats the entry's actor description into a 256-byte stack
 * buffer, draws it at the current text cursor, and runs the paired
 * FUN_0004c560 pass for the entry.
 *
 * Confirmed (0x52ab0-0x52b50):
 *   - Loop is `do { } while (--count)`: XOR EDI,EDI / MOV EBX,0x20 /
 *     ... / ADD EDI,0x1ca7c / DEC EBX / JNE 0x52ac3.  The table base is
 *     re-read from [0x331f5c] inside the loop (0x52ac3), not hoisted.
 *   - Guard is two separate byte tests, not one int: MOV AL,[ESI+0xc] /
 *     TEST AL,AL / JE, then MOV AL,[ESI+0xd] / TEST AL,AL / JE.  Ghidra's
 *     `piVar1[3]` int load is wrong.
 *   - The three position components are FLOAT fields at +0x28/+0x2c/+0x30
 *     (FADD dword ptr), not ints -- Ghidra's `(float)piVar1[10]` cast would
 *     emit an FILD and a wrong value.  Operand order per component is
 *     FLD [EAX+n] (up vector) / FADD [ESI+0x28+n] / FSTP, i.e. `up[n] + pos`,
 *     up first.  Three independent 2-term adds, no association ambiguity.
 *   - 0x31fc44 holds a POINTER to the up vector; it is dereferenced once
 *     (MOV EAX,[0x31fc44] at 0x52ad9) and indexed [EAX], [EAX+4], [EAX+8].
 *   - CALL 0x4b220 at 0x52afb takes the local 3-float position in EAX
 *     (LEA EAX,[EBP-0xc] at 0x52af2, scheduled between the FLD and FADD of
 *     the third component).  All five XBE call sites of 0x4b220 set EAX to a
 *     pointer, and the callee does MOV ESI,EAX then reads [ESI]/[+4]/[+8], so
 *     the kb declaration is corrected to `float *position @<eax>`.
 *   - CALL 0x4c560 at 0x52b38 takes the entry pointer in ESI: the callee
 *     opens with TEST ESI,ESI / MOV AL,[ESI+0xc], and the other two XBE call
 *     sites (0x4c914, 0x52748) both load ESI immediately before the call.
 *     kb declaration corrected to `void *entry @<esi>`.
 *   - ai_debug_describe_actor is called with 5 args (ADD ESP,0x14) and its
 *     char* return is discarded; the stack buffer is what gets drawn.
 *   - The color dword [0x2ee6d0] is loaded at 0x52b18, BEFORE the ADD ESP,0x14
 *     and before the pushes for 0x189cb0, so it is the right-to-left-first
 *     (last) argument.  FUN_0004b2b0's return (EAX) is the `position`
 *     argument; Ghidra models it as `extraout_EAX` and reorders the call.
 *   - Frame: SUB ESP,0x10c = 0xc (float[3] at EBP-0xc) + 0x100 (buf at
 *     EBP-0x10c); epilogue is MOV ESP,EBP / POP EBP, no `leave`.
 *
 * Uncertain: no __FILE__ string and no assert anchor, so the original symbol
 *   name is unknown; kept as FUN_00052ab0.
 * Uncertain: the table at 0x331f5c is left as raw offsets.  Only +0x00 (int
 *   actor handle), +0x0c and +0x0d (enable bytes) and +0x28/+0x2c/+0x30
 *   (float position) are observed here, which is far too little to justify a
 *   struct for a 0x1ca7c-byte record.
 *
 * Called from FUN_000534d0 (per-frame ai_debug render dispatch), gated on
 * the byte at 0x5aca9b. */
void FUN_00052ab0(void)
{
  float position[3];
  char buf[256];
  float *up;
  char *entry;
  int offset;
  int count;

  offset = 0;
  count = 32;
  do {
    entry = (char *)(offset + *(int *)0x331f5c);
    if (entry[0xc] != 0 && entry[0xd] != 0) {
      up = *(float **)0x31fc44;
      position[0] = up[0] + *(float *)(entry + 0x28);
      position[1] = up[1] + *(float *)(entry + 0x2c);
      position[2] = up[2] + *(float *)(entry + 0x30);
      FUN_0004b220(position);
      ai_debug_describe_actor(*(int *)entry, -1, 1, buf, 0x100);
      FUN_00189cb0(1, FUN_0004b2b0(), buf, *(int *)0x2ee6d0);
      FUN_0004c560(entry);
    }
    offset += 0x1ca7c;
    count--;
  } while (count != 0);
}

/* FUN_000534d0: per-frame ai_debug render dispatch.  Gated on a byte inside
 * the structure pointed to by the global at 0x632574; when set, refreshes two
 * scratch globals, re-derives the selected encounter index from the selected
 * actor, then fans out to the individual ai_debug renderers, each behind its
 * own byte flag.
 *
 * Confirmed (XBE 0x534d0-0x5361d, 0x14e bytes): no frame at all -- no
 *   PUSH EBP, no SUB ESP, no locals, no callee-saved registers.  Pure global
 *   dispatch.  No FPU.
 * Confirmed: the gate is a pointer dereference, not a byte global --
 *   MOV EAX,[0x632574] / MOV CL,[EAX+1] / TEST CL,CL / JZ end.
 * Confirmed: 0x5ac98c is a *16-bit* store fed by a 32-bit load --
 *   MOV ECX,[0x325660] / ADD ECX,-0x14 / MOV word ptr [0x5ac98c],CX.  The
 *   truncation is in the original; do not widen it.
 * Confirmed: the counter at 0x5acab4 uses CDQ + IDIV 0x3e8, i.e. a *signed*
 *   remainder on int32.  An unsigned modulo would emit DIV and differ for a
 *   negative counter.
 * Confirmed (0x5350e-0x5351e): MOV EDX,[0x6325a4] / PUSH EAX (handle from
 *   [0x5ac9f8]) / PUSH EDX / CALL datum_get / MOV EAX,[EAX+0x34] -- cdecl,
 *   last PUSH is the first argument, so datum_get(actor_data, handle).  The
 *   +0x34 field is the actor's encounter index, matching the use in
 *   FUN_0004b7a0 / ai_debug_select_actor above.
 * Confirmed: three call sites carry arguments that the kb declarations
 *   previously typed away as (void):
 *     0x53575  PUSH EAX ([0x5ac9f4]) / CALL 0x52bb0 / ADD ESP,4   -> 1 arg
 *     0x53588  PUSH 0 / PUSH 1 / PUSH EAX ([0x5ac9f8]) /
 *              CALL 0x4c920 / ADD ESP,0xc                          -> 3 args
 *     0x535c8  MOV DL,[0x5aca67] / CALL 0x52b60                    -> DL arg
 *   The kb declarations for 0x52bb0, 0x4c920 and 0x52b60 are corrected as
 *   part of this lift; 0x52b60 gains an @<dl> byte parameter.
 * Confirmed: 0x535d3-0x535ee is a short-circuit JNZ chain into one shared
 *   call, i.e. a plain three-way ||.
 * Confirmed: the function ends in JMP 0x4bc70 at 0x53618 -- a tail call, so
 *   nothing runs after it.
 *
 * Uncertain: the meaning of the byte at 0x5aca67 passed in DL; it sits
 *   immediately after the 0x5aca66 enable flag, so it is typed as an opaque
 *   mode byte rather than given a speculative name.
 * Uncertain: the identity of 0x325660 and of the 0x14 bias applied to it.
 *
 * Called from render_debug.c (0x53... debug render pass). */
void FUN_000534d0(void)
{
  char *actor;

  if ((*(char **)0x632574)[1] == 0) {
    return;
  }

  *(int16_t *)0x5ac98c = (int16_t)(*(int32_t *)0x325660 - 0x14);
  *(int32_t *)0x5acab4 = (*(int32_t *)0x5acab4 + 1) % 1000;

  if (*(int32_t *)0x5ac9f8 != -1) {
    actor = (char *)datum_get(actor_data, *(int32_t *)0x5ac9f8);
    *(int32_t *)0x5ac9f4 = *(int32_t *)(actor + 0x34);
  }
  if (*(uint8_t *)0x5ac9c1 != 0) {
    FUN_0004b7a0();
  }
  if (*(uint8_t *)0x5aca65 == 0) {
    return;
  }

  if (*(uint8_t *)0x5aca69 != 0) {
    FUN_000494e0();
  }
  if (*(uint8_t *)0x5aca6a != 0) {
    ai_debug_render_points_and_lines();
  }
  if (*(uint8_t *)0x5aca6b != 0) {
    FUN_000495b0();
  }
  if (*(int32_t *)0x5ac9f4 != -1) {
    FUN_00052bb0(*(int32_t *)0x5ac9f4);
  }
  if (*(int32_t *)0x5ac9f8 != -1) {
    FUN_0004c920(*(int32_t *)0x5ac9f8, 1, 0);
  }
  if (*(uint8_t *)0x5ac9fc != 0) {
    FUN_0004c890();
  }
  if (*(uint8_t *)0x5aca9b != 0) {
    FUN_00052ab0();
  }
  if (*(uint8_t *)0x5aca88 != 0) {
    FUN_00049d60();
  }
  if (*(uint8_t *)0x5aca66 != 0) {
    FUN_00052b60(*(uint8_t *)0x5aca67);
  }
  if (*(uint8_t *)0x5aca89 != 0 || *(uint8_t *)0x5aca53 != 0 ||
      *(uint8_t *)0x5aca93 != 0) {
    FUN_0004b810();
  }
  if (*(uint8_t *)0x5aca76 != 0) {
    FUN_0004a770();
  }
  if (*(uint8_t *)0x5aca8c != 0) {
    FUN_0004a8c0();
  }
  if (*(uint8_t *)0x5aca91 != 0) {
    FUN_0004bc70();
  }
}

/* set_real_point3d (0x53620): zero the whole 0xeec-byte ai-debug globals
 * block at 0x5abaa0, then set the byte flag at 0x5abaa4.
 *
 * Confirmed (0x53620-0x5363b, 7 instructions, 0x1c bytes):
 *   PUSH 0xeec / PUSH 0 / PUSH 0x5abaa0 / CALL csmemset (0x8db80)
 *   / ADD ESP,0xc / MOV byte ptr [0x5abaa4],1 / RET
 * cdecl, caller cleanup (ADD ESP,0xc = 3 stack args), no frame pointer, no
 * locals, no callee-saved registers touched, no FPU.  csmemset's return value
 * (the buffer pointer) is left in EAX as an implicit side effect; the declared
 * return type is void.
 *
 * Confirmed ordering: 0x5abaa4 lies INSIDE the zeroed range
 * (0x5abaa0 + 0xeec = 0x5ac98c), so the flag store must stay after the
 * csmemset call.  The store is byte-width in the original, so it is written
 * through a uint8_t * here; a dword store would clobber 0x5abaa5..0x5abaa7.
 *
 * Inferred: this is the full-reset counterpart of FUN_00053650 below, which
 * clears only the 0xee0 tail at 0x5abaac (the 28 x 0x88 AI meter array).  This
 * function additionally clears the 12-byte header at 0x5abaa0..0x5abaab and
 * raises the flag at 0x5abaa4, i.e. an enabled/initialized marker.
 *
 * Uncertain: the kb name "set_real_point3d" is retained only because the build
 * and verification tooling keys off it; it is almost certainly a bad auto-name
 * (no point3d is involved anywhere in the body).  Sibling 0x53650 kept its
 * FUN_ name for the same reason.  Uncertain: the layout and meaning of the
 * 12-byte header, and the exact semantics of the 0x5abaa4 flag. */
void set_real_point3d(void)
{
  csmemset((void *)0x5abaa0, 0, 0xeec);
  *(uint8_t *)0x5abaa4 = 1;
}

/* FUN_00053650: zero the 0xee0-byte ai-debug globals block at 0x5abaac.
 *
 * Confirmed (0x53650-0x53664, 6 instructions, 21 bytes):
 *   PUSH 0xee0 / PUSH 0 / PUSH 0x5abaac / CALL csmemset (0x8db80) / ADD ESP,0xc
 * / RET cdecl, caller cleanup (ADD ESP,0xc = 3 stack args), no frame pointer,
 * no locals, no callee-saved registers touched.  csmemset's return value (the
 * buffer pointer) is left in EAX as an implicit side effect; the declared
 * return type is void.
 *
 * The block runs 0x5abaac..0x5ac98c, immediately below the ai_debug encounter
 * selection state at 0x5ac9d2/0x5ac9f4/0x5ac9f8.  Its element type and count
 * are unknown; the 0xee0 length is taken verbatim from the immediate.
 *
 * Uncertain: the surrounding kb names (0x53640 ai_debug_lineoffire_success,
 * 0x53670 ai_debug_lineofsight_reset) suggest a *_reset shape, but there is no
 * binary evidence for a specific name, so the FUN_ name is retained. */
void FUN_00053650(void)
{
  csmemset((void *)0x5abaac, 0, 0xee0);
}

/* FUN_00053680: per-tick update of the 28-entry AI meter array.
 *
 * TU attribution: both asserts here reference
 * "c:\halo\SOURCE\ai\ai_profile.c" (@ VA 0x25c0ac, referenced from exactly two
 * .text sites: 0x536a6 and 0x536ee, i.e. only this function).  The real
 * ai_profile.c therefore lives around 0x536xx, not in the misnamed
 * src/halo/ai/ai_profile.c (which is documented in its own header as being
 * c:\halo\SOURCE\ai\ai_script.c, 0x540b0+).  The function is kept in
 * ai_debug.obj because that is its current kb.json object membership AND
 * because delinked/ai_debug.obj already bounds it (FUN_00053680 @ +0xa730,
 * next symbol FUN_00053790 @ +0xa840), so the VC71 reference is valid here.
 * Splitting a real ai_profile.obj TU out is a separate, larger change.
 *
 * Confirmed (0x53680-0x5378f):
 *   PUSH EBP / MOV EBP,ESP / SUB ESP,8 / PUSH EBX,ESI,EDI.
 *   EBX = definition cursor, base 0x2c8e9c (biased +4 into the entry, so the
 *         table itself starts at 0x2c8e98), ADD EBX,8 per iteration.
 *   ESI = meter cursor, base 0x5abab8 (biased +0xc into the element, so the
 *         array itself starts at 0x5abaac), ADD ESI,0x88 per iteration.
 *   DI  = index, INC EDI, CMP DI,0x1c / JL -> 28 iterations, 16-bit compare.
 *   [EBP-4] = definition-cursor spill (EBX is reused as the 0x3c divisor at
 *             0x5374b), [EBP-8] = int scratch feeding FIDIV.
 *
 * The meter array is the same 0xee0-byte block that FUN_00053650 zeroes:
 * 28 * 0x88 = 0xee0, base 0x5abaac.  Element layout is taken from the
 * biased-cursor offsets in the disassembly, not from the decompiler.
 *
 * Confirmed details:
 *   - sample_fn returns 16 bits: MOV word [ESI-0xc],AX after CALL EAX.
 *   - bound check is signed 16-bit on AX: TEST AX,AX / JL ; CMP AX,0x3c / JL,
 *     i.e. 0 <= next_index < 60 with a strict upper bound.
 *   - the evicted history entry is MOVSX-widened before the subtraction:
 *     MOVSX ECX,AX ; MOVSX EDX,word [ESI+ECX*2+4] ; SUB dword [ESI-4],EDX.
 *   - count clamp: MOV CX,[ESI+2] ; INC AX ; CMP CX,AX ; JG keep-CX.
 *   - modulo is a signed IDIV by 0x3c; the remainder (DX) is stored back to
 *     history_next_index.
 *   - average: FILD dword [ESI-4] (history_sum, read AFTER the subtraction and
 *     AFTER the history store) / FIDIV dword [EBP-8] (the clamped count as an
 *     int in memory) / FSTP dword [ESI-0x90].
 *
 * Faithful oddity: history_sum is only ever DECREMENTED here (the evicted
 * sample is subtracted); the newly stored sample is never added back.  That is
 * what the binary does — do not "fix" it. */

typedef short(__cdecl *ai_meter_sample_proc)(void);

typedef struct ai_meter_definition {
  short meter_id; /* +0x00 */
  short pad_02; /* +0x02 */
  ai_meter_sample_proc sample_fn; /* +0x04 */
} ai_meter_definition; /* 0x08 */

typedef struct ai_meter {
  short accumulator; /* +0x00 */
  short current_value; /* +0x02 */
  float average; /* +0x04 */
  int history_sum; /* +0x08 */
  short history_next_index; /* +0x0c */
  short history_count; /* +0x0e */
  short history[60]; /* +0x10 */
} ai_meter; /* 0x88 */

#define AI_METER_HISTORY_TICKS 60
#define NUMBER_OF_AI_METERS 28

void FUN_00053680(void)
{
  const ai_meter_definition *definition;
  int divisor;
  ai_meter *meter;
  short index;
  short next_index;
  short count;

  index = 0;
  definition = (const ai_meter_definition *)0x2c8e98;
  meter = (ai_meter *)0x5abaac;

  do {
    if (definition->meter_id != index) {
      display_assert("definition->meter_id == index",
                     "c:\\halo\\SOURCE\\ai\\ai_profile.c", 0x8c, 1);
      system_exit(-1);
    }

    if (definition->sample_fn != 0)
      meter->accumulator = definition->sample_fn();

    meter->current_value = meter->accumulator;
    meter->accumulator = 0;

    if (meter->history_next_index < 0 ||
        meter->history_next_index >= AI_METER_HISTORY_TICKS) {
      display_assert("(meter->history_next_index >= 0) && "
                     "(meter->history_next_index < AI_METER_HISTORY_TICKS)",
                     "c:\\halo\\SOURCE\\ai\\ai_profile.c", 0x97, 1);
      system_exit(-1);
    }

    next_index = meter->history_next_index;
    if (next_index < meter->history_count)
      meter->history_sum -= meter->history[next_index];
    meter->history[next_index] = meter->current_value;

    next_index = (short)(next_index + 1);
    meter->history_next_index = next_index;

    count = meter->history_count;
    if (count <= next_index)
      count = next_index;
    meter->history_count = count;

    index = (short)(index + 1);
    definition++;

    meter->history_next_index = (short)(next_index % AI_METER_HISTORY_TICKS);
    divisor = count;
    meter->average = (float)meter->history_sum / (float)divisor;
    meter++;
  } while (index < NUMBER_OF_AI_METERS);
}

/* FUN_00053790: append the AI pool-usage summary line to an existing string.
 *
 * Formats nine int16 counters from the ai-debug globals block plus the literal
 * 768 into the text buffer, starting at its current NUL terminator:
 *   "ai enc <a>/<b>, actor <c>/<d>/<e>, unit <f>/<g>/<h>, props <i>/768"
 *
 * Confirmed from disassembly at 0x53790:
 *   - one stack parameter, MOV ESI,[EBP+8] (the destination buffer)
 *   - every counter is loaded with MOVSX EAX,word ptr [abs] -> int16, not int
 *   - PUSH ESI serves as the csstrlen argument; ADD EAX,ESI then supplies the
 *     sprintf destination, i.e. sprintf(buf + strlen(buf), ...)
 *   - the counters sit at 0x5abaae + n*0x88 (n = 0..8) inside the 0xee0-byte
 *     ai-debug globals block based at 0x5abaac
 *   - ADD ESP,0x30 after the call = 12 dwords (dest + format + 10 varargs)
 *
 * Uncertain: the individual counter fields have no assert/name evidence, so
 * they stay as raw addresses.  "|n" is byte-exact from 0x25c0d0 (Halo's
 * in-string newline escape); the format also uses the printf space flag. */
void FUN_00053790(char *buf)
{
  crt_sprintf(buf + csstrlen(buf),
              "ai enc % 2d/% 3d, actor % 3d/% 3d/% 3d, unit % 3d/% 3d/% 3d, "
              "props % 3d/% 3d|n",
              *(int16_t *)0x5abb36, *(int16_t *)0x5abaae, *(int16_t *)0x5abcce,
              *(int16_t *)0x5abc46, *(int16_t *)0x5abbbe, *(int16_t *)0x5abe66,
              *(int16_t *)0x5abdde, *(int16_t *)0x5abd56, *(int16_t *)0x5abeee,
              768);
}

/* ai line-spray mode cycler (0x53890): advance the AI debug line-spray mode
 * 0 -> 1 -> 2 -> 0 and echo the new mode's name to the console.
 *
 * Confirmed (XBE bytes at 0x53890):
 *   0FBF05 A2BA5A00  MOVSX EAX,word ptr [0x5abaa2]   ; signed int16 global
 *   40                INC EAX
 *   99                CDQ
 *   B9 03000000       MOV ECX,3
 *   F7F9              IDIV ECX                       ; signed % 3
 *   66:8915 A2BA5A00  MOV word ptr [0x5abaa2],DX     ; store the REMAINDER
 *   0FBFD2            MOVSX EDX,DX
 *   8B0495 788F2C00   MOV EAX,[EDX*4 + 0x2c8f78]     ; const char *[] table
 *   50 / 68 20C12500 / 6A 00 / E8 ...  -> console_printf(0, fmt, name)
 *   66:A1 A2BA5A00    MOV AX,[0x5abaa2]              ; 16-bit result in AX
 *   83C4 0C / C3      ADD ESP,0xc ; RET              ; cdecl, 3 dword args
 *
 * Confirmed data: 0x2c8f78 = {"none", "actions", "activation status"}
 * (read out of the XBE); 0x25c120 = "AI line-spray: %s".
 * 0x5abaa2 is the same signed int16 debug-mode selector that encounters.c
 * dispatches on, so the three names describe that dispatch's cases.
 *
 * Inferred: the trailing MOV AX makes this a short-returning function (the
 * new mode index); the sole caller (FUN_000ffe70 in main.c) discards it. */
int16_t FUN_00053890(void)
{
  *(int16_t *)0x5abaa2 = (int16_t)((*(int16_t *)0x5abaa2 + 1) % 3);
  console_printf(0, "AI line-spray: %s",
                 ((const char **)0x2c8f78)[*(int16_t *)0x5abaa2]);
  return *(int16_t *)0x5abaa2;
}

/* AI-pool word getter (0x538d0): read the AI data-array header pointed to by
 * the global at 0x5ab270 and return its 16-bit field at +0x30.
 *
 * Confirmed (XBE bytes at 0x538d0, 9 bytes, leaf, no frame, no calls):
 *   A1 70B25A00       MOV EAX,[0x005ab270]        ; load the data_t *
 *   66:8B40 30        MOV AX,word ptr [EAX+0x30]  ; 16-bit field load
 *   C3                RET                         ; result in AX (cdecl)
 *
 * Confirmed: 0x5ab270 is a data_t * — actions.c / actors.c / actor_perception.c
 * all consume it as datum_get(*(data_t **)0x5ab270, actor+0x34).  Offset 0x30
 * in data_t is the int16_t member typed as unk_48 in src/types.h.
 *
 * Inferred: the trailing 16-bit-only load makes this a short-returning getter;
 * the upper half of EAX is left holding the pointer and is not part of the
 * result.  Uncertain: the semantic meaning of data_t+0x30 (types.h leaves it
 * unnamed), so no speculative name is applied here.  The original performs no
 * NULL check on the global; none is added. */
int16_t FUN_000538d0(void)
{
  return (*(data_t **)0x5ab270)->unk_48;
}

/* AI actor/swarm population count (0x538f0): walk every active actor via the
 * standard encounter iterator and return the total number of AI entities.
 * Swarm actors (record+6 != 0) contribute their component count from the
 * int16_t at record+0x1e; ordinary actors contribute 1.
 *
 * Confirmed (XBE 0x538f0-0x5393e): frame is PUSH EBP / MOV EBP,ESP /
 *   SUB ESP,0x1c / PUSH ESI.  The single local is the 28-byte (0x1c) iterator
 *   at EBP-0x1c; ESI is the accumulator, zeroed by XOR ESI,ESI at 0x538f7.
 * Confirmed: iterator init is encounter_iterator_next(&iter, 0) -- PUSH ESI
 *   (0) then PUSH EAX (&iter) at 0x538fc/0x538fd, cdecl, so flag = 0 (all
 *   encounters), matching ai_erase's all-actors branch.
 * Confirmed: ADD ESP,0xc at 0x5390c is *coalesced* cleanup for both preceding
 *   calls (2 args + 1 arg).  The loop-back call at 0x5392b has its own
 *   ADD ESP,0x4, so FUN_00059b50 really does take exactly one argument; the
 *   ARG_COUNT hazard on this site is a false positive.
 * Confirmed: FUN_00059b50 returns the actor record pointer (kb decl types it
 *   int); it is dereferenced at +0x6 and +0x1e here, so the result is cast.
 * Confirmed: MOVSX EAX,word ptr [EAX+0x1e] at 0x5391a -- the count field is a
 *   *signed* 16-bit member, not an int.
 * Confirmed: both arms materialise the addend in EAX (MOVSX ... / MOV EAX,1)
 *   and join at 0x53925 before a single ADD ESI,EAX, i.e. a ternary rather
 *   than two separate accumulating branches.
 * Confirmed: MOV AX,SI at 0x53937 -- the result is returned 16-bit in AX, so
 *   this is int16_t-returning despite the accumulator being a full int.
 *
 * Uncertain: the semantics of record+0x1e (a cached swarm component count in
 * actors_move_randomly terms, but that path reads the count from the swarm
 * datum rather than the actor), so no name is applied to the field. */
int16_t FUN_000538f0(void)
{
  char iter[0x1c];
  char *record;
  int total;

  total = 0;
  encounter_iterator_next(iter, 0);
  record = (char *)FUN_00059b50(iter);
  while (record != NULL) {
    total += (*(char *)(record + 6) != 0) ? *(int16_t *)(record + 0x1e) : 1;
    record = (char *)FUN_00059b50(iter);
  }
  return (int16_t)total;
}

/* AI per-actor byte-field population sum (0x53960): walk every active actor
 * via the standard encounter iterator and accumulate the unsigned byte at
 * record+6, returning the 16-bit total.
 *
 * Confirmed (XBE 0x53960-0x539a1, 29 instructions): frame is PUSH EBP /
 *   MOV EBP,ESP / SUB ESP,0x1c / PUSH ESI.  The single local is the 28-byte
 *   (0x1c) iterator at EBP-0x1c; ESI is the accumulator, zeroed by XOR ESI,ESI.
 * Confirmed: iterator init is encounter_iterator_next(&iter, 0) -- PUSH ESI
 *   (still 0) then PUSH EAX (&iter), cdecl right-to-left, so flag = 0 (all
 *   encounters), matching the sibling counter at 0x538f0.
 * Confirmed: the ADD ESP,0xc after the second CALL is *coalesced* cleanup for
 *   both preceding calls (2 dwords + 1 dword).  The loop-back call site has
 *   its own ADD ESP,0x4, so FUN_00059b50 really takes exactly one argument;
 *   the ARG_COUNT hazard on the first site is a false positive.
 * Confirmed: FUN_00059b50 returns the actor record pointer (kb decl types it
 *   int); it is dereferenced at +0x6 here, so the result is cast.
 * Confirmed: MOVZX DX,byte ptr [EAX+0x6] -- the summed member is an *unsigned
 *   8-bit* field widened to 16 bits, then ADD ESI,EDX.  It is not a word.
 * Confirmed: the loop is a guarded while -- TEST EAX,EAX / JZ past the body
 *   before the first iteration, then TEST/JNZ at the bottom.
 * Confirmed: MOV AX,SI in the epilogue -- the result leaves in AX, so this is
 *   int16_t-returning and the accumulator is short-width.  The kb declaration
 *   previously typed it void, which is wrong.
 *
 * Uncertain: the semantics of record+6.  The sibling 0x538f0 tests the same
 *   byte for non-zero to select its swarm branch; here it is summed instead,
 *   so no speculative field name is applied. */
int16_t FUN_00053960(void)
{
  char iter[0x1c];
  unsigned char *record;
  short total;

  total = 0;
  encounter_iterator_next(iter, 0);
  record = (unsigned char *)FUN_00059b50(iter);
  while (record != NULL) {
    total += record[6];
    record = (unsigned char *)FUN_00059b50(iter);
  }
  return total;
}

/* 0x000539c0 - debug overlay row: encounter and t-prop pool usage
 * (FUN_000539c0).
 *
 * One of the "%d|t..." debug-overlay row family (0x539c0, 0x53a20, 0x53a90,
 * 0x53af0, 0x53b80, 0x53bf0, ...) that all format a line into the shared debug
 * scratch buffer at 0x5ab280 and hand it to the column-layout row printer
 * FUN_00053800 with an array of tab-stop x positions.  This row uses two stops
 * {150, 300} for its four fields.
 *
 * Globals (raw pointer-cast idiom, matching this TU):
 *   0x5ab280 (char[])  : shared debug sprintf scratch buffer
 *   0x5abb36 (int16)   : encounters in use
 *   0x5abaae (int16)   : encounter pool capacity
 *   0x5abeee (int16)   : t-props in use  (768 is the literal capacity)
 *   0x2ee6c4 (void *)  : row-printer context pointer, passed to FUN_00053800
 *                        in EAX.  Confirmed: MOV EAX,[0x2ee6c4] at 0x53a00,
 *                        i.e. the *contents* of the global, not its address.
 *                        FUN_00053800 itself falls back to the same load when
 *                        its EAX argument is NULL (0x5382a).
 *
 * Confirmed (XBE 0x539c0-0x53a17, 24 instructions):
 *   frame is PUSH EBP / MOV EBP,ESP / PUSH ECX -- the single 4-byte local is
 *   the 2-element int16 column array at EBP-4 (stores 0x96 at EBP-4 and 0x12c
 *   at EBP-2).  All three counters are read with MOVSX WORD PTR, i.e. they are
 *   signed 16-bit globals sign-extended to int for the varargs call; declaring
 *   them int would be a load-width bug.  The fourth conversion is the literal
 *   768 (PUSH 0x300), so the pairing is (0x5abb36 / 0x5abaae) and
 *   (0x5abeee / 768).
 *   A single ADD ESP,0x24 cleans both calls (6 dwords for the sprintf + 3 for
 *   the row printer); the call-site argument-count audit reads that merged
 *   cleanup as 9 stack args for FUN_00053800, which is a false positive.
 *
 * As in the siblings, the original schedules the two column stores between the
 * sprintf argument pushes and its CALL; they target a local, so the observable
 * order is unchanged.
 */
void FUN_000539c0(void)
{
  short column_positions[2];

  crt_sprintf((char *)0x5ab280, "encounters %d/%d|tprops %d/%d",
              (int)*(int16_t *)0x5abb36, (int)*(int16_t *)0x5abaae,
              (int)*(int16_t *)0x5abeee, 768);
  column_positions[0] = 0x96; /* 150 */
  column_positions[1] = 0x12c; /* 300 */
  FUN_00053800((char *)0x5ab280, 2, column_positions, *(void **)0x2ee6c4);
}

/* 0x00053a20 - debug overlay row: actor and unit pool usage (FUN_00053a20).
 *
 * Sibling of FUN_000539c0 in the "%d/%d" debug-overlay row family (0x539c0,
 * 0x53a20, 0x53a90, 0x53af0, ...): format one line into the shared debug
 * scratch buffer at 0x5ab280, then hand it to the column-layout row printer
 * FUN_00053800 with the same two tab stops {150, 300}.
 *
 * Globals (raw pointer-cast idiom, matching this TU):
 *   0x5ab280 (char[])  : shared debug sprintf scratch buffer
 *   0x5abcce (int16)   : actors field 1
 *   0x5abc46 (int16)   : actors field 2
 *   0x5abbbe (int16)   : actors field 3
 *   0x5abe66 (int16)   : units field 1
 *   0x5abdde (int16)   : units field 2
 *   0x5abd56 (int16)   : units field 3
 *   0x2ee6c4 (void *)  : row-printer context pointer, passed to FUN_00053800
 *                        in EAX -- the *contents* of the global, not its
 *                        address.
 *
 * Confirmed (XBE 0x53a20-0x53a8x):
 *   frame is PUSH EBP / MOV EBP,ESP / PUSH ECX -- the single 4-byte local is
 *   the 2-element int16 column array at EBP-4 (MOV WORD PTR [EBP-4],0x96 at
 *   0x53a5e and MOV WORD PTR [EBP-2],0x12c at 0x53a64; 16-bit stores, so the
 *   array is short[2], not int[2]).  All six counters are read with
 *   MOVSX r32,WORD PTR [abs], i.e. signed 16-bit globals sign-extended to int
 *   for the varargs call; declaring them int would be a load-width bug.
 *   The six sprintf data pushes trace back, each to its immediately preceding
 *   MOVSX, as 0x5abcce / 0x5abc46 / 0x5abbbe / 0x5abe66 / 0x5abdde / 0x5abd56
 *   in C argument order (EAX/ECX/EDX are reused across the interleaved
 *   schedule, so the pushes must be traced individually).
 *   At the second call the LEA EAX,[EBP-4] / PUSH EAX happens first and EAX is
 *   then reloaded from [0x2ee6c4] for the register argument.
 *   A single ADD ESP,0x2c cleans both calls (8 dwords for the sprintf + 3 for
 *   the row printer); the call-site argument-count audit reads that merged
 *   cleanup as 11 stack args for FUN_00053800, which is a false positive.
 *   Ghidra additionally prints FUN_00053800 with zero arguments and drops both
 *   column stores; the four arguments above come from the disassembly.
 *
 * The format string is transcribed verbatim from 0x25c154, including the
 * missing '/' between the last two unit fields ("%d/%d%d").
 *
 * As in the siblings, the original schedules the two column stores between the
 * sprintf argument pushes and its CALL; they target a local, so the observable
 * order is unchanged.
 */
void FUN_00053a20(void)
{
  short column_positions[2];

  crt_sprintf((char *)0x5ab280, "actors %d/%d/%d|units %d/%d%d",
              (int)*(int16_t *)0x5abcce, (int)*(int16_t *)0x5abc46,
              (int)*(int16_t *)0x5abbbe, (int)*(int16_t *)0x5abe66,
              (int)*(int16_t *)0x5abdde, (int)*(int16_t *)0x5abd56);
  column_positions[0] = 0x96; /* 150 */
  column_positions[1] = 0x12c; /* 300 */
  FUN_00053800((char *)0x5ab280, 2, column_positions, *(void **)0x2ee6c4);
}

/* 0x00053a90 - debug overlay row: swarm and t-component pool usage
 * (FUN_00053a90).
 *
 * Next sibling of FUN_00053a20 in the "%d/%d" debug-overlay row family
 * (0x539c0, 0x53a20, 0x53a90, 0x53af0, ...): format one line into the shared
 * debug scratch buffer at 0x5ab280, then hand it to the column-layout row
 * printer FUN_00053800.  Unlike the two-column siblings this row uses a single
 * tab stop {150}.
 *
 * Globals (raw pointer-cast idiom, matching this TU):
 *   0x5ab280 (char[])  : shared debug sprintf scratch buffer
 *   0x5ac4c6 (int16)   : swarms field 1
 *   0x5ac43e (int16)   : swarms field 2
 *   0x5ac54e (int16)   : t-components field 1
 *   0x2ee6c4 (void *)  : row-printer context pointer, passed to FUN_00053800
 *                        in EAX -- the *contents* of the global, not its
 *                        address.
 *
 * The third swarm field (32) and the second t-component field (256) are not
 * globals at all: they are PUSH 0x20 / PUSH 0x100 immediates, i.e. the
 * compile-time pool capacities baked into the format's denominators.
 *
 * Confirmed (XBE 0x53a90-0x53ae4, 24 instructions):
 *   frame is PUSH EBP / MOV EBP,ESP / PUSH ECX -- the single 4-byte local is
 *   the 1-element int16 column array at EBP-4 (MOV WORD PTR [EBP-4],0x96 at
 *   0x53abd; a 16-bit store, so the array is short[], not int[]).
 *   All three counters are read with MOVSX r32,WORD PTR [abs], i.e. signed
 *   16-bit globals sign-extended to int for the varargs call; declaring them
 *   int would be a load-width bug.  The three MOVSX are hoisted ahead of the
 *   push block by the MSVC scheduler (EDX <- 0x5ac4c6, ECX <- 0x5ac43e,
 *   EAX <- 0x5ac54e), so the pushes must be traced individually rather than to
 *   the nearest preceding load; in C argument order they are 0x5ac4c6 /
 *   0x5ac43e / 32 / 0x5ac54e / 256.
 *   At the second call (0x53ad8) the LEA EAX,[EBP-4] / PUSH EAX happens first
 *   and EAX is then reloaded from [0x2ee6c4] for the register argument, in the
 *   middle of the remaining pushes.
 *   A single ADD ESP,0x28 cleans both calls (7 dwords for the sprintf + 3 for
 *   the row printer); the call-site argument-count audit reads that merged
 *   cleanup as 10 stack args for FUN_00053800, which is a false positive --
 *   the declared 3 stack args + 1 @<eax> register arg is correct.
 *   Ghidra additionally prints FUN_00053800 with zero arguments and drops the
 *   column store; the four arguments above come from the disassembly.
 *
 * The format string is transcribed verbatim from 0x25c174.
 *
 * As in the siblings, the original schedules the column store between the
 * sprintf argument pushes and its CALL; it targets a local, so the observable
 * order is unchanged.
 */
void FUN_00053a90(void)
{
  short column_positions[1];

  crt_sprintf((char *)0x5ab280, "swarms %d/%d/%d|tcomponents %d/%d",
              (int)*(int16_t *)0x5ac4c6, (int)*(int16_t *)0x5ac43e, 32,
              (int)*(int16_t *)0x5ac54e, 256);
  column_positions[0] = 0x96; /* 150 */
  FUN_00053800((char *)0x5ab280, 1, column_positions, *(void **)0x2ee6c4);
}

/* 0x00053af0 - debug overlay row: t-prop usage by allegiance
 * (FUN_00053af0).
 *
 * Next sibling of FUN_00053a90 in the "%d/%d" debug-overlay row family
 * (0x539c0, 0x53a20, 0x53a90, 0x53af0, ...): format one line into the shared
 * debug scratch buffer at 0x5ab280, then hand it to the column-layout row
 * printer FUN_00053800.  This is the widest row in the family: nine counters
 * grouped enemy / friend / dead, laid out over three tab stops
 * {150, 300, 450}.
 *
 * Globals (raw pointer-cast idiom, matching this TU):
 *   0x5ab280 (char[])  : shared debug sprintf scratch buffer
 *   0x5ac10e (int16)   : enemy field 1     (a)
 *   0x5ac196 (int16)   : enemy field 2     (o)
 *   0x5ac21e (int16)   : enemy field 3     (u)
 *   0x5ac2a6 (int16)   : friend field 1    (a)
 *   0x5ac32e (int16)   : friend field 2    (o)
 *   0x5ac3b6 (int16)   : friend field 3    (u)
 *   0x5abf76 (int16)   : dead field 1      (a)
 *   0x5abffe (int16)   : dead field 2      (o)
 *   0x5ac086 (int16)   : dead field 3      (u)
 *   0x2ee6c4 (void *)  : row-printer context pointer, passed to FUN_00053800
 *                        in EAX -- the *contents* of the global, not its
 *                        address.
 *
 * Confirmed (XBE 0x53af0-, disassembly):
 *   frame is PUSH EBP / MOV EBP,ESP / SUB ESP,0x8 -- the two-dword frame holds
 *   the 3-element int16 column array at EBP-8..EBP-3 (MOV WORD PTR [EBP-8],
 *   0x96 / [EBP-6],0x12c / [EBP-4],0x1c2; 16-bit stores, so the array is
 *   short[], not int[]; only 6 of the 8 frame bytes are used).
 *   All nine counters are read with MOVSX r32,WORD PTR [abs], i.e. signed
 *   16-bit globals sign-extended to int for the varargs call; declaring them
 *   int would be a load-width bug.  The MOVSX loads are interleaved three at a
 *   time with the pushes (EAX/ECX/EDX round-robin) by the MSVC scheduler, so
 *   the pushes must be traced individually rather than to the nearest
 *   preceding load; in C argument order they are 0x5ac10e / 0x5ac196 /
 *   0x5ac21e / 0x5ac2a6 / 0x5ac32e / 0x5ac3b6 / 0x5abf76 / 0x5abffe /
 *   0x5ac086.
 *   At the second call the LEA EAX,[EBP-8] / PUSH EAX happens first and EAX is
 *   then reloaded from [0x2ee6c4] for the register argument, in the middle of
 *   the remaining pushes.
 *   A single ADD ESP,0x38 cleans both calls (11 dwords for the sprintf + 3 for
 *   the row printer); the call-site argument-count audit reads that merged
 *   cleanup as 14 stack args for FUN_00053800, which is a false positive --
 *   the declared 3 stack args + 1 @<eax> register arg is correct.
 *   Ghidra additionally prints FUN_00053800 with zero arguments and drops the
 *   column stores; the four arguments above come from the disassembly.
 *
 * The format string is transcribed verbatim from 0x25c198: the XBE bytes are
 * literally '|' followed by 't', not an escaped tab, matching every other row
 * in this family.
 *
 * As in the siblings, the original schedules the column stores between the
 * sprintf argument pushes and its CALL; they target a local, so the observable
 * order is unchanged.
 */
void FUN_00053af0(void)
{
  short column_positions[3];

  crt_sprintf((char *)0x5ab280,
              "props a/o/u:|tenemy %d/%d/%d|tfriend %d/%d/%d|tdead %d/%d/%d",
              (int)*(int16_t *)0x5ac10e, (int)*(int16_t *)0x5ac196,
              (int)*(int16_t *)0x5ac21e, (int)*(int16_t *)0x5ac2a6,
              (int)*(int16_t *)0x5ac32e, (int)*(int16_t *)0x5ac3b6,
              (int)*(int16_t *)0x5abf76, (int)*(int16_t *)0x5abffe,
              (int)*(int16_t *)0x5ac086);
  column_positions[0] = 0x96; /* 150 */
  column_positions[1] = 0x12c; /* 300 */
  column_positions[2] = 0x1c2; /* 450 */
  FUN_00053800((char *)0x5ab280, 3, column_positions, *(void **)0x2ee6c4);
}
