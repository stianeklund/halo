/*
 * rasterizer_preinitialize (0x1553f0)
 *
 * Creates the IDirect3D8 object and performs a probe CreateDevice call to
 * verify that D3D hardware is usable.  The device is created with a fixed
 * 640x480 back-buffer, D3DSWAPEFFECT_DISCARD, fullscreen, auto depth/stencil
 * D3DFMT_D24S8 (0x2a), and BehaviorFlags 0x40
 * (D3DCREATE_HARDWARE_VERTEXPROCESSING on Xbox).  If CreateDevice succeeds,
 * device caps are fetched, Present is called, then the device and IDirect3D8
 * object are released.  On any failure an error is logged.
 *
 * Globals (not in kb.json, hardcoded):
 *   0x476a50  void *  – IDirect3D8 object pointer
 *   0x476ab0  void *  – IDirect3DDevice8 pointer
 *   0x5a59e0  –       – D3DCAPS8 output buffer
 *
 * Present_parameters layout confirmed against D3DPRESENT_PARAMETERS from
 * third_party/xbox/d3d8types.h (0x34 bytes total).
 */

/*
 * Minimal mirror of D3DPRESENT_PARAMETERS fields used here.
 * Layout matches d3d8types.h exactly (4-byte aligned, no pack).
 * Total: 0x34 bytes = 13 DWORD fields.
 */
#pragma pack(push, 4)
typedef struct {
  unsigned int BackBufferWidth; /* +0x00 */
  unsigned int BackBufferHeight; /* +0x04 */
  unsigned int BackBufferFormat; /* +0x08 (D3DFORMAT enum) */
  unsigned int BackBufferCount; /* +0x0c */
  unsigned int MultiSampleType; /* +0x10 (D3DMULTISAMPLE_TYPE) */
  unsigned int SwapEffect; /* +0x14 (D3DSWAPEFFECT enum) */
  void *hDeviceWindow; /* +0x18 (HWND) */
  unsigned int Windowed; /* +0x1c (BOOL) */
  unsigned int EnableAutoDepthStencil; /* +0x20 (BOOL) */
  unsigned int AutoDepthStencilFormat; /* +0x24 (D3DFORMAT enum) */
  unsigned int Flags; /* +0x28 (DWORD) */
  unsigned int FullScreen_RefreshRateInHz; /* +0x2c (UINT) */
  unsigned int FullScreen_PresentationInterval; /* +0x30 (UINT) */
} d3d_present_parameters_t;
#pragma pack(pop)

/* 0x1553f0 */
void rasterizer_preinitialize(void)
{
  int hr;
  d3d_present_parameters_t d3dpp;

  /* 0x1eeab0: stdcall Direct3DCreate(UINT sdkVersion) -> void * (IDirect3D8)
   */
  *(void **)0x476a50 = ((void *(__stdcall *)(unsigned int))0x1eeab0)(0);

  if (*(void **)0x476a50 == 0) {
    error(2, "### ERROR failed to create D3D object");
    error(2, "### ERROR rasterizer_preinitialize failed");
    return;
  }

  /* Zero-fill D3DPRESENT_PARAMETERS (0x34 bytes) then set used fields */
  csmemset(&d3dpp, 0, 0x34);
  d3dpp.BackBufferWidth = 0x280; /* 640 */
  d3dpp.BackBufferHeight = 0x1e0; /* 480 */
  d3dpp.BackBufferFormat = 0x06; /* D3DFMT_A8R8G8B8 */
  d3dpp.SwapEffect = 1; /* D3DSWAPEFFECT_DISCARD */
  d3dpp.Windowed = 0; /* fullscreen */
  d3dpp.EnableAutoDepthStencil = 1;
  d3dpp.AutoDepthStencilFormat = 0x2a; /* D3DFMT_D24S8 */
  d3dpp.Flags = 1;
  d3dpp.FullScreen_PresentationInterval = 0;

  /*
   * 0x1edec0: stdcall IDirect3D8_CreateDevice(
   *   Adapter, DeviceType, hFocusWindow, BehaviorFlags,
   *   pPresentationParameters, ppReturnedDeviceInterface)
   * RET 0x18 confirms 6 args (stdcall).
   */
  hr = ((int(__stdcall *)(unsigned int, unsigned int, void *, unsigned int,
                          d3d_present_parameters_t *, void **))0x1edec0)(
    0, /* D3DADAPTER_DEFAULT */
    1, /* D3DDEVTYPE_HAL */
    0, /* hFocusWindow = NULL (fullscreen) */
    0x40, /* D3DCREATE_HARDWARE_VERTEXPROCESSING */
    &d3dpp, (void **)0x476ab0);

  if (hr < 0) {
    /*
     * 0x167ff0: reports a D3D HRESULT failure with context string and
     * logs via error(). Signature: (HRESULT, const char *context).
     */
    ((void (*)(int, const char *))0x167ff0)(
      hr,
      "IDirect3D8_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,"
      " RASTERIZER_DEVICE_CREATION_FLAGS, &d3d_present_parameters,"
      " &global_d3d_device)");
  }

  if (*(void **)0x476ab0 == 0 || hr < 0) {
    *(void **)0x476ab0 = 0;
    error(2, "### ERROR failed to create D3D device");
    error(2, "### ERROR rasterizer_preinitialize failed");
    return;
  }

  /* 0x1e69f0: D3DDevice_GetDeviceCaps(&d3d_caps) — 1 arg (stdcall, RET 0x4) */
  ((void(__stdcall *)(void *))0x1e69f0)((void *)0x5a59e0);

  /* 0x1ee920: D3DDevice_Present(NULL, NULL, NULL, NULL) — 4 args (stdcall, RET
   * 0x10) */
  ((void(__stdcall *)(void *, void *, void *, void *))0x1ee920)(0, 0, 0, 0);

  /* Release device if acquired */
  if (*(void **)0x476ab0 != 0) {
    /* 0x1e6f50: D3DDevice_Release() — no args */
    ((void (*)(void))0x1e6f50)();
    *(void **)0x476ab0 = 0;
  }

  /* Release IDirect3D8 object */
  if (*(void **)0x476a50 != 0) {
    *(void **)0x476a50 = 0;
  }
}

/*
 * rasterizer_get_default_hardware_format (0x155580)
 *
 * Returns the default D3D texture pointer for a bitmap based on its type.
 * Bitmap types 0 (2D) and 1 (volume) map to the 2D default at 0x3256a4.
 * Bitmap type 2 (cubemap) maps to the cubemap default at 0x3256ac.
 * These globals are populated by rasterizer_filthy_bitmap_default_initialize
 * (FUN_00156e00).
 *
 * Globals:
 *   0x3256a4  void *  – default 2D hardware texture format
 *   0x3256a8  void *  – default volume texture format
 *   0x3256ac  void *  – default cubemap texture format
 */
/* 0x155580 */
void *rasterizer_get_default_hardware_format(void *bitmap_data)
{
  short type;
  void *result;

  if (!bitmap_data) {
    display_assert("bitmap",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0xd1, true);
    system_exit(-1);
  }

  type = *(short *)((char *)bitmap_data + 0xa);
  if (type != 0 && type != 1) {
    if (type == 2) {
      result = *(void **)0x3256ac;
    } else {
      display_assert("### ERROR unsupported bitmap type",
                     "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0xdf, true);
      system_exit(-1);
      result = bitmap_data;
    }
  } else {
    result = *(void **)0x3256a4;
  }

  if (!result) {
    display_assert("hardware_format",
                   "c:\\halo\\SOURCE\\rasterizer\\xbox\\rasterizer_xbox.c", 0xe2, true);
    system_exit(-1);
  }

  return result;
}
