/* 0x20f80 — Check whether an actor meets conditions for a given combat mode.
 * mode 1: actor.field_60c==1 and field_268>=8
 * mode 2: actor.field_60c==0, field_268>=5, field_27c!=0, field_278>=0x4b
 * mode 3: mode 1 conditions plus field_161!=0 */
int actor_combat_check_mode(int actor_handle /* @<eax> */, short mode)
{
  char *actor = (char *)datum_get(*(void **)0x6325a4, actor_handle);

  if (mode == 1) {
    if (*(short *)(actor + 0x60c) == 1 && *(short *)(actor + 0x268) >= 8)
      return 1;
  } else if (mode == 2) {
    if (*(short *)(actor + 0x60c) == 0 && *(short *)(actor + 0x268) >= 5 &&
        *(char *)(actor + 0x27c) != 0 && *(int *)(actor + 0x278) >= 0x4b)
      return 1;
  } else if (mode == 3) {
    if (*(short *)(actor + 0x60c) == 1 && *(short *)(actor + 0x268) >= 8 &&
        *(char *)(actor + 0x161) != 0)
      return 1;
  }
  return 0;
}

char *FUN_000211f0(int actor_handle)
{
  char *actor = (char *)datum_get(*(data_t **)0x6325a4, actor_handle);
  char *actv = (char *)tag_get(0x61637476, *(int *)(actor + 0x5c));
  int weapon_handle = FUN_0003b270(actor_handle);
  if (weapon_handle != -1) {
    int *obj = (int *)object_get_and_verify_type(weapon_handle, 4);
    char *weap = (char *)tag_get(0x77656170, *obj);
    if (weap != 0 && *(int *)(weap + 0x3c8) != -1) {
      actv = (char *)tag_get(0x61637476, *(int *)(weap + 0x3c8));
    }
  }
  return actv;
}
