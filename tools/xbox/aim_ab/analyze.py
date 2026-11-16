import csv,sys,math,collections
rows=[r for r in csv.DictReader(open(sys.argv[1])) if r['kind']=='tick']
S=int(sys.argv[2]); E=int(sys.argv[3])
def v(r,p): return [float(r[p+k]) for k in 'ijk']
def ang(a,b):
    c=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    return math.degrees(math.atan2(math.sqrt(sum(x*x for x in c)),sum(x*y for x,y in zip(a,b))))
for p in ('0','1'):
    pr=[r for r in rows if r['player']==p and S-5<=int(r['tick'])<=E+5]
    cnt=collections.Counter(int(r['tick']) for r in pr)
    print(f"== player {p}: {len(pr)} recs, ticks {min(cnt)}..{max(cnt)}, multi-per-tick {sum(1 for c in cnt.values() if c>1)}, gaps {sum(1 for t in range(min(cnt),max(cnt)) if t not in cnt)}")
    byt={}
    for r in pr: byt.setdefault(int(r['tick']),r)
    ts=sorted(byt)
    zero=sum(1 for t in ts if all(float(byt[t]['pos_'+k])==0 for k in 'xyz'))
    print("  pos-zero rows",zero, " targeted/magnetized rows",sum(1 for t in ts if byt[t]['target']!='ffffffff' or float(byt[t]['magnetism'])>0))
    if p!='0': continue
    print("  rel  tick  stick   r(deg/tick)  g=search_vs_facing  g/r   best_k  tgt  mag")
    ks=(-1,0,1,2,3)
    tally=collections.Counter()
    for i,t in enumerate(ts):
        r=byt[t]
        if i<3: continue
        s=v(r,'search_'); f=v(r,'facing_')
        rr=ang(v(byt[ts[i-1]],'search_'),s)
        g=float(r['search_vs_facing_deg'])
        cands={k:ang(f,v(byt[ts[i-k]],'search_')) for k in ks if 0<=i-k<len(ts)}
        bk=min(cands,key=cands.get)
        if rr>0.05: tally[bk]+=1
        rel=t-S
        if -2<=rel<=215 and (rel%3==0 or 28<=rel<=36 or 118<=rel<=126 or 148<=rel<=170):
            print(f"  {rel:4d} {t} {float(r['stick_yaw']):+.3f} {rr:9.3f} {g:9.3f}  {g/rr if rr>0.05 else float('nan'):5.2f}  k={bk} ({cands[bk]:.3f})  {r['target']} {float(r['magnetism']):.2f}")
    print("  best_k tally over moving ticks:",dict(tally))
