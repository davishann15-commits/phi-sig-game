// Graph-data review only; does not accept ambient flight/render/cadence/audio.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeSkyPaths.h"
#include "HouseCalderWildlifeGoosePaths.h"
#include "HouseCalderWildlifeGooseShore.h"
#include "HouseCalderWater.h"
#include "HouseCalderTerrain.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace HouseCalderWildlifeSkyPathsReviewImpl
{
TWeakObjectPtr<UWorld> LastWorld;bool Done=false;
void Tick(UWorld* World,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSkyPathsReview")) || !World || !World->IsGameWorld() || !World->HasBegunPlay() || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    if(LastWorld.Get()!=World){LastWorld=World;Done=false;}if(Done)return;
    TActorIterator<AHouseCalderWildlifeDirector> It(World);if((!It || !It->Ready()) && World->GetTimeSeconds()<100.)return;Done=true;
    HouseCalderWildlifeSkyPaths::FPlan Plan;const double Began=FPlatformTime::Seconds();bool Passed=It && It->Ready() && Plan.Build(World);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifeSkyPathsReview");IFileManager::Get().MakeDirectory(*Dir,true);
    if(FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreDataReview")))
    {
        HouseCalderWildlifeGoosePaths::FPlan Pond;HouseCalderWildlifeGooseShore::FPlan Shore;Passed=Passed&&Pond.Build(World,Plan)&&Shore.Build(World,Plan,Pond);
        FString Rows=TEXT("route,point,x,y,z,arcCm,terrainZ,wet,waterZ\n");
        for(int32 Route=0;Route<2;++Route)for(int32 I=0;I<Shore.Routes[Route].Points.Num();++I)
        {const FVector P=Shore.Routes[Route].Points[I];float Water=0.f;const bool Wet=HouseCalderWater::WaterAt(FVector2D(P),Water);Rows+=FString::Printf(TEXT("%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f\n"),Route,I,P.X,P.Y,P.Z,Shore.Routes[Route].Arc[I],double(HouseCalderTerrain::SurfaceZ(P.X,P.Y)),int32(Wet),double(Water));}
        FFileHelper::SaveStringToFile(Rows,*(Dir/TEXT("goose_shore.csv")));
        int32 MinWater=100,MaxWater=0,ClockFailures=0,ClockSamples=0;double Closest=1.e20,Seam=0.;
        if(Passed)
        {
            for(double Raw=0.;Raw<3600.;Raw+=.05)
            {
                int32 WaterCount=0;TArray<FVector,TInlineAllocator<16>> Positions;
                for(int32 C=0;C<2;++C)for(int32 M=0;M<8;++M){HouseCalderWildlifeGoosePaths::FCycleSample Bird;if(!Shore.Cycle(Pond,Raw,C,M,Bird))continue;WaterCount+=int32(Bird.OnWater);Positions.Add(Bird.Bird.Position);}
                MinWater=FMath::Min(MinWater,WaterCount);MaxWater=FMath::Max(MaxWater,WaterCount);++ClockSamples;if(WaterCount<6||WaterCount>12)++ClockFailures;
                for(int32 A=0;A<Positions.Num();++A)for(int32 B=A+1;B<Positions.Num();++B)Closest=FMath::Min(Closest,FVector::Distance(Positions[A],Positions[B]));
            }
            for(int32 C=0;C<2;++C)for(int32 M=0;M<8;++M){HouseCalderWildlifeGoosePaths::FCycleSample A,B;const bool Before=Shore.Cycle(Pond,1800.-.0001,C,M,A),After=Shore.Cycle(Pond,1800.,C,M,B);if(Before!=After)++ClockFailures;else if(Before)Seam=FMath::Max(Seam,FVector::Distance(A.Bird.Position,B.Bird.Position));}
            Passed&=ClockFailures==0&&Closest>=120.&&Seam<.1;
        }
        const FString Cycle=FString::Printf(TEXT("%s scope=gooseShoreRoutineData samples=%d failures=%d waterMin=%d waterMax=%d pairMinimumCm=%.6f positionSeamCm=%.6f nativeFeetAccepted=0 serverReactionAcceptance=0\n"),Passed?TEXT("PASS"):TEXT("FAIL"),ClockSamples,ClockFailures,MinWater,MaxWater,Closest,Seam);
        FFileHelper::SaveStringToFile(Cycle,*(Dir/TEXT("goose_shore_cycle.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_SHORE_CYCLE %s"),*Cycle);
        int32 ReactionSamples=0,ReactionFailures=0;double ProjectionError=0.,ReactionSeam=0.;
        if(Passed)for(int32 Route=0;Route<2;++Route)for(int32 Mode:{64,65})
        {
            HouseCalderWildlifeGooseShore::FReaction R;R.Mode=uint8(Mode);R.Start=300.;R.Arc=Mode==64?Shore.Routes[Route].Length:0.;
            HouseCalderWildlifeGoosePaths::FCycleSample Initial;Pond.Cycle(0.,0,Route+6,Initial);Shore.At(Route,R.Arc,Mode==65,Initial);R.Yaw=Initial.Bird.Direction.Rotation().Yaw;
            for(double Age=0.;Age<200.;Age+=.02)
            {
                HouseCalderWildlifeGoosePaths::FCycleSample Bird;double Arc=0.,Error=0.;++ReactionSamples;
                if(!Shore.Reaction(Pond,R.Start+Age,Route+6,R,Bird)||!Shore.Project(Route,Bird.Bird.Position,Arc,Error)){++ReactionFailures;continue;}
                ProjectionError=FMath::Max(ProjectionError,Error);if(Error>.1||Arc<-.1||Arc>Shore.Routes[Route].Length+.1||Bird.Bird.Position.ContainsNaN()||Bird.Mouth<0.||Bird.Mouth>1.)++ReactionFailures;
                if(Age>150.&&Mode==64&&FVector2D::Distance(FVector2D(Bird.Bird.Position),FVector2D(Pond.Homes[Route+6]))>.1)++ReactionFailures;
            }
            const double Hold=Mode==64?.8:2.5,End=Hold+Shore.Routes[Route].Length/35.;
            for(double Boundary:{Hold,End,End+2.})
            {HouseCalderWildlifeGoosePaths::FCycleSample A,B;Shore.Reaction(Pond,R.Start+Boundary-.0001,Route+6,R,A);Shore.Reaction(Pond,R.Start+Boundary,Route+6,R,B);ReactionSeam=FMath::Max(ReactionSeam,FVector::Distance(A.Bird.Position,B.Bird.Position));}
        }
        Passed&=ReactionSamples>=40000&&ReactionFailures==0&&ProjectionError<=.1&&ReactionSeam<.1;
        const FString Reaction=FString::Printf(TEXT("%s scope=gooseReactionPurePathData samples=%d failures=%d maximumRouteProjectionErrorCm=%.6f maximumPositionSeamCm=%.6f actualPlayerTriggerAccepted=0 replicatedReactionsAccepted=0 movingFeetAccepted=0\n"),Passed?TEXT("PASS"):TEXT("FAIL"),ReactionSamples,ReactionFailures,ProjectionError,ReactionSeam);
        FFileHelper::SaveStringToFile(Reaction,*(Dir/TEXT("goose_reaction_cycle.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_DATA %s"),*Reaction);
        const FString Result=FString::Printf(TEXT("%s scope=gooseShoreDataOnly nodes=%d samples=%d rejects=%d workTriangles=%d routes=2 hash=%08x terrain=%08x physicsQueries=0 runtimeReactionsAcceptance=0 nativeRenderAcceptance=0 wholePhase3Complete=0 buildMs=%.2f\n"),Passed?TEXT("PASS"):TEXT("FAIL"),Shore.Nodes,Shore.Samples,Shore.Rejected,Shore.WaterWorks.Num(),Shore.Hash,HouseCalderTerrain::Hash(),(FPlatformTime::Seconds()-Began)*1000.);
        FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_SHORE_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);return;
    }
    const bool Landing=FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePathsReview"));
    if(Landing)
    {
        HouseCalderWildlifeGoosePaths::FPlan Geese;Passed=Passed && Geese.Build(World,Plan);
        FString Rows=TEXT("role,member,point,x,y,z,arcCm\n"),Motion=TEXT("member,scene,x,y,z,splitErrorCm,startSpeedCmS,endSpeedCmS\n");double WorstSplit=0.;
        for(int32 Member=0;Member<HouseCalderWildlifeGoosePaths::LandingCount && !Geese.Touches.IsEmpty();++Member)
        {
            for(int32 Role=0;Role<2;++Role)
            {
                const auto& Curve=Role==0?Geese.Arrivals[Member]:Geese.Departures[Member];
                for(int32 Point=0;Point<Curve.Points.Num();++Point){const FVector& P=Curve.Points[Point];Rows+=FString::Printf(TEXT("%s,%d,%d,%.6f,%.6f,%.6f,%.6f\n"),Role==0?TEXT("arrival"):TEXT("departure"),Member,Point,P.X,P.Y,P.Z,Curve.Arc[Point]);}
            }
            HouseCalderWildlifeSkyPaths::FSample From,Start,End,After,BeforeEnd;const double Duration=Geese.Durations[Member];
            const bool Good=Plan.EvaluateGoose(Geese.SplitScene,1,22+Member,From) && Geese.Arrival(Geese.SplitScene,Member,Start)
                && Geese.Arrival(Geese.SplitScene+Duration,Member,End) && Geese.Arrival(Geese.SplitScene+.001,Member,After)
                && Geese.Arrival(Geese.SplitScene+Duration-.001,Member,BeforeEnd);
            if(!Good){Passed=false;continue;}
            const double Split=FVector::Distance(From.Position,Start.Position),Speed0=FVector::Distance(Start.Position,After.Position)/.001,Speed1=FVector::Distance(End.Position,BeforeEnd.Position)/.001;
            WorstSplit=FMath::Max(WorstSplit,Split);Passed&=Split<=.1 && FMath::Abs(Speed0-Geese.StartSpeeds[Member])<20. && FMath::Abs(Speed1-250.)<5.;
            for(double Age=0.;Age<Duration+.05;Age+=.05)
            {HouseCalderWildlifeSkyPaths::FSample P;if(!Geese.Arrival(Geese.SplitScene+FMath::Min(Age,Duration),Member,P)){Passed=false;continue;}Motion+=FString::Printf(TEXT("%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),Member,Geese.SplitScene+FMath::Min(Age,Duration),P.Position.X,P.Position.Y,P.Position.Z,Split,Speed0,Speed1);}
        }
        int32 MinPond=100,MaxPond=0,ClockSamples=0,ClockFailures=0;double PairMinimum=1.e20,WorstSeam=0.;
        if(Geese.Homes.Num()==8)
        {
            for(double Scene=0.;Scene<3600.;Scene+=.05)
            {
                int32 OnPond=0;TArray<FVector,TInlineAllocator<16>> Positions;
                for(int32 Cohort=0;Cohort<2;++Cohort)for(int32 Member=0;Member<8;++Member)
                {HouseCalderWildlifeGoosePaths::FCycleSample P;if(!Geese.Cycle(Scene,Cohort,Member,P))continue;OnPond+=int32(P.OnWater);Positions.Add(P.Bird.Position);}
                MinPond=FMath::Min(MinPond,OnPond);MaxPond=FMath::Max(MaxPond,OnPond);++ClockSamples;if(OnPond<6 || OnPond>12)++ClockFailures;
                for(int32 A=0;A<Positions.Num();++A)for(int32 B=A+1;B<Positions.Num();++B)PairMinimum=FMath::Min(PairMinimum,FVector::Distance(Positions[A],Positions[B]));
            }
            for(int32 Cohort=0;Cohort<2;++Cohort)for(int32 Member=0;Member<8;++Member)
            {HouseCalderWildlifeGoosePaths::FCycleSample A,B;const bool Before=Geese.Cycle(1800.-.0001,Cohort,Member,A),After=Geese.Cycle(1800.,Cohort,Member,B);if(Before!=After)++ClockFailures;else if(Before)WorstSeam=FMath::Max(WorstSeam,FVector::Distance(A.Bird.Position,B.Bird.Position));}
            Passed&=ClockFailures==0 && PairMinimum>=120. && WorstSeam<.1;
        }
        else Passed=false;
        const FString CycleResult=FString::Printf(TEXT("%s scope=gooseBackgroundCycleData samples=%d failures=%d pondMin=%d pondMax=%d pairMinimumCm=%.6f seamErrorCm=%.6f serverReactionAcceptance=0 nativeRenderAcceptance=0\n"),Passed?TEXT("PASS"):TEXT("FAIL"),ClockSamples,ClockFailures,MinPond,MaxPond,PairMinimum,WorstSeam);
        FFileHelper::SaveStringToFile(CycleResult,*(Dir/TEXT("goose_cycle.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_CYCLE_DATA %s"),*CycleResult);
        FFileHelper::SaveStringToFile(Rows,*(Dir/TEXT("goose_curves.csv")));FFileHelper::SaveStringToFile(Motion,*(Dir/TEXT("goose_motion.csv")));
        const FString Result=FString::Printf(TEXT("%s scope=gooseLandingDataOnly samples=%d rejects=%d hash=%08x terrain=%08x splitScene=%.6f worstSplitErrorCm=%.6f landingCount=8 restContinue=22 physicsQueries=0 nativeRenderAcceptance=0 serverHandoffAcceptance=0 wholePhase3Complete=0 buildMs=%.2f\n"),Passed?TEXT("PASS"):TEXT("FAIL"),Geese.Samples,Geese.Rejected,Geese.Hash,HouseCalderTerrain::Hash(),Geese.SplitScene,WorstSplit,(FPlatformTime::Seconds()-Began)*1000.);
        FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_PATHS_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);return;
    }
    const FString Result=FString::Printf(TEXT("%s scope=skyGraphDataOnly clearSamples=%d hash=%08x terrain=%08x physicsQueries=0 modelRenderAcceptance=0 ambientFlightAcceptance=0 landingPending=1 wholePhase3Complete=0 buildMs=%.2f\n"),Passed?TEXT("PASS"):TEXT("FAIL"),Plan.Samples,Plan.Hash,HouseCalderTerrain::Hash(),(FPlatformTime::Seconds()-Began)*1000.);
    FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_PATHS_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
