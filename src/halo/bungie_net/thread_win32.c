/*
 * bungie_net/thread_win32.c — lightweight thread pool for Xbox
 * XBE source: c:\halo\SOURCE\bungie_net\common\thread_win32.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0x81630  thread_new
 *   0x81720  thread_is_done
 *   0x81770  thread_close
 */

#include "common.h"

#define MAXIMUM_THREADS 32

/* XDK XAPI import thunks (stdcall) */
typedef int(__stdcall *CreateThread_fn)(void *attrs, int stack_size,
                                        void *start, int param,
                                        int creation_flags, int *thread_id);
typedef int(__stdcall *SetThreadPriority_fn)(int handle, int priority);
typedef int(__stdcall *ResumeThread_fn)(int handle);
typedef void(__stdcall *CloseHandle_fn)(int handle);
typedef int(__stdcall *GetExitCodeThread_fn)(int handle, int *exit_code);

#define XCreateThread ((CreateThread_fn)0x1cfd8c)
#define XSetThreadPriority ((SetThreadPriority_fn)0x1cf999)
#define XResumeThread ((ResumeThread_fn)0x1cfaec)
#define XCloseHandle ((CloseHandle_fn)0x1cf900)
#define XGetExitCodeThread ((GetExitCodeThread_fn)0x1cfbbd)

#define STILL_ACTIVE 0x103

typedef struct {
  int handle;
  char in_use;
  char pad[3];
} thread_slot_t;

/* 32-entry thread slot array at 0x334990 */
static thread_slot_t *thread_slots(void)
{
  return (thread_slot_t *)0x334990;
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
  thread_slot_t *slots = thread_slots();
  thread_slot_t *slot = NULL;
  int i;
  int thread_id;
  int handle;
  int priority;

  if (function == NULL) {
    display_assert("function",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0x6b,
                   1);
    system_exit(-1);
  }
  if (thread_reference == NULL) {
    display_assert("thread_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0x6c,
                   1);
    system_exit(-1);
  }

  for (i = 0; i < MAXIMUM_THREADS; i++) {
    if (slots[i].in_use == 0) {
      slot = &slots[i];
      slot->handle = 0;
      slot->in_use = 1;
      break;
    }
  }

  if (slot != NULL) {
    handle = XCreateThread(NULL, 0x4000, function, param, 4, &thread_id);
    slot->handle = handle;
    if (handle != 0) {
      priority = 0;
      if ((priority_flags & 2) != 0) {
        priority = -1;
      } else if ((priority_flags & 4) != 0) {
        priority = 1;
      }
      if (XSetThreadPriority(handle, priority) != 0) {
        if (XResumeThread(slot->handle) != -1) {
          *thread_reference = slot;
          return true;
        }
      }
      XCloseHandle(slot->handle);
      *thread_reference = NULL;
      return false;
    }
  }

  *thread_reference = slot;
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
  int exit_code;

  if (slot == NULL) {
    display_assert("thread_reference",
                   "c:\\halo\\SOURCE\\bungie_net\\common\\thread_win32.c", 0x98,
                   1);
    system_exit(-1);
  }

  if (XGetExitCodeThread(slot->handle, &exit_code) == 0) {
    return false;
  }
  if (exit_code == STILL_ACTIVE) {
    return false;
  }
  return true;
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

  XCloseHandle(slot->handle);
  slot->handle = 0;
  slot->in_use = 0;
}
