#include "HouseBreakDebris.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"

namespace
{
    constexpr int32 MaxPieces = 140;
    UStaticMesh* DebrisMesh(int32 Group)
    {
        static const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
        static const TCHAR* Shard = TEXT("/Game/SeniorSendOff/House/Interactivity/HI_GlassShard.HI_GlassShard");
        // Wedges and glass use the triangular shard; chunks and splinters are boxes.
        const bool bTriangle = Group == 1 || Group == 4;
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, bTriangle ? Shard : Cube);
        return Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, Cube);
    }
}

UHouseBreakDebrisComponent::UHouseBreakDebrisComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    SetIsReplicatedByDefault(false);
    for (FVector& Size : MeshSize) Size = FVector(100.f);
}

void UHouseBreakDebrisComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearAll();
    Super::EndPlay(EndPlayReason);
}

void UHouseBreakDebrisComponent::FinishGhost()
{
    if (UPrimitiveComponent* Hidden = GhostHidden.Get()) Hidden->SetVisibility(true);
    if (IsValid(Ghost)) Ghost->DestroyComponent();
    Ghost = nullptr;
    GhostHidden.Reset();
}

void UHouseBreakDebrisComponent::PlayGhost(UStaticMeshComponent* NewGhost, UPrimitiveComponent* HiddenBody, bool bCollapse,
    const FVector& Direction, float Duration)
{
    if (!NewGhost) return;
    FinishGhost();
    Ghost = NewGhost;
    GhostHidden = HiddenBody;
    if (HiddenBody) HiddenBody->SetVisibility(false);
    GhostStart = NewGhost->GetComponentTransform();
    NewGhost->UpdateBounds();
    const FBox Box = NewGhost->Bounds.GetBox();
    GhostPivot = FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z);
    GhostDirection = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
    if (GhostDirection.IsNearlyZero()) GhostDirection = FVector::ForwardVector;
    GhostAge = 0.f;
    GhostDuration = FMath::Max(.05f, Duration);
    bGhostCollapse = bCollapse;
    SetComponentTickEnabled(true);
}

void UHouseBreakDebrisComponent::UpdateGhost(float DeltaTime)
{
    if (!IsValid(Ghost)) { Ghost = nullptr; return; }
    GhostAge += DeltaTime;
    const float T = FMath::Clamp(GhostAge / GhostDuration, 0.f, 1.f);
    float Spread = 1.f, Squash = 1.f, TiltDegrees = 0.f, Push = 0.f;
    if (bGhostCollapse)
    {
        // Gives way gradually, then drops fast into the broken shape.
        const float E = T * T * (1.6f - .6f * T);
        Squash = 1.f - .42f * E;
        Spread = 1.f + .07f * E;
        TiltDegrees = 11.f * E;
        Push = 3.f * E;
    }
    else
    {
        // Crack shudder: pushed back by the blow, rebounds, settles.
        const float Wave = FMath::Sin(T * PI) * (1.f - .45f * T);
        const float Rebound = FMath::Sin(T * 2.f * PI) * .35f * (1.f - T);
        Squash = 1.f - .035f * Wave;
        Spread = 1.f + .015f * Wave;
        TiltDegrees = 3.5f * (Wave - Rebound);
        Push = 1.8f * (Wave - Rebound);
    }
    // Scale about the floor contact, tip away from the blow (top moves along it).
    const FVector Axis = FVector::CrossProduct(FVector::UpVector, GhostDirection).GetSafeNormal();
    const FQuat Tilt(Axis.IsNearlyZero() ? FVector::RightVector : Axis, FMath::DegreesToRadians(TiltDegrees));
    const FVector Factor(Spread, Spread, Squash);
    const FVector Relative = (GhostStart.GetLocation() - GhostPivot) * Factor;
    FTransform Pose;
    Pose.SetLocation(GhostPivot + Tilt.RotateVector(Relative) + GhostDirection * Push);
    Pose.SetRotation(Tilt * GhostStart.GetRotation());
    Pose.SetScale3D(GhostStart.GetScale3D() * Factor);
    Ghost->SetWorldTransform(Pose);
    if (T >= 1.f) FinishGhost();
}

void UHouseBreakDebrisComponent::ClearAll()
{
    FinishGhost();
    for (UInstancedStaticMeshComponent* Group : Groups)
        if (IsValid(Group)) Group->DestroyComponent();
    Groups.Reset();
    GroupMaterials.Reset();
    Pieces.Reset();
    SetComponentTickEnabled(false);
}

int32 UHouseBreakDebrisComponent::GetLivePieceCount() const
{
    int32 Live = 0;
    for (const FPiece& Piece : Pieces) Live += Piece.bDead ? 0 : 1;
    return Live;
}

UInstancedStaticMeshComponent* UHouseBreakDebrisComponent::GroupComponent(EGroup Group, UMaterialInterface* Material)
{
    // One instanced component per (group, material) pair.
    for (int32 Index = 0; Index < Groups.Num(); ++Index)
        if (IsValid(Groups[Index]) && Groups[Index]->ComponentTags.Contains(FName(*FString::FromInt(Group)))
            && GroupMaterials[Index] == Material)
            return Groups[Index];
    AActor* Owner = GetOwner();
    if (!Owner) return nullptr;
    UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Owner);
    Component->SetMobility(EComponentMobility::Movable);
    // Debris is lit like the piece it came from (a campus prop indoors is on the interior lighting channel).
    if (const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent())) Component->LightingChannels = Root->LightingChannels;
    Component->SetUsingAbsoluteLocation(true);
    Component->SetUsingAbsoluteRotation(true);
    Component->SetUsingAbsoluteScale(true);
    Component->SetWorldTransform(FTransform::Identity);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCanEverAffectNavigation(false);
    Component->SetGenerateOverlapEvents(false);
    Component->bVisibleInRayTracing = false;
    // Small chips do not need their own shadows; bigger chunks get them.
    Component->SetCastShadow(Group == SurfaceChunk || Group == InnerChunk);
    UStaticMesh* Mesh = DebrisMesh(Group);
    Component->SetStaticMesh(Mesh);
    if (Mesh) MeshSize[Group] = Mesh->GetBoundingBox().GetSize().ComponentMax(FVector(.01f));
    if (Material) Component->SetMaterial(0, Material);
    Component->ComponentTags.Add(FName(*FString::FromInt(Group)));
    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);
    Groups.Add(Component);
    GroupMaterials.Add(Material);
    return Component;
}

void UHouseBreakDebrisComponent::AddPiece(EGroup Group, UMaterialInterface* Material, const FVector& Position,
    const FVector& Velocity, const FVector& SizeCm, FRandomStream& Random, float Life, bool bSoft)
{
    UInstancedStaticMeshComponent* Component = GroupComponent(Group, Group == SurfaceWedge && AltMaterial ? AltMaterial.Get() : Material);
    if (!Component) return;
    if (Pieces.Num() >= MaxPieces)
    {
        // Retire the oldest piece in place; instance indices stay stable.
        for (FPiece& Old : Pieces) if (!Old.bDead) { Old.Life = FMath::Min(Old.Life, Old.Age + .4f); break; }
    }
    FPiece& Piece = Pieces.AddDefaulted_GetRef();
    Piece.Group = Group;
    Piece.Position = Position;
    Piece.Velocity = Velocity;
    Piece.Scale = SizeCm / MeshSize[Group];
    Piece.Rotation = FRotator(Random.FRandRange(-90.f, 90.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-90.f, 90.f)).Quaternion();
    Piece.SpinAxis = Random.GetUnitVector();
    // Small pieces spin faster than big ones.
    Piece.SpinSpeed = Random.FRandRange(4.f, 16.f) * FMath::Clamp(6.f / FMath::Max(1.f, SizeCm.GetMax()), .35f, 1.6f);
    Piece.Life = Life;
    // Foam and stuffing float and drift; wood and glass fall hard.
    Piece.Drag = bSoft ? 2.2f : .15f;
    Piece.Gravity = bSoft ? 620.f : 980.f;
    if (bSoft) Piece.SpinSpeed *= .5f;
    Piece.Instance = Component->AddInstance(FTransform(Piece.Rotation, Piece.Position, Piece.Scale), true);
    Piece.Component = Component;
    ++SpawnedTotal;
}

void UHouseBreakDebrisComponent::Burst(EHouseDebrisKind Kind, const FBox& Bounds, const FVector& Impact, const FVector& Direction,
    UMaterialInterface* Surface, UMaterialInterface* Inner, UMaterialInterface* Glass, float MassKg,
    UMaterialInterface* SurfaceAlt)
{
    if (!GetWorld() || GetNetMode() == NM_DedicatedServer || !Bounds.IsValid) return;
    // Mixed pieces (wood frame + metal legs, frame + art) shed both materials.
    AltMaterial = SurfaceAlt;
    FRandomStream Random(GetTypeHash(GetOwner()) ^ (++BurstSeed * 7919));
    const FVector Size = Bounds.GetSize();
    const float MaxDim = FMath::Max(10.f, Size.GetMax());
    const FVector Center = Bounds.GetCenter();
    FVector Dir = Direction.GetSafeNormal();
    if (Dir.IsNearlyZero()) Dir = (Center - Impact).GetSafeNormal();
    if (Dir.IsNearlyZero()) Dir = FVector::ForwardVector;
    // Chips leave the struck face back toward the blow and out to the sides.
    const FVector Back = -Dir;
    const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
    const float MinChip = FMath::Clamp(MaxDim * .012f, .8f, 2.5f);
    const float MaxChip = FMath::Clamp(MaxDim * .06f, 2.5f, 15.f);
    UMaterialInterface* InnerMaterial = Inner ? Inner : Surface;
    // Upholstery exposes foam or stuffing: soft tufts instead of splinters.
    const FString InnerName = GetNameSafe(Inner).ToLower();
    const bool bSoft = InnerName.Contains(TEXT("foam")) || InnerName.Contains(TEXT("stuff")) || InnerName.Contains(TEXT("fabric"))
        || InnerName.Contains(TEXT("fluff")) || InnerName.Contains(TEXT("fill"));
    auto Tuft = [&](float Scale)
    {
        const float A = Random.FRandRange(MinChip * 1.5f, FMath::Lerp(MinChip * 2.f, MaxChip * .8f, Scale));
        return FVector(A, A * Random.FRandRange(.7f, 1.f), A * Random.FRandRange(.55f, .9f));
    };
    auto Chip = [&](float Scale)
    {
        const float A = Random.FRandRange(MinChip, FMath::Lerp(MinChip, MaxChip, Scale));
        return FVector(A, A * Random.FRandRange(.45f, 1.f), A * Random.FRandRange(.25f, .7f));
    };
    auto FromImpact = [&](float Spread)
    {
        // A point on the struck surface region, slightly in from the impact.
        return Impact + FVector(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * Spread;
    };
    auto FromVolume = [&]()
    {
        return Center + FVector(Random.FRandRange(-.42f, .42f) * Size.X, Random.FRandRange(-.42f, .42f) * Size.Y,
            Random.FRandRange(-.35f, .45f) * Size.Z);
    };
    auto Spray = [&](float Speed, float Lift, float Backwards)
    {
        return (Back * Backwards + Side * Random.FRandRange(-.9f, .9f) + Random.GetUnitVector() * .45f).GetSafeNormal()
            * Speed * Random.FRandRange(.55f, 1.f) + FVector::UpVector * Lift * Random.FRandRange(.4f, 1.f);
    };

    if (Kind == EHouseDebrisKind::Hit)
    {
        const int32 Count = Random.RandRange(3, 5);
        for (int32 Index = 0; Index < Count; ++Index)
            AddPiece(Index % 2 ? SurfaceWedge : SurfaceChunk, Surface, FromImpact(2.f), Spray(260.f, 170.f, .9f),
                Chip(.25f), Random, Random.FRandRange(4.f, 6.f));
    }
    else if (Kind == EHouseDebrisKind::Crack)
    {
        const int32 Count = Random.RandRange(6, 9);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const bool bInner = Index % 3 == 0;
            AddPiece(bInner ? InnerChunk : (Index % 2 ? SurfaceWedge : SurfaceChunk), bInner ? InnerMaterial : Surface,
                FromImpact(4.f), Spray(bInner && bSoft ? 180.f : 300.f, 200.f, .8f), bInner && bSoft ? Tuft(.3f) : Chip(.45f),
                Random, Random.FRandRange(6.f, 9.f), bInner && bSoft);
        }
        for (int32 Index = 0; Index < 2; ++Index)
        {
            if (bSoft)
            {
                AddPiece(InnerChunk, InnerMaterial, FromImpact(4.f), Spray(170.f, 190.f, .7f), Tuft(.4f), Random, Random.FRandRange(6.f, 9.f), true);
                continue;
            }
            const float Length = FMath::Clamp(MaxDim * Random.FRandRange(.05f, .11f), 4.f, 18.f);
            AddPiece(InnerSplinter, InnerMaterial, FromImpact(4.f), Spray(240.f, 220.f, .7f),
                FVector(Length, Length * .12f, Length * .06f), Random, Random.FRandRange(6.f, 9.f));
        }
        if (Glass)
            for (int32 Index = 0; Index < 6; ++Index)
                AddPiece(GlassShard, Glass, FromImpact(5.f), Spray(220.f, 140.f, .9f),
                    FVector(Random.FRandRange(1.5f, 5.f), Random.FRandRange(1.2f, 4.f), .2f), Random, Random.FRandRange(6.f, 9.f));
    }
    else
    {
        // Break: a burst through the whole piece, strongest at the impact side.
        // Heavy pieces throw their debris lower and slower.
        const float Weight = FMath::Clamp(FMath::Sqrt(20.f / FMath::Max(1.f, MassKg)), .55f, 1.15f);
        const int32 Chunks = FMath::Clamp(FMath::RoundToInt(MaxDim / 7.f), 12, 26);
        for (int32 Index = 0; Index < Chunks; ++Index)
        {
            const bool bNearImpact = Index < Chunks / 2;
            const FVector Start = bNearImpact ? FromImpact(MaxDim * .12f) : FromVolume();
            const FVector Out = (Start - Center).GetSafeNormal2D();
            const FVector Velocity = bNearImpact ? Spray(380.f * Weight, 260.f * Weight, .75f)
                : (Out * Random.FRandRange(90.f, 260.f) + Random.GetUnitVector() * 60.f) * Weight + FVector::UpVector * Random.FRandRange(40.f, 200.f) * Weight;
            const bool bInner = Index % 3 == 1;
            AddPiece(bInner ? InnerChunk : (Index % 2 ? SurfaceWedge : SurfaceChunk), bInner ? InnerMaterial : Surface,
                Start, bInner && bSoft ? Velocity * .65f : Velocity, bInner && bSoft ? Tuft(.7f) : Chip(Index % 5 == 0 ? 1.f : .6f),
                Random, Random.FRandRange(9.f, 13.f), bInner && bSoft);
        }
        const int32 Splinters = FMath::Clamp(FMath::RoundToInt(MaxDim / 20.f), 3, 8);
        for (int32 Index = 0; Index < Splinters; ++Index)
        {
            if (bSoft)
            {
                // Torn stuffing puffs out of the split, drifting down.
                AddPiece(InnerChunk, InnerMaterial, FromImpact(MaxDim * .12f), Spray(190.f, 230.f, .5f), Tuft(1.f), Random, Random.FRandRange(9.f, 13.f), true);
                AddPiece(InnerChunk, InnerMaterial, FromVolume(), Spray(140.f, 260.f, .2f), Tuft(.6f), Random, Random.FRandRange(9.f, 13.f), true);
                continue;
            }
            const float Length = FMath::Clamp(MaxDim * Random.FRandRange(.08f, .2f), 6.f, 34.f);
            AddPiece(InnerSplinter, InnerMaterial, FromImpact(MaxDim * .15f), Spray(300.f * Weight, 240.f * Weight, .6f),
                FVector(Length, Length * Random.FRandRange(.06f, .12f), Length * .05f), Random, Random.FRandRange(9.f, 13.f));
        }
        // Fine dust-like specks: tiny, quick to settle.
        for (int32 Index = 0; Index < 10; ++Index)
            AddPiece(InnerChunk, InnerMaterial, FromImpact(MaxDim * .1f), Spray(200.f, 120.f, .5f),
                FVector(Random.FRandRange(.5f, 1.2f)), Random, Random.FRandRange(4.f, 6.f), bSoft);
        if (Glass)
            for (int32 Index = 0; Index < 28; ++Index)
                AddPiece(GlassShard, Glass, FromVolume(), Spray(260.f, 180.f, .8f),
                    FVector(Random.FRandRange(2.f, 9.f), Random.FRandRange(1.5f, 7.f), .2f), Random, Random.FRandRange(9.f, 13.f));
    }
    SetComponentTickEnabled(true);
}

void UHouseBreakDebrisComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UWorld* World = GetWorld();
    if (!World) return;
    if (Ghost) UpdateGhost(DeltaTime);
    const float Step = FMath::Min(DeltaTime, .05f);
    FCollisionObjectQueryParams Surfaces;
    Surfaces.AddObjectTypesToQuery(ECC_WorldStatic);
    Surfaces.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseBreakDebris), false, GetOwner());
    bool bAnyLive = false;
    TSet<UInstancedStaticMeshComponent*> Dirty;
    for (FPiece& Piece : Pieces)
    {
        if (Piece.bDead) continue;
        Piece.Age += DeltaTime;
        UInstancedStaticMeshComponent* Component = Piece.Component.Get();
        if (!Component || !Component->IsValidInstance(Piece.Instance)) { Piece.bDead = true; continue; }
        if (!Piece.bResting)
        {
            if (Piece.bSliding)
            {
                // Friction slows a sliding piece to a stop on the floor.
                const float Speed = Piece.Velocity.Size2D();
                const float NewSpeed = FMath::Max(0.f, Speed - 650.f * Step);
                Piece.Velocity = Speed > KINDA_SMALL_NUMBER ? Piece.Velocity.GetSafeNormal2D() * NewSpeed : FVector::ZeroVector;
                Piece.SpinSpeed *= FMath::Max(0.f, 1.f - 6.f * Step);
                FVector Next = Piece.Position + Piece.Velocity * Step;
                FHitResult Wall;
                if (World->LineTraceSingleByObjectType(Wall, Piece.Position, Next, Surfaces, Query))
                { Next = Wall.ImpactPoint + Wall.ImpactNormal * .3f; Piece.Velocity = Piece.Velocity.MirrorByVector(Wall.ImpactNormal) * .25f; }
                Piece.Position = Next;
                if (NewSpeed < 4.f) { Piece.bResting = true; Piece.Velocity = FVector::ZeroVector; }
            }
            else
            {
                Piece.Velocity.Z -= Piece.Gravity * Step;
                Piece.Velocity *= FMath::Max(0.f, 1.f - Piece.Drag * Step); // air drag
                FVector Next = Piece.Position + Piece.Velocity * Step;
                FHitResult Hit;
                if (World->LineTraceSingleByObjectType(Hit, Piece.Position, Next, Surfaces, Query))
                {
                    Next = Hit.ImpactPoint + Hit.ImpactNormal * FMath::Max(.2f, Piece.Scale.Z * MeshSize[Piece.Group].Z * .5f);
                    if (Hit.ImpactNormal.Z > .5f)
                    {
                        if (FMath::Abs(Piece.Velocity.Z) > 130.f && Piece.Bounces < 2)
                        {
                            // Hop, lose energy and spin, then come down again.
                            ++Piece.Bounces;
                            Piece.Velocity = FVector(Piece.Velocity.X * .55f, Piece.Velocity.Y * .55f, FMath::Abs(Piece.Velocity.Z) * .28f);
                            Piece.SpinSpeed *= .6f;
                            Piece.SpinAxis = (Piece.SpinAxis + FVector(Piece.Velocity.Y, -Piece.Velocity.X, 0.f).GetSafeNormal()).GetSafeNormal();
                        }
                        else
                        {
                            Piece.bSliding = true;
                            Piece.Velocity.Z = 0.f;
                            // Settle flat: keep the heading, lie on the broad face.
                            const FRotator Heading(0.f, Piece.Rotation.Rotator().Yaw, 0.f);
                            Piece.RestRotation = Heading.Quaternion();
                        }
                    }
                    else Piece.Velocity = Piece.Velocity.MirrorByVector(Hit.ImpactNormal) * .3f;
                }
                Piece.Position = Next;
                if (Piece.Age > 4.f) Piece.Life = FMath::Min(Piece.Life, Piece.Age + .5f); // fell out of the world
            }
            if (Piece.bSliding || Piece.bResting)
            {
                Piece.RestBlend = FMath::Min(1.f, Piece.RestBlend + Step * 7.f);
                Piece.Rotation = FQuat::Slerp(Piece.Rotation, Piece.RestRotation, Piece.RestBlend);
            }
            if (Piece.SpinSpeed > .01f && !Piece.bResting)
                Piece.Rotation = (FQuat(Piece.SpinAxis, Piece.SpinSpeed * Step) * Piece.Rotation).GetNormalized();
        }
        // Fade by shrinking into the floor over the last second.
        const float Fade = FMath::Clamp((Piece.Life - Piece.Age) / 1.f, 0.f, 1.f);
        const FVector Scale = Piece.Scale * FMath::SmoothStep(0.f, 1.f, Fade);
        if (Fade <= 0.f) Piece.bDead = true;
        else bAnyLive = true;
        Component->UpdateInstanceTransform(Piece.Instance, FTransform(Piece.Rotation, Piece.Position, Piece.bDead ? FVector::ZeroVector : Scale), true, false, true);
        Dirty.Add(Component);
    }
    for (UInstancedStaticMeshComponent* Component : Dirty) Component->MarkRenderStateDirty();
    if (!bAnyLive && !Ghost)
    {
        for (UInstancedStaticMeshComponent* Group : Groups) if (IsValid(Group)) Group->DestroyComponent();
        Groups.Reset(); GroupMaterials.Reset(); Pieces.Reset();
        SetComponentTickEnabled(false);
    }
}
