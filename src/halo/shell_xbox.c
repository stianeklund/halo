void shell_idle(void)
{
}

/* update_loaded_module_section_attributes (0x191260)
 *
 * Debug-only helper: iterates loaded modules and their sections via two
 * kernel enumeration thunks at 0x253080/0x253084/0x253088, finds the
 * section that contains this function, and marks it PAGE_READONLY via a
 * NtProtectVirtualMemory wrapper (0x1d0cfb). NTSTATUS sentinel 0x82db0104
 * signals end-of-enumeration.
 *
 * All four entry points are __stdcall — confirmed by the original disasm
 * which has zero `ADD ESP,N` cleanup after any of the calls. A cdecl decl
 * here makes Clang double-pop the stack (caller cleanup + callee `ret N`),
 * corrupting locals/return-addr and faulting deep in a kernel call.
 *
 * Section VirtualAddress lives at section_buf[+0x104], VirtualSize at +0x108
 * (XBEIMAGE_SECTION with a 256-byte preamble). Inner buffer is 280 bytes. */

typedef int(__stdcall *fn_module_next_t)(int *handle, void *module_buf);
typedef int(__stdcall *fn_section_next_t)(int *handle, void *module_buf,
                                          void *section_buf);
typedef void(__stdcall *fn_close_t)(int handle);
typedef int(__stdcall *fn_virt_protect_t)(void *base, unsigned int size,
                                          unsigned int new_protect,
                                          unsigned int *old_protect);

#define ENUM_NO_MORE 0x82db0104

void update_loaded_module_section_attributes(void)
{
  fn_section_next_t section_next = *(fn_section_next_t *)0x253080;
  fn_module_next_t module_next = *(fn_module_next_t *)0x253084;
  fn_close_t obj_close = *(fn_close_t *)0x253088;
  fn_virt_protect_t virt_protect = (fn_virt_protect_t)0x1d0cfb;

  unsigned char module_buf[280];
  unsigned char section_buf[280];
  int outer_handle = 0;
  int inner_handle = 0;
  unsigned int old_protect;
  int status;

  status = module_next(&outer_handle, module_buf);
  if (status == (int)ENUM_NO_MORE)
    return;

  do {
    inner_handle = 0;
    status = section_next(&inner_handle, module_buf, section_buf);

    while (status != (int)ENUM_NO_MORE) {
      unsigned int va = *(unsigned int *)(section_buf + 0x104);
      unsigned int sz = *(unsigned int *)(section_buf + 0x108);

      if (va < (unsigned int)update_loaded_module_section_attributes &&
          (unsigned int)update_loaded_module_section_attributes < va + sz) {
        virt_protect((void *)va, sz, 0x2, &old_protect);
        obj_close(inner_handle);
        break;
      }

      status = section_next(&inner_handle, module_buf, section_buf);
    }

    status = module_next(&outer_handle, module_buf);
  } while (status != (int)ENUM_NO_MORE);
}

int main(int argc, const char **argv, const char **envp)
{
#if DEBUG_BUILD
  update_loaded_module_section_attributes();
#endif
  rasterizer_preinitialize();
  physical_memory_allocate();
  if (shell_initialize()) {
#ifndef TEST_HARNESS
    main_loop();
#else
    extern void run_tests(void);
    run_tests();
    for (;;) { }
#endif
    shell_dispose();
  }
  return 0;
}
