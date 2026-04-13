void player_control_dispose(void)
{
}

void player_control_new_unit(uint16_t local_player_index, int player_index)
{
  int *slot;
  float *facing;
  int unit;

  assert_halt(local_player_index >= 0 &&
              local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS);
  slot =
    (int *)((char *)player_control_globals + local_player_index * 0x40 + 0x10);
  csmemset(slot, 0, 0x40);
  *slot = player_index;
  *(int16_t *)(slot + 8) = -1;
  *(int16_t *)((char *)slot + 0x22) = -1;
  *(int16_t *)(slot + 9) = -1;
  *(char *)((char *)slot + 0x26) = 0;
  slot[10] = -1;
  *(float *)(slot + 0xf) = 1.49f;
  *(float *)(slot + 0xe) = -1.49f;
  *(int16_t *)(slot + 2) = 0;
  *(int16_t *)((char *)slot + 10) = 0;
  if (player_index != -1) {
    unit = (int)object_get_and_verify_type(player_index, 3);
    facing = (float *)(slot + 3);
    ((void (*)(float *, int))0x10cc00)(facing, unit + 0x1d4);
    if (*facing < *(float *)0x2533c0)
      *facing += *(float *)0x255a54;
    *(int16_t *)(slot + 8) = *(int16_t *)(unit + 0x2a4);
    *(int16_t *)((char *)slot + 0x22) = (int16_t) * (char *)(unit + 0x2cd);
    *(int16_t *)(slot + 9) = (int16_t) * (char *)(unit + 0x2d1);
  }
}

void player_control_initialize_for_new_map(void)
{
  int i;
  int iVar;
  int scenario;

  *(int *)player_control_globals = 0;
  *((int *)player_control_globals + 1) = 0;
  *((int *)player_control_globals + 2) = 0;
  *((int *)player_control_globals + 3) = 0;
  for (i = 0; (int16_t)i < 4; i++) {
    scenario = ((int (*)(void))0x18e450)();
    iVar =
      ((int (*)(int *, int, int))0x19b210)((int *)(scenario + 0x110), 0, 0x80);
    player_control_new_unit(i, -1);
    if (*(float *)((char *)0x4570a8 + i * 4) == *(float *)0x2533c0)
      *(int *)((char *)0x4570a8 + i * 4) = *(int *)(iVar + 0x4c);
    if (*(float *)((char *)0x457098 + i * 4) == *(float *)0x2533c0)
      *(int *)((char *)0x457098 + i * 4) = *(int *)(iVar + 0x50);
  }
}

void player_control_update(float delta_time)
{
  int16_t i;

  if (profile_global_enable && *(char *)0x2f02a0)
    profile_enter_private((void *)0x2f0298);
  collision_log_begin_period(2);
  ((void (*)(void))0xb8f70)();
  for (i = 0; i < 4; i++)
    player_control_get_facing(i, delta_time);
  collision_log_end_period();
  if (profile_global_enable && *(char *)0x2f02a0)
    profile_exit_private((void *)0x2f0298);
}
