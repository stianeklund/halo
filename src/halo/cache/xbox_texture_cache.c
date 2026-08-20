/* Xbox texture cache: retrieve and block on hardware texture data.
 * Source: c:\halo\SOURCE\cache\xbox_texture_cache.c */
#ifdef HALO_RETAIL64
#define HALO_TEXTURE_CACHE_SIZE 0x800000
#else
#define HALO_TEXTURE_CACHE_SIZE 0x1600000
#endif

#define HALO_TEXTURE_CACHE_PAGE_BITS 0xe
#define HALO_TEXTURE_CACHE_PAGE_SIZE (1 << HALO_TEXTURE_CACHE_PAGE_BITS)
#define HALO_TEXTURE_CACHE_PAGE_COUNT \
  (HALO_TEXTURE_CACHE_SIZE >> HALO_TEXTURE_CACHE_PAGE_BITS)
#define HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE 0x104000
#define HALO_TEXTURE_CACHE_STEAL_GUARD_PAGES \
  (HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE >> HALO_TEXTURE_CACHE_PAGE_BITS)
#define HALO_TEXTURE_CACHE_STEALABLE_PAGES \
  (HALO_TEXTURE_CACHE_PAGE_COUNT - (HALO_TEXTURE_CACHE_STEAL_GUARD_PAGES * 2))

/* texture_cache_delete (0x1be920)
 *
 * Teardown: dispose the bitmap-entry data array, then the LRU-vector cache.
 * Order is load-bearing (the data array holds cache-block indices). */
void texture_cache_delete(void)
{
  data_dispose(*(data_t **)0x4ea978);
  lruv_cache_dispose(*(void **)0x4ea980);
}

/* texture_cache_open (0x1be940)
 *
 * 001be940  MOV EAX,[0x004ea978]   ; load the bitmap-entry data array pointer
 * 001be945  PUSH EAX
 * 001be946  CALL 0x00119b20        ; data_delete_all(data)
 * 001be94b  POP ECX                ; cdecl cleanup of the single arg
 *
 * Cache open only empties the bitmap-entry data array; the LRU-vector cache
 * (0x4ea980) is untouched here, unlike texture_cache_delete above. */
void texture_cache_open(void)
{
  data_delete_all(*(data_t **)0x4ea978);
}

/* texture_cache_idle (0x1be950)
 *
 * 001be950  MOV EAX,[0x004ea980]   ; load the LRU-vector cache pointer
 * 001be955  PUSH EAX
 * 001be956  CALL 0x0011d8d0        ; lruv_idle(cache)
 * 001be95b  POP ECX                ; cdecl cleanup of the single arg
 *
 * Idle work is delegated wholly to the LRU-vector cache; unlike
 * texture_cache_open/delete, the bitmap-entry data array (0x4ea978) is not
 * touched here. */
void texture_cache_idle(void)
{
  lruv_idle(*(void **)0x4ea980);
}

/* texture_cache_bitmap_new (0x1be960)
 *
 * Register a bitmap_data with the texture cache: mark it cached, clear the
 * cache-block/hardware-texture bookkeeping, rebase its pixel offset onto the
 * owning 'bitm' tag's pixel-data base, and record its pixel-data size.
 *
 * bitmap_data offsets used (all reached through ESI in the reference):
 *   +0x0e  word  flags        (bit 0x80 = _bitmap_cached_bit)
 *   +0x18  long  pixels offset (relative -> absolute via tag base at +0x38)
 *   +0x1c  long  pixel data size
 *   +0x20  long  owning bitmap tag index
 *   +0x24  long  cache block index (NONE)
 *   +0x28  long  cleared bookkeeping
 *   +0x2c  long  cleared bookkeeping
 *
 * The +0x24/+0x28/+0x2c trio is stored twice — once before the two calls and
 * once after — exactly as the reference does; both store groups are kept.
 *
 * 001be9c7  ADD ESP,0xc  is MSVC's merged cdecl cleanup for BOTH calls
 * (tag_get's 2 pushes + bitmap_get_pixel_data_size's 1 push), not a 3-argument
 * call to bitmap_get_pixel_data_size. */
void texture_cache_bitmap_new(int tag_index, void *bitmap)
{
  char *bm = (char *)bitmap;
  char *bitmap_tag;
  int pixel_data_size;

  if ((*(uint16_t *)(bm + 0xe) & 0x80) != 0) {
    display_assert("!TEST_FLAG(bitmap->flags, _bitmap_cached_bit)",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x9d, true);
    system_exit(-1);
  }

  *(uint16_t *)(bm + 0xe) |= 0x80;
  *(int *)(bm + 0x24) = -1;
  *(int *)(bm + 0x2c) = 0;
  *(int *)(bm + 0x28) = 0;

  bitmap_tag = (char *)tag_get(0x6269746d /* 'bitm' */, tag_index);
  *(int *)(bm + 0x18) += *(int *)(bitmap_tag + 0x38);

  pixel_data_size = bitmap_get_pixel_data_size(bitmap);
  *(int *)(bm + 0x20) = tag_index;
  *(int *)(bm + 0x2c) = 0;
  *(int *)(bm + 0x28) = 0;
  *(int *)(bm + 0x1c) = pixel_data_size;
  *(int *)(bm + 0x24) = -1;
}

/* texture_cache_bitmap_delete (0x1be9f0)
 *
 * Inverse of texture_cache_bitmap_new: if the bitmap_data is still marked
 * cached, release its LRU-vector cache block and clear the bookkeeping.
 *
 * 001be9f4  MOV ESI,[EBP + 0x8]   ; single cdecl arg (bitmap_data *)
 * 001be9f7  MOV AL,byte [ESI+0xe] ; flags low byte
 * 001be9fa  TEST AL,AL
 * 001be9fc  JNS 0x001bea27        ; cached bit (0x80) clear -> nothing to do
 * 001be9fe  MOV EAX,[ESI + 0x24]  ; cache block index
 * 001bea01  CMP EAX,-0x1          ; NONE
 * 001bea06  PUSH EAX              ; arg2 = block index
 * 001bea0c  PUSH EAX              ; arg1 = [0x4ea980] lruv cache
 * 001bea0d  CALL 0x0011d8f0       ; lruv_block_delete(cache, block)
 * 001bea15  AND byte [ESI+0xe],0x7f
 *
 * bitmap_data offsets used (see texture_cache_bitmap_new above):
 *   +0x0e  word  flags (bit 0x80 = _bitmap_cached_bit)
 *   +0x24  long  cache block index (NONE == -1)
 *   +0x2c  long  cleared bookkeeping
 *
 * The flag test and clear are byte-width in the reference even though the
 * field is the same word flags that texture_cache_bitmap_new ORs 0x80 into:
 * bit 7 of the low byte is the sign bit, so the test is a signed-char compare
 * (MOV AL / TEST AL,AL / JNS) and the clear is AND byte,0x7f. +0x28 is NOT
 * re-cleared here, unlike bitmap_new which clears the whole +0x24/+0x28/+0x2c
 * trio. */
void texture_cache_bitmap_delete(void *bitmap)
{
  char *bm = (char *)bitmap;
  int cache_block_index;

  if (*(int8_t *)(bm + 0xe) < 0) {
    cache_block_index = *(int *)(bm + 0x24);
    if (cache_block_index != -1)
      lruv_block_delete(*(void **)0x4ea980, cache_block_index);

    *(uint8_t *)(bm + 0xe) &= 0x7f;
    *(int *)(bm + 0x24) = -1;
    *(int *)(bm + 0x2c) = 0;
  }
}

void *xbox_texture_cache_steal_memory(unsigned int size)
{
  int page_count = ((int)size / HALO_TEXTURE_CACHE_PAGE_SIZE) + 1;
  int remaining_page_count = HALO_TEXTURE_CACHE_STEALABLE_PAGES - page_count;
  char *base = (char *)FUN_001bdd60() +
               (remaining_page_count * HALO_TEXTURE_CACHE_PAGE_SIZE);
  unsigned int stolen_size = page_count * HALO_TEXTURE_CACHE_PAGE_SIZE;
  char *guard_end = base + HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE + stolen_size;

  if (remaining_page_count <= 0) {
    display_assert("remaining_page_count>0",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x13f,
                   true);
    system_exit(-1);
  }

  if (*(int8_t *)0x4ea984 != 0) {
    display_assert("!xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x140,
                   true);
    system_exit(-1);
  }

  lruv_resize(*(void **)0x4ea980, remaining_page_count);
  physical_memory_protect(base + HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE,
                          stolen_size, 4);
  physical_memory_protect(base, HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE, 2);
  physical_memory_protect(guard_end, HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE, 2);
  *(int8_t *)0x4ea984 = 1;
  return base + HALO_TEXTURE_CACHE_STEAL_GUARD_SIZE;
}

void xbox_texture_cache_return_memory(void)
{
  if (*(int8_t *)0x4ea984 == 0) {
    display_assert("xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x159,
                   true);
    system_exit(-1);
  }

  lruv_resize(*(void **)0x4ea980, HALO_TEXTURE_CACHE_PAGE_COUNT);
  physical_memory_protect(FUN_001bdd60(), HALO_TEXTURE_CACHE_SIZE, 0x404);
  *(int8_t *)0x4ea984 = 0;
}

/* FUN_001beb70 (0x1beb70)
 *
 * Texture-cache block -> name callback. Not called directly: its address is
 * handed to FUN_0011db90 by xbox_texture_cache_get_hardware_format
 * (001bf6f8 references 0x1beb70 as data), alongside the LRU-vector cache, so
 * the debug dump can label each cache block with the owning 'bitm' tag name.
 *
 * 001beb73  MOV EAX,[EBP + 0x8]     ; single cdecl arg: cache block index
 * 001beb76  MOV ECX,[0x004ea978]    ; bitmap-entry data array
 * 001beb7c  PUSH EAX                ; arg2 = datum handle
 * 001beb7d  PUSH ECX                ; arg1 = data array
 * 001beb7e  CALL 0x00119320         ; datum_get
 * 001beb83  MOV EDX,[EAX + 0x8]     ; cache_entry+8 = owning bitmap_data
 * 001beb86  MOV EAX,[EDX + 0x20]    ; bitmap_data+0x20 = owning 'bitm' index
 * 001beb89  PUSH EAX
 * 001beb8a  CALL 0x001ba1f0         ; tag_get_name
 * 001beb8f  ADD ESP,0xc             ; merged cdecl cleanup for BOTH calls
 *                                   ; (datum_get's 2 pushes + tag_get_name's
 * 1), ; not a 3-argument tag_get_name call 001beb92  POP EBP / RET
 *
 * EAX is never touched after the CALL, so tag_get_name's result IS this
 * function's return value. Ghidra's `void __cdecl FUN_001beb70(void)` with an
 * `in_stack_00000004` local misses both the parameter and the implicit-EAX
 * return; the disassembly above is authoritative for the signature.
 *
 * cache_entry+8 holding the bitmap_data pointer is proven locally by
 * xbox_texture_cache_request, which stores hardware_format there right after
 * data_new_datum; +0x20 is the owning tag index that texture_cache_bitmap_new
 * writes. The whole body stays one nested expression so the two cdecl cleanups
 * merge the way the reference merges them. */
const char *FUN_001beb70(int cache_block_index)
{
  return tag_get_name(
    *(int32_t *)(*(char **)((char *)datum_get(*(data_t **)0x4ea978,
                                              cache_block_index) +
                            8) +
                 0x20));
}

/* bitmap_format_to_d3d_linear_format (0x1beba0)
 *
 * Look up the linear D3D texture format code for a bitmap format index.
 * Table at 0x2b9618 maps format indices 0..17 to D3D format codes.
 * If flags bit 0x20 is set and format is 10 or 11 (DXT4/DXT5), returns 0x33. */
int bitmap_format_to_d3d_linear_format(int16_t format, uint16_t flags)
{
  int table_base;

  if (format < 0 || format >= 0x12) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1e1, 1);
    system_exit(-1);
  }

  if (((int *)0x2b9618)[format] == -1) {
    display_assert("table[format]!=NONE",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1e2, 1);
    system_exit(-1);
  }

  table_base = 0x2b9618;
  if ((flags & 0x20) && (format == 10 || format == 11))
    return 0x33;

  return ((int *)table_base)[format];
}

/* FUN_001bec30 (0x1bec30)
 *
 * Look up the swizzled D3D texture format code for a bitmap format index.
 * Table at 0x2b9660 maps format indices 0..17 to D3D format codes.
 * If flags bit 0x20 is set and format is 10 or 11 (DXT4/DXT5), returns 0x36. */
int FUN_001bec30(int16_t format, uint16_t flags)
{
  int table_base;

  if (format < 0 || format >= 0x12) {
    display_assert("format>=0 && format<NUMBER_OF_BITMAP_FORMATS",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x206, 1);
    system_exit(-1);
  }

  if (((int *)0x2b9660)[format] == -1) {
    display_assert("table[format]!=NONE",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x207, 1);
    system_exit(-1);
  }

  table_base = 0x2b9660;
  if ((flags & 0x20) && (format == 10 || format == 11))
    return 0x36;

  return ((int *)table_base)[format];
}

/* FUN_001becc0 (0x1becc0)
 *
 * Sort predicate over two bitmap hardware-format pointers: true when the
 * first bitmap's total swizzled data size exceeds the second's. Referenced
 * only as DATA from 0x1bf45e inside texture_cache_debug_render (0x1bf260),
 * i.e. it is handed to a sort as a comparison callback; the sort's own
 * ordering semantics are not observed here.
 *
 * The original subtracts and then tests the difference (sub esi,eax /
 * test esi,esi / setg al) rather than comparing directly, so the
 * subtraction form is kept verbatim. Both cdecl cleanups are merged by the
 * original into a single `add esp,0x8` after the second call; each call
 * itself pushes exactly one argument (the call-site audit's ARG_COUNT
 * warning on the second call is that merged cleanup, not a second arg). */
bool FUN_001becc0(void *bitmap_a, void *bitmap_b)
{
  int size_a;
  int size_b;

  size_a = FUN_00183290(bitmap_a);
  size_b = FUN_00183290(bitmap_b);

  return size_a - size_b > 0;
}

/* texture_cache_flush (0x1bed30)
 *
 * Drain the GPU of any work that could still reference cached texture pages,
 * then dispose every entry in the texture LRU-vector cache.
 *
 * 001bed30  CALL D3DDevice_KickPushBuffer  ; submit pending pushbuffer work
 * 001bed35  CALL D3DDevice_IsBusy          ; return value discarded
 * 001bed3a  MOV EAX,[0x004ea980]           ; the lruv cache pointer
 * 001bed3f  PUSH EAX
 * 001bed40  CALL lruv_cache_dispose_all
 * 001bed45  POP ECX                        ; cdecl cleanup of the one arg
 *
 * The D3DDevice_IsBusy result is genuinely unused: there is no test or branch
 * on EAX after the call, and no loop. kb.json declares it `void`, so the call
 * is transcribed as a bare statement rather than assigned to a dead local. */
void texture_cache_flush(void)
{
  D3DDevice_KickPushBuffer();
  D3DDevice_IsBusy();
  lruv_cache_dispose_all(*(void **)0x4ea980);
}

/* FUN_001bed50 (0x1bed50)
 *
 * Predicate over one texture-cache block: takes a datum handle into the
 * bitmap-entry data array (0x4ea978) and returns 0 only when that entry's
 * byte at +4 is set and its D3D resource at +0xc reports not-busy;
 * otherwise 1. The entry byte at +4 is the load-completion flag written by
 * cache_file_read in xbox_texture_cache_request, and +0xc is the D3D
 * texture resource built by xbox_texture_cache_setup_d3d_texture.
 *
 * Referenced only as DATA from 0x1bf0ba inside FUN_001bf080
 * (texture_cache_new), i.e. it is installed as a callback; the consumer's
 * exact contract is not observed here, so the function keeps its FUN_ name.
 *
 * 001bed53  MOV EAX,[EBP+0x8]        ; datum handle (only stack arg)
 * 001bed56  MOV ECX,[0x004ea978]     ; bitmap-entry data array
 * 001bed5c  PUSH EAX                 ; -> arg 2 (handle)
 * 001bed5d  PUSH ECX                 ; -> arg 1 (data array)
 * 001bed5e  CALL datum_get
 * 001bed63  MOV CL,byte ptr [EAX+0x4]
 * 001bed66  ADD ESP,0x8              ; cdecl cleanup of datum_get's 2 args
 * 001bed69  TEST CL,CL
 * 001bed6b  JZ 0x001bed7c            ; flag clear -> return 1
 * 001bed6d  ADD EAX,0xc
 * 001bed70  PUSH EAX
 * 001bed71  CALL D3DResource_IsBusy  ; __stdcall: callee pops the arg
 * 001bed76  TEST EAX,EAX
 * 001bed78  JNZ 0x001bed7c           ; busy -> return 1
 * 001bed7a  POP EBP / RET            ; falls out with EAX == 0
 * 001bed7c  MOV EAX,0x1 / POP EBP / RET
 *
 * The zero return reuses the IsBusy result already known to be 0 on that
 * edge (no XOR EAX,EAX), and the true return is a full-dword MOV EAX,1, so
 * the return is transcribed as int rather than bool. */
int FUN_001bed50(int cache_block_index)
{
  char *cache_entry = datum_get(*(void **)0x4ea978, cache_block_index);

  if (cache_entry[4] != 0) {
    if (D3DResource_IsBusy(cache_entry + 0xc) == 0) {
      return 0;
    }
  }

  return 1;
}

/* FUN_001bed90 (0x1bed90)
 *
 * Release one texture-cache block: spin until the block's pending load has
 * completed and its D3D resource is no longer referenced by the GPU, then
 * clear the owning bitmap's back-pointer into the cache and free the datum.
 *
 * The assert text at 0x2b9788 is
 * "texture->bitmap->cache_block_index==block_index", which names the
 * datum_get result `texture`, its +8 field `bitmap`, and the parameter
 * `block_index`. It also proves bitmap+0x24 is `cache_block_index`;
 * bitmap+0x2c is written 0 on the same edge, meaning unproven.
 *
 * 001bed93  MOV EAX,[0x004ea978]     ; bitmap-entry data array
 * 001bed99  MOV ESI,[EBP+0x8]        ; single cdecl arg: block_index
 * 001bed9d  PUSH ESI / PUSH EAX
 * 001bed9f  CALL datum_get           ; -> EDI = texture (kept across the loop)
 * 001beda4  ADD ESP,0x8
 * 001bedb0  MOV ECX,[0x004ea978]     ; loop head: re-fetch every iteration
 * 001bedb6  PUSH ESI / PUSH ECX
 * 001bedb8  CALL datum_get
 * 001bedbd  MOV CL,byte ptr [EAX+0x4]
 * 001bedc0  ADD ESP,0x8
 * 001bedc3  TEST CL,CL
 * 001bedc5  JZ 0x001bedb0            ; load flag clear -> keep waiting
 * 001bedc7  ADD EAX,0xc
 * 001bedca  PUSH EAX
 * 001bedcb  CALL D3DResource_IsBusy  ; __stdcall: callee pops the arg
 * 001bedd2  JNZ 0x001bedb0           ; still busy -> keep waiting
 * 001bedd4  MOV EDX,[EDI+0x8]        ; texture->bitmap
 * 001bedd7  CMP [EDX+0x24],ESI
 * 001bedfc  MOV EAX,[EDI+0x8]        ; reloaded: the store below may alias
 * 001bedff  MOV [EAX+0x24],0xffffffff
 * 001bee06  MOV ECX,[EDI+0x8]        ; reloaded again
 * 001bee09  MOV [ECX+0x2c],0x0
 * 001bee10  MOV EDX,[0x004ea978]
 * 001bee16  PUSH ESI / PUSH EDX
 * 001bee18  CALL datum_delete
 *
 * The wait loop is the body of FUN_001bed50 (the "block not ready" predicate)
 * inlined: same +4 flag test, same +0xc resource, same JZ/JNZ back-edges to a
 * single loop head, so it is transcribed inline rather than as a call. The
 * pre-loop datum_get is a separate call whose result survives in EDI; it is
 * not folded into the loop. */
void FUN_001bed90(int block_index)
{
  char *texture;
  char *cache_entry;

  texture = datum_get(*(void **)0x4ea978, block_index);

  do {
    cache_entry = datum_get(*(void **)0x4ea978, block_index);
  } while (cache_entry[4] == 0 || D3DResource_IsBusy(cache_entry + 0xc) != 0);

  if (*(int *)(*(char **)(texture + 8) + 0x24) != block_index) {
    display_assert("texture->bitmap->cache_block_index==block_index",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x187, 1);
    system_exit(-1);
  }

  *(int *)(*(char **)(texture + 8) + 0x24) = -1;
  *(int *)(*(char **)(texture + 8) + 0x2c) = 0;
  datum_delete(*(data_t **)0x4ea978, block_index);
}

/* xbox_texture_cache_setup_d3d_texture (0x1bee30)
 *
 * Populate a D3D texture resource header from a bitmap hardware format.
 * Two paths: if bitmap flags bit 0x10 is set, builds a swizzled texture
 * descriptor with pitch-based size encoding; otherwise builds a linear
 * texture descriptor using log2 dimensions and mipmap level count.
 * Finishes by registering the resource with D3DResource_Register.
 *
 * bitmap  is passed in ESI (hardware_format pointer).
 * texture is passed in EDI (D3D texture header, 5 dwords / 20 bytes).
 *
 * The second read of bitmap+0xa is deliberately a fresh memory load rather
 * than a reuse of t10: the original re-reads it (cmp word ptr [esi+0xa],2), and
 * ending t10's live range at the first test is what frees the register.
 *
 * VC71 match ceiling ~80%, two causes, neither source-controllable:
 *   1. Both params are @<reg>, so the original has NO EBP frame at all
 *      (push ebx ... pop ebx; ret). The VC71 verify lane cannot pass args in
 *      esi/edi, so our build emits a full frame plus [ebp+N] param reads, and
 *      the extra pressure forces `texture` to be reloaded from [ebp+0xc] in
 *      the else branch where the original just keeps it in EDI.
 *   2. VC71 distributes shifts over the pack chains: ((p-1)<<12 | (h-1))<<12
 *      becomes (p<<24 - 0x1000000) | (h<<12 - 0x1000) (the IMM-WARN), and the
 *      else branch's two `desc <<= 4` steps merge into one `shl 8`. Both are
 *      valid unconditionally and cost the same instruction count, so it is a
 *      canonicalization coin flip driven by scheduling, not by source form:
 *      the sequential chain, the flat absolute-shift form (<<24/<<12/<<0), and
 *      pre-decremented h/w locals all compile to identical distributed code. */
void xbox_texture_cache_setup_d3d_texture(void *bitmap /* @<esi> */,
                                          void *texture /* @<edi> */)
{
  assert_halt(bitmap);
  assert_halt(texture);

  ((int *)texture)[1] = 0;
  ((int *)texture)[2] = 0;
  ((int *)texture)[0] = 0x40001;

  if (*(uint16_t *)((char *)bitmap + 0xe) & 0x10) {
    uint32_t format_bits;
    int height, width, pitch;

    format_bits = FUN_001bec30(*(int16_t *)((char *)bitmap + 0xc),
                               *(uint16_t *)((char *)bitmap + 0xe));
    ((int *)texture)[3] = (format_bits << 8) | 0x10029;

    pitch = bitmap_mipmap_get_row_pitch(bitmap, 0);
    height = *(int16_t *)((char *)bitmap + 0x6);
    width = *(int16_t *)((char *)bitmap + 0x4);

    pitch = (pitch / 64) - 1;
    pitch = (pitch << 12) | (height - 1);
    pitch = (pitch << 12) | (width - 1);
    ((int *)texture)[4] = pitch;
  } else {
    int16_t h, w, fmt, t10;
    uint16_t flg;
    uint32_t desc;

    desc = FUN_00108db0(*(int16_t *)((char *)bitmap + 0x8));
    h = *(int16_t *)((char *)bitmap + 0x6);
    desc <<= 4;
    desc |= FUN_00108db0(h);
    w = *(int16_t *)((char *)bitmap + 0x4);
    desc <<= 4;
    desc |= FUN_00108db0(w);
    flg = *(uint16_t *)((char *)bitmap + 0xe);
    fmt = *(int16_t *)((char *)bitmap + 0xc);
    desc <<= 12;
    desc |= bitmap_format_to_d3d_linear_format(fmt, flg);
    t10 = *(int16_t *)((char *)bitmap + 0xa);
    desc <<= 4;
    desc |= (3 - (t10 != 1));
    desc <<= 4;
    desc |= ((FUN_00183120(bitmap) + 1) << 16);

    ((int *)texture)[4] = 0;
    ((int *)texture)[3] =
      desc | (((*(int16_t *)((char *)bitmap + 0xa) != 2) - 1) & 4) | 9;
  }

  D3DResource_Register(texture, *(void **)((char *)bitmap + 0x2c));
}

/* texture_cache_new (0x1bf080)
 *
 * Bring up the Xbox texture cache: the datum table that tracks per-cache-block
 * texture records (0x580 entries of 0x20 bytes), the LRU-V cache itself
 * (0x580 pages of 2^0xe bytes, up to 0x580 blocks, with this TU's own
 * block-delete/block-query callbacks), then the base address of the backing
 * store in hardware texture memory. Every step is fatal on failure.
 *
 * 001bf080  PUSH 0x20 / PUSH 0x580 / PUSH "xbox texture" ; data_new args
 * 001bf0ba  PUSH 0x1bed50 / PUSH 0x1bed90 / PUSH 0x580 /
 *           PUSH 0xe / PUSH 0x580 / PUSH "xbox texture cache"
 *                                 ; lruv_new args, last push is arg1
 * 001bf103  CALL 0x001bdd60       ; base address, no args
 *
 * The repeated PAGE_COUNT (arg2 page_count, arg4 maximum_block_count) is not
 * decompiler register aliasing: 001bf0c4 and 001bf0cb are two independent
 * PUSH 0x580 immediates.
 *
 * The disassembly does TEST EAX,EAX before storing EAX to the global at each
 * step; that is MSVC scheduling of `globals.x = f(); if (!globals.x)`, not a
 * separate source temporary. */
void texture_cache_new(void)
{
  *(data_t **)0x4ea978 =
    data_new("xbox texture", HALO_TEXTURE_CACHE_PAGE_COUNT, 0x20);
  if (*(data_t **)0x4ea978 == NULL) {
    display_assert("xbox_texture_cache_globals.textures",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x62, true);
    system_exit(-1);
  }

  *(void **)0x4ea980 =
    lruv_new((int)"xbox texture cache", HALO_TEXTURE_CACHE_PAGE_COUNT,
             HALO_TEXTURE_CACHE_PAGE_BITS, HALO_TEXTURE_CACHE_PAGE_COUNT,
             FUN_001bed90, FUN_001bed50);
  if (*(void **)0x4ea980 == NULL) {
    display_assert("xbox_texture_cache_globals.cache",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x66, true);
    system_exit(-1);
  }

  *(void **)0x4ea97c = FUN_001bdd60();
  if (*(void **)0x4ea97c == NULL) {
    display_assert("xbox_texture_cache_globals.base_address",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x69, true);
    system_exit(-1);
  }
}

/* texture_cache_close (0x1bf130)
 *
 * Teardown counterpart to texture_cache_new. Asserts the stolen-memory flag is
 * clear, drains the GPU of pending work, releases every cached page, then marks
 * the bitmap-entry data array invalid.
 *
 * 001bf130  MOV AL,[0x004ea984]            ; stolen_memory flag (int8)
 * 001bf137  JZ 0x001bf159                  ; skip assert when clear
 * 001bf14a  CALL display_assert             ; line 0x85, halt=true
 * 001bf151  CALL system_exit(-1)
 * 001bf159  CALL D3DDevice_KickPushBuffer  ; submit pending pushbuffer work
 * 001bf15e  CALL D3DDevice_IsBusy          ; return value discarded
 * 001bf163  MOV EAX,[0x004ea980]           ; the lruv cache pointer
 * 001bf169  CALL lruv_cache_dispose_all
 * 001bf16e  MOV ECX,[0x004ea978]           ; bitmap-entry data array
 * 001bf175  CALL data_make_invalid
 * 001bf17a  ADD ESP,0x8                    ; one coalesced cdecl cleanup for
 *                                          ; the two 1-arg calls above
 *
 * The single ADD ESP,0x8 is MSVC merging the cleanup of both preceding cdecl
 * calls; each callee takes exactly one stack argument (the ARG_COUNT hazard
 * reported against data_make_invalid is that coalescing, not a second arg).
 * As in texture_cache_flush, the D3DDevice_IsBusy result is genuinely unused --
 * no TEST/branch follows it; the call is there to stall until the GPU drains.
 */
void texture_cache_close(void)
{
  if (*(int8_t *)0x4ea984 != 0) {
    display_assert("!xbox_texture_cache_globals.stolen_memory",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x85, true);
    system_exit(-1);
  }

  D3DDevice_KickPushBuffer();
  D3DDevice_IsBusy();
  lruv_cache_dispose_all(*(void **)0x4ea980);
  data_make_invalid(*(data_t **)0x4ea978);
}

bool xbox_texture_cache_request(void *hardware_format, bool block)
{
  int32_t min_block = *(int32_t *)((char *)hardware_format + 0x1c);
  int cache_block_index = FUN_00183290(hardware_format);

  if (cache_block_index <= min_block) {
    cache_block_index = min_block;
  }

  cache_block_index = FUN_0011de10(*(void **)0x4ea980, cache_block_index);
  if (cache_block_index != -1) {
    int cache_page_index =
      lruv_block_get_address(*(void **)0x4ea980, cache_block_index) +
      *(int32_t *)0x4ea97c;
    int new_texture_index =
      data_new_datum(*(void **)0x4ea978, cache_block_index);
    char *cache_entry = datum_get(*(void **)0x4ea978, cache_block_index);

    if (new_texture_index != cache_block_index) {
      display_assert("new_texture_index==cache_block_index",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x1af,
                     true);
      system_exit(-1);
    }

    *(int32_t *)((char *)hardware_format + 0x24) = cache_block_index;
    *(int32_t *)((char *)hardware_format + 0x2c) = cache_page_index;
    *(void **)(cache_entry + 8) = hardware_format;
    xbox_texture_cache_setup_d3d_texture(hardware_format, cache_entry + 0xc);
    *(int16_t *)(cache_entry + 2) =
      cache_file_read(*(int32_t *)((char *)hardware_format + 0x20),
                      *(int32_t *)((char *)hardware_format + 0x18), min_block,
                      cache_page_index, cache_entry + 4, block);
    return true;
  }

  return false;
}

void *xbox_texture_cache_get_hardware_format(void *hardware_format, bool block,
                                             bool load)
{
  void *result = NULL;

  if (!load && block) {
    display_assert("load || !block",
                   "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0xd2, true);
    system_exit(-1);
  }

  if (*(char *)((char *)hardware_format + 0xe) < 0) {
    if (*(int32_t *)((char *)hardware_format + 0x24) == -1 && load) {
      xbox_texture_cache_request(hardware_format, block);
    }
    if (*(int32_t *)((char *)hardware_format + 0x24) != -1) {
      void *entry = datum_get(*(void **)0x4ea978,
                              *(int32_t *)((char *)hardware_format + 0x24));
      lruv_debug_to_file(*(void **)0x4ea980,
                         *(int32_t *)((char *)hardware_format + 0x24));
      if (block) {
        if (*(char *)((char *)entry + 4) != 0)
          goto loaded;
        if (*(uint8_t *)0x4ea98a) {
          const char *name =
            tag_get_name(*(int32_t *)((char *)hardware_format + 0x20));
          console_warning("%s", name);
        }
        cache_files_io_request_enable(*(int16_t *)((char *)entry + 2));
      }
      do {
        if (*(char *)((char *)entry + 4) != 0) {
        loaded:
          if (*(char *)((char *)entry + 5) == 0) {
            *(char *)((char *)entry + 5) = 1;
          }
          result = (char *)entry + 0xc;
          if (result)
            break;
        } else {
          unsigned int t0 = sound_render_time();
          if (system_milliseconds() - t0 > 0x84u) {
            sound_idle();
          }
          SwitchToThread();
        }
      } while (block);
    }
  } else {
    result = *(void **)((char *)hardware_format + 0x28);
  }

  if (block && !result) {
    unsigned int now = system_milliseconds();
    if (now - *(unsigned int *)0x4ea98c > 10000u) {
      terminal_output(
        *(void **)0x2ee6f4,
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
        NULL);
      error(2, "YOU GOT STABBED!!!! double-click \"GETSTABBED.BAT\" on your PC "
               "now!!!");
      terminal_output(
        *(void **)0x2ee6f4,
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
        NULL);
      FUN_0011db90("d:\\stabbed.txt",
                   tag_get_name(*(int32_t *)((char *)hardware_format + 0x20)),
                   *(int32_t *)((char *)hardware_format + 0x1c),
                   *(void **)0x4ea980, (void *)0x18ef30, (void *)0x1beb70);
      *(unsigned int *)0x4ea98c = system_milliseconds();
    }
    result = rasterizer_get_default_hardware_format(hardware_format);
    if (!result) {
      display_assert("hardware_format",
                     "c:\\halo\\SOURCE\\cache\\xbox_texture_cache.c", 0x127,
                     true);
      system_exit(-1);
    }
  }

  return result;
}
