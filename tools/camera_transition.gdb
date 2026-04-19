# Camera-transition trace for temporary third-person -> first-person blends.
#
# Usage:
#   1. Build a Debug XBE so `.gdbinit` exists and points at the current image.
#   2. Start xemu with the GDB stub enabled on :1234.
#   3. From the repo root:
#        gdb -x tools/camera_transition.gdb
#   4. Reproduce a bad transition (vehicle exit, debug/free-camera return,
#      third-person -> first-person toggle).
#
# After the first 0x89cd0 handoff capture, the script disables its breakpoints
# and detaches automatically so the VM resumes at full speed.
#
# This script focuses on the narrow transition path with minimal runtime
# overhead:
#   0x864b0  classify resume path from third-person follow camera
#   0x865a0  install camera fn
#   0x8acb0  first director -> observer handoff after 0x89cd0 install
#
# The goal is to answer three questions quickly:
#   - Why did 0x864b0 choose transition camera vs direct first person?
#   - Did we install 0x89cd0 (transition camera) or jump straight to 0x89270?
#   - Did the camera output block contain the expected transition flags/timers?
#   - Did the first 0x89cd0 handoff already contain transition data?

set pagination off
set print pretty on
set disassembly-flavor intel
set arch i386

# Reuse the auto-generated symbol/target setup from the current Debug build.
source .gdbinit

define dump_transition_block
  set $blk = (unsigned char *)$arg0
  if $blk == 0
    printf "  transition block: (null)\n"
  else
    printf "  transition block @ %p\n", $blk
    printf "    valid=0x%02x general=%g\n", *$blk, *(float *)($blk + 0x48)
    printf "    flags[4c..50]=[%u %u %u %u %u]\n", \
      *(unsigned char *)($blk + 0x4c), \
      *(unsigned char *)($blk + 0x4d), \
      *(unsigned char *)($blk + 0x4e), \
      *(unsigned char *)($blk + 0x4f), \
      *(unsigned char *)($blk + 0x50)
    printf "    timers[54..64]=[%g %g %g %g %g]\n", \
      *(float *)($blk + 0x54), \
      *(float *)($blk + 0x58), \
      *(float *)($blk + 0x5c), \
      *(float *)($blk + 0x60), \
      *(float *)($blk + 0x64)
  end
end

document dump_transition_block
Dump the 0x68-byte director camera result tail that carries transition flags
and timers into the observer path.
end

define dump_observer_transition
  set $obs = (unsigned char *)$arg0
  if $obs == 0
    printf "  observer: (null)\n"
  else
    printf "  observer @ %p source=%p updated=%u\n", \
      $obs, *(void **)($obs + 0x4), *(unsigned char *)($obs + 0x70)
    printf "    observer timers[5c..6c]=[%g %g %g %g %g]\n", \
      *(float *)($obs + 0x5c), \
      *(float *)($obs + 0x60), \
      *(float *)($obs + 0x64), \
      *(float *)($obs + 0x68), \
      *(float *)($obs + 0x6c)
  end
end

document dump_observer_transition
Dump the observer-side transition timer bank after staging/integration.
end

set $trace_transition_player = -1
set $trace_transition_armed = 0

define dump_transition_unit
  set $unit = (unsigned char *)$arg0
  if $unit == 0
    printf "  unit: (null)\n"
  else
    printf "  unit @ %p parent=0x%x seat=%d state_253=0x%02x\n", \
      $unit, *(int *)($unit + 0xcc), *(short *)($unit + 0x2a0), \
      *(unsigned char *)($unit + 0x253)
  end
end

document dump_transition_unit
Dump the unit state that 0x864b0 uses to classify the transition path.
end

# Inline datum resolution: avoids inferior function calls that crash GDB
# on remote stubs.  data_t layout: size @ +0x22, data ptr @ +0x34.
# object_header_data_t: object ptr @ +0x08.
define resolve_unit_from_handle
  set $ruh_handle = $arg0
  set $ruh_table = *(unsigned char **)0x5a8d50
  set $ruh_index = ($ruh_handle & 0xffff)
  set $ruh_elem_size = *(short *)($ruh_table + 0x22)
  set $ruh_data = *(unsigned char **)($ruh_table + 0x34)
  set $ruh_hdr = $ruh_data + $ruh_elem_size * $ruh_index
  set $resolved_unit = *(unsigned char **)($ruh_hdr + 0x08)
end

break *0x864b0
commands
  silent
  set $unit_handle = *(int *)($esp + 4)
  set $out_word_ptr = (short *)*(int *)($esp + 8)
  printf "\n[0x864b0 entry] unit_handle=0x%x out_word_ptr=%p\n", $unit_handle, $out_word_ptr
  if $unit_handle != -1
    resolve_unit_from_handle $unit_handle
    dump_transition_unit $resolved_unit
  end
  continue
end

break *0x865a0 if *(void **)($esp + 4) == (void *)0x89cd0 || *(void **)($esp + 4) == (void *)0x89270
commands
  silent
  printf "\n[0x865a0] player=%u install=%p reset_top=%u\n", \
    ($esi & 0xffff), *(void **)($esp + 4), *(unsigned char *)($esp + 8)
  if $out_word_ptr != 0
    printf "  [0x864b0 result] out_word=%d\n", *$out_word_ptr
  end
  if *(void **)($esp + 4) == (void *)0x89cd0
    set $trace_transition_player = ($esi & 0xffff)
    set $trace_transition_armed = 1
    printf "  armed first 0x8acb0 handoff capture for player=%u\n", $trace_transition_player
  end
  continue
end

break *0x8acb0
commands
  silent
  set $player = (*(unsigned short *)($esp + 4) & 0xffff)
  set $ps = (unsigned char *)(0x3352b4 + $player * 0xf8)
  if $trace_transition_armed && $player == $trace_transition_player && *(void **)($ps + 4) == (void *)0x89cd0
    set $blk = (unsigned char *)*(void **)($esp + 8)
    printf "\n[0x8acb0 first 0x89cd0 handoff] player=%u camera=%p block=%p\n", \
      $player, *(void **)($ps + 4), $blk
    dump_transition_block $blk
    set $trace_transition_armed = 0
    set $trace_transition_player = -1
    printf "  one-shot capture complete; disabling breakpoints and detaching\n"
    delete breakpoints
    detach
  else
    continue
  end
end

printf "\n[camera-transition] armed breakpoints on 0x864b0, 0x865a0, and one-shot 0x8acb0 capture\n"
printf "[camera-transition] type 'continue' and reproduce a bad transition\n\n"
