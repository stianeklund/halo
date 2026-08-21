/* 0xe0620. No xrefs found; the first two stack arguments are never read, so
 * their types are unknown. The third argument is a pointer to two floats that
 * are cached verbatim and also converted to a halved, registration-relative
 * short position used to build a rectangle at 0x46bebc. */
/* Field roles inferred from use: index 0/2 take the y-derived coordinate and
 * index 1/3 the x-derived one, matching the engine's {top,left,bottom,right}
 * rectangle ordering. The 8-byte copy at the end is what the reference emits
 * as two dword stores. */
typedef struct short_rectangle2d {
  short top;
  short left;
  short bottom;
  short right;
} short_rectangle2d;

void FUN_000e0620(int unknown0, int unknown1, float *position)
{
  short_rectangle2d bounds;
  short x;
  short y;
  int registration;

  if (*(char *)0x46beb0 == 0)
    return;

  *(float *)0x46bec4 = position[0];
  *(float *)0x46bec8 = position[1];

  x = (short)(int)position[0];
  x = (short)(x >> 1);
  registration = *(int *)0x46bed8;
  x = (short)(x - *(short *)(registration + 0x10));

  y = (short)(int)position[1];
  y = (short)(y >> 1);
  y = (short)(y - *(short *)(registration + 0x12));

  *(short *)0x46bece = y;
  *(short *)0x46becc = x;

  bounds.top = (short)(y + *(short *)0x46beb2);
  bounds.left = (short)(x + *(short *)0x46beb4);
  bounds.bottom = (short)(y + *(short *)0x46beb6);
  bounds.right = (short)(x + *(short *)0x46beb8);

  if (*(char *)0x46beba != 0 && local_time_get() - *(int *)0x46bed0 <= 7)
    return;

  *(short_rectangle2d *)0x46bebc = bounds;
  *(int *)0x46bed0 = local_time_get();
  *(char *)0x46bebb = 0;
}

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
