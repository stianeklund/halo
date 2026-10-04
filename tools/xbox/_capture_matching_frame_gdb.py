"""GDB worker for capture_matching_frame.py (Halo Xbox debug 2276 only).

Reads the rendered backbuffer before Present. Optional Carousel alignment uses
original engine routines plus verified control/facing fields. Inferior calls
temporarily use scratch on the paused main thread's stack, restored afterward.
"""
import gdb
import json
import math
import os
import struct
import time
from pathlib import Path

root = Path(os.environ['HALO_BARRIER_DIR'])
label = os.environ['HALO_BARRIER_LABEL']

def read(address, size):
    return bytes(gdb.selected_inferior().read_memory(address, size))

def u32(address):
    return struct.unpack('<I', read(address, 4))[0]

def local_players():
    pg = u32(0x5aa6cc)
    pool = u32(0x5aa6d4)
    player_base = u32(pool + 0x34)
    stride = struct.unpack('<H', read(pool + 0x22, 2))[0]
    headers = u32(u32(0x5a8d50) + 0x34)
    pc = u32(0x457090)
    players = []
    for slot in range(4):
        handle = u32(pg + 4 + slot * 4)
        if handle == 0xffffffff:
            continue
        unit_handle = u32(player_base + (handle & 0xffff) * stride + 0x34)
        if unit_handle == 0xffffffff:
            raise RuntimeError('Local player has no living unit')
        unit = u32(headers + (unit_handle & 0xffff) * 12 + 8)
        control = pc + 0x10 + slot * 0x40
        weapons = []
        for index in range(4):
            weapon_handle = u32(unit + 0x2a8 + index * 4)
            if weapon_handle == 0xffffffff:
                continue
            weapon = u32(headers + (weapon_handle & 0xffff) * 12 + 8)
            tag = u32(weapon)
            tags = u32(0x5054f0)
            name = read(u32(tags + (tag & 0xffff) * 0x20 + 0x10), 128)
            weapons.append({'index': index, 'tag': name.split(b'\0')[0].decode('ascii')})
        active_weapon = struct.unpack('<h', read(unit + 0x2a2, 2))[0]
        active_tag = next((w['tag'] for w in weapons if w['index'] == active_weapon), None)
        players.append({'slot': slot, 'unit_handle': unit_handle, 'unit_address': unit,
                        'control_address': control,
                        'position': list(struct.unpack('<3f', read(unit + 0xc, 12))),
                        'yaw_pitch': list(struct.unpack('<2f', read(control + 0xc, 8))),
                        'velocity': list(struct.unpack('<3f', read(unit + 0x18, 12))),
                        'fp_header': read(u32(0x46bea8) + slot * 0x1ea0, 0x90).hex(),
                        'active_weapon_index': active_weapon, 'active_weapon_tag': active_tag,
                        'weapons': weapons})
    return players

def apply_poses():
    players = local_players()
    if len(players) != 2:
        raise RuntimeError('Pose alignment requires exactly two local players')
    poses = [([-4.661903381347656, -11.369110107421875, -0.8562036156654358],
              1.1448506116867065),
             ([5.810912609100342, 3.6055307388305664, -2.723989963531494],
              0.7155909538269043)]
    # Scratch above the paused main-thread ESP is restored before resuming it.
    # Inferior calls run on a lower stack; none of this scratch becomes persistent.
    scratch = int(gdb.parse_and_eval('$esp')) + 0x100
    saved = read(scratch, 36)
    try:
        for player, (position, yaw) in zip(players, poses):
            forward = [math.cos(yaw), math.sin(yaw), 0.0]
            data = struct.pack('<9f', *(position + forward + [0.0, 0.0, 1.0]))
            gdb.selected_inferior().write_memory(scratch, data)
            gdb.execute('call ((void(*)(int,float*,float*,float*))0x143ae0)'
                        '(%#x,(float*)%#x,(float*)%#x,(float*)%#x)' %
                        (player['unit_handle'], scratch, scratch + 12, scratch + 24))
            gdb.execute('call ((void(*)(short,float*))0xb6ea0)(%d,(float*)%#x)' %
                        (player['slot'], scratch + 12))
            unit = player['unit_address']
            inferior = gdb.selected_inferior()
            inferior.write_memory(unit + 0x18, bytes(12))
            for offset in (0x1d4, 0x1e0, 0x204):
                inferior.write_memory(unit + offset, struct.pack('<3f', *forward))
            rifle = next(w['index'] for w in player['weapons'] if 'assault rifle' in w['tag'])
            inferior.write_memory(player['control_address'] + 0x20, struct.pack('<h', rifle))
    finally:
        gdb.selected_inferior().write_memory(scratch, saved)
    write('-alignment.json', {'before': players, 'after': local_players(),
                              'method': 'engine object_set_position + player_control_set_facing',
                              'target_poses': poses})

def resolve(address):
    prologue = read(address, 6)
    if prologue[0] == 0xe9:
        return address + 5 + struct.unpack('<i', prologue[1:5])[0]
    if prologue[0] == 0x68 and prologue[5] == 0xc3:
        return struct.unpack('<I', prologue[1:5])[0]
    return address

def capture_rendered_backbuffer():
    # Match rasterizer_present's own screenshot path, before Present flips buffers.
    scratch = int(gdb.parse_and_eval('$esp')) + 0x100
    saved = read(scratch, 64)
    surface = None
    try:
        inferior = gdb.selected_inferior()
        inferior.write_memory(scratch, bytes(64))
        gdb.execute('call ((int(*)(int,unsigned int,void**))0x1e7d50)'
                    '(0,0,(void**)%#x)' % scratch)
        surface = u32(scratch)
        if surface == 0:
            raise RuntimeError('GetBackBuffer returned NULL')
        gdb.execute('call ((int(*)(void*,void*))0x1ef1e0)((void*)%#x,(void*)%#x)' %
                    (surface, scratch + 4))
        desc = list(struct.unpack('<7I', read(scratch + 4, 28)))
        # Xbox D3DSURFACE_DESC: Format, Type, Usage, Size, MultiSampleType, Width, Height.
        if desc[0] != 18 or desc[5:] != [640, 480] or desc[3] != 640 * 480 * 4:
            raise RuntimeError('Unsupported backbuffer descriptor: ' + repr(desc))
        gdb.execute('call ((int(*)(void*,void*,void*,unsigned int))0x1ef200)'
                    '((void*)%#x,(void*)%#x,(void*)0,0xc0)' % (surface, scratch + 32))
        pitch, bits = struct.unpack('<II', read(scratch + 32, 8))
        if pitch != 2560 or bits == 0:
            raise RuntimeError('Unsupported locked backbuffer: ' + repr((pitch, bits)))
        # In xemu the physical/virtual debug read invokes the NV2A surface access
        # callback; it downloads dirty GPU surfaces and waits for that download.
        data = read(bits, pitch * 480)
        (root / (label + '-rendered.bin')).write_bytes(data)
        return {'surface': hex(surface), 'descriptor': desc, 'pitch': pitch,
                'bits': hex(bits), 'source': 'current rendered backbuffer before Present',
                'byte_count': len(data)}
    finally:
        if surface:
            gdb.execute('call ((unsigned int(*)(void*))0x1ed930)((void*)%#x)' % surface)
        gdb.selected_inferior().write_memory(scratch, saved)

def sample():
    gt = u32(0x45708c)
    gg = u32(0x4566ec)
    return {
        'tick': u32(gt + 0xc),
        'game_time_bytes': read(gt, 0x20).hex(),
        'map': read(gg + 0x14, 256).split(b'\0')[0].decode('ascii', 'replace'),
        'variant': read(0x456af8, 104).hex(),
        'present_counter': struct.unpack('<Q', read(0x325668, 8))[0],
        'flip_counter': struct.unpack('<Q', read(0x325678, 8))[0],
        'render_timing_bits': read(0x325694, 8).hex(),
        'eip': hex(int(gdb.parse_and_eval('$eip'))),
        'render_camera': list(struct.unpack('<6f', read(0x506550, 24))),
        'local_players': local_players(),
    }

def write(suffix, value):
    target = root / (label + suffix)
    temporary = target.with_suffix(target.suffix + '.partial')
    temporary.write_text(json.dumps(value, indent=2))
    temporary.replace(target)

def wait_for(path, timeout=45):
    deadline = time.monotonic() + timeout
    while not path.exists():
        if path.name != 'release.json' and (root / 'release.json').exists():
            raise RuntimeError('Coordinator cancelled the capture')
        if time.monotonic() > deadline:
            raise RuntimeError('Timeout waiting for ' + str(path))
        time.sleep(0.05)
    return json.loads(path.read_text())

class PresentTick(gdb.Breakpoint):
    def __init__(self, address, target):
        super().__init__('*%#x' % address, internal=True)
        self.target = target
    def stop(self):
        return u32(u32(0x45708c) + 0xc) >= self.target

try:
    initial = sample()
    write('-initial.json', initial)
    target = int(wait_for(root / 'target.json')['tick'])
    if initial['tick'] >= target:
        raise RuntimeError('Target must be ahead of the initial game tick')
    entry = resolve(0x157e40)
    if os.environ.get('HALO_ALIGN_POSES') == '1':
        breakpoint = gdb.Breakpoint('*%#x' % entry, temporary=True, internal=True)
        gdb.execute('continue')
        if int(gdb.parse_and_eval('$eip')) != entry:
            raise RuntimeError('Interrupted before initial pose alignment')
        apply_poses()
        camera_update = resolve(0x875f0)
        breakpoint = PresentTick(camera_update, target)
        gdb.execute('continue')
        if int(gdb.parse_and_eval('$eip')) != camera_update:
            raise RuntimeError('Interrupted before target camera update')
        breakpoint.delete()
        if u32(u32(0x45708c) + 0xc) != target:
            raise RuntimeError('Skipped target camera-update tick')
        apply_poses()
        # Establish a common first-person animation start for this visual fixture.
        # These are the original game's map initialization/update routines.
        gdb.execute('call ((void(*)(void))0xdc7a0)()')
        gdb.execute('call ((void(*)(void))0xdeb60)()')
        write('-weapon-reset.json', {'tick': target, 'players': local_players(),
                                     'method': 'first_person_weapons_initialize_for_new_map + update'})
    breakpoint = PresentTick(entry, target)
    gdb.execute('continue')
    if int(gdb.parse_and_eval('$eip')) != entry:
        raise RuntimeError('Interrupted before the presentation entry')
    breakpoint.delete()
    before = sample()
    if before['tick'] != target or before['map'] != initial['map']:
        raise RuntimeError('Target tick skipped or map changed')
    backbuffer = capture_rendered_backbuffer()
    return_pc = u32(int(gdb.parse_and_eval('$esp')))
    breakpoint = gdb.Breakpoint('*%#x' % return_pc, temporary=True, internal=True)
    gdb.execute('continue')
    if int(gdb.parse_and_eval('$eip')) != return_pc:
        raise RuntimeError('Interrupted before the presentation return')
    after = sample()
    if after['tick'] != target:
        raise RuntimeError('Tick changed during presentation')
    if after['present_counter'] != before['present_counter'] + 1:
        raise RuntimeError('Presentation counter did not increment exactly once')
    write('-boundary.json', {'entry': hex(entry), 'return_pc': hex(return_pc),
                             'before': before, 'after': after,
                             'backbuffer': backbuffer,
                             'capture_phase': 'rendered backbuffer immediately before Present'})
    wait_for(root / 'release.json')
except Exception as error:
    write('-error.json', {'error': str(error)})
    raise
finally:
    for breakpoint in gdb.breakpoints() or []:
        breakpoint.delete()
    gdb.execute('detach')
