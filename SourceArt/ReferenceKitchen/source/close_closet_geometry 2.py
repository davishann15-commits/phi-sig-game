import bpy,json,runpy,re
from pathlib import Path
from mathutils import Vector
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/ReferenceKitchen');D=R/'models';DM=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py');S=DM['S'];doc=json.loads((R/'manifest.json').read_text());audit=[]
for label in ['SM_Entrance_p0_p1_Solid','COL_Entrance_p0_p1_Solid','SM_Entrance_m1_p1_Solid','COL_Entrance_m1_p1_Solid']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(R/'context/fbx'/(label+'.fbx')))
 candidates=[o for o in bpy.context.selected_objects if o.type=='MESH' and not o.name.startswith(('UCX_','UBX_','UCP_','USP_'))];o=max(candidates,key=lambda o:len(o.data.polygons));bpy.context.view_layer.objects.active=o;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 if not o.data.materials:o.data.materials.append(bpy.data.materials.new('M_Kitchen_WhiteAppliancePaint'))
 remove=[]
 for p in S['components'](o):
  c=p['center'];lo=p['lo'];hi=p['hi']
  leaf=11.55<c.x<12.42 and -.28<c.y<.26 and .30<c.z<2.13 and max(hi-lo)<2.4
  tv=11.58<c.x<11.75 and -2.1<c.y<-.9 and .8<c.z<2.3 and max(hi-lo)<1.7
  trim=label in ['SM_Entrance_m1_p1_Solid','COL_Entrance_m1_p1_Solid'] and 11.65<c.x<12.6 and -.45<c.y<.30 and .01<c.z<2.3 and max(hi-lo)<1.2
  if (leaf or tv) and label not in ['SM_Entrance_m1_p1_Solid','COL_Entrance_m1_p1_Solid'] or trim:remove+=p['faces'];audit.append(dict(source=label,bounds=[list(lo),list(hi)],faces=len(p['faces'])))
 if label=='COL_Entrance_m1_p1_Solid' and not remove:continue
 if label.startswith('SM_') and not remove:raise RuntimeError('Expected open closet leaf was not found')
 fs=[i for i in range(len(o.data.polygons))if i not in set(remove)];n,used=S['submesh'](o,fs,'SM_Kitchen_'+label)
 for v in n.data.vertices:v.co.x*=1.8;v.co.y*=1.8
 name='SM_Kitchen_'+label;file=D/(name+'.fbx');bpy.ops.object.select_all(action='DESELECT');n.select_set(True);bpy.context.view_layer.objects.active=n;bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
 lo,hi=DM['bbox'](n);paths=[];names=[]
 for m in n.data.materials:
  st=re.sub(r'\.\d+$','',m.name);names.append(m.name)
  paths.append('/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial' if st=='WorldGridMaterial' else ('/Game/SeniorSendOff/House/ReferenceKitchen/' if st.startswith('M_Kitchen_') else '/Game/SeniorSendOff/House/Materials/')+st+'.'+st)
 doc['assets']=[a for a in doc['assets']if a['id']!=label]
 doc['assets'].append(dict(id=label,asset_name=name,intact_fbx=str(file),material_slots=paths,material_names=names,bounds_blender_metres=[list(lo),list(hi)],behavior='architecture',damage=False,source_actor=label,mass_kg=0,break_health=0,movable=False,wall=False,vertices=len(n.data.vertices),faces=len(n.data.polygons)))
(R/'manifest.json').write_text(json.dumps(doc,indent=2));(R/'closet-geometry-audit.json').write_text(json.dumps(audit,indent=2));print('CLOSET_OPEN_LEAF_REMOVED',audit)
