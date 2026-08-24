/*
 * FUN_00168230 @ 0x168230 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateTexture: format/pool/ppTexture arrive in
 * EDX/ECX/EAX, the device argument (s1) is ignored, width/height/levels/
 * usage (s2-s5) are on the stack. EAX passes through from the callee (no
 * explicit return). No direct call sites; RET 0x14. Duplicate template
 * instantiation of FUN_00155380 (rasterizer_xbox.c) in this object.
 */
/* 0x168230 */
void FUN_00168230(int r1, int r2, int r3, int s1, int s2, int s3, int s4,
                  int s5)
{
  (void)s1;
  D3DDevice_CreateTexture(s2, s3, s4, s5, r3, r2, (void *)r1);
}

/*
 * FUN_00168250 @ 0x168250 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DDevice8::CreateVolumeTexture: format/pool/ppVolumeTexture in
 * EDX/ECX/EAX, device (s1) ignored, width/height/depth/levels/usage
 * (s2-s6) on the stack. EAX passes through from the callee (no explicit
 * return). No direct call sites; RET 0x18. Duplicate template
 * instantiation of FUN_001553a0 (rasterizer_xbox.c) in this object.
 */
/* 0x168250 */
void FUN_00168250(int r1, int r2, int r3, int s1, int s2, int s3, int s4,
                  int s5, int s6)
{
  (void)s1;
  D3DDevice_CreateVolumeTexture(s2, s3, s4, s5, s6, r3, r2, (void *)r1);
}

/*
 * FUN_001682c0 @ 0x1682c0 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DTexture8::LockRect: flags/pRect/pLockedRect arrive in
 * EAX/ECX/EDX, texture (s1) and level (s2) on the stack (the texture IS
 * forwarded — no ignored device argument here). Returns S_OK. No direct
 * call sites; RET 0x8. Duplicate template instantiation of FUN_00155b60
 * (rasterizer_xbox.c) in this object.
 */
/* 0x1682c0 */
int FUN_001682c0(int r1, int r2, int r3, int s1, int s2)
{
  D3DTexture_LockRect((void *)s1, s2, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * FUN_00168300 @ 0x168300 — dead D3D8 inline-wrapper instantiation of
 * IDirect3DVolumeTexture8::LockBox: flags/pBox/pLockedBox arrive in
 * EAX/ECX/EDX, volume texture (s1) and level (s2) on the stack. Returns
 * S_OK. No direct call sites; RET 0x8. Duplicate template instantiation of
 * FUN_00155cc0 (rasterizer_xbox.c) in this object.
 */
/* 0x168300 */
int FUN_00168300(int r1, int r2, int r3, int s1, int s2)
{
  D3DVolumeTexture_LockBox((void *)s1, s2, (void *)r3, (void *)r2, r1);
  return 0;
}

/*
 * FUN_00168ae0 @ 0x168ae0 — release a bitmap_data's D3D hardware texture
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
void FUN_00168ae0(void *bitmap)
{
  texture_cache_bitmap_delete(bitmap);

  if (bitmap != NULL && *(void **)((char *)bitmap + 0x28) != NULL) {
    D3DResource_Release(*(void **)((char *)bitmap + 0x28));
    *(void **)((char *)bitmap + 0x28) = NULL;
  }
}
