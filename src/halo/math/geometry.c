/* MSVC 7.1 FABS intrinsic: declared+pragma here so fabs() inlines to a single
 * FABS instruction instead of a CRT call (matches reference behaviour). */
extern double __cdecl fabs(double);
#if defined(_MSC_VER) && !defined(__clang__)
#pragma intrinsic(fabs)
#endif

/* 0x994d0 — Negate all four components of a plane */
void plane_negate(float *plane_in, float *plane_out)
{
  plane_out[0] = -plane_in[0];
  plane_out[1] = -plane_in[1];
  plane_out[2] = -plane_in[2];
  plane_out[3] = -plane_in[3];
}

/* Sutherland-Hodgman 2D polygon clip against a line.
 * Source: c:\halo\SOURCE\math\geometry.c */
int16_t FUN_00106510(int16_t count, float *points, float *line,
                     int16_t max_count, float *out_points,
                     uint32_t *out_bitmask, uint8_t *changed, float epsilon)
{
  /* _chkstk(0x1014): clip_buffer is a 512-float-pair local, not static */
  float clip_buffer[0x200 * 2];
  int16_t out_count;
  uint32_t mask;
  bool any_above;
  bool any_below;
  bool previous_inside;
  bool current_inside;
  int16_t i;
  int byte_size;
  float *previous_point;
  float *current_point;
  float distance;
  float t;
  float clamped_t;
  float dx;
  float dy;
  int out_idx;

  /* Init zero-valued locals up front: MSVC schedules these between the
   * callee-saved pushes and the count<3 assert (0x13-0x21). */
  out_count = 0;
  mask = 0;
  any_above = false;
  any_below = false;

  if (count < 3) {
    display_assert("count>=NUMBER_OF_VERTICES_PER_TRIANGLE",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x546, true);
    system_exit(-1);
  }

  if (changed != NULL) {
    *changed = 0;
  }

  if (points == out_points) {
    if (count > 0x200) {
      display_assert("count<=CLIP_BUFFER_SIZE",
                     "c:\\halo\\SOURCE\\math\\geometry.c", 0x54d, true);
      system_exit(-1);
    }
    csmemcpy(clip_buffer, points, (int)count << 3);
    points = clip_buffer;
  }

  byte_size = (int)count * 8;

  previous_point = points + (int)count * 2 - 2;
  /* Eager-init/conditional-clear idiom matches MSVC codegen at 0x1065ce. */
  previous_inside = true;
  if ((previous_point[0] * line[0] + previous_point[1] * line[1]) - line[2] <
      *(float *)0x2533c0) {
    previous_inside = false;
  }

  if (count < 1) {
    goto zero_result;
  }

  for (i = 0; i < count; ++i) {
    current_point = points + (int)i * 2;
    /* Eager-init/conditional-clear matches MSVC at 0x10660a/0x10662a; FCOM
     * leaves distance on the FPU stack for reuse by the epsilon tests. */
    current_inside = true;
    distance =
      (line[0] * current_point[0] + current_point[1] * line[1]) - line[2];
    if (distance < *(float *)0x2533c0) {
      current_inside = false;
    }

    if (distance > epsilon) {
      any_above = true;
    } else if (distance < -epsilon) {
      any_below = true;
    }

    if (current_inside != previous_inside) {
      if (out_count == max_count) {
        goto overflow;
      }

      if (changed != NULL) {
        *changed = 1;
      }

      /* Hoist deltas: ensures both dx and dy stay on the FPU stack from
       * 0x167-0x16e through 0x1d1 (FLD ST(2)/ST(3) peeks for the
       * denominator and X/Y output writes). */
      dx = previous_point[0] - current_point[0];
      dy = previous_point[1] - current_point[1];
      t =
        -((line[0] * current_point[0] + current_point[1] * line[1]) - line[2]) /
        (dy * line[1] + dx * line[0]);

      /* Clamp: matches the FCOM-then-conditional-FLD pattern at
       * 0x10669f-0x1066cb. Note the branches REPLACE the FPU top-of-stack
       * (FSTP+FLD), so clamped_t is always whichever value ended up on the
       * stack after the conditionals. */
      if (t < *(float *)0x2533c0) {
        clamped_t = *(float *)0x2533c0;
      } else if (*(float *)0x2533c8 < t) {
        clamped_t = *(float *)0x2533c8;
      } else {
        clamped_t = t;
      }

      out_points[(int)out_count * 2] = clamped_t * dx + current_point[0];
      mask |= (uint32_t)1 << ((uint8_t)out_count & 0x1f);
      out_count += 1;
      out_points[((int)out_count - 1) * 2 + 1] = clamped_t * dy + current_point[1];

      if (out_count != 1) {
        out_idx = (int)out_count;
        if (((float)fabs(out_points[out_idx * 2 - 2] - out_points[0]) < epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] - out_points[1]) < epsilon) ||
            ((float)fabs(out_points[out_idx * 2 - 2] - out_points[out_idx * 2 - 4]) <
               epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] - out_points[out_idx * 2 - 3]) <
               epsilon)) {
          out_count -= 1;
        }
      }
    }

    if (current_inside) {
      if (out_count == max_count) {
        goto overflow;
      }

      out_points[(int)out_count * 2] = current_point[0];
      out_points[(int)out_count * 2 + 1] = current_point[1];

      if (out_bitmask == NULL ||
          ((uint32_t)1 << ((uint8_t)i & 0x1f) & *out_bitmask) == 0) {
        mask &= ~((uint32_t)1 << ((uint8_t)out_count & 0x1f));
      } else {
        mask |= (uint32_t)1 << ((uint8_t)out_count & 0x1f);
      }
      out_count += 1;

      if (out_count != 1) {
        out_idx = (int)out_count;
        if (((float)fabs(out_points[out_idx * 2 - 2] - out_points[0]) < epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] - out_points[1]) < epsilon) ||
            ((float)fabs(out_points[out_idx * 2 - 2] - out_points[out_idx * 2 - 4]) <
               epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] - out_points[out_idx * 2 - 3]) <
               epsilon)) {
          out_count -= 1;
        }
      }
    }

    previous_point = current_point;
    previous_inside = current_inside;
  }

  if (out_count == -1) {
    goto overflow;
  }

  if (out_count < 3) {
  zero_result:
    out_count = 0;
  }

  if (any_above) {
    if (!any_below) {
      if (count < 0 || count > max_count) {
        display_assert("count>=0 && count<=maximum_count",
                       "c:\\halo\\SOURCE\\math\\geometry.c", 0x5a1, true);
        system_exit(-1);
      }
      csmemcpy(out_points, points, byte_size);
      out_count = count;
    }
  } else {
    out_count = 0;
  }

  goto done;

overflow:
  out_count = -1;
  if (count < 0 || count > max_count) {
    display_assert("count>=0 && count<=maximum_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x5a8, true);
    system_exit(-1);
  }
  csmemcpy(out_points, points, byte_size);

done:
  if (out_bitmask != NULL) {
    *out_bitmask = mask;
  }

  return out_count;
}
