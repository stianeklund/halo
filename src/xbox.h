// SPDX-License-Identifier: CC0-1.0
//
// Xbox platform header wrapper.
//
// Provides Xbox kernel types (xboxkrnl.h, xboxdef.h) by default. Subsystem
// headers are opt-in — define the relevant macro before including this file:
//
//   #define XBOX_D3D8     - Direct3D 8 types and D3DDevice_* declarations
//   #define XBOX_XAPI     - XINPUT_GAMEPAD, XInputOpen/GetState/SetState, etc.
//   #define XBOX_DSOUND   - DirectSound types and function declarations
//   #define XBOX_XACTENG  - XACT audio engine
//   #define XBOX_XGRAPHIC - XGIsSwizzledFormat, XGSetTextureHeader, etc.
//   #define XBOX_XONLINE  - Xbox Live / networking stubs
//
// Example:
//   #define XBOX_D3D8
//   #include "xbox.h"
//
// Do not include from common.h. Include per .c file as needed.
//
// See third_party/xbox/SOURCES.md for origins and license notices.

#ifndef XBOX_H
#define XBOX_H

// The project compiles with #pragma pack(1) set globally in types.h.
// Xbox headers expect natural (MSVC) struct alignment. Save and restore.
#pragma pack(push)
#pragma pack()

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wgnu-empty-initializer"
#endif

#include "xboxkrnl.h"

#ifdef XBOX_D3D8
#include "d3d8.h"
#endif

#ifdef XBOX_XAPI
#include "xapi.h"
#endif

#ifdef XBOX_DSOUND
#include "dsound.h"
#endif

#ifdef XBOX_XACTENG
#include "xacteng.h"
#endif

#ifdef XBOX_XGRAPHIC
#include "xgraphic.h"
#endif

#ifdef XBOX_XONLINE
#include "xonline.h"
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#pragma pack(pop)

#endif // XBOX_H
