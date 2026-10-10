// Opt-in rendered play test: drive the real first-person character through
// every original porch pair, with normal collision and deliberate door input.
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "HouseRoamingPig.h"
#include "HouseWorldScale.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FHouseWalkPerfState
{
    TArray<FVector> Waypoints;
    TArray<float> FrameTimesMs;
    int32 WaypointIndex = 1;
    int32 ReachedWaypoints = 0;
    int32 CompletedLaps = 0;
    int32 StallEvents = 0;
    int32 WarmupFrames = 0;
    int32 Over25Ms = 0;
    int32 Over33Ms = 0;
    int32 Phase = 0;
    double WarmupStarted = 0;
    double SampleStarted = 0;
    double PreviousTick = 0;
    double LastProgress = 0;
    double LongestNoProgress = 0;
    double StartUsedPhysicalMB = 0;
    float BestWaypointDistance = BIG_NUMBER;
    float TravelDistanceCm = 0;
    FVector PreviousPosition = FVector::ZeroVector;
    bool bStallActive = false;
    TArray<FVector> PigDetour;
    int32 PigDetourIndex = 0;
    int32 PigAvoidanceEvents = 0;
};

int32 HousePerfCVarInt(const TCHAR* Name)
{
    const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
    return CVar ? CVar->GetInt() : -1;
}

float HousePerfCVarFloat(const TCHAR* Name)
{
    const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
    return CVar ? CVar->GetFloat() : -1.f;
}

bool LoadHouseWalkPerfWaypoints(UWorld* World, TArray<FVector>& Out)
{
    FString Source;
    FString CustomPath;
    if (FParse::Value(FCommandLine::Get(), TEXT("HouseWalkRoute="), CustomPath))
    {
        // Optional review route, using the same source metre coordinates as
        // the registered paths: {x,y,z} -> Unreal {100z,100x,100y} feet.
        // Keep the real walking, timing and stall checks unchanged.
        TArray<TSharedPtr<FJsonValue>> Points;
        if (CustomPath.IsEmpty() || !FFileHelper::LoadFileToString(Source, *CustomPath) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Points) ||
            Points.Num() < 3) return false;
        for (const TSharedPtr<FJsonValue>& Value : Points)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) return false;
            const TSharedPtr<FJsonObject> Point = Value->AsObject();
            double X = 0, Y = 0, Z = 0;
            if (!Point.IsValid() || !Point->TryGetNumberField(TEXT("x"), X) ||
                !Point->TryGetNumberField(TEXT("y"), Y) ||
                !Point->TryGetNumberField(TEXT("z"), Z) ||
                !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
            const FVector Position = HouseWorldScale::Point(World, FVector(Z * 100.0, X * 100.0, Y * 100.0));
            if (Position.ContainsNaN()) return false;
            Out.Add(Position);
        }
        if (FVector::Dist(Out[0], Out.Last()) >= 25.f) return false;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_WALK_PERF_ROUTE: source=%s points=%d"),
            *CustomPath, Out.Num());
        return true;
    }
    const FString Path = FPaths::ProjectContentDir() / TEXT("HouseReview/house_walkthrough.json");
    TSharedPtr<FJsonObject> Root;
    const TArray<TSharedPtr<FJsonValue>>* Routes = nullptr;
    if (!FFileHelper::LoadFileToString(Source, *Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) ||
        !Root.IsValid() || !Root->TryGetArrayField(TEXT("routes"), Routes)) return false;

    // These registered forward/return paths cross the upper gallery without
    // teleports or door interaction. Use source coordinates and route order.
    const TCHAR* Names[] = { TEXT("Gallery_to_FarUpperHall"), TEXT("Gallery_to_FarUpperHall_Return") };
    for (int32 RouteNumber = 0; RouteNumber < UE_ARRAY_COUNT(Names); ++RouteNumber)
    {
        const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
        for (const TSharedPtr<FJsonValue>& Value : *Routes)
        {
            const TSharedPtr<FJsonObject> Route = Value->AsObject();
            FString Name;
            if (Route.IsValid() && Route->TryGetStringField(TEXT("id"), Name) &&
                Name == Names[RouteNumber] && Route->TryGetArrayField(TEXT("waypoints"), Points)) break;
        }
        if (!Points || Points->Num() < 2) return false;
        for (int32 Index = RouteNumber == 0 ? 0 : 1; Index < Points->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Point = (*Points)[Index]->AsObject();
            double X = 0, Y = 0, Z = 0;
            if (!Point.IsValid() || !Point->TryGetNumberField(TEXT("x"), X) ||
                !Point->TryGetNumberField(TEXT("y"), Y) ||
                !Point->TryGetNumberField(TEXT("z"), Z)) return false;
            const FVector Position = HouseWorldScale::Point(World, FVector(Z * 100.0, X * 100.0, Y * 100.0));
            if (Position.ContainsNaN()) return false;
            Out.Add(Position);
        }
        Points = nullptr;
    }
    return Out.Num() >= 3 && FVector::Dist2D(Out[0], Out.Last()) < 25.f;
}

float HousePerfPercentile(const TArray<float>& Sorted, float Fraction)
{
    return Sorted.IsEmpty() ? 0.f : Sorted[FMath::Clamp(
        FMath::CeilToInt(Fraction * Sorted.Num()) - 1, 0, Sorted.Num() - 1)];
}

void TickHouseWalkPerf(UWorld* World)
{
    static FHouseWalkPerfState State;
    if (!World || State.Phase < 0 ||
        UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* Controller = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = Controller ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
    if (!Pawn || !Pawn->FirstPersonCamera || !World->GetGameViewport()) return;

    auto Fail = [&](const FString& Why)
    {
        UE_LOG(LogTemp, Error, TEXT("HOUSE_WALK_PERF_FAILED: %s"), *Why);
        State.Phase = -1;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    const double Now = FPlatformTime::Seconds();
    if (State.Phase == 0)
    {
        FString IsolatedUserDir;
        if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedUserDir) ||
            IsolatedUserDir.IsEmpty())
        { Fail(TEXT("Benchmark requires an isolated -UserDir")); return; }
        if (!LoadHouseWalkPerfWaypoints(World, State.Waypoints))
        { Fail(TEXT("Walking waypoints are unavailable or invalid (custom routes require at least three finite points and a closed loop)")); return; }
        UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        if (!Settings) { Fail(TEXT("Game user settings are unavailable")); return; }
        Settings->SetOverallScalabilityLevel(1);
        Settings->SetResolutionScaleValueEx(80.f);
        Settings->SetFrameRateLimit(60.f);
        // The launch arguments already set the native window size. Applying
        // resolution here can block macOS while its window mode changes.
        Settings->ApplyNonResolutionSettings();
        // The game's normal quarter-second GameMode tick is intentional. This
        // opt-in benchmark needs one movement and timing sample per frame.
        if (AGameModeBase* GameMode = World->GetAuthGameMode())
            GameMode->SetActorTickInterval(0.f);
        UStoryMovementComponent* Movement = Pawn->GetStoryMovement();
        if (!Movement) { Fail(TEXT("Story movement component is unavailable")); return; }
        Movement->StopMovementImmediately();
        Movement->SetSprintHeld(false);
        Movement->SetSlideHeld(false);
        const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(State.Waypoints[0] + FVector(0, 0, HalfHeight),
            false, nullptr, ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);
        Controller->SetControlRotation((State.Waypoints[1] - State.Waypoints[0]).GetSafeNormal2D().Rotation());
        State.PreviousPosition = Pawn->GetActorLocation();
        State.WarmupStarted = Now;
        State.Phase = 1;
        int32 Width = 0, Height = 0;
        Controller->GetViewportSize(Width, Height);
        if (Width <= 0 || Height <= 0)
        { Fail(TEXT("Benchmark requires a rendered native viewport")); return; }
        float ScaleNormalized = 0, ScaleValue = 0, MinScale = 0, MaxScale = 0;
        Settings->GetResolutionScaleInformationEx(ScaleNormalized, ScaleValue, MinScale, MaxScale);
        UE_LOG(LogTemp, Display, TEXT("HOUSE_WALK_PERF_READY: viewport=%dx%d quality=%d resolutionScale=%.1f frameLimit=%.1f vsync=%d shadowQuality=%d vsm=%d vsmCoarseNonNanite=%d vsmPages=%d gi=%d lumenHw=%d lumenMeshSdf=%d skylight=%.2f memoryUsedPhysicalMB=%.1f points=%d"),
            Width, Height, Settings->GetOverallScalabilityLevel(), ScaleValue,
            Settings->GetFrameRateLimit(), HousePerfCVarInt(TEXT("r.VSync")),
            HousePerfCVarInt(TEXT("sg.ShadowQuality")), HousePerfCVarInt(TEXT("r.Shadow.Virtual.Enable")),
            HousePerfCVarInt(TEXT("r.Shadow.Virtual.NonNanite.IncludeInCoarsePages")),
            HousePerfCVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")),
            HousePerfCVarInt(TEXT("r.DynamicGlobalIlluminationMethod")),
            HousePerfCVarInt(TEXT("r.Lumen.HardwareRayTracing")),
            HousePerfCVarInt(TEXT("r.Lumen.TraceMeshSDFs")),
            HousePerfCVarFloat(TEXT("r.SkylightIntensityMultiplier")),
            double(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0), State.Waypoints.Num());
        return;
    }
    if (State.Phase == 1)
    {
        ++State.WarmupFrames;
        if (Now - State.WarmupStarted < 3.0 || State.WarmupFrames < 64) return;
        State.SampleStarted = Now;
        State.PreviousTick = Now;
        State.LastProgress = Now;
        State.StartUsedPhysicalMB = double(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
        State.Phase = 2;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_WALK_PERF_START: warmupSeconds=%.2f warmupFrames=%d memoryUsedPhysicalMB=%.1f"),
            Now - State.WarmupStarted, State.WarmupFrames, State.StartUsedPhysicalMB);
        return;
    }

    const float FrameMs = float((Now - State.PreviousTick) * 1000.0);
    State.PreviousTick = Now;
    State.FrameTimesMs.Add(FrameMs);
    if (FrameMs > 25.f) ++State.Over25Ms;
    if (FrameMs > 33.333f) ++State.Over33Ms;
    State.TravelDistanceCm += FVector::Dist2D(Pawn->GetActorLocation(), State.PreviousPosition);
    State.PreviousPosition = Pawn->GetActorLocation();
    if (Now - State.SampleStarted >= 15.0)
    {
        State.FrameTimesMs.Sort();
        const double EndUsedPhysicalMB = double(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
        const bool bPassed = State.FrameTimesMs.Num() >= 100 && State.TravelDistanceCm >= 2000.f &&
            State.StallEvents == 0;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_WALK_PERF_RESULT: passed=%d seconds=%.2f frames=%d p50FrameMs=%.2f p95FrameMs=%.2f p99FrameMs=%.2f worstFrameMs=%.2f over25Ms=%d over33Ms=%d travelCm=%.1f waypoints=%d laps=%d stalls=%d longestNoProgressSeconds=%.2f memoryStartMB=%.1f memoryEndMB=%.1f memoryDeltaMB=%+.1f vsm=%d vsmCoarseNonNanite=%d vsmPages=%d lumenHw=%d lumenMeshSdf=%d skylight=%.2f shadowQuality=%d resolutionQuality=%.1f pigAvoidances=%d"),
            bPassed, Now - State.SampleStarted, State.FrameTimesMs.Num(),
            HousePerfPercentile(State.FrameTimesMs, .50f), HousePerfPercentile(State.FrameTimesMs, .95f),
            HousePerfPercentile(State.FrameTimesMs, .99f), State.FrameTimesMs.Last(),
            State.Over25Ms, State.Over33Ms, State.TravelDistanceCm,
            State.ReachedWaypoints, State.CompletedLaps, State.StallEvents,
            State.LongestNoProgress, State.StartUsedPhysicalMB, EndUsedPhysicalMB,
            EndUsedPhysicalMB - State.StartUsedPhysicalMB,
            HousePerfCVarInt(TEXT("r.Shadow.Virtual.Enable")),
            HousePerfCVarInt(TEXT("r.Shadow.Virtual.NonNanite.IncludeInCoarsePages")),
            HousePerfCVarInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages")),
            HousePerfCVarInt(TEXT("r.Lumen.HardwareRayTracing")),
            HousePerfCVarInt(TEXT("r.Lumen.TraceMeshSDFs")),
            HousePerfCVarFloat(TEXT("r.SkylightIntensityMultiplier")),
            HousePerfCVarInt(TEXT("sg.ShadowQuality")), HousePerfCVarFloat(TEXT("sg.ResolutionQuality")),State.PigAvoidanceEvents);
        State.Phase = -1;
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
        return;
    }

    const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0,
        Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float HalfHeight=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams WalkQuery(SCENE_QUERY_STAT(HouseWalkPerfPigAvoidance),false,Pawn);
    const FCollisionShape Capsule=FCollisionShape::MakeCapsule(Radius,HalfHeight);
    if (State.PigDetour.IsValidIndex(State.PigDetourIndex)
        && FVector::Dist2D(Feet,State.PigDetour[State.PigDetourIndex])<18.f
        && FMath::Abs(Feet.Z-State.PigDetour[State.PigDetourIndex].Z)<35.f
        && Pawn->GetCharacterMovement()->IsMovingOnGround())
    {
        ++State.PigDetourIndex;
        State.BestWaypointDistance=BIG_NUMBER; State.LastProgress=Now; State.bStallActive=false;
        if (!State.PigDetour.IsValidIndex(State.PigDetourIndex))
        { State.PigDetour.Reset(); State.PigDetourIndex=0; }
    }
    FVector MovementTarget=State.PigDetour.IsValidIndex(State.PigDetourIndex)?
        State.PigDetour[State.PigDetourIndex]:State.Waypoints[State.WaypointIndex];
    // Live pets may share the fixed review route. Plan an actual walking
    // detour, checked against every collision (including the pet), rather
    // than treating its upright movement capsule as a broken map doorway.
    if (State.PigDetour.IsEmpty())
    {
        const FVector Forward=(MovementTarget-Feet).GetSafeNormal2D(); FHitResult PetHit;
        if (World->SweepSingleByChannel(PetHit,Pawn->GetActorLocation(),Pawn->GetActorLocation()+Forward*110.f,
            FQuat::Identity,ECC_Pawn,Capsule,WalkQuery))
            if (AHouseRoamingPig* Pig=Cast<AHouseRoamingPig>(PetHit.GetActor()))
            {
                FCollisionQueryParams FloorQuery=WalkQuery;
                for (TActorIterator<APawn> It(World);It;++It) FloorQuery.AddIgnoredActor(*It);
                auto SegmentClear=[&](const FVector& A,const FVector& B)
                {
                    FHitResult Hit;
                    if (World->SweepSingleByChannel(Hit,A+FVector(0,0,HalfHeight),B+FVector(0,0,HalfHeight),
                        FQuat::Identity,ECC_Pawn,Capsule,WalkQuery)) return false;
                    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(A,B)/25.f));
                    for (int32 i=0;i<=Steps;++i)
                    {
                        const FVector Sample=FMath::Lerp(A,B,float(i)/Steps); FHitResult Floor;
                        if (!World->LineTraceSingleByChannel(Floor,Sample+FVector(0,0,35),Sample-FVector(0,0,55),ECC_Visibility,FloorQuery)
                            ||Floor.ImpactNormal.Z<.65f||Floor.ImpactPoint.Z<Sample.Z-40.f) return false;
                    }
                    return true;
                };
                const float Clearance=Radius+Pig->GetCapsuleComponent()->GetScaledCapsuleRadius()+20.f;
                const FVector Right=FVector::CrossProduct(FVector::UpVector,Forward);
                TArray<FVector> Best; float BestLength=BIG_NUMBER;
                for (const float Sign:{1.f,-1.f})
                {
                    const FVector Side=Right*Sign;
                    const FVector A=Feet+Side*(Clearance+10.f);
                    FVector B=Pig->FeetLocation()+Forward*(Clearance+25.f)+Side*(Clearance+10.f); B.Z=Feet.Z;
                    const FVector C=State.Waypoints[State.WaypointIndex];
                    // The original waypoint itself can currently be occupied
                    // by the pet. Walk the clear lateral legs first, then
                    // resume that exact waypoint with normal collision intact.
                    if (!SegmentClear(Feet,A)||!SegmentClear(A,B)) continue;
                    const float Length=FVector::Dist2D(Feet,A)+FVector::Dist2D(A,B)+FVector::Dist2D(B,C);
                    if (Length<BestLength) { BestLength=Length; Best={A,B}; }
                }
                if (!Best.IsEmpty())
                {
                    State.PigDetour=MoveTemp(Best); State.PigDetourIndex=0; ++State.PigAvoidanceEvents;
                    MovementTarget=State.PigDetour[0]; State.BestWaypointDistance=BIG_NUMBER;
                    State.LastProgress=Now; State.bStallActive=false;
                    UE_LOG(LogTemp,Display,TEXT("HOUSE_WALK_PERF_PIG_AVOIDANCE: pig=%s waypoint=%d detour=[%s;%s] resume=%s collisionsRetained=1"),
                        *Pig->PigName,State.WaypointIndex,*State.PigDetour[0].ToCompactString(),
                        *State.PigDetour[1].ToCompactString(),*State.Waypoints[State.WaypointIndex].ToCompactString());
                }
            }
    }
    const FVector Delta = State.Waypoints[State.WaypointIndex] - Feet;
    const float Horizontal = Delta.Size2D();
    const float Distance = (MovementTarget-Feet).Size();
    if (Horizontal < 20.f && FMath::Abs(Delta.Z) < 35.f &&
        Pawn->GetCharacterMovement()->IsMovingOnGround())
    {
        ++State.ReachedWaypoints;
        ++State.WaypointIndex;
        if (State.WaypointIndex >= State.Waypoints.Num())
        { State.WaypointIndex = 1; ++State.CompletedLaps; }
        State.BestWaypointDistance = BIG_NUMBER;
        State.LastProgress = Now;
        State.bStallActive = false;
        State.PigDetour.Reset(); State.PigDetourIndex=0;
        MovementTarget=State.Waypoints[State.WaypointIndex];
    }
    else if (Distance < State.BestWaypointDistance - 2.f)
    {
        State.BestWaypointDistance = Distance;
        State.LastProgress = Now;
        State.bStallActive = false;
    }
    else
    {
        const double NoProgress = Now - State.LastProgress;
        State.LongestNoProgress = FMath::Max(State.LongestNoProgress, NoProgress);
        if (NoProgress > 1.0 && !State.bStallActive)
        { ++State.StallEvents; State.bStallActive = true; }
        if (NoProgress > 4.0)
        {
            FHitResult Obstacle;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseWalkPerfObstacle), false);
            Query.AddIgnoredActor(Pawn);
            const FVector Ahead = Pawn->GetActorLocation() +
                (State.Waypoints[State.WaypointIndex] - Feet).GetSafeNormal2D() * 100.f;
            const bool bHit = World->SweepSingleByChannel(Obstacle, Pawn->GetActorLocation(),
                Ahead, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                    Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Query);
            UE_LOG(LogTemp, Error, TEXT("HOUSE_WALK_PERF_OBSTACLE: hit=%d actor=%s component=%s point=%s velocity=%s grounded=%d"),
                bHit, *GetNameSafe(Obstacle.GetActor()), *GetNameSafe(Obstacle.GetComponent()),
                *Obstacle.ImpactPoint.ToCompactString(), *Pawn->GetVelocity().ToCompactString(),
                Pawn->GetCharacterMovement()->IsMovingOnGround());
            Fail(FString::Printf(TEXT("Route stalled at waypoint %d, distance %.1f cm, position %s"),
                State.WaypointIndex, Distance, *Pawn->GetActorLocation().ToCompactString()));
            return;
        }
    }
    const FVector ToTarget = MovementTarget - Feet;
    FCollisionQueryParams DoorQuery(SCENE_QUERY_STAT(HouseWalkPerfDoor), false);
    DoorQuery.AddIgnoredActor(Pawn);
    FHitResult DoorHit;
    if (World->SweepSingleByChannel(DoorHit, Pawn->GetActorLocation(),
            Pawn->GetActorLocation() + ToTarget.GetSafeNormal2D() * 110.f,
            FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), DoorQuery))
    {
        if (AExistingHouseDoor* Door = Cast<AExistingHouseDoor>(DoorHit.GetActor()); Door && !Door->IsOpen())
        {
            if (Door->bLocked)
            {
                Fail(FString::Printf(TEXT("Walking route meets locked door %s"), *Door->DoorId.ToString()));
                return;
            }
            Controller->SetControlRotation(
                (Door->DoorwayCenter - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
            Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
            UE_LOG(LogTemp, Display, TEXT("HOUSE_WALK_PERF_OPENED_DOOR: %s"), *Door->DoorId.ToString());
            State.LastProgress = Now;
            return;
        }
    }
    Controller->SetControlRotation(ToTarget.GetSafeNormal2D().Rotation());
    Pawn->AddMovementInput(ToTarget.GetSafeNormal2D(),
        FMath::Clamp(ToTarget.Size2D() / 80.f, .08f, 1.f), true);
}
}

void TickHouseTraversalSmoke(UWorld* World)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("HouseWalkPerf")))
    { TickHouseWalkPerf(World); return; }
    if (!FParse::Param(FCommandLine::Get(), TEXT("HouseTraversalSmoke"))) return;
    static const TCHAR* Groups[] = {
        TEXT("Porch_p0_m1_1"), TEXT("Porch_p0_m1_2"),
        TEXT("Porch_p0_p0_1"), TEXT("Porch_p0_p0_2")
    };
    static int32 Route = 0;
    static int32 Stage = 0;
    static double Started = 0;
    static double RouteStarted = 0;
    static double ActionStarted = 0;
    static TArray<float> FrameTimesMs;
    static TWeakObjectPtr<AExistingHouseDoor> Door;
    if (!World || Stage < 0 || UGameplayStatics::GetCurrentLevelName(World, true) != TEXT("Chapter01_House")) return;
    APlayerController* Controller = World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn = Controller ? Cast<AStoryFirstPersonCharacter>(Controller->GetPawn()) : nullptr;
    if (!Pawn || !World->GetGameViewport()) return;
    auto Fail = [](const FString& Why)
    {
        UE_LOG(LogTemp, Error, TEXT("HOUSE_TRAVERSAL_FAILED: %s"), *Why);
        Stage = -1;
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };
    const double Now = FPlatformTime::Seconds();
    if (!Started) Started = Now;
    if (Now - Started > 60) { Fail(TEXT("Timed out")); return; }
    if (Stage == 0)
    {
        static bool bSettingsApplied = false;
        if (!bSettingsApplied)
        {
            if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
            {
                Settings->SetOverallScalabilityLevel(1);
                Settings->SetResolutionScaleValueEx(80.f);
                Settings->SetFrameRateLimit(60.f);
                Settings->ApplyNonResolutionSettings();
            }
            bSettingsApplied = true;
        }
        // Let the real viewport and first visit to the map warm up before
        // sampling frame intervals. This does not turn character collision off.
        if (Now - Started < 3.0) return;
        Stage = 1;
    }
    if (Stage == 1)
    {
        if (Route == UE_ARRAY_COUNT(Groups))
        {
            UE_LOG(LogTemp, Display, TEXT("HOUSE_TRAVERSAL_PASSED: four porch entrances crossed with real player movement and collision"));
            Stage = -1;
            FPlatformMisc::RequestExitWithStatus(false, 0);
            return;
        }
        Door.Reset();
        for (TActorIterator<AExistingHouseDoor> It(World); It; ++It)
            if (It->DoorGroup == FName(Groups[Route])) { Door = *It; break; }
        if (!Door.IsValid()) { Fail(FString::Printf(TEXT("Missing %s"), Groups[Route])); return; }
        const FVector Center = Door->DoorwayCenter;
        // Start in the unobstructed approach to the threshold; farther back
        // this room has sofas that require a turn around them.
        float StartOffset = 300.f;
        FParse::Value(FCommandLine::Get(), TEXT("HouseTraversalStartOffset="), StartOffset);
        if (!FMath::IsFinite(StartOffset) || StartOffset < 150.f || StartOffset > 1000.f)
        { Fail(TEXT("HouseTraversalStartOffset must be a finite distance from 150 to 1000 cm")); return; }
        // DoorwayCenter's floor-to-center distance grows with the house;
        // the character's capsule does not. Place its feet at the live hinge.
        const FVector Start(Center.X, Center.Y - StartOffset,
            Door->GetActorLocation().Z + Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        World->GetAuthGameMode()->SetActorTickInterval(0.f);
        Pawn->SetActorLocation(Start, false, nullptr, ETeleportType::TeleportPhysics);
        Controller->SetControlRotation(FRotator(0.f, 90.f, 0.f));
        if (UStoryMovementComponent* Movement = Pawn->GetStoryMovement())
            Movement->SetSprintHeld((Route % 2) == 1);
        FrameTimesMs.Reset();
        RouteStarted = Now;
        Stage = 2;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_TRAVERSAL_START: %s pace=%s start=%s target=%s"),
            Groups[Route], (Route % 2) ? TEXT("sprint") : TEXT("walk"),
            *Start.ToCompactString(), *Center.ToCompactString());
        return;
    }
    if (Stage == 2)
    {
        if (!Door.IsValid()) { Fail(TEXT("Door disappeared")); return; }
        const FVector Center = Door->DoorwayCenter;
        const FVector Position = Pawn->GetActorLocation();
        if (Now - RouteStarted > .25)
            FrameTimesMs.Add(World->GetDeltaSeconds() * 1000.f);
        if (Position.Y >= Center.Y - 145.f)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Controller->SetControlRotation((Center - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            ActionStarted = Now;
            Stage = 3;
            return;
        }
        if (Now - RouteStarted > 5.0)
        { Fail(FString::Printf(TEXT("Could not approach %s"), Groups[Route])); return; }
        Pawn->AddMovementInput(FVector(0.f, 1.f, 0.f), 1.f, true);
        return;
    }
    if (Stage == 3)
    {
        if (Now - ActionStarted < .25) return;
        if (Door->IsOpen())
        { Fail(FString::Printf(TEXT("%s opened when approached without input"), Groups[Route])); return; }
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        ActionStarted = Now;
        Stage = 31;
        return;
    }
    if (Stage == 31)
    {
        if (Now - ActionStarted < .1) return;
        if (!Door->IsOpen())
        { Fail(FString::Printf(TEXT("E did not open %s"), Groups[Route])); return; }
        Stage = 4;
        return;
    }
    if (Stage == 4)
    {
        if (!Door.IsValid()) { Fail(TEXT("Door disappeared")); return; }
        const FVector Center = Door->DoorwayCenter;
        const FVector Position = Pawn->GetActorLocation();
        FrameTimesMs.Add(World->GetDeltaSeconds() * 1000.f);
        if (Position.Y >= Center.Y + 120.f)
        {
            if (!Door->IsOpen()) { Fail(FString::Printf(TEXT("Crossed closed %s"), Groups[Route])); return; }
            FrameTimesMs.Sort();
            const float P95 = FrameTimesMs.IsEmpty() ? 0.f :
                FrameTimesMs[FMath::Clamp(FMath::FloorToInt(FrameTimesMs.Num() * .95f), 0, FrameTimesMs.Num() - 1)];
            const float Worst = FrameTimesMs.IsEmpty() ? 0.f : FrameTimesMs.Last();
            UE_LOG(LogTemp, Display, TEXT("HOUSE_TRAVERSAL_ROUTE_PASSED: %s seconds=%.2f frames=%d p95FrameMs=%.1f worstFrameMs=%.1f end=%s"),
                Groups[Route], Now - RouteStarted, FrameTimesMs.Num(), P95, Worst, *Position.ToCompactString());
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Controller->SetControlRotation((Center - Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            ActionStarted = Now;
            Stage = 5;
            return;
        }
        if (Now - RouteStarted > 5.0)
        {
            FHitResult Obstacle;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseTraversalObstacle), false);
            Query.AddIgnoredActor(Pawn);
            const FVector Forward = Position + FVector(0, 100, 0);
            World->SweepSingleByChannel(Obstacle, Position, Forward, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                    Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Query);
            UE_LOG(LogTemp, Error, TEXT("HOUSE_TRAVERSAL_OBSTACLE: actor=%s component=%s doorOpen=%d mode=%d"),
                *GetNameSafe(Obstacle.GetActor()), *GetNameSafe(Obstacle.GetComponent()),
                Door->IsOpen(), int32(Pawn->GetCharacterMovement()->MovementMode));
            UE_LOG(LogTemp, Error, TEXT("HOUSE_TRAVERSAL_INPUT: velocity=%s pending=%s ignored=%d"),
                *Pawn->GetVelocity().ToCompactString(),
                *Pawn->GetPendingMovementInputVector().ToCompactString(),
                Controller->IsMoveInputIgnored());
            Fail(FString::Printf(TEXT("Player could not traverse %s; stopped at %s"),
                Groups[Route], *Position.ToCompactString()));
            return;
        }
        Pawn->AddMovementInput(FVector(0.f, 1.f, 0.f), 1.f, true);
    }
    if (Stage == 5)
    {
        if (Now - ActionStarted < .1) return;
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        ActionStarted = Now;
        Stage = 51;
        return;
    }
    if (Stage == 51)
    {
        if (Now - ActionStarted < .1) return;
        if (Door->IsOpen())
        { Fail(FString::Printf(TEXT("Could not close %s from the outside"), Groups[Route])); return; }
        ActionStarted = Now;
        Stage = 6;
        return;
    }
    if (Stage == 6)
    {
        if (Now - ActionStarted < .65) return;
        if (Door->IsOpen())
        { Fail(FString::Printf(TEXT("%s reopened without input"), Groups[Route])); return; }
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Pressed, 1.0));
        Controller->InputKey(FInputKeyParams(EKeys::E, IE_Released, 0.0));
        ActionStarted = Now;
        Stage = 61;
        return;
    }
    if (Stage == 61)
    {
        if (Now - ActionStarted < .1) return;
        if (!Door->IsOpen())
        { Fail(FString::Printf(TEXT("Could not reopen %s from the outside"), Groups[Route])); return; }
        ++Route;
        Stage = 1;
    }
}
#endif
