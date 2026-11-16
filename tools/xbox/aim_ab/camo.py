"""usage: camo.py HOST show | set UNITHANDLE CAMO(float) [bit 0/1]"""
import sys,struct
sys.path.insert(0,'/mnt/g/dev/halo/tools/xbox')
from rng_trace_dump import xbdm_connect,getmem
from aim_assist_debug import setmem
s=xbdm_connect(sys.argv[1],731,30)
u32=lambda a: struct.unpack('<I',getmem(s,a,4))[0]
def table(ptr):
    h=getmem(s,ptr,0x38); mx,sz=struct.unpack_from('<hh',h,0x20); cur,=struct.unpack_from('<h',h,0x2e); data,=struct.unpack_from('<I',h,0x34)
    return mx,sz,cur,data
def obj_addr(handle):
    mx,sz,cur,data=table(u32(0x5a8d50)); idx=handle&0xffff
    hdr=getmem(s,data+idx*sz,sz); return struct.unpack_from('<I',hdr,8)[0]
def unit_info(handle):
    a=obj_addr(handle)
    pos=struct.unpack('<3f',getmem(s,a+0x5c-0xc,12)) if False else struct.unpack('<3f',getmem(s,a+0x50,12))
    flags=u32(a+0x1b4); camo=struct.unpack('<f',getmem(s,a+0x32c,4))[0]
    return a,pos,flags,camo
if sys.argv[2]=='show':
    mx,sz,cur,data=table(u32(0x5aa6d4))
    print('regen/tick',struct.unpack('<f',getmem(s,0x28ac20,4))[0])
    for i in range(mx):
        p=getmem(s,data+i*sz,sz)
        salt,=struct.unpack_from('<H',p,0)
        if salt==0: continue
        lp,=struct.unpack_from('<h',p,2); unit,=struct.unpack_from('<I',p,0x34); timers=struct.unpack_from('<hh',p,0x68)
        line=f"player {i} salt {salt:04x} local {lp} unit {unit:08x} powerup_timers {timers}"
        if unit!=0xffffffff:
            a,pos,flags,camo=unit_info(unit); line+=f" obj@{a:08x} pos ({pos[0]:.3f},{pos[1]:.3f},{pos[2]:.3f}) flags1b4 {flags:08x} camo {camo:.3f}"
        print(line)
else:
    if sys.argv[3].startswith('lp'):
        want=int(sys.argv[3][2:]); mx,sz,cur,data=table(u32(0x5aa6d4))
        for i in range(mx):
            pr=getmem(s,data+i*sz,sz)
            if struct.unpack_from('<H',pr,0)[0] and struct.unpack_from('<h',pr,2)[0]==want: h=struct.unpack_from('<I',pr,0x34)[0]
    else: h=int(sys.argv[3],16)
    a=obj_addr(h)
    if len(sys.argv)>5:
        f=u32(a+0x1b4); f=(f|0x10) if sys.argv[5]=='1' else (f&~0x10)
        setmem(s,a+0x1b4,struct.pack('<I',f))
    setmem(s,a+0x32c,struct.pack('<f',float(sys.argv[4])))
    print(unit_info(h))
