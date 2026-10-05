#include "x87_math.h"
#include "../math/real_math.h"

/* C fmod: VC71 emits the original CALL _CIfmod (0x1daf7e); clang uses the
 * equivalent FPREM loop. */
#if defined(_MSC_VER) && !defined(__clang__)
double __cdecl fmod(double, double);
#define VEHICLE_FMOD(val, div) ((real)fmod((double)(val), (double)(div)))
#else
#define VEHICLE_FMOD(val, div) x87_fmod((val), (double)(div))
#endif
/* FUN_001b5400 (0x1b5400)
 * Iterates objects with type mask 3. For each object whose +0xcc datum equals
 * vehicle_handle, copies and lowercases tag-block element (+0x2e4) name +4.
 * The current object's datum is iter.last_handle (+0x08), written by
 * object_iterator_next; do not replace it with the object pointer.
 */
uint16_t FUN_001b5400(int vehicle_handle, const char *name_filter)
{
  uint32_t exit_count;
  object_iter_t iter;
  void *unit_tag;
  void *object;
  void *tag_element;
  char name_buf[256];
  int filter_is_empty;

  exit_count = 0;
  if (vehicle_handle != -1) {
    unit_tag = tag_get(0x756e6974,
                       *(int *)object_get_and_verify_type(vehicle_handle, 3));
    filter_is_empty = name_filter == NULL || csstrlen(name_filter) == 0;
    object_iterator_new(&iter, 3, 0);
    object = object_iterator_next(&iter);
    while (object != NULL) {
      if (*(int *)((char *)object + 0xcc) == vehicle_handle) {
        tag_element = tag_block_get_element(
          (char *)unit_tag + 0x2e4, (int)*(int16_t *)((char *)object + 0x2a0),
          0x11c);
        csstrcpy(name_buf, (char *)tag_element + 4);
        csstr_tolower(name_buf);
        if ((filter_is_empty || crt_strstr(name_buf, name_filter) != NULL) &&
            unit_try_and_exit_seat(iter.last_handle) != 0) {
          exit_count++;
        }
      }
      object = object_iterator_next(&iter);
    }
  }

  return (uint16_t)exit_count;
}

/* FUN_001b5500 (0x1b5500) */
void FUN_001b5500(int param_1)
{
  char *unit;

  if (param_1 != -1) {
    unit = (char *)object_get_and_verify_type(param_1, 3);
    if (*(int *)(unit + 0xcc) != -1 && *(int16_t *)(unit + 0x2a0) != -1) {
      unit_try_and_exit_seat(param_1);
    }
  }
}

/* vehicles_initialize_for_new_map (0x1b5550)
 *
 * Confirmed: the body is a single RET (C3).
 * Confirmed: only reference is the vehicle object_type_definition table at
 *   0x323de8, slot +0x18 (initialize_for_new_map); name from PAL 2342
 *   objects/object_types.c vehicle_data_definition (T2). */
void vehicles_initialize_for_new_map(void)
{
}

/* vehicles_dispose_from_old_map (0x1b5560)
 *
 * Confirmed: the body is a single RET (C3).
 * Confirmed: only reference is the vehicle object_type_definition table at
 *   0x323de8, slot +0x1c (dispose_from_old_map); name from PAL 2342 (T2). */
void vehicles_dispose_from_old_map(void)
{
}

/* FUN_001b5580 (0x1b5580) */
void vehicle_causes_collision_damage(int param_1, void *param_2)
{
  unit_place(param_1, (char *)param_2 + 0x48);
  object_add_scenario_permutation(param_1, (char *)param_2 + 0x28);
}

/* vehicle_hover (0x1b55c0) — does this vehicle's definition mark it as a
 * hovering vehicle?
 *
 * Confirmed: MOV EAX,[EBP+0x8]; PUSH 0x2; PUSH EAX ->
 * object_get_and_verify_type(vehicle_handle, 2) (type mask 2 = vehicle).
 * Confirmed: MOV ECX,[EAX] -> obj->tag_index (uint32 at object offset 0).
 * Confirmed: PUSH ECX; PUSH 0x76656869 -> tag_get('vehi', tag_index).
 * Confirmed: MOV EAX,[EAX+0x2f0]; SHR EAX,0x7; AND EAX,0x1 -> bit 7 of the
 * 'vehi' tag flags dword at +0x2f0 (the same dword actor_combat.c tests with
 * & 0x100).
 * Confirmed: ADD ESP,0x10 -> both cdecl cleanups (2 calls x 2 args) coalesced.
 * Confirmed: plain RET (cdecl, caller cleans); result returned in EAX.
 * Inferred: name "vehicle_hover" from kb.json; the specific flag bit meaning
 * is unproven beyond "bit 7 of the vehi definition flags".
 */
bool vehicle_hover(int vehicle_handle)
{
  void *vehicle;
  void *vehicle_tag;

  vehicle = object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = tag_get(0x76656869, *(uint32_t *)vehicle);
  return (bool)((*(uint32_t *)((char *)vehicle_tag + 0x2f0) >> 7) & 1);
}

/* FUN_001b5610 (0x1b5610) */
void FUN_001b5610(int vehicle_handle, uint8_t param_2)
{
  char *vehicle;

  if (vehicle_handle != -1) {
    vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
    if (param_2 != 0) {
      object_get_world_position(vehicle_handle, (vector3_t *)(vehicle + 0x454));
      *(uint8_t *)(vehicle + 0x424) |= 2;
      return;
    }
    *(uint8_t *)(vehicle + 0x424) &= 0xfd;
  }
}

/* vehicle_is_flipped (0x1b5680) — reports whether a vehicle has rolled onto
 * its side or back, based on the float at object+0x38.
 *
 * Confirmed: MOV EAX,[EBP+0x8]; PUSH 0x2; PUSH EAX ->
 * object_get_and_verify_type(vehicle_handle, 2) (type mask 2 = vehicle, the
 * same mask vehicle_hover/vehicle_reset/FUN_001b56b0 use). kb.json's prior
 * "void vehicle_is_flipped(void)" decl was wrong: the function takes one
 * cdecl dword argument (the vehicle handle) and returns a bool in EAX.
 * Confirmed: FLD [EAX+0x38]; FCOMP [0x2549d4] (pooled constant, confirmed
 * 0.2f elsewhere -- see units.c:1768) -> compares the float at vehicle+0x38
 * against 0.2f.
 * Confirmed: FNSTSW AX; TEST AH,0x5; JP -> the parity trick for an x87
 * "less than" test. Per FCOM condition codes, ST0<src sets C0=1/C2=0 (AH&5 =
 * 1, odd parity, PF=0, JP NOT taken); ST0==src, ST0>src, and unordered all
 * set AH&5 to an even-parity value (PF=1, JP taken). So EAX=1 (JP not taken,
 * falls to MOV EAX,1) only when the field is strictly less than 0.2f; EAX=0
 * (JP taken, XOR EAX,EAX) for >=, ==, or NaN.
 * Inferred: +0x38 is the Z component of the vector3 at object+0x30 (types.h
 * unk_48, offset/meaning unproven); a small Z reading "flipped" when
 * < 0.2f is consistent with an up-vector upright test, but the vector's
 * identity is not proven here, so it is left as a raw offset rather than a
 * struct field.
 */
bool vehicle_is_flipped(int vehicle_handle)
{
  int result;
  void *vehicle;

  vehicle = object_get_and_verify_type(vehicle_handle, 2);
  result = *(float *)((char *)vehicle + 0x38) < *(float *)0x2549d4;
  return (bool)result;
}

/* FUN_001b56b0 (0x1b56b0) — per-tick contact bookkeeping for a vehicle,
 * driven by the caller's mass-point state array (passed in EDI).
 *
 * Confirmed: PUSH 0x2; PUSH EAX -> object_get_and_verify_type(handle@<eax>, 2)
 * (type mask 2 = vehicle, the same mask vehicle_hover uses).
 * Confirmed: MOV ECX,[ESI]; PUSH ECX; PUSH 0x76656869 ->
 * tag_get('vehi', obj->tag_index).
 * Confirmed: MOV EDX,[EAX+0x8c]; PUSH EDX; PUSH 0x70687973 ->
 * tag_get('phys', vehi_tag->+0x8c).
 * Confirmed: ADD ESP,0x18 -> the three cdecl cleanups (3 calls x 2 args)
 * coalesced into one 24-byte adjust; this is NOT a 6-argument call.
 * Confirmed: MOV CL,[ESI+0x428]; CMP CL,0xff; JNC -> unsigned saturating
 * increment of the byte counter at obj+0x428.
 * Confirmed: loop counter is 16-bit (INC EDX; MOVSX ECX,DX), element stride is
 * 0x130 (IMUL ECX,ECX,0x130) over the incoming EDI base, and the bound is
 * re-read from phys_tag+0x74 on every iteration (CMP ECX,[EAX+0x74]; JL).
 * Confirmed: TEST CL,0x2 / TEST CL,0x10 -> bits 1 and 4 of the element's first
 * dword; bit 1 clears obj+0x428, saturating-increments obj+0x42b and returns;
 * bit 4 only clears obj+0x428.
 * Confirmed: falling out of the loop stores 0 to obj+0x42b.
 * Unknown: the element type behind EDI (stride 0x130, only its first dword is
 * read here), which 'phys' tag_block owns the count at +0x74, and the meaning
 * of the two saturating byte counters at obj+0x428 / obj+0x42b.
 */
void FUN_001b56b0(int vehicle_handle, void *state_array)
{
  char *vehicle;
  void *vehicle_tag;
  char *physics_tag;
  uint32_t flags;
  int16_t i;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = tag_get(0x76656869, *(int32_t *)vehicle);
  physics_tag =
    (char *)tag_get(0x70687973, *(int32_t *)((char *)vehicle_tag + 0x8c));

  if (*(uint8_t *)(vehicle + 0x428) < 0xff) {
    (*(uint8_t *)(vehicle + 0x428))++;
  }

  for (i = 0; i < *(int32_t *)(physics_tag + 0x74); i++) {
    flags = *(uint32_t *)((char *)state_array + i * 0x130);
    if ((flags & 2) != 0) {
      *(uint8_t *)(vehicle + 0x428) = 0;
      if (*(uint8_t *)(vehicle + 0x42b) < 0xff) {
        (*(uint8_t *)(vehicle + 0x42b))++;
      }
      return;
    }
    if ((flags & 0x10) != 0) {
      *(uint8_t *)(vehicle + 0x428) = 0;
    }
  }

  *(uint8_t *)(vehicle + 0x42b) = 0;
}

/*
 * set_real_quaternion (0x1b5750) - store four floats into a real_quaternion.
 *
 * Confirmed: cdecl, EBP frame, no calls. [EBP+0x8] is the destination
 * pointer; the four remaining dword slots are written to +0x0, +0x4, +0x8,
 * +0xc in argument order. Slot [EBP+0xc] is moved with FLD/FSTP, proving the
 * arguments are floats; MSVC bit-copies the other three with MOV because no
 * conversion is needed.
 * Inferred: the component names i/j/k/w follow the real_quaternion field
 * order; the artifact only proves the offsets, not the names. There are no
 * callers in this build, so the argument-to-component mapping is unverified
 * beyond the store offsets.
 */
void set_real_quaternion(float *out, float i, float j, float k, float w)
{
  out[0] = i;
  out[1] = j;
  out[2] = k;
  out[3] = w;
}

/*
 * vehicle_reset (0x1b5770) — clear the vehicle-specific state block that
 * lives at object +0x424 .. +0x47b.
 *
 * Confirmed: MOV EAX,[EBP+0x8]; PUSH 0x2; PUSH EAX -> the kb.json decl
 * "void vehicle_reset(void)" was wrong; the function takes one cdecl dword
 * argument, the vehicle handle, and calls
 * object_get_and_verify_type(vehicle_handle, 2) (mask 2 = vehicle, the same
 * mask vehicle_hover and FUN_001b56b0 use). ADD ESP,0x14 covers only the two
 * calls (2 + 3 dwords), so the parameter is caller-cleaned cdecl.
 * Confirmed: XOR EBX,EBX then every store below writes BX/BL/EBX, i.e. the
 * whole block is zeroed with integer zero.
 * Confirmed store widths from the disassembly: word at +0x424 and +0x426,
 * bytes at +0x428..+0x42b, dwords at +0x42c..+0x440, then +0x448 BEFORE
 * +0x444 (MOV [ESI+0x448] at 0x1b57d8 precedes MOV [ESI+0x444] at 0x1b57de);
 * that inverted pair is preserved here.
 * Confirmed: PUSH 0x8; LEA ECX,[ESI+0x44c]; PUSH EBX; PUSH ECX; CALL 0x8db80
 * -> csmemset(vehicle + 0x44c, 0, 8). Only 8 bytes, so +0x454..+0x45f are
 * deliberately left alone (FUN_001b5610 writes a world position at +0x454).
 * Confirmed: dwords +0x460..+0x478 zeroed after the csmemset call.
 * Unknown: the meaning of every field here except +0x428 / +0x42b, which
 * FUN_001b56b0 uses as saturating contact counters.
 */
void vehicle_reset(int vehicle_handle)
{
  char *vehicle;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);

  *(uint16_t *)(vehicle + 0x424) = 0;
  *(uint16_t *)(vehicle + 0x426) = 0;
  *(uint8_t *)(vehicle + 0x428) = 0;
  *(uint8_t *)(vehicle + 0x429) = 0;
  *(uint8_t *)(vehicle + 0x42a) = 0;
  *(uint8_t *)(vehicle + 0x42b) = 0;
  *(uint32_t *)(vehicle + 0x42c) = 0;
  *(uint32_t *)(vehicle + 0x430) = 0;
  *(uint32_t *)(vehicle + 0x434) = 0;
  *(uint32_t *)(vehicle + 0x438) = 0;
  *(uint32_t *)(vehicle + 0x43c) = 0;
  *(uint32_t *)(vehicle + 0x440) = 0;
  *(uint32_t *)(vehicle + 0x448) = 0;
  *(uint32_t *)(vehicle + 0x444) = 0;

  csmemset(vehicle + 0x44c, 0, 8);

  *(uint32_t *)(vehicle + 0x460) = 0;
  *(uint32_t *)(vehicle + 0x464) = 0;
  *(uint32_t *)(vehicle + 0x468) = 0;
  *(uint32_t *)(vehicle + 0x46c) = 0;
  *(uint32_t *)(vehicle + 0x470) = 0;
  *(uint32_t *)(vehicle + 0x474) = 0;
  *(uint32_t *)(vehicle + 0x478) = 0;
}

/*
 * vehicle_new (0x1b5820) — initialize a freshly created vehicle object.
 *
 * Confirmed: MOV EBX,[EBP+0x8]; PUSH 0x2; PUSH EBX ->
 * object_get_and_verify_type(vehicle_handle, 2) (mask 2 = vehicle, the same
 * mask vehicle_reset / vehicle_hover use). kb.json's prior
 * "void vehicle_new(void)" was wrong; the binary reads one stack parameter.
 * Confirmed: MOV EAX,[ESI]; PUSH EAX; PUSH 0x76656869 ->
 * tag_get('vehi', obj->tag_index).
 * Confirmed: PUSH EBX; CALL 0x001b5770 -> vehicle_reset(vehicle_handle), one
 * stack dword. ADD ESP,0x14 (5 dwords) is the COALESCED cdecl cleanup for all
 * three calls (2 + 2 + 1); it is not a 5-argument call to vehicle_reset.
 * Confirmed: MOV ECX,[EDI+0x8c]; OR EAX,-1; CMP ECX,EAX; JNZ -> the 'vehi'
 * definition's physics tag index at +0x8c compared against -1 (no physics).
 * When absent, bit 0x20 of the object dword at +0x4 is set; when present it is
 * cleared. The compare is then RE-DONE at 0x1b5866 against the same [EDI+0x8c]
 * — two separate tests in the reference, kept as two ifs here.
 * Confirmed: FLD [EDI+0x4]; FMUL [0x253398]; FADD [ESI+0x14]; FSTP [ESI+0x14]
 * -> obj+0x14 += vehi_def+0x4 * 0.5f, in that x87 operand order. 0x253398 is
 * the shared 0.5f constant used across projectiles.c / scenario.c.
 * Confirmed: MOV AL,0x1 with EAX live as 0xffffffff from the earlier OR — a
 * byte-wide write over a live dword is the MSVC bool return ABI, so the
 * function returns true unconditionally.
 * Unknown: the meaning of object+0x14 (a float accumulated by half the vehi
 * definition's +0x4 field) and of flag bit 0x20 at object+0x4.
 */
bool vehicle_new(int vehicle_handle)
{
  char *vehicle;
  char *vehicle_tag;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  vehicle_reset(vehicle_handle);

  if (*(int32_t *)(vehicle_tag + 0x8c) == -1) {
    *(uint32_t *)(vehicle + 0x4) |= 0x20;
  } else {
    *(uint32_t *)(vehicle + 0x4) &= 0xffffffdf;
  }

  if (*(int32_t *)(vehicle_tag + 0x8c) != -1) {
    *(float *)(vehicle + 0x14) =
      *(float *)(vehicle_tag + 0x4) * *(float *)0x253398 +
      *(float *)(vehicle + 0x14);
  }

  return true;
}

/*
 * vehicle_preprocess_node_orientations (0x1b5890)
 *
 * Binary reads two stack params ([EBP+8] handle, [EBP+0xc] node data); the
 * kb decl was (void). Object resolved with type mask 2, then tag_get('vehi')
 * and, when vehi+0x44 != -1, tag_get('antr'). Element 0 of the antr block at
 * +0x24 (size 0x74) holds an int count at +0x5c and a short index array at
 * +0x60; each index selects an antr+0x74 animation (size 0xb4, int16 frame
 * count at +0x22). Index [4] is fetched and discarded. The trailing loop
 * walks the block at elem+0x68 (size 0x14, short at +2) with a signed short
 * counter and scales by the object byte array at +0x44c (0xff -> 1.0f).
 * Constants: 0x2533c0 = 0.0f, 0x2533c8 = 1.0f, 0x253398 = 0.5f; 0x261518 is
 * read from its address. Object/tag fields are unnamed (unproven meaning).
 */
void vehicle_preprocess_node_orientations(int vehicle_handle, void *node_data)
{
  char *object;
  char *vehi;
  char *antr;
  char *elem;
  char *anim;
  char *entry;
  short i;
  unsigned char scale_byte;
  float t;
  float v;

  object = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehi = (char *)tag_get(0x76656869 /* 'vehi' */, *(int *)object);
  if (*(int *)(vehi + 0x44) == -1) {
    return;
  }
  antr = (char *)tag_get(0x616e7472 /* 'antr' */, *(int *)(vehi + 0x44));
  if (*(int *)(antr + 0x24) == 0) {
    return;
  }
  elem = (char *)tag_block_get_element(antr + 0x24, 0, 0x74);
  if (elem == (char *)0) {
    return;
  }

  if (*(int *)(elem + 0x5c) > 0) {
    if ((*(short **)(elem + 0x60))[0] != -1) {
      aiming_screen_apply(
        (int)tag_block_get_element(antr + 0x74, (*(short **)(elem + 0x60))[0], 0xb4),
        (float *)elem, *(float *)(object + 0x434), 0.0f, (int)node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 1) {
    if ((*(short **)(elem + 0x60))[1] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, (*(short **)(elem + 0x60))[1], 0xb4);
      t = (triple_product3d((float *)(object + 0x30), (float *)(object + 0x24),
                            (float *)(object + 0x18)) /
             *(float *)(vehi + 0x2f8) +
           1.0f) *
          0.5f;
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      overlay_animation_apply_continuous(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 2) {
    if ((*(short **)(elem + 0x60))[2] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, (*(short **)(elem + 0x60))[2], 0xb4);
      v = *(float *)(object + 0x42c);
      if (v < 0.0f) {
        t = 0.5f - v / *(float *)(vehi + 0x2fc) * 0.5f;
      } else {
        t = (v / *(float *)(vehi + 0x2f8) + 1.0f) * 0.5f;
      }
      overlay_animation_apply_continuous(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 3) {
    if ((*(short **)(elem + 0x60))[3] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, (*(short **)(elem + 0x60))[3], 0xb4);
      t = *(float *)(object + 0x20) * *(float *)(object + 0x2c) +
          *(float *)(object + 0x1c) * *(float *)(object + 0x28) +
          *(float *)(object + 0x18) * *(float *)(object + 0x24);
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      t = t / (float)fabs(*(float *)(vehi + 0x2f8));
      if (t < 0.0f) {
        t = 0.0f;
      } else if (t > 1.0f) {
        t = 1.0f;
      }
      overlay_animation_apply_continuous(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }

  if (*(int *)(elem + 0x5c) > 4) {
    if ((*(short **)(elem + 0x60))[4] != -1) {
      tag_block_get_element(antr + 0x74, (*(short **)(elem + 0x60))[4], 0xb4);
    }
  }

  if (*(int *)(elem + 0x5c) > 5) {
    if ((*(short **)(elem + 0x60))[5] != -1) {
      anim = (char *)tag_block_get_element(antr + 0x74, (*(short **)(elem + 0x60))[5], 0xb4);
      if (*(float *)(vehi + 0x310) > 0.0f) {
        t = *(float *)(object + 0x438) / *(float *)(vehi + 0x310);
      } else {
        t = 0.0f;
      }
      overlay_animation_apply_continuous(anim, (float)(int)*(short *)(anim + 0x22) * t, node_data);
    }
  }

  for (i = 0; i < *(int *)(elem + 0x68); i++) {
    entry = (char *)tag_block_get_element(elem + 0x68, i, 0x14);
    if (*(short *)(entry + 2) != -1) {
      anim =
        (char *)tag_block_get_element(antr + 0x74, *(short *)(entry + 2), 0xb4);
      scale_byte = *(unsigned char *)(object + 0x44c + i);
      if (scale_byte == 0xff) {
        t = 1.0f;
      } else {
        t = (float)scale_byte * *(float *)0x261518;
      }
      overlay_animation_apply_continuous(anim, (float)(*(short *)(anim + 0x22) - 1) * t, node_data);
    }
  }
}

/* global_up_vector_ptr as a vector, re-read at each use like the original. */
#define GLOBAL_UP3D ((const real_vector3d *)global_up_vector_ptr)

/* Largest angular-velocity correction per tick: 0.8 degrees in radians. */
#define VEHICLE_ANGULAR_ACCELERATION 0.0139626348f

/* Inline vector helpers for the physics variants below. The original expands
 * these in place (no CALLs). */
static __inline real vehicle_dot_product2d(const real_vector2d *a,
                                           const real_vector2d *b)
{
  return a->i * b->i + a->j * b->j;
}

static __inline real vehicle_dot_product3d(const real_vector3d *a,
                                           const real_vector3d *b)
{
  return a->i * b->i + a->j * b->j + a->k * b->k;
}

static __inline real vehicle_magnitude_squared3d(const real_vector3d *v)
{
  return v->i * v->i + v->j * v->j + v->k * v->k;
}

/* *current moves toward desired by at most maximum_speed. */
static __inline void vehicle_interpolate_scalar(real *current, real desired,
                                              real maximum_speed)
{
  real difference = desired - *current;

  if (difference < -maximum_speed) {
    difference = -maximum_speed;
  } else if (difference > maximum_speed) {
    difference = maximum_speed;
  }
  *current += difference;
}

/* result = a x b, written component by component (no temporaries). */
static __inline void vehicle_cross_product3d_in_place(const real_vector3d *a,
                                                     const real_vector3d *b,
                                                     real_vector3d *result)
{
  result->i = a->j * b->k - b->j * a->k;
  result->j = a->k * b->i - a->i * b->k;
  result->k = b->j * a->i - a->j * b->i;
}

/* (b x a) . c, the cross product expanded in the original's operand order. */
static __inline real vehicle_triple_product3d(const real_vector3d *a,
                                              const real_vector3d *b,
                                              const real_vector3d *c)
{
  real_vector3d cross;

  cross.i = a->k * b->j - a->j * b->k;
  cross.j = b->k * a->i - a->k * b->i;
  cross.k = a->j * b->i - b->j * a->i;
  return vehicle_dot_product3d(&cross, c);
}

static __inline real_vector3d *vehicle_cross_product3d(const real_vector3d *a,
                                                       const real_vector3d *b,
                                                       real_vector3d *result)
{
  real k = a->i * b->j - a->j * b->i;
  real j = a->k * b->i - a->i * b->k;
  real i = a->j * b->k - a->k * b->j;

  result->i = i;
  result->j = j;
  result->k = k;
  return result;
}

/*
 * vehicle_accelerate (0x1b5c90)
 *
 * Adds an impulse to a vehicle's velocity and, when the vehicle has a
 * physics tag, spins it about (global_up x acceleration) by pi times the
 * horizontal impulse size; then clears object flag bit 5.
 *
 * Confirmed from disassembly at 0x1b5c90 (cdecl, 2 stack args, frame 0xc):
 *   'vehi' +0x8c == -1 -> return with nothing changed. The 'phys' tag_get
 *   result is unused.
 *   translational_velocity += *acceleration; torque = global_up x
 *   acceleration; magnitude = normalize3d(&torque); magnitude > 0 ->
 *   angular_velocity += torque * (magnitude * pi (0x256980)).
 *   flags (+0x04) &= ~0x20 on both paths.
 */
void vehicle_accelerate(int handle, float *velocity)
{
  vehicle_data_t *vehicle =
    (vehicle_data_t *)object_get_and_verify_type(handle, OBJECT_MASK_VEHICLE);
  vehicle_definition_t *definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);

  if (definition->physics.tag_index != -1) {
    const real_vector3d *acceleration = (const real_vector3d *)velocity;
    real_vector3d *object_velocity =
      (real_vector3d *)&vehicle->unit.object.translational_velocity;
    real_vector3d *object_angular_velocity =
      (real_vector3d *)&vehicle->unit.object.angular_velocity;
    real_vector3d torque;
    real magnitude;

    tag_get(TAG_GROUP_PHYS, definition->physics.tag_index);

    object_velocity->i = object_velocity->i + acceleration->i;
    object_velocity->j = object_velocity->j + acceleration->j;
    object_velocity->k = object_velocity->k + acceleration->k;

    vehicle_cross_product3d_in_place(GLOBAL_UP3D, acceleration, &torque);

    magnitude = normalize3d(&torque.i);
    if (magnitude > 0.0f) {
      magnitude = magnitude * 3.14159265f;
      torque.i = torque.i * magnitude;
      torque.j = torque.j * magnitude;
      torque.k = torque.k * magnitude;

      object_angular_velocity->i = torque.i + object_angular_velocity->i;
      object_angular_velocity->j = torque.j + object_angular_velocity->j;
      object_angular_velocity->k = torque.k + object_angular_velocity->k;
    }

    vehicle->unit.object.flags &= ~0x20;
  }
}

/*
 * vehicle_render_debug (0x1b5d90) — debug walk over a vehicle's physics
 * powered-mass-point block.
 *
 * Confirmed from disassembly at 0x1b5d90:
 *   MOV EAX,[EBP+8]; PUSH 0x2; PUSH EAX -> object_get_and_verify_type(handle,
 * 2) (the kb decl said (void); the binary reads one stack parameter). MOV
 * ECX,[EAX]; PUSH ECX; PUSH 0x76656869 -> tag_get('vehi', obj->tag_index). MOV
 * EAX,[EAX+0x8c]; CMP EAX,-1; JZ exit -> 'vehi' tag physics tag index. PUSH
 * EAX; PUSH 0x70687973 -> tag_get('phys', physics_index). MOV CL,[0x005054f4];
 * TEST CL,CL; JZ exit -> unnamed debug byte gate. MOV EAX,[EAX+0x74]; XOR
 * ECX,ECX; INC ECX; MOVSX EDX,CX; CMP EDX,EAX; JL
 *     -> signed short counter over the dword count at phys+0x74.
 * The loop body is empty in this build: nothing is emitted between INC ECX and
 * the compare, so whatever debug drawing it contained produced no code here.
 */
void vehicle_render_debug(int vehicle_handle)
{
  char *vehicle_tag;
  char *physics_tag;
  int physics_index;
  short i;

  vehicle_tag = (char *)tag_get(
    0x76656869, *(int *)object_get_and_verify_type(vehicle_handle, 2));
  physics_index = *(int *)(vehicle_tag + 0x8c);
  if (physics_index != -1) {
    physics_tag = (char *)tag_get(0x70687973, physics_index);
    if (*(char *)0x5054f4 != 0) {
      for (i = 0; (int)i < *(int *)(physics_tag + 0x74); i++) {
      }
    }
  }
}

/*
 * vehicle_get_estimated_position (0x1b5df0) — predict vehicle contact point.
 *
 * For vehicle types that support ground contact estimation (types 0, 1, 4, 6),
 * casts a ray downward from above the vehicle's current position to find the
 * BSP surface beneath it. The estimated position is:
 *   out[i] = dir_vec[i] + fwd_vec_doubled[i] * ray_t
 * where dir_vec = object_pos + up_vec * 0.4 (start above vehicle) and
 * fwd_vec_doubled = fwd_vec * 2 (ray direction/scale).
 *
 * For types 2, 3, 5 (and types >6): returns -1 immediately.
 * Returns result_buf[2] (EAX on success path, opaque hit info) or -1.
 * Callers only check != -1 to know whether the position was estimated.
 *
 * Confirmed: SUB ESP,0x434 — large stack frame.
 * Confirmed: PUSH 0x2, PUSH ESI -> object_get_and_verify_type(handle, 2).
 * Confirmed: MOV EAX,[EAX] -> obj->tag_index (uint32 at offset 0).
 * Confirmed: PUSH EAX, PUSH 0x76656869 -> tag_get('vehi', tag_index).
 * Confirmed: MOVSX EAX,word ptr [EDI+0x2f4] ->
 * (int16_t)vehicle_tag->type_field. Confirmed: CMP EAX,0x6; JA -> default
 * return -1 for types > 6. Confirmed: switch byte table at 0x1b5f18: types
 * 0,1,4,6 -> case body; 2,3,5 -> default. Confirmed: CALL 0x18e3f0
 * (global_collision_bsp_get) with no args. Confirmed: LEA EDX,[EBP-0xc]; PUSH
 * EDX; PUSH ESI -> object_get_world_position(handle, &adj_pos). Confirmed: MOV
 * EAX,[0x31fc44] -> up_vec_ptr = *(float**)0x31fc44 (global_up_vector_ptr).
 * Confirmed: FMUL float[0x253524] -> 0.4f (constant at 0x253524 = 0x3ECCCCCD).
 * Confirmed: adj_pos[i] = up_vec[i] * 0.4 + world_pos[i] (FSTP to
 * EBP-0xc,-0x8,-0x4). Confirmed: MOV EAX,[0x31fc50] -> fwd_vec_ptr =
 * *(float**)0x31fc50 (global_forward_vector_ptr). Confirmed: FADD ST0,ST0 ->
 * fwd_vec[i] * 2; FSTP to EBP-0x18,-0x14,-0x10. Confirmed: PUSH
 * EAX(result_buf), PUSH 0x7f7fffff(FLT_MAX), PUSH ECX(&fwd_doubled), PUSH
 * EDX(&adj_pos), PUSH 0, PUSH 0, PUSH EDI(bsp), PUSH 1 ->
 * collision_bsp_test_vector. Confirmed: TEST AL,AL; JZ -> if ray misses, fall
 * to default return -1. Confirmed: MOV EAX,[EBP-0x42c] -> result_buf[2] loaded
 * as return value. Confirmed: FMUL [EBP-0x434] -> multiply by result_buf[0]
 * (ray t param). Confirmed: out_pos[i] = fwd_doubled[i] * result_buf[0] +
 * adj_pos[i].
 */
int vehicle_get_estimated_position(int vehicle_handle, vector3_t *out_position)
{
  void *bsp;
  float adj_pos[3]; /* object_pos + up_vec * 0.4, at EBP-0xc */
  float fwd_doubled[3]; /* fwd_vec * 2, at EBP-0x18 */
  float result_buf[0x10d]; /* 0x434/4 floats; ray hit result buffer, at
                              EBP-0x434; only [0] and [2] used */
  int default_ret;
  int16_t vtype;

  object_data_t *obj =
    (object_data_t *)object_get_and_verify_type(vehicle_handle, 2);
  void *vehicle_tag = tag_get(0x76656869, *(uint32_t *)obj);
  default_ret = -1;

  /* First call: store current position into out_position (fills in initial
   * value). */
  object_get_world_position(vehicle_handle, out_position);

  /* Switch on vehicle type at tag+0x2f4. */
  vtype = *(int16_t *)((char *)vehicle_tag + 0x2f4);
  switch (vtype) {
  case 0:
  case 1:
  case 4:
  case 6:
    break;
  default:
    return default_ret;
  }

  /* Get BSP for ray cast. */
  bsp = global_collision_bsp_get();

  /* Get vehicle world position into adj_pos. */
  object_get_world_position(vehicle_handle, (vector3_t *)adj_pos);

  /* Compute adjusted start: pos + up_vec * 0.4 */
  {
    float *up = *(float **)0x31fc44;
    adj_pos[0] = up[0] * 0.4f + adj_pos[0];
    adj_pos[1] = up[1] * 0.4f + adj_pos[1];
    adj_pos[2] = up[2] * 0.4f + adj_pos[2];
  }

  /* Compute direction vector: fwd_vec * 2 */
  {
    float *fwd = *(float **)0x31fc50;
    fwd_doubled[0] = fwd[0] + fwd[0];
    fwd_doubled[1] = fwd[1] + fwd[1];
    fwd_doubled[2] = fwd[2] + fwd[2];
  }

  /* Cast ray. result_buf[0] = t, result_buf[2] = hit object (returned as EAX).
   */
  if (!((char (*)(int, void *, int16_t, int, float *, float *, float,
                  float *))0x149480)(1, bsp, 0, 0, adj_pos, fwd_doubled,
                                     3.4028235e+38f, result_buf)) {
    return default_ret;
  }

  /* Estimated position: adj_pos + fwd_doubled * t */
  out_position->x = fwd_doubled[0] * result_buf[0] + adj_pos[0];
  out_position->y = fwd_doubled[1] * result_buf[0] + adj_pos[1];
  out_position->z = fwd_doubled[2] * result_buf[0] + adj_pos[2];

  /* Return result_buf[2] as EAX (success indicator; caller checks != -1). */
  return *(int *)&result_buf[2];
}

/*
 * compute_acceleration (0x1b5f20)
 *
 * acceleration = desired_velocity - velocity, plus global_gravity on k, then
 * clamped by limit3d to a length that blends from minimum (moving away from
 * the desired velocity) to maximum (moving along it) by the squared cosine
 * between desired_velocity and the acceleration. Returns acceleration.
 *
 * Confirmed from disassembly at 0x1b5f20: register arguments
 * desired_velocity@<ecx>, velocity@<eax>, acceleration@<esi> (all read or
 * written before any stack access), cdecl stack floats maximum [ebp+8] and
 * minimum [ebp+0xc]; FADD [0x32512c] global_gravity onto acceleration.k;
 * dot > 1e-4 (0x253f44, FCOMP + TEST AH,0x41); the blended length is
 * (maximum - minimum) * ((dot * dot / |acceleration|^2) /
 * |desired_velocity|^2) + minimum, otherwise minimum; CALL limit3d; MOV EAX,
 * ESI.
 */
real_vector3d *compute_acceleration(const real_vector3d *desired_velocity,
                                    const real_vector3d *velocity,
                                    real_vector3d *acceleration, real maximum,
                                    real minimum)
{
  real dot;

  acceleration->i = desired_velocity->i - velocity->i;
  acceleration->j = desired_velocity->j - velocity->j;
  acceleration->k = desired_velocity->k - velocity->k;
  acceleration->k += global_gravity;

  dot = vehicle_dot_product3d(desired_velocity, acceleration);
  if (dot > 0.0001f) {
    maximum = (maximum - minimum) *
                ((dot * dot / vehicle_magnitude_squared3d(acceleration)) /
                 vehicle_magnitude_squared3d(desired_velocity)) +
              minimum;
  } else {
    maximum = minimum;
  }

  limit3d(acceleration, maximum);

  return acceleration;
}

/*
 * update_human_tank_physics (0x1b5ff0)
 *
 * Per-tick tank physics: advances the left/right tread accumulators by
 * speed -/+ turn, wraps each into [0, wheel_circumference) via fmod, then —
 * only when the 'phys' tag has exactly 2 powered mass points — gives each
 * tread its ground friction velocity with an identity rotation and hands off
 * to physics_update. Otherwise powered_mass_points is passed as NULL.
 *
 * Confirmed from disassembly at 0x1b5ff0:
 *   object_get_and_verify_type(vehicle_index, 2) -> ESI;
 *   tag_get('vehi', vehicle->tag_index) -> EBX;
 *   tag_get('phys', *(int*)(vehicle_tag+0x8c)) -> [EBP-0x10].
 *   FLD [ESI+0x42c]; FSUB [ESI+0x434] -> left = speed - turn.
 *   FLD [ESI+0x434]; FADD [ESI+0x42c] -> right = turn + speed.
 *   FLD left; FADD [ESI+0x43c]; FST tmp; FSTP [ESI+0x43c]; FLD tmp;
 *   FLD [EBX+0x310]; CALL _CIfmod; FCOM 0.0f; FST [ESI+0x43c]; wrap arm
 *   FADD [EBX+0x310]; FSTP [ESI+0x43c]. Same for right at +0x440.
 *   CMP [phys+0x68],2; PUSH 0; PUSH 0 (shared magic_force/torque NULL).
 *   count == 2: [EDI]=left, [EDI+0x1c..0x24]=0, [EDI+0x28]=1.0f;
 *   [EDI+0x60]=right, [EDI+0x7c..0x84]=0, [EDI+0x88]=1.0f;
 *   physics_update(vehicle_index, EDI, mass_points, NULL, NULL).
 *   count != 2: physics_update(vehicle_index, NULL, mass_points, NULL, NULL).
 *
 * Register args: powered_mass_points arrives in EDI and is never assigned,
 * matching vehicle_update's tank/jeep powered_mass_points@<edi> call.
 *
 * Inferred (names, offsets confirmed): vehicle +0x43c left tread, +0x440
 * right tread; definition +0x310 wheel circumference.
 */
void update_human_tank_physics(int vehicle_index, void *mass_points,
                               void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  struct powered_mass_point_datum *state;
  real left;
  real right;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = (struct physics_definition *)tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  left = vehicle->speed - vehicle->turn;
  right = vehicle->turn + vehicle->speed;

  vehicle->left_tread = left + vehicle->left_tread;
  vehicle->left_tread =
    VEHICLE_FMOD(vehicle->left_tread, definition->wheel_circumference);
  if (vehicle->left_tread < 0.0f) {
    vehicle->left_tread += definition->wheel_circumference;
  }

  vehicle->right_tread = right + vehicle->right_tread;
  vehicle->right_tread =
    VEHICLE_FMOD(vehicle->right_tread, definition->wheel_circumference);
  if (vehicle->right_tread < 0.0f) {
    vehicle->right_tread += definition->wheel_circumference;
  }

  if (physics->powered_mass_points.count == 2) {
    state = (struct powered_mass_point_datum *)powered_mass_points;

    state[0].ground_friction_velocity = left;
    state[0].rotation[0] = 0.0f;
    state[0].rotation[1] = 0.0f;
    state[0].rotation[2] = 0.0f;
    state[0].rotation[3] = 1.0f;

    state[1].ground_friction_velocity = right;
    state[1].rotation[0] = 0.0f;
    state[1].rotation[1] = 0.0f;
    state[1].rotation[2] = 0.0f;
    state[1].rotation[3] = 1.0f;

    physics_update(vehicle_index, state, mass_points, NULL, NULL);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * update_human_jeep_physics (0x1b6140)
 *
 * Per-tick jeep (human ground vehicle) physics: advances the wheel-spin
 * accumulator at vehicle+0x438 by the vehicle's speed (+0x42c), wraps it into
 * [0, divisor) via fmod, then — only when the 'phys' tag has exactly 2
 * powered mass points — fills both powered_mass_point_datum records with the
 * wheel spin as a rotation and hands off to the shared physics_update.
 * Otherwise powered_mass_points is passed as NULL, matching the
 * generic/turret path physics_update already handles.
 *
 * Confirmed from disassembly at 0x1b6140:
 *   MOV EAX,[EBP+8]; PUSH 2; PUSH EAX; CALL 0x13d680 ->
 *     vehicle = object_get_and_verify_type(vehicle_index, 2) (result in ESI).
 *   MOV ECX,[ESI]; PUSH ECX; PUSH 'vehi'; CALL 0x1ba140 ->
 *     vehicle_tag = tag_get('vehi', vehicle->tag_index) (result in EBX).
 *   MOV EDX,[EBX+0x8c]; PUSH EDX; PUSH 'phys'; CALL 0x1ba140 ->
 *     physics_tag = tag_get('phys', *(int*)(vehicle_tag+0x8c)); result (EAX)
 *     saved to [EBP-8] after the following FLD/FADD, which do not touch EAX.
 *   FLD [ESI+0x42c]; FADD [ESI+0x438] -> new_spin = speed + old_spin.
 *   FST [EBP-4]; FSTP [ESI+0x438] -> speculative store of the pre-fmod sum;
 *     unconditionally overwritten below, not reproduced literally.
 *   FLD [EBP-4]; FLD [EBX+0x310]; CALL 0x1daf7e (_CIfmod) ->
 *     new_spin = fmod(new_spin, *(float*)(vehicle_tag+0x310)).
 *   FCOM [0x2533c0(=0.0f)]; FST [ESI+0x438]; FNSTSW AX; TEST AH,5; JP ->
 *     if (new_spin < 0.0f) new_spin += divisor; final value lands in
 *     vehicle+0x438 either way (the pre-branch FST is another speculative
 *     store, overwritten by the wrap arm's FSTP).
 *   MOV ECX,[physics_tag+0x68]; XOR EAX,EAX; CMP ECX,2; PUSH EAX; PUSH EAX;
 *   JNZ 0x1b6233 -> branch on physics_tag->powered_mass_points.count != 2.
 *     The two PUSH EAX(0) execute unconditionally before the branch and are
 *     the shared trailing magic_force/magic_torque=NULL args of
 *     physics_update on both paths (confirmed via ADD ESP,0x14 = 5 args on
 *     both exits).
 *   count != 2 (0x1b6233): PUSH mass_points; PUSH 0(EAX); PUSH vehicle_index;
 *     CALL 0x154270 ->
 *     physics_update(vehicle_index, NULL, mass_points, NULL, NULL).
 *   count == 2 (0x1b61d0): FLD [ESI+0x434]; FMUL [0x253398(=0.5f)] ->
 *     half_turn = turn * 0.5f (vehicle+0x434); FCOS computed first, FSIN
 *     from a reloaded copy of half_turn (order preserved below).
 *     [EDI]=speed(ECX); [EDI+0x1c]=0; [EDI+0x20]=0; [EDI+0x24]=sin;
 *     [EDI+0x28]=cos -> powered_mass_points[0]: ground_friction_velocity =
 *     speed, rotation = {0, 0, sin_half_turn, cos_half_turn}.
 *     [EDI+0x60]=speed(EDX); [EDI+0x7c]=0; [EDI+0x80]=0; FCHS;
 *     [EDI+0x84]=-sin; [EDI+0x88]=cos -> powered_mass_points[1] (offset 0x60
 *     = sizeof(powered_mass_point_datum)): ground_friction_velocity = speed,
 *     rotation = {0, 0, -sin_half_turn, cos_half_turn}.
 *     PUSH EDI; PUSH ECX(vehicle_index reloaded from [EBP+8]); CALL 0x154270
 *     -> physics_update(vehicle_index, powered_mass_points, mass_points,
 *     NULL, NULL).
 *
 * Register args: powered_mass_points arrives in EDI and is never assigned by
 * a MOV inside this function — used raw from entry, matching
 * vehicle_update's documented "tank/jeep powered_mass_points@<edi>" comment
 * at 0x1b8f80.
 *
 * vehicle+0x42c (speed) and vehicle+0x434 (turn) are the PAL-sourced names
 * already Inferred at vehicle_update (0x1b8f80); no struct exists yet for
 * the vehicle/vehicle_tag blobs so raw offsets are used here as elsewhere in
 * this file (see vehicle_get_estimated_position above). vehicle+0x438
 * (wheel-spin accumulator) and vehicle_tag+0x310 (its wrap divisor,
 * presumably part of the documented +0x308 turn-parameter block) are
 * Uncertain: accessed here but not named by any existing PAL reference.
 */
void update_human_jeep_physics(int vehicle_index, void *mass_points,
                                void *powered_mass_points)
{
  char *vehicle;
  char *vehicle_tag;
  struct physics_definition *physics_tag;
  float new_spin;
  float half_turn;
  float sin_half_turn;
  float cos_half_turn;
  float speed;
  struct powered_mass_point_datum *pmp0;
  struct powered_mass_point_datum *pmp1;

  vehicle = (char *)object_get_and_verify_type(vehicle_index, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (struct physics_definition *)tag_get(
    0x70687973, *(int32_t *)(vehicle_tag + 0x8c));

  new_spin = *(float *)(vehicle + 0x42c) + *(float *)(vehicle + 0x438);
  new_spin = x87_fmod(new_spin, *(float *)(vehicle_tag + 0x310));
  if (new_spin < 0.0f) {
    new_spin += *(float *)(vehicle_tag + 0x310);
  }
  *(float *)(vehicle + 0x438) = new_spin;

  if (physics_tag->powered_mass_points.count != 2) {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
    return;
  }

  speed = *(float *)(vehicle + 0x42c);
  half_turn = *(float *)(vehicle + 0x434) * 0.5f;
  cos_half_turn = x87_fcos(half_turn);
  sin_half_turn = x87_fsin(half_turn);

  pmp0 = (struct powered_mass_point_datum *)powered_mass_points;
  pmp1 = pmp0 + 1;

  pmp0->ground_friction_velocity = speed;
  pmp0->rotation[0] = 0.0f;
  pmp0->rotation[1] = 0.0f;
  pmp0->rotation[2] = sin_half_turn;
  pmp0->rotation[3] = cos_half_turn;

  pmp1->ground_friction_velocity = speed;
  pmp1->rotation[0] = 0.0f;
  pmp1->rotation[1] = 0.0f;
  pmp1->rotation[2] = -sin_half_turn;
  pmp1->rotation[3] = cos_half_turn;

  physics_update(vehicle_index, powered_mass_points, mass_points, NULL, NULL);
}

/*
 * update_human_boat_physics (0x1b6250)
 *
 * Boat physics. With exactly three powered mass points: point 0 carries the
 * throttle as water friction velocity and a rudder rotation that shrinks with
 * speed; points 1 and 2 get fixed lift ratios and identity rotations. A
 * roll torque about the forward axis pulls the hull toward an up vector that
 * is banked by the sideways velocity. Otherwise physics_update runs with no
 * powered state and no magic terms.
 *
 * Confirmed from disassembly at 0x1b6250 (frame 0x3c, saves EBX/EDI only):
 *   ESI is read but never saved or loaded -> powered_mass_points@<esi>;
 *   [EBP+8] vehicle index, [EBP+0xc] mass_points.
 *   physics->powered_mass_points.count == 3 (CMP ECX,3 at 0x1b6289).
 *   speed = |sqrt(velocity.velocity) * 2.5|; angle = (1 - MIN(speed, 1)) *
 *   turn (+0x434) * 0.5; state[0]: water_friction_velocity = vehicle.speed,
 *   water_lift_ratio = 0.003, rotation = (0, 0, sin angle, cos angle).
 *   state[1]: lift 0.003, rotation (0,0,0,1); state[2]: lift 0.005, same.
 *   up_relative = forward * -forward.k + global_up; normalize3d != 0 ->
 *   cross = up x forward; spin = (forward x velocity) . global_up * 2pi;
 *   rotate_vector3d_by_sincos(&up_relative, &forward, sin, cos); angle =
 *   0x10c510(&up_relative, &object.up), negated when cross.up_relative > 0.
 *   torque = forward * PIN(sqrt(|angle| * 2 * 0.0139626) * sign(angle) -
 *   angular_velocity.forward, -0.0139626, 0.0139626) * xx_moment; else 0.
 *   physics_update(index, state, mass_points, &zero, &torque).
 */
void update_human_boat_physics(int vehicle_index, void *mass_points,
                               void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = (struct physics_definition *)tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->powered_mass_points.count == 3) {
    struct powered_mass_point_datum *state =
      (struct powered_mass_point_datum *)powered_mass_points;
    object_data_t *object = &vehicle->unit.object;
    real_vector3d zero;
    real_vector3d cross;
    real_vector3d thrust;
    real_vector3d up_relative;
    real dot;
    real angle;
    int sign;
    real speed;
    real negative_k;
    real spin;
    real maximum_angle;
    real *rotation;
    const real *forward;

    speed = x87_fabs(x87_sqrt(vehicle_magnitude_squared3d(
                       (const real_vector3d *)&object->translational_velocity)) *
                     2.5f);
    maximum_angle = vehicle->turn * 0.5f;
    angle = 1.0f - (speed > 1.0f ? 1.0f : speed);

    state[0].water_friction_velocity = vehicle->speed;
    state[0].water_lift_ratio = 0.003f;
    angle *= maximum_angle;
    rotation = state[0].rotation;
    rotation[0] = 0.0f;
    rotation[2] = x87_fsin(angle);
    rotation[1] = 0.0f;
    rotation[3] = x87_fcos(angle);
    zero.i = 0.0f;
    zero.j = 0.0f;
    zero.k = 0.0f;

    forward = &object->forward.x;

    state[1].water_lift_ratio = 0.003f;
    state[1].rotation[0] = 0.0f;
    state[1].rotation[1] = 0.0f;
    state[1].rotation[2] = 0.0f;
    state[1].rotation[3] = 1.0f;

    state[2].water_lift_ratio = 0.005f;
    state[2].rotation[0] = 0.0f;
    state[2].rotation[1] = 0.0f;
    state[2].rotation[2] = 0.0f;
    state[2].rotation[3] = 1.0f;

    negative_k = -object->forward.z;

    up_relative.i = forward[0] * negative_k + GLOBAL_UP3D->i;
    up_relative.j = forward[1] * negative_k + GLOBAL_UP3D->j;
    up_relative.k = forward[2] * negative_k + GLOBAL_UP3D->k;

    if (normalize3d(&up_relative.i) != 0.0f) {
      const real_vector3d *up = (const real_vector3d *)&object->up;
      const real_vector3d *velocity =
        (const real_vector3d *)&object->translational_velocity;
      const real_vector3d *forward3d = (const real_vector3d *)forward;

      vehicle_cross_product3d_in_place(up, forward3d, &cross);

      spin = vehicle_triple_product3d(velocity, forward3d, GLOBAL_UP3D) *
             (2.0f * 3.14159265f);

      rotate_vector3d_by_sincos(&up_relative.i, (float *)forward,
                                x87_fsin(spin), x87_fcos(spin));

      angle = FUN_0010c510(&up_relative.i, &object->up.x);

      if (vehicle_dot_product3d(&cross, &up_relative) > 0.0f) {
        angle = -angle;
      }

      dot = vehicle_dot_product3d(
        (const real_vector3d *)forward,
        (const real_vector3d *)&object->angular_velocity);

      sign = angle != 0.0f ? (angle < 0.0f ? -1 : 1) : 0;

      {
        real correction =
          (real)(x87_sqrtd(x87_fabs(angle) * 2.0 * VEHICLE_ANGULAR_ACCELERATION) *
                   sign -
                 dot);

        if (correction < -VEHICLE_ANGULAR_ACCELERATION) {
          correction = -VEHICLE_ANGULAR_ACCELERATION;
        } else if (correction > VEHICLE_ANGULAR_ACCELERATION) {
          correction = VEHICLE_ANGULAR_ACCELERATION;
        }
        scale_vector3d((const real_vector3d *)forward,
                       correction * physics->xx_moment, &thrust);
      }
    } else {
      thrust.i = 0.0f;
      thrust.j = 0.0f;
      thrust.k = 0.0f;
    }

    physics_update(vehicle_index, state, mass_points, &zero.i, &thrust.i);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * update_alien_fighter_physics_new (0x1b6560)
 *
 * Banshee physics (the variant used when the 'phys' tag's first float is not
 * positive). With exactly two powered mass points: a magic force that seeks
 * forward * speed through compute_acceleration, a magic torque that rotates
 * the vehicle toward its desired facing (pitched by definition +0x364 unless
 * an AI drives it, then yawed by sideways velocity), a thrust fraction that
 * follows the spin rate, and identity rotations for both powered points.
 * Otherwise physics_update runs with no powered state and no magic terms.
 *
 * Confirmed from disassembly at 0x1b6560 (cdecl, 3 stack args, frame 0xa4):
 *   physics->powered_mass_points.count == 2 (CMP ECX,2 at 0x1b659f).
 *   desired_velocity = object.forward * vehicle.speed (+0x42c); throttle =
 *   speed / def+0x2f8 when speed > 0, else -(speed / def+0x2fc).
 *   compute_acceleration(ECX=&desired_velocity, EAX=&translational_velocity,
 *   ESI=&acceleration, throttle * def+0x300, throttle * def+0x304).
 *   magic_force = acceleration * physics->mass (+0x08) * seat_power[0].
 *   0x10a2c0(&current_rotation, &object.forward, &object.up); desired
 *   forward = unit +0x1d4; desired up = global_up + forward * -forward.k,
 *   replaced by *global_forward when normalize3d returns 0 (TEST AH,0x44).
 *   unit_driven_by_ai false -> 0x10c700(&forward, &up, FSIN(def+0x364),
 *   FCOS(def+0x364)).
 *   yaw = (forward.i * velocity.j - forward.j * velocity.i) / def+0x2f8 *
 *   def+0x308; yaw_vectors(&up, &forward, FSIN(yaw), FCOS(yaw)); left =
 *   up x forward expanded inline.
 *   0x1099f0 transposes current_rotation in place; 0x109c70(&desired,
 *   &current, &rotation); 0x10a330(&rotation, &quaternion); 0x10caf0(
 *   &quaternion, &angle, &axis).
 *   desired_angular_velocity = axis * (-angle * def+0x314 * 1/pi
 *   (0x267328)); magic_torque = (desired - object.angular_velocity) *
 *   ((zz + yy + xx) * 1/3) * seat_power[0].
 *   spin = |object.angular_velocity| (inline FSQRT) / def+0x314.
 *   spin > thrust: delta = MIN(spin - thrust, PIN((1 - thrust)^2 * 0.2,
 *   0.01, 0.05)); else delta = MAX(spin - thrust, -MAX(thrust^2 * 0.05,
 *   0.005)); thrust += delta.
 *   state[0]/state[1] (stride 0x60): antigrav_fraction = seat_power[0],
 *   rotation = *global_identity_quaternion ([0x31fc5c]).
 */
void update_alien_fighter_physics_new(int vehicle_index,
                                      void *powered_mass_points,
                                      void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = (struct physics_definition *)tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->powered_mass_points.count == 2) {
    struct powered_mass_point_datum *state =
      (struct powered_mass_point_datum *)powered_mass_points;
    const real_vector3d *object_forward =
      (const real_vector3d *)&vehicle->unit.object.forward;
    const real_vector3d *object_angular_velocity =
      (const real_vector3d *)&vehicle->unit.object.angular_velocity;
    real_vector3d magic_force;
    real_vector3d magic_torque;
    real_vector3d axis;
    real_vector3d desired_angular_velocity;
    real_vector3d angular_acceleration;
    real spin;
    real thrust_delta;

    {
      real_vector3d desired_velocity;
      real_vector3d acceleration;
      real throttle;

      desired_velocity.i = object_forward->i * vehicle->speed;
      desired_velocity.j = object_forward->j * vehicle->speed;
      desired_velocity.k = object_forward->k * vehicle->speed;

      if (vehicle->speed > 0.0f) {
        throttle = vehicle->speed / definition->field_2f8[0];
      } else {
        throttle = -(vehicle->speed / definition->field_2f8[1]);
      }

      compute_acceleration(
        &desired_velocity,
        (const real_vector3d *)&vehicle->unit.object.translational_velocity,
        &acceleration, throttle * definition->field_2f8[2],
        throttle * definition->field_2f8[3]);

      magic_force.i = acceleration.i * physics->mass;
      magic_force.j = acceleration.j * physics->mass;
      magic_force.k = acceleration.k * physics->mass;
      magic_force.i = magic_force.i * vehicle->unit.seat_power[0];
      magic_force.j = magic_force.j * vehicle->unit.seat_power[0];
      magic_force.k = magic_force.k * vehicle->unit.seat_power[0];
    }

    {
      real_matrix3x3 current_rotation;
      real_matrix3x3 desired_rotation;
      real_matrix3x3 rotation;
      real_quaternion rotation_quaternion;
      real yaw;
      real angle;
      real t;
      real scale;

      FUN_0010a2c0((float *)&current_rotation,
                   (float *)&vehicle->unit.object.forward,
                   (float *)&vehicle->unit.object.up);

      desired_rotation.forward =
        *(real_vector3d *)&vehicle->unit.desired_facing_vector;

      /* point on the line through global_up along forward, at -forward.k */
      t = -desired_rotation.forward.k;
      desired_rotation.up.i =
        desired_rotation.forward.i * t + GLOBAL_UP3D->i;
      desired_rotation.up.j =
        desired_rotation.forward.j * t + GLOBAL_UP3D->j;
      desired_rotation.up.k =
        desired_rotation.forward.k * t + GLOBAL_UP3D->k;

      if (normalize3d(&desired_rotation.up.i) == 0.0f) {
        desired_rotation.up = *(real_vector3d *)global_forward_vector_ptr;
      }

      if (!unit_driven_by_ai(vehicle_index)) {
        FUN_0010c700(&desired_rotation.forward.i, &desired_rotation.up.i,
                     x87_fsin(definition->field_364),
                     x87_fcos(definition->field_364));
      }

      yaw = (desired_rotation.forward.i *
               vehicle->unit.object.translational_velocity.y -
             desired_rotation.forward.j *
               vehicle->unit.object.translational_velocity.x) /
            definition->field_2f8[0] * definition->field_308;

      yaw_vectors(&desired_rotation.up.i, &desired_rotation.forward.i,
                  x87_fsin(yaw), x87_fcos(yaw));

      desired_rotation.left.i =
        desired_rotation.up.j * desired_rotation.forward.k -
        desired_rotation.up.k * desired_rotation.forward.j;
      desired_rotation.left.j =
        desired_rotation.up.k * desired_rotation.forward.i -
        desired_rotation.up.i * desired_rotation.forward.k;
      desired_rotation.left.k =
        desired_rotation.forward.j * desired_rotation.up.i -
        desired_rotation.up.j * desired_rotation.forward.i;

      FUN_001099f0((float *)&current_rotation, (float *)&current_rotation);
      FUN_00109c70((float *)&desired_rotation, (float *)&current_rotation,
                   (float *)&rotation);
      FUN_0010a330((float *)&rotation, (float *)&rotation_quaternion);
      FUN_0010caf0((float *)&rotation_quaternion, &angle, &axis.i);

      scale = -angle * definition->field_314 * (1.0f / 3.14159265f);
      desired_angular_velocity.i = axis.i * scale;
      desired_angular_velocity.j = axis.j * scale;
      desired_angular_velocity.k = axis.k * scale;
    }

    angular_acceleration.i =
      desired_angular_velocity.i - object_angular_velocity->i;
    angular_acceleration.j =
      desired_angular_velocity.j - object_angular_velocity->j;
    angular_acceleration.k =
      desired_angular_velocity.k - object_angular_velocity->k;

    {
      real moment =
        (physics->zz_moment + physics->yy_moment + physics->xx_moment) *
        (1.0f / 3);

      magic_torque.i = angular_acceleration.i * moment;
      magic_torque.j = angular_acceleration.j * moment;
      magic_torque.k = angular_acceleration.k * moment;
    }
    magic_torque.i = magic_torque.i * vehicle->unit.seat_power[0];
    magic_torque.j = magic_torque.j * vehicle->unit.seat_power[0];
    magic_torque.k = magic_torque.k * vehicle->unit.seat_power[0];

    spin = (real)(x87_sqrtd(vehicle_magnitude_squared3d(
                    object_angular_velocity)) /
                  definition->field_314);

    if (spin > vehicle->thrust) {
      thrust_delta = (1.0f - vehicle->thrust) * (1.0f - vehicle->thrust) * 0.2f;
      if (thrust_delta < 0.01f) {
        thrust_delta = 0.01f;
      } else if (thrust_delta > 0.05f) {
        thrust_delta = 0.05f;
      }
      if (!(spin - vehicle->thrust > thrust_delta)) {
        thrust_delta = spin - vehicle->thrust;
      }
    } else {
      thrust_delta = vehicle->thrust * vehicle->thrust * 0.05f;
      if (!(thrust_delta > 0.005f)) {
        thrust_delta = 0.005f;
      }
      thrust_delta = -thrust_delta;
      if (spin - vehicle->thrust > thrust_delta) {
        thrust_delta = spin - vehicle->thrust;
      }
    }

    vehicle->thrust += thrust_delta;

    state[0].antigrav_fraction = vehicle->unit.seat_power[0];
    *(real_quaternion *)state[0].rotation =
      *(real_quaternion *)global_identity_quaternion_ptr;
    state[1].antigrav_fraction = vehicle->unit.seat_power[0];
    *(real_quaternion *)state[1].rotation =
      *(real_quaternion *)global_identity_quaternion_ptr;

    physics_update(vehicle_index, state, mass_points, &magic_force.i,
                   &magic_torque.i);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * update_alien_fighter_physics_old (0x1b69a0)
 *
 * Banshee physics (the variant used when the 'phys' tag's radius is positive).
 * With exactly two powered mass points: a magic force that lifts against
 * gravity in proportion to forward speed and thrusts toward vehicle.speed, a
 * magic torque that turns the vehicle toward its desired facing (yawed by
 * sideways velocity), and identity rotations for both powered points.
 * Otherwise physics_update runs with no powered state and no magic terms.
 *
 * Confirmed from disassembly at 0x1b69a0 (frame 0xf0, saves EBX/ESI only):
 *   EDI is read but never saved or loaded -> powered_mass_points arrives in
 *   EDI (the wrapper 0x1b8f10 moves its incoming EAX there); [EBP+8] vehicle
 *   index, [EBP+0xc] mass_points.
 *   physics->powered_mass_points.count == 2 (CMP EAX,2 at 0x1b69e1).
 *   facing = unit +0x1d4; perpendicular is first copied from *global_up, then
 *   overwritten with (-facing.i*facing.k, -facing.j*facing.k,
 *   1 - facing.k^2); normalize3d == 0 -> (1,0,0).
 *   speed = velocity . forward (k, j, i order); thrust = (vehicle.speed -
 *   speed) * mass * 0.05; lift = |speed / def+0x2f8| * mass * global_gravity
 *   * 1.05 (0x2b7cf4); force = up * lift + forward * thrust.
 *   yaw = (velocity.j * facing.i - velocity.i * facing.j) * pi/2 (0x2568bc)
 *   / |def+0x2f8|; yaw_vectors(&perpendicular, &facing, FSIN, FCOS).
 *   matrix_from_forward_and_up(&actual, &object.forward, &object.up) and
 *   (&desired, &facing, &perpendicular); matrix_inverse(&desired, &desired);
 *   matrix4x3_multiply(&actual, &desired, &difference); 0x109fc0(
 *   &difference, &quaternion); 0x10caf0(&quaternion, &angle, &axis).
 *   torque = (axis * angle * 0.13333334 (0x2b7cf0) - angular_velocity) *
 *   radius^2 * mass * 0.05.
 *   state[0]/state[1] (stride 0x60): antigrav_fraction = seat_power[0],
 *   rotation = (0,0,0,1) as immediates; force and torque *= seat_power[0].
 */
void update_alien_fighter_physics_old(int vehicle_index,
                                      void *mass_points,
                                      void *powered_mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = (struct physics_definition *)tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (physics->powered_mass_points.count == 2) {
    struct powered_mass_point_datum *state =
      (struct powered_mass_point_datum *)powered_mass_points;
    object_data_t *object = &vehicle->unit.object;
    real_vector3d facing = *(real_vector3d *)&vehicle->unit.desired_facing_vector;
    real_vector3d perpendicular = *GLOBAL_UP3D;
    real_matrix4x3 actual;
    real_matrix4x3 desired;
    real_matrix4x3 difference;
    real_vector3d axis;
    real_vector3d force;
    real_vector3d torque;
    real_vector3d scaled;
    real_vector2d velocity;
    real yaw;
    real speed;
    real thrust;
    real lift;
    real scale;

    perpendicular.i = -facing.k * facing.i;
    perpendicular.j = -(facing.k * facing.j);
    perpendicular.k = 1.0f - facing.k * facing.k;

    if (normalize3d(&perpendicular.i) == 0.0f) {
      perpendicular.i = 1.0f;
      perpendicular.j = 0.0f;
      perpendicular.k = 0.0f;
    }

    speed = vehicle_dot_product3d(
      (const real_vector3d *)&object->translational_velocity,
      (const real_vector3d *)&object->forward);
    thrust = (vehicle->speed - speed) * physics->mass * 0.05f;
    lift = (x87_fabs(speed / definition->field_2f8[0]) * physics->mass) *
           global_gravity * 1.05f;

    force.i = lift * object->up.x + thrust * object->forward.x;
    force.j = lift * object->up.y + thrust * object->forward.y;
    force.k = lift * object->up.z + thrust * object->forward.z;

    {
      real *destination = &velocity.i;
      const real *source = &object->translational_velocity.x;
      short component_index;

      for (component_index = 0; component_index < 2; component_index++) {
        destination[component_index] = source[component_index];
      }
    }

    yaw = (facing.i * velocity.j - facing.j * velocity.i) *
          (3.14159265f * 0.5f) / x87_fabs(definition->field_2f8[0]);

    yaw_vectors(&perpendicular.i, &facing.i, x87_fsin(yaw), x87_fcos(yaw));

    {
      real_quaternion rotation;
      real angle;

      matrix_from_forward_and_up((float *)&actual, &object->forward.x,
                                 &object->up.x);
      matrix_from_forward_and_up((float *)&desired, &facing.i,
                                 &perpendicular.i);
      matrix_inverse((float *)&desired, (float *)&desired);
      matrix4x3_multiply((float *)&actual, (float *)&desired,
                         (float *)&difference);
      FUN_00109fc0((float *)&difference, (float *)&rotation);
      FUN_0010caf0((float *)&rotation, &angle, &axis.i);
      scale_vector3d(&axis, angle * (4.0f / 30.0f), &scaled);
    }

    scale = physics->radius * physics->radius * physics->mass * 0.05f;

    torque.i = (scaled.i - object->angular_velocity.x) * scale;
    torque.j = (scaled.j - object->angular_velocity.y) * scale;
    torque.k = (scaled.k - object->angular_velocity.z) * scale;

    state[0].antigrav_fraction = vehicle->unit.seat_power[0];
    state[0].rotation[3] = 1.0f;
    state[0].rotation[0] = 0.0f;
    state[0].rotation[1] = 0.0f;
    state[0].rotation[2] = 0.0f;

    state[1].antigrav_fraction = vehicle->unit.seat_power[0];
    state[1].rotation[3] = 1.0f;
    state[1].rotation[0] = 0.0f;
    state[1].rotation[1] = 0.0f;
    state[1].rotation[2] = 0.0f;

    scale_vector3d(&force, vehicle->unit.seat_power[0], &force);
    scale_vector3d(&torque, vehicle->unit.seat_power[0], &torque);

    physics_update(vehicle_index, state, mass_points, &force.i, &torque.i);
  } else {
    physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
  }
}

/*
 * slowly_stop_vehicle (0x1b6ca0)
 *
 * Coasts a vehicle whose stop timer (+0x426) is running: counts the timer
 * down, damps linear and angular velocity by 0.835, advances the position by
 * one tick of velocity and the basis by one tick of angular velocity, zeroes
 * both velocities when the timer reaches zero, then object_set_position.
 *
 * Confirmed from disassembly at 0x1b6ca0 (cdecl, 1 stack arg, frame 0x64):
 *   DEC WORD [EDI+0x426]; six FMUL 0.835 (0x2b7cf8) stores through ESI
 *   (+0x18 velocity) and EBX (+0x3c angular velocity).
 *   position = (velocity.i + position.x, position.y + velocity.j,
 *   position.z + velocity.k).
 *   axis = angular_velocity; normalize3d != 0 -> 0x1092d0(&rotation, &axis,
 *   FSIN, FCOS) and matrix_scale_transform_vector(&rotation, &forward/&up,
 *   &out); else copies of forward/up.
 *   stop_time == 0 -> both velocities = *global_zero_vector (re-read).
 */
void slowly_stop_vehicle(int vehicle_index)
{
  vehicle_data_t *vehicle = (vehicle_data_t *)object_get_and_verify_type(
    vehicle_index, OBJECT_MASK_VEHICLE);
  real_point3d *object_position =
    (real_point3d *)&vehicle->unit.object.position;
  real_vector3d *velocity =
    (real_vector3d *)&vehicle->unit.object.translational_velocity;
  real_vector3d *angular_velocity =
    (real_vector3d *)&vehicle->unit.object.angular_velocity;
  real_matrix4x3 rotation;
  real_point3d position;
  real_vector3d forward;
  real_vector3d up;
  real magnitude;

  vehicle->stop_time--;

  velocity->i *= 0.835f;
  velocity->j *= 0.835f;
  velocity->k *= 0.835f;
  angular_velocity->i *= 0.835f;
  angular_velocity->j *= 0.835f;
  angular_velocity->k *= 0.835f;

  position.x = velocity->i + object_position->x;
  position.y = object_position->y + velocity->j;
  position.z = object_position->z + velocity->k;

  {
    real_vector3d axis;

    axis = *angular_velocity;

    magnitude = normalize3d(&axis.i);
    if (magnitude != 0.0f) {
      FUN_001092d0((float *)&rotation, &axis.i, x87_fsin(magnitude),
                   x87_fcos(magnitude));
      matrix_scale_transform_vector((float *)&rotation,
                                    &vehicle->unit.object.forward.x,
                                    &forward.i);
      matrix_scale_transform_vector((float *)&rotation,
                                    &vehicle->unit.object.up.x, &up.i);
    } else {
      forward = *(real_vector3d *)&vehicle->unit.object.forward;
      up = *(real_vector3d *)&vehicle->unit.object.up;
    }
  }

  if (!vehicle->stop_time) {
    *velocity = *(real_vector3d *)global_zero_vector_ptr;
    *angular_velocity = *(real_vector3d *)global_zero_vector_ptr;
  }

  object_set_position(vehicle_index, &position.x, &forward.i, &up.i);
}

/*
 * create_pelican_effect (0x1b6e20)
 *
 * Spawns the vehicle tag's thruster-wash effect ('vehi' tag +0x3ec) under
 * every "hover thrusters" and "jet thrusters" marker of the vehicle.
 *
 * Confirmed from disassembly at 0x1b6e20:
 *   MOV EBX,[EBP+0x8] -> one cdecl stack argument (the vehicle datum handle);
 *   kb.json declared it (void), corrected here. Both xrefs (0x1b8239 and
 *   0x1b855d, inside FUN_001b81d0) are unconditional calls.
 *   PUSH 0x2; PUSH EBX; CALL 0x13d680 -> object_get_and_verify_type(h, 2).
 *   MOV EAX,[EAX]; PUSH 0x76656869 -> tag_get('vehi', obj->tag_index).
 *   MOV EAX,[EDI+0x3ec]; CMP EAX,-0x1; JZ exit -> nothing to do when the
 *   effect tag reference is NONE.
 *   Marker buffer is at EBP-0x78c and is exactly 16 * 0x6c = 0x6c0 bytes; the
 *   first query is capped at 0xf and the second at 0x10 minus the first
 *   result, appended at (first_count * 0x6c).
 *   Per marker, ESI = &markers[i]; ESI+0x3c is the world-transform forward
 *   vector and ESI+0x60 the world-transform position (marker record =
 *   {int16 node; local matrix4x3 @0x04; world matrix4x3 @0x38}, and a
 *   matrix4x3 is {scale, forward[3], left[3], up[3], position[3]}).
 *   PUSH &dir; PUSH 0x3e860a92; PUSH 0x0; PUSH ESI+0x3c; CALL 0x10b120;
 *   PUSH EAX; CALL 0x10b4c0; ADD ESP,0x14 -> random_direction3d(seed,
 *   marker_forward, 0.0f, 0.2617994f, dir) with the seed fetched last
 *   (cdecl right-to-left), matching the call in C source order.
 *   CMP BX,word [EBP-0x1c] selects vehi+0x444 for hover markers and vehi+0x448
 *   for jet markers; FMUL [0x254640] (6.0f); FADD [0x253f40] (2.0f).
 *   PUSH ECX(&collision); PUSH EDX(handle); PUSH EAX(&velocity);
 *   PUSH ESI+0x60; PUSH 0x61; CALL 0x14df70; ADD ESP,0x14.
 *   On a hit, three effect markers are built: "incident" forward = -dir,
 *   "normal" forward = collision+0x24, "reflected" forward = 0x10c8e0(dir,
 *   collision+0x24); all three points are the hit position collision+0x18.
 *   FLD [0x2533c8] (1.0f); FSUB [collision+0x14] -> fade = 1 - hit fraction,
 *   passed as both scale arguments.
 *   The final call pushes 15 dwords (ADD ESP,0x3c) for the 12 declared
 *   parameters of effect_new_unattached_from_markers (3 of them are floats).
 * Inferred: name from the 2276 symbol dump; the marker-name strings are the
 *   literals at 0x2b7d18/0x2b7d08 and 0x28ab18/0x26b188/0x2b7cfc.
 * Unknown: the meaning of vehi+0x444 / vehi+0x448 (per-thruster-class speed
 *   scalars), collision flag set 0x61, and the trailing 0.0f/0.0f/1 effect
 *   arguments.
 */
void create_pelican_effect(int vehicle_handle)
{
  char markers[16 * 0x6c]; /* EBP-0x78c */
  int16_t collision_result[40]; /* EBP-0xcc, 80-byte raycast result */
  float marker_forwards[9]; /* EBP-0x7c: 3 marker forward vectors */
  float marker_points[9]; /* EBP-0x58: 3 marker positions */
  const char *marker_names[3]; /* EBP-0x34 */
  float velocity[3]; /* EBP-0x28 */
  float fade; /* EBP-0x14 */
  float direction[3]; /* EBP-0x10 */
  char *vehicle;
  char *vehicle_tag;
  char *marker;
  int16_t hover_count;
  int16_t jet_count;
  int marker_count;
  int16_t i;
  float speed;

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(int *)vehicle);
  if (*(int *)(vehicle_tag + 0x3ec) == -1) {
    return;
  }

  hover_count = object_get_marker_by_name(
    vehicle_handle, (void *)"hover thrusters", markers, 0xf);
  jet_count = object_get_marker_by_name(
    vehicle_handle, (void *)"jet thrusters", markers + (int)hover_count * 0x6c,
    0x10 - (int)hover_count);
  marker_count = (int)hover_count + (int)jet_count;

  i = 0;
  if (marker_count <= 0) {
    return;
  }

  do {
    marker = markers + (int)i * 0x6c;
    seed_random_vector_in_cone3d((int *)random_math_get_local_seed_address(),
                                 (float *)(marker + 0x3c), 0.0f, 0.2617994f,
                                 direction);

    if (i < hover_count) {
      speed = *(float *)(vehicle + 0x444);
    } else {
      speed = *(float *)(vehicle + 0x448);
    }
    speed = speed * *(float *)0x254640 + *(float *)0x253f40;
    velocity[0] = direction[0] * speed;
    velocity[1] = direction[1] * speed;
    velocity[2] = direction[2] * speed;

    if (FUN_0014df70(0x61, (float *)(marker + 0x60), velocity, vehicle_handle,
                     collision_result)) {
      marker_forwards[0] = -direction[0];
      marker_forwards[1] = -direction[1];
      marker_forwards[2] = -direction[2];
      marker_forwards[3] = *(float *)((char *)collision_result + 0x24);
      marker_forwards[4] = *(float *)((char *)collision_result + 0x28);
      marker_forwards[5] = *(float *)((char *)collision_result + 0x2c);

      marker_points[0] = *(float *)((char *)collision_result + 0x18);
      marker_points[1] = *(float *)((char *)collision_result + 0x1c);
      marker_points[2] = *(float *)((char *)collision_result + 0x20);
      marker_points[3] = marker_points[0];
      marker_points[4] = marker_points[1];
      marker_points[5] = marker_points[2];
      marker_points[6] = marker_points[0];
      marker_points[7] = marker_points[1];
      marker_points[8] = marker_points[2];

      marker_names[0] = "incident";
      marker_names[1] = "normal";
      marker_names[2] = "reflected";

      FUN_0010c8e0(direction, (float *)((char *)collision_result + 0x24),
                   &marker_forwards[6]);

      fade = *(float *)0x2533c8 - *(float *)((char *)collision_result + 0x14);
      effect_new_unattached_from_markers(
        *(int *)(vehicle_tag + 0x3ec), -1, (float *)0, 3, marker_names,
        marker_points, marker_forwards, fade, fade, 0.0f, 0.0f, 1);
    }
    i = i + 1;
  } while ((int)i < marker_count);
}

/*
 * create_ghost_effect (0x1b7020)
 *
 * Spawns the vehicle's thruster-wash effect (definition->effect) where each
 * "hover thrusters" marker's jittered forward ray hits the world, while seat
 * power 0 is positive.
 *
 * Confirmed from disassembly at 0x1b7020:
 *   CMP [EBX+0x3ec],-1; JZ exit; FLD [ESI+0x2e8]; FCOMP 0.0f; JNZ exit ->
 *   effect index != NONE && seat_power[0] > 0.
 *   object_get_marker_by_name(vehicle_index, "hover thrusters" (0x2b7d18),
 *   markers (EBP-0x7b4, 16 x 0x6c), 15); MOVSX -> int count at EBP-0x24.
 *   Only hover markers are queried: the jet-marker count is the constant 0.
 *   seed_random_vector_in_cone3d(seed, &marker->matrix.forward, 0.0f,
 *   15.0f (0x41700000), &direction); vector = direction.
 *   FUN_0014df70(0x61, &marker->matrix.position, &vector, vehicle_index,
 *   &collision) -> TEST AL,AL.
 *   scale = -forward.k * (1.0f - collision.t) * seat_power[0], clamped to
 *   [0, 1]; a negative scale skips straight to the next marker.
 *   midpoint = (collision.point + marker position) * 0.5f.
 *   Four effect markers: "incident" (0x28ab18) -direction, "normal"
 *   (0x26b188) collision plane normal, "reflected" (0x2b7cfc) and "midpoint"
 *   (0x2b7d28) both FUN_0010c8e0(direction, normal) (reflect); points are the
 *   hit point three times, then the midpoint.
 *   effect_new_unattached_from_markers(effect, NONE, NULL, 4, names, points,
 *   forwards, scale, scale, 0.0f, 0.0f, 1); ADD ESP,0x48 includes the two
 *   earlier call frames.
 */
void create_ghost_effect(int vehicle_index)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  object_marker markers[16];
  struct collision_result collision;
  real_vector3d marker_forwards[4];
  real_point3d marker_points[4];
  const char *marker_names[4];
  real_vector3d vector;
  real_point3d midpoint;
  real_vector3d direction;
  real scale;
  object_marker *marker;
  int16_t hover_marker_count;
  int16_t jet_marker_count;
  int16_t marker_index;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);

  if (definition->effect.tag_index != NONE &&
      vehicle->unit.seat_power[0] > 0.0f) {
    hover_marker_count = object_get_marker_by_name(
      vehicle_index, (void *)"hover thrusters", markers, 15);
    jet_marker_count = 0;

    for (marker_index = 0;
         marker_index < hover_marker_count + jet_marker_count;
         marker_index++) {
      marker = &markers[marker_index];

      seed_random_vector_in_cone3d(
        (int *)random_math_get_local_seed_address(),
        (float *)&marker->matrix.forward, 0.0f, 15.0f, &direction.i);

      vector = direction;

      if (FUN_0014df70(0x61, (float *)&marker->matrix.position, &vector.i,
                       vehicle_index, (int16_t *)&collision)) {
#define GHOST_WASH_SCALE \
  (-marker->matrix.forward.z * (1.0f - collision.t) * vehicle->unit.seat_power[0])
        scale = GHOST_WASH_SCALE < 0.0f   ? 0.0f
                : GHOST_WASH_SCALE > 1.0f ? 1.0f
                                          : GHOST_WASH_SCALE;
#undef GHOST_WASH_SCALE

        if (scale > 0.0f) {
          midpoint.x = (collision.point.x + marker->matrix.position.x) * 0.5f;
          midpoint.y = (collision.point.y + marker->matrix.position.y) * 0.5f;
          midpoint.z = (collision.point.z + marker->matrix.position.z) * 0.5f;
          marker_points[0] = collision.point;
          marker_points[1] = collision.point;
          marker_forwards[0].i = -direction.i;
          marker_names[0] = "incident";
          marker_forwards[0].j = -direction.j;
          marker_names[1] = "normal";
          marker_forwards[1] = *(real_vector3d *)collision.plane.normal;
          marker_points[2] = collision.point;
          marker_forwards[0].k = -direction.k;
          marker_names[2] = "reflected";

          FUN_0010c8e0(&direction.i, collision.plane.normal,
                       &marker_forwards[2].i);

          marker_points[3] = midpoint;
          marker_names[3] = "midpoint";

          FUN_0010c8e0(&direction.i, collision.plane.normal,
                       &marker_forwards[3].i);

          effect_new_unattached_from_markers(
            definition->effect.tag_index, NONE, NULL, 4,
            (void *)marker_names, &marker_points[0].x, &marker_forwards[0].i,
            scale, scale, 0.0f, 0.0f, 1);
        }
      }
    }
  }
}

/* create_crashing_effects (0x1b72b0)
 *
 * cdecl, three stack args, no return.  dv = vehicle velocity (+0x18) minus
 * previous_velocity; when |dv| > 0.02 and some mass point (0x130-byte
 * records, flag byte bit 0x2 = in contact) touches, scale =
 * (|dv| - 0.02) * 45.454544.  The game-globals block (+0x188, element 0,
 * 0x98 bytes) damage effect at +0x48 is applied to the vehicle itself with
 * the scale clamped to [0,1] at damage_params+0x40, the vehicle center
 * (+0x50) at +0x1c and dv at +0x34; the vehicle definition's crash sound
 * (+0x3cc) is played with the same clamped scale.  Returns early when both
 * tags are NONE. */
void create_crashing_effects(int vehicle_index, float *previous_velocity,
                             void *mass_points)
{
  char *vehicle;
  char *definition;
  char *physics;
  char *globals_block;
  char damage_params[0x54];
  float delta[3];
  float magnitude;
  float scale;
  int16_t i;

  vehicle = (char *)object_get_and_verify_type(vehicle_index, 2);
  definition = (char *)tag_get(0x76656869, *(int *)vehicle);
  physics = (char *)tag_get(0x70687973, *(int *)(definition + 0x8c));
  globals_block = (char *)tag_block_get_element(
    (char *)game_globals_get() + 0x188, 0, 0x98);
  if (*(int *)(globals_block + 0x48) == -1 &&
      *(int *)(definition + 0x3cc) == -1)
    return;
  delta[0] = *(float *)(vehicle + 0x18) - previous_velocity[0];
  delta[1] = *(float *)(vehicle + 0x1c) - previous_velocity[1];
  delta[2] = *(float *)(vehicle + 0x20) - previous_velocity[2];
  magnitude =
    x87_sqrt(delta[2] * delta[2] + delta[1] * delta[1] + delta[0] * delta[0]);
  if (!(magnitude > 0.02f))
    return;
  for (i = 0; i < *(int *)(physics + 0x74); i++) {
    tag_block_get_element(physics + 0x74, i, 0x80);
    if (*((char *)mass_points + i * 0x130) & 0x2)
      break;
  }
  if (!(i < *(int *)(physics + 0x74)))
    return;

  scale = (magnitude - 0.02f) * 45.454544f;
  if (*(int *)(globals_block + 0x48) != -1) {
    damage_data_new(damage_params, *(int *)(globals_block + 0x48));
    if (scale < 0.0f)
      *(float *)(damage_params + 0x40) = 0.0f;
    else if (scale > 1.0f)
      *(float *)(damage_params + 0x40) = 1.0f;
    else
      *(float *)(damage_params + 0x40) = scale;
    ((int *)(damage_params + 0x1c))[0] = ((int *)(vehicle + 0x50))[0];
    ((int *)(damage_params + 0x1c))[1] = ((int *)(vehicle + 0x50))[1];
    ((int *)(damage_params + 0x1c))[2] = ((int *)(vehicle + 0x50))[2];
    ((float *)(damage_params + 0x34))[0] = delta[0];
    ((float *)(damage_params + 0x34))[1] = delta[1];
    ((float *)(damage_params + 0x34))[2] = delta[2];
    object_cause_damage(damage_params, vehicle_index, -1, -1, -1, NULL);
  }
  if (*(int *)(definition + 0x3cc) != -1) {
    if (scale < 0.0f)
      scale = 0.0f;
    else if (scale > 1.0f)
      scale = 1.0f;
    object_impulse_sound_new(vehicle_index, *(int *)(definition + 0x3cc), -1,
                             *(float **)0x31fc1c, *(float **)0x31fc3c, scale);
  }
}

/* update_suspension (0x1b74d0)
 *
 * cdecl, one stack arg; returns bool in AL.  Needs an animation graph
 * (vehicle definition +0x44) whose first "object" (antr +0x24, 0x74-byte
 * elements) exists; otherwise returns 0 with nothing written.
 * For each suspension entry (animation +0x68, 0x14 bytes: int16 mass point,
 * int16 animation, float +0x4 extension, float +0x8 compression) with a valid
 * mass point (< phys +0x74 count) and animation: cast a 0xc0a0 ray along the
 * mass point's down vector (+0x50) from its position (+0x38), starting
 * (+0x8 - phys+0x14 - (+0x4 - +0x8)) along it and 2 * (+0x4 - +0x8) long;
 * state = clamp((1 - t) * 2, 0, 1) where t is the collision fraction at
 * result+0x14; the previous per-mass-point byte at vehicle+0x44c+i
 * (0xff = 1.0, else byte / 255) is replaced by
 * quantize_real_to_byte_lower_bound(0, 1, (state + previous) * 0.5).
 * Tracks the largest (state - previous).  Returns 1 only after playing the
 * suspension sound (definition +0x3bc) for a largest jump > 0.3, with scale
 * clamp((jump - 0.3) * 1.6666667, 0, 1); every other path returns 0. */
bool update_suspension(int vehicle_index)
{
  char *vehicle;
  char *definition;
  char *animation_graph;
  char *animation;
  char *physics;
  int *suspensions;
  char *suspension;
  char *mass_point;
  int16_t collision_result[40];
  float matrix[13];
  float point[3];
  float start[3];
  float vector[3];
  float down[3];
  float previous;
  float offset;
  float length;
  float state;
  float change;
  float largest;
  float scale;
  int16_t i;
  int sound;

  vehicle = (char *)object_get_and_verify_type(vehicle_index, 2);
  definition = (char *)tag_get(0x76656869, *(int *)vehicle);
  if (*(int *)(definition + 0x44) == -1)
    return 0;
  animation_graph =
    (char *)tag_get(0x616e7472, *(int *)(definition + 0x44));
  if (*(int *)(animation_graph + 0x24) == 0)
    return 0;
  animation = (char *)tag_block_get_element(animation_graph + 0x24, 0, 0x74);
  if (animation == NULL)
    return 0;
  physics = (char *)tag_get(0x70687973, *(int *)(definition + 0x8c));
  largest = 0.0f;
  matrix4x3_from_forward_up_position(matrix, (float *)(vehicle + 0xc),
                                     (float *)(vehicle + 0x24),
                                     (float *)(vehicle + 0x30));
  suspensions = (int *)(animation + 0x68);
  for (i = 0; i < *suspensions; i++) {
    suspension = (char *)tag_block_get_element(suspensions, i, 0x14);
    if (*(int16_t *)suspension < 0 ||
        *(int16_t *)suspension >= *(int *)(physics + 0x74) ||
        *(int16_t *)(suspension + 0x2) == -1)
      continue;
    tag_block_get_element(animation_graph + 0x74,
                          *(int16_t *)(suspension + 0x2), 0xb4);
    mass_point = (char *)tag_block_get_element(
      physics + 0x74, *(int16_t *)suspension, 0x80);
    if (*(uint8_t *)(vehicle + 0x44c + i) == 0xff)
      previous = 1.0f;
    else
      previous = (float)*(uint8_t *)(vehicle + 0x44c + i) * 0.003921569f;
    matrix_transform_point(matrix, (float *)(mass_point + 0x38), point);
    matrix_transform_vector(matrix, (float *)(mass_point + 0x50), down);
    length = *(float *)(suspension + 0x4) - *(float *)(suspension + 0x8);
    offset = *(float *)(suspension + 0x8) - *(float *)(physics + 0x14) - length;
    start[0] = down[0] * offset + point[0];
    start[1] = down[1] * offset + point[1];
    start[2] = down[2] * offset + point[2];
    length = length + length;
    vector[0] = down[0] * length;
    vector[1] = down[1] * length;
    vector[2] = down[2] * length;
    FUN_0014df70(0xc0a0, start, vector, vehicle_index, collision_result);
    state = (1.0f - *(float *)((char *)collision_result + 0x14)) * 2.0f;
    if (state < 0.0f)
      state = 0.0f;
    else if (state > 1.0f)
      state = 1.0f;
    change = state - previous;
    if (change > largest)
      largest = change;
    *(uint8_t *)(vehicle + 0x44c + i) =
      quantize_real_to_byte_lower_bound(0.0f, 1.0f, (state + previous) * 0.5f);
  }
  sound = *(int *)(definition + 0x3bc);
  if (sound == -1)
    return 0;
  if (!(largest > 0.3f))
    return 0;
  scale = (largest - 0.3f) * 1.6666667f;
  if (scale < 0.0f)
    scale = 0.0f;
  else if (scale > 1.0f)
    scale = 1.0f;
  object_impulse_sound_new(vehicle_index, sound, -1, *(float **)0x31fc1c,
                           *(float **)0x31fc3c, scale);
  return 1;
}

/* create_slipping_effects (0x1b77f0)
 *
 * vehicle_index in EAX (PUSH EAX to object_get_and_verify_type before any
 * EAX write); two stack args, only the second (per-mass-point 0x130-byte
 * state records) is read.  For each touching mass point (flag byte bit
 * 0x2) sliding faster than 0.03 (|+0x54|), spawns the definition's
 * slipping material effect (+0x3dc) of type 10 when the phys mass point
 * flag +0x24 bit 0 is set else 9, material +0x70, at
 * position(+0x4) + normal(+0x60) * (+0x74 - phys+0x68 + 0.003), velocity
 * normal * 0.5 + slip * (0.8660254 / |slip|), with scale
 * clamp((|slip| - 0.03) * 4.5454545, 0, 1). */
void create_slipping_effects(int vehicle_index, void *powered_mass_points,
                             void *mass_points)
{
  char *vehicle;
  char *definition;
  char *physics;
  char *mass_point;
  char *state;
  float position[3];
  float velocity[3];
  float speed;
  float scale;
  float lift;
  float inverse;
  int16_t i;

  (void)powered_mass_points;
  vehicle = (char *)object_get_and_verify_type(vehicle_index, 2);
  definition = (char *)tag_get(0x76656869, *(int *)vehicle);
  physics = (char *)tag_get(0x70687973, *(int *)(definition + 0x8c));
  if (*(int *)(definition + 0x3dc) == -1)
    return;
  for (i = 0; i < *(int *)(physics + 0x74); i++) {
    state = (char *)mass_points + i * 0x130;
    mass_point = (char *)tag_block_get_element(physics + 0x74, i, 0x80);
    if (!(*(uint8_t *)state & 0x2))
      continue;
    speed = x87_sqrt(((float *)(state + 0x54))[0] * ((float *)(state + 0x54))[0] +
                     ((float *)(state + 0x54))[1] * ((float *)(state + 0x54))[1] +
                     ((float *)(state + 0x54))[2] * ((float *)(state + 0x54))[2]);
    if (!(speed > 0.03f))
      continue;
    scale = (speed - 0.03f) * 4.5454545f;
    lift = *(float *)(state + 0x74) - *(float *)(mass_point + 0x68) + 0.003f;
    position[0] = lift * *(float *)(state + 0x60) + *(float *)(state + 0x4);
    position[1] = lift * *(float *)(state + 0x64) + *(float *)(state + 0x8);
    position[2] = lift * *(float *)(state + 0x68) + *(float *)(state + 0xc);
    inverse = 0.8660254f / speed;
    velocity[0] = *(float *)(state + 0x60) * 0.5f +
                  inverse * ((float *)(state + 0x54))[0];
    velocity[1] = *(float *)(state + 0x64) * 0.5f +
                  inverse * ((float *)(state + 0x54))[1];
    velocity[2] = *(float *)(state + 0x68) * 0.5f +
                  inverse * ((float *)(state + 0x54))[2];
    if (scale < 0.0f)
      scale = 0.0f;
    else if (scale > 1.0f)
      scale = 1.0f;
    material_effect_new(*(int *)(definition + 0x3dc),
                        (*(uint32_t *)(mass_point + 0x24) & 0x1) ? 10 : 9,
                        (short)*(uint16_t *)(state + 0x70), position, velocity,
                        vehicle + 0x48, scale);
  }
}

/* 0x1b79c0 — vehicle_export_function_values
 *
 * Fills the vehicle's four exported function values (object+0xd4, stride 4)
 * from the vehicle tag's four int16 export sources at tag+0x31c.  A source of
 * 0 leaves the slot untouched; every other source starts at 0.0f, the
 * switch (jump table at 0x1b7e48, sources 1..36) computes the value, and
 * the result is clamped to [0,1] before the store.
 *
 * Confirmed: cdecl, 1 stack arg (vehicle_handle at [EBP+8]);
 *   object_get_and_verify_type(handle, 2) / tag_get('vehi').
 * Confirmed: the three normalizers are max(|tag+0x2f8|, |tag+0x2fc|),
 *   max(|tag+0x330|, |tag+0x334|) and max(|tag+0x308|, |tag+0x30c|), each
 *   picked by FCOMP/TEST AH,0x41/JNZ (first operand when strictly greater).
 * Confirmed: jump table targets (source -> vehicle offset used):
 *   1,28-31: |+0x42c| / speed max.  2: max(+0x42c, 0) / |tag+0x2f8|.
 *   3: |min(+0x42c, 0)| / |tag+0x2fc|.  4/5/6: |+0x430| over the slide
 *   max / |tag+0x330| / |tag+0x334|.  7: max of sources 1 and 4 (the second
 *   operand is spilled as a double, FSTP qword [EBP-0x34]).  8/9/10:
 *   |+0x434| over the turn max / |tag+0x308| / |tag+0x30c|.  11/12: 1.0f
 *   when byte +0x424 has bit 0x4 / 0x8, else 0.0f.  13 and out-of-range:
 *   0.0f.  14/15/16: magnitude of +0x18 over speed max; 15 needs byte +0x4
 *   & 0x1c, 16 needs & 0x2.  17: |dot(+0x18, +0x24)|, 18/19: |dot(+0x18,
 *   +0x30)|, both over speed max.  20/21/24-27: +0x43c / +0x440 / +0x438
 *   over tag+0x310.  22: |+0x42c - +0x434|, 23: |+0x434 + +0x42c|, over
 *   speed max.  32: square of 0x254e6c * |perpendicular part of +0x18
 *   about +0x24| (FUN_0010b8a0 then FUN_00012fe0).  33/34: +0x444 / +0x448.
 *   35: lerp between source 17 and source 2 (unsigned byte +0x428 scaled by
 *   *0x2549d4, +1, *0.5, clamped).  36: (|+0x18| / tag+0x2f8 * +0x448 -
 *   *0x2533e8) * *0x2b7d40.
 * Inferred (Halo CE vehicle tag layout, not proven here): tag+0x2f8/0x2fc
 *   are max forward/reverse speed, +0x308/0x30c max left/right turn,
 *   +0x310 wheel circumference, +0x31c the A..D "in" sources, +0x330/0x334
 *   max left/right slide.
 * Unknown: the values of *0x2549d4, *0x254e6c, *0x2533e8 and *0x2b7d40 are
 *   read from memory rather than restated as literals.
 */
void vehicle_export_function_values(int vehicle_handle)
{
  char *vehicle;
  char *vehicle_tag;
  float speed_max;
  float speed_forward;
  float speed_reverse;
  float slide_left;
  float slide_right;
  float slide_max;
  float turn_left;
  float turn_right;
  float turn_max;
  float *value_out;
  int16_t *source;
  int count;
  float value;
  float interpolation;
  float velocity_value;
  float speed_value;
  float parallel[3];
  float perpendicular[3];

  vehicle = (char *)object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(int *)vehicle);

  speed_forward = (float)fabs(*(float *)(vehicle_tag + 0x2f8));
  speed_reverse = (float)fabs(*(float *)(vehicle_tag + 0x2fc));
  speed_max = speed_forward > speed_reverse ? speed_forward : speed_reverse;
  slide_left = (float)fabs(*(float *)(vehicle_tag + 0x330));
  slide_right = (float)fabs(*(float *)(vehicle_tag + 0x334));
  slide_max = slide_left > slide_right ? slide_left : slide_right;
  turn_left = (float)fabs(*(float *)(vehicle_tag + 0x308));
  turn_right = (float)fabs(*(float *)(vehicle_tag + 0x30c));
  turn_max = turn_left > turn_right ? turn_left : turn_right;

  value_out = (float *)(vehicle + 0xd4);
  source = (int16_t *)(vehicle_tag + 0x31c);
  count = 4;
  do {
    if (*source != 0) {
      value = 0.0f;
      switch (*source) {
      case 1:
      case 28:
      case 29:
      case 30:
      case 31:
        value = (float)(fabs(*(float *)(vehicle + 0x42c)) / speed_max);
        break;
      case 2:
        value = (*(float *)(vehicle + 0x42c) < 0.0f ? 0.0f
                                                   : *(float *)(vehicle + 0x42c)) /
                speed_forward;
        break;
      case 3:
        value = (float)(fabs(*(float *)(vehicle + 0x42c) > 0.0f
                               ? 0.0f
                               : *(float *)(vehicle + 0x42c)) /
                        speed_reverse);
        break;
      case 4:
        value = (float)(fabs(*(float *)(vehicle + 0x430)) / slide_max);
        break;
      case 5:
        value = (float)(fabs(*(float *)(vehicle + 0x430)) / slide_left);
        break;
      case 6:
        value = (float)(fabs(*(float *)(vehicle + 0x430)) / slide_right);
        break;
      case 7:
        value = (float)(fabs(*(float *)(vehicle + 0x42c)) / speed_max >
                            fabs(*(float *)(vehicle + 0x430)) / slide_max
                          ? fabs(*(float *)(vehicle + 0x42c)) / speed_max
                          : fabs(*(float *)(vehicle + 0x430)) / slide_max);
        break;
      case 8:
        value = (float)(fabs(*(float *)(vehicle + 0x434)) / turn_max);
        break;
      case 9:
        value = (float)(fabs(*(float *)(vehicle + 0x434)) / turn_left);
        break;
      case 10:
        value = (float)(fabs(*(float *)(vehicle + 0x434)) / turn_right);
        break;
      case 11:
        value = (*(uint8_t *)(vehicle + 0x424) & 4) ? 1.0f : 0.0f;
        break;
      case 12:
        value = (*(uint8_t *)(vehicle + 0x424) & 8) ? 1.0f : 0.0f;
        break;
      case 14:
        value = FUN_00012fe0((float *)(vehicle + 0x18)) / speed_max;
        break;
      case 15:
        if (*(uint8_t *)(vehicle + 4) & 0x1c) {
          value = FUN_00012fe0((float *)(vehicle + 0x18)) / speed_max;
        }
        break;
      case 16:
        if (*(uint8_t *)(vehicle + 4) & 2) {
          value = FUN_00012fe0((float *)(vehicle + 0x18)) / speed_max;
        }
        break;
      case 17:
        value = (float)(fabs(vehicle_dot_product3d(
                               (const real_vector3d *)(vehicle + 0x18),
                               (const real_vector3d *)(vehicle + 0x24))) /
                        speed_max);
        break;
      case 18:
      case 19:
        value = (float)(fabs(vehicle_dot_product3d(
                               (const real_vector3d *)(vehicle + 0x18),
                               (const real_vector3d *)(vehicle + 0x30))) /
                        speed_max);
        break;
      case 20:
        value = *(float *)(vehicle + 0x43c) / *(float *)(vehicle_tag + 0x310);
        break;
      case 21:
        value = *(float *)(vehicle + 0x440) / *(float *)(vehicle_tag + 0x310);
        break;
      case 22:
        value = (float)(fabs(*(float *)(vehicle + 0x42c) -
                             *(float *)(vehicle + 0x434)) /
                        speed_max);
        break;
      case 23:
        value = (float)(fabs(*(float *)(vehicle + 0x434) +
                             *(float *)(vehicle + 0x42c)) /
                        speed_max);
        break;
      case 24:
      case 25:
      case 26:
      case 27:
        value = *(float *)(vehicle + 0x438) / *(float *)(vehicle_tag + 0x310);
        break;
      case 32:
        FUN_0010b8a0((float *)(vehicle + 0x18), (float *)(vehicle + 0x24),
                     parallel, perpendicular);
        value = FUN_00012fe0(perpendicular) * *(float *)0x254e6c;
        value = value * value;
        break;
      case 34:
        value = *(float *)(vehicle + 0x448);
        break;
      case 33:
        value = *(float *)(vehicle + 0x444);
        break;
      case 35:
        velocity_value = (float)(fabs(vehicle_dot_product3d(
                                        (const real_vector3d *)(vehicle + 0x18),
                                        (const real_vector3d *)(vehicle + 0x24))) /
                                 speed_max);
        speed_value =
          (float)(fabs(*(float *)(vehicle + 0x42c)) / speed_forward);
        interpolation =
          ((float)*(uint8_t *)(vehicle + 0x428) * *(float *)0x2549d4 + 1.0f) *
          0.5f;
        if (interpolation < 0.0f) {
          interpolation = 0.0f;
        } else if (interpolation > 1.0f) {
          interpolation = 1.0f;
        }
        value = interpolation * speed_value +
                (1.0f - interpolation) * velocity_value;
        break;
      case 36:
        value = (FUN_00012fe0((float *)(vehicle + 0x18)) /
                   *(float *)(vehicle_tag + 0x2f8) *
                   *(float *)(vehicle + 0x448) -
                 *(float *)0x2533e8) *
                *(float *)0x2b7d40;
        break;
      }
      if (value < 0.0f) {
        value = 0.0f;
      } else if (value > 1.0f) {
        value = 1.0f;
      }
      *value_out = value;
    }
    value_out = value_out + 1;
    source = source + 1;
    count = count - 1;
  } while (count != 0);
}

/*
 * vehicle_moving_near_any_player (0x1b7ee0)
 *
 * Scans all local players. For each local player whose unit is on foot (not
 * currently inside a vehicle, checked via object_data+0xcc == NONE), collects
 * up to MAXIMUM_NUMBER_OF_LOCAL_PLAYERS unit handles and their bounding-sphere
 * centres. Then iterates every vehicle object in the world. For each vehicle,
 * checks each on-foot player:
 *   1. The player's unit is not already sitting inside this vehicle
 *      (object_data[unit]+0xcc != vehicle_datum_handle).
 *   2. Squared distance between vehicle position (+0x50) and the cached
 *      player bounding-sphere centre is < 100.0 (within 10 world units).
 *   3. Squared velocity magnitude (+0x18) is >= ~1/900 (speed >= ~1/30 u/tick).
 * Returns true if any qualifying vehicle is found.
 *
 * Called from game_safe_to_save (0xa7530) to block saving while a moving
 * vehicle is close to a player on foot.
 */


#include "../../common.h"

/* Squared proximity threshold: 10.0^2 = 100.0 world units. */
#define VEHICLE_NEAR_PLAYER_DIST_SQ 100.0f
/* Squared velocity threshold: (1/30)^2 = 1/900. Vehicle must exceed this to
 * be considered "moving". Value from binary at 0x25620c. */
#define VEHICLE_MIN_SPEED_SQ 0.001111111138f

bool vehicle_moving_near_any_player(void)
{
  /* Object iterator buffer: 0x10-byte struct identical in layout to
   * data_iter_t. object_iterator_next writes the current datum handle at
   * byte offset 0x08 (iter_buf[2] as an int array). */
  int iter_buf[4];
  int unit_handles[4]; /* handles of on-foot player units */
  float player_pos[12]; /* 3-float bounding-sphere centre per player */
  float radius_scratch; /* radius out-param, not used here */
  int16_t lpi; /* current local_player_index */
  int16_t n; /* count of on-foot player units collected */
  int16_t i;
  int player_handle;
  char *player;
  int unit_handle;
  void *unit_obj;
  void *veh_obj;
  int vehicle_datum_handle;
  float dx, dy, dz;
  float vx, vy, vz;
  char found; /* 1 = no vehicle found yet, 0 = found; matches local_5 */

  n = 0;
  found = 1;

  /* Phase 1: collect on-foot local player units and their positions.
   * local_player_get_next(-1) returns the first valid local_player_index. */
  lpi = ((int16_t(*)(int16_t))0xba4c0)((int16_t)-1);
  if (lpi == (int16_t)-1)
    goto done;

  /* Push ESI before inner loop (matches disasm 001b7f07: PUSH ESI). */
  do {
    /* First call: validate player index. */
    player_handle = ((int (*)(int16_t))0xba3c0)(lpi);
    if (player_handle != -1) {
      /* Second call: get handle for datum_get (matches disasm 001b7f16). */
      player_handle = ((int (*)(int16_t))0xba3c0)(lpi);
      player = (char *)((void *(*)(void *, int))0x119320)(*(void **)0x5aa6d4,
                                                          player_handle);
      unit_handle = *(int *)(player + 0x34);
      if (unit_handle != -1) {
        /* object_get_and_verify_type(handle, 3): accepts biped or vehicle. */
        unit_obj = ((void *(*)(int, int))0x13d680)(unit_handle, 3);
        /* +0xcc = parent_object_index; NONE (-1) means unit is on foot. */
        if (*(int *)((char *)unit_obj + 0xcc) == -1) {
          unit_handles[n] = unit_handle;
          /* object_get_bounding_sphere: writes centre to &player_pos[n*3],
           * radius to &radius_scratch. Centre is at object_data+0x50. */
          ((void (*)(int, float *, float *))0x1aae0)(
            unit_handle, &player_pos[n * 3], &radius_scratch);
          n++;
        }
      }
    }
    lpi = ((int16_t(*)(int16_t))0xba4c0)(lpi);
  } while (lpi != (int16_t)-1);

  if (n == 0)
    goto done;

  /* Phase 2: iterate all vehicle objects (type_mask=2 = bit 1 = vehicle). */
  object_iterator_new(iter_buf, 2, 0);

  while ((veh_obj = object_iterator_next(iter_buf)) != NULL) {
    /* iter_buf[2] holds the datum handle of the current vehicle object,
     * written by object_iterator_next at offset 0x08 in the iter buffer. */
    vehicle_datum_handle = iter_buf[2];

    i = 0;
    if (n <= 0)
      continue;

    do {
      unit_obj = ((void *(*)(int, int))0x13d680)(unit_handles[i], 3);
      /* Skip player whose unit is already inside this vehicle. */
      if (*(int *)((char *)unit_obj + 0xcc) == vehicle_datum_handle) {
        i++;
        continue;
      }

      /* Squared distance: vehicle pos (+0x50) vs player bounding centre. */
      dx = *(float *)((char *)veh_obj + 0x50) - player_pos[i * 3];
      dy = *(float *)((char *)veh_obj + 0x54) - player_pos[i * 3 + 1];
      dz = *(float *)((char *)veh_obj + 0x58) - player_pos[i * 3 + 2];
      if (dx * dx + dy * dy + dz * dz >= VEHICLE_NEAR_PLAYER_DIST_SQ) {
        i++;
        continue;
      }

      /* Squared velocity: vehicle velocity (+0x18) must exceed threshold. */
      vx = *(float *)((char *)veh_obj + 0x18);
      vy = *(float *)((char *)veh_obj + 0x1c);
      vz = *(float *)((char *)veh_obj + 0x20);
      if (vx * vx + vy * vy + vz * vz >= VEHICLE_MIN_SPEED_SQ) {
        found = 0;
        goto done;
      }
      i++;
    } while (i < n);
  }

done:
  return found == 0;
}

/*
 * vehicle_stuck (0x1b8060)
 *
 * For a vehicle with stuck mass points (+0x478 bitmask), averages the
 * flagged mass points' positions from the 'phys' tag, transforms the
 * average to world space and writes the normalized direction from the
 * vehicle's origin to it. Returns TRUE only when that direction is nonzero.
 *
 * Confirmed from disassembly at 0x1b8060 (cdecl, 2 stack args, frame 0x64):
 *   stuck_mass_point_flags == 0 -> FALSE; physics_instance_new (0x1509c0)
 *   into a 0x3c-byte local; FALSE when it fails.
 *   center = *global_origin3d; for each mass point (short index, count from
 *   instance.physics +0x74) with (1 << index) set: tag_block_get_element(
 *   block, index, 0x80) +0x38 added to center, count++.
 *   count > 0 -> center *= 1.0 / count (FILD); matrix_transform_point(
 *   &instance.world_matrix, &center, &center_in_world);
 *   object_get_world_position(index, &origin); direction = center_in_world -
 *   origin; normalize3d != 0 -> TRUE.
 */
char vehicle_stuck(int unit_handle, float *vec)
{
  vehicle_data_t *vehicle = (vehicle_data_t *)object_get_and_verify_type(
    unit_handle, OBJECT_MASK_VEHICLE);
  char stuck = 0;

  if (vehicle->stuck_mass_point_flags) {
    struct physics_instance instance;

    if (FUN_001509c0(&instance, unit_handle)) {
      real_point3d center = *global_origin3d_ptr;
      short mass_point_count = 0;
      short mass_point_index;

      for (mass_point_index = 0;
           mass_point_index < instance.physics->mass_points.count;
           mass_point_index++) {
        if (vehicle->stuck_mass_point_flags & (1 << mass_point_index)) {
          struct mass_point_definition *mass_point =
            (struct mass_point_definition *)tag_block_get_element(
              (void *)&instance.physics->mass_points, mass_point_index,
              sizeof(struct mass_point_definition));

          center.x = center.x + mass_point->position.x;
          center.y = center.y + mass_point->position.y;
          center.z = center.z + mass_point->position.z;
          mass_point_count++;
        }
      }

      if (mass_point_count > 0) {
        real scale = 1.0f / mass_point_count;
        real_point3d center_in_world;
        real_point3d origin;

        center.x *= scale;
        center.y *= scale;
        center.z *= scale;

        matrix_transform_point((float *)&instance.world_matrix, &center.x,
                               &center_in_world.x);
        object_get_world_position(unit_handle, (vector3_t *)&origin);
        vec[0] = center_in_world.x - origin.x;
        vec[1] = center_in_world.y - origin.y;
        vec[2] = center_in_world.z - origin.z;

        if (normalize3d(vec) != 0.0f) {
          stuck = 1;
        }
      }
    }
  }

  return stuck;
}

/*
 * update_human_plane_physics (0x1b81d0)
 *
 * Pelican physics. With vehicle flag bit 1 set the mass points are cleared
 * and no force is applied. Otherwise hover eases toward a flag-selected
 * factor (0.25 / 1.0 / 0.75) scaled by the unused throttle, thrust follows
 * the squared throttle, a magic force drives toward vehicle.speed and lifts
 * against gravity, and a magic torque turns the hull toward its desired
 * facing (yawed by sideways velocity). Both paths finish with
 * create_pelican_effect.
 *
 * Confirmed from disassembly at 0x1b81d0 (cdecl, 3 stack args, frame 0xf8):
 *   TEST CL,2 on the word at +0x424 -> csmemset([EBP+0x10], 0,
 *   physics->mass_points.count * 0x130); [EBP+0xc] (powered state) unused.
 *   throttle = PIN(speed, 0, def+0x2f8) / def+0x2f8, squared.
 *   factor = flags&4 ? 0.25 : flags&8 ? 1.0 : 0.75; hover (+0x444) moves
 *   toward factor * (1 - throttle) * seat_power[0] by at most 0.05 (PIN at
 *   -0.05 (0x2b7d44) / 0.05); thrust (+0x448) = throttle * seat_power[0].
 *   desired_up = (-f.k*f.i, -(f.k*f.j), 1 - f.k^2), (1,0,0) if degenerate.
 *   drive = (speed - velocity.forward) * thrust * mass * 0.05; lift =
 *   (|dot / def+0x2f8| * 1.05 + hover * 1.3 (0x255b9c)) * gravity * mass.
 *   yaw = (velocity.j*f.i - velocity.i*f.j) * pi/2 / |def+0x2f8|.
 *   Rotation chain 0x109e10 x2, 0x109150, 0x109850, 0x109fc0, 0x10caf0;
 *   torque = (axis * angle/30 - angular_velocity) * radius^2 * mass * 0.05.
 *   physics_update(index, NULL, mass_points, &force, &torque).
 */
void update_human_plane_physics(int vehicle_index, void *powered_mass_points,
                                void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = (struct physics_definition *)tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);

  if (vehicle->flags & 0x2) {
    csmemset(mass_points, 0,
             physics->mass_points.count * sizeof(struct mass_point_datum));
  } else {
    object_data_t *object = &vehicle->unit.object;
    real_vector3d desired_forward;
    real_vector3d desired_up;
    real_vector3d force;
    real_vector3d torque;
    real throttle;
    real thrust;
    real factor;

    if (vehicle->speed < 0.0f) {
      throttle = 0.0f;
    } else if (vehicle->speed > definition->field_2f8[0]) {
      throttle = definition->field_2f8[0];
    } else {
      throttle = vehicle->speed;
    }
    throttle = throttle / definition->field_2f8[0];
    throttle = throttle * throttle;

    factor = !(vehicle->flags & 0x4) ? ((vehicle->flags & 0x8) ? 1.0f : 0.75f)
                                     : 0.25f;
    factor *= 1.0f - throttle;
    vehicle_interpolate_scalar(&vehicle->hover,
                               factor * vehicle->unit.seat_power[0], 0.05f);

    vehicle->thrust = thrust = throttle * vehicle->unit.seat_power[0];

    desired_forward = *(real_vector3d *)&vehicle->unit.desired_facing_vector;

    desired_up.i = -(desired_forward.k * desired_forward.i);
    desired_up.j = -(desired_forward.k * desired_forward.j);
    desired_up.k = 1.0f - desired_forward.k * desired_forward.k;

    if (normalize3d(&desired_up.i) == 0.0f) {
      desired_up.i = 1.0f;
      desired_up.j = 0.0f;
      desired_up.k = 0.0f;
    }

    {
      real dot;
      real drive;
      real lift;

      dot = vehicle_dot_product3d(
        (const real_vector3d *)&object->translational_velocity,
        (const real_vector3d *)&object->forward);

      drive = (vehicle->speed - dot) * thrust * physics->mass * 0.05f;
      lift = (x87_fabs(dot / definition->field_2f8[0]) * 1.05f +
              vehicle->hover * 1.3f) *
             global_gravity;
      lift *= physics->mass;

      force.i = lift * object->up.x + drive * object->forward.x;
      force.j = lift * object->up.y + drive * object->forward.y;
      force.k = lift * object->up.z + drive * object->forward.z;
    }

    {
      real_vector2d velocity;
      real yaw;

      {
        real *destination = &velocity.i;
        const real *source = &object->translational_velocity.x;
        short component_index;

        for (component_index = 0; component_index < 2; component_index++) {
          destination[component_index] = source[component_index];
        }
      }

      yaw = (velocity.j * desired_forward.i - velocity.i * desired_forward.j) *
            (3.14159265f / 2) / (real)fabs(definition->field_2f8[0]);

      yaw_vectors(&desired_up.i, &desired_forward.i, x87_fsin(yaw),
                  x87_fcos(yaw));
    }

    {
      real_matrix4x3 vehicle_rotation;
      real_matrix4x3 desired_rotation;
      real_matrix4x3 rotation;
      real_quaternion quaternion;
      real_vector3d axis;
      real_vector3d scaled;
      real angle;
      real scale;

      matrix_from_forward_and_up((float *)&vehicle_rotation,
                                 &object->forward.x, &object->up.x);
      matrix_from_forward_and_up((float *)&desired_rotation,
                                 &desired_forward.i, &desired_up.i);
      matrix_inverse((float *)&desired_rotation, (float *)&desired_rotation);
      matrix4x3_multiply((float *)&vehicle_rotation,
                         (float *)&desired_rotation, (float *)&rotation);
      FUN_00109fc0((float *)&rotation, (float *)&quaternion);
      FUN_0010caf0((float *)&quaternion, &angle, &axis.i);

      scale_vector3d(&axis, angle * (1.0f / 30), &scaled);

      scale = physics->radius * physics->radius * physics->mass * 0.05f;

      torque.i = (scaled.i - object->angular_velocity.x) * scale;
      torque.j = (scaled.j - object->angular_velocity.y) * scale;
      torque.k = (scaled.k - object->angular_velocity.z) * scale;
    }

    scale_vector3d(&force, vehicle->unit.seat_power[0], &force);
    scale_vector3d(&torque, vehicle->unit.seat_power[0], &torque);

    physics_update(vehicle_index, NULL, mass_points, &force.i, &torque.i);
  }

  create_pelican_effect(vehicle_index);
}

/*
 * update_alien_scout_physics (0x1b8570)
 *
 * Ghost physics. Every powered mass point gets seat power 0 as its antigrav
 * fraction and an identity rotation. Out of deep water and not upside down,
 * the hover fraction drives three magic-force/torque terms (throttle
 * acceleration, steering yaw torque, self-levelling torque) plus a boost
 * term, all scaled by antigrav; then physics_update runs, the hover fraction
 * seeks the grounded-powered-mass-point ratio by at most 0.1 per tick, and
 * create_ghost_effect spawns the thruster wash.
 *
 * Confirmed from disassembly at 0x1b8570 (cdecl, 4 stack args, frame 0x94):
 *   FUN_0018f510(&object.location (+0x48), &object.position) -> water depth
 *   (float, held in ST0 across the powered-mass-point loop).
 *   magic_force/magic_torque = *global_zero_vector3d ([0x31fc38]).
 *   Loop over physics->powered_mass_points.count: state[i] +0x18 = antigrav,
 *   +0x1c..+0x24 = 0, +0x28 = 1.0f.
 *   water_depth < 0.5f && up.k > -0.2f (0x255ba4).
 *   matrix4x3_from_forward_up_position(&matrix, &position, forward, up);
 *   0x109780 inverse-transforms translational_velocity into local_velocity.
 *   Constants (read from .rdata): 0.8f, 0.05f, 0.98f, 0.785398185 (double),
 *   0.0069813174 (double), 1e-4 (double epsilon), 2.0f, +/-0.0034906587f,
 *   15.0f, 0.3f, 2.5f, 0.0015514038f, 0.0038785094f, -0.005817764f, 0.004f,
 *   1/30, 0.002f, 0.001f, 0.4f, +/-0.1f.
 *   Dot and cross products are expanded inline (no CALLs); the boost
 *   torque/lift products are yy*hover*speed / mass*hover*speed.
 *   The 2d up/angular-velocity copies are not materialized: the dots read
 *   the object fields directly.
 *   Mass-point loop: tag_block_get_element(&physics->mass_points, i, 0x80);
 *   powered when +0x20 != NONE; grounded when mass_points[i].flags & 0x10.
 *   create_ghost_effect(vehicle_index) last.
 */
void update_alien_scout_physics(int vehicle_index, float steering,
                                void *powered_mass_points, void *mass_points)
{
  vehicle_data_t *vehicle;
  vehicle_definition_t *definition;
  struct physics_definition *physics;
  struct powered_mass_point_datum *state;
  real_matrix4x3 matrix;
  vector3_t local_velocity;
  real_vector3d magic_force;
  real_vector3d magic_torque;
  real antigrav;
  real water_depth;
  int16_t mass_point_index;

  vehicle = (vehicle_data_t *)object_get_and_verify_type(vehicle_index,
                                                         OBJECT_MASK_VEHICLE);
  definition = (vehicle_definition_t *)tag_get(
    TAG_GROUP_VEHI, vehicle->unit.object.tag_index);
  physics = (struct physics_definition *)tag_get(
    TAG_GROUP_PHYS, definition->physics.tag_index);
  state = (struct powered_mass_point_datum *)powered_mass_points;

  /* object.location (+0x48, still unk_72/unk_76 in object_data_t) */
  water_depth = FUN_0018f510(&vehicle->unit.object.unk_72,
                             &vehicle->unit.object.position);

  magic_force = *(real_vector3d *)global_zero_vector_ptr;
  magic_torque = *(real_vector3d *)global_zero_vector_ptr;
  antigrav = vehicle->unit.seat_power[0];

  for (mass_point_index = 0;
       mass_point_index < physics->powered_mass_points.count;
       mass_point_index++) {
    state[mass_point_index].antigrav_fraction = antigrav;
    state[mass_point_index].rotation[0] = 0.0f;
    state[mass_point_index].rotation[1] = 0.0f;
    state[mass_point_index].rotation[2] = 0.0f;
    state[mass_point_index].rotation[3] = 1.0f;
  }

  if (water_depth < 0.5f && vehicle->unit.object.up.z > -0.2f) {
    const real_vector3d *object_forward =
      (const real_vector3d *)&vehicle->unit.object.forward;
    const real_vector3d *object_up =
      (const real_vector3d *)&vehicle->unit.object.up;
    const real_vector3d *object_angular_velocity =
      (const real_vector3d *)&vehicle->unit.object.angular_velocity;

    matrix4x3_from_forward_up_position(
      &matrix, &vehicle->unit.object.position.x, (float *)&object_forward->i,
      (float *)&object_up->i);
    real_matrix3x3_transform_vector(
      &matrix, &vehicle->unit.object.translational_velocity, &local_velocity);

    if (vehicle->hover > 0.0f) {
      real maximum_speed = definition->field_2f8[0];
      real maximum_acceleration;
      real_vector2d target_velocity;
      real_vector3d acceleration;
      real scale;

      if (vehicle->flags & FLAG(3)) {
        maximum_speed *= 0.8f;
      }

      maximum_acceleration = definition->field_2f8[2];

      target_velocity.i = maximum_speed * vehicle->unit.throttle.x;
      target_velocity.j = maximum_speed * vehicle->unit.throttle.y;
      acceleration.i = target_velocity.i - local_velocity.x;
      acceleration.j = target_velocity.j - local_velocity.y;
      acceleration.k = 0.0f;

      if (vehicle->on_ground_ticks > 0 && fabs(steering) > 0.785398185f) {
        real reduction = vehicle->on_ground_ticks * 0.05f;

        if (reduction > 0.98f) {
          reduction = 0.98f;
        }
        maximum_acceleration *= 1.0f - reduction;
      }

      limit3d(&acceleration, maximum_acceleration);
      matrix_scale_transform_vector((float *)&matrix, &acceleration.i,
                                    &acceleration.i);

      scale = physics->mass * vehicle->hover;
      magic_force.i += acceleration.i * scale;
      magic_force.j += acceleration.j * scale;
      magic_force.k += acceleration.k * scale;
    }

    if (vehicle->hover > 0.0f) {
      real current = vehicle_dot_product3d(object_up, object_angular_velocity);
      int32_t sign = steering != 0.0f ? (steering < 0.0f ? -1 : 1) : 0;
      real desired = (real)x87_sqrtd(fabs(steering) * 0.0069813174f) * sign;
      real error;
      real torque;

      if (fabs(desired) > 0.0001f && steering / desired < 2.0f) {
        desired = steering * 0.5f;
      }

      error = desired - current;
      if (error < -0.0034906587f) {
        error = -0.0034906587f;
      } else if (error > 0.0034906587f) {
        error = 0.0034906587f;
      }
      torque = error * physics->zz_moment;
      torque *= vehicle->hover;

      magic_torque.i += object_up->i * torque;
      magic_torque.j += object_up->j * torque;
      magic_torque.k += object_up->k * torque;
    }

    if (vehicle->hover < 1.0f) {
      real_vector3d left;
      real_vector2d forward2d;
      real_vector2d left2d;
      real_vector2d control_torque = *(real_vector2d *)global_zero_vector2d_ptr;
      real torque_a;
      real torque_b;

      left.i = object_forward->k * object_up->j -
               object_up->k * object_forward->j;
      left.j = object_up->k * object_forward->i -
               object_up->i * object_forward->k;
      left.k = object_forward->j * object_up->i -
               object_up->j * object_forward->i;
      forward2d.i = object_forward->i;
      forward2d.j = object_forward->j;
      left2d.i = left.i;
      left2d.j = left.j;
      normalize2d(&forward2d.i);
      normalize2d(&left2d.i);

      if (object_up->k > 0.0f) {
        real_vector2d level_torque;
        real_vector2d alignment;
        real_vector2d rate;
        int32_t sign_a;
        int32_t sign_b;
        real weight_a;
        real weight_b;

        /* The 2d up/angular-velocity views read the object fields in place. */
        alignment.i =
          vehicle_dot_product2d((const real_vector2d *)object_up, &forward2d);
        alignment.j =
          vehicle_dot_product2d((const real_vector2d *)object_up, &left2d);
        rate.i = vehicle_dot_product2d(
          (const real_vector2d *)object_angular_velocity, &left2d);
        rate.j = -vehicle_dot_product2d(
          (const real_vector2d *)object_angular_velocity, &forward2d);
        level_torque = *(real_vector2d *)global_zero_vector2d_ptr;
        level_torque.i -= alignment.i;
        level_torque.j -= alignment.j;
        level_torque.i -= 15.0f * rate.i;
        level_torque.j -= 15.0f * rate.j;
        sign_a = vehicle->unit.throttle.x * level_torque.i != 0.0f
                   ? (vehicle->unit.throttle.x * level_torque.i < 0.0f ? -1 : 1)
                   : 0;
        weight_a = (real)fabs(level_torque.i) * sign_a;
        sign_b = vehicle->unit.throttle.y * level_torque.j != 0.0f
                   ? (vehicle->unit.throttle.y * level_torque.j < 0.0f ? -1 : 1)
                   : 0;
        weight_b = (real)fabs(level_torque.j) * sign_b;
        weight_a += 1.0f;
        if (weight_a < 0.3f) {
          weight_a = 0.3f;
        } else if (weight_a > 2.5f) {
          weight_a = 2.5f;
        }
        control_torque.i +=
          vehicle->unit.throttle.x * weight_a * 0.0015514038f;
        weight_b += 1.0f;
        if (weight_b < 0.3f) {
          weight_b = 0.3f;
        } else if (weight_b > 2.5f) {
          weight_b = 2.5f;
        }
        control_torque.j +=
          vehicle->unit.throttle.y * weight_b * 0.0015514038f;
        {
          real level_scale = (1.0f - object_up->k) * 0.0038785094f;

          torque_a = level_scale * level_torque.i;
          torque_b = level_scale * level_torque.j;
          torque_a += control_torque.i;
        }
      } else {
        torque_a = vehicle->unit.throttle.x * 0.0015514038f;
        torque_b = vehicle->unit.throttle.y * 0.0015514038f;
        torque_a += control_torque.i;
      }

      torque_b += control_torque.j;

      {
        real_vector3d torque = *(real_vector3d *)global_zero_vector_ptr;
        real left_scale = physics->yy_moment * torque_a;
        real forward_scale = -(physics->xx_moment * torque_b);
        real scale;

        torque.i += left.i * left_scale;
        torque.j += left.j * left_scale;
        torque.k += left.k * left_scale;
        torque.i += object_forward->i * forward_scale;
        torque.j += object_forward->j * forward_scale;
        torque.k += object_forward->k * forward_scale;

        scale = 1.0f - vehicle->hover;
        magic_torque.i += torque.i * scale;
        magic_torque.j += torque.j * scale;
        magic_torque.k += torque.k * scale;
      }
    }

    if (vehicle->flags & FLAG(3)) {
      real speed =
        vehicle_dot_product3d(object_forward,
                              (const real_vector3d *)&vehicle->unit.object
                                .translational_velocity) /
        definition->field_2f8[0];
      real_vector3d left;

      if (speed < 0.0f) {
        speed = 0.0f;
      } else if (speed > 1.0f) {
        speed = 1.0f;
      }

      vehicle_cross_product3d(object_up, object_forward, &left);

      if (speed > 0.0f) {
        real torque =
          physics->yy_moment * vehicle->hover * speed * -0.005817764f;
        real lift;

        magic_torque.i += left.i * torque;
        magic_torque.j += left.j * torque;
        magic_torque.k += left.k * torque;

        lift = physics->mass * vehicle->hover * speed * 0.004f;
        magic_force.i += GLOBAL_UP3D->i * lift;
        magic_force.j += GLOBAL_UP3D->j * lift;
        magic_force.k += GLOBAL_UP3D->k * lift;
      }

      if (vehicle->airborne_ticks > 0) {
        real_vector3d axis;

        vehicle_cross_product3d(&left, GLOBAL_UP3D, &axis);
        if (normalize3d(&axis.i) > 0.0f) {
          real fade = 1.0f - vehicle->airborne_ticks * (1.0f / 30);
          real axis_scale;
          real up_scale;

          if (fade < 0.0f) {
            fade = 0.0f;
          } else if (fade > 1.0f) {
            fade = 1.0f;
          }
          axis_scale = (1.0f - vehicle->hover) * physics->mass * fade * 0.002f;

          magic_force.i += axis.i * axis_scale;
          magic_force.j += axis.j * axis_scale;
          magic_force.k += axis.k * axis_scale;

          up_scale = (1.0f - vehicle->hover) * physics->mass * fade * 0.001f;
          magic_force.i += GLOBAL_UP3D->i * up_scale;
          magic_force.j += GLOBAL_UP3D->j * up_scale;
          magic_force.k += GLOBAL_UP3D->k * up_scale;
        }
      }
    }

    magic_force.i *= antigrav;
    magic_force.j *= antigrav;
    magic_force.k *= antigrav;
    magic_torque.i *= antigrav;
    magic_torque.j *= antigrav;
    magic_torque.k *= antigrav;
  }

  physics_update(vehicle_index, state, mass_points, &magic_force.i,
                 &magic_torque.i);

  {
    real maximum = 0.4f > vehicle->unit.object.up.z
                     ? 0.4f
                     : vehicle->unit.object.up.z;
    int32_t powered_count = 0;
    int32_t grounded_count = 0;
    real ratio = 0.0f;
    real target;

    for (mass_point_index = 0;
         mass_point_index < physics->mass_points.count;
         mass_point_index++) {
      struct mass_point_definition *mass_point_definition =
        (struct mass_point_definition *)tag_block_get_element(
          &physics->mass_points, mass_point_index,
          sizeof(struct mass_point_definition));

      if (mass_point_definition->powered_mass_point_index != NONE) {
        powered_count++;
        if (((struct mass_point_datum *)mass_points)[mass_point_index].flags &
            FLAG(4)) {
          grounded_count++;
        }
      }
    }

    if ((int16_t)powered_count > 0) {
      ratio = (real)(int16_t)grounded_count / (real)(int16_t)powered_count;
    }

    target = ratio * maximum;
    if (target < 0.0f) {
      target = 0.0f;
    } else if (target > 1.0f) {
      target = 1.0f;
    }
    if (target - vehicle->hover > 0.1f) {
      target = vehicle->hover + 0.1f;
    } else if (target - vehicle->hover < -0.1f) {
      target = vehicle->hover - 0.1f;
    }

    vehicle->hover = target;
  }

  create_ghost_effect(vehicle_index);
}

/*
 * update_alien_fighter_physics (0x1b8f10) — select which alien-fighter
 * (Banshee) physics update to run for this vehicle, then run the ghost
 * effect update.
 *
 * Confirmed from disassembly at 0x1b8f10:
 *   PUSH EDI (callee save, POP EDI at both exits); PUSH 0x2; PUSH ESI;
 *   MOV EDI,EAX -> object_get_and_verify_type(vehicle_handle@<esi>, 2), with
 *   the incoming EAX stashed in EDI. So this function takes three register
 *   arguments: ESI, EAX and EBX (EBX is PUSHed at 0x1b8f44 as a call argument
 *   and only ever reclaimed by ADD ESP — never POPped — so it is an incoming
 *   argument, not a save).
 *   MOV EAX,[EAX]; PUSH EAX; PUSH 0x76656869 -> tag_get('vehi',
 * obj->tag_index). MOV ECX,[EAX+0x8c]; PUSH ECX; PUSH 0x70687973 ->
 *     tag_get('phys', vehi_tag->physics_tag_index at +0x8c).
 *   ADD ESP,0x18 -> all three cdecl cleanups (3 calls x 2 args) coalesced.
 *   FLD [EAX]; FCOMP [0x002533c0]; FNSTSW AX; TEST AH,0x41; JNZ 0x1b8f60.
 *     TEST AH,0x41 masks C0|C3, so the jump is taken when phys[0] <= 0.0f and
 *     the FALL-THROUGH is the phys[0] > 0.0f case. 0x2533c0 is the shared 0.0f
 *     constant. Fall-through runs 0x1b69a0 (the "_old" variant).
 *   Fall-through: PUSH ESI; CALL 0x1b69a0; ADD ESP,0x8 -> two stack args, the
 *     EBX pushed at 0x1b8f44 plus ESI: update_alien_fighter_physics_old(esi,
 * ebx). Taken: PUSH EDI; PUSH ESI; CALL 0x1b6560; ADD ESP,0xc -> three stack
 * args: update_alien_fighter_physics_new(esi, edi(=incoming eax), ebx). Both
 * paths end PUSH ESI; CALL 0x1b7020; ADD ESP,0x4 -> create_ghost_effect(esi).
 * Inferred: names from kb.json symbol dump.
 * Unknown: the meaning of the EAX and EBX arguments (they are only forwarded,
 *   never inspected here), and which 'phys' field lives at offset 0.
 */
void update_alien_fighter_physics(int vehicle_handle, int param_2, int param_3)
{
  void *vehicle;
  char *vehicle_tag;
  float *physics_tag;

  vehicle = object_get_and_verify_type(vehicle_handle, 2);
  vehicle_tag = (char *)tag_get(0x76656869, *(uint32_t *)vehicle);
  physics_tag = (float *)tag_get(0x70687973, *(int32_t *)(vehicle_tag + 0x8c));

  if (*physics_tag > *(float *)0x2533c0) {
    update_alien_fighter_physics_old(vehicle_handle, (void *)param_3,
                                     (void *)param_2);
    create_ghost_effect(vehicle_handle);
    return;
  }

  update_alien_fighter_physics_new(vehicle_handle, (void *)param_2,
                                   (void *)param_3);
  create_ghost_effect(vehicle_handle);
}

/* vehicle_update @ 0x001b8f80
 *
 * Confirmed: sole reference is the vehicle object_type_definition table at
 *   0x323de8, slot +0x30 (datum update; abs ref at 0x323e18). Name and block
 *   structure from PAL 2342 units/vehicles.c vehicle_update (T2).
 * Confirmed: cdecl, one stack arg; returns AL=1 (0x1b9867). Frame 0x327c via
 *   _chkstk: mass_points [ebp-0x327c] (32 x 0x130), powered_mass_points
 *   [ebp-0xc7c] (32 x 0x60), damage data [ebp-0x7c] (0x54).
 * Confirmed: physics dispatch is a 7-entry jump table at 0x1b9870 on the
 *   definition's short at +0x2f4 (0 tank, 1 jeep, 2 boat, 3 plane, 4 alien
 *   scout, 5 alien fighter, 6 turret = inline physics_update); register args:
 *   tank/jeep powered_mass_points@<edi>, boat powered_mass_points@<esi>,
 *   create_slipping_effects index@<eax>, FUN_001b56b0 index@<eax> +
 *   mass_points@<edi> (its extra pushed powered_mass_points is never read by
 *   the callee and is not passed here), update_alien_fighter_physics
 *   index@<esi>/powered@<eax>/mass@<ebx>.
 * Confirmed: collision-user push/pop asserts at vehicles.c lines 308/372,
 *   user id 0x11; profile section at 0x32e4a0 (enable byte 0x32e4a8).
 * Confirmed: both dot products are accumulated (k + j) + i (x87 order at
 *   0x1b90b3-0x1b90e9); written in that association.
 * Inferred (PAL names, offsets confirmed): vehicle +0x424 vehicle flags,
 *   +0x426 stop_time, +0x429 upending type, +0x42a upending ticks, +0x42c
 *   speed, +0x430 slide, +0x434 turn; unit +0x1b8 control flags, +0x1d4
 *   desired facing, +0x228/+0x22c throttle, +0x2e8/+0x2ec seat power;
 *   definition +0x2f0 flags, +0x2f8/+0x330 speed parameters, +0x308 turn
 *   parameters, +0x318 blur speed, +0x340/+0x344 roll clamp.
 */
bool vehicle_update(int vehicle_index)
{
  char *vehicle;
  char *definition;
  char *falling_damage;
  char *child;
  float *forward;
  float *up;
  float *desired_facing;
  float left_i;
  float left_j;
  float left_k;
  float steering_angle;
  float torque;
  float roll;
  float desired_position;
  float target;
  float floor_z;
  float ceiling_z;
  int child_index;
  uint8_t upending_type;
  uint16_t vehicle_flags;
  char blur;
  real_vector3d torque_axis;
  real_vector3d cross;
  real_vector3d previous_velocity;
  char animation_update[2];
  char damage_data[0x54];
  char powered_mass_points[0xc00];
  char mass_points[0x2600];

  vehicle = (char *)object_get_and_verify_type(vehicle_index, 2);
  definition = (char *)tag_get(0x76656869, *(int *)vehicle);

  if (*(char *)0x449ef1 != '\0' && *(char *)0x32e4a8 != '\0') {
    profile_enter_private((void *)0x32e4a0);
  }

  if (*(int *)(vehicle + 0xcc) != -1) {
    *(float *)(vehicle + 0x3c) = 0.0f;
    *(float *)(vehicle + 0x40) = 0.0f;
    *(float *)(vehicle + 0x44) = 0.0f;
    *(float *)(vehicle + 0x18) = 0.0f;
    *(float *)(vehicle + 0x1c) = 0.0f;
    *(float *)(vehicle + 0x20) = 0.0f;
    *(uint32_t *)(vehicle + 4) &= ~0x20u;
    goto animate;
  }

  if ((*(uint32_t *)(vehicle + 0x1b8) & 1) != 0) {
    *(uint8_t *)(vehicle + 0x424) |= 4;
  } else {
    *(uint8_t *)(vehicle + 0x424) &= ~4;
  }
  if ((*(uint32_t *)(vehicle + 0x1b8) & 2) != 0 ||
      ((*(uint8_t *)(definition + 0x2f0) & 0x10) != 0 &&
       ((*(float *)(vehicle + 0x228) > 0.0f &&
         *(float *)(vehicle + 0x42c) < 0.0f) ||
        (*(float *)(vehicle + 0x228) < 0.0f &&
         *(float *)(vehicle + 0x42c) > 0.0f)))) {
    *(uint8_t *)(vehicle + 0x424) |= 8;
  } else {
    *(uint8_t *)(vehicle + 0x424) &= ~8;
  }

  /* left = cross_product3d(up, forward); steering = atan2(left . facing,
   * facing . forward). */
  forward = (float *)(vehicle + 0x24);
  up = (float *)(vehicle + 0x30);
  desired_facing = (float *)(vehicle + 0x1d4);
  left_i = up[1] * forward[2] - forward[1] * up[2];
  left_j = forward[0] * up[2] - up[0] * forward[2];
  left_k = forward[1] * up[0] - up[1] * forward[0];
  vehicle_flags = *(uint16_t *)(vehicle + 0x424);
  steering_angle = (float)atan2(
      left_k * desired_facing[2] + left_j * desired_facing[1] +
          left_i * desired_facing[0],
      desired_facing[2] * forward[2] + desired_facing[1] * forward[1] +
          desired_facing[0] * forward[0]);

  upending_type = *(uint8_t *)(vehicle + 0x429);
  if ((vehicle_flags & 0x10) != 0 && upending_type != 0 &&
      *(uint8_t *)(vehicle + 0x42a) < 30 && *(float *)(vehicle + 0x38) <= 0.9f) {
    torque = (upending_type == 2 || upending_type == 4) ? 0.3 : -0.3;
    if (upending_type == 4 || upending_type == 3) {
      cross_product3d(forward, up, &torque_axis.i);
    } else {
      torque_axis = *(real_vector3d *)forward;
    }
    roll = *(float *)(vehicle + 0x38) * -2.0f;
    if (roll < *(float *)(definition + 0x340)) {
      roll = *(float *)(definition + 0x340);
    } else if (roll > *(float *)(definition + 0x344)) {
      roll = *(float *)(definition + 0x344);
    }
    torque = roll * torque;
    *(uint32_t *)(vehicle + 4) &= ~0x20u;
    if (upending_type == 2 || upending_type == 1) {
      cross_product3d(forward, up, &cross.i);
      /* in place: 0x1b91d7/0x1b91e3 push [ebp-0x1c] as both base and out */
      vector3d_scale_add(&torque_axis.i, &cross.i, -forward[2],
                         &torque_axis.i); /* dup-args-ok */
    }
    FUN_00012fb0(&torque_axis.i, torque, (float *)(vehicle + 0x3c));
    switch (*(int16_t *)(definition + 0x2f4)) {
    case 0:
      FUN_00012fb0(forward,
                   FUN_00013070(forward, (float *)(vehicle + 0x18)),
                   (float *)(vehicle + 0x18));
      break;
    case 5:
      *(float *)(vehicle + 0x20) = (-0.01f > *(float *)(vehicle + 0x20))
                                       ? *(float *)(vehicle + 0x20)
                                       : -0.01f;
      break;
    }
    *(uint8_t *)(vehicle + 0x42a) = *(uint8_t *)(vehicle + 0x42a) + 1;
  } else {
    *(uint8_t *)(vehicle + 0x42a) = 0;
    *(uint8_t *)(vehicle + 0x429) = 0;
    *(uint16_t *)(vehicle + 0x424) = vehicle_flags & 0xffef;
  }

  if ((*(uint8_t *)(vehicle + 0x424) & 8) != 0) {
    physics_variable_speed_update_seek((float *)(vehicle + 0x42c),
                                       definition + 0x2f8, 0.0f, 1.0f);
  } else {
    physics_variable_speed_update_seek((float *)(vehicle + 0x42c),
                                       definition + 0x2f8,
                                       *(float *)(vehicle + 0x228), 1.0f);
    physics_variable_speed_update_seek((float *)(vehicle + 0x430),
                                       definition + 0x330,
                                       *(float *)(vehicle + 0x22c), 1.0f);
  }

  if (*(int16_t *)(definition + 0x2f4) != 0) {
    if (*(float *)(vehicle + 0x42c) < 0.0f) {
      desired_position = -steering_angle;
    } else {
      desired_position = steering_angle;
    }
    if (desired_position < *(float *)(definition + 0x30c) * 0.017453292f) {
      desired_position = *(float *)(definition + 0x30c) * 0.017453292f;
    } else if (desired_position >
               *(float *)(definition + 0x308) * 0.017453292f) {
      desired_position = *(float *)(definition + 0x308) * 0.017453292f;
    }
    FUN_00154750((float *)(vehicle + 0x434), definition + 0x308, 0,
                 desired_position,
                 *(float *)(definition + 0x314) * 0.017453292f *
                     0.033333335f);
  } else if (*(float *)(vehicle + 0x42c) == 0.0f) {
    physics_variable_speed_update_seek((float *)(vehicle + 0x434),
                                       definition + 0x2f8, 0.0f, 1.0f);
  } else {
    target = steering_angle * 0.63661975f;
    if (target < -1.0f) {
      target = -1.0f;
    } else if (target > 1.0f) {
      target = 1.0f;
    }
    physics_variable_speed_update_seek(
        (float *)(vehicle + 0x434), definition + 0x2f8,
        target * *(float *)(definition + 0x2f8), 2.0f);
  }

  if (*(int *)(definition + 0x8c) != -1) {
    if (((*(uint32_t *)(definition + 0x2f0) & 1) != 0 &&
         *(float *)(vehicle + 0x42c) != 0.0f) ||
        ((*(uint32_t *)(definition + 0x2f0) & 2) != 0 &&
         *(float *)(vehicle + 0x434) != 0.0f) ||
        ((*(uint32_t *)(definition + 0x2f0) & 4) != 0 &&
         *(float *)(vehicle + 0x2e8) != 0.0f) ||
        ((*(uint32_t *)(definition + 0x2f0) & 8) != 0 &&
         *(float *)(vehicle + 0x2ec) != 0.0f) ||
        ((*(uint32_t *)(definition + 0x2f0) & 0x20) != 0 &&
         *(float *)(vehicle + 0x430) != 0.0f)) {
      *(uint32_t *)(vehicle + 4) &= ~0x20u;
    }
  }

  assert_halt_msg_at("global_current_collision_user_depth < "
                     "MAXIMUM_COLLISION_USER_STACK_DEPTH",
                     "c:\\halo\\SOURCE\\units\\vehicles.c", 0x134,
                     *(int16_t *)0x4761d8 < 0x20);
  /* global_current_collision_users[depth++] = 0x11 (vehicles). */
  ((int16_t *)0x5a8c80)[(*(int16_t *)0x4761d8)++] = 0x11;

  if (*(int *)(definition + 0x8c) != -1 &&
      (*(uint8_t *)(vehicle + 4) & 0x20) == 0) {
    previous_velocity = *(real_vector3d *)(vehicle + 0x18);
    switch (*(int16_t *)(definition + 0x2f4)) {
    case 0:
      update_human_tank_physics(vehicle_index, mass_points,
                                powered_mass_points);
      break;
    case 1:
      update_human_jeep_physics(vehicle_index, mass_points,
                                powered_mass_points);
      break;
    case 2:
      update_human_boat_physics(vehicle_index, mass_points,
                                powered_mass_points);
      break;
    case 3:
      update_human_plane_physics(vehicle_index, powered_mass_points,
                                 mass_points);
      break;
    case 4:
      update_alien_scout_physics(vehicle_index, steering_angle,
                                 powered_mass_points, mass_points);
      break;
    case 5:
      update_alien_fighter_physics(vehicle_index, (int)powered_mass_points,
                                   (int)mass_points);
      break;
    case 6:
      physics_update(vehicle_index, NULL, mass_points, NULL, NULL);
      break;
    }
    create_slipping_effects(vehicle_index, powered_mass_points, mass_points);
    if (!update_suspension(vehicle_index)) {
      create_crashing_effects(vehicle_index, &previous_velocity.i,
                              mass_points);
    }
    FUN_001b56b0(vehicle_index, mass_points);
    if ((*(uint32_t *)(vehicle + 4) & 0x20) != 0) {
      *(int16_t *)(vehicle + 0x426) = 15;
    }
    if ((*(uint32_t *)(vehicle + 4) & 0x1000000) == 0 &&
        ((1 << *(uint8_t *)(definition + 0x2f4)) & 0x28) != 0) {
      floor_z = *(float *)((char *)scenario_get() + 0x10);
      ceiling_z = *(float *)((char *)scenario_get() + 0x14);
      if (floor_z != 0.0f && *(float *)(vehicle + 0x14) < floor_z) {
        *(float *)(vehicle + 0x20) =
            ((floor_z - *(float *)(vehicle + 0x14)) * 0.015625f -
             *(float *)(vehicle + 0x20) * 0.0625f) *
                *(float *)(vehicle + 0x2e8) +
            *(float *)(vehicle + 0x20);
      }
      if (ceiling_z != 0.0f && *(float *)(vehicle + 0x14) > ceiling_z) {
        *(float *)(vehicle + 0x20) =
            *(float *)(vehicle + 0x20) -
            ((*(float *)(vehicle + 0x14) - ceiling_z) * 0.015625f +
             *(float *)(vehicle + 0x20) * 0.0625f) *
                *(float *)(vehicle + 0x2e8);
      }
    }
  } else if (*(int16_t *)(vehicle + 0x426) > 0) {
    slowly_stop_vehicle(vehicle_index);
    update_suspension(vehicle_index);
  }

  assert_halt_msg_at("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\units\\vehicles.c", 0x174,
                     *(int16_t *)0x4761d8 > 1);
  --*(int16_t *)0x4761d8;

  if ((*(uint8_t *)(definition + 0x2f0) & 0x40) != 0) {
    falling_damage = (char *)tag_block_get_element(
        (char *)game_globals_get() + 0x188, 0, 0x98);
    if (-*(float *)(falling_damage + 0x8c) > *(float *)(vehicle + 0x20)) {
      child_index = *(int *)(vehicle + 0xc8);
      while (child_index != -1) {
        child = (char *)object_get_and_verify_type(child_index, -1);
        damage_data_new(damage_data, *(int *)(falling_damage + 0x38));
        object_cause_damage(damage_data, child_index, -1, -1, -1, NULL);
        child_index = *(int *)(child + 0xc4);
      }
    }
  }

animate:
  if (*(int *)(definition + 0x44) != -1) {
    animation_update[0] = 0;
    animation_update[1] = 0;
    unit_update_animation(vehicle_index, animation_update);
  }

  blur = 1;
  if (!(*(float *)(definition + 0x318) <=
        (float)fabs(*(float *)(vehicle + 0x42c)))) {
    blur = 0;
  }
  if (blur != (*(uint8_t *)(vehicle + 0x424) & 1)) {
    object_permute_region(vehicle_index, "~blur", -1, blur);
    if (blur) {
      *(uint8_t *)(vehicle + 0x424) |= 1;
    } else {
      *(uint8_t *)(vehicle + 0x424) &= ~1;
    }
  }

  if (*(char *)0x449ef1 != '\0' && *(char *)0x32e4a8 != '\0') {
    profile_exit_private((void *)0x32e4a0);
  }

  return 1;
}
