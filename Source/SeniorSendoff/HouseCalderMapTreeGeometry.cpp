#include "HouseCalderMapTreeGeometry.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"

namespace HouseCalderMapTreeGeometryImpl
{
const FName Wood(TEXT("CalderConifer_Trunk_01")),BranchMaterial(TEXT("CalderConiferBranches")),Needles(TEXT("CalderConiferTwoSided"));
void Stem(FHallMesh& Mesh,const TArray<FVector>& Spine,float Radius,int32 Sides,FName Key=Wood)
{
    FHallSection& Section=Mesh.Sec(Key);
    const int32 First=Section.V.Num();
    float Along=0.f;
    for (int32 Ring=0;Ring<Spine.Num();++Ring)
    {
        if (Ring) Along+=FVector::Dist(Spine[Ring],Spine[Ring-1])/140.f;
        const FVector Axis=(Spine[FMath::Min(Ring+1,Spine.Num()-1)]-Spine[FMath::Max(Ring-1,0)]).GetSafeNormal();
        const FVector U=FVector::CrossProduct(Axis,FMath::Abs(Axis.Z)>.9f?FVector::ForwardVector:FVector::UpVector).GetSafeNormal();
        const FVector V=FVector::CrossProduct(Axis,U).GetSafeNormal();
        const float T=float(Ring)/(Spine.Num()-1),R=FMath::Lerp(Radius,.28f,T)*(Ring==0?1.15f:1.f);
        for (int32 Side=0;Side<=Sides;++Side)
        {
            const float Angle=2.f*PI*Side/Sides;
            const FVector Normal=U*FMath::Cos(Angle)+V*FMath::Sin(Angle);
            Section.V.Add(Spine[Ring]+Normal*R*(1.f+.045f*FMath::Sin(Side*2.3f+Ring)));
            Section.N.Add(Normal);Section.UV.Add(FVector2D(float(Side)/Sides,-Along));
            Section.T.Add(FProcMeshTangent(V*FMath::Cos(Angle)-U*FMath::Sin(Angle),false));
        }
    }
    for (int32 Ring=0;Ring+1<Spine.Num();++Ring) for (int32 Side=0;Side<Sides;++Side)
    {
        const int32 A=First+Ring*(Sides+1)+Side,B=A+1,C=B+Sides+1,D=A+Sides+1;
        if (FVector::DotProduct(FVector::CrossProduct(Section.V[B]-Section.V[A],Section.V[C]-Section.V[A]),Section.N[A])<0)
            Section.I.Append({A,B,C,A,C,D});
        else Section.I.Append({A,C,B,A,D,C});
    }
}
void Spray(FHallMesh& Mesh,const FVector& Root,const FVector& End,const FVector& Across,float Width,int32 Pieces)
{
    // Curved, three-piece ribbon. The same sampled needle spray remains intact
    // in each LOD rather than turning into an opaque cone at distance.
    const FVector Along=End-Root;
    for (int32 Part=0;Part<Pieces;++Part)
    {
        const float A=float(Part)/Pieces,B=float(Part+1)/Pieces;
        auto Center=[&](float T){return Root+Along*T+FVector(0,0,-12.f*FMath::Sin(PI*T));};
        const FVector CA=Center(A),CB=Center(B),U=Across*Width;
        const FVector Normal=FVector::CrossProduct(Across,CB-CA).GetSafeNormal();
        Mesh.QuadUV(Needles,CA-U,CA+U,CB+U,CB-U,Normal,{0,1-A},{1,1-A},{1,1-B},{0,1-B},Across);
    }
}
void Geometry(FHallMesh& Mesh,int32 Lod)
{
    FRandomStream Random(2026101793);
    TArray<FVector> Bole;
    for (int32 Ring=0;Ring<=20;++Ring)
    {
        const float Z=-8.f+Ring*100.4f;
        Bole.Add(FVector(FMath::Sin(Z*.003f)*8.f,FMath::Sin(Z*.002f)*5.f,Z));
    }
    Stem(Mesh,Bole,34.f,Lod==0?20:Lod==1?12:8);
    for (int32 Tier=0;Tier<23;++Tier)
    {
        const float Z=240.f+Tier*73.f,T=Z/2000.f;
        const float Reach=340.f*FMath::Pow(1.f-T,.68f);
        const int32 Limbs=5+Tier%3;
        for (int32 Limb=0;Limb<Limbs;++Limb)
        {
            const float Angle=Tier*2.39996f+Limb*2.f*PI/Limbs+Random.FRandRange(-.16f,.16f);
            const FVector Out(FMath::Cos(Angle),FMath::Sin(Angle),0),Side(-Out.Y,Out.X,0);
            const FVector Start(FMath::Sin(Z*.003f)*8.f,FMath::Sin(Z*.002f)*5.f,Z);
            const FVector End=Start+Out*Reach*Random.FRandRange(.72f,1.17f)+FVector(0,0,Random.FRandRange(-46.f,12.f));
            const FVector Bend=FMath::Lerp(Start,End,.53f)+FVector(0,0,-18.f);
            TArray<FVector> LimbPoints={Start,FMath::Lerp(Start,Bend,.5f),Bend,FMath::Lerp(Bend,End,.55f),End};
            if (Lod==0 || (Limb%2==0 && (Lod<2 || Tier%3==0))) Stem(Mesh,LimbPoints,FMath::Lerp(7.f,1.1f,T),Lod==0?5:Lod==1?4:3,HouseCalderMapTreeGeometryImpl::BranchMaterial);
            if (Lod==2 && Limb%2!=0) continue;
            const int32 Twigs=Lod<2?2:1;
            for (int32 Twig=0;Twig<Twigs;++Twig)
            {
                const float Along=.24f+.73f*Twig/FMath::Max(1,Twigs-1);
                const FVector Anchor=FMath::Lerp(Start,End,Along);
                for (const float Sign:{-1.f,1.f})
                {
                    const float Length=(65.f+70.f*(1.f-T))*(1.f-.45f*Along);
                    const FVector Tip=Anchor+Out*Length*.55f+Side*Sign*Length+FVector(0,0,-17.f);
                    Spray(Mesh,Anchor,Tip,Out,Length*(Lod==2?.58f:.42f),Lod==0?2:1);
                }
            }
        }
    }
}
void Describe(const FHallMesh& From,FMeshDescription& Description)
{
    FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
    auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
    auto UVs=Attributes.GetVertexInstanceUVs();UVs.SetNumChannels(1);
    auto Names=Attributes.GetPolygonGroupMaterialSlotNames();
    for (const FName Key:From.Order)
    {
        const auto& Section=From.Sections[Key];
        const FPolygonGroupID Group=Description.CreatePolygonGroup();Names[Group]=Key;
        TArray<FVertexID> Vertices;
        for (const FVector& Point:Section.V) { const auto Vertex=Description.CreateVertex();Positions[Vertex]=FVector3f(Point);Vertices.Add(Vertex); }
        for (int32 Triangle=0;Triangle<Section.I.Num();Triangle+=3)
        {
            TArray<FVertexInstanceID,TInlineAllocator<3>> Corners;
            for (int32 Corner=0;Corner<3;++Corner)
            {
                const int32 Index=Section.I[Triangle+Corner];const auto Instance=Description.CreateVertexInstance(Vertices[Index]);
                Normals[Instance]=FVector3f(Section.N[Index]);Tangents[Instance]=FVector3f(Section.T[Index].TangentX);
                Signs[Instance]=1.f;UVs.Set(Instance,0,FVector2f(Section.UV[Index]));Corners.Add(Instance);
            }
            Description.CreateTriangle(Group,Corners);
        }
    }
}
void DeadGeometry(FHallMesh& Mesh,const FString& Name,const UBodySetup* Body,int32 Lod)
{
    const int32 Sides=Lod==0?28:Lod==1?14:8;
    auto Curve=[&](const TArray<FVector>& Guides,float Radius)
    {
        TArray<FVector> Points;
        const int32 Steps=Lod==0?5:Lod==1?3:2;
        for (int32 Segment=0;Segment+1<Guides.Num();++Segment)
            for (int32 Step=0;Step<Steps;++Step)
            {
                const double T=double(Step)/Steps;
                const FVector A=Guides[FMath::Max(0,Segment-1)],B=Guides[Segment],C=Guides[Segment+1],D=Guides[FMath::Min(Segment+2,Guides.Num()-1)];
                Points.Add(.5*((2.*B)+(-A+C)*T+(2.*A-5.*B+4.*C-D)*T*T+(-A+3.*B-3.*C+D)*T*T*T));
            }
        Points.Add(Guides.Last());Stem(Mesh,Points,Radius,Sides);
    };
    if (Name.Contains(TEXT("AmbiguousStump")))
    {
        Curve({{0,0,-5},{-6,3,42},{4,-2,92},{3,4,151}},35.f);
        for (int32 Splinter=0;Splinter<9;++Splinter)
        {
            const float Angle=Splinter*2.39996f;
            const FVector Root(FMath::Cos(Angle)*15,FMath::Sin(Angle)*15,118);
            Curve({Root,Root+FVector(4,-3,16),Root+FVector(6,-4,35+Splinter%3*7)},2.1f);
        }
        Curve({{-4,0,66},{-26,8,78},{-43,16,69}},9.f);
        return;
    }
    if (Name.Contains(TEXT("LeaningDeadwood")))
    {
        const FVector Difference=Body->AggGeom.BoxElems[1].Center-FVector(1.5,-4.5,93);
        const FVector Contact=Difference+FVector(-28,-10,706);
        Curve({{0,0,-6},Contact*.25+FVector(-12,9,2),Contact*.54+FVector(-18,21,-9),Contact,Contact+FVector(-70,50,80)},30.f);
        Curve({Difference+FVector(0,0,-6),Difference+FVector(8,-11,253),Difference+FVector(-15,-6,706),
            Difference+FVector(11,7,829),Difference+FVector(68,15,1111),Difference+FVector(112,40,1288)},35.f);
        Curve({Difference+FVector(11,7,829),Difference+FVector(-74,29,943),Difference+FVector(-157,58,1196)},13.f);
        Curve({Difference+FVector(-13,-7,568),Difference+FVector(84,5,643),Difference+FVector(148,37,834)},12.f);
        for (int32 Branch=0;Branch<4;++Branch)
        {
            const FVector Root=Contact*(.27f+Branch*.15f),Side=FVector::CrossProduct(Contact.GetSafeNormal(),FVector::UpVector).GetSafeNormal();
            Curve({Root,Root+Side*(Branch%2?85.f:-85.f)+FVector(0,0,20),Root+Side*(Branch%2?150.f:-150.f)+FVector(0,0,55)},6.f-Branch);
        }
        return;
    }
    const float Mirror=Name.EndsWith(TEXT("1"))?-1.f:1.f;
    Curve({{0,0,-8},{8,-12,245},{-22,7,515},{20,19,740},{-13,29,949},{-84,43,1181},{-64,56,1310}},34.f);
    for (int32 Branch=0;Branch<6;++Branch)
    {
        const float Z=420.f+Branch*120.f,Sign=Branch%2?1.f:-1.f;
        const FVector Root(0,0,Z),Tip(Sign*(160+Branch*14)*Mirror,23+Branch*9,Z+260-Branch*13);
        Curve({Root,FMath::Lerp(Root,Tip,.48f)+FVector(0,-9,15),Tip},11.f-Branch);
    }
}
}
UStaticMesh* HouseCalderMapTreeGeometry::Conifer(UHouseBlackHallSubsystem* Look)
{
    if (!Look) return nullptr;
    auto* Bark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Trees/MI_CalderTree_Maple_Trunk_01.MI_CalderTree_Maple_Trunk_01"));
    auto* BranchBark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Trees/MI_CalderTree_Maple_Tileable.MI_CalderTree_Maple_Tileable"));
    auto* LeafParent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NatureFoliage.M_NatureFoliage"));
    auto* Base=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Woods/T_CalderWoodsEvergreen_A.T_CalderWoodsEvergreen_A"));
    auto* Normal=LoadObject<UTexture2D>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Woods/T_CalderWoodsEvergreen_N.T_CalderWoodsEvergreen_N"));
    if (!Bark || !BranchBark || !LeafParent || !Base || !Normal) return nullptr;
    auto* Leaf=UMaterialInstanceDynamic::Create(LeafParent,Look,HouseCalderMapTreeGeometryImpl::Needles);
    Leaf->SetTextureParameterValue(TEXT("BaseMap"),Base);Leaf->SetTextureParameterValue(TEXT("NormalMap"),Normal);
    Leaf->SetVectorParameterValue(TEXT("Tint"),FLinearColor::White);
    Leaf->SetScalarParameterValue(TEXT("WindStrength"),.48f);Leaf->SetScalarParameterValue(TEXT("Transmission"),.22f);
    Look->Materials.Add(HouseCalderMapTreeGeometryImpl::Wood,Bark);
    Look->Materials.Add(HouseCalderMapTreeGeometryImpl::BranchMaterial,BranchBark);
    Look->Materials.Add(HouseCalderMapTreeGeometryImpl::Needles,Leaf);
    UStaticMesh* Mesh=NewObject<UStaticMesh>(Look,MakeUniqueObjectName(Look,UStaticMesh::StaticClass(),TEXT("CalderConiferTemplate")),RF_Transient);
    Mesh->NeverStream=true;Mesh->bAllowCPUAccess=true;
    for (const FName Key:{HouseCalderMapTreeGeometryImpl::Wood,HouseCalderMapTreeGeometryImpl::BranchMaterial,HouseCalderMapTreeGeometryImpl::Needles})
        Mesh->GetStaticMaterials().Add(FStaticMaterial(Look->Mat(Key),Key
#if WITH_EDITORONLY_DATA
            ,Key
#endif
        ));
    TArray<FMeshDescription> Descriptions;Descriptions.SetNum(3);TArray<const FMeshDescription*> Pointers;
    for (int32 Lod=0;Lod<3;++Lod)
    {
        FHallMesh Geometry;HouseCalderMapTreeGeometryImpl::Geometry(Geometry,Lod);
        HouseCalderMapTreeGeometryImpl::Describe(Geometry,Descriptions[Lod]);Pointers.Add(&Descriptions[Lod]);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bFastBuild=true;Parameters.bBuildSimpleCollision=false;Parameters.bAllowCpuAccess=true;
    Parameters.bMarkPackageDirty=false;Parameters.bCommitMeshDescription=false;
    if (!Mesh->BuildFromMeshDescriptions(Pointers,Parameters)) return nullptr;
    Mesh->CreateBodySetup();Mesh->GetBodySetup()->CollisionTraceFlag=CTF_UseSimpleAsComplex;
    auto* Render=Mesh->GetRenderData();Render->ScreenSize[0].Default=1.f;Render->ScreenSize[1].Default=.35f;Render->ScreenSize[2].Default=.12f;
    UE_LOG(LogTemp,Display,TEXT("CALDER_CONIFER_NEW triangles=%d/%d/%d oldCones=0"),Render->LODResources[0].GetNumTriangles(),Render->LODResources[1].GetNumTriangles(),Render->LODResources[2].GetNumTriangles());
    return Mesh;
}

UStaticMesh* HouseCalderMapTreeGeometry::Deadwood(UHouseBlackHallSubsystem* Look,UStaticMesh* Original,const FString& Name)
{
    if (!Look || !Original || !Original->GetBodySetup()) return nullptr;
    if (Name.Contains(TEXT("LeaningDeadwood")) && Original->GetBodySetup()->AggGeom.BoxElems.Num()!=2) return nullptr;
    UMaterialInterface* Bark=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Trees/MI_CalderTree_Maple_Trunk_01.MI_CalderTree_Maple_Trunk_01"));
    if (!Bark) return nullptr;
    Look->Materials.Add(HouseCalderMapTreeGeometryImpl::Wood,Bark);
    UStaticMesh* Mesh=NewObject<UStaticMesh>(Look,MakeUniqueObjectName(Look,UStaticMesh::StaticClass(),TEXT("CalderNewDeadwood")),RF_Transient);
    Mesh->NeverStream=true;Mesh->bAllowCPUAccess=true;
    const FName Key=HouseCalderMapTreeGeometryImpl::Wood;
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Bark,Key
#if WITH_EDITORONLY_DATA
        ,Key
#endif
    ));
    TArray<FMeshDescription> Descriptions;Descriptions.SetNum(3);TArray<const FMeshDescription*> Pointers;
    for (int32 Lod=0;Lod<3;++Lod)
    {
        FHallMesh Geometry;HouseCalderMapTreeGeometryImpl::DeadGeometry(Geometry,Name,Original->GetBodySetup(),Lod);
        HouseCalderMapTreeGeometryImpl::Describe(Geometry,Descriptions[Lod]);Pointers.Add(&Descriptions[Lod]);
    }
    UStaticMesh::FBuildMeshDescriptionsParams Parameters;
    Parameters.bFastBuild=true;Parameters.bBuildSimpleCollision=false;Parameters.bAllowCpuAccess=true;
    Parameters.bMarkPackageDirty=false;Parameters.bCommitMeshDescription=false;
    if (!Mesh->BuildFromMeshDescriptions(Pointers,Parameters)) return nullptr;
    Mesh->SetBodySetup(DuplicateObject<UBodySetup>(Original->GetBodySetup(),Mesh,TEXT("CalderUnchangedDeadwoodBody")));
    auto* Render=Mesh->GetRenderData();Render->ScreenSize[0].Default=1.f;Render->ScreenSize[1].Default=.35f;Render->ScreenSize[2].Default=.12f;
    UE_LOG(LogTemp,Display,TEXT("CALDER_MAP_DEADWOOD batch=%s triangles=%d/%d/%d oldModel=0 originalBodyCopied=1"),*Name,
        Render->LODResources[0].GetNumTriangles(),Render->LODResources[1].GetNumTriangles(),Render->LODResources[2].GetNumTriangles());
    return Mesh;
}
