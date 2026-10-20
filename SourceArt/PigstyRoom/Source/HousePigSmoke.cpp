#include "HouseRoamingPig.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Scalability.h"

namespace
{
struct FPigSmoke
{
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs;
    TWeakObjectPtr<AExistingHouseDoor> Door;
    TWeakObjectPtr<AHousePigSnackPile> Food;
    int32 Stage=0, FoodStarted=0, IndependentFrames=0;
    double Began=0, StageBegan=0;
    float AirSeconds[2]={0,0};
    FVector Start[2];
    FString Capture;
    TWeakObjectPtr<AExistingHouseDoor> TestGate;
    bool bSettingsApplied=false, bClosingTestGate=false;
    bool ReachedDownstairs[2]={false,false}, ReachedGreatRoom[2]={false,false};
    int32 LastLoggedStage=INDEX_NONE;
    double NextProgressLog=0;
};
bool SendE(APlayerController* PC,EInputEvent Event)
{
    if (!PC||!PC->PlayerInput||!FSlateApplication::IsInitialized()) return false;
    PC->InputKey(FInputKeyEventArgs(nullptr,FSlateApplication::Get().GetInputDeviceIdForMouse(),EKeys::E,Event,1.f,false,0)); return true;
}
bool PlacePlayer(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,FVector Feet,FVector Look)
{
    FCollisionQueryParams Query; Query.AddIgnoredActor(Pawn);
    for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
    FHitResult Floor;
    if (!World->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,35),Feet-FVector(0,0,70),ECC_Visibility,Query)||Floor.ImpactNormal.Z<.65f) return false;
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Center=Floor.ImpactPoint+FVector(0,0,Half+2.f);
    FCollisionQueryParams SpaceQuery; SpaceQuery.AddIgnoredActor(Pawn);
    if (World->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(
        Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),SpaceQuery)) return false;
    Pawn->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    const FRotator Aim=(Look-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(Aim); Pawn->SetActorRotation(FRotator(0,Aim.Yaw,0)); return true;
}
}
#endif

void TickHousePigSmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    const bool StairSmoke=FParse::Param(FCommandLine::Get(),TEXT("HousePigStairSmoke"));
    if (!World||(!FParse::Param(FCommandLine::Get(),TEXT("HousePigSmoke"))&&!StairSmoke)
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    static FPigSmoke S;
    if (S.World.Get()!=World) { S=FPigSmoke(); S.World=World; }
    if (S.Stage<0) return;
    const double Now=FPlatformTime::Seconds(); if (!S.Began) S.Began=Now;
    if (!S.bSettingsApplied && GEngine)
    {
        Scalability::FQualityLevels Quality; Quality.SetFromSingleQualityLevel(1);
        Scalability::SetQualityLevels(Quality);
        GEngine->Exec(World,TEXT("r.SetRes 1280x720w"));
        GEngine->Exec(World,TEXT("r.ScreenPercentage 100"));
        GEngine->Exec(World,TEXT("t.MaxFPS 60"));
        if (AGameModeBase* Mode=World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
        S.bSettingsApplied=true;
        auto Value=[](const TCHAR* Name) { const IConsoleVariable* V=IConsoleManager::Get().FindConsoleVariable(Name); return V?V->GetInt():INDEX_NONE; };
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_PROFILE quality=1 resolution=1280x720 runtimeOnly=1 savedSettings=0 shadowQuality=%d giQuality=%d reflectionQuality=%d"),
            Value(TEXT("sg.ShadowQuality")),Value(TEXT("sg.GlobalIlluminationQuality")),Value(TEXT("sg.ReflectionQuality")));
    }
    auto Finish=[&](bool Pass,const FString& Detail)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_RESULT passed=%d independentFrames=%d detail=%s"),Pass?1:0,S.IndependentFrames,*Detail);
        if (StairSmoke) UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_STAIR_SMOKE_RESULT passed=%d downstairs=[%d,%d] greatRoom=[%d,%d] detail=%s"),
            Pass?1:0,S.ReachedDownstairs[0]?1:0,S.ReachedDownstairs[1]?1:0,S.ReachedGreatRoom[0]?1:0,S.ReachedGreatRoom[1]?1:0,*Detail);
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (const AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_PIG name=%s feet=%s HP=%.1f snacks=%d escapes=%d hallNodes=%d travel=%.1f gait=%.1f floorStops=%d"),
                    *Pig->PigName,*Pig->FeetLocation().ToString(),Pig->Health,Pig->SnacksEaten,Pig->Escapes,Pig->HallNodesVisited,Pig->TravelDistance,Pig->GaitDistance,Pig->UnsafeFloorStops);
        S.Stage=-1; FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    if (Now-S.Began>(StairSmoke?200.:100.)) { Finish(false,TEXT("Native feeding/escape/house traversal timed out")); return; }
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn||!PC->PlayerInput||!World->GetGameViewport()) return;
    if (S.Stage!=S.LastLoggedStage)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_STAGE stage=%d elapsed=%.2f"),S.Stage,Now-S.Began);
        S.LastLoggedStage=S.Stage;
    }
    if (S.Stage>0 && Now>=S.NextProgressLog)
    {
        S.NextProgressLog=Now+3.;
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (const AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_PROGRESS stage=%d name=%s feet=%s escapes=%d hall=%d %s"),
                    S.Stage,*Pig->PigName,*Pig->FeetLocation().ToString(),Pig->Escapes,Pig->HallNodesVisited,*Pig->MovementDebug());
    }
    if (S.Stage==0)
    {
        if (Now-S.Began<3.) return;
        for (TActorIterator<AHouseRoamingPig> It(World);It;++It) S.Pigs.Add(*It);
        if (S.Pigs.Num()!=2) { Finish(false,TEXT("Expected exactly two installed pigs")); return; }
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It) if (It->DoorId==TEXT("HallEnd_West")) { S.Door=*It; break; }
        S.Food=S.Pigs[0]->SnackPile;
        if (!S.Door.IsValid()||S.Door->bLocked||!S.Food.IsValid()||S.Pigs[1]->SnackPile!=S.Food.Get())
        { Finish(false,TEXT("Pigsty gate or shared edible snack pile is unconfigured")); return; }
        if (S.Door->IsOpen()) { Finish(false,TEXT("Test starts with the pigsty door closed; map gate was already open")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); S.Start[i]=Pig->FeetLocation();
            if (!Pig->BodyMesh||!Pig->HeadMesh||!Pig->UpperLegMesh||!Pig->LowerLegMesh||!Pig->IsInsidePigsty())
            { Finish(false,TEXT("Missing pig meshes or spawn outside the pigsty")); return; }
            int32 Outside=INDEX_NONE;
            for (int32 n=0;n<Pig->RoamGraph.Num();++n) if (!Pig->RoamGraph[n].bInsidePigsty) { Outside=n; break; }
            if (Outside==INDEX_NONE||Pig->RequestRoamNode(Outside)) { Finish(false,TEXT("Closed pigsty gate permits an escape route")); return; }
            if (!Pig->RequestFeeding()) { Finish(false,TEXT("Pig cannot start physical feeding approach")); return; }
        }
        const float Before=S.Pigs[0]->Health;
        AHouseRoamingPig* HurtPig=S.Pigs[0].Get();
        const FVector BodyPoint=HurtPig->FeetLocation()+HurtPig->GetActorForwardVector()*8.5f+FVector(0,0,35);
        const FVector TraceStart=BodyPoint+HurtPig->GetActorRightVector()*150.f;
        FCollisionQueryParams WeaponQuery(SCENE_QUERY_STAT(PigSmokeWeaponTrace),false,Pawn); FHitResult WeaponHit;
        if (!World->LineTraceSingleByChannel(WeaponHit,TraceStart,BodyPoint,ECC_Visibility,WeaponQuery)||WeaponHit.GetActor()!=HurtPig)
        { Finish(false,TEXT("Existing weapon visibility channel does not target the pig body")); return; }
        const float Damage=UGameplayStatics::ApplyPointDamage(HurtPig,25.f,(BodyPoint-TraceStart).GetSafeNormal(),WeaponHit,PC,Pawn,UDamageType::StaticClass());
        if (FMath::Abs(Damage-25.f)>.01f||FMath::Abs(S.Pigs[0]->Health-(Before-25.f))>.01f||S.Pigs[1]->Health!=S.Pigs[1]->MaxHealth)
        { Finish(false,TEXT("Actual per-pig TakeDamage health update failed")); return; }
        S.Pigs[0]->RequestFeeding();
        S.FoodStarted=S.Food->FoodRemaining;
        if (!PlacePlayer(World,Pawn,PC,FVector(-2675,-1325,302.3),FVector(-2675,-1110,420)))
        { Finish(false,TEXT("Feeding healthbar viewpoint is blocked")); return; }
        S.Stage=1; S.StageBegan=Now; return;
    }
    for (int32 i=0;i<2;++i)
    {
        AHouseRoamingPig* Pig=S.Pigs[i].Get();
        if (!Pig) { Finish(false,TEXT("Pig actor disappeared")); return; }
        FCollisionQueryParams Query; Query.AddIgnoredActor(Pawn);
        for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
        FHitResult Floor;
        const FVector Feet=Pig->FeetLocation();
        const bool Supported=World->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,25),Feet-FVector(0,0,75),ECC_Visibility,Query)
            && Floor.ImpactNormal.Z>.5f && Feet.Z-Floor.ImpactPoint.Z<60.f;
        S.AirSeconds[i]=Supported?0:S.AirSeconds[i]+World->GetDeltaSeconds();
        if (S.AirSeconds[i]>.7f||Feet.Z< -80.f) { Finish(false,TEXT("Pig lost safe floor support")); return; }
        if (S.Stage>=11)
        {
            S.ReachedDownstairs[i]|=Feet.Z<50.f&&Feet.X< -1850.f&&Feet.Y<850.f;
            S.ReachedGreatRoom[i]|=Feet.Z<50.f&&Feet.X> -1285.f&&Feet.Y< -550.f;
        }
    }
    if (FVector::Dist2D(S.Pigs[0]->FeetLocation()-S.Start[0],S.Pigs[1]->FeetLocation()-S.Start[1])>20.f) ++S.IndependentFrames;
    if (S.Stage==1)
    {
        if (S.Door->IsOpen()||!S.Pigs[0]->IsInsidePigsty()||!S.Pigs[1]->IsInsidePigsty())
        { Finish(false,TEXT("Pigs opened or crossed a closed door")); return; }
        if (Now-S.StageBegan<5.||S.Pigs[0]->SnacksEaten<1||S.Pigs[1]->SnacksEaten<1) return;
        if (S.Food->FoodRemaining>=S.FoodStarted||S.Food->Cheetos->GetInstanceCount()!=S.Food->FoodRemaining
            ||S.Pigs[0]->Health<=75.f||S.Pigs[0]->DamageEvents!=1)
        { Finish(false,TEXT("Eating did not remove visible food and heal the damaged pig")); return; }
        FVector LabelA,LabelB; FVector2D ScreenA,ScreenB;
        if (!S.Pigs[0]->ShouldShowHealthTo(PC,LabelA)||!S.Pigs[1]->ShouldShowHealthTo(PC,LabelB))
        { Finish(false,TEXT("Independent healthbars are outside the feeding HUD context")); return; }
        int32 Width=0,Height=0; PC->GetViewportSize(Width,Height);
        const float Scale=FMath::Clamp(Width/1400.f,.65f,1.25f);
        auto LabelFits=[&](const FVector& WorldLabel,FVector2D& Screen)
        {
            if (!PC->ProjectWorldLocationToScreen(WorldLabel,Screen,true)) return false;
            const float HalfWidth=64*Scale;
            return Screen.X>=HalfWidth+10*Scale && Screen.X<=Width-HalfWidth-10*Scale
                && Screen.Y>=150*Scale && Screen.Y+36*Scale<=Height-140*Scale;
        };
        if (!LabelFits(LabelA,ScreenA)||!LabelFits(LabelB,ScreenB))
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_HEALTH_PROJECTION_FAILED viewport=%dx%d labels=[%s,%s] camera=%s"),
                Width,Height,*ScreenA.ToString(),*ScreenB.ToString(),*Pawn->FirstPersonCamera->GetComponentLocation().ToString());
            Finish(false,TEXT("Pig health label rectangles would be clipped or covered in the feeding snapshot")); return;
        }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_HEALTH_PROJECTED viewport=%dx%d labels=[%s,%s]"),Width,Height,*ScreenA.ToString(),*ScreenB.ToString());
        S.Capture=FPaths::ProjectSavedDir()/TEXT("HousePigSmoke/pig_health_feeding.png");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.Capture),true); IFileManager::Get().Delete(*S.Capture,false,true);
        FScreenshotRequest::RequestScreenshot(S.Capture,true,false);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_FEEDING HP=[%.1f,%.1f] snacks=[%d,%d] food=%d visibleFood=%d capture=%s"),
            S.Pigs[0]->Health,S.Pigs[1]->Health,S.Pigs[0]->SnacksEaten,S.Pigs[1]->SnacksEaten,S.Food->FoodRemaining,S.Food->Cheetos->GetInstanceCount(),*S.Capture);
        S.Stage=2; S.StageBegan=Now; return;
    }
    if (S.Stage==2)
    {
        if (Now-S.StageBegan<.5) return;
        if (IFileManager::Get().FileSize(*S.Capture)<=0) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Feeding healthbar snapshot was not saved")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2130,-1245,302.3),S.Door->DoorwayCenter))
        { Finish(false,TEXT("Player cannot stand at pigsty gate")); return; }
        S.Stage=3; S.StageBegan=Now; return;
    }
    if (S.Stage==3)
    {
        if (Now-S.StageBegan<.2) return;
        if (Pawn->FocusedDoorForInteraction()!=S.Door.Get()||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual player E gate-opening input cannot focus door")); return; }
        S.Stage=4; S.StageBegan=Now; return;
    }
    if (S.Stage==4) { SendE(PC,IE_Released); S.Stage=5; return; }
    if (S.Stage==5)
    {
        if (Now-S.StageBegan<1.) return;
        if (!S.Door->IsOpen()||S.Door->IsActorTickEnabled()) { Finish(false,TEXT("Player E did not finish opening pigsty gate")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2334.8,-1245,380)))
        { Finish(false,TEXT("Post-opening observer position is blocked")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); int32 Goal=INDEX_NONE;
            const FVector Aim(-2177,i==0?-350.f:300.f,302.3); float Best=BIG_NUMBER;
            for (int32 n=0;n<Pig->RoamGraph.Num();++n)
                if (!Pig->RoamGraph[n].bInsidePigsty)
                { const float D=FVector::DistSquared(Pig->RoamGraph[n].Feet,Aim); if (D<Best) { Goal=n; Best=D; } }
            if (!Pig->RequestRoamNode(Goal)) { Finish(false,TEXT("Player-open gate has no safe house graph route")); return; }
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_ESCAPE_GOAL name=%s node=%d %s"),*Pig->PigName,Goal,*Pig->MovementDebug());
        }
        S.Stage=6; S.StageBegan=Now; return;
    }
    if (S.Stage==6)
    {
        if (S.Pigs[0]->Escapes<1||S.Pigs[1]->Escapes<1||S.Pigs[0]->HallNodesVisited<3||S.Pigs[1]->HallNodesVisited<3) return;
        if (S.IndependentFrames<15||S.Pigs[0]->TravelDistance<400||S.Pigs[1]->TravelDistance<400
            ||S.Pigs[0]->GaitDistance<200||S.Pigs[1]->GaitDistance<200)
        { Finish(false,TEXT("Pigs did not independently walk with actual gait and travel")); return; }
        if (!StairSmoke)
        { Finish(true,TEXT("Two independent pigs ate finite visible food, own HP/healing/HUD passed, closed gate held, actual player E enabled physical escape and house-hall wandering; floor support remained valid")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_MAIN_CHECKS_PASSED continuingToStairs=1"));
        S.Stage=11; S.StageBegan=Now; return;
    }
    if (S.Stage==11 || S.Stage==15)
    {
        const bool GreatGate=S.Stage==15;
        const FName Id=GreatGate?FName(TEXT("SM_OriginalLeaf_Entrance_m1_m1_Solid_01")):FName(TEXT("SM_OriginalLeaf_Upper_p0_m2_Solid_04"));
        S.TestGate.Reset();
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It) if (It->DoorId==Id) { S.TestGate=*It; break; }
        if (!S.TestGate.IsValid()||S.TestGate->bLocked) { Finish(false,TEXT("Required house traversal gate is missing/locked")); return; }
        const FVector Feet=GreatGate?FVector(-1500,-715.5,8):FVector(-2177,735,302.3);
        if (!PlacePlayer(World,Pawn,PC,Feet,S.TestGate->DoorwayCenter)) { Finish(false,TEXT("Player house-gate standing approach is blocked")); return; }
        S.bClosingTestGate=S.TestGate->IsOpen();
        S.Stage=12; S.StageBegan=Now; return;
    }
    if (S.Stage==12)
    {
        if (Now-S.StageBegan<.2) return;
        if (Pawn->FocusedDoorForInteraction()!=S.TestGate.Get()||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual player E cannot focus house traversal gate")); return; }
        S.Stage=13; S.StageBegan=Now; return;
    }
    if (S.Stage==13) { SendE(PC,IE_Released); S.Stage=14; return; }
    if (S.Stage==14)
    {
        if (Now-S.StageBegan<1.) return;
        if (!S.TestGate.IsValid()||S.TestGate->IsActorTickEnabled()) return;
        if (S.bClosingTestGate)
        {
            if (S.TestGate->IsOpen()) { Finish(false,TEXT("House gate did not close before actual E opening test")); return; }
            S.bClosingTestGate=false; S.Stage=12; S.StageBegan=Now; return;
        }
        if (!S.TestGate->IsOpen()) { Finish(false,TEXT("Actual player E did not open house traversal gate")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_STAIR_SMOKE_GATE_OPENED_BY_PLAYER id=%s"),*S.TestGate->DoorId.ToString());
        if (S.TestGate->DoorId==TEXT("SM_OriginalLeaf_Upper_p0_m2_Solid_04")) { S.Stage=15; S.StageBegan=Now; return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2334.8,-1245,380)))
        { Finish(false,TEXT("Observer cannot stand clear of the pig traversal route")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); int32 Goal=INDEX_NONE; float Best=BIG_NUMBER;
            const FVector Target(i==0?-1071.f:-1251.f,-715.5,8);
            for (int32 n=0;n<Pig->RoamGraph.Num();++n)
            { const float Distance=FVector::DistSquared(Pig->RoamGraph[n].Feet,Target); if (Distance<Best) { Goal=n; Best=Distance; } }
            if (!Pig->RequestRoamNode(Goal)) { Finish(false,TEXT("Player-open house gates still have no physical downstairs path")); return; }
        }
        S.Stage=16; S.StageBegan=Now; return;
    }
    if (S.Stage==16)
    {
        if (!S.ReachedDownstairs[0]||!S.ReachedDownstairs[1]||!S.ReachedGreatRoom[0]||!S.ReachedGreatRoom[1]) return;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_STAIR_SMOKE_TRAVERSED downstairs=2 greatRoom=2 elapsed=%.2f"),Now-S.StageBegan);
        Finish(true,TEXT("Both pigs physically descended the registered staircase and wandered into the downstairs great room after actual player E opened both gates; floor checks ignored all pawns and stayed supported"));
    }
#endif
}
