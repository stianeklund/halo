/* RNG draw trace -- DIAGNOSTIC INSTRUMENTATION, NOT recovered game code.
 *
 * Compiled only when HALO_RNG_TRACE is defined (tools/build/build.py
 * --rng-trace).  Used to find which lifted function desyncs a system-link game
 * against a pristine build-2276 instance: both sides record every draw from the
 * GLOBAL random seed into a ring buffer, and tools/xbox/rng_trace_dump.py
 * diffs the two traces.  See docs/rng-trace.md.
 *
 * Every source file that includes this header inserts its instrumentation
 * behind a `#line` directive that restores the original physical line
 * numbering: assert_halt() stamps __LINE__ into the binary, so an unrestored
 * line shift would change codegen (and the VC71 score) of every function below
 * the insertion point.  With the flag off the emitted objects are
 * byte-identical to the uninstrumented build.
 */
#ifndef HALO_MATH_RNG_TRACE_H
#define HALO_MATH_RNG_TRACE_H

#define RNG_TRACE_MAGIC             0x54474e52u /* 'R','N','G','T' little-endian */
#define RNG_TRACE_VERSION           1u
#define RNG_TRACE_CAPACITY          65536u      /* must stay a power of two */
#define RNG_TRACE_GLOBAL_SEED_ADDR  0x46e3f4u   /* global seed; 0x46e3f8 is local */

/* Event kind, packed into the top 8 bits of rng_trace_record_t.tick.
 * The trailing "steps" count is how many LCG iterations the event advances the
 * global seed; rng_trace_dump.py uses it to verify draw-sequence continuity
 * (seed[n+1] == lcg(seed[n]) applied `steps` times). */
#define RNG_TRACE_KIND_REAL          0u /* random_math_real            steps 1 */
#define RNG_TRACE_KIND_REAL_RANGE    1u /* random_real_range           steps 1 */
#define RNG_TRACE_KIND_SEED_STEP     2u /* random_seed_step            steps 1 */
#define RNG_TRACE_KIND_RANGE         3u /* seed_random_range                steps 1 */
#define RNG_TRACE_KIND_DIRECTION3D   4u /* random_seed_get_direction3d steps 1 */
#define RNG_TRACE_KIND_ORIENTATION   5u /* seed_random_orientation     steps 3 */
#define RNG_TRACE_KIND_DIR3D_INLINE  6u /* random_direction3d          steps 1 */
#define RNG_TRACE_KIND_SET_SEED      7u /* set_random_seed             reseed   */
#define RNG_TRACE_KIND_NET_SET_SEED  8u /* network_game_set_random_seed  info   */
#define RNG_TRACE_KIND_MAP_SEED      9u /* game_initialize_for_new_map   reseed */
#define RNG_TRACE_KIND_PERIODIC_SEED 10u /* periodic_functions_initialize reseed */
/* Damage-path probes (value = float bits, extra = object handle).  Not seed
 * events: the dump tool treats every "probe:" kind as informational. */
#define RNG_TRACE_KIND_DAMAGE_SCALE  11u /* object_cause_damage scale        info */
#define RNG_TRACE_KIND_BODY_BEFORE   12u /* root object body vitality, entry info */
#define RNG_TRACE_KIND_BODY_AFTER    13u /* FUN_00136f40 body vitality       info */
#define RNG_TRACE_KIND_SHIELD_AFTER  14u /* FUN_00136f40 shield vitality     info */
#define RNG_TRACE_KIND_ANIM_CHOOSE   15u /* model_animation_choose_random: value=anim index, caller2=its caller  info */
#define RNG_TRACE_KIND_UNIT_STATE    16u /* unit_animation_set_state entry: value=(anim_state<<8)|old_state, caller=its caller, caller2=unit handle  info */
#define RNG_TRACE_KIND_ANIM_UPDATE_IN 17u /* unit_animation_update before original updater: value=state[1]<<16|state[0], caller=unit_update_animation site, caller2=unit handle  info */
#define RNG_TRACE_KIND_ANIM_UPDATE_OUT 18u /* unit_animation_update after original updater: value=state[0]<<16|result, caller=unit_update_animation site, caller2=unit handle  info */
/* Turn-in-place fork gates in FUN_001a4c50, which is unported on BOTH builds.
 * Sampled in its ported caller (biped_update) rather than inside the fork: the
 * fork performs no write to any of these four fields before the gate chain at
 * 0x1a5109, so the caller's read is the same value the gates see.  The pristine
 * host gets the identical record from a binary probe at 0x1a5109
 * (artifacts/rng_trace/build_original_probes.py).
 *   bits 0-7   +0x42a  animation mode  (gates at 0x1a5109 / 0x1a5120)
 *   bits 8-15  +0x257  (drives [ebp-2] and the early exit at 0x1a4f65)
 *   bit  16    +0x1b4 & 0x4000         (gate at 0x1a513c)
 *   bit  17    +0x1b8 & 0x100          (gate at 0x1a514b) */
#define RNG_TRACE_KIND_TURN_GATES    19u /* value=packed gates, caller2=unit handle  info */
#define RNG_TRACE_KIND_TURN_COSINE   20u /* raw float bits of the fcomp operand at 0x1a5169  info */
/* Client-side reconstruction of the fork's compared value, from the ported
 * caller.  FUN_001a4c50 computes, at 0x1a5061..0x1a50cd:
 *     d = (+0x1d4, +0x1d8, 0), normalized
 *     cos = d.x * f.x + d.y * f.y     with f = +0x24, used RAW
 * f is neither flattened nor renormalized, so a non-zero f.z lowers cos even
 * when the two vectors point the same way in the XY plane.  Kind 22 carries
 * f.z for that reason. */
#define RNG_TRACE_KIND_TURN_COS_C    21u /* raw float bits of the reconstructed cosine  info */
#define RNG_TRACE_KIND_TURN_FWD_Z    22u /* raw float bits of the current facing z (+0x2c)  info */
/* Components behind kind 21, to separate "the reconstruction is wrong" from
 * "our simulation is wrong".  Kind 23 is the squared XY length of the desired
 * facing BEFORE normalization: a value at or near zero means the degenerate
 * fallback fired and kind 21's 1.0 is an artifact, not a measurement. */
#define RNG_TRACE_KIND_TURN_DES_LEN2 23u /* |desired facing XY|^2 before normalize  info */
#define RNG_TRACE_KIND_TURN_DES_X    24u /* raw bits of +0x1d4  info */
#define RNG_TRACE_KIND_TURN_DES_Y    25u /* raw bits of +0x1d8  info */
#define RNG_TRACE_KIND_TURN_FWD_X    26u /* raw bits of +0x24   info */
#define RNG_TRACE_KIND_TURN_FWD_Y    27u /* raw bits of +0x28   info */
/* Kinds 21-27 are RETIRED: they belonged to the caller-side reconstruction of
 * the fork's cosine, which reads +0x24 one update too early (the fork rewrites
 * it at 0x1a4f0e).  The kept definitions only stop the numbers being reused,
 * so old captures stay readable.
 *
 * Kinds 28-30 replace them.  They do NOT reconstruct anything: they report the
 * two vectors the fork actually dots, plus the flag word that selects which
 * function last wrote the desired facing.  Paired capture proved our cosine is
 * a bit-exact constant (self-dot) while the pristine host sweeps a real turn,
 * so unit+0x1d4 equals unit+0x24 on our build.  Three candidate writers remain
 * (unit_set_control's producer, unit_update's static arm, players.c's
 * input-disabled arm) and +0x1b4 bit 0 picks the second of them. */
#define RNG_TRACE_KIND_DESIRED_X     28u /* raw bits of unit+0x1d4  info */
#define RNG_TRACE_KIND_CURRENT_X     29u /* raw bits of unit+0x24   info */
#define RNG_TRACE_KIND_UNIT_FLAGS    30u /* full dword unit+0x1b4   info */
/* object_cause_damage entry on BOTH builds: value = damage effect tag index
 * (damage_params[0]), caller2 = object handle.  Host mirror is a binary
 * detour at 0x137d20 (build_original_probes.py).  Names every object the
 * damage pass touches, so a differing candidate set is visible. */
#define RNG_TRACE_KIND_DAMAGE_TARGET 31u /* value=jpt! tag, caller2=object handle  info */
/* object_find_in_radius accept branch on BOTH builds: value = found_count
 * before the store, caller2 = accepted object handle.  Host mirror is a
 * binary detour at 0x141793.  Separates "the radius query returned a
 * different candidate set" from "the unported applier rejected it". */
#define RNG_TRACE_KIND_RADIUS_HIT    32u /* object_find_in_radius accept: value=object position z bits (obj+0x58), caller2=object handle  info */
#define RNG_TRACE_KIND_LOS_RESULT    33u /* FUN_0014df70 exit, only when returning into FUN_00138900 or the projectile sweep projectile_collision_test_line (value bit 31 set): value=(result<<16)|(uint16)collision_result[0], caller2=hit-t float bits  info */
#define RNG_TRACE_KIND_DAMAGE_ORIGIN 34u /* FUN_00138e30 entry: value=origin.x bits (+0x28), caller2=origin.y bits (+0x2c)  info */
#define RNG_TRACE_KIND_SWEEP_POS     36u /* projectile_collision_test_line entry: value=new_pos.x bits, caller2=new_pos.z bits  info */
#define RNG_TRACE_KIND_PROJECTILE_ACCEL_X 37u /* projectile_accelerate entry: value=projectile handle, caller2=acceleration.x bits  info */
#define RNG_TRACE_KIND_PROJECTILE_ACCEL_YZ 38u /* projectile_accelerate entry: value=acceleration.y bits, caller2=acceleration.z bits  info */
#define RNG_TRACE_KIND_SWEEP_NEW_Y_HANDLE 39u /* projectile_collision_test_line entry: value=new_pos.y bits, caller2=projectile handle  info */
#define RNG_TRACE_KIND_SWEEP_POS_XY 40u /* projectile_collision_test_line: value=proj_pos.x bits, caller2=proj_pos.y bits  info */
#define RNG_TRACE_KIND_SWEEP_POS_Z_VEL_X 41u /* projectile_collision_test_line: value=proj_pos.z bits, caller2=velocity.x bits  info */
#define RNG_TRACE_KIND_SWEEP_VEL_YZ 42u /* projectile_collision_test_line: value=velocity.y bits, caller2=velocity.z bits  info */
#define RNG_TRACE_KIND_NET_UPDATE_FLAGS 43u /* client game update: value=flags, caller2=local player count  info */
#define RNG_TRACE_KIND_NET_UPDATE_BUTTONS_01 44u /* client game update: value=slot 0 buttons, caller2=slot 1 buttons  info */
#define RNG_TRACE_KIND_NET_UPDATE_BUTTONS_23 45u /* client game update: value=slot 2 buttons, caller2=slot 3 buttons  info */
#define RNG_TRACE_KIND_THROW_UNIT_XY 46u /* grenade release: value=thrower position.x bits, caller2=position.y bits  info */
#define RNG_TRACE_KIND_THROW_UNIT_Z_HANDLE 47u /* grenade release: value=thrower position.z bits, caller2=unit handle  info */
#define RNG_TRACE_KIND_THROW_SEAT_XY 48u /* grenade release after unit_set_seat_state: value=seat x bits, caller2=seat y bits  info */
#define RNG_TRACE_KIND_THROW_SEAT_Z_HANDLE 49u /* grenade release after unit_set_seat_state: value=seat z bits, caller2=unit handle  info */
#define RNG_TRACE_KIND_THROW_FINAL_XY 50u /* grenade release before object_translate: value=target x bits, caller2=target y bits  info */
#define RNG_TRACE_KIND_THROW_FINAL_Z_HANDLE 51u /* grenade release before object_translate: value=target z bits, caller2=unit handle  info */
#define RNG_TRACE_KIND_RESPONSE_HIT_XY 52u /* projectile_collision entry: hit position x/y bits  info */
#define RNG_TRACE_KIND_RESPONSE_HIT_Z_HANDLE 53u /* entry: hit position z bits/projectile handle  info */
#define RNG_TRACE_KIND_RESPONSE_VEL_XY 54u /* entry: incoming velocity x/y bits  info */
#define RNG_TRACE_KIND_RESPONSE_VEL_Z_TYPE 55u /* entry: incoming velocity z bits/collision type  info */
#define RNG_TRACE_KIND_RESPONSE_NORMAL_XY 56u /* entry: collision normal x/y bits  info */
#define RNG_TRACE_KIND_RESPONSE_NORMAL_Z_T 57u /* entry: collision normal z/hit fraction bits  info */
#define RNG_TRACE_KIND_RESPONSE_OBJECT_META 58u /* entry: collided object handle/raw result +0x34  info */
#define RNG_TRACE_KIND_TRY_PLACE_IN_POS_XY 59u /* object_try_place entry: object position x/y bits  info */
#define RNG_TRACE_KIND_TRY_PLACE_IN_POS_Z_HANDLE 60u /* entry: object position z bits/object handle  info */
#define RNG_TRACE_KIND_TRY_PLACE_TARGET_XY 61u /* entry: requested target x/y bits  info */
#define RNG_TRACE_KIND_TRY_PLACE_TARGET_Z_HANDLE 62u /* entry: requested target z bits/object handle  info */
#define RNG_TRACE_KIND_TRY_PLACE_OUT_POS_XY 63u /* object_try_place exit: object position x/y bits  info */
#define RNG_TRACE_KIND_TRY_PLACE_OUT_POS_Z_RESULT 64u /* exit: object position z bits/result  info */
#define RNG_TRACE_KIND_TRY_PLACE_COLLISION_TYPE_T 65u /* exit: collision type/hit fraction bits  info */
#define RNG_TRACE_KIND_TRY_PLACE_COLLISION_OBJECT_SURFACE 66u /* exit: collided object handle/surface index  info */
#define RNG_TRACE_KIND_LOCAL_RAY_ORIGIN_XY 67u
#define RNG_TRACE_KIND_LOCAL_RAY_ORIGIN_Z_DIR_X 68u
#define RNG_TRACE_KIND_LOCAL_RAY_DIR_YZ 69u
#define RNG_TRACE_KIND_LOCAL_RAY_SOURCE_TRANSLATION_XY 70u
#define RNG_TRACE_KIND_LOCAL_RAY_SOURCE_TRANSLATION_Z_SCALE 71u
#define RNG_TRACE_KIND_LOCAL_RAY_INVERSE_TRANSLATION_XY 72u
#define RNG_TRACE_KIND_LOCAL_RAY_INVERSE_TRANSLATION_Z_SCALE 73u
#define RNG_TRACE_KIND_LOCAL_RAY_MATRIX_HASHES 74u
#define RNG_TRACE_KIND_LOCAL_RAY_OBJECT_NODE 75u
#define RNG_TRACE_KIND_LOCAL_RAY_ANIMATION_STATE 76u
#define RNG_TRACE_KIND_LOCAL_RAY_ROOT_POSE_HASHES 77u
#define RNG_TRACE_KIND_LOCAL_RAY_NODE7_POSE_QUATERNION_XY 78u
#define RNG_TRACE_KIND_LOCAL_RAY_NODE7_POSE_QUATERNION_ZW 79u
#define RNG_TRACE_KIND_LOCAL_RAY_NODE7_POSE_POSITION_XY 80u
#define RNG_TRACE_KIND_LOCAL_RAY_NODE7_POSE_POSITION_Z_SCALE 81u
#define RNG_TRACE_KIND_LOCAL_RAY_NODE7_MATRIX_HASH 82u
#define RNG_TRACE_KIND_OBJECT_TRANSLATE_POS_XY 83u /* object_translate entry: requested position x/y bits  info */
#define RNG_TRACE_KIND_OBJECT_TRANSLATE_POS_Z_HANDLE 84u /* entry: requested position z bits/object handle  info */
#define RNG_TRACE_KIND_OBJECT_TRANSLATE_PRE_CONNECT_XY 85u
#define RNG_TRACE_KIND_OBJECT_TRANSLATE_PRE_CONNECT_Z_HANDLE 86u
#define RNG_TRACE_KIND_OBJECT_TRANSLATE_EXIT_XY 87u
#define RNG_TRACE_KIND_OBJECT_TRANSLATE_EXIT_Z_HANDLE 88u
#define RNG_TRACE_KIND_BIPED_PHYSICS_ENTRY 89u /* flags/object handle */
#define RNG_TRACE_KIND_BIPED_PRE_QUERY_Y 90u /* new_velocity.y/object handle */
#define RNG_TRACE_KIND_BIPED_QUERY_OUT_Y 91u /* los_dir2.y/object handle */
#define RNG_TRACE_KIND_BIPED_WRITEBACK_Y 92u /* new_position.y/object handle */
#define RNG_TRACE_KIND_BIPED_QUERY_POS_WORLD_XY 93u
#define RNG_TRACE_KIND_BIPED_QUERY_POS_WORLD_Z_NEW_POS_X 94u
#define RNG_TRACE_KIND_BIPED_QUERY_NEW_POS_YZ 95u
#define RNG_TRACE_KIND_BIPED_QUERY_OUT_XY 96u
#define RNG_TRACE_KIND_BIPED_QUERY_OUT_Z_HANDLE 97u

/* 16 bytes. */
typedef struct {
  uint32_t tick;        /* (kind << 24) | (game tick & 0x00ffffff) */
  uint32_t seed_before; /* global seed BEFORE the draw.  For the two setter
                         * kinds this is instead the seed value being
                         * installed, i.e. the post-event state. */
  uint32_t caller;      /* __builtin_return_address(0) of the primitive */
  uint32_t caller2;     /* reserved (always 0): the EBP-chain hop was dropped
                         * because an unframed caller can leave an aligned
                         * garbage saved-EBP that faults on dereference */
} rng_trace_record_t;

typedef struct {
  uint32_t magic;       /* RNG_TRACE_MAGIC once the first record is written */
  uint32_t version;
  uint32_t capacity;
  uint32_t write_index; /* total records ever written; ring slot = idx % cap */
  rng_trace_record_t records[RNG_TRACE_CAPACITY];
} rng_trace_buffer_t;

/* Defined in src/halo/math/random_math.c; exported so tools/xbox
 * symbolization can resolve its runtime VA from the appended PE exports. */
__declspec(dllexport) extern rng_trace_buffer_t halo_rng_trace;

/* Records one event.  Ignores every seed pointer other than the global seed. */
__declspec(dllexport) void rng_trace_note(const void *seed, unsigned int kind,
                    unsigned int seed_before, void *caller, void *extra);

/* Call from inside the primitive itself so that __builtin_return_address(0)
 * names the game function that asked for the draw. */
#define RNG_TRACE(seed_ptr, kind, before)                                  \
  rng_trace_note((const void *)(seed_ptr), (kind), (unsigned int)(before), \
                 __builtin_return_address(0), (void *)0)

/* Probe record from inside a game function: `value` is any 32-bit payload
 * (float bits via RNG_TRACE_BITS), `extra` lands in caller2. */
#ifdef HALO_RNG_TRACE_DEEP
#define RNG_TRACE_EX_KIND_ENABLED(kind) 1
#else
/* Kinds 67--82 are the retired high-volume local-ray experiment. */
#define RNG_TRACE_EX_KIND_ENABLED(kind) ((kind) < 67u || (kind) > 82u)
#endif
#define RNG_TRACE_EX(kind, value, extra)                                   \
  do {                                                                     \
    if (RNG_TRACE_EX_KIND_ENABLED(kind)) {                                 \
      rng_trace_note((const void *)RNG_TRACE_GLOBAL_SEED_ADDR, (kind),     \
                     (unsigned int)(value), __builtin_return_address(0),   \
                     (void *)(extra));                                     \
    }                                                                      \
  } while (0)
#define RNG_TRACE_BITS(float_lvalue) (*(const unsigned int *)&(float_lvalue))

#endif /* HALO_MATH_RNG_TRACE_H */
