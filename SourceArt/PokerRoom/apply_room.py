import json,os,unreal
ROOT='/Users/Stewart/Projects/Senior sendoff/outputs/poker-room-v1'
helper=os.path.join(unreal.Paths.project_content_dir(),'Python/apply_bedroom.py')
exec(compile(open(helper).read().split('\ntextures={}')[0],helper,'exec'))
DEST='/Game/SeniorSendOff/House/PokerRoom'
PLAN=json.load(open(ROOT+'/import/import_manifest.json'))
meshes={r['name']:A.load_asset(DEST+'/'+r['name']) for r in PLAN['meshes']}
native={r['name']:r for r in json.load(open(ROOT+'/import/native-assets.json'))}
assert abs(native['SM_POK_Chair']['boundsCm'][1][1]-28.65)<.01
assert abs(native['POK_SM_LastHandSign']['boundsCm'][1][1]-3.67)<.01
unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
def fullstate(a):
 s=state(a)
 if isinstance(a,unreal.PointLight):
  c=a.get_component_by_class(unreal.PointLightComponent)
  s['light']=[c.get_editor_property('intensity'),c.get_editor_property('attenuation_radius'),str(c.get_editor_property('light_color')),c.get_editor_property('cast_shadows')]
 return s
before={k:fullstate(a) for k,a in actors.items()}
for n,a in actors.items():
 if n.startswith('POK_'):unreal.EditorLevelLibrary.destroy_actor(a)
new=[]
def meshactor(label,mesh,loc=(0,0,0),yaw=0,scale=(1,1,1),collision=False):
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*loc),unreal.Rotator(pitch=0,yaw=yaw,roll=0))
 a.set_actor_label(label);a.set_folder_path('Second floor/Poker room');a.set_actor_scale3d(unreal.Vector(*scale))
 c=a.static_mesh_component;c.set_static_mesh(meshes[mesh] if isinstance(mesh,str) else mesh)
 c.set_collision_profile_name('BlockAll' if collision else 'NoCollision');c.set_editor_property('generate_overlap_events',False)
 if collision:c.set_visibility(False);c.set_hidden_in_game(True);c.set_cast_shadow(False)
 new.append(label);return a
G=json.load(open(ROOT+'/geometry/incremental_manifest.json'))
for r in G['actors']:meshactor(r['label'],r['mesh'])
replacement='HallEnd_SM_HallEnd_ThreeRoomsPracticalLights'
actors[replacement].static_mesh_component.set_static_mesh(meshes['POK_SM_OtherHallPracticalLights'])
actors[replacement].static_mesh_component.set_editor_property('override_materials',[])
C=[-2685,-1110,302.3]
meshactor('POK_Table','SM_POK_Table',C)
meshactor('POK_Table_Collision','SM_POK_Table_Collision',C,collision=True)
chairs=json.load(open(ROOT+'/furniture/incremental_manifest.json'))['suggestedActorPoses']
cube=A.load_asset('/Engine/BasicShapes/Cube')
import math
for r in chairs:
 p=[C[i]+r['localPositionCm'][i] for i in range(3)];yaw=r['yawDegrees']+180
 meshactor(r['name'],r['mesh'],p,yaw)
 angle=math.radians(yaw);oy=.8534565
 center=[p[0]-math.sin(angle)*oy,p[1]+math.cos(angle)*oy,p[2]+46.3918]
 meshactor(r['name']+'_Collision',cube,center,yaw,[.58,.555931,.908164],True)
board=[-2374,-1005,302.3]
meshactor('POK_Sideboard','SM_POK_Sideboard',board,90)
meshactor('POK_Sideboard_Collision','SM_POK_Sideboard_Collision',board,90,collision=True)
meshactor('POK_Pendant','SM_POK_Pendant',[-2685,-1110,566.1])
meshactor('POK_TableDetails','POK_SM_TablePack',[-2685,-1110,379.4])
meshactor('POK_Deck','POK_SM_Deck',[-2748,-1122,379.4])
for i,p in enumerate([[-97,-42],[-103,35],[100,-38],[96,40],[-38,-49],[39,-49],[-38,48],[39,48]]):
 meshactor('POK_SeatChips_%02d'%i,['POK_SM_ChipStack5','POK_SM_ChipStack25','POK_SM_ChipStack100'][i%3],[C[0]+p[0],C[1]+p[1],379.4])
meshactor('POK_Drinks','POK_SM_BarClutter',[-2374,-1005,396.05],90)
meshactor('POK_LastHandSign','POK_SM_LastHandSign',[-2685,-891,434],180)
meshactor('POK_SpadePrint','POK_SM_SpadeArt',[-2966,-1120,429],-90)
meshactor('POK_HeartPrint','POK_SM_HeartArt',[-2780,-1366,429],0)
meshactor('POK_Clock','POK_SM_Clock',[-2465,-891,470],180)
# The runtime actor owns only the focus box and five animated cards.
table=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.HousePokerTable,unreal.Vector(*C))
table.set_actor_label('POK_PokerTable');table.set_folder_path('Second floor/Poker room')
table.set_editor_property('card_mesh',meshes['POK_SM_Card'])
table.set_editor_property('card_material',A.load_asset(DEST+'/POK_M_CardAtlas'))
table.set_editor_property('deck_origin',unreal.Vector(-63,-12,79.4))
dest=[unreal.Transform(location=unreal.Vector((i-2)*14,35,77.12),rotation=unreal.Rotator(pitch=0,yaw=180+(i-2)*3,roll=0),scale=unreal.Vector(1,1,1)) for i in range(5)]
table.set_editor_property('card_destinations',dest)
table.set_actor_location(unreal.Vector(C[0]+.01,C[1],C[2]),False,False);table.set_actor_location(unreal.Vector(*C),False,False)
new.append('POK_PokerTable')
def light(a,loc,lumens,radius,color,source):
 a.set_actor_location(unreal.Vector(*loc),False,False)
 c=a.get_component_by_class(unreal.PointLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE)
 c.set_editor_property('intensity_units',unreal.LightUnits.LUMENS);c.set_intensity(lumens);c.set_attenuation_radius(radius)
 c.set_light_color(unreal.LinearColor(*color,1));c.set_editor_property('cast_shadows',False);c.set_editor_property('source_radius',source)
light(actors['HallEnd_west_Practical'],[-2685,-1110,492],2200,620,[1,.79,.52],25)
for label,loc,lumens,radius in [('POK_WestSconce1',[-2947,-1230,473],180,350),('POK_WestSconce2',[-2947,-1010,473],180,350),('POK_BarLight',[-2430,-1005,470],380,310)]:
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*loc));a.set_actor_label(label);a.set_folder_path('Second floor/Poker room')
 light(a,loc,lumens,radius,[1,.72,.43],12);new.append(label)
after={a.get_actor_label():fullstate(a) for a in unreal.EditorLevelLibrary.get_all_level_actors()}
changed={replacement,'HallEnd_west_Practical'}
protected=[n for n in before if not n.startswith('POK_') and n not in changed]
errors=[n for n in protected if before[n]!=after.get(n)]
if errors:raise RuntimeError('Protected actor changed '+str(errors))
registry=os.path.join(unreal.Paths.project_content_dir(),'HouseReview/original_house_doors.json')
assert open(registry,'rb').read()==open(ROOT+'/backup/original_house_doors.json','rb').read()
if not unreal.EditorLevelLibrary.save_current_level():raise RuntimeError('Map save failed')
json.dump({'changed':sorted(changed),'new':new,'protected':len(protected),'allProtectedUnchanged':True,'actors':after},open(ROOT+'/import/applied.json','w'),indent=2)
unreal.log('POKER_ROOM_APPLY_PASSED new=%d protected=%d'%(len(new),len(protected)))
