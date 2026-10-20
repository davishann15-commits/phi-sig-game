// Explicit native rendering with a review clock; normal cadence/audio separate.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeCrowRoost.h"
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
namespace HouseCalderWildlifeCrowRenderReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;
    HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeCrowRoost::FPlan Plan;
    int32 Phase=0,Stage=0,Event=0,Member=0,Shot=0,Compared=0,HiddenAfter=0,Visible[2]={};
    double Began=0.,At=0.,Scene=0.,WorstError=0.;FVector Eye=FVector::ZeroVector;
    FString Dir,Rows=TEXT("stage,scene,visible,active,errorCm,bounds,anchors\n");TArray<FString> Captures;
};FState S;
void Finish(bool Passed,const FString& Why)
{
    for(const FString& File:S.Captures)Passed&=IFileManager::Get().FileSize(*File)>1000;
    FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("submitted.csv")));
    const FString Result=FString::Printf(TEXT("%s scope=explicitNativeCrowRoostRender captures=%d compared=%d crossingVisible=%d roostVisible=%d hiddenAfter=%d worstRootCm=%.6f hash=%08x baseSky=%08x physicalEyeCm=170 actualInstances=17 hiddenBoundsAnchors=2 noPerBirdActors=1 callsAccepted=0 normalCadenceAccepted=0 pineTaxonomyVerified=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.Captures.Num(),S.Compared,S.Visible[0],S.Visible[1],S.HiddenAfter,S.WorstError,S.Plan.Hash,S.Sky.Hash,*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_CROW_RENDER_REVIEW %s"),*Result);S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
FVector Target(double Scene)
{HouseCalderWildlifeSkyPaths::FSample Bird;return S.Plan.Evaluate(Scene,S.Event,S.Member,Bird)?Bird.Position:S.Plan.Targets[S.Member].End;}
bool Eye(UWorld* W,APawn* Pawn,FVector& Out)
{
    const FVector Aim=Target(S.Scene);const FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderCrowStandingEye),true,Pawn);
    for(double Radius:{2500.,4000.,6000.})for(int32 Angle=0;Angle<36;++Angle)
    {
        FVector P=Aim+FRotator(0,Angle*10.,0).Vector()*Radius;if(FMath::Abs(P.X)>31000.||FMath::Abs(P.Y)>31000.)continue;
        float Water=0.f;if(HouseCalderWater::WaterAt(FVector2D(P),Water))continue;
        const double Ground=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;
        if(!W->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Ground+150.),FVector(P.X,P.Y,Ground-150.),ECC_Visibility,Query)||FMath::Abs(Hit.ImpactPoint.Z-Ground)>3.||Hit.ImpactNormal.Z<.766)continue;
        P.Z=Hit.ImpactPoint.Z+170.;bool Clear=true;
        for(double Future:{0.,2.5,5.})if(!S.Sky.SolidSegmentClear(P,Target(S.Scene+Future),25.)||W->LineTraceSingleByChannel(Hit,P,Target(S.Scene+Future),ECC_Visibility,Query)){Clear=false;break;}
        if(Clear){Out=P;return true;}
    }
    return false;
}
void Capture(int32 Index)
{
    const FString File=S.Dir/FString::Printf(TEXT("detail_crow_roost_%d_%d.png"),S.Stage,Index);GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderCrowRoostReview"))||!W||!W->IsGameWorld()||!W->HasBegunPlay()||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=W){S=FState();S.World=W;S.Began=Wall;S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderCrowRenderReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Phase<0)return;if(Wall-S.Began>180.){Finish(false,TEXT("review timeout"));return;}
    auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;
    if(!D||!Pawn||!D->Ready())return;if(D->SkyFailedForReview()){Finish(false,TEXT("runtime roost plan failed"));return;}if(!D->CrowRoostHashForReview())return;
    if(S.Phase==0)
    {
        if(!S.Sky.Build(W)||!S.Plan.Build(W,S.Sky)||S.Plan.Hash!=D->CrowRoostHashForReview()||S.Plan.Hash!=0xb7ca5f24||S.Sky.Hash!=0x74bb095c){Finish(false,TEXT("runtime/data authority mismatch"));return;}
        for(int32 E=0;E<12;++E)if(HouseCalderWildlifeCrowRoost::Count(E)==15){S.Event=E;break;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
        S.Camera=W->SpawnActor<ACameraActor>();if(!S.Camera.IsValid()){Finish(false,TEXT("camera unavailable"));return;}S.Camera->GetCameraComponent()->SetFieldOfView(65.f);PC->SetViewTarget(S.Camera.Get());S.Phase=1;
    }
    if(S.Phase==1)
    {
        S.Member=S.Stage==0?0:14;const double Start=35.+150.*S.Event;
        if(S.Stage==0)
        {
            double Best=1.e30;S.Scene=Start+40.;
            for(double Age=20.;Age<S.Plan.Routes[0].Length/1200.-12.;Age+=.1)
            {HouseCalderWildlifeSkyPaths::FSample P;if(!S.Plan.Evaluate(Start+Age,S.Event,0,P))continue;const double Dist=FVector2D::DistSquared(FVector2D(P.Position),FVector2D(18000,2000));if(Dist<Best){Best=Dist;S.Scene=Start+Age;}}
        }
        else S.Scene=Start+S.Plan.LaunchSpacing*S.Member+S.Plan.Routes[S.Member].Length/1200.-5.5;
        if(!Eye(W,Pawn,S.Eye)){Finish(false,TEXT("no unobstructed physical standing eye"));return;}
        S.Camera->SetActorLocationAndRotation(S.Eye,(S.Eye-Target(S.Scene)).Rotation());D->SetSkySceneForReview(S.Scene);D->SetTimingView(S.Stage==0?TEXT("crow_crossing"):TEXT("crow_roost_entry"));S.At=Wall;S.Shot=0;S.Phase=2;return;
    }
    if(S.Phase==2)
    {
        if(Wall-S.At<5.5)return;FTransform Root;bool Visible=false;double Rendered=0.;if(!D->ReadSkyForReview(1,S.Member,Root,Visible,Rendered)||Visible){Finish(false,TEXT("target did not hide outside view"));return;}
        S.Camera->SetActorRotation((Target(S.Scene)-S.Eye).Rotation());S.At=Wall;S.Phase=3;return;
    }
    if(S.Phase==3)
    {
        const double Age=Wall-S.At,Scene=S.Scene+FMath::Min(Age,8.);D->SetSkySceneForReview(Scene);S.Camera->SetActorRotation((Target(Scene)-S.Eye).Rotation());
        UInstancedStaticMeshComponent* Part=nullptr;TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(D);for(auto* P:Parts)if(P->GetName()==TEXT("Wildlife_Sky_1"))Part=P;
        if(!Part||Part->GetInstanceCount()!=17||Part->NumCustomDataFloats!=225){Finish(false,TEXT("actual crow pool invalid"));return;}
        bool Bounds=true,Anchors=true;for(const auto& Route:S.Plan.Routes)for(const FVector& P:Route.Points)Bounds&=Part->Bounds.GetBox().IsInsideOrOn(P);
        for(int32 I:{15,16}){FTransform Anchor;Part->GetInstanceTransform(I,Anchor,true);Anchors&=Anchor.GetScale3D().IsNearlyZero()&&Part->PerInstanceSMCustomData[I*225+224]==0.f;}
        FTransform Root;bool Visible=false;double Rendered=0.;if(!D->ReadSkyForReview(1,S.Member,Root,Visible,Rendered)){Finish(false,TEXT("missing submitted crow root"));return;}
        HouseCalderWildlifeSkyPaths::FSample Expected;const bool Active=S.Plan.Evaluate(Rendered,S.Event,S.Member,Expected);double Error=0.;
        if(Visible)
        {if(!Active){Finish(false,TEXT("inactive bird remains visible"));return;}Error=FVector::Distance(Root.GetTranslation(),Expected.Position);S.WorstError=FMath::Max(S.WorstError,Error);++S.Compared;++S.Visible[S.Stage];}
        else if(!Active&&S.Stage==1&&Age>6.){++S.HiddenAfter;if(!Root.GetScale3D().IsNearlyZero()){Finish(false,TEXT("roosted instance did not hide"));return;}}
        if(!Bounds||!Anchors||Error>.2||(Age>.3&&Active&&!Visible)){Finish(false,TEXT("submitted pose/bounds/reentry mismatch"));return;}
        S.Rows+=FString::Printf(TEXT("%d,%.6f,%d,%d,%.6f,%d,%d\n"),S.Stage,Rendered,int32(Visible),int32(Active),Error,int32(Bounds),int32(Anchors));
        const double Times[]={.5,2.8,5.,7.4};if(S.Shot<4&&Age>=Times[S.Shot])Capture(S.Shot++);
        if(Age>8.5)
        {if(S.Visible[S.Stage]<20||S.Shot!=4){Finish(false,TEXT("insufficient actual visible samples/captures"));return;}if(++S.Stage==2){Finish(S.HiddenAfter>=20,TEXT("actual flight/reentry/roost-hide/bounds proof; inspect images"));return;}S.Phase=1;}
    }
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
