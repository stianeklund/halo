/* Invoke the stack trace logger. The depth parameter is incremented
 * by 1 to skip this wrapper's own frame. */
char stack_walk(int16_t a1)
{
  return ((char (*)(int, int, int))0x92460)(0, (int)a1 + 1, 0);
}
