#include "HouseFurnitureInteractionComponent.h"
#include "HouseInteractiveProp.h"
#include "HouseBasketball.h"
#include "HouseWoodcutting.h"
#include "GameFramework/GameStateBase.h"
#include "HouseSfx.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

namespace
{
    const FName MovableFurnitureTag(TEXT("SSOMovableFurniture"));
    // Server-only reservation so two players cannot pull the same body at once.
    const FName HeldFurnitureTag(TEXT("SSOFurnitureHeld"));

    bool ResolveFurnitureSurface(UPrimitiveComponent* Body, const FVector& Point,
        const FVector& ViewLocation, const FVector& ViewRight, const FVector& ViewForward,
        float Range, FHitResult& Surface)
    {
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FurnitureSurface),true);
        const float Depth=FMath::Max(280.f,Body->Bounds.SphereRadius*2.f+20.f);
        auto Trace=[&](const FVector& Candidate,FHitResult& Hit)
        {
            return Body->LineTraceComponent(Hit,Candidate-ViewForward*Depth,Candidate+ViewForward*Depth,Query)
                && FVector::DistSquared(ViewLocation,Hit.ImpactPoint)<=FMath::Square(Range);
        };
        if(Trace(Point,Surface))return true;
        // A simple hull may bridge empty space between cushions or chair arms.
        // Focus and pickup agree on a reachable real surface beside that gap.
        const FVector ViewUp=FVector::CrossProduct(ViewForward,ViewRight).GetSafeNormal();
        for(float Radius:{8.f,16.f,32.f,48.f})
        {
            float Best=BIG_NUMBER;
            for(const FVector& Offset:{ViewRight*Radius,-ViewRight*Radius,ViewUp*Radius,-ViewUp*Radius})
            {
                FHitResult Candidate;
                if(Trace(Point+Offset,Candidate))
                {
                    const float Distance=FVector::DistSquared(Point,Candidate.ImpactPoint);
                    if(Distance<Best){Best=Distance;Surface=Candidate;}
                }
            }
            if(Best<BIG_NUMBER)return true;
        }
        return false;
    }

    bool GetFurnitureSupportBottom(UPrimitiveComponent* Body, float& OutBottom)
    {
        const UBodySetup* Setup = Body->GetBodySetup();
        if (!Setup) return false;
        const FTransform& BodyToWorld = Body->GetComponentTransform();
        double Bottom = TNumericLimits<double>::Max();
        // Test actual collision vertices, not transformed render-box corners:
        // the latter can sit well below the feet of tilted chairs and sofas.
        for (const FKConvexElem& Hull : Setup->AggGeom.ConvexElems)
        {
            if (!CollisionEnabledHasPhysics(Hull.GetCollisionEnabled())) continue;
            const FTransform& ElementToBody = Hull.GetTransform();
            for (const FVector& Vertex : Hull.VertexData)
                Bottom = FMath::Min(Bottom, BodyToWorld.TransformPosition(
                    ElementToBody.TransformPosition(Vertex)).Z);
        }
        for (const FKBoxElem& Box : Setup->AggGeom.BoxElems)
        {
            if (!CollisionEnabledHasPhysics(Box.GetCollisionEnabled())) continue;
            const FTransform ElementToBody = Box.GetTransform();
            for (int32 Corner = 0; Corner < 8; ++Corner)
            {
                const FVector Vertex((Corner & 1 ? .5 : -.5) * Box.X,
                    (Corner & 2 ? .5 : -.5) * Box.Y, (Corner & 4 ? .5 : -.5) * Box.Z);
                Bottom = FMath::Min(Bottom, BodyToWorld.TransformPosition(
                    ElementToBody.TransformPosition(Vertex)).Z);
            }
        }
        if (Bottom == TNumericLimits<double>::Max() || !FMath::IsFinite(Bottom)) return false;
        OutBottom = static_cast<float>(Bottom);
        return true;
    }

    FVector Flatten(const FVector& V, const FVector& Fallback)
    {
        const FVector F(V.X, V.Y, 0.f);
        return F.SizeSquared() > KINDA_SMALL_NUMBER ? F.GetSafeNormal() : Fallback;
    }

    /** Large pieces: both palms on the top lip of the face the player stands at. */
    bool CaptureLargeTopGrip(UPrimitiveComponent* Body, const FHitResult& Primary, const FVector& ViewRight,
        const FVector& ViewForward, FVector& R, FVector& L, FVector& RN, FVector& LN, FVector& Face, float& Span)
    {
        FVector N(Primary.ImpactNormal.X, Primary.ImpactNormal.Y, 0.f);
        // Looking down at a seat or top: lift the side that faces the player.
        if (N.SizeSquared() < .2f) N = -FVector(ViewForward.X, ViewForward.Y, 0.f);
        if (N.SizeSquared() < KINDA_SMALL_NUMBER) return false;
        N.Normalize();
        const FVector E = Body->Bounds.BoxExtent, O = Body->Bounds.Origin;
        const float Height = E.Z * 2.f + 40.f;
        FVector Tangent = FVector::CrossProduct(FVector::UpVector, N).GetSafeNormal();
        if (FVector::DotProduct(Tangent, ViewRight) < 0.f) Tangent = -Tangent;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FurnitureTopLip), true);
        auto Top = [&](const FVector& At, FHitResult& Hit)
        {
            const FVector In = At - N * 5.f;
            return Body->LineTraceComponent(Hit, FVector(In.X, In.Y, O.Z + Height), FVector(In.X, In.Y, O.Z - Height), Query);
        };
        FHitResult Center;
        if (!Top(Primary.ImpactPoint, Center)) return false;
        const float FaceWidth = 2.f * (FMath::Abs(Tangent.X) * E.X + FMath::Abs(Tangent.Y) * E.Y);
        float S = FMath::Clamp(FaceWidth * .22f, 13.f, 25.f);
        FHitResult RH, LH;
        bool bFound = false;
        for (int32 Attempt = 0; Attempt < 4 && !bFound; ++Attempt, S *= .7f)
            bFound = Top(Center.ImpactPoint + Tangent * S, RH) && Top(Center.ImpactPoint - Tangent * S, LH)
                && FMath::Abs(RH.ImpactPoint.Z - Center.ImpactPoint.Z) < 22.f
                && FMath::Abs(LH.ImpactPoint.Z - Center.ImpactPoint.Z) < 22.f;
        if (!bFound)
        {
            RH = Center; LH = Center;
            RH.ImpactPoint += Tangent * 6.f; LH.ImpactPoint -= Tangent * 6.f;
        }
        R = RH.ImpactPoint; L = LH.ImpactPoint;
        RN = RH.ImpactNormal.GetSafeNormal(); LN = LH.ImpactNormal.GetSafeNormal();
        Face = N;
        Span = FMath::Max(40.f, 2.f * (FMath::Abs(N.X) * E.X + FMath::Abs(N.Y) * E.Y));
        return true;
    }

    /** Medium pieces: one palm on each side, like carrying a box or an end table. */
    bool CaptureMediumSideGrip(UPrimitiveComponent* Body, const FHitResult& Primary, const FVector& ViewForward,
        FVector& R, FVector& L, FVector& RN, FVector& LN)
    {
        const FVector Flat = Flatten(ViewForward, FVector::ZeroVector);
        if (Flat.IsNearlyZero()) return false;
        const FVector Side(-Flat.Y, Flat.X, 0.f);
        const FVector E = Body->Bounds.BoxExtent, C = Body->Bounds.Origin;
        const float DepthHalf = FMath::Abs(Flat.X) * E.X + FMath::Abs(Flat.Y) * E.Y;
        const float SideHalf = FMath::Abs(Side.X) * E.X + FMath::Abs(Side.Y) * E.Y;
        const float Reach = Body->Bounds.SphereRadius + 25.f;
        const float Bottom = C.Z - E.Z;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FurnitureSideGrip), true);
        // Search heights and depths for real outer side surfaces (frame,
        // arms, sides), not fabric or slats near the middle of the piece.
        const float Heights[] = { .55f, .7f, .4f, .85f, .25f, .95f };
        const float Depths[] = { -.3f, -.55f, 0.f, -.75f, .25f };
        for (const float Outer : { .55f, .35f })
            for (float H : Heights)
                for (float D : Depths)
                {
                    const FVector Mid = FVector(C.X, C.Y, Bottom + 2.f * E.Z * H) + Flat * (D * DepthHalf);
                    FHitResult RH, LH;
                    if (!Body->LineTraceComponent(RH, Mid + Side * Reach, Mid, Query)
                        || !Body->LineTraceComponent(LH, Mid - Side * Reach, Mid, Query)) continue;
                    if (FVector::DotProduct(RH.ImpactPoint - Mid, Side) < Outer * SideHalf
                        || FVector::DotProduct(LH.ImpactPoint - Mid, -Side) < Outer * SideHalf) continue;
                    const float Width = FVector::Dist(RH.ImpactPoint, LH.ImpactPoint);
                    if (Width < 20.f || Width > 150.f) continue;
                    // Round legs and spindles face every way; only reject surfaces facing inward.
                    if (FVector::DotProduct(RH.ImpactNormal, Side) < -.1f || FVector::DotProduct(LH.ImpactNormal, -Side) < -.1f) continue;
                    R = RH.ImpactPoint; L = LH.ImpactPoint;
                    RN = RH.ImpactNormal.GetSafeNormal(); LN = LH.ImpactNormal.GetSafeNormal();
                    return true;
                }
        return false;
    }

    // Critically damped follow (SmoothDamp). Smooth, no overshoot, frame-rate independent.
    FVector SmoothDamp(const FVector& Current, const FVector& Target, FVector& Velocity, float SmoothTime, float MaxSpeed, float DeltaTime)
    {
        SmoothTime = FMath::Max(.0001f, SmoothTime);
        const float Omega = 2.f / SmoothTime;
        const float X = Omega * DeltaTime;
        const float Exp = 1.f / (1.f + X + .48f * X * X + .235f * X * X * X);
        FVector Change = Current - Target;
        Change = Change.GetClampedToMaxSize(MaxSpeed * SmoothTime);
        const FVector Goal = Current - Change;
        const FVector Temp = (Velocity + Omega * Change) * DeltaTime;
        Velocity = (Velocity - Omega * Temp) * Exp;
        FVector Output = Goal + (Change + Temp) * Exp;
        if (FVector::DotProduct(Target - Current, Output - Target) > 0.f) { Output = Target; Velocity = FVector::ZeroVector; }
        return Output;
    }
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
#if !UE_BUILD_SHIPPING
    // Automated checks assert an immediate release/throw on the same frame.
    if (FString(FCommandLine::Get()).Contains(TEXT("Smoke"))) bAnimatedTransitions = false;
#endif

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
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, HeldBody);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, bDraggingHeavy);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, CarrySize);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, CarryPhase);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, HeldMass);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, InitialBend);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, SetDownBend);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, bTwoHandedGrip);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, LocalRightContact);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, LocalLeftContact);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, LocalRightNormal);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, LocalLeftNormal);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, LocalFingerDirection);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, LocalFaceNormal);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, bBasketballCharging);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, BasketballChargeStart);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, BasketballFeedback);
    DOREPLIFETIME(UHouseFurnitureInteractionComponent, BasketballFeedbackAt);
}

ESSOCarrySize UHouseFurnitureInteractionComponent::ClassifyFurniture(const UPrimitiveComponent* Body, const AActor* Furniture)
{
    if (const AHouseInteractiveProp* Prop = Cast<AHouseInteractiveProp>(Furniture))
        if (Prop->CarrySize != ESSOCarrySize::Auto) return Prop->CarrySize;
    if (!Body) return ESSOCarrySize::Medium;
    const float Mass = Body->IsSimulatingPhysics() ? Body->GetMass()
        : (Cast<AHouseInteractiveProp>(Furniture) ? Cast<AHouseInteractiveProp>(Furniture)->MassKg : 15.f);
    const FVector E = Body->Bounds.BoxExtent;
    const float Footprint = FMath::Max(E.X, E.Y);
    // A large footprint matters even when a light asset was given a low mass.
    if (Mass >= 65.f || Footprint > 110.f || (Footprint > 75.f && Mass >= 35.f)) return ESSOCarrySize::Large;
    if (Mass <= 12.f && Footprint <= 40.f && E.Z <= 45.f) return ESSOCarrySize::Small;
    return ESSOCarrySize::Medium;
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

void UHouseFurnitureInteractionComponent::ThrowHeldFurniture()
{
    if (!GetOwner()) return;
    if (HeldBody && Cast<AHouseWoodAxe>(HeldBody->GetOwner()))
    {
        if (GetOwner()->HasAuthority()) Cast<AHouseWoodAxe>(HeldBody->GetOwner())->BeginSwing(Cast<AStoryFirstPersonCharacter>(GetOwner()));
        else ServerThrowHeldFurniture();
        return;
    }
    if (BeginBasketballShot()) return;
    if (GetOwner()->HasAuthority()) ThrowHeldFurnitureAuthoritative();
    else ServerThrowHeldFurniture();
}

float UHouseFurnitureInteractionComponent::ShotClock() const
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
}
float UHouseFurnitureInteractionComponent::GetBasketballPower() const
{ return bBasketballCharging ? FMath::Clamp((ShotClock() - BasketballChargeStart) / AHouseBasketball::ChargeSeconds, 0.f, 1.f) : 0; }
float UHouseFurnitureInteractionComponent::GetBasketballIdealPower() const
{
    const auto* Ball = HeldBody ? Cast<AHouseBasketball>(HeldBody->GetOwner()) : nullptr;
    FVector View; FRotator Aim;
    return Ball && GetServerView(View, Aim) ? Ball->SuggestedPower(View, Aim) : -1;
}
FString UHouseFurnitureInteractionComponent::GetBasketballFeedback() const
{ return ShotClock() - BasketballFeedbackAt < 1.8f ? BasketballFeedback : FString(); }
bool UHouseFurnitureInteractionComponent::BeginBasketballShot()
{
    if (!bHoldingFurniture || !HeldBody || !Cast<AHouseBasketball>(HeldBody->GetOwner())) return false;
    if (bBasketballCharging || CarryPhase == ESSOCarryPhase::SettingDown || CarryPhase == ESSOCarryPhase::ThrowWindup) return true;
    if (GetOwner()->HasAuthority()) BasketballInputAuthoritative(true, false, FRotator::ZeroRotator);
    else
    {
        bBasketballCharging = true; BasketballChargeStart = ShotClock();
        ServerBasketballInput(true, false, FRotator::ZeroRotator);
    }
    return true;
}
void UHouseFurnitureInteractionComponent::ReleaseBasketballShot()
{
    // A very quick tap can arrive before the server's charge state replicates.
    // Still send its release after the reliable press, rather than leaving it held.
    const bool PendingClientPress = GetOwner() && !GetOwner()->HasAuthority() && bHoldingFurniture
        && HeldBody && Cast<AHouseBasketball>(HeldBody->GetOwner());
    if (!bBasketballCharging && !PendingClientPress) return;
    FVector View; FRotator Aim;
    if (!GetServerView(View, Aim)) { CancelBasketballShot(); return; }
    if (GetOwner()->HasAuthority()) BasketballInputAuthoritative(false, false, Aim);
    else { bBasketballCharging = false; ServerBasketballInput(false, false, Aim); }
}
void UHouseFurnitureInteractionComponent::CancelBasketballShot()
{
    if (!bBasketballCharging) return;
    bBasketballCharging = false; PendingBasketballPower = -1;
    if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
    else ServerBasketballInput(false, true, FRotator::ZeroRotator);
}
void UHouseFurnitureInteractionComponent::ServerBasketballInput_Implementation(bool Begin, bool Cancel, FRotator Aim)
{ BasketballInputAuthoritative(Begin, Cancel, Aim); }
void UHouseFurnitureInteractionComponent::BasketballInputAuthoritative(bool Begin, bool Cancel, const FRotator& Aim)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (Cancel) { bBasketballCharging = false; PendingBasketballPower = -1; GetOwner()->ForceNetUpdate(); return; }
    if (!bHoldingFurniture || !HeldBody || !Cast<AHouseBasketball>(HeldBody->GetOwner())
        || CarryPhase == ESSOCarryPhase::SettingDown || CarryPhase == ESSOCarryPhase::ThrowWindup) return;
    if (Begin)
    {
        if (bBasketballCharging) return;
        bBasketballCharging = true; BasketballChargeStart = ShotClock(); GetOwner()->ForceNetUpdate(); return;
    }
    if (!bBasketballCharging || Aim.ContainsNaN()) return;
    PendingBasketballPower = GetBasketballPower(); PendingBasketballAim = Aim.GetNormalized();
    const float Ideal = GetBasketballIdealPower();
    BasketballFeedback = Ideal < 0 ? TEXT("FREE THROW")
        : FMath::Abs(PendingBasketballPower - Ideal) <= AHouseBasketball::GreenHalfWidth ? TEXT("GREEN RELEASE")
        : PendingBasketballPower < Ideal ? TEXT("EARLY - SHORT") : TEXT("LATE - LONG");
    BasketballFeedbackAt = ShotClock(); bBasketballCharging = false;
    // Release on this input frame, not after a second delayed furniture wind-up.
    ExecuteThrowAuthoritative(); PendingBasketballPower = -1;
}

AActor* UHouseFurnitureInteractionComponent::FocusedFurnitureForInteraction() const
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    FVector ViewLocation;
    FRotator ViewRotation;
    if (!Owner || !World || bHoldingFurniture || !GetServerView(ViewLocation, ViewRotation)) return nullptr;

    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Owner);
    Query.bTraceComplex = false;
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, ViewLocation,
        ViewLocation + ViewRotation.Vector() * GrabRange, ECC_Visibility, Query)) return nullptr;
    AActor* Furniture = Hit.GetActor();
    UPrimitiveComponent* Body = Hit.GetComponent();
    const bool Available=IsValid(Furniture) && IsValid(Body) && Furniture->ActorHasTag(MovableFurnitureTag)
        && Furniture->GetRootComponent() == Body
        && (Body->IsSimulatingPhysics() || (Cast<AHouseInteractiveProp>(Furniture) && Cast<AHouseInteractiveProp>(Furniture)->CanBeMoved()))
        && !Body->ComponentHasTag(HeldFurnitureTag)
        && FVector::Dist(ViewLocation, Hit.ImpactPoint) <= GrabRange;
    FHitResult Surface;
    return Available && ResolveFurnitureSurface(Body,Hit.ImpactPoint,ViewLocation,
        FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y),ViewRotation.Vector(),GrabRange,Surface)
        ?Furniture:nullptr;
}

void UHouseFurnitureInteractionComponent::ServerToggleGrab_Implementation()
{
    ToggleGrabAuthoritative();
}

void UHouseFurnitureInteractionComponent::ServerReleaseGrab_Implementation()
{
    ReleaseGrabAuthoritative();
}

void UHouseFurnitureInteractionComponent::ServerThrowHeldFurniture_Implementation()
{
    ThrowHeldFurnitureAuthoritative();
}

bool UHouseFurnitureInteractionComponent::GetServerView(FVector& OutLocation, FRotator& OutRotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (!Pawn || !Pawn->GetController()) return false;
    if (const AStoryFirstPersonCharacter* Character = Cast<AStoryFirstPersonCharacter>(Pawn);
        Character && Character->FirstPersonCamera)
    {
        // Read this frame's camera transform. A cached camera-manager location
        // can still point into the previous module immediately after a loop.
        OutLocation = Character->FirstPersonCamera->GetComponentLocation();
        OutRotation = Character->GetBaseAimRotation();
    }
    else Pawn->GetController()->GetPlayerViewPoint(OutLocation, OutRotation);
    return !OutLocation.ContainsNaN() && !OutRotation.ContainsNaN();
}

// ---------------------------------------------------------------------------
// Timing. All derived from replicated values so every machine animates alike.
// ---------------------------------------------------------------------------
float UHouseFurnitureInteractionComponent::BraceDuration() const
{
    if (!bAnimatedTransitions) return .05f;
    return FMath::Clamp(.55f + HeldMass / 300.f, .6f, 1.15f);
}

float UHouseFurnitureInteractionComponent::LiftDuration() const
{
    switch (CarrySize)
    {
    case ESSOCarrySize::Small: return .42f;
    case ESSOCarrySize::Large: return BraceDuration() + .5f;
    default: return FMath::Clamp(.95f + HeldMass / 130.f, .95f, 1.5f);
    }
}

float UHouseFurnitureInteractionComponent::WindupDuration() const
{
    return CarrySize == ESSOCarrySize::Small ? .2f : (CarrySize == ESSOCarrySize::Medium ? .36f : .28f);
}

float UHouseFurnitureInteractionComponent::SetDownDuration() const
{
    return CarrySize == ESSOCarrySize::Small ? .3f : (CarrySize == ESSOCarrySize::Medium ? .55f : .42f);
}

float UHouseFurnitureInteractionComponent::FollowDuration() const
{
    return LastReleasedSize == ESSOCarrySize::Small ? .38f : (LastReleasedSize == ESSOCarrySize::Medium ? .45f : .4f);
}

float UHouseFurnitureInteractionComponent::LargeHeave() const
{
    if (!bHoldingFurniture || CarrySize != ESSOCarrySize::Large) return 0.f;
    const float Brace = BraceDuration();
    float Heave = FMath::SmoothStep(Brace, Brace + .5f, PickupAge);
    if (CarryPhase == ESSOCarryPhase::SettingDown) Heave *= 1.f - FMath::SmoothStep(0.f, SetDownDuration(), PhaseAge);
    return Heave;
}

float UHouseFurnitureInteractionComponent::GetThrowWindup() const
{
    if (bBasketballCharging) return FMath::SmoothStep(0.f, .22f, ShotClock() - BasketballChargeStart) * .75f;
    if (!bHoldingFurniture || CarryPhase != ESSOCarryPhase::ThrowWindup) return 0.f;
    const float T = FMath::Clamp(PhaseAge / WindupDuration(), 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}

float UHouseFurnitureInteractionComponent::GetThrowFollowThrough() const
{
    const float Duration = FollowDuration();
    return FollowAge < Duration ? FMath::Sin(FollowAge / Duration * PI) : 0.f;
}

float UHouseFurnitureInteractionComponent::GetStrain() const
{
    if (!bHoldingFurniture) return 0.f;
    const float Wind = GetThrowWindup();
    float Strain = 0.f;
    switch (CarrySize)
    {
    case ESSOCarrySize::Large:
    {
        const float Brace = BraceDuration();
        Strain = FMath::SmoothStep(0.f, .35f, PickupAge) * (1.f - .55f * FMath::SmoothStep(Brace + .2f, Brace + .9f, PickupAge))
            + .2f * MoveAlpha + .3f * Wind;
        break;
    }
    case ESSOCarrySize::Medium:
    {
        const float Lift = LiftDuration();
        Strain = .6f * FMath::SmoothStep(.2f, .5f, PickupAge) * (1.f - FMath::SmoothStep(Lift - .1f, Lift + .4f, PickupAge))
            + .25f + .3f * Wind;
        break;
    }
    default: Strain = .5f * Wind; break;
    }
    return FMath::Clamp(Strain, 0.f, 1.f);
}

FVector UHouseFurnitureInteractionComponent::GetCameraEffortOffset() const
{
    FVector Offset = FVector::ZeroVector;
    const float T = EffortClock;
    const ESSOCarrySize Size = bHoldingFurniture ? CarrySize : LastReleasedSize;
    if (bHoldingFurniture)
    {
        const float Step = FMath::Sin(2.f * WalkPhase), Sway = FMath::Sin(WalkPhase);
        if (CarrySize == ESSOCarrySize::Large)
        {
            const float Brace = BraceDuration();
            const float Tremble = GetStrain() * (PickupAge < Brace + .3f ? .35f : .12f);
            Offset += FVector(FMath::Sin(T * 37.f) * .6f, FMath::Sin(T * 29.f + 1.f) * .5f, FMath::Sin(T * 43.f + 2.f) * .7f) * Tremble;
            // Dip into the heave, then come back up as the side leaves the floor.
            const float Heave = FMath::SmoothStep(Brace, Brace + .5f, PickupAge);
            Offset.Z -= 3.5f * FMath::Sin(Heave * PI);
            // Each step is a tug: lurch forward and sink slightly.
            Offset.X += Step * 1.3f * MoveAlpha;
            Offset.Z -= FMath::Abs(Sway) * 1.1f * MoveAlpha;
        }
        else if (CarrySize == ESSOCarrySize::Medium)
        {
            Offset.Z += Step * 1.f * MoveAlpha - .6f * MoveAlpha;
            Offset.Y += Sway * .7f * MoveAlpha;
            Offset += FVector(0.f, FMath::Sin(T * 31.f) * .15f, FMath::Sin(T * 41.f) * .2f) * (PickupAge < LiftDuration() ? GetStrain() : 0.f);
        }
        else Offset.Z += Step * .35f * MoveAlpha;
    }
    const float Wind = GetThrowWindup(), Follow = GetThrowFollowThrough();
    const float Lunge = Size == ESSOCarrySize::Small ? 2.5f : (Size == ESSOCarrySize::Medium ? 4.5f : 5.5f);
    Offset.X += -Lunge * .6f * Wind + Lunge * Follow;
    Offset.Z -= (Size == ESSOCarrySize::Medium ? 2.f : .8f) * Wind;
    return Offset;
}

// ---------------------------------------------------------------------------
// Pick up
// ---------------------------------------------------------------------------
void UHouseFurnitureInteractionComponent::ToggleGrabAuthoritative()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !Owner->HasAuthority() || !World || !IsValid(PhysicsHandle)) return;

    if (bHoldingFurniture)
    {
        if (auto* Axe = HeldBody ? Cast<AHouseWoodAxe>(HeldBody->GetOwner()) : nullptr)
        {
            if (Axe->CanReturnToStump(Cast<AStoryFirstPersonCharacter>(Owner)))
            { ReleaseGrabAuthoritative(); Axe->EmbedInStump(); return; }
            Axe->CancelSwing();
        }
        BeginSetDownAuthoritative();
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
        || Body->ComponentHasTag(HeldFurnitureTag)) return;
    if (AHouseInteractiveProp* Prop = Cast<AHouseInteractiveProp>(Furniture))
    {
        if (auto* Axe = Cast<AHouseWoodAxe>(Prop)) Axe->PreparePickup();
        if (!Prop->CanBeMoved() || !Prop->WakeForInteraction(Owner)) return;
    }
    if (!Body->IsSimulatingPhysics()) return;

    // Actor movement replication follows the root body's transform. The setup
    // tool must make the draggable mesh the root and enable Visibility collision.
    if (Furniture->GetRootComponent() != Body || FVector::Dist(ViewLocation, Hit.ImpactPoint) > GrabRange) return;

    Furniture->SetReplicates(true);
    Furniture->SetReplicateMovement(true);
    Furniture->ForceNetUpdate();

    HeldMass = Body->GetMass();
    const FVector Extent = Body->Bounds.BoxExtent;
    CarrySize = ClassifyFurniture(Body, Furniture);
    bDraggingHeavy = CarrySize == ESSOCarrySize::Large;
    HeldRotation = StartingBodyRotation = Body->GetComponentRotation();
    StartingViewYaw=ViewRotation.Yaw;
    const FVector Flat = Flatten(ViewDirection, Owner->GetActorForwardVector());

    switch (CarrySize)
    {
    case ESSOCarrySize::Large: HoldDistance = 54.f; break;
    case ESSOCarrySize::Small:
    {
        // Held by the centre of mass (palm on the near face), so the
        // distance is to the middle of the piece.
        const float DepthHalf = FMath::Abs(Flat.X) * Extent.X + FMath::Abs(Flat.Y) * Extent.Y;
        HoldDistance = FMath::Clamp(DepthHalf + 34.f, 46.f, 85.f);
        break;
    }
    default:
    {
        // Keep the near face of the piece clear of the camera at chest height.
        const float DepthHalf = FMath::Abs(Flat.X) * Extent.X + FMath::Abs(Flat.Y) * Extent.Y;
        HoldDistance = FMath::Clamp(DepthHalf + 24.f, FMath::Min(MinimumHoldDistance, 50.f), 95.f);
        break;
    }
    }
    FVector SurfaceGrip;
    if(!CaptureHandContacts(Body,Hit.ImpactPoint,ViewLocation,FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y),ViewDirection,SurfaceGrip))return;
    // Drive the rigid body at its mass centre; the separately captured palm
    // stays on the handle. This avoids a long tool levering its carry spring.
    if (Cast<AHouseWoodAxe>(Furniture)) SurfaceGrip = Body->GetCenterOfMass();
    HeldLocalGrip=Body->GetComponentTransform().InverseTransformPosition(SurfaceGrip);
    PickupGripStart = SmoothedTarget = SurfaceGrip;
    SmoothedOffset = SurfaceGrip - ViewLocation;
    LeadVelocity = FVector::ZeroVector;
    SpringVelocity = FVector::ZeroVector;
    float SupportBottom = Body->Bounds.Origin.Z - Extent.Z;
    if (!GetFurnitureSupportBottom(Body, SupportBottom) && bDraggingHeavy)
    {
        // One pickup-time fallback for an unsupported collision shape. Do not
        // introduce per-frame mesh traces or depend on editor-only render data.
        FVector SupportPoint;
        const FVector Below = Body->Bounds.Origin - FVector(0, 0, Extent.Z + 100000.f);
        if (Body->GetClosestPointOnCollision(Below, SupportPoint) < 0.f) return;
        SupportBottom = SupportPoint.Z;
    }
    GripHeightAboveBottom = FMath::Max(0.f, SurfaceGrip.Z - SupportBottom);
    InitialBend = FMath::Clamp(ViewLocation.Z-SurfaceGrip.Z-44.f,0.f,105.f);
    SetDownBend = 0.f;
    PickupAge = 0.f; PhaseAge = 0.f; FollowAge = 100.f;
    SavedLinearDamping = Body->GetLinearDamping(); SavedAngularDamping = Body->GetAngularDamping();
    Body->SetEnableGravity(true);
    struct FTune { float BodyLinear, BodyAngular, Stiff, Damp, AngStiff, AngDamp, Interp; };
    const FTune Tune = CarrySize == ESSOCarrySize::Large ? FTune{2.4f, 4.0f, 1100.f, 300.f, 600.f, 200.f, 7.f}
        : CarrySize == ESSOCarrySize::Medium ? FTune{1.4f, 3.0f, 2600.f, 220.f, 1600.f, 160.f, 13.f}
        : FTune{1.0f, 2.4f, 3200.f, 200.f, 2200.f, 180.f, 20.f};
    Body->SetLinearDamping(Tune.BodyLinear);
    Body->SetAngularDamping(Tune.BodyAngular);
    PhysicsHandle->SetLinearStiffness(Tune.Stiff);
    PhysicsHandle->SetLinearDamping(Tune.Damp);
    PhysicsHandle->SetAngularStiffness(Tune.AngStiff);
    PhysicsHandle->SetAngularDamping(Tune.AngDamp);
    PhysicsHandle->SetInterpolationSpeed(Tune.Interp);
    // Small/medium targets are already smoothed in the player's frame; a second
    // world-space interpolation would make the piece trail behind while walking.
    PhysicsHandle->bInterpolateTarget = false;
    // The holder's own capsule must not shove or tilt the piece in their hands.
    SavedPawnResponse = Body->GetCollisionResponseToChannel(ECC_Pawn);
    Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    PhysicsHandle->GrabComponentAtLocationWithRotation(Body,NAME_None,SurfaceGrip,HeldRotation);
    if (PhysicsHandle->GetGrabbedComponent() != Body) { Body->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse); return; }
    if (Cast<AHouseWoodAxe>(Furniture))
    {
        // An end-gripped tool needs orientation support from the wrist. The
        // generic centre-of-mass furniture drive otherwise lets its head sag.
        Body->SetEnableGravity(false);
        PhysicsHandle->SetLinearStiffness(50000.f); PhysicsHandle->SetLinearDamping(1000.f);
        PhysicsHandle->SetAngularStiffness(80000.f); PhysicsHandle->SetAngularDamping(3000.f);
    }
    Body->ComponentTags.AddUnique(HeldFurnitureTag);
    HeldActor = Furniture; HeldComponent = Body; HeldBody = Body;
    bHoldingFurniture = true;
    CarryPhase = ESSOCarryPhase::Lifting;
    SetComponentTickEnabled(true);
    Owner->ForceNetUpdate();
}

void UHouseFurnitureInteractionComponent::OnRep_HoldingFurniture()
{
    PickupAge=0.f;
    if (bHoldingFurniture) FollowAge = 100.f;
    SetComponentTickEnabled(true);
}

void UHouseFurnitureInteractionComponent::OnRep_CarryPhase()
{
    PhaseAge = 0.f;
    if (CarryPhase == ESSOCarryPhase::Thrown) StartFollowThrough();
    SetComponentTickEnabled(true);
}

void UHouseFurnitureInteractionComponent::StartFollowThrough()
{
    FollowAge = 0.f;
    LastReleasedSize = CarrySize;
    if (const APawn* Pawn = Cast<APawn>(GetOwner())) ThrowDirection = Pawn->GetBaseAimRotation().Vector().GetSafeNormal();
    if (CarrySize == ESSOCarrySize::Large) ThrowDirection = Flatten(ThrowDirection, ThrowDirection);
    SetComponentTickEnabled(true);
}

bool UHouseFurnitureInteractionComponent::CaptureHandContacts(UPrimitiveComponent* Body,const FVector& Point,const FVector& ViewLocation,
    const FVector& ViewRight,const FVector& ViewForward,FVector& OutSurface)
{
    const FTransform Transform=Body->GetComponentTransform();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FurniturePalm),true);
    const float TraceDepth=FMath::Max(280.f,Body->Bounds.SphereRadius*2.f+20.f);
    auto Trace=[&](const FVector& Candidate,FHitResult& Hit)
    { return Body->LineTraceComponent(Hit,Candidate-ViewForward*TraceDepth,Candidate+ViewForward*TraceDepth,Query); };
    FHitResult Primary;
    if(!ResolveFurnitureSurface(Body,Point,ViewLocation,ViewRight,ViewForward,GrabRange,Primary))return false;
    OutSurface=Primary.ImpactPoint;
    FVector RightPoint=Primary.ImpactPoint,LeftPoint=Primary.ImpactPoint;
    FVector RightNormal=Primary.ImpactNormal,LeftNormal=Primary.ImpactNormal;
    FVector FingerWorld=FVector::ZeroVector, FaceWorld=FVector::ZeroVector;
    bTwoHandedGrip=false;
    bool bShaped=false;
    if(Cast<AHouseBasketball>(Body->GetOwner()))
    {
        // Cup the lower side of the sphere instead of gripping its upper face.
        // This keeps the ball above the palm and visible while aiming.
        FVector PalmSide=(ViewRight*.6f-FVector::UpVector*.8f).GetSafeNormal();
        RightPoint=LeftPoint=Body->GetCenterOfMass()+PalmSide*Body->Bounds.BoxExtent.GetMax();
        RightNormal=LeftNormal=PalmSide;
        FingerWorld=Flatten(ViewForward,FVector::ForwardVector);
        bShaped=true;
    }
    else if(CarrySize==ESSOCarrySize::Large)
    {
        bShaped=CaptureLargeTopGrip(Body,Primary,ViewRight,ViewForward,RightPoint,LeftPoint,RightNormal,LeftNormal,FaceWorld,FaceSpan);
        if(bShaped)
        {
            bTwoHandedGrip=true;
            OutSurface=(RightPoint+LeftPoint)*.5f;
            FingerWorld=-FaceWorld; // fingers hook over the lip, into the piece
        }
    }
    else if(CarrySize==ESSOCarrySize::Medium)
    {
        bShaped=CaptureMediumSideGrip(Body,Primary,ViewForward,RightPoint,LeftPoint,RightNormal,LeftNormal);
        if(bShaped)
        {
            bTwoHandedGrip=true;
            // Grab between the palms: steadier than one face point, and the
            // piece no longer pivots around a single front contact.
            OutSurface=(RightPoint+LeftPoint)*.5f;
            FingerWorld=(Flatten(ViewForward,FVector::ForwardVector)-FVector::UpVector*.35f).GetSafeNormal();
        }
    }
    if(CarrySize==ESSOCarrySize::Small)
    {
        // One palm on the selected surface; physics holds the centre of mass so
        // the piece does not swing down around the fingertips.
        OutSurface=Body->GetCenterOfMass();
    }
    if (Cast<AHouseWoodAxe>(Body->GetOwner()))
    {
        // Grip the wooden handle, not the steel head or the clicked surface.
        OutSurface = RightPoint = LeftPoint = Transform.TransformPosition(AHouseWoodAxe::GripLocal());
        RightNormal = LeftNormal = Transform.TransformVectorNoScale(FVector(0, -1, 0));
        FingerWorld = Transform.GetUnitAxis(EAxis::Z); bTwoHandedGrip = false;
    }
    if(!bShaped && CarrySize!=ESSOCarrySize::Small)
    {
        float Span=FMath::Clamp(Body->Bounds.BoxExtent.Size2D()*.25f,4.f,21.f);
        // Fallback for odd shapes: two palms on the selected face, closing in
        // toward the selected point until both land on real geometry.
        for(int32 Attempt=0;Attempt<5 && Span>=3.f;++Attempt,Span*=.5f)
        {
            FHitResult R,L;
            if(Trace(Primary.ImpactPoint+ViewRight*Span,R) && Trace(Primary.ImpactPoint-ViewRight*Span,L)
                && FVector::DistSquared(R.ImpactPoint,Primary.ImpactPoint)<=FMath::Square(Span*2.f+8.f)
                && FVector::DistSquared(L.ImpactPoint,Primary.ImpactPoint)<=FMath::Square(Span*2.f+8.f)
                && FMath::Abs(FVector::DotProduct(R.ImpactPoint-Primary.ImpactPoint,ViewForward))<=8.f
                && FMath::Abs(FVector::DotProduct(L.ImpactPoint-Primary.ImpactPoint,ViewForward))<=8.f)
            {
                RightPoint=R.ImpactPoint;LeftPoint=L.ImpactPoint;
                RightNormal=R.ImpactNormal;LeftNormal=L.ImpactNormal;
                bTwoHandedGrip=true;break;
            }
        }
        if(CarrySize==ESSOCarrySize::Large)
        {
            FaceWorld=Flatten(Primary.ImpactNormal,-Flatten(ViewForward,FVector::ForwardVector));
            FaceSpan=FMath::Max(40.f,2.f*(FMath::Abs(FaceWorld.X)*Body->Bounds.BoxExtent.X+FMath::Abs(FaceWorld.Y)*Body->Bounds.BoxExtent.Y));
        }
    }
    LocalRightContact=Transform.InverseTransformPosition(RightPoint);
    LocalLeftContact=Transform.InverseTransformPosition(LeftPoint);
    LocalRightNormal=Transform.InverseTransformVectorNoScale(RightNormal).GetSafeNormal();
    LocalLeftNormal=Transform.InverseTransformVectorNoScale(LeftNormal).GetSafeNormal();
    LocalFingerDirection=FingerWorld.IsNearlyZero()?FVector::ZeroVector:Transform.InverseTransformVectorNoScale(FingerWorld).GetSafeNormal();
    LocalFaceNormal=FaceWorld.IsNearlyZero()?FVector::ZeroVector:Transform.InverseTransformVectorNoScale(FaceWorld).GetSafeNormal();
    UE_LOG(LogTemp,Display,TEXT("FURNITURE_SURFACE_GRIP body=%s size=%d shaped=%d twoHands=%d separation=%.2f right=%s left=%s scale=%s"),
        *GetNameSafe(Body),static_cast<int32>(CarrySize),bShaped,bTwoHandedGrip,FVector::Dist(RightPoint,LeftPoint),
        *RightPoint.ToString(),*LeftPoint.ToString(),*Transform.GetScale3D().ToString());
    return true;
}

bool UHouseFurnitureInteractionComponent::GetHandContact(bool Right,FVector& Position,FVector& Normal) const
{
    if(bHoldingFurniture && IsValid(HeldBody))
    {
        const FTransform T=HeldBody->GetComponentTransform();
        Position=T.TransformPosition(Right?LocalRightContact:LocalLeftContact);
        Normal=T.TransformVectorNoScale(Right?LocalRightNormal:LocalLeftNormal).GetSafeNormal();
        return true;
    }
    Position=Right?LastRightContact:LastLeftContact;
    Normal=Right?LastRightNormal:LastLeftNormal;
    const float Duration=FollowDuration();
    if(FollowAge<Duration)
    {
        // Follow-through: the empty hands keep travelling along the throw,
        // then drop away while the carry pose fades out.
        const float F=FollowAge/Duration;
        const float Out=FMath::Sin(FMath::Min(F*1.6f,1.f)*HALF_PI);
        const float Reach=LastReleasedSize==ESSOCarrySize::Small?38.f:(LastReleasedSize==ESSOCarrySize::Medium?22.f:24.f);
        const float Drop=LastReleasedSize==ESSOCarrySize::Small?20.f:(LastReleasedSize==ESSOCarrySize::Medium?14.f:4.f);
        Position+=ThrowDirection*Reach*Out-FVector::UpVector*Drop*F*F;
    }
    return CarryPoseBlend>.001f;
}

bool UHouseFurnitureInteractionComponent::GetHandFingerDirection(bool Right,FVector& OutFingers) const
{
    if(bHoldingFurniture && IsValid(HeldBody))
    {
        if(LocalFingerDirection.IsNearlyZero())return false;
        OutFingers=HeldBody->GetComponentTransform().TransformVectorNoScale(LocalFingerDirection).GetSafeNormal();
        return true;
    }
    OutFingers=Right?LastRightFingers:LastLeftFingers;
    return !OutFingers.IsNearlyZero() && CarryPoseBlend>.001f;
}

float UHouseFurnitureInteractionComponent::GetPickupBend() const
{
    if(!bHoldingFurniture)return 0.f;
    float Bend=0.f;
    switch(CarrySize)
    {
    case ESSOCarrySize::Large:
        // Stay low through the brace, rise a little once the side is up.
        Bend=InitialBend*FMath::SmoothStep(0.f,.45f,PickupAge)*(1.f-.25f*LargeHeave());
        break;
    case ESSOCarrySize::Medium:
        Bend=InitialBend*FMath::SmoothStep(0.f,.3f,PickupAge)*(1.f-FMath::SmoothStep(.45f,LiftDuration()+.2f,PickupAge));
        break;
    default:
        Bend=InitialBend*FMath::SmoothStep(0.f,.14f,PickupAge)*(1.f-FMath::SmoothStep(.22f,.62f,PickupAge));
        break;
    }
    if(CarryPhase==ESSOCarryPhase::SettingDown && CarrySize!=ESSOCarrySize::Large)
        Bend=FMath::Max(Bend,SetDownBend*FMath::SmoothStep(0.f,.75f,PhaseAge/SetDownDuration()));
    return Bend;
}

float UHouseFurnitureInteractionComponent::GetMovementScale() const
{
    if(!bHoldingFurniture)
        return FollowAge<.3f && LastReleasedSize!=ESSOCarrySize::Small ? .7f : 1.f;
    switch(CarrySize)
    {
    case ESSOCarrySize::Large:
        // Rooted while bracing and heaving; then a slow, heavy drag.
        if(PickupAge<BraceDuration()+.3f)return .08f;
        if(CarryPhase==ESSOCarryPhase::ThrowWindup)return .15f;
        if(CarryPhase==ESSOCarryPhase::SettingDown)return .2f;
        {
            // Tethered: if the piece falls behind the hands, the player slows to it.
            float Tether=1.f;
            if(GetOwner())
            {
                FVector R,L,N;GetHandContact(true,R,N);GetHandContact(false,L,N);
                const float Reach=FVector::Dist2D(GetOwner()->GetActorLocation(),(R+L)*.5f);
                Tether=FMath::Clamp(1.f-(Reach-68.f)/24.f,.2f,1.f);
            }
            return FMath::Clamp(.5f-HeldMass/700.f,.26f,.45f)*Tether;
        }
    case ESSOCarrySize::Medium:
        if(CarryPhase==ESSOCarryPhase::ThrowWindup)return .4f;
        if(CarryPhase==ESSOCarryPhase::SettingDown)return .35f;
        if(PickupAge<LiftDuration())return .3f;
        return FMath::Clamp(.74f-HeldMass/220.f,.52f,.7f);
    default:
        return CarryPhase==ESSOCarryPhase::ThrowWindup?.9f:1.f;
    }
}

float UHouseFurnitureInteractionComponent::GetPickupLean() const
{
    if(!bHoldingFurniture || !bDraggingHeavy || !GetOwner())return 0.f;
    FVector Right,Left,Normal;
    GetHandContact(true,Right,Normal);GetHandContact(false,Left,Normal);
    const float Distance=FVector::Dist2D(GetOwner()->GetActorLocation(),(Right+Left)*.5f);
    // Lean over the capsule's collision margin to reach a grounded piece.
    // Keep the viewpoint at least 35 cm back from the selected surface.
    return FMath::Clamp(Distance-35.f,0.f,24.f)*FMath::SmoothStep(.12f,.65f,PickupAge);
}

// ---------------------------------------------------------------------------
// Per frame
// ---------------------------------------------------------------------------
void UHouseFurnitureInteractionComponent::TickComponent(float DeltaTime,ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    PickupAge+=DeltaTime; PhaseAge+=DeltaTime; FollowAge+=DeltaTime; EffortClock+=DeltaTime;
    if (bHoldingFurniture && HeldBody)
    {
        SfxHeldLocation = HeldBody->GetComponentLocation();
        if (const AHouseInteractiveProp* HeldProp = Cast<AHouseInteractiveProp>(HeldBody->GetOwner())) SfxHeldSurface = uint8(HeldProp->SfxSurface);
    }
    if (bHoldingFurniture != bSfxWasHolding)
    {
        bSfxWasHolding = bHoldingFurniture;
        const FVector At = HeldBody ? HeldBody->GetComponentLocation() : SfxHeldLocation;
        if (bHoldingFurniture) HouseSfx::Handle(GetWorld(), At, 0, EHouseSurface(SfxHeldSurface), HeldMass);
        else if (CarryPhase != ESSOCarryPhase::Thrown) HouseSfx::Handle(GetWorld(), At, 1, EHouseSurface(SfxHeldSurface), HeldMass);
    }
    if (CarryPhase != SfxLastPhase)
    {
        if (CarryPhase == ESSOCarryPhase::Thrown) HouseSfx::Handle(GetWorld(), HeldBody ? HeldBody->GetComponentLocation() : SfxHeldLocation, 2, EHouseSurface(SfxHeldSurface), HeldMass);
        SfxLastPhase = CarryPhase;
    }
    if(const AActor* Owner=GetOwner())
    {
        const float Speed=Owner->GetVelocity().Size2D();
        MoveAlpha=FMath::FInterpTo(MoveAlpha,FMath::Clamp(Speed/220.f,0.f,1.f),DeltaTime,6.f);
        // One half cycle per footstep; heavier pieces take shorter steps.
        WalkPhase=FMath::Fmod(WalkPhase+DeltaTime*Speed/(CarrySize==ESSOCarrySize::Large?55.f:72.f)*PI,2.f*PI);
    }
    const bool bFollowing=FollowAge<FollowDuration();
    const float PoseTarget=bHoldingFurniture||(bFollowing && FollowAge<FollowDuration()*.55f)?1.f:0.f;
    const float PoseSpeed=bHoldingFurniture
        ?(CarrySize==ESSOCarrySize::Small?11.f:(CarrySize==ESSOCarrySize::Medium?7.f:6.f))
        :(bFollowing?8.f:9.f);
    CarryPoseBlend=FMath::FInterpTo(CarryPoseBlend,PoseTarget,DeltaTime,PoseSpeed);
    float GripTarget=0.f;
    if(bHoldingFurniture)
    {
        FVector Right,Normal,Left;GetHandContact(true,Right,Normal);GetHandContact(false,Left,Normal);
        const auto* Character=Cast<AStoryFirstPersonCharacter>(GetOwner());
        const bool Contact=Character && (Character->GetNetMode()==NM_DedicatedServer
            ? FVector::Dist(Right,Character->GetActorLocation())<170.f
            : FVector::Dist(Right,Character->GetFurnitureHandGripPoint(true))<10.f
                && (!bTwoHandedGrip || FVector::Dist(Left,Character->GetFurnitureHandGripPoint(false))<10.f));
        if(Contact)GripTarget=CarrySize==ESSOCarrySize::Small
            ?FMath::SmoothStep(.06f,.28f,PickupAge):FMath::SmoothStep(.12f,.5f,PickupAge);
    }
    // Fingers snap open at the moment of release, then relax.
    FingerGripBlend=FMath::FInterpTo(FingerGripBlend,GripTarget,DeltaTime,bFollowing && FollowAge<.2f?26.f:12.f);
    if(bHoldingFurniture)
    {
        GetHandContact(true,LastRightContact,LastRightNormal);
        GetHandContact(false,LastLeftContact,LastLeftNormal);
        FVector Fingers;
        if(GetHandFingerDirection(true,Fingers)){LastRightFingers=LastLeftFingers=Fingers;}
        else LastRightFingers=LastLeftFingers=FVector::ZeroVector;
    }
    else
    {
        if(CarryPoseBlend<.002f && !bFollowing){CarryPoseBlend=0;SetComponentTickEnabled(false);}
        return;
    }
    if(!GetOwner() || !GetOwner()->HasAuthority())return;
    AActor* Furniture=HeldActor.Get(); UPrimitiveComponent* Body=HeldComponent.Get();
    FVector ViewLocation; FRotator ViewRotation;
    if(!IsValid(Furniture) || !IsValid(Body) || !Body->IsSimulatingPhysics()
        || !IsValid(PhysicsHandle) || PhysicsHandle->GetGrabbedComponent()!=Body
        || !GetServerView(ViewLocation,ViewRotation)
        || FVector::Dist(ViewLocation,Body->GetComponentTransform().TransformPosition(HeldLocalGrip))>BreakDistance)
    { ReleaseGrabAuthoritative(); return; }

    // Phase transitions.
    if(CarryPhase==ESSOCarryPhase::Lifting && PickupAge>=LiftDuration())
    { CarryPhase=ESSOCarryPhase::Holding; PhaseAge=0.f; GetOwner()->ForceNetUpdate(); }
    if(CarryPhase==ESSOCarryPhase::SettingDown && PhaseAge>=SetDownDuration()+.04f)
    { ReleaseGrabAuthoritative(); return; }
    if(CarryPhase==ESSOCarryPhase::ThrowWindup && PhaseAge>=WindupDuration())
    { ExecuteThrowAuthoritative(); return; }

    const bool bLarge=CarrySize==ESSOCarrySize::Large, bSmall=CarrySize==ESSOCarrySize::Small;
    FRotator CarryAim=ViewRotation;
    CarryAim.Pitch=FMath::Clamp(FRotator::NormalizeAxis(CarryAim.Pitch),bSmall?-40.f:-25.f,bSmall?40.f:25.f);
    const FVector Direction=CarryAim.Vector();
    const FVector Flat=Flatten(Direction,GetOwner()->GetActorForwardVector());
    const FVector RightAxis(-Flat.Y,Flat.X,0.f);
    const FVector Up=FVector::UpVector;
    const float Wind=GetThrowWindup();
    const float Bob=FMath::Sin(2.f*WalkPhase)*MoveAlpha, Sway=FMath::Sin(WalkPhase)*MoveAlpha;
    const float Breath=FMath::Sin(EffortClock*1.7f);
    const FVector ActualGrip=Body->GetComponentTransform().TransformPosition(HeldLocalGrip);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(FurnitureCarryClearance),false);
    Query.AddIgnoredActor(GetOwner());Query.AddIgnoredActor(Furniture);
    FVector Target;
    float Lift=0.f;
    if(bLarge)
    {
        // Brace (rooted, gripping), heave one side up, then drag it.
        // The far side stays on the floor; the held side rides a few cm up.
        const float LiftCm=FMath::Clamp(17.f-HeldMass*.03f,9.f,15.f);
        Lift=LiftCm*LargeHeave()+4.f*Wind;
        if(PickupAge<BraceDuration())Target=PickupGripStart;
        else
        {
            Target=ViewLocation+Flat*(HoldDistance-10.f*Wind+2.5f*Bob);
            Target.Z=ActualGrip.Z;
            FHitResult Ground;
            const FVector Probe(Target.X,Target.Y,ActualGrip.Z+20.f);
            if(GetWorld()->LineTraceSingleByChannel(Ground,Probe,Probe-FVector(0,0,160.f+GripHeightAboveBottom),ECC_Pawn,Query)
                && Ground.ImpactNormal.Z>.65f)
            {
                const float Desired=Ground.ImpactPoint.Z+GripHeightAboveBottom+Lift+1.f;
                // The heave curve already eases the rise; only cap a runaway target.
                Target.Z=FMath::Min(Desired,ActualGrip.Z+25.f);
            }
        }
    }
    else
    {
        FVector Rest;
        if(bSmall)
        {
            // Held out in the right hand, a little low and to the side.
            // Small court props need a visible relaxed grip, not a rest below
            // the viewport. Keep existing basketball and furniture poses intact.
            const float Lower=Cast<AHouseBasketball>(Furniture)?20.f
                :Furniture->ActorHasTag(TEXT("SSOCourtSmallProp"))?12.f:30.f;
            Rest=ViewLocation+Direction*HoldDistance+RightAxis*17.f-Up*Lower+Up*(Bob*1.2f+Breath*.3f);
            // Wind-up: draw back past the shoulder before the release.
            if (Cast<AHouseBasketball>(Furniture) && bBasketballCharging)
                Rest = FMath::Lerp(Rest, ViewLocation + Flat * 38 + RightAxis * 6 + Up * 20,
                    FMath::SmoothStep(0.f, .22f, ShotClock() - BasketballChargeStart));
            else Rest+=(RightAxis*16.f-Direction*12.f+Up*30.f)*Wind;
        }
        else
        {
            // Hugged at chest height with both hands, bouncing with each step.
            Rest=ViewLocation+Direction*HoldDistance-Up*30.f+Up*(Bob*2.f+Breath*.5f)+RightAxis*Sway*1.4f;
            // Wind-up: swing low and back toward the hips.
            Rest+=(-Direction*14.f-Up*13.f)*Wind;
        }
        Target=FMath::Lerp(PickupGripStart,Rest,FMath::SmoothStep(bSmall?.06f:.28f,LiftDuration(),PickupAge));
        if(CarryPhase==ESSOCarryPhase::SettingDown)
            Target=FMath::Lerp(SetDownFrom,SetDownTo,FMath::SmoothStep(0.f,SetDownDuration(),PhaseAge));
    }
    if (auto* Axe = Cast<AHouseWoodAxe>(Furniture); Axe && CarryPhase != ESSOCarryPhase::SettingDown)
    {
        FVector Grip; FQuat Rot; Axe->CarryPose(ViewLocation, ViewRotation, Grip, Rot);
        const FVector MassCentre = Grip + Rot.RotateVector((HeldLocalGrip - AHouseWoodAxe::GripLocal()) * Body->GetComponentScale());
        Target = FMath::Lerp(PickupGripStart, MassCentre, FMath::SmoothStep(.06f, LiftDuration(), PickupAge));
    }
    if(!bLarge)
    {
        // Lead the spring by the player's own velocity. The handle damps toward
        // a still target, which would otherwise leave the piece trailing the
        // palms by damping/stiffness * speed (~25 cm at a jog).
        const FVector PawnVelocity=GetOwner()->GetVelocity();
        // Eased like the body's own response, so starting or stopping a walk
        // never kicks the piece ahead of the palms.
        LeadVelocity=FMath::VInterpTo(LeadVelocity,FVector(PawnVelocity.X,PawnVelocity.Y,0.f),DeltaTime,bSmall?14.f:11.f);
        Target+=LeadVelocity*(bSmall?(200.f/3200.f):(220.f/2600.f));
    }
    const bool bFitReach=PickupAge>.35f && CarryPhase!=ESSOCarryPhase::SettingDown && !(bLarge && PickupAge<BraceDuration());
    if(bFitReach)
        if(const auto* Character=Cast<AStoryFirstPersonCharacter>(GetOwner());Character && Character->FirstPersonArms)
        {
            USkeletalMeshComponent* Arms=Character->FirstPersonArms;
            // Fit the physical carry target to this selected character's real
            // arm length. Leave elbow bend and room for the spring's small sag;
            // a short person never receives a generic unreachable hold point.
            for(int32 Pass=0;Pass<4;++Pass)
                for(bool Right:{true,false})
                {
                    if(!Right && !bTwoHandedGrip)continue;
                    const FName U=Right?TEXT("upperarm_r"):TEXT("upperarm_l");
                    const FName L=Right?TEXT("lowerarm_r"):TEXT("lowerarm_l");
                    const FName H=Right?TEXT("hand_r"):TEXT("hand_l");
                    if(Arms->GetBoneIndex(U)==INDEX_NONE || Arms->GetBoneIndex(L)==INDEX_NONE || Arms->GetBoneIndex(H)==INDEX_NONE)continue;
                    const FVector Shoulder=Arms->GetSocketLocation(U),Elbow=Arms->GetSocketLocation(L),Hand=Arms->GetSocketLocation(H);
                    const float Reach=.85f*(FVector::Dist(Shoulder,Elbow)+FVector::Dist(Elbow,Hand));
                    if(Reach<15.f)continue;
                    FVector Contact,Normal;GetHandContact(Right,Contact,Normal);
                    const FVector Into=Normal;
                    FVector FingerRef=FVector::UpVector;GetHandFingerDirection(Right,FingerRef);
                    FVector Fingers=FVector::VectorPlaneProject(FingerRef,Into).GetSafeNormal();
                    if(Fingers.IsNearlyZero())Fingers=FVector::VectorPlaneProject(FVector::UpVector,Into).GetSafeNormal();
                    if(Fingers.IsNearlyZero())Fingers=FVector::CrossProduct(Into,FVector(0,1,0)).GetSafeNormal();
                    const FQuat Palm=FRotationMatrix::MakeFromXZ(Fingers,Into).ToQuat();
                    const FVector Wrist=Contact+(Target-ActualGrip)-Palm.RotateVector(FVector(6,0,-1.5)*Arms->GetComponentScale());
                    const FVector Correction=Shoulder+(Wrist-Shoulder).GetClampedToMaxSize(Reach)-Wrist;
                    Target+=bLarge?FVector(Correction.X,Correction.Y,0):Correction;
                }
        }
    FHitResult Obstacle;
    if(GetWorld()->SweepSingleByChannel(Obstacle,ViewLocation,Target,FQuat::Identity,ECC_Visibility,
        FCollisionShape::MakeSphere(bLarge?12.f:9.f),Query))
    {
        // Ease away from a wall rather than dropping a piece on one obstructed frame.
        Target=Obstacle.Location+Obstacle.ImpactNormal*6.f;
        if(bLarge)Target.Z=FMath::Min(Target.Z,ActualGrip.Z+30.f*DeltaTime);
    }
    // Critically damped follow: weighty for big pieces, snappy for small ones,
    // and quick during a wind-up so the throw reads as one motion.
    const float SmoothTime=CarryPhase==ESSOCarryPhase::ThrowWindup?.05f:(bLarge?.22f:(bSmall?.06f:.11f));
    const float MaxFollow=bLarge?FMath::Clamp(200.f-HeldMass*.4f,65.f,155.f):(bSmall?600.f:340.f);
    if(bLarge)SmoothedTarget=SmoothDamp(SmoothedTarget,Target,SpringVelocity,SmoothTime,MaxFollow,DeltaTime);
    else
    {
        // Smooth relative to the viewpoint: walking never makes the piece lag,
        // only arm-relative motion (lift, sway, wind-up) is eased.
        SmoothedOffset=SmoothDamp(SmoothedOffset,Target-ViewLocation,SpringVelocity,SmoothTime,MaxFollow,DeltaTime);
        SmoothedTarget=ViewLocation+SmoothedOffset;
    }

    FRotator DesiredRotation=(FQuat(FVector::UpVector,FMath::DegreesToRadians(FRotator::NormalizeAxis(ViewRotation.Yaw-StartingViewYaw)))*StartingBodyRotation.Quaternion()).Rotator();
    if (auto* Axe = Cast<AHouseWoodAxe>(Furniture); Axe && CarryPhase != ESSOCarryPhase::SettingDown)
    {
        FVector Grip; FQuat Rot; Axe->CarryPose(ViewLocation, ViewRotation, Grip, Rot);
        const float AxeLift = FMath::SmoothStep(.06f, LiftDuration(), PickupAge);
        DesiredRotation = FQuat::Slerp(StartingBodyRotation.Quaternion(), Rot, AxeLift).Rotator();
    }
    HeldRotation=FMath::RInterpConstantTo(HeldRotation,DesiredRotation,DeltaTime,Cast<AHouseWoodAxe>(Furniture)?900.f:bLarge?12.f:(bSmall?80.f:55.f));
    FQuat FinalRotation=HeldRotation.Quaternion();
    if(bLarge && !LocalFaceNormal.IsNearlyZero())
    {
        // Tip the piece about its far edge so the held side rises.
        const FVector Face=Flatten(HeldRotation.RotateVector(LocalFaceNormal),Flat);
        const FVector Axis=FVector::CrossProduct(Face,Up).GetSafeNormal();
        const float Angle=FMath::Asin(FMath::Clamp(Lift/FMath::Max(FaceSpan,40.f),0.f,.4f));
        if(!Axis.IsNearlyZero())FinalRotation=FQuat(Axis,Angle)*FinalRotation;
    }
    else if(!bLarge)
    {
        const float Roll=bSmall?0.f:1.6f*Sway;
        const float Tip=bSmall?-12.f*Wind:6.f*Wind;
        FinalRotation=FQuat(Flat,FMath::DegreesToRadians(Roll))*FQuat(RightAxis,FMath::DegreesToRadians(Tip))*FinalRotation;
    }
    DebugTargetError=FVector::Dist(ActualGrip,SmoothedTarget);
    PhysicsHandle->SetTargetLocationAndRotation(SmoothedTarget,FinalRotation.Rotator());
    // Reject violent accumulated spring motion instead of letting a camera
    // turn or a collision launch a sofa or an overhead-held small object.
    const float MaxSpeed=bLarge?200.f:650.f;
    const FVector Velocity=Body->GetPhysicsLinearVelocity();
    if(Velocity.SizeSquared()>FMath::Square(MaxSpeed))Body->SetPhysicsLinearVelocity(Velocity.GetClampedToMaxSize(MaxSpeed));
    const FVector Spin=Body->GetPhysicsAngularVelocityInRadians();
    const float MaxSpin = Cast<AHouseWoodAxe>(Furniture) ? 24.f : 3.5f;
    if(Spin.SizeSquared()>FMath::Square(MaxSpin))Body->SetPhysicsAngularVelocityInRadians(Spin.GetClampedToMaxSize(MaxSpin));
}

void UHouseFurnitureInteractionComponent::TranslateHeldFurniture(const FVector& Delta)
{
    UPrimitiveComponent* Body = HeldComponent.Get();
    if (!bHoldingFurniture || !IsValid(Body) || !IsValid(PhysicsHandle) || !GetOwner()->HasAuthority()) return;
    FVector Target; FRotator Rotation;
    PhysicsHandle->GetTargetLocationAndRotation(Target, Rotation);
    const FVector Velocity = Body->GetPhysicsLinearVelocity();
    const FVector Angular = Body->GetPhysicsAngularVelocityInRadians();
    // Recreate the kinematic anchor at the translated body; retaining its old
    // interpolation anchor would yank furniture back toward the previous copy.
    PhysicsHandle->ReleaseComponent();
    Body->SetWorldLocation(Body->GetComponentLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
    PhysicsHandle->GrabComponentAtLocationWithRotation(Body, NAME_None, Body->GetComponentTransform().TransformPosition(HeldLocalGrip), HeldRotation);
    PhysicsHandle->SetTargetLocationAndRotation(Target + Delta, Rotation);
    PickupGripStart+=Delta;SmoothedTarget+=Delta;SetDownFrom+=Delta;SetDownTo+=Delta;
    LastRightContact+=Delta;LastLeftContact+=Delta;
    Body->SetPhysicsLinearVelocity(Velocity);
    Body->SetPhysicsAngularVelocityInRadians(Angular);
    if (AActor* Furniture = HeldActor.Get()) Furniture->ForceNetUpdate();
}

// ---------------------------------------------------------------------------
// Set down / release
// ---------------------------------------------------------------------------
void UHouseFurnitureInteractionComponent::BeginSetDownAuthoritative()
{
    bBasketballCharging = false; PendingBasketballPower = -1;
    AActor* Owner=GetOwner();
    UPrimitiveComponent* Body=HeldComponent.Get();
    if(!Owner || !bHoldingFurniture)return;
    if(CarryPhase==ESSOCarryPhase::SettingDown || CarryPhase==ESSOCarryPhase::ThrowWindup)return;
    FVector ViewLocation; FRotator ViewRotation;
    if(!bAnimatedTransitions || !IsValid(Body) || !GetServerView(ViewLocation,ViewRotation))
    { ReleaseGrabAuthoritative(); return; }
    const FVector ActualGrip=Body->GetComponentTransform().TransformPosition(HeldLocalGrip);
    if(CarrySize==ESSOCarrySize::Large)
    {
        // Lower the raised side back to the floor, then let go.
        SetDownFrom=SetDownTo=ActualGrip;
        SetDownBend=InitialBend;
    }
    else
    {
        const FVector Flat=Flatten(ViewRotation.Vector(),Owner->GetActorForwardVector());
        const FVector Probe=FVector(ViewLocation.X,ViewLocation.Y,ActualGrip.Z)+Flat*(HoldDistance+6.f);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(FurnitureSetDown),false);
        Query.AddIgnoredActor(Owner);
        if(AActor* Furniture=HeldActor.Get())Query.AddIgnoredActor(Furniture);
        FHitResult Floor;
        if(!GetWorld()->LineTraceSingleByChannel(Floor,Probe+FVector(0,0,10.f),Probe-FVector(0,0,260.f),ECC_Visibility,Query)
            || Floor.ImpactNormal.Z<.6f)
        { ReleaseGrabAuthoritative(); return; }
        SetDownFrom=SmoothedTarget;
        SetDownTo=FVector(Probe.X,Probe.Y,Floor.ImpactPoint.Z+GripHeightAboveBottom+1.5f);
        SetDownBend=FMath::Clamp(ViewLocation.Z-SetDownTo.Z-44.f,0.f,105.f)*(CarrySize==ESSOCarrySize::Small?.6f:1.f);
    }
    CarryPhase=ESSOCarryPhase::SettingDown;
    PhaseAge=0.f;
    Owner->ForceNetUpdate();
}

void UHouseFurnitureInteractionComponent::ReleaseGrabAuthoritative()
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    if (auto* Axe = Cast<AHouseWoodAxe>(HeldActor.Get())) Axe->CancelSwing();
    bBasketballCharging = false;
    if (IsValid(PhysicsHandle)) PhysicsHandle->ReleaseComponent();
    if (UPrimitiveComponent* Body = HeldComponent.Get())
    {
        GetHandContact(true,LastRightContact,LastRightNormal);GetHandContact(false,LastLeftContact,LastLeftNormal);
        Body->ComponentTags.Remove(HeldFurnitureTag);
        Body->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
        Body->SetEnableGravity(true);
        Body->SetLinearDamping(SavedLinearDamping);Body->SetAngularDamping(SavedAngularDamping);
        Body->WakeAllRigidBodies();
    }
    if (AActor* Furniture = HeldActor.Get()) Furniture->ForceNetUpdate();
    HeldActor.Reset();
    HeldComponent.Reset();
    HeldBody=nullptr;
    HoldDistance = 0.0f;
    bHoldingFurniture = false;
    CarryPhase = ESSOCarryPhase::None;
    LastReleasedSize = CarrySize;
    SetComponentTickEnabled(CarryPoseBlend>.001f);
    if (AActor* Owner = GetOwner()) Owner->ForceNetUpdate();
}

// ---------------------------------------------------------------------------
// Throw / shove
// ---------------------------------------------------------------------------
void UHouseFurnitureInteractionComponent::ThrowHeldFurnitureAuthoritative()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !bHoldingFurniture) return;
    if (auto* Axe = HeldBody ? Cast<AHouseWoodAxe>(HeldBody->GetOwner()) : nullptr)
    { Axe->BeginSwing(Cast<AStoryFirstPersonCharacter>(Owner)); return; }
    if (HeldBody && Cast<AHouseBasketball>(HeldBody->GetOwner()))
    { BasketballInputAuthoritative(true, false, FRotator::ZeroRotator); return; }
    if (CarryPhase == ESSOCarryPhase::ThrowWindup || CarryPhase == ESSOCarryPhase::SettingDown
        || CarryPhase == ESSOCarryPhase::Thrown) return;
    if (!bAnimatedTransitions) { ExecuteThrowAuthoritative(); return; }
    // A large piece cannot be shoved until it is actually off the floor.
    if (CarrySize == ESSOCarrySize::Large && PickupAge < BraceDuration()) return;
    CarryPhase = ESSOCarryPhase::ThrowWindup;
    PhaseAge = 0.f;
    Owner->ForceNetUpdate();
}

void UHouseFurnitureInteractionComponent::ExecuteThrowAuthoritative()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !bHoldingFurniture) return;

    UPrimitiveComponent* Body = HeldComponent.Get();
    AActor* Furniture = HeldActor.Get();
    FVector ViewLocation;
    FRotator ViewRotation;
    const bool bCanThrow = IsValid(Body) && IsValid(Furniture)
        && Furniture->GetRootComponent() == Body
        && (Body->IsSimulatingPhysics() || (Cast<AHouseInteractiveProp>(Furniture) && Cast<AHouseInteractiveProp>(Furniture)->CanBeMoved()))
        && IsValid(PhysicsHandle) && PhysicsHandle->GetGrabbedComponent() == Body
        && GetServerView(ViewLocation, ViewRotation)
        && FVector::Dist(ViewLocation, Body->GetComponentTransform().TransformPosition(HeldLocalGrip)) <= BreakDistance;
    // Match the crosshair in three dimensions, including hanging fixtures.
    FVector Forward = ViewRotation.Vector().GetSafeNormal();
    if (Forward.IsNearlyZero()) Forward = Owner->GetActorForwardVector();
    const FVector Flat = Flatten(Forward, Owner->GetActorForwardVector());
    const FVector RightAxis(-Flat.Y, Flat.X, 0.f);
    const float Mass = Body ? FMath::Max(.25f, Body->GetMass()) : 10.f;
    const ESSOCarrySize Size = CarrySize;
    FVector Launch = FVector::ZeroVector, Spin = FVector::ZeroVector;
    if (bCanThrow)
    {
        switch (Size)
        {
        case ESSOCarrySize::Small:
            // Long, flat, one-handed throw with an end-over-end tumble.
            Launch = (Forward * SmallThrowSpeed + FVector::UpVector * 150.f) * FMath::Clamp(FMath::Sqrt(6.f / Mass), .7f, 1.15f);
            Spin = -RightAxis * 5.f;
            break;
        case ESSOCarrySize::Medium:
            // Short two-handed heave with an arc.
            Launch = (Forward * MediumThrowSpeed + FVector::UpVector * 190.f) * FMath::Clamp(FMath::Sqrt(20.f / Mass), .45f, .95f);
            Spin = -RightAxis * 1.5f;
            break;
        default:
            // Grounded shove: big furniture slides, it does not fly.
            Launch = Flat * FMath::Clamp(300.f - HeldMass * .6f, 120.f, 260.f) + FVector::UpVector * 25.f;
            break;
        }
    }

    // Basketball uses the player's release-time aim and power. No perfect lob.
    if (bCanThrow)
        if (const AHouseBasketball* Ball = Cast<AHouseBasketball>(Furniture))
        {
            ViewRotation = PendingBasketballAim;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(BasketballReleaseClearance), false, Owner);
            Q.AddIgnoredActor(Furniture); FHitResult Hit;
            const FVector Release = AHouseBasketball::ReleasePoint(ViewLocation, ViewRotation);
            if (!GetWorld()->SweepSingleByChannel(Hit, Body->GetComponentLocation(), Release, FQuat::Identity,
                ECC_Visibility, FCollisionShape::MakeSphere(12.4f), Q))
                Body->SetWorldLocation(Release, false, nullptr, ETeleportType::TeleportPhysics);
            Launch = Ball->ShotVelocity(ViewLocation, ViewRotation, FMath::Max(0.f, PendingBasketballPower));
            Spin = -FVector(-ViewRotation.Vector().Y, ViewRotation.Vector().X, 0).GetSafeNormal() * 6.f;
        }

    // Release also clears the server reservation, disables hold tick and
    // replicates the new hold state. A stale or invalid grip still gets cleaned up.
    ReleaseGrabAuthoritative();
    CarryPhase = ESSOCarryPhase::Thrown;
    StartFollowThrough();
    Owner->ForceNetUpdate();
    if (!bCanThrow || !IsValid(Body) || !Body->IsSimulatingPhysics()) return;

    Body->WakeAllRigidBodies();
    // The wind-up pulled the piece backward; launch from rest along the aim.
    if (Size != ESSOCarrySize::Large) Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
    if (AHouseBasketball* Ball = Cast<AHouseBasketball>(Furniture))
    { Body->SetLinearDamping(.003f); Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); Ball->MarkShot(); }
    Body->AddImpulse(Launch, NAME_None, true);
    if (!Spin.IsNearlyZero()) Body->SetPhysicsAngularVelocityInRadians(Spin);
    if (IsValid(Furniture)) Furniture->ForceNetUpdate();
}
