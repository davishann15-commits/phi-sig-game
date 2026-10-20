// Explicit native pond sequence, real server-rate clock with diagnostic offset.
// Reads submitted ISM roots; does not establish reactions/audio/art acceptance.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGoosePaths.h"
#include "HouseCalderLandform.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
namespace HouseCalderWildlifePondReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;
    HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Plan;
    bool Ready=false,Done=false;double Began=0.,Worst=0.,Travel=0.,FirstRaw=0.;
    int32 Compared=0,Frames=0,Shot=0,Min=100,Max=0;bool Phases[16]={};
    FString Dir,Rows=TEXT("raw,id,renderedRaw,visible,phase,x,y,z,errorCm,inBounds,onWater\n");TArray<FString> Captures;
};
FState S;
const FVector Eye(25200,-16420,890); // Original P7 dam-footbridge standing eye.
void Finish(bool Passed,const FString& Reason)
{
    S.Done=true;for(const auto& File:S.Captures)Passed&=IFileManager::Get().FileSize(*File)>1000;
    FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("pond.csv")));
    const FString Result=FString::Printf(TEXT("%s scope=explicitNativePondSequence captures=%d expected=8 compared=%d frames=%d worstRootErrorCm=%.5f pondMin=%d pondMax=%d phase1=%d phase2=%d phase3=%d phase4=%d phase5=%d firstRaw=%.4f maximumIncomingTravelCm=%.2f plan=%08x originalP7Eye=1 realServerRate=1 diagnosticOffset=1 normalCadenceAcceptance=0 reactionsPending=1 callsPending=1 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.Captures.Num(),S.Compared,S.Frames,S.Worst,S.Min,S.Max,int32(S.Phases[1]),int32(S.Phases[2]),int32(S.Phases[3]),int32(S.Phases[4]),int32(S.Phases[5]),S.FirstRaw,S.Travel,S.Plan.Hash,*Reason);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_POND_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePondReview")) || !World || !World->IsGameWorld() || !World->HasBegunPlay() || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Began=Wall;S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderGoosePondReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Done)return;if(Wall-S.Began>270.){Finish(false,TEXT("sequence timeout"));return;}
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(World);auto* D=It?*It:nullptr;if(!Pawn || !D || !D->Ready())return;
    if(D->PondFailedForReview() || D->SkyFailedForReview()){Finish(false,TEXT("runtime pond/sky failed"));return;}if(!D->PondReadyForReview() || !D->SkyHashForReview())return;
    const double Raw=HouseCalderWildlife::CycleTime(World);
    if(!S.Ready)
    {
        if(!S.Sky.Build(World) || !S.Plan.Build(World,S.Sky) || S.Sky.Hash!=D->SkyHashForReview() || S.Plan.Hash!=D->PondHashForReview()){Finish(false,TEXT("reference/runtime plan mismatch"));return;}
        if(Raw>S.Plan.SplitScene-4.){Finish(false,TEXT("review began after pre-handoff window"));return;}
        S.FirstRaw=Raw;Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
        FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(false,TEXT("camera spawn failed"));return;}
        S.Camera->SetActorLocation(Eye);S.Camera->GetCameraComponent()->SetFieldOfView(65.f);PC->SetViewTarget(S.Camera.Get());D->SetTimingView(TEXT("pond_native_P7"));S.Ready=true;
    }
    const double FirstTouch=S.Plan.SplitScene+S.Plan.Durations[0],LastTouch=S.Plan.SplitScene+S.Plan.Durations[7],SwimAt=LastTouch+23.;
    HouseCalderWildlifeGoosePaths::FCycleSample Look;S.Plan.Cycle(Raw,1,0,Look);
    FVector Target=Raw<S.Plan.SplitScene?S.Sky.Goose[1].Points[0]:Look.Bird.Position;
    if(Raw<S.Plan.SplitScene){HouseCalderWildlifeSkyPaths::FSample V;if(S.Sky.EvaluateGoose(Raw,1,22,V))Target=V.Position;}
    if(Raw>=FirstTouch-3.)Target=FVector(25100,-12700,700);
    S.Camera->SetActorRotation((Target-Eye).Rotation());S.Camera->GetCameraComponent()->SetFieldOfView(Raw<FirstTouch-3.?65.f:45.f);
    if(++S.Frames>5)
    {
        int32 OnWater=0;
        for(int32 Id=0;Id<16;++Id)
        {
            HouseCalderWildlifeGoosePaths::FCycleSample Live;S.Plan.Cycle(Raw,Id/8,Id%8,Live);OnWater+=int32(Live.OnWater);S.Phases[Live.Phase]=true;
            FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;if(!D->ReadPondForReview(Id,Root,Visible,Phase,Rendered)){Finish(false,TEXT("missing actual pond instance"));return;}
            if(!Visible || Rendered<0.)continue;
            HouseCalderWildlifeGoosePaths::FCycleSample Expected;if(!S.Plan.Cycle(Rendered,Id/8,Id%8,Expected)){Finish(false,TEXT("visible inactive instance"));return;}
            const double Error=FVector::Distance(Root.GetTranslation(),Expected.Bird.Position);S.Worst=FMath::Max(S.Worst,Error);++S.Compared;
            FBox Bounds(ForceInit);bool Inside=false;TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(D);for(const auto* Part:Parts)if(Part->GetName()==TEXT("Wildlife_PondGeese")){Bounds=Part->Bounds.GetBox();Inside=Bounds.IsInsideOrOn(Root.GetTranslation());}
            if(Id>=8)S.Travel=FMath::Max(S.Travel,FVector::Distance(Root.GetTranslation(),S.Plan.Arrivals[Id%8].Points[0]));
            S.Rows+=FString::Printf(TEXT("%.6f,%d,%.6f,%d,%d,%.5f,%.5f,%.5f,%.6f,%d,%d\n"),Raw,Id,Rendered,int32(Visible),Phase,Root.GetTranslation().X,Root.GetTranslation().Y,Root.GetTranslation().Z,Error,int32(Inside),int32(Expected.OnWater));
            if(Error>.2 || !Inside || (Expected.OnWater&&FMath::Abs(Root.GetTranslation().Z-14.*Expected.Bird.Scale-HouseCalderLandform::PondWaterZ())>.2)){Finish(false,FString::Printf(TEXT("actual root/bounds/water contact mismatch id=%d raw=%.6f root=%s boundsMin=%s boundsMax=%s inside=%d error=%.6f"),Id,Raw,*Root.GetTranslation().ToString(),*Bounds.Min.ToString(),*Bounds.Max.ToString(),int32(Inside),Error));return;}
        }
        S.Min=FMath::Min(S.Min,OnWater);S.Max=FMath::Max(S.Max,OnWater);if(OnWater<6 || OnWater>12){Finish(false,TEXT("pond population outside6to12"));return;}
        for(int32 Member=22;Member<30;++Member){FTransform Root;bool Visible=false;double Rendered=-1.;if(D->ReadSkyForReview(0,Member,Root,Visible,Rendered)&&Visible&&Rendered>=S.Plan.SplitScene){Finish(false,TEXT("duplicate local and pond arrival"));return;}}
    }
    const double Times[]={S.Plan.SplitScene-1.,S.Plan.SplitScene+2.,FirstTouch-3.,FirstTouch+.4,LastTouch+.8,SwimAt+1.,SwimAt+15.,SwimAt+41.};
    if(S.Shot<8 && Raw>=Times[S.Shot] && !FScreenshotRequest::IsScreenshotRequested())
    {const FString File=S.Dir/FString::Printf(TEXT("detail_wild_pond_%02d.png"),S.Shot++);GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);}
    if(S.Shot==8 && Raw>Times[7]+2. && !FScreenshotRequest::IsScreenshotRequested())Finish(S.Compared>500 && S.Travel>30000. && S.Phases[1]&&S.Phases[2]&&S.Phases[3]&&S.Phases[4]&&S.Phases[5],TEXT("native sequence files produced; inspect all images; reactions/audio/two-peer/performance gates remain"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
