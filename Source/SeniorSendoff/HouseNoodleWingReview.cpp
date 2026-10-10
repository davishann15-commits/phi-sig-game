// Focused opt-in traversal check. Never runs during an ordinary game.
#include "HouseCarryTiersReview.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "ExistingHouseDoor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

void TickHouseNoodleWingReview(AStoryFirstPersonCharacter* Pawn)
{
 if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
 UWorld* W=Pawn->GetWorld();
 if (!W || UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House")) return;
 APlayerController* PC=Cast<APlayerController>(Pawn->GetController()); if(!PC)return;
 struct FState { TWeakObjectPtr<UWorld> W; TArray<TArray<FVector>> Routes; int32 Phase=0, Way=1, Step=0, Frames=0, DoorCheck=0; TArray<TWeakObjectPtr<AExistingHouseDoor>> CheckDoors; double Start=0, LastProgress=0, LastShot=0, LastLog=0, Dt=0, DoorPauseUntil=0; float Best=1e9; FString Dir, CSV; TSet<FName> Doors; };
 static FState S;
 const double Now=W->GetTimeSeconds();
 auto* Move=Pawn->GetCharacterMovement(); const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 auto Finish=[&](bool Pass,const FString& Reason){
  FFileHelper::SaveStringToFile(S.CSV,*FPaths::Combine(S.Dir,TEXT("walk.csv")));
  const FString Summary=FString::Printf(TEXT("%s\n%s\nphase=%d way=%d doors=%d frames=%d mean_ms=%.2f\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Reason,S.Phase,S.Way,S.Doors.Num(),S.Frames,S.Frames?1000*S.Dt/S.Frames:0);
  FFileHelper::SaveStringToFile(Summary,*FPaths::Combine(S.Dir,TEXT("result.txt")));
  UE_LOG(LogTemp,Display,TEXT("NOODLE_WALK_%s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Summary);S.Step=-1;FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
 };
 if(S.W.Get()!=W){
  S=FState();S.W=W;S.Start=Now;
  if(auto* Settings=UGameUserSettings::GetGameUserSettings()){Settings->SetOverallScalabilityLevel(2);Settings->ApplyNonResolutionSettings();}S.Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NoodleWingReview"));IFileManager::Get().MakeDirectory(*S.Dir,true);
  FString File=FPaths::Combine(FPaths::ProjectDir(),TEXT("SourceArt/NoodleWing/walk.json"));FParse::Value(FCommandLine::Get(),TEXT("NoodleRoute="),File);FString Raw;TSharedPtr<FJsonObject> Json;
  if(!FFileHelper::LoadFileToString(Raw,*File)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Json)){Finish(false,TEXT("Cannot load route"));return;}
  for(auto& Group:Json->GetArrayField(TEXT("routes"))){TArray<FVector> Points;for(auto& Value:Group->AsArray()){auto P=Value->AsArray();Points.Add(FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()));}S.Routes.Add(Points);}
  S.CSV=TEXT("phase,way,time,x,y,feet_z,mode,frame_ms\n");
 }
 if(S.Step<0)return;
 if(S.Step==2){
  if(S.CheckDoors.IsEmpty()){
   FHitResult RailHit;FCollisionQueryParams RailQuery(SCENE_QUERY_STAT(NoodleUpperGuard),false,Pawn);
   const bool Guard=W->SweepSingleByChannel(RailHit,FVector(1600,-260,398),FVector(1600,-110,398),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()),RailQuery);
   if(!Guard || !RailHit.GetActor() || !RailHit.GetActor()->ActorHasTag(TEXT("NoodleWing"))){Finish(false,TEXT("Upper open edge guard does not block the player"));return;}
   UE_LOG(LogTemp,Display,TEXT("NOODLE_UPPER_GUARD_COLLISION_PASS %s"),*GetNameSafe(RailHit.GetComponent()));
   for(TActorIterator<AExistingHouseDoor>It(W);It;++It)if(S.Doors.Contains(It->DoorId))S.CheckDoors.Add(*It);
   if(S.CheckDoors.Num()!=5){Finish(false,TEXT("Expected five entrance doors"));return;}
  }
  if(S.DoorCheck>=S.CheckDoors.Num()){Finish(true,TEXT("All rooms and three stair floors walked both ways without recovery teleports. Five separate door checks passed from normal interaction range."));return;}
  auto* Door=S.CheckDoors[S.DoorCheck].Get();if(!Door){Finish(false,TEXT("Missing door"));return;}
  // Separate door check setup after the complete, uninterrupted walking routes.
  // Interaction has a 270 cm reach; never try to close a remote doorway.
  Pawn->SetActorLocation(Door->DoorwayCenter+FVector(170,0,-28),false,nullptr,ETeleportType::TeleportPhysics);
  Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Flying);
  if(!Door->IsOpen()){Finish(false,TEXT("Door was not opened on the route: ")+Door->DoorId.ToString());return;}
  if(!Door->Interact(Pawn)){Finish(false,TEXT("Close interaction rejected in range: ")+Door->DoorId.ToString());return;}
  S.Start=Now;S.Step=3;return;
 }
 if(S.Step==3){
  if(Now-S.Start<1.5)return;
  auto* Door=S.CheckDoors[S.DoorCheck].Get();FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(NoodleClosedDoor),false,Pawn);
  W->LineTraceSingleByChannel(Hit,Door->DoorwayCenter+FVector(100,0,0),Door->DoorwayCenter-FVector(100,0,0),ECC_Pawn,Q);
  if(Door->IsOpen()||Hit.GetActor()!=Door){Finish(false,TEXT("Closed doorway blocking failed: ")+Door->DoorId.ToString()+TEXT(" hit ")+GetNameSafe(Hit.GetActor()));return;}
  UE_LOG(LogTemp,Display,TEXT("NOODLE_DOOR_CLOSED_COLLISION_PASS %s"),*Door->DoorId.ToString());
  ++S.DoorCheck;S.Step=2;return;
 }
 if(S.DoorPauseUntil>Now){Move->StopMovementImmediately();return;}
 if(S.Step==0){
  if(Now-S.Start<6)return;
  Pawn->SetActorLocation(S.Routes[S.Phase][0]+FVector(0,0,Half+3),false,nullptr,ETeleportType::TeleportPhysics);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Walking);S.LastProgress=Now;S.Best=1e9;S.Way=1;S.Step=1;return;
 }
 const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);
 const FVector Goal=S.Routes[S.Phase][S.Way];FVector To=Goal-Feet;const float D=To.Size2D();
 for(TActorIterator<AExistingHouseDoor>It(W);It;++It){
  auto*Door=*It;
  const bool Relevant=Door->DoorId==TEXT("SM_OriginalLeaf_Basement_m1_p1_Solid_01") || Door->DoorId==TEXT("SM_OriginalLeaf_Upper_m1_p1_Solid_05") || Door->DoorId==TEXT("NW_LaundryDoor") || Door->DoorId==TEXT("NW_MainStairDoor") || Door->DoorId==TEXT("BH_ExitDoor");
  if(!Relevant||Door->bLocked||Door->IsOpen())continue;
  if(FVector::Dist2D(Door->DoorwayCenter,Feet)<210 && FMath::Abs(Door->DoorwayCenter.Z-Feet.Z-110)<95){
   if(Door->Interact(Pawn)){Move->StopMovementImmediately();S.DoorPauseUntil=Now+.8;S.LastProgress=Now+.8;S.Doors.Add(Door->DoorId);UE_LOG(LogTemp,Display,TEXT("NOODLE_DOOR_OPEN %s"),*Door->DoorId.ToString());}
  }
 }
 if(S.DoorPauseUntil>Now)return;
 S.Frames++;S.Dt+=W->GetDeltaSeconds();
 if(Now-S.LastLog>.12){S.LastLog=Now;S.CSV+=FString::Printf(TEXT("%d,%d,%.3f,%.2f,%.2f,%.2f,%d,%.2f\n"),S.Phase,S.Way,Now,Feet.X,Feet.Y,Feet.Z,(int)Move->MovementMode,1000*W->GetDeltaSeconds());}
 if(D<26 && FMath::Abs(To.Z)<32){
  UE_LOG(LogTemp,Display,TEXT("NOODLE_WAY_PASS phase=%d way=%d feet=%s"),S.Phase,S.Way,*Feet.ToCompactString());
  if(++S.Way>=S.Routes[S.Phase].Num()){
   if(++S.Phase>=S.Routes.Num()){
    S.Step=2;S.Start=Now;return;
   }
   S.Step=0;S.Start=Now-5.5;return;
  }
  S.LastProgress=Now;S.Best=1e9;return;
 }
 if(D<S.Best-3){S.Best=D;S.LastProgress=Now;}
 if(Now-S.LastProgress>7 || Feet.Z<-650){
  TArray<FHitResult>Hits;FCollisionQueryParams Q(SCENE_QUERY_STAT(NoodleStuck),false,Pawn);
  W->SweepMultiByChannel(Hits,Pawn->GetActorLocation(),Pawn->GetActorLocation()+To.GetSafeNormal2D()*95,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half-2),Q);
  for(auto&H:Hits)UE_LOG(LogTemp,Display,TEXT("NOODLE_BLOCKER actor=%s component=%s point=%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.ImpactPoint.ToCompactString());
  FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,TEXT("stuck.png")),false,false);
  Finish(false,FString::Printf(TEXT("Blocked at %s heading to %s"),*Feet.ToCompactString(),*Goal.ToCompactString()));return;
 }
 To.Z=0;Pawn->AddMovementInput(To.GetSafeNormal(),1);PC->SetControlRotation((To.GetSafeNormal()+FVector(0,0,.07)).Rotation());
 if(Now-S.LastShot>6){S.LastShot=Now;FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,FString::Printf(TEXT("route%d_way%02d.png"),S.Phase,S.Way)),false,false);}
}
#endif
