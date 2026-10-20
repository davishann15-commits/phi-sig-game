// Thirty-minute Calder dusk-to-night progression; existing lights/sky only.
#include "HouseCalderFeatures.h"
#include "HouseCalderLandscape.h"
#include "HouseCampusInteriors.h"
#include "HouseDuskAtmosphere.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/StrongObjectPtr.h"
namespace HouseCalderNightfall
{
constexpr double Duration=1800.;
struct FFog
{
    TWeakObjectPtr<UExponentialHeightFogComponent> Component;
    FLinearColor Color,Glow;float Density=0.,Opacity=0.;
};
struct FState
{
    TWeakObjectPtr<ADirectionalLight> Sun;TWeakObjectPtr<USkyLightComponent> Sky;
    TWeakObjectPtr<USkyAtmosphereComponent> Atmosphere;TArray<FFog> Fog;
    FRotator SunRotation;FLinearColor SkyTint;float SkyIntensity=0.;
    TWeakObjectPtr<UPostProcessComponent> Film;
    TStrongObjectPtr<UTextureCube> FillCube,OriginalCube;
    FLinearColor OriginalTint;float OriginalIntensity=0.,FillStrength=.10f;
    bool OriginalTintOverride=false,OriginalIntensityOverride=false;
    double Next=0.,NextLog=0.,LastAlpha=-1.;bool Ready=false;
};
TMap<TWeakObjectPtr<UWorld>,FState> States;
double Seconds(UWorld* World,bool& Pinned)
{
    Pinned=false;
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("CalderNightfallReview"))||FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorViews")))
    {float Time=0.;if(FParse::Value(FCommandLine::Get(),TEXT("CalderNightfallSeconds="),Time)&&FMath::IsFinite(Time)){Pinned=true;return FMath::Max(0.,double(Time));}}
#endif
    if(const auto* GS=World->GetGameState())return GS->GetServerWorldTimeSeconds();
    return World->GetTimeSeconds();
}
bool Initialize(UWorld* W,FState& S)
{
    bool DuskBuilt=false;
    for(TActorIterator<AActor> It(W);It;++It)if(It->FindComponentByClass<UHouseDuskAtmosphereComponent>()){DuskBuilt=true;break;}
    if(!DuskBuilt)return false;
    TActorIterator<ADirectionalLight> Light(W);if(Light){S.Sun=*Light;S.SunRotation=Light->GetActorRotation();}
    for(TActorIterator<ASkyLight> It(W);It;++It)if(auto* C=It->GetLightComponent()){S.Sky=C;S.SkyIntensity=C->Intensity;break;}
    for(TActorIterator<AActor> It(W);It;++It)if(auto* C=It->FindComponentByClass<USkyAtmosphereComponent>()){S.Atmosphere=C;S.SkyTint=C->SkyLuminanceFactor;break;}
    if(!S.Sun.IsValid()||!S.Sky.IsValid()||!S.Atmosphere.IsValid())return false;
    for(TActorIterator<AExponentialHeightFog> It(W);It;++It)if(auto* C=It->GetComponent())
    {FFog F;F.Component=C;F.Color=C->FogInscatteringLuminance;F.Glow=C->DirectionalInscatteringLuminance;F.Density=C->FogDensity;F.Opacity=C->FogMaxOpacity;S.Fog.Add(F);}
    for(TActorIterator<AActor> It(W);It;++It)
    {
        TInlineComponentArray<UPostProcessComponent*> Parts(*It);
        for(auto* C:Parts)if(C->GetFName()==TEXT("ExteriorDuskFilmLook")){S.Film=C;break;}
        if(S.Film.IsValid())break;
    }
    if(auto* C=S.Film.Get())
    {
        const auto& P=C->Settings;S.OriginalCube.Reset(P.AmbientCubemap);S.OriginalTint=P.AmbientCubemapTint;S.OriginalIntensity=P.AmbientCubemapIntensity;
        S.OriginalTintOverride=P.bOverride_AmbientCubemapTint;S.OriginalIntensityOverride=P.bOverride_AmbientCubemapIntensity;
        S.FillCube.Reset(UTextureCube::CreateTransient(32,32,PF_FloatRGBA,MakeUniqueObjectName(GetTransientPackage(),UTextureCube::StaticClass(),TEXT("CalderNightAmbient"))));
        if(auto* Cube=S.FillCube.Get())
        {
            Cube->SRGB=false;Cube->NeverStream=true;Cube->Filter=TF_Bilinear;
            auto& Bulk=Cube->GetPlatformData()->Mips[0].BulkData;auto* Pixels=static_cast<FFloat16Color*>(Bulk.Lock(LOCK_READ_WRITE));
            for(int32 I=0;I<6*32*32;++I)Pixels[I]=FFloat16Color(FLinearColor::White);
            Bulk.Unlock();Cube->UpdateResource();
        }
    }
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("HouseExteriorViews")))FParse::Value(FCommandLine::Get(),TEXT("CalderNightFill="),S.FillStrength);
#endif
    S.FillStrength=FMath::Clamp(S.FillStrength,0.f,2.f);
    S.Ready=true;UE_LOG(LogTemp,Display,TEXT("CALDER_NIGHTFALL_READY durationSeconds=1800 sharedGameStateClock=1 newActors=0 newLights=0 indoorFixturesUnchanged=1 fogDensityOpacityUnchanged=1 ambientFill=%.3f transientCubeBytes=49152"),S.FillStrength);return true;
}
void Fill(UWorld* W,FState& S)
{
    auto* Film=S.Film.Get();auto* PC=W->GetFirstPlayerController();if(!Film||!PC||!PC->PlayerCameraManager)return;
    const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
    const float Outside=FMath::Clamp(FMath::Min(UHouseDuskAtmosphereComponent::DistanceOutsideHouse(Eye),HouseCampusInteriors::DistanceOutside(Eye))/150.f,0.f,1.f);
    const float Alpha=float(FMath::Max(0.,S.LastAlpha));auto& P=Film->Settings;
    if(Outside>0.f&&Alpha>0.f&&S.FillCube.IsValid())
    {P.AmbientCubemap=S.FillCube.Get();P.bOverride_AmbientCubemapTint=true;P.AmbientCubemapTint=FLinearColor(.42f,.55f,.78f);P.bOverride_AmbientCubemapIntensity=true;P.AmbientCubemapIntensity=S.FillStrength*Alpha*Alpha*Outside;}
    else
    {P.AmbientCubemap=S.OriginalCube.Get();P.bOverride_AmbientCubemapTint=S.OriginalTintOverride;P.AmbientCubemapTint=S.OriginalTint;P.bOverride_AmbientCubemapIntensity=S.OriginalIntensityOverride;P.AmbientCubemapIntensity=S.OriginalIntensity;}
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!W||!W->IsGameWorld()||!W->HasBegunPlay()||W->GetNetMode()==NM_DedicatedServer||!HouseCalderFeatures::Active()||!HouseCalderLandscape::Enabled()||FParse::Param(FCommandLine::Get(),TEXT("NoDusk"))||FParse::Param(FCommandLine::Get(),TEXT("NoCalderNightfall"))||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    auto& S=States.FindOrAdd(W);const double Now=W->GetTimeSeconds();if(!S.Ready&&!Initialize(W,S))return;
    if(Now<S.Next){Fill(W,S);return;}S.Next=Now+1.;bool Pinned=false;const double Elapsed=Seconds(W,Pinned);
    const double U=FMath::Clamp(Elapsed/Duration,0.,1.),Alpha=U*U*(3.-2.*U);
    if(Alpha!=S.LastAlpha)
    {
        FRotator Rotation=S.SunRotation;Rotation.Pitch=FMath::Lerp(S.SunRotation.Pitch,8.5,Alpha);S.Sun->SetActorRotation(Rotation);
        S.Atmosphere->SetSkyLuminanceFactor(FMath::Lerp(S.SkyTint,FLinearColor(.12f,.23f,.50f),float(Alpha)));
        S.Sky->SetIntensity(S.SkyIntensity*FMath::Lerp(1.f,.85f,float(Alpha)));
        for(const auto& F:S.Fog)if(auto* C=F.Component.Get())
        {C->FogInscatteringLuminance=FMath::Lerp(F.Color,F.Color*FLinearColor(.28f,.34f,.52f),float(Alpha));C->DirectionalInscatteringLuminance=F.Glow*(1.f-float(Alpha));C->MarkRenderStateDirty();}
        S.LastAlpha=Alpha;
    }
    Fill(W,S);
    if(Now>=S.NextLog)
    {
        S.NextLog=Now+10.;bool FogUnchanged=true;for(const auto& F:S.Fog)if(auto* C=F.Component.Get())FogUnchanged&=C->FogDensity==F.Density&&C->FogMaxOpacity==F.Opacity;
        UE_LOG(LogTemp,Display,TEXT("CALDER_NIGHTFALL seconds=%.6f alpha=%.8f duration=1800 pinned=%d nativeWall=%.9f sunPitch=%.5f skyIntensity=%.5f fogDensityOpacityUnchanged=%d netMode=%d"),Elapsed,Alpha,int32(Pinned),FPlatformTime::Seconds(),S.Sun->GetActorRotation().Pitch,S.Sky->Intensity,int32(FogUnchanged),int32(W->GetNetMode()));
    }
}
void Cleanup(UWorld* W,bool,bool){States.Remove(W);}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);FWorldDelegates::OnWorldCleanup.AddStatic(&Cleanup);}}Registration;
}
