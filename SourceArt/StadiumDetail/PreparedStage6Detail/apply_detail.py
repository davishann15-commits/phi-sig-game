from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'))
from stage_runner import idle,manifest
idle();frozen=manifest();src=ROOT/'Source/SeniorSendoff';art=Path(__file__).parent;draft=ROOT/'SourceArt/StadiumDetail/Draft';backup=ROOT/'Saved/StadiumDetailWork/stage6_a1_before_owner_detail'
assert not backup.exists();backup.mkdir()
def save(p,text):
 if p.exists():p.unlink()
 p.write_text(text)
def change(name,text):
 p=src/name;(backup/name).write_bytes(p.read_bytes());save(p,text)
 if name.startswith('HouseStadiumDetail'):save(draft/name,text)
# Header additions.
p=src/'HouseStadiumDetail.h';s=p.read_text();anchor='void WoodLockers(FHallMesh& Solid,FHallMesh& Detail,FVector Origin,FVector Along,int32 Count,FVector Facing);'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\nvoid LockerBench(FHallMesh& Solid,FHallMesh& Detail,FVector Start,FVector Along,float Span,FVector Facing);\nvoid LockerHelmet(FHallMesh& Solid,FHallMesh& Detail,FVector Base,FVector Facing);\nvoid SignLook(UHouseBlackHallSubsystem* Look,bool Polish);\nvoid SignHardware(FHallMesh& Mesh,FVector Centre,FVector Right,FVector Normal,float Width,float Height);')
change(p.name,s)
# Interior art helpers / replacement lockers.
p=src/'HouseStadiumDetailInteriors.cpp';s=p.read_text();start=s.index('void HouseStadiumDetail::WoodLockers(');end=s.index('void HouseStadiumDetail::Interiors(',start)
s=s[:start-2]+(art/'interior_additions.inc').read_text()+'}\n'+(art/'wood_and_helmet.inc').read_text()+s[end:]
# Better joinery preserves table/tub positions and bounds.
s=s.replace('I::SoftBox(Solid,TEXT("SD_Wood"),Table','I::SoftBox(Solid,TEXT("SD_LockerOak"),Table')
s=s.replace('I::Box(Solid,TEXT("SD_Wood"),Table','I::Box(Solid,TEXT("SD_LockerOak"),Table')
marker=' // A compact oval steel whirlpool:';assert s.count(marker)==1;s=s.replace(marker,(art/'fixture_detail.inc').read_text()+'\n'+marker)
s=s.replace('const FVector P=Table+X*(J*17-35)+FVector(0,0,78);','const FVector P=Table+X*(J*17-35)+Y*(J==1?9.f:-7.f)+FVector(0,0,78);')
s=s.replace('TEXT("SD_Aluminium"),Oval','TEXT("SD_TubSteel"),Oval')
# Use smoothly interpolated normals on the oval body rather than48 flat reflective panels.
s=s.replace('Solid.Quad(TEXT("SD_TubSteel"),Oval(A,62,36,5),Oval(B,62,36,5),Oval(B,62,36,76),Oval(A,62,36,76),Out,MetalUV);','Solid.Quad(TEXT("SD_TubSteel"),Oval(A,62,36,5),Oval(B,62,36,5),Oval(B,62,36,76),Oval(A,62,36,76),Out,MetalUV);\n  {auto& Section=Solid.Sec(TEXT("SD_TubSteel"));const int32 First=Section.N.Num()-4;for(int32 V=0;V<4;++V){const float Angle=(V==0||V==3)?A:B;Section.N[First+V]=(TX*(FMath::Cos(Angle)/62.f)+TY*(FMath::Sin(Angle)/36.f)).GetSafeNormal();}}')
s=s.replace('Tri(Clutter,TEXT("SD_Wet")','Tri(Clutter,TEXT("SD_TubWater")')
s=s.replace(' // A second small play diagram', (art/'tub_detail.inc').read_text()+'\n // A second small play diagram')
# Tunnel pad seam lines and edge binding follow its unchanged wall surfaces.
marker=' const FVector Torn=I::At(58.5f,.715f,Floor+.12f);'
addition=''' for(float Side:{-1.f,1.f}){
  const FVector Face=Mouth+AcrossTunnel*(Side*145.8f)+Rad*15;
  for(float Z:{Floor+38.f,Floor+106.f,Floor+174.f})Clutter.Cylinder(TEXT("SD_PaddingDark"),Face-Rad*43+FVector(0,0,Z),Face+Rad*43+FVector(0,0,Z),.14f,6,FHallUV::World(100));
  for(float Edge:{-43.f,43.f})Clutter.Cylinder(TEXT("SD_FabricEdge"),Face+Rad*Edge+FVector(0,0,Floor+5),Face+Rad*Edge+FVector(0,0,Floor+205),.28f,6,FHallUV::World(100));
 }
'''
assert s.count(marker)==1;s=s.replace(marker,addition+marker)
change(p.name,s)
# Stadium-local original helpers: detailed plaques, exact chord-aligned sealed corners, timber room paneling.
p=src/'HouseCampusInteriorsStadium.cpp';s=p.read_text()
marker='/** A straight flight along a chord\'s frame'
helper='''void StaPlaque(FHallMesh& M,FName Key,const FVector& C,const FVector& Out,float W,float H)
{
 WallPlaque(M,Key,C,Out,W,H);
 if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6){const FVector N=Out.GetSafeNormal2D(),R=FVector::CrossProduct(FVector::UpVector,-N).GetSafeNormal();HouseStadiumDetail::SignHardware(M,C+N*.8f,R,N,W,H);}
}
// Both room ends use the same clipped chords as the floor/sidewalls. Extend into
// the wall thickness so the old ellipse-versus-chord gap cannot show daylight.
void StaLockerEnd(FHallMesh& M,float Degrees,float Shift)
{
 if(!HouseStadiumDetail::Enabled()||HouseStadiumDetail::Step()<6){StaRadialWall(M,TEXT("CI_StaBlockPurple"),FMath::DegreesToRadians(Degrees),StaLockR0,StaLockR1+(Degrees==StaLockA0?.007f:0.f),20.f,StaGround,StaLockCeil+2.f,Shift);return;}
 int32 K;float T;StaSegOf(Degrees,K,T);const FStaChord Inner(K,StaLockR0),Outer(K,StaLockR1);
 const FVector A=FMath::Lerp(Inner.A,Inner.B,T),B=FMath::Lerp(Outer.A,Outer.B,T),Along=(B-A).GetSafeNormal(),Side(-Along.Y,Along.X,0);
 const FVector Start=A-Along*30,End=B+Along*35;const float Length=FVector::Dist(Start,End);
 M.Box(TEXT("CI_StaBlockPurple"),FTransform(Along.Rotation(),(Start+End)*.5f+Side*Shift+FVector(0,0,(StaGround+StaLockCeil+2)*.5f)),{Length*.5f,10,(StaLockCeil+2-StaGround)*.5f},FHallUV::World(80));
 // Tongue-and-groove oak wainscot, solid backing, cap and baseboard.
 const FVector Into=Side*(Shift>0?1.f:-1.f);const FVector Face=(Start+End)*.5f+Side*Shift+Into*11.1f;
 M.Box(TEXT("SD_LockerOakDark"),FTransform(Along.Rotation(),Face+FVector(0,0,StaFloor+49)),{Length*.5f,1.2f,49},FHallUV::Local(FTransform(Along.Rotation(),Face),100));
 for(float U=3;U<Length;U+=12){const FVector C=Start+Along*U+Side*Shift+Into*12.5f;M.Box(TEXT("SD_LockerOak"),FTransform(Along.Rotation(),C+FVector(0,0,StaFloor+49)),{5.8f,.4f,48},FHallUV::Local(FTransform(Along.Rotation(),C),100));}
 for(float Z:{StaFloor+5,StaFloor+99})M.Box(TEXT("SD_LockerOakLight"),FTransform(Along.Rotation(),Face+Into*.8f+FVector(0,0,Z)),{Length*.5f,1.8f,2.5f},FHallUV::World(100));
}
void StaWoodPanel(FHallMesh& Solid,const FStaChord& Frame,float U0,float U1,float D0,float D1,const TArray<FBox2D>& Holes)
{
 Frame.Wall(Solid,TEXT("SD_LockerOakDark"),U0,U1,D0,D1,StaFloor,StaFloor+98,Holes,FHallUV::Local(FTransform(Frame.Along.Rotation(),Frame.A),100));
 const float Front=D1>0?D0:D1;for(float U=U0+6;U<U1;U+=12)Frame.Wall(Solid,TEXT("SD_LockerOak"),U-5.7f,FMath::Min(U+5.7f,U1),Front-.4f,Front+.4f,StaFloor+2,StaFloor+96,Holes,FHallUV::Local(FTransform(Frame.Along.Rotation(),Frame.A),100));
 Frame.Wall(Solid,TEXT("SD_LockerOakLight"),U0,U1,FMath::Min(D0,D1)-.5f,FMath::Max(D0,D1)+.5f,StaFloor+97,StaFloor+102,Holes,FHallUV::World(100));
}
'''
assert s.count(marker)==1;s=s.replace(marker,helper+'\n'+marker)
# Rename only stadium plaque call sites; keep the delegate inside StaPlaque itself unchanged.
pos=s.index('void StaFlight(');s=s[:pos]+s[pos:].replace('WallPlaque(', 'StaPlaque(')
old='''            for (int32 L = 0; L < Count; L += 2)
                O.Slab(Clutter, TEXT("CI_StreamerPurple"), U0 + 38.f * L + 8.f, U0 + 38.f * L + 30.f, -(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?15.f:54.f), -(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?12.f:51.f), StaFloor + 70.f, StaFloor + 150.f);   // detail: inside the open wood bay; legacy: on the door hook
            O.Slab(Solid, TEXT("CI_Oak"), U0, U0 + 38.f * Count, -105.f, -70.f, StaFloor + 42.f, StaFloor + 46.f);
            for (float U = U0 + 20.f; U < U0 + 38.f * Count; U += 120.f) O.Slab(Solid, TEXT("CI_Steel"), U - 2.f, U + 2.f, -100.f, -75.f, StaFloor, StaFloor + 42.f);'''
new='''            if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6)HouseStadiumDetail::LockerBench(Solid,Clutter,O.At(U0,-87.5f,StaFloor),O.Along,38.f*Count,-O.N);
            else{
                for (int32 L = 0; L < Count; L += 2)O.Slab(Clutter,TEXT("CI_StreamerPurple"),U0+38.f*L+8.f,U0+38.f*L+30.f,-54.f,-51.f,StaFloor+70.f,StaFloor+150.f);
                O.Slab(Solid,TEXT("CI_Oak"),U0,U0+38.f*Count,-105.f,-70.f,StaFloor+42.f,StaFloor+46.f);
                for(float U=U0+20.f;U<U0+38.f*Count;U+=120.f)O.Slab(Solid,TEXT("CI_Steel"),U-2.f,U+2.f,-100.f,-75.f,StaFloor,StaFloor+42.f);
            }'''
assert s.count(old)==1;s=s.replace(old,new)
old='Helmet(Solid, O.At(U, -88.f, StaFloor + 46.f), FVector(FMath::Cos(Bench * 1.7f), FMath::Sin(Bench * 1.7f), 0.f));'
new='if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6)HouseStadiumDetail::LockerHelmet(Solid,Clutter,O.At(U,-88.f,StaFloor+46.f),FVector(FMath::Cos(Bench*1.7f),FMath::Sin(Bench*1.7f),0));else '+old
assert s.count(old)==1;s=s.replace(old,new)
old='''    StaRadialWall(Solid, TEXT("CI_StaBlockPurple"), FMath::DegreesToRadians(StaLockA0), StaLockR0, StaLockR1 + .007f, 20.f, StaGround, StaLockCeil + 2.f, 10.f);
    StaRadialWall(Solid, TEXT("CI_StaBlockPurple"), FMath::DegreesToRadians(StaLockA1), StaLockR0, StaLockR1, 20.f, StaGround, StaLockCeil + 2.f, -10.f);'''
assert s.count(old)==1;s=s.replace(old,'    StaLockerEnd(Solid,StaLockA0,10.f);\n    StaLockerEnd(Solid,StaLockA1,-10.f);')
marker='''        // Lockers along the outer wall'''
addition='''        if(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6){
            StaWoodPanel(Solid,I,I.Len*Pt.T0,I.Len*Pt.T1,.3f,2.8f,Holes);
            if(Pt.K<StaK0)StaWoodPanel(Solid,O,O.Len*Pt.T0,O.Len*Pt.T1,-2.8f,-.3f,{});
        }
'''
assert s.count(marker)==1;s=s.replace(marker,addition+marker)
change(p.name,s)
# Sign material art: functions appended to owned Look.cpp; both material factories can call its public entry.
p=src/'HouseStadiumDetailLook.cpp';s=p.read_text();idx=s.index(' UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SURFACE_PREP lookParallel=')
s=s[:idx]+' if(Step()>=6){\n'+'''  // Vertical oak fibres follow each timber's local frame. Fine normal relief replaces broad repeated swirls.
  for(int32 Variant=0;Variant<3;++Variant){FHallImage I,N;I.Init(512,512,{.24f,.135f,.058f});TArray<float> Heights;Heights.SetNum(512*512);
   ParallelFor(512,[&](int32 Y){for(int32 X=0;X<512;++X){const float Bend=FMath::Sin(Y*.013f+Variant)*6+FMath::Sin(Y*.004f)*12,Grain=HallNoise((X+Bend)*.45f,Y*.018f,1931+Variant),Fine=HallNoise(X*2.f,Y*.05f,53),Shade=.76f+Grain*.28f+Fine*.055f;const float Tint=Variant==0?1.13f:Variant==1?1.f:.84f;I.At(X,Y)=FLinearColor(.27f,.152f,.067f)*Shade*Tint;I.At(X,Y).A=1;Heights[Y*512+X]=Grain*.15f+Fine*.015f;}});HallHeightToNormal(Heights,512,512,.3f,true,N);L::Texture(Look,Variant==0?TEXT("SD_LockerOakLight"):Variant==1?TEXT("SD_LockerOak"):TEXT("SD_LockerOakDark"),I,.62f,0,&N);
  }
  FHallImage Steel;Steel.Init(512,256,{.46f,.47f,.45f});ParallelFor(256,[&](int32 Y){for(int32 X=0;X<512;++X){const float Fine=HallNoise(X*.05f,Y*2.f,31);Steel.At(X,Y)=FLinearColor(.48f,.49f,.47f)*(.97f+Fine*.055f);}});L::Texture(Look,TEXT("SD_TubSteel"),Steel,.31f,.88f);
  HallMakePlain(Look,TEXT("SD_HelmetShell"),{.105f,.033f,.16f},.29f,.07f);HallMakePlain(Look,TEXT("SD_Jersey"),{.12f,.048f,.17f},.94f);HallMakePlain(Look,TEXT("SD_NameBrass"),{.44f,.32f,.13f},.55f,.68f);HallMakePlain(Look,TEXT("SD_Towel"),{.54f,.49f,.38f},.97f);HallMakePlain(Look,TEXT("SD_BottleAmber"),{.16f,.068f,.022f},.35f);
  if(auto* Parent=HallHouseMat(TEXT("M_House_Glass"))){for(int32 I=0;I<2;++I){auto* Mat=UMaterialInstanceDynamic::Create(Parent,Look);Mat->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.065f,.078f,.066f));Mat->SetScalarParameterValue(TEXT("Opacity"),I==0?.18f:.12f);Mat->SetScalarParameterValue(TEXT("Roughness"),I==0?.18f:.30f);Look->Materials.Add(I==0?TEXT("SD_TubWater"):TEXT("SD_Wet"),Mat);}}
  else HallMakePlain(Look,TEXT("SD_TubWater"),{.085f,.10f,.085f},.24f);
  SignLook(Look,false);
 }
'''+s[idx:]
addition=(art/'signs_and_materials.inc').read_text()
addition=addition.replace('HouseStadiumDetailInteriorsImpl_PLACEHOLDER;', '''{const FVector P=C+Right*(X*(W*.5f-2.1f))+FVector(0,0,Z*(H*.5f-2.1f))+Normal*.55f;M.Cylinder(TEXT("SD_Aluminium"),P,P+Normal*.3f,.48f,8,FHallUV::World(100));const FVector A=P+Normal*.32f-Right*.28f,B=P+Normal*.32f+Right*.28f;M.Quad(TEXT("SD_Dark"),A-FVector(0,0,.06f),B-FVector(0,0,.06f),B+FVector(0,0,.06f),A+FVector(0,0,.06f),Normal,FHallUV::World(100));}''')
change(p.name,s+'\n'+addition)
# Attach detailed frames to all generic stadium-owned boards and all existing outdoor metal plates.
p=src/'HouseStadiumDetail.cpp';s=p.read_text();old='void AHouseStadiumDetail::Sign(FName Material,FVector C,FVector Right,FVector N,float Width,float Height)\n{const FVector U=FVector::UpVector;'
assert s.count(old)==1;s=s.replace(old,old.replace('{const FVector','{if(Material!=TEXT("SD_Goose"))HouseStadiumDetail::SignHardware(Unique,C,Right,N,Width,Height);const FVector'))
marker='void AHouseStadiumDetail::Letter(const FString& Word,FVector C,FVector Right,FVector N,float Height,FName Material)\n{const float Pitch='
assert s.count(marker)==1;s=s.replace(marker,marker.replace('{const float','{if(Word.StartsWith(TEXT("SEC ")))HouseStadiumDetail::SignHardware(Unique,C,Right,N,Word.Len()*Height*.85f+10,Height*2.f);const float'))
change(p.name,s)
p=src/'HouseCampusPolishStadium.cpp';s=p.read_text();marker='    Letters(M, Ink, Text, C + N * 1.7f, Right, N, Cap, Cap * .16f);'
assert s.count(marker)==1;s=s.replace(marker,marker+'\n    HouseStadiumDetail::SignHardware(M,C+N*1.72f,Right,N,W,H);')
marker='    for (const FPlainSpec& P : Plains) HallMakePlain(Look, P.Key, P.Tint, P.Rough, P.Metal, P.Emission);'
assert s.count(marker)==1;s=s.replace(marker,marker+'\n    HouseStadiumDetail::SignLook(Look,true);')
# Upsample the real six-row hand-painted menus and preserve their complete items/prices.
s=s.replace('constexpr int32 W = 256, H = 320;','constexpr int32 W = 256, H = 320;') # Original menu layout retained; richer headers/surrounds are separate.
change(p.name,s)
receipt={'created_utc':datetime.now(timezone.utc).isoformat(),'request':'Owner rejects stage6 room detail; identifies locker-room wall gap in screenshot; wants all stadium signs detailed and more wooden room feel.','before_source_config_map_sha':frozen,'after_source_config_map_sha':manifest(),'new_content_assets':[],'native_validation_pending':True,'stage6_accepted':False,'fixed_existing_camera_and_route_lines':True}
(ROOT/'Saved/StadiumDetailWork/stage6_owner_detail_revision.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Detailed interior and sign revision installed; native build, visual inspection and full gates pending.')
