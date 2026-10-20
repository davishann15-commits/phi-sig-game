#include "HouseInteractiveProp.h"
#include "HouseDamageSubsystem.h"
#include "HouseFurnitureInteractionComponent.h"
#include "HouseBreakDebris.h"
#include "HouseClothFlag.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInterface.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
    const FName MovableTag(TEXT("SSOMovableFurniture"));
    const FName HeldTag(TEXT("SSOFurnitureHeld"));
    const FName BreakableTag(TEXT("HouseBreakable"));
    float PropMassResponse(float MassKg)
    {
        return FMath::Clamp(FMath::Sqrt(15.f / FMath::Max(.1f, MassKg)), .22f, 1.2f);
    }

    bool IsGreatRoomSofa(FName Id)
    {
        return Id == TEXT("SM_GR3_NearSeating__001") || Id == TEXT("SM_GR3_NearSeating__003")
            || Id == TEXT("SM_GR3_FarSeating__001") || Id == TEXT("SM_GR3_FarSeating__002")
            || Id == TEXT("SM_GR3_FarSeating__003");
    }

    bool IsAddedDamageMaterial(const FStaticMaterial& Material)
    {
        return Material.ImportedMaterialSlotName.ToString().StartsWith(TEXT("M_HI_"))
            || Material.MaterialSlotName.ToString().StartsWith(TEXT("M_HI_"));
    }

    FName PropMaterialIdentity(FName Name)
    {
        FString Base, Suffix;
        if (!Name.ToString().Split(TEXT("."), &Base, &Suffix, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
            || Suffix.IsEmpty()) return Name;
        for (const TCHAR Character : Suffix) if (!FChar::IsDigit(Character)) return Name;
        return FName(*Base);
    }

    int32 PropOverrideSlot(const UStaticMesh* Intact, const UStaticMesh* Desired, int32 OriginalIndex)
    {
        if (!Intact || !Desired || !Intact->GetStaticMaterials().IsValidIndex(OriginalIndex)) return INDEX_NONE;
        const FStaticMaterial& Original = Intact->GetStaticMaterials()[OriginalIndex];
        const TArray<FStaticMaterial>& Slots = Desired->GetStaticMaterials();
        if (Intact == Desired) return OriginalIndex;
        // FBX import drops unused slots independently for every damage stage.
        // Preserve imported identities first; render slot names support assets
        // whose importer did not retain the original imported name.
        for (const FName Name : {Original.ImportedMaterialSlotName, Original.MaterialSlotName})
        {
            if (Name.IsNone()) continue;
            for (int32 Index = 0; Index < Slots.Num(); ++Index)
                if (!IsAddedDamageMaterial(Slots[Index]) && Slots[Index].ImportedMaterialSlotName == Name) return Index;
            for (int32 Index = 0; Index < Slots.Num(); ++Index)
                if (!IsAddedDamageMaterial(Slots[Index]) && Slots[Index].MaterialSlotName == Name) return Index;
        }
        // Blender can add a numeric duplicate suffix to the same material.
        // Exact names above win; normalize only that terminal suffix here.
        for (const FName Name : {Original.ImportedMaterialSlotName, Original.MaterialSlotName})
        {
            if (Name.IsNone()) continue;
            const FName Identity = PropMaterialIdentity(Name);
            for (int32 Index = 0; Index < Slots.Num(); ++Index)
                if (!IsAddedDamageMaterial(Slots[Index])
                    && (PropMaterialIdentity(Slots[Index].ImportedMaterialSlotName) == Identity
                        || PropMaterialIdentity(Slots[Index].MaterialSlotName) == Identity)) return Index;
        }
        // Anonymous legacy meshes and the plain engine smoke-test shapes have
        // no cross-stage material identities. Never apply this fallback to a
        // named imported slot or to newly exposed foam/splinter/core material.
        if (Slots.IsValidIndex(OriginalIndex) && !IsAddedDamageMaterial(Slots[OriginalIndex]))
        {
            const FStaticMaterial& Candidate = Slots[OriginalIndex];
            const bool bAnonymous = Original.ImportedMaterialSlotName.IsNone() && Original.MaterialSlotName.IsNone()
                && Candidate.ImportedMaterialSlotName.IsNone() && Candidate.MaterialSlotName.IsNone()
                && Slots.Num() == Intact->GetStaticMaterials().Num();
            const bool bEngineShapes = Intact->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"))
                && Desired->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"));
            if (bAnonymous || bEngineShapes) return OriginalIndex;
        }
        return INDEX_NONE;
    }
}

AHouseInteractiveProp::AHouseInteractiveProp()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 30.f;
    MinNetUpdateFrequency = 5.f;
    SetCanBeDamaged(true);

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetGenerateOverlapEvents(false);
    Body->SetNotifyRigidBodyCollision(true);
    Body->SetSimulatePhysics(false);
    Body->SetLinearDamping(.45f);
    Body->SetAngularDamping(1.25f);
    Body->BodyInstance.bAutoWeld = false;
    Body->BodyInstance.bGenerateWakeEvents = true;
    Tags.AddUnique(MovableTag);
    Tags.AddUnique(BreakableTag);
    BreakDebris = CreateDefaultSubobject<UHouseBreakDebrisComponent>(TEXT("BreakDebris"));
    ClothFlag = CreateDefaultSubobject<UHouseClothFlagComponent>(TEXT("ClothFlag"));
}

void AHouseInteractiveProp::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshProp();
}

void AHouseInteractiveProp::RefreshProp()
{
    if (!bBroken) OriginalScale = GetActorScale3D();
    if (!HasActorBegunPlay())
    {
        Health = FMath::Max(.1f, BreakHealth);
        AttachmentHealth = FMath::Max(.1f, DetachHealth);
    }
    ApplyAppearance();
    if (HasActorBegunPlay()) ApplyPhysicsState();
}

void AHouseInteractiveProp::BeginPlay()
{
    Super::BeginPlay();
    if (bPhysicsActive && FParse::Param(FCommandLine::Get(), TEXT("HouseInteractivitySmoke")))
        UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_BEGIN_ACTIVE id=%s sim=%d"), *PropId.ToString(), Body->IsSimulatingPhysics());
    if (!bBroken) OriginalScale = GetActorScale3D();   // a piece already broken (a late join) is crushed: keep the intact scale
    if (HasAuthority())
    {
        Health = bBroken ? 0.f : FMath::Max(.1f, BreakHealth);
        AttachmentHealth = bDetachedFromWall ? 0.f : FMath::Max(.1f, DetachHealth);
    }
    Body->OnComponentHit.AddDynamic(this, &AHouseInteractiveProp::OnBodyHit);
    Body->OnComponentWake.AddDynamic(this, &AHouseInteractiveProp::OnBodyWake);
    {
        FString SfxNames = PropName + TEXT(" ") + PropId.ToString();
        if (IntactMesh)
        {
            SfxNames += TEXT(" ") + IntactMesh->GetName();
            for (const FStaticMaterial& M : IntactMesh->GetStaticMaterials())
            {
                SfxNames += TEXT(" ") + M.MaterialSlotName.ToString();
                if (M.MaterialInterface) SfxNames += TEXT(" ") + M.MaterialInterface->GetName();
            }
        }
        for (const TObjectPtr<UMaterialInterface>& O : MaterialOverrides) if (O) SfxNames += TEXT(" ") + O->GetName();
        SfxSurface = HouseSfx::SurfaceFor(SfxNames);
    }
    Body->OnComponentSleep.AddDynamic(this, &AHouseInteractiveProp::OnBodySleep);
    ApplyAppearance();
    if (ShouldUseCloth())
    {
        // Flags hang as cloth. A dedicated server has no visuals but keeps the
        // same rules, so every machine agrees the piece is cloth.
        const bool bVisual = ClothFlag && ClothFlag->Initialize(Body, !bWallMounted);
        bClothMode = bVisual || GetNetMode() == NM_DedicatedServer;
        if (bClothMode)
        {
            bPhysicsActive = false;
            if (bVisual && (bDetachedFromWall || bBroken)) ClothFlag->Release(Body->Bounds.Origin, true);
        }
        UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_CLOTH id=%s mesh=%s active=%d particles=%d"), *PropId.ToString(),
            *GetNameSafe(IntactMesh), bClothMode ? 1 : 0, ClothFlag ? ClothFlag->GetParticleCount() : 0);
    }
    ApplyPhysicsState();
}

void AHouseInteractiveProp::SetCasterUprightLock(bool bLock)
{
    if (!Body || bCasterUprightLock == bLock) return;
    bCasterUprightLock = bLock;
    FBodyInstance* Instance = Body->GetBodyInstance();
    if (!Instance) return;
    const FVector Linear = Body->GetPhysicsLinearVelocity();
    const FVector Angular = Body->GetPhysicsAngularVelocityInRadians();
    if (bLock)
    {
        // Snap the last few degrees upright so the lock holds a level chair.
        const FRotator Level(0.f, GetActorRotation().Yaw, 0.f);
        Body->SetWorldRotation(Level, false, nullptr, ETeleportType::TeleportPhysics);
    }
    Instance->bLockXRotation = bLock;
    Instance->bLockYRotation = bLock;
    Instance->bLockZRotation = false;
    Instance->bLockXTranslation = Instance->bLockYTranslation = Instance->bLockZTranslation = false;
    Instance->SetDOFLock(bLock ? EDOFMode::SixDOF : EDOFMode::None);
    Body->SetPhysicsLinearVelocity(Linear);
    Body->SetPhysicsAngularVelocityInRadians(bLock ? FVector(0.f, 0.f, Angular.Z) : Angular);
    Body->WakeAllRigidBodies();
}

bool AHouseInteractiveProp::IsRollingChair() const
{
    if (bRollingChair) return true;
    if (bBroken || !IntactMesh) return false;
    const FString Name = (IntactMesh->GetName() + TEXT(" ") + PropName).ToLower();
    return Name.Contains(TEXT("officechair")) || Name.Contains(TEXT("office chair")) || Name.Contains(TEXT("rolling chair"))
        || Name.Contains(TEXT("swivel")) || Name.Contains(TEXT("gamingchair"));
}

bool AHouseInteractiveProp::ShouldUseCloth() const
{
    if (bClothFlag) return true;
    if (!IntactMesh) return false;
    const FString Text = (PropName + TEXT(" ") + PropId.ToString() + TEXT(" ") + IntactMesh->GetName()).ToLower();
    bool bNamed = false;
    for (const TCHAR* Word : {TEXT("flag"), TEXT("banner"), TEXT("pennant"), TEXT("tapestry")}) bNamed |= Text.Contains(Word);
    // Generated house flags carry their design in material names instead.
    for (const FStaticMaterial& Material : IntactMesh->GetStaticMaterials())
    {
        const FString Name = GetNameSafe(Material.MaterialInterface).ToLower();
        bNamed |= Name.Contains(TEXT("colorado")) || Name.Contains(TEXT("rangers")) || Name.Contains(TEXT("pennant")) || Name.Contains(TEXT("banner"));
    }
    bNamed |= Text.Contains(TEXT("smf_upper_all_039")); // stars-and-stripes flag over the great room balcony
    bNamed |= Text.Contains(TEXT("smf_upper_all_038")); // Tennessee tri-star flag beside it
    if (!bNamed) return false;
    // Only thin sheets: never a flag-printed mug or cooler.
    const FVector Size = IntactMesh->GetBoundingBox().GetSize() * GetActorScale3D().GetAbs();
    return Size.GetMin() < FMath::Max(14.f, Size.GetMax() * .12f) && Size.GetMax() > 15.f;
}

void AHouseInteractiveProp::TearCloth(const FVector& PullFrom, AController* Instigator, bool bCountAsBroken)
{
    if (!HasAuthority()) return;
    const bool bFirst = !bDetachedFromWall && !bBroken;
    bDetachedFromWall = true;
    AttachmentHealth = 0.f;
    if (bCountAsBroken) { bBroken = true; DamageStage = 3; Health = 0.f; ++BreakEvents; }
    if (bFirst)
    {
        MulticastClothRelease(PullFrom);
        if (UHouseDamageSubsystem* Damage = GetWorld()->GetSubsystem<UHouseDamageSubsystem>())
            Damage->RegisterBrokenActor(this, DamageCategory, Instigator);
    }
    ApplyPhysicsState();
    FlushNetDormancy();
    ForceNetUpdate();
}

void AHouseInteractiveProp::MulticastClothRelease_Implementation(FVector_NetQuantize PullFrom)
{
    if (ClothFlag && ClothFlag->IsActive()) ClothFlag->Release(PullFrom, false);
    if (Body && bClothMode) Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AHouseInteractiveProp::ClearIncomingVelocity()
{
    bIncomingVelocityValid = false;
    IncomingVelocity = FVector::ZeroVector;
}

bool AHouseInteractiveProp::HasFreshIncomingVelocity() const
{
    // A held object's last spring velocity must not be reused after release.
    // Frame age also rejects a sample retained across sleeping or a long pause.
    return bIncomingVelocityValid && !bIncomingVelocityWasHeld
        && GFrameCounter >= IncomingVelocityFrame && GFrameCounter - IncomingVelocityFrame <= 1;
}

void AHouseInteractiveProp::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !bPhysicsActive || !Body->IsSimulatingPhysics() || !Body->RigidBodyIsAwake())
    {
        ClearIncomingVelocity();
        SetActorTickEnabled(false);
        return;
    }
    // Chaos reports OnComponentHit after solving contact, when the body may
    // already have stopped or bounced. Keep the incoming speed for this step.
    if (IsRollingChair())
    {
        // On the floor and upright, a five-star base rolls and swivels but does
        // not tip. Carried, thrown or knocked flying, it tumbles freely.
        const float Lean = FMath::Acos(FMath::Clamp(Body->GetUpVector().Z, -1.f, 1.f));
        const float Vertical = FMath::Abs(Body->GetPhysicsLinearVelocity().Z);
        const bool bHeld = Body->ComponentHasTag(HeldTag);
        if (!bCasterUprightLock && !bHeld && Vertical < 40.f && Lean < .12f) SetCasterUprightLock(true);
        else if (bCasterUprightLock && (bHeld || Vertical > 160.f)) SetCasterUprightLock(false);
        if (bCasterUprightLock && !bHeld)
        {
            // A shoved office chair tops out around a brisk jog.
            const FVector Velocity = Body->GetPhysicsLinearVelocity();
            const FVector Flat(Velocity.X, Velocity.Y, 0.f);
            if (Flat.SizeSquared() > FMath::Square(620.f))
                Body->SetPhysicsLinearVelocity(Flat.GetClampedToMaxSize(620.f) + FVector(0, 0, Velocity.Z));
        }
    }
    if (IsRollingChair() && !bCasterUprightLock && !Body->ComponentHasTag(HeldTag) && FMath::Abs(Body->GetPhysicsLinearVelocity().Z) < 80.f)
    {
        // A five-star base resists tipping: damp roll/pitch spin and ease back
        // upright from a small tilt, but leave the swivel (yaw) free.
        const FVector Up = Body->GetUpVector();
        const float Tilt = FMath::Acos(FMath::Clamp(Up.Z, -1.f, 1.f));
        FVector Spin = Body->GetPhysicsAngularVelocityInRadians();
        Spin -= FVector(Spin.X, Spin.Y, 0.f) * FMath::Min(1.f, 6.f * DeltaSeconds);
        if (Tilt > .02f && Tilt < .6f)
            Spin += FVector::CrossProduct(Up, FVector::UpVector).GetSafeNormal() * Tilt * 9.f * DeltaSeconds;
        Body->SetPhysicsAngularVelocityInRadians(Spin);
    }
    IncomingVelocity = Body->GetPhysicsLinearVelocity();
    IncomingVelocityFrame = GFrameCounter;
    bIncomingVelocityValid = !IncomingVelocity.ContainsNaN();
    bIncomingVelocityWasHeld = Body->ComponentHasTag(HeldTag);
}

void AHouseInteractiveProp::OnBodyWake(UPrimitiveComponent* WakingComponent, FName BoneName)
{
    ClearIncomingVelocity();
    if (WakingComponent == Body && HasAuthority() && bPhysicsActive)
        SetActorTickEnabled(true);
}

void AHouseInteractiveProp::OnBodySleep(UPrimitiveComponent* SleepingComponent, FName BoneName)
{
    if (SleepingComponent != Body) return;
    ClearIncomingVelocity();
    SetActorTickEnabled(false);
}

void AHouseInteractiveProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AHouseInteractiveProp, bBroken);
    DOREPLIFETIME(AHouseInteractiveProp, DamageStage);
    DOREPLIFETIME(AHouseInteractiveProp, bPhysicsActive);
    DOREPLIFETIME(AHouseInteractiveProp, bDetachedFromWall);
    DOREPLIFETIME(AHouseInteractiveProp, Health);
    DOREPLIFETIME(AHouseInteractiveProp, AttachmentHealth);
}

bool AHouseInteractiveProp::CanBeMoved() const
{
    return bWallMounted || bDetachedFromWall || (bBroken ? bBrokenVersionMovable : bMovable);
}

FString AHouseInteractiveProp::GetInteractionLabel() const
{
    if (bBroken) return FString::Printf(TEXT("broken %s"), *PropName);
    if (DamageStage >= 2) return FString::Printf(TEXT("badly damaged %s"), *PropName);
    if (DamageStage == 1) return FString::Printf(TEXT("damaged %s"), *PropName);
    return PropName;
}

UStaticMesh* AHouseInteractiveProp::MeshForDamageStage(int32 Stage) const
{
    if (Stage >= 3 && BrokenMesh) return BrokenMesh;
    if (Stage >= 2 && HeavilyDamagedMesh) return HeavilyDamagedMesh;
    if (Stage >= 1 && DamagedMesh) return DamagedMesh;
    return IntactMesh;
}

void AHouseInteractiveProp::UpdateDamageStage(AController* Instigator)
{
    const float Fraction = Health / FMath::Max(.1f, BreakHealth);
    const int32 NewStage = bBroken ? 3 : Fraction <= 1.f / 3.f ? 2 : Fraction <= 2.f / 3.f ? 1 : 0;
    if (NewStage == DamageStage) return;
    const bool bSwap = Body->GetStaticMesh() != MeshForDamageStage(NewStage);
    const bool bWasSimulating = Body->IsSimulatingPhysics();
    const FVector Velocity = bWasSimulating ? Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector Angular = bWasSimulating ? Body->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;
    if (bSwap)
    {
        // Contents still see the old top surface during their support check.
        // These sofas' loose cloth pillows overlap the upholstered surface in
        // their authored rest pose. Do not release them just because a crack
        // mesh was swapped in; the sofa has not moved yet. They still release
        // when the player moves the couch or when it actually breaks.
        if (!IsGreatRoomSofa(PropId)) WakeSupportedProps(Instigator ? Instigator->GetPawn() : nullptr);
        ReleaseBodyHolders();
    }
    // A released constraint must not leave the newly created rigid body parked
    // in midair. This is also the first activation of previously untouched props.
    if (CanBeMoved() && (!bWallMounted || bDetachedFromWall)) bPhysicsActive = true;
    DamageStage = NewStage;
    ApplyAppearance();
    ApplyPhysicsState();
    RestoreReleasedMotion(Velocity, Angular);
    if (bSwap) LastImpactTime = GetWorld()->GetTimeSeconds();
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_DAMAGE_STAGE id=%s stage=%d health=%.1f mesh=%s"),
        *PropId.ToString(), DamageStage, Health, *GetNameSafe(Body->GetStaticMesh()));
}

void AHouseInteractiveProp::ApplyAppearance()
{
    if (!Body) return;
    UStaticMesh* Desired = MeshForDamageStage(bBroken ? 3 : DamageStage);
    if (Desired && Body->GetStaticMesh() != Desired)
    {
        ClearIncomingVelocity();
        // Recreate an unconstrained, non-simulating body, then explicitly start
        // the new collision shape in ApplyPhysicsState. SetStaticMesh otherwise
        // copies the old solver state while the handle/contact shape disappears.
        if (Body->IsSimulatingPhysics()) Body->SetSimulatePhysics(false);
        Body->SetStaticMesh(Desired);
    }
    // SetStaticMesh keeps the component's positional override array. Clear it
    // before remapping so a removed seam slot cannot overwrite exposed filling.
    Body->EmptyOverrideMaterials();
    for (int32 OriginalSlot = 0; OriginalSlot < MaterialOverrides.Num(); ++OriginalSlot)
        if (MaterialOverrides[OriginalSlot])
        {
            const int32 DesiredSlot = PropOverrideSlot(IntactMesh, Desired, OriginalSlot);
            if (DesiredSlot != INDEX_NONE) Body->SetMaterial(DesiredSlot, MaterialOverrides[OriginalSlot]);
        }

    if (CanBeMoved()) Tags.AddUnique(MovableTag);
    else Tags.Remove(MovableTag);
    const bool bDamageable = (bBreakable && !bBroken) || (bWallMounted && !bDetachedFromWall);
    if (bDamageable) Tags.AddUnique(BreakableTag);
    else Tags.Remove(BreakableTag);
    SetCanBeDamaged(bDamageable);

    // Imported broken meshes preserve their material slots and silhouette, with
    // individual original parts snapped or collapsed offline. This emergency
    // fallback keeps an unconfigured asset visibly crushed, never invisible.
    if (bBroken && !BrokenMesh)
        Body->SetWorldScale3D(OriginalScale * FVector(1.f, .94f, .38f));
}

void AHouseInteractiveProp::ApplyPhysicsState()
{
    if (!Body || !Body->GetStaticMesh()) return;
    const bool bShouldSimulate = bPhysicsActive && CanBeMoved() && !bClothMode;
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetCollisionObjectType(ECC_WorldDynamic);
    Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    if (bBroken || ActorHasTag(TEXT("SSOLibrarySmallItem")) || ActorHasTag(TEXT("SSOBoywickSmallItem")))
    {
        // Loose sheets and chips remain targetable and collide with furniture
        // and floors, without snagging player or animal capsules.
        Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Body->SetCanEverAffectNavigation(false);
    }
    if (bClothMode)
    {
        // Players brush through cloth; hits and throws still find it until it is torn down.
        Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        if (bDetachedFromWall || bBroken) Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    Body->SetEnableGravity(true);
    if (bShouldSimulate)
    {
        const UBodySetup* Collision = Body->GetStaticMesh()->GetBodySetup();
        if (!Collision || Collision->CollisionTraceFlag == CTF_UseComplexAsSimple
            || Collision->AggGeom.GetElementCount() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("HOUSE_PROP_INVALID_DYNAMIC_COLLISION id=%s mesh=%s"),
                *PropId.ToString(), *GetNameSafe(Body->GetStaticMesh()));
        }
        Body->SetMassOverrideInKg(NAME_None, FMath::Clamp(MassKg, .1f, 300.f), true);
        // A changed leg or collapsed seat can begin slightly inside the floor.
        // Bound the solver correction instead of ejecting the entire furnishing.
        Body->SetMaxDepenetrationVelocity(NAME_None, 100.f);
        Body->BodyInstance.SetOverrideIterationCounts(true);
        Body->BodyInstance.SetPositionSolverIterationCount(8);
        Body->BodyInstance.SetVelocitySolverIterationCount(2);
        Body->SetLinearDamping(.6f);
        Body->SetAngularDamping(1.8f);
        if (IsRollingChair())
        {
            // Casters: almost no sliding friction, so rolling resistance (damping)
            // decides how far it coasts: ~2.5 m from a walk bump, ~4 m from a sprint.
            if (!CasterMaterial)
            {
                CasterMaterial = NewObject<UPhysicalMaterial>(this);
                CasterMaterial->Friction = .05f;
                CasterMaterial->StaticFriction = .06f;
                CasterMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
                CasterMaterial->bOverrideFrictionCombineMode = true;
                CasterMaterial->Restitution = .08f;
            }
            Body->SetPhysMaterialOverride(CasterMaterial);
            Body->SetLinearDamping(1.9f);
            Body->SetAngularDamping(2.2f);
        }
        Body->SetUseCCD(true);
        Body->SetPhysicsMaxAngularVelocityInDegrees(240.f, false);
    }
    const bool bStartingSimulation = bShouldSimulate && !Body->IsSimulatingPhysics();
    if (bStartingSimulation || !bShouldSimulate) ClearIncomingVelocity();
    if (Body->IsSimulatingPhysics() != bShouldSimulate) Body->SetSimulatePhysics(bShouldSimulate);
    if (bStartingSimulation) Body->WakeAllRigidBodies();
    SetActorTickEnabled(HasAuthority() && bShouldSimulate && Body->RigidBodyIsAwake());
}

void AHouseInteractiveProp::RestoreReleasedMotion(const FVector& Velocity, const FVector& AngularVelocity)
{
    ClearIncomingVelocity();
    if (!Body->IsSimulatingPhysics()) return;
    // Preserve a genuine throw/fall through a stage change, but discard extreme
    // handle correction velocities. No upwards boost is needed for breakage.
    Body->SetPhysicsLinearVelocity(Velocity.ContainsNaN() ? FVector::ZeroVector : Velocity.GetClampedToMaxSize(1000.f));
    Body->SetPhysicsAngularVelocityInDegrees(AngularVelocity.ContainsNaN() ? FVector::ZeroVector : AngularVelocity.GetClampedToMaxSize(180.f));
    Body->WakeAllRigidBodies();
    if (HasAuthority() && bPhysicsActive) SetActorTickEnabled(true);
}

bool AHouseInteractiveProp::WakeForInteraction(AActor* Interactor)
{
    if (!HasAuthority() || !CanBeMoved() || !Body || !Body->GetStaticMesh()) return false;
    if (GetWorld()->GetTimeSeconds() < 5.f && FParse::Param(FCommandLine::Get(), TEXT("HouseInteractivitySmoke")))
        UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_STARTUP_WAKE id=%s interactor=%s"), *PropId.ToString(), *GetNameSafe(Interactor));
    if (const APawn* Pawn = Cast<APawn>(Interactor)) LastInteractor = Pawn->GetController();
    else if (Interactor) LastInteractor = Interactor->GetInstigatorController();
    if (bClothMode)
    {
        // E on a hung flag pulls it down; there is no rigid body to carry.
        TearCloth(Interactor ? Interactor->GetActorLocation() : Body->Bounds.Origin, LastInteractor.Get(), false);
        return false;
    }
    if (bWallMounted && !bDetachedFromWall)
    {
        const FVector TowardPlayer = Interactor ? Interactor->GetActorLocation() - Body->Bounds.Origin : -GetActorForwardVector();
        DetachFromWall(TowardPlayer, LastInteractor.Get());
    }
    const bool bWasParked = !bPhysicsActive;
    ClearIncomingVelocity();
    bPhysicsActive = true;
    ApplyPhysicsState();
    if (!bSuppressSupportedWakeForHit && (bWasParked || IsGreatRoomSofa(PropId)))
        WakeSupportedProps(Interactor);
    Body->WakeAllRigidBodies();
    SetActorTickEnabled(true);
    FlushNetDormancy();
    ForceNetUpdate();
    return Body->IsSimulatingPhysics();
}

void AHouseInteractiveProp::DetachFromWall(const FVector& AwayFromWall, AController* Instigator)
{
    if (!HasAuthority() || !bWallMounted || bDetachedFromWall || !Body) return;
    if (bClothMode) { TearCloth(Body->Bounds.Origin + AwayFromWall.GetSafeNormal() * 120.f, Instigator, false); return; }
    bDetachedFromWall = true;
    AttachmentHealth = 0.f;
    bPhysicsActive = true;
    LastInteractor = Instigator;
    FVector Away = AwayFromWall.GetSafeNormal2D();
    if (Away.IsNearlyZero()) Away = -GetActorForwardVector().GetSafeNormal2D();
    if (Away.IsNearlyZero()) Away = FVector::ForwardVector;
    // Clear the hook before gravity starts. The original framed picture remains
    // visible and intact during its fall; its broken variant is a later state.
    Body->SetWorldLocation(Body->GetComponentLocation() + Away * 4.f, false, nullptr, ETeleportType::TeleportPhysics);
    ApplyAppearance();
    ApplyPhysicsState();
    if (Body->IsSimulatingPhysics())
    {
        Body->AddImpulse((Away * 100.f + FVector::UpVector * 18.f) * PropMassResponse(MassKg), NAME_None, true);
        Body->SetPhysicsAngularVelocityInDegrees(FVector::CrossProduct(FVector::UpVector, Away) * 55.f * PropMassResponse(MassKg));
        Body->WakeAllRigidBodies();
    }
    LastImpactTime = GetWorld()->GetTimeSeconds();
    if (UHouseDamageSubsystem* Damage = GetWorld()->GetSubsystem<UHouseDamageSubsystem>())
        Damage->RegisterBrokenActor(this, DamageCategory, Instigator);
    FlushNetDormancy();
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_DETACHED id=%s actor=%s"), *PropId.ToString(), *GetName());
}

void AHouseInteractiveProp::WakeSupportedProps(AActor* Interactor)
{
    if (!HasAuthority() || !Body || bWallMounted || !GetWorld()) return;
    // Authored shelf contents can be hidden by another book in a downward
    // trace. Wake their own bodies once when the case moves or changes shape.
    if (ActorHasTag(TEXT("SSOLibraryShelf")))
    {
        const FName ContentsTag(*FString::Printf(TEXT("ShelfContents_%s"), *PropId.ToString()));
        for (TActorIterator<AHouseInteractiveProp> It(GetWorld()); It; ++It)
            if (*It != this && It->ActorHasTag(ContentsTag) && !It->bPhysicsActive
                && !It->bWallMounted && It->CanBeMoved()) It->WakeForInteraction(Interactor);
    }
    const FBox Support = Body->Bounds.GetBox();
    const bool bSmallSupport = ActorHasTag(TEXT("SSOLibrarySmallItem"));
    if (Support.GetSize().GetMax() < (bSmallSupport ? 6.f : 45.f)) return;
    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams NearbyQuery(SCENE_QUERY_STAT(HouseSupportWake), false, this);
    TArray<FOverlapResult> Nearby;
    GetWorld()->OverlapMultiByObjectType(Nearby, Support.GetCenter() + FVector(0, 0, 12),
        FQuat::Identity, ObjectTypes, FCollisionShape::MakeBox(Support.GetExtent() + FVector(5, 5, 18)), NearbyQuery);
    int32 Woken = 0;
    for (const FOverlapResult& Overlap : Nearby)
    {
        auto* Resting = Cast<AHouseInteractiveProp>(Overlap.GetActor());
        if (!Resting || Resting == this || Resting->bPhysicsActive || Resting->bWallMounted
            || !Resting->CanBeMoved() || !Resting->Body) continue;
        const FBox Item = Resting->Body->Bounds.GetBox();
        const bool bLooseLibraryItem = Resting->ActorHasTag(TEXT("SSOLibrarySmallItem"));
        if (Item.Min.Z < Support.Min.Z + ((bSmallSupport || bLooseLibraryItem) ? -.1f : 8.f) || Item.Min.Z > Support.Max.Z + 14.f) continue;
        const FVector Bottom(Item.GetCenter().X, Item.GetCenter().Y, Item.Min.Z);
        FCollisionQueryParams SupportQuery(SCENE_QUERY_STAT(HouseRestingSupport), false, Resting);
        FHitResult Contact;
        if (!GetWorld()->LineTraceSingleByChannel(Contact, Bottom + FVector(0, 0, 6),
            Bottom - FVector(0, 0, 26), ECC_Visibility, SupportQuery) || Contact.GetActor() != this) continue;
        // Sleeping cups, books and cushions must join physics when their actual
        // support is moved. The query runs once per wake/break, never every frame.
        Resting->WakeForInteraction(Interactor);
        // A filled library shelf may carry far more than 24 separate books.
        if (++Woken >= (ActorHasTag(TEXT("SSOLibraryRebuild")) ? 256 : 24)) break;
    }
}

void AHouseInteractiveProp::OnRep_PropState()
{
    if (bClothMode)
    {
        // A late joiner or a missed multicast still sees the flag down.
        if ((bDetachedFromWall || bBroken) && ClothFlag && ClothFlag->IsActive() && !ClothFlag->IsReleased())
            ClothFlag->Release(Body->Bounds.Origin, true);
        ApplyPhysicsState();
        return;
    }
    const bool bMeshChanged = Body && Body->GetStaticMesh() != MeshForDamageStage(bBroken ? 3 : DamageStage);
    const FVector Velocity = Body && Body->IsSimulatingPhysics() ? Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector Angular = Body && Body->IsSimulatingPhysics() ? Body->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;
    ApplyAppearance();
    ApplyPhysicsState();
    if (bMeshChanged) RestoreReleasedMotion(Velocity, Angular);
}

void AHouseInteractiveProp::ReleaseBodyHolders()
{
    UWorld* World = GetWorld();
    if (!World || !Body) return;
    // Mesh replacement recreates the rigid body. Release matching constraints
    // before the swap so a destroyed chair cannot leave a player's grab stuck.
    for (TActorIterator<APawn> It(World); It; ++It)
    {
        TInlineComponentArray<UPhysicsHandleComponent*> Handles(*It);
        for (UPhysicsHandleComponent* Handle : Handles)
        {
            if (Handle && Handle->GetGrabbedComponent() == Body)
            {
                if (UHouseFurnitureInteractionComponent* Interaction = It->FindComponentByClass<UHouseFurnitureInteractionComponent>())
                    Interaction->ReleaseGrab();
                else Handle->ReleaseComponent();
            }
        }
    }
    Body->ComponentTags.Remove(HeldTag);
}

float AHouseInteractiveProp::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority() || !CanBeDamaged() || bBroken || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
        return 0.f;
    const float ActualDamage = FMath::Max(0.f, Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser));
    if (ActualDamage <= 0.f) return 0.f;
    const int32 StageBefore = bBroken ? 3 : DamageStage;
    const bool bWasBroken = bBroken;
    ++DamageEvents;
    TGuardValue<bool> ContactOriginGuard(bApplyingContactDamage,
        bApplyingContactDamage || Cast<AHouseInteractiveProp>(DamageCauser) != nullptr);
    // A moving prop's collision applies damage to both sides once. If this
    // target also receives the same physics callback, do not charge that impact
    // a second time after the initial hit has just woken its rigid body.
    if (Cast<AHouseInteractiveProp>(DamageCauser)) LastImpactTime = GetWorld()->GetTimeSeconds();
    FVector HitLocation = Body->Bounds.Origin;
    FVector HitDirection = DamageCauser ? (HitLocation - DamageCauser->GetActorLocation()).GetSafeNormal() : FVector::UpVector;
    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
        HitLocation = Point.HitInfo.ImpactPoint;
        HitDirection = Point.ShotDirection.GetSafeNormal();
    }
    if (bWallMounted && !bDetachedFromWall)
    {
        AttachmentHealth = FMath::Max(0.f, AttachmentHealth - ActualDamage);
        if (AttachmentHealth <= 0.f) DetachFromWall(-HitDirection, EventInstigator);
        // An LCD can crack before its mount gives way. Its fitted replacement
        // stays parked until the mount releases; pictures keep their existing
        // intact-then-detach behavior.
        if (ActorHasTag(TEXT("HouseWallTelevision")))
        {
            Health = FMath::Max(0.f, Health - ActualDamage);
            if (Health <= 0.f)
            {
                if (!bDetachedFromWall) DetachFromWall(-HitDirection, EventInstigator);
                BreakProp(HitLocation, HitDirection, EventInstigator);
            }
            else UpdateDamageStage(EventInstigator);
        }
        {
            const uint8 Kind = bBroken && !bWasBroken ? 2 : ((bBroken ? 3 : DamageStage) > StageBefore ? 1 : 0);
            if (Kind > 0 || GetWorld()->GetTimeSeconds() - LastHitFxTime > .1f) MulticastDamageFx(HitLocation, HitDirection, Kind);
        }
        ForceNetUpdate();
        return ActualDamage;
    }
    Health = FMath::Max(0.f, Health - ActualDamage);
    if (Health <= 0.f) BreakProp(HitLocation, HitDirection, EventInstigator);
    else
    {
        UpdateDamageStage(EventInstigator);
        bool bWokeForImpact = false;
        if (!bClothMode && CanBeMoved())
        {
            const bool bPreviousSuppression = bSuppressSupportedWakeForHit;
            bSuppressSupportedWakeForHit = IsGreatRoomSofa(PropId);
            bWokeForImpact = WakeForInteraction(DamageCauser);
            bSuppressSupportedWakeForHit = bPreviousSuppression;
        }
        if (bWokeForImpact && !bApplyingContactDamage && !Cast<AHouseInteractiveProp>(DamageCauser))
        {
            Body->AddImpulse(HitDirection * 32.f * PropMassResponse(MassKg), NAME_None, true);
            // A visible shudder: the top rocks away from the blow, then settles.
            const FVector Rock = FVector::CrossProduct(FVector::UpVector, HitDirection).GetSafeNormal();
            if (!Rock.IsNearlyZero()) Body->AddAngularImpulseInDegrees(Rock * 120.f * PropMassResponse(MassKg), NAME_None, true);
        }
    }
    {
        const uint8 Kind = bBroken && !bWasBroken ? 2 : ((bBroken ? 3 : DamageStage) > StageBefore ? 1 : 0);
        if (Kind > 0 || GetWorld()->GetTimeSeconds() - LastHitFxTime > .1f) MulticastDamageFx(HitLocation, HitDirection, Kind);
    }
    ForceNetUpdate();
    return ActualDamage;
}

void AHouseInteractiveProp::BreakProp(const FVector& HitLocation, const FVector& HitDirection, AController* Instigator)
{
    if (!HasAuthority() || bBroken) return;
    if (bClothMode) { TearCloth(HitLocation - HitDirection.GetSafeNormal() * 120.f, Instigator, true); return; }
    WakeSupportedProps(Instigator ? Instigator->GetPawn() : nullptr);
    const FVector PreviousVelocity = Body->IsSimulatingPhysics() ? Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const FVector PreviousAngular = Body->IsSimulatingPhysics() ? Body->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;
    const FBox PreviousBounds = Body->Bounds.GetBox();
    ReleaseBodyHolders();
    bBroken = true;
    DamageStage = 3;
    Health = 0.f;
    ++BreakEvents;
    bPhysicsActive = bBrokenVersionMovable || bDetachedFromWall;
    LastInteractor = Instigator;
    ApplyAppearance();
    if (ActorHasTag(TEXT("SSOLibraryShelf")))
    {
        // The collapsed case has a different center and bottom. Preserve the
        // visible body's position, rather than a tipped case's old floor pivot.
        const FRotator RestRotation(0.f, GetActorRotation().Yaw, 0.f);
        SetActorRotation(RestRotation, ETeleportType::TeleportPhysics);
        Body->UpdateBounds();
        FBox NewBounds = Body->Bounds.GetBox();
        FVector Offset = PreviousBounds.GetCenter() - NewBounds.GetCenter();
        FCollisionObjectQueryParams FloorTypes(ECC_WorldStatic);
        FCollisionQueryParams FloorQuery(SCENE_QUERY_STAT(ShelfWreckFloor), false, this);
        FHitResult Floor;
        const FVector Probe = PreviousBounds.GetCenter() + FVector(0, 0, 20.f);
        if (GetWorld()->LineTraceSingleByObjectType(Floor, Probe, Probe - FVector(0, 0, PreviousBounds.GetExtent().Z + 100.f), FloorTypes, FloorQuery)
            && PreviousBounds.Min.Z - Floor.ImpactPoint.Z < 35.f)
            Offset.Z = Floor.ImpactPoint.Z + 1.f - NewBounds.Min.Z;
        SetActorLocation(GetActorLocation() + Offset, false, nullptr, ETeleportType::TeleportPhysics);
    }
    ApplyPhysicsState();
    RestoreReleasedMotion(PreviousVelocity, PreviousAngular);
    if (Body->IsSimulatingPhysics())
    {
        const FVector Direction = HitDirection.GetSafeNormal();
        if (!bApplyingContactDamage)
        {
            // Give way rather than fly: slump along the floor and tip away from
            // the blow. Heavy pieces barely shift; light ones skid and roll.
            const FVector Along(Direction.X, Direction.Y, 0.f);
            Body->AddImpulse(Along.GetSafeNormal() * 38.f * PropMassResponse(MassKg), NAME_None, true);
            const FVector Tip = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
            if (!Tip.IsNearlyZero()) Body->AddAngularImpulseInDegrees(Tip * 110.f * PropMassResponse(MassKg), NAME_None, true);
        }
        Body->WakeAllRigidBodies();
    }
    LastImpactTime = GetWorld()->GetTimeSeconds();
    if (UHouseDamageSubsystem* Damage = GetWorld()->GetSubsystem<UHouseDamageSubsystem>())
        Damage->RegisterBrokenActor(this, DamageCategory, Instigator);
    FlushNetDormancy();
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_BROKEN id=%s actor=%s variant=%s impact=%s"),
        *PropId.ToString(), *GetName(), *GetNameSafe(BrokenMesh), *HitLocation.ToCompactString());
}

void AHouseInteractiveProp::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority() || !bPhysicsActive || !IsValid(OtherActor) || !OtherComponent
        || OtherActor == this || Cast<APawn>(OtherActor) || Body->ComponentHasTag(HeldTag)) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastImpactTime < .2f) return;
    const FVector RelativeVelocity = Body->GetPhysicsLinearVelocity() - OtherComponent->GetComponentVelocity();
    FVector OtherIncoming = OtherComponent->GetComponentVelocity();
    if (const AHouseInteractiveProp* OtherProp = Cast<AHouseInteractiveProp>(OtherActor);
        OtherProp && OtherProp->Body == OtherComponent && OtherProp->HasFreshIncomingVelocity())
        OtherIncoming = OtherProp->IncomingVelocity;
    const float IncomingNormalSpeed = HasFreshIncomingVelocity()
        ? FMath::Abs(FVector::DotProduct(IncomingVelocity - OtherIncoming, Hit.ImpactNormal)) : 0.f;
    const float CurrentNormalSpeed = FMath::Abs(FVector::DotProduct(RelativeVelocity, Hit.ImpactNormal));
    // A supported light object can receive a large constraint impulse from a
    // heavy seat without an actual impact. Do not turn resting support forces
    // into damage; retain pre-contact and angular point motion for real hits.
    const FVector PointVelocity = Body->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint)
        - OtherComponent->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
    const float PointNormalSpeed = FMath::Abs(FVector::DotProduct(PointVelocity, Hit.ImpactNormal));
    if (FMath::Max3(IncomingNormalSpeed, CurrentNormalSpeed, PointNormalSpeed) < 90.f) return;
    const float ImpulseSpeed = NormalImpulse.Size() / FMath::Max(.1f, Body->GetMass());
    const float ImpactSpeed = FMath::Max3(IncomingNormalSpeed, CurrentNormalSpeed, ImpulseSpeed);
#if !UE_BUILD_SHIPPING
    if (PropId == FName(TEXT("smoke_thrown_piece")) && Now - LastImpactDiagnosticTime >= .1f)
    {
        LastImpactDiagnosticTime = Now;
        UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_THROW_IMPACT other=%s incoming=%.2f current=%.2f impulse=%.2f threshold=%.2f fresh=%d"),
            *GetNameSafe(OtherActor), IncomingNormalSpeed, CurrentNormalSpeed, ImpulseSpeed, ImpactBreakSpeed,
            HasFreshIncomingVelocity() ? 1 : 0);
    }
#endif
    const float BreakSpeed = IsRollingChair() ? FMath::Max(ImpactBreakSpeed, 820.f) : ImpactBreakSpeed;
    if (!FMath::IsFinite(ImpactSpeed)) return;
    if (ImpactSpeed >= 90.f && Now - LastBumpSoundTime >= .18f && Now - LastHitFxTime >= .25f)
    {
        LastBumpSoundTime = Now;
        MulticastBumpFx(Hit.ImpactPoint, uint8(FMath::Clamp(ImpactSpeed / 4.f, 1.f, 255.f)));
    }
    if (ImpactSpeed < BreakSpeed) return;
    LastImpactTime = Now;
    FVector Direction = -Hit.ImpactNormal;
    if (Direction.IsNearlyZero()) Direction = RelativeVelocity.GetSafeNormal();
    AController* Instigator = LastInteractor.Get();
    const float ImpactDamage = FMath::Clamp(8.f * FMath::Square(ImpactSpeed / FMath::Max(100.f, ImpactBreakSpeed))
        * FMath::Sqrt(FMath::Max(.1f, Body->GetMass()) / 15.f), 8.f, 32.f);
    // Only deliberately moved bodies reach this callback. Parked furnishings
    // cannot start breaking themselves from initial settling or player bumps.
    if (OtherActor->ActorHasTag(BreakableTag) && OtherActor->CanBeDamaged())
        UGameplayStatics::ApplyPointDamage(OtherActor, ImpactDamage, Direction, Hit, Instigator, this, UDamageType::StaticClass());
    if (!bBroken && bBreakable)
    {
        FPointDamageEvent Damage;
        Damage.Damage = ImpactDamage;
        Damage.ShotDirection = Direction;
        Damage.HitInfo = Hit;
        // Chaos already applied this contact's physical impulse. Adding another
        // synthetic kick on top makes heavy items bounce after every stage swap.
        TGuardValue<bool> ContactDamageGuard(bApplyingContactDamage, true);
        TakeDamage(Damage.Damage, Damage, Instigator, OtherActor);
    }
}

UMaterialInterface* AHouseInteractiveProp::DebrisSurfaceMaterial(int32 Rank) const
{
    // Use the material covering most of the piece (leather on a sofa, not its
    // small metal base), measured by triangles per material in the intact mesh.
    int32 Best = 0;
    if (IntactMesh && IntactMesh->GetRenderData() && IntactMesh->GetRenderData()->LODResources.Num() > 0)
    {
        TMap<int32, uint32> Triangles;
        for (const FStaticMeshSection& Section : IntactMesh->GetRenderData()->LODResources[0].Sections)
            if (!IntactMesh->GetStaticMaterials().IsValidIndex(Section.MaterialIndex)
                || !IsAddedDamageMaterial(IntactMesh->GetStaticMaterials()[Section.MaterialIndex]))
                Triangles.FindOrAdd(Section.MaterialIndex) += Section.NumTriangles;
        // Rank 0 = main material, rank 1 = the next most used one.
        Triangles.ValueSort([](uint32 A, uint32 B) { return A > B; });
        int32 Position = 0;
        for (const TPair<int32, uint32>& Entry : Triangles)
        {
            if (Position == Rank) { Best = Entry.Key; break; }
            ++Position;
        }
        if (Rank > 0 && Position < Rank) return nullptr;
    }
    else if (Rank > 0) return nullptr;
    if (MaterialOverrides.IsValidIndex(Best) && MaterialOverrides[Best]) return MaterialOverrides[Best];
    if (IntactMesh) return IntactMesh->GetMaterial(Best);
    return Body ? Body->GetMaterial(0) : nullptr;
}

UMaterialInterface* AHouseInteractiveProp::DebrisInnerMaterial(bool bBreak) const
{
    // The damage variants add their exposed wood, foam or core as M_HI_ slots.
    for (UStaticMesh* Mesh : {bBreak ? BrokenMesh.Get() : nullptr, HeavilyDamagedMesh.Get(), DamagedMesh.Get(), BrokenMesh.Get()})
    {
        if (!Mesh) continue;
        const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
            if (IsAddedDamageMaterial(Slots[Index]) && Slots[Index].MaterialInterface) return Slots[Index].MaterialInterface;
    }
    return nullptr;
}

UMaterialInterface* AHouseInteractiveProp::DebrisGlassMaterial() const
{
    if (IntactMesh)
    {
        const TArray<FStaticMaterial>& Slots = IntactMesh->GetStaticMaterials();
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
        {
            const FString Name = (Slots[Index].MaterialSlotName.ToString() + Slots[Index].ImportedMaterialSlotName.ToString()).ToLower();
            if (Name.Contains(TEXT("glass")) || Name.Contains(TEXT("screen")) || Name.Contains(TEXT("mirror")) || Name.Contains(TEXT("lcd")))
            {
                UMaterialInterface* Override = MaterialOverrides.IsValidIndex(Index) ? MaterialOverrides[Index].Get() : nullptr;
                return Override ? Override : Slots[Index].MaterialInterface.Get();
            }
        }
    }
    if (ActorHasTag(TEXT("HouseWallTelevision")) && Body) return Body->GetMaterial(0);
    // Framed pictures and mirrors shed glass even when the asset has no glass slot.
    const FString Name = (PropName + TEXT(" ") + PropId.ToString()).ToLower();
    for (const TCHAR* Word : {TEXT("frame"), TEXT("collage"), TEXT("picture"), TEXT("photo"), TEXT("mirror"), TEXT("artwork")})
        if (Name.Contains(Word) && (bWallMounted || Body->Bounds.BoxExtent.GetMin() < 6.f))
            return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/BasementLibrary/M_Lib_JarGlass.M_Lib_JarGlass"));
    return nullptr;
}

void AHouseInteractiveProp::MulticastDamageFx_Implementation(FVector_NetQuantize Impact, FVector_NetQuantizeNormal Direction, uint8 Kind)
{
    PlayDamageFx(Impact, Direction, Kind);
}

void AHouseInteractiveProp::MulticastBumpFx_Implementation(FVector_NetQuantize At, uint8 Strength)
{
    // Strength counts in 4 cm/s steps: 90 cm/s is a nudge, 600+ a proper wallop.
    HouseSfx::Impact(GetWorld(), At, SfxSurface, FMath::Clamp((Strength * 4.f - 60.f) / 540.f, .05f, 1.f), MassKg);
}

void AHouseInteractiveProp::PlayDamageFx(const FVector& Impact, const FVector& Direction, uint8 Kind)
{
    UWorld* World = GetWorld();
    if (!World || !Body) return;
    LastHitFxTime = World->GetTimeSeconds();
    if (GetNetMode() == NM_DedicatedServer) return;
    if (bClothMode) HouseSfx::Impact(World, Impact, EHouseSurface::Soft, .5f, MassKg);
    else if (Kind == 0) HouseSfx::Impact(World, Impact, SfxSurface, .9f, MassKg);
    else HouseSfx::Break(World, Impact, SfxSurface, MassKg, Kind == 1);
    if (bClothMode)
    {
        // Cloth does not crack or splinter: it flaps away from the blow.
        if (ClothFlag) ClothFlag->AddImpulse(Impact, Direction, Kind >= 1 ? 520.f : 380.f);
        if (UHouseDamageSubsystem* Damage = World->GetSubsystem<UHouseDamageSubsystem>()) Damage->AddCameraImpact(Impact, .1f);
        return;
    }
    const EHouseDebrisKind DebrisKind = Kind >= 2 ? EHouseDebrisKind::Break : (Kind == 1 ? EHouseDebrisKind::Crack : EHouseDebrisKind::Hit);
    // Stand-in animation: a break buckles the last damaged shape down into the
    // broken one; a crack shudders. The real body stays the physics authority.
    if (BreakDebris && Kind >= 1 && Body->IsVisible())
    {
        UStaticMesh* GhostMesh = Kind >= 2 ? MeshForDamageStage(2) : Body->GetStaticMesh().Get();
        if (Kind >= 2 && GhostMesh == BrokenMesh) GhostMesh = IntactMesh;
        if (GhostMesh)
        {
            UStaticMeshComponent* Ghost = NewObject<UStaticMeshComponent>(this);
            Ghost->SetMobility(EComponentMobility::Movable);
            Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Ghost->SetStaticMesh(GhostMesh);
            Ghost->SetWorldTransform(Body->GetComponentTransform());
            Ghost->LightingChannels = Body->LightingChannels;   // lit like the piece it stands in for
            for (int32 OriginalSlot = 0; OriginalSlot < MaterialOverrides.Num(); ++OriginalSlot)
                if (MaterialOverrides[OriginalSlot])
                {
                    const int32 Slot = PropOverrideSlot(IntactMesh, GhostMesh, OriginalSlot);
                    if (Slot != INDEX_NONE) Ghost->SetMaterial(Slot, MaterialOverrides[OriginalSlot]);
                }
            Ghost->RegisterComponent();
            const float Heavy = FMath::Clamp(FMath::Sqrt(FMath::Max(1.f, MassKg)) / 10.f, 0.f, 1.f);
            BreakDebris->PlayGhost(Ghost, Body, Kind >= 2, Direction, Kind >= 2 ? FMath::Lerp(.15f, .24f, Heavy) : .16f);
        }
    }
#if !UE_BUILD_SHIPPING
    UE_LOG(LogTemp, Display, TEXT("HOUSE_PROP_DEBRIS id=%s kind=%d surface=%s inner=%s glass=%s"), *PropId.ToString(), Kind,
        *(GetNameSafe(DebrisSurfaceMaterial(0)) + TEXT("+") + GetNameSafe(DebrisSurfaceMaterial(1))), *GetNameSafe(DebrisInnerMaterial(Kind >= 2)), *GetNameSafe(DebrisGlassMaterial()));
#endif
    if (BreakDebris)
        BreakDebris->Burst(DebrisKind, Body->Bounds.GetBox(), Impact, Direction, DebrisSurfaceMaterial(0),
            DebrisInnerMaterial(Kind >= 2), DebrisGlassMaterial(), MassKg, DebrisSurfaceMaterial(1));
    if (UHouseDamageSubsystem* Damage = World->GetSubsystem<UHouseDamageSubsystem>())
    {
        const float Size = Body->Bounds.BoxExtent.GetMax();
        const float Strength = Kind >= 2 ? FMath::Clamp(.5f + FMath::Sqrt(FMath::Max(1.f, MassKg)) * .16f + Size * .004f, .7f, 3.f)
            : (Kind == 1 ? .45f : .15f);
        Damage->AddCameraImpact(Impact, Strength);
    }
}
