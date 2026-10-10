import unreal,json
from pathlib import Path
R=Path('/Users/Stewart/Projects/Senior sendoff/outputs/house-interactivity')
m=json.load(open(R/'extraction-manifest.json'))
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/Story/Maps/Chapter01_House')
def rot(r):return unreal.Rotator(pitch=r[0],yaw=r[1],roll=r[2])
def transform(d):return unreal.Transform(location=unreal.Vector(*d['location']),rotation=rot(d['rotation']),scale=unreal.Vector(*d['scale']))
actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
report=[]
for p in m['pieces']:
 a=actors['Interactive_'+p['id']]
 r=p['source_transform']['rotation'];before=a.get_actor_rotation()
 intended=rot(r);positional=unreal.Rotator(*r)
 t=transform(p['source_transform']);loc=t.transform_location(unreal.Vector(*p['local_origin_cm']))
 if any(abs(v)>.01 for v in r):report.append({'id':p['id'],'expected':r,'before':[before.pitch,before.yaw,before.roll],'positional':[positional.pitch,positional.yaw,positional.roll]})
 a.set_actor_location_and_rotation(loc,intended,False,True)
 a.set_actor_scale3d(unreal.Vector(*p['source_transform']['scale']))
 if isinstance(a,unreal.HouseReactiveFixture):a.refresh_fixture()
 else:a.refresh_prop()
 actual=a.get_actor_rotation()
 assert (actual.quaternion().angular_distance(intended.quaternion())<.0001),p['id']
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Story/Maps/Chapter01_House')
json.dump(report,open(R/'rotation-fix.json','w'),indent=2)
unreal.log('HOUSE_ROTATION_FIX saved599 nonzero='+str(len(report)))
