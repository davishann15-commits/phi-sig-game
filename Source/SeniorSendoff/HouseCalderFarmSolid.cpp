#include "HouseCalderFarmSolid.h"
#include "HouseCalderFarm.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "StaticMeshResources.h"

namespace HouseCalderFarmSolidImpl
{
struct FPart
{
    TWeakObjectPtr<UProceduralMeshComponent> Component;
    TArray<FVector> WorldVertices;
    TArray<int32> Indices;
};
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AActor> Owner;
    TArray<FPart> Parts;
    uint32 Crc=0;
    bool Reviewed=false;
};
FState State;

void Append(FPart& Target,const FHallMesh& Mesh,const FTransform& Pose)
{
    for (const FName Key:Mesh.Order)
    {
        const FHallSection& Section=Mesh.Sections.FindChecked(Key);
        const int32 Base=Target.WorldVertices.Num();
        for (const FVector& Point:Section.V) Target.WorldVertices.Add(Pose.TransformPosition(Point));
        for (int32 I=0;I+2<Section.I.Num();I+=3)
        {
            const FVector A=Target.WorldVertices[Base+Section.I[I]],B=Target.WorldVertices[Base+Section.I[I+1]],C=Target.WorldVertices[Base+Section.I[I+2]];
            if (FVector::CrossProduct(B-A,C-A).SizeSquared()<1e-8) continue;
            Target.Indices.Append({Base+Section.I[I],Base+Section.I[I+1],Base+Section.I[I+2]});
        }
    }
}

void Install(AActor* Owner,FPart&& Data,const TCHAR* Name)
{
    UProceduralMeshComponent* Part=NewObject<UProceduralMeshComponent>(Owner,Name,RF_Transient);
    Owner->AddInstanceComponent(Part);Part->SetupAttachment(Owner->GetRootComponent());Part->SetMobility(EComponentMobility::Static);
    Part->bUseComplexAsSimpleCollision=true;Part->bUseAsyncCooking=false;
    Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);Part->SetGenerateOverlapEvents(false);
    Part->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable,0.f));
    Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;
    Part->SetCanEverAffectNavigation(false);Part->SetComponentTickEnabled(false);Part->SetVisibility(false);Part->SetHiddenInGame(true);
    Part->RegisterComponent();TArray<FVector> Local;Local.Reserve(Data.WorldVertices.Num());
    for (const FVector& Point:Data.WorldVertices) Local.Add(Part->GetComponentTransform().InverseTransformPosition(Point));
    Part->CreateMeshSection_LinearColor(0,Local,Data.Indices,TArray<FVector>(),TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>(),true);
    Data.Component=Part;
    for (const FVector& Point:Data.WorldVertices)
    {
        const int32 Packed[]={FMath::RoundToInt(Point.X*100),FMath::RoundToInt(Point.Y*100),FMath::RoundToInt(Point.Z*100)};
        State.Crc=FCrc::MemCrc32(Packed,sizeof(Packed),State.Crc);
    }
    State.Crc=FCrc::MemCrc32(Data.Indices.GetData(),Data.Indices.Num()*sizeof(int32),State.Crc);
    State.Parts.Add(MoveTemp(Data));
}
}

bool HouseCalderFarmSolid::Enabled()
{
    return HouseCalderFarm::Enabled() && FParse::Param(FCommandLine::Get(),TEXT("CalderFarmSolidTrial"));
}

void HouseCalderFarmSolid::Build(AActor* Owner,const FHallMesh& Boards,const FHallMesh& Roof,
    const FHallMesh& Fence,const FHallMesh& Hay,const TArray<FTransform>& Bales)
{
    if (!Owner || !Owner->GetWorld() || !Enabled()) return;
    namespace SI=HouseCalderFarmSolidImpl;
    if (SI::State.World.Get()!=Owner->GetWorld()) {SI::State=SI::FState();SI::State.World=Owner->GetWorld();}
    if (SI::State.Parts.Num()) return;
    SI::State.Owner=Owner;const double Start=FPlatformTime::Seconds();
    SI::FPart BarnPart;SI::Append(BarnPart,Boards,FTransform::Identity);SI::Append(BarnPart,Roof,FTransform::Identity);
    SI::Install(Owner,MoveTemp(BarnPart),TEXT("CalderFarmSolidBarn"));
    SI::FPart FencePart;SI::Append(FencePart,Fence,FTransform::Identity);SI::Install(Owner,MoveTemp(FencePart),TEXT("CalderFarmSolidFence"));
    SI::FPart BalePart;for (const FTransform& Pose:Bales) SI::Append(BalePart,Hay,Pose);
    SI::Install(Owner,MoveTemp(BalePart),TEXT("CalderFarmSolidBales"));
    int32 Triangles=0;for (const SI::FPart& Part:SI::State.Parts) Triangles+=Part.Indices.Num()/3;
    UE_LOG(LogTemp,Display,TEXT("CALDER_FARM_SOLID built parts=%d triangles=%d bales=%d crc=%08x dedicated=%d additionalISMs=0 ms=%.1f"),
        SI::State.Parts.Num(),Triangles,Bales.Num(),SI::State.Crc,int32(Owner->GetNetMode()==NM_DedicatedServer),(FPlatformTime::Seconds()-Start)*1000);
}

#if !UE_BUILD_SHIPPING
namespace HouseCalderFarmSolidReviewImpl
{
FIntVector Bucket(const FVector& Point)
{return FIntVector(FMath::FloorToInt(Point.X*20),FMath::FloorToInt(Point.Y*20),FMath::FloorToInt(Point.Z*20));}
void Tick(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderFarmSolidReview"))) return;
    namespace SI=HouseCalderFarmSolidImpl;auto& State=SI::State;
    if (State.World.Get()!=World) {State=SI::FState();State.World=World;}
    if (State.Reviewed || World->GetTimeSeconds()<9 || (HouseCalderTerrain::Enabled() && !HouseCalderTerrain::CollisionReady(World))) return;
    State.Reviewed=true;const bool On=HouseCalderFarmSolid::Enabled(),Server=World->GetNetMode()==NM_DedicatedServer;
    int32 Failures=0,Samples=0,Hits=0,Frozen=0,Unmatched=0,Vertices=0;float Worst=0;
    TMap<FIntVector,TArray<FVector>> DrawVertices;
    if (On && !Server && State.Owner.IsValid())
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(State.Owner.Get());
        for (UInstancedStaticMeshComponent* Part:Parts)
        {
            if (!Part->GetName().StartsWith(TEXT("CalderFarm_"))) continue;
            const UStaticMesh* Mesh=Part->GetStaticMesh();const FStaticMeshRenderData* Data=Mesh?Mesh->GetRenderData():nullptr;
            if (!Mesh || !Mesh->bAllowCPUAccess || !Data || !Data->LODResources.Num()) {++Failures;continue;}
            const auto& Positions=Data->LODResources[0].VertexBuffers.PositionVertexBuffer;
            for (int32 I=0;I<Part->GetInstanceCount();++I)
            {
                FTransform Pose;Part->GetInstanceTransform(I,Pose,true);
                for (uint32 V=0;V<Positions.GetNumVertices();++V)
                {const FVector Point=Pose.TransformPosition(FVector(Positions.VertexPosition(V)));DrawVertices.FindOrAdd(Bucket(Point)).Add(Point);}
            }
        }
    }
    FString Contacts=TEXT("part,sample,x,y,z,hit,errorCm\n"),Faces=TEXT("part,vertex,x,y,z\n");
    if (On && State.Parts.Num()!=3) ++Failures;
    if (!On && State.Parts.Num()) ++Failures;
    for (const SI::FPart& Record:State.Parts)
    {
        UProceduralMeshComponent* Part=Record.Component.Get();
        if (!Part || Part->GetCollisionEnabled()!=ECollisionEnabled::QueryAndPhysics || Part->CastShadow || Part->CanEverAffectNavigation()) {++Failures;continue;}
        for (int32 I=0;I<Record.WorldVertices.Num();++I)
        {
            const FVector Point=Record.WorldVertices[I];++Vertices;
            if (HouseCalderTerrain::InFrozen(Point.X,Point.Y)) ++Frozen;
            Faces+=FString::Printf(TEXT("%s,%d,%.6f,%.6f,%.6f\n"),*Part->GetName(),I,Point.X,Point.Y,Point.Z);
            if (!Server)
            {
                bool Matched=false;const FIntVector Key=Bucket(Point);
                for (int32 Z=-1;Z<=1 && !Matched;++Z) for (int32 Y=-1;Y<=1 && !Matched;++Y) for (int32 X=-1;X<=1 && !Matched;++X)
                    if (const TArray<FVector>* Found=DrawVertices.Find(Key+FIntVector(X,Y,Z)))
                        for (const FVector& Candidate:*Found) if (FVector::DistSquared(Point,Candidate)<.02*.02) {Matched=true;break;}
                if (!Matched) ++Unmatched;
            }
        }
        const int32 Step=FMath::Max(1,(Record.Indices.Num()/3+599)/600);
        for (int32 I=0;I+2<Record.Indices.Num();I+=Step*3)
        {
            const FVector A=Record.WorldVertices[Record.Indices[I]],B=Record.WorldVertices[Record.Indices[I+1]],C=Record.WorldVertices[Record.Indices[I+2]];
            const FVector Centre=(A+B+C)/3,Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
            if (Centre.Z<HouseExteriorSite::GroundHeight(Centre.X,Centre.Y)+5) continue;
            FHitResult Hit;const bool Found=Part->LineTraceComponent(Hit,Centre+Normal*.25,Centre-Normal*.25,FCollisionQueryParams(SCENE_QUERY_STAT(CalderFarmSolid),true));
            ++Samples;Hits+=int32(Found);const float Error=Found?FVector::Distance(Hit.ImpactPoint,Centre):1000;Worst=FMath::Max(Worst,Error);
            if (!Found || Error>.5f) ++Failures;
            Contacts+=FString::Printf(TEXT("%s,%d,%.4f,%.4f,%.4f,%d,%.4f\n"),*Part->GetName(),Samples,Centre.X,Centre.Y,Centre.Z,int32(Found),Error);
        }
    }
    if (Frozen || Unmatched || (On && Samples==0)) ++Failures;
    const FString Result=FString::Printf(TEXT("CALDER_FARM_SOLID_REVIEW %s enabled=%d dedicated=%d parts=%d vertices=%d unmatchedRenderVertices=%d frozenHits=%d contacts=%d hits=%d maxContactErrorCm=%.4f crc=%08x failures=%d routesPending=1\n"),
        Failures?TEXT("FAIL"):TEXT("PASS"),int32(On),int32(Server),State.Parts.Num(),Vertices,Unmatched,Frozen,Samples,Hits,Worst,State.Crc,Failures);
    UE_LOG(LogTemp,Display,TEXT("%s"),*Result);const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderFarmSolidReview");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(Contacts,*(Dir/TEXT("contacts.csv")));FFileHelper::SaveStringToFile(Faces,*(Dir/TEXT("vertices.csv")));
    if (Failures) FPlatformMisc::RequestExitWithStatus(false,1);
}
struct FRegistration {FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}} Registration;
}
#endif
