"""Owned bedding/weathering trial; source rock and other masters read-only.

No geometry or water-level edits. The wet shoreline band remains separate work.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
folder='/Game/SeniorSendOff/Calder/Materials'
path=folder+'/M_CalderStrataRock';report=ROOT/'SourceArt/CalderLandscape/phase2/rock_strata_material.json'
files=['Content/SeniorSendOff/Calder/Materials/'+name+'.uasset' for name in
       ['M_CalderGround','M_CalderRock','M_CalderWater','M_CalderFalls','M_CalderStream','M_CalderFarmRoof']]
guards={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in files}
if A.does_asset_exist(path):
    ownership=ROOT/'SourceArt/CalderLandscape/phase2/rock_strata_owned_creation.json'
    proven=report.exists() and json.loads(report.read_text())['asset']==path
    if not proven and ownership.exists():
        proof=json.loads(ownership.read_text())
        proven=proof['asset']==path and proof['failed_owned_asset_sha256']==hashlib.sha256((ROOT/'Content/SeniorSendOff/Calder/Materials/M_CalderStrataRock.uasset').read_bytes()).hexdigest()
    assert proven,'Unproven existing asset'
    assert A.delete_asset(path)
mat=A.duplicate_asset(folder+'/M_CalderRock',path);assert mat
spec=importlib.util.spec_from_file_location('calder_strata_helpers',ROOT/'Tools/calder/phase2_assets.py')
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h)
base=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
normal=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_NORMAL)
rough=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_ROUGHNESS)
assert base and normal and rough
position=h.node(mat,'WorldPosition')
bed=h.node(mat,'ScalarParameter',parameter_name='CalderStrataStrength',default_value=1.)
streak=h.node(mat,'ScalarParameter',parameter_name='CalderStreakStrength',default_value=.20)
grey=h.node(mat,'ScalarParameter',parameter_name='CalderRockGray',default_value=.30)
field=h.custom(mat,r'''
struct CalderBedNoise {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float sample(float2 p) {
  float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),
              lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
};
CalderBedNoise n;
float macro=n.sample(P.xy/870.0+P.z/1900.0);
float phase=(P.z-.14*P.x+.07*P.y)/82.0+n.sample(P.xy/520.0)*.16;
float edge=min(frac(phase),1-frac(phase));
float bedLine=(1-smoothstep(.025,.09+fwidth(phase),edge))*saturate(Bed);
float across=P.x*.84+P.y*.54;
float seep=smoothstep(.53,.78,n.sample(float2(across/75.0,P.z/1900.0)))*saturate(Streak);
return float3(bedLine,seep,macro);
''',{'P':position,'Bed':bed,'Streak':streak},'CMOT_FLOAT3')
colour=h.custom(mat,r'''
float l=dot(Source,float3(.2126,.7152,.0722));
float3 c=lerp(Source,l*float3(.96,.94,.88),saturate(Gray*1.5));
c*=.96+.09*Field.b-.34*Field.r;
return max(0,lerp(c,c*float3(.65,.68,.62),Field.g*.30));
''',{'Source':base,'Field':field,'Gray':grey},'CMOT_FLOAT3')
bump=h.custom(mat,r'''
float3 n=normalize(SourceN),dx=ddx(P),dy=ddy(P);
float3 rx=cross(dy,n),ry=cross(n,dx);float det=dot(dx,rx);
float height=-Field.r*1.15+Field.b*.12;
return normalize(max(abs(det),1e-5)*n-sign(det)*(ddx(height)*rx+ddy(height)*ry));
''',{'SourceN':normal,'P':position,'Field':field},'CMOT_FLOAT3')
roughness=h.custom(mat,'return clamp(SourceR+.035*Field.r-.055*Field.g,.12,1);',
                   {'SourceR':rough,'Field':field},'CMOT_FLOAT1')
h.output(colour,'BASE_COLOR');h.output(bump,'NORMAL');h.output(roughness,'ROUGHNESS')
mat.set_editor_properties({'blend_mode':unreal.BlendMode.BLEND_OPAQUE,
                          'used_with_instanced_static_meshes':True})
E.recompile_material(mat);stats=E.get_statistics(mat)
assert stats.num_pixel_shader_instructions>0 and 0<stats.num_pixel_texture_samples<=12
for name,sha in guards.items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha,name
A.save_loaded_asset(mat)
data={'asset':path,'pixel_instructions':stats.num_pixel_shader_instructions,
      'pixel_samples':stats.num_pixel_texture_samples,'original_material_sha256':guards,
      'opaque':True,'ISM_enabled':True,'default_enabled':False,'switch':'CalderRockStrataTrial',
      'bedding':'Tilted82cm beds, shallow recessed shale seams; zero strength on masonry',
      'preserved':'Original scan inputs/projection/moss/AO, positions/collision/water authority',
      'scope':'Compile-only candidate; native/visual/performance and shoreline wet bands still open'}
report.unlink(missing_ok=True);report.write_text(json.dumps(data,indent=2)+'\n')
unreal.log(f'CALDER_ROCK_STRATA_MATERIAL PASS instructions={stats.num_pixel_shader_instructions} samples={stats.num_pixel_texture_samples} originalsUnchanged=6')
