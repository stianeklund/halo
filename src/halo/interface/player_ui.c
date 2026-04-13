void player_ui_dispose(void)
{
  csmemset(player_ui_globals, 0, sizeof(player_ui_globals));
}

__int16
player_ui_get_single_player_local_player_controller(__int16 local_player_index)
{
  assert_halt_msg(local_player_index >= 0 && local_player_index < 4,
                  "invalid local player index");
  return word_46BFC4[(__int16)local_player_index];
}

void player_ui_remember_player1_profile(bool save)
{
  if (*(int *)0x30f02c != *(int *)0x46bf10) {
    if (*(int *)0x46bf10 == -1) {
      error(2, "player 1 has no active player profile assigned");
    } else {
      if (!((bool (*)(int, void *))0x1c1280)(*(int *)0x46bf10,
                                             (void *)0x46c110))
        error(2, "player 1 has no active player profile assigned");
    }
    *(int *)0x30f02c = *(int *)0x46bf10;
  }
  if (save && *(char *)0x46c110)
    ((void (*)(void *))0x1c2c50)((void *)0x46c110);
}

void player_ui_initialize(void)
{
  int i;
  char *profile;

  csmemset(player_ui_globals, 0, 0x230);
  for (i = 0; i < 4; i++) {
    profile = player_ui_globals + i * 0x38;
    assert_halt(profile != NULL);
    csmemset(profile, 0, 0x30);
    *(int16_t *)(profile + 0x18) = -1;
    *(char *)(profile + 0x28) = 0;
    *(char *)(profile + 0x29) = 0;
    *(int *)(profile + 0x30) = -1;
    word_46BFC4[i] = -1;
  }
  *(int *)0x46c038 = -1;
  *(char *)0x46c10c = 1;
}
