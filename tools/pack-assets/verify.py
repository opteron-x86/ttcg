"""Check reopened NIF geometry, physical bounds, texture references and DDS mip data."""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from pack_assets import PACK_BOUNDS, pack_runtime_files


def check():
    report={}
    for name in ('standard','tournament'):
        original=json.loads((ROOT/f'build/pack-assets/{name}.json').read_text())
        actual=json.loads((ROOT/f'build/pack-assets/{name}-nif.json').read_text())
        assert actual['sse'] and actual['root']=='BSFadeNode'
        assert actual['bsx']==194 and actual['inventory']['zoom']==1
        assert actual['inventory']['rotation']==original['inventoryRotation']==[1250,0,3302]
        c=actual['collision']
        assert c['target']==0 and c['layer']==4 and c['flags']==129
        assert c['motion']==4 and c['quality']==4 and c['mass']>0
        assert all(c['inertia'][i]>0 for i in (0,5,10))
        half=c['halfExtents']
        assert all(abs(half[i]*69.99125-original['collisionHalfExtents'][i])<.001 for i in range(3))
        assert len(original['meshes'])==len(actual['meshes'])
        tris=0;verts=0;paths=set();norm_error=0
        for before,after in zip(original['meshes'],actual['meshes']):
            assert before['name']==after['name']
            assert before['triangles']==after['triangles']
            assert len(before['vertices'])==len(after['vertices'])==len(after['uvs'])==len(after['normals'])
            for p,q,n,oldn,uv,olduv in zip(before['vertices'],after['vertices'],after['normals'],before['normals'],after['uvs'],before['uvs']):
                assert all(math.isfinite(v) for v in (*q,*n,*uv))
                assert max(abs(p[i]-q[i]) for i in range(3))<.001
                assert max(abs(uv[i]-olduv[i]) for i in range(2))<.001
                norm_error=max(norm_error,max(abs(n[i]-oldn[i]) for i in range(3)))
                assert .97<math.sqrt(sum(v*v for v in n))<1.02
                assert all(PACK_BOUNDS[i]<=q[i]<=PACK_BOUNDS[i+3] for i in range(3))
            for t in after['triangles']:
                assert len(set(t))==3 and min(t)>=0 and max(t)<len(after['vertices'])
                p,q,r=(after['vertices'][i] for i in t)
                a=[q[i]-p[i] for i in range(3)];b=[r[i]-p[i] for i in range(3)]
                area=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
                assert sum(v*v for v in area)>1e-15,(name,after['name'],t)
            assert after['flags1'] & (1<<31) and after['flags2'] & 1
            assert not after['flags1'] & 2 # rigid, not skinned
            assert set(after['textures'])=={'0','1'}
            for path in after['textures'].values():
                relative=Path(path.replace('\\','/'))
                assert not relative.is_absolute() and '..' not in relative.parts
                assert (ROOT/relative).is_file()
                paths.add(str(relative))
            tris+=len(after['triangles']);verts+=len(after['vertices'])
        assert norm_error<.009
        report[name]={'triangles':tris,'vertices':verts,'materials':len(actual['meshes']),
                      'maximumNormalQuantizationError':norm_error,'textures':sorted(paths),
                      'collisionHalfExtents':original['collisionHalfExtents']}
    dds=[]
    for name,path in pack_runtime_files(ROOT).items():
        if path.suffix!='.dds':continue
        b=path.read_bytes()
        assert b[:4]==b'DDS ' and struct.unpack_from('<I',b,4)[0]==124
        h,w=struct.unpack_from('<II',b,12);mips=struct.unpack_from('<I',b,28)[0]
        assert mips==1+int(math.log2(max(h,w)))
        fourcc=b[84:88];expected=b'DXT5' if path.stem.endswith('_n') else b'DXT1'
        assert fourcc==expected
        size=128;bw,bh=w,h
        for _ in range(mips):
            size+=max(1,(bw+3)//4)*max(1,(bh+3)//4)*(16 if fourcc==b'DXT5' else 8)
            bw=max(1,bw//2);bh=max(1,bh//2)
        assert len(b)==size,(name,len(b),size)
        dds.append({'path':name,'width':w,'height':h,'mips':mips,'compression':fourcc.decode()})
    report['dds']=dds
    report['sha256']={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in pack_runtime_files(ROOT).items()}
    (ROOT/'build/pack-assets/verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Pack assets verified: reopened SSE geometry, normals, UVs, bounds, collision, 6 DDS textures and complete mip chains')
    return report


if __name__=='__main__':check()
