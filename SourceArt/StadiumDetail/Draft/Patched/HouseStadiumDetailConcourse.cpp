#include "HouseStadiumDetail.h"
#include "HouseCampusFallKit.h"
namespace HouseStadiumDetailConcourseImpl
{
struct FFrame
{FVector C,Along,N;float Length;FFrame(int32 K,float R){const float A=K*PI/20,B=(K+1)*PI/20;const FVector P(2400+FMath::Cos(A)*5600*R,-13300+FMath::Sin(A)*3800*R,0),Q(2400+FMath::Cos(B)*5600*R,-13300+FMath::Sin(B)*3800*R,0);C=(P+Q)*.5f;Along=(Q-P).GetSafeNormal();N={Along.Y,-Along.X,0};Length=FVector::Dist(P,Q);}FVector At(float U,float D,float Z){return C+Along*U+N*D+FVector(0,0,Z);}};
void Trophy(FHallMesh& M,FVector Base,FVector Across,float Height)
{
 const float Scale=Height/36.f;const TArray<FVector2D> Profile={{0,23*Scale},{6*Scale,24*Scale},{11.8f*Scale,33*Scale},{12.8f*Scale,35*Scale},{14*Scale,34*Scale},{11*Scale,25*Scale},{2*Scale,15*Scale},{2*Scale,5*Scale},{8*Scale,3*Scale},{8*Scale,0},{0,0}};
 HouseCampusFallKit::Lathe(M,TEXT("SD_Gold"),Base,Profile,12);
 for(float Side:{-1.f,1.f})for(int32 J=0;J<8;++J){const float A=-HALF_PI+J*PI/8,B=-HALF_PI+(J+1)*PI/8;auto P=[&](float Angle){return Base+Across*(Side*(12+FMath::Cos(Angle)*7)*Scale)+FVector(0,0,(25+FMath::Sin(Angle)*8)*Scale);};M.Cylinder(TEXT("SD_Gold"),P(A),P(B),.9f*Scale,6,FHallUV::World(100));}
}
}
void AHouseStadiumDetail::Concourse()
{
 namespace C=HouseStadiumDetailConcourseImpl;
 // Keep the exact Gate4/5 corridors clear: hardware/rails stay against the wall to the side of each224cm opening.
 for(int32 K:{8,10})
 {
  C::FFrame F(K,1.f);const FRotator Rot=F.Along.Rotation();
  for(float Side:{-1.f,1.f})
  {
   const FVector P=F.At(Side*245,100,-57);Box(TEXT("SD_Steel"),P+FVector(0,0,50),{35,75,100},Rot);for(float Z:{75.f,95.f})Pipe(TEXT("SD_Aluminium"),P+F.N*38+FVector(0,0,Z),P+F.N*80+FVector(0,0,Z),2);
   // Short queue wing leaves the centre and the accepted north-plaza FreeRoam line unencumbered.
   Rail(F.At(Side*290,30,-57),F.At(Side*420,30,-57));
  }
 }
 C::FFrame Plaque(9,1.f);Sign(TEXT("SD_Dedication"),Plaque.At(-160,10,225),-Plaque.Along,Plaque.N,185,66);Box(TEXT("SD_Steel"),Plaque.At(-160,6,225),{194,6,74},Plaque.Along.Rotation());
 // A glazed display mounted to an existing outside wall; no extra rooms or interior collision volume.
 C::FFrame Fame(13,1.f);const FVector Mid=Fame.At(0,29,170);const FRotator CaseRot=Fame.Along.Rotation();
 Box(TEXT("SD_Dark"),Mid-Fame.N*16,{280,4,190},CaseRot);
 for(float S:{-1.f,1.f}){Box(TEXT("SD_Wood"),Mid+Fame.Along*(S*137),{6,35,190},CaseRot);Box(TEXT("SD_Wood"),Mid+FVector(0,0,S*92),{280,35,6},CaseRot);}
 Sign(TEXT("SD_HallOfFame"),Mid+Fame.N*18+FVector(0,0,69),-Fame.Along,Fame.N,266,46);
 for(float Z:{104.f,162.f})Box(TEXT("SD_Wood"),Fame.At(0,30,Z),{266,31,3},CaseRot);
 for(int32 I=0;I<3;++I){const float U=(I-1)*81.f;C::Trophy(Unique,Fame.At(U,32,163.5f),Fame.Along,32+I*4.f);Box(TEXT("SD_Purple"),Fame.At(U,32,111),{55,10,11},CaseRot);Letter(FString::Printf(TEXT("%d"),1948+I*15),Fame.At(U,38,111),-Fame.Along,Fame.N,6,TEXT("SD_Cream"));}
 // The pane and frame collide; the objects behind glass are unreachable and share the merged material mesh.
 Box(TEXT("SD_Glass"),Mid+Fame.N*18+FVector(0,0,-21),{266,1,132},CaseRot);
 // Cork bulletin board and thumb-tacked period flyers; invented names only.
 C::FFrame Notices(7,1.f);Box(TEXT("SD_Wood"),Notices.At(-260,23,195),{195,18,135},Notices.Along.Rotation());Sign(TEXT("SD_Flyers"),Notices.At(-260,33,195),-Notices.Along,Notices.N,180,120);
 // Overflow at two existing wall recesses, never in Gate5 or STA's exact approach.
 FRandomStream Rand(2417);for(int32 K:{13,15}){C::FFrame F(K,1.025f);const FVector P=F.At(0,0,-57);Pipe(TEXT("SD_Steel"),P,P+FVector(0,0,85),32);Pipe(TEXT("SD_Dark"),P+FVector(0,0,83),P+FVector(0,0,85),28,false,18000);for(int32 I=0;I<22;++I){const FVector Cup=P+FVector(Rand.FRandRange(-27,27),Rand.FRandRange(-27,27),Rand.FRandRange(80,97));Pipe(TEXT("SD_Cream"),Cup,Cup+FVector(0,0,11),3.7f,false,9000);}for(int32 I=0;I<8;++I)Box(TEXT("SD_Cream"),P+FVector(Rand.FRandRange(-75,75),Rand.FRandRange(-75,75),.3f),{25,18,.4f},FRotator(0,I*37,0),false,9000);}
}
