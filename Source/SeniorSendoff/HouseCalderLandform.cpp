// The baked Calder landform (see HouseCalderLandform.h).
#include "HouseCalderLandform.h"
#include "HouseCalderFarLand.h"
#include "HouseCampusLayout.h"
#include "HouseExteriorSiteLayout.h"

#include "HouseCalderLandformData.inl"

namespace HouseCalderLandformImpl
{
constexpr int32 Cells = HouseCalderLandformData::Lines - 1;

const TMap<int32, TArray<TArray<FVector>>>& Pieces()
{
    static const TMap<int32, TArray<TArray<FVector>>> Map = []
    {
        TMap<int32, TArray<TArray<FVector>>> Out;
        for (int32 C = 0; C < HouseCalderLandformData::NumWallCells; ++C)
        {
            const HouseCalderLandformData::FCellRef& Ref = HouseCalderLandformData::WallCells[C];
            TArray<TArray<FVector>>& Polys = Out.Add(Ref.Cell);
            int32 At = Ref.First;
            for (int32 P = 0; P < Ref.Pieces; ++P)
            {
                const int32 Count = int32(HouseCalderLandformData::PieceVerts[At++].X);
                TArray<FVector>& Poly = Polys.AddDefaulted_GetRef();
                for (int32 K = 0; K < Count; ++K, ++At)
                {
                    const HouseCalderLandformData::FPieceVert& V = HouseCalderLandformData::PieceVerts[At];
                    Poly.Add(FVector(V.X, V.Y, V.Z));
                }
            }
        }
        return Out;
    }();
    return Map;
}
}

float HouseCalderLandform::LatticeRelief(int32 I, int32 J)
{
    constexpr int32 Lines = HouseCalderLandformData::Lines;
    if (I < 0 || J < 0 || I >= Lines || J >= Lines) return 0.f;
    return float(HouseCalderLandformData::Lattice[J * Lines + I]) / 100.f;
}

const TArray<TArray<FVector>>* HouseCalderLandform::WallPieces(int32 I, int32 J)
{
    if (I < 0 || J < 0 || I >= HouseCalderLandformImpl::Cells || J >= HouseCalderLandformImpl::Cells) return nullptr;
    return HouseCalderLandformImpl::Pieces().Find(J * HouseCalderLandformImpl::Cells + I);
}

const TArray<FBox2D>& HouseCalderLandform::FrozenRects()
{
    static const TArray<FBox2D> Rects = []
    {
        TArray<FBox2D> Out;
        for (int32 K = 0; K < HouseCalderLandformData::NumFrozen; ++K)
        {
            const float* R = HouseCalderLandformData::Frozen[K];
            Out.Add(FBox2D(FVector2D(R[0], R[2]), FVector2D(R[1], R[3])));
        }
        return Out;
    }();
    return Rects;
}

double HouseCalderLandform::Relief(double X, double Y)
{
    // Inside the square the lattice is the authority (HouseCalderTerrain never asks here). Beyond it: the outer land
    // (HouseCalderFarLand, landform.py far()) over today's grade there (-60 south of Y 0, -362 beyond Y 1600, the bank
    // between: exactly the Grade() that HouseExteriorSite::GroundHeight adds this to, so GroundHeight = the outer land).
    if (FMath::Abs(X) <= HouseCalderFarLand::World && FMath::Abs(Y) <= HouseCalderFarLand::World) return 0.0;
    return HouseCalderFarLand::Ground(X, Y) - double(HouseExteriorSite::BaseGrade(float(X), float(Y)));
}

double HouseCalderLandform::PadDZ(int32 Seed)
{
    if (!HouseCampusLayout::Blocks().IsValidIndex(Seed)) return 0.0;
    const TCHAR* Id = HouseCampusLayout::Blocks()[Seed].Id;
    for (int32 K = 0; K < HouseCalderLandformData::NumPads; ++K)
        if (FCString::Strcmp(HouseCalderLandformData::Pads[K].Id, Id) == 0) return HouseCalderLandformData::Pads[K].DZ;
    return 0.0;
}

bool HouseCalderLandform::ApronDepth(const TCHAR* Id, TCHAR Face, float& OutDepth)
{
    for (int32 K = 0; K < HouseCalderLandformData::NumAprons; ++K)
    {
        const HouseCalderLandformData::FApron& A = HouseCalderLandformData::Aprons[K];
        if (A.Face == Face && FCString::Strcmp(A.Id, Id) == 0) { OutDepth = A.Depth; return true; }
    }
    return false;
}

double HouseCalderLandform::TowerLift() { return HouseCalderLandformData::TowerLift; }
// Where the town stands: the outer land's ground at (X, Y) (the valley floor in the town, the near ridge's face above the
// river, the water tower's knob); inside the square, the walkable ground.
double HouseCalderLandform::Valley(double X, double Y) { return HouseCalderFarLand::Ground(X, Y); }
// The north ridge sheets' base: the foothill top along the north seam at X (landform.py north_base(); Y is not used).
double HouseCalderLandform::RidgeBase(double X, double) { return HouseCalderFarLand::NorthBase(X); }

const TArray<HouseCalderLandform::FWall>& HouseCalderLandform::Walls()
{
    static const TArray<FWall> Out = []
    {
        TArray<FWall> W;
        for (int32 K = 0; K < HouseCalderLandformData::NumWalls; ++K)
        {
            const HouseCalderLandformData::FWall& D = HouseCalderLandformData::Walls[K];
            W.Add({ D.Name, EWallKind(FMath::Clamp(D.Kind, 0, int32(EWallKind::RoadAbutment))), FVector2D(D.AX, D.AY), FVector2D(D.BX, D.BY) });
        }
        return W;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FStair>& HouseCalderLandform::Stairs()
{
    static const TArray<FStair> Out = []
    {
        TArray<FStair> S;
        for (int32 K = 0; K < HouseCalderLandformData::NumStairs; ++K)
        {
            const HouseCalderLandformData::FStair& D = HouseCalderLandformData::Stairs[K];
            if (D.HZ - D.FZ < 10.f) continue;   // a steep run the checks found that rises less than one riser: no stair
            S.Add({ D.Name, FVector2D(D.HX, D.HY), FVector2D(D.FX, D.FY), D.HZ, D.FZ, D.Width });
        }
        return S;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FRamp>& HouseCalderLandform::Ramps()
{
    static const TArray<FRamp> Out = []
    {
        TArray<FRamp> R;
        for (int32 K = 0; K < HouseCalderLandformData::NumRamps; ++K)
        {
            const HouseCalderLandformData::FRamp& D = HouseCalderLandformData::Ramps[K];
            R.Add({ D.Name, FVector2D(D.AX, D.AY), FVector2D(D.BX, D.BY), D.Z0, D.Z1 });
        }
        return R;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FReserve>& HouseCalderLandform::Reserves()
{
    static const TArray<FReserve> Out = []
    {
        TArray<FReserve> R;
        for (int32 K = 0; K < HouseCalderLandformData::NumReserves; ++K)
        {
            const HouseCalderLandformData::FReserve& D = HouseCalderLandformData::Reserves[K];
            R.Add({ D.Name, FVector2D(D.X, D.Y), FVector2D(D.SX, D.SY), D.Z });
        }
        return R;
    }();
    return Out;
}

bool HouseCalderLandform::TreeHidden(const FVector2D& At)
{
    for (int32 K = 0; K < HouseCalderLandformData::NumHiddenTrees; ++K)
        if (FMath::Abs(HouseCalderLandformData::HiddenTrees[K][0] - At.X) < 1.5f && FMath::Abs(HouseCalderLandformData::HiddenTrees[K][1] - At.Y) < 1.5f) return true;
    return false;
}

// ---- The builders' data (landform.py exports(); see HouseCalderLandform.h)
namespace HouseCalderLandformImpl
{
template <int32 N>
TArray<FVector2D> Points2(const float (&Pts)[N][2])
{
    TArray<FVector2D> Out;
    for (int32 K = 0; K < N; ++K) Out.Add(FVector2D(Pts[K][0], Pts[K][1]));
    return Out;
}

template <int32 N>
TArray<HouseCalderLandform::FWaterPoint> WaterPoints(const HouseCalderLandformData::FWaterPt (&Pts)[N])
{
    TArray<HouseCalderLandform::FWaterPoint> Out;
    for (int32 K = 0; K < N; ++K) Out.Add({ FVector2D(Pts[K].X, Pts[K].Y), Pts[K].WaterZ, Pts[K].HalfWidth, Pts[K].Depth, Pts[K].S });
    return Out;
}

HouseCalderLandform::FWaterLedge Ledge(const HouseCalderLandformData::FLedge& D)
{
    return { D.Name, FVector2D(D.AX, D.AY), FVector2D(D.BX, D.BY), D.TopZ, D.BottomZ, FVector2D(D.WallAX, D.WallAY), FVector2D(D.WallBX, D.WallBY), D.CurtainY, FVector2D(D.FlowX, D.FlowY) };
}

TArray<HouseCalderLandform::FRoadsideRun> RoadsideRuns(int32 Kind)
{
    TArray<HouseCalderLandform::FRoadsideRun> Out;
    for (int32 K = 0; K < HouseCalderLandformData::NumRoadside; ++K)
    {
        const HouseCalderLandformData::FRoadsideRef& D = HouseCalderLandformData::Roadside[K];
        if (D.Kind != Kind) continue;
        HouseCalderLandform::FRoadsideRun& R = Out.AddDefaulted_GetRef();
        R.Name = D.Name;
        R.Kind = HouseCalderLandform::ERoadsideRun(Kind);
        R.Side = D.Side;
        R.Value = D.Value;
        R.bAtWall = (D.Flags & 1) != 0;
        R.bWalk = (D.Flags & 2) != 0;
        for (int32 P = D.First; P < D.First + D.Count; ++P) R.Points.Add(FVector2D(HouseCalderLandformData::RoadsidePts[P][0], HouseCalderLandformData::RoadsidePts[P][1]));
    }
    return Out;
}

TArray<HouseCalderLandform::FSpot> Spots(const HouseCalderLandformData::FSpot* D, int32 Num)
{
    TArray<HouseCalderLandform::FSpot> Out;
    for (int32 K = 0; K < Num; ++K) Out.Add({ D[K].Name, FVector(D[K].X, D[K].Y, D[K].Z), D[K].Yaw, D[K].GroundZ });
    return Out;
}
}

const TArray<FVector2D>& HouseCalderLandform::PondOutline()
{
    static const TArray<FVector2D> Out = HouseCalderLandformImpl::Points2(HouseCalderLandformData::PondOutline);
    return Out;
}

float HouseCalderLandform::PondWaterZ() { return HouseCalderLandformData::PondWaterZ; }

const TArray<HouseCalderLandform::FWaterPoint>& HouseCalderLandform::CreekPoints()
{
    static const TArray<FWaterPoint> Out = HouseCalderLandformImpl::WaterPoints(HouseCalderLandformData::CreekPts);
    return Out;
}

const TArray<HouseCalderLandform::FWaterPoint>& HouseCalderLandform::GlenPoints()
{
    static const TArray<FWaterPoint> Out = HouseCalderLandformImpl::WaterPoints(HouseCalderLandformData::GlenPts);
    return Out;
}

const HouseCalderLandform::FWaterLedge& HouseCalderLandform::CalderFalls()
{
    static const FWaterLedge Out = HouseCalderLandformImpl::Ledge(HouseCalderLandformData::Ledges[0]);
    return Out;
}

const HouseCalderLandform::FWaterLedge& HouseCalderLandform::Cascade()
{
    static const FWaterLedge Out = HouseCalderLandformImpl::Ledge(HouseCalderLandformData::Ledges[1]);
    return Out;
}

const HouseCalderLandform::FPlungePool& HouseCalderLandform::PlungePool()
{
    static const FPlungePool Out = []
    {
        const HouseCalderLandformData::FPool& D = HouseCalderLandformData::PlungePool;
        return FPlungePool{ FVector2D(D.X, D.Y), D.Radius, D.WaterZ, D.Depth, D.DeepFromY };
    }();
    return Out;
}

const HouseCalderLandform::FGlenHeadwall& HouseCalderLandform::GlenHeadwall()
{
    static const FGlenHeadwall Out = []
    {
        const HouseCalderLandformData::FHeadwall& D = HouseCalderLandformData::Headwall;
        return FGlenHeadwall{ FVector2D(D.X, D.Y), FVector2D(D.FlowX, D.FlowY), D.WaterZ, D.InvertZ, D.WaterHalf, D.FloorHalf, D.WingWX, D.WingEX, D.GrateWidth };
    }();
    return Out;
}

const TArray<HouseCalderLandform::FWaterCrossing>& HouseCalderLandform::WaterCrossings()
{
    static const TArray<FWaterCrossing> Out = []
    {
        TArray<FWaterCrossing> C;
        for (const HouseCalderLandformData::FCrossing& D : HouseCalderLandformData::Crossings)
            C.Add({ D.Name, EWaterCrossing(FMath::Clamp(D.Kind, 0, int32(EWaterCrossing::RoadBridge))), FVector2D(D.X, D.Y), FVector2D(D.AxisX, D.AxisY),
                    D.Back, D.On, D.Width, D.DeckZ, D.WaterZ, D.InvertZ, D.EndZ0, D.EndZ1, D.Pipe });
        return C;
    }();
    return Out;
}

const HouseCalderLandform::FPondDam& HouseCalderLandform::PondDam()
{
    static const FPondDam Out = []
    {
        FPondDam D;
        D.Crest = HouseCalderLandformImpl::Points2(HouseCalderLandformData::DamCrest);
        D.CrestZ = HouseCalderLandformData::DamCrestZ;
        D.CrestWidth = HouseCalderLandformData::DamCrestWidth;
        D.NotchWidth = HouseCalderLandformData::DamNotchWidth;
        D.OutletZ = HouseCalderLandformData::DamOutletZ;
        D.Race = HouseCalderLandformImpl::Points2(HouseCalderLandformData::RacePts);
        D.RaceWaterZ = HouseCalderLandformData::RaceWaterZ;
        D.RaceHalfWidth = HouseCalderLandformData::RaceHalfWidth;
        D.RaceFloorHalfWidth = HouseCalderLandformData::RaceFloorHalfWidth;
        return D;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FSteppingStone>& HouseCalderLandform::SteppingStones()
{
    static const TArray<FSteppingStone> Out = []
    {
        TArray<FSteppingStone> S;
        for (const HouseCalderLandformData::FStone& D : HouseCalderLandformData::Stones)
            S.Add({ D.Crossing, FVector2D(D.X, D.Y), D.TopZ, D.BedZ, D.Radius, D.Yaw });
        return S;
    }();
    return Out;
}

const TArray<FString>& HouseCalderLandform::SteppingStoneCrossings()
{
    static const TArray<FString> Out = []
    {
        TArray<FString> N;
        for (const TCHAR* const Name : HouseCalderLandformData::StoneCrossings) N.Add(Name);
        return N;
    }();
    return Out;
}

const HouseCalderLandform::FPondDock& HouseCalderLandform::PondDock()
{
    static const FPondDock Out = []
    {
        const HouseCalderLandformData::FDock& D = HouseCalderLandformData::Dock;
        return FPondDock{ FVector2D(D.LandX, D.LandY), FVector2D(D.EdgeX, D.EdgeY), FVector2D(D.TipX, D.TipY), FVector2D(D.AxisX, D.AxisY),
                          D.Width, D.DeckZ, D.WaterZ, D.TipDepth, D.RailFromEdge };
    }();
    return Out;
}

const HouseCalderLandform::FRockHouse& HouseCalderLandform::RockHouse()
{
    static const FRockHouse Out = []
    {
        const HouseCalderLandformData::FRockHouse& D = HouseCalderLandformData::RockHouse;
        return FRockHouse{ D.X0, D.X1, D.FaceY, D.OverhangY, D.ShelfY, D.WetY, D.FloorZ, D.PoolZ, FVector2D(D.PathAX, D.PathAY),
                           FVector2D(D.PathBX, D.PathBY), FVector2D(D.RailAX, D.RailAY), FVector2D(D.RailBX, D.RailBY) };
    }();
    return Out;
}

const TArray<HouseCalderLandform::FLandformPath>& HouseCalderLandform::LandformPaths()
{
    static const TArray<FLandformPath> Out = []
    {
        TArray<FLandformPath> P;
        for (int32 K = 0; K < HouseCalderLandformData::NumPaths; ++K)
        {
            const HouseCalderLandformData::FPathRef& D = HouseCalderLandformData::Paths[K];
            FLandformPath& Path = P.AddDefaulted_GetRef();
            Path.Name = D.Name;
            Path.Width = D.Width;
            Path.bClosed = D.bClosed;
            for (int32 V = D.First; V < D.First + D.Count; ++V)
            {
                const HouseCalderLandformData::FPathVert& PV = HouseCalderLandformData::PathVerts[V];
                Path.Points.Add(FVector2D(PV.X, PV.Y));
                Path.Stretches.Add(EPathStretch(FMath::Clamp(PV.Stretch, 0, int32(EPathStretch::Causeway))));
            }
        }
        return P;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FRoad>& HouseCalderLandform::Roads()
{
    static const TArray<FRoad> Out = []
    {
        TArray<FRoad> Result;
        for (int32 K = 0; K < HouseCalderLandformData::NumRoads; ++K)
        {
            const HouseCalderLandformData::FRoadRef& Ref = HouseCalderLandformData::Roads[K];
            FRoad& Road = Result.AddDefaulted_GetRef();
            Road.Name = Ref.Name;
            Road.Width = Ref.Width;
            Road.Mode = Ref.Mode;
            for (int32 V = Ref.FirstPoint; V < Ref.FirstPoint + Ref.PointCount; ++V)
                Road.Points.Add(FVector2D(HouseCalderLandformData::RoadPoints[V][0], HouseCalderLandformData::RoadPoints[V][1]));
            for (int32 Q = Ref.FirstPoly; Q < Ref.FirstPoly + Ref.PolyCount; ++Q)
            {
                const HouseCalderLandformData::FRoadPoly& PolyRef = HouseCalderLandformData::RoadPolys[Q];
                TArray<FVector2D>& Poly = Road.Polygons.AddDefaulted_GetRef();
                for (int32 V = PolyRef.First; V < PolyRef.First + PolyRef.Count; ++V)
                    Poly.Add(FVector2D(HouseCalderLandformData::RoadPolyPoints[V][0], HouseCalderLandformData::RoadPolyPoints[V][1]));
            }
        }
        return Result;
    }();
    return Out;
}

const HouseCalderLandform::FCrowLedge& HouseCalderLandform::CrowLedge()
{
    static const FCrowLedge Out = { HouseCalderLandformImpl::Points2(HouseCalderLandformData::CrowLedgeOutline), HouseCalderLandformData::CrowLedgeZ };
    return Out;
}

const TArray<HouseCalderLandform::FRoadsideRun>& HouseCalderLandform::Guardrails()
{
    static const TArray<FRoadsideRun> Out = HouseCalderLandformImpl::RoadsideRuns(int32(ERoadsideRun::Guardrail));
    return Out;
}

const TArray<HouseCalderLandform::FRoadsideRun>& HouseCalderLandform::Ditches()
{
    static const TArray<FRoadsideRun> Out = HouseCalderLandformImpl::RoadsideRuns(int32(ERoadsideRun::Ditch));
    return Out;
}

const TArray<HouseCalderLandform::FRoadsideRun>& HouseCalderLandform::NeedsRail()
{
    static const TArray<FRoadsideRun> Out = HouseCalderLandformImpl::RoadsideRuns(int32(ERoadsideRun::NeedsRail));
    return Out;
}

const TArray<HouseCalderLandform::FCatchBasin>& HouseCalderLandform::CatchBasins()
{
    static const TArray<FCatchBasin> Out = []
    {
        TArray<FCatchBasin> B;
        for (int32 K = 0; K < HouseCalderLandformData::NumBasins; ++K)
        {
            const HouseCalderLandformData::FBasin& D = HouseCalderLandformData::Basins[K];
            B.Add({ D.Name, FVector(D.X, D.Y, D.Z), D.bLot });
        }
        return B;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FDitchCulvert>& HouseCalderLandform::DitchCulverts()
{
    static const TArray<FDitchCulvert> Out = []
    {
        TArray<FDitchCulvert> C;
        for (int32 K = 0; K < HouseCalderLandformData::NumCulverts; ++K)
        {
            const HouseCalderLandformData::FCulvertX& D = HouseCalderLandformData::Culverts[K];
            C.Add({ D.Ditch, D.Under, FVector2D(D.X, D.Y), FVector2D(D.AxisX, D.AxisY), D.Length, D.RoadZ, D.Side });
        }
        return C;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FEdgeClosure>& HouseCalderLandform::EdgeClosures()
{
    static const TArray<FEdgeClosure> Out = []
    {
        TArray<FEdgeClosure> C;
        for (int32 K = 0; K < HouseCalderLandformData::NumClosures; ++K)
        {
            const HouseCalderLandformData::FClosureRef& D = HouseCalderLandformData::Closures[K];
            FEdgeClosure& E = C.AddDefaulted_GetRef();
            E.Edge = D.Edge;
            E.Kind = EEdgeClosure(FMath::Clamp(D.Kind, 0, int32(EEdgeClosure::BoulderChoke)));
            for (int32 P = D.First; P < D.First + D.Count; ++P) E.Points.Add(FVector2D(HouseCalderLandformData::ClosurePts[P][0], HouseCalderLandformData::ClosurePts[P][1]));
        }
        return C;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FHedgeSegment>& HouseCalderLandform::HedgeSegments()
{
    static const TArray<FHedgeSegment> Out = []
    {
        TArray<FHedgeSegment> H;
        for (int32 K = 0; K < HouseCalderLandformData::NumHedgeSegs; ++K)
        {
            const HouseCalderLandformData::FHedgeSeg& D = HouseCalderLandformData::HedgeSegs[K];
            H.Add({ D.Hedge, FVector2D(D.AX, D.AY), FVector2D(D.BX, D.BY), D.BottomZ, D.TopZ, D.bFrozen });
        }
        return H;
    }();
    return Out;
}

const TArray<FString>& HouseCalderLandform::HedgeNames()
{
    static const TArray<FString> Out = []
    {
        TArray<FString> N;
        for (const TCHAR* const Name : HouseCalderLandformData::HedgeNames) N.Add(Name);
        return N;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FHedgeGap>& HouseCalderLandform::HedgeGaps()
{
    static const TArray<FHedgeGap> Out = []
    {
        TArray<FHedgeGap> G;
        for (int32 K = 0; K < HouseCalderLandformData::NumHedgeGaps; ++K)
        {
            const HouseCalderLandformData::FHedgeGap& D = HouseCalderLandformData::HedgeGaps[K];
            G.Add({ D.Hedge, D.From, D.To, D.Why });
        }
        return G;
    }();
    return Out;
}

const TArray<HouseCalderLandform::FSpot>& HouseCalderLandform::Overlooks()
{
    static const TArray<FSpot> Out = HouseCalderLandformImpl::Spots(HouseCalderLandformData::Overlooks, int32(UE_ARRAY_COUNT(HouseCalderLandformData::Overlooks)));
    return Out;
}

const TArray<HouseCalderLandform::FSpot>& HouseCalderLandform::EyeSpots()
{
    static const TArray<FSpot> Out = HouseCalderLandformImpl::Spots(HouseCalderLandformData::EyeSpots, int32(UE_ARRAY_COUNT(HouseCalderLandformData::EyeSpots)));
    return Out;
}

float HouseCalderLandform::LowestWalkableZ() { return HouseCalderLandformData::LowestWalkableZ; }
FVector2D HouseCalderLandform::LowestWalkableAt() { return FVector2D(HouseCalderLandformData::LowestWalkableX, HouseCalderLandformData::LowestWalkableY); }
float HouseCalderLandform::FallLimitZ() { return HouseCalderLandformData::FallLimitZ; }
float HouseCalderLandform::MinWalkableZ() { return HouseCalderLandformData::MinWalkableZ; }
