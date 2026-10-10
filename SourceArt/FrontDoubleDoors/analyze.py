import bpy,bmesh,json
from pathlib import Path
B=Path(__file__).resolve().parent
rows=json.loads((B/'audit.json').read_text()); out=[]
for a in rows:
    if not a['label'].startswith(('SM_Entrance','COL_Entrance')):continue
    for c in a['components']:
        if not c.get('exported'):continue
        p=B/'meshes'/(c['mesh'].split('.')[-1]+'.fbx')
        bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(p))
        o=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith(('UCX','UBX')))
        bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001);bm.verts.ensure_lookup_table()
        seen=set();groups=[]
        for v in bm.verts:
            if v in seen:continue
            stack=[v];seen.add(v);vs=[]
            while stack:
                w=stack.pop();vs.append(w)
                for e in w.link_edges:
                    z=e.other_vert(w)
                    if z not in seen:seen.add(z);stack.append(z)
            ps=[o.matrix_world@w.co for w in vs]
            ps=[(p.x*100*c['scale'][0]+c['loc'][0],-p.y*100*c['scale'][1]+c['loc'][1],p.z*100*c['scale'][2]+c['loc'][2]) for p in ps]
            lo=[min(p[j] for p in ps) for j in range(3)];hi=[max(p[j] for p in ps) for j in range(3)]
            if hi[0]<-220 or lo[0]>150 or hi[1]<-960 or lo[1]>-890 or hi[2]<-5 or lo[2]>290:continue
            fs={f for w in vs for f in w.link_faces};mats=sorted(set(f.material_index for f in fs))
            groups.append(dict(verts=len(vs),faces=len(fs),lo=lo,hi=hi,mats=[o.data.materials[i].name if i<len(o.data.materials) else str(i) for i in mats]))
        out.append(dict(label=a['label'],mesh=c['mesh'],groups=groups))
(B/'islands.json').write_text(json.dumps(out,indent=2))
print('FRONT_DOOR_ISLANDS',json.dumps(out),flush=True)
