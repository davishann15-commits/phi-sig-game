// Independent ordinary-input farm route. Original routes and movement settings stay fixed.
#if !UE_BUILD_SHIPPING
#include "HouseCalderFarmSolid.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderFarmSolidWalkReviewImpl
{
const FVector2D Start(-21600,500);
const FVector2D Goals[]={
    {-22600,500},{-24000,-500},{-25800,-3600},{-27200,-3600},
    {-27200,-3000},{-27200,-1800},{-27200,-3600},{-25800,-3600},
    {-24000,-500},{-22600,500},{-21600,500}
};
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Walker;
    double StartTime=0,LegStart=0,Progress=0,FinishAt=0;
    int32 Phase=0,Goal=0,Reached=0,Profiles=0;
    float Best=MAX_flt;
    FVector Target;
    FString Dir,Report;
    TArray<FString> Captures;
};
FState State;

bool Floor(UWorld* World,AActor* Ignore,const FVector2D& Point,float& Z)
{
    const float Datum=HouseExteriorSite::GroundHeight(Point.X,Point.Y);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderFarmWalkFloor),true);Query.AddIgnoredActor(Ignore);
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit,FVector(Point,Datum+120),FVector(Point,Datum-120),ECC_Visibility,Query)) return false;
    Z=Hit.ImpactPoint.Z;return Hit.ImpactNormal.Z>=.766f && FMath::Abs(Z-Datum)<=3.f;
}
void Note(const FString& Line)
{State.Report+=Line+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_SOLID_WALK %s"),*Line);}
void Finish(bool Passed,const FString& Reason)
{
    if (State.Walker.IsValid()) State.Walker->GetCharacterMovement()->StopMovementImmediately();
    State.Phase=-1;
    Note(FString::Printf(TEXT("%s reached=%d/%d profiles=%d terrainHash=%08x %s"),Passed?TEXT("PASS"):TEXT("FAIL"),
        State.Reached,int32(UE_ARRAY_COUNT(Goals)),State.Profiles,HouseCalderTerrain::Hash(),*Reason));
    FFileHelper::SaveStringToFile(State.Report,*(State.Dir/TEXT("result.txt")));
    FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool Profiles(UWorld* World,AStoryFirstPersonCharacter* Pawn)
{
    const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector2D Previous=Start;FCollisionQueryParams Query(SCENE_QUERY_STAT(CalderFarmWalkProfile),true);Query.AddIgnoredActor(Pawn);
    int32 Failures=0;FString Csv=TEXT("leg,sample,x,y,z,floor,clear\n");
    for (int32 Leg=0;Leg<int32(UE_ARRAY_COUNT(Goals));++Leg)
    {
        const FVector2D End=Goals[Leg];const int32 Count=FMath::CeilToInt(FVector2D::Distance(Previous,End)/25.);
        for (int32 I=0;I<=Count;++I)
        {
            const FVector2D Point=FMath::Lerp(Previous,End,double(I)/Count);float Z=0;
            const bool Found=Floor(World,Pawn,Point,Z);
            const bool Clear=Found && !World->OverlapBlockingTestByChannel(FVector(Point,Z+Half+3),FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeCapsule(FMath::Max(1.f,Radius-1),Half-1),Query);
            ++State.Profiles;if (!Found || !Clear) ++Failures;
            Csv+=FString::Printf(TEXT("%d,%d,%.3f,%.3f,%.3f,%d,%d\n"),Leg,I,Point.X,Point.Y,Z,int32(Found),int32(Clear));
        }
        Previous=End;
    }
    FFileHelper::SaveStringToFile(Csv,*(State.Dir/TEXT("profiles.csv")));
    Note(FString::Printf(TEXT("PROFILE %s samples=%d failures=%d actualPawnRadius=%.2f actualPawnHalf=%.2f"),Failures?TEXT("FAIL"):TEXT("PASS"),State.Profiles,Failures,Radius,Half));
    return Failures==0;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderFarmSolidWalkReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    const double Now=World->GetTimeSeconds();
    if (State.World.Get()!=World) {State=FState();State.World=World;State.StartTime=Now;}
    if (State.Phase<0) return;
    APlayerController* PC=World->GetFirstPlayerController();AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn || Now-State.StartTime<8 || !HouseCalderTerrain::CollisionReady(World)) return;
    State.Walker=Pawn;
    if (State.Phase==0)
    {
        State.Dir=FPaths::ProjectSavedDir()/TEXT("CalderFarmSolidWalkReview");IFileManager::Get().MakeDirectory(*State.Dir,true);
        if (!HouseCalderFarmSolid::Enabled()) {Finish(false,TEXT("farm solid trial required"));return;}
        if (!Profiles(World,Pawn)) {Finish(false,TEXT("authored gate/barn route has a physical obstruction or floor gap"));return;}
        float Z=0;if (!Floor(World,Pawn,Start,Z)) {Finish(false,TEXT("no start floor"));return;}
        const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // One setup teleport, then only normal movement input along every leg.
        Pawn->SetActorLocation(FVector(Start,Z+Half+4),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        if (AHUD* Hud=PC->GetHUD()) Hud->bShowHUD=false;
        State.Phase=1;
    }
    if (State.Phase==1)
    {
        float Z=0;if (!Floor(World,Pawn,Goals[State.Goal],Z)) {Finish(false,TEXT("no goal floor"));return;}
        State.Target=FVector(Goals[State.Goal],Z);State.LegStart=State.Progress=Now;State.Best=MAX_flt;State.Phase=2;
        PC->SetControlRotation(FRotator(-5,(State.Target-Pawn->GetActorLocation()).Rotation().Yaw,0));return;
    }
    if (State.Phase==2)
    {
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const float Distance=FVector::Dist2D(Feet,State.Target);
        if (Distance<State.Best-5) {State.Best=Distance;State.Progress=Now;}
        if (Now-State.Progress>8 || Now-State.LegStart>30)
        {Finish(false,FString::Printf(TEXT("walking stalled goal=%d feet=%s distance=%.2f"),State.Goal,*Feet.ToString(),Distance));return;}
        if (Distance>35 || FMath::Abs(Feet.Z-State.Target.Z)>8) return;
        Pawn->GetCharacterMovement()->StopMovementImmediately();++State.Reached;
        Note(FString::Printf(TEXT("WALK reached=%d/%d feet=%s realMovementInput=1"),State.Reached,int32(UE_ARRAY_COUNT(Goals)),*Feet.ToString()));
        if (State.Reached==1 || State.Reached==6 || State.Reached==11)
        {
            const FString Capture=State.Dir/FString::Printf(TEXT("Farm_%02d.png"),State.Reached);
            FScreenshotRequest::RequestScreenshot(Capture,false,false);State.Captures.Add(Capture);
        }
        if (++State.Goal<int32(UE_ARRAY_COUNT(Goals))) State.Phase=1;
        else {State.Phase=3;State.FinishAt=Now+2;}
        return;
    }
    if (State.Phase==3 && Now>=State.FinishAt && !FScreenshotRequest::IsScreenshotRequested())
    {
        bool Images=State.Captures.Num()==3;
        for (const FString& Capture:State.Captures) Images &= IFileManager::Get().FileSize(*Capture)>0;
        Finish(Images,TEXT("gate both directions; approach and open barn gable/interior; three saved captures"));
    }
}
struct FRegistration
{
    FRegistration()
    {
        FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);
        FCoreDelegates::OnBeginFrame.AddLambda([]
        {if (State.Phase==2 && State.Walker.IsValid()) State.Walker->AddMovementInput((State.Target-State.Walker->GetActorLocation()).GetSafeNormal2D(),1.f);});
    }
} Registration;
}
#endif
