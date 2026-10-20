// Native submitted-geometry proof for the opt-in background shore routine.
// Explicit diagnostic clock offset, real server rate; no player reactions yet.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGooseShore.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
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
namespace HouseCalderWildlifeGooseShoreReviewImpl
{
struct FPrevious {TArray<FVector> Points[2];int64 Cycles[2]={};uint8 Planted=0;};
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;
    HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Pond;HouseCalderWildlifeGooseShore::FPlan Shore;
    bool Ready=false,Done=false,Medium=false,Outward=false;double Began=0.,Worst=0.;int32 Shot=0,Compared=0,Frames=0,Moving[2]={};
    TArray<double> Gaps,Drifts;FPrevious Previous[16];TArray<FString> Captures;FString Dir,Rows=TEXT("raw,id,phase,stand,walkAge,rootErrorCm,planted,gapSamples,driftSamples\n");
};
FState S;
double P95(TArray<double> Values){if(Values.IsEmpty())return 1.e9;Values.Sort();return Values[FMath::Clamp(FMath::CeilToInt(Values.Num()*.95)-1,0,Values.Num()-1)];}
void Finish(bool Passed,const FString& Reason)
{
    S.Done=true;for(const auto& File:S.Captures)Passed&=IFileManager::Get().FileSize(*File)>1000;
    FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("shore.csv")));
    const FString Result=FString::Printf(TEXT("%s scope=nativeGooseShoreBackground tier=%s direction=%s captures=%d expected=8 rootSamples=%d worstRootErrorCm=%.6f movingBird6=%d movingBird7=%d gapSamples=%d gapP95Cm=%.6f driftSamples=%d driftP95Cm=%.6f limitCm=3 actualSubmittedISMData=1 allSoleVertices=1 physical170cmEye=1 realServerRate=1 diagnosticOffset=1 playerReactionsAccepted=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.Medium?TEXT("medium15Hz"):TEXT("full"),S.Outward?TEXT("outward"):TEXT("return"),S.Captures.Num(),S.Compared,S.Worst,S.Moving[0],S.Moving[1],S.Gaps.Num(),P95(S.Gaps),S.Drifts.Num(),P95(S.Drifts),*Reason);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_SHORE_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool Eye(UWorld* World,AActor* Pawn,const FVector& Target,double Distance,FVector& Out)
{
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderGooseShoreStanding),true,Pawn);
    for(int32 Attempt=0;Attempt<72;++Attempt)
    {
        FVector P=Target+FRotator(0,90.+Attempt*5.,0).Vector()*Distance;float Water=0.f;if(HouseCalderWater::WaterAt(FVector2D(P),Water))continue;
        const double Z=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;
        if(!World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Z+150.),FVector(P.X,P.Y,Z-150.),ECC_Visibility,Query)||FMath::Abs(Hit.ImpactPoint.Z-Z)>3.||Hit.ImpactNormal.Z<.766)continue;
        P.Z=Hit.ImpactPoint.Z+170.;if(World->LineTraceSingleByChannel(Hit,P,Target,ECC_Visibility,Query))continue;Out=P;return true;
    }
    return false;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreReview"))||!World||!World->IsGameWorld()||!World->HasBegunPlay()||UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Began=Wall;S.Medium=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreMediumReview"));S.Outward=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreOutwardReview"));S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderGooseShoreReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Done)return;if(Wall-S.Began>180.){Finish(false,TEXT("shore review timeout"));return;}
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(World);auto* D=It?*It:nullptr;if(!Pawn||!D||!D->Ready()||!D->PondReadyForReview())return;
    if(D->PondFailedForReview()){Finish(false,TEXT("runtime shore failed"));return;}
    const double Raw=HouseCalderWildlife::CycleTime(World);
    if(!S.Ready)
    {
        if(!S.Sky.Build(World)||!S.Pond.Build(World,S.Sky)||!S.Shore.Build(World,S.Sky,S.Pond)||S.Shore.Hash!=D->ShoreHashForReview()){Finish(false,TEXT("reference/runtime shore mismatch"));return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
        FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(false,TEXT("camera spawn failed"));return;}
        FVector Position;const FVector Target=S.Shore.Routes[0].Points.Last();if(!Eye(World,Pawn,Target,S.Medium?6000.:700.,Position)){Finish(false,TEXT("no clear physical standing eye"));return;}
        S.Camera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());S.Camera->GetCameraComponent()->SetFieldOfView(S.Medium?40.f:60.f);PC->SetViewTarget(S.Camera.Get());D->SetTimingView(S.Medium?TEXT("goose_shore_medium"):TEXT("goose_shore_full"));S.Ready=true;
    }
    const double HomeBy=S.Pond.SplitScene+S.Pond.Durations[0]-6.,Return[2]={HomeBy-S.Shore.Routes[0].Length/35.,HomeBy-S.Shore.Routes[1].Length/35.};
    const double Start=S.Pond.SplitScene+S.Pond.Durations[7]+23.+FMath::Max(FVector::Distance(S.Pond.Brakes[6],S.Pond.Homes[6]),FVector::Distance(S.Pond.Brakes[7],S.Pond.Homes[7]))/35.+20.;
    const double End[2]={Start+S.Shore.Routes[0].Length/35.,Start+S.Shore.Routes[1].Length/35.};
    const double ReturnTimes[8]={Return[1]-3.,Return[1]-1.2,Return[1]+1.,Return[0]-1.2,Return[0]+1.,Return[0]+4.,HomeBy-2.,HomeBy+2.};
    const double OutTimes[8]={Start+1.,Start+10.,End[0]-4.,End[0]-.3,End[0]+2.,End[1]-3.,End[1]-.3,End[1]+3.};const double* Times=S.Outward?OutTimes:ReturnTimes;
    const int32 Cohort=S.Outward?1:0;const int32 Follow=S.Outward?(Raw<End[0]+3.?6:7):(Raw<Return[0]-3.?7:6);
    HouseCalderWildlifeGoosePaths::FCycleSample Look;S.Shore.Cycle(S.Pond,Raw,Cohort,Follow,Look);S.Camera->SetActorRotation((Look.Bird.Position-S.Camera->GetActorLocation()).Rotation());
    if(++S.Frames>5)
    {
        int32 Water=0;for(int32 Id=0;Id<16;++Id)
        {
            HouseCalderWildlifeGoosePaths::FCycleSample Live;S.Shore.Cycle(S.Pond,Raw,Id/8,Id%8,Live);Water+=int32(Live.OnWater);
            FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;if(!D->ReadPondForReview(Id,Root,Visible,Phase,Rendered)){Finish(false,TEXT("missing actual pond instance"));return;}if(!Visible||Rendered<0.)continue;
            HouseCalderWildlifeGoosePaths::FCycleSample Expected;S.Shore.Cycle(S.Pond,Rendered,Id/8,Id%8,Expected);const double Error=FVector::Distance(Root.GetTranslation(),Expected.Bird.Position);S.Worst=FMath::Max(S.Worst,Error);++S.Compared;if(Error>.2){Finish(false,TEXT("native shore root mismatch"));return;}
            if(Id/8!=Cohort||Id%8<6||Expected.Stand<.999||Expected.WalkAge<0.)continue;
            FCalderGooseSoleProbe Probe;if(!D->ReadPondSolesForReview(Id,Probe)){Finish(false,TEXT("missing submitted sole geometry"));return;}S.Gaps.Append(Probe.Gaps);++S.Moving[Id%8-6];auto& Previous=S.Previous[Id];
            for(int32 Leg=0;Leg<2;++Leg)
            {
                if((Probe.PlantedMask&Previous.Planted&(1<<Leg))&&Probe.Cycles[Leg]==Previous.Cycles[Leg]&&Probe.Points[Leg].Num()==Previous.Points[Leg].Num())
                for(int32 V=0;V<Probe.Points[Leg].Num();++V)S.Drifts.Add(FVector::Distance(Probe.Points[Leg][V],Previous.Points[Leg][V]));
                Previous.Points[Leg]=Probe.Points[Leg];Previous.Cycles[Leg]=Probe.Cycles[Leg];
            }
            Previous.Planted=Probe.PlantedMask;S.Rows+=FString::Printf(TEXT("%.6f,%d,%d,%.6f,%.6f,%.6f,%d,%d,%d\n"),Rendered,Id,Phase,Expected.Stand,Expected.WalkAge,Error,int32(Probe.PlantedMask),S.Gaps.Num(),S.Drifts.Num());
        }
        if(Water<6||Water>12){Finish(false,TEXT("shore population outside6to12"));return;}
    }
    if(S.Shot<8&&Raw>=Times[S.Shot]&&!FScreenshotRequest::IsScreenshotRequested())
    {const FString File=S.Dir/FString::Printf(TEXT("detail_goose_shore_%02d.png"),S.Shot++);GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);}
    if(S.Shot==8&&Raw>Times[7]+2.&&!FScreenshotRequest::IsScreenshotRequested())Finish(S.Moving[0]>=60&&S.Moving[1]>=60&&S.Gaps.Num()>=60&&S.Drifts.Num()>=30&&P95(S.Gaps)<=3.&&P95(S.Drifts)<=3.,TEXT("native files produced; inspect all images; hiss/reactions/two-peer/audio/performance remain"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
