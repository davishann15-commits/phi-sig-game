#include "HouseStadiumDetail.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AudioMixerDevice.h"
#include "Sound/SoundSubmix.h"
#include "EngineUtils.h"
#include "HouseBlackHall.h"
#include "HouseCampusInteriors.h"
#include "HouseDuskAtmosphere.h"
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
void Dump(const TArray<int16>& Samples,const TCHAR* Name)
{
 const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("StadiumSoundReview"));IFileManager::Get().MakeDirectory(*Folder,true);const uint32 Size=Samples.Num()*sizeof(int16);TArray<uint8> Bytes;Bytes.SetNumZeroed(44+Size);
 auto U32=[&](int32 At,uint32 Value){for(int32 I=0;I<4;++I)Bytes[At+I]=uint8(Value>>(I*8));};auto U16=[&](int32 At,uint16 Value){Bytes[At]=uint8(Value);Bytes[At+1]=uint8(Value>>8);};
 FMemory::Memcpy(Bytes.GetData(),"RIFF",4);U32(4,36+Size);FMemory::Memcpy(Bytes.GetData()+8,"WAVEfmt ",8);U32(16,16);U16(20,1);U16(22,1);U32(24,Rate);U32(28,Rate*2);U16(32,2);U16(34,16);FMemory::Memcpy(Bytes.GetData()+36,"data",4);U32(40,Size);FMemory::Memcpy(Bytes.GetData()+44,Samples.GetData(),Size);FFileHelper::SaveArrayToFile(Bytes,*(Folder/(FString(Name)+TEXT(".wav"))));
}

}
USoundSubmix* AHouseStadiumDetail::RegisteredSoundReviewMix(int32 Voice)const
{
 auto* Device=FAudioDeviceManager::GetAudioMixerDeviceFromWorldContext(this);
 return Device&&SoundReviewSubmixes.IsValidIndex(Voice)&&Device->GetSubmixInstance(SoundReviewSubmixes[Voice].Get()).IsValid()?SoundReviewSubmixes[Voice].Get():nullptr;
}
USoundSubmix* HouseStadiumDetail::SoundReviewMix(UWorld* World,int32 Voice)
{
#if !UE_BUILD_SHIPPING
 if(World&&Enabled()&&Step()>=7&&FParse::Param(FCommandLine::Get(),TEXT("StadiumSoundReview"))){TActorIterator<AHouseStadiumDetail> It(World);if(It)return It->RegisteredSoundReviewMix(Voice);}
#endif
 return nullptr;
}
USoundWaveProcedural* AHouseStadiumDetail::FreshMoodWave(int32 Voice)
{
 auto* Wave=NewObject<USoundWaveProcedural>(this,FName(*FString::Printf(TEXT("CalderStadiumWave%d_%d"),Voice,SoundGeneration++)),RF_Transient);
 Wave->SetSampleRate(HouseStadiumDetailMoodImpl::Rate);Wave->NumChannels=1;Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->bLooping=false;Wave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;
 if(SoundReviewSubmixes.IsValidIndex(Voice))Wave->SoundSubmixObject=SoundReviewSubmixes[Voice].Get();
 Waves[Voice]=Wave;const auto& Clip=Pcm[Voice==2?1:0];Wave->QueueAudio(reinterpret_cast<const uint8*>(Clip.GetData()),Clip.Num()*sizeof(int16));QueuedSoundBytes[Voice]=Clip.Num()*sizeof(int16);return Wave;
}
void AHouseStadiumDetail::Mood()
{
 SetActorTickInterval(1.f/30.f);
 for(FName Key:{FName(TEXT("SD_FlickerBulb")),FName(TEXT("SD_FloodFlicker"))})if(auto* M=Cast<UMaterialInstanceDynamic>(Look->Mat(Key)))Flickers.Add(M);
 Programs=NewObject<UInstancedStaticMeshComponent>(this,TEXT("CalderStadiumDriftingPrograms"));AddInstanceComponent(Programs);Programs->SetupAttachment(RootComponent);Programs->SetMobility(EComponentMobility::Movable);Programs->SetStaticMesh(BoxKit);Programs->SetMaterial(0,Look->Mat(TEXT("CF_Program")));Programs->SetCastShadow(false);Programs->bAffectDistanceFieldLighting=false;Programs->bAffectDynamicIndirectLighting=false;Programs->SetCollisionEnabled(ECollisionEnabled::NoCollision);Programs->SetGenerateOverlapEvents(false);Programs->SetCanEverAffectNavigation(false);Programs->SetCullDistances(6000,8000);Programs->RegisterComponent();
 for(int32 I=0;I<6;++I)
 {
  const float Deg=220.f+I*4.f,Segment=FMath::FloorToFloat(Deg/9.f),T=Deg/9.f-Segment,R=.707f+(I+2)*.024f,Z=180.f+(I+2)*63.f+.20f;
  auto P=[&](float Angle){const float A=FMath::DegreesToRadians(Angle);return FVector(2400+FMath::Cos(A)*5600*R,-13300+FMath::Sin(A)*3800*R,Z);};
  const FVector Left=P(Segment*9.f),Right=P((Segment+1)*9.f),Along=(Right-Left).GetSafeNormal();const FVector Origin=FMath::Lerp(Left,Right,T);
  ProgramOrigins.Add(Origin);ProgramAlong.Add(Along);Programs->AddInstance(FTransform(Along.Rotation(),Origin,{.20,.14,.002}),false);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("nosound"))||GetNetMode()==NM_DedicatedServer)return;
#if !UE_BUILD_SHIPPING
 if(FParse::Param(FCommandLine::Get(),TEXT("StadiumSoundReview")))
  for(int32 I=0;I<4;++I){auto* Mix=NewObject<USoundSubmix>(this,FName(*FString::Printf(TEXT("CalderStadiumDiagnosticMix%d"),I)),RF_Transient);SoundReviewSubmixes.Add(Mix);if(auto* Device=GetWorld()->GetAudioDeviceRaw())Device->RegisterSoundSubmix(Mix,true);}
#endif
 Pcm.Add(HouseStadiumDetailMoodImpl::Render(false));Pcm.Add(HouseStadiumDetailMoodImpl::Render(true));
#if !UE_BUILD_SHIPPING
 if(FParse::Param(FCommandLine::Get(),TEXT("StadiumSoundReview"))){HouseStadiumDetailMoodImpl::Dump(Pcm[0],TEXT("HumSource"));HouseStadiumDetailMoodImpl::Dump(Pcm[1],TEXT("HalyardSource"));}
#endif
 Waves.SetNum(3);QueuedSoundBytes.SetNumZeroed(3);
 for(int32 I=0;I<3;++I)
 {
  auto* Audio=NewObject<UAudioComponent>(this,FName(*FString::Printf(TEXT("CalderStadiumSound%d"),I)));AddInstanceComponent(Audio);Audio->SetupAttachment(RootComponent);Audio->bAutoActivate=false;Audio->bAutoDestroy=false;Audio->bAllowSpatialization=true;Audio->bOverrideAttenuation=true;Audio->AttenuationOverrides.bAttenuate=true;Audio->AttenuationOverrides.bSpatialize=true;Audio->AttenuationOverrides.AttenuationShapeExtents={I==0?600.f:160.f,0,0};Audio->AttenuationOverrides.FalloffDistance=I==0?6500.f:4200.f;Audio->AttenuationOverrides.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;Audio->AttenuationOverrides.dBAttenuationAtMax=-45.f;Audio->SetVolumeMultiplier(I==0?.12f:I==1?.035f:.22f);Audio->SetWorldLocation(I==0?FVector(2400,-9348,3840):I==1?FVector(2550,-9920,2280):FVector(3700,-9730,2710));Audio->RegisterComponent();Sounds.Add(Audio);
 }
 UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_MOOD sources=3 floodHum=1 paCarrier=1 halyard=1 driftingPrograms=6 newLights=0 paperHz=30 paperChordBounded=1 freshWaveOnResume=1"));
}
void AHouseStadiumDetail::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);const double Now=GetWorld()->GetTimeSeconds();auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||!PC->PlayerCameraManager)return;const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();const double D2=FVector::DistSquared2D(Eye,{2400,-13300,0});
 if(D2<FMath::Square(18000.))
 {
  if(Now>=NextMood){NextMood=Now+.1;const bool Dim=FMath::Fmod(Now,23.)>22.65;for(int32 I=0;I<Flickers.Num();++I)Flickers[I]->SetVectorParameterValue(TEXT("Emission"),I==0?FLinearColor(14,7,2)*(Dim?.45f:1.f):FLinearColor(22,24,28)*(Dim?.89f:1.f));}
  if(Programs&&D2<FMath::Square(8000.))for(int32 I=0;I<ProgramOrigins.Num();++I)
  {
   const float P=FMath::Fmod(float(Now)*.12f+I*.17f,1.f),Envelope=FMath::Sin(P*PI),Rock=FMath::Sin(P*2*PI);const FVector At=ProgramOrigins[I]+ProgramAlong[I]*(Rock*50.f)+FVector(0,0,Envelope*3.5f);
   Programs->UpdateInstanceTransform(I,FTransform(FRotator(FMath::Sin(P*4*PI)*2.f*Envelope,ProgramAlong[I].Rotation().Yaw+Rock*15.f,Rock*4.f*Envelope),At,{.20,.14,.002}),false,I==ProgramOrigins.Num()-1,true);
  }
 }
 const bool Inside=FMath::Min(HouseCampusInteriors::DistanceOutside(Eye),UHouseDuskAtmosphereComponent::DistanceOutsideHouse(Eye))<=1.f;
 for(int32 I=0;I<Sounds.Num();++I)
 {
  const auto& Attenuation=Sounds[I]->AttenuationOverrides;const bool Far=FVector::Dist(Eye,Sounds[I]->GetComponentLocation())>Attenuation.AttenuationShapeExtents.X+Attenuation.FalloffDistance;
  if(Inside||Far){if(Sounds[I]->IsPlaying())Sounds[I]->Stop();continue;}
  if(I==2)
  {
   const int64 Cycle=FMath::FloorToInt64((Now+7.)/37.);const bool Active=FMath::Fmod(Now+7.,37.)<1.;if(!Active){if(Sounds[I]->IsPlaying())Sounds[I]->Stop();continue;}if(LastHalyardCycle==Cycle)continue;LastHalyardCycle=Cycle;Sounds[I]->SetSound(FreshMoodWave(I));Sounds[I]->Play();continue;
  }
  if(!Sounds[I]->IsPlaying()){Sounds[I]->SetSound(FreshMoodWave(I));Sounds[I]->Play();}
  else if(Waves[I]->GetAvailableAudioByteCount()<HouseStadiumDetailMoodImpl::Rate/2){Waves[I]->QueueAudio(reinterpret_cast<const uint8*>(Pcm[0].GetData()),Pcm[0].Num()*sizeof(int16));QueuedSoundBytes[I]+=Pcm[0].Num()*sizeof(int16);}
 }
 // The optional review will be appended here after stage6 acceptance.

}
void AHouseStadiumDetail::EndPlay(const EEndPlayReason::Type Reason)
{for(const auto& C:Sounds)if(C)C->Stop();if(auto* Device=GetWorld()?GetWorld()->GetAudioDeviceRaw():nullptr)for(const auto& Mix:SoundReviewSubmixes)if(Mix)Device->UnregisterSoundSubmix(Mix.Get(),false);Super::EndPlay(Reason);}
