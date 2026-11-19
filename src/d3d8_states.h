#ifndef D3D8_STATES_H
#define D3D8_STATES_H

/* Xbox D3D8 render-state and texture-stage-state constants.
 *
 * An XDK inline SetRenderState of a "simple" state compiles to
 * D3DDevice_SetRenderState_Simple(method, value) followed by
 * D3D__RenderState[state] = value. The binary's D3DSIMPLERENDERSTATEENCODE
 * table (0x282b90) pairs each D3DRS_* index below with the NV097 method
 * word in nv097.h; D3D__RenderState (0x1fb698) is a kb.json data global.
 *
 * Include it only from the TUs that use it. Not from common.h or
 * xdk_common.h: extra macro names in every TU change VC71 tie-breaks.
 * Pulled in globally, it cost decals_delete_permanent_from_cluster and
 * rasterizer_debug_draw one byte each, though neither uses any of these names.
 * Some TUs still define a few of these locally with the same values;
 * identical redefinitions are legal. */

/* D3DRENDERSTATETYPE indices (D3D__RenderState slots). */
#define D3DRS_ZFUNC 0x39
#define D3DRS_ALPHABLENDENABLE 0x3b
#define D3DRS_ALPHATESTENABLE 0x3c
#define D3DRS_ALPHAREF 0x3d
#define D3DRS_SRCBLEND 0x3e
#define D3DRS_DESTBLEND 0x3f
#define D3DRS_ZWRITEENABLE 0x40
#define D3DRS_COLORWRITEENABLE 0x43
#define D3DRS_BLENDOP 0x4a

/* D3DTEXTURESTAGESTATETYPE */
#define D3DTSS_ADDRESSU 0xa
#define D3DTSS_ADDRESSV 0xb
#define D3DTSS_ADDRESSW 0xc
#define D3DTSS_MAGFILTER 0xd
#define D3DTSS_MINFILTER 0xe
#define D3DTSS_MIPFILTER 0xf

/* D3DTEXTUREADDRESS */
#define D3DTADDRESS_WRAP 1
#define D3DTADDRESS_CLAMP 3
#define D3DTADDRESS_BORDER 4

/* D3DTEXTUREFILTERTYPE */
#define D3DTEXF_POINT 1
#define D3DTEXF_LINEAR 2

/* D3DCULL */
#define D3DCULL_NONE 0
#define D3DCULL_CCW 0x901

/* D3DCLEAR flags (Xbox splits TARGET into per-channel bits) */
#define D3DCLEAR_ZBUFFER 0x1
#define D3DCLEAR_STENCIL 0x2
#define D3DCLEAR_TARGET_A 0x80
#define D3DCLEAR_TARGET 0xf0

/* D3DBLEND / D3DBLENDOP / D3DCMPFUNC */
#define D3DBLEND_ZERO 0
#define D3DBLEND_INVSRCCOLOR 0x301
#define D3DBLENDOP_ADD 0x8006
#define D3DCMP_EQUAL 0x202

#endif
