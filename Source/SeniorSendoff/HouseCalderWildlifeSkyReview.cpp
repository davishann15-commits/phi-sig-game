// Explicit native flight setup: advances a review-only clock, reads submitted
// ISMs, and uses physical standing eyes. Normal server-clock cadence is separate.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeSkyPaths.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
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
namespace HouseCalderWildlifeSkyReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;HouseCalderWildlifeSkyPaths::FPlan Plan;
    int32 Phase=0,Stage=0,Shot=0,Frames=0,Compared=0,VisibleSamples=0;double Began=0.,At=0.,Scene=0.,MoveAt=0.,WorstError=0.,Travel=0.;
    FVector Eye=FVector::ZeroVector,LastRoot=FVector::ZeroVector;bool HadRoot=false;FString Dir,Rows=TEXT("stage,scene,visible,x,y,z,errorCm,insideBounds,travelFromEntryCm\n");TArray<FString> Captures;
};
FState S;
const TCHAR* Names[]={TEXT("goose_v0"),TEXT("goose_v1"),TEXT("crow_stream"),TEXT("bats_pond"),TEXT("bats_glen"),TEXT("bats_lamp")};
void Finish(bool Passed,const FString& Reason)
{
    for(const FString& File:S.Captures)Passed&=IFileManager::Get().FileSize(*File)>1000;
    FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("flight.csv")));
    const FString Result=FString::Printf(TEXT("%s scope=explicitNativeSkyMotion captures=%d expected=18 compared=%d visibleSamples=%d worstRootErrorCm=%.5f maximumTravelFromEntryCm=%.2f physicalEyeCm=170 nativeRender=1 reviewClockSetup=1 normalCadenceAcceptance=0 landingPending=1 callsPending=1 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.Captures.Num(),S.Compared,S.VisibleSamples,S.WorstError,S.Travel,*Reason);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_REVIEW %s"),*Result);S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool Sample(double Scene,HouseCalderWildlifeSkyPaths::FSample& Out)
{
    if(S.Stage<2)return S.Plan.EvaluateGoose(Scene,S.Stage,0,Out);
    if(S.Stage==2)return S.Plan.EvaluateCrow(Scene,0,0,Out);
    return S.Plan.EvaluateBat(Scene,S.Stage==3?0:S.Stage==4?3:6,Out);
}
void KindSlot(int32& Kind,int32& Slot){const int32 Member=S.Stage==3?0:S.Stage==4?3:6;Kind=S.Stage<2?0:S.Stage==2?1:2+Member%2;Slot=S.Stage<3?0:Member/2;}
double ChooseScene()
{
    if(S.Stage>=3)return 320.;const double Start=S.Stage<2?120.+900.*S.Stage:35.,Duration=S.Stage<2?S.Plan.Goose[S.Stage].Length/1800.:S.Plan.Crow.Length/1200.;
    double Best=Start,Distance=1.e30;const FVector2D Desired=S.Stage<2?FVector2D(14000,-10000):FVector2D(16000,14000);
    for(double T=Start;T<Start+Duration-10.;T+=.1){HouseCalderWildlifeSkyPaths::FSample P;if(!Sample(T,P) || FMath::Abs(P.Position.X)>28000. || FMath::Abs(P.Position.Y)>28000.)continue;const double D=FVector2D::DistSquared(FVector2D(P.Position),Desired);if(D<Distance){Distance=D;Best=T;}}return Best;
}
bool Eye(UWorld* World,APawn* Pawn,const FVector& Target,FVector& Out)
{
    const FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderSkyStandingEye),true,Pawn);const double First=S.Stage<3?6000.:700.;
    for(double Distance:{First,First+700.,First+1400.})for(int32 Angle=0;Angle<36;++Angle)
    {
        FVector P=Target+FRotator(0,Angle*10.,0).Vector()*Distance;if(FMath::Abs(P.X)>31000. || FMath::Abs(P.Y)>31000.)continue;float Water=0.f;if(HouseCalderWater::WaterAt(FVector2D(P),Water))continue;
        const double Ground=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;if(!World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Ground+150.),FVector(P.X,P.Y,Ground-150.),ECC_Visibility,Query) || FMath::Abs(Hit.ImpactPoint.Z-Ground)>3. || Hit.ImpactNormal.Z<.766)continue;
        P.Z=Hit.ImpactPoint.Z+170.;bool Clear=true;
        // Some exterior facades are appearance-only. The authored FBlock/DZ
        // boxes must also clear every member throughout this six-second shot.
        for(double Future:{0.,3.5,7.})
        {
            if(S.Stage<3)for(int32 Member=0;Member<(S.Stage<2?40:15);++Member)
            {
                HouseCalderWildlifeSkyPaths::FSample Bird;const bool Active=S.Stage<2?S.Plan.EvaluateGoose(S.Scene+Future,S.Stage,Member,Bird):S.Plan.EvaluateCrow(S.Scene+Future,0,Member,Bird);
                if(Active && (!S.Plan.SolidSegmentClear(P,Bird.Position,115.) || World->LineTraceSingleByChannel(Hit,P,Bird.Position,ECC_Visibility,Query))){Clear=false;break;}
            }
            else {HouseCalderWildlifeSkyPaths::FSample Bird;if(!Sample(S.Scene+Future,Bird) || !S.Plan.SolidSegmentClear(P,Bird.Position,30.) || World->LineTraceSingleByChannel(Hit,P,Bird.Position,ECC_Visibility,Query))Clear=false;}
            if(!Clear)break;
        }
        if(!Clear)continue;Out=P;return true;
    }
    return false;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSkyReview")) || !World || !World->IsGameWorld() || !World->HasBegunPlay() || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Began=Wall;S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSkyReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Phase<0)return;if(Wall-S.Began>180.){Finish(false,TEXT("flight review timed out"));return;}
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(World);auto* D=It?*It:nullptr;if(!Pawn || !D || !D->Ready())return;
    if(D->SkyFailedForReview()){Finish(false,TEXT("runtime sky build failed"));return;}
    if(!D->SkyHashForReview())return;
    if(S.Phase==0)
    {
        if(!S.Plan.Build(World) || S.Plan.Hash!=D->SkyHashForReview()){Finish(false,TEXT("review and runtime route authority differ"));return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(AHUD* HUD=PC->GetHUD())HUD->bShowHUD=false;
        FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);if(!S.Camera.IsValid()){Finish(false,TEXT("camera spawn failed"));return;}S.Phase=1;
    }
    if(S.Phase==1)
    {
        S.Scene=ChooseScene();HouseCalderWildlifeSkyPaths::FSample P;if(!Sample(S.Scene,P) || !Eye(World,Pawn,P.Position,S.Eye)){Finish(false,TEXT("no clear physical standing eye for flight"));return;}
        S.Camera->GetCameraComponent()->SetFieldOfView(S.Stage<3?65.f:40.f);S.Camera->SetActorLocationAndRotation(S.Eye,(P.Position-S.Eye).Rotation());PC->SetViewTarget(S.Camera.Get());D->SetTimingView(FName(*FString::Printf(TEXT("sky_%s"),Names[S.Stage])));
        D->SetSkySceneForReview(S.Scene);S.At=Wall;S.MoveAt=HouseCalderWildlife::ServerTime(World);S.Shot=0;S.Frames=0;S.HadRoot=false;S.Phase=2;return;
    }
    const double Age=Wall-S.At;int32 Kind=0,Slot=0;KindSlot(Kind,Slot);FTransform Root;bool Visible=false;double RenderedScene=-1.;
    if(D->ReadSkyForReview(Kind,Slot,Root,Visible,RenderedScene) && RenderedScene>=0. && ++S.Frames>4)
    {
        HouseCalderWildlifeSkyPaths::FSample Expected;if(Sample(RenderedScene,Expected))
        {
            const double Error=Visible?FVector::Distance(Root.GetTranslation(),Expected.Position):0.;S.WorstError=FMath::Max(S.WorstError,Error);++S.Compared;if(Visible)++S.VisibleSamples;
            const FVector Entry=S.Stage<2?S.Plan.Goose[S.Stage].Points[0]:S.Stage==2?S.Plan.Crow.Points[0]:S.Plan.BatCentres[S.Stage==3?0:S.Stage==4?3:6];const double Travel=FVector::Distance(Expected.Position,Entry);S.Travel=FMath::Max(S.Travel,Travel);
            bool Inside=true;TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(D);for(const auto* Part:Parts)if(Part->GetName()==FString::Printf(TEXT("Wildlife_Sky_%d"),Kind))Inside=Part->Bounds.GetBox().IsInsideOrOn(Expected.Position);
            S.Rows+=FString::Printf(TEXT("%s,%.6f,%d,%.5f,%.5f,%.5f,%.6f,%d,%.3f\n"),Names[S.Stage],RenderedScene,int32(Visible),Root.GetTranslation().X,Root.GetTranslation().Y,Root.GetTranslation().Z,Error,int32(Inside),Travel);
            if(Error>.2 || (Visible&&!Inside)){Finish(false,TEXT("submitted root or moving component bounds mismatch"));return;}
            if(Age>1. && !Visible){Finish(false,TEXT("tracked bird should be visible inside400m and camera frustum"));return;}
        }
    }
    const double MovingAge=HouseCalderWildlife::ServerTime(World)-S.MoveAt;HouseCalderWildlifeSkyPaths::FSample Look;if(Sample(S.Scene+MovingAge,Look))S.Camera->SetActorRotation((Look.Position-S.Eye).Rotation());D->SetSkySceneForReview(S.Scene+MovingAge);
    if(S.Shot<3 && Age>2.+S.Shot*1.5 && !FScreenshotRequest::IsScreenshotRequested())
    {const FString File=S.Dir/FString::Printf(TEXT("detail_wild_%s_%02d.png"),Names[S.Stage],S.Shot++);GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);}
    if(S.Shot==3 && Age>6.5 && !FScreenshotRequest::IsScreenshotRequested())
    {if(++S.Stage>=6){Finish(S.Compared>=120 && S.VisibleSamples>=120 && S.Travel>30000.,TEXT("native files produced; individual visual inspection and normal clock/two-peer/audio/performance gates remain"));return;}S.Phase=1;}
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
