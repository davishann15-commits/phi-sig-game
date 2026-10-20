// Opt-in Phase2 ground-cover checkpoint. This does not claim that the full
// landscape palette, forests, farm or macro-detail specification is complete.
#if !UE_BUILD_SHIPPING
#include "HouseCalderLandscape.h"
#include "HouseCalderFarm.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

namespace HouseCalderLandscapeReviewImpl
{
TWeakObjectPtr<UWorld> LastWorld;
bool bDone=false;
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderLandscapeReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    if (LastWorld.Get()!=World) { LastWorld=World; bDone=false; }
    if (bDone || World->GetTimeSeconds()<10.f || !HouseCalderTerrain::CollisionReady(World)) return;
    const bool bOn=HouseCalderLandscape::Enabled() && World->GetNetMode()!=NM_DedicatedServer;
    if (bOn)
    {
        bool bBuilt=false;
        for (TActorIterator<AActor> It(World);It;++It) bBuilt |= It->ActorHasTag(TEXT("CalderCoverBuilt"));
        if (!bBuilt && World->GetTimeSeconds()<90.f) return;
    }
    bDone=true;
    int32 AllCalderISMs=0,FarmFootprints=0,Batches=0,Instances=0,BadSettings=0,BadPlacement=0,FrozenHits=0,WaterFootprints=0,MaxTriangles=0;
    float WorstGap=0.f;
    FString Csv=TEXT("batch,instance,x,y,z,gap,hit\n");
    for (TActorIterator<AActor> It(World);It;++It)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
        for (UInstancedStaticMeshComponent* Component : Components)
        {
            if (Component->GetName().StartsWith(TEXT("Calder"))) ++AllCalderISMs;
            if (!Component->GetName().StartsWith(TEXT("CalderCover_"))) continue;
            ++Batches;
            const UStaticMesh* Mesh=Component->GetStaticMesh();
            const FStaticMeshRenderData* Data=Mesh?Mesh->GetRenderData():nullptr;
            const int32 Triangles=Data && Data->LODResources.Num()?Data->LODResources[0].GetNumTriangles():0;
            MaxTriangles=FMath::Max(MaxTriangles,Triangles);
            if (Cast<UHierarchicalInstancedStaticMeshComponent>(Component) || !Mesh || Triangles<=0 || Triangles>60
                || Component->CastShadow || Component->bAffectDistanceFieldLighting || Component->bAffectDynamicIndirectLighting
                || Component->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
                || Component->InstanceEndCullDistance<2500 || Component->InstanceEndCullDistance>4500) ++BadSettings;
            for (int32 I=0;I<Component->GetInstanceCount();++I)
            {
                ++Instances; FTransform T; Component->GetInstanceTransform(I,T,true);
                const FVector Root=T.GetLocation();
                const FBox Bounds=Mesh->GetBoundingBox().TransformBy(T);
                const FBox2D Footprint(FVector2D(Bounds.Min),FVector2D(Bounds.Max));
                const double Extent=FMath::Max(FMath::Max(FMath::Abs(Bounds.Min.X-Root.X),FMath::Abs(Bounds.Max.X-Root.X)),
                    FMath::Max(FMath::Abs(Bounds.Min.Y-Root.Y),FMath::Abs(Bounds.Max.Y-Root.Y)));
                if (HouseCalderFarm::Occupied(FVector2D(Root),Extent)) ++FarmFootprints;
                for (const FBox2D& Frozen : HouseCalderTerrain::FrozenRects()) if (Frozen.Intersect(Footprint)) ++FrozenHits;
                bool bWet=false;
                for (const double X : {Bounds.Min.X,Root.X,Bounds.Max.X})
                    for (const double Y : {Bounds.Min.Y,Root.Y,Bounds.Max.Y})
                    {
                        float WaterZ=0.f;
                        bWet |= HouseCalderWater::WaterAt(FVector2D(X,Y),WaterZ) || HouseCalderWater::DeckAt(FVector2D(X,Y),WaterZ);
                    }
                if (bWet) ++WaterFootprints;
                FHitResult Hit;
                const bool bHit=World->LineTraceSingleByChannel(Hit,Root+FVector(0,0,120),Root-FVector(0,0,120),ECC_Visibility,
                    FCollisionQueryParams(SCENE_QUERY_STAT(CalderCoverReview),true));
                const FString Name=Hit.GetComponent()?Hit.GetComponent()->GetName():TEXT("none");
                const float Gap=bHit?float(Root.Z-Hit.ImpactPoint.Z):1000.f;
                WorstGap=FMath::Max(WorstGap,FMath::Abs(Gap));
                if (!bHit || !Name.StartsWith(TEXT("CalderTerrainCollision_")) || FMath::Abs(Gap)>3.f || Hit.ImpactNormal.Z<.8f) ++BadPlacement;
                Csv+=FString::Printf(TEXT("%s,%d,%.3f,%.3f,%.3f,%.3f,%s\n"),*Component->GetName(),I,Root.X,Root.Y,Root.Z,Gap,*Name);
            }
        }
    }
    const bool bPass=AllCalderISMs<=24 && FarmFootprints==0 && BadSettings==0 && BadPlacement==0 && FrozenHits==0 && WaterFootprints==0 && Instances<=120000 && Batches<=24
        && (bOn?(Batches==4 && Instances>0):(Instances==0 && Batches==0));
    const FString Summary=FString::Printf(TEXT("CALDER_LANDSCAPE_REVIEW %s scope=groundCoverCheckpoint wholePhase2Complete=0 enabled=%d batches=%d instances=%d maxTuftTriangles=%d badSettings=%d badPlacement=%d frozenHits=%d waterFootprints=%d worstRootGapCm=%.3f allCalderISMs=%d farmFootprints=%d\n"),
        bPass?TEXT("PASS"):TEXT("FAIL"),int32(bOn),Batches,Instances,MaxTriangles,BadSettings,BadPlacement,FrozenHits,WaterFootprints,WorstGap,AllCalderISMs,FarmFootprints);
    UE_LOG(LogTemp,Display,TEXT("%s"),*Summary);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderLandscapeReview");
    IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Summary,*(Dir/TEXT("result.txt")));
    FFileHelper::SaveStringToFile(Csv,*(Dir/TEXT("placements.csv")));
    if (!bPass || !FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorViews"))) FPlatformMisc::RequestExitWithStatus(false,bPass?0:1);
}
struct FRegistration
{
    FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); }
};
FRegistration Registration;
}
#endif
