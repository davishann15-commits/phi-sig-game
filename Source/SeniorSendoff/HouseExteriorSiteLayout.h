#pragma once

// World-centimeter coordinates shared by the street, concrete approach and runtime review.
namespace HouseExteriorSite
{
constexpr float FrontStreetSetback = 1200.f;
constexpr float StreetY(float OriginalY) { return OriginalY - FrontStreetSetback; }
constexpr float FrontRoadNearY = StreetY(-2950.f);
constexpr float FrontRoadFarY = StreetY(-4350.f);
constexpr float FrontSidewalkInnerY = StreetY(-2700.f);
}
