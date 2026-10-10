import json,os,unreal,math
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/pigsty-room-v1'
helper=os.path.join(unreal.Paths.project_content_dir(),'Python/apply_bedroom.py')
exec(compile(open(helper).read().split('\ntextures={}')[0],helper,'exec'))
DEST='/Game/SeniorSendOff/House/PigstyRoom';PLAN=json.load(open(ROOT+'/import/import_manifest.json'))
meshes={r['name']:A.load_asset(DEST+'/'+r['name']) for r in PLAN['meshes']}
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
def fullstate(a):
 s=state(a)
 if isinstance(a,unreal.StaticMeshActor):
  c=a.static_mesh_component;s['visible']=c.is_visible();s['hidden']=c.get_editor_property('hidden_in_game');s['castShadow']=c.get_editor_property('cast_shadow')
 if isinstance(a,unreal.PointLight):
  c=a.get_component_by_class(unreal.PointLightComponent);s['light']=[c.get_editor_property('intensity'),c.get_editor_property('attenuation_radius'),str(c.get_editor_property('light_color')),c.get_editor_property('cast_shadows')]
 return s
before={k:fullstate(a) for k,a in actors.items()};removed=[];new=[];changed=set()
for n,a in actors.items():
 if n.startswith(('POK_','PIG_')):unreal.EditorLevelLibrary.destroy_actor(a);removed.append(n)
def meshactor(label,mesh,loc=(0,0,0),yaw=0,scale=(1,1,1),collision=False):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
 a.set_actor_label(label);a.set_folder_path('Second floor/Pigsty');a.set_actor_scale3d(unreal.Vector(*scale))
 c=a.static_mesh_component;c.set_static_mesh(meshes[mesh] if isinstance(mesh,str) else mesh)
 c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');c.set_editor_property('generate_overlap_events',False)
 if collision:c.set_visibility(False);c.set_hidden_in_game(True);c.set_cast_shadow(False)
 new.append(label);return a
G=PLAN['geometry']
for r in G['replacements']:
 a=actors[r['actor']];c=a.static_mesh_component;c.set_static_mesh(meshes[r['mesh']]);c.set_editor_property('override_materials',[]);changed.add(r['actor'])
 c.set_collision_profile_name('BlockAll' if r.get('collision') else 'NoCollision')
for r in G['actors']:meshactor(r['label'],r['mesh'],collision=r.get('collision')=='BlockAll')
mat=[-2825,-920,302.3]
meshactor('PIG_Mattress','PIG_SM_Mattress',mat)
meshactor('PIG_MattressCollision','PIG_SM_Mattress_COL',mat,collision=True)
meshactor('PIG_Blanket','PIG_SM_Blanket',[-2874,-920,324.8])
meshactor('PIG_CheetosBag','PIG_SM_CheetosBag',[-2668,-1038,302.6],-24)
meshactor('PIG_Crumbs','PIG_SM_OrangeCrumbs',[-2675,-1110,302.8],23)
meshactor('PIG_WaterTrough','PIG_SM_WaterTrough',[-2450,-835,302.5])
meshactor('PIG_FeedBucket','PIG_SM_FeedBucket',[-2888,-805,302.3],12)
meshactor('PIG_BrokenBoards','PIG_SM_BrokenBoards',[-2870,-1308,302.4],-8)
for i,(x,y,yaw,size) in enumerate([(-2840,-980,45,1),(-2890,-1240,10,1.5),(-2515,-810,-20,1.3),(-2740,-1327,80,1),(-2875,-1045,115,1.2),(-2510,-1295,67,1)]):
 meshactor('PIG_Straw_%02d'%i,'PIG_SM_StrawPatch',[x,y,302.5],yaw,[size,size,1])
# Visible practical sources with a low warm room fill and a cooler damp corner.
meshactor('PIG_BulkheadLamp','PIG_SM_BulkheadLamp',[-2700,-770,476],180)
meshactor('PIG_Vent','PIG_SM_WallVent',[-2468,-770,486],180)
meshactor('PIG_OutsideDoorPlaque','PIG_SM_KeepShutPlaque',[-2318,-1112,427],90)
meshactor('PIG_KeepShutSign','PIG_SM_KeepShutPlaque',[-2395,-771,430],0)
meshactor('PIG_WallScratch_West','PIG_SM_GrimeScratch',[-2970,-1120,318],90,[1.3,1,1.2])
meshactor('PIG_WallScratch_North','PIG_SM_GrimeScratch',[-2860,-766,319],0,[1.4,1,1.4])
# Low obstructions are physical without making decorative straw/food expensive.
cube=A.load_asset('/Engine/BasicShapes/Cube')
for label,loc,scale in [('PIG_TroughCollision',[-2450,-835,319.3],[1.4,.46,.34]),('PIG_BucketCollision',[-2888,-805,320.3],[.32,.32,.36])]:meshactor(label,cube,loc,scale=scale,collision=True)
food=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.HousePigSnackPile,unreal.Vector(-2675,-1110,302.3));food.set_actor_label('PIG_EdibleCheetos');food.set_folder_path('Second floor/Pigsty')
for key,val in [('cheeto_mesh',meshes['PIG_SM_Cheeto']),('food_count',24),('scatter_radius',50),('scatter_seed',2481),('feeding_offset_a',unreal.Vector(-90,0,-.5)),('feeding_offset_b',unreal.Vector(90,0,-.5))]:food.set_editor_property(key,val)
p=food.get_actor_location();food.set_actor_location(p+unreal.Vector(.01,0,0),False,False);food.set_actor_location(p,False,False);new.append('PIG_EdibleCheetos')
for label,name,seed,loc,yaw in [('PIG_Pepper','Pepper',173,[-2800,-1185,350.3],20),('PIG_Coal','Coal',284,[-2570,-970,350.3],-120)]:
 pig=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.HouseRoamingPig,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0));pig.set_actor_label(label);pig.set_folder_path('Second floor/Pigsty')
 for key,val in [('body_mesh',meshes['SM_PIG_BlackBody']),('head_mesh',meshes['SM_PIG_BlackHead']),('upper_leg_mesh',meshes['SM_PIG_BlackUpperLeg']),('lower_leg_mesh',meshes['SM_PIG_BlackLowerLeg']),('pig_name',name),('personality_seed',seed),('snack_pile',food),('bed_location',unreal.Vector(-2825,-920,323.3)),('pigsty_minimum',unreal.Vector(-2972,-1371.8425,285)),('pigsty_maximum',unreal.Vector(-2349.2328,-765,380))]:pig.set_editor_property(key,val)
 pig.set_actor_location(unreal.Vector(loc[0]+.01,loc[1],loc[2]),False,False);pig.set_actor_location(unreal.Vector(*loc),False,False);new.append(label)
def light(a,loc,lumens,radius,color,source,shadow=False):
 a.set_actor_location(unreal.Vector(*loc),False,False);c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(lumens);c.set_attenuation_radius(radius);c.set_light_color(unreal.LinearColor(*color,1));c.set_editor_property('cast_shadows',shadow);c.set_editor_property('source_radius',source)
light(actors['HallEnd_west_Practical'],[-2700,-813,491],2100,650,[1,.64,.28],12,False);changed.add('HallEnd_west_Practical')
for label,loc,lumens,radius,color in [('PIG_DampCornerFill',[-2900,-1170,478],650,480,[.50,.63,.72]),('PIG_FloorBounce',[-2650,-1070,440],1100,450,[.90,.76,.52])]:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*loc));a.set_actor_label(label);a.set_folder_path('Second floor/Pigsty');light(a,loc,lumens,radius,color,30);new.append(label)
after={a.get_actor_label():fullstate(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
protected=[n for n in before if n not in removed and n not in changed]
errors=[n for n in protected if before[n]!=after.get(n)]
if errors:raise RuntimeError('Protected actor changed '+str(errors))
assert not any(n.startswith('POK_') for n in after)
assert sum(a.get_class().get_name()=='HouseRoamingPig' for a in unreal.EditorLevelLibrary.get_all_level_actors())==2
registry=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json');assert open(registry,'rb').read()==open(ROOT+'/backup/original_house_doors.json','rb').read()
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Map save failed')
json.dump({'changed':sorted(changed),'removed':removed,'new':new,'protected':len(protected),'allProtectedUnchanged':True,'doorRegistryUnchanged':True,'actors':after},open(ROOT+'/import/applied.json','w'),indent=2)
unreal.log('PIGSTY_ROOM_APPLY_PASSED new=%d removed=%d protected=%d'%(len(new),len(removed),len(protected)))
