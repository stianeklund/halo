/* units.c — unit lifecycle and query helpers.
 *
 * Corresponds to units.obj. Functions sorted by XBE address.
 */

#include "../../common.h"
#include "../../x87_math.h"

#define NUMBER_OF_UNIT_BASE_SEATS 6
#define NUMBER_OF_UNIT_BASE_WEAPONS 1
#define MAXIMUM_WEAPONS_PER_UNIT 4
#define MAXIMUM_COLLISION_USER_STACK_DEPTH 32

char *FUN_0008dc30(char *destination, const char *source)
{
  const char *source_cursor;
  char *destination_cursor;
  unsigned int source_size;

  if (destination == NULL || source == NULL) {
    display_assert("s1 && s2", "c:\\halo\\SOURCE\\cseries\\cseries.c", 0x122,
                   true);
    system_exit(-1);
  }

  source_cursor = source;
  while (*source_cursor != '\0') {
    source_cursor += 1;
  }
  source_size = (unsigned int)(source_cursor - source) + 1;

  destination_cursor = destination - 1;
  do {
    destination_cursor += 1;
  } while (*destination_cursor != '\0');

  {
    unsigned int i;
    for (i = source_size >> 2; i != 0; i -= 1) {
      *(uint32_t *)destination_cursor = *(const uint32_t *)source;
      source += 4;
      destination_cursor += 4;
    }
  }

  {
    unsigned int i;
    for (i = source_size & 3; i != 0; i -= 1) {
      *destination_cursor = *source;
      source += 1;
      destination_cursor += 1;
    }
  }

  return destination;
}

void FUN_00123470(void *mode_tag, void *animation, int animation_index,
                  void *out_matrix)
{
  uint8_t node_data[0x800];

  FUN_00121d60(mode_tag, animation, animation_index, node_data);
  component_vectors_from_normal3d(out_matrix, (float *)(node_data + 0x10),
                                  (float *)node_data);
}

/* FUN_001234b0 (0x1234b0) - animation_get_root_delta
 *
 * Computes the delta position between frame param_3 and frame (param_3-1)
 * of an animation. Uses _chkstk for 0x1000 bytes of stack.
 * Two 0x800-byte node data buffers are filled via FUN_00121d60.
 * The translation component (offset 0x10 from each buffer base) is
 * subtracted to produce the frame delta in param_4[0..2].
 *
 * Confirmed: 4 cdecl params, void return. _chkstk 0x1000 frame.
 */
void FUN_001234b0(void *mode_tag, void *animation, int frame_index,
                  float *out_delta)
{
  uint8_t frame_data[0x800];
  uint8_t prev_frame_data[0x800];
  int frame;

  if (*(short *)((char *)animation + 0x22) < 2) {
    display_assert("animation->frame_count>1",
                   "c:\\halo\\SOURCE\\models\\model_animations.c", 0xdd,
                   true);
    system_exit(-1);
  }

  frame = frame_index;
  if ((short)frame == 0) {
    frame = 1;
  }

  FUN_00121d60(mode_tag, animation, frame, frame_data);
  FUN_00121d60(mode_tag, animation, frame - 1, prev_frame_data);

  out_delta[0] = *(float *)(frame_data + 0x10) -
                 *(float *)(prev_frame_data + 0x10);
  out_delta[1] = *(float *)(frame_data + 0x14) -
                 *(float *)(prev_frame_data + 0x14);
  out_delta[2] = *(float *)(frame_data + 0x18) -
                 *(float *)(prev_frame_data + 0x18);
}

/* FUN_001a67b0 (0x1a67b0)
 *
 * Returns the animation state string for the given animation index (param_1)
 * and column (param_2, 0 or 1) from the global animation-name table at
 * 0x32d7c8. The table has 0xd1 rows of 2 char* pointers each.
 * Returns "<error>" if param_1 is out of range [0, 0xd0].
 *
 * Confirmed: MOVSX EAX,CX (sign-extends param_1); MOVZX ECX (zero-extends
 * param_2); LEA EDX,[ECX + EAX*2]; MOV EAX,[EDX*4 + 0x32d7c8].
 */
char *FUN_001a67b0(short param_1, unsigned char param_2)
{
  char *result;

  result = "<error>";
  if (param_1 >= 0 && param_1 < 0xd1) {
    result = ((char **)0x32d7c8)[(unsigned int)param_2 +
                                 (unsigned int)(short)param_1 * 2];
  }
  return result;
}


/* FUN_001a67e0 (0x1a67e0)
 *
 * Searches the global animation-name table at 0x32d7c8 for an entry whose
 * first string (column 0) matches param_1. The table has 0xd1 rows of 2
 * char* pointers each (stride = 8 bytes). Returns the row index [0, 0xd0]
 * on match, or -1 if not found.
 *
 * Confirmed: MOVSX EAX,SI; MOV ECX,[EAX*8 + 0x32d7c8]; PUSH EDI (param_1);
 * PUSH ECX; CALL csstrcmp; TEST EAX,EAX; JZ found; INC ESI; CMP SI,0xd1;
 * JL loop. Found: MOV AX,SI; Not-found: MOV AX,BX (BX = -1 via OR EBX,-1).
 */
short FUN_001a67e0(const char *param_1)
{
  short sVar2;
  int iVar1;

  sVar2 = 0;
  do {
    iVar1 = csstrcmp(((const char **)0x32d7c8)[(int)sVar2 * 2], param_1);
    if (iVar1 == 0) {
      return sVar2;
    }
    sVar2 = sVar2 + 1;
  } while (sVar2 < 0xd1);
  return -1;
}


/* FUN_001a6820 (0x1a6820)
 *
 * Returns verify_tag_reference result for the animation at index bool(param_2)
 * (0 or 1, clamped to count-1) from the tag block at param_1+0x2a8.
 * Returns -1 if clamped index is negative (count == 0).
 *
 * Confirmed: MOV AL,[ebp+0xc]; XOR ECX,ECX; TEST AL,AL; MOV EAX,[edx+0x2a8];
 * SETNE CL; ADD EDX,0x2a8; DEC EAX; MOVSX ECX,CX; CMP ECX,EAX; JG skip;
 * MOV EAX,ECX; TEST AX,AX; JGE proceed; OR EAX,-1; RET.
 */
int FUN_001a6820(int param_1, char param_2)
{
  int iVar1;
  int *block;
  int bVal;

  bVal = (int)(short)(unsigned short)(param_2 != '\0');
  iVar1 = *(int *)(param_1 + 0x2a8) - 1;
  block = (int *)(param_1 + 0x2a8);
  if (bVal <= iVar1) {
    iVar1 = bVal;
  }
  if ((short)iVar1 < 0) {
    return -1;
  }
  return (int)verify_tag_reference(
    (int *)tag_block_get_element(block, (int)(short)iVar1, 0x30));
}

/* FUN_001a6870 (0x1a6870)
 *
 * Returns verify_tag_reference result for the animation at index bool(param_3)
 * (0 or 1, clamped) from a nested tag block. First gets element param_2 from
 * the block at param_1+0x2e4 (stride 0x11c), then reads sub-block at +0xdc.
 * Returns -1 if clamped index is negative.
 *
 * Confirmed: MOVSX EAX,CX (param_2); ADD ECX,0x2e4; CALL tag_block_get_element;
 * MOV DL,[ebp+0x10] (param_3); XOR ECX,ECX; TEST DL,DL; SETNE CL;
 * LEA EDX,[EAX+0xdc]; MOV EAX,[EDX]; DEC EAX; MOVSX ECX,CX; ...
 */
int FUN_001a6870(int param_1, short param_2, char param_3)
{
  int iVar1;
  int iVar2;
  int *block;
  int bVal;

  iVar1 =
    (int)tag_block_get_element((void *)(param_1 + 0x2e4), (int)param_2, 0x11c);
  bVal = (int)(short)(unsigned short)(param_3 != '\0');
  iVar2 = *(int *)(iVar1 + 0xdc) - 1;
  block = (int *)(iVar1 + 0xdc);
  if (bVal <= iVar2) {
    iVar2 = bVal;
  }
  if ((short)iVar2 < 0) {
    return -1;
  }
  return (int)verify_tag_reference(
    (int *)tag_block_get_element(block, (int)(short)iVar2, 0x30));
}

/* FUN_001a6bc0 (0x1a6bc0)
 *
 * Returns non-zero if the unit's animation count field at offset 0x338
 * (a signed short) is greater than zero. Uses object_get_and_verify_type
 * with type_mask=3 (biped|vehicle).
 *
 * Confirmed: CALL 0x13d680 (object_get_and_verify_type) with type_mask=3;
 * CMP word ptr [EAX+0x338],CX (CX=0); SETG CL; MOV AL,CL.
 */
int FUN_001a6bc0(int param_1)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(param_1, 3);
  return *(short *)(unit + 0x338) > 0;
}

/* FUN_001a6ca0 (0x1a6ca0)
 *
 * Returns the dialogue-category string for the given index (param_1) from the
 * global pointer table at 0x32de50. The table has 0xb entries (indices 0-10).
 * Returns "<error>" if param_1 is out of range.
 *
 * Confirmed: MOVSX EAX,CX (sign-extends param_1); MOV EAX,[EAX*4 + 0x32de50].
 */
char *FUN_001a6ca0(short param_1)
{
  char *result;

  result = "<error>";
  if (param_1 >= 0 && param_1 < 0xb) {
    result = ((char **)0x32de50)[(short)param_1];
  }
  return result;
}

/* FUN_001a6cd0 (0x1a6cd0)
 *
 * Searches the 11-entry dialogue-category pointer table at 0x32de50 for a
 * case-sensitive match with param_1 using csstrcmp. Returns the index (0–10)
 * on success. On no match, returns 0 (BX register held zero throughout).
 *
 * Confirmed: XOR EBX,EBX; XOR ESI,ESI; MOVSX EAX,SI; MOV ECX,[EAX*4+table];
 * PUSH EDI; PUSH ECX; CALL csstrcmp; TEST EAX,EAX; JE found;
 * INC ESI; CMP SI,0xb; JL loop; MOV AX,BX; RET.
 * found: MOV AX,SI; RET.
 */
short FUN_001a6cd0(const char *param_1)
{
  unsigned short result;
  unsigned short i;

  result = 0;
  i = 0;
  do {
    if (csstrcmp(((char **)0x32de50)[(short)i], param_1) == 0) {
      return (short)i;
    }
    i++;
  } while ((short)i < 0xb);
  return (short)result;
}

/* FUN_001a6d10 (0x1a6d10) — unit_dialogue_format_speech_name
 *
 * Formats the current speech sound name for the given unit.
 * If the unit has no active speech (speech_count at +0x338 is 0),
 * outputs "<none>". Otherwise looks up the sound tag name via
 * tag_get_name(+0x33c), strips path components based on full_path flag,
 * and optionally prepends the dialogue variant name from FUN_001a67b0.
 *
 * full_path=0: uses strrchr to find last backslash (show filename only)
 * full_path!=0: uses strchr loop to strip all path prefix (show leaf)
 *
 * Returns the output buffer pointer.
 *
 * Confirmed: cdecl, 4 stack params (ADD ESP cleanup at callers).
 * Confirmed: FUN_001d9179 = snprintf, FUN_001ba1f0 = tag_get_name.
 * Confirmed: 0x257984 = "%s", 0x259f40 = "%s %s", 0x25ad08 = "<none>".
 */
char *FUN_001a6d10(int unit_handle, char full_path, int16_t max_len,
                   char *output)
{
  char *unit;
  char *current_name;
  char *found;
  char *variant_name;
  int16_t dialogue_index;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (*(int16_t *)(unit + 0x338) == 0) {
    snprintf(output, (int)max_len, "<none>");
    return output;
  }

  current_name = "<none>";
  if (*(int *)(unit + 0x33c) != -1) {
    current_name = (char *)tag_get_name(*(int *)(unit + 0x33c));
  }

  if (full_path == '\0') {
    found = strrchr(current_name, '\\');
    if (found != NULL) {
      current_name = found + 1;
    }
  } else {
    while (current_name != NULL) {
      found = crt_strchr(current_name, '\\');
      if (found == NULL) {
        break;
      }
      current_name = found + 1;
    }
  }

  dialogue_index = *(int16_t *)(unit + 0x33a);
  if (dialogue_index != -1) {
    if (full_path == '\0') {
      variant_name = FUN_001a67b0(dialogue_index, 0);
      snprintf(output, (int)max_len, "%s", variant_name);
      return output;
    }
    variant_name = FUN_001a67b0(dialogue_index, 0);
    snprintf(output, (int)max_len, "%s %s", variant_name, current_name);
    return output;
  }

  snprintf(output, (int)max_len, "%s", current_name);
  return output;
}

/* FUN_001a6e20 (0x1a6e20) — unit_dialogue_log_lost_speech
 *
 * Logs a "lost speech" debug message when a speech item is being replaced.
 * Only produces output when the debug flag at 0x5aca56 is nonzero.
 *
 * Looks up the unit's tag name via tag_get('unit'), then resolves the speech
 * item's dialogue name either via FUN_001a67b0 (if speech_item+2 != -1) or
 * via tag_get_name (if speech_item+4 != -1), falling back to "<unknown>".
 * Logs "lost <waiting|queued> speech <name>" via console_printf.
 *
 * Register args: unit_handle @<eax>, speech_item @<ecx>.
 * Stack arg: priority (short) — 2 = "waiting", otherwise "queued".
 *
 * Confirmed: PUSH EAX (unit_handle) / MOV ESI,ECX (speech_item) at entry.
 * Confirmed: PUSH 0x3 for object_get_and_verify_type.
 * Confirmed: CMP word [EBP+0x8],0x2 for priority check.
 * Confirmed: tag_get(0x756e6974, *unit) for unit tag name.
 * Confirmed: console_printf(0, "%s: lost %s speech %s", ...) at 0xff4d0.
 */
void FUN_001a6e20(int unit_handle, void *speech_item, short priority)
{
  char *unit;
  char *unit_tag;
  char *speech_name;
  char *lost_type;
  int16_t dialogue_index;
  int tag_index;
  char *found;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (speech_item == NULL) {
    display_assert("speech_item",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x415, 1);
    system_exit(-1);
  }
  if (*(char *)0x5aca56 != '\0') {
    unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
    dialogue_index = *(int16_t *)((char *)speech_item + 2);
    if (dialogue_index != (int16_t)-1) {
      speech_name = (char *)FUN_001a67b0(dialogue_index, 0);
    } else {
      tag_index = *(int *)((char *)speech_item + 4);
      if (tag_index != -1) {
        speech_name = (char *)tag_get_name(tag_index);
        found = strrchr(speech_name, '\\');
        if (found != NULL) {
          speech_name = found + 1;
        }
      } else {
        speech_name = "<unknown>";
      }
    }
    if (priority == 2) {
      lost_type = "waiting";
    } else {
      lost_type = "queued";
    }
    console_printf(0, "%s: lost %s speech %s",
                   *(char **)(unit_tag + 0x2c), lost_type, speech_name);
  }
}

/* FUN_001a6ef0 (0x1a6ef0) — unit_dialogue_queue_speech_item
 *
 * Queues or promotes a speech item for the unit's dialogue system.
 * Three priority levels (param_2):
 *   1 = queue into backup slot (unit+0x368)
 *   2 = promote to current slot (unit+0x338), evicting existing if present
 *   3 = promote to current and clear backup if identical
 *
 * Copies 0x30 bytes (12 dwords, REP MOVSD with ECX=0xC) of speech data.
 * After promotion, copies timing fields from the speech item and computes
 * a vocalization timer from the sound tag's duration:
 *   timer = (duration_ms * 30) / 1000  (converting ms to 30Hz ticks).
 * If no sound tag is set, asserts unless it's a standalone server
 * or force_vocalizations is off.
 *
 * Confirmed: ADD ESP,0x8 after object_get_and_verify_type (cdecl, 2 args).
 * Confirmed: REP MOVSD with ECX=0xC for 0x30-byte copy.
 * Confirmed: IMUL ECX,0x1e / MUL 0x10624dd3 / SAR 0x6 = divide by 1000.
 * Confirmed: assert "speech_item" at line 0x12E, file unit_dialogue.c.
 * Confirmed: assert "AI_BEHAVIOR(force_vocalizations)" at line 0x168.
 * Confirmed: assert "unit->unit.speech.current.priority > _unit_speech_none"
 *            at line 0x16E.
 */
void FUN_001a6ef0(int unit_handle, short priority, void *speech_item)
{
  char *unit;
  int sound_tag_index;
  char *sound_tag;
  int duration_ticks;
  short game_conn;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (speech_item == NULL) {
    display_assert("speech_item",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x12e, 1);
    system_exit(-1);
  }

  /* If unit has AI-controlled speech bit set, only allow priority 10 (override) */
  if ((*(uint8_t *)(unit + 0xb6) & 4) != 0 &&
      *(int16_t *)speech_item != 10) {
    return;
  }

  if (priority >= 2) {
    /* Promoting to current slot — log existing speech being evicted */
    if (*(int16_t *)(unit + 0x338) > 0 &&
        *(char *)(unit + 0x3a4) == '\0') {
      FUN_001a6e20(unit_handle, unit + 0x338, 2);
    }

    /* Copy 0x30 bytes of speech data to current slot (unit+0x338) */
    memcpy(unit + 0x338, speech_item, 0x30);

    /* If priority == 3, clear backup slot if it exists and isn't the same */
    if (priority == 3 && *(int16_t *)(unit + 0x368) > 0) {
      if (speech_item != (void *)(unit + 0x368)) {
        FUN_001a6e20(unit_handle, unit + 0x368, 1);
      }
      *(int16_t *)(unit + 0x368) = 0;
    }

    /* Copy timing/state fields from the speech item */
    *(uint16_t *)(unit + 0x3a8) = *(uint16_t *)(unit + 0x340);
    *(uint8_t *)(unit + 0x3a4) = 0;
    *(uint8_t *)(unit + 0x3a5) = 0;
    *(uint8_t *)(unit + 0x3a6) = 0;
    *(uint32_t *)(unit + 0x3b0) = 0xffffffff;
    *(uint16_t *)(unit + 0x3ae) = *(uint16_t *)(unit + 0x344);
    *(uint16_t *)(unit + 0x3ac) = *(uint16_t *)(unit + 0x342);

    /* Compute vocalization timer from sound tag duration */
    sound_tag_index = *(int *)(unit + 0x33c);
    if (sound_tag_index != -1) {
      sound_tag = (char *)tag_get(0x736e6421, sound_tag_index);
      duration_ticks = *(int *)(sound_tag + 0x84) * 30;
      *(int16_t *)(unit + 0x3aa) = (int16_t)(duration_ticks / 1000);
      return;
    }
    game_conn = game_connection();
    if (game_conn == 0 && *(char *)0x5ac9cd != '\0') {
      /* Standalone with force_vocalizations off — use default 45 ticks */
    } else {
      display_assert("AI_BEHAVIOR(force_vocalizations)",
                     "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x168, 1);
      system_exit(-1);
    }
    *(int16_t *)(unit + 0x3aa) = 0x2d;
    return;
  }

  if (priority == 1) {
    /* Queue to backup slot */
    if (*(int16_t *)(unit + 0x338) < 1) {
      display_assert(
          "unit->unit.speech.current.priority > _unit_speech_none",
          "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x16e, 1);
      system_exit(-1);
    }
    memcpy(unit + 0x368, speech_item, 0x30);
  }
}

/* FUN_001a71c0 (0x1a71c0) — unit_dialogue_activation
 *
 * Complex dialogue activation function. Determines the appropriate
 * dialogue type based on the unit's combat situation (seeing enemy,
 * taking damage, proximity to threats). Handles both voluntary (param_3=0)
 * and involuntary (param_3=1) dialogue with different priority systems.
 *
 * For voluntary speech: checks cooldown timers at +0x39e/0x39c/0x39a,
 * random suppression, and maps vehicle/threat state to dialogue type.
 * For involuntary speech: checks weapon projectile data to determine
 * if the projectile is fast, uses AI actor state for threat level.
 *
 * On success, builds a speech item (0x30 bytes) and queues it via
 * FUN_001a6ef0. Also optionally fires an AI unit effect via FUN_00040860.
 *
 * Returns 1 on success, 0 on failure.
 *
 * Confirmed: cdecl, 5 stack params.
 * Confirmed: object_get_and_verify_type(unit_handle, 3).
 * Confirmed: dialogue tag at unit+0x334.
 * Confirmed: 0x253f3c = 0.6f, 0x253524 = 0.4f, 0x2549d4 = 0.2f.
 * Confirmed: speech_buf at [EBP-0x44] (0x30 bytes).
 * Confirmed: DAT_005ac9ca = dialogue mute flag.
 * Confirmed: DAT_006325a4 = actor data pointer.
 */
char FUN_001a71c0(int unit_handle, int *param_2, char param_3, char param_4,
                  float param_5)
{
  char *unit;
  char result;
  char is_above_zero;
  char is_above_threshold;
  int dialogue_type;
  int dialogue_obj_handle;
  int16_t ai_effect_type;
  int ai_effect_count;
  char speech_buf[0x30];
  char *tag_data;
  int actor_handle;
  char is_fast_projectile;
  char has_threat;
  char urgent;
  int16_t priority;
  short speech_count;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  result = 0;

  if (*(int *)(unit + 0x334) == -1)
    return 0;

  dialogue_type = 0;
  is_above_zero = *(float *)(unit + 0xa8) > *(float *)0x2533c0;
  is_above_threshold = *(float *)(unit + 0xa8) >= *(float *)0x253f3c;

  urgent = 0;
  ai_effect_type = -1;
  ai_effect_count = 0;

  if (param_2 != NULL && *param_2 != -1) {
    tag_data = (char *)tag_get(0x6a707421, *param_2);
    dialogue_type = (int)*(int16_t *)(tag_data + 0x1c6);
  }

  if (param_3 != '\0') {
    /* Involuntary dialogue (being shot at) */
    actor_handle = *(int *)(unit + 0x1a8);
    if (actor_handle == -1)
      actor_handle = *(int *)(unit + 0x1a4);

    has_threat = 0;
    is_fast_projectile = 0;
    if (*param_2 != -1) {
      tag_data = (char *)tag_get(0x6a707421, *param_2);
      is_fast_projectile = 1;
      if (*(float *)(tag_data + 0x1f4) < *(float *)0x253f40)
        is_fast_projectile = 0;
    }

    if (actor_handle == -1) {
      has_threat = param_5 + *(float *)0x2549d4 < *(float *)(unit + 0xa8);
    } else {
      tag_data = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
      has_threat = *(int16_t *)(tag_data + 0x6e) >= 3;
    }

    if ((int16_t)dialogue_type == 1) {
      dialogue_type = 0x10;
    } else if ((int16_t)dialogue_type == 7) {
      dialogue_type = 0x11;
    } else if (is_fast_projectile != 0) {
      dialogue_type = 0x13;
    } else if (!has_threat) {
      dialogue_type = 0xe;
    } else {
      dialogue_type = (param_4 != '\0' ? 3 : 0) + 0xf;
    }

    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    urgent = 1;

    if ((int16_t)dialogue_type != 0xe) {
      ai_effect_type = 2;
      ai_effect_count = ((int16_t)dialogue_type == 0x12) ? 4 : 1;
    }

    if ((int16_t)dialogue_type == -1)
      goto ai_effect;
  } else {
    /* Voluntary dialogue (spotting enemy) */
    if (*(int16_t *)(unit + 0x39e) != 0)
      return result;

    if ((int16_t)dialogue_type == 1) {
      urgent = 1;
      dialogue_type = 9;
    } else if (is_above_threshold) {
      urgent = 1;
      dialogue_type = 7;
    } else {
      if (*(int16_t *)(unit + 0x39c) != 0)
        return result;

      if (*(int16_t *)(unit + 0x39a) > 2)
        return result;

      if (*(int16_t *)(unit + 0x338) != 0) {
        if (random_math_real(
                (unsigned int *)get_global_random_seed_address()) >=
            *(float *)0x253524)
          return result;
      }

      dialogue_type = (is_above_zero == 0 ? 2 : 0) + 6;
    }

    if ((int16_t)dialogue_type == -1)
      goto ai_effect;
  }

  dialogue_obj_handle = -1;
  if (param_3 == '\0') {
    priority = (int16_t)(urgent ? 7 : 2);
  } else {
    priority = 10;
  }

  speech_count = FUN_001a68d0(unit_handle, (int)priority, 1, 0, 0,
                              (short *)&dialogue_type, &dialogue_obj_handle);

  if (*(char *)0x5ac9ca == '\0' && speech_count > 0) {
    csmemset(speech_buf, 0, 0x30);
    *(int16_t *)(speech_buf + 0x00) = priority;
    *(int16_t *)(speech_buf + 0x02) = (int16_t)dialogue_type;
    *(int *)(speech_buf + 0x04) = dialogue_obj_handle;
    *(int16_t *)(speech_buf + 0x0c) = 7;
    ai_communication_packet_new(speech_buf + 0x10);
    FUN_001a6ef0(unit_handle, speech_count, speech_buf);
    result = 1;

    if (is_above_threshold == 0) {
      *(int16_t *)(unit + 0x39a) += 1;
      *(int16_t *)(unit + 0x39c) = 0x1e;
      *(int16_t *)(unit + 0x398) = 0x16;
    } else {
      *(int16_t *)(unit + 0x39e) = 0x3c;
    }
  }

ai_effect:
  if (ai_effect_type != -1) {
    ai_handle_unit_effect(unit_handle, (int)ai_effect_type, ai_effect_count);
  }

  return result;
}

/* FUN_001a74d0 (0x1a74d0) — unit_dialogue_scream
 *
 * Triggers a unit scream dialogue event. Maps scream_type [0..5] to a
 * dialogue index via switch:
 *   0 → 10 (pain_body_minor)
 *   1 → random 50/50: 0x27 (scream) or fall through to case 2
 *   2 → 11 (pain_body_major)
 *   3 → 12 (pain_shield)
 *   4 → 13 (pain_falling)
 *   5 → 0xb7 (183, death)
 * Then looks up the dialogue tag at unit+0x334, fetches the sound reference
 * at dialogue_index*0x10+0x1c, and calls FUN_001a68d0 to allocate a speech
 * slot. On success, builds a speech item struct (0x30 bytes) and queues it
 * via FUN_001a6ef0.
 *
 * Returns 1 on success, 0 if no dialogue tag or sound not available.
 *
 * Confirmed: cdecl, 2 stack params.
 * Confirmed: switch table at 0x1a7638 (6 entries).
 * Confirmed: 0x253398 = 0.5f for random threshold.
 * Confirmed: FUN_00042d20 = ai_communication_packet_new.
 * Confirmed: merged ADD ESP,0x1c cleans csmemset(3)+packet_new(1)+6ef0(3).
 */
char FUN_001a74d0(int unit_handle, int scream_type)
{
  char *unit;
  int16_t dialogue_index;
  int dialogue_tag_index;
  char *dialogue_tag;
  uint32_t sound_ref;
  short result;
  char speech_buf[0x30];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  dialogue_index = (int16_t)scream_type;

  if ((int16_t)scream_type < 0 || (int16_t)scream_type >= 6) {
    display_assert(
        "(scream_type >= 0) && (scream_type < NUMBER_OF_UNIT_SCREAM_TYPES)",
        "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 599, 1);
    system_exit(-1);
  }

  switch ((int16_t)scream_type) {
  case 0:
    dialogue_index = 10;
    break;
  case 1:
    if (random_math_real((unsigned int *)get_global_random_seed_address()) < 0.5f) {
      dialogue_index = 0x27;
      break;
    }
    /* fall through */
  case 2:
    dialogue_index = 0xb;
    break;
  case 3:
    dialogue_index = 0xc;
    break;
  case 4:
    dialogue_index = 0xd;
    break;
  case 5:
    dialogue_index = 0xb7;
    break;
  default:
    display_assert("!\"unreachable\"",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x27b, 1);
    system_exit(-1);
  }

  dialogue_tag_index = *(int *)(unit + 0x334);
  if (dialogue_tag_index != -1) {
    dialogue_tag = (char *)tag_get(0x75646c67, dialogue_tag_index);
    sound_ref = *(uint32_t *)(dialogue_tag + (int)dialogue_index * 0x10 + 0x1c);
    if (sound_ref != 0xffffffff) {
      result = FUN_001a68d0(unit_handle, 9, 1, 0, 0, &dialogue_index,
                            (int *)&sound_ref);
      if (result > 0) {
        csmemset(speech_buf, 0, 0x30);
        *(int16_t *)(speech_buf + 0x00) = 9;
        *(int16_t *)(speech_buf + 0x02) = dialogue_index;
        *(uint32_t *)(speech_buf + 0x04) = sound_ref;
        *(int16_t *)(speech_buf + 0x0c) = 7;
        ai_communication_packet_new(speech_buf + 0x10);
        FUN_001a6ef0(unit_handle, result, speech_buf);
        return 1;
      }
    }
  }
  return 0;
}

/* FUN_001a7650 (0x1a7650) — dialogue_definition_get_variant
 *
 * Searches the dialogue variants tag block at tag_data+0x2b4 for entries
 * matching the given dialogue_type. Collects up to 16 matching variant
 * indices into a local array. If exactly one match, returns that variant's
 * dialogue tag index. If multiple matches, picks one at random using
 * get_global_random_seed_address. Asserts if the chosen variant's dialogue
 * tag (0x75646c67 = "udlg") fails to load.
 *
 * Returns the dialogue tag index, or -1 if no matching variant found.
 *
 * Confirmed: thiscall — @ecx = tag_data pointer (MOV ESI,ECX at 0x1a7658).
 * Confirmed: 1 stack param = dialogue_type (int16_t).
 * Confirmed: tag_block at tag_data+0x2b4, element_size=0x18.
 * Confirmed: variant dialogue ref at element+0x14.
 * Confirmed: assert string "dialogue_definition_get(dialogue_index)" at 0x2b6874.
 * Confirmed: line 0x408 in unit_dialogue.c.
 */
int FUN_001a7650(void *tag_data, int dialogue_type)
{
  int *block;
  int count;
  int match_count;
  int16_t match_indices[16];
  int i;
  short *element;
  int16_t chosen;
  int chosen_element;
  int dialogue_tag_index;

  block = (int *)((char *)tag_data + 0x2b4);
  count = *block;
  match_count = 0;

  if (count > 0) {
    for (i = 0; i < count; i++) {
      element = (short *)tag_block_get_element(block, i, 0x18);
      if ((int16_t)dialogue_type == -1 || *element == (int16_t)dialogue_type) {
        match_indices[match_count] = (int16_t)i;
        match_count++;
      }
    }

    if ((int16_t)match_count > 0) {
      if ((int16_t)match_count == 1) {
        chosen = match_indices[0];
      } else {
        chosen = random_range(
            (unsigned int *)get_global_random_seed_address(),
            0, (int16_t)match_count);
        chosen = match_indices[chosen];
      }

      chosen_element = (int)tag_block_get_element(block, (int)chosen, 0x18);
      dialogue_tag_index = *(int *)(chosen_element + 0x14);

      if (tag_get(0x75646c67, dialogue_tag_index) == NULL) {
        display_assert("dialogue_definition_get(dialogue_index)",
                       "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x408, 1);
        system_exit(-1);
      }

      return dialogue_tag_index;
    }
  }

  return -1;
}

/* FUN_001a7730 (0x1a7730) — unit_dialogue_select_variant
 *
 * Selects a dialogue variant for the unit. Queries FUN_001a7650 with the
 * unit's current dialogue type (+0x6e). If that fails (returns -1), tries
 * type 0 (default). If that also fails, tries type -1 (wildcard).
 * Stores the resulting dialogue tag index at unit+0x334.
 *
 * Confirmed: @eax = unit_handle (PUSH EAX at 001a7734).
 * Confirmed: FUN_001a7650 takes @ecx = tag data pointer, stack param = type.
 * Confirmed: three fallback calls in sequence.
 * Confirmed: result stored at [ESI + 0x334].
 */
void FUN_001a7730(int unit_handle)
{
  char *unit;
  char *tag_data;
  int result;
  int16_t dialogue_type;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  tag_data = (char *)tag_get(0x756e6974, *(int *)unit);

  dialogue_type = *(int16_t *)(unit + 0x6e);
  if (dialogue_type > 0) {
    result = FUN_001a7650(tag_data, (int)dialogue_type);
    if (result != -1) {
      *(int *)(unit + 0x334) = result;
      return;
    }
  }

  result = FUN_001a7650(tag_data, 0);
  if (result == -1) {
    result = FUN_001a7650(tag_data, -1);
  }
  *(int *)(unit + 0x334) = result;
}

/* unit_set_actively_controlled_flag (0x1a7f80)
 *
 * Sets bit 5 (0x20) of the byte at object_data_t+0xb6 (offset 182,
 * the byte just before unk_183) on the resolved unit object.
 * Distinct from unit_delete (0x1a7fc0) which sets the same bit at
 * offset 0xb7 (unk_183).
 *
 * Confirmed: CALL 0x13d680 with type_mask=3 (biped|vehicle).
 * Confirmed: OR byte ptr [EAX+0xb6],0x20.
 */
void unit_set_actively_controlled_flag(int unit_handle)
{
  object_data_t *obj;

  obj = (object_data_t *)object_get_and_verify_type(unit_handle, 3);
  *(uint8_t *)((char *)obj + 0xb6) |= 0x20;
}

/* unit_delete (0x1a7fc0)
 *
 * Marks a unit object for deletion by setting bit 5 (0x20) of the
 * object flags byte at object_data_t.unk_183 (offset 0xB7). The
 * actual object destruction happens later during the object system's
 * garbage-collection pass. Resolves handle via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
void unit_delete(int datum_handle)
{
  object_data_t *obj;

  obj = (object_data_t *)object_get_and_verify_type(datum_handle, 3);
  obj->unk_183 |= 0x20;
}

/* units_update (0x1a7ff0)
 *
 * Per-tick global update for the units subsystem. Reads the units globals
 * pointer at 0x4e4cf8 (a small {int16_t, int16_t, uint8_t} struct) and
 * rotates: copies the "max ticks this frame" (offset +2) into "current
 * ticks" (offset +0), then zeroes the max-ticks field (+2) and the
 * ready-flag byte (+4). Called once per game tick from the main update loop.
 */
void units_update(void)
{
  int16_t *p = *(int16_t **)0x4e4cf8;

  p[0] = p[1];
  p[1] = 0;
  *(uint8_t *)&p[2] = 0;
}

/* unit_persistent_control (0x1a8190)
 *
 * Sets persistent control state on a unit. Stores animation_ticks at offset
 * 0x1c0 and control_flags at offset 0x1c4. Asserts if control_flags has any
 * bits set beyond position 14 (NUMBER_OF_UNIT_CONTROL_FLAGS = 15).
 */
void unit_persistent_control(int unit_handle, int animation_ticks,
                             int control_flags)
{
  char *unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if ((control_flags & 0xffff8000) != 0) {
    display_assert(
      "VALID_FLAGS(persistent_control_flags, NUMBER_OF_UNIT_CONTROL_FLAGS)",
      "c:\\halo\\SOURCE\\units\\units.c", 0x605, 1);
    system_exit(-1);
  }

  *(int *)(unit + 0x1c4) = control_flags;
  *(int *)(unit + 0x1c0) = animation_ticks;
}

int unit_get_seat_enter_position(int unit_handle, int target_unit_handle,
                                 int16_t seat_index, float *out_pos_a,
                                 float *out_pos_b, float *out_pos_c)
{
  char marker_name[256];
  uint8_t hint_marker_data[0x6c];
  uint8_t mode_matrix[0x34];
  uint8_t enter_position_matrix[0x34];
  uint8_t seat_marker_data[0xa0];
  char *unit = (char *)object_get_and_verify_type(unit_handle, 3);
  char *unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  void *mode_tag = (void *)tag_get(0x6d6f6465, *(int *)(unit_tag + 0x34));
  char *antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  char *target_unit = (char *)object_get_and_verify_type(target_unit_handle, 3);
  char *target_unit_tag = (char *)tag_get(0x756e6974, *(int *)target_unit);
  char *seat = (char *)tag_block_get_element(target_unit_tag + 0x2e4,
                                             (int)seat_index, 0x11c);
  int *mode_block = (int *)(antr_tag + 0xc);
  int16_t mode_index = 0;

  while ((int)mode_index < *mode_block) {
    char *mode =
      (char *)tag_block_get_element(mode_block, (int)mode_index, 0x64);

    if (crt_stricmp(mode, seat + 4) == 0) {
      if (*(int *)(mode + 0x40) < 8) {
        return 0;
      }

      mode_index = *(int16_t *)(*(int *)(mode + 0x44) + 0xe);
      if (mode_index == -1) {
        return 0;
      }

      mode =
        (char *)tag_block_get_element(antr_tag + 0x74, (int)mode_index, 0xb4);
      object_get_markers_by_string_id(target_unit_handle, seat + 0x24,
                                      seat_marker_data, 1);
      FUN_00123470(mode_tag, mode, 0, mode_matrix);
      matrix4x3_multiply((float *)(seat_marker_data + 0x38),
                         (float *)mode_matrix, (float *)enter_position_matrix);
      csstrcpy(marker_name, seat + 0x24);
      FUN_0008dc30(marker_name, " enter-hint");
      object_get_markers_by_string_id(target_unit_handle, marker_name,
                                      hint_marker_data, 1);

      if (out_pos_b != NULL) {
        out_pos_b[0] = *(float *)(seat_marker_data + 0x60);
        out_pos_b[1] = *(float *)(seat_marker_data + 0x64);
        out_pos_b[2] = *(float *)(seat_marker_data + 0x68);
      }

      if (out_pos_a != NULL) {
        out_pos_a[0] = *(float *)(enter_position_matrix + 0x28);
        out_pos_a[1] = *(float *)(enter_position_matrix + 0x2c);
        out_pos_a[2] = *(float *)(enter_position_matrix + 0x30);
      }

      if (out_pos_c != NULL) {
        out_pos_c[0] = *(float *)(hint_marker_data + 0x60);
        out_pos_c[1] = *(float *)(hint_marker_data + 0x64);
        out_pos_c[2] = *(float *)(hint_marker_data + 0x68);
      }

      return 1;
    }

    mode_index += 1;
  }

  return 0;
}

/* FUN_001a8550 (0x1a8550) — acceleration plan evaluator
 *
 * Evaluates a 3-phase acceleration plan (accelerate, coast, decelerate)
 * over a given time delta. Each phase consumes time from delta_time and
 * updates position and velocity accordingly:
 *   Phase 1 (acceleration): v += accel*t, pos += (0.5*accel*t + vel)*t
 *   Phase 2 (coast):        pos += vel*t
 *   Phase 3 (deceleration): v += accel*t, pos += (0.5*accel*t + vel)*t
 *
 * Returns 1 if all time was consumed (plan still active), 0 if plan
 * complete or flag was already set.
 *
 * Confirmed: thiscall — @ecx = plan struct pointer.
 * Confirmed: 5 stack params: delta_time, position, out_pos*, velocity, out_vel*.
 * Confirmed: 0x253398 = 0.5f, 0x2533c0 = 0.0f.
 * Confirmed: plan+0x0c = accel1, +0x10 = dur1, +0x14 = dur2.
 * Confirmed: plan+0x18 = accel3, +0x1c = dur3.
 */
char FUN_001a8550(void *plan, float delta_time, float position, float *out_position,
                  float velocity, float *out_velocity)
{
  char done;
  float t;
  float vel;

  done = *(char *)plan;
  vel = velocity;

  if (done != 0 || !(*(float *)0x2533c0 < delta_time))
    goto store_out;

  /* Phase 1: acceleration */
  if (*(float *)0x2533c0 < *(float *)((char *)plan + 0x10)) {
    t = delta_time;
    if (*(float *)((char *)plan + 0x10) < delta_time)
      t = *(float *)((char *)plan + 0x10);
    position = (t * *(float *)((char *)plan + 0xc) * 0.5f + vel) * t + position;
    vel = t * *(float *)((char *)plan + 0xc) + vel;
    delta_time = delta_time - t;
  }

  if (!(*(float *)0x2533c0 < delta_time))
    goto store_out;

  /* Phase 2: coast */
  if (*(float *)0x2533c0 < *(float *)((char *)plan + 0x14)) {
    t = delta_time;
    if (*(float *)((char *)plan + 0x14) < delta_time)
      t = *(float *)((char *)plan + 0x14);
    position = vel * t + position;
    delta_time = delta_time - t;
  }

  if (!(*(float *)0x2533c0 < delta_time))
    goto store_out;

  /* Phase 3: deceleration */
  if (*(float *)0x2533c0 < *(float *)((char *)plan + 0x1c)) {
    t = delta_time;
    if (*(float *)((char *)plan + 0x1c) < delta_time)
      t = *(float *)((char *)plan + 0x1c);
    position = (0.5f * t * *(float *)((char *)plan + 0x18) + vel) * t + position;
    vel = t * *(float *)((char *)plan + 0x18) + vel;
    delta_time = delta_time - t;
  }

  if (*(float *)0x2533c0 < delta_time) {
    *out_position = position;
    *out_velocity = vel;
    return 1;
  }

store_out:
  *out_position = position;
  *out_velocity = vel;
  return done;
}

/* unit_animation_start_action (0x1a8990)
 *
 * Sets the unit's seated animation state by looking up an animation index from
 * the unit's animation tag hierarchy and calling model_animation_choose_random.
 *
 * When state == 0: clears unk_596 (animation state byte at 0x254) and sets
 * unk_602 (0x25a) to 0xffff (NONE), then returns.
 *
 * For state 1-9: walks the unit tag -> antr tag -> mode element -> sub-element
 * -> weapon-element hierarchy using unk_592/593/594 as indices. Then:
 *
 *   States 1-4, 8: index the animation kind table in the sub-element block
 *     (sub_element+0x98/0x9c). Kind indices: 0x15, 0x16, 0x17, 0x18, 0x14.
 *   States 5-7, 9: index the animation kind table in the weapon-element block
 *     (weapon_element+0x30/0x34). Sub-indices: 0, 1, 8, 9.
 *
 * If a valid animation index is found (DI != -1):
 *   - Calls object_set_region_count(object_handle, 6) unless state == 7.
 *   - Calls model_animation_choose_random(1, antr_tag_index, animation_index).
 *   - Stores result in unk_602 (0x25a), clears unk_604 (0x25c), sets
 *     unk_596 (0x254) to (uint8_t)state.
 *
 * Confirmed: switch jump table at 0x1a8af8 (9 entries for ECX=0-8).
 * Confirmed: MOVSX ECX,byte ptr [ESI+0x250/0x251/0x252] for tag block indices.
 * Confirmed: MOV byte ptr [ESI+0x254],CL; MOV word ptr [ESI+0x25a],AX.
 */
void unit_animation_start_action(int object_handle, int16_t state)
{
  unit_data_t *unit;
  char *unit_tag;
  char *antr_tag;
  char *mode_element;
  char *sub_element;
  char *weapon_element;
  int16_t animation_index;
  int anim_kind_idx;
  int anim_sub_idx;

  unit = (unit_data_t *)object_get_and_verify_type(object_handle, 3);

  if (state == 0) {
    unit->unk_596 = 0;
    unit->unk_602 = (uint16_t)0xffff;
    return;
  }

  unit_tag = (char *)tag_get(0x756e6974, unit->object.tag_index);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  mode_element = (char *)tag_block_get_element(
    antr_tag + 0xc, (int)(int8_t)unit->unk_592, 0x64);
  sub_element = (char *)tag_block_get_element(mode_element + 0x58,
                                              (int)(int8_t)unit->unk_593, 0xbc);
  weapon_element = (char *)tag_block_get_element(
    sub_element + 0xb0, (int)(int8_t)unit->unk_594, 0x3c);

  animation_index = (int16_t)-1;

  switch (state) {
  case 1:
    anim_kind_idx = 0x15;
    goto lookup_sub;
  case 2:
    anim_kind_idx = 0x16;
    goto lookup_sub;
  case 3:
    anim_kind_idx = 0x17;
    goto lookup_sub;
  case 4:
    anim_kind_idx = 0x18;
    goto lookup_sub;
  case 8:
    anim_kind_idx = 0x14;
    goto lookup_sub;
  lookup_sub:
    if (anim_kind_idx < *(int *)(sub_element + 0x98))
      animation_index =
        *(int16_t *)(*(int *)(sub_element + 0x9c) + anim_kind_idx * 2);
    break;

  case 5:
    anim_sub_idx = 0;
    goto lookup_weapon;
  case 6:
    anim_sub_idx = 1;
    goto lookup_weapon;
  case 7:
    anim_sub_idx = 8;
    goto lookup_weapon;
  case 9:
    anim_sub_idx = 9;
    goto lookup_weapon;
  lookup_weapon:
    if (anim_sub_idx < *(int *)(weapon_element + 0x30))
      animation_index =
        *(int16_t *)(*(int *)(weapon_element + 0x34) + anim_sub_idx * 2);
    break;
  }

  if (animation_index != (int16_t)-1) {
    if (state != 7)
      object_set_region_count(object_handle, 6);
    unit->unk_602 = (int16_t)model_animation_choose_random(
      1, *(int *)(unit_tag + 0x44), animation_index);
    unit->unk_604 = 0;
    unit->unk_596 = (uint8_t)state;
  }
}

/* FUN_001a8b20 (0x1a8b20)
 *
 * Attempts to set the current weapon animation state on a unit by looking up
 * the appropriate animation index from the unit's animation tag data and
 * calling model_animation_choose_random to select the sequence.
 *
 * Resolves the animation graph via the unit tag (group 'unit') and its nested
 * animation data (group 'antr'). Navigates through the weapon's animation mode
 * and weapon-type blocks using per-unit indices (unk_592, unk_593, unk_594).
 *
 * The incoming state code is remapped to an animation table index:
 *   1 -> 4 (fire-1), 2 -> 5 (fire-2), 3 -> 6 (charged-1), 4 -> 7 (charged-2),
 *   5 -> 2 (chamber-1), 6 -> 3 (chamber-2)
 *
 * Early-outs:
 *   - state < unit->unk_597 (already at or past this state)
 *   - unk_595 is in the set of "active" animation states (0x17–0x23, 0x27,
 * 0x29)
 *   - table index out of range or entry == -1 (animation not defined)
 *
 * On success, writes the chosen random animation index into unk_606 (0x25e),
 * clears unk_608 (0x260), and sets unk_597 (0x255) = state.
 *
 * On failure, if developer mode is enabled (DAT_005054fb != 0) and this is a
 * biped (object.type == 0) with no seat (unk_586 == -1), prints a warning:
 *   MISSING: <tag_path> '<state_name> <mode_name>'
 */
void FUN_001a8b20(int object_handle, int16_t state)
{
  unit_data_t *unit;
  int16_t current_state;
  char *unit_tag;
  char *antr_tag;
  void *mode_block;
  void *type_block;
  void *dest_block;
  int anim_table_index;
  int16_t anim_index;
  int16_t chosen;

  unit = (unit_data_t *)object_get_and_verify_type(object_handle, 3);

  /* bail if already at or past this state */
  current_state = (int16_t)(int8_t)unit->unk_597;
  if (state < current_state) {
    return;
  }

  /* bail for specific animation states that are already active */
  switch ((uint8_t)unit->unk_595) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    return;
  default:
    break;
  }

  /* navigate to the animation destination block for this unit's weapon type */
  unit_tag = tag_get(0x756e6974, *(int *)unit);
  antr_tag = tag_get(0x616e7472, *(int *)((char *)unit_tag + 0x44));
  mode_block = tag_block_get_element((char *)antr_tag + 0xc,
                                     (int)(int8_t)unit->unk_592, 0x64);
  type_block = tag_block_get_element((char *)mode_block + 0x58,
                                     (int)(int8_t)unit->unk_593, 0xbc);
  dest_block = tag_block_get_element((char *)type_block + 0xb0,
                                     (int)(int8_t)unit->unk_594, 0x3c);

  /* remap state code to animation table index */
  anim_table_index = -1;
  switch (state) {
  case 1:
    anim_table_index = 4;
    break;
  case 2:
    anim_table_index = 5;
    break;
  case 3:
    anim_table_index = 6;
    break;
  case 4:
    anim_table_index = 7;
    break;
  case 5:
    anim_table_index = 2;
    break;
  case 6:
    anim_table_index = 3;
    break;
  default:
    goto missing;
  }

  /* look up animation index in the destination block's array */
  if (anim_table_index < *(int *)((char *)dest_block + 0x30)) {
    anim_index =
      *(int16_t *)(*(int *)((char *)dest_block + 0x34) + anim_table_index * 2);
    if (anim_index != -1) {
      chosen = (int16_t)model_animation_choose_random(
        1, *(int *)((char *)unit_tag + 0x44), anim_index);
      unit->unk_606 = chosen;
      unit->unk_608 = 0;
      unit->unk_597 = (uint8_t)state;
      return;
    }
  }

missing:
  /* developer-mode warning: animation not defined */
  if (*(uint8_t *)0x5054fb != 0 && unit->object.type == 0 &&
      (int16_t)unit->unk_586 == -1) {
    const char *state_name = FUN_001205f0((void *)0x322148, anim_table_index);
    const char *tag_path =
      tag_name_strip_path(*(char **)((char *)unit_tag + 0x3c));
    console_warning("MISSING: %s '%s %s'", tag_path, state_name, type_block);
  }
}

/* unit_find_nearby_seat (0x1a8ce0)
 *
 * Searches the child object chain of target_unit for a unit that occupies the
 * given seat_index, or that belongs to a friendly team (via game_allegiance).
 *
 * Walks the linked list starting at target_unit's first_child_object (offset
 * 0xC8). For each child that is a biped or vehicle (type 0 or 1):
 *   - If the child unit's seat tag index (offset 0x2A0) matches seat_index,
 *     records the child's handle in *out_unit_handle.
 *   - Otherwise, if the searching unit has an active rider/owner (offset 0x1C8
 *     != NONE) and the child's team is friendly with the searching unit's team,
 *     clears the "empty" flag but does not update the output handle.
 *
 * Returns true only if unit_handle != target_unit_handle AND no matching/
 * friendly child was found. If out_unit_handle is non-NULL, writes the handle
 * of the last seat-matching child found (or NONE if none matched).
 */
bool unit_find_nearby_seat(int unit_handle, int target_unit_handle,
                           int16_t seat_index, int *out_unit_handle)
{
  unit_data_t *unit_data;
  object_data_t *target_obj;
  object_data_t *child_obj;
  unit_data_t *child_unit;
  int child_handle;
  int result_handle;
  bool not_found;

  unit_data = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  target_obj =
    (object_data_t *)object_get_and_verify_type(target_unit_handle, 3);

  not_found = (unit_handle != target_unit_handle);
  result_handle = -1;

  child_handle = target_obj->unk_200.value;
  while (child_handle != -1) {
    child_obj = (object_data_t *)object_get_and_verify_type(child_handle, -1);

    /* check if child is biped (type 0) or vehicle (type 1) */
    if ((1 << (*(uint8_t *)&child_obj->type & 0x1f)) & 3) {
      child_unit = (unit_data_t *)object_get_and_verify_type(child_handle, 3);

      if (child_unit->unk_672 == seat_index) {
        /* child occupies the requested seat */
        result_handle = child_handle;
        not_found = false;
      } else if (unit_data->unk_456.value != -1 &&
                 game_allegiance_get_team_is_friendly(
                   (int16_t)unit_data->object.unk_104,
                   (uint16_t)child_obj->unk_104)) {
        /* child is on a friendly team; seat is occupied/blocked */
        not_found = false;
      }
    }

    child_handle = child_obj->next_object_index.value;
  }

  if (out_unit_handle != NULL) {
    *out_unit_handle = result_handle;
  }
  return not_found;
}

/* unit_handle_weapon_state_change (0x1a8e10)
 *
 * Dispatches a unit animation state transition based on an incoming state code.
 * Maps input state values 1-8 to either FUN_001a8b20 (which sets a unit
 * animation transition state with a remapped index) or
 * unit_animation_start_action (which initiates a seat-based animation
 * sequence). The state remapping is: state 1 -> FUN_001a8b20 with index 1 state
 * 2 -> FUN_001a8b20 with index 2 state 3 -> FUN_001a8b20 with index 5 state 4
 * -> FUN_001a8b20 with index 6 state 5 -> unit_animation_start_action with
 * index 5 state 6 -> unit_animation_start_action with index 6 state 7 ->
 * FUN_001a8b20 with index 3 state 8 -> FUN_001a8b20 with index 4
 */
void unit_handle_weapon_state_change(int object_handle, int16_t state)
{
  switch (state) {
  case 1:
    FUN_001a8b20(object_handle, 1);
    return;
  case 2:
    FUN_001a8b20(object_handle, 2);
    return;
  case 3:
    FUN_001a8b20(object_handle, 5);
    return;
  case 4:
    FUN_001a8b20(object_handle, 6);
    return;
  case 5:
    unit_animation_start_action(object_handle, 5);
    return;
  case 6:
    unit_animation_start_action(object_handle, 6);
    return;
  case 7:
    FUN_001a8b20(object_handle, 3);
    return;
  case 8:
    FUN_001a8b20(object_handle, 4);
    return;
  }
}

/* unit_record_damage (0x1a8ee0)
 *
 * Records damage in the unit's 4-slot damage tracking array at +0x3E0.
 * Each slot is 16 bytes: [timestamp, damage_amount, killing_object, attacker_object].
 *
 * First scans existing slots for a matching attacker or killing object handle;
 * if found, accumulates damage and updates the timestamp. Otherwise, finds an
 * empty slot (timestamp == -1) or evicts the oldest slot with the smallest
 * accumulated damage.
 *
 * If notify_ai is true and attacker_team != NONE, checks team allegiance
 * and notifies the AI system about killing sprees. Resolves the actual
 * attacker unit by following the player->unit chain and seat hierarchy.
 *
 * Confirmed: SUB ESP,0x8; 4 callee-saved regs; 7 stack params.
 * Confirmed: LEA EAX,[EDI+0x3E4] — damage array starts at +0x3E4 (per-slot +0x4).
 * Confirmed: IMUL/FCOMP loop with 4 iterations.
 * Confirmed: assert "best_new_attacker_index!=NONE" at line 0x136C, file units.c.
 * Confirmed: FUN_000b5aa0 = game_time_get, FUN_000a7a30 = game_allegiance check.
 * Confirmed: FUN_0003ff40 = ai_handle_killing_spree.
 */
void unit_record_damage(int unit_handle, float damage_amount, int16_t damage_type,
                        char notify_ai, int attacker_object, int16_t attacker_team,
                        int killing_object)
{
  char *unit;
  char found_existing;
  int timestamp;
  float *slot_damage;
  int i;
  int16_t empty_slot;
  int16_t best_damage_slot;
  int16_t best_oldest_slot;
  int16_t s;
  char *attacker_unit;
  int attacker_unit_handle;
  int seated_handle;
  int killing_spree_count;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  found_existing = 0;
  timestamp = game_time_get();

  /* Scan existing 4 slots for matching attacker or killing object */
  slot_damage = (float *)(unit + 0x3e4);
  i = 4;
  do {
    if ((attacker_object != -1 && *(int *)(((char *)slot_damage) + 8) == attacker_object) ||
        *(int *)(((char *)slot_damage) + 4) == killing_object) {
      *(int *)(((char *)slot_damage) - 4) = timestamp;
      found_existing = 1;
      *slot_damage = damage_amount + *slot_damage;
    }
    slot_damage = (float *)((char *)slot_damage + 0x10);
    i = i - 1;
  } while (i != 0);

  if (!found_existing) {
    /* Find an empty slot (timestamp == -1) */
    empty_slot = 0;
    do {
      if (*(int *)(unit + ((int)empty_slot + 0x3e) * 0x10) == -1) {
        goto write_slot;
      }
      empty_slot = empty_slot + 1;
    } while (empty_slot < 4);

    /* No empty slot — find the slot with the smallest damage (to evict) */
    best_damage_slot = 0;
    s = 1;
    do {
      if (*(float *)(unit + 0x3e4 + (int)best_damage_slot * 0x10) <
          *(float *)(unit + 0x3f4 + ((int)s - 1) * 0x10)) {
        best_damage_slot = s;
      }
      s = s + 1;
    } while (s < 4);

    /* Among remaining slots (excluding best_damage_slot), find the oldest */
    best_oldest_slot = -1;
    s = 0;
    do {
      if (s != best_damage_slot) {
        if (best_oldest_slot == -1 ||
            *(uint32_t *)(unit + 0x3e0 + (int)s * 0x10) <
                *(uint32_t *)(unit + ((int)best_oldest_slot + 0x3e) * 0x10)) {
          best_oldest_slot = s;
        }
      }
      s = s + 1;
    } while (s < 4);

    if (best_oldest_slot == -1) {
      display_assert("best_new_attacker_index!=NONE",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x136c, 1);
      system_exit(-1);
    }

    empty_slot = best_oldest_slot;

  write_slot:
    {
      int slot_base;
      slot_base = (int)empty_slot * 0x10;
      *(int *)(unit + slot_base + 0x3ec) = attacker_object;
      *(int *)(unit + slot_base + 0x3e8) = killing_object;
      /* Store damage as raw float bits via int assignment matches original MOV */
      *(float *)(unit + slot_base + 0x3e4) = damage_amount;
      *(int *)(unit + ((int)empty_slot + 0x3e) * 0x10) = timestamp;
    }
  }

  /* AI notification section */
  if (notify_ai == '\0') {
    return;
  }
  if ((int16_t)attacker_team == -1) {
    return;
  }
  if (!game_allegiance_get_team_is_friendly(
          *(int16_t *)(unit + 0x68), attacker_team)) {
    return;
  }

  /* Resolve the actual attacking unit */
  attacker_unit = NULL;
  attacker_unit_handle = killing_object;
  if (attacker_object != -1) {
    {
      char *player_entry;
      player_entry = (char *)datum_get(*(void **)0x5aa6d4, attacker_object);
      seated_handle = *(int *)(player_entry + 0x34);
      if (seated_handle != -1) {
        attacker_unit = (char *)object_get_and_verify_type(seated_handle, 3);
        attacker_unit_handle = seated_handle;
        if (attacker_unit != NULL) {
          goto have_attacker;
        }
      }
    }
  }
  attacker_unit = (char *)object_try_and_get_and_verify_type(killing_object, 3);
  attacker_unit_handle = killing_object;
  if (attacker_unit == NULL) {
    return;
  }

have_attacker:
  /* Follow seat hierarchy (driver/gunner) to the controlling unit */
  if (damage_type == 9) {
    seated_handle = *(int *)(attacker_unit + 0x2d4);
  } else {
    seated_handle = *(int *)(attacker_unit + 0x2d8);
  }
  if (seated_handle != -1) {
    attacker_unit = (char *)object_get_and_verify_type(seated_handle, 3);
    attacker_unit_handle = seated_handle;
  }

  /* Skip AI-controlled units */
  if ((*(uint8_t *)(attacker_unit + 0xb6) & 4) != 0) {
    return;
  }

  /* Update killing spree counter */
  {
    int last_damage_time;
    last_damage_time = *(int *)(attacker_unit + 0x3dc);
    if (last_damage_time == -1 || last_damage_time + 0x78 < timestamp) {
      *(int16_t *)(attacker_unit + 0x3da) = 0;
    }
    *(int16_t *)(attacker_unit + 0x3da) =
        *(int16_t *)(attacker_unit + 0x3da) + 1;
    killing_spree_count = (int)(uint16_t) *(int16_t *)(attacker_unit + 0x3da);
    *(int *)(attacker_unit + 0x3dc) = timestamp;

    if (ai_handle_killing_spree(attacker_unit_handle,
                                (short)killing_spree_count) != '\0') {
      *(int16_t *)(attacker_unit + 0x3da) = 0;
    }
  }
}

/* 0x1a9200 — get world-space position of the "head" marker on a unit.
 * Thin wrapper: calls object_get_markers_by_string_id for the string at
 * 0x2909e4 ("head"), then extracts XYZ from offset 0x60 in the marker
 * output record. Identical pattern to FUN_001a9520 ("body" marker). */
void unit_get_head_position(int object_handle, float *out_position)
{
  char marker_buf[0x6c];
  object_get_markers_by_string_id(object_handle, (void *)0x2909e4, marker_buf,
                                  1);
  out_position[0] = *(float *)(marker_buf + 0x60);
  out_position[1] = *(float *)(marker_buf + 0x64);
  out_position[2] = *(float *)(marker_buf + 0x68);
}

/* unit_set_seat_state (0x1a9240)
 *
 * Computes a 3D position representing the unit's current seat state and writes
 * it into the caller-supplied float[3].
 *
 * Three major paths:
 *
 * 1. Unit has a parent (parent_object_index != -1):
 *    Copies the parent object's world position (offset 0x0C). If the parent's
 *    object type is 0 or 1 (biped/vehicle) and the unit has a valid seat
 *    definition index (unk_672 != -1), looks up the seat's marker name (at
 *    seat_def + 0x84) and resolves it on the parent via
 *    object_get_markers_by_string_id. For seat type 1, skips if the marker
 *    name byte at seat_def + 0x84 is zero.
 *
 * 2. Unit has no parent and no special flags:
 *    If unit flags byte (0xB6) bit 2 is clear AND object type is 0 (biped),
 *    delegates to FUN_001a1140 with zeroed optional parameters.
 *
 * 3. Unit has no parent but has flags/non-zero type:
 *    If unk_728 is NONE, gets the "head" marker on the unit itself. Otherwise,
 *    resolves unk_728 as a unit, reads its seat index (unk_672), looks up the
 *    seat definition's marker name (seat_def + 0x24) from the original unit's
 *    tag, and resolves it on the original unit via
 * object_get_markers_by_string_id.
 */
void unit_set_seat_state(int unit_handle, float *position)
{
  char *unit;
  char *unit_tag;
  int seat_index;
  char *seat_object;
  char *parent_unit;
  char *seat_def;
  int16_t seat_def_index;
  uint8_t seat_type;
  uint32_t type_mask;
  char marker_buf[0x6c];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  seat_index = *(int *)(unit + 0xcc);

  if (seat_index == -1) {
    /* Unit is not in a seat */
    if (!(*(uint8_t *)(unit + 0xb6) & 0x04) && *(int16_t *)(unit + 0x64) == 0) {
      /* Simple biped with no special flags — delegate to
       * biped_estimate_position */
      biped_estimate_position(unit_handle, 0, (vector3_t *)0,
                              (vector3_t *)0, /* dup-args-ok */
                              (vector3_t *)0, (vector3_t *)position);
      return;
    }

    /* Has flags or non-zero type: check unk_728 */
    if (*(int *)(unit + 0x2d8) == -1) {
      /* No related unit — find "head" marker on this unit */
      object_get_markers_by_string_id(unit_handle, (void *)0x2909e4, marker_buf,
                                      1);
      position[0] = *(float *)(marker_buf + 0x60);
      position[1] = *(float *)(marker_buf + 0x64);
      position[2] = *(float *)(marker_buf + 0x68);
      return;
    }

    /* Related unit exists — get its seat definition */
    parent_unit = (char *)object_get_and_verify_type(*(int *)(unit + 0x2d8), 3);
    seat_def_index = *(int16_t *)(parent_unit + 0x2a0);
    seat_def = (char *)tag_block_get_element(unit_tag + 0x2e4,
                                             (int)seat_def_index, 0x11c);
    object_get_markers_by_string_id(unit_handle, seat_def + 0x24, marker_buf,
                                    1);
    position[0] = *(float *)(marker_buf + 0x60);
    position[1] = *(float *)(marker_buf + 0x64);
    position[2] = *(float *)(marker_buf + 0x68);
    return;
  }

  /* Unit IS in a seat — seat_index is the parent object handle */
  seat_object = (char *)object_get_and_verify_type(seat_index, -1);

  /* Copy seat object's world position */
  position[0] = *(float *)(seat_object + 0x0c);
  position[1] = *(float *)(seat_object + 0x10);
  position[2] = *(float *)(seat_object + 0x14);

  /* Check if seat type is biped (0) or vehicle (1) */
  seat_type = *(uint8_t *)(seat_object + 0x64);
  type_mask = 1 << seat_type;
  if (!(type_mask & 0x03))
    return;

  /* Seat type is 0 or 1 — refine position from seat marker */
  if (*(int16_t *)(unit + 0x2a0) == -1)
    return;

  /* Get the seat definition from the parent's unit tag */
  unit_tag = (char *)tag_get(0x756e6974, *(int *)seat_object);
  seat_def_index = *(int16_t *)(unit + 0x2a0);
  seat_def =
    (char *)tag_block_get_element(unit_tag + 0x2e4, (int)seat_def_index, 0x11c);

  /* For seat type 1 (vehicle), skip if marker name at +0x84 is empty */
  if (*(int16_t *)(seat_object + 0x64) == 1) {
    if (*(uint8_t *)(seat_def + 0x84) == 0)
      return;
  }

  /* Look up the seat marker on the parent object */
  object_get_markers_by_string_id(*(int *)(unit + 0xcc), seat_def + 0x84,
                                  marker_buf, 1);
  position[0] = *(float *)(marker_buf + 0x60);
  position[1] = *(float *)(marker_buf + 0x64);
  position[2] = *(float *)(marker_buf + 0x68);
}

/* unit_estimate_position (0x1a93e0)
 *
 * Estimates the world-space position for a unit given a desired estimation mode
 * and optional predicted body position. Handles three major paths:
 *
 * 1. Unit has no parent AND flag byte (unit+0xb6) bit 2 is clear AND type==0
 *    (biped): delegate entirely to biped_estimate_position and return.
 *
 * 2. Unit type==0 (biped) AND has a parent AND parent type==1 (vehicle): call
 *    vehicle_get_estimated_position to get the vehicle's predicted position
 * into a local vector, and use that as the local_body_pos for the delta. If
 * vehicle_get_estimated_position returns -1 (failed), fall through to path 3.
 *
 * 3. Fallback: call object_get_world_position to get unit's world position as
 *    local_body_pos.
 *
 * After path 2/3: call unit_set_seat_state to compute the unit's current
 * estimated position into out_position, then add the delta
 * (body_position - local_body_pos) to out_position.
 *
 * Confirmed: assert strings "body_position && estimated_position" and
 * "(estimate_mode >= 0) && (estimate_mode <
 * NUMBER_OF_UNIT_ESTIMATE_POSITION_MODES)" reference
 * "c:\halo\SOURCE\units\units.c", lines 0x14b1 and 0x14b2. Confirmed: CMP
 * EAX,-0x1 / JNZ pattern for parent_object_index checks. Confirmed: TEST byte
 * ptr [EDI+0xb6],0x4 for flags check. Confirmed: CMP word ptr [EDI+0x64],0x0
 * and CMP word ptr [EAX+0x64],0x1 for type checks. Confirmed: FPU tail sequence
 * — FXCH after three FLD/FSUB pairs to reorder x/y.
 */
void unit_estimate_position(int unit_handle, int16_t estimate_mode,
                            vector3_t *body_position, vector3_t *desired_facing,
                            vector3_t *desired_gun_offset,
                            vector3_t *out_position)
{
  char *unit;
  char *parent_obj;
  int parent_handle;
  int result;
  vector3_t local_body_pos;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (body_position == NULL || out_position == NULL) {
    display_assert("body_position && estimated_position",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14b1, true);
    system_exit(-1);
  }
  if (estimate_mode < 0 || estimate_mode >= 4) {
    display_assert("(estimate_mode >= 0) && (estimate_mode < "
                   "NUMBER_OF_UNIT_ESTIMATE_POSITION_MODES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14b2, true);
    system_exit(-1);
  }

  parent_handle = *(int *)(unit + 0xcc);

  if (parent_handle == -1 && !(*(uint8_t *)(unit + 0xb6) & 0x4)) {
    /* No parent, no flag: if biped, fully delegate to biped_estimate_position
     */
    if (*(int16_t *)(unit + 0x64) == 0) {
      biped_estimate_position(unit_handle, estimate_mode, body_position,
                              desired_facing, desired_gun_offset, out_position);
      return;
    }
  } else if (*(int16_t *)(unit + 0x64) == 0 && parent_handle != -1) {
    /* Biped with a parent: if parent is a vehicle, try vehicle position */
    parent_obj = (char *)object_get_and_verify_type(parent_handle, -1);
    if (*(int16_t *)(parent_obj + 0x64) == 1) {
      result =
        vehicle_get_estimated_position(*(int *)(unit + 0xcc), &local_body_pos);
      if (result != -1)
        goto apply_delta;
    }
  }

  /* Fallback: use world position as local_body_pos reference */
  object_get_world_position(unit_handle, &local_body_pos);

apply_delta:
  /* Get the unit's current estimated seat position into out_position */
  unit_set_seat_state(unit_handle, (float *)out_position);

  /* Add (body_position - local_body_pos) delta to out_position */
  out_position->x += body_position->x - local_body_pos.x;
  out_position->y += body_position->y - local_body_pos.y;
  out_position->z += body_position->z - local_body_pos.z;
}

/* unit_impulse_to_animation_kind (0x1a9560)
 *
 * Maps an animation impulse index (0-13) to an animation kind index and an
 * update_kind value. The impulse index selects an animation kind (e.g. 0x1d
 * for "impulse", 0x20 for "melee_attack_long_step", etc.) and writes 3 or 6
 * to *out_update_kind depending on whether the impulse uses a ranged or melee
 * update mode.
 *
 * If impulse_index is out of [0, 13] the function asserts and terminates.
 * If out_update_kind is NULL the write is skipped (TEST EBX,EBX gate).
 *
 * Impulse-to-kind mapping (jump table at 0x1a969c):
 *   0  -> 0x1d  (update 6)   8  -> 0x04  (update 3)
 *   1  -> 0x20  (update 6)   9  -> 0x05  (update 3)
 *   2  -> 0x21  (update 6)   10 -> 0x06  (update 3)
 *   3  -> 0x22  (update 6)   11 -> 0x07  (update 3)
 *   4  -> 0x1b  (update 3)   12 -> 0x28  (update 6)
 *   5  -> 0x1c  (update 3)   13 -> 0x29  (update 6)
 *   6  -> 0x1e  (update 6)
 *   7  -> 0x1f  (update 6)
 *
 * Register args: impulse_index @<ax>, out_update_kind @<ebx>.
 * Returns kind index in AX (int16_t).
 *
 * Confirmed: MOV DI,AX at entry; switch dispatch from 0x1a969c.
 * Confirmed: MOV word ptr [EBX],0x3 or 0x6; MOV AX,SI; RET.
 * Confirmed: assert "animation_impulse>=0 &&
 * animation_impulse<NUMBER_OF_UNIT_ANIMATION_IMPULSES" file
 * "c:\\halo\\SOURCE\\units\\units.c", line 0x14f4.
 */
int16_t unit_impulse_to_animation_kind(int16_t impulse_index,
                                       int16_t *out_update_kind)
{
  static const int16_t kind_table[14] = { 0x1d, 0x20, 0x21, 0x22, 0x1b,
                                          0x1c, 0x1e, 0x1f, 0x04, 0x05,
                                          0x06, 0x07, 0x28, 0x29 };
  /* update 3 for impulses 4,5,8,9,10,11; update 6 for all others. */
  static const int16_t update_table[14] = { 6, 6, 6, 6, 3, 3, 6,
                                            6, 3, 3, 3, 3, 6, 6 };
  int16_t kind;

  if (impulse_index < 0 || impulse_index >= 14) {
    display_assert("animation_impulse>=0 && "
                   "animation_impulse<NUMBER_OF_UNIT_ANIMATION_IMPULSES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x14f4, 1);
    system_exit(-1);
    return -1;
  }

  kind = kind_table[impulse_index];

  if (out_update_kind != NULL)
    *out_update_kind = update_table[impulse_index];

  return kind;
}

/* unit_animation_state_allows_impulse (0x1a96f0)
 *
 * Returns true if the unit's current animation state (unk_595 at +0x253)
 * is one that permits an animation impulse to be applied.
 *
 * States in [0x17..0x29] that block impulses (switch table 0x1a97a8):
 *   0x17-0x1b, 0x1d-0x1f, 0x20-0x23, 0x27, 0x29 -> return false.
 * States outside [0x17..0x29] -> fall to parent check.
 *
 * If parent_object_index != -1 (unit is seated/mounted):
 *   - If unk_672 (seat-anim index) is -1 -> return false.
 *   - Resolves parent via object_try_and_get_and_verify_type (0x13d640).
 *   - Looks up the seat-anim entry at parent_unit_tag+0x2e4, indexed by
 *     unk_672, element size 0x11c.
 *   - If impulse_index is in [0xc, 0xd]: returns bit 8 of seat_anim[0].
 *   - If impulse_index is outside [0xc, 0xd]: returns false.
 *
 * If parent_object_index == -1 (no parent):
 *   - If impulse_index NOT in [0xc, 0xd] -> return true.
 *   - If impulse_index in [0xc, 0xd] -> return false.
 *
 * Register args: unit_handle @<eax>, impulse_index @<edi> (leaked from caller).
 * Returns bool in AL.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: MOVSX EAX,byte[ESI+0x253]; ADD -0x17; CMP 0x12; jump table
 * 0x1a97a8. Confirmed: MOVSX ECX,DI at 0x1a976c and 0x1a9786 (DI = caller's EDI
 * = anim_index). Confirmed: CMP [ESI+0x2a0],-1 -> unk_672 check. Confirmed:
 * object_try_and_get_and_verify_type at 0x13d640 / tag_get /
 * tag_block_get_element. Confirmed: MOV EAX,[EAX]; SHR EAX,8; AND AL,1 -> bit 8
 * gate.
 */
bool unit_animation_state_allows_impulse(int unit_handle, int impulse_index)
{
  unit_data_t *unit;
  int state;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  state = (int)(int8_t)unit->unk_595;

  /* Switch on state - 0x17 for values in [0x17, 0x29] */
  if ((unsigned)(state - 0x17) <= 0x12) {
    switch (state) {
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1d:
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x27:
    case 0x29:
      return false;
    default:
      break;
    }
  }

  /* Check parent (mounted) case */
  if (unit->object.parent_object_index.value != -1) {
    unit_data_t *parent;
    char *parent_tag;
    int *seat_anim;

    if ((int16_t)unit->unk_672 == -1)
      return false;

    parent = (unit_data_t *)object_try_and_get_and_verify_type(
      (int)unit->object.parent_object_index.value, 3);
    if (parent == NULL)
      return false;

    parent_tag = (char *)tag_get(0x756e6974, *(int *)parent);
    seat_anim = (int *)tag_block_get_element(
      parent_tag + 0x2e4, (int)(int16_t)unit->unk_672, 0x11c);

    /* Only impulses 0xc and 0xd can fire when mounted */
    if ((int16_t)impulse_index < 0xc || (int16_t)impulse_index > 0xd)
      return false;

    return (bool)((seat_anim[0] >> 8) & 1);
  }

  /* No parent: impulses 0xc and 0xd are blocked */
  if ((int16_t)impulse_index >= 0xc && (int16_t)impulse_index <= 0xd)
    return false;

  return true;
}

/* unit_test_animation_impulse (0x1a97c0)
 *
 * Tests whether a specific animation impulse can be applied to a unit.
 * First checks if the current animation state allows the impulse, then
 * looks up the unit's animation tag block to verify the impulse has a
 * valid animation entry.
 *
 * Returns true (AL=1) if the animation impulse is available and mapped
 * to a non-NONE animation index; false otherwise.
 *
 * Tag lookup chain:
 *   unit tag (0x756e6974) -> animations tag ref at +0x44 ->
 *   animation tag (0x616e7472) -> block at +0xC, elem size 100 ->
 *   sub-block at result+0x58, elem size 0xBC ->
 *   count at +0x98, data pointer at +0x9C, indexed by animation kind.
 *
 * Confirmed: PUSH [EBP+0x8] / PUSH 0x3 -> cdecl, 2 stack params.
 * Confirmed: MOV EAX,EBX -> @eax for unit_animation_state_allows_impulse.
 * Confirmed: LEA EBX,[EBP-0x4] / MOV EAX,EDI -> @ebx, @ax for
 *            unit_impulse_to_animation_kind.
 * Confirmed: MOVSX EDX,byte[ESI+0x250] and byte[ESI+0x251] for anim indices.
 * Confirmed: CMP AX,0xffff / SETNZ AL for return logic.
 */
uint32_t unit_test_animation_impulse(int unit_handle, int impulse_index)
{
  char *unit;
  char *anim_tag;
  char *anim_block;
  int16_t anim_kind;
  int16_t update_kind;
  int16_t *data;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (!unit_animation_state_allows_impulse(unit_handle, impulse_index)) {
    return 0;
  }

  /* Look up unit tag -> animations tag -> animation block elements */
  {
    char *unit_tag;
    int anim_tag_index;

    unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
    anim_tag_index = *(int *)(unit_tag + 0x44);
    anim_tag = (char *)tag_get(0x616e7472, anim_tag_index);
  }

  anim_block = (char *)tag_block_get_element(
      anim_tag + 0xc, (int)*(int8_t *)(unit + 0x250), 100);
  anim_block = (char *)tag_block_get_element(
      anim_block + 0x58, (int)*(int8_t *)(unit + 0x251), 0xbc);

  /* Map impulse to animation kind */
  anim_kind = unit_impulse_to_animation_kind((int16_t)impulse_index,
                                             &update_kind);

  if (anim_kind < 0 || (int)anim_kind >= *(int *)(anim_block + 0x98)) {
    /* Out of range — return false (matches OR EAX,-1; CMP AX,0xffff; SETNZ) */
    return (uint32_t)(int16_t)-1 != (int16_t)-1;
  }

  data = *(int16_t **)(anim_block + 0x9c);
  return (uint32_t)(data[anim_kind] != (int16_t)-1);
}

/* unit_scripting_unit_driver (0x1a9900)
 *
 * Copies the unit's aiming vector (unk_492, offset 0x1EC) into the output
 * buffer. Resolves the unit via object_get_and_verify_type with type mask
 * 0x3 (biped | vehicle).
 */
void unit_scripting_unit_driver(int unit_handle, void *out_aiming)
{
  char *unit;
  float *out = (float *)out_aiming;

  unit = object_get_and_verify_type(unit_handle, 3);
  out[0] = *(float *)(unit + 0x1ec);
  out[1] = *(float *)(unit + 0x1f0);
  out[2] = *(float *)(unit + 0x1f4);
}

/* unit_scripting_unit_gunner (0x1a9930)
 *
 * Copies the unit's looking vector (unk_528, offset 0x210) into the output
 * buffer. Resolves the unit via object_get_and_verify_type with type mask
 * 0x3 (biped | vehicle).
 */
void unit_scripting_unit_gunner(int unit_handle, void *out_looking)
{
  char *unit;
  float *out = (float *)out_looking;

  unit = object_get_and_verify_type(unit_handle, 3);
  out[0] = *(float *)(unit + 0x210);
  out[1] = *(float *)(unit + 0x214);
  out[2] = *(float *)(unit + 0x218);
}

/* units_debug_get_closest_unit (0x1a9960)
 *
 * Gets the unit's facing vector by delegating to object_get_orientation (an
 * object-level orientation getter in objects.c). Passes NULL for the up-vector
 * output, requesting only the forward direction.
 */
void units_debug_get_closest_unit(int unit_handle, void *out_facing)
{
  object_get_orientation(unit_handle, (float *)out_facing, 0);
}

/* unit_is_alive (0x1a9a30)
 *
 * Returns whether the given unit handle refers to a unit that is currently
 * alive. Resolves the handle via object_get_and_verify_type with type mask
 * 0x3 (bit 0 = biped, bit 1 = vehicle — accepts any unit object; asserts
 * otherwise) and tests bit 6 of the unit's flag word at offset 0x1B4
 * (unit_data_t.unk_436).
 */
bool unit_is_alive(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  return (unit->unk_436 >> 6) & 1;
}

/* Check if a unit is in a vehicle seat based on seat state byte at +0x253. */
bool unit_is_busy(int unit_handle)
{
  char *unit = (char *)object_get_and_verify_type(unit_handle, 3);
  int seat_state = *(signed char *)(unit + 0x253);
  switch (seat_state) {
  case 0x17:
  case 0x18:
  case 0x19:
  case 0x1a:
  case 0x1b:
  case 0x1d:
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x22:
  case 0x23:
  case 0x27:
  case 0x29:
    return true;
  default:
    return false;
  }
}

/* FUN_001AA170 (0x1aa170) — find nearest biped
 *
 * Iterates all biped objects (type_mask=1) to find the nearest biped
 * to the given unit, excluding the unit itself and any biped with
 * bit 2 of byte +0xb6 set. Uses 3D Euclidean distance (SQRT).
 * If unit_handle is -1, uses FLT_MAX (0x7f7fffff) as distance for all
 * candidates, effectively selecting the first valid biped.
 *
 * Returns the nearest biped's datum handle, or -1 if none found.
 *
 * Confirmed: cdecl, 1 stack param (unit_handle).
 * Confirmed: object_iterator_new(iter, 1, 0) — biped type only.
 * Confirmed: FLOAT_002533c0 = 0.0f (used when unit_handle == -1).
 * Confirmed: initial best_dist = FLT_MAX (0x7f7fffff).
 */
int FUN_001AA170(int unit_handle)
{
  int best_handle;
  float best_dist;
  int iter[4];
  char *obj;
  float pos_a[3];
  float pos_b[3];
  float dx, dy, dz, dist;

  best_handle = -1;
  best_dist = 3.4028235e+38f;

  object_iterator_new(iter, 1, 0);
  obj = (char *)object_iterator_next(iter);
  while (obj != NULL) {
    if (iter[2] != unit_handle &&
        (*(uint8_t *)(obj + 0xb6) & 4) == 0) {
      if (unit_handle == -1) {
        dist = *(float *)0x2533c0;
      } else {
        object_get_world_position(unit_handle, (vector3_t *)pos_a);
        object_get_world_position(iter[2], (vector3_t *)pos_b);
        dx = pos_b[0] - pos_a[0];
        dy = pos_b[1] - pos_a[1];
        dz = pos_b[2] - pos_a[2];
        dist = sqrtf(dx * dx + dy * dy + dz * dz);
      }
      if (dist < best_dist) {
        best_handle = iter[2];
        best_dist = dist;
      }
    }
    obj = (char *)object_iterator_next(iter);
  }
  return best_handle;
}

/* unit_debug_ninja_rope (0x1aa240)
 * Collision-based ninja rope debug function.
 * Casts a ray from the unit's aim position along a scaled facing direction.
 * If the ray hits geometry with a fraction above 0.95, translates the unit
 * to the hit point offset upward by 0.25.
 * Uses the collision stack depth guard to prevent reentrant collision. */
void unit_debug_ninja_rope(int unit_handle)
{
  char *unit;
  float origin[3];
  float direction[3];
  char collision_result[80];
  int16_t depth;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_set_seat_state(unit_handle, origin);

  direction[0] = *(float *)(unit + 0x1ec) * 25.0f;
  direction[1] = *(float *)(unit + 0x1f0) * 25.0f;
  direction[2] = *(float *)(unit + 0x1f4) * 25.0f;

  if (global_current_collision_user_depth >= MAXIMUM_COLLISION_USER_STACK_DEPTH) {
    display_assert(
        "global_current_collision_user_depth < "
        "MAXIMUM_COLLISION_USER_STACK_DEPTH",
        "c:\\halo\\SOURCE\\units\\units.c", 0x19f8, 1);
    system_exit(-1);
  }

  depth = global_current_collision_user_depth;
  global_current_collision_user_depth = depth + 1;
  collision_user_stack[depth] = 0x15;

  if (FUN_0014df70(0x22, origin, direction, unit_handle,
                   (int16_t *)collision_result)) {
    /* collision_result + 0x2c = fraction, compare > 0.95 */
    if (*(float *)(collision_result + 0x2c) > 0.95f) {
      /* collision_result + 0x20 = hit y-coordinate, add 0.25 */
      *(float *)(collision_result + 0x20) += 0.25f;
      /* collision_result + 0x18 = hit position */
      object_translate(unit_handle,
                       (float *)(collision_result + 0x18), 0);
    }
  }

  if (global_current_collision_user_depth < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1a01, 1);
    system_exit(-1);
  }
  global_current_collision_user_depth -= 1;
}

/* FUN_001aa360 (0x1aa360) — unit_set_user_animation
 *
 * Validates a user animation index against the range
 * [0, NUMBER_OF_UNIT_USER_ANIMATIONS). The function resolves the unit tag
 * and its animation tag (antr) but does not use them beyond validation.
 * Asserts if the index is out of range. Returns 0 (AL cleared to 0).
 *
 * Confirmed: cdecl, 3 stack params (unit_handle, param_2, index).
 * Confirmed: assert "index>=0 && index<NUMBER_OF_UNIT_USER_ANIMATIONS"
 *   at line 0x1a21 in units.c.
 * Confirmed: NUMBER_OF_UNIT_USER_ANIMATIONS == 2 (CMP AX,0x2).
 * Confirmed: returns AL=0 (XOR AL,AL at 001aa3bb).
 */
char FUN_001aa360(int unit_handle, int param_2, int16_t index)
{
  char *unit;
  int unit_tag_index;
  char *unit_tag;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag_index = *(int *)unit;
  unit_tag = (char *)tag_get(0x756e6974, unit_tag_index);
  tag_get(0x616e7472, *(int *)(unit_tag + 0x44));

  if (index < 0 || index >= 2) {
    display_assert("index>=0 && index<NUMBER_OF_UNIT_USER_ANIMATIONS",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1a21, 1);
    system_exit(-1);
  }

  return 0;
}

/* FUN_001aa430 (0x1aa430) — unit_can_see_point
 *
 * Returns true if the given point is within the unit's field of view.
 * Computes a direction vector from the unit's head marker to the target
 * point, normalizes it, then takes the dot product with the unit's
 * facing/aiming vector at +0x210. Compares against cos(param_3) where
 * param_3 is the half-angle of the vision cone.
 *
 * Returns false if unit_handle is NONE (-1).
 *
 * Confirmed: cdecl, 3 stack params (unit_handle, point, half_angle).
 * Confirmed: "head" marker name at 0x2909e4.
 * Confirmed: marker buffer 0x78 bytes, position at buffer+0x60.
 * Confirmed: FPU dot product order: z*fz + y*fy + x*fx.
 * Confirmed: FCOS + FCOMPP comparison: dot > cos(angle) → return 1.
 * Confirmed: normalize3d return (magnitude) discarded via FSTP ST0.
 */
char FUN_001aa430(int unit_handle, float *point, float half_angle)
{
  char *unit;
  char marker_buf[0x78];
  float *marker_pos;
  float dir[3];
  float dot;

  if (unit_handle == -1) {
    return 0;
  }

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  object_get_markers_by_string_id(unit_handle, (void *)0x2909e4,
                                  marker_buf, 1);

  /* dir = point - head_marker_position. The original keeps dir as a single
   * contiguous 3-float vector ([ebp-0xc/-0x8/-0x4]) and passes its base to
   * normalize3d, which reads all three components in place. dir MUST be an
   * array, not three separate float locals: clang scatters separate locals
   * across non-adjacent (and reordered) stack slots, so normalize3d(&dir_x)
   * would read uninitialized stack for components [1] and [2] -> huge garbage
   * -> normalize collapse -> assert_valid_real_normal3d crash (PoA marines). */
  marker_pos = (float *)(marker_buf + 0x60);
  dir[0] = point[0] - marker_pos[0];
  dir[1] = point[1] - marker_pos[1];
  dir[2] = point[2] - marker_pos[2];

  normalize3d(dir);

  dot = dir[0] * *(float *)(unit + 0x210) +
        dir[1] * *(float *)(unit + 0x214) +
        dir[2] * *(float *)(unit + 0x218);

  if (dot > x87_fcos(half_angle)) {
    return 1;
  }

  return 0;
}

/* any_unit_is_dangerous (0x1aa3c0)
 *
 * Iterates all unit objects (type_mask=3: bipeds + vehicles) and returns true
 * if any unit is considered "dangerous" — i.e. in a combat-relevant state
 * that should block saving or other safe-state transitions.
 *
 * A unit is dangerous when:
 *   - unk_595 (offset 0x253) == 0x21 and unk_573 (offset 0x23D) != 3
 *     (unit in some active/aggressive state, not in state 3)
 *   - unk_595 == 0x19 or 0x18, and bit 2 of unk_584 (offset 0x248) is clear
 *     (unit in alert/combat stance without a particular suppression flag)
 *
 * Called by game_all_quiet, game_safe_to_save, and players_respawn_coop.
 */
bool any_unit_is_dangerous(void)
{
  int iter_buf[4];
  unit_data_t *unit;
  uint8_t state;

  object_iterator_new(iter_buf, 3, 1);
  unit = (unit_data_t *)object_iterator_next(iter_buf);

  while (unit != NULL) {
    state = unit->unk_595;
    if (state == 0x21 && unit->unk_573 != 3)
      return true;
    if ((state == 0x19 || state == 0x18) && (unit->unk_584 & 4) == 0)
      return true;
    unit = (unit_data_t *)object_iterator_next(iter_buf);
  }

  return false;
}

/* unit_detach_from_parent (0x1aa5c0)
 *
 * Detaches a unit from its parent vehicle. Computes a direction vector
 * from parent to child position, normalizes it, and scales by 0.02f
 * (the eject velocity factor at 0x2b6af4). Detaches the object from
 * its parent, finds a safe spawn location (scenario_location_underwater),
 * sets the new position, clears flags, adds the ejection velocity, and
 * recomputes node matrices.
 *
 * Confirmed: cdecl, 1 stack param (object_handle).
 * Confirmed: calls object_get_and_verify_type twice (child and parent).
 * Confirmed: normalize3d returns 0.0 when vector is zero.
 * Confirmed: 0x2b6af4 = 0.02f (ejection velocity scale).
 * Confirmed: clears bit 5 (0x20) of flags at +0x4.
 * Confirmed: clears bit 15 (0x8000) of flags at +0x1b4.
 */
void unit_detach_from_parent(int object_handle)
{
  char *unit;
  int parent_handle;
  float parent_pos[3];
  float child_pos[3];
  float dir[3];
  float len;

  unit = (char *)object_get_and_verify_type(object_handle, 3);
  parent_handle = *(int *)(unit + 0xcc);
  if (parent_handle == -1)
    return;

  object_get_and_verify_type(parent_handle, 3);
  object_get_world_position(parent_handle, (vector3_t *)parent_pos);
  object_get_world_position(object_handle, (vector3_t *)child_pos);

  dir[0] = child_pos[0] - parent_pos[0];
  dir[1] = child_pos[1] - parent_pos[1];
  dir[2] = child_pos[2] - parent_pos[2];

  len = normalize3d(dir);
  if (len == *(float *)0x2533c0) {
    dir[0] = *(float *)(unit + 0x24);
    dir[1] = *(float *)(unit + 0x28);
    dir[2] = *(float *)(unit + 0x2c);
  }

  dir[0] = dir[0] * *(float *)0x2b6af4;
  dir[1] = dir[1] * *(float *)0x2b6af4;
  dir[2] = dir[2] * *(float *)0x2b6af4;

  object_detach_from_parent(object_handle);

  child_pos[0] = *(float *)(unit + 0xc);
  child_pos[1] = *(float *)(unit + 0x10);
  child_pos[2] = *(float *)(unit + 0x14);
  scenario_location_underwater(child_pos);
  object_set_position(object_handle, child_pos, NULL, NULL);

  *(uint32_t *)(unit + 0x4) &= ~0x20u;
  *(uint32_t *)(unit + 0x1b4) &= ~0x8000u;

  *(float *)(unit + 0x18) += dir[0];
  *(float *)(unit + 0x1c) += dir[1];
  *(float *)(unit + 0x20) += dir[2];

  object_set_garbage(object_handle, 1);
  object_compute_node_matrices(object_handle);
}

/* unit_update_seat_occupancy (0x1aa890)
 *
 * Walks the child object chain of a unit (starting at unk_200, offset 0xC8)
 * and updates two seat-occupancy handles: unk_724 (offset 0x2D4) for seats
 * with flag bit 2 (0x4) set, and unk_728 (offset 0x2D8) for seats with flag
 * bit 3 (0x8) set. For each child that is a biped or vehicle with a valid
 * seat tag index, retrieves the seat definition flags from the unit tag.
 *
 * Bit 2 (0x4): writes child handle to unk_724, but only if the unit's
 * unk_436 bit 0 is clear AND unk_724 is currently NONE.
 * Bit 3 (0x8): writes child handle to unk_728, but only if unk_728 is
 * currently NONE or equals unk_724.
 *
 * Register arg: unit_handle passed in EAX (vehicle_handle).
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: child chain via [ESI+0xC8] -> [EBX+0xC4].
 * Confirmed: seat flags from tag_block_get_element(tag+0x2e4, index, 0x11c).
 * Confirmed: TEST CL,0x4 and TEST CL,0x8 for seat flag bits.
 * Confirmed: stores to [ESI+0x2D4] and [ESI+0x2D8].
 */
void unit_update_seat_occupancy(int vehicle_handle)
{
  char *unit;
  char *unit_tag;
  int child_handle;
  char *child_obj;
  int *seat_element;
  int seat_flags;

  unit = (char *)object_get_and_verify_type(vehicle_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  child_handle = *(int *)(unit + 0xc8);

  while (child_handle != -1) {
    child_obj = (char *)object_get_and_verify_type(child_handle, -1);

    if (((1 << (*(uint8_t *)(child_obj + 0x64) & 0x1f)) & 3) &&
        *(int16_t *)(child_obj + 0x2a0) != -1) {
      seat_element = (int *)tag_block_get_element(
        unit_tag + 0x2e4, (int)*(int16_t *)(child_obj + 0x2a0), 0x11c);
      seat_flags = *seat_element;

      if ((seat_flags & 4) == 0 || (*(uint8_t *)(unit + 0x1b4) & 1) ||
          *(int *)(unit + 0x2d4) != -1) {
        /* bit 2 not set, or unit already has unk_724 occupied —
         * check bit 3 only */
        if (seat_flags & 8) {
          if (*(int *)(unit + 0x2d8) == -1 ||
              *(int *)(unit + 0x2d8) == *(int *)(unit + 0x2d4)) {
            *(int *)(unit + 0x2d8) = child_handle;
          }
        }
      } else {
        /* bit 2 set and unk_724 is NONE and unk_436 bit 0 clear */
        *(int *)(unit + 0x2d4) = child_handle;
        if (seat_flags & 8) {
          if (*(int *)(unit + 0x2d8) == -1) {
            *(int *)(unit + 0x2d8) = child_handle;
          }
        }
      }
    }

    child_handle = *(int *)(child_obj + 0xc4);
  }
}

/* unit_get_equipment (0x1aa970)
 *
 * Returns the equipment datum handle stored in the unit's equipment slot
 * (unit_data_t.unk_712, offset 0x2C8). Resolves the unit via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
int unit_get_equipment(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  return unit->unk_712.value;
}

/* unit_try_add_grenade (0x1aa990)
 *
 * Attempts to add a grenade to the unit's inventory. The equipment object
 * must be of powerup_type _equipment_powerup_grenade (6). Looks up the
 * grenade type's maximum count from the game globals tag block at offset
 * 0x128, then checks the unit's current grenade count array at offset 0x2CE.
 * If there is room, increments the count, plays an effect if a local player
 * is carrying the unit, and deletes the equipment object.
 *
 * Returns: true if the grenade was added, false if the unit is already at
 * maximum capacity for that grenade type.
 */
bool unit_try_add_grenade(int unit_handle, int equipment_handle)
{
  int *equipment_obj;
  char *equipment_tag;
  char *unit;
  int16_t grenade_type;
  int16_t max_count;
  char *game_globals;
  char current_count;
  int player_index;
  char *player;

  equipment_obj = (int *)object_get_and_verify_type(equipment_handle, 8);
  equipment_tag = (char *)tag_get(0x65716970, *equipment_obj);
  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (*(int16_t *)(equipment_tag + 0x308) != 6) {
    display_assert("equipment_definition->equipment.powerup_type==_equipment_"
                   "powerup_grenade",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1c72, 1);
    system_exit(-1);
  }

  grenade_type = *(int16_t *)(equipment_tag + 0x30a);
  game_globals = (char *)game_globals_get();
  max_count =
    *(int16_t *)tag_block_get_element(game_globals + 0x128, grenade_type, 0x44);

  if (max_count == 0)
    return false;

  current_count = *(char *)(unit + grenade_type + 0x2ce);
  if ((int16_t)current_count >= max_count)
    return false;

  *(char *)(unit + grenade_type + 0x2ce) = current_count + 1;

  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    player = (char *)datum_get(player_data, player_index);
    if (*(int16_t *)(player + 2) != -1)
      item_activate_equipment_effect(equipment_handle);
  }

  object_delete(equipment_handle);
  return true;
}

/* unit_set_grenade_count (0x1aaa90)
 *
 * Adds count to the unit's grenade count for the given grenade_type, then
 * records grenade_type as the "last active" grenade type in two byte fields.
 * The grenade count array lives at unit+0x2CE (one byte per grenade type).
 * Offsets 0x2CC and 0x2CD both receive the low byte of grenade_type; these
 * are the last-used-grenade-type fields (two copies, presumably for different
 * subsystems).
 *
 * Asserts: grenade_count >= 0; 0 <= grenade_type <
 * NUMBER_OF_UNIT_GRENADE_TYPES. Returns: updated grenade count (int16_t,
 * sign-extended from byte).
 *
 * Confirmed: CMP word ptr [EBP+0x10],0x0 -> grenade_count is short at +0x10.
 * Confirmed: MOV BX,[EBP+0xC] -> grenade_type is short at +0xC.
 * Confirmed: MOV DL,byte ptr [EBP+0x10]; ADD byte ptr [EAX],DL -> byte add.
 * Confirmed: MOV byte ptr [ESI+0x2cd],BL / MOV byte ptr [ESI+0x2cc],BL.
 * Confirmed: MOVSX AX,byte ptr [EAX] -> return is sign-extended byte (short).
 */
int16_t unit_set_grenade_count(int unit_handle, int16_t grenade_type,
                               int16_t grenade_count)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (grenade_count < 0) {
    display_assert("grenade_count>=0", "c:\\halo\\SOURCE\\units\\units.c",
                   0x1c8d, 1);
    system_exit(-1);
  }

  if ((grenade_type < 0) || (grenade_type > 1)) {
    display_assert(
      "(grenade_type >= 0) && (grenade_type < NUMBER_OF_UNIT_GRENADE_TYPES)",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1c8e, 1);
    system_exit(-1);
  }

  unit[grenade_type + 0x2ce] += (char)grenade_count;
  unit[0x2cd] = (char)grenade_type;
  unit[0x2cc] = (char)grenade_type;

  return (int16_t)unit[grenade_type + 0x2ce];
}

/* unit_pickup_equipment (0x1aab20)
 *
 * Attempts to pick up an equipment object for a unit. Validates that the
 * equipment's powerup_type is neither _equipment_powerup_none (0) nor
 * _equipment_powerup_grenade (6). If the unit already holds equipment
 * (unit+0x2C8 != NONE) and flag==1, deletes the existing equipment first.
 * If the equipment slot is free, detaches the equipment from the map,
 * hides it, attaches it to the unit, and triggers any pickup sound effect
 * if the unit has a controlling player. Returns true on success. */
bool unit_pickup_equipment(int unit_handle, int equipment_handle, short flag)
{
  int *equipment_obj;
  int equipment_def;
  char *unit_obj;
  int player_handle;
  char *player;

  equipment_obj = (int *)object_get_and_verify_type(equipment_handle, 8);
  equipment_def = (int)tag_get(0x65716970, *equipment_obj);
  unit_obj = (char *)object_get_and_verify_type(unit_handle, 3);

  if (*(short *)(equipment_def + 0x308) == 0) {
    display_assert(
      "equipment_definition->equipment.powerup_type!=_equipment_powerup_none",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1ca1, 1);
    system_exit(NONE);
  }
  if (*(short *)(equipment_def + 0x308) == 6) {
    display_assert("equipment_definition->equipment.powerup_type!=_equipment_"
                   "powerup_grenade",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1ca2, 1);
    system_exit(NONE);
  }

  if (*(int *)(unit_obj + 0x2c8) != NONE && flag == 1) {
    object_delete(*(int *)(unit_obj + 0x2c8));
    *(int *)(unit_obj + 0x2c8) = NONE;
  }

  if (*(int *)(unit_obj + 0x2c8) == NONE) {
    /* detach equipment from map and hide it */
    object_disconnect_from_map(equipment_handle);
    object_set_garbage(equipment_handle, 0);

    /* if the unit has a controlling player, trigger the pickup sound */
    player_handle = player_index_from_unit_index(unit_handle);
    if (player_handle != NONE) {
      player_handle = player_index_from_unit_index(unit_handle);
      player = (char *)datum_get(player_data, player_handle);
      if (*(short *)(player + 0x2) != NONE) {
        item_activate_equipment_effect(equipment_handle);
      }
    }

    /* attach equipment to unit */
    item_attach_to_unit(equipment_handle, unit_handle);
    *(int *)(unit_obj + 0x2c8) = equipment_handle;
    return true;
  }

  return false;
}

/* unit_clear_seat_tag (0x1aac40)
 *
 * Clears the unit's equipment/seat tag handle at offset 0x2C8
 * (unit_data_t.unk_712). If the current value is not NONE (-1), it calls
 * object_delete (0x140cc0) on that handle to destroy the associated object,
 * then sets the field to NONE. Resolves the unit via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
void unit_clear_seat_tag(int unit_handle)
{
  unit_data_t *unit;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  if (unit->unk_712.value != -1) {
    object_delete(unit->unk_712.value);
    unit->unk_712.value = -1;
  }
}

/* unit_clear_weapons (0x1aac80)
 *
 * Deletes all weapons from a unit's weapon slots EXCEPT the one at the
 * current weapon index (unk_674, offset 0x2A2). For each of the 4 weapon
 * slots: if the slot is occupied (handle != NONE) and the slot index does
 * not match the current weapon index, deletes the weapon object and clears
 * the slot handle to NONE. Also resets the next-weapon index (unk_676,
 * offset 0x2A4) or current-weapon index (unk_674) to NONE if they matched
 * the cleared slot. Called by unit_enter_seat when flag==2 to strip all
 * secondary weapons before seating.
 */
void unit_clear_weapons(int unit_handle)
{
  unit_data_t *unit;
  int16_t i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (unit->unk_680[i].value != -1 && i != (int16_t)unit->unk_674) {
      object_delete(unit->unk_680[i].value);
      unit->unk_680[i].value = -1;
      if (i == (int16_t)unit->unk_676) {
        unit->unk_676 = (uint16_t)-1;
      }
      if (i == (int16_t)unit->unk_674) {
        unit->unk_674 = (uint16_t)-1;
      }
    }
  }
}

/* unit_find_empty_weapon_slot (0x1aad60)
 *
 * Scans the unit's 4 weapon slots (unit_data_t.unk_680, offset 0x2A8)
 * for the first slot containing NONE (-1) and returns its index (0-3).
 * Returns -1 if all slots are occupied.
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type(handle, 3).
 * Confirmed: CMP dword ptr [EAX + ESI*4 + 0x2a8], -1 — weapon slot check.
 * Confirmed: CMP CX, 0x4 — loop bound is 4 (MAXIMUM_WEAPONS_PER_UNIT).
 * Confirmed: Returns int16_t (MOV AX,CX / MOV AX,DX).
 */
int16_t unit_find_empty_weapon_slot(int unit_handle)
{
  unit_data_t *unit;
  int16_t i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    if (unit->unk_680[i].value == -1)
      return i;
  }
  return -1;
}

/* unit_count_weapons (0x1aad90)
 *
 * Counts the number of "countable" weapons held by a unit. Iterates all 4
 * weapon slots (unit_data_t.unk_680, offset 0x2A8). For each non-NONE slot,
 * resolves the weapon object, looks up its weapon tag via tag_get("weap"),
 * and checks byte at tag+0x308 bit 0x10. Weapons without that bit set are
 * counted. Returns the count as int16_t.
 */
int16_t unit_count_weapons(int unit_handle)
{
  unit_data_t *unit;
  int count;
  int weapon_handle;
  int *weapon_obj;
  char *weapon_tag;
  int i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  count = 0;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    weapon_handle = unit->unk_680[i].value;
    if (weapon_handle != -1) {
      weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      weapon_tag = (char *)tag_get(0x77656170, *weapon_obj);
      if ((*(uint8_t *)(weapon_tag + 0x308) & 0x10) == 0) {
        count++;
      }
    }
  }

  return (int16_t)count;
}

/* unit_weapon_is_new (0x1aae00)
 *
 * Returns true if the given weapon is "new" to the unit — i.e. no existing
 * weapon in the unit's 4 weapon slots shares the same tag definition (first
 * dword of the weapon object data, the tag_index). Resolves the target weapon
 * via object_get_and_verify_type with type mask 4 (weapon), then iterates all
 * weapon slots comparing tag indices. If any match is found, returns false;
 * otherwise returns true.
 */
bool unit_weapon_is_new(int unit_handle, int weapon_unit_handle)
{
  unit_data_t *unit;
  int *target_weapon_obj;
  int *slot_weapon_obj;
  bool is_new;
  int weapon_handle;
  int i;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  target_weapon_obj = (int *)object_get_and_verify_type(weapon_unit_handle, 4);
  tag_get(0x77656170, *target_weapon_obj);
  is_new = true;

  for (i = 0; i < MAXIMUM_WEAPONS_PER_UNIT; i++) {
    weapon_handle = unit->unk_680[i].value;
    if (weapon_handle != -1) {
      slot_weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
      if (*target_weapon_obj == *slot_weapon_obj) {
        is_new = false;
      }
    }
  }

  return is_new;
}

/* unit_get_grenade_count (0x1aae70)
 *
 * Returns the current grenade count for the given grenade_type from the
 * unit's grenade count array at unit+0x2CE. If grenade_type is NONE (-1),
 * returns 0 without reading the array. The count byte is sign-extended to
 * int16_t before return.
 *
 * Asserts: grenade_type == NONE || (0 <= grenade_type <
 * NUMBER_OF_UNIT_GRENADE_TYPES).
 *
 * Confirmed: MOV SI,[EBP+0xC] -> grenade_type is short at +0xC.
 * Confirmed: CMP SI,-0x1 / JZ -> NONE check precedes range assert.
 * Confirmed: MOVSX AX,byte ptr [ECX+EDI+0x2CE] -> sign-extended byte return.
 * Confirmed: XOR AX,AX in NONE branch -> return 0.
 */
int16_t unit_get_grenade_count(int unit_handle, int16_t grenade_type)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (grenade_type == -1)
    return 0;

  if ((grenade_type < 0) || (grenade_type > 1)) {
    display_assert("grenade_type==NONE || (grenade_type>=0 && "
                   "grenade_type<NUMBER_OF_UNIT_GRENADE_TYPES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1ea7, 1);
    system_exit(-1);
  }

  return (int16_t)unit[grenade_type + 0x2ce];
}

/* unit_get_current_grenade_type (0x1aaee0)
 *
 * Returns the current grenade type index for the given unit. Validates
 * that the stored index at unit+0x2cc is NONE (-1) or in [0, 2).
 * Returns -1 (NONE) or 0/1 as a sign-extended int16_t.
 *
 * Confirmed: cdecl, 1 stack param (unit_handle).
 * Confirmed: object_get_and_verify_type(param_1, 3).
 * Confirmed: assert string at 0x2b6c98, line 0x1eb8.
 * Confirmed: sign-extends byte at +0x2cc to int16_t return.
 */
int16_t unit_get_current_grenade_type(int unit_handle)
{
  char *unit;
  char grenade_index;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  grenade_index = *(char *)(unit + 0x2cc);

  if (grenade_index != -1 && (grenade_index < 0 || grenade_index > 1)) {
    display_assert(
        "unit->unit.current_grenade_index==NONE || "
        "(unit->unit.current_grenade_index>=0 && "
        "unit->unit.current_grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
        "c:\\halo\\SOURCE\\units\\units.c", 0x1eb8, 1);
    system_exit(-1);
  }

  return (int16_t)(signed char)*(char *)(unit + 0x2cc);
}

/* FUN_001ab6e0 (0x1ab6e0)
 * Returns a pointer to the base seat name string given a base_seat_index.
 * Asserts that the index is in [0, NUMBER_OF_UNIT_BASE_SEATS).
 * The seat name table at 0x32e484 contains: asleep, alert, stand, crouch, flee, flaming. */
const char *FUN_001ab6e0(int16_t base_seat_index)
{
  if (base_seat_index < 0 || base_seat_index >= NUMBER_OF_UNIT_BASE_SEATS) {
    display_assert(
        "base_seat_index>=0 && base_seat_index<NUMBER_OF_UNIT_BASE_SEATS",
        "c:\\halo\\SOURCE\\units\\units.c", 0x200f, 1);
    system_exit(-1);
  }
  return *(const char **)(0x32e484 + (int)base_seat_index * 4);
}

/* FUN_001ab730 (0x1ab730)
 * Searches the base seat name table for a matching name (case-insensitive).
 * Returns the index [0..5] if found, or -1 if no match.
 * @edi = seat_name string to search for. */
int16_t FUN_001ab730(const char *seat_name)
{
  int16_t i;

  for (i = 0; i < NUMBER_OF_UNIT_BASE_SEATS; i++) {
    if (crt_stricmp(seat_name,
                    *(const char **)(0x32e484 + (int)i * 4)) == 0) {
      return i;
    }
  }
  return -1;
}

/* FUN_001ab770 (0x1ab770)
 * Returns a pointer to the base weapon name string given a base_weapon_index.
 * Asserts that the index is in [0, NUMBER_OF_UNIT_BASE_WEAPONS).
 * The weapon name table is a local array containing just "unarmed". */
const char *FUN_001ab770(int16_t base_weapon_index)
{
  const char *weapon_names[1];

  weapon_names[0] = "unarmed";

  if (base_weapon_index < 0 || base_weapon_index >= NUMBER_OF_UNIT_BASE_WEAPONS) {
    display_assert(
        "base_weapon_index>=0 && "
        "base_weapon_index<NUMBER_OF_UNIT_BASE_WEAPONS",
        "c:\\halo\\SOURCE\\units\\units.c", 0x2043, 1);
    system_exit(-1);
  }
  return weapon_names[base_weapon_index];
}

/* unit_set_animation (0x1ab7c0)
 *
 * Sets the current animation on a unit object. Writes the animation graph
 * tag index to offset 0x7c, the animation index (int16_t) to offset 0x80,
 * and zeroes the animation frame counter at offset 0x82. When the debug
 * flag at 0x5054fc is set, logs the unit name and animation name to the
 * console via console_printf, optionally filtered by the debug unit handle
 * at 0x5ac9f8.
 */
void unit_set_animation(int unit_handle, int anim_graph_tag_index,
                        int16_t animation_index)
{
  int *unit;
  const char *anim_name;
  int debug_filter;
  void *tag_data;

  unit = (int *)object_get_and_verify_type(unit_handle, 3);

  /* Set animation graph tag index, animation index, and zero frame counter */
  *(int *)((char *)unit + 0x7c) = anim_graph_tag_index;
  *(int16_t *)((char *)unit + 0x80) = animation_index;
  *(int16_t *)((char *)unit + 0x82) = 0;

  /* Debug logging path */
  if (*(char *)0x5054fc != 0) {
    anim_name = "<none>";
    if (anim_graph_tag_index != -1) {
      tag_data = tag_get(0x616e7472, anim_graph_tag_index);
      if (animation_index != -1) {
        anim_name = (const char *)tag_block_get_element(
          (char *)tag_data + 0x74, (int)animation_index, 0xb4);
      }
    }

    debug_filter = *(int *)0x5ac9f8;
    if (debug_filter == -1 || *(int *)((char *)unit + 0x1a4) == debug_filter ||
        *(int *)((char *)unit + 0x1a8) == debug_filter) {
      console_printf(0, "%s: animation %s",
                     tag_name_strip_path(tag_get_name(*(int *)unit)),
                     anim_name);
    }
  }
}

/* FUN_001ab870 (0x1ab870)
 * Updates an animation state and optionally triggers an impulse sound.
 * Calls animation_update_internal with update_kind=1, using the animation
 * graph tag index and state pointer passed via @ecx/@edx.
 * If the update produces a sound index (!= -1), plays it via
 * object_impulse_sound_new with scale 1.0. Returns the animation result. */
int16_t FUN_001ab870(void *animation_state, int animation_graph_tag_index,
                     int unit_handle)
{
  int16_t result;
  int sound_index;

  result = (int16_t)animation_update_internal(
      1, animation_graph_tag_index, (short *)animation_state, &sound_index);
  if (sound_index != -1) {
    object_impulse_sound_new(unit_handle, sound_index, 0,
                             *(float **)0x31fc1c,
                             *(float **)0x31fc3c, 1.0f);
  }
  return result;
}

/* FUN_001ab8c0 (0x1ab8c0)
 * Computes or copies lighting data for a unit.
 * If the unit has a parent unit (at +0xcc), copies the parent's lighting
 * values from +0x290 and +0x294. Otherwise, computes an ambient RGB color
 * brightness and self-illumination value from the unit's position and
 * orientation. */
void FUN_001ab8c0(int unit_handle)
{
  char *unit;
  void *parent;
  float color[3];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  parent = object_try_and_get_and_verify_type(
      *(int *)(unit + 0xcc), 3);
  if (parent == NULL) {
    FUN_0013a740((int)(unit + 0xc), (int)(unit + 0x48), color);
    *(float *)(unit + 0x290) = real_rgb_color_brightness(color);
    *(float *)(unit + 0x294) = object_get_self_illumination(unit_handle);
    return;
  }
  *(float *)(unit + 0x290) = *(float *)((char *)parent + 0x290);
  *(float *)(unit + 0x294) = *(float *)((char *)parent + 0x294);
}

/* FUN_001ab940 (0x1ab940)
 * Returns the weapon handle at the given weapon slot index for a unit.
 * The index is validated against [0, MAXIMUM_WEAPONS_PER_UNIT).
 * Returns -1 (NONE) if the index is -1. */
int FUN_001ab940(int16_t weapon_index, char *unit_data)
{
  if (weapon_index == -1) {
    return -1;
  }
  if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
    system_exit(-1);
  }
  return *(int *)(unit_data + 0x2a8 + (int)weapon_index * 4);
}

/* unit_detach_weapon (0x1ab990)
 *
 * Detaches a weapon/item from a unit. If the item has no parent, connects it
 * to the map and attaches it at the unit's "left hand" marker. If it has a
 * parent, asserts that the parent is the unit. Then detaches the item from
 * the unit, clears its velocity, generates a random direction offset based on
 * the unit's unk_492 facing vector with a small random angle and scale, adds
 * that to the root position, and attempts to place the item at the new
 * position. If placement fails and the game engine is not running, deletes
 * the item. Also deletes if the unit has flag bit 0x100000 set in unk_436.
 *
 * Register args: unit_handle in EDI, weapon_handle in ESI.
 *
 * Confirmed: PUSH 0x3 / PUSH EDI -> object_get_and_verify_type(unit, 3).
 * Confirmed: PUSH 0x1c / PUSH ESI -> object_get_and_verify_type(weapon, 0x1c).
 * Confirmed: parent check at [EBX+0xCC] against -1 and EDI.
 * Confirmed: object_attach_to_marker(edi, "left hand", esi, "").
 * Confirmed: item_attach_to_unit(esi, -1) to detach.
 * Confirmed: global zero vector copied from [0x31fc38].
 * Confirmed: assert "item->object.parent_object_index==unit_index" at 0x20c5.
 * Confirmed: random_direction3d with angle 0x3ec90fdb, scale range [0x3cda740e,
 * 0x3d23d70b]. Confirmed: [EBX+0x1b0] = EDI (weapon remembers its detaching
 * unit). Confirmed: flag check at unit+0x1b4 bit 0x100000.
 */
void unit_detach_weapon(int unit_handle, int weapon_handle)
{
  char *unit;
  char *weapon;
  int parent;
  float *global_origin;
  float direction[3];
  float position[3];
  int *seed;
  float scale;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon = (char *)object_get_and_verify_type(weapon_handle, 0x1c);
  parent = *(int *)(weapon + 0xcc);

  if (parent == -1) {
    /* Weapon has no parent — connect to map and attach at marker */
    object_connect_to_map(weapon_handle, 0);
    object_set_garbage(weapon_handle, 1);
    object_attach_to_marker(unit_handle, (void *)0x2b6d2c, weapon_handle,
                            (void *)0x25386f);
  } else if (parent != unit_handle) {
    display_assert("item->object.parent_object_index==unit_index",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x20c5, 1);
    system_exit(-1);
  }

  /* Detach weapon from unit and parent */
  item_attach_to_unit(weapon_handle, -1);
  object_detach_from_parent(weapon_handle);

  /* Zero the weapon's velocity fields at +0x18 and +0x3c */
  global_origin = *(float **)0x31fc38;
  *(float *)(weapon + 0x18) = global_origin[0];
  *(float *)(weapon + 0x1c) = global_origin[1];
  *(float *)(weapon + 0x20) = global_origin[2];

  global_origin = *(float **)0x31fc38;
  *(float *)(weapon + 0x3c) = global_origin[0];
  *(float *)(weapon + 0x40) = global_origin[1];
  *(float *)(weapon + 0x44) = global_origin[2];

  /* Generate random throw direction from unit's facing vector (unk_492) */
  seed = get_global_random_seed_address();
  random_direction3d(seed, (float *)(unit + 0x1ec), 0.0f, 0.39269909f,
                     direction);

  /* Scale direction by random amount */
  seed = get_global_random_seed_address();
  scale = random_real_range(seed, 0.02666667f, 0.04f);
  direction[0] *= scale;
  direction[1] *= scale;
  direction[2] *= scale;

  /* Get root parent position and offset by scaled direction */
  object_get_root_location(unit_handle, position, 0);
  direction[0] = position[0] + direction[0];
  direction[1] = position[1] + direction[1];
  direction[2] = position[2] + direction[2];

  /* Record the detaching unit on the weapon */
  *(int *)(weapon + 0x1b0) = unit_handle;

  /* Set weapon position and try to place it */
  item_set_position(weapon_handle, direction, 0);
  unit_set_seat_state(unit_handle, position);

  if (!object_try_place(weapon_handle, position)) {
    if (!game_engine_running()) {
      object_delete(weapon_handle);
    }
  }

  /* If unit has death/despawn flag, force-delete the weapon */
  if (*(uint32_t *)(unit + 0x1b4) & 0x100000) {
    object_delete(weapon_handle);
  }
}

/* unit_has_weapon_with_flag (0x1ac3f0)
 *
 * Returns true if any of the unit's equipped weapons has the given flag
 * bit set in its flags field (weapon_data+0x1dc).
 *
 * Walks the 4-slot weapon handle array at unit+0x2a8; for each slot that
 * is not NONE (-1), resolves the weapon object with type_mask=4 and tests
 * bit (1 << flag_index) against the 32-bit flags at weapon_data+0x1dc.
 *
 * Confirmed: LEA ESI,[EAX+0x2a8] — weapon slot array at unit+0x2a8.
 * Confirmed: PUSH 0x4 for weapon type_mask in inner object_get_and_verify_type.
 * Confirmed: MOV ECX,BX; SHL EDX,CL — flag_index used as shift count (byte).
 * Confirmed: CMP EAX,-0x1 / JZ skip — slot NONE guard.
 * Confirmed: TEST EDX,ECX at [EAX+0x1dc] — flags field at +0x1dc.
 */
bool unit_has_weapon_with_flag(int unit_handle, int flag_index)
{
  int *unit;
  int *weapon_slots;
  int i;

  unit = (int *)object_get_and_verify_type(unit_handle, 3);
  weapon_slots = (int *)((char *)unit + 0x2a8);

  for (i = 0; i < 4; i++) {
    if (weapon_slots[i] != NONE) {
      int *weapon = (int *)object_get_and_verify_type(weapon_slots[i], 4);
      if ((1 << (flag_index & 0x1f)) & *(uint32_t *)((char *)weapon + 0x1dc))
        return 1;
    }
  }
  return 0;
}

/* unit_try_animation_state (0x1acd70)
 *
 * Searches the unit's animation graph for a matching animation mode and
 * weapon label. The animation graph is resolved via: unit tag -> antr tag
 * at offset +0x44. The antr tag's animation modes block starts at tag+0xc.
 *
 * For each mode:
 *   - If seat_label is non-NULL, compares it (case-insensitive) against the
 *     mode's name string; skips non-matching modes.
 *   - Within the mode, iterates sub-animations at mode+0x58 (size 0xBC each).
 *   - Within each sub-animation, iterates weapon labels at sub_anim+0xB0
 *     (size 0x3C each).
 *   - Matches weapon_label: NULL matches anything; "unarmed" matches empty
 *     strings; otherwise case-insensitive compare.
 *
 * If reset_flag is 0, returns true on first match without updating state.
 * If reset_flag is non-zero, updates the unit's animation state fields:
 *   - unk_592 (0x250) = mode index
 *   - unk_593 (0x251) = sub-animation index
 *   - unk_594 (0x252) = weapon label index
 *   - base_seat_index (0x257) = matched base seat label index (-1 if none)
 *   - unk_595 (0x253) = 0xff if previously != 0x1c
 *   - unk_584 (0x248) bit 1: set if mode has multi-weapon animation channels
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: tag_get('unit', *unit) then tag_get('antr', unit_tag+0x44).
 * Confirmed: stricmp via CALL 0x1dd801.
 * Confirmed: csstrcmp via CALL 0x8dcb0 for "unarmed" check.
 * Confirmed: stores to offsets 0x250, 0x251, 0x252, 0x253, 0x257, 0x248.
 * Confirmed: base_seat_labels table at 0x32e484, 6 entries.
 */
bool unit_try_animation_state(int unit_handle, int seat_label, int weapon_label,
                              int reset_flag)
{
  char *unit;
  char *unit_tag;
  char *antr_tag;
  int *anim_block;
  int mode_count;
  int mode_index;
  char *mode;
  int sub_count;
  int16_t sub_index;
  char *sub_anim;
  int *weapon_block;
  int weapon_count;
  int16_t weapon_index;
  char *weapon_name;
  bool found;
  bool has_multi_weapon;
  int16_t base_seat;
  int16_t si;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  anim_block = (int *)(antr_tag + 0xc);
  mode_count = *anim_block;
  found = false;

  if (mode_count < 1)
    return false;

  mode_index = 0;
  while (1) {
    mode = (char *)tag_block_get_element(anim_block, mode_index, 0x64);

    if (seat_label != 0 && crt_stricmp((const char *)seat_label, mode) != 0) {
      goto next_mode;
    }

    sub_count = *(int *)(mode + 0x58);
    sub_index = 0;
    if (sub_count < 1)
      goto next_mode;

    while (1) {
      sub_anim =
        (char *)tag_block_get_element(mode + 0x58, (int)sub_index, 0xbc);
      weapon_block = (int *)(sub_anim + 0xb0);
      weapon_count = *weapon_block;
      weapon_index = 0;

      if (weapon_count < 1)
        goto next_sub;

      while (1) {
        weapon_name =
          (char *)tag_block_get_element(weapon_block, (int)weapon_index, 0x3c);

        if (weapon_label == 0)
          goto matched;
        if (csstrcmp((const char *)weapon_label, "unarmed") == 0 &&
            *weapon_name == '\0')
          goto matched;
        if (crt_stricmp((const char *)weapon_label, weapon_name) == 0)
          goto matched;

        weapon_index++;
        if ((int)(int16_t)weapon_index >= weapon_count)
          goto next_sub;
        continue;

      matched:
        if (reset_flag == 0)
          goto found_match;

        /* Check if mode has multi-weapon animation channels */
        {
          int num_key_types = *(int *)(mode + 0x40);
          int *key_data = *(int **)(mode + 0x44);
          has_multi_weapon = false;

          if ((num_key_types >= 3 && *(int16_t *)(key_data + 1) != -1) ||
              (num_key_types >= 4 &&
               *(int16_t *)((char *)key_data + 6) != -1) ||
              (num_key_types >= 5 &&
               *(int16_t *)((char *)key_data + 8) != -1)) {
            has_multi_weapon = true;
          }
        }

        if (*(uint8_t *)(unit + 0x253) != 0x1c)
          *(uint8_t *)(unit + 0x253) = 0xff;

        *(uint8_t *)(unit + 0x250) = (uint8_t)mode_index;

        /* Find base_seat_index by matching seat_label against table */
        base_seat = -1;
        for (si = 0; si < NUMBER_OF_UNIT_BASE_SEATS; si++) {
          if (crt_stricmp((const char *)seat_label,
                          *(const char **)(0x32e484 + si * 4)) == 0) {
            base_seat = si;
            break;
          }
        }

        *(uint8_t *)(unit + 0x252) = (uint8_t)weapon_index;
        *(int8_t *)(unit + 0x257) = (int8_t)base_seat;
        *(uint8_t *)(unit + 0x251) = (uint8_t)sub_index;

        if (has_multi_weapon) {
          *(uint8_t *)(unit + 0x248) |= 0x2;
        } else {
          *(uint8_t *)(unit + 0x248) &= ~0x2;
        }

      found_match:
        found = true;
        goto next_sub;
      }

    next_sub:
      sub_index++;
      if ((int)(int16_t)sub_index >= *(int *)(mode + 0x58))
        goto next_mode;
    }

  next_mode:
    mode_index++;
    if ((int)(int16_t)mode_index >= *anim_block)
      break;
  }

  return found;
}

/* FUN_001ad260 (0x1ad260) — unit animation state transition
 *
 * Sets the unit's animation state (unk_595 at offset 0x253). Looks up the
 * correct animation from the unit tag's animation graph hierarchy based on
 * the desired state (param_2). There are two lookup paths:
 *
 *   1. Mode-relative: looks up in the mode's animation block at mode+0x98/0x9c
 *      using a "mode animation index" (EBX path).
 *   2. Overlay-relative: looks up in the unit mode's overlay block at
 *      unit_mode+0x40/0x44 using an "overlay index" (local_c path).
 *
 * After lookup, calls FUN_00120f20 (model_animation_choose_random) and
 * unit_set_animation to apply. Handles developer-mode missing-animation
 * warnings. Also resolves the "weapon idle" overlay and stores it.
 *
 * Called by unit_abort_animation, unit_open, unit_close, and others.
 *
 * Returns 1 on success, 0 on failure (animation not found for certain states).
 *
 * Confirmed: cdecl, 2 stack params (unit_handle, anim_state as int16_t).
 * Confirmed: jump table at 0x1ad714 (44 entries for states 0..0x2b).
 * Confirmed: calls unit_set_animation(@eax,@edi,@bx).
 * Confirmed: calls FUN_001a88b0(@ecx = anim_state).
 * Confirmed: DAT_005054fb = developer mode flag.
 * Confirmed: 0x322308 = mode anim name table, 0x322450 = overlay anim name table.
 */
char FUN_001ad260(int unit_handle, int16_t anim_state)
{
  int *unit;
  char *unit_tag;
  char *antr_tag;
  char *unit_mode;
  char *mode_block;
  int anim_graph_tag_index;
  int16_t mode_anim_index;
  int16_t overlay_index;
  int16_t anim_index;
  int16_t old_state;
  char was_none;
  char did_change;
  int16_t weapon_idle_anim;
  int16_t transition_speed;

  unit = (int *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  anim_graph_tag_index = *(int *)(unit_tag + 0x44);
  antr_tag = (char *)tag_get(0x616e7472, anim_graph_tag_index);

  unit_mode = (char *)tag_block_get_element(antr_tag + 0xc,
      (int)*(signed char *)((char *)unit + 0x250), 100);
  mode_block = (char *)tag_block_get_element(unit_mode + 0x58,
      (int)*(signed char *)((char *)unit + 0x251), 0xbc);
  tag_block_get_element(mode_block + 0xb0,
      (int)*(signed char *)((char *)unit + 0x252), 0x3c);

  old_state = (int16_t)(signed char)*((char *)unit + 0x253);
  was_none = (old_state == -1);
  did_change = 0;

  if (!was_none && anim_state == old_state)
    goto resolve_weapon_idle;

  mode_anim_index = -1;
  overlay_index = -1;

  if (old_state == 0x21) {
    FUN_001ab110(unit_handle, 1);
  }

  /* Map anim_state to either a mode animation index or overlay index */
  if ((int)anim_state > 0x2b)
    goto set_not_found;

  switch ((int)anim_state) {
  case 0:  mode_anim_index = 0; break;
  case 1:  mode_anim_index = 1; break;
  case 2:  mode_anim_index = 2; break;
  case 3:  mode_anim_index = 3; break;
  case 4:  mode_anim_index = 8; break;
  case 5:  mode_anim_index = 9; break;
  case 6:  mode_anim_index = 10; break;
  case 7:  mode_anim_index = 11; break;
  case 8:  mode_anim_index = 0x23; break;
  case 9:  mode_anim_index = 0x24; break;
  case 10: mode_anim_index = 0x25; break;
  case 11: mode_anim_index = 0x26; break;
  case 12: mode_anim_index = 12; break;
  case 13: mode_anim_index = 13; break;
  case 14: mode_anim_index = 14; break;
  case 15: mode_anim_index = 15; break;
  case 0x14: mode_anim_index = 0x10; break;
  case 0x15: mode_anim_index = 0x11; break;
  case 0x16: mode_anim_index = 0x12; break;
  case 0x1e: mode_anim_index = 0x27; break;
  case 0x1f: mode_anim_index = 0x2a; break;
  case 0x20: mode_anim_index = 0x2e; break;
  case 0x21: mode_anim_index = 0x14; break;
  case 0x22: mode_anim_index = 0x2c; break;
  case 0x23: mode_anim_index = 0x2d; break;
  case 0x24: mode_anim_index = 0x2f; break;
  case 0x27: mode_anim_index = 0x30; break;
  case 0x28: mode_anim_index = 0x31; break;
  case 0x29: mode_anim_index = 0x32; break;
  case 0x10: overlay_index = 0x17; goto overlay_path;
  case 0x11: overlay_index = 0x18; goto overlay_path;
  case 0x12: overlay_index = 0x19; goto overlay_path;
  case 0x13: overlay_index = 0x1a; goto overlay_path;
  case 0x18: overlay_index = 0; goto overlay_path;
  case 0x19: overlay_index = 1; goto overlay_path;
  case 0x25: overlay_index = 0x1b; goto overlay_path;
  case 0x26: overlay_index = 0x1c; goto overlay_path;
  case 0x2b: overlay_index = 0x1d; goto overlay_path;
  default:
    goto set_not_found;
  }

  /* Mode animation lookup path */
  if ((int)mode_anim_index < *(int *)(mode_block + 0x98)) {
    anim_index = *(int16_t *)(*(int *)(mode_block + 0x9c) +
                              (int)mode_anim_index * 2);
  } else {
    goto set_not_found;
  }
  goto check_found;

overlay_path:
  /* Overlay animation lookup path */
  if ((int)overlay_index >= *(int *)(unit_mode + 0x40))
    goto set_not_found;
  anim_index = *(int16_t *)(*(int *)(unit_mode + 0x44) +
                            (int)overlay_index * 2);
  goto check_found;

set_not_found:
  anim_index = -1;

check_found:
  /* Check if animation was found */
  if (*(uint8_t *)0x5054fb != 0 && *(int16_t *)((char *)unit + 0x64) == 0) {
    /* Developer mode: warn about missing animations */
    if (anim_index == -1) {
      const char *mn;
      const char *on;
      if (mode_anim_index != -1) {
        mn = FUN_001205f0((void *)0x322308, mode_anim_index);
      } else {
        mn = (const char *)0x25386f;
      }
      if (overlay_index != -1) {
        on = (const char *)FUN_001205f0((void *)0x322450, overlay_index);
      } else {
        on = (const char *)mode_block;
      }
      console_warning("MISSING: %s \'%s %s %s\'",
          tag_name_strip_path(*(const char **)(unit_tag + 0x3c)),
          unit_mode, on, mn);
      goto check_early_return;
    }
  } else {
    if (anim_index == -1)
      goto check_early_return;
  }
  goto apply_animation;

check_early_return:
  /* For certain states, return 0 if animation not found */
  switch ((int)anim_state) {
  case 0x1e:
  case 0x1f:
  case 0x20:
  case 0x21:
  case 0x27:
  case 0x29:
    return 0;
  }

apply_animation:
  anim_index = (int16_t)model_animation_choose_random(1, anim_graph_tag_index,
                                                       anim_index);
  unit_set_animation(unit_handle, anim_graph_tag_index, anim_index);

  old_state = (int16_t)(signed char)*((char *)unit + 0x253);

  /* Determine transition speed */
  transition_speed = 6;
  if ((anim_state == 0 || anim_state == 2 || anim_state == 3) &&
      (old_state == 0 || old_state == 2 || old_state == 3)) {
    transition_speed = 1;
  }
  if (anim_state == 0x16 || anim_state == 0x15) {
    transition_speed = 2;
  }

  did_change = 1;

resolve_weapon_idle:
  {
    int16_t new_weapon_idle;

    new_weapon_idle = FUN_001a88b0(anim_state);

    if (was_none || new_weapon_idle != FUN_001a88b0(old_state)) {
      /* Resolve weapon idle animation */
      if (new_weapon_idle >= 0 &&
          (int)new_weapon_idle < *(int *)(mode_block + 0x98)) {
        weapon_idle_anim = *(int16_t *)(*(int *)(mode_block + 0x9c) +
                                        (int)new_weapon_idle * 2);
      } else {
        weapon_idle_anim = -1;
      }

      weapon_idle_anim = (int16_t)model_animation_choose_random(
          1, anim_graph_tag_index, weapon_idle_anim);
      *(int16_t *)((char *)unit + 0x24a) = weapon_idle_anim;

      if (*(uint8_t *)0x5054fb != 0 &&
          *(int16_t *)((char *)unit + 0x64) == 0 &&
          weapon_idle_anim == -1 && new_weapon_idle != -1) {
        const char *wn = FUN_001205f0((void *)0x322308, (int16_t)new_weapon_idle);
        console_warning("MISSING: %s \'%s %s %s\'",
            tag_name_strip_path(*(const char **)(unit_tag + 0x3c)),
            unit_mode, mode_block, wn);
      }

      if (was_none) {
        /* Resolve base weapon overlay (index 9) */
        if (*(int *)(unit_mode + 0x40) > 9) {
          weapon_idle_anim = *(int16_t *)(*(int *)(unit_mode + 0x44) + 0x12);
        } else {
          weapon_idle_anim = -1;
        }
        weapon_idle_anim = (int16_t)model_animation_choose_random(
            1, anim_graph_tag_index, weapon_idle_anim);
        *(int16_t *)((char *)unit + 0x24c) = weapon_idle_anim;

        if (*(uint8_t *)0x5054fb != 0 &&
            *(int16_t *)((char *)unit + 0x64) == 0 &&
            weapon_idle_anim == -1) {
          const char *oln = FUN_001205f0((void *)0x322450, 9);
          console_warning("MISSING: %s \'%s %s\'",
              tag_name_strip_path(*(const char **)(unit_tag + 0x3c)),
              unit_mode, oln);
        }

        transition_speed = 6;
      }
    } else if (did_change == 0) {
      goto skip_transition;
    }

    object_set_region_count(unit_handle, transition_speed);
  }

skip_transition:
  *((char *)unit + 0x253) = (char)anim_state;
  return 1;
}

/* unit_find_best_enter_seat (0x1ad800)
 *
 * Iterates over the target unit's seat definitions and finds the best seat
 * for the given unit to enter. For each seat, computes entry positions via
 * unit_get_seat_enter_position and checks distances from the unit's world
 * position. Seats are scored based on whether they are empty (state 2) or
 * occupied by a unit that can be approached (state 1).
 *
 * Returns: seat state (0 = none found, 1 = approach occupied seat,
 * 2 = enter empty seat). Writes the chosen seat index through out_seat_index.
 */
uint16_t unit_find_best_enter_seat(int unit_handle, int target_unit_handle,
                                   int16_t *out_seat_index)
{
  char *unit;
  char *target_unit;
  char *target_tag;
  int seat_count;
  int *seat_block;
  int seat_index;
  int best_seat;
  uint16_t best_state;
  float best_distance;
  float distance;
  float distance2;
  float pos_a[3];
  float pos_b[3];
  uint8_t found_flag;

  unit = object_get_and_verify_type(unit_handle, 3);
  target_unit = object_get_and_verify_type(target_unit_handle, 3);
  target_tag = tag_get(0x756e6974, *(int *)target_unit);

  best_state = 0;
  best_seat = -1;
  found_flag = 0;

  if ((*(uint8_t *)(target_unit + 0xb6) & 4) == 0 &&
      (*(uint32_t *)(target_unit + 0x1b4) & 0x10000) == 0) {
    seat_block = (int *)(target_tag + 0x2e4);
    seat_count = *seat_block;
    best_distance = 3.4028235e+38f;

    for (seat_index = 0; seat_index < seat_count; seat_index++) {
      unsigned int *seat_element =
        tag_block_get_element(seat_block, seat_index, 0x11c);

      if (!unit_get_seat_enter_position(unit_handle, target_unit_handle,
                                        (int16_t)seat_index, pos_a, pos_b,
                                        NULL))
        continue;

      distance = sqrtf((*(float *)(unit + 0x50) - pos_b[0]) *
                         (*(float *)(unit + 0x50) - pos_b[0]) +
                       (*(float *)(unit + 0x54) - pos_b[1]) *
                         (*(float *)(unit + 0x54) - pos_b[1]) +
                       (*(float *)(unit + 0x58) - pos_b[2]) *
                         (*(float *)(unit + 0x58) - pos_b[2]));

      distance2 = sqrtf((*(float *)(unit + 0x50) - pos_a[0]) *
                          (*(float *)(unit + 0x50) - pos_a[0]) +
                        (*(float *)(unit + 0x54) - pos_a[1]) *
                          (*(float *)(unit + 0x54) - pos_a[1]) +
                        (*(float *)(unit + 0x58) - pos_a[2]) *
                          (*(float *)(unit + 0x58) - pos_a[2]));

      if (distance2 < distance)
        distance = distance2;

      if (distance >= *(float *)0x2533c8)
        continue;

      if (((*seat_element & 0x200) == 0 ||
           *(int *)(target_unit + 0x2d4) != -1) &&
          *(char *)(seat_element + 1) != 0 &&
          unit_try_animation_state(unit_handle, (int)(seat_element + 1), 0,
                                   0)) {
        int blocking_unit = -1;
        uint16_t seat_state = 0;

        if (!unit_find_nearby_seat(unit_handle, target_unit_handle,
                                   (int16_t)seat_index, &blocking_unit)) {
          if (blocking_unit != -1) {
            char *blocking_obj =
              (char *)object_get_and_verify_type(blocking_unit, 3);
            if (*(int *)(blocking_obj + 0x1a4) != -1 &&
                ai_handle_unit_approach(*(int *)(blocking_obj + 0x1a4),
                                        unit_handle, 0)) {
              seat_state = 1;
            }
          }
        } else {
          seat_state = 2;
        }

        if (seat_state != 0) {
          float threshold = *(float *)0x2533c8;
          if (found_flag && ((*seat_element >> 2) & 1) == 0)
            threshold = *(float *)0x2533ec;

          if (best_seat == -1 || best_state < seat_state ||
              distance * threshold < best_distance) {
            best_distance = distance;
            best_state = seat_state;
            best_seat = seat_index;
            found_flag = ((*seat_element >> 2) & 1);
          }
        }
      }
    }
  }

  if (out_seat_index == NULL) {
    display_assert("parent_seat_index != NULL",
                   "c:\\halo\\SOURCE\\units\\units.c", 0xff8, 1);
    system_exit(-1);
  }

  *out_seat_index = (int16_t)best_seat;
  return best_state;
}

/* unit_clip_to_aiming_bounds (0x1ada90)
 *
 * Clamps a unit's aiming/looking vector to the unit's constraint bounds.
 * flag=1 selects primary constraints (offset 0x266/0x268),
 * flag=0 selects secondary constraints (offset 0x267/0x278).
 * Returns 1 if the vector was clamped, 0 if it was already within bounds.
 * Builds a local coordinate frame from the unit's forward/up node vectors,
 * transforms the vector to local yaw/pitch angles, clamps, and transforms back.
 */
char unit_clip_to_aiming_bounds(int unit_handle, float *vector, char flag)
{
  /* Original stack: SUB ESP,0x4c (76 bytes). Matrix is the primary local —
     forward/up/left/gravity are written directly into its rows, not copied. */
  float matrix[13];
  float relative_vector[3];
  float angles[2];
  char clamped;
  char *unit;
  char enabled;
  float *bounds;
  float *pos;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  clamped = 0;

  if (flag) {
    enabled = *(char *)(unit + 0x266);
    bounds = (float *)(unit + 0x268);
  } else {
    enabled = *(char *)(unit + 0x267);
    bounds = (float *)(unit + 0x278);
  }

  if (!(char)valid_real_normal3d(vector)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "vector", (double)vector[0], (double)vector[1],
                            (double)vector[2]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x15e8, 1);
    system_exit(-1);
  }

  if (!enabled)
    return clamped;

  matrix[0] = 1.0f;
  object_get_orientation(unit_handle, &matrix[1], &matrix[7]);

  /* left = cross(forward, up), written directly into matrix[4..6] */
  matrix[4] = matrix[3] * matrix[8] - matrix[9] * matrix[2];
  matrix[5] = matrix[9] * matrix[1] - matrix[3] * matrix[7];
  matrix[6] = matrix[2] * matrix[7] - matrix[8] * matrix[1];

  pos = *(float **)0x31fc1c;
  matrix[10] = pos[0];
  matrix[11] = pos[1];
  matrix[12] = pos[2];
  real_matrix4x3_transform_point(matrix, vector, relative_vector);

  if (!(char)real_vector3d_valid(relative_vector)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_vector2d(%f, %f, %f)",
                            "&relative_vector", (double)relative_vector[0],
                            (double)relative_vector[1],
                            (double)relative_vector[2]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x15fa, 1);
    system_exit(-1);
  }

  vector_to_angles(angles, relative_vector);

  if ((*(uint32_t *)&angles[1] & 0x7f800000) == 0x7f800000) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real(0x%08X %f)",
                            "relative_aiming_angles.pitch",
                            *(uint32_t *)&angles[1], (double)angles[1]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x15fd, 1);
    system_exit(-1);
  }

  if ((*(uint32_t *)&angles[0] & 0x7f800000) == 0x7f800000) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real(0x%08X %f)",
                            "relative_aiming_angles.yaw",
                            *(uint32_t *)&angles[0], (double)angles[0]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x15fe, 1);
    system_exit(-1);
  }

  /* clamp yaw to bounds */
  if (angles[0] < bounds[0]) {
    angles[0] = bounds[0];
    clamped = 1;
  } else if (angles[0] > bounds[1]) {
    angles[0] = bounds[1];
    clamped = 1;
  }

  /* clamp pitch to bounds */
  if (angles[1] < bounds[2]) {
    angles[1] = bounds[2];
    clamped = 1;
  } else if (angles[1] > bounds[3]) {
    angles[1] = bounds[3];
    clamped = 1;
  } else if (!clamped) {
    return 0;
  }

  if ((*(uint32_t *)&angles[1] & 0x7f800000) == 0x7f800000) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real(0x%08X %f)",
                            "relative_aiming_angles.pitch",
                            *(uint32_t *)&angles[1], (double)angles[1]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x161d, 1);
    system_exit(-1);
  }

  if ((*(uint32_t *)&angles[0] & 0x7f800000) == 0x7f800000) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real(0x%08X %f)",
                            "relative_aiming_angles.yaw",
                            *(uint32_t *)&angles[0], (double)angles[0]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x161e, 1);
    system_exit(-1);
  }

  angles_to_vector(relative_vector, angles);

  if (!(char)real_vector3d_valid(relative_vector)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_vector2d(%f, %f, %f)",
                            "&relative_vector", (double)relative_vector[0],
                            (double)relative_vector[1],
                            (double)relative_vector[2]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1621, 1);
    system_exit(-1);
  }

  matrix_transform_vector(matrix, relative_vector, vector);

  if (!(char)real_vector3d_valid(vector)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_vector2d(%f, %f, %f)",
                            "vector", (double)vector[0], (double)vector[1],
                            (double)vector[2]),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1623, 1);
    system_exit(-1);
  }

  return clamped;
}

/* unit_get_weapon (0x1adeb0)
 *
 * Returns the weapon datum handle stored in the unit's weapon slot array
 * (unit_data_t.unk_680, offset 0x2A8) at the given weapon_index. If
 * weapon_index is NONE (-1), returns NONE. Asserts that the index is in
 * range [0, MAXIMUM_WEAPONS_PER_UNIT=4). Resolves the unit via
 * object_get_and_verify_type with type mask 3 (biped | vehicle).
 */
int unit_get_weapon(int unit_handle, int16_t weapon_index)
{
  unit_data_t *unit;
  int result;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  result = -1;
  if (weapon_index != -1) {
    if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
      display_assert("index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
      system_exit(-1);
    }
    result = unit->unk_680[weapon_index].value;
  }
  return result;
}

/* unit_set_actively_controlled (0x1adf10)
 *
 * Reattaches all weapons in the unit's weapon slots (unk_680, offset 0x2A8,
 * 4 entries) and updates the unit's alive/active flags (unk_436, offset 0x1B4).
 *
 * If the unit has an actor (offset 0x1A4), swarm actor (0x1A8), or unk_456
 * (0x1C8), param_2 is forced to 1 (the unit is always considered active).
 *
 * When the unit is active and object flag bit 2 at offset 0xB6 is clear:
 *   - Sets bit 0 (alive) and bit 6 in unk_436
 * Otherwise:
 *   - Clears bit 0 and bit 6 in unk_436
 *
 * Then iterates all 4 weapon slots and calls item_attach_to_unit for each
 * valid weapon handle, and finally tail-calls unit_update_seat_occupancy.
 */
void unit_set_actively_controlled(int unit_handle, char param_2)
{
  char *unit;
  uint32_t flags;
  int *weapon_slots;
  int i;

  unit = object_get_and_verify_type(unit_handle, 3);

  /* Force active if the unit has an actor, swarm actor, or unk_456 */
  if (*(int *)(unit + 0x1a4) != -1 || *(int *)(unit + 0x1a8) != -1 ||
      *(int *)(unit + 0x1c8) != -1) {
    param_2 = 1;
  }

  if ((*(uint8_t *)(unit + 0xb6) & 4) == 0 && param_2 != 0) {
    flags = *(uint32_t *)(unit + 0x1b4);
    *(uint32_t *)(unit + 0x1b4) = flags | 1;
    flags = flags | 0x41;
  } else {
    flags = *(uint32_t *)(unit + 0x1b4);
    *(uint32_t *)(unit + 0x1b4) = flags & 0xfffffffe;
    flags = flags & 0xffffffbe;
  }
  *(uint32_t *)(unit + 0x1b4) = flags;

  /* Reattach all weapons in the unit's weapon slots */
  weapon_slots = (int *)(unit + 0x2a8);
  for (i = 4; i != 0; i--) {
    if (*weapon_slots != -1) {
      item_attach_to_unit(*weapon_slots, unit_handle);
    }
    weapon_slots++;
  }

  unit_update_seat_occupancy(unit_handle);
}

/* unit_current_weapon_is_busy (0x1ae1a0)
 *
 * Gets the weapon handle in the unit's current seat (via unit_get_weapon
 * with the seat's weapon_index at offset 0x2A2), then checks whether the
 * weapon object is in state 2 or 3 at offset 0x211. Returns true if the
 * weapon is in one of those states, false otherwise (including when there
 * is no current weapon).
 */
bool unit_current_weapon_is_busy(int unit_handle)
{
  char *unit;
  int weapon_handle;
  char *weapon;

  unit = object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle, *(int16_t *)(unit + 0x2a2));
  if (weapon_handle == -1)
    return false;

  weapon = object_get_and_verify_type(weapon_handle, 4);
  if (*(char *)(weapon + 0x211) == 2 || *(char *)(weapon + 0x211) == 3)
    return true;

  return false;
}

/* unit_get_seat_label (0x1ae290)
 *
 * Returns the seat label string for a unit. If the unit has a parent object
 * (offset 0xCC != NONE) and a valid seat tag index (offset 0x2A0 != -1),
 * retrieves the label from the parent's unit tag seat block at offset +4
 * within the 0x11C-sized seat element. Otherwise falls back to a base seat
 * label from the global table at 0x32e484 indexed by base_seat_index
 * (offset 0x257), asserting it is in range [0, 6).
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: parent_object_index at [ESI + 0xCC].
 * Confirmed: seat tag index at [ESI + 0x2A0].
 * Confirmed: tag_get(0x756e6974, ...) for 'unit' tag.
 * Confirmed: tag_block_get_element(tag+0x2e4, seat_index, 0x11c).
 * Confirmed: assert "base_seat_index>=0 &&
 * base_seat_index<NUMBER_OF_UNIT_BASE_SEATS" at line 0x200f. Confirmed: global
 * table at 0x32e484, indexed by MOVSX of byte at +0x257.
 */
int unit_get_seat_label(int unit_handle)
{
  char *unit;
  int parent_handle;
  int16_t seat_tag_index;
  int *parent_obj;
  char *unit_tag;
  char *seat_element;
  int16_t base_seat;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  parent_handle = *(int *)(unit + 0xcc);
  if (parent_handle != -1 && *(int16_t *)(unit + 0x2a0) != -1) {
    parent_obj = (int *)object_get_and_verify_type(parent_handle, 3);
    unit_tag = (char *)tag_get(0x756e6974, *parent_obj);
    seat_tag_index = *(int16_t *)(unit + 0x2a0);
    seat_element = (char *)tag_block_get_element(unit_tag + 0x2e4,
                                                 (int)seat_tag_index, 0x11c);
    return (int)(seat_element + 4);
  }

  base_seat = (int16_t) * (int8_t *)(unit + 0x257);
  if (base_seat < 0 || base_seat >= NUMBER_OF_UNIT_BASE_SEATS) {
    display_assert(
      "base_seat_index>=0 && base_seat_index<NUMBER_OF_UNIT_BASE_SEATS",
      "c:\\halo\\SOURCE\\units\\units.c", 0x200f, 1);
    system_exit(-1);
  }
  return *(int *)(0x32e484 + base_seat * 4);
}

/* unit_clear_seat_equipment (0x1ae330)
 *
 * Clears the unit's seat equipment handle at offset 0x2C8
 * (unit_data_t.unk_712). If the current value is not NONE (-1), calls the
 * dual-register function at 0x1ab990 (EDI=unit_handle, ESI=equipment_handle)
 * to detach/remove the equipment, then sets the field to NONE.
 */
void unit_clear_seat_equipment(int unit_handle)
{
  unit_data_t *unit;
  int equipment_handle;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  equipment_handle = unit->unk_712.value;
  if (equipment_handle != -1) {
    unit_detach_weapon(unit_handle, equipment_handle);
    unit->unk_712.value = -1;
  }
}

/* unit_can_enter_seat (0x1ae370)
 *
 * Checks whether a unit can enter a given seat object. Verifies both object
 * types (unit=3, seat=4), retrieves the unit's seat label string via 0x1ae290
 * and the seat object's weapon label string via 0xfae80, then calls 0x1acd70
 * to attempt seat matching. If the match succeeds, dispatches through the game
 * engine vtable (current_game_engine->vtable[0x58]) for engine-specific
 * validation.
 *
 * Returns: true if the seat can be entered, false otherwise.
 */
bool unit_can_enter_seat(int unit_handle, int seat_object_handle)
{
  int seat_label;
  int weapon_label;
  char can_enter;

  object_get_and_verify_type(unit_handle, 3);
  object_get_and_verify_type(seat_object_handle, 4);

  seat_label = unit_get_seat_label(unit_handle);
  weapon_label = (int)weapon_get_label(seat_object_handle);
  can_enter =
    (char)unit_try_animation_state(unit_handle, seat_label, weapon_label, 0);

  if (can_enter != 0) {
    /* 0xa8b30: game engine vtable dispatch */
    can_enter =
      (char)game_engine_allow_weapon_pick_up(unit_handle, seat_object_handle);
  }
  return can_enter != 0;
}

/* unit_should_swap_weapon (0x1ae3c0)
 *
 * Checks whether a unit should swap one of its current weapons for the given
 * weapon object. Iterates the unit's 4 weapon slots (offset 0x2A8). If any
 * non-current slot already holds the same weapon tag, returns false. If the
 * current slot holds the same weapon tag, returns true only when the current
 * weapon's ammo is above the threshold at 0x2533c0 AND the new weapon has
 * equal or greater ammo. Otherwise returns true (new weapon type is not
 * already held).
 */
bool unit_should_swap_weapon(int unit_handle, int weapon_handle)
{
  char *unit;
  int *weapon_obj;
  int weapon_tag;
  int current_weapon;
  int seat_index;
  int *weapon_slot;
  bool should_swap;

  unit = object_get_and_verify_type(unit_handle, 3);
  weapon_obj = (int *)object_get_and_verify_type(weapon_handle, 4);
  weapon_tag = *weapon_obj;

  tag_get(0x77656170, weapon_tag);

  current_weapon = unit_get_weapon(unit_handle, *(int16_t *)(unit + 0x2a2));
  if (current_weapon == -1)
    return false;

  should_swap = true;
  seat_index = 0;
  weapon_slot = (int *)(unit + 0x2a8);

  do {
    int slot_weapon = *weapon_slot;
    if (slot_weapon != -1) {
      int *slot_weapon_obj = (int *)object_get_and_verify_type(slot_weapon, 4);
      if (weapon_tag == *slot_weapon_obj) {
        if (seat_index == *(int16_t *)(unit + 0x2a2)) {
          float slot_ammo = *(float *)((char *)slot_weapon_obj + 0x1f0);
          if (slot_ammo > *(float *)0x2533c0) {
            float new_ammo = *(float *)((char *)weapon_obj + 0x1f0);
            if (new_ammo < slot_ammo)
              goto next_seat;
          }
        }
        should_swap = false;
      }
    }
  next_seat:
    seat_index++;
    weapon_slot++;
  } while (seat_index < 4);

  return should_swap;
}

/* unit_next_weapon_index (0x1ae490)
 *
 * Scans the unit's weapon slots circularly in the given direction to find the
 * next valid/usable weapon. Uses unit->unk_680[] (offset 0x2A8) as the weapon
 * handle array and unit->unk_696[] (offset 0x2B8) as a priority/ordering array.
 *
 * If weapon_index is NONE (-1), starts scanning from slot 0. If direction is 0,
 * picks the weapon with the lowest priority value; if direction is nonzero,
 * picks the first valid weapon found. Returns the index of the best weapon
 * found, or NONE (-1) if no valid weapon exists.
 *
 * Callees (by address, not in kb.json):
 *   0x1ae290 — get unit animation tag pointer (EAX=unit_handle@<eax>)
 *   0xfae80  — get weapon tag info pointer (1 stack arg: weapon_handle)
 *   0x1acd70 — check unit can use weapon (EAX=unit_handle@<eax>, 3 stack args)
 *   0xa8b30  — weapon usability callback (2 stack args)
 *   0xfb090  — weapon has must-be-readied flag (1 stack arg)
 */
int16_t unit_next_weapon_index(int unit_handle, int16_t weapon_index,
                               int16_t direction)
{
  unit_data_t *unit;
  int current_index;
  int best_index;
  int iter_index;
  int weapon_handle;
  int anim_tag;
  int weapon_tag;
  char can_use;
  char usable;
  char must_be_readied;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  best_index = -1;

  if (weapon_index == (int16_t)-1) {
    weapon_index = 0;
  } else if (weapon_index < 0 || weapon_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert("current_index>=0 && current_index<MAXIMUM_WEAPONS_PER_UNIT",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1e40, 1);
    system_exit(-1);
  }

  current_index = weapon_index;

  do {
    iter_index = (int)(int16_t)current_index;
    weapon_handle = unit->unk_680[iter_index].value;

    if (weapon_handle != -1) {
      /* Validate both the unit and weapon objects */
      object_get_and_verify_type(unit_handle, 3);
      object_get_and_verify_type(weapon_handle, 4);

      anim_tag = unit_get_seat_label(unit_handle);
      weapon_tag = (int)weapon_get_label(weapon_handle);
      can_use =
        (char)unit_try_animation_state(unit_handle, anim_tag, weapon_tag, 0);

      if (can_use != 0) {
        /* 0xa8b30: weapon usability callback */
        usable =
          (char)game_engine_allow_weapon_pick_up(unit_handle, weapon_handle);
        if (usable != 0) {
          /* direction != 0: pick first valid; direction == 0: pick lowest
           * priority */
          if (direction != 0) {
            best_index = current_index;
          } else {
            if ((int16_t)best_index == (int16_t)-1 ||
                unit->unk_696[(int)(int16_t)best_index].value <
                  unit->unk_696[iter_index].value) {
              best_index = current_index;
            }
          }

          /* 0xfb090: check weapon must-be-readied flag */
          must_be_readied =
            (char)((int (*)(int))0xfb090)(unit->unk_680[iter_index].value);
          if (must_be_readied != 0)
            return (int16_t)best_index;

          if ((int16_t)current_index != weapon_index)
            return (int16_t)best_index;
        }
      }
    }

    /* Advance to next slot, wrapping around */
    if (direction < 0) {
      if ((int16_t)current_index == 0)
        current_index = 3;
      else
        current_index = iter_index - 1;
    } else {
      if ((int16_t)current_index == 3)
        current_index = 0;
      else
        current_index = iter_index + 1;
    }
  } while ((int16_t)current_index != weapon_index);

  return (int16_t)best_index;
}

/* unit_set_in_vehicle (0x1ae600)
 *
 * Attempts to stow/put the unit's current weapon into a vehicle slot.
 * Returns true (1) if the weapon was successfully placed, false (0) otherwise.
 *
 * Steps:
 * 1. Gets the unit tag definition via tag_get("unit", unit->tag_index).
 * 2. Looks up the current weapon handle via unit_get_weapon.
 * 3. Calls FUN_001ae490 to compute the next weapon index.
 * 4. Skips if weapon is NONE, or if next index equals current and flag is
 * false.
 * 5. Checks the weapon object's flags byte (bit 0 must be clear).
 * 6. Calls FUN_000fd360(weapon_handle, flag) to attempt the placement.
 * 7. On success: fires unit event 0xd, calls FUN_001ab990, clears the weapon
 *    slot, resets current/next weapon indices, and optionally deletes the
 *    weapon object if weapon_can_be_fired returns false.
 */
bool unit_set_in_vehicle(int unit_handle, bool flag)
{
  unit_data_t *unit;
  int weapon_handle;
  int16_t new_index;
  object_data_t *weapon_obj;
  int16_t cur_index;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  tag_get(0x756e6974, *(int *)unit);
  (void)object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle, unit->unk_674);
  new_index = unit_next_weapon_index(unit_handle, unit->unk_674, 1);

  if (weapon_handle == -1)
    return false;
  if (new_index == unit->unk_674 && !flag)
    return false;

  weapon_obj = (object_data_t *)object_get_and_verify_type(weapon_handle, -1);
  if (weapon_obj->flags & 1)
    return false;

  if (!((bool (*)(int, bool))0xfd360)(weapon_handle, flag))
    return false;

  ((void (*)(int, int))0xde360)(unit_handle, 0xd);

  unit_detach_weapon(unit_handle, weapon_handle);

  cur_index = (int16_t)unit->unk_674;
  unit->unk_680[cur_index].value = -1;
  unit->unk_674 = (uint16_t)-1;
  new_index = unit_next_weapon_index(unit_handle, -1, 0);
  unit->unk_676 = (uint16_t)new_index;

  if (!((bool (*)(int))0xfaf50)(weapon_handle))
    object_delete(weapon_handle);

  return true;
}

/* unit_apply_alignment_vector (0x1af180)
 *
 * Sets the unit object's facing direction (forward and up vectors) from a
 * 2D alignment vector (x, y) representing a direction in the ground plane,
 * but only if the unit has no parent (is not seated/mounted).
 *
 * Steps:
 *   1. Resolves unit via object_get_and_verify_type (type_mask=3, @<eax>).
 *   2. If parent_object_index != -1 (unit is mounted), returns immediately.
 *   3. Asserts alignment_vector is a valid 2D normal via FUN_00028610.
 *   4. Copies alignment_vector[0] -> unit+0x24 (object forward x),
 *      alignment_vector[1] -> unit+0x28 (object forward y),
 *      0.0f               -> unit+0x2c (object forward z).
 *   5. Loads the canonical "up" vector from the pointer at 0x31fc44 and
 *      writes it to unit+0x30, +0x34, +0x38.
 *   6. Asserts the forward/up pair are valid axes via FUN_00084a70.
 *
 * Register args: unit_handle @<eax>, alignment_vector @<ecx>.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type (0x13d680).
 * Confirmed: MOV EBX,ECX at entry; CMP [ESI+0xcc],-1 -> parent gate.
 * Confirmed: CALL 0x28610 (valid_real_normal2d check on EBX=alignment_vector).
 * Confirmed: MOV ECX,[EBX]; FSTP [ESI+0x28]; MOV [ESI+0x24],ECX; MOV
 * [ESI+0x2c],0. Confirmed: MOV EDX,[0x31fc44]; copies 3 floats to
 * [ESI+0x30..0x38]. Confirmed: assert string "alignment_vector" at 0x2b7234.
 * Confirmed: CALL 0x84a70 (valid_real_vector3d_axes2 check).
 */
void unit_apply_alignment_vector(int unit_handle, float *alignment_vector)
{
  unit_data_t *unit;
  float *up_vector;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* Only apply if the unit is a top-level object (no parent). */
  if (unit->object.parent_object_index.value != -1)
    return;

  /* Assert the 2D alignment vector is a valid normal (valid_real_normal2d). */
  if (!valid_real_normal2d(alignment_vector)) {
    display_assert("assert_valid_real_normal2d(alignment_vector)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2482, 1);
    system_exit(-1);
  }

  /* Copy 2D alignment direction into object forward vector (zero z).
   * Confirmed: MOV ECX,[EBX]; FSTP [ESI+0x28]; MOV [ESI+0x24],ECX; MOV
   * [ESI+0x2c],0. Note: store order in binary is y first (FSTP [ESI+0x28]) then
   * x (MOV [ESI+0x24]). Both reads from [EBX] are sourced before any store, so
   * no aliasing concern. */
  *(float *)((char *)unit + 0x24) = alignment_vector[0];
  *(float *)((char *)unit + 0x28) = alignment_vector[1];
  *(float *)((char *)unit + 0x2c) = 0.0f;

  /* Copy the canonical up vector (world up) from the global at 0x31fc44.
   * Confirmed: MOV EDX,[0x31fc44]; copies 3 dwords to [ESI+0x30,+0x34,+0x38].
   */
  up_vector = *(float **)0x31fc44;
  *(float *)((char *)unit + 0x30) = up_vector[0];
  *(float *)((char *)unit + 0x34) = up_vector[1];
  *(float *)((char *)unit + 0x38) = up_vector[2];

  /* Assert forward/up are valid orthogonal axes
   * (valid_real_normal3d_perpendicular). */
  if (!valid_real_normal3d_perpendicular((float *)((char *)unit + 0x24),
                                         (float *)((char *)unit + 0x30))) {
    display_assert("assert_valid_real_vector3d_axes2(forward, up)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2486, 1);
    system_exit(-1);
  }
}

/* unit_verify_vectors (0x1af620)
 *
 * Validates 6 directional vectors stored on a unit object:
 *   - unk_468 (offset 0x1D4) — facing vector
 *   - unk_480 (offset 0x1E0) — aiming vector
 *   - unk_516 (offset 0x204) — looking vector
 *   - unk_36/unk_48 (offsets 0x24/0x30) — forward/up from object_data_t
 *   - unk_492 (offset 0x1EC) — additional vector
 *   - unk_528 (offset 0x210) — additional vector
 *
 * Each vector is checked via valid_real_normal3d; additionally the
 * forward/up pair at 0x24/0x30 is checked for perpendicularity via
 * valid_real_normal3d_perpendicular. Returns true only if all checks pass.
 *
 * Register arg: unit_handle passed in EAX.
 *
 * Confirmed: PUSH 0x3 / PUSH EAX -> object_get_and_verify_type.
 * Confirmed: LEA offsets 0x1d4, 0x1e0, 0x204, 0x24, 0x30, 0x1ec, 0x210.
 * Confirmed: CALL 0x21fb0 (valid_real_normal3d) and 0x84a70 (perpendicular
 * check). Confirmed: returns bool (MOV AL,1 / XOR AL,AL).
 */
bool unit_verify_vectors(int unit_handle)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(unit_handle, 3);

  if (!valid_real_normal3d((float *)(obj + 0x1d4)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x1e0)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x204)))
    return false;
  if (!valid_real_normal3d_perpendicular((float *)(obj + 0x24),
                                         (float *)(obj + 0x30)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x1ec)))
    return false;
  if (!valid_real_normal3d((float *)(obj + 0x210)))
    return false;

  return true;
}

/* unit_control_trace (0x1af6b0)
 *
 * Validates the unit's internal vectors (facing, aiming, looking, forward, up)
 * via an internal verify function at 0x1af620. If any vector is invalid, dumps
 * the unit's position, orientation, facing/aiming/looking vectors both as
 * float and as raw hex to the error log, then retries verification. If it
 * still fails, fires a fatal assert ("unit_verify_vectors FAILURE").
 *
 * Register arg: unit_handle passed in EDI.
 * Stack arg: label string (e.g. "unit-control") used in the error header.
 *
 * The verify function at 0x1af620 takes unit_handle in EAX, calls
 * assert_valid_real_normal3d on each direction vector and returns true
 * if all are valid.
 *
 * The function at 0x49ac0 builds a textual location string for the unit
 * (using the actor or swarm actor index) into a caller-provided buffer.
 */
void unit_control_trace(int unit_handle, const char *label)
{
  unit_data_t *unit;
  int32_t actor;
  char location_buf[512];

  /* Verify vectors — returns true if all vectors are valid normals. */
  if (unit_verify_vectors(unit_handle))
    return;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* 0x1af6d3: choose actor_index; fall back to swarm_actor_index */
  actor = unit->actor_index.value;
  if (actor == -1)
    actor = unit->swarm_actor_index.value;

  /* 0x1af6f7: build location string into stack buffer */
  ((void (*)(int, int, int, char *, int))0x49ac0)(actor, unit_handle, 1,
                                                  location_buf, 512);

  /* header: "unit_verify_vectors: problems with %s at location %s" */
  error(2, "**** unit_verify_vectors: problems with %s at location %s",
        location_buf, label);

  /* dump object position, forward, up as floats */
  error(2, "  object: pos %f %f %f, fwd %f %f %f, up %f %f %f",
        (double)unit->object.unk_12.x, (double)unit->object.unk_12.y,
        (double)unit->object.unk_12.z, (double)unit->object.unk_36.x,
        (double)unit->object.unk_36.y, (double)unit->object.unk_36.z,
        (double)unit->object.unk_48.x, (double)unit->object.unk_48.y,
        (double)unit->object.unk_48.z);

  /* dump desired facing, aiming, looking as floats */
  error(
    2, "  desired facing %f %f %f, aiming %f %f %f, looking %f %f %f",
    (double)unit->unk_468.x, (double)unit->unk_468.y, (double)unit->unk_468.z,
    (double)unit->unk_480.x, (double)unit->unk_480.y, (double)unit->unk_480.z,
    (double)unit->unk_516.x, (double)unit->unk_516.y, (double)unit->unk_516.z);

  /* dump aiming vector + velocity as floats */
  error(2, "  aiming vector %f %f %f velocity %f %f %f",
        (double)unit->unk_492.x, (double)unit->unk_492.y,
        (double)unit->unk_492.z, (double)unit->unk_504.x,
        (double)unit->unk_504.y, (double)unit->unk_504.z);

  /* dump looking vector + velocity as floats */
  error(2, "  looking vector %f %f %f velocity %f %f %f",
        (double)unit->unk_528.x, (double)unit->unk_528.y,
        (double)unit->unk_528.z, (double)unit->unk_540.x,
        (double)unit->unk_540.y, (double)unit->unk_540.z);

  error(2, "  warning, hex dump follows...");

  /* dump object position, forward, up as hex (raw dword reinterpret) */
  error(
    2, "  object: pos %08X %08X %08X, fwd %08X %08X %08X, up %08X %08X %08X",
    *(uint32_t *)&unit->object.unk_12.x, *(uint32_t *)&unit->object.unk_12.y,
    *(uint32_t *)&unit->object.unk_12.z, *(uint32_t *)&unit->object.unk_36.x,
    *(uint32_t *)&unit->object.unk_36.y, *(uint32_t *)&unit->object.unk_36.z,
    *(uint32_t *)&unit->object.unk_48.x, *(uint32_t *)&unit->object.unk_48.y,
    *(uint32_t *)&unit->object.unk_48.z);

  /* dump desired facing, aiming, looking as hex */
  error(2,
        "  desired facing %08X %08X %08X, aiming %08X %08X %08X, looking %08X "
        "%08X %08X",
        *(uint32_t *)&unit->unk_468.x, *(uint32_t *)&unit->unk_468.y,
        *(uint32_t *)&unit->unk_468.z, *(uint32_t *)&unit->unk_480.x,
        *(uint32_t *)&unit->unk_480.y, *(uint32_t *)&unit->unk_480.z,
        *(uint32_t *)&unit->unk_516.x, *(uint32_t *)&unit->unk_516.y,
        *(uint32_t *)&unit->unk_516.z);

  /* dump aiming vector + velocity as hex */
  error(2, "  aiming vector %08X %08X %08X velocity %08X %08X %08X",
        *(uint32_t *)&unit->unk_492.x, *(uint32_t *)&unit->unk_492.y,
        *(uint32_t *)&unit->unk_492.z, *(uint32_t *)&unit->unk_504.x,
        *(uint32_t *)&unit->unk_504.y, *(uint32_t *)&unit->unk_504.z);

  /* dump looking vector + velocity as hex */
  error(2, "  looking vector %08X %08X %08X velocity %08X %08X %08X",
        *(uint32_t *)&unit->unk_528.x, *(uint32_t *)&unit->unk_528.y,
        *(uint32_t *)&unit->unk_528.z, *(uint32_t *)&unit->unk_540.x,
        *(uint32_t *)&unit->unk_540.y, *(uint32_t *)&unit->unk_540.z);

  /* retry verification */
  if (unit_verify_vectors(unit_handle))
    return;

  /* fatal assert if vectors are still broken */
  display_assert("unit_verify_vectors FAILURE, see above for details",
                 "c:\\halo\\SOURCE\\units\\units.c", 0x252, true);
  system_exit(-1);
}

/* unit_set_control (0x1af990)
 *
 * Validates and applies a unit_control block to the given unit. The control
 * data (param_2) is a 0x40-byte struct containing animation state, aiming
 * speed, control flags, weapon/grenade/zoom indices, throttle vector,
 * primary trigger, facing/aiming/looking vectors.
 *
 * Validation asserts (with original line numbers):
 *   - throttle magnitude <= 3.0f (line 0x5e1)
 *   - animation_state in [0,7) (line 0x5e2)
 *   - aiming_speed in [0,2) (line 0x5e3)
 *   - control_flags valid (bit 7 clear) (line 0x5e4)
 *   - facing/aiming/looking vectors are valid normals (lines 0x5e5-0x5e7)
 *   - weapon_index NONE or in [0,4) (line 0x5e8)
 *   - grenade_index NONE or in [0,2) (line 0x5e9)
 *   - zoom_level NONE or >= 0 (line 0x5ea)
 *   - primary_trigger is not NaN/Inf (line 0x5eb)
 *
 * After validation, copies fields from control_data into unit_data_t at
 * offsets documented in the store-offset table below.
 *
 * Store-offset table (unit <- control_data):
 *   +0x228 <- +0x0C  throttle (vector3)
 *   +0x234 <- +0x18  primary_trigger (float)
 *   +0x238 <- +0x01  aiming_speed (byte)
 *   +0x2A4 <- +0x04  weapon_index (word, if not NONE)
 *   +0x2CD <- +0x06  grenade_index (byte, if not NONE)
 *   +0x2D1 <- +0x08  zoom_level (byte)
 *   +0x1B8 <- +0x02  control_flags (zext word -> dword)
 *   +0x204 <- +0x34  looking_vector (vector3)
 *   +0x1E0 <- +0x28  aiming_vector (vector3)
 *   +0x1D4 <- +0x1C  facing_vector (vector3)
 *   +0x256 <- +0x00  animation_state (byte)
 */
void unit_set_control(int unit_handle, void *unit_control)
{
  unit_data_t *unit;
  char *cd;
  float mag;
  float *looking;

  cd = (char *)unit_control;
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* validate throttle magnitude <= 3.0f */
  mag = sqrtf(*(float *)(cd + 0x14) * *(float *)(cd + 0x14) +
              *(float *)(cd + 0x10) * *(float *)(cd + 0x10) +
              *(float *)(cd + 0x0c) * *(float *)(cd + 0x0c));
  if (!(mag <= *(float *)0x254644)) {
    display_assert("magnitude3d(&control_data->throttle)<=3.0f",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e1, 1);
    system_exit(-1);
  }

  /* validate animation_state in [0, 7) */
  if (cd[0] < 0 || cd[0] >= 7) {
    display_assert("control_data->animation_state>=0 && control_data->"
                   "animation_state<NUMBER_OF_UNIT_ANIMATION_STATES",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e2, 1);
    system_exit(-1);
  }

  /* validate aiming_speed in [0, 2) */
  if (cd[1] < 0 || cd[1] >= 2) {
    display_assert("control_data->aiming_speed>=0 && control_data->"
                   "aiming_speed<NUMBER_OF_UNIT_AIMING_SPEEDS",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e3, 1);
    system_exit(-1);
  }

  /* validate control_flags (bit 7 must be clear) */
  if (cd[3] & 0x80) {
    display_assert("VALID_FLAGS(control_data->control_flags, "
                   "NUMBER_OF_UNIT_CONTROL_FLAGS)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e4, 1);
    system_exit(-1);
  }

  /* validate facing_vector is a valid normal */
  if (!((bool (*)(float *))0x21fb0)((float *)(cd + 0x1c))) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&control_data->facing_vector", (double)*(float *)(cd + 0x1c),
               (double)*(float *)(cd + 0x20), (double)*(float *)(cd + 0x24)),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5e5, 1);
    system_exit(-1);
  }

  /* validate aiming_vector is a valid normal */
  if (!((bool (*)(float *))0x21fb0)((float *)(cd + 0x28))) {
    display_assert(
      csprintf(error_string_buffer,
               "%s: assert_valid_real_normal3d(%f, %f, %f)",
               "&control_data->aiming_vector", (double)*(float *)(cd + 0x28),
               (double)*(float *)(cd + 0x2c), (double)*(float *)(cd + 0x30)),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5e6, 1);
    system_exit(-1);
  }

  /* validate looking_vector is a valid normal */
  looking = (float *)(cd + 0x34);
  if (!((bool (*)(float *))0x21fb0)(looking)) {
    display_assert(csprintf(error_string_buffer,
                            "%s: assert_valid_real_normal3d(%f, %f, %f)",
                            "&control_data->looking_vector", (double)looking[0],
                            (double)*(float *)(cd + 0x38),
                            (double)*(float *)(cd + 0x3c)),
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e7, 1);
    system_exit(-1);
  }

  /* validate weapon_index: NONE or in [0, MAXIMUM_WEAPONS_PER_UNIT) */
  if (*(int16_t *)(cd + 4) != -1 &&
      (*(int16_t *)(cd + 4) < 0 ||
       *(int16_t *)(cd + 4) >= MAXIMUM_WEAPONS_PER_UNIT)) {
    display_assert("control_data->weapon_index==NONE || (control_data->"
                   "weapon_index>=0 && control_data->"
                   "weapon_index<MAXIMUM_WEAPONS_PER_UNIT)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e8, 1);
    system_exit(-1);
  }

  /* validate grenade_index: NONE or in [0, NUMBER_OF_UNIT_GRENADE_TYPES) */
  if (*(int16_t *)(cd + 6) != -1 &&
      (*(int16_t *)(cd + 6) < 0 ||
       *(int16_t *)(cd + 6) >= NUMBER_OF_UNIT_GRENADE_TYPES)) {
    display_assert("control_data->grenade_index==NONE || (control_data->"
                   "grenade_index>=0 && control_data->"
                   "grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5e9, 1);
    system_exit(-1);
  }

  /* validate zoom_level: NONE or >= 0 */
  if (*(int16_t *)(cd + 8) != -1 && *(int16_t *)(cd + 8) < 0) {
    display_assert("control_data->zoom_level==NONE || (control_data->"
                   "zoom_level>=0)",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x5ea, 1);
    system_exit(-1);
  }

  /* validate primary_trigger is not NaN/Inf */
  if ((*(uint32_t *)(cd + 0x18) & 0x7f800000) == 0x7f800000) {
    display_assert(
      csprintf(error_string_buffer, "%s: assert_valid_real(0x%08X %f)",
               "control_data->primary_trigger", *(uint32_t *)(cd + 0x18),
               (double)*(float *)(cd + 0x18)),
      "c:\\halo\\SOURCE\\units\\units.c", 0x5eb, 1);
    system_exit(-1);
  }

  /* copy throttle vector (control +0x0C -> unit +0x228) */
  unit->unk_552.x = *(float *)(cd + 0x0c);
  unit->unk_552.y = *(float *)(cd + 0x10);
  unit->unk_552.z = *(float *)(cd + 0x14);

  /* copy primary trigger (control +0x18 -> unit +0x234) */
  unit->unk_564 = *(float *)(cd + 0x18);

  /* copy aiming speed (control +0x01 -> unit +0x238) */
  unit->unk_568 = cd[1];

  /* copy weapon index if not NONE (control +0x04 -> unit +0x2A4) */
  if (*(int16_t *)(cd + 4) != -1)
    unit->unk_676 = *(uint16_t *)(cd + 4);

  /* copy grenade index if not NONE (control +0x06 -> unit +0x2CD) */
  if (*(int16_t *)(cd + 6) != -1)
    unit->unk_717 = cd[6];

  /* copy zoom level (control +0x08 -> unit +0x2D1) */
  unit->unk_721 = cd[8];

  /* copy control flags (control +0x02 zext word -> unit +0x1B8 dword) */
  unit->unk_440 = (uint32_t) * (uint16_t *)(cd + 2);

  /* copy looking vector (control +0x34 -> unit +0x204) */
  unit->unk_516.x = looking[0];
  unit->unk_516.y = looking[1];
  unit->unk_516.z = looking[2];

  /* copy aiming vector (control +0x28 -> unit +0x1E0) */
  unit->unk_480.x = *(float *)(cd + 0x28);
  unit->unk_480.y = *(float *)(cd + 0x2c);
  unit->unk_480.z = *(float *)(cd + 0x30);

  /* copy facing vector (control +0x1C -> unit +0x1D4) */
  unit->unk_468.x = *(float *)(cd + 0x1c);
  unit->unk_468.y = *(float *)(cd + 0x20);
  unit->unk_468.z = *(float *)(cd + 0x24);

  /* copy animation state (control +0x00 -> unit +0x256) */
  unit->unk_598 = cd[0];

  /* trace/profile call */
  unit_control_trace(unit_handle, "unit-control");
}

/* unit_reset_weapon_state (0x1b1290)
 *
 * Resets the unit's weapon zoom/ready state. If the unit has an associated
 * player and that player is valid, and the unit's zoom_level is not 0xFF,
 * retrieves the unit's current weapon and plays its zoom-deactivation sound
 * (weapon tag +0x4bc) at scale 1.0. Then clears zoom_level and unk_721 to
 * 0xFF and zeroes unk_760. Finally calls player_clear_aim_assist.
 */
void unit_reset_weapon_state(int unit_handle)
{
  unit_data_t *unit;
  int player_index;
  char *player;
  int weapon_handle;
  weapon_data_t *weapon;
  void *weapon_tag;
  int sound_tag_index;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  player_index = player_index_from_unit_index(unit_handle);
  if (player_index != -1) {
    player =
      (char *)datum_get(player_data, player_index_from_unit_index(unit_handle));
    if (*(short *)(player + 2) != -1 && unit->zoom_level != 0xFF) {
      unit_data_t *unit2 =
        (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
      weapon_handle = unit_get_weapon(unit_handle, unit2->unk_674);
      if (weapon_handle != -1) {
        weapon = (weapon_data_t *)object_get_and_verify_type(weapon_handle, 4);
        weapon_tag = tag_get(0x77656170, weapon->item.object.tag_index);
        sound_tag_index = *(int *)((char *)weapon_tag + 0x4bc);
        if (sound_tag_index != -1) {
          sound_impulse_start(sound_tag_index, 1.0f);
        }
      }
    }
  }
  unit->zoom_level = 0xFF;
  unit->unk_721 = 0xFF;
  unit->unk_760 = 0;
  player_clear_aim_assist(unit_handle);
}

/* unit_apply_animation_impulse (0x1b1a20)
 *
 * Attempts to apply an animation impulse to a unit. The impulse is an index
 * in [0, NUMBER_OF_UNIT_ANIMATION_IMPULSES) that maps to an animation kind
 * index and an update_kind via FUN_001a9560. The function:
 *
 *   1. Checks whether the unit's current animation state allows impulses
 *      (FUN_001a96f0 @<eax>=unit_handle). Returns false immediately if not.
 *   2. Resolves unit tag -> antr tag at unit_tag+0x44. Uses the unit's
 *      current mode (unk_592 at +0x250) and sub-anim (unk_593 at +0x251)
 *      to reach the sub-animation block at mode+0x58 (element size 0xbc).
 *   3. Maps the impulse index to an animation kind index (AX) and an
 *      update_kind (written to *out_update_kind) via FUN_001a9560.
 *   4. Looks up the animation kind index in the sub-anim's kind table at
 *      sub_anim+0x98 (count) / sub_anim+0x9c (int16[] ptr). Returns false
 *      if out of range or the slot is -1.
 *   5. Calls object_set_region_count(unit_handle, update_kind) to set
 *      the interpolation mode.
 *   6. Calls model_animation_choose_random(1, antr_tag_idx, kind_anim_idx)
 *      to choose an animation.
 *   7. Calls unit_set_animation(@<eax>=unit_handle, @<edi>=antr_tag_idx,
 *      @<bx>=chosen_anim).
 *   8. Sets unk_584 (0x248) bit 0 and unk_595 (0x253) = 0x1d.
 *   9. If anim_data is non-NULL, the unit type is 0 (biped), and the unit
 *      has no parent, calls FUN_001af180(@<eax>=unit_handle, @<ecx>=anim_data)
 *      to apply an alignment vector.
 *   10. Returns true on success.
 *
 * Register args: FUN_001a96f0 takes unit_handle @<eax>.
 *                FUN_001a9560 takes impulse_index @<ax>, out_update_kind
 * @<ebx>. FUN_001af180 takes unit_handle @<eax>, anim_data @<ecx>.
 *
 * Confirmed: PUSH EBX (unit_handle) / PUSH 0x3 -> object_get_and_verify_type.
 * Confirmed: MOV EAX,EBX -> CALL 0x1a96f0 (register arg).
 * Confirmed: tag_get(0x756e6974, *unit) then tag_get(0x616e7472,
 * unit_tag+0x44). Confirmed: MOVSX EDX,byte ptr [ESI+0x250] (unk_592 mode
 * index). Confirmed: tag_block_get_element(antr+0xc, unk_592, 0x64). Confirmed:
 * MOVSX ECX,byte ptr [ESI+0x251] (unk_593 sub-anim index). Confirmed:
 * tag_block_get_element(mode+0x58, unk_593, 0xbc). Confirmed: MOV
 * EAX,[EBP+0xc]; LEA EBX,[EBP-0xc]; CALL 0x1a9560 (reg args). Confirmed: CMP
 * [local_c+0x98] / ptr at [local_c+0x9c] / word table indexed by AX. Confirmed:
 * object_set_region_count(unit_handle, update_kind). Confirmed: MOV
 * EAX,[EDI+0x44]; PUSH EBX; PUSH EAX; PUSH 0x1 ->
 * model_animation_choose_random. Confirmed: MOV EDI,[EDI+0x44]; CALL 0x1ab7c0
 * (unit_set_animation @<eax>,@<edi>,@<bx>). Confirmed: OR byte ptr
 * [ESI+0x248],0x1; MOV byte ptr [ESI+0x253],0x1d. Confirmed: CMP word ptr
 * [ESI+0x64],0x0 (type field == 0); CMP [ESI+0xcc],-1 (parent_object_index).
 * Confirmed: MOV EAX,[EBP+0x8]; CALL 0x1af180 (FUN_001af180 @<eax>,@<ecx>).
 */
bool unit_apply_animation_impulse(int unit_handle, int anim_index,
                                  void *anim_data)
{
  unit_data_t *unit;
  char *unit_tag;
  char *antr_tag;
  char *mode_elem;
  char *sub_anim;
  int16_t kind_anim_index;
  int16_t update_kind;
  int16_t chosen_anim;
  int antr_tag_index;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* Check if the unit's animation state allows applying an impulse.
   * FUN_001a96f0 takes @<eax>=unit_handle, @<edi>=impulse_index (leaked).
   * Confirmed disassembly: MOV EDI,[EBP+0xc] at 0x1a34, MOV EAX,EBX at 0x1a3c,
   * CALL 0x1a96f0 at 0x1a42 — EDI = anim_index at call time.
   */
  if (!unit_animation_state_allows_impulse(unit_handle, anim_index))
    return false;

  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));

  /* Locate the animation mode element for the unit's current mode (unk_592).
   * antr_tag+0x0c is the mode block; element size 0x64. */
  mode_elem = (char *)tag_block_get_element(antr_tag + 0xc,
                                            (int)(int8_t)unit->unk_592, 0x64);

  /* Locate the sub-animation element for the unit's current sub-anim (unk_593).
   * mode+0x58 is the sub-anim block; element size 0xbc. */
  sub_anim = (char *)tag_block_get_element(mode_elem + 0x58,
                                           (int)(int8_t)unit->unk_593, 0xbc);

  /* Map the impulse index to an animation kind index.
   * FUN_001a9560 takes impulse_index @<ax> and &update_kind @<ebx>;
   * writes update_kind (3 or 6) through the pointer, returns kind index in AX.
   * Confirmed: LEA EBX,[EBP-0xc]; MOV EAX,[EBP+0xc]; CALL 0x1a9560.
   */
  kind_anim_index =
    unit_impulse_to_animation_kind((int16_t)anim_index, &update_kind);

  /* Bounds-check the kind index against the sub-anim's kind table. */
  if (kind_anim_index < 0)
    return false;
  if ((int)kind_anim_index >= *(int *)(sub_anim + 0x98))
    return false;

  /* Index the kind->animation table (int16[] at sub_anim+0x9c). */
  kind_anim_index =
    *(int16_t *)(*(int *)(sub_anim + 0x9c) + (int)kind_anim_index * 2);
  if (kind_anim_index == -1)
    return false;

  /* Set interpolation mode and choose a random animation variant. */
  object_set_region_count(unit_handle, update_kind);

  antr_tag_index = *(int *)(unit_tag + 0x44);
  chosen_anim =
    (int16_t)model_animation_choose_random(1, antr_tag_index, kind_anim_index);

  /* Apply the chosen animation to the unit.
   * unit_set_animation: @<eax>=unit_handle, @<edi>=antr_tag_index,
   * @<bx>=chosen_anim. Confirmed: MOV EDI,[EDI+0x44]; MOV EAX,[EBP+0x8]; CALL
   * 0x1ab7c0.
   */
  unit_set_animation(unit_handle, antr_tag_index, chosen_anim);

  /* Mark animation impulse as active and set state to 0x1d. */
  unit->unk_584 |= 0x1;
  unit->unk_595 = 0x1d;

  /* If anim_data is provided and this is a top-level biped (type==0, no
   * parent), apply the facing alignment vector. FUN_001af180 takes unit_handle
   * @<eax>, anim_data @<ecx>. Confirmed: TEST ECX,ECX (param_3); CMP
   * [ESI+0x64],0; CMP [ESI+0xcc],-1.
   */
  if (anim_data != NULL && unit->object.type == 0 &&
      unit->object.parent_object_index.value == -1) {
    unit_apply_alignment_vector(unit_handle, (float *)anim_data);
  }

  return true;
}

/* unit_can_melee_attack (0x1b1d00)
 *
 * Returns true if the unit at object_handle can be hit by a melee attack
 * originating from position *position (a float[3]). The check is:
 *
 *   1. Object must be a biped (type == 0) via
 * object_try_and_get_and_verify_type with mask=3 (units).
 *   2. Unit tag flags dword at +0x17c must NOT have bit 0x10000 set.
 *   3. Dot product of (unit.pos - position) with unit.unk_528 must be <= 0.0f
 *      (i.e., the attacker is not in front of the victim along the victim's
 *      forward vector); OR the unit's seat label must be "asleep".
 *
 * Confirmed: PUSH 0x3 / PUSH EDI -> object_try_and_get_and_verify_type.
 * Confirmed: CMP word ptr [ESI+0x64],0x0; JNZ -> type == 0 check.
 * Confirmed: MOV EAX,[ESI]; PUSH EAX; PUSH 0x756e6974 ->
 * tag_get('unit',tag_index). Confirmed: MOV ECX,[EAX+0x17c]; TEST ECX,0x10000;
 * JNZ -> flag check. Confirmed: FLD [ESI+0x50]; FSUB [EAX]; ... -> dot product
 * over unk_528. Confirmed: FCOMP [0x2533c0](0.0f); FNSTSW AX; TEST AH,0x41; JZ
 * -> > 0.0f early return true. Confirmed: MOV ESI,[0x32e484] ("asleep" str
 * ptr); MOV EAX,EDI; CALL unit_get_seat_label. Confirmed: PUSH EAX; PUSH ESI;
 * CALL csstrcmp; TEST EAX,EAX; JNZ -> return false on mismatch.
 */
char unit_unsuspecting(int object_handle, void *position)
{
  unit_data_t *unit;
  char *unit_tag;
  float dx;
  float dy;
  float dz;
  float dot;
  int seat_label;
  float *pos;

  unit = (unit_data_t *)object_try_and_get_and_verify_type(object_handle, 3);
  if (unit == NULL)
    return 0;
  if (unit->object.type != 0)
    return 0;

  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  if (*(int *)(unit_tag + 0x17c) & 0x10000)
    return 0;

  pos = (float *)position;
  dx = unit->object.unk_80 - pos[0];
  dy = unit->object.unk_84 - pos[1];
  dz = unit->object.unk_88 - pos[2];

  dot = dx * unit->unk_528.x + dy * unit->unk_528.y + dz * unit->unk_528.z;

  if (dot > *(float *)0x2533c0)
    return 1;

  /* Dot product <= 0: only allow if unit is asleep */
  seat_label = unit_get_seat_label(object_handle);
  if (csstrcmp(*(const char **)0x32e484, (const char *)seat_label) != 0)
    return 0;

  return 1;
}

/* unit_enter_seat (0x1b1db0)
 *
 * Attempts to place a unit into a weapon/item seat. Validates that the seat
 * object (type 4) has flag bit 0x800 set and has no parent (parent_object_index
 * == -1). Then checks unit_can_enter_seat and game_engine_unit_can_enter_seat.
 *
 * If flag == 2, clears all existing weapons first. Finds an empty weapon slot
 * via an internal helper (0x1aad60, EAX reg-arg). If a slot is found:
 *   - disconnects the seat object from the map
 *   - disables garbage collection on it
 *   - attaches it to the unit
 *   - stores the seat object handle in unk_680[slot]
 *   - clears unk_696[slot]
 *
 * Based on the flag value:
 *   flag 0: sets unk_676 via unit_next_weapon_index(unit, unk_674, 0)
 *   flag 1: if control_flags bit 0x800 is clear, calls
 *           player_control_set_unit_seat, then sets unk_676 = slot
 *   flag 2: sets unk_676 = slot
 *   default: returns true without changing unk_676
 *
 * Returns true if the seat was entered, false otherwise.
 */
bool unit_enter_seat(int unit_handle, int seat_object_handle, int16_t flag)
{
  object_data_t *seat_obj;
  unit_data_t *unit;
  int16_t seat_index;

  seat_obj = (object_data_t *)object_get_and_verify_type(seat_object_handle, 4);
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  if (!(seat_obj->flags & 0x800))
    return false;
  if (seat_obj->parent_object_index.value != -1)
    return false;
  if (!unit_can_enter_seat(unit_handle, seat_object_handle))
    return false;
  if (!game_engine_unit_can_enter_seat(unit_handle, seat_object_handle))
    return false;

  if (flag == 2)
    unit_clear_weapons(unit_handle);

  seat_index = unit_find_empty_weapon_slot(unit_handle);

  if (seat_index == -1)
    return false;

  object_disconnect_from_map(seat_object_handle);
  object_set_garbage(seat_object_handle, 0);
  item_attach_to_unit(seat_object_handle, unit_handle);

  unit->unk_680[(int16_t)seat_index].value = seat_object_handle;
  unit->unk_696[(int16_t)seat_index].value = 0;

  switch (flag) {
  case 0:
    unit->unk_676 = unit_next_weapon_index(unit_handle, unit->unk_674, 0);
    return true;
  case 1:
    if (!(unit->unk_440 & 0x800))
      player_control_set_unit_seat(unit_handle, seat_index);
    /* fall through */
  case 2:
    unit->unk_676 = seat_index;
    return true;
  default:
    return true;
  }
}

/* unit_update_weapon_readiness (0x1b1ee0)
 *
 * Transitions the unit's active weapon based on its "next weapon" index
 * (unk_676, offset 0x2A4). If the unit currently holds a weapon (unk_674,
 * offset 0x2A2), attempts to place/stow it via weapon_try_place. On
 * success, detaches the current weapon from the parent, disconnects it
 * from the map, marks it as garbage, re-attaches it to the unit via
 * item_attach_to_unit, and clears unk_674.
 *
 * When unk_674 becomes -1 (no active weapon), looks up the "next" weapon
 * (EBX). If a next weapon exists, resolves its label, looks up the
 * animation state, connects the weapon to the map, attaches it at the
 * appropriate marker, copies unk_676 to unk_674, records game_time in
 * unk_696[weapon_index], and activates the weapon. If no next weapon,
 * uses the "unarmed" animation state and sets unk_674 to -1.
 *
 * Always calls unit_reset_weapon_state at the end.
 *
 * Register arg: unit_handle passed in ESI.
 *
 * Confirmed: PUSH 0x3 / PUSH ESI -> object_get_and_verify_type.
 * Confirmed: XOR ECX,ECX; MOV CX,[EAX+0x2a4] — unk_676.
 * Confirmed: XOR EDX,EDX; MOV DX,[EAX+0x2a2] — unk_674.
 * Confirmed: weapon_try_place at 0xfd360, object_detach_from_parent at
 * 0x1411c0. Confirmed: object_disconnect_from_map at 0x13fd00, FUN_0x13fb30 at
 * 0x13fb30. Confirmed: object_set_garbage at 0x13ffc0, item_attach_to_unit at
 * 0xf69c0. Confirmed: weapon_get_label at 0xfae80, unit_get_seat_label at
 * 0x1ae290. Confirmed: unit_try_animation_state at 0x1acd70. Confirmed:
 * object_connect_to_map at 0x140ce0, object_attach_to_marker at 0x144860.
 * Confirmed: weapon_activate at 0xfd2e0, unit_reset_weapon_state at 0x1b1290.
 * Confirmed: game_time_get at 0xb5aa0.
 * Confirmed: "unarmed" string at 0x2b6e68.
 */
void unit_update_weapon_readiness(int unit_handle, int flag)
{
  char *unit;
  char *unit_tag;
  int next_weapon_handle;
  int cur_weapon_handle;
  int seat_label;
  int weapon_label;
  char *antr_tag;
  char *mode_element;
  char *sub_anim;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

  /* Resolve next weapon from unk_676 */
  {
    char *u2 = (char *)object_get_and_verify_type(unit_handle, 3);
    uint16_t next_idx = *(uint16_t *)(u2 + 0x2a4);
    next_weapon_handle = unit_get_weapon(unit_handle, (int16_t)next_idx);
  }

  /* Resolve current weapon from unk_674 */
  {
    char *u3 = (char *)object_get_and_verify_type(unit_handle, 3);
    uint16_t cur_idx = *(uint16_t *)(u3 + 0x2a2);
    cur_weapon_handle = unit_get_weapon(unit_handle, (int16_t)cur_idx);
  }

  /* Try to place/stow the current weapon */
  if (cur_weapon_handle != -1) {
    if (weapon_try_place(cur_weapon_handle, flag)) {
      object_detach_from_parent(cur_weapon_handle);
      object_disconnect_from_map(cur_weapon_handle);
      object_activate(cur_weapon_handle);
      object_set_garbage(cur_weapon_handle, 0);
      item_attach_to_unit(cur_weapon_handle, unit_handle);
      *(uint16_t *)(unit + 0x2a2) = (uint16_t)-1;
    }
  }

  /* If no active weapon, transition to next or unarmed */
  if (*(int16_t *)(unit + 0x2a2) == -1) {
    if (next_weapon_handle != -1) {
      weapon_label = (int)weapon_get_label(next_weapon_handle);
      seat_label = unit_get_seat_label(unit_handle);
      unit_try_animation_state(unit_handle, seat_label, weapon_label, 1);

      /* Look up animation sub-element for weapon attachment markers */
      antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
      mode_element = (char *)tag_block_get_element(
        antr_tag + 0xc, (int)*(int8_t *)(unit + 0x250), 0x64);
      sub_anim = (char *)tag_block_get_element(
        mode_element + 0x58, (int)*(int8_t *)(unit + 0x251), 0xbc);

      /* Connect weapon to map and attach at markers */
      object_connect_to_map(next_weapon_handle, 0);
      object_set_garbage(next_weapon_handle, 1);
      object_attach_to_marker(unit_handle, sub_anim + 0x40, next_weapon_handle,
                              sub_anim + 0x20);

      /* Copy next weapon index to current */
      {
        uint16_t next_idx = *(uint16_t *)(unit + 0x2a4);
        *(uint16_t *)(unit + 0x2a2) = next_idx;
        if (next_idx != (uint16_t)-1) {
          int16_t cur = *(int16_t *)(unit + 0x2a2);
          ((int *)(unit + 0x2b8))[(int)cur] = game_time_get();
        }
      }

      weapon_activate(next_weapon_handle);
      unit_reset_weapon_state(unit_handle);
      return;
    }

    /* No next weapon — use "unarmed" animation */
    seat_label = unit_get_seat_label(unit_handle);
    unit_try_animation_state(unit_handle, seat_label, (int)"unarmed", 1);
    *(uint16_t *)(unit + 0x2a2) = (uint16_t)-1;
  }

  unit_reset_weapon_state(unit_handle);
}

/* FUN_001b2780 / unit_new (0x1b2780)
 *
 * Unit creation/initialization. Called when a new unit object is created.
 * Sets default values for all unit-specific fields: weapons, seats,
 * animation state, aiming vectors, control flags, etc.
 * Copies the object's forward vector to all 5 directional vector slots.
 * If the unit tag has initial seat occupancy data and a valid dialog
 * entry, assigns equipment creation. Returns 1 on success, 0 if the
 * unit tag's animation graph index is -1.
 *
 * Confirmed: 1 cdecl param (unit_handle), returns char (0 or 1).
 */
char FUN_001b2780(int unit_handle)
{
  char *unit;
  char *unit_tag;
  float *forward;
  char valid;
  int i;
  short seat_idx;
  char *seat_entry;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

  if (*(int *)(unit_tag + 0x44) == -1) { /* animation graph index */
    return 0;
  }

  /* Initialize weapon slots to -1 */
  *(int *)(unit + 0x2c8) = -1;
  csmemset((void *)(unit + 0x2a8), 0xff, 0x10);

  /* Initialize animation/seat tracking fields */
  *(int16_t *)(unit + 0x2a2) = -1;
  *(int16_t *)(unit + 0x2a4) = -1;
  *(uint8_t *)(unit + 0x2cc) = 0xff;
  *(uint8_t *)(unit + 0x2cd) = 0xff;
  *(uint8_t *)(unit + 0x2d0) = 0xff;
  *(uint8_t *)(unit + 0x2d1) = 0xff;
  *(int *)(unit + 0x1c8) = -1;
  *(int *)(unit + 0x1a4) = -1;
  *(int *)(unit + 0x1a8) = -1;
  *(int *)(unit + 0x1ac) = -1;
  *(int *)(unit + 0x1b0) = -1;
  *(int16_t *)(unit + 0x2a0) = -1;
  *(int *)(unit + 0x2d4) = -1;
  *(int *)(unit + 0x2d8) = -1;

  /* Clear base seat/weapon fields */
  *(int16_t *)(unit + 0x248) = 0;
  *(uint8_t *)(unit + 0x250) = 0xff;
  *(uint8_t *)(unit + 0x251) = 0xff;
  *(uint8_t *)(unit + 0x252) = 0xff;
  *(uint8_t *)(unit + 0x253) = 0xff;
  *(uint8_t *)(unit + 0x254) = 0;
  *(uint8_t *)(unit + 0x255) = 0;
  *(int16_t *)(unit + 0x24a) = -1;
  *(int16_t *)(unit + 0x24c) = -1;
  *(int16_t *)(unit + 0x25a) = -1;
  *(int16_t *)(unit + 0x25e) = -1;
  *(int16_t *)(unit + 0x262) = -1;
  *(uint8_t *)(unit + 0x257) = 2;
  *(int16_t *)(unit + 0x24e) = -1;
  *(uint8_t *)(unit + 0x258) = 0xff;
  *(int16_t *)(unit + 0x1ce) = -1;
  *(uint8_t *)(unit + 0x1bf) = 0xff;
  *(uint8_t *)(unit + 0x266) = 0;

  /* Zero-fill aiming/looking orientation buffers */
  csmemset((void *)(unit + 0x268), 0, 0x10);
  *(uint8_t *)(unit + 0x267) = 0;
  csmemset((void *)(unit + 0x278), 0, 0x10);

  /* Validate forward vector */
  forward = (float *)(unit + 0x24);
  valid = (char)valid_real_normal3d(forward);
  if (valid == 0) {
    csprintf((char *)0x5ab100,
             "%s: assert_valid_real_normal3d(%f, %f, %f)",
             "&unit->object.forward", (double)forward[0],
             (double)*(float *)(unit + 0x28),
             (double)*(float *)(unit + 0x2c),
             "c:\\halo\\SOURCE\\units\\units.c", 0x189, true);
    display_assert((char *)0x5ab100, "c:\\halo\\SOURCE\\units\\units.c",
                   0x189, true);
    system_exit(-1);
  }

  /* Copy forward vector to all 5 direction vector slots */
  *(float *)(unit + 0x210) = forward[0];
  *(float *)(unit + 0x214) = *(float *)(unit + 0x28);
  *(float *)(unit + 0x218) = *(float *)(unit + 0x2c);
  *(float *)(unit + 0x204) = forward[0];
  *(float *)(unit + 0x208) = *(float *)(unit + 0x28);
  *(float *)(unit + 0x20c) = *(float *)(unit + 0x2c);
  *(float *)(unit + 0x1ec) = forward[0];
  *(float *)(unit + 0x1f0) = *(float *)(unit + 0x28);
  *(float *)(unit + 0x1f4) = *(float *)(unit + 0x2c);
  *(float *)(unit + 0x1e0) = forward[0];
  *(float *)(unit + 0x1e4) = *(float *)(unit + 0x28);
  *(float *)(unit + 0x1e8) = *(float *)(unit + 0x2c);
  *(float *)(unit + 0x1d4) = forward[0];
  *(float *)(unit + 0x1d8) = *(float *)(unit + 0x28);
  *(float *)(unit + 0x1dc) = *(float *)(unit + 0x2c);

  /* Initialize control/animation state */
  *(int *)(unit + 0x1c0) = 0;
  *(int *)(unit + 0x334) = -1;
  *(int *)(unit + 0x1b4) = *(int *)(unit + 0x1b4) | 0x100;

  csmemset((void *)(unit + 0x338), 0, 0x7c);
  *(int *)(unit + 0x3a0) = -1;

  FUN_001a6bf0(unit_handle);

  csmemset((void *)(unit + 0x3e0), 0xff, 0x40);

  *(int16_t *)(unit + 0x3b4) = 0;
  *(int16_t *)(unit + 0x3b6) = 0;
  *(int *)(unit + 0x3b8) = 0;
  *(int16_t *)(unit + 0x3da) = 0;
  *(int *)(unit + 0x3bc) = -1;
  *(int *)(unit + 0x3cc) = -1;
  *(int16_t *)(unit + 0x2e4) = -1;
  *(int16_t *)(unit + 0x2e6) = -1;
  *(int *)(unit + 0x2f4) = 0x3f800000; /* 1.0f */
  *(uint8_t *)(unit + 0x23b) = 0;
  *(int *)(unit + 0x3c0) = -1;
  *(int *)(unit + 0x3dc) = -1;

  /* Assign initial grenade count from tag */
  {
    short grenade_type;
    grenade_type = *(short *)(unit_tag + 0x2c4);
    if (grenade_type >= 0 && grenade_type < 2 &&
        *(short *)(unit_tag + 0x2c6) >= 0) {
      *(uint8_t *)(unit + 0x2ce + (int)grenade_type) =
          *(uint8_t *)(unit_tag + 0x2c6);
    }
  }

  /* Set flags */
  *(int *)(unit + 0x4) = *(int *)(unit + 0x4) | 0x6000;

  /* Check powered melee probability */
  if (*(float *)(unit_tag + 0x22c) > *(float *)0x2533c0 &&
      *(float *)(unit_tag + 0x230) > *(float *)0x2533c0 &&
      *(float *)(unit_tag + 0x244) > *(float *)0x2533c0) {
    float random_val;

    random_val = random_math_real(
        (unsigned int *)get_global_random_seed_address());
    if (random_val < *(float *)(unit_tag + 0x244)) {
      *(int *)(unit + 0x1b4) = *(int *)(unit + 0x1b4) | 0x2000;
    } else {
      *(int *)(unit + 0x1b4) = *(int *)(unit + 0x1b4) & ~0x2000;
    }
  }

  /* Set initial health region permutation from tag */
  valid = (char)game_engine_running();
  if (valid == 0 &&
      (*(short *)(unit + 0x68) == 0 || *(short *)(unit + 0x68) == -1)) {
    *(int16_t *)(unit + 0x68) = *(int16_t *)(unit_tag + 0x180);
  }

  /* Apply initial animation state */
  unit_try_animation_state(unit_handle, *(int *)0x32e48c, 0, 1);

  /* Create initial weapons */
  unit_create_initial_weapons(unit_handle);

  /* Check for power-up equipment in seats */
  i = 0;
  seat_idx = 0;
  if (*(int *)(unit_tag + 0x2e4) > 0) {
    for (;;) {
      seat_entry = (char *)
          tag_block_get_element((int *)(unit_tag + 0x2e4), i, 0x11c);
      if (*(int *)(seat_entry + 0x104) != -1) {
        break;
      }
      seat_idx = seat_idx + 1;
      i = (int)seat_idx;
      if (i >= *(int *)(unit_tag + 0x2e4)) {
        return 1;
      }
    }
    ai_create_mounted_weapons_for_unit(unit_handle);
  }

  return 1;
}

/* unit_board_vehicle (0x1b2b80)
 *
 * Handles a unit boarding a vehicle at a specific seat. Validates the seat via
 * unit_find_nearby_seat, attaches the unit to the vehicle at the seat's marker,
 * sets up weapon state and animations for the boarding sequence. Asserts that
 * the unit is not already parented. If the unit's tag has a boarding animation
 * (animation graph entry index > 7 and valid boarding animation), plays it.
 * Returns true if the boarding succeeds, false if the seat check fails.
 */
bool unit_board_vehicle(int unit_handle, int vehicle_handle, int16_t seat_index)
{
  unit_data_t *unit;
  unit_data_t *vehicle_unit;
  void *unit_tag;
  void *seat_def;
  void *marker_name;
  vector3_t unit_pos;
  char markers[0x90]; /* marker output buffer */
  vector3_t delta;
  int weapon_handle;
  char *weapon_label;
  void *anim_tag;
  void *anim_entry;
  int16_t boarding_anim_index;
  int anim_result;

  if (!unit_find_nearby_seat(unit_handle, vehicle_handle, seat_index, 0))
    return false;

  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  vehicle_unit = (unit_data_t *)object_get_and_verify_type(vehicle_handle, 3);

  unit_tag = tag_get(0x756e6974, vehicle_unit->object.tag_index);
  seat_def =
    tag_block_get_element((char *)unit_tag + 0x2e4, (int)seat_index, 0x11c);

  if (unit->object.parent_object_index.value != -1) {
    display_assert("unit->object.parent_object_index==NONE",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1095, 1);
    system_exit(-1);
  }

  /* Get unit world position */
  object_get_world_position(unit_handle, &unit_pos);

  /* Find the seat marker on the vehicle */
  marker_name = (char *)seat_def + 0x24;
  object_get_markers_by_string_id(vehicle_handle, marker_name, markers, 1);

  /* Compute position delta: unit_pos - marker_pos */
  /* Marker position is at offset 0x60 in the marker output buffer */
  delta.x = unit_pos.x - *(float *)(markers + 0x60);
  delta.y = unit_pos.y - *(float *)(markers + 0x64);
  delta.z = unit_pos.z - *(float *)(markers + 0x68);

  /* Transform delta through marker's rotation matrix (at offset 0x38) */
  real_matrix3x3_transform_vector(markers + 0x38, &delta,
                                  &delta); /* dup-args-ok */

  /* Attach unit to vehicle at seat marker */
  object_attach_to_marker(vehicle_handle, marker_name, unit_handle,
                          (void *)0x25386f);

  /* Set seat index and parent */
  unit->unk_672 = seat_index;
  unit->object.parent_object_index.value = vehicle_handle;

  /* Update unit seat occupancy tracking. */
  unit_update_seat_occupancy(vehicle_handle);

  /* Re-fetch unit data after potential reallocation */
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);

  /* Set next weapon index */
  unit->unk_676 = unit_next_weapon_index(unit_handle, unit->unk_674, 0);

  unit_update_weapon_readiness(unit_handle, 1);

  /* Get current weapon */
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle, (int16_t)unit->unk_674);

  if (weapon_handle == -1)
    weapon_label = "unarmed";
  else
    weapon_label = weapon_get_label(weapon_handle);

  if (!unit_try_animation_state(unit_handle, (int)((char *)seat_def + 4),
                                (int)weapon_label, 1)) {
    /* Retry with NULL weapon label */
    unit_try_animation_state(unit_handle, (int)((char *)seat_def + 4), 0, 1);
  }

  /* Check for boarding animation in the unit's animation graph */
  unit = (unit_data_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = tag_get(0x756e6974, unit->object.tag_index);
  anim_tag = tag_get(0x616e7472, *(uint32_t *)((char *)unit_tag + 0x44));
  anim_entry = tag_block_get_element((char *)anim_tag + 0xc,
                                     (int)(int8_t)unit->unk_592, 100);

  if (*(int *)((char *)anim_entry + 0x40) > 7) {
    boarding_anim_index =
      *(int16_t *)(*(int *)((char *)anim_entry + 0x44) + 0xe);
    if (boarding_anim_index != -1) {
      int anim_graph_tag_index;

      /* Set interpolation */
      object_set_region_count(unit_handle, 6);

      /* Choose random boarding animation */
      anim_graph_tag_index = *(uint32_t *)((char *)unit_tag + 0x44);
      anim_result = model_animation_choose_random(1, anim_graph_tag_index,
                                                  boarding_anim_index);

      /* Set unit animation graph, index, and reset frame counter */
      unit_set_animation(unit_handle, anim_graph_tag_index,
                         (int16_t)anim_result);

      /* Set animation state byte */
      *((uint8_t *)unit + 0x253) = 0x1a;

      /* Adjust interpolation position with the delta */
      object_adjust_interpolation_position(unit_handle, &delta);

      /* Recursively update child object positions */
      object_update_children_recursive(unit_handle);
    }
  }

  unit_vehicle_board_notify(unit_handle, vehicle_handle);
  unit_reset_weapon_state(unit_handle);
  return true;
}

/* unit_get_zoom_level (0x1a8690)
 * Returns the unit's current zoom level from byte at unit+0x2D0. */
int16_t unit_get_zoom_level(int unit_handle)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  return (int16_t)*(int8_t *)(unit + 0x2D0);
}

/* unit_kill (0x1a7fa0)
 * Marks a unit for killing by setting bit 0x40 on object flags at +0xB6. */
void unit_kill(int unit_handle)
{
  char *obj;

  obj = (char *)object_get_and_verify_type(unit_handle, 3);
  *(uint8_t *)(obj + 0xB6) |= 0x40;
}

/* unit_set_controllable (0x1a9a50)
 * Sets or clears bit 6 (0x40) of unit flags dword at +0x1B4. */
void unit_set_controllable(int unit_handle, char controllable)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (controllable) {
    *(uint32_t *)(unit + 0x1B4) |= 0x40;
  } else {
    *(uint32_t *)(unit + 0x1B4) &= ~0x40u;
  }
}

/* unit_set_possessed (0x1a9a90)
 * Sets or clears bit 27 (0x8000000) of unit flags dword at +0x1B4. */
void unit_set_possessed(int unit_handle, char possessed)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (possessed) {
    *(uint32_t *)(unit + 0x1B4) |= 0x8000000;
  } else {
    *(uint32_t *)(unit + 0x1B4) &= ~0x8000000u;
  }
}

/* FUN_001a9ec0 (0x1a9ec0)
 * Returns datum handle at unit+0x2D4, or NONE if object lookup fails. */
int FUN_001a9ec0(int unit_handle)
{
  char *unit;

  unit = (char *)object_try_and_get_and_verify_type(unit_handle, 3);
  if (unit != NULL) {
    return *(int *)(unit + 0x2D4);
  }
  return -1;
}

/* FUN_001a9ef0 (0x1a9ef0)
 * Returns datum handle at unit+0x2D8, or NONE if object lookup fails. */
int FUN_001a9ef0(int unit_handle)
{
  char *unit;

  unit = (char *)object_try_and_get_and_verify_type(unit_handle, 3);
  if (unit != NULL) {
    return *(int *)(unit + 0x2D8);
  }
  return -1;
}

/* FUN_001a9f20 (0x1a9f20) — unit_shield_sapping_update
 *
 * Called each tick for units in the shield-sapping state (unit+0x253 == 0x2a
 * = '*'). Checks whether the unit's current seat index (+0x80) exceeds the
 * seat's max-shield-sapping threshold (+0x2e in the animation seat entry).
 * If so, iterates all players via the player_data table (0x5aa6d4) and for
 * each player whose unit (+0x34) is not NONE, computes the squared distance
 * between the sapping unit's position (+0x50) and the player's unit position
 * (+0x50). If within range (squared distance < 16.0f at 0x254e74), applies
 * damage from the unit tag's shield-sapping damage effect at +0x294.
 *
 * If any damage was dealt, resets the cooldown counter at unit+0x1be to 0.
 * Otherwise, increments it.
 *
 * Confirmed: cdecl, 1 stack param (unit_handle).
 * Confirmed: player iteration via data_iterator_new/next (0x1197b0/0x119810).
 * Confirmed: damage_data_new (0x136750) + object_cause_damage (0x137d20).
 * Confirmed: attacker handle stored at damage_params+0x0c = [EBP-0x5c].
 * Confirmed: 0x254e74 = 16.0f (squared distance threshold).
 */
void FUN_001a9f20(int unit_handle)
{
  char *unit;
  char *unit_tag;
  char *antr_tag;
  char *seat_entry;
  char *player_entry;
  int player_unit_handle;
  char *player_unit;
  float dx;
  float dy;
  float dz;
  char damage_params[0x60];
  data_iter_t player_iter;
  char did_damage;
  int damage_effect_index;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

  if (*(char *)(unit + 0x253) != '*') {
    return;
  }

  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit + 0x7c));
  seat_entry = (char *)tag_block_get_element(
      antr_tag + 0x74, (int)*(int16_t *)(unit + 0x80), 0xb4);

  if (*(int16_t *)(unit + 0x80) < *(int16_t *)(seat_entry + 0x2e)) {
    return;
  }

  did_damage = 0;
  data_iterator_new(&player_iter, *(data_t **)0x5aa6d4);
  player_entry = (char *)data_iterator_next(&player_iter);

  while (player_entry != NULL) {
    player_unit_handle = *(int *)(player_entry + 0x34);
    if (player_unit_handle != -1) {
      player_unit =
          (char *)object_get_and_verify_type(player_unit_handle, 3);

      dx = *(float *)(unit + 0x50) - *(float *)(player_unit + 0x50);
      dy = *(float *)(unit + 0x54) - *(float *)(player_unit + 0x54);
      dz = *(float *)(unit + 0x58) - *(float *)(player_unit + 0x58);

      if (dx * dx + dy * dy + dz * dz < 16.0f) {
        damage_effect_index = *(int *)(unit_tag + 0x294);
        damage_data_new(damage_params, damage_effect_index);
        *(int *)(damage_params + 0x0c) = unit_handle;
        object_cause_damage(damage_params, player_unit_handle,
                            (short)-1, (short)-1, (short)-1, 0);
        did_damage = 1;
      }
    }
    player_entry = (char *)data_iterator_next(&player_iter);
  }

  if (did_damage) {
    *(uint8_t *)(unit + 0x1be) = 0;
  } else {
    *(uint8_t *)(unit + 0x1be) += 1;
  }
}

/* FUN_001a7e70 (0x1a7e70)
 * Guard wrapper around unit_has_weapon_definition_index.
 * Returns false if either handle is NONE. */
char FUN_001a7e70(int unit_handle, int definition_index)
{
  char result;

  result = 0;
  if (unit_handle != -1 && definition_index != -1) {
    result = unit_has_weapon_definition_index(unit_handle, definition_index);
  }
  return result;
}

/* unit_destroy (0x1a91e0)
 * Destroys a unit: runs object_destroy then unit_test_spawning. */
void unit_destroy(int unit_handle)
{
  object_destroy(unit_handle);
  unit_test_spawning(unit_handle);
}

/* unit_scripting_can_blink (0x1a9c00)
 * Sets whether a unit can blink. Inverted: bit 22 (0x400000) at +0x1B4
 * means "cannot blink", so can_blink=false sets the bit. */
void unit_scripting_can_blink(int unit_handle, char can_blink)
{
  char *unit;

  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    if (can_blink == '\0') {
      *(uint32_t *)(unit + 0x1B4) |= 0x400000;
    } else {
      *(uint32_t *)(unit + 0x1B4) &= ~0x400000u;
    }
  }
}

/* unit_scripting_doesnt_drop_items (0x1a9c40)
 * Iterates child objects and sets bit 20 (0x100000) on each unit's flags. */
void unit_scripting_doesnt_drop_items(int object_list)
{
  int iter_state;
  int child;
  char *unit;

  child = FUN_000ce450(object_list, &iter_state);
  while (child != -1) {
    unit = (char *)object_try_and_get_and_verify_type(child, 3);
    if (unit != NULL) {
      *(uint32_t *)(unit + 0x1B4) |= 0x100000;
    }
    child = FUN_000ce320(object_list, &iter_state);
  }
}

/* FUN_001a7d40 (0x1a7d40)
 * Sums grenade counts for all grenade types (2 types). */
int FUN_001a7d40(int datum_handle)
{
  int i;
  int sum;
  char *ptr;
  char *unit;

  unit = (char *)object_try_and_get_and_verify_type(datum_handle, 3);
  sum = 0;
  if (unit != NULL) {
    ptr = unit + 0x2ce;
    i = 2;
    do {
      sum += (int16_t)*(int8_t *)ptr;
      ptr++;
      i--;
    } while (i != 0);
  }
  return sum;
}

/* FUN_001a7d80 (0x1a7d80)
 * Sets or clears bit 23 (0x800000) in unit flags for all child units. */
void FUN_001a7d80(int datum_handle, char flag)
{
  int iter_state;
  int child;
  char *unit;
  uint32_t flags;

  child = FUN_000ce450(datum_handle, &iter_state);
  while (child != -1) {
    unit = (char *)object_try_and_get_and_verify_type(child, 3);
    if (unit != NULL) {
      if (flag == '\0') {
        flags = *(uint32_t *)(unit + 0x1b4) & 0xff7fffff;
      } else {
        flags = *(uint32_t *)(unit + 0x1b4) | 0x800000;
      }
      *(uint32_t *)(unit + 0x1b4) = flags;
    }
    child = FUN_000ce320(datum_handle, &iter_state);
  }
}

/* FUN_001a7df0 (0x1a7df0)
 * Returns true only if FUN_001ac180 returns true for ALL child units. */
char FUN_001a7df0(int datum_handle, int param_2, int param_3, int param_4)
{
  char result;
  char cVar2;
  int child;
  char *unit;
  int iter_state;

  result = 1;
  child = FUN_000ce450(datum_handle, &iter_state);
  while (child != -1) {
    unit = (char *)object_try_and_get_and_verify_type(child, 3);
    if (unit != NULL) {
      if (result && (cVar2 = FUN_001ac180(child, param_2, (void *)param_3, param_4), cVar2 != '\0')) {
        result = 1;
      } else {
        result = 0;
      }
    }
    child = FUN_000ce320(datum_handle, &iter_state);
  }
  return result;
}

/* FUN_001a7ea0 (0x1a7ea0)
 * Checks if the unit's current weapon has the given tag definition. */
char FUN_001a7ea0(int unit_handle, int weapon_def_tag)
{
  char *unit;
  int weapon;
  int *weapon_data;

  if (unit_handle != -1 && weapon_def_tag != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    weapon = unit_get_weapon(unit_handle, (int)*(int16_t *)(unit + 0x2a2));
    if (weapon != -1) {
      weapon_data = (int *)object_get_and_verify_type(weapon, 4);
      return *weapon_data == weapon_def_tag;
    }
  }
  return 0;
}

/* unit_get_animation_frames_remaining (0x1a84c0)
 * Returns the number of animation frames remaining.
 * Outputs the current animation state byte to *animation_state_out. */
int unit_get_animation_frames_remaining(int unit_handle, int16_t *animation_state_out)
{
  char *unit;
  char *anim_tag;
  char *anim_entry;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  anim_tag = (char *)tag_get(0x616e7472, *(int *)(unit + 0x7c));
  anim_entry = (char *)tag_block_get_element(
      (int *)(anim_tag + 0x74),
      (int)*(int16_t *)(unit + 0x80),
      0xb4);
  if (animation_state_out == NULL) {
    display_assert("animation_state",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x738, 1);
    system_exit(-1);
  }
  *animation_state_out = (int16_t)*(int8_t *)(unit + 0x253);
  return (int)*(uint16_t *)(anim_entry + 0x22) -
         (int)*(uint16_t *)(unit + 0x82);
}

/* FUN_001a8730 (0x1a8730)
 * Returns 1 for vehicle-related animation states. @ecx = anim state ptr. */
char FUN_001a8730(void *anim_state)
{
  switch (*(uint8_t *)((char *)anim_state + 0xb)) {
  case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
  case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21:
  case 0x22: case 0x23: case 0x27: case 0x29:
    return 1;
  }
  return 0;
}

/* FUN_001a8790 (0x1a8790)
 * Returns 0 for vehicle/combat animation states. @ecx = anim state ptr. */
char FUN_001a8790(void *anim_state)
{
  switch (*(uint8_t *)((char *)anim_state + 0xb)) {
  case 1: case 2: case 3:
  case 0x17: case 0x1a: case 0x1b: case 0x1c:
  case 0x1d: case 0x1e: case 0x1f:
  case 0x21: case 0x22: case 0x23:
  case 0x27: case 0x29:
    return 0;
  }
  return 1;
}

/* FUN_001a87f0 (0x1a87f0)
 * Boolean: idle and free, unless overridden by certain anim states.
 * @ecx = anim state ptr. Switch table verified from binary. */
char FUN_001a87f0(void *anim_state)
{
  char result;

  result = *(char *)((char *)anim_state + 0xc) == '\0' &&
           *(int16_t *)((char *)anim_state + 0x1a) == -1;
  switch (*(uint8_t *)((char *)anim_state + 0xb)) {
  case 0x14: case 0x15: case 0x16:
  case 0x24: case 0x25: case 0x26:
    result = 0;
  }
  return result;
}

/* FUN_001a8850 (0x1a8850)
 * Returns 0 for specific vehicle/boarding animation states.
 * @ecx = anim state ptr. */
char FUN_001a8850(void *anim_state)
{
  switch (*(uint8_t *)((char *)anim_state + 0xb)) {
  case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
  case 0x1d: case 0x22: case 0x23:
    return 0;
  }
  return 1;
}

/* FUN_001a88b0 (0x1a88b0)
 * Maps animation state to animation index. @ecx = anim state value. */
int FUN_001a88b0(int16_t anim_state)
{
  switch ((int)anim_state) {
  case 0x0: case 0x2: case 0x3:
  case 0x10: case 0x11: case 0x12: case 0x13:
  case 0x14: case 0x15: case 0x16:
  case 0x25: case 0x26:
    return 0x19;
  case 0x4: case 0x5: case 0x6: case 0x7:
  case 0x8: case 0x9: case 0xa: case 0xb:
  case 0xc: case 0xd: case 0xe: case 0xf:
    return 0x1a;
  }
  return -1;
}

/* FUN_001a86b0 (0x1a86b0)
 * Animation state transition check. @ecx = anim state ptr, @edx = target state. */
char FUN_001a86b0(void *anim_state, int16_t target_state)
{
  switch (*(uint8_t *)((char *)anim_state + 0xb)) {
  case 0x2: case 0x3: case 0x25: case 0x26:
    if (target_state != 0) {
      return 1;
    }
    break;
  case 0x17: case 0x1a: case 0x1b: case 0x1c:
    break;
  case 0x18: case 0x19:
    if (target_state > 0x17 && target_state < 0x1a) {
      return 1;
    }
    break;
  case 0x1d: case 0x1e: case 0x1f:
  case 0x21: case 0x22: case 0x23:
  case 0x27: case 0x29:
    if (target_state != 0x17) {
      return 0;
    }
    goto default_return;
  default:
    goto default_return;
  }
  return 0;
default_return:
  return 1;
}

/* unit_impulse (0x1a8da0)
 * Applies scaled impulse to unit's transitional velocity (unit+0x18). */
void unit_impulse(int unit_index, int unused, float *impulse_vector, float scale)
{
  uint32_t *unit;
  char *unit_tag;
  char *phys_tag;
  (void)unused;

  unit = (uint32_t *)object_get_and_verify_type(unit_index, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  if (*(int *)(unit_tag + 0x8c) != -1) {
    phys_tag = (char *)tag_get(0x70687973, *(int *)(unit_tag + 0x8c));
    scale = scale / *(float *)(phys_tag + 8);
    *(float *)((char *)unit + 0x18) = scale * impulse_vector[0] + *(float *)((char *)unit + 0x18);
    *(float *)((char *)unit + 0x1c) = scale * impulse_vector[1] + *(float *)((char *)unit + 0x1c);
    *(float *)((char *)unit + 0x20) = scale * impulse_vector[2] + *(float *)((char *)unit + 0x20);
  }
}

/* unit_get_aiming_unit_index (0x1a9880)
 * Returns the datum index of the unit that controls aiming.
 * If the unit is in a seat with the aiming flag, returns the parent. */
int unit_get_aiming_unit_index(int unit_index)
{
  char *unit;
  uint32_t *parent;
  char *parent_tag;
  uint8_t *seat;

  if (unit_index == -1) {
    return -1;
  }
  unit = (char *)object_get_and_verify_type(unit_index, 3);
  if (*(int *)(unit + 0xcc) != -1 && *(int16_t *)(unit + 0x2a0) != -1) {
    parent = (uint32_t *)object_get_and_verify_type(*(int *)(unit + 0xcc), 3);
    parent_tag = (char *)tag_get(0x756e6974, *parent);
    seat = (uint8_t *)tag_block_get_element(
        (int *)(parent_tag + 0x2e4),
        (int)*(int16_t *)(unit + 0x2a0), 0x11c);
    if ((*seat & 9) != 0) {
      return *(int *)(unit + 0xcc);
    }
  }
  return unit_index;
}

/* unit_scripting_set_emotion_animation (0x1a9b30)
 * Looks up an animation by name and sets it as the unit's emotion animation. */
void unit_scripting_set_emotion_animation(int unit_index, const char *animation_name)
{
  char *unit;
  int16_t anim;

  if (unit_index != -1) {
    unit = (char *)object_get_and_verify_type(unit_index, 3);
    anim = FUN_00120cb0(*(int *)(unit + 0x7c), animation_name);
    if (anim != -1) {
      *(int16_t *)(unit + 0x1ce) = anim;
      return;
    }
    console_warning("couldn\'t find the emotion animation \'%s\'", animation_name);
  }
}

/* unit_scripting_suspended (0x1a9b80)
 * Suspends or unsuspends a unit. Sets/clears bit 24 (0x1000000) at +0x1B4,
 * zeros the velocity from global origin, and clears biped bit if type==0. */
void unit_scripting_suspended(int unit_index, char suspended)
{
  char *unit;
  uint32_t flags;
  char *global_origin;
  char *biped;

  if (unit_index != -1) {
    unit = (char *)object_get_and_verify_type(unit_index, 3);
    if (suspended != '\0') {
      flags = *(uint32_t *)(unit + 0x1b4) | 0x1000000;
    } else {
      flags = *(uint32_t *)(unit + 0x1b4) & 0xfeffffff;
    }
    *(uint32_t *)(unit + 0x1b4) = flags;
    global_origin = *(char **)0x31fc38;
    *(uint32_t *)(unit + 0x18) = *(uint32_t *)global_origin;
    *(uint32_t *)(unit + 0x1c) = *(uint32_t *)(global_origin + 4);
    *(uint32_t *)(unit + 0x20) = *(uint32_t *)(global_origin + 8);
    if (*(int16_t *)(unit + 0x64) == 0) {
      biped = (char *)object_get_and_verify_type(unit_index, 1);
      *(uint32_t *)(biped + 0x424) &= 0xfffffffe;
    }
  }
}

/* FUN_001a9c90 (0x1a9c90) — unit_scripting_vehicle_test_seat_list
 *
 * Checks whether any unit occupying a named seat in the given vehicle
 * (unit_handle) is present in the object_list. Iterates seats from the
 * unit tag block at +0x2e4 (element size 0x11c), compares seat_name at
 * element+4 via crt_stricmp. For each matching seat, scans all unit objects
 * (type mask 3) to find one whose parent at +0xcc is this unit and whose
 * seat index at +0x2a0 matches. Then checks if that seated unit is in the
 * child list via FUN_000ce450/FUN_000ce320.
 *
 * Returns true (1) if any seated occupant was found in the object_list.
 *
 * Confirmed: cdecl, 3 stack params.
 * Confirmed: seat element size 0x11c, name at element+4.
 * Confirmed: object iterator at EBP-0x18 (16 bytes), datum handle at iter+0x08.
 * Confirmed: child iter state at EBP-0x8 (single int).
 */
char FUN_001a9c90(int unit_handle, const char *seat_name, int object_list)
{
  char *unit_data;
  char *unit_tag;
  int *seats_block;
  char *seat_element;
  int obj_iter[4];
  int child_iter_state;
  int child_handle;
  int seated_handle;
  char *seated_unit;
  char result;
  int16_t seat_index;
  int seat_count;

  result = 0;

  if (unit_handle == -1) {
    return 0;
  }

  unit_data = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit_data);
  seats_block = (int *)(unit_tag + 0x2e4);
  seat_count = *seats_block;
  seat_index = 0;

  if (seat_count <= 0) {
    return 0;
  }

  while ((int)seat_index < seat_count) {
    seat_element =
        (char *)tag_block_get_element(seats_block, (int)seat_index, 0x11c);

    if (crt_stricmp(seat_name, seat_element + 4) == 0) {
      /* Seat name matches — find the occupant */
      object_iterator_new(obj_iter, 3, 0);
      seated_unit = (char *)object_iterator_next(obj_iter);

      while (seated_unit != NULL) {
        if (*(int *)(seated_unit + 0xcc) == unit_handle &&
            *(int16_t *)(seated_unit + 0x2a0) == seat_index) {
          /* Found the unit sitting in this seat — check object list */
          seated_handle = obj_iter[2];
          child_handle = FUN_000ce450(object_list, &child_iter_state);
          while (child_handle != -1) {
            if (seated_handle == child_handle) {
              goto found;
            }
            child_handle = FUN_000ce320(object_list, &child_iter_state);
          }
          /* child_handle == -1 here; check if seated_handle also -1 */
          if (seated_handle == child_handle) {
            goto found;
          }
          break;
        }
        seated_unit = (char *)object_iterator_next(obj_iter);
      }
    }

    seat_index++;
    seat_count = *seats_block;
    continue;
found:
    result = 1;
    seat_index++;
    seat_count = *seats_block;
  }

  return result;
}

/* unit_scripting_vehicle_test_seat (0x1a9da0)
 * Returns true if unit_index is sitting in the named seat of vehicle_index. */
char unit_scripting_vehicle_test_seat(int vehicle_index, const char *seat_name, int unit_index)
{
  uint32_t *vehicle;
  char *vehicle_tag;
  int *seats_block;
  char *seat_element;
  char *unit_data;
  int16_t i;

  if (vehicle_index != -1 && unit_index != -1) {
    vehicle = (uint32_t *)object_get_and_verify_type(vehicle_index, 3);
    vehicle_tag = (char *)tag_get(0x756e6974, *vehicle);
    seats_block = (int *)(vehicle_tag + 0x2e4);
    if (0 < *seats_block) {
      i = 0;
      do {
        seat_element = (char *)tag_block_get_element(seats_block, (int)i, 0x11c);
        if (crt_stricmp(seat_name, seat_element + 4) == 0) {
          unit_data = (char *)object_get_and_verify_type(unit_index, 3);
          if (*(int *)(unit_data + 0xcc) == vehicle_index &&
              *(int16_t *)(unit_data + 0x2a0) == i) {
            return 1;
          }
        }
        i++;
      } while ((int)i < *seats_block);
    }
  }
  return 0;
}

/* FUN_001aa4d0 (0x1aa4d0) — unit_driven_by_ai
 * Returns true if the unit (or its driver) has an AI actor. */
char FUN_001aa4d0(int unit_handle)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(int *)(unit + 0x2d4) != -1) {
    unit = (char *)object_get_and_verify_type(*(int *)(unit + 0x2d4), 3);
  }
  return *(int *)(unit + 0x1a4) != -1;
}

/* FUN_001aa510 (0x1aa510) — unit_gunned_by_ai
 * Returns true if the unit (or its gunner) has an AI actor. */
char FUN_001aa510(int unit_handle)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(int *)(unit + 0x2d8) != -1) {
    unit = (char *)object_get_and_verify_type(*(int *)(unit + 0x2d8), 3);
  }
  return *(int *)(unit + 0x1a4) != -1;
}

/* FUN_001a7cc0 (0x1a7cc0)
 * Returns the body vitality (0x90) or 0.0 if dead, default if invalid. */
float FUN_001a7cc0(int datum_handle)
{
  char *obj;
  float result;

  obj = (char *)object_try_and_get_and_verify_type(datum_handle, -1);
  result = *(float *)0x255e94;
  if (obj != NULL) {
    if ((*(uint8_t *)(obj + 0xb6) & 4) != 0) {
      return 0.0f;
    }
    result = *(float *)(obj + 0x90);
  }
  return result;
}

/* FUN_001a7d00 (0x1a7d00)
 * Returns the shield vitality (0x94) or 0.0 if dead, default if invalid. */
float FUN_001a7d00(int datum_handle)
{
  char *obj;
  float result;

  obj = (char *)object_try_and_get_and_verify_type(datum_handle, -1);
  result = *(float *)0x255e94;
  if (obj != NULL) {
    if ((*(uint8_t *)(obj + 0xb6) & 4) != 0) {
      return 0.0f;
    }
    result = *(float *)(obj + 0x94);
  }
  return result;
}

/* FUN_001a7c70 (0x1a7c70)
 * Iterates child objects and calls FUN_001a7b50 on each. */
void FUN_001a7c70(int parent_handle, int param_2, int param_3)
{
  int iter_state;
  int child;

  child = FUN_000ce450(parent_handle, &iter_state);
  while (child != -1) {
    FUN_001a7b50(child, param_2, param_3);
    child = FUN_000ce320(parent_handle, &iter_state);
  }
}

/* unit_inventory_next_grenade (0x1a9980)
 * Cycles through grenade types to find the next available one. */
int16_t unit_inventory_next_grenade(int unit_handle, int current_index, int16_t direction)
{
  char *unit;
  int16_t start;
  int16_t saved;
  int idx;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  saved = -1;
  start = (int16_t)current_index;
  if (start == -1) {
    current_index = 0;
    idx = current_index;
  } else {
    idx = current_index;
    if (start < 0 || 1 < start) {
      display_assert("current_index>=0 && current_index<NUMBER_OF_UNIT_GRENADE_TYPES",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x163e, 1);
      system_exit(-1);
    }
  }
  do {
    start = (int16_t)current_index;
    if (*(char *)((int)start + 0x2ce + (int)unit) > '\0') {
      if (start != (int16_t)idx) {
        return start;
      }
      saved = (int16_t)current_index;
      if (direction == 0) {
        return start;
      }
    }
    if (direction < 0) {
      if (start == 0) {
        current_index = 1;
      } else {
        current_index = (int)start - 1;
      }
    } else if (start == 1) {
      current_index = 0;
    } else {
      current_index = (int)start + 1;
    }
    if ((int16_t)current_index == (int16_t)idx) {
      return saved;
    }
  } while (1);
}

/* unit_scripting_unit_riders (0x1a9e40)
 * Creates a list of all child units that have a seat assignment. */
int unit_scripting_unit_riders(int unit_handle)
{
  char *unit;
  int list;
  int child;
  char *child_obj;

  list = -1;
  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    list = FUN_000ce200();
    if (list != -1) {
      child = *(int *)(unit + 0xc8);
      while (child != -1) {
        child_obj = (char *)object_get_and_verify_type(child, -1);
        if (((1 << (*(uint8_t *)(child_obj + 0x64) & 0x1f)) & 3u) != 0 &&
            *(int16_t *)(child_obj + 0x2a0) != -1) {
          FUN_000ce2b0(list, child);
        }
        child = *(int *)(child_obj + 0xc4);
      }
    }
  }
  return list;
}

/* units_debug_get_next_unit (0x1aa080)
 * Finds the next debug-selectable unit after current_unit. */
int units_debug_get_next_unit(int current_unit)
{
  int result;
  char *iVar1;
  char local_14[8];
  int local_c = 0;

  result = -1;
  if (current_unit != -1) {
    object_iterator_new(local_14, 3, 0);
    iVar1 = (char *)object_iterator_next(local_14);
    while (iVar1 != NULL && local_c != current_unit) {
      iVar1 = (char *)object_iterator_next(local_14);
    }
    iVar1 = (char *)object_iterator_next(local_14);
    while (iVar1 != NULL) {
      if (*(int *)(iVar1 + 0x1a4) == -1 &&
          *(int *)(iVar1 + 0x1a8) == -1 &&
          (*(uint8_t *)(iVar1 + 0xb6) & 4) == 0) {
        result = local_c;
        if (local_c != -1) {
          return local_c;
        }
        break;
      }
      iVar1 = (char *)object_iterator_next(local_14);
    }
  }
  object_iterator_new(local_14, 3, 0);
  iVar1 = (char *)object_iterator_next(local_14);
  while (1) {
    if (iVar1 == NULL) {
      return result;
    }
    if (*(int *)(iVar1 + 0x1a4) == -1 &&
        *(int *)(iVar1 + 0x1a8) == -1 &&
        (*(uint8_t *)(iVar1 + 0xb6) & 4) == 0) {
      break;
    }
    iVar1 = (char *)object_iterator_next(local_14);
  }
  return local_c;
}

/* unit_set_desired_flashlight_state (0x1aa550)
 * Sets the desired flashlight state. Bit 28=on, bit 29=off at +0x1B4. */
void unit_set_desired_flashlight_state(int unit_handle, char desired)
{
  char *unit;

  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    if (desired != '\0') {
      *(uint32_t *)(unit + 0x1b4) |= 0x10000000;
    } else {
      *(uint32_t *)(unit + 0x1b4) |= 0x20000000;
    }
  }
}

/* unit_get_current_flashlight_state (0x1aa590)
 * Returns whether the flashlight is currently on. Bit 19 of +0x1B4. */
char unit_get_current_flashlight_state(int unit_handle)
{
  char *unit;

  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    return (*(uint32_t *)(unit + 0x1b4) >> 0x13) & 1;
  }
  return 0;
}

/* unit_seat_filled (0x1aa700)
 * Returns true if any unit is sitting in the specified seat. */
char unit_seat_filled(int unit_handle, int16_t seat_index)
{
  char *iVar1;
  char local_14[16];

  object_iterator_new(local_14, 3, 0);
  iVar1 = (char *)object_iterator_next(local_14);
  while (1) {
    if (iVar1 == NULL) {
      return 0;
    }
    if (*(int *)(iVar1 + 0xcc) == unit_handle &&
        *(int16_t *)(iVar1 + 0x2a0) == seat_index) {
      break;
    }
    iVar1 = (char *)object_iterator_next(local_14);
  }
  return 1;
}

/* unit_seat_is_driver (0x1aa770)
 * Returns true if the seat at seat_index has the driver flag (bit 2). */
char unit_seat_is_driver(int unit_handle, int16_t seat_index)
{
  uint32_t *unit;
  char *unit_tag;
  uint32_t *seat;

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  if (seat_index >= 0) {
    if ((int)seat_index < *(int *)(unit_tag + 0x2e4)) {
      seat = (uint32_t *)tag_block_get_element(
          (int *)(unit_tag + 0x2e4), (int)seat_index, 0x11c);
      return (*seat >> 2) & 1;
    }
  }
  return 0;
}

/* unit_seat_is_gunner (0x1aa7d0)
 * Returns true if the seat at seat_index has the gunner flag (bit 3). */
char unit_seat_is_gunner(int unit_handle, int16_t seat_index)
{
  uint32_t *unit;
  char *unit_tag;
  uint32_t *seat;

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  if (seat_index >= 0) {
    if ((int)seat_index < *(int *)(unit_tag + 0x2e4)) {
      seat = (uint32_t *)tag_block_get_element(
          (int *)(unit_tag + 0x2e4), (int)seat_index, 0x11c);
      return (*seat >> 3) & 1;
    }
  }
  return 0;
}

/* unit_seat_allow_noncombatants (0x1aa830)
 * Returns true if the seat at seat_index allows noncombatants (bit 10). */
char unit_seat_allow_noncombatants(int unit_handle, int16_t seat_index)
{
  uint32_t *unit;
  char *unit_tag;
  uint32_t *seat;

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  if (seat_index >= 0) {
    if ((int)seat_index < *(int *)(unit_tag + 0x2e4)) {
      seat = (uint32_t *)tag_block_get_element(
          (int *)(unit_tag + 0x2e4), (int)seat_index, 0x11c);
      return (*seat >> 10) & 1;
    }
  }
  return 0;
}

/* unit_has_weapon_definition_index (0x1aad00)
 * Checks whether a unit is carrying a weapon with the given tag definition. */
char unit_has_weapon_definition_index(int unit_handle, int definition_index)
{
  char *unit;
  int weapon_handle;
  int *weapon;
  int16_t i;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  i = 0;
  do {
    weapon_handle = *(int *)(unit + 0x2a8 + i * 4);
    if (weapon_handle != -1) {
      weapon = (int *)object_get_and_verify_type(weapon_handle, 4);
      if (*weapon == definition_index) {
        return 1;
      }
    }
    i++;
  } while (i < 4);
  return 0;
}

/* unit_start_running_blindly (0x1ac450)
 *
 * Sets bit 25 (0x2000000) of unit flags at +0x1B4 and computes a random
 * blind-running direction stored at unit+0x3C4.
 *
 * If the unit has an actor (unit+0x1A4 != -1), tries to get a running blind
 * vector via actor_get_running_blind_vector. On success, stores 0 at +0x3C4
 * and uses a 25-degree random spread. On failure (or no actor), computes the
 * unit's facing yaw via vector_to_angles, wraps it to [0, pi), and uses a
 * 100-degree random spread.
 *
 * The final direction is: base_angle + random_real_range(seed, -spread, spread).
 *
 * Confirmed: PUSH [EBP+0x8] / PUSH 0x3 -> cdecl, 1 stack param.
 * Confirmed: TEST EAX,0x2000000 -> early exit if already running blindly.
 * Confirmed: OR EAX,0x2000000 -> set flag.
 * Confirmed: CALL 0x3ce40 = actor_get_running_blind_vector.
 * Confirmed: CALL 0x10cc00 = vector_to_angles.
 * Confirmed: FCOM [0x256980] (pi); FSUB [0x255a54] (2*pi) -> wrap to [0,pi).
 * Confirmed: 0x3edf66f3 = 0.4363f (25 deg), 0x3fdf66f3 = 1.7453f (100 deg).
 * Confirmed: push-then-fstp pattern for random_real_range float args.
 * Confirmed: FADD [ESI+0x3c4]; FSTP [ESI+0x3c4] -> accumulate into direction.
 */
void unit_start_running_blindly(int unit_handle)
{
  char *unit;
  uint32_t flags;
  int actor_handle;
  char has_blind_vector;
  float base_angle;
  float spread;
  int *seed;
  float angles[2];
  char blind_vector[12];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  flags = *(uint32_t *)(unit + 0x1b4);
  if ((flags & 0x2000000) != 0) {
    return;
  }
  *(uint32_t *)(unit + 0x1b4) = flags | 0x2000000;

  actor_handle = *(int *)(unit + 0x1a4);
  if (actor_handle != -1) {
    has_blind_vector = actor_get_running_blind_vector(actor_handle, (float *)blind_vector);
  } else {
    has_blind_vector = 0;
  }

  if (has_blind_vector) {
    *(float *)(unit + 0x3c4) = 0.0f;
    spread = 0.4363323f;
  } else {
    vector_to_angles(angles, (float *)(unit + 0x24));
    base_angle = angles[0];
    if (base_angle > 3.1415927f) {
      base_angle = base_angle - 6.2831855f;
    }
    *(float *)(unit + 0x3c4) = base_angle;
    spread = 1.7453293f;
  }

  seed = get_global_random_seed_address();
  *(float *)(unit + 0x3c4) =
      random_real_range(seed, -spread, spread) + *(float *)(unit + 0x3c4);
}

/* unit_stop_running_blindly (0x1ac520)
 * Clears bit 25 (0x2000000) of unit flags at +0x1B4. */
void unit_stop_running_blindly(int unit_handle)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  *(uint32_t *)(unit + 0x1b4) &= ~0x2000000u;
}

/* unit_flying_through_air (0x1ac650)
 * Returns true if this is a biped (type==0) and it's flying through air. */
char unit_flying_through_air(int unit_handle)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(int16_t *)(unit + 0x64) == 0) {
    return FUN_001a0db0(unit_handle);
  }
  return 0;
}

/* unit_abort_animation (0x1ad7e0)
 * Aborts the current animation by calling FUN_001ad260 with state 0. */
void unit_abort_animation(int unit_handle)
{
  FUN_001ad260(unit_handle, 0);
}

/* unit_open (0x1ae160)
 * Opens a unit by transitioning to animation state 0x25. */
void unit_open(int unit_handle)
{
  if (unit_handle != -1) {
    FUN_001ad260(unit_handle, 0x25);
  }
}

/* unit_close (0x1ae180)
 * Closes a unit by transitioning to animation state 0x26. */
void unit_close(int unit_handle)
{
  if (unit_handle != -1) {
    FUN_001ad260(unit_handle, 0x26);
  }
}

/* units_set_desired_flashlight_state (0x1ae210)
 * Iterates child objects and sets flashlight state for each unit. */
void units_set_desired_flashlight_state(int object_list, char desired)
{
  int iter_state;
  int child;
  char *unit;
  uint32_t flags;

  child = FUN_000ce450(object_list, &iter_state);
  while (child != -1) {
    unit = (char *)object_try_and_get_and_verify_type(child, 3);
    if (unit != NULL && child != -1) {
      unit = (char *)object_get_and_verify_type(child, 3);
      if (desired == '\0') {
        flags = *(uint32_t *)(unit + 0x1b4) | 0x20000000;
      } else {
        flags = *(uint32_t *)(unit + 0x1b4) | 0x10000000;
      }
      *(uint32_t *)(unit + 0x1b4) = flags;
    }
    child = FUN_000ce320(object_list, &iter_state);
  }
}

/* unit_scripting_set_seat (0x1ae750)
 * Sets the unit's seat override from a seat name string. */
void unit_scripting_set_seat(int unit_handle, const char *seat_name)
{
  char *unit;
  char seat;

  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    seat = (char)FUN_001ab730(seat_name);
    *(char *)(unit + 0x1bf) = seat;
  }
}

/* unit_handle_deleted_object (0x1ae780)
 * Cleans up references to a deleted object in all unit fields. */
void unit_handle_deleted_object(int unit_handle, int deleted_handle)
{
  char *unit;
  int16_t i;
  int *weapon_slot;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(int *)(unit + 0x244) == deleted_handle) {
    *(int *)(unit + 0x244) = -1;
  }
  if (*(int *)(unit + 0x2d4) == deleted_handle) {
    *(int *)(unit + 0x2d4) = -1;
  }
  if (*(int *)(unit + 0x2d8) == deleted_handle) {
    *(int *)(unit + 0x2d8) = -1;
  }
  i = 0;
  weapon_slot = (int *)(unit + 0x2a8);
  do {
    if (*weapon_slot == deleted_handle) {
      *weapon_slot = -1;
      if (i == *(int16_t *)(unit + 0x2a4)) {
        *(int16_t *)(unit + 0x2a4) = -1;
      }
      if (i == *(int16_t *)(unit + 0x2a2)) {
        *(int16_t *)(unit + 0x2a2) = -1;
      }
    }
    i++;
    weapon_slot++;
  } while (i < 4);
  if (*(int16_t *)(unit + 0x2a2) == -1) {
    *(int16_t *)(unit + 0x2a4) = FUN_001ae490(unit_handle, -1, 0);
  }
  if (*(int *)(unit + 0x2c8) == deleted_handle) {
    *(int *)(unit + 0x2c8) = -1;
  }
  if (*(int *)(unit + 0x3bc) == deleted_handle) {
    *(int *)(unit + 0x3bc) = -1;
  }
}

/* unit_stop_custom_animation (0x1af0d0)
 * Stops a custom animation if the current state is 0x1c. */
void unit_stop_custom_animation(int unit_handle)
{
  char *unit;

  if (unit_handle != -1) {
    unit = (char *)object_get_and_verify_type(unit_handle, 3);
    if (*(char *)(unit + 0x253) == '\x1c') {
      FUN_001ad260(unit_handle, 0);
    }
  }
}

/* unit_custom_animation_at_frame (0x1af100)
 * Starts a custom animation and sets it to a specific frame. */
char unit_custom_animation_at_frame(int unit_handle, int param_2, int param_3, int param_4,
                                    int16_t frame)
{
  char *unit;
  char *anim_tag;
  char *anim_entry;

  if (!FUN_001ac180(unit_handle, param_2, (void *)param_3, param_4)) {
    return 0;
  }
  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  anim_tag = (char *)tag_get(0x616e7472, *(int *)(unit + 0x7c));
  anim_entry = (char *)tag_block_get_element(
      (int *)(anim_tag + 0x74),
      (int)*(int16_t *)(unit + 0x80), 0xb4);
  if (frame >= 0 && frame < *(int16_t *)(anim_entry + 0x22)) {
    *(int16_t *)(unit + 0x82) = frame;
    return 1;
  }
  return 0;
}

/* unit_has_animation_to_enter_seat (0x1b0d00)
 * Returns true if the unit has an animation to enter the given seat. */
char unit_has_animation_to_enter_seat(int unit_handle, int vehicle_handle, int16_t seat_index)
{
  uint32_t *vehicle;
  char *unit_tag;
  char *unit_data;
  char *seat_entry;

  vehicle = (uint32_t *)object_get_and_verify_type(vehicle_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *vehicle);
  if (seat_index >= 0) {
    if ((int)seat_index < *(int *)(unit_tag + 0x2e4)) {
      unit_data = (char *)object_get_and_verify_type(unit_handle, 3);
      if (*(int16_t *)(unit_data + 0x64) != 1) {
        seat_entry = (char *)tag_block_get_element(
            (int *)(unit_tag + 0x2e4), (int)seat_index, 0x11c);
        if (!FUN_001acd70(unit_handle, (const char *)(seat_entry + 4), 0, 0)) {
          return 0;
        }
      }
      return 1;
    }
  }
  return 0;
}

/* unit_get_zoom_magnification (0x1b1350)
 * Returns the zoom magnification for the current weapon. */
float unit_get_zoom_magnification(int unit_handle, int zoom_level)
{
  char *unit;
  int weapon;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon = unit_get_weapon(unit_handle, (int)*(int16_t *)(unit + 0x2a2));
  if (weapon != -1) {
    return weapon_get_zoom_magnification(weapon, zoom_level);
  }
  return 1.0f;
}

/* unit_inventory_next_weapon (0x1b1b40)
 * Advances to the next weapon in the inventory. */
int unit_inventory_next_weapon(int unit_handle, int slot, int direction)
{
  return FUN_001ae490(unit_handle, slot, direction);
}

/* FUN_001a7a90 (0x1a7a90)
 * Applies damage to an object if it's not dead (bit 2 of +0xB6 clear).
 *
 * body_dmg/shield_dmg are passed BY VALUE. The original (delinked) takes three
 * 4-byte stack args at [ebp+8]/[ebp+0xc]/[ebp+0x10] and does
 *   lea ecx,[ebp+0xc]  (&body_dmg) / lea eax,[ebp+0x10] (&shield_dmg)
 * to box the by-value floats into the pointers FUN_001365d0 expects (1365d0
 * dereferences arg2/arg3). Declaring them as float* and forwarding the pointers
 * reinterprets the float bit-pattern (1.0f == 0x3f800000) as an address and
 * dereferences it — an infinite page-fault storm that froze PoA after the intro
 * (FUN_000bf380 calls this with floats pushed by value). */
void FUN_001a7a90(int param_1, float body_dmg, float shield_dmg)
{
  char *obj;

  if (param_1 != -1) {
    obj = (char *)object_get_and_verify_type(param_1, -1);
    if ((*(uint8_t *)(obj + 0xb6) & 4) == 0) {
      FUN_001365d0(param_1, &body_dmg, &shield_dmg);
    }
  }
}

/* FUN_001a7ad0 (0x1a7ad0)
 * Iterates child objects and applies damage to each alive object. */
void FUN_001a7ad0(int parent_handle, int param_2, int param_3)
{
  int iter_state;
  int child;
  char *obj;
  int local_c;
  int local_8;

  child = FUN_000ce450(parent_handle, &iter_state);
  while (child != -1) {
    local_8 = param_3;
    local_c = param_2;
    if (child != -1) {
      obj = (char *)object_get_and_verify_type(child, -1);
      if ((*(uint8_t *)(obj + 0xb6) & 4) == 0) {
        FUN_001365d0(child, (float *)&local_c, (float *)&local_8);
      }
    }
    child = FUN_000ce320(parent_handle, &iter_state);
  }
}

/* FUN_001a7b50 (0x1a7b50)
 * Computes and stores body/shield vitality ratios. Triggers events when
 * vitality transitions from nonzero to zero. */
void FUN_001a7b50(int datum_handle, float body_damage, float shield_damage)
{
  float shield_ratio;
  char *obj;
  float body_ratio;

  if (datum_handle == -1) {
    return;
  }
  obj = (char *)object_get_and_verify_type(datum_handle, -1);
  if ((*(uint8_t *)(obj + 0xb6) & 4) != 0) {
    return;
  }
  if (*(float *)(obj + 0x8c) <= 0.0f) {
    body_ratio = 0.0f;
  } else if (shield_damage < *(float *)(obj + 0x8c)) {
    body_ratio = shield_damage / *(float *)(obj + 0x8c);
  } else {
    body_ratio = 1.0f;
  }
  if (*(float *)(obj + 0x88) <= 0.0f) {
    shield_ratio = 0.0f;
  } else if (body_damage < *(float *)(obj + 0x88)) {
    shield_ratio = body_damage / *(float *)(obj + 0x88);
  } else {
    shield_ratio = 1.0f;
  }
  if (0.0f < *(float *)(obj + 0x94) && body_ratio <= 0.0f) {
    FUN_00136b40(datum_handle);
  }
  *(float *)(obj + 0x94) = body_ratio;
  if (0.0f < *(float *)(obj + 0x90) && shield_ratio <= 0.0f) {
    FUN_00137540(datum_handle);
  }
  *(float *)(obj + 0x90) = shield_ratio;
}

/* unit_test_spawning (0x1a9130)
 * Tests if unit should spawn attached objects on death. */
uint32_t unit_test_spawning(int unit_handle)
{
  uint32_t *unit;
  char *unit_tag;
  uint32_t flags;
  int *seed;
  int16_t count;

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  flags = unit[0x6d];
  if ((flags & 0x20000) == 0) {
    unit_tag = (char *)tag_get(0x756e6974, *unit);
    if (*(int *)(unit_tag + 0x258) != -1) {
      seed = get_global_random_seed_address();
      count = random_range((unsigned int *)seed,
          *(int16_t *)(unit_tag + 0x25c),
          (int16_t)(*(int16_t *)(unit_tag + 0x25e) + 1));
      if (count > 0) {
        FUN_0003f350(unit_handle, *(int *)(unit_tag + 0x258), count,
                     *(float *)(unit_tag + 0x260) * *(float *)0x2546a4);
      }
      unit[0x6d] = unit[0x6d] | 0x20000;
      return flags;
    }
  }
  return flags & 0xffff0000;
}

/* scripting_set_magic_base_seat (0x1ae730)
 * Sets the global magic base seat from a seat name string. */
void scripting_set_magic_base_seat(const char *param_1)
{
  *(int16_t *)0x32de80 = FUN_001ab730(param_1);
}

/* FUN_001a7790 (0x1a7790)
 * Updates the unit's dialogue/speech state machine each tick. */
void FUN_001a7790(int param_1)
{
  int16_t sVar1;
  char *unit;
  int16_t marker_count;
  char marker_buf[0x6c];
  float position[3];
  float forward[3];

  unit = (char *)object_get_and_verify_type(param_1, 3);

  if ((*(uint32_t *)(unit + 0x1b4) & 0x100) != 0) {
    FUN_001a7730(param_1);
    *(uint32_t *)(unit + 0x1b4) &= ~0x100u;
  }

  if (*(int16_t *)(unit + 0x398) > 0) {
    sVar1 = *(int16_t *)(unit + 0x398) - 1;
    *(int16_t *)(unit + 0x398) = sVar1;
    if (sVar1 == 0 && *(int16_t *)(unit + 0x39a) > 0) {
      *(int16_t *)(unit + 0x39a) -= 1;
      *(int16_t *)(unit + 0x398) = 0x16;
    }
  }
  if (*(int16_t *)(unit + 0x39c) > 0) {
    *(int16_t *)(unit + 0x39c) -= 1;
  }
  if (*(int16_t *)(unit + 0x39c) > 0) {
    *(int16_t *)(unit + 0x39c) -= 1;
  }

  if (*(int16_t *)(unit + 0x338) > 0) {
    if (*(int16_t *)(unit + 0x3a8) > 0) {
      *(int16_t *)(unit + 0x3a8) -= 1;
    } else {
      if (*(uint8_t *)(unit + 0x3a4) == 0) {
        marker_count = object_get_markers_by_string_id(
            param_1, (void *)0x2909e4, marker_buf, 1);
        if (marker_count == 0) {
          position[0] = (*(float **)0x31fc1c)[0];
          position[1] = (*(float **)0x31fc1c)[1];
          position[2] = (*(float **)0x31fc1c)[2];
          forward[0] = (*(float **)0x31fc3c)[0];
          forward[1] = (*(float **)0x31fc3c)[1];
          forward[2] = (*(float **)0x31fc3c)[2];
          *(int16_t *)marker_buf = 0;
        } else {
          position[0] = *(float *)(marker_buf + 0x2c);
          position[1] = *(float *)(marker_buf + 0x30);
          position[2] = *(float *)(marker_buf + 0x34);
          forward[0] = *(float *)(marker_buf + 0x08);
          forward[1] = *(float *)(marker_buf + 0x0c);
          forward[2] = *(float *)(marker_buf + 0x10);
        }
        if (*(int *)(unit + 0x33c) != -1) {
          *(int *)(unit + 0x3b0) = object_impulse_sound_new(
              param_1, *(int *)(unit + 0x33c), *(int16_t *)marker_buf,
              position, forward, 1.0f);
        }
        FUN_00044fd0(param_1,
            *(uint16_t *)(unit + 0x338), *(uint16_t *)(unit + 0x33a),
            unit + 0x348);
        *(uint8_t *)(unit + 0x3a4) = 1;
      }
      if (*(int16_t *)(unit + 0x3ac) > 0) {
        *(int16_t *)(unit + 0x3ac) -= 1;
      }
      if (*(int16_t *)(unit + 0x3aa) > 0) {
        if (*(int16_t *)(unit + 0x338) < 1) {
          display_assert(
              "unit->unit.speech.current.priority > _unit_speech_none",
              "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x2f5, 1);
          system_exit(-1);
        }
        *(int16_t *)(unit + 0x3aa) -= 1;
        if (*(int16_t *)(unit + 0x3aa) == 0) {
          *(int *)(unit + 0x3b0) = -1;
        }
      } else {
        if (*(uint8_t *)(unit + 0x3a6) == 0) {
          FUN_00046530(param_1,
              *(uint16_t *)(unit + 0x338), *(uint16_t *)(unit + 0x33a),
              0, -1, unit + 0x348);
          *(uint8_t *)(unit + 0x3a6) = 1;
        }
        if (*(int16_t *)(unit + 0x3ae) > 0) {
          *(int16_t *)(unit + 0x3ae) -= 1;
        }
        if (*(int16_t *)(unit + 0x3ae) == 0) {
          *(int16_t *)(unit + 0x3ac) = 0;
        }
      }
    }
  }

  if (*(int16_t *)(unit + 0x3ac) == 0 && *(uint8_t *)(unit + 0x3a5) == 0) {
    FUN_00045290(param_1,
        *(uint16_t *)(unit + 0x338), *(uint16_t *)(unit + 0x33a),
        unit + 0x348);
    *(uint8_t *)(unit + 0x3a5) = 1;
  }

  sVar1 = *(int16_t *)(unit + 0x338);
  if (sVar1 > 0) {
    if (*(int16_t *)(unit + 0x3aa) == 0 && *(int16_t *)(unit + 0x3ae) == 0) {
      *(int16_t *)(unit + 0x338) = 0;
    }
    sVar1 = *(int16_t *)(unit + 0x338);
  }
  if (sVar1 == 0 && *(int16_t *)(unit + 0x368) > 0) {
    FUN_001a6ef0(param_1, 3, unit + 0x368);
  }
}

/* FUN_001a6bf0 (0x1a6bf0)
 * Selects dialogue variant for the unit if none is set. */
void FUN_001a6bf0(int unit_handle)
{
  uint32_t *unit;
  char *unit_tag;
  int *dialogue_block;
  int16_t *variant;
  uint16_t count;
  int16_t i;
  int16_t variants[16];

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  if (*(int16_t *)((char *)unit + 0x6e) == 0) {
    dialogue_block = (int *)(unit_tag + 0x2b4);
    count = 0;
    i = 0;
    if (0 < *dialogue_block) {
      do {
        variant = (int16_t *)tag_block_get_element(dialogue_block, (int)i, 0x18);
        if (*variant < 100) {
          if (count >= 0xf) {
            error(2, "unit_dialogue_determine_variant overflowed variant array");
            break;
          }
          variants[(int16_t)count] = *variant;
          count++;
        }
        i++;
      } while ((int)i < *dialogue_block);
      if ((int16_t)count > 0) {
        *(int16_t *)((char *)unit + 0x6e) =
            variants[*(int *)0x4e4cf4 % (int)(int16_t)count];
        *(int *)0x4e4cf4 = *(int *)0x4e4cf4 + 1;
      }
    }
  }
}

/* FUN_001a70d0 (0x1a70d0)
 * Initiates direct sound speech on a unit (used for scripted dialogue). */
void FUN_001a70d0(int unit_handle, int sound_tag, int sound_handle)
{
  char *unit;
  int result;
  char speech_buf[0x30];
  int local_8;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  local_8 = -1;
  result = FUN_001a68d0(unit_handle, 6, 0, 0, 0, (int16_t *)&local_8, &result);
  if ((int16_t)result < 3) {
    result = 2;
  }
  csmemset(speech_buf, 0, 0x30);
  *(int16_t *)(speech_buf + 0x00) = 6;
  *(int16_t *)(speech_buf + 0x02) = -1;
  *(int *)(speech_buf + 0x04) = sound_tag;
  *(int16_t *)(speech_buf + 0x0c) = 0x18;
  ai_communication_packet_new(speech_buf + 0x10);
  FUN_001a6ef0(unit_handle, (int16_t)result, speech_buf);
  if (*(int *)(unit + 0x33c) != sound_tag) {
    display_assert(
        "unit->unit.speech.current.sound_definition_index == sound_definition_index",
        "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x196, 1);
    system_exit(-1);
  }
  *(int *)(unit + 0x3b0) = sound_handle;
  *(uint8_t *)(unit + 0x3a4) = 1;
  *(int16_t *)(unit + 0x3a8) = 0;
  FUN_00044fd0(unit_handle, 6, 0xffff, unit + 0x348);
}

/* unit_export_function_values (0x1a8010)
 * Exports 4 animation function values from unit tag to unit+0xD4. */
void unit_export_function_values(int unit_handle)
{
  uint32_t *unit;
  char *unit_tag;
  float fVar1;
  float *pfVar4;
  int16_t *psVar5;
  char *anim_tag;
  char *anim_entry;
  int local_8;

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  pfVar4 = (float *)(unit + 0x35);
  psVar5 = (int16_t *)(unit_tag + 0x198);
  local_8 = 4;
  do {
    if (*psVar5 != 0) {
      fVar1 = 0.0f;
      switch (*psVar5) {
      case 1:
        fVar1 = *(float *)(unit + 0xba);
        break;
      case 2:
        fVar1 = *(float *)(unit + 0xbb);
        break;
      case 3:
        fVar1 = (float)*(uint8_t *)((char *)unit + 0x2d3) * *(float *)0x261518;
        break;
      case 4:
        fVar1 = *(float *)(unit + 0xa6);
        break;
      case 5:
        fVar1 = *(float *)(unit + 0xbc);
        break;
      case 6:
        if ((*(uint8_t *)((char *)unit + 0xb6) & 4) == 0 &&
            (unit[0x6d] & 0x400000) == 0) {
          fVar1 = *(float *)0x2533c8;
        }
        break;
      case 7:
        anim_tag = (char *)tag_get(0x616e7472, unit[0x1f]);
        anim_entry = (char *)tag_block_get_element(
            (int *)(anim_tag + 0x74),
            (int)*(int16_t *)(unit + 0x20), 0xb4);
        if (*(int16_t *)(unit + 0x20) < *(int16_t *)(anim_entry + 0x2e)) {
          fVar1 = (float)(int)*(int16_t *)(unit + 0x20) /
                  (float)(int)*(int16_t *)(anim_entry + 0x2e);
        } else {
          fVar1 = *(float *)0x2533c8 -
                  (float)(int)*(int8_t *)((char *)unit + 0x1be) * *(float *)0x26f2e0;
        }
        break;
      }
      *pfVar4 = fVar1;
    }
    psVar5++;
    pfVar4++;
    local_8--;
  } while (local_8 != 0);
}

/* unit_get_melee_range_and_ticks (0x1a83e0)
 * Gets melee attack animation timing data. */
char unit_get_melee_range_and_ticks(int unit_handle, char is_secondary,
    int *out_tick_count, float *out_attack_time, int16_t *out_frame_count,
    float *out_damage_time)
{
  uint32_t *unit;
  char *unit_tag;
  char *anim_tag;
  char *anim_set;
  char *anim_mode;
  int anim_index;
  int16_t anim_id;

  unit = (uint32_t *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *unit);
  tag_get(0x6d6f6465, *(int *)(unit_tag + 0x34));
  anim_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  anim_set = (char *)tag_block_get_element(
      (int *)(anim_tag + 0xc), (int)*(int8_t *)(unit + 0x94), 100);
  anim_mode = (char *)tag_block_get_element(
      (int *)(anim_set + 0x58), (int)*(int8_t *)((char *)unit + 0x251), 0xbc);
  anim_index = (-(uint32_t)(is_secondary != '\0') & 3) + 0x27;
  if (anim_index < *(int *)(anim_mode + 0x98)) {
    anim_id = *(int16_t *)(*(int *)(anim_mode + 0x9c) + anim_index * 2);
  } else {
    anim_id = -1;
  }
  if (anim_id == -1) {
    return 0;
  }
  anim_tag = (char *)tag_block_get_element(
      (int *)(anim_tag + 0x74), (int)anim_id, 0xb4);
  FUN_00120710((int)anim_tag, (int)out_attack_time, (int)out_damage_time);
  if (out_tick_count != NULL) {
    *out_tick_count = (int)*(int16_t *)(anim_tag + 0x34);
  }
  if (out_frame_count != NULL) {
    *out_frame_count = *(int16_t *)(anim_tag + 0x22);
  }
  return 1;
}

/* unit_set_seat (0x1ae1e0)
 * Attempts to set the unit's seat via animation lookup. */
char unit_set_seat(int unit_handle, int seat_name)
{
  return FUN_001acd70(unit_handle, (const char *)seat_name, 0, 1) != '\0';
}

/* unit_start_flaming_to_death (0x1af2a0)
 * Initiates the flaming-to-death state for a unit. */
void unit_start_flaming_to_death(int unit_handle, int param_2)
{
  char *unit;
  int *seed;
  int16_t ticks;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_set_in_vehicle(unit_handle, 1);
  *(uint32_t *)(unit + 0x1b4) |= 0x80;
  *(uint16_t *)(unit + 0xb6) = (*(uint16_t *)(unit + 0xb6) & ~0x4u) | 0x800;
  if (*(char *)(unit + 0x23b) == '\0') {
    seed = get_global_random_seed_address();
    ticks = random_range((unsigned int *)seed, 0x3c, 0x96);
    if (ticks < 1) {
      ticks = 1;
    } else if (ticks > 0xff) {
      ticks = 0xff;
    }
    *(char *)(unit + 0x23b) = (char)ticks;
    *(int *)(unit + 0x3c0) = param_2;
    unit_start_running_blindly(unit_handle);
  }
}

/* unit_handle_region_destroyed (0x1abcd0)
 * Triggers scream when a body region is destroyed. */
void unit_handle_region_destroyed(int unit_handle, int param_2, uint32_t flags)
{
  char *unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if ((*(uint8_t *)(unit + 0xb6) & 4) == 0) {
    FUN_001a74d0(unit_handle, ((flags & 0x200) != 0) + 3);
  }
}


/* unit_aiming_vector (0x1ab410)
 * Computes the unit's aiming vector (offset 0x320, 3 floats) from position
 * deltas transformed by the orientation matrix, scaled by tag sensitivity,
 * offset by 0.5, clamped to [0.0, 0.7]. Updates origin (0x2FC) and delta
 * (0x308). Uses seat marker if in a vehicle; early-exits with 0.5 if marker
 * not found. Register arg: unit_handle in EAX. */
void unit_aiming_vector(int unit_handle)
{
  char *unit;
  int parent_handle;
  char *parent_unit;
  int tag_data;
  int seat_entry;
  float *sensitivity;
  int16_t marker_result;
  char marker_buf[60];
  float pos[3];
  float fwd[3];
  float up[3];
  float side[3];
  float dx, dy, dz;
  float ddx, ddy, ddz;
  float val;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  parent_handle = *(int *)(unit + 0xcc);

  if (parent_handle == -1 || *(int16_t *)(unit + 0x2a0) == -1) {
    /* No parent or no seat -- use unit's own transform */
    tag_data = (int)tag_get(0x756e6974, *(int *)unit);
    pos[0] = *(float *)(unit + 0x0c);
    pos[1] = *(float *)(unit + 0x10);
    pos[2] = *(float *)(unit + 0x14);
    fwd[0] = *(float *)(unit + 0x24);
    fwd[1] = *(float *)(unit + 0x28);
    fwd[2] = *(float *)(unit + 0x2c);
    up[0] = *(float *)(unit + 0x30);
    up[1] = *(float *)(unit + 0x34);
    up[2] = *(float *)(unit + 0x38);
    sensitivity = (float *)(tag_data + 0x200);
  } else {
    /* In a vehicle seat -- use parent marker transform */
    parent_unit = (char *)object_get_and_verify_type(parent_handle, 3);
    tag_data = (int)tag_get(0x756e6974, *(int *)parent_unit);
    seat_entry = (int)tag_block_get_element(
        (void *)(tag_data + 0x2e4),
        (int)*(int16_t *)(unit + 0x2a0), 0x11c);
    marker_result = object_get_markers_by_string_id(
        parent_handle, (void *)(seat_entry + 0x24), marker_buf, 1);
    if (marker_result == 0) {
      *(float *)(unit + 0x320) = 0.5f;
      *(float *)(unit + 0x324) = 0.5f;
      *(float *)(unit + 0x328) = 0.5f;
      return;
    }
    object_get_world_position(parent_handle, (vector3_t *)pos);
    sensitivity = (float *)(seat_entry + 0x64);
  }

  /* Delta from previous aiming origin */
  dx = pos[0] - *(float *)(unit + 0x2fc);
  dy = pos[1] - *(float *)(unit + 0x300);
  dz = pos[2] - *(float *)(unit + 0x304);

  /* Double-delta: subtract previous delta */
  ddx = dx - *(float *)(unit + 0x308);
  ddy = dy - *(float *)(unit + 0x30c);
  ddz = dz - *(float *)(unit + 0x310);

  /* side = up x fwd (verified from FSUBP order in disasm at 0x1ab537-0x1ab55f) */
  side[0] = fwd[2] * up[1] - fwd[1] * up[2];
  side[1] = up[2] * fwd[0] - fwd[2] * up[0];
  side[2] = up[0] * fwd[1] - up[1] * fwd[0];

  /* Project double-delta onto orientation axes, scale by sensitivity, +0.5 */
  *(float *)(unit + 0x320) =
      (fwd[0] * ddx + fwd[1] * ddy + fwd[2] * ddz) * sensitivity[0] + 0.5f;
  *(float *)(unit + 0x324) =
      (side[0] * ddx + side[1] * ddy + side[2] * ddz) * sensitivity[1] + 0.5f;
  *(float *)(unit + 0x328) =
      (up[0] * ddx + up[1] * ddy + up[2] * ddz) * sensitivity[2] + 0.5f;

  /* Clamp each component to [0.0, 0.7] */
  val = *(float *)(unit + 0x320);
  if (val < 0.0f)
    val = 0.0f;
  else if (val > 0.7f)
    val = 0.7f;
  *(float *)(unit + 0x320) = val;

  val = *(float *)(unit + 0x324);
  if (val < 0.0f)
    val = 0.0f;
  else if (val > 0.7f)
    val = 0.7f;
  *(float *)(unit + 0x324) = val;

  val = *(float *)(unit + 0x328);
  if (val < 0.0f)
    val = 0.0f;
  else if (val > 0.7f)
    val = 0.7f;
  *(float *)(unit + 0x328) = val;

  /* Update aiming origin */
  *(float *)(unit + 0x2fc) = pos[0];
  *(float *)(unit + 0x300) = pos[1];
  *(float *)(unit + 0x304) = pos[2];

  /* Update aiming delta */
  *(float *)(unit + 0x308) = dx;
  *(float *)(unit + 0x30c) = dy;
  *(float *)(unit + 0x310) = dz;
}

/* unit_drop_grenades_on_death (0x1abb20)
 * Creates grenade weapon objects for each grenade the unit carries and drops
 * them. Iterates over 2 grenade types. For each, looks up the grenade tag
 * from game globals, creates weapon objects, disconnects from map, then
 * detaches (drops physically). Register arg: unit_handle in EAX. */
void unit_drop_grenades_on_death(int unit_handle)
{
  char *unit;
  char *grenade_count_ptr;
  int grenade_type;
  int globals;
  int grenade_tag;
  int new_handle;
  char placement[136];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  grenade_count_ptr = unit + 0x2ce;

  /* The binary uses a negative-offset trick to compute the grenade type index:
   * ESI = 0xFFFFFD32 - unit_addr; index = ESI + grenade_count_ptr
   * Since 0xFFFFFD32 + 0x2CE = 0 (mod 2^32), this yields index = type (0 or 1).
   * We compute the type directly. */
  for (grenade_type = 0; grenade_type < NUMBER_OF_UNIT_GRENADE_TYPES; grenade_type++) {
    globals = (int)game_globals_get();
    grenade_tag = (int)tag_block_get_element(
        (void *)(globals + 0x128), grenade_type, 0x44);

    while (*grenade_count_ptr > 0) {
      object_placement_data_new(placement, *(int *)(grenade_tag + 0x30), unit_handle);
      new_handle = object_new(placement);
      if (new_handle != -1) {
        object_disconnect_from_map(new_handle);
        unit_detach_weapon(unit_handle, new_handle);
      }
      *grenade_count_ptr = *grenade_count_ptr - 1;
    }
    grenade_count_ptr++;
  }
}

/* unit_drop_weapons_on_death (0x1abbd0)
 * Iterates over all 4 weapon slots and drops weapons not connected to the
 * map. For map-connected weapons, asserts with an error message. Updates
 * the next-weapon index if needed, clears the slot, and deletes the weapon
 * if it cannot be fired. Register arg: unit_handle in EAX. */
void unit_drop_weapons_on_death(int unit_handle)
{
  char *unit;
  int weapon_handle;
  char *weapon_data;
  int slot;
  int *weapon_slot_ptr;
  const char *unit_name;
  const char *weapon_name;
  const char *msg;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  slot = 0;
  weapon_slot_ptr = (int *)(unit + 0x2a8);

  do {
    weapon_handle = *weapon_slot_ptr;
    if (weapon_handle != -1 &&
        (int16_t)slot != *(int16_t *)(unit + 0x2a2)) {
      weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
      if ((*(uint32_t *)(weapon_data + 4) & 0x800) != 0) {
        weapon_name = tag_get_name(*(int *)weapon_data);
        unit_name = tag_get_name(*(int *)unit);
        msg = csprintf(error_string_buffer,
            "a %s tried to drop a %s which was connected to the map.",
            unit_name, weapon_name);
        display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x2132, 1);
        system_exit(-1);
      }
      unit_detach_weapon(unit_handle, weapon_handle);
      if ((int16_t)slot == *(int16_t *)(unit + 0x2a4)) {
        *(int16_t *)(unit + 0x2a4) = *(int16_t *)(unit + 0x2a2);
      }
      *weapon_slot_ptr = -1;
      if (!weapon_can_be_fired(weapon_handle)) {
        object_delete(weapon_handle);
      }
    }
    slot++;
    weapon_slot_ptr++;
  } while ((int16_t)slot < 4);
}

/* unit_get_weapon_name (0x1ae700)
 * Returns the name string of the unit's currently selected weapon.
 * If no weapon is equipped, returns "unarmed".
 * Register arg: unit_handle in ESI.
 * Stack arg: 1 cdecl param (unused in function body, always 1 from callers). */
char *unit_get_weapon_name(int unit_handle, int unused)
{
  char *unit;
  int weapon_handle;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle,
      *(int16_t *)(unit + 0x2a2));
  if (weapon_handle == -1) {
    return "unarmed";
  }
  return weapon_get_label(weapon_handle);
}

/* unit_has_night_vision_weapon (0x1b13a0)
 * Checks whether the unit's current weapon has the night-vision flag
 * (bit 0x4000 at weapon tag offset 0x308). Returns true if zoom_level
 * is not 0xFF, a weapon exists, and the flag is set.
 * Register arg: unit_handle in ESI. */
char unit_has_night_vision_weapon(int unit_handle)
{
  char *unit;
  int weapon_handle;
  char *weapon_data;
  int tag_data;
  char result;

  result = 0;
  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  if (*(uint8_t *)(unit + 0x2d0) == 0xff) {
    return result;
  }
  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon_handle = unit_get_weapon(unit_handle,
      *(int16_t *)(unit + 0x2a2));
  if (weapon_handle == -1) {
    return result;
  }
  weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
  tag_data = (int)tag_get(0x77656170, *(int *)weapon_data);
  if (*(uint32_t *)(tag_data + 0x308) & 0x4000) {
    return 1;
  }
  return result;
}

/* unit_solo_player_integrated_night_vision_is_active (0x1b2610)
 * Returns true if there is exactly one local player, that player has a
 * valid unit, and that unit's current weapon has the night-vision flag. */
char unit_solo_player_integrated_night_vision_is_active(void)
{
  int16_t count;
  int16_t local_idx;
  int player_index;
  char *player;
  int unit_handle;
  char result;

  result = 0;
  count = local_player_count();
  if (count != 1) {
    return result;
  }
  local_idx = local_player_get_next(-1);
  player_index = local_player_get_player_index(local_idx);
  if (player_index == -1) {
    return result;
  }
  player = (char *)datum_get(player_data, player_index);
  unit_handle = *(int *)(player + 0x34);
  if (unit_handle == -1) {
    return result;
  }
  return unit_has_night_vision_weapon(unit_handle);
}

/* scripting_magic_melee_attack (0x1b2260)
 * Triggers a melee attack on the first player's unit. Gets player 0's
 * unit handle from player_data, then calls unit_melee_attack_begin. */
void scripting_magic_melee_attack(void)
{
  char *player;
  int unit_handle;

  player = (char *)datum_get(player_data, 0);
  unit_handle = *(int *)(player + 0x34);
  unit_melee_attack_begin(unit_handle, 0, 0);
}

/* unit_select_weapon_after_vehicle_exit (0x1b2740)
 * After exiting a vehicle, selects the next available weapon and updates
 * weapon readiness. Reads current weapon index, finds the next weapon,
 * stores it as the next weapon index, then calls unit_update_weapon_readiness.
 * Register arg: unit_handle in EAX. */
void unit_select_weapon_after_vehicle_exit(int unit_handle)
{
  char *unit;
  uint16_t current_idx;
  int16_t next_idx;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  current_idx = *(uint16_t *)(unit + 0x2a2);
  next_idx = unit_next_weapon_index(unit_handle, (int16_t)current_idx, 0);
  *(int16_t *)(unit + 0x2a4) = next_idx;
  unit_update_weapon_readiness(unit_handle, 1);
}

/* FUN_001abd10 (0x1abd10)
 * Plays impact sounds for melee damage. Looks up the unit's material type
 * sound via FUN_0018e500 and plays it on the unit. If a damage effect tag is
 * provided, also plays the effect's melee impact sound (tag 'jpt!'+0x120).
 * Register args: @eax = material_type, @esi = unit_handle,
 *                @edi = damage_effect_tag (or -1).
 * Confirmed from callers 0x1ae840 @001aea76, 0x1aea90 @001af016. */
void FUN_001abd10(int16_t material_type, int unit_handle, int weapon_tag_index)
{
  char *material_effects;
  int sound_tag;
  char *weapon_tag;
  float *position;
  float *forward;

  position = *(float **)0x31fc1c;
  forward = *(float **)0x31fc3c;

  material_effects = (char *)FUN_0018e500(material_type);
  sound_tag = *(int *)(material_effects + 0x370);
  if (sound_tag != -1) {
    object_impulse_sound_new(unit_handle, sound_tag, -1, position, forward,
                             1.0f);
  }

  if (weapon_tag_index != -1) {
    weapon_tag = (char *)tag_get(0x6a707421, weapon_tag_index);
    sound_tag = *(int *)(weapon_tag + 0x120);
    if (sound_tag != -1) {
      object_impulse_sound_new(unit_handle, sound_tag, -1, position, forward,
                               1.0f);
    }
  }
}

/* unit_flame_to_death (0x1ac550)
 * Handles the flame-to-death damage effect when a unit's flame-death
 * timer expires. Clears flame flags on the unit, looks up the flame damage
 * effect tag from game globals, and applies it. If the unit doesn't die,
 * logs a warning and sets a fallback flag.
 * cdecl: 1 stack param (unit_handle). */
void unit_flame_to_death(int unit_handle)
{
  char *unit;
  char *globals;
  char *element;
  char *object_data;
  char damage_params[0x54];
  char *parent_obj;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  globals = (char *)game_globals_get();
  element = (char *)tag_block_get_element(globals + 0x188, 0, 0x98);

  object_data = (char *)object_get_and_verify_type(unit_handle, 3);
  *(uint32_t *)(object_data + 0x1b4) &= ~0x02000000u;

  *(uint8_t *)(unit + 0xb7) &= ~0x08;
  *(uint32_t *)(unit + 0x1b4) &= ~0x80u;

  if (element != NULL && *(int *)(element + 0x78) != -1) {
    parent_obj = (char *)object_try_and_get_and_verify_type(
        *(int *)(unit + 0x3c0), -1);

    damage_data_new(damage_params, *(int *)(element + 0x78));

    if (parent_obj != NULL) {
      *(int *)(damage_params + 0x08) = *(int *)(parent_obj + 0x70);
      {
        int cause_player = *(int *)(parent_obj + 0x74);
        if (cause_player == -1) {
          cause_player = *(int *)(unit + 0x3c0);
        }
        *(int *)(damage_params + 0x0c) = cause_player;
      }
      *(int16_t *)(damage_params + 0x10) = *(int16_t *)(parent_obj + 0x68);
    }

    object_cause_damage(damage_params, unit_handle, -1, -1, -1, 0);
  }

  if ((*(uint8_t *)(unit + 0xb6) & 0x4) == 0) {
    const char *tag_name;
    const char *stripped;
    tag_name = tag_get_name(*(int *)unit);
    stripped = tag_name_strip_path(tag_name);
    error(2, "WARNING: %s tried to die from flaming to death but couldn't",
          stripped);
    *(uint8_t *)(unit + 0xb6) |= 0x20;
  }
}

/* FUN_001ab110 (0x1ab110)
 * Grenade throw release. Detaches the held grenade from the unit, computes
 * its throw velocity using the unit's forward vector and tag throw speed,
 * optionally applies random spread if the throw animation was cut short,
 * subtracts the grenade's world position to get relative velocity, and
 * attempts to place it.
 * cdecl: 2 stack params (unit_handle, flag).
 * Confirmed from callers 0x1ad260, 0x1b0d90, 0x1b1400, 0x1b3690. */
void FUN_001ab110(int unit_handle, char flag)
{
  char *unit;
  char *unit_tag;
  int grenade_handle;
  int ebx;
  char *throw_params;
  float forward[3];
  float cross_fwd[3];
  float cross2[3];
  float velocity[3];
  float seat_pos[3];
  float ratio_val;
  float throw_speed;
  float rand_val;
  float rand_x;
  float rand_y;
  float rand_z;
  float one_minus;
  char *grenade_data;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

  if (*(uint8_t *)(unit + 0x23d) != 2) {
    return;
  }

  grenade_handle = *(int *)(unit + 0x244);
  ebx = -1;
  if (grenade_handle == ebx) {
    return;
  }

  /* Detach grenade from parent */
  object_detach_from_parent(grenade_handle);

  if (*(int *)(unit + 0x1a4) != ebx) {
    /* Actor-controlled: get world position and transform via actor */
    object_get_world_position(*(int *)(unit + 0x244), (void *)cross2);
    actor_aim_grenade(*(int *)(unit + 0x1a4), cross2, velocity);
  } else {
    if (*(int *)(unit + 0x1c8) != ebx) {
      /* Player-controlled with weapon: compute velocity from marker */
      char *globals;
      globals = (char *)game_globals_get();
      throw_params = (char *)tag_block_get_element(
          globals + 0x170, 0, 0xf4);

      /* Get unit's forward vector */
      forward[0] = *(float *)(unit + 0x1ec);
      forward[1] = *(float *)(unit + 0x1f0);
      forward[2] = *(float *)(unit + 0x1f4);

      {
        float *up_ptr = *(float **)0x31fc44;
        float mag;
        cross_product3d(up_ptr, forward, cross_fwd);
        mag = normalize3d(cross_fwd);
        if (mag == 0.0f) {
          cross_fwd[0] = up_ptr[0];
          cross_fwd[1] = up_ptr[1];
          cross_fwd[2] = up_ptr[2];
        }
      }

      cross_product3d(forward, cross_fwd, cross2);
      normalize3d(cross2);

      /* Get the unit's seat/marker position */
      unit_set_seat_state(unit_handle, seat_pos);

      /* Apply throw direction offsets from throw params */
      {
        float fwd_s = *(float *)(throw_params + 0x68);
        float right_s = *(float *)(throw_params + 0x6c);
        float up_s = *(float *)(throw_params + 0x70);

        seat_pos[0] += forward[0] * fwd_s + cross_fwd[0] * right_s
                       + cross2[0] * up_s;
        seat_pos[1] += forward[1] * fwd_s + cross_fwd[1] * right_s
                       + cross2[1] * up_s;
        seat_pos[2] += forward[2] * fwd_s + cross_fwd[2] * right_s
                       + cross2[2] * up_s;
      }

      object_translate(grenade_handle, seat_pos, 0);
      ebx = -1;
    }

    /* Compute throw velocity from unit tag grenade speed and forward */
    throw_speed = *(float *)(unit_tag + 0x2c0) * *(float *)0x2546a4;
    velocity[0] = throw_speed * *(float *)(unit + 0x1ec);
    velocity[1] = throw_speed * *(float *)(unit + 0x1f0);
    velocity[2] = throw_speed * *(float *)(unit + 0x1f4);
  }

  /* Apply random spread if throw timer is short */
  if (flag != 0) {
    int throw_timer = (int)*(int16_t *)(unit + 0x23e);
    int throw_total = (int)*(int16_t *)(unit + 0x240);
    ratio_val = (float)throw_timer / (float)throw_total;
    if (ratio_val < 1.0f) {
      rand_val = random_real_range(
          get_global_random_seed_address(), 0.02f, 0.046875f);
      rand_x = rand_val * *(float *)(unit + 0x1ec);
      rand_y = rand_val * *(float *)(unit + 0x1f0);
      rand_z = rand_val * *(float *)(unit + 0x1f4);

      velocity[0] = velocity[0] * ratio_val;
      velocity[1] = velocity[1] * ratio_val;
      velocity[2] = velocity[2] * ratio_val;

      one_minus = 1.0f - ratio_val;
      velocity[0] = rand_x * one_minus + velocity[0];
      velocity[1] = rand_y * one_minus + velocity[1];
      velocity[2] = rand_z * one_minus + velocity[2];
    }
  }

  /* Subtract grenade's current world position to get relative velocity */
  grenade_data = (char *)object_get_and_verify_type(grenade_handle, ebx);
  velocity[0] -= *(float *)(grenade_data + 0x18);
  velocity[1] -= *(float *)(grenade_data + 0x1c);
  velocity[2] -= *(float *)(grenade_data + 0x20);

  projectile_accelerate(grenade_handle, velocity);

  /* Mark grenade as released */
  *(uint8_t *)(unit + 0x23d) = 3;
  *(int *)(unit + 0x244) = -1;

  /* Try to place the grenade; delete on failure */
  unit_set_seat_state(unit_handle, cross2);
  if (object_try_place(grenade_handle, cross2) == 0) {
    object_delete(grenade_handle);
  }
}

/* FUN_001a6280 (0x1a6280)
 * Biped death state handler. After a biped is killed, decides which
 * post-death sub-state to enter: limp-noodle (ragdoll-like), dying-airborne
 * (fell while dying), or normal dying. Checks limp-noodle counter, airborne
 * ticks, and animation state.
 * Register args: @edi = unit_handle, @ebx = pointer to state byte pair.
 * Confirmed from caller 0x1a6350 @001a65ed. */
void FUN_001a6280(int unit_handle, char *state_out)
{
  char *biped;
  char *biped_tag;

  biped = (char *)object_get_and_verify_type(unit_handle, 1);
  biped_tag = (char *)tag_get(0x62697064, *(int *)biped);

  /* Check limp-noodle state */
  if ((*(uint8_t *)(biped + 0x424) & 0x20) != 0 &&
      *(uint8_t *)(biped + 0x47c) < *(uint8_t *)(biped + 0x47d)) {
    if (*(char *)0x4e4cf3 == 0) {
      FUN_001a0680(unit_handle);
    }
    FUN_001a2800(unit_handle, "post-limp-noodle");
    state_out[1] = 0;
    return;
  }

  /* Check dying-airborne state */
  if (*(int8_t *)(biped + 0x459) > 2 &&
      (*(uint32_t *)(biped_tag + 0x2f4) & 0x400) == 0) {
    if (*(uint8_t *)(biped + 0x253) == 0x18) {
      FUN_001a2160(unit_handle);
    }
    *state_out = 0x18;
    FUN_001a2800(unit_handle, "post-dying-airborne");
    state_out[1] = 0;
    return;
  }

  /* Normal dying state */
  if (*(uint8_t *)(biped + 0x253) == 0x18) {
    *(int *)(biped + 0x468) = 0;
    FUN_001a4440(unit_handle);
  }
  *state_out = 0x19;
  FUN_001a2800(unit_handle, "post-dying");
  state_out[1] = 0;
}

/* FUN_001a68d0 (0x1a68d0) — unit dialogue speech slot allocation.
 *
 * Resolves a vocalization type to a sound definition index via the unit's
 * dialogue tag (udlg). Walks the vocalization fallback table at 0x2b6420
 * when the primary type yields no sound. Evaluates priority-based
 * preemption of the current speech slot against timing thresholds in the
 * global tables at 0x2b65c4 (short[11]) and 0x2b65dc (float[11]).
 *
 * Returns a result code:
 *   0 = no speech possible
 *   1 = interrupt current speech
 *   2 = immediate slot available or priority preemption
 *   3 = priority exceeds current threshold
 *
 * Source file: unit_dialogue.c, lines 0x80–0x82 asserts.
 */
short FUN_001a68d0(int unit_handle, short priority, char param_3, char param_4,
                   int *param_5, short *vocalization_type_ref,
                   int *sound_definition_index_ref)
{
  char *unit;
  int udlg_tag;
  short voc_type;
  int snd_def_idx;
  short slot_priority;
  short slot_secondary;
  short max_priority;
  int priority_int;
  short result;
  char bVar;
  float timing_threshold;
  short ftol_result;
  short *priority_table;
  float *timing_table;
  short *fallback_table;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  game_time_get();

  result = 0;

  if (vocalization_type_ref == NULL) {
    display_assert("vocalization_type_reference",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x80, 1);
    system_exit(-1);
  }
  if (sound_definition_index_ref == NULL) {
    display_assert("sound_definition_index_reference",
                   "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x81, 1);
    system_exit(-1);
  }
  if (priority < 0 || priority > 10) {
    display_assert(
      "(priority >= 0) && (priority < NUMBER_OF_UNIT_SPEECH_PRIORITIES)",
      "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x82, 1);
    system_exit(-1);
  }

  voc_type = *vocalization_type_ref;
  snd_def_idx = *sound_definition_index_ref;

  priority_table = (short *)0x2b65c4;
  timing_table = (float *)0x2b65dc;
  fallback_table = (short *)0x2b6420;

  /* Walk the dialogue tag's vocalization table with fallback chain */
  if (snd_def_idx == NONE && *(int *)(unit + 0x334) != NONE && voc_type != -1) {
    udlg_tag = (int)tag_get(0x75646c67, *(int *)(unit + 0x334));
    do {
      if (voc_type < 0 || voc_type > 0xd0) {
        display_assert(
          "(vocalization_type >= 0) && (vocalization_type < "
          "NUMBER_OF_VOCALIZATION_TYPES)",
          "c:\\halo\\SOURCE\\units\\unit_dialogue.c", 0x90, 1);
        system_exit(-1);
      }
      snd_def_idx = *(int *)(udlg_tag + (int)voc_type * 0x10 + 0x1c);
    } while (param_3 != '\0' && snd_def_idx == NONE &&
             (game_connection() != 0 || *(char *)0x5ac9cd == '\0') &&
             (voc_type = fallback_table[voc_type], voc_type != -1));
  }

  /* Check if unit can speak (flag check and game connection) */
  if ((*(uint8_t *)(unit + 0xb6) & 4) != 0 && priority != 10) {
    goto done;
  }
  if (game_connection() == 0 && *(char *)0x5ac9cd != '\0') {
    /* pass through */
  } else if (snd_def_idx == NONE) {
    goto done;
  }

  /* Evaluate speech slot priority */
  slot_priority = *(short *)(unit + 0x338);
  if (slot_priority == 0) {
    result = 2;
  } else {
    slot_secondary = *(short *)(unit + 0x368);
    max_priority = slot_priority;
    if (slot_priority <= slot_secondary) {
      max_priority = slot_secondary;
    }

    priority_int = (int)priority;

    /* High-priority interrupt check (priority 2, 7, or 10) */
    if ((priority_int == 2 || priority_int == 7 || priority_int == 10) &&
        *(char *)(unit + 0x3a4) != '\0' &&
        *(short *)(unit + 0x3aa) == 0 &&
        max_priority < priority) {
      slot_priority = 0;
      max_priority = slot_secondary;
    }

    if (priority_table[priority_int] >= max_priority) {
      result = 3;
    } else if (priority >= 7 && priority_table[priority_int] >= slot_priority) {
      result = 2;
    } else if (param_4 != '\0' &&
               timing_table[priority_int] != 0.0f) {
      timing_threshold = timing_table[priority_int];
      if (timing_threshold == *(float *)0x2548fc) {
        /* REAL_MAX => always interrupt */
        bVar = 1;
      } else {
        ftol_result = (short)(timing_threshold * 30.0f);
        bVar = (*(short *)(unit + 0x3ae) + *(short *)(unit + 0x3aa) <
                ftol_result) ? 1 : 0;
        if (!bVar) goto done;
      }
      if (priority <= max_priority) {
        if (priority <= *(short *)(unit + 0x368)) goto done;
        if (slot_priority == 2 || slot_priority == 7) {
          bVar = 1;
        }
        if (priority != 6 && !bVar) goto done;
      }
      result = 1;
    }
  }

done:
  *vocalization_type_ref = voc_type;
  *sound_definition_index_ref = snd_def_idx;
  if (param_5 != NULL) {
    *param_5 = *(int *)(unit + 0x3a0);
  }
  return result;
}

/* FUN_001ac680 (0x1ac680) — acceleration plan builder.
 *
 * Computes a piecewise acceleration/deceleration plan given initial
 * position, velocity, maximum velocity, and maximum acceleration.
 * The plan output structure at param_5 has layout:
 *   +0x00: bool  at_rest
 *   +0x04: float initial_p
 *   +0x08: float initial_v
 *   +0x0C: float accel_a
 *   +0x10: float accel_t
 *   +0x14: float coast_t
 *   +0x18: float decel_a
 *   +0x1C: float decel_t
 *
 * Recursive for the negative-velocity case (negates p and v, then
 * negates the plan output). Source file: units.c lines 0x7b7-0x860.
 */
void FUN_001ac680(float initial_p, float initial_v, float max_v,
                  float max_a, int plan)
{
  float fVar1;        /* |initial_v| / max_a */
  float fVar4;        /* discriminant / intermediate */
  float neg_max_a;
  float doubled_v;
  float t;
  float actual_t;
  float coasting_vel;
  float coast_diff;
  char bVar;          /* initial_v <= 0 */

  *(int *)(plan + 0x0c) = 0x7f7fffff;
  *(int *)(plan + 0x10) = 0x7f7fffff;
  *(int *)(plan + 0x14) = 0x7f7fffff;
  *(int *)(plan + 0x18) = 0x7f7fffff;
  *(int *)(plan + 0x1c) = 0x7f7fffff;
  *(float *)(plan + 4) = initial_p;
  *(float *)(plan + 8) = initial_v;

  /* Check if effectively at rest (MSVC intrinsic fabs → inline FABS) */
  if (fabs(initial_p) < 0.001 && fabs(initial_v) < 0.001) {
    *(uint8_t *)plan = 1;
    *(int *)(plan + 0x0c) = 0;
    *(int *)(plan + 0x10) = 0;
    *(int *)(plan + 0x14) = 0;
    *(int *)(plan + 0x18) = 0;
    *(int *)(plan + 0x1c) = 0;
    return;
  }
  *(uint8_t *)plan = 0;

  fVar1 = (float)(fabs(initial_v) / max_a);
  bVar = (initial_v > 0.0f) ? 1 : 0;

  /* Check if we're moving in the wrong direction (need to reverse) */
  if (fVar1 * 0.5f * initial_v * 0.5f + initial_p < 0.0f) {
    /* Recursive case: negate and re-plan */
    FUN_001ac680(-initial_p, -initial_v, max_v, max_a, plan);
    *(float *)(plan + 0x04) = *(float *)(plan + 0x04) * -1.0f;
    *(float *)(plan + 0x08) = *(float *)(plan + 0x08) * -1.0f;
    *(float *)(plan + 0x0c) = *(float *)(plan + 0x0c) * -1.0f;
    *(float *)(plan + 0x18) = *(float *)(plan + 0x18) * -1.0f;
    goto validate;
  }

  fVar4 = initial_v * 0.5f * fVar1 + initial_p;

  if (fVar4 < 0.0f) {
    /* Overshoot case: initial_p near zero, velocity away from target */
    if (initial_p <= -0.001f) {
      display_assert("plan->initial_p > -1e-03f",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x7b7, 1);
      system_exit(-1);
    }
    if (*(float *)(plan + 8) >= 0.0f) {
      display_assert("plan->initial_v < 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x7b8, 1);
      system_exit(-1);
    }
    *(int *)(plan + 0x0c) = 0;
    *(int *)(plan + 0x10) = 0;
    fVar4 = *(float *)(plan + 8) * *(float *)(plan + 8) /
            (*(float *)(plan + 4) + *(float *)(plan + 4));
    *(float *)(plan + 0x18) = fVar4;
    *(float *)(plan + 0x1c) = -(*(float *)(plan + 8) / fVar4);
    *(int *)(plan + 0x14) = 0;
    goto validate;
  }

  /* Normal deceleration case */
  if (!bVar) {
    /* Decelerating (initial_v <= 0): quadratic formula */
    neg_max_a = -max_a;
    doubled_v = initial_v + initial_v;
    fVar4 = doubled_v * doubled_v - neg_max_a * fVar4 * 4.0f;
    if (fVar4 < 0.0f) {
      display_assert("disc >= 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x7eb, 1);
      system_exit(-1);
    }
    fVar4 = sqrtf(fVar4);
    t = (-doubled_v - fVar4) / (neg_max_a + neg_max_a);
    actual_t = (fVar4 - doubled_v) / (neg_max_a + neg_max_a);
    if (t >= 0.0f && (actual_t < 0.0f || t < actual_t)) {
      /* use t */
    } else if (actual_t >= 0.0f) {
      t = actual_t;
    } else {
      t = 0.0f;
      goto check_t;
    }
    initial_v = t;
    goto check_t;
  } else {
    /* Accelerating (initial_v > 0): direct sqrt */
    initial_v = sqrtf(fVar4 / max_a);
  }

check_t:
  if (initial_v < 0.0f) {
    display_assert("t >= 0", "c:\\halo\\SOURCE\\units\\units.c", 0x7fa, 1);
    system_exit(-1);
  }

  /* Apply maximum velocity constraint */
  if (max_v <= 0.0f) {
    actual_t = initial_v;
  } else {
    if (!bVar) {
      max_v = max_v + *(float *)(plan + 8);
    }
    actual_t = max_v / max_a;
    if (actual_t < 0.0f) {
      actual_t = 0.0f;
    }
    if (initial_v <= actual_t) {
      actual_t = initial_v;
    }
  }

  /* Fill plan fields */
  *(float *)(plan + 0x0c) = -max_a;
  *(float *)(plan + 0x18) = max_a;
  if (bVar) {
    /* initial_v > 0: accel_t = actual_t + fVar1, decel_t = actual_t */
    *(float *)(plan + 0x10) = actual_t + fVar1;
    *(float *)(plan + 0x1c) = actual_t;
  } else {
    /* initial_v <= 0: accel_t = actual_t, decel_t = actual_t + fVar1 */
    *(float *)(plan + 0x1c) = actual_t + fVar1;
    *(float *)(plan + 0x10) = actual_t;
  }

  /* Check if we need a coasting phase */
  if (actual_t < initial_v) {
    coasting_vel = -max_a * *(float *)(plan + 0x10) + *(float *)(plan + 8);
    coast_diff = initial_v - actual_t;
    if (coasting_vel >= 0.0f) {
      display_assert("coasting_vel < 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x850, 1);
      system_exit(-1);
    }
    *(float *)(plan + 0x14) =
      ((coast_diff * coasting_vel + coast_diff * coasting_vel) -
       coast_diff * coast_diff * max_a) / coasting_vel;
    if (*(float *)(plan + 0x14) < 0.0f) {
      display_assert("plan->coast_t >= 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x852, 1);
      system_exit(-1);
    }
    if (actual_t + *(float *)(plan + 0x14) < initial_v) {
      display_assert("plan->coast_t + actual_t >= t",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x853, 1);
      system_exit(-1);
    }
    goto validate;
  }

  *(int *)(plan + 0x14) = 0;

validate:
  if (*(int *)(plan + 0x0c) == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->accel_a",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85c, 1);
    system_exit(-1);
  }
  if (*(int *)(plan + 0x10) == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->accel_t",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85d, 1);
    system_exit(-1);
  }
  if (*(int *)(plan + 0x14) == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->coast_t",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85e, 1);
    system_exit(-1);
  }
  if (*(int *)(plan + 0x18) == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->decel_a",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x85f, 1);
    system_exit(-1);
  }
  if (*(int *)(plan + 0x1c) == 0x7f7fffff) {
    display_assert("REAL_MAX != plan->decel_t",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x860, 1);
    system_exit(-1);
  }
}

/* unit_adjust_projectile_ray (0x1acf90) — adjust projectile ray origin
 * and direction based on unit state.
 *
 * If use_unit_forward is true, copies the unit's forward vector (+0x1ec)
 * into direction. If adjust_origin is true, projects the origin onto the
 * unit's aim position along the direction vector. Finally computes
 * velocity_out as the dot product of the root location's direction with
 * the forward direction.
 *
 * Source: units.c
 */
void unit_adjust_projectile_ray(int unit_handle, float *origin,
                                float *direction, float *velocity_out,
                                char adjust_origin, char use_unit_forward)
{
  char *unit;
  float aim_pos[3];
  float dot;
  float root_dir[3];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  /* Copy unit forward vector as direction if requested */
  if (use_unit_forward != '\0') {
    direction[0] = *(float *)(unit + 0x1ec);
    direction[1] = *(float *)(unit + 0x1f0);
    direction[2] = *(float *)(unit + 0x1f4);
  }

  /* Project origin onto aim position along direction */
  if (adjust_origin != '\0') {
    unit_set_seat_state(unit_handle, aim_pos);
    dot = (origin[0] - aim_pos[0]) * direction[0] +
          (origin[1] - aim_pos[1]) * direction[1] +
          (origin[2] - aim_pos[2]) * direction[2];
    origin[0] = dot * direction[0] + aim_pos[0];
    origin[1] = dot * direction[1] + aim_pos[1];
    origin[2] = dot * direction[2] + aim_pos[2];
  }

  /* Compute velocity as dot(root_direction, direction) */
  object_get_root_location(unit_handle, root_dir, 0);
  *velocity_out = root_dir[0] * direction[0] +
                  root_dir[2] * direction[2] +
                  root_dir[1] * direction[1];
}

/* unit_render_debug (0x1ad060) — debug rendering for unit state.
 *
 * Draws debug visualization for unit aim vectors, seat positions, and
 * animation throttle when the corresponding debug flags (0x5054f5-f7)
 * are enabled. Conditional on global debug toggles.
 *
 * Source: units.c
 */
void unit_render_debug(int unit_handle)
{
  char *unit;
  int unit_tag;
  float eye_pos[3];
  float head_pos[3];
  char marker_data[0x6c];       /* marker output; position at +0x60 */
  int seat_idx;
  float seat_a[3], seat_b[3], seat_c[3];
  int seat_count;
  int local_player;
  char result;
  char *text;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);

  /* Debug aim/look vectors */
  if (*(char *)0x5054f7 != '\0') {
    unit_set_seat_state(unit_handle, eye_pos);
    object_get_world_position(unit_handle, (vector3_t *)head_pos);
    head_pos[2] = head_pos[2] + 0.1f;
    /* Draw aim direction line (scale 1.0, yellow) */
    FUN_00189320(1, eye_pos, (void *)(unit + 0x1ec), 0x3f800000,
                 *(int *)0x2ee6c4);
    /* Draw aim direction line (scale 0.5, green) */
    FUN_00189320(1, eye_pos, (void *)(unit + 0x1e0), 0x3f000000,
                 *(int *)0x2ee6d0);
    /* Draw head position (scale 1.0, yellow) */
    FUN_00189320(1, head_pos, (void *)(unit + 0x24), 0x3f800000,
                 *(int *)0x2ee6c4);
    /* Draw body direction (scale 0.5, green) */
    FUN_00189320(1, head_pos, (void *)(unit + 0x1d4), 0x3f000000,
                 *(int *)0x2ee6d0);
  }

  /* Debug seat positions */
  if (*(char *)0x5054f6 != '\0') {
    {
      int player_idx;
      player_idx = local_player_get_player_index(*(int16_t *)0x506548);
      local_player = (int)datum_get(*(data_t **)0x5aa6d4, player_idx);
    }
    seat_count = *(int *)(local_player + 0x34);
    if (seat_count != NONE) {
      seat_idx = 0;
      if (seat_idx < *(int *)(unit_tag + 0x2e4)) {
        do {
          result = (char)unit_get_seat_enter_position(
            seat_count, unit_handle, (int16_t)seat_idx,
            seat_a, seat_b, seat_c);
          if (result != '\0') {
            FUN_00189150(1, seat_a, 0.25f, *(void **)0x2ee6d0);
            FUN_00189150(1, seat_b, 0.25f, *(void **)0x2ee6d8);
            FUN_00189150(1, seat_c, 0.25f, *(void **)0x2ee6e0);
          }
          seat_idx += 1;
        } while ((int)(short)seat_idx < *(int *)(unit_tag + 0x2e4));
      }
    }
  }

  /* Debug animation throttle */
  if (*(char *)0x5054f5 != '\0') {
    object_get_markers_by_string_id(unit_handle, (void *)0x2909e4,
                                    marker_data, 1);
    /* Position is at offset 0x60 within the marker output struct.
     * marker_data base is EBP-0x98; position at EBP-0x38 = base+0x60. */
    head_pos[0] = *(float *)((char *)marker_data + 0x60);
    head_pos[1] = *(float *)((char *)marker_data + 0x64);
    head_pos[2] = *(float *)((char *)marker_data + 0x68);
    text = csprintf((char *)0x5ab100, "%.2f",
                     (double)*(float *)(unit + 0x298));
    FUN_00189cb0(0, head_pos, text, *(int *)0x2ee6f0);
  }
}

/* vehicle_scripting_find_available_seats (0x1adfc0) — find available seats
 * in a unit matching a substring filter and seat desire type.
 *
 * Iterates unit seats from the unit tag (0x756e6974), checks the seat name
 * against the substring filter, applies the seat desire type filter, and
 * stores matching seat indices in seat_indices up to max_seats.
 *
 * seat_desire_type values:
 *   -1 = NONE (no filter, accept all)
 *   0 = NOT driver (bit 2 clear)
 *   1 = gunner (bit 3 set)
 *   2 = neither driver nor gunner
 *   3 = driver (bit 2 set)
 *   default = accept
 *
 * Source: units.c line 0x178f-0x1790 asserts.
 */
int16_t vehicle_scripting_find_available_seats(int unit_handle,
                                               int seat_substring_addr,
                                               int16_t seat_desire_type,
                                               int16_t *seat_indices,
                                               int16_t max_seats)
{
  const char *seat_substring;
  char *unit;
  int unit_tag;
  char match_all;
  short found_count;
  int loop_idx;
  uint32_t *seat_entry;
  char seat_name_buf[256];
  uint8_t bit_val;
  char seat_filled;

  seat_substring = (const char *)seat_substring_addr;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);

  if (seat_substring == NULL) {
    display_assert("seat_substring_name",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x178f, 1);
    system_exit(-1);
  }
  if (seat_desire_type != -1 &&
      (seat_desire_type < 0 || seat_desire_type >= 5)) {
    display_assert("(seat_desire_type == NONE) || "
                   "((seat_desire_type >= 0) && "
                   "(seat_desire_type < NUMBER_OF_VEHICLE_SEAT_DESIRE_TYPES))",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x1790, 1);
    system_exit(-1);
  }

  /* Determine if we should match all seats or filter by substring */
  if (seat_substring != NULL && csstrlen(seat_substring) != 0) {
    match_all = 0;
  } else {
    match_all = 1;
  }

  found_count = 0;
  loop_idx = 0;
  while ((short)loop_idx < *(int *)(unit_tag + 0x2e4)) {
    seat_entry = (uint32_t *)tag_block_get_element(
      (void *)(unit_tag + 0x2e4), loop_idx, 0x11c);

    if (found_count >= max_seats) {
      return found_count;
    }

    /* Copy and lowercase the seat name for comparison */
    csstrcpy(seat_name_buf, (const char *)(seat_entry + 1));
    csstr_tolower(seat_name_buf);

    /* Check substring match */
    if (!match_all && crt_strstr(seat_name_buf, seat_substring) == NULL) {
      goto next_seat;
    }

    /* Check seat desire type filter */
    switch ((int)seat_desire_type) {
    case 0:
      bit_val = ~(uint8_t)(*seat_entry >> 2);
      break;
    case 1:
      bit_val = (uint8_t)(*seat_entry >> 3);
      break;
    case 2:
      if ((*seat_entry & 4) != 0 || (*seat_entry & 8) != 0)
        goto next_seat;
      goto accept_seat;
    case 3:
      bit_val = (uint8_t)(*seat_entry >> 2);
      break;
    default:
      goto accept_seat;
    }
    if ((bit_val & 1) == 0)
      goto next_seat;

  accept_seat:
    seat_filled = unit_seat_filled(unit_handle, (int16_t)loop_idx);
    if (seat_filled == '\0') {
      seat_indices[found_count] = (int16_t)loop_idx;
      found_count = found_count + 1;
    }

  next_seat:
    loop_idx += 1;
  }
  return found_count;
}

/* unit_leap_begin (0x1b1c70) — start a leap animation.
 *
 * Checks the unit's animation state via a switch; if the state is not one
 * of the dying/dead/special states, and either the unit has a vehicle
 * or melee is not active, requests animation state 0x27 (leap). If the
 * forward vector is non-NULL, applies it as the unit's alignment vector.
 *
 * Returns 1 on success, 0 if the state blocks leaping.
 *
 * Source: units.c
 */
char unit_leap_begin(int unit_handle, float *forward)
{
  char *unit;
  char result;
  char anim_ok;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  result = 0;

  switch (*(uint8_t *)(unit + 0x253)) {
  case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
  case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21:
  case 0x22: case 0x23: case 0x27: case 0x29:
    /* These states block leaping */
    break;
  default:
    if (*(short *)(unit + 0x64) != 0 ||
        (*(uint8_t *)(unit + 0x424) & 1) == 0) {
      anim_ok = FUN_001ad260(unit_handle, 0x27);
      if (anim_ok != '\0') {
        if (forward != 0) {
          unit_apply_alignment_vector(unit_handle, forward);
        }
        result = 1;
      }
    }
    break;
  }
  return result;
}

/* unit_throw_grenade_begin (0x1b2090) — begin grenade throw animation.
 *
 * Checks grenade availability, animation state eligibility, weapon state,
 * and initiates the grenade throw animation (state 0x21). Sets up
 * animation frame counts, applies alignment vector, triggers first-person
 * weapon message and player aim assist clear, and creates the grenade
 * throw effect if one is defined in the game globals.
 *
 * Returns 1 on success, 0 if throw cannot begin.
 *
 * Source: units.c
 */
char unit_throw_grenade_begin(int unit_handle, float *alignment_vector)
{
  char *unit;
  int unit_tag;
  int weapon_handle;
  char result;
  int16_t grenade_type;
  int16_t grenade_count;
  char anim_ok;
  int antr_tag;
  int anim_element;
  int gg;
  int gg_element;
  int effect_tag;
  float local_buf[3]; /* alignment_vector scratch; z uninitialized per original */
  char weapon_blocks;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);

  /* Get the current weapon handle */
  {
    int temp_unit;
    temp_unit = (int)object_get_and_verify_type(unit_handle, 3);
    weapon_handle = unit_get_weapon(unit_handle,
      *(int16_t *)(temp_unit + 0x2a2));
  }

  result = 0;

  /* Check grenade availability */
  grenade_type = unit_get_current_grenade_type(unit_handle);
  grenade_count = unit_get_grenade_count(unit_handle, grenade_type);
  if (grenade_count < 1) {
    return result;
  }

  /* Check animation state */
  switch (*(uint8_t *)(unit + 0x253)) {
  case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
  case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21:
  case 0x22: case 0x23: case 0x27: case 0x29:
    return result;
  }

  /* Check if weapon prevents grenade throwing */
  weapon_blocks = (char)weapon_prevents_grenade_throwing(weapon_handle);
  if (weapon_blocks != '\0') {
    return result;
  }

  /* Stop weapon reload and melee attack */
  if (weapon_handle != NONE) {
    weapon_stop_reload(weapon_handle);
  }
  biped_stop_melee_attack(unit_handle);

  /* Clear some unit state */
  *(uint8_t *)(unit + 0x254) = 0;
  *(int16_t *)(unit + 0x25a) = -1;

  /* Request grenade throw animation */
  anim_ok = FUN_001ad260(unit_handle, 0x21);
  if (anim_ok == '\0') {
    return result;
  }

  /* Set up animation timing */
  *(uint8_t *)(unit + 0x23d) = 1;
  *(int16_t *)(unit + 0x23e) = 0;
  antr_tag = (int)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  anim_element = (int)tag_block_get_element(
    (void *)(antr_tag + 0x74),
    (int)*(int16_t *)(unit + 0x80), 0xb4);
  *(int16_t *)(unit + 0x240) =
    (int16_t)(*(int16_t *)(anim_element + 0x34) -
              *(int16_t *)(unit + 0x82) + 1);

  /* Apply alignment vector */
  if (alignment_vector != NULL) {
    unit_apply_alignment_vector(unit_handle, alignment_vector);
  } else {
    /* Compute alignment from unit forward direction if magnitude > 0 */
    local_buf[0] = *(float *)(unit + 0x1ec);
    local_buf[1] = *(float *)(unit + 0x1f0);
    if (magnitude3d(local_buf) > 0.0f) {
      unit_apply_alignment_vector(unit_handle, local_buf);
    }
  }
  /* Trigger first-person weapon messages and player aim assist */
  first_person_weapon_message_from_unit(unit_handle, 0x11);
  player_clear_aim_assist(unit_handle);

  /* Create grenade throw effect */
  gg = (int)game_globals_get();
  gg_element = (int)tag_block_get_element(
    (void *)(gg + 0x128),
    (int)*(int8_t *)(unit + 0x2cc), 0x44);
  effect_tag = *(int *)(gg_element + 0x10);
  if (effect_tag != NONE) {
    FUN_0009ec30(effect_tag, unit_handle, unit_handle, (short)-1,
                 0.0f, 0.0f, 0, 0);
  }
  return 1;
}

/* unit_melee_attack_begin (0x1b1b60) — begin a melee attack animation.
 *
 * Checks the unit's animation state via a switch table; if the state is one
 * of the dying/dead/special states (0x17-0x23,0x27,0x29), the attack is
 * blocked. Otherwise, determines the target animation state based on
 * param_2 (forced hit), melee readiness, and unit flags. Requests the
 * animation via FUN_001ad260. On success (or forced), sets melee flags
 * and optionally applies alignment vector.
 *
 * param_2: if non-zero, force melee hit (animation state 0x20, type=4).
 * param_3: if non-zero, pointer to float[3] alignment vector.
 *
 * Returns 1 on success, 0 if blocked.
 */
char unit_melee_attack_begin(int unit_handle, char param_2, int param_3)
{
  char *unit;
  int unit_tag;
  char anim_state;
  char result;
  uint8_t melee_ready;
  char anim_ok;
  int16_t new_state;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);
  result = 0;

  switch (*(uint8_t *)(unit + 0x253)) {
  case 0x17: case 0x18: case 0x19: case 0x1a: case 0x1b:
  case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21:
  case 0x22: case 0x23: case 0x27: case 0x29:
    /* These animation states block melee attacks */
    break;
  default:
    /* Check if melee is ready: either unit is in a vehicle (seat != 0)
     * or the melee-ready bit (unit+0x424, bit 0) is set */
    melee_ready = 0;
    if (*(short *)(unit + 0x64) == 0) {
      melee_ready = *(uint8_t *)(unit + 0x424) & 1;
    }

    if (param_2 != '\0') {
      new_state = 0x20;  /* forced melee hit */
    } else {
      anim_state = *(char *)(unit + 0x253);
      if (anim_state == 0x28) {
        new_state = 0x29;  /* melee continuation */
      } else {
        new_state = (int16_t)((melee_ready != 0) + 0x1e);
      }
    }

    anim_ok = FUN_001ad260(unit_handle, new_state);

    if (anim_ok != '\0' || param_2 != '\0') {
      /* Check unit tag flag bit 8 at offset 0x17c */
      if ((*(uint32_t *)(unit_tag + 0x17c) & 0x100) != 0) {
        *(uint8_t *)(unit + 0x253) = 0x19;
      }
      /* Apply alignment vector if provided */
      if (param_3 != 0) {
        unit_apply_alignment_vector(unit_handle, (float *)param_3);
      }
      if (param_2 != '\0') {
        *(uint8_t *)(unit + 0x239) = 4;
        *(uint8_t *)(unit + 0x23a) = 0;
        return 1;
      }
      *(uint8_t *)(unit + 0x239) = 1;
      result = 1;
    }
    break;
  }
  return result;
}

/* unit_place (0x1b24d0) — place a unit with placement data.
 *
 * Applies placement body vitality if > 0.0f, and if the "dead" flag
 * (placement+4, bit 0) is set, kills the unit: sets animation state,
 * clears weapons, clears grenade state, deletes weapon object,
 * marks as dead, and sets up the death animation frame.
 *
 * Source: units.c
 */
void unit_place(int unit_handle, void *placement)
{
  char *unit;
  int unit_tag;
  int antr_tag;
  int anim_element;
  int death_frame;
  int flags;
  int obj_flags;
  float *place;

  place = (float *)placement;
  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);

  /* Apply body vitality from placement if positive */
  if (place[0] > 0.0f) {
    *(float *)(unit + 0x90) = place[0];
  }

  /* Check the "dead" flag: bit 0 of the second dword (placement+4) */
  if ((*(int *)((char *)placement + 4) & 1) != 0) {
    /* Kill: set animation state and make dead */
    FUN_001b1400(unit_handle, 1, 0, 0, 0, 0, 0, -1, 0);

    if (*(char *)(unit + 0x253) == 0x19) {
      /* Clear weapons and grenade state */
      unit_clear_weapons(unit_handle);
      csmemset((void *)(unit + 0x2ce), 0, 2);

      /* Delete weapon object if one exists */
      if (*(int *)(unit + 0x2c8) != NONE) {
        object_delete(*(int *)(unit + 0x2c8));
        *(int *)(unit + 0x2c8) = NONE;
      }

      /* Look up the death animation frame count */
      antr_tag = (int)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
      anim_element = (int)tag_block_get_element(
        (void *)(antr_tag + 0x74),
        (int)*(short *)(unit + 0x80),
        0xb4);
      death_frame = (int)*(short *)(anim_element + 0x22) - 4;

      /* Mark as dead: set object flag bits */
      *(uint8_t *)(unit + 0xb6) = *(uint8_t *)(unit + 0xb6) | 4;
      obj_flags = *(int *)(unit + 0x4);
      *(int *)(unit + 0x4) = obj_flags | 0x20000;

      /* Clamp death frame to >= 0 (branchless: mask with sign) */
      *(uint16_t *)(unit + 0x82) = (uint16_t)(death_frame & ((death_frame < 0) - 1));

      /* Set additional unit flags */
      flags = *(int *)(unit + 0x1b4);
      *(int *)(unit + 0x1b4) = flags | 0x200;

      /* Record game time and clear body vitality */
      *(int *)(unit + 0x3cc) = game_time_get();
      *(float *)(unit + 0x90) = 0.0f;
      *(float *)(unit + 0x94) = 0.0f;

      /* Update position and children */
      FUN_00136b40(unit_handle);
      object_update_children_recursive(unit_handle);
    }
  }
}

/* unit_create_initial_weapons (0x1b2660) — create initial weapons for a unit.
 *
 * Iterates the unit tag's initial weapons tag block (at tag+0x2d8, element
 * size 0x24), and for each weapon tag index that is not NONE, creates an
 * object via object_placement_data_new + object_new. Then tries to pick
 * it up: if the game engine is running and the unit already has that weapon
 * definition, delete the duplicate; otherwise attempt unit_enter_seat and
 * delete on failure.
 *
 * Source: units.c
 */
void unit_create_initial_weapons(int unit_handle)
{
  char *unit;
  int unit_tag;
  int *initial_weapons_block;
  int loop_counter;
  int element;
  int weapon_tag_index;
  int weapon_handle;
  char *weapon_data;
  char engine_running;
  char has_weapon;
  char enter_ok;
  uint8_t placement[136]; /* 0x88 bytes for object_placement_data */

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);
  initial_weapons_block = (int *)(unit_tag + 0x2d8);
  loop_counter = 0;

  if (*initial_weapons_block > 0) {
    element = 0;
    do {
      element = (int)tag_block_get_element(initial_weapons_block, element, 0x24);
      weapon_tag_index = *(int *)(element + 0xc);
      if (weapon_tag_index != NONE) {
        object_placement_data_new(placement, weapon_tag_index, unit_handle);
        weapon_handle = object_new(placement);
        if (weapon_handle != NONE) {
          weapon_data = (char *)object_get_and_verify_type(weapon_handle, 4);
          engine_running = game_engine_running();
          if (engine_running != '\0') {
            has_weapon = unit_has_weapon_definition_index(unit_handle, *(int *)weapon_data);
            if (has_weapon != '\0') {
              goto delete_weapon;
            }
          }
          enter_ok = unit_enter_seat(unit_handle, weapon_handle, 0);
          if (enter_ok != '\0') {
            goto next_weapon;
          }
delete_weapon:
          object_delete(weapon_handle);
        }
      }
next_weapon:
      loop_counter = loop_counter + 1;
      element = (int)(short)loop_counter;
    } while (element < *initial_weapons_block);
  }
}

/* unit_scripting_enter_vehicle (0x1b32d0) — scripted vehicle entry.
 *
 * Makes a unit enter a specific seat of a vehicle by seat name.
 * Validates both handles, checks the seat name length, verifies the
 * unit is not dying/dead, handles existing vehicle occupancy by
 * calling unit_exit_seat_end, then iterates the vehicle's seats
 * tag block looking for a matching seat name. Checks seat availability
 * and type compatibility before calling unit_board_vehicle.
 *
 * Source: units.c
 */
void unit_scripting_enter_vehicle(int unit_handle, int vehicle_handle,
                                  char *seat_name)
{
  char *unit_data;
  char *vehicle_unit;
  int vehicle_tag;
  int *seats_block;
  int seat_idx;
  int seat_element;
  int stricmp_result;
  char seat_filled;

  if (unit_handle == NONE) {
    return;
  }
  if (vehicle_handle == NONE) {
    return;
  }
  /* Validate seat name is a non-empty string */
  if (csstrlen(seat_name) == 0) {
    return;
  }

  unit_data = (char *)object_get_and_verify_type(unit_handle, 3);
  /* Check if unit is dying/dead (bit 2 of byte at +0xb6) */
  if ((*(uint8_t *)(unit_data + 0xb6) & 4) != 0) {
    return;
  }

  /* If unit is already in a vehicle, try to exit first */
  if (*(int *)(unit_data + 0xcc) != NONE) {
    if (*(short *)(unit_data + 0x2a0) != -1) {
      unit_exit_seat_end(unit_handle);
    }
    /* If still in a vehicle after attempting exit, abort */
    if (*(int *)(unit_data + 0xcc) != NONE) {
      return;
    }
  }

  /* Get the vehicle's unit tag and iterate seats */
  vehicle_unit = (char *)object_get_and_verify_type(vehicle_handle, 3);
  vehicle_tag = (int)tag_get(0x756e6974, *(int *)vehicle_unit);
  seats_block = (int *)(vehicle_tag + 0x2e4);
  seat_idx = 0;

  if (*seats_block > 0) {
    seat_element = 0;
    while (1) {
      seat_element = (int)tag_block_get_element(seats_block, seat_element, 0x11c);
      stricmp_result = csstricmp(seat_name, (char *)(seat_element + 4));

      if (stricmp_result == 0) {
        /* Seat name matches - check availability */
        seat_filled = unit_seat_filled(vehicle_handle, (int16_t)seat_idx);
        if (seat_filled == '\0') {
          /* Seat is available - check type compatibility */
          if (*(short *)(unit_data + 0x64) == 1 ||
              FUN_001acd70(unit_handle, (const char *)(seat_element + 4), 0, 0) != '\0') {
            /* Compatible - board the vehicle */
            unit_board_vehicle(unit_handle, vehicle_handle, (int16_t)seat_idx);
            return;
          }
        }
      }

      seat_idx = seat_idx + 1;
      seat_element = (int)(short)seat_idx;
      if (seat_element >= *seats_block) {
        return;
      }
    }
  }
}

/* vehicle_scripting_load_magic (0x1b3400) — load units into vehicle seats.
 *
 * Iterates child objects of the given group_handle. For each child that is
 * a biped/vehicle type and not dying/dead, tries to seat it in the vehicle.
 * Finds available seats via vehicle_scripting_find_available_seats, checks
 * seat type compatibility, handles existing vehicle occupancy, and calls
 * unit_board_vehicle. Returns the count of successfully seated units.
 *
 * Source: units.c
 */
uint16_t vehicle_scripting_load_magic(int vehicle_handle, int seat_substring,
                                      int group_handle)
{
  char *vehicle_unit;
  int vehicle_tag;
  int child_handle;
  char *child_data;
  int16_t seat_count;
  short seat_indices[16];
  int iter_state[1];
  int inner_idx;
  int16_t seat_idx;
  int seat_element;
  char seat_type_ok;
  char board_ok;
  uint16_t loaded_count;

  loaded_count = 0;
  if (vehicle_handle == NONE) {
    return 0;
  }

  vehicle_unit = (char *)object_get_and_verify_type(vehicle_handle, 3);
  vehicle_tag = (int)tag_get(0x756e6974, *(int *)vehicle_unit);

  /* Find available seats matching the seat substring */
  seat_count = vehicle_scripting_find_available_seats(
    vehicle_handle, seat_substring, -1, seat_indices, 0x10);

  /* Get first child of group */
  child_handle = FUN_000ce450(group_handle, iter_state);
  if (child_handle == NONE) {
    return 0;
  }

  do {
    child_data = (char *)object_get_and_verify_type(child_handle, -1);

    /* Check if child is a biped or vehicle type (bit 0 or 1 of object_type) */
    if (((1 << (*(uint8_t *)(child_data + 0x64) & 0x1f)) & 3) != 0 &&
        (*(uint8_t *)(vehicle_unit + 0xb6) & 4) == 0) {
      inner_idx = 0;
      if ((short)seat_count > 0) {
        do {
          seat_idx = seat_indices[(short)inner_idx];
          if (seat_idx != -1) {
            seat_element = (int)tag_block_get_element(
              (void *)(vehicle_tag + 0x2e4), (int)seat_idx, 0x11c);

            /* Check seat type compatibility */
            if (*(short *)(child_data + 0x64) != 1) {
              seat_type_ok = FUN_001acd70(child_handle, (const char *)(seat_element + 4), 0, 0);
              if (seat_type_ok == '\0') {
                goto next_seat;
              }
            }

            /* Handle existing vehicle occupancy */
            if (*(int *)(child_data + 0xcc) != NONE) {
              if (*(short *)(child_data + 0x2a0) != -1) {
                unit_exit_seat_end(child_handle);
              }
              if (*(int *)(child_data + 0xcc) != NONE) {
                goto next_seat;
              }
            }

            /* Board the vehicle */
            board_ok = unit_board_vehicle(child_handle, vehicle_handle, seat_idx);
            if (board_ok != '\0') {
              seat_indices[(short)inner_idx] = -1;
              loaded_count = loaded_count + 1;
              break;
            }
          }
next_seat:
          inner_idx = inner_idx + 1;
        } while ((short)inner_idx < (short)seat_count);
      }
    }

    /* Get next child */
    child_handle = FUN_000ce320(group_handle, iter_state);
  } while (child_handle != NONE);

  return loaded_count;
}

/* unit_try_and_exit_seat (0x1b3580) — attempt to exit the current seat.
 *
 * Checks if the unit is in a vehicle seat and meets the conditions to exit:
 * not a player type (seat type != 1), animation state allows exit, and
 * the seat has exit animation data. If the vehicle's driver matches this
 * unit, opens the vehicle. Sets garbage flag, updates animation state to
 * 0x1b (exiting), and notifies AI.
 *
 * Returns 1 on success, 0 if exit is blocked.
 *
 * Source: units.c
 */
char unit_try_and_exit_seat(int unit_handle)
{
  char *unit;
  int vehicle_handle;
  int unit_tag;
  int antr_tag;
  int mode_element;
  int anim_block_count;
  int16_t exit_anim;
  char *vehicle_data;
  int anim_graph_tag_index;
  int16_t animation_index;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  /* Must be in a vehicle and have a powered seat */
  if (*(int *)(unit + 0xcc) == NONE || *(short *)(unit + 0x2a0) == -1) {
    return 0;
  }

  /* Player-type units (seat type 1) use a direct exit */
  if (*(short *)(unit + 0x64) == 1) {
    unit_exit_seat_end(unit_handle);
    return 0;
  }

  /* Check if animation state blocks exit */
  if (FUN_001a8730((void *)(unit + 0x248)) != '\0') {
    return 0;
  }

  /* Look up unit and animation tags */
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);
  antr_tag = (int)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));

  /* Get the mode element for this unit's current seat */
  mode_element = (int)tag_block_get_element(
    (void *)(antr_tag + 0xc),
    (int)*(int8_t *)(unit + 0x250),
    100);

  /* Check if the mode has exit animations (block count > 8) */
  anim_block_count = *(int *)(mode_element + 0x40);
  if (anim_block_count <= 8) {
    return 0;
  }

  /* Get the exit animation index from sub-block element 0, offset 0x10 */
  exit_anim = *(short *)(*(int *)(mode_element + 0x44) + 0x10);
  if (exit_anim == -1) {
    return 0;
  }

  /* If this unit is the vehicle's driver, open the vehicle */
  vehicle_handle = *(int *)(unit + 0xcc);
  vehicle_data = (char *)object_get_and_verify_type(vehicle_handle, 3);
  if (*(int *)(vehicle_data + 0x2d4) == unit_handle) {
    unit_open(vehicle_handle);
  }

  /* Start exit animation: look up via FUN_000fad00, then set */
  anim_graph_tag_index = *(int *)(unit_tag + 0x44);
  animation_index = FUN_000fad00(anim_graph_tag_index, exit_anim);
  unit_set_animation(unit_handle, anim_graph_tag_index, animation_index);

  /* Mark unit as garbage and set exit animation state */
  object_set_garbage(unit_handle, 1);
  *(uint8_t *)(unit + 0x253) = 0x1b;

  /* Notify AI subsystem of vehicle exit */
  ai_handle_exit_vehicle(unit_handle);

  return 1;
}

/* unit_impact_melee_damage (0x1b2290) — apply melee damage at impact point.
 *
 * When the unit has the "melee causes instant kill" flag (tag+0x17c bit 13),
 * and the target is a seated unit with body vitality, and the target's unit
 * tag has the "instant kill on melee" flag (tag+0x17c bit 22): performs
 * unit_cause_melee_damage, resets sound, and deletes the attacking unit.
 *
 * Otherwise, when the unit has the "melee causes boarding" flag (bit 12),
 * and the target is a biped/vehicle that is not dying/dead: walks up the
 * parent chain to ensure the attacker is not boarding its own hierarchy,
 * then copies the global forward/up vectors, negates the damage direction,
 * computes a cross product to derive orientation, translates the unit, and
 * starts a forced melee attack.
 *
 * Source: units.c
 */
void unit_impact_melee_damage(int unit_handle, int param_2, int param_3,
                              int param_4, int param_5, int param_6,
                              float *param_7, int param_8)
{
  char *unit;
  int unit_tag;
  char *target_data;
  int target_tag;
  int parent_handle;
  int parent_obj;
  char *fwd_ptr;
  char *up_ptr;
  float *impact_dir;
  float local_vec[3];
  float mag;
  int flags;
  int unit_flags;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit);
  target_data = (char *)object_get_and_verify_type(param_2, -1);

  /* Check instant-kill melee path: unit has bit 13 of tag flags */
  if ((*(uint32_t *)(unit_tag + 0x17c) & 0x2000) != 0 &&
      *(short *)(target_data + 0x64) == 0 &&
      *(float *)(target_data + 0x94) > 0.0f) {
    target_tag = (int)tag_get(0x756e6974, *(int *)target_data);
    if ((*(uint32_t *)(target_tag + 0x17c) & 0x400000) != 0) {
      unit_cause_melee_damage(unit_handle, 1, param_2, param_3, param_4,
                              param_5, (int)param_7);
      FUN_00137540(unit_handle);
      object_delete(unit_handle);
      return;
    }
  }

  /* Check boarding melee path: unit has bit 12 of tag flags */
  if ((*(uint32_t *)(unit_tag + 0x17c) & 0x1000) == 0) {
    return;
  }
  /* Target must be a biped or vehicle (object type 0 or 1) */
  if (((1 << (*(uint8_t *)(target_data + 0x64) & 0x1f)) & 3) == 0) {
    return;
  }
  /* Target must not be dying/dead */
  if ((*(uint8_t *)(target_data + 0xb6) & 4) != 0) {
    return;
  }

  /* Walk up the target's parent chain to ensure we're not boarding ourself */
  parent_handle = *(int *)(target_data + 0xcc);
  while (parent_handle != NONE) {
    parent_obj = (int)object_get_and_verify_type(parent_handle, -1);
    if (parent_handle == unit_handle) {
      return;
    }
    if (*(short *)(parent_obj + 0x64) != 1) {
      return;
    }
    parent_handle = *(int *)(parent_obj + 0xcc);
  }

  /* Copy global forward vector to unit position (obj+0x18) */
  fwd_ptr = *(char **)0x0031fc38;
  *(float *)(unit + 0x18) = *(float *)fwd_ptr;
  *(float *)(unit + 0x1c) = *(float *)(fwd_ptr + 4);
  *(float *)(unit + 0x20) = *(float *)(fwd_ptr + 8);

  /* Copy global forward vector to unit up (obj+0x3c) */
  fwd_ptr = *(char **)0x0031fc38;
  *(float *)(unit + 0x3c) = *(float *)fwd_ptr;
  *(float *)(unit + 0x40) = *(float *)(fwd_ptr + 4);
  *(float *)(unit + 0x44) = *(float *)(fwd_ptr + 8);

  /* Copy and negate damage direction as the impact vector (obj+0x24) */
  impact_dir = (float *)(unit + 0x24);
  impact_dir[0] = param_7[0];
  impact_dir[1] = param_7[1];
  impact_dir[2] = param_7[2];
  impact_dir[0] = -impact_dir[0];
  impact_dir[1] = -impact_dir[1];
  impact_dir[2] = -impact_dir[2];

  /* Derive left vector via cross product: forward = cross(up, impact_dir) */
  cross_product3d((float *)(unit + 0x30), impact_dir, local_vec);
  mag = normalize3d(local_vec);
  if (mag == 0.0f) {
    /* Impact direction parallel to up — use global up as fallback */
    up_ptr = *(char **)0x0031fc44;
    cross_product3d((float *)up_ptr, impact_dir, local_vec);
    mag = normalize3d(local_vec);
    if (mag == 0.0f) {
      /* Still degenerate — use global left vector */
      fwd_ptr = *(char **)0x0031fc3c;
      local_vec[0] = *(float *)fwd_ptr;
      local_vec[1] = *(float *)(fwd_ptr + 4);
      local_vec[2] = *(float *)(fwd_ptr + 8);
    }
  }

  /* Recompute up from impact_dir and local_vec */
  cross_product3d(impact_dir, local_vec, (float *)(unit + 0x30));

  /* Translate unit to damage position and attach to target */
  object_translate(unit_handle, (float *)param_6, (void *)param_8);
  object_attach_to_parent(param_2, unit_handle, param_3);

  /* Set object flags for boarding */
  flags = *(int *)(unit + 0x4);
  *(int *)(unit + 0x4) = flags | 0x20;
  unit_flags = *(int *)(unit + 0x1b4);
  *(int *)(unit + 0x1b4) = unit_flags | 0x8000;

  /* Start forced melee attack (param_2=1 for forced, param_3=0 for no alignment) */
  unit_melee_attack_begin(unit_handle, 1, 0);
}

/* unit_cause_melee_damage (0x1ae840)
 * Applies melee damage from a unit to a target. Resolves the melee marker
 * position, optionally performs a collision test to adjust the damage origin,
 * then builds damage params and applies via object_cause_damage.
 * cdecl, 7 stack params.
 * If melee_hit is false and the collision result has a valid material type,
 * plays the melee clang sound via FUN_001abd10. */
void unit_cause_melee_damage(int unit_handle, char melee_hit, int target_handle,
                             int param_4, int param_5, int param_6,
                             int param_7)
{
  char *unit;
  char *unit_tag;
  int16_t marker_count;
  /* FUN_0014df70 (collision raycast) writes an 80-byte result struct through
   * this pointer (stores at +0..+0x4e, derived from disasm). The original
   * allocates 0x50 bytes (lea [ebp-0x68], direction at [ebp-0x18]); a 4-byte
   * buffer here overflowed the saved EBX/EDI/ESI/EBP slots, corrupting the
   * caller's `unit` pointer to NONE (0xffffffff) -> [unit+0x25a] page fault
   * (CR2=0x259) when an elite melees in PoA. Matches the correct siblings at
   * units.c:2233 and units.c:11326. */
  char collision_result[80];
  float *position;
  float melee_pos[3];
  float direction[3];
  char coll_hit;
  int16_t coll_depth;
  int weapon_handle;
  char *weapon_obj;
  char *weapon_tag;
  int damage_effect_index;
  char marker_buf[96];
  char damage_params[0x54];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

  if (*(int *)(unit_tag + 0x294) == -1) {
    goto cleanup;
  }

  marker_count = object_get_markers_by_string_id(
      unit_handle, (void *)0x25961c, marker_buf, 1);

  if (marker_count == 1) {
    /* marker found — use marker position as melee origin */
    melee_pos[0] = *(float *)(marker_buf + 0x60);
    melee_pos[1] = *(float *)(marker_buf + 0x64);
    melee_pos[2] = *(float *)(marker_buf + 0x68);

    /* collision user stack depth check */
    if (*(int16_t *)0x4761d8 >= MAXIMUM_COLLISION_USER_STACK_DEPTH) {
      display_assert(
          "global_current_collision_user_depth < "
          "MAXIMUM_COLLISION_USER_STACK_DEPTH",
          "c:\\halo\\SOURCE\\units\\units.c", 0x21b9, true);
      system_exit(-1);
    }

    coll_depth = *(int16_t *)0x4761d8;
    *(int16_t *)0x4761d8 = coll_depth + 1;

    position = (float *)(unit + 0x50);
    *(int16_t *)(0x5a8c80 + coll_depth * 2) = 7;

    /* direction = melee_pos - unit position */
    direction[0] = melee_pos[0] - position[0];
    direction[1] = melee_pos[1] - position[1];
    direction[2] = melee_pos[2] - position[2];

    coll_hit = (char)FUN_0014df70(0x1000e9, position, direction, -1,
                                  (int16_t *)collision_result);
    if (coll_hit != 0) {
      /* collision hit — snap melee position to unit center */
      melee_pos[0] = position[0];
      melee_pos[1] = position[1];
      melee_pos[2] = position[2];
    }

    if (*(int16_t *)0x4761d8 < 2) {
      display_assert("global_current_collision_user_depth > 1",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x21c1, true);
      system_exit(-1);
    }
    *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 - 1;
  } else {
    /* no marker — use unit position directly */
    position = (float *)(unit + 0x50);
    melee_pos[0] = position[0];
    melee_pos[1] = position[1];
    melee_pos[2] = position[2];
  }

  /* Determine the damage effect tag index */
  damage_effect_index = *(int *)(unit_tag + 0x294);

  /* Check if the current weapon has an override melee damage effect */
  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  weapon_handle =
      unit_get_weapon(unit_handle, *(int16_t *)(unit + 0x2a2));

  if (weapon_handle != -1) {
    weapon_obj = (char *)object_get_and_verify_type(weapon_handle, 4);
    weapon_tag = (char *)tag_get(0x77656170, *(int *)weapon_obj);
    /* Check weapon flags bit 15 (byte at +0x309, high bit) for melee override */
    if ((char)(*(uint32_t *)(weapon_tag + 0x308) >> 8) < 0) {
      damage_effect_index = *(int *)(weapon_tag + 0x3a0);
    }
  }

  /* Build damage params */
  damage_data_new(damage_params, damage_effect_index);

  *(int *)(damage_params + 0x14) = *(int *)(unit + 0x48);
  *(int *)(damage_params + 0x18) = *(int *)(unit + 0x4c);
  *(int16_t *)(damage_params + 0x10) = *(int16_t *)(unit + 0x68);
  *(int *)(damage_params + 0x08) = *(int *)(unit + 0x1c8);
  *(float *)(damage_params + 0x20) = melee_pos[1];
  *(int *)(damage_params + 0x2c) = *(int *)(unit + 0x54);
  *(int *)(damage_params + 0x0c) = unit_handle;
  *(float *)(damage_params + 0x1c) = melee_pos[0];
  *(int *)(damage_params + 0x28) = *(int *)(unit + 0x50);
  *(float *)(damage_params + 0x24) = melee_pos[2];
  *(int *)(damage_params + 0x30) = *(int *)(unit + 0x58);

  if (target_handle == -1) {
    FUN_00138e30(damage_params, -1);
  } else {
    object_cause_damage(damage_params, target_handle, (short)param_4,
                        (short)param_5, (short)param_6, (unsigned int)param_7);
  }

  if (melee_hit == 0 && *(int16_t *)(damage_params + 0x4c) != -1) {
    FUN_001abd10(*(int16_t *)(damage_params + 0x4c), unit_handle,
                 damage_effect_index);
  }

cleanup:
  *(uint8_t *)(unit + 0x239) = 0;
}

/* unit_died (0x1b3060)
 * Handles unit death or feign-death. On real death (param_2=0): sets garbage,
 * releases player/actor refs, clears seats, drops weapons and grenades.
 * On feign death (param_2!=0): checks feign_death_timer and random chance
 * to decide living/feigning.
 * cdecl, 2 stack params. */
void unit_died(int unit_handle, char param_2)
{
  char *unit;
  char *unit_tag;
  char *actor_data;
  int weapon_handle;
  int temp_unit;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);

  if (param_2 == 0) {
    /* Real death */
    *(int16_t *)(unit + 0x3d0) = 0;
    object_set_garbage_flag(unit_handle, 1);

    /* Release player reference */
    if (*(int *)(unit + 0x1c8) != -1) {
      player_died(*(int *)(unit + 0x1c8));
      *(int *)(unit + 0x1c8) = -1;
    }

    /* Release actor reference (first slot) */
    if (*(int *)(unit + 0x1a4) != -1) {
      actor_data =
          (char *)datum_get(*(data_t **)0x6325a4, *(int *)(unit + 0x1a4));
      *(int16_t *)(unit + 0x2e4) = *(int16_t *)(actor_data + 0x34);
      *(int16_t *)(unit + 0x2e6) = *(int16_t *)(actor_data + 0x3a);
      actor_died(*(int *)(unit + 0x1a4));
      *(int *)(unit + 0x1a4) = -1;
    }

    /* Release actor reference (second slot) */
    if (*(int *)(unit + 0x1a8) != -1) {
      actor_data =
          (char *)datum_get(*(data_t **)0x6325a4, *(int *)(unit + 0x1a8));
      *(int16_t *)(unit + 0x2e4) = *(int16_t *)(actor_data + 0x34);
      *(int16_t *)(unit + 0x2e6) = *(int16_t *)(actor_data + 0x3a);
      actor_swarm_unit_died(*(int *)(unit + 0x1a8), unit_handle);
      *(int *)(unit + 0x1a8) = -1;
    }

    /* Record time of death */
    *(int *)(unit + 0x3cc) = game_time_get();
  } else {
    /* Feign death */
    if (*(int16_t *)(unit + 0x3d0) < 1) {
      display_assert("unit->unit.feign_death_timer > 0",
                     "c:\\halo\\SOURCE\\units\\units.c", 0x13eb, true);
      system_exit(-1);
    }

    unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);

    {
      int *seed;
      float rnd;
      seed = get_global_random_seed_address();
      rnd = random_math_real((unsigned int *)seed);
      if (rnd < *(float *)(unit_tag + 0x248)) {
        *(uint32_t *)(unit + 0x1b4) |= 0x2000;
      } else {
        *(uint32_t *)(unit + 0x1b4) &= ~0x2000u;
      }
    }
  }

  /* Common death cleanup */
  *(uint32_t *)(unit + 0x1b4) &= 0xffffffee;
  *(int *)(unit + 0x1b8) = 0;

  /* Notify current weapon */
  if (*(int16_t *)(unit + 0x2a2) != -1) {
    temp_unit = (int)object_get_and_verify_type(unit_handle, 3);
    weapon_handle =
        unit_get_weapon(unit_handle, *(int16_t *)(temp_unit + 0x2a2));
    weapon_owner_update(weapon_handle);
  }

  /* Clear integrated light bit */
  temp_unit = (int)object_get_and_verify_type(unit_handle, 3);
  *(uint32_t *)(temp_unit + 0x1b4) &= ~0x02000000u;

  /* Detach from parent (vehicle seat exit) */
  if (*(int *)(unit + 0xcc) != -1) {
    if (*(int16_t *)(unit + 0x2a0) == -1) {
      unit_detach_from_parent(unit_handle);
    } else {
      unit_exit_seat_end(unit_handle);
    }
  }

  /* Drop weapons and grenades */
  *(int16_t *)(unit + 0x368) = 0;
  unit_drop_weapons_on_death(unit_handle);

  /* Clean up secondary weapon reference */
  temp_unit = (int)object_get_and_verify_type(unit_handle, 3);
  if (*(int *)(temp_unit + 0x2c8) != -1) {
    weapon_handle = *(int *)(temp_unit + 0x2c8);
    unit_detach_weapon(unit_handle, weapon_handle);
    *(int *)(temp_unit + 0x2c8) = -1;
  }

  unit_drop_grenades_on_death(unit_handle);

  /* Hide in vehicle if not flagged */
  if (*(char *)(unit + 0x23c) == 0) {
    unit_set_in_vehicle(unit_handle, 1);
  }

  /* Final cleanup */
  *(int16_t *)(unit + 0x25e) = -1;
  *(int16_t *)(unit + 0x25a) = -1;
  *(uint8_t *)(unit + 0x239) = 0;
  if (*(char *)(unit + 0x23d) == 1) {
    *(uint8_t *)(unit + 0x23d) = 0;
  }
}

/* unit_exit_seat_end (0x1b2dd0)
 * Finalizes a unit's exit from a vehicle seat. Gets the seat marker transform,
 * detaches from parent, repositions the unit, applies seat exit matrix, and
 * reselects weapon. If unit is a biped, calls biped_exit_seat_end.
 * cdecl, 1 stack param. */
void unit_exit_seat_end(int unit_handle)
{
  char *unit;
  char *parent_unit;
  char *parent_tag;
  char *seat_element;
  char *node_matrix;
  float exit_offset[3];
  char marker_buf[56];
  char transform_buf[40];
  float exit_position[3];
  char matrix_out[48];
  int seat_handle;
  int seat_rotation_offset;
  float model_node_pos[3];

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  seat_handle = *(int *)(unit + 0xcc);

  if (seat_handle == -1 || *(int16_t *)(unit + 0x2a0) == -1) {
    return;
  }

  parent_unit = (char *)object_get_and_verify_type(seat_handle, 3);
  parent_tag = (char *)tag_get(0x756e6974, *(int *)parent_unit);
  seat_element = (char *)tag_block_get_element(
      parent_tag + 0x2e4, (int)*(int16_t *)(unit + 0x2a0), 0x11c);

  node_matrix = (char *)object_get_node_matrix(unit_handle, 0);

  /* Get marker position from seat */
  object_get_markers_by_string_id(
      seat_handle, (void *)(seat_element + 0x24), marker_buf, 1);

  /* Compute exit offset: node_matrix.position - marker_position */
  exit_offset[0] = *(float *)(node_matrix + 0x28) - *(float *)(marker_buf + 0x60);
  exit_offset[1] = *(float *)(node_matrix + 0x2c) - *(float *)(marker_buf + 0x64);
  exit_offset[2] = *(float *)(node_matrix + 0x30) - *(float *)(marker_buf + 0x68);

  /* Transform exit offset through marker rotation (result unused) */
  {
    float transform_out[3];
    real_matrix3x3_transform_vector(transform_buf,
                                    (vector3_t *)exit_offset,
                                    (vector3_t *)transform_out);
  }

  /* Get the unit's own model node 0 data for default position */
  {
    char *own_unit_tag;
    char *own_model_tag;
    char *node_data;

    own_unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
    own_model_tag = (char *)tag_get(0x6d6f6465, *(int *)(own_unit_tag + 0x34));
    node_data =
        (char *)tag_block_get_element(own_model_tag + 0xb8, 0, 0x9c);

    seat_rotation_offset = (int)(node_data + 0x68);
    model_node_pos[0] = *(float *)(node_data + 0x28);
    model_node_pos[1] = *(float *)(node_data + 0x2c);
    model_node_pos[2] = *(float *)(node_data + 0x30);
  }

  /* Notify parent driver if this unit was the driver */
  if (*(int *)(parent_unit + 0x2d4) == unit_handle &&
      *(char *)(parent_unit + 0x253) != 0x25 &&
      *(int *)(unit + 0xcc) != -1) {
    FUN_001ad260(*(int *)(unit + 0xcc), 0x25);
  }

  /* Record exit info */
  *(int *)(unit + 0x2dc) = seat_handle;
  *(int *)(unit + 0x2e0) = game_time_get();

  /* Clear driver/gunner references if they point to this unit */
  if (*(int *)(unit + 0x2d4) == unit_handle) {
    *(int *)(unit + 0x2d4) = -1;
  }
  if (*(int *)(unit + 0x2d8) == unit_handle) {
    *(int *)(unit + 0x2d8) = -1;
  }

  /* Detach from parent */
  object_detach_from_parent(unit_handle);

  /* Compute exit world position */
  exit_position[0] = exit_offset[0] + *(float *)(unit + 0x0c);
  exit_position[1] = exit_offset[1] + *(float *)(unit + 0x10);
  exit_position[2] = exit_offset[2] + *(float *)(unit + 0x14) - model_node_pos[2];

  object_set_position(unit_handle, exit_position, 0, 0);

  /* Multiply node matrix by seat rotation to get new orientation */
  {
    char *node_matrix2;

    node_matrix2 = (char *)object_get_node_matrix(unit_handle, 0);
    matrix4x3_multiply((float *)node_matrix2, (float *)seat_rotation_offset,
                       (float *)matrix_out);

    /* Copy forward (offset 0x04) and up (offset 0x1C) from result */
    *(int *)(unit + 0x24) = *(int *)(matrix_out + 0x04);
    *(int *)(unit + 0x28) = *(int *)(matrix_out + 0x08);
    *(int *)(unit + 0x2c) = *(int *)(matrix_out + 0x0c);
    *(int *)(unit + 0x30) = *(int *)(matrix_out + 0x1c);
    *(int *)(unit + 0x34) = *(int *)(matrix_out + 0x20);
    *(int *)(unit + 0x38) = *(int *)(matrix_out + 0x24);
  }

  /* Set object as garbage (for cleanup) */
  object_set_garbage(unit_handle, 1);

  /* Reset seat index and set exit-state */
  *(int16_t *)(unit + 0x2a0) = -1;
  *(uint8_t *)(unit + 0x257) = 2;

  /* Clear parent driver/gunner references to this unit */
  if (*(int *)(parent_unit + 0x2d4) == unit_handle) {
    *(int *)(parent_unit + 0x2d4) = -1;
  }
  if (*(int *)(parent_unit + 0x2d8) == unit_handle) {
    *(int *)(parent_unit + 0x2d8) = -1;
  }

  /* Update vehicle seat occupancy and weapon selection */
  unit_update_seat_occupancy(seat_handle);
  unit_select_weapon_after_vehicle_exit(unit_handle);

  /* Send animation update (anim type 0x14 = seat exit) */
  {
    uint8_t anim_data[2];
    anim_data[0] = 0x14;
    anim_data[1] = 0x00;
    FUN_001b0d90(unit_handle, (char *)anim_data);
  }

  /* Create object header block reference for exit velocity */
  {
    char *block_ref;
    block_ref =
        (char *)object_header_block_reference_get(unit_handle, unit + 0x198);

    *(float *)(block_ref + 0x10) = model_node_pos[0];
    *(float *)(block_ref + 0x14) = model_node_pos[1];
    *(float *)(block_ref + 0x18) = model_node_pos[2];
  }

  /* Biped-specific exit handling */
  if (*(int16_t *)(unit + 0x64) == 0) {
    biped_exit_seat_end(unit_handle, seat_handle);
  }

  /* Update children */
  object_update_children_recursive(unit_handle);
}

/* FUN_001aaf40 (0x1aaf40) — grenade throw initiation
 * Decrements grenade count (unless infinite), creates a grenade placement
 * from the unit's forward vector and marker position, spawns the grenade
 * object, and attaches it to the unit.
 * Register args: @edi = unit_handle. No stack params.
 * Called from the grenade throw state (state=1) in the unit update switch. */
void FUN_001aaf40(int unit_handle)
{
  char *unit;
  char *globals_entry;
  int grenade_index;
  char marker_buf[0x6c]; /* marker output buffer at EBP-0xf4 */
  char placement[0x88];  /* object placement data at EBP-0x88 */
  float forward[3];      /* at EBP-0x54 */
  float perp_out[12];    /* at EBP-0x48 */
  int new_handle;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  tag_get(0x756e6974, *(int *)unit);

  grenade_index = (int)*(int8_t *)(unit + 0x2cc);
  {
    char *game_globals;
    game_globals = (char *)game_globals_get();
    globals_entry =
        (char *)tag_block_get_element(game_globals + 0x128, grenade_index, 0x44);
  }

  /* Check if we should consume a grenade (skip if infinite) */
  if (*(int *)(unit + 0x1c8) != -1) {
    if (*(char *)0x5aa892 != 0) {
      goto skip_decrement;
    }
    if (FUN_000a9570(*(int *)(unit + 0x1c8)) != 0) {
      goto skip_decrement;
    }
  }

  if (*(int *)(unit + 0x1a4) != -1) {
    if (actor_has_unlimited_grenades() != 0) {
      goto skip_decrement;
    }
  }

  /* Validate grenade index */
  if (*(int8_t *)(unit + 0x2cc) < 0 || *(int8_t *)(unit + 0x2cc) > 1) {
    display_assert(
        "unit->unit.current_grenade_index>=0 && "
        "unit->unit.current_grenade_index<NUMBER_OF_UNIT_GRENADE_TYPES",
        "c:\\halo\\SOURCE\\units\\units.c", 0x1f1e, true);
    system_exit(-1);
  }

  /* Validate grenade count > 0 */
  if (*(int8_t *)(unit + 0x2ce + *(int8_t *)(unit + 0x2cc)) < 1) {
    display_assert(
        "unit->unit.grenade_counts[unit->unit.current_grenade_index]>0",
        "c:\\halo\\SOURCE\\units\\units.c", 0x1f1f, true);
    system_exit(-1);
  }

  /* Decrement grenade count */
  *(int8_t *)(unit + 0x2ce + *(int8_t *)(unit + 0x2cc)) -= 1;

skip_decrement:
  /* Get grenade marker position */
  object_get_markers_by_string_id(
      unit_handle, (void *)0x2b6d2c, marker_buf, 1);

  /* Build object placement data for the grenade */
  object_placement_data_new(placement, *(int *)(globals_entry + 0x40),
                            unit_handle);
  *(uint32_t *)(placement + 0x04) |= 2;

  /* Copy unit's aim forward vector */
  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  forward[0] = *(float *)(unit + 0x1ec);
  forward[1] = *(float *)(unit + 0x1f0);
  forward[2] = *(float *)(unit + 0x1f4);

  /* Compute perpendicular vector for orientation */
  perpendicular3d(forward, perp_out);
  normalize3d(perp_out);

  /* Copy marker position into placement position (offset 0x18) */
  *(int *)(placement + 0x18) = *(int *)(marker_buf + 0x60);
  *(int *)(placement + 0x1c) = *(int *)(marker_buf + 0x64);
  *(int *)(placement + 0x20) = *(int *)(marker_buf + 0x68);

  /* Spawn the grenade object */
  new_handle = object_new(placement);
  if (new_handle != -1) {
    /* Attach grenade to the unit at the marker node */
    object_attach_to_parent(unit_handle, new_handle,
                            *(int *)marker_buf);
    *(int *)(unit + 0x244) = new_handle;
    *(uint8_t *)(unit + 0x23d) = 2;
    return;
  }

  *(uint8_t *)(unit + 0x23d) = 3;
}

/* FUN_001abd90 (0x1abd90) — melee lunge collision damage
 * Tests for melee collision against the unit's parent (seat occupant target),
 * and applies damage using the unit's melee damage effect tag. When collision
 * hits, computes a surface normal and applies damage with collision context.
 * Otherwise applies damage with no-target params.
 * Register args: @edi = unit_handle. No stack params.
 * Called at the melee lunge phase (state 4) in unit_update. */
void FUN_001abd90(int unit_handle)
{
  char *unit;
  char *unit_tag_data;
  int parent_handle;
  char collision_buf[0x4c0 - 0x64]; /* large collision buffer */
  char collision_result[0x60];
  char damage_params[0x54];
  float direction[3];
  float point_out[3];
  float surface_out[16];
  float normal_out[16];
  char hit_found;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag_data = (char *)tag_get(0x756e6974, *(int *)unit);

  /* Only proceed if in melee lunge state 4, has parent, and has damage effect */
  if (*(char *)(unit + 0x239) != 4) {
    return;
  }
  parent_handle = *(int *)(unit + 0xcc);
  if (parent_handle == -1) {
    return;
  }
  if (*(int *)(unit_tag_data + 0x294) == -1) {
    return;
  }

  hit_found = 0;

  if (*(char *)(unit + 0x23a) == 0) {
    /* First attempt — perform collision test */
    if (*(int16_t *)0x4761d8 >= MAXIMUM_COLLISION_USER_STACK_DEPTH) {
      display_assert(
          "global_current_collision_user_depth < "
          "MAXIMUM_COLLISION_USER_STACK_DEPTH",
          "c:\\halo\\SOURCE\\units\\units.c", 0x22e6, true);
      system_exit(-1);
    }

    {
      int16_t coll_depth;
      char coll_result;

      coll_depth = *(int16_t *)0x4761d8;
      *(int16_t *)0x4761d8 = coll_depth + 1;
      *(int16_t *)(0x5a8c80 + coll_depth * 2) = 8;

      /* Set up collision test against parent */
      coll_result = FUN_0014c8e0((int *)collision_buf, parent_handle);

      if (coll_result != 0) {
        /* Get world position and compute melee direction */
        object_get_world_position(unit_handle, (vector3_t *)point_out);

        direction[0] = *(float *)(unit + 0x24) * *(float *)0x2549d4;
        direction[1] = *(float *)(unit + 0x28) * *(float *)0x2549d4;
        direction[2] = *(float *)(unit + 0x2c) * *(float *)0x2549d4;

        vector3d_scale_add(point_out, direction, *(float *)0xbf000000,
                           point_out);

        coll_result = FUN_0014cb00((int)collision_buf, (void *)3, point_out,
                                   direction, (int16_t *)collision_result);

        if (coll_result != 0) {
          /* Compute hit point */
          vector3d_scale_add(point_out, direction,
                             *(float *)(collision_result + 0x0c),
                             surface_out);

          /* Get surface normal */
          {
            int surface_base;
            surface_base = *(int *)(collision_buf + 0x70 - 0x64);
            FUN_0010a1c0(
                (float *)(surface_base +
                          *(int16_t *)collision_result * 0x34),
                (float *)(collision_result + 0x14),
                (float *)normal_out);
          }

          /* Negate normal if backfacing */
          if (*(int *)(collision_result + 0x1c) < 0) {
            plane_negate((float *)normal_out, (float *)normal_out);
          }

          hit_found = 1;
        }
      }

      /* Pop collision user depth */
      if (*(int16_t *)0x4761d8 < 2) {
        display_assert("global_current_collision_user_depth > 1",
                       "c:\\halo\\SOURCE\\units\\units.c", 0x22fe, true);
        system_exit(-1);
      }
      *(int16_t *)0x4761d8 = *(int16_t *)0x4761d8 - 1;
    }
  }

  /* Build damage params from melee damage effect */
  {
    int damage_effect;
    damage_effect = *(int *)(unit_tag_data + 0x294);
    damage_data_new(damage_params, damage_effect);
  }

  /* Set damage params common fields */
  *(int *)(damage_params + 0x00) = unit_handle;
  *(int16_t *)(damage_params + 0x04) = *(int16_t *)(unit + 0x68);
  *(int *)(damage_params + 0x08) = *(int *)(unit + 0x1c8);
  *(float *)(damage_params + 0x20) = 0.03333333f; /* 1/30 */

  if (hit_found) {
    /* Copy hit position and forward direction into damage params */
    *(float *)(damage_params + 0x34) = surface_out[0];
    *(float *)(damage_params + 0x38) = surface_out[1];
    *(float *)(damage_params + 0x30) = surface_out[0]; /* duplicate */
    *(float *)(damage_params + 0x3c) = surface_out[2];

    /* Copy unit forward as damage direction */
    *(float *)(damage_params + 0x2c) = *(float *)(unit + 0x24);
    *(float *)(damage_params + 0x28) = *(float *)(unit + 0x28);
    *(float *)(damage_params + 0x24) = *(float *)(unit + 0x2c);

    *(uint32_t *)(damage_params + 0x04) |= 2;
    *(char *)(unit + 0x23a) = 10;

    object_cause_damage(damage_params, parent_handle,
                        *(int16_t *)collision_result,
                        *(int16_t *)(collision_result + 0x02),
                        *(int16_t *)(collision_result + 0x1a),
                        (unsigned int)normal_out);
  } else {
    object_cause_damage(damage_params, parent_handle,
                        (short)-1, (short)-1, (short)-1, 0);
  }

  /* Decrement attack timer */
  *(char *)(unit + 0x23a) = *(char *)(unit + 0x23a) - 1;
}

/* unit_adjust_plan_overlap (0x1acb70) — FPU quadratic solver for movement plans
 * When two movement plans overlap in time, adjusts the later plan by solving
 * a quadratic equation to trim its acceleration/deceleration phases.
 * Register args: @ecx = plan_a, @eax = plan_b. Stack: dummy, delta_time.
 * Pure math, no side effects beyond plan modification. */
void unit_adjust_plan_overlap(void *plan_a_ptr, void *plan_b_ptr, int dummy,
                              float delta_time)
{
  char *plan_a;
  char *plan_b;
  char *adjust_plan;
  float total_a;
  float total_b;
  float t_extension;
  float t_factor;
  float disc;
  float adj_amount;
  float min_t;
  float accel_t;
  float decel_t;
  float v_peak;

  plan_a = (char *)plan_a_ptr;
  plan_b = (char *)plan_b_ptr;

  /* Both plans must be active (byte 0 == 0) */
  if (*plan_a != 0 || *plan_b != 0) {
    return;
  }

  /* Compute total time for each plan: decel_t + vel_t + accel_t */
  total_a = *(float *)(plan_a + 0x1c) + *(float *)(plan_a + 0x14) +
            *(float *)(plan_a + 0x10);
  total_b = *(float *)(plan_b + 0x1c) + *(float *)(plan_b + 0x14) +
            *(float *)(plan_b + 0x10);

  /* Determine which plan to adjust */
  if (*(float *)(plan_a + 0x10) > 0.0f && total_b > total_a) {
    /* plan_a has priority — adjust plan_b using plan_a's extension */
    t_extension = total_b - total_a;
    adjust_plan = plan_a;
  } else if (*(float *)(plan_b + 0x10) > 0.0f && total_a > total_b) {
    /* plan_b has priority — adjust plan_a using plan_b's extension */
    t_extension = total_a - total_b;
    adjust_plan = plan_b;
  } else {
    return;
  }

  if (adjust_plan == NULL) {
    return;
  }

  /* Assert t_extension > 0 */
  if (t_extension <= 0.0f) {
    display_assert("t_extension > 0",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x8ae, true);
    system_exit(-1);
  }

  /* Solve quadratic for adjustment amount */
  t_factor = (t_extension + *(float *)(adjust_plan + 0x14)) * delta_time;

  {
    float abs_vpeak;
    abs_vpeak = *(float *)(adjust_plan + 0x10) *
                    *(float *)(adjust_plan + 0x0c) +
                *(float *)(adjust_plan + 0x08);
    if (abs_vpeak < 0.0f) {
      abs_vpeak = -abs_vpeak;
    }
    disc = t_factor * t_factor -
           (-t_extension) * abs_vpeak * delta_time * 4.0f;
  }

  /* Assert discriminant >= 0 */
  if (disc < 0.0f) {
    display_assert("disc >= 0", "c:\\halo\\SOURCE\\units\\units.c", 0x8c4,
                   true);
    system_exit(-1);
  }

  /* Quadratic formula: (-b + sqrt(disc)) / (2*a) */
  adj_amount = (sqrtf(disc) - t_factor) / (delta_time + delta_time);

  /* Clamp to min(accel_t, decel_t) */
  if (*(float *)(adjust_plan + 0x10) <= *(float *)(adjust_plan + 0x1c)) {
    min_t = *(float *)(adjust_plan + 0x10);
  } else {
    min_t = *(float *)(adjust_plan + 0x1c);
  }

  if (adj_amount > min_t) {
    if (*(float *)(adjust_plan + 0x10) <= *(float *)(adjust_plan + 0x1c)) {
      adj_amount = *(float *)(adjust_plan + 0x10);
    } else {
      adj_amount = *(float *)(adjust_plan + 0x1c);
    }
  }

  /* Apply adjustment if positive */
  if (adj_amount <= 0.0f) {
    return;
  }

  accel_t = *(float *)(adjust_plan + 0x10) - adj_amount;
  *(float *)(adjust_plan + 0x10) = accel_t;

  v_peak = accel_t * *(float *)(adjust_plan + 0x0c) +
           *(float *)(adjust_plan + 0x08);

  decel_t = *(float *)(adjust_plan + 0x1c) - adj_amount;
  *(float *)(adjust_plan + 0x1c) = decel_t;

  *(float *)(adjust_plan + 0x14) =
      ((v_peak + v_peak + adj_amount * *(float *)(adjust_plan + 0x0c)) *
       adj_amount) / v_peak;

  /* Assert both times remain non-negative */
  if (accel_t < 0.0f || decel_t < 0.0f) {
    display_assert(
        "(adjust_plan->accel_t >= 0) && (adjust_plan->decel_t >= 0)",
        "c:\\halo\\SOURCE\\units\\units.c", 0x8d8, true);
    system_exit(-1);
    return;
  }
}

/* unit_update_running_blind (0x1af340) — update running blind direction
 * Computes a random steering angle for the unit when running blind (no actor
 * guidance). Uses fcos/fsin to rotate the run vector around the global up axis.
 * Register args: @eax = unit_handle, @esi = run_vector (float[3]).
 * No stack params. */
void unit_update_running_blind(int unit_handle, float *run_vector)
{
  char *unit;
  char has_actor_guidance;
  char *msg;
  float max_left;
  float local_val;
  float angle_delta;
  float angle;
  float cos_angle;
  float sin_angle;
  float *up_axis;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  has_actor_guidance = 0;

  if (*(int *)(unit + 0x1a4) == -1 ||
      actor_get_running_blind_vector(*(int *)(unit + 0x1a4), run_vector) ==
          0) {
    /* No actor guidance — use global left vector as default */
    char *left_ptr;
    left_ptr = *(char **)0x31fc3c;
    run_vector[0] = *(float *)left_ptr;
    run_vector[1] = *(float *)(left_ptr + 4);
    run_vector[2] = *(float *)(left_ptr + 8);
  } else {
    has_actor_guidance = 1;
  }

  /* Assert valid normal */
  if (valid_real_normal3d(run_vector) == 0) {
    msg = csprintf(
        (char *)0x5ab100,
        "%s: assert_valid_real_normal3d(%f, %f, %f)", "run_vector",
        (double)run_vector[0], (double)run_vector[1],
        (double)run_vector[2], "c:\\halo\\SOURCE\\units\\units.c", 0x253e,
        true);
    display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x253e, true);
    system_exit(-1);
  }

  local_val = 1.0f;
  max_left = *(float *)0x2533c8; /* 1.0f */

  if (has_actor_guidance) {
    /* Compute max turn rates based on current angle */
    float fwd_range;
    float bwd_range;

    fwd_range =
        (*(float *)0x254a58 - *(float *)(unit + 0x3c4)) * *(float *)0x2b7258;
    bwd_range =
        (*(float *)(unit + 0x3c4) + *(float *)0x254a58) * *(float *)0x2b7258;

    if (fwd_range < *(float *)0x2533c8) {
      local_val = fwd_range;
    }
    if (bwd_range < *(float *)0x2533c8) {
      max_left = bwd_range;
    }
  }

  /* Apply pitch constraints */
  {
    float fwd_pitch;
    float bwd_pitch;

    fwd_pitch =
        (*(float *)0x2b7254 - *(float *)(unit + 0x3c8)) * *(float *)0x2b7250;
    if (fwd_pitch < local_val) {
      local_val = fwd_pitch;
    }

    bwd_pitch =
        (*(float *)(unit + 0x3c8) + *(float *)0x2b7254) * *(float *)0x2b7250;
    if (bwd_pitch < max_left) {
      max_left = bwd_pitch;
    }
  }

  /* Determine random angle delta */
  if (max_left > local_val) {
    /* Right turn dominant */
    if (max_left < *(float *)0x255e94) {
      angle_delta = *(float *)0x2b7248;
      goto apply_angle;
    }
    if (max_left >= *(float *)0x2533c8) {
      max_left = *(float *)0x2533c8;
    }
    max_left = max_left * *(float *)0x2b724c;
    local_val = 0.02094395f;
  } else {
    /* Left turn dominant */
    if (local_val < *(float *)0x255e94) {
      angle_delta = *(float *)0x2b724c;
      goto apply_angle;
    }
    if (local_val >= *(float *)0x2533c8) {
      local_val = *(float *)0x2533c8 * *(float *)0x2b7248;
    } else {
      local_val = local_val * *(float *)0x2b7248;
    }
    max_left = -0.02094395f;
  }

  {
    int *seed;
    seed = get_global_random_seed_address();
    angle_delta = random_real_range((int *)seed, max_left, local_val);
  }

apply_angle:
  /* Accumulate angle delta into unit steering */
  angle_delta += *(float *)(unit + 0x3c8);
  *(float *)(unit + 0x3c8) = angle_delta;

  angle = angle_delta + *(float *)(unit + 0x3c4);
  *(float *)(unit + 0x3c4) = angle;

  /* Wrap angle to [-pi, pi] */
  if (angle < *(float *)0x26e280) {
    *(float *)(unit + 0x3c4) = angle + *(float *)0x255a54;
  } else if (angle > *(float *)0x256980) {
    *(float *)(unit + 0x3c4) = angle - *(float *)0x255a54;
  }

  /* Rotate run_vector by the angle around the global up axis */
  cos_angle = x87_fcos(*(float *)(unit + 0x3c4));
  sin_angle = x87_fsin(*(float *)(unit + 0x3c4));
  up_axis = *(float **)0x31fc44;
  rotate_vector3d_by_sincos(run_vector, up_axis, sin_angle, cos_angle);

  /* Assert valid normal after rotation */
  if (valid_real_normal3d(run_vector) == 0) {
    msg = csprintf(
        (char *)0x5ab100,
        "%s: assert_valid_real_normal3d(%f, %f, %f)", "run_vector",
        (double)run_vector[0], (double)run_vector[1],
        (double)run_vector[2], "c:\\halo\\SOURCE\\units\\units.c", 0x2585,
        true);
    display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x2585, true);
    system_exit(-1);
  }
}

/* FUN_001acd70 (0x1acd70) — unit_try_animation_state
 * Searches the unit's animation graph for a matching seat/weapon animation mode.
 * Register arg: unit_handle in EAX. */
char FUN_001acd70(int unit_handle, const char *seat_label,
                  const char *weapon_name, char apply_state)
{
  char *unit;
  char *unit_tag;
  char *antr_tag;
  int *anim_block;
  int mode_count;
  int16_t mode_index;
  char *mode;
  int sub_count;
  int16_t sub_index;
  char *sub_anim;
  int *weapon_block;
  int weapon_count;
  int16_t weapon_index;
  char *weapon_entry;
  char found;
  char has_multi_weapon;
  int16_t base_seat;
  int16_t si;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (char *)tag_get(0x756e6974, *(int *)unit);
  antr_tag = (char *)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  anim_block = (int *)(antr_tag + 0xc);
  mode_count = *anim_block;
  found = 0;

  if (mode_count < 1)
    return 0;

  mode_index = 0;
  while (1) {
    mode = (char *)tag_block_get_element(anim_block, (int)mode_index, 0x64);

    if (seat_label != 0 &&
        crt_stricmp(seat_label, mode) != 0) {
      goto next_mode;
    }

    sub_count = *(int *)(mode + 0x58);
    sub_index = 0;
    if (sub_count < 1)
      goto next_mode;

    while (1) {
      sub_anim = (char *)tag_block_get_element(
        (int *)(mode + 0x58), (int)sub_index, 0xbc);
      weapon_block = (int *)(sub_anim + 0xb0);
      weapon_count = *weapon_block;
      weapon_index = 0;

      if (weapon_count < 1)
        goto next_sub;

      while (1) {
        weapon_entry = (char *)tag_block_get_element(
          weapon_block, (int)weapon_index, 0x3c);

        if (weapon_name == 0)
          goto matched;
        if (csstrcmp(weapon_name, "unarmed") == 0 &&
            *weapon_entry == '\0')
          goto matched;
        if (crt_stricmp(weapon_name, weapon_entry) == 0)
          goto matched;

        weapon_index++;
        if ((int)(int16_t)weapon_index >= weapon_count)
          goto next_sub;
        continue;

      matched:
        if (apply_state == 0)
          goto found_match;

        {
          int num_key_types;
          int *key_data;

          num_key_types = *(int *)(mode + 0x40);
          key_data = *(int **)(mode + 0x44);
          has_multi_weapon = 0;

          if ((num_key_types >= 3 &&
               *(int16_t *)((char *)key_data + 4) != -1) ||
              (num_key_types >= 4 &&
               *(int16_t *)((char *)key_data + 6) != -1) ||
              (num_key_types >= 5 &&
               *(int16_t *)((char *)key_data + 8) != -1)) {
            has_multi_weapon = 1;
          }
        }

        if (*(uint8_t *)(unit + 0x253) != 0x1c)
          *(uint8_t *)(unit + 0x253) = 0xff;

        *(uint8_t *)(unit + 0x250) = (uint8_t)mode_index;

        base_seat = -1;
        for (si = 0; si < NUMBER_OF_UNIT_BASE_SEATS; si++) {
          if (crt_stricmp(seat_label,
                          *(const char **)(0x32e484 + (int)si * 4)) == 0) {
            base_seat = si;
            break;
          }
        }

        *(uint8_t *)(unit + 0x252) = (uint8_t)weapon_index;
        *(int8_t *)(unit + 0x257) = (int8_t)base_seat;
        *(uint8_t *)(unit + 0x251) = (uint8_t)sub_index;

        if (has_multi_weapon) {
          *(uint8_t *)(unit + 0x248) |= 0x2;
        } else {
          *(uint8_t *)(unit + 0x248) &= ~0x2;
        }

      found_match:
        found = 1;
        goto next_sub;
      }

    next_sub:
      sub_index++;
      if ((int)(int16_t)sub_index >= *(int *)(mode + 0x58))
        goto next_mode;
    }

  next_mode:
    mode_index++;
    if ((int)(int16_t)mode_index >= *anim_block)
      break;
  }

  return found;
}

/* FUN_001ae490 (0x1ae490) — unit_next_weapon_index
 * Scans weapon slots circularly for the next valid weapon.
 * Register arg: unit_handle in EBX. */
int16_t FUN_001ae490(int unit_handle, int16_t current_index, int16_t direction)
{
  char *unit;
  int iter_index;
  int weapon_handle;
  int seat_label;
  char *weapon_label;
  char can_use;
  char usable;
  char readied;
  int best_index;
  int current;

  unit = (char *)object_get_and_verify_type(unit_handle, 3);
  best_index = -1;

  if (current_index == (int16_t)-1) {
    current_index = 0;
  } else if (current_index < 0 || current_index >= MAXIMUM_WEAPONS_PER_UNIT) {
    display_assert(
      "current_index>=0 && current_index<MAXIMUM_WEAPONS_PER_UNIT",
      "c:\\halo\\SOURCE\\units\\units.c", 0x1e40, 1);
    system_exit(-1);
  }

  current = current_index;

  do {
    iter_index = (int)(int16_t)current;
    weapon_handle = *(int *)(unit + 0x2a8 + iter_index * 4);

    if (weapon_handle != -1) {
      object_get_and_verify_type(unit_handle, 3);
      object_get_and_verify_type(weapon_handle, 4);

      seat_label = unit_get_seat_label(unit_handle);
      weapon_label = (char *)weapon_get_label(weapon_handle);
      can_use = FUN_001acd70(unit_handle, (const char *)seat_label,
                             (const char *)weapon_label, 0);

      if (can_use != 0) {
        usable = (char)game_engine_allow_weapon_pick_up(unit_handle, weapon_handle);
        if (usable != 0) {
          if (direction != 0) {
            best_index = current;
          } else {
            if ((int16_t)best_index == (int16_t)-1 ||
                *(int *)(unit + 0x2b8 + (int)(int16_t)best_index * 4) <
                  *(int *)(unit + 0x2b8 + iter_index * 4)) {
              best_index = current;
            }
          }

          readied = (char)weapon_must_be_readied(
            *(int *)(unit + 0x2a8 + iter_index * 4));
          if (readied != 0)
            return (int16_t)best_index;

          if ((int16_t)current != current_index)
            return (int16_t)best_index;
        }
      }
    }

    if (direction < 0) {
      if ((int16_t)current == 0)
        current = 3;
      else
        current = iter_index - 1;
    } else {
      if ((int16_t)current == 3)
        current = 0;
      else
        current = iter_index + 1;
    }
  } while ((int16_t)current != current_index);

  return (int16_t)best_index;
}

/* FUN_001b04b0 (0x1b04b0) — unit_postprocess_nodes
 * Applies IK constraints and weapon-hold overlays to animation node matrices. */
void FUN_001b04b0(int unit_handle, int node_matrices)
{
  unsigned int *unit_data;
  int unit_tag;
  int anim_graph;
  int anim_mode;
  int mode_ext;
  int ik_point;
  int weapon_ik_point;
  unsigned int *unit_data2;
  int weapon_handle;
  short ik_index;
  unsigned char *anim_ctrl;
  char ik_active;
  char weapon_ik_active;
  int *ik_block;
  int *weapon_ik_block;
  int idx;

  unit_data = (unsigned int *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *(int *)unit_data);

  if ((*(unsigned int *)(unit_tag + 0x17c) & 0x800) != 0) {
    return;
  }
  if (*(signed char *)((int)unit_data + 0x250) == -1) {
    return;
  }

  anim_graph = (int)tag_get(0x616e7472, *(int *)(unit_tag + 0x44));
  anim_mode = (int)tag_block_get_element(
    (void *)(anim_graph + 0xc),
    (int)*(signed char *)((int)unit_data + 0x250), 100);
  mode_ext = (int)tag_block_get_element(
    (void *)(anim_mode + 0x58),
    (int)*(signed char *)((int)unit_data + 0x251), 0xbc);

  if (*(int *)((int)unit_data + 0xcc) != -1) {
    ik_active = FUN_001a8850((void *)((int)unit_data + 0x248));
    if (ik_active != '\0') {
      ik_block = (int *)(anim_mode + 0x4c);
      ik_index = 0;
      if (0 < *(int *)(anim_mode + 0x4c)) {
        idx = 0;
        do {
          ik_point = (int)tag_block_get_element(ik_block, idx, 0x40);
          FUN_001414e0(unit_handle, ik_point,
                       *(int *)((int)unit_data + 0xcc),
                       ik_point + 0x20, node_matrices);
          ik_index = ik_index + 1;
          idx = (int)ik_index;
        } while (idx < *ik_block);
      }
    }
  }

  if (*(short *)((int)unit_data + 0x2a2) != -1) {
    anim_ctrl = (unsigned char *)((int)unit_data + 0x248);
    weapon_ik_active = FUN_001a87f0((void *)anim_ctrl);
    if (weapon_ik_active != '\0') {
      weapon_ik_block = (int *)(mode_ext + 0xa4);
      ik_index = 0;
      if (0 < *(int *)(mode_ext + 0xa4)) {
        idx = 0;
        do {
          weapon_ik_point = (int)tag_block_get_element(
            weapon_ik_block, idx, 0x40);
          unit_data2 = (unsigned int *)object_get_and_verify_type(
            unit_handle, 3);
          weapon_handle = unit_get_weapon(unit_handle,
            (short)*(unsigned short *)((int)unit_data2 + 0x2a2));
          FUN_001414e0(unit_handle, weapon_ik_point,
                       weapon_handle,
                       weapon_ik_point + 0x20, node_matrices);
          ik_index = ik_index + 1;
          idx = (int)ik_index;
        } while (idx < *weapon_ik_block);
      }
      *anim_ctrl = *anim_ctrl & 0xfe;
    }
  }
}

/* -----------------------------------------------------------------------
 * FUN_00122a50 — animation 1D overlay frame apply
 *
 * For animation type 1 (overlay), interpolates rotation, translation, and
 * scale between the current frame and next frame for each node.  The result
 * is blended into the node output buffer using blend_weight.
 *
 * Disassembly range: 0x122a50 – 0x122e43.
 * Source: c:\halo\SOURCE\models\model_animations.c
 * ----------------------------------------------------------------------- */
void FUN_00122a50(int animation, float frame_pos, float blend_weight,
                  int node_output)
{
  short *data;
  short *next_data;
  unsigned short frame_count_u;
  int compressed;
  short *data_cursor;
  short *next_cursor;
  int frame_index;
  int next_frame_index;
  int node_output_ptr;
  float weight_complement;
  float frac;
  float floor_val;
  int frame_idx_int;
  int rotation_counter;
  int translation_counter;
  int scale_counter;
  unsigned int node_idx;
  unsigned int has_translation;
  unsigned int has_rotation;
  unsigned int has_scale;
  float rot_a[4];
  float rot_b[4];
  float interp_rot[4];
  float interp_trans[3];
  float interp_scale;
  int temp_scale_a;
  int temp_scale_b;

  weight_complement = *(float *)0x2533c8 - blend_weight;
  frac = (float)x87_fmod(frame_pos, 1.0);
  floor_val = (float)floor((double)frame_pos);
  frame_idx_int = (int)floor_val;

  if (frame_pos < *(float *)0x2533c0 ||
      frame_pos > (float)(int)*(short *)(animation + 0x22)) {
    error(2,
      "### ERROR animation frame index out of bounds B(%f,%x) -- tell Bernie!!",
      (double)frame_pos, *(int *)&frame_pos);
  }

  frame_count_u = *(unsigned short *)(animation + 0x22);
  if ((short)frame_idx_int >= (short)frame_count_u) {
    frame_idx_int = (int)(unsigned short)(frame_count_u - 1);
    frac = 1.0f;
    frame_pos = (float)(int)(short)frame_idx_int;
  }

  if (*(short *)(animation + 0x20) == 1) {
    if ((*(unsigned char *)(animation + 0x3a) & 1) == 0 ||
        (*(char *)0x322600 == '\0' &&
         *(int *)(animation + 0x88) != 0)) {
      compressed = 0;
    } else {
      compressed = 1;
    }

    frame_index = (int)(short)frame_idx_int;
    if (frame_index == (int)(short)frame_count_u - 1) {
      next_frame_index = 0;
    } else {
      next_frame_index = frame_index + 1;
    }

    data = (short *)FUN_00120500((void *)animation, (short)frame_idx_int);
    next_data = (short *)FUN_00120500((void *)animation,
                                      (short)next_frame_index);

    rotation_counter = 0;
    translation_counter = 0;
    scale_counter = 0;
    node_idx = 0;
    if (0 < *(short *)(animation + 0x2c)) {
      do {
        node_output_ptr = (short)node_idx * 0x20 + node_output;
        if ((node_idx & 0x1f) == 0) {
          int bit_idx = (int)(short)((short)node_idx >> 5);
          has_translation = *(unsigned int *)(animation + 0x5c + bit_idx * 4);
          has_rotation = *(unsigned int *)(animation + 0x6c + bit_idx * 4);
          has_scale = *(unsigned int *)(animation + 0x7c + bit_idx * 4);
        }

        data_cursor = data;
        next_cursor = next_data;

        if ((has_rotation & 1) != 0) {
          if (compressed) {
            FUN_00121330((void *)animation, (float)frame_index,
                         (unsigned short)rotation_counter,
                         (short)node_idx, interp_rot);
            rotation_counter = rotation_counter + 1;
          } else {
            rot_a[0] = (float)(int)data[0] * *(float *)0x290dd8;
            rot_a[1] = (float)(int)data[1] * *(float *)0x290dd8;
            rot_a[2] = (float)(int)data[2] * *(float *)0x290dd8;
            rot_a[3] = (float)(int)data[3] * *(float *)0x290dd8;
            rot_b[0] = (float)(int)next_data[0] * *(float *)0x290dd8;
            rot_b[1] = (float)(int)next_data[1] * *(float *)0x290dd8;
            rot_b[2] = (float)(int)next_data[2] * *(float *)0x290dd8;
            rot_b[3] = (float)(int)next_data[3] * *(float *)0x290dd8;
            data = data + 4;
            next_data = next_data + 4;
            quaternions_interpolate_and_normalize(rot_a, rot_b, frac,
                                                  interp_rot);
          }
          quaternions_interpolate_and_normalize(*(float **)0x31fc5c,
                                                interp_rot, blend_weight,
                                                interp_rot);
          FUN_0010b9c0(interp_rot, (float *)node_output_ptr,
                       (float *)node_output_ptr);
          data_cursor = data;
        }
        has_rotation = has_rotation >> 1;
        next_cursor = next_data;
        data = data_cursor;

        if ((has_translation & 1) != 0) {
          if (compressed) {
            animation_get_node_orientations(
              (void *)animation, frame_pos,
              (unsigned short)translation_counter,
              (short)node_idx, interp_trans);
            translation_counter = translation_counter + 1;
          } else {
            points_interpolate((float *)data_cursor, (float *)next_cursor,
                               frac, interp_trans);
            data = data_cursor + 6;
            next_data = next_cursor + 6;
          }
          *(float *)(node_output_ptr + 0x10) =
            interp_trans[0] * blend_weight +
            *(float *)(node_output_ptr + 0x10);
          *(float *)(node_output_ptr + 0x14) =
            interp_trans[1] * blend_weight +
            *(float *)(node_output_ptr + 0x14);
          *(float *)(node_output_ptr + 0x18) =
            interp_trans[2] * blend_weight +
            *(float *)(node_output_ptr + 0x18);
        }
        has_translation = has_translation >> 1;

        if ((has_scale & 1) != 0) {
          if (compressed) {
            overlay_animation_apply_continuous_scaled(
              (void *)animation, frame_pos,
              (unsigned short)scale_counter,
              (short)node_idx, &interp_scale);
            scale_counter = scale_counter + 1;
          } else {
            temp_scale_a = *(int *)data;
            temp_scale_b = *(int *)next_data;
            data = data + 2;
            next_data = next_data + 2;
            scalars_interpolate(*(float *)&temp_scale_a,
                                *(float *)&temp_scale_b,
                                frac, &interp_scale);
          }
          *(float *)(node_output_ptr + 0x1c) =
            (interp_scale * blend_weight + weight_complement) *
            *(float *)(node_output_ptr + 0x1c);
        }
        has_scale = has_scale >> 1;
        node_idx = node_idx + 1;
      } while ((short)node_idx < *(short *)(animation + 0x2c));
    }

    if (!compressed) {
      int check_base;
      check_base = (int)FUN_00120500((void *)animation,
                                      (short)frame_idx_int);
      if ((int)data - check_base !=
          (int)*(short *)(animation + 0x24)) {
        display_assert(
          "compressed || ((byte *)data-(byte *)animation_get_frame_data"
          "(animation, frame_index)==animation->frame_size)",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x334, 1);
        system_exit(-1);
      }
      check_base = (int)FUN_00120500((void *)animation,
                                      (short)next_frame_index);
      if ((int)next_data - check_base !=
          (int)*(short *)(animation + 0x24)) {
        display_assert(
          "compressed || ((byte *)next_data-(byte *)"
          "animation_get_frame_data(animation, next_frame_index)"
          "==animation->frame_size)",
          "c:\\halo\\SOURCE\\models\\model_animations.c", 0x335, 1);
        system_exit(-1);
      }
    }
  }
}

/* -----------------------------------------------------------------------
 * FUN_00122e50 — animation 2D blend
 *
 * Performs 2D bilinear interpolation of 4 corner animation frames,
 * blending rotation (quaternion slerp) and translation for each node
 * based on direction and throttle parameters.
 *
 * Disassembly range: 0x122e50 – 0x123462.
 * Source: c:\halo\SOURCE\models\model_animations.c
 * ----------------------------------------------------------------------- */
void FUN_00122e50(int animation, float *blend_params, float direction,
                  float throttle, int node_output)
{
  int direction_count;
  int throttle_count;
  int throttle_count_s;
  short dir_count_s;
  char is_compressed;
  float yaw_range;
  float ratio;
  int dir_frame;
  float dir_frac;
  unsigned short neg_dir_offset;
  unsigned short neg_thr_offset;
  int thr_frame;
  float thr_frac;
  short thr_s;
  short dir_s;
  int frame_00;
  int frame_10;
  int frame_01;
  int frame_11;
  short *data_00;
  short *data_10;
  short *data_01;
  short *data_11;
  int rotation_counter;
  unsigned int node_idx;
  int node_out_ptr;
  unsigned int has_translation;
  unsigned int has_rotation;
  int translation_counter;
  float dir_complement;
  float thr_complement;
  float rot_00[4];
  float rot_10[4];
  float rot_01[4];
  float rot_11[4];
  float blend_a[4];
  float blend_b[4];
  float final_rot[4];
  float trans_00[3];
  float trans_10[3];
  float trans_01[3];
  float trans_11[3];
  int temp_int;

  direction_count = (unsigned short)(*(short *)(blend_params + 2) +
                    *(short *)((int)blend_params + 10)) + 1;
  throttle_count = (unsigned short)(*(short *)(blend_params + 5) +
                   *(short *)((int)blend_params + 0x16)) + 1;

  if (*(short *)(animation + 0x20) != 1) {
    return;
  }

  throttle_count_s = (int)(short)throttle_count;
  dir_count_s = (short)direction_count;
  if (dir_count_s * throttle_count_s > (int)*(short *)(animation + 0x22)) {
    return;
  }

  is_compressed = FUN_00120620(animation);

  /* Direction axis */
  if (direction >= *(float *)0x2533c0) {
    yaw_range = blend_params[1];
  } else {
    yaw_range = blend_params[0];
  }
  if (yaw_range == *(float *)0x2533c0) {
    ratio = 0.0f;
  } else {
    ratio = direction / yaw_range;
  }

  dir_frame = (int)ratio;
  dir_frac = (float)x87_fmod(ratio, 1.0);
  if (dir_frac < *(float *)0x2533c0) {
    dir_frame = dir_frame - 1;
    dir_frac = dir_frac + *(float *)0x2533c8;
  }

  neg_dir_offset = *(unsigned short *)((int)blend_params + 10);
  if ((short)dir_frame >= (short)neg_dir_offset) {
    dir_frame = (int)(unsigned short)(neg_dir_offset - 1);
    dir_frac = 1.0f;
  }

  neg_dir_offset = *(unsigned short *)(blend_params + 2);
  if ((int)(short)dir_frame < -(int)(short)neg_dir_offset) {
    dir_frame = -(int)(unsigned int)neg_dir_offset;
    dir_frac = 0.0f;
  }
  dir_frame = dir_frame + (unsigned int)neg_dir_offset;

  if (dir_frac < *(float *)0x2533c0 ||
      !(dir_frac < *(float *)0x2533c8 || dir_frac == *(float *)0x2533c8)) {
    csprintf((char *)0x5ab100,
      "d0==%f direction(%f) yaw_delta(%f,%f)",
      (double)dir_frac, (double)direction,
      (double)blend_params[0], (double)blend_params[1]);
    display_assert((char *)0x5ab100,
      "c:\\halo\\SOURCE\\models\\model_animations.c", 0x365, 1);
    system_exit(-1);
  }

  /* Throttle axis */
  if (throttle >= *(float *)0x2533c0) {
    yaw_range = blend_params[4];
  } else {
    yaw_range = blend_params[3];
  }
  if (yaw_range == *(float *)0x2533c0) {
    ratio = 0.0f;
  } else {
    ratio = throttle / yaw_range;
  }

  thr_frame = (int)ratio;
  thr_frac = (float)x87_fmod(ratio, 1.0);
  if (thr_frac < *(float *)0x2533c0) {
    thr_frame = thr_frame - 1;
    thr_frac = thr_frac + *(float *)0x2533c8;
  }

  neg_thr_offset = *(unsigned short *)((int)blend_params + 0x16);
  if ((short)thr_frame >= (short)neg_thr_offset) {
    thr_frame = (int)(unsigned short)(neg_thr_offset - 1);
    thr_frac = 1.0f;
  }

  neg_thr_offset = *(unsigned short *)(blend_params + 5);
  if ((int)(short)thr_frame < -(int)(short)neg_thr_offset) {
    thr_frame = -(int)(unsigned int)neg_thr_offset;
    thr_frac = 0.0f;
  }
  thr_frame = thr_frame + (unsigned int)neg_thr_offset;

  thr_s = (short)thr_frame;
  dir_s = (short)dir_frame;

  if (thr_s < 0 || thr_s >= (short)throttle_count ||
      dir_s < 0 || dir_s >= dir_count_s) {
    return;
  }

  {
    int next_dir;
    int next_thr;
    next_dir = (int)(short)dir_s + 1;
    if (next_dir == (int)(short)dir_count_s) {
      next_dir = (int)(short)dir_s;
    }
    next_thr = (int)(short)thr_s + 1;
    if (next_thr == throttle_count_s) {
      next_thr = (int)(short)thr_s;
    }

    frame_00 = thr_frame * direction_count + dir_frame;
    frame_10 = thr_frame * direction_count + next_dir;
    frame_01 = dir_frame + next_thr * direction_count;
    frame_11 = next_thr * direction_count + next_dir;

    data_00 = (short *)FUN_00120500((void *)animation, (short)frame_00);
    data_10 = (short *)FUN_00120500((void *)animation, (short)frame_10);
    data_01 = (short *)FUN_00120500((void *)animation, (short)frame_01);
    data_11 = (short *)FUN_00120500((void *)animation, (short)frame_11);

    translation_counter = 0;
    node_idx = 0;
    rotation_counter = 0;

    if (0 >= *(short *)(animation + 0x2c)) {
      return;
    }

    do {
      node_out_ptr = (short)node_idx * 0x20 + node_output;
      if ((node_idx & 0x1f) == 0) {
        int bit_idx = (int)(short)((short)node_idx >> 5);
        has_translation = *(unsigned int *)(animation + 0x5c + bit_idx * 4);
        has_rotation = *(unsigned int *)(animation + 0x6c + bit_idx * 4);
      }

      if ((has_rotation & 1) != 0) {
        if (is_compressed != '\0') {
          temp_int = (int)(short)frame_00;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter,
                       (short)node_idx, rot_00);
          temp_int = (int)(short)frame_10;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter,
                       (short)node_idx, rot_10);
          temp_int = (int)(short)frame_01;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter,
                       (short)node_idx, rot_01);
          temp_int = (int)(short)frame_11;
          FUN_00121330((void *)animation, (float)temp_int,
                       (unsigned short)rotation_counter,
                       (short)node_idx, rot_11);
          rotation_counter = rotation_counter + 1;
        } else {
          quaternion_decompress_8byte(data_00, rot_00);
          data_00 = data_00 + 4;
          quaternion_decompress_8byte(data_10, rot_10);
          data_10 = data_10 + 4;
          quaternion_decompress_8byte(data_01, rot_01);
          data_01 = data_01 + 4;
          quaternion_decompress_8byte(data_11, rot_11);
          data_11 = data_11 + 4;
        }
        quaternions_interpolate_and_normalize(rot_00, rot_10, dir_frac,
                                              blend_a);
        quaternions_interpolate_and_normalize(rot_01, rot_11, dir_frac,
                                              blend_b);
        quaternions_interpolate_and_normalize(blend_a, blend_b, thr_frac,
                                              final_rot);
        FUN_0010b9c0(final_rot, (float *)node_out_ptr,
                     (float *)node_out_ptr);
      }
      has_rotation = has_rotation >> 1;

      if ((has_translation & 1) != 0) {
        dir_complement = *(float *)0x2533c8 - dir_frac;
        thr_complement = *(float *)0x2533c8 - thr_frac;

        if (is_compressed != '\0') {
          temp_int = (int)(short)frame_00;
          animation_get_node_orientations(
            (void *)animation, (float)temp_int,
            (unsigned short)translation_counter,
            (short)node_idx, trans_00);
          temp_int = (int)(short)frame_10;
          animation_get_node_orientations(
            (void *)animation, (float)temp_int,
            (unsigned short)translation_counter,
            (short)node_idx, trans_10);
          temp_int = (int)(short)frame_01;
          animation_get_node_orientations(
            (void *)animation, (float)temp_int,
            (unsigned short)translation_counter,
            (short)node_idx, trans_01);
          temp_int = (int)(short)frame_11;
          animation_get_node_orientations(
            (void *)animation, (float)temp_int,
            (unsigned short)translation_counter,
            (short)node_idx, trans_11);
          translation_counter = translation_counter + 1;
        } else {
          trans_00[0] = *(float *)data_00;
          trans_00[1] = *((float *)data_00 + 1);
          trans_00[2] = *((float *)data_00 + 2);
          data_00 = data_00 + 6;
          trans_10[0] = *(float *)data_10;
          trans_10[1] = *((float *)data_10 + 1);
          trans_10[2] = *((float *)data_10 + 2);
          data_10 = data_10 + 6;
          trans_01[0] = *(float *)data_01;
          trans_01[1] = *((float *)data_01 + 1);
          trans_01[2] = *((float *)data_01 + 2);
          data_01 = data_01 + 6;
          trans_11[0] = *(float *)data_11;
          trans_11[1] = *((float *)data_11 + 1);
          trans_11[2] = *((float *)data_11 + 2);
          data_11 = data_11 + 6;
        }

        *(float *)(node_out_ptr + 0x10) =
          (trans_10[0] * dir_frac + trans_00[0] * dir_complement) *
            thr_complement +
          (trans_11[0] * dir_frac + trans_01[0] * dir_complement) *
            thr_frac +
          *(float *)(node_out_ptr + 0x10);
        *(float *)(node_out_ptr + 0x14) =
          (trans_10[1] * dir_frac + trans_00[1] * dir_complement) *
            thr_complement +
          (trans_11[1] * dir_frac + trans_01[1] * dir_complement) *
            thr_frac +
          *(float *)(node_out_ptr + 0x14);
        *(float *)(node_out_ptr + 0x18) =
          (trans_10[2] * dir_frac + trans_00[2] * dir_complement) *
            thr_complement +
          (trans_11[2] * dir_frac + trans_01[2] * dir_complement) *
            thr_frac +
          *(float *)(node_out_ptr + 0x18);
      }
      has_translation = has_translation >> 1;
      node_idx = node_idx + 1;
    } while ((short)node_idx < *(short *)(animation + 0x2c));
  }
}

/* -----------------------------------------------------------------------
 * FUN_00123560 — model render geometry (multi-pass)
 *
 * Iterates over model geometry groups and parts, performing up to 3
 * rendering passes (opaque, environment-mapped, transparent).  For each
 * shader part, dispatches to the appropriate render function based on
 * shader type and pass index.
 *
 * Disassembly range: 0x123560 – 0x12398b.
 * Source: c:\halo\SOURCE\models\models.c
 * ----------------------------------------------------------------------- */
void FUN_00123560(int model_tag, int permutation_data, int *node_matrices,
                  int render_data, short shader_permutation,
                  short detail_level, unsigned char flags)
{
  char shader_valid;
  char shader_transparent;
  short shader_perm_idx;
  int geometry;
  unsigned char *part;
  int shader_ref;
  int shader_tag;
  int pass;
  unsigned int pass_limit;
  int geom_idx;
  int part_idx;
  int *parts_block;
  int typed_shader;
  short actual_detail;
  unsigned int env_count;
  int env_slot;
  /* Local array for environment map cross-referencing.
   * Each entry is 16 bytes: [0]=ptr(2b), [2]=short, [4]=short, [6]=short.
   * Max 32 entries. */
  unsigned char env_data[32 * 16];
  float transformed_centroid[3];

  pass = 0;
  pass_limit = (unsigned int)((unsigned char)~flags & 2);

  do {
    geom_idx = 0;
    env_count = 0;

    if (0 < *(int *)(model_tag + 0xc4)) {
      do {
        geometry = (int)tag_block_get_element(
          (void *)(model_tag + 0xc4), geom_idx, 0x4c);

        if (*(char *)(geom_idx + permutation_data) != -1) {
          int perm_block;
          perm_block = (int)tag_block_get_element(
            (void *)(geometry + 0x40),
            (int)*(char *)(geom_idx + permutation_data), 0x58);
          shader_perm_idx =
            *(short *)(perm_block + 0x40 + shader_permutation * 2);

          if (*(char *)0x5aa250 == '\0' && shader_perm_idx != -1) {
            int section;
            section = (int)tag_block_get_element(
              (void *)(model_tag + 0xd0),
              (int)shader_perm_idx, 0x30);
            part_idx = 0;
            parts_block = (int *)(section + 0x24);

            if (0 < *(int *)(section + 0x24)) {
              int pi;
              pi = 0;
              do {
                part = (unsigned char *)tag_block_get_element(
                  (void *)parts_block, pi, 0x68);
                shader_ref = (int)tag_block_get_element(
                  (void *)(model_tag + 0xdc),
                  (int)*(short *)(part + 4), 0x20);
                shader_tag = (int)tag_get(0x73686472,
                  *(int *)(shader_ref + 0xc));

                shader_valid = ((char (*)(int))shader_type_is_valid_for_model)(
                  (int)*(unsigned short *)(shader_tag + 0x24));

                if (shader_valid != '\0' && (part[0] & 1) == 0) {
                  shader_transparent = shader_type_is_transparent(
                    *(short *)(shader_tag + 0x24));

                  if (shader_transparent != '\0') {
                    /* Transparent shader — pass 2 */
                    if ((short)pass == 2) {
                      if ((flags & 2) != 0) {
                        display_assert(
                          "!TEST_FLAG(flags, _render_model_shadow_bit)",
                          "c:\\halo\\SOURCE\\models\\models.c", 0x1ba, 1);
                        system_exit(-1);
                      }
                      if (*(short *)(part + 8) < 0 ||
                          (int)*(short *)(part + 8) >=
                            *(int *)(model_tag + 0xb8)) {
                        display_assert(
                          "part->centroid_primary_node_index>=0 && "
                          "part->centroid_primary_node_index<"
                          "model->nodes.count",
                          "c:\\halo\\SOURCE\\models\\models.c", 0x1bd, 1);
                        system_exit(-1);
                      }
                      if (*(short *)(part + 10) < 0 ||
                          (int)*(short *)(part + 10) >=
                            *(int *)(model_tag + 0xb8)) {
                        display_assert(
                          "part->centroid_secondary_node_index>=0 && "
                          "part->centroid_secondary_node_index<"
                          "model->nodes.count",
                          "c:\\halo\\SOURCE\\models\\models.c", 0x1be, 1);
                        system_exit(-1);
                      }
                      matrix_transform_point(
                        (float *)((int)*(short *)(part + 8) * 0x34 +
                                  *node_matrices),
                        (float *)(part + 0x14),
                        transformed_centroid);
                      actual_detail = detail_level;
                      if (detail_level == 0) {
                        actual_detail = *(short *)(shader_ref + 0x10);
                      }

                      env_slot = (int)(short)env_count;
                      FUN_0017cbd0(shader_tag, (int)actual_detail,
                                   (int)(part + 0x44), -1,
                                   *(int *)(part + 0x48),
                                   (int)(part + 0x54), -1,
                                   transformed_centroid,
                                   (int)&env_data[env_slot * 16]);

                      if ((short)env_count < 0x20 &&
                          *(short *)&env_data[env_slot * 16 + 8] != -1 &&
                          (flags & 1) == 0 &&
                          ((char)part[7] > '\0' || (char)part[6] > '\0')) {
                        *(short *)&env_data[env_slot * 16 + 12] =
                          (short)part_idx;
                        *(short *)&env_data[env_slot * 16 + 10] =
                          (short)(char)part[7];
                        env_count = env_count + 1;
                      }
                    }
                  } else {
                    /* Non-transparent shader */
                    if (*(short *)(shader_tag + 0x24) == 4) {
                      typed_shader = ((int (*)(int, int))FUN_001906b0)(
                        shader_tag, 4);
                      if ((*(unsigned char *)(typed_shader + 0x28) & 8) != 0) {
                        /* Environment-mapped shader — pass 1 */
                        if ((short)pass == 1) {
                          if ((flags & 2) != 0) {
                            display_assert(
                              "!TEST_FLAG(flags, _render_model_shadow_bit)",
                              "c:\\halo\\SOURCE\\models\\models.c",
                              0x1eb, 1);
                            system_exit(-1);
                          }
                          actual_detail = detail_level;
                          if (detail_level == 0) {
                            actual_detail = *(short *)(shader_ref + 0x10);
                          }
                          FUN_0017cbc0(shader_tag, (int)actual_detail,
                                       (int)(part + 0x44), -1,
                                       *(int *)(part + 0x48),
                                       (int)(part + 0x54), -1);
                        }
                        goto next_part;
                      }
                    }

                    /* Opaque shader — pass 0 */
                    if ((short)pass == 0) {
                      if ((flags & 2) == 0) {
                        actual_detail = detail_level;
                        if (detail_level == 0) {
                          actual_detail = *(short *)(shader_ref + 0x10);
                        }
                        FUN_0017cbc0(shader_tag, (int)actual_detail,
                                     (int)(part + 0x44), -1,
                                     *(int *)(part + 0x48),
                                     (int)(part + 0x54), -1);
                        ((void (*)(int, int *, unsigned char *))FUN_0017d2b0)(
                          render_data, node_matrices, part);
                      } else {
                        actual_detail = detail_level;
                        if (detail_level == 0) {
                          actual_detail = *(short *)(shader_ref + 0x10);
                        }
                        FUN_0017ccd0((void *)shader_tag, (int)actual_detail,
                                     (void *)(part + 0x44),
                                     (void *)(part + 0x54));
                      }
                    }
                  }
                }
next_part:
                part_idx = part_idx + 1;
                pi = (int)(short)part_idx;
              } while (pi < *parts_block);
            }
          }
        }

        geom_idx = (int)(short)((short)geom_idx + 1);
      } while (geom_idx < *(int *)(model_tag + 0xc4));

      /* Post-pass: cross-reference environment map entries.
       * Each 16-byte entry:
       *   [0..3] = ptr_a (int), [4..7] = ptr_b (int),
       *   [8..9] = short val, [10..11] = (short)(char)part[7],
       *   [12..13] = (short)part_idx */
      if (0 < (short)env_count) {
        unsigned int i;
        unsigned int env_total;
        env_total = env_count & 0xffff;
        for (i = 0; i < env_total; i++) {
          short j;
          short match_field = *(short *)&env_data[i * 16 + 10];
          for (j = 0; j < (short)env_count; j++) {
            if (match_field == *(short *)&env_data[j * 16 + 12] &&
                0 < match_field) {
              *(short *)(*(int *)&env_data[i * 16 + 4]) =
                *(short *)&env_data[j * 16 + 8];
              *(short *)(*(int *)&env_data[j * 16]) =
                *(short *)&env_data[i * 16 + 8];
              break;
            }
          }
        }
      }
    }

    pass = pass + 1;
    if ((short)pass_limit < (short)pass) {
      return;
    }
  } while (1);
}

/* unit_cause_player_melee_damage (0x1aea90) — Player melee damage sweep
 *
 * Performs a 5x5 raycast sweep in the unit's facing direction to find
 * melee collision targets. Applies melee damage to the best candidate,
 * accelerates hit bipeds, and plays clang sound effects on material hits.
 *
 * Confirmed: cdecl, 1 stack param (unit_handle).
 * Confirmed: uses global_current_collision_user_depth at 0x4761d8.
 * Confirmed: 0x2533f0 = 0.8f, 0x25496c = 0.1f.
 * Confirmed: 0x2b7204 = 0.035f, 0x253394 = 30.0f.
 */
void unit_cause_player_melee_damage(int unit_handle)
{
  unsigned int *unit;
  int unit_tag_data;
  int best_object;
  int hit_target;
  unsigned int *weapon_data;
  int weapon_tag_data;
  int melee_damage_effect;
  int melee_response_effect;
  float *facing;
  float perp[3];
  float cross_vec[3];
  float ray_dir[3];
  float ray_origin[3];
  char collision_result[80];
  char marker_buf[0x6c];
  int16_t depth;
  int outer_i;
  int outer_count;
  int inner_i;
  int inner_count;
  float fi;
  float fj;
  char hit;
  char *obj_data;
  int parent_handle;
  short best_type;
  float best_fraction;
  int16_t material_idx;
  unsigned int material_surface;
  int16_t current_weapon_idx;
  int globals;
  char *globals_element_ptr;
  float dot_product;
  float melee_scale;
  float kick_vec[3];
  int16_t hit_material;
  int weapon_handle;
  char damage_data[0x54];

  unit = (unsigned int *)object_get_and_verify_type(unit_handle, 3);
  unit_tag_data = (int)tag_get(0x756e6974, *unit);
  best_object = -1;
  hit_material = -1;
  material_idx = -1;
  melee_response_effect = -1;

  object_get_markers_by_string_id(unit_handle, (void *)0x2909e4,
                                  marker_buf, 1);

  ray_origin[0] = *(float *)(marker_buf + 0x60);
  ray_origin[1] = *(float *)(marker_buf + 0x64);
  ray_origin[2] = *(float *)(marker_buf + 0x68);

  if (global_current_collision_user_depth >=
      MAXIMUM_COLLISION_USER_STACK_DEPTH) {
    display_assert(
        "global_current_collision_user_depth < "
        "MAXIMUM_COLLISION_USER_STACK_DEPTH",
        "c:\\halo\\SOURCE\\units\\units.c", 0x2212, 1);
    system_exit(-1);
  }
  depth = global_current_collision_user_depth;
  global_current_collision_user_depth = depth + 1;
  collision_user_stack[depth] = 8;

  facing = (float *)((char *)unit + 0x1ec);
  perpendicular3d(facing, perp);
  normalize3d(perp);

  best_type = 0;
  best_fraction = 0.0f;

  cross_vec[0] = perp[2] * facing[1] - perp[1] * facing[2];
  cross_vec[1] = perp[0] * facing[2] - perp[2] * facing[0];
  cross_vec[2] = perp[1] * facing[0] - perp[0] * facing[1];

  outer_i = -2;
  outer_count = 5;
  do {
    fi = (float)outer_i;
    inner_i = -2;
    inner_count = 5;
    do {
      fj = (float)inner_i;

      ray_dir[0] = facing[0] * 0.8f +
                   (fi * perp[0] + cross_vec[0] * fj) * 0.1f;
      ray_dir[1] = facing[1] * 0.8f +
                   (fi * perp[1] + cross_vec[1] * fj) * 0.1f;
      ray_dir[2] = facing[2] * 0.8f +
                   (fi * perp[2] + fj * cross_vec[2]) * 0.1f;

      hit = FUN_0014df70(0x1000e9, ray_origin, ray_dir,
                         unit_handle, (int16_t *)collision_result);

      if (hit != 0) {
        if (*(short *)collision_result == 2) {
          if (best_object == -1) {
            hit_material = *(short *)(collision_result + 0x34);
            if ((*(unsigned int *)(collision_result + 0x4c) & 8) != 0) {
              material_idx =
                  (int16_t)(unsigned char)
                  (*(unsigned int *)(collision_result + 0x4c) >> 8);
              material_surface =
                  *(unsigned int *)(collision_result + 0x44);
            }
          }
        } else if (*(short *)collision_result == 3) {
          hit_target = *(int *)(collision_result + 0x38);
          obj_data = (char *)object_get_and_verify_type(hit_target, -1);

          if (*(short *)(obj_data + 100) != 2) {
            parent_handle = *(int *)(obj_data + 0xcc);
            if (parent_handle != -1) {
              hit_target = parent_handle;
              obj_data = (char *)object_get_and_verify_type(
                  parent_handle, -1);
            }
          }

          if (best_object == -1 ||
              (*(short *)(obj_data + 100) == 0 &&
               ((best_type == 0 &&
                 *(float *)(collision_result + 0x14) <
                     best_fraction) ||
                best_type != 0))) {
            best_type = *(short *)(obj_data + 100);
            hit_material = *(short *)(collision_result + 0x34);
            best_fraction = *(float *)(collision_result + 0x14);
            best_object = hit_target;
          }
        }
      }
      inner_i = inner_i + 1;
      inner_count = inner_count - 1;
    } while (inner_count != 0);
    outer_i = outer_i + 1;
    outer_count = outer_count - 1;
  } while (outer_count != 0);

  if (global_current_collision_user_depth < 2) {
    display_assert("global_current_collision_user_depth > 1",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x2256, 1);
    system_exit(-1);
  }
  global_current_collision_user_depth -= 1;

  {
    char *re_unit;
    re_unit = (char *)object_get_and_verify_type(unit_handle, 3);
    current_weapon_idx = *(short *)(re_unit + 0x2a2);
    re_unit = (char *)object_get_and_verify_type(unit_handle, 3);

    melee_damage_effect = -1;
    if (current_weapon_idx != -1) {
      if (current_weapon_idx < 0 || current_weapon_idx >= 4) {
        display_assert(
            "index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
            "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
        system_exit(-1);
      }
      weapon_handle = *(int *)(re_unit + 0x2a8 +
                               current_weapon_idx * 4);
      if (weapon_handle != -1) {
        weapon_data = (unsigned int *)object_get_and_verify_type(
            weapon_handle, 4);
        weapon_tag_data = (int)tag_get(0x77656170, *weapon_data);
        melee_damage_effect = *(int *)(weapon_tag_data + 0x3a0);
        melee_response_effect = *(int *)(weapon_tag_data + 0x3b0);
        if (melee_damage_effect != -1)
          goto got_damage_effect;
      }
    }
    melee_damage_effect = *(int *)(unit_tag_data + 0x294);
  }

got_damage_effect:
  if (best_object != -1) {
    char *hit_obj_data;
    hit_obj_data = (char *)object_get_and_verify_type(best_object, -1);
    if (*(short *)(hit_obj_data + 0x64) == 1) {
      char *hit_obj_tag;
      hit_obj_tag = (char *)tag_get(0x6f626a65,
                            *(unsigned int *)hit_obj_data);
      melee_scale = *(float *)(hit_obj_tag + 0x20) * 0.035f;
      kick_vec[0] = melee_scale * facing[0];
      kick_vec[1] = melee_scale * facing[1];
      kick_vec[2] = melee_scale * facing[2];
      vehicle_accelerate(best_object, kick_vec);
    }
  }

  if (melee_damage_effect != -1) {
    damage_data_new(damage_data, melee_damage_effect);
    *(unsigned int *)(damage_data + 0x04) |= 1;
    *(short *)(damage_data + 0x10) = *(short *)((char *)unit + 0x68);
    *(unsigned int *)(damage_data + 0x14) = unit[0x12];
    *(unsigned int *)(damage_data + 0x18) = unit[0x13];
    *(unsigned int *)(damage_data + 0x08) = unit[0x72];
    *(unsigned int *)(damage_data + 0x0c) = unit_handle;
    *(unsigned int *)(damage_data + 0x1c) = *(unsigned int *)&ray_origin[0];
    *(unsigned int *)(damage_data + 0x20) = *(unsigned int *)&ray_origin[1];
    *(unsigned int *)(damage_data + 0x24) = *(unsigned int *)&ray_origin[2];
    *(unsigned int *)(damage_data + 0x28) = unit[0x14];
    *(unsigned int *)(damage_data + 0x2c) = unit[0x15];
    *(unsigned int *)(damage_data + 0x30) = unit[0x16];
    *(float *)(damage_data + 0x34) = facing[0];
    *(float *)(damage_data + 0x38) = facing[1];
    *(float *)(damage_data + 0x3c) = facing[2];
    *(short *)(damage_data + 0x4c) = hit_material;

    if (best_object == -1) {
      if ((short)material_idx != -1) {
        FUN_00146a90(material_idx, damage_data,
                     material_surface);
      }
    } else {
      char *hit_type_data;
      hit_type_data = (char *)object_get_and_verify_type(best_object, -1);
      if (*(short *)(hit_type_data + 100) == 7) {
        ((void (*)(int))0x95c10)(best_object);
      }

      globals = (int)game_globals_get();
      globals_element_ptr = (char *) (int)tag_block_get_element(
          (void *)(globals + 0x170), 0, 0xf4);

      dot_product = 0.0f;
      if (0.0f < *(float *)(globals_element_ptr + 0x34)) {
        dot_product =
            (*(float *)&unit[9] * *(float *)&unit[6] +
             *(float *)&unit[10] * *(float *)&unit[7] +
             *(float *)&unit[0xb] * *(float *)&unit[8]) * 30.0f;
        dot_product = dot_product /
                      *(float *)(globals_element_ptr + 0x34);
        if (dot_product < 0.0f) {
          dot_product = 0.0f;
        } else if (dot_product > 1.0f) {
          dot_product = 1.0f;
        }
      }

      if (*(short *)((char *)unit + 0x64) == 0) {
        char *biped_check;
        biped_check = (char *)object_get_and_verify_type(unit_handle, 1);
        if (*(char *)(biped_check + 0x459) > 0x0f) {
          dot_product = 1.5f;
        }
      }

      *(float *)(damage_data + 0x40) = dot_product;

      hit_type_data = (char *)object_get_and_verify_type(best_object, -1);
      if (*(short *)(hit_type_data + 100) == 0) {
        object_cause_damage(damage_data, best_object,
                            -1, -1, -1, 0);
      }
    }
  }

  if ((short)hit_material != -1) {
    FUN_001abd10((short)hit_material, unit_handle, melee_damage_effect);
    if (melee_response_effect != -1) {
      damage_data_new(damage_data, melee_response_effect);
      *(float *)(damage_data + 0x34) = -facing[0];
      *(unsigned int *)(damage_data + 0x28) = unit[0x14];
      *(float *)(damage_data + 0x38) = -facing[1];
      *(unsigned int *)(damage_data + 0x2c) = unit[0x15];
      *(unsigned int *)(damage_data + 0x30) = unit[0x16];
      *(float *)(damage_data + 0x3c) = -facing[2];
      *(unsigned int *)(damage_data + 0x1c) = unit[0x14];
      *(unsigned int *)(damage_data + 0x20) = unit[0x15];
      *(unsigned int *)(damage_data + 0x24) = unit[0x16];
      *(unsigned int *)(damage_data + 0x04) |= 8;
      object_cause_damage(damage_data, unit_handle,
                          -1, -1, -1, 0);
    }
  }

  *(char *)((int)unit + 0x239) = 0;
}

/* FUN_001afd30 (0x1afd30) — unit_preprocess_nodes
 *
 * Applies animation overlay processing to unit nodes.
 * Confirmed: cdecl, 2 stack params (unit_handle, node_output).
 */
void FUN_001afd30(int unit_handle, int node_output)
{
  int unit;
  int unit_tag_data;
  int anim_tag_data;
  int seat_block;
  short anim_index;
  int anim_entry;
  char seat_anim_idx;
  float *aiming_vec;
  char valid;
  /* local_matrix is a real_matrix4x3 (13 floats):
   *   [0]    scale
   *   [1..3] forward (object_get_orientation out_forward)
   *   [4..6] left    (cross product of forward x up)
   *   [7..9] up      (object_get_orientation out_up)
   *  [10..12] position (from global 0x31fc1c)
   * MSVC lays these contiguous on the stack; separate locals in clang
   * would break the matrix passed to real_matrix4x3_transform_point. */
  float local_matrix[13];
  float rel_aim_vec[3];
  float rel_look_vec[3];
  float aim_angles[2];
  float look_angles[2];
  float *steering_ptr;
  float max_frame;
  float temp;

  unit = (int)object_get_and_verify_type(unit_handle, 3);
  unit_tag_data = (int)tag_get(0x756e6974, *(unsigned int *)unit);
  anim_tag_data = (int)tag_get(0x616e7472,
                          *(unsigned int *)(unit_tag_data + 0x44));

  if (*(short *)(unit + 0x25a) != -1) {
    int repl_anim;
    repl_anim = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0x74),
        (int)*(short *)(unit + 0x25a), 0xb4);
    ((void (*)(int, short, int))0x122060)(repl_anim,
        *(short *)(unit + 0x25c), node_output);
  }

  if (*(short *)(unit + 0x25e) != -1) {
    int ovl_anim;
    ovl_anim = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0x74),
        (int)*(short *)(unit + 0x25e), 0xb4);
    ((void (*)(int, short, int))0x122240)(ovl_anim,
        *(short *)(unit + 0x260), node_output);
  }

  if (*(short *)(unit + 0x262) != -1) {
    int ovl_anim2;
    ovl_anim2 = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0x74),
        (int)*(short *)(unit + 0x262), 0xb4);
    ((void (*)(int, short, int))0x122240)(ovl_anim2,
        *(short *)(unit + 0x264), node_output);
  }

  unit_control_trace(unit_handle, "unit-preprocess-nodes");
  *(char *)(unit + 0x266) = 0;
  *(char *)(unit + 0x267) = 0;

  if ((*(unsigned int *)(unit_tag_data + 0x17c) & 0x800) == 0 &&
      *(char *)(unit + 0x250) != -1) {
    seat_block = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0xc),
        (int)*(char *)(unit + 0x250), 100);

    if (*(char *)(unit + 600) != -1) {
      if (*(int *)(seat_block + 0x40) < 0xc) {
        anim_index = -1;
      } else {
        anim_index = *(short *)(*(int *)(seat_block + 0x44) + 0x16);
      }
      if (*(short *)(unit + 0x1ce) != -1) {
        anim_index = *(short *)(unit + 0x1ce);
      }
      if (anim_index != -1) {
        anim_entry = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0x74), (int)anim_index, 0xb4);
        seat_anim_idx = *(char *)(unit + 600);
        if (seat_anim_idx >= 0 &&
            (short)seat_anim_idx < *(short *)(anim_entry + 0x22)) {
          ((void (*)(int, short, int))0x122240)(anim_entry, (short)seat_anim_idx,
                                  node_output);
        }
      }
    }

    if (0.0f < *(float *)(unit + 0x298) &&
        *(int *)(seat_block + 0x40) > 10) {
      anim_index = *(short *)(*(int *)(seat_block + 0x44) + 0x14);
      if (anim_index != -1) {
        int throttle_anim;
        throttle_anim = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0x74), (int)anim_index, 0xb4);
        ((void (*)(int, int, unsigned int, int))0x122450)(throttle_anim, 0,
            *(unsigned int *)(unit + 0x298), node_output);
      }
    }

    if ((*(unsigned char *)(unit + 0x248) & 2) != 0) {
      int steer_idx;
      int steer_offset;
      int steering_count;
      steering_count = 4;
      steer_idx = 2;
      steer_offset = 4;
      steering_ptr = (float *)(unit + 0x314);
      do {
        if (steer_idx >= 0 &&
            steer_idx < *(int *)(seat_block + 0x40)) {
          anim_index = *(short *)(steer_offset +
                                  *(int *)(seat_block + 0x44));
          if (anim_index != -1) {
            anim_entry = (int)tag_block_get_element(
        (void *)(anim_tag_data + 0x74), (int)anim_index, 0xb4);
            max_frame =
                (float)(*(short *)(anim_entry + 0x22) - 1);
            ((void (*)(int, float, int))0x122690)(anim_entry,
                         max_frame * *steering_ptr,
                         node_output);
          }
        }
        steer_offset = steer_offset + 2;
        steering_ptr = steering_ptr + 1;
        steer_idx = steer_idx + 1;
        steering_count = steering_count - 1;
      } while (steering_count != 0);
    }

    if ((*(unsigned int *)(unit_tag_data + 0x17c) & 0x400) == 0) {
      char anim_state_byte;
      anim_state_byte = *(char *)(unit + 0x253);
      if ((anim_state_byte < 0x17 ||
           (anim_state_byte > 0x23 && anim_state_byte != 0x29)) &&
          *(char *)(unit + 0x254) == 0) {
        int unit_anim_block;
        unit_anim_block = (int)tag_block_get_element(
            (void *)(seat_block + 0x58),
            (int)*(char *)(unit + 0x251), 0xbc);

        aim_angles[0] = *(float *)*(int *)0x31fc54;
        aim_angles[1] = *(float *)(*(int *)0x31fc54 + 4);

        if (*(short *)(unit + 0x24a) != -1) {
          aiming_vec = (float *)(unit + 0x1ec);
          valid = valid_real_normal3d(aiming_vec);
          if (valid == 0) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real_normal3d(%f, %f, %f)",
                "&unit->unit.aiming_vector",
                (double)*aiming_vec,
                (double)*(float *)(unit + 0x1f0),
                (double)*(float *)(unit + 500));
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x670, 1);
            system_exit(-1);
          }

          local_matrix[0] = 1.0f;
          object_get_orientation(unit_handle, &local_matrix[1], &local_matrix[7]);
          /* left = up x forward (verified vs original 0x1afd30: cross_product3d
             pushes a=up[ebp-0x48], b=forward[ebp-0x60]).  A prior lift passed
             (forward, up) which negates the left basis vector, flipping the
             aim/look matrix -> wrong relative aim angles -> biped aims up/down. */
          cross_product3d(&local_matrix[7], &local_matrix[1], &local_matrix[4]);
          local_matrix[10] = *(float *)*(int *)0x31fc1c;
          local_matrix[11] = *(float *)(*(int *)0x31fc1c + 4);
          local_matrix[12] = *(float *)(*(int *)0x31fc1c + 8);
          real_matrix4x3_transform_point(
              local_matrix, aiming_vec,
              rel_aim_vec);

          valid = real_vector3d_valid(rel_aim_vec);
          if (valid == 0) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real_vector2d(%f, %f, %f)",
                "&relative_aiming_vector",
                (double)rel_aim_vec[0],
                (double)rel_aim_vec[1],
                (double)rel_aim_vec[2]);
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x67d, 1);
            system_exit(-1);
          }

          vector_to_angles(aim_angles, rel_aim_vec);

          temp = aim_angles[1];
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real(0x%08X %f)",
                "relative_aiming_angles.pitch",
                *(unsigned int *)&temp, (double)temp);
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x680, 1);
            system_exit(-1);
          }
          temp = aim_angles[0];
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real(0x%08X %f)",
                "relative_aiming_angles.yaw",
                *(unsigned int *)&temp, (double)temp);
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x681, 1);
            system_exit(-1);
          }

          *(char *)(unit + 0x266) = 1;
          *(float *)(unit + 0x268) =
              -((float)(int)*(short *)(unit_anim_block + 0x68) *
                *(float *)(unit_anim_block + 0x60));
          *(float *)(unit + 0x26c) =
              (float)(int)*(short *)(unit_anim_block + 0x6a) *
              *(float *)(unit_anim_block + 0x64);
          *(float *)(unit + 0x270) =
              -((float)(int)*(short *)(unit_anim_block + 0x74) *
                *(float *)(unit_anim_block + 0x6c));
          *(float *)(unit + 0x274) =
              (float)(int)*(short *)(unit_anim_block + 0x76) *
              *(float *)(unit_anim_block + 0x70);

          {
            int aim_overlay;
            aim_overlay = (int)tag_block_get_element(
                (void *)(anim_tag_data + 0x74),
                (int)*(short *)(unit + 0x24a), 0xb4);
            FUN_00122e50(aim_overlay,
                (float *)(unit_anim_block + 0x60),
                aim_angles[0], aim_angles[1], node_output);
          }
        }

        if ((*(short *)(unit + 0x2a2) != -1 ||
             *(int *)(unit + 0x1c8) != -1) &&
            *(short *)(unit + 0x24c) != -1) {
          float *look_ptr;
          look_ptr = (float *)(seat_block + 0x20);
          local_matrix[0] = 1.0f;
          object_get_orientation(unit_handle, &local_matrix[1], &local_matrix[7]);
          /* left = up x forward (verified vs original 0x1afd30: cross_product3d
             pushes a=up[ebp-0x48], b=forward[ebp-0x60]).  A prior lift passed
             (forward, up) which negates the left basis vector, flipping the
             aim/look matrix -> wrong relative aim angles -> biped aims up/down. */
          cross_product3d(&local_matrix[7], &local_matrix[1], &local_matrix[4]);
          local_matrix[10] = *(float *)*(int *)0x31fc1c;
          local_matrix[11] = *(float *)(*(int *)0x31fc1c + 4);
          local_matrix[12] = *(float *)(*(int *)0x31fc1c + 8);
          real_matrix4x3_transform_point(
              local_matrix, (void *)(unit + 0x210),
              rel_look_vec);

          valid = real_vector3d_valid(rel_look_vec);
          if (valid == 0) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real_vector2d(%f, %f, %f)",
                "&relative_looking_vector",
                (double)rel_look_vec[0],
                (double)rel_look_vec[1],
                (double)rel_look_vec[2]);
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x6a7, 1);
            system_exit(-1);
          }

          vector_to_angles(look_angles, rel_look_vec);
          look_angles[0] = look_angles[0] - aim_angles[0];
          look_angles[1] = look_angles[1] - aim_angles[1];

          temp = look_angles[1];
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real(0x%08X %f)",
                "relative_looking_angles.pitch",
                *(unsigned int *)&temp, (double)temp);
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x6ae, 1);
            system_exit(-1);
          }
          temp = look_angles[0];
          if ((*(unsigned int *)&temp & 0x7f800000) == 0x7f800000) {
            csprintf((char *)0x5ab100,
                "%s: assert_valid_real(0x%08X %f)",
                "relative_looking_angles.yaw",
                *(unsigned int *)&temp, (double)temp);
            display_assert(0,
                "c:\\halo\\SOURCE\\units\\units.c", 0x6af, 1);
            system_exit(-1);
          }

          *(char *)(unit + 0x267) = 1;
          *(float *)(unit + 0x278) =
              -((float)(int)*(short *)(seat_block + 0x28) *
                *look_ptr);
          *(float *)(unit + 0x27c) =
              (float)(int)*(short *)(seat_block + 0x2a) *
              *(float *)(seat_block + 0x24);
          *(float *)(unit + 0x280) =
              -((float)(int)*(short *)(seat_block + 0x34) *
                *(float *)(seat_block + 0x2c));
          *(float *)(unit + 0x284) =
              (float)(int)*(short *)(seat_block + 0x36) *
              *(float *)(seat_block + 0x30);

          {
            int look_overlay;
            look_overlay = (int)tag_block_get_element(
                (void *)(anim_tag_data + 0x74),
                (int)*(short *)(unit + 0x24c), 0xb4);
            FUN_00122e50(look_overlay,
                look_ptr, look_angles[0], look_angles[1],
                node_output);
          }
        }
      }
    }
  }
}

/* FUN_001b0630 (0x1b0630) — euler aiming vector update
 *
 * Updates aiming/desired vectors with angular velocity constraints,
 * motion planning, and bounds clamping.
 * Confirmed: cdecl, 7 stack params.
 */
void FUN_001b0630(int transform_matrix, float *aiming_vector,
                  float *desired_vector, float *angular_velocity,
                  float *aiming_bounds, float angular_velocity_limit,
                  float angular_acceleration_limit)
{
  float local_aim[3];
  float local_desired[3];
  float desired_clamped[3];
  float end_aim[3];
  float aim_angles[2];
  float desired_angles[2];
  float end_angles[2];
  float vel_yaw;
  float vel_pitch;
  float vel_vec[3];
  float rotated[3];
  float rot_angles[2];
  char plan_yaw[32];
  char plan_pitch[32];
  float cos_v;
  float sin_v;
  char wrapping;
  char yaw_ok;
  float norm_len;
  float delta_yaw;
  float delta_pitch;
  char pitch_ok;

  if (angular_velocity_limit < 0.0f) {
    display_assert("angular_velocity_limit >= 0.0f",
                   "c:\\halo\\SOURCE\\units\\units.c", 0x962, 1);
    system_exit(-1);
  }
  if (angular_acceleration_limit <= 0.0f ||
      angular_acceleration_limit >= 10000.0f) {
    display_assert(
        "(angular_acceleration_limit > 0.0f) && "
        "(angular_acceleration_limit < 10000.0f)",
        "c:\\halo\\SOURCE\\units\\units.c", 0x963, 1);
    system_exit(-1);
  }

  if (transform_matrix == 0) {
    local_aim[0] = aiming_vector[0];
    local_aim[1] = aiming_vector[1];
    local_aim[2] = aiming_vector[2];
    local_desired[0] = desired_vector[0];
    local_desired[1] = desired_vector[1];
    local_desired[2] = desired_vector[2];
  } else {
    real_matrix4x3_transform_point((void *)transform_matrix, aiming_vector,
                                   local_aim);
    real_matrix4x3_transform_point((void *)transform_matrix, desired_vector,
                                   local_desired);
  }

  wrapping = (aiming_bounds[1] - aiming_bounds[0]) - 6.2831853f >
             -1.0e-4f;

  vector_to_angles(aim_angles, local_aim);
  vector_to_angles(desired_angles, local_desired);

  yaw_ok = 1;
  if (wrapping == 0) {
    if (desired_angles[0] < aiming_bounds[0]) {
      desired_angles[0] = aiming_bounds[0];
      yaw_ok = 0;
    } else if (desired_angles[0] > aiming_bounds[1]) {
      desired_angles[0] = aiming_bounds[1];
      yaw_ok = 0;
    }
  } else {
    if (desired_angles[0] >= aiming_bounds[0]) {
      if (desired_angles[0] > aiming_bounds[1]) {
        desired_angles[0] = desired_angles[0] - 6.2831853f;
        if (desired_angles[0] < aiming_bounds[0]) {
          display_assert(
              "desired_aiming_angles.yaw >= aiming_bounds->x0",
              "c:\\halo\\SOURCE\\units\\units.c", 0x986, 1);
          system_exit(-1);
        }
      }
    } else {
      desired_angles[0] = desired_angles[0] + 6.2831853f;
      if (desired_angles[0] <= aiming_bounds[1]) {
        display_assert(
            "desired_aiming_angles.yaw <= aiming_bounds->x1",
            "c:\\halo\\SOURCE\\units\\units.c", 0x981, 1);
        system_exit(-1);
      }
    }
  }

  if (desired_angles[1] < aiming_bounds[2]) {
    desired_angles[1] = aiming_bounds[2];
    goto clamp_and_reconstruct;
  } else if (desired_angles[1] > aiming_bounds[3]) {
    desired_angles[1] = aiming_bounds[3];
    goto clamp_and_reconstruct;
  } else if (yaw_ok) {
    desired_clamped[0] = desired_vector[0];
    desired_clamped[1] = desired_vector[1];
    desired_clamped[2] = desired_vector[2];
    goto done_clamp;
  }

clamp_and_reconstruct:
  angles_to_vector(desired_clamped, desired_angles);
  if (transform_matrix != 0) {
    matrix_transform_vector((float *)transform_matrix, desired_clamped,
                            desired_clamped);
    normalize3d(desired_clamped);
  }

done_clamp:
  vel_vec[0] = angular_velocity[0];
  vel_vec[1] = angular_velocity[1];
  vel_vec[2] = angular_velocity[2];
  norm_len = normalize3d(vel_vec);
  if (norm_len == 0.0f) {
    vel_yaw = 0.0f;
    vel_pitch = 0.0f;
  } else {
    cos_v = x87_fcos(norm_len);
    rotated[0] = local_aim[0];
    rotated[1] = local_aim[1];
    rotated[2] = local_aim[2];
    sin_v = x87_fsin(norm_len);
    rotate_vector3d_by_sincos(rotated, vel_vec, sin_v, cos_v);
    vector_to_angles(rot_angles, rotated);
    vel_yaw = rot_angles[0] - aim_angles[0];
    vel_pitch = rot_angles[1] - aim_angles[1];
  }

  delta_yaw = aim_angles[0] - desired_angles[0];
  delta_pitch = aim_angles[1] - desired_angles[1];

  if (wrapping != 0) {
    if (delta_yaw > 3.1415927f) {
      delta_yaw = delta_yaw - 6.2831853f;
    } else if (delta_yaw < -3.1415927f) {
      delta_yaw = delta_yaw + 6.2831853f;
    }
  }

  FUN_001ac680(delta_yaw, vel_yaw, angular_velocity_limit,
               angular_acceleration_limit, (int)plan_yaw);
  FUN_001ac680(delta_pitch, vel_pitch, angular_velocity_limit,
               angular_acceleration_limit, (int)plan_pitch);
  unit_adjust_plan_overlap(plan_yaw, plan_pitch,
                           angular_velocity_limit,
                           angular_acceleration_limit);

  pitch_ok = FUN_001a8550(plan_yaw, 1.0f, delta_yaw, &delta_yaw,
                          vel_yaw, &rot_angles[0]);
  yaw_ok = FUN_001a8550(plan_pitch, 1.0f, delta_pitch, &delta_pitch,
                        vel_pitch, &rot_angles[1]);

  if (pitch_ok == 0 || yaw_ok == 0) {
    end_angles[0] = delta_yaw + desired_angles[0];
    end_angles[1] = delta_pitch + desired_angles[1];

    if (wrapping == 0) {
      if (end_angles[0] < aiming_bounds[0]) {
        end_angles[0] = aiming_bounds[0];
      } else if (end_angles[0] > aiming_bounds[1]) {
        end_angles[0] = aiming_bounds[1];
      }
    } else {
      if (end_angles[0] >= aiming_bounds[0]) {
        if (end_angles[0] > aiming_bounds[1]) {
          end_angles[0] = end_angles[0] - 6.2831853f;
          if (end_angles[0] < aiming_bounds[0]) {
            display_assert(
                "end_aiming_angles.yaw >= aiming_bounds->x0",
                "c:\\halo\\SOURCE\\units\\units.c", 0xaaa, 1);
            system_exit(-1);
          }
        }
      } else {
        end_angles[0] = end_angles[0] + 6.2831853f;
        if (end_angles[0] <= aiming_bounds[1]) {
          display_assert(
              "end_aiming_angles.yaw <= aiming_bounds->x1",
              "c:\\halo\\SOURCE\\units\\units.c", 0xaa5, 1);
          system_exit(-1);
        }
      }
    }

    if (end_angles[1] < aiming_bounds[2]) {
      end_angles[1] = aiming_bounds[2];
    } else if (end_angles[1] > aiming_bounds[3]) {
      end_angles[1] = aiming_bounds[3];
    }

    if (end_angles[0] < aiming_bounds[0] ||
        !(end_angles[0] <= aiming_bounds[1])) {
      display_assert(
          "(end_aiming_angles.yaw >= aiming_bounds->x0) && "
          "(end_aiming_angles.yaw <= aiming_bounds->x1)",
          "c:\\halo\\SOURCE\\units\\units.c", 0xab3, 1);
      system_exit(-1);
    }
    if (end_angles[1] < aiming_bounds[2] ||
        !(end_angles[1] <= aiming_bounds[3])) {
      display_assert(
          "(end_aiming_angles.pitch >= aiming_bounds->y0) && "
          "(end_aiming_angles.pitch <= aiming_bounds->y1)",
          "c:\\halo\\SOURCE\\units\\units.c", 0xab4, 1);
      system_exit(-1);
    }

    angles_to_vector(end_aim, end_angles);

    {
      float total_yaw;
      float total_pitch;
      float vel_end[3];
      total_yaw = rot_angles[0] + end_angles[0];
      total_pitch = rot_angles[1] + end_angles[1];
      (void)total_pitch;
      angles_to_vector(vel_end, &total_yaw);

      angular_velocity[0] =
          rot_angles[1] * end_aim[1] -
          vel_end[2] * rot_angles[0];
      angular_velocity[1] =
          vel_end[2] * end_aim[2] -
          rot_angles[1] * end_aim[0];
      angular_velocity[2] =
          rot_angles[0] * end_aim[0] -
          end_aim[1] * vel_end[2];
    }
    normalize3d(angular_velocity);

    {
      float speed;
      speed = ((float (*)(void))0x1d94f0)();
      if (speed > angular_velocity_limit) {
        speed = angular_velocity_limit;
      }
      angular_velocity[0] = speed * angular_velocity[0];
      angular_velocity[1] = speed * angular_velocity[1];
      angular_velocity[2] = speed * angular_velocity[2];
    }

    if (transform_matrix != 0) {
      matrix_transform_vector((float *)transform_matrix, end_aim,
                              aiming_vector);
      normalize3d(aiming_vector);
      goto final_validate;
    }
    aiming_vector[0] = end_aim[0];
    aiming_vector[1] = end_aim[1];
  } else {
    aiming_vector[0] = desired_clamped[0];
    aiming_vector[1] = desired_clamped[1];
    aiming_vector[2] = desired_clamped[2];
    {
      float *zero_vec;
      zero_vec = *(float **)0x31fc38;
      angular_velocity[0] = zero_vec[0];
      angular_velocity[1] = zero_vec[1];
      end_aim[2] = zero_vec[2];
    }
  }
  angular_velocity[2] = end_aim[2];

final_validate:
  {
    char aim_valid;
    aim_valid = valid_real_normal3d(aiming_vector);
    if (aim_valid == 0) {
      csprintf((char *)0x5ab100,
          "%s: assert_valid_real_normal3d(%f, %f, %f)",
          "aiming_vector",
          (double)aiming_vector[0],
          (double)aiming_vector[1],
          (double)aiming_vector[2]);
      display_assert(0, "c:\\halo\\SOURCE\\units\\units.c",
                     0xae9, 1);
      system_exit(-1);
    }
  }
}

/* FUN_001b0d90 (0x1b0d90) — animation state update
 *
 * Evaluates the current unit animation state and applies transitions.
 * Confirmed: cdecl, 2 stack params.
 * Returns: int16_t (animation flags bitmask).
 */
short FUN_001b0d90(int unit_handle, char *anim_state)
{
  unsigned int *unit;
  int unit_tag_data;
  short desired_state;
  short base_seat;
  short anim_status;
  unsigned short result;
  char apply_flag;
  short global_seat;
  char unit_anim_byte;
  unsigned int *vehicle_unit;
  int vehicle_tag;
  unsigned char *seat_element;
  int mode_tag;
  int anim_graph;
  int anim_element;
  float delta[3];
  char out_matrix[52];
  void *world_matrix;
  int biped_data;
  int biped_tag;

  unit = (unsigned int *)object_get_and_verify_type(unit_handle, 3);
  unit_tag_data = (int)tag_get(0x756e6974, *unit);
  desired_state = (short)*anim_state;
  result = 0;
  apply_flag = 0;

  if (desired_state < 0 || desired_state >= 0x2c) {
    display_assert(
        "desired_state>=0 && desired_state<NUMBER_OF_UNIT_STATES",
        "c:\\halo\\SOURCE\\units\\units.c", 0xb61, 1);
    system_exit(-1);
  }

  base_seat = -1;
  if (unit[0x33] == (unsigned int)-1 &&
      (*(unsigned char *)((int)unit + 0xb6) & 4) == 0) {
    switch (*(unsigned char *)((int)unit + 0x256)) {
    case 0:
      base_seat = 0;
      break;
    case 1:
    case 2:
      base_seat = 1;
      break;
    case 3:
      base_seat = (anim_state[1] != 0) + 2;
      if (base_seat == -1) goto seat_assert;
      break;
    case 4:
      base_seat = 2;
      break;
    case 5:
      base_seat = 4;
      break;
    case 6:
      base_seat = 5;
      break;
    default:
    seat_assert:
      display_assert("desired_base_seat_index!=NONE",
                     "c:\\halo\\SOURCE\\units\\units.c", 0xb73, 1);
      system_exit(-1);
      break;
    }

    if (unit[0x72] != (unsigned int)-1) {
      global_seat = *(short *)0x32de80;
      if (global_seat != -1) {
        if (global_seat < 0) {
          base_seat = 0;
        } else {
          base_seat = 6;
          if (global_seat < 7) {
            base_seat = global_seat;
          }
        }
      }
    }

    if (*(char *)((int)unit + 0x1bf) != -1) {
      base_seat = (short)*(char *)((int)unit + 0x1bf);
    }
    if ((unit[0x6e] & 0x200) != 0) {
      base_seat = 1;
    }
    if (*(char *)((int)unit + 0x23b) != 0) {
      base_seat = 5;
    }

    if (*(char *)((int)unit + 599) != (char)base_seat) {
      char can_change;
      can_change = FUN_001a86b0((void *)((int)unit + 0x248),
                                desired_state);
      if (can_change != 0) {
        char *weapon_name;
        const char *seat_label;
        weapon_name = unit_get_weapon_name(unit_handle, 1);
        seat_label = FUN_001ab6e0(base_seat);
        FUN_001acd70(unit_handle, seat_label, weapon_name, 1);
      }
    }
  }

  if (*(short *)((int)unit + 0x262) != -1) {
    anim_status = FUN_001ab870(
        (void *)((int)unit + 0x262),
        *(int *)(unit_tag_data + 0x44), unit_handle);
    if (anim_status == 2) {
      *(short *)((int)unit + 0x262) = -1;
    }
  }

  if (*(short *)(unit + 0x20) != -1) {
    anim_status = FUN_001ab870(
        (void *)((int)unit + 0x80),
        (int)unit[0x1f], unit_handle);
    if (anim_status == 1) {
      unit_anim_byte = *(char *)((int)unit + 0x253);
      if (unit_anim_byte >= 0x1e && unit_anim_byte <= 0x29) {
        switch (unit_anim_byte) {
        case 0x1e:
        case 0x1f:
        case 0x29:
          unit_cause_melee_damage(unit_handle, 0, -1, -1, -1, -1, 0);
          break;
        case 0x21:
          FUN_001ab110(unit_handle, 0);
          break;
        default:
          break;
        }
      }
    } else if (anim_status == 2) {
      unit_anim_byte = *(char *)((int)unit + 0x253);
      switch (unit_anim_byte) {
      case 0x19:
        if ((*(unsigned char *)(unit_tag_data + 0x17c) & 2) == 0) {
          goto start_limp;
        }
        if ((*(unsigned char *)((int)unit + 4) & 0x20) != 0) {
          goto destroy_unit;
        }
        if (*(short *)(unit + 0x19) != 0) {
          goto set_garbage_flag;
        }
        biped_data = (int)object_get_and_verify_type(unit_handle, 1);
        biped_tag = (int)tag_get(0x62697064,
                            *(unsigned int *)biped_data);
        if ((*(unsigned char *)(biped_data + 0x424) & 1) != 0 &&
            (*(unsigned int *)(biped_tag + 0x2f4) & 0x400) == 0) {
          goto start_limp;
        }
      destroy_unit:
        unit_destroy(unit_handle);
        break;
      start_limp:
        if (*(short *)(unit + 0x19) == 0) {
          biped_start_limp_body_physics(unit_handle);
        }
      set_garbage_flag:
        *(unsigned char *)((int)unit + 0x248) =
            *(unsigned char *)((int)unit + 0x248) | 4;
        *(short *)((int)unit + 0x82) =
            *(short *)((int)unit + 0x82) - 1;
        break;

      case 0x1a:
        vehicle_unit = (unsigned int *)object_get_and_verify_type(
            unit[0x33], 3);
        vehicle_tag = (int)tag_get(0x756e6974, *vehicle_unit);
        seat_element = (unsigned char *)tag_block_get_element(
            (void *)(vehicle_tag + 0x2e4),
            (int)*(short *)(unit + 0xa8), 0x11c);
        object_set_garbage(unit_handle, (~*seat_element) & 1);
        if (vehicle_unit[0xb5] == (unsigned int)unit_handle) {
          unit_close((int)unit[0x33]);
        }
        break;

      case 0x1b:
        mode_tag = (int)tag_get(0x6d6f6465,
                           *(unsigned int *)(unit_tag_data + 0x34));
        anim_graph = (int)tag_get(0x616e7472, unit[0x1f]);
        anim_element = (int)tag_block_get_element(
            (void *)(anim_graph + 0x74),
            (int)*(short *)(unit + 0x20), 0xb4);
        FUN_001234b0((void *)mode_tag, (void *)anim_element,
                     *(unsigned short *)((int)unit + 0x82), delta);
        world_matrix = (void *)object_get_world_matrix(unit_handle,
                                               out_matrix);
        matrix_scale_transform_vector((float *)world_matrix,
                                      delta, delta);
        unit_exit_seat_end(unit_handle);
        vector3d_add((float *)(unit + 6), delta, (float *)(unit + 6));
        break;

      case 0x25:
      case 0x26:
        *(short *)((int)unit + 0x82) =
            *(short *)((int)unit + 0x82) - 1;
        break;

      case 0x27:
        result = 1;
        desired_state = 0x28;
        break;

      default:
        break;
      }

      {
        char anim_ok;
        anim_ok = FUN_001a8790((void *)((int)unit + 0x248));
        if (anim_ok == 0) {
          apply_flag = 1;
        }
      }
    }
  }

  if (*(short *)((int)unit + 0x25a) != -1) {
    anim_status = FUN_001ab870(
        (void *)((int)unit + 0x25a),
        *(int *)(unit_tag_data + 0x44), unit_handle);
    if (anim_status == 2) {
      int refreshed_unit;
      object_set_region_count(unit_handle, 6);
      refreshed_unit = (int)object_get_and_verify_type(unit_handle, 3);
      *(char *)(refreshed_unit + 0x254) = 0;
      *(short *)(refreshed_unit + 0x25a) = -1;
    }
  }

  if (*(short *)((int)unit + 0x25e) != -1) {
    anim_status = FUN_001ab870(
        (void *)((int)unit + 0x25e),
        *(int *)(unit_tag_data + 0x44), unit_handle);
    if (anim_status == 2 || anim_status == 4) {
      unit_anim_byte = *(char *)((int)unit + 0x253);
      if (unit_anim_byte < 3 || unit_anim_byte > 4) {
        *(char *)((int)unit + 0x255) = 0;
        *(short *)((int)unit + 0x25e) = -1;
      }
    }
  }

  if (apply_flag == 0) {
    if ((short)desired_state ==
        (short)*(char *)((int)unit + 0x253)) {
      goto done;
    }
    {
      char can_apply;
      can_apply = FUN_001a86b0((void *)((int)unit + 0x248),
                               desired_state);
      if (can_apply == 0) goto done;
    }
  }
  FUN_001ad260(unit_handle, desired_state);

done:
  return (short)(result & 0xffff);
}

/* FUN_001b1400 (0x1b1400) — animation impulse
 *
 * Selects movement or attack animation based on unit state.
 * Confirmed: cdecl, 9 stack params.
 */
void FUN_001b1400(int unit_handle, char is_melee, char is_throw,
                  char is_airborne, char is_ground, char is_ping,
                  float throttle_magnitude, int weapon_class,
                  int alignment_vector)
{
  unsigned int *unit;
  int unit_tag;
  int anim_graph;
  char moving;
  char fast_moving;
  short throttle_dir;
  char engine_running;
  short anim_idx;
  short chosen_anim;
  int16_t anim_entry_idx;
  char anim_state;
  char movement_type;
  char must_apply;
  int biped_data;
  int biped_tag;
  int anim_element;
  short frame_count;
  short random_frame;
  float align_out[2];

  unit = (unsigned int *)object_get_and_verify_type(unit_handle, 3);
  unit_tag = (int)tag_get(0x756e6974, *unit);

  if (is_melee != 0) {
    is_throw = 0;
    moving = 1;
    if (*(float *)(unit_tag + 0x228) <= 0.0f ||
        (float)unit[0x27] <= *(float *)(unit_tag + 0x228)) {
      fast_moving = 0;
    } else {
      fast_moving = 1;
      goto check_ping;
    }
  } else if (is_throw != 0) {
    is_melee = 1;
    moving = 1;
  } else {
    if ((float)unit[0x27] >= *(float *)(unit_tag + 0x218) ||
        (float)unit[0x26] >= *(float *)(unit_tag + 0x218)) {
      moving = 1;
    } else {
      moving = 0;
    }
    fast_moving = (float)unit[0x27] >= *(float *)(unit_tag + 0x220);

    if (is_ground == 0 && *(char *)(unit + 0x6d) >= 0) {
      goto check_ping;
    }
  }
  fast_moving = 0;

check_ping:
  if (is_ping != 0) {
    fast_moving = 1;
    moving = 1;
  }

  if ((short)weapon_class == -1) {
    weapon_class = 0;
  }

  {
    float abs_throttle;
    abs_throttle = throttle_magnitude;
    if (abs_throttle < 0.0f) abs_throttle = -abs_throttle;

    if ((double)abs_throttle < *(double *)0x25b3f0) {
      throttle_dir = 3;
    } else if ((double)abs_throttle > *(double *)0x2b7a98) {
      throttle_dir = 0;
    } else {
      throttle_dir = 1;
      if (throttle_magnitude <= 0.0f) {
        throttle_dir = 2;
      }
    }
  }

  engine_running = game_engine_running();
  if (engine_running != 0 && (short)weapon_class == 2 &&
      fast_moving && is_melee != 0) {
    throttle_dir = 1;
  }

  if (!moving && is_melee == 0) {
    return;
  }

  unit_tag = (int)tag_get(0x756e6974, *unit);
  anim_graph = (int)tag_get(0x616e7472, *(unsigned int *)(unit_tag + 0x44));

  if (!fast_moving && is_melee == 0) {
    if (*(short *)((int)unit + 0x262) != -1 &&
        *(short *)(unit + 0x99) <= *(short *)(unit_tag + 0x2c8)) {
      return;
    }
    anim_idx = ((short (*)(int, short, int))0x120670)(0, throttle_dir,
                                            weapon_class);
    if (anim_idx < 0 || anim_idx >= *(int *)(anim_graph + 0x3c)) {
      anim_entry_idx = -1;
    } else {
      anim_entry_idx =
          *(short *)(*(int *)(anim_graph + 0x40) + anim_idx * 2);
    }
    chosen_anim = (short)model_animation_choose_random(
        1, *(unsigned int *)(unit_tag + 0x44), anim_entry_idx);
    if (chosen_anim == -1) {
      return;
    }
    *(short *)((int)unit + 0x262) = chosen_anim;
    *(short *)(unit + 0x99) = 0;
    return;
  }

  anim_state = (is_melee != 0) * 2 + 0x17;

  if (is_melee == 0) {
    movement_type = 1;
    {
      char can_change;
      can_change = FUN_001a86b0((void *)((int)unit + 0x248),
                                (short)anim_state);
      if (can_change != 0) goto force_apply;
    }
    must_apply = 0;
  } else {
    movement_type = fast_moving + 2;
  force_apply:
    must_apply = 1;
  }

  if (*(char *)((int)unit + 0x253) == 0x17 &&
      *(short *)((int)unit + 0x82) >
          *(short *)(unit_tag + 0x2ca)) {
    must_apply = 1;
  }

  if (is_melee == 0) {
    if ((*(unsigned char *)((int)unit + 0xb6) & 4) != 0) {
      must_apply = 0;
    }
    if (unit[0x33] != (unsigned int)-1) {
      return;
    }
  }

  if (!must_apply) {
    return;
  }

  if (is_melee != 0) {
    char *weapon_name;
    weapon_name = unit_get_weapon_name(unit_handle, 1);
    FUN_001acd70(unit_handle, *(char **)0x32e48c, weapon_name, 0);
  }

  if ((short)anim_state == 0x19 && *(short *)(unit + 0x19) == 0) {
    biped_data = (int)object_get_and_verify_type(unit_handle, 1);
    biped_tag = (int)tag_get(0x62697064, *(unsigned int *)biped_data);
    if ((*(unsigned char *)(biped_data + 0x424) & 1) != 0 &&
        (*(unsigned int *)(biped_tag + 0x2f4) & 0x400) == 0) {
      char transition_ok;
      anim_state = 0x18;
      transition_ok = FUN_001ad260(unit_handle, 0x18);
      if (transition_ok != 0) goto alignment_section;
    }
  }

  anim_idx = ((short (*)(int, short, int))0x120670)((int)movement_type, throttle_dir,
                                          weapon_class);
  if (anim_idx < 0 || anim_idx >= *(int *)(anim_graph + 0x3c)) {
    anim_entry_idx = -1;
  } else {
    anim_entry_idx =
        *(short *)(*(int *)(anim_graph + 0x40) + anim_idx * 2);
  }

  chosen_anim = model_animation_choose_random(
      1, *(unsigned int *)(unit_tag + 0x44), anim_entry_idx);

  if (chosen_anim == -1) {
    if (is_melee != 0) {
      unsigned short flags;
      flags = *(unsigned short *)((int)unit + 0x248);
      flags = (flags & 0xfff7) | 4;
      *(unsigned short *)((int)unit + 0x248) = flags;
      if ((*(unsigned char *)(unit_tag + 0x17c) & 2) != 0) {
        unit_destroy(unit_handle);
      }
    }
  } else {
    if (*(char *)((int)unit + 0x253) == 0x21) {
      FUN_001ab110(unit_handle, 1);
    }
    object_set_region_count(unit_handle, 3);
    *(char *)((int)unit + 0x253) = anim_state;
    unit_set_animation(unit_handle,
                       *(unsigned int *)(unit_tag + 0x44),
                       chosen_anim);
    *(unsigned char *)((int)unit + 0x248) =
        *(unsigned char *)((int)unit + 0x248) | 1;

    if (is_melee != 0) {
      if (is_airborne == 0 && is_throw == 0) {
        short quarter;
        short half;
        anim_element = (int)tag_block_get_element(
            (void *)(anim_graph + 0x74), (int)chosen_anim, 0xb4);
        frame_count = *(short *)(anim_element + 0x22);
        quarter = frame_count >> 2;
        half = (frame_count >> 1) + quarter;
        random_frame = FUN_00017940(quarter, half);
        *(char *)((int)unit + 0x23c) = (char)random_frame;
        if ((char)random_frame < 2) {
          *(char *)((int)unit + 0x23c) = 1;
        }
      } else {
        *(char *)((int)unit + 0x23c) = 0;
      }
    }

    if (throttle_dir != 0) {
      short standing_idx;
      short standing_ref;
      anim_element = (int)tag_block_get_element(
            (void *)(anim_graph + 0x74), (int)chosen_anim, 0xb4);
      standing_idx = ((short (*)(int, short, int))0x120670)(
          (int)movement_type, 0, weapon_class);
      if (standing_idx < 0 ||
          standing_idx >= *(int *)(anim_graph + 0x3c)) {
        standing_ref = -1;
      } else {
        standing_ref = *(short *)(*(int *)(anim_graph + 0x40) +
                                  standing_idx * 2);
      }
      if (*(short *)(anim_element + 0x42) == standing_ref) {
        throttle_dir = 0;
      }
    }

    if (is_melee != 0) {
      if (throttle_dir == 3) {
        *(unsigned char *)((int)unit + 0x248) =
            *(unsigned char *)((int)unit + 0x248) | 8;
      } else {
        *(unsigned char *)((int)unit + 0x248) =
            *(unsigned char *)((int)unit + 0x248) & 0xf7;
      }
    }
  }

alignment_section:
  if (alignment_vector != 0 &&
      (*(unsigned int *)(unit_tag + 0x17c) & 0x200) == 0 &&
      *(short *)(unit + 0x19) == 0 &&
      unit[0x33] == (unsigned int)-1 &&
      (fast_moving || is_melee != 0)) {
    float *avec;
    avec = (float *)alignment_vector;
    switch (throttle_dir) {
    case 0:
      align_out[0] = -avec[0];
      align_out[1] = -avec[1];
      unit_apply_alignment_vector(unit_handle, align_out);
      return;
    case 1:
      align_out[0] = -avec[1];
      align_out[1] = avec[0];
      unit_apply_alignment_vector(unit_handle, align_out);
      return;
    case 2:
      align_out[0] = avec[1];
      align_out[1] = -avec[0];
      unit_apply_alignment_vector(unit_handle, align_out);
      return;
    case 3:
      align_out[0] = avec[0];
      align_out[1] = avec[1];
      unit_apply_alignment_vector(unit_handle, align_out);
      return;
    default:
      display_assert(0, "c:\\halo\\SOURCE\\units\\units.c",
                     0x11d2, 1);
      system_exit(-1);
      align_out[0] = avec[0];
      align_out[1] = avec[1];
      unit_apply_alignment_vector(unit_handle, align_out);
      return;
    }
  }
}
/* FUN_001b3690 (0x1b3690) — unit_update
 *
 * Main per-tick update for a unit (biped or vehicle). This is one of the
 * largest functions in units.obj (~5900 bytes, ~1600 instructions).
 *
 * Sections:
 *   1.  Debug tracing enter/exit
 *   2.  Animation frame counter increment
 *   3.  Running blind / aiming vector initialization
 *   4.  Effect flags and flashlight timer
 *   5.  Parent weapon holder aiming vector propagation
 *   6.  Melee timer, firing timer, vehicle enter countdown
 *   7.  Seat exit, death, vehicle exit
 *   8.  Weapon slot switch, grenade type switch, zoom level, heat sounds
 *   9.  Aiming/looking angular interpolation (instant / smooth / euler)
 *  10.  Combat state: grenade throw, melee, firing
 *  11.  Weapon firing effects and weapon power update
 *  12.  Forward/up axis validation asserts
 *  13.  Aim assist blending, weapon seat interpolation loop
 *  14.  Effect cooldown, melee collision, post-update calls
 *  15.  Steering decay, integrated light toggle, camo/flash effect
 *  16.  Per-weapon integrated light power decay/ramp
 *
 * Stack frame: SUB ESP, 0x58. Returns char (always 1).
 * Confirmed: cdecl, 1 stack param. All offsets from disassembly.
 */
char FUN_001b3690(int unit_handle)
{
    /* --- All variable declarations (C89) --- */
    int *unit;
    int tag_data;
    char local_7;
    char local_5;
    char local_8;
    char local_9;
    char local_6;
    char cVar6;
    int iVar10;
    uint32_t uVar15;
    uint32_t unit_flags;
    float fVar1;
    float fVar20;
    float local_14;
    float aim_vel_limit;
    float aim_accel_limit;
    float *aim_vec;
    float saved_aim[3];
    /* Euler-aim transform (real_matrix4x3): scale + forward/left/up rows +
     * translation. MUST be one contiguous 13-float block — it is passed by
     * address to FUN_001b0630, which does real_matrix4x3_transform_point on it.
     * The decompiler split it into separate stack locals (local_5c/58/4c/48/44/
     * 40/34) that MSVC happened to place contiguously; clang scatters separate
     * locals, so passing &local_5c yielded a garbage matrix and the AI aim
     * vector collapsed to world-up (0,0,1). An explicit aggregate fixes it. */
    struct { float scale; float forward[3]; float left[3]; float up[3];
             float translation[3]; } em;
    float zoom_interp;
    int zoom_sound_ref;
    char zoom_level_new;
    short grenade_idx;
    short *anim_ptr;
    short anim_counter;
    unsigned char anim_byte;
    int holder;
    int wpn_tag;
    int game_glob;
    int fp_iface;
    int elem;
    float delta_angle;
    float ratio;
    short seat_i;
    int seat_off;
    char is_held;
    float flash_mod;
    int eff_timer_val;
    int eff_handle;
    int eff_data;
    int16_t eff_anim;
    short wpn_idx;
    int unit_data_tmp;

    /* === Entry === */
    unit = (int *)object_get_and_verify_type(unit_handle, 3);
    tag_data = (int)tag_get(0x756e6974, *unit);
    local_7 = 0;
    local_5 = 0;
    local_8 = 0;
    local_9 = FUN_000ab9e0();

    /* [1] Debug trace enter */
    if (*(char *)0x449ef1 != 0 && *(char *)0x32de90 != 0) {
        ((void (*)(void *))0x8fa40)((void *)0x32de88);
    }
    unit_control_trace(unit_handle, (const char *)0x2b7c98);

    /* [2] Animation frame counter */
    anim_ptr = *(short **)0x4e4cf8;
    *(short *)((char *)unit + 0x1bc) = *(short *)((char *)unit + 0x1bc) + 1;
    anim_counter = *(short *)((char *)unit + 0x1bc);

    if (*(char *)((char *)anim_ptr + 4) == 0 && *anim_ptr < anim_counter) {
        *(char *)((char *)anim_ptr + 4) = 1;
        local_7 = 1;
        *(short *)((char *)unit + 0x1bc) = 0;
    } else {
        if (anim_ptr[1] > anim_counter) {
            anim_counter = anim_ptr[1];
        }
        anim_ptr[1] = anim_counter;
    }

    /* [3] Running blind vs static initialization */
    if ((unit[0x6d] & 0x2000000) != 0) {
        /* Running blind */
        unit_update_running_blind(unit_handle, (float *)((char *)unit + 0x1d4));
        /* Copy aiming -> looking, desired */
        *(float *)((char *)unit + 0x1e0) = *(float *)((char *)unit + 0x1d4);
        *(float *)((char *)unit + 0x1e4) = *(float *)((char *)unit + 0x1d8);
        *(float *)((char *)unit + 0x1e8) = *(float *)((char *)unit + 0x1dc);
        *(float *)((char *)unit + 0x204) = *(float *)((char *)unit + 0x1d4);
        *(float *)((char *)unit + 0x208) = *(float *)((char *)unit + 0x1d8);
        *(float *)((char *)unit + 0x20c) = *(float *)((char *)unit + 0x1dc);
        /* Up from global 0x31fc3c */
        {
            float *g = *(float **)0x31fc3c;
            *(float *)((char *)unit + 0x228) = g[0];
            *(float *)((char *)unit + 0x22c) = g[1];
            *(float *)((char *)unit + 0x230) = g[2];
        }
        unit[0x6e] = 0;
    } else if ((unit[0x6d] & 1) == 0) {
        /* Static: copy object forward -> all aiming slots */
        float *fwd = (float *)((char *)unit + 0x24);
        *(float *)((char *)unit + 0x204) = fwd[0];
        *(float *)((char *)unit + 0x208) = fwd[1];
        *(float *)((char *)unit + 0x20c) = fwd[2];
        *(float *)((char *)unit + 0x1e0) = fwd[0];
        *(float *)((char *)unit + 0x1e4) = fwd[1];
        *(float *)((char *)unit + 0x1e8) = fwd[2];
        *(float *)((char *)unit + 0x1d4) = fwd[0];
        *(float *)((char *)unit + 0x1d8) = fwd[1];
        *(float *)((char *)unit + 0x1dc) = fwd[2];
        /* Up from global 0x31fc38 */
        {
            float *g = *(float **)0x31fc38;
            *(float *)((char *)unit + 0x228) = g[0];
            *(float *)((char *)unit + 0x22c) = g[1];
            *(float *)((char *)unit + 0x230) = g[2];
        }
        unit[0x6e] = 0;
    }

    /* [4-7] Main update block (unless tag has interlocked flag 0x800) */
    if ((*(uint32_t *)(tag_data + 0x17c) & 0x800) == 0) {
        eff_timer_val = unit[0x70];
        local_6 = 0;

        /* [4] Effect timer countdown */
        if (eff_timer_val > 0) {
            uVar15 = (uint32_t)unit[0x6e] | (uint32_t)unit[0x71];
            unit[0x6e] = (int)uVar15;
            if ((unit[0x71] & 0x800) == 0) {
                unit[0x8d] = 0;
            } else {
                if (eff_timer_val % 7 == 0) {
                    uVar15 = uVar15 | 0x800;
                } else {
                    uVar15 = uVar15 & 0xfffff7ff;
                }
                unit[0x6e] = (int)uVar15;
                unit[0x8d] = 0x3f800000;
            }
            unit[0x70] = eff_timer_val - 1;
            if (eff_timer_val - 1 == 0) {
                unit[0x71] = 0;
            }
        }

        /* [5] Parent weapon holder aiming vectors */
        if ((unit[0x6d] & 0x8000000) == 0) {
            /* Primary holder (+0x2D4) */
            if (unit[0xb5] != -1 &&
                (*(unsigned char *)((char *)unit + 0xb6) & 4) == 0) {
                holder = (int)object_get_and_verify_type(unit[0xb5], 3);
                *(short *)((char *)unit + 0x68) = *(short *)(holder + 0x68);
                local_6 = 1;

                if (*(int *)(holder + 0x1c8) != -1 ||
                    (*(char *)(holder + 0x253) != 0x1b &&
                     *(char *)(holder + 0x253) != 0x1a)) {
                    assert_halt(valid_real_normal3d((float *)(holder + 0x1d4)));
                    unit[0x6e] = (int)((uint32_t)unit[0x6e] |
                                 (*(uint32_t *)(holder + 0x1b8) & 0x3f));
                    *(float *)((char *)unit + 0x1d4) = *(float *)(holder + 0x1d4);
                    *(float *)((char *)unit + 0x1d8) = *(float *)(holder + 0x1d8);
                    *(float *)((char *)unit + 0x1dc) = *(float *)(holder + 0x1dc);
                    *(float *)((char *)unit + 0x228) = *(float *)(holder + 0x228);
                    *(float *)((char *)unit + 0x22c) = *(float *)(holder + 0x22c);
                    *(float *)((char *)unit + 0x230) = *(float *)(holder + 0x230);
                }
            }
            /* Secondary holder (+0x2D8) */
            if (unit[0xb6] != -1 &&
                (*(unsigned char *)((char *)unit + 0xb6) & 4) == 0) {
                holder = (int)object_get_and_verify_type(unit[0xb6], 3);
                if (local_6 == 0) {
                    *(short *)((char *)unit + 0x68) = *(short *)(holder + 0x68);
                }
                if (*(int *)(holder + 0x1c8) != -1 ||
                    (*(char *)(holder + 0x253) != 0x1b &&
                     *(char *)(holder + 0x253) != 0x1a)) {
                    assert_halt(valid_real_normal3d((float *)(holder + 0x1e0)));
                    *(float *)((char *)unit + 0x1e0) = *(float *)(holder + 0x1e0);
                    *(float *)((char *)unit + 0x1e4) = *(float *)(holder + 0x1e4);
                    *(float *)((char *)unit + 0x1e8) = *(float *)(holder + 0x1e8);
                    *(float *)((char *)unit + 0x204) = *(float *)(holder + 0x1e0);
                    *(float *)((char *)unit + 0x208) = *(float *)(holder + 0x1e4);
                    *(float *)((char *)unit + 0x20c) = *(float *)(holder + 0x1e8);
                    unit[0x6e] = (int)((uint32_t)unit[0x6e] |
                                 (*(uint32_t *)(holder + 0x1b8) & 0x7c00));
                    unit[0x8d] = *(int *)(holder + 0x234);
                }
            }
            /* Flashlight-held counter */
            if (((uint32_t)unit[0x6e] & 0x7c00) == 0) {
                if (*(char *)((char *)unit + 0x2d2) < 0x7f) {
                    *(char *)((char *)unit + 0x2d2) =
                        *(char *)((char *)unit + 0x2d2) + 1;
                }
            } else {
                *(char *)((char *)unit + 0x2d2) = 0;
            }
        }

        /* Flashlight + firing combined check */
        if (*(char *)0x5aa891 != 0 &&
            ((uint32_t)unit[0x6e] & 0x800) != 0 &&
            ((uint32_t)unit[0x6e] & 0x2000) != 0) {
            local_5 = 1;
        }

        /* [6a] Melee timer (+0x32C) */
        if ((*(unsigned char *)((char *)unit + 0x1b4) & 0x10) == 0) {
            fVar20 = *(float *)((char *)unit + 0x32c) - *(float *)0x28ac20;
            *(float *)((char *)unit + 0x32c) = fVar20;
            if (fVar20 < 0.0f) {
                *(float *)((char *)unit + 0x32c) = 0.0f;
            }
        } else {
            cVar6 = ((char (*)(void))0xa8e30)();
            fVar20 = *(float *)0x28ac20;
            if (cVar6 != 0 && *(short *)((char *)unit + 0x3d2) != 0 &&
                *(short *)((char *)unit + 0x3d2) == 1) {
                iVar10 = (int)object_get_and_verify_type(unit_handle, 3);
                iVar10 = unit_get_weapon(unit_handle,
                             *(int16_t *)(iVar10 + 0x2a2));
                fVar20 = *(float *)0x28ac20;
                if (iVar10 != -1) {
                    int *wo = (int *)object_get_and_verify_type(iVar10, 4);
                    iVar10 = (int)tag_get(0x77656170, *wo);
                    fVar20 = *(float *)0x28ac20;
                    if (*(float *)(iVar10 + 0x4d0) != 0.0f) {
                        fVar20 = *(float *)(iVar10 + 0x4d0);
                    }
                }
            }
            fVar1 = *(float *)((char *)unit + 0x32c);
            *(float *)((char *)unit + 0x32c) = fVar20 + fVar1;
            if (fVar20 + fVar1 > 1.0f) {
                *(float *)((char *)unit + 0x32c) = 1.0f;
                *(short *)((char *)unit + 0x3d2) = 0;
            }
        }

        /* [6b] Firing timer (+0x330) */
        if ((*(unsigned char *)((char *)unit + 0x1b4) & 0x20) == 0) {
            fVar20 = *(float *)((char *)unit + 0x330) - *(float *)0x26f2e0;
            *(float *)((char *)unit + 0x330) = fVar20;
            if (fVar20 < 0.0f) {
                *(float *)((char *)unit + 0x330) = 0.0f;
            }
        } else {
            fVar20 = *(float *)((char *)unit + 0x330) + *(float *)0x26f2e0;
            *(float *)((char *)unit + 0x330) = fVar20;
            if (fVar20 > 1.0f) {
                *(float *)((char *)unit + 0x330) = 1.0f;
            }
        }

        /* [6c] Vehicle enter countdown (+0x3D8) */
        {
            int16_t t = *(int16_t *)((char *)unit + 0x3d8);
            if (t > 0) {
                t--;
                *(int16_t *)((char *)unit + 0x3d8) = t;
                if (t == 0) { unit[0xf5] = 0; }
            }
        }

        /* [7a] Seat exit timer (+0x23C) */
        {
            char st = *(char *)((char *)unit + 0x23c);
            if (st > 0) {
                st--;
                *(char *)((char *)unit + 0x23c) = st;
                if (st == 0) { unit_set_in_vehicle(unit_handle, 1); }
            }
        }

        /* [7b] Death / vehicle exit (+0x3D0) */
        {
            int16_t et = *(int16_t *)((char *)unit + 0x3d0);
            if (et > 0 && (*(unsigned char *)((char *)unit + 4) & 0x20) != 0) {
                et--;
                *(int16_t *)((char *)unit + 0x3d0) = et;
                if (et == 0) {
                    if (*(float *)((char *)unit + 0x90) <= 0.0f) {
                        unit_died(unit_handle, 0);
                    } else {
                        *(unsigned char *)((char *)unit + 0xb6) =
                            *(unsigned char *)((char *)unit + 0xb6) & 0xfb;
                        anim_byte = *(unsigned char *)((char *)unit + 0x248);
                        unit_set_actively_controlled(unit_handle, 1);
                        unit_try_animation_state(unit_handle,
                            (int)*(int **)0x32e48c, 0, 1);
                        FUN_001ad260(unit_handle,
                            (int16_t)((~(anim_byte >> 3) & 1) | 0x22));
                        *(unsigned char *)((char *)unit + 0x248) =
                            *(unsigned char *)((char *)unit + 0x248) & 0xfb;
                        if (*(short *)((char *)unit + 0x64) == 0) {
                            biped_stop_limp_body_physics(unit_handle);
                        }
                        FUN_001a74d0(unit_handle, 5);
                    }
                }
            }
        }
    } /* end tag 0x800 check */

    /* ================================================================
     * [8-10] Weapon management, aiming, combat state
     * (unless tag has "interlocked weapons" flag 0x400)
     * ================================================================ */
    if ((*(uint32_t *)(tag_data + 0x17c) & 0x400) == 0) {
        if ((*(unsigned short *)((char *)unit + 0xb6) & 4) == 0) {
            /* [8a] Weapon slot switch */
            if ((*(unsigned short *)((char *)unit + 0xb6) & 0x400) == 0) {
                if (*(short *)((char *)unit + 0x2a4) !=
                    *(short *)((char *)unit + 0x2a2)) {
                    cVar6 = FUN_001a8730((void *)((char *)unit + 0x248));
                    if (cVar6 == 0) {
                        iVar10 = (int)object_get_and_verify_type(unit_handle, 3);
                        iVar10 = unit_get_weapon(unit_handle,
                                     *(int16_t *)(iVar10 + 0x2a4));
                        if (iVar10 != -1) {
                            cVar6 = unit_can_enter_seat(unit_handle, iVar10);
                            if (cVar6 != 0) {
                                unit_update_weapon_readiness(unit_handle, 1);
                            }
                        }
                    }
                }
            } else {
                unit_set_in_vehicle(unit_handle, 1);
            }

            /* [8b] Grenade type switch */
            if (*(char *)((char *)unit + 0x2cd) !=
                *(char *)((char *)unit + 0x2cc)) {
                cVar6 = FUN_001a8730((void *)((char *)unit + 0x248));
                if (cVar6 == 0) {
                    grenade_idx = unit_inventory_next_grenade(unit_handle,
                        (short)(signed char)*(char *)((char *)unit + 0x2cd), 0);
                    if (grenade_idx != -1) {
                        *(char *)((char *)unit + 0x2cc) = (char)grenade_idx;
                    }
                }
            }

            /* [8c] Grenade count enforcement */
            if (*(char *)0x5aa892 != 0 && unit[0x72] != -1) {
                char *gc = (char *)((char *)unit + 0x2ce);
                int gi;
                for (gi = 2; gi != 0; gi--) {
                    signed char gv = *gc;
                    if (gv < 2) { gv = 1; }
                    *gc = gv;
                    gc++;
                }
                if (*(char *)((char *)unit + 0x2cd) == (char)-1) {
                    *(char *)((char *)unit + 0x2cd) = 0;
                }
            }

            /* [8d] Zoom level change + weapon heat sound */
            zoom_level_new = *(char *)((char *)unit + 0x2d1);
            if (zoom_level_new != *(char *)((char *)unit + 0x2d0)) {
                *(char *)((char *)unit + 0x2d0) = zoom_level_new;
                if (zoom_level_new == (char)-1) {
                    unit[0xbe] = 0;
                }
                iVar10 = ((int (*)(int))0xba500)(unit_handle);
                if (iVar10 != -1) {
                    int pi = ((int (*)(int))0xba500)(unit_handle);
                    int pd = (int)datum_get(*(void **)0x5aa6d4, pi);
                    if (*(short *)(pd + 2) != -1) {
                        iVar10 = (int)object_get_and_verify_type(unit_handle, 3);
                        iVar10 = unit_get_weapon(unit_handle,
                                     *(int16_t *)(iVar10 + 0x2a2));
                        if (iVar10 != -1) {
                            int *wo = (int *)object_get_and_verify_type(iVar10, 4);
                            wpn_tag = (int)tag_get(0x77656170, *wo);
                            zoom_level_new = *(char *)((char *)unit + 0x2d0);
                            if (zoom_level_new == (char)-1) {
                                zoom_sound_ref = *(int *)(wpn_tag + 0x4bc);
                            } else {
                                zoom_sound_ref = *(int *)(wpn_tag + 0x4ac);
                            }
                            zoom_interp = 1.0f;
                            if (zoom_level_new != (char)-1 &&
                                *(short *)(wpn_tag + 0x3da) > 1) {
                                int denom = (int)*(short *)(wpn_tag + 0x3da) - 1;
                                zoom_interp =
                                    (float)(int)(signed char)zoom_level_new /
                                    (float)denom;
                            }
                            if (zoom_sound_ref != -1) {
                                ((void (*)(int, float))0x1c7480)(
                                    zoom_sound_ref, zoom_interp);
                            }
                        }
                    }
                }
            }
        } /* end seat-held check */

        /* [9] Aiming and looking update */
        unit_control_trace(unit_handle, (const char *)0x2b7c28);

        if (*(char *)((char *)unit + 0x238) == 1) {
            local_14 = *(float *)(tag_data + 0x26c);
        } else {
            local_14 = 1.0f;
        }

        aim_vec = (float *)((char *)unit + 0x1ec);
        aim_vel_limit = local_14 * *(float *)(tag_data + 0x264) *
                        *(float *)0x2546a4;
        saved_aim[0] = aim_vec[0];
        saved_aim[1] = aim_vec[1];
        saved_aim[2] = aim_vec[2];
        aim_accel_limit = local_14 * *(float *)(tag_data + 0x268) *
                          *(float *)0x25620c;

        /* [9a] Aiming: 3 paths */
        if (aim_vel_limit == 0.0f && aim_accel_limit == 0.0f) {
            /* Instant */
            float *des = (float *)((char *)unit + 0x1e0);
            assert_halt(valid_real_normal3d(des));
            aim_vec[0] = des[0]; aim_vec[1] = des[1]; aim_vec[2] = des[2];
            unit_clip_to_aiming_bounds(unit_handle, aim_vec, 1);
            {
                float *g = *(float **)0x31fc38;
                *(float *)((char *)unit + 0x1f8) = g[0];
                *(float *)((char *)unit + 0x1fc) = g[1];
                *(float *)((char *)unit + 0x200) = g[2];
            }
            unit_control_trace(unit_handle, (const char *)0x2b7bf0);
        } else if (*(char *)((char *)unit + 0x266) == 0) {
            /* Smooth */
            ((void (*)(float*,float*,float*,float,float))0x10f770)(
                aim_vec,
                (float *)((char *)unit + 0x1e0),
                (float *)((char *)unit + 0x1f8),
                aim_vel_limit, aim_accel_limit);
            unit_control_trace(unit_handle, (const char *)0x2b7bc4);
        } else {
            /* Euler */
            float *grav;
            em.scale = 1.0f;
            object_get_orientation(unit_handle, em.forward, em.up);
            em.left[0] = em.forward[2] * em.up[1] - em.up[2] * em.forward[1];
            em.left[1] = em.up[2] * em.forward[0] - em.forward[2] * em.up[0];
            em.left[2] = em.forward[1] * em.up[0] - em.up[1] * em.forward[0];
            grav = *(float **)0x31fc1c;
            em.translation[0] = grav[0]; em.translation[1] = grav[1];
            em.translation[2] = grav[2];
            FUN_001b0630((int)&em, aim_vec,
                (float *)((char *)unit + 0x1e0),
                (float *)((char *)unit + 0x1f8),
                (float *)((char *)unit + 0x268),
                aim_vel_limit, aim_accel_limit);
            unit_control_trace(unit_handle, (const char *)0x2b7bd8);
        }

        /* Delta angle + steering byte */
        FUN_0010c510(aim_vec, saved_aim);
        iVar10 = tag_data;
        delta_angle = FUN_0010c510(aim_vec, saved_aim);
        ratio = delta_angle / (*(float *)(iVar10 + 0x264) * *(float *)0x2546a4);
        if (ratio < 0.0f) { ratio = 0.0f; }
        else if (ratio > 1.0f) { ratio = 1.0f; }
        ratio = ratio * *(float *)0x2602c8;
        *(char *)((char *)unit + 0x2d3) = (char)(int)ratio;

        /* Assert aim vector */
        assert_halt(valid_real_normal3d(aim_vec));

        /* [9c] Looking direction */
        aim_vel_limit = local_14 * *(float *)(iVar10 + 0x270) *
                        *(float *)0x2546a4;
        aim_accel_limit = local_14 * *(float *)(iVar10 + 0x274) *
                          *(float *)0x25620c;

        if (aim_vel_limit == 0.0f && aim_accel_limit == 0.0f) {
            float *des = (float *)((char *)unit + 0x204);
            float *lk = (float *)((char *)unit + 0x210);
            lk[0] = des[0]; lk[1] = des[1]; lk[2] = des[2];
            unit_clip_to_aiming_bounds(unit_handle, lk, 0);
            {
                float *g = *(float **)0x31fc38;
                *(float *)((char *)unit + 0x21c) = g[0];
                *(float *)((char *)unit + 0x220) = g[1];
                *(float *)((char *)unit + 0x224) = g[2];
            }
            unit_control_trace(unit_handle, (const char *)0x2b7bac);
        } else if (*(char *)((char *)unit + 0x267) == 0) {
            float *lk = (float *)((char *)unit + 0x210);
            ((void (*)(float*,float*,float*,float,float))0x10f770)(
                lk,
                (float *)((char *)unit + 0x204),
                (float *)((char *)unit + 0x21c),
                aim_vel_limit, aim_accel_limit);
            unit_control_trace(unit_handle, (const char *)0x2b7b80);
        } else {
            float *lk = (float *)((char *)unit + 0x210);
            float *grav;
            em.scale = 1.0f;
            object_get_orientation(unit_handle, em.forward, em.up);
            em.left[0] = em.forward[2] * em.up[1] - em.up[2] * em.forward[1];
            em.left[1] = em.up[2] * em.forward[0] - em.forward[2] * em.up[0];
            em.left[2] = em.forward[1] * em.up[0] - em.up[1] * em.forward[0];
            grav = *(float **)0x31fc1c;
            em.translation[0] = grav[0]; em.translation[1] = grav[1];
            em.translation[2] = grav[2];
            FUN_001b0630((int)&em, lk,
                (float *)((char *)unit + 0x204),
                (float *)((char *)unit + 0x21c),
                (float *)((char *)unit + 0x278),
                aim_vel_limit, aim_accel_limit);
            unit_control_trace(unit_handle, (const char *)0x2b7b94);
        }

        /* Assert look vector */
        assert_halt(valid_real_normal3d((float *)((char *)unit + 0x210)));
        unit_control_trace(unit_handle, (const char *)0x2b7b4c);

        /* [10] Combat state switch */
        if (local_5 == 0) {
            uint32_t ff = (uint32_t)unit[0x6e] >> 0xd;
            switch (*(signed char *)((char *)unit + 0x23d)) {
            case 0:
                if ((ff & 1) != 0) {
                    unit_throw_grenade_begin(unit_handle, 0);
                }
                break;
            case 1:
                if (*(short *)((char *)unit + 0x82) > 1) {
                    FUN_001aaf40(unit_handle);
                }
                break;
            case 2:
                *(short *)((char *)unit + 0x23e) =
                    *(short *)((char *)unit + 0x23e) + 1;
                if (*(char *)((char *)unit + 0x253) != 0x21) {
                    FUN_001ab110(unit_handle, 1);
                }
                break;
            case 3:
                if (*(char *)((char *)unit + 0x253) != 0x21 &&
                    (ff & 1) == 0) {
                    *(char *)((char *)unit + 0x23d) = (char)(ff & 1);
                }
                break;
            }
        }

        /* [11] Weapon firing effects */
        if (*(short *)((char *)unit + 0x2a2) != -1) {
            flash_mod = *(float *)((char *)unit + 0x234);
            uVar15 = 0;

            if (*(short *)((char *)unit + 0x2a2) ==
                *(short *)((char *)unit + 0x2a4)) {
                /* Current = desired weapon */
                if (unit[0x70] > 0 && (unit[0x71] & 0x800) != 0) {
                    local_6 = 1;
                } else {
                    local_6 = 0;
                }
                if (local_5 == 0) {
                    if (local_9 != 0 &&
                        (*(unsigned char *)((char *)unit + 0x1b8) & 0x10) != 0) {
                        uVar15 = 1;
                    }
                    if (((uint32_t)unit[0x6e] & 0x800) != 0) { uVar15 |= 2; }
                    if (((uint32_t)unit[0x6e] & 0x1000) != 0) { uVar15 |= 4; }
                }
                {
                    int ut = (int)tag_get(0x756e6974, *unit);
                    if ((*(uint32_t *)(ut + 0x17c) & 0x800000) != 0) {
                        int ud2 = (int)object_get_and_verify_type(unit_handle, 3);
                        int w = unit_get_weapon(unit_handle,
                                    *(int16_t *)(ud2 + 0x2a2));
                        ((void (*)(int))0xfaeb0)(w);
                    }
                }
                if (((uint32_t)unit[0x6e] & 0x400) != 0) { uVar15 |= 8; }
                cVar6 = FUN_001a8730((void *)((char *)unit + 0x248));
                if (cVar6 != 0 && local_6 == 0) { uVar15 |= 0x10; }
                if (*(short *)((char *)unit + 0x64) == 0 &&
                    *(char *)((char *)unit + 0x45d) > 0) {
                    uVar15 |= 0x10;
                }
                if (*(char *)((char *)unit + 0x2d0) != (char)-1) {
                    uVar15 |= 0x40;
                }
            } else {
                uVar15 = 0x20;
            }

            {
                int ud3 = (int)object_get_and_verify_type(unit_handle, 3);
                int w2 = unit_get_weapon(unit_handle,
                             *(int16_t *)(ud3 + 0x2a2));
                ((void (*)(int, uint32_t, float))0xfc4b0)(
                    w2, uVar15, flash_mod);
            }
        }
    } /* end tag 0x400 check */

    /* [12] Assert forward/up axes valid */
    {
        float *obj_fwd = (float *)((char *)unit + 0x24);
        float *obj_up = (float *)((char *)unit + 0x30);
        cVar6 = (char)valid_real_normal3d_perpendicular(obj_fwd, obj_up);
        if (cVar6 == 0) {
            char *msg = csprintf(*(char **)0x5ab100,
                "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)",
                "&unit->object.forward", "&unit->object.up",
                (double)obj_fwd[0], (double)obj_fwd[1], (double)obj_fwd[2],
                (double)obj_up[0], (double)obj_up[1], (double)obj_up[2]);
            display_assert(msg, "c:\\halo\\SOURCE\\units\\units.c", 0x483, 1);
            system_exit(-1);
        }
    }
    assert_halt(valid_real_normal3d((float *)((char *)unit + 0x1ec)));
    assert_halt(valid_real_normal3d((float *)((char *)unit + 0x210)));

    /* [13] Aim assist blending + weapon seat loop */
    if ((*(uint32_t *)(tag_data + 0x17c) & 0x800) == 0) {
        if ((*(unsigned char *)((char *)unit + 0x248) & 2) != 0) {
            unit_aiming_vector(unit_handle);
            *(float *)((char *)unit + 0x314) =
                *(float *)((char *)unit + 0x314) * 0.7f +
                *(float *)((char *)unit + 0x320) * 0.3f;
            *(float *)((char *)unit + 0x318) =
                *(float *)((char *)unit + 0x318) * 0.7f +
                *(float *)((char *)unit + 0x324) * 0.3f;
            *(float *)((char *)unit + 0x31c) =
                *(float *)((char *)unit + 0x31c) * 0.7f +
                *(float *)((char *)unit + 0x328) * 0.3f;
        }

        /* Weapon seat interpolation loop */
        {
            int *tb = (int *)(tag_data + 0x2cc);
            seat_i = 0;
            if (*tb > 0) {
                seat_off = 0;
                do {
                    elem = (int)tag_block_get_element(tb, seat_off, 0x44);
                    if (seat_i == 0) {
                        if (unit[0xb5] == -1 &&
                            (*(unsigned char *)((char *)unit + 0x1b4) & 1) == 0) {
                            is_held = 0;
                        } else {
                            is_held = 1;
                        }
                    } else {
                        if (unit[0xb6] == -1 || unit[0xb6] == unit[0xb5]) {
                            is_held = 0;
                        } else {
                            is_held = 1;
                        }
                    }
                    if ((*(unsigned char *)((char *)unit + 0xb6) & 4) == 0 &&
                        is_held) {
                        if (*(int *)((char *)unit + seat_off * 4 + 0x2e8) !=
                            0x3f800000) {
                            fVar20 = 1.0f /
                                (*(float *)(elem + 4) * TICKS_PER_SECOND) +
                                *(float *)((char *)unit + seat_off * 4 + 0x2e8);
                            *(float *)((char *)unit + seat_off * 4 + 0x2e8) =
                                fVar20;
                            if (fVar20 > 1.0f) {
                                *(float *)((char *)unit + seat_off * 4 + 0x2e8) =
                                    1.0f;
                            }
                        }
                    } else {
                        if (*(float *)((char *)unit + seat_off * 4 + 0x2e8) !=
                            0.0f) {
                            fVar20 =
                                *(float *)((char *)unit + seat_off * 4 + 0x2e8) -
                                1.0f /
                                (*(float *)(elem + 8) * TICKS_PER_SECOND);
                            *(float *)((char *)unit + seat_off * 4 + 0x2e8) =
                                fVar20;
                            if (fVar20 < 0.0f) {
                                *(float *)((char *)unit + seat_off * 4 + 0x2e8) =
                                    0.0f;
                            }
                        }
                    }
                    seat_i++;
                    seat_off = (int)seat_i;
                } while (seat_off < *tb);
            }
        }
    }

    /* [14] Effect cooldown timer (+0x3B6) */
    {
        int16_t et2 = *(int16_t *)((char *)unit + 0x3b6);
        if (et2 > 0) {
            et2--;
            *(int16_t *)((char *)unit + 0x3b6) = et2;
            if (et2 == 0) {
                eff_handle = *(int *)((char *)unit + 0x3bc);
                eff_data = *(int *)((char *)unit + 0x3b8);
                eff_anim = *(int16_t *)((char *)unit + 0x3b4);
                ((void (*)(int, int, int16_t, int, int, int))0x40460)(
                    unit_handle, eff_handle, eff_anim, eff_data, 0, 1);
                *(int16_t *)((char *)unit + 0x3b4) = 0;
                unit[0xef] = -1;
                unit[0xee] = 0;
            }
        }
    }

    /* Post-update calls */
    FUN_001abd90(unit_handle);
    FUN_001a7790(unit_handle);

    /* Weapon alert sound */
    if ((local_7 != 0 || unit[0x72] != -1) &&
        (FUN_001ab8c0(unit_handle),
         *(char *)0x5054fa != 0) &&
        unit[0x72] != -1) {
        iVar10 = ((int (*)(void))0xb5aa0)();
        if (iVar10 >= *(int *)0x32e480 + 0x1e) {
            ((void (*)(int, const char *, ...))0x8f390)(
                2, (const char *)0x2b7b20,
                (double)*(float *)((char *)unit + 0x290),
                (double)*(float *)((char *)unit + 0x294));
            *(int *)0x32e480 = iVar10;
        }
    }

    /* Flame-to-death timer (+0x23B) */
    {
        char ft = *(char *)((char *)unit + 0x23b);
        if (ft > 0) {
            ft--;
            *(char *)((char *)unit + 0x23b) = ft;
            if (ft == 0) { unit_flame_to_death(unit_handle); }
        }
    }

    /* [15] Steering decay */
    {
        float neg = -(*(float *)((char *)unit + 0x298));
        float cl;
        if (*(float *)0x25e884 <= neg) {
            cl = neg;
            if (neg > *(float *)0x25496c) { cl = *(float *)0x25496c; }
        } else {
            cl = *(float *)0x25e884;
        }
        *(float *)((char *)unit + 0x298) = cl + *(float *)((char *)unit + 0x298);
    }

    /* Integrated light toggle flags */
    unit_flags = (uint32_t)unit[0x6d];
    cVar6 = local_8;
    if ((unit_flags & 0x10000000) != 0) {
        cVar6 = 1;
        if ((unit_flags & 0x80000) != 0) { cVar6 = local_8; }
        unit[0x6d] = (int)(unit_flags & 0xefffffff);
    }
    unit_flags = (uint32_t)unit[0x6d];
    if ((unit_flags & 0x20000000) != 0) {
        if ((unit_flags & 0x80000) != 0) { cVar6 = 1; }
        unit[0x6d] = (int)(unit_flags & 0xdfffffff);
    }

    /* [17] Weapon flash/camo effect */
    if ((local_9 != 0 &&
         (*(unsigned char *)((char *)unit + 0x1b8) & 0x10) != 0) ||
        *(float *)((char *)unit + 0x2f4) < 0.0f ||
        cVar6 != 0) {

        iVar10 = (int)object_get_and_verify_type(unit_handle, 3);
        if (*(char *)(iVar10 + 0x2d0) != (char)-1) {
            iVar10 = (int)object_get_and_verify_type(unit_handle, 3);
            iVar10 = unit_get_weapon(unit_handle,
                         *(int16_t *)(iVar10 + 0x2a2));
            if (iVar10 != -1) {
                int *wo = (int *)object_get_and_verify_type(iVar10, 4);
                wpn_tag = (int)tag_get(0x77656170, *wo);
                if ((*(uint32_t *)(wpn_tag + 0x308) & 0x4000) != 0 &&
                    (*(unsigned char *)((char *)unit + 0x1b8) & 0x10) != 0) {
                    game_glob = (int)game_globals_get();
                    assert_halt_msg(game_glob != 0, "game_globals");
                    fp_iface = (int)tag_block_get_element(
                        (void *)(game_glob + 0x17c), 0, 0xc0);
                    assert_halt_msg(fp_iface != 0,
                        "game_globals_first_person_interface");
                    if ((unit[0x6d] & 0x4000000) == 0) {
                        iVar10 = *(int *)(fp_iface + 0x54);
                    } else {
                        iVar10 = *(int *)(fp_iface + 0x64);
                    }
                    if (iVar10 != -1) {
                        FUN_0009ec30(iVar10, unit_handle, unit_handle,
                            -1, 0.0f, 0.0f, 0, 0);
                    }
                    unit[0x6d] = unit[0x6d] ^ 0x4000000;
                    if ((*(unsigned char *)((char *)unit + 0x1b8) & 0x10) != 0) {
                        goto label_flashlight_done;
                    }
                }
            }
        }
        /* Normal flashlight toggle */
        if (((unit[0x6d] & 0x80000) != 0 ||
             *(float *)((char *)unit + 0x2f4) > *(float *)0x2549d4) &&
            unit[0x33] == -1) {
            FUN_0009ec30(*(int *)(tag_data + 0x194), unit_handle, unit_handle,
                -1, 0.0f, 0.0f, 0, 0);
            unit[0x6d] = unit[0x6d] ^ 0x80000;
        }
    }
label_flashlight_done:

    /* [18] Camo/weapon power timers */
    if ((unit[0x6d] & 0x80000) == 0) {
        if (*(float *)((char *)unit + 0x2f4) < 1.0f) {
            *(float *)((char *)unit + 0x2f4) =
                *(float *)((char *)unit + 0x2f4) + *(float *)0x25620c;
        }
        if (*(float *)((char *)unit + 0x2f0) != 0.0f) {
            fVar20 = *(float *)((char *)unit + 0x2f0) - *(float *)0x28af18;
            *(float *)((char *)unit + 0x2f0) = fVar20;
            if (fVar20 < 0.0f) {
                *(float *)((char *)unit + 0x2f0) = 0.0f;
            }
        }
    } else {
        if ((*(uint32_t *)(tag_data + 0x17c) & 0x1000000) == 0) {
            *(float *)((char *)unit + 0x2f4) =
                *(float *)((char *)unit + 0x2f4) - *(float *)0x253f2c;
        }
        if (unit[0x33] != -1 ||
            (*(unsigned char *)((char *)unit + 0xb6) & 4) != 0) {
            unit[0x6d] = unit[0x6d] & (int)0xfff7ffff;
        }
        if (*(int *)((char *)unit + 0x2f0) != 0x3f800000) {
            fVar20 = *(float *)((char *)unit + 0x2f0) + *(float *)0x2647d4;
            *(float *)((char *)unit + 0x2f0) = fVar20;
            if (fVar20 > 1.0f) {
                *(float *)((char *)unit + 0x2f0) = 1.0f;
            }
        }
    }

    /* [19] Per-weapon integrated light power */
    iVar10 = (int)object_get_and_verify_type(unit_handle, 3);
    if (*(char *)(iVar10 + 0x2d0) != (char)-1) {
        unit_data_tmp = (int)object_get_and_verify_type(unit_handle, 3);
        wpn_idx = *(int16_t *)(unit_data_tmp + 0x2a2);
        unit_data_tmp = (int)object_get_and_verify_type(unit_handle, 3);
        if (wpn_idx != -1) {
            if (wpn_idx < 0 || wpn_idx >= 4) {
                display_assert(
                    "index>=0 && index<MAXIMUM_WEAPONS_PER_UNIT",
                    "c:\\halo\\SOURCE\\units\\units.c", 0x20ac, 1);
                system_exit(-1);
            }
            iVar10 = *(int *)(unit_data_tmp + 0x2a8 + wpn_idx * 4);
            if (iVar10 != -1) {
                int *wo = (int *)object_get_and_verify_type(iVar10, 4);
                wpn_tag = (int)tag_get(0x77656170, *wo);
                if ((*(uint32_t *)(wpn_tag + 0x308) & 0x4000) != 0) {
                    if ((unit[0x6d] & 0x4000000) == 0) {
                        if (*(float *)((char *)unit + 0x2f8) != 0.0f) {
                            fVar20 = *(float *)((char *)unit + 0x2f8) -
                                     *(float *)0x28af18;
                            *(float *)((char *)unit + 0x2f8) = fVar20;
                            if (fVar20 < 0.0f) {
                                *(float *)((char *)unit + 0x2f8) = 0.0f;
                            }
                        }
                    } else {
                        if (*(int *)((char *)unit + 0x2f8) != 0x3f800000) {
                            fVar20 = *(float *)((char *)unit + 0x2f8) +
                                     *(float *)0x255960;
                            *(float *)((char *)unit + 0x2f8) = fVar20;
                            if (fVar20 > 1.0f) {
                                *(float *)((char *)unit + 0x2f8) = 1.0f;
                            }
                        }
                    }
                }
            }
        }
    }

    /* [20] Debug trace exit */
    unit_control_trace(unit_handle, (const char *)0x2b7aec);
    if (*(char *)0x449ef1 != 0 && *(char *)0x32de90 != 0) {
        ((void (*)(void *))0x8fac0)((void *)0x32de88);
    }

    return 1;
}

/* FUN_001a6350 (0x1a6350)
 * Biped per-tick update dispatcher. Called each tick for biped-type units.
 * If the biped is free (no parent object), runs the full update chain:
 *   - FUN_001a4440: pre-update setup
 *   - normalize forward vector at +0x1D4, set animation state byte at +0x42a
 *   - clamp/reset velocity at +0x228, melee counters at +0x459/+0x45a
 *   - FUN_001a4c50: turning, FUN_001a5300: moving
 *   - FUN_001a2900/FUN_001a2a60/FUN_001a2b10/FUN_001a6280: death/air/land/slip
 *   - melee damage timer at +0x45d/+0x45e
 *   - FUN_001a2440, FUN_001a1e70, FUN_001a0b30: footstep/marker events
 * If seated in a vehicle (parent +0xCC != -1), handles ejection and exit.
 * FUN_001b0d90 runs at the end in both cases; tracks suspension ticks at +0x6C.
 * Always returns 1 (via CONCAT31).
 * Confirmed: cdecl, 1 stack param, returns char. */
char FUN_001a6350(int unit_handle)
{
    unsigned int *biped;
    char *biped_tag;
    char *parent;
    char *vehicle;
    char *weapon_unit;
    int weapon_handle;
    int weapon_handle_saved;
    char result1;
    char result2;
    char shifted;
    signed char anim_state;
    float mag_sq;
    char *fwd_ptr;
    float *vec;
    short anim_result;
    unsigned char state_pair[2]; /* [0]=local_8 (flags), [1]=local_7 (ground contact) */

    biped = (unsigned int *)object_get_and_verify_type(unit_handle, 1);
    biped_tag = (char *)tag_get(0x62697064, *(int *)biped);

    if (*(char *)0x004e4cf1 != 0) {
        return 1;
    }

    /* Debug trace enter */
    if (*(char *)0x00449ef1 != 0 && *(char *)0x0032d1d8 != 0) {
        profile_enter_private((void *)0x0032d1d0);
    }

    FUN_001a2800(unit_handle, "pre-update");

    state_pair[0] = 0;
    state_pair[1] = 0;

    if (*(int *)((char *)biped + 0xcc) != -1) {
        /* Biped is seated in a parent object */
        parent = (char *)object_get_and_verify_type(
            *(int *)((char *)biped + 0xcc), -1);

        if (*(short *)(parent + 0x64) == 1) {
            /* Parent is a vehicle */
            vehicle = (char *)object_get_and_verify_type(
                *(int *)((char *)biped + 0xcc), 2);

            FUN_001a1fb0(unit_handle);

            if ((*(unsigned char *)((char *)biped + 0x1b8) & 0x40) != 0) {
                unit_try_and_exit_seat(unit_handle);
            }

            if (*(char *)0x0032d1c8 != 0 &&
                *(float *)(vehicle + 0x38) < 0.0f &&
                (*(unsigned char *)(vehicle + 0x4) & 0x2) != 0) {
                unit_exit_seat_end(unit_handle);
            }
        } else if (*(short *)(parent + 0x64) == 0) {
            state_pair[0] = (*(unsigned char *)(parent + 0xb6) & 0x4) | 0x20;
        }
    } else {
        /* Biped is free (not seated) */
        FUN_001a4440(unit_handle);

        if ((*(unsigned char *)((char *)biped + 0xb6) & 0x4) != 0 ||
            (*(unsigned char *)(biped_tag + 0x2f4) & 0x44) == 0) {
            /* Normalize the forward vector at +0x1D4 */
            vec = (float *)((char *)biped + 0x1d4);
            *(float *)((char *)biped + 0x1dc) = 0.0f;
            if (normalize3d(vec) == 0.0f) {
                fwd_ptr = *(char **)0x0031fc3c;
                vec[0] = *(float *)(fwd_ptr);
                vec[1] = *(float *)(fwd_ptr + 4);
                vec[2] = *(float *)(fwd_ptr + 8);
            }
        }

        /* Set animation mode byte at +0x42a from anim state at +0x253 */
        anim_state = *(signed char *)((char *)biped + 0x253);
        switch (anim_state) {
        case 0:
        case 2:
        case 3:
            *(unsigned char *)((char *)biped + 0x42a) = 0;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            *(unsigned char *)((char *)biped + 0x42a) = 1;
            break;
        default:
            *(unsigned char *)((char *)biped + 0x42a) = 2;
            break;
        }

        /* Clamp velocity vector at +0x228 if magnitude squared < threshold */
        mag_sq = *(float *)((char *)biped + 0x228) *
                     *(float *)((char *)biped + 0x228) +
                 *(float *)((char *)biped + 0x22c) *
                     *(float *)((char *)biped + 0x22c) +
                 *(float *)((char *)biped + 0x230) *
                     *(float *)((char *)biped + 0x230);
        if (mag_sq < *(float *)0x00255d1c) {
            fwd_ptr = *(char **)0x0031fc38;
            *(float *)((char *)biped + 0x228) = *(float *)(fwd_ptr);
            *(float *)((char *)biped + 0x22c) = *(float *)(fwd_ptr + 4);
            *(float *)((char *)biped + 0x230) = *(float *)(fwd_ptr + 8);
        }

        /* Melee counter at +0x459 (primary) */
        if ((*(int *)((char *)biped + 0x424) & 1) == 0) {
            *(unsigned char *)((char *)biped + 0x459) = 0;
        } else if (*(signed char *)((char *)biped + 0x459) < 0x7f) {
            *(unsigned char *)((char *)biped + 0x459) =
                *(unsigned char *)((char *)biped + 0x459) + 1;
        }

        /* Melee counter at +0x45a (secondary) */
        if ((*(int *)((char *)biped + 0x424) & 2) == 0) {
            *(unsigned char *)((char *)biped + 0x45a) = 0;
        } else if (*(signed char *)((char *)biped + 0x45a) < 0x7f) {
            *(unsigned char *)((char *)biped + 0x45a) =
                *(unsigned char *)((char *)biped + 0x45a) + 1;
        }

        state_pair[0] = 0;
        state_pair[1] = *(unsigned char *)((char *)biped + 0x1b8) & 1;

        FUN_001a2800(unit_handle, "pre-turning");

        if ((*(unsigned char *)((char *)biped + 0xb6) & 0x4) == 0) {
            FUN_001a4c50(unit_handle, state_pair);
            FUN_001a2800(unit_handle, "post-turning");
        }

        FUN_001a5300(unit_handle, state_pair);
        FUN_001a2800(unit_handle, "post-moving");

        if ((*(unsigned char *)((char *)biped + 0xb6) & 0x4) != 0) {
            FUN_001a6280(unit_handle, (char *)state_pair);
        } else {
            if ((*(int *)((char *)biped + 0x424) & 1) != 0) {
                FUN_001a2900(unit_handle, (char *)state_pair);
            } else if (*(short *)((char *)biped + 0x460) != -1) {
                FUN_001a2a60(unit_handle, (char *)state_pair);
            } else if ((*(int *)((char *)biped + 0x424) & 2) != 0) {
                FUN_001a2b10(unit_handle);
            }
        }

        FUN_001a2800(unit_handle, "post-dead/air/land/slip");

        /* Melee damage timer */
        if (*(signed char *)((char *)biped + 0x45d) != 0) {
            if (*(unsigned char *)((char *)biped + 0x45d) ==
                *(unsigned char *)((char *)biped + 0x45e)) {
                unit_cause_player_melee_damage(unit_handle);
            }
            *(signed char *)((char *)biped + 0x45d) =
                *(signed char *)((char *)biped + 0x45d) - 1;
        } else {
            /* Check if weapon can initiate melee */
            if (*(int *)((char *)biped + 0x1c8) != -1 &&
                (*(signed char *)((char *)biped + 0x1b8) < 0)) {
                weapon_unit = (char *)object_get_and_verify_type(unit_handle, 3);
                weapon_handle = unit_get_weapon(unit_handle,
                    *(short *)(weapon_unit + 0x2a2));
                weapon_handle_saved = weapon_handle;

                if (!weapon_prevents_melee_attack(weapon_handle) &&
                    *(signed char *)((char *)biped + 0x2d0) == -1) {
                    unit_animation_start_action(unit_handle, 7);
                    weapon_stop_reload(weapon_handle_saved);
                    first_person_weapon_message_from_unit(unit_handle, 4);

                    result1 = weapon_get_animation_frame(
                        weapon_handle_saved, 0, 0xd, -1);
                    result2 = weapon_get_animation_frame(
                        weapon_handle_saved, 1, 0xd, -1);

                    shifted = (signed char)result1 >> 2;
                    *(signed char *)((char *)biped + 0x45d) =
                        result1 - shifted;
                    *(signed char *)((char *)biped + 0x45e) =
                        (result1 - result2) - shifted;
                }
            }
        }

        FUN_001a2440(unit_handle);
        FUN_001a1e70(unit_handle);
        FUN_001a0b30(unit_handle);
    }

    /* Post-section: animation state update */
    anim_result = FUN_001b0d90(unit_handle, (char *)state_pair);
    if (anim_result == 1) {
        FUN_001a2290(unit_handle);
    }

    /* Suspension tick counter at +0x6C */
    if ((*(unsigned char *)((char *)biped + 0xb6) & 0x4) != 0 &&
        (*(unsigned char *)((char *)biped + 0x4) & 0x20) != 0) {
        *(short *)((char *)biped + 0x6c) =
            *(short *)((char *)biped + 0x6c) + 1;
    } else {
        *(short *)((char *)biped + 0x6c) = 0;
    }

    FUN_001a2800(unit_handle, "post-update");

    /* Debug trace exit */
    if (*(char *)0x00449ef1 != 0 && *(char *)0x0032d1d8 != 0) {
        profile_exit_private((void *)0x0032d1d0);
    }

    return 1;
}
