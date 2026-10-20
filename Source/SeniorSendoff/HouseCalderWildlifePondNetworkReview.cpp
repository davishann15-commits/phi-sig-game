// Real-lobby, two-native-peer background goose proof. No reaction acceptance.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGoosePaths.h"
#include "HouseCalderTerrain.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace HouseCalderWildlifePondNetworkReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;
    HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Plan;
    int32 Phase=0,Compared=0,PhaseHits[16]={};bool Ready=false;double Began=0.,At=0.,Next=0.,Worst=0.,LastStamp=-1.;
    FString Dir,Capture,Rows=TEXT("wall,hostRaw,clientRaw,renderedRaw,id,phase,errorCm\n");
};
FState S;
void Finish(UWorld* W,bool Passed,const FString& Why)
{
    const FString Role=W->GetNetMode()==NM_Client?TEXT("client"):TEXT("host");S.Phase=-1;
    if(Role==TEXT("client"))FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("client-samples.csv")));
    const FString Result=FString::Printf(TEXT("%s scope=nativePondBackgroundReplication role=%s normalLobby=1 terrain=%08x plan=%08x deerStates=10 pondStates=16 compared=%d maximumSameWallErrorCm=%.4f limitCm=120 arrivalSamples=%d waterSamples=%d brakeSamples=%d swimSamples=%d nativeRender=1 realServerRate=1 diagnosticOffset=1 reactionsPending=1 callsPending=1 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),*Role,HouseCalderTerrain::Hash(),S.Plan.Hash,S.Compared,S.Worst,S.PhaseHits[2],S.PhaseHits[1],S.PhaseHits[3],S.PhaseHits[4],*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/(Role+TEXT("-result.txt"))));if(!Passed && !IFileManager::Get().FileExists(*(S.Dir/TEXT("failed"))))FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("failed")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_POND_MP %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!W || !W->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePondNetworkReview")))return;
    FString Dir;if(!FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeSessionDir="),Dir))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=W){S=FState();S.World=W;S.Dir=Dir;S.Began=Wall;}
    if(S.Phase<0)return;if(Wall-S.Began>300.){Finish(W,false,TEXT("lobby/background synchronization timeout"));return;}
    if(IFileManager::Get().FileExists(*(S.Dir/TEXT("failed")))){Finish(W,false,TEXT("peer reported failure"));return;}
    const FString Map=UGameplayStatics::GetCurrentLevelName(W,true);
    if(Map==TEXT("Lobby"))
    {
        if(W->GetNetMode()!=NM_ListenServer || S.Phase!=0)return;auto* Lobby=W->GetGameState<ASeniorLobbyGameState>();if(!Lobby || Lobby->GetMembers().Num()!=2)return;
        ASeniorLobbyController *Host=nullptr,*Remote=nullptr;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* C=Cast<ASeniorLobbyController>(It->Get()))(C->IsLocalController()?Host:Remote)=C;
        if(!Host || !Remote || !Remote->HasClientLoadedCurrentWorld())return;Host->SetCharacter(0);Remote->SetCharacter(1);Host->SetReady(true);Remote->SetReady(true);if(!Lobby->CanStart() || !Host->CanStartStory())return;S.Phase=1;Host->StartStory(false);return;
    }
    if(Map!=TEXT("Chapter01_House"))return;
    TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;APlayerController* PC=nullptr;for(FConstPlayerControllerIterator P=W->GetPlayerControllerIterator();P;++P)if(P->Get()->IsLocalController())PC=P->Get();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if(!D || !D->Ready() || !Pawn || !D->PondReadyForReview() || !D->SkyHashForReview() || !HouseCalderTerrain::CollisionReady(W))return;
    if(D->PondFailedForReview() || D->SkyFailedForReview() || D->States.Items.Num()!=10 || D->PondStates.Items.Num()!=16){Finish(W,false,TEXT("invalid native director or replicated roster"));return;}
    const bool Client=W->GetNetMode()==NM_Client;const double Raw=HouseCalderWildlife::CycleTime(W);
    if(!S.Ready)
    {
        if(!S.Sky.Build(W) || !S.Plan.Build(W,S.Sky) || S.Plan.Hash!=D->PondHashForReview()){Finish(W,false,TEXT("peer plan does not match runtime"));return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
        FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=W->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(W,false,TEXT("no camera"));return;}S.Camera->SetActorLocation(FVector(25200,-16420,890));S.Camera->GetCameraComponent()->SetFieldOfView(65.f);PC->SetViewTarget(S.Camera.Get());
        S.Ready=true;S.Phase=1;S.At=Wall;S.Capture=S.Dir/(Client?TEXT("detail_wild_pond_client.png"):TEXT("detail_wild_pond_host.png"));
        if(Client)FFileHelper::SaveStringToFile(FString::Printf(TEXT("%08x|%08x"),HouseCalderTerrain::Hash(),S.Plan.Hash),*(S.Dir/TEXT("client-ready")));return;
    }
    const double FirstTouch=S.Plan.SplitScene+S.Plan.Durations[0],End=S.Plan.SplitScene+S.Plan.Durations[7]+30.;HouseCalderWildlifeGoosePaths::FCycleSample Look;S.Plan.Cycle(Raw,1,0,Look);
    const FVector Target=Raw<FirstTouch-3.&&Look.Bird.Active?Look.Bird.Position:FVector(25100,-12700,700);S.Camera->SetActorRotation((Target-S.Camera->GetActorLocation()).Rotation());
    if(S.Phase==3)
    {
        if(!FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*S.Capture)>1000)
        {
            if(Client){FFileHelper::SaveStringToFile(TEXT("captured"),*(S.Dir/TEXT("client-captured")));Finish(W,true,TEXT("native roster/shared-clock roots checked; inspect image; reactions and calls separate"));}
            else if(IFileManager::Get().FileExists(*(S.Dir/TEXT("client-captured"))))Finish(W,true,TEXT("client acknowledged background rendering and capture"));
        }
        return;
    }
    if(!Client)
    {
        if(S.Phase==1)
        {
            FString Ready;if(!FFileHelper::LoadFileToString(Ready,*(S.Dir/TEXT("client-ready"))))return;if(Ready!=FString::Printf(TEXT("%08x|%08x"),HouseCalderTerrain::Hash(),S.Plan.Hash)){Finish(W,false,TEXT("client terrain/plan mismatch"));return;}
            S.Phase=2;S.At=Wall;return;
        }
        if(Wall-S.At<5.)return;
        for(int32 Id=0;Id<16;++Id)
        {
            FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;if(!D->ReadPondForReview(Id,Root,Visible,Phase,Rendered) || !Visible)continue;
            HouseCalderWildlifeGoosePaths::FCycleSample Expected;if(!S.Plan.Cycle(Rendered,Id/8,Id%8,Expected)){Finish(W,false,TEXT("host displays inactive background goose"));return;}
            const double Error=FVector::Distance(Root.GetTranslation(),Expected.Bird.Position);S.Worst=FMath::Max(S.Worst,Error);++S.Compared;++S.PhaseHits[Phase];if(Error>.2){Finish(W,false,TEXT("host native root does not match its shared-clock state"));return;}
        }
        if(Wall>=S.Next){S.Next=Wall+.1;FFileHelper::SaveStringToFile(FString::Printf(TEXT("%.9f|%.9f"),Wall,Raw),*(S.Dir/TEXT("authority-clock")));}
        if(IFileManager::Get().FileExists(*(S.Dir/TEXT("client-checked")))){FFileHelper::SaveStringToFile(TEXT("done"),*(S.Dir/TEXT("done")));FScreenshotRequest::RequestScreenshot(S.Capture,false,false);S.Phase=3;}return;
    }
    if(S.Phase==2)
    {
        if(IFileManager::Get().FileExists(*(S.Dir/TEXT("done")))){FScreenshotRequest::RequestScreenshot(S.Capture,false,false);S.Phase=3;}return;
    }
    FString Clock;if(!FFileHelper::LoadFileToString(Clock,*(S.Dir/TEXT("authority-clock"))))return;TArray<FString> Fields;Clock.ParseIntoArray(Fields,TEXT("|"));if(Fields.Num()!=2)return;
    const double Stamp=FCString::Atod(*Fields[0]),HostRaw=FCString::Atod(*Fields[1]);if(Stamp==S.LastStamp || Wall-Stamp>.4 || Wall-S.At<5.)return;S.LastStamp=Stamp;
    for(int32 Id=0;Id<16;++Id)
    {
        FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;if(!D->ReadPondForReview(Id,Root,Visible,Phase,Rendered) || !Visible)continue;
        HouseCalderWildlifeGoosePaths::FCycleSample Expected;if(!S.Plan.Cycle(HostRaw+(Wall-Stamp),Id/8,Id%8,Expected))continue;
        const double Error=FVector::Distance(Root.GetTranslation(),Expected.Bird.Position);S.Worst=FMath::Max(S.Worst,Error);++S.Compared;++S.PhaseHits[Phase];
        S.Rows+=FString::Printf(TEXT("%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f\n"),Wall,HostRaw,Raw,Rendered,Id,Phase,Error);
        if(Error>120.){Finish(W,false,TEXT("same-wall native root mismatch exceeds1.2m"));return;}
    }
    if(Raw>End)
    {
        if(S.Compared<120 || S.PhaseHits[1]<10 || S.PhaseHits[2]<10 || S.PhaseHits[3]<10 || S.PhaseHits[4]<10){Finish(W,false,TEXT("insufficient live arrival/water/brake/swim samples"));return;}
        FFileHelper::SaveStringToFile(TEXT("checked"),*(S.Dir/TEXT("client-checked")));S.Phase=2;
    }
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
