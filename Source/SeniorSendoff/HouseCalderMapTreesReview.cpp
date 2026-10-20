#if !UE_BUILD_SHIPPING
#include "HouseCalderMapTrees.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

namespace HouseCalderMapTreesReviewImpl
{
TWeakObjectPtr<UWorld> LastWorld;
bool Done=false;
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay()
        || !FParse::Param(FCommandLine::Get(),TEXT("CalderMapTreesReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    if (LastWorld.Get()!=World) { LastWorld=World;Done=false; }
    if (Done || World->GetTimeSeconds()<9.f) return;
    Done=true;
    const bool Enabled=HouseCalderMapTrees::Enabled();
    TArray<UInstancedStaticMeshComponent*> Batches;
    for (TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*Actor);
        for (auto* Part:Parts) if (HouseCalderMapTrees::TreeBatch(Part->GetName())) Batches.Add(Part);
    }
    Batches.Sort([](const auto& A,const auto& B){return A.GetName()<B.GetName();});
    int32 Failures=0,Instances=0,Legacy=0,Contacts=0,ReplacementBatches=0;
    double MaxError=0.;
    FString Poses=TEXT("batch,index,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");
    FString Bodies=TEXT("batch,box,x,y,z,pitch,yaw,roll,sx,sy,sz\n");
    FString Settings=TEXT("batch,count,collision,cullStart,cullEnd,shadow,distanceField,indirect,boxes,lods,mesh,tri0,tri1,tri2\n");
    for (auto* Batch:Batches)
    {
        UStaticMesh* Mesh=Batch->GetStaticMesh();
        if (!Mesh || !Mesh->GetBodySetup() || !Mesh->GetRenderData()) { ++Failures;continue; }
        const auto* Body=Mesh->GetBodySetup();const auto* Render=Mesh->GetRenderData();
        const bool Replaced=Mesh->GetName().StartsWith(TEXT("CalderWoodsFittedTree")) || Mesh->GetName().StartsWith(TEXT("CalderNewDeadwood"));
        ReplacementBatches+=int32(Replaced);Legacy+=int32(!Replaced);
        if (Enabled && (!Replaced || Render->LODResources.Num()!=3)) ++Failures;
        if (Replaced && (Render->LODResources[0].GetNumTriangles()<=Render->LODResources[1].GetNumTriangles()
            || Render->LODResources[1].GetNumTriangles()<=Render->LODResources[2].GetNumTriangles())) ++Failures;
        Settings+=FString::Printf(TEXT("%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d\n"),*Batch->GetName(),Batch->GetInstanceCount(),
            int32(Batch->GetCollisionEnabled()),Batch->InstanceStartCullDistance,Batch->InstanceEndCullDistance,int32(Batch->CastShadow),
            int32(Batch->bAffectDistanceFieldLighting),int32(Batch->bAffectDynamicIndirectLighting),Body->AggGeom.BoxElems.Num(),Render->LODResources.Num(),
            *Mesh->GetName(),Render->LODResources[0].GetNumTriangles(),Render->LODResources.Num()>1?Render->LODResources[1].GetNumTriangles():0,
            Render->LODResources.Num()>2?Render->LODResources[2].GetNumTriangles():0);
        for (int32 Index=0;Index<Body->AggGeom.BoxElems.Num();++Index)
        {
            const FKBoxElem& Box=Body->AggGeom.BoxElems[Index];
            Bodies+=FString::Printf(TEXT("%s,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"),*Batch->GetName(),Index,
                Box.Center.X,Box.Center.Y,Box.Center.Z,Box.Rotation.Pitch,Box.Rotation.Yaw,Box.Rotation.Roll,double(Box.X),double(Box.Y),double(Box.Z));
        }
        for (int32 Index=0;Index<Batch->GetInstanceCount();++Index)
        {
            FTransform Pose;Batch->GetInstanceTransform(Index,Pose,true);++Instances;
            const FVector P=Pose.GetLocation(),S=Pose.GetScale3D();const FQuat Q=Pose.GetRotation();
            Poses+=FString::Printf(TEXT("%s,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"),*Batch->GetName(),Index,
                P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
            if (Batch->GetCollisionEnabled()==ECollisionEnabled::NoCollision) continue;
            for (const FKBoxElem& Box:Body->AggGeom.BoxElems)
                for (const float Height:{-.3f,0.f,.3f}) for (int32 Axis=0;Axis<2;++Axis) for (const float Sign:{-1.f,1.f})
                {
                    const FTransform BoxPose(Box.Rotation,Box.Center);
                    const float Radius=(Axis==0?Box.X:Box.Y)*.5f;
                    FVector A(0,0,Height*Box.Z),B=A,Expected=A;
                    A[Axis]=Sign*(Radius+20.f);B[Axis]=Sign*FMath::Max(0.f,Radius-10.f);Expected[Axis]=Sign*Radius;
                    FHitResult Hit;
                    const bool Blocked=Batch->LineTraceComponent(Hit,Pose.TransformPosition(BoxPose.TransformPosition(A)),
                        Pose.TransformPosition(BoxPose.TransformPosition(B)),FCollisionQueryParams(SCENE_QUERY_STAT(CalderMapTreeContact),false));
                    const double Error=Blocked?FVector::Dist(Pose.InverseTransformPosition(Hit.ImpactPoint),BoxPose.TransformPosition(Expected)):100000.;
                    if (!Blocked || Hit.Item!=Index || Error>.5) ++Failures;
                    MaxError=FMath::Max(MaxError,Error);++Contacts;
                }
        }
    }
    // Baked tree-only material sections are also surveyed: trunks/cones must
    // not survive inside the town's combined skyline meshes.
    int32 LegacyTownSections=0,NewTownSections=0;
    for (TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UStaticMeshComponent*> Parts(*Actor);
        for (auto* Part:Parts) if (Part->GetName().StartsWith(TEXT("CampusPolishSkyline")))
            for (const FName Slot:Part->GetMaterialSlotNames())
            {
                LegacyTownSections+=int32(Slot==TEXT("CPT_Trunk") || Slot.ToString().StartsWith(TEXT("CPT_Leaf")));
                NewTownSections+=int32(Slot.ToString().StartsWith(TEXT("CalderTownTree_")));
            }
    }
    if (Enabled && LegacyTownSections>0) ++Failures;
    const FString Result=FString::Printf(TEXT("%s enabled=%d batches=%d replacements=%d instances=%d legacyBatches=%d legacyTownSections=%d newTownSections=%d contacts=%d maxErrorCm=%.6f failures=%d crossRunPoseBodySettingsComparisonPending=1\n"),
        Failures==0?TEXT("PASS"):TEXT("FAIL"),int32(Enabled),Batches.Num(),ReplacementBatches,Instances,Legacy,LegacyTownSections,NewTownSections,Contacts,MaxError,Failures);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderMapTreesReview");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Poses,*(Dir/TEXT("poses.csv")));FFileHelper::SaveStringToFile(Bodies,*(Dir/TEXT("bodies.csv")));
    FFileHelper::SaveStringToFile(Settings,*(Dir/TEXT("settings.csv")));FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_MAP_TREES_REVIEW %s"),*Result);
    if (Failures || (!FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorViews")) && !FParse::Param(FCommandLine::Get(),TEXT("CalderLandscapeReview"))))
        FPlatformMisc::RequestExitWithStatus(false,Failures==0?0:1);
}
struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
