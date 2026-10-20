import unreal,json,traceback
from pathlib import Path
OUT=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BoywickMetaHuman/evidence/probe.json')
report={}
a=unreal.EditorAssetLibrary
sub=unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
try:
 paths=a.list_assets('/Game/Characters/MetaHumans',recursive=True)
 report['sources']=[str(p) for p in paths if '/MH_' in p and a.find_asset_data(p).asset_class_path.asset_name=='MetaHumanCharacter']
 src='/Game/Characters/MetaHumans/Runner/MH_Runner_MaleFace'
 c=a.load_asset(src)
 if not sub.try_add_object_to_edit(c):raise RuntimeError('Edit refused')
 try:
  report['constraints']=[{'name':str(x.name),'active':x.is_active,'value':x.target_measurement} for x in sub.get_body_constraints(c)]
  report['landmarks']=[[v.x,v.y,v.z]for v in sub.get_face_landmarks(c)]
  report['buildable']=sub.can_build_meta_human(c)
  report['properties']=[x for x in dir(c) if not x.startswith('_')]
  report['collection']=str(sub.get_preview_collection(c))
  report['body_points']=str(sub.get_preset_body_key_points(c))
  report['skin']=str(c.get_editor_property('skin_settings'))
 finally:sub.remove_object_to_edit(c)
 report['wardrobe']=[str(x)for x in a.list_assets('/MetaHumanCharacter',recursive=True) if '/WI_' in str(x) and any(k in str(x)for k in ('Outfit','Cloth','Hair','Top','Pant'))]
 bp=a.load_asset('/Game/MetaHumans/RunnerMaleFace/MH_Runner_MaleFace/BP_MH_Runner_MaleFace')
 actor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(bp.generated_class(),unreal.Vector())
 report['components']=[{'name':x.get_name(),'type':x.get_class().get_name(),'mesh':str(x.get_editor_property('skeletal_mesh_asset')),'materials':[str(m)for m in x.get_materials()]}for x in actor.get_components_by_class(unreal.SkeletalMeshComponent)]
 unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
except Exception:report['error']=traceback.format_exc()
OUT.write_text(json.dumps(report,indent=2))
unreal.log('BOYWICK_PROBE_WRITTEN '+str(OUT))
frames=[0]
def quit_later(dt):
 frames[0]+=1
 if frames[0]>8:
  unreal.unregister_slate_post_tick_callback(handle)
  unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(quit_later)
