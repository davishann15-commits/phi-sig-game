#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderWildlifeBirdCalls.h"
#include "HouseCalderWildlifePondWater.h"
#include "HouseCalderLandform.h"
#include "HouseCalderTerrain.h"
#include "HouseCampusInteriors.h"
#include "HouseDuskAtmosphere.h"
#include "Async/Async.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioMixerDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWaveProcedural.h"

namespace HouseCalderWildlifeSoundImpl
{
void Wav(const TArray<int16>& Pcm,const FString& Name)
{
    TArray<uint8> Bytes;auto U32=[&Bytes](uint32 V){for(int32 B=0;B<4;++B)Bytes.Add(uint8(V>>(B*8)));};
    auto U16=[&Bytes](uint16 V){Bytes.Add(uint8(V));Bytes.Add(uint8(V>>8));};
    const uint32 Size=Pcm.Num()*sizeof(int16),Rate=HouseCalderWildlifeSoundPcm::Rate;
    Bytes.Append(reinterpret_cast<const uint8*>("RIFF"),4);U32(Size+36);Bytes.Append(reinterpret_cast<const uint8*>("WAVEfmt "),8);
    U32(16);U16(1);U16(1);U32(Rate);U32(Rate*2);U16(2);U16(16);Bytes.Append(reinterpret_cast<const uint8*>("data"),4);U32(Size);
    Bytes.Append(reinterpret_cast<const uint8*>(Pcm.GetData()),Size);
    const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSound"));IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveArrayToFile(Bytes,*(Dir/(Name+TEXT(".wav"))));
}
FVector NearestCreek(const FVector& Eye,double& Distance)
{
    FVector Best=FVector::ZeroVector;Distance=TNumericLimits<double>::Max();
    for(const auto* Points:{&HouseCalderLandform::CreekPoints(),&HouseCalderLandform::GlenPoints()})
        for(int32 I=1;I<Points->Num();++I)
        {
            const auto& A=(*Points)[I-1];const auto& B=(*Points)[I];const FVector2D Delta=B.At-A.At;
            const double T=FMath::Clamp(FVector2D::DotProduct(FVector2D(Eye)-A.At,Delta)/FMath::Max(1.,Delta.SizeSquared()),0.,1.);
            const FVector At(FMath::Lerp(A.At,B.At,T),FMath::Lerp(A.WaterZ,B.WaterZ,T)+15.);
            const double D=FVector::DistSquared(At,Eye);if(D<Distance){Distance=D;Best=At;}
        }
    Distance=FMath::Sqrt(Distance);return Best;
}
}
bool HouseCalderWildlife::SoundEnabled()
{return Enabled() && !FParse::Param(FCommandLine::Get(),TEXT("NoCalderWildlifeSound")) && GEngine && GEngine->UseSound();}
bool AHouseCalderWildlifeDirector::WildlifeSoundSubmixRegistered()const
{
    auto* Device=FAudioDeviceManager::GetAudioMixerDeviceFromWorldContext(this);
    return Device && SoundSubmix && Device->GetSubmixInstance(SoundSubmix.Get()).IsValid();
}
void AHouseCalderWildlifeDirector::StopSound()
{
    if(Runtime && Runtime->Sound.Recording && WildlifeSoundSubmixRegistered())
    {
        const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSound"));IFileManager::Get().MakeDirectory(*Dir,true);
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("WildlifeMix_partial"),Dir,SoundSubmix.Get(),nullptr);Runtime->Sound.Recording=false;
    }
    for(UAudioComponent* Voice:SoundVoices)if(Voice)Voice->Stop();
    if(SoundSubmix)if(auto* Device=GetWorld()?GetWorld()->GetAudioDeviceRaw():nullptr)Device->UnregisterSoundSubmix(SoundSubmix.Get(),false);
}
void AHouseCalderWildlifeDirector::TickSound(double Now,float)
{
    if(!HouseCalderWildlife::SoundEnabled() || !Runtime || GetNetMode()==NM_DedicatedServer)return;
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC || !PC->IsLocalController() || !PC->PlayerCameraManager)return;
    auto& Audio=Runtime->Sound;const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
    const FVector Forward=PC->PlayerCameraManager->GetCameraRotation().Vector();
    if(!Audio.Ready)
    {
        if(!Audio.Task.IsValid()){const bool Birds=HouseCalderWildlifeBirdCalls::Enabled();Audio.Task=Async(EAsyncExecution::ThreadPool,[Birds]{return HouseCalderWildlifeSoundPcm::Build(Birds);});}
        if(!Audio.Task.IsReady())return;Audio.Pcm=Audio.Task.Consume();
        for(const auto& Samples:Audio.Pcm)
        {double Energy=0.;for(const int16 Sample:Samples)Energy+=FMath::Square(double(Sample)/32768.);Audio.Rms.Add(FMath::Sqrt(Energy/FMath::Max(1,Samples.Num())));}
        SoundSubmix=NewObject<USoundSubmix>(this,TEXT("CalderWildlifeMix"),RF_Transient);
        // NewObject does not run USoundSubmix::PostLoad registration. Without
        // this, recording silently falls back to the unrelated master mix.
        if(auto* Device=GetWorld()->GetAudioDeviceRaw())Device->RegisterSoundSubmix(SoundSubmix.Get(),true);
        SoundFalloff=NewObject<USoundAttenuation>(this,NAME_None,RF_Transient);auto& A=SoundFalloff->Attenuation;
        A.bAttenuate=true;A.bSpatialize=true;A.AttenuationShape=EAttenuationShape::Sphere;A.AttenuationShapeExtents=FVector(300,0,0);A.FalloffDistance=9000.;A.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;
        A.bAttenuateWithLPF=true;A.LPFRadiusMin=1000.;A.LPFRadiusMax=9000.;A.LPFFrequencyAtMin=18000.;A.LPFFrequencyAtMax=1800.;
        A.bEnableOcclusion=true;A.OcclusionTraceChannel=ECC_Visibility;A.OcclusionLowPassFilterFrequency=850.;A.OcclusionVolumeAttenuation=.16;A.OcclusionInterpolationTime=.25;
        if(HouseCalderWildlifeBirdCalls::Enabled())
        {
            BirdSoundFalloff=DuplicateObject<USoundAttenuation>(SoundFalloff.Get(),this);
            auto& BirdFalloff=BirdSoundFalloff->Attenuation;
            BirdFalloff.FalloffDistance=39700.;BirdFalloff.LPFRadiusMax=40000.;
            // A flock's carrying call needs a gentler distance fade than a
            // nearby leaf crunch. Keep the ground-animal curve unchanged.
            BirdFalloff.dBAttenuationAtMax=-35.f;
            // The enclosing ridges lie beyond the400m flight-render radius.
            // A sound-only raven needs its own distant curve, without making
            // the approved goose/crow falloff louder or extending their range.
            RavenSoundFalloff=DuplicateObject<USoundAttenuation>(BirdSoundFalloff.Get(),this);
            auto& RavenFalloff=RavenSoundFalloff->Attenuation;
            RavenFalloff.FalloffDistance=99700.;RavenFalloff.LPFRadiusMax=100000.;
            RavenFalloff.dBAttenuationAtMax=-35.f;
        }
        for(int32 I=0;I<6;++I)
        {
            auto* C=NewObject<UAudioComponent>(this,FName(*FString::Printf(TEXT("CalderWildlifeAudio%d"),I)),RF_Transient);
            C->SetupAttachment(RootComponent);C->bAutoActivate=false;C->bAutoDestroy=false;C->SetVolumeMultiplier(0.f);C->bAllowSpatialization=I!=4;if(I!=4)C->AttenuationSettings=SoundFalloff.Get();C->RegisterComponent();
            auto* Wave=NewObject<USoundWaveProcedural>(this,NAME_None,RF_Transient);Wave->SetSampleRate(HouseCalderWildlifeSoundPcm::Rate);Wave->NumChannels=1;Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->bLooping=false;Wave->SoundGroup=SOUNDGROUP_Default;Wave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;Wave->SoundSubmixObject=SoundSubmix.Get();C->SetSound(Wave);
            SoundVoices.Add(C);SoundWaves.Add(Wave);
        }
        Audio.Ready=true;Audio.Started=Now;for(double& Birth:Audio.LastShoreSplash)Birth=-1.e30;
        Audio.Voices[4].Kind=2;Audio.Voices[4].Gain=.04f;Audio.Voices[5].Kind=3;Audio.Voices[5].Gain=.18f;
        const double Scene=HouseCalderWildlife::SceneTime(GetWorld());for(int32 I=0;I<2;++I)Audio.Cursors[I]=int64(Scene*HouseCalderWildlifeSoundPcm::Rate)%Audio.Pcm[I+2].Num();
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOUND_READY pooledOneShots=4 loops=2 voices=6 bankWorkerOnce=1 recordedCorvidBuffers=%d sharedAttenuation=1 PlayWhenSilent=1 indoorBothDistances=1 originalWoodsOwlUnchanged=1 nightSnowyChirpsPerMinute=60"),HouseCalderWildlifeBirdCalls::Enabled()?2:0);
    }
    if(!Audio.SubmixVerified && WildlifeSoundSubmixRegistered())
    {Audio.SubmixVerified=true;UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOUND_SUBMIX_REGISTERED id=%u noMasterRecordingFallback=1"),SoundSubmix->GetUniqueID());}
    if(!Audio.Dumped && FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSoundDump")))
    {
        const TCHAR* Names[]={TEXT("DeerBlow"),TEXT("LeafCrunch"),TEXT("NightBed"),TEXT("Creek"),TEXT("LonePeeper"),TEXT("GooseHiss"),TEXT("GooseSplash"),TEXT("GooseHonk"),TEXT("GooseHonkEcho2"),TEXT("GooseHonkEcho3"),TEXT("GooseHonkEcho4"),TEXT("GooseHonkEcho5"),TEXT("CrowCaw"),TEXT("RavenCroak")};
        for(int32 I=0;I<Audio.Pcm.Num();++I)HouseCalderWildlifeSoundImpl::Wav(Audio.Pcm[I],Names[I]);Audio.Dumped=true;
    }
    const double Outside=FMath::Min(double(HouseCampusInteriors::DistanceOutside(Eye)),double(UHouseDuskAtmosphereComponent::DistanceOutsideHouse(Eye)));
    const float Indoor=Outside<=1.?0.f:float(FMath::SmoothStep(0.,150.,Outside));
    bool ProtectedLawn=false;for(const auto& Rect:HouseCalderLandform::FrozenRects())ProtectedLawn|=Rect.IsInside(FVector2D(Eye));
    double CreekDistance;const FVector Creek=HouseCalderWildlifeSoundImpl::NearestCreek(Eye,CreekDistance);
    Audio.Voices[4].At=Eye;Audio.Voices[5].At=Creek;
    auto Play=[&](int32 Kind,int32 Species,const FVector& At,float Gain,double Deadline,bool Overhead=false,bool Urgent=false,double Offset=0.) -> int32
    {
        const bool RavenCall=Kind==HouseCalderWildlifeBirdCalls::Raven;
        if(!Audio.Pcm.IsValidIndex(Kind)||Now>Deadline || FVector::Distance(Eye,At)>(RavenCall?100000.:Overhead?40000.:9300.)){++Audio.MissedDeadlines;return -1;}
        for(int32 I=0;I<4;++I)if(Audio.Voices[I].Until>Now && Audio.Voices[I].Species==Species)
        {if(Urgent&&Audio.Voices[I].Overhead){SoundVoices[I]->Stop();SoundWaves[I]->ResetAudio();Audio.Voices[I].Until=0.;}else return -1;}
        for(int32 I=0;I<4;++I)if(Audio.Voices[I].Until<=Now)
        {
            const int32 Skip=FMath::Clamp(FMath::FloorToInt(FMath::Max(0.,Offset)*HouseCalderWildlifeSoundPcm::Rate),0,Audio.Pcm[Kind].Num());if(Skip>=Audio.Pcm[Kind].Num())return -1;
            auto& V=Audio.Voices[I];V.Kind=Kind;V.Species=Species;V.At=At;V.Gain=Gain;V.Overhead=Overhead;V.Started=Now-Offset;V.FollowEvent=V.FollowMember=-1;V.Until=Now+double(Audio.Pcm[Kind].Num()-Skip)/HouseCalderWildlifeSoundPcm::Rate+.15;V.Applied=-1.;
            auto* C=SoundVoices[I].Get();auto* Wave=SoundWaves[I].Get();C->Stop();C->AttenuationSettings=RavenCall?RavenSoundFalloff.Get():Overhead?BirdSoundFalloff.Get():SoundFalloff.Get();Wave->ResetAudio();Wave->QueueAudio(reinterpret_cast<const uint8*>(Audio.Pcm[Kind].GetData()+Skip),(Audio.Pcm[Kind].Num()-Skip)*sizeof(int16));C->SetWorldLocation(At);C->SetVolumeMultiplier(0.f);C->Play();++Audio.Calls;
            Audio.GooseHiss+=Kind==HouseCalderWildlifeBirdCalls::Hiss;Audio.GooseSplash+=Kind==HouseCalderWildlifeBirdCalls::Splash;Audio.GooseHonk+=Kind>=HouseCalderWildlifeBirdCalls::Honk&&Kind<HouseCalderWildlifeBirdCalls::Caw;Audio.GooseEcho+=Kind>=HouseCalderWildlifeBirdCalls::FirstEcho&&Kind<HouseCalderWildlifeBirdCalls::Caw;
            UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_CALL kind=%d species=%d pool=%d at=%s age=%.3f deadline=%.3f overhead=%d offsetSeconds=%.3f nativeWall=%.9f authorityTime=%.6f"),Kind,Species,I,*At.ToString(),Now-(Deadline-.8),Deadline,int32(Overhead),Offset,FPlatformTime::Seconds(),Now);return I;
        }
        ++Audio.MissedDeadlines;return -1;
    };
    for(const auto& State:States.Items)
    {
        const float* Seen=Audio.SeenStarts.Find(State.Id);
        if(!Seen || *Seen!=State.StartTime)
        {
            Audio.SeenStarts.Add(State.Id,State.StartTime);
            if(State.Mood==ECalderWildlifeMood::Alarm && (State.Cause==1 || State.Cause==2))
                Play(0,State.Species,AnimalPosition(State.Id)+FVector(0,0,80),.7f,State.StartTime+.8);
        }
    }
    if(HouseCalderWildlifeBirdCalls::Enabled()&&Runtime->Pond.Ready&&Runtime->Sky.Ready)
    {
        using namespace HouseCalderWildlifeBirdCalls;const double Raw=HouseCalderWildlife::CycleTime(GetWorld()),Scene=HouseCalderWildlife::SceneTime(GetWorld());
        for(const auto& State:PondStates.Items)
        {
            const float* Seen=Audio.PondSeenStarts.Find(State.Id);
            if(!Seen||*Seen!=State.StartTime)
            {
                Audio.PondSeenStarts.Add(State.Id,State.StartTime);HouseCalderWildlifeGoosePaths::FCycleSample Bird;
                if(Runtime->Pond.Sample(Raw,State.Id/8,State.Id%8,Bird))
                {
                    const double Age=FMath::Max(0.,Raw-State.StartTime-HouseCalderWildlife::CycleReviewOffset());
                    if(State.Cause==64)Play(Hiss,1,Bird.Bird.Position+FVector(0,0,50.*Bird.Bird.Scale),.35f,Now+.8-Age,false,true,Age);
                    else if(State.Cause==3)Play(Splash,201,FVector(Bird.Bird.Position.X,Bird.Bird.Position.Y,Bird.WaterSurface+3.),.28f,Now+.8-Age,false,false,Age);
                }
            }
        }
        for(int32 Id:{6,7,14,15})
        {
            const double Birth=Runtime->Pond.ShoreSplashBirth(Raw,Id),Age=Raw-Birth;HouseCalderWildlifeGoosePaths::FCycleSample Bird;
            if(Age<0.||Age>=.5||Birth<=Audio.LastShoreSplash[Id]||!Runtime->Pond.Sample(Birth+.001,Id/8,Id%8,Bird))continue;
            const FVector At(Bird.Bird.Position.X,Bird.Bird.Position.Y,Bird.WaterSurface);
            if(!Bird.OnWater||!HouseCalderWildlifePondWater::WetEnvelope(At,15.))continue;
            Audio.LastShoreSplash[Id]=Birth;Play(Splash,201,At+FVector(0,0,3.),.10f,Now+.5-Age,false,false,Age);
        }
        for(int32 Event=0;Event<2;++Event)
        {
            HouseCalderWildlifeSkyPaths::FSample Leader,Caller;int32 Index=0,Member=0;double Age=0.;
            if(!Runtime->Sky.Plan.EvaluateGoose(Scene,Event,0,Leader)||!Cue(Leader.Age,Event,Index,Member,Age)||!Runtime->Sky.Plan.EvaluateGoose(Scene,Event,Member,Caller))continue;
            const int64 Key=FMath::FloorToInt64(Raw/1800.)*10000+Event*100+Index;if(Key==Audio.LastHonk)continue;Audio.LastHonk=Key;
            if(FVector::Distance(Eye,Caller.Position)>40000.)continue;FVector Face=FVector::ZeroVector;int32 Delay=0;const bool Echo=RidgeEcho(Caller.Position,Face,Delay);
            const int32 Voice=Play(Echo?FirstEcho+Delay-2:Honk,1,Caller.Position,.85f,Now+HonkSeconds-Age,true,false,Age);
            if(Voice>=0){Audio.Voices[Voice].FollowEvent=Event;Audio.Voices[Voice].FollowMember=Member;UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_HONK member=%d event=%d age=%.3f echoDelay=%d echoDb=-18 filteredOnce=1 ridge=%s physicsQueries=0 oneGooseCall=1"),Member,Event,Age,Delay,*Face.ToString());}
        }
    }
    if(HouseCalderWildlifeBirdCalls::Enabled()&&Runtime->Sky.Ready&&Runtime->Sky.CrowRoost.Hash)
    {
        using namespace HouseCalderWildlifeBirdCalls;const double Raw=HouseCalderWildlife::CycleTime(GetWorld()),Scene=HouseCalderWildlife::SceneTime(GetWorld());
        for(int32 Event=0;Event<12;++Event)
        {
            HouseCalderWildlifeSkyPaths::FSample Leader,Caller;int32 Index=0,Member=0;double Age=0.;
            if(!Runtime->Sky.CrowRoost.Evaluate(Scene,Event,0,Leader)||!CrowCue(Leader.Age,Event,HouseCalderWildlifeCrowRoost::Count(Event),Index,Member,Age)||!Runtime->Sky.CrowRoost.Evaluate(Scene,Event,Member,Caller))continue;
            const int64 Key=FMath::FloorToInt64(Raw/1800.)*100000+Event*1000+Index;if(Key==Audio.LastCrow)continue;Audio.LastCrow=Key;
            const int32 Voice=Play(Caw,2,Caller.Position,.42f,Now+CawSeconds-Age,true,false,Age);
            if(Voice>=0){Audio.Voices[Voice].FollowEvent=Event;Audio.Voices[Voice].FollowMember=Member;UE_LOG(LogTemp,Display,TEXT("CALDER_CROW_CAW event=%d member=%d index=%d scene=%.6f age=%.6f at=%s sharedClock=1"),Event,Member,Index,Scene,Age,*Caller.Position.ToString());}
        }
        const int32 Event=FMath::FloorToInt((Scene-110.)/300.);const double Age=Scene-110.-Event*300.;
        const int64 Key=FMath::FloorToInt64(Raw/1800.)*10+Event;
        if(Event>=0&&Age>=0.&&Age<RavenSeconds&&Key!=Audio.LastRaven)
        {
            Audio.LastRaven=Key;FVector Face;int32 Delay=0;
            if(RidgeEcho(FVector(0,18000,3000),Face,Delay))
            {const FVector At=Face+FVector(0,0,1200);if(Play(Raven,3,At,.32f,Now+RavenSeconds-Age,false,false,Age)>=0)UE_LOG(LogTemp,Display,TEXT("CALDER_RAVEN_CROAK scene=%.6f ridge=%s distanceCm=%.3f soundOnly=1 lawnCapApplies=1"),Scene,*At.ToString(),FVector::Distance(Eye,At));}
            else UE_LOG(LogTemp,Warning,TEXT("CALDER_RAVEN_NO_RIDGE scene=%.6f"),Scene);
        }
    }
    if(Now>=Audio.NextCrunch)
    {
        Audio.NextCrunch=Now+.9;double Nearest=3000.;int32 Id=-1;
        for(const auto& State:States.Items)if(HouseCalderWildlifePose::Speed(State.Mood)>0.)
        {
            const FVector Delta=AnimalPosition(State.Id)-Eye;const double Distance=Delta.Size();
            const bool Unseen=FVector::DotProduct(Delta.GetSafeNormal(),Forward)<FMath::Cos(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5+10.));
            if(Unseen && Distance<Nearest){Nearest=Distance;Id=State.Id;}
        }
        if(Id>=0)Play(1,States.Items[Id].Species,AnimalPosition(Id)+FVector(0,0,5),.18f,Now+.8);
    }
    const double Scene=HouseCalderWildlife::SceneTime(GetWorld());
    if(Audio.NextPeeperScene<0. || Scene<Audio.LastScene)Audio.NextPeeperScene=83.+200.*FMath::FloorToDouble((Scene-83.)/200.+1.);
    if(Scene>=Audio.NextPeeperScene)
    {
        // Three isolated calls/ten minutes; never a frog chorus.
        const FVector At(-6500,9600,HouseCalderTerrain::SurfaceZ(-6500,9600)+80.);Play(4,200,At,.10f,Now+.8);Audio.NextPeeperScene+=200.;
    }
    Audio.LastScene=Scene;
    double RmsBound=0.;int32 Playing=0;TMap<int32,int32> SpeciesVoices;
    for(int32 I=0;I<6;++I)
    {
        const auto& V=Audio.Voices[I];const bool Active=I<4?Now<V.Until:I==4 || CreekDistance<4500.;
        if(Active){if(!V.Overhead)RmsBound+=V.Gain*Audio.Rms[V.Kind];++Playing;if(I<4)Audio.MaxSpecies=FMath::Max(Audio.MaxSpecies,++SpeciesVoices.FindOrAdd(V.Species));}
    }
    const float Lawn=ProtectedLawn?float(FMath::Min(1.,.008/FMath::Max(.000001,RmsBound))):1.f;
    for(int32 I=0;I<6;++I)
    {
        auto& V=Audio.Voices[I];auto* C=SoundVoices[I].Get();auto* Wave=SoundWaves[I].Get();
        const bool Active=I<4?Now<V.Until:I==4 || CreekDistance<4500.;
        if(!Active){if(C->IsPlaying()){C->Stop();Wave->ResetAudio();}if(V.Applied!=0.f){C->SetVolumeMultiplier(0.f);V.Applied=0.f;}continue;}
        if(V.Overhead&&V.FollowEvent>=0)
        {
            HouseCalderWildlifeSkyPaths::FSample Bird;const double Clock=HouseCalderWildlife::SceneTime(GetWorld());
            if(V.Kind==HouseCalderWildlifeBirdCalls::Caw)
            {if(Now-V.Started<HouseCalderWildlifeBirdCalls::CawSeconds&&Runtime->Sky.CrowRoost.Evaluate(Clock,V.FollowEvent,V.FollowMember,Bird))V.At=Bird.Position;}
            else if(Now-V.Started<HouseCalderWildlifeBirdCalls::HonkSeconds&&Runtime->Sky.Plan.EvaluateGoose(Clock,V.FollowEvent,V.FollowMember,Bird))V.At=Bird.Position;
        }
        C->SetWorldLocation(V.At);
        const float Gain=V.Gain*Indoor*(V.Overhead?1.f:Lawn)*(V.Kind==1 && FVector::Distance(Eye,V.At)>3000.?0.f:1.f);
        if((Gain==0.f && V.Applied!=0.f) || FMath::Abs(Gain-V.Applied)>.0005f){C->SetVolumeMultiplier(Gain);V.Applied=Gain;}
        if(I>=4)
        {
            const auto& Pcm=Audio.Pcm[V.Kind];int32& Cursor=Audio.Cursors[I-4];
            while(Wave->GetAvailableAudioByteCount()<HouseCalderWildlifeSoundPcm::Rate*2)
            {const int32 Count=FMath::Min(HouseCalderWildlifeSoundPcm::Rate/4,Pcm.Num()-Cursor);Wave->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()+Cursor),Count*sizeof(int16));Cursor=(Cursor+Count)%Pcm.Num();}
            if(!C->IsPlaying())C->Play();
        }
    }
    Audio.PeakVoices=FMath::Max(Audio.PeakVoices,Playing);if(auto* Device=GetWorld()->GetAudioDeviceRaw())Audio.PeakSources=FMath::Max(Audio.PeakSources,Device->GetNumActiveSources());
    float RecordSeconds=0.;FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeRecord="),RecordSeconds);
    if(RecordSeconds>0. && !Audio.Recording && !Audio.Recorded && Now-Audio.Started>=(FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioReview"))?0.:10.) && WildlifeSoundSubmixRegistered())
    {UAudioMixerBlueprintLibrary::StartRecordingOutput(this,RecordSeconds+2.f,SoundSubmix.Get());Audio.RecordUntil=Now+RecordSeconds;Audio.Recording=true;UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_AUDIO_RECORD_START nativeWall=%.9f authorityTime=%.6f seconds=%.3f registeredSubmixOnly=1"),FPlatformTime::Seconds(),Now,RecordSeconds);if(FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioReview")))UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_AUDIO_RECORD_START nativeWall=%.9f authorityTime=%.6f seconds=%.3f registeredSubmixOnly=1"),FPlatformTime::Seconds(),Now,RecordSeconds);}
    if(Audio.Recording && Now>=Audio.RecordUntil)
    {
        const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSound"));IFileManager::Get().MakeDirectory(*Dir,true);
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("WildlifeMix"),Dir,SoundSubmix.Get(),nullptr);Audio.Recording=false;Audio.Recorded=true;
        UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOUND_RECORD path=%s seconds=%.1f wildlifeSubmixOnly=1"),*(Dir/TEXT("WildlifeMix.wav")),RecordSeconds);
    }
    if(Now>=Audio.NextStats)
    {
        Audio.NextStats=Now+10.;UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_SOUND voices=%d peakVoices=%d peakActiveSources=%d calls=%d expiredOrDistant=%d insideGain=%.3f lawn=%d conservativeLawnRms=%.6f sourceCap24=%d voiceCap7=%d"),Playing,Audio.PeakVoices,Audio.PeakSources,Audio.Calls,Audio.MissedDeadlines,Indoor,int32(ProtectedLawn),RmsBound*Lawn*Indoor,int32(Audio.PeakSources<=24),int32(Audio.PeakVoices<=7));
    }
}
#if !UE_BUILD_SHIPPING
bool AHouseCalderWildlifeDirector::ReadGooseAudioForReview(FCalderGooseAudioProbe& Out)const
{
    if(!Runtime||!Runtime->Sound.Ready||!WildlifeSoundSubmixRegistered())return false;const auto& A=Runtime->Sound;
    Out.Buffers=A.Pcm.Num();Out.Voices=SoundVoices.Num();Out.PeakVoices=A.PeakVoices;Out.PeakSources=A.PeakSources;Out.MaxSpecies=A.MaxSpecies;Out.Hiss=A.GooseHiss;Out.Splash=A.GooseSplash;Out.Honk=A.GooseHonk;Out.Echo=A.GooseEcho;return true;
}
#endif
