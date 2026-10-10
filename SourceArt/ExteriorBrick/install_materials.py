"""Save exterior masonry and upright-grass materials. Never load or save the house map."""
import unreal
from pathlib import Path

D='/Game/SeniorSendOff/House/ExteriorBrick'
G='/Game/SeniorSendOff/House/ExteriorNature'
C='/Game/SeniorSendOff/House/ExteriorCars'
A=unreal.EditorAssetLibrary
E=unreal.MaterialEditingLibrary
T=unreal.AssetToolsHelpers.get_asset_tools()
P=Path(__file__).resolve().parent

def node(m,kind,**props):
    n=E.create_material_expression(m,getattr(unreal,'MaterialExpression'+kind))
    for k,v in props.items():n.set_editor_property(k,v)
    return n

def link(a,b,slot,output=''):
    names=list(E.get_material_expression_input_names(b))
    key=lambda s:''.join(c for c in s.lower() if c.isalnum())
    slot=next((v for v in names if key(v)==key(slot)),slot)
    if slot=='Coordinates':slot=next((v for v in names if 'uv' in v.lower()),slot)
    assert E.connect_material_expressions(a,output,b,slot),(b,slot,output,names)

def out(n,prop,output=''):
    assert E.connect_material_property(n,output,getattr(unreal.MaterialProperty,'MP_'+prop))

def scalar(m,name,v):return node(m,'ScalarParameter',parameter_name=name,default_value=v)
def vector(m,name,v):return node(m,'VectorParameter',parameter_name=name,default_value=unreal.LinearColor(*v,1))
def op(m,kind,a,b):
    n=node(m,kind);link(a,n,'A');link(b,n,'B');return n
def custom(m,code,inputs,typ='CMOT_FLOAT3'):
    n=node(m,'Custom',code=code,output_type=getattr(unreal.CustomMaterialOutputType,typ))
    data=[]
    for name,_ in inputs:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);data.append(i)
    n.set_editor_property('inputs',data)
    for name,source in inputs:link(source,n,name)
    return n
def material(folder,name):
    p=folder+'/'+name
    m=A.load_asset(p) if A.does_asset_exist(p) else T.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    assert m,p
    E.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True)
    return m
def save(m):
    E.recompile_material(m)
    assert A.save_loaded_asset(m,False),m.get_name()
    unreal.log('EXTERIOR_DETAIL_SAVED '+m.get_path_name())
def texture(m,name,p,kind):
    return node(m,'TextureSampleParameter2D',parameter_name=name,texture=A.load_asset(p),
       sampler_type=getattr(unreal.MaterialSamplerType,'SAMPLERTYPE_'+kind))

tasks=[]
for label in ['Brick','BrickFace']:
    for kind in ['Base','Normal','Surface']:
        task=unreal.AssetImportTask()
        task.set_editor_property('filename',str(P/'textures'/f'{label}_{kind}.png'))
        task.set_editor_property('destination_path',D)
        task.set_editor_property('destination_name',f'T_{label}_{kind}')
        task.set_editor_property('automated',True)
        task.set_editor_property('replace_existing',True)
        task.set_editor_property('save',False)
        tasks.append((task,kind))
T.import_asset_tasks([t for t,k in tasks])
for task,kind in tasks:
    paths=task.get_editor_property('imported_object_paths');assert len(paths)==1,paths
    tex=A.load_asset(paths[0])
    tex.set_editor_property('srgb',kind=='Base')
    tex.set_editor_property('compression_settings',getattr(unreal.TextureCompressionSettings,
        'TC_NORMALMAP' if kind=='Normal' else 'TC_MASKS' if kind=='Surface' else 'TC_DEFAULT'))
    tex.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property('address_y',unreal.TextureAddress.TA_WRAP)
    tex.set_editor_property('max_texture_size',1024)
    if kind=='Normal':tex.set_editor_property('flip_green_channel',False)
    assert A.save_loaded_asset(tex,False)
    unreal.log('EXTERIOR_DETAIL_SAVED '+tex.get_path_name())

for face in [False,True]:
    label='BrickFace' if face else 'Brick'
    m=material(D,'M_'+label)
    uv=node(m,'TextureCoordinate')
    sampleuv=uv
    if not face:
        height=texture(m,'HeightMap',D+'/T_Brick_Surface','MASKS');link(uv,height,'Coordinates')
        bump=node(m,'BumpOffset',height_ratio=.004,reference_plane=.84)
        link(uv,bump,'Coordinate');link(height,bump,'Height','B')
        sampleuv=bump
    base=texture(m,'BaseMap',D+'/T_'+label+'_Base','COLOR');link(sampleuv,base,'Coordinates')
    n=texture(m,'NormalMap',D+'/T_'+label+'_Normal','NORMAL');link(sampleuv,n,'Coordinates')
    surface=texture(m,'SurfaceMap',D+'/T_'+label+'_Surface','MASKS');link(sampleuv,surface,'Coordinates')
    tint=vector(m,'Tint',(1,1,1))
    world=node(m,'WorldPosition')
    col=custom(m,'''float variation=1 + .025*sin(P.x*.019+P.z*.017) + .020*sin(P.y*.033-P.z*.031);
float grade=-60-302*saturate(P.y/1600); float wet=(1-saturate((P.z-grade)/48))*.10;
return Base*Tint*variation*(1-wet);''',[('Base',base),('Tint',tint),('P',world)])
    out(col,'BASE_COLOR');out(n,'NORMAL');out(surface,'ROUGHNESS','R');out(surface,'AMBIENT_OCCLUSION','G')
    out(scalar(m,'Specular',.22),'SPECULAR')
    save(m)

m=material(G,'M_NatureGrassBlades')
m.set_editor_property('two_sided',True)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
uv=node(m,'TextureCoordinate');pos=node(m,'WorldPosition');clock=node(m,'Time')
random=node(m,'PerInstanceRandom');tint=vector(m,'Tint',(.12,.19,.035))
col=custom(m,'''float tip=saturate(1-UV.y);
float ridge=.77+.23*(1-abs(UV.x*2-1));
float vary=.83+Random*.30;
return Tint*ridge*vary*lerp(.65,1.15,tip);''',[('UV',uv),('Random',random),('Tint',tint)])
out(col,'BASE_COLOR');out(op(m,'Multiply',col,scalar(m,'Transmission',.65)),'SUBSURFACE_COLOR')
out(scalar(m,'Roughness',.84),'ROUGHNESS');out(scalar(m,'Specular',.20),'SPECULAR')
# Preserve the gently upward volume normal on the reverse side of a blade.
out(op(m,'Multiply',node(m,'Constant3Vector',constant=unreal.LinearColor(0,0,1,1)),node(m,'TwoSidedSign')),'NORMAL')
wind=custom(m,'''float tip=saturate(1-UV.y); tip*=tip;
float phase=Clock*1.05+P.x*.015+P.y*.010+Random*6.283;
float wave=sin(phase)*.65+sin(Clock*1.85+P.y*.023)*.25;
return float3(wave, sin(phase*.84+1.1)*.40, -abs(wave)*.10)*Strength*tip;''',
    [('P',pos),('Clock',clock),('UV',uv),('Random',random),('Strength',scalar(m,'WindStrength',3.0))])
out(wind,'WORLD_POSITION_OFFSET');save(m)

for paint in [False,True]:
    m=material(C,'M_CarPaint' if paint else 'M_CarSurface')
    m.set_editor_property('use_material_attributes',True)
    attrs=node(m,'MakeMaterialAttributes')
    link(vector(m,'Tint',(.4,.4,.4)),attrs,'BaseColor')
    link(scalar(m,'Roughness',.26 if paint else .65),attrs,'Roughness')
    link(scalar(m,'Metallic',.05 if paint else 0),attrs,'Metallic')
    link(scalar(m,'Specular',.5),attrs,'Specular')
    link(vector(m,'Emission',(0,0,0)),attrs,'EmissiveColor')
    if paint:
        m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_CLEAR_COAT)
        link(scalar(m,'ClearCoat',1),attrs,'ClearCoat')
        link(scalar(m,'ClearCoatRoughness',.18),attrs,'ClearCoatRoughness')
    out(attrs,'MATERIAL_ATTRIBUTES')
    save(m)
(P/'evidence/materials.txt').write_text('PASS: six brick textures, two masonry masters, grass-blade master, and two car masters saved. No map edits.\n')
