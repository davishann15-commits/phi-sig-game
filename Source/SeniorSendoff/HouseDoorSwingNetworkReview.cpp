// Opt-in two-process check through the normal lobby and owning client's E RPC.
#include "ExistingHouseDoor.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "SeniorLobby.h"
#include "HouseInteractiveProp.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
namespace
{
void TickDoorNetwork(UWorld* W,ELevelTick,float)
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("DoorSwingNetworkReview"))||!W||!W->IsGameWorld())return;
 FString Dir;if(!FParse::Value(FCommandLine::Get(),TEXT("DoorSwingSessionDir="),Dir))return;
 struct FState{TWeakObjectPtr<UWorld> W;int Phase=0;double Start=0,Step=0,Logged=0;TWeakObjectPtr<AHouseInteractiveProp> Chair;FString Last;};static FState S;
 const double Now=FPlatformTime::Seconds();if(S.W.Get()!=W){S=FState();S.W=W;S.Start=S.Step=Now;}
 auto Fail=[&](const FString& M){UE_LOG(LogTemp,Error,TEXT("DOOR_SWING_MP_FAIL mode=%d %s"),int(W->GetNetMode()),*M);FFileHelper::SaveStringToFile(M,*(Dir/TEXT("failed")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,1);};
 if(S.Phase<0)return;if(Now-S.Start>180){Fail(TEXT("Timed out"));return;}
 const FString Map=UGameplayStatics::GetCurrentLevelName(W,true);
 if(Map==TEXT("Lobby")){
  if(W->GetNetMode()!=NM_ListenServer||S.Phase!=0)return;
  auto* Lobby=W->GetGameState<ASeniorLobbyGameState>();if(!Lobby||Lobby->GetMembers().Num()!=2)return;
  ASeniorLobbyController *Host=nullptr,*Remote=nullptr;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* C=Cast<ASeniorLobbyController>(It->Get())){if(C->IsLocalController())Host=C;else Remote=C;}
  if(!Host||!Remote||!Remote->HasClientLoadedCurrentWorld())return;
  Host->SetCharacter(0);Remote->SetCharacter(1);Host->SetReady(true);Remote->SetReady(true);
  if(!Lobby->CanStart()||!Host->CanStartStory())return;
  S.Phase=1;Host->StartStory(false);UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP normal two-player lobby travel"));return;}
 if(Map!=TEXT("Chapter01_House"))return;
 AExistingHouseDoor *L=nullptr,*R=nullptr;for(TActorIterator<AExistingHouseDoor> It(W);It;++It){if(It->DoorId==TEXT("FrontWalnut_Left"))L=*It;if(It->DoorId==TEXT("FrontWalnut_Right"))R=*It;}
 if(!L||!R)return;
 APlayerController* Local=nullptr;APlayerController* Remote=nullptr;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* C=It->Get()){if(C->IsLocalController())Local=C;else Remote=C;}
 auto* P=Local?Cast<AStoryFirstPersonCharacter>(Local->GetPawn()):nullptr;if(!P)return;
 auto Write=[&](const FString& T){FFileHelper::SaveStringToFile(T,*(Dir/TEXT("command")));S.Step=Now;};
 auto Ack=[&](const FString& T){FString Got;return FFileHelper::LoadFileToString(Got,*(Dir/TEXT("ack")))&&Got==T;};
 const auto Full=[](AExistingHouseDoor* D,float Y){return !D->IsActorTickEnabled()&&FMath::Abs(FMath::FindDeltaAngleDegrees(D->GetLeafPivot()->GetRelativeRotation().Yaw,Y))<.25f;};
 if(W->GetNetMode()==NM_Client){
  FString Command;if(!FFileHelper::LoadFileToString(Command,*(Dir/TEXT("command"))))return;
  Local->SetControlRotation((L->DoorwayCenter-P->FirstPersonCamera->GetComponentLocation()).Rotation());
  if(Command==S.Last)return;
  if(Command==TEXT("ready")){
   FString Layout;if(!FFileHelper::LoadFileToString(Layout,*(Dir/TEXT("door-layout.txt"))))return;
   TArray<FString> Rows;Layout.ParseIntoArrayLines(Rows);int Count=0;
   for(const FString& Row:Rows){TArray<FString> Fields;Row.ParseIntoArray(Fields,TEXT("|"),false);if(Fields.Num()!=8){Fail(TEXT("Malformed layout record"));return;}
    AExistingHouseDoor* Found=nullptr;for(TActorIterator<AExistingHouseDoor> It(W);It;++It)if(It->DoorId.ToString()==Fields[0]){Found=*It;break;}
    if(!Found)return;
    FVector Center,Extent,Way;Center.InitFromString(Fields[4]);Extent.InitFromString(Fields[5]);Way.InitFromString(Fields[6]);
    if(Found->DoorGroup.ToString()!=Fields[1]||FMath::Abs(Found->ClosedYaw-FCString::Atof(*Fields[2]))>.1||FMath::Abs(Found->OpenYaw-FCString::Atof(*Fields[3]))>.1||!Found->PanelCenterWorld.Equals(Center,.2)||!Found->PanelHalfExtent.Equals(Extent,.2)||!Found->DoorwayCenter.Equals(Way,.2)||Found->bLocalLeafSpace!=(Fields[7]==TEXT("1")))return;
    if(Extent.X>1&&(!Found->GetPanelCollision()->GetUnscaledBoxExtent().Equals(Extent,.2)||Found->GetPanelCollision()->GetCollisionEnabled()==ECollisionEnabled::NoCollision))return;
    ++Count;
   }
   if(Now-S.Start<10||FVector::Dist2D(P->GetActorLocation(),FVector(-36.65,-1180,0))>8)return;
   auto* D=P->FocusedDoorForInteraction();if(!D||D->DoorGroup!=L->DoorGroup)return;
   UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_CLIENT_LAYOUT all %d authored and procedural door settings and panel collision match server"),Count);FFileHelper::SaveStringToFile(Command,*(Dir/TEXT("ack")));S.Last=Command;return;}
  if(Command==TEXT("open")||Command==TEXT("close")||Command==TEXT("reopen")){
   if(Now-S.Step<.7)return;
   auto* D=P->FocusedDoorForInteraction();if(!D||D->DoorGroup!=L->DoorGroup){Fail(TEXT("Owning client cannot focus double door"));return;}
   Local->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Pressed,1.f));Local->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E,IE_Released,0.f));
   UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_CLIENT_E %s"),*Command);S.Last=Command;S.Step=Now;return;}
  if(Command.StartsWith(TEXT("blocked="))){const float Expected=FCString::Atof(*Command.RightChop(8));
   if(Now-S.Logged>2){UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_CLIENT_POSE leftOpen=%d rightOpen=%d leftYaw=%.4f rightYaw=%.4f leftTick=%d rightTick=%d expected=%.4f"),L->IsOpen(),R->IsOpen(),L->GetLeafPivot()->GetRelativeRotation().Yaw,R->GetLeafPivot()->GetRelativeRotation().Yaw,L->IsActorTickEnabled(),R->IsActorTickEnabled(),Expected);S.Logged=Now;}if(!L->IsOpen()||!R->IsOpen()||!Full(L,Expected)||!Full(R,-90))return;
   UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_CLIENT_BLOCKED yaw=%.3f expected=%.3f otherLeaf=-90"),L->GetLeafPivot()->GetRelativeRotation().Yaw,Expected);
   FScreenshotRequest::RequestScreenshot(Dir/TEXT("client-partly-open.png"),false,false);}
  else if(Command==TEXT("closed")){if(L->IsOpen()||R->IsOpen()||!Full(L,0)||!Full(R,0))return;}
  else if(Command==TEXT("clear")){if(!L->IsOpen()||!R->IsOpen()||!Full(L,90)||!Full(R,-90))return;}
  else if(Command==TEXT("done")){UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_PASS client: E RPC, identical partial angle, reverse, clear and full reopening"));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;}
  else return;
  FFileHelper::SaveStringToFile(Command,*(Dir/TEXT("ack")));S.Last=Command;S.Step=Now;return;
 }
 if(W->GetNetMode()==NM_ListenServer&&S.Phase==8&&Now-S.Step>3){UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_PASS server: replicated furniture stops leaf, independent double door, owning client E reversal and full reopening"));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,0);return;}
 if(W->GetNetMode()!=NM_ListenServer||!Remote)return;
 auto* RP=Cast<AStoryFirstPersonCharacter>(Remote->GetPawn());if(!RP)return;
 if(S.Phase==0){if(Now-S.Start<12)return;
  FString Layout;int Count=0;for(TActorIterator<AExistingHouseDoor> It(W);It;++It){It->bAlwaysRelevant=true;It->ForceNetUpdate();Layout+=FString::Printf(TEXT("%s|%s|%.3f|%.3f|%s|%s|%s|%d\n"),*It->DoorId.ToString(),*It->DoorGroup.ToString(),It->ClosedYaw,It->OpenYaw,*It->PanelCenterWorld.ToString(),*It->PanelHalfExtent.ToString(),*It->DoorwayCenter.ToString(),It->bLocalLeafSpace?1:0);++Count;}
  FFileHelper::SaveStringToFile(Layout,*(Dir/TEXT("door-layout.txt")));UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_SERVER_LAYOUT count=%d"),Count);
  for(TActorIterator<AHouseInteractiveProp> It(W);It;++It)if(It->IntactMesh&&It->PropName.Contains(TEXT("chair"))&&!It->IsClothFlag()){S.Chair=*It;break;}
  if(!S.Chair.IsValid()){Fail(TEXT("No existing replicated furniture chair"));return;}
  auto* Chair=S.Chair.Get();Chair->Body->SetSimulatePhysics(false);Chair->bPhysicsActive=false;Chair->bAlwaysRelevant=true;
  const FVector At(-114.425,-888.845,46.7);Chair->SetActorLocation(Chair->GetActorLocation()+At-Chair->Body->Bounds.Origin,false,nullptr,ETeleportType::TeleportPhysics);Chair->FlushNetDormancy();Chair->ForceNetUpdate();
  P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);P->SetActorLocation(FVector(-5000,-5000,1000),false,nullptr,ETeleportType::TeleportPhysics);
  RP->GetCharacterMovement()->SetMovementMode(MOVE_Flying);RP->SetActorLocation(FVector(-36.65,-1180,RP->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);
  Remote->SetControlRotation((L->DoorwayCenter-RP->FirstPersonCamera->GetComponentLocation()).Rotation());RP->ForceNetUpdate();Write(TEXT("ready"));S.Phase=1;return;}
 if(S.Phase==1&&Ack(TEXT("ready"))){Write(TEXT("open"));S.Phase=2;return;}
 if(S.Phase==2&&L->IsOpen()&&R->IsOpen()&&!L->IsActorTickEnabled()&&!R->IsActorTickEnabled()){
  const float Y=L->GetLeafPivot()->GetRelativeRotation().Yaw;if(Y<8||Y>82||!Full(R,-90)){Fail(TEXT("Server doors did not stop independently at furniture"));return;}
  UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_SERVER_BLOCKED yaw=%.3f otherLeaf=-90"),Y);S.Last=FString::Printf(TEXT("blocked=%.4f"),Y);Write(S.Last);S.Phase=3;return;}
 if(S.Phase==3&&Ack(S.Last)){
  S.Chair->SetActorLocation(S.Chair->GetActorLocation()+FVector(10000,0,0),false,nullptr,ETeleportType::TeleportPhysics);S.Chair->ForceNetUpdate();Write(TEXT("close"));S.Phase=4;return;}
 if(S.Phase==4&&!L->IsOpen()&&!R->IsOpen()&&Full(L,0)&&Full(R,0)){Write(TEXT("closed"));S.Phase=5;return;}
 if(S.Phase==5&&Ack(TEXT("closed"))){Write(TEXT("reopen"));S.Phase=6;return;}
 if(S.Phase==6&&L->IsOpen()&&R->IsOpen()&&Full(L,90)&&Full(R,-90)){Write(TEXT("clear"));S.Phase=7;return;}
 if(S.Phase==7&&Ack(TEXT("clear"))){Write(TEXT("done"));S.Phase=8;return;}
 if(S.Phase==8&&Now-S.Step>3){UE_LOG(LogTemp,Display,TEXT("DOOR_SWING_MP_PASS server: replicated furniture stops leaf, independent double door, owning client E reversal and full reopening"));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,0);}
}
struct FDoorNetworkRegistration{FDoorNetworkRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&TickDoorNetwork);}} Registration;
}
#endif
