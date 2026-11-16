import sys,struct,math,time
sys.path.insert(0,'/mnt/g/dev/halo/tools/xbox')
from rng_trace_dump import xbdm_connect,getmem
def cmd(s,c):
    s.sendall((c+"\r\n").encode()); return s.recv(1024).decode(errors='replace').strip()
def ang(a,b):
    c=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    return math.degrees(math.atan2(math.sqrt(sum(x*x for x in c)),sum(x*y for x,y in zip(a,b))))
s=xbdm_connect(sys.argv[1],731,30)
for n in range(int(sys.argv[2])):
    r=cmd(s,"stop"); time.sleep(0.05)
    rg=getmem(s,0x506540,0x28)
    fi,=struct.unpack_from('<i',rg,0); lp,=struct.unpack_from('<h',rg,8)
    rf=struct.unpack_from('<3f',rg,0x1c)
    obs=[struct.unpack('<3f',getmem(s,0x33571c+i*0x29c+0x74+0x20,12)) for i in (0,1)]
    print(f"frame {fi} render.lp {lp}  render_vs_obs0 {ang(rf,obs[0]):.3f}  render_vs_obs1 {ang(rf,obs[1]):.3f}  obs0 {tuple(round(x,4) for x in obs[0])}")
    cmd(s,"go"); time.sleep(0.37)
