// Opt-in verification of the saved October 11 cleanup; no normal-play changes.
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
void TickHouseCleanupReview(UWorld* W)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("HouseCleanupWalkReview"))||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
 auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!P||!P->HasAuthority())return;
 struct FState{TWeakObjectPtr<UWorld> W;double Start=0,Step=0;int Phase=0;FString Dir,Log;TWeakObjectPtr<AExistingHouseDoor> Door;TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;};static FState S;
 static const FDelegateHandle MovementInput=FCoreDelegates::OnBeginFrame.AddLambda([]()
 {
  // This check is scheduled by the 4 Hz game-mode tick. Supply normal movement
  // input every rendered frame, as a held movement key does, rather than short
  // isolated impulses whose speed depends on the machine's frame rate.
  if(S.Pawn.IsValid()&&(S.Phase==3||S.Phase==4))S.Pawn->AddMovementInput(FVector(S.Phase==3?-1.f:1.f,0,0),1.f);
 });
 double Now=W->GetTimeSeconds();if(S.W.Get()!=W){S=FState();S.W=W;S.Pawn=P;S.Start=S.Step=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("HouseCleanupWalkReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](FString T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("HOUSE_CLEANUP_WALK %s"),*T);};
 auto Finish=[&](bool OK,FString T){Note((OK?TEXT("PASS "):TEXT("FAIL "))+T);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
 if(S.Phase<0)return;if(Now-S.Start>90){Finish(false,TEXT("Timed out"));return;}
 const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();constexpr float Floor=-361.966f,StartX=-1300.f,EndX=-1740.f,Y=728.f;
 if(S.Phase==0){if(Now-S.Start<8)return;
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CleanupFloor),false,P);
  for(TActorIterator<AExistingHouseDoor> It(W);It;++It)Q.AddIgnoredActor(*It); // Measure permanent floor support beneath the movable leaves.
  for(float X:{-1530.f,-1475.f,-1400.f,-1360.f})for(float Lane:{690.f,728.f,760.f})
   if(!W->LineTraceSingleByChannel(H,FVector(X,Lane,Floor+18),FVector(X,Lane,Floor-18),ECC_Pawn,Q)||H.ImpactNormal.Z<.7f){Finish(false,TEXT("Basement threshold floor support missing"));return;}
  Note(TEXT("All 12 basement connection floor samples supported"));
  for(float X:{-1518.f,1518.f})for(float Lane:{2100.f,2500.f,2900.f,3300.f,3700.f})
   if(!W->LineTraceSingleByChannel(H,FVector(X,Lane,-300),FVector(X,Lane,-410),ECC_Pawn,Q)||H.ImpactNormal.Z<.7f){Finish(false,TEXT("Court soil edge support missing"));return;}
  Note(TEXT("Both court soil edges have continuous collision at all 10 seam samples"));
  for(TActorIterator<AExistingHouseDoor> It(W);It;++It)if(It->DoorId==TEXT("SM_OriginalLeaf_Basement_p0_m2_Solid_01"))S.Door=*It;
  if(!S.Door.IsValid()){Finish(false,TEXT("Main basement stair door missing"));return;}
  P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(StartX,Y,Floor+Half+2),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetControlRotation((S.Door->DoorwayCenter-P->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=1;S.Step=Now;return;}
 if(S.Phase==1){if(Now-S.Step<.7)return;
  if(P->FocusedDoorForInteraction()!=S.Door.Get()){Finish(false,TEXT("Normal E targeting did not find basement stair door"));return;}
  PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.));Note(TEXT("Normal E opened the main basement stair door"));S.Phase=2;S.Step=Now;return;}
 if(S.Phase==2){if(Now-S.Step<1)return;FScreenshotRequest::RequestScreenshot(S.Dir/TEXT("BasementJoinOpen.png"),false,false);S.Phase=3;S.Step=Now;return;}
 if(S.Phase==3||S.Phase==4){const bool Into=S.Phase==3;const float Goal=Into?EndX:StartX,Sign=Into?-1.f:1.f;const FVector Feet=P->GetActorLocation()-FVector(0,0,Half);
  if(FMath::Abs(Feet.Z-Floor)>18||Now-S.Step>8){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CleanupBlocker),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+FVector(Sign*80,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q);Note(TEXT("Blocker ")+GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent())+TEXT(" at ")+H.ImpactPoint.ToString());if(auto* C=Cast<UStaticMeshComponent>(H.GetComponent()))Note(TEXT("Mesh ")+GetNameSafe(C->GetStaticMesh()));Finish(false,TEXT("Actual walking stalled at ")+Feet.ToString());return;}
  if(Sign*(Feet.X-Goal)>-12){P->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Player walked %s feet=%s"),Into?TEXT("into stairwell"):TEXT("back into basement"),*Feet.ToString()));++S.Phase;S.Step=Now;if(S.Phase==4)PC->SetControlRotation(FRotator(0,0,0));return;}
  return;}
 if(S.Phase==5&&Now-S.Step>.5)Finish(true,TEXT("Saved basement connection, normal E, actual two-way walking and both court soil seams passed. Multiplayer not exercised."));
}
#else
void TickHouseCleanupReview(UWorld*){}
#endif
