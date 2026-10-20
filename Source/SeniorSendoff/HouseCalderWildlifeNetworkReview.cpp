// Separate deer synchronization proof through the real two-player lobby.
// The original campus/71-waypoint multiplayer harness stays unchanged.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeApproachReview.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Templates/Atomic.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderWildlifeNetworkReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ACameraActor> Camera;
    int32 Phase=0;
    double Start=0,Step=0,StateWall=0,EventStart=0,DataAge=0,GpuAge=0;
    int32 Id=-1;
    FString Dir,Capture;
};
FState S;
// Disk encoding/readback is separate from the half-second visibility gate.
// Arm for the following frame after the actual body ISM state is verified.
// At frame end the GPU fence follows all view draws; its completion timestamp
// is a conservative upper bound on the first completed flee frame.
TAtomic<uint64> ProofFrame{MAX_uint64};TAtomic<double> ProofGpuTime{0.};
FGPUFenceRHIRef ProofFence;
void FrameEnd()
{
    const uint64 Frame=ProofFrame.Load();if(Frame==MAX_uint64 || GFrameCounterRenderThread<Frame)return;
    if(!ProofFence)
    {ProofFence=RHICreateGPUFence(TEXT("CalderWildlifeFleeFrame"));FRHICommandListImmediate::Get().WriteGPUFence(ProofFence);return;}
    if(ProofFence->Poll())
    {ProofGpuTime.Store(FPlatformTime::Seconds());ProofFrame.Store(MAX_uint64);ProofFence.SafeRelease();}
}

void Fail(const FString& Why)
{
    UE_LOG(LogTemp,Error,TEXT("CALDER_WILDLIFE_MP_FAIL %s"),*Why);
    FFileHelper::SaveStringToFile(Why,*(S.Dir/TEXT("failed")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,1);
}
void Pass(UWorld* World)
{
    const FString Text=FString::Printf(TEXT("PASS role=%s normalLobby=1 terrainHash=%08x nativeRender=%d logicProbeOnly=%d originalHillRouteGatePending=1 soundOtherSpeciesFinalPerformancePending=1\n"),World->GetNetMode()==NM_Client?TEXT("client"):TEXT("host"),HouseCalderTerrain::Hash(),int32(!HouseCalderWildlife::LogicProbe()),int32(HouseCalderWildlife::LogicProbe()));
    FFileHelper::SaveStringToFile(Text,*(S.Dir/(World->GetNetMode()==NM_Client?TEXT("client-result.txt"):TEXT("host-result.txt"))));
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_MP_%s"),*Text);S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,0);
}
bool Observer(UWorld* World,APlayerController* PC,APawn* Pawn,AHouseCalderWildlifeDirector* Director,int32 Id)
{
    if(HouseCalderWildlife::LogicProbe())return true;
    if(!S.Camera.IsValid()){FActorSpawnParameters Params;Params.Owner=Director->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);S.Camera->GetCameraComponent()->SetFieldOfView(65.f);}
    const FVector At=Director->AnimalPosition(Id),Target=At+FVector(0,0,85);FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderWildlifeMPObserver),true,Pawn);
    for(int32 I=0;I<24;++I)
    {
        FVector Eye=At+FRotator(0,I*15.,0).Vector()*850.;const float Z=HouseExteriorSite::GroundHeight(Eye.X,Eye.Y);FHitResult Ground,Block;
        if(!World->LineTraceSingleByChannel(Ground,FVector(Eye.X,Eye.Y,Z+150),FVector(Eye.X,Eye.Y,Z-150),ECC_Visibility,Q) || FMath::Abs(Ground.ImpactPoint.Z-Z)>3.)continue;
        Eye.Z=Ground.ImpactPoint.Z+170.;if(World->LineTraceSingleByChannel(Block,Eye,Target,ECC_Visibility,Q))continue;
        S.Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());PC->SetViewTarget(S.Camera.Get());return true;
    }
    return false;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeNetworkReview")))return;
    FString Directory;if(!FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeSessionDir="),Directory))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Dir=Directory;S.Start=S.Step=Wall;}
    if(S.Phase<0)return;if(Wall-S.Start>240.){Fail(TEXT("normal lobby/deer test timed out"));return;}
    const FString Map=UGameplayStatics::GetCurrentLevelName(World,true);
    if(Map==TEXT("Lobby"))
    {
        if(World->GetNetMode()!=NM_ListenServer || S.Phase!=0)return;
        auto* Lobby=World->GetGameState<ASeniorLobbyGameState>();if(!Lobby || Lobby->GetMembers().Num()!=2)return;
        ASeniorLobbyController *Host=nullptr,*Remote=nullptr;
        for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)if(auto* C=Cast<ASeniorLobbyController>(It->Get()))(C->IsLocalController()?Host:Remote)=C;
        if(!Host || !Remote || !Remote->HasClientLoadedCurrentWorld())return;
        Host->SetCharacter(0);Remote->SetCharacter(1);Host->SetReady(true);Remote->SetReady(true);
        if(!Lobby->CanStart() || !Host->CanStartStory())return;S.Phase=1;Host->StartStory(false);
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_MP real lobby two players readied; ordinary story travel"));return;
    }
    if(Map!=TEXT("Chapter01_House"))return;
    TActorIterator<AHouseCalderWildlifeDirector> It(World);auto* D=It?*It:nullptr;
    APlayerController *Local=nullptr,*Remote=nullptr;
    for(FConstPlayerControllerIterator P=World->GetPlayerControllerIterator();P;++P)if(auto* C=P->Get())(C->IsLocalController()?Local:Remote)=C;
    auto* Me=Local?Cast<AStoryFirstPersonCharacter>(Local->GetPawn()):nullptr;
    if(!D || !D->Ready() || !Me || !HouseCalderTerrain::CollisionReady(World))return;
    if(World->GetNetMode()==NM_Client)
    {
        if(S.Phase==0)
        {
            FFileHelper::SaveStringToFile(FString::Printf(TEXT("%08x|%08x"),HouseCalderTerrain::Hash(),D->HabitatHash()),*(S.Dir/TEXT("client-ready")));
            S.Phase=1;S.Step=Wall;
        }
        if(!Observer(World,Local,Me,D,S.Id<0?5:S.Id)){Fail(TEXT("no physically legitimate client observer camera"));return;}
        if(S.Phase==1)
        {
            FString Event;if(!FFileHelper::LoadFileToString(Event,*(S.Dir/TEXT("host-flee.txt"))))return;
            TArray<FString> F;Event.ParseIntoArray(F,TEXT("|"),false);if(F.Num()!=11){Fail(TEXT("malformed expected-state record"));return;}
            const int32 Id=FCString::Atoi(*F[0]);const double Start=FCString::Atod(*F[1]),StateWall=FCString::Atod(*F[2]);
            if(!D->States.Items.IsValidIndex(Id))return;const auto& Actual=D->States.Items[Id];
            if(Actual.Mood!=ECalderWildlifeMood::Fleeing || FMath::Abs(Actual.StartTime-Start)>.002)return;
            FVector From,To,Pose;From.InitFromString(F[5]);To.InitFromString(F[6]);Pose.InitFromString(F[7]);
            const bool Same=F[3]==FString::Printf(TEXT("%08x"),HouseCalderTerrain::Hash()) && F[4]==FString::Printf(TEXT("%08x"),D->HabitatHash())
                && FVector(Actual.From).Equals(From,.11) && FVector(Actual.To).Equals(To,.11) && Actual.Seed==uint32(FCString::Strtoui64(*F[8],nullptr,10))
                && Actual.Cause==1 && Actual.Habitat==1 && D->AnimalPosition(Id,Start+.25).Equals(Pose,.11);
            if(!Same){Fail(TEXT("replicated state/terrain/habitat or pure reference pose differs"));return;}
            S.Id=Id;S.EventStart=Start;S.StateWall=StateWall;S.DataAge=Wall-StateWall;
            if(S.DataAge<0 || S.DataAge>.5){Fail(FString::Printf(TEXT("replicated flee arrived %.4fs after authority change; limit0.5s"),S.DataAge));return;}
            if(!HouseCalderWildlife::LogicProbe())
            {
                TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(D);bool Found=false;FTransform PoseNow;
                for(auto* Part:Parts)if(Part->GetName()==TEXT("Wildlife_DeerBody"))
                    Found=Part->IsVisible() && Part->GetInstanceTransform(Id,PoseNow,true) && PoseNow.GetLocation().Equals(D->AnimalPosition(Id),.2);
                if(!Found){Fail(TEXT("client's actual visible body instance did not update to its replicated flee"));return;}
                S.Capture=S.Dir/TEXT("detail_wild_deer_client_flee.png");ProofFrame.Store(GFrameCounter+1);S.Phase=2;
            }
            else S.Phase=3;
        }
        if(S.Phase==2)
        {
            const double Completed=ProofGpuTime.Load();
            if(Completed>0.)
            {
                S.GpuAge=Completed-S.StateWall;
                if(S.GpuAge<0. || S.GpuAge>.5){Fail(FString::Printf(TEXT("client flee GPU frame completed %.4fs after authority change; limit0.5s"),S.GpuAge));return;}
                UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_MP_RENDER id=%d gpuFrameCompletionUpperBoundMs=%.2f dataMs=%.2f actualInstanceChecked=1 nextFrameFence=1 screenshotTimingSeparate=1"),S.Id,S.GpuAge*1000.,S.DataAge*1000.);
                FScreenshotRequest::RequestScreenshot(S.Capture,false,false);S.Phase=5;
            }
            else if(Wall-S.StateWall>.5){Fail(TEXT("client flee GPU frame not completed within0.5s"));return;}
        }
        if(S.Phase==5 && !FScreenshotRequest::IsScreenshotRequested() && IFileManager::Get().FileSize(*S.Capture)>0)S.Phase=3;
        if(S.Phase==3)
        {
            const FString Ack=FString::Printf(TEXT("id=%d dataMs=%.2f gpuFrameMs=%.2f screenshotCompletionMs=%.2f nativeRender=%d"),S.Id,S.DataAge*1000.,S.GpuAge*1000.,(Wall-S.StateWall)*1000.,int32(!HouseCalderWildlife::LogicProbe()));
            FFileHelper::SaveStringToFile(Ack,*(S.Dir/TEXT("client-flee-ack")));UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_MP_CLIENT %s"),*Ack);S.Phase=4;
        }
        if(S.Phase==4 && IFileManager::Get().FileExists(*(S.Dir/TEXT("done"))))Pass(World);
        return;
    }
    // A successful client exits and removes its pawn before the host's quiet
    // shutdown delay. Finish before requiring a still-connected remote pawn.
    if(World->GetNetMode()==NM_ListenServer && S.Phase==4)
    {if(Wall-S.Step>2.)Pass(World);return;}
    if(World->GetNetMode()!=NM_ListenServer || !Remote || !Remote->GetPawn())return;
    if(S.Phase==0)
    {
        FString Ready;if(!FFileHelper::LoadFileToString(Ready,*(S.Dir/TEXT("client-ready"))))return;
        if(Ready!=FString::Printf(TEXT("%08x|%08x"),HouseCalderTerrain::Hash(),D->HabitatHash())){Fail(TEXT("native client terrain/habitat hash mismatch"));return;}
        auto* ObserverPawn=Cast<AStoryFirstPersonCharacter>(Remote->GetPawn());if(!ObserverPawn)return;
        ObserverPawn->GetCharacterMovement()->StopMovementImmediately();ObserverPawn->GetCharacterMovement()->DisableMovement();
        ObserverPawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);ObserverPawn->ForceNetUpdate();
        // The observer is excluded by the same production3D rule, with the host
        // remaining an ordinary walking player whose approach causes the flee.
        S.Phase=1;S.Step=Wall;return;
    }
    if(S.Phase==1 && Wall-S.Step>3.)
    {
        if(!HouseCalderWildlifeApproachReview::BeginNetwork(World,Local,Me,D)){Fail(TEXT("host ordinary-input approach could not start"));return;}S.Phase=2;return;
    }
    if(S.Phase==2)
    {
        int32 Id=-1;if(!HouseCalderWildlifeApproachReview::NetworkFled(Id))return;
        const auto& Item=D->States.Items[Id];const FVector Pose=D->AnimalPosition(Id,Item.StartTime+.25);
        const FString Event=FString::Printf(TEXT("%d|%.9f|%.9f|%08x|%08x|%s|%s|%s|%u|%d|%d"),Id,double(Item.StartTime),D->StateChangeWallTime(Id),HouseCalderTerrain::Hash(),D->HabitatHash(),*FVector(Item.From).ToString(),*FVector(Item.To).ToString(),*Pose.ToString(),Item.Seed,Item.Cause,Item.Habitat);
        FFileHelper::SaveStringToFile(Event,*(S.Dir/TEXT("host-flee.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_MP_HOST ordinary walking player caused flee id=%d start=%.3f"),Id,Item.StartTime);S.Phase=3;S.Step=Wall;
    }
    if(S.Phase==3)
    {
        FString Ack;if(!FFileHelper::LoadFileToString(Ack,*(S.Dir/TEXT("client-flee-ack")))){if(Wall-S.Step>3.)Fail(TEXT("client did not acknowledge the replicated flee"));return;}
        FFileHelper::SaveStringToFile(TEXT("done"),*(S.Dir/TEXT("done")));S.Phase=4;S.Step=Wall;UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_MP_HOST client acknowledgement %s"),*Ack);return;
    }
    if(S.Phase==4 && Wall-S.Step>2.)Pass(World);
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);FCoreDelegates::OnEndFrameRT.AddStatic(&FrameEnd);}}Registration;
}
#endif
