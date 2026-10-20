import json, re, sys
d=sys.argv[1]
script=json.load(open(d+'/script.json')); durs=json.load(open(d+'/vo/durs.json'))
# per-scene UI events (seconds after the voice starts) -> drive both picture and sound effects
EV={
 'home':   {'press_big':2.1,'vol_up1':5.2,'vol_up2':6.0},
 'places': {'talk':0.6,'crowd':1.3,'home_back':99},
 'test':   {'play1':0.3,'yes1':1.3,'play2':1.8,'yes2':2.6,'play3':3.1,'no3':4.0,'play4':4.4,'yes4':5.0,'play5':5.4,'yes5':5.9,'play6':6.2,'no6':6.7},
 'connect':{'press':0.6,'connected':2.4},
 'voice':  {'speak':3.6},
 'trust':  {},
 'helper': {'unlock':0.2,'safety':3.4},
}
LEAD=0.45; TAIL=0.75
t=0.0; scenes=[]
for i,s in enumerate(script):
    vo=durs[s['id']]
    lead = 0.9 if i==0 else LEAD
    dur = lead+vo+TAIL + (1.6 if s['id']=='outro' else 0)
    sents=[x.strip() for x in re.split(r'(?<=[।])\s*', s['say']) if x.strip()]
    tot=sum(len(x) for x in sents); subs=[]; c=0
    for x in sents:
        a=lead+vo*c/tot; c+=len(x); b=lead+vo*c/tot
        subs.append([round(a,3),round(b,3),x])
    scenes.append({'id':s['id'],'start':round(t,3),'dur':round(dur,3),'vo':lead,'voDur':vo,'subs':subs,'ev':EV.get(s['id'],{})})
    t+=dur
tl={'total':round(t,3),'scenes':scenes}
open(d+'/timeline.js','w').write('const TL='+json.dumps(tl,ensure_ascii=False)+';')
json.dump(tl,open(d+'/timeline.json','w'),ensure_ascii=False,indent=1)
print('total',round(t,2))
