#include "HouseSurroundingsPolish.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorCampus.h"
#include "HouseCalderTerrain.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseExteriorSiteLayout.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseSurroundings
{
namespace
{
const FHallUV UV=FHallUV::World(100);
const FName Metal(TEXT("EX_Metal")),Dark(TEXT("EX_Dark")),Trim(TEXT("EX_Trim")),Stone(TEXT("EX_DetailStone"));
void Box(FHallMesh& M,FName K,FVector A,FVector B){M.BoxMinMax(K,A,B,UV);}
void Rail(FHallMesh& M,FName K,FVector A,FVector B,float R=2.f){M.Cylinder(K,A,B,R,8,UV);}
void Texture(UHouseBlackHallSubsystem* L,FName Key,const FHallImage& Image,const TArray<float>& Height,float Rough,float Strength,float Metalness=0)
{
 FHallImage Normal;HallHeightToNormal(Height,Image.W,Image.H,Strength,true,Normal);
 const FString Name=Key.ToString();auto* B=HallMakeTexture(Image,true,false,*Name);auto* N=HallMakeTexture(Normal,false,true,*(Name+TEXT("_Normal")));
 L->Textures.Add(Key,B);L->Textures.Add(FName(*(Name+TEXT("_Normal"))),N);HallMakeTextured(L,Key,B,N,Rough,FLinearColor::White,1,Metalness);
}
void SignLook(UHouseBlackHallSubsystem* L,FName Key,const FString& Text,FLinearColor Colour,int W=1024,int H=128)
{
 FHallImage Image;Image.Init(W,H,Colour);if(Key!=FName(TEXT("EX_StopSign"))){Image.Rect({3,3},{float(W-3),float(H-3)},FLinearColor(.70f,.74f,.67f));Image.Rect({7,7},{float(W-7),float(H-7)},Colour);}
 FHallTextStyle S;S.Height=H*.47f;S.Stroke=H*.055f;S.Spacing=.17f;S.WidthScale=FMath::Min(1.f,(W-36)/HallTextWidth(Text,S));
 HallDrawText(Image,Text,{(W-HallTextWidth(Text,S))*.5f,H*.73f},S,FLinearColor(.86f,.87f,.79f));
 auto* T=HallMakeTexture(Image,true,false,*Key.ToString(),false);L->Textures.Add(Key,T);HallMakeTextured(L,Key,T,nullptr,.5f);
}
void Sign(FHallMesh& M,FName K,FVector C,FVector Right,FVector Up,float W,float H,FVector N)
{
 M.QuadUV(K,C-Right*W*.5f-Up*H*.5f,C+Right*W*.5f-Up*H*.5f,C+Right*W*.5f+Up*H*.5f,C-Right*W*.5f+Up*H*.5f,N,{0,1},{1,1},{1,0},{0,0},Right);
}
// The site grade plus the Calder relief (exactly 0 in the frozen zones, where every caller here is today).
float Grade(float X,float Y){return (Y<=0?-60.f:Y>=1600?-362.f:-60.f-302.f*Y/1600)+HouseCalderTerrain::ReliefLattice(X,Y);}
void GroundLine(FHallMesh& M,FName K,FVector A,FVector B,float W)
{
 const FVector Side=FVector::CrossProduct((B-A).GetSafeNormal(),FVector::UpVector)*W*.5f;
 M.Quad(K,A-Side,B-Side,B+Side,A+Side,FVector::UpVector,UV);
}
void Bench(FHallMesh& M,FVector C)
{
 for(int I=0;I<5;++I)Box(M,TEXT("EX_Wood"),C+FVector(-110,-28+I*11,42),C+FVector(110,-19+I*11,47));
 for(int I=0;I<4;++I)Box(M,TEXT("EX_Wood"),C+FVector(-110,27,53+I*11),C+FVector(110,33,61+I*11));
 for(float X:{-86.f,86.f}){Rail(M,Metal,C+FVector(X,-22,0),C+FVector(X,-22,44),3);Rail(M,Metal,C+FVector(X,25,0),C+FVector(X,25,90),3);Rail(M,Metal,C+FVector(X,-25,67),C+FVector(X,30,67),3);}
}
void Bin(FHallMesh& M,FVector C)
{
 M.Cylinder(Dark,C,C+FVector(0,0,86),29,12,UV);M.Cylinder(Metal,C+FVector(0,0,82),C+FVector(0,0,87),32,12,UV);
 for(int I=0;I<12;++I){float A=I*PI/6;const FVector D(FMath::Cos(A)*30,FMath::Sin(A)*30,0);Rail(M,Metal,C+D+FVector(0,0,6),C+D+FVector(0,0,80),1.2f);}
 M.Cylinder(Dark,C+FVector(0,0,88),C+FVector(0,0,89),20,12,UV);
}
}
bool Enabled(){return !FParse::Param(FCommandLine::Get(),TEXT("NoSurroundingsPolish"));}
void CreateLook(UHouseBlackHallSubsystem* L)
{
 if(!Enabled())return;
 constexpr int N=512;FHallImage Image;TArray<float> Height;Height.SetNum(N*N);
 for(int Kind=0;Kind<5;++Kind)
 {
  Image.Init(N,N,FLinearColor::Black);
  for(int Y=0;Y<N;++Y)for(int X=0;X<N;++X)
  {
   const float Grain=HallNoise(X*1.37f,Y*1.91f,213+Kind*17),Cloud=HallFbm(X*.018f,Y*.018f,3,71+Kind);
   float V=0,H=Grain*.08f;FLinearColor C;
   if(Kind==0){V=.070f+(Grain-.5f)*.033f+(Cloud-.5f)*.018f;C=FLinearColor(V*.97f,V,V*1.02f);H=Grain*.21f;}
   else if(Kind==1){V=.37f+(Grain-.5f)*.08f+(Cloud-.5f)*.055f;C=FLinearColor(V,V*.97f,V*.90f);H=Grain*.10f;}
   else if(Kind==2){V=.48f+(Grain-.5f)*.08f+(Cloud-.5f)*.07f;C=FLinearColor(V,V*.93f,V*.82f);const float Seam=FMath::Min(float(Y%128),float(128-Y%128));if(Seam<3){C*=.63f;H=-.13f;}}
   else if(Kind==3){const float Row=Y/64,Shift=int(Row)%2?32:0;const float Joint=FMath::Min(float(Y%64),float((X+int(Shift))%128));V=.065f+Cloud*.055f+Grain*.025f;if(Joint<3)V*=.38f;C=FLinearColor(V*.96f,V,V*1.05f);H=Joint<3?-.3f:Grain*.18f;}
   else{V=.53f+(Grain-.5f)*.055f+(Cloud-.5f)*.045f;C=FLinearColor(V,V*.97f,V*.90f);if(Y%64<4){C*=.55f;H=-.3f;}}
   Image.At(X,Y)=C;Height[Y*N+X]=H;
  }
  const TCHAR* Keys[]={TEXT("EX_StreetAsphalt"),TEXT("EX_StreetConcrete"),TEXT("EX_DetailStone"),TEXT("EX_FratRoof"),TEXT("EX_FratSiding")};
  Texture(L,Keys[Kind],Image,Height,Kind==0?.90f:.83f,Kind==0?2.4f:1.7f);
 }
 HallMakePlain(L,TEXT("EX_RoadRepair"),FLinearColor(.046f,.050f,.054f),.94f);
 HallMakePlain(L,TEXT("EX_RoadCrack"),FLinearColor(.019f,.017f,.014f),.95f);
 HallMakePlain(L,TEXT("EX_RoadOil"),FLinearColor(.028f,.026f,.023f),.5f);
 HallMakePlain(L,TEXT("EX_CurbJoint"),FLinearColor(.14f,.13f,.115f),.94f);
 HallMakePlain(L,TEXT("EX_PotSoil"),FLinearColor(.064f,.043f,.022f),1);
 HallMakePlain(L,TEXT("EX_FlowerPetal"),FLinearColor(.43f,.15f,.20f),.86f);
 HallMakePlain(L,TEXT("EX_PotClay"),FLinearColor(.29f,.13f,.067f),.87f);
 SignLook(L,TEXT("EX_StreetNameFront"),TEXT("FRATERNITY PARK DR"),FLinearColor(.012f,.09f,.045f));
 // Under the campus plan (HouseCampusLayout) the side street is Calder Dr, the main entrance to the campus.
 SignLook(L,TEXT("EX_StreetNameSide"),HouseCampusLayout::Enabled()?TEXT("CALDER DR"):TEXT("PAT HEAD SUMMITT ST"),FLinearColor(.012f,.09f,.045f));
 SignLook(L,TEXT("EX_CampusLibrarySign"),TEXT("UNIVERSITY LIBRARY"),FLinearColor(.031f,.045f,.050f));
 SignLook(L,TEXT("EX_CampusHallSign"),TEXT("UNIVERSITY HALL"),FLinearColor(.031f,.045f,.050f));
 SignLook(L,TEXT("EX_CampusScienceSign"),TEXT("SCIENCE CENTER"),FLinearColor(.031f,.045f,.050f));
 SignLook(L,TEXT("EX_BusStopSign"),TEXT("CAMPUS SHUTTLE"),FLinearColor(.18f,.058f,.013f),512,128);
 SignLook(L,TEXT("EX_StopSign"),TEXT("STOP"),FLinearColor(.43f,.013f,.009f),256,256);
 L->Materials.Add(TEXT("EX_CampusLimestone"),L->Mat(TEXT("EX_DetailStone")));
 L->Materials.Add(TEXT("EX_CampusWarmBrick"),L->Mat(TEXT("EX_BrickRed")));
}
void Beam(FHallMesh& M,FName K,FVector A,FVector B,float Width,float Depth)
{
 const FVector D=B-A;M.Box(K,FTransform(FRotationMatrix::MakeFromX(D).ToQuat(),(A+B)*.5f),FVector(D.Size()*.5f,Width*.5f,Depth*.5f),UV);
}
void Window(FHallMesh& M,FVector P,FVector R,FVector N,float W,float H,bool Lit,int32 Room)
{
 const FVector Up=FVector::UpVector;const FVector A=P-R*W*.5f,B=P+R*W*.5f,C=B+Up*H,D=A+Up*H;
 // Dark reveal, inset glazing and raised trim replace flat painted rectangles.
 M.Quad(Dark,A+N*1,B+N*1,C+N*1,D+N*1,N,UV);
 if(Room>=0)
 {
  // The interior-mapped room: u runs along cross(N, up) on every wall side, v up from the sill (M_CampusInteriorWindow).
  const FVector U=FVector::CrossProduct(N,Up);const bool bFlip=FVector::DotProduct(U,R)<0.f;const float U0=bFlip?1.f:0.f,U1=bFlip?0.f:1.f;
  M.QuadUV(FName(*FString::Printf(TEXT("CI_Room%d%s"),Room%4,Lit?TEXT("Lit"):TEXT("Dark"))),A+R*8+Up*8+N*3,B-R*8+Up*8+N*3,C-R*8-Up*8+N*3,D+R*8-Up*8+N*3,N,{U0,0},{U1,0},{U1,1},{U0,1},U);
 }
 else M.Quad(Lit?FName(TEXT("EX_Window")):FName(TEXT("EX_Glass")),A+R*8+Up*8+N*3,B-R*8+Up*8+N*3,C-R*8-Up*8+N*3,D+R*8-Up*8+N*3,N,UV);
 Beam(M,Trim,A+N*12,D+N*12,10,16);Beam(M,Trim,B+N*12,C+N*12,10,16);
 Beam(M,Trim,D+N*12,C+N*12,12,16);Beam(M,Trim,A+N*12,B+N*12,12,22);
 Beam(M,Trim,P+N*9,P+Up*H+N*9,5,8);Beam(M,Trim,P-R*(W*.5f-8)+Up*H*.5f+N*9,P+R*(W*.5f-8)+Up*H*.5f+N*9,5,8);
 Beam(M,Stone,A-R*12+N*11,B+R*12+N*11,19,36);Beam(M,Stone,D-R*13+N*9,C+R*13+N*9,16,23);
 if(Lit){for(float S:{-1.f,1.f}){const FVector Q=P+R*S*(W*.5f-20)+N*4+Up*12;M.Quad(TEXT("EX_DetailStone"),Q-R*9,Q+R*9,Q+R*9+Up*(H-24),Q-R*9+Up*(H-24),N,UV);}}
}
void RoofServices(FHallMesh& M,const FBox2D& B,float Z,int Seed,FName SignKey)
{
 const float CX=(B.Min.X+B.Max.X)*.5f;
 // Parapet cap, coping seams, drain pipes and a screened mechanical enclosure.
 for(float Y:{B.Min.Y-17,B.Max.Y+17}){Box(M,Stone,{B.Min.X-20,Y-13,Z+38},{B.Max.X+20,Y+13,Z+108});Box(M,Trim,{B.Min.X-25,Y-20,Z+105},{B.Max.X+25,Y+20,Z+114});}
 for(float X:{B.Min.X-17,B.Max.X+17}){Box(M,Stone,{X-13,B.Min.Y,Z+38},{X+13,B.Max.Y,Z+108});Box(M,Trim,{X-20,B.Min.Y-25,Z+105},{X+20,B.Max.Y+25,Z+114});}
 for(float X:{B.Min.X+85,B.Max.X-85})for(float Y:{B.Min.Y-24,B.Max.Y+24}){Rail(M,Metal,{X,Y,-40},{X,Y,Z+75},4);Rail(M,Metal,{X,Y,-40},{X,Y+FMath::Sign(Y)*20,-54},4);}
 const FVector A(CX-220,B.Min.Y+250,Z+40),E(CX+80,B.Min.Y+550,Z+175);
 for(float ZZ=A.Z+12;ZZ<E.Z-8;ZZ+=13)Box(M,Dark,{A.X-1,A.Y-2,ZZ},{E.X+1,A.Y-1,ZZ+4});
 for(float X:{CX-144,CX+4})M.Cylinder(Dark,{X,B.Min.Y+400,Z+176},{X,B.Min.Y+400,Z+179},51,12,UV);
 for(float X:{CX-110,CX+180}){M.Cylinder(Metal,{X,B.Min.Y+690,Z+38},{X,B.Min.Y+690,Z+165},17,10,UV);M.Cylinder(Dark,{X,B.Min.Y+690,Z+164},{X,B.Min.Y+690,Z+174},28,10,UV);}
 const float Y=B.Max.Y;const FVector N(0,1,0),R(1,0,0);
 Sign(M,!SignKey.IsNone()?SignKey:Seed%3==0?FName(TEXT("EX_CampusLibrarySign")):Seed%3==1?FName(TEXT("EX_CampusHallSign")):FName(TEXT("EX_CampusScienceSign")),{CX,Y+99,340},R,FVector::UpVector,580,66,N);
 for(float X:{CX-165,CX+165})Rail(M,Metal,{X,Y+101,-5},{X,Y+101,80},3);
 for(float X:{CX-520,CX+520}){if(!HouseCampus::EntranceStandClear(Seed,X-75,X+75,Y+220,Y+380))continue;   // (no planter on a Calder stair: HouseExteriorCampus)
  Box(M,TEXT("EX_PotSoil"),{X-68,Y+227,8},{X+68,Y+373,10});for(int I=0;I<7;++I){const float XX=X-50+I*16;Rail(M,TEXT("EX_Hedge"),{XX,Y+290,10},{XX+8,Y+298,110},4);M.Dome(TEXT("EX_Hedge"),{XX,Y+298,80},FVector::UpVector,20,39,8);}}
}
void Street(FHallMesh& M)
{
 if(!Enabled())return;
 // Decoration never owns road collision. Repairs lie flush above existing paving.
 FRandomStream R(12102026);
 for(int I=0;I<44;++I)
 {
  const float X=R.FRandRange(-19400,7600),Y=R.FRandRange(-5450,-4250);const float W=R.FRandRange(80,260),D=R.FRandRange(65,180);
  const FVector P(X,Y,-71.7f);M.Box(TEXT("EX_RoadRepair"),FTransform(FRotator(0,R.FRandRange(-8,8),0),P),{W*.5f,D*.5f,.08f},UV);
  FVector Last=P+FVector(-W*.6f,-D*.6f,.3f);
  for(int J=1;J<8;++J){const FVector Q=P+FVector(-W*.6f+W*J*.18f,-D*.6f+D*J*.15f+R.FRandRange(-18,18),.3f);GroundLine(M,TEXT("EX_RoadCrack"),Last,Q,R.FRandRange(.9f,1.7f));Last=Q;}
 }
 for(float X=-19400;X<8400;X+=150)
 {
  const bool bFarGap=HouseCampusLayout::Enabled()&&X>-5350.f&&X<-4350.f;   // the far sidewalk ramps down to the visitor drive here
  for(float Y:{-4025.f,-5675.f})if(Y>-5000.f||!bFarGap)GroundLine(M,TEXT("EX_CurbJoint"),{X,Y-123,-56.7f},{X,Y+123,-56.7f},.65f);
  for(float Y:{-4149.f,-5551.f})if(Y>-5000.f||!bFarGap)GroundLine(M,TEXT("EX_CurbJoint"),{X,Y-5,-56.7f},{X,Y+5,-56.7f},1.2f);
 }
 for(float Y=-5900;Y<4500;Y+=170)for(float X:{8575.f,10425.f})GroundLine(M,TEXT("EX_CurbJoint"),{X-124,Y,Grade(X-124,Y)+3.3f},{X+124,Y,Grade(X+124,Y)+3.3f},.65f);
 // Iron grates and service covers, complete with flush frames and slots.
 for(float X:{-8100.f,-4200.f,1200.f,7200.f})
 {
  Box(M,Metal,{X-45,-5530,-71.3f},{X+45,-5460,-70.7f});
  for(float XX=X-38;XX<X+39;XX+=11)Box(M,Dark,{XX,-5523,-70.6f},{XX+5,-5467,-70.4f});
  M.Cylinder(Metal,{X+800,-4530,-71.4f},{X+800,-4530,-70.8f},34,24,UV);
  for(float Offset=-20;Offset<26;Offset+=10)GroundLine(M,TEXT("EX_RoadCrack"),{X+778,-4530+Offset,-70.6f},{X+822,-4530+Offset,-70.6f},1.2f);
 }
 for(float X:{-6750.f,-5750.f,-4750.f,4100.f,5100.f})
 {
  const float Y=X<0?-2180:4100,Z=X<0?-57.6f:-359.6f;M.Dome(TEXT("EX_RoadOil"),{X,Y,Z},FVector::UpVector,34,.08f,12);
 }
 // Crossing and stop bar at the junction, with useful street names.
 for(float Y=-5490;Y<-4220;Y+=118)Box(M,TEXT("EX_Line"),{8320,Y,-71.2f},{8510,Y+60,-71.f});
 // The junction's stop bar. Fraternity Park Dr ends here at the side street, so it is the road that stops; its traffic
 // comes in eastbound, keeping to +Y, so the bar runs from the centre line to the near kerb, in line with the STOP sign.
 Box(M,TEXT("EX_Line"),{8150,-4800,-71.2f},{8175,-4220,-71.f});
 Rail(M,Metal,{8080,-3810,-60},{8080,-3810,265},4);
 const FVector N(0,-1,0);const TArray<FVector2D> Oct={{-33,-80},{33,-80},{80,-33},{80,33},{33,80},{-33,80},{-80,33},{-80,-33}};
 M.Prism(Trim,Oct,{8080,-3818,170},{1,0,0},{0,0,1},N,3,UV);
 // Clip the atlas to an octagon rather than putting a square sign over its rim.
 {
  auto& S=M.Sec(TEXT("EX_StopSign"));const int Base=S.V.Num();const FVector Centre(8080,-3822,170);
  S.V.Add(Centre);S.N.Add(N);S.UV.Add({.5f,.5f});S.T.Add(FProcMeshTangent(FVector(-1,0,0),false));
  for(const auto P:Oct){const FVector2D Q=P*.945f;S.V.Add(Centre+FVector(-Q.X,0,Q.Y));S.N.Add(N);S.UV.Add({float(.5f+Q.X/152),float(.5f-Q.Y/152)});S.T.Add(FProcMeshTangent(FVector(-1,0,0),false));}
  for(int I=0;I<8;++I){const int A=Base+1+I,B=Base+1+(I+1)%8;if(FVector::DotProduct(FVector::CrossProduct(S.V[A]-Centre,S.V[B]-Centre),N)<0)S.I.Append({Base,A,B});else S.I.Append({Base,B,A});}
 }
 // Street-name blades. Under the campus plan (HouseCampusLayout) they sit on the pole top the way real blades do: each runs
 // along the street it names and is lettered on both sides, so CALDER DR reads from Fraternity Park Dr both ways and
 // FRATERNITY PARK DR from Calder Dr and the side street north of the junction. -NoCampusLayout keeps the old faces.
 if(HouseCampusLayout::Enabled())
 {
  Box(M,Metal,{8075,-3815,259},{8085,-3805,267});Box(M,Metal,{8075,-3815,303},{8085,-3805,312});   // post cap, cross fitting
  Box(M,Metal,{7920,-3811,265},{8240,-3809,305});Box(M,Metal,{8079,-3970,310},{8081,-3650,350});   // the blades' edges
  for(float S:{-1.f,1.f})
  {
   Sign(M,TEXT("EX_StreetNameFront"),{8080,-3810+S*1.2f,285},{S,0,0},FVector::UpVector,320,40,{0,S,0});
   Sign(M,TEXT("EX_StreetNameSide"),{8080+S*1.2f,-3810,330},{0,-S,0},FVector::UpVector,320,40,{S,0,0});
  }
 }
 else
 {
  Sign(M,TEXT("EX_StreetNameFront"),{8080,-3826,285},{-1,0,0},FVector::UpVector,320,40,N);
  Sign(M,TEXT("EX_StreetNameSide"),{8080,-3827,330},{-1,0,0},FVector::UpVector,320,40,N);
 }
 // A compact shuttle shelter on a dedicated off-sidewalk pad.
 // frozen edge: stays literal (the pad straddles the property edge at Y -5900, so the Calder relief must stay 0 under it).
 Box(M,TEXT("EX_StreetConcrete"),{-3210,-6350,-90},{-2490,-5810,-55});
 for(float X:{-3160.f,-2540.f})for(float Y:{-6260.f,-5870.f})Rail(M,Metal,{X,Y,-55},{X,Y,230},4);
 Box(M,Dark,{-3210,-6340,230},{-2490,-5810,244});
 for(int J=0;J<3;++J){const float X=-3148+J*206;M.Quad(TEXT("EX_Glass"),{X,-6266,-20},{X+190,-6266,-20},{X+190,-6266,210},{X,-6266,210},{0,1,0},UV);}
 Bench(M,{-2850,-6170,-55});Bin(M,{-3370,-6130,-60});
 Sign(M,TEXT("EX_BusStopSign"),{-2850,-5812,210},{1,0,0},FVector::UpVector,400,75,{0,1,0});
 // The first pair stands on the campus side of the property edge, on the ground there; the second is in the frozen lot.
 for(const FVector P:{FVector(-1200,-6070,HouseExteriorSite::GroundHeight(-1200,-6070)),FVector(7650,-3720,-60)}){Bench(M,P);Bin(M,P+FVector(185,0,0));}
 for(float X=-6950;X<-6590;X+=82){Rail(M,Metal,{X,-2760,-60},{X,-2760,18},3);Rail(M,Metal,{X+40,-2760,-60},{X+40,-2760,18},3);Rail(M,Metal,{X,-2760,18},{X+40,-2760,18},3);}
 UE_LOG(LogTemp,Display,TEXT("SURROUNDINGS_STREET repairs=44 drains=4 crossing=1 shelter=1 extraRealLights=0"));
}
void Garden(TArray<FTransform>& Shrubs)
{
 if(!Enabled())return;FRandomStream R(1928);
 const float Widths[]={2500,2800,2450},Depths[]={2200,2100,2250};int House=0;
 for(const FVector C:{FVector(-10100,-1500,-60),FVector(-13900,-1480,-60),FVector(-17400,-1420,-60)})
 {
  for(float S:{-1.f,1.f})Shrubs.Add(FTransform(FRotator(0,R.FRand()*360,0),C+FVector(S*(Widths[House]*.35f+90),-Depths[House]*.5f-535,154),FVector(.29f)));
  ++House;
  for(float S:{-1.f,1.f})for(int I=0;I<7;++I){const float Scale=R.FRandRange(.52f,.73f);Shrubs.Add(FTransform(FRotator(0,R.FRand()*360,0),C+FVector(S*(600+I*64),-1450+R.FRandRange(-20,20),0),FVector(Scale,Scale,Scale*.8f)));}
 }
}
}
