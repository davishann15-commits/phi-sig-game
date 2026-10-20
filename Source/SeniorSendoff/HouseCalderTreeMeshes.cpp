#include "HouseCalderTreeMeshes.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

namespace HouseCalderTreeMeshesImpl
{
int32 SectionRole(const UStaticMesh* Mesh,int32 Slot)
{
    const UMaterialInterface* Material=Mesh->GetMaterial(Slot);
    if (!Material) return INDEX_NONE;
    const FString Name=Material->GetName();
    return Name.Contains(TEXT("TwoSided"))?0:Name.Contains(TEXT("_Trunk_"))?1:2;
}
struct FFit
{
    float HeightScale=1.f,ZShift=0.f,SourceFloor=0.f,SourceHeight=1.f,CrownLift=0.f,StemScale=1.f,CrownScale=1.f;
    void VerticalMap(const FVector3f& Point,float& Height,float& Derivative) const
    {
        Height=(Point.Z-SourceFloor)*HeightScale;Derivative=HeightScale;
        if (CrownLift<=0.f) return;
        const float T=(Point.Z-SourceFloor)/SourceHeight,K=1.f-CrownLift,D=K+(1.f-K)*T;
        Height/=D;Derivative=HeightScale*K/(D*D);
    }
    float RadialInner=50.f,RadialOuter=150.f;
    // Integrate a strictly positive radial derivative. Inner stem caliper is
    // retained; large branches use the constrained crown scale without folding.
    void ScaleAt(const FVector3f& Point,float& Scale,float& RadialCoefficient,float& VerticalDerivative) const
    {
        float Height,HeightDerivative;VerticalMap(Point,Height,HeightDerivative);
        const float ZT=FMath::Clamp((Height-400.f)/400.f,0.f,1.f);
        const float Stem=FMath::Lerp(StemScale,CrownScale,ZT*ZT*(3.f-2.f*ZT));
        const float StemDerivative=(CrownScale-StemScale)*6.f*ZT*(1.f-ZT)*HeightDerivative/400.f;
        const float Radius=FMath::Sqrt(Point.X*Point.X+Point.Y*Point.Y);
        if (Radius<=RadialInner)
        { Scale=Stem;RadialCoefficient=0.f;VerticalDerivative=StemDerivative;return; }
        const float Width=RadialOuter-RadialInner,Offset=(RadialInner+RadialOuter)*.5f;
        float MappedRadius,RadialDerivative,StemInfluence;
        if (Radius>=RadialOuter)
        {
            MappedRadius=CrownScale*Radius+(Stem-CrownScale)*Offset;
            RadialDerivative=CrownScale;StemInfluence=Offset;
        }
        else
        {
            const float T=(Radius-RadialInner)/Width;
            const float Integral=T*T*T-.5f*T*T*T*T;
            MappedRadius=Stem*RadialInner+Width*(Stem*T+(CrownScale-Stem)*Integral);
            RadialDerivative=FMath::Lerp(Stem,CrownScale,T*T*(3.f-2.f*T));
            StemInfluence=RadialInner+Width*(T-Integral);
        }
        Scale=MappedRadius/Radius;
        RadialCoefficient=(RadialDerivative-Scale)/(Radius*Radius);
        VerticalDerivative=StemInfluence*StemDerivative/Radius;
    }
    FVector3f Position(const FVector3f& Point) const
    {
        float Scale,Radial,Vertical;ScaleAt(Point,Scale,Radial,Vertical);
        if (CrownLift<=0.f) return FVector3f(Point.X*Scale,Point.Y*Scale,Point.Z*HeightScale+ZShift);
        float Height,HeightDerivative;VerticalMap(Point,Height,HeightDerivative);
        return FVector3f(Point.X*Scale,Point.Y*Scale,SourceFloor*HeightScale+Height+ZShift);
    }
    void Frame(const FVector3f& Point,FVector3f& Normal,FVector3f& Tangent) const
    {
        float Scale,Radial,Vertical;ScaleAt(Point,Scale,Radial,Vertical);
        float Height,HeightDerivative;this->VerticalMap(Point,Height,HeightDerivative);
        const float A=Scale+Radial*Point.X*Point.X,B=Radial*Point.X*Point.Y,D=Scale+Radial*Point.Y*Point.Y;
        const float Determinant=A*D-B*B;
        const float NX=(D*Normal.X-B*Normal.Y)/Determinant,NY=(A*Normal.Y-B*Normal.X)/Determinant;
        Normal=FVector3f(NX,NY,(Normal.Z-Vertical*(Point.X*NX+Point.Y*NY))/HeightDerivative).GetSafeNormal();
        Tangent=FVector3f(A*Tangent.X+B*Tangent.Y+Point.X*Vertical*Tangent.Z,
            B*Tangent.X+D*Tangent.Y+Point.Y*Vertical*Tangent.Z,HeightDerivative*Tangent.Z).GetSafeNormal();
    }
};
TBitArray<> LeafGroups(const FStaticMeshLODResources& Data,const FStaticMeshSection& Section,float Coverage)
{
    TBitArray<> Keep(true,Section.NumTriangles);
    if (Coverage>=1.f) return Keep;
    const auto Indices=Data.IndexBuffer.GetArrayView();
    const auto& Positions=Data.VertexBuffers.PositionVertexBuffer;
    TArray<int32> Parents;Parents.SetNum(Positions.GetNumVertices());
    for (int32 Index=0;Index<Parents.Num();++Index) Parents[Index]=Index;
    auto Root=[&](int32 Index)
    {
        while (Parents[Index]!=Index) { Parents[Index]=Parents[Parents[Index]];Index=Parents[Index]; }
        return Index;
    };
    auto Join=[&](int32 A,int32 B) { A=Root(A);B=Root(B);Parents[FMath::Max(A,B)]=FMath::Min(A,B); };
    // Position welding joins UV/tangent seams too, so folded cards and both
    // triangles of a leaf survive or disappear together. No half-leaf wedges.
    TMap<FIntVector,int32> Welded;
    for (uint32 Wedge=0;Wedge<Section.NumTriangles*3;++Wedge)
    {
        const int32 Index=Indices[Section.FirstIndex+Wedge];
        const FVector3f Point=Positions.VertexPosition(Index);
        const FIntVector Key(FMath::RoundToInt(Point.X*100.f),FMath::RoundToInt(Point.Y*100.f),FMath::RoundToInt(Point.Z*100.f));
        if (const int32* Found=Welded.Find(Key)) Join(Index,*Found);else Welded.Add(Key,Index);
    }
    for (uint32 Triangle=0;Triangle<Section.NumTriangles;++Triangle)
    {
        const uint32 First=Section.FirstIndex+Triangle*3;
        Join(Indices[First],Indices[First+1]);Join(Indices[First],Indices[First+2]);
    }
    TMap<int32,uint32> Hashes;
    uint32 Lowest=TNumericLimits<uint32>::Max();int32 Always=INDEX_NONE;
    for (uint32 Triangle=0;Triangle<Section.NumTriangles;++Triangle)
    {
        const int32 Group=Root(Indices[Section.FirstIndex+Triangle*3]);
        if (!Hashes.Contains(Group))
        {
            const FVector3f Point=Positions.VertexPosition(Group);
            uint32 Hash=uint32(FMath::RoundToInt(Point.X*100.f))*73856093u
                ^uint32(FMath::RoundToInt(Point.Y*100.f))*19349663u
                ^uint32(FMath::RoundToInt(Point.Z*100.f))*83492791u;
            Hash=(Hash^(Hash>>16))*2246822519u;Hash^=Hash>>13;
            Hashes.Add(Group,Hash);
            if (Hash<Lowest) { Lowest=Hash;Always=Group; }
        }
    }
    int32 Retained=0;
    for (uint32 Triangle=0;Triangle<Section.NumTriangles;++Triangle)
    {
        const int32 Group=Root(Indices[Section.FirstIndex+Triangle*3]);
        Keep[Triangle]=Group==Always || double(Hashes[Group])/4294967296.0<Coverage;
        Retained+=int32(Keep[Triangle]);
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_LEAF_GROUPS groups=%d originalTriangles=%d retainedTriangles=%d targetCoverage=%.2f intactCards=1"),
        Hashes.Num(),Section.NumTriangles,Retained,Coverage);
    return Keep;
}
int32 Append(FMeshDescription& Description,UStaticMesh* Source,int32 Lod,int32 Role,
    const TArray<UMaterialInterface*>& Materials,const TArray<FPolygonGroupID>& Groups,float GridCm=0.f,const FFit* Fit=nullptr,float LeafCoverage=1.f)
{
    const FStaticMeshLODResources& Data=Source->GetRenderData()->LODResources[Lod];
    const auto& Buffers=Data.VertexBuffers;
    const auto Indices=Data.IndexBuffer.GetArrayView();
    FStaticMeshAttributes Attributes(Description);
    auto Positions=Attributes.GetVertexPositions();
    auto Normals=Attributes.GetVertexInstanceNormals();
    auto Tangents=Attributes.GetVertexInstanceTangents();
    auto Signs=Attributes.GetVertexInstanceBinormalSigns();
    auto UVs=Attributes.GetVertexInstanceUVs();
    auto Colors=Attributes.GetVertexInstanceColors();
    TArray<FVertexID> Vertices;
    Vertices.Init(FVertexID(INDEX_NONE),Buffers.PositionVertexBuffer.GetNumVertices());
    int32 Triangles=0;
    for (const FStaticMeshSection& Section:Data.Sections)
    {
        const int32 ActualRole=SectionRole(Source,Section.MaterialIndex);
        if (Role!=INDEX_NONE && ActualRole!=Role) continue;
        const int32 MaterialIndex=Materials.Find(Source->GetMaterial(Section.MaterialIndex));
        if (!Groups.IsValidIndex(MaterialIndex)) return INDEX_NONE;
        const TBitArray<> Keep=LeafGroups(Data,Section,ActualRole==0?LeafCoverage:1.f);
        for (uint32 Triangle=0;Triangle<Section.NumTriangles;++Triangle)
        {
            if (!Keep[Triangle]) continue;
            FVector3f Points[3];
            for (uint32 Corner=0;Corner<3;++Corner)
            {
                const uint32 Index=Indices[Section.FirstIndex+Triangle*3+Corner];
                if (Index>=uint32(Vertices.Num())) return INDEX_NONE;
                Points[Corner]=Buffers.PositionVertexBuffer.VertexPosition(Index);
                if (GridCm>0.f)
                {
                    // Spatial clustering preserves the large continuous bole
                    // while removing sub-pixel twigs. Keep original UV/pivot
                    // payloads; do not interpolate packed shader coordinates.
                    for (int32 Axis=0;Axis<3;++Axis)
                        Points[Corner][Axis]=FMath::RoundToFloat(Points[Corner][Axis]/GridCm)*GridCm;
                }
            }
            if (Fit) for (FVector3f& Point:Points) Point=Fit->Position(Point);
            if (GridCm>0.f && FVector3f::CrossProduct(Points[1]-Points[0],Points[2]-Points[0]).SizeSquared()<.0001f) continue;
            TArray<FVertexInstanceID,TInlineAllocator<3>> Corners;
            for (uint32 Corner=0;Corner<3;++Corner)
            {
                const uint32 Index=Indices[Section.FirstIndex+Triangle*3+Corner];
                if (Index>=uint32(Vertices.Num())) return INDEX_NONE;
                if (Vertices[Index].GetValue()==INDEX_NONE)
                {
                    Vertices[Index]=Description.CreateVertex();
                    Positions[Vertices[Index]]=Points[Corner];
                }
                const FVertexInstanceID Instance=Description.CreateVertexInstance(Vertices[Index]);
                FVector3f Normal(Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index));
                FVector3f Tangent(Buffers.StaticMeshVertexBuffer.VertexTangentX(Index));
                if (Fit) Fit->Frame(Buffers.PositionVertexBuffer.VertexPosition(Index),Normal,Tangent);
                Normals[Instance]=Normal;
                Tangents[Instance]=Tangent;
                Signs[Instance]=Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index).W;
                for (int32 Channel=0;Channel<UVs.GetNumChannels();++Channel)
                    UVs.Set(Instance,Channel,Channel<int32(Buffers.StaticMeshVertexBuffer.GetNumTexCoords())?
                        Buffers.StaticMeshVertexBuffer.GetVertexUV(Index,Channel):FVector2f::ZeroVector);
                const FLinearColor Color=Index<Buffers.ColorVertexBuffer.GetNumVertices()?
                    FLinearColor(Buffers.ColorVertexBuffer.VertexColor(Index)):FLinearColor::White;
                Colors[Instance]=FVector4f(Color.R,Color.G,Color.B,Color.A);
                Corners.Add(Instance);
            }
            Description.CreateTriangle(Groups[MaterialIndex],Corners);
            ++Triangles;
        }
    }
    return Triangles;
}
}

UStaticMesh* HouseCalderTreeMeshes::RetainStructure(UObject* Outer,UStaticMesh* Leaves,UStaticMesh* Structure,
    float NearGridCm,float MiddleGridCm,float FarGridCm,int32 ForestBudget)
{
    if (!Outer || !Leaves || !Structure || !Leaves->GetRenderData() || !Structure->GetRenderData()
        || Leaves->GetRenderData()->LODResources.Num()!=3 || (Structure->GetRenderData()->LODResources.Num()!=1 && Structure->GetRenderData()->LODResources.Num()!=3)
        || !Leaves->bAllowCPUAccess || !Structure->bAllowCPUAccess) return nullptr;
    const bool ReducedWood=Structure->GetRenderData()->LODResources.Num()==3;
    const float WoodGrids[3]={NearGridCm,MiddleGridCm,FarGridCm};
    for (const float Grid:WoodGrids)
        if (!FMath::IsFinite(Grid) || Grid<0.f || Grid>64.f) return nullptr;
    const double Started=FPlatformTime::Seconds();
    TArray<UMaterialInterface*> Materials;
    for (UStaticMesh* Source:{Leaves,Structure})
        for (const FStaticMaterial& Slot:Source->GetStaticMaterials())
            if (Slot.MaterialInterface) Materials.AddUnique(Slot.MaterialInterface);
    UStaticMesh* Mesh=NewObject<UStaticMesh>(Outer,MakeUniqueObjectName(Outer,UStaticMesh::StaticClass(),TEXT("CalderTreeRetainedStructure")),RF_Transient);
    Mesh->NeverStream=true;
    Mesh->bAllowCPUAccess=true;
    for (int32 Index=0;Index<Materials.Num();++Index)
    {
        const FName Slot(*FString::Printf(TEXT("CalderTreeSlot%d"),Index));
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Materials[Index],Slot
#if WITH_EDITORONLY_DATA
            ,Slot
#endif
        ));
    }
    TArray<FMeshDescription> Descriptions;
    Descriptions.SetNum(3);
    TArray<const FMeshDescription*> Pointers;
    int32 LeafCounts[3]={},SecondaryCounts[3]={},TrunkCounts[3]={};
    for (int32 Lod=0;Lod<3;++Lod)
    {
        FMeshDescription& Description=Descriptions[Lod];
        FStaticMeshAttributes Attributes(Description);
        Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(FMath::Max(
            Leaves->GetRenderData()->LODResources[Lod].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(),
            Structure->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords()));
        TArray<FPolygonGroupID> Groups;
        auto Names=Attributes.GetPolygonGroupMaterialSlotNames();
        for (int32 Index=0;Index<Materials.Num();++Index)
        {
            const FPolygonGroupID Group=Description.CreatePolygonGroup();
            Names[Group]=Mesh->GetStaticMaterials()[Index].MaterialSlotName;
            Groups.Add(Group);
        }
        const float Coverage[3]={ForestBudget==2?.55f:1.f,ForestBudget==2?.18f:.55f,ForestBudget==2?.04f:.20f};
        LeafCounts[Lod]=HouseCalderTreeMeshesImpl::Append(Description,Leaves,ForestBudget?2:Lod,0,Materials,Groups,0.f,nullptr,ForestBudget?Coverage[Lod]:1.f);
        const int32 WoodLod=ReducedWood?(ForestBudget==2?2:ForestBudget?FMath::Min(Lod+1,2):Lod):0;
        TrunkCounts[Lod]=HouseCalderTreeMeshesImpl::Append(Description,Structure,WoodLod,1,Materials,Groups);
        SecondaryCounts[Lod]=HouseCalderTreeMeshesImpl::Append(Description,Structure,WoodLod,2,Materials,Groups,
            ForestBudget?WoodGrids[Lod]:(ReducedWood?0.f:WoodGrids[Lod]));
        if (LeafCounts[Lod]<=0 || SecondaryCounts[Lod]<0 || TrunkCounts[Lod]<=0) return nullptr;
        Pointers.Add(&Description);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bBuildSimpleCollision=false;
    Parameters.bFastBuild=true;
    Parameters.bAllowCpuAccess=true;
    Parameters.bMarkPackageDirty=false;
    Parameters.bCommitMeshDescription=false;
    Parameters.PerLODOverrides.SetNum(3);
    for (auto& LodParameters:Parameters.PerLODOverrides)
    {
        // Donor UVs include packed wind/pivot data. Keep full precision rather
        // than introducing half-float quantization while copying render data.
        LodParameters.bUseFullPrecisionUVs=true;
        LodParameters.bUseHighPrecisionTangentBasis=true;
    }
    if (!Mesh->BuildFromMeshDescriptions(Pointers,Parameters)) return nullptr;
    FStaticMeshRenderData* Render=Mesh->GetRenderData();
    if (Render->LODResources[0].GetNumTriangles()<=Render->LODResources[1].GetNumTriangles()
        || Render->LODResources[1].GetNumTriangles()<=Render->LODResources[2].GetNumTriangles()) return nullptr;
    Render->ScreenSize[0].Default=1.f;
    Render->ScreenSize[1].Default=.35f;
    Render->ScreenSize[2].Default=.12f;
    UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_STRUCTURE source=%s woodLods=%d trunkTriangles=%d/%d/%d leafTriangles=%d/%d/%d secondaryOrClusteredWood=%d/%d/%d woodGridCm=%.1f/%.1f/%.1f actualTriangles=%d/%d/%d uvChannels=%d ms=%.1f collision=0 defaultBinding=0"),
        *Leaves->GetName(),Structure->GetRenderData()->LODResources.Num(),TrunkCounts[0],TrunkCounts[1],TrunkCounts[2],LeafCounts[0],LeafCounts[1],LeafCounts[2],SecondaryCounts[0],SecondaryCounts[1],SecondaryCounts[2],ReducedWood?0.f:WoodGrids[0],ReducedWood?0.f:WoodGrids[1],ReducedWood?0.f:WoodGrids[2],Render->LODResources[0].GetNumTriangles(),
        Render->LODResources[1].GetNumTriangles(),Render->LODResources[2].GetNumTriangles(),
        Render->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(),(FPlatformTime::Seconds()-Started)*1000.);
    return Mesh;
}

UStaticMesh* HouseCalderTreeMeshes::FitToOriginal(UObject* Outer,UStaticMesh* Prepared,UStaticMesh* Original,float CrownLift)
{
    auto Fail=[&](const TCHAR* Reason)->UStaticMesh*
    {
        UE_LOG(LogTemp,Error,TEXT("CALDER_TREE_FIT_REJECT original=%s reason=%s"),*GetNameSafe(Original),Reason);
        return nullptr;
    };
    if (!Outer || !Prepared || !Original || !Prepared->bAllowCPUAccess || !Prepared->GetRenderData()
        || Prepared->GetRenderData()->LODResources.Num()!=3 || !Original->GetBodySetup()
        || Original->GetBodySetup()->AggGeom.BoxElems.Num()>1) return Fail(TEXT("inputs/body"));
    const FBox SourceBox=Prepared->GetBoundingBox(),TargetBox=Original->GetBoundingBox();
    if (SourceBox.GetSize().Z<1. || TargetBox.GetSize().Z<1.) return Fail(TEXT("source/target height"));
    const auto& NearData=Prepared->GetRenderData()->LODResources[0];
    const auto Indices=NearData.IndexBuffer.GetArrayView();
    TArray<float> StemRadii;
    for (const FStaticMeshSection& Section:NearData.Sections)
        if (HouseCalderTreeMeshesImpl::SectionRole(Prepared,Section.MaterialIndex)==1)
            for (uint32 Wedge=0;Wedge<Section.NumTriangles*3;++Wedge)
            {
                const FVector3f Point=NearData.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[Section.FirstIndex+Wedge]);
                if (Point.Z-SourceBox.Min.Z>=30. && Point.Z-SourceBox.Min.Z<=250.)
                    StemRadii.Add(FMath::Sqrt(Point.X*Point.X+Point.Y*Point.Y));
            }
    // Some acquired donors assign the lower bole to the tileable bark slot,
    // with only the upper stem in a named trunk slot. Measure that actual
    // lower wood when needed; keep the established maple fit unchanged.
    if (StemRadii.Num()<30)
        for (const FStaticMeshSection& Section:NearData.Sections)
            if (HouseCalderTreeMeshesImpl::SectionRole(Prepared,Section.MaterialIndex)!=0)
                for (uint32 Wedge=0;Wedge<Section.NumTriangles*3;++Wedge)
                {
                    const FVector3f Point=NearData.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[Section.FirstIndex+Wedge]);
                    if (Point.Z-SourceBox.Min.Z>=30. && Point.Z-SourceBox.Min.Z<=250.)
                        StemRadii.Add(FMath::Sqrt(Point.X*Point.X+Point.Y*Point.Y));
                }
    // Coarse conventional LODs can span an entire lower stem between two
    // rings. Cross-section edge intersections measure that same actual bole
    // without depending on how many vertices the simplifier kept in the band.
    if (StemRadii.Num()<30)
        for (const FStaticMeshSection& Section:NearData.Sections)
            if (HouseCalderTreeMeshesImpl::SectionRole(Prepared,Section.MaterialIndex)!=0)
                for (uint32 Triangle=0;Triangle<Section.NumTriangles;++Triangle)
                    for (const float Height:{45.f,90.f,135.f,180.f,225.f})
                        for (uint32 Edge=0;Edge<3;++Edge)
                        {
                            const FVector3f A=NearData.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[Section.FirstIndex+Triangle*3+Edge]);
                            const FVector3f B=NearData.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[Section.FirstIndex+Triangle*3+(Edge+1)%3]);
                            const float Z=SourceBox.Min.Z+Height;
                            if ((A.Z<Z)==(B.Z<Z)) continue;
                            const FVector3f Point=FMath::Lerp(A,B,(Z-A.Z)/(B.Z-A.Z));
                            StemRadii.Add(FMath::Sqrt(Point.X*Point.X+Point.Y*Point.Y));
                        }
    if (StemRadii.Num()<30) return Fail(TEXT("stem sample count"));
    StemRadii.Sort();
    const float StemRadius=StemRadii[FMath::FloorToInt((StemRadii.Num()-1)*.95f)];
    if (StemRadius<=1.f) return Fail(TEXT("stem radius"));
    auto Extent=[](const FBox& Box,int32 Axis)
    { return FMath::Max(FMath::Abs(float(Box.Min[Axis])),FMath::Abs(float(Box.Max[Axis]))); };
    HouseCalderTreeMeshesImpl::FFit Fit;
    Fit.SourceFloor=float(SourceBox.Min.Z);Fit.SourceHeight=float(SourceBox.GetSize().Z);
    Fit.CrownLift=FMath::Clamp(CrownLift,0.f,.65f);
    Fit.HeightScale=float(TargetBox.GetSize().Z/SourceBox.GetSize().Z);
    Fit.ZShift=float(TargetBox.Min.Z)-Fit.SourceFloor*Fit.HeightScale;
    const bool HasTrunk=Original->GetBodySetup()->AggGeom.BoxElems.Num()==1;
    const float TargetStem=HasTrunk?Original->GetBodySetup()->AggGeom.BoxElems[0].X*.5f:
        FMath::Min(Extent(TargetBox,0)*.08f,float(TargetBox.GetSize().Z)*.018f);
    Fit.StemScale=TargetStem/StemRadius;
    Fit.RadialInner=StemRadius*1.3f;Fit.RadialOuter=StemRadius*3.3f;
    const float Offset=(Fit.RadialInner+Fit.RadialOuter)*.5f;
    Fit.CrownScale=FMath::Min(
        (Extent(TargetBox,0)-Fit.StemScale*Offset)/FMath::Max(1.f,Extent(SourceBox,0)-Offset),
        (Extent(TargetBox,1)-Fit.StemScale*Offset)/FMath::Max(1.f,Extent(SourceBox,1)-Offset));
    if (Fit.StemScale<=0.f || Fit.CrownScale<=0.f) return Fail(TEXT("scale"));
    // The analytic initial bound assumes fully blended upper wood. A short
    // tree can have a wide limb still inside the stem-to-crown transition.
    // Solve against the actual near vertices rather than relaxing its gate.
    auto FitsCrown=[&](float Candidate)
    {
        Fit.CrownScale=Candidate;
        for (uint32 Index=0;Index<NearData.VertexBuffers.PositionVertexBuffer.GetNumVertices();++Index)
        {
            const FVector3f Point=Fit.Position(NearData.VertexBuffers.PositionVertexBuffer.VertexPosition(Index));
            if (FMath::Abs(Point.X)>Extent(TargetBox,0)-.005f || FMath::Abs(Point.Y)>Extent(TargetBox,1)-.005f) return false;
        }
        return true;
    };
    const float InitialCrown=Fit.CrownScale;
    if (!FitsCrown(InitialCrown))
    {
        float Low=.0001f,High=InitialCrown;
        if (!FitsCrown(Low)) return Fail(TEXT("stem outside crown envelope"));
        for (int32 Iteration=0;Iteration<24;++Iteration)
        {
            const float Middle=(Low+High)*.5f;
            if (FitsCrown(Middle)) Low=Middle;else High=Middle;
        }
        Fit.CrownScale=Low;
        UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_CROWN_SOLVE original=%s initial=%.7f fitted=%.7f originalEnvelopeGateUnchanged=1"),
            *Original->GetName(),InitialCrown,Fit.CrownScale);
    }

    UStaticMesh* Mesh=NewObject<UStaticMesh>(Outer,MakeUniqueObjectName(Outer,UStaticMesh::StaticClass(),TEXT("CalderWoodsFittedTree")),RF_Transient);
    Mesh->NeverStream=true; Mesh->bAllowCPUAccess=true;
    TArray<UMaterialInterface*> Materials;
    for (const FStaticMaterial& Slot:Prepared->GetStaticMaterials()) Materials.Add(Slot.MaterialInterface);
    for (int32 Index=0;Index<Materials.Num();++Index)
    {
        const FName Slot(*FString::Printf(TEXT("CalderFittedSlot%d"),Index));
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Materials[Index],Slot
#if WITH_EDITORONLY_DATA
            ,Slot
#endif
        ));
    }
    TArray<FMeshDescription> Descriptions;Descriptions.SetNum(3);
    TArray<const FMeshDescription*> Pointers;
    for (int32 Lod=0;Lod<3;++Lod)
    {
        FMeshDescription& Description=Descriptions[Lod];
        FStaticMeshAttributes Attributes(Description);Attributes.Register();
        Attributes.GetVertexInstanceUVs().SetNumChannels(Prepared->GetRenderData()->LODResources[Lod].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
        TArray<FPolygonGroupID> Groups;auto Names=Attributes.GetPolygonGroupMaterialSlotNames();
        for (int32 Index=0;Index<Materials.Num();++Index)
        {
            const FPolygonGroupID Group=Description.CreatePolygonGroup();
            Names[Group]=Mesh->GetStaticMaterials()[Index].MaterialSlotName;Groups.Add(Group);
        }
        if (HouseCalderTreeMeshesImpl::Append(Description,Prepared,Lod,INDEX_NONE,Materials,Groups,0.f,&Fit)<=0) return Fail(TEXT("append"));
        Pointers.Add(&Description);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bBuildSimpleCollision=false;Parameters.bFastBuild=true;Parameters.bAllowCpuAccess=true;
    Parameters.bMarkPackageDirty=false;Parameters.bCommitMeshDescription=false;
    Parameters.PerLODOverrides.SetNum(3);
    for (auto& Lod:Parameters.PerLODOverrides) { Lod.bUseFullPrecisionUVs=true;Lod.bUseHighPrecisionTangentBasis=true; }
    if (!Mesh->BuildFromMeshDescriptions(Pointers,Parameters)) return Fail(TEXT("mesh build"));
    Mesh->SetBodySetup(DuplicateObject<UBodySetup>(Original->GetBodySetup(),Mesh,TEXT("CalderPreservedTrunkBody")));
    for (int32 Lod=0;Lod<3;++Lod)
    {
        if (Original->GetRenderData() && Original->GetRenderData()->LODResources.IsValidIndex(Lod))
            Mesh->GetRenderData()->ScreenSize[Lod]=Original->GetRenderData()->ScreenSize[Lod];
        if (Mesh->GetRenderData()->LODResources[Lod].GetNumTriangles()!=Prepared->GetRenderData()->LODResources[Lod].GetNumTriangles()) return Fail(TEXT("triangle count"));
    }
    const FBox Actual=Mesh->GetBoundingBox();
    UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_FIT_ENVELOPE original=%s targetMin=%s targetMax=%s actualMin=%s actualMax=%s stemScale=%.5f crownScale=%.5f"),
        *Original->GetName(),*TargetBox.Min.ToString(),*TargetBox.Max.ToString(),*Actual.Min.ToString(),*Actual.Max.ToString(),Fit.StemScale,Fit.CrownScale);
    if (FMath::Abs(Actual.Min.Z-TargetBox.Min.Z)>.1 || FMath::Abs(Actual.Max.Z-TargetBox.Max.Z)>.1
        || Extent(Actual,0)>Extent(TargetBox,0)+.1f || Extent(Actual,1)>Extent(TargetBox,1)+.1f) return Fail(TEXT("envelope"));
    UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_FIT original=%s fitted=%s heightCm=%.2f stemScale=%.5f crownScale=%.5f sourceStem95=%.3f preservedBoxRadius=%.3f collisionBodyCopied=1 instancesMoved=0"),
        *Original->GetName(),*Mesh->GetName(),Actual.GetSize().Z,Fit.StemScale,Fit.CrownScale,StemRadius,TargetStem);
    return Mesh;
}
