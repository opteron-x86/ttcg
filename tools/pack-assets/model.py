"""Build the pack meshes, or render geometry read back from their shipped NIFs.

Run with Blender --background --factory-startup --python model.py -- build|preview.
"""
from pathlib import Path
import json
import math
import sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'build/pack-assets'
PREVIEW=ROOT/'build/previews'
TEXTURES=ROOT/'textures/Tessera/packs'
W,H,D=8.0,11.0,.96


def clear():
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.materials):bpy.data.materials.remove(m)


def material(name, texture, atlas=-1, gloss=20, spec=.45):
    if name in bpy.data.materials:return bpy.data.materials[name]
    m=bpy.data.materials.new(name);m.use_nodes=True
    m['texture']=texture;m['atlas']=atlas;m['glossiness']=gloss;m['specularStrength']=spec
    nodes=m.node_tree.nodes;links=m.node_tree.links
    bsdf=nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value=.7 if gloss<30 else .38
    coordinates=None
    if atlas>=0:
        # The editable .blend uses the same atlas placement as the NIF export.
        uv=nodes.new('ShaderNodeTexCoord')
        low=nodes.new('ShaderNodeVectorMath');low.operation='MAXIMUM';low.inputs[1].default_value=(0,0,0)
        high=nodes.new('ShaderNodeVectorMath');high.operation='MINIMUM';high.inputs[1].default_value=(1,1,1)
        mapping=nodes.new('ShaderNodeVectorMath');mapping.operation='MULTIPLY_ADD'
        mapping.inputs[1].default_value=(.8/4,.8/2,1)
        mapping.inputs[2].default_value=((atlas%4+.1)/4,(1-atlas//4+.1)/2,0)
        links.new(uv.outputs['UV'],low.inputs[0]);links.new(low.outputs['Vector'],high.inputs[0])
        links.new(high.outputs['Vector'],mapping.inputs[0]);coordinates=mapping.outputs['Vector']
    for suffix in ('','_n'):
        t=nodes.new('ShaderNodeTexImage')
        if coordinates is not None:links.new(coordinates,t.inputs['Vector'])
        t.image=bpy.data.images.load(str(TEXTURES/f'{texture}{suffix}.dds'),check_existing=True)
        t.interpolation='Linear'
        if not suffix:links.new(t.outputs['Color'],bsdf.inputs['Base Color'])
        else:
            t.image.colorspace_settings.name='Non-Color'
            # Skyrim uses DirectX tangent normals; invert green for Blender.
            sep=nodes.new('ShaderNodeSeparateColor');links.new(t.outputs['Color'],sep.inputs['Color'])
            inv=nodes.new('ShaderNodeMath');inv.operation='SUBTRACT';inv.inputs[0].default_value=1
            links.new(sep.outputs['Green'],inv.inputs[1])
            combine=nodes.new('ShaderNodeCombineColor')
            links.new(sep.outputs['Red'],combine.inputs['Red'])
            links.new(inv.outputs[0],combine.inputs['Green'])
            links.new(sep.outputs['Blue'],combine.inputs['Blue'])
            norm=nodes.new('ShaderNodeNormalMap');norm.inputs['Strength'].default_value=.6
            links.new(combine.outputs['Color'],norm.inputs['Color']);links.new(norm.outputs['Normal'],bsdf.inputs['Normal'])
            strength=nodes.new('ShaderNodeMath');strength.operation='MULTIPLY';strength.inputs[1].default_value=spec
            links.new(t.outputs['Alpha'],strength.inputs[0]);links.new(strength.outputs[0],bsdf.inputs['Specular IOR Level'])
    return m


def uvmap(obj):
    mesh=obj.data
    uv=mesh.uv_layers.new(name='UVMap') if not mesh.uv_layers else mesh.uv_layers.active
    for face in mesh.polygons:
        for loop in face.loop_indices:
            p=mesh.vertices[mesh.loops[loop].vertex_index].co
            axis=max(range(3),key=lambda a:abs(face.normal[a]))
            uv.data[loop].uv=((p.y/H+.5,p.z/D+.5) if axis==0 else
                              (p.x/W+.5,p.z/D+.5) if axis==1 else
                              (p.x/W+.5,p.y/H+.5))


def mesh_obj(name, verts, faces, mat, smooth=False):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    obj.data.materials.append(mat)
    for face in mesh.polygons:face.use_smooth=smooth
    uvmap(obj)
    return obj


def bevel(obj,width=.05,segments=2):
    mod=obj.modifiers.new('Soft hand-folded edges','BEVEL');mod.width=width;mod.segments=segments
    mod=obj.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL');mod.keep_sharp=True


def tube(name,points,radius,mat,closed=False):
    # Parallel-transport tube frames keep seams and topology deterministic.
    verts=[];faces=[];sides=8;n=len(points)
    for i,p in enumerate(points):
        p=Vector(p)
        tangent=(Vector(points[(i+1)%n])-Vector(points[i-1 if i else (-1 if closed else 0)])).normalized()
        if tangent.length<.1:tangent=Vector((1,0,0))
        ref=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))
        side=tangent.cross(ref).normalized();up=tangent.cross(side).normalized()
        for j in range(sides):
            a=j*math.tau/sides
            verts.append(p+radius*(math.cos(a)*side+math.sin(a)*up))
    for i in range(n if closed else n-1):
        for j in range(sides):faces.append((i*sides+j,((i+1)%n)*sides+j,((i+1)%n)*sides+(j+1)%sides,i*sides+(j+1)%sides))
    if not closed:faces.extend((tuple(reversed(range(sides))),tuple((n-1)*sides+j for j in range(sides))))
    return mesh_obj(name,verts,faces,mat,True)


def ring_path(half,depth,n=64):
    # Rounded rectangle in a cross section, matching the softened paper body.
    r=.17;pts=[]
    for cx,cz,start in ((half-r,depth-r,0),(-half+r,depth-r,90),(-half+r,-depth+r,180),(half-r,-depth+r,270)):
        for k in range(n//4):
            a=math.radians(start+90*k/(n//4-1))
            # Top right -> top left -> lower left -> lower right.
            x=cx+r*math.cos(a);z=cz+r*math.sin(a)
            pts.append((x,z))
    return pts


def strap(name,axis,offset,width,mat):
    half=W/2+.045 if axis=='x' else H/2+.045
    profile=ring_path(half,D/2+(.17 if axis=='x' else .14),32)
    verts=[]
    for p,z in profile:
        for side in (-1,1):
            verts.append((p,offset+side*width/2,z) if axis=='x' else (offset+side*width/2,p,z))
    faces=[];n=len(profile)
    for i in range(n):faces.append((2*i,2*((i+1)%n),2*((i+1)%n)+1,2*i+1))
    obj=mesh_obj(name,verts,faces,mat,True)
    solid=obj.modifiers.new('Woven band thickness','SOLIDIFY');solid.thickness=.034
    bevel(obj,.013,2)
    return obj


def polygon_stamp(name,points,z,mat,height=.025):
    n=len(points);v=[(x,y,z) for x,y in points]+[(x,y,z+height) for x,y in points]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    o=mesh_obj(name,v,faces,mat);bevel(o,.006,1);return o


def seal(tournament,wax,gold):
    cx,cy=2.13,-2.12
    n=56;v=[];f=[]
    # Different radial waves give the perimeter a poured, irregular outline.
    for radius,z in ((.04,.84),(.48,.84),(.535,.9),(.625,.87),(.79,.665),(.735,.615)):
        for i in range(n):
            a=math.tau*i/n
            ripple=1+.042*math.sin(5*a+.6)+.029*math.sin(9*a+2)+.016*math.sin(17*a)
            v.append((cx+radius*math.cos(a)*ripple,cy+radius*math.sin(a)*ripple,z+.008*math.sin(7*a)))
    f.append(tuple(range(n)))
    for ring in range(5):
        for i in range(n):f.append((ring*n+i,(ring+1)*n+i,(ring+1)*n+(i+1)%n,ring*n+(i+1)%n))
    f.append(tuple(reversed(range(5*n,6*n))))
    mesh_obj('Poured wax seal',v,f,wax,True)
    stampmat=gold if tournament else wax
    # A small inset diamond and five tiles form the stamped Tessera device.
    radius=.355
    pts=[(cx+radius*math.sin(a),cy+radius*math.cos(a),.851) for a in [math.pi/2*k for k in range(5)]]
    tube('Seal impression border',pts,.012,stampmat)
    for dx,dy in ((0,0),(-.145,0),(.145,0),(0,-.145),(0,.145)):
        r=.066
        polygon_stamp('Seal tessera',[(cx+dx-r,cy+dy),(cx+dx,cy+dy-r),(cx+dx+r,cy+dy),(cx+dx,cy+dy+r)],.853,stampmat,.016)


def build(tournament):
    name='tournament' if tournament else 'standard'
    plain=material(name+' paper','materials',1 if tournament else 0)
    front=material(name+' printed face',name)
    cord=material('Linen cord','materials',2,16,.3)
    band=material('Woven gold band','materials',5,28,.6)
    gold=material('Antique gold stamp','materials',6,45,.85)
    wax=material('Crimson wax' if tournament else 'Ochre wax','materials',4 if tournament else 3,58,.9)
    bpy.ops.mesh.primitive_cube_add(size=1)
    body=bpy.context.object;body.name='Folded green wrapper'
    body.dimensions=(W,H,D)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    body.data.materials.append(plain);body.data.materials.append(front)
    for p in body.data.polygons:p.material_index=1 if p.normal.z>.9 else 0
    uvmap(body);bevel(body,.12,3)
    # Real overlapping flaps on the reverse, all opaque and physically thick.
    flaps=[('Left fold',[(-3.90,-5.34),(-.20,-2.00),(.4,1.2),(-3.90,5.34)],-.492),
           ('Right fold',[(3.90,5.34),(-.05,2.40),(-.35,-1.5),(3.90,-5.34)],-.511),
           ('Lower fold',[(-3.82,-5.34),(3.82,-5.34),(3.65,-4.90),(.18,.48),(-.24,.42),(-3.66,-4.95)],-.530),
           ('Upper fold',[(3.82,5.34),(-3.82,5.34),(-3.68,4.95),(-.35,-.7),(.10,-.82),(3.7,4.95)],-.550)]
    for label,pts,z in flaps:
        # Reverse the winding so the exterior normal faces down.
        obj=mesh_obj(label,[(x,y,z) for x,y in pts],[tuple(reversed(range(len(pts))))],plain)
        sol=obj.modifiers.new('Paper thickness','SOLIDIFY');sol.thickness=.021
        bevel(obj,.018,2)
    if tournament:
        strap('Long tournament ribbon','y',2.13,.58,band)
        strap('Cross tournament ribbon','x',-2.12,.40,band)
        # Narrow gold selvedges catch the light along the ribbon.
        for x in (1.89,2.37):
            points=[(x,p,z+.021 if z>0 else z-.021) for p,z in ring_path(H/2+.049,D/2+.15,48)]
            tube('Ribbon selvedge',points,.014,gold,True)
    else:
        p=[(x,-2.12,z) for x,z in ring_path(W/2+.055,D/2+.086)]
        tube('Cross binding cord',p,.052,cord,True)
        p=[(2.13,y,z) for y,z in ring_path(H/2+.055,D/2+.085)]
        tube('Long binding cord',p,.052,cord,True)
        # Short ends peeking from beneath the seal.
        for side in (-1,1):
            p=[(2.13+side*t*.9,-2.12-t*.75,.63+.055*math.sin(t*math.pi)) for t in [i/14 for i in range(15)]]
            tube('Tied cord end',p,.045,cord)
    seal(tournament,wax,gold)
    return export(name)


def export(name):
    deps=bpy.context.evaluated_depsgraph_get();buckets={}
    for obj in list(bpy.context.scene.objects):
        if obj.type!='MESH':continue
        evaluated=obj.evaluated_get(deps);mesh=evaluated.to_mesh();mesh.calc_loop_triangles()
        normals=mesh.corner_normals
        for tri in mesh.loop_triangles:
            mat=obj.data.materials[tri.material_index]
            bucket=buckets.setdefault(mat.name,{'name':mat.name,'vertices':[],'normals':[],'uvs':[],'triangles':[],
                'textures':{'0':f'textures\\Tessera\\packs\\{mat["texture"]}.dds','1':f'textures\\Tessera\\packs\\{mat["texture"]}_n.dds'},
                'glossiness':mat['glossiness'],'specularStrength':mat['specularStrength'],'lookup':{}})
            ids=[]
            for loop in tri.loops:
                v=obj.matrix_world@mesh.vertices[mesh.loops[loop].vertex_index].co
                n=(obj.matrix_world.to_3x3().inverted().transposed()@normals[loop].vector).normalized()
                uv=list(mesh.uv_layers.active.data[loop].uv)
                tile=mat['atlas']
                if tile>=0:
                    # Keep each material inside its atlas tile, with a wide mip gutter.
                    uv=[(tile%4+.10+.80*max(0,min(1,uv[0])))/4,
                        (1-tile//4+.10+.80*max(0,min(1,uv[1])))/2]
                key=tuple(round(x,6) for x in (*v,*n,*uv))
                if key not in bucket['lookup']:
                    bucket['lookup'][key]=len(bucket['vertices'])
                    bucket['vertices'].append(list(v));bucket['normals'].append(list(n));bucket['uvs'].append(uv)
                ids.append(bucket['lookup'][key])
            if len(set(ids))==3:
                a,b,c=(Vector(bucket['vertices'][i]) for i in ids)
                if (b-a).cross(c-a).length_squared>1e-15:
                    bucket['triangles'].append(ids)
        evaluated.to_mesh_clear()
    meshes=list(buckets.values())
    for m in meshes:del m['lookup']
    coords=[v for m in meshes for v in m['vertices']]
    low=[min(v[i] for v in coords) for i in range(3)];high=[max(v[i] for v in coords) for i in range(3)]
    center=[(low[i]+high[i])/2 for i in range(3)]
    for m in meshes:
        for v in m['vertices']:
            for i in range(3):v[i]-=center[i]
    half=[(high[i]-low[i])/2 for i in range(3)]
    # INV rotations are stored in thousandths of a radian. Turn the printed
    # face through half a turn in the inventory, preserving its existing tilt.
    data={'name':f'Tessera {name} pack','meshes':meshes,'collisionHalfExtents':half,'inventoryRotation':[1250,0,3302]}
    WORK.mkdir(parents=True,exist_ok=True)
    (WORK/f'{name}.json').write_text(json.dumps(data,separators=(',',':'))+'\n')
    for image in bpy.data.images:
        if image.source=='FILE':image.pack()
    bpy.ops.wm.save_as_mainfile(filepath=str(WORK/f'{name}.blend'))
    print(name,sum(len(m['triangles']) for m in meshes),'triangles',len(meshes),'materials')
    return data


def load_roundtrip(name,offset=(0,0,0),rotation=0,reverse=False):
    data=json.loads((WORK/f'{name}-nif.json').read_text())
    objects=[]
    for m in data['meshes']:
        texture=Path(m['textures']['0'].replace('\\','/')).stem
        mat=material(m['name'],texture,gloss=m['glossiness'],spec=m['specularStrength'])
        o=mesh_obj(m['name'],m['vertices'],m['triangles'],mat,True)
        u=o.data.uv_layers.active
        for face in o.data.polygons:
            for li in face.loop_indices:u.data[li].uv=m['uvs'][o.data.loops[li].vertex_index]
        o.data.update()
        o.data.normals_split_custom_set([tuple(m['normals'][loop.vertex_index]) for loop in o.data.loops])
        o.location=offset;o.rotation_euler.z=rotation
        if reverse:o.rotation_euler.x=math.pi
        objects.append(o)
    return objects


def track(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


def lighting(camera_location,target=(0,0,0),scale=25):
    scene=bpy.context.scene;scene.render.engine='CYCLES'
    scene.cycles.samples=40;scene.cycles.use_denoising=True
    scene.render.resolution_x=1800;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
    scene.world.color=(.15,.15,.15)
    scene.view_settings.view_transform='AgX'
    scene.view_settings.look='AgX - Medium High Contrast'
    scene.view_settings.exposure=.45
    bpy.ops.object.camera_add(location=camera_location)
    camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=scale
    track(camera,target);scene.camera=camera
    for name,loc,power,size,color in [('Large softbox',(-8,-4,19),6500,13,(1,.88,.70)),('Cool fill',(10,4,13),4000,11,(.77,.86,1)),('Rim',(-3,12,7),2500,8,(1,.73,.43))]:
        bpy.ops.object.light_add(type='AREA',location=loc)
        obj=bpy.context.object;obj.name=name;obj.data.energy=power;obj.data.shape='DISK';obj.data.size=size;obj.data.color=color;track(obj,(0,0,0))
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.82))
    floor=bpy.context.object;floor.name='Preview surface'
    mat=bpy.data.materials.new('Warm stone backdrop');mat.diffuse_color=(.17,.145,.105,1);mat.use_nodes=True
    p=mat.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(.17,.145,.105,1);p.inputs['Roughness'].default_value=.93
    floor.data.materials.append(mat)


def preview():
    PREVIEW.mkdir(parents=True,exist_ok=True)
    for reverse in (False,True):
        clear()
        load_roundtrip('standard',(-4.9,0,.02),-.12,reverse)
        load_roundtrip('tournament',(4.9,.3,.02),.12,reverse)
        lighting((10,-18,25),target=(0,.2,0),scale=23.3)
        bpy.context.scene.render.filepath=str(PREVIEW/('card-packs-back.png' if reverse else 'card-packs.png'))
        bpy.ops.render.render(write_still=True)


def ui_previews():
    """Transparent pack images for the album tiles and inspector.

    Read back the shipped meshes and DDSs so the images match the inventory packs.
    """
    output=ROOT/'TesseraUI/assets/packs';output.mkdir(parents=True,exist_ok=True)
    for name in ('standard','tournament'):
        clear();objects=load_roundtrip(name)
        coords=[o.matrix_world@v.co for o in objects for v in o.data.vertices]
        low=Vector(tuple(min(v[i] for v in coords) for i in range(3)))
        high=Vector(tuple(max(v[i] for v in coords) for i in range(3)))
        center=(low+high)/2;radius=((high-low)/2).length
        yaw,pitch=math.radians(-82),math.radians(75)
        direction=Vector((math.cos(pitch)*math.cos(yaw),math.cos(pitch)*math.sin(yaw),math.sin(pitch)))
        distance=radius*1.25/(512/672)/math.tan(math.pi/8)*.60
        lighting(center+direction*distance,target=center)
        bpy.data.objects.remove(bpy.data.objects['Preview surface'],do_unlink=True)
        scene=bpy.context.scene;scene.render.film_transparent=True
        scene.render.resolution_x=512;scene.render.resolution_y=672
        scene.camera.data.type='PERSP';scene.camera.data.sensor_fit='VERTICAL'
        scene.camera.data.lens=scene.camera.data.sensor_height/(2*math.tan(math.pi/8))
        scene.camera.rotation_euler=(-direction).to_track_quat('-Z','Y').to_euler()
        scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
        scene.render.filepath=str(output/f'{name}.png')
        bpy.ops.render.render(write_still=True)


if __name__=='__main__':
    mode=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'build'
    if mode=='build':
        for tournament in (False,True):clear();build(tournament)
    elif mode=='preview':preview()
    elif mode=='ui':ui_previews()
    else:raise SystemExit(f'Unknown mode {mode}')
