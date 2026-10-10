"""Import the spa room, preserve unrelated actors, and retire the middle doorway."""
import glob, json, os, re
import unreal

project=unreal.Paths.project_dir()
source=os.path.join(project,'SourceArt/HotTubRoomV1')
dest='/Game/SeniorSendOff/House/HotTubRoomV1'
assets=unreal.EditorAssetLibrary
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
editing=unreal.MaterialEditingLibrary

def import_file(path,name):
    task=unreal.AssetImportTask();task.filename=path;task.destination_path=dest
    task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True
    if path.endswith('.fbx'):
        ui=unreal.FbxImportUI();ui.import_mesh=True;ui.import_as_skeletal=False
        ui.import_materials=False;ui.import_textures=False
        ui.static_mesh_import_data.auto_generate_collision=False
        ui.static_mesh_import_data.combine_meshes=True
        task.options=ui
    asset_tools.import_asset_tasks([task])
    result=assets.load_asset(dest+'/'+name)
    if not result:raise RuntimeError('Import failed '+name)
    return result

def node(m,cls):return editing.create_material_expression(m,cls)
def scalar(m,v):
    n=node(m,unreal.MaterialExpressionConstant);n.r=v;return n
def color(m,v):
    n=node(m,unreal.MaterialExpressionConstant3Vector);n.constant=unreal.LinearColor(*v,1);return n
def uv(m,tiling=1):
    n=node(m,unreal.MaterialExpressionTextureCoordinate);n.u_tiling=tiling;n.v_tiling=tiling;return n
def connect(a,b,input='',output=''):
    if not input:
        names=editing.get_material_expression_input_names(b)
        if len(names)!=1:raise RuntimeError('Ambiguous inputs '+str(names))
        input=names[0]
    if not editing.connect_material_expressions(a,output,b,input):raise RuntimeError('Material connection '+input)
def prop(n,m,p,output=''):
    if not editing.connect_material_property(n,output,p):raise RuntimeError('Material property '+str(p))
def op(m,cls,a,b,aout='',bout=''):
    n=node(m,cls);connect(a,n,'A',aout);connect(b,n,'B',bout);return n
def mul(m,a,b,aout='',bout=''):return op(m,unreal.MaterialExpressionMultiply,a,b,aout,bout)
def add(m,a,b):return op(m,unreal.MaterialExpressionAdd,a,b)
def lerp(m,a,b,alpha,alphaout=''):
    n=node(m,unreal.MaterialExpressionLinearInterpolate)
    connect(a,n,'A');connect(b,n,'B');connect(alpha,n,'Alpha',alphaout);return n
def sample(m,texture,tiling=1):
    n=node(m,unreal.MaterialExpressionTextureSample);n.texture=texture;connect(uv(m,tiling),n,'UVs');return n
def function(m,path):
    f=unreal.load_object(None,path+'.'+path.rsplit('/',1)[-1])
    if not f:raise RuntimeError('Missing material function '+path)
    n=node(m,unreal.MaterialExpressionMaterialFunctionCall);n.set_editor_property('material_function',f);return n
def height_normal(m,texture,tiling,strength):
    tex=node(m,unreal.MaterialExpressionTextureObject);tex.texture=texture
    n=function(m,'/Engine/Functions/Engine_MaterialFunctions03/Procedurals/NormalFromHeightmap')
    connect(tex,n,'Height Map');connect(uv(m,tiling),n,'Coordinates')
    connect(scalar(m,strength),n,'Normal Map Intensity');connect(scalar(m,.002),n,'Height Map UV Offset')
    sel=op(m,unreal.MaterialExpressionAppendVector,color(m,(1,0,0)),scalar(m,0))
    connect(sel,n,'Height Map Channel Selector');return n
def material(name):
    path=dest+'/M_'+name
    m=assets.load_asset(path) if assets.does_asset_exist(path) else None
    if not m:m=asset_tools.create_asset('M_'+name,dest,unreal.Material,unreal.MaterialFactoryNew())
    editing.delete_all_material_expressions(m);return m
def save_material(m):
    editing.recompile_material(m)
    if not assets.save_loaded_asset(m):raise RuntimeError('Material save failed '+m.get_name())
    return m
def substrate_input(bsdf,name,value,output=''):
    normalize=lambda v:v.replace(' ','').replace('_','').lower()
    names=editing.get_material_expression_input_names(bsdf)
    matches=[v for v in names if normalize(v)==normalize(name)]
    if len(matches)!=1:raise RuntimeError('Substrate input '+name+' '+str(names))
    connect(value,bsdf,matches[0],output)

stone=import_file(os.path.join(source,'T_Spa_Travertine.png'),'T_Spa_Travertine')
wood=assets.load_asset('/Game/SeniorSendOff/House/GreatRoomV3/T_GR3_Walnut')
fabric=assets.load_asset('/Game/SeniorSendOff/House/Textures/T_Fabric_Normal')
if not wood or not fabric:raise RuntimeError('Missing preserved source textures')
palette={
 'Spa_Travertine':((.83,.82,.77),.73,0,stone),
 'Spa_Walnut':((.73,.63,.50),.52,0,wood),
 'Spa_Brass':((.51,.32,.10),.28,.9,None),
 'Spa_CreamAcrylic':((.76,.79,.73),.20,0,None),
 'Spa_DarkMetal':((.018,.021,.020),.50,.7,None),
 'Spa_Linen':((.73,.69,.58),.98,0,None),
 'Spa_GreenLeaf':((.032,.087,.031),.72,0,None),
 'Spa_TerraCotta':((.30,.11,.061),.82,0,None),
 'Spa_Sage':((.11,.20,.16),.93,0,None),
 'Spa_Art':((.28,.35,.28),.8,0,None),
 'Spa_Plaster':((.71,.69,.61),.88,0,None),
 'Spa_Grout':((.33,.31,.27),.98,0,None),
 'Spa_Glass':((.34,.54,.48),.12,0,None),
 'Spa_LampGlow':((1,.68,.30),.5,0,None),
 'Spa_Citrus':((.65,.34,.020),.7,0,None),
 'Spa_White':((.8,.79,.73),.78,0,None),
 'Spa_TowelStripe':((.055,.16,.125),.96,0,None),
 'Spa_Water':((.06,.22,.18),.15,0,None),
}
materials={}
for name,(tint,rough,metal,texture) in palette.items():
    m=material(name);base=color(m,tint)
    if texture:
        tiling=.65 if texture==wood else 1.3
        base=mul(m,sample(m,texture,tiling),base,'RGB')
        prop(height_normal(m,texture,tiling,.32 if texture==wood else .2),m,unreal.MaterialProperty.MP_NORMAL)
    elif name in ('Spa_Linen','Spa_Sage'):
        tex=sample(m,fabric,22);tex.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        flat=function(m,'/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal')
        connect(tex,flat,'Normal','RGB');connect(scalar(m,.8),flat,'Flatness')
        prop(flat,m,unreal.MaterialProperty.MP_NORMAL)
    prop(base,m,unreal.MaterialProperty.MP_BASE_COLOR)
    prop(scalar(m,rough),m,unreal.MaterialProperty.MP_ROUGHNESS)
    prop(scalar(m,metal),m,unreal.MaterialProperty.MP_METALLIC)
    if name=='Spa_GreenLeaf':m.set_editor_property('two_sided',True)
    if name=='Spa_LampGlow':prop(mul(m,base,scalar(m,3.5)),m,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if name=='Spa_Glass':
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('two_sided',True);prop(scalar(m,.23),m,unreal.MaterialProperty.MP_OPACITY)
    materials[name]=save_material(m)

# Single-layer water supports filled-volume transmission on the current Mac renderer.
water=material('Spa_ReactiveWater_v2')
water.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
water.set_editor_property('shading_model',next(getattr(unreal.MaterialShadingModel,k) for k in dir(unreal.MaterialShadingModel) if k.startswith('MSM') and 'WATER' in k))
water.set_editor_property('two_sided',True)
vertex=node(water,unreal.MaterialExpressionVertexColor)
foam=mul(water,vertex,scalar(water,.42),'R')
tex=node(water,unreal.MaterialExpressionTextureSample);tex.texture=assets.load_asset('/Game/SeniorSendOff/House/Textures/T_Wood_Normal')
tex.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
pan=node(water,unreal.MaterialExpressionPanner);pan.set_editor_property('speed_x',.025);pan.set_editor_property('speed_y',.019);connect(uv(water,2.2),pan,'Coordinate');connect(pan,tex,'UVs')
flat=function(water,'/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal');connect(tex,flat,'Normal','RGB');connect(scalar(water,.925),flat,'Flatness')
bsdf=node(water,unreal.MaterialExpressionSubstrateSingleLayerWaterBSDF)
for key,value in {'BaseColor':lerp(water,color(water,(.012,.075,.060)),color(water,(.67,.77,.69)),foam),'Metallic':scalar(water,0),'Specular':scalar(water,.55),'Roughness':scalar(water,.075),'Normal':flat,'TopMaterialOpacity':add(water,scalar(water,.10),foam),'WaterAlbedo':color(water,(.05,.30,.36)),'WaterExtinction':color(water,(.0125,.003,.004)),'WaterPhaseG':scalar(water,.2),'ColorScaleBehindWater':color(water,(1,1,1))}.items():substrate_input(bsdf,key,value)
prop(bsdf,water,unreal.MaterialProperty.MP_FRONT_MATERIAL)
save_material(water)
steam=material('Spa_Steam_v2')
steam.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
steam.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
steam.set_editor_property('translucency_pass',unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF)
steam.set_editor_property('two_sided',True);steam.set_editor_property('used_with_instanced_static_meshes',True)
cloud=unreal.load_object(None,'/Engine/EngineVolumetrics/Fogsheet/Materials/T_EV_FogSheet_CloudTex_01.T_EV_FogSheet_CloudTex_01')
cs=sample(steam,cloud)
offset=node(steam,unreal.MaterialExpressionConstant2Vector);offset.r=.5;offset.g=.5
delta=op(steam,unreal.MaterialExpressionSubtract,uv(steam),offset)
dot=op(steam,unreal.MaterialExpressionDotProduct,delta,delta)
length=node(steam,unreal.MaterialExpressionSquareRoot);connect(dot,length)
edge=node(steam,unreal.MaterialExpressionOneMinus);connect(mul(steam,length,scalar(steam,2.05)),edge)
soft=node(steam,unreal.MaterialExpressionSaturate);connect(edge,soft)
soft2=node(steam,unreal.MaterialExpressionPower);soft2.set_editor_property('const_exponent',1.5);connect(soft,soft2,'Base')
custom=node(steam,unreal.MaterialExpressionPerInstanceCustomData);custom.set_editor_property('data_index',0)
interp=node(steam,unreal.MaterialExpressionVertexInterpolator);connect(custom,interp)
cf=add(steam,mul(steam,cs,scalar(steam,.6),'R'),scalar(steam,.4))
alpha=mul(steam,mul(steam,mul(steam,soft2,cf),interp),scalar(steam,2.2))
emission=mul(steam,color(steam,(18,20,18)),alpha)
prop(alpha,steam,unreal.MaterialProperty.MP_OPACITY)
bsdf=node(steam,unreal.MaterialExpressionSubstrateUnlitBSDF)
substrate_input(bsdf,'EmissiveColor',emission)
oneMinus=node(steam,unreal.MaterialExpressionOneMinus);connect(alpha,oneMinus)
substrate_input(bsdf,'TransmittanceColor',color(steam,(1,1,1)))
prop(bsdf,steam,unreal.MaterialProperty.MP_FRONT_MATERIAL)
save_material(steam)

meshes={}
legacy_materials={}
legacy_mesh=assets.load_asset('/Game/ExistingHouseDoors/SM_OriginalRoom_Entrance_m1_m2_Solid')
for slot in legacy_mesh.static_materials:
    key=re.sub(r'_\d{3}$','',str(slot.material_slot_name).split('.')[0])
    legacy_materials[key]=slot.material_interface
for path in sorted(glob.glob(os.path.join(source,'*.fbx'))):
    name=os.path.basename(path)[:-4];mesh=import_file(path,name)
    if name.startswith('COL'):
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    else:
        for i,slot in enumerate(mesh.static_materials):
            slot_name=re.sub(r'_\d{3}$','',str(slot.material_slot_name).split('.')[0])
            target=materials.get(slot_name) or legacy_materials.get(slot_name)
            if not target:target=assets.load_asset('/Game/SeniorSendOff/House/Materials/MI_SSO_'+slot_name)
            if not target:raise RuntimeError('Unmapped material '+name+' '+slot_name)
            mesh.set_material(i,target)
    assets.save_loaded_asset(mesh);meshes[name]=mesh

unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
def snapshot(a):
    loc=a.get_actor_location();rot=a.get_actor_rotation();scale=a.get_actor_scale3d()
    row={'class':a.get_class().get_name(),'location':[loc.x,loc.y,loc.z],
         'rotation':[rot.roll,rot.pitch,rot.yaw],'scale':[scale.x,scale.y,scale.z]}
    if isinstance(a,unreal.StaticMeshActor):
        mesh=a.static_mesh_component.static_mesh;row['mesh']=mesh.get_path_name() if mesh else None
    return row
before={n:snapshot(a) for n,a in actors.items()}
changed={'COL_Entrance_p0_m2_Solid','SM_Entrance_m1_m2_Solid','COL_Entrance_m1_m2_Solid','OriginalDoor_Entrance_p0_m2_Solid_01'}
middle='OriginalDoor_Entrance_m1_m2_Solid_02'
for label,mesh_name in [('COL_Entrance_p0_m2_Solid','COL_SPA_Entrance_p0_m2_DoorClear'),('SM_Entrance_m1_m2_Solid','SM_SPA_Entrance_m1_m2_MiddleSealed'),('COL_Entrance_m1_m2_Solid','COL_SPA_Entrance_m1_m2_MiddleSealed')]:
    actor=actors[label];actor.static_mesh_component.set_static_mesh(meshes[mesh_name])
    actor.static_mesh_component.set_collision_profile_name('BlockAll' if label.startswith('COL') else 'NoCollision')
    if label.startswith('COL'):actor.static_mesh_component.set_visibility(False)
for label,a in list(actors.items()):
    if label==middle or label.startswith(('SM_SpaRoom_','COL_SpaRoom','SpaRoom_')):unreal.EditorLevelLibrary.destroy_actor(a)
entry=actors['OriginalDoor_Entrance_p0_m2_Solid_01']
entry.set_editor_property('doorway_center',unreal.Vector(-2358.5,499.5,118))
entry.set_editor_property('closed_yaw',0.0)
entry.set_editor_property('open_yaw',-90.0)
entry.set_actor_location(unreal.Vector(-2358.51,580.5,8),False,False)
entry.set_actor_location(unreal.Vector(-2358.5,580.5,8),False,False)

new_labels=[]
for name,mesh in meshes.items():
    if not name.startswith(('SM_SpaRoom_','COL_SpaRoom')):continue
    actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,0))
    actor.set_actor_label(name);actor.set_folder_path('First floor spa lounge')
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_collision_profile_name('BlockAll' if name.startswith('COL') else 'NoCollision')
    if name.startswith('COL'):actor.static_mesh_component.set_visibility(False)
    new_labels.append(name)
water_actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.HouseHotTubWater,unreal.Vector(-2795,425,73))
water_actor.set_actor_label('SpaRoom_ReactiveHotTubWater');water_actor.set_folder_path('First floor spa lounge')
for key,value in {'water_size':unreal.Vector2D(295,250),'water_depth':49.0,'corner_radius':22.0,
                  'water_material':water,'steam_material':steam,'steam_count':20,'steam_opacity':.20}.items():
    water_actor.set_editor_property(key,value)
water_actor.set_actor_location(unreal.Vector(-2795.01,425,73),False,False)
water_actor.set_actor_location(unreal.Vector(-2795,425,73),False,False)
water_actor.refresh_water()
new_labels.append(water_actor.get_actor_label())

def light(label,location,rotation,intensity,color_value,rect=False,size=(100,100),radius=350):
    cls=unreal.RectLight if rect else unreal.PointLight
    actor=unreal.EditorLevelLibrary.spawn_actor_from_class(cls,unreal.Vector(*location),unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
    actor.set_actor_label('SpaRoom_'+label);actor.set_folder_path('First floor spa lounge')
    comp=actor.get_component_by_class(unreal.RectLightComponent if rect else unreal.PointLightComponent)
    comp.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
    comp.set_editor_property('intensity',intensity);comp.set_editor_property('light_color',unreal.Color(*color_value,255))
    comp.set_editor_property('cast_shadows',label!='WaterGlow')
    comp.set_editor_property('attenuation_radius',float(radius))
    if rect:
        comp.set_editor_property('source_width',size[0]);comp.set_editor_property('source_height',size[1])
    else:comp.set_editor_property('source_radius',8.0)
    new_labels.append(actor.get_actor_label())
light('TubCeiling',(-2780,445,269),(-90,0,0),1900,(255,222,181),True,(220,200),550)
light('LoungeCeiling',(-2780,-75,269),(-90,0,0),2100,(255,231,196),True,(220,250),550)
light('EntryWarmth',(-2440,460,225),(0,0,0),300,(255,211,151),radius=225)
light('LoungeWest',(-2990,-100,185),(0,0,0),250,(255,204,132),radius=225)
light('TubWest',(-2990,485,185),(0,0,0),250,(255,204,132),radius=225)
light('WaterGlow',(-2795,425,47),(0,0,0),65,(134,222,199),radius=105)

after={a.get_actor_label():snapshot(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=0
for name,row in before.items():
    if name in changed or name==middle or name.startswith(('SM_SpaRoom_','COL_SpaRoom','SpaRoom_')):continue
    if after.get(name)!=row:raise RuntimeError('Unexpected actor change '+name)
    protected+=1
if middle in after:raise RuntimeError('Middle door still present')
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Map save failed')
json.dump({'map':'/Game/Story/Maps/Chapter01_House','actors':new_labels,'removedDoor':middle,
           'protectedActors':protected,'beforeCount':len(before),'afterCount':len(after)},
          open(os.path.join(source,'applied_manifest.json'),'w'),indent=2)
unreal.log('SPA_ROOM_SAVED protected='+str(protected)+' new='+str(len(new_labels)))
