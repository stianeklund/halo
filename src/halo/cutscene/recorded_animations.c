/* Recorded animation thread system — plays back scripted unit animations
 * for cinematics and AI scripted sequences. */

/* FUN_00094020 @ 0x00094020
 *
 * Unpack a 0x40-byte byte-swap record through FUN_00093780, then copy the
 * following 12-byte vector from *cursor into out_vec and advance the cursor
 * by 0xc. Four cdecl stack args: [ebp+8]=out_vec, [ebp+c]=record,
 * [ebp+10]=cursor, [ebp+14]=count. No @<reg>.
 *
 * Callees (cdecl, ported): FUN_00093780.
 */
void FUN_00094020(void *out_vec, void *record, int *cursor, unsigned char count)
{
  int *src;
  int *dst;

  FUN_00093780(record, cursor, count);
  src = (int *)*cursor;
  dst = (int *)out_vec;
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  *cursor = *cursor + 0xc;
}

/* FUN_00094060 @ 0x00094060
 *
 * Copy a raw 0x40-byte record from *cursor via REP MOVSD, advance 0x40, then
 * copy the following 12-byte vector into out_vec and advance 0xc. Three cdecl
 * stack args: [ebp+8]=out_vec, [ebp+c]=record, [ebp+10]=cursor. No call, no
 * @<reg>.
 */
void FUN_00094060(void *out_vec, void *record, int *cursor)
{
  int *src;
  int *dst;

  memcpy(record, (void *)*cursor, 0x40);
  *cursor = *cursor + 0x40;
  src = (int *)*cursor;
  dst = (int *)out_vec;
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  *cursor = *cursor + 0xc;
}

/* FUN_000940a0 @ 0x000940a0
 *
 * Dispatch recorded-animation playback-v0 events while *ticks covers the
 * next event's encoded time_delta. Four cdecl args; [ebp+8] is unread
 * except as the first handler argument (same vtable-slot shape as
 * recorded_animation_apply_event_stream_v1). Header byte bits 1:0 select
 * the time_delta encoding (0/1/byte/word); bits 7:2 index the apply
 * table at 0x2ee960. Returns AL=0 only when the current event is type 1
 * (bits 7:2 == 1) and *ticks equals its time_delta.
 */
char FUN_000940a0(void *unused, char *control, int *ticks, int *playback_stream)
{
  unsigned char *header;
  unsigned short time_delta;
  unsigned int header_size;
  void (*handler)(void *, char *, unsigned char *, int *);

  if (control == NULL) {
    display_assert("control",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x113, 1);
    system_exit(-1);
  }
  if (ticks == NULL) {
    display_assert("ticks",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x114, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert("playback_stream",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x115, 1);
    system_exit(-1);
  }
  if (*playback_stream == 0) {
    display_assert("*playback_stream",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c",
                   0x116, 1);
    system_exit(-1);
  }

  for (;;) {
    header = (unsigned char *)*playback_stream;
    switch (*header & 3) {
    case 0:
      time_delta = 0;
      header_size = 1;
      break;
    case 1:
      time_delta = 1;
      header_size = 1;
      break;
    case 2:
      time_delta = header[1];
      header_size = 2;
      if (time_delta <= 1 || time_delta > 0xff) {
        display_assert(
          "time_delta>1&&time_delta<=UNSIGNED_CHAR_MAX",
          "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c", 0x12d,
          1);
        system_exit(-1);
      }
      break;
    case 3:
      time_delta = *(unsigned short *)(header + 1);
      header_size = 3;
      if (time_delta <= 0xff) {
        display_assert(
          "time_delta>UNSIGNED_CHAR_MAX",
          "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c", 0x132,
          1);
        system_exit(-1);
      }
      break;
    default:
      display_assert(
        "!\"unreachable\"",
        "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c", 0x135, 1);
      system_exit(-1);
    }

    if (*ticks < (int)time_delta || (*header & 0xfc) == 4) {
      if ((*header & 0xfc) == 4 && *ticks == (int)time_delta) {
        return 0;
      }
      return 1;
    }

    *playback_stream = *playback_stream + (int)header_size;
    if ((*header & 0xfc) >= 0x5c) {
      display_assert(
        "header->event_type<NUMBEROF(apply_funcs)",
        "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback.c", 0x13b, 1);
      system_exit(-1);
    }
    handler = ((
      void (**)(void *, char *, unsigned char *, int *))0x2ee960)[*header >> 2];
    if (handler != NULL) {
      handler(unused, control, header, playback_stream);
    }
    *ticks = *ticks - (int)time_delta;
  }
}

/* FUN_000942a0 @ 0x000942a0
 *
 * Playback-v1 event: animation_state_set. Four asserts against
 * recorded_animation_playback_v1.c:0x19, then copy anim_event_v1+4 into
 * *control and advance *playback_stream by 6. Three cdecl stack args:
 * [ebp+8]=control, [ebp+c]=anim_event_v1, [ebp+10]=playback_stream.
 */
void FUN_000942a0(char *control, short *anim_event_v1, int *playback_stream)
{
  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x19, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x19, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 2) {
    display_assert(
      "anim_event_v1->type==_playback_v1_animation_state_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x19, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x19, 1);
    system_exit(-1);
  }
  *control = *((char *)anim_event_v1 + 4);
  *playback_stream = *playback_stream + 6;
}

/* FUN_00094350 @ 0x00094350
 *
 * Playback-v1 event: aiming_speed_set. Same four asserts as FUN_000942a0
 * against recorded_animation_playback_v1.c:0x1a, type==3, then store
 * anim_event_v1+4 into control[1] and advance *playback_stream by 6.
 */
void FUN_00094350(char *control, short *anim_event_v1, int *playback_stream)
{
  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x1a, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1a, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 3) {
    display_assert(
      "anim_event_v1->type==_playback_v1_aiming_speed_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1a, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1a, 1);
    system_exit(-1);
  }
  control[1] = *((char *)anim_event_v1 + 4);
  *playback_stream = *playback_stream + 6;
}

/* FUN_00094400 @ 0x00094400
 *
 * Playback-v1 event: control_flags_set. Same four asserts at
 * recorded_animation_playback_v1.c:0x1b, type==4, then store the word at
 * anim_event_v1+4 into control+2 and advance *playback_stream by 6.
 */
void FUN_00094400(char *control, short *anim_event_v1, int *playback_stream)
{
  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x1b, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1b, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 4) {
    display_assert(
      "anim_event_v1->type==_playback_v1_control_flags_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1b, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1b, 1);
    system_exit(-1);
  }
  *(short *)(control + 2) = anim_event_v1[2];
  *playback_stream = *playback_stream + 6;
}

/* FUN_000944b0 @ 0x000944b0
 *
 * Playback-v1 event: weapon_index_set. Same four asserts at
 * recorded_animation_playback_v1.c:0x1c, type==5, then store the word at
 * anim_event_v1+4 into control+4 and advance *playback_stream by 6.
 */
void FUN_000944b0(char *control, short *anim_event_v1, int *playback_stream)
{
  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x1c, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1c, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 5) {
    display_assert(
      "anim_event_v1->type==_playback_v1_weapon_index_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1c, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x1c, 1);
    system_exit(-1);
  }
  *(short *)(control + 4) = anim_event_v1[2];
  *playback_stream = *playback_stream + 6;
}

/* FUN_00094560 @ 0x00094560
 *
 * Playback-v1 event: throttle_set. Four asserts on consecutive lines
 * 0x21-0x24 of recorded_animation_playback_v1.c, type==6, then copy two
 * dwords from anim_event_v1+4/+8 into control+0xc/+0x10, zero control+0x14,
 * and advance *playback_stream by 0xc.
 */
void FUN_00094560(char *control, short *anim_event_v1, int *playback_stream)
{
  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x21, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x22, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 6) {
    display_assert(
      "anim_event_v1->type==_playback_v1_throttle_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x23, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x24, 1);
    system_exit(-1);
  }
  *(int *)(control + 0xc) = *(int *)(anim_event_v1 + 2);
  *(int *)(control + 0x10) = *(int *)(anim_event_v1 + 4);
  *(int *)(control + 0x14) = 0;
  *playback_stream = *playback_stream + 0xc;
}

/* apply_facing_vector @ 0x00094620
 *
 * Playback-v1 event: facing_vector_set. Four asserts at
 * recorded_animation_playback_v1.c:0x2c, type==9, then copy the 12-byte
 * vector at anim_event_v1+4 into control+0x1c and advance *playback_stream
 * by 0x10.
 */
void apply_facing_vector(char *control, short *anim_event_v1,
                         int *playback_stream)
{
  int *src;
  int *dst;

  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x2c, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2c, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 9) {
    display_assert(
      "anim_event_v1->type==_playback_v1_facing_vector_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2c, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2c, 1);
    system_exit(-1);
  }
  src = (int *)((char *)anim_event_v1 + 4);
  dst = (int *)(control + 0x1c);
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  *playback_stream = *playback_stream + 0x10;
}

/* apply_aiming_vector @ 0x000946e0
 *
 * Playback-v1 event: aiming_vector_set. Four asserts at
 * recorded_animation_playback_v1.c:0x2d, type==10, then copy the 12-byte
 * vector at anim_event_v1+4 into control+0x28 and advance *playback_stream
 * by 0x10.
 */
void apply_aiming_vector(char *control, short *anim_event_v1,
                         int *playback_stream)
{
  int *src;
  int *dst;

  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x2d, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2d, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 10) {
    display_assert(
      "anim_event_v1->type==_playback_v1_aiming_vector_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2d, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2d, 1);
    system_exit(-1);
  }
  src = (int *)((char *)anim_event_v1 + 4);
  dst = (int *)(control + 0x28);
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  *playback_stream = *playback_stream + 0x10;
}

/* apply_looking_vector @ 0x000947a0
 *
 * Playback-v1 event: looking_vector_set. Four asserts at
 * recorded_animation_playback_v1.c:0x2e, type==0xb, then copy the 12-byte
 * vector at anim_event_v1+4 into control+0x34 and advance *playback_stream
 * by 0x10.
 */
void apply_looking_vector(char *control, short *anim_event_v1,
                          int *playback_stream)
{
  int *src;
  int *dst;

  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x2e, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2e, 1);
    system_exit(-1);
  }
  if (*anim_event_v1 != 0xb) {
    display_assert(
      "anim_event_v1->type==_playback_v1_looking_vector_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2e, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x2e, 1);
    system_exit(-1);
  }
  src = (int *)((char *)anim_event_v1 + 4);
  dst = (int *)(control + 0x34);
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  *playback_stream = *playback_stream + 0x10;
}

/* apply_angle_vector @ 0x00094860
 *
 * Playback-v1 event: facing/aiming/looking angles. Asserts on lines
 * 0x38-0x3b, type in [0x10, 0x16], convert the angle pair at
 * anim_event_v1+4 through angles_to_vector, then copy the resulting
 * vector into facing (0x1c) unless type==0x15, aiming (0x28) unless
 * type==0x14, looking (0x34) unless type==0x13, and advance the stream
 * by 0xc.
 */
void apply_angle_vector(char *control, short *anim_event_v1,
                        int *playback_stream)
{
  float local_vec[3];
  int *dst;
  int *src;
  short event_type;

  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x38, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x39, 1);
    system_exit(-1);
  }
  event_type = *anim_event_v1;
  if (event_type < 0x10 || 0x16 < event_type) {
    display_assert(
      "anim_event_v1->type>=_playback_v1_facing_angles_set && "
      "anim_event_v1->type<=_playback_v1_facing_aiming_looking_angles_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x3a, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x3b, 1);
    system_exit(-1);
  }
  angles_to_vector(local_vec, (float *)(anim_event_v1 + 2));
  src = (int *)local_vec;
  if (*anim_event_v1 != 0x15) {
    dst = (int *)(control + 0x1c);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  if (*anim_event_v1 != 0x14) {
    dst = (int *)(control + 0x28);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  if (*anim_event_v1 != 0x13) {
    dst = (int *)(control + 0x34);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  *playback_stream = *playback_stream + 0xc;
}

/* apply_multi_vector @ 0x00094970
 *
 * Playback-v1 event: facing/aiming/looking vector set. Asserts on lines
 * 0x56-0x59, type in [0xc, 0xf], then copy the 12-byte vector at
 * anim_event_v1+4 into facing (0x1c) unless type==0xe, aiming (0x28)
 * unless type==0xd, looking (0x34) unless type==0xc, and advance the
 * stream by 0x10.
 */
void apply_multi_vector(char *control, short *anim_event_v1,
                        int *playback_stream)
{
  int *src;
  int *dst;
  short event_type;

  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0x56, 1);
    system_exit(-1);
  }
  if (anim_event_v1 == NULL) {
    display_assert(
      "anim_event_v1",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x57, 1);
    system_exit(-1);
  }
  event_type = *anim_event_v1;
  if (event_type < 0xc || 0xf < event_type) {
    display_assert(
      "anim_event_v1->type>=_playback_v1_facing_aiming_vector_set&&"
      "anim_event_v1->type<=_playback_v1_facing_aiming_looking_vector_set",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x58, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0x59, 1);
    system_exit(-1);
  }
  src = (int *)((char *)anim_event_v1 + 4);
  if (*anim_event_v1 != 0xe) {
    dst = (int *)(control + 0x1c);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  if (*anim_event_v1 != 0xd) {
    dst = (int *)(control + 0x28);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  if (*anim_event_v1 != 0xc) {
    dst = (int *)(control + 0x34);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
  }
  *playback_stream = *playback_stream + 0x10;
}

/* FUN_00094a70 @ 0x00094a70
 *
 * Thin cdecl wrapper: FUN_00093780(record, cursor, count). Four stack
 * args; [ebp+8] is unread (same vtable slot shape as FUN_00094020).
 */
void FUN_00094a70(void *out_vec, void *record, int *cursor, unsigned char count)
{
  FUN_00093780(record, cursor, count);
}

/* recorded_animation_apply_event_stream_v1 @ 0x00094a90
 *
 * Dispatch playback-v1 events while *ticks covers the next event's
 * duration word. Four cdecl args; [ebp+8] is unread. Handler table
 * at 0x2eea70 is indexed by signed event type; NULL slots skip a
 * 4-byte header. Returns AL=0 only when the current event is type 1
 * and *ticks equals its duration.
 */
char recorded_animation_apply_event_stream_v1(void *unused, char *control,
                                              int *ticks, int *playback_stream)
{
  short *event;
  short event_type;
  void (*handler)(char *, short *, int *);

  if (control == NULL) {
    display_assert(
      "control", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0xa2, 1);
    system_exit(-1);
  }
  if (ticks == NULL) {
    display_assert(
      "ticks", "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c",
      0xa3, 1);
    system_exit(-1);
  }
  if (playback_stream == NULL) {
    display_assert(
      "playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0xa4, 1);
    system_exit(-1);
  }
  if (*playback_stream == 0) {
    display_assert(
      "*playback_stream",
      "c:\\halo\\SOURCE\\cutscene\\recorded_animation_playback_v1.c", 0xa5, 1);
    system_exit(-1);
  }

  event = (short *)*playback_stream;
  if (*ticks >= (int)(unsigned short)event[1]) {
    do {
      event_type = event[0];
      if (event_type == 1) {
        goto check_end;
      }
      handler = ((void (**)(char *, short *, int *))0x2eea70)[(int)event_type];
      if (handler != NULL) {
        handler(control, event, playback_stream);
      } else {
        *playback_stream = (int)(event + 2);
      }
      *ticks = *ticks - (int)(unsigned short)event[1];
      event = (short *)*playback_stream;
    } while (*ticks >= (int)(unsigned short)event[1]);
  }
  if (*event == 1) {
  check_end:
    if (*ticks == (int)(unsigned short)event[1]) {
      return 0;
    }
  }
  return 1;
}

/* Allocate animation thread data array and debug tracking buffer. */
void recorded_animations_initialize(void)
{
  *(void **)0x44df04 = game_state_data_new("recorded animations", 0x40, 100);
  if (!*(void **)0x44df04) {
    display_assert("animation_threads",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x6c,
                   1);
    system_exit(-1);
  }

  *(void **)0x44df0c = ((void *(*)(int, int, const char *, int))0x8ee60)(
    0x400, 0, "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x6f);
  if (!*(void **)0x44df0c) {
    display_assert("animation_threads_debug",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x70,
                   1);
    system_exit(-1);
  }
}

/* Free the debug tracking buffer. */
void recorded_animations_dispose(void)
{
  if (*(void **)0x44df0c != 0) {
    ((void (*)(void *, const char *, int))0x8ef70)(
      *(void **)0x44df0c, "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c",
      0x7b);
    *(void **)0x44df0c = 0;
  }
}

/* Mark animation thread data as invalid for old map disposal. */
void recorded_animations_dispose_from_old_map(void)
{
  data_make_invalid(*(void **)0x44df04);
}

/* recorded_animations_clear_debug_storage @ 0x00094c70
 *
 * Assert animation_threads_debug at 0x44df0c, then csmemset 0x400 bytes.
 * Frameless in the original.
 */
void recorded_animations_clear_debug_storage(void)
{
  if (*(void **)0x44df0c == NULL) {
    display_assert("animation_threads_debug",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x99,
                   1);
    system_exit(-1);
  }
  csmemset(*(void **)0x44df0c, 0, 0x400);
}

/* Advance all active recorded animation threads by one tick.
 *
 * For each allocated thread, either:
 *   - dispose it (object gone, or finished flag set) by clearing debug slot,
 *     restoring the unit's animation-driven flags, and deleting the datum;
 *   - otherwise, tick its per-type event stream via vtable dispatch, sanity
 *     check against the recorded debug state, and apply the resulting frame
 *     to the unit. The vtable returns "still has events" — the finished bit
 *     is set when the vtable reports zero (stream exhausted).
 */
void recorded_animations_update(void)
{
  data_iter_t iter;
  char *thread;
  char *dbg_slot;
  char stream_active;
  int *relative_ticks;
  uint16_t flags;
  int dbg_index;
  void **vtable;
  int stream_delta;
  scenario_t *scenario;
  char *anim_def;
  char *msg;

  data_iterator_new(&iter, *(data_t **)0x44df04);
  thread = (char *)data_iterator_next(&iter);
  while (thread != NULL) {
    if (object_try_and_get_and_verify_type(*(int *)(thread + 4), 3) == NULL) {
      datum_delete(*(data_t **)0x44df04, iter.datum_handle);
    } else {
      flags = *(uint16_t *)(thread + 0xa);
      if ((flags & 1) == 0) {
        /* Active thread: tick the per-type event stream via vtable. The
         * callback returns nonzero while events remain in the stream and
         * zero once the stream is exhausted. */
        *(int16_t *)(thread + 8) = *(int16_t *)(thread + 8) - 1;
        relative_ticks = (int *)(thread + 0xc);
        vtable = (void **)((void **)0x2eebb0)[*(int16_t *)(thread + 0x60)];
        stream_active = ((char (*)(char *, char *, int *, int *))vtable[1])(
                          thread + 0x54, thread + 0x14, relative_ticks,
                          (int *)(thread + 0x10)) ?
                          1 :
                          0;
        if (*relative_ticks < 0) {
          display_assert("thread->relative_ticks>=0",
                         "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c",
                         0x15b, 1);
          system_exit(-1);
        }
        dbg_index = (iter.datum_handle & 0xffff) * 0x10;
        dbg_slot = (char *)(dbg_index + *(int *)0x44df0c);
        if (*dbg_slot != 0) {
          stream_delta = *(int *)(thread + 0x10) - *(int *)(dbg_slot + 4);
          /* Assert holds when stream_delta is below the recorded length, or
           * exactly at the end while events are still being produced. */
          if (!(stream_delta < *(int *)(dbg_slot + 8) ||
                (stream_delta == *(int *)(dbg_slot + 8) &&
                 stream_active != 0))) {
            display_assert(
              "thread->event_stream-thread_debug->event_stream_start<"
              "thread_debug->stream_length||(thread->event_stream-thread_debug"
              "->event_stream_start==thread_debug->stream_length&&finished)",
              "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x162, 1);
            system_exit(-1);
          }
        }
        *relative_ticks = *relative_ticks + 1;
        ((void (*)(int, char *))0x1af990)(*(int *)(thread + 4), thread + 0x14);
        /* Stream exhausted → set finished bit so next tick takes the
         * cleanup path. Stream still active → keep the thread alive. */
        if (stream_active != 0)
          *(uint8_t *)(thread + 0xa) = *(uint8_t *)(thread + 0xa) & 0xfe;
        else
          *(uint8_t *)(thread + 0xa) = *(uint8_t *)(thread + 0xa) | 1;
      } else {
        /* Finished thread: clean up and delete. */
        dbg_index = (iter.datum_handle & 0xffff) * 0x10;
        dbg_slot = (char *)(dbg_index + *(int *)0x44df0c);
        if (*dbg_slot != 0 && (flags & 2) == 0 &&
            *(int16_t *)(thread + 8) != 0) {
          scenario = global_scenario_get();
          anim_def = (char *)tag_block_get_element(
            (char *)scenario + 0x36c, *(int16_t *)(dbg_slot + 0xc), 0x40);
          msg = csprintf((char *)0x5ab100, "animation %s appears corrupt",
                         anim_def);
          display_assert(
            msg, "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x175, 1);
          system_exit(-1);
        }
        *dbg_slot = 0;
        ((void (*)(int, int))0x1a9a50)(*(int *)(thread + 4),
                                       (*(uint8_t *)(thread + 0xa) >> 2) & 1);
        ((void (*)(int, int))0x1a9a90)(*(int *)(thread + 4), 0);
        ((void (*)(int, int))0x1adf10)(*(int *)(thread + 4), 0);
        ((void (*)(int, int))0x13ff50)(*(int *)(thread + 4), 1);
        if ((*(uint8_t *)(thread + 0xa) & 8) != 0)
          ((void (*)(int))0xc99e0)(*(int *)(thread + 4));
        if ((*(uint8_t *)(thread + 0xa) & 0x10) != 0)
          ((void (*)(int, int))0x1b5610)(*(int *)(thread + 4), 1);
        datum_delete(*(data_t **)0x44df04, iter.datum_handle);
      }
    }
    thread = (char *)data_iterator_next(&iter);
  }
}

/* Return nonzero while an active recorded animation controls this unit. */
char recorded_animation_controlling_unit(int unit_handle)
{
  data_iter_t iter;
  char *thread;
  char result;

  result = 0;
  data_iterator_new(&iter, *(data_t **)0x44df04);
  thread = (char *)data_iterator_next(&iter);
  while (thread != NULL) {
    if (*(int *)(thread + 4) == unit_handle &&
        (*(uint8_t *)(thread + 0xa) & 1) == 0) {
      result = 1;
      break;
    }
    thread = (char *)data_iterator_next(&iter);
  }
  return result;
}

/* render_debug_recording @ 0x000950b0
 *
 * Overlay dump of active recorded-animation threads when the debug flag
 * at 0x44df08 is set. Builds a tab-separated string (recording name,
 * ticks left, object name) in a 0x2800-byte stack buffer, then draws it
 * with tab stops 200/300. No stack args, no @<reg>. Frame uses _chkstk
 * 0x2818.
 */
void render_debug_recording(void)
{
  char buf[0x2800];
  data_iter_t iter;
  void *object_name;
  short tab_stops[2];
  short i;
  int len;
  int written;
  char *thread;
  void *object;
  char *dbg_slot;
  char *rec_name;

  if (*(char *)0x44df08 == 0) {
    return;
  }

  i = 0;
  len = 0;
  tab_stops[0] = 0xc8;
  tab_stops[1] = 0x12c;
  if (*(short *)0x2eebc0 > 0) {
    do {
      written = crt_sprintf(buf + (short)len, "|n");
      len = len + written;
      i = i + 1;
    } while (i < *(short *)0x2eebc0);
  }
  written =
    crt_sprintf(buf + (short)len, "recording name|tticks left|tobject name");
  len = len + written;

  data_iterator_new(&iter, *(data_t **)0x44df04);
  thread = (char *)data_iterator_next(&iter);
  while (thread != NULL) {
    object = object_try_and_get_and_verify_type(*(int *)(thread + 4), -1);
    dbg_slot = (char *)((iter.datum_handle & 0xffff) * 0x10 + *(int *)0x44df0c);
    if ((*(unsigned char *)(thread + 0xa) & 1) == 0 && object != NULL &&
        *(short *)((char *)object + 0x6a) != -1) {
      object_name =
        tag_block_get_element((char *)global_scenario_get() + 0x204,
                              (int)*(short *)((char *)object + 0x6a), 0x24);
      rec_name = (char *)0x25b724;
      if (*dbg_slot != 0) {
        rec_name =
          (char *)tag_block_get_element((char *)global_scenario_get() + 0x36c,
                                        (int)*(short *)(dbg_slot + 0xc), 0x40);
      }
      written = crt_sprintf(buf + (short)len, "|n%s|t", rec_name);
      len = len + written;
      written = crt_sprintf(buf + (short)len, "%d|t",
                            (unsigned int)*(unsigned short *)(thread + 8));
      len = len + written;
      written = crt_sprintf(buf + (short)len, "%s", object_name);
      len = len + written;
    }
    thread = (char *)data_iterator_next(&iter);
  }
  buf[0x400] = 0;
  draw_string_set_tab_stops(tab_stops, 2);
  FUN_00189c40(1, buf);
  draw_string_set_tab_stops(tab_stops, 0);
}

/* Clear animation threads and zero the debug buffer for a new map. */
void recorded_animations_initialize_for_new_map(void)
{
  ((void (*)(void *))0x119b20)(*(void **)0x44df04);
  if (!*(void **)0x44df0c) {
    display_assert("animation_threads_debug",
                   "c:\\halo\\SOURCE\\cutscene\\recorded_animations.c", 0x99,
                   1);
    system_exit(-1);
  }
  csmemset(*(void **)0x44df0c, 0, 0x400);
}

/* Start a recorded animation on a unit. Thin forwarder to the shared worker
 * with an empty flag set; the sibling at 0x95660
 * (recorded_animation_play_and_delete) is the same body with flags = 8. The
 * worker takes the unit handle in EAX and returns success in AL. */
char recorded_animation_play(int actor, short anim_idx)
{
  return recorded_animation_play_internal(actor, anim_idx, 0);
}

char recorded_animation_play_and_delete(int actor, short anim_idx)
{
  return recorded_animation_play_internal(actor, anim_idx, 8);
}

/* FUN_000956e0 @ 0x000956e0
 *
 * Load unit object (type mask 0x100 / 'unit'), look up its 'ctrl' tag,
 * call FUN_00097080(handle, event+0x28), then OR animation-driven bits
 * from event+0x30 into object+0x1c4 and store (event+0x34)-1 at +0x1c8.
 */
void FUN_000956e0(int object_handle, char *event)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x100);
  tag_get(0x6374726c, *object);
  FUN_00097080(object_handle, event + 0x28);
  if ((*(unsigned char *)(event + 0x30) & 1) != 0) {
    object[0x71] = object[0x71] | 1;
  }
  if ((*(unsigned char *)(event + 0x30) & 0x10) != 0) {
    object[0x71] = object[0x71] | 2;
  }
  *(short *)((char *)object + 0x1c8) = (short)(*(short *)(event + 0x34) - 1);
}

/* FUN_00095750 @ 0x00095750
 *
 * Load unit object (type mask 0x100) and look up its 'ctrl' tag. Returns 1
 * in AL. One cdecl stack arg.
 */
char FUN_00095750(int object_handle)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x100);
  tag_get(0x6374726c, *object);
  return 1;
}

/* FUN_00095790 @ 0x00095790
 *
 * Twin of FUN_00095750: load unit object and look up its 'ctrl' tag.
 * Returns 1 in AL.
 */
char FUN_00095790(int object_handle)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x100);
  tag_get(0x6374726c, *object);
  return 1;
}

/* FUN_000959b0 @ 0x000959b0
 *
 * Load machine object (type mask 0x200), look up its 'life' tag, call
 * FUN_00097080(handle, event+0x28), then copy event+0x30 (12 bytes) to
 * object+0x1c4 and event+0x3c/+0x40/+0x44 to object+0x1d0/+0x1d4/+0x1d8.
 */
void FUN_000959b0(int object_handle, char *event)
{
  int *object;
  int *src;
  int *dst;

  object = (int *)object_get_and_verify_type(object_handle, 0x200);
  tag_get(0x6c696669, *object);
  FUN_00097080(object_handle, event + 0x28);
  src = (int *)(event + 0x30);
  dst = (int *)((char *)object + 0x1c4);
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  *(int *)((char *)object + 0x1d0) = *(int *)(event + 0x3c);
  *(int *)((char *)object + 0x1d4) = *(int *)(event + 0x40);
  *(int *)((char *)object + 0x1d8) = *(int *)(event + 0x44);
}

/* FUN_00095a20 @ 0x00095a20
 *
 * Load machine object (type mask 0x200) and look up its 'life' tag.
 * Returns 1 in AL.
 */
char FUN_00095a20(int object_handle)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x200);
  tag_get(0x6c696669, *object);
  return 1;
}

/* FUN_00095a60 @ 0x00095a60
 *
 * Twin of FUN_00095a20: load machine object and look up its 'life' tag.
 * Returns 1 in AL.
 */
char FUN_00095a60(int object_handle)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x200);
  tag_get(0x6c696669, *object);
  return 1;
}

/* FUN_00095ad0 @ 0x00095ad0
 *
 * Load object type mask 0x80, call FUN_00097080(handle, event+0x28), then
 * OR event+0x30 bits 1/2/4/8 into object+0x1c4.
 */
void FUN_00095ad0(int object_handle, char *event)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x80);
  FUN_00097080(object_handle, event + 0x28);
  if ((*(unsigned char *)(event + 0x30) & 1) != 0) {
    object[0x71] = object[0x71] | 1;
  }
  if ((*(unsigned char *)(event + 0x30) & 2) != 0) {
    object[0x71] = object[0x71] | 2;
  }
  if ((*(unsigned char *)(event + 0x30) & 4) != 0) {
    object[0x71] = object[0x71] | 4;
  }
  if ((*(unsigned char *)(event + 0x30) & 8) != 0) {
    object[0x71] = object[0x71] | 8;
  }
}

/* FUN_00095b50 @ 0x00095b50
 *
 * Load object type mask 0x80, look up 'mach' tag, set object+4 bit
 * 0x2000, then set/clear 0x4000 and 0x8000 from tag+0x292 bit 4.
 * Returns 1 in AL.
 */
char FUN_00095b50(int object_handle)
{
  int *object;
  char *tag;
  int flags;

  object = (int *)object_get_and_verify_type(object_handle, 0x80);
  tag = (char *)tag_get(0x6d616368, object[0]);
  flags = object[1] | 0x2000;
  object[1] = flags;
  if ((tag[0x292] & 4) != 0) {
    flags = flags | 0x4000;
  } else {
    flags = flags & ~0x4000;
  }
  object[1] = flags;
  flags = object[1];
  if ((tag[0x292] & 4) != 0) {
    object[1] = flags | 0x8000;
  } else {
    object[1] = flags & ~0x8000;
  }
  return 1;
}

/* FUN_00095be0 @ 0x00095be0
 *
 * Load object type mask 0x80 and look up its 'mach' tag. Two cdecl
 * args; [ebp+c] is unread (callers pass a second handle).
 */
void FUN_00095be0(int object_handle, int unit_handle)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x80);
  tag_get(0x6d616368, *object);
}

/* FUN_00095c10 @ 0x00095c10
 *
 * Load object type mask 0x80, look up its 'mach' tag, and if bit 8 of
 * the machine flags byte at object+0x1c4 is set, call
 * FUN_00097040(handle, 1.0f). One cdecl stack arg: [ebp+8]=object_handle.
 * No @<reg>.
 */
void FUN_00095c10(int object_handle)
{
  int *object;

  object = (int *)object_get_and_verify_type(object_handle, 0x80);
  tag_get(0x6d616368, *object);
  if ((*(unsigned char *)((char *)object + 0x1c4) & 8) != 0) {
    FUN_00097040(object_handle, 1.0f);
  }
}
