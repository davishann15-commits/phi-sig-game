"""Import and place the measured, complete photo-reference great room layout."""
import glob
import json
import os
import re
import unreal

project=unreal.Paths.project_dir()
source=os.path.join(project,'SourceArt/GreatRoomV3')
dest='/Game/SeniorSendOff/House/GreatRoomV3'
assets=unreal.EditorAssetLibrary
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
editing=unreal.MaterialEditingLibrary
layout=json.load(open(os.path.join(source,'layout_manifest.json')))

def import_file(path,name):
    t=unreal.AssetImportTask();t.filename=path;t.destination_path=dest
    t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
    if path.lower().endswith('.fbx'):
        ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False
        ui.import_materials=False;ui.import_textures=False
        ui.static_mesh_import_data.auto_generate_collision=False
        t.options=ui
    asset_tools.import_asset_tasks([t])
    result=assets.load_asset(dest+'/'+name)
    if not result:raise RuntimeError('Import failed '+name)
    return result

meshes={}
for path in sorted(glob.glob(os.path.join(source,'*.fbx'))):
    name=os.path.basename(path)[:-4]
    mesh=import_file(path,name)
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Not mesh '+name)
    meshes[name]=mesh
textures={}
for name in ('T_GR_RoseRug','T_GR_GreenRug','T_GR_LeatherGrain','T_GR_Checker','T_GR3_Walnut'):
    textures[name]=import_file(os.path.join(source,name+'.png'),name)

def expression(m,cls,x=-300,y=0):
    return editing.create_material_expression(m,cls,x,y)

def constant(m,value):
    x=expression(m,unreal.MaterialExpressionConstant);x.r=value;return x

def color(m,value):
    x=expression(m,unreal.MaterialExpressionConstant3Vector)
    x.constant=unreal.LinearColor(*value,1);return x

def connect(a,output,b,input):
    if not editing.connect_material_expressions(a,output,b,input):
        raise RuntimeError('Material connection failed '+input)

def connect_single(a,output,b):
    names=editing.get_material_expression_input_names(b)
    if len(names)!=1:raise RuntimeError('Expected single material input '+str(names))
    connect(a,output,b,names[0])

def uv(m,tiling=1.0):
    n=expression(m,unreal.MaterialExpressionTextureCoordinate)
    n.u_tiling=tiling;n.v_tiling=tiling
    return n

def multiply(m,a,b,a_out='',b_out=''):
    n=expression(m,unreal.MaterialExpressionMultiply)
    connect(a,a_out,n,'A');connect(b,b_out,n,'B');return n

def lerp(m,a,b,alpha,alpha_out=''):
    n=expression(m,unreal.MaterialExpressionLinearInterpolate)
    connect(a,'',n,'A');connect(b,'',n,'B');connect(alpha,alpha_out,n,'Alpha');return n

def luma(m,sample):
    n=expression(m,unreal.MaterialExpressionDotProduct)
    connect(sample,'RGB',n,'A');connect(color(m,(.299,.587,.114)),'',n,'B');return n

def function(m,path):
    f=unreal.load_object(None,path+'.'+path.rsplit('/',1)[-1])
    if not f:raise RuntimeError('Missing engine material function '+path)
    n=expression(m,unreal.MaterialExpressionMaterialFunctionCall)
    n.set_editor_property('material_function',f);return n

def sample_texture(m,texture,tiling=1.0):
    n=expression(m,unreal.MaterialExpressionTextureSample,-650,-150)
    n.texture=texture;connect(uv(m,tiling),'',n,'UVs');return n

def height_normal(m,texture,tiling,strength):
    # Shared height texture supplies the very fine grain; four small samples
    # avoid an extra generated normal asset and preserve source editability.
    t=expression(m,unreal.MaterialExpressionTextureObject);t.texture=texture
    n=function(m,'/Engine/Functions/Engine_MaterialFunctions03/Procedurals/NormalFromHeightmap')
    connect(t,'',n,'Height Map');connect(uv(m,tiling),'',n,'Coordinates')
    connect(constant(m,strength),'',n,'Normal Map Intensity')
    connect(constant(m,.002),'',n,'Height Map UV Offset')
    selector=expression(m,unreal.MaterialExpressionAppendVector)
    connect(color(m,(1,0,0)),'',selector,'A');connect(constant(m,0),'',selector,'B')
    connect(selector,'',n,'Height Map Channel Selector');return n

def cloth_normal(m,texture,tiling=32.0,strength=.22):
    n=sample_texture(m,texture,tiling)
    n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    flat=function(m,'/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal')
    connect(n,'RGB',flat,'Normal');connect(constant(m,1-strength),'',flat,'Flatness');return flat

def corduroy(m,tint):
    coord=uv(m)
    mask=expression(m,unreal.MaterialExpressionComponentMask)
    for component,value in [('r',True),('g',False),('b',False),('a',False)]:
        mask.set_editor_property(component,value)
    connect_single(coord,'',mask)
    rib=multiply(m,mask,constant(m,240.0))
    wave=expression(m,unreal.MaterialExpressionSine);wave.set_editor_property('period',1.0);connect_single(rib,'',wave)
    shade=expression(m,unreal.MaterialExpressionAdd)
    connect(multiply(m,wave,constant(m,.05)),'',shade,'A');connect(constant(m,.98),'',shade,'B')
    base=multiply(m,color(m,tint),shade)
    xy=expression(m,unreal.MaterialExpressionAppendVector)
    connect(multiply(m,wave,constant(m,.13)),'',xy,'A');connect(constant(m,0),'',xy,'B')
    n=expression(m,unreal.MaterialExpressionDeriveNormalZ);connect_single(xy,'',n)
    return base,n

def make_material(name,tint,roughness=.65,metallic=0,texture=None,leather=False,normal=None,
                  texture_tiling=1.0,wood=False,rug=False,cord=False):
    path=dest+'/M_GR3_'+name
    m=assets.load_asset(path) if assets.does_asset_exist(path) else None
    if not m:m=asset_tools.create_asset('M_GR3_'+name,dest,unreal.Material,unreal.MaterialFactoryNew())
    editing.delete_all_material_expressions(m)
    base=color(m,tint);rough=constant(m,roughness);surface_normal=None
    if texture:
        sample=sample_texture(m,texture,texture_tiling)
        if leather:
            wear=luma(m,sample)
            base=lerp(m,color(m,tuple(v*.83 for v in tint)),color(m,tuple(v*1.70 for v in tint)),wear)
            rough=lerp(m,constant(m,roughness-.10),constant(m,roughness+.12),wear)
            surface_normal=height_normal(m,texture,8.0,.90)
        elif wood:
            base=multiply(m,sample,color(m,tint),'RGB')
            rough=lerp(m,constant(m,.46),constant(m,.61),luma(m,sample))
            surface_normal=height_normal(m,texture,1.25,.35)
        elif rug:
            faded=lerp(m,sample,color(m,(.33,.33,.33)),constant(m,.07))
            base=multiply(m,faded,color(m,tint))
        else:base=multiply(m,sample,color(m,tint),'RGB')
    if cord:
        base,surface_normal=corduroy(m,tint)
    if normal and not surface_normal:
        surface_normal=cloth_normal(m,normal,64.0 if rug else 32.0,.18 if rug else .23)
    editing.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    editing.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    editing.connect_material_property(constant(m,metallic),'',unreal.MaterialProperty.MP_METALLIC)
    if surface_normal:editing.connect_material_property(surface_normal,'',unreal.MaterialProperty.MP_NORMAL)
    if name.endswith('Rug'):m.set_editor_property('two_sided',True)
    editing.recompile_material(m)
    if not assets.save_loaded_asset(m):raise RuntimeError('Material save failed '+name)
    return m

fabric_normal=assets.load_asset('/Game/SeniorSendOff/House/Textures/T_Fabric_Normal')
leather=textures['T_GR_LeatherGrain']
new_materials={
    'BronzeLeather':make_material('BronzeLeather',(.18,.098,.048),.58,texture=leather,leather=True),
    'CognacLeather':make_material('CognacLeather',(.31,.135,.047),.56,texture=leather,leather=True),
    'OliveLeather':make_material('OliveLeather',(.087,.102,.055),.63,texture=leather,leather=True),
    'OxbloodLeather':make_material('OxbloodLeather',(.093,.034,.023),.57,texture=leather,leather=True),
    'Walnut':make_material('Walnut',(.78,.65,.50),.54,texture=textures['T_GR3_Walnut'],texture_tiling=.85,wood=True),
    'BlackMetal':make_material('BlackMetal',(.018,.016,.014),.64,.62),
    'GreenCord':make_material('GreenCord',(.025,.052,.032),.93,cord=True),
    'CheckerCloth':make_material('CheckerCloth',(.87,.84,.72),.95,texture=textures['T_GR_Checker'],texture_tiling=3.0,normal=fabric_normal),
    'IvoryCloth':make_material('IvoryCloth',(.64,.61,.53),.96,normal=fabric_normal),
    'Brass':make_material('Brass',(.48,.29,.073),.34,.92),
    'Ceramic':make_material('Ceramic',(.70,.68,.59),.42),
    'RoseRug':make_material('RoseRug',(.78,.84,.96),.99,texture=textures['T_GR_RoseRug'],normal=fabric_normal,rug=True),
    'GreenRug':make_material('GreenRug',(.84,.91,.89),.99,texture=textures['T_GR_GreenRug'],normal=fabric_normal,rug=True),
}
for name,tint in {
    'BronzeStitch':(.094,.052,.027), 'CognacStitch':(.17,.079,.035),
    'OliveStitch':(.045,.055,.028), 'OxbloodStitch':(.055,.019,.014),
    'GreenStitch':(.014,.032,.020), 'IvoryStitch':(.37,.34,.28),
    'CheckerStitch':(.33,.235,.125),
}.items():new_materials[name]=make_material(name,tint,.79)

for name,mesh in meshes.items():
    if name.startswith('COL'):
        body=mesh.get_editor_property('body_setup')
        if not body:raise RuntimeError('Missing collision '+name)
        body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    else:
        for i,slot in enumerate(mesh.static_materials):
            slot_name=re.sub(r'_\d{3}$','',str(slot.material_slot_name).split('.')[0])
            target=new_materials.get(slot_name)
            if not target:
                path='/Game/SeniorSendOff/House/Materials/'+slot_name
                target=assets.load_asset(path) if assets.does_asset_exist(path) else None
            if not target:raise RuntimeError('Missing material '+name+' '+slot_name)
            mesh.set_material(i,target)
    if not assets.save_loaded_asset(mesh):raise RuntimeError('Mesh save failed '+name)
    unreal.log('GR3_SAVED '+name+' '+str(mesh.get_bounds()))

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
def snapshot(a):
    loc=a.get_actor_location();rot=a.get_actor_rotation();scale=a.get_actor_scale3d()
    row={'class':a.get_class().get_name(),'loc':[loc.x,loc.y,loc.z],
         'rot':[rot.roll,rot.pitch,rot.yaw],'scale':[scale.x,scale.y,scale.z]}
    if isinstance(a,unreal.StaticMeshActor):
        mesh=a.static_mesh_component.static_mesh;row['mesh']=mesh.get_path_name() if mesh else None
    return row
before={name:snapshot(a) for name,a in actors.items()}
affected=set(layout['clearedChunks'])
legacy={'SM_GR_PhotoSectionalOttoman','COL_GR_PhotoSectionalOttoman','SM_GR_VintageRug'}
for label,info in layout['clearedChunks'].items():
    actor=actors.get(label)
    if not info['asset']:
        if actor:unreal.EditorLevelLibrary.destroy_actor(actor)
        continue
    if not isinstance(actor,unreal.StaticMeshActor):raise RuntimeError('Missing chunk '+label)
    actor.static_mesh_component.set_static_mesh(meshes[info['asset']])
    actor.static_mesh_component.set_collision_profile_name('BlockAll' if label.startswith('COL') else 'NoCollision')
    if label.startswith('COL'):actor.static_mesh_component.set_visibility(False)

for label,a in list(actors.items()):
    if label in legacy or label.startswith(('SM_GR2_','COL_GR2_','SM_GR3_','COL_GR3_')):
        unreal.EditorLevelLibrary.destroy_actor(a)

new_labels=[]
for name,mesh in meshes.items():
    if not name.startswith(('SM_GR2_','COL_GR2_','SM_GR3_','COL_GR3_')):continue
    actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0))
    actor.set_actor_label(name);actor.set_folder_path('Great room polished photo layout')
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_collision_profile_name('BlockAll' if name.startswith('COL') else 'NoCollision')
    if name.startswith('COL'):actor.static_mesh_component.set_visibility(False)
    new_labels.append(name)

after={a.get_actor_label():snapshot(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected_count=0
for name,row in before.items():
    if name not in affected and name not in legacy and not name.startswith(('SM_GR2_','COL_GR2_','SM_GR3_','COL_GR3_')):
        if after.get(name)!=row:raise RuntimeError('Unexpected map change '+name)
        protected_count+=1
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Map save failed')
with open(os.path.join(source,'applied_manifest.json'),'w') as f:
    json.dump({'map':'/Game/Story/Maps/Chapter01_House','newActors':new_labels,
        'changedChunks':layout['clearedChunks'],'beforeCount':len(before),'afterCount':len(after),
        'protectedActorsVerified':protected_count},f,indent=2)
unreal.log('GR3_COMPLETE_LAYOUT_SAVED '+str(len(new_labels))+' new actors')
