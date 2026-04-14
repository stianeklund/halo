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
        particle_delete(datum_handle);
      }
    } else {
      datum_delete(particle_data, datum_handle);
    }
  }

  if (profile_global_enable && *(char *)0x2ef1e8)
    profile_exit_private((void *)0x2ef1e0);
}
