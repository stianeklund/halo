
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
