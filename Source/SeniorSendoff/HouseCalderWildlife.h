#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Engine/NetSerialization.h"
#include "HouseCalderWildlife.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class USoundWaveProcedural;
class USoundAttenuation;
class USoundSubmix;
class AHouseCalderWildlifeDirector;
struct FHouseCalderWildlifeRuntime;
struct FHouseCalderWildlifeRuntimeDeleter { void operator()(FHouseCalderWildlifeRuntime* Value) const; };
#if !UE_BUILD_SHIPPING
struct FCalderWildlifeSoleProbe
{
    uint8 Tier=0,PlantedMask=0;double PoseAge=0.,RouteSurfaceDifferenceCm=0.;
    FVector Centres[4]={};double Gaps[4]={};int64 Cycles[4]={};
};
struct FCalderGooseSoleProbe
{
    uint8 PlantedMask=0;int64 Cycles[2]={};double Stand=0.;
    TArray<FVector> Points[2];TArray<double> Gaps;
};
struct FCalderGooseAudioProbe
{
    int32 Buffers=0,Voices=0,PeakVoices=0,PeakSources=0,MaxSpecies=0,Hiss=0,Splash=0,Honk=0,Echo=0;
};
#endif

UENUM()
enum class ECalderWildlifeMood : uint8 { Feeding, Staring, Alarm, Fleeing, Returning, Walking };

USTRUCT()
struct FHouseCalderWildlifeState : public FFastArraySerializerItem
{
    GENERATED_BODY()
    UPROPERTY() uint16 Id=0;
    UPROPERTY() uint8 Species=0;
    UPROPERTY() uint8 Habitat=0;
    UPROPERTY() uint8 Variant=0;
    // 0=scene,1=direct player,2=40m alarm,3=explicit appearance setup.
    UPROPERTY() uint8 Cause=0;
    UPROPERTY() ECalderWildlifeMood Mood=ECalderWildlifeMood::Feeding;
    UPROPERTY() uint32 Seed=0;
    UPROPERTY() uint16 FacingYaw=0;
    UPROPERTY() float StartTime=0.f;
    UPROPERTY() ECalderWildlifeMood PreviousMood=ECalderWildlifeMood::Feeding;
    UPROPERTY() float PreviousPhase=0.f;
    UPROPERTY() FVector_NetQuantize10 From=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantize10 To=FVector::ZeroVector;
};

USTRUCT()
struct FHouseCalderWildlifeStates : public FFastArraySerializer
{
    GENERATED_BODY()
    UPROPERTY() TArray<FHouseCalderWildlifeState> Items;
    TWeakObjectPtr<AHouseCalderWildlifeDirector> Owner;
    bool IsPond=false;
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& Info)
    { return FastArrayDeltaSerialize<FHouseCalderWildlifeState,FHouseCalderWildlifeStates>(Items,Info,*this); }
    void PostReplicatedAdd(const TArrayView<int32> Added,int32 FinalSize);
    void PostReplicatedChange(const TArrayView<int32> Changed,int32 FinalSize);
};
template<> struct TStructOpsTypeTraits<FHouseCalderWildlifeStates> : public TStructOpsTypeTraitsBase2<FHouseCalderWildlifeStates>
{ enum { WithNetDeltaSerializer=true }; };

UCLASS(NotBlueprintable)
class SENIORSENDOFF_API AHouseCalderWildlifeDirector : public AActor
{
    GENERATED_BODY()
public:
    AHouseCalderWildlifeDirector();
    virtual ~AHouseCalderWildlifeDirector() override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(Replicated) FHouseCalderWildlifeStates States;
    UPROPERTY(Replicated) FHouseCalderWildlifeStates PondStates;
    void StateReceived(bool PondOnly=false);
    bool ForceSpecies(const FString& Species);
    bool Ready() const;
    FVector AnimalPosition(int32 Index,double Time=-1.) const;
    float AnimalReactionScale(int32 Index) const;
    double WildlifeTickP95Ms() const;
    void SetTimingView(FName View);
    USoundSubmix* WildlifeSoundSubmix()const{return SoundSubmix.Get();}
    bool WildlifeSoundSubmixRegistered()const;
#if !UE_BUILD_SHIPPING
    bool PlaceForBoundsReview(int32 Habitat,const FVector& Point);
    bool StartSoleReviewGroup(int32 Habitat);
    bool ReadRenderedSolesForReview(int32 Index,FCalderWildlifeSoleProbe& Out)const;
    bool SetBirdArtForReview(int32 Kind,double Phase,const FTransform& Root,int32 GoosePoseMode=-1);
    bool BirdArtFailedForReview()const{return BirdArtBuildFailed;}
    bool ReadSkyForReview(int32 Kind,int32 Slot,FTransform& Root,bool& Visible,double& Scene)const;
    bool SetSkySceneForReview(double Scene);
    bool ReadPondForReview(int32 Id,FTransform& Root,bool& Visible,int32& Phase,double& Raw)const;
    bool ReadPondSolesForReview(int32 Id,FCalderGooseSoleProbe& Out)const;
    bool ReadGooseAudioForReview(FCalderGooseAudioProbe& Out)const;
    uint32 ShoreHashForReview()const;
    double PondChangeWallForReview(int32 Id)const;
    uint32 PondHashForReview()const;
    bool PondReadyForReview()const;
    bool PondFailedForReview()const;
    bool SkyFailedForReview()const;
    uint32 SkyHashForReview()const;
    uint32 CrowRoostHashForReview()const;
#endif
    double HoofGapP95Cm() const;
    double HoofDriftP95Cm() const;
    int32 HoofGapSamples() const;
    int32 HoofDriftSamples() const;
    int32 ValidPathCount() const;
    int32 RenderComponentCount() const;
    uint32 HabitatHash() const;
    double StateChangeWallTime(int32 Index) const;
    const TArray<FVector>& HabitatNodes(int32 Group) const;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> DrawParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> OwnedMeshes;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> SoundVoices;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundWaveProcedural>> SoundWaves;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> SoundFalloff;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> BirdSoundFalloff;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> RavenSoundFalloff;
    UPROPERTY(Transient) TObjectPtr<USoundSubmix> SoundSubmix;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> BirdArtParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> SkyParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> PondParts;
    UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> PondEffectParts;
    bool BirdArtBuildAttempted=false,BirdArtBuildFailed=false;
    TUniquePtr<FHouseCalderWildlifeRuntime,FHouseCalderWildlifeRuntimeDeleter> Runtime;
    void AuthorityStep(double Now);
    void ChangeState(int32 Index,ECalderWildlifeMood Mood,const FVector& Destination,double Now,uint8 Cause=0);
    bool CreateVisuals();
    void Draw(double Now,float DeltaSeconds);
    void DrawSky(double Now);
    void TickPond(double Now);
    void PondReactionStep(double Now,double Raw);
    void DrawPond(double Now);
    void DrawPondEffects(double Now);
    void TickSound(double Now,float DeltaSeconds);
    void StopSound();
#if !UE_BUILD_SHIPPING
    void DrawBirdArt();
#endif
};

namespace HouseCalderWildlife
{
bool Enabled();
bool BirdsEnabled();
bool SoundEnabled();
/** Nonrendered diagnostic only, requires both explicit probe flag and NullRHI. */
bool LogicProbe();
bool ScenePinned();
double ServerTime(const UWorld* World);
double SceneTime(const UWorld* World);
double CycleTime(const UWorld* World);
double CycleReviewOffset();
const TCHAR* MoodName(ECalderWildlifeMood Mood);
}
