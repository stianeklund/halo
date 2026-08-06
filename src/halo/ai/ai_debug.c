
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
