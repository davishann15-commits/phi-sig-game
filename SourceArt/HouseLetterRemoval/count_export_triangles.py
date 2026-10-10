import bpy,json
from pathlib import Path
root=Path(__file__).parent
counts={}
for row in json.loads((root/'evidence/cleaned.json').read_text()):
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=row['file'])
    count=0;degenerate=0;areas=[]
    for o in bpy.context.scene.objects:
        if o.type!='MESH' or o.name.startswith(('UCX_','UBX_','USP_','UCP_')):continue
        o.data.calc_loop_triangles();count+=len(o.data.loop_triangles)
        for t in o.data.loop_triangles:
            a,b,c=[o.matrix_world@o.data.vertices[v].co for v in t.vertices]
            area=(b-a).cross(c-a).length*.5;areas.append(area)
            if area<1.e-10:degenerate+=1
    counts[row['asset']]=dict(total=count,degenerate=degenerate,smallest_areas=sorted(areas)[:8])
(root/'evidence/exported-triangles.json').write_text(json.dumps(counts,indent=2))
