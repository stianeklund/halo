import sys,struct
sys.path.insert(0,'/mnt/g/dev/halo/tools/xbox')
from rng_trace_dump import xbdm_connect,getmem
s=xbdm_connect(sys.argv[1],731,30)
u32=lambda a: struct.unpack('<I',getmem(s,a,4))[0]
want=sys.argv[2:]
for i in range(0,1200):
    p=u32(0x2f3708+i*4)
    if p<0x10000 or p>0x1000000: break
    e=getmem(s,p,12); name_p,typ,addr=struct.unpack('<IhxxI',e)
    try: name=getmem(s,name_p,48).split(b'\0')[0].decode()
    except Exception: continue
    if name in want: print(name, 'type',typ,'addr',hex(addr),'value',getmem(s,addr,4).hex())
