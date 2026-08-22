/* Stack walker and linker map symbol table loader for Xbox.
 *
 * Walks the x86 EBP chain to collect return addresses, resolves them
 * against a parsed MSVC linker .map file, and dumps the stack trace
 * to a file or the error log.
 *
 * The map file path is hardcoded in stack_walk_initialize() — change it there
 * to control which .map file is loaded (e.g. when running off HDD). */

/* Globals in the stack-walk/profiler data region.  The first three are real
 * data symbols in kb.json (stack_walk_windows.obj) rather than absolute pointer
 * casts: MSVC models a store through a constant-address cast as a store through
 * an unknown pointer that may alias the stack, so it will not schedule an
 * argument PUSH across one.  With declared globals it hoists the PUSH the way
 * the reference does -- +16.7pp on stack_walk_dispose, and +10pp operand-level
 * on stack_walk_initialize.
 *
 *   0x2ee780: int32_t    stack_walk_bias        — map RVA -> runtime VA bias
 *   0x2ee784: uint8_t    stack_walk_load_failed — 1 if map load failed
 *   0x2ee788: int32_t[3] stack_walk_symbols     — {count, names, entries}
 *   0x449ef8: uint32_t * g_sw_frame             — current frame ptr in walk
 *   0x449efc: uint32_t * g_sw_base              — stack base (floor) in walk */

/* Cross-TU helpers in profile.obj (called by name; kb.json decls drive
 * the halo.xbe.def export names that the linker resolves against). */

/* Parse a hex digit; returns 0-15 or -1 on non-hex. */
static int hex_digit(int c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/* Parse a hex string (no "0x" prefix) into a uint32_t. */
static uint32_t parse_hex(const char *s)
{
  uint32_t val;
  int d;

  val = 0;
  while (*s && (d = hex_digit((int)(unsigned char)*s)) >= 0) {
    val = (val << 4) | (uint32_t)d;
    s++;
  }
  return val;
}

/* -----------------------------------------------------------------------
 * FUN_00092370 (0x92370) — walk EBP chain and collect return addresses.
 *
 * __fastcall: ECX = skip (frames to skip), EDX = frames[] output array.
 * Additional args on stack: max = capacity of frames[], count = out ptr.
 *
 * Captures the caller's EBP via __builtin_frame_address(0), saves it to
 * the profiler globals so walk_ebp_chain can bound the walk, then
 * delegates to the profile.obj helper at 0x922a0.
 * ----------------------------------------------------------------------- */
void __fastcall FUN_00092370(int skip, int32_t *frames, uint32_t max,
                             uint32_t *count)
{
  uint32_t *frame;
  uint32_t *base;

  frame = (uint32_t *)__builtin_frame_address(0);
  base = *(uint32_t **)0x449efc;

  *(uint32_t **)0x449ef8 = frame;

  /* skip one extra to hide this wrapper frame */
  FUN_000922a0(skip + 1, frames, max, count);

  *(uint32_t **)0x449ef8 = (uint32_t *)base;
}

/* -----------------------------------------------------------------------
 * stack_walk_dispose (0x92440) — reset the symbol table to "not loaded" state.
 *
 * The reference is 29 bytes and CALLs symbol_table_dispose (0x92090) with
 * &symtab; MSVC hoists the `PUSH 0x2ee788` above the two global stores and
 * cleans with `POP ECX`.  The previous lift open-coded the three zero stores
 * that symbol_table_dispose ends with and dropped the call, which LEAKED both
 * of the symbol table's heap allocations: 0x92090 debug_free()s symtab[1]
 * (line 0x227) and symtab[2] (line 0x228) when non-NULL before zeroing.
 * Found by the SHAPE-WARN call-count census (ours 0 vs reference 1) -- see
 * docs/lift-learnings.md section 49.
 *
 * Reference instruction order is `push 0x2ee788` FIRST, then the two global
 * stores, then the call.  Reaching the globals through absolute pointer casts
 * blocked that hoist and cost 1 of 6 LCS positions (83.3%); the real data
 * symbols restore it (100.0%).
 *
 * Reference instruction order is `push 0x2ee788` FIRST, then the two global
 * stores, then the call; VC71 emits the push after the stores from this
 * source form, which costs 1 of 6 LCS positions (83.3%).
 * ----------------------------------------------------------------------- */
void stack_walk_dispose(void)
{
  stack_walk_bias = -1;
  stack_walk_load_failed = 0;
  symbol_table_dispose(stack_walk_symbols);
}

/* -----------------------------------------------------------------------
 * stack_walk_with_context (0x92460) — dump stack trace to the error log.
 *
 * Collects up to 0x40 frames via FUN_00092370 (skipping `depth` frames),
 * resolves each return address via the profile.obj lookup helper, and
 * writes them to the error/debug output.
 * ----------------------------------------------------------------------- */
char stack_walk_with_context(int a1, int16_t depth, int a3)
{
  int32_t frames[0x40];
  uint32_t count;
  uint32_t i;
  int32_t *symtab;
  char *sym;
  int32_t offset;

  count = 0;
  FUN_00092370((int)depth, frames, 0x40, &count);

  symtab = stack_walk_symbols;
  offset = stack_walk_bias;

  for (i = 0; i < count; i++) {
    sym = NULL;
    if (offset != -1) {
      sym = FUN_00092110(frames[i] - offset, symtab);
    }
    if (sym != NULL) {
      error(2, "  [%d] 0x%08x  %s", (int)i, (unsigned int)frames[i], sym);
    } else {
      error(2, "  [%d] 0x%08x", (int)i, (unsigned int)frames[i]);
    }
  }

  return 1;
}

/* -----------------------------------------------------------------------
 * load_symbol_table (0x92710) — parse an MSVC linker .map file.
 *
 * Reads the map file, locates the "_load_symbol_table" reference symbol
 * to compute the RVA->VA bias, then builds a sorted in-memory symbol
 * table via the profile.obj helper.
 *
 * Returns 1 on success, 0 on failure. Static buffers are safe because
 * this function is called once at startup on single-threaded Xbox.
 *
 * KNOWN INCOMPLETE (n_r=468 vs our 243 insns, 29.8% VC71): the third
 * parameter `build_timestamp` is declared to match the reference call site at
 * 0x92d30 (which pushes 0x268e30 = "Thu Aug 23 16:11:48 2001") but is not yet
 * consumed here.  The original almost certainly validates the .map file's
 * "Timestamp is ..." header line against it and bails on a mismatch; this
 * implementation instead derives the RVA->VA bias by scanning the map for
 * "_load_symbol_table".  Recovering the real body needs a first-pass RE pass
 * on 0x92710, not byte-accuracy levers.
 * ----------------------------------------------------------------------- */
int load_symbol_table(const char *map_path, int32_t *symtab_out,
                      const char *build_timestamp)
{
  void *f;
  char line[0x100];
  static char name_pool[0x4000];
  static char obj_pool[0x4000];
  static int32_t entries[0x800]; /* 0x200 symbols * 4 int32 each */
  int32_t count;
  int name_pool_pos;
  int obj_pool_pos;
  int in_entry_section;
  char *seg_tok;
  char *tok;
  char *off_tok;
  char *rva_tok;
  char *name_tok;
  char *obj_tok;
  uint32_t rva;
  int32_t bias;
  int found_ref;

  f = crt_fopen(map_path, "rt");
  if (f == NULL) {
    error(2, "Couldn't read map file '%s'", map_path);
    return 0;
  }

  count = 0;
  name_pool_pos = 0;
  obj_pool_pos = 0;
  in_entry_section = 0;
  bias = 0;
  found_ref = 0;

  while (crt_fgets(line, (int)sizeof(line), f) != NULL) {
    int len;
    char *p;

    len = csstrlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' ||
                       line[len - 1] == ' ' || line[len - 1] == '\t')) {
      line[--len] = '\0';
    }
    p = line;
    while (*p == ' ' || *p == '\t')
      p++;

    if (!in_entry_section) {
      if (crt_strstr(p, "entry point at") != NULL)
        in_entry_section = 1;
      continue;
    }

    if (*p == '\0')
      continue;
    /* Skip "Static symbols" section header */
    if (crt_strstr(p, "Static symbols") == p)
      continue;

    /* Parse "SSSS:OOOOOOOO  name  RRRRRRRR  obj" */
    seg_tok = crt_strtok(p, " \t");
    if (seg_tok == NULL)
      continue;

    tok = crt_strchr(seg_tok, ':');
    if (tok == NULL)
      continue;
    *tok = '\0';
    off_tok = tok + 1;

    rva_tok = crt_strtok(NULL, " \t");
    name_tok = crt_strtok(NULL, " \t");
    obj_tok = crt_strtok(NULL, " \t");

    if (rva_tok == NULL || name_tok == NULL)
      continue;

    rva = parse_hex(rva_tok);
    if (rva == 0)
      continue;

    /* Use "_load_symbol_table" as reference to compute the VA bias */
    if (!found_ref && csstrcmp(name_tok, "_load_symbol_table") == 0) {
      bias = (int32_t)rva - 0x92710;
      found_ref = 1;
    }

    if (count >= 0x200)
      continue;

    {
      int name_len;
      int obj_len;
      int base_idx;

      name_len = csstrlen(name_tok) + 1;
      obj_len = obj_tok != NULL ? csstrlen(obj_tok) + 1 : 1;

      if (name_pool_pos + name_len > (int)sizeof(name_pool))
        continue;
      if (obj_pool_pos + obj_len > (int)sizeof(obj_pool))
        continue;

      base_idx = count * 4;
      entries[base_idx + 0] = (int32_t)parse_hex(off_tok);
      entries[base_idx + 1] = (int32_t)rva;
      entries[base_idx + 2] = name_pool_pos;
      entries[base_idx + 3] = obj_pool_pos;

      csmemcpy(name_pool + name_pool_pos, name_tok, (size_t)name_len);
      name_pool_pos += name_len;

      if (obj_tok != NULL) {
        csmemcpy(obj_pool + obj_pool_pos, obj_tok, (size_t)obj_len);
      } else {
        obj_pool[obj_pool_pos] = '\0';
      }
      obj_pool_pos += obj_len;
      count++;
    }
  }

  crt_fclose(f);

  if (count == 0 || !found_ref)
    return 0;

  symtab_out[0] = count;
  symtab_out[1] = (int32_t)name_pool;
  symtab_out[2] = (int32_t)entries;
  symbol_table_dispose(symtab_out);

  stack_walk_bias = bias;
  return 1;
}

/* -----------------------------------------------------------------------
 * stack_walk_initialize (0x92d30) — initialize the stack walker.
 *
 * Loads the linker map from the hardcoded path, then normalizes the "not
 * loaded" sentinel: if the bias global is still -1 after the load attempt it
 * is forced to 0 (symbol resolution disabled but not treated as an error).
 *
 * The reference discards load_symbol_table's return value and never calls
 * stack_walk_dispose or sets g_sw_failed -- an earlier lift did both, which
 * cost 4 of 10 LCS positions.  Verified against the 10-insn reference at
 * 0x92d30..0x92d5c; the three pushes are "d:\\cachebeta.map" (0x268e1c),
 * &g_sw_symtab (0x2ee788) and the build timestamp (0x268e30).
 * Change "d:\\cachebeta.map" to load from HDD or a custom location.
 * ----------------------------------------------------------------------- */
void stack_walk_initialize(void)
{
  load_symbol_table("d:\\cachebeta.map", stack_walk_symbols,
                    "Thu Aug 23 16:11:48 2001");
  if (stack_walk_bias == -1)
    stack_walk_bias = 0;
}

/* Invoke the stack trace logger. The depth parameter is incremented
 * by 1 to skip this wrapper's own frame. */
char stack_walk(int16_t a1)
{
  return stack_walk_with_context(0, (int)a1 + 1, 0);
}