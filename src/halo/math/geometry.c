/* MSVC 7.1 FABS intrinsic: declared+pragma here so fabs() inlines to a single
 * FABS instruction instead of a CRT call. */
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

/* 0x106390 — Perimeter of a closed 2D polygon.
 * vertices is a flat array of (x,y) pairs; vertex_count is the vertex count.
 * Seeds the accumulator with the closing edge dist(vertex[0], vertex[last]),
 * then walks the vertex[i] -> vertex[i+1] edges (vertex_count-1 of them).
 * Term ordering under each sqrt matches the original codegen: x-term first for
 * the closing edge, y-term first inside the loop. Source:
 * c:\halo\SOURCE\math\geometry.c */
float convex_hull2d_perimeter(int16_t vertex_count, float *vertices)
{
  float perimeter;
  uint16_t remaining;

  perimeter = sqrtf((vertices[0] - vertices[vertex_count * 2 + -2]) *
                      (vertices[0] - vertices[vertex_count * 2 + -2]) +
                    (vertices[1] - vertices[vertex_count * 2 + -1]) *
                      (vertices[1] - vertices[vertex_count * 2 + -1]));

  if (1 < vertex_count) {
    remaining = (uint16_t)(vertex_count - 1);
    do {
      remaining = remaining - 1;
      perimeter =
        sqrtf((vertices[3] - vertices[1]) * (vertices[3] - vertices[1]) +
              (vertices[2] - vertices[0]) * (vertices[2] - vertices[0])) +
        perimeter;
      vertices = vertices + 2;
    } while (remaining != 0);
  }
  return perimeter;
}

/* 0x1063f0 — Liang-Barsky ray/segment clip against a convex 2D polygon.
 * Walks each polygon edge (index i -> next, with wrap), classifies the edge
 * as entering or leaving by the sign of the edge/ray-direction cross product,
 * and tightens the parametric interval [tmin,tmax]. Rejects (returns 0) when
 * the ray runs parallel to an edge on its outside, or when the interval
 * becomes empty (tmax < tmin). On acceptance writes the clipped interval.
 * Source: c:\halo\SOURCE\math\geometry.c */
bool convex_hull2d_test_vector(int16_t num_verts, float *polygon2d,
                               float *ray_origin, float *ray_dir,
                               float *out_tmin, float *out_tmax)
{
  float tmin;
  float tmax;
  float dx;
  float dy;
  float denom;
  float num;
  float t;
  float new_tmin;
  float new_tmax;
  float *pts_iy;
  int16_t i;
  int cur;
  int next;

  tmin = -3.4028235e38f; /* -FLT_MAX */
  tmax = 3.4028235e38f; /* +FLT_MAX */

  if (num_verts > 0) {
    i = 0;
    do {
      cur = (int)i;
      next = (((int)num_verts <= i + 1) - 1) & (i + 1); /* wrap to 0 */

      pts_iy = polygon2d + cur * 2 + 1; /* &pts[i].y */
      dx = polygon2d[next * 2] - polygon2d[cur * 2];
      dy = polygon2d[next * 2 + 1] - *pts_iy;

      denom = dy * ray_dir[0] - dx * ray_dir[1];
      num = (ray_origin[1] - *pts_iy) * dx -
            (ray_origin[0] - polygon2d[cur * 2]) * dy;

      if (fabs(denom) < *(double *)0x2533d0) {
        /* ray parallel to this edge: reject if strictly outside */
        if (num < *(float *)0x253f44) {
          return 0;
        }
      } else {
        t = num / denom;
        if (denom <= *(float *)0x2533c0) {
          /* entering half-plane -> candidate tmax */
          new_tmax = t;
          new_tmin = tmin;
          if (tmax <= t) {
            new_tmax = tmax;
            new_tmin = tmin;
          }
        } else {
          /* leaving half-plane -> candidate tmin */
          new_tmax = tmax;
          new_tmin = t;
          if (t <= tmin) {
            new_tmax = tmax;
            new_tmin = tmin;
          }
        }
        tmin = new_tmin;
        tmax = new_tmax;
        if (tmax < tmin) {
          return 0;
        }
      }
      i = i + 1;
    } while (i < num_verts);
  }

  if (out_tmin != NULL) {
    *out_tmin = tmin;
  }
  if (out_tmax != NULL) {
    *out_tmax = tmax;
  }
  return 1;
}

/* Sutherland-Hodgman 2D polygon clip against a line.
 * Source: c:\halo\SOURCE\math\geometry.c */
int16_t convex_polygon2d_clip_to_plane(int16_t count, float *points,
                                       float *line, int16_t max_count,
                                       float *out_points, uint32_t *out_bitmask,
                                       uint8_t *changed, float epsilon)
{
  /* _chkstk(0x1014): clip_buffer is a 512-float-pair local, not static. */
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
  previous_inside =
    *(float *)0x2533c0 <=
    (previous_point[0] * line[0] + previous_point[1] * line[1]) - line[2];

  if (count < 1) {
    goto zero_result;
  }

  for (i = 0; i < count; ++i) {
    current_point = points + (int)i * 2;
    distance =
      (line[0] * current_point[0] + current_point[1] * line[1]) - line[2];
    current_inside = *(float *)0x2533c0 <= distance;

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

      dx = previous_point[0] - current_point[0];
      dy = previous_point[1] - current_point[1];
      t =
        -((line[0] * current_point[0] + current_point[1] * line[1]) - line[2]) /
        (dy * line[1] + dx * line[0]);

      clamped_t = *(float *)0x2533c0;
      if (*(float *)0x2533c0 <= t) {
        clamped_t = t;
        if (*(float *)0x2533c8 < t) {
          clamped_t = *(float *)0x2533c8;
        }
      }

      out_points[(int)out_count * 2] = clamped_t * dx + current_point[0];
      mask |= (uint32_t)1 << ((uint8_t)out_count & 0x1f);
      out_count += 1;
      out_points[((int)out_count - 1) * 2 + 1] =
        clamped_t * dy + current_point[1];

      if (out_count != 1) {
        out_idx = (int)out_count;
        if (((float)fabs(out_points[out_idx * 2 - 2] - out_points[0]) <
               epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] - out_points[1]) <
               epsilon) ||
            ((float)fabs(out_points[out_idx * 2 - 2] -
                         out_points[out_idx * 2 - 4]) < epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] -
                         out_points[out_idx * 2 - 3]) < epsilon)) {
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
        if (((float)fabs(out_points[out_idx * 2 - 2] - out_points[0]) <
               epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] - out_points[1]) <
               epsilon) ||
            ((float)fabs(out_points[out_idx * 2 - 2] -
                         out_points[out_idx * 2 - 4]) < epsilon &&
             (float)fabs(out_points[out_idx * 2 - 1] -
                         out_points[out_idx * 2 - 3]) < epsilon)) {
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

/* 0x106900 — Reject a 2D polygon whose vertex coordinates are not finite.
 * Walks vertex_count (x,y) pairs (8 bytes each) and returns 0 as soon as any
 * x or y float has all exponent bits set (IEEE 754 infinity or NaN); returns
 * 1 when every coordinate is finite. Returns a byte (bool): the original sets
 * AL only for the true path, leaving the loop's last-read dword in the upper
 * bytes of EAX. Loop counter stays 16-bit to match the original codegen.
 * Source: c:\halo\SOURCE\math\geometry.c */
bool convex_polygon2d_verify(int16_t vertex_count, uint32_t *vertices)
{
  int16_t i;

  i = 0;
  if (0 < vertex_count) {
    do {
      if ((vertices[i * 2] & 0x7f800000) == 0x7f800000 ||
          (vertices[i * 2 + 1] & 0x7f800000) == 0x7f800000) {
        return 0;
      }
      i = i + 1;
    } while (i < vertex_count);
  }
  return 1;
}

/* 0x106dc0 — Verify that a 3D polygon is convex and (near-)planar.
 * vertices is a flat array of (x,y,z) triples (12 bytes each); vertex_count is
 * the vertex count. A reference plane normal is built from the first three
 * vertices as cross(vert0 - vert1, vert2 - vert1). For every vertex the corner
 * normal cross(prev - cur, next - cur) is dotted against that reference normal;
 * if any dot falls below a small negative epsilon (0xb58637bd = -1e-6) the
 * winding has reversed and the function returns 0. The current vertex is also
 * rejected if any component is IEEE 754 infinity or NaN (all exponent bits
 * set). prev wraps to the last vertex on the first iteration; next wraps to
 * vertex 0 on the last. The reference-normal setup runs unconditionally before
 * the count guard, and the loop counter stays 16-bit, matching the original
 * codegen. Returns a byte (bool). Source: c:\halo\SOURCE\math\geometry.c */
bool convex_polygon3d_verify(int16_t vertex_count, float *vertices)
{
  float edge_a0, edge_a1, edge_a2;
  float edge_b0, edge_b1, edge_b2;
  float ref0, ref1, ref2;
  float a0, a1, a2, b0, b1, b2, c0, c1, c2, dot;
  float cx, cy, cz;
  float *prev, *cur, *next;
  int last;
  int16_t i;

  edge_a0 = vertices[0] - vertices[3];
  edge_a1 = vertices[1] - vertices[4];
  edge_a2 = vertices[2] - vertices[5];
  edge_b0 = vertices[6] - vertices[3];
  edge_b1 = vertices[7] - vertices[4];
  edge_b2 = vertices[8] - vertices[5];
  ref0 = edge_a1 * edge_b2 - edge_a2 * edge_b1;
  ref1 = edge_a2 * edge_b0 - edge_a0 * edge_b2;
  ref2 = edge_a0 * edge_b1 - edge_a1 * edge_b0;

  if (vertex_count <= 0) {
    return 1;
  }

  last = vertex_count - 1;
  for (i = 0; i < vertex_count; i++) {
    if (i == 0) {
      prev = vertices + vertex_count * 3 - 3;
    } else {
      prev = vertices + i * 3 - 3;
    }
    cur = vertices + i * 3;
    if (i == last) {
      next = vertices;
    } else {
      next = cur + 3;
    }

    cx = cur[0];
    if ((*(uint32_t *)&cx & 0x7f800000) == 0x7f800000) {
      return 0;
    }
    cy = cur[1];
    if ((*(uint32_t *)&cy & 0x7f800000) == 0x7f800000) {
      return 0;
    }
    cz = cur[2];
    if ((*(uint32_t *)&cz & 0x7f800000) == 0x7f800000) {
      return 0;
    }

    a0 = prev[0] - cur[0];
    a1 = prev[1] - cur[1];
    a2 = prev[2] - cur[2];
    b0 = next[0] - cur[0];
    b1 = next[1] - cur[1];
    b2 = next[2] - cur[2];
    c0 = a1 * b2 - a2 * b1;
    c1 = a2 * b0 - a0 * b2;
    c2 = a0 * b1 - a1 * b0;
    dot = ref0 * c0 + ref1 * c1 + ref2 * c2;
    if (dot < -9.99999997e-07f) {
      return 0;
    }
  }
  return 1;
}
