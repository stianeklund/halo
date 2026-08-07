/*
 * bungie_net/64bit_math.c — 64-bit unsigned integer math on 16-bit limbs
 * XBE source: c:\halo\SOURCE\bungie_net\common\64bit_math.c
 *
 * Confirmed TU identity: the assert at 0x7ff5f pushes the __FILE__ string at
 * 0x265a54, "c:\halo\SOURCE\bungie_net\common\64bit_math.c". The neighbouring
 * functions 0x7ffe0 / 0x80070 / 0x800d0 share that string (0x265a81,
 * "a && result", is one of their assert messages), so they belong here too.
 *
 * Operands are 4-element arrays of 16-bit limbs in little-endian order
 * (index 0 = least significant). Every limb access in the disassembly is a
 * MOVZX of a `word ptr` at +0/+2/+4/+6 and every store is a 16-bit
 * `MOV word ptr [dst+N], AX`.
 *
 * NOTE (resolved): these functions were originally filed under the object
 * label "tiff_file.obj", an adjacent-linkage grouping artifact that made
 * `maintain.py` try to move them into src/halo/bitmaps/tiff_file.c. kb.json
 * now carries a real "64bit_math.obj" entry mapped to this file, so that no
 * longer applies.
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x7ff40  math64_add
 *   0x7ffe0  math64_negate
 *   0x80070  math64_subtract
 *   0x800d0  math64_multiply
 *   0x80210  math64_divide
 */

#include "common.h"

/* An 8-byte limb group. math64_divide's working register is two of these laid
 * out contiguously, and the reference copies each group with a single
 * load/load/store/store dword pair rather than four 16-bit moves, so the
 * copies have to be whole-object assignments and not element loops. */
typedef struct {
  uint16_t limb[4];
} math64_half_t;

/* The 128-bit shift-subtract working register: half[0] is the quotient
 * accumulator, half[1] the remainder accumulator, and the shift step walks all
 * eight limbs through `limb`. The union is what lets one object be addressed
 * both ways; the reference's shift loop indexes `word ptr [EBP+ECX*2-0x2c]`
 * for ECX in 0..7, i.e. straight across the two halves. */
typedef union {
  math64_half_t half[2];
  uint16_t limb[8];
} math64_work_t;

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

/* 64-bit subtract: result = a - b, implemented as a + (-b).
 *
 * Confirmed (0x80070-0x800ca):
 *  - cdecl, three stack pointer args: a @[EBP+8]->EDI, b @[EBP+0xc]->ESI,
 *    result @[EBP+0x10]. Bare `RET` with no immediate, so the caller cleans;
 *    no register args of its own and no return value.
 *  - Guard order is TEST EDI (a) / TEST ESI (b) / TEST EAX (result), so the
 *    assert condition is `a && b && result`. The failure block at 0x8008e
 *    pushes 0x265a40 "a && b && result", 0x265a54 (the 64bit_math.c __FILE__
 *    string), line 0x4f and TRUE into display_assert, then system_exit(-1).
 *  - `SUB ESP,0x8` (0x80073) reserves exactly one 8-byte local at EBP-0x8.
 *    Eight bytes is four 16-bit limbs, which independently corroborates the
 *    limb layout the rest of this TU uses.
 *  - 0x800ab-0x800ae is the register-argument call into math64_negate:
 *    ESI already holds b (loaded at 0x80078, never rewritten on the path to
 *    the call) and `LEA EBX,[EBP-0x8]` points at the scratch, i.e.
 *    math64_negate(b, negated_b) — it is `b` that gets negated, not `a`.
 *  - 0x800b3-0x800c1 is the cdecl call into math64_add, pushed in reverse
 *    argument order: PUSH EAX = [EBP+0x10] (result), PUSH ECX = LEA [EBP-0x8]
 *    (the scratch), PUSH EDI = [EBP+8] (a), then `ADD ESP,0xc` for the three
 *    dwords. So math64_add(a, negated_b, result), and negate strictly precedes
 *    add.
 *  - EDI is loaded at 0x8007c and consumed 15 instructions later at 0x800bb
 *    with no intervening write; likewise ESI from 0x80078 to the call at
 *    0x800ae. Neither is a decompiler register-aliasing artifact.
 *
 * Note that because math64_negate is a true two's-complement negate (see
 * above), this is a correct modular subtract — unlike math64_multiply, whose
 * seeded accumulator makes it produce wrong products.
 *
 * Match note: the call into math64_negate is the whole gap. The reference
 * passes its two arguments in ESI/EBX, which is an MSVC LTCG custom calling
 * convention we cannot ask VC71 to reproduce from source (`__fastcall` is
 * ECX/EDX, not ESI/EBX), so our compile necessarily emits a two-push cdecl
 * call plus the register loads the reference folds away. This is the known
 * @<reg>-call-site ceiling, not a structural defect in the lift. Behaviour is
 * proven separately by unicorn equivalence.
 */
void math64_subtract(const uint16_t *a, const uint16_t *b, uint16_t *result)
{
  uint16_t negated_b[4];

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x4f,
                 a && b && result);

  math64_negate(b, negated_b);
  math64_add(a, negated_b, result);
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

/* 64-bit division by restoring shift-subtract: 64 iterations over a 128-bit
 * working register. Both outputs are optional.
 *
 * Confirmed (0x80210-0x80326):
 *  - cdecl, FOUR stack args and no return value: [EBP+8]->ESI, [EBP+0xc]->EDI,
 *    [EBP+0x10], [EBP+0x14]; bare `RET`, caller cleans. Both call sites live in
 *    FUN_00080fc0 (a modular-exponentiation loop) at 0x81018 and 0x81041, each
 *    pushing four dwords with a literal `PUSH 0x0` for arg3 -- so arg3 is
 *    nullable by design, and the `ADD ESP,0x1c` that follows is the combined
 *    cleanup for those 16 bytes plus the 12 left by the preceding
 *    math64_multiply call.
 *  - Guard order is `CMP ESI,EBX(=0) / JZ` then `CMP EDI,EBX / JNZ body`, so
 *    the assert tests arg1 then arg2. Its message string at 0x265a90 is
 *    literally "numerator && denominator" (read back from the XBE), which pins
 *    arg1 = numerator and arg2 = denominator. __FILE__ is the usual 0x265a54
 *    64bit_math.c string; line 0x7c.
 *  - `SUB ESP,0x2c` = 44 bytes: work[8] at EBP-0x2c (16), the subtract result
 *    at EBP-0x1c (8), four dead bytes at EBP-0x14, the inlined negate scratch
 *    at EBP-0x10 (8) and the trial copy at EBP-0x8 (8). The dead four bytes are
 *    not a source local -- see the sign-test bullet below, which is what makes
 *    the compiler reserve them.
 *    (check_lift_hazards' frame-size heuristic reports a 32-byte gap here. It
 *    is a false positive: _sum_locals only knows built-in type keywords, so it
 *    counts the three scalars and skips work/trial/difference, whose types are
 *    the file-local typedefs above. 16+8+8 aggregate + 8 inlined scratch + 4
 *    pad = 44, and VC71 emits `sub esp,0x2c` byte-identical to the reference.)
 *  - The 64-iteration counter is kept in the *parameter* slot [EBP+8]
 *    (`MOV dword ptr [EBP+8],0x40` at 0x80268, `DEC dword ptr [EBP+8] / JNZ` at
 *    0x802f6): numerator is enregistered in ESI for the whole body, so MSVC
 *    reuses its now-free home slot as scratch. That is a register-allocation
 *    artifact, not a second use of the parameter.
 *  - Both loops end `CMP reg,N / JC`, i.e. *unsigned* counters. The shift
 *    accumulator ends `SHR EAX,0x10` (not SAR), so the carry is unsigned too --
 *    unlike math64_add above, whose SETG proves a signed accumulator.
 *  - The trial subtraction is math64_subtract INLINED, not a call: 0x80296's
 *    `TEST ESI,ESI` is all that survives of subtract's own
 *    `a && b && result` guard (a and result are address-of-local, provably
 *    non-null, so only b == numerator is still tested), the failure block at
 *    0x802a6 pushes subtract's line 0x4f and its 0x265a40 message, and the body
 *    is subtract's exact `math64_negate(b, scratch)` +
 *    `math64_add(a, scratch, result)` pair at 0x802c6 / 0x802d7. So the source
 *    calls math64_subtract and VC71 inlines it; we write the call.
 *  - The sign test is `MOV EAX,dword ptr [EBP-0x16] / TEST AH,AH / JS`.
 *    EBP-0x16 is &difference.limb[3], so AH is that limb's high byte, i.e. bit
 *    63 of the 64-bit difference: the branch keeps the subtraction when the
 *    result is non-negative. Note the load is a *dword* at an address only two
 *    bytes below the end of an 8-byte object -- it deliberately overruns into
 *    EBP-0x14. That pins the source form: writing the test as a signed
 *    comparison, `(int16_t)difference.limb[3] >= 0`, makes VC71 emit
 *    `cmpw $0,...` on the last local and the frame comes out 0x28, four bytes
 *    short. Writing it as the high-bit mask below makes VC71 widen the load to
 *    a dword, which in turn forces it to reserve four bytes of trailing frame
 *    so the overrun stays inside the frame -- reproducing both the reference's
 *    `MOV EAX,dword / TEST AH,AH / JS` and its exact `SUB ESP,0x2c`. Two
 *    independent reference artifacts fall out of one source choice, so the mask
 *    form is the recovered original, not a score tweak.
 *  - `INC word ptr [EBP-0x2c]` at 0x802ec is the quotient-bit set: the register
 *    was just shifted left, so limb 0's bit 0 is clear and ++ sets it.
 *  - Each 8-byte group copy is load/load/store/store of dwords
 *    (0x80298, 0x802e6, 0x80306, 0x80318), never four 16-bit moves, which is
 *    why math64_half_t exists and the copies are whole-object assignments.
 *  - At the tail EDI still holds the low dword of the remainder accumulator
 *    (loaded at 0x80298 every iteration, rewritten at 0x802e6 when the
 *    subtraction is kept), so `MOV dword ptr [EAX],EDI` at 0x8031b is a CSE of
 *    that value and not a fifth variable.
 *
 * ORIGINAL BUG, reproduced deliberately -- the operands are swapped. The
 * working register is seeded from `denominator` (0x80246 copies from EDI =
 * arg2) and the value trial-subtracted each iteration is `numerator` (ESI =
 * arg1), so this computes denominator / numerator, not numerator /
 * denominator. FUN_00080fc0 calls it as
 * `math64_divide(product, modulus, NULL, &accumulator)` plainly intending
 * `product % modulus`, and gets `modulus % product` instead. This is the same
 * flavour of unfinished scaffolding as math64_multiply's seeded accumulator
 * above; the 2276 beta does not appear to exercise this bungie_net TU. The
 * parameter names are kept as the assert string spells them so that `#cond`
 * reproduces "numerator && denominator" byte for byte.
 */
void math64_divide(const uint16_t *numerator, const uint16_t *denominator,
                   uint16_t *quotient, uint16_t *remainder)
{
  math64_work_t work;
  math64_half_t trial;
  math64_half_t difference;
  uint32_t carry;
  uint32_t i;
  int32_t count;

  assert_halt_at("c:\\halo\\SOURCE\\bungie_net\\common\\64bit_math.c", 0x7c,
                 numerator && denominator);

  /* Seed the low half with the dividend and clear the high half. */
  for (i = 0; i < 4; i++) {
    work.limb[i] = denominator[i];
    work.limb[i + 4] = 0;
  }

  count = 0x40;
  do {
    /* Shift the whole 128-bit register left by one. */
    carry = 0;
    for (i = 0; i < 8; i++) {
      carry = carry + work.limb[i] * 2;
      work.limb[i] = (uint16_t)carry;
      carry >>= 16;
    }

    trial = work.half[1];
    math64_subtract(trial.limb, numerator, difference.limb);

    /* Bit 63 clear = the difference is non-negative, i.e. the divisor fit:
     * keep it and set the quotient bit. See the sign-test bullet above for why
     * this is the mask form and not a signed comparison. */
    if ((difference.limb[3] & 0x8000) == 0) {
      work.limb[0]++;
      work.half[1] = difference;
    }
  } while (--count != 0);

  if (quotient) {
    *(math64_half_t *)quotient = work.half[0];
  }
  if (remainder) {
    *(math64_half_t *)remainder = work.half[1];
  }
}
