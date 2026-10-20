// Native deer-first acceptance. Setup teleports are logged; approaches/retreats
// use ordinary movement input and actual sprint/crawl keys, never speed changes.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlifeApproachReview.h"
#include "HouseCalderWildlife.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderLandform.h"
#include "HouseCampusLayout.h"
#include "HouseCampusInteriors.h"
#include "HouseCampusPolish.h"
#include "HouseDuskAtmosphere.h"
#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "UObject/UnrealType.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderWildlifeApproachReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Pawn;
    TWeakObjectPtr<AHouseCalderWildlifeDirector> Director;
    TWeakObjectPtr<ACameraActor> Camera;
    TArray<FString> Modes,Captures;
    TArray<int32> RepKeys;
    FString Dir,Report,Rows=TEXT("mode,id,mood,distanceCm,expectedCm,errorCm,toleranceCm,serverTime,cause\n");
    FVector Start,Target,BoundsFrom,BoundsTo;
    int32 BoundsGroupFrom=0,BoundsGroupTo=0,MotionFrame=0,SoundStage=0;
    FString SoundName,SoundWav;
    int32 Phase=0,Trial=0,Habitat=1,Seen=0,PassedCases=0,ReactionCount=0,Detail=0,RecoveryId=-1,SettlingId=-1;
    double Began=0,TrialAt=0,ProgressAt=0,Best=MAX_flt,Until=0,FledAt=0,ReturnAt=0,StillAt=0,SettledAt=0;
    bool Settled=false,SettledThisTrial=false,Full=false,Recover=false,Network=false;
};
FState S;
void Note(const FString& Text)
{S.Report+=Text+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_REVIEW %s"),*Text);}
void Key(APlayerController* PC,FKey K,bool Press)
{PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Press?IE_Pressed:IE_Released,Press?1.f:0.f));}
void Posture(APlayerController* PC,AStoryFirstPersonCharacter* Pawn,const FString& Mode)
{
    auto* M=Cast<UStoryMovementComponent>(Pawn->GetCharacterMovement());
    Key(PC,EKeys::LeftShift,false);
    if(M && M->WantsToCrawl()!=(Mode==TEXT("crouch"))){Key(PC,EKeys::C,true);Key(PC,EKeys::C,false);}
    if(Mode==TEXT("sprint"))Key(PC,EKeys::LeftShift,true);
}
void Finish(bool Passed,const FString& Why)
{
    if(S.Pawn.IsValid())
    {S.Pawn->GetCharacterMovement()->StopMovementImmediately();if(auto* PC=Cast<APlayerController>(S.Pawn->GetController()))Posture(PC,S.Pawn.Get(),TEXT("walk"));}
    S.Phase=-1;
    if(Passed)for(const auto& Path:S.Captures)if(IFileManager::Get().FileSize(*Path)<=0){Passed=false;Note(TEXT("FAIL missing requested native screenshot ")+Path);}
    Note(FString::Printf(TEXT("%s scope=deerOnly logicProbe=%d renderFeetPending=%d cases=%d/%d directReactions=%d captures=%d tickP95ms=%.4f hoofGapP95cm=%.3f hoofDriftP95cm=%.3f actualSoleSamples=%d/%d terrainHash=%08x soundMultiplayerOtherSpeciesFinalPerformancePending=1 wholePhase3Complete=0 reason=%s"),
        Passed?TEXT("PASS"):TEXT("FAIL"),int32(HouseCalderWildlife::LogicProbe()),int32(HouseCalderWildlife::LogicProbe()),S.PassedCases,S.Modes.Num(),S.ReactionCount,S.Captures.Num(),S.Director.IsValid()?S.Director->WildlifeTickP95Ms():0.,S.Director.IsValid()?S.Director->HoofGapP95Cm():0.,S.Director.IsValid()?S.Director->HoofDriftP95Cm():0.,S.Director.IsValid()?S.Director->HoofGapSamples():0,S.Director.IsValid()?S.Director->HoofDriftSamples():0,HouseCalderTerrain::Hash(),*Why));
    FFileHelper::SaveStringToFile(S.Report,*(S.Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("reactions.csv")));
    FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
bool Floor(UWorld* World,AActor* Pawn,const FVector2D& P,float& Z)
{
    const float Datum=HouseExteriorSite::GroundHeight(P.X,P.Y);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderWildlifeReviewFloor),true,Pawn);FHitResult Hit;
    if(!World->LineTraceSingleByChannel(Hit,FVector(P,Datum+150),FVector(P,Datum-150),ECC_Visibility,Q))return false;
    Z=Hit.ImpactPoint.Z;return Hit.ImpactNormal.Z>=.766f && FMath::Abs(Z-Datum)<=3.f;
}
bool StandingEye(UWorld* World,AActor* Pawn,const FVector& At,double Angle,double Scale,int32 Tries,double Radius,FVector& Eye)
{
    for(const double Distance:{Radius,Radius+100.,Radius+250.})for(int32 Try=0;Try<Tries;++Try)
    {
        const double Offset=Try==0?0.:((Try+1)/2)*5.*(Try%2?1.:-1.);
        FVector Candidate=At+FRotator(0,Angle+Offset,0).Vector()*Distance;float Z=0;
        if(!Floor(World,Pawn,FVector2D(Candidate),Z))continue;Candidate.Z=Z+170.;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderWildlifeStandingView),true,Pawn);bool Blocked=false;
        for(const double Height:{20.,80.,135.})
        {FHitResult Hit;Blocked|=World->LineTraceSingleByChannel(Hit,Candidate,At+FVector(0,0,Height*Scale),ECC_Visibility,Q);}
        if(!Blocked){Eye=Candidate;return true;}
    }
    return false;
}
void AdvanceTrial(APlayerController* PC,AStoryFirstPersonCharacter* Pawn,double Now)
{
    PC->SetViewTarget(Pawn);
    if(++S.Trial<S.Modes.Num()){S.Phase=1;return;}
    if(S.Recover){if(S.Director.IsValid())S.Director->SetTimingView(TEXT("retreat"));S.Phase=4;S.Target=S.Start;S.Best=MAX_flt;S.ProgressAt=Now;return;}
    S.Phase=6;S.Until=Now+2.;
}
bool Camera(UWorld* World,APlayerController* PC,AHouseCalderWildlifeDirector* D)
{
    if(!S.Camera.IsValid())
    {FActorSpawnParameters Params;Params.Owner=D->GetOwner();S.Camera=World->SpawnActor<ACameraActor>(Params);}
    if(!S.Camera.IsValid())return false;
    S.Camera->GetCameraComponent()->SetFieldOfView(65.f);PC->SetViewTarget(S.Camera.Get());return true;
}
bool Profile(UWorld* World,AStoryFirstPersonCharacter* Pawn,const FVector& From,const FVector& To)
{
    const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const int32 Count=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(From,To)/25.));
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderWildlifeReviewCapsule),true,Pawn);float Before=0;
    for(int32 I=0;I<=Count;++I)
    {
        const FVector2D P(FMath::Lerp(From,To,double(I)/Count));float Z=0;
        if(!Floor(World,Pawn,P,Z) || (I>0 && FMath::Abs(Z-Before)>18.f)
            || World->OverlapBlockingTestByChannel(FVector(P,Z+Half+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(FMath::Max(1.f,Radius-1),Half-1),Q))return false;
        Before=Z;
    }
    return true;
}
bool Setup(UWorld* World,APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* D)
{
    Posture(PC,Pawn,TEXT("walk"));Pawn->GetCharacterMovement()->StopMovementImmediately();
    // A parked view pawn is deliberately excluded by the production 3D check.
    Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
    if(!D->ForceSpecies(TEXT("deer")))return false;
    const FString Mode=S.Modes[S.Trial];S.Habitat=Mode==TEXT("campus")?2:1;
    const double Factor=Mode==TEXT("sprint")?2.:Mode==TEXT("crouch")?.6:Mode==TEXT("campus")?.5:1.;
    const double Outer=6000.*Factor;bool Found=false;int32 Focal=-1;
    for(int32 Angle=0;Angle<72 && !Found;++Angle)
    {
        const FVector Dir=FRotator(0,90.+Angle*5.,0).Vector();double Ahead=-TNumericLimits<double>::Max();
        for(const auto& Item:D->States.Items)if(Item.Habitat==S.Habitat)
        {const double Along=FVector::DotProduct(D->AnimalPosition(Item.Id),Dir);if(Along>Ahead){Ahead=Along;Focal=Item.Id;}}
        const FVector Home=D->AnimalPosition(Focal);FVector Start=Home+Dir*(Outer*1.5),End=Home+Dir*(900.*Factor);float Z=0;
        if(FVector::Dist2D(Start,FVector::ZeroVector)>45000. || !Floor(World,Pawn,FVector2D(Start),Z))continue;
        Start.Z=Z;if(!Profile(World,Pawn,Start,End))continue;
        S.Start=Start;S.Target=End;Found=true;
    }
    if(!Found){Note(FString::Printf(TEXT("PROFILE FAIL mode=%s no physically clear ordinary-input line from1.5x outer range; no animal threshold changed"),*Mode));return false;}
    const double Now=World->GetTimeSeconds();S.TrialAt=S.ProgressAt=Now;S.Best=MAX_flt;S.Seen=0;S.SettledThisTrial=false;S.Settled=false;
    S.RepKeys.Init(INDEX_NONE,D->States.Items.Num());for(const auto& Item:D->States.Items)S.RepKeys[Item.Id]=Item.ReplicationKey;
    Pawn->SetActorLocation(S.Start+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4),false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Posture(PC,Pawn,Mode);PC->SetViewTarget(Pawn);
    PC->SetControlRotation(FRotator(-4,(D->AnimalPosition(Focal)+FVector(0,0,80)-Pawn->GetPawnViewLocation()).Rotation().Yaw,0));
    Note(FString::Printf(TEXT("SETUP mode=%s habitat=%d focal=%d start=%s goal=%s minimumStartDistanceCm=%.1f realMovementInput=1 playerSpeedUnchanged=1 setupTeleports=2"),*Mode,S.Habitat,Focal,*S.Start.ToString(),*S.Target.ToString(),Outer*1.5));
    D->SetTimingView(FName(*(TEXT("approach_")+Mode)));S.Phase=2;return true;
}
void Capture(const FString& Name)
{
    if(HouseCalderWildlife::LogicProbe())return;
    if(FScreenshotRequest::IsScreenshotRequested())return;
    const FString Path=S.Dir/(Name+TEXT(".png"));FScreenshotRequest::RequestScreenshot(Path,false,false);S.Captures.Add(Path);
}
bool ReadReactions(AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* D,double Now)
{
    const FString Mode=S.Modes[S.Trial];const double Factor=Mode==TEXT("sprint")?2.:Mode==TEXT("crouch")?.6:Mode==TEXT("campus")?.5:1.;
    for(const auto& Item:D->States.Items)
    {
        if(!S.RepKeys.IsValidIndex(Item.Id) || Item.ReplicationKey==S.RepKeys[Item.Id])continue;S.RepKeys[Item.Id]=Item.ReplicationKey;
        if(Item.Habitat!=S.Habitat || Item.Cause!=1)continue;
        const int32 Bit=Item.Mood==ECalderWildlifeMood::Staring?1:Item.Mood==ECalderWildlifeMood::Alarm?2:Item.Mood==ECalderWildlifeMood::Fleeing?4:0;
        if(Bit==0 || (S.Seen&Bit)!=0)continue;
        const double Base=Bit==1?6000.:Bit==2?4500.:3500.,Expected=Base*Factor,Tolerance=FMath::Max(120.,Expected*.2);
        const double Scale=Item.Variant==1?1.11:Item.Variant==2?.68:1.;
        const double Distance=FVector::Distance(FVector(Item.From)+FVector(0,0,80*Scale),Pawn->GetActorLocation()),Error=FMath::Abs(Distance-Expected);
        const auto* M=Cast<UStoryMovementComponent>(Pawn->GetCharacterMovement());
        const bool PostureCorrect=Mode==TEXT("sprint")?M&&M->IsSprinting():Mode==TEXT("crouch")?Pawn->bIsCrouched:!Pawn->bIsCrouched && (!M||!M->IsSprinting());
        S.Rows+=FString::Printf(TEXT("%s,%d,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n"),*Mode,Item.Id,HouseCalderWildlife::MoodName(Item.Mood),Distance,Expected,Error,Tolerance,HouseCalderWildlife::ServerTime(Pawn->GetWorld()),Item.Cause);
        Note(FString::Printf(TEXT("REACTION %s mode=%s id=%d mood=%s distanceCm=%.1f expectedCm=%.1f toleranceCm=%.1f postureCorrect=%d actual3D=1 direct=1"),Error<=Tolerance&&PostureCorrect?TEXT("PASS"):TEXT("FAIL"),*Mode,Item.Id,HouseCalderWildlife::MoodName(Item.Mood),Distance,Expected,Tolerance,int32(PostureCorrect)));
        if(Error>Tolerance || !PostureCorrect){Finish(false,TEXT("reaction distance or actual player posture failed"));return false;}
        S.Seen|=Bit;++S.ReactionCount;Capture(Mode+TEXT("_")+HouseCalderWildlife::MoodName(Item.Mood));
        if(Bit==1 && Mode==TEXT("walk") && !S.SettledThisTrial){S.Phase=3;S.StillAt=Now;S.SettledAt=0.;S.SettlingId=Item.Id;S.Until=Now+17.;Pawn->GetCharacterMovement()->StopMovementImmediately();}
        if(Bit==4){S.RecoveryId=Item.Id;S.FledAt=Item.StartTime;}
    }
    return true;
}
void Tick(UWorld* World,ELevelTick,float)
{
    if(!World || !World->IsGameWorld() || !World->HasBegunPlay() || (!FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeReview")) && !FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeNetworkReview")))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    if(FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeNetworkReview")) && (World->GetNetMode()!=NM_ListenServer || !S.Network))return;
    if(FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSoleReview")))return;
    const double Now=World->GetTimeSeconds();if(S.World.Get()!=World){S=FState();S.World=World;S.Began=Now;}
    if(S.Phase<0)return;
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    TActorIterator<AHouseCalderWildlifeDirector> It(World);AHouseCalderWildlifeDirector* D=It?*It:nullptr;
    if(!Pawn || !D || !D->Ready() || !HouseCalderTerrain::CollisionReady(World))
    {if(Now-S.Began>110.){S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeReview");IFileManager::Get().MakeDirectory(*S.Dir,true);Finish(false,TEXT("deer assets/director/player/terrain not ready"));}return;}
    S.Pawn=Pawn;S.Director=D;
    if(S.Phase==0)
    {
        S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeReview");IFileManager::Get().MakeDirectory(*S.Dir,true);
        FString Mode;FParse::Value(FCommandLine::Get(),TEXT("CalderWildlifeReviewMode="),Mode);
        if(Mode.IsEmpty()){S.Modes={TEXT("walk"),TEXT("sprint"),TEXT("crouch"),TEXT("campus")};S.Full=true;S.Recover=true;}
        else if(Mode==TEXT("detail")){S.Phase=6;S.Until=Now;}
        else if(Mode==TEXT("bounds")){S.Phase=10;S.Until=Now;}
        else if(Mode==TEXT("sound")){S.Phase=20;S.Until=Now;}
        else if(Mode==TEXT("walk")||Mode==TEXT("sprint")||Mode==TEXT("crouch")||Mode==TEXT("campus"))S.Modes={Mode};
        else {Finish(false,TEXT("unknown review mode"));return;}
        if(FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeReturnProbe")))S.Recover=true;
        if(AHUD* HUD=PC->GetHUD())HUD->bShowHUD=false;
        if(S.Phase!=6 && S.Phase!=10 && S.Phase!=20)S.Phase=1;
    }
    if(S.Phase>=20)
    {
        TInlineComponentArray<UAudioComponent*> Voices(D);
        UAudioComponent* OriginalCrickets=nullptr;
        for(TActorIterator<AHouseCampusPolish> Polish(World);Polish;++Polish)
            if(auto* Property=FindFProperty<FObjectPropertyBase>(AHouseCampusPolish::StaticClass(),TEXT("Crickets")))
                OriginalCrickets=Cast<UAudioComponent>(Property->GetObjectPropertyValue_InContainer(*Polish));
        if(!HouseCalderWildlife::SoundEnabled())
        {
            if(Now-S.Began<20.)return;
            Finish(Voices.Num()==0 && OriginalCrickets && OriginalCrickets->IsPlaying(),TEXT("NoWildlifeSound:0wildlifeaudio components and original campus cricket component playing;original woodsowl source unchanged"));return;
        }
        if(Voices.Num()!=6 || !D->WildlifeSoundSubmix() || !D->WildlifeSoundSubmixRegistered())
        {if(Now-S.Began>30.)Finish(false,TEXT("pooled wildlife audio not ready"));return;}
        if(OriginalCrickets){Finish(false,TEXT("original campus crickets overlap replacement night bed"));return;}
        if(auto* Device=World->GetAudioDeviceRaw())if(Device->GetNumActiveSources()>24){Finish(false,TEXT("active audio sources exceed24"));return;}
        const TCHAR* Names[]={TEXT("frat_lawn"),TEXT("inside_house"),TEXT("inside_campus"),TEXT("near_creek")};
        if(S.Phase==20)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
            if(!Camera(World,PC,D)){Finish(false,TEXT("sound camera missing"));return;}
            FVector Eye;bool Located=false;
            if(S.SoundStage==0)
            {float Z=0;Eye=FVector(-300,-2800,0);Located=Floor(World,Pawn,FVector2D(Eye),Z);Eye.Z=Z+170.;}
            else if(S.SoundStage==1)Eye=FVector(0,600,170.8),Located=true;
            else if(S.SoundStage==2)
            {const int32 Seed=HouseCampusLayout::Find(TEXT("LIB"));if(Seed>=0){Eye=HouseCampusInteriors::Frame(Seed).TransformPosition(FVector(0,1450,HouseCampusInteriors::FloorTop(HouseCampusLayout::Blocks()[Seed].Height,0)+170.));Located=true;}}
            else
            {
                const auto& Points=HouseCalderLandform::CreekPoints();
                for(int32 I=Points.Num()/3;I<Points.Num() && !Located;I+=FMath::Max(1,Points.Num()/30))for(const double Sign:{-1.,1.})
                {
                    const auto& P=Points[I];FVector Try(P.At.X+Sign*(P.HalfWidth+300.),P.At.Y,0);float Z=0;
                    if(Floor(World,Pawn,FVector2D(Try),Z)){Eye=Try;Eye.Z=Z+170.;Located=true;break;}
                }
            }
            if(!Located){Finish(false,TEXT("sound view has no legitimate standing location"));return;}
            S.Camera->SetActorLocationAndRotation(Eye,FRotator(-5,90,0));
            S.SoundName=Names[S.SoundStage];D->SetTimingView(FName(*S.SoundName));S.Phase=21;S.Until=Now+3.;return;
        }
        if(S.Phase==21 && Now>=S.Until)
        {
            FVector Eye=S.Camera->GetActorLocation();
            if(S.SoundStage==1 || S.SoundStage==2)
            {
                FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(WildlifeSoundIndoorFloor),true,Pawn);const double Expected=Eye.Z-170.;
                if(!World->LineTraceSingleByChannel(Hit,FVector(Eye.X,Eye.Y,Expected+35.),FVector(Eye.X,Eye.Y,Expected-30.),ECC_Pawn,Q) || FMath::Abs(Hit.ImpactPoint.Z-Expected)>12.)
                {Finish(false,TEXT("indoor sound view lacks an actual floor"));return;}
                Eye.Z=Hit.ImpactPoint.Z+170.;S.Camera->SetActorLocation(Eye);
                const bool Interior=S.SoundStage==1?UHouseDuskAtmosphereComponent::DistanceOutsideHouse(Eye)<1.:HouseCampusInteriors::DistanceOutside(Eye)<1.;
                if(!Interior){Finish(false,TEXT("sound test point is not inside requested building"));return;}
            }
            for(UAudioComponent* Voice:Voices)if((S.SoundStage==1 || S.SoundStage==2) && Voice->VolumeMultiplier>.00001f)
            {Finish(false,FString::Printf(TEXT("wildlife pooled voice not muted indoors name=%s playing=%d volume=%.6f"),*Voice->GetName(),int32(Voice->IsPlaying()),Voice->VolumeMultiplier));return;}
            Note(FString::Printf(TEXT("SOUND_RECORD view=%s eye=%s actualFloorPlus170=1 wildlifeVoices=%d"),*S.SoundName,*Eye.ToString(),Voices.Num()));
            UAudioMixerBlueprintLibrary::StartRecordingOutput(D,10.f,D->WildlifeSoundSubmix());S.Phase=22;S.Until=Now+8.;return;
        }
        if(S.Phase==22 && Now>=S.Until)
        {
            const FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSound"));IFileManager::Get().MakeDirectory(*Dir,true);
            UAudioMixerBlueprintLibrary::StopRecordingOutput(D,EAudioRecordingExportType::WavFile,TEXT("Wildlife_")+S.SoundName,Dir,D->WildlifeSoundSubmix());
            S.SoundWav=Dir/(TEXT("Wildlife_")+S.SoundName+TEXT(".wav"));Capture(TEXT("detail_wild_sound_")+S.SoundName);S.Phase=23;S.Until=Now+8.;return;
        }
        if(S.Phase==23 && !FScreenshotRequest::IsScreenshotRequested())
        {
            if(IFileManager::Get().FileSize(*S.SoundWav)<=44){if(Now>=S.Until)Finish(false,TEXT("requested wildlife submix WAV missing"));return;}
            Note(TEXT("SOUND_WAV ")+S.SoundWav);
            if(++S.SoundStage==4){Finish(true,TEXT("four native wildlife-only submix recordings made; waveform RMS analysis still required;voice6/source24/actual indoor volume0 checks passed"));return;}
            S.Phase=20;
        }
        return;
    }
    if(S.Phase==17)
    {
        // Finish the player's reaction snapshot before changing view target.
        // Otherwise its first-person head can appear in the same queued frame.
        if(FScreenshotRequest::IsScreenshotRequested() || Now<S.Until || S.Captures.IsEmpty() || IFileManager::Get().FileSize(*S.Captures.Last())<=0)
        {if(Now-S.ProgressAt>8.)Finish(false,TEXT("reaction snapshot did not finish before motion camera setup"));return;}
        Posture(PC,Pawn,TEXT("walk"));
        const auto& FleeState=D->States.Items[S.RecoveryId];const FVector At=D->AnimalPosition(S.RecoveryId);FVector Eye;
        const double Scale=FleeState.Variant==1?1.11:FleeState.Variant==2?.68:1.;
        if(!StandingEye(World,Pawn,At,FleeState.FacingYaw*360./65535.+90.,Scale,25,500.,Eye) || !Camera(World,PC,D))
        {Finish(false,TEXT("initial flee camera lacks legitimate floor"));return;}
        S.Camera->SetActorLocationAndRotation(Eye,(At+FVector(0,0,80.*Scale)-Eye).Rotation());
        S.Phase=14;S.MotionFrame=0;S.Until=Now+.6;D->SetTimingView(FName(*(TEXT("flee_")+S.Modes[S.Trial])));return;
    }
    if(S.Phase==14)
    {
        const int32 Id=S.RecoveryId;const FVector At=D->AnimalPosition(Id);const auto& State=D->States.Items[Id];
        FVector Eye;const double Scale=State.Variant==1?1.11:State.Variant==2?.68:1.;
        const double Angle=State.FacingYaw*360./65535.+90.;
        if(!StandingEye(World,Pawn,At,Angle,Scale,25,500.,Eye))
        {Finish(false,TEXT("no legitimate standing view for real flee motion"));return;}
        S.Camera->SetActorLocationAndRotation(Eye,(At+FVector(0,0,80.*Scale)-Eye).Rotation());
        if(Now>=S.Until && !FScreenshotRequest::IsScreenshotRequested())
        {
            Note(FString::Printf(TEXT("MOTION mode=%s frame=%d mood=%s position=%s eye=%s groundEyeCm=170"),*S.Modes[S.Trial],S.MotionFrame,HouseCalderWildlife::MoodName(State.Mood),*At.ToString(),*Eye.ToString()));
            Capture(FString::Printf(TEXT("%s_flee_pose_%02d"),*S.Modes[S.Trial],S.MotionFrame));++S.MotionFrame;S.Until=Now+.2;
            if(S.MotionFrame==8)S.Phase=15;
        }
        return;
    }
    if(S.Phase==15)
    {if(Now>=S.Until && !FScreenshotRequest::IsScreenshotRequested())AdvanceTrial(PC,Pawn,Now);return;}
    if(S.Phase==10)
    {
        if(HouseCalderWildlife::LogicProbe() || !HouseCalderWildlife::ScenePinned()){Finish(false,TEXT("bounds proof needs native rendering and pinned background clock"));return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
        double Farthest=0.;
        for(int32 A=0;A<3;++A)for(int32 B=A+1;B<3;++B)
            for(const FVector& From:D->HabitatNodes(A))for(const FVector& To:D->HabitatNodes(B))
            {const double Distance=FVector::Dist(From,To);if(Distance>Farthest){Farthest=Distance;S.BoundsFrom=From;S.BoundsTo=To;S.BoundsGroupFrom=A;S.BoundsGroupTo=B;}}
        Note(FString::Printf(TEXT("BOUNDS distanceCm=%.3f required>=30000 validAuthoredNodes=1 rendererSetupOnly=1 noGameplayRouteClaim=1"),Farthest));
        if(Farthest<30000. || !D->PlaceForBoundsReview(S.BoundsGroupFrom,S.BoundsFrom) || !Camera(World,PC,D))
        {Finish(false,TEXT("no native300m bounds setup on existing validated habitat nodes"));return;}
        S.Phase=11;S.Until=Now+3.;D->SetTimingView(TEXT("bounds_source"));return;
    }
    if(S.Phase==11 || S.Phase==13)
    {
        const FVector At=D->AnimalPosition(0);FVector Eye;
        if(!StandingEye(World,Pawn,At,90.,1.11,73,500.,Eye)){Finish(false,TEXT("bounds view lacks physical standing ground/sightline"));return;}
        S.Camera->SetActorLocationAndRotation(Eye,(At+FVector(0,0,88.8)-Eye).Rotation());
        if(Now<S.Until || FScreenshotRequest::IsScreenshotRequested())return;
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(D);bool Visible=false;
        for(auto* Part:Parts)if(Part->GetName()==TEXT("Wildlife_DeerBody"))
            Visible=Part->IsVisible() && Part->Bounds.GetBox().IsInsideOrOn(At+FVector(0,0,88.8)) && Part->GetLastRenderTimeOnScreen()>Now-.5;
        if(!Visible){Finish(false,TEXT("actual wildlife body bounds/visible render timestamp failed"));return;}
        Capture(S.Phase==11?TEXT("detail_wild_deer_bounds_source"):TEXT("detail_wild_deer_bounds_destination"));
        S.Phase=S.Phase==11?12:16;S.Until=Now+.3;return;
    }
    if(S.Phase==12)
    {
        if(Now<S.Until || FScreenshotRequest::IsScreenshotRequested())return;
        if(!D->PlaceForBoundsReview(S.BoundsGroupTo,S.BoundsTo)){Finish(false,TEXT("bounds destination rejected"));return;}
        S.Phase=13;S.Until=Now+3.;D->SetTimingView(TEXT("bounds_destination"));return;
    }
    if(S.Phase==16)
    {
        if(Now>=S.Until && !FScreenshotRequest::IsScreenshotRequested())Finish(true,TEXT("same body instance renders>300m from its explicit test spawn;two native captures require inspection;renderer-only setup,not gameplay path"));
        return;
    }
    if(S.Phase==1){if(!Setup(World,PC,Pawn,D))Finish(false,TEXT("ordinary-input approach profile unavailable"));return;}
    if(S.Phase==2 || S.Phase==3)
    {
        if(!ReadReactions(Pawn,D,Now))return;
        if(S.Phase==3)
        {
            const bool SameDeerFeeding=D->States.Items.IsValidIndex(S.SettlingId) && D->States.Items[S.SettlingId].Mood==ECalderWildlifeMood::Feeding;
            if(SameDeerFeeding && S.SettledAt==0.)S.SettledAt=Now;
            if(Now>=S.Until)
            {if(!SameDeerFeeding || S.SettledAt-S.StillAt<14.8){Finish(false,TEXT("15-second stillness did not settle the same wary deer at the required time"));return;}S.SettledThisTrial=true;S.Phase=2;S.ProgressAt=Now;S.Best=MAX_flt;Note(TEXT("SETTLE PASS ordinary player stillness>=15s; no state forced"));}
            return;
        }
        if(S.Seen==7)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();++S.PassedCases;
            if(S.Network){Posture(PC,Pawn,TEXT("walk"));S.Phase=9;FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("reactions.csv")));FFileHelper::SaveStringToFile(S.Report,*(S.Dir/TEXT("approach.txt")));return;}
            if(!HouseCalderWildlife::LogicProbe()){S.Phase=17;S.Until=Now+.2;S.ProgressAt=Now;return;}
            Posture(PC,Pawn,TEXT("walk"));AdvanceTrial(PC,Pawn,Now);return;
        }
        const double Distance=FVector::Dist2D(Pawn->GetActorLocation(),S.Target);
        if(Distance<S.Best-5.){S.Best=Distance;S.ProgressAt=Now;}
        if(Now-S.ProgressAt>8. || Now-S.TrialAt>160. || Distance<45.){Finish(false,TEXT("approach stalled or missing required reaction"));return;}
        return;
    }
    if(S.Phase==4)
    {
        const double Distance=FVector::Dist2D(Pawn->GetActorLocation(),S.Target);
        if(Distance<S.Best-5.){S.Best=Distance;S.ProgressAt=Now;}
        if(Now-S.ProgressAt>8.){Finish(false,TEXT("ordinary-input retreat stalled"));return;}
        if(Distance<45.){Pawn->GetCharacterMovement()->StopMovementImmediately();S.Phase=5;D->SetTimingView(TEXT("return_wait"));S.Until=S.FledAt+370.;Note(TEXT("RETURN waiting real2-4minutes; server clock not accelerated"));}
        return;
    }
    if(S.Phase==5)
    {
        const auto& Item=D->States.Items[S.RecoveryId];
        if(Item.Mood==ECalderWildlifeMood::Returning && S.ReturnAt==0.)
        {
            D->SetTimingView(TEXT("return_walk"));S.ReturnAt=Item.StartTime;const double Delay=S.ReturnAt-S.FledAt;
            Note(FString::Printf(TEXT("RETURN %s delaySeconds=%.2f required=120..240 id=%d"),Delay>=119.8 && Delay<=240.2?TEXT("PASS"):TEXT("FAIL"),Delay,S.RecoveryId));
            if(Delay<119.8 || Delay>240.2){Finish(false,TEXT("return delay outside required2-4minutes"));return;}
        }
        if(S.ReturnAt>0. && Item.Mood==ECalderWildlifeMood::Feeding && Now>S.ReturnAt+1.){S.Phase=6;S.Until=Now;return;}
        if(Now>S.Until){Finish(false,TEXT("deer did not return to feed after real recovery wait"));return;}
        return;
    }
    if(S.Phase==6)
    {
        if(HouseCalderWildlife::LogicProbe()){S.Phase=8;S.Until=Now+2.;return;}
        Pawn->GetCharacterMovement()->StopMovementImmediately();Posture(PC,Pawn,TEXT("walk"));Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
        if(!Camera(World,PC,D)){Finish(false,TEXT("detail camera missing"));return;}
        S.Phase=7;S.Detail=0;S.Until=Now+1.;return;
    }
    if(S.Phase==7)
    {
        const int32 Ids[]={0,1,2};const int32 Id=Ids[FMath::Min(2,S.Detail/3)];const FVector At=D->AnimalPosition(Id);
        const double Heading=D->States.Items[Id].FacingYaw*360./65535.,Angle=Heading+(S.Detail%3==0?90.:S.Detail%3==1?-50.:180.);
        const double Scale=D->States.Items[Id].Variant==1?1.11:D->States.Items[Id].Variant==2?.68:1.;
        const FVector Target=At+FVector(0,0,80.*Scale);FVector Eye;
        if(!StandingEye(World,Pawn,At,Angle,Scale,17,350.,Eye)){Finish(false,TEXT("detail view has no clear physical standing location within40degrees; eye height unchanged"));return;}
        D->SetTimingView(FName(*FString::Printf(TEXT("detail_%02d"),S.Detail)));
        S.Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());
        // Startup can still reset the player's view after the first setup.
        // Require the real cached POV to settle at this standing eye before
        // requesting a screenshot; a camera actor's transform alone is not proof.
        PC->SetViewTarget(S.Camera.Get());
        const APlayerCameraManager* Manager=PC->PlayerCameraManager.Get();
        if(!Manager || FVector::DistSquared(Manager->GetCameraLocation(),Eye)>4.
            || FVector::DotProduct(Manager->GetCameraRotation().Vector(),(Target-Eye).GetSafeNormal())<.9999)
        {S.Until=Now+.3;return;}
        if(Now>=S.Until && !FScreenshotRequest::IsScreenshotRequested())
        {Capture(FString::Printf(TEXT("detail_wild_deer_%02d"),S.Detail));++S.Detail;S.Until=Now+3.;if(S.Detail==9){S.Phase=8;S.Until=Now+2.;}}
        return;
    }
    if(S.Phase==8 && Now>=S.Until && !FScreenshotRequest::IsScreenshotRequested())
    {
        const bool Feet=D->HoofGapSamples()>=100 && D->HoofDriftSamples()>=100 && D->HoofGapP95Cm()<=3. && D->HoofDriftP95Cm()<=3.;
        if(HouseCalderWildlife::LogicProbe())Finish(true,TEXT("nonrendered ordinary-input reaction diagnostic only; static assets/render/feet untested"));
        else Finish(Feet,Feet?TEXT("deer-only route/reaction/detail/actual sole metrics passed; remaining master gates are separate"):TEXT("actual imported sole sample count/gap/drift failed"));
    }
}
struct FRegistration
{
    FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);FCoreDelegates::OnBeginFrame.AddLambda([]
    {if((S.Phase==2 || S.Phase==4) && S.Pawn.IsValid())S.Pawn->AddMovementInput((S.Target-S.Pawn->GetActorLocation()).GetSafeNormal2D(),1.f);});}
}Registration;
}
namespace HouseCalderWildlifeApproachReview
{
bool BeginNetwork(UWorld* World,APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* Director)
{
    namespace R=HouseCalderWildlifeApproachReviewImpl;
    R::S=R::FState();R::S.World=World;R::S.Pawn=Pawn;R::S.Director=Director;R::S.Network=true;R::S.Began=World->GetTimeSeconds();R::S.Modes={TEXT("walk")};
    R::S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeNetworkApproach");IFileManager::Get().MakeDirectory(*R::S.Dir,true);
    if(AHUD* HUD=PC->GetHUD())HUD->bShowHUD=false;return R::Setup(World,PC,Pawn,Director);
}
bool NetworkFled(int32& Id)
{namespace R=HouseCalderWildlifeApproachReviewImpl;Id=R::S.RecoveryId;return R::S.Network && R::S.Phase==9 && R::S.Seen==7 && Id>=0;}
}
#endif
