import bpy, sys, os, math, random, bmesh, json, numpy as np
from mathutils import Vector, Matrix, Quaternion
WORK='/Users/Stewart/Documents/Codex/2026-09-11/c/work'
TOOLS=WORK+'/cobble-tools'
ASSETS=TOOLS+'/assets'
OUT='/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/Cobble'
sys.path[:0]=[TOOLS+'/mpfb2/src',WORK]
original_path=bpy.utils.extension_path_user
bpy.utils.extension_path_user=lambda package,**kwargs: TOOLS+'/mpfb-user' if package=='mpfb' else original_path(package,**kwargs)
import addon_utils
addon_utils.enable('mpfb',default_set=True,persistent=False)
from mpfb.services.humanservice import HumanService
from mpfb.services.targetservice import TargetService
from mpfb.services.objectservice import ObjectService

# Visual parameters only: roster names, statistics and abilities are deliberately absent.
CAST=[
 dict(h=1.7526,muscle=.50,weight=.42,hair='short02',haircolor=(.025,.018,.014),shirt=(.15,.19,.15),shorts=(.012,.016,.020),
      targets={'head/head-oval':.35,'chin/chin-width-decr':.12,'nose/nose-scale-depth-incr':.13,'neck/neck-scale-vert-incr':.16}),
 dict(h=1.79,muscle=.48,weight=.46,hair='short01',haircolor=(.095,.054,.027),shirt=(.012,.54,.55),shorts=(.009,.012,.013),
      targets={'head/head-rectangular':.22,'chin/chin-width-incr':.12,'nose/nose-scale-horiz-decr':.1}),
 dict(h=1.81,muscle=.43,weight=.40,hair='short02',haircolor=(.16,.065,.028),shirt=(.012,.025,.056),shorts=(.57,.57,.53),
      targets={'head/head-oval':.4,'head/head-scale-horiz-decr':.12,'nose/nose-scale-depth-incr':.15,'neck/neck-scale-vert-incr':.18}),
 dict(h=1.80,muscle=.73,weight=.50,hair='short01',haircolor=(.028,.013,.007),shirt=None,shorts=(.009,.012,.016),
      targets={'head/head-square':.42,'chin/chin-width-incr':.18,'chin/chin-prominent-incr':.1,'torso/torso-vshape-incr':.13}),
 dict(h=1.81,muscle=.49,weight=.68,hair='short02',haircolor=(.023,.012,.009),shirt=(.008,.011,.015),shorts=(.012,.020,.048),
      targets={'head/head-round':.38,'head/head-fat-incr':.16,'chin/chin-width-incr':.14,'nose/nose-width1-incr':.12}),
 dict(h=1.79,muscle=.57,weight=.48,hair='short01',haircolor=(.025,.013,.008),shirt=(.010,.012,.014),shorts=(.055,.058,.060),
      targets={'head/head-rectangular':.31,'chin/chin-height-incr':.12,'nose/nose-scale-depth-incr':.14,'head/head-scale-horiz-decr':.06}),
 dict(h=1.77,muscle=.53,weight=.67,hair='short02',haircolor=(.025,.012,.007),shirt=(.53,.56,.56),shorts=(.40,.32,.21),
      targets={'head/head-square':.24,'head/head-round':.22,'head/head-fat-incr':.10,'chin/chin-width-incr':.15}),
 dict(h=1.80,muscle=.56,weight=.46,hair='short02',haircolor=(.082,.039,.018),shirt=(.009,.010,.013),shorts=(.42,.45,.46),
      targets={'head/head-oval':.3,'chin/chin-width-decr':.10,'nose/nose-scale-depth-incr':.12}),
]
# Provisional art-direction heights, not measurements inferred from perspective photos.
# These can be replaced with supplied real heights without changing roster identity.
for character, height_m in zip(CAST, (1.7526, 1.76, 1.88, 1.82, 1.78, 1.86, 1.70, 1.80)):
    character['h'] = height_m

def activate(o):
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active=o

def material(name,color,rough=.7):
    m=bpy.data.materials.new(name);m.use_nodes=True;m.diffuse_color=(*color,1)
    bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(*color,1);bs.inputs['Roughness'].default_value=rough
    return m

def setmat(o,m):
    o.data.materials.clear();o.data.materials.append(m)

def tint_texture_material(o,name,color,dest):
    for m in o.data.materials:
        if not m or not m.use_nodes:continue
        m.name=name
        for n in m.node_tree.nodes:
            if n.type!='TEX_IMAGE' or not n.image or 'ormal' in n.name:continue
            source=n.image;pixels=np.asarray(source.pixels[:],dtype=np.float32).reshape(-1,4)
            # Preserve strand opacity and luminance variation while matching the reference color.
            lum=np.maximum(.04,pixels[:,:3].mean(axis=1))
            pixels[:,:3]=np.minimum(1,np.asarray(color)[None,:]*(.6+lum[:,None]*1.7))
            im=bpy.data.images.new(name+'_Texture',width=source.size[0],height=source.size[1],alpha=True)
            im.pixels.foreach_set(pixels.ravel());im.filepath_raw=dest+'/'+name+'_Texture.png';im.file_format='PNG';im.save();n.image=im
        bs=m.node_tree.nodes.get('Principled BSDF')
        if bs:bs.inputs['Roughness'].default_value=.85

def addasset(h,name,kind='Clothes',subdir='clothes'):
    p=ASSETS+'/'+subdir+'/'+name+'/'+name+'.mhclo'
    o=HumanService.add_mhclo_asset(p,h,asset_type=kind,material_type='GAMEENGINE',subdiv_levels=0)
    for mod in o.modifiers:
        if mod.type=='SUBSURF':mod.levels=0;mod.render_levels=0
    return o

def bake(o,rig):
    # Bake modeling targets and clothing masks, but retain the genuine skeleton deformation.
    for mod in o.modifiers:
        if mod.type=='ARMATURE':mod.show_viewport=False;mod.show_render=False
    activate(o);bpy.ops.object.convert(target='MESH')
    o=bpy.context.object
    for p in o.data.polygons:p.use_smooth=True
    o.parent=rig
    mod=o.modifiers.new('Skeleton','ARMATURE');mod.object=rig
    return o

def delete_vertices(o,predicate):
    bm=bmesh.new();bm.from_mesh(o.data)
    doomed=[v for v in bm.verts if predicate(v.co)]
    bmesh.ops.delete(bm,geom=doomed,context='VERTS');bm.to_mesh(o.data);bm.free();o.data.update()

def cut_plane(o,point,normal,outer=False):
    bm=bmesh.new();bm.from_mesh(o.data)
    bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,
        plane_co=point,plane_no=normal,clear_inner=not outer,clear_outer=outer)
    bm.to_mesh(o.data);bm.free();o.data.update()

def garment_texture(o,index,dest):
    m=o.data.materials[0];nt=m.node_tree;bs=nt.nodes.get('Principled BSDF');base=tuple(bs.inputs['Base Color'].default_value)
    coord=nt.nodes.new('ShaderNodeTexCoord');noise=nt.nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=310;noise.inputs['Detail'].default_value=2
    nt.links.new(coord.outputs['Object'],noise.inputs['Vector'])
    ramp=nt.nodes.new('ShaderNodeValToRGB');ramp.color_ramp.elements[0].color=tuple(x*.81 for x in base[:3])+(1,);ramp.color_ramp.elements[1].color=tuple(x*1.10 for x in base[:3])+(1,)
    nt.links.new(noise.outputs['Fac'],ramp.inputs['Fac']);color=ramp.outputs['Color']
    if index in (2,7):
        wave=nt.nodes.new('ShaderNodeTexWave');wave.wave_type='BANDS';wave.bands_direction='X' if index==2 else 'Z';wave.inputs['Scale'].default_value=14 if index==2 else 22
        nt.links.new(coord.outputs['Object'],wave.inputs['Vector'])
        stripe=nt.nodes.new('ShaderNodeValToRGB');stripe.color_ramp.interpolation='CONSTANT';stripe.color_ramp.elements[0].position=.0;stripe.color_ramp.elements[0].color=base
        stripe.color_ramp.elements[1].position=.94 if index==2 else .84;stripe.color_ramp.elements[1].color=tuple(x*(.60 if index==2 else .80) for x in base[:3])+(1,)
        nt.links.new(wave.outputs['Fac'],stripe.inputs['Fac']);color=stripe.outputs['Color']
    emit=nt.nodes.new('ShaderNodeEmission');nt.links.new(color,emit.inputs[0]);out=nt.nodes.get('Material Output');nt.links.new(emit.outputs[0],out.inputs['Surface'])
    image=bpy.data.images.new(m.name+'_Cotton',width=1024,height=1024,alpha=False);tex=nt.nodes.new('ShaderNodeTexImage');tex.image=image;nt.nodes.active=tex
    activate(o);scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1;scene.render.bake.margin=8
    bpy.ops.object.bake(type='EMIT')
    image.filepath_raw=dest+'/'+m.name+'_Cotton.png';image.file_format='PNG';image.save()
    nt.links.new(bs.outputs[0],out.inputs['Surface']);nt.links.new(tex.outputs['Color'],bs.inputs['Base Color'])

def curls(rig,height,color,index):
    rng=random.Random(index*731);parts=[];mat=material(f'C{index:02d}_CurlFiber',tuple(x*.20 for x in color),.92)
    top=rig.data.bones['head'].tail_local;center=Vector((0,top.y,top.z-.075))
    for i in range(150):
        phi=rng.uniform(0,math.tau);theta=rng.uniform(.05,1.37)
        pos=center+Vector((.091*math.sin(theta)*math.cos(phi),.095*math.sin(theta)*math.sin(phi),.09*math.cos(theta)))
        outward=(pos-center).normalized();tangent=outward.cross(Vector((0,0,1))).normalized()
        if tangent.length<.1:tangent=Vector((1,0,0))
        cross=outward.cross(tangent);curve=bpy.data.curves.new('Curl','CURVE');curve.dimensions='3D';curve.resolution_u=1;curve.bevel_depth=.0008;curve.bevel_resolution=1
        sp=curve.splines.new('POLY');sp.points.add(11)
        for k,p in enumerate(sp.points):
            a=k/11*math.tau*1.25;point=pos+outward*(k/11*.015)+tangent*(math.cos(a)*.009)+cross*(math.sin(a)*.009);p.co=(*point,1)
        o=bpy.data.objects.new('Curly hair',curve);bpy.context.collection.objects.link(o);o.data.materials.append(mat);activate(o);bpy.ops.object.convert(target='MESH');bind(o,rig,'head');parts.append(o)
    return parts

def neckline_trim(o,rig,mat):
    bm=bmesh.new();bm.from_mesh(o.data);bound=[e for e in bm.edges if e.is_boundary];edges=set(bound);parts=[]
    while edges:
        edge=edges.pop();chain=[edge.verts[0],edge.verts[1]]
        while True:
            nxt=next((e for e in chain[-1].link_edges if e in edges),None)
            if not nxt:break
            edges.remove(nxt);chain.append(nxt.other_vert(chain[-1]))
            if chain[-1]==chain[0]:break
        if len(chain)<4:continue
        curve=bpy.data.curves.new('Jersey binding','CURVE');curve.dimensions='3D';curve.bevel_depth=.003;curve.bevel_resolution=1
        spline=curve.splines.new('POLY');spline.points.add(len(chain)-1)
        for p,v in zip(spline.points,chain):p.co=(*v.co,1)
        trim=bpy.data.objects.new('Black jersey edge',curve);bpy.context.collection.objects.link(trim);trim.data.materials.append(mat);activate(trim);bpy.ops.object.convert(target='MESH');bind(trim,rig,'spine_03');parts.append(trim)
    bm.free();return parts

def bind(o,rig,bone):
    o.vertex_groups.new(name=bone).add(list(range(len(o.data.vertices))),1,'REPLACE')
    o.parent=rig;m=o.modifiers.new('Skeleton','ARMATURE');m.object=rig

def detailed_eyes(eyes,rig,index):
    setmat(eyes,material(f'C{index:02d}_EyeWhite',(.38,.35,.32),.32));objects=[]
    iriscolor=((.013,.010,.008),(.030,.019,.010),(.047,.045,.034),(.027,.020,.014),
               (.036,.025,.015),(.026,.021,.015),(.030,.024,.017),(.040,.027,.016))[index-1]
    iris=material(f'C{index:02d}_Iris',iriscolor,.31);pupil=material(f'C{index:02d}_Pupil',(.001,.001,.001),.24)
    for sign in (-1,1):
        points=[v.co for v in eyes.data.vertices if v.co.x*sign>0]
        middle=sum(points,Vector())/len(points);front=min(v.y for v in points)
        for label,radius,y,mat in [('Iris',.0060 if index==1 else .0057,front-.0004,iris),('Pupil',.0026 if index==1 else .0022,front-.0014,pupil)]:
            bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,location=(middle.x,y,middle.z))
            o=bpy.context.object;o.name=f'C{index:02d}_{label}';o.scale=(radius,.001,radius);setmat(o,mat)
            bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
            for p in o.data.polygons:p.use_smooth=True
            bind(o,rig,'head');objects.append(o)
    return objects

def textmesh(text,loc,size,mat,rig,bone='spine_03',back=False):
    curve=bpy.data.curves.new('Printed lettering','FONT');curve.body=text;curve.align_x='CENTER';curve.align_y='CENTER';curve.size=size;curve.resolution_u=2
    font='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
    if os.path.exists(font):curve.font=bpy.data.fonts.load(font,check_existing=True)
    o=bpy.data.objects.new('Garment print',curve);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(math.pi/2,0,math.pi if back else 0)
    o.data.materials.append(mat);activate(o);bpy.ops.object.convert(target='MESH');bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bind(o,rig,bone);return o

def pineapple(loc,size,rig,back=False):
    # A small actual mesh applique fitted to the shirt, including the leaf crown.
    yellow=material('Pineapple gold',(.40,.25,.04));green=material('Pineapple leaves',(.07,.15,.04));objects=[]
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,location=loc)
    fruit=bpy.context.object;fruit.name='Pineapple shirt print';fruit.scale=(size*.4,.001,size*.6);setmat(fruit,yellow);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);bind(fruit,rig,'spine_03');objects.append(fruit)
    sign=-1 if back else 1
    for dx in [-.36,-.18,0,.18,.36]:
        verts=[(loc[0]-size*.1,loc[1],loc[2]+size*.4),(loc[0]+size*.1,loc[1],loc[2]+size*.4),(loc[0]+size*dx,loc[1]-.001*sign,loc[2]+size*(1.05-abs(dx)*.65))]
        mesh=bpy.data.meshes.new('Leaf');mesh.from_pydata(verts,[],[(0,1,2),(2,1,0)]);o=bpy.data.objects.new('Leaf applique',mesh);bpy.context.collection.objects.link(o);setmat(o,green);bind(o,rig,'spine_03');objects.append(o)
    return objects

def direction(rig,name,d):
    pb=rig.pose.bones[name]; rb=pb.bone
    rest=rb.matrix_local.to_quaternion();v=(rb.tail_local-rb.head_local).normalized()
    wanted=v.rotation_difference(Vector(d).normalized()) @ rest
    basis=(pb.parent.matrix.to_quaternion() @ pb.parent.bone.matrix_local.to_quaternion().inverted() @ rest) if pb.parent else rest
    pb.rotation_mode='QUATERNION';pb.rotation_quaternion=basis.inverted() @ wanted
    bpy.context.view_layer.update()

def pose(rig,arms=False,phase=0,walk=False):
    for p in rig.pose.bones:p.rotation_mode='QUATERNION';p.rotation_quaternion=Quaternion();p.location=(0,0,0)
    bpy.context.view_layer.update()
    wave=math.sin(phase)
    for side,sign in [('l',1),('r',-1)]:
        if arms:
            direction(rig,'upperarm_'+side,(sign*.16,-.23,-.24))
            direction(rig,'lowerarm_'+side,(-sign*.04,-.31,.12+wave*.005))
            direction(rig,'hand_'+side,(-sign*.015,-.10,.005))
        else:
            swing=sign*.24*wave if walk else .006*wave
            direction(rig,'upperarm_'+side,(sign*.07,swing,-.30))
            direction(rig,'lowerarm_'+side,(sign*.012,-.035+swing*.6,-.28))
            direction(rig,'hand_'+side,(sign*.003,-.035,-.095))
        if walk:
            direction(rig,'thigh_'+side,(sign*.02,-sign*.17*wave,-.40))
            direction(rig,'calf_'+side,(sign*.005,sign*.12*wave,-.40))
        # Gently curled relaxed fingers, rather than a stiff spread-hand bind pose.
        for finger in ['index','middle','ring','pinky']:
            for joint,angle in [('01',12),('02',16),('03',8)]:
                p=rig.pose.bones.get(f'{finger}_{joint}_{side}')
                if p:p.rotation_quaternion=Quaternion((1,0,0),math.radians(angle))
    rig.pose.bones['spine_03'].rotation_quaternion=Quaternion((0,1,0),.006*wave)
    bpy.context.view_layer.update()

def action(rig,name,arms=False,walk=False):
    a=bpy.data.actions.new(name);a.use_fake_user=True;rig.animation_data_create();rig.animation_data.action=a
    for frame in range(1,62,5):
        phase=(frame-1)/60*math.tau
        pose(rig,arms,phase,walk)
        for pb in rig.pose.bones:
            pb.keyframe_insert('rotation_quaternion',frame=frame,group=pb.name)
            pb.keyframe_insert('location',frame=frame,group=pb.name)
    return a

def export(path,rig,objects,scale,animation=None):
    rig.animation_data_clear()
    if animation:rig.animation_data_create();rig.animation_data.action=animation
    else:
        for p in rig.pose.bones:p.matrix_basis=Matrix.Identity(4)
    bpy.context.scene.frame_set(1);bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=path,use_selection=True,object_types={'ARMATURE','MESH'},global_scale=scale,
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',
        add_leaf_bones=False,use_armature_deform_only=True,mesh_smooth_type='FACE',path_mode='COPY',embed_textures=True,
        bake_anim=bool(animation),bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0)

def setup_render(height):
    s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=24
    s.render.resolution_x=640;s.render.resolution_y=960;s.render.resolution_percentage=100
    s.render.film_transparent=True;s.view_settings.view_transform='AgX'
    s.world.color=(.08,.08,.08)
    for name,loc,power,color,size in [('Key',(-2,-3,3),170,(1,.95,.9),3),('Fill',(2,-2,1.8),70,(.90,.94,1),2),('Rim',(0,2,2.5),150,(.92,.96,1),2)]:
        d=bpy.data.lights.new(name,'AREA');d.energy=power;d.color=color;d.shape='DISK';d.size=size
        o=bpy.data.objects.new(name,d);bpy.context.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,height*.6))-o.location).to_track_quat('-Z','Y').to_euler()
    d=bpy.data.cameras.new('ReviewCamera');o=bpy.data.objects.new('ReviewCamera',d);bpy.context.collection.objects.link(o)
    o.location=(.25,-4,height*.57);o.rotation_euler=(Vector((0,0,height*.52))-o.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=height*1.13;s.camera=o
    return o

def build(index):
    c=CAST[index-1];key=f'C{index:02d}';dest=OUT+'/'+key;os.makedirs(dest,exist_ok=True)
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    macro=TargetService.get_default_macro_info_dict();macro.update(gender=1.,age=.5,muscle=c['muscle'],weight=c['weight'],height=.5)
    macro['race']={'caucasian':1.,'asian':0.,'african':0.}
    h=HumanService.create_human(macro_detail_dict=macro);h.name=key+'_Body'
    for target,value in c['targets'].items():TargetService.load_target(h,TOOLS+'/mpfb2/src/mpfb/data/targets/'+target+'.target.gz',weight=value)
    from cobble_face_shape import prepare_face_targets
    prepare_face_targets(index,h)
    HumanService.set_character_skin(ASSETS+'/skins/young_caucasian_male/young_caucasian_male.mhmat',h,skin_type='GAMEENGINE')
    rig=HumanService.add_builtin_rig(h,'game_engine');rig.name=key+'_Rig'
    height=max(v.co.z for v in h.data.vertices);bpy.context.view_layer.update()
    # Evaluated body height includes all fitted macro targets.
    height=h.dimensions.z
    bodyobjects=[h]
    eyes=addasset(h,'low-poly','Eyes','eyes');bodyobjects.append(eyes)
    brows=addasset(h,'eyebrow001','Eyebrows','eyebrows');bodyobjects.append(brows)
    # Basic clean skin and texture-controlled hair import directly into Unreal.
    hair=addasset(h,c['hair'],'Hair','hair');bodyobjects.append(hair)
    tint_texture_material(hair,key+'_Hair',c['haircolor'],dest)
    tint_texture_material(brows,key+'_Brows',c['haircolor'],dest)
    shorts=addasset(h,'toigo_wool_pants');bodyobjects.append(shorts)
    shortmat=material(key+'_Shorts',c['shorts']);setmat(shorts,shortmat)
    shirt=None
    if c['shirt']:
        shirt=addasset(h,'male_casualsuit02' if index==1 else ('namuhekam_male_polo_shirt' if index==7 else 'toigo_basic_tucked_t-shirt'));bodyobjects.append(shirt)
        setmat(shirt,material(key+'_Shirt',c['shirt']))
    if index==3:bodyobjects.append(addasset(h,'shoes05'))
    mats={n:material(key+'_'+n,col) for n,col in dict(black=(.008,.009,.012),white=(.75,.75,.7),brown=(.08,.035,.012),metal=(.32,.33,.35),sage=(.24,.27,.22)).items()}
    # Remove original garment masks because trousers are being shortened into shorts.
    for mod in list(h.modifiers):
        if mod.type=='MASK' and ('wool' in mod.name or 'wool' in mod.vertex_group or (index in (1,2) and mod.vertex_group!='body')):h.modifiers.remove(mod)
    bodyobjects=[bake(o,rig) for o in bodyobjects]
    h=bodyobjects[0];eyes=bodyobjects[1];brows=bodyobjects[2];hair=bodyobjects[3];shorts=bodyobjects[4]
    if shirt:shirt=bodyobjects[5]
    bodyobjects.extend(detailed_eyes(eyes,rig,index))
    if index==5:
        eye_z=sum(v.co.z for v in eyes.data.vertices)/len(eyes.data.vertices)
        for v in hair.data.vertices:
            if v.co.y<rig.data.bones['head'].head_local.y-.025:
                floor=eye_z+.016+.024*math.exp(-(v.co.x/.025)**2)
                v.co.z=max(v.co.z,floor)
    if index==1:
        from braxton_detail import subdivide
        subdivide(h)
    from cobble_face_texture import apply_reference_face
    apply_reference_face(index,h,rig,dest)
    from cobble_skin_detail import add_skin_detail
    add_skin_detail(index,h,dest)
    bodyobjects.remove(brows);bpy.data.objects.remove(brows,do_unlink=True)
    height=max(v.co.z for v in h.data.vertices);scale=c['h']/height
    hem=height*(.342 if index==2 else .375)
    cut_plane(shorts,(0,0,hem),(0,0,1))
    # Straight, slightly loose shorts hems; retain topology above the cut.
    for v in shorts.data.vertices:
        if v.co.z<hem+.001:v.co.z=hem
    if shirt:
        if index==1:
            cut_plane(shirt,(0,0,height*.505),(0,0,1))
            # Long-sleeve shell: sleeves are pushed up below the elbow in the reference.
            def sleeve_position(v):
                b=rig.data.bones['lowerarm_l' if v.x>0 else 'lowerarm_r'];d=b.tail_local-b.head_local
                return (v-b.head_local).dot(d)/d.length_squared
            delete_vertices(shirt,lambda v:abs(v.x)>height*.16 and sleeve_position(v)>.32)
        for v in shirt.data.vertices:
            if abs(v.co.x)<height*.15:
                v.co.x*=1.025
                v.co.y*=1.035
                if v.co.z<height*.56:v.co.z-=height*(.018 if index!=5 else .036)
        if index==2:
            shoulder=rig.data.bones['upperarm_l'].head_local.x
            cut_plane(shirt,(shoulder*.95,0,0),(1,0,0),outer=True)
            cut_plane(shirt,(-shoulder*.95,0,0),(-1,0,0),outer=True)
            bodyobjects.extend(neckline_trim(shirt,rig,mats['black']))
        garment_texture(shirt,index,dest)
    # Conceal body only underneath clothes, preserving real neck, arms, legs and hands.
    torso_top=rig.data.bones['neck_01'].head_local.z
    delete_vertices(h,lambda v: (hem+.035<v.z<height*.535) or
        (index==1 and height*.535<=v.z<torso_top-.07 and abs(v.x)<height*.12))
    if index==8:
        # Gloves replace hands visually, while arms retain their own skinned topology.
        hand_ids={g.index for g in h.vertex_groups if g.name.startswith(('hand_','index_','middle_','ring_','pinky_','thumb_'))}
        doomed={v.index for v in h.data.vertices if sum(g.weight for g in v.groups if g.group in hand_ids)>.90}
        bm=bmesh.new();bm.from_mesh(h.data);bm.verts.ensure_lookup_table();bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.index in doomed],context='VERTS');bm.to_mesh(h.data);bm.free()
    try:
        from cobble_accessories import add_accessories
        bodyobjects.extend(add_accessories(index,h,rig,mats))
    except ImportError:print('ACCESSORIES_NOT_READY')
    from cobble_hair_detail import refine_hair
    bodyobjects.extend(refine_hair(index,hair,rig,h,dest))
    if index==1:
        from braxton_detail import refine_braxton
        refine_braxton(h,shirt,shorts,rig,bodyobjects,dest)
    # Model print lettering as fitted, rigged garment details (not a photograph billboard).
    chest=rig.data.bones['spine_03'].tail_local.z
    if shirt:
        yf=min(v.co.y for v in shirt.data.vertices if chest-.14<v.co.z<chest+.02)
        yb=max(v.co.y for v in shirt.data.vertices if chest-.14<v.co.z<chest+.02)
        lines={2:[('BUZZ CITY',.030,0),('2',.105,-.115)],5:[('SUPER BOWL LX',.018,.025),('CHAMPIONS',.029,-.02),('SEAHAWKS',.016,-.066)],6:[('Coastal',.035,-.02)],8:[('I AM AFRAID',.024,.01),('OF WOMEN',.024,-.035)]}.get(index,[])
        printmat=material(key+'_TealPrint',(.01,.24,.20)) if index==6 else mats['white']
        for t,size,dz in lines:bodyobjects.append(textmesh(t,(0,yf-.005,chest+dz),size,printmat,rig))
        backlines={2:[('BALL',.028,.015),('2',.10,-.10)],3:[('MYRTLE BEACH',.018,-.12),('SOUTH CAROLINA',.013,-.15)],5:[('SUPER BOWL',.02,.0),('CHAMPIONS',.025,-.07)]}.get(index,[])
        for t,size,dz in backlines:bodyobjects.append(textmesh(t,(0,yb+.005,chest+dz),size,mats['white'],rig,back=True))
        if index==3:
            bodyobjects.extend(pineapple((.073,yf-.007,chest+.012),.019,rig))
            bodyobjects.extend(pineapple((0,yb+.007,chest-.05),.072,rig,back=True))
    # Canonical body FBX has a clean rest pose. Animations use this exact skeleton.
    bpy.context.scene.render.fps=30;bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=61
    export(dest+'/SK_'+key+'.fbx',rig,bodyobjects,scale)
    idle=action(rig,'AN_'+key+'_Idle');export(dest+'/AN_'+key+'_Idle.fbx',rig,[],scale,idle)
    walk=action(rig,'AN_'+key+'_Walk',walk=True);export(dest+'/AN_'+key+'_Walk.fbx',rig,[],scale,walk)
    # First-person mesh is a real skinned arm subset, never the full owner's body/head.
    armobjects=[]
    for source in bodyobjects:
        obj=source.copy();obj.data=source.data.copy();bpy.context.collection.objects.link(obj);obj.name=source.name+'_Arms'
        armgroups={g.index for g in obj.vertex_groups if g.name.startswith(('upperarm_','lowerarm_','hand_','index_','middle_','ring_','pinky_','thumb_'))}
        keep={v.index for v in obj.data.vertices if sum(g.weight for g in v.groups if g.group in armgroups)>.5}
        bm=bmesh.new();bm.from_mesh(obj.data);bm.verts.ensure_lookup_table();bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.index not in keep],context='VERTS');bm.to_mesh(obj.data);bm.free()
        if len(obj.data.polygons):armobjects.append(obj)
        else:bpy.data.objects.remove(obj,do_unlink=True)
    export(dest+'/SK_'+key+'_Arms.fbx',rig,armobjects,scale)
    arms=action(rig,'AN_'+key+'_ArmsIdle',arms=True);export(dest+'/AN_'+key+'_ArmsIdle.fbx',rig,[],scale,arms)
    for o in armobjects:o.hide_render=True;o.hide_set(True)
    rig.animation_data.action=idle;bpy.context.scene.frame_set(1)
    camera=setup_render(height)
    bpy.ops.wm.save_as_mainfile(filepath=dest+'/'+key+'.blend')
    bpy.context.scene.render.filepath=dest+'/'+key+'_Review.png';bpy.ops.render.render(write_still=True)
    portrait_center=Vector((0,-.04,rig.data.bones['head'].head_local.z+.065))
    camera.location=(0,-3,portrait_center.z)
    camera.rotation_euler=(portrait_center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=height*.28
    bpy.context.scene.render.resolution_x=512;bpy.context.scene.render.resolution_y=640
    bpy.context.scene.render.filepath=dest+'/T_'+key+'.png';bpy.ops.render.render(write_still=True)
    # A close-up is always part of QA: full-body portraits hide facial fitting errors.
    face_center=Vector((0,-.04,rig.data.bones['head'].head_local.z+.07))
    camera.location=(0,-2.5,face_center.z)
    camera.rotation_euler=(face_center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=.43
    bpy.context.scene.render.resolution_x=768;bpy.context.scene.render.resolution_y=960
    bpy.context.scene.cycles.samples=48
    bpy.context.scene.render.filepath=dest+'/'+key+'_FaceFront.png';bpy.ops.render.render(write_still=True)
    camera.location=(1.1,-2.2,face_center.z)
    camera.rotation_euler=(face_center-camera.location).to_track_quat('-Z','Y').to_euler()
    bpy.context.scene.render.filepath=dest+'/'+key+'_FaceAngle.png';bpy.ops.render.render(write_still=True)
    print('COBBLE_BUILT',key,'height',height,'exportscale',scale,'vertices',sum(len(o.data.vertices) for o in bodyobjects),'armvertices',sum(len(o.data.vertices) for o in armobjects))

if __name__ == '__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['1']
    for i in args:build(int(i))
