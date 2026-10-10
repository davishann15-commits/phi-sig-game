#include "HouseCoplanarFix.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif

int32 UHouseCoplanarFixLibrary::LiftCoplanarOverlaps(UStaticMesh* Mesh, float Offset, bool bApply, FString& Report)
{
#if WITH_EDITOR
    if (!Mesh) return -1;
    FMeshDescription* MD = Mesh->GetMeshDescription(0);
    if (!MD) { Report = TEXT("no mesh description"); return -1; }
    FStaticMeshAttributes Attr(*MD);
    TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();

    struct FTri { FVertexID V[3]; FVector3d P[3]; FVector3d N; double Area = 0, D = 0; int32 Group = 0; int32 Ax = 2; FVector2d Lo, Hi; FVector2d Q[3]; };
    TArray<FTri> Tris;
    Tris.Reserve(MD->Triangles().Num());
    for (const FTriangleID T : MD->Triangles().GetElementIDs())
    {
        TArrayView<const FVertexID> Vs = MD->GetTriangleVertices(T);
        if (Vs.Num() != 3) continue;
        FTri X;
        for (int32 K = 0; K < 3; ++K) { X.V[K] = Vs[K]; X.P[K] = FVector3d(Pos[Vs[K]]); }
        const FVector3d C = FVector3d::CrossProduct(X.P[1] - X.P[0], X.P[2] - X.P[0]);
        const double L = C.Size();
        if (L < 1e-3) continue;
        X.N = C / L; X.Area = 0.5 * L; X.D = FVector3d::DotProduct(X.N, X.P[0]);
        X.Group = MD->GetTrianglePolygonGroup(T).GetValue();
        const FVector3d A(FMath::Abs(X.N.X), FMath::Abs(X.N.Y), FMath::Abs(X.N.Z));
        X.Ax = (A.X >= A.Y && A.X >= A.Z) ? 0 : (A.Y >= A.Z ? 1 : 2);
        const int32 U = X.Ax == 0 ? 1 : 0, W = X.Ax == 2 ? 1 : 2;
        X.Lo = FVector2d(1e30, 1e30); X.Hi = FVector2d(-1e30, -1e30);
        for (int32 K = 0; K < 3; ++K)
        {
            X.Q[K] = FVector2d(X.P[K][U], X.P[K][W]);
            X.Lo = FVector2d(FMath::Min(X.Lo.X, X.Q[K].X), FMath::Min(X.Lo.Y, X.Q[K].Y));
            X.Hi = FVector2d(FMath::Max(X.Hi.X, X.Q[K].X), FMath::Max(X.Hi.Y, X.Q[K].Y));
        }
        Tris.Add(X);
    }

    // Bucket by rounded normal, then sweep each bucket sorted by plane offset.
    TMap<FIntVector, TArray<int32>> Buckets;
    for (int32 I = 0; I < Tris.Num(); ++I)
        Buckets.FindOrAdd(FIntVector(FMath::RoundToInt(Tris[I].N.X * 20), FMath::RoundToInt(Tris[I].N.Y * 20), FMath::RoundToInt(Tris[I].N.Z * 20))).Add(I);

    auto Side = [](const FVector2d& P, const FVector2d& A, const FVector2d& B) { return (P.X - B.X) * (A.Y - B.Y) - (A.X - B.X) * (P.Y - B.Y); };
    auto Inside = [&Side](const FVector2d& P, const FTri& T)
    {
        const double S1 = Side(P, T.Q[0], T.Q[1]), S2 = Side(P, T.Q[1], T.Q[2]), S3 = Side(P, T.Q[2], T.Q[0]);
        const double Ar = FMath::Abs(Side(T.Q[0], T.Q[1], T.Q[2]));
        const double E = 1e-3 * Ar;
        return (S1 > E && S2 > E && S3 > E) || (S1 < -E && S2 < -E && S3 < -E);
    };
    auto Overlaps = [&Inside](const FTri& A, const FTri& B)
    {
        const FVector2d CA = (A.Q[0] + A.Q[1] + A.Q[2]) / 3.0, CB = (B.Q[0] + B.Q[1] + B.Q[2]) / 3.0;
        if (Inside(CA, B) || Inside(CB, A)) return true;
        for (int32 K = 0; K < 3; ++K)
        {
            if (Inside((A.Q[K] * 2.0 + A.Q[(K + 1) % 3] + A.Q[(K + 2) % 3]) / 4.0, B)) return true;
            if (Inside((B.Q[K] * 2.0 + B.Q[(K + 1) % 3] + B.Q[(K + 2) % 3]) / 4.0, A)) return true;
        }
        return false;
    };

    TArray<TPair<int32, int32>> PairList; int32 Pairs = 0;
    TArray<FString> Samples;
    for (auto& Bk : Buckets)
    {
        TArray<int32>& Ids = Bk.Value;
        if (Ids.Num() < 2) continue;
        Ids.Sort([&Tris](int32 A, int32 B) { return Tris[A].D < Tris[B].D; });
        for (int32 I = 0; I < Ids.Num(); ++I)
        {
            const FTri& A = Tris[Ids[I]];
            for (int32 J = I + 1; J < Ids.Num() && Tris[Ids[J]].D - A.D < 0.05; ++J)
            {
                const FTri& B = Tris[Ids[J]];
                if (A.Ax != B.Ax || FVector3d::DotProduct(A.N, B.N) < 0.9995) continue;
                const double M = 0.05;
                if (A.Lo.X > B.Hi.X - M || B.Lo.X > A.Hi.X - M || A.Lo.Y > B.Hi.Y - M || B.Lo.Y > A.Hi.Y - M) continue;
                if (!Overlaps(A, B)) continue;
                ++Pairs;
                const int32 Small = A.Area < B.Area ? Ids[I] : Ids[J];
                const int32 Big = Small == Ids[I] ? Ids[J] : Ids[I];
                PairList.Add(TPair<int32, int32>(Small, Big));
                if (Samples.Num() < 12)
                {
                    const FVector3d C = (Tris[Small].P[0] + Tris[Small].P[1] + Tris[Small].P[2]) / 3.0;
                    Samples.Add(FString::Printf(TEXT("(%.0f,%.0f,%.1f g%d/g%d a%.0f/%.0f)"), C.X, C.Y, C.Z, Tris[Small].Group, Tris[Big].Group, Tris[Small].Area, Tris[Big].Area));
                }
            }
        }
    }

    // Stacked layers (a nosing strip on a tread plate on the slab) each get one more step.
    TMap<int32, int32> Level;
    for (int32 Pass = 0; Pass < 6; ++Pass)
    {
        bool bChanged = false;
        for (const auto& Pr : PairList)
        {
            const int32 Want = Level.FindRef(Pr.Value) + 1;
            int32& L = Level.FindOrAdd(Pr.Key);
            if (L < Want) { L = FMath::Min(Want, 4); bChanged = true; }
        }
        if (!bChanged) break;
    }
    // Each vertex of a lifted triangle moves along each distinct normal by the highest level using it.
    TMap<FVertexID, TArray<FVector4d>> Moves;
    for (const auto& Lv : Level)
    {
        const FTri& T = Tris[Lv.Key];
        for (int32 K = 0; K < 3; ++K)
        {
            TArray<FVector4d>& Ns = Moves.FindOrAdd(T.V[K]);
            bool bHave = false;
            for (FVector4d& N : Ns)
                if (FVector3d::DotProduct(FVector3d(N.X, N.Y, N.Z), T.N) > 0.99) { N.W = FMath::Max(N.W, (double)Lv.Value); bHave = true; break; }
            if (!bHave) Ns.Add(FVector4d(T.N.X, T.N.Y, T.N.Z, (double)Lv.Value));
        }
    }
    const int32 NumLift = Level.Num();
    Report = FString::Printf(TEXT("tris=%d pairs=%d lift=%d verts=%d %s"), Tris.Num(), Pairs, NumLift, Moves.Num(), *FString::Join(Samples, TEXT(" ")));
    if (bApply && Moves.Num())
    {
        Mesh->Modify();
        for (const auto& Mv : Moves)
        {
            FVector3d Sum = FVector3d::ZeroVector;
            for (const FVector4d& N : Mv.Value) Sum += FVector3d(N.X, N.Y, N.Z) * N.W;
            Pos[Mv.Key] += FVector3f(Sum * Offset);
        }
        Mesh->CommitMeshDescription(0);
        Mesh->Build(true);
        Mesh->PostEditChange();
        Mesh->MarkPackageDirty();
    }
    return NumLift;
#else
    Report = TEXT("editor only");
    return -1;
#endif
}

int32 UHouseCoplanarFixLibrary::CutTrianglesInBox(UStaticMesh* Mesh, FVector Min, FVector Max, FVector Scale, bool bAnyVertex, bool bApply, FString& Report)
{
#if WITH_EDITOR
    if (!Mesh) return -1;
    FMeshDescription* MD = Mesh->GetMeshDescription(0);
    if (!MD) { Report = TEXT("no mesh description"); return -1; }
    FStaticMeshAttributes Attr(*MD);
    TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
    const FBox Box(Min, Max);
    TArray<FTriangleID> Doomed;
    int32 Total = 0;
    for (const FTriangleID T : MD->Triangles().GetElementIDs())
    {
        ++Total;
        TArrayView<const FVertexID> Vs = MD->GetTriangleVertices(T);
        if (Vs.Num() != 3) continue;
        FVector C = FVector::ZeroVector; bool bAny = false;
        for (int32 K = 0; K < 3; ++K)
        {
            const FVector P = FVector(Pos[Vs[K]]) * Scale;
            C += P / 3.f;
            if (Box.IsInside(P)) bAny = true;
        }
        if (bAnyVertex ? bAny : Box.IsInside(C)) Doomed.Add(T);
    }
    Report = FString::Printf(TEXT("tris=%d cut=%d"), Total, Doomed.Num());
    if (bApply && Doomed.Num())
    {
        Mesh->Modify();
        TArray<FEdgeID> Edges; TArray<FVertexInstanceID> Instances; TArray<FPolygonGroupID> Groups;
        for (const FTriangleID T : Doomed) MD->DeleteTriangle(T, &Edges, &Instances, &Groups);
        for (const FEdgeID E : Edges) if (MD->IsEdgeValid(E)) MD->DeleteEdge(E);
        for (const FVertexInstanceID I : Instances) if (MD->IsVertexInstanceValid(I)) MD->DeleteVertexInstance(I);
        TArray<FVertexID> Lonely;
        for (const FVertexID V : MD->Vertices().GetElementIDs()) if (MD->GetVertexVertexInstanceIDs(V).Num() == 0 && MD->GetVertexConnectedEdgeIDs(V).Num() == 0) Lonely.Add(V);
        for (const FVertexID V : Lonely) MD->DeleteVertex(V);
        FElementIDRemappings Remap;
        MD->Compact(Remap);
        Mesh->CommitMeshDescription(0);
        Mesh->Build(true);
        Mesh->PostEditChange();
        Mesh->MarkPackageDirty();
    }
    return Doomed.Num();
#else
    Report = TEXT("editor only");
    return -1;
#endif
}
