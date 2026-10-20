// Two real rendered peers, normal lobby, host's ordinary walking approach.
// Disk files are observations/barriers only; animal data arrives by FastArray.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGooseReactionReview.h"
#include "HouseCalderWildlifeGooseShore.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
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
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Templates/Atomic.h"
namespace HouseCalderWildlifeGooseReactionNetworkReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;
    HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Pond;HouseCalderWildlifeGooseShore::FPlan Shore;
    int32 Stage=0,Id=-1,Samples=0,Hiss=0,Retreat=0,Wait=0,Return=0;bool Ready=false,Event=false,Fenced=false,Captured=false;
    double Began=0.,At=0.,Next=0.,Last=-1.,EventWall=0.,DataAge=-1.,GpuAge=-1.,Worst=0.,LastTickWall=0.,StableSince=0.;
    FString Dir,Capture,Rows=TEXT("wall,authorityRaw,clientRaw,renderedRaw,id,cause,phase,errorCm\n");
};
FState S;TAtomic<uint64> ProofFrame{MAX_uint64};TAtomic<double> ProofTime{0.};FGPUFenceRHIRef Fence;
void FrameEnd()
{
    const uint64 Frame=ProofFrame.Load();if(Frame==MAX_uint64||GFrameCounterRenderThread<Frame)return;
    if(!Fence){Fence=RHICreateGPUFence(TEXT("CalderGooseHissFrame"));FRHICommandListImmediate::Get().WriteGPUFence(Fence);return;}
    if(Fence->Poll()){ProofTime.Store(FPlatformTime::Seconds());ProofFrame.Store(MAX_uint64);Fence.SafeRelease();}
}
FString Hashes(AHouseCalderWildlifeDirector* D){return FString::Printf(TEXT("%08x|%08x|%08x|%08x"),HouseCalderTerrain::Hash(),D->HabitatHash(),D->PondHashForReview(),D->ShoreHashForReview());}
bool ObserverEye(UWorld* W,AActor* Pawn,FVector& Eye)
{
    const FVector Bank=S.Shore.Routes[0].Points.Last();const FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderGooseNetworkStandingEye),true,Pawn);
    for(double Radius:{350.,500.,700.,1000.,1800.})for(int32 Attempt=0;Attempt<72;++Attempt)
    {
        // Stay beside the real approach corridor so the host's body cannot
        // cover the goose. This is only the diagnostic observer's camera.
        const double Yaw=-10.+Attempt*5.;
        // A close eye is in front of the stopped host; farther eyes stay
        // laterally off the corridor. Both keep the goose unobstructed.
        if(Radius>=550.&&FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,-10.))<30.)continue;
        FVector P=Bank+FRotator(0,Yaw,0).Vector()*Radius;float Water=0.f;if(HouseCalderWater::WaterAt(FVector2D(P),Water))continue;
        const double Datum=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;
        if(!W->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Datum+150.),FVector(P.X,P.Y,Datum-150.),ECC_Visibility,Q)||FMath::Abs(Hit.ImpactPoint.Z-Datum)>3.||Hit.ImpactNormal.Z<.766)continue;
        P.Z=Hit.ImpactPoint.Z+170.;bool Clear=true;for(double Height:{4.,35.,80.})Clear&=!W->LineTraceSingleByChannel(Hit,P,FVector(Bank.X,Bank.Y,Bank.Z-27.2+Height),ECC_Visibility,Q);
        if(!Clear)continue;Eye=P;return true;
    }
    return false;
}
void Finish(UWorld* W,bool Passed,const FString& Why)
{
    S.Stage=-1;const FString Role=W->GetNetMode()==NM_Client?TEXT("client"):TEXT("host");
    const FString Result=FString::Printf(TEXT("%s scope=nativeGooseReactionReplication role=%s normalLobby=1 nativeRender=1 realPlayerApproach=1 id=%d dataMs=%.3f followingViewGpuFrameMs=%.3f responseLimitMs=500 sameWallPoseSamples=%d maximumSameWallErrorCm=%.6f poseLimitCm=120 hissSamples=%d retreatSamples=%d homeWaitSamples=%d returnSamples=%d fullRealRecovery=%d callsAccepted=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),*Role,S.Id,S.DataAge*1000.,S.GpuAge*1000.,S.Samples,S.Worst,S.Hiss,S.Retreat,S.Wait,S.Return,int32(Passed),*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/(Role+TEXT("-result.txt"))));FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/(Role+TEXT("-samples.csv"))));if(!Passed&&!IFileManager::Get().FileExists(*(S.Dir/TEXT("failed"))))FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("failed")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_MP %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!W||!W->IsGameWorld()||!FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionNetworkReview")))return;
    FString Dir;if(!FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeSessionDir="),Dir))return;const double Wall=FPlatformTime::Seconds();
    if(S.World.Get()!=W){S=FState();S.World=W;S.Dir=Dir;S.Began=Wall;}if(S.Stage<0)return;
    const double TickGap=S.LastTickWall>0.?Wall-S.LastTickWall:0.;S.LastTickWall=Wall;
    if(Wall-S.Began>420.){Finish(W,false,TEXT("reaction peer timeout"));return;}if(IFileManager::Get().FileExists(*(S.Dir/TEXT("failed")))){Finish(W,false,TEXT("peer failure"));return;}
    const FString Map=UGameplayStatics::GetCurrentLevelName(W,true);
    if(Map==TEXT("Lobby"))
    {
        if(W->GetNetMode()!=NM_ListenServer||S.Stage!=0)return;auto* Lobby=W->GetGameState<ASeniorLobbyGameState>();if(!Lobby||Lobby->GetMembers().Num()!=2)return;
        ASeniorLobbyController *Host=nullptr,*Remote=nullptr;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)if(auto* C=Cast<ASeniorLobbyController>(It->Get()))(C->IsLocalController()?Host:Remote)=C;
        if(!Host||!Remote||!Remote->HasClientLoadedCurrentWorld())return;Host->SetCharacter(0);Remote->SetCharacter(1);Host->SetReady(true);Remote->SetReady(true);if(!Lobby->CanStart()||!Host->CanStartStory())return;S.Stage=1;Host->StartStory(false);return;
    }
    if(Map!=TEXT("Chapter01_House"))return;const bool Client=W->GetNetMode()==NM_Client;
    APlayerController* PC=nullptr;APlayerController* Remote=nullptr;for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)(It->Get()->IsLocalController()?PC:Remote)=It->Get();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;if(!PC||!Pawn||!D||!D->Ready()||!D->PondReadyForReview()||!D->ShoreHashForReview()||!HouseCalderTerrain::CollisionReady(W))return;
    if(D->PondFailedForReview()||D->States.Items.Num()!=10||D->PondStates.Items.Num()!=16){Finish(W,false,TEXT("director/roster invalid"));return;}
    if(!S.Ready)
    {
        if(!S.Sky.Build(W)||!S.Pond.Build(W,S.Sky)||!S.Shore.Build(W,S.Sky,S.Pond)||S.Shore.Hash!=D->ShoreHashForReview()){Finish(W,false,TEXT("peer/runtime route mismatch"));return;}
        S.Ready=true;S.Stage=1;S.At=Wall;S.StableSince=Wall;S.Capture=S.Dir/(Client?TEXT("detail_goose_reaction_client.png"):TEXT("detail_goose_reaction_host.png"));
        if(Client)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
            FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=W->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(W,false,TEXT("no native observer camera"));return;}
            FVector Eye;if(!ObserverEye(W,Pawn,Eye)){Finish(W,false,TEXT("no physically clear side observer eye"));return;}S.Camera->SetActorLocationAndRotation(Eye,(S.Shore.Routes[0].Points.Last()-Eye).Rotation());S.Camera->GetCameraComponent()->SetFieldOfView(65.f);PC->SetViewTarget(S.Camera.Get());if(!FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsTrial")))FFileHelper::SaveStringToFile(Hashes(D),*(S.Dir/TEXT("client-ready")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_MP_EYE at=%s standing170cm=1 sideOfApproach=1 geometryUnchanged=1"),*Eye.ToString());
        }
        return;
    }
    if(Client&&S.Stage==1&&FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsTrial")))
    {
        // Effects integration review v1 encountered a5.14s character-outfit
        // streaming stall after the original ready marker. Start the ordinary
        // approach only after10 actual seconds without a>150ms client tick gap.
        // This is test setup only: the500ms response limit remains unchanged.
        if(TickGap>.15)S.StableSince=Wall;
        if(Wall-S.StableSince<10.)return;
        FFileHelper::SaveStringToFile(Hashes(D),*(S.Dir/TEXT("client-ready")));S.Stage=2;
        UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_MP_STABLE_START seconds=%.3f maximumTickGapMs=150 responseLimitMs=500 setupOnly=1 startupPerformanceAccepted=0"),Wall-S.StableSince);
    }
    if(!Client)
    {
        if(S.Stage==1)
        {
            FString Ready;if(!FFileHelper::LoadFileToString(Ready,*(S.Dir/TEXT("client-ready"))))return;if(Ready!=Hashes(D)){Finish(W,false,TEXT("client terrain/habitat/pond/shore mismatch"));return;}
            auto* Observer=Remote?Cast<AStoryFirstPersonCharacter>(Remote->GetPawn()):nullptr;if(!Observer)return;Observer->GetCharacterMovement()->StopMovementImmediately();Observer->GetCharacterMovement()->DisableMovement();Observer->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);Observer->ForceNetUpdate();S.Stage=2;S.At=Wall;return;
        }
        if(S.Stage==2)
        {if(Wall-S.At<3.)return;if(!HouseCalderWildlifeGooseReactionReview::BeginNetwork(W,PC,Pawn,D)){Finish(W,false,TEXT("ordinary host approach setup failed"));return;}S.Stage=3;return;}
        S.Id=HouseCalderWildlifeGooseReactionReview::NetworkFocal();const auto* State=D->PondStates.Items.FindByPredicate([](const auto& X){return X.Id==S.Id;});
        if(State)
        {
            const double Raw=HouseCalderWildlife::CycleTime(W);
            if(!S.Event&&State->Cause==64)
            {S.Event=true;S.EventWall=D->PondChangeWallForReview(S.Id);FFileHelper::SaveStringToFile(FString::Printf(TEXT("%d|%.9f|%.9f"),S.Id,double(State->StartTime),S.EventWall),*(S.Dir/TEXT("hiss-event")));}
            if(Wall>=S.Next)
            {S.Next=Wall+.1;const FString Sample=FString::Printf(TEXT("%.9f|%.9f|%d|%d|%.9f|%u|%s|%s|%u"),Wall,Raw,S.Id,int32(State->Cause),double(State->StartTime),uint32(State->FacingYaw),*FVector(State->From).ToString(),*FVector(State->To).ToString(),State->Seed);FFileHelper::SaveStringToFile(Sample,*(S.Dir/TEXT("authority-sample")));}
        }
        bool Passed=false;int32 Id=-1;if(HouseCalderWildlifeGooseReactionReview::NetworkDone(Passed,Id))
        {
            if(!Passed){Finish(W,false,TEXT("host native ordinary-input/recovery checks failed"));return;}
            if(!IFileManager::Get().FileExists(*(S.Dir/TEXT("client-complete"))))return;
            if(S.Stage==3){FScreenshotRequest::RequestScreenshot(S.Capture,false,false);S.Stage=4;return;}
            if(S.Stage==4&&!FScreenshotRequest::IsScreenshotRequested()&&IFileManager::Get().FileSize(*S.Capture)>1000)
            {FFileHelper::SaveStringToFile(TEXT("complete"),*(S.Dir/TEXT("done")));S.Stage=5;S.At=Wall;return;}
            if(S.Stage==5&&Wall-S.At>2.)Finish(W,true,TEXT("host input/recovery and client GPU/pose acknowledgement complete;inspect captures"));
        }
        return;
    }
    FString Event;
    if(!S.Event&&FFileHelper::LoadFileToString(Event,*(S.Dir/TEXT("hiss-event"))))
    {
        TArray<FString> F;Event.ParseIntoArray(F,TEXT("|"),false);if(F.Num()!=3){Finish(W,false,TEXT("malformed observation"));return;}S.Id=FCString::Atoi(*F[0]);S.EventWall=FCString::Atod(*F[2]);const double Start=FCString::Atod(*F[1]);
        const auto* State=D->PondStates.Items.FindByPredicate([](const auto& X){return X.Id==S.Id;});
        if(State&&State->Cause==64&&FMath::Abs(State->StartTime-Start)<.002)
        {
            S.DataAge=Wall-S.EventWall;if(S.DataAge<0.||S.DataAge>.5){Finish(W,false,TEXT("replicated hiss data exceeds500ms"));return;}
            FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;
            if(D->ReadPondForReview(S.Id,Root,Visible,Phase,Rendered)&&Visible&&Phase==9&&Rendered>Start+HouseCalderWildlife::CycleReviewOffset()+.05)
            {S.Event=true;ProofFrame.Store(GFrameCounter+1);UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_MP_ARM id=%d dataMs=%.3f actualSubmittedRoot=1 followingFrame=1"),S.Id,S.DataAge*1000.);}
        }
        if(!S.Event&&Wall-S.EventWall>.5){Finish(W,false,TEXT("actual visible hiss pose not submitted within500ms"));return;}
    }
    if(S.Event&&!S.Fenced)
    {
        const double Completed=ProofTime.Load();if(Completed>0.)
        {S.GpuAge=Completed-S.EventWall;if(S.GpuAge<0.||S.GpuAge>.5){Finish(W,false,TEXT("following native hiss GPU view exceeds500ms"));return;}S.Fenced=true;FScreenshotRequest::RequestScreenshot(S.Capture,false,false);UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_MP_RENDER id=%d gpuFrameMs=%.3f limitMs=500 nextFrameFence=1 PNGTimingSeparate=1"),S.Id,S.GpuAge*1000.);}
        else if(Wall-S.EventWall>.5){Finish(W,false,TEXT("following native hiss GPU frame missed500ms deadline"));return;}
    }
    if(S.Fenced&&!S.Captured&&!FScreenshotRequest::IsScreenshotRequested()&&IFileManager::Get().FileSize(*S.Capture)>1000)S.Captured=true;
    if(S.Id>=0&&S.Camera.IsValid())
    {FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;if(D->ReadPondForReview(S.Id,Root,Visible,Phase,Rendered)&&Visible)S.Camera->SetActorRotation((Root.GetTranslation()-S.Camera->GetActorLocation()).Rotation());}
    FString Sample;if(FFileHelper::LoadFileToString(Sample,*(S.Dir/TEXT("authority-sample"))))
    {
        TArray<FString> F;Sample.ParseIntoArray(F,TEXT("|"),false);if(F.Num()!=9)return;const double Stamp=FCString::Atod(*F[0]),HostRaw=FCString::Atod(*F[1]);
        if(Stamp!=S.Last&&Wall-Stamp>=0.&&Wall-Stamp<.4)
        {
            S.Last=Stamp;const int32 Id=FCString::Atoi(*F[2]),Cause=FCString::Atoi(*F[3]);FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;
            if(D->ReadPondForReview(Id,Root,Visible,Phase,Rendered)&&Visible)
            {
                HouseCalderWildlifeGoosePaths::FCycleSample Expected;const double At=HostRaw+(Wall-Stamp);
                if(Cause>=64)
                {
                    FVector From,To;From.InitFromString(F[6]);To.InitFromString(F[7]);HouseCalderWildlifeGooseShore::FReaction R;R.Mode=uint8(Cause);R.Start=FCString::Atod(*F[4])+HouseCalderWildlife::CycleReviewOffset();R.Yaw=FRotator::DecompressAxisFromShort(uint16(FCString::Atoi(*F[5])));double Error=0.;S.Shore.Project(Id%8-6,From,R.Arc,Error);R.Expiry=S.Shore.HomeDeadline(S.Pond,R.Start,Id/8);
                    if(At<R.Expiry)S.Shore.Reaction(S.Pond,At,Id%8,R,Expected);else S.Shore.Cycle(S.Pond,At,Id/8,Id%8,Expected);
                    const auto* Actual=D->PondStates.Items.FindByPredicate([Id](const auto& X){return X.Id==Id;});if(Actual&&Actual->Cause==Cause&&FMath::Abs(Actual->StartTime-FCString::Atod(*F[4]))<.002)
                    {if(!FVector(Actual->From).Equals(From,.11)||!FVector(Actual->To).Equals(To,.11)||Actual->Seed!=uint32(FCString::Strtoui64(*F[8],nullptr,10))){Finish(W,false,TEXT("actual FastArray fields differ from authority observation"));return;}}
                }
                else S.Shore.Cycle(S.Pond,At,Id/8,Id%8,Expected);
                const double Error=FVector::Distance(Root.GetTranslation(),Expected.Bird.Position);S.Worst=FMath::Max(S.Worst,Error);++S.Samples;S.Hiss+=Phase==9;S.Retreat+=Phase==10;S.Wait+=Phase==11;S.Return+=Phase==12;
                S.Rows+=FString::Printf(TEXT("%.9f,%.9f,%.9f,%.9f,%d,%d,%d,%.6f\n"),Wall,HostRaw,HouseCalderWildlife::CycleTime(W),Rendered,Id,Cause,Phase,Error);if(Error>120.){Finish(W,false,TEXT("actual client pose exceeds1.2m same-wall tolerance"));return;}
                if(S.Captured&&S.Samples>=100&&S.Hiss>=1&&S.Retreat>=10&&S.Wait>=10&&S.Return>=10&&Cause<64&&Phase==6)FFileHelper::SaveStringToFile(TEXT("complete"),*(S.Dir/TEXT("client-complete")));
            }
        }
    }
    if(IFileManager::Get().FileExists(*(S.Dir/TEXT("done"))))Finish(W,S.Captured&&S.Fenced&&S.Return>=10,TEXT("real FastArray hiss/retreat/homewait/return and actual client GPU/roots checked;inspect image"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);FCoreDelegates::OnEndFrameRT.AddStatic(&FrameEnd);}}Registration;
}
#endif
