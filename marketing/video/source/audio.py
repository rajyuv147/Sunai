import json, sys, subprocess, numpy as np, soundfile as sf
d=sys.argv[1]; SR=48000
tl=json.load(open(d+'/timeline.json')); N=int((tl['total']+0.5)*SR)
vo=np.zeros(N); fx=np.zeros(N); mus=np.zeros(N)
def load(p):
    raw=subprocess.run(['ffmpeg','-loglevel','error','-i',p,'-ar',str(SR),'-ac','1','-f','f32le','-'],capture_output=True).stdout
    return np.frombuffer(raw,dtype=np.float32).astype(np.float64)
def add(buf,x,t,g=1.0):
    i=int(t*SR); x=x[:max(0,N-i)]; buf[i:i+len(x)]+=x*g
tt=lambda dur:np.arange(int(dur*SR))/SR
def click():
    t=tt(0.09); return (np.sin(2*np.pi*1900*t)*np.exp(-t*90)*0.6+np.sin(2*np.pi*900*t)*np.exp(-t*60)*0.4)
def chime(f0=880):
    t=tt(1.2); x=sum(np.sin(2*np.pi*f*t+0)*np.exp(-t*k)*a for f,k,a in [(f0,4,.6),(f0*1.5,5,.35),(f0*2,6,.2)])
    t2=tt(1.2); y=np.zeros(len(x)+int(.12*SR)); y[:len(x)]+=x*.6; y[int(.12*SR):]+=sum(np.sin(2*np.pi*f*t2)*np.exp(-t2*k)*a for f,k,a in [(f0*1.26,4,.6),(f0*1.89,5,.3)])*.6
    return y
vmask=np.zeros(N)
for s in tl['scenes']:
    x=load(f"{d}/vo/{s['id']}.wav"); t0=s['start']+s['vo']; add(vo,x,t0)
    vmask[int(t0*SR):int((t0+len(x)/SR)*SR)]=1
    e=s['ev']; base=s['start']+s['vo']
    taps={'home':['press_big','vol_up1','vol_up2'],'places':['talk','crowd'],'test':['yes1','yes2','no3','yes4','yes5','no6'],
          'connect':['press'],'voice':['speak'],'helper':['safety']}.get(s['id'],[])
    for k in taps: add(fx,click(),base+e[k]-0.02,0.35)
    if s['id']=='connect': add(fx,chime(988),base+e['connected'],0.22)
    if s['id']=='test': add(fx,chime(784),base+e['no6']+0.15,0.2)
    if s['id']=='intro': add(fx,chime(659),s['start']+0.05,0.22)
    if s['id']=='home':  add(fx,chime(784),base+e['press_big']+0.05,0.14)
# music: warm pad, I-V-vi-IV in C, 4 s per chord, plus a soft arpeggio
chords=[[60,64,67],[55,59,62],[57,60,64],[53,57,60]]
mf=lambda m:440*2**((m-69)/12)
t=np.arange(N)/SR; CH=4.0
for ci in range(int(N/SR/CH)+1):
    c=chords[ci%4]; a=int(ci*CH*SR); b=min(N,int((ci+1)*CH*SR+SR))
    if a>=N: break
    tl_=np.arange(b-a)/SR; env=np.minimum(1,tl_/0.8)*np.minimum(1,np.maximum(0,(CH+1-tl_))/1.0)
    for m in c+[c[0]-12]:
        f=mf(m); mus[a:b]+= (np.sin(2*np.pi*f*tl_)+0.3*np.sin(2*np.pi*2*f*tl_+0.5)+0.12*np.sin(2*np.pi*3*f*tl_))*env*0.09*(1+0.1*np.sin(2*np.pi*0.3*tl_))
    for k in range(8):  # arpeggio eighths
        st=a+int(k*CH/8*SR); 
        if st>=N: break
        m=(c+[c[0]+12])[k%4]+12; tn=tt(0.6)
        add(mus,np.sin(2*np.pi*mf(m)*tn)*np.exp(-tn*7)*0.07,st/SR)
# duck under voice (smoothed)
k=int(0.25*SR); sm=np.convolve(vmask,np.ones(k)/k,mode='same')
mg=0.55-0.33*sm
fade=np.minimum(1,t/1.5)*np.minimum(1,np.maximum(0,(tl['total']-t))/2.5)
mix=vo*0.95+fx+mus*mg*fade*0.55
mix=mix/np.abs(mix).max()*0.89
sf.write(d+'/mix.wav',np.stack([mix,mix],1).astype(np.float32),SR)
print('done',N/SR)
