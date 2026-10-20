#include "HouseExteriorCampus.h"
#include "HouseStadiumDetail.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHall.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "HouseSurroundingsPolish.h"
#include "HouseCampusFall.h"
#include "HouseCampusLayout.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusPolish.h"
#include "HouseCampusFallKit.h"
#include "HouseCalderFarLand.h"
#include "HouseCalderOuterLand.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderFeatures.h"
#include "HouseExteriorSiteLayout.h"

namespace HouseCampus
{
namespace
{
constexpr float Ground=-60.f,Rear=-362.f;
const FName Brick(TEXT("EX_Brick")),Stone(TEXT("EX_Stone")),Trim(TEXT("EX_Trim")),Glass(TEXT("EX_Glass")),Dark(TEXT("EX_Dark"));
const FName Concrete(TEXT("EX_Concrete")),Metal(TEXT("EX_Metal")),Glow(TEXT("EX_Window")),Orange(TEXT("EX_Orange"));
const FHallUV Plain=FHallUV::World(100),BrickUV=FHallUV::World(100),StoneUV=FHallUV::World(180);
void Box(FHallMesh& M,FName K,float X,float XX,float Y,float YY,float Z,float ZZ)
{ M.BoxMinMax(K,FVector(X,Y,Z),FVector(XX,YY,ZZ),K==Brick?BrickUV:Plain); }
void Panel(FHallMesh& M,FName K,FVector A,FVector B,float Bottom,float Top,FVector N)
{M.Quad(K,A+FVector(0,0,Bottom),B+FVector(0,0,Bottom),B+FVector(0,0,Top),A+FVector(0,0,Top),N,Plain);}
void Windows(FHallMesh& M,const FBox2D& B,int Floors,float Height,int Seed)
{
 const float Z0=Ground+90,Story=Height/Floors;
 for(int Side=0;Side<4;++Side)
 {
  FVector A,Bb,N;float Length;
  if(Side<2){const float Y=Side==0?B.Max.Y+5:B.Min.Y-5;A=FVector(B.Min.X+160,Y,0);Bb=FVector(B.Max.X-160,Y,0);N=FVector(0,Side==0?1:-1,0);}
  else{const float X=Side==2?B.Min.X-5:B.Max.X+5;A=FVector(X,B.Min.Y+160,0);Bb=FVector(X,B.Max.Y-160,0);N=FVector(Side==2?-1:1,0,0);}
  Length=(Bb-A).Size();const FVector Along=(Bb-A).GetSafeNormal();
  for(int Floor=0;Floor<Floors;++Floor)for(int I=0;I*360+220<Length;++I)
  {
   const FVector P=A+Along*(I*360),Q=P+Along*175;const float Z=Z0+Floor*Story;
   if(HouseSurroundings::Enabled())HouseSurroundings::Window(M,(P+Q)*.5f+FVector(0,0,Z),Along,N,175,212,HouseCampusFall::WindowLit(I,Floor,Seed),
    HouseCampusInteriors::RoomWindows()?HouseCampusInteriors::WindowRoom(Seed,Side,I,Floor):-1);
   else{Panel(M,Stone,P-Along*9,Q+Along*9,Z-9,Z+221,N);
    Panel(M,HouseCampusFall::WindowLit(I,Floor,Seed)?Glow:Glass,P+N*.9f,Q+N*.9f,Z,Z+212,N);
    Panel(M,Trim,P+Along*82+N*2,Q-Along*82+N*2,Z,Z+212,N);
    Panel(M,Trim,P+N*2,Q+N*2,Z+104,Z+109,N);}
  }
 }
}
/** Where an entrance's canopy post or planter (canonical box X0..X1 x Y0..Y1) stands: at Base on the apron, or, reaching past
 *  a shallower apron on the Calder terrain (the landform clips an apron where entrance steps need the room), down on the
 *  lowest ground under it, so nothing floats over the steps' landing. Today exactly Base. */
float EntranceFoot(int Seed,float Base,float ApronEnd,float X0,float X1,float Y0,float Y1)
{
 if(!HouseCalderTerrain::Enabled()||Y1<=ApronEnd)return Base;
 float Low=Base;
 for(const float X:{X0,X1})for(const float Y:{Y0,Y1})Low=FMath::Min(Low,FoundationGround(Seed,FVector2D(X,Y))-2.f);
 return Low;
}
void Building(FHallMesh& M,FLayout& Layout,const FBox2D& B,float Height,int Style,int Seed,FName SignKey=NAME_None,float Apron=550.f)
{
 ++Layout.Buildings;Layout.BuildingFootprints.Add(FBox2D(B.Min-FVector2D(150),B.Max+FVector2D(150)));
 const FName Body=Style==1?FName(TEXT("EX_CampusLimestone")):Style==2?FName(TEXT("EX_CampusWarmBrick")):Brick;
 Box(M,Body,B.Min.X,B.Max.X,B.Min.Y,B.Max.Y,Ground-70,Ground+Height);
 Box(M,Stone,B.Min.X-12,B.Max.X+12,B.Min.Y-12,B.Max.Y+12,Ground,Ground+75);
 for(float Z=Ground+440;Z<Ground+Height-50;Z+=420)Box(M,Stone,B.Min.X-8,B.Max.X+8,B.Min.Y-8,B.Max.Y+8,Z,Z+13);
 Box(M,Stone,B.Min.X-28,B.Max.X+28,B.Min.Y-28,B.Max.Y+28,Ground+Height-40,Ground+Height+12);
 Box(M,Dark,B.Min.X+45,B.Max.X-45,B.Min.Y+45,B.Max.Y-45,Ground+Height+12,Ground+Height+38);
 for(float X=B.Min.X+65;X<B.Max.X;X+=1200)for(float Y:{B.Min.Y-16,B.Max.Y+16})Box(M,Stone,X-22,X+22,Y-13,Y+13,Ground+75,Ground+Height-41);
 const int Floors=FMath::Clamp(FMath::FloorToInt(Height/420),2,7);Windows(M,B,Floors,Height-50,Seed);
 const float CX=(B.Min.X+B.Max.X)*.5f,Y=B.Max.Y;
 Box(M,Stone,CX-450,CX+450,Y+1,Y+85,Ground+6,Ground+390);
 Panel(M,Glass,FVector(CX-320,Y+87,0),FVector(CX+320,Y+87,0),Ground+8,Ground+330,FVector(0,1,0));
 for(float X:{CX-325,CX,CX+325})Box(M,Metal,X-4,X+4,Y+88,Y+96,Ground+10,Ground+331);
 Box(M,Dark,CX-470,CX+470,Y+2,Y+240,Ground+337,Ground+355);
 // The apron stops short of any paving that does not lead to this door (HouseCampusLayout::ApronDepth); under the plan
 // the canopy posts and planters stand on the ground, under the apron or not (-NoCampusLayout keeps the old campus exactly).
 const float PostBase=HouseCampusLayout::Enabled()?Ground-2:Ground+4,PlanterBase=HouseCampusLayout::Enabled()?Ground-2:Ground+5;
 if(Apron>0)Box(M,Concrete,CX-520,CX+520,Y,Y+Apron,Ground+1,Ground+5);
 for(float X:{CX-365,CX+365})if(EntranceStandClear(Seed,X-9,X+9,Y+186,Y+204))M.Cylinder(Metal,FVector(X,Y+195,EntranceFoot(Seed,PostBase,Y+Apron,X-9,X+9,Y+186,Y+204)),FVector(X,Y+195,Ground+340),9,8,Plain);
 // Architectural service equipment and entrance planters add scale without more lights.
 Box(M,Metal,CX-220,CX+80,B.Min.Y+250,B.Min.Y+550,Ground+Height+38,Ground+Height+175);
 for(float X:{CX-520,CX+520})if(EntranceStandClear(Seed,X-75,X+75,Y+220,Y+380))Box(M,Stone,X-75,X+75,Y+220,Y+380,EntranceFoot(Seed,PlanterBase,Y+Apron,X-75,X+75,Y+220,Y+380),Ground+68);
 if(HouseSurroundings::Enabled())
 {
  HouseSurroundings::RoofServices(M,B,Ground+Height,Seed,SignKey);
  for(float X:{CX-290,CX+290}){Box(M,Trim,X-4,X+4,Y+96,Y+112,Ground+28,Ground+310);M.Cylinder(Metal,{X+FMath::Sign(CX-X)*34,Y+119,Ground+103},{X+FMath::Sign(CX-X)*34,Y+119,Ground+163},3,8,Plain);}
  for(float X=CX-435;X<CX+445;X+=66)Box(M,Metal,X-2,X+2,Y+6,Y+235,Ground+332,Ground+340);
  for(float X=B.Min.X+65;X<B.Max.X;X+=1200)for(float YY:{B.Min.Y-19,B.Max.Y+19})
  {Box(M,Stone,X-35,X+35,YY-14,YY+14,Ground+69,Ground+118);Box(M,Trim,X-34,X+34,YY-16,YY+16,Ground+Height-86,Ground+Height-49);}
 }
}
/** An open window's outside: the same frame, mullion, sill and head as HouseSurroundings::Window, with real glass set into
 *  the reveal instead of a painted pane (P is the bottom middle, 5 cm out from the wall face). */
void OpenWindowDecor(FHallMesh& M,FVector P,FVector R,FVector N,float W,float H)
{
 const FVector Up=FVector::UpVector;const FVector A=P-R*W*.5f,B=P+R*W*.5f,C=B+Up*H,D=A+Up*H;
 HouseSurroundings::Beam(M,Trim,A+N*12,D+N*12,10,16);HouseSurroundings::Beam(M,Trim,B+N*12,C+N*12,10,16);
 HouseSurroundings::Beam(M,Trim,D+N*12,C+N*12,12,16);HouseSurroundings::Beam(M,Trim,A+N*12,B+N*12,12,22);
 HouseSurroundings::Beam(M,Trim,P-N*17,P+Up*H-N*17,5,4);HouseSurroundings::Beam(M,Trim,P-R*(W*.5f)+Up*H*.5f-N*17,P+R*(W*.5f)+Up*H*.5f-N*17,5,4);
 HouseSurroundings::Beam(M,Stone,A-R*12+N*11,B+R*12+N*11,19,36);HouseSurroundings::Beam(M,Stone,D-R*13+N*9,C+R*13+N*9,16,23);
 const FVector G=-N*17;
 M.Quad(TEXT("CI_Glass"),A+G,B+G,C+G,D+G,N,Plain);
 M.Quad(TEXT("CI_Glass"),A+G-N*.3f,B+G-N*.3f,C+G-N*.3f,D+G-N*.3f,-N,Plain);
}
/** A walk-in building's outside (HouseCampusInteriors): Building()'s look, hollow on its open floors. The walls of the open
 *  floors are a skin with every window and door cut through, the closed floors above stay solid, and the plinth and string
 *  courses are rings round the walls instead of slabs through the rooms. The main doors get a stone portal. */
void ShellBuilding(FHallMesh& M,const FBox2D& B,float Height,int Style,int Seed,FName SignKey)
{
 const FName Body=Style==1?FName(TEXT("EX_CampusLimestone")):Style==2?FName(TEXT("EX_CampusWarmBrick")):Brick;
 const int Floors=HouseCampusInteriors::Floors(Height),Open=HouseCampusInteriors::OpenFloors(Seed);
 const float Top=HouseCampusInteriors::OpenTop(Seed);
 HouseCampusInteriors::OpenWalls(M,Body,Seed,0.f,20.f,Body==Brick?BrickUV:Plain);
 if(Top<Ground+Height)Box(M,Body,B.Min.X,B.Max.X,B.Min.Y,B.Max.Y,Top,Ground+Height);
 TArray<HouseCampusInteriors::FOpening> Doors;HouseCampusInteriors::DoorOpenings(Seed,Doors);
 // A box against side S's outer face (S 0 +Y, 1 -Y, 2 -X, 3 +X): U0..U1 along the side, D0..D1 out from the face.
 auto SideBox=[&](FName K,int S,float U0,float U1,float D0,float D1,float Z0,float Z1)
 {
  if(U1-U0<.5f)return;
  switch(S)
  {
  case 0:Box(M,K,U0,U1,B.Max.Y+D0,B.Max.Y+D1,Z0,Z1);break;
  case 1:Box(M,K,U0,U1,B.Min.Y-D1,B.Min.Y-D0,Z0,Z1);break;
  case 2:Box(M,K,B.Min.X-D1,B.Min.X-D0,U0,U1,Z0,Z1);break;
  default:Box(M,K,B.Max.X+D0,B.Max.X+D1,U0,U1,Z0,Z1);break;
  }
 };
 auto Ring=[&](float Out,float Z0,float Z1,bool bGaps)
 {
  // Every side is split round its door openings (listed in order along the side). Sides 0 and 1 run the full length
  // with the corners; 2 and 3 run between them.
  for(int S=0;S<4;++S)
  {
   float U=S<2?B.Min.X-Out:B.Min.Y;const float UEnd=S<2?B.Max.X+Out:B.Max.Y;
   if(bGaps)for(const auto& D:Doors)if(D.Side==S&&D.Z0<Z1){SideBox(Stone,S,U,D.U0,0.f,Out,Z0,Z1);U=D.U1;}
   SideBox(Stone,S,U,UEnd,0.f,Out,Z0,Z1);
  }
 };
 Ring(12,Ground,Ground+75,true);
 for(float Z=Ground+440;Z<Ground+Height-50;Z+=420)Ring(8,Z,Z+13,true);
 Box(M,Stone,B.Min.X-28,B.Max.X+28,B.Min.Y-28,B.Max.Y+28,Ground+Height-40,Ground+Height+12);
 Box(M,Dark,B.Min.X+45,B.Max.X-45,B.Min.Y+45,B.Max.Y-45,Ground+Height+12,Ground+Height+38);
 auto OnDoor=[&](float U0,float U1){for(const auto& D:Doors)if(D.Side==0&&D.U0<U1+10&&D.U1>U0-10)return true;return false;};
 for(float X=B.Min.X+65;X<B.Max.X;X+=1200)for(float Y:{B.Min.Y-16,B.Max.Y+16})if(Y<B.Min.Y||!OnDoor(X-22,X+22))Box(M,Stone,X-22,X+22,Y-13,Y+13,Ground+75,Ground+Height-41);
 // Windows: real glass on the open floors, the lit-or-dark painted windows above (the same grid and pattern as Windows()).
 for(int Side=0;Side<4;++Side)
 {
  FVector N;
  if(Side<2)N=FVector(0,Side==0?1:-1,0);else N=FVector(Side==2?-1:1,0,0);
  const FVector Along=Side<2?FVector(1,0,0):FVector(0,1,0);
  const float Plane=Side==0?B.Max.Y:Side==1?B.Min.Y:Side==2?B.Min.X:B.Max.X;
  TArray<FVector2D> Cols;HouseCampusInteriors::WindowColumns(Seed,Side,Cols);
  for(int Floor=0;Floor<Floors;++Floor)for(int I=0;I<Cols.Num();++I)
  {
   const float U=(Cols[I].X+Cols[I].Y)*.5f,Z=HouseCampusInteriors::WindowSill(Height,Floor);
   const FVector Bottom=(Side<2?FVector(U,Plane,0):FVector(Plane,U,0))+N*5+FVector(0,0,Z);
   if(Floor<Open&&HouseCampusInteriors::WindowReplaced(Seed,Side,Floor,Cols[I].X,Cols[I].Y))continue;   // a door takes its place
   // Real glass on an open floor (unless the building keeps that room closed); the painted or interior-mapped window else.
   if(Floor<Open&&HouseCampusInteriors::WindowOpen(Seed,Side,Floor,Cols[I].X,Cols[I].Y))OpenWindowDecor(M,Bottom,Along,N,175,212);
   else if(HouseSurroundings::Enabled())HouseSurroundings::Window(M,Bottom,Along,N,175,212,HouseCampusFall::WindowLit(I,Floor,Seed),
    HouseCampusInteriors::RoomWindows()?HouseCampusInteriors::WindowRoom(Seed,Side,I,Floor):-1);
   else{const FVector P=Bottom-Along*87.5f,Q=Bottom+Along*87.5f;Panel(M,HouseCampusFall::WindowLit(I,Floor,Seed)?Glow:Glass,P-N*4.1f,Q-N*4.1f,Z,Z+212,N);}
  }
 }
 // The entrance: a stone portal round the doors (the doors themselves are ACampusDoor actors), the canopy on its posts, the
 // apron and planters, and the building's carved name over the door (RoofServices).
 const float CX=(B.Min.X+B.Max.X)*.5f,Y=B.Max.Y;
 for(const auto& D:Doors)if(D.Side==0)
 {
  Box(M,Stone,D.U0-80,D.U0,Y,Y+55,Ground,D.Z1+70);Box(M,Stone,D.U1,D.U1+80,Y,Y+55,Ground,D.Z1+70);
  Box(M,Stone,D.U0-80,D.U1+80,Y,Y+55,D.Z1,D.Z1+70);Box(M,Stone,D.U0-95,D.U1+95,Y,Y+65,D.Z1+70,D.Z1+82);
 }
 // Every other door: a steel frame, a hood over it, a concrete stoop; a staff plaque beside a locked back door.
 for(const auto& D:Doors)if(D.Side!=0)
 {
  SideBox(Metal,D.Side,D.U0-9,D.U0,0,4,Ground,D.Z1+9);SideBox(Metal,D.Side,D.U1,D.U1+9,0,4,Ground,D.Z1+9);SideBox(Metal,D.Side,D.U0-9,D.U1+9,0,4,D.Z1,D.Z1+9);
  SideBox(Dark,D.Side,D.U0-45,D.U1+45,0,95,D.Z1+34,D.Z1+46);
  for(float U:{D.U0-38,D.U1+38})SideBox(Metal,D.Side,U-2,U+2,0,90,D.Z1+46,D.Z1+48);
  // The stoop (on the Calder terrain it stops short of a stair that climbs to the door: the stair's top tread is the
  // landing there).
  float Stoop=120;
  if(HouseCalderFeatures::Active())
  {
   const FTransform T=HouseCampusLayout::Frame(Seed);
   auto Canon=[&](float U,float Dist){return D.Side==0?FVector(U,B.Max.Y+Dist,0):D.Side==1?FVector(U,B.Min.Y-Dist,0):D.Side==2?FVector(B.Min.X-Dist,U,0):FVector(B.Max.X+Dist,U,0);};
   for(float Dist=2;Dist<=120&&Stoop==120;Dist+=2)for(const float U:{D.U0-70,(D.U0+D.U1)*.5f,D.U1+70})
   {const FVector W=T.TransformPosition(Canon(U,Dist));if(HouseCalderFeatures::InStair(FVector2D(W.X,W.Y))||HouseCalderFeatures::InRamp(FVector2D(W.X,W.Y))){Stoop=Dist-2;break;}}
  }
  if(Stoop>=20)SideBox(Concrete,D.Side,D.U0-70,D.U1+70,0,Stoop,Ground+1,Ground+4);
  if(D.Side==1&&D.U1-D.U0<120)HouseCampusFallKit::Card(M,TEXT("CI_SignStaff"),FVector(D.U1+75,B.Min.Y-.7f,Ground+162),FVector(-1,0,0),FVector::UpVector,70,24.5f,FVector(0,-1,0));
 }
 Box(M,Dark,CX-470,CX+470,Y+2,Y+240,Ground+337,Ground+355);
 const float Apron=HouseCampusLayout::ApronDepth(Seed,HouseCampusLayout::Blocks()[Seed].Faces[0],HouseCampusLayout::MainApronW,HouseCampusLayout::MainApronD);
 const float PostBase=Ground-2,PlanterBase=Ground-2;
 if(Apron>0)Box(M,Concrete,CX-520,CX+520,Y,Y+Apron,Ground+1,Ground+5);
 for(float X:{CX-365,CX+365})if(EntranceStandClear(Seed,X-9,X+9,Y+186,Y+204))M.Cylinder(Metal,FVector(X,Y+195,EntranceFoot(Seed,PostBase,Y+Apron,X-9,X+9,Y+186,Y+204)),FVector(X,Y+195,Ground+340),9,8,Plain);
 Box(M,Metal,CX-220,CX+80,B.Min.Y+250,B.Min.Y+550,Ground+Height+38,Ground+Height+175);
 for(float X:{CX-520,CX+520})if(EntranceStandClear(Seed,X-75,X+75,Y+220,Y+380))Box(M,Stone,X-75,X+75,Y+220,Y+380,EntranceFoot(Seed,PlanterBase,Y+Apron,X-75,X+75,Y+220,Y+380),Ground+68);
 if(HouseSurroundings::Enabled())
 {
  HouseSurroundings::RoofServices(M,B,Ground+Height,Seed,SignKey);
  for(float X=CX-435;X<CX+445;X+=66)Box(M,Metal,X-2,X+2,Y+6,Y+235,Ground+332,Ground+340);
  for(float X=B.Min.X+65;X<B.Max.X;X+=1200)for(float YY:{B.Min.Y-19,B.Max.Y+19})
  {if(YY>B.Max.Y&&OnDoor(X-35,X+35))continue;Box(M,Stone,X-35,X+35,YY-14,YY+14,Ground+69,Ground+118);Box(M,Trim,X-34,X+34,YY-16,YY+16,Ground+Height-86,Ground+Height-49);}
 }
}
/** Appends Src to Dst moved by T (positions, normals and tangents; UVs and winding are unchanged by a rotation). */
void AppendTransformed(FHallMesh& Dst,const FHallMesh& Src,const FTransform& T)
{
 for(const FName& K:Src.Order)
 {
  const FHallSection& S=Src.Sections[K];FHallSection& D=Dst.Sec(K);const int32 Base=D.V.Num();
  for(int32 I=0;I<S.V.Num();++I){D.V.Add(T.TransformPosition(S.V[I]));D.N.Add(T.TransformVectorNoScale(S.N[I]));D.UV.Add(S.UV[I]);D.T.Add(FProcMeshTangent(T.TransformVectorNoScale(S.T[I].TangentX),S.T[I].bFlipTangentY));}
  for(const int32 Idx:S.I)D.I.Add(Base+Idx);
 }
}
/** A second entrance (stone surround, glass doors, canopy on posts, pad) on face Face of building Seed (world footprint B),
 *  built facing +Y, turned and set on the building's level pad (the Z of HouseCampusLayout::Frame). */
void SideEntrance(FHallMesh& M,int32 Seed,const FBox2D& B,TCHAR Face,float Pad=420.f)
{
 FHallMesh E;
 const FVector N=HouseCampusLayout::FaceNormal(Face);
 const FVector2D C=(B.Min+B.Max)*.5f;const FVector2D H=(B.Max-B.Min)*.5f;
 // On the Calder terrain, where a stair climbs away from this door (Band and Music's west door, walk_29's stair out of its
 // court) the surround stands flush, 6 cm proud, not 70: the projecting block stood on the stair's first treads.
 const FVector2D Front(C.X+N.X*(H.X+40.f),C.Y+N.Y*(H.Y+40.f));
 const float Proud=HouseCalderFeatures::Active()&&HouseCalderFeatures::InStair(Front)?6.f:70.f;
 Box(E,Stone,-330,330,1,Proud,Ground+6,Ground+330);
 Panel(E,Glass,FVector(-240,Proud+2,0),FVector(240,Proud+2,0),Ground+8,Ground+290,FVector(0,1,0));
 for(float X:{-245.f,0.f,245.f})Box(E,Metal,X-4,X+4,Proud+3,Proud+11,Ground+10,Ground+291);
 Box(E,Dark,-360,360,2,200,Ground+297,Ground+313);
 for(float X:{-300.f,300.f})E.Cylinder(Metal,FVector(X,165,Ground-2),FVector(X,165,Ground+298),7,8,Plain);
 if(Pad>0)Box(E,Concrete,-400,400,0,Pad,Ground+1,Ground+5);
 const FVector At(C.X+N.X*H.X,C.Y+N.Y*H.Y,HouseCampusLayout::Frame(Seed).GetLocation().Z);
 AppendTransformed(M,E,FTransform(FRotator(0,HouseCampusLayout::FaceYaw(Face),0),At));
}
/** A walk lamp standing on the ground at (X,Y) (HouseExteriorSite::GroundHeight: today's grade plus the Calder relief). */
void CampusLamp(FHallMesh& M,float X,float Y)
{
 const float LampFoot=HouseExteriorSite::GroundHeight(X,Y);
 M.Cylinder(Metal,FVector(X,Y,LampFoot),FVector(X,Y,LampFoot+520),6,8,Plain);
 Box(M,Dark,X-23,X+23,Y-23,Y+23,LampFoot+512,LampFoot+528);
 // Homecoming: mercury-vapour lenses, and the school's banners (HouseCampusFall) in place of the orange ones.
 Box(M,HouseCampusFall::Enabled()?HouseCampusFall::MercuryLensKey():FName(TEXT("EX_Flood")),X-17,X+17,Y-17,Y+17,LampFoot+507,LampFoot+512);
 if(!HouseCampusFall::Enabled())Box(M,Orange,X+10,X+90,Y-2,Y+2,LampFoot+280,LampFoot+448);
}
void LeafCluster(FHallMesh& M,FName K,FVector P,FVector U,FVector V)
{
 const FVector Face=FVector::CrossProduct(U,V).GetSafeNormal(),C=P+Face*5;
 const FVector N=(Face*.6f+FVector::UpVector*.75f).GetSafeNormal(),Tan=U.GetSafeNormal();
 M.QuadUV(K,P-U-V,P+U-V,C+U,C-U,N,{0,1},{1,1},{1,.5f},{0,.5f},Tan);
 M.QuadUV(K,C-U,C+U,P+U+V,P-U+V,N,{0,.5f},{1,.5f},{1,0},{0,0},Tan);
}
}
// ------------------------------------------------------------------ foundations on the Calder relief
// SourceArt/CalderLandscape, Phase 1 "Buildings on slopes". Every plan building stands on its level pad; its walls stop 70 cm
// under the floor. Where the ground beside a wall lies lower, the shell is carried down to 30 cm below the lowest ground by
// that wall: up to a 60 cm fall the stone plinth course runs down; deeper, a rusticated basement storey in the building's
// stone with dark blind windows on the facade's own columns, and one fixed service door on a stoop on the lowest side that
// has room for it. All of it is built in the canonical frame into the building's own mesh, so it collides as the shell does.
// Where the relief within 2 m of the footprint is flat (today) nothing is sampled and nothing is built; a wall whose ground
// reaches into a frozen zone is left as it is.
namespace FoundationImpl
{
constexpr float Probe=60.f,FaceProbe=12.f,Spacing=100.f,Below=30.f,Still=5.f,PlinthFall=60.f;
constexpr float CoreOut=8.f,FaceOut=12.f,CoursePitch=46.f,JointGap=6.f,HeadDrop=25.f,SillClear=25.f,Reveal=14.f;
constexpr float DoorHalf=50.f,DoorTall=205.f,DoorRoom=244.f,StoopHalf=90.f,StoopOut=120.f,Riser=17.f,Tread=30.f;
constexpr int32 MaxSteps=6;

/** Building Seed's canonical frame and the ground round it. Side S: 0 +Y (main), 1 -Y, 2 -X, 3 +X; U runs along +X on
 *  sides 0 and 1 and along +Y on 2 and 3; Dist is how far out from the side's outer face. */
struct FSite
{
    int32 Seed=0;
    FBox2D B;
    FTransform T;
    float Lift=0.f;
    explicit FSite(int32 InSeed):Seed(InSeed),B(HouseCampusInteriors::CanonRect(InSeed)),T(HouseCampusLayout::Frame(InSeed)),Lift(float(HouseCampusLayout::Frame(InSeed).GetLocation().Z)){}
    float Plane(int32 S) const{return S==0?B.Max.Y:S==1?B.Min.Y:S==2?B.Min.X:B.Max.X;}
    float UMin(int32 S) const{return S<2?B.Min.X:B.Min.Y;}
    float UMax(int32 S) const{return S<2?B.Max.X:B.Max.Y;}
    FVector2D At(int32 S,float U,float Dist) const
    {
        const float Pl=Plane(S)+((S==0||S==3)?Dist:-Dist);
        return S<2?FVector2D(U,Pl):FVector2D(Pl,U);
    }
    FVector2D World(const FVector2D& P) const{const FVector W=T.TransformPosition(FVector(P.X,P.Y,0.f));return FVector2D(W.X,W.Y);}
    /** The ground there in the canonical frame (the floor is Ground). */
    float GroundAt(const FVector2D& P) const{const FVector2D W=World(P);return HouseExteriorSite::GroundHeight(W.X,W.Y)-Lift;}
    bool FrozenAt(const FVector2D& P) const{const FVector2D W=World(P);return HouseCalderTerrain::InFrozen(W.X,W.Y);}
    /** The highest or lowest ground over U0..U1 along side S, D0..D1 out from it (a 3 x 3 grid). */
    float Extreme(int32 S,float U0,float U1,float D0,float D1,bool bHighest) const
    {
        float Best=bHighest?-FLT_MAX:FLT_MAX;
        for(int32 I=0;I<3;++I)for(int32 J=0;J<3;++J)
        {
            const float Z=GroundAt(At(S,FMath::Lerp(U0,U1,I*.5f),FMath::Lerp(D0,D1,J*.5f)));
            Best=bHighest?FMath::Max(Best,Z):FMath::Min(Best,Z);
        }
        return Best;
    }
    bool AnyFrozen(int32 S,float U0,float U1,float D0,float D1) const
    {
        for(const float U:{U0,U1})for(const float Dist:{D0,D1})if(FrozenAt(At(S,U,Dist)))return true;
        return false;
    }
};
/** The canonical side a plan face (N, S, E or W) is on. */
int32 SideOf(const HouseCampusLayout::FBlock& P,TCHAR Face)
{
    const FVector N=FRotator(0.f,-HouseCampusLayout::FaceYaw(P.Faces[0]),0.f).RotateVector(HouseCampusLayout::FaceNormal(Face));
    return N.Y>.5f?0:N.Y<-.5f?1:N.X<0.f?2:3;
}
/** Runs of side S kept clear of basement windows and the service door: the entrances (the real doors, the main entrance's
 *  canopy, planters and steps, the stand-in side entrances) and the landings in front of them. */
void KeepOuts(int32 Seed,int32 S,TArray<FVector2D>& Keep)
{
    TArray<HouseCampusInteriors::FOpening> Doors;HouseCampusInteriors::DoorOpenings(Seed,Doors);
    for(const HouseCampusInteriors::FOpening& D:Doors)if(D.Side==S)Keep.Add(FVector2D(D.U0-100.f,D.U1+100.f));
    if(S==0)Keep.Add(FVector2D(-620.f,620.f));
    const HouseCampusLayout::FBlock& P=HouseCampusLayout::Blocks()[Seed];
    for(int32 F=1;P.Faces[F];++F)if(SideOf(P,P.Faces[F])==S&&!HouseCampusInteriors::RealDoorOn(Seed,P.Faces[F]))Keep.Add(FVector2D(-480.f,480.f));
}
/** The facade's window columns on side S that may take a basement window or the service door. */
void Columns(const FSite& Site,int32 S,TArray<FVector2D>& Cols)
{
    TArray<FVector2D> All;HouseCampusInteriors::WindowColumns(Site.Seed,S,All);
    TArray<FVector2D> Keep;KeepOuts(Site.Seed,S,Keep);
    for(const FVector2D& C:All)
    {
        bool bClear=!Site.AnyFrozen(S,C.X-Reveal,C.Y+Reveal,0.f,Probe);
        for(const FVector2D& K:Keep)bClear&=!(K.X<C.Y+Reveal&&K.Y>C.X-Reveal);
        if(bClear)Cols.Add(C);
    }
}
}
bool Foundation(int32 Seed,FFoundation& Out)
{
 Out=FFoundation();
 if(!HouseCampusLayout::Enabled()||!HouseCampusLayout::Blocks().IsValidIndex(Seed))return false;
 // The gate: the relief within 2 m of the footprint (exactly 0 today, and in every frozen zone).
 const FBox2D Around=HouseCampusLayout::Blocks()[Seed].Rect().ExpandBy(200.f);
 if(HouseCalderTerrain::ReliefFlat(Around)||HouseCalderTerrain::BoxFrozen(Around))return false;
 const FoundationImpl::FSite Site(Seed);
 Out.bActive=true;
 // Each wall's outline, 60 cm out (and at the foundation's face, 12 cm out) every metre, round the corners.
 for(int32 S=0;S<4;++S)
 {
  const float U0=Site.UMin(S)-FoundationImpl::Probe,U1=Site.UMax(S)+FoundationImpl::Probe;
  const int32 Count=FMath::CeilToInt((U1-U0)/FoundationImpl::Spacing);
  float Low=FLT_MAX,High=-FLT_MAX;bool bFrozen=false;
  for(int32 K=0;K<=Count;++K)for(const float Dist:{FoundationImpl::FaceProbe,FoundationImpl::Probe})
  {
   const FVector2D P=Site.At(S,FMath::Min(U0+K*FoundationImpl::Spacing,U1),Dist);
   bFrozen|=Site.FrozenAt(P);
   const float Z=Site.GroundAt(P);Low=FMath::Min(Low,Z);High=FMath::Max(High,Z);
  }
  if(bFrozen)continue;
  Out.Drop[S]=Ground-Low;Out.Rise[S]=High-Ground;Out.Bottom[S]=Low-FoundationImpl::Below;
 }
 // The service door: on the lowest side with a basement storey where a column has room for a door under the floor line,
 // at the column with the lowest ground (nearer the middle of the side on a tie).
 // (Two sides often share their lowest ground at a corner: the main facade loses such a tie.)
 auto Depth=[&Out](int32 S){return Out.Drop[S]-(S==0?10.f:0.f);};
 int32 Order[4]={0,1,2,3};
 for(int32 I=1;I<4;++I)for(int32 J=I;J>0&&Depth(Order[J])>Depth(Order[J-1]);--J)Swap(Order[J],Order[J-1]);
 for(const int32 S:Order)
 {
  if(Out.Drop[S]<=FoundationImpl::PlinthFall)break;
  TArray<FVector2D> Cols;FoundationImpl::Columns(Site,S,Cols);
  float Best=FLT_MAX;
  for(const FVector2D& C:Cols)
  {
   const float U=(C.X+C.Y)*.5f;
   const float Under=Site.Extreme(S,U-FoundationImpl::StoopHalf,U+FoundationImpl::StoopHalf,FoundationImpl::FaceProbe,FoundationImpl::StoopOut,true);
   if(Under>Ground-FoundationImpl::DoorRoom)continue;
   if(Site.AnyFrozen(S,U-FoundationImpl::StoopHalf,U+FoundationImpl::StoopHalf,0.f,FoundationImpl::StoopOut+FoundationImpl::MaxSteps*FoundationImpl::Tread+20.f))continue;
   const float Score=Under+FMath::Abs(U)*.01f;
   if(Score<Best){Best=Score;Out.DoorSide=S;Out.DoorU=U;Out.DoorSill=Under+FoundationImpl::Riser;}
  }
  if(Out.DoorSide>=0)break;
 }
 return true;
}
bool EntranceStandClear(int32 Seed,float X0,float X1,float Y0,float Y1)
{
 if(!HouseCalderFeatures::Active()||!HouseCampusLayout::Blocks().IsValidIndex(Seed))return true;
 const FTransform T=HouseCampusLayout::Frame(Seed);
 for(int32 I=0;I<=2;++I)for(int32 J=0;J<=2;++J)
 {
  const FVector W=T.TransformPosition(FVector(FMath::Lerp(X0,X1,I*.5f),FMath::Lerp(Y0,Y1,J*.5f),0.f));
  if(HouseCalderFeatures::InStair(FVector2D(W.X,W.Y),15.f)||HouseCalderFeatures::InRamp(FVector2D(W.X,W.Y),15.f))return false;
 }
 return true;
}
float FoundationGround(int32 Seed,const FVector2D& Canon)
{
 if(!HouseCampusLayout::Blocks().IsValidIndex(Seed))return Ground;
 return FoundationImpl::FSite(Seed).GroundAt(Canon);
}
namespace FoundationImpl
{
struct FHole{float U0,U1,Z0,Z1;};
/** A box against side S's outer face: U0..U1 along it, D0..D1 out from it. */
void WallBox(FHallMesh& M,const FBox2D& B,FName K,int32 S,float U0,float U1,float D0,float D1,float Z0,float Z1)
{
 if(U1-U0<.5f||Z1-Z0<.5f)return;
 switch(S)
 {
 case 0:Box(M,K,U0,U1,B.Max.Y+D0,B.Max.Y+D1,Z0,Z1);break;
 case 1:Box(M,K,U0,U1,B.Min.Y-D1,B.Min.Y-D0,Z0,Z1);break;
 case 2:Box(M,K,B.Min.X-D1,B.Min.X-D0,U0,U1,Z0,Z1);break;
 default:Box(M,K,B.Max.X+D0,B.Max.X+D1,U0,U1,Z0,Z1);break;
 }
}
/** A pane facing out from side S, Dist out from its face. */
void WallPane(FHallMesh& M,const FBox2D& B,FName K,int32 S,float U0,float U1,float Dist,float Z0,float Z1)
{
 const float Sg=(S==0||S==3)?1.f:-1.f;
 const float Pl=(S==0?B.Max.Y:S==1?B.Min.Y:S==2?B.Min.X:B.Max.X)+Sg*Dist;
 const FVector Nrm=S<2?FVector(0,Sg,0):FVector(Sg,0,0);
 auto Corner=[S,Pl](float U,float Z){return S<2?FVector(U,Pl,Z):FVector(Pl,U,Z);};
 M.Quad(K,Corner(U0,Z0),Corner(U1,Z0),Corner(U1,Z1),Corner(U0,Z1),Nrm,Plain);
}
/** The rusticated courses from the floor line down to Bottom (each a block's height with a recessed joint under it, the
 *  core showing in the joint), cut round the windows and the door. */
void Courses(FHallMesh& M,const FBox2D& B,FName K,int32 S,float UA,float UB,float Bottom,TArray<FHole> Holes)
{
 Holes.Sort([](const FHole& L,const FHole& R){return L.U0<R.U0;});
 for(float Top=Ground;Top>Bottom+.5f;Top-=CoursePitch)
 {
  const float Z0=FMath::Max(Bottom,Top-CoursePitch+JointGap),Z1=Top;
  float U=UA;
  for(const FHole& H:Holes)
  {
   if(H.Z0>=Z1||H.Z1<=Z0||H.U1<=U||H.U0>=UB)continue;
   const float HA=FMath::Max(H.U0,U),HB=FMath::Min(H.U1,UB);
   WallBox(M,B,K,S,U,HA,CoreOut,FaceOut,Z0,Z1);
   if(H.Z0>Z0)WallBox(M,B,K,S,HA,HB,CoreOut,FaceOut,Z0,H.Z0);
   if(H.Z1<Z1)WallBox(M,B,K,S,HA,HB,CoreOut,FaceOut,H.Z1,Z1);
   U=HB;
  }
  WallBox(M,B,K,S,U,UB,CoreOut,FaceOut,Z0,Z1);
 }
}
/** A blind basement window in the column U0..U1: dark glass in a painted frame behind steel bars, a stone sill and lintel. */
void BlindWindow(FHallMesh& M,const FBox2D& B,int32 S,float U0,float U1,float Sill,float Head,TArray<FHole>& Holes)
{
 Holes.Add({U0-Reveal,U1+Reveal,Sill-12.f,Head+14.f});
 WallPane(M,B,Glass,S,U0,U1,CoreOut+1.f,Sill,Head);
 WallBox(M,B,Trim,S,U0-6.f,U0,CoreOut,CoreOut+3.f,Sill,Head);
 WallBox(M,B,Trim,S,U1,U1+6.f,CoreOut,CoreOut+3.f,Sill,Head);
 WallBox(M,B,Trim,S,U0,U1,CoreOut,CoreOut+3.f,Head-6.f,Head);
 WallBox(M,B,Trim,S,U0,U1,CoreOut,CoreOut+3.f,Sill,Sill+6.f);
 const float Mid=(U0+U1)*.5f;
 WallBox(M,B,Trim,S,Mid-3.f,Mid+3.f,CoreOut,CoreOut+2.5f,Sill+6.f,Head-6.f);
 for(const float F:{.25f,.75f}){const float U=FMath::Lerp(U0,U1,F);WallBox(M,B,Metal,S,U-1.2f,U+1.2f,CoreOut+1.5f,CoreOut+3.5f,Sill+6.f,Head-6.f);}
 WallBox(M,B,Metal,S,U0,U1,CoreOut+1.5f,CoreOut+3.5f,(Sill+Head)*.5f-1.2f,(Sill+Head)*.5f+1.2f);
 WallBox(M,B,Stone,S,U0-Reveal,U1+Reveal,0.f,17.f,Sill-12.f,Sill);
 WallBox(M,B,Stone,S,U0-Reveal,U1+Reveal,0.f,14.f,Head,Head+14.f);
}
/** The fixed service door (scenery: no ACampusDoor, it never opens): a steel leaf in a steel frame, dressed stone jambs and
 *  lintel, a concrete stoop level with its sill, and steps down to the ground in front of it. */
void ServiceDoor(FHallMesh& M,const FSite& Site,int32 S,float U,float Sill,TArray<FHole>& Holes)
{
 const FBox2D& B=Site.B;
 const float Top=Sill+DoorTall;
 Holes.Add({U-72.f,U+72.f,Sill-1.f,Top+22.f});
 WallBox(M,B,Stone,S,U-72.f,U-58.f,0.f,14.f,Sill,Top+8.f);
 WallBox(M,B,Stone,S,U+58.f,U+72.f,0.f,14.f,Sill,Top+8.f);
 WallBox(M,B,Stone,S,U-72.f,U+72.f,0.f,14.f,Top+8.f,Top+22.f);
 WallBox(M,B,Metal,S,U-58.f,U-DoorHalf,0.f,12.f,Sill,Top);
 WallBox(M,B,Metal,S,U+DoorHalf,U+58.f,0.f,12.f,Sill,Top);
 WallBox(M,B,Metal,S,U-58.f,U+58.f,0.f,12.f,Top,Top+8.f);
 WallBox(M,B,Dark,S,U-DoorHalf,U+DoorHalf,0.f,9.f,Sill,Top);
 WallBox(M,B,Metal,S,U+32.f,U+38.f,9.f,13.f,Sill+92.f,Sill+112.f);
 // The stoop, level with the sill, standing on the ground (its foot 30 cm under the lowest ground beneath it).
 WallBox(M,B,Concrete,S,U-StoopHalf,U+StoopHalf,0.f,StoopOut,Site.Extreme(S,U-StoopHalf,U+StoopHalf,0.f,StoopOut,false)-Below,Sill);
 // Steps down while the ground ahead lies more than a riser and a half below the last tread.
 float Tread0=Sill;
 for(int32 K=0;K<MaxSteps;++K)
 {
  const float D0=StoopOut+K*Tread,D1=D0+Tread;
  if(Tread0-Site.Extreme(S,U-75.f,U+75.f,D0,D1,true)<=Riser*1.5f)break;
  Tread0-=Riser;
  WallBox(M,B,Concrete,S,U-75.f,U+75.f,D0,D1,Site.Extreme(S,U-75.f,U+75.f,D0,D1,false)-Below,Tread0);
 }
}
/** Building Seed's foundation into its own (canonical) mesh: nothing where the relief round it is flat. */
void Build(FHallMesh& M,int32 Seed,float Height,int32 Style)
{
 FFoundation F;
 if(!Foundation(Seed,F))return;
 const FSite Site(Seed);
 const FBox2D& B=Site.B;
 const FName Base=Style==1?FName(TEXT("EX_CampusLimestone")):Stone;   // the rusticated base in the building's stone
 const float Story=HouseCampusInteriors::Story(Height);
 int32 Blind=0,Basements=0,Plinths=0;
 for(int32 S=0;S<4;++S)
 {
  if(F.Drop[S]<=Still)continue;
  // Sides 0 and 1 hold the corners (as the plinth ring does); 2 and 3 take a corner whose end side has no foundation.
  float UA=Site.UMin(S),UB=Site.UMax(S);
  if(S<2){UA-=FaceOut;UB+=FaceOut;}
  else{if(F.Drop[1]<=Still)UA-=FaceOut;if(F.Drop[0]<=Still)UB+=FaceOut;}
  const float Bot=F.Bottom[S];
  if(F.Drop[S]<=PlinthFall){WallBox(M,B,Stone,S,UA,UB,0.f,FaceOut,Bot,Ground);++Plinths;continue;}
  // The basement storey: the core, its windows and door, the courses cut round them.
  ++Basements;
  WallBox(M,B,Base,S,UA,UB,0.f,CoreOut,Bot,Ground);
  TArray<FHole> Holes;
  if(F.DoorSide==S)ServiceDoor(M,Site,S,F.DoorU,F.DoorSill,Holes);
  TArray<FVector2D> Cols;Columns(Site,S,Cols);
  if(F.DoorSide==S)Cols.RemoveAll([&F](const FVector2D& C){return FMath::Abs((C.X+C.Y)*.5f-F.DoorU)<1.f;});
  // The ground that must stay clear under each column's window (at the face and 60 cm out, across its stones).
  TArray<float> Clear;
  for(const FVector2D& C:Cols)Clear.Add(Site.Extreme(S,C.X-Reveal,C.Y+Reveal,FaceProbe,Probe,true));
  // The top row hangs under the floor line, as tall as fits under a third of the columns; deeper rows (a fall of more than
  // a storey) are full windows a storey apart. Each window only where its column's ground lies clear below its sill.
  float FirstTall=0.f;
  for(const float Tall:{212.f,160.f,120.f,80.f,55.f})
  {
   int32 Fit=0;
   for(const float Z:Clear)Fit+=Z<=Ground-HeadDrop-Tall-SillClear?1:0;
   if(Fit>0&&Fit*3>=Cols.Num()){FirstTall=Tall;break;}
  }
  for(int32 Row=0;Row<4&&FirstTall>0.f;++Row)
  {
   const float Head=Ground-HeadDrop-Row*Story,Tall=Row==0?FirstTall:212.f;
   if(Head-Tall-SillClear<Bot)break;
   for(int32 I=0;I<Cols.Num();++I)if(Clear[I]<=Head-Tall-SillClear){BlindWindow(M,B,S,Cols[I].X,Cols[I].Y,Head-Tall,Head,Holes);++Blind;}
  }
  Courses(M,B,Base,S,UA,UB,Bot,Holes);
 }
 const HouseCampusLayout::FBlock& P=HouseCampusLayout::Blocks()[Seed];
 UE_LOG(LogTemp,Display,TEXT("CAMPUS_FOUNDATION id=%s drop=%.0f,%.0f,%.0f,%.0f rise=%.0f,%.0f,%.0f,%.0f plinths=%d basements=%d blindWindows=%d serviceDoor=%d"),
  P.Id,F.Drop[0],F.Drop[1],F.Drop[2],F.Drop[3],F.Rise[0],F.Rise[1],F.Rise[2],F.Rise[3],Plinths,Basements,Blind,F.DoorSide);
}
}
void CreateLook(UHouseBlackHallSubsystem* Look)
{
 HallMakePlain(Look,TEXT("EX_CampusLimestone"),FLinearColor(.38f,.35f,.29f),.87f);
 HallMakePlain(Look,TEXT("EX_CampusWarmBrick"),FLinearColor(.25f,.09f,.055f),.85f);
 HallMakePlain(Look,TEXT("EX_CampusDirt"),FLinearColor(.25f,.15f,.085f),.95f);
 HallMakePlain(Look,TEXT("EX_CampusSeats"),FLinearColor(.22f,.09f,.025f),.70f);
 HallMakePlain(Look,TEXT("EX_CampusTurf"),FLinearColor(.028f,.08f,.038f),.92f);
 HallMakePlain(Look,TEXT("EX_CampusScore"),FLinearColor(.05f,.065f,.08f),.68f,0,FLinearColor(.06f,.095f,.14f));
 auto* LeafParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureTreeFoliage.M_NatureTreeFoliage"));
 auto* Clusters=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/T_Nature_LeafClusters.T_Nature_LeafClusters"));
 for(int Species=0;Species<3;++Species)for(int Palette=0;Palette<3;++Palette)
 {
  const FName Key(*FString::Printf(TEXT("EX_ForestLeaf%d_%d"),Species,Palette));
  if(LeafParent&&Clusters)
  {
   auto* M=UMaterialInstanceDynamic::Create(LeafParent,Look);
   M->SetTextureParameterValue(TEXT("BaseMap"),Clusters);
   M->SetVectorParameterValue(TEXT("AtlasRect"),FLinearColor(Palette/3.f+.0005f,Species/3.f+.0005f,1.f/3.f-.001f,1.f/3.f-.001f));
   M->SetScalarParameterValue(TEXT("WindStrength"),.8f);
   M->SetScalarParameterValue(TEXT("Transmission"),.42f);
   Look->Materials.Add(Key,M);
  }
  else HallMakePlain(Look,Key,FLinearColor(.22f,.11f,.032f),.94f);
 }
 HallMakePlain(Look,TEXT("EX_RidgeNear"),FLinearColor(.045f,.07f,.075f),1,0,FLinearColor(.005f,.009f,.012f));
 HallMakePlain(Look,TEXT("EX_RidgeMid"),FLinearColor(.075f,.105f,.14f),1,0,FLinearColor(.011f,.016f,.023f));
 HallMakePlain(Look,TEXT("EX_RidgeFar"),FLinearColor(.12f,.15f,.20f),1,0,FLinearColor(.021f,.028f,.039f));
}
FLayout BuildCampus(FHallMesh& Near,FHallMesh& Far)
{
 FLayout L;
 // Front-road campus clusters keep a broad central sightline to the stadium.
 struct FBlock{float X,Y,W,D,H;int Style;};
 const FBlock Blocks[]={
 {-14800,-8100,5200,3000,2120,0},{-8700,-7900,3800,2600,2540,1},
 {-4850,-7000,2100,1700,1260,2},{9600,-7700,3500,2500,1780,1},
 {14500,-8450,4300,3600,2540,0},{-14500,-12500,4800,3000,2120,2},
 {-8700,-13100,3600,3000,2960,0},{-14800,-17300,4400,3500,1700,1},
 {-9300,-18500,3500,4300,2540,2},{-14300,-22700,4800,3700,2960,0},
 {-7900,-24100,4200,3100,2120,1},{10000,-12800,3000,3400,2540,2},
 {15000,-14200,4200,3900,2960,0},{11000,-19100,3300,3200,2120,1},
 {15500,-22300,4200,3600,2540,2},{2100,-23000,7300,2900,2120,0}};
 const bool bPlan=HouseCampusLayout::Enabled();
 if(!bPlan){int Seed=0;for(const FBlock& B:Blocks)Building(Near,L,FBox2D(FVector2D(B.X-B.W/2,B.Y-B.D/2),FVector2D(B.X+B.W/2,B.Y+B.D/2)),B.H,B.Style,Seed++);}
 else
 {
  // The campus plan (HouseCampusLayout). The old footprints are still handed to tree planting as fixed reservations, so
  // the planting (and WOODS_PASS_ORIGINAL_TREES) never changes; the buildings themselves stand where the plan puts them.
  for(const FBlock& B:Blocks){L.BuildingFootprints.Add(FBox2D(FVector2D(B.X-B.W/2-150,B.Y-B.D/2-150),FVector2D(B.X+B.W/2+150,B.Y+B.D/2+150)));++L.Buildings;}
  const TArray<HouseCampusLayout::FBlock>& Plan=HouseCampusLayout::Blocks();
  for(int32 I=0;I<Plan.Num();++I)
  {
   const HouseCampusLayout::FBlock& P=Plan[I];const TCHAR Main=P.Faces[0];
   const bool bTurned=Main==TEXT('E')||Main==TEXT('W');
   const float W=bTurned?P.Y1-P.Y0:P.X1-P.X0,D=bTurned?P.X1-P.X0:P.Y1-P.Y0;
   // Built with its main entrance on +Y in its own frame, then turned to face the way the plan says and set on its level
   // pad (HouseCampusLayout::Frame).
   FHallMesh Own;FLayout Unused;
   const FBox2D Canon(FVector2D(-W*.5f,-D*.5f),FVector2D(W*.5f,D*.5f));
   if(HouseCampusInteriors::OpenFloors(I)>0)ShellBuilding(Own,Canon,P.Height,P.Style,I,HouseCampusLayout::SignKey(I));   // a walk-in building
   else Building(Own,Unused,Canon,P.Height,P.Style,I,HouseCampusLayout::SignKey(I),HouseCampusLayout::ApronDepth(I,Main,HouseCampusLayout::MainApronW,HouseCampusLayout::MainApronD));
   // On sloping ground the shell is carried down to the ground beside it (nothing at all where the relief is flat).
   FoundationImpl::Build(Own,I,P.Height,P.Style);
   AppendTransformed(Near,Own,HouseCampusLayout::Frame(I));
   for(int32 F=1;P.Faces[F];++F)if(!HouseCampusInteriors::RealDoorOn(I,P.Faces[F]))
    SideEntrance(Near,I,P.Rect(),P.Faces[F],HouseCampusLayout::ApronDepth(I,P.Faces[F],HouseCampusLayout::SideApronW,HouseCampusLayout::SideApronD));
  }
 }
 // The clock tower that breaks up the academic skyline is its own actor (HouseClockTower), so its hands, bells and
 // crows can move. It stands between the walk and the library, in the band no grid tree can reach.
 // Campus walks, small plazas, lamp banners and benches are emissive scenery.
 if(bPlan)
 {
  // Walks, plazas and benches are the plan's ground mesh (HouseCampusLayout::BuildGround); the lamps follow its walks.
  TArray<FVector2D> Lamps;HouseCampusLayout::LampPoints(Lamps);
  for(const FVector2D& P:Lamps)CampusLamp(Near,P.X,P.Y);
 }
 else
 {
 for(float X:{-11200.f,-6100.f,7200.f,12800.f})Box(Near,Concrete,X-100,X+100,-27000,-6100,Ground+1,Ground+4);
 for(float Y:{-6000.f,-10300.f,-16100.f,-21100.f})Box(Near,Concrete,-17800,18000,Y-100,Y+100,Ground+1,Ground+4);
 for(float X=-16000;X<17000;X+=1900)CampusLamp(Near,X,-5900);
 for(float X:{-11300.f,7300.f})for(float Y=-9000;Y>-25000;Y-=2800)CampusLamp(Near,X,Y);
 for(float X:{-11000.f,-6900.f,7800.f,12000.f})for(float Y:{-9700.f,-15900.f}){
  Box(Near,Dark,X-110,X+110,Y-28,Y+28,Ground+40,Ground+47);Box(Near,Dark,X-110,X+110,Y+23,Y+33,Ground+47,Ground+89);
  for(float XX:{X-85,X+85})Box(Near,Metal,XX-3,XX+3,Y-20,Y+24,Ground+3,Ground+40);
 }
 }
 // Relocated stadium: a real stepped bowl, open field, concourse arches and press box.
 constexpr float RX=5600,RY=3800;const FVector C(StadiumX,StadiumY,Ground);constexpr int Seg=40;
 auto P=[&](float A,float R,float Z){return C+FVector(FMath::Cos(A)*RX*R,FMath::Sin(A)*RY*R,Z);};
 // With the stadium walk-in (HouseCampusInteriors), its gates, a vomitory, the players' tunnel and the press box are built
 // open, and the window band over the concourse is lit.
 const bool bWalkIn=HouseCampusInteriors::StadiumOpen(),bPolish=HouseCampusPolish::Enabled();
 for(int K=0;K<Seg;++K){const float A=K*2*PI/Seg,B=(K+1)*2*PI/Seg;const FVector A0=P(A,1,0),B0=P(B,1,0);const FVector N=FVector(FMath::Cos((A+B)/2),FMath::Sin((A+B)/2),0);
  // (With the exterior polish on, every segment's window is a real opening, and its facade, window dressing, the grilles
  // in the arches and the flood lamp banks are HouseCampusPolish's.)
  if(HouseCampusInteriors::StadiumGate(K))HouseCampusInteriors::StadiumGateWall(Near,K,A0,B0,N,bPolish);else if(bPolish)HouseCampusPolish::StadiumWall(Near,K,A0,B0);else{Panel(Near,Brick,A0,B0,0,1050,N);Panel(Near,Stone,A0+N*3,B0+N*3,30,85,N);}
  if(HouseStadiumDetail::Aisle(K)){const FVector CapL=A0+N*3,CapR=B0+N*3;const float CapGap=135.f/FVector::Dist(CapL,CapR);Panel(Near,Stone,CapL,FMath::Lerp(CapL,CapR,.5f-CapGap),1040,1110,N);Panel(Near,Stone,FMath::Lerp(CapL,CapR,.5f+CapGap),CapR,1040,1110,N);}else Panel(Near,Stone,A0+N*3,B0+N*3,1040,1110,N);if(!bPolish)Panel(Near,HouseCampusInteriors::StadiumConcourse(K)?Glow:HouseSurroundings::Enabled()?Dark:Glow,A0+N*4+(B0-A0)*.27f,A0+N*4+(B0-A0)*.60f,420,660,N);
  for(int Row=0;Row<12;++Row){if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;if(HouseStadiumDetail::Enabled()){HouseStadiumDetail::Tier(Near,K,Row,Stone);continue;}const float R0=.69f+Row*.024f,R1=R0+.024f,Z=240+Row*63.f;Near.Quad(TEXT("EX_CampusSeats"),P(A,R0,Z),P(B,R0,Z),P(B,R1,Z),P(A,R1,Z),FVector::UpVector,Plain);Panel(Near,Stone,P(A,R1,0),P(B,R1,0),Z-63,Z,N);
   // Football stands: closed risers toward the field, a podium wall under the front row and a back wall up to the rim.
   if(HouseCampusFall::Enabled()){if(Row==0&&HouseCampusInteriors::StadiumTunnel(K))HouseCampusInteriors::StadiumTunnelWall(Near,K,P(A,R0,0),P(B,R0,0),-N,Z);else Panel(Near,Stone,P(A,R0,0),P(B,R0,0),Row==0?0.f:Z-63,Z,-N);if(Row==11)Panel(Near,Stone,P(A,R1,0),P(B,R1,0),Z,1050,-N);}}
  if(HouseStadiumDetail::Aisle(K))HouseStadiumDetail::Rim(Near,K,Stone);else Near.Quad(Stone,P(A,.98f,1050),P(B,.98f,1050),P(B,1.045f,1050),P(A,1.045f,1050),FVector::UpVector,Plain);
  // (The rim's rail and its posts would run through the walk-in press box: none under it.)
  const bool bUnderPress=bWalkIn&&K>=7&&K<=12;
  if(!HouseStadiumDetail::Enabled()&&K%2==0&&!bUnderPress)Near.Cylinder(Metal,P(A,1.02f,1110),P(A,1.02f,1230),4,6,Plain);
  if(HouseSurroundings::Enabled())
  {
   if(!HouseStadiumDetail::Enabled()&&!bUnderPress)HouseSurroundings::Beam(Near,Metal,P(A,1.02f,1230),P(B,1.02f,1230),5,5);
   if(!HouseStadiumDetail::Enabled())for(int Row=0;Row<12;++Row){if(HouseCampusInteriors::StadiumVomitory(K,Row))continue;const float Rb=.70f+Row*.024f,Z=246+Row*63.f;HouseSurroundings::Beam(Near,HouseCampusFall::Enabled()?HouseCampusFall::SeatKey(Row):FName(TEXT("EX_Orange")),P(A+.003f,Rb,Z),P(B-.003f,Rb,Z),15,12);}
   if(!HouseStadiumDetail::Enabled()&&K%5==0){for(int Row=0;Row<12;++Row){const float Rb=.69f+Row*.024f,Z=241+Row*63.f;HouseSurroundings::Beam(Near,Stone,P(A-.007f,Rb,Z),P(A+.007f,Rb,Z),26,4);}
    Near.Cylinder(Metal,P(A,.72f,340),P(A,.98f,1050),4,8,Plain);}
   if(K%2==0&&!HouseCampusInteriors::StadiumGate(K)&&!HouseStadiumDetail::Portal(K))
   {
    const FVector Centre=(A0+B0)*.5f+N*9,Along=(B0-A0).GetSafeNormal();
    auto Arch=[](float W,float H){TArray<FVector2D> Shape={{-W,0},{W,0},{W,H}};for(int I=1;I<=12;++I){float Angle=I*PI/12;Shape.Add({W*FMath::Cos(Angle),H+W*FMath::Sin(Angle)});}return Shape;};
    Near.Prism(Stone,Arch(135,295),Centre,Along,FVector::UpVector,N,12,Plain);
    Near.Prism(Dark,Arch(112,295),Centre+N*13+FVector(0,0,8),Along,FVector::UpVector,N,2,Plain);
    if(!bPolish&&(!HouseCampusFall::Enabled()||(K!=8&&K!=10)))for(int J=-2;J<=2;++J)HouseSurroundings::Beam(Near,Metal,Centre+Along*(J*37)+N*17+FVector(0,0,8),Centre+Along*(J*37)+N*17+FVector(0,0,285),4,4);
   }
  }
 }
 // The baseball diamond; with homecoming on, the football field, track and goalposts are HouseCampusFall's instead.
 if(!HouseCampusFall::Enabled())
 {
 Box(Near,TEXT("EX_CampusTurf"),StadiumX-3300,StadiumX+3300,StadiumY-2000,StadiumY+2000,Ground+1,Ground+4);
 const FVector Home(StadiumX,StadiumY+1580,Ground+4.5f),First(StadiumX+1580,StadiumY,Ground+4.5f),Second(StadiumX,StadiumY-1580,Ground+4.5f),Third(StadiumX-1580,StadiumY,Ground+4.5f);
 Near.Quad(TEXT("EX_CampusDirt"),Home,First,Second,Third,FVector::UpVector,Plain);
 Near.Cylinder(TEXT("EX_CampusDirt"),C+FVector(0,0,5),C+FVector(0,0,13),180,24,Plain);
 for(const FVector Base:{Home,First,Second,Third})Near.Box(Trim,FTransform(FRotator(0,45,0),Base+FVector(0,0,2)),FVector(18,18,1),Plain);
 for(const FVector Side:{First,Third}){const FVector Direction=(Side-Home).GetSafeNormal();const FVector End=Home+Direction*4100;Near.Box(Trim,FTransform(FRotationMatrix::MakeFromX(Direction).ToQuat(),(Home+End)*.5f+FVector(0,0,.8f)),FVector(2050,2,.5f),Plain);}
 }
 if(bWalkIn){HouseCampusInteriors::StadiumPressShell(Near);HouseCampusInteriors::StadiumTower(Near);}
 else Box(Near,Dark,StadiumX-2300,StadiumX+2300,StadiumY+3300,StadiumY+4050,Ground+1110,Ground+1950);
 Box(Near,Stone,StadiumX-2350,StadiumX+2350,StadiumY+3250,StadiumY+4100,Ground+1910,Ground+1980);
 Panel(Near,Glass,FVector(StadiumX-2140,StadiumY+4055,0),FVector(StadiumX+2140,StadiumY+4055,0),Ground+1290,Ground+1760,FVector(0,1,0));
 for(float X=StadiumX-2100;X<StadiumX+2150;X+=350)Box(Near,Trim,X-4,X+4,StadiumY+4056,StadiumY+4062,Ground+1290,Ground+1760);
 // Orange T monogram and scoreboard lamps are mesh emission, not more real lights.
 if(!HouseCampusFall::Enabled())
 {
  Box(Near,Orange,StadiumX-600,StadiumX+600,StadiumY+4065,StadiumY+4072,Ground+1650,Ground+1780);
  Box(Near,Orange,StadiumX-110,StadiumX+110,StadiumY+4065,StadiumY+4072,Ground+1290,Ground+1650);
 }
 if(!(HouseStadiumDetail::Enabled()&&HouseStadiumDetail::Step()>=2))
 {
 Box(Near,Dark,StadiumX-850,StadiumX+850,StadiumY-3800,StadiumY-3680,Ground+1250,Ground+2250);
 Box(Near,TEXT("EX_CampusScore"),StadiumX-770,StadiumX+770,StadiumY-3678,StadiumY-3674,Ground+1380,Ground+2150);
 }
 if(HouseSurroundings::Enabled())
 {
  const FVector Base(StadiumX,StadiumY-3670,Ground+1450);
  auto Word=[&](const FString& Text,FVector C,float Size){const float Step=Size*.94f;C.X-=Step*(Text.Len()-1)*.5f;
   for(int I=0;I<Text.Len();++I)for(const auto& Stroke:HallGlyph(Text[I]))for(int J=1;J<Stroke.Num();++J)
   {auto V=[&](FVector2D Q){return C+FVector(I*Step+Q.X*Size/6,0,Q.Y*Size/6);};HouseSurroundings::Beam(Near,Trim,V(Stroke[J-1]),V(Stroke[J]),Size*.065f,3);}};
  // Homecoming puts the Calder State bulb scoreboard (HouseCampusFall) on this face instead.
  if(!HouseCampusFall::Enabled()){Word(TEXT("VOLS"),Base+FVector(0,0,475),110);Word(TEXT("HOME  GUEST"),Base+FVector(0,0,310),70);Word(TEXT("03    01"),Base+FVector(0,0,90),152);}
  for(float X=StadiumX-700;X<StadiumX+700;X+=280)Box(Near,Metal,X-3,X+3,StadiumY+3240,StadiumY+3255,Ground+1130,Ground+1895);
  for(float X=StadiumX-2250;X<StadiumX+2250;X+=520)for(float Y:{StadiumY+3320,StadiumY+4030})Box(Near,Metal,X-20,X+20,Y-15,Y+15,Ground+1150,Ground+1880);
 }
 for(float Deg:{35.f,90.f,145.f}){const float A=FMath::DegreesToRadians(Deg);const FVector Base=P(A,1.04f,0),Top=Base+FVector(0,0,3900);Near.Cylinder(Metal,Base,Top,20,10,Plain);if(!bPolish)for(int J=0;J<5;++J)Box(Near,TEXT("EX_Flood"),Top.X-145+J*65,Top.X-103+J*65,Top.Y-30,Top.Y+30,Top.Z-10,Top.Z+110);L.StadiumLamps.Add(Top+FVector(0,-200,-150));}
 L.BuildingFootprints.Add(FBox2D(FVector2D(StadiumX-RX-250,StadiumY-RY-300),FVector2D(StadiumX+RX+250,StadiumY+RY+800)));
 // Low-cost blocks farther into the same front campus, never behind the house. The far lawn under them lies on the valley
 // floor beyond the walkable square (-60 today and with -NoCalderTerrain). With the Calder terrain the outer land
 // (HouseCalderOuterLand: the lower bluff, the town's valley, the river) is the ground there, so the lawn box goes.
 const float FarLawn=HouseCalderTerrain::ValleyZ(0.f,-40400.f);
 if(!HouseCalderTerrain::Enabled())Box(Far,TEXT("EX_Grass"),-31000,31000,-49000,-31800,FarLawn-75,FarLawn-3);
 // Under the plan, none of them may stand on the walkable ground that ends at Y -32000 (F3 and F7 used to).
 // With the polish on, the town of Calder stands there instead (HouseCampusPolishTown.cpp).
 // With the Calder terrain (and the polish off) the lawn box is gone and the lowest valley floor under them lies 15-23 m
 // below today's grade: each is built as today and set down on the lowest outer land under its footprint.
 if(!HouseCampusPolish::Enabled()){FRandomStream R(82026);for(int K=0;K<12;++K){const float X=R.FRandRange(-25000,24000),Y0=R.FRandRange(-43000,-31000),H=R.FRandRange(1500,3000);const float Y=bPlan?FMath::Min(Y0,-33600.f):Y0;FLayout Unused;const FBox2D Foot(FVector2D(X-1600,Y-1400),FVector2D(X+1600,Y+1400));
  if(!HouseCalderTerrain::Enabled()){Building(Far,Unused,Foot,H,K%3,K+41);continue;}
  FHallMesh Own;Building(Own,Unused,Foot,H,K%3,K+41);float Low=HouseCalderTerrain::ValleyZ(X,Y);for(const float Cx:{Foot.Min.X,Foot.Max.X})for(const float Cy:{Foot.Min.Y,Foot.Max.Y})Low=FMath::Min(Low,HouseCalderTerrain::ValleyZ(Cx,Cy));
  AppendTransformed(Far,Own,FTransform(FVector(0.f,0.f,Low-Ground)));}}
 UE_LOG(LogTemp,Display,TEXT("CAMPUS_LAYOUT buildings=%d distantBuildings=12 stadium=(%.0f,%.0f) rearBuildings=0 lights=3 homecoming=%d plan=%d"),L.Buildings,StadiumX,StadiumY,int32(HouseCampusFall::Enabled()),int32(bPlan));
 return L;
}
void Mountains(FHallMesh& M)
{
 // The three north ridges rise off the foothills' base (HouseCalderTerrain::RidgeBase; -362 today and with -NoCalderTerrain).
 // With the Calder terrain that is the foothill top along the north seam (landform.py ex_ridges(): only Z moves, every
 // vertex keeps its X and Y), and the near sheet is notched where the Calder Branch valley leaves to the north-east.
 const bool bCalder=HouseCalderTerrain::Enabled();
 for(int Layer=0;Layer<3;++Layer)
 {
 const float Y0=Layer==0?32000:Layer==1?65000:112000,Depth=Layer==0?19000:Layer==1?28000:40000,H=Layer==0?8000:Layer==1?18500:34000;
  const FName K=Layer==0?TEXT("EX_RidgeNear"):Layer==1?TEXT("EX_RidgeMid"):TEXT("EX_RidgeFar");constexpr int NX=96,NY=8;
  auto P=[&](int I,int J){const float X=-145000+290000.f*I/NX,T=float(J)/NY;const float Wave=.55f+.18f*FMath::Sin(X*.000057f+Layer*1.9f)+.11f*FMath::Sin(X*.000131f+Layer)+.07f*FMath::Cos(X*.000291f+1.1f*Layer);const float Cross=FMath::Pow(FMath::Max(0.f,FMath::Sin(T*PI)),.65f);const float Edge=FMath::Clamp((145000-FMath::Abs(X))/20000,0.f,1.f);const float VertY=Y0+T*Depth+1600*FMath::Sin(X*.00008f+Layer);const float Z=HouseCalderTerrain::RidgeBase(X,VertY)+H*Wave*Cross*Edge;return FVector(X,VertY,bCalder&&Layer==0?float(HouseCalderFarLand::NeNotch(X,VertY,Z,900.0)):Z);};
  // With the Calder terrain each sheet is seated on the ground (HouseCalderOuterLand::SeatSheet, Z only): a rim left over a
  // lower outer land (the Calder Branch valley, the cove walls' flanks) sinks under it, and the near sheet's front row,
  // which dips inside the walkable square west of X 0, sinks so the sheet rises out of the outer land beyond the seam
  // instead of standing over the north foothills.
  TArray<FVector> Seated;
  if(bCalder){Seated.SetNum((NX+1)*(NY+1));for(int I=0;I<=NX;++I)for(int J=0;J<=NY;++J)Seated[I*(NY+1)+J]=P(I,J);HouseCalderOuterLand::SeatSheet(Seated,NX,NY);}
  auto V=[&](int I,int J){return bCalder?Seated[I*(NY+1)+J]:P(I,J);};
  for(int I=0;I<NX;++I)for(int J=0;J<NY;++J){const FVector A=V(I,J),B=V(I+1,J),C=V(I+1,J+1),D=V(I,J+1);FVector N=FVector::CrossProduct(B-A,D-A).GetSafeNormal();if(N.Z<0)N=-N;M.Quad(K,A,B,C,D,N,FHallUV::World(3000));}
 }
 UE_LOG(LogTemp,Display,TEXT("REAR_MOUNTAINS ridges=3 nearestY=32000 farthestY=152000 buildings=0"));
}
void RearTrees(TArray<FTransform>& Big,TArray<FTransform>& Small,TArray<FTransform>& Distant)
{
 FRandomStream R(731026);int Close=0;
 // Keep full individual leaves and interactive trunks along the reachable
 // woodland edge. Deeper canopy uses the same photographed autumn leaf atlas
 // on far fewer folded clusters instead of hundreds of complete leaf meshes.
 for(float X=-7600;X<=7600;X+=900)for(float Y=ForestStartY;Y<6000;Y+=900){const float XX=X+R.FRandRange(-170,170),YY=Y+R.FRandRange(-110,110);const float S=R.FRandRange(.97f,1.35f);(Close++%3?Big:Small).Add(FTransform(FRotator(0,R.FRand()*360,0),FVector(XX,YY,Rear-5),FVector(S,S,S*R.FRandRange(.92f,1.18f))));}
 for(float X=-24500;X<=24500;X+=1000)for(float Y=6500;Y<=31000;Y+=1250){if(X>8300&&X<10700)continue;if(R.FRand()<.08f)continue;const float S=R.FRandRange(.82f,1.30f);Distant.Add(FTransform(FRotator(0,R.FRand()*360,0),FVector(X+R.FRandRange(-300,300),Y+R.FRandRange(-300,300),Rear-7),FVector(S,S,S*R.FRandRange(.85f,1.25f))));}
 UE_LOG(LogTemp,Display,TEXT("REAR_WOODS detailedTrees=%d distantTrees=%d startsY=%.0f courtClear=1"),Close,Distant.Num(),ForestStartY);
}
void DistantTree(FHallMesh& M,int Variant)
{
 FRandomStream R(601+Variant*79);const float Height=Variant==1?1200.f:1000.f;
 const FName Bark(TEXT("EX_NatureBark"));M.Cylinder(Bark,FVector(0,0,0),FVector(14,-8,Height*.85f),26,7,Plain);
 for(int Branch=0;Branch<9;++Branch)
 {
  const float A=Branch*2.39996f+Variant;const float Reach=R.FRandRange(220,440);
  const FVector Start(0,0,Height*(.35f+Branch*.04f)),Tip(FMath::Cos(A)*Reach,FMath::Sin(A)*Reach,Height*(.63f+Branch*.035f));
  M.Cylinder(Bark,Start,Tip,9,5,Plain);
  for(int J=0;J<9;++J)
  {
   const FVector P=FMath::Lerp(Start,Tip,.55f+J*.06f)+FVector(R.FRandRange(-100,100),R.FRandRange(-100,100),R.FRandRange(-65,130));
   const FRotator Rot(R.FRandRange(-65,65),R.FRandRange(0,360),R.FRandRange(-65,65));
   const float S=R.FRandRange(53,83);const FName K(*FString::Printf(TEXT("EX_ForestLeaf%d_%d"),Variant,(Branch+J)%3));
   LeafCluster(M,K,P,Rot.RotateVector(FVector(S,0,0)),Rot.RotateVector(FVector(0,0,S)));
  }
 }
}
}
