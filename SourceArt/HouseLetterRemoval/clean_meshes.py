"""Remove inspected lettering geometry only. Preserve collision and object transforms."""
import bpy,bmesh,json,re,runpy
from pathlib import Path
root=Path(__file__).parent
(root/'clean').mkdir(exist_ok=True)
audit=json.loads((root/'evidence/audit.json').read_text())
result=[]
for rec in audit['meshes']:
    name=Path(rec['file']).stem
    if name.startswith('HI_SMF_Upper_ALL_066_'):continue
    if not (name.startswith(('HI_SMF_Upper_ALL_062','HI_SMF_Upper_ALL_066')) or name in (
        'SM_Upper_p0_m1_Solid','SM_OriginalRoom_Basement_m1_m1_Solid','SM_Basement_m1_p0_Solid_NoodleCut')):continue
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    for mat in list(bpy.data.materials):bpy.data.materials.remove(mat)
    bpy.ops.import_scene.fbx(filepath=rec['file'])
    removed=0; kept=0;objects=[]
    for obj in bpy.context.scene.objects:
        if obj.type!='MESH':continue
        objects.append(obj.name)
        if not obj.material_slots or obj.name.startswith(('UCX_','UBX_','USP_','UCP_')):continue
        bm=bmesh.new();bm.from_mesh(obj.data)
        discard=[]
        for f in bm.faces:
            mat=obj.material_slots[f.material_index].material.name
            p=obj.matrix_world@f.calc_center_median()
            drop=False
            if name.startswith('HI_SMF_Upper_ALL_062'):
                drop='IdentityScroll' in mat
            elif name.startswith('HI_SMF_Upper_ALL_066'):
                # Label geometry sits in front of the continuous striped cloth.
                # Damage stages retain those same face groups and UVs.
                drop='IdentityRed' in mat and abs(p.y)<.48 and abs(p.z)<.195 and p.x>-.0015
            elif name=='SM_Upper_p0_m1_Solid':
                drop='Identity' in mat
            elif name=='SM_Basement_m1_p0_Solid_NoodleCut':
                drop='IdentityRed' in mat
            else:
                # Preserve the unrelated red/white T emblems at y=4.18m.
                drop='IdentityRed' in mat and (p.x < -6.7 or (3.22<p.y<3.24 and p.z < -2))
            if drop:discard.append(f)
        removed+=len(discard)
        bmesh.ops.delete(bm,geom=discard,context='FACES')
        kept+=len(bm.faces)
        bm.to_mesh(obj.data);bm.free()
    assert removed>0,(name,'no lettering removed')
    assert kept>0,(name,'object unexpectedly empty')
    path=root/'clean'/(name+'.fbx')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',
        use_mesh_modifiers=False,add_leaf_bones=False,bake_anim=False)
    result.append(dict(asset=rec['asset'],file=str(path),removed_faces=removed,kept_faces=kept,objects=objects))
# Rebuild the flag's three fracture stages from the clean flag, so letters
# cannot return on moved/rotated fragments when it is hit or destroyed.
generator=runpy.run_path(str(root.parent/'HouseInteractivity/source/generate_damage_v2.py'))
fn=generator['create_stages'];fn.__globals__['OUT']=str(root/'clean')
base=next(r for r in audit['meshes'] if r['asset'].endswith('.HI_SMF_Upper_ALL_066'))
row=dict(asset_name='HI_SMF_Upper_ALL_066',id='SMF_Upper_ALL__066',
    intact_fbx=str(root/'clean/HI_SMF_Upper_ALL_066.fbx'),
    material_slots=[s['material'] for s in base['slots']])
changes,stage_audit=fn(row)
for stage in ('damaged','heavily_damaged','broken'):
    name=changes[stage+'_asset_name'];path=changes[stage+'_fbx']
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=path)
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(filepath=path,use_selection=True,object_types={'MESH'},
        apply_unit_scale=True,axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',
        use_mesh_modifiers=False,add_leaf_bones=False,bake_anim=False)
    result.append(dict(asset='/Game/SeniorSendOff/House/Interactivity/'+name+'.'+name,
        file=path,regenerated_from_clean_flag=True,geometry=stage_audit['stages'][stage]))
(root/'evidence/cleaned.json').write_text(json.dumps(result,indent=2))
print('LETTER_CLEAN_COMPLETE',len(result))
