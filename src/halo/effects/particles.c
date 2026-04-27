/* Allocate a new particle system header and initialize it (0xa1210).
 * Copies position, velocity, and tint/orientation data into the datum,
 * resolves lighting color via FUN_00139480, then runs setup (FUN_000a0fd0).
 * Returns the datum handle, or -1 on failure. */
int FUN_000a1210(int tag_index, float *position, float *velocity,
                 void *ext_data, float scale)
{
  int handle;
  char *datum;
  char local_buf[12];

  handle = data_new_at_index(particle_system_header_data);
  if (handle != -1) {
    datum = (char *)datum_get(particle_system_header_data, handle);
    *(int *)(datum + 0x08) = tag_index;
    *(int *)(datum + 0x0c) = -1;

    *(float *)(datum + 0x20) = position[0];
    *(float *)(datum + 0x24) = position[1];
    *(float *)(datum + 0x28) = position[2];

    *(float *)(datum + 0x2c) = velocity[0];
    *(float *)(datum + 0x30) = velocity[1];
    *(float *)(datum + 0x34) = velocity[2];

    *(float *)(datum + 0x38) = ((float *)ext_data)[0];
    *(float *)(datum + 0x3c) = ((float *)ext_data)[1];
    *(float *)(datum + 0x40) = ((float *)ext_data)[2];
    *(float *)(datum + 0x44) = ((float *)ext_data)[3];

    *(float *)(datum + 0x14) = scale;
    *(uint32_t *)(datum + 0x04) |= 1;

    FUN_00139480((void *)(datum + 0x20), (void *)(datum + 0x48), local_buf, 0);

    if (!FUN_000a0fd0(handle)) {
      datum_delete(particle_system_header_data, handle);
      return -1;
    }
  }
  return handle;
}

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

/* Check whether a 3D point has all finite float components (0x0a16b0).
 * Returns true if none of the three components are NaN or infinity
 * (IEEE 754 exponent field != 0x7f800000). */
bool valid_real_point3d(float *point)
{
  uint32_t *p = (uint32_t *)point;
  if ((p[0] & 0x7f800000) == 0x7f800000)
    return false;
  if ((p[1] & 0x7f800000) == 0x7f800000)
    return false;
  if ((p[2] & 0x7f800000) == 0x7f800000)
    return false;
  return true;
}

/* TODO: particle_delete also reverted — see git 08bf664 for implementation */

/* TODO: particle_step and particle_move temporarily reverted to original
 * binary due to a stale datum handle crash during gameplay. These need
 * debugging — the implementations are preserved in git history (commit
 * 08bf664). The issue manifests as "particle index #X is unused or changed"
 * in datum_get after loading a campaign level. */

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
        {
          /* particle_step at 0xa1b60: EDI=datum_handle, stack=delta_time */
          int _edi = datum_handle;
          int _dt = *(int *)&delta_time;
          int _result;
          asm volatile("pushl %[dt]\n\t"
                       "movl $0xa1b60, %%eax\n\t"
                       "call *%%eax\n\t"
                       "addl $4, %%esp"
                       : "+D"(_edi), "=a"(_result)
                       : [dt] "r"(_dt)
                       : "ecx", "edx", "memory", "cc");
          if ((char)_result)
            ((bool (*)(int, float))0xa1c30)(datum_handle, delta_time);
        }
      } else {
        {
          /* particle_delete at 0xa18c0: EBX=datum_handle */
          int _ebx = datum_handle;
          asm volatile("movl $0xa18c0, %%eax\n\t"
                       "call *%%eax"
                       : "+b"(_ebx)
                       :
                       : "eax", "ecx", "edx", "esi", "edi", "memory", "cc");
        }
      }
    } else {
      datum_delete(particle_data, datum_handle);
    }
  }

  if (profile_global_enable && *(char *)0x2ef1e8)
    profile_exit_private((void *)0x2ef1e0);
}
