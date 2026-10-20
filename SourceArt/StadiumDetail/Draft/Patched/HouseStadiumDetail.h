#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HouseBlackHallMesh.h"
#include "HouseStadiumDetail.generated.h"
class UHouseBlackHallSubsystem;
class UStaticMesh;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAudioComponent;
class USoundWaveProcedural;
class UInstancedStaticMeshComponent;
struct FHallImage;
namespace HouseStadiumDetail
{
bool Enabled();
int32 Step();
bool Aisle(int32 Segment);
bool Portal(int32 Segment);
bool PortalBand(int32 Segment,int32 Row);
bool CrossAisle(int32 Segment,int32 Row);
void CreateLook(UHouseBlackHallSubsystem* Look);
void WeatherField(FHallImage& Image);
/** Preserve tread material identity and every legacy branch when disabled. */
void Tier(FHallMesh& Mesh,int32 Segment,int32 Row,FName Stone);
void Rim(FHallMesh& Mesh,int32 Segment,FName Stone);
/** Existing STA loader owns these additions and frees them with its original interior. */
void Interiors(FHallMesh& Solid,FHallMesh& Clutter,FHallMesh& Glow);
void WoodLockers(FHallMesh& Solid,FHallMesh& Detail,FVector Origin,FVector Along,int32 Count,FVector Facing);
/** Matching collision for retained substantial field props, never for paper/chalk/grass. */
void FieldCollision(const FHallMesh& Visual,FHallMesh& Solid);
UStaticMesh* Kit(UHouseBlackHallSubsystem* Look,bool Tube);
UStaticMesh* PlankKit(UHouseBlackHallSubsystem* Look);
UStaticMesh* GrassKit(UHouseBlackHallSubsystem* Look);
}
UCLASS(NotPlaceable,Transient)
class SENIORSENDOFF_API AHouseStadiumDetail : public AActor
{
    GENERATED_BODY()
public:
    AHouseStadiumDetail();
    static void SpawnFor(AActor* Exterior,UHouseBlackHallSubsystem* Look);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Shared unit kits. Dimensions are full cm; all reachable substantial props collide. */
    void Box(FName Material,FVector Centre,FVector Dimensions,FRotator Rotation=FRotator::ZeroRotator,bool Solid=true,float Cull=20000.f);
    void Pipe(FName Material,FVector A,FVector B,float Radius=2.5f,bool Solid=true,float Cull=20000.f);
    void Plank(FName Material,FVector Centre,float Span,FRotator Rotation);
    void Sign(FName Material,FVector Centre,FVector Right,FVector Normal,float Width,float Height);
    void Letter(const FString& Word,FVector Centre,FVector Right,FVector Normal,float Height,FName Material=TEXT("SD_Cream"));
    void Rail(FVector A,FVector B,bool Middle=true);
    void FinishMesh(const FHallMesh& Geometry,FName Name,float Cull=30000.f);
    void Seating();void Structures();void Masts();void Field();void Concourse();void Mood();
    UHouseBlackHallSubsystem* Look=nullptr;
    FHallMesh Unique,Collision;
    int32 Instances=0,Triangles=0;
private:
    UHierarchicalInstancedStaticMeshComponent* Batch(FName Material,int32 Shape,bool Solid,float Cull);
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> BoxKit;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> PipeKit;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> BenchKit;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> OwnedMeshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Flickers;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Sounds;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundWaveProcedural>> Waves;
    UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Programs;
    TArray<FVector> ProgramOrigins;
    TArray<TArray<int16>> Pcm;
    double NextMood=0.;
    int64 LastHalyardCycle=-1;
};
