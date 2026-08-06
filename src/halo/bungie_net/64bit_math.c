/*
 * bungie_net/64bit_math.c — 64-bit unsigned integer math on 16-bit limbs
 * XBE source: c:\halo\SOURCE\bungie_net\common\64bit_math.c
 *
 * Confirmed TU identity: the assert at 0x7ff5f pushes the __FILE__ string at
 * 0x265a54, "c:\halo\SOURCE\bungie_net\common\64bit_math.c". The neighbouring
 * unported functions 0x7ffe0 / 0x80070 / 0x800d0 share that string (0x265a81,
 * "a && result", is one of their assert messages), so they belong here too.
 *
 * Operands are 4-element arrays of 16-bit limbs in little-endian order
 * (index 0 = least significant). Every limb access in the disassembly is a
 * MOVZX of a `word ptr` at +0/+2/+4/+6 and every store is a 16-bit
 * `MOV word ptr [dst+N], AX`.
 *
 * NOTE: kb.json currently files 0x7ff40 under the object label "tiff_file.obj".
 * That label is an adjacent-linkage grouping artifact, not the real TU — these
 * are 64bit_math.c functions. Because of it, `maintain.py` (which derives the
 * expected source file from the kb object -> source mapping) will try to move
 * this function into src/halo/bitmaps/tiff_file.c. Splitting a real
 * "64bit_math.obj" entry out of tiff_file.obj is the proper fix and is left to
 * a dedicated kb.json object-partition change.
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x7ff40  math64_add
 */

#include "common.h"

/* 64-bit unsigned add: result = a + b, carry propagated across four 16-bit
 * limbs.
 *
 * Confirmed (0x7ff40-0x7ffd5):
 *  - cdecl, three stack pointer args: a @[EBP+8]->ESI, b @[EBP+0xc]->EBX,
 *    result @[EBP+0x10]->EDI. No register args and no return value.
 *  - The null guard tests run in argument order (TEST ESI / TEST EBX /
 *    TEST EDI), so the assert condition is `a && b && result`. Its failure
 *    block at 0x7ff5b is laid out ahead of the body and reached by
 *    fall-through; the body starts at the aligned 0x7ff78.
 *  - The carry is materialised with XOR ECX,ECX / CMP EAX,0xffff / SETG CL.
 *    SETG (not SETA) proves a *signed* accumulator, so the sum is an `int` and
 *    0xffff is a plain int constant.
 *  - Limbs 1..3 accumulate as (carry + a[i]) + b[i]: `ADD ECX,EDX` folds the
 *    carry into a[i] first, then `ADD EAX,ECX` adds b[i].
 *  - Limb 0 has no carry-in (no `ADD ECX,EDX` before the accumulate) and limb 3
 *    computes no carry-out (no XOR/CMP/SETG after its store) — i.e. exactly a
 *    uniform loop body with carry=0 constant-folded into the first iteration
 * and the last iteration's dead carry eliminated. VC71 fully unrolls the loop
 *    below to the same constant +0/+2/+4/+6 displacements the reference uses,
 *    which is why it is written as a loop rather than four copies.
 *
 * Match note (VC71 78.2%, 55 candidate vs 55 reference insns, dp_lcs 90.9%):
 * the whole gap is register allocation. VC71 narrows limb 3 to 16-bit
 * memory-operand adds (`addw 0x6(%esi),%dx`) because the final carry is dead;
 * that pins the carry in EDX and forces a `movl %edx,%eax` shuffle per limb,
 * whereas the reference keeps the carry in ECX and accumulates into the b-limb
 * register. Commutative reorderings of the sum, loop vs. straight-line, and a
 * single-accumulator form all compile to byte-identical VC71 output, and /O2
 * beats /O1 and /Og /Os here. Behaviour is proven separately: unicorn
 * equivalence is 100/100 seeds, 0 diverged, high confidence, 80.7% coverage.
 */
void math64_add(const uint16_t *a, const uint16_t *b, uint16_t *result)
{
  int32_t sum;
  int32_t carry;
  int32_t i;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x21,
                 a && b && result);

  carry = 0;
  for (i = 0; i < 4; i++) {
    sum = carry + a[i] + b[i];
    result[i] = (uint16_t)sum;
    carry = (sum > 0xffff);
  }
}
