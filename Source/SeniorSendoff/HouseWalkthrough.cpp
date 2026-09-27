#include "HouseWalkthrough.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SpotLightComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHouseReview, Log, All);

namespace HouseReview
{
    // Canonical Unity metres -> Unreal centimetres. Importer uses this same basis.
    FVector ReadPosition(const TSharedPtr<FJsonObject>& Value)
    {
        return FVector(Value->GetNumberField(TEXT("z")), Value->GetNumberField(TEXT("x")),
            Value->GetNumberField(TEXT("y"))) * 100.0;
    }

    TSharedPtr<FJsonValue> VectorJson(const FVector& Value)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("x"), Value.X);
        Object->SetNumberField(TEXT("y"), Value.Y);
        Object->SetNumberField(TEXT("z"), Value.Z);
        return MakeShared<FJsonValueObject>(Object);
    }
}

AHouseWalkthroughCharacter::AHouseWalkthroughCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(32.0f, 90.0f);
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    GetCharacterMovement()->MaxWalkSpeed = 270.0f;
    // The floor elevations and photographed stair risers now stay at source
    // height while only the house plan expands. Keep normal human step reach.
    GetCharacterMovement()->MaxStepHeight = 35.0f;
    GetCharacterMovement()->SetWalkableFloorAngle(50.0f);
    GetCharacterMovement()->JumpZVelocity = 330.0f;
    GetCharacterMovement()->AirControl = 0.15f;
    GetCharacterMovement()->BrakingDecelerationWalking = 1600.0f;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    bUseControllerRotationYaw = true;
    ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    ViewCamera->SetupAttachment(GetCapsuleComponent());
    ViewCamera->SetRelativeLocation(FVector(0, 0, 73));
    ViewCamera->bUsePawnControlRotation = true;
    ViewCamera->FieldOfView = 98.0f; // Approximately Unity's 65-degree vertical view at 16:9.
    ViewCamera->bOverrideAspectRatioAxisConstraint = true;
    ViewCamera->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
    Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("ReviewFlashlight"));
    Flashlight->SetupAttachment(ViewCamera);
    Flashlight->SetRelativeLocation(FVector(12, 10, -8));
    Flashlight->SetIntensityUnits(ELightUnits::Lumens);
    Flashlight->SetIntensity(450.0f);
    Flashlight->SetAttenuationRadius(1200.0f);
    Flashlight->SetInnerConeAngle(14.0f);
    Flashlight->SetOuterConeAngle(28.0f);
    Flashlight->SetLightColor(FLinearColor(1.0f, 0.93f, 0.80f));
    Flashlight->SetCastShadows(true);
    Flashlight->SetVisibility(false);
}

void AHouseWalkthroughCharacter::BeginPlay()
{
    Super::BeginPlay();
    ReviewStartTime = FPlatformTime::Seconds();
    ReviewStartedUtc = FDateTime::UtcNow().ToIso8601();
    const TCHAR* Command = FCommandLine::Get();
    bSmoke = FParse::Param(Command, TEXT("sso-house-smoke"));
    bCapture = FParse::Param(Command, TEXT("sso-house-capture"));
    bAllViews = FParse::Param(Command, TEXT("sso-house-all-views"));
    bExitAfterReview = bSmoke || bCapture;
    OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("HouseReview"));
    FParse::Value(Command, TEXT("sso-house-output="), OutputDirectory);
    LayoutPath = FPaths::ProjectContentDir() / TEXT("HouseReview/house_walkthrough.json");
    FParse::Value(Command, TEXT("sso-house-layout="), LayoutPath);
    if (!LoadReviewLayout())
    {
        Status = TEXT("House layout could not be loaded. See Saved/Logs.");
        if (IsAutomatedReview()) { AddError(Status); FinishReview(); }
        return;
    }
    JumpToLanding(1);
    int32 RequestedStart = 0;
    int32 RequestedCount = Routes.Num();
    FParse::Value(Command, TEXT("sso-house-route-start="), RequestedStart);
    FParse::Value(Command, TEXT("sso-house-route-count="), RequestedCount);
    RouteStartIndex = FMath::Clamp(RequestedStart, 0, Routes.Num());
    RouteEndIndex = RouteStartIndex + FMath::Clamp(RequestedCount, 0, Routes.Num() - RouteStartIndex);
    if (IsAutomatedReview()) StartReview();
}

void AHouseWalkthroughCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxisKey(EKeys::MouseX, this, &AHouseWalkthroughCharacter::LookHorizontal);
    Input->BindAxisKey(EKeys::MouseY, this, &AHouseWalkthroughCharacter::LookVertical);
    Input->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AHouseWalkthroughCharacter::StartUserJump);
    Input->BindKey(EKeys::SpaceBar, IE_Released, this, &AHouseWalkthroughCharacter::StopUserJump);
    Input->BindKey(EKeys::F, IE_Pressed, this, &AHouseWalkthroughCharacter::ToggleFlashlight);
}

void AHouseWalkthroughCharacter::LookHorizontal(float Value)
{
    if (!IsAutomatedReview()) AddControllerYawInput(Value * 0.10f);
}

void AHouseWalkthroughCharacter::LookVertical(float Value)
{
    if (!IsAutomatedReview()) AddControllerPitchInput(-Value * 0.10f);
}

void AHouseWalkthroughCharacter::ToggleFlashlight()
{
    if (!IsAutomatedReview()) Flashlight->ToggleVisibility();
}

void AHouseWalkthroughCharacter::StartUserJump()
{
    if (!IsAutomatedReview()) Jump();
}

void AHouseWalkthroughCharacter::StopUserJump()
{
    if (!IsAutomatedReview()) StopJumping();
}

bool AHouseWalkthroughCharacter::IsFlashlightOn() const
{
    return Flashlight->IsVisible();
}

void AHouseWalkthroughCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bReviewFinished) return;
    if (IsAutomatedReview()) { UpdateReview(DeltaSeconds); return; }
    const APlayerController* Player = Cast<APlayerController>(Controller);
    if (!Player || Player->IsPaused()) return;
    GetCharacterMovement()->MaxWalkSpeed = Player->IsInputKeyDown(EKeys::LeftShift) ? 450.0f : 270.0f;
    const FRotator YawOnly(0, Player->GetControlRotation().Yaw, 0);
    const float Forward = float(Player->IsInputKeyDown(EKeys::W)) - float(Player->IsInputKeyDown(EKeys::S));
    const float Right = float(Player->IsInputKeyDown(EKeys::D)) - float(Player->IsInputKeyDown(EKeys::A));
    AddMovementInput(YawOnly.Vector(), Forward);
    AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Right);
    if (GetFeet().Z < -1200.0f) JumpToLanding(1);
}

bool AHouseWalkthroughCharacter::LoadReviewLayout()
{
    FString Json;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Json, *LayoutPath) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
    {
        UE_LOG(LogHouseReview, Error, TEXT("Cannot read house layout: %s"), *LayoutPath);
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Root->TryGetArrayField(TEXT("landings"), Items)) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Items)
    {
        const TSharedPtr<FJsonObject> Item = Value->AsObject();
        if (!Item.IsValid() || !Item->HasTypedField<EJson::Object>(TEXT("feet")) ||
            !Item->HasTypedField<EJson::Object>(TEXT("walkDirection"))) return false;
        FHouseReviewLanding Landing;
        Landing.Name = Item->GetStringField(TEXT("name"));
        Landing.Feet = HouseReview::ReadPosition(Item->GetObjectField(TEXT("feet")));
        Landing.Direction = HouseReview::ReadPosition(Item->GetObjectField(TEXT("walkDirection"))).GetSafeNormal();
        Landings.Add(MoveTemp(Landing));
    }
    if (Root->TryGetArrayField(TEXT("routes"), Items))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Items)
        {
            const TSharedPtr<FJsonObject> Item = Value->AsObject();
            if (!Item.IsValid()) return false;
            FHouseReviewRoute Route;
            Route.Name = Item->GetStringField(TEXT("id"));
            const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
            if (!Item->TryGetArrayField(TEXT("waypoints"), Points)) return false;
            for (const TSharedPtr<FJsonValue>& Point : *Points)
            {
                if (!Point->AsObject().IsValid()) return false;
                Route.Feet.Add(HouseReview::ReadPosition(Point->AsObject()));
            }
            Routes.Add(MoveTemp(Route));
        }
    }
    // Twelve source-aligned views cover all floors and the source-supported room repairs.
    const TArray<FString> CapturePrefixes = { TEXT("01_"), TEXT("02_"), TEXT("05_"), TEXT("06_"),
        TEXT("10_"), TEXT("20_"), TEXT("24_"), TEXT("40_"), TEXT("42_"), TEXT("45_"), TEXT("74_"), TEXT("76_") };
    ExpectedCaptureCount = bCapture && !bAllViews ? CapturePrefixes.Num() : 0;
    if (Root->TryGetArrayField(TEXT("views"), Items))
    {
        TotalAvailableViews = Items->Num();
        if (bCapture && bAllViews) ExpectedCaptureCount = TotalAvailableViews;
        TSet<FString> SelectedViewNames;
        for (const TSharedPtr<FJsonValue>& Value : *Items)
        {
            const TSharedPtr<FJsonObject> Item = Value->AsObject();
            if (!Item.IsValid()) return false;
            const FString Name = Item->GetStringField(TEXT("name"));
            if (!bAllViews && !CapturePrefixes.ContainsByPredicate([&Name](const FString& Prefix) { return Name.StartsWith(Prefix); })) continue;
            if (Name.IsEmpty() || SelectedViewNames.Contains(Name) || FPaths::GetCleanFilename(Name) != Name ||
                !Item->HasTypedField<EJson::Object>(TEXT("position")) || !Item->HasTypedField<EJson::Object>(TEXT("target"))) return false;
            SelectedViewNames.Add(Name);
            FHouseReviewView View;
            View.Name = Name;
            View.Position = HouseReview::ReadPosition(Item->GetObjectField(TEXT("position")));
            View.Target = HouseReview::ReadPosition(Item->GetObjectField(TEXT("target")));
            double VerticalFOV = 0;
            if (!Item->TryGetNumberField(TEXT("fieldOfView"), VerticalFOV) &&
                !Item->TryGetNumberField(TEXT("fov"), VerticalFOV)) return false;
            if (!FMath::IsFinite(VerticalFOV) || VerticalFOV <= 1 || VerticalFOV >= 179) return false;
            View.VerticalFieldOfView = float(VerticalFOV);
            Views.Add(MoveTemp(View));
        }
    }
    if (bCapture && (Views.IsEmpty() || Views.Num() != ExpectedCaptureCount))
    {
        UE_LOG(LogHouseReview, Error, TEXT("Requested view set is incomplete: loaded %d, expected %d"),
            Views.Num(), ExpectedCaptureCount);
        return false;
    }
    UE_LOG(LogHouseReview, Display, TEXT("House review loaded %d landings, %d routes, %d views; canonical metres mapped to Unreal cm"),
        Landings.Num(), Routes.Num(), Views.Num());
    return Landings.Num() > 0;
}

FVector AHouseWalkthroughCharacter::GetFeet() const
{
    return GetActorLocation() - FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

void AHouseWalkthroughCharacter::PlaceAtFeet(const FVector& Feet, const FVector& Direction)
{
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocation(Feet + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), false, nullptr, ETeleportType::TeleportPhysics);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    if (Controller) Controller->SetControlRotation(Direction.Rotation());
}

void AHouseWalkthroughCharacter::JumpToLanding(int32 Number)
{
    static constexpr int32 Indices[] = { 9, 0, 3, 1, 2, 4, 5, 6, 7, 8 };
    if (IsAutomatedReview() && RouteIndex >= 0) return;
    if (Number < 0 || Number > 9 || !Landings.IsValidIndex(Indices[Number])) return;
    const FHouseReviewLanding& Landing = Landings[Indices[Number]];
    PlaceAtFeet(Landing.Feet, Landing.Direction);
    Status = Landing.Name;
}

bool AHouseWalkthroughCharacter::FindSupport(const FVector& ExpectedFeet, FHitResult& Hit) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseReviewSupport), false, this);
    return GetWorld()->LineTraceSingleByChannel(Hit, ExpectedFeet + FVector(0, 0, 35),
        ExpectedFeet - FVector(0, 0, 70), ECC_Pawn, Query) &&
        Hit.ImpactNormal.Z >= GetCharacterMovement()->GetWalkableFloorZ();
}

void AHouseWalkthroughCharacter::AddError(const FString& Message)
{
    Errors.Add(MakeShared<FJsonValueString>(Message));
    UE_LOG(LogHouseReview, Error, TEXT("%s"), *Message);
}

void AHouseWalkthroughCharacter::StartReview()
{
#if WITH_EDITOR
    // The first native launch can still be compiling imported mesh/material
    // resources. Capture the completed house, not temporary default shaders.
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
#endif
    UE_LOG(LogHouseReview, Display, TEXT("SSO_HOUSE_RENDER_ASSETS_READY"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    ReviewStartTime = FPlatformTime::Seconds();
    PhaseStartTime = ReviewStartTime;
    GetCharacterMovement()->MaxWalkSpeed = 200.0f;
    Flashlight->SetVisibility(false);
    if (bSmoke)
    {
        if (!GetCapsuleComponent()->IsQueryCollisionEnabled() ||
            GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_WorldStatic) != ECR_Block ||
            GetCharacterMovement()->GravityScale <= 0 || GetCharacterMovement()->GetGravityZ() >= 0)
            AddError(TEXT("Smoke requires the real blocking capsule and downward gravity to be enabled."));
        if (Routes.IsEmpty() || RouteEndIndex <= RouteStartIndex)
            AddError(TEXT("No routes selected for native connectivity smoke."));
        for (const FHouseReviewLanding& Landing : Landings)
        {
            FHitResult Hit;
            const bool bSupported = FindSupport(Landing.Feet, Hit) && FMath::Abs(Landing.Feet.Z - Hit.ImpactPoint.Z) < 25.0f;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseReviewCapsule), false, this);
            const FVector Center = Landing.Feet + FVector(0, 0, 90);
            const bool bOverlapping = GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeCapsule(32.0f, 90.0f), Query);
            TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("landing"), Landing.Name);
            Result->SetBoolField(TEXT("support"), bSupported);
            Result->SetBoolField(TEXT("capsuleClear"), !bOverlapping);
            Result->SetNumberField(TEXT("floorErrorCm"), Hit.bBlockingHit ? Landing.Feet.Z - Hit.ImpactPoint.Z : 9999);
            Result->SetStringField(TEXT("supportActor"), Hit.GetActor() ? Hit.GetActor()->GetName() : TEXT(""));
            Result->SetField(TEXT("expectedFeetCm"), HouseReview::VectorJson(Landing.Feet));
            Result->SetField(TEXT("supportNormal"), HouseReview::VectorJson(Hit.ImpactNormal));
            FloorResults.Add(MakeShared<FJsonValueObject>(Result));
            if (!bSupported || bOverlapping) AddError(TEXT("Landing support/capsule check failed: ") + Landing.Name);
        }
    }
    WriteReport(TEXT("RUNNING"));
}

void AHouseWalkthroughCharacter::StartRoute()
{
    RouteIndex = RouteIndex < 0 ? RouteStartIndex : RouteIndex + 1;
    if (!Routes.IsValidIndex(RouteIndex) || RouteIndex >= RouteEndIndex) { StartCaptures(); return; }
    const FHouseReviewRoute& Route = Routes[RouteIndex];
    RouteWaypoints.Reset();
    RouteDistance = 0;
    RouteSimulationSeconds = 0;
    WaypointSimulationSeconds = 0;
    NoProgressSimulationSeconds = 0;
    PhaseStartTime = FPlatformTime::Seconds();
    if (Route.Feet.Num() < 2) { FinishRoute(false, TEXT("Fewer than two waypoints")); return; }
    WaypointIndex = 1;
    PlaceAtFeet(Route.Feet[0], (Route.Feet[1] - Route.Feet[0]).GetSafeNormal2D());
    PreviousPosition = GetActorLocation();
    LastBlocker.Reset();
    BestWaypointDistance = BIG_NUMBER;
    Status = FString::Printf(TEXT("Checking route %d/%d: %s"), RouteIndex + 1, Routes.Num(), *Route.Name);
}

void AHouseWalkthroughCharacter::UpdateReview(float DeltaSeconds)
{
    if (bReviewFinished) return;
    const double Now = FPlatformTime::Seconds();
    // Bound a bad environment or unattended graphics launch to twenty minutes.
    if (Now - ReviewStartTime > 1500) { AddError(TEXT("Native review exceeded 25 minute limit.")); FinishReview(); return; }
    if (CaptureIndex >= 0) { UpdateCaptures(); return; }
    if (RouteIndex < 0)
    {
        if (Now - PhaseStartTime < 2.0) return;
        if (bSmoke) StartRoute(); else StartCaptures();
        return;
    }
    if (!Routes.IsValidIndex(RouteIndex) || !Routes[RouteIndex].Feet.IsValidIndex(WaypointIndex)) return;
    RouteSimulationSeconds += DeltaSeconds;
    if (RouteSimulationSeconds < 0.35) return;
    WaypointSimulationSeconds += DeltaSeconds;
    NoProgressSimulationSeconds += DeltaSeconds;
    const FHouseReviewRoute& Route = Routes[RouteIndex];
    RouteDistance += FVector::Distance(GetActorLocation(), PreviousPosition);
    PreviousPosition = GetActorLocation();
    const FVector Delta = Route.Feet[WaypointIndex] - GetFeet();
    const float Horizontal = Delta.Size2D();
    const float Distance = Delta.Size();
    if (Distance < BestWaypointDistance - 2.0f) { BestWaypointDistance = Distance; NoProgressSimulationSeconds = 0; }
    if (Horizontal < 12.0f && FMath::Abs(Delta.Z) < 28.0f && GetCharacterMovement()->IsMovingOnGround())
    {
        TSharedRef<FJsonObject> Point = MakeShared<FJsonObject>();
        Point->SetNumberField(TEXT("index"), WaypointIndex);
        Point->SetField(TEXT("actualFeetCm"), HouseReview::VectorJson(GetFeet()));
        Point->SetField(TEXT("expectedFeetCm"), HouseReview::VectorJson(Route.Feet[WaypointIndex]));
        Point->SetNumberField(TEXT("errorCm"), Distance);
        Point->SetNumberField(TEXT("simulationSeconds"), WaypointSimulationSeconds);
        Point->SetBoolField(TEXT("grounded"), true);
        RouteWaypoints.Add(MakeShared<FJsonValueObject>(Point));
        ++WaypointIndex;
        if (!Route.Feet.IsValidIndex(WaypointIndex)) { FinishRoute(true, TEXT("")); return; }
        BestWaypointDistance = BIG_NUMBER;
        WaypointSimulationSeconds = 0;
        NoProgressSimulationSeconds = 0;
    }
    else if (NoProgressSimulationSeconds > 4.0 || WaypointSimulationSeconds > 35.0 || GetFeet().Z < -1200.0)
    {
        FHitResult Blocker;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseReviewBlocker), false, this);
        GetWorld()->SweepSingleByChannel(Blocker, GetActorLocation(), GetActorLocation() + Delta.GetSafeNormal2D() * 60.0,
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(31.0f, 88.0f), Query);
        LastBlocker = Blocker.GetActor() ? Blocker.GetActor()->GetName() : TEXT("no forward blocker; check floor support");
        FinishRoute(false, FString::Printf(TEXT("Waypoint %d stalled; error %.1f cm; %s"), WaypointIndex, Distance, *LastBlocker));
        return;
    }
    // Horizontal steering only. Gravity and the real collision must carry the capsule over stairs.
    const FVector ToTarget = Route.Feet[WaypointIndex] - GetFeet();
    if (Controller) Controller->SetControlRotation(ToTarget.GetSafeNormal2D().Rotation());
    AddMovementInput(ToTarget.GetSafeNormal2D(), FMath::Clamp(float(ToTarget.Size2D()) / 45.0f, 0.08f, 1.0f));
}

void AHouseWalkthroughCharacter::FinishRoute(bool bReached, const FString& Reason)
{
    const FHouseReviewRoute& Route = Routes[RouteIndex];
    const bool bGrounded = GetCharacterMovement()->IsMovingOnGround();
    const bool bComplete = Route.Feet.Num() >= 2 && RouteWaypoints.Num() == Route.Feet.Num() - 1;
    FString Failure = Reason;
    if (bReached && (!bGrounded || !bComplete))
    {
        bReached = false;
        Failure = TEXT("Route did not finish all waypoints grounded.");
    }
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("route"), Route.Name);
    Result->SetBoolField(TEXT("reached"), bReached);
    Result->SetBoolField(TEXT("grounded"), bGrounded);
    Result->SetNumberField(TEXT("routeIndex"), RouteIndex);
    Result->SetNumberField(TEXT("expectedWaypointCount"), FMath::Max(0, Route.Feet.Num() - 1));
    Result->SetNumberField(TEXT("wallSeconds"), FPlatformTime::Seconds() - PhaseStartTime);
    Result->SetNumberField(TEXT("simulationSeconds"), RouteSimulationSeconds);
    Result->SetNumberField(TEXT("endpointErrorCm"), Route.Feet.IsEmpty() ? 9999 : FVector::Distance(GetFeet(), Route.Feet.Last()));
    Result->SetNumberField(TEXT("distanceCm"), RouteDistance);
    Result->SetField(TEXT("actualFeetCm"), HouseReview::VectorJson(GetFeet()));
    Result->SetArrayField(TEXT("waypoints"), RouteWaypoints);
    Result->SetStringField(TEXT("failure"), Failure);
    RouteResults.Add(MakeShared<FJsonValueObject>(Result));
    if (!bReached) AddError(TEXT("Route failed: ") + Route.Name + TEXT(" — ") + Failure);
    UE_LOG(LogHouseReview, Display, TEXT("SSO_HOUSE_ROUTE %s reached=%d"), *Route.Name, bReached);
    WriteReport(TEXT("RUNNING"));
    StartRoute();
}

void AHouseWalkthroughCharacter::StartCaptures()
{
    GetCharacterMovement()->StopMovementImmediately();
    if (!bCapture) { FinishReview(); return; }
    if (Views.IsEmpty()) { AddError(TEXT("No review cameras loaded.")); FinishReview(); return; }
    CaptureCamera = GetWorld()->SpawnActor<ACameraActor>();
    if (!CaptureCamera) { AddError(TEXT("Could not create review camera.")); FinishReview(); return; }
    if (APlayerController* Player = Cast<APlayerController>(Controller)) Player->SetViewTarget(CaptureCamera);
    else { AddError(TEXT("No player viewport controller for requested captures.")); FinishReview(); return; }
    CaptureCamera->GetCameraComponent()->bConstrainAspectRatio = false;
    CaptureCamera->GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
    CaptureCamera->GetCameraComponent()->SetAspectRatioAxisConstraint(AspectRatio_MaintainXFOV);
    CaptureIndex = 0;
    PhaseStartTime = 0;
}

void AHouseWalkthroughCharacter::UpdateCaptures()
{
    const double Now = FPlatformTime::Seconds();
    if (!Views.IsValidIndex(CaptureIndex)) { FinishReview(); return; }
    const FHouseReviewView& View = Views[CaptureIndex];
    if (PhaseStartTime == 0)
    {
        APlayerController* Player = Cast<APlayerController>(Controller);
        CaptureViewportWidth = 0;
        CaptureViewportHeight = 0;
        if (Player) Player->GetViewportSize(CaptureViewportWidth, CaptureViewportHeight);
        if (CaptureViewportWidth <= 0 || CaptureViewportHeight <= 0)
        {
            AddError(TEXT("Capture requires a nonzero native viewport."));
            FinishReview();
            return;
        }
        const float Aspect = float(CaptureViewportWidth) / float(CaptureViewportHeight);
        CaptureHorizontalFOV = FMath::RadiansToDegrees(2.0f * FMath::Atan(
            FMath::Tan(FMath::DegreesToRadians(View.VerticalFieldOfView * 0.5f)) * Aspect));
        CaptureCamera->SetActorLocationAndRotation(View.Position, (View.Target - View.Position).Rotation());
        CaptureCamera->GetCameraComponent()->SetAspectRatio(Aspect);
        CaptureCamera->GetCameraComponent()->SetFieldOfView(CaptureHorizontalFOV);
        Status = TEXT("Capturing ") + View.Name;
        PhaseStartTime = Now;
        PendingScreenshot.Reset();
        ScreenshotRequestedTime = 0;
        return;
    }
    if (Now - PhaseStartTime < 2.5) return;
    if (PendingScreenshot.IsEmpty())
    {
        int32 CurrentWidth = 0;
        int32 CurrentHeight = 0;
        if (APlayerController* Player = Cast<APlayerController>(Controller)) Player->GetViewportSize(CurrentWidth, CurrentHeight);
        if (CurrentWidth != CaptureViewportWidth || CurrentHeight != CaptureViewportHeight)
        {
            // A resize invalidates the FOV conversion; settle again at the new viewport aspect.
            PhaseStartTime = 0;
            return;
        }
        PendingScreenshot = OutputDirectory / (View.Name + TEXT(".png"));
        if (!IFileManager::Get().Delete(*PendingScreenshot, false, true))
        {
            AddError(TEXT("Cannot clear previous screenshot: ") + View.Name);
            FinishReview();
            return;
        }
        ScreenshotRequestedTime = Now;
        FScreenshotRequest::RequestScreenshot(PendingScreenshot, false, false);
        return;
    }
    if (Now - ScreenshotRequestedTime < 0.5) return;
    if (IFileManager::Get().FileSize(*PendingScreenshot) <= 0)
    {
        if (Now - ScreenshotRequestedTime < 12.0) return;
        AddError(TEXT("Screenshot did not complete: ") + View.Name);
    }
    else
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("view"), View.Name);
        Result->SetStringField(TEXT("path"), PendingScreenshot);
        Result->SetNumberField(TEXT("viewportWidth"), CaptureViewportWidth);
        Result->SetNumberField(TEXT("viewportHeight"), CaptureViewportHeight);
        Result->SetNumberField(TEXT("aspectRatio"), double(CaptureViewportWidth) / CaptureViewportHeight);
        Result->SetNumberField(TEXT("sourceVerticalFovDegrees"), View.VerticalFieldOfView);
        Result->SetNumberField(TEXT("unrealHorizontalFovDegrees"), CaptureHorizontalFOV);
        Result->SetNumberField(TEXT("bytes"), IFileManager::Get().FileSize(*PendingScreenshot));
        CaptureResults.Add(MakeShared<FJsonValueString>(PendingScreenshot));
        CaptureMetadata.Add(MakeShared<FJsonValueObject>(Result));
        UE_LOG(LogHouseReview, Display, TEXT("SSO_HOUSE_CAPTURE %s"), *PendingScreenshot);
    }
    ++CaptureIndex;
    PhaseStartTime = 0;
    WriteReport(TEXT("RUNNING"));
}

bool AHouseWalkthroughCharacter::WriteReport(const FString& Result)
{
    TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("status"), Result);
    Report->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
    Report->SetStringField(TEXT("startedUtc"), ReviewStartedUtc);
    Report->SetNumberField(TEXT("elapsedWallSeconds"), FPlatformTime::Seconds() - ReviewStartTime);
    Report->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
    Report->SetStringField(TEXT("description"), TEXT("Opt-in native Unreal map inspection. Assisted routes and captures; not a campaign playthrough."));
    Report->SetStringField(TEXT("layout"), FPaths::GetCleanFilename(LayoutPath));
    Report->SetStringField(TEXT("coordinateSystem"), TEXT("Unreal centimetres = (Unity.z, Unity.x, Unity.y) * 100"));
    Report->SetNumberField(TEXT("capsuleRadiusCm"), 32);
    Report->SetNumberField(TEXT("capsuleHalfHeightCm"), 90);
    Report->SetBoolField(TEXT("connectivityRequested"), bSmoke);
    Report->SetNumberField(TEXT("routeStartIndex"), RouteStartIndex);
    Report->SetNumberField(TEXT("routeEndIndexExclusive"), RouteEndIndex);
    Report->SetNumberField(TEXT("totalAvailableRoutes"), Routes.Num());
    Report->SetBoolField(TEXT("allRoutesRequested"), bSmoke && RouteStartIndex == 0 && RouteEndIndex == Routes.Num());
    Report->SetBoolField(TEXT("capturesRequested"), bCapture);
    Report->SetBoolField(TEXT("allViewsRequested"), bCapture && bAllViews);
    Report->SetNumberField(TEXT("expectedCaptureCount"), ExpectedCaptureCount);
    Report->SetNumberField(TEXT("totalAvailableViews"), TotalAvailableViews);
    Report->SetNumberField(TEXT("expectedRouteCount"), bSmoke ? RouteEndIndex - RouteStartIndex : 0);
    Report->SetNumberField(TEXT("expectedFloorCount"), bSmoke ? Landings.Num() : 0);
    Report->SetStringField(TEXT("fovBasis"), TEXT("Canonical vertical degrees converted to Unreal horizontal degrees: 2*atan(tan(vertical/2)*actualViewportWidth/actualViewportHeight); MaintainXFOV; no aspect bars"));
    Report->SetStringField(TEXT("routeMethod"), TEXT("Teleport only to each route start; stock CharacterMovement with horizontal steering, collision, gravity and grounded waypoint arrival. No jumping or vertical steering."));
    Report->SetNumberField(TEXT("maximumReviewWallSeconds"), 1200);
    Report->SetNumberField(TEXT("maximumStalledSimulationSeconds"), 4);
    Report->SetNumberField(TEXT("maximumWaypointSimulationSeconds"), 35);
    Report->SetArrayField(TEXT("floors"), FloorResults);
    Report->SetArrayField(TEXT("routes"), RouteResults);
    Report->SetArrayField(TEXT("captures"), CaptureResults);
    Report->SetArrayField(TEXT("captureMetadata"), CaptureMetadata);
    Report->SetArrayField(TEXT("errors"), Errors);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    if (!FFileHelper::SaveStringToFile(Json + TEXT("\n"), *(OutputDirectory / TEXT("smoke_report.json"))))
    {
        if (!bReportWriteFailed) AddError(TEXT("Could not write house review report to ") + OutputDirectory);
        bReportWriteFailed = true;
        return false;
    }
    return true;
}

void AHouseWalkthroughCharacter::FinishReview()
{
    bReviewFinished = true;
    GetCharacterMovement()->StopMovementImmediately();
    if (bSmoke && (FloorResults.Num() != Landings.Num() || RouteResults.Num() != RouteEndIndex - RouteStartIndex))
        AddError(TEXT("Native connectivity run is incomplete: result counts do not match selected floors/routes."));
    if (bCapture && CaptureResults.Num() != ExpectedCaptureCount)
        AddError(TEXT("Native capture run is incomplete: successful screenshots do not match selected views."));
    bool bPassed = Errors.IsEmpty() && !bReportWriteFailed;
    Status = bPassed ? TEXT("Native review passed") : TEXT("Native review found issues; see smoke_report.json");
    if (!WriteReport(bPassed ? TEXT("PASS") : TEXT("FAIL"))) bPassed = false;
    UE_LOG(LogHouseReview, Display, TEXT("SSO_HOUSE_REVIEW %s floors=%d routes=%d captures=%d errors=%d"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), FloorResults.Num(), RouteResults.Num(), CaptureResults.Num(), Errors.Num());
    if (bExitAfterReview) FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void AHouseWalkthroughController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
    if (PlayerCameraManager)
    {
        PlayerCameraManager->ViewPitchMin = -85.0f;
        PlayerCameraManager->ViewPitchMax = 85.0f;
    }
}

void AHouseWalkthroughController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AHouseWalkthroughController::ToggleReviewPause).bExecuteWhenPaused = true;
    InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AHouseWalkthroughController::QuitReview).bExecuteWhenPaused = true;
    const FKey Keys[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
        EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
    for (int32 Number = 0; Number < 10; ++Number)
    {
        FInputKeyBinding Binding(FInputChord(Keys[Number]), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this, Number]() { SelectLanding(Number); });
        InputComponent->KeyBindings.Add(MoveTemp(Binding));
    }
}

void AHouseWalkthroughController::SelectLanding(int32 Number)
{
    if (AHouseWalkthroughCharacter* Character = Cast<AHouseWalkthroughCharacter>(GetPawn()))
        if (!Character->IsAutomatedReview()) Character->JumpToLanding(Number);
}

void AHouseWalkthroughController::ToggleReviewPause()
{
    const AHouseWalkthroughCharacter* Character = Cast<AHouseWalkthroughCharacter>(GetPawn());
    if (Character && Character->IsAutomatedReview()) return;
    const bool bPaused = !IsPaused();
    SetPause(bPaused);
    bShowMouseCursor = bPaused;
    if (bPaused)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        SetInputMode(InputMode);
    }
    else SetInputMode(FInputModeGameOnly());
}

void AHouseWalkthroughController::QuitReview()
{
    // Let Unreal handle standalone versus Play-In-Editor quit semantics.
    if (IsPaused()) ConsoleCommand(TEXT("quit"));
}

void AHouseWalkthroughHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    const AHouseWalkthroughCharacter* Character = Cast<AHouseWalkthroughCharacter>(GetOwningPawn());
    if (Character && Character->IsAutomatedReview()) return;
    const float Scale = FMath::Clamp(Canvas->SizeX / 1600.0f, 0.65f, 1.3f);
    DrawRect(FLinearColor(0.015f, 0.02f, 0.03f, 0.78f), 16, 16, Canvas->SizeX - 32, 80 * Scale);
    DrawText(TEXT("SENIOR SEND-OFF  |  HOUSE WALKTHROUGH"), FLinearColor(0.9f, 0.78f, 0.56f), 28, 24, nullptr, Scale);
    DrawText(TEXT("WASD move  |  Mouse look  |  Shift sprint  |  Space jump  |  F flashlight  |  Esc pause"),
        FLinearColor::White, 28, 48 * Scale, nullptr, Scale);
    if (Character) DrawText(Character->GetReviewStatus(), FLinearColor(0.75f, 0.83f, 0.91f), 28, 70 * Scale, nullptr, Scale);
    DrawRect(FLinearColor(0.015f, 0.02f, 0.03f, 0.78f), 16, Canvas->SizeY - 64 * Scale, Canvas->SizeX - 32, 48 * Scale);
    DrawText(TEXT("Review jumps: 1 great room   2 black hall   3 hall   4 library   5 gallery   6 stair landing   7 dining   8 porch   9 second floor   0 balcony"),
        FLinearColor::White, 28, Canvas->SizeY - 51 * Scale, nullptr, Scale * 0.86f);
    if (GetOwningPlayerController() && GetOwningPlayerController()->IsPaused())
    {
        DrawRect(FLinearColor(0.01f, 0.015f, 0.02f, 0.92f), Canvas->SizeX * 0.5f - 210, Canvas->SizeY * 0.5f - 50, 420, 100);
        DrawText(TEXT("Paused — Esc resumes / Q quits"), FLinearColor::White, Canvas->SizeX * 0.5f - 185, Canvas->SizeY * 0.5f - 12);
    }
}

AHouseWalkthroughGameMode::AHouseWalkthroughGameMode()
{
    DefaultPawnClass = AHouseWalkthroughCharacter::StaticClass();
    PlayerControllerClass = AHouseWalkthroughController::StaticClass();
    HUDClass = AHouseWalkthroughHUD::StaticClass();
}
