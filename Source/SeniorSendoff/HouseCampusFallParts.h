// Internal to the homecoming campus (HouseCampusFall*.cpp): what the stadium and campus builders hand to the actor.
#pragma once

#include "CoreMinimal.h"
#include "HouseBlackHallMesh.h"

namespace HouseCampusFallParts
{
/** A cloth that flutters (a sheet, a pennant, a goalpost streamer): a Cols x Rows grid from its top-left corner Origin,
    Width along Right and Height along Down, seen from the front (normal Right x Down). */
struct FCloth
{
    FName Front, Back;
    FVector Origin = FVector::ZeroVector, Right = FVector::ForwardVector, Down = -FVector::UpVector;
    float Width = 100.f, Height = 100.f;
    int32 Cols = 6, Rows = 4;
    float Amp = 1.f;      // flutter strength (cm at full swing is about 6 x Amp)
    float Pinned = 0.f;   // 0 hangs from its top edge; 1 is tied at its four corners (a sheet on a wall)
    float Taper = 0.f;    // 1 narrows to a point at the bottom (a pennant)
};

/** A soft camera-facing glow: haze around a flood head or a lamp. */
struct FHalo
{
    FVector P = FVector::ZeroVector;
    float Size = 100.f;
    FLinearColor Color = FLinearColor::White;   // vertex colour: RGB glow, A opacity
    float Flicker = 0.f;                         // 1 breathes with the candles and embers it surrounds
};

/** Everything the builders make. The static meshes are split by area so each culls as a group. */
struct FParts
{
    FHallMesh Field, Stands, Score, Gate, Green, Solid, Dorms, Lamps, Walks;
    TArray<FCloth> Cloths;
    TArray<FHalo> Halos;
    FVector Bonfire = FVector::ZeroVector;
    FBox LeafBox = FBox(ForceInit);   // where leaves come down (the front green)
    int32 Lanterns = 0;
    /** The exterior's trees (X, Y, and the instance scale in Z), read before building so props keep clear of trunks
        and the leaf piles gather under the crowns. */
    TArray<FVector> Trees;

    /** No tree trunk or low crown within Radius of At. */
    bool Clear(const FVector2D& At, float Radius) const
    {
        for (const FVector& T : Trees) if (FVector2D::DistSquared(At, FVector2D(T.X, T.Y)) < FMath::Square(Radius + 180.f * T.Z)) return false;
        return true;
    }
    /** The first of Candidates that is Clear, else the last. */
    FVector2D FirstClear(const TArray<FVector2D>& Candidates, float Radius) const
    {
        for (const FVector2D& C : Candidates) if (Clear(C, Radius)) return C;
        return Candidates.Last();
    }
};

/** The football stadium: field, goalposts, benches, scoreboard, press box, stands dressing, gate, float and band trailer. */
void Stadium(FParts& P);
/** The campus around it: dorm windows, lamp banners, kiosk, news boxes, phone booth, bikes, leaves, chalk, bonfire. */
void Campus(FParts& P);
}
