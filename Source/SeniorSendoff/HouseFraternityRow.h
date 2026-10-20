#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;
namespace HouseFraternityRow
{
/** Upgrade can be disabled as a complete legacy scene/performance control. */
bool Enabled();
/** Cost-only A/B switch; emissive fixtures and all scene geometry remain present. */
bool LightsEnabled();
int32 ExteriorLightBudget();
struct FHouse
{
    const TCHAR* Id;
    int32 Index; // west to east,0..4; architecture and household state
    float L,R,F,B,Floor,Height;
    float X() const { return (L+R)*.5f; }
    float Y() const { return (F+B)*.5f; }
    float Width() const { return R-L; }
    float Depth() const { return B-F; }
};
/** Stable world-centimetre descriptors. Keep current plan and registry IDs R1..R5. */
const TArray<FHouse>& Houses();
/** Offset wide rear landing keeps the existing common path clear. */
float RearStairX(const FHouse& House);
/** Real-light positions coincide with authored pendants and a live bulb on each party string. */
FVector PorchLight(const FHouse& House);
FVector YardLight(const FHouse& House);
void CreateLook(UHouseBlackHallSubsystem* Look);
/** Legacy decorative batch stays south ofY0 andX<-8560. Return ORIGINAL THREE planting NoGo boxes in BOTH modes. */
TArray<FBox2D> Build(FHallMesh& Mesh);
/** Permanent hollow walls/floors/porches/steps for five houses. New Tudor/QueenAnne decoration is in Detail, outside the legacy bounds. */
void BuildStructures(FHallMesh& Solid,FHallMesh& Detail,FHallMesh& Glow);
/** Floor-to-ceiling outside wall skin with actual front200cm/rear140cm door holes. Thickness defaults30cm. */
void Walls(FHallMesh& Mesh,const FHouse& House,FName Material,float Depth=30.f,float Inset=0.f);
}
