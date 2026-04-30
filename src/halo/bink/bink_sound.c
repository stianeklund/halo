/* Return the DirectSound handle if sound is initialized, else 0 (0x1c94b0). */
int bink_get_dsound_handle(void)
{
  return *(char *)0x4fdbc0 ? *(int *)0x50545c : 0;
}
