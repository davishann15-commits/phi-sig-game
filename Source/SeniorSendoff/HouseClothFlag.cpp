#include "HouseClothFlag.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "KismetProceduralMeshLibrary.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"

namespace
{
    constexpr float SubStep = 1.f / 90.f;
    struct FPawnCapsule { FVector Bottom, Top, Velocity; float Radius; };

    /**
     * Split long render edges (shared midpoints, no cracks) until every edge is
     * shorter than MaxEdge, so long stripes and plain panels bend with the cloth
     * instead of staying straight between two distant vertices.
     */
    void Refine(TArray<FVector>& Pos, TArray<FVector>& Nrm, TArray<FVector2D>& UV, TArray<int32>& Tris, float MaxEdge, int32 MaxVertices)
    {
        const float Limit = MaxEdge * MaxEdge;
        for (int32 Pass = 0; Pass < 8; ++Pass)
        {
            TMap<uint64, int32> Mid;
            auto Key = [](int32 A, int32 B) { return A < B ? (uint64(A) << 32) | uint32(B) : (uint64(B) << 32) | uint32(A); };
            auto Split = [&](int32 A, int32 B) -> int32
            {
                if (FVector::DistSquared(Pos[A], Pos[B]) <= Limit) return -1;
                const uint64 K = Key(A, B);
                if (const int32* Found = Mid.Find(K)) return *Found;
                if (Pos.Num() >= MaxVertices) return -1;
                const int32 M = Pos.Add((Pos[A] + Pos[B]) * .5f);
                Nrm.Add((Nrm[A] + Nrm[B]).GetSafeNormal());
                UV.Add((UV[A] + UV[B]) * .5f);
                Mid.Add(K, M);
                return M;
            };
            TArray<int32> Out;
            Out.Reserve(Tris.Num() * 2);
            bool bChanged = false;
            for (int32 T = 0; T + 2 < Tris.Num(); T += 3)
            {
                const int32 V0 = Tris[T], V1 = Tris[T + 1], V2 = Tris[T + 2];
                const int32 M0 = Split(V0, V1), M1 = Split(V1, V2), M2 = Split(V2, V0);
                auto Add = [&](int32 A, int32 B, int32 C) { Out.Add(A); Out.Add(B); Out.Add(C); };
                const int32 Mask = (M0 >= 0 ? 1 : 0) | (M1 >= 0 ? 2 : 0) | (M2 >= 0 ? 4 : 0);
                bChanged |= Mask != 0;
                switch (Mask)
                {
                case 0: Add(V0, V1, V2); break;
                case 1: Add(V0, M0, V2); Add(M0, V1, V2); break;
                case 2: Add(V0, V1, M1); Add(V0, M1, V2); break;
                case 4: Add(V0, V1, M2); Add(M2, V1, V2); break;
                case 3: Add(M0, V1, M1); Add(V0, M0, M1); Add(V0, M1, V2); break;
                case 6: Add(V0, V1, M1); Add(V0, M1, M2); Add(M2, M1, V2); break;
                case 5: Add(V0, M0, M2); Add(M0, V1, V2); Add(M2, M0, V2); break;
                default: Add(V0, M0, M2); Add(M0, V1, M1); Add(M2, M1, V2); Add(M0, M1, M2); break;
                }
            }
            Tris = MoveTemp(Out);
            if (!bChanged) break;
        }
    }
}

UHouseClothFlagComponent::UHouseClothFlagComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    SetIsReplicatedByDefault(false);
}

void UHouseClothFlagComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(Cloth)) Cloth->DestroyComponent();
    Cloth = nullptr;
    if (UStaticMeshComponent* Body = Source.Get()) Body->SetVisibility(true);
    Super::EndPlay(EndPlayReason);
}

bool UHouseClothFlagComponent::Initialize(UStaticMeshComponent* SourceBody, bool bHangFromTop)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (bActive || !Owner || !World || !SourceBody || World->GetNetMode() == NM_DedicatedServer) return false;
    UStaticMesh* Mesh = SourceBody->GetStaticMesh();
    if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0) return false;
    const FTransform T = SourceBody->GetComponentTransform();

    // Flag plane from the mesh's own box: thinnest axis is the cloth normal.
    const FBox LocalBox = Mesh->GetBoundingBox();
    const FVector Scale = T.GetScale3D().GetAbs();
    const FVector Ext = LocalBox.GetExtent() * Scale;
    int32 Thin = 0;
    for (int32 Axis = 1; Axis < 3; ++Axis) if (Ext[Axis] < Ext[Thin]) Thin = Axis;
    const int32 A = (Thin + 1) % 3, B = (Thin + 2) % 3;
    auto WorldAxis = [&](int32 Axis) { FVector V = FVector::ZeroVector; V[Axis] = 1.f; return T.TransformVectorNoScale(V).GetSafeNormal(); };
    FVector AxisA = WorldAxis(A), AxisB = WorldAxis(B);
    const bool bAIsUp = FMath::Abs(AxisA.Z) > FMath::Abs(AxisB.Z);
    FVector Up = bAIsUp ? AxisA : AxisB;
    Across = bAIsUp ? AxisB : AxisA;
    if (FMath::Abs(Up.Z) < .6f) return false; // lying flat: not a hung flag
    if (Up.Z < 0.f) Up = -Up;
    Down = -Up;
    Width = 2.f * (bAIsUp ? Ext[B] : Ext[A]);
    Height = 2.f * (bAIsUp ? Ext[A] : Ext[B]);
    if (Width < 10.f || Height < 10.f) return false;
    Out = WorldAxis(Thin);
    const FVector Center = T.TransformPosition(LocalBox.GetCenter());

    // Which side is the wall? The cloth lies against it and can never pass it.
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ClothFlagSetup), false, Owner);
    bool bWallBehind = false;
    for (const float Sign : {1.f, -1.f})
    {
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Center + Out * Sign * (Ext[Thin] + 1.f), Center + Out * Sign * 30.f, ECC_Visibility, Query))
        { Out = -Out * Sign; bWallBehind = true; break; }
    }
    // A flag hanging free (from a railing) has no wall plane.
    bWall = bWallBehind;
    FHitResult Floor;
    if (World->LineTraceSingleByChannel(Floor, Center, Center - FVector(0, 0, 800.f), ECC_Visibility, Query)) FloorZ = Floor.ImpactPoint.Z;
    else FloorZ = Center.Z - Height - 300.f;
    Origin = Center - Across * (Width * .5f) - Down * (Height * .5f) - Out * Ext[Thin];

    NU = FMath::Clamp(FMath::RoundToInt(Width / 9.f) + 1, 5, 18);
    NV = FMath::Clamp(FMath::RoundToInt(Height / 9.f) + 1, 5, 18);
    const float Cell = FMath::Min(Width / (NU - 1), Height / (NV - 1));

    // Read the render mesh. Fails in packaged builds unless the mesh allows CPU access.
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    int32 TotalVertices = 0;
    for (int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
    {
        TArray<FVector> Vertices, Normals; TArray<int32> Triangles; TArray<FVector2D> UVs; TArray<FProcMeshTangent> Tangents;
        UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Mesh, 0, SectionIndex, Vertices, Triangles, Normals, UVs, Tangents);
        if (Normals.Num() != Vertices.Num()) Normals.Init(FVector::ForwardVector, Vertices.Num());
        if (UVs.Num() != Vertices.Num()) UVs.Init(FVector2D::ZeroVector, Vertices.Num());
        // Edge length limit in mesh units (component scale is applied later).
        const float MeshScale = FMath::Max(KINDA_SMALL_NUMBER, Scale.GetMax());
        Refine(Vertices, Normals, UVs, Triangles, Cell * .7f / MeshScale, 30000);
        FSectionData& Section = Sections.AddDefaulted_GetRef();
        Section.Triangles = MoveTemp(Triangles);
        Section.UV0 = MoveTemp(UVs);
        Section.RestWorld.SetNum(Vertices.Num());
        Section.RestNormals.SetNum(Vertices.Num());
        Section.Grid.SetNum(Vertices.Num());
        for (int32 V = 0; V < Vertices.Num(); ++V)
        {
            const FVector WorldPos = T.TransformPosition(Vertices[V]);
            Section.RestWorld[V] = WorldPos;
            Section.RestNormals[V] = Normals.IsValidIndex(V) ? T.TransformVectorNoScale(Normals[V]).GetSafeNormal() : Out;
            Section.Grid[V] = FVector2D(FMath::Clamp(FVector::DotProduct(WorldPos - Origin, Across) / Width, 0.f, 1.f),
                FMath::Clamp(FVector::DotProduct(WorldPos - Origin, Down) / Height, 0.f, 1.f));
        }
        Section.Positions = Section.RestWorld;
        Section.Normals = Section.RestNormals;
        TotalVertices += Vertices.Num();
    }
    if (TotalVertices == 0 || TotalVertices > 90000) { Sections.Reset(); return false; }

    // Cloth grid on the back face (against the wall): ~9 cm cells.
    P.SetNum(NU * NV); Prev.SetNum(NU * NV); Rest.SetNum(NU * NV); InvMass.Init(1.f, NU * NV);
    for (int32 J = 0; J < NV; ++J)
        for (int32 I = 0; I < NU; ++I)
        {
            const FVector Position = Origin + Across * (Width * I / (NU - 1)) + Down * (Height * J / (NV - 1));
            P[Index(I, J)] = Prev[Index(I, J)] = Rest[Index(I, J)] = Position;
        }
    auto Link = [&](int32 I0, int32 J0, int32 I1, int32 J1, float Stiffness)
    {
        if (I1 < 0 || I1 >= NU || J1 < 0 || J1 >= NV) return;
        FLink& L = Links.AddDefaulted_GetRef();
        L.A = Index(I0, J0); L.B = Index(I1, J1);
        L.Rest = FVector::Dist(Rest[L.A], Rest[L.B]);
        L.Stiffness = Stiffness;
    };
    for (int32 J = 0; J < NV; ++J)
        for (int32 I = 0; I < NU; ++I)
        {
            Link(I, J, I + 1, J, 1.f); Link(I, J, I, J + 1, 1.f);              // stretch
            Link(I, J, I + 1, J + 1, .6f); Link(I + 1, J, I, J + 1, .6f);      // shear
            Link(I, J, I + 2, J, .3f); Link(I, J, I, J + 2, .3f);            // soft bending
        }
    // Tacks: a railing flag is tied along the whole top edge; a wall flag is
    // tacked every ~30 cm along the top, plus its bottom corners if it is wide.
    const int32 TackStep = FMath::Max(1, FMath::RoundToInt(30.f / (Width / (NU - 1))));
    for (int32 I = 0; I < NU; ++I)
        if (bHangFromTop || I == 0 || I == NU - 1 || I % TackStep == 0) Pins.Add(Index(I, 0));
    if (!bHangFromTop && bWallBehind && Width > Height * 1.1f) { Pins.Add(Index(0, NV - 1)); Pins.Add(Index(NU - 1, NV - 1)); }
    for (int32 Pin : Pins) InvMass[Pin] = 0.f;

    Cloth = NewObject<UProceduralMeshComponent>(Owner);
    Cloth->SetUsingAbsoluteLocation(true);
    Cloth->SetUsingAbsoluteRotation(true);
    Cloth->SetUsingAbsoluteScale(true);
    Cloth->SetWorldTransform(FTransform::Identity);
    Cloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Cloth->bUseAsyncCooking = false;
    Cloth->SetCastShadow(SourceBody->CastShadow);
    Cloth->RegisterComponent();
    Owner->AddInstanceComponent(Cloth);
    for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
    {
        FSectionData& Section = Sections[SectionIndex];
        Cloth->CreateMeshSection_LinearColor(SectionIndex, Section.RestWorld, Section.Triangles, Section.RestNormals, Section.UV0,
            TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
        const int32 MaterialIndex = LOD.Sections.IsValidIndex(SectionIndex) ? LOD.Sections[SectionIndex].MaterialIndex : SectionIndex;
        Cloth->SetMaterial(SectionIndex, SourceBody->GetMaterial(MaterialIndex));
    }
    Source = SourceBody;
    SourceBody->SetVisibility(false);
    bActive = true;
    bAwake = true;
    SetComponentTickEnabled(true);
    return true;
}

float UHouseClothFlagComponent::GetMaxDisplacement() const
{
    float Most = 0.f;
    for (int32 K = 0; K < P.Num(); ++K) Most = FMath::Max(Most, float(FVector::Dist(P[K], Rest[K])));
    return Most;
}

FBox UHouseClothFlagComponent::GetClothBounds() const
{
    FBox Box(ForceInit);
    for (const FVector& Point : P) Box += Point;
    return Box;
}

void UHouseClothFlagComponent::AddImpulse(const FVector& WorldPoint, const FVector& Direction, float Speed)
{
    if (!bActive) return;
    FVector Dir = Direction.GetSafeNormal();
    // A blow into the wall makes the cloth slap back out.
    if (bWall && FVector::DotProduct(Dir, Out) < 0.f) Dir = (Dir - FVector::DotProduct(Dir, Out) * Out * 1.7f).GetSafeNormal();
    const float Radius = FMath::Clamp(FMath::Max(Width, Height) * .35f, 25.f, 80.f);
    for (int32 K = 0; K < P.Num(); ++K)
    {
        if (InvMass[K] <= 0.f) continue;
        const float Distance = FVector::Dist(P[K], WorldPoint);
        if (Distance >= Radius) continue;
        const float Weight = FMath::Square(1.f - Distance / Radius);
        Prev[K] -= Dir * Speed * Weight * SubStep;
    }
    Wake();
}

void UHouseClothFlagComponent::Release(const FVector& PullFrom, bool bQuick)
{
    if (!bActive || bReleased) return;
    bReleased = true;
    PendingPins = Pins;
    // Tacks nearest the pull give first; the far side holds a moment longer.
    PendingPins.Sort([&](int32 X, int32 Y) { return FVector::DistSquared(P[X], PullFrom) < FVector::DistSquared(P[Y], PullFrom); });
    PinTimer = 0.f;
    if (bQuick) { for (int32 Pin : PendingPins) InvMass[Pin] = 1.f; PendingPins.Reset(); }
    // Peel off toward the puller.
    const FVector Toward = (PullFrom - GetClothBounds().GetCenter()).GetSafeNormal2D();
    for (int32 K = 0; K < P.Num(); ++K)
        if (InvMass[K] > 0.f) Prev[K] -= (Out * 90.f + Toward * 60.f) * SubStep;
    Wake();
}

void UHouseClothFlagComponent::Step(float Dt)
{
    Time += Dt;
    const float Damping = bReleased ? .975f : .985f;
    const FVector Gravity(0, 0, -980.f);
    for (int32 J = 0; J < NV; ++J)
        for (int32 I = 0; I < NU; ++I)
        {
            const int32 Id = Index(I, J);
            if (InvMass[Id] <= 0.f) { P[Id] = Prev[Id] = Rest[Id]; continue; }
            const float U = float(I) / (NU - 1), V = float(J) / (NV - 1);
            // Gentle room air: the free bottom breathes, the tacked top does not.
            const float Air = bReleased ? 0.f : 22.f * V * (FMath::Sin(Time * 1.3f + U * 4.f + V * 2.5f) * (.6f + .4f * FMath::Sin(Time * .37f + U)));
            const FVector Velocity = (P[Id] - Prev[Id]) * Damping;
            const FVector Next = P[Id] + Velocity + (Gravity + Out * Air) * Dt * Dt;
            Prev[Id] = P[Id];
            P[Id] = Next;
        }
    for (int32 Iteration = 0; Iteration < 6; ++Iteration)
    {
        for (const FLink& L : Links)
        {
            const float WA = InvMass[L.A], WB = InvMass[L.B];
            if (WA + WB <= 0.f) continue;
            const FVector Delta = P[L.B] - P[L.A];
            const float Length = Delta.Size();
            if (Length < KINDA_SMALL_NUMBER) continue;
            // Cloth resists stretching, not compressing: it folds instead.
            float Error = Length - L.Rest;
            if (Error < 0.f) Error *= L.Stiffness < .5f ? .5f : .08f;
            const FVector Correction = Delta * (Error / Length) * L.Stiffness / (WA + WB);
            P[L.A] += Correction * WA;
            P[L.B] -= Correction * WB;
        }
    }
}

void UHouseClothFlagComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UWorld* World = GetWorld();
    if (!bActive || !World) return;
    // Players nearby (brushing past, walking through or over the cloth).
    TArray<FPawnCapsule, TInlineAllocator<4>> Pawns;
    const FBox Bounds = GetClothBounds().ExpandBy(120.f);
    for (TActorIterator<ACharacter> It(World); It; ++It)
    {
        const UCapsuleComponent* Capsule = It->GetCapsuleComponent();
        if (!Capsule || !Bounds.IsInsideOrOn(It->GetActorLocation())) continue;
        FPawnCapsule& Entry = Pawns.AddDefaulted_GetRef();
        const float Half = Capsule->GetScaledCapsuleHalfHeight();
        Entry.Bottom = It->GetActorLocation() - FVector(0, 0, Half);
        Entry.Top = It->GetActorLocation() + FVector(0, 0, Half);
        Entry.Radius = Capsule->GetScaledCapsuleRadius() + 3.f;
        Entry.Velocity = It->GetVelocity();
        if (Entry.Velocity.SizeSquared() > 100.f) Wake();
    }
    if (!bAwake) return;
    SetComponentTickInterval(0.f);
    // Tacks popping one at a time during a tear-down.
    if (PendingPins.Num() > 0)
    {
        PinTimer -= DeltaTime;
        if (PinTimer <= 0.f)
        {
            const int32 Pin = PendingPins[0];
            PendingPins.RemoveAt(0);
            InvMass[Pin] = 1.f;
            Prev[Pin] = P[Pin] - (Out * 120.f + Down * 60.f) * SubStep;
            PinTimer = FMath::FRandRange(.08f, .16f);
        }
    }
    Accumulator = FMath::Min(Accumulator + DeltaTime, SubStep * 4.f);
    float MaxSpeed = 0.f;
    while (Accumulator >= SubStep)
    {
        Accumulator -= SubStep;
        Step(SubStep);
        // Contacts: wall behind, floor below, players' capsules.
        for (int32 Id = 0; Id < P.Num(); ++Id)
        {
            if (InvMass[Id] <= 0.f) continue;
            FVector& Point = P[Id];
            if (bWall)
            {
                const float Behind = FVector::DotProduct(Point - Origin, Out);
                if (Behind < .3f) Point += Out * (.3f - Behind);
            }
            if (Point.Z < FloorZ + .8f)
            {
                Point.Z = FloorZ + .8f;
                Prev[Id].Z = Point.Z;
                Prev[Id].X = FMath::Lerp(Prev[Id].X, Point.X, .5f);
                Prev[Id].Y = FMath::Lerp(Prev[Id].Y, Point.Y, .5f);
            }
            for (const FPawnCapsule& Pawn : Pawns)
            {
                const FVector Closest = FMath::ClosestPointOnSegment(Point, Pawn.Bottom, Pawn.Top);
                FVector Away = Point - Closest;
                Away.Z = 0.f;
                const float Distance = Away.Size();
                if (Distance >= Pawn.Radius) continue;
                const FVector Dir = Distance > KINDA_SMALL_NUMBER ? Away / Distance : Out;
                Point = FVector(Closest.X, Closest.Y, Point.Z) + Dir * Pawn.Radius;
                Point += FVector(Pawn.Velocity.X, Pawn.Velocity.Y, 0.f) * SubStep * .5f;
            }
            MaxSpeed = FMath::Max(MaxSpeed, float((Point - Prev[Id]).Size() / SubStep));
        }
    }
    UpdateMesh();
    // Sleep when still and nobody is near; keep a slow check for visitors.
    if (MaxSpeed < 2.f && Pawns.Num() == 0 && PendingPins.Num() == 0)
    {
        SleepTimer += DeltaTime;
        if (SleepTimer > 2.f) { bAwake = false; SetComponentTickInterval(.1f); }
    }
    else SleepTimer = 0.f;
}

void UHouseClothFlagComponent::UpdateMesh()
{
    if (!IsValid(Cloth)) return;
    // Per particle: displacement, and rotation of the cloth surface from rest.
    TArray<FVector> Displacement; Displacement.SetNum(P.Num());
    TArray<FQuat> Turn; Turn.SetNum(P.Num());
    for (int32 J = 0; J < NV; ++J)
        for (int32 I = 0; I < NU; ++I)
        {
            const int32 Id = Index(I, J);
            Displacement[Id] = P[Id] - Rest[Id];
            const int32 L = Index(FMath::Max(I - 1, 0), J), R = Index(FMath::Min(I + 1, NU - 1), J);
            const int32 T = Index(I, FMath::Max(J - 1, 0)), B = Index(I, FMath::Min(J + 1, NV - 1));
            const FVector Now = FVector::CrossProduct(P[R] - P[L], P[B] - P[T]).GetSafeNormal();
            const FVector Was = FVector::CrossProduct(Rest[R] - Rest[L], Rest[B] - Rest[T]).GetSafeNormal();
            Turn[Id] = Now.IsNearlyZero() || Was.IsNearlyZero() ? FQuat::Identity : FQuat::FindBetweenNormals(Was, Now);
        }
    for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
    {
        FSectionData& Section = Sections[SectionIndex];
        for (int32 V = 0; V < Section.RestWorld.Num(); ++V)
        {
            const float GX = Section.Grid[V].X * (NU - 1), GY = Section.Grid[V].Y * (NV - 1);
            const int32 I0 = FMath::Clamp(FMath::FloorToInt(GX), 0, NU - 2), J0 = FMath::Clamp(FMath::FloorToInt(GY), 0, NV - 2);
            const float FX = GX - I0, FY = GY - J0;
            const int32 C00 = Index(I0, J0), C10 = Index(I0 + 1, J0), C01 = Index(I0, J0 + 1), C11 = Index(I0 + 1, J0 + 1);
            const float W00 = (1 - FX) * (1 - FY), W10 = FX * (1 - FY), W01 = (1 - FX) * FY, W11 = FX * FY;
            // Rotate each vertex's offset from the cloth plane with the surface,
            // so a thick hem or printed layer bends with the fabric.
            const FVector RestBase = Origin + Across * (Section.Grid[V].X * Width) + Down * (Section.Grid[V].Y * Height);
            const FVector Offset = Section.RestWorld[V] - RestBase;
            const FVector Base = RestBase + Displacement[C00] * W00 + Displacement[C10] * W10 + Displacement[C01] * W01 + Displacement[C11] * W11;
            const FVector Normal = Section.RestNormals[V];
            FVector RotatedOffset = FVector::ZeroVector, RotatedNormal = FVector::ZeroVector;
            for (const TPair<int32, float>& Corner : {TPair<int32, float>(C00, W00), TPair<int32, float>(C10, W10), TPair<int32, float>(C01, W01), TPair<int32, float>(C11, W11)})
            {
                RotatedOffset += Turn[Corner.Key].RotateVector(Offset) * Corner.Value;
                RotatedNormal += Turn[Corner.Key].RotateVector(Normal) * Corner.Value;
            }
            Section.Positions[V] = Base + RotatedOffset;
            Section.Normals[V] = RotatedNormal.GetSafeNormal();
        }
        Cloth->UpdateMeshSection_LinearColor(SectionIndex, Section.Positions, Section.Normals, Section.UV0,
            TArray<FLinearColor>(), TArray<FProcMeshTangent>());
    }
}
