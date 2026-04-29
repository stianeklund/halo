void particle_systems_initialize(void)
{
  particle_system_header_data =
    game_state_data_new("particle systems", 0x40, 0x158);
  particle_system_data =
    game_state_data_new("particle system particles", 0x200, 0x80);
}

void particle_systems_initialize_for_new_map(void)
{
  data_delete_all(particle_system_header_data);
  data_delete_all(particle_system_data);
}

void particle_system_delete(int particle_system_handle)
{
  char *entry, *tag, *particle_entry;
  int particle_index, next_particle;
  int i, instance_count;

  entry =
    (char *)datum_get(particle_system_header_data, particle_system_handle);
  tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
  instance_count = *(int *)(tag + 0x5c);
  for (i = 0; i < instance_count; i++) {
    particle_index = *(int *)(entry + 0x94 + i * 0x40);
    while (particle_index != NONE) {
      particle_entry = (char *)datum_get(particle_system_data, particle_index);
      next_particle = *(int *)(particle_entry + 4);
      datum_delete(particle_system_data, particle_index);
      particle_index = next_particle;
    }
  }
  datum_delete(particle_system_header_data, particle_system_handle);
}

void particle_systems_dispose(void)
{
}

/* Advance particle type state to next state index (0x9f920).
 * Computes next_state = current_state + delta, where delta is +1 or -1
 * based on the direction flag at type_state+0x38. If next_state is valid
 * (0 <= next_state < particle_states.count), just stores it. Otherwise:
 * - If type can loop (flag bit 0) and has an object and states exist:
 *   - If ping-pong mode (flag bit 1): bounce off ends, flip direction
 *   - Else: wrap to state 0
 * - Otherwise: terminate by setting both current_state and next_state to -1 */
void FUN_0009f920(void *type_state_arg, void *type_def_arg, void *ps_datum)
{
  char *type_state = (char *)type_state_arg;
  char *type_def = (char *)type_def_arg;
  char direction;
  short delta;
  short current_state;
  short next_state;
  int state_count;
  unsigned int flags;

  direction = *(char *)(type_state + 0x38);
  delta = (direction != 0) ? 1 : -1;
  current_state = *(short *)type_state;
  next_state = current_state + delta;
  *(short *)(type_state + 0x2) = next_state;

  if (next_state >= 0 && (int)next_state < *(int *)(type_def + 0x68)) {
    /* Valid next state */
    return;
  }

  /* Out of bounds - check if we can loop */
  flags = *(unsigned int *)(type_def + 0x20);
  if ((flags & 1) == 0) {
    goto terminate;
  }
  if (*(int *)((char *)ps_datum + 0xc) == -1) {
    goto terminate;
  }
  state_count = *(int *)(type_def + 0x68);
  if (state_count <= 0) {
    goto terminate;
  }

  /* Can loop */
  if ((flags & 2) != 0) {
    /* Ping-pong mode: bounce off ends and flip direction */
    int bounced = (int)current_state - (int)delta;
    if (bounced < 0) {
      *(short *)(type_state + 0x2) = 0;
      *(char *)(type_state + 0x38) = (direction == 0) ? 1 : 0;
      return;
    }
    if (bounced > state_count - 1) {
      bounced = state_count - 1;
    }
    *(short *)(type_state + 0x2) = (short)bounced;
    *(char *)(type_state + 0x38) = (direction == 0) ? 1 : 0;
    return;
  }

  /* Wrap mode: restart at state 0 */
  *(short *)(type_state + 0x2) = 0;
  return;

terminate:
  *(short *)type_state = -1;
  *(short *)(type_state + 0x2) = -1;
}

/* Advance particle state to next state index (0x9f9d0).
 * Similar to FUN_0009f920 but operates on individual particle state rather
 * than type state. Computes next_state = current_state + delta, where delta
 * is +1 or -1 based on the direction flag at particle+0x2. If next_state is
 * valid (0 <= next_state < particle_states.count at sys_def+0x74), stores it.
 * Otherwise:
 * - If type can loop (flag bit 2) and states exist:
 *   - If ping-pong mode (flag bit 3): bounce off ends, flip direction
 *   - Else: wrap to state 0
 * - Otherwise: terminate by setting both current/next_state to -1 */
void FUN_0009f9d0(void *particle_arg, void *sys_def_arg)
{
  char *particle = (char *)particle_arg;
  char *sys_def = (char *)sys_def_arg;
  char direction;
  short delta;
  short current_state;
  short next_state;
  int state_count;
  unsigned int flags;

  direction = *(char *)(particle + 0x2);
  delta = (direction != 0) ? 1 : -1;
  current_state = *(short *)(particle + 0x8);
  next_state = current_state + delta;
  *(short *)(particle + 0xa) = next_state;

  if (next_state >= 0 && (int)next_state < *(int *)(sys_def + 0x74)) {
    /* Valid next state */
    return;
  }

  /* Out of bounds - check if we can loop */
  flags = *(unsigned int *)(sys_def + 0x20);
  if ((flags & 4) == 0) {
    goto terminate;
  }
  state_count = *(int *)(sys_def + 0x74);
  if (state_count <= 0) {
    goto terminate;
  }

  /* Can loop */
  if ((flags & 8) != 0) {
    /* Ping-pong mode: bounce off ends and flip direction */
    int bounced = (int)current_state - (int)delta;
    if (bounced < 0) {
      *(short *)(particle + 0xa) = 0;
      *(char *)(particle + 0x2) = (direction == 0) ? 1 : 0;
      return;
    }
    if (bounced > state_count - 1) {
      bounced = state_count - 1;
    }
    *(short *)(particle + 0xa) = (short)bounced;
    *(char *)(particle + 0x2) = (direction == 0) ? 1 : 0;
    return;
  }

  /* Wrap mode: restart at state 0 */
  *(short *)(particle + 0xa) = 0;
  return;

terminate:
  *(short *)(particle + 0x8) = -1;
  *(short *)(particle + 0xa) = -1;
}

void particle_systems_dispose_from_old_map(void)
{
  int particle_system_index;

  if (particle_system_header_data && particle_system_header_data->valid) {
    for (particle_system_index =
           data_next_index(particle_system_header_data, NONE);
         particle_system_index != NONE;
         particle_system_index = data_next_index(particle_system_header_data,
                                                 particle_system_index)) {
      particle_system_delete(particle_system_index);
    }
    data_make_invalid(particle_system_header_data);
    data_make_invalid(particle_system_data);
  }
}

/* Main per-particle-system update tick (0xa0180).
 * Updates object attachment, runs system physics, then iterates all
 * particle types and their linked particles — handling state transitions,
 * lifetime, interpolation, flag-based multipliers, and particle physics.
 * Dead particles are unlinked and deleted. If no active types remain and
 * the system has no object attachment, deletes the system via
 * particle_system_delete. */
void FUN_000a0180(float dt, int particle_system_handle)
{
  char *ps_datum;
  char *tag_def;
  char *type_def;
  char *type_state;
  char *saved_type_state;
  char *states_block;
  char *state_elem;
  char *particle;
  char *prev_particle;
  char *type_state_def;
  char *tag_block_ptr;
  int particle_handle;
  short next_state;
  short particle_next_state;
  short i;
  short active_types;
  float t, t_inv;
  float duration;

  ps_datum =
    (char *)datum_get(particle_system_header_data, particle_system_handle);
  tag_def = (char *)tag_get(0x7063746c, *(int *)(ps_datum + 8));
  active_types = 0;

  /* Object attachment logic */
  if (*(int *)(ps_datum + 0xc) != -1) {
    char *obj =
      (char *)object_get_and_verify_type(*(int *)(ps_datum + 0xc), -1);
    if ((*(unsigned int *)(obj + 4) & 0x800) != 0 &&
        FUN_001403a0(*(int *)(ps_datum + 0xc), *(short *)(ps_datum + 0x12),
                     (void *)(ps_datum + 0x14))) {
      *(unsigned int *)(ps_datum + 4) = *(unsigned int *)(ps_datum + 4) | 1;
    } else {
      *(unsigned int *)(ps_datum + 4) = *(unsigned int *)(ps_datum + 4) & ~1u;
    }
    object_get_world_position(*(int *)(ps_datum + 0xc),
                              (vector3_t *)(ps_datum + 0x20));
    object_get_root_location(*(int *)(ps_datum + 0xc),
                             (float *)(ps_datum + 0x2c), (float *)0);
    *(float *)(ps_datum + 0x2c) =
      *(float *)(ps_datum + 0x2c) * *(float *)0x253394;
    *(float *)(ps_datum + 0x30) =
      *(float *)(ps_datum + 0x30) * *(float *)0x253394;
    *(float *)(ps_datum + 0x34) =
      *(float *)(ps_datum + 0x34) * *(float *)0x253394;
  }

  if (*(short *)(tag_def + 0x48) < 0 || *(short *)(tag_def + 0x48) >= 2) {
    display_assert(
      "system_definition->system_update_physics>=0 && "
      "system_definition->system_update_physics<"
      "NUMBER_OF_PARTICLE_SYSTEM_UPDATE_PHYSICS",
      "c:\\halo\\SOURCE\\effects\\particle_systems.c", 0x2e1, 1);
    system_exit(-1);
  }

  /* Indirect call: system physics update */
  {
    typedef void (*system_physics_fn)(char *, float);
    ((system_physics_fn *)(0x26ab08))[*(short *)(tag_def + 0x48)](ps_datum, dt);
  }

  tag_block_ptr = tag_def + 0x5c;
  i = 0;
  if (*(int *)tag_block_ptr < 1)
    goto done;

  /* Outer loop: iterate particle types */
  for (;;) {
    type_def =
      (char *)tag_block_get_element((void *)tag_block_ptr, (int)(short)i, 0x80);
    type_state = ps_datum + 0x58 + (int)(short)i * 0x40;
    saved_type_state = type_state;

    /* Skip disabled types (flag 0x100) */
    if ((*(unsigned int *)(type_def + 0x20) & 0x100) != 0)
      goto next_type;

    /* Decrement type timer */
    *(float *)(type_state + 4) = *(float *)(type_state + 4) - dt;

    /* If current state is NONE, skip */
    if (*(short *)type_state == -1)
      goto next_type;

    states_block = type_def + 0x68;

  state_transition_loop:
    state_elem = (char *)tag_block_get_element((void *)states_block,
                                               (int)*(short *)type_state, 0xc0);
    next_state = *(short *)(type_state + 2);

    /* If timer >= 0.0, do interpolation */
    if (*(float *)(type_state + 4) >= 0.0f)
      goto do_interpolation;

    /* Timer expired: state transition */
    if (next_state == -1) {
      /* No next state: advance via FUN_0009f920 */
      FUN_0009f920(type_state, type_def, ps_datum);
      duration = random_real_range((int *)random_math_get_local_seed_address(),
                                   *(float *)(state_elem + 0x28),
                                   *(float *)(state_elem + 0x2c));
    } else {
      *(short *)type_state = next_state;
      *(short *)(type_state + 2) = -1;
      state_elem = (char *)tag_block_get_element((void *)states_block,
                                                 (int)next_state, 0xc0);
      duration = random_real_range((int *)random_math_get_local_seed_address(),
                                   *(float *)(state_elem + 0x20),
                                   *(float *)(state_elem + 0x24));
    }
    *(float *)(type_state + 8) = duration;
    *(float *)(type_state + 4) = duration + *(float *)(type_state + 4);
    if (*(short *)type_state == -1)
      goto after_interpolation;
    goto state_transition_loop;

  do_interpolation:
    if (next_state == -1) {
      /* Single state: copy properties directly */
      csmemcpy(type_state + 0xc, state_elem + 0x34, 0x28);
    } else {
      char *state_elem2 = (char *)tag_block_get_element((void *)states_block,
                                                        (int)next_state, 0xc0);
      float *src = (float *)(state_elem + 0x34);
      float *src2 = (float *)(state_elem2 + 0x34);
      float *dst = (float *)(type_state + 0xc);
      int k;
      t = *(float *)(type_state + 4) / *(float *)(type_state + 8);
      if (t < 0.0f)
        t = 0.0f;
      else if (t > 1.0f)
        t = 1.0f;
      t_inv = 1.0f - t;
      for (k = 0; k < 10; k++) {
        dst[k] = t * src[k] + t_inv * src2[k];
      }
    }

    /* Flag-based multipliers */
    if ((*(unsigned int *)(type_def + 0x20) & 0x200) != 0) {
      *(float *)(type_state + 0x18) =
        *(float *)(ps_datum + 0x38) * *(float *)(type_state + 0x18);
      *(float *)(type_state + 0x1c) =
        *(float *)(ps_datum + 0x3c) * *(float *)(type_state + 0x1c);
      *(float *)(type_state + 0x20) =
        *(float *)(ps_datum + 0x40) * *(float *)(type_state + 0x20);
      *(float *)(type_state + 0x24) =
        *(float *)(ps_datum + 0x44) * *(float *)(type_state + 0x24);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x800) != 0) {
      *(float *)(type_state + 0x2c) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x2c);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x1000) != 0) {
      *(float *)(type_state + 0x30) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x30);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x2000) != 0) {
      *(float *)(type_state + 0xc) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0xc);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x4000) != 0) {
      *(float *)(type_state + 0x10) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x10);
    }
    if ((*(unsigned int *)(type_def + 0x20) & 0x8000) != 0) {
      *(float *)(type_state + 0x14) =
        *(float *)(ps_datum + 0x14) * *(float *)(type_state + 0x14);
    }

  after_interpolation:
    /* If current state is NONE, no particles to process */
    if (*(short *)type_state == -1)
      goto type_done;

    /* Emit particles if attached (flag bit 0) */
    prev_particle = (char *)0;
    if ((*(unsigned char *)(ps_datum + 4) & 1) != 0) {
      FUN_0009fd30(ps_datum, (int)(short)i, dt);
    }

    /* Inner particle loop: walk linked list at type_state + 0x3c */
    {
      short bx = *(short *)(type_state + 0x3c);
      for (;;) {
        if (bx == -1)
          goto particles_done;
        particle_handle = (int)bx;
        particle = (char *)datum_get(particle_system_data, particle_handle);

        /* Decrement particle lifetime */
        *(float *)(particle + 0xc) = *(float *)(particle + 0xc) - dt;

        /* Particle state init: if state == NONE and type has states */
        if (*(short *)(particle + 8) == -1 && *(int *)(type_def + 0x74) > 0) {
          char *pstate_elem;
          *(short *)(particle + 8) = 0;
          pstate_elem =
            (char *)tag_block_get_element((void *)(type_def + 0x74), 0, 0x178);
          duration = random_real_range(
            (int *)random_math_get_local_seed_address(),
            *(float *)(pstate_elem + 0x20), *(float *)(pstate_elem + 0x24));
          *(float *)(particle + 0xc) = duration;
          *(float *)(particle + 0x10) = duration;
          FUN_000a0080(type_def, *(short *)(particle + 8), particle + 0x48);
        }

        /* If particle direction byte is 0, kill state */
        if (*(char *)(particle + 3) == 0) {
          *(short *)(particle + 8) = -1;
        }

        if (*(short *)(particle + 8) != -1) {
          char *pstates_block = type_def + 0x74;

          /* Particle state transition loop */
          for (;;) {
            char *pstate_elem = (char *)tag_block_get_element(
              (void *)pstates_block, (int)*(short *)(particle + 8), 0x178);

            /* If lifetime >= 0, stop transitioning */
            if (*(float *)(particle + 0xc) >= 0.0f)
              break;

            particle_next_state = *(short *)(particle + 0xa);
            if (particle_next_state == -1) {
              /* End of states: advance particle state */
              FUN_0009f9d0(particle, type_def);
              duration = random_real_range(
                (int *)random_math_get_local_seed_address(),
                *(float *)(pstate_elem + 0x28), *(float *)(pstate_elem + 0x2c));
            } else {
              /* Advance to next particle state */
              *(short *)(particle + 8) = particle_next_state;
              *(short *)(particle + 0xa) = -1;
              pstate_elem = (char *)tag_block_get_element(
                (void *)pstates_block, (int)particle_next_state, 0x178);
              duration = random_real_range(
                (int *)random_math_get_local_seed_address(),
                *(float *)(pstate_elem + 0x20), *(float *)(pstate_elem + 0x24));
            }
            *(float *)(particle + 0x10) = duration;
            *(float *)(particle + 0xc) = duration + *(float *)(particle + 0xc);

            if (*(short *)(particle + 0xa) == -1) {
              int *src_p = (int *)(particle + 0x64);
              int *dst_p = (int *)(particle + 0x48);
              int n;
              for (n = 7; n != 0; n--) {
                *dst_p = *src_p;
                src_p++;
                dst_p++;
              }
            } else {
              /* Regenerate particle output via FUN_000a0080 using next_state */
              FUN_000a0080(type_def, *(short *)(particle + 0xa),
                           particle + 0x64);
            }

            type_state = saved_type_state;

            if (*(short *)(particle + 8) == -1)
              break;
          } /* end particle state transition loop */

          if (*(short *)(particle + 8) != -1) {
            /* Particle is alive: apply physics */
            type_state_def = (char *)tag_block_get_element(
              (void *)states_block, (int)*(short *)type_state, 0xc0);

            if (*(short *)(particle + 0xa) == -1) {
              /* Single state: direct scale */
              *(float *)(particle + 0x40) = *(float *)(particle + 0x50) *
                                              *(float *)(type_state + 0x14) *
                                              dt +
                                            *(float *)(particle + 0x40);
              t = *(float *)(particle + 0x4c);
            } else {
              /* Interpolated state */
              tag_block_get_element((void *)(type_def + 0x74),
                                    (int)*(short *)(particle + 8), 0x178);
              t = *(float *)(particle + 0xc) / *(float *)(particle + 0x10);
              if (t < 0.0f)
                t = 0.0f;
              else if (t > 1.0f)
                t = 1.0f;
              t_inv = 1.0f - t;
              *(float *)(particle + 0x40) =
                (t * *(float *)(particle + 0x50) +
                 t_inv * *(float *)(particle + 0x6c)) *
                  *(float *)(type_state + 0x14) * dt +
                *(float *)(particle + 0x40);
              t = t * *(float *)(particle + 0x4c) +
                  t_inv * *(float *)(particle + 0x68);
            }

            *(float *)(particle + 0x44) =
              t * *(float *)(type_state + 0x10) * dt +
              *(float *)(particle + 0x44);

            if (*(short *)(type_state_def + 0xb2) < 0 ||
                *(short *)(type_state_def + 0xb2) >= 1) {
              display_assert(
                "type_state_definition->particle_update_physics>=0 && "
                "type_state_definition->particle_update_physics<"
                "NUMBER_OF_PARTICLE_SYSTEM_TYPE_UPDATE_PHYSICS",
                "c:\\halo\\SOURCE\\effects\\particle_systems.c", 0x3af, 1);
              system_exit(-1);
            }

            /* Indirect call: particle physics update */
            {
              typedef void (*particle_physics_fn)(char *, int, float, char *);
              ((particle_physics_fn *)(0x26ab1c))[*(
                short *)(type_state_def + 0xb2)](ps_datum, (int)(short)i, dt,
                                                 particle);
            }

            prev_particle = particle;
            bx = *(short *)(particle + 4);
            continue;
          }
        }

        /* Particle is dead: unlink and delete */
        if (prev_particle == (char *)0) {
          *(int *)(type_state + 0x3c) = *(int *)(particle + 4);
        } else {
          *(int *)(prev_particle + 4) = *(int *)(particle + 4);
        }
        datum_delete(particle_system_data, particle_handle);
        bx = *(short *)(particle + 4);
        *(short *)(type_state + 0x3a) = *(short *)(type_state + 0x3a) - 1;
        continue;
      }
    }

  particles_done:
    active_types = active_types + 1;
  type_done:
  next_type:
    i = i + 1;
    if ((int)(short)i >= *(int *)tag_block_ptr)
      break;
  }

done:
  /* Clear bit 1 of flags */
  *(unsigned int *)(ps_datum + 4) = *(unsigned int *)(ps_datum + 4) & ~2u;
  /* If no active types and no object, delete system */
  if (active_types == 0 && *(int *)(ps_datum + 0xc) == -1) {
    particle_system_delete(particle_system_handle);
  }
}

/* Initialize particle system type instances from the pctl tag (0xa0fd0).
 * For each particle type in the tag definition:
 *   - If the type has no particle states, returns false (setup failed).
 *   - Otherwise, initializes the instance state: sets the current state
 *     index to 0, next state to NONE, marks as initialized, clears
 *     particle count and first-particle handle, and picks a random
 *     duration from the first particle state's bounds.
 * Resolves the BSP location from the system's position, sets the
 * "location resolved" flag, then runs an initial 0.001s update tick
 * via FUN_000a0180 if all types were valid. */
char FUN_000a0fd0(int particle_handle)
{
  char *entry;
  char *tag;
  int *tag_block_ptr;
  char *type_def;
  char *state_elem;
  char *instance;
  short i;
  int idx;
  char result;
  float duration;

  entry = (char *)datum_get(particle_system_header_data, particle_handle);
  tag = (char *)tag_get(0x7063746c, *(int *)(entry + 8));
  result = 1;
  scenario_location_from_point(entry + 0x18, entry + 0x20);
  tag_block_ptr = (int *)(tag + 0x5c);
  *(unsigned int *)(entry + 4) |= 2;
  idx = 0;
  i = 0;
  if (*tag_block_ptr < 1) {
    result = 1;
  } else {
    do {
      instance = entry + 0x58 + idx * 0x40;
      type_def = (char *)tag_block_get_element(tag_block_ptr, idx, 0x80);
      if (*(int *)(type_def + 0x68) == 0) {
        result = 0;
      } else {
        *(short *)(instance + 0x00) = 0;
        *(short *)(instance + 0x02) = (short)NONE;
        *(char *)(instance + 0x38) = 1;
        *(short *)(instance + 0x3a) = 0;
        *(int *)(instance + 0x3c) = NONE;
        if (*(int *)(type_def + 0x68) > 0) {
          state_elem =
            (char *)tag_block_get_element((int *)(type_def + 0x68), 0, 0xc0);
          duration = random_real_range(
            (int *)random_math_get_local_seed_address(),
            *(float *)(state_elem + 0x20), *(float *)(state_elem + 0x24));
          *(float *)(instance + 0x04) = duration;
          *(float *)(instance + 0x08) = duration;
        }
      }
      i = i + 1;
      idx = (int)i;
    } while (idx < *tag_block_ptr);
    if (result == 0) {
      return 0;
    }
  }
  FUN_000a0180(0.001f, particle_handle);
  return result;
}

void particle_systems_update(float dt)
{
  int particle_system_index;

  assert_halt(particle_system_header_data &&
              particle_system_header_data->valid);
  for (particle_system_index =
         data_next_index(particle_system_header_data, NONE);
       particle_system_index != NONE;
       particle_system_index =
         data_next_index(particle_system_header_data, particle_system_index)) {
    FUN_000a0180(dt, particle_system_index);
  }
}
