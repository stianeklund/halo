import csv,sys,math
rows=list(csv.DictReader(open(sys.argv[1])))
sc=[r for r in rows if r['scripted']=='1' and r['player']=='0']
ts=[int(r['tick']) for r in sc]; st=[i for i in range(len(ts)) if i==0 or ts[i]-ts[i-1]>1][-1]; t0=ts[st]
p1={int(r['tick']):[float(r['pos_'+k]) for k in 'xyz'] for r in rows if r['kind']=='tick' and r['player']=='1'}
p0={int(r['tick']):r for r in rows if r['kind']=='tick' and r['player']=='0'}
xs=[p1[t] for t in range(t0,t0+121) if t in p1]
if xs: print(f"p1 lateral range over script: x {min(p[0] for p in xs):.2f}..{max(p[0] for p in xs):.2f}  y {min(p[1] for p in xs):.2f}..{max(p[1] for p in xs):.2f}")
tk=[p0[t] for t in range(t0,t0+121) if t in p0]
print('p0 ticks with target',sum(r['target']!='ffffffff' for r in tk),'/',len(tk))
for r in rows:
    if r['kind']!='shot' or not (t0<=int(r['tick'])<=t0+125): continue
    t=int(r['tick']); dy=float(r['final_yaw'])-float(r['aim_yaw'])
    o=p0[t]; eye=[float(o['pos_'+k]) for k in 'xyz']
    tgt_yaw=None; vel=None
    if t in p1:
        d=[a-b for a,b in zip(p1[t],eye)]; tgt_yaw=math.degrees(math.atan2(d[1],d[0]))
        if t-1 in p1:
            v=[(a-b)*30 for a,b in zip(p1[t],p1[t-1])]
            # angular velocity of target as seen from p0 (deg/s), yaw
            yaw_prev=math.degrees(math.atan2(p1[t-1][1]-eye[1],p1[t-1][0]-eye[0])); vel=(tgt_yaw-yaw_prev)*30
    print(f"shot rel {t-t0:3d} tgt {r['target']} aim_yaw {float(r['aim_yaw']):+.2f} -> {float(r['final_yaw']):+.2f} (bend yaw {dy:+.3f}, total {float(r['shot_bend_deg']):.3f})"
          + (f" | target yaw {tgt_yaw:+.2f}, target moving {vel:+.1f} deg/s" if tgt_yaw is not None and vel is not None else ""))
