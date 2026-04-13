void xbox_demos_launch(void)
{
  char launch_data[0xc00];

  if (((bool (*)(void))0xe0430)()) {
    csmemset(launch_data, 0, sizeof(launch_data));
    ((void (*)(void))0xe04a0)();
    ((uint32_t(*)(const char *, void *))0x1d25e0)("d:\\XDemos\\XDemos.xbe",
                                                  launch_data);
    ((uint32_t(*)(const char *, void *))0x1d25e0)(0, launch_data);
    while (true) {
    }
  }

  error(2, "XDEMOS not found!");
}
