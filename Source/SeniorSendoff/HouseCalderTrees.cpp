#include "HouseCalderTrees.h"
#include "HouseCalderMapTrees.h"
#include "HouseBlackHall.h"
#include "HouseCalderLandscape.h"
#include "HouseCalderTreeMeshes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StaticMeshResources.h"

namespace HouseCalderTreesImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UStaticMesh> Prepared;
    TMap<FString,TWeakObjectPtr<UStaticMesh>> Fitted;
    TArray<FName> WindKeys;
};
FState State;
}

bool HouseCalderTrees::Enabled()
{
    // Candidate rollout stays explicit until physical/visual/performance checks.
    return HouseCalderMapTrees::Enabled() || (HouseCalderLandscape::Enabled()
        && FParse::Param(FCommandLine::Get(),TEXT("CalderWoodsTreeTrial"))
        && !FParse::Param(FCommandLine::Get(),TEXT("NoCalderTrees")));
}

UStaticMesh* HouseCalderTrees::WoodsMesh(AActor* Owner,UHouseBlackHallSubsystem* Look,
    const FString& Name,UStaticMesh* Original,const TArray<FTransform>& Instances)
{
    if (HouseCalderMapTrees::Enabled()) return HouseCalderMapTrees::Mesh(Owner,Look,Name,Original,Instances);
    if (!Enabled() || !Owner || !Look || !Original ||
        (Name!=TEXT("ExteriorWoodsPassMaple0") && Name!=TEXT("ExteriorWoodsPassMaple1"))) return Original;
    auto& State=HouseCalderTreesImpl::State;
    if (State.World.Get()!=Owner->GetWorld())
    { State=HouseCalderTreesImpl::FState();State.World=Owner->GetWorld(); }
    if (const auto* Found=State.Fitted.Find(Name)) if (Found->IsValid()) return Found->Get();
    if (!State.Prepared.IsValid())
    {
        UStaticMesh* Leaves=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeCandidate_Maple.SM_CalderTreeCandidate_Maple"));
        UStaticMesh* Wood=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Trees/SM_CalderTreeWood_Maple.SM_CalderTreeWood_Maple"));
        State.Prepared=HouseCalderTreeMeshes::RetainStructure(Look,Leaves,Wood);
    }
    UStaticMesh* Mesh=HouseCalderTreeMeshes::FitToOriginal(Look,State.Prepared.Get(),Original);
    if (!Mesh)
    {
        UE_LOG(LogTemp,Error,TEXT("CALDER_WOODS_TREE_FAILED name=%s retainingOriginal=1"),*Name);
        return Original;
    }
    const bool Gold=Name.EndsWith(TEXT("1"));
    const FName WindKey(Gold?TEXT("CalderMapleLeafTwoSided1"):TEXT("CalderMapleLeafTwoSided0"));
    int32 LeafSlots=0;
    for (int32 Slot=0;Slot<Mesh->GetStaticMaterials().Num();++Slot)
        if (UMaterialInterface* Parent=Mesh->GetMaterial(Slot))
            if (Parent->GetName().Contains(TEXT("TwoSided")))
            {
                UMaterialInstanceDynamic* Material=UMaterialInstanceDynamic::Create(Parent,Look,WindKey);
                Material->SetScalarParameterValue(TEXT("WindStrength"),1.3f);
                Material->SetVectorParameterValue(TEXT("AutumnTint"),Gold?FLinearColor(.48f,.23f,.035f):FLinearColor(.52f,.09f,.016f));
                Look->Materials.Add(WindKey,Material);
                // This is an owned transient mesh. UStaticMesh::SetMaterial is
                // an editor transaction/rebuild path; assign the runtime slot
                // directly so initialized render resources stay intact.
                Mesh->GetStaticMaterials()[Slot].MaterialInterface=Material;
                ++LeafSlots;
            }
    if (LeafSlots!=1) return Original;
    State.WindKeys.AddUnique(WindKey);State.Fitted.Add(Name,Mesh);
    const auto& Lods=Mesh->GetRenderData()->LODResources;
    UE_LOG(LogTemp,Display,TEXT("CALDER_WOODS_TREE name=%s instances=%d triangles=%d/%d/%d originalBodyUnchanged=1 instanceInputUnchanged=1 windKey=%s trial=1"),
        *Name,Instances.Num(),Lods[0].GetNumTriangles(),Lods[1].GetNumTriangles(),Lods[2].GetNumTriangles(),*WindKey.ToString());
    return Mesh;
}

void HouseCalderTrees::AppendWindKeys(TArray<FName>& Keys)
{
    HouseCalderMapTrees::AppendWindKeys(Keys);
    if (Enabled()) for (const FName Key:HouseCalderTreesImpl::State.WindKeys) Keys.AddUnique(Key);
}
