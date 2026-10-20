// -CalderVerifyProbe (the Calder 1d/1e verification, Saved/CalderWork/plan/integration_1de_verify.md; diagnosis only): once the
// terrain's collision is ready (8 s in), writes what an independent check of the hard rules needs to Saved/CalderVerifyProbe/
// (each file deleted first), logs CALDER_VERIFY_* lines and ends the run unless another self-exiting review shares it. It
// changes nothing in the world.
//  - frozen.csv: inside every frozen rectangle (HouseCalderTerrain::FrozenRects, 25 cm in), on a 50 cm grid, the first static
//    surface from above (8000 down to -1500): on the WorldStatic objects traced complex and on the Pawn channel traced simple
//    (every pawn ignored): its Z (0.1 cm) and its component's name. A -NoCalderTerrain run writes the same file; the two must
//    match point for point.
//  - parts.csv: every primitive component in the world: owner, name, class, visible, collision, cast shadow, cull distance,
//    bounds; for every Calder* part, what of it lies inside a frozen rectangle (2 cm in): procedural-mesh (collision)
//    triangles with a vertex inside, instances whose origin is inside, a static mesh's vertices inside (when its CPU copy is
//    kept; else -1 and its bounds' overlap), with up to 6 of those points. CALDER_VERIFY_PARTS logs the totals.
//  - water.csv: every 50 cm over the open water the water builder reports (HouseCalderWater::WaterAt), the first floor the
//    player could stand on under it (Pawn channel, the pawn's own walkable rule, Unwalkable parts skipped) and the depth, and
//    whether a bank (dry standing ground from 60 cm under the water's level to 45 cm over it) lies within 3 m; dry.csv the dry
//    standing ground round it. CALDER_VERIFY_WATER logs the deepest point, the points over 120 cm and the points over 60 cm
//    within 3 m of a bank.
//  - lights.csv: every light component in the world (owner, name, class, location); CALDER_VERIFY_LIGHTS logs the counts.
#if !UE_BUILD_SHIPPING
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExterior.h"
#include "HouseExteriorSiteLayout.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

namespace HouseCalderVerifyProbeImpl
{
struct FVerifyState { TWeakObjectPtr<UWorld> World; double Start = 0.0; bool bDone = false; };
FVerifyState& VerifyState() { static FVerifyState S; return S; }

bool VerifySharedRun()
{
    static const TCHAR* const SelfExiting[] = {
        TEXT("FreeRoamWalkReview"), TEXT("ParkingLotWalkReview"), TEXT("WoodsWalkReview"), TEXT("ClockTowerReview"), TEXT("HouseExteriorViews"),
        TEXT("CampusInteriorsReview"), TEXT("CampusInteriorsSmoke"), TEXT("HouseExteriorWalkReview"), TEXT("HouseWalkPerf"), TEXT("CampusDrumsRecord"),
        TEXT("WestFieldWalkReview"), TEXT("CampusLayoutReview"), TEXT("CalderTerrainReview"), TEXT("CalderReachReview"), TEXT("CalderProbe") };
    TArray<FString> Tokens, Switches;
    FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
    for (FString Sw : Switches)
    {
        int32 Eq = INDEX_NONE;
        if (Sw.FindChar(TEXT('='), Eq)) Sw.LeftInline(Eq);
        for (const TCHAR* Name : SelfExiting) if (Sw.Equals(Name, ESearchCase::CaseSensitive)) return true;
    }
    return false;
}

bool InsideFrozen(const FVector& P, double Inset)
{
    for (const FBox2D& R : HouseCalderTerrain::FrozenRects())
        if (P.X > R.Min.X + Inset && P.X < R.Max.X - Inset && P.Y > R.Min.Y + Inset && P.Y < R.Max.Y - Inset) return true;
    return false;
}

double FrozenOverlap(const FBox& B)
{
    double Area = 0.0;
    for (const FBox2D& R : HouseCalderTerrain::FrozenRects())
    {
        const double W = FMath::Min(B.Max.X, double(R.Max.X)) - FMath::Max(B.Min.X, double(R.Min.X));
        const double H = FMath::Min(B.Max.Y, double(R.Max.Y)) - FMath::Max(B.Min.Y, double(R.Min.Y));
        if (W > 0.0 && H > 0.0) Area += W * H;
    }
    return Area;
}

void WriteFrozen(UWorld* W, const FString& Dir)
{
    FCollisionQueryParams Complex(SCENE_QUERY_STAT(CalderVerifyFrozen), true);
    FCollisionQueryParams Simple(SCENE_QUERY_STAT(CalderVerifyFrozenPawn), false);
    for (TActorIterator<APawn> It(W); It; ++It) { Complex.AddIgnoredActor(*It); Simple.AddIgnoredActor(*It); }
    const FCollisionObjectQueryParams Statics(ECC_WorldStatic);
    TArray<FString> Rows;
    Rows.Add(TEXT("rect,x,y,staticZ,staticPart,pawnZ,pawnPart"));
    int32 Points = 0, StaticHits = 0, CalderHits = 0;
    const TArray<FBox2D>& Rects = HouseCalderTerrain::FrozenRects();
    for (int32 K = 0; K < Rects.Num(); ++K)
    {
        const FBox2D& R = Rects[K];
        for (double X = R.Min.X + 25.0; X <= R.Max.X - 25.0; X += 50.0)
            for (double Y = R.Min.Y + 25.0; Y <= R.Max.Y - 25.0; Y += 50.0)
            {
                ++Points;
                FHitResult A, B;
                const bool bA = W->LineTraceSingleByObjectType(A, FVector(X, Y, 8000.0), FVector(X, Y, -1500.0), Statics, Complex) && A.GetComponent();
                const bool bB = W->LineTraceSingleByChannel(B, FVector(X, Y, 8000.0), FVector(X, Y, -1500.0), ECC_Pawn, Simple) && B.GetComponent();
                StaticHits += bA ? 1 : 0;
                if ((bA && A.GetComponent()->GetName().StartsWith(TEXT("Calder"))) || (bB && B.GetComponent()->GetName().StartsWith(TEXT("Calder")))) ++CalderHits;
                Rows.Add(FString::Printf(TEXT("%d,%.0f,%.0f,%s,%s,%s,%s"), K, X, Y,
                    bA ? *FString::Printf(TEXT("%.1f"), A.ImpactPoint.Z) : TEXT("-"), bA ? *A.GetComponent()->GetName() : TEXT("-"),
                    bB ? *FString::Printf(TEXT("%.1f"), B.ImpactPoint.Z) : TEXT("-"), bB ? *B.GetComponent()->GetName() : TEXT("-")));
            }
    }
    FFileHelper::SaveStringArrayToFile(Rows, *(Dir / TEXT("frozen.csv")));
    UE_LOG(LogTemp, Display, TEXT("CALDER_VERIFY_FROZEN points=%d staticHits=%d calderFirstHits=%d terrain=%d"), Points, StaticHits, CalderHits, int32(HouseCalderTerrain::Enabled()));
}

void WriteParts(UWorld* W, const FString& Dir)
{
    TArray<FString> Rows;
    Rows.Add(TEXT("owner,name,class,visible,collision,castShadow,cullDistance,minX,minY,minZ,maxX,maxY,maxZ,frozenTris,frozenInstances,frozenVerts,frozenBoundsM2,frozenSamples"));
    int32 Parts = 0, CalderParts = 0, FrozenTris = 0, FrozenInstances = 0, FrozenVerts = 0, NoCpu = 0, Shadowed = 0, NoCull = 0;
    TArray<FString> Worst;
    for (TActorIterator<AActor> It(W); It; ++It)
    {
        TArray<UPrimitiveComponent*> Comps;
        It->GetComponents(Comps);
        for (UPrimitiveComponent* C : Comps)
        {
            if (!C || !C->IsRegistered()) continue;
            ++Parts;
            const FString Name = C->GetName();
            const bool bCalder = Name.StartsWith(TEXT("Calder"));
            const FBox B = C->Bounds.GetBox();
            int32 Tris = 0, Insts = 0, Verts = 0;
            double Overlap = 0.0;
            FString Samples;   // up to 6 of the points inside a frozen rectangle, "x:y:z" joined by '|'
            auto Sample = [&Samples](const FVector& V) { if (Samples.Len() < 120) Samples += FString::Printf(TEXT("%s%.1f:%.1f:%.1f"), Samples.IsEmpty() ? TEXT("") : TEXT("|"), V.X, V.Y, V.Z); };
            float Cull = C->LDMaxDrawDistance;
            if (const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(C)) Cull = float(Ism->InstanceEndCullDistance);
            if (bCalder)
            {
                ++CalderParts;
                if (UProceduralMeshComponent* Pm = Cast<UProceduralMeshComponent>(C))
                {
                    const FTransform T = Pm->GetComponentTransform();
                    for (int32 Sec = 0; Sec < Pm->GetNumSections(); ++Sec)
                        if (const FProcMeshSection* Section = Pm->GetProcMeshSection(Sec))
                            for (int32 I = 0; I + 2 < Section->ProcIndexBuffer.Num(); I += 3)
                            {
                                bool bIn = false;
                                for (int32 V = 0; V < 3 && !bIn; ++V)
                                {
                                    const uint32 Index = Section->ProcIndexBuffer[I + V];
                                    if (Index >= uint32(Section->ProcVertexBuffer.Num())) continue;
                                    const FVector Wv = T.TransformPosition(Section->ProcVertexBuffer[Index].Position);
                                    bIn = InsideFrozen(Wv, 2.0);
                                    if (bIn) Sample(Wv);
                                }
                                Tris += bIn ? 1 : 0;
                            }
                }
                else if (const UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(C))
                {
                    for (int32 I = 0; I < Ism->GetInstanceCount(); ++I)
                    {
                        FTransform T;
                        if (Ism->GetInstanceTransform(I, T, true) && !T.GetScale3D().IsNearlyZero() && InsideFrozen(T.GetLocation(), 2.0)) { ++Insts; Sample(T.GetLocation()); }
                    }
                }
                else if (const UStaticMeshComponent* Sm = Cast<UStaticMeshComponent>(C))
                {
                    const UStaticMesh* Mesh = Sm->GetStaticMesh();
                    const FStaticMeshRenderData* Rd = Mesh ? Mesh->GetRenderData() : nullptr;
                    if (Rd && Rd->LODResources.Num() > 0 && Rd->LODResources[0].VertexBuffers.PositionVertexBuffer.GetVertexData())
                    {
                        const FPositionVertexBuffer& Pb = Rd->LODResources[0].VertexBuffers.PositionVertexBuffer;
                        const FTransform T = Sm->GetComponentTransform();
                        for (uint32 I = 0; I < Pb.GetNumVertices(); ++I)
                        {
                            const FVector Wv = T.TransformPosition(FVector(Pb.VertexPosition(I)));
                            if (!InsideFrozen(Wv, 2.0)) continue;
                            ++Verts;
                            Sample(Wv);
                        }
                    }
                    else
                    {
                        Verts = -1;
                        ++NoCpu;
                    }
                }
                Overlap = FrozenOverlap(B) / 10000.0;
                FrozenTris += Tris;
                FrozenInstances += Insts;
                FrozenVerts += FMath::Max(0, Verts);
                if ((Tris > 0 || Insts > 0 || Verts > 0) && Worst.Num() < 12) Worst.Add(FString::Printf(TEXT("%s(tris=%d inst=%d verts=%d)"), *Name, Tris, Insts, Verts));
                if (C->CastShadow && Name.StartsWith(TEXT("CalderWater_"))) ++Shadowed;
                if (C->IsVisible() && Cull <= 0.f) ++NoCull;
            }
            Rows.Add(FString::Printf(TEXT("%s,%s,%s,%d,%d,%d,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%.0f,%d,%d,%d,%.1f,%s"), *It->GetName(), *Name, *C->GetClass()->GetName(),
                int32(C->IsVisible()), int32(C->GetCollisionEnabled()), int32(C->CastShadow), Cull, B.Min.X, B.Min.Y, B.Min.Z, B.Max.X, B.Max.Y, B.Max.Z, Tris, Insts, Verts, Overlap, *Samples));
        }
    }
    FFileHelper::SaveStringArrayToFile(Rows, *(Dir / TEXT("parts.csv")));
    UE_LOG(LogTemp, Display, TEXT("CALDER_VERIFY_PARTS parts=%d calderParts=%d frozenTris=%d frozenInstances=%d frozenVerts=%d noCpuCopy=%d shadowedWater=%d visibleCalderWithoutCull=%d worst=%s"),
        Parts, CalderParts, FrozenTris, FrozenInstances, FrozenVerts, NoCpu, Shadowed, NoCull, Worst.Num() ? *FString::Join(Worst, TEXT(";")) : TEXT("none"));
}

void WriteLights(UWorld* W, const FString& Dir)
{
    TArray<FString> Rows;
    Rows.Add(TEXT("owner,name,class,visible,x,y,z"));
    int32 Total = 0, Point = 0, Spot = 0, Rect = 0, Calder = 0;
    for (TActorIterator<AActor> It(W); It; ++It)
    {
        TArray<ULightComponent*> Lights;
        It->GetComponents(Lights);
        for (const ULightComponent* L : Lights)
        {
            if (!L || !L->IsRegistered()) continue;
            ++Total;
            Point += L->IsA<UPointLightComponent>() && !L->IsA<USpotLightComponent>() ? 1 : 0;
            Spot += L->IsA<USpotLightComponent>() ? 1 : 0;
            Rect += L->IsA<URectLightComponent>() ? 1 : 0;
            Calder += L->GetName().StartsWith(TEXT("Calder")) || It->GetName().StartsWith(TEXT("Calder")) ? 1 : 0;
            const FVector P = L->GetComponentLocation();
            Rows.Add(FString::Printf(TEXT("%s,%s,%s,%d,%.0f,%.0f,%.0f"), *It->GetName(), *L->GetName(), *L->GetClass()->GetName(), int32(L->IsVisible()), P.X, P.Y, P.Z));
        }
    }
    FFileHelper::SaveStringArrayToFile(Rows, *(Dir / TEXT("lights.csv")));
    UE_LOG(LogTemp, Display, TEXT("CALDER_VERIFY_LIGHTS total=%d point=%d spot=%d rect=%d calderNamed=%d"), Total, Point, Spot, Rect, Calder);
}

void WriteWater(UWorld* W, APawn* Pawn, const FString& Dir)
{
    FBox Region(ForceInit);
    for (TActorIterator<AActor> It(W); It; ++It)
    {
        TArray<UPrimitiveComponent*> Comps;
        It->GetComponents(Comps);
        for (const UPrimitiveComponent* C : Comps) if (C && C->IsRegistered() && C->GetName().StartsWith(TEXT("CalderWater_"))) Region += C->Bounds.GetBox();
    }
    if (!Region.IsValid)
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_VERIFY_WATER none (no CalderWater_ parts)"));
        return;
    }
    const UCharacterMovementComponent* Move = Pawn ? Pawn->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderVerifyWater), false);
    for (TActorIterator<APawn> It(W); It; ++It) Q.AddIgnoredActor(*It);
    // The floor a player could stand on under Top: the first upward hit that is walkable by the pawn's rule and not an Unwalkable part.
    auto StandFloor = [W, Move, &Q](const FVector2D& P, double Top, double Bottom, float& OutZ)
    {
        FCollisionQueryParams Local = Q;
        for (int32 Pass = 0; Pass < 6; ++Pass)
        {
            FHitResult Hit;
            if (!W->LineTraceSingleByChannel(Hit, FVector(P, Top), FVector(P, Bottom), ECC_Pawn, Local) || !Hit.GetComponent()) return false;
            const bool bUnwalkable = Hit.GetComponent()->GetWalkableSlopeOverride().GetWalkableSlopeBehavior() == WalkableSlope_Unwalkable;
            const bool bWalk = !bUnwalkable && (Move ? Move->IsWalkable(Hit) : Hit.ImpactNormal.Z >= .71f) && !Hit.bStartPenetrating;
            if (bWalk) { OutZ = float(Hit.ImpactPoint.Z); return true; }
            Local.AddIgnoredComponent(Hit.GetComponent());
        }
        return false;
    };
    constexpr double Step = 50.0;
    const double X0 = FMath::FloorToDouble((Region.Min.X - 300.0) / Step) * Step, Y0 = FMath::FloorToDouble((Region.Min.Y - 300.0) / Step) * Step;
    const int32 NX = int32((Region.Max.X + 300.0 - X0) / Step) + 1, NY = int32((Region.Max.Y + 300.0 - Y0) / Step) + 1;
    // 0 nothing to stand on, 1 water, 2 dry standing ground
    TArray<uint8> Kind;
    TArray<float> WaterZ, FloorZ;
    TArray<FString> DryRows;
    DryRows.Add(TEXT("x,y,floorZ,part"));
    Kind.SetNumZeroed(NX * NY);
    WaterZ.SetNumZeroed(NX * NY);
    FloorZ.SetNumZeroed(NX * NY);
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const FVector2D P(X0 + I * Step, Y0 + J * Step);
            const int32 K = J * NX + I;
            float Z = 0.f, F = 0.f;
            if (HouseCalderWater::WaterAt(P, Z))
            {
                Kind[K] = 1;
                WaterZ[K] = Z;
                FloorZ[K] = StandFloor(P, Z + 40.0, Z - 600.0, F) ? F : -1e9f;
                continue;
            }
            const float G = HouseExteriorSite::GroundHeight(float(P.X), float(P.Y));
            if (StandFloor(P, G + 300.0, G - 300.0, F)) { Kind[K] = 2; FloorZ[K] = F; DryRows.Add(FString::Printf(TEXT("%.0f,%.0f,%.1f"), P.X, P.Y, F)); }
        }
    TArray<FString> Rows;
    Rows.Add(TEXT("x,y,waterZ,floorZ,depth,bankWithin3m"));
    int32 Water = 0, NoFloor = 0, Over120 = 0, Over60Near = 0;
    float Deepest = 0.f;
    FVector2D DeepAt = FVector2D::ZeroVector;
    TArray<FString> Bad;
    constexpr int32 Reach = 6;   // 3 m at 50 cm
    for (int32 J = 0; J < NY; ++J)
        for (int32 I = 0; I < NX; ++I)
        {
            const int32 K = J * NX + I;
            if (Kind[K] != 1) continue;
            ++Water;
            const FVector2D P(X0 + I * Step, Y0 + J * Step);
            bool bNear = false;
            for (int32 DJ = -Reach; DJ <= Reach && !bNear; ++DJ)
                for (int32 DI = -Reach; DI <= Reach && !bNear; ++DI)
                {
                    if (DI * DI + DJ * DJ > Reach * Reach) continue;
                    const int32 II = I + DI, JJ = J + DJ;
                    if (II < 0 || JJ < 0 || II >= NX || JJ >= NY) continue;
                    // a bank: dry standing ground near the water's level (from 60 cm under it to a step over it), not a deck or a
                    // ledge far above it, nor ground far below it
                    bNear = Kind[JJ * NX + II] == 2 && FloorZ[JJ * NX + II] <= WaterZ[K] + 45.f && FloorZ[JJ * NX + II] >= WaterZ[K] - 60.f;
                }
            const bool bFloor = FloorZ[K] > -1e8f;
            const float Depth = bFloor ? WaterZ[K] - FloorZ[K] : -1.f;
            if (!bFloor) ++NoFloor;
            if (Depth > Deepest) { Deepest = Depth; DeepAt = P; }
            if (Depth > 120.f) { ++Over120; if (Bad.Num() < 12) Bad.Add(FString::Printf(TEXT("(%.0f,%.0f) %.0f"), P.X, P.Y, Depth)); }
            else if (Depth > 60.f && bNear) { ++Over60Near; if (Bad.Num() < 12) Bad.Add(FString::Printf(TEXT("(%.0f,%.0f) %.0f near bank"), P.X, P.Y, Depth)); }
            Rows.Add(FString::Printf(TEXT("%.0f,%.0f,%.1f,%s,%.1f,%d"), P.X, P.Y, WaterZ[K], bFloor ? *FString::Printf(TEXT("%.1f"), FloorZ[K]) : TEXT("-"), Depth, int32(bNear)));
        }
    FFileHelper::SaveStringArrayToFile(Rows, *(Dir / TEXT("water.csv")));
    FFileHelper::SaveStringArrayToFile(DryRows, *(Dir / TEXT("dry.csv")));
    UE_LOG(LogTemp, Display, TEXT("CALDER_VERIFY_WATER region=(%.0f,%.0f)-(%.0f,%.0f) waterPoints=%d noFloor=%d deepest=%.1f at=(%.0f,%.0f) over120=%d over60NearBank=%d worst=%s"),
        Region.Min.X, Region.Min.Y, Region.Max.X, Region.Max.Y, Water, NoFloor, Deepest, DeepAt.X, DeepAt.Y, Over120, Over60Near, Bad.Num() ? *FString::Join(Bad, TEXT(";")) : TEXT("none"));
}

void VerifyTick(UWorld* W, ELevelTick, float)
{
    if (!W || !W->IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("CalderVerifyProbe"))) return;
    if (UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    FVerifyState& S = VerifyState();
    if (S.World.Get() != W) { S = FVerifyState(); S.World = W; S.Start = W->GetTimeSeconds(); }
    if (S.bDone) return;
    bool bExterior = false;
    for (TActorIterator<AHouseExteriorDetail> It(W); It; ++It) bExterior = true;
    APlayerController* PC = W->GetFirstPlayerController();
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!bExterior || !Pawn || W->GetTimeSeconds() - S.Start < 8.0 || !HouseCalderTerrain::CollisionReady(W)) return;
    S.bDone = true;
    const double Began = FPlatformTime::Seconds();
    const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CalderVerifyProbe"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    for (const TCHAR* File : { TEXT("frozen.csv"), TEXT("parts.csv"), TEXT("lights.csv"), TEXT("water.csv"), TEXT("dry.csv") }) IFileManager::Get().Delete(*(Dir / File));
    WriteFrozen(W, Dir);
    WriteParts(W, Dir);
    WriteLights(W, Dir);
    WriteWater(W, Pawn, Dir);
    UE_LOG(LogTemp, Display, TEXT("CALDER_VERIFY done terrain=%d hash=%08x ms=%.0f dir=%s"), int32(HouseCalderTerrain::Enabled()), HouseCalderTerrain::Hash(), (FPlatformTime::Seconds() - Began) * 1000.0, *Dir);
    if (!VerifySharedRun()) FPlatformMisc::RequestExitWithStatus(false, 0);
}

struct FVerifyRegistration { FVerifyRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&VerifyTick); } };
FVerifyRegistration VerifyRegistrationInstance;
}
#endif
