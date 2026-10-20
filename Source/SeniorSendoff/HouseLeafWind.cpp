#include "HouseLeafWind.h"
#include "HouseExteriorNature.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseDetail.h"
#include "HouseWoodsDetail.h"
#include "HouseWoodsAtmosphere.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HighResScreenshot.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#endif

namespace HouseLeafWindImpl
{
constexpr int32 LeafCount = 96;
constexpr int32 FallingPerPool = 24;
constexpr float PoolRadius = 1450.f;
constexpr int32 WoodsAudioRate = 16000;
constexpr int32 NaturalLeafLimit = 36;
float GustAt(double Time)
{
    const float Main = FMath::Max(0.f,FMath::Sin(float(Time)*.42f-1.f));
    const float Small = FMath::Max(0.f,FMath::Sin(float(Time)*.79f+.3f));
    return FMath::Clamp(.13f + .82f*Main*Main + .17f*Small*Small*Small,0.f,1.f);
}

bool InWoodland(const FVector& Point)
{
    if (FMath::Abs(Point.X) > 7980.f || Point.Y < 4860.f || Point.Y > 19000.f) return false;
    // Do not drift leaves or play close woodland sounds inside the existing hut.
    return FVector2D((Point.X - HouseWoods::HutX) / 1.25f, Point.Y - HouseWoods::HutY).Size() > 620.f;
}

/** Original, locally synthesized 16 kHz mono sounds. No files, downloads or branded samples. */
TArray<int16> WoodlandClip(int32 Kind)
{
    const float Duration = Kind == 0 ? 8.f : Kind == 1 ? 2.7f : Kind == 2 ? 3.6f : .72f;
    const int32 Count = FMath::CeilToInt(Duration * WoodsAudioRate);
    TArray<float> Samples; Samples.SetNumZeroed(Count);
    FRandomStream Noise(2026101350 + Kind);
    float Low = 0.f, Slow = 0.f, Phase = 0.f;
    for (int32 Sample = 0; Sample < Count; ++Sample)
    {
        const float Time = Sample / float(WoodsAudioRate);
        const float White = Noise.FRandRange(-1.f, 1.f);
        Low += .075f * (White - Low);
        Slow += .006f * (White - Slow);
        float Value = 0.f;
        if (Kind == 0)
        {
            // Broad, low canopy wash; slow modulation rather than a tonal drone.
            const float Envelope = .68f + .19f * FMath::Sin(2.f * PI * Time / Duration)
                + .10f * FMath::Sin(4.f * PI * Time / Duration + .7f);
            Value = (Low * .9f + Slow * 1.6f) * Envelope;
        }
        else if (Kind == 1)
        {
            const float Envelope = FMath::Pow(FMath::Sin(PI * Time / Duration), 1.7f);
            const float Frequency = 114.f + 34.f * FMath::Sin(Time * 1.15f) + 2.8f * FMath::Sin(Time * 34.f);
            Phase += 2.f * PI * Frequency / WoodsAudioRate;
            Value = Envelope * (.15f * FMath::Sin(Phase) + .055f * FMath::Sin(Phase * 2.003f) + Low * .23f);
        }
        else if (Kind == 2)
        {
            // A subdued three-note owl phrase, separated by silence and heard at distance.
            const float Starts[] = {.16f, .86f, 1.65f};
            const float Lengths[] = {.40f, .43f, .90f};
            for (int32 Note = 0; Note < 3; ++Note)
            {
                const float Age = Time - Starts[Note];
                if (Age < 0.f || Age > Lengths[Note]) continue;
                const float Envelope = FMath::Pow(FMath::Sin(PI * Age / Lengths[Note]), 1.2f);
                const float NotePhase = 2.f * PI * (365.f * Age - 17.f * Age * Age);
                Value += Envelope * (.22f * FMath::Sin(NotePhase) + .021f * FMath::Sin(NotePhase * 2.f));
            }
        }
        else
        {
            for (int32 Crack = 0; Crack < 3; ++Crack)
            {
                const float Age = Time - (.05f + Crack * .043f);
                if (Age < 0.f) continue;
                Value += (White - Low) * .31f * FMath::Exp(-Age * 125.f)
                    + .10f * FMath::Sin(Age * 2.f * PI * (660.f + Crack * 137.f)) * FMath::Exp(-Age * 72.f);
            }
        }
        Samples[Sample] = Value;
    }
    if (Kind == 0)
    {
        // Crossfade the final 100 ms into the initial noise: continuously queued buffers have no seam click.
        const int32 Seam = WoodsAudioRate / 10;
        for (int32 Sample = 0; Sample < Seam; ++Sample)
        {
            const float Alpha = Sample / float(Seam - 1);
            Samples[Count - Seam + Sample] = FMath::Lerp(Samples[Count - Seam + Sample], Samples[Sample], Alpha);
        }
    }
    // The tail now joins the first 100 ms. Omit those first samples so its end
    // connects to the following sample at the beginning of every queued repeat.
    const int32 Skip = Kind == 0 ? WoodsAudioRate / 10 : 0;
    TArray<int16> Pcm; Pcm.SetNumUninitialized(Count - Skip);
    for (int32 Sample = Skip; Sample < Count; ++Sample)
    {
        const float Time = Sample / float(WoodsAudioRate);
        const float Fade = Kind == 0 ? 1.f : FMath::Min(1.f, FMath::Min(Time, Duration - Time) / .025f);
        Pcm[Sample - Skip] = int16(FMath::Clamp(Samples[Sample] * Fade, -.9f, .9f) * 32000.f);
    }
    return Pcm;
}
}

AHouseLeafWind::AHouseLeafWind()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    bReplicates = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LeafWindOrigin"));
    for(int32 K=0;K<3;++K)
    {
        UInstancedStaticMeshComponent* P=CreateDefaultSubobject<UInstancedStaticMeshComponent>(
            *FString::Printf(TEXT("DriftingLeaves%d"),K));
        P->SetupAttachment(RootComponent);
        P->SetMobility(EComponentMobility::Movable);
        P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        P->SetGenerateOverlapEvents(false);
        P->SetCanEverAffectNavigation(false);
        P->SetCastShadow(false);
        P->SetCullDistances(1150,1750);
        Pools.Add(P);
    }
    for(int32 K=0;K<9;++K)
    {
        UInstancedStaticMeshComponent* P=CreateDefaultSubobject<UInstancedStaticMeshComponent>(
            *FString::Printf(TEXT("FallingTreeLeaves%d"),K));
        P->SetupAttachment(RootComponent);
        P->SetMobility(EComponentMobility::Movable);
        P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        P->SetGenerateOverlapEvents(false);
        P->SetCanEverAffectNavigation(false);
        P->SetCastShadow(false);
        P->SetCullDistances(2100,2800);
        FallingPools.Add(P);
    }
    Rustle=CreateDefaultSubobject<UAudioComponent>(TEXT("LocalLeafRustle"));
    Rustle->SetupAttachment(RootComponent);
    Rustle->bAutoActivate=false;
    Rustle->bAutoDestroy=false;
    Rustle->bAllowSpatialization=true;
    Rustle->bOverrideAttenuation=true;
    FSoundAttenuationSettings& A=Rustle->AttenuationOverrides;
    A.bAttenuate=true;
    A.bSpatialize=true;
    A.AttenuationShape=EAttenuationShape::Sphere;
    A.AttenuationShapeExtents=FVector(150.f);
    A.FalloffDistance=1000.f;
    A.bEnableOcclusion=true;
    A.OcclusionTraceChannel=ECC_Visibility;
    A.OcclusionLowPassFilterFrequency=800.f;
    A.OcclusionVolumeAttenuation=.08f;
    A.OcclusionInterpolationTime=.35f;
    Rustle->SetVolumeMultiplier(0.f);
}

void AHouseLeafWind::Initialize(UHouseBlackHallSubsystem* Look,const TArray<FTransform>& FallenLeaves,const TArray<HouseNature::FTreeCanopy>& Canopies)
{
    if(!Look||!GetWorld()||GetNetMode()==NM_DedicatedServer)
    {SetActorTickEnabled(false);return;}
    for(int32 K=0;K<3;++K)
    {
        FHallMesh M;HouseNature::SingleLeaf(M,K);
        UStaticMesh* Mesh=HwBuildStaticMesh(M,*FString::Printf(TEXT("SM_WindLeaf%d"),K),Look,{});
        if(!Mesh){SetActorTickEnabled(false);return;}
        Pools[K]->SetStaticMesh(Mesh);
    }
    for(int32 K=0;K<HouseLeafWindImpl::LeafCount;++K)
    {
        FHouseWindLeaf L;
        L.Palette=K%3;
        L.Instance=Pools[L.Palette]->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector(.0001f)));
        L.Size=Random.FRandRange(.72f,1.35f);
        L.Phase=Random.FRandRange(0.f,2.f*PI);
        L.Threshold=Random.FRandRange(.24f,.58f);
        L.Drag=Random.FRandRange(.7f,1.35f);
        Leaves.Add(L);
    }
    TreeCanopies=Canopies;
    for(int32 Pool=0;Pool<9;++Pool)
    {
        FHallMesh M;HouseNature::SingleLeaf(M,Pool%3,HouseNature::ETreeKind(Pool/3));
        UStaticMesh* Mesh=HwBuildStaticMesh(M,*FString::Printf(TEXT("SM_FallingTreeLeaf%d"),Pool),Look,{});
        FallingPools[Pool]->SetStaticMesh(Mesh);
        for(int32 K=0;K<HouseLeafWindImpl::FallingPerPool;++K)
        {
            FHouseFallingLeaf L;L.Pool=Pool;
            L.Instance=FallingPools[Pool]->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector(.0001f)));
            FallingLeaves.Add(L);
        }
    }
    for(const FTransform& T:FallenLeaves)Seeds.Add(T.GetTranslation());
    const FName Names[]={TEXT("EX_NatureGold"),TEXT("EX_NatureCopper"),TEXT("EX_NatureRust"),
        TEXT("EX_NatureGreenLeaf"),TEXT("EX_NatureShrub"),TEXT("EX_NatureShrubTips"),
        TEXT("EX_NatureGrass"),TEXT("EX_NatureDryGrass")};
    for(int32 K=0;K<8;++K)if(UMaterialInstanceDynamic* M=Cast<UMaterialInstanceDynamic>(Look->Mat(Names[K])))
    {
        WindMaterials.Add(M);
        WindAmplitudes.Add(K<4?2.4f:K==4?.6f:K==5?.9f:K==6?2.7f:3.8f);
    }
    for(const TCHAR* Kind:{TEXT("Oak"),TEXT("Birch")})for(int32 Palette=0;Palette<3;++Palette)
        if(UMaterialInstanceDynamic* M=Cast<UMaterialInstanceDynamic>(Look->Mat(FName(*FString::Printf(TEXT("EX_Nature%sLeaf%d"),Kind,Palette)))))
        {
            WindMaterials.Add(M);WindAmplitudes.Add(FCString::Strcmp(Kind,TEXT("Oak"))==0?2.f:3.1f);
        }
    RustleWave=LoadObject<USoundWave>(nullptr,TEXT("/Game/SeniorSendOff/House/ExteriorNature/S_LeavesRustlingLoop.S_LeavesRustlingLoop"));
    if(RustleWave)
    {
        RustleWave->bLooping=true;
        Rustle->SetSound(RustleWave);
        Rustle->OnAudioPlaybackPercentNative.AddUObject(this,&AHouseLeafWind::AudioPlayback);
    }
    else UE_LOG(LogTemp,Error,TEXT("LEAF_WIND missing saved rustle sound"));
    bReady=true;
    UE_LOG(LogTemp,Display,TEXT("LEAF_WIND ready pool=%d palettes=3 seed_patches=%d audio=%s; local cosmetic/no collision/no added lights"),
        Leaves.Num(),Seeds.Num(),*GetNameSafe(RustleWave));
    UE_LOG(LogTemp,Display,TEXT("TREE_LEAVES ready falling_pool=%d species=3 canopy_templates=%d"),FallingLeaves.Num(),TreeCanopies.Num());
}

void AHouseLeafWind::EnableWoodlandEffects(UHouseBlackHallSubsystem* Look,
    const TArray<FName>& SwayMaterialKeys, const TArray<FTransform>& WoodlandTrees)
{
    if (!bReady || bWoodlandEnabled || !Look) return;
    bWoodlandEnabled = true;
    bWoodlandAudioEnabled = !FParse::Param(FCommandLine::Get(), TEXT("NoWoodsAudio"));
    bWoodlandDiagnostics = FParse::Param(FCommandLine::Get(), TEXT("WoodsMotionReview"))
        || FParse::Param(FCommandLine::Get(), TEXT("WoodsPassReview"));
    for (const FName& Key : SwayMaterialKeys)
        if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Look->Mat(Key)))
        {
            if (WindMaterials.Contains(Material)) continue;
            float Baseline = 0.f;
            if (!Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("WindStrength")), Baseline) || Baseline <= 0.f) continue;
            WindMaterials.Add(Material);
            WindAmplitudes.Add(Baseline);
            ++WoodlandRegisteredMaterials;
        }
    for (const FTransform& Tree : WoodlandTrees)
        if (HouseLeafWindImpl::InWoodland(Tree.GetLocation())) WoodlandSources.Add(Tree.GetLocation());
    BuildWoodlandAudio();
    UE_LOG(LogTemp, Display, TEXT("WOODS_MOTION enabled=1 sources=%d registeredWindMIDs=%d groundPool=%d sharedFallingPool=%d naturalCap=%d independentSeed=2026101341 noTransformEdits=1 audio=%d emptyHook=WoodsPartyStreetFade"),
        WoodlandSources.Num(), WoodlandRegisteredMaterials, Leaves.Num(), FallingLeaves.Num(), HouseLeafWindImpl::NaturalLeafLimit, bWoodlandAudioEnabled);
}

void AHouseLeafWind::RegisterWindMaterial(UMaterialInstanceDynamic* Material, float Baseline)
{
    if (!bReady || !Material || Baseline <= 0.f || WindMaterials.Contains(Material)) return;
    WindMaterials.Add(Material);
    WindAmplitudes.Add(Baseline);
}

bool AHouseLeafWind::IsGroundArea(const FVector& Point) const
{
    if (bWoodlandEnabled && Point.Y >= 4860.f) return HouseLeafWindImpl::InWoodland(Point);
    return HouseNature::IsLeafGround(Point.X, Point.Y);
}

float AHouseLeafWind::GroundHeight(const FVector& Point) const
{
    return bWoodlandEnabled && HouseLeafWindImpl::InWoodland(Point)
        ? HouseWoods::Height(Point.X, Point.Y) : HouseNature::ApproximateGroundHeight(Point.X, Point.Y);
}

void AHouseLeafWind::BuildWoodlandAudio()
{
    const TCHAR* Names[] = {TEXT("WoodsCanopyWind"), TEXT("WoodsBranchCreak"), TEXT("WoodsDistantOwl"),
        TEXT("WoodsUnseenTwig"), TEXT("WoodsPartyStreetFade")};
    WoodlandPcm.SetNum(4);
    WoodlandWaves.SetNum(4);
    for (int32 Kind = 0; Kind < 5; ++Kind)
    {
        UAudioComponent* Component = NewObject<UAudioComponent>(this, Names[Kind], RF_Transient);
        Component->SetupAttachment(RootComponent);
        Component->bAutoActivate = false;
        Component->bAutoDestroy = false;
        Component->bAllowSpatialization = Kind > 0 && Kind < 4;
        Component->bOverrideAttenuation = Kind > 0 && Kind < 4;
        Component->SetVolumeMultiplier(0.f);
        if (Component->bOverrideAttenuation)
        {
            FSoundAttenuationSettings& Attenuation = Component->AttenuationOverrides;
            Attenuation.bAttenuate = true;
            Attenuation.bSpatialize = true;
            Attenuation.AttenuationShape = EAttenuationShape::Sphere;
            Attenuation.AttenuationShapeExtents = FVector(250.f);
            Attenuation.FalloffDistance = Kind == 2 ? 6500.f : 2600.f;
            Attenuation.bEnableOcclusion = true;
            Attenuation.OcclusionTraceChannel = ECC_Visibility;
            Attenuation.OcclusionLowPassFilterFrequency = 1100.f;
            Attenuation.OcclusionVolumeAttenuation = .28f;
            Attenuation.OcclusionInterpolationTime = .65f;
        }
        Component->RegisterComponent();
        WoodlandAudio.Add(Component);
        if (Kind < 4 && bWoodlandAudioEnabled)
        {
            WoodlandPcm[Kind] = HouseLeafWindImpl::WoodlandClip(Kind);
            USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this, NAME_None, RF_Transient);
            Wave->SetSampleRate(HouseLeafWindImpl::WoodsAudioRate);
            Wave->NumChannels = 1;
            Wave->Duration = Kind == 0 ? INDEFINITELY_LOOPING_DURATION : WoodlandPcm[Kind].Num() / float(HouseLeafWindImpl::WoodsAudioRate);
            Wave->SoundGroup = SOUNDGROUP_Default;
            Wave->bLooping = false;
            WoodlandWaves[Kind] = Wave;
            Component->SetSound(Wave);
        }
    }
    // Hook stays silent: attaching an actual exterior party/street sound is a separate integration.
    WoodlandAudio[4]->SetVolumeMultiplier(1.f);
    UE_LOG(LogTemp, Display, TEXT("WOODS_AUDIO originalProceduralPCM=%d sampleRate=%d components=4 emptyFadeHooks=1 noNightclubChanges=1"),
        bWoodlandAudioEnabled, HouseLeafWindImpl::WoodsAudioRate);
}

void AHouseLeafWind::PlayWoodlandSound(int32 Kind, const FVector& At, float Gain)
{
    if (!bWoodlandAudioEnabled || Kind < 1 || Kind > 3 || !WoodlandWaves[Kind] || WoodlandPcm[Kind].IsEmpty()) return;
    UAudioComponent* Component = WoodlandAudio[Kind];
    USoundWaveProcedural* Wave = WoodlandWaves[Kind];
    Component->Stop();
    Wave->ResetAudio();
    Wave->QueueAudio(reinterpret_cast<const uint8*>(WoodlandPcm[Kind].GetData()), WoodlandPcm[Kind].Num() * sizeof(int16));
    Component->SetWorldLocation(At);
    Component->SetVolumeMultiplier(Gain);
    Component->Play();
    WoodlandSoundRemaining[Kind] = Wave->Duration + .08f;
    ++WoodlandSoundEvents;
    if (bWoodlandDiagnostics)
        UE_LOG(LogTemp, Display, TEXT("WOODS_AUDIO event=%s pcmBytes=%d at=(%.0f,%.0f,%.0f) gain=%.3f nativePlaying=%d"),
            *Component->GetName(), WoodlandPcm[Kind].Num() * int32(sizeof(int16)), At.X, At.Y, At.Z, Gain, Component->IsPlaying());
}

void AHouseLeafWind::TickWoodland(float Dt, double Now, const FVector& Viewer, const FVector& Wind)
{
    if (!bWoodlandEnabled) return;
    const bool InWoods = HouseLeafWindImpl::InWoodland(Viewer)
        && Viewer.Z > GroundHeight(Viewer) - 35.f && Viewer.Z < GroundHeight(Viewer) + 1400.f;
    const float Depth = InWoods ? HouseWoodsAtmosphere::DepthAt(Viewer) : 0.f;
    WoodlandGain = FMath::FInterpTo(WoodlandGain, InWoods ? FMath::Lerp(.32f, 1.f, Depth) : 0.f, Dt, 1.2f);
    WoodlandPartyStreetGain = FMath::FInterpTo(WoodlandPartyStreetGain, 1.f - .94f * Depth, Dt, 1.2f);
    if (WoodlandAudio.Num() == 5) WoodlandAudio[4]->SetVolumeMultiplier(WoodlandPartyStreetGain);
    WoodlandCacheCountdown -= Dt;
    if (InWoods && (WoodlandCacheCountdown <= 0.f || FVector::DistSquared2D(Viewer, WoodlandCachePosition) > FMath::Square(650.f)))
    {
        WoodlandCacheCountdown = 2.f;
        WoodlandCachePosition = Viewer;
        NearbyWoodlandSources.Reset();
        for (const FVector& Source : WoodlandSources)
            if (FVector::DistSquared2D(Viewer, Source) < FMath::Square(1800.f)) NearbyWoodlandSources.Add(Source);
    }
    WoodlandLeafCountdown -= Dt;
    int32 ActiveNatural = 0;
    float MinimumTravel = FLT_MAX, MaximumTravel = 0.f;
    for (const FHouseFallingLeaf& Leaf : FallingLeaves) if (Leaf.Age >= 0.f && Leaf.bWoodland)
    {
        ++ActiveNatural;
        if (Leaf.Age > .2f)
        {
            const float Travel = FVector::Distance(Leaf.Position, Leaf.WoodlandOrigin);
            MinimumTravel = FMath::Min(MinimumTravel, Travel);
            MaximumTravel = FMath::Max(MaximumTravel, Travel);
        }
    }
    if (InWoods && WoodlandLeafCountdown <= 0.f)
    {
        WoodlandLeafCountdown = WoodlandRandom.FRandRange(.65f, 1.6f);
        if (!NearbyWoodlandSources.IsEmpty() && !FallingLeaves.IsEmpty() && ActiveNatural < HouseLeafWindImpl::NaturalLeafLimit)
        {
            const FVector Source = NearbyWoodlandSources[WoodlandRandom.RandRange(0, NearbyWoodlandSources.Num() - 1)];
            const int32 Count = WoodlandRandom.RandRange(1, 3);
            for (int32 NewLeaf = 0; NewLeaf < Count && ActiveNatural < HouseLeafWindImpl::NaturalLeafLimit; ++NewLeaf)
            {
                // Only take genuinely idle shared slots. A player hit always retains priority.
                const int32 First = WoodlandRandom.RandRange(0, FallingLeaves.Num() - 1);
                FHouseFallingLeaf* Available = nullptr;
                for (int32 Offset = 0; Offset < FallingLeaves.Num(); ++Offset)
                    if (FallingLeaves[(First + Offset) % FallingLeaves.Num()].Age < 0.f)
                    { Available = &FallingLeaves[(First + Offset) % FallingLeaves.Num()]; break; }
                if (!Available) break;
                FHouseFallingLeaf& Leaf = *Available;
                Leaf.Position = Source + FVector(WoodlandRandom.FRandRange(-130, 130), WoodlandRandom.FRandRange(-130, 130), WoodlandRandom.FRandRange(380, 740));
                Leaf.WoodlandOrigin = Leaf.Position;
                Leaf.Velocity = Wind * WoodlandRandom.FRandRange(18, 38) + FVector(0, 0, -35);
                Leaf.Age = 0.f; Leaf.Delay = 0.f; Leaf.RestAge = 0.f;
                Leaf.Size = WoodlandRandom.FRandRange(.80f, 1.25f);
                Leaf.Phase = WoodlandRandom.FRandRange(0, 2 * PI);
                Leaf.Yaw = WoodlandRandom.FRandRange(-180, 180); Leaf.Roll = 25.f; Leaf.Pitch = 15.f;
                Leaf.Normal = FVector::UpVector;
                Leaf.bGroundKnown = false; Leaf.bLanded = false; Leaf.bWoodland = true;
                ++ActiveNatural; ++WoodlandShedLeaves;
            }
        }
    }
    if (bWoodlandAudioEnabled && WoodlandAudio.Num() == 5)
    {
        UAudioComponent* Canopy = WoodlandAudio[0];
        USoundWaveProcedural* Wave = WoodlandWaves[0];
        if (WoodlandGain > .008f)
        {
            if (Wave->GetAvailableAudioByteCount() < HouseLeafWindImpl::WoodsAudioRate * int32(sizeof(int16)))
                Wave->QueueAudio(reinterpret_cast<const uint8*>(WoodlandPcm[0].GetData()), WoodlandPcm[0].Num() * sizeof(int16));
            Canopy->SetVolumeMultiplier(WoodlandGain * (.085f + Gust * .065f));
            if (!Canopy->IsPlaying())
            {
                Canopy->Play();
                if (bWoodlandDiagnostics) UE_LOG(LogTemp, Display, TEXT("WOODS_AUDIO canopyPlayStart=1 queuedPCMBytes=%d nativePlaying=%d"),
                    Wave->GetAvailableAudioByteCount(), Canopy->IsPlaying());
            }
        }
        else if (Canopy->IsPlaying()) { Canopy->Stop(); Wave->ResetAudio(); }
        for (int32 Kind = 1; Kind < 4; ++Kind)
            if (WoodlandSoundRemaining[Kind] > 0.f)
            {
                WoodlandSoundRemaining[Kind] -= Dt;
                if (WoodlandSoundRemaining[Kind] <= 0.f || !InWoods)
                { WoodlandAudio[Kind]->Stop(); WoodlandSoundRemaining[Kind] = 0.f; }
            }
        if (InWoods) WoodlandSoundCountdown -= Dt;
        if (InWoods && WoodlandSoundCountdown <= 0.f)
        {
            WoodlandSoundCountdown = WoodlandAudioRandom.FRandRange(9.f, 18.f);
            const int32 Choice = WoodlandAudioRandom.RandRange(0, 5);
            const int32 Kind = Choice < 3 ? 1 : Choice < 5 ? 2 : 3;
            const float Angle = WoodlandAudioRandom.FRandRange(0, 2 * PI);
            const float Distance = Kind == 2 ? WoodlandAudioRandom.FRandRange(2400, 3800) : WoodlandAudioRandom.FRandRange(500, 1400);
            FVector At = Viewer + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0);
            At.X = FMath::Clamp(At.X, -7800., 7800.); At.Y = FMath::Clamp(At.Y, 4900., 18800.);
            At.Z = HouseWoods::Height(At.X, At.Y) + (Kind == 1 ? 510.f : Kind == 2 ? 1050.f : 10.f);
            PlayWoodlandSound(Kind, At, WoodlandGain * (Kind == 3 ? .15f : .24f));
        }
    }
    if (bWoodlandDiagnostics && Now - WoodlandLastReport >= 3.)
    {
        WoodlandLastReport = Now;
        const int32 Queued = bWoodlandAudioEnabled && WoodlandWaves[0] ? WoodlandWaves[0]->GetAvailableAudioByteCount() : 0;
        UE_LOG(LogTemp, Display, TEXT("WOODS_MOTION_SAMPLE inside=%d depth=%.3f sourcesNear=%d naturalSpawned=%d activeNatural=%d naturalTravelCm=%.2f..%.2f landedNatural=%d windMIDs=%d gust=%.3f ambientGain=%.3f partyStreetHookGain=%.3f hookConnected=0 canopyPlaying=%d queuedPCMBytes=%d soundEvents=%d fallingCapacity=%d"),
            InWoods, Depth, NearbyWoodlandSources.Num(), WoodlandShedLeaves, ActiveNatural, MinimumTravel == FLT_MAX ? 0.f : MinimumTravel, MaximumTravel, WoodlandLandedLeaves, WoodlandRegisteredMaterials,
            Gust, WoodlandGain, WoodlandPartyStreetGain, WoodlandAudio.Num() > 0 && WoodlandAudio[0]->IsPlaying(), Queued, WoodlandSoundEvents, FallingLeaves.Num());
    }
}

int32 AHouseLeafWind::ActiveFallingLeaves() const
{
    int32 Count=0;for(const FHouseFallingLeaf& L:FallingLeaves)if(L.Age>=0.f)++Count;return Count;
}

void AHouseLeafWind::ShedLeaves(const FTransform& Tree,uint8 Kind,uint8 Variant,const FVector& Direction,int32 Seed)
{
    if(!bReady||Kind>=3)return;
    const HouseNature::FTreeCanopy* Canopy=TreeCanopies.FindByPredicate([&](const HouseNature::FTreeCanopy& C)
        {return uint8(C.Kind)==Kind&&C.Variant==Variant;});
    if(!Canopy||Canopy->Leaves.IsEmpty())return;
    APlayerController* Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->PlayerCameraManager||FVector::DistSquared2D(Tree.GetLocation(),Player->PlayerCameraManager->GetCameraLocation())>FMath::Square(2600.f))return;
    FRandomStream R(Seed);
    const int32 Count=Kind==uint8(HouseNature::ETreeKind::Oak)?36:Kind==uint8(HouseNature::ETreeKind::Birch)?24:30;
    const int32 PerKind=3*HouseLeafWindImpl::FallingPerPool;
    for(int32 K=0;K<Count;++K)
    {
        // Ring buffers per species cap repeated impacts; there are no spawned leaf actors.
        FHouseFallingLeaf& L=FallingLeaves[Kind*PerKind+((ShedCursors[Kind]++*29)%PerKind)];
        L.Position=Tree.TransformPosition(Canopy->Leaves[R.RandRange(0,Canopy->Leaves.Num()-1)]);
        L.Velocity=Direction*R.FRandRange(15,42)+FVector(R.FRandRange(-25,25),R.FRandRange(-25,25),R.FRandRange(-25,25));
        L.Age=0.f;L.Delay=R.FRandRange(0,.6f);L.RestAge=0.f;L.Size=R.FRandRange(.75f,1.45f);
        L.Phase=R.FRandRange(0,2*PI);L.Yaw=R.FRandRange(-180,180);L.Roll=R.FRandRange(-70,70);L.Pitch=R.FRandRange(-40,40);
        L.bGroundKnown=L.bLanded=L.bWoodland=false;L.Normal=FVector::UpVector;
        FallingPools[L.Pool]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
    }
    ++ShedEvents;
    HitRustle=.8f;HitSoundPosition=Tree.GetLocation()+FVector(0,0,130);
    UE_LOG(LogTemp,Display,TEXT("TREE_LEAVES shed species=%d seed=%d new=%d active=%d capacity=%d"),Kind,Seed,Count,ActiveFallingLeaves(),FallingLeaves.Num());
}

void AHouseLeafWind::TickFallingLeaves(float Dt,const FVector& Viewer,const FVector& Wind)
{
    int32 Queries=0;bool Dirty=false;
    for(FHouseFallingLeaf& L:FallingLeaves)
    {
        if(L.Age<0.f)continue;
        Dirty=true;L.Age+=Dt;
        if(FVector::DistSquared2D(Viewer,L.Position)>FMath::Square(2800.f)||L.Age>24.f||(L.bLanded&&L.RestAge>8.f))
        {
            L.Age=-1.f;
            FallingPools[L.Pool]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
            continue;
        }
        if(!L.bGroundKnown&&Queries<12)
        {
            ++Queries;FHitResult H;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(TreeLeafGround),true,this);
            if(APlayerController* P=GetWorld()->GetFirstPlayerController())if(P->GetPawn())Q.AddIgnoredActor(P->GetPawn());
            if(GetWorld()->LineTraceSingleByChannel(H,L.Position+FVector(0,0,2),L.Position-FVector(0,0,2500),ECC_Visibility,Q)&&H.ImpactNormal.Z>.45f)
            {L.Surface=H.ImpactPoint;L.Normal=H.ImpactNormal;L.bGroundKnown=true;}
            else
            {
                L.Age=-1.f;
                FallingPools[L.Pool]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
                continue;
            }
        }
        if(!L.bGroundKnown||L.Age<L.Delay)continue;
        L.Phase+=Dt*(2.6f+L.Size);
        if(!L.bLanded)
        {
            const FVector Across(-Wind.Y,Wind.X,0);
            const FVector Target=Wind*(25.f+Gust*65.f)+Across*(FMath::Sin(L.Phase)*42.f);
            L.Velocity.X=FMath::FInterpTo(L.Velocity.X,Target.X,Dt,1.8f);
            L.Velocity.Y=FMath::FInterpTo(L.Velocity.Y,Target.Y,Dt,1.8f);
            // Air drag limits the fall; oscillating tilt produces an irregular flutter.
            L.Velocity.Z=FMath::Max(L.Velocity.Z-Dt*120.f,-110.f-L.Size*45.f);
            const FVector Next=L.Position+L.Velocity*Dt;
            FHitResult Contact;
            FCollisionQueryParams Q(SCENE_QUERY_STAT(TreeLeafContact),true,this);
            if(APlayerController* P=GetWorld()->GetFirstPlayerController())if(P->GetPawn())Q.AddIgnoredActor(P->GetPawn());
            // Only check contact near the cached surface. This bounds queries for a full burst.
            if(Next.Z<L.Surface.Z+120.f&&Queries<12)
            {
                ++Queries;
                if(GetWorld()->LineTraceSingleByChannel(Contact,Next+FVector(0,0,125),Next-FVector(0,0,220),ECC_Visibility,Q)&&Contact.ImpactNormal.Z>.45f)
                {L.Surface=Contact.ImpactPoint;L.Normal=Contact.ImpactNormal;}
            }
            L.Position=Next;
            if(Next.Z<=L.Surface.Z+1.f)
            {
                L.Position.Z=L.Surface.Z+1.f;L.bLanded=true;L.Velocity*=.15f;++LandedLeaves;
                if(L.bWoodland)++WoodlandLandedLeaves;
            }
            L.Roll=FMath::Sin(L.Phase)*62.f;
            L.Pitch=FMath::Sin(L.Phase*1.37f)*34.f;
            L.Yaw+=Dt*(18.f+FMath::Sin(L.Phase*.7f)*55.f);
        }
        else
        {
            L.RestAge+=Dt;
            L.Roll=FMath::FInterpTo(L.Roll,0.f,Dt,7.f);L.Pitch=FMath::FInterpTo(L.Pitch,0.f,Dt,7.f);
        }
        const float Fade=FMath::SmoothStep(0.f,.15f,L.Age-L.Delay)*(1.f-FMath::SmoothStep(6.5f,8.f,L.RestAge));
        const FQuat Ground=L.bLanded?FRotationMatrix::MakeFromZ(L.Normal).ToQuat():FQuat::Identity;
        FallingPools[L.Pool]->UpdateInstanceTransform(L.Instance,FTransform(Ground*FRotator(L.Pitch,L.Yaw,L.Roll).Quaternion(),L.Position,FVector(L.Size*Fade)),true,false,true);
    }
    if(Dirty)for(UInstancedStaticMeshComponent* P:FallingPools)P->MarkRenderStateDirty();
}

bool AHouseLeafWind::FindGround(const FVector& Point,FVector& Ground,FVector& Normal) const
{
    if(!GetWorld()||FMath::Abs(Point.X)>8250||Point.Y < -6100||Point.Y>(bWoodlandEnabled?19000.f:6000.f)
        ||!IsGroundArea(Point))return false;
    const float Expected=GroundHeight(Point);
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(LeafWindGround),true,this);
    if(APlayerController* P=GetWorld()->GetFirstPlayerController())if(P->GetPawn())Q.AddIgnoredActor(P->GetPawn());
    if(!GetWorld()->LineTraceSingleByChannel(Hit,FVector(Point.X,Point.Y,Expected+82.f),
        FVector(Point.X,Point.Y,Expected-125.f),ECC_Visibility,Q))return false;
    if(Hit.ImpactNormal.Z<.65f||Hit.ImpactPoint.Z>Expected+72.f)return false;
    Ground=Hit.ImpactPoint;
    Normal=Hit.ImpactNormal;
    return true;
}

bool AHouseLeafWind::RecycleLeaf(FHouseWindLeaf& L,const FVector& Viewer,const FVector& Forward)
{
    const bool InWoods=bWoodlandEnabled&&HouseLeafWindImpl::InWoodland(Viewer);
    FRandomStream& SourceRandom=InWoods?WoodlandRandom:Random;
    for(int32 Attempt=0;Attempt<18;++Attempt)
    {
        const float A=SourceRandom.FRandRange(0.f,2.f*PI),D=SourceRandom.FRandRange(220.f,1000.f);
        FVector P=Viewer+Forward*250.f+FVector(FMath::Cos(A)*D,FMath::Sin(A)*D,0);
        // Some leaves originate in existing piles; the rest pass across open lawn and walks.
        if(!InWoods&&Attempt<6 && Seeds.Num()>0)
        {
            const FVector Seed=Seeds[Random.RandRange(0,Seeds.Num()-1)];
            if(FVector::DistSquared2D(Seed,Viewer)<FMath::Square(1050.f))P=Seed+FVector(Random.FRandRange(-40,40),Random.FRandRange(-40,40),0);
        }
        FVector Ground,Normal;
        if(!FindGround(P,Ground,Normal))continue;
        L.Position=Ground;L.GroundZ=Ground.Z;L.Normal=Normal;
        L.LastGroundQuery=Ground;L.Age=0.f;
        L.Yaw=SourceRandom.FRandRange(-180.f,180.f);L.Roll=L.Pitch=0.f;
        L.Velocity=FVector::ZeroVector;
        return true;
    }
    L.Age=-1.f;
    return false;
}

void AHouseLeafWind::AudioPlayback(const UAudioComponent* Component,const USoundWave* Wave,float Percent)
{
    if(Component==Rustle.Get()&&Wave==RustleWave.Get()&&FMath::IsFinite(Percent))++AudioSamples;
}

void AHouseLeafWind::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bReady||!GetWorld())return;
    APlayerController* Player=GetWorld()->GetFirstPlayerController();
    if(!Player||!Player->IsLocalController()||!Player->PlayerCameraManager)return;
    const double WorldTime=GetWorld()->GetTimeSeconds();
    Review(Player,WorldTime);
    const double Time=GetWorld()->GetGameState()?GetWorld()->GetGameState()->GetServerWorldTimeSeconds():WorldTime;
    Gust=HouseLeafWindImpl::GustAt(Time);
    for(int32 K=0;K<WindMaterials.Num();++K)
        WindMaterials[K]->SetScalarParameterValue(TEXT("WindStrength"),WindAmplitudes[K]*(.25f+Gust*2.1f));
    const FVector Viewer=Player->PlayerCameraManager->GetCameraLocation();
    FVector Forward=Player->PlayerCameraManager->GetCameraRotation().Vector();
    Forward.Z=0;Forward.Normalize();
    const float Expected=GroundHeight(Viewer);
    const bool Outside=IsGroundArea(Viewer)&&Viewer.Z>Expected-35.f&&Viewer.Z<Expected+650.f;
    const float Dt=FMath::Min(DeltaSeconds,.05f);
    ListenerGain=FMath::FInterpTo(ListenerGain,Outside?1.f:0.f,Dt,3.5f);
    const float Direction=-.37f+FMath::Sin(float(Time)*.077f)*.36f;
    const FVector Wind(FMath::Cos(Direction),FMath::Sin(Direction),0),Across(-Wind.Y,Wind.X,0);
    TickWoodland(Dt,WorldTime,Viewer,Wind);
    TickFallingLeaves(Dt,Viewer,Wind);
    MovingLeaves=0;GroundChecks=0;
    FVector SoundPosition=Viewer;float Nearest=FLT_MAX;
    if(!Outside&&bLeavesVisible)
    {
        for(FHouseWindLeaf& L:Leaves)
        {
            L.Age=-1.f;
            Pools[L.Palette]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
        }
        for(UInstancedStaticMeshComponent* P:Pools)P->MarkRenderStateDirty();
        bLeavesVisible=false;
    }
    if(Outside)for(FHouseWindLeaf& L:Leaves)
    {
        if(L.Age<0 || L.Age>48.f || FVector::DistSquared2D(L.Position,Viewer)>FMath::Square(HouseLeafWindImpl::PoolRadius))
            if(!RecycleLeaf(L,Viewer,Forward))
            {
                Pools[L.Palette]->UpdateInstanceTransform(L.Instance,FTransform(FQuat::Identity,L.Position,FVector(.0001f)),true,false,true);
                continue;
            }
        L.Age+=Dt;
        const float Push=FMath::Max(0.f,Gust-L.Threshold);
        const FVector Target=Wind*(Push*165.f*L.Drag)
            +Across*(FMath::Sin(float(Time)*1.7f+L.Phase)*Push*22.f);
        L.Velocity=FMath::VInterpTo(L.Velocity,Target,Dt,Push>.02f?2.3f:5.f);
        FVector Next=L.Position+L.Velocity*Dt;
        bool Clear=IsGroundArea(Next);
        if(Clear&&GroundChecks<10&&FVector::DistSquared2D(Next,L.LastGroundQuery)>144.f)
        {
            ++GroundChecks;
            FVector Ground,Normal;
            Clear=FindGround(Next,Ground,Normal)&&FMath::Abs(Ground.Z-L.GroundZ)<16.f;
            if(Clear)
            {
                FHitResult Block;
                FCollisionQueryParams Q(SCENE_QUERY_STAT(LeafWindObstacle),true,this);
                Clear=!GetWorld()->LineTraceSingleByChannel(Block,L.LastGroundQuery+FVector(0,0,10),Ground+FVector(0,0,10),ECC_Visibility,Q);
                if(Clear){L.GroundZ=Ground.Z;L.Normal=Normal;L.LastGroundQuery=Ground;}
            }
        }
        if(Clear)L.Position=Next;else L.Velocity*=.12f;
        L.Position.Z=FMath::FInterpTo(L.Position.Z,L.GroundZ,Dt,12.f);
        const float Speed=L.Velocity.Size2D();
        if(Speed>8.f)++MovingLeaves;
        const float Tumble=FMath::SmoothStep(12.f,65.f,Speed);
        L.Phase+=Speed*Dt/(26.f*L.Size);
        L.Roll=FMath::FInterpTo(L.Roll,FMath::Sin(L.Phase)*58.f*Tumble,Dt,7.f);
        L.Pitch=FMath::FInterpTo(L.Pitch,FMath::Sin(L.Phase*1.27f+.8f)*25.f*Tumble,Dt,7.f);
        if(Speed>3.f)L.Yaw=FMath::FixedTurn(L.Yaw,L.Velocity.Rotation().Yaw+FMath::Sin(L.Phase)*28.f,Dt*80.f);
        const float Lift=.7f+FMath::Abs(FMath::Sin(L.Phase))*4.f*Tumble
            +FMath::Max(0.f,FMath::Sin(L.Phase*1.7f))*5.f*Push;
        const float Fade=FMath::SmoothStep(0.f,1.1f,L.Age);
        const FQuat GroundRotation=FRotationMatrix::MakeFromZ(L.Normal).ToQuat();
        const FQuat Rotation=GroundRotation*FRotator(L.Pitch,L.Yaw,L.Roll).Quaternion();
        Pools[L.Palette]->UpdateInstanceTransform(L.Instance,
            FTransform(Rotation,L.Position+FVector(0,0,Lift),FVector(L.Size*Fade)),true,false,true);
        const float Distance=FVector::DistSquared(L.Position,Viewer);
        if(Distance<Nearest){Nearest=Distance;SoundPosition=L.Position+FVector(0,0,18);}
    }
    if(Outside)
    {
        for(UInstancedStaticMeshComponent* P:Pools)P->MarkRenderStateDirty();
        bLeavesVisible=true;
        LastNearby=Nearest==FLT_MAX?0.f:1.f-FMath::SmoothStep(700.f,1150.f,FMath::Sqrt(Nearest));
        Rustle->SetWorldLocation(SoundPosition);
    }
    HitRustle=FMath::Max(0.f,HitRustle-Dt*.45f);
    RustleVolume=ListenerGain*FMath::Max(LastNearby*(.055f+Gust*.25f),HitRustle*.35f);
    if(HitRustle>.1f)Rustle->SetWorldLocation(HitSoundPosition);
    Rustle->SetVolumeMultiplier(RustleVolume);
    if(RustleWave&&!bSoundStarted&&Outside&&(LastNearby>.1f||HitRustle>.1f))
    {
        bSoundStarted=true;
        Rustle->Play((bWoodlandEnabled&&HouseLeafWindImpl::InWoodland(Viewer)?WoodlandRandom:Random).FRandRange(0.f,5.f));
    }
}

void AHouseLeafWind::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Rustle)Rustle->Stop();
    for(UAudioComponent* Component:WoodlandAudio)if(Component)Component->Stop();
    for(USoundWaveProcedural* Wave:WoodlandWaves)if(Wave)Wave->ResetAudio();
    if(bWoodlandDiagnostics)UE_LOG(LogTemp,Display,TEXT("WOODS_MOTION_END naturalSpawned=%d naturalLanded=%d sounds=%d allWoodlandAudioStopped=1"),
        WoodlandShedLeaves,WoodlandLandedLeaves,WoodlandSoundEvents);
    Super::EndPlay(Reason);
}

void AHouseLeafWind::ReviewView(APlayerController* Player,const FVector& From,const FVector& To)
{
#if !UE_BUILD_SHIPPING
    if(!ReviewCamera)
    {
        ReviewCamera=GetWorld()->SpawnActor<ACameraActor>();
        ReviewCamera->GetCameraComponent()->SetFieldOfView(78.f);
    }
    ReviewCamera->SetActorLocation(From);
    ReviewCamera->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(From,To));
    Player->SetViewTarget(ReviewCamera);
#endif
}

void AHouseLeafWind::Review(APlayerController* Player,double Now)
{
#if !UE_BUILD_SHIPPING
    static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("HouseLeafWindReview"));
    if(!Enabled)return;
    const FString Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("LeafWindReview"));
    if(ReviewStart<0.)
    {
        ReviewStart=Now;IFileManager::Get().MakeDirectory(*Dir,true);
        ReviewView(Player,FVector(750,-3400,58),FVector(1100,-2750,-38));
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW started: normal shared gusts, actual ground queries and native audio cursor"));
    }
    const float Age=Now-ReviewStart;
    if(Age>2.f&&Age<18.f)
    {
        ReviewMinGust=FMath::Min(ReviewMinGust,Gust);ReviewMaxGust=FMath::Max(ReviewMaxGust,Gust);
        ReviewOutsideVolume=FMath::Max(ReviewOutsideVolume,RustleVolume);
        if(ReviewPositions.Num()!=Leaves.Num())for(const FHouseWindLeaf& L:Leaves)ReviewPositions.Add(L.Position);
        else for(int32 K=0;K<Leaves.Num();++K)
            ReviewTravel=FMath::Max(ReviewTravel,float(FVector::Dist2D(Leaves[K].Position,ReviewPositions[K])));
    }
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(Dir,Name),false,false);};
    if(ReviewStage==0&&Age>4.f){Shot(TEXT("LeavesBeforeGust.png"));ReviewStage=1;}
    if(ReviewStage==1&&Age>9.f){Shot(TEXT("LeavesTumbling.png"));ReviewStage=2;}
    if(ReviewStage==2&&Age>17.f){Shot(TEXT("LeavesSettling.png"));ReviewStage=3;}
    if(ReviewStage==3&&Age>18.f)
    {ReviewView(Player,FVector(-35,-360,150),FVector(150,-100,120));ReviewStage=4;}
    if(ReviewStage==4&&Age>21.5f)
    {
        ReviewInsideVolume=RustleVolume;
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW indoors gain=%.5f native_audio_samples=%d"),RustleVolume,AudioSamples);
        ReviewView(Player,FVector(-36,-2640,85),FVector(80,-2310,-35));ReviewStage=5;
    }
    if(Now-ReviewLastReport>3.f)
    {
        ReviewLastReport=Now;
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW age=%.1f gust=%.2f moving=%d gain=%.3f native_audio_samples=%d"),Age,Gust,MovingLeaves,RustleVolume,AudioSamples);
    }
    if(ReviewStage==5&&Age>25.f)
    {
        Shot(TEXT("LeavesOnFrontWalk.png"));ReviewStage=6;
        const bool Passed=Leaves.Num()==HouseLeafWindImpl::LeafCount&&ReviewTravel>30.f&&ReviewMaxGust>.8f&&ReviewMinGust<.3f
            &&ReviewOutsideVolume>.1f&&ReviewInsideVolume<.005f&&RustleVolume>.02f&&AudioSamples>20&&Rustle->IsPlaying();
        const FString Report=FString::Printf(TEXT("%s: pool=%d drift_cm=%.1f gust_range=%.2f..%.2f outdoor_volume=%.3f indoor_volume=%.5f reentry_volume=%.3f native_audio_samples=%d playing=%d\n"),
            Passed?TEXT("PASS"):TEXT("FAIL"),Leaves.Num(),ReviewTravel,ReviewMinGust,ReviewMaxGust,ReviewOutsideVolume,ReviewInsideVolume,RustleVolume,AudioSamples,Rustle->IsPlaying());
        FFileHelper::SaveStringToFile(Report,*FPaths::Combine(Dir,TEXT("result.txt")));
        UE_LOG(LogTemp,Display,TEXT("LEAF_WIND_REVIEW %s"),*Report);
    }
    if(ReviewStage==6&&Age>27.f&&!FScreenshotRequest::IsScreenshotRequested())
    {
        FString Result;FFileHelper::LoadFileToString(Result,*FPaths::Combine(Dir,TEXT("result.txt")));
        FPlatformMisc::RequestExitWithStatus(false,Result.StartsWith(TEXT("PASS"))?0:1);
        ReviewStage=7;
    }
#endif
}
