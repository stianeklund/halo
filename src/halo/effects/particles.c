void particles_initialize(void)
{
  particle_data = game_state_data_new("particle", 0x400, 0x70);
  if (!particle_data)
    error(0, "couldn't allocate particle globals");
}

void particles_initialize_for_new_map(void)
{
  data_delete_all(particle_data);
}

void particles_dispose_from_old_map(void)
{
  data_make_invalid(particle_data);
}

void particles_dispose(void)
{
  if (particle_data)
    particle_data = 0;
}

/* Delete a particle, releasing any attached render state (secondary bitmap
 * sprite). datum_handle arrives via EBX from the caller's loop context. */
void particle_delete(int datum_handle)
{
  char *particle;
  char *tag;

  particle = (char *)datum_get(particle_data, datum_handle);
  tag = (char *)tag_get(0x70617274, *(int *)(particle + 4));

  /* release secondary bitmap render state if present */
  if (*(int *)(tag + 0x64) != -1) {
    /* 0xa1770 takes EAX=particle, ECX=tag->sprite_count, ESI=tag->bitmap,
     * plus one stack arg (scale = 0.0). */
    int _eax = (int)particle;
    int _ecx = *(int *)(tag + 0x58);
    int _esi = *(int *)(tag + 0x64);
    asm volatile("pushl $0\n\t"
                 "movl $0xa1770, %%edx\n\t"
                 "call *%%edx\n\t"
                 "addl $4, %%esp"
                 : "+a"(_eax), "+c"(_ecx), "+S"(_esi)
                 :
                 : "edx", "memory", "cc");
  }

  datum_delete(particle_data, datum_handle);
}

/* Advance a particle's animation frame. Returns true if the particle is
 * still alive after stepping. datum_handle arrives via EDI.
 * For instantaneous particles (tag flag bit 3), steps once per tick.
 * For animated particles, accumulates delta_time against frame_period
 * and steps when a full period elapses. */
bool particle_step(int datum_handle, float delta_time)
{
  char *particle;
  char *tag;
  uint32_t tag_flags;
  char alive = 1;

  particle = (char *)datum_get(particle_data, datum_handle);
  tag = (char *)tag_get(0x70617274, *(int *)(particle + 4));
  tag_flags = *(uint32_t *)tag;

  /* frozen/attached particle — skip stepping */
  if ((tag_flags & 2) && (*(uint8_t *)(particle + 2) & 2))
    return (bool)alive;

  /* instantaneous particle (no animation frames) */
  if (tag_flags & 8) {
    if (delta_time != *(float *)0x2533c0) {
      /* call particle_step_frame at 0xa1a90: EAX = datum_handle */
      int _eax = datum_handle;
      asm volatile("movl $0xa1a90, %%edx\n\t"
                   "call *%%edx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "memory", "cc");
      return (bool)(char)_eax;
    }
    return (bool)alive;
  }

  /* first tick: initialize frame time */
  if (*(int *)(particle + 0x1c) == (int)0xbf800000) {
    int _eax = datum_handle;
    asm volatile("movl $0xa1a90, %%edx\n\t"
                 "call *%%edx"
                 : "+a"(_eax)
                 :
                 : "ecx", "edx", "memory", "cc");
    alive = (char)_eax;
    *(float *)(particle + 0x1c) = 0.0f;
  }

  /* advance animation: consume delta_time in frame_period-sized steps */
  if (delta_time <= *(float *)0x2533c0 || !alive)
    return (bool)alive;

  while (alive) {
    float remaining =
      *(float *)(particle + 0x20) - *(float *)(particle + 0x1c);
    if (remaining > delta_time) {
      *(float *)(particle + 0x1c) += delta_time;
      break;
    }
    {
      int _eax = datum_handle;
      asm volatile("movl $0xa1a90, %%edx\n\t"
                   "call *%%edx"
                   : "+a"(_eax)
                   :
                   : "ecx", "edx", "memory", "cc");
      alive = (char)_eax;
    }
    delta_time -= remaining;
    if (delta_time <= *(float *)0x2533c0)
      break;
  }

  return (bool)alive;
}

/* Move a particle through the world for one tick. Handles free-flying
 * particles (point physics + collision) and object-attached particles
 * (velocity damping). Returns true if the particle is still alive. */
bool particle_move(int datum_handle, float delta_time)
{
  char *particle;
  char *tag;
  char *pphy;
  char *vel;
  int collision_flags;
  int collision_hit;
  char moved = 0;
  char collision_data[8];
  float local_14;

  particle = (char *)datum_get(particle_data, datum_handle);
  tag = (char *)tag_get(0x70617274, *(int *)(particle + 4));

  /* attached-to-object particles: just verify object still exists */
  if (*(uint8_t *)(particle + 2) & 2) {
    int obj = *(int *)(particle + 8);
    if (obj == -1)
      return 1;
    if (((int (*)(int, int))0x13d640)(obj, -1) != 0)
      return 1;
    datum_delete(particle_data, datum_handle);
    return 0;
  }

  /* get point physics tag for this particle type */
  pphy = (char *)tag_get(0x70706879, *(int *)(tag + 0x20));
  vel = particle + 0x48;

  if (*(int *)(particle + 8) == -1) {
    /* ---- free particle: point physics simulation ---- */
    float age = ((float (*)(int))0xa1670)(datum_handle);

    /* run point physics: returns collision flags in EAX */
    collision_flags = ((int (*)(int, void *, void *, int, void *, void *, int,
                                void *, void *, float, float))0x154a50)(
      0, pphy, particle + 0x28, -1, particle + 0x30, vel, 0, collision_data,
      &local_14, age, delta_time);
    collision_hit = collision_flags & 4;

    if (collision_hit) {
      /* collision occurred — update render state and effects */
      if (*(int *)(tag + 0x54) != -1 || *(int *)(tag + 0x30) != 0) {
        /* compute distance factor for LOD */
        float vx = *(float *)vel;
        float vy = *(float *)(vel + 4);
        float vz = *(float *)(vel + 8);
        float dist;
        float scale;

        {
          float mag;
          float dv;
          /* inline sqrt(vx*vx + vy*vy + vz*vz) */
          dv = vx * vx + vy * vy + vz * vz;
          asm volatile("fsqrt" : "+t"(dv));
          mag = dv;
          dist = (mag - *(float *)0x26ad4c) /
                 (*(float *)0x26ad48 - *(float *)0x26ad4c);
        }
        if (dist < *(float *)0x2533c0)
          dist = 0.0f;
        else if (dist > *(float *)0x2533c8)
          dist = 1.0f;
        scale = dist;

        /* update secondary bitmap render state */
        if (*(int *)(tag + 0x54) != -1) {
          int _eax = (int)particle;
          int _ecx = *(int *)(tag + 0x48);
          int _esi = *(int *)(tag + 0x54);
          int _scale = *(int *)&scale;
          asm volatile("pushl %[s]\n\t"
                       "movl $0xa1770, %%edx\n\t"
                       "call *%%edx\n\t"
                       "addl $4, %%esp"
                       : "+a"(_eax), "+c"(_ecx), "+S"(_esi)
                       : [s] "m"(_scale)
                       : "edx", "memory", "cc");
        }

        /* spawn contrail effect if applicable */
        if (*(int *)(tag + 0x30) != -1) {
          char has_contrail =
            ((char (*)(void *))0x9f3b0)(particle + 0x30);
          if (has_contrail) {
            ((void (*)(int, int, float, void *, void *, void *,
                       float))0x9f430)(
              *(int *)(tag + 0x30), 8, local_14, particle + 0x30,
              collision_data, particle + 0x28, scale);
          }
        }
      }

      /* "die on contact" flag */
      if (*(uint8_t *)tag & 0x20) {
        if (*(int *)(tag + 0x54) == -1) {
          particle_delete(datum_handle);
          return 0;
        }
        ((void (*)(int))0xa14f0)(datum_handle);
        return 0;
      }
    }

    /* check kill conditions from collision result */
    if ((collision_flags & 1) && (*(uint8_t *)(tag + 1) & 1))
      goto kill_particle;
    if ((collision_flags & 2) && *(int8_t *)tag < 0)
      goto kill_particle;

    /* apply aging if collided or has other physics results */
    if (collision_hit || (collision_flags & 8)) {
      if (*(float *)(particle + 0x60) > *(float *)0x2533f0)
        moved = 1;
      *(float *)(particle + 0x20) += *(float *)(tag + 0x88);
    }
  } else {
    /* ---- object-attached particle: velocity damping ---- */
    if (!(*(uint8_t *)(particle + 2) & 0x40)) {
      if (((int (*)(int, int))0x13d640)(*(int *)(particle + 8), -1) == 0) {
        datum_delete(particle_data, datum_handle);
        return 0;
      }
    }

    float age = ((float (*)(int))0xa1670)(datum_handle);
    float drag_factor = age * *(float *)(pphy + 0x24) * age;
    float damping = ((float (*)(void *, float))0x1548a0)(pphy, age);

    if (damping == *(float *)0x2533c0) {
      if (drag_factor == *(float *)0x2533c0)
        damping = *(float *)0x2533c8;
      else
        damping = *(float *)0x2533c0;
    } else {
      damping = *(float *)0x2533c8 - (drag_factor / damping) * delta_time;
      if (damping < *(float *)0x2533c0)
        damping = *(float *)0x2533c0;
      else if (damping > *(float *)0x2533c8)
        damping = *(float *)0x2533c8;
    }

    /* apply damping to velocity and update position */
    moved = 1;
    float new_vx = damping * *(float *)vel;
    *(float *)vel = new_vx;
    float new_vy = damping * *(float *)(vel + 4);
    *(float *)(vel + 4) = new_vy;
    float new_vz = damping * *(float *)(vel + 8);
    *(float *)(vel + 8) = new_vz;

    *(float *)(particle + 0x30) += new_vx * delta_time;
    *(float *)(particle + 0x34) += new_vy * delta_time;
    *(float *)(particle + 0x38) += new_vz * delta_time;
  }

  /* update facing direction if velocity is above threshold */
  {
    float vx = *(float *)vel;
    float vy = *(float *)(vel + 4);
    float vz = *(float *)(vel + 8);
    float speed_sq = vx * vx + vy * vy + vz * vz;
    if (speed_sq >= *(float *)0x255d90) {
      *(float *)(particle + 0x3c) = *(float *)vel;
      *(float *)(particle + 0x40) = *(float *)(vel + 4);
      *(float *)(particle + 0x44) = *(float *)(vel + 8);
    } else if (moved) {
      if (*(uint8_t *)tag & 0x10)
        goto kill_particle;
      *(uint8_t *)(particle + 2) |= 2;
    }
  }

  /* advance particle age */
  *(float *)(particle + 0x54) += delta_time * *(float *)(particle + 0x58);
  return 1;

kill_particle:
  particle_delete(datum_handle);
  return 0;
}

void particles_update(float delta_time)
{
  int datum_handle;
  char *datum;
  char *tag;
  bool just_created;
  float new_lifetime;

  if (profile_global_enable && *(char *)0x2ef1e8)
    profile_enter_private((void *)0x2ef1e0);

  for (datum_handle = data_next_index(particle_data, -1); datum_handle != -1;
       datum_handle = data_next_index(particle_data, datum_handle)) {
    datum = (char *)datum_get(particle_data, datum_handle);
    tag = (char *)tag_get(0x70617274, *(int *)(datum + 4));
    just_created = *(float *)(datum + 0x14) == *(float *)0x2533c0;
    if (render - *(int *)(datum + 0x10) < 0x10) {
      new_lifetime = delta_time + *(float *)(datum + 0x14);
      *(float *)(datum + 0x14) = new_lifetime;
      if (new_lifetime < *(float *)(datum + 0x18) || just_created ||
          *(int16_t *)(tag + 0x9e) != 0) {
        if (particle_step(datum_handle, delta_time))
          particle_move(datum_handle, delta_time);
      } else {
        particle_delete(datum_handle);
      }
    } else {
      datum_delete(particle_data, datum_handle);
    }
  }

  if (profile_global_enable && *(char *)0x2ef1e8)
    profile_exit_private((void *)0x2ef1e0);
}
