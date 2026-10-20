// Real controlled-pawn approach. Teleports only place the initial player;
// approach and retreat use normal movement/input without animal forcing.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlifeGooseReactionReview.h"
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeGooseShore.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace HouseCalderWildlifeGooseReactionReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeGoosePaths::FPlan Pond;HouseCalderWildlifeGooseShore::FPlan Shore;
    int32 Stage=0,Id=-1,Roots=0;double Began=0.,SetupAt=0.,ReactionAt=0.,Threshold=0.,Worst=0.,ProgressAt=0.,Best=1.e30;
    FVector Start,Goal;FString Mode,Dir,Rows=TEXT("raw,id,cause,phase,x,y,z,rootErrorCm,planted\n");TArray<FString> Captures;
    TArray<double> Gaps,Drifts;TArray<FVector> Previous[2];int64 Cycles[2]={};uint8 Planted=0;bool SeenWater=false,SeenRecovery=false,Network=false,Passed=false;
};
FState S;
double P95(TArray<double> V){if(V.IsEmpty())return 1.e9;V.Sort();return V[FMath::Clamp(FMath::CeilToInt(V.Num()*.95)-1,0,V.Num()-1)];}
void Key(APlayerController* PC,FKey K,bool Down){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Down?IE_Pressed:IE_Released,Down?1.f:0.f));}
void Posture(APlayerController* PC,AStoryFirstPersonCharacter* Pawn)
{
    auto* M=Cast<UStoryMovementComponent>(Pawn->GetCharacterMovement());Key(PC,EKeys::LeftShift,false);
    if(M&&M->WantsToCrawl()!=(S.Mode==TEXT("crouch"))){Key(PC,EKeys::C,true);Key(PC,EKeys::C,false);}if(S.Mode==TEXT("sprint"))Key(PC,EKeys::LeftShift,true);
}
void Finish(bool Passed,const FString& Why)
{
    S.Stage=-1;for(const auto& File:S.Captures)Passed&=IFileManager::Get().FileSize(*File)>1000;
    const FString Result=FString::Printf(TEXT("%s scope=nativeGoosePlayerReaction mode=%s captures=%d expected=4 effectiveHissDistanceCm=%.3f expectedCm=600 toleranceCm=120 actualRoots=%d worstRootErrorCm=%.6f gapSamples=%d gapP95Cm=%.6f driftSamples=%d driftP95Cm=%.6f footLimitCm=3 enteredWater=%d returnedShore=%d ordinaryPlayerInput=1 noAnimalForcing=1 serverCheckHz=10 twoPeerAccepted=0 callsAccepted=0 wholePhase3Complete=0 reason=%s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),*S.Mode,S.Captures.Num(),S.Threshold,S.Roots,S.Worst,S.Gaps.Num(),P95(S.Gaps),S.Drifts.Num(),P95(S.Drifts),int32(S.SeenWater),int32(S.SeenRecovery),*Why);
    FFileHelper::SaveStringToFile(Result,*(S.Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(S.Rows,*(S.Dir/TEXT("reaction.csv")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_PLAYER_REVIEW %s"),*Result);
    if(S.Network){S.Passed=Passed;S.Stage=9;}else FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Capture(int32 Index)
{
    // Keep screenshot/readback work outside the half-second GPU response gate.
    if(S.Network&&S.World.IsValid()&&HouseCalderWildlife::ServerTime(S.World.Get())<S.ReactionAt+1.2)return;
    if(S.Captures.Num()!=Index||FScreenshotRequest::IsScreenshotRequested())return;
    const FString File=S.Dir/FString::Printf(TEXT("detail_goose_reaction_%02d.png"),Index);GetHighResScreenshotConfig().SetHDRCapture(false);FScreenshotRequest::RequestScreenshot(File,false,false);S.Captures.Add(File);
}
bool Floor(UWorld* W,AActor* Pawn,const FVector2D& P,double& Z)
{
    float Water=0.f;if(HouseCalderWater::WaterAt(P,Water))return false;const double Datum=HouseExteriorSite::GroundHeight(P.X,P.Y);FHitResult Hit;const FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderGoosePlayerFloor),true,Pawn);
    if(!W->LineTraceSingleByChannel(Hit,FVector(P,Datum+150.),FVector(P,Datum-150.),ECC_Visibility,Q)||FMath::Abs(Hit.ImpactPoint.Z-Datum)>3.||Hit.ImpactNormal.Z<.766)return false;Z=Hit.ImpactPoint.Z;return true;
}
bool Profile(UWorld* W,AStoryFirstPersonCharacter* Pawn,const FVector& A,const FVector& B)
{
    const double Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();const FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderGoosePlayerCapsule),true,Pawn);
    const int32 Count=FMath::CeilToInt(FVector::Dist2D(A,B)/25.);double Before=0.;
    for(int32 I=0;I<=Count;++I){const FVector P=FMath::Lerp(A,B,double(I)/Count);double Z=0.;if(!Floor(W,Pawn,FVector2D(P),Z)||(I&&FMath::Abs(Z-Before)>18.)||W->OverlapBlockingTestByChannel(FVector(P.X,P.Y,Z+Half+3.),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius-1.,Half-1.),Q))return false;Before=Z;}return true;
}
bool Sight(UWorld* W,AActor* Pawn,const FVector& From,const FVector& To)
{
    FHitResult Hit;const FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderGoosePlayerSight),true,Pawn);
    return !W->LineTraceSingleByChannel(Hit,From,To,ECC_Visibility,Q);
}
bool Setup(UWorld* W,APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* D)
{
    Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(FVector(0,0,12000),false,nullptr,ETeleportType::TeleportPhysics);
    const double Factor=S.Mode==TEXT("sprint")?2.:S.Mode==TEXT("crouch")?.6:1.;const FVector Bank=S.Shore.Routes[0].Points.Last();bool Found=false;
    for(int32 Try=0;Try<72&&!Found;++Try)
    {
        const FVector Direction=FRotator(0,90.+Try*5.,0).Vector();FVector A=Bank+Direction*(1800.*Factor),B=Bank+Direction*(450.*Factor),Near=Bank+Direction*(600.*Factor);double Z=0.,NearZ=0.;if(!Floor(W,Pawn,FVector2D(A),Z)||!Profile(W,Pawn,A,B)||!Floor(W,Pawn,FVector2D(Near),NearZ))continue;A.Z=Z;bool Clear=true;
        for(int32 Route=0;Route<2;++Route)Clear&=FVector::Dist2D(A,S.Shore.Routes[Route].Points.Last())>900.*Factor;
        // A clear capsule route can still look over a wall that hides the
        // goose's legs. Require clear bank/body/foot sightlines as well, from
        // a conservative70cm eye at both the trigger and retreat positions.
        Clear&=FVector::Dist2D(Near,S.Shore.Routes[1].Points.Last())>600.*Factor+100.;
        for(const FVector Eye:{FVector(A.X,A.Y,Z+70.),FVector(Near.X,Near.Y,NearZ+70.)})
        {
            for(double Height:{4.,35.,80.})Clear&=Sight(W,Pawn,Eye,FVector(Bank.X,Bank.Y,Bank.Z-27.2+Height));
            Clear&=Sight(W,Pawn,Eye,S.Pond.Homes[6]+FVector(0,0,35.));
        }
        if(!Clear)continue;S.Start=A;S.Goal=B;Found=true;
    }
    if(!Found)return false;Pawn->SetActorLocation(S.Start+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4.),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Posture(PC,Pawn);PC->SetViewTarget(Pawn);if(auto* HUD=PC->GetHUD())HUD->bShowHUD=false;
    S.SetupAt=HouseCalderWildlife::ServerTime(W);S.ProgressAt=S.SetupAt;D->SetTimingView(FName(*(TEXT("goose_player_")+S.Mode)));S.Stage=1;
    UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_PLAYER_SETUP mode=%s start=%s goal=%s ordinaryInput=1 setupTeleports=2 playerSpeedUnchanged=1 clearBankFeetBodySightlines=1 conservativeEyeHeightCm=70"),*S.Mode,*S.Start.ToString(),*S.Goal.ToString());return true;
}
void Tick(UWorld* W,ELevelTick,float)
{
    if(!W||!W->IsGameWorld()||!W->HasBegunPlay()||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionReview"))&&!(S.Network&&W->GetNetMode()==NM_ListenServer))return;
    const double Wall=FPlatformTime::Seconds();if(S.World.Get()!=W){S=FState();S.World=W;S.Began=Wall;S.Mode=TEXT("walk");FParse::Value(FCommandLine::Get(),TEXT("CalderGooseReactionMode="),S.Mode);S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderGooseReactionReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    if(S.Stage<0||S.Stage==9)return;if(Wall-S.Began>390.){Finish(false,TEXT("ordinary-input reaction/recovery timeout"));return;}
    APlayerController* PC=nullptr;for(FConstPlayerControllerIterator P=W->GetPlayerControllerIterator();P;++P)if(P->Get()->IsLocalController())PC=P->Get();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;TActorIterator<AHouseCalderWildlifeDirector> It(W);auto* D=It?*It:nullptr;if(!Pawn||!D||!D->Ready()||!D->PondReadyForReview())return;
    if(D->PondFailedForReview()){Finish(false,TEXT("runtime reaction data rejected"));return;}
    const double Now=HouseCalderWildlife::ServerTime(W),Raw=HouseCalderWildlife::CycleTime(W);
    if(S.Stage==0)
    {
        if(!S.Sky.Build(W)||!S.Pond.Build(W,S.Sky)||!S.Shore.Build(W,S.Sky,S.Pond)||S.Shore.Hash!=D->ShoreHashForReview()||!Setup(W,PC,Pawn,D)){Finish(false,TEXT("no physically clear initial approach/reference mismatch"));return;}return;
    }
    if(S.Stage==1)
    {
        PC->SetControlRotation((S.Shore.Routes[0].Points.Last()+FVector(0,0,45)-Pawn->GetPawnViewLocation()).Rotation());
        for(const auto& State:D->PondStates.Items)if(State.Cause==64&&State.StartTime>=S.SetupAt)
        {
            S.Id=State.Id;S.ReactionAt=State.StartTime;HouseCalderWildlifeGoosePaths::FCycleSample Bird;S.Pond.Cycle(0.,0,State.Id%8,Bird);const double Factor=S.Mode==TEXT("sprint")?2.:S.Mode==TEXT("crouch")?.6:1.;S.Threshold=FVector::Distance(FVector(State.From)+FVector(0,0,45.*Bird.Bird.Scale),Pawn->GetActorLocation())/Factor;
            if(FMath::Abs(S.Threshold-600.)>120.){Finish(false,TEXT("hiss trigger outside unchanged20percent tolerance"));return;}
            Pawn->GetCharacterMovement()->StopMovementImmediately();Key(PC,EKeys::LeftShift,false);S.Stage=2;break;
        }
        if(S.Stage==1)
        {
            if(Now-S.SetupAt>20.){Finish(false,TEXT("no hiss from ordinary player approach"));return;}
            Pawn->AddMovementInput((S.Goal-Pawn->GetActorLocation()).GetSafeNormal2D(),1.);return;
        }
    }
    const auto* State=D->PondStates.Items.FindByPredicate([](const auto& X){return X.Id==S.Id;});if(!State){Finish(false,TEXT("missing reaction state"));return;}
    FTransform Root;bool Visible=false;int32 Phase=0;double Rendered=-1.;if(!D->ReadPondForReview(S.Id,Root,Visible,Phase,Rendered)||!Visible){Finish(false,TEXT("missing visible actual reacting goose"));return;}
    HouseCalderWildlifeGoosePaths::FCycleSample Expected;
    if(State->Cause>=64)
    {
        HouseCalderWildlifeGooseShore::FReaction R;R.Mode=State->Cause;R.Start=double(State->StartTime)+HouseCalderWildlife::CycleReviewOffset();double Error=0.;S.Shore.Project(S.Id%8-6,FVector(State->From),R.Arc,Error);R.Yaw=FRotator::DecompressAxisFromShort(State->FacingYaw);R.Expiry=S.Shore.HomeDeadline(S.Pond,R.Start,S.Id/8);
        if(Rendered<R.Expiry)S.Shore.Reaction(S.Pond,Rendered,S.Id%8,R,Expected);else S.Shore.Cycle(S.Pond,Rendered,S.Id/8,S.Id%8,Expected);
    }
    else S.Shore.Cycle(S.Pond,Rendered,S.Id/8,S.Id%8,Expected);
    const double Error=FVector::Distance(Root.GetTranslation(),Expected.Bird.Position);S.Worst=FMath::Max(S.Worst,Error);++S.Roots;if(Error>.2){Finish(false,TEXT("rendered reacting root differs from replicated path"));return;}
    PC->SetControlRotation((Root.GetTranslation()+FVector(0,0,45)-Pawn->GetPawnViewLocation()).Rotation());
    FCalderGooseSoleProbe Probe;if(!D->ReadPondSolesForReview(S.Id,Probe)){Finish(false,TEXT("submitted reaction sole probe missing"));return;}
    if(Probe.Stand>.999&&Expected.WalkAge>=0.)
    {
        S.Gaps.Append(Probe.Gaps);for(int32 Leg=0;Leg<2;++Leg)
        {if((Probe.PlantedMask&S.Planted&(1<<Leg))&&Probe.Cycles[Leg]==S.Cycles[Leg]&&Probe.Points[Leg].Num()==S.Previous[Leg].Num())for(int32 V=0;V<Probe.Points[Leg].Num();++V)S.Drifts.Add(FVector::Distance(Probe.Points[Leg][V],S.Previous[Leg][V]));S.Previous[Leg]=Probe.Points[Leg];S.Cycles[Leg]=Probe.Cycles[Leg];}S.Planted=Probe.PlantedMask;
    }
    else S.Planted=0;
    S.Rows+=FString::Printf(TEXT("%.6f,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%d\n"),Rendered,S.Id,int32(State->Cause),Phase,Root.GetTranslation().X,Root.GetTranslation().Y,Root.GetTranslation().Z,Error,int32(Probe.PlantedMask));
    const double Age=Now-S.ReactionAt;if(Age>=.15)Capture(0);if(Age>=2.)Capture(1);
    if(Age>3.&&S.Stage==2){S.Stage=3;S.ProgressAt=Now;S.Best=1.e30;}
    if(S.Stage==3)
    {
        const double Distance=FVector::Dist2D(Pawn->GetActorLocation(),S.Start);
        if(Distance<80.){Pawn->GetCharacterMovement()->StopMovementImmediately();S.Stage=4;}
        else{Pawn->AddMovementInput((S.Start-Pawn->GetActorLocation()).GetSafeNormal2D(),1.);if(Distance<S.Best-5.){S.Best=Distance;S.ProgressAt=Now;}if(Now-S.ProgressAt>8.){Finish(false,TEXT("ordinary retreat blocked"));return;}}
    }
    if(Phase==11){S.SeenWater=true;Capture(2);}
    if(State->Cause==65)S.SeenRecovery=true;
    if(S.Mode!=TEXT("walk")&&S.SeenWater&&Age>3.)Capture(3);
    if(S.Mode==TEXT("walk")&&S.SeenRecovery&&State->Cause<64&&Phase==6)Capture(3);
    if(S.Captures.Num()==4&&!FScreenshotRequest::IsScreenshotRequested()&&Age>5.)Finish(S.SeenWater&&(S.Mode!=TEXT("walk")||S.SeenRecovery)&&S.Gaps.Num()>=60&&S.Drifts.Num()>=30&&P95(S.Gaps)<=3.&&P95(S.Drifts)<=3.,TEXT("native ordinary-input files produced; inspect all4images;two-peer/audio/performance remain"));
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
namespace HouseCalderWildlifeGooseReactionReview
{
bool BeginNetwork(UWorld* World,APlayerController* PC,AStoryFirstPersonCharacter* Pawn,AHouseCalderWildlifeDirector* Director)
{
    namespace R=HouseCalderWildlifeGooseReactionReviewImpl;R::S=R::FState();R::S.World=World;R::S.Network=true;R::S.Mode=TEXT("walk");R::S.Began=FPlatformTime::Seconds();R::S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderGooseReactionReview");IFileManager::Get().MakeDirectory(*R::S.Dir,true);
    return R::S.Sky.Build(World)&&R::S.Pond.Build(World,R::S.Sky)&&R::S.Shore.Build(World,R::S.Sky,R::S.Pond)&&R::S.Shore.Hash==Director->ShoreHashForReview()&&R::Setup(World,PC,Pawn,Director);
}
bool NetworkDone(bool& Passed,int32& Id){namespace R=HouseCalderWildlifeGooseReactionReviewImpl;Id=R::S.Id;Passed=R::S.Passed;return R::S.Network&&R::S.Stage==9;}
int32 NetworkFocal(){namespace R=HouseCalderWildlifeGooseReactionReviewImpl;return R::S.Network?R::S.Id:-1;}
}
#endif
