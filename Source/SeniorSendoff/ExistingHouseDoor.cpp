#include "ExistingHouseDoor.h"
#include "HouseReactiveFixture.h"
#include "HouseInteractiveProp.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

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
    PanelCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
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
    // Network-spawned procedural doors receive hinge/panel settings in their
    // initial bunch. Apply those settings before installing player collision.
    OnConstruction(GetActorTransform());
    MotionFromYaw = MotionToYaw = bOpen ? OpenYaw : ClosedYaw;
    MotionElapsed = MotionDuration;
    if (HasAuthority()) CreatePorchGlassFixture();
    UpdateBlockingCollision(bOpen ? OpenYaw : ClosedYaw);
    SetActorTickEnabled(false);
    if (HasAuthority()) PublishMotion();
    else if (Swing.Revision) OnRep_Swing();
}

void AExistingHouseDoor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    MotionElapsed = FMath::Min(MotionElapsed + DeltaSeconds, MotionDuration);
    const float T = FMath::Clamp(MotionElapsed / MotionDuration, 0.f, 1.f);
    const float Eased = T * T * (3.f - 2.f * T);
    const float Requested = FMath::Lerp(MotionFromYaw, MotionToYaw, Eased);
    const float Current = LeafPivot->GetRelativeRotation().Yaw;
    const float Next = ClearSwingYaw(Current, Requested);
    LeafPivot->SetRelativeRotation(FRotator(0.f, Next, 0.f));
    UpdateBlockingCollision(Next);
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(Next, Requested)) > .02f)
    {
        MotionFromYaw = MotionToYaw = Next;
        MotionDuration = MotionElapsed = 0.f;
        SetActorTickEnabled(false);
        if (HasAuthority()) PublishMotion();
    }
    else if (T >= 1.f)
    {
        SetActorTickEnabled(false);
        // A client can stop slightly sooner against a quantized furniture
        // transform. Always send the settled authority pose as a correction.
        MotionFromYaw = MotionToYaw = Next;
        MotionDuration = MotionElapsed = 0.f;
        if (HasAuthority()) PublishMotion();
    }
}

void AExistingHouseDoor::StartMotion()
{
    MotionFromYaw = LeafPivot->GetRelativeRotation().Yaw;
    PrepareSwing();
    // Plan a contact-limited arc before broadcasting it. Recheck each frame
    // too, since players can drag a chair into a door while it is moving.
    const float Target = MotionFromYaw + FMath::FindDeltaAngleDegrees(MotionFromYaw, bOpen ? OpenYaw : ClosedYaw);
    MotionToYaw = ClearSwingYaw(MotionFromYaw, Target);
    MotionElapsed = 0.f;
    MotionDuration = FMath::Max(.20f, .52f * FMath::Abs(MotionToYaw - MotionFromYaw) / 90.f);
    PublishMotion();
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
        ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
    if (Leaf->GetCollisionEnabled() != LeafMode) Leaf->SetCollisionEnabled(LeafMode);
    if (PanelCollision->GetCollisionEnabled() != PanelMode) PanelCollision->SetCollisionEnabled(PanelMode);
}

float AExistingHouseDoor::ServerTime() const
{
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void AExistingHouseDoor::PublishMotion()
{
    if (!HasAuthority()) return;
    Swing.FromYaw = MotionFromYaw;
    Swing.ToYaw = MotionToYaw;
    Swing.StartedAt = ServerTime() - MotionElapsed;
    Swing.Duration = MotionDuration;
    Swing.bOpening = bOpen;
    ++Swing.Revision;
    if (!Swing.Revision) ++Swing.Revision;
    ForceNetUpdate();
}

void AExistingHouseDoor::OnRep_Swing()
{
    bOpen = Swing.bOpening;
    MotionFromYaw = Swing.FromYaw;
    MotionToYaw = Swing.ToYaw;
    MotionDuration = Swing.Duration;
    MotionElapsed = FMath::Clamp(ServerTime() - Swing.StartedAt, 0.f, MotionDuration);
    PrepareSwing();
    if (MotionDuration <= 0.f || MotionElapsed >= MotionDuration)
    {
        LeafPivot->SetRelativeRotation(FRotator(0.f, MotionToYaw, 0.f));
        UpdateBlockingCollision(MotionToYaw);
        SetActorTickEnabled(false);
    }
    else SetActorTickEnabled(true);
}

FBox AExistingHouseDoor::SwingBounds() const
{
    if (PanelHalfExtent.X > 1.f)
        return FBox(PanelCollision->GetRelativeLocation() - PanelCollision->GetUnscaledBoxExtent(),
            PanelCollision->GetRelativeLocation() + PanelCollision->GetUnscaledBoxExtent());
    if (const UStaticMesh* Mesh = Leaf->GetStaticMesh())
        return Mesh->GetBoundingBox().TransformBy(Leaf->GetRelativeTransform());
    return FBox(ForceInit);
}

void AExistingHouseDoor::PrepareSwing()
{
    FittedFrameContacts.Reset();
    const FBox Bounds = SwingBounds();
    if (!Bounds.IsValid) return;
    FTransform Pivot = LeafPivot->GetRelativeTransform();
    Pivot.SetRotation(FRotator(0.f, ClosedYaw, 0.f).Quaternion());
    Pivot = Pivot * Root->GetComponentTransform();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseDoorFittedFrame), false, this);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits, Pivot.TransformPosition(Bounds.GetCenter()),
        Pivot.GetRotation(), Objects, FCollisionShape::MakeBox(Bounds.GetExtent() * Pivot.GetScale3D().GetAbs()), Query);
    for (const FOverlapResult& Hit : Hits)
    {
        // Original door art includes hinges/handles that meet the stationary
        // fitted frame. Do not interpret that authored contact as furniture.
        // Fixed and movable interactive furniture remain blocking alike.
        if (Hit.GetComponent() && !Cast<AHouseInteractiveProp>(Hit.GetActor())
            && !Cast<AExistingHouseDoor>(Hit.GetActor()))
            FittedFrameContacts.AddUnique(Hit.GetComponent());
    }
}

bool AExistingHouseDoor::IsSwingBlocked(float Yaw, const FBox& Bounds) const
{
    FTransform Pivot = LeafPivot->GetRelativeTransform();
    Pivot.SetRotation(FRotator(0.f, Yaw, 0.f).Quaternion());
    Pivot = Pivot * Root->GetComponentTransform();
    FVector Extent = Bounds.GetExtent() * Pivot.GetScale3D().GetAbs();
    // Small frame/floor clearance keeps hinge hardware and door jambs from
    // stopping a correctly fitted leaf. There is no enlarged furniture buffer.
    Extent.X = FMath::Max(.5f, Extent.X - 1.f);
    Extent.Y = FMath::Max(.5f, Extent.Y - 1.f);
    Extent.Z = FMath::Max(.5f, Extent.Z - 1.5f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseDoorSwing), false, this);
    for (const auto& Contact : FittedFrameContacts)
        if (Contact.IsValid()) Query.AddIgnoredComponent(Contact.Get());
    Query.AddIgnoredActor(PorchGlassFixture);
    TArray<AActor*> Attached;
    GetAttachedActors(Attached, true, true);
    for (AActor* Actor : Attached) Query.AddIgnoredActor(Actor);
    FCollisionObjectQueryParams Objects;
    for (ECollisionChannel Channel : {ECC_WorldStatic, ECC_WorldDynamic, ECC_PhysicsBody, ECC_Pawn})
        Objects.AddObjectTypesToQuery(Channel);
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits, Pivot.TransformPosition(Bounds.GetCenter()),
        Pivot.GetRotation(), Objects, FCollisionShape::MakeBox(Extent), Query);
    for (const FOverlapResult& Hit : Hits)
    {
        const UPrimitiveComponent* Component = Hit.GetComponent();
        const AActor* Actor = Hit.GetActor();
        if (!Component || !Actor || Actor == this || Actor->GetOwner() == this) continue;
        // Static architecture retains the door's authored travel. Imported
        // wall/floor buffers must not turn this furniture check into new room
        // barriers. All fixed and movable house furniture uses interactive
        // prop bodies and is checked even when it cannot be picked up.
        if (Component->GetCollisionObjectType() == ECC_WorldStatic
            && !Cast<AHouseInteractiveProp>(Actor) && !Cast<AExistingHouseDoor>(Actor)
            && !Cast<APawn>(Actor) && !Component->IsSimulatingPhysics()) continue;
        // Paired leaves have a shared seam; each still checks the furniture on
        // its own side. Approach triggers, effects and cosmetic cloth are ignored.
        if (const AExistingHouseDoor* Door = Cast<AExistingHouseDoor>(Actor);
            Door && !DoorGroup.IsNone() && Door->DoorGroup == DoorGroup) continue;
        if (Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
            || Component->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Block)
        {
#if !UE_BUILD_SHIPPING
            static const bool bReview = FParse::Param(FCommandLine::Get(), TEXT("DoorSwingAudit"))
                || FParse::Param(FCommandLine::Get(), TEXT("DoorSwingReview"));
            if (bReview) UE_LOG(LogTemp, Display, TEXT("DOOR_SWING_CONTACT %s yaw=%.2f actor=%s component=%s"),
                *DoorId.ToString(), Yaw, *GetNameSafe(Actor), *GetNameSafe(Component));
#endif
            return true;
        }
    }
    return false;
}

float AExistingHouseDoor::ClearSwingYaw(float From, float To) const
{
    const FBox Bounds = SwingBounds();
    const float Delta = FMath::FindDeltaAngleDegrees(From, To);
    if (!Bounds.IsValid || FMath::Abs(Delta) < .01f) return To;
    const float Radius = (Bounds.GetCenter().Size2D() + Bounds.GetExtent().Size2D())
        * GetActorScale3D().GetAbs().GetMax();
    // At most half a degree and one centimetre of tip travel per query; even a
    // slow frame checks the whole arc instead of jumping over a narrow chair leg.
    const float Step = FMath::Min(.5f, FMath::RadiansToDegrees(1.f / FMath::Max(1.f, Radius)));
    const int32 Count = FMath::CeilToInt(FMath::Abs(Delta) / Step);
    float LastClear = From;
    for (int32 Index = 1; Index <= Count; ++Index)
    {
        const float Yaw = From + Delta * (float(Index) / Count);
        if (IsSwingBlocked(Yaw, Bounds)) return LastClear;
        LastClear = Yaw;
    }
    return From + Delta;
}

void AExistingHouseDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AExistingHouseDoor, Swing);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, DoorwayCenter, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, DoorId, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, ClosedYaw, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, OpenYaw, COND_InitialOnly);
    DOREPLIFETIME(AExistingHouseDoor, bLocked);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, DoorGroup, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, PanelCenterWorld, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, PanelHalfExtent, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, bLocalLeafSpace, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AExistingHouseDoor, SourceGeometryOriginWorld, COND_InitialOnly);
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
