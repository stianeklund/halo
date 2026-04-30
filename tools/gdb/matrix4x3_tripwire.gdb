source .gdbinit

define dump_matrix4x3_debug
  printf "matrix4x3_debug_trip_count = %d\n", matrix4x3_debug_trip_count
  printf "A:\n"
  p matrix4x3_debug_last_a
  printf "B:\n"
  p matrix4x3_debug_last_b
  printf "OUT:\n"
  p matrix4x3_debug_last_out
end

break matrix4x3_debug_break
commands
  silent
  printf "\n[matrix4x3 tripwire]\n"
  dump_matrix4x3_debug
  bt 8
end
