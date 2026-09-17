/*
 * bungie_net/thread_win32.c — lightweight thread pool for Xbox
 * XBE source: c:\halo\SOURCE\bungie_net\common\thread_win32.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x81170  FUN_00081170 (from public_key_crypt.c — same COFF object)
 *   0x81250  FUN_00081250 (from public_key_crypt.c — same COFF object)
 *   0x81300  FUN_00081300 (from public_key_crypt.c — same COFF object)
 *   0x81410  FUN_00081410 (from public_key_crypt.c — same COFF object)
 *   0x81480  FUN_00081480 (from random_numbers.c — same COFF object)
 *   0x81630  thread_new
 *   0x81720  thread_is_done
 *   0x81770  thread_close
 *   0x817e0  create_mutex
 *   0x81870  mutex_acquire (take_mutex)
 *   0x818d0  mutex_release (release_mutex)
 *   0x81910  FUN_00081910
 *   0x81980  FUN_00081980
 */

#include "common.h"

#define MAXIMUM_THREADS 32

/* XDK XAPI imports — called by name through kb.json __stdcall decls
 * (CreateThread 0x1cfd8c, SetThreadPriority 0x1cf999, ResumeThread 0x1cfaec,
 * CloseHandle 0x1cf900, GetExitCodeThread 0x1cfbbd, WaitForSingleObject
 * 0x1d0336, ReleaseMutex 0x1d0099). The former raw fn-pointer casts hid the
 * calling convention from kb.json audits (lift-learnings §30). */

/* WaitForSingleObject return codes */
#define WAIT_OBJECT_0 0x00
#define WAIT_ABANDONED 0x80

#define STILL_ACTIVE 0x103

typedef struct {
  int handle;
  char in_use;
  char pad[3];
} thread_slot_t;

#define g_thread_slots ((thread_slot_t *)0x334990)

/* Running counter used to build unique mutex names ("mutex_%ld"). */
#define g_mutex_name_counter (*(int *)0x334988)

/*
 * FUN_00081170 — generate the 2-dword modulus/base/generator triple.
 *
 * TU: c:\halo\SOURCE\bungie_net\common\public_key_crypt.c (confirmed by the
 * __FILE__ string at 0x265da0 pushed by both asserts below); it links into
 * the same COFF object as the thread_win32 routines.
 *
 * For each of the two dwords: draw p[i] = rand(0xffff) * rand(0xffff) + 2
 * until it is >= 0xffffff, then draw x[i] in [0xff, p[i]-2] and
 * g[i] in [0xff, p[i]-1].
 *
 * Confirmed: assert "x->dwords[i] < (p->dwords[i] - 2)" at line 0xa2,
 * assert "g->dwords[i] < (p->dwords[i] - 1)" at line 0xa3.
 * Confirmed: FUN_00080eb0 called twice with 0xffff; FUN_00081410 called as
 * (0xff, p[i]-2) then (0xff, p[i]-1) — first PUSH is the last argument.
 * Confirmed: the retry compare is JC (unsigned) against 0xffffff.
 * Unknown: the semantic names of the function and of FUN_00080eb0 /
 * FUN_00081410 (no string or symbol evidence); parameter names p/x/g come
 * from the assert strings.
 */
void FUN_00081170(unsigned int *p, unsigned int *x, unsigned int *g)
{
  int i;

  for (i = 0; i < 2; i++) {
    do {
      p[i] = FUN_00080eb0(0xffff) * FUN_00080eb0(0xffff) + 2;
    } while (p[i] < 0xffffff);

    x[i] = (unsigned int)FUN_00081410(0xff, (int)(p[i] - 2));
    g[i] = (unsigned int)FUN_00081410(0xff, (int)(p[i] - 1));

    if (x[i] >= p[i] - 2) {
      display_assert("x->dwords[i] < (p->dwords[i] - 2)",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0xa2, 1);
      system_exit(-1);
    }
    if (g[i] >= p[i] - 1) {
      display_assert("g->dwords[i] < (p->dwords[i] - 1)",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0xa3, 1);
      system_exit(-1);
    }
  }
}

/*
 * 0x81250 - compute a Diffie-Hellman public key from (p, x, g) and dump all
 * four 64-bit values through error() at severity 2.
 *
 * From public_key_crypt.c (same COFF object as FUN_00081170 above), cdecl
 * with four stack params: p@[EBP+8], x@[EBP+0xc], g@[EBP+0x10],
 * public_key@[EBP+0x14].
 *
 * Confirmed: the loop runs exactly 2 iterations ([EBP-8] initialised to 2,
 * DEC/JNZ). The original walks x with a single cursor and reaches the other
 * three arrays through precomputed byte deltas (p-x, g-x, public_key-x)
 * folded into MOV [EDX+ECX*1]; that is a strength-reduction of the plain
 * per-array index used here.
 * Confirmed by register order at 0x81290-0x81298: EDI = g[i], EBX = x[i],
 * ESI = p[i], matching FUN_00081090's declared p@esi / x@ebx / g@edi.
 * Confirmed: EAX from that call is stored to public_key[i] (MOV
 * [EDX+ECX*1],EAX at 0x812a3) -- FUN_00081090 tail-calls FUN_00080fc0,
 * which returns the low dword of its accumulator in EAX.
 * Confirmed: the error() varargs are pushed high-to-low as public_key[1],
 * public_key[0], g[1], g[0], x[1], x[0], p[1], p[0], format, 2 -- first
 * PUSH is the last argument, so the printed order matches the format
 * string at 0x265e2c (read verbatim from the pristine XBE .rdata).
 * Unknown: the semantic name of this function and of FUN_00081090 (no
 * string or symbol evidence); parameter names come from the format string.
 */
void FUN_00081250(unsigned int *p, unsigned int *x, unsigned int *g,
                  unsigned int *public_key)
{
  int i;
  unsigned int result[2];

  for (i = 0; i < 2; i++) {
    result[i] = FUN_00081090(p[i], x[i], g[i]);
    public_key[i] = result[i];
  }

  error(2, "p= %8lX%8lX\nx= %8lX%8lX\ng= %8lX%8lX\npublic key= %8lX%8lX\n\n",
        p[0], p[1], x[0], x[1], g[0], g[1], public_key[0], public_key[1]);
}

/*
 * 0x81300 - compute the shared/private key from (public_key, p, x) and dump
 * all four 64-bit values through error() at severity 2.
 *
 * From public_key_crypt.c (same COFF object as FUN_00081250 above), cdecl
 * with four stack params: public_key@[EBP+8], p@[EBP+0xc], x@[EBP+0x10],
 * private_key@[EBP+0x14]. Parameter names come from the format string at
 * 0x265e68 ("public_key= ... p= ... x= ... private key= ...", read verbatim
 * from the pristine XBE .rdata) and from the assert reason "x<(p-1)".
 *
 * Confirmed: the loop runs exactly 2 iterations ([EBP-4] initialised to 2,
 * DEC/JNZ at 0x813bf-0x813c9). The original walks p with a single cursor
 * (ESI) and reaches the other three arrays through precomputed byte deltas
 * (public_key-p, x-p, private_key-p) folded into MOV [reg+ESI*1]; that is a
 * strength-reduction of the plain per-array index used here.
 * Confirmed: assert reason "p>2" at 0x265de0, line 0x85 (CMP EDI,2 / JA
 * skips the assert, so it fires when p[i] <= 2); assert reason "x<(p-1)" at
 * 0x265dd8, line 0x86 (LEA ECX,[EDI-1] / CMP EBX,ECX / JC skips it, so it
 * fires when x[i] >= p[i] - 1). Both strings read verbatim from the
 * pristine XBE .rdata.
 * Confirmed by register order at 0x8138a-0x81391: EAX = x[i], ECX =
 * public_key[i], EDX = p[i], matching FUN_00080fc0's declared
 * exponent@eax / base@ecx / modulus@edx.
 * Confirmed: the EAX result is byte-reversed at 0x81396-0x813ba (the exact
 * shift/mask sequence kept below) before the store to private_key[i] at
 * 0x813bc.
 * Confirmed: the error() varargs are pushed high-to-low as private_key[1],
 * private_key[0], x[1], x[0], p[1], p[0], public_key[1], public_key[0],
 * format, 2 -- first PUSH is the last argument, so the printed order
 * matches the format string.
 * Unknown: the semantic name of this function and of FUN_00080fc0 (no
 * string or symbol evidence).
 */
void FUN_00081300(unsigned int *public_key, unsigned int *p, unsigned int *x,
                  unsigned int *private_key)
{
  int public_key_delta = (int)public_key - (int)p;
  int x_delta = (int)x - (int)p;
  int private_key_delta = (int)private_key - (int)p;
  unsigned int *cursor = p;
  int count = 2;
  unsigned int base;
  unsigned int exponent;
  unsigned int modulus;
  unsigned int value;

  while (1) {
    exponent = *(unsigned int *)((int)cursor + x_delta);
    modulus = *cursor;
    base = *(unsigned int *)((int)cursor + public_key_delta);

    if (modulus < 3) {
      display_assert("p>2",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x85, 1);
      system_exit(-1);
    }
    if (exponent >= modulus - 1) {
      display_assert("x<(p-1)",
                     "c:\\halo\\SOURCE\\bungie_net\\common\\public_key_crypt.c",
                     0x86, 1);
      system_exit(-1);
    }

    value = FUN_00080fc0(exponent, base, modulus);
    *(unsigned int *)((int)cursor + private_key_delta) =
      (((value & 0xff0000) | (value >> 16)) >> 8) |
      (((value << 16) | (value & 0xff00)) << 8);

    cursor++;
    count--;
    if (count == 0) {
      break;
    }
  }

  error(2,
        "public_key= %8lX%8lX\np= %8lX%8lX\nx= %8lX%8lX\nprivate key= "
        "%8lX%8lX\n\n",
        public_key[0], public_key[1], p[0], p[1], x[0], x[1], private_key[0],
        private_key[1]);
}

/*
 * FUN_00081410 — seeded pseudo-random draw used by the public_key_crypt
 * key generator (callers: FUN_00081170 with (0xff, p-2)/(0xff, p-1) and
 * message_header.c with (0, count-1)).
 *
 * Confirmed (0x81410-0x81475): on the first call — byte 0x334980 still zero —
 * it seeds the CRT generator with srand(crt_time(NULL)) (PUSH 0 / CALL
 * 0x1d9d28 / PUSH EAX / CALL 0x1d9cf9 / ADD ESP,8) and sets the byte to 1.
 * It then returns
 *   low + (int)((double)rand() * (unsigned)high
 *               / ((double)(unsigned)low + 32767.0))
 * Both parameter FILDs carry the MSVC unsigned fixup (TEST/JGE + FADD
 * 4294967296.0 at 0x265d40), so low and high are read as unsigned; the rand()
 * result is loaded signed (FILD [EBP-4], no fixup).  The divisor constant at
 * 0x265eb0 is 32767.0 and the low parameter really is part of the divisor —
 * FILD [EBP+8] at 0x81457, FADD [0x265eb0] at 0x81462, FDIVP at 0x81468 —
 * not a decompiler artifact.  The trailing ADD EAX,ESI adds the raw (signed)
 * low parameter to the _ftol2 result.
 * Unknown: the function's real name and the meaning of byte 0x334980 beyond
 * "CRT generator already seeded".
 */
int FUN_00081410(int low, int high)
{
  int value;

  if (*(char *)0x334980 == 0) {
    FUN_001d9cf9(crt_time((int *)0));
    *(char *)0x334980 = 1;
  }

  value = rand();
  return low + (int)((double)value * (double)(unsigned int)high /
                     ((double)(unsigned int)low + 32767.0));
}

/*
 * FUN_00081480 — pick a pseudo-random 64-bit value between *min and *max.
 *
 * TU: c:\halo\SOURCE\bungie_net\common\random_numbers.c (confirmed by the
 * __FILE__ string at 0x265f08 pushed by all three asserts below); it links
 * into the same COFF object as the thread_win32 routines.
 *
 * Confirmed: parameters are min=[ebp+8] (ESI), max=[ebp+0xc] (EBX),
 * result=[ebp+0x10] (EDI) — the assert compares at 0x81558 and 0x8158a test
 * result against ESI ("result->qword >= min->qword", line 0x3a) and against
 * EBX ("result->qword <= max->qword", line 0x3b) respectively.
 * Confirmed: the seed-once guard is the byte at 0x334980; on the first call
 * srand(crt_time(NULL)) runs (0x814c4-0x814d4, ADD ESP,8 covers both cdecl
 * args) and the byte is set to 1.
 * Confirmed x87 order at 0x814eb-0x81540: FILD the rand() dword first, then
 * build (double)max->qword, FMULP, then build (double)min->qword, FADD the
 * double constant 32767.0 at 0x265eb0, FDIVP. So the scale is
 *   rand() * (double)max->qword / ((double)min->qword + 32767.0)
 * — note the divisor is min, not (max - min); that is what the binary does
 * (it only coincides with the usual "min + range*rand/RAND_MAX" idiom when
 * min is 0), and it is why the two range asserts exist.
 * Confirmed: each unsigned-64 -> double conversion is open-coded as
 * (double)(int64)(v & 0x7fffffffffffffff) - (double)(int64)(v & 1<<63)
 * (FILD low63, FILD signbit, FCHS, FADDP).
 * Confirmed: the _ftol2 result (EDX:EAX) is stored to a local qword and
 * passed as math64_add(min, &offset, result) — first PUSH is the last arg.
 * Unknown: the semantic name of this function; parameter names min/max/
 * result come from the assert strings.
 */
void FUN_00081480(unsigned int *min, unsigned int *max, unsigned int *result)
{
  unsigned int rand_value;
  uint64_t min_qword;
  uint64_t max_qword;
  int64_t offset;

  if (min == 0 || max == 0 || result == 0) {
    display_assert("min && max && result",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\random_numbers.c",
                   0x2e, 1);
    system_exit(-1);
  }

  if (*(char *)0x334980 == 0) {
    FUN_001d9cf9((unsigned int)crt_time(0));
    *(char *)0x334980 = 1;
  }

  rand_value = (unsigned int)rand();
  max_qword = *(uint64_t *)max;
  min_qword = *(uint64_t *)min;

  offset = (int64_t)((double)(int)rand_value *
                     ((double)(int64_t)(max_qword & 0x7fffffffffffffffULL) -
                      (double)(int64_t)(max_qword & 0x8000000000000000ULL)) /
                     (((double)(int64_t)(min_qword & 0x7fffffffffffffffULL) -
                       (double)(int64_t)(min_qword & 0x8000000000000000ULL)) +
                      32767.0));

  math64_add((const uint16_t *)min, (const uint16_t *)&offset,
             (uint16_t *)result);

  if (result[1] < min[1] || (result[1] == min[1] && result[0] < min[0])) {
    display_assert("result->qword >= min->qword",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\random_numbers.c",
                   0x3a, 1);
    system_exit(-1);
  }
  if (result[1] > max[1] || (result[1] == max[1] && result[0] > max[0])) {
    display_assert("result->qword <= max->qword",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\random_numbers.c",
                   0x3b, 1);
    system_exit(-1);
  }
}

/*
 * FUN_000815f0 — claim the first available 0x28-byte mutex slot.
 *
 * Confirmed: scans byte 0x334ab4 with stride 0x28 through 0x334fb4; on
 * the first zero it clears dword 0x334a90 + index*0x28 and byte +4, then sets
 * byte +0x24. Returns the slot pointer in EAX, or NULL when the pool is full
 * (XOR EAX,EAX at 0x815f0, LEA EAX at 0x81612 — confirmed by disassembly).
 * Unknown: the original name.
 */
__declspec(noinline) void *FUN_000815f0(void)
{
  char *slot_in_use;
  int index;
  int *slot;

  index = 0;
  slot_in_use = (char *)0x334ab4;
  do {
    if (*slot_in_use == 0) {
      slot = (int *)(0x334a90 + index * 0x28);
      *(char *)(slot + 1) = 0;
      *slot = 0;
      *(char *)((char *)slot + 0x24) = 1;
      return slot;
    }
    slot_in_use = slot_in_use + 0x28;
    index = index + 1;
  } while ((int)slot_in_use < 0x334fb4);
  return NULL;
}

/* 0x815c0 — get_thread_from_pool */
int get_thread_from_pool(void)
{
  int i;
  for (i = 0; i < MAXIMUM_THREADS; i++) {
    if (g_thread_slots[i].in_use == 0) {
      return i;
    }
  }
  return 0;
}

/*
 * thread_new — allocate a thread slot and create an Xbox thread.
 *
 * Searches the 32-slot pool for an unused entry, creates a suspended thread
 * via CreateThread, sets its priority based on priority_flags bits, then
 * resumes it. Returns true on success.
 *
 * Confirmed: assert "function" at line 0x6b, "thread_reference" at line 0x6c.
 * Confirmed: CREATE_SUSPENDED (0x4) flag, stack size 0x4000.
 * Confirmed: priority_flags bit 0x2 = below-normal (-1), bit 0x4 = above-normal
 * (+1).
 */
bool thread_new(int priority_flags, void *function, int param,
                void **thread_reference)
{
  void **ref = thread_reference;
  thread_slot_t *slot = NULL;
  int i;
  int handle;
  int priority;

  if (function == NULL) {
    display_assert("function",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0x6b,
                   1);
    system_exit(-1);
  }
  if (ref == NULL) {
    display_assert("thread_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0x6c,
                   1);
    system_exit(-1);
  }

  for (i = 0; i < MAXIMUM_THREADS; i++) {
    if (g_thread_slots[i].in_use == 0) {
      slot = &g_thread_slots[i];
      slot->handle = 0;
      slot->in_use = 1;
      break;
    }
  }

  if (slot != NULL) {
    handle = (int)CreateThread(NULL, 0x4000, function, (void *)param, 4,
                               (int *)&function);
    slot->handle = handle;
    if (handle != 0) {
      priority = 0;
      if ((priority_flags & 2) != 0) {
        priority = -1;
      } else if ((priority_flags & 4) != 0) {
        priority = 1;
      }
      if (SetThreadPriority(handle, priority) != 0 &&
          ResumeThread(slot->handle) != -1) {
        *ref = slot;
        return true;
      }
      CloseHandle(slot->handle);
      *ref = NULL;
      return false;
    }
  }

  *ref = slot;
  return false;
}

/*
 * thread_is_done — check whether a thread has finished executing.
 *
 * Calls GetExitCodeThread and returns true if the thread exited (i.e. the
 * exit code is not STILL_ACTIVE).
 *
 * Confirmed: assert "thread_reference" at line 0x98.
 * Confirmed: compares exit code against 0x103 (STILL_ACTIVE).
 */
bool thread_is_done(void *thread_reference)
{
  thread_slot_t *slot = (thread_slot_t *)thread_reference;
  bool is_done = false;
  int exit_code;

  if (slot == NULL) {
    display_assert("thread_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0x98,
                   1);
    system_exit(-1);
  }

  if (GetExitCodeThread(slot->handle, &exit_code) != 0) {
    if (exit_code != STILL_ACTIVE) {
      is_done = true;
    }
  }

  return is_done;
}

/*
 * thread_close — close a thread handle and release its slot.
 *
 * Confirmed: assert "thread_reference" at line 0xa8.
 * Confirmed: assert "thread_reference->in_use" at line 0xa9.
 * Confirmed: calls CloseHandle, then zeroes handle and in_use.
 */
void thread_close(void *thread_reference)
{
  thread_slot_t *slot = (thread_slot_t *)thread_reference;

  if (slot == NULL) {
    display_assert("thread_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xa8,
                   1);
    system_exit(-1);
  }
  if (slot->in_use == 0) {
    display_assert("thread_reference->in_use",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xa9,
                   1);
    system_exit(-1);
  }

  CloseHandle(slot->handle);
  slot->handle = 0;
  slot->in_use = 0;
}

/*
 * create_mutex — claim a mutex slot and create a named Win32 mutex in it.
 *
 * Mutex slot layout (stride 0x28, pool 0x334a90 .. 0x334fb4):
 *   +0x00 int  handle      (CreateMutexA result; deref'd by take/release_mutex)
 *   +0x04 char name[0x20]  (snprintf destination, size 0x20)
 *   +0x24 char in_use      (assert "mutex_reference->in_use", FUN_00081910)
 *
 * Confirmed: assert "mutex_reference" at line 0xb8.
 * Confirmed: the counter at 0x334988 is read, the OLD value is formatted and
 * the stored value is incremented (MOV ECX,EAX / INC EAX / MOV [0x334988],EAX).
 * Confirmed: the counter is read only on the slot-found path (0x81817 is inside
 * the JZ-not-taken branch).
 * Confirmed: snprintf(slot+4, 0x20, "mutex_%ld", counter) — first PUSH is the
 * last argument; CreateMutexA(NULL, FALSE, slot+4).
 * Confirmed: the handle is stored into the slot before the success test, and
 * the out parameter is set to NULL when CreateMutexA fails.
 */
bool create_mutex(int **mutex_reference)
{
  int *slot;
  int counter;
  int handle;

  if (mutex_reference == NULL) {
    display_assert("mutex_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xb8,
                   1);
    system_exit(-1);
  }

  slot = (int *)FUN_000815f0();
  if (slot == NULL) {
    *mutex_reference = NULL;
    return 0;
  }

  counter = g_mutex_name_counter;
  g_mutex_name_counter = counter + 1;
  snprintf((char *)(slot + 1), 0x20, "mutex_%ld", counter);

  handle = (int)CreateMutexA(NULL, 0, (const char *)(slot + 1));
  *slot = handle;
  if (handle == 0) {
    *mutex_reference = NULL;
    return 0;
  }
  *mutex_reference = slot;
  return 1;
}

/*
 * take_mutex — acquire a mutex with a timeout.
 *
 * Calls WaitForSingleObject(*mutex_reference, timeout_ms). Returns true if the
 * wait succeeded (WAIT_OBJECT_0 = 0) or the mutex was abandoned (WAIT_ABANDONED
 * = 0x80). Returns false on timeout or any other error.
 *
 * Confirmed: assert "mutex_reference" at line 0xd3.
 * Confirmed: WaitForSingleObject at 0x1d0336; success codes 0x00 and 0x80.
 */
bool take_mutex(int *mutex_reference, int timeout_ms)
{
  bool success = false;
  int result;

  if (mutex_reference == NULL) {
    display_assert("mutex_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xd3,
                   1);
    system_exit(-1);
  }
  result = WaitForSingleObject(*mutex_reference, timeout_ms);
  if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
    success = true;
  }
  return success;
}

/*
 * release_mutex — release a mutex.
 *
 * Calls ReleaseMutex(*mutex_reference) via the XDK thunk at 0x1d0099
 * (NtReleaseMutant wrapper). Returns void.
 *
 * Confirmed: assert "mutex_reference" at line 0xe6.
 * Confirmed: ReleaseMutex at 0x1d0099.
 */
void release_mutex(int *mutex_reference)
{
  if (mutex_reference == NULL) {
    display_assert("mutex_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xe6,
                   1);
    system_exit(-1);
  }
  ReleaseMutex(*mutex_reference);
}

/*
 * FUN_00081910 — close a mutex handle and clear its reference.
 *
 * Confirmed: asserts "mutex_reference" at line 0xf0 and
 * "mutex_reference->in_use" at line 0xf1; calls CloseHandle before clearing
 * offsets +0x04, +0x00, and +0x24, in that order.
 */
void FUN_00081910(int *mutex_reference)
{
  if (mutex_reference == NULL) {
    display_assert("mutex_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xf0,
                   1);
    system_exit(-1);
  }
  if (*(char *)((char *)mutex_reference + 0x24) == 0) {
    display_assert("mutex_reference->in_use",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0xf1,
                   1);
    system_exit(-1);
  }

  CloseHandle(*mutex_reference);
  *(char *)((char *)mutex_reference + 4) = 0;
  *mutex_reference = 0;
  *(char *)((char *)mutex_reference + 0x24) = 0;
}


/*
 * FUN_00081980 — allocate and initialize a 0x18-byte transport address copy.
 *
 * Confirmed: asserts transport_initialized and address at transport_address.c
 * lines 0x1e and 0x1f; debug_malloc(0x18, false, __FILE__, 0x21) returns the
 * copied address. The four dword input fields and two word parameters occupy
 * output offsets +0x00 through +0x13; output offset +0x14 is zeroed.
 */
typedef struct {
  unsigned int field_00;
  unsigned int field_04;
  unsigned int field_08;
  unsigned int field_0c;
  unsigned short field_10;
  unsigned short field_12;
  unsigned int field_14;
} transport_address_copy_t;

void *FUN_00081980(const unsigned int *address, unsigned short field_10,
                   unsigned short field_12)
{
  transport_address_copy_t *result;

  if (*(char *)0x335090 == 0) {
    display_assert("transport_initialized",
                   "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c",
                   0x1e, 1);
    system_exit(-1);
  }
  if (address == NULL) {
    display_assert("address",
                   "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c",
                   0x1f, 1);
    system_exit(-1);
  }

  result = (transport_address_copy_t *)debug_malloc(
    0x18, 0, "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c",
    0x21);
  if (result != NULL) {
    result->field_00 = address[0];
    result->field_04 = address[1];
    result->field_08 = address[2];
    result->field_0c = address[3];
    result->field_10 = field_10;
    result->field_12 = field_12;
    result->field_14 = 0;
  }
  return result;
}


/*
 * FUN_00081a20 — free a transport-address allocation.
 *
 * Confirmed: asserts transport_initialized and address at transport_address.c
 * lines 0x2f and 0x30, then calls debug_free(address, __FILE__, 0x32).
 */
void FUN_00081a20(void *address)
{
  if (*(char *)0x335090 == 0) {
    display_assert("transport_initialized",
                   "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c",
                   0x2f, 1);
    system_exit(-1);
  }
  if (address == NULL) {
    display_assert("address",
                   "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c",
                   0x30, 1);
    system_exit(-1);
  }
  debug_free(address,
             "c:\\halo\\SOURCE\\bungie_net\\network\\transport_address.c",
             0x32);
}
