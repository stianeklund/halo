/* Return true if any projectile object (type 0x20) exists in the world.
 * Loads the projectile's tag definition as a side effect (cache priming). */
bool dangerous_projectiles_near_player(void)
{
  char iter[16];
  void *projectile;

  ((void (*)(void *, int, int))0x13d6f0)(iter, 0x20, 0);
  projectile = ((void *(*)(void *))0x13d730)(iter);
  if (projectile != NULL) {
    tag_get(0x70726f6a, *(int *)projectile);
    return true;
  }
  return false;
}
