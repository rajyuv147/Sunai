import sherpa_onnx, soundfile as sf, numpy as np, json, sys, glob
d, script, out, sid, speed = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), float(sys.argv[5])
onnx=glob.glob(d+'/*.onnx')[0]
tts=sherpa_onnx.OfflineTts(sherpa_onnx.OfflineTtsConfig(model=sherpa_onnx.OfflineTtsModelConfig(
    vits=sherpa_onnx.OfflineTtsVitsModelConfig(model=onnx,tokens=d+'/tokens.txt',data_dir=d+'/espeak-ng-data'),num_threads=4)))
durs={}
for s in json.load(open(script)):
    a=tts.generate(s['say'],sid=sid,speed=speed)
    x=np.array(a.samples,dtype=np.float32)
    # trim edge silence
    nz=np.where(np.abs(x)>0.01)[0]; x=x[max(0,nz[0]-200):nz[-1]+2000]
    x=x/ max(1e-6,np.abs(x).max())*0.89
    sf.write(f"{out}/{s['id']}.wav",x,a.sample_rate)
    durs[s['id']]=len(x)/a.sample_rate
    print(s['id'], round(durs[s['id']],2), a.sample_rate)
json.dump(durs,open(out+'/durs.json','w'))
