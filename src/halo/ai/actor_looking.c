/* actor_looking.c — AI actor looking/targeting subsystem.
 *
 * Corresponds to actor_looking.obj (or the actor_looking portion of the
 * compiled AI module).
 * Assertion path: c:\halo\SOURCE\ai\actor_looking.c
 */

#include "../../common.h"

/* FUN_00014540 (0x14540)
 * Initialize actor looking state from the scripted look target at activation.
 *
 * Called during actor activation (FUN_0003ec80) after prop and movement init.
 * Reads the actor's scripted-look target handle at actor+0x1dc.  If set (not
 * -1) and the actor also has a secondary-look object handle at actor+0x1e0,
 * it attempts to find an existing look-at entry for that object via
 * prop_get_active_by_unit_index.  If one is found it is passed as an
 * object-handle look target (type 1); otherwise the object's position is
 * fetched via unit_get_head_position and a position look target (type 3) is
 * issued via FUN_00027a60.
 *
 * Confirmed: datum_get(actor_data, actor_handle) at 0x14552.
 * Confirmed: guard on [actor+0x1dc] != -1 AND [actor+0x1e0] != -1 at
 *   0x14559/0x14567.
 * Confirmed: prop_get_active_by_unit_index(actor_handle, [actor+0x1e0]) at
 * 0x14574. Confirmed: branch on return == -1 at 0x1457c/0x1457f. Confirmed:
 * type=1 path: MOV word [EBP-0x10],1; MOV [EBP-0xc],EAX at 0x14581/0x14587.
 * Confirmed: type=3 path: MOV word [EBP-0x10],3 at 0x14597; CALL 0x1a9200
 *   with args ([actor+0x1e0], &buf[1]) at 0x1459d.
 * Confirmed: FUN_00027a60(actor_handle, 8, 5, &buf) at 0x145ae.
 * Confirmed: cdecl: ADD ESP,0x10 after FUN_00027a60; ADD ESP,0x8 after
 *   prop_get_active_by_unit_index and unit_get_head_position. */
void FUN_00014540(int actor_handle)
{
  char *actor;
  int look_object; /* [actor+0x1e0]: handle of scripted look object */
  int look_entry; /* return from prop_get_active_by_unit_index: existing look
                     entry or -1 */

  /* Buffer passed to FUN_00027a60: { int16_t type; int16_t pad; int data[3]; }
   * type=1 (object handle look): data[0] = look_entry handle
   * type=3 (position look):      data[0..2] = xyz position from
   * unit_get_head_position Total: 2 + 2(pad) + 12 = 16 bytes (0x10), matching
   * SUB ESP,0x10 */
  short look_buf[8]; /* 16 bytes on stack: [0]=type word, [2..7]=data dwords */

  actor = (char *)datum_get(actor_data, actor_handle);

  if (*(int *)(actor + 0x1dc) == -1)
    return;
  if (*(int *)(actor + 0x1e0) == -1)
    return;

  look_object = *(int *)(actor + 0x1e0);

  look_entry = prop_get_active_by_unit_index(actor_handle, look_object);
  if (look_entry == -1) {
    /* No existing look entry: use object's world position (type 3) */
    look_buf[0] = 3;
    unit_get_head_position(look_object, (float *)&look_buf[2]);
  } else {
    /* Existing look entry found: use it as an object-handle target (type 1) */
    look_buf[0] = 1;
    *(int *)&look_buf[2] = look_entry;
  }

  FUN_00027a60(actor_handle, 8, 5, look_buf);
}

/* FUN_000145c0 (0x145c0)
 * If actor+0x1dc is a valid conversation handle, finish it via
 * ai_conversation_finish.  Called on actor deactivation. */
void FUN_000145c0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x1dc) != -1) {
    ai_conversation_finish(*(int *)(actor + 0x1dc), 0, 0);
  }
}

/* FUN_000145f0 (0x145f0)
 * Same as FUN_000145c0 — second copy for a different calling context. */
void FUN_000145f0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x1dc) != -1) {
    ai_conversation_finish(*(int *)(actor + 0x1dc), 0, 0);
  }
}

/* FUN_000146f0 (0x146f0)
 * Initialize actor looking state: copy seed byte, set look priority/spec/frame
 * type fields, clear look state flags, optionally override priority. */
void FUN_000146f0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  *(char *)(actor + 0x426) = *(char *)(actor + 0x358);
  *(short *)(actor + 0x3e8) = 5;
  *(short *)(actor + 0x3ec) = 2;
  *(short *)(actor + 0x3fc) = 4;
  *(char *)(actor + 0x427) = 0;
  *(char *)(actor + 0x428) = 0;
  *(char *)(actor + 0x424) = 0;
  *(char *)(actor + 0x425) = 0;
  if (*(short *)(actor + 0x15e) != 4 && *(short *)(actor + 0x6e) >= 4) {
    *(char *)(actor + 0x454) = 1;
    *(short *)(actor + 0x3e8) = 7;
  }
}

/* FUN_00014b40 (0x14b40)
 * If actor has an associated unit (actor+0x18 != -1), stop it running blindly.
 */
void FUN_00014b40(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0x18) != -1) {
    unit_stop_running_blindly(*(int *)(actor + 0x18));
  }
}

/* FUN_00014b70 (0x14b70)
 * Set actor control-override handle to 0xffff and controlled flag to 1. */
void FUN_00014b70(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  *(short *)(actor + 0xa4) = (short)0xffff;
  *(char *)(actor + 0xa2) = 1;
}

/* FUN_00014ff0 (0x14ff0)
 * If actor+0xb8 equals param_2 (current look-frame id), replace it with
 * param_3. */
void FUN_00014ff0(int actor_handle, int param_2, int param_3)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  if (*(int *)(actor + 0xb8) == param_2) {
    *(int *)(actor + 0xb8) = param_3;
  }
}

/* FUN_00015020 (0x15020)
 * Return 1 if param_1 is in the exclusive range (8, 13), else 0.
 * Confirmed: 16-bit load (MOV AX,[EBP+8]) at 0x15023; CMP AX,9 / CMP AX,0xC. */
int FUN_00015020(short param_1)
{
  if (param_1 >= 9 && param_1 <= 0xc) {
    return 1;
  }
  return 0;
}

/* Compute the cross product of two 3D vectors.
 *
 * out = a × b
 *
 * Confirmed: pure x87 arithmetic, no assertions, no globals.
 * Confirmed: FLD [ECX] / FMUL [EAX+0x4] / FLD [ECX+0x4] / FMUL [EAX] / FSUBP
 *   computes out[2] = a[0]*b[1] - a[1]*b[0] (z component).
 * Confirmed: three-component store at [EAX], [EAX+0x4], [EAX+0x8].
 */
void cross_product3d(float *a, float *b, float *out)
{
  /* Load all inputs before any store so b==out (in-place) is safe.
   * The binary (0x178d0) holds all three FPU results on the x87 stack
   * before the first FSTP, giving the same aliasing guarantee. */
  float a0 = a[0], a1 = a[1], a2 = a[2];
  float b0 = b[0], b1 = b[1], b2 = b[2];
  out[0] = a1 * b2 - a2 * b1;
  out[1] = a2 * b0 - a0 * b2;
  out[2] = a0 * b1 - a1 * b0;
}

/* FUN_0002a2b0 (0x2a2b0)
 * Update the actor's look-direction validity flag (actor+0x505).
 *
 * Reads the actor record and the look-specification slot at actor+0x3ec.
 * If the spec type word (actor+0x3ec) is 0 (no look command active) and the
 * actor is not in a vehicle (FUN_0002a3d0 returns 0), the look-priority word
 * at actor+0x3e8 is reset to 0.
 *
 * If the look-priority (actor+0x3e8) is > 2 AND a look spec is active
 * (actor+0x3ec != 0), FUN_00028660 is called with EBX=&actor[0x3ec] and
 * EDI=&actor[0x524] (the look-direction float[3] output buffer).  On success
 * actor+0x505 is set to 1; on failure or when no look spec is active it is
 * set to 0.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data=DAT_006325a4, actor_handle) at 0x2a2c0.
 * Confirmed: EBX = &actor[0x3ec] via LEA EBX,[ESI+0x3ec] at 0x2a2c7.
 * Confirmed: CMP word [EBX],0x0 at 0x2a2d0; JNZ 0x2a2ec.
 * Confirmed: CALL FUN_0002a3d0(actor_handle) at 0x2a2d7; TEST AL,AL.
 * Confirmed: MOV word [ESI+0x3e8],0x0 at 0x2a2e3 when spec==0 && not mounted.
 * Confirmed: CMP word [ESI+0x3e8],0x3 (JL skip) at 0x2a2ec; CMP [EBX],0x0 (JZ
 * skip). Confirmed: LEA EDI,[ESI+0x524] at 0x2a2ff; PUSH actor_handle; CALL
 * 0x28660. Confirmed: FUN_00028660 register args: EBX=short*look_spec,
 * EDI=float*direction. Confirmed: MOV byte [ESI+0x505],0x1 on success at
 * 0x2a313; RET. Confirmed: MOV byte [ESI+0x505],0x0 on fallthrough at 0x2a31f;
 * RET. Inferred: actor+0x3e8 = look priority (int16_t); actor+0x3ec = look spec
 * type (int16_t). Inferred: actor+0x505 = look-direction valid flag (char).
 * Inferred: actor+0x524 = computed look direction (float[3]). */
void FUN_0002a2b0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  short *look_spec = (short *)(actor + 0x3ec);

  if (*look_spec == 0) {
    if (!FUN_0002a3d0(actor_handle))
      *(short *)(actor + 0x3e8) = 0;
  }
  if (*(short *)(actor + 0x3e8) > 2 && *look_spec != 0) {
    if (FUN_00028660(actor_handle, look_spec, (float *)(actor + 0x524))) {
      actor[0x505] = 1;
      return;
    }
  }
  actor[0x505] = 0;
}

/* FUN_0002a3d0 (0x2a3d0)
 * Return the in-vehicle / mounted flag byte for the actor.
 *
 * Looks up the actor record via datum_get(actor_data, actor_handle) and
 * returns the byte at actor+0x4a8.  Non-zero means the actor is currently
 * mounted in or on a vehicle.
 *
 * Confirmed: cdecl, single stack arg (actor_handle).
 * Confirmed: datum_get(actor_data=DAT_006325a4, actor_handle) at 0x2a3de.
 * Confirmed: MOV AL,byte ptr [EAX+0x4a8] at 0x2a3e3; ADD ESP,0x8; RET. */
char FUN_0002a3d0(int actor_handle)
{
  char *actor = (char *)datum_get(actor_data, actor_handle);
  return actor[0x4a8];
}
