#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderFeatures.h"
#include "HouseCalderTerrain.h"
#include "HouseExteriorSiteLayout.h"
#include "HouseExterior.h"
#include "StoryMovementComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "ProfilingDebugging/CsvProfiler.h"

CSV_DEFINE_CATEGORY(CalderWildlife, true);

namespace HouseCalderWildlifeImpl
{
bool Moving(ECalderWildlifeMood M)
{return M==ECalderWildlifeMood::Fleeing || M==ECalderWildlifeMood::Returning || M==ECalderWildlifeMood::Walking;}
double Percentile(const TArray<double>& Values,double Fraction=.95)
{if(Values.IsEmpty())return 0.;TArray<double> Sorted=Values;Sorted.Sort();return Sorted[FMath::Min(Sorted.Num()-1,FMath::FloorToInt(Sorted.Num()*Fraction))];}
void Force(const TArray<FString>& Args,UWorld* World)
{
    for (TActorIterator<AHouseCalderWildlifeDirector> It(World);It;++It)
        if (It->HasAuthority()) {It->ForceSpecies(Args.IsEmpty()?TEXT("deer"):Args[0]);return;}
    UE_LOG(LogTemp,Warning,TEXT("CALDER_WILDLIFE_FORCE no authority director; requires active Calder terrain in Chapter01_House"));
}
FAutoConsoleCommandWithWorldAndArgs ForceCommand(TEXT("Calder.Wildlife"),TEXT("Force an implemented wildlife species appearance on the authority"),FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Force));
// The base GameState averages startup time offsets across many old samples.
// Fresh pond phase transitions already carry authority timestamps: use those
// only for the opt-in ambient cycle, without touching deer reaction time,
// engine/player clocks, or adding idle heartbeat replication.
TWeakObjectPtr<UWorld> CycleAnchorWorld;double CycleWorldDelta=0.;
TWeakObjectPtr<UWorld> LastWorld;bool Spawned=false;
void Spawn(UWorld* World,ELevelTick,float)
{
    if (!World || !World->IsGameWorld() || !World->HasBegunPlay() || !HouseCalderWildlife::Enabled()
        || World->GetNetMode()==NM_Client || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    if (LastWorld.Get()!=World){LastWorld=World;Spawned=false;}
    if (Spawned || World->GetTimeSeconds()<1.f || !HouseCalderTerrain::CollisionReady(World)) return;
    TActorIterator<AHouseExteriorDetail> Exterior(World);if(!Exterior)return;
    Spawned=true;FActorSpawnParameters Params;Params.Name=TEXT("CalderWildlifeDirector");Params.Owner=*Exterior;
    World->SpawnActor<AHouseCalderWildlifeDirector>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
}
struct FRegistration {FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Spawn);}} Registration;
}

bool HouseCalderWildlife::Enabled()
{
    return HouseCalderFeatures::Active() && !FParse::Param(FCommandLine::Get(),TEXT("NoCalderWildlife"));
}
bool HouseCalderWildlife::LogicProbe()
{
#if !UE_BUILD_SHIPPING
    return FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeLogicProbe")) && FParse::Param(FCommandLine::Get(),TEXT("nullrhi"));
#else
    return false;
#endif
}
bool HouseCalderWildlife::BirdsEnabled()
{return Enabled()&&!FParse::Param(FCommandLine::Get(),TEXT("NoCalderBirds"));}
double HouseCalderWildlife::ServerTime(const UWorld* World)
{
    if (!World)return 0.;if(const auto* GS=World->GetGameState())return GS->GetServerWorldTimeSeconds();return World->GetTimeSeconds();
}
bool HouseCalderWildlife::ScenePinned()
{static const bool Pinned=[]{float Value=0.f;return FParse::Value(FCommandLine::Get(),TEXT("CalderSceneTime="),Value);}();return Pinned;}
double HouseCalderWildlife::CycleReviewOffset()
{
#if !UE_BUILD_SHIPPING
    // Explicit native test setup only; still advances at actual server rate.
    static const double Offset=[]
    {
        float Value=0.f;
        if(FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePondReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePondNetworkReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionNetworkReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsReview")) || FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioReview")))FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeClockOffset="),Value);
        return double(Value);
    }();return Offset;
#else
    return 0.;
#endif
}
double HouseCalderWildlife::CycleTime(const UWorld* World)
{
    float Pinned=0.f;if(FParse::Value(FCommandLine::Get(),TEXT("CalderSceneTime="),Pinned))return double(Pinned);
    double T=ServerTime(World);
    if(World && World->GetNetMode()==NM_Client && HouseCalderWildlifeImpl::CycleAnchorWorld.Get()==World)T=World->GetTimeSeconds()+HouseCalderWildlifeImpl::CycleWorldDelta;
    return T+CycleReviewOffset();
}
double HouseCalderWildlife::SceneTime(const UWorld* World)
{const double T=CycleTime(World);return FMath::Fmod(FMath::Fmod(T,1800.)+1800.,1800.);}
const TCHAR* HouseCalderWildlife::MoodName(ECalderWildlifeMood Mood)
{
    switch(Mood){case ECalderWildlifeMood::Staring:return TEXT("stare");case ECalderWildlifeMood::Alarm:return TEXT("stampBlow");case ECalderWildlifeMood::Fleeing:return TEXT("flee");case ECalderWildlifeMood::Returning:return TEXT("return");case ECalderWildlifeMood::Walking:return TEXT("walk");default:return TEXT("feed");}
}
void FHouseCalderWildlifeStates::PostReplicatedAdd(const TArrayView<int32>,int32){if(Owner.IsValid())Owner->StateReceived(IsPond);}
void FHouseCalderWildlifeStates::PostReplicatedChange(const TArrayView<int32>,int32){if(Owner.IsValid())Owner->StateReceived(IsPond);}
AHouseCalderWildlifeDirector::AHouseCalderWildlifeDirector()
{
    PrimaryActorTick.bCanEverTick=true;bReplicates=true;bAlwaysRelevant=true;SetReplicateMovement(false);SetCanBeDamaged(false);
    SetNetUpdateFrequency(10.f);SetMinNetUpdateFrequency(10.f);NetPriority=2.f;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("CalderWildlifeRoot"));States.Owner=this;PondStates.Owner=this;PondStates.IsPond=true;
    Runtime.Reset(new FHouseCalderWildlifeRuntime());
}
void FHouseCalderWildlifeRuntimeDeleter::operator()(FHouseCalderWildlifeRuntime* Value)const{delete Value;}
AHouseCalderWildlifeDirector::~AHouseCalderWildlifeDirector()=default;
void AHouseCalderWildlifeDirector::BeginPlay(){Super::BeginPlay();States.Owner=this;PondStates.Owner=this;PondStates.IsPond=true;}
void AHouseCalderWildlifeDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Runtime)for(int32 I=0;I<Runtime->Animals.Num();++I)
    {const auto& A=Runtime->Animals[I];UE_LOG(LogTemp,Display,TEXT("CALDER_DEER_POSE_CACHE id=%d hits=%lld misses=%lld reusedMediumKeys=%lld freshReferenceChecks=%lld maxPositionErrorCm=%.12f maxAngleErrorDegrees=%.12f maxScaleError=%.12f"),I,A.MemoHits,A.MemoMisses,A.ReusedKeys,A.CacheChecks,A.CachePositionErrorCm,A.CacheAngleErrorDegrees,A.CacheScaleError);}
    if(Runtime)for(const auto& View:Runtime->ViewTickMs)
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_VIEW_TICK view=%s samples=%d p50ms=%.4f p95ms=%.4f clock=Cycles64"),*View.Key.ToString(),View.Value.Num(),HouseCalderWildlifeImpl::Percentile(View.Value,.5),HouseCalderWildlifeImpl::Percentile(View.Value));
    if(Runtime)for(const auto& Mood:Runtime->SoleWindows)
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOLE_MOOD mood=%s gapSamples=%d driftSamples=%d gapP95cm=%.4f driftP95cm=%.4f realRingWindow=1"),HouseCalderWildlife::MoodName(ECalderWildlifeMood(Mood.Key)),Mood.Value.Gaps.Num(),Mood.Value.Drifts.Num(),HouseCalderWildlifeImpl::Percentile(Mood.Value.Gaps),HouseCalderWildlifeImpl::Percentile(Mood.Value.Drifts));
    StopSound();Runtime.Reset();Super::EndPlay(Reason);
}
void AHouseCalderWildlifeDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(AHouseCalderWildlifeDirector,States);DOREPLIFETIME(AHouseCalderWildlifeDirector,PondStates);}
void AHouseCalderWildlifeDirector::StateReceived(bool PondOnly)
{
    if(!Runtime)return;if(PondOnly)
    {
        auto& Pond=Runtime->Pond;++Pond.Received;double Newest=-1.;for(const auto& State:PondStates.Items)Newest=FMath::Max(Newest,double(State.StartTime));
        if(GetNetMode()==NM_Client && Newest>Pond.LastClockStamp)
        {
            Pond.LastClockStamp=Newest;
            // Late joiners can receive idle states last changed many minutes
            // ago. Those are valid states, not fresh clock observations.
            const double GameStateNow=HouseCalderWildlife::ServerTime(GetWorld());
            if(FMath::Abs(Newest-GameStateNow)<.75)
            {
                HouseCalderWildlifeImpl::CycleAnchorWorld=GetWorld();HouseCalderWildlifeImpl::CycleWorldDelta=Newest-GetWorld()->GetTimeSeconds();
                UE_LOG(LogTemp,Display,TEXT("CALDER_POND_CLOCK authorityStamp=%.6f localWorld=%.6f averagedGameState=%.6f freshPhaseEvent=1 extraHeartbeat=0"),Newest,double(GetWorld()->GetTimeSeconds()),GameStateNow);
            }
        }
        return;
    }Runtime->Dirty=true;++Runtime->Received;
    const double Now=HouseCalderWildlife::ServerTime(GetWorld());
    for(const auto& S:States.Items) if(S.Mood==ECalderWildlifeMood::Fleeing)
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_RECEIVED id=%d mood=flee stateStart=%.3f serverNow=%.3f ageMs=%.1f role=%d"),S.Id,S.StartTime,Now,(Now-S.StartTime)*1000.,int32(GetLocalRole()));
}
void AHouseCalderWildlifeDirector::SetTimingView(FName View){if(Runtime)Runtime->TimingView=View;}
#if !UE_BUILD_SHIPPING
bool AHouseCalderWildlifeDirector::PlaceForBoundsReview(int32 Habitat,const FVector& Point)
{
    FString Mode;FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeReviewMode="),Mode);
    if(Mode!=TEXT("bounds") || !HouseCalderWildlife::ScenePinned() || !HasAuthority() || !Ready() || !Runtime->Plan.Groups.IsValidIndex(Habitat))return false;
    if(!Runtime->Plan.Groups[Habitat].Points.ContainsByPredicate([&Point](const FVector& P){return P.Equals(Point,.01);}))return false;
    auto& State=States.Items[0];State.Habitat=uint8(Habitat);State.Mood=ECalderWildlifeMood::Feeding;State.From=State.To=Point;State.StartTime=float(HouseCalderWildlife::ServerTime(GetWorld()));State.FacingYaw=0;State.Cause=3;
    auto& Animal=Runtime->Animals[0];Animal.Home=Point;Animal.Route={Point};Animal.Controls.Reset();Animal.KeyStart=-100.;
    for(int32 Leg=0;Leg<4;++Leg)Animal.FootCycles[Leg]=MIN_int64;
    States.MarkItemDirty(State);Runtime->Dirty=true;ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_BOUNDS_SETUP id=0 habitat=%d point=%s setupTeleportOnly=1 noGameplayRouteClaim=1"),Habitat,*Point.ToString());return true;
}
#endif
bool AHouseCalderWildlifeDirector::Ready()const{return Runtime && Runtime->Graph && !Runtime->Failed && States.Items.Num()==10 && (GetNetMode()==NM_DedicatedServer || Runtime->Visuals || HouseCalderWildlife::LogicProbe());}
FVector AHouseCalderWildlifeDirector::AnimalPosition(int32 Index,double Time)const
{if(!Runtime || !States.Items.IsValidIndex(Index)||!Runtime->Animals.IsValidIndex(Index))return FVector::ZeroVector;return HouseCalderWildlifePose::Root(States.Items[Index],Runtime->Animals[Index],Time<0?HouseCalderWildlife::ServerTime(GetWorld()):Time);}
float AHouseCalderWildlifeDirector::AnimalReactionScale(int32 Index)const
{return States.Items.IsValidIndex(Index)&&States.Items[Index].Habitat==2?.5f:1.f;}
double AHouseCalderWildlifeDirector::WildlifeTickP95Ms()const{return Runtime?HouseCalderWildlifeImpl::Percentile(Runtime->TickMs):0.;}
double AHouseCalderWildlifeDirector::HoofGapP95Cm()const
{
    double Worst=0.;if(Runtime)for(const auto& Mood:Runtime->SoleWindows)Worst=FMath::Max(Worst,HouseCalderWildlifeImpl::Percentile(Mood.Value.Gaps));return Worst;
}
double AHouseCalderWildlifeDirector::HoofDriftP95Cm()const
{
    double Worst=0.;if(Runtime)for(const auto& Mood:Runtime->SoleWindows)Worst=FMath::Max(Worst,HouseCalderWildlifeImpl::Percentile(Mood.Value.Drifts));return Worst;
}
int32 AHouseCalderWildlifeDirector::HoofGapSamples()const{return Runtime?Runtime->HoofGaps.Num():0;}
int32 AHouseCalderWildlifeDirector::HoofDriftSamples()const{return Runtime?Runtime->HoofDrifts.Num():0;}
int32 AHouseCalderWildlifeDirector::ValidPathCount()const{return Runtime?Runtime->Plan.PathSamples:0;}
int32 AHouseCalderWildlifeDirector::RenderComponentCount()const{return DrawParts.Num()+SkyParts.Num()+PondParts.Num()+PondEffectParts.Num();}
double AHouseCalderWildlifeDirector::StateChangeWallTime(int32 Index)const{return Runtime && Runtime->ChangeWallTimes.IsValidIndex(Index)?Runtime->ChangeWallTimes[Index]:0.;}
uint32 AHouseCalderWildlifeDirector::HabitatHash()const{return Runtime?Runtime->Plan.Hash:0;}
const TArray<FVector>& AHouseCalderWildlifeDirector::HabitatNodes(int32 Group)const
{static const TArray<FVector> Empty;return Runtime&&Runtime->Plan.Groups.IsValidIndex(Group)?Runtime->Plan.Groups[Group].Points:Empty;}

void AHouseCalderWildlifeDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!Runtime || Runtime->Failed || !HouseCalderWildlife::Enabled())return;
    const uint64 TickStart=FPlatformTime::Cycles64();const double Now=HouseCalderWildlife::ServerTime(GetWorld());
    const bool Measure=Runtime->Graph && (Runtime->Visuals || GetNetMode()==NM_DedicatedServer || HouseCalderWildlife::LogicProbe());
    if(!Runtime->Graph)
    {
        if(!HouseCalderTerrain::CollisionReady(GetWorld()))return;
        Runtime->Graph=Runtime->Plan.Build(GetWorld());
        if(!Runtime->Graph){Runtime->Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_WILDLIFE_FAILED authored habitat has no safe connected graph"));return;}
    }
    if(HasAuthority() && States.Items.IsEmpty())
    {
        const int32 Counts[]={3,4,3};int32 Id=0;
        for(int32 Group=0;Group<3;++Group)
        {
            const auto& Points=Runtime->Plan.Groups[Group].Points;FVector Centre=FVector::ZeroVector;
            for(const FVector& P:Points)Centre+=P;Centre/=Points.Num();TSet<int32> Used;
            for(int32 Local=0;Local<Counts[Group];++Local)
            {
                const FVector Desired=Centre+FVector((Local%2)*430-215,(Local/2)*460-230,0);int32 Best=INDEX_NONE;double BestDistance=TNumericLimits<double>::Max();
                for(int32 I=0;I<Points.Num();++I)if(!Used.Contains(I) && FVector::DistSquaredXY(Desired,Points[I])<BestDistance)
                {Best=I;BestDistance=FVector::DistSquaredXY(Desired,Points[I]);}
                if(Best<0){Runtime->Failed=true;return;}Used.Add(Best);
                FHouseCalderWildlifeState S;S.Id=Id++;S.Habitat=uint8(Group);S.Variant=S.Id==0?1:Local==Counts[Group]-1?2:0;
                S.Seed=2026101731u+S.Id*239u;S.From=FVector(FMath::RoundToDouble(Points[Best].X*10.)/10.,FMath::RoundToDouble(Points[Best].Y*10.)/10.,FMath::RoundToDouble(Points[Best].Z*10.)/10.);S.To=S.From;S.StartTime=float(Now);S.FacingYaw=uint16(S.Seed%65536u);
                States.Items.Add(S);States.MarkItemDirty(States.Items.Last());
            }
        }
        Runtime->Dirty=true;Runtime->LastChange=Now;ForceNetUpdate();
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_ROSTER species=deer groups=3 counts=3/4/3 animals=10 hardAntlerBucks=1 spotlessYoung=3 authority=1 sceneTime=%.3f terrain=%08x"),HouseCalderWildlife::SceneTime(GetWorld()),HouseCalderTerrain::Hash());
    }
    if(States.Items.Num()!=10)return;
    if(Runtime->Animals.Num()!=States.Items.Num())
    {
        Runtime->Animals.SetNum(States.Items.Num());
        for(int32 I=0;I<States.Items.Num();++I)
        {
            auto& Animal=Runtime->Animals[I];const auto& S=States.Items[I];Animal.Scale=S.Variant==1?1.11f:S.Variant==2?.68f:1.f;
            // Home selection is reconstructed from the stable initial roster, not a
            // late joiner's current replicated From/To (which may already be fleeing).
            const int32 Count=S.Habitat==1?4:3,Local=int32(S.Id)-(S.Habitat==0?0:S.Habitat==1?3:7);
            const auto& Points=Runtime->Plan.Groups[S.Habitat].Points;FVector Centre=FVector::ZeroVector;
            for(const FVector& P:Points)Centre+=P;Centre/=Points.Num();TSet<int32> Used;
            for(int32 J=0;J<FMath::Min(Local+1,Count);++J)
            {
                const FVector Desired=Centre+FVector((J%2)*430-215,(J/2)*460-230,0);int32 Best=INDEX_NONE;double Distance=TNumericLimits<double>::Max();
                for(int32 K=0;K<Points.Num();++K)if(!Used.Contains(K)&&FVector::DistSquaredXY(Desired,Points[K])<Distance)
                {Best=K;Distance=FVector::DistSquaredXY(Desired,Points[K]);}
                Used.Add(Best);Animal.Home=Points[Best];
            }
        }
        Runtime->Dirty=true;
    }
    if(Runtime->Dirty)
    {
        for(int32 I=0;I<States.Items.Num();++I)
        {
            const auto& S=States.Items[I];auto& Animal=Runtime->Animals[I];
            if(HouseCalderWildlifeImpl::Moving(S.Mood))
            {
                if(!Runtime->Plan.Route(S.Habitat,S.From,S.To,Animal.Route))
                {Runtime->Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_WILDLIFE_FAILED route id=%d"),S.Id);return;}
            }
            else Animal.Route={FVector(S.From)};
        }
        Runtime->Dirty=false;
    }
    if(GetNetMode()!=NM_DedicatedServer && !HouseCalderWildlife::LogicProbe() && !Runtime->Visuals)
    {Runtime->Visuals=CreateVisuals();if(!Runtime->Visuals){Runtime->Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_WILDLIFE_FAILED owned static deer assets missing or invalid; run prepared asset batch"));return;}}
    if(HasAuthority() && Now>=Runtime->CheckAt)
    {Runtime->CheckAt=Now+.1;AuthorityStep(Now);}
    TickPond(Now);
    if(GetNetMode()!=NM_DedicatedServer && !HouseCalderWildlife::LogicProbe())
    {
        static const bool Profile=FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifePerfBreakdown"));uint64 At=Profile?FPlatformTime::Cycles64():0;
        Draw(Now,DeltaSeconds);
        if(Profile){CSV_CUSTOM_STAT(CalderWildlife,DeerMs,float(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()-At)),ECsvCustomStatOp::Set);At=FPlatformTime::Cycles64();}
        DrawPond(Now);DrawPondEffects(Now);
        if(Profile){CSV_CUSTOM_STAT(CalderWildlife,PondMs,float(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()-At)),ECsvCustomStatOp::Set);At=FPlatformTime::Cycles64();}
        DrawSky(Now);
        if(Profile){CSV_CUSTOM_STAT(CalderWildlife,SkyMs,float(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()-At)),ECsvCustomStatOp::Set);At=FPlatformTime::Cycles64();}
        TickSound(Now,DeltaSeconds);
        if(Profile)CSV_CUSTOM_STAT(CalderWildlife,SoundMs,float(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()-At)),ECsvCustomStatOp::Set);
    }
#if !UE_BUILD_SHIPPING
    DrawBirdArt();
#endif
    if(HasAuthority() && Now-Runtime->LastChange>2. && NetDormancy!=DORM_DormantAll)SetNetDormancy(DORM_DormantAll);
    if(Measure)
    {
        const double Elapsed=FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()-TickStart);
        CSV_CUSTOM_STAT(CalderWildlife,TickMs,float(Elapsed),ECsvCustomStatOp::Set);
        HouseCalderWildlifeStats::Sample(Runtime->TickMs,Runtime->TickCursor,Elapsed);
        auto& ViewSamples=Runtime->ViewTickMs.FindOrAdd(Runtime->TimingView);auto& Cursor=Runtime->ViewTickCursors.FindOrAdd(Runtime->TimingView);
        HouseCalderWildlifeStats::Sample(ViewSamples,Cursor,Elapsed);
    }
    if(Now>=Runtime->StatsAt+20.)
    {
        int32 EffectsVisible=0;for(bool Visible:Runtime->Pond.EffectVisible)EffectsVisible+=int32(Visible);
        Runtime->StatsAt=Now;UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_TICK p95ms=%.4f hoofGapP95=%.3f plantedDriftP95=%.3f components=%d visible=%d states=%d changed=%d received=%d wholePhase3Complete=0"),WildlifeTickP95Ms(),HoofGapP95Cm(),HoofDriftP95Cm(),RenderComponentCount(),(Runtime->Visible?DrawParts.Num():0)+(Runtime->Sky.Visible[0]?1:0)+(Runtime->Sky.Visible[1]?1:0)+(Runtime->Sky.Visible[2]?1:0)+(Runtime->Sky.Visible[3]?1:0)+(Runtime->Pond.Visible?1:0)+EffectsVisible,States.Items.Num(),Runtime->Changes,Runtime->Received);
    }
}
void AHouseCalderWildlifeDirector::ChangeState(int32 Index,ECalderWildlifeMood Mood,const FVector& Destination,double Now,uint8 Cause)
{
    if(!HasAuthority() || !States.Items.IsValidIndex(Index) || !Runtime->Animals.IsValidIndex(Index))return;
    auto& S=States.Items[Index];auto& Animal=Runtime->Animals[Index];FVector Direction;
    const FVector Raw=HouseCalderWildlifePose::Root(S,Animal,Now,&Direction);
    auto Quantize=[](const FVector& P){return FVector(FMath::RoundToDouble(P.X*10.)/10.,FMath::RoundToDouble(P.Y*10.)/10.,FMath::RoundToDouble(P.Z*10.)/10.);};
    const FVector Position=Quantize(Raw),Target=Quantize(Destination);
    TArray<FVector> Route;
    if(HouseCalderWildlifeImpl::Moving(Mood) && !Runtime->Plan.Route(S.Habitat,Position,Target,Route))
    {UE_LOG(LogTemp,Error,TEXT("CALDER_WILDLIFE_FAILED state route id=%d mood=%s"),S.Id,HouseCalderWildlife::MoodName(Mood));Runtime->Failed=true;return;}
    if(Runtime->ChangeWallTimes.Num()!=States.Items.Num())Runtime->ChangeWallTimes.SetNumZeroed(States.Items.Num());
    Runtime->ChangeWallTimes[Index]=FPlatformTime::Seconds();
    const double PreviousPhase=HouseCalderWildlifePose::AnimationPhase(S,Animal,Now);
    FlushNetDormancy();SetNetDormancy(DORM_Awake);S.PreviousMood=S.Mood;S.PreviousPhase=float(PreviousPhase);S.From=Position;S.To=Target;S.StartTime=float(Now);S.Mood=Mood;S.Cause=Cause;
    S.FacingYaw=uint16(FRotator::ClampAxis(Direction.Rotation().Yaw)*65535.f/360.f);
    Animal.Route=HouseCalderWildlifeImpl::Moving(Mood)?MoveTemp(Route):TArray<FVector>{Position};
    Animal.KeyStart=-100.;for(int32 Leg=0;Leg<4;++Leg)Animal.FootCycles[Leg]=MIN_int64;
    States.MarkItemDirty(S);ForceNetUpdate();Runtime->LastChange=Now;++Runtime->Changes;
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_STATE id=%d habitat=%d mood=%s start=%.3f from=(%.1f,%.1f,%.1f) to=(%.1f,%.1f,%.1f) seed=%u cause=%d authority=1"),S.Id,S.Habitat,HouseCalderWildlife::MoodName(Mood),S.StartTime,S.From.X,S.From.Y,S.From.Z,S.To.X,S.To.Y,S.To.Z,S.Seed,S.Cause);
}
void AHouseCalderWildlifeDirector::AuthorityStep(double Now)
{
    struct FPlayer{FVector Position;double Scale,Still;};TArray<FPlayer> Players;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        auto* PC=It->Get();APawn* Pawn=PC?PC->GetPawn():nullptr;
        if(!Pawn || !Pawn->IsPlayerControlled())continue;
        const FVector P=Pawn->GetActorLocation();
        if(P.Z-HouseExteriorSite::GroundHeight(P.X,P.Y)>500.)continue;
        auto* Character=Cast<ACharacter>(Pawn);const auto* Movement=Character?Cast<UStoryMovementComponent>(Character->GetCharacterMovement()):nullptr;
        const double Factor=Movement&&Movement->IsSprinting()?2.:Character&&Character->bIsCrouched?.6:1.;
        auto* Still=Runtime->StillPawns.Find(Pawn);
        if(!Still){Runtime->StillPawns.Add(Pawn,{P,Now});Still=Runtime->StillPawns.Find(Pawn);}
        if(FVector::DistSquared(P,Still->Key)>25. || Pawn->GetVelocity().SizeSquared()>25.){Still->Key=P;Still->Value=Now;}
        Players.Add({P,Factor,Now-Still->Value});
    }
    TArray<FVector> AlarmPositions;
    for(int32 I=0;I<States.Items.Num();++I)
    {
        auto& S=States.Items[I];auto& Animal=Runtime->Animals[I];const FVector P=AnimalPosition(I,Now);
        const double RangeScale=AnimalReactionScale(I);double Nearest=TNumericLimits<double>::Max();const FPlayer* Threat=nullptr;
        for(const auto& Player:Players)
        {const double D=FVector::Distance(P+FVector(0,0,80*Animal.Scale),Player.Position)/(Player.Scale*RangeScale);if(D<Nearest){Nearest=D;Threat=&Player;}}
        if(HouseCalderWildlifeImpl::Moving(S.Mood))
        {
            const double Duration=HouseCalderWildlifePaths::TravelSeconds(HouseCalderWildlifePaths::Length(Animal.Route),HouseCalderWildlifePose::Speed(S.Mood));
            if(Now-S.StartTime>=Duration)
            {
                const bool Fled=S.Mood==ECalderWildlifeMood::Fleeing;
                if(S.Mood==ECalderWildlifeMood::Returning)Animal.Escaped=false;
                ChangeState(I,Fled&&Nearest<6000.?ECalderWildlifeMood::Staring:ECalderWildlifeMood::Feeding,Threat?Threat->Position:P,Now);continue;
            }
            if(S.Mood==ECalderWildlifeMood::Fleeing)continue;
        }
        if(Nearest<3500. && S.Mood!=ECalderWildlifeMood::Fleeing && Now>=Animal.NextEscapeCheck)
        {
            Animal.NextEscapeCheck=Now+1.;
            const auto& Graph=Runtime->Plan.Groups[S.Habitat];int32 Best=INDEX_NONE;double BestScore=-TNumericLimits<double>::Max();
            for(int32 N=0;N<Graph.Points.Num();++N)
            {
                const FVector& Candidate=Graph.Points[N];if(FVector::DistSquaredXY(Candidate,P)<300.*300.)continue;
                double Safety=TNumericLimits<double>::Max();
                for(const auto& Player:Players)Safety=FMath::Min(Safety,FVector::Distance(Candidate+FVector(0,0,80*Animal.Scale),Player.Position)/(Player.Scale*RangeScale));
                // An animal already at its safest edge watches the threat.
                // Never submit another zero-length flee or reset its timer.
                if(Safety<Nearest+200.)continue;
                double Separation=1000.;for(int32 J=0;J<States.Items.Num();++J)if(J!=I&&States.Items[J].Habitat==S.Habitat)
                    Separation=FMath::Min(Separation,FVector::DistXY(Candidate,FVector(States.Items[J].To)));
                const double Score=Safety-FVector::DistXY(Candidate,P)*.06+FMath::Min(500.,Separation)*.4;
                if(Score>BestScore){Best=N;BestScore=Score;}
            }
            if(Best!=INDEX_NONE)
            {
                Animal.Escaped=true;Animal.ComeBackAt=Now+120.+double(S.Seed%121u);
                ChangeState(I,ECalderWildlifeMood::Fleeing,Graph.Points[Best],Now,1);
                UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_REACTION id=%d mood=flee distanceCm=%.2f thresholdCm=%.2f pawnScale=%.2f habitatScale=%.2f direct=1"),S.Id,Nearest*Threat->Scale*RangeScale,3500.*Threat->Scale*RangeScale,Threat->Scale,RangeScale);
                AlarmPositions.Add(P);continue;
            }
        }
        if(HouseCalderWildlifeImpl::Moving(S.Mood) && Nearest>=6000.)continue;
        ECalderWildlifeMood Desired=Nearest<4500.?ECalderWildlifeMood::Alarm:Nearest<6000.?ECalderWildlifeMood::Staring:ECalderWildlifeMood::Feeding;
        if(S.Mood==ECalderWildlifeMood::Alarm && Nearest>=4500. && Nearest<4800.)Desired=ECalderWildlifeMood::Alarm;
        else if(S.Mood==ECalderWildlifeMood::Staring && Nearest>=6000. && Nearest<6300.)Desired=ECalderWildlifeMood::Staring;
        if(Threat && Threat->Still>=15. && Desired!=ECalderWildlifeMood::Feeding)Desired=ECalderWildlifeMood(uint8(Desired)-1);
        if(Now<Animal.AlarmUntil && uint8(Desired)<uint8(ECalderWildlifeMood::Alarm))Desired=ECalderWildlifeMood::Alarm;
        if(Desired!=S.Mood)
        {ChangeState(I,Desired,Threat?Threat->Position:P,Now,Threat?1:0);
            if(Threat && Desired!=ECalderWildlifeMood::Feeding) UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_REACTION id=%d mood=%s distanceCm=%.2f thresholdCm=%.2f pawnScale=%.2f habitatScale=%.2f direct=1 still=%.2f"),S.Id,HouseCalderWildlife::MoodName(Desired),Nearest*Threat->Scale*RangeScale,(Desired==ECalderWildlifeMood::Alarm?4500.:6000.)*Threat->Scale*RangeScale,Threat->Scale,RangeScale,Threat->Still);
            if(Desired==ECalderWildlifeMood::Alarm)AlarmPositions.Add(P);continue;}
        if(Animal.Escaped && Now>=Animal.ComeBackAt && Nearest>6000.)
        {ChangeState(I,ECalderWildlifeMood::Returning,Animal.Home,Now);continue;}
        if(!HouseCalderWildlife::ScenePinned() && !Animal.Escaped && Desired==ECalderWildlifeMood::Feeding && Now-S.StartTime>28.+S.Seed%37u)
        {
            const auto& Graph=Runtime->Plan.Groups[S.Habitat];int32 Closest=0;double Distance=TNumericLimits<double>::Max();
            for(int32 N=0;N<Graph.Points.Num();++N)if(FVector::DistSquared(P,Graph.Points[N])<Distance){Closest=N;Distance=FVector::DistSquared(P,Graph.Points[N]);}
            const auto& Links=Graph.Links[Closest];int32 Best=INDEX_NONE;double BestScore=-TNumericLimits<double>::Max();
            const FVector Heading=FRotator(0,S.FacingYaw*360./65535.,0).Vector();
            for(int32 N:Links)
            {
                const FVector& Candidate=Graph.Points[N];const double HomeDistance=FVector::DistXY(Candidate,Animal.Home);if(HomeDistance>1000.)continue;
                double Separation=1000.;for(int32 J=0;J<States.Items.Num();++J)if(J!=I&&States.Items[J].Habitat==S.Habitat)
                    Separation=FMath::Min(Separation,FVector::DistXY(Candidate,AnimalPosition(J,Now)));
                if(Separation<180.)continue;
                const uint32 Choice=S.Seed+uint32(N)*71u+uint32(Now/31.)*239u;
                const double Score=double(Choice%101u)*.8+FVector::DotProduct((Candidate-P).GetSafeNormal2D(),Heading)*90.-HomeDistance*.16+FMath::Min(600.,Separation)*.08;
                if(Score>BestScore){Best=N;BestScore=Score;}
            }
            if(Best!=INDEX_NONE)ChangeState(I,ECalderWildlifeMood::Walking,Graph.Points[Best],Now);
        }
    }
    for(const FVector& At:AlarmPositions)for(int32 I=0;I<States.Items.Num();++I)
    {
        auto& S=States.Items[I];if(FVector::DistSquared(AnimalPosition(I,Now),At)>4000.*4000. || (S.Mood==ECalderWildlifeMood::Alarm || S.Mood==ECalderWildlifeMood::Fleeing || S.Mood==ECalderWildlifeMood::Returning))continue;
        Runtime->Animals[I].AlarmUntil=Now+10.;ChangeState(I,ECalderWildlifeMood::Alarm,At,Now,2);++Runtime->Alarms;
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_ALARM id=%d radiusCm=4000 authority=1"),S.Id);
    }
}
bool AHouseCalderWildlifeDirector::ForceSpecies(const FString& Species)
{
    if(!HasAuthority() || !Runtime || !Runtime->Graph || !Species.Equals(TEXT("deer"),ESearchCase::IgnoreCase))
    {UE_LOG(LogTemp,Warning,TEXT("CALDER_WILDLIFE_FORCE unimplemented=%s currentSlice=deerOnly"),*Species);return false;}
    for(int32 I=0;I<States.Items.Num();++I)
    {auto& Animal=Runtime->Animals[I];Animal.Escaped=false;Animal.AlarmUntil=0.;auto& S=States.Items[I];
        // The documented appearance command is an explicit setup reset. Ordinary
        // runtime transitions continue to use the existing graph with no snaps.
        S.From=Animal.Home;S.To=Animal.Home;Animal.Route={Animal.Home};
        ChangeState(I,ECalderWildlifeMood::Feeding,Animal.Home,HouseCalderWildlife::ServerTime(GetWorld()),3);}
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_FORCE species=deer appearances=10 authoredHomesReset=1 setupOnly=1 noReactionBypass=1"));return true;
}
