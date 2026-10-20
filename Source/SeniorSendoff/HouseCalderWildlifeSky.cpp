// Opt-in deterministic local flight on the existing director. Landing/calls remain
// a later integration gate; normal play does not enable this prototype.
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderWildlifeBirds.h"
#include "HouseCalderWildlifeBirdCalls.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "StaticMeshResources.h"
namespace HouseCalderWildlifeSkyImpl
{
bool OriginalEnabled(){static const bool B=FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSkyTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePondTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioTrial"));return B;}
bool CrowEnabled(){static const bool B=HouseCalderWildlife::BirdsEnabled()||FParse::Param(FCommandLine::Get(),TEXT("CalderCrowRoostTrial"));return B;}
bool GooseEnabled(){return HouseCalderWildlife::BirdsEnabled()||OriginalEnabled();}
bool Enabled(){return GooseEnabled()||CrowEnabled();}
int32 Count(int32 Kind){return Kind==0?40:Kind==1?(CrowEnabled()?17:15):4;}
bool Evaluate(const HouseCalderWildlifeSkyPaths::FPlan& Plan,double Scene,int32 Kind,int32 Slot,HouseCalderWildlifeSkyPaths::FSample& Out)
{
    if(Kind==0){for(int32 Event=0;Event<2;++Event)if(Plan.EvaluateGoose(Scene,Event,Slot,Out))return true;return false;}
    if(Kind==1){for(int32 Event=0;Event<12;++Event)if(Plan.EvaluateCrow(Scene,Event,Slot,Out))return true;return false;}
    return Plan.EvaluateBat(Scene,Slot*2+Kind-2,Out);
}
}
void AHouseCalderWildlifeDirector::DrawSky(double Now)
{
    if(!HouseCalderWildlifeSkyImpl::Enabled() || !Runtime || !Ready() || GetNetMode()==NM_DedicatedServer)return;
    auto& Sky=Runtime->Sky;
    if(!Sky.Attempted)
    {
        Sky.Attempted=true;const double Began=FPlatformTime::Seconds();
        if(!Sky.PlanReady)Sky.PlanReady=Sky.Plan.Build(GetWorld());
        if(!Sky.PlanReady){Sky.Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_SKY_FAILED invalid analytic flight plan"));return;}
        if(HouseCalderWildlifeSkyImpl::CrowEnabled()&&!Sky.CrowRoost.Build(GetWorld(),Sky.Plan))
        {Sky.Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_SKY_FAILED crow roost plan"));return;}
        auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/M_CalderBirdPart.M_CalderBirdPart"));
        if(!Material){Sky.Failed=true;return;}
        for(int32 Kind=0;Kind<4;++Kind)
        {
            UStaticMesh* Mesh=HouseCalderWildlifeBirds::Build(this,Kind,Material);
            if(!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num()!=3){Sky.Failed=true;return;}
            // Owned meshes only, changed in memory. This 12m margin covers the
            // fastest flight displacement between the 2Hz bounds refreshes.
            Mesh->SetPositiveBoundsExtension(FVector(1200));Mesh->SetNegativeBoundsExtension(FVector(1200));Mesh->CalculateExtendedBounds();OwnedMeshes.Add(Mesh);
            auto* Part=NewObject<UInstancedStaticMeshComponent>(this,FName(*FString::Printf(TEXT("Wildlife_Sky_%d"),Kind)));AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);
            Part->SetIsReplicated(false);Part->SetMobility(EComponentMobility::Movable);Part->SetStaticMesh(Mesh);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;Part->SetHasPerInstancePrevTransforms(true);Part->SetNumCustomDataFloats(225);Part->SetCullDistances(0,40000);
            const int32 Count=HouseCalderWildlifeSkyImpl::Count(Kind);Sky.Slots[Kind].SetNum(Count);Sky.Current[Kind].Init(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),Count);
            if(Kind==1&&HouseCalderWildlifeSkyImpl::CrowEnabled())
            {
                FBox Footprint(ForceInit);for(const auto& Route:Sky.CrowRoost.Routes)for(const FVector& P:Route.Points)Footprint+=P;
                Sky.Current[Kind][15].SetTranslation(Footprint.Min);Sky.Current[Kind][16].SetTranslation(Footprint.Max);
                UE_LOG(LogTemp,Display,TEXT("CALDER_CROW_ROOST_READY hash=%08x birds=15 hiddenBoundsAnchors=2 actualInstances=17 towerUntouched=1 callsPending=1"),Sky.CrowRoost.Hash);
            }
            Sky.Previous[Kind]=Sky.Current[Kind];Part->AddInstances(Sky.Current[Kind],false,false,false);Part->SetVisibility(false);Part->RegisterComponent();SkyParts.Add(Part);
        }
        Sky.Ready=true;UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_READY components=4 slots=40/%d/4/4 localNonreplicated=1 controls=16 customFloats=225 collision=0 actorsPerAnimal=0 runtimeSkeletons=0 landingPending=1 callsPending=1 buildMs=%.2f"),HouseCalderWildlifeSkyImpl::Count(1),(FPlatformTime::Seconds()-Began)*1000.);
    }
    if(!Sky.Ready || Sky.Failed)return;
    struct FView{FVector Eye,Forward;double Cos;};TArray<FView,TInlineAllocator<2>> Views;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(const auto* PC=It->Get())if(PC->IsLocalController() && PC->PlayerCameraManager)
        Views.Add({PC->PlayerCameraManager->GetCameraLocation(),PC->PlayerCameraManager->GetCameraRotation().Vector(),FMath::Cos(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5+10.))});
    double Scene=HouseCalderWildlife::SceneTime(GetWorld());
#if !UE_BUILD_SHIPPING
    if(Sky.ReviewScene>=0.)Scene=Sky.ReviewScene;
#endif
    bool AnyChanged=false;Sky.BatchCalls=0;Sky.BoundsCalls=0;++Sky.Frames;
    TArray<float,TInlineAllocator<225>> Data;Data.SetNumZeroed(225);
    for(int32 Kind=0;Kind<4;++Kind)
    {
        if(Kind==0&&!HouseCalderWildlifeSkyImpl::GooseEnabled())continue;
        if(Kind>=2&&!HouseCalderWildlifeSkyImpl::OriginalEnabled())continue;
        bool Changed=false;Sky.Shown[Kind]=0;
        for(int32 Slot=0;Slot<Sky.Slots[Kind].Num();++Slot)
        {
            if(Kind==1&&HouseCalderWildlifeSkyImpl::CrowEnabled()&&Slot>=15)continue;
            auto& State=Sky.Slots[Kind][Slot];HouseCalderWildlifeSkyPaths::FSample Sample;bool Active=false;
            if(Kind==1&&HouseCalderWildlifeSkyImpl::CrowEnabled())
            {for(int32 Event=0;Event<12;++Event)if(Sky.CrowRoost.Evaluate(Scene,Event,Slot,Sample)){Active=true;break;}}
            else Active=HouseCalderWildlifeSkyImpl::Evaluate(Sky.Plan,Scene,Kind,Slot,Sample);
            if(Active && Kind==0 && Sample.Event==1 && Slot>=22 && Runtime->Pond.Ready && Scene>=Runtime->Pond.Plan.SplitScene)Active=false;bool Show=false;
            if(Active)for(const auto& View:Views){const FVector Delta=Sample.Position-View.Eye;Show|=Delta.SizeSquared()<=40000.*40000. && (Delta.SizeSquared()<10000. || FVector::DotProduct(Delta.GetSafeNormal(),View.Forward)>View.Cos);}
            if(Show)++Sky.Shown[Kind];
            if(!Show && !State.Visible)continue;
            const bool Reset=!State.Visible || !Show || State.Seed!=Sample.Seed;
            const bool Frozen=Show && State.Scene==Scene;
            if(Frozen && State.Settled)continue;
            FTransform Root;
            if(Show)Root=FTransform(Sample.Direction.Rotation(),Sample.Position,FVector(Sample.Scale));
            else Root=FTransform(FQuat::Identity,Sky.Current[Kind][Slot].GetTranslation(),FVector::ZeroVector);
            if(!Reset)HouseCalderWildlifeBirds::Pack(State.Controls,Data.GetData()+112);
            HouseCalderWildlifeBirds::Pose(Kind,Scene+double(Sample.Seed%1009u)*.013,Root,State.Controls,Kind==0&&HouseCalderWildlifeBirdCalls::Enabled()?HouseCalderWildlifeBirdCalls::Mouth(Sample.Age,Sample.Event,Slot):Kind==1&&HouseCalderWildlifeSkyImpl::CrowEnabled()?HouseCalderWildlifeBirdCalls::CrowMouth(Sample.Age+Sky.CrowRoost.LaunchSpacing*Slot,Sample.Event,HouseCalderWildlifeCrowRoost::Count(Sample.Event),Slot):0.);
            HouseCalderWildlifeBirds::Pack(State.Controls,Data.GetData());
            if(Reset || Frozen)HouseCalderWildlifeBirds::Pack(State.Controls,Data.GetData()+112);
            Data[224]=Show?Sample.Scale:0.f;
            Sky.Previous[Kind][Slot]=Reset || Frozen?Root:Sky.Current[Kind][Slot];Sky.Current[Kind][Slot]=Root;
            SkyParts[Kind]->SetCustomData(Slot,MakeArrayView(Data.GetData(),Data.Num()),false);
            State.Visible=Show;State.Settled=Frozen;State.Scene=Scene;State.Seed=Sample.Seed;Changed=true;
        }
        if(Sky.Shown[Kind]>0)Sky.HideAfter[Kind]=Now+5.;const bool Visible=Sky.Shown[Kind]>0 || (Sky.Visible[Kind] && Now<Sky.HideAfter[Kind]);
        if(Visible!=Sky.Visible[Kind]){SkyParts[Kind]->SetVisibility(Visible);Sky.Visible[Kind]=Visible;}
        if(Changed){SkyParts[Kind]->BatchUpdateInstancesTransforms(0,Sky.Current[Kind],Sky.Previous[Kind],false,false,false);++Sky.BatchCalls;AnyChanged=true;}
    }
    if(AnyChanged && Now>=Sky.NextBounds)
    {Sky.NextBounds=Now+.5;for(UInstancedStaticMeshComponent* Part:SkyParts){Part->UpdateBounds();Part->MarkRenderTransformDirty();++Sky.BoundsCalls;}}
    if(Now>=Sky.StatsAt+20.){Sky.StatsAt=Now;UE_LOG(LogTemp,Display,TEXT("CALDER_SKY_FRAME scene=%.3f shown=%d/%d/%d/%d batch=%d bounds=%d localViewCount=%d pinned=%d pathHash=%08x"),Scene,Sky.Shown[0],Sky.Shown[1],Sky.Shown[2],Sky.Shown[3],Sky.BatchCalls,Sky.BoundsCalls,Views.Num(),int32(HouseCalderWildlife::ScenePinned()),Sky.Plan.Hash);}
}
#if !UE_BUILD_SHIPPING
bool AHouseCalderWildlifeDirector::ReadSkyForReview(int32 Kind,int32 Slot,FTransform& Root,bool& Visible,double& Scene)const
{
    if(!Runtime || !Runtime->Sky.Ready || Kind<0 || Kind>=4 || !Runtime->Sky.Slots[Kind].IsValidIndex(Slot))return false;
    if(!SkyParts[Kind]->GetInstanceTransform(Slot,Root,true))return false;Visible=Runtime->Sky.Slots[Kind][Slot].Visible;Scene=Runtime->Sky.Slots[Kind][Slot].Scene;return true;
}
bool AHouseCalderWildlifeDirector::SetSkySceneForReview(double Scene)
{
    if((!FParse::Param(FCommandLine::Get(),TEXT("CalderWildlifeSkyReview"))&&!FParse::Param(FCommandLine::Get(),TEXT("CalderCrowRoostReview"))) || !Runtime)return false;
    Runtime->Sky.ReviewScene=Scene;return true;
}
bool AHouseCalderWildlifeDirector::SkyFailedForReview()const{return Runtime && Runtime->Sky.Failed;}
uint32 AHouseCalderWildlifeDirector::SkyHashForReview()const{return Runtime && Runtime->Sky.Ready?Runtime->Sky.Plan.Hash:0u;}
uint32 AHouseCalderWildlifeDirector::CrowRoostHashForReview()const{return Runtime&&Runtime->Sky.Ready?Runtime->Sky.CrowRoost.Hash:0u;}
#endif
