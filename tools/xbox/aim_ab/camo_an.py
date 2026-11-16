import csv,sys
rows=list(csv.DictReader(open(sys.argv[1])))
sc=[r for r in rows if r['scripted']=='1' and r['player']=='0']
ts=[int(r['tick']) for r in sc]
st=[i for i in range(len(ts)) if i==0 or ts[i]-ts[i-1]>1][-1]
t0=ts[st]; t1=ts[-1]
print(f"script ticks {t0}..{t1}")
ticks=[r for r in rows if r['player']=='0' and r['kind']=='tick' and t0<=int(r['tick'])<=t1]
shots=[r for r in rows if r['player']=='0' and r['kind']=='shot' and t0<=int(r['tick'])<=t1+5]
tg=sum(r['target']!='ffffffff' for r in ticks)
print(f"ticks {len(ticks)}  with target {tg}  max autoaim_level {max(float(r['autoaim_level']) for r in ticks):.3f}  max magnetism {max(float(r['magnetism']) for r in ticks):.3f}")
for i in range(0,len(ticks),10):
    r=ticks[i]; print(f"  rel {int(r['tick'])-t0:4d} tgt {r['target']} aa {float(r['autoaim_level']):.3f} mag {float(r['magnetism']):.3f}")
for r in shots:
    print(f"  SHOT rel {int(r['tick'])-t0:4d} tgt {r['target']} bend {float(r['shot_bend_deg']):.3f} deg")
print("shots:",len(shots))
