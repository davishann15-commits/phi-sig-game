#if !UE_BUILD_SHIPPING
#include "HouseCalderRidgeForest.h"
#include "HouseCalderFarLand.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "StaticMeshResources.h"

namespace HouseCalderRidgeForestReviewImpl
{
TWeakObjectPtr<UWorld> LastWorld;
bool Done=false;
void Tick(UWorld* World,ELevelTick,float)
{
    if(!World||!World->IsGameWorld()||!World->HasBegunPlay()
        || !FParse::Param(FCommandLine::Get(),TEXT("CalderRidgeForestReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    if(LastWorld.Get()!=World){LastWorld=World;Done=false;}
    if(Done||World->GetTimeSeconds()<12.f)return;
    Done=true;
    int32 Tiles=0,AllCalderISMs=0,Failures=0,Vertices=0,Valley=0,Middle=0,High=0,ValleyLOD0=0,GreenLOD0=0;
    double LowestOffset=TNumericLimits<double>::Max(),HighestOffset=TNumericLimits<double>::Lowest();
    FString Csv=TEXT("tile,lod,vertex,x,y,z,overSurfaceCm,r,g,b,a\n");
    if(!HouseCalderRidgeForest::Enabled())++Failures;
    for(TActorIterator<AActor> Actor(World);Actor;++Actor)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*Actor);
        for(UInstancedStaticMeshComponent* Part:Components)
        {
            if(Part->GetName().StartsWith(TEXT("Calder")))++AllCalderISMs;
            if(!Part->GetName().StartsWith(TEXT("CalderRidgeCanopy_")))continue;
            ++Tiles;
            UStaticMesh* Mesh=Part->GetStaticMesh();const FStaticMeshRenderData* Data=Mesh?Mesh->GetRenderData():nullptr;
            if(Cast<UHierarchicalInstancedStaticMeshComponent>(Part)||!Data||Data->LODResources.Num()!=3
                || !Mesh->bAllowCPUAccess||Part->GetInstanceCount()!=1||Part->CastShadow
                || Part->bAffectDistanceFieldLighting||Part->bAffectDynamicIndirectLighting
                || Part->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
                || Part->InstanceStartCullDistance!=0||Part->InstanceEndCullDistance!=900000){++Failures;continue;}
            FTransform Pose;Part->GetInstanceTransform(0,Pose,true);
            if(!Pose.Equals(FTransform::Identity,.0001))++Failures;
            if(!(Data->LODResources[0].GetNumTriangles()>Data->LODResources[1].GetNumTriangles()
                && Data->LODResources[1].GetNumTriangles()>Data->LODResources[2].GetNumTriangles()))++Failures;
            for(int32 LOD=0;LOD<3;++LOD)
            {
                const auto& Buffer=Data->LODResources[LOD].VertexBuffers;
                if(Buffer.ColorVertexBuffer.GetNumVertices()!=Buffer.PositionVertexBuffer.GetNumVertices()){++Failures;continue;}
                for(uint32 Index=0;Index<Buffer.PositionVertexBuffer.GetNumVertices();++Index)
                {
                    const FVector Position=Pose.TransformPosition(FVector(Buffer.PositionVertexBuffer.VertexPosition(Index)));
                    const FColor Colour=Buffer.ColorVertexBuffer.VertexColor(Index);
                    const double Surface=HouseCalderFarLand::SkylineGround(Position.X,Position.Y),Offset=Position.Z-Surface;
                    ++Vertices;LowestOffset=FMath::Min(LowestOffset,Offset);HighestOffset=FMath::Max(HighestOffset,Offset);
                    if(!FMath::IsFinite(Offset)||Offset<0.||Offset>2200.
                        || (Colour.A>0 && FMath::Max(FMath::Abs(Position.X),FMath::Abs(Position.Y))<40000.))++Failures;
                    if(Colour.A>0){if(Surface<4600.)++Valley;else if(Surface<23100.)++Middle;else++High;}
                    if(LOD==0 && Colour.A>0 && Surface<4600.){++ValleyLOD0;GreenLOD0+=int32(Colour.G>Colour.R);}
                    Csv+=FString::Printf(TEXT("%s,%d,%u,%.3f,%.3f,%.3f,%.4f,%u,%u,%u,%u\n"),
                        *Part->GetName(),LOD,Index,Position.X,Position.Y,Position.Z,Offset,Colour.R,Colour.G,Colour.B,Colour.A);
                }
            }
        }
    }
    if(Tiles!=12||AllCalderISMs>24||Vertices<1000||Valley==0||Middle==0||High==0)++Failures;
    const double GreenFraction=ValleyLOD0?double(GreenLOD0)/ValleyLOD0:0.;
    if(GreenFraction<.10 || GreenFraction>.25)++Failures;
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("CalderRidgeForestReview");IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Result=FString::Printf(TEXT("%s tiles=%d allCalderISMs=%d vertices=%d failures=%d offsetCm=%.4f..%.4f bands=%d/%d/%d valleyGreen=%.6f newCollision=0 defaultEnabled=0 visualPerformanceAcceptancePending=1\n"),
        Failures==0?TEXT("PASS"):TEXT("FAIL"),Tiles,AllCalderISMs,Vertices,Failures,LowestOffset,HighestOffset,Valley,Middle,High,GreenFraction);
    FFileHelper::SaveStringToFile(Csv,*(Directory/TEXT("vertices.csv")));FFileHelper::SaveStringToFile(Result,*(Directory/TEXT("result.txt")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_RIDGE_FOREST_REVIEW %s"),*Result);
    FPlatformMisc::RequestExitWithStatus(false,Failures==0?0:1);
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}};
FRegistration Registration;
}
#endif
