"""Split the authored library kit without changing its reference arrangement.
Run in Blender. Parts of one book stay together; each book/sheet/chip is a body.
"""
import bpy, json, math, runpy, collections, re, copy
from pathlib import Path
from mathutils import Vector

ROOT = Path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/BasementLibrary')
OUT = ROOT / 'loose-items'
(OUT / 'models').mkdir(parents=True, exist_ok=True)
source_manifest=OUT/'before/manifest.json'
DOC = json.loads((source_manifest if source_manifest.exists() else ROOT/'manifest.json').read_text())
DM = runpy.run_path('/Users/Stewart/Documents/Unreal Projects/SeniorSendoff/SourceArt/HouseInteractivity/source/generate_damage_v2.py')
DEST = '/Game/SeniorSendOff/House/BasementLibrary'
ASSETS = {a['id']: a for a in DOC['assets']}
result = {'assets': [], 'replacements': {}, 'chairs': [], 'chair_horizontal_ratio': 1.1 / 1.8}

def load(row, field='intact'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=row[field + '_fbx'])
    for ob in list(bpy.context.scene.objects):
        if ob.type == 'MESH':
            bpy.context.view_layer.objects.active = ob
            bpy.ops.object.select_all(action='DESELECT')
            ob.select_set(True)
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return next(o for o in bpy.context.scene.objects if o.type == 'MESH' and not o.name.startswith('UCX_'))

def export_piece(source, faces, ident, name, mass, health=12):
    part = DM['part_mesh'](source, faces)
    lo, hi = DM['bbox'](part)
    center = (lo + hi) * .5
    for v in part.data.vertices:
        v.co -= center
    part.name = 'SM_Lib_' + ident
    collision_parts=[DM['part_mesh'](part,c['faces']) for c in DM['S']['components'](part)] if ident.endswith('_support') else [part]
    hulls = DM['hulls_for'](collision_parts, part.name)
    assert len(hulls)>0, ident
    for collision_part in collision_parts:
        if collision_part!=part:DM['discard'](collision_part)
    if 'Paper' in ident:
        # A sheet has its own finite CCD collider, not a six-millimetre block
        # intersecting all the neighbouring sheets in the original stack.
        for hull in hulls:
            hlo, hhi = DM['bbox'](hull)
            mid = (hlo.z + hhi.z) * .5
            thickness = max(.001, hi.z - lo.z)
            for v in hull.data.vertices:
                v.co.z = mid + (v.co.z-mid) * thickness / (hhi.z-hlo.z)
    bpy.ops.object.select_all(action='DESELECT')
    part.select_set(True)
    for h in hulls: h.select_set(True)
    bpy.context.view_layer.objects.active = part
    file = OUT / 'models' / (part.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(file), use_selection=True, apply_unit_scale=True,
                             axis_forward='-Z', axis_up='Y', add_leaf_bones=False)
    names = [re.sub(r'\.\d+$', '', m.name) for m in part.data.materials]
    row = dict(id=ident, asset_name=part.name, intact_fbx=str(file),
               material_names=names, material_slots=[DEST+'/'+n+'.'+n for n in names],
               bounds_blender_metres=[list(lo-center), list(hi-center)],
               behavior='furniture', mass_kg=mass, break_health=health,
               movable=True, wall=False, damage=True, small_item=True, hulls=len(hulls),
               vertices=len(part.data.vertices), faces=len(part.data.polygons))
    result['assets'].append(row)
    for o in [part, *hulls]: DM['discard'](o)
    return dict(asset=ident, name=name, offset_blender_metres=list(center))

def split(ident, classify, properties, reuse=None):
    source = load(ASSETS[ident])
    groups = collections.defaultdict(list)
    for c in DM['S']['components'](source):
        # Classification uses entire connected pieces, preserving cover text/UVs.
        p = DM['part_mesh'](source, c['faces'])
        lo, hi = DM['bbox'](p)
        names = {re.sub(r'\.\d+$', '', source.data.materials[source.data.polygons[f].material_index].name) for f in c['faces']}
        key = classify((lo+hi)*.5, names)
        groups[key].extend(c['faces'])
        DM['discard'](p)
    pieces=[]; shared={}
    for key, faces in sorted(groups.items()):
        name, mass, health = properties(key)
        shared_key = reuse(key) if reuse else key
        if shared_key in shared:
            p = DM['part_mesh'](source, faces); lo,hi=DM['bbox'](p)
            pieces.append(dict(asset=shared[shared_key],name=name,offset_blender_metres=list((lo+hi)*.5)))
            DM['discard'](p)
            continue
        piece = export_piece(source, faces, 'Loose_'+ident+'_'+str(key), name, mass, health)
        shared[shared_key] = piece['asset']
        # A support (case/riser) is still a movable furnishing, not tiny clutter.
        if key == 'support': result['assets'][-1]['small_item'] = False
        pieces.append(piece)
    result['replacements'][ident] = pieces
    if ident=='OpenPokerChipCase':
        # Lift the loose chips clear of the solid bottom/felt surface so the
        # original decorative embed does not begin physics inside its case.
        for piece in pieces:
            if not piece['asset'].endswith('_support'):piece['offset_blender_metres'][2]+=.010
    print('LIBRARY_SPLIT', ident, len(pieces), flush=True)

for variant in range(5):
    centers=[]; x=-.31
    for k in range(7+variant):
        w=.027+(k%3)*.012
        centers.append((x+w/2)*1.8)
        x+=w+.004
        if x>.23: break
    split('BookShelfContents'+str(variant),
          lambda c,n: str(min(range(len(centers)),key=lambda k:abs(c.x-centers[k]))).zfill(2),
          lambda k: ('Individual shelf book', .35+(int(k)%3)*.15, 14))
split('StackedBooks', lambda c,n: str(min(range(6),key=lambda k:abs(c.z-(.020+k*.031)))).zfill(2),
      lambda k: ('Individual hardback book', .65, 16))
split('PaperScatter', lambda c,n: str(round((c.z-.002)/.001)).zfill(2),
      lambda k: ('Loose study paper', .1, 8))
split('LoosePaperStack', lambda c,n: str(min(range(8),key=lambda k:abs(c.z-(.001+k*.001)))).zfill(2),
      lambda k: ('Loose paper', .1, 8))
split('PartyGameStack', lambda c,n: 'white' if c.x>.25 else str(min(range(6),key=lambda k:abs(c.z-(.018+k*.036)))).zfill(2),
      lambda k: ('Individual party game box', .45, 16))
split('DeskRiser', lambda c,n: 'notebook' if c.z>.092 else 'support',
      lambda k: ('Red notebook', .4, 12) if k=='notebook' else ('Wood monitor riser', 2.6, 20))
split('FrameRails', lambda c,n: '01' if c.y>.045 else '00',
      lambda k: ('Loose frame rail', 1.5, 18))

chip_centers=[Vector(((k-3)*.066*1.8,(-.105+j*.017)*1.8,.073)) for k in range(7) for j in range(12)]
def chip_group(c,names):
    if any('Chip' in n or n=='M_Lib_Paper' for n in names):
        return str(min(range(84),key=lambda k:(Vector((c.x,c.y,0))-Vector((chip_centers[k].x,chip_centers[k].y,0))).length_squared)).zfill(2)
    return 'support'
split('OpenPokerChipCase', chip_group,
      lambda k: ('Open silver chip case', 5, 32) if k=='support' else ('Poker chip', .1, 8),
      reuse=lambda k: 'support' if k=='support' else str((int(k)//12)%4))
split('GrayBackpack', lambda c,n: 'jacket' if any('GrayClothes' in m or 'Linen' in m for m in n) else 'bag',
      lambda k: ('Loose gray jacket', 1.1, 16) if k=='jacket' else ('Black nylon bag', 3.9, 24))

# Preserve both chair silhouettes, material details and all three damage stages.
for ident in ['MeshOfficeChair','GrayOfficeChair']:
    row=copy.deepcopy(ASSETS[ident]); ratio=result['chair_horizontal_ratio']
    for field in ['intact','damaged','heavily_damaged','broken']:
        body=load(row,field)
        for ob in bpy.context.scene.objects:
            if ob.type=='MESH':
                for v in ob.data.vertices: v.co.x*=ratio; v.co.y*=ratio
        name=row['asset_name'] if field=='intact' else row[field+'_asset_name']
        body.name=name
        file=OUT/'models'/(name+'.fbx')
        bpy.ops.object.select_all(action='DESELECT')
        for ob in bpy.context.scene.objects:
            if ob.type=='MESH':ob.select_set(True)
        bpy.context.view_layer.objects.active=body
        bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,apply_unit_scale=True,
                                 axis_forward='-Z',axis_up='Y',add_leaf_bones=False)
        row[field+'_fbx']=str(file)
        if field=='intact':
            lo,hi=DM['bbox'](body);row['bounds_blender_metres']=[list(lo),list(hi)]
    result['chairs'].append(row)
    print('LIBRARY_CHAIR_NARROWED',ident,row['bounds_blender_metres'],flush=True)

(OUT/'split-manifest.json').write_text(json.dumps(result,indent=2))
print('LIBRARY_LOOSE_ITEMS_BUILT',len(result['assets']),flush=True)
