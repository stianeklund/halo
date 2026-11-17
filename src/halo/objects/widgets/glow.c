#include "x87_math.h" /* x87_fsin/x87_fcos: inline x87 ops */
#include "../objects.h" /* REAL_ONE_THIRTIETH_POOL */
/* Glow widget update/simulate.
 *
 * TU: c:\halo\SOURCE\objects\widgets\glow.c  (confirmed via __FILE__ assert
 * string "c:\halo\SOURCE\objects\widgets\glow.c" referenced at 0x1345b0+0x50c).
 *
 * glow_update (0x1345b0) — per-frame glow widget update.  ABI (immutable, from
 * kb.json): param_1 glow_widget in EAX; param_2 object_handle on stack; cdecl,
 * void return.  Prologue moves EAX->EBX and keeps the widget there throughout,
 * the glow tag block ('glw!') in ESI, and object_handle ([ebp+8]) in EDI.
 *
 * Widget layout (base = glow_widget):
 *   +0x02  uint8   one-time init flag
 *   +0x04  int16   live marker count
 *   +0x08  marker array base (0x6c/108-byte stride) passed to
 *          object_get_marker_by_name
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


/* glow_globals data-array pointers (PAL-2342 glow.c: glow_globals.glow_data /
 * glow_globals.glow_particle_data).  Addresses from glow_initialize's stores.
 */
#define GLOW_DATA (*(data_t **)0x5a90c8)
#define GLOW_PARTICLE_DATA (*(data_t **)0x5a90cc)

/* glow_dispose (0x132f40)
 *
 * Confirmed: the body is a single RET (C3).  Only reference is the object
 * dispose table entry at 0x32358c. */
void glow_dispose(void)
{
}

/* glow_initialize_for_new_map (0x132f50)
 *
 * PAL-2342 (T2): data_make_valid on each glow pool that exists; the 2276 pool
 * call is the function kb.json names data_delete_all (0x119b20). */
void glow_initialize_for_new_map(void)
{
  if (GLOW_DATA)
    data_delete_all(GLOW_DATA);
  if (GLOW_PARTICLE_DATA)
    data_delete_all(GLOW_PARTICLE_DATA);
}

/* glow_dispose_from_old_map (0x132f80)
 *
 * PAL-2342 (T2): data_make_invalid on each glow pool that exists. */
void glow_dispose_from_old_map(void)
{
  if (GLOW_DATA)
    data_make_invalid(GLOW_DATA);
  if (GLOW_PARTICLE_DATA)
    data_make_invalid(GLOW_PARTICLE_DATA);
}

/* glow_initialize (0x133750)
 *
 * Allocates the "glow" (8 x 0x25c) and "glow particles" (0x200 x 0x64)
 * game-state data arrays, each only if not already allocated.  Failures are
 * reported through error(2, ...) (non-fatal), not display_assert.  Name and
 * shape from PAL-2342 glow.c (T2); strings verified at 0x29ab58/0x29ab48/
 * 0x29ab1c/0x29aaf8. */
void glow_initialize(void)
{
  if (!GLOW_DATA) {
    GLOW_DATA = game_state_data_new("glow", 8, 0x25c);
    if (GLOW_DATA) {
      if (!GLOW_PARTICLE_DATA) {
        GLOW_PARTICLE_DATA = game_state_data_new("glow particles", 0x200, 0x64);
        if (!GLOW_PARTICLE_DATA) {
          error(2, "could not allocate glow particle data array");
          return;
        }
      }
    } else {
      error(2, "could not allocate glow data array");
    }
  }
}

/* Number of glow markers = capacity of the +0x22a ordering table (0x234-0x22a
 * = 10 bytes = 5 int16 slots) and the object_get_marker_by_name max. */
#define GLOW_MARKER_MAX 5

void glow_update(int glow_widget, int object_handle)
{
  float *glow_tag;
  short marker_count;
  float scale_b, ratio;
  int particle;
  void *tag_block;

  char *w;
  w = (char *)glow_widget;
  glow_tag = (float *)tag_get(TAG_GROUP_GLW, *(int *)(w + 0x224));
  if (glow_tag == 0)
    return;

  marker_count = (short)object_get_marker_by_name(object_handle, glow_tag,
                                                        w + 8, GLOW_MARKER_MAX);
  *(short *)(w + 4) = marker_count;

  if (*(char *)(w + 2) == 0) {
    short marker_order[GLOW_MARKER_MAX];
    short i, j;
    int best_j;
    float best_dot;
    short *order_out;
    float *basis_i;
    float *basis_j;

    /* One-time init: build nearest-neighbour ordering, thread it, and
     * accumulate segment arc lengths. */
    if (*(short *)(w + 4) > 1) {
      order_out = marker_order;
      basis_i = (float *)(w + 0x70);
      i = 0;
      if (i < *(short *)(w + 4)) {
        do {
          float d[3];
          best_j = -1;
          best_dot = 0.0f;
          basis_j = (float *)(w + 0x70);
          j = 0;
          do {
            if (i != j) {
              float dot;
              d[0] = basis_j[-2] - basis_i[-2];
              d[1] = basis_j[-1] - basis_i[-1];
              d[2] = basis_j[0] - basis_i[0];
              normalize3d((real_vector3d *)d);
              dot = d[0] * basis_i[-0xb] + d[1] * basis_i[-0xa] +
                    d[2] * basis_i[-9];
              if (dot > best_dot) {
                best_dot = dot;
                best_j = j;
              }
            }
            basis_j += 0x6c / 4;
            j++;
          } while (j < *(short *)(w + 4));
          *order_out = (short)best_j;
          order_out++;
          basis_i += 0x6c / 4;
          i++;
        } while (i < *(short *)(w + 4));
      }

      /* Thread the ordering into the +0x22a table, filling from the last
       * slot backward: each step finds the marker whose nearest neighbour is
       * the previously chained marker. */
      {
        short prev_idx = -1;
        int remaining = marker_count;
        short k;
        glow_tag = (float *)(w + 0x22a + (marker_count - 1) * 2);
        if (remaining != 0) {
          do {
            k = *(short *)(w + 4) - 1;
            while (k >= 0) {
              if (marker_order[k] == prev_idx) {
                *(short *)glow_tag = (short)k;
                break;
              }
              k--;
            }
            glow_tag = (float *)((char *)glow_tag - 2);
            remaining--;
            prev_idx = (short)k;
          } while (remaining != 0);
        }
      }

      /* Cumulative arc length along the threaded ordering. */
      *(float *)(w + 0x234) = 0.0f;
      *(float *)(w + 0x238) = 0.0f;
      {
        int seg_index;
        short seg_count;
        seg_index = 0;
        seg_count = 0;
        if (*(short *)(w + 4) - 1 > 0) {
          do {
            int a_idx = *(short *)(w + 0x22a + seg_index * 2);
            int b_idx = *(short *)(w + 0x22a + (seg_index + 1) * 2);
            volatile float point_a[3];
            volatile float point_b[3];
            point_a[0] = *(float *)(w + 8 + a_idx * 0x6c + 0x60);
            point_a[1] = *(float *)(w + 8 + a_idx * 0x6c + 0x64);
            point_a[2] = *(float *)(w + 8 + a_idx * 0x6c + 0x68);
            point_b[0] = *(float *)(w + 8 + b_idx * 0x6c + 0x60);
            point_b[1] = *(float *)(w + 8 + b_idx * 0x6c + 0x64);
            point_b[2] = *(float *)(w + 8 + b_idx * 0x6c + 0x68);
            {
              float dx = point_b[0] - point_a[0];
              float dy = point_b[1] - point_a[1];
              float dz = point_b[2] - point_a[2];
              float dist = sqrtf(dx * dx + dy * dy + dz * dz);
              *(float *)(w + 0x234) += dist;
              *(float *)(w + 0x23c + seg_index * 4) = *(float *)(w + 0x234);
              seg_count++;
              seg_index = (int)seg_count;
            }
          } while (seg_index < *(short *)(w + 4) - 1);
        }
      }

      glow_particles_initialize(glow_widget);
      *(short *)(w + 0x258) = 0;
      *(char *)(w + 2) = 1;
      return;
    }
  } else if (*(short *)(w + 4) > 1) {
    float scale_a;

    /* Steady-state: derive a scale ratio from the two tag functions. */
    float v;
    scale_a = glow_tag[0x19];
    if (*(short *)((char *)glow_tag + 0x60) != -1) {
      if (!object_get_function_value(
            object_handle, *(short *)((char *)glow_tag + 0x60), &ratio))
        v = *(float *)0x2533c0;
      else
        v = ratio;
      scale_a =
        ((glow_tag[0x1b] - glow_tag[0x1a]) * v + glow_tag[0x1a]) * scale_a;
    }
    scale_b = glow_tag[0x1d];
    if (*(short *)((char *)glow_tag + 0x70) != -1) {
      if (!object_get_function_value(
            object_handle, *(short *)((char *)glow_tag + 0x70), &ratio))
        v = *(float *)0x2533c0;
      else
        v = ratio;
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
        glow_normal_particle_update_position(
          particle, glow_widget, object_handle, *(float *)0x50654c * scale_b,
          ratio);
        /* glow_normal_particle_update_color recomputes the particle RGB colour
         * from the glow tag, which it reaches via glow_widget passed in EBX
         * (@<ebx>).  The original 0x1345b0 kept glow_widget in EBX across the
         * call; our lift must pass it explicitly or the tag lookup reads
         * garbage and the particle renders the wrong colour (orange-not-blue
         * trail bug). */
        glow_normal_particle_update_color(particle, object_handle, glow_widget);
        *(int *)(particle + 0x24) = *(int *)(particle + 0x20);
      }
    }
  }

  /* Advance loop particles: age, fade, integrate position, expire. */
  for (particle = *(int *)(w + 0x250); particle != 0;
       particle = *(int *)(particle + 0x5c)) {
    if ((*(unsigned char *)(particle + 0x54) & 2) != 0) {
      *(short *)(particle + 0x50) += (short)game_time_get();
      /* glow_trailing_particle_update_color computes the age-based fade into
       * particle+0x58; it reads the particle via ESI (movswl 0x50/0x52(%esi),
       * fstps 0x58(%esi) in the pristine XBE) — an undeclared @<esi> arg the
       * original kept live in ESI across the loop.  Dropping it read garbage
       * and broke the trailing particle fade/colour (orange-not-blue trail bug,
       * loop-B class). */
      glow_trailing_particle_update_color(glow_widget, particle);
      tag_block = tag_get(TAG_GROUP_GLW, *(int *)(w + 0x224));
      if ((*(unsigned char *)((char *)tag_block + 0x28) & 0x10) != 0) {
        int age = *(short *)(particle + 0x50);
        int life = *(short *)(particle + 0x52);
        float t = *(float *)0x2533c8 - (float)age / life;
        if (t < *(float *)0x2533c0)
          t = *(float *)0x2533c0;
        *(float *)(particle + 0x24) = t * *(float *)(particle + 0x20);
      }
      glow_trailing_particle_update_velocity(glow_widget, particle);
      {
        float dt = *(float *)0x50654c;
        tag_get(TAG_GROUP_GLW, *(int *)(w + 0x224));
        *(float *)(particle + 0x2c) += dt * *(float *)(particle + 0x44);
        *(float *)(particle + 0x30) += dt * *(float *)(particle + 0x48);
        *(float *)(particle + 0x34) += dt * *(float *)(particle + 0x4c);
      }
      tag_get(TAG_GROUP_GLW, *(int *)(w + 0x224));
      if (*(short *)(particle + 0x50) > *(short *)(particle + 0x52)) {
        int prev = *(int *)(particle + 0x60);
        int next = *(int *)(particle + 0x5c);
        if (prev != 0)
          *(int *)(prev + 0x5c) = next;
        else
          *(int *)(w + 0x250) = next;
        if (next != 0)
          *(int *)(next + 0x60) = prev;
        else
          *(int *)(w + 0x254) = prev;
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
          return;
        }
        *(short *)(w + 0x24c) += 1;
        if (*(int *)(w + 0x254) != 0) {
          *(int *)(*(int *)(w + 0x254) + 0x5c) = particle;
          *(int *)(particle + 0x60) = *(int *)(w + 0x254);
        } else {
          *(int *)(w + 0x250) = particle;
        }
        *(int *)(w + 0x254) = particle;
        *(short *)(w + 0x258) -= (short)(int)spacing;
      } while (spacing < (float)(int)*(short *)(w + 0x258));
    }
  }
}

/* nonuniform_cubic_spline (0x1335e0 / objects.obj, ..\math\real_math.h inline)
 * — nonuniform cubic spline of four samples f0..f3 at knots t0..t3, evaluated
 * at t (Newton divided differences). The assert string at 0x29aae4, "t>= t0 &&
 * t <= t3" (real_math.h line 0x5fa), names t0/t3/t; the PAL reference calls the
 * function nonuniform_cubic_spline.
 *
 * The divided differences reuse the f1/f3 parameter slots
 * (FSTP [EBP+0xc] / [EBP+0x14] at 0x133647 / 0x133656), matching in-place
 * updates of the parameters. cdecl, float return in ST0.
 * Name: PAL-2342 glow.c (T2) — same slot in PAL's symbol order, between
 * glow_render and nonuniform_cubic_spline_vector3d (its only caller);
 * carries the real_math.h assert "t>= t0 && t <= t3".
 */
float nonuniform_cubic_spline(float f0, float f1, float f2, float f3, float t0,
                              float t1, float t2, float t3, float t)
{
  if (!(t >= t0 && t <= t3)) {
    display_assert("t>= t0 && t <= t3", "..\\math\\real_math.h", 0x5fa, 1);
    system_exit(-1);
  }

  f3 = (f3 - f2) / (t3 - t2);
  f2 = (f2 - f1) / (t2 - t1);
  f1 = (f1 - f0) / (t1 - t0);
  f3 = (f3 - f2) / (t3 - t1);
  f2 = (f2 - f1) / (t2 - t0);
  f3 = (f3 - f2) / (t3 - t0);

  return f0 + (t - t0) * (f1 + (t - t1) * (f2 + (t - t2) * f3));
}

/* nonuniform_cubic_spline_vector3d (0x1336a0 / objects.obj / glow.c) — blend
 * the x, y and z components of four points via nonuniform_cubic_spline, calling
 * it once per axis (offsets 0/4/8 into pt_a..pt_d) with the same five extra
 * values each time. Called three times by get_particle_world_position
 * (0x1339a0, xrefs at 0x133f91/0x133fbe/0x133ff7); nonuniform_cubic_spline's
 * own role and pt_a..pt_d/ w_a..w_e's semantics are unconfirmed -- names are
 * mechanical placeholders, not a claim about what is being blended.
 *
 * nonuniform_cubic_spline (ported above) is cdecl with a float return via ST0;
 * its kb.json signature is widened from disassembly -- Ghidra's decompile shows
 * void(void) because it can't recover args from a caller alone -- so this
 * call's ABI matches the binary. Argument order per call site (first PUSH is
 * the last cdecl arg): *(pt+off), then w_a..w_e unchanged from
 * nonuniform_cubic_spline_vector3d's own params. The call_site_audit's
 * SWALLOWED hazard on the third call (cleanup_args=9) is a false lead: that
 * call is the function's last, so there is no following call for those 9 pushes
 * to belong to -- it is this caller's normal deferred-cleanup pattern (one ADD
 * ESP,0x48 batches calls 1+2's cleanup before call 3 issues its own pushes,
 * then ADD ESP,0x24 cleans call 3 alone). */
void nonuniform_cubic_spline_vector3d(float *out_xyz, float *pt_a, float *pt_b,
                                      float *pt_c, float *pt_d, float w_a,
                                      float w_b, float w_c, float w_d,
                                      float w_e)
{
  out_xyz[0] = nonuniform_cubic_spline(pt_a[0], pt_b[0], pt_c[0], pt_d[0], w_a,
                                       w_b, w_c, w_d, w_e);
  out_xyz[1] = nonuniform_cubic_spline(pt_a[1], pt_b[1], pt_c[1], pt_d[1], w_a,
                                       w_b, w_c, w_d, w_e);
  out_xyz[2] = nonuniform_cubic_spline(pt_a[2], pt_b[2], pt_c[2], pt_d[2], w_a,
                                       w_b, w_c, w_d, w_e);
}

/* glow_normal_particle_new (0x1337c0 / objects.obj / glow.c)
 *
 * Allocates a new "normal" glow particle datum from GLOW_PARTICLE_DATA and
 * seeds the fields glow_normal_particle_update_position doesn't recompute every
 * frame: an initial scale/size sampled from the glowdef's random ranges when no
 * object function drives that channel, an initial color lerp between the
 * glowdef's min/max color when no color function is bound and the glowdef isn't
 * flagged solid-color, and an initial phase either uniformly random over [0,
 * period) or spread evenly across the `count` particles being created by the
 * caller (glow_particles_initialize). Returns 0 (particle stays NULL) if the
 * pool is exhausted.
 *
 * Confirmed against 001337c0-00133992:
 *   glow_tag = tag_get(TAG_GROUP_GLW, *(glow_widget_ptr+0x224)).
 *   idx = data_new_at_index(GLOW_PARTICLE_DATA); returns 0 if idx == -1.
 *   particle = datum_get(GLOW_PARTICLE_DATA, idx); particle+4 = idx.
 *   scale (+0x1c): if glow_tag+0x80 (function index, see the glowdef layout
 *     comment below for glow_normal_particle_update_position) == -1,
 * random_real_range over [glow_tag+0x84, glow_tag+0x88) (scale_lower/upper)
 * assigned directly -- no extra lerp math, matching the disassembly (FSTP
 * straight from the call's ST0 result). size (+0x20): if glow_tag+0x9c (a
 * second, distinct function index) == -1, random_real_range over
 * [glow_tag+0xa0, glow_tag+0xa4) -- the same radius range
 * glow_trailing_particle_new samples -- divided by the widget's
 * glow_widget_ptr+0x228 int16 divisor (the same divisor
 *     glow_trailing_particle_new applies to its own size field). The FST
 *     (not FSTP) at 0x133870 plus the later FILD/FDIVR at
 *     0x133883-0x133888 divide the live FPU value in place; written here as
 *     two sequential C stores to the same slot to preserve that shape.
 *   color (+0xc/+0x10/+0x14/+0x18): if glow_tag+0xb0 (a third function index)
 *     == -1 AND glow_tag+0x28 bit0 is clear, alpha (+0xc) = 1.0f and RGB is
 *     lerped between glow_tag+0xb8/0xbc/0xc0 (min) and glow_tag+0xc8/0xcc/0xd0
 *     (max) by one shared random t in [0,1) -- the identical lerp
 *     glow_trailing_particle_new performs, minus its particle+0x54 |= 2 flag
 *     set (disassembly here has no such OR).
 *   phase (+0x28): glow_tag+0x24 selects the distribution: 0 =
 *     random_real_range(0, glow_widget_ptr+0x234 [period]); 1 = (index/count)
 *     * period; any other value asserts and exits (reason string is NULL in
 *     the binary -- PUSH 0x0 at 0x13391d -- filepath
 *     "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", line 0x3b1).
 *   +0x8: random_real_range(0, 2*pi) -- written unconditionally at the end;
 *     no consumer lifted yet, so it stays a raw offset store (field_08,
 *     offset accessed only, semantics unproven).
 *
 * Every random draw refetches random_math_get_local_seed_address()
 * immediately before its matching random_real_range() call, matching
 * disassembly -- the seed pointer is never cached across draws.
 */
int glow_normal_particle_new(int glow_widget_ptr, short index, short count)
{
  int glow_tag;
  int idx;
  int particle;
  float fmin;
  float fmax;
  float t;

  glow_tag = (int)tag_get(TAG_GROUP_GLW, *(int *)(glow_widget_ptr + 0x224));
  particle = 0;
  idx = data_new_at_index(GLOW_PARTICLE_DATA);
  if (idx != -1) {
    particle = (int)datum_get(GLOW_PARTICLE_DATA, idx);
    *(int *)(particle + 4) = idx;

    if (*(int16_t *)(glow_tag + 0x80) == -1) {
      fmax = *(float *)(glow_tag + 0x88);
      fmin = *(float *)(glow_tag + 0x84);
      *(float *)(particle + 0x1c) = random_real_range(
        random_math_get_local_seed_address(), fmin, fmax);
    }

    if (*(int16_t *)(glow_tag + 0x9c) == -1) {
      fmax = *(float *)(glow_tag + 0xa4);
      fmin = *(float *)(glow_tag + 0xa0);
      *(float *)(particle + 0x20) = random_real_range(
        random_math_get_local_seed_address(), fmin, fmax);
      *(float *)(particle + 0x20) =
        *(float *)(particle + 0x20) /
        (float)*(int16_t *)(glow_widget_ptr + 0x228);
    }

    if ((*(int16_t *)(glow_tag + 0xb0) == -1) &&
        ((*(unsigned char *)(glow_tag + 0x28) &
          FLAG(_glow_definition_modify_particle_color_bit)) == 0)) {
      t = random_real_range(random_math_get_local_seed_address(), 0.0f,
                            1.0f);
      *(float *)(particle + 0xc) = 1.0f;
      *(float *)(particle + 0x10) =
        (*(float *)(glow_tag + 0xc8) - *(float *)(glow_tag + 0xb8)) * t +
        *(float *)(glow_tag + 0xb8);
      *(float *)(particle + 0x14) =
        (*(float *)(glow_tag + 0xcc) - *(float *)(glow_tag + 0xbc)) * t +
        *(float *)(glow_tag + 0xbc);
      *(float *)(particle + 0x18) =
        (*(float *)(glow_tag + 0xd0) - *(float *)(glow_tag + 0xc0)) * t +
        *(float *)(glow_tag + 0xc0);
    }

    switch (*(int16_t *)(glow_tag + 0x24)) {
    case 0:
      *(float *)(particle + 0x28) =
        random_real_range(random_math_get_local_seed_address(), 0.0f,
                          *(float *)(glow_widget_ptr + 0x234));
      break;
    case 1:
      *(float *)(particle + 0x28) =
        ((float)(int)index / (int)count) * *(float *)(glow_widget_ptr + 0x234);
      break;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x3b1, 1);
      system_exit(-1);
      break;
    }

    *(float *)(particle + 8) = random_real_range(
      random_math_get_local_seed_address(), 0.0f, 6.2831855f);
  }

  return particle;
}

/* get_particle_world_position (0x1339a0 / objects.obj / glow.c) — place a
 * glow particle on the spline through the glow's markers at the particle's
 * parameter t (+0x28), then offset it around the spline by an angle.
 *
 * Glow widget fields used: marker count (short +0x4); marker matrices at
 * +0x40 + i*0x6c (forward +0x44, up +0x5c, position +0x68); marker order
 * (short[] +0x22a); marker times (float[] +0x238). Particle fields: marker
 * index (short +0x2), initial angle (+0x8), orbit distance (+0x1c),
 * t (+0x28), position (+0x2c).
 *
 * 1. Find the marker span containing t (assert glow.c 0x437), pin it, and
 *    store it at particle+0x2; assert more than 1 marker (0x43b).
 * 2. Build 4 control points (positions, ups, sides) and 4 knots:
 *    - 2 markers: the ends, plus points at 1/4 and 3/4 between them.
 *    - 3 markers: the ends plus the middle marker; the missing point is the
 *      midpoint of the span the particle is in.
 *    - more: 4 consecutive markers around the span (assert 0x49c), with
 *      sides = cross(up, forward) per marker.
 * 3. Spline the position into particle+0x2c, and the up and side vectors,
 *    then add (side*cos(a) + up*sin(a)) * distance, a = rate*t + angle.
 *
 * Faithful 2276 bugs (the binary wins over the PAL reference):
 *  - Every interpolated point's z adds the start point's y (FADD [EBP-0x4c]
 *    at 0x133cf6 etc.), as in point_from_parametric_line.  PAL has the same
 *    bug; the rest below differ from PAL.
 *  - The 2- and 3-marker paths never compute sides[] (no cross products
 *    between 0x133c0e and 0x133f66), so the third spline reads uninitialized
 *    stack and so does the resulting side offset.
 *  - 3 markers, particle in span 1: only knots[2] is stored, set to the
 *    midpoint of knots[0] and marker time 1 (the case computes the product
 *    and JMPs to the shared FADD knots[0]; FSTP [EBP-0x8] tail at 0x133f63);
 *    knots[1] stays uninitialized.  PAL instead stores the midpoint in
 *    knots[1] and marker time 1 in knots[2].
 *  - PAL's 2- and 3-marker paths call cross_product3d for sides[]; 2276 has
 *    no such call (its only calls are sin/cos and the three splines).
 *
 * glow_widget arrives in EAX (MOV ESI,EAX at 0x1339ab); particle_ptr and
 * rotation_rate are cdecl stack args. */
void get_particle_world_position(int glow_widget, int particle_ptr,
                                 float rotation_rate)
{
  vector3_t sides[4];
  vector3_t up;
  real_vector3d ups[4];
  vector3_t positions[4];
  vector3_t side;
  float knots[4];
  float angle;
  float sin_angle;
  float cos_angle;
  short marker_index;
  short first_marker_index;
  short last_marker_index;
  short index;
  int marker;
  glow_datum *glow;
  glow_particle *particle;

  glow = (glow_datum *)glow_widget;
  particle = (glow_particle *)particle_ptr;
  for (marker_index = 0; marker_index < glow->number_of_markers - 1;
       marker_index++) {
    if (glow->marker_time_index[marker_index] <= particle->t &&
        glow->marker_time_index[marker_index + 1] > particle->t)
      break;
  }
  if (!(marker_index < glow->number_of_markers - 1)) {
    display_assert("marker_index<glow->number_of_markers-1",
                   "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x437, 1);
    system_exit(-1);
  }
  particle->parent_marker_index =
    marker_index < 0 ? 0 :
                       (marker_index > glow->number_of_markers - 1 ?
                          glow->number_of_markers - 1 :
                          marker_index);

  if (!(glow->number_of_markers > 1)) {
    display_assert("glow->number_of_markers > 1",
                   "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x43b, 1);
    system_exit(-1);
  }

  switch (glow->number_of_markers) {
  case 2:
    positions[0] = glow->markers[0].matrix.position;
    positions[3] = glow->markers[1].matrix.position;
    ups[0] = glow->markers[0].matrix.up;
    ups[3] = glow->markers[1].matrix.up;
    *(int *)&knots[0] = *(int *)&glow->marker_time_index[0];
    *(int *)&knots[3] = *(int *)&glow->marker_time_index[1];

    positions[1].x = (positions[3].x - positions[0].x) * 0.25f + positions[0].x;
    positions[1].y = (positions[3].y - positions[0].y) * 0.25f + positions[0].y;
    positions[1].z = (positions[3].z - positions[0].z) * 0.25f + positions[0].y;
    positions[2].x = (positions[3].x - positions[0].x) * 0.75f + positions[0].x;
    positions[2].y = (positions[3].y - positions[0].y) * 0.75f + positions[0].y;
    positions[2].z = (positions[3].z - positions[0].z) * 0.75f + positions[0].y;

    ups[1].i = (ups[3].i - ups[0].i) * 0.25f + ups[0].i;
    ups[1].j = (ups[3].j - ups[0].j) * 0.25f + ups[0].j;
    ups[1].k = (ups[3].k - ups[0].k) * 0.25f + ups[0].j;
    ups[2].i = (ups[3].i - ups[0].i) * 0.75f + ups[0].i;
    ups[2].j = (ups[3].j - ups[0].j) * 0.75f + ups[0].j;
    ups[2].k = (ups[3].k - ups[0].k) * 0.75f + ups[0].j;

    knots[1] = (knots[3] - knots[0]) * 0.25f + knots[0];
    knots[2] = (knots[3] - knots[0]) * 0.75f + knots[0];
    break;

  case 3:
    positions[0] = glow->markers[0].matrix.position;
    positions[3] = glow->markers[2].matrix.position;
    ups[0] = glow->markers[0].matrix.up;
    ups[3] = glow->markers[2].matrix.up;
    *(int *)&knots[0] = *(int *)&glow->marker_time_index[0];
    *(int *)&knots[3] = *(int *)&glow->marker_time_index[2];

    switch (particle->parent_marker_index) {
    case 0:
      positions[1] = glow->markers[1].matrix.position;
      ups[1] = glow->markers[1].matrix.up;
      *(int *)&knots[1] = *(int *)&glow->marker_time_index[1];

      positions[2].x =
        (positions[3].x - positions[1].x) * 0.5f + positions[1].x;
      positions[2].y =
        (positions[3].y - positions[1].y) * 0.5f + positions[1].y;
      positions[2].z =
        (positions[3].z - positions[1].z) * 0.5f + positions[1].y;

      ups[2].i = (ups[3].i - ups[1].i) * 0.5f + ups[1].i;
      ups[2].j = (ups[3].j - ups[1].j) * 0.5f + ups[1].j;
      ups[2].k = (ups[3].k - ups[1].k) * 0.5f + ups[1].j;

      knots[2] = (knots[3] - knots[1]) * 0.5f + knots[1];
      break;

    case 1:
      positions[2] = glow->markers[1].matrix.position;
      ups[2] = glow->markers[1].matrix.up;

      positions[1].x =
        (positions[2].x - positions[0].x) * 0.5f + positions[0].x;
      positions[1].y =
        (positions[2].y - positions[0].y) * 0.5f + positions[0].y;
      positions[1].z =
        (positions[2].z - positions[0].z) * 0.5f + positions[0].y;

      ups[1].i = (ups[2].i - ups[0].i) * 0.5f + ups[0].i;
      ups[1].j = (ups[2].j - ups[0].j) * 0.5f + ups[0].j;
      ups[1].k = (ups[2].k - ups[0].k) * 0.5f + ups[0].j;

      knots[2] = (glow->marker_time_index[1] - knots[0]) * 0.5f + knots[0];
      break;
    }
    break;

  default:
    for (marker_index = 0; marker_index < glow->number_of_markers - 1;
         marker_index++) {
      if (glow->marker_time_index[marker_index] <= particle->t &&
          particle->t <= glow->marker_time_index[marker_index + 1])
        break;
    }
    if (!(marker_index < glow->number_of_markers - 1)) {
      display_assert("marker_index<glow->number_of_markers-1",
                     "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x49c, 1);
      system_exit(-1);
    }
    marker_index = marker_index < 0 ?
                     0 :
                     (marker_index > glow->number_of_markers - 1 ?
                        glow->number_of_markers - 1 :
                        marker_index);

    first_marker_index = marker_index;
    last_marker_index = (short)(marker_index + 1);
    while (last_marker_index - first_marker_index + 1 < 4) {
      if (first_marker_index > 0)
        first_marker_index--;
      if (last_marker_index < glow->number_of_markers - 1)
        last_marker_index++;
    }

    *(int *)&knots[0] = *(int *)&glow->marker_time_index[first_marker_index];
    *(int *)&knots[1] =
      *(int *)&glow->marker_time_index[first_marker_index + 1];
    *(int *)&knots[2] =
      *(int *)&glow->marker_time_index[first_marker_index + 2];
    *(int *)&knots[3] =
      *(int *)&glow->marker_time_index[first_marker_index + 3];

    for (index = 0; index < 4; index++) {
      /* 2276 forms glow + order*0x6c and folds markers' +0x08 into each
       * displacement (+0x44/+0x5c/+0x68); &glow->markers[order] adds the +8
       * to the pointer instead and costs ~4pp VC71. So this is a row base
       * over glow_datum, read through markers[0]. */
      marker =
        glow_widget + glow->marker_order[first_marker_index + index] * 0x6c;
      positions[index] =
        ((glow_datum *)marker)->markers[0].matrix.position;
      ups[index] = ((glow_datum *)marker)->markers[0].matrix.up;
      /* sides[index] = cross(matrix_up, matrix_forward) */
      sides[index].x = ((glow_datum *)marker)->markers[0].matrix.up.j *
                         ((glow_datum *)marker)->markers[0].matrix.forward.k -
                       ((glow_datum *)marker)->markers[0].matrix.up.k *
                         ((glow_datum *)marker)->markers[0].matrix.forward.j;
      sides[index].y = ((glow_datum *)marker)->markers[0].matrix.up.k *
                         ((glow_datum *)marker)->markers[0].matrix.forward.i -
                       ((glow_datum *)marker)->markers[0].matrix.up.i *
                         ((glow_datum *)marker)->markers[0].matrix.forward.k;
      sides[index].z = ((glow_datum *)marker)->markers[0].matrix.up.i *
                         ((glow_datum *)marker)->markers[0].matrix.forward.j -
                       ((glow_datum *)marker)->markers[0].matrix.up.j *
                         ((glow_datum *)marker)->markers[0].matrix.forward.i;
    }
    break;
  }

  nonuniform_cubic_spline_vector3d(
    particle->position, (float *)&positions[0], (float *)&positions[1],
    (float *)&positions[2], (float *)&positions[3], knots[0], knots[1],
    knots[2], knots[3], particle->t);
  nonuniform_cubic_spline_vector3d(
    (float *)&up, (float *)&ups[0], (float *)&ups[1], (float *)&ups[2],
    (float *)&ups[3], knots[0], knots[1], knots[2], knots[3], particle->t);
  nonuniform_cubic_spline_vector3d(
    (float *)&side, (float *)&sides[0], (float *)&sides[1], (float *)&sides[2],
    (float *)&sides[3], knots[0], knots[1], knots[2], knots[3], particle->t);

  angle = rotation_rate * particle->t + particle->initial_angle;
  sin_angle = x87_fsin(angle);
  cos_angle = x87_fcos(angle);
  particle->position[0] =
    (side.x * cos_angle + up.x * sin_angle) * particle->distance_to_object +
    particle->position[0];
  particle->position[1] =
    (side.y * cos_angle + up.y * sin_angle) * particle->distance_to_object +
    particle->position[1];
  particle->position[2] =
    (side.z * cos_angle + up.z * sin_angle) * particle->distance_to_object +
    particle->position[2];
}

/* glow_normal_particle_update_position — advance a glow particle's phase
 * animation by one frame and recompute its world position.  If the glow
 * definition binds an object function, resample it and remap into the scale
 * output.  Then step the phase counter by +/-delta (direction per particle
 * flags bit0), wrapping against the period: boundary mode 0 reflects and flips
 * direction, mode 1 plain-wraps. Ends by recomputing the particle's world
 * position.
 *
 * ABI: particle_ptr in ESI, glow_widget_ptr in EDI (register args); the
 * remaining three are cdecl stack args.
 *
 * DO NOT "simplify" the two switch statements below into if/else-if chains.
 * The original dispatches boundary_effect with MOVSX/SUB EAX,0/JZ/DEC EAX/JZ
 * (0x1340f7 and 0x1341dd) -- a switch lowering.  An if-chain emits CMP/JE and
 * lays the case bodies out in the opposite order, which cost ~54 percentage
 * points of VC71 match (84.1% -> 30.1%) when it was tried. */
void glow_normal_particle_update_position(int particle_ptr, int glow_widget_ptr,
                                          int object_handle, float delta,
                                          float ratio)
{
  void *glowdef;
  short function_index;
  unsigned int flags;
  float function_value;

  glowdef = tag_get(TAG_GROUP_GLW, *(int *)(glow_widget_ptr + 0x224));

  function_index = *(short *)((char *)glowdef + 0x80);
  if (function_index != -1) {
    if (!object_get_function_value(object_handle, function_index,
                                   &function_value))
      function_value = 0.0f;
    /* Evaluation order is binary-confirmed: the inner (0x90-0x8c) term is
     * loaded FIRST (FLD [EBX+0x90] at 0x1340b6), then the outer (0x88-0x84)
     * scale, then FMULP.  Writing the outer factor first inverts the FPU
     * load order. */
    *(float *)(particle_ptr + 0x1c) = ((*(float *)((char *)glowdef + 0x90) -
                                        *(float *)((char *)glowdef + 0x8c)) *
                                         function_value +
                                       *(float *)((char *)glowdef + 0x8c)) *
                                        (*(float *)((char *)glowdef + 0x88) -
                                         *(float *)((char *)glowdef + 0x84)) +
                                      *(float *)((char *)glowdef + 0x84);
  }

  flags = *(unsigned int *)(particle_ptr + 0x54);

  if ((flags & FLAG(_glow_particle_moving_backwards_bit)) != 0) {
    /* reverse phase: step down.  FLD [ESI+0x28]; FSUB [EBP+0xc] at 0x1340ee
     * -> phase is the left operand here. */
    *(float *)(particle_ptr + 0x28) = *(float *)(particle_ptr + 0x28) - delta;
    /* MOVSX EAX,word [EBX+0x22]; SUB EAX,0; JZ; DEC EAX; JZ (0x1340f7) --
     * a switch dispatch chain, not a compare chain. */
    switch (*(short *)((char *)glowdef + 0x22)) {
    case 0:
      /* 0x134180: a single FCOMP guard, then the loop is entered by JMP into
       * its body -> if + do/while (one pre-test). */
      if (*(float *)(particle_ptr + 0x28) < 0.0f) {
        do {
          *(float *)(particle_ptr + 0x28) =
            *(float *)(glow_widget_ptr + 0x234) +
            *(float *)(particle_ptr + 0x28);
        } while (*(float *)(particle_ptr + 0x28) < 0.0f);
        flags &= ~(unsigned int)FLAG(_glow_particle_moving_backwards_bit);
        *(unsigned int *)(particle_ptr + 0x54) = flags;
        *(float *)(particle_ptr + 0x28) =
          *(float *)(glow_widget_ptr + 0x234) - *(float *)(particle_ptr + 0x28);
      }
      break;
    case 1:
      /* 0x13413d: one pre-test then a rotated do/while -> plain while. */
      while (*(float *)(particle_ptr + 0x28) < 0.0f) {
        *(float *)(particle_ptr + 0x28) =
          *(float *)(glow_widget_ptr + 0x234) + *(float *)(particle_ptr + 0x28);
      }
      break;
    default:
      display_assert("glow effect received illegal boundary effect",
                     "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x320, 1);
      system_exit(-1);
    }
  } else {
    /* forward phase: step up.  FLD [EBP+0xc]; FADD [ESI+0x28] at 0x1341d4 ->
     * delta is the left operand here (opposite of the reverse branch). */
    *(float *)(particle_ptr + 0x28) = delta + *(float *)(particle_ptr + 0x28);
    switch (*(short *)((char *)glowdef + 0x22)) {
    case 0:
      /* 0x134232 FCOM (non-popping) guard followed by a SECOND FCOMP test at
       * 0x134243 whose false edge lands on the reflect block (0x13426c) --
       * i.e. an if guard wrapping a plain while, two distinct pre-tests.
       * Bound sense is TEST AH,0x41 / JNZ, which is `>` on the phase (see
       * lift-learnings section 38); writing `period < phase` yields the
       * TEST AH,0x5 / JP form instead. */
      if (*(float *)(particle_ptr + 0x28) >
          *(float *)(glow_widget_ptr + 0x234)) {
        while (*(float *)(particle_ptr + 0x28) >
               *(float *)(glow_widget_ptr + 0x234)) {
          *(float *)(particle_ptr + 0x28) = *(float *)(particle_ptr + 0x28) -
                                            *(float *)(glow_widget_ptr + 0x234);
        }
        *(float *)(particle_ptr + 0x28) =
          *(float *)(glow_widget_ptr + 0x234) - *(float *)(particle_ptr + 0x28);
        flags |= FLAG(_glow_particle_moving_backwards_bit);
        *(unsigned int *)(particle_ptr + 0x54) = flags;
      }
      break;
    case 1:
      while (*(float *)(particle_ptr + 0x28) >
             *(float *)(glow_widget_ptr + 0x234)) {
        *(float *)(particle_ptr + 0x28) =
          *(float *)(particle_ptr + 0x28) - *(float *)(glow_widget_ptr + 0x234);
      }
      break;
    default:
      display_assert("glow effect received illegal boundary effect",
                     "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 0x33d, 1);
      system_exit(-1);
    }
  }

  get_particle_world_position(glow_widget_ptr, particle_ptr, ratio);
}

/* glow_trailing_particle_new (0x134350 / objects.obj / glow.c)
 *
 * Allocates a new trailing-particle datum for a glow widget and initializes its
 * position, velocity distribution, size, lifetime and color-lerp state from the
 * 'glw!' tag definition.  The glow-widget pointer arrives in EBX (@<ebx>).
 *
 * Distribution mode is a signed word at glowtag+0x26:
 *   0 = fixed direction  (velocity dir from tag_data+0x38..0x40 cleared to +Z
 * unit) 1 = per-variant table (glow_widget+0x5c + variant*0x6c, variant =
 * particle+2) 2 = random spherical  (three [-1,1] randoms then normalized) Any
 * other value asserts and exits.
 *
 * Returns the new particle pointer (int), or 0 if the pool is full.
 */
int glow_trailing_particle_new(int glow_widget /* @<ebx> */)
{
  int glow_tag; /* pvVar5 — 'glw!' tag definition block */
  int particle;
  int idx;
  int variant;
  int base;
  int16_t dist; /* distribution mode at glow_tag+0x26 (signed word) */
  float fmin;
  float fmax;
  float scale;
  float radius;
  float t;

  glow_tag = (int)tag_get(TAG_GROUP_GLW, *(int *)(glow_widget + 0x224));
  particle = 0;

  idx = data_new_at_index(GLOW_PARTICLE_DATA);
  if (idx != -1) {
    particle = (int)datum_get(GLOW_PARTICLE_DATA, idx);
    *(int *)(particle + 4) = idx;

    /* Initial world position. */
    if (*(int16_t *)(glow_widget + 4) > 1) {
      fmax = *(float *)(glow_widget + 0x234) * *(float *)(glow_tag + 0x10c);
      fmin = *(float *)(glow_widget + 0x234) * *(float *)(glow_tag + 0x108);
      *(float *)(particle + 0x28) = random_real_range(
        random_math_get_local_seed_address(), fmin, fmax);
      get_particle_world_position(glow_widget, particle, 0.0f);
    } else {
      *(uint32_t *)(particle + 0x2c) = *(uint32_t *)(glow_widget + 0x68);
      *(uint32_t *)(particle + 0x30) = *(uint32_t *)(glow_widget + 0x6c);
      *(uint32_t *)(particle + 0x34) = *(uint32_t *)(glow_widget + 0x70);
    }

    /* Velocity direction by distribution mode. */
    dist = *(int16_t *)(glow_tag + 0x26);
    switch (dist) {
    case 0:
      *(uint32_t *)(particle + 0x38) = 0;
      *(uint32_t *)(particle + 0x3c) = 0;
      *(float *)(particle + 0x40) = 1.0f;
      break;
    case 1:
      variant = *(int16_t *)(particle + 2);
      base = glow_widget + variant * 0x6c + 0x5c;
      *(uint32_t *)(particle + 0x38) = *(uint32_t *)(base);
      *(uint32_t *)(particle + 0x3c) = *(uint32_t *)(base + 4);
      *(uint32_t *)(particle + 0x40) = *(uint32_t *)(base + 8);
      break;
    case 2:
      *(float *)(particle + 0x38) = random_real_range(
        random_math_get_local_seed_address(), -1.0f, 1.0f);
      *(float *)(particle + 0x3c) = random_real_range(
        random_math_get_local_seed_address(), -1.0f, 1.0f);
      *(float *)(particle + 0x40) = random_real_range(
        random_math_get_local_seed_address(), -1.0f, 1.0f);
      normalize3d((real_vector3d *)(particle + 0x38));
      break;
    default:
      display_assert("unknown trailing particle distribution?",
                     "c:\\halo\\SOURCE\\objects\\widgets\\glow.c", 996, 1);
      system_exit(-1);
    }

    /* Scale the velocity direction by tag magnitude. */
    scale = *(float *)(glow_tag + 0x104) * REAL_ONE_THIRTIETH_POOL;
    *(float *)(particle + 0x38) = *(float *)(particle + 0x38) * scale;
    *(float *)(particle + 0x3c) = *(float *)(particle + 0x3c) * scale;
    *(float *)(particle + 0x40) = *(float *)(particle + 0x40) * scale;

    /* Particle size (tag radius range / widget divisor). */
    radius = random_real_range(random_math_get_local_seed_address(),
                               *(float *)(glow_tag + 0xa0),
                               *(float *)(glow_tag + 0xa4));
    *(float *)(particle + 0x20) =
      radius / (float)*(int16_t *)(glow_widget + 0x228);

    /* Lifetime in ticks. */
    *(int16_t *)(particle + 0x52) =
      (int16_t)(int)(*(float *)(glow_tag + 0x100) * TICKS_PER_SECOND);

    /* Color lerp between tag min (0xb8..0xc0) and max (0xc8..0xd0). */
    t = random_real_range(random_math_get_local_seed_address(), 0.0f,
                          1.0f);
    *(float *)(particle + 0xc) = 1.0f;
    *(float *)(particle + 0x10) =
      *(float *)(glow_tag + 0xb8) +
      t * (*(float *)(glow_tag + 0xc8) - *(float *)(glow_tag + 0xb8));
    *(float *)(particle + 0x14) =
      *(float *)(glow_tag + 0xbc) +
      t * (*(float *)(glow_tag + 0xcc) - *(float *)(glow_tag + 0xbc));
    *(uint32_t *)(particle + 0x54) |= FLAG(_glow_particle_trailing_bit);
    *(float *)(particle + 0x18) =
      *(float *)(glow_tag + 0xc0) +
      t * (*(float *)(glow_tag + 0xd0) - *(float *)(glow_tag + 0xc0));
  }

  return particle;
}
