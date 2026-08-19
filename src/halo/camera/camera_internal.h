/* Camera-internal register-arg shims.
 *
 * The callees below live in the unported portion of the original XBE and use
 * non-standard calling conventions: one or two arguments are passed in CPU
 * registers (EAX/AX/BL/ESI/EBX) rather than on the stack.
 *
 * These are reached by declaring the register positions in kb.json with
 * `@<reg>` annotations and calling the callee by name -- tools/build/patch.py
 * marshals the stack args into the right registers in a generated thunk. That
 * is the project's sanctioned mechanism (CLAUDE.md: "Register-arg callees must
 * be added to kb.json with @<reg> and called by name"; "No Inline ASM").
 *
 * These shims previously used GCC-style `asm volatile`, which cl.exe 13.10
 * cannot parse, so this whole translation unit failed to compile under VC71
 * and its 13 functions were never byte-scored. The asm was also unsound: an
 * empty `asm volatile("" : "+a"(x))` followed by a separate call does not
 * guarantee the value survives in EAX to the call instruction. */
#ifndef HALO_CAMERA_INTERNAL_H
#define HALO_CAMERA_INTERNAL_H

/* 0x86de0  mode-0/1 camera set — now ported. */
static __inline void camera_internal_set_mode_0_1(int16_t player, int reset,
                                                  int mode_flags)
{
  director_set_player_camera_normal(player, (char)reset, (char)mode_flags);
}

/* 0x86fa0  mode-2 camera set — now ported. mode_flags arg is unused by the
 * scripted-camera setter (only reset_flag matters). */
static __inline void camera_internal_set_mode_2(int16_t player, int reset,
                                                int mode_flags)
{
  (void)mode_flags;
  director_set_player_camera_scripted(player, (char)reset);
}

/* 0x87050  mode-4 camera set — now ported. */
static __inline void camera_internal_set_mode_4(int reset_flag, int16_t player,
                                                int mode_flags)
{
  director_apply_replay_mode_for_player((char)reset_flag, player,
                                        (char)mode_flags);
}

/* 0x86600  per-player data init — now a real C function. The shim stays as
 * a thin forwarder so director.c call sites don't change when more helpers
 * get ported. */
static __inline void camera_internal_init_player(int16_t player)
{
  director_init_player_cameras(player);
}

/* 0x865a0  install camera fn for player (@si=player, cdecl: fn, reset_byte) */
static __inline void camera_internal_set_camera_fn(int16_t player,
                                                   void *camera_fn,
                                                   char reset_top_timer)
{
  FUN_000865a0(player, (int)camera_fn, reset_top_timer != 0);
}

/* 0x86a50  cycle camera mode (@eax=player, @ebx=mode_table, cdecl: count).
 * 0x86a50 reads the table as an int16_t array (`movswl (%ebx,%edx,2)`) and
 * the count as a 16-bit signed stack arg (`movswl 0x8(%ebp),%ecx`). */
static __inline void camera_internal_cycle_mode(int player, int16_t *mode_table,
                                                int16_t count)
{
  FUN_00086a50(player, mode_table, count);
}

/* 0x86be0  re-evaluate camera state (@eax=player, @bl=force_flag, no stack).
 * 0x86be0 only ever reads BL (five `testb %bl,%bl`, never written first), so
 * the annotation is @<bl> and the generated thunk loads the byte. */
static __inline void camera_internal_reevaluate(int16_t player, char force_flag)
{
  FUN_00086be0(player, force_flag);
}

/* 0x86670  per-player look/walk integrator (@ax=player, cdecl: mode_flags
 * byte, fwd float). Updates per-player camera-state floats; large helper
 * not yet ported. */
static __inline void camera_internal_integrate(int16_t player,
                                               uint8_t mode_flags, float fwd)
{
  FUN_00086670(player, mode_flags, fwd);
}

/* 0x87110  build camera input — now ported. */
static __inline uint8_t camera_internal_poll_input(void *out_buf, int player)
{
  return (uint8_t)director_compute_camera_input((short *)out_buf, player);
}

#endif
