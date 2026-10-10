import bpy,bmesh,json
from pathlib import Path
root=Path(__file__).parent
(root/'clean').mkdir(exist_ok=True)
rows=[]
for rec in json.loads((root/'candidates.json').read_text()):
    name=rec['asset'].rsplit('.',1)[-1]
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    for mat in list(bpy.data.materials):bpy.data.materials.remove(mat)
    bpy.ops.import_scene.fbx(filepath=str(root/(name+'.fbx')))
    removed=0
    for obj in bpy.context.scene.objects:
        if obj.type!='MESH' or not obj.material_slots:continue
        bm=bmesh.new();bm.from_mesh(obj.data);bm.faces.ensure_lookup_table()
        index=bm.faces.layers.int.new('source_face')
        for i,f in enumerate(bm.faces):f[index]=i
        welded=bm.copy();idx=welded.faces.layers.int.get('source_face')
        bmesh.ops.remove_doubles(welded,verts=list(welded.verts),dist=1e-7)
        remaining=set(welded.verts);discard=set()
        while remaining:
            seed=remaining.pop();verts={seed};stack=[seed]
            while stack:
                v=stack.pop()
                for e in v.link_edges:
                    n=e.other_vert(v)
                    if n in remaining:remaining.remove(n);verts.add(n);stack.append(n)
            faces={f for v in verts for f in v.link_faces}
            pts=[obj.matrix_world@v.co for v in verts]
            mats={obj.material_slots[f.material_index].material.name for f in faces}
            if mats and all('Brick' in m for m in mats) and min(p.y for p in pts)>4.99 and max(p.y for p in pts)>7.1 and max(p.x for p in pts)-min(p.x for p in pts)<.15:
                assert len(faces)==188,(name,len(faces))
                discard.update(f[idx] for f in faces)
        welded.free()
        doomed=[f for f in bm.faces if f[index] in discard]
        removed+=len(doomed);bmesh.ops.delete(bm,geom=doomed,context='FACES')
        bm.faces.layers.int.remove(index);bm.to_mesh(obj.data);bm.free()
    assert removed==188,(name,removed)
    output=root/'clean'/(name+'.fbx')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.export_scene.fbx(filepath=str(output),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
        axis_forward='-Z',axis_up='Y',mesh_smooth_type='FACE',use_mesh_modifiers=False,add_leaf_bones=False,bake_anim=False)
    rows.append(dict(asset=rec['asset'],file=str(output),removed=removed))
(root/'cleaned.json').write_text(json.dumps(rows,indent=2))
