// Draping flat things on the Calder terrain (SourceArt/CalderLandscape, step 1c): a footprint (a walk, slab, disc, ramp or
// painted strip) is split along the relief lattice's lines and every cell's diagonal, and in a cell a declared wall splits,
// along that cell's pieces (the ground's own triangles there), so each piece lies over one triangle of the ground where it is
// a plane; its top then follows the ground exactly, stepping where the ground steps at a wall, and slabs get side walls down
// to the ground along an outline split at the same lines and at the walls. The built stairs' and ramps' footprints
// (HouseCalderFeatures::Footprints) are cut out of every sheet and slab: the paving gives way where a stair or ramp takes over. Builders call this only where the relief under them is not flat
// (HouseCalderTerrain::ReliefFlat), and keep today's flat primitive everywhere else.
#pragma once

#include "CoreMinimal.h"

struct FHallMesh;
struct FHallUV;

namespace HouseCalderDrape
{
/** A convex outline (either winding) cut along the lattice lines and diagonals; each piece is convex. */
void Pieces(const TArray<FVector2D>& Outline, TArray<TArray<FVector2D>>& Out);
/** The outline's points with every lattice line and diagonal crossing inserted (closed: the first point is not repeated). */
TArray<FVector2D> Densify(const TArray<FVector2D>& Outline);
/** A sheet over a convex outline at Z(X, Y), facing up (paint, lawn overlays, the tops of slabs). */
void Sheet(FHallMesh& M, FName Key, const TArray<FVector2D>& Outline, TFunctionRef<float(float, float)> Z, const FHallUV& UV);
/** A slab: its top at TopZ(X, Y) and side walls round the outline down to BottomZ(X, Y). */
void Slab(FHallMesh& M, FName Key, const TArray<FVector2D>& Outline, TFunctionRef<float(float, float)> TopZ, TFunctionRef<float(float, float)> BottomZ, const FHallUV& UV, bool bSides = true);
/** Outlines the layout uses: an axis-aligned rectangle; a straight run from A to B, Width wide; an annulus sector (degrees,
 *  anticlockwise from +X, split into Seg pieces each drawn as its own convex quad: call per piece). */
TArray<FVector2D> RectOutline(const FBox2D& R);
TArray<FVector2D> RunOutline(const FVector2D& A, const FVector2D& B, float Width);
TArray<FVector2D> SectorPiece(const FVector2D& Centre, float R0, float R1, float Deg0, float Deg1);
}
