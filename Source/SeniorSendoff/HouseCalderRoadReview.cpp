// Opt-in physical proof for Ashby Lane's new approaches, bridge and turnaround.
// Existing reviews, movement settings, waypoints and limits are unchanged.
#if !UE_BUILD_SHIPPING
#include "HouseCalderLandform.h"
#include "HouseCalderRoads.h"
#include "HouseCalderTerrain.h"
#include "HouseCalderWater.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryCampaign.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderRoadReviewImpl
{
struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AStoryFirstPersonCharacter> Walker;
    TArray<FVector2D> Goals;
    int32 Phase = 0, Goal = 0;
    double Start = 0., LegStart = 0., Progress = 0.;
    float Best = MAX_flt;
    FString Dir, Report;
};
FState& State() { static FState S; return S; }
void Note(FState& S, const FString& Line)
{
    S.Report += Line + TEXT("\n");
    UE_LOG(LogTemp, Display, TEXT("CALDER_ROAD_REVIEW %s"), *Line);
}
void Finish(FState& S, bool bPass, const FString& Why)
{
    if (S.Walker.IsValid()) S.Walker->GetCharacterMovement()->StopMovementImmediately();
    S.Phase = -1;
    Note(S, FString::Printf(TEXT("%s reached=%d/%d hash=%08x %s"), bPass ? TEXT("PASS") : TEXT("FAIL"),
        S.Goal, S.Goals.Num(), HouseCalderTerrain::Hash(), *Why));
    FFileHelper::SaveStringToFile(S.Report, *(S.Dir / TEXT("result.txt")));
    FPlatformMisc::RequestExitWithStatus(false, bPass ? 0 : 1);
}
bool Floor(UWorld* W, AActor* Ignore, const FVector2D& P, float Expected, float& Z,FHitResult* OutHit=nullptr)
{
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderRoadReview), true);
    Q.AddIgnoredActor(Ignore);
    FHitResult Hit;
    if (!W->LineTraceSingleByChannel(Hit, FVector(P, Expected + 40.f), FVector(P, Expected - 500.f), ECC_Visibility, Q)) return false;
    Z = Hit.ImpactPoint.Z;
    if(OutHit) *OutHit=Hit;
    return Hit.ImpactNormal.Z >= .76604444f;
}
float ExpectedAt(const FVector2D& P)
{
    float Z = HouseCalderTerrain::SurfaceZ(P.X, P.Y);
    if (!HouseCalderRoads::FloorAt(P, Z)) HouseCalderWater::DeckAt(P, Z);
    return Z;
}
float ExpectedBridgeTop(const HouseCalderLandform::FWaterCrossing& Bridge,double Along,double Across,bool bVerge)
{
    const double Half=Bridge.Width*.5;
    const float Centre=bVerge?Bridge.DeckZ-float(.02*(Half-45.))+12.f:Bridge.DeckZ-float(.02*FMath::Abs(Across));
    const FVector2D Axis=Bridge.Axis.GetSafeNormal(),Side(-Axis.Y,Axis.X);
    float Result=Centre;
    for(const double End:{-double(Bridge.Back),double(Bridge.On)})
    {
        const double Weight=1.-FMath::SmoothStep(0.,300.,FMath::Abs(Along-End));
        if(Weight<=0.) continue;
        float Target=-MAX_flt;
        if(bVerge)
        {
            for(const double Edge:{End-3.,End,End+3.})
              for(const double L:{Half-45.,Half-22.5,Half})
            {
                const FVector2D P=Bridge.Centre+Axis*Edge+Side*(Across<0.?-L:L);
                Target=FMath::Max(Target,HouseExteriorSite::GroundHeight(P.X,P.Y));
            }
        }
        else
        {
            const FVector2D P=Bridge.Centre+Axis*(End+(End<0.?-.1:.1))+Side*Across;
            Target=HouseExteriorSite::GroundHeight(P.X,P.Y)-12.f;
        }
        Result+=float(Weight)*(Target-Centre);
    }
    return Result;
}
bool Profiles(UWorld* W, AActor* Ignore, FState& S)
{
    int32 Samples = 0, ExpectedSamples = 0, Bad = 0;
    float WorstError = 0.f;
    double WorstGrade = 0.;
    FString Csv=TEXT("road,across,x,y,expected,hit,walkable,component,gradePct\n");
    for (const HouseCalderLandform::FRoad& Road : HouseCalderLandform::Roads())
    {
        if (Road.Mode == 2) continue;
        TArray<double> Run;
        Run.Add(0.);
        for (int32 I = 1; I < Road.Points.Num(); ++I) Run.Add(Run.Last() + (Road.Points[I] - Road.Points[I-1]).Size());
        for (const double Across : { -.4 * Road.Width, 0., .4 * Road.Width })
        {
            bool bPrevious = false;
            FVector Previous;
            for (int32 I = 1; I < Road.Points.Num(); ++I)
            {
                const FVector2D A = Road.Points[I-1], B = Road.Points[I], Direction = (B-A).GetSafeNormal(), Side(-Direction.Y,Direction.X);
                const double Length = (B-A).Size();
                const int32 Steps = FMath::Max(1,FMath::CeilToInt(Length/10.));
                ExpectedSamples += Steps + 1;
                for (int32 K = 0; K <= Steps; ++K)
                {
                    const double T = double(K)/Steps, Along = FMath::Lerp(Run[I-1],Run[I],T);
                    const FVector2D P = FMath::Lerp(A,B,T) + Side*Across;
                    double Offset = 3.-15.*(1.-FMath::SmoothStep(0.,300.,Along));
                    if (Road.Mode == 0) Offset -= 15.*FMath::SmoothStep(Run.Last()-300.,Run.Last(),Along);
                    FVector2D Datum=P;
                    if(Along<.001) Datum+=Direction*.1;
                    else if(Along>Run.Last()-.001) Datum-=Direction*.1;
                    const float Expected = HouseExteriorSite::GroundHeight(Datum.X,Datum.Y)+float(Offset);
                    float Z = 0.f;
                    FHitResult ProfileHit;
                    const bool bFloor = Floor(W,Ignore,P,Expected,Z,&ProfileHit);
                    double Grade=0.;
                    ++Samples;
                    const float Error = bFloor ? FMath::Abs(Z-Expected) : MAX_flt;
                    if (bFloor) WorstError = FMath::Max(WorstError,Error);
                    if (!bFloor || Error > 1.f)
                    {
                        if (Bad++ < 8) Note(S,FString::Printf(TEXT("PROFILE_FAIL road=%s at=(%.2f,%.2f) expected=%.3f actual=%.3f floor=%d"),*Road.Name,P.X,P.Y,Expected,Z,int32(bFloor)));
                    }
                    const FVector Current(P,Z);
                    if (bFloor && bPrevious && FVector::Dist2D(Current,Previous) > 1.)
                    {
                        Grade = FMath::Abs(Current.Z-Previous.Z)/FVector::Dist2D(Current,Previous)*100.;
                        WorstGrade = FMath::Max(WorstGrade,Grade);
                        if (Grade > 12. && Bad++ < 8) Note(S,FString::Printf(TEXT("PROFILE_GRADE_FAIL road=%s at=(%.2f,%.2f) gradePct=%.3f"),*Road.Name,P.X,P.Y,Grade));
                    }
                    Csv+=FString::Printf(TEXT("%s,%.1f,%.3f,%.3f,%.3f,%.3f,%d,%s,%.3f\n"),*Road.Name,Across,P.X,P.Y,Expected,Z,int32(bFloor),*GetNameSafe(ProfileHit.GetComponent()),Grade);
                    Previous = Current; bPrevious = bFloor;
                }
            }
        }
    }
    FFileHelper::SaveStringToFile(Csv,*(S.Dir/TEXT("road_profiles.csv")));
    int32 DeckSamples = 0, UnderSamples = 0, ParapetSamples = 0;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CalderRoadBridgeReview),true);
    Q.AddIgnoredActor(Ignore);
    for (const HouseCalderLandform::FWaterCrossing& Bridge : HouseCalderLandform::WaterCrossings())
    {
        if (Bridge.Kind != HouseCalderLandform::EWaterCrossing::RoadBridge) continue;
        const FVector2D Axis = Bridge.Axis.GetSafeNormal(), Side(-Axis.Y,Axis.X);
        for (double Along = -Bridge.Back; Along <= Bridge.On; Along += 10.)
            for (const double Across : {-220.,0.,220.})
            {
                const FVector2D P = Bridge.Centre + Axis*Along + Side*Across;
                const float Expected = ExpectedBridgeTop(Bridge,Along,Across,false);
                float Z = 0.f;
                ++DeckSamples;
                if (!Floor(W,Ignore,P,Expected,Z) || FMath::Abs(Z-Expected) > 1.f) ++Bad;
            }
        for (double Along = -200.; Along <= 200.; Along += 20.)
            for (double Across = -300.; Across <= 300.; Across += 20.)
            {
                const FVector2D P = Bridge.Centre+Axis*Along+Side*Across;
                const float Ground = HouseExteriorSite::GroundHeight(P.X,P.Y);
                float Z = 0.f;
                FHitResult Hit;
                ++UnderSamples;
                if (!Floor(W,Ignore,P,Ground,Z) || FMath::Abs(Z-Ground)>1.f
                    || W->LineTraceSingleByChannel(Hit,FVector(P,Ground+2.f),FVector(P,Ground+200.f),ECC_Visibility,Q)) ++Bad;
            }
        for (double Along = -Bridge.Back+50.; Along <= Bridge.On-50.; Along += 50.)
            for (const double Sign : {-1.,1.})
            {
                const FVector2D A = Bridge.Centre+Axis*Along+Side*(Sign*270.), B = Bridge.Centre+Axis*Along+Side*(Sign*500.);
                FHitResult Hit;
                ++ParapetSamples;
                if (!W->LineTraceSingleByChannel(Hit,FVector(A,Bridge.DeckZ+60.f),FVector(B,Bridge.DeckZ+60.f),ECC_Pawn,Q)) ++Bad;
                const FVector2D Coping = Bridge.Centre+Axis*Along+Side*(Sign*(Bridge.Width*.5+20.));
                const float Top = ExpectedBridgeTop(Bridge,Along,Sign*(Bridge.Width*.5+20.),true)+107.f;
                float Z = 0.f;
                if (!Floor(W,Ignore,Coping,Top,Z) || FMath::Abs(Z-Top)>1.f) ++Bad;
            }
    }
    const bool bPass = Samples == ExpectedSamples && Samples >= 6000 && DeckSamples == 603 && UnderSamples == 651 && ParapetSamples == 78 && Bad == 0;
    Note(S,FString::Printf(TEXT("PROFILES %s pavingSamples=%d deckSamples=%d underSamples=%d parapetSamples=%d bad=%d worstPavingCm=%.3f maxGradePct=%.3f"),
        bPass?TEXT("PASS"):TEXT("FAIL"),Samples,DeckSamples,UnderSamples,ParapetSamples,Bad,WorstError,WorstGrade));
    return bPass;
}
void AppendRoute(const TArray<FVector2D>& Points,TArray<FVector2D>& Out)
{
    for (int32 I=1;I<Points.Num();++I)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt((Points[I]-Points[I-1]).Size()/200.));
        for(int32 K=1;K<=Steps;++K) Out.Add(FMath::Lerp(Points[I-1],Points[I],double(K)/Steps));
    }
}
void Tick(UWorld* W,ELevelTick,float)
{
    if (!W || !W->IsGameWorld() || !FParse::Param(FCommandLine::Get(),TEXT("CalderRoadReview"))
        || UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House")) return;
    FState& S=State(); const double Now=W->GetTimeSeconds();
    if(S.World.Get()!=W) { S=FState(); S.World=W; S.Start=Now; }
    if(S.Phase<0) return;
    APlayerController* PC=W->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if(!Pawn || Now-S.Start<8. || !HouseCalderTerrain::CollisionReady(W)) return;
    S.Walker=Pawn;
    if(S.Phase==0)
    {
        S.Dir=FPaths::ProjectSavedDir()/TEXT("CalderRoadReview");
        IFileManager::Get().MakeDirectory(*S.Dir,true);
        if(!HouseCalderRoads::Active() || !Profiles(W,Pawn,S)) { Finish(S,false,TEXT("physical profiles failed")); return; }
        const auto& Roads=HouseCalderLandform::Roads();
        const FVector2D Start=Roads[0].Points[0];
        AppendRoute(Roads[0].Points,S.Goals);
        S.Goals.Add(Roads[1].Points[0]);
        AppendRoute(Roads[1].Points,S.Goals);
        const TArray<FVector2D> Forward=S.Goals;
        for(int32 I=Forward.Num()-2;I>=0;--I) S.Goals.Add(Forward[I]);
        S.Goals.Add(Start);
        float Z=0.f;
        if(!Floor(W,Pawn,Start,ExpectedAt(Start),Z)) { Finish(S,false,TEXT("no start floor")); return; }
        Pawn->SetActorLocation(FVector(Start,Z+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+4.f),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        S.Phase=1;
    }
    if(S.Phase==1)
    {
        S.Best=MAX_flt; S.LegStart=S.Progress=Now; S.Phase=2;
        return;
    }
    const FVector2D Target=S.Goals[S.Goal];
    const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const float Distance=FVector2D::Distance(FVector2D(Feet),Target);
    if(Distance<S.Best-5.f) { S.Best=Distance; S.Progress=Now; }
    if(Now-S.Progress>8. || Now-S.LegStart>30.) { Finish(S,false,FString::Printf(TEXT("walking stalled feet=%s distance=%.1f"),*Feet.ToString(),Distance)); return; }
    PC->SetControlRotation(FRotator(-5.f,(FVector(Target,Feet.Z)-Feet).Rotation().Yaw,0.f));
    if(Distance>35.f || FMath::Abs(Feet.Z-ExpectedAt(Target))>8.f) return;
    if(++S.Goal==S.Goals.Num()) { Finish(S,true,TEXT("ordinary movement input; approaches and bridge both directions")); return; }
    if(S.Goal%100==0) Note(S,FString::Printf(TEXT("WALK reached=%d/%d feet=%s"),S.Goal,S.Goals.Num(),*Feet.ToString()));
    S.Phase=1;
}
struct FRegistration
{
    FRegistration()
    {
        FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);
        FCoreDelegates::OnBeginFrame.AddLambda([]
        {
            FState& S=State();
            if(S.Phase==2 && S.Walker.IsValid())
                S.Walker->AddMovementInput((FVector(S.Goals[S.Goal],0)-S.Walker->GetActorLocation()).GetSafeNormal2D(),1.f);
        });
    }
};
FRegistration Registration;
}
#endif
