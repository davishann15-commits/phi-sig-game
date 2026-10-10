import bpy,runpy,json,re
from pathlib import Path
R=Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/GreatRoomTVs');s=runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/extract_interactivity.py')
d=json.load(open(R/'context/actors.json'));rows=[]
for label in ['SM_Entrance_p0_m1_Solid','SM_Entrance_p0_p0_Solid']:
 a=next(x for x in d if x['label']==label);c=a['components'][0];bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=c['export']);o=next(x for x in bpy.context.scene.objects if x.type=='MESH' and not x.name.startswith('UCX_'))
 ps=s['components'](o);tvs=[p for p in ps if .8<p['size'].z<1.2 and 1.5<p['size'].y<2 and p['size'].x<.2 and 2<p['center'].z<2.8 and any(m in p['materials'] for m in ['MI_SSO_Black','MI_SSO_MetalDark'])];assert len(tvs)==2
 cut={i for p in tvs for i in p['faces']};keep=set(range(len(o.data.polygons)))-cut;name='SM_GRTV_Retained_'+label;ob,slots=s['submesh'](o,keep,name)
 pathmap={p.rsplit('/',1)[-1].split('.')[0]:p for p in c['materials'] if p}
 mats=[pathmap[re.sub(r'\.\d+$','',m.name)] for m in ob.data.materials]
 s['export'].__globals__['OUT']=str(R/'models');bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob;f=str(R/'models'/(name+'.fbx'));bpy.ops.export_scene.fbx(filepath=f,use_selection=True,apply_unit_scale=True,axis_forward='-Z',axis_up='Y',add_leaf_bones=False,mesh_smooth_type='FACE')
 bounds=s['bounds'](tvs)
 rows.append(dict(label=label,asset_name=name,fbx=f,materials=mats,original_mesh=c['mesh'],source_faces=len(o.data.polygons),removed_tv_faces=len(cut),removed_tv_components=len(tvs),retained_faces=len(keep),conservation_pass=len(keep)+len(cut)==len(o.data.polygons),tv_bounds=[list(bounds[0]),list(bounds[1])]))
(R/'shell-manifest.json').write_text(json.dumps(rows,indent=2));print('TV_SHELLS_EXTRACTED',len(rows))
