"""Import and place the measured, complete photo-reference great room layout."""
import glob
import json
import os
import re
import unreal

project=unreal.Paths.project_dir()
source=os.path.join(project,'SourceArt/GreatRoomV2')
dest='/Game/SeniorSendOff/House/GreatRoomV2'
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
for name in ('T_GR_RoseRug','T_GR_GreenRug','T_GR_LeatherGrain','T_GR_Checker'):
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

def make_material(name,tint,roughness=.65,metallic=0,texture=None,leather=False,normal=None):
    path=dest+'/M_GR2_'+name
    m=assets.load_asset(path) if assets.does_asset_exist(path) else None
    if not m:m=asset_tools.create_asset('M_GR2_'+name,dest,unreal.Material,unreal.MaterialFactoryNew())
    editing.delete_all_material_expressions(m)
    base=color(m,tint)
    if texture:
        sample=expression(m,unreal.MaterialExpressionTextureSample,-650,-150)
        sample.texture=texture
        if leather:
            desat=expression(m,unreal.MaterialExpressionDotProduct,-450,-150)
            connect(sample,'RGB',desat,'A')
            connect(color(m,(.299,.587,.114)),'',desat,'B')
            mix=expression(m,unreal.MaterialExpressionLinearInterpolate,-180,-100)
            connect(base,'',mix,'A')
            connect(color(m,tuple(v*2.6 for v in tint)),'',mix,'B')
            connect(desat,'',mix,'Alpha')
            base=mix
        else:base=sample
    editing.connect_material_property(base,'RGB' if texture and not leather else '',unreal.MaterialProperty.MP_BASE_COLOR)
    editing.connect_material_property(constant(m,roughness),'',unreal.MaterialProperty.MP_ROUGHNESS)
    editing.connect_material_property(constant(m,metallic),'',unreal.MaterialProperty.MP_METALLIC)
    if normal:
        n=expression(m,unreal.MaterialExpressionTextureSample,-650,200);n.texture=normal
        n.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        editing.connect_material_property(n,'RGB',unreal.MaterialProperty.MP_NORMAL)
    if name.endswith('Rug'):m.set_editor_property('two_sided',True)
    editing.recompile_material(m)
    if not assets.save_loaded_asset(m):raise RuntimeError('Material save failed '+name)
    return m

wood_normal=assets.load_asset('/Game/SeniorSendOff/House/Textures/T_Wood_Normal')
fabric_normal=assets.load_asset('/Game/SeniorSendOff/House/Textures/T_Fabric_Normal')
leather=textures['T_GR_LeatherGrain']
new_materials={
    'BronzeLeather':make_material('BronzeLeather',(.15,.093,.049),.60,texture=leather,leather=True),
    'CognacLeather':make_material('CognacLeather',(.225,.104,.041),.55,texture=leather,leather=True),
    'OliveLeather':make_material('OliveLeather',(.075,.072,.047),.65,texture=leather,leather=True),
    'OxbloodLeather':make_material('OxbloodLeather',(.065,.026,.016),.57,texture=leather,leather=True),
    'Walnut':make_material('Walnut',(.13,.065,.026),.55,normal=wood_normal),
    'BlackMetal':make_material('BlackMetal',(.018,.016,.014),.43,.65),
    'GreenCord':make_material('GreenCord',(.025,.049,.032),.94,normal=fabric_normal),
    'CheckerCloth':make_material('CheckerCloth',(1,1,1),.94,texture=textures['T_GR_Checker'],normal=fabric_normal),
    'IvoryCloth':make_material('IvoryCloth',(.62,.59,.49),.94,normal=fabric_normal),
    'Brass':make_material('Brass',(.48,.29,.073),.48,.92),
    'Ceramic':make_material('Ceramic',(.70,.68,.59),.42),
    'RoseRug':make_material('RoseRug',(1,1,1),.97,texture=textures['T_GR_RoseRug'],normal=fabric_normal),
    'GreenRug':make_material('GreenRug',(1,1,1),.97,texture=textures['T_GR_GreenRug'],normal=fabric_normal),
}

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
    unreal.log('GR2_SAVED '+name+' '+str(mesh.get_bounds()))

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
    if label in legacy or label.startswith(('SM_GR2_','COL_GR2_')):
        unreal.EditorLevelLibrary.destroy_actor(a)

new_labels=[]
for name,mesh in meshes.items():
    if not name.startswith(('SM_GR2_','COL_GR2_')):continue
    actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0))
    actor.set_actor_label(name);actor.set_folder_path('Great room measured photo layout')
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_collision_profile_name('BlockAll' if name.startswith('COL') else 'NoCollision')
    if name.startswith('COL'):actor.static_mesh_component.set_visibility(False)
    new_labels.append(name)

after={a.get_actor_label():snapshot(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
for name,row in before.items():
    if name not in affected and name not in legacy and not name.startswith(('SM_GR2_','COL_GR2_')):
        if after.get(name)!=row:raise RuntimeError('Unexpected map change '+name)
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Map save failed')
with open(os.path.join(source,'applied_manifest.json'),'w') as f:
    json.dump({'map':'/Game/Story/Maps/Chapter01_House','newActors':new_labels,
        'changedChunks':layout['clearedChunks'],'beforeCount':len(before),'afterCount':len(after),
        'protectedActorsVerified':len(before)-len(affected)-len(legacy)},f,indent=2)
unreal.log('GR2_COMPLETE_LAYOUT_SAVED '+str(len(new_labels))+' new actors')
