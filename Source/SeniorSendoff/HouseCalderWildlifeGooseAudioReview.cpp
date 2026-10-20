// Actual mixer/submix, voice caps and submitted beak controls. No sound or
// photographic acceptance is inferred from synthesis or compilation alone.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeBirdCalls.h"
#include "HouseCalderWildlifeBirds.h"
#include "HouseCalderWildlifeSkyPaths.h"
#include "HouseCalderWildlifeGoosePaths.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace HouseCalderWildlifeGooseAudioReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Camera;bool Ready=false,Done=false,Hiss=false;
    HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Pond;
    double Began=0.,Started=0.,WorstMouth=0.;int32 MouthSamples=0,Shot=0;FString Dir;
};FState S;
void Finish(UWorld* W,bool Passed,const FString& Why,const FCalderGooseAudioProbe& P)
{
    S.Done=true;const FString Result=FString::Printf(TEXT("%s scope=nativeGooseAudioIntegration buffers=%d components=%d peakVoices=%d peakSources=%d maxPerSpecies=%d honks=%d echoes=%d splashes=%d hisses=%d actualBeakSamples=%d maximumBeakAngleErrorRad=%.6f captures=%d registeredWildlifeSubmix=1 sourceWavsDumped=1 subjectiveListeningAccepted=0 indoorLawnAccepted=0 fullPerformanceAccepted=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),P.Buffers,P.Voices,P.PeakVoices,P.PeakSources,P.MaxSpecies,P.Honk,P.Echo,P.Splash,P.Hiss,S.MouthSamples,S.WorstMouth,S.Shot,*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_AUDIO_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioReview"))||!W||!W->IsGameWorld()||!W->HasBegunPlay()||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=W){S=FState();S.World=W;S.Began=Wall;S.Hiss=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionReview"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionNetworkReview"))||FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeReview"));S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderGooseAudioReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Done)return;FCalderGooseAudioProbe P;if(Wall-S.Began>180.&&!S.Hiss){Finish(W,false,TEXT("audio review timeout"),P);return;}
    TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;auto* PC=W->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;if(!D||!PC||!Pawn||!D->ReadGooseAudioForReview(P))return;
    if(P.Buffers!=HouseCalderWildlifeBirdCalls::Count||P.Voices!=6||P.PeakVoices>7||P.PeakSources>24||P.MaxSpecies>1){Finish(W,false,TEXT("actual audio pool/source/species limit failed"),P);return;}
    if(S.Hiss)return; // Existing ordinary-input review owns this test's end.
    if(!S.Ready)
    {
        if(!S.Sky.Build(W)||!S.Pond.Build(W,S.Sky)){Finish(W,false,TEXT("native audio reference path failed"),P);return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->DisableMovement();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);if(auto* H=PC->GetHUD())H->bShowHUD=false;
        S.Camera=W->SpawnActor<ACameraActor>();if(!S.Camera.IsValid()){Finish(W,false,TEXT("no native audio camera"),P);return;}
        const FVector Eye(25544.683,-10860.777,HouseExteriorSite::GroundHeight(25544.683,-10860.777)+170.);S.Camera->SetActorLocation(Eye);S.Camera->GetCameraComponent()->SetFieldOfView(65.f);PC->SetViewTarget(S.Camera.Get());S.Ready=true;S.Started=Wall;D->SetTimingView(TEXT("goose_audio_arrival"));
    }
    const double Scene=HouseCalderWildlife::SceneTime(W);FVector Look=S.Pond.Touches[0];
    UInstancedStaticMeshComponent* SkyPart=nullptr;TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(D);for(auto* Part:Parts)if(Part->GetName()==TEXT("Wildlife_Sky_0"))SkyPart=Part;
    for(int32 Event=0;Event<2;++Event)
    {
        HouseCalderWildlifeSkyPaths::FSample Lead;if(!S.Sky.EvaluateGoose(Scene,Event,0,Lead))continue;Look=Lead.Position;
        int32 Index=0,Member=0;double Age=0.;if(!HouseCalderWildlifeBirdCalls::Cue(Lead.Age,Event,Index,Member,Age)||!SkyPart)continue;
        FTransform Root;bool Visible=false;double Rendered=0.;if(!D->ReadSkyForReview(0,Member,Root,Visible,Rendered)||!Visible)continue;HouseCalderWildlifeSkyPaths::FSample Bird;if(!S.Sky.EvaluateGoose(Rendered,Event,Member,Bird))continue;
        const double Mouth=HouseCalderWildlifeBirdCalls::Mouth(Bird.Age,Event,Member);if(Mouth<.1)continue;const int32 Offset=Member*225+14*7;if(SkyPart->PerInstanceSMCustomData.Num()<Offset+4){Finish(W,false,TEXT("missing actual beak custom data"),P);return;}
        TArray<FTransform> Closed;HouseCalderWildlifeBirds::Pose(0,Rendered+double(Bird.Seed%1009u)*.013,Root,Closed,0.);
        const auto& A=SkyPart->PerInstanceSMCustomData;FQuat Q(A[Offset],A[Offset+1],A[Offset+2],A[Offset+3]);Q.Normalize();const double Angle=Q.AngularDistance(Closed[14].GetRotation());S.WorstMouth=FMath::Max(S.WorstMouth,FMath::Abs(Angle-.20*Mouth));++S.MouthSamples;
        if(S.WorstMouth>.005){Finish(W,false,TEXT("submitted beak and scheduled honk cue disagree"),P);return;}
    }
    S.Camera->SetActorRotation((Look-S.Camera->GetActorLocation()).Rotation());
    const double At[]={5.,18.,33.,62.};if(S.Shot<4&&Wall-S.Started>=At[S.Shot]&&!FScreenshotRequest::IsScreenshotRequested())
    {GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(S.Dir/FString::Printf(TEXT("detail_goose_audio_%02d.png"),S.Shot++),false,false);}
    if(Wall-S.Started>=67.&&!FScreenshotRequest::IsScreenshotRequested())
    {
        const FString Wav=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSound/WildlifeMix.wav");
        Finish(W,P.Honk>0&&P.Echo>0&&P.Splash>0&&S.MouthSamples>0&&S.Shot==4&&IFileManager::Get().FileSize(*Wav)>44,TEXT("native pool/beak/mix files produced;analyzePCM/listen/checkindoors/lawn/performance"),P);
    }
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
