// Small geometry helpers shared by the homecoming campus files (HouseCampusFall*). Header-only, in their own namespace.
#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHallMesh.h"
#include "HouseBlackHallPaint.h"

namespace HouseCampusFallKit
{
inline const FHallUV& PlainUV() { static const FHallUV UV = FHallUV::World(100.f); return UV; }

inline void Tri(FHallMesh& M, FName Key, const FVector& A, const FVector& B, const FVector& C, const FVector& Nrm, const FHallUV& UV = PlainUV())
{
    FHallSection& Sec = M.Sec(Key);
    const int32 Base = Sec.V.Num();
    FVector UAx, VAx; FHallMesh::PlanarAxes(Nrm, UAx, VAx);
    for (const FVector& P : { A, B, C }) { Sec.V.Add(P); Sec.N.Add(Nrm); Sec.UV.Add(M.ComputeUV(P, Nrm, UV)); Sec.T.Add(FProcMeshTangent(UAx, false)); }
    if (FVector::DotProduct(FVector::CrossProduct(B - A, C - A), Nrm) < 0) Sec.I.Append({ Base, Base + 1, Base + 2 });
    else Sec.I.Append({ Base, Base + 2, Base + 1 });
}

/** Square-section bar from A to B; its thickness runs toward Facing (which must not be parallel to AB). */
inline void Bar(FHallMesh& M, FName Key, const FVector& A, const FVector& B, float Width, float Thick, const FVector& Facing = FVector::UpVector)
{
    const FVector Along = B - A;
    const float Len = Along.Size();
    if (Len < .01f) return;
    FVector F = Facing;
    if (FMath::Abs(FVector::DotProduct(F.GetSafeNormal(), Along / Len)) > .98f) F = FMath::Abs(Along.Z / Len) > .7f ? FVector::ForwardVector : FVector::UpVector;
    M.Box(Key, FTransform(FRotationMatrix::MakeFromXZ(Along / Len, F).ToQuat(), (A + B) * .5f), FVector(Len * .5f, Width * .5f, Thick * .5f), PlainUV());
}

inline void Ball(FHallMesh& M, FName Key, const FVector& C, float R, int32 Seg)
{
    M.Dome(Key, C, FVector::UpVector, R, R, Seg);
    M.Dome(Key, C, -FVector::UpVector, R, R, Seg);
}

/** Flat polygon (convex or star-shaped about Hub) facing Nrm. */
inline void Fan(FHallMesh& M, FName Key, const FVector& Hub, const TArray<FVector>& Rim, const FVector& Nrm, bool bClosed = true)
{
    for (int32 K = 0; K + 1 < Rim.Num() + (bClosed ? 1 : 0); ++K) Tri(M, Key, Hub, Rim[K], Rim[(K + 1) % Rim.Num()], Nrm);
}

/** A rectangle carrying a whole texture: Center, the reader's Right and Up, facing N. */
inline void Card(FHallMesh& M, FName Key, const FVector& Center, const FVector& Right, const FVector& Up, float Width, float Height, const FVector& N)
{
    const FVector R = Right * (Width * .5f), U = Up * (Height * .5f);
    M.QuadUV(Key, Center - R - U, Center + R - U, Center + R + U, Center - R + U, N, FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0), Right);
}

/** Surface of revolution about +Z at Base: Profile is (radius, height) going round the outline. */
inline void Lathe(FHallMesh& M, FName Key, const FVector& Base, const TArray<FVector2D>& Profile, int32 Seg, float Squash = 1.f)
{
    for (int32 J = 0; J + 1 < Profile.Num(); ++J)
    {
        const FVector2D A = Profile[J], B = Profile[J + 1], T = B - A;
        const FVector2D Out = FVector2D(-T.Y, T.X).GetSafeNormal();
        for (int32 K = 0; K < Seg; ++K)
        {
            const float A0 = 2.f * PI * K / Seg, A1 = 2.f * PI * (K + 1) / Seg;
            const FVector D0(FMath::Cos(A0), FMath::Sin(A0) * Squash, 0.f), D1(FMath::Cos(A1), FMath::Sin(A1) * Squash, 0.f);
            const FVector Nrm = ((D0 + D1).GetSafeNormal() * Out.X + FVector::UpVector * Out.Y).GetSafeNormal();
            const FVector P0 = Base + D0 * A.X + FVector(0, 0, A.Y), P1 = Base + D1 * A.X + FVector(0, 0, A.Y), P2 = Base + D1 * B.X + FVector(0, 0, B.Y), P3 = Base + D0 * B.X + FVector(0, 0, B.Y);
            if (A.X < .01f) Tri(M, Key, Base + FVector(0, 0, A.Y), P2, P3, Nrm);
            else if (B.X < .01f) Tri(M, Key, P0, P1, Base + FVector(0, 0, B.Y), Nrm);
            else M.Quad(Key, P0, P1, P2, P3, Nrm, PlainUV());
        }
    }
}

/** A ribbed pumpkin (radius R, height H) at Base on the ground; its stem leans a little. */
inline void Pumpkin(FHallMesh& M, FName Body, FName Stem, const FVector& Base, float R, float H, int32 Ribs, float Yaw)
{
    constexpr int32 Seg = 28, Rings = 8;
    TArray<FVector> Ring[Rings + 1];
    for (int32 J = 0; J <= Rings; ++J)
    {
        const float V = J / float(Rings), Lat = PI * (V - .5f);
        const float Rr = R * FMath::Cos(Lat) * (J == 0 || J == Rings ? .25f : 1.f), Z = H * .5f * (1.f + FMath::Sin(Lat));
        for (int32 K = 0; K < Seg; ++K)
        {
            const float A = 2.f * PI * K / Seg + FMath::DegreesToRadians(Yaw);
            const float Rib = 1.f - .1f * FMath::Square(FMath::Sin(Ribs * .5f * (A - FMath::DegreesToRadians(Yaw))));
            Ring[J].Add(Base + FVector(FMath::Cos(A) * Rr * Rib, FMath::Sin(A) * Rr * Rib, Z));
        }
    }
    const FVector Mid = Base + FVector(0, 0, H * .5f);
    for (int32 J = 0; J < Rings; ++J)
        for (int32 K = 0; K < Seg; ++K)
        {
            const FVector &A = Ring[J][K], &B = Ring[J][(K + 1) % Seg], &C = Ring[J + 1][(K + 1) % Seg], &D = Ring[J + 1][K];
            M.Quad(Body, A, B, C, D, ((A + B + C + D) * .25f - Mid).GetSafeNormal(), PlainUV());
        }
    Fan(M, Body, Base + FVector(0, 0, H), Ring[Rings], FVector::UpVector);
    const FVector Top = Base + FVector(0, 0, H * .97f);
    M.Cylinder(Stem, Top, Top + FVector(R * .08f, R * .05f, H * .22f), R * .09f, 6, PlainUV());
}

/** A carved face glowing on a pumpkin: two triangle eyes, a nose and a gap-toothed grin, just proud of the skin. */
inline void JackFace(FHallMesh& M, FName Glow, const FVector& Base, float R, float H, float Yaw, int32 Style)
{
    const FVector Out = FRotator(0.f, Yaw, 0.f).Vector(), Right = FVector::CrossProduct(FVector::UpVector, Out), Up = FVector::UpVector;
    const FVector C = Base + Out * (R * .99f) + FVector(0, 0, H * .52f);
    auto P = [&](float X, float Y) { return C + Right * (X * R) + Up * (Y * H); };
    const FVector N = Out;
    const float Tilt = Style % 2 ? .06f : 0.f;
    for (const float S : { -1.f, 1.f })
        Tri(M, Glow, P(S * .42f, .1f + Tilt * S), P(S * .14f, .1f - Tilt * S), P(S * .3f, .3f), N);
    Tri(M, Glow, P(-.07f, -.02f), P(.07f, -.02f), P(0.f, .1f), N);
    TArray<FVector> Mouth;
    const int32 Teeth = Style % 3 + 2;
    for (int32 K = 0; K <= 8; ++K) { const float X = -.5f + K / 8.f; Mouth.Add(P(X * .95f, -.2f - .12f * (1.f - FMath::Square(2.f * X)))); }
    for (int32 K = 8; K >= 0; --K)
    {
        const float X = -.5f + K / 8.f;
        const bool bTooth = (K % (8 / Teeth + 1)) == 1;
        Mouth.Add(P(X * .9f, -.12f - (bTooth ? .07f : 0.f) - .05f * (1.f - FMath::Square(2.f * X))));
    }
    // The grin is a strip between the two curves.
    for (int32 K = 0; K < 8; ++K) M.Quad(Glow, Mouth[K], Mouth[K + 1], Mouth[17 - K - 1], Mouth[17 - K], N, PlainUV());
}

/** A rectangular straw bale (90 x 45 x 38) with two twine bands. */
inline void HayBale(FHallMesh& M, FName Straw, FName Twine, const FVector& Base, float Yaw)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    M.Box(Straw, FTransform(Q, Base + FVector(0, 0, 19)), FVector(45, 22.5f, 19), FHallUV::World(90.f));
    for (const float S : { -16.f, 16.f }) M.Box(Twine, FTransform(Q, Base + Q.RotateVector(FVector(S, 0, 19))), FVector(.6f, 22.8f, 19.3f), PlainUV());
}

/** A 1980s ten-speed: steel diamond frame, drop bars, thin wheels. Base on the ground, facing Yaw. */
inline void TenSpeed(FHallMesh& M, FName Frame, FName Tyre, FName Chrome, const FVector& Base, float Yaw)
{
    const FQuat Q = FRotator(0.f, Yaw, 0.f).Quaternion();
    auto P = [&](float X, float Z, float Y = 0.f) { return Base + Q.RotateVector(FVector(X, Y, Z)); };
    const FVector Side = Q.RotateVector(FVector(0, 1, 0));
    constexpr float WR = 34.f;
    for (const float X : { -52.f, 52.f })
    {
        constexpr int32 Seg = 20;
        for (int32 K = 0; K < Seg; ++K)
        {
            const float A0 = 2.f * PI * K / Seg, A1 = 2.f * PI * (K + 1) / Seg;
            Bar(M, Tyre, P(X + FMath::Cos(A0) * WR, WR + FMath::Sin(A0) * WR), P(X + FMath::Cos(A1) * WR, WR + FMath::Sin(A1) * WR), 2.4f, 2.4f, Side);
        }
        M.Cylinder(Chrome, P(X, WR, -4.f), P(X, WR, 4.f), 3.f, 8, PlainUV());
    }
    const FVector BB = P(0, 28), Seat = P(-14, 82), Head = P(34, 80), Rear = P(-52, WR), Front = P(52, WR);
    for (const auto& Tube : { TPair<FVector, FVector>(BB, Seat), TPair<FVector, FVector>(BB, Head), TPair<FVector, FVector>(Seat, Head), TPair<FVector, FVector>(BB, Rear), TPair<FVector, FVector>(Seat, Rear), TPair<FVector, FVector>(Head, Front) })
        Bar(M, Frame, Tube.Key, Tube.Value, 2.6f, 2.6f, Side);
    Bar(M, Chrome, Seat, Seat + FVector(0, 0, 9), 1.6f, 1.6f, Side);
    M.Box(Frame, FTransform(Q, Seat + FVector(0, 0, 10)), FVector(13, 6, 2), PlainUV());
    const FVector Stem = Head + FVector(0, 0, 8);
    Bar(M, Chrome, Head, Stem, 1.8f, 1.8f, Side);
    for (const float S : { -1.f, 1.f })
    {
        const FVector Grip = Stem + Side * (S * 20.f);
        Bar(M, Chrome, Stem, Grip, 1.6f, 1.6f, FVector::UpVector);
        Bar(M, Chrome, Grip, Grip + Q.RotateVector(FVector(8, 0, -8)), 1.6f, 1.6f, Side);
        Bar(M, Chrome, Grip + Q.RotateVector(FVector(8, 0, -8)), Grip + Q.RotateVector(FVector(-2, 0, -14)), 1.6f, 1.6f, Side);
    }
}

/** HallGlyph letters as flat strips on a surface (chalk, painted lettering). Returns the total width. */
inline float GlyphStrips(FHallMesh& M, FName Key, const FString& Text, const FVector& Origin, const FVector& Right, const FVector& Up, const FVector& N, float Cap, float Stroke, int32 WobbleSeed = 0)
{
    const float Unit = Cap / 6.f, Advance = 5.f * Unit;
    FRandomStream R(WobbleSeed);
    for (int32 K = 0; K < Text.Len(); ++K)
    {
        const FVector Start = Origin + Right * (Advance * K);
        const float Lean = WobbleSeed ? R.FRandRange(-.08f, .08f) : 0.f, Lift = WobbleSeed ? R.FRandRange(-.06f, .06f) * Cap : 0.f;
        for (const TArray<FVector2D>& Line : HallGlyph(Text[K]))
            for (int32 J = 1; J < Line.Num(); ++J)
            {
                auto W = [&](const FVector2D& G) { return Start + Right * ((G.X + G.Y * Lean) * Unit) + Up * (G.Y * Unit + Lift); };
                const FVector A = W(Line[J - 1]), B = W(Line[J]), D = (B - A).GetSafeNormal(), Side = FVector::CrossProduct(N, D).GetSafeNormal() * (Stroke * .5f);
                const FVector A2 = A - D * Stroke * .4f, B2 = B + D * Stroke * .4f;
                M.Quad(Key, A2 - Side, B2 - Side, B2 + Side, A2 + Side, N, PlainUV());
            }
    }
    return Advance * Text.Len() - Unit;
}

/** 5 x 7 incandescent scoreboard font: rows top to bottom, bit 4 is the left column. */
inline const uint8* BulbGlyph(TCHAR Ch)
{
    static const uint8 Blank[7] = { 0, 0, 0, 0, 0, 0, 0 };
    static const TMap<TCHAR, TArray<uint8>> Font = []
    {
        TMap<TCHAR, TArray<uint8>> F;
        auto Add = [&F](TCHAR C, std::initializer_list<uint8> Rows) { F.Add(C, TArray<uint8>(Rows)); };
        Add('0', { 14, 17, 19, 21, 25, 17, 14 }); Add('1', { 4, 12, 4, 4, 4, 4, 14 }); Add('2', { 14, 17, 1, 2, 4, 8, 31 });
        Add('3', { 31, 2, 4, 2, 1, 17, 14 }); Add('4', { 2, 6, 10, 18, 31, 2, 2 }); Add('5', { 31, 16, 30, 1, 1, 17, 14 });
        Add('6', { 6, 8, 16, 30, 17, 17, 14 }); Add('7', { 31, 1, 2, 4, 8, 8, 8 }); Add('8', { 14, 17, 17, 14, 17, 17, 14 });
        Add('9', { 14, 17, 17, 15, 1, 2, 12 });
        Add('A', { 14, 17, 17, 31, 17, 17, 17 }); Add('B', { 30, 17, 17, 30, 17, 17, 30 }); Add('C', { 14, 17, 16, 16, 16, 17, 14 });
        Add('D', { 28, 18, 17, 17, 17, 18, 28 }); Add('E', { 31, 16, 16, 30, 16, 16, 31 }); Add('F', { 31, 16, 16, 30, 16, 16, 16 });
        Add('G', { 14, 17, 16, 23, 17, 17, 15 }); Add('H', { 17, 17, 17, 31, 17, 17, 17 }); Add('I', { 14, 4, 4, 4, 4, 4, 14 });
        Add('J', { 7, 2, 2, 2, 2, 18, 12 }); Add('K', { 17, 18, 20, 24, 20, 18, 17 }); Add('L', { 16, 16, 16, 16, 16, 16, 31 });
        Add('M', { 17, 27, 21, 21, 17, 17, 17 }); Add('N', { 17, 17, 25, 21, 19, 17, 17 }); Add('O', { 14, 17, 17, 17, 17, 17, 14 });
        Add('P', { 30, 17, 17, 30, 16, 16, 16 }); Add('Q', { 14, 17, 17, 17, 21, 18, 13 }); Add('R', { 30, 17, 17, 30, 20, 18, 17 });
        Add('S', { 15, 16, 16, 14, 1, 1, 30 }); Add('T', { 31, 4, 4, 4, 4, 4, 4 }); Add('U', { 17, 17, 17, 17, 17, 17, 14 });
        Add('V', { 17, 17, 17, 17, 17, 10, 4 }); Add('W', { 17, 17, 17, 21, 21, 21, 10 }); Add('X', { 17, 17, 10, 4, 10, 17, 17 });
        Add('Y', { 17, 17, 10, 4, 4, 4, 4 }); Add('Z', { 31, 1, 2, 4, 8, 16, 31 });
        Add(':', { 0, 12, 12, 0, 12, 12, 0 }); Add('\'', { 12, 12, 4, 8, 0, 0, 0 }); Add('!', { 4, 4, 4, 4, 4, 0, 4 });
        Add('-', { 0, 0, 0, 31, 0, 0, 0 }); Add('#', { 10, 10, 31, 10, 31, 10, 10 }); Add('.', { 0, 0, 0, 0, 0, 12, 12 });
        return F;
    }();
    const TArray<uint8>* G = Font.Find(FChar::ToUpper(Ch));
    return G ? G->GetData() : Blank;
}

/** Point on a hanging cord between A and B (T 0..1), sagging Sag at the middle. */
inline FVector Cord(const FVector& A, const FVector& B, float T, float Sag) { return FMath::Lerp(A, B, T) - FVector(0, 0, Sag * 4.f * T * (1.f - T)); }
}
