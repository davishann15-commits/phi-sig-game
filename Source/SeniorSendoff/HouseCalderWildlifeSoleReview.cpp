// Native diagnostic only: current rigid sole matrices actually sent to the ISM,
// measured against independent surface queries and the current stance phase.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderTerrain.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
bool AHouseCalderWildlifeDirector::ReadRenderedSolesForReview(int32 Index,FCalderWildlifeSoleProbe& Out)const
{
    if(!Runtime || !States.Items.IsValidIndex(Index) || !Runtime->Animals.IsValidIndex(Index))return false;
    const auto& A=Runtime->Animals[Index];if(A.Controls.Num()!=HouseCalderWildlifePose::Controls)return false;
    const double Now=HouseCalderWildlife::ServerTime(GetWorld());Out=FCalderWildlifeSoleProbe();Out.Tier=A.LastPoseTier;Out.PoseAge=Now-A.LastRoot;
    const FVector Chord=HouseCalderWildlifePaths::Evaluate(A.Route,FMath::Max(0.,Now-States.Items[Index].StartTime)*HouseCalderWildlifePose::Speed(States.Items[Index].Mood));
    Out.RouteSurfaceDifferenceCm=HouseCalderTerrain::SurfaceZ(Chord.X,Chord.Y)-Chord.Z;
    auto Reference=A;FTransform Root,Tail,Antlers,Contact;TArray<FTransform> ReferenceControls;
    HouseCalderWildlifePose::Evaluate(States.Items[Index],Reference,Now,HouseCalderWildlife::SceneTime(GetWorld()),Root,Tail,Antlers,Contact,ReferenceControls);
    for(int32 Leg=0;Leg<4;++Leg)
    {
        if(!Reference.Planted[Leg] || Runtime->Soles[Leg].IsEmpty())continue;
        Out.PlantedMask|=uint8(1<<Leg);Out.Cycles[Leg]=Reference.CurrentFootCycles[Leg];double Lowest=TNumericLimits<double>::Max();FVector Centre=FVector::ZeroVector;
        // Decode the compressed palette actually submitted to the ISM,
        // independently of the CPU transforms used to construct it.
        const int32 Joint=7+Leg*4,Stride=HouseCalderWildlifePose::CustomFloats,Offset=Index*Stride+Joint*6;
        if(DrawParts.IsEmpty()||DrawParts[0]->PerInstanceSMCustomData.Num()<(Index+1)*Stride)return false;
        const auto& Data=DrawParts[0]->PerInstanceSMCustomData;const double X=Data[Offset],Y=Data[Offset+1],Z=Data[Offset+2];
        const FQuat Q(X,Y,Z,FMath::Sqrt(FMath::Max(0.,1.-X*X-Y*Y-Z*Z)));
        const FTransform Submitted(Q,FVector(Data[Offset+3],Data[Offset+4],Data[Offset+5]),FVector(Data[Index*Stride+Stride-1]));
        for(const FVector& V:Runtime->Soles[Leg])
        {const FVector P=Submitted.TransformPosition(V);Centre+=P;Lowest=FMath::Min(Lowest,P.Z-HouseCalderTerrain::SurfaceZ(P.X,P.Y));}
        Out.Centres[Leg]=Centre/Runtime->Soles[Leg].Num();Out.Gaps[Leg]=FMath::Abs(Lowest);
    }
    return true;
}
#endif

#if !UE_BUILD_SHIPPING
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
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

bool AHouseCalderWildlifeDirector::StartSoleReviewGroup(int32 Habitat)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSoleReview")) || !HouseCalderWildlife::ScenePinned()
        || !HasAuthority() || !Ready() || !Runtime->Plan.Groups.IsValidIndex(Habitat))return false;
    const auto& Graph=Runtime->Plan.Groups[Habitat];const double Now=HouseCalderWildlife::ServerTime(GetWorld());
    for(int32 I=0;I<States.Items.Num();++I)if(States.Items[I].Habitat==Habitat)
    {
        auto& A=Runtime->Animals[I];auto& State=States.Items[I];FVector To=A.Home;double Best=0.;
        for(const FVector& P:Graph.Points)if(FVector::DistSquaredXY(P,A.Home)>Best){Best=FVector::DistSquaredXY(P,A.Home);To=P;}
        TArray<FVector> Route;if(!Runtime->Plan.Route(Habitat,A.Home,To,Route) || HouseCalderWildlifePaths::Length(Route)<3500.)return false;
        State.From=A.Home;State.To=A.Home;A.Route={A.Home};A.Controls.Reset();A.Escaped=true;A.ComeBackAt=Now+240.;
        ChangeState(I,ECalderWildlifeMood::Fleeing,To,Now,3);
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOLE_SETUP group=%d explicitValidGraphGaitSetup=1 ordinaryReactionVerdict=0"),Habitat);return true;
}
namespace HouseCalderWildlifeSoleReviewImpl
{
struct FVariant
{TArray<double> Gaps,Drifts;};
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AHouseCalderWildlifeDirector> Director;TWeakObjectPtr<ACameraActor> Camera;
    int32 Phase=0,Group=0,Shot=0,Frames=0;double Began=0.,StageAt=0.,NextShot=0.,MaxRouteSurfaceDifferenceCm=0.;
    FString Dir,Rows=TEXT("group,id,variant,frame,tier,pose_age_s,server_time,leg,cycle,x,y,z,gap_cm,drift_cm\n");
    TArray<FString> Captures;TMap<uint8,FVariant> Variants;TMap<int32,TPair<int64,FVector>> Anchors;TMap<int32,FVector> LastPositions;
};
FState S;
double P95(const TArray<double>& Values)
{if(Values.IsEmpty())return 0.;auto Copy=Values;Copy.Sort();return Copy[FMath::Min(Copy.Num()-1,FMath::FloorToInt(Copy.Num()*.95))];}
void Finish(bool Passed,const FString& Why)
{
    FString Text;for(int32 Variant=0;Variant<3;++Variant)
    {
        const FVariant* V=S.Variants.Find(uint8(Variant));const double Gap=V?P95(V->Gaps):0.,Drift=V?P95(V->Drifts):0.;
        const int32 Gaps=V?V->Gaps.Num():0,Drifts=V?V->Drifts.Num():0;
        Passed&=Gaps>=60 && Drifts>=30 && Gap<=3. && Drift<=3.;
        Text+=FString::Printf(TEXT("VARIANT id=%d gapSamples=%d driftSamples=%d gapP95cm=%.4f driftP95cm=%.4f\n"),Variant,Gaps,Drifts,Gap,Drift);
    }
    for(const FString& Path:S.Captures)Passed&=IFileManager::Get().FileSize(*Path)>0;
    Text=FString::Printf(TEXT("%s scope=nativeCurrentReducedPoseSoles frames=%d captures=%d maxGraphChordSurfaceDifferenceCm=%.4f explicitGraphGaitSetup=1 reactionAcceptance=0 physicsPathQueries=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.Frames,S.Captures.Num(),S.MaxRouteSurfaceDifferenceCm,*Why)+Text;
    FFileHelper::SaveStringToFile(Text,*(S.Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("soles.csv")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOLE_REVIEW %s"),*Text);S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool Standing(UWorld* World,AActor* Pawn,const FVector& At,double Angle,double Scale,FVector& Eye)
{
    const FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderSoleReviewCamera),true,Pawn);
    int32 FloorMiss=0,FloorMismatch=0,FloorSlope=0,GoodFloors=0,BlockedRays=0;double ClosestEnd=TNumericLimits<double>::Max(),BlockedHeight=0.;FVector BlockAt=FVector::ZeroVector;FString BlockName;
    for(const double Radius:{5000.,5500.,6000.,6500.,7000.,8000.,9000.,11000.,13000.})for(int32 Try=0;Try<73;++Try)
    {
        const double Offset=Try==0?0.:((Try+1)/2)*5.*(Try%2?1.:-1.);FVector P=At+FRotator(0,Angle+Offset,0).Vector()*Radius;
        const double Datum=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;
        if(!World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Datum+150.),FVector(P.X,P.Y,Datum-150.),ECC_Visibility,Q)){++FloorMiss;continue;}
        if(FMath::Abs(Hit.ImpactPoint.Z-Datum)>3.){++FloorMismatch;continue;}if(Hit.ImpactNormal.Z<.766){++FloorSlope;continue;}
        P.Z=Hit.ImpactPoint.Z+170.;if(FVector::Distance(P,At)<4500. || FVector::Distance(P,At)>14500.)continue;bool Blocked=false;
        ++GoodFloors;
        for(const double Height:{20.,80.,135.})
        {
            const FVector Target=At+FVector(0,0,Height*Scale);const bool Ray=World->LineTraceSingleByChannel(Hit,P,Target,ECC_Visibility,Q);Blocked|=Ray;
            if(Ray){++BlockedRays;const double End=FVector::Distance(Hit.ImpactPoint,Target);if(End<ClosestEnd){ClosestEnd=End;BlockedHeight=Height;BlockAt=Hit.ImpactPoint;BlockName=Hit.GetComponent()?Hit.GetComponent()->GetName():TEXT("none");}}
        }
        if(!Blocked){Eye=P;return true;}
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOLE_CAMERA_BLOCK at=%s surfaceZ=%.3f floorMiss=%d floorMismatch=%d floorSlope=%d goodFloors=%d blockedRays=%d nearestEndCm=%.3f targetHeight=%.1f hit=%s component=%s"),*At.ToString(),HouseCalderTerrain::SurfaceZ(At.X,At.Y),FloorMiss,FloorMismatch,FloorSlope,GoodFloors,BlockedRays,ClosestEnd,BlockedHeight,*BlockAt.ToString(),*BlockName);
    return false;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!World || !World->IsGameWorld() || !World->HasBegunPlay() || !FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSoleReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    const double Now=World->GetTimeSeconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Began=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSoleReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Phase<0)return;if(Now-S.Began>120.){Finish(false,TEXT("native medium-sole diagnostic timed out"));return;}
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    TActorIterator<AHouseCalderWildlifeDirector> It(World);auto* D=It?*It:nullptr;
    if(!PC || !Pawn || !D || !D->Ready())return;
    if(S.Phase==0)
    {
        if(HouseCalderWildlife::LogicProbe() || !HouseCalderWildlife::ScenePinned()){Finish(false,TEXT("native rendering and pinned background required"));return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
        if(AHUD* HUD=PC->GetHUD())HUD->bShowHUD=false;FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);S.Director=D;
        if(!S.Camera.IsValid()){Finish(false,TEXT("camera spawn failed"));return;}S.Camera->GetCameraComponent()->SetFieldOfView(65.f);S.Phase=1;
    }
    if(S.Phase==1)
    {
        if(!D->StartSoleReviewGroup(S.Group)){Finish(false,TEXT("no long validated habitat route"));return;}
        S.StageAt=Now;S.NextShot=Now+1.5;S.Shot=0;S.Anchors.Reset();S.LastPositions.Reset();S.Phase=2;D->SetTimingView(S.Group==0?TEXT("medium_woods_gait"):TEXT("medium_farm_gait"));return;
    }
    if(S.Phase==2)
    {
        const int32 Focus=S.Group==0?0:3;const FVector At=D->AnimalPosition(Focus);const auto& Focal=D->States.Items[Focus];const double Scale=Focal.Variant==1?1.11:Focal.Variant==2?.68:1.;FVector Eye;
        if(!Standing(World,Pawn,At,Focal.FacingYaw*360./65535.+90.,Scale,Eye)){Finish(false,TEXT("no clear physical170cm standing eye within medium-pose range"));return;}
        S.Camera->SetActorLocationAndRotation(Eye,(At+FVector(0,0,80.*Scale)-Eye).Rotation());PC->SetViewTarget(S.Camera.Get());
        if(Now-S.StageAt>=1.)
        {
            ++S.Frames;
            for(const auto& State:D->States.Items)if(State.Habitat==S.Group && State.Mood==ECalderWildlifeMood::Fleeing)
            {
                const FVector P=D->AnimalPosition(State.Id);const FVector* Before=S.LastPositions.Find(State.Id);const bool Moving=Before && FVector::DistSquared(P,*Before)>.01;S.LastPositions.Add(State.Id,P);
                FCalderWildlifeSoleProbe Probe;if(!Moving || !D->ReadRenderedSolesForReview(State.Id,Probe) || Probe.Tier!=1 || Probe.PoseAge>.025)continue;
                S.MaxRouteSurfaceDifferenceCm=FMath::Max(S.MaxRouteSurfaceDifferenceCm,FMath::Abs(Probe.RouteSurfaceDifferenceCm));
                auto& V=S.Variants.FindOrAdd(State.Variant);
                for(int32 Leg=0;Leg<4;++Leg)if(Probe.PlantedMask&(1<<Leg))
                {
                    const int32 Key=State.Id*4+Leg;double Drift=-1.;const auto* Anchor=S.Anchors.Find(Key);
                    if(Anchor && Anchor->Key==Probe.Cycles[Leg]){Drift=FVector::Distance(Probe.Centres[Leg],Anchor->Value);V.Drifts.Add(Drift);}
                    else S.Anchors.Add(Key,{Probe.Cycles[Leg],Probe.Centres[Leg]});
                    V.Gaps.Add(Probe.Gaps[Leg]);const FVector C=Probe.Centres[Leg];
                    S.Rows+=FString::Printf(TEXT("%d,%d,%d,%d,%d,%.6f,%.6f,%d,%lld,%.5f,%.5f,%.5f,%.5f,%.5f\n"),S.Group,State.Id,State.Variant,S.Frames,Probe.Tier,Probe.PoseAge,Now,Leg,static_cast<long long>(Probe.Cycles[Leg]),C.X,C.Y,C.Z,Probe.Gaps[Leg],Drift);
                }
            }
        }
        if(S.Shot<4 && Now>=S.NextShot && !FScreenshotRequest::IsScreenshotRequested())
        {
            const FString Path=S.Dir/FString::Printf(TEXT("detail_wild_medium_%d_%02d.png"),S.Group,S.Shot);FScreenshotRequest::RequestScreenshot(Path,false,false);S.Captures.Add(Path);++S.Shot;S.NextShot=Now+1.2;
            UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOLE_CAMERA group=%d shot=%d eye=%s localGroundEyeCm=170 distanceCm=%.1f"),S.Group,S.Shot,*Eye.ToString(),FVector::Distance(Eye,At));
        }
        if(Now-S.StageAt>=6.5 && S.Shot==4 && !FScreenshotRequest::IsScreenshotRequested()
            && IFileManager::Get().FileSize(*S.Captures.Last())>0)
        {
            if(++S.Group<2){S.Phase=1;return;}
            Finish(true,TEXT("actual current reduced-pose sole positions; independent current stance and surface queries; per-variant thresholds unchanged"));
        }
    }
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
