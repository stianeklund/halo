# Xbox Platform Headers

Vendored Xbox platform headers used for type definitions, constants, and
function declarations when implementing Halo functions that interact with
Xbox APIs directly.

**License notice:** Several files carry GPL v2 or LGPL v2.1 licenses
inherited from the Cxbx emulation project and Wine. These headers are used
solely as reference material for type and constant definitions and are not
linked into any binary produced by this project. See individual file headers
for full copyright notices.

## File inventory

| File | Source project | URL | License |
|------|---------------|-----|---------|
| `xboxkrnl.h` | xbox-includes | https://github.com/mborgerson/xbox-includes | CC0 1.0 |
| `xboxdef.h` | xbox-includes | https://github.com/mborgerson/xbox-includes | CC0 1.0 |
| `ntstatus.h` | nxdk | https://github.com/XboxDev/nxdk | CC0 1.0 |
| `d3d8.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `d3d8types.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `d3d8types_wine.h` | xbox-includes (Wine) | https://github.com/mborgerson/xbox-includes | LGPL v2.1 |
| `xapi.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `dsound.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `dsoundtypes.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `xacteng.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `xgraphic.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | GPL v2 |
| `xonline.h` | xbox-includes (Cxbx) | https://github.com/mborgerson/xbox-includes | see file |

## Notes

- `xboxkrnl.h` and `xboxdef.h` are from Stefan Schmidt's xbox-includes
  aggregation project (CC0), which consolidates headers from Cxbx, Wine, and
  OpenXDK. They provide the complete Xbox kernel export list (371+ functions)
  and NT base types (LIST_ENTRY, RTL_CRITICAL_SECTION, LARGE_INTEGER, etc.).

- `ntstatus.h` is taken from nxdk instead of xbox-includes because the nxdk
  version carries a cleaner CC0 license and is more actively maintained (last
  updated 2022). Note: xboxkrnl.h already defines the most common STATUS_*
  codes, so ntstatus.h should only be included standalone where extended
  status code coverage is needed — not after xbox.h.

- `d3d8.h` and `d3d8types.h` cover the full Xbox Direct3D 8 API: all
  D3DFORMAT values (swizzled, compressed, linear, depth), D3DPRIMITIVETYPE
  (including Xbox-only quads/polygons), D3DSurface, D3DTexture, D3DVertexBuffer,
  all D3DDevice_* function declarations, pixel/vertex shader definition structs,
  and D3DRENDERSTATETYPE (Xbox extended set).

- `xapi.h` covers XINPUT_GAMEPAD, XINPUT_STATE, XINPUT_RUMBLE,
  XINPUT_CAPABILITIES, controller subtype defines, XInitDevices, XInputOpen,
  XInputGetState/SetState, XLaunchNewImageA, CreateMutex/CloseHandle, and
  QueryPerformanceCounter.

- `dsound.h` / `dsoundtypes.h` cover CDirectSound, CDirectSoundBuffer,
  CDirectSoundStream, CMcpxStream, DSBUFFERDESC, DSSTREAMDESC, XMEDIAPACKET,
  DS3DBUFFER, and ~150 DirectSound patch declarations.

- `xacteng.h` covers the XACT audio engine: XACTEngineCreate/DoWork,
  IXACTEngine_Register*, IXACTSoundBank_Play/Stop, and 3D listener/source
  positioning.
