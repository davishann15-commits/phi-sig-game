#if !UE_BUILD_SHIPPING
#include "HouseCalderTrees.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshResources.h"

namespace HouseCalderTreeFitReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    double Start=0.;
    float WindMin=TNumericLimits<float>::Max(),WindMax=TNumericLimits<float>::Lowest();
    bool Done=false;
};
FState State;
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay()
        || !FParse::Param(FCommandLine::Get(),TEXT("CalderTreeFitReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    if (State.World.Get()!=World) { State=FState();State.World=World;State.Start=World->GetTimeSeconds(); }
    if (State.Done) return;
    TArray<UHierarchicalInstancedStaticMeshComponent*> Batches;
    for (TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(*Actor);
        for (auto* Component:Components)
            if (Component->GetName()==TEXT("ExteriorWoodsPassMaple0") || Component->GetName()==TEXT("ExteriorWoodsPassMaple1"))
            {
                Batches.Add(Component);
                for (UMaterialInterface* Material:Component->GetMaterials())
                    if (Material && Material->GetName().StartsWith(TEXT("CalderMapleLeafTwoSided")))
                    {
                        float Wind=0.f;
                        if (Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("WindStrength")),Wind))
                        { State.WindMin=FMath::Min(State.WindMin,Wind);State.WindMax=FMath::Max(State.WindMax,Wind); }
                    }
            }
    }
    if (World->GetTimeSeconds()-State.Start<12.) return;
    State.Done=true;
    Batches.Sort([](const auto& A,const auto& B){return A.GetName()<B.GetName();});
    const bool Trial=HouseCalderTrees::Enabled();
    int32 Instances=0,Replacements=0,Failures=0,Samples=0;
    FString Poses=TEXT("batch,index,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");
    FString Contacts=TEXT("batch,index,axis,sign,z,hit,item,localErrorCm\n");
    if (Batches.Num()!=2) ++Failures;
    for (auto* Batch:Batches)
    {
        UStaticMesh* Mesh=Batch->GetStaticMesh();
        if (!Mesh || !Mesh->GetBodySetup() || !Mesh->GetRenderData()) { ++Failures;continue; }
        const bool Replaced=Mesh->GetName().StartsWith(TEXT("CalderWoodsFittedTree"));
        Replacements+=int32(Replaced);
        const UBodySetup* Body=Mesh->GetBodySetup();
        if (Replaced!=Trial || Body->CollisionTraceFlag!=CTF_UseSimpleAsComplex || Body->AggGeom.BoxElems.Num()!=1
            || !Body->AggGeom.SphereElems.IsEmpty() || !Body->AggGeom.SphylElems.IsEmpty() || !Body->AggGeom.ConvexElems.IsEmpty()
            || Batch->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || Batch->CastShadow
            || Batch->bAffectDistanceFieldLighting || Batch->bAffectDynamicIndirectLighting
            || Batch->InstanceStartCullDistance!=18000 || Batch->InstanceEndCullDistance!=22500
            || Mesh->GetRenderData()->LODResources.Num()!=3) ++Failures;
        if (Body->AggGeom.BoxElems.Num()!=1) continue;
        const FKBoxElem& Box=Body->AggGeom.BoxElems[0];
        if (Box.X!=60.f || Box.Y!=60.f || Box.Z!=400.f || !Box.Center.Equals(FVector(0,0,200),0.)
            || !Box.Rotation.Equals(FRotator::ZeroRotator,0.)) ++Failures;
        const int32 Expected=Batch->GetName().EndsWith(TEXT("0"))?35:30;
        if (Batch->GetInstanceCount()!=Expected) ++Failures;
        for (int32 Index=0;Index<Batch->GetInstanceCount();++Index)
        {
            FTransform Pose;
            if (!Batch->GetInstanceTransform(Index,Pose,true)) { ++Failures;continue; }
            ++Instances;
            const FVector P=Pose.GetTranslation(),S=Pose.GetScale3D();const FQuat Q=Pose.GetRotation();
            Poses+=FString::Printf(TEXT("%s,%d,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n"),
                *Batch->GetName(),Index,P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
            for (const float Z:{60.f,160.f,320.f}) for (int32 Axis=0;Axis<2;++Axis) for (const float Sign:{-1.f,1.f})
            {
                FVector Outside(0,0,Z),Inside(0,0,Z);Outside[Axis]=Sign*50.f;Inside[Axis]=Sign*10.f;
                FHitResult Hit;
                const bool Blocked=Batch->LineTraceComponent(Hit,Pose.TransformPosition(Outside),Pose.TransformPosition(Inside),
                    FCollisionQueryParams(SCENE_QUERY_STAT(CalderTreeFit),false));
                const FVector Local=Blocked?Pose.InverseTransformPosition(Hit.ImpactPoint):FVector(100000,100000,100000);
                FVector ExpectedPoint(0,0,Z);ExpectedPoint[Axis]=Sign*30.f;
                const double Error=FVector::Dist(Local,ExpectedPoint);
                if (!Blocked || Hit.Item!=Index || Error>.5) ++Failures;
                ++Samples;
                Contacts+=FString::Printf(TEXT("%s,%d,%d,%.0f,%.0f,%d,%d,%.6f\n"),
                    *Batch->GetName(),Index,Axis,Sign,Z,int32(Blocked),Hit.Item,Error);
            }
        }
    }
    if (Instances!=65 || Replacements!=(Trial?2:0) || Samples!=780) ++Failures;
    const float WindRange=State.WindMax-State.WindMin;
    if (Trial && (!FMath::IsFinite(WindRange) || WindRange<=.01f)) ++Failures;
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("CalderTreeFitReview");
    IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Result=FString::Printf(TEXT("%s trial=%d batches=%d replacements=%d instances=%d contactSamples=%d failures=%d windRange=%.6f originalPoseComparisonPending=1\n"),
        Failures==0?TEXT("PASS"):TEXT("FAIL"),int32(Trial),Batches.Num(),Replacements,Instances,Samples,Failures,Trial?WindRange:0.f);
    FFileHelper::SaveStringToFile(Poses,*(Directory/TEXT("poses.csv")));
    FFileHelper::SaveStringToFile(Contacts,*(Directory/TEXT("contacts.csv")));
    FFileHelper::SaveStringToFile(Result,*(Directory/TEXT("result.txt")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_TREE_FIT_REVIEW %s"),*Result);
    FPlatformMisc::RequestExitWithStatus(false,Failures==0?0:1);
}
struct FRegistration { FRegistration() { FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick); } };
FRegistration Registration;
}
#endif
