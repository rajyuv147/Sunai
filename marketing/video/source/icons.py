import glob, json, os, re, sys, xml.etree.ElementTree as ET
A='{http://schemas.android.com/apk/res/android}'
def col(c):
    if not c or c.startswith('@'): return None
    c=c.lstrip('#')
    if len(c)==8:
        a=int(c[:2],16)
        if a==0: return 'none'
        return '#'+c[2:]
    return '#'+c
def conv(el, tint):
    out=[]
    for ch in el:
        tag=ch.tag
        if tag=='group':
            tr=[]
            tx=ch.get(A+'translateX','0'); ty=ch.get(A+'translateY','0')
            sx=ch.get(A+'scaleX','1'); sy=ch.get(A+'scaleY','1')
            out.append(f'<g transform="translate({tx} {ty}) scale({sx} {sy})">'+conv(ch,tint)+'</g>')
        elif tag=='path':
            f=col(ch.get(A+'fillColor')) or 'none'
            s=col(ch.get(A+'strokeColor')) or 'none'
            if tint:
                if f!='none': f='currentColor'
                if s!='none': s='currentColor'
            sw=ch.get(A+'strokeWidth','0')
            lc=ch.get(A+'strokeLineCap','butt'); lj=ch.get(A+'strokeLineJoin','miter')
            fr=ch.get(A+'fillType','nonZero')
            out.append(f'<path d="{ch.get(A+"pathData")}" fill="{f}" stroke="{s}" stroke-width="{sw}" stroke-linecap="{lc}" stroke-linejoin="{lj}" fill-rule="{"evenodd" if fr=="evenOdd" else "nonzero"}"/>')
    return ''.join(out)
res={}
for p in sorted(glob.glob(sys.argv[1]+'/ic_*.xml')):
    r=ET.parse(p).getroot()
    if r.tag!='vector': continue
    vb=f"0 0 {r.get(A+'viewportWidth')} {r.get(A+'viewportHeight')}"
    n=os.path.basename(p)[:-4]
    for tint in (True,False):
        res[n+('' if tint else '_raw')]=f'<svg viewBox="{vb}" xmlns="http://www.w3.org/2000/svg">'+conv(r,tint)+'</svg>'
open(sys.argv[2],'w').write('const ICONS='+json.dumps(res)+';')
print(len(res), sorted(res)[:50])
