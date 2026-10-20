from pathlib import Path
import sys, json
ROOT=Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'SourceArt/StadiumDetail'));from stage_runner import idle,manifest
idle();src=ROOT/'Source/SeniorSendoff';draft=ROOT/'SourceArt/StadiumDetail/Draft';before=manifest()
def put(name,s):
 for p in [src/name]+([draft/name] if name.startswith('HouseStadiumDetail') else []):p.unlink();p.write_text(s)
p=src/'HouseCampusInteriorsStadium.cpp';s=p.read_text();m='FVector(StaAt(A, .79f, StaGround + 135.f)) + Into * 20.f';assert s.count(m)==1;s=s.replace(m,'FVector(StaAt(A, .79f, StaGround + 135.f)) + Into * (HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=6?30.f:20.f)');put(p.name,s)
p=src/'HouseStadiumDetailInteriors.cpp';s=p.read_text();m='Solid.Quad(TEXT("SD_HelmetShell"),V[0],V[1],V[2],V[3],N,FHallUV::World(100));';assert s.count(m)==1
s=s.replace(m,m+'{auto& S=Solid.Sec(TEXT("SD_HelmetShell"));const int32 First=S.N.Num()-4;for(int32 I=0;I<4;++I){const FVector D=V[I]-P(0,0,16);S.N[First+I]=(X*(FVector::DotProduct(D,X)/(14*14))+Y*(FVector::DotProduct(D,Y)/(11.7f*11.7f))+FVector::UpVector*(D.Z/(15*15))).GetSafeNormal();}}')
m=' I::Piping(Clutter,TEXT("SD_FabricEdge"),Table,X,Y,90.6f,31.5f,73.5f,.38f);';assert s.count(m)==1;s=s.replace(m,m+'\n I::Piping(Clutter,TEXT("SD_FabricEdge"),Table,X,Y,90.8f,32.4f,76.8f,.30f);')
m=' for(float S:{-1.f,1.f})Clutter.Cylinder(TEXT("SD_Aluminium"),Table+X*10+Y*(S*2)+FVector(0,0,78.2f),Table+X*26+Y*(-S*2)+FVector(0,0,78.2f),.35f,8,FHallUV::World(100));';assert s.count(m)==1
s=s.replace(m,m+'''
 for(float S:{-1.f,1.f})for(int32 J=0;J<16;++J){const float A=J*2*PI/16,B=(J+1)*2*PI/16;const FVector C=Table+X*8+Y*(S*3.6f)+FVector(0,0,78.2f);Clutter.Cylinder(TEXT("SD_Aluminium"),C+X*(FMath::Cos(A)*2.8f)+Y*(FMath::Sin(A)*1.7f),C+X*(FMath::Cos(B)*2.8f)+Y*(FMath::Sin(B)*1.7f),.32f,8,FHallUV::World(100));}
''')
m=' // Drain and graduated inner fill marks';assert s.count(m)==1
s=s.replace(m,''' // The inspection side also has stamped maintenance markings, vents and a removable rear cover.
 HouseCampusFallKit::Card(Clutter,TEXT("SD_AgitatorLabel"),Motor+TX*12.15f+FVector(0,0,108),TY,FVector::UpVector,18,7,TX);
 for(float U:{-9.f,9.f})for(float Z:{90.f,115.f})I::Screw(Clutter,Motor+TX*12.05f+TY*U+FVector(0,0,Z),TX,.4f);
 for(int32 J=0;J<6;++J)I::Box(Clutter,TEXT("SD_Dark"),Motor+TX*12.1f+TY*((J-2.5f)*2.7f)+FVector(0,0,97),{.15f,1.4f,4},TR);
'''+m);put(p.name,s)
p=src/'HouseStadiumDetailLook.cpp';s=p.read_text();old='S.WidthScale=FMath::Min(1.f,(I.W*Width)/FMath::Max(1.f,HallTextWidth(Painted,S)));';assert s.count(old)==1;s=s.replace(old,'S.WidthScale=1.f;const float Fit=FMath::Min(1.f,(I.W*Width-8)/FMath::Max(1.f,HallTextWidth(Painted,S)));S.Height*=Fit;S.Stroke=S.Height*.085f;H=S.Height;')
m='  SignLook(Look,false);';assert s.count(m)==1
s=s.replace(m,'''  FHallImage Pad,PadNormal;Pad.Init(256,256,{.105f,.042f,.16f});TArray<float> PadHeight;PadHeight.SetNum(256*256);ParallelFor(256,[&](int32 Y){for(int32 X=0;X<256;++X){const float G=HallNoise(X*3.f,Y*3.f,86);Pad.At(X,Y)=FLinearColor(.105f,.042f,.16f)*(.96f+G*.085f);PadHeight[Y*256+X]=G*.05f;}});HallHeightToNormal(PadHeight,256,256,.25f,true,PadNormal);L::Texture(Look,TEXT("SD_Padding"),Pad,.65f,0,&PadNormal);
'''+m)
# Richer slate/chalk face and the existing on-air lamp keep their original meaning.
m=' UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SIGNS group=';idx=s.index(m)
s=s[:idx]+''' if(!Polish){
  S::Board(Look,TEXT("CI_StaOnAirFace"),TEXT("ON AIR"),TEXT(""),TEXT(""),512,152,{.32f,.018f,.018f},Cream,1986);
  UTexture2D* Air=Look->Textures.FindRef(TEXT("CI_StaOnAirFace"));HallMakeTextured(Look,TEXT("CI_StaOnAirFace"),Air,nullptr,.5f,FLinearColor::White,0,0,{6.f,.6f,.4f});
  FHallImage Slate;Slate.Init(1024,512,{.035f,.075f,.049f});ParallelFor(512,[&](int32 Y){for(int32 X=0;X<1024;++X){const float Rub=HallNoise(X*.015f,Y*.05f,86);Slate.At(X,Y)=FLinearColor(.035f,.075f,.049f)*(1+Rub*.12f);}});
  S::Line(Slate,TEXT("BEAT DUNMORE"),136,77,{.62f,.68f,.57f});Slate.Stroke({{182,97},{856,145}},5,{.62f,.68f,.57f},.84f);
  S::Line(Slate,TEXT("24 - 17"),382,183,{.80f,.72f,.37f});S::Line(Slate,TEXT("HOMECOMING  OCT 18 1986"),471,23,{.48f,.56f,.46f});HouseStadiumDetailLookImpl::Texture(Look,TEXT("CI_StaChalk"),Slate,.98f,0,nullptr,false);
 }
'''+s[idx:];put(p.name,s)
# Diagnostic rays into the remaining dark lintel, without moving any accepted camera.
p=src/'HouseStadiumDetailReview.cpp';s=p.read_text();m=' Checked=W;int32 Rays=0,Leaks=0;';assert s.count(m)==1
s=s.replace(m,m+'''
 const FVector Eye(4342.79,-10560.73,115),Target(5096.13,-10872.15,25);const FRotationMatrix Camera((Target-Eye).Rotation());Params.bReturnFaceIndex=true;
 for(const FVector2D Pixel:{FVector2D(120,210),FVector2D(240,245)}){const FVector D=(Camera.GetUnitAxis(EAxis::X)+Camera.GetUnitAxis(EAxis::Y)*((Pixel.X/1280.f*2-1)*FMath::Tan(FMath::DegreesToRadians(40.f)))+Camera.GetUnitAxis(EAxis::Z)*((1-Pixel.Y/720.f*2)*FMath::Tan(FMath::DegreesToRadians(40.f))*720.f/1280.f)).GetSafeNormal();FHitResult H;if(W->LineTraceSingleByChannel(H,Eye,Eye+D*2500,ECC_Pawn,Params)){int32 Section=INDEX_NONE;auto* Material=H.GetComponent()->GetMaterialFromCollisionFaceIndex(H.FaceIndex,Section);UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_LINTEL_DIAGNOSTIC pixel=%.0f,%.0f hit=%s normal=%s component=%s section=%d material=%s"),Pixel.X,Pixel.Y,*H.ImpactPoint.ToString(),*H.ImpactNormal.ToString(),*GetNameSafe(H.GetComponent()),Section,*GetNameSafe(Material));}}
''');put(p.name,s)
(ROOT/'Saved/StadiumDetailWork/stage6_a3_visual_refinement_plan.json').write_text(json.dumps({'before':before,'after':manifest(),'reason':'Actual a2 image shows wainscot masking the lower chalkboard; dedication title clips due font spacing. Fix those, smooth helmet normals, pad texture and add visible motor maintenance hardware. All original camera/route lines preserved.','native_pending':True},indent=2)+'\n')
print('A3 visual fixes installed; fresh native build/capture required.')
