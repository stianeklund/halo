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

/* __chkstk: reserve a large (>1 page) stack frame for the caller.
 * Clang (i386-pc-win32) emits `mov eax, <framesize>; call __chkstk` and — contrary
 * to a long-held assumption — emits NO following `sub esp, eax`.  __chkstk itself
 * must lower ESP by the frame size and relocate the return address, exactly as the
 * original MSVC runtime does (cachebeta.xbe 0x1d90e0).  A bare `ret` left the frame
 * UNALLOCATED: a function's locals/spills aliased live ESP and were clobbered by the
 * next argument push (NULL+0x99 fault in actor_has_accessible_firing_position
 * 0x25a00; corrupted firing-position/aim records in FUN_00025c10 0x25c10).
 * Byte-faithful to 0x1d90e0; Xbox fully commits the thread stack, so no probing. */
__attribute__((naked)) void _chkstk(void) {
    __asm__(
        "test %eax, %eax\n\t"  /* frame size == 0? nothing to do */
        "je 1f\n\t"
        "neg %eax\n\t"         /* eax = -size */
        "add %esp, %eax\n\t"   /* eax = esp - size */
        "add $4, %eax\n\t"     /* account for the return-address slot */
        "xchg %eax, %esp\n\t"  /* esp = new frame top; eax = old esp */
        "mov (%eax), %eax\n\t" /* eax = saved return address */
        "push %eax\n\t"        /* re-push it at the new top */
        "1:\n\t"
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
