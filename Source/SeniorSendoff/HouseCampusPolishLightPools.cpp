// Calder State exterior polish, step 3d (light): no lights, only the light the lamps would throw, laid on the ground as
// emissive pools (M_LightPoolDecal): blue-green under every mercury walk lamp, warm under the lit windows of the ground
// floors and round the ticket booths, the bonfire's embers, a bluish wash under the flood towers and over the field; and a
// low haze lying over the green and the quad (camera-facing puffs, see AHouseCampusPolish::UpdatePuffs).
#include "HouseCampusPolish.h"
#include "HouseCampusFall.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorCampus.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseBlackHallPaint.h"

void HouseCampusPolish::LightPools(FParts& P)
{
    constexpr float G = -60.f;   // today's grade: kept by the stadium's apron ring and field (the terrain holds them)
    const FLinearColor Mercury(.55f, .95f, .88f), Warm(1.f, .72f, .42f), Flood(.7f, .82f, 1.f);
    // The walk lamps' mercury heads, 5.2 m up.
    TArray<FVector2D> Lamps;
    HouseCampusLayout::LampPoints(Lamps);
    for (const FVector2D& L : Lamps) if (L.Y < -5600.f) P.Pools.Add({ FVector(L, HouseExteriorSite::GroundHeight(L.X, L.Y) + 2.f), 520.f, Mercury, .45f });
    // The lit windows of each building's ground floor throw a warm rectangle on the ground below them: the painted lit
    // windows only (on an open floor a door may take a window's place, and the real glass looks into a room that is dark
    // until someone comes near), at most five a building.
    for (int32 Seed = 0; Seed < HouseCampusLayout::Blocks().Num(); ++Seed)
    {
        const bool bOpen = HouseCampusInteriors::OpenFloors(Seed) > 0;
        int32 Spill = 0;
        const HouseCampusLayout::FBlock& B = HouseCampusLayout::Blocks()[Seed];
        const TCHAR Main = B.Faces[0];
        const bool bTurned = Main == TEXT('E') || Main == TEXT('W');
        const float W = bTurned ? B.Y1 - B.Y0 : B.X1 - B.X0, D = bTurned ? B.X1 - B.X0 : B.Y1 - B.Y0;
        const FTransform T = HouseCampusLayout::Frame(Seed);
        for (int32 Side = 0; Side < 4; ++Side)
        {
            TArray<FVector2D> Cols;
            HouseCampusInteriors::WindowColumns(Seed, Side, Cols);
            const FVector N = Side == 0 ? FVector(0, 1, 0) : Side == 1 ? FVector(0, -1, 0) : Side == 2 ? FVector(-1, 0, 0) : FVector(1, 0, 0);
            const float Plane = Side == 0 ? D * .5f : Side == 1 ? -D * .5f : Side == 2 ? -W * .5f : W * .5f;
            for (int32 I = 0; I < Cols.Num() && Spill < 5; ++I)
            {
                if (!HouseCampusFall::WindowLit(I, 0, Seed)) continue;
                if (bOpen && (HouseCampusInteriors::WindowReplaced(Seed, Side, 0, Cols[I].X, Cols[I].Y) || HouseCampusInteriors::WindowOpen(Seed, Side, 0, Cols[I].X, Cols[I].Y))) continue;
                const float U = (Cols[I].X + Cols[I].Y) * .5f;
                const FVector Local = (Side < 2 ? FVector(U, Plane, 0.f) : FVector(Plane, U, 0.f)) + N * 160.f + FVector(0, 0, G + 2.f);
                FVector World = T.TransformPosition(Local);
                // On the ground beside the wall, which on a hillside need not be the building's pad (a walk-out face).
                World.Z = HouseExteriorSite::GroundHeight(World.X, World.Y) + 2.f;
                if (HouseCampusLayout::OnTrafficLane(FVector2D(World))) continue;
                P.Pools.Add({ World, 190.f, Warm, .3f });
                ++Spill;
            }
        }
    }
    // The ticket booths at the north gates, the bonfire's embers.
    for (const FVector2D& Booth : { FVector2D(3330.f, -9030.f), FVector2D(1560.f, -8975.f) }) P.Pools.Add({ FVector(Booth, G + 4.f), 320.f, Warm, .35f });
    if (HouseCampusFall::Enabled() && HouseCampusFall::BonfireSpot().Z > 0.f)
    {
        const FVector2D Fire(HouseCampusFall::BonfireSpot());
        P.Pools.Add({ FVector(Fire, HouseExteriorSite::GroundHeight(Fire.X, Fire.Y) + 2.f), 700.f, FLinearColor(1.f, .45f, .15f), .5f });
    }
    // The floods: a bluish wash under each tower, and over the field.
    TArray<FFloodBank> Banks;
    FloodBanks(Banks);
    for (const FFloodBank& B : Banks) P.Pools.Add({ FVector(B.Centre.X, B.Centre.Y, G + 3.f) + FVector(B.Facing.X, B.Facing.Y, 0.f).GetSafeNormal() * 600.f, 1300.f, Flood, .22f });
    for (const float X : { -1800.f, 0.f, 1800.f }) P.Pools.Add({ FVector(HouseCampus::StadiumX + X, HouseCampus::StadiumY, G + 4.f), 2200.f, Flood, .1f });
    // A low haze over the green and the quad: soft puffs a metre or so off the grass, thicker in the hollows.
    FRandomStream R(1986);
    const FBox2D Areas[] = { FBox2D(FVector2D(-1500.f, -9200.f), FVector2D(7000.f, -6900.f)), FBox2D(FVector2D(-14200.f, -12400.f), FVector2D(-9700.f, -9300.f)), FBox2D(FVector2D(-4000.f, -16500.f), FVector2D(-1000.f, -12000.f)) };
    for (const FBox2D& A : Areas)
        for (int32 K = 0; K < 26; ++K)
        {
            const FVector2D At(R.FRandRange(A.Min.X, A.Max.X), R.FRandRange(A.Min.Y, A.Max.Y));
            const float Hollow = HallFbm(At.X * .0004f, At.Y * .0004f, 3, 7);
            P.Puffs.Add({ FVector(At, HouseExteriorSite::GroundHeight(At.X, At.Y) + R.FRandRange(60.f, 140.f)), R.FRandRange(900.f, 1600.f), FLinearColor(.66f, .7f, .78f, .11f + .09f * Hollow), 0 });
        }
}
