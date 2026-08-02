/* rasterizer_lights.c — lens flare queue submission.
 *
 * TU proven by the __FILE__ string
 * "c:\halo\SOURCE\rasterizer\rasterizer_lights.c" at 0x2b01b4, referenced by
 * every assert in FUN_00181670. */

#define RASTERIZER_LIGHTS_FILE "c:\\halo\\SOURCE\\rasterizer\\rasterizer_lights.c"

/* Bounds recovered from the assert compares in the original code. */
#define MAXIMUM_QUEUED_LENS_FLARES 8                        /* cmp di,8   */
#define MAXIMUM_LENS_FLARE_MARKERS_PER_STRUCTURE 0x10000    /* cmp esi,0x10000 */
#define MAXIMUM_LIGHTS_PER_MAP 0x380                        /* cmp ax,0x380 */
#define MAXIMUM_LENS_FLARES_PER_FRAME 0x400                 /* cmp ecx,0x400 */

/* AND EAX,0xffffff7f — bit 7 of the compressed window index is a flag, the
 * remaining bits are the window index proper. */
#define _lens_flare_window_index_mask (~0x80)

/* Lens flare definition tag. Only the cutoff distance at +0x1c is touched
 * here; everything before it is unexamined by this function. */
typedef struct lens_flare_definition {
  unsigned char field_00[0x1c];
  real cutoff_distance; /* +0x1c */
} lens_flare_definition;

/* Submission parameters, 0x28 bytes, copied wholesale into the frame queue.
 * Offsets confirmed against the disassembly and against the caller
 * FUN_00181900 (rasterizer_text.c), which fills the same buffer. */
typedef struct lens_flare_parameters {
  lens_flare_definition *definition; /* +0x00 */
  real position[3];                  /* +0x04 FLD dword, not FILD */
  unsigned int field_10;             /* +0x10 compressed direction normal */
  unsigned int field_14;             /* +0x14 compressed perpendicular normal */
  unsigned int color;                /* +0x18 gated on the high (alpha) byte */
  short field_1c;                    /* +0x1c -1 selects the structure path */
  short light_index;                 /* +0x1e also hi word of the structure
                                      *       lens flare marker index */
  short lens_flare_index;            /* +0x20 also lo word of that index */
  unsigned char compressed_window_index; /* +0x22 */
  unsigned char field_23;            /* +0x23 */
  int field_24;                      /* +0x24 */
} lens_flare_parameters;

/* Per-map light lens flare state, stride 0x22 (IMUL ESI,ESI,0x22). The first
 * word is a cache key: when it changes, the 0x20-byte tail is cleared. */
typedef struct lens_flare_light_state {
  short definition_index;    /* +0x00 */
  unsigned char state[0x20]; /* +0x02 */
} lens_flare_light_state;

#define lens_flare_light_states ((lens_flare_light_state *)0x4bed80)

/* Window/view parameters block at 0x5a5bc0. origin/forward are inferred from
 * the use below (a dot product of forward against position-origin compared to
 * a cutoff distance); window_index is proven by the assert string. */
typedef struct window_parameters {
  short field_00; /* +0x00 must be 0 for lens flares to be queued */
  short window_index; /* +0x02 */
  int field_04;   /* +0x04 */
  real origin[3]; /* +0x08 */
  real forward[3];/* +0x14 */
} window_parameters;

#define global_window_parameters (*(window_parameters *)0x5a5bc0)

/* Submit one lens flare to this frame's queue (0x181670).
 *
 * Rejects the flare unless lens flares are enabled, this is the primary
 * window, the frame queue has room, the flare is nearer than its definition's
 * cutoff distance along the view direction, and its color has a non-zero
 * alpha. Accepted flares are copied into a queue slot obtained from
 * FUN_00181020 and tagged either as a directly-queued flare, a structure
 * marker flare, or a map light flare (which also resets that light's cached
 * state when its definition changes). */
void FUN_00181670(int *params)
{
  lens_flare_parameters *parameters;
  lens_flare_parameters *queued;
  lens_flare_light_state *light_state;
  int queued_count;
  int structure_lens_flare_index;
  short light_index;

  parameters = (lens_flare_parameters *)params;

  assert_halt_at(RASTERIZER_LIGHTS_FILE, 0x10a, parameters);
  assert_halt_at(RASTERIZER_LIGHTS_FILE, 0x10b, parameters->definition);
  assert_halt_at(RASTERIZER_LIGHTS_FILE, 0x10c,
                 (parameters->compressed_window_index &
                  _lens_flare_window_index_mask) ==
                   global_window_parameters.window_index);

  if (*(char *)0x3256d7 != 0 && *(short *)0x46e008 <= 1 &&
      (*(short *)0x46e008 != 1 || *(short *)0x31fa98 <= 1) &&
      global_window_parameters.field_00 == 0) {
    queued_count = *(int *)0x4d0480;
    if (queued_count < MAXIMUM_LENS_FLARES_PER_FRAME) {
      /* Cutoff of zero means "never cull"; otherwise cull once the flare is
       * at or beyond the cutoff distance along the view direction. The x87
       * accumulation order is z, then y, then x (FADDP pairs the z and y
       * products first) — keep the source in that order. */
      if (parameters->definition->cutoff_distance == 0.0f ||
          global_window_parameters.forward[2] *
                (parameters->position[2] - global_window_parameters.origin[2]) +
              global_window_parameters.forward[1] *
                (parameters->position[1] - global_window_parameters.origin[1]) +
              global_window_parameters.forward[0] *
                (parameters->position[0] - global_window_parameters.origin[0]) <
            parameters->definition->cutoff_distance) {
        if ((parameters->color & 0xff000000) > 0) {
          *(int *)0x4d0480 = queued_count + 1;
          queued =
            (lens_flare_parameters *)FUN_00181020((short)queued_count);
          csmemcpy(queued, parameters, 0x28);
          light_index = parameters->light_index;
          if (parameters->field_1c == -1) {
            if (light_index == -1) {
              queued->light_index = (short)0x8000;
              assert_halt_at(
                RASTERIZER_LIGHTS_FILE, 0x136,
                parameters->lens_flare_index >= 0 &&
                  parameters->lens_flare_index < MAXIMUM_QUEUED_LENS_FLARES);
            } else {
              structure_lens_flare_index =
                ((int)light_index << 16) | (int)parameters->lens_flare_index;
              assert_halt_at(RASTERIZER_LIGHTS_FILE, 0x141,
                             structure_lens_flare_index >= 0 &&
                               structure_lens_flare_index <
                                 MAXIMUM_LENS_FLARE_MARKERS_PER_STRUCTURE);
              queued->lens_flare_index =
                (short)(structure_lens_flare_index + 8);
              queued->light_index =
                (short)((structure_lens_flare_index >> 16) | 0xffff8000);
            }
          } else {
            light_state = &lens_flare_light_states[queued->light_index];
            assert_halt_at(RASTERIZER_LIGHTS_FILE, 0x152,
                           parameters->light_index >= 0 &&
                             parameters->light_index < MAXIMUM_LIGHTS_PER_MAP);
            if (parameters->field_1c != light_state->definition_index) {
              csmemset(light_state->state, 0, 0x20);
              light_state->definition_index = queued->field_1c;
            }
          }
          if (*(short *)0x3256ba == 2) {
            (*(int *)0x5a554c)++;
          }
        }
      }
    } else if (*(char *)0x4d0484 == 0) {
      error(2, "### ERROR too many lens flares submitted to frame");
      *(char *)0x4d0484 = 1;
    }
  }
}
