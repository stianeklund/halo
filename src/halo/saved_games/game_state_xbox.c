/*
 * game_state_xbox.c — Xbox-specific game state buffer and file management.
 *
 * Corresponds to game_state_xbox.obj (game_state_xbox.c in the original
 * source tree at c:\halo\SOURCE\saved games\game_state_xbox.c).
 */

#ifdef XDK_BUILD
void __stdcall MmFreeContiguousMemory(void *BaseAddress);
#else
#include "xbox.h"
#endif

/* Xbox kernel file I/O wrappers (stdcall) */
typedef bool(__stdcall *close_handle_fn)(int handle);
typedef int(__stdcall *set_file_pointer_fn)(int handle, int distance_to_move,
                                            int *distance_high,
                                            uint32_t move_method);
typedef bool(__stdcall *read_file_fn)(int handle, void *buffer,
                                      uint32_t number_of_bytes_to_read,
                                      int *number_of_bytes_read,
                                      void *overlapped);
typedef bool(__stdcall *write_file_fn)(int handle, void *buffer,
                                       uint32_t number_of_bytes_to_write,
                                       int *number_of_bytes_written,
                                       void *overlapped);
typedef bool(__stdcall *delete_file_fn)(const char *path);
typedef int (*open_save_file_fn)(int param_1);
typedef char (*get_save_path_fn)(short index, void *out_path);
typedef int (*get_last_error_fn)(void);
typedef void (*crc_begin_fn)(uint32_t *checksum);

#define XCloseHandle ((close_handle_fn)0x1cf900)
#define XSetFilePointer ((set_file_pointer_fn)0x1d1610)
#define XReadFile ((read_file_fn)0x1d13c9)
#define XWriteFile ((write_file_fn)0x1d14b6)
#define XDeleteFile ((delete_file_fn)0x1d0ff9)
#define xapi_GetLastError ((get_last_error_fn)0x1d2240)
#define xbox_game_state_open_file ((open_save_file_fn)0x1c0780)
#define xbox_saved_game_get_path ((get_save_path_fn)0xe0bf0)
#define crc_checksum_begin ((crc_begin_fn)0x1190b0)

/* xbox_game_state_globals layout (at 0x4ea9b0):
 *   +0x00 (0x4ea9b0): char  buffer_allocated
 *   +0x04 (0x4ea9b4): void* buffer
 *   +0x08 (0x4ea9b8): int   buffer_size
 *   +0x0C (0x4ea9bc): char  file_open
 *   +0x0D (0x4ea9bd): char  file_written
 *   +0x10 (0x4ea9c0): int   file_handle
 */

/* 0x1c0220
 * Release the contiguous physical memory buffer allocated for Xbox game-state
 * saves. Asserts that the buffer is marked allocated before freeing, then
 * clears the flag. The globals at 0x4ea9b0 (buffer_allocated flag) and
 * 0x4ea9b4 (buffer pointer) belong to xbox_game_state_globals.
 */
void xbox_game_state_dispose_buffer(void)
{
  assert_halt(*(char *)0x4ea9b0);
  MmFreeContiguousMemory(*(void **)0x4ea9b4);
  *(char *)0x4ea9b0 = 0;
}

/* 0x1c0330
 * Close the Xbox game-state file handle. Asserts that the file is marked open,
 * then calls the close-file routine and clears the open flag.
 */
void xbox_game_state_close_file(void)
{
  assert_halt(*(char *)0x4ea9bc);
  XCloseHandle(*(int *)0x4ea9c0);
  *(char *)0x4ea9bc = 0;
}

/* 0x1c0370
 * Write the game-state buffer to the save file. Asserts that both the buffer
 * is allocated and the file is open, seeks to the beginning, then writes the
 * entire buffer. Sets the file_written flag on success.
 */
char game_state_write_to_file(void)
{
  int bytes_written;

  assert_halt(*(char *)0x4ea9b0); /* buffer_allocated */
  assert_halt(*(char *)0x4ea9bc); /* file_open */

  if (XSetFilePointer(*(int *)0x4ea9c0, 0, NULL, 0) != -1) {
    if (XWriteFile(*(int *)0x4ea9c0, *(void **)0x4ea9b4, *(uint32_t *)0x4ea9b8,
                   &bytes_written, NULL) &&
        bytes_written == *(int *)0x4ea9b8) {
      *(char *)0x4ea9bd = 1; /* file_written */
      return 1;
    }
  }

  display_assert(csprintf((char *)0x5ab100,
                          "couldn't write saved game file (#%d)",
                          xapi_GetLastError()),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x84, 1);
  system_exit(-1);
  return 0;
}

/* 0x1c0720 — return the Xbox save-game filename.
 * Used as the leaf file name when constructing the save path. */
const char *FUN_001c0720(void)
{
  return "savegame.bin";
}

/* 0x1c0750
 * Delete the local player's save game file. Gets the profile directory
 * path and, if valid, deletes the file at that path. */
void FUN_001c0750(void)
{
  char path_buffer[256];

  if (xbox_saved_game_get_path(0, path_buffer)) {
    XDeleteFile(path_buffer);
  }
}

/* 0x1c0910
 * Read and verify a saved game from persistent storage. Reads header first,
 * then checksums the remaining data in 128KB chunks. Returns 1 on success.
 *
 * param_1 (header):      destination buffer for the header portion
 * param_2 (scratch):     pointer to a uint32_t holding the expected checksum;
 *                         zeroed before CRC computation and restored on
 * mismatch param_3 (header_size): byte count for the header read param_4
 * (buffer_size): total byte count (header + body) to checksum param_5 (flags):
 * optional output byte; set to 1 if checksum mismatch with a non-zero expected
 * checksum
 */
char game_state_read_header_from_persistent_storage(void *header,
                                                    uint32_t *scratch,
                                                    int header_size,
                                                    int buffer_size,
                                                    char *flags)
{
  static char scratch_buffer[0x20000]; /* 128KB — avoids _chkstk */
  char path_buffer[0x100];

  int file_handle;
  char result;
  int bytes_transferred;
  uint32_t checksum;
  uint32_t saved_checksum;
  int remaining;
  int chunk;

  file_handle = xbox_game_state_open_file(0);
  result = 0;

  if (flags != NULL) {
    *flags = 0;
  }

  if (file_handle == -1) {
    return result;
  }

  /* Seek to beginning */
  if (XSetFilePointer(file_handle, 0, NULL, 0) == -1) {
    goto read_error;
  }

  /* Read the header */
  if (!XReadFile(file_handle, header, (uint32_t)header_size, &bytes_transferred,
                 NULL) ||
      bytes_transferred != header_size) {
    goto read_error;
  }

  /* Save the expected checksum and prepare for computation */
  saved_checksum = *scratch;
  crc_checksum_begin(&checksum);
  *scratch = 0;
  crc_checksum_buffer(&checksum, header, header_size);

  /* Read and checksum remaining data in 128KB chunks */
  remaining = buffer_size - header_size;
  while (remaining > 0) {
    chunk = remaining;
    if ((unsigned int)remaining > 0x20000) {
      chunk = 0x20000;
    }

    if (XReadFile(file_handle, scratch_buffer, (uint32_t)chunk,
                  &bytes_transferred, NULL) &&
        bytes_transferred == chunk) {
      crc_checksum_buffer(&checksum, scratch_buffer, chunk);
    }

    sound_idle(); /* sound_pump / idle tick */
    remaining -= chunk;
  }

  /* Verify checksum */
  if (checksum == saved_checksum) {
    result = 1;
    XCloseHandle(file_handle);
    return result;
  }

  /* Checksum mismatch */
  if (flags != NULL && saved_checksum != 0) {
    *flags = 1;
  }
  error(2, "checksum failed on persistent storage");
  XCloseHandle(file_handle);
  return result;

read_error:
  display_assert(csprintf((char *)0x5ab100,
                          "couldn't read header from persistent storage (#%d)",
                          xapi_GetLastError()),
                 "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x12a, 1);
  system_exit(-1);

  /* After the fatal assert, attempt to delete the corrupt save file */
  if (xbox_saved_game_get_path(0, path_buffer)) {
    XDeleteFile(path_buffer);
  }
  XCloseHandle(file_handle);
  return result;
}

/* 0x1c0c20
 * Read game-state data from persistent storage into a caller-supplied buffer.
 * Opens the save file, seeks to the beginning, reads `size` bytes into `dst`,
 * and verifies the byte count. On failure, asserts with the last error code,
 * exits, and attempts to delete the corrupt save file.
 */
void FUN_001c0c20(void *dst, int size)
{
  char path_buffer[0x100];
  int bytes_read;

  int file_handle = xbox_game_state_open_file(0);
  if (file_handle == -1) {
    return;
  }

  if (XSetFilePointer(file_handle, 0, NULL, 0) == -1 ||
      !XReadFile(file_handle, dst, size, &bytes_read, NULL) ||
      bytes_read != size) {
    display_assert(
      csprintf((char *)0x5ab100, "failed to read from persistent storage (#%d)",
               xapi_GetLastError()),
      "c:\\halo\\SOURCE\\saved games\\game_state_xbox.c", 0x17f, 1);
    system_exit(-1);

    if (xbox_saved_game_get_path(0, path_buffer)) {
      XDeleteFile(path_buffer);
    }
  }

  XCloseHandle(file_handle);
}

/* 0x1c0cd0
 * Close the file handle obtained from xbox_game_state_open_file for a given
 * parameter. Opens the file, and if valid, closes the resulting handle.
 */
void FUN_001c0cd0(int param_1)
{
  int handle;

  handle = xbox_game_state_open_file(param_1);
  if (handle != -1) {
    XCloseHandle(handle);
  }
}

/* 0x1c0cf0
 * Wait for any in-flight asynchronous player profile writes to complete, then
 * clear the profile write state buffer at 0x4ea9c8 (0x6c bytes).
 */
void FUN_001c0cf0(void)
{
  if (*(int *)0x4eaa2c != 0) {
    error(2, "waiting for asynchronous player profile writes to finish...");
    do {
      /* spin until the async write thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaa2c));
    thread_close(*(void **)0x4eaa2c);
    *(int *)0x4eaa2c = 0;
  }
  csmemset((void *)0x4ea9c8, 0, 0x6c);
}

/* 0x1c0d70
 * Delete a player profile by index. Calls the saved-game file deletion
 * routine and logs an error if it fails.
 */
void FUN_001c0d70(int param_1)
{
  if (param_1 != -1) {
    if (!delete_enumerated_saved_game_file(param_1)) {
      error(2, "player_profile_delete() failed (profile index= #0x%lX)",
            param_1);
    }
  }
}
