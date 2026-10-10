import bpy,bmesh,json
from pathlib import Path
root=Path(__file__).parent
out=[]
for name in ['HI_SMF_Upper_ALL_066','SM_OriginalRoom_Basement_m1_m1_Solid']:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=str(root/'source'/(name+'.fbx')))
    for obj in list(bpy.context.scene.objects):
        if obj.type!='MESH' or not obj.material_slots:continue
        bm=bmesh.new();bm.from_mesh(obj.data);bm.verts.ensure_lookup_table()
        # Weld split normals/UV seams for reliable connected-island bounds.
        bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0000001)
        remaining=set(bm.verts)
        while remaining:
            seed=remaining.pop();verts={seed};stack=[seed]
            while stack:
                v=stack.pop()
                for e in v.link_edges:
                    n=e.other_vert(v)
                    if n in remaining:remaining.remove(n);verts.add(n);stack.append(n)
            faces={f for v in verts for f in v.link_faces}
            mats={obj.material_slots[f.material_index].material.name for f in faces}
            if not any('Identity' in m for m in mats):continue
            pts=[obj.matrix_world@v.co for v in verts]
            out.append(dict(mesh=name,materials=sorted(mats),faces=len(faces),
                min=[min(p[i] for p in pts) for i in range(3)],max=[max(p[i] for p in pts) for i in range(3)]))
        bm.free()
(root/'evidence/islands.json').write_text(json.dumps(out,indent=2))
