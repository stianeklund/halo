# Camera-transition trace for vehicle exit — watchpoint edition.
#
# Usage:
#   gdb -nx -x tools/camera_transition_watch_snapshot.gdb
#   continue
#   press X to exit the vehicle
#
# bp1 captures unit address, sets a hardware watchpoint on unit+0x253,
# then continues. Each write to state_253 is logged with a backtrace.
# bp2 captures the camera install decision.

set pagination off
set disassembly-flavor intel
set arch i386
add-symbol-file build/halo 0x643000 -s .rdata 0x661000 -s .data 0x66d000 -s .thunks 0x66e000
target remote 127.0.0.1:1234

python

import gdb

def resolve_unit(handle):
    tbl = int(gdb.parse_and_eval("*(unsigned int *)0x5a8d50"))
    idx = handle & 0xffff
    esz = int(gdb.parse_and_eval("*(short *)%d" % (tbl + 0x22)))
    dat = int(gdb.parse_and_eval("*(unsigned int *)%d" % (tbl + 0x34)))
    hdr = dat + esz * idx
    obj = int(gdb.parse_and_eval("*(unsigned int *)%d" % (hdr + 0x08)))
    return obj

def read_u8(addr):
    return int(gdb.parse_and_eval("*(unsigned char *)%d" % addr)) & 0xff

def read_i16(addr):
    return int(gdb.parse_and_eval("*(short *)%d" % addr))

def read_u32(addr):
    return int(gdb.parse_and_eval("*(unsigned int *)%d" % addr))

def read_float(addr):
    return float(gdb.parse_and_eval("*(float *)%d" % addr))

saved_handle = [0]
saved_unit = [0]
wp = [None]
write_count = [0]

class ExitStateBP(gdb.Breakpoint):
    def stop(self):
        ebp = int(gdb.parse_and_eval("$ebp"))
        uh = read_u32(ebp + 8)
        u = resolve_unit(uh)
        saved_handle[0] = uh
        saved_unit[0] = u

        parent = read_u32(u + 0xcc)
        seat = read_i16(u + 0x2a0)
        state = read_u8(u + 0x253)

        print("\n[bp1 0x1b367a] state_253 write")
        print("  unit_handle=0x%x unit=0x%x" % (uh, u))
        print("  parent=0x%x seat=%d state_253=0x%02x" % (parent, seat, state))

        state_addr = u + 0x253
        print("  setting watchpoint on *0x%x (unit+0x253)" % state_addr)
        wp[0] = StateWatchpoint("*(unsigned char *)0x%x" % state_addr)
        write_count[0] = 0

        self.enabled = False
        bp2.enabled = True
        return False

class StateWatchpoint(gdb.Breakpoint):
    def __init__(self, expr):
        super().__init__(expr, type=gdb.BP_WATCHPOINT, wp_class=gdb.WP_WRITE)

    def stop(self):
        write_count[0] += 1
        u = saved_unit[0]
        new_val = read_u8(u + 0x253)
        eip = int(gdb.parse_and_eval("$eip"))

        print("\n[watch #%d] state_253 written = 0x%02x at eip=0x%x" % (
            write_count[0], new_val, eip))

        try:
            bt = gdb.execute("bt 6", to_string=True)
            print(bt)
        except:
            print("  (backtrace unavailable)")

        if new_val == 0x00:
            print("  *** state_253 cleared to 0x00 — this is the bug writer ***")

        return False

class CameraInstallBP(gdb.Breakpoint):
    def stop(self):
        esp = int(gdb.parse_and_eval("$esp"))
        install_fn = read_u32(esp + 4)
        reset_top = read_u8(esp + 8)

        uh = saved_handle[0]
        u = resolve_unit(uh)
        parent = read_u32(u + 0xcc)
        seat = read_i16(u + 0x2a0)
        state = read_u8(u + 0x253)

        print("\n[bp2 0x865a0] camera install")
        print("  install_fn=0x%x reset_top=%d" % (install_fn, reset_top))
        print("  parent=0x%x seat=%d state_253=0x%02x" % (parent, seat, state))
        print("  total state_253 writes observed: %d" % write_count[0])

        if install_fn == 0x89cd0:
            print("  -> TRANSITION camera (expected)")
        elif install_fn == 0x89270:
            print("  -> DIRECT first-person (bug path)")

        if wp[0]:
            wp[0].delete()
        gdb.execute("delete breakpoints")
        gdb.execute("detach")
        return True

bp1 = ExitStateBP("*0x1b367a")
bp2 = CameraInstallBP("*0x865a0")
bp2.enabled = False

print("\n[camera-trace] armed. Type 'continue', then press X to exit vehicle.\n")

end
