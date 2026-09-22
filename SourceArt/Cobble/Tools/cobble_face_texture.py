"""Project the supplied frontal reference onto the existing, rigged face UVs."""
import bpy
import os
import struct
import sys
import numpy as np

WORK = '/Users/Stewart/Documents/Codex/2026-09-11/c/work'
REFERENCE = '/Users/Stewart/Downloads/COBBLE'

# Top-left coordinates in the upright photograph; left/right mean image left/right.
LANDMARKS = [
    # Full-resolution conversion of IMG_1979.HEIC, identical in Braxton and COBBLE.
    dict(file=WORK+'/cobble-reference/IMG_1979.jpg', eye=(.4920,.1258), eyehalf=.02555, nose=(.4934,.1447), mouth=(.4934,.1617), chin=(.4927,.1968), hair=(.4927,.1020), left=.4320, right=.5490),
    dict(file=WORK+'/cobble-reference/IMG_1982.jpg', eye=(.4792,.0847), eyehalf=.02595, nose=(.4781,.1025), mouth=(.4795,.1228), chin=(.4810,.1617), hair=(.4737,.0598), left=.4240, right=.5358),
    dict(file=WORK+'/cobble-reference/IMG_1985.jpg', eye=(.4539,.1678), eyehalf=.01825, nose=(.4547,.1798), mouth=(.4547,.1930), chin=(.4554,.2220), hair=(.4532,.1442), left=.4079, right=.4942),
    dict(file=WORK+'/cobble-reference/IMG_1988.jpg', eye=(.4967,.1310), eyehalf=.02080, nose=(.4985,.1491), mouth=(.4985,.1672), chin=(.4978,.2001), hair=(.4956,.0987), left=.4496, right=.5439),
    dict(file=REFERENCE+'/IMG_1993.JPG', eye=(.4791,.1772), eyehalf=.02265, nose=(.4803,.1917), mouth=(.4812,.2090), chin=(.4829,.2429), hair=(.4769,.1532), left=.4291, right=.5333),
    dict(file=REFERENCE+'/IMG_1994.JPG', eye=(.5038,.0516), eyehalf=.02775, nose=(.5000,.0782), mouth=(.5017,.0994), chin=(.5000,.1397), hair=(.5043,.0237), left=.4496, right=.5556),
    dict(file=REFERENCE+'/IMG_1999.JPG', eye=(.4953,.0391), eyehalf=.02520, nose=(.4940,.0609), mouth=(.4932,.0821), chin=(.4949,.1288), hair=(.4949,.0064), left=.4368, right=.5538),
    dict(file=REFERENCE+'/IMG_2003.JPG', eye=(.5214,.1667), eyehalf=.02225, nose=(.5239,.1840), mouth=(.5222,.2000), chin=(.5214,.2288), hair=(.5214,.1404), left=.4786, right=.5650),
]
EYEBROW_V = (.1150,.0685,.1579,.1200,.1667,.0372,.0237,.1570)
# Photo-light correction measured from dark brow and unaffected neck patches.
# All values are LINEAR RGB. The stronger upper-face veil fades at the nose.
PHOTO_LIGHT = {
    1: ((.02897,.02171,.02632),(.00724,.00543,.00658),(1.04274,1.11274,1.19263),(.33423,.19924,.18173)),
    2: ((0,0,0),(0,0,0),(.86571,.75755,.71884),(.24513,.13476,.15256)),
    3: ((.02878,.01510,.01557),(.00720,.00377,.00389),(1.04283,1.23941,1.24157),(.37444,.24513,.24108)),
    4: ((.13344,.15433,.23821),(.03336,.03858,.05955),(1.13701,.91966,.85303),(.23313,.14350,.15256)),
    5: ((.09912,.10300,.14530),(.02478,.02575,.03632),(1.12231,1.16728,1.02331),(.32458,.21768,.20285)),
    6: ((.10913,.11543,.18527),(.02728,.02886,.04632),(1.06997,1.02860,1.03946),(.28766,.18862,.19566)),
    7: ((.09912,.10300,.17557),(.02478,.02575,.04389),(.98237,1.04206,.95882),(.22533,.16196,.16840)),
    8: ((.05666,.04283,.05968),(.01417,.01071,.01492),(.94133,.98791,.97438),(.24920,.15879,.15879)),
}

def _jpeg_orientation(path):
    """Read metadata only; the supplied JPEG is never rewritten."""
    with open(path, 'rb') as stream:
        data = stream.read()
    marker = data.find(b'Exif\x00\x00')
    if marker < 0: return 1
    start = marker + 6
    order = '<' if data[start:start+2] == b'II' else '>'
    try:
        offset = start + struct.unpack_from(order+'I', data, start+4)[0]
        count = struct.unpack_from(order+'H', data, offset)[0]
        for i in range(count):
            entry = offset + 2 + i*12
            tag, kind, size = struct.unpack_from(order+'HHI', data, entry)
            if tag == 274 and kind == 3 and size == 1:
                return struct.unpack_from(order+'H', data, entry+8)[0]
    except (struct.error, ValueError): pass
    return 1

def _raw_uv(u, v, orientation):
    """Upright top-left photograph coordinates -> Blender's source-image UV."""
    if orientation == 2: u, v = 1-u, v
    elif orientation == 3: u, v = 1-u, 1-v
    elif orientation == 4: u, v = u, 1-v
    elif orientation == 5: u, v = v, u
    elif orientation == 6: u, v = v, 1-u
    elif orientation == 7: u, v = 1-v, 1-u
    elif orientation == 8: u, v = 1-v, u
    return np.stack((u, 1-v), axis=-1)

def _smooth(low, high, value):
    value = np.clip((value-low)/(high-low), 0, 1)
    return value*value*(3-2*value)

def _group_vertices(human, name):
    group = human.vertex_groups.get(name)
    return [v for v in human.data.vertices if group and any(g.group == group.index and g.weight > .5 for g in v.groups)]

def _anchors(human, rig):
    coords = np.array([v.co[:] for v in human.data.vertices])
    lips = np.array([v.co[:] for v in _group_vertices(human, 'lips')])
    if not len(lips): raise RuntimeError('Face projection needs the existing lips vertex group')
    mouth = lips.mean(axis=0)
    eye_objects = [o for o in bpy.data.objects if o.type == 'MESH' and o.parent == rig and 'low-poly' in o.name and not o.name.endswith('_Arms')]
    if eye_objects:
        eye_coords = np.array([v.co[:] for v in eye_objects[0].data.vertices])
        eye_z = eye_coords[:,2].mean()
        eye_half = np.abs(eye_coords[:,0]).mean()
    else:
        eye_z = rig.data.bones['head'].tail_local.z - .115
        eye_half = .030
    brow_objects = [o for o in bpy.data.objects if o.type == 'MESH' and o.parent == rig and 'eyebrow' in o.name and not o.name.endswith('_Arms')]
    brow_z = np.mean([v.co.z for v in brow_objects[0].data.vertices]) if brow_objects else eye_z+.017
    center = coords[(np.abs(coords[:,0]) < .013) & (coords[:,2] > mouth[2]+.015) & (coords[:,2] < eye_z+.008)]
    nose = center[np.argmin(center[:,1])]
    chin_candidates = coords[(np.abs(coords[:,0]) < .021) & (coords[:,2] < mouth[2]-.020) & (coords[:,2] > mouth[2]-.09) & (coords[:,1] < mouth[1]+.022)]
    chin_z = np.min(chin_candidates[:,2]) if len(chin_candidates) else mouth[2]-.048
    # Forehead occupies about half the eye-to-chin distance on this mesh.
    hair_z = eye_z + (eye_z-chin_z)*.57
    return dict(eye_z=eye_z, eye_half=eye_half, brow_z=brow_z, mouth_z=mouth[2], nose_z=nose[2], chin_z=chin_z, hair_z=hair_z, mouth_y=mouth[1])

def _sample(image, uvs):
    w, h = image.size
    pixels = np.empty(w*h*4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(h,w,4)
    uvs = np.asarray(uvs)
    x = np.clip((uvs[:,0]*(w-1)).astype(int),0,w-1)
    y = np.clip((uvs[:,1]*(h-1)).astype(int),0,h-1)
    # Image.pixels is encoded sRGB for these byte photographs/atlases; shader
    # texture nodes decode to linear. Gains must be calculated in that same space.
    encoded = np.median(pixels[y,x,:3], axis=0)
    return np.where(encoded <= .04045, encoded/12.92, ((encoded+.055)/1.055)**2.4)

def apply_reference_face(index1based, human, rig, destination):
    """Bake photo details onto the existing skin atlas; keep the original 3D head."""
    if not 1 <= index1based <= len(LANDMARKS): raise ValueError('Character must be 1..8')
    landmark = LANDMARKS[index1based-1]
    key = f'C{index1based:02d}'
    if not human.data.uv_layers: raise RuntimeError('Body needs its MakeHuman skin UV map')
    anchors = _anchors(human, rig)
    brow_v=EYEBROW_V[index1based-1]
    # Generic eyebrow-card centroids sit only ~8 mm above the eye, which formerly
    # squeezed the photographed forehead into a narrow band. Calibrate both upper
    # landmarks with the upright photo aspect ratio and fitted interocular span.
    meters_per_photo_v = anchors['eye_half']/landmark['eyehalf'] * landmark.get('aspect',4.0/3.0)
    anchors['brow_z'] = anchors['eye_z'] + float(np.clip(
        (landmark['eye'][1]-brow_v)*meters_per_photo_v, .014, .025))
    anchors['hair_z'] = anchors['eye_z'] + float(np.clip(
        (landmark['eye'][1]-landmark['hair'][1])*meters_per_photo_v, .028, .065))
    orientation = _jpeg_orientation(landmark['file'])
    photo = bpy.data.images.load(landmark['file'], check_existing=True)
    photo.colorspace_settings.name = 'sRGB'
    # Blender's image loader already transposes JPEGs on some builds.
    if orientation in (5,6,7,8) and photo.size[1] > photo.size[0]: orientation=1
    print('FACE_PHOTO',photo.size[:],orientation)
    original_uv = human.data.uv_layers.active
    original_uv_index = human.data.uv_layers.active_index
    original_uv_name = original_uv.name
    original_uv_values = np.array([entry.uv[:] for entry in original_uv.data])
    coords = np.array([v.co[:] for v in human.data.vertices])
    normals = np.array([v.normal[:] for v in human.data.vertices])
    x, y, z = coords.T
    model_z = [anchors['chin_z'], anchors['mouth_z'], anchors['nose_z'], anchors['eye_z'], anchors['brow_z'], anchors['hair_z']]
    photo_u = [landmark[n][0] for n in ('chin','mouth','nose','eye','eye','hair')]
    photo_v = [landmark['chin'][1],landmark['mouth'][1],landmark['nose'][1],landmark['eye'][1],brow_v,landmark['hair'][1]]
    u = np.interp(z, model_z, photo_u) + x*landmark['eyehalf']/anchors['eye_half']
    v = np.interp(z, model_z, photo_v)
    projected = _raw_uv(u, v, orientation)
    half_width = (landmark['right']-landmark['left'])/2 * anchors['eye_half']/landmark['eyehalf']
    # Taper before the photographed facial silhouette; sideburns/background must
    # not smear onto the wider 3D cheek or jaw during frontal projection.
    weights = .99 * (1-_smooth(.68,.94,np.abs(x)/half_width))
    weights *= _smooth(anchors['chin_z']-.008, anchors['chin_z']+.012,z)
    weights *= 1-_smooth(anchors['hair_z']-.004,anchors['hair_z']+.007,z)
    # Keep the photographed fringe out of the skin and preserve the real brow shape.
    forehead_band = max(.007, brow_v-landmark['hair'][1])
    weights *= _smooth(landmark['hair'][1]+forehead_band*.08,
                       landmark['hair'][1]+forehead_band*.55,v)
    weights *= _smooth(-.25,.25,-normals[:,1])
    # Forehead/brow recesses must retain the photo; exclude only the rear of the
    # head. The forward normal and image-space masks already protect the sides.
    weights *= 1-_smooth(anchors['mouth_y']+.070,anchors['mouth_y']+.120,y)
    if index1based==1:
        # The front photo includes ears beside the jaw. Keep those pixels off
        # the side of the 3D neck; the modeled ears already have their own UVs.
        lower_face=1-_smooth(anchors['mouth_z']-.005,anchors['mouth_z']+.030,z)
        weights*=1-lower_face*_smooth(.40,.70,np.abs(x)/half_width)
    if index1based==2:
        # The sunglasses are separate geometry; do not bake a second pair onto skin.
        weights *= _smooth(.1025,.1200,v)

    uv_photo = human.data.uv_layers.new(name='ReferenceFaceProjection')
    uv_photo_name = uv_photo.name
    corners = np.array([loop.vertex_index for loop in human.data.loops])
    uv_photo.data.foreach_set('uv',projected[corners].astype(np.float32).ravel())
    mask = human.data.color_attributes.new(name='ReferenceFaceBlend',type='FLOAT_COLOR',domain='CORNER')
    mask_name = mask.name
    corner_colors = np.repeat(weights[corners,None],4,axis=1).astype(np.float32)
    corner_colors[:,3] = 1
    mask.data.foreach_set('color',corner_colors.ravel())
    haze = human.data.color_attributes.new(name='ReferenceLightVeil',type='FLOAT_COLOR',domain='CORNER')
    haze_name = haze.name
    light_fit = PHOTO_LIGHT.get(index1based)
    haze_colors = np.zeros((len(corners),4),dtype=np.float32)
    haze_colors[:,3] = 1
    if light_fit:
        upper, lower, _, _ = (np.asarray(value) for value in light_fit)
        upper_weight = 1-_smooth(landmark['eye'][1],landmark['nose'][1],v)
        veil = lower[None,:] + upper_weight[:,None]*(upper-lower)[None,:]
        haze_colors[:,:3] = veil[corners]
    haze.data.foreach_set('color',haze_colors.ravel())
    human.data.uv_layers.active_index = original_uv_index
    human.data.uv_layers[original_uv_name].active_render = True

    # Sample matching cheek areas in both source images to reduce room-light color casts.
    material = human.data.materials[0].copy()
    material.name = key+'_Skin'
    human.data.materials[0] = material
    nodes, links = material.node_tree.nodes, material.node_tree.links
    base = nodes.get('DiffuseTexture')
    if not base or not base.image: raise RuntimeError('Body skin diffuse texture node is missing')
    if '_SkinReference' in base.image.name:
        original_alpha = nodes.get('AlphaMapTexture')
        if original_alpha and original_alpha.image and '_SkinReference' not in original_alpha.image.name:
            # Re-running a review must not accumulate previously projected facial details.
            base.image = original_alpha.image
    cheek_corners = np.where((np.abs(x[corners])>.031)&(np.abs(x[corners])<.049)&(z[corners]>anchors['mouth_z']+.012)&(z[corners]<anchors['eye_z']-.015)&(y[corners]<anchors['mouth_y']+.024))[0]
    if len(cheek_corners):
        original_values = original_uv_values[cheek_corners]
        skin_color = _sample(base.image, original_values)
        ref_color = _sample(photo, projected[corners[cheek_corners]])
        # Preserve photographed complexion instead of forcing every face to the
        # pale stock atlas. Only a small exposure lift compensates for room light.
        gain = np.full(3, 1.10)
        body_gain = np.clip(ref_color*gain/np.maximum(skin_color,.035), .35, 1.75)
        if light_fit:
            gain = np.asarray(light_fit[2])
            body_gain = np.clip(np.asarray(light_fit[3])/np.maximum(skin_color,.02), .22, 1.80)
    else:
        gain = np.ones(3)
        body_gain = np.ones(3)

    output = next(n for n in nodes if n.type=='OUTPUT_MATERIAL' and n.is_active_output)
    old_surface = output.inputs['Surface'].links[0].from_socket
    temporary = []
    def add(kind):
        n=nodes.new(kind);temporary.append(n);return n
    uv_node=add('ShaderNodeUVMap');uv_node.uv_map=uv_photo_name
    image_node=add('ShaderNodeTexImage');image_node.image=photo;image_node.extension='EXTEND'
    links.new(uv_node.outputs['UV'],image_node.inputs['Vector'])
    veil_node=add('ShaderNodeVertexColor');veil_node.layer_name=haze_name
    dehaze=add('ShaderNodeMixRGB');dehaze.blend_type='SUBTRACT';dehaze.inputs[0].default_value=1;dehaze.use_clamp=True
    links.new(image_node.outputs['Color'],dehaze.inputs[1]);links.new(veil_node.outputs['Color'],dehaze.inputs[2])
    correction=add('ShaderNodeMixRGB');correction.blend_type='MULTIPLY';correction.inputs[0].default_value=1
    correction.inputs[2].default_value=(*gain,1);links.new(dehaze.outputs[0],correction.inputs[1])
    corrected_color = correction.outputs[0]
    if light_fit:
        # Fluorescent flare is spatially uneven. Retain the actual facial
        # luminance detail while suppressing residual blue/green color blotches.
        target_color = np.asarray(light_fit[3])
        target_luma = max(float(target_color.dot((.2126,.7152,.0722))), .01)
        luma = add('ShaderNodeRGBToBW');links.new(corrected_color,luma.inputs['Color'])
        neutral = add('ShaderNodeMixRGB');neutral.blend_type='MULTIPLY';neutral.inputs[0].default_value=1
        neutral.inputs[2].default_value=(*(target_color/target_luma),1)
        links.new(luma.outputs[0],neutral.inputs[1])
        chroma = add('ShaderNodeMixRGB');chroma.blend_type='MIX';chroma.inputs[0].default_value=.72
        links.new(corrected_color,chroma.inputs[1]);links.new(neutral.outputs[0],chroma.inputs[2])
        corrected_color = chroma.outputs[0]
    blend=add('ShaderNodeMixRGB');blend.blend_type='MIX'
    body_tint=add('ShaderNodeMixRGB');body_tint.blend_type='MULTIPLY';body_tint.inputs[0].default_value=1
    body_tint.inputs[2].default_value=(*body_gain,1);links.new(base.outputs['Color'],body_tint.inputs[1])
    mask_node=add('ShaderNodeVertexColor');mask_node.layer_name=mask_name
    links.new(mask_node.outputs['Color'],blend.inputs[0]);links.new(body_tint.outputs[0],blend.inputs[1]);links.new(corrected_color,blend.inputs[2])
    emission=add('ShaderNodeEmission');links.new(blend.outputs[0],emission.inputs['Color']);links.new(emission.outputs[0],output.inputs['Surface'])
    atlas=bpy.data.images.new(key+'_SkinReference',width=4096,height=4096,alpha=True)
    atlas.colorspace_settings.name='sRGB'
    target=add('ShaderNodeTexImage');target.image=atlas;nodes.active=target
    scene=bpy.context.scene
    old_engine=scene.render.engine;old_samples=scene.cycles.samples
    old_active=bpy.context.view_layer.objects.active;old_selected=list(bpy.context.selected_objects)
    modifier_states=[(m,m.show_render,m.show_viewport) for m in human.modifiers if m.type=='ARMATURE']
    try:
        for modifier,_,_ in modifier_states: modifier.show_render=False;modifier.show_viewport=False
        scene.render.engine='CYCLES';scene.cycles.samples=1
        bpy.ops.object.select_all(action='DESELECT');human.hide_set(False);human.select_set(True);bpy.context.view_layer.objects.active=human
        bpy.ops.object.bake(type='EMIT',margin=8,use_clear=True,use_selected_to_active=False)
        os.makedirs(destination,exist_ok=True)
        atlas.filepath_raw=os.path.join(destination,key+'_SkinReference.png');atlas.file_format='PNG';atlas.save()
    finally:
        links.new(old_surface,output.inputs['Surface'])
        for n in temporary:nodes.remove(n)
        for modifier,render,viewport in modifier_states:modifier.show_render=render;modifier.show_viewport=viewport
        scene.render.engine=old_engine;scene.cycles.samples=old_samples
        bpy.ops.object.select_all(action='DESELECT')
        for obj in old_selected:
            if obj.name in bpy.context.view_layer.objects:obj.select_set(True)
        bpy.context.view_layer.objects.active=old_active
        human.data.uv_layers.remove(human.data.uv_layers[uv_photo_name]);human.data.color_attributes.remove(human.data.color_attributes[mask_name])
        human.data.color_attributes.remove(human.data.color_attributes[haze_name])
        human.data.uv_layers.active_index=original_uv_index
        restored_uv=human.data.uv_layers[original_uv_name]
        restored_uv.active_render=True
        restored_uv.data.foreach_set('uv',original_uv_values.astype(np.float32).ravel())
        human.data.update();bpy.context.view_layer.update()
    base.image=atlas
    material.update_tag();human.data.update();bpy.context.view_layer.update()
    print('COBBLE_FACE_BAKED',key,'orientation',orientation,'anchors',anchors,'gain',gain.tolist(),'path',atlas.filepath_raw)
    return atlas

def _diagnose(human, rig):
    print('FACE_DIAG_HEAD', tuple(rig.data.bones['head'].head_local), tuple(rig.data.bones['head'].tail_local))
    for group_name in ('lips', 'head', 'joint-l-eye', 'joint-r-eye', 'scalp', 'ears'):
        group = human.vertex_groups.get(group_name)
        coords = np.array([v.co[:] for v in human.data.vertices if group and any(g.group == group.index and g.weight > .5 for g in v.groups)])
        if len(coords): print('FACE_DIAG_GROUP', group_name, len(coords), coords.min(axis=0).tolist(), coords.max(axis=0).tolist(), coords.mean(axis=0).tolist())
    for obj in bpy.data.objects:
        if obj.type == 'MESH':
            print('FACE_DIAG_OBJECT', obj.name, len(obj.data.vertices), tuple(obj.dimensions), obj.parent.name if obj.parent else None)
            if 'low-poly' in obj.name:
                c = np.array([v.co[:] for v in obj.data.vertices]); print('FACE_DIAG_EYES', c.min(axis=0).tolist(), c.max(axis=0).tolist(), c.mean(axis=0).tolist(), np.abs(c[:,0]).mean())
    for material in human.data.materials:
        if material and material.use_nodes:
            print('FACE_DIAG_UV',[(uv.name,uv.active_render) for uv in human.data.uv_layers])
            print('FACE_DIAG_LINKS',[(l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name) for l in material.node_tree.links])
            for node in material.node_tree.nodes:
                if node.type == 'TEX_IMAGE' and node.image: print('FACE_DIAG_TEXTURE', material.name, node.name, node.image.name, tuple(node.image.size), node.image.filepath)

if __name__ == '__main__':
    body = next(o for o in bpy.data.objects if o.type == 'MESH' and o.name.endswith('_Body'))
    rig = body.parent
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    if args and args[0]=='--test':
        from mathutils import Vector
        index=int(args[1]) if len(args)>1 else 1
        destination=WORK+'/cobble-face-review'
        apply_reference_face(index,body,rig,destination)
        # The reference already supplies eyebrows; separate generic cards double them.
        for obj in bpy.data.objects:
            if obj.type=='MESH' and obj.parent==rig and 'eyebrow' in obj.name:obj.hide_render=True
        scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=32
        scene.render.resolution_x=720;scene.render.resolution_y=900;scene.render.resolution_percentage=100
        camera=scene.camera
        center=Vector((0,-.07,rig.data.bones['head'].head_local.z+.05))
        camera.location=(.025,-2.5,center.z);camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=.40
        scene.render.filepath=destination+f'/C{index:02d}_FaceReview.png'
        bpy.ops.render.render(write_still=True)
    else:_diagnose(body,rig)
