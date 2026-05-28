/* x87 FPU math helpers matching the original MSVC 7.1 binary.
 * clang -mno-sse compiles sinf/cosf to libm calls (different argument
 * reduction) and fmod to FPREM1 (IEEE remainder, not C fmod). These
 * helpers emit the exact x87 instructions the original binary uses. */
#ifndef X87_MATH_H
#define X87_MATH_H

static __inline float x87_fcos(float val) {
  float r;
  __asm__ __volatile__("fcos" : "=t"(r) : "0"(val));
  return r;
}

static __inline float x87_fsin(float val) {
  float r;
  __asm__ __volatile__("fsin" : "=t"(r) : "0"(val));
  return r;
}

static __inline float x87_fcos_mul(float val, float mul) {
  float r;
  __asm__ __volatile__("fmuls %2\n\tfcos" : "=t"(r) : "0"(val), "m"(mul));
  return r;
}

static __inline float x87_fsin_mul(float val, float mul) {
  float r;
  __asm__ __volatile__("fmuls %2\n\tfsin" : "=t"(r) : "0"(val), "m"(mul));
  return r;
}

static __inline float x87_fsin_msub(float val, float mul, float sub) {
  float r;
  __asm__ __volatile__("fmuls %2\n\tfsubs %3\n\tfsin"
                       : "=t"(r) : "0"(val), "m"(mul), "m"(sub));
  return r;
}

static __inline float x87_fmod(float val, double divisor) {
  float r;
  __asm__ __volatile__("fldl %2\n\tfxch %%st(1)\n\t"
                       "1: fprem\n\tfnstsw %%ax\n\t"
                       "testb $4, %%ah\n\tjnz 1b\n\t"
                       "fstp %%st(1)"
                       : "=t"(r) : "0"(val), "m"(divisor) : "ax");
  return r;
}

#endif /* X87_MATH_H */
