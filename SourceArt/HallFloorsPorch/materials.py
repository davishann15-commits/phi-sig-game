import unreal
D='/Game/SeniorSendOff/House/HallFloorsPorch'
ml=unreal.MaterialEditingLibrary;ed=unreal.EditorAssetLibrary;at=unreal.AssetToolsHelpers.get_asset_tools()
COMMON='''
struct PaintFunctions {
 float h(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float n(float2 p) { float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(h(i),h(i+float2(1,0)),f.x),lerp(h(i+float2(0,1)),h(i+1),f.x),f.y); }
}; PaintFunctions f;
'''
def make(name,code=None,color=(.1,.1,.1),rough=.6,metal=0,opacity=None,normal=None):
 m=unreal.load_asset(D+'/'+name)or at.create_asset(name,D,unreal.Material,unreal.MaterialFactoryNew());ml.delete_all_material_expressions(m)
 def scalar(v,prop):
  x=ml.create_material_expression(m,unreal.MaterialExpressionConstant);x.r=v;ml.connect_material_property(x,'',prop)
 if code:
  c=ml.create_material_expression(m,unreal.MaterialExpressionCustom);c.set_editor_property('code',COMMON+code);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
  inp=unreal.CustomInput();inp.set_editor_property('input_name','P');c.set_editor_property('inputs',[inp])
  p=ml.create_material_expression(m,unreal.MaterialExpressionWorldPosition);ml.connect_material_expressions(p,'',c,'P')
  rgb=ml.create_material_expression(m,unreal.MaterialExpressionComponentMask);[rgb.set_editor_property(k,v)for k,v in [('r',True),('g',True),('b',True),('a',False)]];ml.connect_material_expressions(c,'',rgb,ml.get_material_expression_input_names(rgb)[0]);ml.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
  alpha=ml.create_material_expression(m,unreal.MaterialExpressionComponentMask);[alpha.set_editor_property(k,v)for k,v in [('r',False),('g',False),('b',False),('a',True)]];ml.connect_material_expressions(c,'',alpha,ml.get_material_expression_input_names(alpha)[0]);ml.connect_material_property(alpha,'',unreal.MaterialProperty.MP_ROUGHNESS)
 else:
  c=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);c.constant=unreal.LinearColor(*color,1);ml.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR);scalar(rough,unreal.MaterialProperty.MP_ROUGHNESS)
 scalar(metal,unreal.MaterialProperty.MP_METALLIC)
 if opacity is not None:
  m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True);scalar(opacity,unreal.MaterialProperty.MP_OPACITY)
 if name=='HFP_NavyLinen':m.set_editor_property('two_sided',True)
 if normal:
  c=ml.create_material_expression(m,unreal.MaterialExpressionCustom);c.set_editor_property('code',COMMON+normal);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
  inp=unreal.CustomInput();inp.set_editor_property('input_name','P');c.set_editor_property('inputs',[inp]);p=ml.create_material_expression(m,unreal.MaterialExpressionWorldPosition);ml.connect_material_expressions(p,'',c,'P');ml.connect_material_property(c,'',unreal.MaterialProperty.MP_NORMAL)
 ml.recompile_material(m);assert ed.save_loaded_asset(m,False);return m
FLOOR='''
float2 q=P.xy;
float low=f.n(q*.006), med=f.n(q*.09), grit=f.n(q*1.9);
float worn=smoothstep(.37,.68,f.n(q*.014+27));
float stains=smoothstep(.67,.84,f.n(q*.027+45));
float2 cell=floor(q/21), local=frac(q/21);
float seed=f.h(cell);float2 center=float2(f.h(cell+7),f.h(cell+23))*.6+.2;
float chip=(1-smoothstep(.027,.056,length((local-center)*float2(1,1.8))))*step(.91,seed);
float scratch=(1-smoothstep(.01,.024,abs(sin((q.x+f.n(q*.17)*2)*1.7))))*smoothstep(.77,.9,f.n(q*.06+12));
float speck=smoothstep(.94,.998,grit)*.11;
float3 base=BASE;
float3 col=base*(.93+low*.11+med*.06+grit*.035);
col=lerp(col,col*.72,stains*.25);
col=lerp(col,col*1.09,worn*.3);
col=lerp(col,float3(.27,.26,.205),saturate(chip*.9+scratch*.45));
col+=speck*.055;
float r=lerp(.76,.48,worn*.72)*(.91+med*.09);r=lerp(r,.86,chip);
return float4(col,clamp(r,.2,.9));
'''
WOOD='''
float2 p=P.xy;float width=22;float lane=floor(p.y/width);
float offset=f.h(float2(lane,18))*160;float board=floor((p.x+offset)/180);
float seed=f.h(float2(lane,board));
float2 uv=float2((p.x+offset)/180,frac(p.y/width));
float grain=f.n(float2(p.x*.014,p.y*.62+f.n(p.xy*.03)*2));
float fine=sin(p.y*8+f.n(float2(p.x*.015,p.y*.21))*10)*.03;
float edge=min(min(frac((p.x+offset)/180),1-frac((p.x+offset)/180))*180,min(frac(p.y/width),1-frac(p.y/width))*width);
float seam=1-smoothstep(.07,.28,edge);
float2 knotp=float2(frac((p.x+offset)/180),frac(p.y/width))-float2(.3+seed*.4,.2+seed*.6);
float knot=(1-smoothstep(.04,.17,length(knotp*float2(1.4,.8))))*step(.62,seed);
float3 col=float3(.30,.132,.041)*(.7+seed*.4+grain*.26+fine);
col=lerp(col,float3(.024,.013,.006),seam*.9+knot*.6);
return float4(col,.43+f.n(p*.11)*.2);
'''
LEATHER='''
float fine=f.n(P.xz*3.6);float pores=f.n(P.xz*9);float wear=f.n(P.xz*.025);
float crease=pow(abs(sin(P.z*.19+f.n(P.xy*.026)*5)),18)*f.n(P.xz*.15);
float3 col=float3(.108,.048,.021)*(.7+wear*.55+fine*.18);col*=1-crease*.13;
return float4(col,.45+fine*.12+pores*.06);
'''
TIMBER='''
float grain=f.n(float2(P.x*.018,P.y*.7+P.z*.24));float fine=sin(P.y*5+P.z*3+f.n(P.xz*.01)*6)*.035;
float old=f.n(P.xz*.021);float3 col=float3(.34,.117,.027)*(.7+grain*.4+old*.22+fine);
return float4(col,.58+grain*.17);
'''
MATS={}
def build():
 for n,base in [('Hall',(.042,.041,.032)),('Bath',(.041,.040,.032)),('Landing',(.039,.041,.036)),('Porch',(.11,.099,.078)),('Stair',(.042,.044,.038))]:
  name='HFP_'+({'Hall':'HallCoating','Bath':'BathCoating','Landing':'LandingCoating','Porch':'PorchCoating','Stair':'StairPaint'}[n]);MATS[name]=make(name,FLOOR.replace('BASE','float3'+str(base)))
 MATS['HFP_GalleryOak']=make('HFP_GalleryOak',WOOD)
 MATS['HFP_PorchTimber']=make('HFP_PorchTimber',TIMBER)
 MATS['HFP_Leather']=make('HFP_Leather',LEATHER,normal='float a=f.n(P.xz*4),b=f.n(P.xz*4+.07);return normalize(float3((a-b)*.24,(f.n(P.xz*4+.13)-a)*.2,1));')
 MATS['HFP_NavyLinen']=make('HFP_NavyLinen','float weave=(sin(P.x*12)*sin(P.z*12))*.08;return float4(float3(.009,.013,.028)*(1+weave),.9);')
 for n,c,r,met in [('PaintedWhite',(.69,.68,.61),.36,0),('Brass',(.40,.25,.077),.28,.85),('Chrome',(.55,.57,.59),.23,.92),('BlackSteel',(.014,.016,.016),.4,.7),('Walnut',(.068,.027,.013),.48,0),('PorchTimberRaw',(.28,.18,.085),.84,0),('LeatherSeam',(.061,.025,.010),.67,0),('DarkCrack',(.008,.005,.003),.95,0),('Foam',(.44,.365,.19),.97,0),('DarkBronze',(.091,.077,.057),.38,.6),('Rubber',(.014,.014,.012),.96,0)]:MATS['HFP_'+n]=make('HFP_'+n,color=c,rough=r,metal=met)
 MATS['HFP_Glass']=make('HFP_Glass',color=(.075,.10,.11),rough=.08,opacity=.12)
 return MATS
