#include "HouseWestField.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseWoodsDetail.h"

namespace HouseWestField
{
namespace
{
float Smooth(float A,float B,float V){return FMath::SmoothStep(A,B,V);}
float BankEdge(float Y){return Smooth(1770,2120,Y)*(1-Smooth(3850,4300,Y));}
const FHallUV UV=FHallUV::World(140);
void Plank(FHallMesh& M,FName Key,FVector A,FVector B,float Width,float Depth)
{
 const FVector D=B-A;const FTransform T(FRotationMatrix::MakeFromX(D).ToQuat(),(A+B)*.5f);
 // Grain runs along each board instead of being projected across its end face.
 M.Box(Key,T,FVector(D.Size()*.5f,Width*.5f,Depth*.5f),FHallUV::Local(T,140));
}
}
float RaisedHeight(float X,float Y,float Original)
{
 if(X>=-2500||X<=-21000||Y<=300||Y>=6500)return Original;
 const float West=Smooth(-21000,-19500,X);
 const float Field=West*(1-Smooth(-4300,-3700,X))*(1-Smooth(4300,6500,Y));
 const float MoundJoin=West*Smooth(2500,2830,-X)*BankEdge(Y);
 // Start at the actual grade at Y=300, then reach the mound's level. Blending
 // the old falling grade directly produced an unnecessary dip in the field.
 const float Profile=FMath::Lerp(-116.625f,MoundTop,Smooth(300,1650,Y));
 return FMath::Max(FMath::Lerp(Original,Profile,Field),FMath::Lerp(Original,MoundTop,MoundJoin));
}
float JoinBank(float X,float Y,float OriginalSurface)
{
 if(X>=-2500)return OriginalSurface;
 const float Target=FMath::Lerp(-361.2f,MoundTop,BankEdge(Y));
 return FMath::Lerp(OriginalSurface,Target,Smooth(2500,2830,-X));
}
void CreateLook(UHouseBlackHallSubsystem* L)
{
 constexpr int Size=512;FHallImage Base,Normal;Base.Init(Size,Size,FLinearColor::Black);TArray<float> Relief;Relief.SetNum(Size*Size);
 for(int Y=0;Y<Size;++Y)for(int X=0;X<Size;++X)
 {
  const float Warp=8*FMath::Sin(X*.029f)+3*FMath::Sin(X*.073f),Grain=HallNoise(X*.045f,(Y+Warp)*.44f,611),Broad=HallFbm(X*.018f,Y*.045f,3,619);
  const float Crack=FMath::Pow(FMath::Max(0.f,1-FMath::Abs(FMath::Sin((Y+Warp)*.153f))*25),2.f)*Smooth(.3f,.7f,HallNoise(X*.012f,Y*.023f,631));
  const float V=.20f+Broad*.12f+(Grain-.5f)*.06f-Crack*.12f;
  Base.At(X,Y)=FLinearColor(V,V*.92f,V*.78f);Relief[Y*Size+X]=Grain*.10f-Crack*.23f;
 }
 HallHeightToNormal(Relief,Size,Size,1.6f,true,Normal);
 auto* B=HallMakeTexture(Base,true,false,TEXT("T_WestFieldFence"));auto* N=HallMakeTexture(Normal,false,true,TEXT("T_WestFieldFenceN"));
 L->Textures.Add(TEXT("T_WestFieldFence"),B);L->Textures.Add(TEXT("T_WestFieldFenceN"),N);
 for(int I=0;I<3;++I)HallMakeTextured(L,*FString::Printf(TEXT("EX_WestFieldFence%d"),I),B,N,.96f,I==0?FLinearColor(1.08f,1.08f,1.04f):I==1?FLinearColor(.78f,.75f,.70f):FLinearColor(.98f,.90f,.78f),.75f);
 HallMakePlain(L,TEXT("EX_WestFieldNail"),FLinearColor(.12f,.087f,.062f),.83f,.25f);
 HallMakePlain(L,TEXT("EX_WestFieldSplit"),FLinearColor(.036f,.030f,.022f),.97f);
}
void Fence(FHallMesh& Solid,FHallMesh& Detail)
{
 FRandomStream R(20261012);int Posts=0,Boards=0;
 auto Run=[&](float Start,float End)
 {
  const int Bays=FMath::CeilToInt((End-Start)/245.f);TArray<FVector> Foot,Top;
  for(int I=0;I<=Bays;++I)
  {
   const float X=FMath::Lerp(Start,End,float(I)/Bays),Y=FenceY+R.FRandRange(-9,9),Z=HouseWoods::Height(X,Y);
   const FVector A(X,Y,Z-18),B(X+R.FRandRange(-6,6),Y+R.FRandRange(-7,7),Z+R.FRandRange(132,146));
   const FName K(*FString::Printf(TEXT("EX_WestFieldFence%d"),I%3));Plank(Solid,K,A,B,14,14);Foot.Add(FVector(X,Y,Z));Top.Add(B);++Posts;
   // Small exposed fasteners and end splits stay in a shadow-free detail batch.
   for(float H:{37.f,79.f,119.f})for(float S:{-1.f,1.f})Detail.Cylinder(TEXT("EX_WestFieldNail"),{X,Y+S*8,Z+H},{X,Y+S*8.6f,Z+H},1.25f,6,UV);
   Plank(Detail,TEXT("EX_WestFieldSplit"),B+FVector(-1,-7.3f,-23),B+FVector(1,-7.3f,-3),.55f,.3f);
  }
  for(int I=0;I<Bays;++I)for(int Row=0;Row<3;++Row)
  {
   // A few damaged lower rails, warped spans and uneven replacement boards.
   if(Row==0&&I%19==11)continue;
   const FVector A=Foot[I]+FVector(0,-2,34+Row*41),B=Foot[I+1]+FVector(0,-2,34+Row*41);
   const FVector Mid=(A+B)*.5f+FVector(0,R.FRandRange(-2,2),R.FRandRange(-5,2));
   const FName K(*FString::Printf(TEXT("EX_WestFieldFence%d"),(I+Row)%3));
   Plank(Solid,K,A,Mid,5,16);Plank(Solid,K,Mid,B,5,16);++Boards;
   if(Row==1&&I%13==4){const FVector C=FMath::Lerp(A,B,.7f);Plank(Detail,TEXT("EX_WestFieldSplit"),C+FVector(-19,-2.8f,-2),C+FVector(19,-2.8f,2),.7f,.3f);}
  }
 };
 Run(FenceWest,GateWest);Run(GateEast,FenceEast);
 // One fallen offcut gives wear without obstructing the usable opening.
 const float Z=HouseWoods::Height(GateEast+260,FenceY+85);
 Plank(Detail,TEXT("EX_WestFieldFence1"),{GateEast+205,FenceY+83,Z+4},{GateEast+330,FenceY+135,Z+8},5,14);
 UE_LOG(LogTemp,Display,TEXT("WEST_FIELD_FENCE posts=%d rails=%d opening=%.0fcm woodlandPathClear=1"),Posts,Boards,GateEast-GateWest);
}
}
