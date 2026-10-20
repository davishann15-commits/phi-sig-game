// Opt-in saved-map regression check; no normal gameplay changes.
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "HouseBlackHall.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
void TickHouseJoinRepairReview(UWorld* W)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("HouseJoinWalkReview"))||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
 auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!P||!P->HasAuthority())return;
 struct FState{TWeakObjectPtr<UWorld> W;double Start=0,Step=0;int Phase=0,Route=0;FString Dir,Log;TWeakObjectPtr<AExistingHouseDoor> Stair;};static FState S;
 double Now=W->GetTimeSeconds();if(S.W.Get()!=W){S=FState();S.W=W;S.Start=S.Step=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("HouseJoinWalkReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](FString T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("HOUSE_JOIN_WALK %s"),*T);};
 auto Finish=[&](bool OK,FString T){Note((OK?TEXT("PASS "):TEXT("FAIL "))+T);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
 if(S.Phase<0)return;if(Now-S.Start>90){Finish(false,TEXT("Timed out"));return;}
 const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 const FVector Center=S.Route==0?FVector(-2436.28,940.28,332.53):FVector(0,870.1,-361.9);
 const float StartY=S.Route==0?825.f:750.f,EndY=S.Route==0?1080.f:1100.f;
 auto Place=[&](){const FVector Center=S.Route==0?FVector(-2436.28,940.28,332.53):FVector(0,870.1,-361.9);const float StartY=S.Route==0?825.f:750.f;P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(Center.X,StartY,Center.Z+Half+2),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetControlRotation((Center+FVector(0,0,125)-P->FirstPersonCamera->GetComponentLocation()).Rotation());};
 auto E=[&](){PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.));};
 if(S.Phase==0){if(Now-S.Start<8)return;
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(JoinFloorSupport),false,P);
  for(auto Pair:{TPair<float,float>(-2436.28f,332.53f),TPair<float,float>(-2448.f,.99f)})for(float Y:{872.f,902.f,932.f})
   if(!W->LineTraceSingleByChannel(H,FVector(Pair.Key,Y,Pair.Value+18),FVector(Pair.Key,Y,Pair.Value-18),ECC_Pawn,Q)||H.ImpactNormal.Z<.7f){Finish(false,TEXT("Threshold support missing"));return;}
  Note(TEXT("All six repaired threshold floor samples have support"));
  for(TActorIterator<AExistingHouseDoor> It(W);It;++It)if(It->DoorId==TEXT("SM_OriginalLeaf_Upper_p0_m2_Solid_04"))S.Stair=*It;
  if(!S.Stair.IsValid()){Finish(false,TEXT("Stair door missing"));return;}Place();S.Phase=1;S.Step=Now;return;}
 if(S.Phase==1){if(Now-S.Step<.6)return;
  auto* Door=Cast<AExistingHouseDoor>(P->FocusedDoorForInteraction());
  if(!Door || (S.Route==0 && Door!=S.Stair.Get()) || (S.Route==1 && !Door->DoorGroup.ToString().StartsWith(TEXT("BH_PatioPair")))){Finish(false,TEXT("Normal E targeting did not find requested door"));return;}
  E();Note(S.Route==0?TEXT("Normal E interaction opened stair door"):TEXT("Normal E interaction opened basement French doors"));S.Phase=2;S.Step=Now;return;}
 if(S.Phase==2){if(Now-S.Step<1)return;FScreenshotRequest::RequestScreenshot(S.Dir/(S.Route==0?TEXT("StairThresholdOpen.png"):TEXT("BasementOpening.png")),false,false);S.Phase=3;S.Step=Now;return;}
 if(S.Phase==3||S.Phase==4){const bool Out=S.Phase==3;const float Goal=Out?EndY:StartY;const float Sign=Out?1:-1;
  const FVector Feet=P->GetActorLocation()-FVector(0,0,Half);
  if(FMath::Abs(Feet.Z-Center.Z)>18||Now-S.Step>8){
   FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(JoinWalkerBlocker),false,P);
   W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+FVector(0,Sign*80,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q);
   Note(TEXT("Blocker ")+GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent())+TEXT(" at ")+H.ImpactPoint.ToString()+TEXT(" normal ")+H.ImpactNormal.ToString());
   if(auto* C=Cast<UStaticMeshComponent>(H.GetComponent()))Note(TEXT("Blocker mesh ")+GetNameSafe(C->GetStaticMesh()));
   FScreenshotRequest::RequestScreenshot(S.Dir/TEXT("PassageBlocker.png"),false,false);
   Finish(false,TEXT("Passage walking stalled at ")+Feet.ToString());return;}
  if(Sign*(Feet.Y-Goal)>-12){P->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Player walked route=%d direction=%s feet=%s"),S.Route,Out?TEXT("out"):TEXT("return"),*Feet.ToString()));++S.Phase;S.Step=Now;if(S.Phase==4)PC->SetControlRotation(FRotator(0,-90,0));return;}
  P->AddMovementInput(FVector(0,Sign,0),1);return;}
 if(S.Phase==5){if(Now-S.Step<.4)return;
  if(S.Route==0){S.Route=1;Place();S.Phase=1;S.Step=Now;return;}
  Finish(true,TEXT("Saved stair thresholds supported, normal E door prompts, and actual two-way stair/basement passage walking passed. Multiplayer not exercised."));}
}
#else
void TickHouseJoinRepairReview(UWorld*){}
#endif
