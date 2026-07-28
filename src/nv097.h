#ifndef NV097_H
#define NV097_H

/* NV2A pushbuffer command word for D3DDevice_SetRenderState_Simple(reg, v):
 * reg = (count 1 << 18) | NV097 method offset. Value bit layout for
 * SET_COLOR_MASK: one write-enable byte per channel (blue=bit0, green=bit8,
 * red=bit16, alpha=bit24). A dropped byte silently disables that channel's
 * writes — 0x101 instead of 0x10101 killed red-channel writes during the
 * active-camo capture pass (garbled-red camo regression, 2026-07-12).
 *
 * Shared by src/common.h (clang build) and src/xdk_common.h (the header
 * vc71_verify.py force-includes).  Kept in its own file so the two include
 * paths cannot drift: before this split only common.h defined these, so any
 * TU using them (e.g. rasterizer.c) failed to compile under VC71 with
 * "C2065: undeclared identifier" and was silently unverifiable. */
#define NV097_SET_COLOR_MASK_CMD 0x40358
#define NV097_COLOR_MASK_NONE    0x00000000
#define NV097_COLOR_MASK_RGB     0x00010101
#define NV097_COLOR_MASK_RGBA    0x01010101

#endif
