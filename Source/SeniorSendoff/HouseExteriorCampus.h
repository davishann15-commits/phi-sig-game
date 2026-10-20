#pragma once
#include "CoreMinimal.h"
struct FHallMesh;
class UHouseBlackHallSubsystem;
namespace HouseCampus
{
constexpr float StadiumX=2400.f,StadiumY=-13300.f;
constexpr float ForestStartY=4650.f;
struct FLayout
{
    TArray<FBox2D> BuildingFootprints;
    TArray<FVector> StadiumLamps;
    int32 Buildings=0;
};
/** A plan building's foundation on the Calder relief (SourceArt/CalderLandscape, Phase 1 "Buildings on slopes"), in the
 *  building's canonical frame (main entrance on +Y, its floor on the pad at -60). Where the ground by a wall lies below the
 *  floor, BuildCampus carries the shell down to 30 cm below the lowest ground there: a stone plinth course for a fall of up
 *  to 60 cm, deeper a rusticated basement storey with dark blind windows on the facade's columns, and on the lowest side
 *  that has room one fixed service door (never an ACampusDoor) on a stoop. Nothing at all where the relief within 2 m of the
 *  footprint is flat (today), nor in a frozen zone. */
struct FFoundation
{
    bool bActive=false;                 // the relief round the building is not flat (the rest is unset when false)
    float Drop[4]={0,0,0,0};            // per side (0 +Y main, 1 -Y, 2 -X, 3 +X): the floor less the lowest ground by that wall
    float Rise[4]={0,0,0,0};            // the highest ground by that wall less the floor (uphill burial: the landform's to fix)
    float Bottom[4]={0,0,0,0};          // where the foundation stops on that side (only where Drop > 5 cm)
    int32 DoorSide=-1;                  // the service door: its side (-1 none), its middle along the side, its sill
    float DoorU=0,DoorSill=0;
};
bool Foundation(int32 Seed,FFoundation& Out);
/** The ground under a point of building Seed's canonical frame, in that frame (HouseExteriorSite::GroundHeight less its pad). */
float FoundationGround(int32 Seed,const FVector2D& Canon);
/** An entrance's planter or canopy post (a box X0..X1 x Y0..Y1 in building Seed's canonical frame) stands clear of the Calder
 *  stairs and ramps (a flight that comes down to the door: the Union steps). Always true with the features off (today). */
bool EntranceStandClear(int32 Seed,float X0,float X1,float Y0,float Y1);
void CreateLook(UHouseBlackHallSubsystem* Look);
FLayout BuildCampus(FHallMesh& Near,FHallMesh& Far);
void Mountains(FHallMesh& Far);
void RearTrees(TArray<FTransform>& Big,TArray<FTransform>& Small,TArray<FTransform>& Distant);
void DistantTree(FHallMesh& Mesh,int32 Variant);
}
