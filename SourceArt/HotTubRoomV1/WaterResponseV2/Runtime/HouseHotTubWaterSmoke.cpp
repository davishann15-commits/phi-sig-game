// Opt-in native proof of the spa interaction. Only the initial dry stair
// approach is placed; entry, wading and exit use actual character movement.
#include "HouseHotTubWater.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
struct FSpaWaterSmoke
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseHotTubWater> Water;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    int32 Stage = 0, LastStage = INDEX_NONE, Target = 0;
    int32 EntriesBefore = 0, MotionBefore = 0, ExitsBefore = 0, SplashBefore = 0;
    int32 InitialActors = 0, FinalActors = 0, InitialComponents = 0;
    int32 MaxActiveSplash = 0, WadingFrames = 0, EntryFrames = 0;
    float Gravity = 0, WalkSpeed = 0, MaxHeight = 0, MaxEnergy = 0;
    float WadingDistance = 0, StairHeight = 0, JumpPeak = 0;
    float DecayInitialEnergy = 0, DecayFinalEnergy = 0, StationaryMotion = 0;
    float LastFrameAt = 0;
    bool bSavedJets = true, bJumpHeld = false, bEntered = false, bExited = false;
    bool bEntryCapture = false, bWadeCapture = false, bLookIgnored = false, bMoveIgnored = false, bCursor = false;
    double WallStarted = 0, Started = 0, StageStarted = 0, EntryAt = 0;
    FVector LastFeet = FVector::ZeroVector;
    FString Evidence;
    TArray<FString> Captures;
};

int32 SpaActorCount(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed()) ++Count;
    return Count;
}

FVector SpaFeet(AStoryFirstPersonCharacter* Pawn)
{
    return Pawn->GetActorLocation() - FVector(0, 0,
        Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

bool SpaPlaceDryApproach(UWorld* World, AHouseHotTubWater* Water,
    AStoryFirstPersonCharacter* Pawn, APlayerController* PC)
{
    // Authored stairs are at local X121, local Y291/255/219/183.
    // The approach is dry and inside the existing south wall.
    const FVector Approach = Water->GetActorTransform().TransformPosition(FVector(121, 305, 0));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SpaWaterDrySetup), false, Pawn);
    for (TActorIterator<APawn> It(World); It; ++It) Query.AddIgnoredActor(*It);
    FHitResult Floor;
    if (!World->LineTraceSingleByChannel(Floor, Approach + FVector(0, 0, 80),
        Approach - FVector(0, 0, 180), ECC_Pawn, Query) || Floor.ImpactNormal.Z < .8f)
        return false;
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Center = Floor.ImpactPoint + FVector(0, 0, Half + 2.15f);
    if (Water->IsPointInsideWater(Floor.ImpactPoint) || World->OverlapBlockingTestByChannel(
        Center, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, Half), Query)) return false;
    Pawn->SetActorLocation(Center, false, nullptr, ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->StopMovementImmediately();
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    PC->SetControlRotation(FRotator(-46, Water->GetActorRotation().Yaw - 90, 0));
    if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
    UE_LOG(LogTemp, Display, TEXT("HOUSE_HOT_TUB_WATER_DRY_SETUP center=%s floor=%s"),
        *Center.ToCompactString(), *Floor.ImpactPoint.ToCompactString());
    return true;
}

void SpaCapture(FSpaWaterSmoke& S, const TCHAR* Name)
{
    if (FScreenshotRequest::IsScreenshotRequested()) return;
    const FString Path = FPaths::Combine(S.Evidence, FString(Name) + TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Path, false, false);
    S.Captures.Add(Path);
}
}

static void RunHouseHotTubWaterSmoke(UWorld* World)
{
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("HouseHotTubWaterSmoke"))
        || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    static FSpaWaterSmoke S;
    if (S.World.Get() != World)
    {
        S = FSpaWaterSmoke(); S.World = World;
        S.WallStarted = FPlatformTime::Seconds(); S.Started = World->GetTimeSeconds();
        FParse::Value(FCommandLine::Get(), TEXT("HouseHotTubWaterEvidence="), S.Evidence, false);
        if (S.Evidence.IsEmpty()) S.Evidence = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HouseHotTubWaterSmoke"));
        S.Evidence = FPaths::ConvertRelativePathToFull(S.Evidence);
        IFileManager::Get().MakeDirectory(*S.Evidence, true);
    }
    if (S.Stage < 0) return;
    APlayerController* PC = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = PC ? Cast<AStoryFirstPersonCharacter>(PC->GetPawn()) : nullptr;
    AHouseHotTubWater* Water = S.Water.Get();
    const double Now = World->GetTimeSeconds();
    auto Finish = [&](bool bPass, const FString& Detail)
    {
        if (Pawn && S.bJumpHeld) Pawn->StopJumping();
        if (Water) Water->bJetsRunning = S.bSavedJets;
        S.FinalActors = SpaActorCount(World);
        TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetBoolField(TEXT("passed"), bPass);
        Report->SetStringField(TEXT("detail"), Detail);
        Report->SetNumberField(TEXT("stage"), S.Stage);
        Report->SetNumberField(TEXT("seconds"), Now - S.Started);
        Report->SetStringField(TEXT("movementMethod"), TEXT("One dry stair-approach setup placement, then ordinary CharacterMovement stair walking, native character jump, wading input and physical exit. No water-entry or progress teleports and no injected water impulses."));
        Report->SetBoolField(TEXT("enteredPhysically"), S.bEntered);
        Report->SetBoolField(TEXT("exitedPhysically"), S.bExited);
        Report->SetNumberField(TEXT("climbedStairHeightCm"), S.StairHeight);
        Report->SetNumberField(TEXT("jumpPeakFeetAboveWaterCm"), S.JumpPeak);
        Report->SetNumberField(TEXT("wadingDistanceCm"), S.WadingDistance);
        Report->SetNumberField(TEXT("wadingFrames"), S.WadingFrames);
        Report->SetNumberField(TEXT("entryEffectFrames"), S.EntryFrames);
        Report->SetNumberField(TEXT("maximumActiveSplashParticles"), S.MaxActiveSplash);
        Report->SetNumberField(TEXT("maximumDisplacementCm"), S.MaxHeight);
        Report->SetNumberField(TEXT("maximumWaveEnergy"), S.MaxEnergy);
        Report->SetNumberField(TEXT("stationaryEnergyInitial"), S.DecayInitialEnergy);
        Report->SetNumberField(TEXT("stationaryEnergyFinal"), S.DecayFinalEnergy);
        Report->SetNumberField(TEXT("worldActorsBefore"), S.InitialActors);
        Report->SetNumberField(TEXT("worldActorsAfter"), S.FinalActors);
        if (Pawn)
        {
            Report->SetStringField(TEXT("finalPawnCenter"), Pawn->GetActorLocation().ToCompactString());
            Report->SetStringField(TEXT("finalPawnFeet"), SpaFeet(Pawn).ToCompactString());
            Report->SetStringField(TEXT("finalPawnVelocity"), Pawn->GetCharacterMovement()->Velocity.ToCompactString());
        }
        if (Water)
        {
            Report->SetNumberField(TEXT("entryImpulses"), Water->PawnEntryImpulseCount - S.EntriesBefore);
            Report->SetNumberField(TEXT("motionImpulses"), Water->PawnMotionImpulseCount - S.MotionBefore);
            Report->SetNumberField(TEXT("exitImpulses"), Water->PawnExitImpulseCount - S.ExitsBefore);
            Report->SetNumberField(TEXT("splashBursts"), Water->SplashBurstCount - S.SplashBefore);
            Report->SetNumberField(TEXT("finalActiveSplashParticles"), Water->ActiveSplashParticleCount);
            Report->SetBoolField(TEXT("jetsRestored"), Water->bJetsRunning == S.bSavedJets);
        }
        TArray<TSharedPtr<FJsonValue>> Captures;
        for (const FString& Path : S.Captures) Captures.Add(MakeShared<FJsonValueString>(Path));
        Report->SetArrayField(TEXT("captures"), Captures);
        FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *FPaths::Combine(S.Evidence, TEXT("result.json")));
        UE_LOG(LogTemp, Display, TEXT("HOUSE_HOT_TUB_WATER_SMOKE_RESULT passed=%d stage=%d entry=%d motion=%d splash=%d distance=%.2f detail=%s"),
            bPass ? 1 : 0, S.Stage, Water ? Water->PawnEntryImpulseCount - S.EntriesBefore : 0,
            Water ? Water->PawnMotionImpulseCount - S.MotionBefore : 0,
            Water ? Water->SplashBurstCount - S.SplashBefore : 0, S.WadingDistance, *Detail);
        S.Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
    };
    auto Stage = [&](int32 Next) { S.Stage = Next; S.StageStarted = Now; };
    if (FPlatformTime::Seconds() - S.WallStarted > 65.)
    { Finish(false, TEXT("Native water interaction exceeded 65 seconds")); return; }
    if (!Pawn || !PC || !Pawn->FirstPersonCamera || !World->GetGameViewport()) return;
    if (S.Stage != S.LastStage)
    {
        S.LastStage = S.Stage;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_HOT_TUB_WATER_SMOKE_STAGE stage=%d seconds=%.2f pawn=%s"),
            S.Stage, Now - S.Started, *Pawn->GetActorLocation().ToCompactString());
    }
    const float Age = float(Now - S.StageStarted);
    if (S.Stage == 0)
    {
        if (Now - S.Started < 3.) return;
        FString UserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
        { Finish(false, TEXT("Requires isolated UserDir to protect the player save")); return; }
        for (TActorIterator<AHouseHotTubWater> It(World); It; ++It)
        { if (!Water) Water = *It; else { Finish(false, TEXT("Expected exactly one authored hot tub water actor")); return; } }
        if (!Water) { Finish(false, TEXT("Authored hot tub water actor missing")); return; }
        S.Water = Water; S.Pawn = Pawn;
        S.bSavedJets = Water->bJetsRunning; Water->bJetsRunning = false;
        S.Gravity = Pawn->GetCharacterMovement()->GravityScale;
        S.WalkSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
        S.bLookIgnored = PC->IsLookInputIgnored(); S.bMoveIgnored = PC->IsMoveInputIgnored(); S.bCursor = PC->bShowMouseCursor;
        S.EntriesBefore = Water->PawnEntryImpulseCount; S.MotionBefore = Water->PawnMotionImpulseCount;
        S.ExitsBefore = Water->PawnExitImpulseCount; S.SplashBefore = Water->SplashBurstCount;
        S.InitialActors = SpaActorCount(World);
        TArray<UActorComponent*> Parts; Water->GetComponents(Parts); S.InitialComponents = Parts.Num();
        if (!SpaPlaceDryApproach(World, Water, Pawn, PC))
        { Finish(false, TEXT("No dry player-sized stair approach clearance at the authored tub")); return; }
        S.LastFeet = SpaFeet(Pawn); Stage(1); return;
    }
    if (!Water) { Finish(false, TEXT("Water actor vanished during traversal")); return; }
    const FVector Feet = SpaFeet(Pawn);
    const FVector Local = Water->GetActorTransform().InverseTransformPosition(Feet);
    UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
    if ((S.Stage == 2 || S.Stage == 3 || S.Stage == 7) && Now - S.LastFrameAt > .5)
    {
        S.LastFrameAt = float(Now);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_HOT_TUB_WATER_TRAVERSAL stage=%d localFeet=%s velocity=%s mode=%d entries=%d motion=%d splashes=%d active=%d"),
            S.Stage, *Local.ToCompactString(), *Movement->Velocity.ToCompactString(), int32(Movement->MovementMode),
            Water->PawnEntryImpulseCount - S.EntriesBefore, Water->PawnMotionImpulseCount - S.MotionBefore,
            Water->SplashBurstCount - S.SplashBefore, Water->ActiveSplashParticleCount);
    }
    S.MaxHeight = FMath::Max(S.MaxHeight, Water->GetMaximumDisplacement());
    S.MaxEnergy = FMath::Max(S.MaxEnergy, Water->GetWaveEnergy());
    S.MaxActiveSplash = FMath::Max(S.MaxActiveSplash, Water->ActiveSplashParticleCount);
    if (!FMath::IsFinite(Water->GetWaveEnergy()) || !FMath::IsFinite(Water->GetMaximumDisplacement())
        || Water->GetMaximumDisplacement() > Water->MaximumWaveHeight + .02f
        || Water->ActiveSplashParticleCount > 160)
    { Finish(false, TEXT("Water simulation or bounded splash pool exceeded its limits")); return; }
    if (S.Stage == 1)
    {
        if (Age < .7f) return;
        Stage(2); return;
    }
    if (S.Stage == 2)
    {
        S.StairHeight = FMath::Max(S.StairHeight, Feet.Z);
        Pawn->AddMovementInput(Water->GetActorQuat().RotateVector(FVector(0, -1, 0)), .24f, true);
        if (Local.Y > 198.f)
        { if (Age > 4.f) Finish(false, TEXT("Normal walking could not climb the authored spa steps")); return; }
        Pawn->Jump(); S.bJumpHeld = true; Stage(3); return;
    }
    if (S.Stage == 3)
    {
        S.JumpPeak = FMath::Max(S.JumpPeak, float(Local.Z));
        if (S.bJumpHeld && Age > .13f) { Pawn->StopJumping(); S.bJumpHeld = false; }
        const FVector Goal(0, 20, Local.Z);
        const FVector Delta = Goal - Local;
        if (Delta.Size2D() > 16.f)
            Pawn->AddMovementInput(Water->GetActorQuat().RotateVector(FVector(Delta.X, Delta.Y, 0).GetSafeNormal()), .34f, true);
        if (Water->PawnEntryImpulseCount > S.EntriesBefore)
        {
            if (!S.bEntered) { S.bEntered = true; S.EntryAt = Now; }
            if (Water->ActiveSplashParticleCount > 0) ++S.EntryFrames;
            PC->SetControlRotation(FRotator(-68, Water->GetActorRotation().Yaw + 180, 0));
            if (!S.bEntryCapture && Now - S.EntryAt >= .12 && !FScreenshotRequest::IsScreenshotRequested())
            { SpaCapture(S, TEXT("entry_splash")); S.bEntryCapture = true; }
            if (Age > .7f && Movement->IsMovingOnGround() && Delta.Size2D() <= 20.f)
            { S.LastFeet = Feet; Stage(4); return; }
        }
        if (Age > 5.f) Finish(false, TEXT("Physical jump into the actual basin did not land and generate an entry splash"));
        return;
    }
    if (S.Stage == 4)
    {
        static const FVector2D Targets[] = {{-44,-18},{44,-18},{44,18},{-44,18},{0,0}};
        const FVector2D Delta = Targets[S.Target] - FVector2D(Local.X, Local.Y);
        if (Delta.Size() < 9.f) S.Target = (S.Target + 1) % UE_ARRAY_COUNT(Targets);
        const FVector2D NewDelta = Targets[S.Target] - FVector2D(Local.X, Local.Y);
        Pawn->AddMovementInput(Water->GetActorQuat().RotateVector(FVector(NewDelta.X, NewDelta.Y, 0).GetSafeNormal()), .15f, true);
        S.WadingDistance += FVector::Dist2D(Feet, S.LastFeet); S.LastFeet = Feet; ++S.WadingFrames;
        PC->SetControlRotation(FRotator(-64, Water->GetActorRotation().Yaw + 180, 0));
        if (!S.bWadeCapture && Age > 1.f && Water->ActiveSplashParticleCount > 0 && !FScreenshotRequest::IsScreenshotRequested())
        { SpaCapture(S, TEXT("moving_water_wade")); S.bWadeCapture = true; }
        if (Age < 4.3f) return;
        if (Water->PawnMotionImpulseCount - S.MotionBefore < 4 || S.WadingDistance < 110.f
            || S.MaxActiveSplash == 0 || !S.bEntryCapture || !S.bWadeCapture)
        { Finish(false, TEXT("Actual wading did not generate repeated moving water and visible splash effects")); return; }
        Stage(5); return;
    }
    if (S.Stage == 5)
    {
        // Normal braking is allowed to finish before measuring a still player.
        if (Age < .65f) return;
        S.DecayInitialEnergy = Water->GetWaveEnergy(); S.StationaryMotion = float(Water->PawnMotionImpulseCount);
        if (S.DecayInitialEnergy < .0001f)
        { Finish(false, TEXT("Wading left no wave energy to measure natural decay")); return; }
        Stage(6); return;
    }
    if (S.Stage == 6)
    {
        if (Age < 5.f) return;
        S.DecayFinalEnergy = Water->GetWaveEnergy();
        if (S.DecayFinalEnergy > S.DecayInitialEnergy * .35f || Water->ActiveSplashParticleCount != 0
            || Water->PawnMotionImpulseCount != FMath::RoundToInt(S.StationaryMotion))
        { Finish(false, TEXT("Standing still did not let the water settle and the splash effects expire")); return; }
        Water->bJetsRunning = S.bSavedJets;
        Stage(7); return;
    }
    if (S.Stage == 7)
    {
        // Walk across the existing submerged east seat and rim onto the broad
        // dry east walkway. The normal 45 cm step height provides the route.
        PC->SetControlRotation(FRotator(-35, Water->GetActorRotation().Yaw, 0));
        Pawn->AddMovementInput(Water->GetActorForwardVector(), .3f, true);
        if (Local.X < 248.f || !Movement->IsMovingOnGround())
        { if (Age > 6.f) Finish(false, TEXT("The player could not physically leave the basin onto the dry room walkway")); return; }
        S.bExited = !Water->IsPointInsideWater(Feet); Stage(8); return;
    }
    if (S.Stage == 8)
    {
        if (Age < .7f) return;
        TArray<UActorComponent*> Parts; Water->GetComponents(Parts);
        if (!S.bExited || Water->PawnExitImpulseCount <= S.ExitsBefore
            || Parts.Num() != S.InitialComponents || SpaActorCount(World) != S.InitialActors
            || Movement->GravityScale != S.Gravity || Movement->MaxWalkSpeed != S.WalkSpeed
            || PC->IsLookInputIgnored() != S.bLookIgnored || PC->IsMoveInputIgnored() != S.bMoveIgnored
            || PC->bShowMouseCursor != S.bCursor)
        { Finish(false, TEXT("Exit, movement/input settings or bounded actor/component counts changed")); return; }
        const FVector Camera = Pawn->FirstPersonCamera->GetComponentLocation();
        PC->SetControlRotation((Water->GetActorLocation() - Camera).Rotation());
        Stage(9); return;
    }
    if (S.Stage == 9)
    {
        if (Age < .35f) return;
        if (!FScreenshotRequest::IsScreenshotRequested()) SpaCapture(S, TEXT("spa_after_exit"));
        Stage(10); return;
    }
    if (S.Stage == 10 && Age > .35f)
        Finish(true, TEXT("The actual player climbed the authored stairs, jumped into the basin, produced an entry splash and repeated wading wakes, let the real grid settle without jets, and walked out over the original submerged seat and rim. Splash particles expired, movement/input stayed intact, and actor/component counts remained bounded."));
}
#endif

void TickHouseHotTubWaterSmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (!World || !FParse::Param(FCommandLine::Get(), TEXT("HouseHotTubWaterSmoke"))) return;
    static TWeakObjectPtr<UWorld> TestWorld; TestWorld = World;
    static const FDelegateHandle PostTick = FWorldDelegates::OnWorldPostActorTick.AddLambda(
        [](UWorld* TickedWorld, ELevelTick, float)
        { if (TickedWorld == TestWorld.Get()) RunHouseHotTubWaterSmoke(TickedWorld); });
    (void)PostTick;
#else
    (void)World;
#endif
}
