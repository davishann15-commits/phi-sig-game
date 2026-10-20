// Opt-in native terrain/fence acceptance; never runs during normal gameplay.
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "HouseWestField.h"
#include "HouseWoodsDetail.h"
#include "HouseExterior.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CoreDelegates.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"

void TickHouseWestFieldReview(UWorld* W)
{
 const bool Walk=FParse::Param(FCommandLine::Get(),TEXT("WestFieldWalkReview")),Scene=FParse::Param(FCommandLine::Get(),TEXT("WestFieldReview"));
 if((!Walk&&!Scene)||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
 auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!Pawn)return;
 struct FState{TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;int Phase=0,Leg=0;double Start=0,Step=0;FVector Goal;TArray<FVector2D> Route;FString Log,Dir;};static FState S;
 static const FDelegateHandle Input=FCoreDelegates::OnBeginFrame.AddLambda([](){if(S.Phase==1&&S.Pawn.IsValid())S.Pawn->AddMovementInput((S.Goal-S.Pawn->GetActorLocation()).GetSafeNormal2D(),1);});
 if(S.World.Get()!=W){S=FState();S.World=W;S.Pawn=Pawn;S.Start=S.Step=W->GetTimeSeconds();S.Dir=FPaths::ProjectSavedDir()/TEXT("WestFieldReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](const FString& Text){S.Log+=Text+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("WEST_FIELD_REVIEW %s"),*Text);};
 auto Finish=[&](bool Good,const FString& Message){Note((Good?TEXT("PASS "):TEXT("FAIL "))+Message);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;if(!Good||Walk)FPlatformMisc::RequestExitWithStatus(false,Good?0:1);};
 if(S.Phase<0)return;const double Now=W->GetTimeSeconds();const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 if(Now-S.Start>100){Finish(false,TEXT("Walking route timed out"));return;}
 if(S.Phase==0)
 {
  if(Now-S.Start<8)return;AHouseExteriorDetail* E=nullptr;for(TActorIterator<AHouseExteriorDetail> It(W);It;++It)E=*It;if(!E)return;
  FCollisionQueryParams Q(SCENE_QUERY_STAT(WestFieldGround),true,Pawn);FHitResult H;int Plateau=0,Slope=0,Mound=0,Foliage=0;
  auto Ground=[&](float X,float Y,float Expected,float Tolerance){return W->LineTraceSingleByChannel(H,{X,Y,Expected+90},{X,Y,Expected-90},ECC_Pawn,Q)&&FMath::Abs(H.ImpactPoint.Z-Expected)<Tolerance&&H.ImpactNormal.Z>.70f;};
  for(float X:{-17500.f,-12500.f,-7000.f,-4400.f})for(float Y:{2100.f,2800.f,3700.f})Plateau+=int(Ground(X,Y,HouseWestField::MoundTop,6));
  for(float X:{-12000.f,-6500.f})for(float Y:{4450.f,4595.f,4605.f,5000.f,5500.f,6000.f,6490.f})Slope+=int(Ground(X,Y,HouseWoods::Height(X,Y),10));
  for(float X:{-2800.f,-2950.f,-3100.f,-3250.f,-3400.f,-3550.f,-3700.f,-4200.f})Mound+=int(Ground(X,2800,HouseWestField::MoundTop,5));
  bool Paths=Ground(-3480,700,-60-302*700/1600.f+3,3)&&Ground(-3480,1480,-60-302*1480/1600.f+3,3)&&Ground(-2500,1630,-359,3);
  UProceduralMeshComponent* Fence=nullptr;bool FenceSafe=false;
  TInlineComponentArray<UPrimitiveComponent*> Parts(E);FCollisionQueryParams FQ(SCENE_QUERY_STAT(WestFieldFence),true);
  for(TActorIterator<AActor> It(W);It;++It)if(*It!=E)FQ.AddIgnoredActor(*It);
  for(auto* P:Parts)if(P->GetName()==TEXT("ExteriorWestFieldFence"))Fence=Cast<UProceduralMeshComponent>(P);else FQ.AddIgnoredComponent(P);
  if(Fence)FenceSafe=!Fence->CastShadow&&!Fence->bAffectDynamicIndirectLighting&&Fence->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
  const auto Capsule=FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half);
  int Blocked=0;for(float X:{-18800.f,-13000.f,-9500.f,-8100.f,-6000.f,-4300.f})
  {const float Z=HouseWoods::Height(X,HouseWestField::FenceY)+Half+2;Blocked+=int(W->SweepSingleByChannel(H,{X,HouseWestField::FenceY-160,Z},{X,HouseWestField::FenceY+160,Z},FQuat::Identity,ECC_Pawn,Capsule,FQ)&&H.GetComponent()==Fence);}
  const float GX=(HouseWestField::GateWest+HouseWestField::GateEast)*.5f,GZ=HouseWoods::Height(GX,HouseWestField::FenceY)+Half+3;
  const bool Opening=!W->SweepSingleByChannel(H,{GX,HouseWestField::FenceY-200,GZ},{GX,HouseWestField::FenceY+200,GZ},FQuat::Identity,ECC_Pawn,Capsule,FQ);
  TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Plants(E);bool Fitted=true;
  for(auto* P:Plants)if(P->GetName().StartsWith(TEXT("ExteriorTree"))||P->GetName().StartsWith(TEXT("ExteriorGrass")))for(int I=0;I<P->GetInstanceCount();++I)
  {FTransform T;P->GetInstanceTransform(I,T,true);const FVector V=T.GetLocation();if(V.X<-4100&&V.X>-7900&&V.Y>1700&&V.Y<5900){++Foliage;const float Error=FMath::Abs(V.Z-HouseWoods::Height(V.X,V.Y));if(Error>=12&&Fitted)Note(TEXT("Plant height mismatch ")+P->GetName()+TEXT(" ")+V.ToString()+FString::Printf(TEXT(" expectedZ=%.1f"),HouseWoods::Height(V.X,V.Y)));Fitted&=Error<12;}}
  Note(FString::Printf(TEXT("plateau=%d/12 woodsSlopeAndSeam=%d/14 moundJoin=%d/8 perimeterPathsClear=%d fenceBlocked=%d/6 gateClear=%d fittedFieldPlants=%d plantsAtGrade=%d"),Plateau,Slope,Mound,int(Paths),Blocked,int(Opening),Foliage,int(Fitted)));
  if(Plateau!=12||Slope!=14||Mound!=8||!Paths||Blocked!=6||!Opening||!FenceSafe||!Fitted||Foliage<100){Finish(false,TEXT("Actual terrain/fence checks failed"));return;}
  if(!Walk){Finish(true,TEXT("Field matches west mound; slope has collision, wooden fence blocks and openings remain clear"));return;}
  S.Route={{-4500,2900},{-5700,3200},{GX,3800},{GX,4200},{GX,4830},{GX,5480},{-6800,5900}};S.Leg=1;S.Step=Now;
  const auto P=S.Route[0];Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation({P.X,P.Y,HouseWoods::Height(P.X,P.Y)+Half+3},false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);S.Goal=FVector(S.Route[1],0);S.Phase=1;return;
 }
 if(S.Phase==1)
 {
  const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);PC->SetControlRotation((S.Goal-Feet).Rotation());
  if(Now-S.Step>15||Feet.Z<HouseWoods::Height(Feet.X,Feet.Y)-35){Finish(false,TEXT("Walk stalled or dropped at ")+Feet.ToString());return;}
  if(FVector::Dist2D(Feet,S.Goal)<45){Pawn->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Walk leg %d reached %s"),S.Leg,*Feet.ToString()));++S.Leg;S.Step=Now;
   if(S.Leg>=S.Route.Num()){FScreenshotRequest::RequestScreenshot(S.Dir/TEXT("FieldFenceWoodsWalk.png"),false,false);S.Phase=2;return;}S.Goal=FVector(S.Route[S.Leg],0);}
 }
 if(S.Phase==2&&Now-S.Step>1)Finish(true,TEXT("Player walked across the raised field, through the fence opening and down into the woods without a stall or ground drop. Single-player check."));
}
#else
void TickHouseWestFieldReview(UWorld*){}
#endif
