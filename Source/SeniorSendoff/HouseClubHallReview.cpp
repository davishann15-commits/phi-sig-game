// Development-only acceptance of the saved finite club wing. No normal-play changes.
#include "StoryCampaign.h"
#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "HouseImpossibleHallway.h"
#include "HouseNightClub.h"
#include "HouseClubPole.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "InputKeyEventArgs.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
void TickHouseClubHallReview(UWorld* W)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("ClubHallWalkReview"))||!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
 auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!P||!P->HasAuthority())return;
 struct FState{TWeakObjectPtr<UWorld> W;TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;TWeakObjectPtr<AExistingHouseDoor> Door;TWeakObjectPtr<AHouseNightClub> Club;double Start=0,Step=0;int Phase=0,Leg=0,Frames=0;float MaxStep=0;FVector Goal=FVector::ZeroVector,Previous=FVector::ZeroVector;FString Dir,Log;};static FState S;
 static const FDelegateHandle Input=FCoreDelegates::OnBeginFrame.AddLambda([]()
 {
  if(S.Pawn.IsValid()&&S.Phase==3){auto* Pawn=S.Pawn.Get();const FVector Pos=Pawn->GetActorLocation();S.MaxStep=FMath::Max(S.MaxStep,float(FVector::Dist2D(Pos,S.Previous)));S.Previous=Pos;++S.Frames;Pawn->AddMovementInput((S.Goal-Pos).GetSafeNormal2D(),1.f);}
 });
 const double Now=W->GetTimeSeconds();if(S.W.Get()!=W){S=FState();S.W=W;S.Pawn=P;S.Start=S.Step=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("ClubHallWalkReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](FString T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("CLUB_HALL_WALK %s"),*T);};
 auto Finish=[&](bool OK,FString T){Note((OK?TEXT("PASS "):TEXT("FAIL "))+T);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
 if(S.Phase<0)return;if(Now-S.Start>100){Finish(false,TEXT("Timed out"));return;}
 const float Half=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();constexpr float Floor=333.1f;
 static const FVector Route[]={FVector(-2395,-1780,Floor),FVector(-2190,-1830,Floor),FVector(-1860,-1830,Floor),FVector(-1800,-1430,Floor),FVector(-2110,-1370,Floor),FVector(-2050,-1680,Floor),FVector(-2395,-1780,Floor),FVector(-2395,-1400,Floor)};
 if(S.Phase==0){if(Now-S.Start<8)return;
  if(TActorIterator<AHouseImpossibleHallway> It{W};It){Finish(false,TEXT("Hallway teleport actor still present"));return;}
  for(TActorIterator<AHouseNightClub> It(W);It;++It)S.Club=*It;
  for(TActorIterator<AExistingHouseDoor> It(W);It;++It)if(It->DoorId==TEXT("HallEnd_End"))S.Door=*It;
  if(!S.Club.IsValid()||!S.Door.IsValid()){Finish(false,TEXT("Club or preserved hall door missing"));return;}
  auto* Club=S.Club.Get();if(Club->ClubLights.Num()!=6||Club->UpperExtraRegions.Num()!=3){Finish(false,TEXT("Club volume or real-light budget mismatch"));return;}
  for(auto L:Club->ClubLights)if(!L||L->CastShadows){Finish(false,TEXT("Club light unexpectedly casts shadows"));return;}
  FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(ClubHallFloor),false,P);
  for(float X:{-2690.f,-2400.f,-2100.f,-1800.f,-1470.f})for(float Y:{-2160.f,-2000.f,-1770.f,-1565.f})
   if(!W->LineTraceSingleByChannel(H,FVector(X,Y,Floor+14),FVector(X,Y,Floor-15),ECC_Pawn,Q)||H.ImpactNormal.Z<.7f){Finish(false,TEXT("New floor support missing at ")+FVector(X,Y,Floor).ToString());return;}
  for(const FVector Point:{FVector(-2400,-1840,490),FVector(-1850,-1830,490),FVector(-1800,-1380,490)})if(Club->GetMusicGainAtWorld(Point)<.9f){Finish(false,TEXT("Music missing in connected club region"));return;}
  for(const FVector Point:{FVector(-2395,-1400,490),FVector(-2970,-1000,490),FVector(-1900,-2300,490)})if(Club->GetMusicGainAtWorld(Point)>.001f){Finish(false,TEXT("Music leaks into hallway or neighboring room"));return;}
  const float G0=Club->GetMusicGainAtWorld(FVector(-2395,-1525,490)),G1=Club->GetMusicGainAtWorld(FVector(-2395,-1575,490)),G2=Club->GetMusicGainAtWorld(FVector(-2395,-1675,490));
  if(!(G0<G1&&G1<G2&&G2>.85f)){Finish(false,TEXT("New entry fade not gradual"));return;}
  Note(TEXT("Teleport actor absent; 20 floor samples solid; six shadow-free club lights; music confined and entry fade gradual"));
  P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(-2395,-1400,Floor+Half+2),false,nullptr,ETeleportType::TeleportPhysics);P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetControlRotation((S.Door->DoorwayCenter-P->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=1;S.Step=Now;return;}
 if(S.Phase==1){if(Now-S.Step<.7)return;if(P->FocusedDoorForInteraction()!=S.Door.Get()){Finish(false,TEXT("Normal E did not target preserved doorway"));return;}
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));Note(TEXT("Normal E opened the former hallway room door"));S.Phase=2;S.Step=Now;return;}
 if(S.Phase==2){if(Now-S.Step<1)return;S.Phase=3;S.Step=Now;S.Goal=Route[0];S.Previous=P->GetActorLocation();return;}
 if(S.Phase==3){const FVector Feet=P->GetActorLocation()-FVector(0,0,Half);
  if(FMath::Abs(Feet.Z-Floor)>18||Now-S.Step>9||S.MaxStep>100){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(ClubHallBlocker),false,P);W->SweepSingleByChannel(H,P->GetActorLocation(),P->GetActorLocation()+(S.Goal-P->GetActorLocation()).GetSafeNormal2D()*90,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(P->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q);Note(TEXT("Blocker ")+GetNameSafe(H.GetActor())+TEXT("/")+GetNameSafe(H.GetComponent()));Finish(false,TEXT("Walk stalled, floor dropped or teleport detected at ")+Feet.ToString());return;}
  if(FVector::Dist2D(Feet,S.Goal)<36){P->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Walk leg %d reached %s"),S.Leg,*Feet.ToString()));++S.Leg;S.Step=Now;if(S.Leg==UE_ARRAY_COUNT(Route)){Finish(true,FString::Printf(TEXT("Eight actual walking legs through both joins and back through the normal door; frames=%d maximumStep=%.2f cm"),S.Frames,S.MaxStep));return;}S.Goal=Route[S.Leg];PC->SetControlRotation((S.Goal-Feet).Rotation());}
 }
}
#else
void TickHouseClubHallReview(UWorld*){}
#endif
