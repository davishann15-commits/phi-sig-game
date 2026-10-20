// PREPARED ONLY. Append to owned Mood.cpp only after stage6 acceptance/advance7.
// Actual registered-submix recordings, live-source consumption and temporal geometry.
#if !UE_BUILD_SHIPPING
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HouseCampusFall.h"
#include "HouseExteriorSiteLayout.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UnrealClient.h"
namespace HouseStadiumSoundReviewImpl
{
struct FPhase{const TCHAR* Name;double Seconds;FVector Eye,Target;};
const FPhase Phases[]={
 {TEXT("ground"),48,{2400,-9000,113},{2400,-10000,1700}},
 {TEXT("stands"),48,{5958.87,-12299.70,350},{7351.47,-11908.27,860}},
 {TEXT("first_snare"),54,{2400,-9000,113},{2400,-10000,1700}},
 {TEXT("inside_stadium"),48,{4342.79,-10560.73,115},{5096.13,-10872.15,25}},
 {TEXT("inside_house"),48,{0,400,170},{800,400,170}},
 {TEXT("far"),48,{16000,-16000,170},{2400,-13300,800}},
 {TEXT("recovery"),228,{2400,-9000,113},{2400,-10000,1700}}
};
struct FState
{
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;
 TWeakObjectPtr<UAudioComponent> Voices[4];TWeakObjectPtr<UInstancedStaticMeshComponent> Programs;
 TWeakObjectPtr<UProceduralMeshComponent> Cloth;int32 ClothSection=-1;
 TArray<int32> FlagIndices;TArray<FVector> FlagFirst;
 TArray<FString> Rows;int32 Phase=0,Drain[3]={},DrumGenerations=0,PaperCorners=0,PaperProblems=0,SourceRoutingProblems=0,InsidePlayingProblems=0,WaveId[4]={},Queue[4]={};
 double PhaseStart=0,NextSample=0,TemporalStart=0,NextShot=0;int32 Temporal=0,PaperShots=0,FlagShots=0,DimSamples[2]={},BrightSamples[2]={};
 float MinPaperClearance=FLT_MAX,FlagDelta=0;bool Ready=false,Recording=false,PhaseShot=false,Done=false,AudioDone=false;
};
FState State;
FString Dir(){return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("StadiumSoundReview"));}
USoundSubmix* Mix(UWorld* W,int32 Index){return HouseStadiumDetail::SoundReviewMix(W,Index);}
void Pose(UWorld* W,FVector Eye,FVector Target,float Fov=75.f)
{
 auto& S=State;if(!S.Camera.IsValid())return;S.Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());S.Camera->GetCameraComponent()->SetFieldOfView(Fov);
 if(auto* PC=W->GetFirstPlayerController())if(PC->GetViewTarget()!=S.Camera.Get())PC->SetViewTarget(S.Camera.Get());
}
void StopRecord(UWorld* W)
{
 auto& S=State;if(!S.Recording)return;
 const TCHAR* VoiceNames[]={TEXT("Flood"),TEXT("PA"),TEXT("Halyard"),TEXT("Snare")};
 for(int32 I=0;I<4;++I)if(auto* Submix=Mix(W,I))UAudioMixerBlueprintLibrary::StopRecordingOutput(W,EAudioRecordingExportType::WavFile,FString::Printf(TEXT("P%d_%s_%s"),S.Phase,Phases[S.Phase].Name,VoiceNames[I]),Dir(),Submix,nullptr);
 S.Recording=false;UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SOUND_RECORD_STOP phase=%d world=%.6f audio=%.6f submixes=4 noMasterFallback=1"),S.Phase,W->GetTimeSeconds(),W->GetAudioTimeSeconds());
}
void Finish(UWorld* W,bool Passed,const TCHAR* Why)
{
 auto& S=State;if(S.Done)return;StopRecord(W);S.Done=true;IFileManager::Get().MakeDirectory(*Dir(),true);
 const FString Result=FString::Printf(TEXT("%s scope=nativeStadiumSound registeredSubmixes=4 phases=7 drumGenerations=%d drain0=%d drain1=%d drain2=%d routingProblems=%d insidePlayingProblems=%d paperCorners=%d paperProblems=%d minimumPaperClearanceCm=%.6f flagDisplacementCm=%.6f scoreboardDimSamples=%d floodDimSamples=%d temporalPaper=%d temporalFlags=%d reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),S.DrumGenerations,S.Drain[0],S.Drain[1],S.Drain[2],S.SourceRoutingProblems,S.InsidePlayingProblems,S.PaperCorners,S.PaperProblems,S.MinPaperClearance,S.FlagDelta,S.DimSamples[0],S.DimSamples[1],S.PaperShots,S.FlagShots,Why);
 FFileHelper::SaveStringToFile(Result,*(Dir()/TEXT("result.txt")));FFileHelper::SaveStringArrayToFile(S.Rows,*(Dir()/TEXT("live_sources.csv")));UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SOUND_REVIEW %s"),*Result);FPlatformMisc::RequestExit(false);
}
void Tick(UWorld* W,ELevelTick,float)
{
 static const bool Requested=FParse::Param(FCommandLine::Get(),TEXT("StadiumSoundReview"));if(!Requested||!W||!W->IsGameWorld()||HouseStadiumDetail::Step()<7||W->GetTimeSeconds()<12)return;
 auto& S=State;if(S.World.Get()!=W){S=FState();S.World=W;}if(S.Done)return;
 const double Now=W->GetTimeSeconds();auto* PC=W->GetFirstPlayerController();if(!PC)return;
 AHouseStadiumDetail* Stadium=nullptr;{TActorIterator<AHouseStadiumDetail> It(W);if(It)Stadium=*It;}if(!Stadium)return;
 if(!S.Ready)
 {
  bool Registered=true;for(int32 I=0;I<4;++I)Registered&=Mix(W,I)!=nullptr;
  if(!Registered){if(Now>45)Finish(W,false,TEXT("Four owned submixes were not actually registered"));return;}
  TInlineComponentArray<UAudioComponent*> Owned(Stadium);for(auto* C:Owned)for(int32 I=0;I<3;++I)if(C->GetName()==FString::Printf(TEXT("CalderStadiumSound%d"),I))S.Voices[I]=C;
  for(TActorIterator<AHouseCampusFall> It(W);It;++It){TInlineComponentArray<UAudioComponent*> Parts(*It);for(auto* C:Parts)if(C->GetName()==TEXT("CampusFallDrums"))S.Voices[3]=C;TInlineComponentArray<UProceduralMeshComponent*> Meshes(*It);for(auto* M:Meshes)if(M->GetName()==TEXT("CampusFallCloth"))S.Cloth=M;}
  for(int32 I=0;I<4;++I)if(!S.Voices[I].IsValid()){Finish(W,false,TEXT("Missing real source component"));return;}
  TInlineComponentArray<UInstancedStaticMeshComponent*> Instanced(Stadium);for(auto* C:Instanced)if(C->GetName()==TEXT("CalderStadiumDriftingPrograms"))S.Programs=C;
  if(!S.Programs.IsValid()||S.Programs->GetInstanceCount()!=6){Finish(W,false,TEXT("Six real moving programs required"));return;}
  FActorSpawnParameters Spawn;Spawn.Name=TEXT("CalderStadiumSoundReviewCamera");Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;S.Camera=W->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),FTransform::Identity,Spawn);if(!S.Camera.IsValid())return;
  if(APawn* Pawn=PC->GetPawn()){Pawn->SetActorHiddenInGame(true);if(auto* Ch=Cast<ACharacter>(Pawn))if(auto* Movement=Ch->GetCharacterMovement())Movement->SetMovementMode(MOVE_None);Pawn->SetActorLocation({0,0,12000},false,nullptr,ETeleportType::TeleportPhysics);}if(auto* Hud=PC->GetHUD())Hud->bShowHUD=false;
  S.Rows.Add(TEXT("world,audio,phase,inside,voice,playing,wave_id,available_bytes,source_x,source_y,source_z,listener_x,listener_y,listener_z"));S.PhaseStart=Now;S.NextSample=Now;S.Ready=true;IFileManager::Get().MakeDirectory(*Dir(),true);
  UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SOUND_BEGIN world=%.6f audio=%.6f actualRegisteredSubmixes=4 defaultTimerUnchanged=1 expectedRealAudioSeconds=522"),Now,W->GetAudioTimeSeconds());
 }
 if(!S.AudioDone)
 {
  const auto& P=Phases[S.Phase];FVector Eye=P.Eye;if(S.Phase==5)Eye.Z=HouseExteriorSite::GroundHeight(Eye.X,Eye.Y)+170.f;Pose(W,Eye,P.Target);const double Age=Now-S.PhaseStart;
  if(Age>=3&&!S.Recording){bool Registered=true;for(int32 I=0;I<4;++I)Registered&=Mix(W,I)!=nullptr;if(!Registered){Finish(W,false,TEXT("Refusing master recording fallback"));return;}for(int32 I=0;I<4;++I)UAudioMixerBlueprintLibrary::StartRecordingOutput(W,float(P.Seconds-Age+2),Mix(W,I));S.Recording=true;UE_LOG(LogTemp,Display,TEXT("CALDER_STADIUM_SOUND_RECORD_START phase=%d name=%s world=%.6f audio=%.6f seconds=%.3f"),S.Phase,P.Name,Now,W->GetAudioTimeSeconds(),P.Seconds-Age);}
  if(Age>=3.5&&!S.PhaseShot&&!FScreenshotRequest::IsScreenshotRequested()){FScreenshotRequest::RequestScreenshot(Dir()/FString::Printf(TEXT("P%d_%s.png"),S.Phase,P.Name),false,false);S.PhaseShot=true;}
  if(Age>=P.Seconds)
  {StopRecord(W);++S.Phase;S.PhaseShot=false;S.PhaseStart=Now;if(S.Phase==UE_ARRAY_COUNT(Phases)){S.AudioDone=true;S.TemporalStart=Now;S.Temporal=0;S.NextShot=Now+3;}}
 }
 const FVector Listener=PC->PlayerCameraManager?PC->PlayerCameraManager->GetCameraLocation():FVector::ZeroVector;
 const bool Inside=FMath::Min(HouseCampusInteriors::DistanceOutside(Listener),UHouseDuskAtmosphereComponent::DistanceOutsideHouse(Listener))<=1.f;
 if(Now>=S.NextSample)
 {
  S.NextSample=Now+.5;
  for(int32 I=0;I<4;++I)
  {
   auto* C=S.Voices[I].Get();auto* Wave=C?Cast<USoundWaveProcedural>(C->GetSound()):nullptr;const int32 Id=Wave?int32(Wave->GetUniqueID()):0,Available=Wave?Wave->GetAvailableAudioByteCount():0;const bool Playing=C&&C->IsPlaying();
   if(I<3&&Playing&&Id==S.WaveId[I]&&Available<S.Queue[I])++S.Drain[I];if(I==3&&Id&&Id!=S.WaveId[I])++S.DrumGenerations;
   if(Playing&&(!Wave||Wave->SoundSubmixObject!=Mix(W,I)))++S.SourceRoutingProblems;if(I<3&&Inside&&Now-S.PhaseStart>=3.5&&Playing)++S.InsidePlayingProblems;
   const FVector At=C?C->GetComponentLocation():FVector::ZeroVector;S.Rows.Add(FString::Printf(TEXT("%.6f,%.6f,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f"),Now,W->GetAudioTimeSeconds(),S.Phase,int(Inside),I,int(Playing),Id,Available,At.X,At.Y,At.Z,Listener.X,Listener.Y,Listener.Z));S.WaveId[I]=Id;S.Queue[I]=Available;
  }
  FCollisionQueryParams Query(SCENE_QUERY_STAT(StadiumMovingPaper),true);if(PC->GetPawn())Query.AddIgnoredActor(PC->GetPawn());
  for(int32 I=0;I<6;++I){FTransform X;if(!S.Programs->GetInstanceTransform(I,X,true)){++S.PaperProblems;continue;}const double Floor=180.+(I+2)*63.;for(float A:{-50.f,50.f})for(float B:{-50.f,50.f}){const FVector Corner=X.TransformPosition({A,B,-50});FHitResult Hit;++S.PaperCorners;const FVector Probe(Corner.X,Corner.Y,Floor);const bool Found=W->LineTraceSingleByChannel(Hit,Probe+FVector(0,0,6),Probe-FVector(0,0,10),ECC_Pawn,Query);const float Clearance=Found?float(Corner.Z-Hit.ImpactPoint.Z):-1000.f;S.MinPaperClearance=FMath::Min(S.MinPaperClearance,Clearance);if(!Found||Hit.ImpactNormal.Z<.98||FMath::Abs(Hit.ImpactPoint.Z-Floor)>1||Clearance<.01f||Clearance>6)++S.PaperProblems;}}
  if(S.Cloth.IsValid())for(int32 Part=0;Part<S.Cloth->GetNumSections();++Part)if(S.Cloth->GetMaterial(Part)==Stadium->Look->Mat(TEXT("SD_Fabric")))if(auto* Sec=S.Cloth->GetProcMeshSection(Part))
  {
   if(S.ClothSection<0){S.ClothSection=Part;for(int32 I=0;I<Sec->ProcVertexBuffer.Num();++I){const FVector At=Sec->ProcVertexBuffer[I].Position;if(At.X>3800&&At.X<3900&&At.Z<2880&&At.Z>2780){S.FlagIndices.Add(I);S.FlagFirst.Add(At);}}}
   if(Part==S.ClothSection)for(int32 I=0;I<S.FlagIndices.Num();++I)if(Sec->ProcVertexBuffer.IsValidIndex(S.FlagIndices[I]))S.FlagDelta=FMath::Max(S.FlagDelta,float(FVector::Dist(S.FlagFirst[I],FVector(Sec->ProcVertexBuffer[S.FlagIndices[I]].Position))));
  }
 }
 for(int32 I=0;I<2;++I)if(auto* Material=Cast<UMaterialInstanceDynamic>(Stadium->Look->Mat(I==0?TEXT("SD_FlickerBulb"):TEXT("SD_FloodFlicker"))))
 {FLinearColor Colour;const bool Found=Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Emission")),Colour);if(Found){const float Base=I==0?14.f:22.f;if(Colour.R<Base*.95f)++S.DimSamples[I];else ++S.BrightSamples[I];}}
 if(S.AudioDone)
 {
  if(S.Temporal<4)
  {
   const bool Flood=S.Temporal>=2,Dim=(S.Temporal%2)==0;
   if(Flood)Pose(W,{-1650,-11400,3300},{-2370.742,-11032.163,3840},45);else Pose(W,{2650,-14200,1250},{2400,-16972,1720},42);
   const double Phase=FMath::Fmod(Now,23.);
   if(Now>=S.NextShot&&!FScreenshotRequest::IsScreenshotRequested()&&((Dim&&Phase>22.72&&Phase<22.9)||(!Dim&&Phase>.4&&Phase<.65)))
   {const TCHAR* Name=Flood?(Dim?TEXT("flood_flicker_dim.png"):TEXT("flood_flicker_bright.png")):(Dim?TEXT("flicker_dim.png"):TEXT("flicker_bright.png"));FScreenshotRequest::RequestScreenshot(Dir()/Name,false,false);++S.Temporal;S.NextShot=Now+(S.Temporal<4?.25:3.);}
  }
  else if(S.Temporal==4)
  {
   Pose(W,{-485.863,-15722.141,710},{-451.251,-15776.403,495.18},75);
   if(Now>=S.NextShot&&!FScreenshotRequest::IsScreenshotRequested()){FScreenshotRequest::RequestScreenshot(Dir()/FString::Printf(TEXT("paper_%02d.png"),S.PaperShots),false,false);++S.PaperShots;S.NextShot=Now+.8;if(S.PaperShots==6){++S.Temporal;S.NextShot=Now+3;}}
  }
  else if(S.Temporal==5)
  {
   Pose(W,{2400,-13300,114},{2400,-9900,1650},63);
   if(Now>=S.NextShot&&!FScreenshotRequest::IsScreenshotRequested()){FScreenshotRequest::RequestScreenshot(Dir()/FString::Printf(TEXT("flags_%02d.png"),S.FlagShots),false,false);++S.FlagShots;S.NextShot=Now+.8;if(S.FlagShots==6){++S.Temporal;S.NextShot=Now+3;}}
  }
  else if(S.Temporal==6)
  {
   // Look down from the unchanged original170cm tunnel eye to inspect the actual torn floor banner.
   Pose(W,{4594.49,-10869.98,115},{4492.084,-10983.377,-54.88},75);
   if(Now>=S.NextShot&&!FScreenshotRequest::IsScreenshotRequested()){FScreenshotRequest::RequestScreenshot(Dir()/TEXT("banner_floor.png"),false,false);++S.Temporal;S.NextShot=Now+1;}
  }
  else if(Now>=S.NextShot&&!FScreenshotRequest::IsScreenshotRequested())
  {const bool Passed=S.DrumGenerations>=2&&S.Drain[0]>20&&S.Drain[1]>20&&S.SourceRoutingProblems==0&&S.InsidePlayingProblems==0&&S.PaperCorners>1000&&S.PaperProblems==0&&S.FlagDelta>.5f&&S.DimSamples[0]>0&&S.DimSamples[1]>0&&S.BrightSamples[0]>0&&S.BrightSamples[1]>0;Finish(W,Passed,TEXT("Native files require offline PCM/schedule/image inspection before acceptance"));}
 }
 if(Now-S.PhaseStart>900&&!S.AudioDone)Finish(W,false,TEXT("Unexpected phase stall"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
