#include "ExistingHouseDoor.h"
#include "HouseReactiveFixture.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"

AExistingHouseDoor::AExistingHouseDoor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
    RootComponent = Root;
    LeafPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeafPivot"));
    LeafPivot->SetupAttachment(Root);
    Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OriginalDoorLeaf"));
    Leaf->SetupAttachment(LeafPivot);
    Leaf->SetCollisionProfileName(TEXT("BlockAll"));
    Glass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OriginalDoorGlass"));
    Glass->SetupAttachment(LeafPivot);
    Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AddedHandle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AddedDoorHandle"));
    AddedHandle->SetupAttachment(LeafPivot);
    AddedHandle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PanelCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("MovingPanelCollision"));
    PanelCollision->SetupAttachment(LeafPivot);
    PanelCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PanelCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    PanelCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Approach = CreateDefaultSubobject<UBoxComponent>(TEXT("Approach"));
    Approach->SetupAttachment(Root);
    // Kept as a serialized component for existing map instances, but it does
    // not react to players. A door moves only after deliberate interaction.
    Approach->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Approach->SetCollisionResponseToAllChannels(ECR_Ignore);
    Approach->SetGenerateOverlapEvents(false);
}

void AExistingHouseDoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Leaf->SetStaticMesh(OriginalLeaf);
    Glass->SetStaticMesh(OriginalGlass);
    AddedHandle->SetStaticMesh(AddedHandleMesh);
    const FTransform HingeTransform = GetActorTransform();
    if (bLocalLeafSpace)
    {
        Leaf->SetRelativeTransform(FTransform::Identity);
        Glass->SetRelativeTransform(FTransform::Identity);
        AddedHandle->SetRelativeLocation(AddedHandleWorldPosition);
        AddedHandle->SetRelativeRotation(AddedHandleRotation);
        PanelCollision->SetBoxExtent(PanelHalfExtent.ComponentMax(FVector(1.f)));
        PanelCollision->SetRelativeLocation(PanelCenterWorld);
    }
    else
    {
        // Extracted FBX vertices retain the source mesh's original world
        // coordinates. Convert its world anchor through the hinge transform so
        // enlarging the actor does not apply the moved hinge offset twice.
        const FVector SourceOriginLocal = HingeTransform.InverseTransformPosition(SourceGeometryOriginWorld);
        Leaf->SetRelativeScale3D(FVector(1.8f, 1.8f, 1.f));
        Leaf->SetRelativeLocation(SourceOriginLocal);
        Glass->SetRelativeScale3D(FVector(1.8f, 1.8f, 1.f));
        Glass->SetRelativeLocation(SourceOriginLocal);
        AddedHandle->SetRelativeLocation(HingeTransform.InverseTransformPosition(AddedHandleWorldPosition));
        AddedHandle->SetRelativeRotation(AddedHandleRotation);
        // Extents are hinge-local lengths; the root already supplies the scale.
        PanelCollision->SetBoxExtent(PanelHalfExtent.ComponentMax(FVector(1.f)));
        PanelCollision->SetRelativeLocation(HingeTransform.InverseTransformPosition(PanelCenterWorld));
    }
    LeafPivot->SetRelativeRotation(FRotator(0.f, bOpen ? OpenYaw : ClosedYaw, 0.f));
    Approach->SetRelativeLocation(HingeTransform.InverseTransformPosition(DoorwayCenter));
    Approach->SetBoxExtent(DoorGroup.IsNone() ? FVector(125.f, 125.f, 115.f)
        : FVector(165.f, 350.f, 115.f));
    UpdateBlockingCollision(bOpen ? OpenYaw : ClosedYaw);
}

void AExistingHouseDoor::BeginPlay()
{
    Super::BeginPlay();
    MotionFromYaw = MotionToYaw = bOpen ? OpenYaw : ClosedYaw;
    MotionElapsed = MotionDuration;
    if (HasAuthority()) CreatePorchGlassFixture();
    UpdateBlockingCollision(bOpen ? OpenYaw : ClosedYaw);
    SetActorTickEnabled(false);
}

void AExistingHouseDoor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    MotionElapsed = FMath::Min(MotionElapsed + DeltaSeconds, MotionDuration);
    const float T = FMath::Clamp(MotionElapsed / MotionDuration, 0.f, 1.f);
    const float Eased = T * T * (3.f - 2.f * T);
    const float Next = FMath::Lerp(MotionFromYaw, MotionToYaw, Eased);
    LeafPivot->SetRelativeRotation(FRotator(0.f, Next, 0.f));
    UpdateBlockingCollision(Next);
    if (T >= 1.f) SetActorTickEnabled(false);
}

void AExistingHouseDoor::StartMotion()
{
    MotionFromYaw = LeafPivot->GetRelativeRotation().Yaw;
    MotionToYaw = bOpen ? OpenYaw : ClosedYaw;
    MotionElapsed = 0.f;
    MotionDuration = FMath::Max(.20f, .52f * FMath::Abs(MotionToYaw - MotionFromYaw) / 90.f);
    if (FMath::IsNearlyEqual(MotionFromYaw, MotionToYaw, .01f))
    {
        UpdateBlockingCollision(MotionToYaw);
        SetActorTickEnabled(false);
        return;
    }
    SetActorTickEnabled(true);
}

void AExistingHouseDoor::UpdateBlockingCollision(float CurrentYaw)
{
    // Keep the moving leaf solid throughout its swing and when fully open.
    // Once it is beside the frame it no longer covers the passage, but the
    // player must still be unable to walk through the visible panel.
    const bool bHasPanel = PanelHalfExtent.X > 1.f;
    const bool bGlassBroken = PorchGlassFixture && PorchGlassFixture->IsBroken();
    if (bGlassBroken && !bComplexFrameReady && OriginalLeaf)
    {
        if (!BrokenFrameMesh)
        {
            const FString Name = OriginalLeaf->GetName().Replace(TEXT("SM_OriginalPorchFrame_"), TEXT("HI_BrokenPorchFrame_"));
            BrokenFrameMesh = LoadObject<UStaticMesh>(nullptr,
                *FString::Printf(TEXT("/Game/SeniorSendOff/House/Interactivity/%s.%s"),*Name,*Name));
        }
        if (BrokenFrameMesh && Leaf->GetStaticMesh()!=BrokenFrameMesh) Leaf->SetStaticMesh(BrokenFrameMesh);
        // The imported frame has actual muntins and an open centre. A convex
        // hull around it would leave a solid invisible sheet after shattering.
        // Only this runtime body setup is changed; no mesh asset is saved.
        if (UBodySetup* Setup = Leaf->GetStaticMesh()->GetBodySetup())
        {
            Setup->CollisionTraceFlag = CTF_UseComplexAsSimple;
            Leaf->RecreatePhysicsState();
            bComplexFrameReady = true;
        }
    }
    const ECollisionEnabled::Type LeafMode = (!bHasPanel || bGlassBroken)
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
    const ECollisionEnabled::Type PanelMode = (bHasPanel && !bGlassBroken)
        ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
    if (Leaf->GetCollisionEnabled() != LeafMode) Leaf->SetCollisionEnabled(LeafMode);
    if (PanelCollision->GetCollisionEnabled() != PanelMode) PanelCollision->SetCollisionEnabled(PanelMode);
}

void AExistingHouseDoor::OnRep_Open()
{
    StartMotion();
}

void AExistingHouseDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AExistingHouseDoor, bOpen);
    DOREPLIFETIME(AExistingHouseDoor, PorchGlassFixture);
}

void AExistingHouseDoor::CreatePorchGlassFixture()
{
    if (!HasAuthority() || PorchGlassFixture || !OriginalGlass || PanelHalfExtent.X <= 1.f) return;
    const FTransform AnchorTransform = LeafPivot->GetComponentTransform();
    AHouseReactiveFixture* Fixture = GetWorld()->SpawnActorDeferred<AHouseReactiveFixture>(
        AHouseReactiveFixture::StaticClass(), AnchorTransform, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Fixture) return;
    Fixture->FixtureKind = EHouseReactiveFixtureKind::Window;
    Fixture->FixtureId = FName(*(DoorId.ToString() + TEXT("_PorchGlass")));
    Fixture->OriginalMesh = OriginalGlass;
    Fixture->MeshTransform = Glass->GetRelativeTransform();
    Fixture->MotionPivotLocal = FVector::ZeroVector;
    for (int32 Index = 0; Index < Glass->GetNumMaterials(); ++Index)
        Fixture->MaterialOverrides.Add(Glass->GetMaterial(Index));
    Fixture->FinishSpawning(AnchorTransform);
    Fixture->AttachToComponent(LeafPivot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Fixture->SetActorRelativeTransform(FTransform::Identity);
    PorchGlassFixture = Fixture;
    OnRep_PorchGlassFixture();
    ForceNetUpdate();
    Fixture->ForceNetUpdate();
}

void AExistingHouseDoor::OnRep_PorchGlassFixture()
{
    if (!PorchGlassFixture) return;
    Glass->SetVisibility(false);
    Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PorchGlassFixture->OnFixtureBroken.RemoveAll(this);
    PorchGlassFixture->OnFixtureBroken.AddUObject(this, &AExistingHouseDoor::OnPorchGlassBroken);
    if (PorchGlassFixture->IsBroken()) OnPorchGlassBroken(PorchGlassFixture);
}

void AExistingHouseDoor::OnPorchGlassBroken(AHouseReactiveFixture* Fixture)
{
    if (Fixture == PorchGlassFixture) UpdateBlockingCollision(LeafPivot->GetRelativeRotation().Yaw);
}

bool AExistingHouseDoor::IsDoorwayOccupied() const
{
    FVector Along = DoorwayCenter - GetActorLocation();
    Along.Z = 0.f;
    const float HalfWidth = FMath::Max(45.f, Along.Size());
    Along.Normalize();
    const FVector Normal(-Along.Y, Along.X, 0.f);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It)) continue;
        const ACharacter* Character = Cast<ACharacter>(*It);
        const float Radius = Character && Character->GetCapsuleComponent()
            ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.f;
        const FVector Delta = It->GetActorLocation() - DoorwayCenter;
        if (FMath::Abs(Delta.Z) > 130.f) continue;
        // Refuse to swing shut across a player standing in the opening. This
        // allows closing from either side once they step clear of the frame.
        if (FMath::Abs(FVector::DotProduct(Delta, Normal)) < Radius + 20.f
            && FMath::Abs(FVector::DotProduct(Delta, Along)) < HalfWidth + Radius + 10.f)
            return true;
    }
    return false;
}

bool AExistingHouseDoor::Interact(APawn* Player)
{
    if (!HasAuthority() || bLocked || !Player) return false;
    if (FVector::Dist(Player->GetActorLocation(), DoorwayCenter) > 270.f) return false;
    if (bOpen && IsDoorwayOccupied()) return false;
    SetGroupOpen(!bOpen);
    return true;
}

void AExistingHouseDoor::SetGroupOpen(bool bShouldOpen)
{
    if (DoorGroup.IsNone())
    {
        if (bOpen == bShouldOpen) return;
        bOpen = bShouldOpen;
        StartMotion();
        ForceNetUpdate();
        return;
    }
    for (TActorIterator<AExistingHouseDoor> It(GetWorld()); It; ++It)
        if (It->DoorGroup == DoorGroup && !It->bLocked)
        {
            if (It->bOpen == bShouldOpen) continue;
            It->bOpen = bShouldOpen;
            It->StartMotion();
            It->ForceNetUpdate();
        }
}
