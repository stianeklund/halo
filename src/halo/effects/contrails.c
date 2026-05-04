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
int16_t FUN_00097c80(int16_t min, int16_t max)
{
  return random_range(random_math_get_local_seed_address(), min, max);
}

/* contrail_delete, contrail_compute, contrail_validate, contrail_add_point,
 * contrail_set_state, and contrail_render_update all use register params
 * (@<eax>, @<esi>) and are called from original binary code that we haven't
 * ported. The current thunk system only bridges our code → original, not
 * original → our code. So these must remain in the original binary until
 * the entire TU is ported together. See feedback_register_args.md. */

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
      asm volatile("movl $0x97ae0, %%edx\n\t"
                   "call *%%edx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "esi", "edi", "memory", "cc");
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
          asm volatile("pushl $1\n\t"
                       "pushl $1\n\t"
                       "movl $0x97e40, %%edx\n\t"
                       "call *%%edx\n\t"
                       "addl $8, %%esp"
                       : "+a"(_eax)
                       :
                       : "ecx", "edx", "esi", "edi", "memory", "cc");
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
          asm volatile("pushl $1\n\t"
                       "pushl %[r]\n\t"
                       "movl $0x97e40, %%edx\n\t"
                       "call *%%edx\n\t"
                       "addl $8, %%esp"
                       : "+a"(_eax)
                       : [r] "r"(result)
                       : "ecx", "edx", "esi", "edi", "memory", "cc");
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
        /* contrail_add_point takes ESI as a register param in the original
         * binary. Call the original directly to avoid thunk mismatch. */
        {
          char *_esi = datum;
          asm volatile("movl $0x97db0, %%eax\n\t"
                       "call *%%eax"
                       : "+S"(_esi)
                       :
                       : "eax", "ecx", "edx", "edi", "memory", "cc");
        }
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
      asm volatile("movl $0x97ae0, %%edx\n\t"
                   "call *%%edx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "esi", "edi", "memory", "cc");
    }
    if (i == 4 && *(int *)(datum + 8) == -1)
      ((void (*)(int))0x978f0)(datum_handle);
    datum_handle = data_next_index(contrail_data, datum_handle);
  }
}

/* Detach a contrail from its parent object (clear attached flag and
   invalidate the object handle). */
void FUN_0009f6e0(int contrail_handle)
{
  char *datum;

  datum = (char *)datum_get(contrail_data, contrail_handle);
  *(uint32_t *)(datum + 4) &= ~1u;
  *(int *)(datum + 0xc) = -1;
}
