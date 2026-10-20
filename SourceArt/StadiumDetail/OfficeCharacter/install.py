from pathlib import Path
import json,math
ROOT=Path(__file__).resolve().parents[3];HERE=Path(__file__).parent

def rep(s,a,b):
 assert s.count(a)==1,a[:120]
 return s.replace(a,b,1)

def write(name,s):
 p=ROOT/'Source/SeniorSendoff'/name
 assert p.read_bytes()==(HERE/(name+'.before')).read_bytes(),name+' preimage changed'
 p.unlink();p.write_text(s)
 if name.startswith('HouseStadiumDetail'):
  d=ROOT/'SourceArt/StadiumDetail/Draft'/name;d.unlink();d.write_text(s)

helpers=(HERE/'mesh_helpers.inc').read_text()
helpers=helpers.replace('C.SizeSquared()','float(C.SizeSquared())')
helpers=helpers.replace('X,Up,2.35f,-1);','X,Up,2.35f,2);')
helpers=helpers.replace('S.UV[J]={.75f+(Old.X-.25f)*.30f,.75f+(Old.Y-.25f)*.30f};','S.UV[J]={.25f+(Old.X-.25f)*.3264f,.75f+(Old.Y-.75f)*.3264f};')
helpers=helpers.replace('const int32 First=Solid.Sec(Body).V.Num();HouseCampusFallKit::Lathe','const int32 First=Solid.Sec(Body).V.Num(),FirstIndex=Solid.Sec(Body).I.Num();HouseCampusFallKit::Lathe')
helpers=helpers.replace('S.N[J]=Tilt.RotateVector(S.N[J]);','S.N[J]=-Tilt.RotateVector(S.N[J]);')
helpers=helpers.replace('S.T[J].TangentX=Tilt.RotateVector(S.T[J].TangentX);}','S.T[J].TangentX=Tilt.RotateVector(S.T[J].TangentX);}for(int32 J=FirstIndex;J<S.I.Num();J+=3)Swap(S.I[J+1],S.I[J+2]);')
(HERE/'mesh_helpers.inc').write_text(helpers)
s=(HERE/'HouseStadiumDetailInteriors.cpp.before').read_text()
k='void Word(FHallMesh& M,FString Text'
pos=s.index(k);s=s[:pos]+helpers+'\n'+s[pos:]
a=s.index(' const FVector Phone=P(');b=s.index(' // Rubbed varnish',a)
s=s[:a]+''' const FVector Phone=P(60,-14,76.5f);I::RotaryPhone(Solid,Detail,Phone,PX,PY);
'''+s[b:]
a=s.index(' // Four-spoke upholstered swivel chair');b=s.index(' // Old steel filing cabinet',a)
s=s[:a]+(HERE/'chairs.inc').read_text()+s[b:]
s=rep(s,'float Degrees,float Curl=0)\n{','float Degrees,float Curl=0,int32 Grid=3)\n{')
s=rep(s,'Cell<0?1.f:1.f/3.f,U0=Cell<0?0.f:(Cell%3)*Scale,V0=Cell<0?0.f:(Cell/3)*Scale','Cell<0?1.f:1.f/Grid,U0=Cell<0?0.f:(Cell%Grid)*Scale,V0=Cell<0?0.f:(Cell/Grid)*Scale')
k=' // Brass doorplate is beside the opening';pos=s.index(k);s=s[:pos]+(HERE/'walls.inc').read_text()+s[pos:]
write('HouseStadiumDetailInteriors.cpp',s)
s=(HERE/'HouseStadiumDetailLook.cpp.before').read_text()
s=rep(s,'LeatherN,.81f,{.54f,.40f,.28f},.75f)','LeatherN,.66f,{.46f,.34f,.25f},.60f)')
k='  // Printed, folded homecoming program.';pos=s.index(k);s=s[:pos]+(HERE/'look_details.inc').read_text()+'\n'+s[pos:]
write('HouseStadiumDetailLook.cpp',s)
# Append two close, physically reachable eyes. All previous views remain byte-exact.
x=(.77755077,.62882017);y=(-.62882017,.77755077);o=(5105.785881991889,-11111.779237862414)
def p(u,v,z):return [round(o[0]+x[0]*u+y[0]*v,3),round(o[1]+x[1]*u+y[1]*v,3),z]
new=[{'name':'detail_stadium_coach_phone','eye':p(220,-60,115),'target':p(216,-177,31),'floor_z':-55,'fov':52},{'name':'detail_stadium_coach_chair_wall','eye':p(0,-205,115),'target':p(155,-268,39),'floor_z':-55,'fov':70}]
pth=ROOT/'SourceArt/StadiumDetail/CoachOffice/office_views.json';data=json.loads(pth.read_text());assert len(data['views'])==3;data['views']+=new;pth.write_text(json.dumps(data,indent=2)+'\n')
s=(HERE/'HouseExterior.cpp.before').read_text();anchor='    { TEXT("detail_stadium_coach_board"), FVector(5293.421,-11065.495,115.000), FVector(5436.071,-11228.932,89.000), 65.f },'
extra=''
for v in new:
 eye=','.join(f'{n:.3f}' for n in v['eye']);target=','.join(f'{n:.3f}' for n in v['target']);extra+=f'\n    {{ TEXT("{v["name"]}"), FVector({eye}), FVector({target}), {v["fov"]}.f }},'
s=rep(s,anchor,anchor+extra);write('HouseExterior.cpp',s)
pth=ROOT/'SourceArt/StadiumDetail/Draft/office_views.inl';pth.write_text(pth.read_text()+extra+'\n')
s=(HERE/'HouseStadiumDetailReview.cpp.before').read_text();a='FVector(5293.421,-11065.495,-55)})';extra=', '.join('FVector('+','.join(f'{n:.3f}' for n in v['eye'][:2])+',-55)' for v in new);s=rep(s,a,'FVector(5293.421,-11065.495,-55),'+extra+'})');write('HouseStadiumDetailReview.cpp',s)
# Strengthen final coverage to both new close views; never remove earlier camera/timing coverage.
for name in ['assemble_final_acceptance.py','recovery.py','finalize_docs.py']:
 pth=ROOT/'SourceArt/StadiumDetail'/name;s=pth.read_text();s=s.replace('==38','==40').replace('== 38','== 40').replace('else 38)','else 40)').replace('== 19','== 21').replace('Final38','Final40').replace('every38','every40').replace('Three appended170cm office views','Five appended170cm office views').replace('all24media frames','all25media frames');pth.write_text(s)
pth=ROOT/'SourceArt/StadiumDetail/OfficeClutter/preview.py';s=pth.read_text();s=s.replace("'detail_stadium_coach_board']","'detail_stadium_coach_board','detail_stadium_coach_phone','detail_stadium_coach_chair_wall']");s=s.replace('Three exact prior locker/office poses plus one appended170cm board eye','Four prior locker/office poses plus two appended170cm phone/chair-wall eyes').replace('Native four-view desk and board clutter proof','Native six-view office character proof');pth.write_text(s)
print('Installed chair/telephone/wall revision. All 40 permanent views retained; native build/proof required.')
print(new)
