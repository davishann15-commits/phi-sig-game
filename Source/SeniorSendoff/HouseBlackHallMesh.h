#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

class UMaterialInterface;

/** UV mapping for generated geometry. */
struct FHallUV
{
    /** 0 = world planar (texture period in cm), 1 = local planar (period in cm, local space), 2 = explicit box mapping into 0..1 over the box */
    int32 Mode = 0;
    float Period = 100.f;
    FVector2D Offset = FVector2D::ZeroVector;
    /** For local/box modes: the transform used for planar projection. */
    FTransform Frame = FTransform::Identity;
    /** Box mode: size of the box (local) */
    FVector BoxSize = FVector(100);
    /** Optional height reference: v = (TopZ - z) / Period when bTopDown is set (local z). */
    bool bTopDown = false;
    float TopZ = 0.f;

    static FHallUV World(float Period, FVector2D Offset = FVector2D::ZeroVector) { FHallUV U; U.Mode = 0; U.Period = Period; U.Offset = Offset; return U; }
    static FHallUV Local(const FTransform& Frame, float Period) { FHallUV U; U.Mode = 1; U.Frame = Frame; U.Period = Period; return U; }
};

struct FHallSection
{
    TArray<FVector> V; TArray<int32> I; TArray<FVector> N; TArray<FVector2D> UV; TArray<FProcMeshTangent> T;
};

/** Accumulates procedural geometry grouped by material key, then builds a UProceduralMeshComponent. */
struct FHallMesh
{
    TMap<FName, FHallSection> Sections;
    TArray<FName> Order;

    FHallSection& Sec(FName Key)
    {
        if (!Sections.Contains(Key)) Order.Add(Key);
        return Sections.FindOrAdd(Key);
    }

    static void PlanarAxes(const FVector& Nrm, FVector& UAx, FVector& VAx)
    {
        if (FMath::Abs(Nrm.Z) > .7f)
        {
            UAx = FVector(1, 0, 0);
            VAx = FVector::CrossProduct(Nrm, UAx).GetSafeNormal();
        }
        else
        {
            UAx = FVector::CrossProduct(Nrm, FVector::UpVector).GetSafeNormal();
            VAx = FVector(0, 0, -1);
        }
    }

    FVector2D ComputeUV(const FVector& P, const FVector& Nrm, const FHallUV& M) const
    {
        FVector UAx, VAx;
        if (M.Mode == 0)
        {
            PlanarAxes(Nrm, UAx, VAx);
            float V = FVector::DotProduct(P, VAx);
            if (M.bTopDown) V = M.TopZ - P.Z;
            return FVector2D(FVector::DotProduct(P, UAx) / M.Period + M.Offset.X, V / M.Period + M.Offset.Y);
        }
        const FVector LP = M.Frame.InverseTransformPosition(P);
        const FVector LN = M.Frame.InverseTransformVectorNoScale(Nrm);
        PlanarAxes(LN, UAx, VAx);
        if (M.Mode == 2)
        {
            // Normalised across the box: u over the face's horizontal axis, v top-down.
            const FVector Half = M.BoxSize * .5f;
            const float UExt = FMath::Abs(UAx.X) * Half.X + FMath::Abs(UAx.Y) * Half.Y + FMath::Abs(UAx.Z) * Half.Z;
            const float VExt = FMath::Abs(VAx.X) * Half.X + FMath::Abs(VAx.Y) * Half.Y + FMath::Abs(VAx.Z) * Half.Z;
            return FVector2D(FVector::DotProduct(LP, UAx) / FMath::Max(1.f, 2 * UExt) + .5f, FVector::DotProduct(LP, VAx) / FMath::Max(1.f, 2 * VExt) + .5f);
        }
        float V = FVector::DotProduct(LP, VAx);
        if (M.bTopDown && FMath::Abs(LN.Z) < .7f) V = M.TopZ - LP.Z;
        return FVector2D(FVector::DotProduct(LP, UAx) / M.Period + M.Offset.X, V / M.Period + M.Offset.Y);
    }

    /** Quad with corners in any winding; it is oriented so its front faces Nrm (UE front faces are wound opposite to the right-hand cross product). */
    void Quad(FName Key, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Nrm, const FHallUV& M)
    {
        FHallSection& S = Sec(Key);
        const int32 Base = S.V.Num();
        const FVector P[4] = { A, B, C, D };
        FVector UAx, VAx;
        const FVector TN = M.Mode == 0 ? Nrm : M.Frame.InverseTransformVectorNoScale(Nrm);
        PlanarAxes(TN, UAx, VAx);
        const FVector TangentWorld = M.Mode == 0 ? UAx : M.Frame.TransformVectorNoScale(UAx);
        for (int32 K = 0; K < 4; ++K)
        {
            S.V.Add(P[K]); S.N.Add(Nrm); S.UV.Add(ComputeUV(P[K], Nrm, M));
            S.T.Add(FProcMeshTangent(TangentWorld, false));
        }
        const bool bFront = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Nrm) < 0;
        if (bFront) { S.I.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 }); }
        else { S.I.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 }); }
    }

    /** Quad with explicit UVs (A,B,C,D around the quad); Tangent = direction of +U. */
    void QuadUV(FName Key, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Nrm,
        const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FVector2D& UD, const FVector& Tangent)
    {
        FHallSection& S = Sec(Key);
        const int32 Base = S.V.Num();
        const FVector P[4] = { A, B, C, D }; const FVector2D UV[4] = { UA, UB, UC, UD };
        for (int32 K = 0; K < 4; ++K) { S.V.Add(P[K]); S.N.Add(Nrm); S.UV.Add(UV[K]); S.T.Add(FProcMeshTangent(Tangent, false)); }
        const bool bFront = FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Nrm) < 0;
        if (bFront) { S.I.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 }); }
        else { S.I.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 }); }
    }

    /** Oriented box. Faces whose bit is set in SkipMask are omitted (bits: +X,-X,+Y,-Y,+Z,-Z). */
    void Box(FName Key, const FTransform& T, const FVector& Half, const FHallUV& M, int32 SkipMask = 0)
    {
        const FVector Axes[3] = { T.GetUnitAxis(EAxis::X), T.GetUnitAxis(EAxis::Y), T.GetUnitAxis(EAxis::Z) };
        const FVector C = T.GetLocation();
        const FVector H(Half.X * T.GetScale3D().X, Half.Y * T.GetScale3D().Y, Half.Z * T.GetScale3D().Z);
        for (int32 Face = 0; Face < 6; ++Face)
        {
            if (SkipMask & (1 << Face)) continue;
            const int32 Ax = Face / 2; const float Sgn = (Face % 2 == 0) ? 1.f : -1.f;
            const FVector N = Axes[Ax] * Sgn;
            const FVector U = Axes[(Ax + 1) % 3], W = Axes[(Ax + 2) % 3];
            const float HU = H[(Ax + 1) % 3], HW = H[(Ax + 2) % 3];
            const FVector Ctr = C + N * H[Ax];
            Quad(Key, Ctr - U * HU - W * HW, Ctr + U * HU - W * HW, Ctr + U * HU + W * HW, Ctr - U * HU + W * HW, N, M);
        }
    }

    /** Axis-aligned box from world min/max. */
    void BoxMinMax(FName Key, const FVector& Min, const FVector& Max, const FHallUV& M, int32 SkipMask = 0)
    {
        Box(Key, FTransform((Min + Max) * .5f), (Max - Min) * .5f, M, SkipMask);
    }

    /** Cylinder between two points. */
    void Cylinder(FName Key, const FVector& A, const FVector& B, float R, int32 Seg, const FHallUV& M, bool bCaps = true)
    {
        const FVector Ax = (B - A).GetSafeNormal();
        FVector X = FVector::CrossProduct(Ax, FMath::Abs(Ax.Z) < .9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
        FVector Y = FVector::CrossProduct(Ax, X);
        FHallSection& S = Sec(Key);
        const float Len = (B - A).Size();
        for (int32 K = 0; K < Seg; ++K)
        {
            const float A0 = 2 * PI * K / Seg, A1 = 2 * PI * (K + 1) / Seg;
            const FVector D0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0), D1 = X * FMath::Cos(A1) + Y * FMath::Sin(A1);
            const int32 Base = S.V.Num();
            const FVector P[4] = { A + D0 * R, A + D1 * R, B + D1 * R, B + D0 * R };
            const FVector N[4] = { D0, D1, D1, D0 };
            const float U0 = K / float(Seg), U1 = (K + 1) / float(Seg);
            const FVector2D UV[4] = { FVector2D(U0, 0), FVector2D(U1, 0), FVector2D(U1, Len / M.Period), FVector2D(U0, Len / M.Period) };
            for (int32 Q = 0; Q < 4; ++Q) { S.V.Add(P[Q]); S.N.Add(N[Q]); S.UV.Add(UV[Q]); S.T.Add(FProcMeshTangent(Ax, false)); }
            if (FVector::DotProduct(FVector::CrossProduct(P[1] - P[0], P[2] - P[0]), D0 + D1) < 0) S.I.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
            else S.I.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
        }
        if (bCaps)
            for (int32 End = 0; End < 2; ++End)
            {
                const FVector Ctr = End ? B : A; const FVector N = End ? Ax : -Ax;
                const int32 Base = S.V.Num();
                S.V.Add(Ctr); S.N.Add(N); S.UV.Add(FVector2D(.5f, .5f)); S.T.Add(FProcMeshTangent(X, false));
                for (int32 K = 0; K <= Seg; ++K)
                {
                    const float A0 = 2 * PI * K / Seg;
                    const FVector D0 = X * FMath::Cos(A0) + Y * FMath::Sin(A0);
                    S.V.Add(Ctr + D0 * R); S.N.Add(N); S.UV.Add(FVector2D(.5f + .5f * FMath::Cos(A0), .5f + .5f * FMath::Sin(A0))); S.T.Add(FProcMeshTangent(X, false));
                }
                for (int32 K = 0; K < Seg; ++K)
                {
                    const FVector P0 = S.V[Base], P1 = S.V[Base + 1 + K], P2 = S.V[Base + 2 + K];
                    if (FVector::DotProduct(FVector::CrossProduct(P1 - P0, P2 - P0), N) < 0) S.I.Append({ Base, Base + 1 + K, Base + 2 + K });
                    else S.I.Append({ Base, Base + 2 + K, Base + 1 + K });
                }
            }
    }

    /** Hemisphere / dome (for cameras, detectors). */
    void Dome(FName Key, const FVector& Base, const FVector& Up, float R, float Height, int32 Seg)
    {
        FVector X = FVector::CrossProduct(Up, FMath::Abs(Up.Z) < .9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
        FVector Y = FVector::CrossProduct(Up, X);
        FHallSection& S = Sec(Key);
        const int32 Rings = 5;
        const int32 Start = S.V.Num();
        for (int32 Rg = 0; Rg <= Rings; ++Rg)
        {
            const float Phi = HALF_PI * Rg / Rings;
            for (int32 K = 0; K <= Seg; ++K)
            {
                const float Th = 2 * PI * K / Seg;
                const FVector Dir = (X * FMath::Cos(Th) + Y * FMath::Sin(Th)) * FMath::Cos(Phi);
                const FVector P = Base + Dir * R + Up * FMath::Sin(Phi) * Height;
                S.V.Add(P); S.N.Add((Dir + Up * FMath::Sin(Phi)).GetSafeNormal()); S.UV.Add(FVector2D(K / float(Seg), Rg / float(Rings))); S.T.Add(FProcMeshTangent(X, false));
            }
        }
        for (int32 Rg = 0; Rg < Rings; ++Rg)
            for (int32 K = 0; K < Seg; ++K)
            {
                const int32 A = Start + Rg * (Seg + 1) + K, B = A + 1, C = A + Seg + 1, D = C + 1;
                const FVector Nrm = S.N[A] + S.N[D];
                if (FVector::DotProduct(FVector::CrossProduct(S.V[B] - S.V[A], S.V[D] - S.V[A]), Nrm) < 0) S.I.Append({ A, B, D, A, D, C });
                else S.I.Append({ A, D, B, A, C, D });
            }
    }

    /** Flat prism from a 2D outline in a plane (Origin, axes AxU/AxV, normal along Cross), extruded by Depth toward Normal. */
    void Prism(FName Key, const TArray<FVector2D>& Outline, const FVector& Origin, const FVector& AxU, const FVector& AxV, const FVector& Normal, float Depth, const FHallUV& M)
    {
        // Fan triangulation (outline must be convex or star-shaped around its centroid).
        FVector2D Ctr(0, 0); for (const FVector2D& P : Outline) Ctr += P; Ctr /= FMath::Max(1, Outline.Num());
        auto W = [&](const FVector2D& P, float D) { return Origin + AxU * P.X + AxV * P.Y + Normal * D; };
        FHallSection& S = Sec(Key);
        for (int32 Face = 0; Face < 2; ++Face)
        {
            const float D = Face ? Depth : 0.f; const FVector N = Face ? Normal : -Normal;
            const int32 Base = S.V.Num();
            S.V.Add(W(Ctr, D)); S.N.Add(N); S.UV.Add(ComputeUV(W(Ctr, D), N, M)); S.T.Add(FProcMeshTangent(AxU, false));
            for (const FVector2D& P : Outline) { S.V.Add(W(P, D)); S.N.Add(N); S.UV.Add(ComputeUV(W(P, D), N, M)); S.T.Add(FProcMeshTangent(AxU, false)); }
            for (int32 K = 0; K < Outline.Num(); ++K)
            {
                const int32 I1 = Base + 1 + K, I2 = Base + 1 + (K + 1) % Outline.Num();
                if (FVector::DotProduct(FVector::CrossProduct(S.V[I1] - S.V[Base], S.V[I2] - S.V[Base]), N) < 0) S.I.Append({ Base, I1, I2 });
                else S.I.Append({ Base, I2, I1 });
            }
        }
        for (int32 K = 0; K < Outline.Num(); ++K)
        {
            const FVector2D P0 = Outline[K], P1 = Outline[(K + 1) % Outline.Num()];
            const FVector A = W(P0, 0), B = W(P1, 0), C = W(P1, Depth), D = W(P0, Depth);
            FVector N = FVector::CrossProduct(B - A, Normal).GetSafeNormal();
            const FVector Mid = (A + B) * .5f;
            if (FVector::DotProduct(N, Mid - W(Ctr, 0)) < 0) N = -N;
            Quad(Key, A, B, C, D, N, M);
        }
    }

    int32 NumTriangles() const { int32 N = 0; for (const auto& P : Sections) N += P.Value.I.Num() / 3; return N; }

    /** Creates one section per key. Materials are looked up by key. */
    void Build(UProceduralMeshComponent* PM, TFunctionRef<UMaterialInterface*(FName)> MaterialFor, bool bCollision) const
    {
        int32 Index = PM->GetNumSections();
        for (const FName& Key : Order)
        {
            const FHallSection& S = Sections[Key];
            if (S.I.Num() == 0) continue;
            PM->CreateMeshSection(Index, S.V, S.I, S.N, S.UV, TArray<FColor>(), S.T, bCollision);
            PM->SetMaterial(Index, MaterialFor(Key));
            ++Index;
        }
    }
};
