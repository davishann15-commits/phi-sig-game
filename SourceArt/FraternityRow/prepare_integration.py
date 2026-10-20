"""Build reviewable integration candidates only. Does not edit runtime Source."""
from pathlib import Path
R=Path(__file__).resolve().parents[2];D=R/'SourceArt/FraternityRow/Draft';O=D/'Integrated';O.mkdir(exist_ok=True)
changes={}
def load(n):return (R/'Source/SeniorSendoff'/n).read_text()
def replace(s,a,b):
    assert a in s,'Missing expected source fragment: '+a[:90]
    return s.replace(a,b,1)
def inc(s):return replace(s,'#include "HouseFraternityRow.h"','#include "HouseFraternityRow.h"\n#include "HouseFraternityRowYards.h"')
s=inc(load('HouseExterior.cpp'))
if 'TEXT("detail_row_road_wide")' not in s:
    a=s.index('const FExView ExViews[]');b=s.index('\n};',a);s=s[:b]+'\n'+(D/'row_views.inc').read_text()+s[b:]
s=replace(s,'    HouseFraternityRow::CreateLook(Look);','    HouseFraternityRow::CreateLook(Look);\n    HouseFraternityRowYards::CreateLook(Look);')
s=replace(s,'        Slab(Asphalt, -7200, -3700, -2500, -300, GF - 40, GF + 2, B.AsphaltUV);\n        Slab(Asphalt, -5800, -5200, StreetY(-2990), -2500, GF - 40, GF + 2, B.AsphaltUV);','''        if(!HouseFraternityRow::Enabled())
        {
            Slab(Asphalt, -7200, -3700, -2500, -300, GF - 40, GF + 2, B.AsphaltUV);
            Slab(Asphalt, -5800, -5200, StreetY(-2990), -2500, GF - 40, GF + 2, B.AsphaltUV);
        }''')
s=replace(s,'    HouseParking::Build(B.ParkingSolid,B.ParkingDetail);','''    if(!HouseFraternityRow::Enabled())HouseParking::Build(B.ParkingSolid,B.ParkingDetail);''')
s=replace(s,'    HouseLotDetail::Build(LotWear, LotProps);','    if(!HouseFraternityRow::Enabled())HouseLotDetail::Build(LotWear, LotProps);')
needle='    TArray<FTransform> ParkingPlants;HouseParking::Landscaping(Site.Bush,ParkingPlants);'
new=needle+'''
    FHallMesh RowSolid,RowDetail,RowGlow,YardSolid,YardDecor,YardGlow;
    if(HouseFraternityRow::Enabled())
    {
        // Keep all base generation/vehicle RNG draws and original NoGo exclusions. Trees were recorded above.
        auto InOldLot=[](const FTransform& T){const FVector P=T.GetLocation();return P.X>HouseParking::West&&P.X<HouseParking::East&&P.Y>HouseParking::South&&P.Y<HouseParking::North;};
        Site.CarsA.RemoveAll(InOldLot);Site.CarsB.RemoveAll(InOldLot);Site.CarsC.RemoveAll(InOldLot);
        ParkingPlants.Reset(); // Existing-bush clearance filtering in Landscaping() is intentionally retained.
        for(const FVector P:{FVector(-7160,-2600,0),FVector(-3740,-2530,180)})
        {
            Site.LotPoles.RemoveAll([&](const FTransform& T){return FVector::DistSquared2D(T.GetLocation(),FVector(P.X,P.Y,0))<1;});
            const FVector Head=FVector(P.X,P.Y,HouseExteriorSite::GroundHeight(P.X,P.Y))+FRotator(0,P.Z,0).RotateVector(HouseStreetScene::LensOffset());
            B.LightSpecs.RemoveAll([&](const FExBuilder::FLightSpec& L){return FVector::DistSquared(L.Pos,Head)<1;});
        }
        HouseFraternityRow::BuildStructures(RowSolid,RowDetail,RowGlow);
        HouseFraternityRowYards::Build(YardSolid,YardDecor,YardGlow);
        // Retain the exact pedestrian connector independently of the removed parking builder.
        RowSolid.BoxMinMax(TEXT("EX_StreetConcrete"),{-4490,-285,-85},{-3550,-105,-57},FHallUV::World(180));
        if(HouseFraternityRow::LightsEnabled())for(int32 I=2;I<5;++I)
        {
            const auto& H=HouseFraternityRow::Houses()[I];
            const FVector Porch=HouseFraternityRow::PorchLight(H);
            const FVector Yard=HouseFraternityRow::YardLight(H);
            B.LightSpecs.Add({Porch,FLinearColor(1.f,.65f,.32f),650.f,700.f,5500.f});
            B.LightSpecs.Add({Yard,FLinearColor(1.f,.72f,.43f),500.f,800.f,5500.f});
        }
    }
'''
s=replace(s,needle,new)
s=replace(s,'''        for (const FTransform& T : Xforms) H->AddInstance(T, true);
        Instanced.Add(H);
        Instances += Xforms.Num();
        Triangles += M.NumTriangles() * Xforms.Num();''','''        // Keep every scatter draw and variant assignment; clear only rendered grass/shrubs overlapping new solids.
        const bool RowVegetation=HouseFraternityRow::Enabled()&&(FString(Name).StartsWith(TEXT("Grass"))||FString(Name).StartsWith(TEXT("Bush")));
        int32 Added=0,RowCleared=0;
        for (const FTransform& T : Xforms)
        {
            if(RowVegetation)
            {
                const FBox WorldBounds=SM->GetBoundingBox().TransformBy(T);
                if(HouseFraternityRowYards::ExistingVegetationBlocked(FBox2D(FVector2D(WorldBounds.Min),FVector2D(WorldBounds.Max))))
                {++RowCleared;continue;}
            }
            H->AddInstance(T,true);++Added;
        }
        if(RowCleared>0)UE_LOG(LogTemp,Display,TEXT("FRATERNITY_ROW_VEGETATION batch=%s removedOverlappingActualMeshBounds=%d retained=%d"),Name,RowCleared,Added);
        Instanced.Add(H);
        Instances += Added;
        Triangles += M.NumTriangles() * Added;''')
s=replace(s,'    Local(B.FraternityRow,TEXT("ExteriorFraternityRow"),false,false);','''    Local(B.FraternityRow,TEXT("ExteriorFraternityRow"),false,false);
    Local(RowSolid,TEXT("FraternityRowStructure"),true,true);
    Local(RowDetail,TEXT("FraternityRowDetail"),false,false);
    Make(RowGlow,TEXT("FraternityRowGlow"),false,false);
    Local(YardSolid,TEXT("FraternityRowYardsSolid"),true,false);
    Local(YardDecor,TEXT("FraternityRowYardsDecor"),false,false);
    Make(YardGlow,TEXT("FraternityRowYardsGlow"),false,false);
    if(HouseFraternityRow::Enabled())HouseFraternityRowYards::AttachInstances(this,Look);''')
s=replace(s,'        for (float X : { -5045.f, -5705.f, -6365.f })','        if(!HouseFraternityRow::Enabled())for (float X : { -5045.f, -5705.f, -6365.f })')
s=replace(s,'if(FString(Name).StartsWith(TEXT("ExteriorFar"))','if(FString(Name).StartsWith(TEXT("FraternityRowDetail"))||FString(Name).StartsWith(TEXT("FraternityRowGlow"))||FString(Name).StartsWith(TEXT("FraternityRowYards"))||FString(Name).StartsWith(TEXT("ExteriorFar"))')
if 'detail_row_R5_lot_front' not in s:
    a=s.index('const FExView ExViews[]');b=s.index('\n};',a);s=s[:b]+'\n'+(D/'row_lot_views.inc').read_text()+s[b:]
changes['HouseExterior.cpp']=s
s=load('HouseCampusInteriorsRooms.h')
s=replace(s,'    bool bLightsInsideOnly = false;','''    bool bLightsInsideOnly = false;
    /** Anchor's height above its floor when classifying pooled lights. Older definitions retain300cm. */
    float LightAnchorFloorOffset = 300.f;
    /** Human interiors opt in; wandering wildlife outside does not keep these rooms loaded. */
    bool bPlayerProximityOnly = false;''')
s=replace(s,'void GenericDefs(TArray<FInteriorDef>& Out);','void GenericDefs(TArray<FInteriorDef>& Out);\nvoid RowDefs(TArray<FInteriorDef>& Out);')
changes['HouseCampusInteriorsRooms.h']=s
s=load('HouseCampusInteriors.cpp')
s=replace(s,'#include "HouseCampusInteriors.h"','#include "HouseCampusInteriors.h"\n#include "HouseFraternityRow.h"')
s=replace(s,'        GenericDefs(Defs);   // Tier B and C (2d)','        GenericDefs(Defs);   // Tier B and C (2d)\n        RowDefs(Defs);       // Append only: preserve all existing standalone seeds.')
s=s.replace('P.Z + HouseCampusLayout::BlockDZ(B.Seed) - 300.f','P.Z + HouseCampusLayout::BlockDZ(B.Seed) - Def->LightAnchorFloorOffset')
s=s.replace('P.Z + HouseCampusLayout::BlockDZ(Outside->Seed) - 300.f','P.Z + HouseCampusLayout::BlockDZ(Outside->Seed) - Def->LightAnchorFloorOffset')
s=replace(s,'    TArray<FVector> Interest;','    TArray<FVector> Interest, PlayerInterest;')
s=replace(s,'Interest.Add(View); }','Interest.Add(View); PlayerInterest.Add(View); }')
s=replace(s,'    for (TActorIterator<APawn> It(W); It; ++It) Interest.Add(It->GetActorLocation());','''    for (TActorIterator<APawn> It(W); It; ++It)
    {
        Interest.Add(It->GetActorLocation());
        if(It->IsPlayerControlled()||It->GetPlayerState()!=nullptr)PlayerInterest.Add(It->GetActorLocation());
    }''')
s=replace(s,'            for (const FVector& P : Interest) Near =','''            const auto* Definition=HouseCampusInteriorsImpl::InteriorFor(Seed);
            const TArray<FVector>& Relevant=Definition&&Definition->bPlayerProximityOnly?PlayerInterest:Interest;
            for (const FVector& P : Relevant) Near =''')
s=replace(s,'    bool bOuter = DoorGroup.ToString().EndsWith(TEXT("_Main"));','''    const FString GroupName=DoorGroup.ToString();
    bool bOuter = GroupName.EndsWith(TEXT("_Main"));
    if(HouseFraternityRow::Enabled())for(const auto& H:HouseFraternityRow::Houses())
        if(GroupName==FString::Printf(TEXT("Campus_%s_Rear"),H.Id))bOuter=true;''')
changes['HouseCampusInteriors.cpp']=s
# Exactly the approved light budget change; all other thresholds remain.
s=load('HouseWoodsPass.cpp');s=replace(s,'#include "HouseWoodsPass.h"','#include "HouseWoodsPass.h"\n#include "HouseFraternityRow.h"');s=replace(s,'ExteriorLights <= 53','ExteriorLights <= HouseFraternityRow::ExteriorLightBudget()');changes['HouseWoodsPass.cpp']=s
s=load('HouseCampusLayoutReview.cpp');s='#include "HouseFraternityRow.h"\n'+s;s=replace(s,'ExteriorLights <= 53','ExteriorLights <= HouseFraternityRow::ExteriorLightBudget()');changes['HouseCampusLayoutReview.cpp']=s
s=load('HouseCalderTerrainReview.cpp');s='#include "HouseFraternityRow.h"\n'+s
s=replace(s,'constexpr int32 MaxLights = 53;','int32 MaxLights() { return HouseFraternityRow::ExteriorLightBudget(); }')
s=s.replace('S.Lights <= MaxLights,','S.Lights <= MaxLights(),').replace('S.Lights, MaxLights,','S.Lights, MaxLights(),')
changes['HouseCalderTerrainReview.cpp']=s
s=load('HouseClockTower.cpp');s=replace(s,'#include "HouseClockTower.h"','#include "HouseClockTower.h"\n#include "HouseFraternityRow.h"')
s=replace(s,'ExteriorLights <= 53','ExteriorLights <= HouseFraternityRow::ExteriorLightBudget()');changes['HouseClockTower.cpp']=s

s=load('HouseExteriorWalkReview.cpp');s='#include "HouseFraternityRow.h"\n'+s
s=replace(s,'        for(float X:{-5500.f,3750.f})if(!Surface(X,-3700,-58,TEXT("EX_Asphalt")))return;','''        if(!Surface(-5500,-3700,HouseFraternityRow::Enabled()?-60.f:-58.f,HouseFraternityRow::Enabled()?TEXT("EX_Grass"):TEXT("EX_Asphalt")))return;
        if(!Surface(3750,-3700,-58,TEXT("EX_Asphalt")))return;''')
changes['HouseExteriorWalkReview.cpp']=s
s=load('HouseParkingLotReview.cpp');s='#include "HouseFraternityRow.h"\n'+s
old='  if(CenterPoles!=0||LotCars<3||!ReservedClear||BlueVertices!=8||SymbolVertices!=8||Plants<25||Floors!=7||Curbs!=2||Paths!=4||!Safe||E->Lights>53){Finish(false,TEXT("Actual parking layout checks failed"));return;}'
new='''  if(HouseFraternityRow::Enabled())
  {
   int RemovedParts=0;for(auto* P:Meshes)if(P->GetName().StartsWith(TEXT("ExteriorParkingLot"))||P->GetName().StartsWith(TEXT("ExteriorLotProps")))++RemovedParts;
   if(CenterPoles!=0||LotCars!=0||BlueVertices!=0||SymbolVertices!=0||Plants!=0||Floors!=7||Curbs!=0||Paths!=4||!Safe||RemovedParts!=0||E->Lights!=HouseFraternityRow::ExteriorLightBudget())
   {Finish(false,TEXT("Reclaimed Row lawn must have zero parkingcars/bays/curbs/props, supported ground and all original pedestrian routes"));return;}
   Note(TEXT("ReclaimedRow westLotAbsent=1 oldWalkWaypointsUnchanged=1 mainHouseConnectorSupported=1"));
  }
  else if(CenterPoles!=0||LotCars<3||!ReservedClear||BlueVertices!=8||SymbolVertices!=8||Plants<25||Floors!=7||Curbs!=2||Paths!=4||!Safe||E->Lights>53){Finish(false,TEXT("Actual parking layout checks failed"));return;}'''
s=replace(s,old,new)
s=replace(s,'int RemovedParts=0;for(auto* P:Meshes)','''int RemovedPoles=0;for(auto* P:Parts)if(P->GetName().StartsWith(TEXT("ExteriorPoleLot")))for(int I=0;I<P->GetInstanceCount();++I){FTransform T;P->GetInstanceTransform(I,T,true);for(FVector2D XY:{FVector2D(-7160,-2600),FVector2D(-3740,-2530)})RemovedPoles+=FVector2D::Distance(FVector2D(T.GetLocation()),XY)<1;}
   int RemovedParts=0;for(auto* P:Meshes)''')
s=replace(s,'||RemovedParts!=0||E->Lights!=HouseFraternityRow::ExteriorLightBudget()','||RemovedParts!=0||RemovedPoles!=0||E->Lights!=HouseFraternityRow::ExteriorLightBudget()')
changes['HouseParkingLotReview.cpp']=s
s=load('HouseCampusInteriorsSmoke.cpp');s='#include "HouseFraternityRow.h"\n'+s
s=replace(s,'        List.ParseIntoArray(S.Ids, TEXT(","));','''        if(List.Equals(TEXT("LIB,HAR,SCI,UNI,STA,TWR,MAIN,HLTH,COM,ASH,HUM,KER,FLD,ART,MUS,ENG,PLT,ALU"))&&HouseFraternityRow::Enabled())List+=TEXT(",R1,R2,R3,R4,R5");
        List.ParseIntoArray(S.Ids, TEXT(","));''')
# New deliberately-dark house has exactly one upper light anchor; existing building minima are untouched.
s=replace(s,"MinAnchors = Tier == TEXT('A') ? 6 : Tier == TEXT('B') ? 4 : 1;","MinAnchors = Id==TEXT(\"R1\") ? 1 : Tier == TEXT('A') ? 6 : Tier == TEXT('B') ? 4 : 1;")
s=replace(s,'    if (Anchors.Num() < MinAnchors || FloorHits < Anchors.Num())','    if ((Id==TEXT("R1")&&Anchors.Num()!=1) || Anchors.Num() < MinAnchors || FloorHits < Anchors.Num())')
changes['HouseCampusInteriorsSmoke.cpp']=s
# Extend the native physical walkthrough without changing existing stops or tolerances.
s=load('HouseCampusInteriorsReview.cpp')
s=replace(s,'#include "HouseCampusInteriors.h"','#include "HouseCampusInteriors.h"\n#include "HouseFraternityRow.h"')
route=D/'row_walk.inc'
if route.exists():
    s=replace(s,'const TArray<FBuildingRoute>& Routes()',route.read_text()+'\nconst TArray<FBuildingRoute>& Routes()')
    s=replace(s,'        GenericRoutes(All);','        GenericRoutes(All);\n        RowRoutes(All);')
s=replace(s,'    auto World2 = [&Frame]', '    const bool bRow = HouseFraternityRow::Enabled() && HouseFraternityRow::Houses().ContainsByPredicate([BuildingId](const HouseFraternityRow::FHouse& H){return FCString::Strcmp(H.Id,BuildingId)==0;});\n    auto World2 = [&Frame]')
s=replace(s,'const float Indoor = -60.f + HouseCampusInteriors::BaseDZ(S.Seed);','const float Indoor = bRow ? HouseCampusInteriors::InteriorFloorZ(S.Seed,FMath::Max(0,Stop.Floor)) : -60.f + HouseCampusInteriors::BaseDZ(S.Seed);')
s=replace(s,'const bool bDid = Door->Interact(Pawn);','const bool bDid = bRow && Door->IsOpen() ? true : Door->Interact(Pawn);')
old='''        ACampusDoor* A = FindDoor(TEXT("MainA"));
        ACampusDoor* B = FindDoor(TEXT("MainB"));
        ++S.DoorsChecked;
        S.DoorsOk += A && B && A->IsOpen() && B->IsOpen() ? 1 : 0;
        Note(FString::Printf(TEXT("main doors open: A=%d B=%d"), A ? int32(A->IsOpen()) : -1, B ? int32(B->IsOpen()) : -1));'''
new='''        const CR::FStop& OpenStop=Route[S.Stop];
        const bool Main=FCString::Strcmp(OpenStop.Door,TEXT("MainA"))==0;
        ACampusDoor* A = FindDoor(OpenStop.Door);
        ACampusDoor* B = Main ? FindDoor(TEXT("MainB")) : nullptr;
        if(bRow&&FString(OpenStop.Door).EndsWith(TEXT("A")))
        { FString Pair(OpenStop.Door);Pair[Pair.Len()-1]=TEXT('B');B=FindDoor(*Pair); }
        const bool Pair=Main||(bRow&&FString(OpenStop.Door).EndsWith(TEXT("A")));
        const bool Open=A&&A->IsOpen()&&(!Pair||(B&&B->IsOpen()));
        if(Main){++S.DoorsChecked;S.DoorsOk += A&&B&&Open ? 1:0;}
        else if(!Open)++S.Problems;
        Note(FString::Printf(TEXT("doors %s open: A=%d B=%d"),OpenStop.Door,A?int32(A->IsOpen()):-1,B?int32(B->IsOpen()):-1));'''
s=replace(s,old,new)
s=replace(s,'        if (Dist < 60.f)','        if (Dist < (bRow ? 12.f : 60.f))')
s=replace(s,'if (Dist < S.Best - 25.f)','if (Dist < S.Best - (bRow ? 4.f : 25.f))')
s=replace(s,'if (Now - S.Progress > 7.0)','if (Now - S.Progress > 7.0 || (bRow && Now - S.Step > 30.0))')
changes['HouseCampusInteriorsReview.cpp']=s
for n,s in changes.items():(O/n).write_text(s)
print('Prepared shared-file drafts only:',','.join(changes))
