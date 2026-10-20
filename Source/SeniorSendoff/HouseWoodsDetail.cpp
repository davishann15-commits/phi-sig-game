#include "HouseWoodsDetail.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLook.h"
#include "HouseWestField.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

namespace HouseWoods
{
namespace
{
constexpr float GroundBase=-362.f;
const FHallUV WoodUV=FHallUV::World(110.f),Plain=FHallUV::World(90.f);
float Smooth(float A,float B,float V){return FMath::SmoothStep(A,B,V);}
float ClearingDistance(float X,float Y){return FVector2D((X-HutX)/1.25f,Y-HutY).Size();}
void Box(FHallMesh& M,FName K,FVector A,FVector B){M.BoxMinMax(K,A,B,WoodUV);}
void Beam(FHallMesh& M,FName K,FVector A,FVector B,float R){M.Cylinder(K,A,B,R,9,WoodUV);}
void Surface(FHallMesh& M,FName K,FVector A,FVector B,FVector C,FVector D)
{FVector N=FVector::CrossProduct(B-A,D-A).GetSafeNormal();if(N.Z<0)N=-N;M.Quad(K,A,B,C,D,N,FHallUV::World(180));}
FVector At(float X,float Y,float Raise=0){return FVector(X,Y,Height(X,Y)+Raise);}
FTransform GroundPlacement(float X,float Y,float Yaw,float Scale,float Raise)
{
 const FVector Up(-(Height(X+10,Y)-Height(X-10,Y))/20.f,-(Height(X,Y+10)-Height(X,Y-10))/20.f,1);
 const FVector Forward=FRotator(0,Yaw,0).Vector();
 return FTransform(FRotationMatrix::MakeFromZX(Up.GetSafeNormal(),Forward).ToQuat(),At(X,Y,Raise),FVector(Scale));
}
UMaterialInstanceDynamic* PlainMaterial(UHouseBlackHallSubsystem* Look,FName Key,FLinearColor Tint,float Rough=.92f,float Metal=0,FLinearColor Emission=FLinearColor::Black)
{
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropPlain.M_NaturePropPlain"));
 if(!Parent)return HallMakePlain(Look,Key,Tint,Rough,Metal,Emission);
 auto* M=UMaterialInstanceDynamic::Create(Parent,Look);M->SetVectorParameterValue(TEXT("Tint"),Tint);M->SetVectorParameterValue(TEXT("Emission"),Emission);
 M->SetScalarParameterValue(TEXT("Roughness"),Rough);M->SetScalarParameterValue(TEXT("Metallic"),Metal);Look->Materials.Add(Key,M);return M;
}
void Ring(FHallMesh& M,FName K,FVector Center,FVector Axis,float Radius,float Tube)
{
 const FVector U=FVector::CrossProduct(Axis,FVector::UpVector).GetSafeNormal(),V=FVector::CrossProduct(Axis,U).GetSafeNormal();
 for(int I=0;I<24;++I){float A=I*2*PI/24,B=(I+1)*2*PI/24;Beam(M,K,Center+(U*FMath::Cos(A)+V*FMath::Sin(A))*Radius,Center+(U*FMath::Cos(B)+V*FMath::Sin(B))*Radius,Tube);}
}
void RockLook(UHouseBlackHallSubsystem* Look)
{
 // One shared mineral surface and normal map, rather than geometry for every grain.
 constexpr int Size=512;FHallImage Stone,Normal;Stone.Init(Size,Size,FLinearColor::Black);TArray<float> Relief;Relief.SetNum(Size*Size);
 for(int Y=0;Y<Size;++Y)for(int X=0;X<Size;++X)
 {
  const float Coarse=HallFbm(X*.022f,Y*.022f,4,717),Grain=HallNoise(X*.94f,Y*.94f,727),Fine=HallNoise(X*2.1f,Y*2.1f,743);
  const float Warp=HallFbm(X*.011f,Y*.011f,3,751)*24.f;
  const float Vein=FMath::Pow(FMath::Max(0.f,1-FMath::Abs(FMath::Sin((X*.53f+Y*.82f+Warp)*.092f))*12.f),2.f);
  const float Crack=FMath::Pow(FMath::Max(0.f,1-FMath::Abs(FMath::Sin((X*.71f-Y*.32f+Warp*1.9f)*.071f))*24.f),2.f);
  const float Mineral=FMath::SmoothStep(.78f,.97f,Grain),Moss=FMath::SmoothStep(.55f,.80f,HallFbm(X*.038f,Y*.038f,3,761));
  const float Value=.11f+Coarse*.17f+Grain*.08f+Mineral*.17f+Vein*.09f-Crack*.11f;
  const FLinearColor Grey(Value,Value*1.015f,Value*.98f),Green(Value*.47f,Value*.61f,Value*.30f);
  Stone.At(X,Y)=FMath::Lerp(Grey,Green,Moss*.72f);Relief[Y*Size+X]=Coarse*.17f+Fine*.09f+Mineral*.10f-Crack*.18f+Vein*.035f;
 }
 HallHeightToNormal(Relief,Size,Size,2.1f,true,Normal);
 auto* BaseTexture=HallMakeTexture(Stone,true,false,TEXT("T_WoodsStone"));auto* NormalTexture=HallMakeTexture(Normal,false,true,TEXT("T_WoodsStoneN"));
 Look->Textures.Add(TEXT("T_WoodsStone"),BaseTexture);Look->Textures.Add(TEXT("T_WoodsStoneN"),NormalTexture);
 auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
 for(int V=0;V<3;++V)
 {
  const FName Key(*FString::Printf(TEXT("EX_WoodsRock%d"),V));const FLinearColor Tint=V==1?FLinearColor(1.1f,.97f,.80f):V==2?FLinearColor(.82f,.91f,1.f):FLinearColor::White;
  if(!Parent){HallMakeTextured(Look,Key,BaseTexture,NormalTexture,.97f,Tint,.8f);continue;}
  auto* Material=UMaterialInstanceDynamic::Create(Parent,Look);Material->SetTextureParameterValue(TEXT("BaseMap"),BaseTexture);Material->SetTextureParameterValue(TEXT("NormalMap"),NormalTexture);
  Material->SetScalarParameterValue(TEXT("UVScale"),1.f);Material->SetScalarParameterValue(TEXT("Roughness"),.97f);Material->SetScalarParameterValue(TEXT("NormalStrength"),.8f);
  Material->SetVectorParameterValue(TEXT("Tint"),Tint);Material->SetVectorParameterValue(TEXT("Emission"),FLinearColor::Black);Look->Materials.Add(Key,Material);
 }
}
}
const TArray<FHollow>& Hollows()
{
 static const TArray<FHollow> H={{{-4200,8000},440,135},{{2700,10400},580,175},{{-3300,16000},520,150}};return H;
}
const TArray<FVector2D>& Trail()
{
 static const TArray<FVector2D> T={{-2400,4380},{-2800,5000},{-3350,6600},{-2900,8000},{-1700,9600},{-1200,11200},{-800,13000},{-800,13200},{-800,13600}};return T;
}
const TArray<FVector2D>& SmoothTrail()
{
 // A shared, finely sampled curve drives both the visible ribbon and its clear walking corridor.
 static const TArray<FVector2D> Curve=[]()
 {
  TArray<FVector2D> Samples;const auto& Points=Trail();
  for(int I=0;I+1<Points.Num();++I)
  {
   const FVector2D P0=Points[FMath::Max(0,I-1)],P1=Points[I],P2=Points[I+1],P3=Points[FMath::Min(Points.Num()-1,I+2)];
   const int Steps=FMath::CeilToInt((P2-P1).Size()/48.f);
   for(int J=0;J<Steps;++J){const float T=float(J)/Steps;Samples.Add((P1*2+(P2-P0)*T+(P0*2-P1*5+P2*4-P3)*T*T+(-P0+P1*3-P2*3+P3)*T*T*T)*.5f);}
  }
  Samples.Add(Points.Last());return Samples;
 }();return Curve;
}
float TrailDistance(float X,float Y)
{
 const FVector2D P(X,Y);float Squared=MAX_flt;const auto& T=SmoothTrail();
 for(int I=0;I+1<T.Num();++I){const FVector2D AB=T[I+1]-T[I];float U=FMath::Clamp(FVector2D::DotProduct(P-T[I],AB)/AB.SizeSquared(),0.f,1.f);Squared=FMath::Min(Squared,float((P-T[I]-AB*U).SizeSquared()));}return FMath::Sqrt(Squared);
}
float Height(float X,float Y)
{
 if(X<=XMin||X>=XMax||Y<=YMin||Y>=YMax)return HouseWestField::RaisedHeight(X,Y,GroundBase);
 const float Fade=Smooth(YMin,YMin+700,Y)*(1-Smooth(YMax-600,YMax,Y))*Smooth(XMin,XMin+600,X)*(1-Smooth(XMax-600,XMax,X));
 struct FHill {float X,Y,R,H;};
 static const FHill Hills[]={{-6100,6800,1900,285},{-5200,11000,2050,435},{3400,7800,1950,340},{3500,14600,2350,520},{-5800,17300,2050,480},{6500,17000,1650,350},{-500,18000,2100,255}};
 float Z=0;
 for(const FHill& H:Hills){const float R2=FVector2D(X-H.X,Y-H.Y).SizeSquared()/(H.R*H.R);if(R2<1)Z+=H.H*FMath::Square(1-R2);}
 for(const FHollow& H:Hollows()){const float R2=FVector2D(X-H.Center.X,Y-H.Center.Y).SizeSquared()/(H.Radius*H.Radius);if(R2<1)Z-=H.Depth*FMath::Square(1-R2);}
 Z+=12*FMath::Sin(X*.0023f)*FMath::Sin(Y*.0019f);
 // A gently graded trail and a level clearing keep the hut reachable.
 Z*=Smooth(170,380,TrailDistance(X,Y))*Smooth(460,1150,ClearingDistance(X,Y));
 return HouseWestField::RaisedHeight(X,Y,GroundBase+Z*Fade);
}
bool TreeAllowed(float X,float Y)
{
 if(Y<YMin)return true;
 if(TrailDistance(X,Y)<280||ClearingDistance(X,Y)<900)return false;
 for(const FHollow& H:Hollows())if(FVector2D(X-H.Center.X,Y-H.Center.Y).Size()<H.Radius*.9f)return false;
 return true;
}
void FitTrees(TArray<FTransform>& Big,TArray<FTransform>& Small,TArray<FTransform>& Distant)
{
 auto Fit=[&](TArray<FTransform>& Trees,bool Cheap)
 {
  Trees.RemoveAll([](const FTransform& T){const auto P=T.GetLocation();return !TreeAllowed(P.X,P.Y);});
  for(FTransform& T:Trees){FVector P=T.GetLocation();if(P.Y<YMin){if(P.Y>=1600){P.Z=Height(P.X,P.Y)-4;T.SetLocation(P);}continue;}P.Z=Height(P.X,P.Y)-4;T.SetLocation(P);
   const float N=.5f+.5f*FMath::Sin(P.X*.00371f+P.Y*.00713f);const float S=Cheap?FMath::Lerp(.58f,1.74f,N):FMath::Lerp(.72f,1.42f,N);
   T.SetScale3D(FVector(S,S,S*(.88f+.20f*N)));
  }
 };
 Fit(Big,false);Fit(Small,false);Fit(Distant,true);
 // Mature focal oaks/birches around the clearing retain the existing hit-and-leaf behaviour.
 for(int I=0;I<6;++I){float A=I*2*PI/6+.3f;float X=HutX+FMath::Cos(A)*1320,Y=HutY+FMath::Sin(A)*1050;if(!TreeAllowed(X,Y))continue;
  Big.Add(FTransform(FRotator(0,I*57.f,0),At(X,Y,-4),FVector(I%2?1.30f:1.65f)));
 }
}
void CreateLook(UHouseBlackHallSubsystem* Look)
{
 PlainMaterial(Look,TEXT("EX_WoodsCutWood"),FLinearColor(.30f,.22f,.12f));
 PlainMaterial(Look,TEXT("EX_WoodsGrowthRing"),FLinearColor(.085f,.046f,.025f));
 PlainMaterial(Look,TEXT("EX_WoodsMoss"),FLinearColor(.043f,.059f,.025f));
 PlainMaterial(Look,TEXT("EX_WoodsPetalCream"),FLinearColor(.61f,.53f,.30f));
 PlainMaterial(Look,TEXT("EX_WoodsPetalGold"),FLinearColor(.47f,.27f,.055f));
 PlainMaterial(Look,TEXT("EX_WoodsPetalPurple"),FLinearColor(.27f,.12f,.24f));
 PlainMaterial(Look,TEXT("EX_WoodsStem"),FLinearColor(.05f,.085f,.025f));
 RockLook(Look);
 PlainMaterial(Look,TEXT("EX_WoodsHutWood"),FLinearColor(.12f,.075f,.042f));
 PlainMaterial(Look,TEXT("EX_WoodsRoofMetal"),FLinearColor(.10f,.12f,.11f),.78f,.4f);
 PlainMaterial(Look,TEXT("EX_WoodsLantern"),FLinearColor(.8f,.49f,.18f),.4f,0,FLinearColor(2.8f,1.5f,.55f));
 // Use the house lawn's photographed surfaces and palette, with gentler coverage variation.
 FHallImage Coverage;Coverage.Init(512,512,FLinearColor::Black);
 for(int Y=0;Y<512;++Y)for(int X=0;X<512;++X){const float N=HallFbm(X*.009f,Y*.009f,3,919);Coverage.At(X,Y)=FLinearColor(.38f+N*.10f,.075f+N*.055f,.35f+N*.12f,1);}
 auto* CoverageTexture=HallMakeTexture(Coverage,true,false,TEXT("T_WoodsEvenCoverage"),false);Look->Textures.Add(TEXT("T_WoodsEvenCoverage"),CoverageTexture);
 auto* HouseGround=Cast<UMaterialInstanceDynamic>(Look->Mat(TEXT("EX_Grass")));
 auto* GroundParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_WoodsGround.M_WoodsGround"));
 if(!GroundParent)GroundParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureGround.M_NatureGround"));
 auto* WoodlandGround=UMaterialInstanceDynamic::Create(GroundParent,Look);WoodlandGround->CopyMaterialUniformParameters(HouseGround);WoodlandGround->SetTextureParameterValue(TEXT("CoverageMap"),CoverageTexture);Look->Materials.Add(TEXT("EX_WoodsGround"),WoodlandGround);
 // A MID cannot parent another MID. Create the trail from the saved master
 // and explicitly assign its dry, lighter dirt maps so no blank fallback appears.
 FHallImage TrailDirt,TrailNormal;TrailDirt.Init(256,256,FLinearColor::Black);TArray<float> TrailRelief;TrailRelief.SetNum(256*256);
 for(int Y=0;Y<256;++Y)for(int X=0;X<256;++X)
 {
  const float Cloud=HallFbm(X*.035f,Y*.035f,4,813),Grain=HallNoise(X*1.9f,Y*1.9f,827),Pebble=FMath::SmoothStep(.87f,.98f,HallNoise(X*.8f,Y*.8f,839));
  const float Value=.24f+Cloud*.14f+Grain*.045f+Pebble*.08f;
  TrailDirt.At(X,Y)=FLinearColor(Value,Value*.78f,Value*.52f);TrailRelief[Y*256+X]=Cloud*.09f+Grain*.10f+Pebble*.13f;
 }
 HallHeightToNormal(TrailRelief,256,256,1.25f,true,TrailNormal);
 auto* TrailBaseTexture=HallMakeTexture(TrailDirt,true,false,TEXT("T_WoodsTrailDirt"));auto* TrailNormalTexture=HallMakeTexture(TrailNormal,false,true,TEXT("T_WoodsTrailDirtN"));
 Look->Textures.Add(TEXT("T_WoodsTrailDirt"),TrailBaseTexture);Look->Textures.Add(TEXT("T_WoodsTrailDirtN"),TrailNormalTexture);
 auto* TrailParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_WoodsTrail.M_WoodsTrail"));
 if(TrailParent)
 {
  auto* TrailMaterial=UMaterialInstanceDynamic::Create(TrailParent,Look);TrailMaterial->CopyMaterialUniformParameters(WoodlandGround);
  TrailMaterial->SetTextureParameterValue(TEXT("DirtMap"),TrailBaseTexture);TrailMaterial->SetTextureParameterValue(TEXT("DirtNormalMap"),TrailNormalTexture);Look->Materials.Add(TEXT("EX_WoodsTrail"),TrailMaterial);
 }
 else HallMakeTextured(Look,TEXT("EX_WoodsTrail"),TrailBaseTexture,TrailNormalTexture,.99f,FLinearColor::White,.65f);
}
void Terrain(FHallMesh& M)
{
 constexpr int NX=132,NY=117;
 auto P=[](int I,int J){const float X=XMin+(XMax-XMin)*I/NX,Y=YMin+(YMax-YMin)*J/NY;return At(X,Y);};
 for(int I=0;I<NX;++I)for(int J=0;J<NY;++J){const FVector A=P(I,J),B=P(I+1,J),C=P(I+1,J+1),D=P(I,J+1);Surface(M,TEXT("EX_WoodsGround"),A,B,C,D);}
 const auto& T=SmoothTrail();TArray<FVector> Left,Right;TArray<float> Distance;float Along=0;
 for(int I=0;I<T.Num();++I)
 {
  const FVector2D Direction=(T[FMath::Min(T.Num()-1,I+1)]-T[FMath::Max(0,I-1)]).GetSafeNormal(),Side(-Direction.Y,Direction.X);
  const float Width=194+5*FMath::Sin(T[I].X*.006f+T[I].Y*.005f);const FVector2D A=T[I]-Side*Width,B=T[I]+Side*Width;
  Left.Add(At(A.X,A.Y,1.3f));Right.Add(At(B.X,B.Y,1.3f));if(I)Along+=(T[I]-T[I-1]).Size();Distance.Add(Along/180.f);
 }
 for(int I=0;I+1<T.Num();++I)
 {
  const FVector N=FVector::CrossProduct(Left[I+1]-Left[I],Right[I]-Left[I]).GetSafeNormal();const FVector Up=N.Z<0?-N:N;
  M.QuadUV(TEXT("EX_WoodsTrail"),Left[I],Left[I+1],Right[I+1],Right[I],Up,{Distance[I],0},{Distance[I+1],0},{Distance[I+1],1},{Distance[I],1},FVector::ForwardVector);
  auto& Section=M.Sec(TEXT("EX_WoodsTrail"));const int End=Section.V.Num();
  for(int J=End-4;J<End;++J){const FVector V=Section.V[J];const FVector Normal=FVector(-(Height(V.X+10,V.Y)-Height(V.X-10,V.Y))/20.f,-(Height(V.X,V.Y+10)-Height(V.X,V.Y-10))/20.f,1).GetSafeNormal();Section.N[J]=Normal;Section.T[J]=FProcMeshTangent((FVector::ForwardVector-Normal*Normal.X).GetSafeNormal(),false);}
 }
}
void Hut(FHallMesh& M)
{
 const FName Bark(TEXT("EX_NatureBark")),Wood(TEXT("EX_WoodsHutWood")),Cut(TEXT("EX_WoodsCutWood")),Roof(TEXT("EX_WoodsRoofMetal"));
 auto P=[](float X,float Y,float Z){return FVector(HutX+X,HutY+Y,HutFloor+Z);};
 // Solid plank floor and short porch steps; the open doorway is 130 cm wide.
 Box(M,Wood,P(-205,-370,-16),P(205,255,-6));
 for(float X=-204;X<205;X+=17)Box(M,Wood,P(X,-370,-6),P(FMath::Min(X+16.6f,205.f),255,0));
 for(int Step=0;Step<3;++Step)Box(M,Wood,P(-85,-425+Step*18,-18),P(85,-405+Step*18,-12+Step*6));
 for(int Row=0;Row<16;++Row){float Z=8+Row*16;
  Beam(M,Bark,P(-196,250,Z),P(196,250,Z),10);
  for(float X:{-190.f,190.f}){if(Z>92&&Z<181){Beam(M,Bark,P(X,-245,Z),P(X,-78,Z),10);Beam(M,Bark,P(X,78,Z),P(X,255,Z),10);}else Beam(M,Bark,P(X,-255,Z),P(X,255,Z),10);}
  if(Z<224){Beam(M,Bark,P(-198,-250,Z),P(-65,-250,Z),10);Beam(M,Bark,P(65,-250,Z),P(198,-250,Z),10);}else Beam(M,Bark,P(-198,-250,Z),P(198,-250,Z),10);
 }
 for(float X:{-190.f,190.f}){Box(M,Cut,P(X-6,-88,84),P(X+6,-77,190));Box(M,Cut,P(X-6,77,84),P(X+6,88,190));Box(M,Cut,P(X-6,-88,84),P(X+6,88,93));Box(M,Cut,P(X-6,-88,181),P(X+6,88,190));Beam(M,Wood,P(X,-82,132),P(X,82,132),3);Beam(M,Wood,P(X,0,91),P(X,0,182),3);}
 Box(M,Cut,P(-74,-264,0),P(-65,-240,230));Box(M,Cut,P(65,-264,0),P(74,-240,230));Box(M,Cut,P(-74,-264,224),P(74,-240,234));
 // Weathered timber name board on the exterior gable, above the door opening.
 Box(M,Wood,P(-115,-265,250),P(115,-257,288));
 Box(M,Cut,P(-111,-266,254),P(111,-265.2f,284));
 for(float X:{-105.f,105.f})for(float Z:{257.f,281.f})
  M.Cylinder(Roof,P(X,-266.2f,Z),P(X,-267.1f,Z),1.05f,8,Plain);
 for(int I=0;I<7;++I){const float X=-101+I*31,Z=255+(I%3)*13;
  Box(M,Wood,P(X,-266.3f,Z),P(X+13,-266.1f,Z+.35f));}
 // A weathered pitched metal roof, raised seams, rafters and triangular gable boarding.
 Surface(M,Roof,P(-235,-410,262),P(0,-410,345),P(0,285,345),P(-235,285,262));
 Surface(M,Roof,P(0,-410,345),P(235,-410,262),P(235,285,262),P(0,285,345));
 for(float Y=-390;Y<285;Y+=86){Beam(M,Roof,P(-235,Y,264),P(0,Y,348),2);Beam(M,Roof,P(0,Y,348),P(235,Y,264),2);Beam(M,Wood,P(-215,Y,252),P(0,Y,332),6);Beam(M,Wood,P(0,Y,332),P(215,Y,252),6);}
 for(float X=-190;X<190;X+=20){const float Top=332-FMath::Abs(X)*.36f;for(float Y:{-250.f,250.f})Box(M,Wood,P(X,Y-4,250),P(X+18,Y+4,Top));}
 for(float X:{-190.f,190.f})Beam(M,Bark,P(X,-352,-3),P(X,-352,253),8);
 // Bench and a small table leave a clear walking lane to the back wall.
 Box(M,Wood,P(-160,177,41),P(150,219,47));for(float X:{-140.f,125.f})Box(M,Wood,P(X,182,0),P(X+10,211,41));Box(M,Wood,P(-160,216,46),P(150,224,92));
 Box(M,Cut,P(84,-18,68),P(158,55,75));for(float X:{91.f,143.f})for(float Y:{-11.f,39.f})Box(M,Wood,P(X,Y,0),P(X+8,Y+8,68));
 // Lantern geometry glows; one small real light is added by the exterior owner.
 Beam(M,Roof,P(0,-200,267),P(0,-200,246),2);Box(M,Roof,P(-10,-210,220),P(10,-190,224));Box(M,TEXT("EX_WoodsLantern"),P(-7,-207,225),P(7,-193,241));Box(M,Roof,P(-10,-210,241),P(10,-190,245));
 for(float X:{-9.f,9.f})for(float Y:{-209.f,-191.f})Beam(M,Roof,P(X,Y,223),P(X,Y,242),1);
}
FScatter Scatter()
{
 FScatter S;FRandomStream R(991026);
 const FVector Hero(-3950,6860,Height(-3950,6860)+2);
 S.Logs.Add(FTransform(FRotator(0,35,0),Hero,FVector(1.15f,1.1f,1.1f)));
 auto Allowed=[](float X,float Y){return TrailDistance(X,Y)>350&&ClearingDistance(X,Y)>1050;};
 for(int I=0;I<20;++I){const float X=R.FRandRange(-7200,7200),Y=R.FRandRange(6100,17800);if(!Allowed(X,Y)){--I;continue;}const float A=R.FRandRange(0,2*PI),Length=R.FRandRange(.65f,1.45f);const FVector2D V(FMath::Cos(A),FMath::Sin(A));float Z0=Height(X-V.X*300*Length,Y-V.Y*300*Length),Z1=Height(X+V.X*300*Length,Y+V.Y*300*Length);FVector Dir(V.X*600*Length,V.Y*600*Length,Z1-Z0);const FQuat Q=FRotationMatrix::MakeFromX(Dir).ToQuat();S.Logs.Add(FTransform(Q,FVector(X,Y,(Z0+Z1)*.5f+2),FVector(Length,R.FRandRange(.7f,1.3f),R.FRandRange(.75f,1.2f))));}
 for(int I=0;I<175;++I){float X=R.FRandRange(-7600,7600),Y=R.FRandRange(5300,18500);if(!Allowed(X,Y))continue;S.Branches.Add(GroundPlacement(X,Y,R.FRandRange(0,360),R.FRandRange(.7f,1.6f),2));}
 for(int I=0;I<420;++I){float X=R.FRandRange(-7500,7500),Y=R.FRandRange(5100,18000);if(TrailDistance(X,Y)<175||ClearingDistance(X,Y)<440)continue;S.Flowers.Add(GroundPlacement(X,Y,R.FRandRange(0,360),R.FRandRange(.7f,1.4f),1));}
 for(const FVector2D Patch:{FVector2D(-3690,6500),FVector2D(-1190,12970),FVector2D(-3850,8420)})for(int I=0;I<20;++I){float X=Patch.X+R.FRandRange(-110,110),Y=Patch.Y+R.FRandRange(-160,160);if(TrailDistance(X,Y)<175)continue;S.Flowers.Add(GroundPlacement(X,Y,R.FRandRange(0,360),R.FRandRange(.85f,1.2f),1));}
 for(int I=0;I<105;++I){float X=R.FRandRange(-7600,7600),Y=R.FRandRange(5400,18400);if(!Allowed(X,Y))continue;S.Rocks.Add(GroundPlacement(X,Y,R.FRandRange(0,360),R.FRandRange(.6f,1.6f),-2));}
 for(int I=0;I<2100;++I){float X=R.FRandRange(-7700,7700),Y=R.FRandRange(4800,18700);if(ClearingDistance(X,Y)<430)continue;S.Leaves.Add(GroundPlacement(X,Y,R.FRandRange(0,360),R.FRandRange(.8f,1.5f),1));}
 // Separate seed preserves every existing log, flower and rock placement.
 FRandomStream Undergrowth(104127);
 auto PlantAllowed=[](float X,float Y,float Margin){return TrailDistance(X,Y)>Margin&&ClearingDistance(X,Y)>620;};
 for(int I=0;I<430;++I)
 {
  const float X=Undergrowth.FRandRange(-7850,7850),Y=Undergrowth.FRandRange(4900,18900);if(!PlantAllowed(X,Y,340))continue;
  const float Size=Undergrowth.FRandRange(.7f,1.75f);auto T=GroundPlacement(X,Y,Undergrowth.FRandRange(0,360),Size,-2);
  T.SetScale3D(FVector(Size,Size,Size*Undergrowth.FRandRange(.72f,1.12f)));S.Shrubs.Add(T);
 }
 // Jittered patches leave leaf-covered breaks and keep the walking trail open.
 for(float X=-7850;X<7850;X+=94)for(float Y=4870;Y<18900;Y+=94)
 {
  const float PX=X+Undergrowth.FRandRange(-39,39),PY=Y+Undergrowth.FRandRange(-39,39);if(!PlantAllowed(PX,PY,228))continue;
  const float Patch=HallFbm(PX*.0011f,PY*.0011f,3,1079);if(Undergrowth.FRand()>FMath::Lerp(.35f,.98f,Patch))continue;
  const bool Dry=Undergrowth.FRand()<.33f;const float Size=Undergrowth.FRandRange(.82f,1.38f);
  auto T=GroundPlacement(PX,PY,Undergrowth.FRandRange(0,360),Size,-.5f);T.SetScale3D(FVector(Size,Size,Undergrowth.FRandRange(.8f,1.5f)));
  (Dry?S.GrassDry:S.GrassGreen).Add(T);
 }
 // A few clearly visible boulders and smaller companions beside the trail.
 for(const FVector2D P:{FVector2D(-3850,6270),FVector2D(-3900,9050),FVector2D(350,12530),FVector2D(-2350,14850)})
 {
  S.Rocks.Add(GroundPlacement(P.X,P.Y,Undergrowth.FRandRange(0,360),2.65f,-5));
  for(int J=0;J<4;++J){const float A=Undergrowth.FRandRange(0,2*PI),D=Undergrowth.FRandRange(120,220);const float PX=P.X+FMath::Cos(A)*D,PY=P.Y+FMath::Sin(A)*D;if(Allowed(PX,PY))S.Rocks.Add(GroundPlacement(PX,PY,Undergrowth.FRandRange(0,360),Undergrowth.FRandRange(.75f,1.3f),-3));}
 }
 // Undergrowth surrounds stones without sprouting through or hiding their surfaces.
 auto InStone=[&](const FTransform& Plant,bool Bush)
 {
  for(const FTransform& Stone:S.Rocks){const float Radius=51.f*Stone.GetScale3D().X+(Bush?115.f*Plant.GetScale3D().X:43.f*Plant.GetScale3D().X);if(FVector::DistSquared2D(Plant.GetLocation(),Stone.GetLocation())<Radius*Radius)return true;}return false;
 };
 S.Shrubs.RemoveAll([&](const FTransform& T){return InStone(T,true);});
 S.GrassGreen.RemoveAll([&](const FTransform& T){return InStone(T,false);});S.GrassDry.RemoveAll([&](const FTransform& T){return InStone(T,false);});
 return S;
}
void FallenLog(FHallMesh& M,int V,TArray<FBox>& Collision)
{
 const float R=V==1?42.f:V==2?26.f:34.f;const FName Bark(TEXT("EX_NatureBark")),Cut(TEXT("EX_WoodsCutWood"));
 M.Cylinder(Bark,FVector(-300,0,R),FVector(300,0,R),R,18,WoodUV);
 for(float Sign:{-1.f,1.f}){M.Cylinder(Cut,FVector(Sign*299,0,R),FVector(Sign*301,0,R),R*.91f,20,Plain);for(float F:{.25f,.48f,.72f})Ring(M,TEXT("EX_WoodsGrowthRing"),FVector(Sign*301.3f,0,R),FVector(Sign,0,0),R*F,.30f);}
 for(int I=0;I<4;++I){const float X=-210+I*125;Beam(M,Bark,FVector(X,0,R),FVector(X+34,I%2?90.f:-85.f,R+22),7);}
 // Thin, irregular patches follow the bark radius; no rectangular moss blocks.
 for(int I=0;I<7;++I)
 {
  const float X=-225+I*74,Half=23+I%3*6;
  for(int J=0;J<5;++J)
  {
   const float A=-.53f+J*.19f,B=A+.19f;
   auto P=[&](float Angle,int Edge){const float XX=X+(Edge?Half:-Half)+5*FMath::Sin(Angle*9+I*2.3f+Edge);return FVector(XX,FMath::Sin(Angle)*(R+.25f),R+FMath::Cos(Angle)*(R+.25f));};
   M.Quad(TEXT("EX_WoodsMoss"),P(A,0),P(A,1),P(B,1),P(B,0),FVector(0,FMath::Sin((A+B)*.5f),FMath::Cos((A+B)*.5f)),WoodUV);
  }
 }
 Collision.Add(FBox(FVector(-300,-R,0),FVector(300,R,R*2)));
}
void BranchPile(FHallMesh& M,int V)
{
 const FName Bark(TEXT("EX_NatureBark")),Cut(TEXT("EX_WoodsCutWood"));
 const FVector A(-105,-5,5),B(93,12,9),C(155,35,14);Beam(M,Bark,A,B,5+V);Beam(M,Bark,B,C,3);Beam(M,Bark,FVector(12,7,8),FVector(38,-77,14),2.8f);Beam(M,Bark,FVector(-35,0,6),FVector(-79,63,11),2);
 M.Cylinder(Cut,A-FVector(.5f,0,0),A+FVector(.2f,0,0),4.4f+V,9,Plain);
 if(V==2)Beam(M,Bark,FVector(-75,-70,3),FVector(61,49,7),3.5f);
}
void Flowers(FHallMesh& M,int V)
{
 FRandomStream R(453+V*79);const FName Petal=V==0?TEXT("EX_WoodsPetalCream"):V==1?TEXT("EX_WoodsPetalGold"):TEXT("EX_WoodsPetalPurple");
 for(int I=0;I<7;++I){FVector Base(R.FRandRange(-38,38),R.FRandRange(-38,38),0),Top=Base+FVector(R.FRandRange(-7,7),R.FRandRange(-7,7),R.FRandRange(20,49));Beam(M,TEXT("EX_WoodsStem"),Base,Top,.65f);
  for(int J=0;J<5;++J){const float A=J*2*PI/5;const FVector D(FMath::Cos(A),FMath::Sin(A),0),Side(-D.Y,D.X,0);Surface(M,Petal,Top,Top+D*5+Side*2.5f+FVector(0,0,1),Top+D*9+FVector(0,0,-.4f),Top+D*5-Side*2.5f+FVector(0,0,1));}
  M.Cylinder(TEXT("EX_WoodsPetalGold"),Top,Top+FVector(0,0,1.5f),2,7,Plain);
  const FVector P=FMath::Lerp(Base,Top,.45f);Surface(M,TEXT("EX_WoodsStem"),P,P+FVector(6,-2,2),P+FVector(13,0,3),P+FVector(6,3,2));
 }
}
void Rock(FHallMesh& M,int V,TArray<FBox>& Collision)
{
 const float Radius=V==0?39.f:V==1?51.f:30.f,HeightScale=V==1?43.f:29.f;constexpr int Sides=18,Rings=8;
 const FName Key(*FString::Printf(TEXT("EX_WoodsRock%d"),V));
 auto Point=[&](float A,float Latitude)
 {
  const float Shape=1.f+.11f*FMath::Sin(A*3+V*2.6f)+.055f*FMath::Sin(A*7+Latitude*4.1f+V);
  const float C=FMath::Cos(Latitude);return FVector(FMath::Cos(A)*Radius*C*Shape+Latitude*4,FMath::Sin(A)*Radius*C*Shape*(V==2?.79f:1.09f),HeightScale*(.333f+.667f*FMath::Sin(Latitude)));
 };
 auto NormalAt=[&](float A,float L){if(L>1.56f)return FVector::UpVector;const FVector DU=Point(A+.003f,L)-Point(A-.003f,L),DV=Point(A,L+.003f)-Point(A,L-.003f);return FVector::CrossProduct(DU,DV).GetSafeNormal();};
 for(int J=0;J<Rings;++J)for(int I=0;I<Sides;++I)
 {
  const float A=I*2*PI/Sides,B=(I+1)*2*PI/Sides,L0=-.52f+J*(HALF_PI+.52f)/Rings,L1=-.52f+(J+1)*(HALF_PI+.52f)/Rings;
  const FVector P=Point(A,L0),Q=Point(B,L0),U=Point(B,L1),T=Point(A,L1);const FVector N=(NormalAt(A,L0)+NormalAt(B,L1)).GetSafeNormal();
  const FVector Tangent=(Q-P).GetSafeNormal();M.QuadUV(Key,P,Q,U,T,N,{float(I)/Sides*2,float(J)/Rings},{float(I+1)/Sides*2,float(J)/Rings},{float(I+1)/Sides*2,float(J+1)/Rings},{float(I)/Sides*2,float(J+1)/Rings},Tangent);
  auto& Section=M.Sec(Key);const int End=Section.N.Num();Section.N[End-4]=NormalAt(A,L0);Section.N[End-3]=NormalAt(B,L0);Section.N[End-2]=NormalAt(B,L1);Section.N[End-1]=NormalAt(A,L1);
 }
 Collision.Add(FBox(FVector(-Radius*.76f,-Radius*.76f,0),FVector(Radius*.76f,Radius*.76f,HeightScale*.86f)));
}
void ThickGrass(FHallMesh& M,bool Dry,int V)
{
 FRandomStream R(6311+V*71+(Dry?19:0));const FName Key=Dry?TEXT("EX_NatureDryGrass"):TEXT("EX_NatureGrass");
 for(int J=0;J<40;++J)
 {
  const float Angle=R.FRandRange(0,2*PI),Length=R.FRandRange(Dry?37.f:28.f,Dry?77.f:63.f),Width=R.FRandRange(.65f,1.25f);
  const FVector P(R.FRandRange(-42,42),R.FRandRange(-42,42),.05f),U(FMath::Cos(Angle)*Width,FMath::Sin(Angle)*Width,0);
  const FVector Lean(FMath::Cos(Angle+HALF_PI)*R.FRandRange(6,18),FMath::Sin(Angle+HALF_PI)*R.FRandRange(6,18),0);
  const FVector Mid=P+Lean*.33f+FVector(0,0,Length*.55f),Tip=P+Lean+FVector(0,0,Length);
  const FVector N=(FVector(-FMath::Sin(Angle),FMath::Cos(Angle),0)*.4f+FVector::UpVector).GetSafeNormal(),Tangent=(U-N*FVector::DotProduct(U,N)).GetSafeNormal();
  M.QuadUV(Key,P-U,P+U,Mid+U*.55f,Mid-U*.55f,N,{0,1},{1,1},{1,.45f},{0,.45f},Tangent);
  M.QuadUV(Key,Mid-U*.55f,Mid+U*.55f,Tip,Tip,N,{0,.45f},{1,.45f},{.5f,0},{.5f,0},Tangent);
 }
}
}
