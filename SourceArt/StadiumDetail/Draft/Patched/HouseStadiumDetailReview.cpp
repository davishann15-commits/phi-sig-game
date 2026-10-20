// Opt-in additive stadium acceptance. Original reviewers/routes/tolerances remain byte-identical.
#include "HouseStadiumDetail.h"
#include "HouseExterior.h"
#include "HouseCalderTerrain.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#if !UE_BUILD_SHIPPING
namespace HouseStadiumDetailReviewImpl
{
FVector Mid(int32 K,float R,float Z)
{const float A=K*PI/20,B=(K+1)*PI/20;return {2400+(FMath::Cos(A)+FMath::Cos(B))*.5f*5600*R,-13300+(FMath::Sin(A)+FMath::Sin(B))*.5f*3800*R,Z};}
void Tick(UWorld* W,ELevelTick,float)
{
 if(!W||!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("StadiumDetailReview"))||W->GetTimeSeconds()<12||!HouseCalderTerrain::CollisionReady(W))return;
 static TWeakObjectPtr<UWorld> Done;if(Done.Get()==W)return;Done=W;bool OK=true;int32 Actors=0,Components=0,Instances=0,NewLights=0,Floors=0,Misses=0,Blocked=0,Sweeps=0;float MinTread=FLT_MAX,MaxError=0;TArray<FString> Problems;
 AHouseStadiumDetail* Stadium=nullptr;for(TActorIterator<AHouseStadiumDetail> It(W);It;++It){Stadium=*It;++Actors;}
 for(TActorIterator<AActor> It(W);It;++It){TInlineComponentArray<UActorComponent*> Parts(*It);for(const auto* C:Parts)if(C->GetName().StartsWith(TEXT("CalderStadium"))){++Components;if(C->IsA<ULightComponent>())++NewLights;if(const auto* H=Cast<UHierarchicalInstancedStaticMeshComponent>(C))Instances+=H->GetInstanceCount();}}
 const bool Enabled=HouseStadiumDetail::Enabled();OK&=NewLights==0&&(Enabled?(Actors==1&&Stadium&&Instances>1000):(Actors==0&&Components==0));
 FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderStadiumFloor),true);if(auto* PC=W->GetFirstPlayerController())if(PC->GetPawn())Q.AddIgnoredActor(PC->GetPawn());
 if(Enabled)
 {
  for(int32 K:{0,2,17,19,20,22,37,39})
  {
   const FVector Along=(Mid(K+1,.69f,0)-Mid(K-1,.69f,0)).GetSafeNormal(),Normal(Along.Y,-Along.X,0);
   FVector Prev[2];bool Have=false;
   // Analytic sampling at actual step centres, both sides of the centre rail. Require real blocking floors.
   auto Flight=[&](float R0,float R1,float Z0,float Z1,int32 Count)
   {
    const float Run=FMath::Abs(FVector::DotProduct(Mid(K,R1,0)-Mid(K,R0,0),Normal))/Count,Rise=(Z1-Z0)/Count;MinTread=FMath::Min(MinTread,Run);OK&=Run>=30.f&&Rise>=14.f&&Rise<=18.f;
    for(int32 J=0;J<Count;++J)
    {
     const float T=(J+.5f)/Count,Top=FMath::Lerp(Z0,Z1,float(J+1)/Count);const FVector C=Mid(K,FMath::Lerp(R0,R1,T),Top);
     for(int32 Side=0;Side<2;++Side){const FVector P=C+Along*(Side?80.f:-80.f);FHitResult H;const bool Hit=W->LineTraceSingleByChannel(H,P+FVector(0,0,6),P-FVector(0,0,10),ECC_Pawn,Q);const float Error=Hit?FMath::Abs(float(H.ImpactPoint.Z)-Top):1000;MaxError=FMath::Max(MaxError,Error);++Floors;if(!Hit||H.ImpactNormal.Z<.95f||Error>1.f){++Misses;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("floor segment=%d r0=%.4f step=%d side=%d expected=%s hit=%s normalZ=%.3f component=%s"),K,R0,J,Side,*P.ToString(),*H.ImpactPoint.ToString(),H.ImpactNormal.Z,*GetNameSafe(H.GetComponent())));}
      if(Have){const FVector Start=Prev[Side]+FVector(0,0,18+97),End=P+FVector(0,0,18+97);++Sweeps;if(W->SweepSingleByChannel(H,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("blocked segment=%d r0=%.4f step=%d side=%d start=%s end=%s hit=%s component=%s"),K,R0,J,Side,*Start.ToString(),*End.ToString(),*H.ImpactPoint.ToString(),*GetNameSafe(H.GetComponent())));}}
      Prev[Side]=P;
     }Have=true;
    }
   };
   Flight(.60f,.69f,-56,180,14);for(int32 Row=0;Row<11;++Row)Flight(.69f+Row*.024f,.714f+Row*.024f,180+Row*63.f,243+Row*63.f,4);Flight(.954f,1.025f,873,990,7);
  }
  auto CheckFloor=[&](FVector P,const TCHAR* Label){FHitResult H;++Floors;const bool Hit=W->LineTraceSingleByChannel(H,P+FVector(0,0,6),P-FVector(0,0,10),ECC_Pawn,Q);const float Error=Hit?FMath::Abs(float(H.ImpactPoint.Z-P.Z)):1000.f;MaxError=FMath::Max(MaxError,Error);if(!Hit||H.ImpactNormal.Z<.95f||Error>1.f){++Misses;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("floor %s at=%s hit=%s component=%s"),Label,*P.ToString(),*H.ImpactPoint.ToString(),*GetNameSafe(H.GetComponent())));}};
  for(int32 K:{4,16})
  {
   const FVector Along=(Mid(K+1,1,0)-Mid(K-1,1,0)).GetSafeNormal(),Normal(Along.Y,-Along.X,0);
   const float Run=FMath::Abs(FVector::DotProduct(Mid(K,.882f,0)-Mid(K,1.08f,0),Normal))/25;MinTread=FMath::Min(MinTread,Run);OK&=Run>=30.f;
   FVector Prev[2];bool Have=false;
   for(int32 J=0;J<25;++J)for(int32 Side=0;Side<2;++Side)
   {
    const FVector P=Mid(K,1.08f-.198f*(J+.5f)/25,-57+426.f*(J+1)/25)+Along*(Side?80.f:-80.f);CheckFloor(P,TEXT("new-portal-stair"));
    if(Have){FHitResult H;++Sweeps;if(W->SweepSingleByChannel(H,Prev[Side]+FVector(0,0,115),P+FVector(0,0,115),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(36,96),Q)){++Blocked;OK=false;if(Problems.Num()<32)Problems.Add(FString::Printf(TEXT("portal blocked segment=%d step=%d side=%d component=%s"),K,J,Side,*GetNameSafe(H.GetComponent())));}}
    Prev[Side]=P;if(Side==1)Have=true;
   }
   for(float R:{.80f,.82f,.84f,.86f})for(float Side:{-1.f,1.f})CheckFloor(Mid(K,R,369)+Along*(Side*80),TEXT("new-portal-landing"));
  }
  for(int32 K:{2,17})
  {
   const FVector Along=(Mid(K+1,1,0)-Mid(K-1,1,0)).GetSafeNormal();const float Side=K==2?1.f:-1.f;
   for(int32 J=0;J<3;++J)CheckFloor(Mid(K,.774f,369+(J+1)*15.75f)+Along*(Side*(235-(J+.5f)*35)),TEXT("cross-aisle-side-steps"));
   MinTread=FMath::Min(MinTread,35.f);
  }
  for(int32 K:{3,4,16})for(float T:{.1f,.3f,.5f,.7f,.9f})
  {
   const float A=K*PI/20,B=(K+1)*PI/20;const FVector L(2400+FMath::Cos(A)*5600*.774f,-13300+FMath::Sin(A)*3800*.774f,369),R(2400+FMath::Cos(B)*5600*.774f,-13300+FMath::Sin(B)*3800*.774f,369);
   CheckFloor(FMath::Lerp(L,R,T),TEXT("row-D-cross-aisle"));
  }
  if(HouseStadiumDetail::Step()>=4)for(int32 Row=0;Row<3;++Row)CheckFloor({5900,-14000+Row*100.,-56+Row*16.+18},TEXT("band-access-aisle"));
  if(HouseStadiumDetail::Step()>=6){MinTread=FMath::Min(MinTread,30.f);for(int32 Flight=1;Flight<=8;++Flight){const bool North=Flight%2==1;for(int32 Tread=0;Tread<8;++Tread){const double Y=North?-9490+(Tread+.5)*30:-9250-(Tread+.5)*30;CheckFloor({North?4888.:4772.,Y,-55+(Flight-1)*140.+(Tread+1)*17.5},TEXT("press-tower30cm-tread"));}}}
 }
 const FString Result=FString::Printf(TEXT("%s enabled=%d step=%d actors=%d components=%d instances=%d newLights=%d floorSamples=%d misses=%d maxFloorError=%.3f capsuleSweeps=%d blocked=%d minTread=%.3f\n"),OK?TEXT("PASS"):TEXT("FAIL"),int(Enabled),HouseStadiumDetail::Step(),Actors,Components,Instances,NewLights,Floors,Misses,MaxError,Sweeps,Blocked,Enabled?MinTread:0.f);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("StadiumDetailReview");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));FFileHelper::SaveStringArrayToFile(Problems,*(Dir/TEXT("problems.txt")));for(const FString& Problem:Problems)UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_PROBLEM %s"),*Problem);UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_REVIEW %s"),*Result);
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
