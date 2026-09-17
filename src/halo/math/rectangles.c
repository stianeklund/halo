#include <stdint.h>

/* 0x1089d0 — Set 2D point coordinates.
 * Binary evidence (0x1089d0..0x1089e7, 23 B, cdecl):
 *   MOV  EAX,dword ptr [EBP+0x8]
 *   MOV  CX,word ptr [EBP+0xc]
 *   MOV  DX,word ptr [EBP+0x10]
 *   MOV  word ptr [EAX],CX
 *   MOV  word ptr [EAX+0x2],DX
 */
void set_point2d(int16_t *point, int16_t x, int16_t y)
{
  point[0] = x;
  point[1] = y;
}

/* 0x1089f0 — Offset a 2D point by (dx, dy).
 * Binary evidence (0x1089f0..0x108a07, 23 B, cdecl):
 *   MOV  EAX,dword ptr [EBP+0x8]
 *   MOV  CX,word ptr [EBP+0xc]
 *   MOV  DX,word ptr [EBP+0x10]
 *   ADD  word ptr [EAX],CX
 *   ADD  word ptr [EAX+0x2],DX
 */
void offset_point2d(int16_t *point, int16_t dx, int16_t dy)
{
  point[0] += dx;
  point[1] += dy;
}

/* 0x108a10 — Compute rectangle width (right - left).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary evidence (0x108a10..0x108a24, 20 B, cdecl):
 *   MOV   ECX,dword ptr [EBP+0x8]
 *   XOR   EAX,EAX
 *   MOV   AX,word ptr [ECX+0x6]
 *   MOVSX ECX,word ptr [ECX+0x2]
 *   SUB   EAX,ECX
 */
int rectangle2d_width(const int16_t *rect)
{
  return (int)(uint16_t)rect[3] - (int)rect[1];
}

/* 0x108a30 — Compute rectangle height (bottom - top).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary evidence (0x108a30..0x108a43, 19 B, cdecl):
 *   MOV   ECX,dword ptr [EBP+0x8]
 *   XOR   EAX,EAX
 *   MOV   AX,word ptr [ECX+0x4]
 *   MOVSX ECX,word ptr [ECX]
 *   SUB   EAX,ECX
 */
int rectangle2d_height(const int16_t *rect)
{
  return (int)(uint16_t)rect[2] - (int)rect[0];
}

/* 0x108a50 — Inset a 2D rectangle bounds by (dx, dy).
 * rect layout: {top, left, bottom, right} as int16_t[4].
 * Binary evidence (0x108a50..0x108a6f, 31 B, cdecl):
 *   MOV  EAX,dword ptr [EBP+0x8]
 *   MOV  CX,word ptr [EBP+0xc]
 *   ADD  word ptr [EAX+0x2],CX
 *   SUB  word ptr [EAX+0x6],CX
 *   MOV  CX,word ptr [EBP+0x10]
 *   ADD  word ptr [EAX],CX
 *   SUB  word ptr [EAX+0x4],CX
 */
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
