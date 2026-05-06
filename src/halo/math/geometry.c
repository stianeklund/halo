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
  static float clip_buffer[0x200 * 2];
  int16_t out_count;
  uint32_t mask;
  bool any_above;
  bool any_below;
  bool previous_inside;
  int16_t i;
  int byte_size;
  float *previous_point;

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
  previous_inside =
    *(float *)0x2533c0 <=
    (previous_point[0] * line[0] + previous_point[1] * line[1]) - line[2];

  any_above = false;
  any_below = false;
  out_count = 0;
  mask = 0;

  if (count < 1) {
    goto zero_result;
  }

  for (i = 0; i < count; ++i) {
    float *current_point = points + (int)i * 2;
    float distance =
      (line[0] * current_point[0] + current_point[1] * line[1]) - line[2];
    bool current_inside = *(float *)0x2533c0 <= distance;

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

      {
        float t;
        float clamped_t;
        int out_idx;

        t = -((line[0] * current_point[0] + current_point[1] * line[1]) -
              line[2]) /
            ((previous_point[1] - current_point[1]) * line[1] +
             (previous_point[0] - current_point[0]) * line[0]);

        clamped_t = *(float *)0x2533c0;
        if (*(float *)0x2533c0 <= t) {
          clamped_t = t;
          if (*(float *)0x2533c8 < t) {
            clamped_t = *(float *)0x2533c8;
          }
        }

        out_points[(int)out_count * 2] =
          clamped_t * (previous_point[0] - current_point[0]) + current_point[0];
        mask |= (uint32_t)1 << ((uint8_t)out_count & 0x1f);
        out_count += 1;
        out_points[((int)out_count - 1) * 2 + 1] =
          clamped_t * (previous_point[1] - current_point[1]) + current_point[1];

        if (out_count != 1) {
          out_idx = (int)out_count;
          if ((fabsf(out_points[out_idx * 2 - 2] - out_points[0]) < epsilon &&
               fabsf(out_points[out_idx * 2 - 1] - out_points[1]) < epsilon) ||
              (fabsf(out_points[out_idx * 2 - 2] -
                     out_points[out_idx * 2 - 4]) < epsilon &&
               fabsf(out_points[out_idx * 2 - 1] -
                     out_points[out_idx * 2 - 3]) < epsilon)) {
            out_count -= 1;
          }
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
        int out_idx = (int)out_count;
        if ((fabsf(out_points[out_idx * 2 - 2] - out_points[0]) < epsilon &&
             fabsf(out_points[out_idx * 2 - 1] - out_points[1]) < epsilon) ||
            (fabsf(out_points[out_idx * 2 - 2] - out_points[out_idx * 2 - 4]) <
               epsilon &&
             fabsf(out_points[out_idx * 2 - 1] - out_points[out_idx * 2 - 3]) <
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
