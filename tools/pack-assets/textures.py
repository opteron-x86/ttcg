"""Original, deterministic paper, woven cloth and stamped-ink texture assets."""
from pathlib import Path
import math
import subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/pack-assets/textures'
DEST = ROOT / 'textures/Tessera/packs'
RNG = np.random.default_rng(18427)
FONT = '/usr/share/fonts/liberation/LiberationSerif-Regular.ttf'


def field(w, h):
    out = np.zeros((h, w), dtype=np.float32)
    for size, weight in ((8, 0.4), (32, 0.22), (128, 0.15), (512, 0.12)):
        a = RNG.integers(0, 256, (max(2, int(size*h/w)), size), dtype=np.uint8)
        out += (np.asarray(Image.fromarray(a).resize((w, h), Image.Resampling.BICUBIC), dtype=np.float32)/255-.5)*weight
    return out


def paper(w, h, color):
    noise = field(w, h)
    fine = RNG.normal(0, .9, (h, w)).astype(np.float32)
    rgb = np.clip(np.array(color)[None, None, :] + noise[:, :, None]*42 + fine[:, :, None], 0, 255)
    height = noise*12 + fine*.18
    return rgb.astype(np.uint8), height


def diamond(draw, cx, cy, rx, ry, fill=None, outline=None, width=1):
    pts = [(cx, cy-ry), (cx+rx, cy), (cx, cy+ry), (cx-rx, cy)]
    draw.polygon(pts, fill=fill)
    if outline is not None:
        draw.line(pts+[pts[0]], fill=outline, width=width, joint='curve')


def star(draw, cx, cy, r, fill=255):
    pts=[]
    for i in range(16):
        angle=-math.pi/2+i*math.pi/8
        radius=r if i%4==0 else r*.56 if i%2==0 else r*.18
        pts.append((cx+math.cos(angle)*radius,cy+math.sin(angle)*radius))
    draw.polygon(pts, fill=fill)


def tracked(draw, text, cy, size, tracking, width, center=.5):
    font=ImageFont.truetype(FONT,size)
    span=sum(font.getlength(c) for c in text)+tracking*(len(text)-1)
    x=width*center-span/2
    for c in text:
        draw.text((x,cy),c,font=font,fill=255,stroke_width=0)
        x+=font.getlength(c)+tracking


def ornament(w, h, tournament):
    img=Image.new('L',(w,h));d=ImageDraw.Draw(img)
    # Fine engraved border; the cord and folds remain visible over the print.
    for inset,line in ((70,4),(87,2),(108,1)):
        d.rectangle((inset,inset,w-inset,h-inset),outline=215,width=line)
    for x,y in ((88,88),(w-88,88),(88,h-88),(w-88,h-88)):
        diamond(d,x,y,42,42,fill=0,outline=240,width=4)
        star(d,x,y,28,230)
    tracked(d,'TESSERA',230,132,19,w)
    d.line((260,426,w-260,426),fill=190,width=2)
    diamond(d,w/2,426,16,12,fill=230)
    cx,cy=w*.5,h*.485
    r=285
    for margin,stroke in ((0,6),(18,2),(35,2)):
        diamond(d,cx,cy,r+margin,r+margin,outline=210,width=stroke)
    # Nine tiles recall the playing board. These are new vector ornaments.
    for row in range(-1,2):
        for col in range(-1,2):
            x=cx+(col-row)*70; y=cy+(col+row)*70
            diamond(d,x,y,63,63,fill=60,outline=200,width=3)
            if row==0 and col==0:star(d,x,y,51)
            elif (row+col)%2==0:star(d,x,y,24,180)
            else:diamond(d,x,y,11,22,fill=140)
    # Short flourishes keep the package ornament readable at inventory size.
    for sign in (-1,1):
        x=cx+sign*(r+64)
        diamond(d,x,cy,17,36,outline=220,width=3)
        d.line((x+sign*25,cy,x+sign*95,cy),fill=150,width=2)
        diamond(d,cx,cy+sign*(r+70),15,28,fill=180)
    if tournament:
        # Paired laurel branches are drawn into the gilt, not separate geometry.
        for side in (-1,1):
            pts=[]
            for t in np.linspace(-.7,.8,70):
                pts.append((cx+side*(330+65*math.cos(t*2)),cy+310*t))
            d.line(pts,fill=225,width=4)
            for t in np.linspace(-.6,.7,11):
                x=cx+side*(330+65*math.cos(t*2));y=cy+310*t
                d.polygon([(x,y+11),(x+side*47,y-21),(x+side*58,y-67),(x+side*16,y-37)],fill=175)
        tracked(d,'TOURNAMENT',1640,72,10,w)
        d.line((320,1770,w-320,1770),fill=170,width=2)
        star(d,w/2,1840,36,230)
    else:
        for x in (w/2-67,w/2,w/2+67):diamond(d,x,1755,13,20,fill=180)
    return img


def save_maps(name, rgb, height, spec):
    WORK.mkdir(parents=True,exist_ok=True);DEST.mkdir(parents=True,exist_ok=True)
    color=Image.fromarray(np.uint8(np.clip(rgb,0,255)))
    color.save(WORK/f'{name}.png')
    gy,gx=np.gradient(height)
    n=np.stack((-gx*.7,-gy*.7,np.ones_like(gx)),axis=2)
    n/=np.linalg.norm(n,axis=2,keepdims=True)
    normal=np.concatenate((np.clip((n*.5+.5)*255,0,255),spec[:,:,None]),axis=2).astype(np.uint8)
    Image.fromarray(normal,'RGBA').save(WORK/f'{name}_n.png')
    for suffix,compression in (('', 'dxt1'),('_n','dxt5')):
        subprocess.run(['magick',str(WORK/f'{name}{suffix}.png'),'-define',f'dds:compression={compression}',
                        str(DEST/f'{name}{suffix}.dds')],check=True)


def face(tournament):
    w,h=1536,2048
    rgb,height=paper(w,h,(28,45,36) if tournament else (43,65,46))
    yy,xx=np.mgrid[0:h,0:w]
    # Rubbed edges and subtle diagonal pressure lines from the folded wrapper.
    edge=np.minimum.reduce([xx,w-1-xx,yy,h-1-yy]).astype(np.float32)
    wear=np.exp(-edge/32)*(.7+np.maximum(field(w,h),0)*6)
    rgb=rgb.astype(np.float32)+wear[:,:,None]*np.array([34,24,12])
    crease=np.exp(-np.abs(yy-(150+xx*.12))/6)*.55
    crease+=np.exp(-np.abs(yy-(h-160-xx*.10))/5)*.45
    height+=crease*1.8
    ink=np.asarray(ornament(w,h,tournament),dtype=np.float32)/255
    grain=field(w,h)
    ink*=np.clip(.93+grain*1.2,0,1)
    ink*=np.where(RNG.random((h,w))<.015,.3,1)
    gold=np.stack((182+grain*35,147+grain*30,81+grain*20),axis=2)
    rgb=rgb*(1-ink[:,:,None])+gold*ink[:,:,None]
    height+=ink*1.4
    spec=np.clip(27+ink*155,0,255)
    # A square atlas keeps complete mip chains while preserving the printed aspect on the mesh.
    resize=lambda a:np.asarray(Image.fromarray(a.astype(np.float32),'F').resize((2048,2048),Image.Resampling.BICUBIC))
    rgb=np.stack([resize(rgb[:,:,c]) for c in range(3)],axis=2)
    save_maps('tournament' if tournament else 'standard',rgb,resize(height),resize(spec))


def materials():
    colors=[(41,62,44),(27,43,34),(152,126,83),(168,115,44),
            (104,34,27),(166,136,75),(185,150,78),(75,62,37)]
    specs=[29,29,32,170,180,125,190,35]
    rgb=np.zeros((1024,2048,3),np.float32)
    height=np.zeros((1024,2048),np.float32);spec=np.zeros_like(height)
    y,x=np.mgrid[:512,:512]
    for i,(base,shine) in enumerate(zip(colors,specs)):
        color,bump=paper(512,512,base)
        color=color.astype(np.float32)
        if i in (2,5):
            weave=np.sin(x*(.6 if i==5 else .3)+np.sin(y*.3)*.7)*np.cos(y*.9)
            color+=weave[:,:,None]*7
            bump+=weave*1.6
        elif i in (3,4):
            # Wax has broad, shallow tool marks rather than paper grain.
            bump*=.16
            bump+=np.sin(np.sqrt((x-256)**2+(y-256)**2)*.12)*.15
        row,col=divmod(i,4)
        rgb[row*512:(row+1)*512,col*512:(col+1)*512]=color
        height[row*512:(row+1)*512,col*512:(col+1)*512]=bump
        spec[row*512:(row+1)*512,col*512:(col+1)*512]=shine
    save_maps('materials',rgb,height,spec)


if __name__=='__main__':
    face(False);face(True);materials()
    print('Authored 3 colour maps and 3 normal/specular maps with DDS mip chains')
