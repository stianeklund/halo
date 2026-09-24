/* Build a 3D convex hull from a point cloud (0x107c30).
 * Validates the four output-array pointers (null asserts + system_exit(-1),
 * matching FUN_00106f50's own idiom at geometry.c), then calls FUN_00106f50
 * to build the initial tetrahedron/hull, then calls convex_hull3d_expand
 * once per input point to fold each remaining point into the hull. Returns
 * false immediately if either FUN_00106f50 or any convex_hull3d_expand call
 * fails; true once every point has been folded in.
 * point_count is read/compared as a 16-bit value (TEST SI,SI / CMP DI,SI in
 * the binary), so the loop index and count stay int16_t here. */
bool convex_hull3d(int16_t point_count, float *points,
                   int16_t vertices_capacity, char *vertices,
                   int16_t edges_capacity, char *edges,
                   int16_t surfaces_capacity, char *surfaces)
{
  int16_t i;
  bool ok;

  if (points == (float *)0) {
    display_assert("points", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ec, 1);
    system_exit(-1);
  }
  if (vertices == (char *)0) {
    display_assert("vertices", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ed, 1);
    system_exit(-1);
  }
  if (edges == (char *)0) {
    display_assert("edges", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ee, 1);
    system_exit(-1);
  }
  if (surfaces == (char *)0) {
    display_assert("surfaces", "c:\\halo\\SOURCE\\math\\geometry.c", 0x8ef, 1);
    system_exit(-1);
  }

  ok = FUN_00106f50(point_count, points, vertices_capacity, vertices,
                    edges_capacity, edges, surfaces_capacity, surfaces);
  if (!ok) {
    return 0;
  }

  for (i = 0; i < point_count; i++) {
    ok = convex_hull3d_expand(point_count, points, vertices_capacity, vertices,
                              edges_capacity, edges, surfaces_capacity,
                              surfaces, i);
    if (!ok) {
      return 0;
    }
  }

  return 1;
}

/* Store a 2D rectangle (0x1089a0).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Store order follows the binary: left, top, right, bottom. */
void set_rectangle2d(int16_t *rect, int16_t left, int16_t top, int16_t right,
                     int16_t bottom)
{
  rect[1] = left;
  rect[0] = top;
  rect[3] = right;
  rect[2] = bottom;
}

/* Store a 2D point (0x1089d0).
 * point layout: {x, y} as int16_t[2]. */
void set_point2d(int16_t *point, int16_t x, int16_t y)
{
  point[0] = x;
  point[1] = y;
}

/* Offset a 2D point by (dx, dy) (0x1089f0).
 * point layout: {x, y} as int16_t[2]. */
void offset_point2d(int16_t *point, int16_t dx, int16_t dy)
{
  point[0] += dx;
  point[1] += dy;
}

/* Width of a 2D rectangle (0x108a10).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary reads `right` zero-extended (xor eax,eax; mov ax,[ecx+6]) and
 * `left` sign-extended (movsx ecx,[ecx+2]); both extensions preserved. */
int rectangle2d_width(const int16_t *rect)
{
  return (int)(uint16_t)rect[3] - (int)rect[1];
}

/* Height of a 2D rectangle (0x108a30).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary reads `bottom` zero-extended (xor eax,eax; mov ax,[ecx+4]) and
 * `top` sign-extended (movsx ecx,[ecx]); both extensions preserved. */
int rectangle2d_height(const int16_t *rect)
{
  return (int)(uint16_t)rect[2] - (int)rect[0];
}

/* Inset a 2D rectangle by (dx, dy) (0x108a50).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Store order follows the binary: left, right, top, bottom. */
void inset_rectangle2d(int16_t *rect, int16_t dx, int16_t dy)
{
  rect[1] += dx;
  rect[3] -= dx;
  rect[0] += dy;
  rect[2] -= dy;
}

/* Offset a 2D rectangle by (dx, dy) (0x108a70).
 * rect layout: {top, left, bottom, right} as int16_t[4]. */
void rect2d_offset(int16_t *rect, int16_t dx, int16_t dy)
{
  rect[1] += dx;
  rect[3] += dx;
  rect[0] += dy;
  rect[2] += dy;
}

/* Compute the bounding-rectangle union ("hull") of two 2D rectangles
 * (0x108c60).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * hull.top/left = min(a,b); hull.bottom/right = max(a,b); all signed 16-bit
 * compares (JG/JLE/CMP on 16-bit regs). Store order follows the binary:
 * left, right, top, bottom. */
void rectangle2d_hull_from_rectangles2d(const int16_t *rect_a,
                                        const int16_t *rect_b, int16_t *hull)
{
  hull[1] = (rect_a[1] <= rect_b[1]) ? rect_a[1] : rect_b[1];
  hull[3] = (rect_a[3] > rect_b[3]) ? rect_a[3] : rect_b[3];
  hull[0] = (rect_a[0] <= rect_b[0]) ? rect_a[0] : rect_b[0];
  hull[2] = (rect_a[2] > rect_b[2]) ? rect_a[2] : rect_b[2];
}

/* Test whether a 2D point lies inside a 2D rectangle (0x108cd0).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * point layout: {x, y} as int16_t[2].
 * Binary compares point[0] against left/right and point[1] against
 * top/bottom, all as signed 16-bit compares (JL/JGE); the range is
 * half-open at right/bottom. Result is returned in AL. */
boolean point2d_in_rectangle2d(const int16_t *rect, const int16_t *point)
{
  return (boolean)(point[0] >= rect[1] && point[0] < rect[3] &&
                   point[1] >= rect[0] && point[1] < rect[2]);
}

/* Test whether `interior` lies entirely inside `rect` (0x108d00).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary compare order is left, right, top, bottom; all signed 16-bit
 * (JL/JG), and the bounds are inclusive on every edge.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t interior_rectangle2d(const int16_t *rect, const int16_t *interior)
{
  return (int32_t)(interior[1] >= rect[1] && interior[3] <= rect[3] &&
                   interior[0] >= rect[0] && interior[2] <= rect[2]);
}

/* Test whether two 2D rectangles are identical (0x108d40).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary compare order is left, right, top, bottom, all 16-bit equality
 * compares short-circuiting to the common failure exit.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t equal_rectangle2d(const int16_t *rect_a, const int16_t *rect_b)
{
  return (int32_t)(rect_a[1] == rect_b[1] && rect_a[3] == rect_b[3] &&
                   rect_a[0] == rect_b[0] && rect_a[2] == rect_b[2]);
}

/* Test whether two 2D points are identical (0x108d80).
 * point layout: {x, y} as int16_t[2].
 * Binary compares x then y as 16-bit equality compares, both short-circuiting
 * to the common failure exit.
 * Returns a full 32-bit 0/1 in EAX (mov eax,1 / xor eax,eax), not a byte
 * boolean, so the return type is int32_t. */
int32_t equal_point2d(const int16_t *point_a, const int16_t *point_b)
{
  return (int32_t)(point_a[0] == point_b[0] && point_a[1] == point_b[1]);
}

/* Compute floor(log2(value)) (0x108db0).
 * Returns 0 for value <= 1. */
int16_t FUN_00108db0(unsigned int value)
{
  int result = 0;
  if (value > 0) {
    while (value != 1) {
      value >>= 1;
      result++;
    }
  }
  return (int16_t)result;
}

/* Compute a shift-count-style ceiling(log2(value)) (0x108dd0). No callers in
 * this binary (xrefs_to empty).
 * Binary shape (verbatim from disassembly, not the (buggy, ecx-dropping)
 * Ghidra decompile): ecx=0; if (value==0) skip straight to `return ecx+1`
 * (i.e. returns 1 for value==0). Otherwise eax=value-1; if eax==1 (value==2)
 * skip the loop (returns 1). Otherwise loop: eax>>=1; ecx++; while(eax!=1).
 * NOTE: value==1 makes eax=0 after the decrement, so `eax>>=1` stays 0
 * forever and the loop never reaches eax==1 -- this mirrors an infinite
 * loop present in the original binary for that input and is preserved
 * as-is; not fixed here. */
int32_t ceiling_log2(uint32_t value)
{
  uint32_t eax;
  int32_t ecx = 0;

  if (value != 0) {
    eax = value - 1;
    if (eax != 1) {
      do {
        eax >>= 1;
        ecx++;
      } while (eax != 1);
    }
  }
  return ecx + 1;
}

/* Compute the largest power of 2 <= value (0x108df0). Sibling of
 * ceiling_log2 at 0x108dd0; no callers in this binary (xrefs_to empty).
 * Binary shape (from disassembly): param is read via movzx word ptr
 * [ebp+8] (single ushort arg). eax=1 is set unconditionally before the
 * compare; if value<2, that 1 is returned untouched (JL skips the loop).
 * Otherwise ecx=2, then loop: eax=ecx; ecx=eax+eax; while(ecx<=value)
 * repeat. Both eax and ecx stay full 32-bit registers throughout (no
 * truncation on return), preserved as int32_t/uint32_t locals. */
int32_t floor_power2(uint16_t value)
{
  int32_t edx = (int32_t)value;
  int32_t eax = 1;
  int32_t ecx;

  if (edx >= 2) {
    ecx = 2;
    do {
      eax = ecx;
      ecx = eax * 2;
    } while (ecx <= edx);
  }
  return eax;
}

/* Compute the smallest power of 2 >= value (0x108e20). Sibling of
 * floor_power2 at 0x108df0. Binary shape (from disassembly): param is read
 * via movzx word ptr [ebp+8] (single ushort arg). ecx = value; eax = 1
 * unconditionally; if value <= 1 (JLE), that 1 is returned untouched.
 * Otherwise loop: eax <<= 1; while (eax < value) repeat. */
int32_t ceiling_power2(uint16_t value)
{
  int32_t ecx = (int32_t)value;
  int32_t eax = 1;

  if (ecx > 1) {
    do {
      eax <<= 1;
    } while (eax < ecx);
  }
  return eax;
}

/* Integer square root, digit-by-digit (0x108e40). Sibling of ceiling_power2
 * at 0x108e20; no callers found in xrefs_to scan (local index gap, not
 * evidence of dead code). Ghidra's decompile drops the return value and the
 * final rounding step, so this is lifted verbatim from disassembly:
 *   esi=value (single uint arg, [ebp+8]); eax=result=0; edx=bit=0x40000000.
 *   loop: ecx=bit+result; if ecx<=esi: esi-=ecx, result(eax)=ecx+bit;
 *         bit>>=2; result>>=1; while(bit!=0).
 *   tail: if esi>result, result++.
 * Both bit and result stay full 32-bit registers throughout. */
uint32_t integer_square_root(uint32_t value)
{
  uint32_t result = 0;
  uint32_t bit = 0x40000000;
  uint32_t test;

  do {
    test = bit + result;
    if (test <= value) {
      value -= test;
      result = test + bit;
    }
    bit >>= 2;
    result >>= 1;
  } while (bit != 0);

  if (value > result) {
    result++;
  }

  return result;
}
