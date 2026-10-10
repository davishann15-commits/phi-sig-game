#include "HouseVentNetwork.h"

#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseWorldScale.h"
#include "HouseFurnitureInteractionComponent.h"
#include "StoryCampaign.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

namespace
{
enum class EAnchor : uint8 { Pigsty, HallNorth, HallEast, Library };

struct FPortal
{
    const TCHAR* Id;
    const TCHAR* Label;
    EAnchor Anchor;
    FVector Facade;
    FVector RoomForward;
    float RoomFloorZ;
    FIntPoint Cell;
    float DuctYaw;
};

// Facade positions are the existing map/black-hall builder's measured centres.
// These do not describe holes: every shell behind them is still solid.
const FPortal Portals[] = {
    { TEXT("PigstyVent"), TEXT("Pigsty vent"), EAnchor::Pigsty,
        FVector(-2468.f, -589.f, 505.f), FVector(0.f, -1.f, 0.f), 302.3f, FIntPoint(-6, 0), 0.f },
    { TEXT("BlackHallNorthVent"), TEXT("Black hall north vent"), EAnchor::HallNorth,
        FVector(1092.f, 791.25f, -71.f), FVector(0.f, -1.f, 0.f), -329.f, FIntPoint(6, 0), 180.f },
    { TEXT("BlackHallEastVent"), TEXT("Black hall east vent"), EAnchor::HallEast,
        FVector(1223.25f, -100.f, -71.f), FVector(-1.f, 0.f, 0.f), -329.f, FIntPoint(6, 5), 180.f },
    { TEXT("LibraryHatch01"), TEXT("Library ceiling hatch 1"), EAnchor::Library,
        FVector(-2700.f, 283.5f, -101.35f), FVector::ZeroVector, -329.f, FIntPoint(-6, 5), 0.f },
    { TEXT("LibraryHatch02"), TEXT("Library ceiling hatch 2"), EAnchor::Library,
        FVector(-2700.f, -292.5f, -101.35f), FVector::ZeroVector, -329.f, FIntPoint(0, 5), -90.f },
    { TEXT("LibraryHatch03"), TEXT("Library ceiling hatch 3"), EAnchor::Library,
        FVector(-2700.f, -904.5f, -101.35f), FVector::ZeroVector, -329.f, FIntPoint(0, 0), 90.f }
};
constexpr int32 PortalCount = UE_ARRAY_COUNT(Portals);

FPortal ResolvedPortal(const UWorld* World, const FPortal& Authored)
{
    FPortal Portal = Authored;
    Portal.Facade = HouseWorldScale::Point(World, Authored.Facade);
    Portal.RoomFloorZ = HouseWorldScale::Point(World, FVector(0.f, 0.f, Authored.RoomFloorZ)).Z;
    return Portal;
}

int32 PortalIndex(FName Id)
{
    for (int32 Index = 0; Index < PortalCount; ++Index)
        if (Id == FName(Portals[Index].Id)) return Index;
    return INDEX_NONE;
}

bool IsRouteCell(int32 X, int32 Y)
{
    if (X < -6 || X > 6) return false;
    // Two long trunks and three connecting risers form alternative routes.
    if (Y == 0 || Y == 5) return true;
    if (Y > 0 && Y < 5 && (X == -6 || X == 0 || X == 6)) return true;
    // Two maintenance branches leave the lower trunk.
    return (Y == -1 || Y == -2) && (X == -3 || X == 3);
}

bool HasSavedFacade(UWorld* World, const FPortal& Portal)
{
    if (!World) return false;
    if (Portal.Anchor == EAnchor::HallNorth || Portal.Anchor == EAnchor::HallEast)
    {
        const UHouseBlackHallSubsystem* Hall = World->GetSubsystem<UHouseBlackHallSubsystem>();
        return Hall && IsValid(Hall->GetDetail()) && Hall->GetDetail()->bRebuilt;
    }
    if (Portal.Anchor == EAnchor::Pigsty)
    {
        for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        {
            const UStaticMeshComponent* Component = It->GetStaticMeshComponent();
            const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
            if (Mesh && Mesh->GetName() == TEXT("PIG_SM_WallVent")
                && FVector::Dist(It->GetActorLocation(), Portal.Facade) < 45.f) return true;
        }
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("LibraryCeilingPanel"))
            && FVector::Dist(It->GetActorLocation(), Portal.Facade) < 45.f) return true;
    return false;
}
}

AHouseVentNetwork::AHouseVentNetwork()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(false);
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("VentNetworkRoot"));
    Root->SetMobility(EComponentMobility::Static);
    RootComponent = Root;
}

void AHouseVentNetwork::BeginPlay()
{
    Super::BeginPlay();
    // Runtime-only networks can arrive from replication at their class default
    // transform. A saved or already scaled spawn must never be scaled twice.
    if (GetActorScale3D().Equals(FVector::OneVector))
        SetActorScale3D(FVector(HouseWorldScale::Factor(GetWorld())));
    BuildDuct();
    if (HasAuthority())
    {
        RefreshPortalAnchors();
        if (NumActivePortals() < PortalCount)
        {
            FTimerHandle Retry;
            GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this,
                [this]() { RefreshPortalAnchors(); }), .75f, false);
        }
    }
}

void AHouseVentNetwork::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
        for (const auto& Entry : PendingControllers)
            if (APlayerController* PC = Entry.Value.Get()) PC->ClientSetCameraFade(false);
    PendingTransitions.Empty();
    PendingControllers.Empty();
    ReturnPoints.Empty();
    Super::EndPlay(EndPlayReason);
}

void AHouseVentNetwork::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseVentNetwork, ActivePortalMask);
}

void AHouseVentNetwork::RefreshPortalAnchors()
{
    if (!HasAuthority()) return;
    uint8 NewMask = 0;
    if (GetWorld() && UGameplayStatics::GetCurrentLevelName(this, true) == TEXT("Chapter01_House"))
        for (int32 Index = 0; Index < PortalCount; ++Index)
            if (HasSavedFacade(GetWorld(), ResolvedPortal(GetWorld(), Portals[Index]))) NewMask |= uint8(1u << Index);
    if (NewMask != ActivePortalMask)
    {
        ActivePortalMask = NewMask;
        ForceNetUpdate();
    }
    UE_LOG(LogTemp, Display,
        TEXT("HOUSE_VENT_NETWORK portals=%d/%d pigsty=%d hallNorth=%d hallEast=%d library=%d%d%d cells=%d bounds=%s"),
        NumActivePortals(), PortalCount, !!(NewMask & 1), !!(NewMask & 2), !!(NewMask & 4),
        !!(NewMask & 8), !!(NewMask & 16), !!(NewMask & 32), GetDuctSectionCount(), *GetDuctBounds().ToString());
}

bool AHouseVentNetwork::IsPortalActive(FName PortalId) const
{
    const int32 Index = PortalIndex(PortalId);
    return Index != INDEX_NONE && (ActivePortalMask & uint8(1u << Index)) != 0;
}

FString AHouseVentNetwork::PortalDisplayName(FName PortalId) const
{
    const int32 Index = PortalIndex(PortalId);
    return Index == INDEX_NONE ? FString() : FString(Portals[Index].Label);
}

TArray<FName> AHouseVentNetwork::GetPortalIds() const
{
    TArray<FName> Ids;
    Ids.Reserve(PortalCount);
    for (const FPortal& Portal : Portals) Ids.Add(FName(Portal.Id));
    return Ids;
}

int32 AHouseVentNetwork::GetPortalCount() const { return PortalCount; }

int32 AHouseVentNetwork::NumActivePortals() const
{
    int32 Count = 0;
    for (int32 Index = 0; Index < PortalCount; ++Index)
        if (ActivePortalMask & uint8(1u << Index)) ++Count;
    return Count;
}

int32 AHouseVentNetwork::GetDuctSectionCount() const
{
    int32 Count = 0;
    for (int32 Y = -2; Y <= 5; ++Y)
        for (int32 X = -6; X <= 6; ++X)
            if (IsRouteCell(X, Y)) ++Count;
    return Count;
}

FBox AHouseVentNetwork::GetDuctBounds() const
{
    const FVector A = GetActorTransform().TransformPosition(ServiceOrigin
        + FVector(-6.5f * CellSize, -2.5f * CellSize, -4.f));
    const FVector B = GetActorTransform().TransformPosition(ServiceOrigin
        + FVector(6.5f * CellSize, 5.5f * CellSize, ClearHeight + 4.f));
    return FBox(A.ComponentMin(B), A.ComponentMax(B));
}

TArray<FIntPoint> AHouseVentNetwork::GetRouteCells() const
{
    TArray<FIntPoint> Cells;
    Cells.Reserve(GetDuctSectionCount());
    for (int32 Y = -2; Y <= 5; ++Y)
        for (int32 X = -6; X <= 6; ++X)
            if (IsRouteCell(X, Y)) Cells.Add(FIntPoint(X, Y));
    return Cells;
}

TArray<FVector> AHouseVentNetwork::GetDuctCellCenters() const
{
    TArray<FVector> Centers;
    Centers.Reserve(GetDuctSectionCount());
    for (const FIntPoint& Cell : GetRouteCells())
        Centers.Add(GetActorTransform().TransformPosition(CellCenterLocal(Cell) + FVector(0.f, 0.f, ClearHeight * .5f)));
    return Centers;
}

FVector AHouseVentNetwork::CellCenterLocal(const FIntPoint& Cell) const
{
    return ServiceOrigin + FVector(Cell.X * CellSize, Cell.Y * CellSize, 0.f);
}

FVector AHouseVentNetwork::CellCenterWorld(const FIntPoint& Cell) const
{
    return GetActorTransform().TransformPosition(CellCenterLocal(Cell));
}

FVector AHouseVentNetwork::GetPortalDuctLocation(FName PortalId) const
{
    const int32 Index = PortalIndex(PortalId);
    return Index == INDEX_NONE ? FVector::ZeroVector
        : GetActorTransform().TransformPosition(CellCenterLocal(Portals[Index].Cell) + FVector(0.f, 0.f, ClearHeight * .5f));
}

FVector AHouseVentNetwork::GetPortalFacadeLocation(FName PortalId) const
{
    const int32 Index = PortalIndex(PortalId);
    return Index == INDEX_NONE ? FVector::ZeroVector : HouseWorldScale::Point(GetWorld(), Portals[Index].Facade);
}

FVector AHouseVentNetwork::GetPortalRoomApproach(FName PortalId) const
{
    const int32 Index = PortalIndex(PortalId);
    if (Index == INDEX_NONE || !GetWorld()) return FVector::ZeroVector;
    const FPortal Portal = ResolvedPortal(GetWorld(), Portals[Index]);
    const FVector Forward = Portal.RoomForward.IsNearlyZero() ? FVector::ZeroVector : Portal.RoomForward;
    FVector Base = Portal.Anchor == EAnchor::Pigsty
        ? HouseWorldScale::Point(GetWorld(), FVector(-2530.f, -900.f, 0.f))
        : Portal.Facade + Forward * 120.f;
    Base.Z = Portal.RoomFloorZ;
    // Ceiling hatches sit over tables, chairs and a coffee chest. Query the
    // live map so the approach can move to nearby clear floor as furniture
    // changes, while always retaining a direct view of the same facade.
    const FVector2D Offsets[] = {
        FVector2D::ZeroVector,
        FVector2D(90.f, 0.f), FVector2D(-90.f, 0.f),
        FVector2D(0.f, 90.f), FVector2D(0.f, -90.f),
        FVector2D(90.f, 90.f), FVector2D(90.f, -90.f),
        FVector2D(-90.f, 90.f), FVector2D(-90.f, -90.f),
        FVector2D(150.f, 0.f), FVector2D(-150.f, 0.f),
        FVector2D(0.f, 150.f), FVector2D(0.f, -150.f),
        FVector2D(150.f, 75.f), FVector2D(150.f, -75.f),
        FVector2D(-150.f, 75.f), FVector2D(-150.f, -75.f)
    };
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseVentRoomApproach), false);
    Query.AddIgnoredActor(this);
    for (const FVector2D& Offset : Offsets)
    {
        const FVector Foot(Base.X + Offset.X, Base.Y + Offset.Y, Portal.RoomFloorZ);
        const FVector Eye = Foot + FVector(0.f, 0.f, 160.f);
        if (FVector::Dist(Eye, Portal.Facade) > 265.f) continue;
        FHitResult FloorHit;
        if (!GetWorld()->LineTraceSingleByChannel(FloorHit, Foot + FVector(0.f, 0.f, 55.f),
            Foot - FVector(0.f, 0.f, 25.f), ECC_Visibility, Query)
            || FMath::Abs(FloorHit.ImpactPoint.Z - Portal.RoomFloorZ) > 14.f
            || FloorHit.ImpactNormal.Z < .7f) continue;
        if (GetWorld()->OverlapBlockingTestByChannel(Foot + FVector(0.f, 0.f, 111.f),
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(38.f, 108.f), Query)) continue;
        FHitResult SightHit;
        if (GetWorld()->LineTraceSingleByChannel(SightHit, Eye, Portal.Facade,
            ECC_Visibility, Query)
            && FVector::Dist(SightHit.ImpactPoint, Portal.Facade) > 32.f) continue;
        return Foot;
    }
    return FVector::ZeroVector;
}

bool AHouseVentNetwork::IsInDuct(const APawn* Pawn) const
{
    if (!Pawn) return false;
    const FVector Local = GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation()) - ServiceOrigin;
    if (Local.Z < -15.f || Local.Z > ClearHeight + 25.f) return false;
    const int32 X = FMath::RoundToInt(Local.X / CellSize);
    const int32 Y = FMath::RoundToInt(Local.Y / CellSize);
    return IsRouteCell(X, Y) && FMath::Abs(Local.X - X * CellSize) <= CellSize * .5f + 8.f
        && FMath::Abs(Local.Y - Y * CellSize) <= CellSize * .5f + 8.f;
}

FName AHouseVentNetwork::FindFocusedPortal(const APawn* Pawn, float MinimumFacing) const
{
    if (!Pawn || Pawn->GetWorld() != GetWorld()) return NAME_None;
    FVector View = Pawn->GetActorLocation();
    FRotator ViewRotation = Pawn->GetActorRotation();
    if (const AController* Controller = Pawn->GetController())
        Controller->GetPlayerViewPoint(View, ViewRotation);
    const bool bInside = IsInDuct(Pawn);
    float BestScore = BIG_NUMBER;
    FName Best = NAME_None;
    for (int32 Index = 0; Index < PortalCount; ++Index)
    {
        if (!(ActivePortalMask & uint8(1u << Index))) continue;
        const FVector Target = bInside ? GetPortalDuctLocation(FName(Portals[Index].Id))
            : GetPortalFacadeLocation(FName(Portals[Index].Id));
        const FVector ToTarget = Target - View;
        const float Distance = ToTarget.Size();
        if (Distance < 1.f || Distance > (bInside ? CellSize * GetActorScale3D().X * 1.25f : 270.f)) continue;
        if (!bInside)
        {
            if (FVector::DotProduct(ViewRotation.Vector(), ToTarget / Distance) < MinimumFacing) continue;
            FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseVentFacadeFocus), false);
            Query.AddIgnoredActor(Pawn);
            Query.AddIgnoredActor(this);
            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByChannel(Hit, View, Target, ECC_Visibility, Query)
                && FVector::Dist(Hit.ImpactPoint, Target) > 32.f) continue;
        }
        const float Score = Distance + (bInside ? 0.f
            : FVector::CrossProduct(ViewRotation.Vector(), ToTarget).Size() * 2.f);
        if (Score < BestScore) { BestScore = Score; Best = FName(Portals[Index].Id); }
    }
    return Best;
}

void AHouseVentNetwork::BuildDuct()
{
    if (DuctShell || !GetWorld()) return;
    FHallMesh Shell;
    FHallMesh Trim;
    const FHallUV UV = FHallUV::Local(FTransform::Identity, 85.f);
    const float Half = CellSize * .5f;
    const float Wall = 3.f;
    for (int32 Y = -2; Y <= 5; ++Y)
        for (int32 X = -6; X <= 6; ++X)
        {
            if (!IsRouteCell(X, Y)) continue;
            const FVector C = CellCenterLocal(FIntPoint(X, Y));
            Shell.BoxMinMax(TEXT("Metal"), C + FVector(-Half, -Half, -4.f),
                C + FVector(Half, Half, 0.f), UV);
            Shell.BoxMinMax(TEXT("Metal"), C + FVector(-Half, -Half, ClearHeight),
                C + FVector(Half, Half, ClearHeight + 4.f), UV);
            if (!IsRouteCell(X + 1, Y))
                Shell.BoxMinMax(TEXT("Metal"), C + FVector(Half - Wall, -Half, 0.f),
                    C + FVector(Half, Half, ClearHeight), UV);
            if (!IsRouteCell(X - 1, Y))
                Shell.BoxMinMax(TEXT("Metal"), C + FVector(-Half, -Half, 0.f),
                    C + FVector(-Half + Wall, Half, ClearHeight), UV);
            if (!IsRouteCell(X, Y + 1))
                Shell.BoxMinMax(TEXT("Metal"), C + FVector(-Half, Half - Wall, 0.f),
                    C + FVector(Half, Half, ClearHeight), UV);
            if (!IsRouteCell(X, Y - 1))
                Shell.BoxMinMax(TEXT("Metal"), C + FVector(-Half, -Half, 0.f),
                    C + FVector(Half, -Half + Wall, ClearHeight), UV);
            // Narrow outside seams suggest joined sheet-metal modules; trim
            // has no collision and never narrows the usable crawl clearance.
            if ((X + 6) % 3 == 0 || Y == 0 || Y == 5)
            {
                Trim.BoxMinMax(TEXT("Trim"), C + FVector(-Half + 3.f, -Half + 3.f, ClearHeight - .4f),
                    C + FVector(-Half + 4.f, Half - 3.f, ClearHeight), UV);
                Trim.BoxMinMax(TEXT("Trim"), C + FVector(Half - 4.f, -Half + 3.f, ClearHeight - .4f),
                    C + FVector(Half - 3.f, Half - 3.f, ClearHeight), UV);
            }
        }

    for (const FPortal& Portal : Portals)
    {
        const FVector C = CellCenterLocal(Portal.Cell);
        // A marked maintenance plate gives each duct-side exit a visible E target.
        Trim.BoxMinMax(TEXT("Trim"), C + FVector(-24.f, -24.f, ClearHeight - .9f),
            C + FVector(24.f, 24.f, ClearHeight - .5f), UV);
    }

    // This private material has a soft ambient response so a player can read
    // each turn without brightening any saved house wall or vent facade.
    UMaterialInterface* Metal = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/SeniorSendOff/House/VentNetwork/M_VentCrawlSteel.M_VentCrawlSteel"));
    UMaterialInterface* TrimMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/SeniorSendOff/House/PigstyRoom/PIG_M_RustyMetal.PIG_M_RustyMetal"));
    if (!Metal) Metal = UMaterial::GetDefaultMaterial(MD_Surface);
    if (!TrimMaterial) TrimMaterial = Metal;
    DuctShell = NewObject<UProceduralMeshComponent>(this, TEXT("VentDuctShell"));
    DuctShell->SetupAttachment(RootComponent);
    DuctShell->SetMobility(EComponentMobility::Static);
    DuctShell->bUseComplexAsSimpleCollision = true;
    DuctShell->bUseAsyncCooking = false;
    DuctShell->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    DuctShell->RegisterComponent();
    Shell.Build(DuctShell, [Metal](FName) { return Metal; }, true);
    DuctTrim = NewObject<UProceduralMeshComponent>(this, TEXT("VentDuctTrim"));
    DuctTrim->SetupAttachment(RootComponent);
    DuctTrim->SetMobility(EComponentMobility::Static);
    DuctTrim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DuctTrim->RegisterComponent();
    Trim.Build(DuctTrim, [TrimMaterial](FName) { return TrimMaterial; }, false);

    const FIntPoint LightCells[] = { FIntPoint(-6, 0), FIntPoint(-3, 0), FIntPoint(0, 0),
        FIntPoint(3, 0), FIntPoint(6, 0), FIntPoint(-6, 5), FIntPoint(-3, 5),
        FIntPoint(0, 5), FIntPoint(3, 5), FIntPoint(6, 5) };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(LightCells); ++Index)
    {
        UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
        Light->SetupAttachment(RootComponent);
        Light->SetRelativeLocation(CellCenterLocal(LightCells[Index]) + FVector(0.f, 0.f, ClearHeight - 9.f));
        Light->SetIntensity(1600.f * FMath::Square(HouseWorldScale::Factor(GetWorld())));
        Light->SetAttenuationRadius(360.f * HouseWorldScale::Factor(GetWorld()));
        Light->SetLightColor(Index % 3 == 0 ? FLinearColor(1.f, .32f, .11f)
            : FLinearColor(.58f, .78f, 1.f));
        Light->SetCastShadows(false);
        Light->RegisterComponent();
        DuctLights.Add(Light);
    }
}

bool AHouseVentNetwork::IsCapsuleClear(const APawn* Pawn, const FVector& Location,
    float Radius, float HalfHeight) const
{
    if (!GetWorld() || !Pawn) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseVentLanding), false);
    Query.AddIgnoredActor(Pawn);
    return !GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity,
        ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Query);
}

bool AHouseVentNetwork::FindDuctLanding(const AStoryFirstPersonCharacter* Pawn,
    int32 PortalIndexValue, FVector& OutLocation) const
{
    if (!Pawn || PortalIndexValue < 0 || PortalIndexValue >= PortalCount) return false;
    const UCapsuleComponent* Capsule = Pawn->GetCapsuleComponent();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float DuctScale = GetActorScale3D().Z;
    if (2.f * Radius > (CellSize - 2.f * 3.f) * DuctScale || 2.f * HalfHeight > (ClearHeight - 4.f) * DuctScale) return false;
    const FIntPoint Cell = Portals[PortalIndexValue].Cell;
    const FIntPoint Candidates[] = { Cell, Cell + FIntPoint(1, 0), Cell + FIntPoint(-1, 0),
        Cell + FIntPoint(0, 1), Cell + FIntPoint(0, -1) };
    for (const FIntPoint& Candidate : Candidates)
    {
        if (!IsRouteCell(Candidate.X, Candidate.Y)) continue;
        const FVector World = GetActorTransform().TransformPosition(
            CellCenterLocal(Candidate) + FVector(0.f, 0.f, HalfHeight / DuctScale + 2.f));
        if (IsCapsuleClear(Pawn, World, Radius, HalfHeight))
        {
            OutLocation = World;
            return true;
        }
    }
    return false;
}

bool AHouseVentNetwork::FindRoomLanding(const AStoryFirstPersonCharacter* Pawn,
    int32 PortalIndexValue, FVector& OutLocation, FRotator& OutRotation) const
{
    if (!Pawn || PortalIndexValue < 0 || PortalIndexValue >= PortalCount || !GetWorld()) return false;
    const FPortal Portal = ResolvedPortal(GetWorld(), Portals[PortalIndexValue]);
    // Reserve enough room for the tallest current character, even while the
    // actual pawn still has its small prone collision capsule.
    const float CurrentRadius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float CurrentHalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float StandingRadius = FMath::Max(38.f, CurrentRadius);
    const float StandingHalfHeight = FMath::Max(108.f, CurrentHalfHeight);
    auto IsSupportedAndClear = [&](const FVector& Foot)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseVentExitFloor), false);
        Query.AddIgnoredActor(Pawn);
        Query.AddIgnoredActor(this);
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit,
            FVector(Foot.X, Foot.Y, Portal.RoomFloorZ + 55.f),
            FVector(Foot.X, Foot.Y, Portal.RoomFloorZ - 25.f), ECC_Visibility, Query)
            || FMath::Abs(Hit.ImpactPoint.Z - Portal.RoomFloorZ) > 14.f
            || Hit.ImpactNormal.Z < .7f) return false;
        const FVector Prone(Foot.X, Foot.Y, Portal.RoomFloorZ + CurrentHalfHeight + 3.f);
        const FVector Standing(Foot.X, Foot.Y, Portal.RoomFloorZ + StandingHalfHeight + 3.f);
        return IsCapsuleClear(Pawn, Prone, CurrentRadius, CurrentHalfHeight)
            && IsCapsuleClear(Pawn, Standing, StandingRadius, StandingHalfHeight);
    };
    if (const FReturnPoint* Return = ReturnPoints.Find(
        TWeakObjectPtr<AStoryFirstPersonCharacter>(const_cast<AStoryFirstPersonCharacter*>(Pawn))))
        if (Return->PortalId == FName(Portal.Id)
            && FVector::Dist2D(Return->Transform.GetLocation(), Portal.Facade) < 270.f
            && IsSupportedAndClear(Return->Transform.GetLocation()))
        {
            OutLocation = FVector(Return->Transform.GetLocation().X, Return->Transform.GetLocation().Y,
                Portal.RoomFloorZ + CurrentHalfHeight + 3.f);
            OutRotation = Return->Transform.Rotator();
            return true;
        }

    const FVector PreferredFoot = GetPortalRoomApproach(FName(Portal.Id));
    if (!PreferredFoot.IsNearlyZero() && IsSupportedAndClear(PreferredFoot))
    {
        OutLocation = FVector(PreferredFoot.X, PreferredFoot.Y,
            Portal.RoomFloorZ + CurrentHalfHeight + 3.f);
        OutRotation = Portal.RoomForward.IsNearlyZero() ? Pawn->GetActorRotation()
            : Portal.RoomForward.Rotation();
        return true;
    }

    const FVector Forward = Portal.RoomForward.IsNearlyZero()
        ? FVector(1.f, 0.f, 0.f) : Portal.RoomForward;
    const FVector Side(-Forward.Y, Forward.X, 0.f);
    const float Depths[] = { Portal.RoomForward.IsNearlyZero() ? 0.f : 120.f, 160.f, 200.f, 75.f };
    const float Sides[] = { 0.f, 65.f, -65.f, 120.f, -120.f };
    for (float Depth : Depths)
        for (float Offset : Sides)
        {
            FVector Candidate = Portal.Facade + Forward * Depth + Side * Offset;
            if (!IsSupportedAndClear(Candidate)) continue;
            OutLocation = FVector(Candidate.X, Candidate.Y,
                Portal.RoomFloorZ + CurrentHalfHeight + 3.f);
            OutRotation = Portal.RoomForward.IsNearlyZero() ? Pawn->GetActorRotation()
                : Portal.RoomForward.Rotation();
            return true;
        }
    return false;
}

bool AHouseVentNetwork::Interact(AStoryFirstPersonCharacter* Pawn, FName PortalId)
{
    if (!HasAuthority() || !IsValid(Pawn) || Pawn->GetWorld() != GetWorld()
        || !Pawn->GetController() || !IsPortalActive(PortalId)) return false;
    for (auto It = PendingTransitions.CreateIterator(); It; ++It)
        if (!It->IsValid()) It.RemoveCurrent();
    const TWeakObjectPtr<AStoryFirstPersonCharacter> WeakPawn(Pawn);
    if (PendingTransitions.Contains(WeakPawn)) return false;
    const TWeakObjectPtr<APlayerController> WeakController(Cast<APlayerController>(Pawn->GetController()));
    const int32 Index = PortalIndex(PortalId);
    const bool bEntering = !IsInDuct(Pawn);
    if (FindFocusedPortal(Pawn, .2f) != PortalId) return false;
    if (bEntering && (Pawn->CarriedHousePig()
        || (Pawn->FurnitureInteraction && Pawn->FurnitureInteraction->bHoldingFurniture))) return false;
    if (!bEntering)
    {
        FVector Landing; FRotator Rotation;
        if (!FindRoomLanding(Pawn, Index, Landing, Rotation)) return false;
    }
    PendingTransitions.Add(WeakPawn);
    PendingControllers.Add(WeakPawn, WeakController);
    if (APlayerController* PC = WeakController.Get())
        PC->ClientSetCameraFade(true, FColor::Black, FVector2D(0.f, 1.f), .12f, false, true);
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this,
        [this, WeakPawn, WeakController, Index, bEntering]()
        { FinishTransition(WeakPawn, WeakController, Index, bEntering); }), .15f, false);
    return true;
}

void AHouseVentNetwork::FinishTransition(TWeakObjectPtr<AStoryFirstPersonCharacter> WeakPawn,
    TWeakObjectPtr<APlayerController> WeakController, int32 PortalIndexValue, bool bEntering)
{
    PendingTransitions.Remove(WeakPawn);
    PendingControllers.Remove(WeakPawn);
    APlayerController* PC = WeakController.Get();
    AStoryFirstPersonCharacter* Pawn = WeakPawn.Get();
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || PortalIndexValue < 0
        || PortalIndexValue >= PortalCount || !PC || PC->GetPawn() != Pawn)
    {
        if (PC) PC->ClientSetCameraFade(false);
        return;
    }
    bool bMoved = false;
    if (bEntering && !IsInDuct(Pawn) && IsPortalActive(FName(Portals[PortalIndexValue].Id)))
    {
        const FTransform RoomTransform = Pawn->GetActorTransform();
        if (Pawn->SetVentCrawlForced(true))
        {
            FVector Destination;
            if (FindDuctLanding(Pawn, PortalIndexValue, Destination))
            {
                if (UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement())
                    Movement->StopMovementImmediately();
                bMoved = Pawn->TeleportTo(Destination,
                    FRotator(0.f, Portals[PortalIndexValue].DuctYaw, 0.f), false, false);
                if (bMoved)
                    ReturnPoints.Add(WeakPawn, FReturnPoint{ FName(Portals[PortalIndexValue].Id), RoomTransform });
            }
            if (!bMoved) Pawn->SetVentCrawlForced(false);
        }
    }
    else if (!bEntering && IsInDuct(Pawn))
    {
        FVector Destination; FRotator Rotation;
        if (FindRoomLanding(Pawn, PortalIndexValue, Destination, Rotation))
        {
            if (UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement())
                Movement->StopMovementImmediately();
            bMoved = Pawn->TeleportTo(Destination, Rotation, false, false);
            if (bMoved)
            {
                Pawn->SetVentCrawlForced(false);
                ReturnPoints.Remove(WeakPawn);
            }
        }
    }
    if (bMoved)
    {
        PC->SetControlRotation(Pawn->GetActorRotation());
        PC->ClientSetRotation(Pawn->GetActorRotation());
        Pawn->ForceNetUpdate();
    }
    if (PC)
        PC->ClientSetCameraFade(true, FColor::Black, FVector2D(1.f, 0.f), .18f, false, false);
}
