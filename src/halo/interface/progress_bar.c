/*
 * interface/progress_bar.c — loading progress bar UI
 * XBE source: c:\halo\SOURCE\interface\progress_bar.c
 *
 * Re-implemented functions (by XBE address, ascending):
 *   0xe1c70  progress_bar_initialize
 *   0xe1c80  progress_bar_dispose
 *   0xe1c90  progress_bar_begin
 *   0xe1cc0  progress_bar_end
 *   0xe1ce0  ui_automation_is_active
 *   0xe3300  progress_bar_display
 */

#include "common.h"

/* XDK SetThreadPriority thunk (stdcall) */
typedef int(__stdcall *SetThreadPriority_fn)(int handle, int priority);
#define XSetThreadPriority ((SetThreadPriority_fn)0x1cf999)

/* Progress bar globals */
static char *progress_bar_suppress(void)
{
  return (char *)0x46c3ec;
}

static char *progress_bar_active(void)
{
  return (char *)0x46c3f8;
}

static int *progress_bar_screen_initialized(void)
{
  return (int *)0x46c3f0;
}

static float *progress_bar_start_progress(void)
{
  return (float *)0x46c214;
}

static char *progress_bar_rendering_enabled(void)
{
  return (char *)0x30f030;
}

/* progress_bar_initialize — no-op stub. */
void progress_bar_initialize(void)
{
}

/* progress_bar_dispose — no-op stub. */
void progress_bar_dispose(void)
{
}

/*
 * progress_bar_begin — start a progress bar session.
 *
 * Sets the suppress flag to the inverse of the argument, marks the bar
 * as active, and raises the current thread to highest priority.
 *
 * Confirmed: SETZ AL stores !(param_1) to 0x46c3ec.
 * Confirmed: MOV byte ptr [0x46c3f8], 1.
 * Confirmed: SetThreadPriority(-2, 2) — current thread,
 * THREAD_PRIORITY_HIGHEST.
 */
void progress_bar_begin(bool suppress)
{
  *progress_bar_suppress() = (suppress == 0);
  *progress_bar_active() = 1;
  XSetThreadPriority(-2, 2);
}

/*
 * progress_bar_end — finish a progress bar session.
 *
 * Clears the active flag and restores the current thread to normal priority.
 *
 * Confirmed: MOV byte ptr [0x46c3f8], 0.
 * Confirmed: SetThreadPriority(-2, 0) — current thread, THREAD_PRIORITY_NORMAL.
 */
void progress_bar_end(void)
{
  *progress_bar_active() = 0;
  XSetThreadPriority(-2, 0);
}

/*
 * ui_automation_is_active — return whether a progress bar is currently active.
 *
 * Confirmed: MOV AL, [0x46c3f8] / RET — single byte load, returns bool.
 */
bool ui_automation_is_active(void)
{
  return *progress_bar_active();
}

/*
 * progress_bar_display — update the progress bar with the current progress.
 *
 * Asserts progress is in [0.0, 1.0]. If the bar is active and progress > 0,
 * initializes the progress screen on first call (0xe29a0), stores the start
 * progress, and if rendering is enabled, calls the render function (0xe2e50)
 * with the normalized progress fraction.
 *
 * Confirmed: FCOMP against 0.0f at 0x2533c0 and 1.0f at 0x2533c8.
 * Confirmed: assert "(progress>=0.f) && (progress<=1.f)" at line 0x410.
 * Confirmed: CALL 0xe29a0 (progress screen init), CALL 0xe2e50 (render).
 * Confirmed: normalized = (progress - start) / (1.0f - start).
 */
void progress_bar_display(float progress)
{
  if (progress < 0.0f || progress > 1.0f) {
    display_assert("(progress>=0.f) && (progress<=1.f)",
                   "c:\\halo\\SOURCE\\interface\\progress_bar.c", 0x410, 1);
    system_exit(-1);
  }

  if (*progress_bar_active() != 0 && progress > 0.0f) {
    if (*progress_bar_screen_initialized() == 0) {
      progress_bar_screen_initialize();
      *progress_bar_start_progress() = progress;
    }
    if (*progress_bar_rendering_enabled() != 0) {
      float normalized = (progress - *progress_bar_start_progress()) /
                         (1.0f - *progress_bar_start_progress());
      progress_bar_render(normalized);
    }
  }
}
