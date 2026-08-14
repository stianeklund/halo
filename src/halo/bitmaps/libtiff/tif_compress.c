/* ===========================================================================
 * tif_compress.c -- vendored libtiff codec registry and "not implemented"
 * codec stubs.
 *
 * The translation unit name is PROVEN, not inferred. The pristine image holds
 * libtiff's RCS id string at VA 0x2c9938:
 *
 *     "$Header: /usr/people/sam/tiff/libtiff/RCS/tif_compress.c,v 1.26"
 *     " 92/02/10 19:06:13 sam Exp $"
 *
 * i.e. upstream libtiff v3.2-era tif_compress.c, which opens with
 * `static char rcsid[] = "$Header: ... $";`. That string is 0x5c bytes with
 * its NUL, and 0x2c9938 + 0x5c is exactly 0x2c9994 -- the base of the codec
 * table this file walks. The RCS id and `_TIFFBuiltinCODECS` are the first two
 * data objects tif_compress.c declares, emitted back to back in declaration
 * order, so the table is this TU's data and the body below is this TU's code.
 *
 * kb.json files FUN_00064fe0 under props.obj (whose source mapping is
 * ai/props.c), which is a stale attribution artifact and not a claim about the
 * original link: the neighbouring FUN_00064ee0 (TIFFClose) landed in
 * ai/props.c for the same reason. The evidence above places this body with the
 * other vendored libtiff TUs, so it lives here. Do not let maintain.py fold it
 * into ai/props.c.
 *
 * Transcribed from upstream rather than reshaped from the decompiler (the
 * vendored-library rule: for a public library the upstream text is better
 * evidence of the original source form than Ghidra output). Ghidra reports
 * this body as `void FUN_00064fe0(void)` and hides the parameter behind an
 * `in_stack_00000004` local -- both wrong; see the disassembly below.
 * ======================================================================== */

/* -------------------------------------------------------------------------
 * FUN_00064fe0 -- upstream `_TIFFNoRowEncode`.
 *
 * Upstream body:
 *
 *     static int
 *     _TIFFNoRowEncode(TIFF* tif, tidata_t pp, tsize_t cc, tsample_t s)
 *     {
 *         (void) pp; (void) cc; (void) s;
 *         return (_TIFFNoEncode(tif, "scanline"));
 *     }
 *
 * Identified as the SCANLINE encode stub, not one of its seven siblings at
 * 0x65020..0x651a0: the pair of string literals pushed here is
 * ("%s %s encoding is not implemented", "scanline"). The siblings are
 * byte-for-byte the same shape and differ ONLY in those two literals --
 * 0x65020 pushes "strip", and the decode variants push a "decoding" format --
 * so the literal is the whole discriminator and 0x25f554 = "scanline" (read
 * out of the pristine image) is what fixes this body as the row/scanline one.
 *
 * Disassembly of 0x64fe0 (pristine cachebeta.xbe, capstone), which the
 * transcription is checked against instruction for instruction:
 *
 *   064fe0  push ebp                       ; frame is params only -- no
 *   064fe1  mov  ebp, esp                  ; `sub esp`, no _chkstk, so every
 *                                          ; value below is register resident
 *   064fe3  mov  edx, [ebp+8]              ; tif. THE function takes stack
 *                                          ; parameters; Ghidra's `void(void)`
 *                                          ; signature is wrong and its
 *                                          ; `in_stack_00000004` is param 1.
 *   064fe6  movzx ecx, word ptr [edx+0x3a] ; tif->td_compression, hoisted out
 *                                          ; of the loop below. ZERO-extended,
 *                                          ; so the field is unsigned 16-bit.
 *   064fea  mov  eax, 0x2c9994             ; _TIFFBuiltinCODECS
 *   064fef  nop                            ; align the loop head to 16
 *   064ff0  cmp  [eax+4], ecx              ; c->scheme == scheme. A DWORD
 *                                          ; compare, not `cmp word`, so
 *                                          ; Bungie's scheme slot is 32-bit
 *                                          ; even though upstream types it
 *                                          ; uint16. Memory operand first =>
 *                                          ; `c->scheme == scheme`, not the
 *                                          ; reversed spelling.
 *   064ff3  je   0x65001                   ;   match -> merge at the deref
 *   064ff5  add  eax, 0xc                  ; c++, sizeof(TIFFCodec) == 0xc
 *   064ff8  cmp  eax, 0x2c99c4             ; &_TIFFBuiltinCODECS[4]
 *   064ffd  jb   0x64ff0                   ; UNSIGNED (pointer) compare. The
 *                                          ; bound test is AFTER the advance,
 *                                          ; i.e. MSVC rotated a `for` whose
 *                                          ; first iteration is provably
 *                                          ; taken -- do not transcribe this
 *                                          ; as a hand-written do/while.
 *   064fff  xor  eax, eax                  ; NOT FOUND -> NULL codec pointer.
 *                                          ; There is no post-loop compare
 *                                          ; here, which is the shape of a
 *                                          ; helper with `return c;` inside
 *                                          ; the loop and `return 0;` after
 *                                          ; it, inlined at the deref below.
 *   065001  mov  eax, [eax]                ; c->name. On the not-found path
 *                                          ; this DEREFERENCES NULL. The
 *                                          ; original has no guard, so none is
 *                                          ; added: `TIFFFindCODEC(...)->name`
 *                                          ; is written unguarded and faults
 *                                          ; exactly where the original does.
 *   065003  mov  ecx, [edx]                ; tif->tif_name, loaded AFTER the
 *                                          ; codec name -- MSVC evaluates
 *                                          ; arguments right to left, which is
 *                                          ; why the last argument's operands
 *                                          ; come first.
 *   065005  push 0x25f554                  ; arg4 "scanline"
 *   06500a  push eax                       ; arg3 c->name
 *   06500b  push 0x25f530                  ; arg2 "%s %s encoding is not
 *                                          ;       implemented"
 *   065010  push ecx                       ; arg1 module = tif->tif_name
 *   065011  call 0x68a30                   ; TIFFError (varargs)
 *   065016  add  esp, 0x10                 ; cdecl, caller cleans 4 dwords.
 *                                          ; The hazard scanner's ARG_COUNT
 *                                          ; complaint (cleanup 4 vs decl 3)
 *                                          ; is a varargs false positive: the
 *                                          ; 4 pushes are module, fmt and two
 *                                          ; variadic arguments.
 *   065019  or   eax, 0xffffffff           ; return -1. NOT a void return --
 *                                          ; the second half of Ghidra's
 *                                          ; signature error.
 *   06501c  pop  ebp
 *   06501d  ret
 *
 * String literals read out of the pristine image: 0x25f530 =
 * "%s %s encoding is not implemented", 0x25f554 = "scanline". They are written
 * as literals so the compiler pools them normally instead of hard-casting
 * their addresses.
 *
 * One source ambiguity is unobservable in the binary and is called out rather
 * than guessed: the body could equally be a flat TIFFError call with
 * `_TIFFGetCodecName` inlined instead of the two-helper decomposition used
 * below. Both compile to these 0x3e bytes. The decomposition below is chosen
 * because it is upstream's, and because the seven siblings share it.
 * ------------------------------------------------------------------------ */

/* Partial view of the TIFF handle. The fuller recovery of this struct lives in
 * tif_open.c (`tiff_t`, offsets 0x00..0x138 with per-field evidence); this TU
 * repeats only the two offsets it touches, at the same offsets and with the
 * same field names and widths, because the struct is still a file-scope
 * typedef in each libtiff TU rather than a shared tiffiop.h. The pad_ run is
 * not a claim that those bytes are unused -- only that this function never
 * reads them. */
typedef struct tiff_s {
  /* 0x00 -- upstream's `char* tif_name`, the first member of `struct tiff`.
   * Pushed straight into the TIFFError module slot (`mov ecx,[edx]` at
   * 0x65003), the same use tif_open.c's TIFFFileName (0x6d850) and
   * tif_flush.c's FUN_00068890 make of it. */
  char *tif_name;
  char pad_004[0x36];
  /* 0x3a -- upstream's `td_compression`, inside a nested `TIFFDirectory
   * tif_dir` upstream but flattened into TIFF in this build (the same
   * flattening tif_open.c recovers independently). Read with
   * `movzx ecx, word ptr [edx+0x3a]` at 0x64fe6, a 16-bit ZERO-extending
   * load, so the field is `unsigned short` and not upstream's plain int. */
  unsigned short td_compression;
} tiff_t;

/* Upstream's `TIFFCodec`. Stride is PROVEN 0xc by the `add eax,0xc` at 0x64ff5
 * and by the table span 0x2c9994..0x2c99c4 holding exactly four entries. */
typedef struct tiff_codec_s {
  /* 0x00 -- codec display name. Dereferenced at 0x65001. The four live values
   * read out of the pristine image are "Null", "LZW", "PackBits", "JPEG". */
  const char *name;
  /* 0x04 -- upstream types this `uint16 scheme`. This build compares it with a
   * full DWORD `cmp [eax+4], ecx` at 0x64ff0, and the live values (1, 5,
   * 32773, 6) are stored as dwords, so Bungie's slot is 32-bit. Declaring it
   * `unsigned short` would produce a word compare and would not match. */
  unsigned long scheme;
  /* 0x08 -- upstream's `TIFFInitMethod init`, the codec's setup routine. The
   * live values are 0x68970 (Null), 0x6d2d0 (LZW), 0x6dd50 (PackBits) and
   * 0x6c5d0 (JPEG). Its signature is NOT observed by this TU -- nothing here
   * calls through the slot -- so it is left as a generic pointer rather than
   * importing upstream's `int (*)(TIFF*, int)` prototype on faith. */
  void *init;
} tiff_codec_t;

/* Upstream's `_TIFFBuiltinCODECS`. Addressed at its original VA rather than
 * redeclared as a local `static const` array: the table is live data that the
 * still-unported codec-registry entry points in this TU read and (upstream)
 * TIFFRegisterCODEC mutates, so a private copy here would silently diverge
 * from the one the rest of the image uses. A literal address also emits the
 * `mov eax, 0x2c9994` / `cmp eax, 0x2c99c4` immediates the reference has,
 * with none of the `__imp_` indirection an HDATA import would add.
 *
 * The element count is PROVEN by the table span: 0x2c99c4 - 0x2c9994 is 0x30
 * bytes, exactly four 0xc-byte entries, and the dword at 0x2c99c4 is unrelated
 * data (not a NULL sentinel entry), so the loop must be bounded by the count
 * and cannot be a name-terminated walk. */
#define TIFF_NCODECS 4
#define tiff_builtin_codecs ((const tiff_codec_t *)0x2c9994)

/* Upstream `TIFFFindCODEC`. Kept as a static helper rather than hand-inlined:
 * the original inlined it (this whole body is one basic-block chain with a
 * single CALL), and hand-inlining a vendored helper reorders the emitted
 * blocks. `__inline` forces the inline regardless of the /Ob level so the
 * shape does not depend on the optimizer's cost model.
 *
 * Returns the codec ENTRY, not its name: the not-found path sets the result to
 * NULL at 0x64fff and the `->name` load at 0x65001 sits at the merge of both
 * paths, so what crosses the merge is a pointer to TIFFCodec. A name-returning
 * helper would have to materialize a fallback string on the miss path instead
 * of zeroing the register. */
static __inline const tiff_codec_t *TIFFFindCODEC(unsigned short scheme) {
  const tiff_codec_t *c;

  for (c = tiff_builtin_codecs; c < tiff_builtin_codecs + TIFF_NCODECS; c++) {
    if (c->scheme == scheme) {
      return c;
    }
  }
  return 0;
}

/* Upstream `_TIFFNoEncode`. The eight stubs at 0x64fe0..0x651a0 differ only in
 * the `method` word and in encoding-vs-decoding wording, which is what upstream
 * factors out here.
 *
 * The `->name` dereference is deliberately UNGUARDED. On a compression scheme
 * that is not one of the four built-in codecs the original executes
 * `mov eax,[eax]` with EAX == 0 and faults; adding a NULL check would change
 * behavior and would cost the match. */
static __inline int _TIFFNoEncode(tiff_t *tif, const char *method) {
  FUN_00068a30(tif->tif_name, "%s %s encoding is not implemented",
               TIFFFindCODEC(tif->td_compression)->name, method);
  return -1;
}

/**
 * Row-encode entry point installed for compression schemes whose encoder is
 * not present in this build: reports the scheme by name and fails.
 *
 * Installed into the codec method table by pointer rather than called
 * directly, which is why the image contains no CALL to this address.
 *
 * @param tif_ TIFF handle.
 * @param pp Sample data to encode. Unused -- the stub never encodes anything.
 * @param cc Byte count of `pp`. Unused.
 * @param s Sample number. Unused.
 * @return Always -1 (failure).
 */
int FUN_00064fe0(void *tif_, char *pp, int cc, int s) {
  tiff_t *tif = (tiff_t *)tif_;

  (void)pp;
  (void)cc;
  (void)s;
  return _TIFFNoEncode(tif, "scanline");
}

/* -------------------------------------------------------------------------
 * FUN_00065020 -- upstream `_TIFFNoStripEncode`.
 *
 * Upstream body:
 *
 *     static int
 *     _TIFFNoStripEncode(TIFF* tif, tidata_t pp, tsize_t cc, tsample_t s)
 *     {
 *         (void) pp; (void) cc; (void) s;
 *         return (_TIFFNoEncode(tif, "strip"));
 *     }
 *
 * Byte-for-byte the same 0x3e-byte shape as FUN_00064fe0 above -- same frame,
 * same hoisted `movzx`, same rotated table walk, same unguarded `->name`
 * deref, same `or eax,-1`. The ONLY difference is the fourth argument's
 * literal, and that literal is the whole discriminator between the eight
 * siblings at 0x64fe0..0x651a0. Read out of the pristine image:
 * 0x25f560 = "strip" (0x25f554 = "scanline" is what fixes 0x64fe0 as the
 * scanline stub), and the format literal is the shared encode-side
 * 0x25f530 = "%s %s encoding is not implemented" -- an `encoding` string, so
 * this is the STRIP ENCODE stub and not one of the decode variants.
 *
 * Disassembly of 0x65020 (pristine cachebeta.xbe, capstone). Only the
 * annotations that differ from 0x64fe0 are repeated; see that body above for
 * the per-instruction reasoning behind the loop rotation, the 32-bit `scheme`
 * compare, the NULL-deref merge and the varargs cleanup.
 *
 *   065020  push ebp
 *   065021  mov  ebp, esp                  ; params only, no `sub esp`
 *   065023  mov  edx, [ebp+8]              ; tif -- a stack parameter, so the
 *                                          ; kb `void FUN_00065020(void)`
 *                                          ; decl was wrong on both counts
 *   065026  movzx ecx, word ptr [edx+0x3a] ; tif->td_compression, u16
 *   06502a  mov  eax, 0x2c9994             ; _TIFFBuiltinCODECS
 *   06502f  nop                            ; loop-head alignment padding --
 *                                          ; not reproducible from C, and not
 *                                          ; something to chase
 *   065030  cmp  [eax+4], ecx              ; c->scheme == scheme
 *   065033  je   0x65041
 *   065035  add  eax, 0xc
 *   065038  cmp  eax, 0x2c99c4             ; &_TIFFBuiltinCODECS[4]
 *   06503d  jb   0x65030                   ; unsigned; test after the advance
 *   06503f  xor  eax, eax                  ; not found -> NULL codec
 *   065041  mov  eax, [eax]                ; c->name; NULL-derefs on the miss
 *                                          ; path, deliberately unguarded
 *   065043  mov  ecx, [edx]                ; tif->tif_name, loaded second
 *                                          ; (right-to-left argument order)
 *   065045  push 0x25f560                  ; arg4 "strip"   <-- the only
 *                                          ;                    difference
 *   06504a  push eax                       ; arg3 c->name
 *   06504b  push 0x25f530                  ; arg2 format
 *   065050  push ecx                       ; arg1 module
 *   065051  call 0x68a30                   ; TIFFError (varargs). The
 *                                          ; ARG_COUNT hazard (cleanup 4 vs
 *                                          ; decl 3) is the vararg, not an
 *                                          ; ABI mismatch.
 *   065056  add  esp, 0x10
 *   065059  or   eax, 0xffffffff           ; return -1
 *   06505c  pop  ebp
 *   06505d  ret
 * ------------------------------------------------------------------------ */

/**
 * Strip-encode entry point installed for compression schemes whose encoder is
 * not present in this build: reports the scheme by name and fails.
 *
 * Installed into the codec method table by pointer rather than called
 * directly, which is why the image contains no CALL to this address.
 *
 * @param tif_ TIFF handle.
 * @param pp Strip data to encode. Unused -- the stub never encodes anything.
 * @param cc Byte count of `pp`. Unused.
 * @param s Sample number. Unused.
 * @return Always -1 (failure).
 */
int FUN_00065020(void *tif_, char *pp, int cc, int s) {
  tiff_t *tif = (tiff_t *)tif_;

  (void)pp;
  (void)cc;
  (void)s;
  return _TIFFNoEncode(tif, "strip");
}

/* -------------------------------------------------------------------------
 * FUN_00065060 -- upstream `_TIFFNoTileEncode`.
 *
 * Upstream body:
 *
 *     static int
 *     _TIFFNoTileEncode(TIFF* tif, tidata_t pp, tsize_t cc, tsample_t s)
 *     {
 *         (void) pp; (void) cc; (void) s;
 *         return (_TIFFNoEncode(tif, "tile"));
 *     }
 *
 * Third member of the encode-side triple, and byte-for-byte the same 0x3e-byte
 * shape as FUN_00064fe0 and FUN_00065020 above: same frame, same hoisted
 * `movzx`, same rotated table walk, same unguarded `->name` deref, same
 * `or eax,-1`. The ONLY difference is the fourth argument's literal.
 *
 * The literal fixes WHICH sibling this is, and it was read out of the pristine
 * image rather than inferred from the address ordering: 0x25f568 = "tile",
 * which sits immediately after 0x25f554 = "scanline" and 0x25f560 = "strip" in
 * the string pool, in upstream's declaration order. The format literal is the
 * shared encode-side 0x25f530 = "%s %s encoding is not implemented"; the
 * decode-side siblings at 0x650a0..0x651a0 use 0x25f570 = "%s %s decoding is
 * not implemented" instead, so an `encoding` format proves this is the TILE
 * ENCODE stub and not the tile DECODE one.
 *
 * Disassembly of 0x65060 (pristine cachebeta.xbe, capstone). Only the
 * annotations that differ from 0x64fe0 are repeated; see that body above for
 * the per-instruction reasoning behind the loop rotation, the 32-bit `scheme`
 * compare, the NULL-deref merge and the varargs cleanup.
 *
 *   065060  push ebp
 *   065061  mov  ebp, esp                  ; params only, no `sub esp`, so the
 *                                          ; lift must declare no stack locals
 *   065063  mov  edx, [ebp+8]              ; tif -- a stack parameter, so the
 *                                          ; kb `void FUN_00065060(void)` decl
 *                                          ; was wrong on both counts (Ghidra
 *                                          ; hid it as `in_stack_00000004`)
 *   065066  movzx ecx, word ptr [edx+0x3a] ; tif->td_compression, u16, hoisted
 *                                          ; ONCE above the loop (loop-invariant)
 *   06506a  mov  eax, 0x2c9994             ; _TIFFBuiltinCODECS
 *   06506f  nop                            ; loop-head alignment padding --
 *                                          ; not reproducible from C
 *   065070  cmp  [eax+4], ecx              ; c->scheme == scheme (memory operand
 *                                          ; first => spell it `c->scheme ==`)
 *   065073  je   0x65081
 *   065075  add  eax, 0xc                  ; stride 0xc == sizeof(TIFFCodec)
 *   065078  cmp  eax, 0x2c99c4             ; &_TIFFBuiltinCODECS[4]; the bound
 *                                          ; is an ADDRESS compare, which is
 *                                          ; what proves the loop is
 *                                          ; count-bounded and not a
 *                                          ; `while (c->name)` sentinel walk --
 *                                          ; a sentinel walk would emit
 *                                          ; `cmp dword [eax],0` here. (The
 *                                          ; dword at 0x2c99c4 does read 0 in
 *                                          ; this image, but nothing loads it.)
 *   06507d  jb   0x65070                   ; unsigned; test after the advance
 *   06507f  xor  eax, eax                  ; not found -> NULL codec, then
 *                                          ; FALLS THROUGH into the deref
 *   065081  mov  eax, [eax]                ; c->name; NULL-derefs on the miss
 *                                          ; path, deliberately unguarded
 *   065083  mov  ecx, [edx]                ; tif->tif_name; EDX is preserved
 *                                          ; across the whole loop (only EAX
 *                                          ; and ECX are touched), so there is
 *                                          ; no register-aliasing risk here
 *   065085  push 0x25f568                  ; arg4 "tile"     <-- the only
 *                                          ;                     difference
 *   06508a  push eax                       ; arg3 c->name
 *   06508b  push 0x25f530                  ; arg2 format
 *   065090  push ecx                       ; arg1 module
 *   065091  call 0x68a30                   ; TIFFError (varargs). The
 *                                          ; ARG_COUNT hazard (cleanup 4 vs
 *                                          ; decl 3) is the vararg, not an
 *                                          ; ABI mismatch.
 *   065096  add  esp, 0x10                 ; cdecl, 4 dword args
 *   065099  or   eax, 0xffffffff           ; return -1 -- dropped entirely by
 *                                          ; the Ghidra `void` decompile
 *   06509c  pop  ebp
 *   06509d  ret
 * ------------------------------------------------------------------------ */

/**
 * Tile-encode entry point installed for compression schemes whose encoder is
 * not present in this build: reports the scheme by name and fails.
 *
 * Installed into the codec method table by pointer rather than called
 * directly, which is why the image contains no CALL to this address.
 *
 * @param tif_ TIFF handle.
 * @param pp Tile data to encode. Unused -- the stub never encodes anything.
 * @param cc Byte count of `pp`. Unused.
 * @param s Sample number. Unused.
 * @return Always -1 (failure).
 */
int FUN_00065060(void *tif_, char *pp, int cc, int s) {
  tiff_t *tif = (tiff_t *)tif_;

  (void)pp;
  (void)cc;
  (void)s;
  return _TIFFNoEncode(tif, "tile");
}

/* -------------------------------------------------------------------------
 * FUN_000650a0 -- upstream `_TIFFNoDecode`.
 *
 * Upstream body:
 *
 *     static int
 *     _TIFFNoDecode(TIFF* tif, const char* method)
 *     {
 *         const TIFFCodec* c = TIFFFindCODEC(tif->tif_dir.td_compression);
 *         TIFFError(tif->tif_name, "%s %s decoding is not implemented",
 *                   c->name, method);
 *         return (-1);
 *     }
 *
 * This is the decode-side counterpart of `_TIFFNoEncode` above, and unlike the
 * encode helper it survives as a real out-of-line function: the two stack
 * parameters at [EBP+8] and [EBP+0xc] are exactly upstream's `tif` and
 * `method`, so the decode stubs at 0x650e0/0x65120/0x65160 CALL this address
 * instead of carrying an inlined copy. That is why the body below is written as
 * the entry point itself rather than as a `static __inline` helper with a
 * one-line wrapper -- an inline helper would emit no code at this address.
 *
 * Ghidra reports it as `void FUN_000650a0(void)` and hides `tif` behind an
 * `in_stack_00000004` local while dropping `method` from the call entirely.
 * All three halves of that are wrong; the disassembly below is the authority.
 *
 * Verbatim, from the pristine image (0x650a0..0x650dd, 23 instructions):
 *
 *   0650a0  push ebp
 *   0650a1  mov  ebp, esp                   ; no `sub esp` -- zero locals
 *   0650a3  mov  edx, dword ptr [ebp+8]     ; param1 tif, held in EDX across
 *                                           ; the whole loop
 *   0650a6  movzx ecx, word ptr [edx+0x3a]  ; tif->td_compression, a 16-bit
 *                                           ; field zero-extended to 32 bits
 *   0650aa  mov  eax, 0x2c9994              ; _TIFFBuiltinCODECS
 *   0650af  nop                             ; alignment padding
 *   0650b0  cmp  dword ptr [eax+4], ecx     ; c->scheme == scheme; the memory
 *                                           ; operand is first, so spell the
 *                                           ; comparison that way round
 *   0650b3  je   0x650c1                    ; found
 *   0650b5  add  eax, 0xc                   ; ++c (sizeof(TIFFCodec) == 0xc)
 *   0650b8  cmp  eax, 0x2c99c4              ; &_TIFFBuiltinCODECS[4]; an
 *                                           ; ADDRESS compare, which is what
 *                                           ; proves the loop is count-bounded
 *                                           ; and not a `while (c->name)` walk
 *   0650bd  jb   0x650b0                    ; unsigned <, loop
 *   0650bf  xor  eax, eax                   ; not found -> NULL, and falls
 *                                           ; THROUGH into the shared tail
 *   0650c1  mov  ecx, dword ptr [ebp+0xc]   ; param2 method, loaded late and
 *                                           ; only for the call
 *   0650c4  mov  eax, dword ptr [eax]       ; c->name -- at the merge of the
 *                                           ; found and not-found paths, so on
 *                                           ; an unknown scheme this faults.
 *                                           ; UNGUARDED on purpose.
 *   0650c6  push ecx                        ; arg4 method
 *   0650c7  mov  ecx, dword ptr [edx]       ; tif->tif_name. Note ECX is reused
 *                                           ; for two different values across
 *                                           ; three instructions; EDX is what
 *                                           ; carries `tif` this far.
 *   0650c9  push eax                        ; arg3 c->name
 *   0650ca  push 0x25f570                   ; arg2 "%s %s decoding is not
 *                                           ; implemented" -- an immediate
 *                                           ; push of the literal's address.
 *                                           ; Distinct from the encode side's
 *                                           ; 0x25f530. (0x25f578 is a pointer
 *                                           ; INTO this string at offset 8,
 *                                           ; not a separate literal.)
 *   0650cf  push ecx                        ; arg1 module
 *   0650d0  call 0x68a30                    ; TIFFError (varargs). The
 *                                           ; ARG_COUNT hazard (cleanup 4 vs
 *                                           ; decl 3) is the vararg, not an
 *                                           ; ABI mismatch.
 *   0650d5  add  esp, 0x10                  ; cdecl, 4 dword args
 *   0650d8  or   eax, 0xffffffff            ; return -1, emitted AFTER the
 *                                           ; call, so a separate statement
 *   0650db  pop  ebp
 *   0650dc  ret
 * ------------------------------------------------------------------------ */

/**
 * Reports that the decoder for this TIFF's compression scheme is absent from
 * this build, naming the scheme and the failed access method.
 *
 * Shared tail of the row/strip/tile decode stubs installed for schemes with no
 * decoder present.
 *
 * @param tif_ TIFF handle.
 * @param method Access method that was attempted -- "scanline", "strip" or
 *        "tile".
 * @return Always -1 (failure).
 */
int FUN_000650a0(void *tif_, const char *method) {
  tiff_t *tif = (tiff_t *)tif_;

  FUN_00068a30(tif->tif_name, "%s %s decoding is not implemented",
               TIFFFindCODEC(tif->td_compression)->name, method);
  return -1;
}

/* -------------------------------------------------------------------------
 * `_TIFFNoDecode`, inline copy.
 *
 * The decode side needs the helper BOTH out-of-line (as FUN_000650a0 above,
 * which is upstream's `_TIFFNoDecode` emitted as a real function) AND inlined
 * into the row/strip/tile stubs below, which each carry their own copy of the
 * codec-table scan and the TIFFError call rather than a CALL to 0x650a0.
 * A single source function cannot produce both in this build -- a
 * `static __inline` function that is never address-taken emits no out-of-line
 * body, and a plain extern function is never inlined at /Ob1 -- so the body is
 * written twice: once as the named entry point, once here as the inline the
 * stubs expand. The duplication is a build artifact, not a claim that the
 * original had two copies in its source.
 *
 * Declared here, above its first caller, because MSVC 7.1 is single-pass and
 * will not inline a definition it has not seen yet.
 *
 * Identical in every respect to `_TIFFNoEncode` except the format literal
 * (0x25f570 "%s %s decoding is not implemented" instead of 0x25f530). The
 * `->name` dereference is UNGUARDED for the same reason it is there: the miss
 * path zeroes EAX and falls through into the shared `mov eax,[eax]`.
 * ------------------------------------------------------------------------ */
static __inline int _TIFFNoDecode(tiff_t *tif, const char *method) {
  FUN_00068a30(tif->tif_name, "%s %s decoding is not implemented",
               TIFFFindCODEC(tif->td_compression)->name, method);
  return -1;
}

/* -------------------------------------------------------------------------
 * FUN_000650e0 -- upstream `_TIFFNoRowDecode`.
 *
 * Upstream body:
 *
 *     static int
 *     _TIFFNoRowDecode(TIFF* tif, tidata_t pp, tsize_t cc, tsample_t s)
 *     {
 *         (void) pp; (void) cc; (void) s;
 *         return (_TIFFNoDecode(tif, "scanline"));
 *     }
 *
 * Verbatim, from the pristine image (0x650e0..0x6511d, 23 instructions).
 * `_TIFFNoDecode` is INLINED here -- there is no CALL to 0x650a0, the only
 * CALL in the body is TIFFError:
 *
 *   0650e0  push  ebp
 *   0650e1  mov   ebp, esp                   ; no `sub esp` -- zero locals
 *   0650e3  mov   edx, dword ptr [ebp+8]     ; param1 tif, kept in EDX across
 *                                            ; the whole loop
 *   0650e6  movzx ecx, word ptr [edx+0x3a]   ; tif->td_compression, a 16-bit
 *                                            ; ZERO-extending load, which is
 *                                            ; what types the field
 *                                            ; `unsigned short`
 *   0650ea  mov   eax, 0x2c9994              ; _TIFFBuiltinCODECS
 *   0650ef  nop                              ; loop-head alignment padding
 *   0650f0  cmp   dword ptr [eax+4], ecx     ; c->scheme == scheme; memory
 *                                            ; operand first
 *   0650f3  je    0x65101                    ; found
 *   0650f5  add   eax, 0xc                   ; ++c
 *   0650f8  cmp   eax, 0x2c99c4              ; &_TIFFBuiltinCODECS[4]; an
 *                                            ; ADDRESS compare, proving the
 *                                            ; loop is count-bounded
 *   0650fd  jb    0x650f0                    ; unsigned <, loop
 *   0650ff  xor   eax, eax                   ; miss -> NULL, falls THROUGH
 *   065101  mov   eax, dword ptr [eax]       ; c->name, at the merge of both
 *                                            ; paths -- faults on an unknown
 *                                            ; scheme. UNGUARDED on purpose.
 *   065103  mov   ecx, dword ptr [edx]       ; tif->tif_name, hoisted above
 *                                            ; the argument pushes
 *   065105  push  0x25f554                   ; arg4 "scanline"
 *   06510a  push  eax                        ; arg3 c->name
 *   06510b  push  0x25f570                   ; arg2 "%s %s decoding is not
 *                                            ; implemented"
 *   065110  push  ecx                        ; arg1 module (tif->tif_name)
 *   065111  call  0x68a30                    ; TIFFError (varargs). The
 *                                            ; ARG_COUNT hazard (cleanup 0x10
 *                                            ; vs decl 3 fixed params) is the
 *                                            ; vararg, not an ABI mismatch.
 *   065116  add   esp, 0x10                  ; cdecl, 4 dword args
 *   065119  or    eax, 0xffffffff            ; return -1, emitted after the
 *                                            ; call, so a separate statement
 *   06511c  pop   ebp
 *   06511d  ret
 *
 * kb.json declares this `void FUN_000650e0(void)` and Ghidra hides `tif`
 * behind an `in_stack_00000004` local. Both halves are wrong: `mov edx,[ebp+8]`
 * proves a stack parameter and `or eax,-1` proves an `int` return. Widened to
 * the four-parameter codec-method form the encode siblings at
 * 0x64fe0/0x65020/0x65060 already carry -- `pp`, `cc` and `s` are NOT observed
 * in this body (a stub reads none of them) and are taken from upstream's
 * prototype plus the shared method-table signature; being trailing cdecl slots
 * they have no effect on the emitted code either way.
 * ------------------------------------------------------------------------ */

/**
 * Row-decode entry point installed for compression schemes whose decoder is
 * not present in this build: reports the scheme by name and fails.
 *
 * Installed into the codec method table by pointer rather than called
 * directly, which is why the image contains no CALL to this address.
 *
 * @param tif_ TIFF handle.
 * @param pp Destination buffer for decoded samples. Unused -- the stub never
 *        decodes anything.
 * @param cc Byte count of `pp`. Unused.
 * @param s Sample number. Unused.
 * @return Always -1 (failure).
 */
int FUN_000650e0(void *tif_, char *pp, int cc, int s) {
  tiff_t *tif = (tiff_t *)tif_;

  (void)pp;
  (void)cc;
  (void)s;
  return _TIFFNoDecode(tif, "scanline");
}
