/*
 * xbox_crt.c — LIBCMT/XAPI CRT helper replacements
 *
 * These are compact SEH functions from the original LIBCMT/XAPI libraries.
 * The originals use __SEH_prolog/__SEH_epilog (compact MSVC SEH thunks).
 * We replace them with __try/__except using clang's built-in SEH support
 * (-target i386-pc-win32 implies ms-extensions; __try/__except compiles
 * without -fms-extensions on Windows targets).
 *
 * The SEH frame shape differs from the MSVC compact form, but semantics
 * are preserved: the __try body matches the original logic exactly; the
 * __except handler catches access violations on bad pointer arguments.
 *
 * See docs/seh-handling.md for the full design rationale.
 */

/*
 * Xbox kernel (xboxkrnl) NT import declarations and types.
 *
 * NTSTATUS/ULONG/HANDLE and the OBJECT_ATTRIBUTES/IO_STATUS_BLOCK layouts are
 * not defined in common.h, so each TU that touches the NT API redefines them
 * locally (mirrors src/halo/cseries/xcontent.c).  These resolve to the
 * xboxkrnl.exe import thunks (NtOpenFile @202, NtQueryVolumeInformationFile
 * @218, NtClose @187), not in-binary FUN_ functions.
 */
typedef long NTSTATUS;
typedef unsigned long ULONG;
typedef void *HANDLE;

typedef struct {
  unsigned short Length;
  unsigned short MaximumLength;
  char *Buffer;
} XAPI_ANSI_STRING;

typedef struct {
  HANDLE RootDirectory; /* +0 */
  XAPI_ANSI_STRING *ObjectName; /* +4 */
  ULONG Attributes; /* +8 */
} XAPI_OBJECT_ATTRIBUTES;

typedef struct {
  NTSTATUS Status;
  ULONG *Information;
} XAPI_IO_STATUS_BLOCK;

extern NTSTATUS __stdcall NtOpenFile(HANDLE *FileHandle, ULONG DesiredAccess,
                                     XAPI_OBJECT_ATTRIBUTES *ObjectAttributes,
                                     XAPI_IO_STATUS_BLOCK *IoStatusBlock,
                                     ULONG ShareAccess, ULONG OpenOptions);

extern NTSTATUS __stdcall NtQueryVolumeInformationFile(
  HANDLE FileHandle, XAPI_IO_STATUS_BLOCK *IoStatusBlock, void *FsInformation,
  ULONG Length, ULONG FsInformationClass);

extern void __stdcall NtClose(HANDLE Handle);

extern void __stdcall RtlInitAnsiString(XAPI_ANSI_STRING *DestinationString,
                                        const char *SourceString);

/* Additional NT imports used by FUN_001d7d84 (section write-to-file):
 *   NtCreateFile           @190  _NtCreateFile@36           (9 dword args)
 *   NtQueryInformationFile @211  _NtQueryInformationFile@20 (5 dword args)
 *   NtWriteFile            @236  _NtWriteFile@32            (8 dword args)
 * AllocationSize/ByteOffset are LARGE_INTEGER* (always NULL here, passed as
 * pointers); we only ever pass 0, so model them as void*. */
extern NTSTATUS __stdcall NtCreateFile(HANDLE *FileHandle, ULONG DesiredAccess,
                                       XAPI_OBJECT_ATTRIBUTES *ObjectAttributes,
                                       XAPI_IO_STATUS_BLOCK *IoStatusBlock,
                                       void *AllocationSize, ULONG FileAttributes,
                                       ULONG ShareAccess, ULONG CreateDisposition,
                                       ULONG CreateOptions);

extern NTSTATUS __stdcall NtQueryInformationFile(HANDLE FileHandle,
                                                 XAPI_IO_STATUS_BLOCK *IoStatusBlock,
                                                 void *FileInformation, ULONG Length,
                                                 ULONG FileInformationClass);

extern NTSTATUS __stdcall NtWriteFile(HANDLE FileHandle, HANDLE Event,
                                      void *ApcRoutine, void *ApcContext,
                                      XAPI_IO_STATUS_BLOCK *IoStatusBlock,
                                      void *Buffer, ULONG Length, void *ByteOffset);

/* Additional NT/Hal imports used by FUN_001d7b37 (recursive delete),
 * XapiMapLetterToDirectory (XapiMapLetterToDirectory) and XapiBootToDash (XapiBootToDash):
 *   NtQueryDirectoryFile @207 _NtQueryDirectoryFile@40 (10 dword args)
 *   NtSetInformationFile @226 _NtSetInformationFile@20 (5 dword args)
 *   KeQuerySystemTime    @128 _KeQuerySystemTime@4     (1 dword arg)
 *   IoCreateSymbolicLink @67  _IoCreateSymbolicLink@8  (2 dword args)
 *   HalReturnToFirmware  @49  _HalReturnToFirmware@4   (1 dword arg)
 * FileName/FileMask are ANSI_STRING*; always NULL here, modelled as void*. */
extern NTSTATUS __stdcall NtQueryDirectoryFile(
  HANDLE FileHandle, HANDLE Event, void *ApcRoutine, void *ApcContext,
  XAPI_IO_STATUS_BLOCK *IoStatusBlock, void *FileInformation, ULONG Length,
  ULONG FileInformationClass, ULONG ReturnSingleEntry, ULONG RestartScan);

extern NTSTATUS __stdcall NtSetInformationFile(
  HANDLE FileHandle, XAPI_IO_STATUS_BLOCK *IoStatusBlock, void *FileInformation,
  ULONG Length, ULONG FileInformationClass);

extern void __stdcall KeQuerySystemTime(void *CurrentTime);

extern NTSTATUS __stdcall IoCreateSymbolicLink(void *SymbolicLinkName,
                                               void *DeviceName);

extern __declspec(noreturn) void __stdcall HalReturnToFirmware(int Routine);

/* The XAPILIB in-binary helpers / CRT primitives used below
 *   crt_sprintf  (0x1d90f0), FUN_001d8766 (0x1d8766, CompareString primitive),
 *   XapiMapLetterToDirectory (XapiMapLetterToDirectory), FUN_001d789a (safe strncpy),
 *   FUN_001d8aef (XeLoadSection wrapper), XGetSectionSize (XGetSectionSize),
 *   FUN_001d8b10 (XeUnloadSection wrapper)
 * are declared by build/generated/decl.h (force-included via common.h) from
 * their kb.json decls, so no local prototypes are needed here. */

/*
 * Auto-power-down timer support (XAPILIB).
 *
 * KTIMER (0x28 bytes) and KDPC (0x20 bytes) are kernel dispatcher objects
 * living at fixed XBE globals; we treat them as opaque storage and only ever
 * pass their addresses to the kernel.  KeSetTimer takes the relative DueTime
 * as a LARGE_INTEGER (64-bit) passed by value — MSVC pushes it as two dwords,
 * LowPart then HighPart, matching the original's
 *   PUSH 0x6329c0; PUSH 0xffffffcd; PUSH 0xb5659000; PUSH 0x6329e0
 * push sequence.  XAPI_TIMER_LARGE_INTEGER mirrors the LowPart/HighPart layout.
 */
typedef struct {
  unsigned long LowPart;
  long HighPart;
} XAPI_TIMER_LARGE_INTEGER;

/* xboxkrnl imports (resolved via xboxkrnl.exe import library, not in-binary):
 *   KeInitializeDpc    @107  _KeInitializeDpc@12     (3 dword args)
 *   KeInitializeTimerEx@113  _KeInitializeTimerEx@8  (2 dword args)
 *   KeSetTimer         @149  _KeSetTimer@16          (timer + 8-byte due + dpc)
 * DeferredRoutine is a kernel DPC callback; we only ever pass its address. */
extern void __stdcall KeInitializeDpc(void *Dpc, void *DeferredRoutine,
                                      void *DeferredContext);
extern void __stdcall KeInitializeTimerEx(void *Timer, int Type);
extern unsigned char __stdcall KeSetTimer(void *Timer,
                                          XAPI_TIMER_LARGE_INTEGER DueTime,
                                          void *Dpc);

/* xboxkrnl imports used by the XAPILIB heap routines (FUN_001d6ca8 free,
 * FUN_001d703b realloc) and their SEH cleanup funclets.  Resolved via the
 * xboxkrnl.exe import library (ordinals confirmed against src/xboxkrnl.exe.def):
 *   RtlEnterCriticalSection @277 _RtlEnterCriticalSection@4  (1 dword arg)
 *   RtlLeaveCriticalSection @294 _RtlLeaveCriticalSection@4  (1 dword arg)
 *   NtFreeVirtualMemory     @199 _NtFreeVirtualMemory@12     (3 dword args)
 *   RtlCompareMemoryUlong   @269 _RtlCompareMemoryUlong@12   (3 dword args)
 *   RtlRaiseException       @302 _RtlRaiseException@4        (1 dword arg)
 * The CRITICAL_SECTION is opaque to us; we only ever pass its address (the
 * heap stores it at heap_base+0x580). */
extern void __stdcall RtlEnterCriticalSection(void *CriticalSection);
extern void __stdcall RtlLeaveCriticalSection(void *CriticalSection);
extern NTSTATUS __stdcall NtFreeVirtualMemory(void **BaseAddress,
                                              ULONG *RegionSize,
                                              ULONG FreeType);
extern ULONG __stdcall RtlCompareMemoryUlong(void *Source, ULONG Length,
                                             ULONG Pattern);
extern void __stdcall RtlRaiseException(void *ExceptionRecord);

/* In-binary XAPILIB heap helpers (unported; declared via build/generated/decl.h
 * from their kb.json decls so they can be called by name and the build
 * generates forwarding thunks):
 *   FUN_001d4a34 (RET 0x10, 4 args, __stdcall) — insert freed chunk into bucket
 *   FUN_001d5598 (RET 0xc,  3 args, __stdcall) — large-chunk free path
 *   FUN_001d4cd9 (RET 0xc,  3 args, __stdcall) — huge-chunk free path
 *   FUN_001d4ec6 (RET 0x14, 5 args, __stdcall) — in-place grow attempt
 *   FUN_001d4dd3 (RET 4,    1 arg,  __stdcall) — chunk header lookup
 *   FUN_001d5c66 (SEH,      3 args, __stdcall) — allocate replacement chunk
 *   FUN_001da290 (memmove)                     — already typed in kb.json */

/*
 * FUN_001d7817  (XAPI case-insensitive string compare, 68 bytes)
 *
 * Locale-aware lstrcmpiA implementation. Tries xCompareStringA (internal
 * CompareString) first; on failure (returns 0) falls back to crt_stricmp.
 * Maps CompareString result (1=LESS,2=EQUAL,3=GREATER) to strcmp-style
 * (-1, 0, +1) via result-2.  NULL pointers are handled before the
 * fallback: NULL < non-NULL, NULL == NULL.
 *
 * Confirmed:
 *   - __stdcall: RET 0x8 pops 2 args (a@[EBP+8], b@[EBP+C])
 *   - ESI saves param b across calls
 *   - NEG EAX; SBB EAX,EAX pattern for null-a path: -1 if b!=0, 0 if both null
 *   - xCompareStringA(1, a, -1, b, -1): 5 stack args, __stdcall
 *   - crt_stricmp(a, b): __cdecl fallback (POP ECX; POP ECX cleanup)
 */
int __stdcall FUN_001d7817(const char *a, const char *b)
{
  int result;

  result = xCompareStringA(1, a, -1, b, -1);
  if (result != 0) {
    return result - 2;
  }

  if (a == 0) {
    if (b != 0)
      return -1;
    return 0;
  }

  if (b == 0) {
    return 1;
  }

  return crt_stricmp(a, b);
}

/*
 * FUN_001d789a  (XAPI strncpy helper, 101 bytes)
 *
 * Copies up to `count` characters from `src` to `dst`.  Stops at NUL or
 * when `count` is exhausted.  If `count` is non-zero on entry and the loop
 * exits with chars remaining (src ran short), the last written byte is
 * backed up by one and overwritten with NUL — matching the original loop
 * shape.  Returns `dst`.  On access violation returns NULL.
 *
 * Confirmed:
 *   - __stdcall: RET 0xC pops 3 args (dst@[EBP+8], src@[EBP+C],
 *     count@[EBP+10])
 *   - SEH table at 0x2c1f28: filter = 0x1d78ea (XOR EAX,EAX;INC EAX;RET
 *     → EXCEPTION_EXECUTE_HANDLER=1), handler restores ESP and returns 0
 *   - Loop uses EBX=0 sentinel (XOR EBX,EBX), ECX=src ptr, EAX=dst ptr
 *   - Tracks both ptr copies in the SEH frame ([EBP-0x1c]/[EBP-0x20])
 *
 * Inferred:
 *   - Called from XapiMapLetterToDirectory, XLaunchNewImageA — XAPI layer
 *   - Function is a safe strncpy variant with access-violation guard
 */
char *__stdcall FUN_001d789a(char *dst, const char *src, int count)
{
  __try {
    char *d;
    const char *s;
    d = dst;
    s = src;
    if (count != 0) {
      while (count != 0) {
        if (*s == '\0') {
          if (count != 0)
            goto done_null;
          break;
        }
        *d = *s;
        d++;
        s++;
        count--;
      }
      d--;
    done_null:
      *d = '\0';
    }
  } __except (1) {
    return 0;
  }
  return dst;
}

/*
 * FUN_001d7cb4  (XAPI open-file-and-read helper, 0x6b bytes)
 *
 * Opens a file by ANSI name and, on success, hands the open handle plus a
 * 328-byte scratch buffer to FUN_001d7b37 (the actual reader), then closes
 * the handle.  Returns FUN_001d7b37's result, or the NtOpenFile NTSTATUS on
 * open failure.
 *
 * Confirmed (from delinked FUN_001d7cb4.obj disasm):
 *   - __stdcall, RET 0x4, one pointer argument (filename ANSI_STRING*) at
 *     [EBP+8].
 *   - ESI saves the running NTSTATUS/result across the call chain.
 *   - The [EBP+8] parameter slot is REUSED as NtOpenFile's FileHandle OUT
 *     param: LEA EAX,[EBP+8]; PUSH EAX. After the call it holds the opened
 *     HANDLE, which is then passed to FUN_001d7b37 and NtClose. We model this
 *     by taking the address of the `name` parameter (&name) so the generated
 *     LEA references [EBP+8], preserving the original stack shape.
 *   - OBJECT_ATTRIBUTES (12 bytes, Xbox 3-field form): RootDirectory=0,
 *     ObjectName=&ansi, Attributes=OBJ_CASE_INSENSITIVE(0x40).
 *   - RtlInitAnsiString(&ansi, name); 2 args, __stdcall.
 *   - NtOpenFile(&name, 0x110101, &oa, &iosb, 3, 0x4021); 6 args, __stdcall.
 *   - FUN_001d7b37(handle, buf); 2 args; buffer is EBP-0x164 (0x148=328 bytes,
 *     FUN_001d7b37 reads fields up to ~+0x40, well inside).
 *   - NtClose(handle); 1 arg, __stdcall.
 *
 * Inferred:
 *   - DesiredAccess 0x110101 = SYNCHRONIZE|FILE_READ_DATA|READ_CONTROL.
 *   - OpenOptions 0x4021 =
 * FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE.
 *   - ShareAccess 3 = FILE_SHARE_READ|FILE_SHARE_WRITE.
 *
 * Reuses the shared XAPI_ NT types/externs declared above (same set as
 * FUN_001d7d21); only RtlInitAnsiString is added.  FUN_001d7b37 is the
 * unported reader, called by its kb name with its real (handle, buf) decl.
 */
int __stdcall FUN_001d7cb4(void *name)
{
  XAPI_ANSI_STRING ansi;
  XAPI_OBJECT_ATTRIBUTES oa;
  XAPI_IO_STATUS_BLOCK iosb;
  unsigned char buf[328];
  NTSTATUS status;

  RtlInitAnsiString(&ansi, (const char *)name);
  oa.RootDirectory = 0;
  oa.ObjectName = &ansi;
  oa.Attributes = 0x40; /* OBJ_CASE_INSENSITIVE */

  /* &name reuses the [EBP+8] parameter slot as the FileHandle OUT param,
   * matching the original's LEA EAX,[EBP+8].  After the call `name` holds the
   * opened HANDLE, reused for the read and close. */
  status = NtOpenFile((HANDLE *)&name, 0x110101, &oa, &iosb, 3, 0x4021);
  if (status >= 0) {
    status = FUN_001d7b37(name, buf);
    NtClose(name);
  }
  return status;
}

/*
 * FUN_001d7d21  (XAPI volume-information query, 99 bytes)
 *
 * Opens a file/volume object by name, queries its volume information
 * (FILE_FS_SIZE_INFORMATION, class 3, into a 0x18-byte output buffer), then
 * closes the handle.  Returns the NTSTATUS: NtOpenFile's status on failure,
 * otherwise NtQueryVolumeInformationFile's status (NtClose's result is
 * discarded).  The caller passes a pointer to the object-attributes name
 * field (ObjectName).
 *
 * Confirmed (disasm 0x1d7d21..0x1d7d83):
 *   - __stdcall: RET 0x4, one 4-byte stack arg at [EBP+8]
 *   - Saves ESI only; ESI holds the live NTSTATUS across the query/close pair
 *   - OBJECT_ATTRIBUTES block [EBP-0x14]: RootDirectory(+0)=0 (ANDL $0,..),
 *     ObjectName(+4)=param, Attributes(+8)=0x40
 *   - NtOpenFile takes &[EBP+8], so the returned HANDLE is written back into
 *     the parameter slot; the same slot is then reused as the handle for the
 *     subsequent query/close calls (single-slot dual use).  We reproduce this
 *     by taking the address of the parameter itself (no copy local), forcing
 *     it to its home [EBP+8] slot.
 *   - NtOpenFile(&handle, 0x100001, &oa, &iosb, 3, 0x800021)  -- 6 stack args
 *   - On success (status >= 0):
 *       NtQueryVolumeInformationFile(handle, &iosb, buf, 0x18, 3)  -- 5 args
 *       NtClose(handle)  -- return discarded
 *   - No FPU, no intrinsics, straight-line control flow
 */
int __stdcall FUN_001d7d21(void *object_name_field)
{
  XAPI_OBJECT_ATTRIBUTES oa; /* [EBP-0x14] */
  XAPI_IO_STATUS_BLOCK iosb; /* [EBP-8] */
  char volume_info[24]; /* [EBP-0x2c]: 0x18-byte volume-info output */
  int status;

  oa.RootDirectory = 0;
  oa.ObjectName = (XAPI_ANSI_STRING *)object_name_field;
  oa.Attributes = 0x40;

  /* object_name_field is the [EBP+8] slot: input name ptr, then the handle
     written back by NtOpenFile and reused for the query/close calls. */
  status = NtOpenFile(&object_name_field, 0x100001, &oa, &iosb, 3, 0x800021);
  if (status >= 0) {
    status = NtQueryVolumeInformationFile(object_name_field, &iosb, volume_info,
                                          0x18, 3);
    NtClose(object_name_field);
  }
  return status;
}

/* strncmp (CRT stub) — provided here since _strncmp is internal to the original
 * LIBCMT and not exported from the XBE import table. Behaviorally equivalent to
 * the standard strncmp; called only by csstrncmp in cseries.c. */
int strncmp(const char *s1, const char *s2, unsigned int n)
{
  unsigned int i;
  unsigned char c1;
  unsigned char c2;

  for (i = 0; i < n; i++) {
    c1 = (unsigned char)s1[i];
    c2 = (unsigned char)s2[i];
    if (c1 != c2)
      return c1 < c2 ? -1 : 1;
    if (c1 == '\0')
      return 0;
  }
  return 0;
}

/* fabs and __chkstk are clang-build-only helpers using GCC-style inline asm /
 * naked attributes that the real VC71 compiler cannot parse.  They are guarded
 * out of the VC71 path (which defines MSVC) so vc71_verify can compile this TU;
 * VC71 supplies fabs as an intrinsic and __chkstk from its own CRT. */
#ifndef MSVC
/* fabs is used by valid_real_normal3d_perpendicular; not in XDK libm.
 * VC71 inlines it as x87 FABS — this stub is only reached by the clang build.
 */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma function(fabs)
#endif
double fabs(double x)
{
#if defined(_MSC_VER) && !defined(__clang__)
  __asm {
    fabs
  }
#else
  __asm__ __volatile__("fabs" : "+t"(x));
#endif
  return x;
}

/* __chkstk (exported as __chkstk after Windows x86 name-mangling of _chkstk).
 * Clang (i386-pc-win32) emits `mov eax, <framesize>; call __chkstk` for any
 * function whose stack frame exceeds one page and — contrary to a long-held
 * assumption — emits NO following `sub esp, eax`.  __chkstk itself must reserve
 * the frame, exactly as the original MSVC runtime does (cachebeta.xbe
 * 0x1d90e0): subtract the size from ESP and relocate the return address.  A
 * bare `ret` here left the frame UNALLOCATED, so a function's locals/spills
 * aliased live ESP and were clobbered by the next argument push — manifested as
 * a NULL+0x99 fault in actor_has_accessible_firing_position (0x25a00) and as
 * corrupted firing-position records driving AI aim in FUN_00025c10 (0x25c10).
 * Byte-faithful to 0x1d90e0; Xbox fully commits the thread stack, so no
 * page-probing is needed. */
#if !defined(_MSC_VER) || defined(__clang__)
__attribute__((naked)) void _chkstk(void)
{
  __asm__("test %eax, %eax\n\t" /* frame size == 0? nothing to do */
          "je 1f\n\t"
          "neg %eax\n\t" /* eax = -size */
          "add %esp, %eax\n\t" /* eax = esp - size */
          "add $4, %eax\n\t" /* account for the return-address slot */
          "xchg %eax, %esp\n\t" /* esp = new frame top; eax = old esp */
          "mov (%eax), %eax\n\t" /* eax = saved return address */
          "push %eax\n\t" /* re-push it at the new top */
          "1:\n\t"
          "ret\n\t");
}
#endif
#endif /* !MSVC */

/* LIBCMT:qsort.obj, 0x1d9260. _shortsort receives hi in EAX; its remaining
 * arguments use cdecl stack slots (verified at 0x1d92ba-0x1d92c4). */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("ty", on)
#endif
static __forceinline void __cdecl qsort_swap(char *left, char *right,
                                             size_t byte_count)
{
  char swap_byte;

  if (left != right) {
    while (byte_count--) {
      swap_byte = *left;
      *left++ = *right;
      *right++ = swap_byte;
    }
  }
}

void __cdecl qsort(void *base, size_t nmemb, size_t width,
                   qsort_compar_proc compar)
{
  char *lo;
  char *hi;
  char *mid;
  char *loguy;
  char *higuy;
  size_t size;
  char *lostk[30];
  char *histk[30];
  int stkptr;

  if (nmemb < 2 || width == 0)
    return;

  stkptr = 0;
  lo = (char *)base;
  hi = (char *)base + width * (nmemb - 1);

recurse:
  size = (size_t)(hi - lo) / width + 1;
  if (size <= 8) {
    _shortsort(hi, lo, width, compar);
  } else {
    mid = lo + (size >> 1) * width;

    if (compar(lo, mid) > 0)
      qsort_swap(lo, mid, width);
    if (compar(lo, hi) > 0)
      qsort_swap(lo, hi, width);
    if (compar(mid, hi) > 0)
      qsort_swap(mid, hi, width);

    loguy = lo;
    higuy = hi;

    for (;;) {
      if (mid > loguy) {
        do {
          loguy += width;
        } while (loguy < mid && compar(loguy, mid) <= 0);
      }

      if (mid <= loguy) {
        do {
          loguy += width;
        } while (loguy <= hi && compar(loguy, mid) <= 0);
      }

      do {
        higuy -= width;
      } while (higuy > mid && compar(higuy, mid) > 0);

      if (loguy > higuy)
        break;

      qsort_swap(loguy, higuy, width);

      if (mid == higuy)
        mid = loguy;
    }

    higuy += width;
    if (mid < higuy) {
      do {
        higuy -= width;
      } while (higuy > mid && compar(higuy, mid) == 0);
    }
    if (mid >= higuy) {
      do {
        higuy -= width;
      } while (higuy > lo && compar(higuy, mid) == 0);
    }

    if (higuy - lo >= hi - loguy) {
      if (lo < higuy) {
        lostk[stkptr] = lo;
        histk[stkptr] = higuy;
        stkptr++;
      }
      if (loguy < hi) {
        lo = loguy;
        goto recurse;
      }
    } else {
      if (loguy < hi) {
        lostk[stkptr] = loguy;
        histk[stkptr] = hi;
        stkptr++;
      }
      if (lo < higuy) {
        hi = higuy;
        goto recurse;
      }
    }
  }

  stkptr--;
  if (stkptr >= 0) {
    lo = lostk[stkptr];
    hi = histk[stkptr];
    goto recurse;
  }
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize("", on)
#endif

/* LIBCMT:localtim.obj, 0x1da92a.  The CRT tm storage is treated as nine
 * 32-bit fields; field offsets are verified from the original stores. */
void *crt_localtime(int *timer)
{
  int *tm;
  int time;

  if (*timer < 0)
    return 0;

  FUN_001e1953();

  if (*timer > 0x3f480 && *timer < 0x7ffc0b7f) {
    time = *timer - *(int *)0x3317d0;
    tm = _gmtime(&time);
    if (*(int *)0x3317d4 != 0 && FUN_001e1997(tm) != 0) {
      time -= *(int *)0x3317d8;
      tm = _gmtime(&time);
      tm[8] = 1;
    }
    return tm;
  }

  tm = _gmtime(timer);
  if (*(int *)0x3317d4 != 0 && FUN_001e1997(tm) != 0) {
    time = tm[0] - *(int *)0x3317d8 - *(int *)0x3317d0;
    tm[8] = 1;
  } else {
    time = tm[0] - *(int *)0x3317d0;
  }

  tm[0] = time % 60;
  if (tm[0] < 0) {
    tm[0] += 60;
    time -= 60;
  }

  time = time / 60 + tm[1];
  tm[1] = time % 60;
  if (tm[1] < 0) {
    tm[1] += 60;
    time -= 60;
  }

  time = time / 60 + tm[2];
  tm[2] = time % 24;
  if (tm[2] < 0) {
    tm[2] += 24;
    time -= 24;
  }

  time /= 24;
  if (time > 0) {
    tm[6] = (tm[6] + time) % 7;
    tm[3] += time;
  } else {
    if (time >= 0)
      return tm;

    tm[6] = (tm[6] + time + 7) % 7;
    tm[3] += time;
    if (tm[3] <= 0) {
      tm[3] += 0x1f;
      tm[5]--;
      tm[7] = 0x16c;
      tm[4] = 0xb;
      return tm;
    }
  }

  tm[7] += time;
  return tm;
}

/* Fixed XBE globals for the auto-power-down timer (XAPILIB).
 *   0x6329e0  KTIMER  (dispatcher timer object)
 *   0x6329c0  KDPC    (deferred procedure call object)
 *   0x632a08  DWORD   "power-down enabled" flag (read by the DPC routine)
 *   0x632a0c  DWORD   "query failed" flag (set when ExQueryNonVolatileSetting
 *                     returns a non-zero DOS error) */
#define XAPI_PWRDOWN_TIMER ((void *)0x6329e0)
#define XAPI_PWRDOWN_DPC ((void *)0x6329c0)
#define XAPI_PWRDOWN_ENABLED (*(unsigned long *)0x632a08)
#define XAPI_PWRDOWN_QUERY_FAILED (*(unsigned long *)0x632a0c)

/*
 * FUN_001d771c  XAPILIB::XAutoPowerDownResetTimer  (0x1d771c, 0x1b bytes)
 *
 * Re-arms the auto-power-down timer.  DueTime is a relative LARGE_INTEGER of
 * 0xffffffcd_b5659000 (== -216000000000 in 100ns ticks == -6 hours): a negative
 * relative due time, so the box powers down 6 hours after the last reset.
 *
 * Disassembly (push order, last push = first C arg):
 *   PUSH -0x33; POP ECX           ; ECX = 0xffffffcd  (DueTime.HighPart)
 *   PUSH 0x6329c0                 ; Dpc   (4th arg)
 *   PUSH ECX                      ; DueTime.HighPart
 *   MOV EAX,0xb5659000; PUSH EAX  ; DueTime.LowPart
 *   PUSH 0x6329e0                 ; Timer (1st arg)
 *   CALL [0x2531e0]               ; KeSetTimer
 * The LARGE_INTEGER is passed by value; MSVC pushes LowPart then HighPart,
 * which the by-value struct argument reproduces.
 */
void XAutoPowerDownResetTimer(void)
{
  XAPI_TIMER_LARGE_INTEGER due_time;

  due_time.LowPart = 0xb5659000UL;
  due_time.HighPart = (long)0xffffffcdUL;
  KeSetTimer(XAPI_PWRDOWN_TIMER, due_time, XAPI_PWRDOWN_DPC);
}

/*
 * FUN_001d7749  (0x1d7749, 0x6a bytes) — auto-power-down timer initialization.
 *
 * Initializes the DPC and timer dispatcher objects, queries the persisted
 * "auto power-down" non-volatile setting (ValueIndex 0x11), records its low
 * bit in the enabled flag, and arms the timer.
 *
 * Disassembly:
 *   KeInitializeDpc(&KDPC, 0x1d7737, 0)   ; DPC routine is the code at 0x1d7737
 *   KeInitializeTimerEx(&KTIMER, 0)       ; Type 0 == NotificationTimer
 *   AND [0x632a08],0 ; AND [0x632a0c],0   ; zero both flags
 *   FUN_001d4464(0x11, &type, &value, 4, &result_len)  ; query NV setting
 *     pushes (reverse order): 0x11, &local_10(type), &local_8(value), 4,
 *                             &local_c(result_len)
 *   if (result == 0)  [0x632a08] = value & 1;
 *   else              [0x632a0c] = 1;
 *   XAutoPowerDownResetTimer();
 *
 * 0x1d7737 is the DPC deferred routine (RET 0x10 callback) embedded between the
 * two ported functions; it remains in the patched image, so its address is
 * passed verbatim.  FUN_001d4464 is the XAPI ExQueryNonVolatileSetting wrapper
 * (stdcall, 5 args, returns the translated DOS error in EAX).
 */
void FUN_001d7749(void)
{
  unsigned long setting_type;  /* [EBP-0xc] local_10: NV setting type out */
  unsigned long result_len;    /* [EBP-0x8] local_c:  NV result length out */
  unsigned long value;         /* [EBP-0x4] local_8:  NV value (4 bytes) */
  int status;

  KeInitializeDpc(XAPI_PWRDOWN_DPC, (void *)0x1d7737, 0);
  KeInitializeTimerEx(XAPI_PWRDOWN_TIMER, 0);
  XAPI_PWRDOWN_ENABLED = 0;
  XAPI_PWRDOWN_QUERY_FAILED = 0;

  status = FUN_001d4464(0x11, &setting_type, &value, 4, &result_len);
  if (status == 0) {
    XAPI_PWRDOWN_ENABLED = value & 1;
  } else {
    XAPI_PWRDOWN_QUERY_FAILED = 1;
  }

  XAutoPowerDownResetTimer();
}

/* Two fixed ANSI_STRING/letter-table globals consumed by FUN_001d819f.
 * Each pair is (drive-letter table, mapped-directory ANSI_STRING) handed to
 * XapiMapLetterToDirectory; we only ever pass their addresses, matching the
 * original's PUSH 0x32fd78 / PUSH 0x32fd80 / PUSH 0x32fd88 / PUSH 0x32fd90. */
#define XAPI_MAP_TABLE_A ((void *)0x32fd78)
#define XAPI_MAP_NAME_A ((unsigned short *)0x32fd80)
#define XAPI_MAP_TABLE_B ((void *)0x32fd88)
#define XAPI_MAP_NAME_B ((unsigned short *)0x32fd90)

/*
 * FUN_001d819f  (XAPI map-handle-to-drive-letter, 0x53 bytes)
 *
 * Formats a 32-bit handle/id as an 8-digit lowercase hex drive letter
 * ("%08lx" -> e.g. "0001a2b3"), then maps that letter to two fixed
 * directories via XapiMapLetterToDirectory.  The first mapping (create=1,
 * out=0, flags=0) is the gate: only if it succeeds (status >= 0) is the
 * second mapping performed, this time forwarding the caller's param_2 as the
 * 5th (out) argument.
 *
 * Confirmed (disasm 0x1d819f..0x1d81f1):
 *   - __stdcall: RET 0x8, two 4-byte stack args (param_1@[EBP+8],
 *     param_2@[EBP+0xc]).
 *   - 12-byte format buffer at [EBP-0xc] (SUB ESP,0xc).
 *   - crt_sprintf(buf, "%08lx", param_1): __cdecl, 3 args, ADD ESP,0xc cleanup;
 *     format string lives at 0x2c1e28 ("%08lx\0").
 *   - XapiMapLetterToDirectory (0x1d7e6b): __stdcall, 6 args, no caller
 *     cleanup.  First call: (0x32fd78, 0x32fd80, buf, 1, 0, 0).
 *   - JL skips the second call when the first returns a negative NTSTATUS.
 *   - Second call: (0x32fd88, 0x32fd90, buf, 1, param_2, 0).
 *
 * Inferred:
 *   - param_2 is an out parameter (short*) for the second mapping; passed as 0
 *     in the first (probe) call, as the caller's pointer in the second.
 */
void __stdcall FUN_001d819f(unsigned long handle_id, short *out)
{
  char letter[12]; /* [EBP-0xc] */
  int status;

  crt_sprintf(letter, "%08lx", handle_id);
  status = XapiMapLetterToDirectory(XAPI_MAP_TABLE_A, XAPI_MAP_NAME_A, letter, 1, 0, 0);
  if (status >= 0) {
    XapiMapLetterToDirectory(XAPI_MAP_TABLE_B, XAPI_MAP_NAME_B, letter, 1, out, 0);
  }
}

/*
 * FUN_001d7d84  (XAPI write-section-to-file, 0xe5 bytes)
 *
 * Builds a target file name by appending `suffix` to the end of `path`
 * in-place (bounded by `path_size`), creates/opens that file for write,
 * verifies it is empty (both EOF/allocation fields are zero), then loads the
 * named XBE section, writes its full contents to the file, and unloads it.
 * The appended suffix is stripped back off (NUL written at the original end)
 * immediately after NtCreateFile, so `path` is restored before return.
 *
 * Returns the NTSTATUS of the last failing/relevant NT call, with
 * STATUS_OBJECT_NAME_COLLISION (0xc0000035) mapped to 0 (success: file already
 * exists, nothing to do).
 *
 * Confirmed (disasm 0x1d7d84..0x1d7e68):
 *   - __stdcall: RET 0x10, four args:
 *       section@[EBP+8], path@[EBP+0xc], path_size@[EBP+0x10], suffix@[EBP+0x14].
 *   - EBX=0 sentinel, EDI holds the running NTSTATUS, ESI holds the
 *     end-of-string pointer (and later the section data pointer).
 *   - end = path + strlen(path); FUN_001d789a(end, suffix, path_size - strlen).
 *   - RtlInitAnsiString(&ansi[EBP-0x10], path).
 *   - OBJECT_ATTRIBUTES [EBP-0x1c]: RootDirectory(+0)=0, ObjectName(+4)=&ansi,
 *     Attributes(+8)=0x40.
 *   - NtCreateFile(&path_slot[EBP+0xc], 0x40100000, &oa, &iosb[EBP-8], 0,
 *       4, 1, 3, 0x22): 9 args.  The [EBP+0xc] (path) slot is reused as the
 *       FileHandle OUT param (LEA EAX,[EBP+0xc]); after the call it holds the
 *       opened HANDLE, reused for query/write/close.
 *   - *end = '\0' (MOV [ESI],BL) immediately after the create call.
 *   - On NtCreateFile failure: STATUS_OBJECT_NAME_COLLISION -> 0, else return
 *     the status.
 *   - NtQueryInformationFile(handle, &iosb, info[EBP-0x54], 0x38, 0x22): 5 args.
 *     Standard-information (class 0x22 == FileStandardInformation); the two
 *     dwords checked at info+0x18 / info+0x1c (EndOfFile/AllocationSize, the
 *     [EBP-0x2c]/[EBP-0x28] locals) must both be zero to proceed.
 *   - FUN_001d8aef(section) -> data ptr (ESI); if 0, skip the write.
 *   - XGetSectionSize(section) -> size (XGetSectionSize, __stdcall, RET 4).
 *   - NtWriteFile(handle, 0, 0, 0, &iosb, data, size, 0): 8 args.
 *   - FUN_001d8b10(section) (XeUnloadSection); result discarded for return.
 *   - NtClose(handle).
 *
 * Inferred:
 *   - DesiredAccess 0x40100000 = GENERIC_WRITE | SYNCHRONIZE.
 *   - FileAttributes 4 = FILE_ATTRIBUTE_SYSTEM, ShareAccess 1 = FILE_SHARE_READ.
 *   - CreateDisposition 3 = FILE_OPEN_IF, CreateOptions 0x22 =
 *     FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE.
 *   - The 0x38-byte query buffer holds FILE_STANDARD_INFORMATION; only the two
 *     8-byte-aligned dwords at +0x18/+0x1c are read (file must be brand new).
 */
int __stdcall FUN_001d7d84(int section, char *path, int path_size, char *suffix)
{
  unsigned char info[56]; /* [EBP-0x54]: 0x38-byte standard-info query out */
  XAPI_OBJECT_ATTRIBUTES oa;  /* [EBP-0x1c] */
  XAPI_ANSI_STRING ansi;      /* [EBP-0x10] */
  XAPI_IO_STATUS_BLOCK iosb;  /* [EBP-8] */
  char *end;
  char *scan;
  int status;
  void *data;
  unsigned long size;

  /* end = path + strlen(path) */
  scan = path;
  while (*scan != '\0')
    scan++;
  end = path + (int)(scan - path);

  /* Append suffix in-place after the string, bounded by remaining space. */
  FUN_001d789a(end, suffix, path_size - (int)(scan - path));

  RtlInitAnsiString(&ansi, path);
  oa.RootDirectory = 0;
  oa.ObjectName = &ansi;
  oa.Attributes = 0x40; /* OBJ_CASE_INSENSITIVE */

  /* &path reuses the [EBP+0xc] parameter slot as the FileHandle OUT param,
   * matching the original's LEA EAX,[EBP+0xc].  After the call `path` holds the
   * opened HANDLE, reused for query/write/close. */
  status = NtCreateFile((HANDLE *)&path, 0x40100000, &oa, &iosb, 0, 4, 1, 3,
                        0x22);

  /* Restore the original terminator, stripping the appended suffix. */
  *end = '\0';

  if (status >= 0) {
    status = NtQueryInformationFile((HANDLE)path, &iosb, info, 0x38, 0x22);
    /* The two zero-checked dwords are the [EBP-0x2c]/[EBP-0x28] locals, which
     * land at info+0x28 / info+0x2c (info base = [EBP-0x54]).  They are read
     * from the query buffer, not separate locals (buffer-alias). */
    if (status >= 0 &&
        *(int *)(info + 0x28) == 0 && *(int *)(info + 0x2c) == 0) {
      data = FUN_001d8aef(section);
      if (data != 0) {
        size = XGetSectionSize((void *)section);
        status = NtWriteFile((HANDLE)path, 0, 0, 0, &iosb, data, size, 0);
        FUN_001d8b10(section);
      }
    }
    NtClose((HANDLE)path);
  } else if (status == (int)0xc0000035) {
    /* STATUS_OBJECT_NAME_COLLISION: file already exists -> treat as success. */
    status = 0;
  }
  return status;
}

/*
 * FUN_001d7b37  (XAPI recursive directory delete, 0x17d bytes)
 *
 * Enumerates the directory `handle` one entry at a time with
 * NtQueryDirectoryFile (RestartScan on the first call, then RestartScan=0).
 * For each FILE_DIRECTORY_INFORMATION record written into `buf`, it
 * NUL-terminates the file name, sub-opens that name relative to `handle`,
 * and either recurses (if the entry is a directory) or clears the file's
 * read-only attribute and marks it for deletion.  The directory handle is
 * sub-opened with FILE_DELETE_ON_CLOSE so the directory itself is removed
 * when its handle is closed by the caller; on STATUS_NO_MORE_FILES
 * (0x80000006) / STATUS_NO_SUCH_FILE (0xc000000f) the directory handle
 * `handle` is finally cleared read-only and marked for deletion.  Returns
 * the last NTSTATUS.
 *
 * Confirmed (disasm 0x1d7b37..0x1d7cb1):
 *   - __stdcall: RET 8, two pointer args handle@[EBP+8], buf@[EBP+0xc].
 *   - ESI=buf (record pointer), EDI=running NTSTATUS, EBX=0 sentinel.
 *   - Record layout used: FileNameLength@+0x3c, FileAttributes@+0x38,
 *     FileName@+0x40.  The NUL is written at FileName[FileNameLength].
 *   - RestartScan BOOLEAN (low byte of local_c) is 1 on the first iteration,
 *     0 thereafter (CONCAT31 -> byte store of 1; AND with 0xffffff00 -> 0).
 *   - Sub-open OBJECT_ATTRIBUTES: RootDirectory=handle, ObjectName=&ansi(name),
 *     Attributes=0x40.  bit-4 of FileAttributes selects DIRECTORY:
 *       DesiredAccess = 0x110100 | (isDir?1:0)
 *       OpenOptions   = 0x4000   | (isDir?1:0x40)   (DELETE_ON_CLOSE|...)
 *       ShareAccess   = 7
 *     where 1=FILE_DIRECTORY_FILE / 0x40=FILE_NON_DIRECTORY_FILE.
 *   - Non-directory branch: optionally clears read-only via FILE_BASIC_INFO
 *     (0x28 bytes, class 4) when the read-only bit (attr&1) is set, masking
 *     attributes to (attr & 0x3126) | 0x80 (FILE_ATTRIBUTE_NORMAL); then sets
 *     FileDispositionInformation (class 0xd) DeleteFile=TRUE (1-byte buffer =
 *     high byte of param_2/buf, set to 1 via CONCAT13).
 *   - Directory branch: recursive call FUN_001d7b37(sub_handle, buf).
 *   - NtClose(sub_handle) after each entry; loop continues while status >= 0.
 *   - Trailer (STATUS_NO_MORE_FILES / STATUS_NO_SUCH_FILE): clear read-only
 *     and mark `handle` itself for deletion.
 *
 * Inferred:
 *   - FileInformationClass 1 = FileDirectoryInformation.
 *   - The 0x28-byte FILE_BASIC_INFORMATION buffer is zeroed before use; only
 *     its FileAttributes field (at +0x20) is set (the local_30 store).
 *   - DeleteFile is a 1-byte BOOLEAN; we use a dedicated local to avoid the
 *     original's high-byte-of-param aliasing trick.
 */
int __stdcall FUN_001d7b37(void *handle, void *buf)
{
  unsigned char basic_info[40]; /* [EBP-0x4c]: FILE_BASIC_INFORMATION (0x28) */
  XAPI_OBJECT_ATTRIBUTES oa;    /* local_28/24/20 */
  XAPI_ANSI_STRING ansi;        /* [EBP-0x18] */
  XAPI_IO_STATUS_BLOCK iosb;    /* [EBP-0x10] */
  HANDLE sub_handle;            /* [EBP-4] */
  unsigned char restart_scan;
  unsigned char delete_file;
  unsigned int attrs;
  unsigned char is_dir;
  int status;
  unsigned char *rec;

  rec = (unsigned char *)buf;
  restart_scan = 1;
  do {
    status = NtQueryDirectoryFile(handle, 0, 0, 0, &iosb, buf, 0x146, 1, 1,
                                  restart_scan);
    restart_scan = 0;
    if (status < 0)
      break;

    /* NUL-terminate the file name: FileName[FileNameLength] = 0 */
    rec[*(int *)(rec + 0x3c) + 0x40] = 0;
    attrs = *(unsigned int *)(rec + 0x38);
    is_dir = (unsigned char)(attrs >> 4) & 1;

    RtlInitAnsiString(&ansi, (const char *)(rec + 0x40));
    oa.RootDirectory = handle;
    oa.ObjectName = &ansi;
    oa.Attributes = 0x40;
    /* OpenOptions computed branchlessly (neg/sbb/and/add idiom): when isDir the
       low bits become 1 (FILE_DIRECTORY_FILE), else 0x40 (NON_DIRECTORY_FILE). */
    status = NtOpenFile(&sub_handle, 0x110100 | is_dir, &oa, &iosb, 7,
                        0x4000u | ((-(int)is_dir & 0xffffffc1u) + 0x40u));
    if (status < 0)
      break;

    if (is_dir != 0) {
      FUN_001d7b37(sub_handle, buf);
    } else {
      attrs = *(unsigned int *)(rec + 0x38);
      if ((attrs & 1) != 0) {
        unsigned int i;
        for (i = 0; i < 10; i++)
          ((unsigned int *)basic_info)[i] = 0;
        *(unsigned int *)(basic_info + 0x20) = (attrs & 0x3126) | 0x80;
        status = NtSetInformationFile(sub_handle, &iosb, basic_info, 0x28, 4);
      }
      if (status >= 0) {
        delete_file = 1;
        status = NtSetInformationFile(sub_handle, &iosb, &delete_file, 1, 0xd);
      }
    }
    NtClose(sub_handle);
  } while (status >= 0);

  if (status == (int)0x80000006 || status == (int)0xc000000f) {
    unsigned int i;
    for (i = 0; i < 10; i++)
      ((unsigned int *)basic_info)[i] = 0;
    *(unsigned int *)(basic_info + 0x20) =
        (*(unsigned int *)(rec + 0x38) & 0x3126) | 0x80;
    NtSetInformationFile(handle, &iosb, basic_info, 0x28, 4);
    delete_file = 1;
    status = NtSetInformationFile(handle, &iosb, &delete_file, 1, 0xd);
  }
  return status;
}

/* XBE header CertificateAddress field (XBE base 0x10000, +0x118).  Holds a
 * pointer to the XBE Certificate; cert+8 is the 32-bit TitleID.  TitleID
 * 0xfffe0000 is the reserved dashboard/system id. */
#define XBE_CERT_ADDRESS (*(unsigned char **)0x10118)

/* LAUNCH_DATA is a fixed 3072-byte (0xc00) Xbox launch-data block. */
typedef struct {
  unsigned long dwArg0;       /* +0x000 */
  unsigned long dwReserved;   /* +0x004 (always 0) */
  unsigned long dwArg2;       /* +0x008 */
  unsigned long dwArg3;       /* +0x00c */
  unsigned char pad[0xc00 - 0x10];
} XAPI_LAUNCH_DATA;

/*
 * XapiBootToDash  (XAPI XapiBootToDash, 0x65 bytes)
 *
 * If the running title is NOT the reserved dashboard title (TitleID at
 * CertificateAddress+8 != 0xfffe0000), it zero-fills a 3072-byte LAUNCH_DATA
 * block, stores the three caller arguments into its first three dword slots
 * (dwReserved left zero), and relaunches the dashboard via
 * XLaunchNewImageA(NULL, &launch_data) — which does not return on success.
 * When the running title already IS the dashboard, it powers down / returns
 * to firmware via HalReturnToFirmware(4) (HalReturnToFirmwareRoutine =
 * ReturnToFirmwareRoutine 4 == HalRebootRoutine) and traps (INT3) since that
 * call does not return.
 *
 * Confirmed (disasm 0x1d81f4..0x1d8258):
 *   - __stdcall: RET 0xc, three 4-byte args: param_1@[EBP+8], param_2@[EBP+0xc],
 *     param_3@[EBP+0x10].
 *   - SUB ESP,0xc00; REP STOSD of 0x300 dwords zeroes the whole block.
 *   - [block+0]=param_1; [block+4] explicitly zeroed (AND ..,0); [block+8]=
 *     param_2; [block+0xc]=param_3.
 *   - CMP dword[CertAddr+8], 0xfffe0000; JE -> firmware path.
 *   - XLaunchNewImageA(0, &block): __stdcall, 2 args, no caller cleanup.
 *   - Firmware path: PUSH 4; CALL [HalReturnToFirmware]; INT3.
 */
void __stdcall XapiBootToDash(unsigned long param_1, unsigned long param_2,
                            unsigned long param_3)
{
  XAPI_LAUNCH_DATA launch_data;

  if (*(int *)(XBE_CERT_ADDRESS + 8) != (int)0xfffe0000) {
    unsigned int i;
    for (i = 0; i < 0x300; i++)
      ((unsigned int *)&launch_data)[i] = 0;
    launch_data.dwReserved = 0;
    launch_data.dwArg0 = param_1;
    launch_data.dwArg2 = param_2;
    launch_data.dwArg3 = param_3;
    XLaunchNewImageA(0, &launch_data);
    return;
  }
  HalReturnToFirmware(4);
  /* Does not return; original traps with INT3 here. */
}

/* FUN_001dd6f5 (wide vsnwprintf-style formatter) and FUN_001d8a88 (XBE section
 * handle lookup by name) are declared by build/generated/decl.h from kb.json. */

/* String literals consumed by XapiMapLetterToDirectory (XapiMapLetterToDirectory). */
#define XTINFO_SECTION_NAME  "$$XTINFO"
#define XTIMAGE_SECTION_NAME "$$XTIMAGE"
#define XSIMAGE_SECTION_NAME "$$XSIMAGE"
#define XTINFO_FILE_SUFFIX   "\\TitleMeta.xbx"
#define XTIMAGE_FILE_SUFFIX  "\\TitleImage.xbx"
#define XSIMAGE_FILE_SUFFIX  "\\SaveImage.xbx"
/* Wide literals for the $$XTINFO TitleMeta.xbx body. */
#define XTINFO_FORMAT  ((const unsigned short *)0x2c1ce0) /* L"%lc%ls%lc%ls%ls" */
#define XTINFO_KEY     ((unsigned short *)0x2c2078)       /* L"TitleName" */
#define XTINFO_CRLF    ((unsigned short *)0x2c2094)       /* L"\r\n" */

/*
 * XapiMapLetterToDirectory  (XAPILIB::XapiMapLetterToDirectory, 0x334 bytes)
 *
 * Creates a drive-letter -> directory mapping (DOS device symbolic link) and,
 * when requested, materializes the title's save metadata sidecar files.
 *
 * Arguments (RET 0x18, six __stdcall stack args):
 *   link_name  @[EBP+8]   symbolic-link name passed to IoCreateSymbolicLink.
 *   name       @[EBP+0xc] ANSI_STRING* of the target directory (Length@+0,
 *                         Buffer@+4); also the ObjectName of the probe-open.
 *   letter     @[EBP+0x10] drive-letter string appended after the directory.
 *   create     @[EBP+0x14] non-zero -> create the directory if missing
 *                         (CreateDisposition OPEN_IF(3) vs OPEN(1)).
 *   title_name @[EBP+0x18] (out) wide title-name string; when non-NULL the
 *                         save-metadata sidecars are written.
 *   flags      @[EBP+0x1c] non-zero -> stamp the directory's creation time
 *                         (KeQuerySystemTime + FILE_BASIC_INFORMATION).
 *
 * Confirmed (disasm 0x1d7e6b..0x1d819c):
 *   - EBX=0 sentinel, EDI=0x80 (FileAttributes) then 0x104 (MAX_PATH).
 *   - Probe-open: NtCreateFile(&h,0x100001,&oa{0,name,0x40},&iosb,0,0x80,3,3,
 *     0x4021).  Success -> NtClose(h); STATUS_NOT_A_DIRECTORY(0xc0000103) ->
 *     status=0; any other negative -> return.
 *   - Build path buffer [EBP-0x168] (260 bytes): copy name->Buffer, then if it
 *     does not end with '\\' append one at [Length] (else decrement
 *     name->Length), then append `letter` at [Length+1].
 *   - RtlInitAnsiString(&ansiA[EBP-0x24], path); oa.ObjectName=&ansiA.
 *   - Create-open: NtCreateFile(&h,0x120117,&oa,&iosb,0,0x80,3,
 *     (create!=0)*2+1, 0x4021).  Negative -> return.
 *   - flags!=0: zero FILE_BASIC_INFORMATION[EBP-0x54](0x28); KeQuerySystemTime
 *     into +0(CreationTime); NtSetInformationFile(h,&iosb,fbi,0x28,4).
 *   - title_name!=0 block:
 *       xtinfo  = XGetSectionHandle("$$XTINFO");   (FUN_001d8a88)
 *       xtimage = XGetSectionHandle("$$XTIMAGE");
 *       xsimage = XGetSectionHandle("$$XSIMAGE");
 *       if (xtinfo != -1 || *title_name != 0):
 *         append "\\TitleMeta.xbx" to path (bounded by 0x104), open it
 *         GENERIC_WRITE OPEN_IF, restore the path terminator, and if the file
 *         is brand-new (QueryInformation EndOfFile/AllocationSize both 0):
 *           if section loadable -> write its bytes (XeLoadSection/XGetSectionSize
 *           /NtWriteFile/XeUnloadSection); else synth a wide
 *           "%lc%ls%lc%ls%ls" body (BOM, "TitleName", '=', title_name, CRLF)
 *           and NtWriteFile it (byte length = wchar count << 1).
 *         NtClose the meta file.
 *       if still ok: xtimage != -1 -> FUN_001d7d84(xtimage, path, 0x104,
 *         "\\TitleImage.xbx"); xsimage != -1 -> FUN_001d7d84(xsimage, path,
 *         0x104, "\\SaveImage.xbx").
 *   - NtClose(create-open handle).
 *   - On overall success: status = IoCreateSymbolicLink(link_name, &ansiA).
 *   - Returns the running NTSTATUS.
 *
 * Inferred:
 *   - 0x100001 = SYNCHRONIZE|FILE_READ_DATA probe access; 0x120117 =
 *     SYNCHRONIZE|READ_CONTROL|FILE_LIST_DIRECTORY|... directory access.
 *   - CreateOptions 0x4021 = FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT.
 *   - FileInformationClass 4 = FileBasicInformation, 0x22 =
 *     FileStandardInformation; the two zero-checked dwords land at the query
 *     buffer +0x18/+0x1c (EndOfFile/AllocationSize).
 *   - 0x8e (142) is the wide buffer element cap; 0xfeff is the UTF-16 BOM.
 */
int __stdcall XapiMapLetterToDirectory(void *link_name, unsigned short *name, char *letter,
                           int create, short *title_name, int flags)
{
  unsigned short wbuf[142];     /* [EBP-0x284]: 0x8e-wchar synth buffer */
  char path_buf[260];           /* [EBP-0x168]: built directory path (MAX_PATH) */
  unsigned char std_info[56];   /* [EBP-0x64]: FILE_STANDARD_INFORMATION (0x38) */
  unsigned char basic_info[40]; /* [EBP-0x54]: FILE_BASIC_INFORMATION (0x28) */
  XAPI_ANSI_STRING ansiA;       /* [EBP-0x24]: dir path ANSI_STRING */
  XAPI_ANSI_STRING ansiB;       /* [EBP-0x2c]: meta path ANSI_STRING */
  XAPI_OBJECT_ATTRIBUTES oa;    /* [EBP-0x10] */
  XAPI_IO_STATUS_BLOCK iosb;    /* [EBP-0x18] */
  HANDLE handle;                /* [EBP-4] */
  HANDLE meta_handle;
  int status;
  int xtinfo;
  int xtimage;
  int xsimage;
  void *section_data;
  unsigned long section_size;
  unsigned int dir_len;
  char *src;
  char *p;
  char *lp;
  char *end;
  int wcount;
  unsigned int i;

  /* --- probe-open: verify the symbolic-link target is creatable --------- */
  oa.RootDirectory = 0;
  oa.ObjectName = (XAPI_ANSI_STRING *)name;
  oa.Attributes = 0x40;
  status = NtCreateFile(&handle, 0x100001, &oa, &iosb, 0, 0x80, 3, 3, 0x4021);
  if (status >= 0) {
    NtClose(handle);
  } else if (status == (int)0xc0000103) { /* STATUS_NOT_A_DIRECTORY */
    status = 0;
  }
  if (status < 0)
    return status;

  /* --- build "<directory>\<letter>" into path_buf ----------------------- */
  /* strcpy(path_buf, name->Buffer) as a source-pointer walk: the original
     keeps the source pointer in a register and addresses the destination as
     path_buf + (p - src) via a precomputed base delta. */
  src = *(char **)((char *)name + 4); /* name->Buffer */
  p = src;
  do {
    path_buf[p - src] = *p;
  } while (*p++ != '\0');
  /* trailing-backslash check on the directory: path_buf[Length-1] */
  dir_len = *name; /* name->Length (re-read) */
  if (path_buf[dir_len - 1] != '\\') {
    path_buf[dir_len] = '\\';
  } else {
    *name = (unsigned short)(dir_len - 1);
  }
  /* append the drive letter after path_buf[Length+1] (Length re-read, may
     have been decremented above). */
  dir_len = *name;
  lp = letter;
  do {
    path_buf[(dir_len + 1) + (lp - letter)] = *lp;
  } while (*lp++ != '\0');

  RtlInitAnsiString(&ansiA, path_buf);
  oa.ObjectName = &ansiA;
  oa.RootDirectory = 0;
  oa.Attributes = 0x40;
  status = NtCreateFile(&handle, 0x120117, &oa, &iosb, 0, 0x80, 3,
                        (create != 0) * 2 + 1, 0x4021);
  /* Nested success body (matches the original's jge-body / single-return
     shape); the create-open failure falls straight to the trailing return. */
  if (status >= 0) {
    /* --- optional creation-time stamp ----------------------------------- */
    if (flags != 0) {
      for (i = 0; i < 10; i++)
        ((unsigned int *)basic_info)[i] = 0;
      KeQuerySystemTime(basic_info); /* CreationTime at +0 */
      NtSetInformationFile(handle, &iosb, basic_info, 0x28, 4);
    }

    /* --- title metadata sidecars ---------------------------------------- */
    if (title_name != 0) {
      xtinfo = FUN_001d8a88(XTINFO_SECTION_NAME);
      xtimage = FUN_001d8a88(XTIMAGE_SECTION_NAME);
      xsimage = FUN_001d8a88(XSIMAGE_SECTION_NAME);

      if (xtinfo != -1 || *title_name != 0) {
        /* append "\\TitleMeta.xbx" after the existing path string; the suffix
           length is bounded by MAX_PATH (0x104) minus the current strlen. */
        end = path_buf;
        while (*end != '\0')
          end++;
        FUN_001d789a(end, XTINFO_FILE_SUFFIX, 0x104 - (int)(end - path_buf));
        RtlInitAnsiString(&ansiB, path_buf);
        oa.ObjectName = &ansiB;
        oa.RootDirectory = 0;
        oa.Attributes = 0x40;
        status = NtCreateFile(&meta_handle, 0x40100000, &oa, &iosb, 0, 4, 1, 3,
                              0x22);
        /* restore terminator, stripping the appended suffix */
        *end = '\0';
        if (status >= 0) {
          status = NtQueryInformationFile(meta_handle, &iosb, std_info, 0x38,
                                          0x22);
          /* EndOfFile/AllocationSize at std_info+0x18 / +0x1c must both be 0 */
          if (status >= 0 && *(int *)(std_info + 0x18) == 0 &&
              *(int *)(std_info + 0x1c) == 0) {
            section_data = 0;
            if (xtinfo == -1 ||
                (section_data = FUN_001d8aef(xtinfo)) == 0) {
              wcount = FUN_001dd6f5(wbuf, 0x8e, XTINFO_FORMAT, 0xfeff,
                                    XTINFO_KEY, 0x3d, title_name, XTINFO_CRLF);
              status = NtWriteFile(meta_handle, 0, 0, 0, &iosb, wbuf,
                                   (unsigned long)(wcount << 1), 0);
            } else {
              section_size = XGetSectionSize((void *)xtinfo);
              status = NtWriteFile(meta_handle, 0, 0, 0, &iosb, section_data,
                                   section_size, 0);
              FUN_001d8b10(xtinfo);
            }
          }
          NtClose(meta_handle);
        } else if (status == (int)0xc0000035) { /* STATUS_OBJECT_NAME_COLLISION */
          status = 0;
        }
      }

      if (status >= 0) {
        if (xtimage != -1)
          status = FUN_001d7d84(xtimage, path_buf, 0x104, XTIMAGE_FILE_SUFFIX);
        if (status >= 0 && xsimage != -1)
          status = FUN_001d7d84(xsimage, path_buf, 0x104, XSIMAGE_FILE_SUFFIX);
      }
    }

    NtClose(handle);
    if (status >= 0)
      status = IoCreateSymbolicLink(link_name, &ansiA);
  }
  return status;
}

/* ==========================================================================
 * XAPILIB heap SEH __finally cleanup funclets (Group A)
 *
 * These three are the compiler-emitted SEH __finally handlers for the heap
 * routines below.  Each runs with the PARENT function's frame still live
 * (no prologue of its own), so it reads the parent's locals via EBP/EBX as
 * the unwinder / inline call leaves them.  Their sole job: if the parent
 * recorded that it took the heap critical section (flag at [EBP-0x1d]), call
 * RtlLeaveCriticalSection on it.
 *
 * They MUST be naked, byte-exact reproductions: clang's own __try/__finally
 * frame layout differs from MSVC's, so a structured re-lift would read the
 * flag from the wrong offset.  The call target 0x253098 is the absolute
 * RtlLeaveCriticalSection IAT slot (preserved across the patch).
 *
 * Bytes are reproduced verbatim from the pristine XBE disassembly.
 * ========================================================================== */

/* FUN_001d63d5  (__finally funclet of FUN_001d5c66, 19 bytes)
 *   80 7d e3 00       cmp byte ptr [ebp-0x1d], 0
 *   74 0c             je  +0x0c
 *   ff b3 80 05 00 00 push dword ptr [ebx+0x580]
 *   ff 15 98 30 25 00 call dword ptr [0x253098]   ; RtlLeaveCriticalSection
 *   c3                ret
 * Parent stores the heap pointer in EBX; cs lives at heap+0x580. */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(naked) void FUN_001d63d5(void)
{
  __asm {
    cmp byte ptr [ebp-0x1d], 0
    je skip
    push dword ptr [ebx+0x580]
    call ds:[0x253098]
  skip:
    ret
  }
}
#else
__attribute__((naked)) void FUN_001d63d5(void)
{
  __asm__ volatile(
      "cmpb $0, -0x1d(%ebp)\n\t"
      "je 1f\n\t"
      "pushl 0x580(%ebx)\n\t"
      "call *0x253098\n\t"
      "1:\n\t"
      "ret");
}
#endif

/* FUN_001d6e65  (__finally funclet of FUN_001d6ca8, 19 bytes)
 * Identical shape to FUN_001d63d5 (heap pointer in EBX). */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(naked) void FUN_001d6e65(void)
{
  __asm {
    cmp byte ptr [ebp-0x1d], 0
    je skip
    push dword ptr [ebx+0x580]
    call ds:[0x253098]
  skip:
    ret
  }
}
#else
__attribute__((naked)) void FUN_001d6e65(void)
{
  __asm__ volatile(
      "cmpb $0, -0x1d(%ebp)\n\t"
      "je 1f\n\t"
      "pushl 0x580(%ebx)\n\t"
      "call *0x253098\n\t"
      "1:\n\t"
      "ret");
}
#endif

/* FUN_001d76fc  (__finally funclet of FUN_001d703b, 22 bytes)
 *   80 7d e3 00       cmp byte ptr [ebp-0x1d], 0
 *   74 0f             je  +0x0f
 *   8b 45 e4          mov eax, dword ptr [ebp-0x1c]
 *   ff b0 80 05 00 00 push dword ptr [eax+0x580]
 *   ff 15 98 30 25 00 call dword ptr [0x253098]   ; RtlLeaveCriticalSection
 *   c3                ret
 * Here the heap pointer is in the parent local [EBP-0x1c], not EBX. */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(naked) void FUN_001d76fc(void)
{
  __asm {
    cmp byte ptr [ebp-0x1d], 0
    je skip
    mov eax, dword ptr [ebp-0x1c]
    push dword ptr [eax+0x580]
    call ds:[0x253098]
  skip:
    ret
  }
}
#else
__attribute__((naked)) void FUN_001d76fc(void)
{
  __asm__ volatile(
      "cmpb $0, -0x1d(%ebp)\n\t"
      "je 1f\n\t"
      "movl -0x1c(%ebp), %eax\n\t"
      "pushl 0x580(%eax)\n\t"
      "call *0x253098\n\t"
      "1:\n\t"
      "ret");
}
#endif

/* ==========================================================================
 * XAPILIB heap SEH wrapper parents (Group B)  —  __try/__except / __finally
 *
 * Per docs/seh-handling.md, the SEH frame shape differs from MSVC's compact
 * __SEH_prolog/epilog form, so vc71_verify reports ~55% on these.  That is the
 * EXPECTED, ACCEPTED ceiling for CRT/XAPI SEH wrappers: the mismatch is in the
 * 7-instruction frame prologue/epilogue, not in the lifted body.
 * ========================================================================== */

/* FUN_001d7a59  (XAPILIB wide-char strncpy, __stdcall, RET 0xc, 3 args)
 *
 * UTF-16 (wchar) analogue of FUN_001d789a (see above).  Copies up to `count`
 * wide chars from `src` to `dst`, stopping at a NUL, then NUL-terminates.  The
 * SEH __except guards faulting pointer arguments and returns 0 (NULL).  On the
 * success path it returns `dst`.
 *
 * Confirmed (disasm 0x1d7a59..0x1d7ac2):
 *   - 0x10-byte SEH locals (push 0x10; push table 0x2c1f68; call __SEH_prolog)
 *   - filter returns 1 (xor eax,eax; inc eax); handler returns 0
 *   - body copies word ptr [edx] -> word ptr [eax]; count at [ebp+0x10]
 *   - decrement-then-store-NUL on the early-out (matches FUN_001d789a logic)
 */
short *__stdcall FUN_001d7a59(short *dst, const short *src, int count)
{
  __try {
    short *d;
    const short *s;
    d = dst;
    s = src;
    if (count != 0) {
      while (count != 0) {
        if (*s == 0) {
          if (count != 0)
            goto done_null;
          break;
        }
        *d = *s;
        d = d + 1;
        s = s + 1;
        count = count - 1;
      }
      d = d - 1;
done_null:
      *d = 0;
    }
  }
  __except (1) { /* EXCEPTION_EXECUTE_HANDLER — catch faulting args */
    return 0;
  }
  return dst;
}

/* FUN_001d6ca8  (XAPILIB heap free-chunk / RtlFreeHeap internal,
 *                __stdcall, RET 0xc, 3 args, returns BOOLEAN in AL)
 *
 *   param_1  heap      — heap descriptor base
 *   param_2  flags     — HEAP_* flags (OR'd with heap+0x18 flags)
 *   param_3  block     — user pointer being freed (header at block-0x10)
 *
 * Returns 1 on success (or trivial NULL free), 0 if NtFreeVirtualMemory failed
 * on a large (DECOMMITTED) chunk.
 *
 * The original is a compact-SEH function whose __finally handler (the naked
 * FUN_001d6e65 funclet above) releases the heap critical section if it was
 * taken.  We express that cleanup with a C __try/__except(1): the `took_lock`
 * local + heap pointer give the funclet's exact behaviour without depending
 * on MSVC's frame offsets.
 *
 * Body transcribed faithfully from the Ghidra decompile of 0x1d6ca8; field
 * offsets and the free-list/bucket pointer surgery are reproduced verbatim.
 * Internal helpers (FUN_001d4a34/5598/4cd9) are unported __stdcall heap
 * helpers called by name. */
unsigned char __stdcall FUN_001d6ca8(void *heap, unsigned int flags, void *block)
{
  char *hp;          /* heap base (iVar9)            */
  char *blk;         /* block header pointer (iVar11)*/
  unsigned char took_lock; /* [EBP-0x1d]             */
  unsigned char result;    /* [EBP-0x1e] return value*/
  unsigned int idx;        /* [EBP-0x24] chunk size  */
  char *node;              /* FUN_001d4a34 result    */
  int *bucket;             /* [EBP-0x2c] free-list head*/
  unsigned short uv;
  unsigned int bit_word;
  int bit_mask;
  int *fwd;
  int *bk;
  char *cur;

  hp = (char *)heap;
  took_lock = 0;
  result = 1;

  if (block == 0)
    return 1;

  __try {
    flags = flags | *(unsigned int *)(hp + 0x18);
    blk = (char *)block - 0x10;

    if ((flags & 1) == 0) {
      RtlEnterCriticalSection(*(void **)(hp + 0x580));
      took_lock = 1;
    }

    if ((*(unsigned char *)(blk + 5) & 8) == 0) {
      idx = (unsigned int)*(unsigned short *)blk;
      node = (char *)FUN_001d4a34(hp, blk, (int *)&idx, 0);
      *(unsigned char *)(node + 5) = *(unsigned char *)(node + 5) & 0x10;

      if (idx < 0x80) {
        bucket = (int *)(hp + 0x180 + (unsigned int)(unsigned short)idx * 8);
        if ((int *)*bucket == bucket) {
          uv = *(unsigned short *)node;
          bit_word = (unsigned int)(uv >> 3);
          bit_mask = 1 << ((unsigned char)uv & 7);
          *(unsigned char *)(hp + bit_word + 0x160) =
              *(unsigned char *)(hp + bit_word + 0x160) | (unsigned char)bit_mask;
        }
        fwd = bucket;
        bk = (int *)bucket[1];
        *(int **)(node + 8) = fwd;
        *(int **)(node + 0xc) = bk;
        *bk = (int)(node + 8);
        bucket[1] = (int)(node + 8);
        *(int *)(hp + 0x30) = *(int *)(hp + 0x30) + (int)idx;
      } else if (idx < *(unsigned int *)(hp + 0x28) ||
                 *(int *)(hp + 0x30) + idx < *(unsigned int *)(hp + 0x2c)) {
        if (idx < 0xff01) {
          *(unsigned char *)(node + 5) = *(unsigned char *)(node + 5) & 0x10;
          cur = (char *)*(int *)(hp + 0x180);
          while ((char *)(hp + 0x180) != cur &&
                 *(unsigned short *)(cur - 8) < (unsigned short)idx) {
            cur = (char *)*(int *)cur;
          }
          fwd = (int *)cur;
          bk = (int *)((int *)cur)[1];
          *(int **)(node + 8) = fwd;
          *(int **)(node + 0xc) = bk;
          *bk = (int)(node + 8);
          ((int *)cur)[1] = (int)(node + 8);
          *(int *)(hp + 0x30) = *(int *)(hp + 0x30) + (int)idx;
        } else {
          FUN_001d4cd9(hp, node, idx);
        }
      } else {
        FUN_001d5598(hp, node, idx);
      }
    } else {
      /* large/virtual-allocated chunk: unlink + NtFreeVirtualMemory.
       * The original passes &[EBP-0x54] (holding blk-0x30) as BaseAddress
       * and &[EBP-0x24] (set to 0) as RegionSize. */
      void *vbase;       /* [EBP-0x54] = blk - 0x30 */
      int vfwd;          /* [EBP-0x58] = *(blk-0x30) */
      int *vbk;          /* [EBP-0x5c] = *(blk-0x2c) */
      ULONG region_size; /* [EBP-0x24] */
      int st;

      vbase = (void *)(blk - 0x30);
      vfwd = *(int *)(blk - 0x30);
      vbk = *(int **)(blk - 0x2c);
      *vbk = vfwd;
      *(int **)(vfwd + 4) = vbk;

      if (took_lock != 0) {
        RtlLeaveCriticalSection(*(void **)(hp + 0x580));
        took_lock = 0;
      }
      region_size = 0;
      st = NtFreeVirtualMemory(&vbase, &region_size, 0x8000);
      if (st < 0)
        result = 0;
    }

    /* Normal-path tail of the original SEH __finally (funclet FUN_001d6e65):
     * release the critical section if we took it.  Expressed inline + in the
     * __except handler because clang's __try/__finally pulls in the MSVC
     * __except_handler3 unwinder, which this freestanding build lacks. */
    if (took_lock != 0) {
      RtlLeaveCriticalSection(*(void **)(hp + 0x580));
      took_lock = 0;
    }
  }
  __except (1) { /* termination cleanup on fault — matches __finally semantics */
    if (took_lock != 0)
      RtlLeaveCriticalSection(*(void **)(hp + 0x580));
  }
  return result;
}
