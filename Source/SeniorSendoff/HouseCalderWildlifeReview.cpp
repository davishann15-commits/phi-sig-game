// First-slice native graph/component proof. Full reactions, renders, feet,
// recordings and two-process behavior are separate required acceptance gates.
#if !UE_BUILD_SHIPPING
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePaths.h"
#include "HouseCalderTerrain.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HouseCalderWildlifeReviewImpl
{
TWeakObjectPtr<UWorld> LastWorld;bool Done=false;
void Tick(UWorld* World,ELevelTick,float)
{
    if(!World || !World->IsGameWorld() || !World->HasBegunPlay()
        || !FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifePathsReview"))
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    if(LastWorld.Get()!=World){LastWorld=World;Done=false;}if(Done || World->GetTimeSeconds()<10.)return;
    const bool Enabled=HouseCalderWildlife::Enabled();AHouseCalderWildlifeDirector* Director=nullptr;int32 Actors=0;
    for(TActorIterator<AHouseCalderWildlifeDirector> It(World);It;++It){++Actors;Director=*It;}
    if(Enabled && (!Director || !Director->Ready()) && World->GetTimeSeconds()<100.)return;
    Done=true;int32 Failures=0,Components=0,Visible=0,Instances=0,Skeletal=0,Lights=0;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        TInlineComponentArray<UInstancedStaticMeshComponent*> Parts(*It);
        for(auto* P:Parts)if(P->GetName().StartsWith(TEXT("Wildlife_")))
        {
            ++Components;Visible+=P->IsVisible()?1:0;Instances+=P->GetInstanceCount();
            if(!It->IsA<AHouseCalderWildlifeDirector>() || Cast<UHierarchicalInstancedStaticMeshComponent>(P)
                || P->GetMobility()!=EComponentMobility::Movable || P->GetCollisionEnabled()!=ECollisionEnabled::NoCollision
                || P->GetGenerateOverlapEvents() || P->CanEverAffectNavigation() || P->CastShadow
                || P->bAffectDistanceFieldLighting || P->bAffectDynamicIndirectLighting)++Failures;
        }
        if(It->IsA<AHouseCalderWildlifeDirector>())
        {TInlineComponentArray<USkeletalMeshComponent*> Sk(*It);Skeletal+=Sk.Num();TInlineComponentArray<ULightComponent*> L(*It);Lights+=L.Num();}
    }
    FString States=TEXT("id,species,habitat,variant,mood,start,x,y,z,toX,toY,toZ,seed\n");
    int32 Counts[3]={},Bucks=0,Young=0;
    if(Enabled)
    {
        if(Actors!=1 || !Director || !Director->Ready())++Failures;
        if(Director && Director->Ready())
        {
            for(const auto& S:Director->States.Items)
            {
                if(S.Habitat>=3 || S.Species!=0){++Failures;continue;}++Counts[S.Habitat];Bucks+=S.Variant==1?1:0;Young+=S.Variant==2?1:0;
                const FVector P=Director->AnimalPosition(S.Id);
                States+=FString::Printf(TEXT("%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u\n"),S.Id,S.Species,S.Habitat,S.Variant,uint8(S.Mood),S.StartTime,P.X,P.Y,P.Z,S.To.X,S.To.Y,S.To.Z,S.Seed);
            }
            if(Counts[0]!=3 || Counts[1]!=4 || Counts[2]!=3 || Bucks!=1 || Young!=3 || Director->ValidPathCount()<=0)++Failures;
        }
        if(Components!=(World->GetNetMode()==NM_DedicatedServer?0:4))++Failures;
    }
    else if(Actors!=0 || Components!=0 || Instances!=0)++Failures;
    if(Components>56 || Visible>20 || Skeletal!=0 || Lights!=0)++Failures;
    const FString Result=FString::Printf(TEXT("%s scope=deerFirstGraphAndComponents wholePhase3Complete=0 enabled=%d authority=%d actors=%d animals=%d groups=%d/%d/%d bucks=%d spotlessYoung=%d components=%d visible=%d instances=%d paths=%d habitatHash=%08x terrainHash=%08x runtimeSkeletons=%d extraLights=%d failures=%d renderFeetReactionSoundMultiplayerAcceptancePending=1\n"),
        Failures==0?TEXT("PASS"):TEXT("FAIL"),int32(Enabled),World->GetNetMode()==NM_Client?0:1,Actors,Director?Director->States.Items.Num():0,Counts[0],Counts[1],Counts[2],Bucks,Young,Components,Visible,Instances,Director?Director->ValidPathCount():0,Director?Director->HabitatHash():0,HouseCalderTerrain::Hash(),Skeletal,Lights,Failures);
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("CalderWildlifePathsReview");IFileManager::Get().MakeDirectory(*Dir,true);
    FFileHelper::SaveStringToFile(Result,*(Dir/TEXT("result.txt")));FFileHelper::SaveStringToFile(States,*(Dir/TEXT("states.csv")));
    UE_LOG(LogTemp,Display,TEXT("CALDER_WILDLIFE_PATHS_REVIEW %s"),*Result);FPlatformMisc::RequestExitWithStatus(false,Failures==0?0:1);
}
struct FRegistration{FRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&Tick);}}Registration;
}
#endif
