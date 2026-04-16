# Beta-XBE assertion tripwire.
#
# cachebeta.xbe keeps live assertions: every precondition check the
# original authors wrote funnels through display_assert at 0x8d9f0.
# When the 4th arg (severity) is non-zero, display_assert calls
# stack_walk — i.e. it is a hard failure, the game is wrong here.
#
# This script breaks on every severity!=0 assertion and prints the
# message, file, line, and a short backtrace, then continues. Any
# lift that violates an original invariant (bad field offset, bad
# arg, stale pointer, failed init order, non-finite float, etc.)
# lights this up immediately instead of crashing later on a
# downstream page fault.
#
# Usage (xemu must be running with -s GDB stub on :1234):
#     gdb -x tools/asserts.gdb
#
# To stop at the first hit for interactive debugging, remove the
# trailing `continue` in the commands block below.

set pagination off
set print pretty on
set disassembly-flavor intel
set arch i386
add-symbol-file build/halo 0x643000 -s .rdata 0x655000 -s .data 0x65d000 -s .thunks 0x65e000
target remote 127.0.0.1:1234

# display_assert(const char *message, const char *file, int line, char severity)
# At function entry (before PUSH EBP), args live at [ESP+4..ESP+10].
break *0x8d9f0 if *(char*)($esp+0x10) != 0
commands
  silent
  printf "\n[ASSERT] severity=%d  %s:%d\n", \
    *(char*)($esp+0x10), \
    *(char**)($esp+8), \
    *(int*)($esp+0xc)
  printf "         message: %s\n", \
    ((*(char**)($esp+4)) ? *(char**)($esp+4) : "(no message)")
  bt 10
  printf "\n"
  continue
end

printf "\n[tripwire] armed at 0x8d9f0 (display_assert, severity!=0)\n"
printf "[tripwire] type 'continue' to run\n\n"
