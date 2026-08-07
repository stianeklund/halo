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
 *   0x800d0  math64_multiply
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

/* 64-bit unsigned multiply: result = a * b, schoolbook over four 16-bit limbs.
 *
 * Confirmed (0x800d0-0x80206):
 *  - cdecl, three stack pointer args: a @[EBP+8], b @[EBP+0xc],
 *    result @[EBP+0x10]. No register args, no return value, RET with no
 *    immediate (caller cleans).
 *  - The guard tests a (TEST EAX / JZ), b (TEST EBX / JZ) then result
 *    (TEST EAX / JNZ body), so the assert condition is `a && b && result`.
 *    The failure block at 0x8011f pushes 0x265a40 "a && b && result",
 *    0x265a54 (the 64bit_math.c __FILE__ string), line 0x5f and TRUE into
 *    display_assert, then system_exit(-1).
 *  - Every limb read is `MOVZX reg, word ptr [...]`; every result store is a
 *    16-bit `MOV word ptr [dst+N], reg`.
 *  - The partial products are 32-bit `IMUL reg,reg` / `IMUL reg,mem`, split
 *    with `AND reg,0xffff` (low half) and `SHR reg,0x10` (high half). SHR, not
 *    SAR, proves the product temporary is *unsigned*.
 *  - The outer loop counter ends `INC EAX / CMP EAX,4 / JC`, i.e. an unsigned
 *    i < 4.
 *  - `a` is re-loaded from [EBP+8] at the top of every outer iteration
 *    (0x80153) — it stays a plain parameter, never enregistered across the
 *    loop.
 *  - Accumulation order inside one outer iteration is fixed by the eight
 *    read-modify-writes at 0x80167/0x80172/0x80185/0x80190/0x801a8/0x801b3/
 *    0x801c1/0x801ce: lo(a[i]*b[0]), hi(a[i]*b[0]), lo(a[i]*b[1]),
 *    hi(a[i]*b[1]), ... into acc[i+j] / acc[i+j+1].
 *
 * Loop shape (this is what takes the match from 69.7% to 100%): the source is
 * a *nested* i/j loop, not a flat i loop with four hand-written limb steps.
 * VC71 fully unrolls the four-iteration inner loop but leaves the outer one
 * rolled, which is exactly the reference: indexed `-0x24(%ebp,%eax,4)`
 * accumulator traffic, with the four b limbs hoisted out by LICM into
 * ESI/EDI and the two spill slots at EBP-0x4/EBP-0x8. Writing the inner steps
 * out by hand instead lets VC71 unroll everything and promote the whole
 * accumulator into registers — 105 instructions of constant-folded arithmetic
 * against the reference's 93, and no indexed memory at all.
 *
 * Two oddities in the original, reproduced deliberately — they are NOT bugs in
 * this lift:
 *
 *  1. The accumulator is seeded with 0,1,2,3,4,5,6 rather than zeros. This is
 *     not a decompiler artifact: the seven stores at 0x800e1-0x80112 are
 *     literal `C7 45 dc 00000000 / C7 45 e0 01000000 / ...` immediates, read
 *     back byte-for-byte from the XBE. Consequently every result limb above
 *     limb 0 is offset by the seed (result[1] comes out one too high, and so
 *     on), so the routine does not actually compute a correct product. It
 *     looks like unfinished scaffolding — this bungie_net TU is Xbox Live
 *     support code that the 2276 beta does not appear to exercise.
 *
 *  2. The accumulator is one element too short for a 4x4-limb product. The
 *     frame is `SUB ESP,0x24` (36 bytes) holding exactly nine dwords: seven at
 *     EBP-0x24..EBP-0xc (the seeded array) plus EBP-0x8 and EBP-0x4, which the
 *     prologue fills with b[3] and b[2] (0x8014d/0x80150). With a seven-element
 *     array `acc[i+j+1]` at i==j==3 addresses EBP-0x8 — the b[3] temporary — so
 *     the last inner step reads b[3] as an accumulator and overwrites it. That
 *     clobber is harmless: b[3] was already consumed by that iteration's
 *     `IMUL ECX,[EBP-0x8]` (0x80194), the loops then exit, and only acc[0..3]
 *     reach `result`.
 *
 *     We declare eight elements so that last access is in bounds, and
 *     deliberately leave acc[7] uninitialised: seeding it costs an extra
 *     `movl $0x0` and drops the match to 94.1%, whereas leaving it out
 *     reproduces the reference's seven init stores and its exact 0x24 frame
 *     (VC71 overlays the b[3] spill on the never-read slot, just as it did for
 *     the original). acc[7] is written but never read, so the indeterminate
 *     value is discarded and behaviour is identical either way.
 *
 * Verified: VC71 100.0% match, 93/93 instructions, operand-normalized 100.0%.
 */
void math64_multiply(const uint16_t *a, const uint16_t *b, uint16_t *result)
{
  uint32_t acc[8];
  uint32_t product;
  uint32_t i;
  uint32_t j;

  acc[0] = 0;
  acc[1] = 1;
  acc[2] = 2;
  acc[3] = 3;
  acc[4] = 4;
  acc[5] = 5;
  acc[6] = 6;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x5f,
                 a && b && result);

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      product = a[i] * b[j];
      acc[i + j] += product & 0xffff;
      acc[i + j + 1] += product >> 16;
    }
  }

  result[0] = (uint16_t)acc[0];
  result[1] = (uint16_t)acc[1];
  result[2] = (uint16_t)acc[2];
  result[3] = (uint16_t)acc[3];
}
