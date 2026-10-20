#include "HouseParkingLot.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLook.h"
#include "HouseSurroundingsPolish.h"

namespace HouseParking
{
namespace
{
const FHallUV UV=FHallUV::World(180);
const FName Concrete(TEXT("EX_StreetConcrete")),White(TEXT("EX_Line")),Yellow(TEXT("EX_Yellow")),Metal(TEXT("EX_Metal")),Dark(TEXT("EX_Dark"));
struct FBay{float A,B;bool Upper,Accessible;};
TArray<FBay> Bays()
{
 TArray<FBay> B;
 for(int I=0;I<6;++I)B.Add({-6860+I*330.f,-6530+I*330.f,true,false});
 B.Add({-4880,-4480,true,true});B.Add({-4160,-3760,true,true});
 for(int I=0;I<3;++I)B.Add({-6860+I*330.f,-6530+I*330.f,false,false});
 B.Add({-5180,-4880,false,false});
 for(int I=0;I<3;++I)B.Add({-4880+I*370.f,-4510+I*370.f,false,false});
 return B;
}
void Box(FHallMesh& M,FName K,FVector A,FVector B){M.BoxMinMax(K,A,B,UV);}
void Paint(FHallMesh& M,FName K,FVector2D A,FVector2D B,float Z=Surface+.3f)
{M.Quad(K,{A.X,A.Y,Z},{B.X,A.Y,Z},{B.X,B.Y,Z},{A.X,B.Y,Z},FVector::UpVector,UV);}
void Stripe(FHallMesh& M,FName K,FVector2D A,FVector2D B,float W)
{const FVector2D D=(B-A).GetSafeNormal(),N(-D.Y,D.X);const FVector P(A,Surface+.35f),Q(B,Surface+.35f),Side(N*W*.5f,0);M.Quad(K,P-Side,Q-Side,Q+Side,P+Side,FVector::UpVector,UV);}
void Curb(FHallMesh& M,float X0,float X1,float Y0,float Y1)
{
 constexpr float Top=Surface+15,Bevel=1.5f;
 Box(M,Concrete,{X0,Y0,Surface-10},{X1,Y1,Top-Bevel});
 M.Quad(Concrete,{X0+Bevel,Y0+Bevel,Top},{X1-Bevel,Y0+Bevel,Top},{X1-Bevel,Y1-Bevel,Top},{X0+Bevel,Y1-Bevel,Top},FVector::UpVector,UV);
 M.Quad(Concrete,{X0,Y0,Top-Bevel},{X1,Y0,Top-Bevel},{X1-Bevel,Y0+Bevel,Top},{X0+Bevel,Y0+Bevel,Top},{0,-.707f,.707f},UV);
 M.Quad(Concrete,{X1,Y1,Top-Bevel},{X0,Y1,Top-Bevel},{X0+Bevel,Y1-Bevel,Top},{X1-Bevel,Y1-Bevel,Top},{0,.707f,.707f},UV);
 M.Quad(Concrete,{X0,Y1,Top-Bevel},{X0,Y0,Top-Bevel},{X0+Bevel,Y0+Bevel,Top},{X0+Bevel,Y1-Bevel,Top},{-.707f,0,.707f},UV);
 M.Quad(Concrete,{X1,Y0,Top-Bevel},{X1,Y1,Top-Bevel},{X1-Bevel,Y1-Bevel,Top},{X1-Bevel,Y0+Bevel,Top},{.707f,0,.707f},UV);
}
void ImageMaterial(UHouseBlackHallSubsystem* L,FName K,FHallImage& Image)
{auto* T=HallMakeTexture(Image,true,false,*K.ToString(),false);L->Textures.Add(K,T);HallMakeTextured(L,K,T,nullptr,.79f);}
void Wheelchair(FHallImage& Image,FVector2D C,float Scale,FLinearColor Colour)
{
 auto P=[&](float X,float Y){return C+FVector2D(X,Y)*Scale;};
 Image.Ellipse(P(0,10),{33*Scale,33*Scale},0,Colour);
 Image.Ellipse(P(0,10),{26*Scale,26*Scale},0,FLinearColor(.015f,.08f,.23f));
 Image.Ellipse(P(-18,-57),{9*Scale,9*Scale},0,Colour);
 Image.Stroke({P(-18,-42),P(-17,5),P(23,5),P(36,37),P(52,32)},7*Scale,Colour);
 Image.Stroke({P(-17,-25),P(25,-25)},7*Scale,Colour);
}
void Label(FHallImage& I,const FString& Text,float Baseline,float Size,FLinearColor Colour)
{FHallTextStyle S;S.Height=Size;S.Stroke=Size*.1f;S.Spacing=.12f;S.WidthScale=FMath::Min(1.f,(I.W-26)/HallTextWidth(Text,S));HallDrawText(I,Text,{(I.W-HallTextWidth(Text,S))*.5f,Baseline},S,Colour);}
void Placard(FHallMesh& M,FName K,FVector C,float W,float H)
{
 const FVector R(-1,0,0),U(0,0,1),N(0,-1,0);
 M.QuadUV(K,C-R*W*.5f-U*H*.5f,C+R*W*.5f-U*H*.5f,C+R*W*.5f+U*H*.5f,C-R*W*.5f+U*H*.5f,N,{0,1},{1,1},{1,0},{0,0},R);
}
void Arrow(FHallMesh& M,FVector2D C,bool AlongX,bool Positive)
{
 const FVector2D F=AlongX?FVector2D(Positive?1:-1,0):FVector2D(0,Positive?1:-1),R(-F.Y,F.X);
 const TArray<FVector2D> Shape={{-9,-70},{9,-70},{9,20},{31,20},{0,72},{-31,20},{-9,20}};
 const int Triangles[][3]={{0,1,2},{0,2,6},{3,4,5}};
 for(const auto& T:Triangles){auto Point=[&](int I){return FVector(C+R*Shape[I].X+F*Shape[I].Y,Surface+.4f);};const FVector A=Point(T[0]),B=Point(T[1]),D=Point(T[2]);auto& S=M.Sec(White);const int Base=S.V.Num();for(auto V:{A,B,D}){S.V.Add(V);S.N.Add(FVector::UpVector);S.UV.Add({V.X/180,V.Y/180});S.T.Add(FProcMeshTangent(FVector::ForwardVector,false));}if(FVector::CrossProduct(B-A,D-A).Z<0)S.I.Append({Base,Base+1,Base+2});else S.I.Append({Base,Base+2,Base+1});}
}
}
void CreateLook(UHouseBlackHallSubsystem* L)
{
 if(!L->Materials.Contains(Concrete))L->Materials.Add(Concrete,L->Mat(TEXT("EX_Concrete")));
 HallMakePlain(L,TEXT("EX_ParkingBlue"),FLinearColor(.015f,.08f,.23f),.85f);
 HallMakePlain(L,TEXT("EX_ParkingMulch"),FLinearColor(.071f,.043f,.023f),.99f);
 HallMakePlain(L,TEXT("EX_ParkingBarkChip"),FLinearColor(.15f,.087f,.041f),.97f);
 const FLinearColor Ivory(.83f,.83f,.76f),Blue(.015f,.08f,.23f);
 FHallImage I;I.Init(256,256,Blue);Wheelchair(I,{110,140},1.8f,Ivory);ImageMaterial(L,TEXT("EX_ParkingSymbol"),I);
 I.Init(384,512,Ivory);I.Rect({8,8},{376,504},Blue);I.Rect({15,15},{369,497},Ivory);
 Label(I,TEXT("RESERVED"),74,42,Blue);Label(I,TEXT("PARKING"),134,50,Blue);
 I.Rect({59,158},{325,416},Blue);Wheelchair(I,{184,299},1.9f,Ivory);Label(I,TEXT("ACCESSIBLE"),474,29,Blue);ImageMaterial(L,TEXT("EX_ParkingReserved"),I);
 I.Init(512,256,Ivory);I.Rect({8,8},{504,248},Blue);I.Rect({15,15},{497,241},Ivory);Label(I,TEXT("PARKING"),104,66,Blue);Label(I,TEXT("5 MPH"),206,67,Blue);ImageMaterial(L,TEXT("EX_ParkingEntry"),I);
}
void Build(FHallMesh& Solid,FHallMesh& Detail)
{
 // Perimeter and islands, with openings at the driveway and pedestrian aisle.
 Curb(Solid,West,West+23,South,North);Curb(Solid,East-23,East,South,North);
 Curb(Solid,West,-5800,South,South+23);Curb(Solid,-5200,East,South,South+23);
 Curb(Solid,West,AccessWest-10,North-23,North);Curb(Solid,AccessEast+10,East,North-23,North);
 Curb(Solid,-7170,-6900,-2450,-2427);Curb(Solid,-7170,-6900,-373,-350);Curb(Solid,-6923,-6900,-2450,-350);
 Box(Solid,TEXT("EX_ParkingMulch"),{-7177,-2435,Surface-8},{-6910,-365,Surface+7});
 // Flat access aisle and a direct paved connection to the existing perimeter walk.
 Box(Solid,Concrete,{AccessWest-10,-285,-85},{-3550,-105,-57});
 for(float X=AccessWest+160;X<-3550;X+=180)Paint(Detail,TEXT("EX_CurbJoint"),{X-.35f,-284},{X+.35f,-106},-56.8f);
 for(const FBay& B:Bays())
 {
  const float Y0=B.Upper?-950.f:-2470.f,Y1=B.Upper?-330.f:-1850.f,Stop=B.Upper?-440.f:-2360.f,CX=(B.A+B.B)*.5f;
  Paint(Detail,White,{B.A-2.5f,Y0},{B.A+2.5f,Y1});Paint(Detail,White,{B.B-2.5f,Y0},{B.B+2.5f,Y1});
  Box(Solid,Concrete,{CX-100,Stop-10,Surface},{CX+100,Stop+10,Surface+11});
  for(float X:{CX-72,CX+72})Detail.Cylinder(Metal,{X,Stop,Surface+11},{X,Stop,Surface+11.4f},1.5f,8,UV);
  if(B.Accessible)
  {
   Paint(Detail,TEXT("EX_ParkingBlue"),{B.A+8,-920},{B.B-8,-344});
   const FVector C(CX,-720,Surface+.6f),R(-1,0,0),U(0,1,0);
   Detail.QuadUV(TEXT("EX_ParkingSymbol"),C-R*78-U*78,C+R*78-U*78,C+R*78+U*78,C-R*78+U*78,FVector::UpVector,{0,1},{1,1},{1,0},{0,0},R);
   Solid.Cylinder(Metal,{CX,-318,-60},{CX,-318,222},3.3f,8,UV);
   Box(Detail,Metal,{CX-34,-317,144},{CX+34,-314,238});Placard(Detail,TEXT("EX_ParkingReserved"),{CX,-320,191},65,91);
  }
 }
 // Shared loading aisle. Diagonal stripes are clipped to the real rectangle.
 Paint(Detail,Yellow,{AccessWest,-940},{AccessWest+7,-300});Paint(Detail,Yellow,{AccessEast-7,-940},{AccessEast,-300});
 for(float Y=-1120;Y<-300;Y+=70)
 {const float Y0=FMath::Max(Y,-935.f),Y1=FMath::Min(Y+300,-305.f);if(Y1>Y0)Stripe(Detail,Yellow,{AccessWest+7+(Y0-Y),Y0},{AccessWest+7+(Y1-Y),Y1},6);}
 // Keep the driveway throat open rather than drawing parking spaces across it.
 Stripe(Detail,Yellow,{-5800,-2480},{-5800,-1920},5);Stripe(Detail,Yellow,{-5200,-2480},{-5200,-1920},5);
 Arrow(Detail,{-5500,-2190},false,true);Arrow(Detail,{-6100,-1400},true,false);Arrow(Detail,{-4600,-1400},true,true);
 // Flush drainage covers and small curb expansion joints.
 for(float Y:{-2050.f,-700.f}){Box(Detail,Metal,{-7125,Y-32,Surface+.2f},{-7060,Y+32,Surface+.6f});for(float X=-7118;X<-7063;X+=10)Box(Detail,Dark,{X,Y-26,Surface+.7f},{X+5,Y+26,Surface+.85f});}
 for(float X=-7140;X<East;X+=160)for(float Y:{South+8,North-8})
 {if(Y>North-10&&X>AccessWest-15&&X<AccessEast+15)continue;if(Y<South+10&&X>-5815&&X<-5185)continue;Paint(Detail,TEXT("EX_CurbJoint"),{X-.4f,Y-8},{X+.4f,Y+8},Surface+15.05f);}
 // Mulch and shrubs outside the bay edges, clear of the driveway and walkway.
 Box(Solid,TEXT("EX_ParkingMulch"),{-7160,-2720,-72},{-5950,-2540,-55});Box(Solid,TEXT("EX_ParkingMulch"),{-5010,-2720,-72},{-3750,-2540,-55});
 Box(Solid,TEXT("EX_ParkingMulch"),{-7150,-280,-72},{AccessWest-35,-110,-55});
 FRandomStream R(12874);for(int I=0;I<240;++I){const bool WestBed=I%3==0;const float X=WestBed?R.FRandRange(-7150,-6940):R.FRandRange(-7100,-4540),Y=WestBed?R.FRandRange(-2370,-430):R.FRandRange(-250,-125);Detail.Box(TEXT("EX_ParkingBarkChip"),FTransform(FRotator(0,R.FRand()*360,0),{X,Y,Surface+7.5f}),{R.FRandRange(1,3.5f),R.FRandRange(.7f,1.8f),.5f},UV);}
 Solid.Cylinder(Metal,{-5090,-2700,-60},{-5090,-2700,190},3.4f,8,UV);Placard(Detail,TEXT("EX_ParkingEntry"),{-5090,-2705,157},104,52);
 UE_LOG(LogTemp,Display,TEXT("PARKING_LOT_LAYOUT bays=15 accessible=2 loadingAisle=320cm driveway=600cm centerPoles=0 curbAndHouseWalk=1"));
}
void ParkCars(TArray<FTransform>& RedCars,TArray<FTransform>& WhiteCars,TArray<FTransform>& BlueCars)
{
 const auto B=Bays();const int Occupancy[]={0,8,2,10,4,12,9,14,1,3,5,11,13};int I=0;
 for(auto* List:{&RedCars,&WhiteCars,&BlueCars})for(FTransform& T:*List)
 {
  const FVector Old=T.GetLocation();if(Old.X<West||Old.X>East||Old.Y<South||Old.Y>North)continue;
  const FBay& Bay=B[Occupancy[I++%UE_ARRAY_COUNT(Occupancy)]];T.SetLocation({(Bay.A+Bay.B)*.5f,Bay.Upper?-640.f:-2160.f,-60});T.SetRotation(FRotator(0,Bay.Upper?90:-90,0).Quaternion());
 }
 UE_LOG(LogTemp,Display,TEXT("PARKING_LOT_VEHICLES remapped=%d accessibleAndDriveAislesEmpty=1 otherCarsUnchanged=1"),I);
}
void Landscaping(TArray<FTransform>& Existing,TArray<FTransform>& NewPlants)
{
 // Retain the independent site RNG; only move bushes that cross the new path.
 Existing.RemoveAll([](const FTransform& T){const FVector P=T.GetLocation();return P.X>AccessWest-60&&P.X<-3370&&P.Y>-460&&P.Y<60;});
 FRandomStream R(4412);
 auto Plant=[&](float X,float Y,float S){NewPlants.Add(FTransform(FRotator(0,R.FRand()*360,0),{X,Y,-55},FVector(S,S,S*.78f)));};
 for(float Y=-2290;Y<-420;Y+=170)Plant(-7055,Y,R.FRandRange(.40f,.52f));
 for(float X=-7000;X<AccessWest-160;X+=180)Plant(X,-205,R.FRandRange(.38f,.49f));
 for(float X=-7000;X<-5980;X+=190)Plant(X,-2630,.43f);
 for(float X=-4930;X<-3870;X+=190)Plant(X,-2630,.43f);
}
}
