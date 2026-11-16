import sys,struct
sys.path.insert(0,'/mnt/g/dev/halo/tools/xbox')
from rng_trace_dump import xbdm_connect,getmem
s=xbdm_connect(sys.argv[1],731,30)
u32=lambda a: struct.unpack('<I',getmem(s,a,4))[0]
def obj_addr(h):
    t=u32(0x5a8d50); hd=getmem(s,t,0x38); sz,=struct.unpack_from('<h',hd,0x22); data,=struct.unpack_from('<I',hd,0x34)
    return struct.unpack_from('<I',getmem(s,data+(h&0xffff)*sz,sz),8)[0]
def tag_name(ti):
    e=getmem(s,u32(0x5054f0)+(ti&0xffff)*0x20,0x20); p=struct.unpack_from('<I',e,0x10)[0]
    return getmem(s,p,96).split(b'\0')[0].decode(errors='replace')
for h in sys.argv[2:]:
    a=obj_addr(int(h,16)); cur,nxt=struct.unpack('<hh',getmem(s,a+0x2a2,4)); ws=struct.unpack('<4I',getmem(s,a+0x2a8,16))
    print(f"unit {h}: current weapon index {cur}, next {nxt}")
    for i,w in enumerate(ws):
        if w==0xffffffff: continue
        print(f"  [{i}] {w:08x} {tag_name(u32(obj_addr(w)))}")
