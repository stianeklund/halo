#include "x87_math.h"
#include "player_effects.h"
__declspec(noinline) char *player_effect_get(int16_t local_player_index)
{
  assert_halt_msg_at("local_player_index>=0 && "
                     "local_player_index<MAXIMUM_NUMBER_OF_LOCAL_PLAYERS",
                     "c:\\halo\\SOURCE\\effects\\player_effects.c", 0x73,
                     local_player_index >= 0 &&
                       local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  assert_halt_at("c:\\halo\\SOURCE\\effects\\player_effects.c", 0x74,
                 player_effect_globals);
  return (char *)&player_effects->effects[local_player_index];
}

void player_effect_initialize(void)
{
  player_effect_globals =
    (char *)game_state_malloc("player effects", 0,
                              sizeof(player_effect_globals_t));
  assert_halt_at("c:\\halo\\SOURCE\\effects\\player_effects.c", 0x7f,
                 player_effect_globals);
}

void player_effect_dispose(void)
{
}

void player_effect_initialize_for_new_map(void)
{
  csmemset(player_effect_globals, 0, sizeof(player_effect_globals_t));
  player_effects->field_3c0 = -1;
  player_effects->field_3e8 = game_time_get();
}

void player_effect_dispose_from_old_map(void)
{
}

/* scripted_player_effect_set_rotation -- store the three script-supplied
 * rotation components, each scaled by the constant at 0x253d4c
 * (0.017453292f, degrees to radians), into the player-effect globals.
 *
 * Confirmed (0xa28e0..0xa2916): EAX = [0x4557ec] (player_effect_globals);
 * FLD [EBP+8] / FMUL [0x253d4c] / FSTP [EAX+0x3d0], then the same shape for
 * [EBP+0xc] -> +0x3d4 and [EBP+0x10] -> +0x3d8.
 *
 * The first stack slot is loaded with FLD float ptr, so the binary reads it
 * as a float. The kb decl still declares it `int` (inherited from the
 * HaloScript caller at 0xc2fe0, which forwards result[0] verbatim); the decl
 * and caller are left unchanged here and the slot's bits are read as a float,
 * which matches both the caller's verbatim dword forward and this FLD.
 *
 * 0xa28e0 / player_effects.obj */
void scripted_player_effect_set_rotation(int param_1, float param_2,
                                         float param_3)
{
  player_effect_globals_t *globals;

  globals = player_effects;
  globals->scripted.field_0c[0] = *(float *)&param_1 * *(float *)0x253d4c;
  globals->scripted.field_0c[1] = param_2 * *(float *)0x253d4c;
  globals->scripted.field_0c[2] = param_3 * *(float *)0x253d4c;
}

/* Forward the two script-supplied motor values to the rumble system.
 *
 * Disassembly (0xa2920..0xa2929, 9 bytes):
 *   55            PUSH EBP
 *   8B EC         MOV  EBP,ESP
 *   5D            POP  EBP
 *   E9 57 72 01 00  JMP 0xb9b80   ; rumble_player_set_scripted_values
 *
 * A pure identical-forward tail call: no argument reload, no `add esp`, and
 * — decisively — no FILD/FSTP conversion code.  MSVC only collapses a wrapper
 * to that bare JMP when the parameter list matches the callee's exactly, so
 * both parameters are `float`, like rumble_player_set_scripted_values(float,
 * float) at 0xb9b80.  The kb decl previously read `(int param_1, float
 * param_2)`, inferred from the HaloScript caller at 0xc3030 where argument 1
 * is pushed with `MOV EDX,[EAX]; PUSH EDX` while argument 2 uses the
 * `PUSH ECX; FSTP [ESP]` float idiom.  That mixed shape is MSVC scheduling,
 * not a type signal — the callee proves it: 0xb9b80 stores BOTH parameters
 * into float slots yet marshals the first with `FLD [EBP+8]; FSTP
 * [EAX+0x820]` and the second with a plain dword `MOV ECX,[EBP+0xc];
 * MOV [EAX+0x824],ECX`.  The 0xc3030 call site was corrected to read its
 * first argument as a float lvalue so the dword is still forwarded verbatim
 * instead of being run through an int-to-float conversion.
 *
 * 0xa2920 / player_effects.obj */
void scripted_player_effect_set_rumble(float left_motor, float right_motor)
{
  rumble_player_set_scripted_values(left_motor, right_motor);
}

/* player_telefrag_effect_stop -- silence the telefragged player's rumble.
 *
 * Confirmed (0xa2930..0xa2965, 54 bytes):
 *   - MOV EAX,[EBP+8] / MOV ECX,[0x5aa6d4] / PUSH EAX / PUSH ECX /
 *     CALL 0x119320: the function takes ONE stack argument (a player datum
 *     handle) even though the kb decl previously read `(void)`.  Argument
 *     order is datum_get(g_players_data, player_handle), the same shape as
 *     the other player lookups in this TU.
 *   - MOVSX ESI,word ptr [EAX+2]: the local-player index is a signed 16-bit
 *     field at player+2, sign-extended before the CMP ESI,-1 / JZ guard.
 *     The C local must therefore be a 32-bit `int` holding the widened value,
 *     not an `int16_t`; declaring it int16_t makes the compiler emit
 *     `xor esi,esi / mov si,[eax+2]` plus a 16-bit `cmp si,-1` instead of the
 *     single MOVSX and 32-bit CMP.  Both calls receive the full dword in ESI.
 *   - PUSH ESI / CALL 0xa2690 (player_effect_get) with its return value
 *     unused, then PUSH 0 / PUSH 0 / PUSH ESI / CALL 0xb9da0.  The single
 *     ADD ESP,0x10 at 0xa2960 cleans up BOTH calls (1 + 3 dwords) -- this is
 *     why the call-site audit reports cleanup=4 against a 3-parameter decl
 *     for rumble_player_continuous; it is not a fourth argument.
 *
 * The discarded player_effect_get result is preserved because the call is a
 * real side-effecting step in the original instruction stream (it carries the
 * bounds asserts at 0xa2690); its return value is genuinely dead.
 *
 * 0xa2930 / player_effects.obj */
void player_telefrag_effect_stop(int player_handle)
{
  char *player;
  int local_player_index;

  player = (char *)datum_get(player_data, player_handle);
  local_player_index = *(int16_t *)(player + 2);

  if (local_player_index != -1) {
    player_effect_get((int16_t)local_player_index);
    rumble_player_continuous((short)local_player_index, 0, 0);
  }
}

/* player_effect_screen_fade_in -- record a screen fade in the player effect
 * globals and stamp its start time.
 *
 * Confirmed (0xa2970..0xa29b7, 72 bytes, no locals):
 *   - EAX = [0x4557ec] (player_effect_globals), read once for the stores.
 *   - [EBP+8] -> globals+0x3b0 via FLD/FSTP (4 bytes); [EBP+0xc] -> +0x3b4
 *     and [EBP+0x10] -> +0x3b8 via dword MOVs; word [EBP+0x14] -> +0x3c0.
 *     The FLD/FSTP makes the first slot a float.  The only caller,
 *     hs_evaluate_fade_in (0xc22a0), evaluates the script parameters
 *     (real, real, real, short), so all three dword slots are floats.
 *   - byte +0x3c2 = 0.
 *   - CALL 0xb5aa0 (game_time_get, cdecl, no args); the globals pointer is
 *     re-read after the call and EAX is stored to +0x3bc.
 *
 * 0xa2970 / player_effects.obj */
void player_effect_screen_fade_in(float red, float green, float blue,
                                  uint16_t ticks)
{
  player_effect_globals_t *globals;

  globals = player_effects;
  globals->field_3b0.red = red;
  globals->field_3b0.green = green;
  globals->field_3b0.blue = blue;
  globals->field_3c0 = ticks;
  globals->field_3c2 = 0;
  player_effects->field_3bc = game_time_get();
}

/* player_effect_screen_fade_out -- same record as
 * player_effect_screen_fade_in, but marks the fade direction byte as 1.
 *
 * Confirmed (0xa29c0..0xa2a07, 72 bytes, no locals):
 *   - EAX = [0x4557ec] (player_effect_globals), read once for the stores.
 *   - [EBP+8] -> globals+0x3b0 via FLD/FSTP (4 bytes); [EBP+0xc] -> +0x3b4
 *     and [EBP+0x10] -> +0x3b8 via dword MOVs; word [EBP+0x14] -> +0x3c0.
 *     All three dword slots are floats (see player_effect_screen_fade_in).
 *   - byte +0x3c2 = 1 (fade_in stores 0).
 *   - CALL 0xb5aa0 (game_time_get, cdecl, no args); the globals pointer is
 *     re-read after the call and EAX is stored to +0x3bc.
 *
 * 0xa29c0 / player_effects.obj */
void player_effect_screen_fade_out(float red, float green, float blue,
                                   uint16_t ticks)
{
  player_effect_globals_t *globals;

  globals = player_effects;
  globals->field_3b0.red = red;
  globals->field_3b0.green = green;
  globals->field_3b0.blue = blue;
  globals->field_3c0 = ticks;
  globals->field_3c2 = 1;
  player_effects->field_3bc = game_time_get();
}

/* player_effect_get_damage_indicators -- copy the local player's four damage
 * indicator bytes to `out`, then age each live indicator by the elapsed game
 * time, saturating at 0xff.
 *
 * Confirmed (0xa2a10..0xa2a6f):
 *   - PUSH [EBP+8] / CALL 0xa2690: the raw dword is forwarded to
 *     player_effect_get(int16_t).
 *   - LEA ESI,[EAX+0xe4]: the indicator array lives at effect+0xe4 and is
 *     4 bytes wide (PUSH 0x4 / PUSH ESI / PUSH ECX / CALL csmemcpy, ADD
 * ESP,0x10). The same +0xe4/4 window is cleared in player_effect_update.
 *   - The copy happens BEFORE the aging pass, so `out` receives the previous
 *     tick's values.
 *   - Loop is a 4-iteration countdown (MOV EDI,4 / INC ESI / DEC EDI / JNZ)
 *     that skips zero entries (CMP byte ptr [ESI],0x0 / JZ).
 *   - Saturation test is signed on the widened sum: MOVSX EDX,AX (int16_t
 *     game_time_get_elapsed) + MOVZX EAX,byte ptr [ESI], CMP EDX,0xff,
 *     JGE -> 0xff.
 *   - game_time_get_elapsed() is called a SECOND time on the non-saturating
 *     path (CALL 0x000b5ae0 at 0xa2a3d and again at 0xa2a52); the sum is
 *     recomputed rather than reused, so both calls are preserved here.
 *
 * 0xa2a10 / player_effects.obj */
void player_effect_get_damage_indicators(int player_index, void *out)
{
  unsigned char *indicators;
  int count;
  int aged;

  indicators =
    ((player_effect_t *)player_effect_get((int16_t)player_index))->field_e4;
  csmemcpy(out, indicators, 4);
  count = 4;
  do {
    if (*indicators != 0) {
      if ((int)game_time_get_elapsed() + (int)*indicators < 0xff) {
        aged = game_time_get_elapsed() + *indicators;
      } else {
        aged = 0xff;
      }
      *indicators = (unsigned char)aged;
    }
    indicators++;
    count--;
  } while (count != 0);
}

/* player_effect_clear_damage_indicators -- zero the local player's four damage
 * indicator bytes.
 *
 * Confirmed (0xa2a70..0xa2a8f):
 *   - MOV EAX,[EBP+8] / PUSH EAX / CALL 0xa2690: the raw dword is forwarded to
 *     player_effect_get(int16_t), same shape as 0xa2a10.
 *   - ADD EAX,0xe4: the same 4-byte indicator array at effect+0xe4 that
 *     player_effect_get_damage_indicators copies out of.
 *   - PUSH 0x4 / PUSH 0x0 / PUSH EAX / CALL 0x8db80 -> csmemset(buf, 0, 4).
 *   - The single ADD ESP,0x10 retires the callee argument of
 *     player_effect_get together with csmemset's three; it is not a 4-argument
 *     csmemset call.
 *
 * 0xa2a70 / player_effects.obj */
void player_effect_clear_damage_indicators(int player_index)
{
  player_effect_t *effect;

  effect = (player_effect_t *)player_effect_get((int16_t)player_index);
  csmemset(effect->field_e4, 0, sizeof(effect->field_e4));
}

/* effect_scale_factor -- returns base + (1.0f - base) * scale.
 *
 * Binary (0xa2a90): FLD [0x2533c8] (1.0f); FSUB [ebp+8]; FMUL [ebp+0xc];
 * FADD [ebp+8]; result left in ST0, plain RET (cdecl, no callees).
 * Parameter meanings unknown; names are positional.
 *
 * 0xa2a90 / player_effects.obj */
float effect_scale_factor(float param_1, float param_2)
{
  return (*(float *)0x002533c8 - param_1) * param_2 + param_1;
}

/* 0xa2ab0 / player_effects.obj.  The descriptor arrives in EBX; the first
 * stack argument (a local player index) is never read.  `scale` is converted
 * to ticks and narrowed back into its own argument slot (FSTP [EBP+0x14]). */
void player_effect_update_screen_flash(int local_player_index, char *effect,
                                       float intensity, float scale,
                                       void *descriptor /* @<ebx> */)
{
  double duration;

  (void)local_player_index;

  scale = scale * TICKS_PER_SECOND;
  HALO_FLT_ROUNDTRIP(scale);
  if (!(((player_effect_t *)effect)->field_1a > ((int16_t *)descriptor)[1] &&
        (float)((player_effect_t *)effect)->field_de >
          scale * *(float *)((char *)descriptor + 0x10)) &&
      unknown_flash_type_table[*(int16_t *)descriptor] != 0) {
    memcpy(&((player_effect_t *)effect)->field_18, descriptor, 0x38);
    /* FST keeps the unnarrowed product in ST0 for _ftol2; a float*float
     * product is exact in double. */
    duration = (double)scale * ((player_effect_t *)effect)->field_28;
    ((player_effect_t *)effect)->field_28 = (float)duration;
    ((player_effect_t *)effect)->field_de = (int16_t)(int64_t)duration;
    ((player_effect_t *)effect)->field_3c =
      effect_scale_factor(*(float *)((char *)descriptor + 0x24), intensity) <
          *(float *)0x002533c0
        ? *(float *)0x002533c0
        : (effect_scale_factor(*(float *)((char *)descriptor + 0x24),
                               intensity) >
               *(float *)((char *)descriptor + 0x20)
             ? *(float *)((char *)descriptor + 0x20)
             : effect_scale_factor(*(float *)((char *)descriptor + 0x24),
                                   intensity));
    ((player_effect_t *)effect)->field_e8 |= 1;
  }
}

/* effect_scale_value -- evaluates a transition function at
 * t = 1.0f - param_2 / param_3 and scales the result by param_1.
 *
 * Binary (0xa2c70): FLD [ebp+0xc]; FDIV [ebp+0x10]; FSUBR [0x2533c8] (1.0f);
 * FSTP [esp]; PUSH EAX (caller-supplied, never defined here);
 * CALL transition_function_evaluate; FMUL [ebp+8]; result in ST0.
 * No xrefs in the binary; float parameter meanings unknown (positional).
 *
 * 0xa2c70 / player_effects.obj */
float effect_scale_value(short function_type /* @<eax> */, float param_1,
                         float param_2, float param_3)
{
  return transition_function_evaluate(function_type, *(float *)0x002533c8 -
                                                       param_2 / param_3) *
         param_1;
}

void player_effect_update(void)
{
  int16_t local_player_index;
  int player_index;
  void *player;
  player_effect_t *effect;

  local_player_index = (int16_t)local_player_get_next(-1);
  while (local_player_index != -1) {
    player_index = local_player_get_player_index(local_player_index);
    if (player_index != -1) {
      player = datum_get(player_data,
                         local_player_get_player_index(local_player_index));
      if (*(int *)((char *)player + 0x34) != -1) {
        local_player_index = (int16_t)local_player_get_next(local_player_index);
        continue;
      }
    }
    effect = (player_effect_t *)player_effect_get(local_player_index);
    csmemset(effect->field_e4, 0, sizeof(effect->field_e4));
    csmemset(player_effect_get(local_player_index), 0, sizeof(player_effect_t));
    rumble_player_clear(local_player_index);
    local_player_index = (int16_t)local_player_get_next(local_player_index);
  }
}

/* scripted_player_effect_set_translation -- store the three script-supplied
 * translation components into the shared player-effect globals.
 *
 * Confirmed (0xa2dc0..0xa2de4, 37 bytes): the globals pointer at 0x4557ec
 * (player_effect_globals, the 0x3ec-byte block allocated at 0xa2700) is loaded
 * once into EAX, then the three incoming stack dwords [EBP+8], [EBP+0xc] and
 * [EBP+0x10] are written verbatim to +0x3c4, +0x3c8 and +0x3cc.  There is no
 * FILD/FSTP conversion and no arithmetic on any of them, so each argument slot
 * is forwarded bit-exact; the dword MOV shape is MSVC scheduling and carries no
 * type signal either way (same caveat as 0xa2920).
 *
 * Unknown: the globals' field types at +0x3c4..+0x3cc.  The kb decl's
 * int/float/float split is inherited from the HaloScript call site at 0xc2f90
 * and is preserved here, so each parameter is stored through a pointer of its
 * own declared type; MSVC copies the float parameters with plain dword MOVs
 * (no FLD/FSTP), reproducing the reference exactly.
 *
 * 0xa2dc0 / player_effects.obj */
void scripted_player_effect_set_translation(int param_1, float param_2,
                                            float param_3)
{
  player_effect_globals_t *globals;

  globals = player_effects;
  *(int *)&globals->scripted.field_00[0] = param_1;
  globals->scripted.field_00[1] = param_2;
  globals->scripted.field_00[2] = param_3;
}

/* scripted_player_effect_start -- script-driven start of the scripted player
 * effect: stores the maximum intensity, converts the duration to ticks and
 * arms the effect.
 *
 * The duration product is narrowed to float32 through its argument slot
 * (FSTP/FLD) before the round-to-nearest FISTP; HALO_FLT_ROUNDTRIP reproduces
 * that under clang.
 *
 * The first argument is a float: the callee copies it with FLD/FSTP. The only
 * caller forwards the HaloScript record's first dword with an integer PUSH.
 *
 * The first store goes through the global directly (a cached pointer there
 * turns the FLD/FSTP copy into an integer move); the pointer is cached after
 * the rounding asm, which forces a reload anyway.
 *
 * 0xa2df0 / player_effects.obj */
enum {
  _scripted_player_effect_active_bit,
  _scripted_player_effect_stopping_bit,
  NUMBER_OF_SCRIPTED_PLAYER_EFFECT_FLAGS
};

void scripted_player_effect_start(float param_1, float param_2)
{
  player_effect_globals_t *globals;
  real duration;
  int16_t ticks;

  player_effects->scripted.field_18 = param_1;
  duration = param_2 * TICKS_PER_SECOND;
  HALO_FLT_ROUNDTRIP(duration);
  ticks = (int16_t)x87_round_to_int(duration);
  globals = player_effects;
  globals->scripted.field_1c = ticks;
  globals->scripted.field_1e = ticks;
  globals->field_3e4 =
    (globals->field_3e4 &
     ~(uint32_t)FLAG(_scripted_player_effect_stopping_bit)) |
    (uint32_t)FLAG(_scripted_player_effect_active_bit);
}

/* scripted_player_effect_stop -- script-driven stop of the scripted player
 * effect: converts the incoming duration to ticks and arms the stop.
 *
 * Confirmed (0xa2e40..0xa2e76): FLD [EBP+8]; FMUL [0x253394]
 * (TICKS_PER_SECOND); FSTP [EBP+8] -- the product is narrowed back into the
 * argument slot -- then FLD; FISTP [EBP-4] (round-to-nearest, no _ftol2).
 * The low word of that integer is stored to both +0x3e0 and +0x3e2 of
 * player_effect_globals (0x4557ec), then bit 1 of the dword at +0x3e4 is set.
 *
 * The kb decl keeps `int param_1` because the only caller (0xc30b0) forwards
 * the HaloScript record's +0 dword bit-exact (MOV EDX,[EAX]; PUSH EDX); the
 * callee reads that slot as a float, so it is reinterpreted, not converted.
 * Unknown: semantics of +0x3e0/+0x3e2 (int16) and of flag bit 1 at +0x3e4.
 *
 * 0xa2e40 / player_effects.obj */
void scripted_player_effect_stop(int param_1)
{
  player_effect_globals_t *globals;
  int16_t ticks;

  *(float *)&param_1 = *(float *)&param_1 * TICKS_PER_SECOND;
  ticks = (int16_t)x87_round_to_int(*(float *)&param_1);
  globals = player_effects;
  globals->scripted.field_1c = ticks;
  globals->scripted.field_1e = ticks;
  globals->field_3e4 |= 2;
}

void player_effect_screen_flash(int player_handle, void *effect_descriptor,
                                float intensity)
{
  int16_t unit_index;
  void *player;
  char *effect;

  if (player_handle == -1)
    return;

  player = datum_get(player_data, player_handle);
  unit_index = *(int16_t *)((char *)player + 2);

  if (unit_index == -1)
    return;

  effect = player_effect_get(unit_index);
  player_effect_update_screen_flash(unit_index, effect, intensity, 1.0f,
                                    effect_descriptor /* @<ebx> */);
}

/* player_telefrag_effect_start -- start the white full-screen flash and
 * full-strength rumble on a player who has just been telefragged.
 *
 * The function builds a synthetic player-effect descriptor on the stack
 * instead of reading one out of a jpt! tag, then runs it through the same two
 * helpers the damage path uses (player_effect_update_screen_flash at 0xa2ab0
 * and player_effect_update_camera_shake).
 *
 * Confirmed (0xa2ed0..0xa2fbc, 237 bytes):
 *   - The kb decl previously read `(void)`; the body reads [EBP+8] (a player
 *     datum handle) and [EBP+0xc] (a float), so there are two stack params.
 *   - SUB ESP,0x84 covers exactly three memory locals, and 0x4 + 0x38 + 0x48
 *     is exactly 0x84: the effect pointer at EBP-0x4 (spilled across the
 *     0xa2ab0 call because that call site reuses EBX), the 0x38-byte
 *     descriptor at EBP-0x3c, and the 0x48-byte effect-data block at
 *     EBP-0x84.  Both aggregates are zeroed by MSVC's `= {0}` expansion,
 *     which stores the first element explicitly and REP STOSes the rest --
 *     the width of that first store gives the element type:
 *       descriptor : MOV word [EBP-0x3c],0 / ECX=0xd / REP STOSD / STOSW
 *                    = 2 + 52 + 2 bytes  -> int16_t[28]
 *       effect_data: MOV dword [EBP-0x84],0 / ECX=0x11 / REP STOSD
 *                    = 4 + 68 bytes      -> float[18]
 *     The descriptor is zeroed first, so it is declared first.
 *   - datum_get(player_data, player_handle) then MOVSX ESI,word ptr [EAX+2]:
 *     the local-player index is sign-extended to 32 bits before CMP ESI,-1,
 *     so the C local is `int`, exactly as in player_telefrag_effect_stop.
 *   - Descriptor stores (offsets from EBP-0x3c; field meanings come from
 *     player_effect_update_screen_flash, which copies all 0x38 bytes into
 *     effect+0x18):
 *       +0x00 word 1     effect type
 *       +0x02 word 2     priority
 *       +0x10 1.0f       duration
 *       +0x20 intensity  maximum
 *       +0x24 0.0f       minimum
 *       +0x28..+0x37     16 bytes copied through *(float **)0x2ee6c4, the
 *                        pointer to the all-ones colour {1,1,1,1} at
 *                        0x267700 -- the same global ai_debug.c and actors.c
 *                        read as a colour.  Loaded once into EAX
 *                        (MOV EAX,[0x2ee6c4]) and copied as four dwords.
 *   - effect_data stores: [0] = 1.0f and [2] = intensity * 0.01.  The
 *     multiply is FLD float [EBP+0xc] / FMUL *double* ptr [0x26aed0] / FSTP
 *     float [EBP-0x7c], and 0x26aed0 holds the double 0.01, so the literal is
 *     unsuffixed and the product is narrowed on the store.
 *   - PUSH EDI / PUSH EDI with EDI = dword ptr [EBP+0xc]: both rumble motor
 *     values are the raw dword of the float parameter and there is no
 *     FISTP/_ftol anywhere in the function.  rumble_player_continuous is
 *     declared with int motor params because 0xb9da0 stores both through
 *     `*(int *)`, so the dword must be forwarded by value; the punned
 *     `*(int *)&intensity` reproduces the plain MOV/PUSH pair instead of an
 *     int conversion.
 *   - The single ADD ESP,0x2c at 0xa2fb3 cleans up all four cdecl calls
 *     (1 + 3 + 4 + 3 dwords).  That is why the call-site audit reports
 *     cleanup=11 against player_effect_update_camera_shake's three stack
 * params; it is not evidence of extra arguments.
 *   - 0xa2ab0 receives the descriptor in EBX (LEA EBX,[EBP-0x3c] immediately
 *     before the CALL) and 0xa2ba0 receives the effect-data block in EAX and
 *     the effect pointer in EBX.  The descriptor is passed as an ordinary
 *     fifth argument here because 0xa2ab0 is file-local in this TU, matching
 *     the other two call sites.
 *
 * 0xa2ed0 / player_effects.obj */
void player_telefrag_effect_start(int player_handle, float intensity)
{
  char *effect;
  int16_t descriptor[28] = { 0 };
  float effect_data[18] = { 0 };
  char *player;
  int local_player_index;
  float *flash_color;

  player = (char *)datum_get(player_data, player_handle);
  local_player_index = *(int16_t *)(player + 2);

  if (local_player_index != -1) {
    effect = player_effect_get((int16_t)local_player_index);

    effect_data[2] = (float)(intensity * 0.01);

    flash_color = *(float **)0x2ee6c4;
    *(float *)((char *)descriptor + 0x28) = flash_color[0];
    *(float *)((char *)descriptor + 0x2c) = flash_color[1];
    *(float *)((char *)descriptor + 0x30) = flash_color[2];
    *(float *)((char *)descriptor + 0x34) = flash_color[3];

    effect_data[0] = 1.0f;
    descriptor[0] = 1;
    descriptor[1] = 2;
    *(float *)((char *)descriptor + 0x10) = 1.0f;
    *(float *)((char *)descriptor + 0x20) = intensity;
    *(float *)((char *)descriptor + 0x24) = 0.0f;

    rumble_player_continuous((short)local_player_index, *(int *)&intensity,
                             *(int *)&intensity);
    player_effect_update_screen_flash(local_player_index, effect, intensity,
                                      1.0f, descriptor /* @<ebx> */);
    player_effect_update_camera_shake(local_player_index, intensity, 1.0f,
                                      effect_data /* @<eax> */,
                                      (void *)effect /* @<ebx> */);
  }
}

/* player_effect_get_screen_flash -- fill the per-window screen flash block
 * (render_scene's window_parameters +0x238) for one local player.
 *
 * Output layout written here (offsets from screen_flash):
 *   +0x00 int16  flash type
 *   +0x04 float  intensity
 *   +0x08 float  alpha (1.0f on the scripted-fade path)
 *   +0x0c float  red, +0x10 green, +0x14 blue
 * +0x02 and anything past +0x18 are never written.
 *
 * Confirmed (0xa2fc0..0xa32df):
 *   - assert screen_flash != NULL (line 0x1e4), then console_is_active()
 *     (0xff4c0): when true NOTHING is written and only the trailing
 *     assert_valid_real check runs.
 *   - scripted fade (player_effect_screen_fade_in/out state at globals
 *     +0x3b0..+0x3c2) wins while duration (+0x3c0, int16) != -1 and either
 *     the fade-out byte (+0x3c2) is set or game_time_get() - start (+0x3bc)
 *     <= duration (CMP ECX,EDX / JG -> player path).
 *   - fade path: type = 1; rgb dwords from +0x3b0/+0x3b4/+0x3b8 to +0x0c;
 *     +0x08 = 0x3f800000; when duration > 0 the ratio
 *     FILD (time-start) / FIDIV duration is PIN'd to [0,1] (recomputed with
 *     a fresh game_time_get() for each use, as the macro expands) and fed to
 *     transition_function_evaluate(5, ratio); otherwise 1.0f.  The value is
 *     stored (FST) and replaced by 1.0f - value when +0x3c2 == 0 (fade in),
 *     then the stored intensity is PIN'd to [0,1].
 *   - player path: skipped for player_index == -1 (CMP AX,0xffff);
 *     effect = player_effect_get(player_index); globals +0x3c0 = -1; when
 *     effect +0xde (int16 ticks) > 0 or effect +0xe8 bit 0: clear bit 0,
 *     type = ((int16 *)0x2ef7e0)[effect +0x18], +0x08..+0x14 = effect
 *     +0x40..+0x4c, intensity = transition_function_evaluate(word +0x2c,
 *     ticks / +0x28 * +0x3c) when +0x28 > 0.0f else +0x3c; ticks -=
 *     game_time_get_elapsed(); assert 0 <= intensity <= 1 (line 0x212).
 *   - assert_valid_real(screen_flash->intensity) at line 0x217.
 * Uncertain: +0x3c2 = 1 is fade OUT (set by 0xa29c0) -- the name of that
 *   flag is inferred from the fade_in/fade_out twins, not proven here.
 *
 * Offsets are player_effect_globals_t / player_effect_t fields
 *   (player_effects.h).  0x2ef7e0 is unknown_flash_type_table (MOVSX
 *   EDX,[EDI+0x18]; MOV AX,[EDX*2+0x2ef7e0]) giving the output flash type;
 *   0x5ab100 is error_string_buffer, the assert_valid_real scratch buffer.
 * Copy shapes: globals +0x3b0 -> out +0x0c is one 12-byte rgb copy
 *   (LEA ECX/LEA EDX, three dwords); effect +0x40 -> out +0x08 is one
 *   16-byte argb copy (alpha first, four dwords).
 * The fade intensity value stays on the x87 stack across the fade-in
 *   flip (FST [ESI+4]; FLD 1.0; FSUB ST0,ST1), and the final [0,1] pin is
 *   one expression (FLD const / FSTP per arm, including the x = x arm).
 * The scripted-fade arm re-reads player_effect_globals after each call
 *   (MOV EAX,[0x4557ec]); only the duration address survives in EDI.
 * The 0x217 check passes csprintf's result straight to display_assert
 *   (display_assert args are pushed before the csprintf call).
 *
 * Uncertain: what the 0x2ef7e0 entries mean beyond feeding out +0x00.
 *
 * 0xa2fc0 / player_effects.obj */
void player_effect_get_screen_flash(int16_t player_index, void *screen_flash)
{
  char *out;
  player_effect_t *effect;
  float value;

  out = (char *)screen_flash;
  assert_halt_at("c:\\halo\\SOURCE\\effects\\player_effects.c", 0x1e4,
                 screen_flash);
  if (!console_is_active()) {
    if (player_effects->field_3c0 != -1 &&
        (player_effects->field_3c2 != 0 ||
         game_time_get() - player_effects->field_3bc <=
           player_effects->field_3c0)) {
      *(int16_t *)(out + 0x00) = 1;
      *(real_rgb_color *)(out + 0x0c) = player_effects->field_3b0;
      *(float *)(out + 0x08) = 1.0f;
      /* PIN(elapsed / duration, 0, 1) expanded in place: each use re-reads
       * game_time_get() (CALL 0xb5aa0 at 0xa3070, 0xa30ad, 0xa30e8) and
       * the single CALL 0x10a710 takes the pinned value from [EBP+0xc]. */
      if (player_effects->field_3c0 > 0) {
        value = CLAMP((float)(game_time_get() - player_effects->field_3bc) /
                        player_effects->field_3c0,
                      0.0f, 1.0f);
        value = transition_function_evaluate(5, value);
      } else {
        value = 1.0f;
      }
      *(float *)(out + 0x04) = value;
      if (player_effects->field_3c2 == 0) {
        *(float *)(out + 0x04) = 1.0f - *(float *)(out + 0x04);
      }
      *(float *)(out + 0x04) = CLAMP(*(float *)(out + 0x04), 0.0f, 1.0f);
    } else if (player_index != -1) {
      effect = (player_effect_t *)player_effect_get(player_index);
      player_effects->field_3c0 = -1;
      if (effect->field_de > 0 || (effect->field_e8 & 1) != 0) {
        effect->field_e8 &= 0xfe;
        *(int16_t *)(out + 0x00) = unknown_flash_type_table[effect->field_18];
        *(real_argb_color *)(out + 0x08) = effect->field_40;
        if (effect->field_28 > 0.0f) {
          *(float *)(out + 0x04) = transition_function_evaluate(
            (short)effect->field_2c,
            effect->field_3c * ((float)effect->field_de / effect->field_28));
        } else {
          *(float *)(out + 0x04) = effect->field_3c;
        }
        effect->field_de -= game_time_get_elapsed();
        assert_halt_msg_at(
          "screen_flash->intensity>=0.0f && screen_flash->intensity<=1.0f",
          "c:\\halo\\SOURCE\\effects\\player_effects.c", 0x212,
          *(float *)(out + 0x04) >= 0.0f && *(float *)(out + 0x04) <= 1.0f);
      }
    }
  }
  if ((*(uint32_t *)(out + 0x04) & 0x7f800000u) == 0x7f800000u) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real(0x%08X %f)",
                            "screen_flash->intensity",
                            *(uint32_t *)(out + 0x04),
                            (double)*(float *)(out + 0x04)),
                   "c:\\halo\\SOURCE\\effects\\player_effects.c", 0x217,
                   1);
    system_exit(-1);
  }
}

/* get_shake_matrix -- apply a random-axis rotation and a random-direction
 * translation to the caller's matrix (ESI).
 *
 * Binary (0xa32e0): if [ebp+0xc] != [0x2533c0]: seed =
 * random_math_get_local_seed_address(); random_seed_get_direction3d(seed,
 * &axis); FUN_001092d0(ESI, &axis, fsin(angle), fcos(angle)) (both floats
 * FSTP'd into the reused arg slots). If [ebp+8] != [0x2533c0]: same random
 * direction, scaled by [ebp+8], stored to matrix +0x28/+0x2c/+0x30.
 * Callers: FUN_000a3370 (x2).
 *
 * 0xa32e0 / player_effects.obj */
void get_shake_matrix(float *matrix /* @<esi> */, float translation_scale,
                      float rotation_angle)
{
  float direction[3];

  if (rotation_angle != *(float *)0x002533c0) {
    random_seed_get_direction3d(random_math_get_local_seed_address(),
                                direction);
    FUN_001092d0(matrix, direction, x87_fsin(rotation_angle),
                 x87_fcos(rotation_angle));
  }
  if (translation_scale != *(float *)0x002533c0) {
    random_seed_get_direction3d(random_math_get_local_seed_address(),
                                direction);
    matrix[10] = direction[0] * translation_scale;
    matrix[11] = direction[1] * translation_scale;
    matrix[12] = direction[2] * translation_scale;
  }
}

/* player_effect_get_camera_effect_matrix -- build the camera-effect transform
 * for a local player into `matrix` (a real_matrix4x3, 13 floats).  The only
 * caller, set_window_camera_values (0x102282), multiplies it into the view.
 *
 * Confirmed: cdecl, 2 stack args, plain RET, no return value.  The caller
 *   pushes a zero-extended word and a float[13] address; the body tests the
 *   index with CMP SI,-1 and forwards the dword to player_effect_get(int16_t).
 * Confirmed: assert "matrix" (line 0x259) runs before the -1 early return,
 *   which leaves `matrix` unwritten.  game_time_get() (0xb5aa0) is called
 *   next and its result is discarded.
 * Confirmed scripted path (player_effect_globals+0x3e4 bit 0):
 *   - scale = +0x3dc; matrix = *identity (REP MOVSD 0xd from *0x31fc60).
 *   - ticks +0x3e0 > 0: scale *= ticks / +0x3e2 when bit 1 is set, else
 *     scale *= 1 - ticks / +0x3e2; then +0x3e0 -= game_time_get_elapsed().
 *     ticks <= 0 with bit 1 set: dword +0x3e4 &= ~1, rumble scale 0.
 *   - bit 0 is re-read through a reloaded player_effect_globals; clear ->
 *     return with matrix = identity.
 *   - scale clamped to [0, 1], passed to rumble_player_set_scale.
 *   - three random_real_range(-1, 1) draws r1, r2, r3 then
 *     FUN_00109e90(matrix, r3*+0x3d0*s, r2*+0x3d4*s, r1*+0x3d8*s); three more
 *     draws q1, q2, q3 then position = (q3*+0x3c8*s, q2*+0x3c4*s, q1*+0x3cc*s)
 *     (FSTP order [ESI+0x28], [ESI+0x2c], [ESI+0x30]).  Field reads use the
 *     first globals load (EBX = globals+0x3c4).
 * Confirmed per-player path (effect = player_effect_get(index)):
 *   - effect+0xe0 ticks > 0 or +0xe8 bit 1: intensity = 1.0f when bit 1 is
 *     set, else transition_function_evaluate(+0x54, 1 - (+0x50 - ticks) /
 *     +0x50) * +0x68.  Clears bit 1, axis = cross(global up (*0x31fc44),
 *     effect+0x00), FUN_001092d0(local, axis, sin(i * +0x58), cos(i * +0x58)),
 *     local position = i * +0x0c..+0x14 + (i * +0x5c) * +0x00..+0x08, then
 *     +0xe0 -= elapsed.  Otherwise the identity is copied.
 *   - effect+0xe2 ticks > 0 or +0xe8 bit 2: local = identity; intensity 1.0f
 *     (bit 2) or transition_function_evaluate(+0x88, ...) * +0xac;
 *     level = ((1 - +0xa8) + FUN_0010a5e0(+0xa0, (+0x84 - ticks) / +0xa4) *
 *     +0xa8) * intensity; translation = level * +0x8c, rotation = level *
 *     +0x90, each forced to 0 unless > 0; clears bit 2;
 *     get_shake_matrix(local, translation + +0xd4, rotation + +0xd8);
 *     rumble_player_continuous(index, +0xcc, +0xd0) as raw dwords;
 *     +0xdc += elapsed and, when > 0, reset with +0xcc..+0xdb zeroed;
 *     get_shake_matrix(local, translation, rotation); +0xe2 -= elapsed;
 *     matrix4x3_multiply(matrix, local, matrix).
 * Field map: globals +0x3c4..+0x3e3 is player_effect_globals_t.scripted
 *   (+0x3dc/+0x3e0/+0x3e2 are its field_18/field_1c/field_1e).
 * effect_fields aliases effect->field_00; indexing through the field
 *   directly gives different register allocation (3 bytes shorter).
 * Uncertain: the x87 temporaries in the original keep i * +0x58 and
 *   i * +0x5c at extended precision; x87_fsin/x87_fcos take float.
 *
 * 0xa3370 / player_effects.obj */
void player_effect_get_camera_effect_matrix(int16_t local_player_index,
                                            float *matrix)
{
  scripted_player_effect_t *scripted;
  player_effect_t *effect;
  real *rumble;
  float *effect_fields;
  float *up;
  const real_matrix4x3 *source;
  real_matrix4x3 effect_matrix;
  float axis[3];
  float scale;
  float random_a;
  float random_b;
  float random_c;
  float intensity;
  float angle;
  float translation;
  float rotation;
  float level;
  float duration;
  float zero_scale;
  int16_t ticks;

  assert_halt_at("c:\\halo\\SOURCE\\effects\\player_effects.c", 0x259, matrix);
  if (local_player_index == -1) {
    return;
  }
  game_time_get();
  /* The binary tests the flag dword through its low byte (TEST byte). */
  if ((*(uint8_t *)&player_effects->field_3e4 & 1) != 0) {
    scripted = &player_effects->scripted;
    scale = scripted->field_18;
    *(real_matrix4x3 *)matrix = *global_identity4x3;
    ticks = scripted->field_1c;
    if (ticks > 0) {
      if ((*(uint8_t *)&player_effects->field_3e4 & 2) != 0) {
        scale = (float)ticks / (float)scripted->field_1e * scale;
      } else {
        scale = (1.0f - (float)ticks / (float)scripted->field_1e) * scale;
      }
      scripted->field_1c -= game_time_get_elapsed();
    } else if ((player_effects->field_3e4 & 2) != 0) {
      player_effects->field_3e4 &= 0xfffffffeu;
      rumble_player_set_scale(0.0f);
    }

    if ((*(uint8_t *)&player_effects->field_3e4 & 1) == 0) {
      return;
    }

    if (scale < 0.0f) {
      scale = 0.0f;
    } else if (scale > 1.0f) {
      scale = 1.0f;
    }
    rumble_player_set_scale(scale);

    random_a = random_real_range((int *)random_math_get_local_seed_address(),
                                 -1.0f, 1.0f);
    random_b = random_real_range((int *)random_math_get_local_seed_address(),
                                 -1.0f, 1.0f);
    random_c = random_real_range((int *)random_math_get_local_seed_address(),
                                 -1.0f, 1.0f);
    FUN_00109e90(matrix, random_c * scripted->field_0c[0] * scale,
                 random_b * scripted->field_0c[1] * scale,
                 random_a * scripted->field_0c[2] * scale);

    random_a = random_real_range((int *)random_math_get_local_seed_address(),
                                 -1.0f, 1.0f);
    random_b = random_real_range((int *)random_math_get_local_seed_address(),
                                 -1.0f, 1.0f);
    random_c = random_real_range((int *)random_math_get_local_seed_address(),
                                 -1.0f, 1.0f);
    ((real_matrix4x3 *)matrix)->position.x =
      random_c * scripted->field_00[1] * scale;
    ((real_matrix4x3 *)matrix)->position.y =
      random_b * scripted->field_00[0] * scale;
    ((real_matrix4x3 *)matrix)->position.z =
      random_a * scripted->field_00[2] * scale;
    return;
  }

  effect = (player_effect_t *)player_effect_get(local_player_index);
  effect_fields = effect->field_00;

  ticks = effect->field_e0;
  if (ticks > 0 || (effect->field_e8 & 2) != 0) {
    if ((effect->field_e8 & 2) != 0) {
      intensity = 1.0f;
    } else {
      duration = effect->field_50;
      zero_scale = effect->field_68;
      intensity = transition_function_evaluate(
                    (int16_t)effect->field_54,
                    1.0f - (duration - (float)ticks) / duration) *
                  zero_scale;
    }
    effect->field_e8 &= 0xfd;

    up = global_up_vector_ptr;
    axis[0] = effect_fields[2] * up[1] - up[2] * effect_fields[1];
    axis[1] = up[2] * effect_fields[0] - effect_fields[2] * up[0];
    axis[2] = effect_fields[1] * up[0] - effect_fields[0] * up[1];

    angle = intensity * effect->field_58;
    FUN_001092d0((float *)&effect_matrix, axis, x87_fsin(angle),
                 x87_fcos(angle));

    translation = intensity * effect->field_5c;
    effect_matrix.position.x = translation * effect_fields[0];
    effect_matrix.position.y = translation * effect_fields[1];
    effect_matrix.position.z = translation * effect_fields[2];
    effect_matrix.position.x =
      intensity * effect->field_0c[0] + effect_matrix.position.x;
    effect_matrix.position.y =
      intensity * effect->field_0c[1] + effect_matrix.position.y;
    effect_matrix.position.z =
      intensity * effect->field_0c[2] + effect_matrix.position.z;

    effect->field_e0 -= game_time_get_elapsed();
    source = &effect_matrix;
  } else {
    source = global_identity4x3;
  }
  *(real_matrix4x3 *)matrix = *source;

  ticks = effect->field_e2;
  if (ticks > 0 || (effect->field_e8 & 4) != 0) {
    effect_matrix = *global_identity4x3;

    if ((effect->field_e8 & 4) != 0) {
      intensity = 1.0f;
    } else {
      intensity = transition_function_evaluate(
                    (int16_t)effect->field_88,
                    1.0f - (effect->field_84 - (float)ticks) /
                             effect->field_84) *
                  effect->field_ac;
    }

    level = ((1.0f - effect->field_a8) +
             FUN_0010a5e0((int16_t)effect->field_a0,
                          (effect->field_84 -
                           (float)effect->field_e2) /
                            effect->field_a4) *
               effect->field_a8) *
            intensity;

    translation = level * effect->field_8c > 0.0f ?
                    level * effect->field_8c : 0.0f;
    rotation = level * effect->field_90 > 0.0f ?
                 level * effect->field_90 : 0.0f;
    effect->field_e8 &= 0xfb;
    rumble = effect->field_cc;
    get_shake_matrix((float *)&effect_matrix,
                     translation + rumble[2],
                     rotation + effect->field_cc[3]);
    rumble_player_continuous(local_player_index, *(int *)&rumble[0],
                             *(int *)&rumble[1]);

    effect->field_dc += game_time_get_elapsed();
    if (effect->field_dc > 0) {
      effect->field_dc = 0;
      csmemset(rumble, 0, sizeof(effect->field_cc));
    }
    get_shake_matrix((float *)&effect_matrix, translation, rotation);
    effect->field_e2 -= game_time_get_elapsed();
    matrix4x3_multiply(matrix, (float *)&effect_matrix, matrix);
  }
}

/* player_effect_apply_damage (0xa3b80) — Apply damage-related effects to a
 * player.
 *
 * Uses the damage effect tag (jpt!) to set screen shake, vibration, and
 * directional damage indicators based on the angle of incoming damage
 * relative to the player's camera orientation.
 *
 * Confirmed: datum_get(*(data_t**)0x5aa6d4, player_handle) for player data.
 * Confirmed: assert_halt on direction != NULL.
 * Confirmed: lock_random_seed / unlock_random_seed bracket the entire function.
 * Confirmed: tag_get('jpt!', *damage_params) for tag lookup.
 * Confirmed: player_effect_update_screen_flash(sVar1, effect, param_4, 1.0f,
 * jpt+0x24). Confirmed: *(unsigned int*)(player+0x1c8) & 0x100 checks vehicle
 * driver flag. Confirmed: Global floats: 0x2533c0=0.0f, 0x2533c8=1.0f,
 * 0x25fea8=~0.0, 0x254a58=~0.7854 (PI/4), 0x26af48=~2.3562 (3*PI/4),
 * 0x2568bc=~1.5708 (PI/2). Confirmed: local_player_get_player_index called
 * twice (original binary artifact). Confirmed: camera+0x20 is forward vector,
 * +0x2c is up vector. Confirmed: effect flags at +0xe4 (right), +0xe5
 * (forward), +0xe6 (down), +0xe7 (side).
 */
void player_effect_start(int player_handle, void *damage_params,
                         void *direction, float damage_amount, float scale)
{
  char *player;
  int16_t unit_index;
  char *jpt_tag;
  player_effect_t *effect;
  int driver_handle;
  int driver_type_valid;
  int damage_type_valid;
  void *camera;
  float attacker_pos[3];
  vector3_t victim_pos;
  float delta[3];
  float rotated_delta[3];
  float length;
  float angle;

  player = (char *)datum_get(player_data, player_handle);
  unit_index = *(int16_t *)(player + 2);

  if ((int)direction == 0) {
    assert_halt_msg_at("direction", "c:\\halo\\SOURCE\\effects\\player_effects.c", 0x156, 0);
  }

  lock_global_random_seed();

  if (unit_index != -1) {
    jpt_tag = (char *)tag_get(0x6a707421, *(int *)damage_params);
    effect = (player_effect_t *)player_effect_get(unit_index);

    player_effect_update_screen_flash(unit_index, (char *)effect,
                                      damage_amount, 1.0f,
                                      (void *)(jpt_tag + 0x24) /* @<ebx> */);
    player_effect_update_camera_impulse(unit_index, (float *)(jpt_tag + 0x98),
                                        direction, damage_amount, 1.0f,
                                        (float *)effect /* @<eax> */);
    player_effect_update_camera_shake(unit_index, damage_amount, 1.0f,
                                      (float *)(jpt_tag + 0xcc) /* @<eax> */,
                                      (void *)effect /* @<ebx> */);
    rumble_player_impulse((short)unit_index, (float *)(jpt_tag + 0x5c),
                          damage_amount, 1.0f);

    if (*(int *)(jpt_tag + 0x120) != -1) {
      sound_impulse_start(*(int *)(jpt_tag + 0x120), 1.0f);
    }

    if ((*(float *)0x2533c0 < scale) &&
        (*(int *)((char *)damage_params + 0xc) != -1)) {
      if ((*(unsigned int *)(jpt_tag + 0x1c8) & 0x100) != 0) {
        effect->field_e4[2] = 1;
        unlock_global_random_seed();
        return;
      }

      driver_handle = local_player_get_player_index(unit_index);
      if (driver_handle == -1) {
        driver_handle = -1;
      } else {
        driver_handle = local_player_get_player_index(unit_index);
        player = (char *)datum_get(player_data, driver_handle);
        driver_handle = *(int *)(player + 0x34);
      }

      driver_type_valid =
        (int)object_try_and_get_and_verify_type(driver_handle, 3) != 0;
      damage_type_valid = (int)object_try_and_get_and_verify_type(
                            *(int *)((char *)damage_params + 0xc), -1) != 0;

      if (driver_type_valid && damage_type_valid) {
        camera = observer_get_camera(unit_index);
        if (camera != (void *)0) {
          unit_get_head_position(driver_handle, attacker_pos);
          object_get_world_position(*(int *)((char *)damage_params + 0xc),
                                    &victim_pos);

          delta[0] = victim_pos.x - attacker_pos[0];
          delta[1] = victim_pos.y - attacker_pos[1];
          delta[2] = victim_pos.z - attacker_pos[2];

          cross_product3d((float *)((char *)camera + 0x20),
                          (float *)((char *)camera + 0x2c), attacker_pos);

          rotated_delta[0] = attacker_pos[0] * delta[0] +
                             attacker_pos[1] * delta[1] +
                             attacker_pos[2] * delta[2];
          rotated_delta[1] = delta[0] * *(float *)((char *)camera + 0x20) +
                             delta[1] * *(float *)((char *)camera + 0x24) +
                             delta[2] * *(float *)((char *)camera + 0x28);
          rotated_delta[2] = delta[0] * *(float *)((char *)camera + 0x2c) +
                             delta[1] * *(float *)((char *)camera + 0x30) +
                             delta[2] * *(float *)((char *)camera + 0x34);

          length = normalize3d(rotated_delta);
          if (length != 0.0f) {
            if ((0.0f < fabsf(rotated_delta[2]))) {
              if (rotated_delta[2] <= 0.0f) {
                effect->field_e4[2] = 1;
              } else {
                effect->field_e4[0] = 1;
              }
            }

            angle = (float)atan2(rotated_delta[1], rotated_delta[0]);
            if ((angle < *(float *)0x254a58) || (*(float *)0x26af48 < angle)) {
              if ((*(float *)0x2568bc < fabsf(angle))) {
                effect->field_e4[1] = 1;
                unlock_global_random_seed();
                return;
              }
              effect->field_e4[3] = 1;
            }
          }
        }
      }
    }
  }

  unlock_global_random_seed();
}
