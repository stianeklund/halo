/* Forwarding wrapper (0x17cd60).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x163590 -- the frame is torn down
 * before the jump, so 0x163590 inherits this function's stack arguments and
 * reads the incoming dword as its own [EBP+8].  Semantics of the argument are
 * unknown; it is forwarded unchanged. */
void FUN_0017cd60(int object_handle)
{
  FUN_00163590(object_handle);
}

/* Forwarding wrapper (0x17cd70).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x163910 -- the frame is torn down
 * before the jump, so 0x163910 inherits this function's stack arguments and
 * reads them as its own [EBP+8] .. [EBP+0x1c] (six dwords).  Semantics of the
 * arguments are unknown; they are forwarded unchanged.  This wrapper is
 * reached only through a data (function-table) reference at 0x195ff3. */
void FUN_0017cd70(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
  FUN_00163910((void *)arg1, arg2, arg3, arg4, arg5, (void *)arg6);
}

/* Forwarding thunks 0x17cd80..0x17d050.
 *
 * These sit in the 16-byte-spaced table that spans 0x17cd60..0x17d070.  Each
 * slot in the table holds one of two shapes: the four-instruction
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP form already lifted above (0x17cd60,
 * 0x17cd70, 0x17cdb0, ...), or the bare one-instruction E9 form lifted here.
 * Every thunk below is the bare form: a lone JMP rel32 occupying the first 5
 * bytes of its slot, with the remaining 11 bytes NOP (0x90) padding.  There is
 * no prologue and no frame, so the target runs directly on the caller's frame.
 *
 * All nineteen targets are void(void) -- verified by disassembling each target
 * out of the raw XBE: none reads [EBP+8] or higher, and the lone [ESP+4] in
 * 0x166400 is a local inside its own SUB ESP,8 frame.  With no arguments to
 * forward, each lift is a plain call to the target and clang reproduces the
 * reference's tail transfer rather than emitting CALL/RET.
 *
 * None of the thunks carries string or PDB evidence of its own name.  Naming a
 * thunk after its target would leave two semantic names for one behaviour once
 * the target itself is named, so the mechanical FUN_ names are kept. */

/* 0x17cd80: bare JMP 0x160970.  Reached by a single CALL at 0x196004. */
void FUN_0017cd80(void)
{
  _rasterizer_hud_end();
}

/* 0x17cd90: bare JMP 0x160980.  Reached by a single CALL at 0x13a72f. */
void FUN_0017cd90(void)
{
  FUN_00160980();
}

/* 0x17cda0: bare JMP 0x163c40.  Reached by a single CALL at 0x195c6e. */
void FUN_0017cda0(void)
{
  FUN_00163c40();
}

/* Forwarding wrapper (0x17cdb0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x163fe0 -- the frame is torn down
 * before the jump, so 0x163fe0 inherits this function's stack arguments and
 * reads the incoming dword as its own [ESP+4] pointer (Ghidra
 * `in_stack_00000004`, passed on to 0x155c20).  Semantics of the pointer are
 * unknown; it is forwarded unchanged.  Reached only through a data
 * (function-table) reference at 0x195c8d. */
void FUN_0017cdb0(void *param_1)
{
  FUN_00163fe0(param_1);
}

/* Forwarding wrapper (0x17cdc0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x1640d0 -- the frame is torn down
 * before the jump, so 0x1640d0 inherits this function's stack arguments and
 * reads them as its own [EBP+8] .. [EBP+0x1c] (six dwords).  Semantics of the
 * arguments are unknown; they are forwarded unchanged.  Reached only through a
 * data (function-table) reference at 0x195c88 (in FUN_00195c40). */
void FUN_0017cdc0(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
  FUN_001640d0((void *)arg1, arg2, arg3, arg4, arg5, (void *)arg6);
}

/* 0x17cde0: bare JMP 0x1609a0.  Reached by a single CALL at 0x195ca1. */
void FUN_0017cde0(void)
{
  _rasterizer_dynamic_lit_geometry_draw();
}

/* 0x17cdf0: bare JMP 0x1643e0.  Reached by a single CALL at 0x195cb9. */
void FUN_0017cdf0(void)
{
  FUN_001643e0();
}

/* Forwarding wrapper (0x17ce00).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x164590 -- the frame is torn down
 * before the jump, so 0x164590 inherits this function's stack arguments and
 * reads the incoming dword as its own [EBP+8].  This wrapper never touches the
 * argument slot itself, so the semantics of the dword are unknown here; it is
 * forwarded unchanged.  Reached only through a data (function-table) reference
 * at 0x195cd8 (in FUN_00195cb0). */
void FUN_0017ce00(int arg1)
{
  FUN_00164590((void *)arg1);
}

/* Forwarding wrapper (0x17ce10).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x1609b0 -- the frame is torn down
 * before the jump, so 0x1609b0 inherits this function's stack arguments and
 * reads them as its own [EBP+8] .. [EBP+0x1c] (six dwords).  This wrapper
 * never touches the argument slots itself, so their semantics are unknown
 * here; they are forwarded unchanged.  Reached only through a data
 * (function-table) reference at 0x195cd3 (in FUN_00195cb0). */
void FUN_0017ce10(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
  FUN_001609b0((void *)arg1, arg2, arg3, arg4, arg5, (void *)arg6);
}

/* 0x17ce30: bare JMP 0x160bc0.  Reached by a single tail JMP at 0x195cec. */
void FUN_0017ce30(void)
{
  FUN_00160bc0();
}

/* 0x17ce40: bare JMP 0x160bd0.  Reached by a single CALL at 0x195d09. */
void FUN_0017ce40(void)
{
  FUN_00160bd0();
}

/* Forwarding wrapper (0x17ce50).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x164690 -- the frame is torn down
 * before the jump, so 0x164690 inherits this function's stack arguments and
 * reads them as its own [EBP+8] .. [EBP+0x1c] (six dwords, confirmed by
 * disassembling 0x164690 in the pristine XBE).  This wrapper never touches the
 * argument slots itself, so their semantics are unknown here; they are
 * forwarded unchanged.  Reached only through a data (function-table) reference
 * at 0x195d20 (in FUN_00195d00). */
void FUN_0017ce50(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
  FUN_00164690((void *)arg1, arg2, arg3, arg4, arg5, (void *)arg6);
}

/* 0x17ce60: bare JMP 0x160be0.  Reached by a single tail JMP at 0x195d36. */
void FUN_0017ce60(void)
{
  FUN_00160be0();
}

/* 0x17ce70: bare JMP 0x160bf0.  Reached by a single CALL at 0x195d68. */
void FUN_0017ce70(void)
{
  FUN_00160bf0();
}

/* Forwarding wrapper (0x17ce80).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x164cf0 -- the frame is torn down
 * before the jump, so 0x164cf0 inherits this function's stack arguments.
 * Disassembling/decompiling 0x164cf0 in the pristine XBE shows it reads its
 * incoming stack slots at +0x04, +0x08, +0x14 and +0x18, so six dwords are
 * forwarded (the +0x0c and +0x10 slots are never read there, but they must
 * still exist to place the later ones).  The asserts inside 0x164cf0 name the
 * +0x04 slot "shader" and the +0x18 slot "vertex_buffer"
 * (c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_environment.c:0x9a6 and
 * :0x9bf); the remaining slots have unknown meaning here and are forwarded
 * unchanged.  Reached only through a data (function-table) reference at
 * 0x195d7f (in FUN_00195d40). */
void FUN_0017ce80(void *shader, int arg2, int arg3, int arg4, int arg5,
                  void *vertex_buffer)
{
  FUN_00164cf0(shader, arg2, arg3, arg4, arg5, vertex_buffer);
}

/* 0x17ce90: bare JMP 0x160c00.  Reached by a single CALL at 0x195d95. */
void FUN_0017ce90(void)
{
  FUN_00160c00();
}

/* 0x17cea0: bare JMP 0x160c10.  Reached by a single CALL at 0x195de8. */
void FUN_0017cea0(void)
{
  FUN_00160c10();
}

/* Forwarding wrapper (0x17ceb0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x165420 -- the frame is torn down
 * before the jump, so 0x165420 inherits this function's stack arguments.
 * Disassembling 0x165420 in the pristine XBE (bounds 0x165420..0x165774) shows
 * it reads its incoming stack slots at +0x04 .. +0x30 relative to its own
 * argument base, i.e. twelve dwords; the +0x28 slot (arg10 here) is never
 * observed accessed, but it must still exist to place the later ones.  Note
 * 0x165420 also WRITES its last slot (`or dword ptr [ebp+0x34],1` / `,7`), so
 * the forward must stay a tail call for the mutation to land in the real
 * caller's frame.  Slot semantics are unknown here and are forwarded
 * unchanged; the two pointer types are the only proven facts (0x165420 reads a
 * word at +0x24 of the first slot's pointer, and three floats at +0x0/+0x4/+0x8
 * of the eighth slot's pointer).  Reached only through a data (function-table)
 * reference at 0x195df2 (in FUN_00195dc0). */
void FUN_0017ceb0(void *arg1, int arg2, int arg3, int arg4, int arg5, int arg6,
                  int arg7, void *arg8, int arg9, int arg10, int arg11,
                  int arg12)
{
  FUN_00165420(arg1, arg2, arg3, arg4, arg5, arg6, arg7, (float *)arg8,
               (uint32_t *)arg9, arg10, arg11, arg12);
}

/* 0x17cec0: bare JMP 0x160c20.  Reached by a single CALL at 0x195e15. */
void FUN_0017cec0(void)
{
  FUN_00160c20();
}

/* 0x17ced0: bare JMP 0x166400.  Reached by a single CALL at 0x195e68. */
void FUN_0017ced0(void)
{
  FUN_00166400();
}

/* Forwarding wrapper (0x17cee0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x165cb0 -- the frame is torn down
 * before the jump, so 0x165cb0 inherits this function's stack arguments.
 * Disassembling 0x165cb0 in the pristine XBE (bounds 0x165cb0..0x165dbd)
 * shows it reads its incoming slots at +0x08, +0x10, +0x14, +0x18 and +0x1c
 * relative to its own EBP, i.e. six dwords; the +0x0c slot (arg2 here) is
 * never observed accessed, but it must still exist to place the later ones.
 * 0x165cb0 writes nothing back into its argument slots, so a plain call is
 * sufficient.  Its asserts name the +0x08 slot "shader" and the +0x1c slot
 * "vertex_buffer" (c:\halo\SOURCE\rasterizer\xbox\
 * rasterizer_xbox_environment_fog.c:0x1ae and :0x1b3).  Only the
 * vertex_buffer slot is proven to be a pointer (0x165cb0 reads a word at its
 * target); the shader slot is only forwarded onward, and the remaining slots
 * have unknown meaning here and are forwarded unchanged.  Reached through a
 * data (function-table) reference used as a surface-draw callback. */
void FUN_0017cee0(void *shader, int arg2, int arg3, int arg4, int arg5,
                  void *vertex_buffer)
{
  FUN_00165cb0(shader, arg2, arg3, arg4, arg5, vertex_buffer);
}

/* 0x17cef0: bare JMP 0x165dd0.  Reached by a single CALL at 0x195e95. */
void FUN_0017cef0(void)
{
  FUN_00165dd0();
}

/* Forwarding wrapper (0x17cf00).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x165de0 -- the frame is torn down
 * before the jump, so 0x165de0 inherits this function's stack arguments.
 * Disassembling 0x165de0 in the pristine XBE (bounds 0x165de0..0x165e68)
 * shows it reads three incoming slots relative to its own EBP: +0x08 as a
 * signed word (MOVSX ESI,AX; IMUL ESI,0x4c; ADD ESI,0x47ddfc -- a table index,
 * asserted >= 0 and < 4), +0x0c as a float (FMUL DWORD PTR [EBP+0xc]), and
 * +0x10 as a non-NULL pointer that receives three dwords ([EDI], [EDI+4] and
 * a literal 0 at [EDI+8]).  The sole caller, at 0x166a6e inside
 * FUN_00166890, pushes them in that order (PUSH EAX = LEA [EBP-0x30] out
 * pointer, PUSH ECX = dword loaded from 0x5a5e1c, PUSH EDX = word loaded from
 * 0x5a5bc2 zero-extended).  The meaning of the index and the scalar is
 * unknown here; they are forwarded unchanged.  The narrowing cast reflects
 * only the declared width of the callee at 0x165de0. */
void FUN_0017cf00(int param_1, float param_2, float *param_3)
{
  FUN_00165de0((int16_t)param_1, param_2, param_3);
}

/* Forwarding wrapper (0x17cf10).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x166890 -- the frame is torn down
 * before the jump, so 0x166890 inherits this function's stack arguments and
 * reads the incoming slot as its own [EBP+8].  0x166890 loads it narrow --
 * `MOV DI, WORD PTR [EBP+8]` at 0x16689a -- and immediately asserts
 * "pass==0 || pass==1" (rasterizer_xbox_environment_fog.c:499), so the dword
 * is a 0/1 pass index; the narrowing cast reflects the callee's declared
 * width.  Reached by two calls from FUN_00195ec0 (0x195ecb, 0x195efc). */
void FUN_0017cf10(int pass_index)
{
  FUN_00166890((int16_t)pass_index);
}

/* Forwarding wrapper (0x17cf20).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x1677d0 -- the frame is torn down
 * before the jump, so 0x1677d0 inherits this function's stack arguments.
 * 0x1677d0 (rasterizer_xbox_environment_fog.c, asserts at :0x3dc/:0x3e0/
 * :0x3e4/:0x3e5) reads its incoming slots at +0x04, +0x14 and +0x18 relative
 * to its own argument base, i.e. six dwords; the +0x08/+0x0c/+0x10 slots
 * (arg2..arg4 here) are never observed accessed, but they must still exist to
 * place the later ones.  Two slots are proven pointers: the +0x04 slot is
 * passed to shader_get_vertex_shader_permutation, and the +0x18 slot is
 * dereferenced as a word (`*(ushort *)slot6`) to feed FUN_00178b40.  The
 * +0x14 slot is an int accumulated into a frame-statistics counter.  Slot
 * semantics beyond that are unknown here and are forwarded unchanged.
 * 0x1677d0 writes nothing back into its argument slots, so a plain call is
 * sufficient.  Reached only through two data (function-table) references at
 * 0x195ee2 and 0x195f13 in FUN_00195ec0. */
void FUN_0017cf20(void *shader, int arg2, int arg3, int arg4, int arg5,
                  void *arg6)
{
  FUN_001677d0(shader, arg2, arg3, arg4, arg5, arg6);
}

/* 0x17cf30: bare JMP 0x167920.  Reached by a CALL at 0x195ef5 and a tail JMP at 0x195f29. */
void FUN_0017cf30(void)
{
  FUN_00167920();
}

/* 0x17cf40: bare JMP 0x15f1f0.  Reached by a single CALL at 0xd1426. */
void FUN_0017cf40(void)
{
  FUN_0015f1f0();
}

/* 0x17cf50: bare JMP 0x15f200.  Reached by a single CALL at 0xd151c. */
void FUN_0017cf50(void)
{
  FUN_0015f200();
}

/* Forwarding wrapper (0x17cf60).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x0015f630 -- the frame is torn down
 * before the jump, so 0x0015f630 inherits this function's stack arguments,
 * caller-cleaned cdecl.
 * Arity/type evidence: the immutable bundle's callees/callee_details/
 * call_site_audit are all empty (the tail JMP is not recorded as a call), so
 * the callee had to be resolved by one bounded Ghidra query.  kb.json declared
 * 0x0015f630 as (void) -- the same placeholder shape that was already found
 * wrong for siblings 0x165de0, 0x166890 and 0x1677d0 -- and Ghidra's stored
 * prototype likewise reports zero parameters with every local marked phantom,
 * i.e. never recovered.  The disassembly of 0x0015f630..0x0015f89f disproves
 * both: it reads entry-relative stack slots [EBP+0x08], [EBP+0x0c],
 * [EBP+0x14], [EBP+0x18], [EBP+0x1c], [EBP+0x20] and [EBP+0x24], and ends
 * MOV ESP,EBP / POP EBP / RET with no immediate (caller-cleaned cdecl, so the
 * tail JMP is ESP-safe).  Slot +0x10 is never read but is bracketed by read
 * slots, so eight dwords are consumed in total -- which matches the eight
 * dwords pushed at both lifted call sites of this wrapper (objects.c and
 * scenario.c), so the arity is caller-proven as well.
 * Slot typing inside 0x0015f630, from its own asserts: +0x08 is dereferenced
 * as *(word *)(p+0x24) under the assert string
 * "shader->base.type==_shader_type_effect", so it is the shader pointer;
 * +0x20 is asserted non-null against the string "centroid" and then read as
 * three floats [p], [p+4], [p+8], so it is a float triple (matches this
 * wrapper's existing float * slot); +0x24 is tested with TEST byte,0x20 under
 * "!TEST_FLAG(geometry_flags, _rasterizer_geometry_viewspace_bit) || ..." so
 * it is the geometry flags.  The remaining slots are only copied into the
 * allocated transparent-geometry-group record (+0x5c, +0x44, +0x54, +0x50) or
 * forwarded to 0x0017edd0, which proves no type, so they keep mechanical
 * names.  kb.json's decl for 0x0015f630 is corrected to this eight-slot
 * signature; this wrapper's own decl is caller-derived and left unchanged.
 * The wrapper's name is unproven (no string or PDB evidence), so the
 * mechanical name is kept. */
void FUN_0017cf60(uint32_t source, uint32_t param, int arg3, int arg4,
                  uint32_t handle, int subcount2, float *origin,
                  uint32_t widget_flags)
{
  FUN_0015f630((void *)source, param, arg3, arg4, handle, subcount2, origin,
               widget_flags);
}

/* Forwarding wrapper (0x17cf70).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x15f210 -- the frame is torn down
 * before the jump, so 0x15f210 inherits this wrapper's incoming stack
 * arguments and reads them as its own.  That four-instruction shape is MSVC's
 * argument-forwarding tail call and is what every other wrapper in this TU
 * compiles to (0x17cf10, 0x17cf20, 0x17cfc0); a wrapper that forwards no
 * argument compiles to a bare JMP with no frame at all, so the frame here is
 * evidence that at least one stack argument slot is passed through.  The
 * exact COUNT is unknown: nothing reads a slot on either side, and one, two
 * or three forwarded dwords all emit these same four instructions (N=2
 * measured at 100% as well).  One int
 * is the minimal signature consistent with the shape.
 *
 * The jump target is an empty function.  The bytes at 0x15f210 are C3
 * followed by fifteen 90 padding bytes, and the previous function in
 * rasterizer_xbox_dynavobgeom.obj ends at 0x15f209, so 0x15f210 is a
 * 16-byte-aligned entry point whose whole body is a bare RET -- not padding
 * the JMP happens to land on.  A bare RET proves only that the callee reads
 * nothing; its parameter comes from this caller's tail-call shape, not from
 * any observed access.  The call is emitted rather than elided because the
 * E9 target is part of the shape being recovered.  No references to
 * 0x17cf70 were found in the binary. */
void FUN_0017cf70(int param_1)
{
  FUN_0015f210(param_1);
}

/* Forwarding wrapper (0x17cf80).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x0015f540 -- the frame is torn down
 * before the jump, so 0x0015f540 inherits this function's stack arguments
 * unchanged, caller-cleaned cdecl.
 * Arity is caller-proven.  The immutable bundle records no callees, no callers
 * and no xrefs for this target, so the sole call site was recovered by scanning
 * the pristine XBE for E8/E9 targets of 0x17cf80: exactly one exists, at
 * 0x0018d433, and it pushes four dwords immediately before the CALL --
 *   push eax (movsx word [esi+8], shl 1) / push ecx (dword [esi]) /
 *   push -4 / push 0
 * -- then cleans with ADD ESP,0x24 (this call's four dwords plus the five
 * pushed by the preceding assert/exit pair).  So kb.json's four-parameter decl
 * for 0x17cf80 is caller-derived and correct, and is kept unchanged.
 * The callee was resolved the same way, by decoding its committed bounds
 * (0x0015f540..0x0015f561, 33 bytes):
 *   push 1 / push 0xff / push 0x2a16c4 / push 0x2a1704 / call 0x0008d9f0
 *   push -1 / call 0x0008e2f0 / add esp,0x14 / ret
 * It has no frame -- no PUSH EBP, and not one [EBP+n] or [ESP+n] slot read;
 * every argument it passes on is an immediate it pushes itself, ADD ESP,0x14
 * balances exactly its own five pushes, and it ends with a plain RET (no
 * RET n).  So the four inherited dwords are handed over by the tail jump and
 * then provably ignored, and nothing is consumed from the caller on either
 * side of the jump.  The four arguments are forwarded here so that the call
 * boundary keeps the shape the tail jump gives it; kb.json's decl for the
 * unported 0x0015f540 is widened from void(void) to the four dwords the jump
 * actually hands it, with mechanical parameter names because the callee never
 * reads them and no naming evidence exists.  This is stack-safe in both
 * spellings (caller-cleaned cdecl, callee reads nothing).
 * The wrapper's name is unproven (no string or PDB evidence), so the
 * mechanical name is kept. */
void FUN_0017cf80(int arg1, int arg2, uint32_t handle, int subcount2)
{
  FUN_0015f540(arg1, arg2, handle, subcount2);
}

/* Forwarding wrapper (0x17cf90).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x15f220 -- the frame is torn down
 * before the jump, so 0x15f220 inherits this wrapper's incoming stack
 * arguments and reads them as its own [EBP+8] and [EBP+0xc].  The argument
 * count is proven by the callee (disassembly at 0x15f224 / 0x15f24c): it
 * loads exactly two dwords, asserts each is non-NULL, and then field-copies
 * from the [EBP+0xc] object into the [EBP+8] object (MOV reg,[EDI+N] /
 * MOV [ESI+N],reg for N = 0x04, 0x09, 0x0a, 0x10, 0x14, 0x19, 0x1a, 0x20,
 * 0x24, 0x30, 0x34, 0x38, 0x3c, 0x48, 0x4c, 0x50, 0x54, 0x5c, 0x60, 0x7c,
 * 0x80, 0x84, 0x86).  The copy direction proves which slot is the
 * destination; the object type is unknown here, so both are void *.  No
 * references to 0x17cf90 were found in the binary. */
void FUN_0017cf90(void *dest, void *src)
{
  FUN_0015f220(dest, src);
}

/* Render sprites by forwarding to the dynavob geometry renderer (0x17cfa0). */
void rasterizer_sprites_render(void *render_data, void *vertices)
{
  FUN_0015f8e0(render_data, vertices);
}

/* Forwarding wrapper (0x17cfb0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x0017ae90 -- the frame is torn down
 * before the jump, so 0x0017ae90 inherits this function's stack arguments
 * unchanged, caller-cleaned cdecl, and its (void) result is this result.
 * Arity evidence: the immutable bundle's callees/callee_details/
 * call_site_audit are empty because Ghidra does not model the tail JMP as a
 * call, not because evidence is missing -- the single touched target 0x17ae90
 * is already lifted in this tree (rasterizer/xbox/rasterizer_xbox_widgets.c,
 * __FILE__-confirmed by its own display_assert) and reads exactly four
 * entry-relative stack slots as full dwords -- [EBP+0x08], [EBP+0x0c],
 * [EBP+0x10] (the centroid float *) and [EBP+0x14] -- returning void with a
 * plain RET (no RET n anywhere in the chain).  So exactly four stack dwords
 * are consumed and none is narrowed at this boundary.
 * In the callee, slot 1 is stored to group+0x4c, slot 2 to group+0x50, slot 3
 * is the centroid read by the plane-distance computation, and slot 4 is both
 * the entry guard and group+0x48; the kb decl's semantic parameter names
 * (object_handle / datum / callback) are unproven by any string or PDB
 * evidence and are kept only for prototype agreement.
 * The bundle records one caller (0x13530e in FUN_00135210), not lifted, so no
 * caller-derived typing or naming is available.  The wrapper's own name is
 * likewise unproven, so the mechanical name is kept. */
void FUN_0017cfb0(int object_handle, int datum, float *position, int callback)
{
  FUN_0017ae90(object_handle, datum, position, callback);
}

/* Forwarding wrapper (0x17cfc0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17b000 -- the frame is torn down
 * before the jump, so 0x17b000 inherits this function's stack arguments and
 * reads them as its own [EBP+8] / [EBP+0xc].  This wrapper never touches the
 * argument slots itself, so their semantics are unknown here; the narrowing
 * casts reflect only the declared widths of the callee at 0x17b000. */
void FUN_0017cfc0(int param_1, int param_2)
{
  FUN_0017b000((int16_t)param_1, (uint16_t)param_2);
}

/* Forwarding wrapper (0x17cfd0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17b480 -- the frame is torn down
 * before the jump, so 0x17b480 inherits this function's stack arguments and
 * reads them as its own [EBP+8] / [EBP+0xc] / [EBP+0x10], and its AL return
 * becomes this function's return value.  This wrapper never touches the
 * argument slots itself, so their semantics are unknown here; the parameter
 * widths follow the callee, which loads all three slots as full DWORDs
 * (MOV reg,dword ptr at 0x17b4ac / 0x17b4b5 / 0x17b4b8 / 0x17b4cc), so the
 * third slot is int, not short. */
char FUN_0017cfd0(int param_1, int param_2, int param_3)
{
  return FUN_0017b480(param_1, param_2, param_3);
}

/* Forwarding wrapper (0x17cfe0).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17b540 -- the frame is torn down
 * before the jump, so 0x17b540 inherits this function's stack argument and
 * reads the incoming dword as its own [EBP+8].
 *
 * The callee 0x17b540 forwards that dword unconverted into the second
 * (float) argument of D3DDevice_SetVertexData2f (`MOV EAX,[EBP+8]` at
 * 0x17b56c, `PUSH EAX` at 0x17b571 -- no FILD/FLD), so the value travelling
 * through this wrapper is a raw IEEE-754 float bit pattern, not an integer.
 * Both lifted call sites agree: 0x17b3xx pushes the literal 0x3f800000
 * (= 1.0f) and rasterizer_text.c loads a dword float field at refl+0x40.
 * The parameter therefore stays a dword (`unsigned int`) so the call sites
 * keep passing bits unchanged, and the argument slot is re-interpreted in
 * place for the float-typed callee.  Converting instead (`(float)value_bits`)
 * would emit a FILD and destroy the value; a union temporary was measured to
 * cost a stack round-trip that blocks the tail call (50% vs the 4-instruction
 * reference). */
void FUN_0017cfe0(unsigned int value_bits)
{
  FUN_0017b540(*(float *)&value_bits);
}

/* Forwarding wrapper (0x17cff0).  Same four-instruction shape as 0x17cfe0:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17b580 -- the frame is torn down
 * before the jump, so 0x17b580 inherits this function's stack argument and
 * reads the incoming slot as its own [EBP+8].  kb.json declared this
 * `void FUN_0017cff0(void)` and Ghidra therefore showed a bare call with no
 * argument; the pass-through is recovered from the POP/JMP pair, not from the
 * decompiler.
 *
 * The callee loads the slot with `MOVZX EAX,byte ptr [EBP+8]` at 0x17b5a2 and
 * feeds it to D3DDevice_SetRenderState_ZEnable, so only the low byte is
 * observed and the parameter is `bool` -- matching the callee's own recovered
 * signature and keeping the forward conversion-free (an `int` parameter would
 * add a CMP/SETNE that the reference does not have).
 *
 * No xrefs to 0x17cff0 exist in the binary, so the caller-side meaning of the
 * flag is unknown beyond "Z-enable on/off". */
void FUN_0017cff0(bool enable)
{
  FUN_0017b580(enable);
}

/* Forwarding wrapper (0x17d000).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17b5c0 -- the frame is torn down
 * before the jump, so 0x17b5c0 inherits this function's stack arguments and
 * reads them as its own [EBP+8] .. [EBP+0x1c] (six dwords).  kb.json declared
 * this `void FUN_0017d000(void)` and Ghidra therefore rendered the body as a
 * bare `FUN_0017b5c0()` call with no arguments; the pass-through is recovered
 * from the POP/JMP pair, not from the decompiler.  A `(void)` wrapper would
 * hand the callee a garbage `point`, which its own NULL assert at
 * rasterizer_xbox_widgets.c:0x164 immediately dereferences.
 *
 * The parameter types are the callee's own recovered signature (see
 * FUN_0017b5c0 in src/halo/rasterizer/xbox/rasterizer_xbox_widgets.c): a 2D
 * point, a >0 radius gate, an optional per-axis scale, an optional integer
 * texcoord repeat count, a rotation angle, and a flat vertex colour.  Matching
 * them exactly keeps the forward conversion-free, which is what lets the tail
 * call survive (the neighbouring 0x17cfe0 note records the measured cost of a
 * temporary that blocks it).
 *
 * No xrefs to 0x17d000 exist in the binary, so the caller-side meaning of the
 * arguments is unknown beyond the callee's use of them. */
void FUN_0017d000(float *point, float radius, float *scale,
                  float *texcoord_repeat, float theta, unsigned int color)
{
  FUN_0017b5c0(point, radius, scale, texcoord_repeat, theta, color);
}

/* Forwarding wrapper (0x17d010).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17b7d0 -- the frame is torn down
 * before the transfer, so 0x17b7d0 inherits this function's stack arguments
 * and reads them verbatim as its own five cdecl slots.
 * kb.json already declares 0x17d010 with the same five-slot shape as the
 * callee decl at 0x17b7d0 (float *, float, float *, float, unsigned int), so
 * no decl correction is needed here and both decls are left unchanged.
 * The wrapper's own name is unproven (no string or PDB evidence), so the
 * mechanical name is kept.
 * Shape note: the original is a true tail JMP.  Because this is an
 * identical-signature void cdecl forward, the compiler preserves the tail
 * transfer rather than emitting a CALL, so no trivial-thunk penalty applies
 * here: VC71 measures 100% (4/4 insns) against the four-instruction
 * reference at 0x17d010..0x17d019. */
void FUN_0017d010(float *position, float radius, float *scale2d, float angle,
                  uint32_t color)
{
  FUN_0017b7d0(position, radius, scale2d, angle, color);
}

/* 0x17d020: bare JMP 0x17ad90.  Reached by CALLs at 0x1351fd, 0x181bfd and 0x182428. */
void FUN_0017d020(void)
{
  FUN_0017ad90();
}

/* Forwarding wrapper (0x17d030).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17ba10 -- the frame is torn down
 * before the jump, so 0x17ba10 inherits this function's stack arguments and
 * reads them as its own [EBP+8] .. [EBP+0x10] (three dwords), and its EAX
 * return becomes this function's return value (Ghidra shows `extraout_EAX`).
 *
 * The parameter types are the callee's own recovered signature (see
 * rasterizer_widget_submit_occlusion_test at 0x17ba10 in
 * src/halo/rasterizer/rasterizer.c): a position pointer, a float radius, and
 * an unsigned index.  kb.json previously declared the middle slot `int`; the
 * slot is forwarded untouched by the POP/JMP, so the callee's own type is the
 * binary-backed one, and matching it keeps the forward conversion-free -- an
 * `int` parameter would insert a FILD that the reference does not have.
 *
 * The only reference is a call at 0x181bdb inside FUN_00181a90, which is not
 * yet lifted, so the caller-side meaning of the arguments is unknown beyond
 * the callee's use of them. */
int FUN_0017d030(float *position, float radius, unsigned int index)
{
  return rasterizer_widget_submit_occlusion_test(position, radius, index);
}

/* Forwarding wrapper (0x17d040).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x17adc0 -- the frame is torn down
 * before the jump, so 0x17adc0 inherits this function's stack argument and
 * reads it as its own [EBP+8], and its EAX return becomes this function's
 * return value.  The single dword parameter therefore comes from the POP/JMP
 * pair, not from the decompiler (Ghidra renders this as `void (void)` because
 * it cannot see through the tail jump).
 *
 * The callee is rasterizer_widget_get_occlusion_test_result at 0x17adc0 in
 * src/halo/rasterizer/xbox/rasterizer_xbox_widgets.c, declared there as
 * `unsigned int (unsigned int index)`.  Unlike the neighbouring 0x17d030 note,
 * the signedness difference against kb.json's `int` costs no instruction (an
 * int<->unsigned dword needs no conversion, whereas an int->float slot would
 * have inserted a FILD), so the kb.json declaration is left unchanged; the
 * sole caller, rasterizer_text.c:783, consumes the result in signed
 * arithmetic.
 *
 * That caller is the lens-flare occlusion path: the result is scaled by 0xff
 * against a per-flare divisor.  The meaning of `index` beyond the callee's own
 * use of it as a widget/occlusion-query slot is unknown. */
int FUN_0017d040(int index)
{
  return (int)rasterizer_widget_get_occlusion_test_result((unsigned int)index);
}

/* 0x17d050: bare JMP 0x16dee0.  Reached by a single tail JMP at 0xdb23e. */
void FUN_0017d050(void)
{
  FUN_0016dee0();
}

/* Forwarding wrapper (0x17d060).  The original is four instructions:
 * PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x16e160 -- the frame is torn down
 * before the jump, so 0x16e160 runs on this function's incoming stack and
 * reads the incoming dwords as its own [EBP+8..0x18].  The wrapper's arity is
 * therefore the callee's arity; Ghidra renders both as `void (void)` because
 * it cannot see through the tail jump (its param_count for 0x16e160 is 0, and
 * that is wrong -- see the call-site evidence below).
 *
 * Arity and slot types come from the sole caller, render_blip at 0xdb4a0,
 * which is cdecl and cleans 20 bytes:
 *     000db495: PUSH EAX          ; arg5, materialized by SETZ AL at 0xdb48c
 *     000db496: PUSH ESI          ; arg4
 *     000db497: PUSH ECX          ; arg3 slot -- dummy push, overwritten below
 *     000db498: MOV  ECX,[EBP+0x14]
 *     000db49b: FSTP float [ESP]  ; arg3 is a float (push-then-fstp idiom)
 *     000db49e: PUSH ECX          ; arg2 = caller's [EBP+0x14]
 *     000db49f: PUSH EDX          ; arg1 = LEA EDX,[EBP-0xc], a stack pointer
 *     000db4a0: CALL 0x17d060
 *     000db4a5: ADD  ESP,0x14     ; 5 dword slots
 *
 * The callee is unlifted (FUN_0016e160); its string constants place it in
 * c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_motion_sensor.c and it
 * issues D3DDevice_Begin, D3DDevice_SetVertexData* and D3DDevice_End, so it
 * draws motion-sensor geometry.  The meaning of the individual arguments
 * beyond their slot widths is unknown; they are forwarded unchanged. */
void FUN_0017d060(void *param_1, int param_2, float param_3, int param_4,
                  int param_5)
{
  FUN_0016e160(param_1, param_2, param_3, param_4, param_5);
}

/* 0x17d070: PUSH EBP / MOV EBP,ESP / POP EBP / JMP 0x16e2e0 -- the frame is
 * torn down before the transfer, so 0x16e2e0 reads this function's cdecl
 * slots verbatim as its own [EBP+0x8..0xc].  Ghidra's draft prototype for
 * both ends was `void (void)`; the callee body
 * (c:\halo\SOURCE\rasterizer\xbox\rasterizer_xbox_motion_sensor.c, named by
 * the __FILE__ string in its own global_d3d_device assert, line 0x9c) reads
 * exactly two incoming slots:
 *   slot 1  float*  -- two floats, used as the screen x/y centre of the last
 *                      quad it emits via D3DDevice_SetVertexData2f(0, ...)
 *   slot 2  float   -- scalar folded into the sweep texcoords as
 *                      (_DAT_00253398 - t*_DAT_00253398) and
 *                      (t*_DAT_00253398 + _DAT_00253398)
 * The sole caller (FUN_000dbcb0, motion_sensor.c) confirms the count and the
 * types: MOV ECX,[0x0046bd30] / PUSH ECX / PUSH 0x5aa680 / CALL 0x17d070 /
 * ADD ESP,0x8 -- two cdecl slots, pointer last-pushed so it is slot 1.
 * `position` is behaviour-derived (quad centre); the scalar has no string or
 * PDB evidence naming it, so it stays param_2.  Slot 2 must stay `float` on
 * both ends: an int-typed forward would make clang insert FILD/FSTP and
 * convert the bits instead of passing the slot through.  Plain forwarding
 * preserves the tail call, as for the sibling thunks at 0x17d030/0x17d040/
 * 0x17d060. */
void FUN_0017d070(float *position, float param_2)
{
  FUN_0016e2e0(position, param_2);
}

/* 0x17d080: draw a three-segment axis cross centred on `point`, each arm
 * spanning `radius` along one world axis.
 *
 * Frame evidence (0x17d080..0x17d143): PUSH EBP / MOV EBP,ESP / SUB ESP,0x18
 * with PUSH ESI / PUSH EDI after the frame, so the locals are exactly six
 * floats.  They are addressed as two three-float arrays by the two LEAs that
 * feed every call site: `LEA EDX,[EBP-0x18]` (segment start) and
 * `LEA ECX,[EBP-0xc]` (segment end).  ESI holds `point` for the whole body
 * and EDI holds `color`.
 *
 * The half-extent is folded into the incoming parameter slot rather than a
 * new local: FLD [EBP+0xc] / FMUL [0x253398] / FSTP [EBP+0xc], where the
 * constant at 0x253398 is 0x3f000000 = 0.5f.  Writing it back to [EBP+0xc]
 * is why the frame is 0x18 and not 0x1c, so the parameter is mutated here
 * instead of copied into a local.
 *
 * Per-axis store offsets, taken from the raw MOV/FSTP destinations (the
 * decompiler's field labels are not used):
 *   axis 0: [EBP-0x18]=FSTP(point[0]-r)  [EBP-0x14]=point[1]
 * [EBP-0x10]=point[2] [EBP-0xc] =FSTP(r+point[0])  [EBP-0x8] =point[1]
 * [EBP-0x4] =point[2] axis 1: [EBP-0x18]=point[0] [EBP-0x14]=FSTP(point[1]-r)
 * [EBP-0x10]=point[2] [EBP-0xc] =point[0] [EBP-0x8] =FSTP(r+point[1]) [EBP-0x4]
 * =point[2] axis 2: [EBP-0x18]=point[0] [EBP-0x14]=point[1]
 * [EBP-0x10]=FSTP(point[2]-r) [EBP-0xc] =point[0] [EBP-0x8] =point[1] [EBP-0x4]
 * =FSTP(r+point[2]) All six components are rewritten before each call even
 * where the value is unchanged, so the source assigns all six each time.
 *
 * x87 operand order is per-site and preserved: the low endpoint is
 * FLD [ESI+n] / FSUB [EBP+0xc] (point[i] - r) while the high endpoint is
 * FLD [EBP+0xc] / FADD [ESI+n] (r + point[i]).
 *
 * Calls: PUSH EDI / PUSH EDI / PUSH ECX(&end) / PUSH EDX(&start) /
 * CALL 0x15a7f0, three times, with one combined ADD ESP,0x30 (3*4 cdecl
 * slots).  Last push is the first C argument, giving
 * FUN_0015a7f0(start, end, color, color) against the callee decl
 * `void FUN_0015a7f0(float*, float*, float*, float*)` -- the same
 * single-colour line call the sibling at 0x17d150 makes.
 *
 * kb.json previously declared this `void FUN_0017d080(void)`; the arity and
 * types above were corrected by an earlier naming-only pass.  The function
 * has no callers and no code or data xrefs anywhere in the binary, so it is
 * dead code and no call-site arity contract is disturbed; it also means no
 * runtime or equivalence oracle is reachable for it.
 * Names are behaviour-derived only (no string or PDB evidence) and therefore
 * provisional; the mechanical function name is kept.
 * Shape note: the reference interleaves the argument pushes and the copy MOVs
 * into the FLD/FSTP chains.  There is no call between, so that is MSVC
 * scheduling an x87 chain against the pushes, not a side-effect order. */
void FUN_0017d080(float *point, float radius, float *color)
{
  float start[3];
  float end[3];

  radius = radius * 0.5f;

  start[0] = point[0] - radius;
  start[1] = point[1];
  start[2] = point[2];
  end[0] = radius + point[0];
  end[1] = point[1];
  end[2] = point[2];
  FUN_0015a7f0(start, end, color, color);

  start[0] = point[0];
  start[1] = point[1] - radius;
  start[2] = point[2];
  end[0] = point[0];
  end[1] = radius + point[1];
  end[2] = point[2];
  FUN_0015a7f0(start, end, color, color);

  start[0] = point[0];
  start[1] = point[1];
  start[2] = point[2] - radius;
  end[0] = point[0];
  end[1] = point[1];
  end[2] = radius + point[2];
  FUN_0015a7f0(start, end, color, color);
}

/* 0x17d150: draw a debug line from `point` to `point + scale * direction`.
 *
 * Frame evidence (0x17d150..0x17d193): PUSH EBP / MOV EBP,ESP / SUB ESP,0xc,
 * so the only locals are three floats at EBP-0xc/-0x8/-0x4, addressed as one
 * array by the single `LEA ECX,[EBP-0xc]` that feeds the call.  Slots:
 *   [EBP+0x8]  float*  -- base point (EAX; also passed as the callee's arg 1)
 *   [EBP+0xc]  float*  -- direction (ECX; read at +0,+4,+8)
 *   [EBP+0x10] float   -- scalar, FLD'd fresh for each component
 *   [EBP+0x14] float*  -- colour, pushed twice (PUSH ECX / PUSH ECX, no
 *                         reload), so both of the callee's colour slots get
 *                         the same pointer: a single-colour line.
 * Each component is FLD [EBP+0x10] / FMUL [ECX+n] / FADD [EAX+n] / FSTP
 * [EBP-0xc+n], i.e. scale*direction[i] + point[i], written in that x87
 * operand order per site.
 * Call: PUSH ECX,PUSH ECX,PUSH ECX(&endpoint),PUSH EAX / CALL 0x15a7f0 /
 * ADD ESP,0x10 -- four cdecl slots, last push first, giving
 * FUN_0015a7f0(point, endpoint, colour, colour) against the callee decl
 * `void FUN_0015a7f0(float*, float*, float*, float*)`.
 * kb.json declared `void FUN_0017d150(void)`; the arity/types above are
 * corrected here.  The bundle reports no callers and no xrefs (code or data)
 * to 0x17d150, so no call site or callback-table arity contract is disturbed.
 * Names are behaviour-derived only (no string or PDB evidence) and therefore
 * provisional; the mechanical function name is kept.
 * Shape note: the reference interleaves the two colour pushes and the
 * LEA/PUSH pair into the third FADD/FSTP pair.  There is no call between, so
 * that is MSVC scheduling an x87 chain against the argument pushes, not a
 * side-effect ordering we can or should express in C. */
void FUN_0017d150(float *point, float *direction, float scale, float *color)
{
  float endpoint[3];

  endpoint[0] = scale * direction[0] + point[0];
  endpoint[1] = scale * direction[1] + point[1];
  endpoint[2] = scale * direction[2] + point[2];
  FUN_0015a7f0(point, endpoint, color, color);
}

/* cinematic_screen_effect_globals — game-state block holding the cinematic
 * screen-effect globals.  Name is string-proven: the assert at 0x17d92e passes
 * "cinematic_screen_effect_globals" as its message.  Contents unknown (0x78
 * bytes), so the pointer stays void *. */
#define cinematic_screen_effect_globals (*(void **)0x47e4d4)

/* 0x17d1a0: fire the screen-effect "reference" collision ray and refresh the
 * cached hit result at 0x47e4cc.
 *
 * param_1 non-zero is the early-out arm at 0x17d292: it stores NONE
 * (0xffffffff) into 0x47e4cc and tail-calls 0x16b180 with the same flag; the
 * ray is not cast.  TEST BL,BL / PUSH EBX shows the incoming slot is used as a
 * one-byte truth value and then forwarded unchanged to 0x16b180 (whose kb decl
 * is `bool flag`), so the parameter is typed `bool` here — the draft `int`
 * prototype would force a bool materialization at both call sites that the
 * reference does not have.
 *
 * The zero arm pushes a collision-user-stack frame tagged 0x15 (assert
 * strings and __FILE__ prove the collision-user-depth macro; the __FILE__ is
 * the binary's TU "c:\halo\SOURCE\rasterizer\rasterizer.c", not this file),
 * builds the ray direction as the three floats at 0x5a5bd4/0x5a5bd8/0x5a5bdc
 * each scaled by the float at 0x2af240 (FLD/FMUL/FSTP into EBP-0xc, -0x8, -0x4
 * in that order), and raycasts from the vector at 0x5a5bc8.
 *
 * Frame evidence: SUB ESP,0x5c with the direction triple at EBP-0xc..-0x4 and
 * the collision result buffer based at EBP-0x5c, i.e. the result buffer is
 * exactly 0x50 bytes (0x14df70 writes an 80-byte struct — see units.c 0x9196).
 * The dword read back on a hit is MOV EDX,[EBP-0x24], which is buffer offset
 * 0x5c-0x24 = 0x38, NOT a separate local.
 *
 * The distance/handle argument is MOVSX EAX, word ptr [0x506548] — a SIGNED
 * 16-bit read of the local-player global.
 *
 * The depth global at 0x4761d8 is read fresh at each of its four uses (the
 * reference never caches it) and the pop is a single DEC word ptr.
 *
 * VC71 shape notes (score levers, behaviour-neutral): the reference schedules
 * the MOVW $0x15 stack-slot store *inside* the direction triple (between the
 * second and third FLD/FMUL pair) and loads MOVSX [0x506548] early, before the
 * first FSTP; the source order below mirrors both.  `depth` is a plain int
 * (the read still sign-extends the int16_t global) because a 16-bit local made
 * clang emit an extra XOR EAX,EAX plus a 32-bit LEA for the increment. */
void FUN_0017d1a0(bool param_1)
{
  char collision_result[80];
  float direction[3];
  int depth;
  int local_player;

  if (!param_1) {
    if (*(int16_t *)0x4761d8 >= 0x20) {
      display_assert("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c", 0x2e4, 1);
      system_exit(-1);
    }

    depth = *(int16_t *)0x4761d8;
    *(int16_t *)0x4761d8 = depth + 1;
    local_player = (int)*(int16_t *)0x506548;

    direction[0] = *(float *)0x5a5bd4 * *(float *)0x2af240;
    direction[1] = *(float *)0x5a5bd8 * *(float *)0x2af240;
    *(int16_t *)(0x5a8c80 + depth * 2) = 0x15;
    direction[2] = *(float *)0x5a5bdc * *(float *)0x2af240;

    if (FUN_0014df70(0xfff80, (float *)0x5a5bc8, direction, local_player,
                     (int16_t *)collision_result)) {
      *(int *)0x47e4cc = *(int *)(collision_result + 0x38);
    }

    if (*(int16_t *)0x4761d8 <= 1) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c", 0x2f4, 1);
      system_exit(-1);
    }
    --*(int16_t *)0x4761d8;

    FUN_0016b180(param_1);
    return;
  }

  *(int *)0x47e4cc = -1;
  FUN_0016b180(param_1);
}

/* 0x17d2b0: dump the skinned vertices of one model part that lie under the
 * screen-effect reference ray, then draw a debug point per unique position and
 * a text label on the one nearest the camera.
 *
 * Prototype.  Ghidra recovered no signature (the body reads
 * `in_stack_00000004/8/c`), and kb.json carried a placeholder
 * `void rasterizer_debug_model_vertices(void)`.  The disassembly reads three
 * stack dwords and never adjusts ESP on return, so it is a 3-argument cdecl:
 *   [EBP+0x08] compared against the screen-effect reference handle at
 *              0x47e4cc (which FUN_0017d1a0 latches from the collision
 *              result's +0x38), so it is the render-data / geometry handle;
 *   [EBP+0x0c] the skinning block -- [+0] is the node-matrix array base and
 *              [+4] is an int16 count, named `skinning->node_matrix_count` by
 *              the asserts at lines 0x36e/0x36f;
 *   [EBP+0x10] `part`, named by the assert string at line 0x33e.
 * The sole caller (units.c, inside FUN_00123560's opaque pass) already passed
 * three arguments through a raw function-pointer cast; correcting the kb decl
 * lets that cast go away.
 *
 * Frame.  MOV EAX,0x20280 / CALL 0x1d90e0 (_chkstk).  The 0x20280 bytes are
 * exactly 0x80 of scalars + two 0x100 text buffers (EBP-0x180, EBP-0x280) +
 * 0x20000 for a 0x800-entry record array at EBP-0x20280.  The array stride is
 * proven twice: `ADD ECX,0x40` in the search loop and `ADD EBX,0x40` in the
 * print loop, with `LEA ESI,[EBX-0x3c]` recovering the record base from the
 * +0x3c cursor.
 *
 * Record layout, derived from the raw MOV destinations (the decompiler types
 * the cursor `undefined2 *`, which halves every printed offset):
 *   +0x00 float position[3]    MOV [EAX],EDX / [EAX+4] / [EAX+8]
 *   +0x0c short index_list[12] MOV [ECX-0x18],AX  (cursor is base+0x24)
 *   +0x24 short vertex_list[12] MOV [ECX],DX
 *   +0x3c uchar index_count    MOV byte [ECX+0x18],1 / INC byte [ECX+0x3c]
 *   +0x3d uchar vertex_count   MOV byte [ECX+0x19],1 / INC byte [ECX+0x3d]
 * The 12-entry cap is the `CMP AL,0xc / JNC` guard on each count, and 12
 * shorts is exactly the gap to the next field, so both arrays are [12].
 *
 * Element signedness is settled by the two membership tests, which differ:
 *   index_list  MOV BX,[EBP-0x14] / CMP word [ECX+EAX*2+0xc],BX   16-bit
 *   vertex_list MOVSX EBX,word [ECX+EAX*2+0x24] / MOVZX EAX,word [EBP-0x10]
 * A 16-bit compare means both sides are `short`; the widened compare means the
 * operands differ in signedness, so vertex_list is `short` while the loop's
 * vertex index is `unsigned short` (it comes from MOVZX of the index buffer).
 *
 * Per-vertex decode (vertex stride 0x20 from SHL ESI,0x5):
 *   +0x00 float position[3]  (passed straight to matrix_transform_point)
 *   +0x0c uint32 packed normal, handed to uncompress_int32_to_real_vector3d
 *   +0x1c int8  node index 0, +0x1d int8 node index 1 -- both MOVSX then
 *         divided by 3 via the 0x55555556 magic multiply with the
 *         SHR 0x1f / ADD sign fixup, i.e. a signed /3
 *   +0x1e int16 node weight, FILD'd and scaled by the constant at 0x290dd8 =
 *         0x38000100 = 1.0f/32767.0f
 * uncompress_int32_to_real_vector3d writes exactly three dwords through its first argument and
 * returns that same pointer (MOV EAX,[EBP+8] / MOV ECX,EAX, EAX untouched
 * afterwards), so the scratch buffer is a float[3] and the copy that follows
 * reads back through the returned pointer.
 *
 * Constants: 0x2533c0 = 0.0f, 0x2533c8 = 1.0f, 0x255e94 = -1.0f,
 * 0x2533d0 = the double 0x3f1a36e2e0000000, which is the double promotion of
 * the float 1e-4f -- the FABS before it is why the compare is `double`
 * (fabs() returns double, so both sides promote).
 *
 * Branch senses, from FNSTSW/TEST rather than the decompiler:
 *   TEST AH,0x44 / JP   -> "not equal"  (C3|C2, even parity == neither set)
 *   TEST AH,0x05 / JNP  -> "less"       (C0|C2, odd parity == C0 set)
 *   TEST AH,0x41 / JNP  -> "less or equal" for the 0x370 assert's upper bound
 * The best-candidate test at 0x17d73f..0x17d761 is therefore
 *   (dot < 0.0f && best_distance < distance) || best_distance == -1.0f
 * with the two arms falling into a shared update block.
 *
 * Uncertain / faithful oddity: [EBP-0x2c] (`best_distance`) is read at
 * 0x17d746 and 0x17d753 but never initialised anywhere in the original --
 * the only write is the update at 0x17d769.  The `== -1.0f` test reads like
 * an intended "no candidate yet" sentinel that was never stored, so the
 * original reads uninitialised stack on the first iteration.  That read is
 * C UB and toolchain-dependent (cl.exe leaves stack garbage; this build's
 * clang happens to zero it via FLDZ), so `best_distance` is explicitly
 * initialised to 0.0f below rather than left to compiler whim -- the
 * sentinel comparison and control flow are otherwise unchanged.
 *
 * x87 association is taken from the stack discipline, not rewritten into a
 * "natural" x,y,z order: the delta is left in ST0..ST2 as (dz,dy,dx) after the
 * three FSUBs, and every following sum consumes ST0 first, giving
 * (dz*dz + dy*dy) + dx*dx for the magnitude and the same z,y,x order for both
 * dot products.
 *
 * The ',' / ' ' separator is the branchless MSVC two-constant select
 * SETNZ DL / DEC EDX / AND EDX,0xfffffff4 / ADD EDX,0x2c: 0x2c when the index
 * is not the last, 0x20 when it is.
 *
 * Object-membership note: the assert __FILE__ here is
 * c:\halo\SOURCE\rasterizer\rasterizer.c, not a sprites TU, so this function's
 * kb object (rasterizer_sprites.obj) does not match its original translation
 * unit.  Flagged only; not investigated. */
typedef struct rasterizer_debug_vertex_record {
  float position[3]; /* +0x00 */
  short index_list[12]; /* +0x0c */
  short vertex_list[12]; /* +0x24 */
  unsigned char index_count; /* +0x3c */
  unsigned char vertex_count; /* +0x3d */
  char pad_3e[2]; /* +0x3e */
} rasterizer_debug_vertex_record;

void rasterizer_debug_model_vertices(int render_data, int *skinning,
                                     unsigned char *part)
{
  float node_weight0;
  float node_weight1;
  int point_count;
  unsigned short vertex_index;
  float distance;
  short strip_index;
  float point[3];
  int best_index;
  float best_distance;
  float point1[3];
  float normal1[3];
  float normal0[3];
  float point0[3];
  float normal[3];
  float vertex_normal[3];
  float decompressed[3];
  char index_text[256];
  char vertex_text[256];
  rasterizer_debug_vertex_record points[0x800];
  unsigned char *vertex;
  float *unpacked;
  short node_index0;
  short node_index1;
  short k;
  int j;
  float dx;
  float dy;
  float dz;
  float magnitude;

  assert_halt_msg_at("part", "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c",
                     0x33e, part != NULL);

  if (*(char *)0x3256bf != 0 && render_data == *(int *)0x47e4cc) {
    point_count = 0;
    best_index = -1;
    best_distance = 0.0f;
    assert_halt_msg_at(
      "part->triangle_buffer.type==_triangle_buffer_type_precompiled_strip",
      "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c", 0x359,
      *(short *)(part + 0x44) == 1);

    for (strip_index = 0; strip_index < *(int *)(part + 0x48) + 2;
         strip_index++) {
      vertex_index = ((unsigned short *)*(void **)(part + 0x3c))[strip_index];
      vertex = *(unsigned char **)(part + 0x30) + vertex_index * 0x20;
      node_index0 = (short)(*(signed char *)(vertex + 0x1c) / 3);
      node_weight0 = *(short *)(vertex + 0x1e) * (1.0f / 32767.0f);
      node_weight1 = 1.0f - node_weight0;
      node_index1 = (short)(*(signed char *)(vertex + 0x1d) / 3);

      point0[0] = 0.0f;
      point0[1] = 0.0f;
      point0[2] = 0.0f;
      normal0[0] = 0.0f;
      normal0[1] = 0.0f;
      normal0[2] = 0.0f;
      point1[0] = 0.0f;
      point1[1] = 0.0f;
      point1[2] = 0.0f;
      normal1[0] = 0.0f;
      normal1[1] = 0.0f;
      normal1[2] = 0.0f;

      unpacked = uncompress_int32_to_real_vector3d(decompressed, *(unsigned int *)(vertex + 0xc));
      vertex_normal[0] = unpacked[0];
      vertex_normal[1] = unpacked[1];
      vertex_normal[2] = unpacked[2];

      assert_halt_msg_at("node_index0<skinning->node_matrix_count",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c", 0x36e,
                         node_index0 < *(short *)((char *)skinning + 4));
      assert_halt_msg_at("node_index1<skinning->node_matrix_count",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c", 0x36f,
                         node_index1 < *(short *)((char *)skinning + 4));
      assert_halt_msg_at("node_weight0>=0.0f && node_weight0<=1.0f",
                         "c:\\halo\\SOURCE\\rasterizer\\rasterizer.c", 0x370,
                         node_weight0 >= 0.0f && node_weight0 <= 1.0f);

      if (node_index0 >= 0) {
        matrix_transform_point((float *)(*skinning + node_index0 * 0x34),
                               (float *)vertex, point0);
        matrix_scale_transform_vector((float *)(*skinning + node_index0 * 0x34),
                                      vertex_normal, normal0);
      }
      if (node_index1 >= 0) {
        matrix_transform_point((float *)(*skinning + node_index1 * 0x34),
                               (float *)vertex, point1);
        matrix_scale_transform_vector((float *)(*skinning + node_index1 * 0x34),
                                      vertex_normal, normal1);
      }

      point[0] = point1[0] * node_weight1 + point0[0] * node_weight0;
      point[1] = point1[1] * node_weight1 + point0[1] * node_weight0;
      point[2] = point1[2] * node_weight1 + point0[2] * node_weight0;
      normal[0] = normal1[0] * node_weight1 + normal0[0] * node_weight0;
      normal[1] = normal1[1] * node_weight1 + normal0[1] * node_weight0;
      normal[2] = normal1[2] * node_weight1 + normal0[2] * node_weight0;
      normalize3d(normal);

      for (j = 0; j < point_count; j++) {
        if (point[0] == points[j].position[0] &&
            point[1] == points[j].position[1] &&
            point[2] == points[j].position[2]) {
          if (points[j].index_count < 12) {
            for (k = 0; k < points[j].index_count; k++) {
              if (points[j].index_list[k] == strip_index) {
                break;
              }
            }
            if (k == points[j].index_count) {
              points[j].index_list[points[j].index_count] = strip_index;
              points[j].index_count++;
            }
          }
          if (points[j].vertex_count < 12) {
            for (k = 0; k < points[j].vertex_count; k++) {
              if (points[j].vertex_list[k] == vertex_index) {
                break;
              }
            }
            if (k == points[j].vertex_count) {
              points[j].vertex_list[points[j].vertex_count] =
                (short)vertex_index;
              points[j].vertex_count++;
            }
          }
          break;
        }
      }

      if (j == point_count && point_count < 0x800) {
        dx = point[0] - *(float *)0x5a5bc8;
        dy = point[1] - *(float *)0x5a5bcc;
        dz = point[2] - *(float *)0x5a5bd0;
        points[point_count].position[0] = point[0];
        points[point_count].position[1] = point[1];
        points[point_count].position[2] = point[2];
        points[point_count].index_list[0] = strip_index;
        points[point_count].vertex_list[0] = (short)vertex_index;
        points[point_count].index_count = 1;
        points[point_count].vertex_count = 1;
        magnitude = sqrtf(dz * dz + dy * dy + dx * dx);
        if (fabs(magnitude) >= 1e-4f) {
          magnitude = 1.0f / magnitude;
          dx = dx * magnitude;
          dy = dy * magnitude;
          dz = dz * magnitude;
        }
        distance = *(float *)0x5a5bdc * dz + *(float *)0x5a5bd8 * dy +
                   *(float *)0x5a5bd4 * dx;
        if ((normal[2] * dz + normal[1] * dy + normal[0] * dx < 0.0f &&
             best_distance < distance) ||
            best_distance == -1.0f) {
          best_index = j;
          best_distance = distance;
        }
        point_count++;
      }
    }

    for (j = 0; j < point_count; j++) {
      if (j == best_index) {
        csstrcpy((char *)0x5ab100, "I=");
        for (k = 0; k < points[j].index_count; k++) {
          crt_sprintf(index_text, "%d%c", points[j].index_list[k],
                      k != points[j].index_count - 1 ? ',' : ' ');
          FUN_0008dc30((char *)0x5ab100, index_text);
        }
        FUN_0008dc30((char *)0x5ab100, "\nV=");
        for (k = 0; k < points[j].vertex_count; k++) {
          crt_sprintf(vertex_text, "%d%c", points[j].vertex_list[k],
                      k != points[j].vertex_count - 1 ? ',' : ' ');
          FUN_0008dc30((char *)0x5ab100, vertex_text);
        }
        FUN_00189150(0, points[j].position, 0.03125f, *(void **)0x2ee6d0);
        FUN_00189cb0(0, points[j].position, (void *)0x5ab100, *(int *)0x2ee6e0);
      } else {
        FUN_00189150(0, points[j].position, 0.03125f, *(void **)0x2ee6c4);
      }
    }
  }
}


/* 0x17d8f0.  Ghidra decompiles this as `void` and drops the result, but the
 * original never pops the x87 stack:
 *     0017d8f4: CALL 0x000b5aa0        ; game_time_get()
 *     0017d8f9: MOV  [EBP-0x4],EAX     ; int spill for the FILD cast
 *     0017d8fc: FILD dword [EBP-0x4]
 *     0017d8ff: FMUL float [0x002546a4] ; seconds-per-tick
 *     0017d908: RET                    ; product left in ST(0)
 * so the return value is a float.  Called four times from
 * the caller at 0x17dc70 (call sites 0x17dcd4, 0x17dcf8, 0x17dd61,
 * 0x17dd85).  The int cast is the left operand to keep the
 * FILD-then-FMUL-memory shape. */
float FUN_0017d8f0(void)
{
  return (float)game_time_get() * *(float *)0x2546a4;
}

/* rasterizer_screen_effects_initialize (0x17d910).  Allocates the 0x78-byte
 * cinematic screen-effect globals block from the game-state heap and halts if
 * the allocation fails.  The original has no frame; it computes into EAX,
 * TESTs EAX, stores to the global, then branches:
 *     0017d919: CALL 0x1bfbf0          ; game_state_malloc
 *     0017d91e: ADD  ESP,0xc
 *     0017d921: TEST EAX,EAX
 *     0017d923: MOV  [0x0047e4d4],EAX
 *     0017d928: JNZ  0x17d947
 * so the null test is on the returned value, not on a re-read of the global.
 * The .rdata assert literals (0x2af314 / 0x2af334 / line 0x36) name Bungie's
 * TU c:\halo\SOURCE\rasterizer\rasterizer_cinematics.c and are reproduced
 * verbatim; kb.json maps this address to rasterizer_sprites.obj.  The layout
 * of the 0x78-byte block is unknown. */
void rasterizer_screen_effects_initialize(void)
{
  void *globals;

  globals = game_state_malloc("screen effect filth", 0, 0x78);
  *(void **)0x47e4d4 = globals;
  if (globals == 0) {
    display_assert("cinematic_screen_effect_globals",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_cinematics.c",
                   0x36, 1);
    system_exit(-1);
  }
}

/* 0x17d950: reset the cinematic screen-effect globals.  Zeroes the whole
 * 0x78-byte block allocated by rasterizer_screen_effects_initialize, then
 * writes the dword 0x3f800000 (1.0f) into the four consecutive slots at
 * +0x64, +0x68, +0x6c and +0x70.  The block's layout is unknown, so those
 * four slots are only inferred to be floats from the stored bit pattern; the
 * store width and value are the binary's.
 * Guarded on the pointer being non-null (MOV EAX,[0x47e4d4] / TEST EAX,EAX /
 * JZ), and the pointer is RE-LOADED from 0x47e4d4 after the csmemset call
 * (0x17d963) rather than reusing the pre-call value or csmemset's EAX return
 * -- the reload is reproduced here to preserve the original's shape.
 * The original's `ADD ESP,0xc` for the cdecl cleanup is scheduled between the
 * +0x6c and +0x70 stores; that is MSVC scheduling, not an ordering
 * requirement.  No string or PDB evidence names this function, so the FUN_
 * name is kept. */
void FUN_0017d950(void)
{
  void *globals;

  globals = cinematic_screen_effect_globals;
  if (globals != (void *)0) {
    csmemset(globals, 0, 0x78);
    globals = cinematic_screen_effect_globals;
    *(float *)((char *)globals + 0x64) = 1.0f;
    *(float *)((char *)globals + 0x68) = 1.0f;
    *(float *)((char *)globals + 0x6c) = 1.0f;
    *(float *)((char *)globals + 0x70) = 1.0f;
  }
}

/* 0x17d980: the whole function body is a single RET -- byte C3 at
 * 0x17d980, bounds 0x17d980..0x17d981, followed by 15 NOP bytes of padding to
 * the next 16-byte slot.  No prologue, no frame, no callee: this is a
 * genuinely empty function in the shipped debug build, not a placeholder
 * for unrecovered logic.  Reached by a single CALL at 0x155070.  Nothing in the binary
 * names it or shows what it did in a build where it was non-empty. */
void FUN_0017d980(void)
{
}

/* 0x17d990: the whole function body is a single RET -- byte C3 at
 * 0x17d990, bounds 0x17d990..0x17d991, followed by 15 NOP bytes of padding to
 * the next 16-byte slot.  No prologue, no frame, no callee: this is a
 * genuinely empty function in the shipped debug build, not a placeholder
 * for unrecovered logic.  Reached by a single CALL at 0x155bc2.  Nothing in the binary
 * names it or shows what it did in a build where it was non-empty. */
void FUN_0017d990(void)
{
}

/* 0x17d9a0: store one float into the cinematic screen-effect globals' float
 * group at +0x64.  The index is read as a signed WORD (MOV AX,word ptr
 * [EBP+0x8] at 0x17d9ad) and bounds-checked against [0,4) with signed
 * compares (TEST AX,AX / JL and CMP AX,0x4 / JGE, both jumping to the shared
 * exit at 0x17d9c6), then sign-extended (MOVSX EAX,AX) to index a dword-scaled
 * slot.  The value is a float: FLD float ptr [EBP+0xc] (0x17d9bc) / FSTP float
 * ptr [ECX+EAX*4+0x64] (0x17d9c2) -- this is direct evidence that the four
 * slots at +0x64..+0x70 written by FUN_0017d950 are floats, not just an
 * inference from the 0x3f800000 bit pattern stored there.
 * Guarded on the pointer being non-null (MOV ECX,[0x47e4d4] / TEST ECX,ECX /
 * JZ 0x17d9c6).  The global is loaded ONCE into ECX and reused for the store;
 * the decompile's separate DAT_0047e4d4 read in the if-condition is decompiler
 * noise, not a second load.  No callees, no callers or xrefs in the binary,
 * and no string or PDB evidence names this function or the indexed field
 * group -- the FUN_ name is kept and the slot meanings stay unknown. */
void FUN_0017d9a0(int16_t param_1, float param_2)
{
  void *globals;

  globals = cinematic_screen_effect_globals;
  if (((globals != (void *)0) && (param_1 >= 0)) && (param_1 < 4)) {
    *(float *)((char *)globals + 0x64 + param_1 * 4) = param_2;
  }
}

/* 0x17d9d0: read one float back out of the cinematic screen-effect globals'
 * float group at +0x64 -- the getter counterpart to the setter FUN_0017d9a0
 * directly above, with the identical guard shape.  The index is read as a
 * signed WORD (MOV AX,word ptr [EBP+0x8] at 0x17d9e3) and bounds-checked
 * against [0,4) with signed compares (TEST AX,AX / JL and CMP AX,0x4 / JGE,
 * both jumping to the shared exit at 0x17d9fb), then sign-extended
 * (MOVSX EAX,AX) to index a dword-scaled slot.
 * ST(0) is live at the RET with no FSTP and no pop, so this returns a float:
 * kb.json's draft `void (void)` prototype was wrong on both the return type
 * and the parameter list and is corrected here.  The six callers are all
 * UNCONDITIONAL_CALL from FUN_000defb0 (0xdf197, 0xdf1bb, 0xdf1df, 0xdf264,
 * 0xdf28b, 0xdf2b2), which is not lifted yet, so the decl change disturbs no
 * existing call site.
 * The original loads the 0.0f constant at [0x002533c0] unconditionally first
 * (FLD at 0x17d9d9) and, on the in-range path, discards it (FSTP ST0) before
 * loading the slot -- so the fallback is 0.0f; that is spelled here as a
 * result accumulator to keep the load-then-replace shape.  Guarded on the
 * pointer being non-null (MOV ECX,[0x47e4d4] / TEST ECX,ECX / JZ 0x17d9fb);
 * the global is loaded ONCE into ECX.
 * No callees.  No string or PDB evidence names this function or the indexed
 * field group -- the FUN_ name is kept and the slot meanings stay unknown. */
float FUN_0017d9d0(int16_t param_1)
{
  void *globals;
  float result;

  result = 0.0f;
  globals = cinematic_screen_effect_globals;
  if (((globals != (void *)0) && (param_1 >= 0)) && (param_1 < 4)) {
    result = *(float *)((char *)globals + 0x64 + param_1 * 4);
  }
  return result;
}

/* 0x17da00.  Latches the cinematic screen-effect globals block allocated by
 * rasterizer_screen_effects_initialize (0x17d910): optionally zero-fills the
 * first 0x38 bytes and sets the byte flag at 0x39, then always sets the byte
 * flag at 0x38.  The zero-fill runs when the caller's byte argument is
 * non-zero, or when the 0x39 flag is still clear.
 *     0017da03: MOV  EAX,[0x0047e4d4]
 *     0017da08: TEST EAX,EAX
 *     0017da0a: JZ   0x0017da34        ; no-op when never allocated
 *     0017da0c: MOV  CL,byte ptr [EBP + 0x8]
 *     0017da0f: TEST CL,CL
 *     0017da11: JNZ  0x0017da1a
 *     0017da13: MOV  CL,byte ptr [EAX + 0x39]
 *     0017da16: TEST CL,CL
 *     0017da18: JNZ  0x0017da30        ; skip the reset, keep the old EAX
 *     0017da1a: PUSH 0x38
 *     0017da1c: PUSH 0x0
 *     0017da1e: PUSH EAX
 *     0017da1f: CALL 0x0008db80        ; csmemset
 *     0017da24: MOV  EAX,[0x0047e4d4]  ; re-read AFTER the call
 *     0017da29: ADD  ESP,0xc
 *     0017da2c: MOV  byte ptr [EAX + 0x39],0x1
 *     0017da30: MOV  byte ptr [EAX + 0x38],0x1
 * The 0x38 store has two predecessors and merges the pre-call EAX with the
 * reloaded one, so the pointer is held in a local rather than re-read at every
 * use.  Only 0x38 bytes are cleared out of the 0x78-byte block, so this is a
 * partial reset; the block layout and the meaning of the two flag bytes are
 * unknown and the offsets stay raw.  The only xref is the CALL at 0x0c3681 in
 * FUN_000c3660 (hs.c).  The one-byte stack argument is proven by this
 * function's own MOV CL,byte ptr [EBP + 0x8] and matches the kb decl. */
void FUN_0017da00(char param_1)
{
  void *globals;

  globals = *(void **)0x47e4d4;
  if (globals != 0) {
    if (param_1 != 0 || *(char *)((char *)globals + 0x39) == 0) {
      csmemset(globals, 0, 0x38);
      globals = *(void **)0x47e4d4;
      *(char *)((char *)globals + 0x39) = 1;
    }
    *(char *)((char *)globals + 0x38) = 1;
  }
}

/* 0x17da40: arm a cinematic screen effect -- seed the record at
 * cinematic_screen_effect_globals with its two ids, clear the running state,
 * store two float parameters, and stamp the start/end times off the game
 * clock.  Guarded on the globals pointer being non-null (MOV EAX,[0x47e4d4] /
 * XOR ECX,ECX / CMP EAX,ECX / JE 0x17da9e, the shared epilogue).
 *
 * Signature is caller-confirmed, not just kb-declared.  The single call site
 * at 0xc36de sets the arguments up as
 *   SUB ESP,0xc / FLD [EAX+0x10] / FSTP [ESP+0x8]   -> param_5
 *                 FLD [EAX+0xc]  / FSTP [ESP+0x4]   -> param_4
 *                 FLD [EAX+0x8]  / FSTP [ESP]       -> param_3
 *   XOR EDX,EDX / MOV DX,[EAX+0x4] / PUSH EDX       -> param_2 (zero-extended)
 *   MOVSX EAX,WORD PTR [EAX]       / PUSH EAX       -> param_1 (sign-extended)
 * so the three trailing slots are genuinely floats reserved by SUB ESP and
 * filled by FSTP -- the values are FPU results, not the pushed dwords a
 * decompiler would report.  param_1 is signed (MOVSX) and param_2 unsigned
 * (XOR/MOV DX), which is what fixes the int16_t / uint16_t split.
 *
 * Store-offset table, taken from the raw MOV destinations rather than the
 * decompiler, because MSVC interleaves them and the order below is the
 * order the reference emits (it is NOT ascending):
 *   +0x23  byte   0            (MOV [EAX+0x23],CL  with CL==0)
 *   +0x24  word   0
 *   +0x28  dword  0
 *   +0x2c  dword  0
 *   +0x30  dword  0
 *   +0x34  dword  0
 *   +0x00  word   param_1
 *   +0x02  word   param_2
 *   +0x3c  float  param_3
 *   +0x40  float  param_4
 *   +0x44  float  start
 *   +0x48  float  start + param_5
 * The +0x23 store is a byte and +0x24 a word -- widening either to a dword
 * would clobber neighbouring state.
 *
 * The globals pointer is RELOADED from 0x47e4d4 after the call (MOV EAX,
 * ds:0x47e4d4 at 0x17da88), not carried in a callee-saved register across it,
 * so the second half re-reads the macro instead of reusing the local.
 *
 * The time stamp is FILD of game_time_get()'s int result spilled to [EBP-4],
 * multiplied by the .rdata literal at 0x2546a4 = 0x3d088889 = 1/30, i.e. ticks
 * converted to seconds at the Xbox 30Hz tick rate.  It must stay a multiply by
 * the reciprocal: dividing by 30.0f would emit FDIV where the reference has
 * FMUL.  This is a distinct .rdata literal from the TICKS_PER_SECOND global at
 * 0x253394, so the macro in common.h is deliberately not used here.
 * FLD ST(0) duplicates the product so +0x44 gets the start time and +0x48 the
 * same value plus param_5 -- param_5 is therefore a duration in seconds and
 * +0x48 an end time, though nothing in the binary names either field.
 *
 * Explicit unknowns: the meaning of the two id words at +0x00/+0x02, of the
 * cleared state at +0x23..+0x37, and of the two floats at +0x3c/+0x40.  No
 * string or PDB evidence names this function, so the FUN_ name is kept.
 *
 * Match ceiling: VC71 measures 95.4% (32/33 insns) and the whole gap is one
 * x87 peephole clang picks differently.  Where the reference duplicates the
 * product and pops it into the field (FLD ST(0) / FSTP [EAX+0x44]), clang
 * emits the single store-and-keep FST [EAX+0x44].  Both leave the same
 * un-narrowed 80-bit product in ST(0) for the following FADD, so the stored
 * float and the +0x48 sum are bit-identical -- this is an instruction-count
 * difference only, not a precision or ordering difference, and it is not
 * worth chasing with the permuter. */
void FUN_0017da40(int16_t param_1, uint16_t param_2, float param_3,
                  float param_4, float param_5)
{
  void *globals;
  int now;
  float start;

  globals = cinematic_screen_effect_globals;
  if (globals != (void *)0) {
    *(char *)((char *)globals + 0x23) = 0;
    *(int16_t *)((char *)globals + 0x24) = 0;
    *(int *)((char *)globals + 0x28) = 0;
    *(int *)((char *)globals + 0x2c) = 0;
    *(int *)((char *)globals + 0x30) = 0;
    *(int *)((char *)globals + 0x34) = 0;
    *(int16_t *)globals = param_1;
    *(uint16_t *)((char *)globals + 0x2) = param_2;
    *(float *)((char *)globals + 0x3c) = param_3;
    *(float *)((char *)globals + 0x40) = param_4;
    now = game_time_get();
    globals = cinematic_screen_effect_globals;
    start = (float)now * (1.0f / 30.0f);
    *(float *)((char *)globals + 0x44) = start;
    *(float *)((char *)globals + 0x48) = start + param_5;
  }
}

/* 0x17dab0.  Arms a cinematic screen effect in the 0x78-byte globals block
 * allocated by rasterizer_screen_effects_initialize (0x17d910), and is a
 * no-op when the block was never allocated.  It clears the same flag/state
 * run that rasterizer_screen_effect_set_video (0x17db40) clears -- the byte
 * at 0x23, the word at 0x24 and the four dwords at 0x28..0x34 -- then records
 * four caller dwords at 0x4c..0x58, timestamps the effect at 0x5c/0x60, and
 * finally writes the caller's byte at 0x20 with 0x21/0x22 cleared.
 *     0017dab4: MOV  EAX,[0x0047e4d4]
 *     0017daba: XOR  EBX,EBX
 *     0017dabc: CMP  EAX,EBX
 *     0017dabe: JZ   0x0017db18        ; no-op when never allocated
 *     0017dac0: MOV  ECX,dword ptr [EBP + 0x8]
 *     0017dac3: MOV  EDX,dword ptr [EBP + 0xc]
 *     0017dac6: MOV  byte ptr [EAX + 0x23],BL
 *     0017dac9: MOV  word ptr [EAX + 0x24],BX
 *     0017dacd: MOV  dword ptr [EAX + 0x28],EBX
 *     0017dad0: MOV  dword ptr [EAX + 0x2c],EBX
 *     0017dad3: MOV  dword ptr [EAX + 0x30],EBX
 *     0017dad6: MOV  dword ptr [EAX + 0x34],EBX
 *     0017dad9: MOV  dword ptr [EAX + 0x4c],ECX
 *     0017dadc: MOV  ECX,dword ptr [EBP + 0x10]
 *     0017dadf: MOV  dword ptr [EAX + 0x50],EDX
 *     0017dae2: MOV  EDX,dword ptr [EBP + 0x14]
 *     0017dae5: MOV  dword ptr [EAX + 0x54],ECX
 *     0017dae8: MOV  dword ptr [EAX + 0x58],EDX
 *     0017daeb: CALL 0x000b5aa0        ; game_time_get()
 *     0017daf0: MOV  CL,byte ptr [EBP + 0x18]
 *     0017daf3: MOV  dword ptr [EBP + -0x4],EAX
 *     0017daf6: FILD dword ptr [EBP + -0x4]
 *     0017daf9: MOV  EAX,[0x0047e4d4]  ; re-read AFTER the call
 *     0017dafe: FMUL float ptr [0x002546a4] ; seconds-per-tick
 *     0017db04: FLD  ST0
 *     0017db06: FSTP float ptr [EAX + 0x5c]
 *     0017db09: FADD float ptr [EBP + 0x1c]
 *     0017db0c: FSTP float ptr [EAX + 0x60]
 *     0017db0f: MOV  byte ptr [EAX + 0x20],CL
 *     0017db12: MOV  byte ptr [EAX + 0x21],BL
 *     0017db15: MOV  byte ptr [EAX + 0x22],BL
 * The global is re-read after the call and only there, so it is held in a
 * local across each call-free run of stores, exactly as sequenced above.
 *
 * The 0x5c/0x60 pair is the same seconds-per-tick timestamp idiom as
 * FUN_0017d8f0 above: game_time_get() spilled to a slot, FILD'd, and
 * multiplied by the float at 0x2546a4.  Note the FLD ST0 / FSTP / FADD
 * ordering: the *narrowed* product is stored at +0x5c, but +0x60 adds
 * param_6 to the still-80-bit x87 copy, not to a reload of +0x5c.  Under
 * -mno-sse clang keeps the float local wide and reproduces that; adding an
 * explicit narrowing, a `volatile` local, or a store/reload here would
 * silently change the +0x60 value, and neither VC71 (mnemonic-only) nor the
 * equivalence float tolerance would report it.
 *
 * The four dwords at 0x4c..0x58 are copied raw -- there is no FLD/FSTP for
 * them in the original -- so the three float parameters go through a dword
 * pun like FUN_0017db20 below rather than through float lvalues, which would
 * double-round.  (Our clang build still lowers the bit-copy through
 * FLDS/FSTPS because the float parameters are x87-live at entry; that is
 * value-identical, not a second semantic.)  param_5 is a one-byte value,
 * proven by MOV CL,byte ptr [EBP + 0x18] / MOV byte ptr [EAX + 0x20],CL.
 *
 * The block layout is unknown and the meaning of the stored slots is
 * unproven, so the offsets stay raw and the name stays FUN_.  The only xref
 * is the CALL at 0x0c3743 in FUN_000c3700 (hs.c), which forwards a six-slot
 * evaluated-argument record.
 *
 * MATCH-SENSITIVE: `arg_slot` and `end_slot` are pure address hoists of
 * globals+0x4c and globals+0x60, computed one store early to reproduce the
 * reference's address-materialization schedule (85.7% -> 96.1% VC71).  They
 * change no side effect and no ordering -- both are derived from the same
 * already-loaded `globals` the neighbouring stores use -- but folding them
 * back into the store expressions costs the match. */
void FUN_0017dab0(int param_1, float param_2, float param_3, float param_4,
                  char param_5, float param_6)
{
  void *globals;
  float timestamp;
  int *arg_slot;
  float *end_slot;

  globals = cinematic_screen_effect_globals;
  if (globals != 0) {
    *(char *)((char *)globals + 0x23) = 0;
    *(short *)((char *)globals + 0x24) = 0;
    *(int *)((char *)globals + 0x28) = 0;
    *(int *)((char *)globals + 0x2c) = 0;
    *(int *)((char *)globals + 0x30) = 0;
    arg_slot = (int *)((char *)globals + 0x4c);
    *(int *)((char *)globals + 0x34) = 0;
    *arg_slot = param_1;
    *(unsigned int *)((char *)globals + 0x50) = *(unsigned int *)&param_2;
    *(unsigned int *)((char *)globals + 0x54) = *(unsigned int *)&param_3;
    *(unsigned int *)((char *)globals + 0x58) = *(unsigned int *)&param_4;
    timestamp = (float)game_time_get() * *(float *)0x2546a4;
    globals = cinematic_screen_effect_globals;
    end_slot = (float *)((char *)globals + 0x60);
    *(float *)((char *)globals + 0x5c) = timestamp;
    *end_slot = timestamp + param_6;
    *(char *)((char *)globals + 0x20) = param_5;
    *(char *)((char *)globals + 0x21) = 0;
    *(char *)((char *)globals + 0x22) = 0;
  }
}

/* 0x17db20.  Stores three dwords into the cinematic screen-effect globals
 * block allocated by rasterizer_screen_effects_initialize (0x17d910), at
 * offsets 0x14/0x18/0x1c; no-op when the block was never allocated.
 *     0017db23: MOV  EAX,[0x0047e4d4]
 *     0017db28: TEST EAX,EAX
 *     0017db2a: JZ   0x0017db3e        ; no-op when never allocated
 *     0017db2c: MOV  ECX,dword ptr [EBP + 0x8]
 *     0017db2f: MOV  EDX,dword ptr [EBP + 0xc]
 *     0017db32: MOV  [EAX + 0x14],ECX
 *     0017db35: MOV  ECX,dword ptr [EBP + 0x10]
 *     0017db38: MOV  [EAX + 0x18],EDX
 *     0017db3b: MOV  [EAX + 0x1c],ECX
 * The global is loaded once and all three stores use that pointer (there is
 * no intervening call), so it is held in a local.  Every argument is copied
 * as a raw dword -- there is no FLD/FSTP anywhere in the original -- so the
 * two float parameters from the kb decl are copied through a dword pun rather
 * than assigned through float lvalues; that spelling is what keeps the plain
 * MOV [EAX+0x18]/[EAX+0x1c] store shape in the reference lane.  (Our clang
 * build still lowers the same bit-copy through FLDS/FSTPS because the float
 * parameters are x87-live at entry; that is value-identical, not a second
 * semantic.)  The block layout is unknown; the meaning of the
 * three stored dwords is unproven, so the offsets stay raw.  The only xref is
 * the CALL at 0x0c378f in FUN_000c3760. */
void FUN_0017db20(int param_1, float param_2, float param_3)
{
  void *globals;

  globals = *(void **)0x47e4d4;
  if (globals != 0) {
    *(int *)((char *)globals + 0x14) = param_1;
    *(unsigned int *)((char *)globals + 0x18) = *(unsigned int *)&param_2;
    *(unsigned int *)((char *)globals + 0x1c) = *(unsigned int *)&param_3;
  }
}

/* rasterizer_screen_effect_set_video (0x17db40).  Arms the cinematic
 * "video" screen effect in the 0x78-byte globals block allocated by
 * rasterizer_screen_effects_initialize (0x17d910): clears the first 0x38
 * bytes, zeroes the ten dwords at 0x3c..0x60, raises the flag byte at 0x23,
 * records the caller's mode word at 0x24, and caches two bitmap-data pointers
 * at 0x28 and 0x34 resolved from the two global bitmap tag indices held in the
 * rasterizer globals block at 0x476204 (+0x128 and +0x138).  When either index
 * is -1 the effect is refused with the .rdata error string at 0x2af380.
 * The assert literals (0x29da1c / 0x2af334 / line 0xe1) name Bungie's TU
 * c:\halo\SOURCE\rasterizer\rasterizer_cinematics.c; kb.json maps this address
 * to rasterizer_sprites.obj.  The only xref is the CALL at 0x0c37d9 in
 * FUN_000c37b0 (hs.c).
 *
 * Global re-reads are reproduced exactly as the original sequences them --
 * [0x47e4d4] is loaded at 0x17db43 (null test), 0x17db9b (csmemset argument),
 * 0x17dba9 (base for the 0x3c..0x60 / 0x23 / 0x24 stores), 0x17dc01 (base for
 * 0x28/0x2c/0x30) and 0x17dc3f (base for 0x34); [0x476204] is loaded at
 * 0x17db53 (null test AND both -1 compares, not reloaded between them),
 * 0x17dbdb (+0x128) and 0x17dc1a (+0x138) -- so each is held in a local only
 * across call-free runs of stores.
 *
 * The mode parameter arrives in a full dword slot but only its low word is
 * consumed: 0017dbae MOV CX,word ptr [EBP+0x8] / 0017dbd7 MOV word ptr
 * [EAX+0x24],CX.  The kb decl keeps `int` (the hs.c caller at 0x0c37d9 widens
 * a uint16 field into the slot); the truncation is expressed at the store.
 * The float parameter is copied as a raw dword -- there is no FLD/FSTP in the
 * original -- so it goes through a dword pun like FUN_0017db20 above, while
 * the 1.0f immediate at +0x30 (0x3f800000) is a plain float store.
 *
 * Both tag lookups reuse the already-pushed 0x30/0 slots as
 * tag_block_get_element's 2nd/3rd arguments (ADD ESP,8 pops only tag_get's own
 * two); C has no spelling for that sharing, so the nested call is written
 * plainly.  The ARG_COUNT note on the error() call site is the variadic
 * ellipsis being counted as a parameter -- the disassembly is
 * PUSH 0x2af380 / PUSH 0x2 / CALL / ADD ESP,8, i.e. two arguments.
 * The 0x78-byte block layout is unknown, so the offsets stay raw. */
void rasterizer_screen_effect_set_video(int mode, float value)
{
  char *globals;
  char *raster_globals;
  void *bitmap_data;

  if (*(void **)0x47e4d4 == 0) {
    return;
  }
  raster_globals = *(char **)0x476204;
  if (raster_globals == 0) {
    display_assert("global_rasterizer_data",
                   "c:\\halo\\SOURCE\\rasterizer\\rasterizer_cinematics.c",
                   0xe1, 1);
    system_exit(-1);
  }
  if (*(int *)(raster_globals + 0x128) == -1 ||
      *(int *)(raster_globals + 0x138) == -1) {
    error(2, "### ERROR cinematics failed to set video mode; global bitmaps "
             "are not set");
    return;
  }
  csmemset(*(void **)0x47e4d4, 0, 0x38);
  globals = *(char **)0x47e4d4;
  *(int *)(globals + 0x3c) = 0;
  *(int *)(globals + 0x40) = 0;
  *(int *)(globals + 0x44) = 0;
  *(int *)(globals + 0x48) = 0;
  *(int *)(globals + 0x4c) = 0;
  *(int *)(globals + 0x50) = 0;
  *(int *)(globals + 0x54) = 0;
  *(int *)(globals + 0x58) = 0;
  *(int *)(globals + 0x5c) = 0;
  *(int *)(globals + 0x60) = 0;
  *(char *)(globals + 0x23) = 1;
  *(short *)(globals + 0x24) = (short)mode;
  raster_globals = *(char **)0x476204;
  bitmap_data = tag_block_get_element(
    (char *)tag_get(0x6269746d /* 'bitm' */, *(int *)(raster_globals + 0x128)) +
      0x60,
    0, 0x30);
  globals = *(char **)0x47e4d4;
  *(void **)(globals + 0x28) = bitmap_data;
  *(unsigned int *)(globals + 0x2c) = *(unsigned int *)&value;
  *(float *)(globals + 0x30) = 1.0f;
  raster_globals = *(char **)0x476204;
  bitmap_data = tag_block_get_element(
    (char *)tag_get(0x6269746d /* 'bitm' */, *(int *)(raster_globals + 0x138)) +
      0x60,
    0, 0x30);
  globals = *(char **)0x47e4d4;
  *(void **)(globals + 0x34) = bitmap_data;
}

/* FUN_0017dc60 (0x17dc60).  Clears the byte at +0x38 of the 0x78-byte screen
 * effect globals block held at [0x47e4d4], guarded by a null test on the block
 * pointer.  The whole body is five instructions:
 *
 *     0017dc60: MOV  EAX,[0x0047e4d4]
 *     0017dc65: TEST EAX,EAX
 *     0017dc67: JZ   0x0017dc6d
 *     0017dc69: MOV  byte ptr [EAX + 0x38],0x0
 *     0017dc6d: RET
 *
 * The global is loaded ONCE (single MOV, reused as the store base), so it is
 * held in a local rather than re-read.  No callers or callees are recorded in
 * the Ghidra artifact for this address, and no assert/string literal names the
 * function, so the name and the meaning of the +0x38 byte stay unknown.  Note
 * that +0x38 is deliberately just past the 0x38 bytes that
 * rasterizer_screen_effect_set_video (0x17db40) clears with csmemset, so it is
 * a separate flag from that effect's own state -- but nothing in the binary
 * proves what it selects, so no name is asserted here. */
void FUN_0017dc60(void)
{
  char *globals;

  globals = *(char **)0x47e4d4;
  if (globals != 0) {
    *(globals + 0x38) = 0;
  }
}

/* rasterizer_screen_effect_get_cinematic_parameters (0x17dc70): advance the
 * cinematic screen effect for this frame.  Two independent fade ramps are
 * evaluated against game time -- one at +0x44/+0x48 driving the convolution
 * blend, one at +0x5c/+0x60 driving the two saturation/desaturation scalars --
 * then three scalar interpolations write the live values into +0x04, +0x0c and
 * +0x10, the 12-byte block at +0x14 is refreshed from a default when it still
 * matches the other default, and values that have reached their inert endpoint
 * are snapped to zero.
 *
 * Names: the TU is c:\halo\SOURCE\rasterizer\rasterizer_cinematics.c and the
 * function name is CEA PDB line-containment evidence (high confidence).  The
 * struct at 0x47e4d4 is the 0x78-byte cinematic_screen_effect_globals block
 * allocated by rasterizer_screen_effects_initialize; its layout has no string
 * or PDB evidence, so every field is addressed by raw offset here and no
 * meaning beyond the arithmetic above is claimed.  Ghidra typed the global
 * `undefined2 *`, which HALVES every offset in its decompile -- all offsets
 * below are read from the disassembly instead.
 *
 * The single stack parameter is UNKNOWN.  The binary takes one dword at
 * [EBP+8] and returns it unchanged on the early-out (MOV EAX,[EBP+0x8] at
 * 0x17deb4), while the main path returns the globals pointer (MOV EAX,ECX at
 * 0x17dead); MSVC also reused that same slot as the home for the second fade
 * scalar.  kb.json's `void *effect` is kept as-is: it matches the observed
 * one-dword-in/one-dword-out ABI, and the sole caller (UNCONDITIONAL_CALL from
 * 0x170ccf in FUN_00170c90) is not lifted, so nothing depends on a better
 * guess.  The second fade scalar is spelled as an ordinary local here rather
 * than punned through the parameter slot.
 *
 * Branch senses are all x87 parity tests and were derived from the FNSTSW
 * masks, not from the decompile (which renders them as the `a < b == (a == b)`
 * XOR noise):
 *   0x17dc97 TEST AH,0x44 / JNP  -> taken iff equal
 *   0x17dcc7 TEST AH,0x05 / JP   -> taken iff ratio >= 0.0
 *   0x17dcf0 TEST AH,0x41 / JZ   -> taken iff ratio > 1.0
 *   0x17de35 TEST AH,0x41 / JP   -> taken iff +0x04 >  [0x253f44]
 *   0x17de82 TEST AH,0x41 / JP   -> skip  iff field  >  [0x253f44]
 *   0x17dea2 TEST AH,0x01 / JNZ  -> skip  iff scalar <  1.0
 *
 * Global loads are the binary's.  0x47e4d4 is re-read after every call because
 * the reference reloads ECX there (0x17dca4, 0x17dcdb, 0x17dcff, 0x17dd2d,
 * 0x17dd68, 0x17dd8c, 0x17ddba, 0x17ddd4, 0x17ddf1, 0x17de03, 0x17de71); it is
 * NOT reloaded before the first scalars_interpolate (0x17dda5), and the reload
 * at 0x17de71 lives inside the window-check arm only, so neither is hoisted.
 * The reloads inside the ratios are spelled as the macro directly in the
 * denominator so that no intermediate float local is introduced: the reference
 * goes FDIVP straight into FCOMP with no FSTP, so those ratios are legitimately
 * 80-bit and a narrowed temporary could flip a compare.  Only the two fade
 * scalars are real floats (FSTP at 0x17dd0c / 0x17dd99).
 *
 * Constants (raw addresses; no evidence names them): [0x2546a4] seconds per
 * game tick, [0x2533c0] 0.0f, [0x2533c8] 1.0f, [0x253f44] a small threshold.
 * [0x2ee710] and [0x2ee718] are pointers that are LOADED THROUGH (MOV ECX,
 * dword ptr [0x2ee710]), not addresses of the data.
 *
 * The FATAL_ERROR at 0x17de62 uses the binary's own file/line literals
 * (rasterizer_cinematics.c, line 0x150), so it is spelled with the
 * explicit-location assert macro. */
void *rasterizer_screen_effect_get_cinematic_parameters(void *effect)
{
  void *globals;
  const float *fade_start;
  const void *defaults;
  float *record;
  int matched;
  int tick;
  float convolution_blend;
  float saturation_blend;

  globals = cinematic_screen_effect_globals;
  if (globals == (void *)0 || *((char *)globals + 0x38) == '\0') {
    return effect;
  }

  if (*(float *)((char *)globals + 0x48) ==
      *(float *)((char *)globals + 0x44)) {
    goto convolution_full;
  }
  fade_start = (const float *)((char *)globals + 0x44);
  tick = game_time_get();
  globals = cinematic_screen_effect_globals;
  if (((float)tick * *(float *)0x2546a4 - *fade_start) /
        (*(float *)((char *)globals + 0x48) - *fade_start) <
      *(float *)0x2533c0) {
    convolution_blend = 0.0f;
  } else {
    fade_start = (const float *)((char *)globals + 0x44);
    if ((FUN_0017d8f0() - *fade_start) /
          (*(float *)((char *)cinematic_screen_effect_globals + 0x48) -
           *fade_start) >
        *(float *)0x2533c8) {
      goto convolution_full;
    }
    fade_start = (const float *)((char *)globals + 0x44);
    convolution_blend =
      (FUN_0017d8f0() - *fade_start) /
      (*(float *)((char *)cinematic_screen_effect_globals + 0x48) -
       *fade_start);
  }
  goto convolution_done;
convolution_full:
  convolution_blend = 1.0f;
convolution_done:

  if (*(float *)((char *)globals + 0x60) ==
      *(float *)((char *)globals + 0x5c)) {
    goto saturation_full;
  }
  fade_start = (const float *)((char *)globals + 0x5c);
  tick = game_time_get();
  globals = cinematic_screen_effect_globals;
  if (((float)tick * *(float *)0x2546a4 - *fade_start) /
        (*(float *)((char *)globals + 0x60) - *fade_start) <
      *(float *)0x2533c0) {
    saturation_blend = 0.0f;
  } else {
    fade_start = (const float *)((char *)globals + 0x5c);
    if ((FUN_0017d8f0() - *fade_start) /
          (*(float *)((char *)cinematic_screen_effect_globals + 0x60) -
           *fade_start) >
        *(float *)0x2533c8) {
      goto saturation_full;
    }
    fade_start = (const float *)((char *)globals + 0x5c);
    saturation_blend =
      (FUN_0017d8f0() - *fade_start) /
      (*(float *)((char *)cinematic_screen_effect_globals + 0x60) -
       *fade_start);
  }
  goto saturation_done;
saturation_full:
  saturation_blend = 1.0f;
saturation_done:

  scalars_interpolate(*(float *)((char *)globals + 0x3c),
                      *(float *)((char *)globals + 0x40), convolution_blend,
                      (float *)((char *)globals + 0x4));

  globals = cinematic_screen_effect_globals;
  scalars_interpolate_and_clamp_0_to_1(
    *(float *)((char *)globals + 0x4c), *(float *)((char *)globals + 0x50),
    saturation_blend, (float *)((char *)globals + 0xc));

  globals = cinematic_screen_effect_globals;
  scalars_interpolate_and_clamp_0_to_1(
    *(float *)((char *)globals + 0x54), *(float *)((char *)globals + 0x58),
    saturation_blend, (float *)((char *)globals + 0x10));

  matched = csmemcmp((char *)cinematic_screen_effect_globals + 0x14,
                     *(const void **)0x2ee710, 0xc);
  globals = cinematic_screen_effect_globals;
  if (matched == 0) {
    defaults = *(const void **)0x2ee718;
    record = (float *)((char *)globals + 0x14);
    *(int *)record = *(const int *)defaults;
    *(int *)((char *)record + 0x4) =
      *(const int *)((const char *)defaults + 0x4);
    *(int *)((char *)record + 0x8) =
      *(const int *)((const char *)defaults + 0x8);
  }

  if (*(float *)((char *)globals + 0x4) <= *(float *)0x253f44) {
    *(int *)((char *)globals + 0x4) = 0;
    *(short *)((char *)globals + 0x2) = 0;
    *(short *)globals = 0;
  } else {
    assert_halt_msg_at("### FATAL_ERROR screen effects can't use convolution "
                       "when main_get_window_count>1\r\nmaybe you forgot to "
                       "turn off the cinematic screen effect?",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_cinematics.c",
                       0x150, main_get_window_count() <= 1);
    globals = cinematic_screen_effect_globals;
  }

  if (*(float *)((char *)globals + 0xc) <= *(float *)0x253f44 &&
      *(float *)((char *)globals + 0x10) <= *(float *)0x253f44 &&
      saturation_blend >= *(float *)0x2533c8) {
    *(int *)((char *)globals + 0xc) = 0;
    *(int *)((char *)globals + 0x10) = 0;
  }

  return globals;
}

/* FUN_0017dec0 (0x17dec0).  Stores its single dword argument into +0x74 of the
 * 0x78-byte screen effect globals block held at [0x47e4d4], guarded by a null
 * test on the block pointer.  The whole body is nine instructions:
 *
 *     0017dec0: PUSH EBP
 *     0017dec1: MOV  EBP,ESP
 *     0017dec3: MOV  EAX,[0x0047e4d4]
 *     0017dec8: TEST EAX,EAX
 *     0017deca: JZ   0x0017ded2
 *     0017decc: MOV  ECX,dword ptr [EBP + 0x8]
 *     0017decf: MOV  dword ptr [EAX + 0x74],ECX
 *     0017ded2: POP  EBP
 *     0017ded3: RET
 *
 * The global is loaded ONCE (single MOV, reused as the store base), so it is
 * held in a local rather than re-read, matching FUN_0017dc60 above.  The
 * argument is moved through ECX as a plain dword -- there is no FLD/FSTP -- so
 * the kb decl's `int` is kept even though the neighbouring slots +0x64..+0x70
 * that FUN_0017d950 initialises are floats; adjacency is a lead, not evidence.
 * The Ghidra artifact records no callers, no callees and no string/assert for
 * this address, so both the function name and the meaning of the +0x74 field
 * are unproven and stay raw. */
void FUN_0017dec0(int param_1)
{
  char *globals;

  globals = *(char **)0x47e4d4;
  if (globals != 0) {
    *(int *)(globals + 0x74) = param_1;
  }
}

/* 0x17dee0: read back the cinematic screen-effect scalar at +0x74, clamped to
 * a default floor.  The default at [0x2af1ac] is loaded first and returned
 * unchanged when the globals pointer is null or when the stored scalar is not
 * strictly greater than [0x2533c0] (0.0f); only a strictly greater value
 * replaces it.
 *
 * Branch sense is read from the FNSTSW mask, not the decompile (Ghidra renders
 * this whole body as an empty `return;` because it does not model the x87
 * return in ST0): 0x17defb TEST AH,0x41 / JNZ -> taken iff C0 (less) or C3
 * (equal) is set, i.e. iff the scalar is less-or-equal OR unordered, so the
 * fall-through is strictly-greater and `>` reproduces the NaN case too.
 *
 * The field is deliberately spelled twice: the reference does NOT reuse the
 * FCOMP operand, it pops the default (FSTP ST0 at 0x17df00) and re-loads
 * [ECX+0x74] (FLD at 0x17df02).  The globals pointer is loaded BEFORE the
 * default (MOV ECX at 0x17dee0 precedes FLD at 0x17dee6), so the two locals
 * are assigned in that order here.
 *
 * +0x74 is a float field: FUN_0017dec0 (0x17dec0) stores one dword into the
 * same slot and this function loads it with FLD.  That store is dword-wide and
 * therefore bit-identical either way, so FUN_0017dec0's `int param_1` decl is
 * left alone.
 *
 * [0x2af1ac] is UNKNOWN -- no string or PDB evidence names it, and its only
 * other appearance (a 4-dword block copy in rasterizer.c) is adjacency, not
 * meaning.  It is kept as a raw address.  The sole xref is an unconditional
 * CALL from 0x15796c in FUN_00157940, which names nothing, so the FUN_ name
 * is kept. */
float FUN_0017dee0(void)
{
  void *globals;
  float value;

  globals = cinematic_screen_effect_globals;
  value = *(const float *)0x2af1ac;
  if (globals != (void *)0 &&
      *(float *)((char *)globals + 0x74) > *(const float *)0x2533c0) {
    value = *(float *)((char *)globals + 0x74);
  }
  return value;
}

/* FUN_0017df10 (0x17df10).  Reserves one slot in a debug-geometry buffer and
 * returns its index, or -1 when either the caller's own per-buffer counter
 * (*count) or the shared global counter [0x47e4f4] has already reached the
 * 0x2000 cap.
 *
 * Ghidra decompiles this as `void` because the return value is parked in ESI:
 *
 *     0017df19: OR   ESI,0xffffffff
 *     ...
 *     0017df49: JNZ  0x17df75          (skips the MOV, keeps EAX = old *count)
 *     0017df73: MOV  EAX,ESI           (overflow path -> -1)
 *
 * EAX still holds the pre-increment `*count` loaded at 0x17df16 on both
 * success paths, so the success result is the index of the slot just claimed.
 * Both compares are signed (JGE at 0x17df21 / 0x17df2d), so the counters are
 * signed int.
 *
 * [0x47e4f4] is loaded twice (CMP at 0x17df23, MOV EDX at 0x17df34) -- the
 * increment is written as a re-read to preserve that shape.  [0x47e4f8] is a
 * one-shot "already warned" byte flag.  [0x3256ba] is the usual 16-bit
 * rasterizer render-mode selector; == 2 bumps the debug counter at 0x5a5540.
 *
 * The error() call pushes exactly two arguments (ADD ESP,0x8 at 0x17df69);
 * the ARG_COUNT hazard on it is the varargs "..." counted as a third param. */
int FUN_0017df10(int *count)
{
  int index;
  int result;
  short *render_mode;
  int *debug_counter;

  result = -1;
  render_mode = (short *)0x3256ba;
  index = *count;
  if (index < 0x2000 && *(int *)0x47e4f4 < 0x2000) {
    *count = index + 1;
    *(int *)0x47e4f4 = *(int *)0x47e4f4 + 1;
    debug_counter = (int *)0x5a5540;
    if (*render_mode == 2) {
      *debug_counter = *debug_counter + 1;
    }
    result = index;
    goto done;
  }
  if (*(char *)0x47e4f8 == 0) {
    error(2, "### WARNING debug geometry buffer overflow");
    *(char *)0x47e4f8 = 1;
  }
done:
  return result;
}

/* VC71 note: the reference keeps the boolean constant 1 in BL across the whole
 * body (MOV BL,0x1 at 0x17df8f, MOV [0x47e4d8],BL / MOV AL,BL at the success
 * exit), which forces PUSH EBX at entry and POP EBX at both exits.  Those four
 * instructions are an MSVC register-allocation choice with no C spelling.
 * Measured: routing both exits through a `bool valid` local (to force the
 * materialize-then-store order the reference uses) moved the score 0.00pp
 * (89.7% before and after), so that candidate was reverted -- do not re-try it.
 *
 * 0x17df80: allocate the three debug-geometry buffers and latch a global
 * "debug buffers valid" flag.  Three identical 0x78000-byte non-zeroed
 * debug_malloc calls store into 0x47e4dc, 0x47e4e4 and 0x47e4ec; if all three
 * succeed the byte flag at 0x47e4d8 is set to 1 and 1 is returned, otherwise
 * error() reports the failure, the flag is cleared and 0 is returned.
 *
 * Return type: the kb decl `bool` is confirmed by the two exits --
 * MOV AL,BL (BL was set to 1 at 0x17df8f) at 0x17dfec and XOR AL,AL at
 * 0x17dfff, i.e. an 8-bit boolean in AL, not a void return.
 *
 * 0x47e4d8 is a BYTE, not an int: the stores are MOV byte ptr
 * [0x0047e4d8],BL (0x17dfe6) and MOV [0x0047e4d8],AL (0x17e001).  This is the
 * same flag class as 0x47e4f8 in FUN_0017df10 above.
 *
 * The __FILE__ argument at 0x2af4b8 is "c:\halo\SOURCE\rasterizer\
 * rasterizer_debug.c" with lines 0x60/0x61/0x62 -- the original TU was
 * rasterizer_debug.c, but kb.json maps this address into
 * rasterizer_sprites.obj, so the string is copied verbatim as binary data and
 * the object mapping is left alone.
 *
 * Call-site audit notes, both artifacts rather than argument bugs:
 *   - debug_malloc "cleanup=12 vs decl=4" is the single ADD ESP,0x30 at
 *     0x17dfcc cleaning all three 4-argument calls at once.
 *   - error "cleanup=2 vs decl=3" is the `...` of
 *     void error(unsigned __int16, const char *, ...) counted as a slot;
 *     exactly two arguments are pushed (PUSH 0x2af48c / PUSH 2 / ADD ESP,8). */
bool FUN_0017df80(void)
{
  *(void **)0x47e4dc = debug_malloc(
    0x78000, false, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x60);
  *(void **)0x47e4e4 = debug_malloc(
    0x78000, false, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x61);
  *(void **)0x47e4ec = debug_malloc(
    0x78000, false, "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x62);
  if (*(void **)0x47e4dc != (void *)0 && *(void **)0x47e4e4 != (void *)0 &&
      *(void **)0x47e4ec != (void *)0) {
    *(char *)0x47e4d8 = 1;
    return 1;
  }
  error(2, "### ERROR failed to allocate debug buffers");
  *(char *)0x47e4d8 = 0;
  return 0;
}

/* Zero-fills four rasterizer-sprite dword globals (0x17e010).  The whole body
 * is XOR EAX,EAX followed by four MOV [imm32],EAX and RET -- no calls, no
 * arguments, no return value.
 *
 * The four dwords are written as signed int to stay consistent with
 * FUN_0017df10 above, which compares [0x47e4f4] with JGE (signed) and uses the
 * same raw-cast form.  Meanings of [0x47e4e0], [0x47e4e8] and [0x47e4f0] are
 * unknown; only their width (dword) and their reset-to-zero role are proven.
 *
 * Explicit unknown: the dwords at 0x47e4e4 and 0x47e4ec lie between the four
 * written slots and are NOT touched here.  Whether they belong to the same
 * record is unproven by this function. */
void FUN_0017e010(void)
{
  *(int *)0x47e4e0 = 0;
  *(int *)0x47e4e8 = 0;
  *(int *)0x47e4f0 = 0;
  *(int *)0x47e4f4 = 0;
}

/* 0x17e030: the whole function body is a single RET -- byte C3 at
 * 0x17e030, bounds 0x17e030..0x17e031, followed by 15 NOP bytes of padding to
 * the next 16-byte slot.  No prologue, no frame, no callee: this is a
 * genuinely empty function in the shipped debug build, not a placeholder
 * for unrecovered logic.  Reached by a single CALL at 0x159056.  Nothing in the binary
 * names it or shows what it did in a build where it was non-empty. */
void FUN_0017e030(void)
{
}

/* Disposes the three debug-geometry buffers (0x17e040).  Guarded by the byte
 * flag at 0x47e4d8: when it is non-zero the three pointers are asserted
 * non-null, freed, and the flag is cleared.
 *
 * The .rdata assert literals name Bungie's TU
 * c:\halo\SOURCE\rasterizer\rasterizer_debug.c and the three fields
 * `debug_data.opaque_triangles` (0x47e4dc), `debug_data.opaque_lines`
 * (0x47e4e4) and `debug_data.non_opaque_primitives` (0x47e4ec); kb.json maps
 * this address to rasterizer_sprites.obj.  The field names prove what the
 * three pointers ARE, not what this function is called, so the name stays raw.
 *
 * Each pointer is loaded twice in the reference -- once for the null test
 * (0017e04d / 0017e076 / 0017e09f) and again as the debug_free argument
 * (0017e0c8 / 0017e0dd / 0017e0f3) -- so they are re-read rather than held in
 * locals.  The function is frameless (entry is MOV AL,[0x47e4d8]; no PUSH EBP)
 * and no locals are introduced.
 *
 * The flag is a byte (MOV AL / MOV byte ptr [0x47e4d8],0x0), not a dword.
 * ADD ESP,0x24 at 0017e109 is the coalesced cleanup for the three 3-dword
 * debug_free calls; the assert paths emit no cleanup because system_exit is
 * noreturn.  The ARG_COUNT hazard on the last debug_free is that coalescing.
 *
 * Explicit unknown: the layout of the `debug_data` record these three fields
 * belong to, and whether the neighbouring dwords zeroed by FUN_0017e010
 * (0x47e4e0 / 0x47e4e8 / 0x47e4f0) are counters paired with them. */
void FUN_0017e040(void)
{
  if (*(char *)0x47e4d8 != 0) {
    if (*(void **)0x47e4dc == 0) {
      display_assert("debug_data.opaque_triangles",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x89,
                     1);
      system_exit(-1);
    }
    if (*(void **)0x47e4e4 == 0) {
      display_assert("debug_data.opaque_lines",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x8a,
                     1);
      system_exit(-1);
    }
    if (*(void **)0x47e4ec == 0) {
      display_assert("debug_data.non_opaque_primitives",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x8b,
                     1);
      system_exit(-1);
    }
    debug_free(*(void **)0x47e4dc,
               "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x8d);
    debug_free(*(void **)0x47e4e4,
               "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x8e);
    debug_free(*(void **)0x47e4ec,
               "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x8f);
    *(char *)0x47e4d8 = 0;
  }
}

/* Record comparator (0x17e130), cdecl with two record pointers on the stack
 * ([EBP+8] -> EDX, [EBP+0xc] -> ESI) and the ordering key returned in EAX.
 * The plain RET (no immediate) and the two stack loads prove cdecl/2 args.
 *
 * Three record fields are touched; none has a proven meaning:
 *   +0x30  signed 16-bit (MOVSX word at 0017e177 / 0017e184)
 *   +0x34  float        (FLD/FCOMP at 0017e14c / 0017e15e)
 *   +0x38  byte flag    (MOV AL/BL + TEST at 0017e136 / 0017e144)
 *
 * When neither flag byte is set the result is the ordinary three-way float
 * comparison on +0x34.  The reference does it with two independent
 * FLD/FCOMP/FNSTSW pairs:
 *   TEST AH,0x41 masks C3|C0, so it is zero only for a > b -> ECX = 1.
 *   TEST AH,0x05 masks C2|C0; JP is taken for gt (0 bits), eq (0 bits) and
 *   unordered (2 bits), so only a < b falls through to MOV EAX,-1.
 * Both fields are therefore re-read per comparison rather than held in float
 * locals -- a float temporary would also let clang keep the value in 80-bit
 * under -mno-sse where MSVC narrows on assignment.
 *
 * When either flag byte is set the float key is ignored and the result is
 * built from the signed 16-bit field instead: negated for the first record,
 * added for the second.  The two entries into that block (JNZ 0017e177 skips
 * the shared TEST AL,AL, JNZ 0017e173 lands on it) are the shape of a single
 * `||` guard whose body re-tests each flag.
 *
 * Explicit unknown: the record type, and what the +0x38 flag distinguishes --
 * only that it switches the sort key from the +0x34 float to the +0x30 short.
 * The PUSH EBX / MOV BL / TEST BL / POP EBX at 0017e143..0017e149 is MSVC
 * spending a callee-saved register on one byte test; it is not reproducible
 * from C. */
int FUN_0017e130(void *record_a, void *record_b)
{
  int result;

  result = 0;
  if (*(char *)((char *)record_a + 0x38) != 0 ||
      *(char *)((char *)record_b + 0x38) != 0) {
    if (*(char *)((char *)record_a + 0x38) != 0) {
      result = -(int)*(short *)((char *)record_a + 0x30);
    }
    if (*(char *)((char *)record_b + 0x38) != 0) {
      result += (int)*(short *)((char *)record_b + 0x30);
    }
    return result;
  }
  if (*(float *)((char *)record_a + 0x34) >
      *(float *)((char *)record_b + 0x34)) {
    result = 1;
  }
  if (*(float *)((char *)record_a + 0x34) <
      *(float *)((char *)record_b + 0x34)) {
    return -1;
  }
  return result;
}

/* 0x17e190: flush the accumulated debug geometry -- sort the non-opaque
 * primitive list, then draw the opaque triangles, the opaque lines and finally
 * every non-opaque primitive, one dynamic vertex buffer at a time.
 *
 * Globals, all spelled by raw address exactly as the rest of this TU does (the
 * assert texts name them as `debug_data.*` fields, but the code only ever
 * touches the separate addresses, so no struct is synthesised):
 *   0x47e4d8  char  "debug buffers valid" flag set by FUN_0017df80
 *   0x47e4dc  void* opaque_triangles buffer      0x47e4e0 int its count
 *   0x47e4e4  void* opaque_lines buffer          0x47e4e8 int its count
 *   0x47e4ec  void* non_opaque_primitives buffer 0x47e4f0 int its count
 *   0x47e4f4  int   shared total primitive count
 *   0x3256dd  char  the same rendering-enabled byte FUN_0015a560 tests
 *   0x325652  word  render-phase marker: 0xd for the duration, 0 on the way
 *                   out (MOV word ptr at 0x17e308 and 0x17e596).  The three
 *                   top guards exit via 0x17e5a0, which does NOT store 0, so
 *                   only the paths that reached the draw code clear it.
 *
 * Record stride is 0x3c (the qsort element size at 0x17e2f5 and the ADD
 * EDI,0x3c in every loop).  +0x30 is the int16 vertex count of the record:
 * MOVSX at 0x17e366/0x17e380/0x17e446/0x17e460/0x17e4f4/0x17e51b, and the
 * vertices themselves are 0x10 bytes each (SHL by 4 for the memcpy size and
 * for the destination stride).  The reference re-reads +0x30 twice per
 * iteration -- once for the memcpy size, once for the running vertex index --
 * and re-reads the buffer base global inside the loop body (0x17e360,
 * 0x17e440, 0x17e4ef) rather than hoisting it, so both are spelled inline.
 *
 * Call-site audit note: the "cleanup vs decl" reports here are MSVC coalescing
 * one ADD ESP over several calls (ADD ESP,0x14 at 0x17e2e2 for the assert
 * group, ADD ESP,0x1c at 0x17e3b5/0x17e495 and ADD ESP,0x28 at 0x17e54a for
 * the unlock/draw group) -- the same artifact class already documented for
 * FUN_0017e040, not an argument-count bug.
 *
 * FUN_0015a560 unknown: the reference pushes TWO dwords at each of the three
 * call sites -- (1,0) at 0x17e39e, (1,0x10) at 0x17e47e, (0,0) at 0x17e530 --
 * but the lifted callee (rasterizer_xbox_decals.c) reads only [ESP+4], and its
 * kb decl is one `char` parameter.  The second dword is left uncalled here; its
 * meaning is unknown and the callee never reads it, so behaviour is unaffected.
 *
 * The 4th argument of the last draw is zero-extended in the reference
 * (XOR EAX,EAX / MOV AX,word ptr at 0x17e539) unlike the MOVSX reads
 * elsewhere, so that one read is spelled unsigned. */
void FUN_0017e190(void)
{
  char ok;
  int handle;
  int primitive_count;
  int remaining;
  int record_offset;
  int vertex_index;
  int index;
  char *record;
  void *vertices;

  ok = 1;
  if (*(char *)0x47e4d8 == 0) {
    return;
  }
  primitive_count = *(int *)0x47e4f4;
  if (primitive_count <= 0) {
    return;
  }
  if (*(char *)0x3256dd == 0) {
    return;
  }
  assert_halt_msg_at("debug_data.opaque_triangles",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x140,
                     *(void **)0x47e4dc != (void *)0);
  assert_halt_msg_at("debug_data.opaque_lines",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x141,
                     *(void **)0x47e4e4 != (void *)0);
  assert_halt_msg_at("debug_data.non_opaque_primitives",
                     "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x142,
                     *(void **)0x47e4ec != (void *)0);
  assert_halt_msg_at(
    "debug_data.opaque_triangle_count <=RASTERIZER_MAXIMUM_DEBUG_PRIMITIVES",
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x143,
    *(int *)0x47e4e0 <= 0x2000);
  assert_halt_msg_at(
    "debug_data.opaque_line_count <=RASTERIZER_MAXIMUM_DEBUG_PRIMITIVES",
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x144,
    *(int *)0x47e4e8 <= 0x2000);
  assert_halt_msg_at(
    "debug_data.non_opaque_primitive_count<=RASTERIZER_MAXIMUM_DEBUG_"
    "PRIMITIVES",
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x145,
    *(int *)0x47e4f0 <= 0x2000);
  assert_halt_msg_at(
    "debug_data.primitive_count <=RASTERIZER_MAXIMUM_DEBUG_PRIMITIVES",
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0x146,
    *(int *)0x47e4f4 <= 0x2000);

  qsort(*(void **)0x47e4ec, (size_t) * (int *)0x47e4f0, 0x3c,
        (qsort_compar_proc)FUN_0017e130);
  *(uint16_t *)0x325652 = 0xd;

  if (*(int *)0x47e4e0 > 0) {
    handle = rasterizer_widget_set_zbuffer_enable(9, *(int *)0x47e4e0 * 3);
    if (handle == -1) {
      ok = 0;
      goto finished;
    }
    vertices = (void *)rasterizer_widget_draw_sprite3d(handle);
    if (vertices != (void *)0) {
      vertex_index = 0;
      primitive_count = *(int *)0x47e4e0;
      if (primitive_count > 0) {
        record_offset = 0;
        remaining = primitive_count;
        do {
          record = (char *)*(void **)0x47e4dc + record_offset;
          csmemcpy((char *)vertices + vertex_index * 0x10, record,
                   (size_t)((int)*(short *)(record + 0x30) << 4));
          vertex_index += *(short *)(record + 0x30);
          record_offset += 0x3c;
          remaining--;
        } while (remaining != 0);
      }
      rasterizer_widget_end(handle);
      FUN_0015a560(1);
      rasterizer_draw_dynamic_vertices(0, primitive_count, handle, 3);
      FUN_0015a290();
    } else {
      error(2, "### ERROR failed to lock dynamic vertex buffers for debug "
               "primitives");
      ok = 0;
    }
    FUN_0017c9f0(handle);
    if (ok == 0) {
      goto finished;
    }
  }

  if (*(int *)0x47e4e8 > 0) {
    handle = rasterizer_widget_set_zbuffer_enable(9, *(int *)0x47e4e8 * 2);
    if (handle == -1) {
      ok = 0;
      goto finished;
    }
    vertices = (void *)rasterizer_widget_draw_sprite3d(handle);
    if (vertices != (void *)0) {
      vertex_index = 0;
      primitive_count = *(int *)0x47e4e8;
      if (primitive_count > 0) {
        record_offset = 0;
        remaining = primitive_count;
        do {
          record = (char *)*(void **)0x47e4e4 + record_offset;
          csmemcpy((char *)vertices + vertex_index * 0x10, record,
                   (size_t)((int)*(short *)(record + 0x30) << 4));
          vertex_index += *(short *)(record + 0x30);
          record_offset += 0x3c;
          remaining--;
        } while (remaining != 0);
      }
      rasterizer_widget_end(handle);
      FUN_0015a560(1);
      rasterizer_draw_dynamic_vertices(0, primitive_count, handle, 2);
      FUN_0015a290();
      FUN_0017c9f0(handle);
    } else {
      error(2, "### ERROR failed to lock dynamic vertex buffers for debug "
               "primitives");
      ok = 0;
      FUN_0017c9f0(handle);
    }
  }

finished:
  index = 0;
  if (ok != 0) {
    record_offset = 0;
    do {
      if (index >= *(int *)0x47e4f0) {
        break;
      }
      record = (char *)*(void **)0x47e4ec + record_offset;
      handle =
        rasterizer_widget_set_zbuffer_enable(9, (int)*(short *)(record + 0x30));
      if (handle == -1) {
        ok = 0;
      } else {
        vertices = (void *)rasterizer_widget_draw_sprite3d(handle);
        if (vertices != (void *)0) {
          csmemcpy(vertices, record,
                   (size_t)((int)*(short *)(record + 0x30) << 4));
          rasterizer_widget_end(handle);
          FUN_0015a560(0);
          rasterizer_draw_dynamic_vertices(0, 1, handle,
                                           *(unsigned short *)(record + 0x30));
          FUN_0015a290();
          FUN_0017c9f0(handle);
        } else {
          error(2, "### ERROR failed to lock dynamic vertex buffers for debug "
                   "primitives");
          ok = 0;
          FUN_0017c9f0(handle);
        }
      }
      index++;
      record_offset += 0x3c;
    } while (ok != 0);
  }
  *(uint16_t *)0x325652 = 0;
}

/* 0x17e5b0: append one debug line to the debug-geometry record list.
 *
 * The four parameters are string-proven by the assert at line 0xab, whose
 * .rdata text is "p0 && p1 && color0 && color1" and whose four TEST/JZ pairs
 * (0017e5d9 EDI=[EBP+8], 0017e5dd EBX=[EBP+0xc], 0017e5e4 ESI=[EBP+0x10],
 * 0017e5eb EAX=[EBP+0x14]) test the incoming slots in that order.  kb.json
 * previously declared the trailing pair `int transform_a, int transform_b`;
 * they are colour pointers, and the decl is corrected here.  The 0x17eb10
 * wrapper below passes its single third dword to both of them, i.e. a
 * single-colour line, matching the "colour pushed twice" shape of the other
 * debug-line helpers in this TU.
 *
 * The __FILE__ at 0x2af4b8 is "c:\halo\SOURCE\rasterizer\rasterizer_debug.c"
 * (lines 0xab..0xae) -- Bungie's TU was rasterizer_debug.c, but kb.json maps
 * this address into rasterizer_sprites.obj, so the string is copied verbatim
 * as binary data and the object mapping is left alone, exactly as for
 * FUN_0017df80 / FUN_0017e040 / FUN_0017e190 above.
 *
 * Globals are the same debug_data set FUN_0017e190 documents: 0x47e4d8 the
 * byte "buffers valid" flag, 0x3256dd the byte rendering-enabled flag,
 * 0x47e4e4/0x47e4e8 the opaque_lines buffer and its count, 0x47e4ec/0x47e4f0
 * the non_opaque_primitives buffer and its count.  0x5a5bc8/cc/d0 is the
 * camera position and 0x5a5bd4/d8/dc the camera forward vector (the same two
 * triples FUN_0017d1a0 uses).
 *
 * Two guards decide the destination list:
 *   - FLD [colour] / FCOMP [0x2533c0] twice, where 0x2533c0 is 0x00000000 =
 *     0.0f.  TEST AH,0x41 masks C3|C0, so the first JZ is taken for
 *     color0[0] > 0 and the second JNZ exits for color1[0] <= 0: the body runs
 *     when either leading component (the alpha) is positive.
 *   - CMP dword ptr [ESI],0x3f800000 and the same on color1.  This is an
 *     INTEGER compare of the float bit pattern for 1.0f, not an FPU compare,
 *     so it is spelled as a dword compare here; writing `color0[0] == 1.0f`
 *     emits FLD/FCOMP instead.  Both alphas at 1.0 select the opaque_lines
 *     list, otherwise the non_opaque_primitives list.
 *
 * The record is 0x3c bytes (IMUL EAX,EAX,0x3c), matching the qsort element
 * size and the ADD EDI,0x3c stride in FUN_0017e190.  Store offsets taken from
 * the raw MOV/FSTP destinations:
 *   +0x00/+0x04/+0x08  p0 xyz, copied as three dword MOVs (MSVC copies the
 *                      12-byte point as integers, not FLD/FSTP)
 *   +0x0c              FUN_000d1c90(color0) -- the packed colour returned in
 *                      EAX and stored as a dword (MOV [ESI+0xc],EAX), so it
 *                      is a uint32_t, not a float
 *   +0x10/+0x14/+0x18  p1 xyz (written through LEA ECX,[ESI+0x10])
 *   +0x1c              FUN_000d1c90(color1)
 *   +0x20..+0x2f       never written here -- the third vertex slot, used only
 *                      by the triangle entry point
 *   +0x30              word 2, the int16 vertex count FUN_0017e130 sorts on
 *                      and FUN_0017e190 MOVSXs (2 vertices = a line)
 *   +0x34              float sort key
 *   +0x38              byte flag, the opaque selector, the same byte
 *                      FUN_0017e130 tests
 *
 * The sort key is the dot product of the camera forward vector with
 * (camera_position - point) for each endpoint, the smaller of the two.  The
 * reference keeps the first dot product in ST0 across the FCOM and only spills
 * the second (FSTP [EBP-0x8]), so only one distance has a stack slot; the
 * min is FCOM [EBP-0x8] / TEST AH,0x41 / FSTP ST0 / FLD [EBP-0x8], i.e.
 * replace with the second only when the second is strictly smaller.
 *
 * Frame: SUB ESP,0x20 with delta1 at EBP-0x20..-0x18, delta0 at
 * EBP-0x14..-0xc, the spilled second distance at EBP-0x8 and the opaque byte
 * at EBP-0x1 (EBP-0x4..-0x2 unused).
 *
 * Call-site audit note: ADD ESP,0x8 at 0017e799 is the coalesced cleanup for
 * the two 1-argument FUN_000d1c90 calls, the same artifact class already
 * documented for FUN_0017e040. */
void FUN_0017e5b0(float *p0, float *p1, float *color0, float *color1)
{
  float delta1[3];
  float delta0[3];
  float distance1;
  char opaque;
  char *buffer;
  char *record;
  int *count;
  int index;
  float distance0;

  if (*(char *)0x47e4d8 != 0 && *(char *)0x3256dd != 0) {
    assert_halt_msg_at("p0 && p1 && color0 && color1",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xab,
                       p0 != NULL && p1 != NULL && color0 != NULL &&
                         color1 != NULL);
    assert_halt_msg_at("debug_data.opaque_triangles",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xac,
                       *(void **)0x47e4dc != NULL);
    assert_halt_msg_at("debug_data.opaque_lines",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xad,
                       *(void **)0x47e4e4 != NULL);
    assert_halt_msg_at("debug_data.non_opaque_primitives",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xae,
                       *(void **)0x47e4ec != NULL);
    if (color0[0] > 0.0f || color1[0] > 0.0f) {
      if (*(uint32_t *)color0 == 0x3f800000 &&
          *(uint32_t *)color1 == 0x3f800000) {
        opaque = 1;
        count = (int *)0x47e4e8;
      } else {
        opaque = 0;
        count = (int *)0x47e4f0;
      }
      index = FUN_0017df10(count);
      if (index != -1) {
        buffer = *(char **)0x47e4e4;
        if (opaque == 0) {
          buffer = *(char **)0x47e4ec;
        }
        record = buffer + index * 0x3c;
        delta0[0] = *(float *)0x5a5bc8 - p0[0];
        delta0[1] = *(float *)0x5a5bcc - p0[1];
        delta0[2] = *(float *)0x5a5bd0 - p0[2];
        delta1[0] = *(float *)0x5a5bc8 - p1[0];
        delta1[1] = *(float *)0x5a5bcc - p1[1];
        delta1[2] = *(float *)0x5a5bd0 - p1[2];
        *(uint16_t *)(record + 0x30) = 2;
        *(uint32_t *)(record + 0x00) = ((uint32_t *)p0)[0];
        *(uint32_t *)(record + 0x04) = ((uint32_t *)p0)[1];
        *(uint32_t *)(record + 0x08) = ((uint32_t *)p0)[2];
        *(uint32_t *)(record + 0x10) = ((uint32_t *)p1)[0];
        *(uint32_t *)(record + 0x14) = ((uint32_t *)p1)[1];
        *(uint32_t *)(record + 0x18) = ((uint32_t *)p1)[2];
        *(uint32_t *)(record + 0x0c) = FUN_000d1c90(color0);
        *(uint32_t *)(record + 0x1c) = FUN_000d1c90(color1);
        distance0 = *(float *)0x5a5bdc * delta0[2] +
                    *(float *)0x5a5bd8 * delta0[1] +
                    *(float *)0x5a5bd4 * delta0[0];
        distance1 = *(float *)0x5a5bdc * delta1[2] +
                    *(float *)0x5a5bd8 * delta1[1] +
                    *(float *)0x5a5bd4 * delta1[0];
        if (distance1 < distance0) {
          distance0 = distance1;
        }
        *(float *)(record + 0x34) = distance0;
        *(char *)(record + 0x38) = opaque;
      }
    }
  }
}

/* 0x17e800: append one debug triangle to the debug-geometry record list.  The
 * triangle sibling of FUN_0017e5b0 above -- same globals, same 0x3c record,
 * same guards, one more vertex.
 *
 * The six parameters are string-proven by the assert at rasterizer_debug.c
 * line 0xe5, whose .rdata text is "p0 && p1 && p2 && color0 && color1 &&
 * color2" and whose six TEST/JZ pairs (0017e829 EDI=[EBP+8], 0017e830
 * [EBP+0xc], 0017e834 EBX=[EBP+0x10], 0017e83b ESI=[EBP+0x14], 0017e842
 * [EBP+0x18], 0017e849 [EBP+0x1c]) test the incoming slots in that order.
 * kb.json declared the trailing three as `void *param_4/5/6`; they are colour
 * pointers, and the decl is corrected here.  The 0x17eb30 wrapper below passes
 * one colour into all three, i.e. a flat-shaded triangle.
 *
 * Differences from the line variant, all binary-derived:
 *   - the guard is a three-way `alpha > 0` disjunction (three FLD/FCOMP
 *     against 0x2533c0 = 0.0f, the first two JZ into the body, the third JNZ
 *     to the exit);
 *   - the opaque test is three dword compares against 0x3f800000, again an
 *     INTEGER compare of the 1.0f bit pattern, not an FPU compare;
 *   - the opaque list is debug_data.opaque_triangles (buffer 0x47e4dc, count
 *     0x47e4e0) instead of opaque_lines;
 *   - the +0x30 vertex count is 3, and the third vertex occupies the
 *     +0x20..+0x2f slot the line variant leaves untouched.
 *
 * Store offsets, from the raw MOV/FSTP destinations:
 *   +0x00/+0x04/+0x08  p0 xyz (dword MOVs through ECX = ESI)
 *   +0x0c              FUN_000d1c90(color0), a uint32_t packed colour in EAX
 *   +0x10/+0x14/+0x18  p1 xyz (through LEA ECX,[ESI+0x10])
 *   +0x1c              FUN_000d1c90(color1)
 *   +0x20/+0x24/+0x28  p2 xyz (through LEA ECX,[ESI+0x20])
 *   +0x2c              FUN_000d1c90(color2)
 *   +0x30              word 3
 *   +0x34              float sort key
 *   +0x38              byte opaque flag
 *
 * The sort key is the smallest of the three per-vertex camera depths, and the
 * reference proves it is written as a nested two-argument min MACRO, not a
 * running minimum: the inner min(distance1, distance2) is evaluated TWICE.
 * The first evaluation is FCOM [EBP-0x8] / JNZ / FLD ST0 vs FLD [EBP-0x8] at
 * 0017ea94..0017eaa3, which leaves the selected value in ST0 while KEEPING
 * distance1 in ST1; the second is the FCOM [EBP-0x8] at 0017ead2 that re-picks
 * between that preserved distance1 and distance2 when distance0 lost the outer
 * comparison.  A running `if (x < best) best = x;` chain would evaluate it
 * once and would not need the ST1 copy, so the double-evaluated ternary below
 * is the faithful spelling.
 *
 * Frame: SUB ESP,0x30 with delta0 at EBP-0x30..-0x28, delta2 at
 * EBP-0x24..-0x1c, delta1 at EBP-0x18..-0x10, distance0 spilled with FST (not
 * FSTP) at EBP-0xc, distance2 at EBP-0x8 and the opaque byte at EBP-0x1;
 * distance1 never leaves the x87 stack.
 *
 * Call-site audit note: ADD ESP,0xc at 0017ea59 is the coalesced cleanup for
 * the three 1-argument FUN_000d1c90 calls, the same artifact class documented
 * for FUN_0017e040. */
void rasterizer_debug_triangle_shaded(float *p0, float *p1, float *p2,
                                      float *color0, float *color1,
                                      float *color2)
{
  float delta0[3];
  float delta2[3];
  float delta1[3];
  float distance0;
  float distance1;
  float distance2;
  char opaque;
  char *buffer;
  char *record;
  int *count;
  int index;

  if (*(char *)0x47e4d8 != 0 && *(char *)0x3256dd != 0) {
    assert_halt_msg_at("p0 && p1 && p2 && color0 && color1 && color2",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xe5,
                       p0 != NULL && p1 != NULL && p2 != NULL &&
                         color0 != NULL && color1 != NULL && color2 != NULL);
    assert_halt_msg_at("debug_data.opaque_triangles",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xe6,
                       *(void **)0x47e4dc != NULL);
    assert_halt_msg_at("debug_data.opaque_lines",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xe7,
                       *(void **)0x47e4e4 != NULL);
    assert_halt_msg_at("debug_data.non_opaque_primitives",
                       "c:\\halo\\SOURCE\\rasterizer\\rasterizer_debug.c", 0xe8,
                       *(void **)0x47e4ec != NULL);
    if (color0[0] > 0.0f || color1[0] > 0.0f || color2[0] > 0.0f) {
      if (*(uint32_t *)color0 == 0x3f800000 &&
          *(uint32_t *)color1 == 0x3f800000 &&
          *(uint32_t *)color2 == 0x3f800000) {
        opaque = 1;
        count = (int *)0x47e4e0;
      } else {
        opaque = 0;
        count = (int *)0x47e4f0;
      }
      index = FUN_0017df10(count);
      if (index != -1) {
        buffer = *(char **)0x47e4dc;
        if (opaque == 0) {
          buffer = *(char **)0x47e4ec;
        }
        record = buffer + index * 0x3c;
        delta0[0] = *(float *)0x5a5bc8 - p0[0];
        delta0[1] = *(float *)0x5a5bcc - p0[1];
        delta0[2] = *(float *)0x5a5bd0 - p0[2];
        delta1[0] = *(float *)0x5a5bc8 - p1[0];
        delta1[1] = *(float *)0x5a5bcc - p1[1];
        delta1[2] = *(float *)0x5a5bd0 - p1[2];
        delta2[0] = *(float *)0x5a5bc8 - p2[0];
        delta2[1] = *(float *)0x5a5bcc - p2[1];
        delta2[2] = *(float *)0x5a5bd0 - p2[2];
        *(uint16_t *)(record + 0x30) = 3;
        *(uint32_t *)(record + 0x00) = ((uint32_t *)p0)[0];
        *(uint32_t *)(record + 0x04) = ((uint32_t *)p0)[1];
        *(uint32_t *)(record + 0x08) = ((uint32_t *)p0)[2];
        *(uint32_t *)(record + 0x10) = ((uint32_t *)p1)[0];
        *(uint32_t *)(record + 0x14) = ((uint32_t *)p1)[1];
        *(uint32_t *)(record + 0x18) = ((uint32_t *)p1)[2];
        *(uint32_t *)(record + 0x20) = ((uint32_t *)p2)[0];
        *(uint32_t *)(record + 0x24) = ((uint32_t *)p2)[1];
        *(uint32_t *)(record + 0x28) = ((uint32_t *)p2)[2];
        *(uint32_t *)(record + 0x0c) = FUN_000d1c90(color0);
        *(uint32_t *)(record + 0x1c) = FUN_000d1c90(color1);
        *(uint32_t *)(record + 0x2c) = FUN_000d1c90(color2);
        distance1 = *(float *)0x5a5bdc * delta1[2] +
                    *(float *)0x5a5bd8 * delta1[1] +
                    *(float *)0x5a5bd4 * delta1[0];
        distance2 = *(float *)0x5a5bdc * delta2[2] +
                    *(float *)0x5a5bd8 * delta2[1] +
                    *(float *)0x5a5bd4 * delta2[0];
        distance0 = *(float *)0x5a5bdc * delta0[2] +
                    *(float *)0x5a5bd8 * delta0[1] +
                    *(float *)0x5a5bd4 * delta0[0];
        *(float *)(record + 0x34) =
          distance0 < (distance1 < distance2 ? distance1 : distance2) ?
            distance0 :
            (distance1 < distance2 ? distance1 : distance2);
        *(char *)(record + 0x38) = opaque;
      }
    }
  }
}
/* Forwarding wrapper (0x17eb10).  A real frame -- PUSH EBP / MOV EBP,ESP ...
 * CALL 0x17e5b0 / ADD ESP,0x10 / POP EBP / RET -- not a tail call.  The third
 * incoming dword is loaded once (MOV EAX,[EBP+0x10] at 0017eb13) and pushed
 * TWICE (PUSH EAX / PUSH EAX at 0017eb19..0017eb1a) before [EBP+0xc] and
 * [EBP+0x8], so 0x17e5b0 receives it as both of its trailing arguments.  The
 * duplicated argument is binary-proven, not a decompiler artifact.
 *
 * The duplicated dword is a colour pointer: the callee's assert at
 * rasterizer_debug.c line 0xab spells its four parameters
 * "p0 && p1 && color0 && color1", and it dereferences both trailing slots as
 * float pointers.  This wrapper's own slot stays `int` because its ten call
 * sites in render_debug.c / hud.c already pass `(int)color`; the cast is done
 * here rather than churning those TUs. */
void FUN_0017eb10(float *vert_a, float *vert_b, int param_3)
{
  FUN_0017e5b0(vert_a, vert_b, (float *)param_3, (float *)param_3);
}

/* Forwarding wrapper (0x17eb30).  Full EBP frame, cdecl call, no tail call:
 * the original pushes six dwords and cleans with ADD ESP,0x18 before
 * POP EBP/RET.
 *
 * Argument decode (first PUSH is the last argument):
 *   0x17eb33 MOV EAX,[EBP+0x14]  -- color
 *   0x17eb36 MOV ECX,[EBP+0xc]   -- point1
 *   0x17eb39 MOV EDX,[EBP+0x8]   -- point0
 *   0x17eb3c PUSH EAX            -- arg 6 = color
 *   0x17eb3d PUSH EAX            -- arg 5 = color
 *   0x17eb3e PUSH EAX            -- arg 4 = color
 *   0x17eb3f MOV EAX,[EBP+0x10] / PUSH EAX -- arg 3 = point2
 *   0x17eb43 PUSH ECX            -- arg 2 = point1
 *   0x17eb44 PUSH EDX            -- arg 1 = point0
 * The single `color` argument therefore fills all three of the callee's
 * trailing slots.  This is not a duplicate-argument hazard, it is what the
 * binary does.  A plausible reading is that 0x17e800 takes one color per
 * vertex and this entry point draws a flat-shaded triangle, but nothing in
 * the binary proves that, so the callee's trailing slots stay param_4/5/6.
 *
 * The kb decl for 0x17e800 was `void rasterizer_debug_triangle_shaded(void)`;
 * the ADD ESP,0x18 here proves six cdecl dword arguments and the decl is
 * widened accordingly.  The return stays void: EAX is not consumed after the
 * CALL at 0x17eb45. */
void FUN_0017eb30(float *point0, float *point1, float *point2, void *color)
{
  rasterizer_debug_triangle_shaded(point0, point1, point2, color, color, color);
}

/* Allocator/initializer (0x17eb50).  Returns bool in AL: the reference sets
 * MOV BL,0x1 before the call and MOV AL,BL on the success path, XOR AL,AL on
 * the failure path.  Ghidra's decompile shows `void` and drops the return
 * value; the disassembly and the kb.json decl both say bool, so the decompile
 * is wrong here.  The PUSH EBX / MOV BL / MOV AL,BL / POP EBX shape is MSVC
 * spending a callee-saved register to materialize the constant 1 and is not
 * reproducible from C.
 *
 * The store to the global at 0x47ec40 is unconditional (MOV [0x47ec40],EAX
 * follows TEST EAX,EAX at 0017eb69..0017eb6b).
 *
 * The __FILE__ / __LINE__ pair passed to debug_malloc is binary-proven:
 * PUSH 0x29 / PUSH 0x2af728 names rasterizer_frame_statistics.c line 0x29,
 * even though kb.json maps this address to rasterizer_sprites.obj.
 *
 * The error() call pushes exactly two arguments (PUSH 0x2af710 / PUSH 0x2,
 * ADD ESP,0x8) -- no varargs are supplied despite the variadic decl.
 *
 * Explicit unknown: what the 0x24000-byte block at 0x47ec40 holds.  It is read
 * elsewhere as an opaque pointer (rasterizer_text.c). */
bool FUN_0017eb50(void)
{
  *(void **)0x47ec40 = debug_malloc(
    0x24000, false,
    "c:\\halo\\SOURCE\\rasterizer\\rasterizer_frame_statistics.c", 0x29);
  if (*(void **)0x47ec40 == NULL) {
    error(2, "### ERROR out of memory");
    return false;
  }
  return true;
}

/* Zero-fills a 0x170-byte global block at 0x5a5400 (0x17eb90).  The whole
 * body is six instructions -- PUSH 0x170 / PUSH 0x0 / PUSH 0x5a5400 /
 * CALL csmemset / ADD ESP,0xc / RET -- with no frame, no arguments and no
 * return value; the csmemset result in EAX is discarded.
 *
 * The debug counter at 0x5a5540 documented on FUN_0017df10 above lies inside
 * this range (0x5a5400 + 0x170 == 0x5a5570), so this function resets it.  That
 * is containment only: it does not prove the 0x170 bytes form one record.
 *
 * Explicit unknown: the layout and meaning of the block.  Only its base, its
 * size, and its reset-to-zero role are proven here.  csmemset (0x8db80) is
 * called by name rather than memset so the call is not lowered inline. */
void FUN_0017eb90(void)
{
  csmemset((void *)0x5a5400, 0, 0x170);
}

/* Frame-rate statistics update (0x17ebb0).  Takes one stack pointer argument
 * (EBX = [EBP+8]) and fills a small float record from a ring of frame
 * timestamps.  Layout proven by the stores at 0x17ec6d..0x17ecee:
 *
 *   +0x00  float   1000 / (now - last)            instantaneous rate
 *   +0x04  int16   sample count  (word store)
 *   +0x08  float   (count * 1000) / (now - arr[count-1])   windowed rate
 *   +0x0c  float   1000 / max inter-sample delta  (slowest frame)
 *   +0x10  float   1000 / min inter-sample delta  (fastest frame)
 *
 * The +0x0c / +0x10 pairing is easy to invert: the FSTP at 0x17ecce consumes
 * the EDI (running MINIMUM delta) chain and targets +0x10; the FSTP at
 * 0x17ecee consumes the [EBP-4] (running MAXIMUM delta) chain and targets
 * +0x0c.  Verified against the disassembly, not the decompiler.
 *
 * Globals:
 *   0x3256ba  int16   gate; only "!= 0" is proven HERE (elsewhere in this tree
 *                     the same word is tested == 2 and == 3, so no narrower
 *                     meaning is claimed).
 *   0x47ec60  int32[] ring of timestamps, newest first; arr[0] is the previous
 *                     frame's timestamp.  0x47ec60 + 0x3c*4 == 0x47ed50, so
 *                     the ring holds at most 60 entries -- matching the 0x3c
 *                     clamp at 0x17ecfa.
 *   0x47ed50  int16   sample count.  Every access in this function is
 *                     word-sized (0x17ebd6, 0x17ed00, 0x17ed0e, 0x17ed19);
 *                     the decompiler's CONCAT22/`& 0xffff0000` dword form is
 *                     an artifact and 0x47ed52 must never be written.  The one
 *                     dword READ (`*(volatile int *)0x47ed50`, seeding idx) is
 *                     deliberate: the original emits `MOV EDX,[0x47ed50]; DEC
 *                     EDX; TEST DX,DX` there, and `volatile` is what keeps VC71
 *                     from folding it into the word load above.
 *   0x254cb8  float   1000.0f (documented at rasterizer.c:960 and elsewhere).
 *   0x25fb8c  float   the unsigned-to-float fixup addend.
 *
 * The `unsigned int -> float` casts are left to the compiler: MSVC's own
 * lowering IS the reference's MOV/FILD/TEST/JGE/FADD sequence, with 0x25fb8c
 * as its fixup addend, so no hand-spelled conditional add is needed.
 *
 * The loop shifts the ring down by one (arr[i] = arr[i-1]) while accumulating
 * the min and max adjacent delta.  It never writes arr[0], so the re-read of
 * 0x47ec60 at 0x17ec3e yields the same value the delta was seeded from; it is
 * kept because the original re-reads it.
 *
 * Explicit unknowns: the meaning of the 0x3256ba gate, and why the delta
 * clamp floor is 1 rather than 0 (only "values below 2 become 1" is proven). */
void FUN_0017ebb0(float *stats_out)
{
  unsigned int now;
  unsigned int min_delta;
  unsigned int max_delta;
  unsigned int delta;
  short idx;
  short count;
  int *p;
  int next;
  float fnum;

  if (*(short *)0x3256ba == 0 || stats_out == 0) {
    *(short *)0x47ed50 = 0;
    return;
  }

  now = system_milliseconds();
  count = *(short *)0x47ed50;
  if (count != 0) {
    min_delta = now - *(unsigned int *)0x47ec60;
    idx = (short)(*(volatile int *)0x47ed50 - 1);
    max_delta = min_delta;
    if (idx > 0) {
      p = (int *)0x47ec60 + idx;
      do {
        if (idx > 1) {
          delta = (unsigned int)p[-1] - (unsigned int)p[0];
          if (delta <= min_delta)
            min_delta = delta;
          if (delta > max_delta)
            max_delta = delta;
        }
        p[0] = p[-1];
        idx--;
        p--;
      } while (idx > 0);
      count = *(short *)0x47ed50;
    }

    delta = now - *(volatile unsigned int *)0x47ec60;
    if (delta <= 1)
      delta = 1;
    fnum = *(const float *)0x254cb8 / (float)delta;
    *(short *)((char *)stats_out + 4) = count;
    stats_out[0] = fnum;

    /* 0x47ec5c + count*4 == &arr[count - 1], the oldest retained sample. */
    delta = now - *(unsigned int *)(0x47ec5c + (int)count * 4);
    if (delta <= 1)
      delta = 1;
    fnum = (float)(int)count * *(const float *)0x254cb8;
    stats_out[2] = fnum / (float)delta;

    if (min_delta <= 1)
      min_delta = 1;
    stats_out[4] = *(const float *)0x254cb8 / (float)min_delta;

    if (max_delta <= 1)
      max_delta = 1;
    stats_out[3] = *(const float *)0x254cb8 / (float)max_delta;
  }

  *(unsigned int *)0x47ec60 = now;
  next = (int)count + 1;
  if (next > 0x3c) {
    *(short *)0x47ed50 = 0x3c;
    return;
  }
  *(short *)0x47ed50 = (short)next;
}

/* Frame-statistics recording start (0x17ed30).  Nine instructions, no frame,
 * no arguments, no return value:
 *
 *   MOV byte ptr [0x3256b8],0x1     -- byte-sized store of 1 (a flag)
 *   CALL 0x8e370                    -- system_milliseconds(), result in EAX
 *   MOV ECX,dword ptr [0x32566c]
 *   MOV [0x47ec48],EAX              -- start timestamp
 *   MOV EAX,[0x325668]
 *   MOV dword ptr [0x47ec4c],0x0
 *   MOV [0x47ec50],EAX
 *   MOV dword ptr [0x47ec54],ECX
 *
 * The two loads from 0x325668 / 0x32566c are hoisted by MSVC ahead of the
 * stores they feed; the C order below preserves the store order, which is the
 * observable one.  0x325668 and 0x32566c are an adjacent dword pair copied
 * verbatim into the adjacent pair 0x47ec50 / 0x47ec54.
 *
 * Explicit unknowns: the meaning of the byte flag at 0x3256b8, and what the
 * 0x325668/0x32566c pair holds.  Elsewhere in this tree 0x325668 is read both
 * as a frame-parity selector (rasterizer_xbox_decals.c) and as the low half of
 * a 64-bit counter incremented with ADD/ADC; nothing in THIS function
 * discriminates, so the two dwords are copied independently rather than as one
 * 64-bit quantity.  The zero written to 0x47ec4c is likewise unexplained --
 * only its position between the timestamp and the copied pair is proven. */
void FUN_0017ed30(void)
{
  *(unsigned char *)0x3256b8 = 1;
  *(unsigned int *)0x47ec48 = system_milliseconds();
  *(int *)0x47ec4c = 0;
  *(int *)0x47ec50 = *(int *)0x325668;
  *(int *)0x47ec54 = *(int *)0x32566c;
}


/* FUN_0017ed90 @ 0x17ed90 -- returns an index/vertex count selected by a
 * 16-bit tag at the head of the first buffer.
 *
 * Traced from disassembly (the decompiler loses the frame here and emits a
 * degenerate `void (void)` body; kb.json's int(void*,void*) decl matches
 * [EBP+8] / [EBP+0xC] / EAX).
 *
 *   0017ed96 XOR EAX,EAX          -- zero dominates both null exits
 *   0017ed9a JZ  0x17edc0         -- triangle_buffer == 0 -> 0
 *   0017eda2 JZ  0x17edbe         -- vertex_buffer   == 0 -> 0
 *   0017eda4 MOV CX,[EDX]         -- one 16-bit load, tested twice
 *   0017edab JNZ 0x17edb6         -- tag == 1: EAX = [EDX+4] + 2, own RET
 *   0017edb9 JNZ 0x17edbe         -- tag != 0: fall through with EAX = 0
 *   0017edbb MOV EAX,[ESI+4]      -- tag == 0: count from the second buffer
 *
 * The +2 on the tag==1 arm and the meaning of the tag itself are UNKNOWN; the
 * dword at +4 of each buffer is the only field this function touches, so no
 * struct is claimed.  The NOP at 0x17edbf lies inside the bounds entry and is
 * not expressible in C. */
int FUN_0017ed90(void *triangle_buffer, void *vertex_buffer)
{
  int count;
  short kind;

  count = 0;
  if (triangle_buffer != 0 && vertex_buffer != 0) {
    kind = *(short *)triangle_buffer;
    if (kind == 1) {
      return *(int *)((char *)triangle_buffer + 4) + 2;
    }
    if (kind == 0) {
      count = *(int *)((char *)vertex_buffer + 4);
    }
  }
  return count;
}

/* Block order note: the reference lays the whole positive path out first and
 * parks both early exits past the main RET -- the negative-argument decode at
 * 0x17eecb and the "widget == 0 -> return 0" tail at 0x17eef2 -- so the C is
 * written main-path-first to keep that order rather than as a leading guard. */
int rasterizer_frame_statistics_count_static_vertices(
  int vertices_per_primitive, int a2, int triangle_count)
{
  void *volatile base;
  void *widget;
  int unique_count;
  int index_count;
  int i;
  unsigned short previous;
  unsigned short index;
  short negated;

  unique_count = 0;
  if (vertices_per_primitive >= 0) {
    widget = rasterizer_widget_begin(vertices_per_primitive);
    base = widget;
    if (widget != 0) {
      assert_halt_msg_at(
        "triangle_count<RASTERIZER_MAXIMUM_TRIANGLES_PER_TRIANGLE_BUFFER",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_frame_statistics.c", 0xd9,
        triangle_count < 0x6000);
      assert_halt_msg_at(
        "rasterizer_frame_statistics_temp_buffer",
        "c:\\halo\\SOURCE\\rasterizer\\rasterizer_frame_statistics.c", 0xda,
        *(void **)0x47ec40 != 0);

      index_count = triangle_count * 3;
      csmemcpy(*(void **)0x47ec40, (void *)((char *)base + a2 * 6),
               triangle_count * 6);
      FUN_00091da0(*(void **)0x47ec40, index_count,
                   (void *)rasterizer_frame_statistics_sort_index_compare);

      previous = 0xffff;
      {
        const unsigned short *indices =
          (const unsigned short *)*(void **)0x47ec40;
        for (i = 0; i < index_count; i++) {
          index = indices[i];
          if (previous != index) {
            previous = index;
            unique_count++;
          }
        }
      }

      rasterizer_widget_set_texture(*(volatile int *)&vertices_per_primitive);
      return *(volatile int *)&unique_count;
    }
    return 0;
  }

  negated = (short)(-vertices_per_primitive);
  if (negated == 3 || negated == 4) {
    return triangle_count / ((int)negated - 2);
  }
  return negated;
}
