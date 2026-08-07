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
 *   0x7ffe0  math64_negate
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

/* 64-bit two's-complement negate: result = -a across four 16-bit limbs.
 *
 * Confirmed (0x7ffe0-0x80062, bytes read back from the XBE):
 *  - REGISTER ARGS, not cdecl. The body reads [ESI]/[ESI+2/4/6] and writes
 *    [EBX]/[EBX+2/4/6] with no prologue, no EBP frame and no load of ESI/EBX
 *    from the stack, and it ends in a bare `RET` (no immediate, nothing to
 *    clean). Only EDI is saved (PUSH EDI at 0x7ffe0 / POP EDI at 0x8005b), so
 *    ESI and EBX are inbound: a@<esi> (read-only), result@<ebx> (written).
 *    Caller-confirmed at 0x80070 (64-bit subtract, same TU, assert line 0x4f):
 *    `MOV ESI,[EBP+0xc]` (its `b` parameter), `SUB ESP,8` + `LEA EBX,[EBP-8]`
 *    (an 8-byte = 4x uint16_t scratch), `CALL 0x7ffe0`, then
 *    `math64_add(a, scratch, result)` — i.e. a - b implemented as a + (-b).
 *    The 8-byte scratch independently confirms the 4x16-bit limb layout.
 *    Second call site 0x802c6 in FUN_00080210. This is an LTCG custom
 *    convention; kb.json carries it as @<esi>/@<ebx> and the build emits the
 *    reverse thunk.
 *  - Guard order is TEST ESI / TEST EBX, so the assert condition is
 *    `a && result` — only two operands (the string at 0x265a84 is literally
 *    "a && result", NOT the "a && b && result" at 0x265a40 that add/multiply/
 *    subtract use). Line 0x3a, same 64bit_math.c __FILE__ string at 0x265a54.
 *  - The borrow lives in EDI: `XOR EDI,EDI` before the guard, and a sticky
 *    `MOV EDI,1` after each of limbs 0..2. It is only ever set, never cleared,
 *    so it means "some lower limb was non-zero". `ADD CX,DI` is a *16-bit* add,
 *    which is why the borrow is a 16-bit type and not an int.
 *  - Each limb re-reads a[i] from memory for the non-zero test
 *    (`MOV AX,[ESI]` ... `MOV [EBX],AX` ... `CMP word ptr [ESI],0`) instead of
 *    reusing the loaded register. `a` and `result` are unrelated pointers, so
 *    the store to result[i] may alias a[i] and the compiler must reload —
 *    i.e. two separate source reads of a[i], store first, test second.
 *  - Limb 0 negates 16-bit (`66 f7 d8` = NEG AX) because there is no carry-in;
 *    limbs 1..3 zero-extend into a 32-bit register (`XOR ECX,ECX` +
 *    `MOV CX,[ESI+2]`), add the borrow 16-bit, then negate 32-bit
 *    (`f7 d9` = NEG ECX, no 0x66 prefix) and store the low 16. That is exactly
 *    the integer-promotion shape of `-sum` on a `uint16_t sum`.
 *  - Limb 0's `+ borrow` is constant-folded away (borrow is provably 0) and
 *    limb 3 computes no borrow-out (dead), i.e. a uniform four-iteration loop
 *    body that VC71 fully unrolls to the constant +0/+2/+4/+6 displacements —
 *    the same treatment math64_add gets, so it is written as a loop here too.
 *
 * The arithmetic is a correct negate, unlike math64_multiply's seeded
 * accumulator: with borrow = 1 the limb value is -(a[i]+1) == ~a[i] (mod
 * 2^16), and borrow is 1 exactly when a lower limb was non-zero, which is the
 * textbook ripple form of ~a + 1.
 */
void math64_negate(const uint16_t *a, uint16_t *result)
{
  uint16_t sum;
  uint16_t borrow;
  int32_t i;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x3a,
                 a && result);

  borrow = 0;
  for (i = 0; i < 4; i++) {
    sum = (uint16_t)(a[i] + borrow);
    result[i] = (uint16_t)-sum;
    if (a[i] != 0) {
      borrow = 1;
    }
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
