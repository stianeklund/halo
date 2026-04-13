void rasterizer_frame_begin(float *elapsed)
{
  char val;

  val = *(char *)0x3256c8;
  if (val < 2) {
    *(char *)0x3256d4 = val;
    *(char *)0x3256d3 = val;
    *(char *)0x3256d2 = val;
    *(char *)0x3256d1 = val;
    *(char *)0x3256d0 = val;
    *(char *)0x3256cf = val;
    *(char *)0x3256ce = val;
    *(char *)0x3256cd = val;
    *(char *)0x3256cc = val;
    *(char *)0x3256ca = val;
    *(char *)0x3256cb = val;
    *(char *)0x3256c9 = val;
    *(char *)0x3256d5 = val;
    *(char *)0x3256c8 = 2;
  }
  if (*(float *)0x325694 == *(float *)0x2533c0)
    *(int *)0x325694 = *(int *)0x2af1ac;
  if (*(float *)0x325698 == *(float *)0x2533c0)
    *(int *)0x325698 = *(int *)0x2af1b0;
  if (*(float *)0x32569c == *(float *)0x2533c0)
    *(int *)0x32569c = *(int *)0x2af1b4;
  if (*(float *)0x3256a0 == *(float *)0x2533c0)
    *(int *)0x3256a0 = *(int *)0x2af1b8;
  ((void (*)(float *))0x157940)(elapsed);
}

int rasterizer_windows_begin(void)
{
  return ((int (*)(void))0x1559d0)();
}

int rasterizer_window_begin(window_parameters_t *a1)
{
  return ((int (*)(window_parameters_t *))0x158df0)(a1);
}

void rasterizer_window_end(void)
{
  ((void (*)(void))0x158f90)();
}

void rasterizer_windows_end(void)
{
  ((void (*)(void))0x155a40)();
}

void rasterizer_frame_end(void)
{
  ((void (*)(void))0x155a70)();
}

void rasterizer_set_vblank_callback(void *cb)
{
  ((void (*)(void *))0x155c10)(cb);
}
