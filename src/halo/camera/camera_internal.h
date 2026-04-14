/* Camera-internal register-arg shims.
 *
 * The callees below live in the unported portion of the original XBE and use
 * non-standard calling conventions: one or two arguments are passed in CPU
 * registers (EAX/AX/AL/ESI) rather than on the stack. Until those functions
 * are ported (or the reverse-thunk codegen lands in knowledge.py), the only
 * safe way to reach them from C is inline asm that pins the register arg and
 * performs the CALL inside the same asm block.
 *
 * Keeping the asm here (rather than at the director.c call sites) lets the
 * director read as plain C. When a callee is ported, its helper body becomes
 * a normal cdecl call and the asm disappears one shim at a time. */
#ifndef HALO_CAMERA_INTERNAL_H
#define HALO_CAMERA_INTERNAL_H

/* 0x86de0  mode-0/1 camera set — now ported. */
static inline void camera_internal_set_mode_0_1(int16_t player, int reset,
                                                int mode_flags)
{
  director_set_player_camera_normal(player, (char)reset, (char)mode_flags);
}

/* 0x86fa0  mode-2 camera set — now ported. mode_flags arg is unused by the
 * scripted-camera setter (only reset_flag matters). */
static inline void camera_internal_set_mode_2(int16_t player, int reset,
                                              int mode_flags)
{
  (void)mode_flags;
  director_set_player_camera_scripted(player, (char)reset);
}

/* 0x87050  mode-4 camera set — now ported. */
static inline void camera_internal_set_mode_4(int reset_flag, int16_t player,
                                              int mode_flags)
{
  director_apply_replay_mode_for_player((char)reset_flag, player,
                                        (char)mode_flags);
}

/* 0x86600  per-player data init — now a real C function. The shim stays as
 * a thin forwarder so director.c call sites don't change when more helpers
 * get ported. */
static inline void camera_internal_init_player(int16_t player)
{
  director_init_player_cameras(player);
}

/* 0x865a0  install camera fn for player (@si=player, cdecl: fn, reset_byte) */
static inline void camera_internal_set_camera_fn(int16_t player, void *camera_fn,
                                                 char reset_top_timer)
{
  asm volatile("pushl %[r]\n\t"
               "pushl %[fn]\n\t"
               "movl $0x865a0, %%eax\n\t"
               "call *%%eax\n\t"
               "addl $8, %%esp"
               :
               : "S"(player), [fn] "r"(camera_fn), [r] "r"((int)reset_top_timer)
               : "eax", "ecx", "edx", "memory", "cc");
}

/* 0x86a50  cycle camera mode (@eax=player, @ebx=mode_table_imm, cdecl: count).
 * mode_table and count are baked as immediates because Clang on i386 will
 * not allocate EBX as a general scratch (it's reserved for PIC), so we
 * cannot use "r" or "b" input constraints — only literals work. */
#define CAMERA_INTERNAL_CYCLE_MODE(player, mode_table_addr, count_imm)        \
  do {                                                                        \
    int _player = (player);                                                   \
    asm volatile("pushl $" #count_imm "\n\t"                                  \
                 "movl $" #mode_table_addr ", %%ebx\n\t"                      \
                 "movl $0x86a50, %%ecx\n\t"                                   \
                 "call *%%ecx\n\t"                                            \
                 "addl $4, %%esp"                                             \
                 : "+a"(_player)                                              \
                 :                                                            \
                 : "ebx", "ecx", "edx", "memory", "cc");                      \
  } while (0)

/* 0x86be0  re-evaluate camera state (@eax=player, @bl=force_flag, no stack).
 * 0x86be0 reads only BL, so loading the full 32-bit value into EBX is fine
 * — the high bytes are ignored. Avoids a "Q" byte-register constraint that
 * would conflict with EAX/ECX/EDX already being in use. */
static inline void camera_internal_reevaluate(int16_t player, char force_flag)
{
  int _player = player;
  asm volatile("movl %[f], %%ebx\n\t"
               "movl $0x86be0, %%ecx\n\t"
               "call *%%ecx"
               : "+a"(_player)
               : [f] "r"((int)force_flag)
               : "ebx", "ecx", "edx", "memory", "cc");
}

/* 0x87110  build camera input   (@eax=out_buf, cdecl: player) -> bool */
static inline uint8_t camera_internal_poll_input(void *out_buf, int player)
{
  void *_eax = out_buf;
  asm volatile("pushl %[pi]\n\t"
               "movl $0x87110, %%ecx\n\t"
               "call *%%ecx\n\t"
               "addl $4, %%esp"
               : "+a"(_eax)
               : [pi] "r"(player)
               : "ecx", "edx", "memory", "cc");
  return (uint8_t)(uintptr_t)_eax;
}

#endif
