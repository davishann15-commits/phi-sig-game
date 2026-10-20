// The Calder terrain's drawn ground and its collision (see HouseCalderTerrainTiles.h).
#include "HouseCalderTerrainTiles.h"
#include "HouseCalderLandform.h"
#include "HouseCalderLandscape.h"
#include "HouseCampusLayout.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"

namespace HouseCalderTerrainTilesImpl
{
constexpr int32 PerSide = int32(2 * HouseCalderTerrain::Half / HouseCalderTerrainTiles::TileSize);   // 5
constexpr double WalkableNz = .766;   // cos 40 deg

struct FTileMesh
{
    TArray<FVector> V;
    TArray<FVector> N;
    TArray<FLinearColor> C;
    TArray<int32> I;
    TMap<int64, int32> Index;
};

using FPoly = TArray<FVector2D, TInlineAllocator<8>>;

/** Keeps the part of a convex polygon where S * ((X - X0) - (Y - Y0)) >= 0: one side of a lattice cell's diagonal. */
void ClipDiagonal(const FPoly& In, FPoly& Out, double X0, double Y0, double S)
{
    Out.Reset();
    const int32 Num = In.Num();
    for (int32 K = 0; K < Num; ++K)
    {
        const FVector2D& A = In[K];
        const FVector2D& B = In[(K + 1) % Num];
        const double FA = S * ((A.X - X0) - (A.Y - Y0)), FB = S * ((B.X - X0) - (B.Y - Y0));
        if (FA >= 0.0) Out.Add(A);
        if ((FA >= 0.0) != (FB >= 0.0))
        {
            const double T = FA / (FA - FB);
            Out.Add(A + (B - A) * T);
        }
    }
}

// Material state belongs to one runtime exterior build; the comparison path
// retains neutral vertex colours and the original material byte for byte.
bool bLandscapeMaterial = false;
int32 PaintedVertices = 0;
FLinearColor MaskSum(0.f, 0.f, 0.f, 0.f);
TMap<FIntPoint, float> HardscapeMasks;

float Smooth(float A, float B, float V)
{
    const float T = FMath::Clamp((V - A) / (B - A), 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}

/** Build-time masks only: R rock, G cove litter, B exposed clay, A cavity.
 *  MeshDescription's fast builder encodes RGB as sRGB; the owned shader decodes
 *  it before blending. Alpha remains linear. Collision is never changed here. */
FLinearColor Paint(float X, float Y, const FVector& Normal)
{
    if (!bLandscapeMaterial || HouseCalderTerrain::InFrozen(X, Y)) return FLinearColor(0.f, 0.f, 0.f, 1.f);
    const float Slope = 1.f - float(Normal.Z);
    const float Rock = Smooth(.180848f, .292893f, Slope); // 35..45 degrees
    const float Wild = FMath::Max(Smooth(19000.f, 23000.f, X), Smooth(19000.f, 24000.f, Y));
    const float NorthCove = Smooth(.05f, .55f, float(Normal.Y));
    const float Patch = .65f + .35f * FMath::Sin(X * .00071f + FMath::Sin(Y * .00039f) * 1.7f);
    const float Litter = Wild * (.18f + .65f * NorthCove) * Patch * (1.f - Rock);
    float Clay = 0.f;
    if (Slope > .035f && Rock < .99f)
    {
        const FIntPoint Bucket(FMath::FloorToInt(X / 400.f), FMath::FloorToInt(Y / 400.f));
        float* Found = HardscapeMasks.Find(Bucket);
        if (!Found)
        {
            const FVector2D Centre((Bucket.X + .5) * 400.0, (Bucket.Y + .5) * 400.0);
            float Near = 0.f;
            for (const float Radius : { 200.f, 400.f, 600.f })
                if (HouseCampusLayout::PavingNear(Centre, Radius)) { Near = (800.f - Radius) / 600.f; break; }
            Found = &HardscapeMasks.Add(Bucket, Near);
        }
        Clay = *Found * Smooth(.035f, .13f, Slope) * (1.f - Rock) * .72f;
    }
    const float Z = HouseExteriorSite::GroundHeight(X, Y);
    const float Around = (HouseExteriorSite::GroundHeight(X - 200.f, Y) + HouseExteriorSite::GroundHeight(X + 200.f, Y)
        + HouseExteriorSite::GroundHeight(X, Y - 200.f) + HouseExteriorSite::GroundHeight(X, Y + 200.f)) * .25f;
    const float Cavity = 1.f - .30f * Smooth(5.f, 65.f, Around - Z);
    const FLinearColor Result(Rock, Litter, Clay, Cavity);
    ++PaintedVertices;
    MaskSum += Result;
    return Result;
}

/** A plain cell's vertex at (X, Y). Its height is read half a millimetre in toward the polygon's middle (Inward), so a vertex
 *  on the edge of a wall-split cell takes this plain cell's own lattice value, never a split piece's height across that edge
 *  (a region corner at (-6600,-21950) on the ENG court's taper read the piece's 1719 for the plain cell's 1705: a 14 cm crack in
 *  the collision, 2026-10-15). Plain cells sharing the vertex agree to the lattice's slope times 0.5 mm. */
int32 Vertex(FTileMesh& T, double X, double Y, const FVector2D& Inward)
{
    const int64 Key = (int64(FMath::RoundToInt(X * 4.0)) << 32) ^ int64(uint32(FMath::RoundToInt(Y * 4.0)));
    if (const int32* Found = T.Index.Find(Key)) return *Found;
    const float FX = float(X), FY = float(Y);
    const FVector2D In = (Inward - FVector2D(X, Y)).GetSafeNormal() * .05;
    const FVector Normal = HouseExteriorSite::GroundNormal(FX, FY);
    const int32 New = T.V.Add(FVector(FX, FY, HouseExteriorSite::GroundHeight(float(X + In.X), float(Y + In.Y))));
    T.N.Add(Normal);
    T.C.Add(Paint(FX, FY, Normal));
    T.Index.Add(Key, New);
    return New;
}

/** A convex piece of one lattice triangle, fanned into its tile, every triangle facing up. */
void Emit(TArray<FTileMesh>& Tiles, const FPoly& P)
{
    if (P.Num() < 3) return;
    double Area = 0.0;
    FVector2D Mid(0.0, 0.0);
    for (int32 K = 0; K < P.Num(); ++K) { Area += P[K].X * P[(K + 1) % P.Num()].Y - P[(K + 1) % P.Num()].X * P[K].Y; Mid += P[K]; }
    if (FMath::Abs(Area) < 2.0) return;   // under 1 cm^2
    Mid /= P.Num();
    const int32 TX = FMath::Clamp(FMath::FloorToInt((Mid.X + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    const int32 TY = FMath::Clamp(FMath::FloorToInt((Mid.Y + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    FTileMesh& T = Tiles[TY * PerSide + TX];
    TArray<int32, TInlineAllocator<8>> Ids;
    for (const FVector2D& Q : P) Ids.Add(Vertex(T, Q.X, Q.Y, Mid));
    for (int32 K = 1; K + 1 < Ids.Num(); ++K)
    {
        const int32 A = Ids[0], B = Ids[K], C = Ids[K + 1];
        if (A == B || B == C || A == C) continue;
        // FHallMesh's convention: (A, B, C) faces the side where Cross(B - A, C - A) points away (front when the dot with the
        // face normal is negative), so an up-facing triangle has a downward cross product.
        if (FVector::CrossProduct(T.V[B] - T.V[A], T.V[C] - T.V[A]).Z < 0.0) T.I.Append({ A, B, C });
        else T.I.Append({ A, C, B });
    }
}

/** A wall-split cell's triangle (relief vertices A, B, C), clipped to the part of the cell inside the region, drawn with its
 *  own vertices (not shared with the other side of the wall, which stands at another height) and its own face normal. */
void EmitPieceTriangle(TArray<FTileMesh>& Tiles, const FVector& A, const FVector& B, const FVector& C, double SX0, double SY0, double SX1, double SY1)
{
    FPoly P = { FVector2D(A), FVector2D(B), FVector2D(C) }, Q;
    // Keep the part inside the region's share of the cell: X >= SX0, X <= SX1, Y >= SY0, Y <= SY1.
    auto Clip = [](const FPoly& In, FPoly& Out, double NX, double NY, double D)
    {
        Out.Reset();
        for (int32 K = 0; K < In.Num(); ++K)
        {
            const FVector2D& U = In[K];
            const FVector2D& V = In[(K + 1) % In.Num()];
            const double FU = NX * U.X + NY * U.Y + D, FV = NX * V.X + NY * V.Y + D;
            if (FU >= 0.0) Out.Add(U);
            if ((FU >= 0.0) != (FV >= 0.0)) Out.Add(U + (V - U) * (FU / (FU - FV)));
        }
    };
    Clip(P, Q, 1, 0, -SX0); Clip(Q, P, -1, 0, SX1); Clip(P, Q, 0, 1, -SY0); Clip(Q, P, 0, -1, SY1);
    if (P.Num() < 3) return;
    const FVector2D A2(A), B2(B), C2(C);
    const double Den = (B2.Y - C2.Y) * (A2.X - C2.X) + (C2.X - B2.X) * (A2.Y - C2.Y);
    if (FMath::Abs(Den) < 1e-9) return;
    const float Scale = HouseCalderTerrain::ReliefScale();
    auto ZAt = [&](const FVector2D& Q2)
    {
        const double WA = ((B2.Y - C2.Y) * (Q2.X - C2.X) + (C2.X - B2.X) * (Q2.Y - C2.Y)) / Den;
        const double WB = ((C2.Y - A2.Y) * (Q2.X - C2.X) + (A2.X - C2.X) * (Q2.Y - C2.Y)) / Den;
        const double R = WA * A.Z + WB * B.Z + (1.0 - WA - WB) * C.Z;
        return HouseExteriorSite::BaseGrade(float(Q2.X), float(Q2.Y)) + float(R) * Scale;
    };
    FVector2D Mid(0.0, 0.0);
    for (const FVector2D& V : P) Mid += V;
    Mid /= P.Num();
    const int32 TX = FMath::Clamp(FMath::FloorToInt((Mid.X + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    const int32 TY = FMath::Clamp(FMath::FloorToInt((Mid.Y + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    FTileMesh& T = Tiles[TY * PerSide + TX];
    TArray<FVector, TInlineAllocator<8>> W;
    for (const FVector2D& V : P) W.Add(FVector(V.X, V.Y, ZAt(V)));
    FVector N = FVector::CrossProduct(W[1] - W[0], W[2] - W[0]).GetSafeNormal();
    if (N.Z < 0.0) N = -N;
    const int32 Base = T.V.Num();
    for (const FVector& V : W) { T.V.Add(V); T.N.Add(N); T.C.Add(Paint(float(V.X), float(V.Y), N)); }
    for (int32 K = 1; K + 1 < W.Num(); ++K)
    {
        const int32 IA = Base, IB = Base + K, IC = Base + K + 1;
        if (FVector::CrossProduct(T.V[IB] - T.V[IA], T.V[IC] - T.V[IA]).Z < 0.0) T.I.Append({ IA, IB, IC });
        else T.I.Append({ IA, IC, IB });
    }
}

/** A vertical skirt 40 cm down under a wall-split piece's inner edge A-B (relief vertices), inside the region's share of the
 *  cell, facing both ways (two quads), so a slit between two pieces at slightly different heights never shows. The edge is
 *  clipped to the region (each region draws its own part), so a skirt never stands in a cut or a footprint next to it (a
 *  piece edge across a curb ramp's cut stood over the ramp, 2026-10-15). */
void EmitPieceSkirt(TArray<FTileMesh>& Tiles, const FVector& EdgeA, const FVector& EdgeB, double SX0, double SY0, double SX1, double SY1)
{
    // Liang-Barsky: the part of the edge inside the region's box.
    double T0 = 0.0, T1 = 1.0;
    const double DX = EdgeB.X - EdgeA.X, DY = EdgeB.Y - EdgeA.Y;
    const double P4[4] = { -DX, DX, -DY, DY };
    const double Q4[4] = { EdgeA.X - (SX0 - .01), (SX1 + .01) - EdgeA.X, EdgeA.Y - (SY0 - .01), (SY1 + .01) - EdgeA.Y };
    for (int32 K = 0; K < 4; ++K)
    {
        if (FMath::Abs(P4[K]) < 1e-12)
        {
            if (Q4[K] < 0.0) return;
            continue;
        }
        const double T = Q4[K] / P4[K];
        if (P4[K] < 0.0) T0 = FMath::Max(T0, T);
        else T1 = FMath::Min(T1, T);
        if (T0 >= T1) return;
    }
    const FVector A = EdgeA + (EdgeB - EdgeA) * T0, B = EdgeA + (EdgeB - EdgeA) * T1;
    const FVector2D M((A.X + B.X) * .5, (A.Y + B.Y) * .5);
    if (FVector2D::Distance(FVector2D(A), FVector2D(B)) < 1.0) return;
    const float Scale = HouseCalderTerrain::ReliefScale();
    auto Top = [Scale](const FVector& V) { return FVector(V.X, V.Y, HouseExteriorSite::BaseGrade(float(V.X), float(V.Y)) + float(V.Z) * Scale); };
    const FVector TA = Top(A), TB = Top(B), BA = TA - FVector(0, 0, 40.0), BB = TB - FVector(0, 0, 40.0);
    const int32 TX = FMath::Clamp(FMath::FloorToInt((M.X + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    const int32 TY = FMath::Clamp(FMath::FloorToInt((M.Y + HouseCalderTerrain::Half) / HouseCalderTerrainTiles::TileSize), 0, PerSide - 1);
    FTileMesh& T = Tiles[TY * PerSide + TX];
    const FVector Side = FVector(-(B.Y - A.Y), B.X - A.X, 0.0).GetSafeNormal();
    for (const float Sgn : { 1.f, -1.f })
    {
        const FVector N = Side * Sgn;
        const int32 Base = T.V.Num();
        for (const FVector& V : { TA, TB, BB, BA }) { T.V.Add(V); T.N.Add(N); T.C.Add(FLinearColor(0.f, 0.f, 1.f, .6f)); }
        // Face N: (A, B, C) faces the side its cross product points away from.
        const FVector Cross = FVector::CrossProduct(T.V[Base + 1] - T.V[Base], T.V[Base + 2] - T.V[Base]);
        if (FVector::DotProduct(Cross, N) < 0.0) T.I.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
        else T.I.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
    }
}

/** One terrain region (an axis-aligned rectangle): cut along the lattice lines and every cell's diagonal. */
void AddRegion(TArray<FTileMesh>& Tiles, const FBox2D& R)
{
    const double H = HouseCalderTerrain::Half, C = HouseCalderTerrain::Cell;
    const int32 I0 = FMath::Clamp(FMath::FloorToInt((R.Min.X + H) / C), 0, HouseCalderTerrain::Cells - 1);
    const int32 I1 = FMath::Clamp(FMath::CeilToInt((R.Max.X + H) / C) - 1, 0, HouseCalderTerrain::Cells - 1);
    const int32 J0 = FMath::Clamp(FMath::FloorToInt((R.Min.Y + H) / C), 0, HouseCalderTerrain::Cells - 1);
    const int32 J1 = FMath::Clamp(FMath::CeilToInt((R.Max.Y + H) / C) - 1, 0, HouseCalderTerrain::Cells - 1);
    FPoly Lo, Hi;
    for (int32 J = J0; J <= J1; ++J)
        for (int32 I = I0; I <= I1; ++I)
        {
            const double CX0 = -H + I * C, CY0 = -H + J * C;
            const double SX0 = FMath::Max(CX0, double(R.Min.X)), SX1 = FMath::Min(CX0 + C, double(R.Max.X));
            const double SY0 = FMath::Max(CY0, double(R.Min.Y)), SY1 = FMath::Min(CY0 + C, double(R.Max.Y));
            if (SX1 - SX0 < .01 || SY1 - SY0 < .01) continue;
            if (HouseCalderTerrain::Enabled())
                if (const TArray<TArray<FVector>>* Pieces = HouseCalderLandform::WallPieces(I, J))
                {
                    // A declared wall crosses this cell: each side is drawn at its own height; the wall itself is built by
                    // HouseCalderFeatures and closes the step between them.
                    for (const TArray<FVector>& Poly : *Pieces)
                    {
                        for (int32 K = 1; K + 1 < Poly.Num(); ++K) EmitPieceTriangle(Tiles, Poly[0], Poly[K], Poly[K + 1], SX0, SY0, SX1, SY1);
                        // Skirts under every piece edge inside the cell: where two pieces meet off a declared wall (past a
                        // wall's end, along a split line) their sides can differ by a few cm; a skirt 40 cm down closes the
                        // slit for the eye and the capsule alike (a declared wall's own mesh covers the real steps).
                        for (int32 K = 0; K < Poly.Num(); ++K)
                        {
                            const FVector& A = Poly[K];
                            const FVector& B = Poly[(K + 1) % Poly.Num()];
                            const bool bOnCellEdge = (FMath::IsNearlyEqual(A.X, B.X, .01) && (FMath::IsNearlyEqual(A.X, CX0, .01) || FMath::IsNearlyEqual(A.X, CX0 + C, .01)))
                                || (FMath::IsNearlyEqual(A.Y, B.Y, .01) && (FMath::IsNearlyEqual(A.Y, CY0, .01) || FMath::IsNearlyEqual(A.Y, CY0 + C, .01)));
                            if (!bOnCellEdge) EmitPieceSkirt(Tiles, A, B, SX0, SY0, SX1, SY1);
                        }
                    }
                    continue;
                }
            const FPoly Sub = { FVector2D(SX0, SY0), FVector2D(SX1, SY0), FVector2D(SX1, SY1), FVector2D(SX0, SY1) };
            ClipDiagonal(Sub, Lo, CX0, CY0, 1.0);    // the (I,J), (I+1,J), (I+1,J+1) triangle
            ClipDiagonal(Sub, Hi, CX0, CY0, -1.0);   // the (I,J), (I+1,J+1), (I,J+1) triangle
            Emit(Tiles, Lo);
            Emit(Tiles, Hi);
        }
}

UStaticMesh* BuildMesh(const FTileMesh& T, UMaterialInterface* Material, const FString& Name)
{
    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    auto Positions = Attributes.GetVertexPositions();
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    auto Signs = Attributes.GetVertexInstanceBinormalSigns();
    auto Colors = Attributes.GetVertexInstanceColors();
    auto UVs = Attributes.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
    const FPolygonGroupID Group = Description.CreatePolygonGroup();
    Slots[Group] = TEXT("Ground");
    TArray<FVertexInstanceID> Instances;
    Instances.Reserve(T.V.Num());
    for (int32 K = 0; K < T.V.Num(); ++K)
    {
        const FVertexID Vertex = Description.CreateVertex();
        Positions[Vertex] = FVector3f(T.V[K]);
        const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
        const FVector Tangent = (FVector(1, 0, 0) - T.N[K] * T.N[K].X).GetSafeNormal();
        Normals[Instance] = FVector3f(T.N[K]);
        Tangents[Instance] = FVector3f(Tangent);
        Signs[Instance] = 1.f;
        Colors[Instance] = FVector4f(T.C[K].R, T.C[K].G, T.C[K].B, T.C[K].A);
        UVs.Set(Instance, 0, FVector2f(float(T.V[K].X / 100.0), float(T.V[K].Y / 100.0)));
        Instances.Add(Instance);
    }
    for (int32 K = 0; K + 2 < T.I.Num(); K += 3)
        Description.CreatePolygon(Group, TArray<FVertexInstanceID, TInlineAllocator<3>>{ Instances[T.I[K]], Instances[T.I[K + 1]], Instances[T.I[K + 2]] });
    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UStaticMesh::StaticClass(), FName(*Name)), RF_Transient);
    Mesh->NeverStream = true;
    Mesh->bAllowCPUAccess = false;
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Ground")
#if WITH_EDITORONLY_DATA
        , TEXT("Ground")
#endif
    ));
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bAllowCpuAccess = false;
    Params.bMarkPackageDirty = false;
    Params.bCommitMeshDescription = false;
    return Mesh->BuildFromMeshDescriptions({ &Description }, Params) ? Mesh : nullptr;
}

UMaterialInterface* GroundMaterial(UObject* Outer)
{
    bLandscapeMaterial = false;
    UMaterialInterface* Base = nullptr;
    if (HouseCalderLandscape::Enabled())
    {
        Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderLandscapeGround.M_CalderLandscapeGround"), nullptr, LOAD_NoWarn | LOAD_Quiet);
        bLandscapeMaterial = Base != nullptr;
        if (!Base) UE_LOG(LogTemp, Warning, TEXT("CALDER_GROUND owned appearance material missing; retaining original ground and lawn overlays"));
    }
    if (!Base) Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderGround.M_CalderGround"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Base)
    {
        UE_LOG(LogTemp, Warning, TEXT("CALDER_TERRAIN M_CalderGround is missing; the tiles fall back to the default material"));
        return nullptr;
    }
    UE_LOG(LogTemp, Display, TEXT("CALDER_GROUND material=%s landscape=%d originalMasterUnchanged=1"), *Base->GetName(), int32(bLandscapeMaterial));
    return UMaterialInstanceDynamic::Create(Base, Outer, TEXT("CT_Ground"));
}

/** Watches the tiles' asynchronous collision cooks and turns CollisionReady on when every body exists. */
void WatchCollision(UWorld* World, const TArray<TWeakObjectPtr<UProceduralMeshComponent>>& Parts)
{
    int32 Valid = 0;
    for (const TWeakObjectPtr<UProceduralMeshComponent>& P : Parts)
    {
        const FBodyInstance* Body = P.IsValid() ? P->GetBodyInstance() : nullptr;
        Valid += Body && Body->IsValidBodyInstance() ? 1 : 0;
    }
    HouseCalderTerrain::SetCollisionReady(World, Valid == Parts.Num());
    if (Valid == Parts.Num())
    {
        UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN_COLLISION ready=%d/%d at build"), Valid, Parts.Num());
        return;
    }
    const double Start = FPlatformTime::Seconds();
    TWeakObjectPtr<UWorld> WeakWorld(World);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Parts, Start](float) -> bool
    {
        UWorld* W = WeakWorld.Get();
        if (!W) return false;
        int32 Ready = 0;
        for (const TWeakObjectPtr<UProceduralMeshComponent>& P : Parts)
        {
            const FBodyInstance* Body = P.IsValid() ? P->GetBodyInstance() : nullptr;
            Ready += Body && Body->IsValidBodyInstance() ? 1 : 0;
        }
        if (Ready < Parts.Num()) return true;
        HouseCalderTerrain::SetCollisionReady(W, true);
        UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN_COLLISION ready=%d/%d ms=%.0f"), Ready, Parts.Num(), (FPlatformTime::Seconds() - Start) * 1000.0);
        return false;
    }));
}
}

bool HouseCalderTerrainTiles::UsesLandscapeGround()
{
    return HouseCalderTerrainTilesImpl::bLandscapeMaterial;
}

int32 HouseCalderTerrainTiles::Build(AActor* Owner, const TArray<FBox2D>& Regions)
{
    if (!Owner || !Owner->GetWorld()) return 0;
    const double Start = FPlatformTime::Seconds();
    HouseCalderTerrainTilesImpl::bLandscapeMaterial = false;
    HouseCalderTerrainTilesImpl::PaintedVertices = 0;
    HouseCalderTerrainTilesImpl::MaskSum = FLinearColor(0.f, 0.f, 0.f, 0.f);
    HouseCalderTerrainTilesImpl::HardscapeMasks.Reset();
    const bool bDraw = Owner->GetWorld()->GetNetMode() != NM_DedicatedServer;
    UMaterialInterface* Material = bDraw ? HouseCalderTerrainTilesImpl::GroundMaterial(Owner) : nullptr;
    TArray<HouseCalderTerrainTilesImpl::FTileMesh> Tiles;
    Tiles.SetNum(HouseCalderTerrainTilesImpl::PerSide * HouseCalderTerrainTilesImpl::PerSide);
    for (const FBox2D& R : Regions) HouseCalderTerrainTilesImpl::AddRegion(Tiles, R);
    TArray<TWeakObjectPtr<UProceduralMeshComponent>> Parts;
    int32 Drawn = 0, Built = 0, SteepTris = 0;
    double CollisionSeconds = 0.0;
    for (int32 K = 0; K < Tiles.Num(); ++K)
    {
        const HouseCalderTerrainTilesImpl::FTileMesh& T = Tiles[K];
        if (T.I.Num() < 3) continue;
        ++Built;
        // Collision: a hidden procedural copy, complex as simple. Cooked here, at load, like today's ExteriorGrounds: the
        // asynchronous cook took 23-24 s to finish behind the rest of the load, and synchronous costs a fraction of that.
        UProceduralMeshComponent* Col = NewObject<UProceduralMeshComponent>(Owner, *FString::Printf(TEXT("CalderTerrainCollision_%02d"), K));
        Col->SetMobility(EComponentMobility::Static);
        Col->bUseComplexAsSimpleCollision = true;
        Col->bUseAsyncCooking = false;
        Col->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Col->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Col->SetCastShadow(false);
        Col->SetVisibility(false);
        Col->SetHiddenInGame(true);
        Col->RegisterComponent();
        const double CookStart = FPlatformTime::Seconds();
        // The rare facet steeper than 40 deg that faces up (a sliver where a wall's step tapers out at its free end) goes into a
        // second hidden part with the Unwalkable override (MASTER_PROMPT: anything over 40 deg is Unwalkable); the rest, skirts
        // included, is the tile's own collision.
        TArray<int32> WalkI, SteepI;
        WalkI.Reserve(T.I.Num());
        for (int32 Tri = 0; Tri + 2 < T.I.Num(); Tri += 3)
        {
            const FVector& A = T.V[T.I[Tri]];
            const FVector N = FVector::CrossProduct(T.V[T.I[Tri + 1]] - A, T.V[T.I[Tri + 2]] - A).GetSafeNormal();
            const double Up = FMath::Abs(N.Z);
            TArray<int32>& Into = Up > .05 && Up < HouseCalderTerrainTilesImpl::WalkableNz ? SteepI : WalkI;
            Into.Append({ T.I[Tri], T.I[Tri + 1], T.I[Tri + 2] });
        }
        Col->CreateMeshSection_LinearColor(0, T.V, WalkI, T.N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
        Parts.Add(Col);
        if (SteepI.Num())
        {
            UProceduralMeshComponent* Steep = NewObject<UProceduralMeshComponent>(Owner, *FString::Printf(TEXT("CalderTerrainSteep_%02d"), K));
            Steep->SetMobility(EComponentMobility::Static);
            Steep->bUseComplexAsSimpleCollision = true;
            Steep->bUseAsyncCooking = false;
            Steep->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
            Steep->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
            Steep->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.f));
            Steep->SetCastShadow(false);
            Steep->SetVisibility(false);
            Steep->SetHiddenInGame(true);
            Steep->RegisterComponent();
            Steep->CreateMeshSection_LinearColor(0, T.V, SteepI, T.N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), true);
            Parts.Add(Steep);
            SteepTris += SteepI.Num() / 3;
        }
        CollisionSeconds += FPlatformTime::Seconds() - CookStart;
        if (!bDraw) continue;
        UStaticMesh* Mesh = HouseCalderTerrainTilesImpl::BuildMesh(T, Material, FString::Printf(TEXT("CalderTerrainTile_%02d"), K));
        if (!Mesh) { UE_LOG(LogTemp, Warning, TEXT("CALDER_TERRAIN tile %d failed to build"), K); continue; }
        UStaticMeshComponent* Draw = NewObject<UStaticMeshComponent>(Owner, *FString::Printf(TEXT("CalderTerrainTile_%02d"), K));
        Draw->SetMobility(EComponentMobility::Static);
        Draw->SetStaticMesh(Mesh);
        Draw->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Draw->SetCastShadow(false);
        Draw->bAffectDistanceFieldLighting = false;
        Draw->RegisterComponent();
        Drawn += T.I.Num() / 3;
    }
    const double CookMs = CollisionSeconds * 1000.0;
    HouseCalderTerrainTilesImpl::WatchCollision(Owner->GetWorld(), Parts);
    UE_LOG(LogTemp, Display, TEXT("CALDER_TERRAIN tiles=%d regions=%d tris=%d draw=%d collisionCookMs=%.0f ms=%.0f steepUnwalkableTris=%d"), Built, Regions.Num(), Drawn, int32(bDraw), CookMs, (FPlatformTime::Seconds() - Start) * 1000.0, SteepTris);
    const int32 Painted = HouseCalderTerrainTilesImpl::PaintedVertices;
    const FLinearColor Mean = Painted ? HouseCalderTerrainTilesImpl::MaskSum / float(Painted) : FLinearColor(0.f, 0.f, 0.f, 1.f);
    UE_LOG(LogTemp, Display, TEXT("CALDER_GROUND_PAINT vertices=%d meanRock=%.4f meanLitter=%.4f meanClay=%.4f meanCavity=%.4f collisionUnchanged=1"), Painted, Mean.R, Mean.G, Mean.B, Mean.A);
    HouseCalderTerrainTilesImpl::HardscapeMasks.Empty();
    return Drawn;
}
