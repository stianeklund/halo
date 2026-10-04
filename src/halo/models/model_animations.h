/* Globals read by model_animations.c (render_model and the animation frame
 * helpers).  Repo-local header: the original header path is not
 * binary-proven.  Debug-global names are the strings in the hs external
 * globals table ({name, hs type, address} entries). */

#ifndef HALO_MODELS_MODEL_ANIMATIONS_H
#define HALO_MODELS_MODEL_ANIMATIONS_H

#include "../../types.h"

#define model_animation_compression (*(char *)0x322600)
#define rasterizer_debug_model_lod (*(short *)0x3256c0)
#define render_model_markers (*(char *)0x5aa251)
#define render_model_index_counts (*(char *)0x5aa252)
#define render_model_vertex_counts (*(char *)0x5aa253)
#define render_model_nodes (*(char *)0x5aa254)

/* same text as src/halo/objects/objects.h (identical redefinition) */
#define global_real_argb_white (*(real_argb_color **)0x2ee6c4)
#define global_real_argb_orange (*(real_argb_color **)0x2ee6f0)

/* profile section whose name string is "render_model"; +0x08 is the
 * section's active byte. */
#define render_model_profile_section ((void *)0x322608)
#define render_model_profile_section_active (*(char *)0x322610)

#endif
