void real_math_reset_precision(void)
{
  __control87(0x9001f, 0xfffff);
}
