/*
 * IDirect3DDevice8_CreateTexture @ 0x168230 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateTexture: format/pool/ppTexture arrive in
 * EDX/ECX/EAX, the device argument (s1) is ignored, width/height/levels/
 * usage (s2-s5) are on the stack. EAX passes through from the callee (no
 * explicit return). No direct call sites; RET 0x14. Duplicate template
 * instantiation of IDirect3DDevice8_CreateTexture_0 (rasterizer_xbox.c) in this object.
 */
/* 0x168230 */
void IDirect3DDevice8_CreateTexture(int r1, int r2, int r3, int s1, int s2, int s3, int s4,
                  int s5)
{
  (void)s1;
  D3DDevice_CreateTexture(s2, s3, s4, s5, r3, r2, (void *)r1);
}

/*
 * IDirect3DDevice8_CreateVolumeTexture @ 0x168250 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateVolumeTexture: format/pool/ppVolumeTexture in
 * EDX/ECX/EAX, device (s1) ignored, width/height/depth/levels/usage
 * (s2-s6) on the stack. EAX passes through from the callee (no explicit
 * return). No direct call sites; RET 0x18. Duplicate template instantiation
 * of IDirect3DDevice8_CreateVolumeTexture_0 (rasterizer_xbox.c) in this object.
 */
/* 0x168250 */
void IDirect3DDevice8_CreateVolumeTexture(int r1, int r2, int r3, int s1, int s2, int s3, int s4,
                  int s5, int s6)
{
  (void)s1;
  D3DDevice_CreateVolumeTexture(s2, s3, s4, s5, s6, r3, r2, (void *)r1);
}

/*
 * IDirect3DDevice8_CreateCubeTexture @ 0x168280 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateCubeTexture: format/pool/ppCubeTexture arrive in
 * EDX/ECX/EAX, device (s1) ignored, edge_length/levels/usage (s2-s4) on
 * the stack. EAX passes through from the callee. No direct call sites;
 * RET 0x10. Duplicate template instantiation of IDirect3DDevice8_CreateCubeTexture_0
 * (rasterizer_xbox.c) in this object.
 */
/* 0x168280 */
void IDirect3DDevice8_CreateCubeTexture(int r1, int r2, int r3, int s1, int s2, int s3, int s4)
{
  (void)s1;
  D3DDevice_CreateCubeTexture(s2, s3, s4, r3, r2, (void *)r1);
}

/*
 * IDirect3DTexture8_LockRect_0 @ 0x1682c0 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DTexture8::LockRect: flags/pRect/pLockedRect arrive in
 * EAX/ECX/EDX, texture (s1) and level (s2) on the stack (the texture IS
 * forwarded — no ignored device argument here). Returns S_OK. No direct
 * call sites; RET 0x8. Duplicate template instantiation of IDirect3DTexture8_LockRect_1
 * (rasterizer_xbox.c) in this object.
 */
/* 0x1682c0 */
int IDirect3DTexture8_LockRect_0(int r1, int r2, int r3, int s1, int s2)
{
  D3DTexture_LockRect((void *)s1, s2, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * IDirect3DVolumeTexture8_LockBox @ 0x168300 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DVolumeTexture8::LockBox: flags/pBox/pLockedBox arrive in
 * EAX/ECX/EDX, volume texture (s1) and level (s2) on the stack. Returns
 * S_OK. No direct call sites; RET 0x8. Duplicate template instantiation of
 * IDirect3DVolumeTexture8_LockBox_0 (rasterizer_xbox.c) in this object.
 */
/* 0x168300 */
int IDirect3DVolumeTexture8_LockBox(int r1, int r2, int r3, int s1, int s2)
{
  D3DVolumeTexture_LockBox((void *)s1, s2, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * IDirect3DCubeTexture8_LockRect @ 0x168340 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DCubeTexture8::LockRect: cube texture, face, and level are stack
 * arguments; pLockedRect, pRect, and flags arrive in EDX, ECX, and EAX.
 * Returns S_OK. No direct call sites; RET 0xc. Duplicate template
 * instantiation of IDirect3DCubeTexture8_LockRect_0 (rasterizer_xbox.c) in this object.
 */
/* 0x168340 */
int IDirect3DCubeTexture8_LockRect(int r1, int r2, int r3, int s1, int s2, int s3)
{
  D3DCubeTexture_LockRect((void *)s1, s2, s3, (void *)r3, (void *)r2, r1);
  return 0;
}

/* 0x168370 */
char rasterizer_bitmap_new(void *bitmap)
{
  char *bm;
  char success;
  int hr;
  int format;
  int mipmap_count;

  bm = (char *)bitmap;
  success = 1;
  if (bitmap == NULL) {
    display_assert(
      "bitmap",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_hardware_bitmaps.c",
      0x33, true);
    system_exit(-1);
  }
  if ((*(uint8_t *)(bm + 0xe) & 1) == 0) {
    display_assert(
      "TEST_FLAG(bitmap->flags, _bitmap_has_power_of_two_dimensions_bit)",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_hardware_bitmaps.c",
      0x34, true);
    system_exit(-1);
  }
  mipmap_count = rasterizer_xbox_bitmap_get_max_mipmap_count(bitmap);
  *(int16_t *)(bm + 0x14) = (int16_t)mipmap_count;
  if (*(void **)0x476ab0 != NULL) {
    format = *(int16_t *)(bm + 0xc);
    switch (*(int16_t *)(bm + 0xa)) {
    case 0:
      hr = D3DDevice_CreateTexture(
        *(int16_t *)(bm + 4), *(int16_t *)(bm + 6), mipmap_count + 1, 0,
        *(int *)((char *)0x2a2428 + format * 4), 1, (void *)(bm + 0x28));
      if (hr < 0) {
        success = 0;
        rasterizer_error(
          hr, "IDirect3DDevice8_CreateTexture(global_d3d_device, "
              "bitmap->width, bitmap->height, bitmap->mipmap_count+1, 0, "
              "rasterizer_bitmap_format_table[bitmap->format], "
              "D3DPOOL_MANAGED, &(IDirect3DTexture8*)bitmap->hardware_format)");
      }
      break;
    case 1:
      hr = D3DDevice_CreateVolumeTexture(
        *(int16_t *)(bm + 4), *(int16_t *)(bm + 6), *(int16_t *)(bm + 8),
        mipmap_count + 1, 0, *(int *)((char *)0x2a2428 + format * 4), 1,
        (void *)(bm + 0x28));
      if (hr < 0) {
        success = 0;
        rasterizer_error(
          hr,
          "IDirect3DDevice8_CreateVolumeTexture(global_d3d_device, "
          "bitmap->width, bitmap->height, bitmap->depth, "
          "bitmap->mipmap_count+1, 0, "
          "rasterizer_bitmap_format_table[bitmap->format], D3DPOOL_MANAGED, "
          "&(IDirect3DVolumeTexture8*)bitmap->hardware_format)");
      }
      break;
    case 2:
      hr = D3DDevice_CreateCubeTexture(
        *(int16_t *)(bm + 4), mipmap_count + 1, 0,
        *(int *)((char *)0x2a2428 + format * 4), 1, (void *)(bm + 0x28));
      if (hr < 0) {
        success = 0;
        rasterizer_error(
          hr,
          "IDirect3DDevice8_CreateCubeTexture(global_d3d_device, "
          "bitmap->width, bitmap->mipmap_count+1, 0, "
          "rasterizer_bitmap_format_table[bitmap->format], D3DPOOL_MANAGED, "
          "&(IDirect3DCubeTexture8*)bitmap->hardware_format)");
      }
      break;
    default:
      display_assert("### ERROR unsupported bitmap type",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_"
                     "hardware_bitmaps.c",
                     0x5b, true);
      system_exit(-1);
      break;
    }
    if (*(void **)(bm + 0x28) == NULL || success == 0) {
      *(void **)(bm + 0x28) = NULL;
      error(2, "### ERROR failed to create bitmap hardware format");
      return 0;
    }
  } else {
    *(void **)(bm + 0x28) = NULL;
  }
  return success;
}

/*
 * rasterizer_bitmap_2d_changed @ 0x168500 — re-upload every mip level of a
 * 2D bitmap into its D3D texture (bitmap->hardware_format at +0x28). The
 * bitmap arrives in ESI (TEST ESI,ESI at entry, no prior write). Mip levels
 * 0..[+0x14] are locked with D3DLOCK_NOOVERWRITE (0x20); bitmaps with
 * flag bit 1 at +0xe set are copied raw with csmemcpy, the others are
 * swizzled by bytes-per-pixel (bitmap_format_get_bits_per_pixel(+0xc) / 8).
 *
 * `success` mirrors the D3D result-check macro: BL starts at 1 (MOV BL,0x1)
 * and the post-LockRect TEST BL,BL branch to rasterizer_error is
 * unreachable at runtime (Ghidra drops it); kept to preserve the binary's
 * blocks. locked_rect is D3DLOCKED_RECT as int[2] (Pitch, pBits @ -0x10).
 * +0x2c is tested non-zero but its meaning is not proven here.
 */
/* 0x168500 */
void rasterizer_bitmap_2d_changed(void *bitmap /* @<esi> */)
{
  char *bm;
  char success;
  short mipmap_index;
  int locked_rect[2];
  void *dst;
  void *src;
  short width;
  short height;

  bm = (char *)bitmap;
  success = 1;
  if (bitmap == NULL) {
    display_assert(
      "bitmap",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_hardware_bitmaps.c",
      0x8d, true);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 != NULL && *(int *)(bm + 0x2c) != 0 &&
      *(void **)(bm + 0x28) != NULL) {
    for (mipmap_index = 0; success && mipmap_index <= *(short *)(bm + 0x14);
         mipmap_index++) {
      D3DTexture_LockRect(*(void **)(bm + 0x28), (int)mipmap_index, locked_rect,
                          NULL, 0x20);
      if (!success) {
        rasterizer_error(0, "IDirect3DTexture8_LockRect((IDirect3DTexture8*)"
                            "bitmap->hardware_format, mipmap_index, "
                            "&d3d_locked_rect, NULL, D3DLOCK_NOOVERWRITE)");
      }
      if (success && (void *)locked_rect[1] != NULL) {
        src = bitmap_mipmap_address(bitmap, mipmap_index);
        dst = (void *)locked_rect[1];
        width = bitmap_mipmap_get_width(bitmap, mipmap_index);
        height = bitmap_mipmap_get_height(bitmap, mipmap_index);
        if ((*(unsigned char *)(bm + 0xe) & 2) != 0) {
          csmemcpy(dst, src,
                   bitmap_mipmap_get_pixel_data_size(bitmap, mipmap_index));
        } else {
          switch (bitmap_format_get_bits_per_pixel(*(unsigned short *)(bm + 0xc)) /
                  8) {
          case 1:
            rasterizer_xbox_bitmap_swizzle2d_byte(dst, src, width, height);
            break;
          case 2:
            rasterizer_xbox_bitmap_swizzle2d_word(dst, src, width, height);
            break;
          case 4:
            rasterizer_xbox_bitmap_swizzle2d_long(dst, src, width, height);
            break;
          default:
            display_assert("### ERROR uncompressed bitmap format does not "
                           "have 1,2 or 4 bytes per pixel",
                           "c:\\halo\\SOURCE\\rasterizer\\xbox\\"
                           "rasterizer_xbox_hardware_bitmaps.c",
                           0xb1, true);
            system_exit(-1);
            break;
          }
        }
        success = 1;
      } else {
        error(2, "### ERROR failed to lock surface");
        success = 0;
      }
    }
    if (!success) {
      error(2, "### ERROR failed to change bitmap hardware format");
    }
  }
}

/*
 * rasterizer_bitmap_cm_changed @ 0x1688d0 — re-upload every mip level of
 * all six faces of a cube-map bitmap into its D3D cube texture
 * (bitmap->hardware_format at +0x28). The bitmap arrives in ESI (TEST
 * ESI,ESI at 0x1688d6, no prior write); the caller rasterizer_bitmap_changed holds it
 * in ESI. Faces 0..5 map through the int16 table at 0x2a2470 before
 * D3DCubeTexture_LockRect(..., D3DLOCK_NOOVERWRITE=0x20). Flag bit 1 at
 * +0xe copies raw with csmemcpy(pixel_data_size / 6); otherwise swizzled
 * by bytes-per-pixel. As in rasterizer_bitmap_2d_changed, the post-LockRect
 * TEST BL,BL branch to rasterizer_error is unreachable at runtime but kept
 * to preserve the binary's blocks. locked_rect is D3DLOCKED_RECT as int[2]
 * (Pitch, pBits @ -0x14). +0x2c is tested non-zero; meaning unproven.
 */
/* 0x1688d0 */
void rasterizer_bitmap_cm_changed(void *bitmap /* @<esi> */)
{
  char *bm;
  char success;
  short mipmap_index;
  short face_index;
  int locked_rect[2];
  void *dst;
  void *src;
  short width;
  short height;

  bm = (char *)bitmap;
  success = 1;
  if (bitmap == NULL) {
    display_assert(
      "bitmap",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_hardware_bitmaps.c",
      0x114, true);
    system_exit(-1);
  }
  if (*(void **)0x476ab0 != NULL && *(int *)(bm + 0x2c) != 0 &&
      *(void **)(bm + 0x28) != NULL) {
    for (mipmap_index = 0; success && mipmap_index <= *(short *)(bm + 0x14);
         mipmap_index++) {
      for (face_index = 0; success && face_index < 6; face_index++) {
        D3DCubeTexture_LockRect(*(void **)(bm + 0x28),
                                (int)((short *)0x2a2470)[face_index],
                                (int)mipmap_index, locked_rect, NULL, 0x20);
        if (!success) {
          rasterizer_error(0,
                           "IDirect3DCubeTexture8_LockRect("
                           "(IDirect3DCubeTexture8*)bitmap->hardware_format, "
                           "face_mapping_table[face_index], mipmap_index, "
                           "&d3d_locked_rect, NULL, D3DLOCK_NOOVERWRITE)");
        }
        if (success && (void *)locked_rect[1] != NULL) {
          src = bitmap_cube_map_address(bitmap, 0, 0, face_index, mipmap_index);
          dst = (void *)locked_rect[1];
          width = bitmap_mipmap_get_width(bitmap, mipmap_index);
          height = bitmap_mipmap_get_height(bitmap, mipmap_index);
          if ((*(unsigned char *)(bm + 0xe) & 2) != 0) {
            csmemcpy(dst, src,
                     bitmap_mipmap_get_pixel_data_size(bitmap, mipmap_index) /
                       6);
          } else {
            switch (
              bitmap_format_get_bits_per_pixel(*(unsigned short *)(bm + 0xc)) / 8) {
            case 1:
              rasterizer_xbox_bitmap_swizzle2d_byte(dst, src, width, height);
              break;
            case 2:
              rasterizer_xbox_bitmap_swizzle2d_word(dst, src, width, height);
              break;
            case 4:
              rasterizer_xbox_bitmap_swizzle2d_long(dst, src, width, height);
              break;
            default:
              display_assert("### ERROR uncompressed bitmap format does not "
                             "have 1,2 or 4 bytes per pixel",
                             "c:\\halo\\SOURCE\\rasterizer\\xbox\\"
                             "rasterizer_xbox_hardware_bitmaps.c",
                             0x13f, true);
              system_exit(-1);
              break;
            }
          }
          success = 1;
        } else {
          error(2, "### ERROR failed to lock surface");
          success = 0;
        }
      }
    }
    if (!success) {
      error(2, "### ERROR failed to change bitmap hardware format");
    }
  }
}

/*
 * rasterizer_bitmap_delete @ 0x168ae0 — release a bitmap_data's D3D hardware texture
 * resource: called from bitmap_delete (bitmaps.c) via a raw function-pointer
 * cast at 0x168ae0, and cross-referenced
 * unconditionally from editor_editing_sandbox @ 0x7c8fc.
 *
 * 00168ae4  MOV ESI,[EBP + 0x8]   ; single cdecl arg (bitmap_data *)
 * 00168ae7  PUSH ESI
 * 00168ae8  CALL 0x001be9f0       ; texture_cache_bitmap_delete(bitmap)
 * 00168af0  TEST ESI,ESI
 * 00168af2  JZ 0x00168b08         ; NULL bitmap -> skip D3D release
 * 00168af4  MOV EAX,[ESI + 0x28]  ; D3D resource pointer
 * 00168af7  TEST EAX,EAX
 * 00168af9  JZ 0x00168b08         ; no resource -> skip
 * 00168afb  PUSH EAX
 * 00168afc  CALL 0x001ed930       ; D3DResource_Release(resource)
 * 00168b01  MOV [ESI + 0x28],0x0  ; clear the resource pointer
 *
 * +0x28 D3D resource pointer, matching texture_cache_bitmap_new (which
 * zeroes it) and texture_cache_bitmap_delete's comment that +0x28 is left
 * untouched there.
 */
/* 0x168ae0 */
void rasterizer_bitmap_delete(void *bitmap)
{
  texture_cache_bitmap_delete(bitmap);

  if (bitmap != NULL && *(void **)((char *)bitmap + 0x28) != NULL) {
    D3DResource_Release(*(void **)((char *)bitmap + 0x28));
    *(void **)((char *)bitmap + 0x28) = NULL;
  }
}

/*
 * rasterizer_bitmap_changed @ 0x168b10 — bitmap_hardware_format_changed dispatcher: NULL
 * asserts the bitmap, sets the 0x325652 render-phase marker to 1, switches
 * on the bitmap type word at +0xa (0=2D, 1=3D, 2=cubemap, else assert), and
 * clears the marker back to 0 on the way out. Every case forwards the
 * bitmap pointer in a register. Case 1 (3D): PUSH EDI/MOV EDI,ESI/CALL
 * 0x1686c0/POP EDI; 0x1686c0 does `TEST EDI,EDI` at entry and reads
 * [EDI+0x28]/[EDI+0x2c]/[EDI+0x14], so its bitmap is @<edi>. Cases 0 and 2
 * call 0x168500 / 0x1688d0 with the bitmap still live in ESI (loaded at
 * 0x168b15); both callees open with `TEST ESI,ESI` (assert "bitmap") and
 * read [ESI+0x2c], so their bitmap is @<esi>. Calling them with no argument
 * leaves ESI as garbage; 0x168500 then hands it to bitmap_mipmap_address
 * (CALL 0x7d000 at 0x1685af), which halts on "unsupported bitmap type".
 */
/* 0x168b10 */
void rasterizer_bitmap_changed(void *bitmap)
{
  uint16_t *new_var;
  char *bm;
  int16_t type;

  new_var = (uint16_t *)0x325652;
  bm = (char *)bitmap;
  if (bitmap == NULL) {
    display_assert(
      "bitmap",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_hardware_bitmaps.c",
      0x70, true);
    system_exit(-1);
  }
  *new_var = 1;
  type = *(int16_t *)(bm + 0xa);
  switch (type) {
  case 0:
    rasterizer_bitmap_2d_changed(bitmap);
    break;
  case 1:
    rasterizer_bitmap_3d_changed(bitmap);
    break;
  case 2:
    rasterizer_bitmap_cm_changed(bitmap);
    break;
  default:
    display_assert(
      "### ERROR unsupported bitmap type",
      "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox_hardware_bitmaps.c",
      0x80, true);
    system_exit(-1);
    break;
  }
  *(uint16_t *)0x325652 = 0;
}

/*
 * IDirect3DDevice8_CreateVertexBuffer_0 @ 0x168bc0 — dead D3D8 inline-wrapper instantiation of
 * D3DDevice_CreateVertexBuffer: fvf/pool/ppVertexBuffer arrive in
 * EDX/ECX/EAX, the device argument (s1) is ignored, and length/usage
 * (s2/s3) are on the stack. The callee HRESULT passes through in EAX;
 * no direct call sites; RET 0xC. Duplicate template instantiation of
 * IDirect3DDevice8_CreateVertexBuffer_2 (rasterizer_xbox_decals.c).
 */
/* 0x168bc0 */
int IDirect3DDevice8_CreateVertexBuffer_0(int r1, int r2, int r3, int s1, int s2, int s3)
{
  (void)s1;
  return D3DDevice_CreateVertexBuffer(s2, s3, r3, r2, (void **)r1);
}

/* 0x168be0 */
int IDirect3DDevice8_CreateIndexBuffer(int r1, int r2, int r3, int s1, int s2, int s3)
{
  (void)s1;
  return D3DDevice_CreateIndexBuffer(s2, s3, r3, r2, (void **)r1);
}

/* 0x168c40 */
void IDirect3DVertexBuffer8_Lock_0(int r1, int r2, int r3, int s1, int s2)
{
  D3DVertexBuffer_Lock((void *)s1, (uint32_t)s2, (uint32_t)r3, (void **)r2,
                       (uint32_t)r1);
}

/* 0x168c70 */
void D3DIndexBuffer_Lock(int base, int *result, int offset, int unused_1, int unused_2)
{
  (void)unused_1;
  (void)unused_2;
  *result = *(int *)(base + 4) + offset;
}

/*
 * IDirect3DIndexBuffer8_Lock @ 0x168ca0 — dead D3D8 inline-wrapper instantiation, same
 * body as D3DIndexBuffer_Lock above plus an explicit success return:
 *   push ebp; mov ebp,esp; mov ecx,[eax+4]; add ecx,[ebp+8];
 *   mov [edx],ecx; xor eax,eax; pop ebp; ret 0xc
 * EAX is the resource base (its +4 dword is read), EDX is the out-pointer.
 * RET 0xc => three stack dwords at +8/+0xc/+0x10; only +8 is read, the
 * other two stay in the signature so the callee-cleans immediate is right.
 * XOR EAX,EAX before the epilogue is a real `return 0` (S_OK) — it is the
 * only instruction difference from 0x168c70 (17 bytes here vs 15 there),
 * so that sibling is void and this one is not.
 * xrefs_to empty: no call sites in the binary.
 */
/* 0x168ca0 */
int IDirect3DIndexBuffer8_Lock(int base, int *result, int offset, int unused_1, int unused_2)
{
  (void)unused_1;
  (void)unused_2;
  *result = *(int *)(base + 4) + offset;
  return 0;
}
