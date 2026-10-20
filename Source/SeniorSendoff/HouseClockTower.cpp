// The campus clock tower actor: builds the tower (HouseClockTowerMesh), keeps its four dials on the clock, rings the
// Westminster Quarters (HouseClockBells) through one spatialised source in the belfry, strikes the bells with their
// hammers in step with the sound, sends the crows off the parapet on the hour and turns the weathervane in the breeze.
// Adds no lights: the dial glow and the uplight on the stone are emission.
#include "HouseClockTower.h"
#include "HouseFraternityRow.h"
#include "HouseClockTowerMesh.h"
#include "HouseBlackHall.h"
#include "HouseBlackHallMesh.h"
#include "HouseCalderTerrain.h"
#include "HouseDuskAtmosphere.h"
#include "HouseExterior.h"
#include "Async/Async.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "StoryCampaign.h"
#include "UnrealClient.h"

namespace HouseClockTowerImpl
{
constexpr float HourPitchRest = 3.f, HammerLift = 32.f, HammerLead = .32f, DetailCull = 9000.f, PartCull = 14000.f, InnerRadius = 2500.f;

TWeakObjectPtr<AHouseClockTower>& Current() { static TWeakObjectPtr<AHouseClockTower> Tower; return Tower; }

float Smooth01(float X) { X = FMath::Clamp(X, 0.f, 1.f); return X * X * (3.f - 2.f * X); }

FString Hms(double Seconds)
{
    const int32 S = FMath::FloorToInt(FMath::Fmod(FMath::Fmod(Seconds, 86400.0) + 86400.0, 86400.0));
    return FString::Printf(TEXT("%02d:%02d:%02d"), S / 3600, (S / 60) % 60, S % 60);
}

bool ParseClock(const FString& Text, double& Out)
{
    TArray<FString> Parts;
    Text.ParseIntoArray(Parts, TEXT(":"));
    if (Parts.Num() < 2) return false;
    Out = FCString::Atoi(*Parts[0]) * 3600.0 + FCString::Atoi(*Parts[1]) * 60.0 + (Parts.Num() > 2 ? FCString::Atof(*Parts[2]) : 0.0);
    return Out >= 0.0 && Out < 86400.0;
}

double WallClock() { return FDateTime::Now().GetTimeOfDay().GetTotalSeconds(); }

/** A whole chime mixed on a worker thread into one buffer, so the belfry needs a single voice however many bells ring. */
TArray<int16> Mix(const TArray<HouseClockBells::FStrike>& Strikes, float Length)
{
    // Long enough for every bell to ring out in full (each strike's PCM ends in its own short fade).
    const int32 Rate = HouseClockBells::SampleRate;
    int32 End = FMath::CeilToInt(Length * Rate);
    for (const HouseClockBells::FStrike& Strike : Strikes)
        if (const TArray<int16>* Pcm = HouseClockBells::Pcm(Strike.Bell)) End = FMath::Max(End, FMath::RoundToInt(Strike.Time * Rate) + Pcm->Num());
    TArray<float> Sum; Sum.SetNumZeroed(End);
    for (const HouseClockBells::FStrike& Strike : Strikes)
    {
        const TArray<int16>* Pcm = HouseClockBells::Pcm(Strike.Bell);
        if (!Pcm) continue;
        const float Gain = (Strike.Bell == HouseClockBells::EBell::Hour ? 1.f : .72f) * Strike.Velocity / 32000.f;
        const int32 Start = FMath::Clamp(FMath::RoundToInt(Strike.Time * Rate), 0, Sum.Num());
        const int32 Count = FMath::Min(Pcm->Num(), Sum.Num() - Start);
        for (int32 I = 0; I < Count; ++I) Sum[Start + I] += (*Pcm)[I] * Gain;
    }
    TArray<int16> Out; Out.SetNumUninitialized(Sum.Num());
    for (int32 I = 0; I < Sum.Num(); ++I)
    {
        // Linear up to .6 of full scale, then a tanh knee that approaches (never reaches) full scale: no hard clipping
        // however many bells overlap.
        const float X = Sum[I] * 1.25f, A = FMath::Abs(X);
        const float Y = A <= .6f ? A : .6f + .4f * (1.f - 2.f / (FMath::Exp(2.f * (A - .6f) / .4f) + 1.f));
        Out[I] = int16(FMath::Sign(X) * Y * 32000.f);
    }
    return Out;
}

/** Natural spreading loss from the belfry (slightly gentler than 1/r, as a big bell carries), keyed over the falloff. */
float Spread(float Distance) { return FMath::Clamp(FMath::Pow(InnerRadius / FMath::Max(Distance, InnerRadius), .75f), 0.f, 1.f); }

void ChimeCommand(const TArray<FString>& Args, UWorld* World)
{
    AHouseClockTower* Tower = AHouseClockTower::Find(World);
    if (!Tower) { UE_LOG(LogTemp, Warning, TEXT("CLOCK_TOWER no tower in this world")); return; }
    Tower->Chime(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 4, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0, TEXT("console"));
}
void TollCommand(const TArray<FString>& Args, UWorld* World) { if (AHouseClockTower* Tower = AHouseClockTower::Find(World)) Tower->Toll(TEXT("console")); }
void CrowsCommand(const TArray<FString>& Args, UWorld* World) { if (AHouseClockTower* Tower = AHouseClockTower::Find(World)) Tower->BurstCrows(TEXT("console")); }
}

static TAutoConsoleVariable<float> CVarHouseClockTowerVolume(TEXT("ClockTower.Volume"), 1.f,
    TEXT("Clock tower bell volume multiplier (0..2), on top of -ClockChimeVolume=."));
static FAutoConsoleCommandWithWorldAndArgs GHouseClockTowerChimeCommand(TEXT("ClockTower.Chime"),
    TEXT("Ring the campus clock tower now. ClockTower.Chime [quarter 1-4, 4 = the hour] [hour 1-12, default from the clock]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HouseClockTowerImpl::ChimeCommand));
static FAutoConsoleCommandWithWorldAndArgs GHouseClockTowerTollCommand(TEXT("ClockTower.Toll"), TEXT("Strike one clock tower bell once."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HouseClockTowerImpl::TollCommand));
static FAutoConsoleCommandWithWorldAndArgs GHouseClockTowerCrowsCommand(TEXT("ClockTower.Crows"), TEXT("Send the clock tower crows off the parapet."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HouseClockTowerImpl::CrowsCommand));

AHouseClockTower::AHouseClockTower()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    bReplicates = false;
    SetCanBeDamaged(false);
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("ClockTowerRoot"));
    Root->SetMobility(EComponentMobility::Static);
    RootComponent = Root;
}

AHouseClockTower* AHouseClockTower::SpawnFor(AActor* Exterior, UHouseBlackHallSubsystem* Look)
{
    UWorld* World = Exterior ? Exterior->GetWorld() : nullptr;
    if (!World || FParse::Param(FCommandLine::Get(), TEXT("NoClockTower"))) return nullptr;
    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    // At the origin like the exterior, so the static parts can be written in world space. The knoll's lift is in those world
    // positions (HouseClockTowerSite::Center and the mesh's world helpers), never in the actor, so nothing is lifted twice.
    AHouseClockTower* Tower = World->SpawnActor<AHouseClockTower>(AHouseClockTower::StaticClass(), FTransform::Identity, Params);
    if (!Tower) return nullptr;
    HouseClockTowerImpl::Current() = Tower;
    Tower->Build(Look);
    return Tower;
}

AHouseClockTower* AHouseClockTower::Find(const UWorld* World)
{
    AHouseClockTower* Tower = HouseClockTowerImpl::Current().Get();
    if (Tower && Tower->GetWorld() == World) return Tower;
    if (!World) return nullptr;
    TActorIterator<AHouseClockTower> It(const_cast<UWorld*>(World));
    return It ? *It : nullptr;
}

UProceduralMeshComponent* AHouseClockTower::MakePart(const FHallMesh& M, const FString& Name, bool bCollision, bool bMovable, float CullDistance, UHouseBlackHallSubsystem* Look, USceneComponent* Parent)
{
    if (M.NumTriangles() == 0) return nullptr;
    UProceduralMeshComponent* PM = NewObject<UProceduralMeshComponent>(this, FName(*Name));
    PM->SetMobility(bMovable ? EComponentMobility::Movable : EComponentMobility::Static);
    PM->SetupAttachment(Parent ? Parent : GetRootComponent());
    PM->SetCastShadow(false);
    PM->bUseComplexAsSimpleCollision = true;
    PM->bUseAsyncCooking = false;
    if (bCollision) { PM->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); PM->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
    else PM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (bMovable || CullDistance > 0.f) { PM->bAffectDynamicIndirectLighting = false; PM->bAffectDistanceFieldLighting = false; }
    if (CullDistance > 0.f) PM->SetCullDistance(CullDistance);
    PM->RegisterComponent();
    AddInstanceComponent(PM);
    M.Build(PM, [Look](FName Key) { return Look->Mat(Key); }, bCollision);
    Meshes.Add(PM);
    Triangles += M.NumTriangles();
    ++Parts;
    return PM;
}

void AHouseClockTower::Build(UHouseBlackHallSubsystem* Look)
{
    const double Started = FPlatformTime::Seconds();
    UWorld* World = GetWorld();
    const TCHAR* Cmd = FCommandLine::Get();
    FParse::Value(Cmd, TEXT("ClockChimeEvery="), ChimeEvery);
    FParse::Value(Cmd, TEXT("ClockTollEvery="), TollEvery);
    FParse::Value(Cmd, TEXT("ClockChimeVolume="), Volume);
    FParse::Value(Cmd, TEXT("ClockChimeFalloff="), Falloff);
    ChimeEvery = FMath::Max(0.f, ChimeEvery); TollEvery = FMath::Max(0.f, TollEvery);
    Volume = FMath::Clamp(Volume, 0.f, 2.f); Falloff = FMath::Clamp(Falloff, 5000.f, 400000.f);
    bChimeNow = FParse::Param(Cmd, TEXT("ClockChimeNow"));
    bReview = FParse::Param(Cmd, TEXT("ClockTowerReview"));
    FString ClockText;
    double Target = 0.0;
    if (FParse::Value(Cmd, TEXT("ClockTime="), ClockText) && HouseClockTowerImpl::ParseClock(ClockText, Target))
    {
        ClockOffset = Target - HouseClockTowerImpl::WallClock();
        bClockOverride = true;
    }
    Rng.Initialize(int32(FPlatformTime::Cycles() & 0x7fffffff));
    const bool bVisual = World->GetNetMode() != NM_DedicatedServer && Look;
    if (bVisual)
    {
        HouseClockTowerMesh::CreateLook(Look);
        {
            FHallMesh Solid, Upper, Detail, DialMesh;
            HouseClockTowerMesh::Build(Solid, Upper, Detail, DialMesh);
            MakePart(Solid, TEXT("ClockTowerSolid"), true, false, 0.f, Look);
            MakePart(Upper, TEXT("ClockTowerUpper"), false, false, 0.f, Look);
            MakePart(Detail, TEXT("ClockTowerDetail"), false, false, HouseClockTowerImpl::DetailCull, Look);
            MakePart(DialMesh, TEXT("ClockTowerDials"), false, false, 0.f, Look);
        }
        FHallMesh HourMesh, MinuteMesh;
        HouseClockTowerMesh::HourHand(HourMesh);
        HouseClockTowerMesh::MinuteHand(MinuteMesh);
        for (int32 F = 0; F < HouseClockTowerMesh::NumFaces; ++F)
        {
            const FVector N = HouseClockTowerMesh::FaceNormal(F), Dial = HouseClockTowerMesh::DialCenter(F);
            UProceduralMeshComponent* Hour = MakePart(HourMesh, FString::Printf(TEXT("ClockTowerHourHand%d"), F), false, true, 0.f, Look);
            UProceduralMeshComponent* Minute = MakePart(MinuteMesh, FString::Printf(TEXT("ClockTowerMinuteHand%d"), F), false, true, 0.f, Look);
            Hour->SetWorldLocation(Dial + N * 5.6f);
            Minute->SetWorldLocation(Dial + N * 9.f);
            HourHands.Add(Hour); MinuteHands.Add(Minute);
        }
        for (int32 B = 0; B < HouseClockTowerMesh::NumBells; ++B)
        {
            const float D = HouseClockTowerMesh::BellDiameter(B);
            const float Cull = B == 4 ? 0.f : HouseClockTowerImpl::PartCull;
            FHallMesh BellMesh, ClapperMesh, HammerMesh;
            HouseClockTowerMesh::Bell(BellMesh, D);
            HouseClockTowerMesh::Clapper(ClapperMesh, D);
            HouseClockTowerMesh::Hammer(HammerMesh, D);
            UProceduralMeshComponent* Bell = MakePart(BellMesh, FString::Printf(TEXT("ClockTowerBell%d"), B), false, true, Cull, Look);
            Bell->SetWorldLocation(HouseClockTowerMesh::BellPivot(B));
            UProceduralMeshComponent* Clapper = MakePart(ClapperMesh, FString::Printf(TEXT("ClockTowerClapper%d"), B), false, true, HouseClockTowerImpl::PartCull, Look, Bell);
            UProceduralMeshComponent* Hammer = MakePart(HammerMesh, FString::Printf(TEXT("ClockTowerHammer%d"), B), false, true, HouseClockTowerImpl::PartCull, Look);
            FVector Pivot, Away;
            HouseClockTowerMesh::HammerPlacement(B, Pivot, Away);
            HammerYaw[B] = Away.Rotation().Yaw;
            Hammer->SetWorldLocationAndRotation(Pivot, FRotator(HouseClockTowerImpl::HourPitchRest, HammerYaw[B], 0.f));
            Bells.Add(Bell); Clappers.Add(Clapper); Hammers.Add(Hammer);
        }
        {
            FHallMesh VaneMesh;
            HouseClockTowerMesh::Vane(VaneMesh);
            Vane = MakePart(VaneMesh, TEXT("ClockTowerVane"), false, true, 0.f, Look);
            Vane->SetWorldLocation(HouseClockTowerMesh::VanePivot());
        }
        // Crows on the merlons: one movable mesh, rewritten only while they fly or shuffle.
        TArray<FTransform> Perches;
        HouseClockTowerMesh::CrowPerches(Perches);
        for (const FTransform& Perch : Perches) { FCrow C; C.Perch = C.Pose = Perch; CrowState.Add(C); }
        Crows = NewObject<UProceduralMeshComponent>(this, TEXT("ClockTowerCrows"));
        Crows->SetMobility(EComponentMobility::Movable);
        Crows->SetupAttachment(GetRootComponent());
        Crows->SetCastShadow(false);
        Crows->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Crows->bAffectDynamicIndirectLighting = false;
        Crows->bAffectDistanceFieldLighting = false;
        Crows->RegisterComponent();
        AddInstanceComponent(Crows);
        {
            TArray<FVector> V, N; TArray<int32> I;
            for (const FCrow& C : CrowState) HouseClockTowerMesh::AppendCrow(C.Pose, 0.f, 1.f, V, N, I);
            Crows->CreateMeshSection(0, V, I, N, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
            Crows->SetMaterial(0, Look->Mat(TEXT("CT_Crow")));
            Triangles += I.Num() / 3; ++Parts;
        }
        // One spatialised voice in the belfry carries every chime.
        if (!FParse::Param(Cmd, TEXT("NoClockChimes")) && GEngine && GEngine->UseSound())
        {
            BellAttenuation = NewObject<USoundAttenuation>(this, NAME_None, RF_Transient);
            FSoundAttenuationSettings& A = BellAttenuation->Attenuation;
            A.bAttenuate = true;
            A.bSpatialize = true;
            A.AttenuationShape = EAttenuationShape::Sphere;
            A.AttenuationShapeExtents = FVector(HouseClockTowerImpl::InnerRadius, 0.f, 0.f);
            A.FalloffDistance = Falloff;
            A.DistanceAlgorithm = EAttenuationDistanceModel::Custom;
            if (FRichCurve* Curve = A.CustomAttenuationCurve.GetRichCurve())
            {
                // The engine reads the curve at (distance - inner radius) / falloff. Keys sit closer together near the tower,
                // where the law is steepest, and the curve is renormalised to reach zero at the falloff so the range is bounded.
                Curve->Reset();
                const float Floor = HouseClockTowerImpl::Spread(HouseClockTowerImpl::InnerRadius + Falloff);
                for (int32 K = 0; K <= 20; ++K)
                {
                    const float X = FMath::Square(K / 20.f);
                    Curve->AddKey(X, FMath::Max(0.f, (HouseClockTowerImpl::Spread(HouseClockTowerImpl::InnerRadius + X * Falloff) - Floor) / (1.f - Floor)));
                }
            }
            // Air soaks up the highs over distance: duller on the lawn than under the tower.
            A.bAttenuateWithLPF = true;
            A.LPFRadiusMin = 4000.f; A.LPFRadiusMax = 60000.f;
            A.LPFFrequencyAtMin = 20000.f; A.LPFFrequencyAtMax = 3200.f;
            A.bEnableOcclusion = false;
            for (int32 K = 0; K < 2; ++K)
            {
                UAudioComponent* Audio = NewObject<UAudioComponent>(this, FName(*FString::Printf(TEXT("ClockTowerBellVoice%d"), K)), RF_Transient);
                Audio->SetupAttachment(GetRootComponent());
                Audio->bAutoActivate = false;
                Audio->bAutoDestroy = false;
                Audio->bAllowSpatialization = true;
                Audio->AttenuationSettings = BellAttenuation;
                Audio->RegisterComponent();
                AddInstanceComponent(Audio);
                Audio->SetWorldLocation(HouseClockTowerSite::Center(HouseClockTowerSite::BelfryHeight));
                Voices.Add(Audio);
                VoiceWaves.Add(nullptr);
            }
        }
    }
    if (HasSound()) HouseClockBells::Prewarm();
    BuiltAt = World->GetTimeSeconds();
    NextEvery = BuiltAt + (ChimeEvery > 0.f ? ChimeEvery : 0.f);
    NextToll = BuiltAt + (TollEvery > 0.f ? Rng.FRandRange(.5f, 1.5f) * TollEvery : 1e12);
    NextFidget = BuiltAt + Rng.FRandRange(4.f, 9.f);
    UpdateHands(true);
    UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER built tris=%d parts=%d lights=0 at=(%.0f,%.0f) top=%.0f lift=%.0f dialRadius=%.0f clock=%s source=%s chimes=%s chimeEvery=%.0f tollEvery=%.0f volume=%.2f falloff=%.0f audio=%d visual=%d ms=%.0f"),
        Triangles, Parts, HouseClockTowerSite::CenterX, HouseClockTowerSite::CenterY, HouseClockTowerSite::GroundZ + HouseClockTowerSite::Lift() + HouseClockTowerSite::TopHeight,
        HouseClockTowerSite::Lift(), HouseClockTowerSite::DialRadius,
        *HouseClockTowerImpl::Hms(ClockSeconds()), bClockOverride ? TEXT("override") : TEXT("system-local"), ChimeEvery > 0.f ? TEXT("every") : TEXT("real-quarters"),
        ChimeEvery, TollEvery, Volume, Falloff, int32(HasSound()), int32(bVisual), (FPlatformTime::Seconds() - Started) * 1000.0);
}

void AHouseClockTower::EndPlay(const EEndPlayReason::Type Reason)
{
    for (UAudioComponent* Audio : Voices) if (Audio) Audio->Stop();
#if !UE_BUILD_SHIPPING
    if (bRecording)
    {
        // Cut short (quit or travel): close the review's recording so the audio device stops appending to it.
        const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClockTowerReview"));
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("ClockTowerHourChime_cut"), Dir);
        bRecording = false;
    }
    if (bReview) if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) PC->ClearAudioListenerOverride();
#endif
    if (HouseClockTowerImpl::Current().Get() == this) HouseClockTowerImpl::Current().Reset();
    Super::EndPlay(Reason);
}

double AHouseClockTower::ClockSeconds() const
{
    return FMath::Fmod(FMath::Fmod(HouseClockTowerImpl::WallClock() + ClockOffset, 86400.0) + 86400.0, 86400.0);
}

int32 AHouseClockTower::CurrentHour12() const
{
    const int32 Hour = FMath::FloorToInt(ClockSeconds() / 3600.0) % 12;
    return Hour == 0 ? 12 : Hour;
}

bool AHouseClockTower::ReadHands(int32 Face, double& OutSeconds12) const
{
    if (!HourHands.IsValidIndex(Face) || !MinuteHands.IsValidIndex(Face) || !HourHands[Face] || !MinuteHands[Face]) return false;
    const FVector R = HouseClockTowerMesh::FaceRight(Face);
    auto Angle = [&](const UProceduralMeshComponent* Hand)
    {
        const FVector Z = Hand->GetUpVector();
        const double Deg = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Z, R), FVector::DotProduct(Z, FVector::UpVector)));
        return FMath::Fmod(Deg + 360.0, 360.0);
    };
    const double MinuteDeg = Angle(MinuteHands[Face]), HourDeg = Angle(HourHands[Face]);
    // The minute hand gives minutes and seconds; the hour hand, less the minute hand's share, gives the hour.
    const int32 Hour = (FMath::RoundToInt((HourDeg - MinuteDeg / 12.0) / 30.0) % 12 + 12) % 12;
    OutSeconds12 = Hour * 3600.0 + MinuteDeg / 6.0 * 60.0;
    return true;
}

void AHouseClockTower::UpdateHands(bool bForce)
{
    const double Clock = ClockSeconds();
    const int64 Second = int64(Clock);
    if (!bForce && Second == LastHandSecond) return;
    LastHandSecond = Second;
    const double Clock12 = FMath::Fmod(Clock, 43200.0);
    const float HourA = float(FMath::DegreesToRadians(Clock12 / 3600.0 * 30.0));
    const float MinuteA = float(FMath::DegreesToRadians(FMath::Fmod(Clock, 3600.0) / 60.0 * 6.0));
    for (int32 F = 0; F < HourHands.Num(); ++F)
    {
        const FVector N = HouseClockTowerMesh::FaceNormal(F), R = HouseClockTowerMesh::FaceRight(F);
        auto Point = [&](float A) { return FVector::UpVector * FMath::Cos(A) + R * FMath::Sin(A); };
        HourHands[F]->SetWorldRotation(FRotationMatrix::MakeFromXZ(N, Point(HourA)).Rotator());
        MinuteHands[F]->SetWorldRotation(FRotationMatrix::MakeFromXZ(N, Point(MinuteA)).Rotator());
    }
}

void AHouseClockTower::LogHands(const TCHAR* Why) const
{
    const double Clock = ClockSeconds();
    double Worst = 0.0;
    FString Faces;
    for (int32 F = 0; F < HourHands.Num(); ++F)
    {
        double Read = 0.0;
        if (!ReadHands(F, Read)) continue;
        double Diff = FMath::Abs(Read - FMath::Fmod(Clock, 43200.0));
        Diff = FMath::Min(Diff, 43200.0 - Diff);
        Worst = FMath::Max(Worst, Diff);
        Faces += FString::Printf(TEXT(" face%d=%s"), F, *HouseClockTowerImpl::Hms(Read));
    }
    UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_HANDS why=%s clock=%s source=%s%s worstDiffSeconds=%.1f"), Why, *HouseClockTowerImpl::Hms(Clock),
        bClockOverride ? TEXT("override") : TEXT("system-local"), *Faces, Worst);
}

void AHouseClockTower::Chime(int32 Quarter, int32 Hour12, const TCHAR* Trigger)
{
    if (bPending || bMixing || bActive)
    {
        // Owed, not lost: it rings as soon as the bells are free (a newer request replaces an older owed one).
        OwedQuarter = FMath::Clamp(Quarter, 1, 4);
        OwedHour12 = Hour12;
        OwedTrigger = Trigger;
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_CHIME deferred=busy quarter=%d trigger=%s"), OwedQuarter, Trigger);
        return;
    }
    FChime C;
    C.Quarter = FMath::Clamp(Quarter, 1, 4);
    C.Hour12 = Hour12 >= 1 && Hour12 <= 12 ? Hour12 : CurrentHour12();
    C.HourCount = C.Quarter == 4 ? C.Hour12 : 0;
    C.Trigger = Trigger;
    C.Length = HouseClockBells::Westminster(C.Quarter, C.Hour12, ChimeSeed++, C.Strikes);
    Queue(MoveTemp(C));
}

void AHouseClockTower::Toll(const TCHAR* Trigger)
{
    if (bPending || bMixing || bActive) return;
    FChime C;
    C.bToll = true;
    C.Trigger = Trigger;
    HouseClockBells::FStrike S;
    S.Time = HouseClockBells::LeadIn;
    const float Pick = Rng.FRand();
    S.Bell = Pick < .55f ? HouseClockBells::EBell::Hour : Pick < .8f ? HouseClockBells::EBell::Q4 : HouseClockBells::EBell::Q3;
    S.Velocity = Rng.FRandRange(.5f, .78f);
    C.Strikes.Add(S);
    C.Length = S.Time + (S.Bell == HouseClockBells::EBell::Hour ? 9.f : 6.f);
    Queue(MoveTemp(C));
}

void AHouseClockTower::Queue(FChime&& C)
{
    Pending = MoveTemp(C);
    bPending = true;
    PendingSince = GetWorld()->GetAudioTimeSeconds();
}

void AHouseClockTower::StartTimeline(double Now, TArray<int16>&& Pcm)
{
    Active = MoveTemp(Pending);
    bPending = false;
    bActive = true;
    ActiveStart = Now;
    NextStrike = 0;
    HourStrikesRung = 0;
    bAudioConsumed = false;
    bool bAudio = false;
    if (HasSound() && Pcm.Num() > 0)
    {
        // A fresh wave per chime on the next voice (never ResetAudio: it only flags a reset that the audio thread applies
        // later, zeroing the count of what was queued after it, and it empties the queue from the wrong thread).
        Voice = (Voice + 1) % Voices.Num();
        UAudioComponent* Audio = Voices[Voice];
        FFeed& Feed = Feeds[Voice];
        if (Feed.bLive)
        {
            // This voice is still ringing an older chime: it gives way (de-clicked by bRequiresStopFade).
            Audio->Stop();
#if !UE_BUILD_SHIPPING
            if (Feed.bRecording && bRecording)
            {
                const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClockTowerReview"));
                IFileManager::Get().MakeDirectory(*Dir, true);
                UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("ClockTowerHourChime"), Dir);
                bRecording = false;
            }
#endif
        }
        USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this, NAME_None, RF_Transient);
        Wave->SetSampleRate(HouseClockBells::SampleRate);
        Wave->NumChannels = 1;
        Wave->Duration = INDEFINITELY_LOOPING_DURATION;
        Wave->SoundGroup = SOUNDGROUP_Default;
        Wave->bLooping = false;
        Wave->bRequiresStopFade = true;
        Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
        VoiceWaves[Voice] = Wave;
        Feed.Pcm = MoveTemp(Pcm);
        Feed.Fed = Feed.QueuedBytes = 0;
        Feed.bLive = true;
        Feed.bRecording = false;
        FeedVoice(Voice, Now);
        IndoorBlend = IndoorTarget();
        Audio->SetSound(Wave);
        Audio->SetVolumeMultiplier(Volume * FMath::Clamp(CVarHouseClockTowerVolume.GetValueOnGameThread(), 0.f, 2.f) * FMath::Lerp(1.f, .55f, IndoorBlend));
        Audio->SetLowPassFilterEnabled(IndoorBlend > .02f);
        Audio->SetLowPassFilterFrequency(FMath::Lerp(20000.f, 1700.f, IndoorBlend));
        Audio->Play();
        ++AudioPlays;
        bAudio = true;
    }
    if (Active.bToll)
    {
        ++Tolls;
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_TOLL bell=%s velocity=%.2f clock=%s trigger=%s audio=%d"), HouseClockBells::Name(Active.Strikes[0].Bell), Active.Strikes[0].Velocity,
            *HouseClockTowerImpl::Hms(ClockSeconds()), *Active.Trigger, int32(bAudio));
        return;
    }
    ++ChimesStarted;
    LastChimeHour12 = Active.Hour12;
    LastChimeQuarter = Active.Quarter;
    static const TCHAR* Kinds[5] = { TEXT("-"), TEXT("quarter_past"), TEXT("half_past"), TEXT("quarter_to"), TEXT("hour") };
    UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_CHIME kind=%s quarter=%d hour12=%d hourCount=%d strikes=%d length=%.1fs clock=%s trigger=%s audio=%d pcmSeconds=%.1f"),
        Kinds[Active.Quarter], Active.Quarter, Active.Hour12, Active.HourCount, Active.Strikes.Num(), Active.Length, *HouseClockTowerImpl::Hms(ClockSeconds()), *Active.Trigger,
        int32(bAudio), bAudio ? Feeds[Voice].Pcm.Num() / float(HouseClockBells::SampleRate) : 0.f);
    LogHands(TEXT("chime"));
#if !UE_BUILD_SHIPPING
    if (bReview && bAudio && !bRecording && Active.Quarter == 4)
    {
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this, Feeds[Voice].Pcm.Num() / float(HouseClockBells::SampleRate) + 2.f);
        bRecording = true;
        Feeds[Voice].bRecording = true;
    }
#endif
}

void AHouseClockTower::UpdateSchedule(double Now)
{
    // Every quarter on the clock, or (for testing) every -ClockChimeEvery seconds: the hour, then the three quarters.
    const double Clock = ClockSeconds();
    const int64 Quarter = int64(Clock / 900.0);
    if (OwedQuarter > 0 && !bPending && !bMixing && !bActive)
    {
        const int32 Q = OwedQuarter, H = OwedHour12;
        OwedQuarter = 0;
        Chime(Q, H, *OwedTrigger);
    }
    if (ChimeEvery > 0.f)
    {
        if (Now >= NextEvery && !bPending && !bMixing && !bActive)
        {
            static const int32 Cycle[4] = { 4, 1, 2, 3 };
            Chime(Cycle[EveryCycle++ % 4], 0, TEXT("every"));
            NextEvery = Now + ChimeEvery;
        }
    }
    else if (LastQuarterIndex >= 0 && Quarter != LastQuarterIndex)
    {
        const int32 Minute = int32((Quarter * 15) % 60);
        Chime(Minute == 0 ? 4 : Minute / 15, CurrentHour12(), TEXT("clock"));
    }
    LastQuarterIndex = Quarter;
    if (bChimeNow && Now - BuiltAt > 3.0 && (!HasSound() || HouseClockBells::Pcm(HouseClockBells::EBell::Hour)))
    {
        bChimeNow = false;
        Chime(4, 0, TEXT("switch"));
    }
    // Now and then a single bell, never near a quarter.
    if (Now >= NextToll)
    {
        const double ToQuarter = 900.0 - FMath::Fmod(Clock, 900.0);
        const bool bQuiet = !bPending && !bMixing && !bActive && OwedQuarter == 0 && GetWorld()->GetAudioTimeSeconds() - LastChimeEnd > 30.0;
        if (bQuiet && (ChimeEvery > 0.f || ToQuarter > 45.0)) Toll(TEXT("random"));
        NextToll = Now + Rng.FRandRange(.5f, 1.5f) * FMath::Max(30.f, TollEvery);
    }
    if (Now >= NextHandsLog)
    {
        NextHandsLog = Now + 60.0;
        LogHands(TEXT("minute"));
    }
}

void AHouseClockTower::UpdateStrikes(double Now)
{
    const double T = Now - ActiveStart;
    while (NextStrike < Active.Strikes.Num() && Active.Strikes[NextStrike].Time <= T)
    {
        const HouseClockBells::FStrike& S = Active.Strikes[NextStrike];
        const int32 B = FMath::Clamp(int32(S.Bell), 0, 4);
        LastStrikeAt[B] = Now;
        LastStrikeVelocity[B] = S.Velocity;
        ++StrikesStruck;
        if (S.bHourCount) ++HourStrikesRung;
        if (!Active.bToll && NextStrike == 0)
        {
            // Listeners hear about the chime as its first bell sounds; the crows leave on the hour.
            OnClockTowerChime.Broadcast(Active.Hour12, Active.Quarter < 4);
            if (Active.Quarter == 4) BurstCrows(TEXT("hour"));
        }
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_STRIKE n=%d/%d bell=%s t=%.2f velocity=%.2f hourCount=%d"), NextStrike + 1, Active.Strikes.Num(),
            HouseClockBells::Name(S.Bell), S.Time, S.Velocity, S.bHourCount ? HourStrikesRung : 0);
        ++NextStrike;
    }
    if (T > Active.Length)
    {
        // The timeline (events, hammers) ends here; the voice rings on until its PCM has played out.
        bActive = false;
        LastChimeEnd = Now;
        if (!Active.bToll)
        {
            ++ChimesFinished;
            if (Active.Quarter == 4) { LastHourCount = HourStrikesRung; LastHourExpected = Active.HourCount; }
            UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_CHIME_DONE quarter=%d hour12=%d hourStrikes=%d strikes=%d audioConsumed=%d"), Active.Quarter, Active.Hour12, HourStrikesRung,
                NextStrike, int32(bAudioConsumed));
        }
    }
}

void AHouseClockTower::UpdateBells(double Now)
{
    for (int32 B = 0; B < Bells.Num(); ++B)
    {
        const double Since = Now - LastStrikeAt[B];
        double Until = 1e9;
        if (bActive)
            for (int32 K = NextStrike; K < Active.Strikes.Num(); ++K)
                if (int32(Active.Strikes[K].Bell) == B) { Until = ActiveStart + Active.Strikes[K].Time - Now; break; }
        const bool bMoving = Since < 4.0 || Until < HouseClockTowerImpl::HammerLead + .05;
        if (!bMoving && !bBellMoving[B]) continue;
        bBellMoving[B] = bMoving;
        // The hammer lifts just before its strike and drops onto the sound bow on the beat, then settles off the bell.
        float Pitch = HouseClockTowerImpl::HourPitchRest;
        if (Until < HouseClockTowerImpl::HammerLead)
        {
            const float U = 1.f - float(Until) / HouseClockTowerImpl::HammerLead;
            Pitch = U < .65f ? FMath::Lerp(HouseClockTowerImpl::HourPitchRest, HouseClockTowerImpl::HammerLift, HouseClockTowerImpl::Smooth01(U / .65f))
                : HouseClockTowerImpl::HammerLift * (1.f - FMath::Square((U - .65f) / .35f));
        }
        else if (Since < .25) Pitch = HouseClockTowerImpl::HourPitchRest * HouseClockTowerImpl::Smooth01(float(Since) / .25f) + 2.5f * FMath::Sin(PI * float(Since) / .25f);
        if (Hammers.IsValidIndex(B) && Hammers[B]) Hammers[B]->SetWorldRotation(FRotator(Pitch, HammerYaw[B], 0.f));
        // The struck bell shivers on its headstock and its clapper swings a little.
        const float S = float(FMath::Min(Since, 10.0)), V = LastStrikeVelocity[B];
        const float Shiver = (B == 4 ? 1.3f : 2.2f) * V * FMath::Exp(-S / .55f) * FMath::Sin(2.f * PI * 2.4f * S);
        const float Swing = 3.2f * V * FMath::Exp(-S / 1.1f) * FMath::Sin(2.f * PI * 1.15f * S);
        Bells[B]->SetRelativeRotation(FRotator(0.f, 0.f, bMoving ? Shiver : 0.f));
        if (Clappers.IsValidIndex(B) && Clappers[B]) Clappers[B]->SetRelativeRotation(FRotator(0.f, 0.f, bMoving ? Swing : 0.f));
    }
}

void AHouseClockTower::BurstCrows(const TCHAR* Trigger)
{
    if (!Crows) return;
    const FVector Hub = HouseClockTowerSite::Center(0.f);
    const float Direction = Rng.FRand() < .5f ? 1.f : -1.f;
    for (FCrow& C : CrowState)
    {
        if (C.Start >= 0.0) continue;
        C.Start = GetWorld()->GetTimeSeconds();
        C.Delay = Rng.FRandRange(0.f, 1.4f);
        C.Omega = Rng.FRandRange(.17f, .3f) * (Rng.FRand() < .85f ? Direction : -Direction);
        // Nearly one full circuit: the circle ends a little short of the crow's own perch (take-off already carried it 3 s
        // round), so it lands from its own side of the tower, never crossing the spire or doubling back.
        C.Duration = 3.f + 5.5f + (2.f * PI - 3.f * FMath::Abs(C.Omega) - Rng.FRandRange(.6f, 1.f)) / FMath::Abs(C.Omega);
        C.Radius = Rng.FRandRange(900.f, 1800.f);
        C.Height = Rng.FRandRange(4300.f, 6600.f);
        const FVector Off = C.Perch.GetLocation() - Hub;
        C.Phase = FMath::Atan2(Off.Y, Off.X);
    }
    bCrowsFlying = true;
    ++CrowBursts;
    UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_CROWS burst=%d trigger=%s clock=%s"), CrowState.Num(), Trigger, *HouseClockTowerImpl::Hms(ClockSeconds()));
}

void AHouseClockTower::UpdateCrows(double Now)
{
    if (!Crows) return;
    if (!bCrowsFlying)
    {
        // A perched crow shuffles round now and then.
        if (Now < NextFidget || CrowState.Num() == 0) return;
        NextFidget = Now + Rng.FRandRange(4.f, 9.f);
        FCrow& C = CrowState[Rng.RandRange(0, CrowState.Num() - 1)];
        const FRotator Base = C.Perch.Rotator();
        C.Pose = FTransform(Base + FRotator(0.f, Rng.FRandRange(-35.f, 35.f), 0.f), C.Perch.GetLocation());
        WriteCrows();
        return;
    }
    const FVector Hub = HouseClockTowerSite::Center(0.f);
    bool bAny = false;
    for (FCrow& C : CrowState)
    {
        if (C.Start < 0.0) continue;
        const float T = float(Now - C.Start - C.Delay);
        if (T < 0.f) { bAny = true; continue; }
        if (T > C.Duration) { C.Start = -1.0; C.Pose = C.Perch; C.Flap = 0.f; C.Fold = 1.f; continue; }
        bAny = true;
        const FVector P0 = C.Perch.GetLocation(), Out = C.Perch.GetRotation().GetForwardVector().GetSafeNormal2D();
        constexpr float Take = 3.f, Land = 5.5f;
        auto Orbit = [&](float Tt)
        {
            const float A = C.Phase + C.Omega * Tt, R = C.Radius + 220.f * FMath::Sin(.53f * Tt + C.Phase), H = C.Height + 260.f * FMath::Sin(.37f * Tt + 2.f * C.Phase);
            return FVector(Hub.X + FMath::Cos(A) * R, Hub.Y + FMath::Sin(A) * R, Hub.Z + H);
        };
        // Velocity-continuous cubics into and out of the circle: no kink at take-off, and the landing leaves the circle at
        // orbit speed and arrives on the perch at rest.
        auto Cubic = [](const FVector& A, const FVector& B, const FVector& D, const FVector& E, float U) { const float V = 1.f - U; return A * (V * V * V) + B * (3.f * V * V * U) + D * (3.f * V * U * U) + E * (U * U * U); };
        auto OrbitVel = [&](float Tt) { return (Orbit(Tt + .01f) - Orbit(Tt - .01f)) / .02f; };
        const float LandAt = C.Duration - Land;
        auto Path = [&](float Tt)
        {
            if (Tt < Take) return Cubic(P0, P0 + Out * 520.f + FVector(0, 0, 120.f), Orbit(Take) - OrbitVel(Take) * (Take / 3.f), Orbit(Take), Tt / Take);
            if (Tt > LandAt)
            {
                const float S = FMath::Min((Tt - LandAt) / Land, 1.f), U = S + S * S - S * S * S;
                return Cubic(Orbit(LandAt), Orbit(LandAt) + OrbitVel(LandAt) * (Land / 3.f), P0 + Out * 650.f + FVector(0, 0, 320.f), P0, U);
            }
            return Orbit(Tt);
        };
        const FVector P = Path(T), Ahead = Path(T + .06f);
        FVector Heading = Ahead - P;
        if (Heading.SizeSquared() < 1e-4f) Heading = Out;
        FRotator Facing = Heading.Rotation();
        Facing.Pitch = FMath::Clamp(Facing.Pitch, -35.f, 35.f);
        // Bank into the circle, easing in after take-off and out before landing.
        const float Bank = HouseClockTowerImpl::Smooth01((T - Take) / .6f) * HouseClockTowerImpl::Smooth01((C.Duration - Land - T) / .6f);
        Facing.Roll = FMath::Sign(C.Omega) * 24.f * Bank;
        FQuat Rot = Facing.Quaternion();
        // Over the last moments of the landing, turn to face the way the perch faces.
        const float Settle = HouseClockTowerImpl::Smooth01((T - (C.Duration - .7f)) / .7f);
        if (Settle > 0.f) Rot = FQuat::Slerp(Rot, C.Perch.GetRotation(), Settle);
        C.Pose = FTransform(Rot, P);
        // Hard flapping off the ledge, bursts and glides round the spire, flapping and folding in to land.
        const float Gate = FMath::Sin(.8f * T + C.Phase) > -.25f ? 1.f : .12f;
        C.Flap = T < Take ? FMath::Sin(2.f * PI * 6.f * T) : T > C.Duration - Land ? FMath::Sin(2.f * PI * 5.f * T) * (1.f - (T - (C.Duration - Land)) / Land)
            : Gate * FMath::Sin(2.f * PI * 4.2f * T + C.Phase) + (1.f - Gate) * .15f;
        C.Fold = T < .35f ? 1.f - T / .35f : T > C.Duration - .9f ? (T - (C.Duration - .9f)) / .9f : 0.f;
    }
    WriteCrows();
    if (!bAny)
    {
        bCrowsFlying = false;
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_CROWS landed=%d"), CrowState.Num());
    }
}

void AHouseClockTower::WriteCrows()
{
    TArray<FVector> V, N; TArray<int32> I;
    V.Reserve(CrowState.Num() * 72); N.Reserve(CrowState.Num() * 72);
    for (const FCrow& C : CrowState) HouseClockTowerMesh::AppendCrow(C.Pose, C.Flap, C.Fold, V, N, I);
    Crows->UpdateMeshSection(0, V, N, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>());
}

float AHouseClockTower::IndoorTarget() const
{
    // Indoors (the house's three blocks) the bells duck a little and lose their brightness through the walls.
    if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
        return 1.f - FMath::Clamp(UHouseDuskAtmosphereComponent::DistanceOutsideHouse(Camera->GetCameraLocation()) / 80.f, 0.f, 1.f);
    return 0.f;
}

void AHouseClockTower::FeedVoice(int32 Index, double Now)
{
    FFeed& Feed = Feeds[Index];
    USoundWaveProcedural* Wave = VoiceWaves[Index];
    if (!Feed.bLive || !Wave) return;
    const int32 Rate = HouseClockBells::SampleRate, Chunk = Rate / 2;
    // Keep about six seconds queued ahead of the mixer: enough to ride out a long game-thread hitch (a screenshot, a
    // level stream) without an underrun that would leave the bells trailing the hammers, while the procedural wave's
    // working buffer stays a few hundred kilobytes.
    while (Feed.Fed < Feed.Pcm.Num() && Wave->GetAvailableAudioByteCount() < 6 * Rate * int32(sizeof(int16)))
    {
        const int32 Count = FMath::Min(Chunk, Feed.Pcm.Num() - Feed.Fed);
        Wave->QueueAudio(reinterpret_cast<const uint8*>(Feed.Pcm.GetData() + Feed.Fed), Count * int32(sizeof(int16)));
        Feed.Fed += Count;
        Feed.QueuedBytes += Count * int32(sizeof(int16));
    }
    const int32 Played = Feed.QueuedBytes - Wave->GetAvailableAudioByteCount();
    // The mixer pulling our PCM is the proof the chime actually played.
    if (Index == Voice && !bAudioConsumed && Played > Rate * int32(sizeof(int16)))
    {
        bAudioConsumed = true;
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_AUDIO playing=%d playedSeconds=%.2f of=%.1f after=%.2fs indoor=%.2f voice=%d"), int32(Voices[Index]->IsPlaying()),
            Played / float(Rate * sizeof(int16)), Feed.Pcm.Num() / float(Rate), Now - ActiveStart, IndoorBlend, Index);
    }
    if (Feed.Fed >= Feed.Pcm.Num() && Wave->GetAvailableAudioByteCount() <= 0)
    {
        Voices[Index]->Stop();
        Feed.bLive = false;
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_AUDIO rang_out voice=%d seconds=%.1f"), Index, Feed.Pcm.Num() / float(Rate));
        Feed.Pcm.Empty();
#if !UE_BUILD_SHIPPING
        if (Feed.bRecording && bRecording)
        {
            const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClockTowerReview"));
            IFileManager::Get().MakeDirectory(*Dir, true);
            UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("ClockTowerHourChime"), Dir);
            bRecording = false;
            UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_RECORDING wav=%s"), *(Dir / TEXT("ClockTowerHourChime.wav")));
        }
#endif
        Feed.bRecording = false;
    }
}

void AHouseClockTower::UpdateAudio(double Now, float Dt)
{
    if (!HasSound()) return;
    IndoorBlend = FMath::FInterpTo(IndoorBlend, IndoorTarget(), Dt, 3.f);
    const float Gain = Volume * FMath::Clamp(CVarHouseClockTowerVolume.GetValueOnGameThread(), 0.f, 2.f) * FMath::Lerp(1.f, .55f, IndoorBlend);
    for (int32 K = 0; K < Voices.Num(); ++K)
    {
        if (!Feeds[K].bLive) continue;
        Voices[K]->SetVolumeMultiplier(Gain);
        Voices[K]->SetLowPassFilterEnabled(IndoorBlend > .02f);
        if (IndoorBlend > .02f) Voices[K]->SetLowPassFilterFrequency(FMath::Lerp(20000.f, 1700.f, IndoorBlend));
        FeedVoice(K, Now);
    }
}

void AHouseClockTower::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // World time drives the schedule, crows and vane; the chime timeline runs on the audio clock (real time, undilated,
    // paused with the game's sounds) so the hammers stay on the beat of what is heard.
    const double Now = GetWorld()->GetTimeSeconds(), AudioNow = GetWorld()->GetAudioTimeSeconds();
    UpdateHands(false);
    UpdateSchedule(Now);
    if (bPending && !bMixing)
    {
        bool bReady = true;
        for (const HouseClockBells::FStrike& S : Pending.Strikes) bReady &= HouseClockBells::Pcm(S.Bell) != nullptr;
        if (!HasSound() || (!bReady && AudioNow - PendingSince > 8.0)) StartTimeline(AudioNow, TArray<int16>());   // no sound: the bells still move and the event fires
        else if (bReady)
        {
            bMixing = true;
            const TArray<HouseClockBells::FStrike> Strikes = Pending.Strikes;
            const float Length = Pending.Length;
            MixFuture = Async(EAsyncExecution::ThreadPool, [Strikes, Length]() { return HouseClockTowerImpl::Mix(Strikes, Length); });
        }
    }
    if (bMixing && MixFuture.IsReady())
    {
        TArray<int16> Pcm = MixFuture.Consume();
        bMixing = false;
        StartTimeline(AudioNow, MoveTemp(Pcm));
    }
    if (bActive) UpdateStrikes(AudioNow);
    UpdateBells(AudioNow);
    UpdateCrows(Now);
    // The vane swings about the evening breeze (the dusk smoke drifts toward +X, -Y; the arrow points into the wind).
    if (Vane) Vane->SetRelativeRotation(FRotator(0.f, 150.6f + 14.f * FMath::Sin(.13f * float(Now)) + 6.f * FMath::Sin(.41f * float(Now) + 1.3f), 0.f));
    UpdateAudio(AudioNow, DeltaSeconds);
    if (bReview) TickReview(Now);
}

void AHouseClockTower::TickReview(double Now)
{
#if !UE_BUILD_SHIPPING
    UWorld* World = GetWorld();
    APlayerController* PC = World->GetFirstPlayerController();
    const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ClockTowerReview"));
    // The review's own camera: the same spots as the ExViews detail_clocktower_* views. The tower's knoll lift moves every
    // target with the tower; Far's and Mid's eyes stay put (Far is on the frozen frat lawn, the sight line's origin; Mid is
    // 15 m up), Face's and Belfry's ride up with the tower (they stand within 15 m of its axis, so they keep their framing).
    struct FShotView { FVector Pos, Target; float Fov; };
    const FVector L(0.f, 0.f, HouseClockTowerSite::Lift());
    const FShotView Far = { FVector(2400, -3750, 140), FVector(-9000, -10800, 4366) + L, 40.f };
    const FShotView Mid = { FVector(-420, -8100, 1500), FVector(-9000, -10800, 3150) + L, 70.f };
    const FShotView Face = { FVector(-7800, -10620, 3990) + L, FVector(-8688, -10800, 3940) + L, 40.f };
    const FShotView Belfry = { FVector(-7700, -10480, 3060) + L, FVector(-9000, -10800, 3150) + L, 46.f };
    auto Look = [&](const FShotView& V)
    {
        if (ACameraActor* Cam = ReviewCamera.Get())
        {
            Cam->SetActorLocationAndRotation(V.Pos, (V.Target - V.Pos).Rotation());
            Cam->GetCameraComponent()->SetFieldOfView(V.Fov);
        }
    };
    auto Shot = [&](const TCHAR* Name)
    {
        FScreenshotRequest::RequestScreenshot(Dir / Name, false, false);
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_REVIEW_SHOT %s t=%.2f clock=%s"), Name, ReviewChimeStart >= 0.0 ? World->GetAudioTimeSeconds() - ReviewChimeStart : -1.0, *HouseClockTowerImpl::Hms(ClockSeconds()));
        ++ReviewShot;
    };
    if (ReviewPhase == 0)
    {
        if (Now - BuiltAt < 6.0 || !PC || !HouseCalderTerrain::CollisionReady(World)) return;
        ReviewStarted = Now;
        IFileManager::Get().MakeDirectory(*Dir, true);
        // Built pieces, collision and culling policy, and no lights.
        int32 Collides = 0, Culled = 0, Bad = 0;
        for (const UProceduralMeshComponent* PM : Meshes)
        {
            const bool bSolid = PM->GetName() == TEXT("ClockTowerSolid");
            Collides += int32(PM->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
            Bad += int32(bSolid == (PM->GetCollisionEnabled() == ECollisionEnabled::NoCollision)) + int32(PM->CastShadow);
            Culled += int32(PM->LDMaxDrawDistance > 0.f);
        }
        TInlineComponentArray<ULocalLightComponent*> TowerLights(this);
        int32 ExteriorLights = -1;
        for (TActorIterator<AHouseExteriorDetail> It(World); It; ++It) ExteriorLights = It->Lights;
        LogHands(TEXT("review"));
        double Worst = 0.0;
        for (int32 F = 0; F < HourHands.Num(); ++F)
        {
            double Read = 0.0;
            if (!ReadHands(F, Read)) { Worst = 1e9; continue; }
            const double Diff = FMath::Abs(Read - FMath::Fmod(ClockSeconds(), 43200.0));
            Worst = FMath::Max(Worst, FMath::Min(Diff, 43200.0 - Diff));
        }
        ReviewNotes = FString::Printf(TEXT("parts=%d tris=%d collidingParts=%d culledParts=%d policyViolations=%d towerLights=%d exteriorLights=%d handsWorstDiffSeconds=%.1f"),
            Parts, Triangles, Collides, Culled, Bad, TowerLights.Num(), ExteriorLights, Worst);
        const bool bStatic = Parts > 20 && Collides == 1 && Culled >= 1 && Bad == 0 && TowerLights.Num() == 0 && ExteriorLights >= 0 && ExteriorLights <= HouseFraternityRow::ExteriorLightBudget() && Worst < 5.0;
        if (!bStatic) ReviewNotes += TEXT(" staticChecks=FAIL");
        // Listen from the frat lawn whatever the camera is doing, and watch from the review camera.
        PC->SetAudioListenerOverride(nullptr, Far.Pos, (Far.Target - Far.Pos).Rotation());
        FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn; P.ObjectFlags |= RF_Transient;
        if (ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, P))
        {
            Cam->GetCameraComponent()->bConstrainAspectRatio = false;
            if (const AStoryFirstPersonCharacter* Pawn = Cast<AStoryFirstPersonCharacter>(PC->GetPawn()))
                if (Pawn->FirstPersonCamera) Cam->GetCameraComponent()->PostProcessSettings = Pawn->FirstPersonCamera->PostProcessSettings;
            ReviewCamera = Cam;
            PC->SetViewTarget(Cam);
            if (AHUD* Hud = PC->GetHUD()) Hud->bShowHUD = false;
            Look(Mid);
        }
        if (ChimeEvery <= 0.f && !bChimeNow) Chime(4, 0, TEXT("review"));
        ReviewPhase = 1;
        return;
    }
    if (ReviewPhase == 1)
    {
        // Pictures through the hour chime: the crows leaving, a hammer lifted and striking, the dial after.
        if (bActive && !Active.bToll && Active.Quarter == 4 && ReviewChimeStart < 0.0) ReviewChimeStart = ActiveStart;
        if (ReviewChimeStart >= 0.0 && !FScreenshotRequest::IsScreenshotRequested())
        {
            const double T = World->GetAudioTimeSeconds() - ReviewChimeStart;
            float FirstCount = -1.f;
            for (const HouseClockBells::FStrike& S : Active.Strikes) if (S.bHourCount) { FirstCount = S.Time; break; }
            // Camera moves happen on a tick after the shot they follow, so each picture shows the view it is named for.
            const bool bCount = FirstCount > 0.f;
            if (ReviewShot == 0 && T > 2.4) Shot(TEXT("1_crows_take_off_mid.png"));
            else if (ReviewShot == 1 && T > 7.0) Shot(TEXT("2_crows_circling_mid.png"));
            else if (ReviewShot == 2) { Look(Belfry); ++ReviewShot; }
            else if (ReviewShot == 3 && bCount && T > FirstCount - .17) Shot(TEXT("3_hammer_lifted_belfry.png"));
            else if (ReviewShot == 4 && bCount && T > FirstCount + .02) Shot(TEXT("4_hammer_strikes_belfry.png"));
            else if (ReviewShot == 5 && bCount && T > FirstCount + .45) Shot(TEXT("5_bell_shivers_belfry.png"));
            else if (ReviewShot == 6) { Look(Face); ++ReviewShot; }
            else if (ReviewShot == 7 && bCount && T > FirstCount + 2.5) { Shot(TEXT("6_dial_during_count_face.png")); LogHands(TEXT("review_shot")); }
            else if (ReviewShot == 8) { Look(Far); ++ReviewShot; }
            else if (ReviewShot == 9 && bCount && T > FirstCount + 5.0) Shot(TEXT("7_from_the_lawn_far.png"));
        }
        if (bAudioConsumed && LastChimeQuarter == 4) bReviewHourHeard = true;
        const bool bDone = ChimesFinished >= 2 && LastHourCount > 0;
        if (!bDone && Now - ReviewStarted < FMath::Max(170.0, ChimeEvery * 3.0 + 140.0)) return;
        ReviewPhase = 2;
    }
    if (ReviewPhase == 2)
    {
        if (bRecording || FScreenshotRequest::IsScreenshotRequested()) return;
        const bool bSound = HasSound();
        const bool bPass = !ReviewNotes.Contains(TEXT("FAIL")) && ReviewNotes.Contains(TEXT("policyViolations=0")) && ChimesFinished >= 1 && LastHourCount >= 1 && LastHourCount <= 12
            && LastHourCount == LastHourExpected && StrikesStruck > 0 && (!bSound || (AudioPlays > 0 && bReviewHourHeard)) && Parts > 20;
        const FString Result = FString::Printf(TEXT("%s %s chimesStarted=%d chimesFinished=%d hourChimeHour12=%d hourStrikesRung=%d strikes=%d tolls=%d audioPlays=%d audioConsumed=%d sound=%d crowBursts=%d shots=%d clock=%s source=%s chimeEvery=%.0f"),
            bPass ? TEXT("PASS") : TEXT("FAIL"), *ReviewNotes, ChimesStarted, ChimesFinished, LastHourExpected, LastHourCount, StrikesStruck, Tolls, AudioPlays, int32(bReviewHourHeard), int32(bSound),
            CrowBursts, ReviewShot, *HouseClockTowerImpl::Hms(ClockSeconds()), bClockOverride ? TEXT("override") : TEXT("system-local"), ChimeEvery);
        IFileManager::Get().MakeDirectory(*Dir, true);
        FFileHelper::SaveStringToFile(Result, *(Dir / TEXT("result.txt")));
        UE_LOG(LogTemp, Display, TEXT("CLOCK_TOWER_REVIEW %s"), *Result);
        ReviewPhase = 3;
        if (!FParse::Param(FCommandLine::Get(), TEXT("ClockTowerReviewStay"))) FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
    }
#endif
}
