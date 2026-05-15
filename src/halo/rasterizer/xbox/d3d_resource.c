/* Xbox D3D resource management — reference counting, GPU registration,
 * and texture surface access wrappers for the XDK D3D8 implementation. */

/* Returns 1 if the resource (or its cube-texture face) has persistent/busy
 * GPU flags set (bits 19-22 of resource[0]). */
static int d3d_resource_is_persistent(uint32_t *r)
{
    uint32_t *face;
    if (r[0] & 0x780000u)
        return 1;
    if ((r[0] & 0x70000u) == 0x50000u) {
        face = (uint32_t *)r[5];
        if (face != NULL && (face[0] & 0x780000u))
            return 1;
    }
    return 0;
}

/* Blocks the CPU until the GPU has finished using the resource.
 * For cube textures with a face sub-resource, handles face fences
 * separately. Clears the fence timestamp after blocking. */
void __stdcall D3DResource_BlockUntilNotBusy(void *resource)
{
    uint32_t *r = (uint32_t *)resource;
    uint32_t *face;
    uint32_t fence;
    uint8_t *dev;

    dev = (uint8_t *)D3D_g_pDevice;
    if (dev == NULL)
        return;

    if ((r[0] & 0x70000u) == 0x50000u) {
        face = (uint32_t *)r[5];
        if (face != NULL) {
            if (d3d_resource_is_persistent(r)) {
                D3D_BlockOnTime(*(uint32_t *)(dev + 0x1cu), 0);
                return;
            }
            r = face;
        }
    }

    fence = r[2];
    if (d3d_resource_is_persistent(r)) {
        D3D_BlockOnTime(*(uint32_t *)((uint8_t *)D3D_g_pDevice + 0x1cu), 0);
        r[2] = 0;
        return;
    }
    D3D_BlockOnTime(fence, 0);
    r[2] = 0;
}

/* Returns 1 if the GPU is still using the resource (fence not yet passed),
 * 0 if the resource is free. Clears the fence timestamp when done. */
int __stdcall D3DResource_IsBusy(void *resource)
{
    uint32_t *r = (uint32_t *)resource;
    uint32_t *face;
    uint32_t fence;
    uint32_t current_time;
    uint32_t counter;
    uint8_t *dev;

    dev = (uint8_t *)D3D_g_pDevice;

    if ((r[0] & 0x70000u) == 0x50000u) {
        face = (uint32_t *)r[5];
        if (face != NULL) {
            if (r[0] & 0x780000u)
                return 1;
            if (face[0] & 0x780000u)
                return 1;
            r = face;
        }
    }

    if (r[0] & 0x780000u)
        return 1;
    if ((r[0] & 0x70000u) == 0x50000u) {
        face = (uint32_t *)r[5];
        if (face != NULL && (face[0] & 0x780000u))
            return 1;
    }

    fence = r[2];
    if (fence == 0u) {
        r[2] = 0;
        return 0;
    }

    current_time = *(uint32_t *)(dev + 0x1cu);
    counter = **(uint32_t **)(dev + 0x3f0u);
    if ((current_time - fence) >= (current_time - counter)) {
        r[2] = 0;
        return 0;
    }
    return 1;
}

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
