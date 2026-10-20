// Explicit development-only door/furniture checks; no fixtures or map changes are saved.
#include "ExistingHouseDoor.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseInteractiveProp.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
FBox DoorBounds(AExistingHouseDoor* D)
{
 if(D->PanelHalfExtent.X>1)return FBox(D->GetPanelCollision()->GetRelativeLocation()-D->GetPanelCollision()->GetUnscaledBoxExtent(),D->GetPanelCollision()->GetRelativeLocation()+D->GetPanelCollision()->GetUnscaledBoxExtent());
 return D->GetOriginalLeafComponent()->GetStaticMesh()->GetBoundingBox().TransformBy(D->GetOriginalLeafComponent()->GetRelativeTransform());
}
FTransform PivotAt(AExistingHouseDoor* D,float Yaw)
{
 FTransform T=D->GetLeafPivot()->GetRelativeTransform();T.SetRotation(FRotator(0,Yaw,0).Quaternion());return T*D->GetRootComponent()->GetComponentTransform();
}
void PlaceOutsideSwing(AStoryFirstPersonCharacter* P,AExistingHouseDoor* D)
{
 const FBox B=DoorBounds(D);const FVector C=PivotAt(D,D->ClosedYaw).TransformPosition(B.GetCenter());
 const FVector O=PivotAt(D,D->ClosedYaw+FMath::FindDeltaAngleDegrees(D->ClosedYaw,D->OpenYaw)*.5f).TransformPosition(B.GetCenter());
 FVector N=(O-C).GetSafeNormal2D();
 FVector At=D->DoorwayCenter-N*225;
 if(D->DoorGroup==TEXT("FrontWalnutDoubleDoors"))At=FVector(-36.65,-1180,0);At.Z=D->GetActorLocation().Z+P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3;
 P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);P->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);
 if(auto* PC=Cast<APlayerController>(P->GetController()))PC->SetControlRotation((D->DoorwayCenter-P->FirstPersonCamera->GetComponentLocation()).Rotation());
}
}

void TickHouseDoorSwingReview(UWorld* W)
{
 const bool Audit=FParse::Param(FCommandLine::Get(),TEXT("DoorSwingAudit"));
 if(!Audit&&!FParse::Param(FCommandLine::Get(),TEXT("DoorSwingReview")))return;
 if(!W||!W->IsGameWorld()||W->GetNetMode()==NM_Client||W->GetMapName().Contains(TEXT("Lobby")))return;
 auto* PC=W->GetFirstPlayerController();auto* P=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!P)return;
 struct FState{TWeakObjectPtr<UWorld> W;TArray<TWeakObjectPtr<AExistingHouseDoor>> Doors;int Phase=0,Index=0;double Start=0,Step=0;FString Log,Dir;TWeakObjectPtr<AHouseInteractiveProp> Chair;float Partial=0;FTransform ChairAt;bool InsertWhileMoving=false;};
 static FState S;
 static const FDelegateHandle Insert=FCoreDelegates::OnBeginFrame.AddLambda([](){
  if(!S.InsertWhileMoving||!S.Chair.IsValid()||!S.Doors.IsValidIndex(S.Index))return;
  auto* Door=S.Doors[S.Index].Get();if(!Door)return;
  if(FMath::Abs(FMath::FindDeltaAngleDegrees(Door->ClosedYaw,Door->GetLeafPivot()->GetRelativeRotation().Yaw))>8){S.Chair->SetActorTransform(S.ChairAt,false,nullptr,ETeleportType::TeleportPhysics);S.InsertWhileMoving=false;UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_REVIEW CHAIR_INSERTED_DURING_SWING"));}
 });
 double Now=W->GetTimeSeconds();
 if(S.W.Get()!=W){S=FState();S.W=W;S.Start=S.Step=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("DoorSwingReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
 auto Note=[&](const FString& T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_REVIEW %s"),*T);};
 auto Finish=[&](bool OK,const FString& T){Note((OK?TEXT("PASS "):TEXT("FAIL "))+T);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
 if(S.Phase<0)return;if(Now-S.Start>240){Finish(false,TEXT("Timeout"));return;}
 if(S.Phase==0){if(Now-S.Start<8)return;
  for(TActorIterator<AExistingHouseDoor> It(W);It;++It)if(!It->bLocked){
   if(Audit||It->DoorId==TEXT("FrontWalnut_Left")||It->DoorId==TEXT("NightClubLower")||It->DoorId==TEXT("NW_MainStairDoor")||It->DoorId==TEXT("SM_OriginalLeaf_Basement_p0_m2_Solid_01"))S.Doors.Add(*It);}
  S.Doors.Sort([](const auto& A,const auto& B){return A->DoorId.ToString()<B->DoorId.ToString();});
  if(S.Doors.IsEmpty()){Finish(false,TEXT("No doors"));return;}S.Phase=1;S.Step=Now;return;}
 AExistingHouseDoor* D=S.Doors.IsValidIndex(S.Index)?S.Doors[S.Index].Get():nullptr;
 if(!D){Finish(true,Audit?TEXT("Housewide swing audit recorded"):TEXT("Single, double, club and basement doors stop at real furniture contact, reverse and reopen after clearing"));return;}
 auto Yaw=[&](){return D->GetLeafPivot()->GetRelativeRotation().Yaw;};
 auto Full=[&](float Want){return FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw(),Want))<.2f;};
 if(S.Phase==1){PlaceOutsideSwing(P,D);S.Phase=11;S.Step=Now;return;}
 if(S.Phase==11){if(Now-S.Step<.6)return;
  if(!Audit&&D->DoorId==TEXT("FrontWalnut_Left")){
   auto* Focus=P->FocusedDoorForInteraction();if(!Focus||Focus->DoorGroup!=D->DoorGroup){Finish(false,TEXT("Normal E cannot focus front doors"));return;}
   PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));Note(TEXT("NORMAL_E front double door interaction"));S.Phase=2;S.Step=Now;return;}
  if(!D->Interact(P)){Finish(false,TEXT("Interaction refused for ")+D->DoorId.ToString());return;}S.Phase=2;S.Step=Now;return;}
 if(S.Phase==2){if(Now-S.Step<1.1)return;Note(FString::Printf(TEXT("BASELINE %s yaw=%.2f open=%.2f settled=%d full=%d bounds=%s"),*D->DoorId.ToString(),Yaw(),D->OpenYaw,!D->IsActorTickEnabled(),Full(D->OpenYaw),*DoorBounds(D).ToString()));
  if(!Audit&&!Full(D->OpenYaw)){Finish(false,TEXT("Selected baseline door failed to open fully ")+D->DoorId.ToString());return;}
  D->Interact(P);S.Phase=3;S.Step=Now;return;}
 if(S.Phase==3){if(Now-S.Step<1.1)return;
  if(Audit){Note(FString::Printf(TEXT("RETURN %s yaw=%.2f closed=%.2f full=%d"),*D->DoorId.ToString(),Yaw(),D->ClosedYaw,Full(D->ClosedYaw)));++S.Index;S.Phase=1;return;}
  if(!Full(D->ClosedYaw)){Finish(false,TEXT("Baseline close failed"));return;}
  AHouseInteractiveProp* Source=nullptr;for(TActorIterator<AHouseInteractiveProp> It(W);It;++It)if(It->PropName.Contains(TEXT("chair"))&&!It->IsClothFlag()&&It->IntactMesh){Source=*It;break;}
  if(!Source){Finish(false,TEXT("No real chair mesh"));return;}
  FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Chair=W->SpawnActor<AHouseInteractiveProp>(AHouseInteractiveProp::StaticClass(),FTransform::Identity,Params);Chair->IntactMesh=Source->IntactMesh;Chair->PropName=TEXT("door swing review chair");Chair->bMovable=false;Chair->bBreakable=false;Chair->SetActorScale3D(Source->GetActorScale3D());Chair->RefreshProp();S.Chair=Chair;
  const FBox B=DoorBounds(D);FVector Tip=B.GetCenter();
  if(B.GetExtent().X>B.GetExtent().Y)Tip.X=(FMath::Abs(B.Min.X)>FMath::Abs(B.Max.X)?B.Min.X:B.Max.X)*.85f;else Tip.Y=(FMath::Abs(B.Min.Y)>FMath::Abs(B.Max.Y)?B.Min.Y:B.Max.Y)*.85f;
  const float Mid=D->ClosedYaw+FMath::FindDeltaAngleDegrees(D->ClosedYaw,D->OpenYaw)*.65f;
  FVector At=PivotAt(D,Mid).TransformPosition(Tip);At.Z=D->GetActorLocation().Z+45;
  Chair->SetActorLocation(At-(Chair->Body->Bounds.Origin-Chair->GetActorLocation()),false,nullptr,ETeleportType::TeleportPhysics);
  Note(FString::Printf(TEXT("CHAIR %s mesh=%s center=%s extent=%s"),*D->DoorId.ToString(),*GetNameSafe(Chair->IntactMesh),*Chair->Body->Bounds.Origin.ToString(),*Chair->Body->Bounds.BoxExtent.ToString()));
  S.ChairAt=Chair->GetActorTransform();
  if(D->DoorId==TEXT("NightClubLower")){Chair->SetActorLocation(Chair->GetActorLocation()+FVector(10000,0,0));S.InsertWhileMoving=true;}
  PlaceOutsideSwing(P,D);D->Interact(P);S.Phase=4;S.Step=Now;return;}
 if(S.Phase==4){if(Now-S.Step<1.1)return;S.Partial=Yaw();const float Travel=FMath::Abs(FMath::FindDeltaAngleDegrees(D->ClosedYaw,Yaw())),Total=FMath::Abs(FMath::FindDeltaAngleDegrees(D->ClosedYaw,D->OpenYaw));
  Note(FString::Printf(TEXT("BLOCKED %s travelled=%.2f total=%.2f yaw=%.2f settled=%d"),*D->DoorId.ToString(),Travel,Total,Yaw(),!D->IsActorTickEnabled()));
  if(Travel<8||Travel>Total-8||D->IsActorTickEnabled()){Finish(false,TEXT("Chair did not stop door at an intermediate angle"));return;}
  const FBox B=DoorBounds(D);FTransform T=PivotAt(D,Yaw());FVector Ext=B.GetExtent()*T.GetScale3D().GetAbs();Ext-=FVector(1,1,1.5);
  if(S.Chair->Body->OverlapComponent(T.TransformPosition(B.GetCenter()),T.GetRotation(),FCollisionShape::MakeBox(Ext))){Finish(false,TEXT("Stopped door overlaps chair collision"));return;}
  FScreenshotRequest::RequestScreenshot(S.Dir/(D->DoorId.ToString()+TEXT("-blocked.png")),false,false);
  S.Phase=41;S.Step=Now;return;}
 if(S.Phase==41){if(Now-S.Step<.35)return;D->Interact(P);S.Phase=5;S.Step=Now;return;}
 if(S.Phase==5){if(Now-S.Step<1.1)return;if(!Full(D->ClosedYaw)){Finish(false,TEXT("Door cannot reverse away from furniture"));return;}
  S.Chair->SetActorLocation(S.Chair->GetActorLocation()+FVector(10000,0,0));D->Interact(P);S.Phase=6;S.Step=Now;return;}
 if(S.Phase==6){if(Now-S.Step<1.1)return;if(!Full(D->OpenYaw)){Finish(false,TEXT("Cleared furniture still blocks door"));return;}Note(TEXT("CLEAR_AND_REOPEN ")+D->DoorId.ToString());S.Chair->SetActorTransform(S.ChairAt,false,nullptr,ETeleportType::TeleportPhysics);D->Interact(P);S.Phase=7;S.Step=Now;return;}
 if(S.Phase==7){if(Now-S.Step<1.1)return;const float Travel=FMath::Abs(FMath::FindDeltaAngleDegrees(D->ClosedYaw,Yaw())),Total=FMath::Abs(FMath::FindDeltaAngleDegrees(D->ClosedYaw,D->OpenYaw));
  Note(FString::Printf(TEXT("CLOSING_BLOCKED %s yaw=%.2f"),*D->DoorId.ToString(),Yaw()));
  if(Travel<.5f||Travel>Total-.5f||D->IsActorTickEnabled()){Finish(false,TEXT("Closing door passed through furniture"));return;}
  const FBox B=DoorBounds(D);FTransform T=PivotAt(D,Yaw());FVector Ext=B.GetExtent()*T.GetScale3D().GetAbs();Ext-=FVector(1,1,1.5);
  if(S.Chair->Body->OverlapComponent(T.TransformPosition(B.GetCenter()),T.GetRotation(),FCollisionShape::MakeBox(Ext))){Finish(false,TEXT("Closing stopped door overlaps chair"));return;}
  D->Interact(P);S.Phase=8;S.Step=Now;return;}
 if(S.Phase==8){if(Now-S.Step<1.1)return;if(!Full(D->OpenYaw)){Finish(false,TEXT("Closing door cannot reverse away from furniture"));return;}S.Chair->Destroy();S.Chair=nullptr;D->Interact(P);S.Phase=9;S.Step=Now;return;}
 if(S.Phase==9){if(Now-S.Step<1.1)return;if(!Full(D->ClosedYaw)){Finish(false,TEXT("Cleared door did not close"));return;}++S.Index;S.Phase=1;S.Step=Now;}
}
#else
void TickHouseDoorSwingReview(UWorld*){}
#endif
