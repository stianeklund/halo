/* x87 FPU math helpers matching the original MSVC 7.1 binary.
 * clang -mno-sse compiles sinf/cosf to libm calls (different argument
 * reduction) and fmod to FPREM1 (IEEE remainder, not C fmod). These
 * helpers emit the exact x87 instructions the original binary uses. */
#ifndef X87_MATH_H
#define X87_MATH_H

#if defined(_MSC_VER) && !defined(__clang__)
/* VC71 verify lane only (the shipping clang build takes the asm-volatile
 * branches below; clang defines _MSC_VER under -target i386-pc-win32, hence
 * the !__clang__ test).  MSVC 7.1 refuses to inline any function containing
 * an __asm block, so __asm helper bodies compile to real CALL sites where
 * the original binary has a bare inline FCOS/FSIN/FSQRT.  Use the compiler
 * intrinsics instead — #pragma intrinsic forces inline x87 emission
 * regardless of /Oi — so the lane emits the same single instruction the
 * original does. */
double __cdecl cos(double);
double __cdecl sin(double);
double __cdecl sqrt(double);
double __cdecl atan2(double, double);
#pragma intrinsic(cos, sin, sqrt, atan2)
#endif

static __inline float x87_fcos(float val) {
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)cos((double)val); /* VC71 intrinsic: FLD dword; FCOS */
#else
  float r;
  __asm__ __volatile__("fcos" : "=t"(r) : "0"(val));
  return r;
#endif
}

static __inline float x87_fsin(float val) {
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)sin((double)val); /* VC71 intrinsic: FLD dword; FSIN */
#else
  float r;
  __asm__ __volatile__("fsin" : "=t"(r) : "0"(val));
  return r;
#endif
}

/* Double-input FSIN/FCOS: FLD QWORD; FSIN/FCOS. Matches the original binary's
 * sin/cos of a double-precision constant (e.g. the debug circle angle), keeping
 * the argument at double width rather than narrowing to float first. */
static __inline double x87_fsin_d(double val) {
  double r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld QWORD PTR [val]
    fsin
    fstp QWORD PTR [r]
  }
#else
  __asm__ __volatile__("fsin" : "=t"(r) : "0"(val));
#endif
  return r;
}

static __inline double x87_fcos_d(double val) {
  double r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld QWORD PTR [val]
    fcos
    fstp QWORD PTR [r]
  }
#else
  __asm__ __volatile__("fcos" : "=t"(r) : "0"(val));
#endif
  return r;
}

static __inline float x87_fcos_mul(float val, float mul) {
  float r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fmul DWORD PTR [mul]
    fcos
    fstp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fmuls %2\n\tfcos" : "=t"(r) : "0"(val), "m"(mul));
#endif
  return r;
}

static __inline float x87_fsin_mul(float val, float mul) {
  float r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fmul DWORD PTR [mul]
    fsin
    fstp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fmuls %2\n\tfsin" : "=t"(r) : "0"(val), "m"(mul));
#endif
  return r;
}

static __inline void x87_fsincos(float val, float *sin_out, float *cos_out) {
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fld st(0)
    fsin
    fxch st(1)
    fcos
    mov eax, DWORD PTR [cos_out]
    fstp DWORD PTR [eax]
    mov eax, DWORD PTR [sin_out]
    fstp DWORD PTR [eax]
  }
#else
  __asm__ __volatile__(
    "flds %2\n\t"
    "fld %%st(0)\n\t"
    "fsin\n\t"
    "fxch %%st(1)\n\t"
    "fcos\n\t"
    "fstps %1\n\t"
    "fstps %0"
    : "=m"(*sin_out), "=m"(*cos_out)
    : "m"(val)
    : "memory");
#endif
}

static __inline float x87_fsin_msub(float val, float mul, float sub) {
  float r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fmul DWORD PTR [mul]
    fsub DWORD PTR [sub]
    fsin
    fstp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fmuls %2\n\tfsubs %3\n\tfsin"
                       : "=t"(r) : "0"(val), "m"(mul), "m"(sub));
#endif
  return r;
}

static __inline float x87_fmod(float val, double divisor) {
  float r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fld QWORD PTR [divisor]
    fxch st(1)
x87_fmod_loop:
    fprem
    fnstsw ax
    test ah, 4
    jnz x87_fmod_loop
    fstp st(1)
    fstp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fldl %2\n\tfxch %%st(1)\n\t"
                       "1: fprem\n\tfnstsw %%ax\n\t"
                       "testb $4, %%ah\n\tjnz 1b\n\t"
                       "fstp %%st(1)"
                       : "=t"(r) : "0"(val), "m"(divisor) : "ax");
#endif
  return r;
}

static __inline float x87_fptan(float val) {
  float r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fptan
    fstp st(0)
    fstp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"(val));
#endif
  return r;
}

/* Round a float to the nearest 32-bit integer using the FPU's current
 * rounding mode (round-to-nearest by default): a bare FLD; FISTP, with no
 * control-word change. This differs from a C (int) cast, which MSVC/clang
 * compile to a truncating conversion (_ftol2 / round-toward-zero). */
static __inline int x87_round_to_int(float val) {
  int r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [val]
    fistp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fistpl %0" : "=m"(r) : "t"(val) : "st");
#endif
  return r;
}

/* atan2 via x87 FPATAN: FLD a; FLD b; FPATAN computes atan2(a, b) in ST0.
 * Matches the original binary's inline fpatan on float inputs rather than a
 * libm atan2f call. */
static __inline float x87_fatan2f(float a, float b) {
  float r;
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fld DWORD PTR [a]
    fld DWORD PTR [b]
    fpatan
    fstp DWORD PTR [r]
  }
#else
  __asm__ __volatile__("fpatan" : "=t"(r) : "0"(b), "u"(a) : "st(1)");
#endif
  return r;
}

static __inline float x87_sqrt(float val) {
#if defined(_MSC_VER) && !defined(__clang__)
  return (float)sqrt((double)val); /* VC71 intrinsic: FLD dword; FSQRT */
#else
  float r;
  __asm__ __volatile__("fsqrt" : "=t"(r) : "0"(val));
  return r;
#endif
}

#endif /* X87_MATH_H */
