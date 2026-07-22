/* Glow widget update/simulate.
 *
 * TU: c:\halo\SOURCE\objects\widgets\glow.c  (confirmed via __FILE__ assert
 * string "c:\halo\SOURCE\objects\widgets\glow.c" referenced at 0x1345b0+0x50c).
 *
 * FUN_001345b0 (0x1345b0) — per-frame glow widget update.  ABI (immutable, from
 * kb.json): param_1 glow_widget in EAX; param_2 object_handle on stack; cdecl,
 * void return.  Prologue moves EAX->EBX and keeps the widget there throughout,
 * the glow tag block ('glw!') in ESI, and object_handle ([ebp+8]) in EDI.
 *
 * Widget layout (base = glow_widget):
 *   +0x02  uint8   one-time init flag
 *   +0x04  int16   live marker count
 *   +0x08  marker array base (0x6c/108-byte stride) passed to
 *          object_get_markers_by_string_id
 *   +0x70  per-marker basis struct (0x6c stride).  Relative to the +0x70 float
 *          pointer for marker i: element -0xb/-0xa/-9 = basis row, elements
 *          -2/-1/0 (bytes -8/-4/0) = marker position used by the neighbour
 *          search; segment arc math reads position at marker+0x60/+0x64/+0x68.
 *   +0x224 glow tag datum handle
 *   +0x22a int16[5] threaded marker ordering table
 *   +0x234 float cumulative segment length (zeroed at init)
 *   +0x238 float (zeroed at init)
 *   +0x23c float[] per-segment running length
 *   +0x24c int16  active particle count
 *   +0x250 int    particle list head
 *   +0x254 int    particle list tail
 *   +0x258 int16  spawn distance accumulator
 *
 * Particle node layout:
 *   +0x04 datum handle; +0x20/+0x24 float alpha base/current;
 *   +0x2c/0x30/0x34 position; +0x44/0x48/0x4c velocity;
 *   +0x50 int16 age; +0x52 int16 lifespan; +0x54 byte flags (bit 0x02 = loop);
 *   +0x5c next; +0x60 prev.
 *
 * Glow tag block ('glw!' = 0x676c7721, float* base):
 *   +0x60 int16 fn-index-A; +0x64 float scaleA; +0x68/+0x6c lerp lo/hi A;
 *   +0x70 int16 fn-index-B; +0x74 float scaleB; +0x78/+0x7c lerp lo/hi B;
 *   +0x28 byte flags (bit 0x10 = age-fade alpha);
 *   +0xfc float spawn-distance threshold divisor.
 *
 * Globals: 0x50654c = frame dt seconds; 0x2533c0 = 0.0f; 0x2533c8 = 1.0f;
 * 0x25bb10 = 0.01f threshold; 0x253394 = 30.0f (TICKS_PER_SECOND);
 * g_glow_particle_data pool pointer at 0x5a90cc.
 *
 * Assert tail: display_assert(...,1) then a call to FUN_001029a0 preceded by
 * `push $-1` (delinked reloc at +0x51d), i.e. the arg-passing system_exit(-1)
 * form (matches the rasterizer_decals precedent), NOT arg-less
 * halt_and_catch_fire().  Confirm with check_assert_targets.py.
 */

#define GLOW_TAG 0x676c7721 /* 'glw!' */

/* Number of glow markers = capacity of the +0x22a ordering table (0x234-0x22a
 * = 10 bytes = 5 int16 slots) and the object_get_markers_by_string_id max. */
#define GLOW_MARKER_MAX 5

void FUN_001345b0(int glow_widget, int object_handle)
{
  char *w;
  float *glow_tag;
  int marker_count;
  short marker_order[GLOW_MARKER_MAX];
  int i, j;
  short best_j;
  float best_dot;
  float d[3];
  float fn_val;
  float scale_a, scale_b, ratio;
  int particle;
  void *tag_block;

  w = (char *)glow_widget;
  glow_tag = (float *)tag_get(GLOW_TAG, *(int *)(w + 0x224));
  if (glow_tag == 0)
    return;

  marker_count = object_get_markers_by_string_id(object_handle, glow_tag, w + 8,
                                                 GLOW_MARKER_MAX);
  *(short *)(w + 4) = (short)marker_count;

  if (*(char *)(w + 2) == 0) {
    /* One-time init: build nearest-neighbour ordering, thread it, and
     * accumulate segment arc lengths. */
    if ((short)marker_count > 1) {
      for (i = 0; i < marker_count; i++) {
        float *basis_i = (float *)(w + 0x70 + i * 0x6c);
        best_j = -1;
        best_dot = 0.0f;
        for (j = 0; j < marker_count; j++) {
          if (i != j) {
            float *basis_j = (float *)(w + 0x70 + j * 0x6c);
            float dot;
            d[0] = basis_j[-2] - basis_i[-2];
            d[1] = basis_j[-1] - basis_i[-1];
            d[2] = basis_j[0] - basis_i[0];
            normalize3d(d);
            dot =
              d[0] * basis_i[-0xb] + d[1] * basis_i[-0xa] + d[2] * basis_i[-9];
            if (dot > best_dot) {
              best_dot = dot;
              best_j = (short)j;
            }
          }
        }
        marker_order[i] = best_j;
      }

      /* Thread the ordering into the +0x22a table, filling from the last
       * slot backward: each step finds the marker whose nearest neighbour is
       * the previously chained marker. */
      {
        short prev_idx = -1;
        short *order_out = (short *)(w + 0x22a + (marker_count - 1) * 2);
        int remaining = marker_count;
        int k;
        while (remaining != 0) {
          k = *(short *)(w + 4) - 1;
          while (k >= 0) {
            if (marker_order[k] == prev_idx) {
              *order_out = (short)k;
              break;
            }
            k--;
          }
          order_out--;
          remaining--;
          prev_idx = (short)k;
        }
      }

      /* Cumulative arc length along the threaded ordering. */
      *(float *)(w + 0x234) = 0.0f;
      *(float *)(w + 0x238) = 0.0f;
      {
        int count = *(short *)(w + 4);
        int seg;
        for (seg = 0; seg < count - 1; seg++) {
          int a_idx = *(short *)(w + 0x22a + seg * 2);
          int b_idx = *(short *)(w + 0x22a + (seg + 1) * 2);
          float *pa = (float *)(w + 8 + a_idx * 0x6c + 0x60);
          float *pb = (float *)(w + 8 + b_idx * 0x6c + 0x60);
          float dx = pb[0] - pa[0];
          float dy = pb[1] - pa[1];
          float dz = pb[2] - pa[2];
          float dist = sqrtf(dx * dx + dy * dy + dz * dz);
          *(float *)(w + 0x234) += dist;
          *(float *)(w + 0x23c + seg * 4) = *(float *)(w + 0x234);
        }
      }

      FUN_001342a0(glow_widget);
      *(short *)(w + 0x258) = 0;
      *(char *)(w + 2) = 1;
      return;
    }
  } else if ((short)marker_count > 1) {
    /* Steady-state: derive a scale ratio from the two tag functions. */
    float v;
    scale_a = glow_tag[0x19];
    if (*(short *)((char *)glow_tag + 0x60) != -1) {
      if (object_get_function_value(
            object_handle, *(short *)((char *)glow_tag + 0x60), &fn_val))
        v = fn_val;
      else
        v = *(float *)0x2533c0;
      scale_a =
        ((glow_tag[0x1b] - glow_tag[0x1a]) * v + glow_tag[0x1a]) * scale_a;
    }
    scale_b = glow_tag[0x1d];
    if (*(short *)((char *)glow_tag + 0x70) != -1) {
      if (object_get_function_value(
            object_handle, *(short *)((char *)glow_tag + 0x70), &fn_val))
        v = fn_val;
      else
        v = *(float *)0x2533c0;
      scale_b =
        ((glow_tag[0x1f] - glow_tag[0x1e]) * v + glow_tag[0x1e]) * scale_b;
    }
    ratio = scale_a / scale_b;
  }

  *(short *)(w + 0x258) += (short)game_time_get();

  /* Advance non-loop particles. */
  if (*(short *)(w + 4) > 1) {
    for (particle = *(int *)(w + 0x250); particle != 0;
         particle = *(int *)(particle + 0x5c)) {
      if ((*(unsigned char *)(particle + 0x54) & 2) == 0) {
        FUN_00134070(particle, glow_widget, object_handle,
                     *(float *)0x50654c * scale_b, ratio);
        /* FUN_00133300 recomputes the particle RGB colour from the glow tag,
         * which it reaches via glow_widget passed in EBX (@<ebx>).  The
         * original 0x1345b0 kept glow_widget in EBX across the call; our lift
         * must pass it explicitly or the tag lookup reads garbage and the
         * particle renders the wrong colour (orange-not-blue trail bug). */
        FUN_00133300(particle, object_handle, glow_widget);
        *(int *)(particle + 0x24) = *(int *)(particle + 0x20);
      }
    }
  }

  /* Advance loop particles: age, fade, integrate position, expire. */
  for (particle = *(int *)(w + 0x250); particle != 0;
       particle = *(int *)(particle + 0x5c)) {
    if ((*(unsigned char *)(particle + 0x54) & 2) != 0) {
      *(short *)(particle + 0x50) += (short)game_time_get();
      /* FUN_001330f0 computes the age-based fade into particle+0x58; it reads
       * the particle via ESI (movswl 0x50/0x52(%esi), fstps 0x58(%esi) in the
       * pristine XBE) — an undeclared @<esi> arg the original kept live in
       * ESI across the loop.  Dropping it read garbage and broke the trailing
       * particle fade/colour (orange-not-blue trail bug, loop-B class). */
      FUN_001330f0(glow_widget, particle);
      tag_block = tag_get(GLOW_TAG, *(int *)(w + 0x224));
      if ((*(unsigned char *)((char *)tag_block + 0x28) & 0x10) != 0) {
        int age = *(short *)(particle + 0x50);
        int life = *(short *)(particle + 0x52);
        float t = *(float *)0x2533c8 - (float)age / life;
        if (t < *(float *)0x2533c0)
          t = *(float *)0x2533c0;
        *(float *)(particle + 0x24) = t * *(float *)(particle + 0x20);
      }
      FUN_001331d0(glow_widget, particle);
      {
        float dt = *(float *)0x50654c;
        tag_get(GLOW_TAG, *(int *)(w + 0x224));
        *(float *)(particle + 0x2c) += dt * *(float *)(particle + 0x44);
        *(float *)(particle + 0x30) += dt * *(float *)(particle + 0x48);
        *(float *)(particle + 0x34) += dt * *(float *)(particle + 0x4c);
      }
      tag_get(GLOW_TAG, *(int *)(w + 0x224));
      if (*(short *)(particle + 0x50) > *(short *)(particle + 0x52)) {
        int prev = *(int *)(particle + 0x60);
        int next = *(int *)(particle + 0x5c);
        if (prev == 0)
          *(int *)(w + 0x250) = next;
        else
          *(int *)(prev + 0x5c) = next;
        if (next == 0)
          *(int *)(w + 0x254) = prev;
        else
          *(int *)(next + 0x60) = prev;
        datum_delete(*(data_t **)0x5a90cc, *(int *)(particle + 4));
        *(short *)(w + 0x24c) -= 1;
      }
    }
  }

  /* Spawn new trailing particles proportional to distance travelled. */
  if (*(float *)0x25bb10 < glow_tag[0x3f]) {
    float spacing = *(float *)0x253394 / glow_tag[0x3f];
    if (spacing < *(float *)0x2533c8)
      spacing = 1.0f;
    if (spacing < (float)(int)*(short *)(w + 0x258)) {
      do {
        particle = glow_trailing_particle_new(glow_widget);
        if (particle == 0) {
          display_assert("the map limit for the number of active glow "
                         "particles has been reached",
                         "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x209,
                         1);
          system_exit(-1);
        }
        *(short *)(w + 0x24c) += 1;
        if (*(int *)(w + 0x254) == 0) {
          *(int *)(w + 0x250) = particle;
        } else {
          *(int *)(*(int *)(w + 0x254) + 0x5c) = particle;
          *(int *)(particle + 0x60) = *(int *)(w + 0x254);
        }
        *(int *)(w + 0x254) = particle;
        *(short *)(w + 0x258) -= (short)(int)spacing;
      } while (spacing < (float)(int)*(short *)(w + 0x258));
    }
  }
}
