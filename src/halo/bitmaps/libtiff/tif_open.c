/* Horizontal differencing predictor accumulator, 8-bit samples.
 *
 * Transcribed from the vendored libtiff (tif_predict.c horAcc8) rather than
 * reshaped from the decompiler: the switch/fallthrough chain below is the
 * REPEAT4 Duff-device macro, and the binary reproduces it exactly -- jump
 * table at 0x6c6d4 with five entries (stride 0..4) plus a `cmp ecx,4 / ja`
 * bound check that lands on the counted-loop default arm.
 *
 * Bungie's copy takes the stride as an explicit third stack argument instead
 * of fetching it from PredictorState(tif)->stride, so there is no TIFF*
 * parameter here. Confirmed against 0x6c680-0x6c6d1.
 */

/* switch(n) with intentional fallthrough -- cases 4..1 emit `op` once each and
 * fall into the next, so `n` copies of `op` run for n <= 4, and the default
 * arm covers the remaining n-4 with a counted loop. Do NOT insert `break`. */
#define REPEAT4(n, op)              \
  switch (n) {                      \
  default: {                        \
    int i;                          \
    for (i = (n) - 4; i > 0; i--) { \
      op;                           \
    }                               \
  }                                 \
  case 4:                           \
    op;                             \
  case 3:                           \
    op;                             \
  case 2:                           \
    op;                             \
  case 1:                           \
    op;                             \
  case 0:;                          \
  }

/**
 * Undo horizontal differencing over a scanline of 8-bit samples in place.
 *
 * Each sample is replaced by the sum of itself and the sample `stride` bytes
 * before it, walking forward so the accumulation carries across the whole row.
 * The byte add wraps (the binary uses `add byte ptr [eax+ecx], dl`).
 *
 * @param cp     scanline base; at least `cc` bytes of caller memory.
 * @param cc     byte count of the scanline.
 * @param stride bytes between a sample and its horizontal predecessor
 *               (samples-per-pixel). Signed compares throughout.
 */
void FUN_0006c680(char *cp, int cc, int stride)
{
  if (cc > stride) {
    cc -= stride;
    do {
      REPEAT4(stride, cp[stride] = (char)(cp[stride] + *cp); cp++)
      cc -= stride;
    } while (cc > 0);
  }
}
