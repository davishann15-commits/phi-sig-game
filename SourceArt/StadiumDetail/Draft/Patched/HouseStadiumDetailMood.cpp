#include "HouseStadiumDetail.h"
#include "HouseBlackHall.h"
#include "HouseCampusInteriors.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace HouseStadiumDetailMoodImpl
{
constexpr int32 Rate=24000;
TArray<int16> Render(bool Clank)
{const int32 Count=Clank?Rate:Rate*4;TArray<int16> Out;Out.SetNum(Count);FRandomStream R(193186);for(int32 I=0;I<Count;++I){const double T=double(I)/Rate;double V;if(Clank){const double Envelope=FMath::Exp(-T*9.);V=Envelope*(FMath::Sin(T*2*PI*1730)+.55*FMath::Sin(T*2*PI*2417)+R.FRandRange(-.18f,.18f))*.15;}else V=(FMath::Sin(T*2*PI*60)*.20+FMath::Sin(T*2*PI*120)*.11+FMath::Sin(T*2*PI*360)*.025)*(.96+.04*FMath::Sin(T*2*PI*.25));Out[I]=int16(FMath::Clamp(V,-.9,.9)*32767.);}return Out;}
}
void AHouseStadiumDetail::Mood()
{
 for(FName Key:{FName(TEXT("SD_FlickerBulb")),FName(TEXT("SD_FloodFlicker"))})if(auto* M=Cast<UMaterialInstanceDynamic>(Look->Mat(Key)))Flickers.Add(M);
 Programs=NewObject<UInstancedStaticMeshComponent>(this,TEXT("CalderStadiumDriftingPrograms"));AddInstanceComponent(Programs);Programs->SetupAttachment(RootComponent);Programs->SetMobility(EComponentMobility::Movable);Programs->SetStaticMesh(BoxKit);Programs->SetMaterial(0,Look->Mat(TEXT("CF_Program")));Programs->SetCastShadow(false);Programs->bAffectDistanceFieldLighting=false;Programs->bAffectDynamicIndirectLighting=false;Programs->SetCollisionEnabled(ECollisionEnabled::NoCollision);Programs->SetGenerateOverlapEvents(false);Programs->SetCanEverAffectNavigation(false);Programs->SetCullDistances(6000,8000);Programs->RegisterComponent();
 for(int32 I=0;I<6;++I){const float A=(220+I*4)*PI/180;const int32 Row=I+2;const FVector P(2400+FMath::Cos(A)*5600*(.69+Row*.024),-13300+FMath::Sin(A)*3800*(.69+Row*.024),180+Row*63.f+2);ProgramOrigins.Add(P);Programs->AddInstance(FTransform(FRotator(0,I*41,0),P,{.20,.14,.002}),false);}
 if(FParse::Param(FCommandLine::Get(),TEXT("nosound"))||GetNetMode()==NM_DedicatedServer)return;
 Pcm.Add(HouseStadiumDetailMoodImpl::Render(false));Pcm.Add(HouseStadiumDetailMoodImpl::Render(true));
 for(int32 I=0;I<3;++I)
 {
  auto* Wave=NewObject<USoundWaveProcedural>(this,FName(*FString::Printf(TEXT("CalderStadiumWave%d"),I)));Wave->SetSampleRate(HouseStadiumDetailMoodImpl::Rate);Wave->NumChannels=1;Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->bLooping=false;Waves.Add(Wave);
  auto* Audio=NewObject<UAudioComponent>(this,FName(*FString::Printf(TEXT("CalderStadiumSound%d"),I)));AddInstanceComponent(Audio);Audio->SetupAttachment(RootComponent);Audio->bAutoActivate=false;Audio->bAutoDestroy=false;Audio->bAllowSpatialization=true;Audio->bOverrideAttenuation=true;Audio->AttenuationOverrides.bAttenuate=true;Audio->AttenuationOverrides.bSpatialize=true;Audio->AttenuationOverrides.AttenuationShapeExtents={I==0?600.f:160.f,0,0};Audio->AttenuationOverrides.FalloffDistance=I==0?6500.f:2200.f;Audio->AttenuationOverrides.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;Audio->AttenuationOverrides.dBAttenuationAtMax=-45.f;Audio->SetSound(Wave);Audio->SetVolumeMultiplier(I==0?.12f:I==1?.035f:.10f);Audio->SetWorldLocation(I==0?FVector(2400,-9348,3840):I==1?FVector(2550,-9920,2280):FVector(3700,-9730,2710));Audio->RegisterComponent();Sounds.Add(Audio);
 }
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_MOOD sources=3 floodHum=1 paCarrier=1 halyard=1 driftingPrograms=6 newLights=0"));
}
void AHouseStadiumDetail::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);const double Now=GetWorld()->GetTimeSeconds();auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||!PC->PlayerCameraManager)return;const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();const double D2=FVector::DistSquared2D(Eye,{2400,-13300,0});if(D2>FMath::Square(18000.))return;
 if(Now>=NextMood)
 {
  NextMood=Now+.1;const bool Dim=FMath::Fmod(Now,23.)>22.65;for(int32 I=0;I<Flickers.Num();++I)Flickers[I]->SetVectorParameterValue(TEXT("Emission"),I==0?FLinearColor(14,7,2)*(Dim?.45f:1.f):FLinearColor(22,24,28)*(Dim?.89f:1.f));
  if(Programs&&D2<FMath::Square(8000.))for(int32 I=0;I<ProgramOrigins.Num();++I){const float P=FMath::Fmod(float(Now)*.12f+I*.17f,1.f),Lift=FMath::Sin(P*PI)*15.f;const FVector At=ProgramOrigins[I]+FVector(FMath::Sin(P*2*PI)*70,0,Lift);Programs->UpdateInstanceTransform(I,FTransform(FRotator(FMath::Sin(P*2*PI)*10,P*360+I*41,0),At,{.20,.14,.002}),false,I==ProgramOrigins.Num()-1,true);}
 }
 const bool Inside=HouseCampusInteriors::DistanceOutside(Eye)<1.f;
 for(int32 I=0;I<Sounds.Num();++I)
 {
  if(Inside){if(Sounds[I]->IsPlaying())Sounds[I]->Stop();Waves[I]->ResetAudio();continue;}
  if(I==2){const int64 Cycle=FMath::FloorToInt64((Now+7.)/37.);const bool Active=FMath::Fmod(Now+7.,37.)<1.;if(!Active){if(Sounds[I]->IsPlaying())Sounds[I]->Stop();Waves[I]->ResetAudio();continue;}if(LastHalyardCycle==Cycle)continue;LastHalyardCycle=Cycle;Waves[I]->ResetAudio();Waves[I]->QueueAudio(reinterpret_cast<const uint8*>(Pcm[1].GetData()),Pcm[1].Num()*sizeof(int16));Sounds[I]->Play();continue;}
  if(Waves[I]->GetAvailableAudioByteCount()<HouseStadiumDetailMoodImpl::Rate/2)Waves[I]->QueueAudio(reinterpret_cast<const uint8*>(Pcm[0].GetData()),Pcm[0].Num()*sizeof(int16));if(!Sounds[I]->IsPlaying())Sounds[I]->Play();
 }
}
void AHouseStadiumDetail::EndPlay(const EEndPlayReason::Type Reason)
{for(const auto& C:Sounds)if(C)C->Stop();Super::EndPlay(Reason);}
