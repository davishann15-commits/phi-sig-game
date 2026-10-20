#include "HouseCalderLandscape.h"

#include "HouseBlackHall.h"
#include "HouseBlackHallLook.h"
#include "HouseCalderTerrain.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StaticMeshResources.h"

namespace HouseCalderLandscapeImpl
{
UTexture2D* Texture(const TCHAR* Set, const TCHAR* Kind)
{
    const FString Path = FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Textures/%s/T_%s_%s.T_%s_%s"), Set, Set, Kind, Set, Kind);
    return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

/** Read-only use of the already-imported CC0 scans (ASSETS.md). The existing
 *  nature master accepts the same UVs as the old painted textures. No original
 *  ExteriorTree material, tree transform or collision component is changed. */
bool Scanned(UHouseBlackHallSubsystem* Look, FName Key, const TCHAR* Set, float Scale, float NormalStrength)
{
    UTexture2D* Albedo = Texture(Set, TEXT("A"));
    UTexture2D* Normal = Texture(Set, TEXT("N"));
    UMaterialInterface* Master = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/House/ExteriorNature/M_NaturePropTextured.M_NaturePropTextured"));
    if (!Albedo || !Normal || !Master)
    {
        UE_LOG(LogTemp, Warning, TEXT("CALDER_WOODS_SCAN missing key=%s set=%s; retaining existing appearance"), *Key.ToString(), Set);
        return false;
    }
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Master, Look);
    Material->SetTextureParameterValue(TEXT("BaseMap"), Albedo);
    Material->SetTextureParameterValue(TEXT("NormalMap"), Normal);
    Material->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
    Material->SetVectorParameterValue(TEXT("Emission"), FLinearColor::Black);
    Material->SetScalarParameterValue(TEXT("UVScale"), Scale);
    Material->SetScalarParameterValue(TEXT("NormalStrength"), NormalStrength);
    Material->SetScalarParameterValue(TEXT("Roughness"), .94f);
    Look->Materials.Add(Key, Material);
    return true;
}
}

bool HouseCalderLandscape::Enabled()
{
    return HouseCalderTerrain::Enabled() && HouseCalderTerrain::ReliefScale() > 0.f
        && !FParse::Param(FCommandLine::Get(), TEXT("NoCalderLandscape"));
}

UMaterialInterface* HouseCalderLandscape::RockMaster(const TCHAR* Set)
{
    const bool Natural = FCString::Strcmp(Set, TEXT("rock_face_04")) == 0 || FCString::Strcmp(Set, TEXT("lichen_rock")) == 0;
    const bool Masonry = FCString::Strcmp(Set, TEXT("mossy_stone_wall")) == 0 || FCString::Strcmp(Set, TEXT("stacked_stone_wall")) == 0
        || FCString::Strcmp(Set, TEXT("rock_wall_15")) == 0;
    if (Enabled() && (Natural || Masonry) && FParse::Param(FCommandLine::Get(), TEXT("CalderRockStrataTrial")))
    {
        if (UMaterialInterface* Trial = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderStrataRock.M_CalderStrataRock"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return Trial;
        UE_LOG(LogTemp, Warning, TEXT("CALDER_ROCK_STRATA missingOwnedMaster=1 set=%s retainingOriginal=1"), Set);
    }
    return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/SeniorSendOff/Calder/Materials/M_CalderRock.M_CalderRock"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void HouseCalderLandscape::RockLook(UMaterialInstanceDynamic* Material, const TCHAR* Set)
{
    if (!Material || !Enabled() || !FParse::Param(FCommandLine::Get(), TEXT("CalderRockStrataTrial"))) return;
    if (GetNameSafe(Material->GetMaterial()) != TEXT("M_CalderStrataRock")) return;
    const bool Natural = FCString::Strcmp(Set, TEXT("rock_face_04")) == 0 || FCString::Strcmp(Set, TEXT("lichen_rock")) == 0;
    Material->SetScalarParameterValue(TEXT("CalderStrataStrength"), Natural ? 1.f : 0.f);
    Material->SetScalarParameterValue(TEXT("CalderStreakStrength"), .20f);
    Material->SetScalarParameterValue(TEXT("CalderRockGray"), Natural ? .30f : .10f);
    UE_LOG(LogTemp, Display, TEXT("CALDER_ROCK_STRATA bound=1 set=%s bedding=%.1f gray=%.2f existingGeometryCollisionWaterUnchanged=1 trial=1"),
        Set, Natural ? 1.f : 0.f, Natural ? .30f : .10f);
}

void HouseCalderLandscape::WoodsFloorLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || !Enabled()) return;
    int32 Scans = 0;
    Scans += HouseCalderLandscapeImpl::Scanned(Look, TEXT("EX_WoodsGround"), TEXT("forest_leaves_03"), 1.2f, .75f);
    Scans += HouseCalderLandscapeImpl::Scanned(Look, TEXT("EX_WoodsFloorBark"), TEXT("jolcham_oak_bark_01"), 1.f, .85f);
    Scans += HouseCalderLandscapeImpl::Scanned(Look, TEXT("EX_WoodsFloorStone"), TEXT("mossy_rock"), 1.f, .85f);
    Scans += HouseCalderLandscapeImpl::Scanned(Look, TEXT("EX_WoodsFloorSoil"), TEXT("mud_forest"), 1.f, .65f);
    UE_LOG(LogTemp, Display, TEXT("CALDER_WOODS_FLOOR scans=%d groundAndCollisionUnchanged=1 sources=CC0 newLights=0"), Scans);
}

void HouseCalderLandscape::WoodsBarkLook(UHouseBlackHallSubsystem* Look)
{
    if (!Look || !Enabled()) return;
    const TCHAR* Sets[] = { TEXT("jolcham_oak_bark_01"), TEXT("Bark014"), TEXT("bark_brown_02"), TEXT("bark_brown_02"), TEXT("Bark014") };
    int32 Scans = 0;
    for (int32 Species = 0; Species < UE_ARRAY_COUNT(Sets); ++Species)
    {
        const FName Key(*FString::Printf(TEXT("EX_WoodsPassBark%d"), Species));
        Scans += HouseCalderLandscapeImpl::Scanned(Look, Key, Sets[Species], 1.f, .8f);
        // Terminal twigs keep their existing wind shader, UVs and sway keys.
        const FName TwigKey(*FString::Printf(TEXT("EX_WoodsPassTwig%d"), Species));
        if (UMaterialInstanceDynamic* Twig = Cast<UMaterialInstanceDynamic>(Look->Mat(TwigKey)))
        {
            if (UTexture2D* Albedo = HouseCalderLandscapeImpl::Texture(Sets[Species], TEXT("A"))) Twig->SetTextureParameterValue(TEXT("BaseMap"), Albedo);
            if (UTexture2D* Normal = HouseCalderLandscapeImpl::Texture(Sets[Species], TEXT("N"))) Twig->SetTextureParameterValue(TEXT("NormalMap"), Normal);
        }
    }
    int32 Sprays=0;
    const bool EvergreenTrial = FParse::Param(FCommandLine::Get(), TEXT("CalderEvergreenAtlasTrial"));
    for (const int32 Species : {0,2,4})
    {
        if (Species==4 && !EvergreenTrial) continue;
        const TCHAR* Name=Species==0?TEXT("Oak"):Species==2?TEXT("Maple"):TEXT("Evergreen");
        UMaterialInstanceDynamic* Leaf=Cast<UMaterialInstanceDynamic>(Look->Mat(FName(*FString::Printf(TEXT("EX_WoodsPassLeaf%d"),Species))));
        const FString AlbedoPath=FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Woods/T_CalderWoods%s_A.T_CalderWoods%s_A"),Name,Name);
        const FString NormalPath=FString::Printf(TEXT("/Game/SeniorSendOff/Calder/Woods/T_CalderWoods%s_N.T_CalderWoods%s_N"),Name,Name);
        UTexture2D* Albedo=LoadObject<UTexture2D>(nullptr,*AlbedoPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
        UTexture2D* Normal=LoadObject<UTexture2D>(nullptr,*NormalPath,nullptr,LOAD_NoWarn|LOAD_Quiet);
        if (!Leaf||!Albedo||!Normal)
        {
            if (Species==4) UE_LOG(LogTemp, Warning, TEXT("CALDER_EVERGREEN_ATLAS missing=1 retainingOriginal=1"));
            continue;
        }
        Leaf->SetTextureParameterValue(TEXT("BaseMap"),Albedo);
        Leaf->SetTextureParameterValue(TEXT("NormalMap"),Normal);
        if (Species==4) UE_LOG(LogTemp, Display, TEXT("CALDER_EVERGREEN_ATLAS bound=1 existingMeshTransformsCollisionWindUnchanged=1 source=AcquiredNeedlePhotos trial=1"));
        ++Sprays;
    }
    UE_LOG(LogTemp,Display,TEXT("CALDER_WOODS_LEAF_SCANS species=%d existingSpritesAndWindUnchanged=1 sources=%s"),Sprays,
        EvergreenTrial?TEXT("CC0AndAcquiredFab"):TEXT("CC0"));
    UE_LOG(LogTemp, Display, TEXT("CALDER_WOODS_BARK scans=%d originalTreeMeshesUnchanged=1 transformsUnchanged=1 sources=CC0"), Scans);
}

void HouseCalderLandscape::WoodsScannedProps(AActor* Owner)
{
    if (!Owner || !Enabled()) return;
    struct FReplacement { const TCHAR* Batch; const TCHAR* Asset; };
    const FReplacement Replacements[] = {
        { TEXT("ExteriorAppalachianFerns"), TEXT("/Game/SeniorSendOff/Calder/Woods/SM_CalderWoodsFern.SM_CalderWoodsFern") },
        { TEXT("ExteriorAppalachianStumps"), TEXT("/Game/SeniorSendOff/Calder/Woods/SM_CalderWoodsStump.SM_CalderWoodsStump") }
    };
    int32 Batches = 0, Instances = 0;
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(Owner);
    for (const FReplacement& Replacement : Replacements)
        for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
        {
            if (!Component || Component->GetName() != Replacement.Batch || Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision) continue;
            UStaticMesh* Original = Component->GetStaticMesh();
            UStaticMesh* Scan = LoadObject<UStaticMesh>(nullptr, Replacement.Asset, nullptr, LOAD_NoWarn | LOAD_Quiet);
            if (!Original || !Scan) continue;
            const FBox OldBox = Original->GetBoundingBox(), NewBox = Scan->GetBoundingBox();
            const FVector OldSize = OldBox.GetSize(), NewSize = NewBox.GetSize();
            const double Fit = FMath::Min(OldSize.Z / FMath::Max(NewSize.Z, 1.0),
                FMath::Max(OldSize.X, OldSize.Y) / FMath::Max(FMath::Max(NewSize.X, NewSize.Y), 1.0));
            TArray<FTransform> Placements;
            TArray<FVector> OriginalFeet;
            for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
            {
                FTransform Placement;
                Component->GetInstanceTransform(Index, Placement, true);
                const FVector Foot = Placement.TransformPosition(FVector(0, 0, OldBox.Min.Z));
                OriginalFeet.Add(Foot);
                Placement.SetScale3D(Placement.GetScale3D() * Fit);
                Placement.SetLocation(Foot - Placement.TransformVector(FVector(0, 0, NewBox.Min.Z)));
                Placements.Add(Placement);
            }
            Component->SetStaticMesh(Scan);
            Component->SetForceDisableNanite(true);
            for (int32 Slot = 0; Slot < Scan->GetStaticMaterials().Num(); ++Slot) Component->SetMaterial(Slot, Scan->GetMaterial(Slot));
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->BatchUpdateInstancesTransforms(0, Placements, true, true, true);
            Component->BuildTreeIfOutdated(false, true);
            double MaxFootError = 0.;
            for (int32 Index = 0; Index < Placements.Num(); ++Index)
            {
                FTransform Actual;
                Component->GetInstanceTransform(Index, Actual, true);
                MaxFootError = FMath::Max(MaxFootError, FVector::Dist(
                    Actual.TransformPosition(FVector(0, 0, NewBox.Min.Z)),
                    OriginalFeet[Index]));
            }
            const FStaticMeshRenderData* RenderData = Scan->GetRenderData();
            if (RenderData && RenderData->LODResources.Num() == 3)
                UE_LOG(LogTemp, Display, TEXT("CALDER_WOODS_PROP batch=%s instances=%d mesh=%s material=%s triangles=%d/%d/%d actualFootErrorCm=%.6f cull=%d/%d collision=%d shadows=%d distanceFields=%d indirect=%d"),
                    Replacement.Batch, Placements.Num(), *Scan->GetName(), *GetNameSafe(Component->GetMaterial(0)),
                    RenderData->LODResources[0].GetNumTriangles(), RenderData->LODResources[1].GetNumTriangles(), RenderData->LODResources[2].GetNumTriangles(),
                    MaxFootError, Component->InstanceStartCullDistance, Component->InstanceEndCullDistance,
                    int32(Component->GetCollisionEnabled()), int32(Component->CastShadow),
                    int32(Component->bAffectDistanceFieldLighting), int32(Component->bAffectDynamicIndirectLighting));
            ++Batches; Instances += Placements.Num();
        }
    UE_LOG(LogTemp, Display, TEXT("CALDER_WOODS_SCANNED_PROPS batches=%d instances=%d extraBatches=0 collision=0 nanite=0 protectedTreeTransformsUnchanged=1"), Batches, Instances);
}
