import bpy, json, math
from pathlib import Path
from mathutils import Vector
root=Path(__file__).parent
report=json.loads((root/'evidence/audit.json').read_text())
primary={r['mesh'] for r in report['candidates']}
allparts=[]
for rec in report['meshes']:
    if rec['asset'] not in primary: continue
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    bpy.ops.import_scene.fbx(filepath=rec['file'])
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
    points=[]
    for o in objects:
        if not o.material_slots:
            bpy.data.objects.remove(o,do_unlink=True)
            continue
        keep=[]
        for p in o.data.polygons:
            mat=o.material_slots[p.material_index].material
            if mat and ('Identity' in mat.name or 'PhiSig' in rec['asset']):
                keep.append(p)
        if not keep:
            bpy.data.objects.remove(o,do_unlink=True)
            continue
        groups={}
        for p in keep:
            groups.setdefault(o.material_slots[p.material_index].material.name,[]).append(p)
        for name,polys in groups.items():
            vertices=sorted({v for p in polys for v in p.vertices})
            pts=[o.matrix_world@o.data.vertices[v].co for v in vertices]
            allparts.append(dict(mesh=Path(rec['file']).stem, material=name,
                vertices=len(vertices),faces=len(polys),
                min=[min(p[i] for p in pts) for i in range(3)],
                max=[max(p[i] for p in pts) for i in range(3)]))
        points += [o.matrix_world@o.data.vertices[v].co for p in keep for v in p.vertices]
        # Hide other geometry for the inspection of lettering only.
        import bmesh
        bm=bmesh.new();bm.from_mesh(o.data);bm.faces.ensure_lookup_table()
        wanted={p.index for p in keep}
        bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.index not in wanted],context='FACES')
        bm.to_mesh(o.data);bm.free()
        for s in o.material_slots:
            if s.material: s.material.diffuse_color=(.9,.2,.12,1) if 'Red' in s.material.name else (.88,.78,.56,1)
    if not points: continue
    lo=Vector([min(p[i] for p in points) for i in range(3)])
    hi=Vector([max(p[i] for p in points) for i in range(3)])
    center=(lo+hi)/2; ext=hi-lo
    scene=bpy.context.scene
    scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO'
    scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=False
    scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD'
    scene.world.color=(.05,.05,.05)
    scene.render.resolution_x=1100;scene.render.resolution_y=850;scene.render.resolution_percentage=100
    for axis in (0,1):
        direction=Vector((1,0,0) if axis==0 else (0,-1,0))
        cam_data=bpy.data.cameras.new('Camera');cam=bpy.data.objects.new('Camera',cam_data);scene.collection.objects.link(cam)
        cam.location=center+direction*max(ext.length,1)*2
        cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
        cam_data.type='ORTHO';cam_data.ortho_scale=max(ext[1-axis]*.82,ext.z, .3)*1.35
        scene.camera=cam;scene.render.filepath=str(root/'evidence'/(Path(rec['file']).stem+'_axis'+str(axis)+'.png'))
        bpy.ops.render.render(write_still=True)
(root/'evidence/geometry.json').write_text(json.dumps(allparts,indent=2))
