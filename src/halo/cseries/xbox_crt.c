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

/*
 * FUN_001d7817  (XAPI case-insensitive string compare, 68 bytes)
 *
 * Locale-aware lstrcmpiA implementation. Tries FUN_001d8766 (internal
 * CompareString) first; on failure (returns 0) falls back to crt_stricmp.
 * Maps CompareString result (1=LESS,2=EQUAL,3=GREATER) to strcmp-style
 * (-1, 0, +1) via result-2.  NULL pointers are handled before the
 * fallback: NULL < non-NULL, NULL == NULL.
 *
 * Confirmed:
 *   - __stdcall: RET 0x8 pops 2 args (a@[EBP+8], b@[EBP+C])
 *   - ESI saves param b across calls
 *   - NEG EAX; SBB EAX,EAX pattern for null-a path: -1 if b!=0, 0 if both null
 *   - FUN_001d8766(1, a, -1, b, -1): 5 stack args, __stdcall
 *   - crt_stricmp(a, b): __cdecl fallback (POP ECX; POP ECX cleanup)
 */
int __stdcall FUN_001d7817(const char *a, const char *b)
{
  int result;

  result = FUN_001d8766(1, a, -1, b, -1);
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
 *   - OpenOptions 0x4021 = FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE.
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

/* fabs is used by valid_real_normal3d_perpendicular; not in XDK libm.
 * VC71 inlines it as x87 FABS — this stub is only reached by the clang build. */
double fabs(double x)
{
  __asm__ __volatile__("fabs" : "+t"(x));
  return x;
}

/* __chkstk (exported as __chkstk after Windows x86 name-mangling of _chkstk).
 * Clang (i386-pc-win32) emits `mov eax, N; call __chkstk` before `sub esp, eax`
 * for stack frames larger than one page.  On Xbox the kernel fully commits the
 * thread stack at creation time, so page-probing is unnecessary.  EAX is
 * preserved by RET so the caller's `sub esp, eax` still gets the frame size. */
__attribute__((naked)) void _chkstk(void)
{
  __asm__("ret\n\t");
}
