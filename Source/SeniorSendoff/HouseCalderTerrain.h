// Calder in the mountains (SourceArt/CalderLandscape): the one ground everything outside the house stands on.
//
// Today's site is two planes (-60 in front, -362 behind the house) joined by a bank, with the woods' hills and the west
// field's mound behind. This adds Relief: tens of metres of landform on the campus side and round the house's side,
// held at exactly 0 in the frozen zones (the house, its lot, the court, the party field, the parking lot, the street,
// frat row, Calder Dr to Y 19800, the west field and the woods). Relief is analytic (named landforms, then constrained:
// frozen zones, building pads, the stadium apron, the tower plaza, road/lot/walk corridors, water), sampled once onto a
// 2 m lattice over the walkable square (+-32000); from then on the lattice is the authority, and the ground mesh, its
// collision and every placer read the same triangle surface.
//
//   HouseExteriorSite::GroundHeight(X,Y) = today's grade + ReliefLattice(X,Y)    (HouseExterior.cpp, next to Grade)
//   SurfaceZ(X,Y)                        = GroundHeight + today's paving offset there (road -12, walk +3, apron +5)
//
// -NoCalderTerrain (or -NoCampusLayout) makes every Relief, pad, lift and valley/ridge height today's, so the site is
// rebuilt exactly as before.
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace HouseCalderTerrain
{
bool Enabled();
/** Debug only: scales every relief (-CalderReliefScale=0..1, default 1), for bisecting a failure. */
float ReliefScale();

/** The walkable square the lattice covers, and its spacing (cm). */
constexpr float Half = 32000.f, Cell = 200.f;
constexpr int32 Cells = int32(2 * Half / Cell);   // 320 cells, 321 lattice lines a side

/** Relief above today's grade from the analytic landform (cm). Exactly 0 in the frozen zones and when off. */
float Relief(float X, float Y);
/** The lattice's triangle surface (each cell split along its (I,J)-(I+1,J+1) diagonal, as the mesh is): what the ground
 *  really is. Outside the square: the analytic Relief. Exactly 0.0f wherever the cell's three corners are 0. */
float ReliefLattice(float X, float Y);
/** Lowest / highest lattice relief over the cells a box touches; Flat = both 0 (a primitive there is built as today). */
float ReliefMin(const FBox2D& Box);
float ReliefMax(const FBox2D& Box);
bool ReliefFlat(const FBox2D& Box);
/** A hash of the lattice (CALDER_TERRAIN hash=): every machine must log the same. */
uint32 Hash();

/** Inside a frozen zone (ground, geometry, collision and look stay as today). */
bool InFrozen(float X, float Y);
/** The box lies wholly inside one frozen rectangle (edges included). */
bool BoxFrozen(const FBox2D& Box);
const TArray<FBox2D>& FrozenRects();

/** What is laid on the ground at a spot, and the height of its top over the grade datum (GroundHeight). */
enum class EPave : uint8 { Lawn, Road, Walk, Apron };
float PaveOffset(EPave Kind);
/** Ground height + the paving offset of Kind (when the caller knows the layer). */
float SurfaceZ(float X, float Y, EPave Kind);
/** Ground height + the paving that is there (HouseCampusLayout::PaveKindAt). */
float SurfaceZ(float X, float Y);

/** A plan building's level pad: how far it stands above today's (-60) grade. 0 when off. */
float PadDZ(int32 Seed);
/** The clock tower's knoll: how far the tower, its plaza and its inside stand above today's. 0 when off. */
float TowerLift();
/** Beyond the walkable square: the outer valley and the town (today -60), and the north ridges' base (today -362). */
float ValleyZ(float X, float Y);
float RidgeBase(float X, float Y);

/** Every walkable terrain tile's collision is cooked (true at once while the terrain is off). Reviews and the multiplayer
 *  client wait for it before they move a pawn. */
bool CollisionReady(const UWorld* World);
void SetCollisionReady(const UWorld* World, bool bReady);

/** Log the lattice's stats and hash once (CALDER_TERRAIN ...). */
void LogSummary();
}
