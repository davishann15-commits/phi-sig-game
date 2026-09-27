#include "HouseFurnitureInteractionComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

namespace
{
    const FName MovableFurnitureTag(TEXT("SSOMovableFurniture"));
    // Server-only reservation so two players cannot pull the same body at once.
    const FName HeldFurnitureTag(TEXT("SSOFurnitureHeld"));
}

UHouseFurnitureInteractionComponent::UHouseFurnitureInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void UHouseFurnitureInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    SetComponentTickEnabled(false);

    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;

    PhysicsHandle = NewObject<UPhysicsHandleComponent>(Owner);
    Owner->AddInstanceComponent(PhysicsHandle);
    PhysicsHandle->RegisterComponent();
    PhysicsHandle->SetLinearStiffness(1800.0f);
    PhysicsHandle->SetLinearDamping(180.0f);
    PhysicsHandle->SetAngularStiffness(1200.0f);
    PhysicsHandle->SetAngularDamping(120.0f);
    PhysicsHandle->SetInterpolationSpeed(12.0f);
}

void UHouseFurnitureInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetOwner() && GetOwner()->HasAuthority()) ReleaseGrabAuthoritative();
    if (IsValid(PhysicsHandle)) PhysicsHandle->DestroyComponent();
    PhysicsHandle = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UHouseFurnitureInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, bHoldingFurniture);
}

void UHouseFurnitureInteractionComponent::ToggleGrab()
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) ToggleGrabAuthoritative();
    else ServerToggleGrab();
}

void UHouseFurnitureInteractionComponent::ReleaseGrab()
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) ReleaseGrabAuthoritative();
    else ServerReleaseGrab();
}

void UHouseFurnitureInteractionComponent::ServerToggleGrab_Implementation()
{
    ToggleGrabAuthoritative();
}

void UHouseFurnitureInteractionComponent::ServerReleaseGrab_Implementation()
{
    ReleaseGrabAuthoritative();
}

bool UHouseFurnitureInteractionComponent::GetServerView(FVector& OutLocation, FRotator& OutRotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->GetController()) return false;
    Pawn->GetController()->GetPlayerViewPoint(OutLocation, OutRotation);
    return !OutLocation.ContainsNaN() && !OutRotation.ContainsNaN();
}

void UHouseFurnitureInteractionComponent::ToggleGrabAuthoritative()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !IsValid(PhysicsHandle)) return;

    if (bHoldingFurniture)
    {
        ReleaseGrabAuthoritative();
        return;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (!GetServerView(ViewLocation, ViewRotation)) return;

    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Owner);
    Query.bTraceComplex = false;
    FHitResult Hit;
    const FVector ViewDirection = ViewRotation.Vector();
    if (!World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewDirection * GrabRange,
        ECC_Visibility, Query)) return;

    AActor* Furniture = Hit.GetActor();
    UPrimitiveComponent* Body = Hit.GetComponent();
    if (!IsValid(Furniture) || !IsValid(Body) || !Furniture->ActorHasTag(MovableFurnitureTag)
        || Body->ComponentHasTag(HeldFurnitureTag) || !Body->IsSimulatingPhysics()) return;

    // Actor movement replication follows the root body's transform. The setup
    // tool must make the draggable mesh the root and enable Visibility collision.
    if (Furniture->GetRootComponent() != Body || FVector::Dist(ViewLocation, Hit.ImpactPoint) > GrabRange) return;

    Furniture->SetReplicates(true);
    Furniture->SetReplicateMovement(true);
    Furniture->ForceNetUpdate();

    HeldRotation = Body->GetComponentRotation();
    HoldDistance = FMath::Clamp(FVector::Dist(ViewLocation, Body->GetComponentLocation()),
        MinimumHoldDistance, MaximumHoldDistance);
    PhysicsHandle->GrabComponentAtLocationWithRotation(Body, NAME_None, Body->GetComponentLocation(), HeldRotation);
    if (PhysicsHandle->GetGrabbedComponent() != Body) return;

    Body->ComponentTags.AddUnique(HeldFurnitureTag);
    HeldActor = Furniture;
    HeldComponent = Body;
    bHoldingFurniture = true;
    SetComponentTickEnabled(true);
    Owner->ForceNetUpdate();
}

void UHouseFurnitureInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bHoldingFurniture) return;

    AActor* Furniture = HeldActor.Get();
    UPrimitiveComponent* Body = HeldComponent.Get();
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!IsValid(Furniture) || !IsValid(Body) || !Body->IsSimulatingPhysics()
        || !IsValid(PhysicsHandle) || PhysicsHandle->GetGrabbedComponent() != Body
        || !GetServerView(ViewLocation, ViewRotation)
        || FVector::Dist(ViewLocation, Body->GetComponentLocation()) > BreakDistance)
    {
        ReleaseGrabAuthoritative();
        return;
    }

    const FVector Direction = ViewRotation.Vector();
    FVector Target = ViewLocation + Direction * HoldDistance;
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(GetOwner());
    Query.AddIgnoredActor(Furniture);
    Query.bTraceComplex = false;
    FHitResult Obstacle;
    if (GetWorld()->LineTraceSingleByChannel(Obstacle, ViewLocation, Target, ECC_Visibility, Query))
    {
        const float ClearDistance = FVector::DotProduct(Obstacle.ImpactPoint - ViewLocation, Direction) - 25.0f;
        if (ClearDistance < MinimumHoldDistance)
        {
            ReleaseGrabAuthoritative();
            return;
        }
        Target = ViewLocation + Direction * ClearDistance;
    }

    PhysicsHandle->SetTargetLocationAndRotation(Target, HeldRotation);
}

void UHouseFurnitureInteractionComponent::ReleaseGrabAuthoritative()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (IsValid(PhysicsHandle)) PhysicsHandle->ReleaseComponent();
    if (UPrimitiveComponent* Body = HeldComponent.Get()) Body->ComponentTags.Remove(HeldFurnitureTag);
    if (AActor* Furniture = HeldActor.Get()) Furniture->ForceNetUpdate();
    HeldActor.Reset();
    HeldComponent.Reset();
    HoldDistance = 0.0f;
    bHoldingFurniture = false;
    SetComponentTickEnabled(false);
    if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
}
