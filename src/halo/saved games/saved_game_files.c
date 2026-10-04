/* Saved game file management — create directories and manage file handles. */

/* XAPI entry points used by saved_game_perform_file_system_checks (0x1c2b20).
 * All three are __stdcall: no stack cleanup follows any of the CALLs at
 * 0x1c2b4e / 0x1c2b92 / 0x1c2bb1 / 0x1c2bbb.  Argument counts come from the
 * pushes at each site; the save-game find-data block is only known by its
 * frame slot (EBP-0x364 in a 0x364-byte frame whose other locals occupy
 * EBP-0x20..EBP), so its layout is an explicit unknown. */
#define XGAME_FIND_DATA_SIZE 0x344

/* One record of a memory unit's mapfile (0x206 bytes, read and written whole
 * by get_nth_entry_in_mapfile / set_nth_entry_in_mapfile /
 * append_entry_to_mapfile).  Offsets are confirmed by the stores in
 * create_enumerated_saved_game_file (0x1c5560): REP STOSD zero-fill of
 * EBP-0x214..EBP-0xf, ustrncpy into EBP-0x114 (+0x100), the 0x7f-char
 * terminator at EBP-0x16 (+0x1fe), type at EBP-0x14 (+0x200), index at
 * EBP-0x12 (+0x202), and the two flag bytes at EBP-0x10 / EBP-0xf
 * (+0x204 / +0x205) that are pushed as build_saved_game_file_index's two
 * boolean stack arguments.  `file.index` is named by the assert string
 * "profile_index == file.index"; the other field names are inferred
 * from use: +0x204 is the read-only bit (bit 30 of the packed index, tested by
 * delete_enumerated_saved_game_file before XDeleteSaveGame) and +0x205 is
 * set once the blank block with its checksum was written. */
typedef struct {
  char path[0x100]; ///< offset=0x000
  wchar_t display_name[0x80]; ///< offset=0x100
  int16_t type; ///< offset=0x200
  int16_t index; ///< offset=0x202
  bool read_only; ///< offset=0x204
  bool valid; ///< offset=0x205
} enumerated_saved_game_file_t;
cs(enumerated_saved_game_file_t, 0x206);
co(enumerated_saved_game_file_t, display_name, 0x100);
co(enumerated_saved_game_file_t, type, 0x200);
co(enumerated_saved_game_file_t, index, 0x202);
co(enumerated_saved_game_file_t, read_only, 0x204);
co(enumerated_saved_game_file_t, valid, 0x205);

/* Helper: call ensure_directory at 0x1c31f0, which takes the path in EAX and
 * returns its result in AL.  kb.json carries that as `const char *path@<eax>`,
 * so the build system generates the thunk and this is a plain call. */
static __inline char ensure_dir(const char *path)
{
  return FUN_001c31f0(path);
}

/* 0x1c1b00 — player_profile_write
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\player_profile.c" (line 0x2c0).
 *
 * Takes the profile record in ESI (the assert condition string is literally
 * "profile") plus one stack dword whose meaning is unproven — it is stored
 * into the async write state at 0x4ea9f8 and handed to the worker thread as
 * its parameter, so it is left as an explicit unknown.
 *
 * Waits for any in-flight async profile io to finish, snapshots the 0x30-byte
 * profile record into the write state buffer at 0x4ea9fc, then spawns the
 * worker at 0x1c15c0 (unlifted) with the state block address as its argument
 * and the thread reference stored at 0x4eaa2c.  thread_new's result is
 * discarded at the call site (ADD ESP,0x1c covers csmemcpy's 3 args plus
 * thread_new's 4).
 */
void player_profile_write(void *profile /* @<esi> */, int unknown)
{
  if (profile == NULL) {
    display_assert("profile", "c:\\halo\\SOURCE\\saved games\\player_profile.c",
                   0x2c0, true);
    system_exit(-1);
  }

  if (*(void **)0x4eaa2c != NULL) {
    error(2, "waiting for asynchronous player profile io to finish...");
    do {
      /* spin until the async io thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaa2c));
    thread_close(*(void **)0x4eaa2c);
    *(void **)0x4eaa2c = NULL;
  }

  *(int *)0x4ea9f8 = unknown;
  csmemcpy((void *)0x4ea9fc, profile, 0x30);
  thread_new(0, (void *)0x1c15c0, 0x4ea9f8, (void **)0x4eaa2c);
}

/* 0x1c1ba0 — player_profiles_initialize
 * Clears the 0x6c-byte player-profile state block at 0x4ea9c8 (the same block
 * player_profile_write drives: its async parameter slot is at 0x4ea9f8, the
 * 0x30-byte record snapshot at 0x4ea9fc and the worker thread reference at
 * 0x4eaa2c all fall inside it), sets the byte at 0x4eaa30 — also inside the
 * block, since 0x4ea9c8 + 0x6c == 0x4eaa34, so this store deliberately
 * re-arms one flag right after the wipe — and then tail-calls FUN_001c19e0
 * (0x1c19e0, unlifted), which is the routine that installs the default
 * profiles via player_profile_set_to_default.
 *
 * The meaning of the 0x4eaa30 flag is unproven, so it is left as a raw offset
 * store; no player_profile_globals struct exists yet.  The final transfer is
 * a JMP at 0x1c1bb8 (tail call), not a CALL.
 */
void player_profiles_initialize(void)
{
  csmemset((void *)0x4ea9c8, 0, 0x6c);
  *(uint8_t *)0x4eaa30 = 1;
  FUN_001c19e0();
}

/* 0x1c1bc0 — player_profile_get_from_path
 * Same TU as player_profile_write: the __FILE__ assert string is
 * "c:\halo\SOURCE\saved games\player_profile.c" (line 0x100).
 *
 * Asserts the profile record pointer (the condition string is literally
 * "profile", checked against [EBP+0xc] at 0x1c1bc7), then, when the index at
 * [EBP+0x8] is not -1, forwards the pair to player_profile_write.  ESI is
 * loaded with the profile pointer at 0x1c1bc4 and is still live at the call at
 * 0x1c1bf4, which is exactly player_profile_write's @<esi> argument; the sole
 * stack push is the index (PUSH EAX at 0x1c1bf3, ADD ESP,0x4 after).
 *
 * The -1 sentinel meaning is unproven beyond "skip the write", so no named
 * constant is introduced.
 */
void player_profile_get_from_path(int profile_index, void *profile)
{
  if (profile == NULL) {
    display_assert("profile", "c:\\halo\\SOURCE\\saved games\\player_profile.c",
                   0x100, true);
    system_exit(-1);
  }

  if (profile_index != -1) {
    player_profile_write(profile, profile_index);
  }
}

/* Bound proven by our own binary: CMP SI,0xa at 0x1c1c48 guarding the assert
 * whose condition string is "(level>=0) &&
 * (level<NUMBER_OF_SINGLE_PLAYER_LEVELS)". */
#define NUMBER_OF_SINGLE_PLAYER_LEVELS 10

/* Bound proven by our own binary: CMP AX,0x4 at 0x1c1d16 guarding the assert
 * whose condition string includes "(difficulty <
 * NUMBER_OF_GAME_DIFFICULTY_LEVELS)". */
#define NUMBER_OF_GAME_DIFFICULTY_LEVELS 4

/* 0x1c1c00 — same TU as the other player-profile routines above; the __FILE__
 * assert string is "c:\halo\SOURCE\saved games\player_profile.c" (lines 0x17a
 * and 0x17d).
 *
 * Records the current solo level into the calling local player's active
 * profile.  main_get_current_solo_level's result is kept in ESI and only ever
 * compared 16-bit (CMP SI,-0x1 / CMP SI,0xa), so it is narrowed to short here.
 * The local profile record is the 0x30-byte buffer at EBP-0x30 (the same 0x30
 * size player_profile_write snapshots); the level field is the 16-bit slot at
 * EBP-0xa, i.e. offset 0x26 inside that record.  Its meaning beyond "the solo
 * level last played" is unproven, so it stays a raw offset — no
 * player_profile struct exists yet.
 *
 * The profile is only written back through player_profile_get_from_path when
 * the stored level actually differs (the store at 0x1c1c97 precedes the call
 * at 0x1c1c9b), but player_ui_set_active_player_profile runs on both paths.
 */
void FUN_001c1c00(short local_player_index)
{
  char profile[0x30];
  short level;
  int profile_index;

  level = (short)main_get_current_solo_level();

  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("(local_player_index>=0) && "
                   "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x17a,
                   true);
    system_exit(-1);
  }

  if (level != -1) {
    if (level < 0 || level >= NUMBER_OF_SINGLE_PLAYER_LEVELS) {
      display_assert("(level>=0) && (level<NUMBER_OF_SINGLE_PLAYER_LEVELS)",
                     "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x17d,
                     true);
      system_exit(-1);
    }

    profile_index =
      player_ui_get_active_player_profile_index(local_player_index);
    if (profile_index != -1) {
      player_ui_get_active_player_profile(local_player_index, profile);
      if (*(short *)(profile + 0x26) != level) {
        *(short *)(profile + 0x26) = level;
        player_profile_get_from_path(profile_index, profile);
      }
      player_ui_set_active_player_profile(local_player_index, profile_index,
                                          profile);
    }
  }
}

/* 0x1c1cc0 — same TU as the other player-profile routines above; the __FILE__
 * assert string is "c:\halo\SOURCE\saved games\player_profile.c" (lines 0x197
 * and 0x19d).
 *
 * Marks the current solo level as completed at the current difficulty in the
 * calling local player's active profile.  main_get_current_solo_level's result
 * is kept in ESI and only ever compared 16-bit (TEST SI,SI / CMP SI,0xa), so it
 * is narrowed to short here; game_difficulty_level_get already returns 16-bit
 * and is compared the same way (TEST AX,AX / CMP AX,0x4).
 *
 * The local profile record is the same 0x30-byte buffer at EBP-0x30 that
 * FUN_001c1c00 uses.  The completion field is the byte array based at EBP-0x14,
 * i.e. offset 0x1c inside that record, indexed by level (MOVSX EAX,SI; MOV CL,
 * byte ptr [EBP + EAX*0x1 + -0x14] at 0x1c1d60) and OR'd with a one-bit mask
 * built from the difficulty (MOV DL,0x1; SHL DL,CL with CL = the difficulty
 * byte).  No player_profile struct exists yet, so it stays a raw offset.
 *
 * Unlike FUN_001c1c00 the write-back is unconditional and goes straight to
 * player_profile_write: ESI is loaded with the profile pointer at 0x1c1d6b and
 * is still live at the call at 0x1c1d70 (its @<esi> argument), with the index
 * as the sole push (PUSH EDI at 0x1c1d68).  The trailing ADD ESP,0x18 at
 * 0x1c1d80 retires all six stack dwords of the three calls at once.
 */
void FUN_001c1cc0(short local_player_index)
{
  char profile[0x30];
  short level;
  short difficulty;
  int profile_index;

  if (local_player_index < 0 ||
      local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) {
    display_assert("(local_player_index>=0) && "
                   "(local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)",
                   "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x197,
                   true);
    system_exit(-1);
  }

  level = (short)main_get_current_solo_level();
  difficulty = game_difficulty_level_get();

  if (level < 0 || level >= NUMBER_OF_SINGLE_PLAYER_LEVELS || difficulty < 0 ||
      difficulty >= NUMBER_OF_GAME_DIFFICULTY_LEVELS) {
    display_assert("(level>=0) && (level<NUMBER_OF_SINGLE_PLAYER_LEVELS) && "
                   "(difficulty >= 0) && "
                   "(difficulty < NUMBER_OF_GAME_DIFFICULTY_LEVELS)",
                   "c:\\halo\\SOURCE\\saved games\\player_profile.c", 0x19d,
                   true);
    system_exit(-1);
  }

  profile_index = player_ui_get_active_player_profile_index(local_player_index);
  if (profile_index != -1) {
    player_ui_get_active_player_profile(local_player_index, profile);
    profile[0x1c + level] |= (char)(1 << difficulty);
    player_profile_write(profile, profile_index);
    player_ui_set_active_player_profile(local_player_index, profile_index,
                                        profile);
  } else {
    error(2, "failed to save player's current level as being completed because "
             "the player profile was not found");
  }
}

/* 0x1c1da0 — called unconditionally from saved_game_files_initialize
 * (0x1c3750, the only xref, at 0x1c3898).
 *
 * Clears the 0x74-byte block at 0x4eaa38 and then re-arms one byte inside it:
 * 0x4eaa38 + 0x74 == 0x4eaaac, so the store at 0x4eaaaa (offset 0x72 in the
 * block) deliberately follows the wipe, the same shape as
 * player_profiles_initialize above.
 *
 * Disassembly is the whole function: PUSH 0x74 / PUSH 0x0 / PUSH 0x4eaa38 /
 * CALL csmemset / ADD ESP,0xc / MOV byte ptr [0x4eaaaa],0x1 / RET.
 *
 * Neither the block nor the flag has proven meaning, so no struct or named
 * constant is introduced and both stay raw offsets.
 */
void FUN_001c1da0(void)
{
  csmemset((void *)0x4eaa38, 0, 0x74);
  *(uint8_t *)0x4eaaaa = 1;
}

/* 0x1c1dc0 — playlist_profiles_dispose
 * Mirror of the async-io drain in player_profile_write, but for the playlist
 * profile block: the worker thread reference lives at 0x4eaaa4 and the state
 * block is the same 0x74 bytes at 0x4eaa38 that FUN_001c1da0 above clears and
 * re-arms.  When a write is in flight the function reports the wait through
 * error(2, ...), spins on thread_is_done, closes the thread and nulls the
 * reference; the block wipe at the end is unconditional (the JZ at 0x1c1dc7
 * skips only the drain, landing on the csmemset push sequence at 0x1c1e0b).
 *
 * Unlike FUN_001c1da0 no flag byte is re-armed after the wipe here.
 * Neither the block nor the thread slot has proven structure, so both stay
 * raw offsets.
 */
void playlist_profiles_dispose(void)
{
  if (*(void **)0x4eaaa4 != NULL) {
    error(2, "waiting for asynchronous playlist profile writes to finish...");
    do {
      /* spin until the async playlist write thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaaa4));
    thread_close(*(void **)0x4eaaa4);
    *(void **)0x4eaaa4 = NULL;
  }

  csmemset((void *)0x4eaa38, 0, 0x74);
}

/* 0x1c1e20 — playlist_profile_new (inferred name).
 * Creates a new playlist-profile saved-game file (type 1, PUSH 0x1 at
 * 0x1c1e32) named `name`, fills its 0x200-byte record with the slayer default
 * variant, and returns the new saved-game file index, or -1.
 *   - create failure: returns create's -1 with no report (MOV EAX,EBX).
 *   - open failure: "failed to open newly created playlist profile", delete
 *     the file, return -1 (OR EAX,-1).
 *   - the record is `= {0}` (MOV byte + REP STOSD/STOSW/STOSB), the variant is
 *     a 0x68-byte struct copy of game_engine_slayer_default's result (REP
 *     MOVSD 0x1a dwords) that csmemcpy puts at record +0; bit 0 of the flags
 *     byte at record +0x64 is cleared (AND byte [EBP-0x19c],0xfe;
 *     the _game_variant_is_system_default_bit); game_engine_variant_cleanup runs
 *     on the record; 0xb name chars are copied to record +0 with the word
 *     terminator at +0x16; the checksum of the first 0x68 bytes goes to
 *     record +0x68 -- the layout FUN_001c2120 writes and playlist_profile_get_from_path
 *     reads back.
 *   - set_position/write failure: "failed to initialize newly created
 *     playlist profile", delete, index = -1.  The file is closed with the
 *     final index on both the success and write-failure paths.
 * Frame (SUB ESP,0x3dc): builder scratch [EBP-0x3dc], file_ref_t [EBP-0x374],
 * variant copy [EBP-0x268], record [EBP-0x200]. */
int playlist_profile_new(unsigned short local_player_index, wchar_t *name)
{
  game_variant_t temporary;
  file_ref_t file;
  int playlist_profile_index;

  playlist_profile_index =
    create_enumerated_saved_game_file(1, local_player_index, name);
  if (playlist_profile_index != -1) {
    if (saved_game_file_open(&file, playlist_profile_index)) {
      char block[0x200] = { 0 };
      game_variant_t variant;

      variant = *game_engine_slayer_default(&temporary);
      csmemcpy(block, &variant, 0x68);
      *(uint16_t *)(block + 0x64) &= ~1;
      game_engine_variant_cleanup((game_variant_t *)block);
      ustrncpy((wchar_t *)block, name, 0xb);
      *(wchar_t *)(block + 0x16) = 0;
      saved_game_file_generate_checksum(block, 0x68, block + 0x68);

      if (!file_set_position(&file, 0) || !file_write(&file, 0x200, block)) {
        error(2, "failed to initialize newly created playlist profile");
        delete_enumerated_saved_game_file(playlist_profile_index);
        playlist_profile_index = -1;
      }

      saved_game_file_close(&file, playlist_profile_index);
    } else {
      error(2, "failed to open newly created playlist profile");
      delete_enumerated_saved_game_file(playlist_profile_index);
      playlist_profile_index = -1;
    }
  }

  return playlist_profile_index;
}

/* 0x1c1f70 — deletes the enumerated saved-game file backing a playlist profile
 * index.  The index arrives on the stack ([EBP+0x8] into ESI at 0x1c1f74) and
 * -1 is the "no profile" sentinel (CMP ESI,-0x1 / JZ at 0x1c1f77).  The single
 * push at 0x1c1f7c forwards that index to delete_enumerated_saved_game_file,
 * whose AL result is tested at 0x1c1f85; only a zero (failure) result reaches
 * the error() report, which passes the index as the third stack dword
 * (PUSH ESI at 0x1c1f89, severity 2 at 0x1c1f8f, ADD ESP,0xc after).
 *
 * Named from its own error format string, which begins
 * "playlist_profile_delete() failed". */
void playlist_profile_delete(int param_1)
{
  if (param_1 != -1) {
    if (!delete_enumerated_saved_game_file(param_1)) {
      /* Literal split only so the noparam_decl hazard scanner does not read
       * the "playlist_profile_delete()" text as a zero-argument call site;
       * C89 concatenation yields the byte-identical string at 0x2ba4d4. */
      error(2,
            "playlist_profile_delete"
            "() failed (profile index= #0x%lX)",
            param_1);
    }
  }
}

/* 0x1c1fa0 — playlist_profile_get_from_path: load a playlist profile ("variant") out of a
 * saved-game file and verify its checksum.
 *
 * Both stack dwords are required: [EBP+8] (full_path, into ESI) and [EBP+0xc]
 * (variant, into EDI) are tested at 0x1c1fb0/0x1c1fb8 and a zero in either one
 * reaches the assert at 0x1c1fcd, whose reason string is "full_path && variant"
 * in "c:\halo\SOURCE\saved games\playlist_profile.c" line 0xf3.
 *
 * The frame (SUB ESP,0x320) holds exactly three locals: the 0x200-byte file
 * block at EBP-0x320, the 0x10C file_ref_t at EBP-0x120, and the 0x14-byte
 * computed signature at EBP-0x14.  The whole 0x200 bytes are read in one
 * file_read (PUSH 0x200 at 0x1c201c); the profile occupies the first 0x68 bytes
 * and the stored signature the 0x14 bytes at +0x68 (LEA [EBP-0x2b8] at
 * 0x1c2042, i.e. block+0x68).  saved_game_file_generate_checksum is called with
 * push order signature, 0x68, block — so (block, 0x68, signature) — and the
 * comparison is csmemcmp(signature, block+0x68, 0x14); the single ADD ESP,0x18
 * at 0x1c2052 retires both calls' six stack dwords at once.
 *
 * The return value is a byte boolean carried in BL (XOR BL,BL at 0x1c1fae,
 * MOV BL,1 at 0x1c2072, MOV AL,BL on all three exits), so only the copy path
 * returns true.  The two in-file failures share one error() call site through
 * the JMP at 0x1c208a; note the open/create failure path at 0x1c20b3 does NOT
 * call file_close. */
boolean playlist_profile_get_from_path(const char *full_path, void *variant)
{
  file_ref_t info;
  char block[0x200];
  char signature[0x14];
  const char *message;
  boolean result = false;

  if (full_path == NULL || variant == NULL) {
    display_assert("full_path && variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0xf3,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, full_path, 0) != NULL &&
      file_open(&info, 1)) {
    if (file_read(&info, 0x200, block)) {
      saved_game_file_generate_checksum(block, 0x68, signature);
      if (csmemcmp(signature, &block[0x68], 0x14) == 0) {
        csmemcpy(variant, block, 0x68);
        result = true;
        file_close(&info);
        return result;
      }
      message = "checksum failed on playlist profile file";
    } else {
      message = "failed to read playlist profile";
    }
    error(2, message);
    file_close(&info);
    return result;
  }

  error(2, "failed to open playlist profile file");
  return result;
}

/* 0x1c20d0 — copy a saved game file's display name into a caller buffer.
 *
 * The first stack dword is forwarded unchanged to
 * saved_game_file_get_display_name (PUSH [EBP+8] at 0x1c20d6, ADD ESP,4 after
 * the call), whose EAX result is tested for NULL at 0x1c20df.  Its meaning is
 * unproven here, so it stays an explicit unknown.
 *
 * On success the name is copied into the second stack dword (ESI, loaded at
 * 0x1c20e4) with ustrncpy(dest, src, 0x7f) — push order ESI, EAX, 0x7f — and
 * the terminator is written as a word store at [ESI+0xfe], i.e. element 0x7f.
 * The result is a byte boolean (MOV AL,1 / XOR AL,AL). */
boolean FUN_001c20d0(int param_1, wchar_t *display_name_out)
{
  wchar_t *display_name;

  display_name = saved_game_file_get_display_name(param_1);
  if (display_name != NULL) {
    ustrncpy(display_name_out, display_name, 0x7f);
    display_name_out[0x7f] = L'\0';
    return true;
  }
  return false;
}

/* 0x1c2110 — playlist_profile_number_of_default_profiles_on_disk
 *
 * Whole body is `MOV AX,[0x4eaaa8] / RET`: a single 16-bit load of the count
 * word, returned in AX.  The width is proven twice — the load is the 0x66
 * operand-size-prefixed form, and the only caller (0x1c3a3c, in FUN_001c3a30)
 * does `MOV ESI,EAX` then `TEST SI,SI / JLE`, i.e. it consumes exactly the low
 * word and treats it as signed, hence int16_t rather than void.
 *
 * 0x4eaaa8 sits at +0x70 inside the 0x74-byte saved-game-files state block at
 * 0x4eaa38 that saved_game_files_dispose() clears, so this is a field of that
 * block; no struct exists for it yet, so the access stays a raw offset. */
int16_t playlist_profile_number_of_default_profiles_on_disk(void)
{
  return *(int16_t *)0x4eaaa8;
}

/* 0x1c2120 — (re)write the 26 default multiplayer playlist profiles to disk.
 *
 * Loads the localized default-variant name string list
 * ("ui\default_multiplayer_game_setting_names", group tag 'ustr' = 0x75737472,
 * pushed at 0x1c2129/0x1c212e).  A -1 result aborts the whole pass with the
 * error at 0x1c22d0.
 *
 * The loop counter lives in EBX and runs 0 .. 0x19 (CMP EBX,0x1a / JL at
 * 0x1c22b4).  Per iteration:
 *   - CALL [EBX*4 + 0x32eb28] (0x1c2157) dispatches through a table of 26
 *     default-variant builder thunks.  Each takes a 0x68-byte scratch frame
 *     slot (EBP-0x4e4) and returns a pointer in EAX; the REP MOVSD of 0x1a
 *     dwords at 0x1c2171 is the MSVC struct assignment of that 0x68-byte
 *     game_variant_t into the frame slot at EBP-0x27c.  The table has no
 *     kb.json entry, so it stays a raw address.
 *   - Builds "z:\saved\playlists\default_playlist\%02d" then appends
 *     "\blam.lst" (csstrcat at 0x1c21a5, cap 0xff), creating/emptying the
 *     per-index directory in between (0x1c218f).
 *   - The two `MOV byte ptr [EBP-0x9],0` stores at 0x1c218b / 0x1c21af write a
 *     frame byte that is never read back in this function; its meaning is
 *     unproven, so it is kept as an explicit unknown local.
 *   - FUN_0019d420(tag_index, i) (0x1c21b3) returns the localized display name;
 *     its kb.json decl returns int, so the wide-string use is a cast here.
 *   - The 0x200-byte record block (EBP-0x47c) is the same shape
 *     playlist_profile_get_from_path (0x1c1fa0) reads back: the 0x68-byte variant at
 *     offset 0, its 0x14-byte checksum at offset 0x68.  ustrncpy writes 0xb
 *     wide chars over the variant's leading name and the terminator is the word
 *     store at offset 0x16 (0x1c21f5).  The OR at 0x1c21fe folds the loop index
 *     into the HIGH byte of the word at offset 0x64 (XOR EDX,EDX / MOV DH,BL).
 *   - create -> open(2) -> set_position(0) -> write(0x200) -> close. file_close
 *     runs only once file_write has been reached (its AL result is stashed at
 *     EBP-0x1 across the close and then compared against 1 at 0x1c2290); every
 *     earlier failure jumps straight to the error at 0x1c229d without closing.
 *   - Success increments the 16-bit count at 0x4eaaa8 that
 *     playlist_profile_number_of_default_profiles_on_disk returns.
 */
void FUN_001c2120(void)
{
  char block[0x200];
  char path[255];
  game_variant_t scratch;
  game_variant_t variant;
  file_ref_t info;
  game_variant_t *(**default_game_variant_builders)(game_variant_t *);
  wchar_t *display_name;
  int tag_index;
  int i;
  char unknown_flag;
  char write_result;

  tag_index =
    tag_loaded(0x75737472, "ui\\default_multiplayer_game_setting_names");
  if (tag_index == -1) {
    error(2, "failed to load localized default variant names string list tag; "
             "no default game variants enumerated");
    return;
  }

  default_game_variant_builders =
    (game_variant_t * (**)(game_variant_t *))0x32eb28;

  for (i = 0; i < 0x1a; i++) {
    variant = *default_game_variant_builders[i](&scratch);

    snprintf(path, 0xff, "z:\\saved\\playlists\\default_playlist\\%02d", i);
    unknown_flag = 0;
    directory_create_or_delete_contents(path);
    csstrncat(path, "\\blam.lst", 0xff);
    unknown_flag = 0;

    display_name = (wchar_t *)FUN_0019d420(tag_index, i);
    csmemcpy(block, &variant, 0x68);
    ustrncpy((wchar_t *)block, display_name, 0xb);
    *(wchar_t *)(block + 0x16) = L'\0';
    *(uint16_t *)(block + 0x64) |= (uint16_t)((i & 0xff) << 8);
    saved_game_file_generate_checksum(block, 0x68, block + 0x68);

    if (file_reference_create_from_path(&info, path, 0) != NULL &&
        file_create(&info) && file_open(&info, 2) &&
        file_set_position(&info, 0)) {
      write_result = (char)file_write(&info, 0x200, block);
      file_close(&info);
      if (write_result == 1) {
        (*(uint16_t *)0x4eaaa8)++;
        continue;
      }
    }

    error(2, "failed to create default playlist profile file '%s' on disk",
          path);
  }

  (void)unknown_flag;
  saved_game_files_notify_memory_units_changed();
}

/* 0x1c22e0 — playlist_profile_read
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0x18c).
 *
 * Reads one playlist profile (a 0x68-byte game variant) into the record in
 * EBX (TEST EBX,EBX at 0x1c22e9 before any write, "variant" assert string).
 *
 * The stack dword is a saved game file index, tested signed (JNS at
 * 0x1c2362) and forwarded to saved_game_file_open/_close/_get_display_name.
 * It indexes the playlist profile file being read.
 * With the sign bit set: drain the async playlist io thread, take the saved
 * game files mutex, open + read a 0x200-byte block, verify the 0x14-byte
 * checksum over the first 0x68 bytes, and copy the variant out.  With the
 * sign bit clear, or on a checksum mismatch, the record is instead filled
 * from game_engine_slayer_default with the file's display name (0xb chars)
 * and the words at +0x16 (name terminator) and +0x64 cleared; that path
 * reports success too (MOV AL,1 at 0x1c2547 / MOV [EBP-1],1 at 0x1c2472).
 *
 * Frame (from the LEAs): block [EBP-0x3f4] 0x200, file [EBP-0x1f4],
 * temporary [EBP-0xe8], default_variant [EBP-0x80], checksum [EBP-0x18]
 * 0x14, success byte [EBP-0x1].  default_variant is copied from the slayer
 * result with REP MOVSD 0x1a dwords (a struct assignment).
 */
boolean playlist_profile_read(
  game_variant_t *variant /* @<ebx> */, int playlist_profile_index)
{
  char block[0x200];
  file_ref_t file;
  game_variant_t temporary;
  game_variant_t default_variant;
  char checksum[0x14];
  boolean success = false;

  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0x18c,
                   true);
    system_exit(-1);
  }

  if (*(void **)0x4eaaa4 != NULL) {
    error(2, "waiting for asynchronous playlist profile io to finish...");
    do {
      /* spin until the async playlist io thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaaa4));
    thread_close(*(void **)0x4eaaa4);
    *(void **)0x4eaaa4 = NULL;
  }

  if (playlist_profile_index & 0x80000000) {
    if (saved_game_files_take_mutex()) {
      if (saved_game_file_open(&file, playlist_profile_index)) {
        if (file_read(&file, 0x200, block)) {
          saved_game_file_generate_checksum(block, 0x68, checksum);
          if (csmemcmp(checksum, &block[0x68], 0x14) == 0) {
            csmemcpy(variant, block, 0x68);
          } else {
            default_variant = *game_engine_slayer_default(&temporary);
            error(2, "checksum failed on playlist profile file, sanitizing "
                     "memory resident version...");
            /* word store MOV [EBP-0x1c],SI = default_variant + 0x64 */
            *(int16_t *)default_variant.pad_64 = 0;
            ustrncpy((wchar_t *)&default_variant,
                     saved_game_file_get_display_name(playlist_profile_index), 0xb);
            /* word store MOV [EBP-0x6a],SI = name element 0xb (+0x16) */
            ((wchar_t *)&default_variant)[0xb] = 0;
            csmemcpy(variant, &default_variant, 0x68);
          }
          success = true;
        } else {
          error(2, "failed to read playlist profile from file");
        }
        saved_game_file_close(&file, playlist_profile_index);
      } else {
        error(2, "failed to open playlist profile file");
      }
      saved_game_files_release_mutex();
    } else {
      error(2, "failed to get saved game files mutex; perhaps another "
               "operation is in progress?");
    }
  } else {
    default_variant = *game_engine_slayer_default(&temporary);
    error(2, "checksum failed on playlist profile file, sanitizing memory "
             "resident version...");
    *(int16_t *)default_variant.pad_64 = 0;
    ustrncpy((wchar_t *)&default_variant,
             saved_game_file_get_display_name(playlist_profile_index), 0xb);
    ((wchar_t *)&default_variant)[0xb] = 0;
    csmemcpy(variant, &default_variant, 0x68);
    success = true;
  }

  return success;
}

/* 0x1c2550 — playlist_profile_write_thread_proc
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0x202).
 *
 * Worker thread started by playlist_profile_write via thread_new with the
 * 0x4eaa38 state block as its parameter: __stdcall (RET 0x4), one stack
 * argument asserted non-NULL ("input"), always returns 0 (XOR EAX,EAX).
 * input[0] is the saved game file index forwarded to open/close/delete and
 * the metadata sync; input + 1 (ADD ESI,4) is the 0x68-byte record, copied
 * into a 0x200-byte block with its 0x14-byte checksum stored at +0x68, and
 * also forwarded as the display name to the metadata sync.
 * Frame (from the LEAs): block [EBP-0x30c] 0x200, file [EBP-0x10c].
 */
int __stdcall playlist_profile_write_thread_proc(int *input)
{
  char block[0x200];
  file_ref_t file;
  int32_t saved_game_file_index;
  bool write_failed;

  if (input == NULL) {
    display_assert("input", "c:\\halo\\SOURCE\\saved games\\playlist_profile.c",
                   0x202, true);
    system_exit(-1);
  }

  error(2, "begin playlist profile write");
  if (saved_game_files_take_mutex()) {
    saved_game_file_index = *input;
    write_failed = false;
    if (saved_game_file_open(&file, saved_game_file_index)) {
      csmemcpy(block, input + 1, 0x68);
      saved_game_file_generate_checksum(block, 0x68, block + 0x68);
      if (!file_set_position(&file, 0) || !file_write(&file, 0x200, block)) {
        error(2, "failed to write playlist profile to file");
        write_failed = true;
      }
      if (saved_game_file_close(&file, saved_game_file_index) &&
          !synchronize_metadata_display_name_with_profile_name(
            saved_game_file_index, (wchar_t *)(input + 1))) {
        error(2, "metadata name may not match game display name");
      }
      if (write_failed) {
        delete_enumerated_saved_game_file(saved_game_file_index);
      }
      saved_game_files_release_mutex();
    } else {
      error(2, "failed to open playlist profile file");
      saved_game_files_release_mutex();
    }
  } else {
    error(2, "failed to get saved game files mutex; perhaps another "
             "operation is in progress?");
  }
  error(2, "end playlist profile write");
  return 0;
}

/* Flush the pending saved-game update (guarded by the byte flag at 0x32eb90)
 * before enumerating the available saved game files.  The two literal 1
 * arguments are pushed as immediates at 0x1c26d1/0x1c26d5. */
void FUN_001c26b0(int param_1, int *param_2, int *param_3)
{
  if (*(uint8_t *)0x32eb90 == 1) {
    FUN_001c2120();
    *(uint8_t *)0x32eb90 = 0;
  }
  saved_game_files_enumerate_available_to_local_player_index(
    param_1, 1, param_2, param_3, 1);
}

/* 0x1c26f0 — playlist_profile_get
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0xd9).
 *
 * Twin of playlist_profile_save below: two cdecl stack arguments,
 * [EBP+0x8] the playlist profile index, only ever compared against -1 and
 * forwarded, and [EBP+0xc] the variant record, asserted non-NULL before
 * anything else (MOV ESI,[EBP+0xc] / TEST ESI,ESI at 0x1c26f5).  An index of
 * -1 is the "no profile" sentinel and fetches the next playlist instead.
 *
 * The result is a byte: XOR BL,BL at 0x1c26f8 seeds it false and the index==-1
 * path returns it via MOV AL,BL at 0x1c2735, while the other path returns the
 * callee's AL untouched — there is no store/reload through BL on that path, so
 * this is two returns rather than one result variable.
 *
 * playlist_profile_read takes the record in EBX
 * (MOV EBX,ESI at 0x1c273b; the callee reads it with TEST EBX,EBX at 0x1c22e9
 * before any write and asserts on the same "variant" condition string) plus
 * the index as its one stack argument — the single PUSH EAX at 0x1c273a is
 * exactly what ADD ESP,0x4 at 0x1c2742 covers, so the register argument is not
 * an extra push.
 */
boolean playlist_profile_get(int unknown, game_variant_t *variant)
{
  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0xd9,
                   true);
    system_exit(-1);
  }

  if (unknown == -1) {
    game_engine_playlist_next(0, 0, 4);
    return false;
  }

  return playlist_profile_read(variant, unknown);
}

/* 0x1c2750 — playlist_profile_write
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0x1ea).
 *
 * Structural twin of player_profile_write above, for the playlist profile
 * block: the record arrives in ESI (TEST ESI,ESI at 0x1c2753 reads the
 * register before any write, and the assert condition string is literally
 * "variant"), plus one stack dword whose meaning is unproven — it is stored
 * into the async state dword at 0x4eaa38 and reaches the worker thread as its
 * parameter, so it stays an explicit unknown.
 *
 * Drains any in-flight playlist profile io (thread reference at 0x4eaaa4),
 * snapshots the 0x68-byte record into the state buffer at 0x4eaa3c
 * (PUSH 0x68 / PUSH ESI / PUSH 0x4eaa3c at 0x1c27be..0x1c27c1), then spawns
 * playlist_profile_write_thread_proc (0x1c2550) with the state block address as its
 * argument and the thread reference slot at 0x4eaaa4.  thread_new's result is
 * discarded: the single ADD ESP,0x1c at 0x1c27e7 covers csmemcpy's 3 args plus
 * thread_new's 4.
 *
 * The body only queues the write; the file access happens on the worker
 * thread.  The 0x74-byte block at 0x4eaa38 has no recovered struct yet, so both the
 * state dword and the record buffer stay raw offsets.
 */
void playlist_profile_write(void *variant /* @<esi> */, int unknown)
{
  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0x1ea,
                   true);
    system_exit(-1);
  }

  if (*(void **)0x4eaaa4 != NULL) {
    error(2, "waiting for asynchronous playlist profile io to finish...");
    do {
      /* spin until the async playlist io thread signals completion */
    } while (!thread_is_done(*(void **)0x4eaaa4));
    thread_close(*(void **)0x4eaaa4);
    *(void **)0x4eaaa4 = NULL;
  }

  *(int *)0x4eaa38 = unknown;
  csmemcpy((void *)0x4eaa3c, variant, 0x68);
  thread_new(0, (void *)playlist_profile_write_thread_proc, 0x4eaa38, (void **)0x4eaaa4);
}

/* 0x1c27f0 — playlist_profile_save
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\playlist_profile.c" (line 0x131).
 *
 * Two cdecl stack arguments: [EBP+0x8] is an int compared against -1
 * (CMP EDI,-0x1 at 0x1c281f) and [EBP+0xc] is the variant record, asserted
 * non-NULL before anything else (MOV ESI,[EBP+0xc] / TEST ESI,ESI at
 * 0x1c27f4).  The int's meaning is unproven — it is only tested against -1
 * and forwarded.  An index of -1 does nothing; otherwise the variant is
 * cleaned up and queued for writing.
 *
 * When the index is not -1 the variant is cleaned up and then handed to
 * playlist_profile_write, which takes the record in ESI (still live from the
 * cleanup call) plus the index on the stack.  The single ADD ESP,0x8 at
 * 0x1c2830 covers both pushes — game_engine_variant_cleanup's one argument
 * and playlist_profile_write's one stack argument — so the register argument
 * is not an extra push.
 */
void playlist_profile_save(int unknown, game_variant_t *variant)
{
  if (variant == NULL) {
    display_assert("variant",
                   "c:\\halo\\SOURCE\\saved games\\playlist_profile.c", 0x131,
                   true);
    system_exit(-1);
  }

  if (unknown != -1) {
    game_engine_variant_cleanup(variant);
    playlist_profile_write(variant, unknown);
  }
}

/* Dispose saved game file handles and clean up. */
void saved_game_files_dispose(void)
{
  if (*(int *)0x4eacbc != 0) {
    ((void (*)(int))0x81910)(*(int *)0x4eacbc);
    *(int *)0x4eacbc = 0;
  }
  if (*(int *)0x4eacc0 != 0) {
    ((void (*)(int))0x81910)(*(int *)0x4eacc0);
    *(int *)0x4eacc0 = 0;
  }
  ((void (*)(void))0x1c0cf0)();
  ((void (*)(void))0x1c1dc0)();
  *(uint8_t *)0x4eacc6 = 0;
}

/* 0x1c2890 — saved_game_file_close
 * Source TU confirmed by the __FILE__ assert string
 * "c:\halo\SOURCE\saved games\saved_game_files.c" (lines 0x25b-0x261).
 *
 * Two cdecl stack arguments: [EBP+0x8] is the saved game file record (asserted
 * against the condition string "saved_game_file" and pushed unchanged into
 * file_close at 0x1c2974/0x1c2975, ADD ESP,0x4 at 0x1c297a) and [EBP+0xc] is a
 * packed saved-game-file index.  The three fields read out of it are exactly
 * the ones build_saved_game_file_index (0x1c36f0) packs in, and the assert
 * condition strings name them: bits 8-15 (MOVZX EDI,AH at 0x1c289b) are
 * `memory_unit`, bits 16-27 (SAR EAX,0x10 / AND EAX,0xfff at 0x1c289e) are
 * `n`, and bits 0-3 (AND ESI,0xf at 0x1c28a6) are `type`.
 *
 * The range checks are signed on both ends (TEST/JL then CMP/JL at 0x1c28f6,
 * 0x1c291f and 0x1c2948), giving the literal `(x >= 0) && (x < LIMIT)` form of
 * each assert; the limits are 2 saved game file types, 9 memory units and 100
 * enumerated files.
 *
 * The result is the conjunction at 0x1c297d-0x1c298b: file_close's AL must be
 * non-zero AND `memory_unit` must still be the hard drive (0), otherwise AL is
 * cleared at 0x1c298e.  The second half is only reachable in a build whose
 * assert at line 0x25b does not halt.
 */
bool saved_game_file_close(file_ref_t *saved_game_file,
                           int32_t saved_game_file_index)
{
  int32_t memory_unit;
  int32_t n;
  int32_t type;

  memory_unit = (saved_game_file_index >> 8) & 0xff;
  n = (saved_game_file_index >> 16) & 0xfff;
  type = saved_game_file_index & 0xf;

  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x25b,
                   true);
    system_exit(-1);
  }

  if (saved_game_file == NULL) {
    display_assert("saved_game_file",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x25e,
                   true);
    system_exit(-1);
  }

  if (type < 0 || type >= 2) {
    display_assert("(type >= 0) && (type < NUMBER_OF_SAVED_GAME_FILE_TYPES)",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x25f,
                   true);
    system_exit(-1);
  }

  if (memory_unit < 0 || memory_unit >= 9) {
    display_assert(
      "(memory_unit >= 0) && (memory_unit < NUMBER_OF_MEMORY_UNITS)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x260, true);
    system_exit(-1);
  }

  if (n < 0 || n >= 100) {
    display_assert(
      "(n >= 0) && (n < "
      "MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x261, true);
    system_exit(-1);
  }

  return file_close(saved_game_file) && memory_unit == 0;
}

/* Extract the type bits from a saved game file index. */
unsigned short saved_game_file_get_type(int saved_game_file_index)
{
  return (unsigned short)(saved_game_file_index & 0xf);
}

/* Notify the saved-game file system that the set of attached memory units
 * changed.  Sets the byte flag at 0x4eacc7 (also set to 1 by
 * saved_game_files_initialize); its consumers are not established here. */
void saved_game_files_notify_memory_units_changed(void)
{
  *(uint8_t *)0x4eacc7 = 1;
}

/* 0x1c29c0 — build the first "untitled" display name that is not already taken
 * on disk.  The caller's buffer arrives in [EBP+8] and is asserted non-NULL
 * with the reason string "display_name" at line 0x2c1 (PUSH 0x2c1 at
 * 0x1c29d3), so the parameter is named after that assert.
 *
 * The name template comes from string index 2 of the 'ustr' tag
 * "ui\saved_game_file_strings" (PUSH 0x75737472 at 0x1c29f6); when the tag is
 * not loaded the routine only reports the error and leaves the buffer empty
 * (MOV word ptr [ESI],0x0 at 0x1c29fb happens before the lookup).
 *
 * Two zero-initialized locals are set up for the existence probe: an 8-byte
 * ASCII buffer at EBP-8 holding wide_to_ascii of the wide string whose pointer
 * lives in the dword at 0x32eb94 (MOV EDX,[0x32eb94] at 0x1c2a14 loads the
 * VALUE, so the global is a wchar_t *), and a 0x100-byte buffer at EBP-0x108
 * (REP STOSD of 0x3f dwords plus the STOSW/STOSB tail at 0x1c2a39-0x1c2a48).
 *
 * 0x1d2f22 is unnamed in kb.json.  Its ABI is read straight off this call
 * site: six stack arguments are pushed at 0x1c2a71-0x1c2a85 and NO stack
 * cleanup follows the CALL at 0x1c2a8f (the loop back-edge at 0x1c2aa0 re-
 * enters with the same ESP), so it is __stdcall, and its EAX is tested at
 * 0x1c2a94, so it returns an int.  A non-zero result ends the search; zero
 * means the candidate name is in use and the counter advances.  The probe
 * argument meanings beyond the buffers are unproven, hence param_3/param_4.
 *
 * The counter is EBX, seeded by XOR EBX,EBX at 0x1c2a51; the value formatted
 * into the name is EBX+1 (LEA EDI,[EBX+1] at 0x1c2a56, pushed as
 * unicode_sprintf's variadic argument at 0x1c2a59) and EBX only takes that
 * value after a zero probe result (MOV EBX,EDI at 0x1c2a98).  Exhausting the
 * 999 candidates (CMP EBX,0x3e7 at 0x1c2aa3) reports the error and clears the
 * buffer again. */
void saved_game_file_get_useable_untitled_profile_name(wchar_t *display_name)
{
  int string_list_tag;

  if (display_name == NULL) {
    display_assert("display_name",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x2c1,
                   1);
    system_exit(-1);
  }

  display_name[0] = L'\0';
  string_list_tag =
    tag_loaded(0x75737472 /* 'ustr' */, "ui\\saved_game_file_strings");
  if (string_list_tag != -1) {
    const wchar_t *root_name = *(const wchar_t **)0x32eb94;
    char root_path[8] = "";
    char save_game_directory[0x100] = "";
    int index;
    int next_index;

    wide_to_ascii(root_name, root_path, 8);

    index = 0;
    do {
      next_index = index + 1;
      unicode_sprintf(display_name, 0x7f,
                      (const wchar_t *)FUN_0019d420(string_list_tag, 2),
                      next_index);
      display_name[0x7f] = L'\0';
      if (FUN_001d2f22(root_path, display_name, 3, 0, save_game_directory,
                       0x100) != 0)
        break;
      index = next_index;
    } while (index < 999);

    if (index == 999) {
      error(2, "%d untitled saved games! clean up your hard drive!!", 999);
      display_name[0] = L'\0';
    }
  } else {
    error(2, "unicode string lis tag '%s' not loaded",
          "ui\\saved_game_file_strings");
  }
}

/* Acquire the saved-game file system mutex.  The mutex handle lives in the
 * dword at 0x4eacbc (initialized by saved_game_files_initialize via 0x817e0);
 * the disassembly loads its VALUE into EAX and passes that as take_mutex's
 * `mutex_reference`.  Timeout is the immediate 0x36ee80 = 3600000 ms.
 * The boolean result is discarded (no test after the call at 0x1c2afb).
 * noinline (VC71 verification only, cl.exe only): the original's playlist
 * profile read lives in playlist_profile.c and CALLs this out of line; with
 * the body in scope cl.exe would inline it there. */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noinline)
#endif
bool saved_game_files_take_mutex(void)
{
  return take_mutex(*(int **)0x4eacbc, 3600000);
}

/* Release the saved-game file system mutex.  Same handle dword at 0x4eacbc as
 * saved_game_files_take_mutex: the disassembly loads its VALUE into EAX
 * (MOV EAX,[0x4eacbc]) and passes that as release_mutex's `mutex_reference`
 * (PUSH EAX; CALL 0x818d0; POP ECX).  No return value.
 * noinline (VC71 verification only, cl.exe only): the original's playlist
 * profile read lives in playlist_profile.c and CALLs this out of line; with
 * the body in scope cl.exe would inline it there. */
#if defined(_MSC_VER) && !defined(__clang__)
__declspec(noinline)
#endif
void saved_game_files_release_mutex(void)
{
  release_mutex(*(int **)0x4eacbc);
}

/* 0x1c2b20 — two file-system health checks over the saved-game root drive.
 *
 * The drive is named by the wide string whose pointer lives in the dword at
 * 0x32eb94 (MOV ECX,[0x32eb94] at 0x1c2b32 loads the VALUE, so the global is
 * a wchar_t *, same as in saved_game_file_get_useable_untitled_profile_name);
 * it is converted into the 8-byte ASCII buffer at EBP-0x10 before each check.
 *
 * Check 1 (0x1c2b45-0x1c2b72): wide_to_ascii's return is pushed straight into
 * XGetDiskFreeSpaceEx as the directory name, the three out-pointers being the
 * 8-byte slots at EBP-0x8 (free bytes available), EBP-0x20 (total bytes) and
 * EBP-0x18 (total free bytes) pushed at 0x1c2b2d-0x1c2b3b.  The free-space
 * test is a 64-bit unsigned compare against 0x2800000 (TEST high / JA / JC /
 * CMP low,0x2800000 / JNC at 0x1c2b57-0x1c2b67), i.e. "less than 40 MB free"
 * returns 1.  A failed query (EAX == 0 at 0x1c2b53) skips the size test.
 *
 * Check 2 (0x1c2b73-0x1c2bed): walk the saved games on that drive with
 * XFindFirstSaveGame / XFindNextSaveGame into the find-data block at
 * EBP-0x364, counting from 1 (MOV ESI,1 at 0x1c2b9c) and stopping at 100
 * (CMP ESI,0x64).  The counter is incremented before each XFindNextSaveGame
 * (INC ESI at 0x1c2bb0), and the loop continues only while that call returns
 * exactly 1 (CMP AL,1 at 0x1c2bb6).  Hitting the limit returns 2; anything
 * else — including an invalid find handle (CMP EDI,-1 at 0x1c2b99) — returns
 * the zeroed EBX (XOR EBX,EBX at 0x1c2b43, MOV AX,BX at 0x1c2be6).
 *
 * The meaning of the three result codes is not proven by this function, so
 * they are left as the literal values the binary returns. */
int16_t saved_game_perform_file_system_checks(void)
{
  char root_path[8];
  uint64_t free_bytes_available;
  uint64_t total_bytes;
  uint64_t total_free_bytes;
  char find_data[XGAME_FIND_DATA_SIZE];
  int find_handle;
  uint32_t count;
  int16_t result;

  result = 0;
  if (GetDiskFreeSpaceExA(
        wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
        &free_bytes_available, &total_bytes, &total_free_bytes) != 0 &&
      free_bytes_available < 0x2800000)
    return 1;

  find_handle = XFindFirstSaveGame(
    wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8), find_data);
  count = 1;
  if (find_handle != -1) {
    do {
      if (count >= 100)
        break;
      count++;
    } while (XFindNextSaveGame(find_handle, find_data) == true);

    if (XFindClose(find_handle) == 0)
      error(2, "XFindClose() failed");

    if (count >= 100)
      result = 2;
  }

  return result;
}

/* 0x1c2bf0 — probe whether a saved-game display name is still unused.
 *
 * A NULL name, or one whose first wide character is already NUL (CMP word ptr
 * [EAX],0x0 at 0x1c2c03), returns the zeroed BL (XOR BL,BL at 0x1c2bfd) with
 * no probe at all.
 *
 * Otherwise the same six-argument __stdcall probe used by
 * saved_game_file_get_useable_untitled_profile_name is issued: the arguments
 * are pushed at 0x1c2c09-0x1c2c2e and no stack cleanup follows the CALL at
 * 0x1c2c2f (MOV ESP,EBP restores the frame), so 0x1d2f22 cleans its own six
 * dwords.  The drive string is wide_to_ascii of the wide string whose pointer
 * lives in the dword at 0x32eb94 (MOV EAX,[0x32eb94] at 0x1c2c1a loads the
 * VALUE) converted into the 8-byte buffer at EBP-0x8; the ADD ESP,0xc at
 * 0x1c2c2b cleans only wide_to_ascii's three cdecl arguments and its EAX
 * return is pushed straight through as the probe's first argument.  Neither
 * the 8-byte buffer nor the 0x100-byte buffer at EBP-0x108 is pre-cleared
 * here (no REP STOSD, unlike 0x1c29xx), and the 0x100-byte buffer is
 * write-only to this function.
 *
 * TEST EAX,EAX / MOV AL,1 / JNZ at 0x1c2c34-0x1c2c38 returns true exactly
 * when the probe result is non-zero, matching the zero-means-name-in-use
 * sense already read off the 0x1c2a8f call site. */
char saved_game_file_name_unique(const wchar_t *name)
{
  char save_game_directory[0x100];
  char root_path[8];
  char result;

  result = 0;
  if (name != NULL && name[0] != L'\0') {
    if (FUN_001d2f22(wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
                     name, 3, 0, save_game_directory, 0x100) != 0)
      result = 1;
  }

  return result;
}

/* 0x1c2c50 — write the last used player-profile directory for player 1 to
 * "z:\lastprof.txt".  Structural twin of
 * saved_game_file_remember_last_used_multiplayer_map (0x1c2fb0): the file
 * reference is created from the path (0x1c2c8f), then file_create (0x1c2ca2)
 * and file_open with flags 2 (PUSH 0x2 at 0x1c2cb4), then a fixed 0x100-byte
 * file_write of the caller's buffer — the length is the constant 0x100 pushed
 * at 0x1c2cca, not a strlen.  The buffer is [EBP+0x8], held in ESI from
 * 0x1c2c5a and pushed as the last file_write argument at 0x1c2cc3.  Assert
 * line number 0x41a is the immediate pushed at 0x1c2c63.  Both error() calls
 * pass severity 2 and the same path string; the create/open failure path at
 * 0x1c2d04 reports "failed to open" and skips file_close entirely. */
void saved_game_file_remember_player1_last_used_profile_directory(
  const char *directory_path)
{
  file_ref_t info;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x41a,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastprof.txt", 0) != NULL &&
      file_create(&info) && file_open(&info, 2)) {
    if (!file_write(&info, 0x100, directory_path))
      error(2, "failed to write to '%s'", "z:\\lastprof.txt");
    file_close(&info);
    return;
  }

  error(2, "failed to open '%s'", "z:\\lastprof.txt");
}

/* Read the last used player-profile directory for player 1 out of
 * "z:\lastprof.txt".  The caller supplies a 0x100-byte buffer; the read is a
 * single 0x100-byte file_read and the final byte is always forced to NUL
 * (MOV byte ptr [ESI+0xff],0 on both exits).  The return value is the
 * file_read result (BL, zero-initialized at 0x1c2d2e), so every failure path
 * returns false. */
bool saved_game_file_retrieve_player1_last_used_profile_directory(
  char *directory_path)
{
  file_ref_t info;
  bool result = false;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x434,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastprof.txt", 0) != NULL &&
      file_open(&info, 1)) {
    result = file_read(&info, 0x100, directory_path);
    if (!result)
      error(2, "failed to read from '%s'", "z:\\lastprof.txt");
    file_close(&info);
    directory_path[0xff] = 0;
    return result;
  }

  error(2, "failed to open '%s'", "z:\\lastprof.txt");
  directory_path[0xff] = 0;
  return result;
}

/* 0x1c2e00 — write the last used multiplayer game-variant directory to
 * "z:\lastmpvr.txt".  Structural twin of
 * saved_game_file_remember_player1_last_used_profile_directory (0x1c2c50):
 * file_reference_create_from_path (0x1c2e3f), file_create (0x1c2e52),
 * file_open with flags 2 (PUSH 0x2 at 0x1c2e64), then a fixed 0x100-byte
 * file_write of the caller's buffer — the length is the constant 0x100 pushed
 * at 0x1c2e7a, not a strlen.  The buffer is [EBP+0x8], held in ESI from
 * 0x1c2e0a and pushed as the last file_write argument at 0x1c2e73.  Assert
 * line number 0x44e is the immediate pushed at 0x1c2e13.  Both error() calls
 * pass severity 2 and the same path string; the create/open failure path at
 * 0x1c2eb4 reports "failed to open" and skips file_close entirely. */
void saved_game_file_remember_last_used_multiplayer_variant_directory(
  const char *directory_path)
{
  file_ref_t info;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x44e,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpvr.txt", 0) != NULL &&
      file_create(&info) && file_open(&info, 2)) {
    if (!file_write(&info, 0x100, directory_path))
      error(2, "failed to write to '%s'", "z:\\lastmpvr.txt");
    file_close(&info);
    return;
  }

  error(2, "failed to open '%s'", "z:\\lastmpvr.txt");
}

/* Read the last used multiplayer game-variant directory out of
 * "z:\lastmpvr.txt".  Structurally identical to the player-profile retrieve
 * above: 0x100-byte file_read into the caller's buffer, final byte forced to
 * NUL on both exits (MOV byte ptr [ESI+0xff],0 at 0x1c2f71 and 0x1c2f94), and
 * the return value is BL (zero-initialized by XOR BL,BL at 0x1c2ede, set from
 * the file_read result at 0x1c2f45), so every failure path returns false.
 * Assert line number 0x468 is the immediate pushed at 0x1c2ee6. */
bool saved_game_file_retrieve_last_used_multiplayer_variant_directory(
  char *directory_path)
{
  file_ref_t info;
  bool result = false;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x468,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpvr.txt", 0) != NULL &&
      file_open(&info, 1)) {
    result = file_read(&info, 0x100, directory_path);
    if (!result)
      error(2, "failed to read from '%s'", "z:\\lastmpvr.txt");
    file_close(&info);
    directory_path[0xff] = 0;
    return result;
  }

  error(2, "failed to open '%s'", "z:\\lastmpvr.txt");
  directory_path[0xff] = 0;
  return result;
}

/* Write the last used multiplayer map name to "z:\lastmpmp.txt".  The file is
 * created (file_create at 0x1c3002) then opened with flags 2 (write) and a
 * fixed 0x100-byte file_write of the caller's buffer at 0x1c3030 — the source
 * length is the constant 0x100, not a strlen.  Assert line number 0x4b7 is the
 * immediate pushed at 0x1c2fc3.  Both error() calls pass severity 2 and the
 * same path string; the create/open/prepare failure path at 0x1c3064 reports
 * "failed to open" and skips file_close entirely. */
void saved_game_file_remember_last_used_multiplayer_map(const char *map_name)
{
  file_ref_t info;

  if (map_name == NULL) {
    display_assert("map_name",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x4b7,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpmp.txt", 0) != NULL &&
      file_create(&info) && file_open(&info, 2)) {
    if (!file_write(&info, 0x100, map_name))
      error(2, "failed to write to '%s'", "z:\\lastmpmp.txt");
    file_close(&info);
    return;
  }

  error(2, "failed to open '%s'", "z:\\lastmpmp.txt");
}

/* Read the last used multiplayer map name out of "z:\lastmpmp.txt".  Mirrors
 * the variant-directory retrieve above: 0x100-byte file_read into the caller's
 * buffer, final byte forced to NUL on both exits (MOV byte ptr [ESI+0xff],0 at
 * 0x1c3121 and 0x1c3144), and the return value is BL (zero-initialized by XOR
 * BL,BL at 0x1c308e, set from the file_read result by MOV BL,AL at 0x1c30f5),
 * so every failure path returns false.  Assert line number 0x4d1 is the
 * immediate pushed at 0x1c3096.  Unlike the remember path there is no
 * file_create; open flags are 1 (read) at 0x1c30d4. */
bool saved_game_file_retrieve_last_used_multiplayer_map(char *map_name)
{
  file_ref_t info;
  bool result = false;

  if (map_name == NULL) {
    display_assert("map_name",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x4d1,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(&info, "z:\\lastmpmp.txt", 0) != NULL &&
      file_open(&info, 1)) {
    result = file_read(&info, 0x100, map_name);
    if (!result)
      error(2, "failed to read from '%s'", "z:\\lastmpmp.txt");
    file_close(&info);
    map_name[0xff] = 0;
    return result;
  }

  error(2, "failed to open '%s'", "z:\\lastmpmp.txt");
  map_name[0xff] = 0;
  return result;
}

/* 0x1c3160 — sign a saved-game buffer with the Xbox signature API.
 *
 * Three cdecl stack params: the buffer at EBP+8 (the assert condition string
 * is literally "buffer"), a word-sized length read with MOVZX at 0x1c319a
 * from EBP+0xc, and an output block at EBP+0x10 handed to the End call.  The
 * signature block's size and layout are not established by this function.
 *
 * All three XAPI entry points are __stdcall: no stack cleanup follows the
 * CALLs at 0x1c318e / 0x1c31a1 / 0x1c31be.  Begin takes a single flags word
 * (PUSH 0) and returns the handle in EAX, which is rejected only on the
 * INVALID_HANDLE_VALUE compare CMP ESI,-1 at 0x1c3195; Update and End return
 * a status tested with TEST EAX,EAX.  The names come from the error strings
 * reported at each site.  Note the Update failure is only reported — the End
 * call is still made on the same handle. */
void saved_game_file_generate_checksum(const void *buffer, unsigned short size,
                                       void *signature)
{
  int signature_handle;

  if (buffer == NULL) {
    display_assert(
      "buffer", "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x4eb, 1);
    system_exit(-1);
  }

  signature_handle = XCalculateSignatureBegin(0);
  if (signature_handle != -1) {
    if (XCalculateSignatureUpdate(signature_handle, buffer, size) != 0)
      error(2, "XCalculateSignatureUpdate() failed");
    if (XCalculateSignatureEnd(signature_handle, signature) != 0)
      error(2, "XCalculateSignatureEnd() failed");
    return;
  }

  /* The begin-failure report is the out-of-line tail block at 0x1c31da, which
   * the JZ at 0x1c3198 jumps forward to; written last here to keep that
   * block order. */
  error(2, "XCalculateSignatureBegin() failed");
}

/* 0x1c31f0 — make sure `path` (in EAX, pushed straight through to
 * file_reference_create_from_path at 0x1c31fc with a4 = 1) exists, creating
 * it with file_create when file_exists reports it missing.  Returns 1 (BL,
 * preloaded by MOV BL,0x1 at 0x1c3204) when the reference was built and the
 * entry exists or was created, 0 (XOR AL,AL at 0x1c3238) otherwise.  Sole
 * caller is saved_game_files_initialize, via the ensure_dir helper above. */
char FUN_001c31f0(const char *path)
{
  file_ref_t info;

  if (file_reference_create_from_path(&info, path, true) != NULL &&
      (file_exists(&info) || file_create(&info)))
    return 1;
  return 0;
}

/* Begin enumerating the saved game files on a memory unit.  `memory_unit`
 * arrives in AX (MOV SI,AX at 0x1c3251) and is asserted to be the hard drive
 * (0) before anything else happens.  It then indexes the mapfile-path table at
 * 0x32eb98 (MOVZX ESI,SI / MOV EAX,[ESI*4+0x32eb98] at 0x1c32a2), whose entry
 * 0 is "z:\saved\hdmu.map".  The zero-extended unit is also the %d argument of
 * the failure message (PUSH ESI at 0x1c32f6), which the decompiler renders as
 * a literal 0.
 *
 * Globals: 0x4eabb0 is the saved_game_files_globals file reference cleared by
 * saved_game_files_initialize; 0x4eacc4 is a word set to 0 on success and to
 * 0xffff on failure; 0x4eacc8 is the enumeration_in_progress flag named by the
 * assert at line 0x66f.
 *
 * Return value (confirmed 2026-09-25 against the raw disassembly while
 * verifying FUN_001c5010, the sole caller that inspects it): the success
 * path explicitly sets AL=1 (MOV AL,0x1 at 0x1c32e4) and stores that same AL
 * into the enumeration_in_progress flag, leaving AL=1 live at RET. The
 * failure tail block reloads AL from *(uint8_t *)0x4eacc8 (MOV
 * AL,[0x004eacc8] at 0x1c3303) immediately before RET; since the entry
 * assert guarantees that flag is 0 at this point and this path never sets
 * it, AL=0 on failure. This was previously declared void, which cannot
 * represent the boolean FUN_001c5010 actually branches on (TEST AL,AL / JZ
 * at 0x1c5052-0x1c5054). */
bool enumerate_saved_game_files_start(int16_t memory_unit)
{
  uint16_t unit;

  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x66b,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x66f,
                   1);
    system_exit(-1);
  }

  unit = (uint16_t)memory_unit;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0) {
    if (file_create((file_ref_t *)0x4eabb0)) {
      if (file_open((file_ref_t *)0x4eabb0, 2)) {
        *(uint16_t *)0x4eacc4 = 0;
        *(uint8_t *)0x4eacc8 = 1;
        return 1;
      }
    }
  }

  error(2, "failed to create/open memory unit mapfile for memory unit #%d",
        unit);
  *(uint16_t *)0x4eacc4 = 0xffff;
  return *(uint8_t *)0x4eacc8;
}

/* End the memory-unit enumeration opened by enumerate_saved_game_files_start.
 * `memory_unit` arrives in SI: it is read before any write (TEST SI,SI at
 * 0x1c3320) and zero-extended as the %d argument of the close-failure message
 * (MOVZX EAX,SI / PUSH EAX at 0x1c337f).  The second assert is the inverse of
 * the start-side one — here the enumeration flag at 0x4eacc8 must be set.
 * The tail sets the same word/flag pair start uses (0x4eacc4 = 0xffff,
 * 0x4eacc8 = 0) and returns true unconditionally (MOV AL,1 at 0x1c33a2). */
bool enumerate_saved_game_files_end(int16_t memory_unit)
{
  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x685,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 == 0) {
    display_assert("saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x687,
                   1);
    system_exit(-1);
  }

  if (!file_close((file_ref_t *)0x4eabb0)) {
    error(2, "failed to close memory unit mapfile for memory unit #%d",
          (uint16_t)memory_unit);
  }

  *(uint16_t *)0x4eacc4 = 0xffff;
  *(uint8_t *)0x4eacc8 = 0;
  return 1;
}

/* Write the next profile record into the memory-unit mapfile opened by
 * enumerate_saved_game_files_start.  `record` arrives in ESI and is used
 * unchecked as both the destination of the index write and the file_write
 * source buffer.  A single combined assert (line 0x741) guards both
 * preconditions: the enumeration_in_progress byte at 0x4eacc8 must be set
 * (TEST AL,AL / JZ at 0x1c33b7 short-circuits before the second check), and
 * the running count word at 0x4eacc4 must not be negative (TEST AX,AX / JGE
 * at 0x1c33bf). That same AX value is reused (not reread) for the 100-record
 * cap compare (CMP AX,0x64 at 0x1c33ea), the word store into record+0x202
 * (MOV word ptr [ESI+0x202],AX at 0x1c33f6), and is then incremented in
 * place (INC word ptr [0x4eacc4] at 0x1c33fd) -- the record gets the
 * pre-increment (0-based) index. The success path returns file_write's
 * result unmodified (ADD ESP,0xc / RET with no MOV into AL after the CALL at
 * 0x1c3409); the cap-exceeded path returns false explicitly (XOR AL,AL at
 * 0x1c3421). */
bool enumerate_saved_game_file(void *record)
{
  int16_t index;

  index = *(int16_t *)0x4eacc4;

  if ((*(uint8_t *)0x4eacc8 == 0) || (index < 0)) {
    display_assert(
      "(saved_game_files_globals.enumeration_in_progress) && "
      "(saved_game_files_globals.next_enumerated_profile_index >= 0)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x741, 1);
    system_exit(-1);
  }

  if (index < 100) {
    *(int16_t *)((char *)record + 0x202) = index;
    (*(int16_t *)0x4eacc4)++;

    return file_write((file_ref_t *)0x4eabb0, 0x206, record);
  }

  error(2, "the maximum number of game files have already been enumerated "
           "(time to clean up your hard drive and/or memory cards)");
  return 0;
}

/* Open the memory-unit mapfile for reading and sanity-check its length.
 * `memory_unit_index` arrives in AX (MOV SI,AX at 0x1c3431) and is asserted to
 * be the hard drive (0); the enumeration flag at 0x4eacc8 must be clear.  The
 * zero-extended unit indexes the same mapfile-path table at 0x32eb98 that
 * enumerate_saved_game_files_start uses, and is the %d argument of both error
 * messages (PUSH ESI at 0x1c34cd/0x1c34e1) — the decompiler renders it as a
 * literal 0.  Unlike the start-side function this only opens (file_open flag
 * 1), it never creates, and it leaves the 0x4eacc4/0x4eacc8 pair alone.  The
 * length check is an unsigned DIV by 0x206 (0x1c34bf) testing the remainder;
 * 0x206 is the mapfile record size.  Returns true whenever the open succeeded
 * (MOV AL,1 at 0x1c34dd), corrupt-length report included. */
__declspec(noinline) bool enumerate_mapfile_start(int16_t memory_unit_index)
{
  uint16_t unit;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x75b,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x75f,
                   1);
    system_exit(-1);
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0) {
    if (file_open((file_ref_t *)0x4eabb0, 1)) {
      if ((uint32_t)file_get_eof((file_ref_t *)0x4eabb0) % 0x206 != 0) {
        error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
              unit);
      }
      return 1;
    }
  }

  error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  return 0;
}

/* Close the memory-unit mapfile opened by enumerate_mapfile_start.
 * `memory_unit_index` arrives in SI: it is read before any write (TEST SI,SI at
 * 0x1c3500) and zero-extended as the %d argument of the failure message
 * (MOVZX EAX,SI / PUSH EAX at 0x1c3588).  Three asserts guard the entry: the
 * unit must be the hard drive (0), the enumeration flag at 0x4eacc8 must be
 * clear, and the unit must be below NUMBER_OF_MEMORY_UNITS (CMP SI,0x9 / JC at
 * 0x1c354e, an unsigned compare).  Unlike enumerate_saved_game_files_end this
 * touches neither 0x4eacc4 nor 0x4eacc8, and it returns the file_close result
 * itself (MOV BL,AL at 0x1c357f / MOV AL,BL at 0x1c359b) rather than a
 * constant. */
__declspec(noinline) bool enumerate_mapfile_end(int16_t memory_unit_index)
{
  bool closed;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x77c,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x77e,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9) {
    display_assert("memory_unit_index < NUMBER_OF_MEMORY_UNITS",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x77f,
                   1);
    system_exit(-1);
  }

  closed = file_close((file_ref_t *)0x4eabb0);
  if (!closed) {
    error(2, "enumerate_mapfile_end FAILED on memory unit #%d",
          (uint16_t)memory_unit_index);
  }

  return closed;
}

/* Read the next 0x206-byte record out of the memory-unit mapfile opened by
 * enumerate_mapfile_start.  `file` arrives in ESI: it is read before any write
 * (TEST ESI,ESI at 0x1c35c9) and pushed as the destination buffer of file_read
 * (PUSH ESI at 0x1c35ed).  Two asserts guard the entry — the enumeration flag
 * at 0x4eacc8 must be clear and `file` must be non-NULL.  The file_read result
 * is RETURNED, not discarded: the CALL at 0x1c35f8 is followed by a plain ADD
 * ESP,0xc / RET with no MOV into AL, and the caller at 0x1c3969
 * (saved_game_file_find_profile_index_for_directory_path) does TEST AL,AL / JZ
 * on it, so EAX is load-bearing.  The layout of the 0x206-byte record is not
 * established here. */
__declspec(noinline) bool enumerate_saved_game_file_from_mapfile(void *file)
{
  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x793,
                   1);
    system_exit(-1);
  }

  if (file == NULL) {
    display_assert("file", "c:\\halo\\SOURCE\\saved games\\saved_game_files.c",
                   0x794, 1);
    system_exit(-1);
  }

  return file_read((file_ref_t *)0x4eabb0, 0x206, file);
}

/* Return how many 0x206-byte records the memory-unit mapfile holds.
 * `memory_unit_index` arrives in SI: it is read before any write (TEST SI,SI at
 * 0x1c3614), compared unsigned against NUMBER_OF_MEMORY_UNITS (CMP SI,0x9 / JC
 * at 0x1c3662) and zero-extended to index the mapfile-path table at 0x32eb98
 * (MOVZX EAX,SI / MOV ECX,[EAX*4+0x32eb98] at 0x1c3688).  The same three entry
 * asserts as enumerate_mapfile_end guard it.  Unlike enumerate_mapfile_start
 * this never opens the file — it only builds the file reference and asks for
 * the size (file_get_size at 0x1c36af writes the dword at EBP-4), then divides
 * it by the 0x206 record size; the MUL 0xfd08e551 / SHR EDX,9 pair at
 * 0x1c36bb-0x1c36c5 is the unsigned magic form of `size / 0x206` (verified
 * exact over the whole 32-bit range).  Both failure paths return 0 (XOR EAX,EAX
 * at 0x1c36cc). */
__declspec(noinline) uint32_t
count_enumerated_profiles_in_mapfile(int16_t memory_unit_index)
{
  uint32_t size;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x860,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x862,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9) {
    display_assert("memory_unit_index < NUMBER_OF_MEMORY_UNITS",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x863,
                   1);
    system_exit(-1);
  }

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0,
        ((const char **)0x32eb98)[(uint16_t)memory_unit_index], 0) != 0) {
    if (file_get_size((file_ref_t *)0x4eabb0, &size)) {
      return size / 0x206;
    }
  }

  return 0;
}

/* Pack a saved game file index out of three register-passed fields plus two
 * stack-passed flags.  The field widths come straight from the masks at
 * 0x1c3713/0x1c371b/0x1c3723: EAX keeps 12 bits, ECX 8 bits, EDX 4 bits, and
 * the two SHL EAX,8 steps at 0x1c3718/0x1c3726 place them at bits 16-27, 8-15
 * and 0-3.  The low nibble is the file type — saved_game_file_get_type
 * (0x1c29a0) reads exactly `index & 0xf` back out.  Both flags are compared
 * against the constant 1 held in CL (MOV CL,1 at 0x1c372e), not merely tested
 * for non-zero, and set bits 30 and 31.  The meaning of the 12-bit and 8-bit
 * fields and of the two flags is not established by this function. */
__declspec(noinline) uint32_t build_saved_game_file_index(int32_t param_eax,
                                                          int32_t param_ecx,
                                                          int32_t file_type,
                                                          bool param_1,
                                                          bool param_2)
{
  uint32_t index;

  index = (uint32_t)((((param_eax & 0xfff) << 8) | (param_ecx & 0xff)) << 8) |
          (uint32_t)(file_type & 0xf);
  if (param_1 == 1)
    index |= 0x40000000;
  if (param_2 == 1)
    index |= 0x80000000;
  return index;
}

/* Initialize saved game files: create directory structure on the
 * Xbox hard drive, allocate file handles, and load profile data. */
void saved_game_files_initialize(void)
{
  if (!ensure_dir((const char *)0x2bae58))
    error(2, "failed to find/create '%s' directory", "z:\\saved");
  if (!ensure_dir((const char *)0x2bae14))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\player_profiles");
  if (!ensure_dir((const char *)0x2bade8))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\player_profiles\\default_profile");
  if (!ensure_dir((const char *)0x2badd4))
    error(2, "failed to find/create '%s' directory", "z:\\saved\\playlists");
  if (!ensure_dir((const char *)0x2badb0))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\playlists\\default_playlist");
  if (!ensure_dir((const char *)0x2bad9c))
    error(2, "failed to find/create '%s' directory", "z:\\saved\\recordings");
  if (!ensure_dir((const char *)0x2bad78))
    error(2, "failed to find/create '%s' directory",
          "z:\\saved\\recordings\\last_recording");

  csmemset((void *)0x4eabb0, 0, 0x11c);
  *(uint8_t *)0x4eacc7 = 1;
  *(int *)0x4eacbc = 0;
  *(int *)0x4eacc0 = 0;

  if (((char (*)(void *))0x817e0)((void *)0x4eacbc)) {
    if (((char (*)(void *))0x817e0)((void *)0x4eacc0)) {
      *(uint8_t *)0x4eacc6 = 1;
      ((void (*)(void))0x1c1ba0)();
      ((void (*)(void))0x1c1da0)();
      return;
    }
  }

  *(uint8_t *)0x4eacc6 = 0;
  display_assert("failed to initialize saved game files",
                 "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0xbf, 1);
  system_exit(-1);
  ((void (*)(void))0x1c1ba0)();
  ((void (*)(void))0x1c1da0)();
}

/* 0x1c38d0 — scan the memory-unit mapfile for the entry whose directory path
 * matches `directory_path` and return that entry's packed saved-game file
 * index, or -1 when nothing matches.
 *
 * `directory_path` is the stack dword at EBP+8 (asserted non-NULL, condition
 * string "directory_path", line 0x486) and `file_type` is the stack dword at
 * EBP+0xc, read as a WORD (MOV SI,word ptr [EBP+0xc] at 0x1c3972) and
 * sign-extended into EDX for build_saved_game_file_index (MOVSX EDX,SI at
 * 0x1c39a6).
 *
 * The result lives in EDI/[EBP-4], preset to -1 by OR EDI,0xffffffff at
 * 0x1c38de and reloaded from [EBP-4] at 0x1c39bf; all three RETs move EDI into
 * EAX, so every failure path returns -1.  Both mutex handles are the VALUES of
 * the dwords at 0x4eacbc (save game files) and 0x4eacc0 (mapfile), the same
 * form saved_game_files_take_mutex uses, with the 0x36ee80 = 3600000 ms
 * timeout.  The mapfile mutex failure path releases only the first mutex.
 *
 * The 0x206-byte record buffer is the base of the 0x214-byte frame
 * (LEA ESI,[EBP-0x214] at 0x1c3963), so the decompiler's local_18/local_10
 * are fields inside it: EBP-0x14 is record+0x200 (the int16 file type compared
 * against the parameter) and the dwords at EBP-0x10 / EBP-0xf are the bytes at
 * record+0x204 / record+0x205 pushed as build_saved_game_file_index's two bool
 * flags (PUSH EDX from EBP-0xf first, so it is the LAST argument).  The path
 * field is record+0 — the same 0x206-byte layout whose display name at +0x100
 * saved_game_file_get_display_name reads.
 *
 * __strnicmp's arguments are PUSH EDX (the csstrlen result cached at EBP-8),
 * PUSH EAX (the record buffer), PUSH ECX (directory_path): first push is the
 * last argument, so it is __strnicmp(directory_path, record, length).
 *
 * The three register-arg callees take zero (XOR ESI,ESI at 0x1c3949 and
 * 0x1c39b8, XOR EAX,EAX at 0x1c3952) — the hard-drive memory unit.  The record
 * read is the loop's break condition (TEST AL,AL / JZ at 0x1c396e), and the
 * loop count from count_enumerated_profiles_in_mapfile is compared signed
 * (JLE at 0x1c3961, JL at 0x1c399a). */
int saved_game_file_find_profile_index_for_directory_path(char *directory_path,
                                                          int file_type)
{
  char record[0x206];
  int32_t length;
  int32_t result;
  int32_t count;
  int32_t i;

  result = -1;

  if (directory_path == NULL) {
    display_assert("directory_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x486,
                   1);
    system_exit(-1);
  }

  length = csstrlen(directory_path);

  if (take_mutex(*(int **)0x4eacbc, 3600000)) {
    if (take_mutex(*(int **)0x4eacc0, 3600000)) {
      count = (int32_t)count_enumerated_profiles_in_mapfile(0);

      if (enumerate_mapfile_start(0)) {
        for (i = 0; i < count; i++) {
          if (!enumerate_saved_game_file_from_mapfile(record))
            break;

          if (*(int16_t *)(record + 0x200) == (int16_t)file_type &&
              __strnicmp(directory_path, record, (unsigned int)length) == 0) {
            result = (int32_t)build_saved_game_file_index(
              i, 0, (int32_t)(int16_t)file_type, (bool)record[0x204],
              (bool)record[0x205]);
            break;
          }
        }

        enumerate_mapfile_end(0);
      }

      release_mutex(*(int **)0x4eacc0);
      release_mutex(*(int **)0x4eacbc);
    } else {
      error(2, "failed to take mapfile mutex");
      release_mutex(*(int **)0x4eacbc);
    }
  } else {
    error(2, "failed to take save game files mutex");
  }

  return result;
}

/* 0x1c3a30 — enumerate_default_playlist_profiles: walk the default playlist
 * files written by FUN_001c2120 (0x1c2120) and hand each one to
 * enumerate_saved_game_file, verifying its checksum on the way.
 *
 * The profile count comes from
 * playlist_profile_number_of_default_profiles_on_disk (ESI, cached at EBP-0x8)
 * and the localized name string list tag from tag_loaded('ustr',
 * "ui\default_multiplayer_game_setting_names") (EDI, cached at EBP-0x4 and
 * reloaded at 0x1c3be8 because ESI/EDI are clobbered inside the loop).  A -1
 * tag index reports and returns; a count <= 0 returns silently (TEST SI,SI /
 * JLE at 0x1c3a68).  All three RETs do MOV AX,BX, so the return value is the
 * 16-bit loop counter — the number of indices walked — which is why the early
 * failure exit at 0x1c3c00 returns a partial count.  The loop bound is a 16-bit
 * signed compare (CMP BX,word ptr [EBP-0x8] at 0x1c3bec) and the snprintf index
 * argument is sign-extended from BX (MOVSX EAX,BX at 0x1c3a7a), hence the
 * explicit int16_t casts.
 *
 * The record built here is the same 0x206-byte mapfile entry
 * saved_game_file_find_profile_index_for_directory_path reads back: path at
 * +0, wide display name at +0x100 (0x7f wide chars, terminator at +0x1fe),
 * the int16 file type at +0x200 (1 = playlist profile) and the two bool flags
 * at +0x204 / +0x205.  The +0x205 flag is set only when the checksum matches,
 * so a corrupt file is still enumerated, just flagged.  The MOV byte/REP
 * STOSD(0x81)/STOSB triple at 0x1c3acb zeroes exactly 0x206 bytes and is the
 * MSVC expansion of a `= ""` initializer on the block-scoped record.
 *
 * The checksum path reads the whole 0x200-byte file into a scratch block,
 * signs its leading 0x68 bytes into a separate 0x14-byte frame slot (EBP-0x1c)
 * and compares that against the stored signature at block+0x68 (LEA ECX,
 * [EBP-0x5c8] at 0x1c3b6c, i.e. block+0x68).  file_close runs on both the read
 * success and read failure paths, but not when file_open failed.
 *
 * enumerate_saved_game_file (0x1c33b0) takes the record in ESI (LEA ESI,
 * [EBP-0x224] at 0x1c3bd9, no other use of ESI at that point) and returns its
 * result in AL (TEST AL,AL at 0x1c3be4). */
int16_t enumerate_default_playlist_profiles(void)
{
  char block[0x200];
  char path[0x100];
  file_ref_t info;
  char signature[0x14];
  wchar_t *display_name;
  int16_t count;
  int tag_index;
  int i;

  count = playlist_profile_number_of_default_profiles_on_disk();
  tag_index =
    tag_loaded(0x75737472, "ui\\default_multiplayer_game_setting_names");
  i = 0;

  if (tag_index == -1) {
    error(2, "failed to enumerate default playlist files because their name "
             "string list tag was not loaded");
  } else if (count > 0) {
    do {
      display_name = (wchar_t *)FUN_0019d420(tag_index, i);
      snprintf(path, 0xff,
               "z:\\saved\\playlists\\default_playlist\\%02d\\blam.lst",
               (int)(int16_t)i);

      if (file_reference_create_from_path(&info, path, 0) != NULL &&
          file_exists(&info)) {
        char record[0x206] = "";

        csstrncpy(record, path, 0xff);
        record[0xff] = '\0';
        ustrncpy((wchar_t *)(record + 0x100), display_name, 0x7f);
        *(wchar_t *)(record + 0x1fe) = L'\0';
        *(int16_t *)(record + 0x200) = 1;
        record[0x204] = 1;

        if (file_open(&info, 1)) {
          if (file_read(&info, 0x200, block)) {
            saved_game_file_generate_checksum(block, 0x68, signature);

            if (csmemcmp(signature, block + 0x68, 0x14) == 0) {
              record[0x205] = 1;
            } else {
              error(2, "checksum validation failed for '%s'", record);
            }
          } else {
            error(2,
                  "failed to read saved game variant file to verify checksum");
          }

          if (!file_close(&info)) {
            error(2, "failed to close saved game variant file after verifying "
                     "checksum");
          }
        } else {
          error(2, "failed to open saved game variant file to verify checksum");
        }

        if (!enumerate_saved_game_file(record)) {
          error(2, "failed to enumerate default playlist file '%s'", path);
          return (int16_t)i;
        }
      }

      i++;
    } while ((int16_t)i < count);
  }

  return (int16_t)i;
}

/* 0x1c3c40 — enumerate_default_player_profiles: the player-profile twin of
 * enumerate_default_playlist_profiles above.  Same record shape, same
 * checksum-then-enumerate sequence, four differences, all read off the
 * disassembly:
 *
 *   - the localized names come from tag_loaded('ustr',
 *     "ui\shell\strings\default_player_profile_names") (PUSH 0x2898d0 / PUSH
 *     0x75737472 at 0x1c3c4c) and the tag index is cached at EBP-0x4 and
 *     reloaded at 0x1c3de5 because EDI is clobbered inside the loop;
 *   - the loop bound is the literal 2 (CMP BX,0x2 / JL at 0x1c3de9), not a
 *     disk count, so there is no pre-loop count test and the tag failure exit
 *     returns immediately;
 *   - the file type stored at record+0x200 is 0, not 1 (XOR EAX,EAX then MOV
 *     word ptr [EBP-0x22],AX / [EBP-0x20],AX at 0x1c3d1a — the same zero also
 *     terminates the wide name at record+0x1fe);
 *   - the checksum covers the leading 0x30 bytes of the 0x200-byte block
 *     (PUSH 0x30 at 0x1c3d5f) and the stored signature sits at block+0x30
 *     (LEA ECX,[EBP-0x5fc] at 0x1c3d69 against the block base EBP-0x62c).
 *
 * All three RETs do MOV AX,BX, so the return value is the 16-bit loop counter;
 * the enumerate failure exit at 0x1c3dfd returns the partial count.  The
 * MOV byte / REP STOSD(0x81) / STOSB triple at 0x1c3cca zeroes exactly 0x206
 * bytes — the MSVC expansion of a `= ""` initializer on the block-scoped
 * record.  enumerate_saved_game_file takes that record in ESI (LEA ESI,
 * [EBP-0x220] at 0x1c3dd6, ESI dead at that point) and answers in AL (TEST
 * AL,AL at 0x1c3de1).  file_close runs on both the read-success and
 * read-failure paths, but not when file_open failed. */
int16_t enumerate_default_player_profiles(void)
{
  char block[0x200];
  char path[0x100];
  file_ref_t info;
  char signature[0x14];
  wchar_t *display_name;
  int tag_index;
  int i;

  tag_index =
    tag_loaded(0x75737472, "ui\\shell\\strings\\default_player_profile_names");
  i = 0;

  if (tag_index == -1) {
    error(2, "failed to enumerate default player profile files because their "
             "name string list tag was not loaded");
    return (int16_t)i;
  }

  do {
    display_name = (wchar_t *)FUN_0019d420(tag_index, i);
    snprintf(path, 0xff,
             "z:\\saved\\player_profiles\\default_profile\\%02d.sav",
             (int)(int16_t)i);

    if (file_reference_create_from_path(&info, path, 0) != NULL &&
        file_exists(&info)) {
      char record[0x206] = "";

      csstrncpy(record, path, 0xff);
      record[0xff] = '\0';
      ustrncpy((wchar_t *)(record + 0x100), display_name, 0x7f);
      *(wchar_t *)(record + 0x1fe) = L'\0';
      *(int16_t *)(record + 0x200) = 0;
      record[0x204] = 1;

      if (file_open(&info, 1)) {
        if (file_read(&info, 0x200, block)) {
          saved_game_file_generate_checksum(block, 0x30, signature);

          if (csmemcmp(signature, block + 0x30, 0x14) == 0) {
            record[0x205] = 1;
          } else {
            error(2, "checksum validation failed for '%s'", record);
          }
        } else {
          error(2, "failed to read saved game player profile file to verify "
                   "checksum");
        }

        if (!file_close(&info)) {
          error(2, "failed to close saved game player profile file after "
                   "verifying checksum");
        }
      } else {
        error(2, "failed to open saved game player profile file to verify "
                 "checksum");
      }

      if (!enumerate_saved_game_file(record)) {
        error(2, "failed to enumerate default player profile file '%s'", path);
        return (int16_t)i;
      }
    }

    i++;
  } while ((int16_t)i < 2);

  return (int16_t)i;
}

/* 0x1c3e40 — get_nth_entry_in_mapfile.  Name confirmed by the callers' error
 * strings ("get_nth_entry_in_mapfile() failed").  Read-side twin of
 * set_nth_entry_in_mapfile below, with the same shape.
 *
 *
 * `memory_unit_index` arrives in AX (MOV SI,AX at 0x1c3e48).  The entry index
 * arrives in EDI: the callee never saves or writes EDI before MOVZX EDI,DI at
 * 0x1c3f37, so only its low 16 bits are used (the kb decl keeps the
 * immutable int32_t@<edi>).  The record pointer is the one stack argument at
 * [EBP+8].  Entry asserts are lines 0x7a7/0x7a9/0x7aa.  The mapfile mutex at
 * 0x4eacc0 is taken with the 3600000 ms timeout; failing it reports and
 * returns the flag still in BL (MOV AL,BL at 0x1c401b).  The file is opened
 * with flag 1 (read).  file_get_eof is divided by 0x206 (unsigned DIV at
 * 0x1c3f35) only to report corruption; the record is read only when
 * offset + 0x206 <= size (LEA EAX,[ESI+0x206] / CMP EAX,EBX / JA at 0x1c3f5c).
 * The out-of-range path leaves the result at its initial 0; the read-failure
 * path stores 0 explicitly (MOV byte ptr [EBP-1],0 at 0x1c3f9d).  A failed
 * close clears the flag again.  The result byte at [EBP-1] is returned in AL
 * at 0x1c3fff. */
bool get_nth_entry_in_mapfile(int16_t memory_unit_index, int32_t entry_index,
                              void *entry)
{
  bool result;
  uint16_t unit;
  uint16_t index;
  uint32_t size;
  uint32_t offset;

  result = 0;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7a7,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7a9,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9 || entry == NULL) {
    display_assert(
      "(memory_unit_index < NUMBER_OF_MEMORY_UNITS) && (file != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7aa, 1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacc0, 3600000)) {
    error(2, "failed to take mapfile mutex");
    return result;
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0 &&
      file_open((file_ref_t *)0x4eabb0, 1)) {
    size = (uint32_t)file_get_eof((file_ref_t *)0x4eabb0);
    index = (uint16_t)entry_index;
    offset = (uint32_t)index * 0x206;

    if (size % 0x206 != 0) {
      error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
            unit);
    }

    if (offset + 0x206 <= size) {
      if (file_set_position((file_ref_t *)0x4eabb0, (int32_t)offset) &&
          file_read((file_ref_t *)0x4eabb0, 0x206, entry)) {
        result = 1;
      } else {
        result = 0;
        error(2, "failed to retrieve entry #%d from memory unit mapfile (#%d)",
              index, unit);
      }
    } else {
      error(2, "invalid profile index (#%d) into memory unit #%d specified",
            index, unit);
    }

    if (!file_close((file_ref_t *)0x4eabb0)) {
      error(2, "failed to close memory unit mapfile for memory unit #%d", unit);
      result = 0;
    }
  } else {
    error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  }

  release_mutex(*(int **)0x4eacc0);
  return result;
}

/* Overwrite the nth 0x206-byte record of the memory-unit mapfile in place.
 * `memory_unit_index` arrives in AX (MOV SI,AX at 0x1c4038); the entry index is
 * the 16-bit stack slot at [EBP+8] (MOVZX EBX,word ptr at 0x1c4126) and the
 * source record is the pointer at [EBP+0xc].  The same three entry asserts as
 * enumerate_mapfile_end guard it (lines 0x7e2/0x7e4/0x7e5), then the mapfile
 * mutex at 0x4eacc0 is taken with the 0x36ee80 = 3600000 ms timeout; failing
 * that reports and returns false with the flag still in BL (MOV AL,BL at
 * 0x1c4203).  The zero-extended unit indexes the mapfile-path table at
 * 0x32eb98 and is the trailing %d of every message; the file is opened with
 * flag 2 (write).  file_get_eof's result is checked for a 0x206 remainder
 * (unsigned DIV at 0x1c4124) purely to report corruption, and the record is
 * written only when offset + 0x206 <= size (LEA EDX,[ESI+0x206] / CMP EDX,EAX /
 * JA at 0x1c414c).  The out-of-range path leaves the result flag at its
 * initial 0 without re-storing it; the write-failure path stores 0 explicitly
 * (MOV byte ptr [EBP-1],0 at 0x1c418a).  A failed close clears the flag again.
 * The result byte at [EBP-1] is returned in AL at 0x1c41e6. */
bool set_nth_entry_in_mapfile(int16_t memory_unit_index, int16_t entry_index,
                              const void *entry)
{
  bool result;
  uint16_t unit;
  uint16_t index;
  uint32_t size;
  uint32_t offset;

  result = 0;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7e2,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7e4,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9 || entry == NULL) {
    display_assert(
      "(memory_unit_index < NUMBER_OF_MEMORY_UNITS) && (file != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x7e5, 1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacc0, 3600000)) {
    error(2, "failed to take mapfile mutex");
    return result;
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0 &&
      file_open((file_ref_t *)0x4eabb0, 2)) {
    size = (uint32_t)file_get_eof((file_ref_t *)0x4eabb0);
    index = (uint16_t)entry_index;
    offset = (uint32_t)index * 0x206;

    if (size % 0x206 != 0) {
      error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
            unit);
    }

    if (offset + 0x206 <= size) {
      if (file_set_position((file_ref_t *)0x4eabb0, (int32_t)offset) &&
          file_write((file_ref_t *)0x4eabb0, 0x206, entry)) {
        result = 1;
      } else {
        result = 0;
        error(2, "failed to update entry #%d from memory unit mapfile (#%d)",
              index, unit);
      }
    } else {
      error(2, "invalid profile index (#%d) into memory unit #%d specified",
            index, unit);
    }

    if (!file_close((file_ref_t *)0x4eabb0)) {
      error(2, "failed to close memory unit mapfile for memory unit #%d", unit);
      result = 0;
    }
  } else {
    error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  }

  release_mutex(*(int **)0x4eacc0);
  return result;
}

/* Append a new 0x206-byte record to the end of the memory-unit mapfile and
 * report the index it landed at.  `memory_unit_index` arrives in AX (MOV SI,AX
 * at 0x1c4216); `file` is the record pointer at [EBP+8] and `profile_index` the
 * out-parameter at [EBP+0xc].  The three entry asserts are lines 0x821/0x823/
 * 0x824, the third covering all of index/file/profile_index (TEST at
 * 0x1c426c-0x1c427e).  The mapfile mutex at 0x4eacc0 is taken with the same
 * 0x36ee80 = 3600000 ms timeout; failing it reports and returns the flag still
 * in BL (MOV AL,BL at 0x1c43e7).  The zero-extended unit indexes the
 * mapfile-path table at 0x32eb98 and is the trailing %d of every message that
 * has one.  file_get_eof is divided by 0x206 (unsigned DIV at 0x1c4306): the
 * quotient is the new entry index and the append offset (IMUL ESI,0x206 at
 * 0x1c430f, computed before the limit test), the remainder only reports
 * corruption.  The 100-profile limit is a SIGNED compare (CMP EDI,0x64 / JGE at
 * 0x1c4315) and its message takes no argument.  On a successful write the flag
 * is stored first (MOV byte ptr [EBP-1],1 at 0x1c435d) and then the count is
 * written through the out-pointer.  A failed close clears the flag again.  The
 * result byte at [EBP-1] is returned in AL at 0x1c43cb. */
bool append_entry_to_mapfile(int16_t memory_unit_index, const void *file,
                             uint32_t *profile_index)
{
  bool result;
  uint16_t unit;
  uint32_t size;
  uint32_t count;
  uint32_t offset;

  result = 0;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x821,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x823,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9 || file == NULL ||
      profile_index == NULL) {
    display_assert(
      "(memory_unit_index < NUMBER_OF_MEMORY_UNITS) && (file != NULL) && "
      "(profile_index != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x824, 1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacc0, 3600000)) {
    error(2, "failed to take mapfile mutex");
    return result;
  }

  unit = (uint16_t)memory_unit_index;

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0, ((const char **)0x32eb98)[unit], 0) != 0 &&
      file_open((file_ref_t *)0x4eabb0, 2)) {
    size = (uint32_t)file_get_eof((file_ref_t *)0x4eabb0);
    count = size / 0x206;
    offset = count * 0x206;

    if ((int32_t)count < 100) {
      if (size % 0x206 != 0) {
        error(2, "memory unit mapfile for memory unit #%d is possibly corrupt",
              unit);
      }

      if (file_set_position((file_ref_t *)0x4eabb0, (int32_t)offset) &&
          file_write((file_ref_t *)0x4eabb0, 0x206, file)) {
        result = 1;
        *profile_index = count;
      } else {
        result = 0;
        error(2, "failed to append entry to memory unit mapfile (#%d)", unit);
      }
    } else {
      error(2, "can't add new entry to memory unit mapfile because the maximum "
               "number of profiles have already been added");
    }

    if (!file_close((file_ref_t *)0x4eabb0)) {
      error(2, "failed to close memory unit mapfile for memory unit #%d", unit);
      result = 0;
    }
  } else {
    error(2, "failed to open memory unit mapfile for memory unit #%d", unit);
  }

  release_mutex(*(int **)0x4eacc0);
  return result;
}

/* 0x1c43f0 — remove_nth_entry_in_mapfile.  Name confirmed by the caller's
 * error string ("remove_nth_entry_in_mapfile() failed").
 *
 *
 * `memory_unit_index` arrives in AX (MOV DI,AX at 0x1c4405) and the entry
 * index in CX (MOVZX ESI,CX / IMUL ESI,ESI,0x206 at 0x1c43fb, before the
 * asserts).  Entry asserts are lines 0x87a/0x87c/0x87d.  The mapfile is
 * rewritten only when file_get_size succeeds and the size covers the whole
 * entry (CMP EAX,EDI / JB at 0x1c44dd, EDI = offset + 0x206), then opened
 * with flag 3 (read|write).  file_set_position's AL is the running result
 * (MOV BL,AL at 0x1c4507) and the shift loop runs only when it is exactly 1
 * (CMP BL,1 at 0x1c450c).  Each following record is copied down one slot
 * through the 0x206-byte stack buffer at EBP-0x20c; a failed read or write
 * reports, clears the result and skips the truncate.  A successful shift
 * truncates the file by one record (ADD EAX,-0x206 / file_set_eof), whose AL
 * becomes the result.  A failed close reports and clears it.  Every exit
 * returns BL. */
bool remove_nth_entry_in_mapfile(int16_t memory_unit_index, int16_t entry_index)
{
  enumerated_saved_game_file_t file;
  uint32_t mapfile_size;
  uint32_t entry_offset;
  uint32_t next_entry_offset;
  bool success;

  entry_offset = (uint32_t)(uint16_t)entry_index * 0x206;
  success = 0;

  if (memory_unit_index != 0) {
    display_assert("memory_unit_index==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x87a,
                   1);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc8 != 0) {
    display_assert("!saved_game_files_globals.enumeration_in_progress",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x87c,
                   1);
    system_exit(-1);
  }

  if ((uint16_t)memory_unit_index >= 9) {
    display_assert("memory_unit_index < NUMBER_OF_MEMORY_UNITS",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x87d,
                   1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacc0, 3600000)) {
    error(2, "failed to take mapfile mutex");
    return success;
  }

  if (file_reference_create_from_path(
        (file_ref_t *)0x4eabb0,
        ((const char **)0x32eb98)[(uint16_t)memory_unit_index], 0) != 0 &&
      file_get_size((file_ref_t *)0x4eabb0, &mapfile_size) &&
      mapfile_size >= entry_offset + 0x206 &&
      file_open((file_ref_t *)0x4eabb0, 3)) {
    success = file_set_position((file_ref_t *)0x4eabb0, (int)entry_offset);

    next_entry_offset = entry_offset + 0x206;

    if (success == 1) {
      while (next_entry_offset < mapfile_size) {
        if (!file_read_from_position((file_ref_t *)0x4eabb0,
                                     (int)next_entry_offset, 0x206, &file) ||
            !file_write_to_position((file_ref_t *)0x4eabb0, (int)entry_offset,
                                    0x206, &file)) {
          error(2, "failed to update memory unit mapfile after removing an "
                   "enumerated file");
          success = 0;
          break;
        }

        next_entry_offset += 0x206;
        entry_offset += 0x206;
      }
    }

    if (success) {
      success =
        file_set_eof((file_ref_t *)0x4eabb0, (int)(mapfile_size - 0x206));
    }

    if (!file_close((file_ref_t *)0x4eabb0)) {
      error(2, "failed to close memory unit map file");
      success = 0;
    }
  }

  release_mutex(*(int **)0x4eacc0);
  return success;
}

/* Unpack a saved game file index and return the display name of the entry it
 * names.  The two fields read here are the ones build_saved_game_file_index
 * (0x1c36f0) packs: MOVZX ESI,AH at 0x1c460d takes the 8-bit memory unit at
 * bits 8-15, SAR EAX,0x10 / AND EAX,0xfff at 0x1c4610 takes the 12-bit file
 * index at bits 16-27.  Both are range-checked (TEST/JL plus CMP 9 and CMP
 * 0x64) before use; either failure reports through error() and leaves the
 * cleared name buffer.  get_nth_entry_in_mapfile takes the memory unit in AX
 * (MOV SI,AX at 0x1c3e48) and the entry index in DI (MOVZX EDI,DI at 0x1c3f37,
 * with EDI never written in the callee) and fills the 0x206-byte stack entry
 * (PUSH 0x206 / file_read at 0x1c3f7c).  The display name lives at +0x100
 * inside that entry — the ustrncpy source is EBP-0x108 against a buffer based
 * at EBP-0x208.  Both exits return the static name buffer at 0x4eaab0 (MOV
 * EAX,0x4eaab0 at 0x1c468f/0x1c46a9). */
wchar_t *saved_game_file_get_display_name(int32_t saved_game_file_index)
{
  char entry[0x208];
  int16_t memory_unit_index;
  int32_t file_index;

  memory_unit_index = (int16_t)((saved_game_file_index >> 8) & 0xff);
  file_index = (saved_game_file_index >> 0x10) & 0xfff;

  if (memory_unit_index != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 299, 1);
    system_exit(-1);
  }

  *(wchar_t *)0x4eaab0 = 0;

  if (memory_unit_index >= 0 && memory_unit_index < 9 && file_index >= 0 &&
      file_index < 100) {
    if (get_nth_entry_in_mapfile(memory_unit_index, file_index, entry)) {
      ustrncpy((wchar_t *)0x4eaab0, (wchar_t *)(entry + 0x100), 0x7f);
      *(wchar_t *)0x4eabae = 0;
    }
  } else {
    error(2, "invalid saved game file index");
  }

  return (wchar_t *)0x4eaab0;
}

/* 0x1c46c0 — delete_enumerated_saved_game_file.  Name confirmed by its own
 * error strings ("...failed in delete_enumerated_saved_game_file()").
 *
 * The memory-units-dirty byte at 0x4eacc7 is tested first; when set it
 * reports and exits through reset_last_player1_profile_index with the flag
 * at [EBP-1] still 0.  The packed index is unpacked like the siblings
 * (AND 0xf type, MOVZX EBX,AH unit, SAR 0x10 / AND 0xfff file index) and the
 * hard-drive assert is line 0x1ef.  get_nth_entry_in_mapfile gets the unit
 * in EAX and the file index still in EDI, filling the 0x208-byte slot at
 * EBP-0x214.  success = (unit == 0) (SETZ AL at 0x1c4781); when bit 30 of
 * the index (read-only) is clear, XDeleteSaveGame (0x1d3185, __stdcall, no
 * ADD ESP) is called with wide_to_ascii(root[unit], root_path, 8) and the
 * entry's display name (EBP-0x114 = entry+0x100).  remove_nth_entry_in_mapfile
 * gets the unit in EAX and the file index in ECX (MOV ECX,EDI at 0x1c47d9).
 * The "unmount" / "mount" messages are unreachable after the line-0x1ef
 * assert but are present in the binary and kept. */
bool delete_enumerated_saved_game_file(int32_t saved_game_file_index)
{
  enumerated_saved_game_file_t file;
  bool success;

  success = false;

  if (*(uint8_t *)0x4eacc7 != 0) { /* memory_units_dirty */
    error(2, "failed to delete saved game file because memory units have "
             "been inserted/removed");
  } else {
    int32_t type = saved_game_file_index & 0xf;
    int32_t memory_unit = (saved_game_file_index >> 8) & 0xff;
    int32_t n = (saved_game_file_index >> 16) & 0xfff;

    if (memory_unit != 0) {
      display_assert("memory_unit==_memory_unit_hard_drive",
                     "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x1ef,
                     true);
      system_exit(-1);
    }

    if (type >= 0 && type < 2 && memory_unit >= 0 && memory_unit < 9 &&
        n >= 0 && n < 100) {
      if (get_nth_entry_in_mapfile((int16_t)memory_unit, n, &file)) {
        char root_path[8] = { 0 };

        success = (memory_unit == 0);

        if (success) {
          if ((saved_game_file_index & 0x40000000) == 0) {
            if (FUN_001d3185(
                  wide_to_ascii(((const wchar_t **)0x32eb94)[memory_unit],
                                root_path, 8),
                  file.display_name) != 0) {
              error(2, "XDeleteSaveGame() failed... ghost meta data likely");
              success = false;
            }
          }

          if (!remove_nth_entry_in_mapfile((int16_t)memory_unit, (int16_t)n)) {
            error(2, "remove_nth_entry_in_mapfile() failed");
            success = false;
          }

          if (memory_unit != 0) {
            error(2, "failed to unmount memory unit");
          }
        } else {
          error(2, "failed to mount memory unit #%d", memory_unit);
        }
      } else {
        error(2, "get_nth_entry_in_mapfile() failed in "
                 "delete_enumerated_saved_game_file()");
      }
    } else {
      error(2, "delete_enumerated_saved_game_file() failed because the game "
               "file index was invalid");
    }
  }

  reset_last_player1_profile_index();

  return success;
}

/* 0x1c4850 — saved_game_file_open.  Opens the file named by a packed saved
 * game file index for read/write.  The kb name enumerate_memory_units_test
 * (CEA PDB line containment) was wrong: that is a void no-argument hs
 * command.  This body's asserts are lines 0x241 and 0x244-0x247, the same
 * shape as saved_game_file_close (0x1c2890, lines 0x25b/0x25e-0x261), and
 * both callers report "failed to open player profile file" on false.
 *
 * Result is the conjunction at 0x1c4940-0x1c4976: get_nth_entry_in_mapfile
 * (unit in EAX, file index in EDI, entry at EBP-0x208) AND
 * file_reference_create_from_path(saved_game_file, entry.path, 0) AND
 * unit == 0 AND file_open(saved_game_file, 3). */
bool saved_game_file_open(file_ref_t *saved_game_file,
                          int32_t saved_game_file_index)
{
  enumerated_saved_game_file_t file;
  int32_t type;
  int32_t memory_unit;
  int32_t n;
  bool success;

  type = saved_game_file_index & 0xf;
  memory_unit = (saved_game_file_index >> 8) & 0xff;
  n = (saved_game_file_index >> 16) & 0xfff;

  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x241,
                   true);
    system_exit(-1);
  }

  if (saved_game_file == NULL) {
    display_assert("saved_game_file",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x244,
                   true);
    system_exit(-1);
  }

  if (type < 0 || type >= 2) {
    display_assert("(type >= 0) && (type < NUMBER_OF_SAVED_GAME_FILE_TYPES)",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x245,
                   true);
    system_exit(-1);
  }

  if (memory_unit < 0 || memory_unit >= 9) {
    display_assert(
      "(memory_unit >= 0) && (memory_unit < NUMBER_OF_MEMORY_UNITS)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x246, true);
    system_exit(-1);
  }

  if (n < 0 || n >= 100) {
    display_assert(
      "(n >= 0) && (n < "
      "MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x247, true);
    system_exit(-1);
  }

  success = get_nth_entry_in_mapfile((int16_t)memory_unit, n, &file) &&
            file_reference_create_from_path(saved_game_file, file.path, 0) &&
            memory_unit == 0 && file_open(saved_game_file, 3);
  return success;
}

/* 0x1c4990 — synchronize_metadata_display_name_with_profile_name.  Name
 * confirmed by its own error string ("XCreateSaveGame() failed in
 * synchronize_metadata_display_name_with_profile_name() ...").
 *
 *
 * Renames the XAPI save-game metadata of a mapfile entry to match
 * game_display_name: creates a new save game with the new name, copies the
 * entry's file(s) into its directory, deletes the old save game and rewrites
 * the mapfile record.  game_display_name is the second stack argument; the
 * assert string "ustrlen(game_display_name)<MAXIMUM_SAVED_GAME_NAME_LENGTH"
 * names it, and the caller FUN_001c15c0 passes the profile payload, whose
 * first field is the wide profile name (player_profile_new ustrncpy's the name to
 * offset 0 of the same record).
 *
 * The result starts TRUE (MOV BL,1 at 0x1c49b1), so the get_nth failure,
 * mount failure, XCreateSaveGame failure and "name unchanged" paths all return
 * true.  Asserts are lines 0x306/0x309/0x30a/0x30b/0x30d and 0x343.
 *
 * Frame (SUB ESP,0x610): file at EBP-0x410 (0x206 bytes, filled by
 * get_nth_entry_in_mapfile with the unit in EAX, n in EDI); root_path[8] at
 * EBP-8; save_game_directory[0x100] at EBP-0x208 (zero-filled with REP STOSD);
 * new_path[0x100] at EBP-0x108; old/new persistent-storage paths [0x100] at
 * EBP-0x510 / EBP-0x610.  Every snprintf is bounded to 0xff with an explicit
 * terminator store at index 0xff.
 *
 * Callees: FUN_001d2f22 = XCreateSaveGame (__stdcall, 6 args; creation flag
 * 1, the 0 pushed from ESI which is memory_unit == 0 on this path),
 * FUN_001d21f2 = CopyFileA (__stdcall: RET 0xc, no ADD ESP at the call sites;
 * its AL is kept as the byte result, MOV BL,AL), FUN_001d3185 =
 * XDeleteSaveGame (__stdcall, 2 args), FUN_001c0720 = the persistent-storage
 * filename (called twice, once per use, as in the binary).  set_nth gets the
 * unit in EAX (XOR EAX,EAX: constant 0 here) and n / &file on the stack. */
bool synchronize_metadata_display_name_with_profile_name(
  int32_t saved_game_file_index, wchar_t *game_display_name)
{
  enumerated_saved_game_file_t file;
  bool success;
  int32_t type;
  int32_t memory_unit;
  int32_t n;

  success = 1;
  type = saved_game_file_index & 0xf;
  memory_unit = (saved_game_file_index >> 8) & 0xff;
  n = (saved_game_file_index >> 0x10) & 0xfff;

  if (memory_unit != 0) {
    display_assert("memory_unit==_memory_unit_hard_drive",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x306,
                   true);
    system_exit(-1);
  }

  if (type < 0 || type >= 2) {
    display_assert("(type >= 0) && (type < NUMBER_OF_SAVED_GAME_FILE_TYPES)",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x309,
                   true);
    system_exit(-1);
  }

  if (memory_unit < 0 || memory_unit >= 9) {
    display_assert(
      "(memory_unit >= 0) && (memory_unit < NUMBER_OF_MEMORY_UNITS)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x30a, true);
    system_exit(-1);
  }

  if (n < 0 || n >= 100) {
    display_assert(
      "(n >= 0) && (n < "
      "MAXIMUM_ENUMERATED_SAVED_GAME_FILES_ANY_TYPE_PER_MEMORY_UNIT)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x30b, true);
    system_exit(-1);
  }

  if ((uint32_t)ustrlen(game_display_name) >= 0x80) {
    display_assert("ustrlen(game_display_name)<MAXIMUM_SAVED_GAME_NAME_LENGTH",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x30d,
                   true);
    system_exit(-1);
  }

  if (get_nth_entry_in_mapfile((int16_t)memory_unit, n, &file)) {
    if (game_display_name != NULL && game_display_name[0] != 0 &&
        ustrcmp(file.display_name, game_display_name) != 0) {
      if (memory_unit == 0) {
        char root_path[8] = { 0 };
        char save_game_directory[0x100] = { 0 };

        if (FUN_001d2f22(
              wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
              game_display_name, 1, 0, save_game_directory, 0x100) == 0) {
          char new_path[0x100];

          switch (file.type) {
          case 0: {
            char old_persistent_storage_path[0x100];
            char new_persistent_storage_path[0x100];
            char *filename;

            snprintf(new_path, 0xff, "%s%s", save_game_directory, "blam.sav");
            new_path[0xff] = 0;
            success = (bool)FUN_001d21f2(file.path, new_path, 1);

            if (success == 1) {
              csstrncpy(old_persistent_storage_path, file.path, 0xff);
              old_persistent_storage_path[0xff] = 0;
              filename = crt_strstr(old_persistent_storage_path, "blam.sav");

              if (filename != NULL) {
                *filename = 0;
                csstrncat(old_persistent_storage_path, FUN_001c0720(), 0xff);
                old_persistent_storage_path[0xff] = 0;
                snprintf(new_persistent_storage_path, 0xff, "%s%s",
                         save_game_directory, FUN_001c0720());
                success = (bool)FUN_001d21f2(old_persistent_storage_path,
                                             new_persistent_storage_path, 1);
              } else {
                success = 0;
              }
            }
            break;
          }

          case 1:
            snprintf(new_path, 0xff, "%s%s", save_game_directory, "blam.lst");
            new_path[0xff] = 0;
            success = (bool)FUN_001d21f2(file.path, new_path, 1);
            break;

          default:
            display_assert("!\"unknown enumerated file type\"",
                           "c:\\halo\\SOURCE\\saved games\\saved_game_files.c",
                           0x343, true);
            system_exit(-1);
            success = 0;
            break;
          }

          if (success == 1) {
            if (FUN_001d3185(root_path, file.display_name) != 0) {
              error(2, "XDeleteSaveGame() failed to delete old saved game "
                       "metadata in rename_metadata_display_name()");
            }

            csstrncpy(file.path, new_path, 0xff);
            file.path[0xff] = 0;
            ustrncpy(file.display_name, game_display_name, 0x7f);
            file.display_name[0x7f] = 0;

            if (!set_nth_entry_in_mapfile((int16_t)memory_unit, (int16_t)n,
                                          &file)) {
              error(2, "failed to update memory unit mapfile after renaming "
                       "saved game metadata");
            }
          } else if (!success) {
            if (FUN_001d3185(root_path, game_display_name) != 0) {
              error(2, "XDeleteSaveGame() failed to delete empty saved game "
                       "metadata in rename_metadata_display_name()");
            }
          }
        } else {
          error(2, "XCreateSaveGame() failed in "
                   "synchronize_metadata_display_name_with_profile_name() "
                   "(maybe the name is already in use?)");
        }
      } else {
        error(2, "failed to mount memory unit #%d", memory_unit);
      }
    }
  } else {
    error(2, "get_nth_entry_in_mapfile() failed");
  }

  return success;
}

/* 0x1c4da0 — copy the path of the directory holding the saved game file
 * named by profile_index into path (at most 0xff chars + NUL).
 *
 * Confirmed from disassembly:
 *   - XOR BL,BL at 0x1c4dae is the return value for the profile_index == -1
 *     early exit (MOV AL,BL at 0x1c4f03); path[0] is cleared first.
 *   - The index is unpacked exactly like saved_game_file_get_display_name:
 *     MOVZX EBX,AH (memory unit, bits 8-15), SAR 0x10 / AND 0xfff (file
 *     index, bits 16-27).  Range checks are unit < 9, 0 <= file < 100.
 *   - get_nth_entry_in_mapfile gets the unit in EAX (MOV EAX,EBX at
 *     0x1c4e3e), the file index in EDI and the 0x208-byte stack entry
 *     (EBP-0x208).  The entry's path is at +0x000 and a 16-bit file type at
 *     +0x200 (MOV AX,[EBP-8]).
 *   - Type 0 strips "blam.sav", type 1 strips "blam.lst" (crt_strstr, then
 *     *found = 0); any other type reports through error() and fails.
 *   - A pathname that does not contain the file name reports through
 *     error() and re-clears path[0] (0x1c4ec6). */
bool saved_game_file_get_path_to_enclosing_directory(int profile_index,
                                                     char *path)
{
  char entry[0x208];
  bool success;
  int memory_unit_index;
  int file_index;
  int16_t type;
  const char *file_name;
  char *found;

  success = 0;

  if (path == NULL) {
    display_assert("full_path",
                   "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x38d,
                   1);
    system_exit(-1);
  }

  *path = 0;

  if (profile_index != -1) {
    memory_unit_index = (profile_index >> 8) & 0xff;
    file_index = (profile_index >> 0x10) & 0xfff;

    if (memory_unit_index != 0) {
      display_assert("memory_unit==_memory_unit_hard_drive",
                     "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x397,
                     1);
      system_exit(-1);
    }

    if (memory_unit_index >= 0 && memory_unit_index < 9 && file_index >= 0 &&
        file_index < 100) {
      if (get_nth_entry_in_mapfile((int16_t)memory_unit_index, file_index,
                                   entry)) {
        type = *(int16_t *)(entry + 0x200);
        if (type == 0) {
          file_name = "blam.sav";
        } else if (type == 1) {
          file_name = "blam.lst";
        } else {
          error(2, "unknown saved game file type");
          return 0;
        }

        csstrncpy(path, entry, 0xff);
        path[0xff] = 0;
        found = crt_strstr(path, file_name);
        if (found != NULL) {
          *found = 0;
          return 1;
        }

        error(2, "player profile pathname doesn't appear to be valid");
        *path = 0;
        return 0;
      }

      error(2, "unable to locate the specified player profile file in memory "
               "unit mapfile");
      return 0;
    }

    error(2, "invalid saved game file index");
    return 0;
  }

  return success;
}

/* 0x1c4f30 — delete every saved-game directory XFindFirstSaveGame /
 * XFindNextSaveGame enumerate on the hard-drive root
 * (*(const wchar_t **)0x32eb94), capped at 100 (CMP EBX,0x64 at 0x1c4f90),
 * then re-run the two default-profile enumerators.
 *
 * Same unit loop shape as FUN_001c5010 below: unit starts at 0 and the loop
 * repeats only while the incremented unit is 0 (INC ESI / JZ at 0x1c4ffa).
 * No mutex is taken here.  enumerate_saved_game_files_start(unit) gets the
 * unit in AX; a false return skips the body including
 * enumerate_saved_game_files_end.  The `unit == 0` guard (0x1c4f4f) skips
 * only the delete/enumerate block, not enumerate_saved_game_files_end.
 *
 * XDeleteSaveGame (FUN_001d3185, __stdcall) gets the 8-byte ascii root and
 * the wide save-game name at find_data+0x244.  The counter is never
 * incremented inside the loop; only the 16-bit sum of the two enumerators
 * is added to it afterwards (MOVSX ECX,AX / ADD EBX,ECX at 0x1c4ff0). */
void FUN_001c4f30(void)
{
  int unit;
  int count;
  char root_path[8] = "";
  char find_data[XGAME_FIND_DATA_SIZE];
  int find_handle;
  int16_t playlist_count;

  count = 0;

  for (unit = 0; unit == 0; unit++) {
    if (enumerate_saved_game_files_start((int16_t)unit)) {
      if (unit == 0) {
        find_handle = XFindFirstSaveGame(
          wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8), find_data);

        if (find_handle != -1) {
          while (count < 100) {
            if (FUN_001d3185(root_path, (const wchar_t *)(find_data + 0x244)) !=
                0)
              error(2, "XDeleteSaveGame() failed to delete profile");

            if (!XFindNextSaveGame(find_handle, find_data))
              break;
          }

          if (!XFindClose(find_handle))
            error(2, "XFindClose() failed");
        }

        playlist_count = enumerate_default_playlist_profiles();
        count +=
          (int16_t)(enumerate_default_player_profiles() + playlist_count);
      }

      enumerate_saved_game_files_end((int16_t)unit);
    }
  }
}

/* 0x1c5010 — verify (and, if necessary, upgrade) the checksums of every
 * saved game file found on memory unit 0, then fold in the default
 * playlist/player profile counts.  No symbol from the 2276 dump names this
 * address, so it keeps the mechanical FUN_ name (T4; see the
 * naming-confidence skill).
 *
 * Only two real callers exist (caller disassembly saved at
 * artifacts/lift/caller_disasm_FUN_001c5010.txt), plus a plain
 * JMP-forwarding thunk at 0x1c58f0; both real callers gate the call behind
 * `if (*(uint8_t *)0x4eacc7) FUN_001c5010();`, with no arguments pushed and
 * no use of AL/EAX afterward -- matching `void FUN_001c5010(void)`.  This
 * function clears 0x4eacc7 back to 0 at 0x1c53db right before returning,
 * making it the (previously undocumented) consumer of the "memory units
 * changed" dirty flag set by saved_game_files_notify_memory_units_changed()
 * and saved_game_files_initialize().
 *
 * Outer loop (0x1c5020-0x1c53d3): `EDI = 0; do { ...; EDI++; } while
 * (EDI == 0)` in the disassembly -- a real loop shape, but EDI only ever
 * reaches 1 in practice (it would have to wrap a full 32 bits to loop
 * again), so the body executes exactly once per call.  Reproduced as
 * `for (unit = 0; unit == 0; unit++)` rather than flattened, per the
 * project's preserve-control-flow-shape convention.  take_mutex/
 * release_mutex bracket the whole iteration (0x4eacbc, the save-game-files
 * mutex used throughout this file, 3600000 ms timeout); a failed
 * take_mutex skips straight to the increment (no start()/end() call at
 * all).  A failed enumerate_saved_game_files_start(unit) skips the whole
 * per-unit body, including enumerate_saved_game_files_end -- only
 * release_mutex still runs (0x1c5054 JZ 0x1c53c0).
 *
 * The `if (unit == 0)` guard (0x1c505a/0x1c505c) is unreachable-false in
 * practice (enumerate_saved_game_files_start already asserts unit == 0),
 * but is reproduced because it is really there; when it is false the
 * default-playlist/player-profile calls at 0x1c53a5/0x1c53ac are skipped
 * entirely and control goes straight to enumerate_saved_game_files_end.
 *
 * Per-file record ("entry", the same 0x206-byte mapfile record documented
 * on enumerate_default_playlist_profiles/enumerate_default_player_profiles
 * above): path at +0, wide display name at +0x100 (0x7f wide chars,
 * terminator at +0x1fe), int16 kind at +0x200 (0 = primary "blam.sav",
 * 1 = alternate "blam.lst", -1 = neither found), and the checksum_valid
 * bool at +0x205.  Unlike the two sibling enumerators, this function never
 * writes entry+0x204 (confirmed absent from the disassembly), so that byte
 * stays 0 from the zero-fill in every record this function enumerates --
 * a real, confirmed difference from enumerate_default_playlist_profiles /
 * enumerate_default_player_profiles, whose records always set it to 1;
 * its meaning is not established here either way.
 *
 * XFindFirstSaveGame/XFindNextSaveGame walk the same root
 * (*(const wchar_t **)0x32eb94) as saved_game_perform_file_system_checks
 * above, capped at 100 records (CMP dword ptr [EBP-4],0x64 at 0x1c50a1).
 * For each match: try "<directory>blam.sav" first (kind 0, checksum covers
 * the leading 0x30 bytes of the 0x200-byte file); if that is not a real
 * file, try "<directory>blam.lst" (kind 1, checksum covers the leading
 * 0x68 bytes); if neither is a real file, log "random crap found by
 * XFindNextSaveGame(): display name= '%s' path= '%hs'" (wide display name
 * at find_data+0x244, narrow path at find_data+0x2c), mark kind -1, and
 * skip straight to the next XFindNextSaveGame call -- no
 * enumerate_saved_game_file call and no count increment for that record
 * (0x1c527e JMP 0x1c5374).
 *
 * Checksum verification only runs when a real file was found.  It signs
 * the file's leading `checksum_size` bytes and compares against the
 * stored 0x14-byte signature at read_buf+checksum_size
 * (saved_game_file_generate_checksum + csmemcmp, same idiom as the sibling
 * enumerators).  On mismatch it logs "checksum validation failed for
 * '%s'" and falls back to the old-style 4-byte CRC at the same offset
 * (crc_new/crc_checksum_buffer/csmemcmp); if that matches, it logs
 * "checksum validation matched old-style crc; updating saved game file
 * '%s'", overwrites the stored signature in place with the new one
 * (csmemcpy), rewrites the file (file_set_position(0) + file_write), and
 * only then sets checksum_valid.  A checksum failure with no CRC match
 * leaves checksum_valid 0, but the record is still enumerated --
 * enumerate_saved_game_file(entry) always runs once a real file was
 * found, regardless of open/read/checksum outcome (a file_open failure
 * skips file_read/file_close and the whole checksum block; a file_read
 * failure still runs file_close).  If enumerate_saved_game_file itself
 * returns false, the whole enumeration loop stops immediately (0x1c536f
 * JZ 0x1c5389, no further XFindNextSaveGame call).
 *
 * After the loop (or immediately, if XFindFirstSaveGame returned -1):
 * enumerate_default_playlist_profiles() then
 * enumerate_default_player_profiles() run in that order (0x1c53a5/
 * 0x1c53ac) and their 16-bit sum is added into the same counter the
 * enumeration loop above uses (MOVSX ECX,AX / ADD [EBP-4],ECX at
 * 0x1c53b3/0x1c53b6) -- the result is never read again, but both calls'
 * side effects (they each call enumerate_saved_game_file in turn) still
 * have to happen. */
void FUN_001c5010(void)
{
  int16_t unit;
  int count;
  char root_path[8] = "";
  char find_data[XGAME_FIND_DATA_SIZE];
  int find_handle;
  int n;
  bool have_record;
  int checksum_size;
  char read_buf[0x200];
  char signature[0x14];
  char *checksum_slot;
  uint32_t old_crc;
  file_ref_t info;
  wchar_t error_msg[0x100];
  char *ascii_msg;
  int16_t playlist_count;
  int16_t player_count;

  unit = 0;
  count = 0;

  for (; unit == 0; unit = (int16_t)(unit + 1)) {
    if (!take_mutex(*(int **)0x4eacbc, 3600000))
      continue;

    if (enumerate_saved_game_files_start(unit)) {
      if (unit == 0) {
        find_handle = XFindFirstSaveGame(
          wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8), find_data);

        if (find_handle != -1) {
          while (count < 100) {
            char entry[0x206] = "";

            have_record = 0;

            n = snprintf(entry, 0xff, "%s%s", find_data + 0x140, "blam.sav");
            if (n > 0 &&
                file_reference_create_from_path(&info, entry, 0) != NULL &&
                file_exists(&info)) {
              *(int16_t *)(entry + 0x200) = 0;
              checksum_size = 0x30;
              have_record = 1;
            } else {
              n = snprintf(entry, 0xff, "%s%s", find_data + 0x140, "blam.lst");
              if (n > 0 &&
                  file_reference_create_from_path(&info, entry, 0) != NULL &&
                  file_exists(&info)) {
                *(int16_t *)(entry + 0x200) = 1;
                checksum_size = 0x68;
                have_record = 1;
              } else {
                unicode_sprintf(
                  error_msg, 0xff,
                  L"random crap found by XFindNextSaveGame(): display name= "
                  L"'%s' path= '%hs'",
                  (wchar_t *)(find_data + 0x244), find_data + 0x2c);
                error_msg[0xff] = 0;
                ascii_msg = wide_to_ascii(error_msg, (char *)error_msg, 0x200);
                error(2, ascii_msg);
                *(int16_t *)(entry + 0x200) = -1;
              }
            }

            if (have_record) {
              ustrncpy((wchar_t *)(entry + 0x100),
                       (wchar_t *)(find_data + 0x244), 0x7f);
              *(wchar_t *)(entry + 0x1fe) = L'\0';

              if (file_open(&info, 3)) {
                if (file_read(&info, 0x200, read_buf)) {
                  saved_game_file_generate_checksum(read_buf, checksum_size,
                                                    signature);
                  checksum_slot = read_buf + checksum_size;

                  if (csmemcmp(signature, checksum_slot, 0x14) == 0) {
                    entry[0x205] = 1;
                  } else {
                    error(2, "checksum validation failed for '%s'", entry);

                    crc_new(&old_crc);
                    crc_checksum_buffer(&old_crc, read_buf, checksum_size);

                    if (csmemcmp(&old_crc, checksum_slot, 4) == 0) {
                      error(2,
                            "checksum validation matched old-style crc; "
                            "updating saved game file '%s'",
                            entry);
                      csmemcpy(checksum_slot, signature, 0x14);

                      if (file_set_position(&info, 0) &&
                          file_write(&info, 0x200, read_buf)) {
                        entry[0x205] = 1;
                      }
                    }
                  }
                } else {
                  error(2, "failed to read saved game file to verify checksum");
                }

                if (!file_close(&info))
                  error(2, "failed to close saved game file after verifying "
                           "checksum");
              } else {
                error(2, "failed to open saved game file to verify checksum");
              }

              if (!enumerate_saved_game_file(entry))
                break;

              count++;
            }

            if (!XFindNextSaveGame(find_handle, find_data))
              break;
          }

          if (!XFindClose(find_handle))
            error(2, "XFindClose() failed");
        }

        playlist_count = enumerate_default_playlist_profiles();
        player_count = enumerate_default_player_profiles();
        count += (int16_t)(player_count + playlist_count);
      }

      enumerate_saved_game_files_end(unit);
    }

    release_mutex(*(int **)0x4eacbc);
  }

  *(uint8_t *)0x4eacc7 = 0;
}

/* saved_game_files_enumerate_available_to_local_player_index (0x1c53f0) —
 * inferred names; assert line 0xec.  Fills
 * player_profile_indices with the saved-game-file indices of every hard-drive
 * mapfile entry whose type (record+0x200) equals saved_game_file_type,
 * skipping read-only entries (record+0x204) unless include_default_profiles
 * is exactly 1, and stops at the capacity passed in the low word of
 * *number_of_profiles.  The found count is written back as a word.
 *
 * Register-arg callees all receive the hard-drive memory unit 0
 * (XOR ESI,ESI at 0x1c546e/0x1c54f8, XOR EAX,EAX at 0x1c5492); the record
 * buffer goes to enumerate_saved_game_file_from_mapfile in ESI (LEA ESI,
 * [EBP-0x20c]); build_saved_game_file_index gets entry index in EAX, 0 in ECX
 * and the zero-extended file type still live in EDX from 0x1c54bc.
 * The mapfile entry count from count_enumerated_profiles_in_mapfile is kept
 * in the player_index argument slot ([EBP+8]) and compared signed.
 *
 * When the general mutex cannot be taken the binary stores the low word of
 * player_index (MOV DX,[EBP+8] at 0x1c5546, before that slot is reused) into
 * *number_of_profiles, not 0; reproduced as-is. */
void saved_game_files_enumerate_available_to_local_player_index(
  int player_index, int saved_game_file_type, int *number_of_profiles,
  int *player_profile_indices, int include_default_profiles)
{
  char record[0x206];
  int number_of_entries;
  int number_of_available_profiles;
  int entry_index;

  if (!(((int16_t)player_index == -1 ||
         ((int16_t)player_index >= 0 && (int16_t)player_index < 4)) &&
        (uint16_t)saved_game_file_type < 2 && number_of_profiles != NULL &&
        player_profile_indices != NULL)) {
    display_assert(
      "((player_index==NONE) || ((player_index>=0) && "
      "(player_index<MAXIMUM_GAMEPADS))) && "
      "(saved_game_file_type<NUMBER_OF_SAVED_GAME_FILE_TYPES) && "
      "(number_of_profiles != NULL) && (player_profile_indices != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0xec, 1);
    system_exit(-1);
  }

  if (!take_mutex(*(int **)0x4eacbc /* general_mutex */, 3600000)) {
    error(2, "failed to take saved game files mutex");
    *(uint16_t *)number_of_profiles = (uint16_t)player_index;
    return;
  }

  if (*(uint8_t *)0x4eacc7 != 0) { /* memory_units_dirty */
    FUN_001c5010();
  }

  number_of_available_profiles = 0;
  number_of_entries = (int)count_enumerated_profiles_in_mapfile(0);
  if (take_mutex(*(int **)0x4eacc0 /* mapfile_mutex */, 3600000)) {
    if (enumerate_mapfile_start(0)) {
      entry_index = 0;
      if (*(uint16_t *)number_of_profiles != 0) {
        do {
          if (entry_index >= number_of_entries) {
            break;
          }
          if (!enumerate_saved_game_file_from_mapfile(record)) {
            break;
          }
          if ((int)*(int16_t *)(record + 0x200) ==
                (int)(uint16_t)saved_game_file_type &&
              ((char)include_default_profiles == 1 || record[0x204] == 0)) {
            player_profile_indices[number_of_available_profiles] =
              (int)build_saved_game_file_index(
                entry_index, 0, (int32_t)(uint16_t)saved_game_file_type,
                (bool)record[0x204], (bool)record[0x205]);
            number_of_available_profiles++;
          }
          entry_index++;
        } while (number_of_available_profiles <
                 (int)*(uint16_t *)number_of_profiles);
      }
      enumerate_mapfile_end(0);
    }
    release_mutex(*(int **)0x4eacc0);
  } else {
    error(2, "failed to take mapfile mutex");
  }

  release_mutex(*(int **)0x4eacbc);
  *(uint16_t *)number_of_profiles = (uint16_t)number_of_available_profiles;
}

/* 0x1c5560 — create_enumerated_saved_game_file.  Name confirmed by its own
 * error string "file_close() failed in create_enumerated_saved_game_file()".
 * Returns the packed saved game file index of the new file, or -1.
 *
 * Parameters are read as MOV BX,[EBP+8] (unsigned compare, JNC, against 2),
 * MOV AX,[EBP+0xc] (-1 or signed < 4) and [EBP+0x10] (non-NULL); the three
 * checks share one assert at line 0x14c.  A dirty memory-unit flag
 * (0x4eacc7) re-enumerates through FUN_001c5010.  The file-system check
 * result is switched on: 1 -> display_error_abort_to_dashboard_deferred(0x21,
 * 1), 2 -> (0x22, 1); any non-zero result returns -1.
 *
 * Frame (SUB ESP,0x620): 0x200-byte blank block at EBP-0x620, file_ref_t at
 * EBP-0x420, save-game directory [0x100] at EBP-0x314, the mapfile record at
 * EBP-0x214 and the 8-byte root path at EBP-0x8.  XCreateSaveGame (0x1d2f22,
 * __stdcall) gets (root_path, display_name, 1, 0, directory, 0x100); a
 * non-zero result means failure.  Type 0 builds "<dir>blam.sav" with a
 * 0x30-byte checksum span and calls FUN_001c0cd0(dir) (inferred
 * name game_state_create_persistent_storage), type 1 builds "<dir>blam.lst" with
 * a 0x68-byte span; any other type stores -1 into record.type and goes
 * straight to the XDeleteSaveGame cleanup.  append_entry_to_mapfile gets
 * unit 0 in EAX and writes the new entry index through its out pointer (the
 * original reuses the [EBP+8] argument slot for it).  On success the result
 * is build_saved_game_file_index(index in EAX, unit 0 in ECX, type in EDX,
 * read_only, valid). */
int32_t create_enumerated_saved_game_file(uint16_t saved_game_file_type,
                                          int16_t local_player_index,
                                          wchar_t *display_name)
{
  uint8_t block[0x200];
  file_ref_t saved_game_file;
  int32_t new_profile_index;
  int16_t file_system_check;

  new_profile_index = -1;

  if (!(saved_game_file_type < 2 &&
        (local_player_index == -1 || local_player_index < 4) &&
        display_name != NULL)) {
    display_assert(
      "(saved_game_file_type<NUMBER_OF_SAVED_GAME_FILE_TYPES) && "
      "((local_player_index==NONE) || "
      "(local_player_index<MAXIMUM_GAMEPADS)) && (display_name != NULL)",
      "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x14c, true);
    system_exit(-1);
  }

  if (*(uint8_t *)0x4eacc7 != 0) { /* memory_units_dirty */
    FUN_001c5010();
  }

  file_system_check = saved_game_perform_file_system_checks();

  switch (file_system_check) {
  case 1: /* out of disk space */
    display_error_abort_to_dashboard_deferred(0x21, 1);
    break;
  case 2: /* too many saved games */
    display_error_abort_to_dashboard_deferred(0x22, 1);
    break;
  }

  if (file_system_check == 0) {
    int32_t number_of_entries =
      (int32_t)count_enumerated_profiles_in_mapfile(0);

    if (number_of_entries < 100) {
      char root_path[8] = { 0 };
      char save_game_directory[0x100] = { 0 };

      if (FUN_001d2f22(wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
                       display_name, 1, 0, save_game_directory, 0x100) == 0) {
        enumerated_saved_game_file_t file = { 0 };
        uint32_t profile_index;
        uint16_t checksum_data_size;

        ustrncpy(file.display_name, display_name, 0x7f);
        file.type = (int16_t)saved_game_file_type;
        file.display_name[0x7f] = 0;
        file.index = (int16_t)number_of_entries;
        file.read_only = false;
        file.valid = false;

        switch (saved_game_file_type) {
        case 0:
          snprintf(file.path, 0xff, "%s%s", save_game_directory, "blam.sav");
          checksum_data_size = 0x30;
          FUN_001c0cd0((int)save_game_directory);
          break;
        case 1:
          snprintf(file.path, 0xff, "%s%s", save_game_directory, "blam.lst");
          checksum_data_size = 0x68;
          break;
        default:
          file.type = -1;
          break;
        }

        if (file.type != -1) {
          if (file_reference_create_from_path(&saved_game_file, file.path, 0) !=
                NULL &&
              file_create(&saved_game_file)) {
            if (file_open(&saved_game_file, 2)) {
              csmemset(block, 0, sizeof(block));
              saved_game_file_generate_checksum(block, checksum_data_size,
                                                block + checksum_data_size);

              if (file_write(&saved_game_file, sizeof(block), block)) {
                file.valid = true;
              }

              if (!file_close(&saved_game_file)) {
                error(2, "file_close() failed in "
                         "create_enumerated_saved_game_file()");
              }
            } else {
              error(2, "failed to write blank saved game file block to disk");
            }

            if (append_entry_to_mapfile(0, &file, &profile_index)) {
              if ((int32_t)profile_index != file.index) {
                display_assert(
                  "profile_index == file.index",
                  "c:\\halo\\SOURCE\\saved games\\saved_game_files.c", 0x1b2,
                  true);
                system_exit(-1);
              }

              return (int32_t)build_saved_game_file_index(
                file.index, 0, saved_game_file_type, file.read_only,
                file.valid);
            }

            error(2, "append_entry_to_mapfile() failed; deleting newly "
                     "created meta data");
          } else {
            error(2, "failed to create empty saved game file '%s'", file.path);
          }
        }

        if (FUN_001d3185(
              wide_to_ascii(*(const wchar_t **)0x32eb94, root_path, 8),
              display_name) != 0) {
          error(2, "XDeleteSaveGame() failed... ghost meta data likely");
        }

        return -1;
      }

      error(2, "XCreateSaveGame() failed to create meta data for a new saved "
               "game file");
      return -1;
    }

    error(2, "failed to create new saved game file because there are already "
             "the maximum number of game files on the hard drive");
    display_error_deferred(0x24, -1, 1, 0);
  }

  return new_profile_index;
}
