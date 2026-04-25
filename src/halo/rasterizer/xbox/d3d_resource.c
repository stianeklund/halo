/* Xbox D3D resource management — reference counting, GPU registration,
 * and texture surface access wrappers for the XDK D3D8 implementation. */

/* Decrements the reference count on a D3D resource. When the count
 * reaches 1 (last reference), handles cube texture child cleanup via
 * recursive release, then destroys the resource if no persistent flags
 * (bits 19-22) are set. Returns the new reference count (lower 16 bits). */
uint32_t __stdcall D3DResource_Release(void *resource)
{
  uint32_t *r = (uint32_t *)resource;
  uint32_t common = r[0];

  if ((common & 0xFFFF) == 1) {
    if ((common & 0x70000) == 0x50000 && r[5] != 0)
      D3DResource_Release((void *)r[5]);
    common = r[0];
    if ((common & 0x780000) == 0) {
      D3D_DestroyResource(resource);
      return 0;
    }
  }
  r[0] = common - 1;
  return (common - 1) & 0xFFFF;
}

/* Registers a D3D resource by adding a base data address to the
 * resource's Data field. For non-vertex-buffer types, the address is
 * masked to 26 bits (Xbox GPU physical address space). */
void __stdcall D3DResource_Register(void *resource, void *data)
{
  uint32_t *r = (uint32_t *)resource;
  uint32_t addr = r[1] + (uint32_t)data;
  if ((r[0] & 0x70000) != 0x20000)
    addr &= 0x3FFFFFF;
  r[1] = addr;
}

/* Forwards to Get2DSurfaceDesc — retrieves the surface description
 * (format, dimensions, multisample type) for a given mip level. */
void __stdcall D3DTexture_GetLevelDesc(void *texture, unsigned int level,
                                       void *desc)
{
  Get2DSurfaceDesc(texture, level, desc);
}

/* Locks a 2D texture mip level for CPU access by forwarding to
 * Lock2DSurface with face=0 (non-cubemap). */
void __stdcall D3DTexture_LockRect(void *texture, unsigned int level,
                                   void *locked_rect, void *rect,
                                   unsigned int flags)
{
  Lock2DSurface(texture, 0, level, locked_rect, rect, flags);
}
