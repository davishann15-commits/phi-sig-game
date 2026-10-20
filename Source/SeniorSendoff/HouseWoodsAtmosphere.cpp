#include "HouseWoodsAtmosphere.h"
#include "HouseWoodsDetail.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace HouseWoodsAtmosphereImpl
{
int32 NearFogLeases = 0;
bool ChangedNearFog = false;
const FName NearFogTag(TEXT("SeniorSendoffWoodsNearFog"));
float Smooth(float Value)
{
    const float Clamped = FMath::Clamp(Value, 0.f, 1.f);
    return Clamped * Clamped * (3.f - 2.f * Clamped);
}

float LowGround(float X, float Y)
{
    float Ground = HouseWoods::Height(X, Y);
    // A bank must not raise an entire 54m mist volume into the canopy. The
    // lowest nearby ground sets its reference height; hills rise out of it.
    for (float OffsetX : {-900.f, 0.f, 900.f})
        for (float OffsetY : {-900.f, 0.f, 900.f})
            Ground = FMath::Min(Ground, HouseWoods::Height(X + OffsetX, Y + OffsetY));
    return Ground;
}

void Configure(ULocalFogVolumeComponent* Fog, float Depth, bool Pool)
{
    // UE combines radial and height coverage multiplicatively. The radial
    // envelope must be strong enough for a short, 6–8m hollow volume; height
    // falloff still keeps its visible density close to the creek floor.
    Fog->RadialFogExtinction = Pool ? .30f : FMath::Lerp(.040f, .085f, Depth);
    Fog->HeightFogExtinction = Pool ? FMath::Lerp(.80f, 1.10f, Depth) : FMath::Lerp(.20f, .34f, Depth);
    Fog->HeightFogFalloff = Pool ? 1200.f : 2500.f;
    Fog->HeightFogOffset = Pool ? .08f : 0.f;
    Fog->FogAlbedo = FLinearColor(.38f, .46f, .52f);
    // Dusk uses a fixed -4 EV exposure. A small sky-scatter luminance keeps
    // the hollow mist readable without a lamp or a glowing ground plane.
    Fog->FogEmissive = Pool ? FLinearColor(.32f, .40f, .50f) : FLinearColor(.018f, .025f, .036f);
    Fog->FogPhaseG = .10f;
    Fog->FogStartDistance = Pool ? 200.f : 1000.f;
    Fog->MarkRenderStateDirty();
}
}

void HouseWoodsAtmosphere::AcquireNearFog()
{
    if (HouseWoodsAtmosphereImpl::NearFogLeases++ != 0) return;
    if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LocalFogVolume.GlobalStartDistance")))
    {
        HouseWoodsAtmosphereImpl::ChangedNearFog = Variable->GetFloat() > 200.f;
        if (HouseWoodsAtmosphereImpl::ChangedNearFog) Variable->Set(200.f, ECVF_SetByTemp, HouseWoodsAtmosphereImpl::NearFogTag);
        UE_LOG(LogTemp, Display, TEXT("WOODS_FOG_NEAR leased=1 effective=%.0f taggedTemporaryOverride=1"), Variable->GetFloat());
    }
}

void HouseWoodsAtmosphere::ReleaseNearFog()
{
    if (HouseWoodsAtmosphereImpl::NearFogLeases <= 0 || --HouseWoodsAtmosphereImpl::NearFogLeases != 0) return;
    if (HouseWoodsAtmosphereImpl::ChangedNearFog)
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LocalFogVolume.GlobalStartDistance")))
        {
            Variable->Unset(ECVF_SetByTemp, HouseWoodsAtmosphereImpl::NearFogTag);
            UE_LOG(LogTemp, Display, TEXT("WOODS_FOG_NEAR released=1 effective=%.0f"), Variable->GetFloat());
        }
    HouseWoodsAtmosphereImpl::ChangedNearFog = false;
}

float HouseWoodsAtmosphere::DepthAt(const FVector& Position)
{
    const float Rear = HouseWoodsAtmosphereImpl::Smooth((Position.Y - 4800.f) / 7000.f);
    const float Side = 1.f - HouseWoodsAtmosphereImpl::Smooth((FMath::Abs(Position.X) - 7400.f) / 1100.f);
    return Rear * Side;
}

void HouseWoodsAtmosphere::Apply(AActor* Owner)
{
    if (!Owner || !Owner->GetWorld() || FParse::Param(FCommandLine::Get(), TEXT("NoWoodsFog"))) return;
    UWorld* World = Owner->GetWorld();
    int32 Retuned = 0, Pools = 0;
    for (TActorIterator<ALocalFogVolume> It(World); It; ++It)
    {
        if (It->GetOwner() != Owner || !It->ActorHasTag(TEXT("WoodlandMist"))) continue;
        FVector Location = It->GetActorLocation();
        Location.Z = HouseWoodsAtmosphereImpl::LowGround(Location.X, Location.Y) + 42.f;
        It->SetActorLocation(Location);
        if (auto* Fog = It->GetComponent())
        {
            HouseWoodsAtmosphereImpl::Configure(Fog, DepthAt(Location), false);
            ++Retuned;
        }
    }
    for (const HouseWoods::FHollow& Hollow : HouseWoods::Hollows())
    {
        const FVector Location(Hollow.Center.X, Hollow.Center.Y,
            HouseWoods::Height(Hollow.Center.X, Hollow.Center.Y) + 38.f);
        FActorSpawnParameters Parameters;
        Parameters.Owner = Owner;
        Parameters.ObjectFlags |= RF_Transient;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Volume = World->SpawnActor<ALocalFogVolume>(ALocalFogVolume::StaticClass(), Location, FRotator::ZeroRotator, Parameters);
        if (!Volume) continue;
        Volume->Tags.Add(TEXT("WoodlandHollowMist"));
        Volume->SetActorEnableCollision(false);
        Volume->SetActorTickEnabled(false);
        const float Radius = Hollow.Radius * 1.38f;
        Volume->SetActorScale3D(FVector(Radius / ULocalFogVolumeComponent::GetBaseVolumeSize()));
        if (auto* Fog = Volume->GetComponent())
        {
            HouseWoodsAtmosphereImpl::Configure(Fog, DepthAt(Location), true);
            UE_LOG(LogTemp, Display, TEXT("WOODS_FOG_COMPONENT registered=%d visible=%d hidden=%d location=%s scale=%s"),
                int32(Fog->IsRegistered()), int32(Fog->IsVisible()), int32(Volume->IsHidden()),
                *Fog->GetComponentLocation().ToString(), *Fog->GetComponentScale().ToString());
            ++Pools;
        }
        UE_LOG(LogTemp, Display, TEXT("WOODS_FOG_POOL position=%s radius=%.0f bottom=%.0f"),
            *Location.ToString(), Radius, Location.Z - 38.f);
    }
    UE_LOG(LogTemp, Display, TEXT("WOODS_ATMOSPHERE retuned=%d lowPools=%d existingSkyAndSunUnchanged=1 newLights=0 poolNear=200 oldVolumeNear=1000 nearLimitRestoredOnEndPlay=1"), Retuned, Pools);
}
