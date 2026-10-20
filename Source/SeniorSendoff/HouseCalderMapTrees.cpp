#include "HouseCalderMapTrees.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderMapTreeGeometry.h"
#include "HouseCalderTreeMeshes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StaticMeshResources.h"

namespace HouseCalderMapTreesImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UHouseBlackHallSubsystem> Look;
    TMap<FString,TWeakObjectPtr<UStaticMesh>> Prepared,Fitted;
    TArray<FName> WindKeys;
};
FState State;
bool Leaf(UMaterialInterface* Material)
{
    return Material && Material->GetName().Contains(TEXT("TwoSided"));
}
UStaticMesh* Prepared(UHouseBlackHallSubsystem* Look,const FString& Species,int32 ForestBudget=0)
{
    const FString CacheKey=Species+FString::FromInt(ForestBudget);
    if (auto* Found=State.Prepared.Find(CacheKey)) if (Found->IsValid()) return Found->Get();
    UStaticMesh* Result=nullptr;
    if (Species==TEXT("Conifer")) Result=HouseCalderMapTreeGeometry::Conifer(Look);
    else
    {
        const FString Leaves=FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeCandidate_%s.SM_CalderTreeCandidate_%s"),*Species,*Species);
        const FString Wood=FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeWood_%s.SM_CalderTreeWood_%s"),*Species,*Species);
        Result=HouseCalderTreeMeshes::RetainStructure(Look,LoadObject<UStaticMesh>(nullptr,*Leaves),LoadObject<UStaticMesh>(nullptr,*Wood),2.f,32.f,64.f,ForestBudget);
    }
    if (Result) State.Prepared.Add(CacheKey,Result);
    return Result;
}
FLinearColor Tint(const FString& Name)
{
    if (Name.Contains(TEXT("Red")) || Name.Contains(TEXT("Maple0"))) return FLinearColor(.52f,.09f,.016f);
    if (Name.Contains(TEXT("Gold")) || Name.Contains(TEXT("Orange")) || Name.Contains(TEXT("Maple1"))) return FLinearColor(.48f,.23f,.035f);
    if (Name.Contains(TEXT("Oak"))) return FLinearColor(.30f,.135f,.036f);
    if (Name.Contains(TEXT("Hickory")) || Name.Contains(TEXT("Birch"))) return FLinearColor(.43f,.29f,.055f);
    if (Name.Contains(TEXT("Tulip"))) return FLinearColor(.36f,.27f,.065f);
    return FLinearColor(.39f,.19f,.04f);
}
void Describe(UStaticMesh* Source,int32 Lod,FHallMesh& Into,const FTransform& Pose,
    UHouseBlackHallSubsystem* Look,const FString& Prefix,bool StaticWind)
{
    const auto& Data=Source->GetRenderData()->LODResources[Lod];
    const auto Indices=Data.IndexBuffer.GetArrayView();
    const auto& Buffers=Data.VertexBuffers;
    for (const FStaticMeshSection& Section:Data.Sections)
    {
        UMaterialInterface* Parent=Source->GetMaterial(Section.MaterialIndex);
        const FName Key(*FString::Printf(TEXT("CalderTownTree_%s_%d"),*Prefix,Section.MaterialIndex));
        if (!Look->Materials.Contains(Key))
        {
            UMaterialInstanceDynamic* Material=UMaterialInstanceDynamic::Create(Parent,Look,Key);
            if (Leaf(Parent))
            {
                Material->SetScalarParameterValue(TEXT("WindStrength"),StaticWind?0.f:1.3f);
                Material->SetVectorParameterValue(TEXT("AutumnTint"),Tint(Prefix));
            }
            Look->Materials.Add(Key,Material);
        }
        FHallSection& Dest=Into.Sec(Key);
        TMap<uint32,int32> Remap;
        for (uint32 Wedge=0;Wedge<Section.NumTriangles*3;++Wedge)
        {
            const uint32 Index=Indices[Section.FirstIndex+Wedge];
            int32* Have=Remap.Find(Index);
            if (!Have)
            {
                Have=&Remap.Add(Index,Dest.V.Num());
                Dest.V.Add(Pose.TransformPosition(FVector(Buffers.PositionVertexBuffer.VertexPosition(Index))));
                FVector Normal(Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index));
                const FVector Scale=Pose.GetScale3D();
                Normal=Pose.TransformVectorNoScale(Normal/Scale).GetSafeNormal();
                Dest.N.Add(Normal);
                Dest.T.Add(FProcMeshTangent(Pose.TransformVector(FVector(Buffers.StaticMeshVertexBuffer.VertexTangentX(Index))).GetSafeNormal(),
                    Buffers.StaticMeshVertexBuffer.VertexTangentZ(Index).W<0));
                Dest.UV.Add(FVector2D(Buffers.StaticMeshVertexBuffer.GetVertexUV(Index,0)));
            }
            Dest.I.Add(*Have);
        }
    }
}
}

bool HouseCalderMapTrees::Enabled()
{
    return HouseCalderLandscape::Enabled()
        && !FParse::Param(FCommandLine::Get(),TEXT("NoCalderTrees"))
        && !FParse::Param(FCommandLine::Get(),TEXT("NoCalderMapTrees"));
}
bool HouseCalderMapTrees::TreeBatch(const FString& Name)
{
    if (Name.StartsWith(TEXT("ExteriorTree")) || Name.StartsWith(TEXT("CampusFallTree"))
        || Name.StartsWith(TEXT("ExteriorForestDistant")) || Name.StartsWith(TEXT("CalderCrowTree"))) return true;
    for (const TCHAR* Species:{TEXT("Oak"),TEXT("Hickory"),TEXT("Maple"),TEXT("TulipPoplar"),TEXT("Hemlock"),TEXT("DogwoodSapling")})
        if (Name.StartsWith(FString(TEXT("ExteriorWoodsPass"))+Species)) return true;
    return Name.StartsWith(TEXT("ExteriorAppalachianBareSnags"))
        || Name==TEXT("ExteriorAppalachianLeaningDeadwood") || Name==TEXT("ExteriorAppalachianAmbiguousStump");
}
UStaticMesh* HouseCalderMapTrees::Mesh(AActor* Owner,UHouseBlackHallSubsystem* Look,const FString& Name,
    UStaticMesh* Original,const TArray<FTransform>& Instances)
{
    if (!Enabled() || !Owner || !Look || !Original || !TreeBatch(Name)) return Original;
    auto& State=HouseCalderMapTreesImpl::State;
    if (State.World.Get()!=Owner->GetWorld())
    { State=HouseCalderMapTreesImpl::FState();State.World=Owner->GetWorld(); }
    State.Look=Look;
    if (auto* Found=State.Fitted.Find(Name)) if (Found->IsValid()) return Found->Get();
    if (Name.StartsWith(TEXT("ExteriorAppalachian")))
    {
        UStaticMesh* Dead=HouseCalderMapTreeGeometry::Deadwood(Look,Original,Name);
        if (Dead) State.Fitted.Add(Name,Dead);
        else UE_LOG(LogTemp,Error,TEXT("CALDER_MAP_TREE_FAILED batch=%s source=deadwood"),*Name);
        return Dead?Dead:Original;
    }
    const FString Species=Name.Contains(TEXT("Hemlock"))?TEXT("Conifer"):
        Name.Contains(TEXT("Maple"))?TEXT("Maple"):TEXT("Beech");
    const int32 ForestBudget=Name.Contains(TEXT("DogwoodSapling"))?2:
        Name.StartsWith(TEXT("ExteriorWoodsPass")) || Name.StartsWith(TEXT("ExteriorForestDistant"))?1:0;
    UStaticMesh* Template=HouseCalderMapTreesImpl::Prepared(Look,Species,ForestBudget);
    const bool EstablishedWoodsMaple=Name==TEXT("ExteriorWoodsPassMaple0") || Name==TEXT("ExteriorWoodsPassMaple1");
    const float Lift=Species==TEXT("Conifer") || EstablishedWoodsMaple?0.f:.55f;
    UStaticMesh* Result=HouseCalderTreeMeshes::FitToOriginal(Look,Template,Original,Lift);
    if (!Result)
    {
        UE_LOG(LogTemp,Error,TEXT("CALDER_MAP_TREE_FAILED batch=%s source=%s"),*Name,*Species);
        return Original;
    }
    const bool MapleTrial=Name==TEXT("ExteriorWoodsPassMaple0") || Name==TEXT("ExteriorWoodsPassMaple1");
    const FName WindKey(*(MapleTrial?FString(TEXT("CalderMapleLeafTwoSided"))+Name.Right(1):
        FString(TEXT("CalderMapTreeLeafTwoSided_"))+Name));
    for (int32 Slot=0;Slot<Result->GetStaticMaterials().Num();++Slot)
        if (UMaterialInterface* Parent=Result->GetMaterial(Slot))
            if (HouseCalderMapTreesImpl::Leaf(Parent))
            {
                UMaterialInstanceDynamic* Material=UMaterialInstanceDynamic::Create(Parent,Look,WindKey);
                Material->SetScalarParameterValue(TEXT("WindStrength"),Species==TEXT("Conifer")?.48f:1.3f);
                if (Species!=TEXT("Conifer")) Material->SetVectorParameterValue(TEXT("AutumnTint"),HouseCalderMapTreesImpl::Tint(Name));
                Look->Materials.Add(WindKey,Material);
                Result->GetStaticMaterials()[Slot].MaterialInterface=Material;
                State.WindKeys.AddUnique(WindKey);
            }
    State.Fitted.Add(Name,Result);
    const auto& Lods=Result->GetRenderData()->LODResources;
    UE_LOG(LogTemp,Display,TEXT("CALDER_MAP_TREE batch=%s donor=%s instances=%d triangles=%d/%d/%d originalBodyCopied=1 transformsUnchanged=1 oldRenderModel=0"),
        *Name,*Species,Instances.Num(),Lods[0].GetNumTriangles(),Lods[1].GetNumTriangles(),Lods[2].GetNumTriangles());
    return Result;
}
void HouseCalderMapTrees::AppendWindKeys(TArray<FName>& Keys)
{
    if (Enabled()) for (const FName Key:HouseCalderMapTreesImpl::State.WindKeys) Keys.AddUnique(Key);
}
void HouseCalderMapTrees::Canopy(UStaticMesh* Mesh,TArray<FVector>& Leaves)
{
    if (!Enabled() || !Mesh || !Mesh->bAllowCPUAccess || !Mesh->GetRenderData()) return;
    Leaves.Reset();
    const auto& Data=Mesh->GetRenderData()->LODResources[0];
    const auto Indices=Data.IndexBuffer.GetArrayView();
    for (const auto& Section:Data.Sections) if (HouseCalderMapTreesImpl::Leaf(Mesh->GetMaterial(Section.MaterialIndex)))
        for (uint32 Triangle=0;Triangle<Section.NumTriangles;Triangle+=4)
        {
            FVector Center=FVector::ZeroVector;
            for (uint32 Corner=0;Corner<3;++Corner) Center+=FVector(Data.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[Section.FirstIndex+Triangle*3+Corner]));
            Leaves.Add(Center/3.);
        }
}
bool HouseCalderMapTrees::Bark(UStaticMesh* Mesh,FHallSection& Out)
{
    if (!Enabled() || !Mesh || !Mesh->bAllowCPUAccess || !Mesh->GetRenderData()) return false;
    const auto& Data=Mesh->GetRenderData()->LODResources[0];
    const auto Indices=Data.IndexBuffer.GetArrayView();
    for (const auto& Section:Data.Sections) if (!HouseCalderMapTreesImpl::Leaf(Mesh->GetMaterial(Section.MaterialIndex)))
        for (uint32 Wedge=0;Wedge<Section.NumTriangles*3;++Wedge)
        {
            const uint32 Index=Indices[Section.FirstIndex+Wedge];
            Out.I.Add(Out.V.Num());
            Out.V.Add(FVector(Data.VertexBuffers.PositionVertexBuffer.VertexPosition(Index)));
            Out.N.Add(FVector(Data.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index)));
        }
    return !Out.I.IsEmpty();
}
void HouseCalderMapTrees::TownLook(UHouseBlackHallSubsystem* Look)
{
    if (Enabled()) HouseCalderMapTreesImpl::State.Look=Look;
}
bool HouseCalderMapTrees::TownTree(FHallMesh& Into,const FHallMesh& Original,const FVector& Foot,bool Evergreen,int32 Tone)
{
    if (!Enabled()) return false;
    auto* Look=HouseCalderMapTreesImpl::State.Look.Get();
    UStaticMesh* Template=Look?HouseCalderMapTreesImpl::Prepared(Look,Evergreen?TEXT("Conifer"):TEXT("Beech"),1):nullptr;
    if (!Template || !Template->GetRenderData()) return false;
    FBox Bounds(ForceInit);
    for (const auto& Pair:Original.Sections) for (const FVector& Point:Pair.Value.V) Bounds+=Point;
    const FBox Source=Template->GetBoundingBox();
    const double Radius=FMath::Min(FMath::Max(FMath::Abs(Bounds.Min.X-Foot.X),FMath::Abs(Bounds.Max.X-Foot.X)),
        FMath::Max(FMath::Abs(Bounds.Min.Y-Foot.Y),FMath::Abs(Bounds.Max.Y-Foot.Y)));
    const double SourceRadius=FMath::Max(FMath::Max(FMath::Abs(Source.Min.X),FMath::Abs(Source.Max.X)),
        FMath::Max(FMath::Abs(Source.Min.Y),FMath::Abs(Source.Max.Y)));
    const double XY=Radius/SourceRadius,Z=Bounds.GetSize().Z/Source.GetSize().Z;
    const FTransform Pose(FRotator::ZeroRotator,FVector(Foot.X,Foot.Y,Bounds.Min.Z-Source.Min.Z*Z),FVector(XY,XY,Z));
    const FString Prefix=Evergreen?TEXT("Evergreen"):Tone==2?TEXT("Red"):Tone==1||Tone==4?TEXT("Gold"):TEXT("Rust");
    HouseCalderMapTreesImpl::Describe(Template,2,Into,Pose,Look,Prefix,true);
    UE_LOG(LogTemp,Display,TEXT("CALDER_TOWN_TREE foot=%s evergreen=%d tone=%d sourceLod=2 legacyGeometryEmitted=0"),*Foot.ToCompactString(),int32(Evergreen),Tone);
    return true;
}
