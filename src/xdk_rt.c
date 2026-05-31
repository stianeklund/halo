/* Runtime stubs for XDK MSVC 7.1 compiled TUs.
 * MSVC generates calls to math library functions that clang inlines as x87
 * instructions, and to CRT intrinsics like __ftol2. This file provides the
 * implementations. Compiled with clang only when ENABLE_XDK_HYBRID is ON. */


#undef sinf
#undef cosf
#undef sqrtf
#undef fabsf

float sinf(float x) {
  float r;
  __asm__ __volatile__("fsin" : "=t"(r) : "0"(x));
  return r;
}
float cosf(float x) {
  float r;
  __asm__ __volatile__("fcos" : "=t"(r) : "0"(x));
  return r;
}
float sqrtf(float x) { return __builtin_sqrtf(x); }
float fabsf(float x) { return __builtin_fabsf(x); }

double atan2_(double y, double x) {
    double result;
    __asm__("fpatan" : "=t"(result) : "0"(x), "u"(y) : "st(1)");
    return result;
}

/* MSVC's __CIpow: compute pow(x,y) with both args on the x87 stack.
 * Input: ST(0)=y, ST(1)=x. Output: ST(0)=x^y. */
__attribute__((naked)) void _CIpow(void) {
    __asm__(
        "fxch %st(1)\n\t"
        "fyl2x\n\t"
        "fld %st(0)\n\t"
        "frndint\n\t"
        "fxch %st(1)\n\t"
        "fsub %st(1), %st(0)\n\t"
        "f2xm1\n\t"
        "fld1\n\t"
        "faddp\n\t"
        "fscale\n\t"
        "fstp %st(1)\n\t"
        "ret\n\t"
    );
}

/* MSVC's __ftol2: truncate ST(0) to int32 in EAX.
 * Non-standard calling convention: float on x87 stack, result in EAX.
 * Sets rounding mode to truncation, converts, restores original rounding. */
__attribute__((naked)) void _ftol2(void) {
    __asm__(
        "subl $8, %esp\n\t"
        "fnstcw (%esp)\n\t"
        "movw (%esp), %ax\n\t"
        "orb $0x0c, %ah\n\t"
        "movw %ax, 4(%esp)\n\t"
        "fldcw 4(%esp)\n\t"
        "fistpl 4(%esp)\n\t"
        "fldcw (%esp)\n\t"
        "movl 4(%esp), %eax\n\t"
        "addl $8, %esp\n\t"
        "ret\n\t"
    );
}
