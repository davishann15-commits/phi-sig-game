// Actual-world route data only. This does not enable or accept ambient crows.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifeCrowRoost.h"
#include "HouseCalderTerrain.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
namespace HouseCalderWildlifeCrowRoostReviewImpl
{
TWeakObjectPtr<UWorld> Last;bool Done=false;
void Tick(UWorld* W,ELevelTick,float)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("CalderCrowRoostDataReview"))||!W||!W->IsGameWorld()||!W->HasBegunPlay()||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    if(Last.Get()!=W){Last=W;Done=false;}if(Done)return;
    TActorIterator<AHouseCalderWildlifeDirector> It(W);if((!It||!It->Ready())&&W->GetTimeSeconds()<100.)return;Done=true;
    const double Began=FPlatformTime::Seconds();HouseCalderWildlifeSkyPaths::FPlan Sky;HouseCalderWildlifeCrowRoost::FPlan Plan;
    bool Passed=It&&It->Ready()&&Sky.Build(W)&&Plan.Build(W,Sky);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderCrowRoostReview");IFileManager::Get().MakeDirectory(*Dir,true);
    FString Trees=TEXT("batch,instance,x,y,z,minx,miny,minz,maxx,maxy,maxz\n"),Targets=TEXT("member,batch,instance,x,y,z,insideTreeBounds\n"),Routes=TEXT("member,point,x,y,z,arcCm\n"),Cadence=TEXT("event,startSeconds,count,lastArrivalSeconds\n");
    for(const auto& T:Plan.Trees){const FVector P=T.Transform.GetTranslation();Trees+=FString::Printf(TEXT("%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),*T.Batch,T.Instance,P.X,P.Y,P.Z,T.Canopy.Min.X,T.Canopy.Min.Y,T.Canopy.Min.Z,T.Canopy.Max.X,T.Canopy.Max.Y,T.Canopy.Max.Z);}
    for(int32 M=0;M<Plan.Targets.Num();++M)
    {
        const auto& T=Plan.Targets[M];Targets+=FString::Printf(TEXT("%d,%s,%d,%.6f,%.6f,%.6f,%d\n"),M,*T.Batch,T.Instance,T.End.X,T.End.Y,T.End.Z,int32(T.Canopy.IsInsideOrOn(T.End)));
        for(int32 I=0;I<Plan.Routes[M].Points.Num();++I){const FVector P=Plan.Routes[M].Points[I];Routes+=FString::Printf(TEXT("%d,%d,%.6f,%.6f,%.6f,%.6f\n"),M,I,P.X,P.Y,P.Z,Plan.Routes[M].Arc[I]);}
    }
    int32 ClockSamples=0,Failures=0,MinCount=100,MaxCount=0;double Seam=0.;
    if(Passed)
    {
        for(int32 E=0;E<HouseCalderWildlifeCrowRoost::Events;++E)
        {
            const int32 Count=HouseCalderWildlifeCrowRoost::Count(E);MinCount=FMath::Min(MinCount,Count);MaxCount=FMath::Max(MaxCount,Count);
            double End=0.;for(int32 M=0;M<Count;++M)End=FMath::Max(End,Plan.LaunchSpacing*M+Plan.Routes[M].Length/HouseCalderWildlifeCrowRoost::Speed);
            Cadence+=FString::Printf(TEXT("%d,%.3f,%d,%.6f\n"),E,35.+150.*E,Count,End);if(Count<3||Count>15||End>=150.)++Failures;
        }
        for(double Scene=0.;Scene<3600.;Scene+=.05)
        {
            ++ClockSamples;TArray<FVector,TInlineAllocator<15>> Positions;int32 Groups=0;
            for(int32 E=0;E<HouseCalderWildlifeCrowRoost::Events;++E)
            {
                bool Any=false;for(int32 M=0;M<15;++M)
                {
                    HouseCalderWildlifeSkyPaths::FSample Bird;if(!Plan.Evaluate(Scene,E,M,Bird))continue;Any=true;
                    if(Bird.Position.ContainsNaN()||!Sky.Clear(Bird.Position,HouseCalderWildlifeCrowRoost::Radius))++Failures;
                    Positions.Add(Bird.Position);
                }
                Groups+=int32(Any);
            }
            if(Groups>1||Positions.Num()>15)++Failures;
            for(int32 A=0;A<Positions.Num();++A)for(int32 B=A+1;B<Positions.Num();++B)if(FVector::Distance(Positions[A],Positions[B])<180.)++Failures;
        }
        for(int32 E=0;E<12;++E)for(int32 M=0;M<15;++M)
        {
            HouseCalderWildlifeSkyPaths::FSample A,B;const bool Before=Plan.Evaluate(1800.-.00001,E,M,A),After=Plan.Evaluate(1800.,E,M,B);
            if(Before!=After)++Failures;else if(Before)Seam=FMath::Max(Seam,FVector::Distance(A.Position,B.Position));
        }
        Passed&=ClockSamples>=72000&&Failures==0&&Seam<.1&&Sky.Hash==0x74bb095c&&HouseCalderTerrain::Hash()==0x72564759;
    }
    FFileHelper::SaveStringToFile(Trees,*(Dir/TEXT("trees.csv")));FFileHelper::SaveStringToFile(Targets,*(Dir/TEXT("targets.csv")));FFileHelper::SaveStringToFile(Routes,*(Dir/TEXT("routes.csv")));FFileHelper::SaveStringToFile(Cadence,*(Dir/TEXT("cadence.csv")));
    const FString Result=FString::Printf(TEXT("%s scope=crowRoostDataOnly trees=%d targets=%d routeSamples=%d candidateRejects=%d clockSamples=%d failures=%d groupMin=%d groupMax=%d minimumPairCm=%.6f maximumGroupSeconds=%.6f launchSpacingSeconds=%.3f wrapSeamCm=%.6f hash=%08x baseSky=%08x terrain=%08x physicsQueries=0 actualExistingTreeBounds=1 pineTaxonomyVerified=0 renderAccepted=0 callsAccepted=0 wholePhase3Complete=0 buildReviewMs=%.2f\n"),Passed?TEXT("PASS"):TEXT("FAIL"),Plan.Trees.Num(),Plan.Targets.Num(),Plan.Samples,Plan.RejectedCandidates,ClockSamples,Failures,MinCount,MaxCount,Plan.MinimumPairDistance,Plan.MaximumGroupDuration,Plan.LaunchSpacing,Seam,Plan.Hash,Sky.Hash,HouseCalderTerrain::Hash(),(FPlatformTime::Seconds()-Began)*1000.);
    FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("CALDER_CROW_ROOST_DATA %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
