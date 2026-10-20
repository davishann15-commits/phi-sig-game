// Ordinary unpinned calm-deer observation; no forced animal state or animal teleport.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseExteriorSiteLayout.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace HouseCalderWildlifeCalmReview
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;TWeakObjectPtr<AHouseCalderWildlifeDirector> Director;
    TArray<FVector> Homes;TArray<bool> Walked;TArray<FString> Captures;TArray<double> Gaps,Drifts;
    TMap<int32,TPair<int64,FVector>> Anchors;
    FString Dir,Rows=TEXT("time,id,mood,homeDistanceCm,x,y,z\n");double Began=-1.,SampleAt=0.,ShotAt=0.,WalkingAt=-1.,FeedSeconds=0.,TotalSeconds=0.,MaxHomeDistance=0.;
    int32 Focus=8,WalkShots=0,FeedShots=0,Flees=0;bool Done=false;
};FState S;
double P95(TArray<double> V){if(V.IsEmpty())return 0.;V.Sort();return V[FMath::Min(V.Num()-1,FMath::FloorToInt(V.Num()*.95))];}
void Finish(bool Good,const FString& Why)
{
    if(S.Done)return;S.Done=true;int32 Walked=0;for(bool W:S.Walked)Walked+=int32(W);
    for(const FString& P:S.Captures)Good&=IFileManager::Get().FileSize(*P)>0;
    Good&=Walked>=8&&S.Flees==0&&S.WalkShots==8&&S.FeedShots>=3&&S.TotalSeconds>0.&&S.FeedSeconds/S.TotalSeconds>.65&&S.MaxHomeDistance<=1000.5&&S.Gaps.Num()>=100&&P95(S.Gaps)<=3.&&P95(S.Drifts)<=3.;
    const FString Result=FString::Printf(TEXT("%s scope=normalUnpinnedCalmDeer seconds=%.3f walked=%d/10 fleeSamples=%d walkCaptures=%d feedCaptures=%d feedFraction=%.4f maxHomeDistanceCm=%.4f submittedSoleGapSamples=%d gapP95cm=%.5f driftP95cm=%.5f sourceAnimalsForced=0 sourceAnimalsTeleported=0 wholePhase3Complete=0 reason=%s\n"),Good?TEXT("PASS"):TEXT("FAIL"),S.Director.IsValid()?HouseCalderWildlife::ServerTime(S.World.Get())-S.Began:0.,Walked,S.Flees,S.WalkShots,S.FeedShots,S.TotalSeconds>0.?S.FeedSeconds/S.TotalSeconds:0.,S.MaxHomeDistance,S.Gaps.Num(),P95(S.Gaps),P95(S.Drifts),*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("states.csv")));UE_LOG(LogTemp,Display,TEXT("CALDER_DEER_CALM_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Good?0:1);
}
bool Eye(UWorld* W,AActor* Pawn,const FVector& At,double Scale,FVector& Result)
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderCalmStandingView),true,Pawn);
    for(double Radius:{450.,600.,800.})for(int32 Try=0;Try<48;++Try)
    {
        FVector P=At+FRotator(0,Try*7.5,0).Vector()*Radius;const double Ground=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;
        if(!W->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Ground+150.),FVector(P.X,P.Y,Ground-150.),ECC_Visibility,Q)||FMath::Abs(Hit.ImpactPoint.Z-Ground)>3.||Hit.ImpactNormal.Z<.766)continue;
        P.Z=Hit.ImpactPoint.Z+170.;bool Blocked=false;for(double H:{25.,80.,135.})Blocked|=W->LineTraceSingleByChannel(Hit,P,At+FVector(0,0,H*Scale),ECC_Visibility,Q);
        if(!Blocked){Result=P;return true;}
    }
    return false;
}
void Tick(UWorld* W,ELevelTick,float Delta)
{
    if(!W||!W->IsGameWorld()||!W->HasBegunPlay()||!FParse::Param(FCommandLine::Get(),TEXT("CalderDeerCalmReview"))||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    if(S.World.Get()!=W){S=FState();S.World=W;S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderDeerCalmReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}if(S.Done)return;
    auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;
    if(!Pawn||!D||!D->Ready())return;const double Now=HouseCalderWildlife::ServerTime(W);
    if(S.Began<0.)
    {
        S.Director=D;S.Began=Now;if(HouseCalderWildlife::ScenePinned()){Finish(false,TEXT("clock must advance normally"));return;}
        for(const auto& A:D->States.Items)S.Homes.Add(D->AnimalPosition(A.Id));S.Walked.Init(false,10);
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
        if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;FActorSpawnParameters P;P.Owner=D->GetOwner();S.Camera=W->SpawnActor<ACameraActor>(P);if(!S.Camera.IsValid()){Finish(false,TEXT("camera missing"));return;}S.Camera->GetCameraComponent()->SetFieldOfView(65.);PC->SetViewTarget(S.Camera.Get());
    }
    for(const auto& A:D->States.Items){S.TotalSeconds+=Delta;if(A.Mood==ECalderWildlifeMood::Feeding)S.FeedSeconds+=Delta;if(A.Mood==ECalderWildlifeMood::Fleeing)++S.Flees;if(A.Mood==ECalderWildlifeMood::Walking){S.Walked[A.Id]=true;if(S.WalkingAt<0.&&A.Habitat==2){S.Focus=A.Id;S.WalkingAt=Now;S.ShotAt=Now+.2;}}}
    const FVector At=D->AnimalPosition(S.Focus);const auto& Focal=D->States.Items[S.Focus];const double Scale=Focal.Variant==1?1.11:Focal.Variant==2?.68:1.;FVector Standing;
    if(!Eye(W,Pawn,At,Scale,Standing)){Finish(false,TEXT("no physical clear standing camera"));return;}
    S.Camera->SetActorLocationAndRotation(Standing,(At+FVector(0,0,80.*Scale)-Standing).Rotation());PC->SetViewTarget(S.Camera.Get());
    FCalderWildlifeSoleProbe Probe;if(D->ReadRenderedSolesForReview(S.Focus,Probe)&&Probe.PoseAge<.03)for(int32 Leg=0;Leg<4;++Leg)if(Probe.PlantedMask&(1<<Leg))
    {S.Gaps.Add(Probe.Gaps[Leg]);const int32 Key=S.Focus*4+Leg;const auto* Before=S.Anchors.Find(Key);if(Before&&Before->Key==Probe.Cycles[Leg])S.Drifts.Add(FVector::Distance(Before->Value,Probe.Centres[Leg]));else S.Anchors.Add(Key,{Probe.Cycles[Leg],Probe.Centres[Leg]});}
    if(Now>=S.SampleAt)
    {
        S.SampleAt=Now+.2;for(const auto& A:D->States.Items){const FVector P=D->AnimalPosition(A.Id);const double Home=FVector::DistXY(P,S.Homes[A.Id]);S.MaxHomeDistance=FMath::Max(S.MaxHomeDistance,Home);S.Rows+=FString::Printf(TEXT("%.5f,%d,%s,%.5f,%.5f,%.5f,%.5f\n"),Now-S.Began,A.Id,HouseCalderWildlife::MoodName(A.Mood),Home,P.X,P.Y,P.Z);}
    }
    FString Name;
    if(S.WalkingAt>=0.&&S.WalkShots<8&&Focal.Mood==ECalderWildlifeMood::Walking&&Now>=S.ShotAt){Name=FString::Printf(TEXT("detail_calm_walk_%02d"),S.WalkShots++);S.ShotAt=Now+.14;}
    else if(S.FeedShots<3&&Focal.Mood==ECalderWildlifeMood::Feeding&&Now-S.Began>5.+S.FeedShots*25.)Name=FString::Printf(TEXT("detail_calm_feed_%02d"),S.FeedShots++);
    if(!Name.IsEmpty()&&!FScreenshotRequest::IsScreenshotRequested()){const FString Path=S.Dir/(Name+TEXT(".png"));FScreenshotRequest::RequestScreenshot(Path,false,false);S.Captures.Add(Path);}
    if(Now-S.Began>=110.&&!FScreenshotRequest::IsScreenshotRequested())Finish(true,TEXT("ordinary habitat feeding and walking observed; visual review separate"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
