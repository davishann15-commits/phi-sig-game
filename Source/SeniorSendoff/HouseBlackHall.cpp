#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallLayout.h"
#include "HouseInteractiveProp.h"
#include "HouseWorldScale.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "KismetProceduralMeshLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshResources.h"
#include "UObject/UObjectIterator.h"

using namespace HallSpec;

// =============================================================================================
// Subsystem
bool UHouseBlackHallSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UMaterialInterface* UHouseBlackHallSubsystem::Mat(FName Key)
{
    if (TObjectPtr<UMaterialInterface>* Found = Materials.Find(Key)) return *Found;
    static UMaterialInterface* Fallback = nullptr;
    if (!Fallback) Fallback = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/Materials/MI_SSO_White.MI_SSO_White"));
    return Fallback;
}

void UHouseBlackHallSubsystem::CreateLook()
{
    if (bLookReady) return;
    bLookReady = true;
    HallCreateLook(this);
}

void UHouseBlackHallSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (UGameplayStatics::GetCurrentLevelName(&InWorld, true) != TEXT("Chapter01_House")) return;
    if (FParse::Param(FCommandLine::Get(), TEXT("NoBlackHallRebuild"))) return;
    const double Start = FPlatformTime::Seconds();
    CreateLook();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    AHouseBlackHallDetail* Built = InWorld.SpawnActor<AHouseBlackHallDetail>(AHouseBlackHallDetail::StaticClass(),
        HouseWorldScale::Transform(&InWorld, FTransform::Identity), Params);
    Detail = Built;
    if (!Built) return;
    Built->Build();
    if (Built->bRebuilt && InWorld.GetNetMode() != NM_Client)
    {
        SpawnDoors();
        SpawnLitter();
        // Old cut-outs replaced by the painted mural; furniture moved clear of the new doors and alcove.
        for (TActorIterator<AHouseInteractiveProp> It(&InWorld); It; ++It)
        {
            AHouseInteractiveProp* Prop = *It;
            const FString Mesh = Prop->IntactMesh ? Prop->IntactMesh->GetName() : FString();
            auto Is = [&](const TCHAR* Id) { return Mesh.EndsWith(Id); };
            if (Is(TEXT("Basement_ALL_049")) || Is(TEXT("Basement_ALL_050")) || Is(TEXT("Basement_ALL_051")) || Is(TEXT("Basement_ALL_052")) || Is(TEXT("Basement_ALL_065")))
            {
                Prop->Destroy();
                continue;
            }
            const FVector Ctr = Prop->GetComponentsBoundingBox(true).GetCenter();
            auto MoveTo = [&](const FVector& NewCenter, float Yaw)
            {
                const FVector Offset = Ctr - Prop->GetActorLocation();
                const FRotator R(0, Yaw, 0);
                Prop->SetActorLocationAndRotation(NewCenter - R.RotateVector(Offset), R + Prop->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
            };
            const float HouseScale = HouseWorldScale::Factor(&InWorld);
            if (Is(TEXT("Basement_ALL_027"))) MoveTo(FVector(Ctr.X, -671.f * HouseScale, Ctr.Z), 0);
            else if (Is(TEXT("Basement_ALL_044"))) MoveTo(FVector(1040.f * HouseScale, -780.f * HouseScale, Ctr.Z), 90.f);
            else if (Is(TEXT("Basement_ALL_061"))) MoveTo(FVector(Ctr.X, -296.f * HouseScale, Ctr.Z), 0);
            else if (Is(TEXT("Basement_ALL_072"))) MoveTo(FVector(Ctr.X, -660.f * HouseScale, Ctr.Z + 22.f * HouseScale), 0);
        }
    }
    UE_LOG(LogTemp, Display, TEXT("BLACKHALL_REBUILD rebuilt=%d tiles=%d collision=%d removed=%d built=%d doors=%d litter=%d ms=%.0f %s"),
        Built->bRebuilt ? 1 : 0, Built->ReplacedTiles, Built->ClippedCollision, Built->RemovedTriangles, Built->BuiltTriangles, Doors.Num(), Litter.Num(),
        (FPlatformTime::Seconds() - Start) * 1000.0, *Built->Notes);
}

void UHouseBlackHallSubsystem::SpawnDoors()
{
    UWorld* World = GetWorld();
    using namespace HallLayout;
    auto Spawn = [&](const FVector& Hinge, float Yaw, EHallDoorStyle Style, float Width, float RoomSign, uint8 Flags,
        const FVector& DoorwayCenter, FName Group, FName Id, bool bLocked)
    {
        const FTransform T = HouseWorldScale::Transform(World, FTransform(FRotator(0, Yaw, 0), Hinge));
        AHouseBlackHallDoor* Door = World->SpawnActorDeferred<AHouseBlackHallDoor>(AHouseBlackHallDoor::StaticClass(), T, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Door) return;
        Door->Setup(Style, Width, 213.f, RoomSign, Flags);
        Door->DoorwayCenter = HouseWorldScale::Point(World, DoorwayCenter);
        Door->DoorGroup = Group;
        Door->DoorId = Id;
        Door->bLocked = bLocked;
        Door->FinishSpawning(T);
        Doors.Add(Door);
    };
    // North wall patio pairs (in-swing; the hall is -Y). Hinge axis 1 cm in front of the wall face.
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FPairSpec& P = Pairs[Index];
        const float HL = P.Center - P.FrameWidth * .5f + Casing, HR = P.Center + P.FrameWidth * .5f - Casing;
        const float LeafW = (HR - HL - 2 * Liner - 0.8f - 0.4f) * .5f;
        const FName Group(*FString::Printf(TEXT("BH_PatioPair%d"), Index + 1));
        const FVector Center(P.Center, 800.f, ZF + 105.f);
        // Leaf at smaller X: local X = +X, local Y = +Y (outside) -> RoomSign -1. Active leaf (photo's right-hand leaf).
        Spawn(FVector(HL + Liner + 0.4f, YN - 1.f, ZF), 0.f, EHallDoorStyle::French, LeafW, -1.f, P.ActiveFlags, Center, Group,
            FName(*FString::Printf(TEXT("BH_Patio%d_A"), Index + 1)), false);
        // Leaf at larger X: local X = -X, local Y = -Y (hall) -> RoomSign +1.
        Spawn(FVector(HR - Liner - 0.4f, YN - 1.f, ZF), 180.f, EHallDoorStyle::French, LeafW, 1.f, P.PassiveFlags, Center, Group,
            FName(*FString::Printf(TEXT("BH_Patio%d_B"), Index + 1)), false);
    }
    // East exit door: out-swing, hinge on the +Y jamb at the exterior face; local X = -Y, local Y = +X (outside) -> RoomSign -1.
    Spawn(FVector(XE + WallT + 0.3f, ExitA1 - Liner - 0.4f, ZF), -90.f, EHallDoorStyle::Exit, ExitA1 - ExitA0 - 2 * Liner - 0.8f, -1.f,
        AHouseBlackHallDoor::OutSwing | AHouseBlackHallDoor::Closer, FVector(XE + 19.f, (ExitA0 + ExitA1) * .5f, ZF + 105.f), NAME_None, TEXT("BH_ExitDoor"), false);
    // Equipment closet: in-swing, locked. Hinge on the +Y jamb at the room face.
    Spawn(FVector(XE + 0.6f, EquipA1 - Liner - 0.4f, ZF), -90.f, EHallDoorStyle::Equipment, EquipA1 - EquipA0 - 2 * Liner - 0.8f, -1.f,
        AHouseBlackHallDoor::Lever | AHouseBlackHallDoor::Sign, FVector(XE - 2.f, (EquipA0 + EquipA1) * .5f, ZF + 105.f), NAME_None, TEXT("BH_EquipmentDoor"), true);
}

// =============================================================================================
// Detail actor: original shell replacement
AHouseBlackHallDetail::AHouseBlackHallDetail()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Root->SetMobility(EComponentMobility::Static);
    RootComponent = Root;
}

namespace
{
struct FHallClipVert { FVector P, N, T; FVector2D UV; FVector W; };

FHallClipVert HallLerpVert(const FHallClipVert& A, const FHallClipVert& B, float T)
{
    FHallClipVert R;
    R.P = FMath::Lerp(A.P, B.P, T); R.N = FMath::Lerp(A.N, B.N, T).GetSafeNormal(); R.T = FMath::Lerp(A.T, B.T, T).GetSafeNormal();
    R.UV = FMath::Lerp(A.UV, B.UV, T); R.W = FMath::Lerp(A.W, B.W, T);
    return R;
}

/** Removes the part of a convex polygon inside Box and appends the remaining (outside) convex pieces. */
void HallSubtractBox(const TArray<FHallClipVert>& Poly, const FBox& Box, TArray<TArray<FHallClipVert>>& Out)
{
    TArray<FHallClipVert> Inside = Poly;
    for (int32 Plane = 0; Plane < 6; ++Plane)
    {
        const int32 Axis = Plane / 2; const bool bMin = Plane % 2 == 0;
        const double Bound = bMin ? Box.Min[Axis] : Box.Max[Axis];
        auto F = [&](const FHallClipVert& V) { return bMin ? V.W[Axis] - Bound : Bound - V.W[Axis]; };
        TArray<FHallClipVert> In, Outer;
        for (int32 I = 0; I < Inside.Num(); ++I)
        {
            const FHallClipVert& A = Inside[I]; const FHallClipVert& B = Inside[(I + 1) % Inside.Num()];
            const double FA = F(A), FB = F(B);
            if (FA >= 0) In.Add(A); else Outer.Add(A);
            if ((FA >= 0) != (FB >= 0))
            {
                const FHallClipVert X = HallLerpVert(A, B, float(FA / (FA - FB)));
                In.Add(X); Outer.Add(X);
            }
        }
        if (Outer.Num() >= 3) Out.Add(Outer);
        Inside = MoveTemp(In);
        if (Inside.Num() < 3) return;
    }
    // Whatever remains is inside the box and is dropped.
}

bool HallRemoveTriangle(const FString& Mat, const FVector& C)
{
    static const FBox HallBox(FVector(-1243.f, -811.f, -331.f), FVector(1243.f, 811.f, -43.f));
    if (!HallBox.IsInside(C)) return false;
    if (Mat.Contains(TEXT("BlackBlock")) || Mat.Contains(TEXT("MortarDark")) || Mat.Contains(TEXT("PaintGrey")) || Mat.Contains(TEXT("WoodOak"))
        || Mat.Contains(TEXT("BHSealedConcrete")) || Mat.Contains(TEXT("BHTapeCharcoal")))
        return true;
    if (Mat.Contains(TEXT("PaintWhite"))) return C.Z < -100.f;
    if (Mat.Contains(TEXT("IdentityRed")) || Mat.Contains(TEXT("IdentityWhite"))) return FMath::Abs(C.Y) > 700.f || FMath::Abs(C.X) > 1150.f;
    return false;
}

struct FHallTileSection
{
    UMaterialInterface* Material = nullptr;
    TArray<FVector> V, N; TArray<FVector2D> UV; TArray<FProcMeshTangent> T; TArray<int32> I;
};

struct FHallTileJob
{
    UStaticMeshComponent* Comp = nullptr;
    bool bRender = false, bCollision = false;
    TArray<FHallTileSection> Render;       // kept render geometry
    TArray<FVector> CollisionV; TArray<int32> CollisionI;
    int32 Removed = 0; bool bChangedRender = false, bChangedCollision = false;
};

bool HallIsShellMesh(const FString& Name)
{
    return Name.StartsWith(TEXT("SM_Basement_")) || Name.StartsWith(TEXT("SM_OriginalRoom_Basement_")) || Name.StartsWith(TEXT("HI_ArchitecturalRemainder_SM_Basement_"))
        || Name.StartsWith(TEXT("SM_FPR_Retained_SM_Entrance_")) || Name.StartsWith(TEXT("SM_Entrance_")) || Name.StartsWith(TEXT("SM_OriginalRoom_Entrance_"));
}
}

void AHouseBlackHallDetail::ReplaceOriginalShell()
{
    UWorld* World = GetWorld();
    const FBox HallBox(FVector(-1243.f, -811.f, -331.f), FVector(1243.f, 811.f, -43.f));
    const TArray<FBox> Carves = HallLayout::CarveBoxes();
    TArray<FHallTileJob> Jobs;
    for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
    {
        UStaticMeshComponent* C = *It;
        if (!IsValid(C) || C->GetWorld() != World || !C->IsRegistered() || !C->GetStaticMesh() || C->IsA<UInstancedStaticMeshComponent>()) continue;
        if (C->GetOwner() == this) continue;
        const FString Name = C->GetStaticMesh()->GetName();
        const bool bShell = HallIsShellMesh(Name);
        const bool bCol = Name.StartsWith(TEXT("COL_")) || Name.StartsWith(TEXT("COLF_"));
        if (!bShell && !bCol) continue;
        // The carve layout is authored at the original scale. Keep all clip
        // decisions there, while copying each source's actual world transform.
        const FBox WorldBounds = C->Bounds.GetBox();
        const FBox B(HouseWorldScale::Unpoint(World, WorldBounds.Min), HouseWorldScale::Unpoint(World, WorldBounds.Max));
        bool bCarve = false; for (const FBox& K : Carves) bCarve |= B.Intersect(K);
        FHallTileJob Job; Job.Comp = C;
        // Only basement tiles are re-made for rendering; the tall entrance tiles keep their static mesh (and Nanite/caching).
        Job.bRender = bShell && Name.Contains(TEXT("Basement")) && C->IsVisible() && !C->bHiddenInGame && (B.Intersect(HallBox) || bCarve);
        Job.bCollision = bCarve && C->IsCollisionEnabled();
        if (Job.bRender || Job.bCollision) Jobs.Add(MoveTemp(Job));
    }
    // Pass 1: read and clip everything in memory; abort cleanly if any mesh data is unavailable.
    for (FHallTileJob& Job : Jobs)
    {
        UStaticMesh* Mesh = Job.Comp->GetStaticMesh();
        const FStaticMeshRenderData* RD = Mesh->GetRenderData();
        if (!RD || RD->LODResources.Num() == 0) { Notes += FString::Printf(TEXT(" no-renderdata:%s"), *Mesh->GetName()); return; }
        const FStaticMeshLODResources& LOD = RD->LODResources[0];
        const FTransform CT = Job.Comp->GetComponentTransform();
        for (int32 S = 0; S < LOD.Sections.Num(); ++S)
        {
            TArray<FVector> V, N; TArray<int32> Tri; TArray<FVector2D> UV; TArray<FProcMeshTangent> Tan;
            UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(Mesh, 0, S, V, Tri, N, UV, Tan);
            if (LOD.Sections[S].NumTriangles > 0 && (V.Num() == 0 || Tri.Num() == 0))
            {
                Notes += FString::Printf(TEXT(" no-cpu-data:%s"), *Mesh->GetName());
                return;
            }
            if (N.Num() != V.Num()) N.Init(FVector::UpVector, V.Num());
            if (UV.Num() != V.Num()) UV.Init(FVector2D::ZeroVector, V.Num());
            if (Tan.Num() != V.Num()) Tan.Init(FProcMeshTangent(FVector::ForwardVector, false), V.Num());
            UMaterialInterface* Material = Job.Comp->GetMaterial(LOD.Sections[S].MaterialIndex);
            const FString MatName = GetNameSafe(Material);
            FHallTileSection Out; Out.Material = Material;
            TMap<int32, int32> Remap;
            auto AddOriginal = [&](int32 Index)
            {
                if (int32* Found = Remap.Find(Index)) return *Found;
                const int32 New = Out.V.Num();
                Out.V.Add(V[Index]); Out.N.Add(N[Index]); Out.UV.Add(UV[Index]); Out.T.Add(Tan[Index]);
                Remap.Add(Index, New);
                return New;
            };
            auto AddVert = [&](const FHallClipVert& X)
            {
                const int32 New = Out.V.Num();
                Out.V.Add(X.P); Out.N.Add(X.N); Out.UV.Add(X.UV); Out.T.Add(FProcMeshTangent(X.T, false));
                return New;
            };
            for (int32 K = 0; K + 2 < Tri.Num(); K += 3)
            {
                const int32 I0 = Tri[K], I1 = Tri[K + 1], I2 = Tri[K + 2];
                const FVector W0 = HouseWorldScale::Unpoint(World, CT.TransformPosition(V[I0]));
                const FVector W1 = HouseWorldScale::Unpoint(World, CT.TransformPosition(V[I1]));
                const FVector W2 = HouseWorldScale::Unpoint(World, CT.TransformPosition(V[I2]));
                const FVector Ctr = (W0 + W1 + W2) / 3.f;
                FBox TB(ForceInit); TB += W0; TB += W1; TB += W2;
                bool bNearCarve = false; for (const FBox& Box : Carves) bNearCarve |= TB.Intersect(Box);
                // Collision copy: every triangle except what the carve boxes remove.
                if (Job.bCollision)
                {
                    if (!bNearCarve)
                    {
                        const int32 Base = Job.CollisionV.Num();
                        Job.CollisionV.Append({ V[I0], V[I1], V[I2] });
                        Job.CollisionI.Append({ Base, Base + 1, Base + 2 });
                    }
                    else
                    {
                        TArray<TArray<FHallClipVert>> Pieces{ { {V[I0], FVector::ZeroVector, FVector::ZeroVector, FVector2D::ZeroVector, W0},
                            {V[I1], FVector::ZeroVector, FVector::ZeroVector, FVector2D::ZeroVector, W1},
                            {V[I2], FVector::ZeroVector, FVector::ZeroVector, FVector2D::ZeroVector, W2} } };
                        for (const FBox& Box : Carves)
                        {
                            TArray<TArray<FHallClipVert>> Next;
                            for (const TArray<FHallClipVert>& Piece : Pieces)
                            {
                                FBox PB(ForceInit); for (const FHallClipVert& X : Piece) PB += X.W;
                                if (!PB.Intersect(Box)) { Next.Add(Piece); continue; }
                                TArray<TArray<FHallClipVert>> Kept;
                                HallSubtractBox(Piece, Box, Kept);
                                Next.Append(Kept);
                                Job.bChangedCollision = true;
                            }
                            Pieces = MoveTemp(Next);
                        }
                        for (const TArray<FHallClipVert>& Piece : Pieces)
                            for (int32 F = 1; F + 1 < Piece.Num(); ++F)
                            {
                                const int32 Base = Job.CollisionV.Num();
                                Job.CollisionV.Append({ Piece[0].P, Piece[F].P, Piece[F + 1].P });
                                Job.CollisionI.Append({ Base, Base + 1, Base + 2 });
                            }
                    }
                }
                if (!Job.bRender) continue;
                if (HallRemoveTriangle(MatName, Ctr)) { ++Job.Removed; Job.bChangedRender = true; continue; }
                if (!bNearCarve)
                {
                    Out.I.Append({ AddOriginal(I0), AddOriginal(I1), AddOriginal(I2) });
                    continue;
                }
                auto MakeV = [&](int32 Idx, const FVector& W) { return FHallClipVert{ V[Idx], N[Idx], Tan[Idx].TangentX, UV[Idx], W }; };
                TArray<TArray<FHallClipVert>> Pieces{ { MakeV(I0, W0), MakeV(I1, W1), MakeV(I2, W2) } };
                bool bClipped = false;
                for (const FBox& Box : Carves)
                {
                    TArray<TArray<FHallClipVert>> Next;
                    for (const TArray<FHallClipVert>& Piece : Pieces)
                    {
                        FBox PB(ForceInit); for (const FHallClipVert& X : Piece) PB += X.W;
                        if (!PB.Intersect(Box)) { Next.Add(Piece); continue; }
                        TArray<TArray<FHallClipVert>> Kept;
                        HallSubtractBox(Piece, Box, Kept);
                        Next.Append(Kept);
                        bClipped = true;
                    }
                    Pieces = MoveTemp(Next);
                }
                if (!bClipped) { Out.I.Append({ AddOriginal(I0), AddOriginal(I1), AddOriginal(I2) }); continue; }
                Job.bChangedRender = true;
                for (const TArray<FHallClipVert>& Piece : Pieces)
                {
                    const int32 A = AddVert(Piece[0]);
                    for (int32 F = 1; F + 1 < Piece.Num(); ++F) Out.I.Append({ A, AddVert(Piece[F]), AddVert(Piece[F + 1]) });
                }
            }
            if (Job.bRender) Job.Render.Add(MoveTemp(Out));
        }
    }
    // Pass 2: apply.
    for (FHallTileJob& Job : Jobs)
    {
        UStaticMeshComponent* C = Job.Comp;
        if (Job.bRender && Job.bChangedRender)
        {
            UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, NAME_None);
            PM->SetupAttachment(RootComponent);
            PM->SetMobility(EComponentMobility::Static);
            PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PM->SetCastShadow(C->CastShadow);
            PM->SetWorldTransform(C->GetComponentTransform());
            PM->RegisterComponent();
            int32 Index = 0;
            for (const FHallTileSection& S : Job.Render)
            {
                if (S.I.Num() == 0) continue;
                PM->CreateMeshSection(Index, S.V, S.I, S.N, S.UV, TArray<FColor>(), S.T, false);
                PM->SetMaterial(Index, S.Material);
                ++Index;
            }
            C->SetVisibility(false);
            C->SetHiddenInGame(true);
            Meshes.Add(PM);
            ++ReplacedTiles;
            RemovedTriangles += Job.Removed;
        }
        if (Job.bCollision && Job.bChangedCollision)
        {
            UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, NAME_None);
            PM->SetupAttachment(RootComponent);
            PM->SetMobility(EComponentMobility::Static);
            PM->bUseComplexAsSimpleCollision = true;
            PM->bUseAsyncCooking = false;
            PM->SetCollisionProfileName(C->GetCollisionProfileName());
            PM->SetCollisionObjectType(C->GetCollisionObjectType());
            PM->SetCollisionResponseToChannels(C->GetCollisionResponseToChannels());
            PM->SetCollisionEnabled(C->GetCollisionEnabled());
            PM->SetVisibility(false);
            PM->SetHiddenInGame(true);
            PM->SetWorldTransform(C->GetComponentTransform());
            PM->RegisterComponent();
            TArray<FVector> Normals; Normals.Init(FVector::UpVector, Job.CollisionV.Num());
            TArray<FVector2D> UVs; UVs.Init(FVector2D::ZeroVector, Job.CollisionV.Num());
            PM->CreateMeshSection(0, Job.CollisionV, Job.CollisionI, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Meshes.Add(PM);
            ++ClippedCollision;
        }
    }
    bRebuilt = true;
}

void AHouseBlackHallDetail::Build()
{
    ReplaceOriginalShell();
    if (bRebuilt) BuildArchitecture();
}

// =============================================================================================
// Doors
AHouseBlackHallDoor::AHouseBlackHallDoor()
{
    bLocalLeafSpace = true;
}

void AHouseBlackHallDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AHouseBlackHallDoor, StyleValue, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseBlackHallDoor, LeafWidth, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseBlackHallDoor, LeafHeight, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseBlackHallDoor, RoomSign, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(AHouseBlackHallDoor, Flags, COND_InitialOnly);
}

void AHouseBlackHallDoor::Setup(EHallDoorStyle InStyle, float InWidth, float InHeight, float InRoomSign, uint8 InFlags)
{
    StyleValue = (uint8)InStyle; LeafWidth = InWidth; LeafHeight = InHeight; RoomSign = InRoomSign >= 0 ? 1.f : -1.f; Flags = InFlags;
    const float T = 4.4f;
    const bool bOut = (Flags & OutSwing) != 0;
    const float CenterY = bOut ? RoomSign * T * .5f : -RoomSign * T * .5f;
    PanelCenterWorld = FVector(LeafWidth * .5f, CenterY, LeafHeight * .5f + .6f);
    PanelHalfExtent = FVector(LeafWidth * .5f, T * .5f + 1.f, LeafHeight * .5f);
    ClosedYaw = 0.f;
    OpenYaw = (bOut ? -RoomSign : RoomSign) * 92.f;
}

void AHouseBlackHallDoor::BeginPlay()
{
    Super::BeginPlay();
    if (UBoxComponent* Panel = GetPanelCollision()) Panel->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    BuildVisual();
}

void AHouseBlackHallDoor::BuildVisual()
{
    UHouseBlackHallSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UHouseBlackHallSubsystem>() : nullptr;
    if (!Sub || Visual) return;
    const float W = LeafWidth, H = LeafHeight, T = 4.4f, S = RoomSign;
    const bool bOut = (Flags & OutSwing) != 0;
    // Leaf occupies local y between Y0 and Y1; RoomY is the face toward the hall, OutY the other face.
    const float RoomY = bOut ? S * T : 0.f, OutY = bOut ? 0.f : -S * T;
    const float Y0 = FMath::Min(RoomY, OutY), Y1 = FMath::Max(RoomY, OutY), YC = (Y0 + Y1) * .5f;
    const EHallDoorStyle Style = (EHallDoorStyle)StyleValue;
    FHallMesh M;
    FHallUV LeafUV; LeafUV.Mode = 2; LeafUV.Frame = FTransform(FVector(W * .5f, YC, H * .5f)); LeafUV.BoxSize = FVector(W, T, H);
    const FHallUV Plain = FHallUV::World(20.f);
    const FName Paint = Style == EHallDoorStyle::Exit ? FName(TEXT("ExitWhite")) : FName(TEXT("DoorWhite"));
    auto LBox = [&](FName Key, float X0, float X1, float YA, float YB, float Z0, float Z1, const FHallUV& UV)
    {
        M.BoxMinMax(Key, FVector(X0, FMath::Min(YA, YB), Z0), FVector(X1, FMath::Max(YA, YB), Z1), UV);
    };
    const float Bottom = .6f;
    if (Style == EHallDoorStyle::French)
    {
        const float Stile = 11.f, TopRail = 11.f, BottomRail = 24.f;
        LBox(Paint, 0, Stile, Y0, Y1, Bottom, H, LeafUV);
        LBox(Paint, W - Stile, W, Y0, Y1, Bottom, H, LeafUV);
        LBox(Paint, Stile, W - Stile, Y0, Y1, H - TopRail, H, LeafUV);
        LBox(Paint, Stile, W - Stile, Y0, Y1, Bottom, BottomRail, LeafUV);
        // Glass with glazing beads on both faces.
        LBox(TEXT("Glass"), Stile - .6f, W - Stile + .6f, YC - .3f, YC + .3f, BottomRail - .6f, H - TopRail + .6f, Plain);
        for (const float Face : { Y0, Y1 })
        {
            const float Dir = Face < YC ? -1.f : 1.f;
            const float A = Face, B = Face + Dir * 1.1f;
            LBox(Paint, Stile, Stile + 1.6f, A, B, BottomRail, H - TopRail, LeafUV);
            LBox(Paint, W - Stile - 1.6f, W - Stile, A, B, BottomRail, H - TopRail, LeafUV);
            LBox(Paint, Stile, W - Stile, A, B, H - TopRail - 1.6f, H - TopRail, LeafUV);
            LBox(Paint, Stile, W - Stile, A, B, BottomRail, BottomRail + 1.6f, LeafUV);
            if (Flags & KickPlate) LBox(TEXT("KickBlack"), 1.5f, W - 1.5f, Face, Face + Dir * .15f, 1.2f, 16.f, Plain);
        }
    }
    else
    {
        LBox(Paint, 0, W, Y0, Y1, Bottom, H, LeafUV);
    }
    const float RoomDir = (RoomY >= YC) ? 1.f : -1.f;
    // Lever handles (and deadbolt) on the latch stile.
    if (Flags & Lever)
        for (const float Face : { Y0, Y1 })
        {
            const float Dir = Face < YC ? -1.f : 1.f;
            M.Cylinder(TEXT("Chrome"), FVector(W - 6.5f, Face, 97.f), FVector(W - 6.5f, Face + Dir * 1.2f, 97.f), 2.8f, 14, Plain);
            M.Cylinder(TEXT("Chrome"), FVector(W - 6.5f, Face + Dir * 1.2f, 97.f), FVector(W - 6.5f, Face + Dir * 5.5f, 97.f), .9f, 10, Plain);
            M.BoxMinMax(TEXT("Chrome"), FVector(W - 19.f, FMath::Min(Face + Dir * 4.6f, Face + Dir * 6.2f), 96.f), FVector(W - 6.f, FMath::Max(Face + Dir * 4.6f, Face + Dir * 6.2f), 98.f), Plain);
            if (Flags & Deadbolt)
            {
                M.Cylinder(TEXT("Chrome"), FVector(W - 6.5f, Face, 111.f), FVector(W - 6.5f, Face + Dir * 1.f, 111.f), 2.8f, 14, Plain);
                if (Dir == RoomDir) M.BoxMinMax(TEXT("Chrome"), FVector(W - 7.1f, FMath::Min(Face + Dir, Face + Dir * 2.4f), 109.5f), FVector(W - 5.9f, FMath::Max(Face + Dir, Face + Dir * 2.4f), 112.5f), Plain);
            }
        }
    if (Flags & FlushBolts)
    {
        LBox(TEXT("Aluminum"), W, W + .25f, YC - 1.f, YC + 1.f, H - 24.f, H - 12.f, Plain);
        LBox(TEXT("Aluminum"), W, W + .25f, YC - 1.f, YC + 1.f, 6.f, 18.f, Plain);
    }
    // Door closer body + arm on the hall side of the top rail.
    if (Flags & Closer)
    {
        const float X0 = Style == EHallDoorStyle::Exit ? 8.f : 6.f;
        LBox(TEXT("MetalDark"), X0, X0 + 28.f, RoomY, RoomY + RoomDir * 5.5f, H - 9.5f, H - 2.5f, Plain);
        LBox(TEXT("MetalDark"), X0 + 28.f, X0 + 50.f, RoomY + RoomDir * 4.5f, RoomY + RoomDir * 6.f, H - 5.f, H - 3.5f, Plain);
        LBox(TEXT("MetalDark"), X0 + 48.f, X0 + 50.f, RoomY + RoomDir * 4.5f, RoomY + RoomDir * 6.f, H - 3.5f, H + 7.f, Plain);
    }
    // Push bar on the exit door.
    if (Style == EHallDoorStyle::Exit)
    {
        LBox(TEXT("Aluminum"), W - 24.f, W - 4.f, RoomY, RoomY + RoomDir * 7.f, 90.f, 105.f, Plain);
        LBox(TEXT("Aluminum"), 6.f, 15.f, RoomY, RoomY + RoomDir * 6.f, 93.f, 103.f, Plain);
        LBox(TEXT("Aluminum"), 15.f, W - 24.f, RoomY + RoomDir * 3.5f, RoomY + RoomDir * 6.8f, 94.5f, 101.5f, Plain);
        LBox(TEXT("RubberBlack"), W - 24.f, W - 22.f, RoomY + RoomDir * 3.5f, RoomY + RoomDir * 6.8f, 94.5f, 101.5f, Plain);
    }
    // Equipment closet sign.
    if (Flags & Sign)
    {
        FHallUV SignUV; SignUV.Mode = 2; SignUV.Frame = FTransform(FVector(W * .5f, RoomY + RoomDir * .15f, 161.f)); SignUV.BoxSize = FVector(44.f, .3f, 22.f);
        LBox(TEXT("SignEquipment"), W * .5f - 22.f, W * .5f + 22.f, RoomY, RoomY + RoomDir * .3f, 150.f, 172.f, SignUV);
    }
    // Hinge knuckles on the swing side.
    const float KnuckleY = bOut ? OutY - RoomDir * .6f : RoomY + RoomDir * .6f;
    for (const float Z : { 22.f, H * .5f + 6.f, H - 26.f })
        M.Cylinder(TEXT("MetalDark"), FVector(-.3f, KnuckleY, Z - 5.f), FVector(-.3f, KnuckleY, Z + 5.f), 1.f, 10, Plain);

    Visual = NewObject<UProceduralMeshComponent>(this, TEXT("HallDoorVisual"));
    Visual->SetupAttachment(GetLeafPivot());
    Visual->bUseComplexAsSimpleCollision = true;
    Visual->bUseAsyncCooking = false;
    Visual->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Visual->SetCollisionResponseToAllChannels(ECR_Ignore);
    Visual->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Visual->RegisterComponent();
    M.Build(Visual, [Sub](FName Key) { return Sub->Mat(Key); }, true);
}
