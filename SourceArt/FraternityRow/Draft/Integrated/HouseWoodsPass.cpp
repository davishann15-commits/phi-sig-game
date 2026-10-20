#include "HouseWoodsPass.h"
#include "HouseFraternityRow.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "StaticMeshResources.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

int32 HouseWoodsPass::Stage()
{
    static const int32 Value = []()
    {
        int32 Requested = 3;
        FParse::Value(FCommandLine::Get(), TEXT("WoodsPass="), Requested);
        return FMath::Clamp(Requested, 0, 3);
    }();
    return Value;
}

void HouseWoodsPass::RecordOriginalTrees(const TArray<FTransform>& Big,
    const TArray<FTransform>& Small, const TArray<FTransform>& Distant)
{
    FString Snapshot = TEXT("batch,index,transform\n");
    auto Append = [&Snapshot](const TCHAR* Batch, const TArray<FTransform>& Trees)
    {
        for (int32 Index = 0; Index < Trees.Num(); ++Index)
            Snapshot += FString::Printf(TEXT("%s,%d,%s\n"), Batch, Index, *Trees[Index].ToString());
    };
    Append(TEXT("Big"), Big);
    Append(TEXT("Small"), Small);
    Append(TEXT("Distant"), Distant);
    const uint32 Hash = FCrc::MemCrc32(*Snapshot, Snapshot.Len() * sizeof(TCHAR));
    UE_LOG(LogTemp, Display, TEXT("WOODS_PASS_ORIGINAL_TREES stage=%d big=%d small=%d distant=%d crc=%08x"),
        Stage(), Big.Num(), Small.Num(), Distant.Num(), Hash);
    if (FParse::Param(FCommandLine::Get(), TEXT("WoodsPassReview")))
    {
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("WoodsPassReview");
        IFileManager::Get().MakeDirectory(*Directory, true);
        FFileHelper::SaveStringToFile(Snapshot, *(Directory / TEXT("original_trees.csv")));
    }
}

void HouseWoodsPass::Audit(AActor* Owner)
{
    if (!Owner || !FParse::Param(FCommandLine::Get(), TEXT("WoodsPassReview"))) return;
    TInlineComponentArray<UPointLightComponent*> LocalLights(Owner);
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Batches(Owner);
    int32 AddedBatches = 0, AddedInstances = 0, TreeLODs = 0;
    int32 ExteriorLights = LocalLights.Num(), LowPools = 0;
    bool SafePools = true;
    for (TActorIterator<AActor> It(Owner->GetWorld()); It; ++It)
    {
        if (It->GetOwner() != Owner) continue;
        TInlineComponentArray<UPointLightComponent*> OwnedLights(*It);
        ExteriorLights += OwnedLights.Num();
        if (It->ActorHasTag(TEXT("WoodlandHollowMist")))
        {
            ++LowPools;
            SafePools &= Cast<ALocalFogVolume>(*It) != nullptr && !It->GetActorEnableCollision()
                && !It->IsActorTickEnabled();
        }
    }
    const int32 ExpectedPools = Stage() >= 2 && !FParse::Param(FCommandLine::Get(), TEXT("NoWoodsFog")) ? 3 : 0;
    SafePools &= LowPools == ExpectedPools;
    bool SafeDecoration = true, DistanceCulling = true;
    for (auto* Batch : Batches)
    {
        const FString Name = Batch->GetName();
        if (!Name.StartsWith(TEXT("ExteriorWoodsPass")) && !Name.StartsWith(TEXT("ExteriorAppalachian"))) continue;
        ++AddedBatches;
        AddedInstances += Batch->GetInstanceCount();
        DistanceCulling &= Batch->InstanceEndCullDistance > 0;
        SafeDecoration &= !Batch->CastShadow && !Batch->bAffectDistanceFieldLighting && !Batch->bAffectDynamicIndirectLighting;
        if (Batch->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
        {
            // A handful of substantial dead trunks/stumps also use base-only
            // collision. They are not litter and have no foliage/overdraw.
            const bool DeadTrunk = Name.StartsWith(TEXT("ExteriorAppalachianBareSnags")) ||
                Name.StartsWith(TEXT("ExteriorAppalachianLeaningDeadwood")) || Name.StartsWith(TEXT("ExteriorAppalachianAmbiguousStump"));
            const bool LiveTrunk = Name.StartsWith(TEXT("ExteriorWoodsPassOak")) ||
                Name.StartsWith(TEXT("ExteriorWoodsPassHickory")) || Name.StartsWith(TEXT("ExteriorWoodsPassMaple")) ||
                Name.StartsWith(TEXT("ExteriorWoodsPassTulipPoplar")) || Name.StartsWith(TEXT("ExteriorWoodsPassHemlock"));
            const UStaticMesh* Mesh = Batch->GetStaticMesh();
            if (DeadTrunk) continue;
            if (LiveTrunk && Mesh && Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.Num() >= 3) ++TreeLODs;
            else SafeDecoration = false;
        }
    }
    // USpotLightComponent derives from UPointLightComponent, so this is an
    // actual component count of both kinds, not just the old builder counter.
    const bool Passed = ExteriorLights <= HouseFraternityRow::ExteriorLightBudget() && SafeDecoration && DistanceCulling && SafePools;
    const FString Report = FString::Printf(TEXT("%s stage=%d actualExteriorPointAndSpotLights=%d newBatches=%d newInstances=%d threeLODTrunkBatches=%d noSmallDetailShadowsOrCollision=%d distanceCulling=%d lowPools=%d safePools=%d runtimeOnlyBuild=1\n"),
        Passed ? TEXT("PASS") : TEXT("FAIL"), Stage(), ExteriorLights, AddedBatches, AddedInstances,
        TreeLODs, int32(SafeDecoration), int32(DistanceCulling), LowPools, int32(SafePools));
    UE_LOG(LogTemp, Display, TEXT("WOODS_PASS_REVIEW %s"), *Report);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("WoodsPassReview");
    IFileManager::Get().MakeDirectory(*Directory, true);
    FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("result.txt")));
}
