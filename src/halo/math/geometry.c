#include "x87_math.h" /* x87_fsin/x87_fcos/x87_fatan2f/x87_fabs: inline x87 ops */
/* MSVC 7.1 FABS intrinsic: declared+pragma here so fabs() inlines to a single
 * FABS instruction instead of a CRT call. */
extern double __cdecl fabs(double);
#if defined(_MSC_VER) && !defined(__clang__)
#pragma intrinsic(fabs)
#else
/* clang builds with -fno-builtin, which ignores the intrinsic pragma above and
 * emits a real CRT call to fabs. The original inlines a single x87 FABS. Force
 * clang to inline via the always-available builtin so the codegen matches the
 * binary (and so equivalence harnesses don't see an external fabs stub). */
#define fabs __builtin_fabs
#endif

/* 0x994d0 — Negate all four components of a plane */
float * plane_negate(float *plane_in, float *plane_out)
{
  plane_out[0] = -plane_in[0];
  plane_out[1] = -plane_in[1];
  plane_out[2] = -plane_in[2];
  plane_out[3] = -plane_in[3];
return plane_out; }

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
        if (*(float *)0x2533c0 >= denom) {
          if (t < tmax)
            tmax = t;
        } else {
          if (t > tmin)
            tmin = t;
        }
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
      clamped_t =
        -((line[0] * current_point[0] + current_point[1] * line[1]) - line[2]) /
        (dy * line[1] + dx * line[0]);
      if (clamped_t < *(float *)0x2533c0) {
        clamped_t = *(float *)0x2533c0;
      } else if (*(float *)0x2533c8 < clamped_t) {
        clamped_t = *(float *)0x2533c8;
      }

      out_points[(int)out_count * 2] = clamped_t * dx + current_point[0];
      mask |= (uint32_t)1 << ((uint8_t)out_count & 0x1f);
      out_count += 1;
      out_points[((int)out_count - 1) * 2 + 1] =
        clamped_t * dy + current_point[1];

      if (out_count != 1) {
        out_idx = (int)out_count;
        if ((epsilon > (float)fabs(out_points[out_idx * 2 - 2] -
                                   out_points[0]) &&
             epsilon > (float)fabs(out_points[out_idx * 2 - 1] -
                                   out_points[1])) ||
            (epsilon > (float)fabs(out_points[out_idx * 2 - 2] -
                                   out_points[out_idx * 2 - 4]) &&
             epsilon > (float)fabs(out_points[out_idx * 2 - 1] -
                                   out_points[out_idx * 2 - 3]))) {
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
        if ((epsilon > (float)fabs(out_points[out_idx * 2 - 2] -
                                   out_points[0]) &&
             epsilon > (float)fabs(out_points[out_idx * 2 - 1] -
                                   out_points[1])) ||
            (epsilon > (float)fabs(out_points[out_idx * 2 - 2] -
                                   out_points[out_idx * 2 - 4]) &&
             epsilon > (float)fabs(out_points[out_idx * 2 - 1] -
                                   out_points[out_idx * 2 - 3]))) {
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

/* 0x106960 — convex_polygon3d_clip_to_plane
 *
 * Sutherland-Hodgman clip of a 3D convex polygon (count xyz triples) against
 * `plane` (i,j,k,d), keeping the side with plane distance >= 0. Output goes
 * to out_verts (capacity max_count); verts may alias out_verts, in which case
 * the input is first copied to a 512-vertex stack buffer (_chkstk 0x1818,
 * buffer at EBP-0x1818 = 0x1800 bytes).
 *
 * Confirmed from 0x106960-0x106db7:
 *  - [EBP+0x1c] (kb: out_bitmask) is only ever written as a BYTE: 0 on entry,
 *    1 when an edge crosses the plane. [EBP+0x24] (kb: changed) is only read
 *    as a BYTE (0x106d08). The kb types are kept so the existing callers
 *    (bsp3d.c, structures.c) still compile; the accesses below are byte-wide.
 *  - Per vertex, distance > epsilon latches a "front" byte (EBP-2); otherwise
 *    distance < -epsilon latches a "back" byte (EBP-1).
 *  - A new vertex is dropped (count--) when it lies within epsilon on all
 *    three axes of the first output vertex or of the previous one.
 *  - Crossing t = -(dist(cur) / dot(prev-cur, n)) clamped to [0,1]
 *    (0x106b19 TEST AH,5 / JP; 0x106b30 TEST AH,0x41 / JNZ).
 *  - Overflow (output full) returns NONE after copying the input unchanged
 *    (assert line 0x637). Fewer than 3 outputs become 0.
 *  - Result selection (0x106cfa): front and back -> clipped count;
 *    front only -> input copied, count; back only -> 0; neither (coplanar)
 *    -> input copied when the [EBP+0x24] byte is set, else 0 (assert 0x630).
 * Distance sums follow the FLD order: last vertex (y*j + z*k) + i*x;
 * loop vertex (z*k + i*x) + y*j.
 */
int16_t convex_polygon3d_clip_to_plane(int16_t count, float *verts,
                                       float *plane, int16_t max_count,
                                       float *out_verts, uint32_t *out_bitmask,
                                       float epsilon, void *changed)
{
  float clip_buffer[0x200 * 3]; /* EBP-0x1818 */
  float *previous;
  float *current;
  float *last;
  float distance;
  float dx;
  float dy; /* EBP-0x14 */
  float dz;
  float t;
  int byte_size; /* EBP-0x08 */
  int16_t i; /* EBP-0x0c */
  int16_t out_count;
  bool inside; /* EBP+0x13 */
  bool previous_inside;
  bool any_front; /* EBP-0x02 */
  bool any_back; /* EBP-0x01 */

  out_count = 0;
  any_front = 0;
  any_back = 0;
  if (count < 3) {
    display_assert("count>=NUMBER_OF_VERTICES_PER_TRIANGLE",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x5d5, true);
    system_exit(-1);
  }
  if (out_bitmask != NULL) {
    *(uint8_t *)out_bitmask = 0;
  }
  if (verts == out_verts) {
    if (count > 0x200) {
      display_assert("count<=CLIP_BUFFER_SIZE",
                     "c:\\halo\\SOURCE\\math\\geometry.c", 0x5dc, true);
      system_exit(-1);
    }
    csmemcpy(clip_buffer, verts, count * 0xc);
    verts = clip_buffer;
  }

  byte_size = count * 0xc;
  previous = verts + count * 3 - 3;
  previous_inside = (previous[1] * plane[1] + previous[2] * plane[2] +
                     plane[0] * previous[0] - plane[3]) >= 0.0f;

  if (count > 0) {
    i = 0;
    do {
      current = verts + i * 3;
      inside = 1;
      distance = current[2] * plane[2] + plane[0] * current[0] +
                 current[1] * plane[1] - plane[3];
      if (!(distance >= 0.0f)) { /* 0x106a84 TEST AH,1 / JZ */
        inside = 0;
      }
      if (distance > epsilon) {
        any_front = 1;
      } else if (distance < -epsilon) {
        any_back = 1;
      }

      if (inside != previous_inside) {
        if (out_count == max_count) {
          out_count = -1;
          break;
        }
        if (out_bitmask != NULL) {
          *(uint8_t *)out_bitmask = 1;
        }
        dx = previous[0] - current[0];
        dy = previous[1] - current[1];
        dz = previous[2] - current[2];
        t = -((current[2] * plane[2] + plane[0] * current[0] +
               current[1] * plane[1] - plane[3]) /
              (dx * plane[0] + dz * plane[2] + dy * plane[1]));
        if (t < 0.0f) {
          t = 0.0f;
        } else if (t > 1.0f) {
          t = 1.0f;
        }
        last = out_verts + out_count * 3;
        last[0] = t * dx + current[0];
        last[1] = dy * t + current[1];
        last[2] = t * dz + current[2];
        out_count++;
        if (out_count != 1) {
          last = out_verts + out_count * 3;
          if ((fabs(last[-3] - out_verts[0]) < epsilon &&
               fabs(last[-2] - out_verts[1]) < epsilon &&
               fabs(last[-1] - out_verts[2]) < epsilon) ||
              (fabs(last[-3] - last[-6]) < epsilon &&
               fabs(last[-2] - last[-5]) < epsilon &&
               fabs(last[-1] - last[-4]) < epsilon)) {
            out_count--;
          }
        }
      }

      if (inside) {
        if (out_count >= max_count) {
          out_count = -1;
          break;
        }
        last = out_verts + out_count * 3;
        last[0] = current[0];
        last[1] = current[1];
        last[2] = current[2];
        out_count++;
        if (out_count != 1) {
          last = out_verts + out_count * 3;
          if ((fabs(last[-3] - out_verts[0]) < epsilon &&
               fabs(last[-2] - out_verts[1]) < epsilon &&
               fabs(last[-1] - out_verts[2]) < epsilon) ||
              (fabs(last[-3] - last[-6]) < epsilon &&
               fabs(last[-2] - last[-5]) < epsilon &&
               fabs(last[-1] - last[-4]) < epsilon)) {
            out_count--;
          }
        }
      }

      previous = current;
      previous_inside = inside;
      i++;
    } while (i < count);

    if (out_count == -1) {
      if (count < 0 || count > max_count) {
        display_assert("count>=0 && count<=maximum_count",
                       "c:\\halo\\SOURCE\\math\\geometry.c", 0x637, true);
        system_exit(-1);
      }
      csmemcpy(out_verts, verts, byte_size);
      return out_count;
    }
    if (out_count < 3) {
      out_count = 0;
    }
  } else {
    out_count = 0;
  }

  if (any_front) {
    if (any_back) {
      return out_count;
    }
  } else if (any_back || *(uint8_t *)&changed == 0) {
    return 0;
  }

  if (count < 0 || count > max_count) {
    display_assert("count>=0 && count<=maximum_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x630, true);
    system_exit(-1);
  }
  csmemcpy(out_verts, verts, byte_size);
  return count;
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

/* 0x106f50 — Build an initial simplex (tetrahedron) from a point cloud: the
 * seed step of a convex-hull/quickhull. Selects four extremal points, emits
 * 4 vertices (stride 0xc), 6 edges (stride 0x20) and 4 surfaces (stride 0x1c)
 * with their connectivity constants, then marks the remaining capacity slots
 * as unused (byte-0 at each slot start). Returns 1 on success, 0 on a
 * degenerate/failed seed. Point selection:
 *   p0 = min-X point; p1 = point farthest from p0;
 *   p2 = point farthest from the p0-p1 line; p3 = point farthest from the
 *   (p0,p1,p2) plane (with a winding swap when p3 is on the plane front).
 * Surface planes come from FUN_001037b0 (plane-from-3-points).
 * Source: c:\halo\SOURCE\math\geometry.c:1710 */
bool FUN_00106f50(int16_t point_count, float *points, int16_t vertices_capacity,
                  char *vertices, int16_t edges_capacity, char *edges,
                  int16_t surfaces_capacity, char *surfaces)
{
  char *vbase;
  float *p0;
  float *p1;
  float *p2;
  float *p3;
  float *pfVar5;
  float *scan;
  float plane[4];
  float edge_x;
  float edge_y;
  float edge_z;
  float len_sq;
  float dz;
  float t;
  float proj_y;
  float proj_z;
  float perp_x;
  float perp_y;
  float perp_dist;
  float best;
  float best_dist;
  unsigned int n;
  int i;
  int min_x_index;
  int far_index;
  int line_index;
  int plane_index;
  int saved_far;

  vbase = vertices;
  min_x_index = -1;
  far_index = -1;
  line_index = -1;
  plane_index = -1;

  if (points == (float *)0) {
    display_assert("points", "c:\\halo\\SOURCE\\math\\geometry.c", 0x6ae, 1);
    system_exit(-1);
  }
  if (vertices == (char *)0) {
    display_assert("vertices", "c:\\halo\\SOURCE\\math\\geometry.c", 0x6af, 1);
    system_exit(-1);
  }
  if (edges == (char *)0) {
    display_assert("edges", "c:\\halo\\SOURCE\\math\\geometry.c", 0x6b0, 1);
    system_exit(-1);
  }
  if (surfaces == (char *)0) {
    display_assert("surfaces", "c:\\halo\\SOURCE\\math\\geometry.c", 0x6b1, 1);
    system_exit(-1);
  }

  if (3 < vertices_capacity && 5 < edges_capacity && 3 < surfaces_capacity &&
      0 < point_count) {
    /* Pass 1: find the point with the minimum X coordinate. */
    i = 0;
    scan = points;
    best = 3.402823466e+38f;
    do {
      if (*scan < best) {
        best = *scan;
        min_x_index = i;
      }
      i = i + 1;
      scan = scan + 3;
    } while ((int16_t)i < point_count);

    if ((int16_t)min_x_index != -1) {
      p0 = points + (int16_t)min_x_index * 3;

      /* Pass 2: find the point farthest (squared distance) from p0. */
      i = 0;
      scan = points + 2;
      best = 0.0f;
      do {
        if (best < (*p0 - scan[-2]) * (*p0 - scan[-2]) +
                     (p0[1] - scan[-1]) * (p0[1] - scan[-1]) +
                     (p0[2] - *scan) * (p0[2] - *scan)) {
          far_index = i;
          best = (*p0 - scan[-2]) * (*p0 - scan[-2]) +
                 (p0[1] - scan[-1]) * (p0[1] - scan[-1]) +
                 (p0[2] - *scan) * (p0[2] - *scan);
        }
        i = i + 1;
        scan = scan + 3;
      } while ((int16_t)i < point_count);

      if ((int16_t)far_index != -1 && best >= 0.01f) {
        p1 = points + (int16_t)far_index * 3;

        /* Pass 3: find the point farthest from the p0-p1 line. */
        i = 0;
        scan = points + 2;
        edge_x = *p1 - *p0;
        edge_y = p1[1] - p0[1];
        edge_z = p1[2] - p0[2];
        len_sq = edge_x * edge_x + edge_y * edge_y + edge_z * edge_z;
        best = 0.0f;
        do {
          dz = *scan - p0[2];
          t = ((scan[-2] - *p0) * edge_x + (scan[-1] - p0[1]) * edge_y +
               dz * edge_z) /
              len_sq;
          proj_y = edge_y * t;
          proj_z = edge_z * t;
          perp_x = (scan[-2] - *p0) - edge_x * t;
          perp_y = (scan[-1] - p0[1]) - proj_y;
          perp_dist =
            perp_x * perp_x + perp_y * perp_y + (dz - proj_z) * (dz - proj_z);
          if (best < perp_dist) {
            line_index = i;
            best = perp_dist;
          }
          i = i + 1;
          scan = scan + 3;
        } while ((int16_t)i < point_count);

        if ((int16_t)line_index != -1 && best >= 0.01f) {
          /* Pass 4: plane through (p0,p1,p2); find the farthest point. */
          best_dist = 0.0f;
          FUN_001037b0(plane, p0, p1, points + (int16_t)line_index * 3);
          saved_far = far_index;
          i = 0;
          scan = points + 2;
          do {
            perp_dist =
              (plane[2] * *scan + plane[1] * scan[-1] + plane[0] * scan[-2]) -
              plane[3];
            if (fabs(best_dist) < fabs(perp_dist)) {
              best_dist = perp_dist;
              plane_index = i;
            }
            i = i + 1;
            scan = scan + 3;
          } while ((int16_t)i < point_count);

          if ((int16_t)plane_index != -1 && fabs(best_dist) >= 0.01f) {
            if (best_dist > 0.0f) {
              far_index = line_index;
              line_index = saved_far;
            }

            /* Emit 4 vertices (stride 0xc). */
            *(int *)(vbase + 4) = 0;
            *(int *)(vbase + 0x10) = 0;
            *(int16_t *)(vbase + 2) = (int16_t)min_x_index;
            *vbase = 1;
            vbase[0xc] = 1;
            vbase[0x18] = 1;
            *(int16_t *)(vbase + 0x1a) = (int16_t)line_index;
            *(int16_t *)(vbase + 0xe) = (int16_t)far_index;
            *(int16_t *)(vbase + 0x26) = (int16_t)plane_index;
            *(int *)(vbase + 0x1c) = 1;
            vbase[0x24] = 1;
            *(int *)(vbase + 0x28) = 3;

            /* Emit 6 edges (stride 0x20). */
            *(int *)(edges + 0x10) = 3;
            *(int *)(edges + 0x58) = 3;
            *(int *)(edges + 0x68) = 3;
            *(int *)(edges + 0x78) = 3;
            *(int *)(edges + 0x84) = 3;
            *(int *)(edges + 0xa4) = 3;
            *(int *)(edges + 0xb0) = 3;
            *(int *)(edges + 0xb8) = 3;
            *(int *)(edges + 4) = 0;
            *(int *)(edges + 0x14) = 0;
            *(int *)(edges + 0x34) = 0;
            *(int *)(edges + 0x48) = 0;
            *(int *)(edges + 0x4c) = 0;
            *(int *)(edges + 0x54) = 0;
            *(int *)(edges + 100) = 0;
            *(int *)(edges + 0x8c) = 0;
            *(int *)(edges + 0x98) = 2;
            *(int *)(edges + 0xa8) = 2;
            *(int *)(edges + 0xb4) = 2;
            *edges = 1;
            *(int *)(edges + 8) = 1;
            *(int *)(edges + 0xc) = 1;
            *(int *)(edges + 0x18) = 1;
            edges[0x20] = 1;
            *(int *)(edges + 0x24) = 1;
            edges[0x40] = 1;
            edges[0x60] = 1;
            *(int *)(edges + 0x74) = 1;
            edges[0x80] = 1;
            *(int *)(edges + 0x88) = 1;
            *(int *)(edges + 0x94) = 1;
            edges[0xa0] = 1;
            *(int *)(edges + 0xac) = 1;
            *(int *)(edges + 0x28) = 2;
            *(int *)(edges + 0x2c) = 2;
            *(int *)(edges + 0x30) = 4;
            *(int *)(edges + 0x38) = 2;
            *(int *)(edges + 0x44) = 2;
            *(int *)(edges + 0x50) = 5;
            *(int *)(edges + 0x6c) = 4;
            *(int *)(edges + 0x70) = 2;
            *(int *)(edges + 0x90) = 5;

            /* Emit 4 surfaces (stride 0x1c); planes via FUN_001037b0. */
            *surfaces = 1;
            pfVar5 = points + (int16_t)line_index * 3;
            p2 = points + (int16_t)far_index * 3;
            FUN_001037b0((float *)(surfaces + 4), p0, p2, pfVar5);
            *(int *)(surfaces + 0x14) = 0;
            surfaces[0x1c] = 1;
            p3 = points + (int16_t)plane_index * 3;
            FUN_001037b0((float *)(surfaces + 0x20), p0, p3, p2);
            *(int *)(surfaces + 0x30) = 0;
            surfaces[0x38] = 1;
            FUN_001037b0((float *)(surfaces + 0x3c), p2, p3, pfVar5);
            *(int *)(surfaces + 0x4c) = 1;
            surfaces[0x54] = 1;
            FUN_001037b0((float *)(surfaces + 0x58), p0, pfVar5, p3);
            *(int *)(surfaces + 0x68) = 2;

            /* Mark remaining capacity slots as unused. */
            if (4 < vertices_capacity) {
              vbase = vbase + 0x30;
              n = (unsigned int)(unsigned short)(vertices_capacity - 4);
              do {
                *vbase = 0;
                vbase = vbase + 0xc;
                n = n - 1;
              } while (n != 0);
            }
            if (6 < edges_capacity) {
              edges = edges + 0xc0;
              n = (unsigned int)(unsigned short)(edges_capacity - 6);
              do {
                *edges = 0;
                edges = edges + 0x20;
                n = n - 1;
              } while (n != 0);
            }
            if (4 < surfaces_capacity) {
              surfaces = surfaces + 0x70;
              n = (unsigned int)(unsigned short)(surfaces_capacity - 4);
              do {
                *surfaces = 0;
                surfaces = surfaces + 0x1c;
                n = n - 1;
              } while (n != 0);
            }
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

/* 0x1056e0 — Dispose of a sphere geometry object.
 * Asserts the sphere handle and its two allocated arrays (vertices at +0x4,
 * triangle_strip_vertex_indices at +0x8) are non-NULL, then frees the two
 * arrays followed by the sphere structure itself.
 * Source: c:\halo\SOURCE\math\geometry.c (lines 0x75-0x7b). */
void FUN_001056e0(void *handle)
{
  if (handle == 0) {
    display_assert("sphere", "c:\\halo\\SOURCE\\math\\geometry.c", 0x75, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)handle + 4) == 0) {
    display_assert("sphere->vertices", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x76, 1);
    system_exit(-1);
  }
  if (*(int *)((char *)handle + 8) == 0) {
    display_assert("sphere->triangle_strip_vertex_indices",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x77, 1);
    system_exit(-1);
  }
  debug_free(*(void **)((char *)handle + 4),
             "c:\\halo\\SOURCE\\math\\geometry.c", 0x79);
  debug_free(*(void **)((char *)handle + 8),
             "c:\\halo\\SOURCE\\math\\geometry.c", 0x7a);
  debug_free(handle, "c:\\halo\\SOURCE\\math\\geometry.c", 0x7b);
}

/* 0x105830 - interpolate a subdivision vertex between two parent vertices.
 * (TU: c:\halo\SOURCE\math\geometry.c)
 *
 * Register ABI (prologue at 0x105830): MOV SI,DX and direct use of AX/CX/BX/EDI
 * with only ESI preserved. Register args (all low-16 values):
 *   subdivision_index@<eax>, subdivision_count@<ecx>, parent2@<edx> (copied to
 *   SI), parent1@<ebx>, sphere@<edi>.  Stack arg: new_vertex ([EBP+0x8]).
 *
 * frac = subdivision_index / subdivision_count (FILD/FIDIV); the new vertex is
 * inv_frac*parent1 + frac*parent2 component-wise (inv_frac = 1.0 - frac; 1.0 at
 * 0x2533c8), written into sphere->vertices[new_vertex] (vertices at sphere+0x4,
 * stride 3 floats), then normalized in place via normalize3d (return
 * discarded). Asserts subdivision_index in (0,count) and each vertex index in
 * [0,vertex_count] (vertex_count is a short at sphere+0xc). */
void calculate_vertex(short subdivision_index /* @<eax> */,
                      short subdivision_count /* @<ecx> */,
                      short parent2 /* @<edx> */, short parent1 /* @<ebx> */,
                      void *sphere /* @<edi> */, short new_vertex)
{
  float frac;
  float inv_frac;
  int itmp;
  float *verts;
  float *vp1;
  float *vp2;
  float *vout;
  short vertex_count;

  itmp = subdivision_index;
  frac = (float)itmp;
  itmp = subdivision_count;
  frac = frac / itmp;
  inv_frac = *(float *)0x002533c8 - frac;

  if (subdivision_index <= 0 || subdivision_index >= subdivision_count) {
    display_assert(
      "subdivision_index > 0 && subdivision_index < subdivision_count",
      "c:\\halo\\SOURCE\\math\\geometry.c", 0x13b, true);
    system_exit(-1);
  }
  vertex_count = *(short *)((char *)sphere + 0xc);
  if (parent1 < 0 || parent1 > vertex_count) {
    display_assert("parent1 >=0 && parent1 <= sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x13c, true);
    system_exit(-1);
  }
  if (parent2 < 0 || parent2 > vertex_count) {
    display_assert("parent2 >=0 && parent2 <= sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x13d, true);
    system_exit(-1);
  }
  if (new_vertex < 0 || new_vertex > vertex_count) {
    display_assert("new_vertex >=0 && new_vertex <= sphere->vertex_count",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x13e, true);
    system_exit(-1);
  }

  verts = *(float **)((char *)sphere + 4);
  vp1 = verts + (int)parent1 * 3;
  vp2 = verts + (int)parent2 * 3;
  vout = verts + (int)new_vertex * 3;
  vout[0] = inv_frac * vp1[0] + frac * vp2[0];
  vout[1] = frac * vp2[1] + inv_frac * vp1[1];
  vout[2] = frac * vp2[2] + inv_frac * vp1[2];
  normalize3d((real_vector3d *)vout);
}

/* 0x105980 — Build a ring/cylinder (torus-like) mesh.
 * Sweeps ring_segment_count rings around a cross-section of
 * cylinder_segment_count segments. For each ring the ring angle drives a
 * cos/sin pair (scaled by param_8) that offsets a cross-section built by
 * rotating a base radius (param_10) about the ring normal, then transforms
 * each vertex by the caller's matrix.
 * Outputs: vertex positions (out_positions, stride 3 floats), tex coords
 * (out_texcoords, stride 2 floats), a triangle-strip index buffer
 * (out_indices), the emitted vertex count (*out_vertex_count) and the number
 * of strip index-runs (*out_index_run_count).
 * The ring normal is the cross product of the cross-section direction
 * (cos*param_8, sin*param_8, 0) with the global axis vector at *0x31fc44,
 * normalized when its length is >= the epsilon at 0x2533d0.
 * Constants: 0x255a54 = 6.2831855f (2*pi), 0x2533c0 = 0.0f, 0x2533c8 = 1.0f,
 * 0x2533d0 = double epsilon. Asserts at geometry.c:0x15a/0x15b.
 * Source: c:\halo\SOURCE\math\geometry.c:346 */
void FUN_00105980(float *matrix, short *out_vertex_count,
                  short *out_index_run_count, float *out_positions,
                  float *out_texcoords, short *out_indices,
                  short ring_segment_count, float param_8,
                  int cylinder_segment_count, float param_10)
{
  float fVar1, fVar2, fVar4, angle;
  float fVar9, fVar10, fVar11, fVar12, fVar_sin;
  int iVar5;
  float *pfVar6;
  short sVar8;
  /* normal[0]=local_38, normal[1]=local_34, normal[2]=local_30; the three
   * must be contiguous+ascending because &normal[0] is passed as the axis
   * argument to rotate_vector3d_by_sincos (stack-aliasing hazard). */
  float normal[3];
  int local_2c;
  float local_28, local_24;
  int local_20, local_1c, local_18, local_14, local_10, local_c, local_8;

  local_1c = 0;
  local_8 = 0;
  if (ring_segment_count <= 2) {
    display_assert("ring_segment_count>2", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x15a, 1);
    system_exit(-1);
  }
  if ((short)cylinder_segment_count <= 2) {
    display_assert("cylinder_segment_count>2",
                   "c:\\halo\\SOURCE\\math\\geometry.c", 0x15b, 1);
    system_exit(-1);
  }
  local_10 = 0;
  if (ring_segment_count >= 0) {
    local_20 = (int)ring_segment_count;
    local_18 = 0;
    local_24 = (float)local_20;
    pfVar6 = *(float **)0x0031fc44;
    do {
      fVar9 = (float)local_18 / local_24;
      angle = *(float *)0x00255a54 * fVar9;
      fVar10 = x87_fcos(angle);
      fVar1 = fVar10 * param_8;
      fVar11 = x87_fsin(angle);
      fVar2 = param_8 * fVar11;
      /* ring normal = (fVar1, fVar2, 0) x axis[]; 0x2533c0 == 0.0f.
       * normal[2]=A, normal[1]=B, normal[0]=C; length sum is (C^2+B^2)+A^2 to
       * match the original's x87 add order. */
      normal[2] = fVar1 * pfVar6[1] - fVar2 * pfVar6[0];
      normal[1] = pfVar6[0] * *(float *)0x002533c0 - fVar1 * pfVar6[2];
      normal[0] = fVar2 * pfVar6[2] - pfVar6[1] * *(float *)0x002533c0;
      fVar4 = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] +
                    normal[2] * normal[2]);
      if (!(x87_fabs(fVar4) < *(double *)0x002533d0)) {
        fVar4 = *(float *)0x002533c8 / fVar4;
        normal[0] = normal[0] * fVar4;
        normal[1] = normal[1] * fVar4;
        normal[2] = fVar4 * normal[2];
      }
      sVar8 = 0;
      if (-1 < (short)cylinder_segment_count) {
        local_c = (int)(short)cylinder_segment_count;
        local_14 = (local_8 - cylinder_segment_count) + -1;
        local_28 = (float)(fVar9 + fVar9);
        do {
          out_texcoords[1] = local_28;
          if (0 < (short)local_10) {
            if (sVar8 == 0) {
              *out_indices = (short)cylinder_segment_count * 2 + 2;
              out_indices = out_indices + 1;
              local_1c = local_1c + 1;
            }
            *out_indices = (short)local_8;
            out_indices[1] = (short)local_14;
            out_indices = out_indices + 2;
          }
          if ((short)local_10 == ring_segment_count) {
            /* last ring: copy the vertex/texcoord from the first ring */
            iVar5 = (local_c + 1) * local_20;
            pfVar6 = out_positions + iVar5 * -3;
            *out_positions = *pfVar6;
            out_positions[1] = pfVar6[1];
            out_positions[2] = pfVar6[2];
            *out_texcoords = out_texcoords[iVar5 * -2];
          } else {
            local_2c = (int)sVar8;
            fVar9 = (float)local_2c / (float)local_c;
            *out_texcoords = (float)(fVar9 + fVar9);
            if (sVar8 == (short)cylinder_segment_count) {
              /* seam: copy from the start of this ring */
              pfVar6 = out_positions + local_c * -3;
              *out_positions = *pfVar6;
              out_positions[1] = pfVar6[1];
              out_positions[2] = pfVar6[2];
            } else {
              angle = *(float *)0x00255a54 * fVar9;
              fVar12 = x87_fcos(angle);
              *out_positions = fVar10 * param_10;
              out_positions[1] = fVar11 * param_10;
              out_positions[2] = 0.0f;
              fVar_sin = x87_fsin(angle);
              rotate_vector3d_by_sincos((real_vector3d *)out_positions, (real_vector3d *)normal, fVar_sin, fVar12);
              *out_positions = fVar1 + *out_positions;
              out_positions[2] = out_positions[2];
              out_positions[1] = fVar2 + out_positions[1];
              matrix_transform_point((const real_matrix4x3 *)matrix, (real_point3d *)out_positions, (real_point3d *)out_positions);
            }
          }
          out_texcoords = out_texcoords + 2;
          out_positions = out_positions + 3;
          local_8 = local_8 + 1;
          local_14 = local_14 + 1;
          sVar8 = sVar8 + 1;
          pfVar6 = *(float **)0x0031fc44;
        } while (sVar8 <= (short)cylinder_segment_count);
      }
      local_10 = local_10 + 1;
      local_18 = local_18 + 1;
    } while ((short)local_10 <= ring_segment_count);
  }
  *out_vertex_count = (short)local_8;
  *out_index_run_count = (short)local_1c;
}

/* 0x105d20 — Reduce a 2D point set to its convex hull as an index list.
 * Gift-wrapping (Jarvis march). shell_update (called with the vertex array in
 * EBX) validates that at least three non-collinear points exist (returns 2);
 * otherwise nothing is emitted and 0 is returned.
 *   Phase 1: pick the start vertex (lowest y, then leftmost x) with an epsilon
 *            tie-break (1e-4f) on both axes.
 *   Phase 2: from the current vertex, atan2(dy,dx) angle scan against a running
 *            angle base, wrapping candidate angles into [-1e-4f, ...) by adding
 *            2*pi; keep the minimum-angle vertex, append its index, and stop
 *            when the chosen vertex closes back on the first. A collinear/
 *            degenerate guard uses a double epsilon (=(double)1e-4f) on the
 *            |component delta| between the chosen and first vertices.
 *   Phase 3: reached only when the walk fills all slots (index_count reaches
 *            vertex_count); compacts a trailing duplicate run to the front with
 *            three bounds asserts (geometry.c 0x279,0x27a,0x282).
 * param_1 = vertex_count, param_2 = float[2] vertex array (x,y; 8-byte stride),
 * param_3 = int16 output index list. Returns the emitted index count in AX.
 * Source: c:\halo\SOURCE\math\geometry.c */
int16_t convex_hull2d_reduce(int16_t vertex_count, float *vertices,
                             int16_t *out_indices)
{
  int16_t index_count;

  index_count = 0;
  if (shell_update(vertex_count, vertices) == 2) {
    float base_angle;
    float best_x;
    float best_y;
    int16_t start_index;
    int16_t current_index;
    int16_t next_index;
    float min_angle;
    char collinear_flag;
    int16_t i;
    int16_t first;
    float *p;
    float *ref;

    base_angle = 0.0f; /* FLOAT_002533c0 = 0.0f, running gift-wrap base */
    best_x = 3.4028235e38f; /* FLT_MAX */
    best_y = 3.4028235e38f;
    start_index = -1; /* SI default = low word of FLT_MAX (dead: count>0) */
    collinear_flag = 0;

    /* Phase 1: lowest y, then leftmost x, with epsilon tie-break. */
    if (vertex_count > 0) {
      p = vertices + 1; /* &vertices[0].y */
      for (i = 0; i < vertex_count; i = i + 1) {
        if ((p[0] < best_y - 1e-4f) ||
            ((p[0] < best_y) && (p[-1] < best_x + 1e-4f)) ||
            ((p[0] < best_y + 1e-4f) && (p[-1] < best_x - 1e-4f))) {
          best_x = p[-1];
          best_y = p[0];
          start_index = i;
        }
        p = p + 2;
      }
    }

    current_index = start_index;
    next_index =
      start_index; /* EBX default (dead: inner loop always assigns) */
    for (;;) {
      min_angle = 3.4028235e38f; /* FLT_MAX reset (0x105de9) */
      if (index_count >= vertex_count) {
        goto compaction;
      }
      out_indices[index_count] = current_index;
      index_count = index_count + 1;

      /* Phase 2: min-angle gift-wrap scan. */
      if (vertex_count > 0) {
        ref = vertices + current_index * 2;
        p = vertices;
        for (i = 0; i < vertex_count; i = i + 1) {
          if ((p[0] != ref[0]) || (p[1] != ref[1])) {
            float angle;
            float dy = p[1] - ref[1];
            float dx = p[0] - ref[0];

#if defined(_MSC_VER) && !defined(__clang__)
            angle = (float)atan2((double)dy, (double)dx) - base_angle;
#else
            angle = x87_fatan2f(dy, dx) - base_angle;
#endif
            if (angle < -1e-4f) {
              do {
                angle = angle + 2.f * _pi; /* 2*pi wrap */
              } while (angle < -1e-4f);
            }
            if (angle < min_angle) {
              min_angle = angle;
              next_index = i;
            }
          }
          p = p + 2;
        }
      }

      base_angle = base_angle + min_angle;
      current_index = next_index;

      first = out_indices[0];
      if (collinear_flag == 0) {
        if ((fabs(vertices[next_index * 2] - vertices[first * 2]) >= 1e-4f) ||
            (fabs(vertices[next_index * 2 + 1] - vertices[first * 2 + 1]) >=
             1e-4f)) {
          collinear_flag = 1;
        }
      }

      first = out_indices[0];
      if (next_index == first) {
        return index_count;
      }
      if (collinear_flag == 0) {
        continue;
      }
      if ((fabs(vertices[next_index * 2] - vertices[first * 2]) >= 1e-4f) ||
          (fabs(vertices[next_index * 2 + 1] - vertices[first * 2 + 1]) >=
           1e-4f)) {
        continue;
      }
      return index_count;
    }

  compaction: {
    int16_t last_hull;
    int16_t search;
    int16_t k;

    search = index_count - 2;
    if (search <= 0) {
      goto assert_start_positive;
    }
    last_hull = out_indices[index_count - 1];
    for (;;) {
      if (out_indices[search] == last_hull) {
        int16_t new_count;

        new_count = (index_count - 1) - search;
        index_count = new_count;
        if (new_count > 0) {
          int src;
          int16_t *psrc;
          int16_t *pdst;

          src = search;
          psrc = out_indices + search;
          pdst = out_indices;
          k = 0;
          do {
            if (vertex_count <= k) {
              display_assert("vertex_index<vertex_count",
                             "c:\\halo\\SOURCE\\math\\geometry.c", 0x279, 1);
              system_exit(-1);
            }
            if (vertex_count <= src) {
              display_assert("start_vertex_index+vertex_index<vertex_count",
                             "c:\\halo\\SOURCE\\math\\geometry.c", 0x27a, 1);
              system_exit(-1);
            }
            k = k + 1;
            *pdst = *psrc;
            psrc = psrc + 1;
            pdst = pdst + 1;
            src = src + 1;
          } while (k < new_count);
        }
        if (search > 0) {
          return index_count;
        }
        goto assert_start_positive;
      }
      search = search - 1;
      if (search < 1) {
        goto assert_start_positive;
      }
    }
  }

  assert_start_positive:
    display_assert("start_vertex_index>0", "c:\\halo\\SOURCE\\math\\geometry.c",
                   0x282, 1);
    system_exit(-1);
  }
  return index_count;
}
