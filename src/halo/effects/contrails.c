void contrails_initialize_for_new_map(void)
{
  data_delete_all(contrail_data);
  data_delete_all(contrail_point_data);
}

void contrails_dispose_from_old_map(void)
{
  data_make_invalid(contrail_point_data);
  data_make_invalid(contrail_data);
}

void contrails_dispose(void)
{
  if (contrail_point_data)
    contrail_point_data = 0;
  if (contrail_data)
    contrail_data = 0;
}

/* 0x978f0 — contrail_delete. Walks the 4 point chains attached to a contrail
 * datum (starting at datum+0x34), deletes every point datum in each chain,
 * then deletes the contrail datum itself. */
void contrail_delete(int contrail_handle)
{
  char *datum;
  int *chain_ptr;
  int point_handle;
  int next_handle;
  char *point_datum;
  int i;

  datum = (char *)datum_get(contrail_data, contrail_handle);
  chain_ptr = (int *)(datum + 0x34);

  for (i = 4; i != 0; i--) {
    point_handle = *chain_ptr;
    while (point_handle != -1) {
      point_datum = (char *)datum_get(contrail_point_data, point_handle);
      next_handle = *(int *)(point_datum + 0x34);
      datum_delete(contrail_point_data, point_handle);
      point_handle = next_handle;
    }
    chain_ptr++;
  }

  datum_delete(contrail_data, contrail_handle);
}

void contrails_reconnect_to_structure_bsp(void)
{
  int contrail_index;
  char *contrail_datum;
  int *chain_ptr;
  int local_c;
  int point_index;
  char *point_datum;

  for (contrail_index = data_next_index(contrail_data, -1);
       contrail_index != -1;
       contrail_index = data_next_index(contrail_data, contrail_index)) {
    contrail_datum = (char *)datum_get(contrail_data, contrail_index);
    tag_get(0x636f6e74, *(int *)(contrail_datum + 4));
    chain_ptr = (int *)(contrail_datum + 0x34);
    local_c = 4;
    do {
      for (point_index = *chain_ptr; point_index != -1;
           point_index = *(int *)(point_datum + 0x34)) {
        point_datum = (char *)datum_get(contrail_point_data, point_index);
        if (*(short *)(point_datum + 0x18) != -1) {
          scenario_location_from_point(point_datum + 0x14, point_datum + 0x1c);
        }
      }
      chain_ptr++;
      local_c--;
    } while (local_c != 0);
  }
}

/*
 * FUN_00097a50 (0x97a50): contrail tick counter — given a contrail handle and
 * delta_time, computes how many full emission periods fit in delta_time and
 * updates the per-datum time accumulator (datum+0x20).
 *
 * datum+0x20 holds the time remaining until the next emission tick.  Each
 * full period consumed decrements that remaining time by datum[0x20] and
 * resets it to the computed period (1.0f / render_freq), incrementing the
 * returned count.  Any leftover fraction is subtracted from datum[0x20] so
 * the accumulator carries forward correctly to the next call.
 *
 * If the tag flag byte (tag[2] & 0x1) is set the render frequency is scaled
 * by datum[0x10] before the period is calculated.
 *
 * Returns the number of complete emission periods that elapsed.
 * Called by contrail_set_state_for_object and contrails_update.
 */
int16_t FUN_00097a50(int contrail_handle, float delta_time)
{
  char *datum;
  char *tag;
  float render_freq;
  float period;
  int16_t count;

  datum = (char *)datum_get(contrail_data, contrail_handle);
  tag = (char *)tag_get(0x636f6e74, *(int *)(datum + 4));

  render_freq = *(float *)(tag + 4);
  if (*(uint8_t *)(tag + 2) & 1) {
    render_freq *= *(float *)(datum + 0x10);
  }
  period = 1.0f / render_freq;

  count = 0;

  if (delta_time <= 0.0f) {
    return count;
  }

  while (delta_time >= *(float *)(datum + 0x20)) {
    delta_time -= *(float *)(datum + 0x20);
    *(float *)(datum + 0x20) = period;
    count++;
    if (delta_time <= 0.0f)
      return count;
  }

  *(float *)(datum + 0x20) -= delta_time;
  return count;
}

/* 0x97c80 — Draw a random short in [min, max) using the module-local LCG seed.
 */
int16_t local_random_range(int16_t min, int16_t max)
{
  return random_range(random_math_get_local_seed_address(), min, max);
}

/* 0x97ca0 — Generate a random direction within a cone using the module-local
 * seed. Wraps random_direction3d with the local random seed. */
void local_random_vector_in_cone3d(float *forward, float zero, float angle,
                                   float *result)
{
  random_direction3d((int *)random_math_get_local_seed_address(), forward, zero,
                     angle, result);
}

/* 0x97cd0 — compute random value in a flag-adjusted range.
 * base = range_min (scaled by datum_scale if flag bit set).
 * range = (range_max - range_min) (scaled by datum_scale if next bit set).
 * Returns random_real_range(0, range) + base. */
float contrail_scale_random_value(float datum_scale, float range_min,
                                  float range_max, unsigned int flags,
                                  int bit_index)
{
  float base;
  float range;
  unsigned int *seed;

  base = range_min;
  if (flags & (1 << (bit_index & 0x1f)))
    base = datum_scale * range_min;
  range = range_max - range_min;
  if (flags & (1 << ((bit_index + 1) & 0x1f)))
    range *= datum_scale;
  seed = random_math_get_local_seed_address();
  return random_real_range((int *)seed, 0.0f, range) + base;
}

void contrails_initialize(void)
{
  contrail_data = game_state_data_new("contrail", 0x100, 0x44);
  contrail_point_data = game_state_data_new("contrail point", 0x400, 0x38);
  if (contrail_data) {
    if (contrail_point_data)
      return;
    contrail_data = 0;
  } else {
    if (contrail_point_data) {
      contrail_point_data = 0;
      error(0, "couldn't allocate contrail globals");
      return;
    }
  }
  error(0, "couldn't allocate contrail globals");
}

/* 0x97db0 — contrail_advance_bitmap_frame. Advances the frame counter for
 * the contrail's bitmap animation sequence. If the current sequence is
 * complete or invalid, picks a new random sequence index from the bitmap tag
 * and resets the frame counter. Also zeroes the render time accumulator
 * (datum+0x24). ESI = contrail datum pointer. */
void FUN_00097db0(char *datum /* @<esi> */)
{
  char *tag;
  char *bitmap;
  int16_t sequence_index;
  int16_t frame_counter;
  char *sequences_block;
  char *seq_element;
  int16_t seq_min;
  int16_t seq_max;

  tag = (char *)tag_get(0x636f6e74, *(int *)(datum + 4));
  bitmap = (char *)tag_get(0x6269746d, *(int *)(tag + 0x3c));

  sequence_index = *(int16_t *)(datum + 0x14);
  *(int16_t *)(datum + 0x16) = *(int16_t *)(datum + 0x16) + 1;
  frame_counter = *(int16_t *)(datum + 0x16);
  *(float *)(datum + 0x24) = 0.0f;

  if (sequence_index < 0)
    goto pick_random;
  sequences_block = bitmap + 0x54;
  if ((int)sequence_index >= *(int *)sequences_block)
    goto pick_random;
  if (frame_counter < 0)
    goto pick_random;

  seq_element =
    (char *)tag_block_get_element(sequences_block, (int)sequence_index, 0x40);
  frame_counter = *(int16_t *)(datum + 0x16);
  if (frame_counter < *(int16_t *)(seq_element + 0x22))
    return;

pick_random:
  seq_min = *(int16_t *)(tag + 0x40);
  seq_max = seq_min + *(int16_t *)(tag + 0x42);
  *(int16_t *)(datum + 0x14) =
    random_range(random_math_get_local_seed_address(), seq_min, seq_max);
  *(int16_t *)(datum + 0x16) = 0;
}

/* 0x97e40 — contrail_add_point.  For each marker on the contrail's attached
 * object, allocates 'count' new contrail-point datums and prepends them to
 * the per-marker point chain.
 *
 * When flag==0 and the existing chain head's position matches the marker's
 * current position, the spawn is skipped for that marker.
 *
 * When count > 1, intermediate points are linearly interpolated between
 * the new marker position and the old chain head position. */
void FUN_00097e40(int contrail_handle /* @<eax> */, int count, int flag)
{
  char *datum;
  char *ctag;
  char *prev_head;
  char *pd;
  void *obj_struct;
  char *obj_tag;
  void *marker_elem;
  int16_t marker_count;
  int16_t emit_count;
  float local_30;
  float local_24;
  float local_10;
  int new_idx;
  char local_218[4 * 0x6c];

  datum = (char *)datum_get(contrail_data, contrail_handle);
  ctag = (char *)tag_get(0x636f6e74, *(int *)(datum + 4));

  if ((int16_t)count == 0)
    return;

  obj_struct = object_get_and_verify_type(*(int *)(datum + 8), -1);
  obj_tag = (char *)tag_get(0x6f626a65, *(int *)obj_struct);
  marker_elem =
    tag_block_get_element(obj_tag + 0x140, *(int16_t *)(datum + 0xc), 0x48);
  marker_count = object_get_markers_by_string_id(
    *(int *)(datum + 8), (char *)marker_elem + 0x10, local_218, 4);

  if (marker_count <= 0)
    return;

  local_30 = contrail_scale_random_value(
    *(float *)(datum + 0x10), *(float *)(ctag + 8), *(float *)(ctag + 0xc),
    *(uint16_t *)(ctag + 2), 1);

  local_24 = *(float *)(ctag + 0x10);
  if (*(uint8_t *)(ctag + 2) & 0x8)
    local_24 *= *(float *)(datum + 0x10);

  local_10 = *(float *)(ctag + 0x14);
  if (*(uint8_t *)(ctag + 2) & 0x10)
    local_10 *= *(float *)(datum + 0x10);

  if (marker_count <= 0)
    return;

  {
    unsigned int remaining = (unsigned int)(uint16_t)marker_count;
    char *marker = local_218;
    int *chain_head = (int *)(datum + 0x34);
    int marker_idx = 0;

    do {
      if (*chain_head == -1) {
        prev_head = NULL;
        emit_count = 1;
      } else {
        prev_head = (char *)datum_get(contrail_point_data, *chain_head);
        emit_count = (int16_t)count;
        if (prev_head != NULL &&
            csmemcmp(marker + 0x60, prev_head + 0x1c, 0xc) == 0 && flag == 0) {
          goto next_marker;
        }
      }

      {
        int cur_iter = 1;
        while ((int16_t)cur_iter <= emit_count) {
          new_idx = data_new_at_index(contrail_point_data);
          if (new_idx == -1)
            goto inner_next;

          pd = (char *)datum_get(contrail_point_data, new_idx);
          *(int *)(pd + 4) = 0;
          *(int *)(pd + 8) = 0;
          *(uint8_t *)(pd + 2) = 3;
          *(uint8_t *)(pd + 3) = 0xff;
          *(float *)(pd + 0xc) = *(float *)(datum + 0x10);

          {
            float vel[3];
            float root_loc[3];
            unsigned int *seed = random_math_get_local_seed_address();
            random_direction3d((int *)seed, (float *)(marker + 0x3c), 0.0f,
                               local_24, vel);

            *(float *)(pd + 0x1c) = *(float *)(marker + 0x60);
            *(float *)(pd + 0x20) = *(float *)(marker + 0x64);
            *(float *)(pd + 0x24) = *(float *)(marker + 0x68);

            scenario_location_from_point(pd + 0x14, pd + 0x1c);

            object_get_root_location(*(int *)(datum + 8), root_loc, NULL);

            *(float *)(pd + 0x28) = vel[0] * local_30 + local_10 * root_loc[0];
            *(float *)(pd + 0x2c) = vel[1] * local_30 + local_10 * root_loc[1];
            *(float *)(pd + 0x30) = vel[2] * local_30 + local_10 * root_loc[2];
          }

          if ((int16_t)cur_iter < emit_count) {
            float t;
            float frac_new;
            float frac_old;
            float blended_pos[3];

            t = (float)cur_iter / (float)(int16_t)emit_count;

            if ((*(uint32_t *)&t & 0x7f800000) == 0x7f800000) {
              char *msg =
                csprintf((char *)0x5ab100, "%s: assert_valid_real(0x%08X %f)",
                         "t", *(uint32_t *)&t, (double)t);
              display_assert(msg, "c:\\halo\\SOURCE\\effects\\contrails.c",
                             0x1d7, 1);
              system_exit(-1);
            }

            frac_new = t;
            frac_old = 1.0f - t;

            *(float *)(pd + 0xc) = frac_new * *(float *)(pd + 0xc) +
                                   frac_old * *(float *)(prev_head + 0xc);

            blended_pos[0] = frac_new * *(float *)(pd + 0x1c) +
                             frac_old * *(float *)(prev_head + 0x1c);
            blended_pos[1] = frac_new * *(float *)(pd + 0x20) +
                             frac_old * *(float *)(prev_head + 0x20);
            blended_pos[2] = frac_new * *(float *)(pd + 0x24) +
                             frac_old * *(float *)(prev_head + 0x24);

            scenario_location_from_point(pd + 0x14, blended_pos);
            *(float *)(pd + 0x1c) = blended_pos[0];
            *(float *)(pd + 0x20) = blended_pos[1];
            *(float *)(pd + 0x24) = blended_pos[2];

            *(float *)(pd + 0x28) = frac_new * *(float *)(pd + 0x28) +
                                    frac_old * *(float *)(prev_head + 0x28);
            *(float *)(pd + 0x2c) = frac_new * *(float *)(pd + 0x2c) +
                                    frac_old * *(float *)(prev_head + 0x2c);
            *(float *)(pd + 0x30) = frac_new * *(float *)(pd + 0x30) +
                                    frac_old * *(float *)(prev_head + 0x30);
          }

          *(int *)(pd + 0x34) = *chain_head;
          *(int16_t *)(datum + 0x2c + marker_idx * 2) += 1;
          *chain_head = new_idx;

        inner_next:
          cur_iter++;
        }
      }

    next_marker:
      marker_idx++;
      chain_head++;
      marker += 0x6c;
      remaining--;
    } while (remaining != 0);
  }
}

/* 0x98580 — contrail_new: allocates a new contrail datum and initialises it.
 *
 * Validates both object_index and definition_index, then calls
 * data_new_at_index to obtain a slot.  On success the datum is populated:
 *   datum+0x02 : flags (cleared)
 *   datum+0x04 : definition_index (tag handle)
 *   datum+0x08 : object_index (object handle)
 *   datum+0x0c : attachment_index (marker index into object tag's contrail
 * list) datum+0x0e : initial sequence frame ceiling (from tag element's
 * max-frame field - 1) datum+0x10 : out_value buffer (filled by
 * object_get_function_value) datum+0x14 : sequence index (set to 0xffff)
 *   datum+0x18 : zeroed
 *   datum+0x1c : zeroed
 *   datum+0x2c..0x33: 4 x int16_t point counts zeroed
 *   datum+0x34..0x43: 4 x int32_t chain heads set to -1
 *
 * If object_get_function_value reports the function is active the attached
 * flag (datum+0x02 bit 0) is set and one initial point is spawned.
 *
 * Returns the new datum handle, or -1 if allocation failed or param_1==-1. */
int contrail_new(int definition_index, int object_index, short attachment_index)
{
  char *datum;
  void *obj_struct;
  char *obj_tag_elem;
  int iVar2;
  unsigned short *puVar6;
  int *puVar4;
  int iVar5;

  if (object_index == -1) {
    display_assert("object_index!=NONE",
                   "c:\\halo\\SOURCE\\effects\\contrails.c", 0x61, 1);
    system_exit(-1);
  }

  if (definition_index == -1) {
    display_assert("definition_index!=NONE",
                   "c:\\halo\\SOURCE\\effects\\contrails.c", 0x62, 1);
    system_exit(-1);
    return -1;
  }

  tag_get(0x636f6e74, definition_index);
  iVar2 = data_new_at_index(contrail_data);
  if (iVar2 == -1)
    return iVar2;

  datum = (char *)datum_get(contrail_data, iVar2);
  obj_struct = object_get_and_verify_type(object_index, -1);

  *(short *)(datum + 0xc) = attachment_index;
  *(short *)(datum + 0x2) = 0;
  *(int *)(datum + 0x4) = definition_index;
  *(int *)(datum + 0x8) = object_index;

  obj_tag_elem = (char *)tag_get(0x6f626a65, *(int *)obj_struct);
  obj_tag_elem = (char *)tag_block_get_element(obj_tag_elem + 0x140,
                                               (int)attachment_index, 0x48);
  *(short *)(datum + 0xe) = *(short *)(obj_tag_elem + 0x30) - 1;
  *(short *)(datum + 0x14) = (short)0xffff;

  FUN_00097db0(datum);

  *(int *)(datum + 0x18) = 0;
  *(int *)(datum + 0x1c) = 0;

  puVar6 = (unsigned short *)(datum + 0x2c);
  puVar4 = (int *)(datum + 0x34);
  iVar5 = 4;
  do {
    *puVar6 = 0;
    *puVar4 = -1;
    puVar6++;
    puVar4++;
    iVar5--;
  } while (iVar5 != 0);

  if (object_get_function_value(*(int *)(datum + 0x8),
                                (unsigned short)*(short *)(datum + 0xe),
                                datum + 0x10)) {
    *(unsigned char *)(datum + 0x2) |= 1;
    FUN_00097e40(iVar2, 1, 1);
  }

  return iVar2;
}

/*
 * contrail_set_state_for_object (0x986d0): given a contrail datum handle and a
 * delta-time, conditionally spawns new contrail points (if the contrail is
 * attached, flag bit 0x1 set) and optionally resets the point chain head to
 * -1 (if reset_points != 0).  Accumulates delta_time into datum+0x28.
 * Called from vehicle/projectile update paths to drive contrail simulation.
 */
void contrail_set_state_for_object(int contrail_handle, bool reset_points,
                                   float delta_time)
{
  void *datum;
  int16_t new_points;
  int count;

  datum = datum_get(contrail_data, contrail_handle);
  (void)tag_get(0x636f6e74, *(int *)((char *)datum + 4));

  if (*(uint8_t *)((char *)datum + 2) & 1) {
    new_points = FUN_00097a50(contrail_handle, delta_time);
    count = (new_points < 1) ? 1 : (int)new_points;
    FUN_00097e40(contrail_handle, count, 0);
  }

  if (reset_points) {
    *(int *)((char *)datum + 8) = -1;
  }

  *(float *)((char *)datum + 0x28) =
    delta_time + *(float *)((char *)datum + 0x28);
}

void contrails_update(float delta_time)
{
  int datum_handle;
  char *datum;
  char *tag;
  float elapsed;
  float render_freq;
  float remaining;
  float step;
  uint8_t attached;
  int saved_point;
  int result;
  int16_t i;

  datum_handle = data_next_index(contrail_data, -1);
  while (datum_handle != -1) {
    datum = (char *)datum_get(contrail_data, datum_handle);
    tag = (char *)tag_get(0x636f6e74, *(int *)(datum + 4));
    elapsed = delta_time - *(float *)(datum + 0x28);
    /* contrail_validate: EAX = datum_handle */
    {
      int _eax = datum_handle;
#if defined(_MSC_VER) && !defined(__clang__)
      __asm {
        mov eax, _eax
        mov edx, 0x97ae0
        call edx
      }
#else
      asm volatile("movl $0x97ae0, %%edx\n\t"
                   "call *%%edx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "esi", "edi", "memory", "cc");
#endif
    }
    *(int *)(datum + 0x28) = 0;
    if (*(int *)(datum + 8) != -1) {
      attached = ((uint8_t(*)(int, int, void *))0x1403a0)(
        *(int *)(datum + 8), *(uint16_t *)(datum + 0xe),
        (void *)(datum + 0x10));
      if (attached != (*(uint8_t *)(datum + 2) & 1)) {
        saved_point = *(int *)(datum + 0x10);
        *(int *)(datum + 0x10) = 0;
        /* contrail_set_state: EAX = datum_handle, stack = (1, 1) */
        {
          int _eax = datum_handle;
#if defined(_MSC_VER) && !defined(__clang__)
          __asm {
            push 1
            push 1
            mov eax, _eax
            mov edx, 0x97e40
            call edx
            add esp, 8
          }
#else
          asm volatile("pushl $1\n\t"
                       "pushl $1\n\t"
                       "movl $0x97e40, %%edx\n\t"
                       "call *%%edx\n\t"
                       "addl $8, %%esp"
                       : "+a"(_eax)
                       :
                       : "ecx", "edx", "esi", "edi", "memory", "cc");
#endif
        }
        *(int *)(datum + 0x10) = saved_point;
      }
      if (attached == 0) {
        *(uint8_t *)(datum + 2) &= 0xfe;
      } else {
        *(uint8_t *)(datum + 2) |= 1;
        result = FUN_00097a50(datum_handle, delta_time);
        /* contrail_set_state: EAX = datum_handle, stack = (result, 1) */
        {
          int _eax = datum_handle;
#if defined(_MSC_VER) && !defined(__clang__)
          __asm {
            push 1
            push result
            mov eax, _eax
            mov edx, 0x97e40
            call edx
            add esp, 8
          }
#else
          asm volatile("pushl $1\n\t"
                       "pushl %[r]\n\t"
                       "movl $0x97e40, %%edx\n\t"
                       "call *%%edx\n\t"
                       "addl $8, %%esp"
                       : "+a"(_eax)
                       : [r] "r"(result)
                       : "ecx", "edx", "esi", "edi", "memory", "cc");
#endif
        }
      }
    }
    render_freq = *(float *)(tag + 0x2c);
    if ((*(uint8_t *)(tag + 2) & 0x20) != 0)
      render_freq *= *(float *)(datum + 0x10);
    render_freq = *(float *)0x2533c8 / render_freq;
    remaining = elapsed;
    if (remaining > *(float *)0x2533c0) {
      do {
        step = render_freq - *(float *)(datum + 0x24);
        if (step > remaining) {
          *(float *)(datum + 0x24) += remaining;
          break;
        }
        FUN_00097db0(datum);
        remaining -= step;
      } while (remaining > *(float *)0x2533c0);
    }
    {
      float fade_rate = *(float *)(tag + 0x24);
      if ((*(uint8_t *)(tag + 3) & 1) != 0)
        fade_rate *= *(float *)(datum + 0x10);
      *(float *)(datum + 0x18) -= fade_rate * elapsed;
    }
    {
      float grow_rate = *(float *)(tag + 0x28);
      if ((*(uint8_t *)(tag + 3) & 2) != 0)
        grow_rate *= *(float *)(datum + 0x10);
      *(float *)(datum + 0x1c) += grow_rate * elapsed;
    }
    /* contrail_render_update: normal cdecl(datum_handle, delta_time) */
    ((void (*)(int, float))0x98200)(datum_handle, delta_time);
    i = 0;
    do {
      if (((0 < *(int16_t *)(datum + 0x2c + (int)i * 2)) !=
           (*(int *)(datum + 0x34 + (int)i * 4) != -1)) == true) {
        display_assert(
          "contrail->contrail_point_counts[instance_index]>0=="
          "contrail->first_contrail_point_indices[instance_index]!=NONE",
          "c:\\halo\\SOURCE\\effects\\contrails.c", 0x10a, 1);
        system_exit(-1);
      }
    } while (*(int *)(datum + 0x34 + (int)i * 4) == -1 && (i++, i < 4));
    /* contrail_validate: EAX = datum_handle */
    {
      int _eax = datum_handle;
#if defined(_MSC_VER) && !defined(__clang__)
      __asm {
        mov eax, _eax
        mov edx, 0x97ae0
        call edx
      }
#else
      asm volatile("movl $0x97ae0, %%edx\n\t"
                   "call *%%edx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "esi", "edi", "memory", "cc");
#endif
    }
    if (i == 4 && *(int *)(datum + 8) == -1)
      ((void (*)(int))0x978f0)(datum_handle);
    datum_handle = data_next_index(contrail_data, datum_handle);
  }
}

/* Detach a particle system header from its parent object (clear attached
   flag and invalidate the object handle).  Despite living in contrails.c,
   this operates on particle_system_header_data (DAT_005aa8a8). */
void FUN_0009f6e0(int contrail_handle)
{
  char *datum;

  datum = (char *)datum_get(particle_system_header_data, contrail_handle);
  *(uint32_t *)(datum + 4) &= ~1u;
  *(int *)(datum + 0xc) = -1;
}
