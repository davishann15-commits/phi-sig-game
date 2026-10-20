// Opt-in first pond integration: server-owned roster plus pure shared-clock
// background motion. Shore reactions,wakes,calls and full acceptance remain.
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePrivate.h"
#include "HouseCalderWildlifeBirds.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HouseCalderTerrain.h"
#include "StaticMeshResources.h"
namespace HouseCalderWildlifePondImpl
{
bool ReactionsEnabled(){static const bool B=HouseCalderWildlife::BirdsEnabled()||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseReactionsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseEffectsTrial"))||FParse::Param(FCommandLine::Get(),TEXT("CalderGooseAudioTrial"));return B;}
bool ShoreEnabled(){static const bool Value=FParse::Param(FCommandLine::Get(),TEXT("CalderGooseShoreTrial"))||ReactionsEnabled();return Value;}
bool Enabled(){static const bool Value=FParse::Param(FCommandLine::Get(),TEXT("CalderGoosePondTrial"))||ShoreEnabled();return Value;}
}
void AHouseCalderWildlifeDirector::TickPond(double Now)
{
    if(!HouseCalderWildlifePondImpl::Enabled() || !Runtime || !Ready())return;auto& Pond=Runtime->Pond;
    if(!Pond.Attempted)
    {
        Pond.Attempted=true;auto& Sky=Runtime->Sky;
        if(!Sky.PlanReady)Sky.PlanReady=Sky.Plan.Build(GetWorld());
        if(!Sky.PlanReady || !Pond.Plan.Build(GetWorld(),Sky.Plan)){Pond.Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_POND_FAILED invalid authored data"));return;}
        Pond.ShoreEnabled=HouseCalderWildlifePondImpl::ShoreEnabled();
        Pond.ReactionsEnabled=HouseCalderWildlifePondImpl::ReactionsEnabled();Pond.Reactions.SetNum(16);
        if(Pond.ShoreEnabled&&!Pond.Shore.Build(GetWorld(),Sky.Plan,Pond.Plan)){Pond.Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_POND_FAILED invalid shore data"));return;}
        // Include the full curved approach, not just its start: the left arm
        // swings farther west before returning to the pond. Two zero-scale
        // corner anchors conservatively cover activation between2Hz refreshes.
        FBox Approach(ForceInit);for(const auto& Curve:Pond.Plan.Arrivals)for(const FVector& Point:Curve.Points)Approach+=Point;
        Pond.HiddenAnchors[0]=Approach.Min-FVector(2000);Pond.HiddenAnchors[1]=Approach.Max+FVector(2000);
    }
    if(Pond.Failed)return;
    if(HasAuthority() && PondStates.Items.IsEmpty())
    {
        FlushNetDormancy();SetNetDormancy(DORM_Awake);
        for(int32 Id=0;Id<16;++Id)
        {
            const int32 Member=Id%8;FHouseCalderWildlifeState S;S.Id=Id;S.Species=1;S.Habitat=3;S.Variant=uint8(Id/8);S.Seed=2026101803u+137u+(22+Member)*71u;
            S.From=Pond.Plan.Homes[Member];S.To=Pond.Plan.Touches[Member];S.StartTime=float(Now);HouseCalderWildlifeGoosePaths::FCycleSample Sample;Pond.Sample(HouseCalderWildlife::CycleTime(GetWorld()),Id/8,Member,Sample);S.Cause=uint8(Sample.Phase);PondStates.Items.Add(S);PondStates.MarkItemDirty(PondStates.Items.Last());
        }
        Runtime->LastChange=HouseCalderWildlife::ServerTime(GetWorld());ForceNetUpdate();
        UE_LOG(LogTemp,Display,TEXT("CALDER_POND_ROSTER states=16 cohorts=2 pondPopulation=6to8 incoming=8 continuingV=22 authority=1 reactionsPending=1 callsPending=1"));
    }
    if(PondStates.Items.Num()!=16)return;
    for(int32 Id=0;Id<16;++Id)
    {
        const auto* S=PondStates.Items.FindByPredicate([Id](const auto& Item){return Item.Id==Id;});
        bool Valid=S&&S->Species==1&&S->Variant==Id/8&&S->Habitat==3&&S->Seed==2026101803u+137u+(22+Id%8)*71u&&FMath::IsFinite(S->StartTime);
        auto& Reaction=Pond.Reactions[Id];Reaction={};
        if(Valid&&S->Cause>=64)
        {
            double Arc=0.,Error=0.;HouseCalderWildlifeGoosePaths::FCycleSample Canonical;
            Valid=Pond.ReactionsEnabled&&Id%8>=6&&(S->Cause==64||S->Cause==65)&&Pond.Shore.Project(Id%8-6,FVector(S->From),Arc,Error)&&Error<=.21&&Pond.Plan.Cycle(0.,0,Id%8,Canonical)&&Pond.Shore.At(Id%8-6,Arc,false,Canonical)&&FVector(S->From).Equals(Canonical.Bird.Position,.21)&&FVector(S->To).Equals(Pond.Plan.Homes[Id%8],.11)&&(S->Cause!=65||Arc<=.21);
            if(Valid){Reaction.Mode=S->Cause;Reaction.Start=double(S->StartTime)+HouseCalderWildlife::CycleReviewOffset();Reaction.Arc=S->Cause==65?0.:Arc;Reaction.Yaw=FRotator::DecompressAxisFromShort(S->FacingYaw);Reaction.Expiry=Pond.Shore.HomeDeadline(Pond.Plan,Reaction.Start,Id/8);}
        }
        else if(Valid)Valid=S->Cause<=(Pond.ShoreEnabled?8:5)&&FVector(S->From).Equals(Pond.Plan.Homes[Id%8],.11)&&FVector(S->To).Equals(Pond.Plan.Touches[Id%8],.11);
        if(!Valid){Pond.Failed=true;UE_LOG(LogTemp,Error,TEXT("CALDER_POND_FAILED replicated roster/data mismatch id=%d"),Id);return;}
    }
    Pond.Ready=true;
    if(HasAuthority() && Now>=Pond.NextAuthority)
    {
        Pond.NextAuthority=Now+.1;const double Raw=HouseCalderWildlife::CycleTime(GetWorld());bool Changed=false;
        for(auto& State:PondStates.Items)
        {
            if(State.Cause>=64)continue;
            HouseCalderWildlifeGoosePaths::FCycleSample Sample;Pond.Sample(Raw,State.Id/8,State.Id%8,Sample);
            if(State.Cause!=Sample.Phase){if(!Changed){FlushNetDormancy();SetNetDormancy(DORM_Awake);}State.Cause=uint8(Sample.Phase);State.StartTime=float(Now);PondStates.MarkItemDirty(State);Changed=true;}
        }
        if(Changed){Runtime->LastChange=Now;ForceNetUpdate();UE_LOG(LogTemp,Display,TEXT("CALDER_POND_PHASE authorityNow=%.6f scene=%.6f naturalPhaseChangesOnly=1 idleHeartbeat=0"),Now,Raw);}
        if(Pond.ReactionsEnabled&&!HouseCalderWildlife::ScenePinned())PondReactionStep(Now,Raw);
    }
}
void AHouseCalderWildlifeDirector::DrawPond(double Now)
{
    if(!Runtime || !Runtime->Pond.Ready || Runtime->Pond.Failed || GetNetMode()==NM_DedicatedServer)return;auto& Pond=Runtime->Pond;
    if(!Pond.Visuals)
    {
        UStaticMesh* Mesh=SkyParts.IsEmpty()?nullptr:SkyParts[0]->GetStaticMesh().Get();
        if(!Mesh)
        {
            auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/SeniorSendOff/Calder/Wildlife/M_CalderBirdPart.M_CalderBirdPart"));Mesh=Material?HouseCalderWildlifeBirds::Build(this,0,Material):nullptr;if(!Mesh){Pond.Failed=true;return;}
            Mesh->SetPositiveBoundsExtension(FVector(1200));Mesh->SetNegativeBoundsExtension(FVector(1200));Mesh->CalculateExtendedBounds();OwnedMeshes.Add(Mesh);
        }
        auto* Part=NewObject<UInstancedStaticMeshComponent>(this,TEXT("Wildlife_PondGeese"));AddInstanceComponent(Part);Part->SetupAttachment(RootComponent);Part->SetIsReplicated(false);Part->SetMobility(EComponentMobility::Movable);Part->SetStaticMesh(Mesh);Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);Part->SetCanEverAffectNavigation(false);Part->SetCastShadow(false);Part->bAffectDistanceFieldLighting=false;Part->bAffectDynamicIndirectLighting=false;Part->SetHasPerInstancePrevTransforms(true);Part->SetNumCustomDataFloats(225);Part->SetCullDistances(0,40000);
        Pond.Slots.SetNum(16);Pond.Current.Init(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),16);// Zero-scale hidden slots still contribute their point to ISM bounds.
        // Seed the north approach before activation so a400m visibility entry
        // cannot teleport outside the2Hz cached box. Inactive cohorts retain
        // the full-approach anchors between cycles; per-instance renderer culling stays on.
        for(int32 Id=0;Id<16;++Id)Pond.Current[Id].SetTranslation(Pond.HiddenAnchors[Id%2]);
        Pond.Previous=Pond.Current;Part->AddInstances(Pond.Current,false,false,false);Part->SetVisibility(false);Part->RegisterComponent();PondParts.Add(Part);Pond.Visuals=true;
        UE_LOG(LogTemp,Display,TEXT("CALDER_POND_DRAW components=1 states=16 serverOwnedRoster=1 pureClockBackground=1 collision=0 runtimeSkeletons=0 actorsPerAnimal=0 reactionsPending=1 wakesPending=1 callsPending=1"));
    }
    struct FView{FVector Eye,Forward;double Cos;};TArray<FView,TInlineAllocator<2>> Views;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(const auto* PC=It->Get())if(PC->IsLocalController() && PC->PlayerCameraManager)Views.Add({PC->PlayerCameraManager->GetCameraLocation(),PC->PlayerCameraManager->GetCameraRotation().Vector(),FMath::Cos(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*.5+10.))});
    const double Raw=HouseCalderWildlife::CycleTime(GetWorld());TArray<float,TInlineAllocator<225>> Data;Data.SetNumZeroed(225);bool Changed=false;Pond.Shown=Pond.OnWater=0;
    for(int32 Id=0;Id<16;++Id)
    {
        HouseCalderWildlifeGoosePaths::FCycleSample Sample;const bool Active=Pond.Sample(Raw,Id/8,Id%8,Sample);auto& Slot=Pond.Slots[Id];if(Active)Pond.OnWater+=int32(Sample.OnWater);
        double Distance=1.e30;bool InFront=false;for(const auto& V:Views){const FVector D=Sample.Bird.Position-V.Eye;Distance=FMath::Min(Distance,D.Size());InFront|=D.SizeSquared()<10000. || FVector::DotProduct(D.GetSafeNormal(),V.Forward)>V.Cos;}
        const bool Flying=Sample.Phase==2 || Sample.Phase==5,Show=Active && Distance<(Flying?40000.:30000.) && (!Flying || InFront);if(Show)++Pond.Shown;
        if(!Show&&!Slot.Visible)continue;
        const bool Reset=!Show || !Slot.Visible,Fixed=Slot.Scene==Raw;
        const bool Animate=Flying || (InFront&&Distance<15000.);
        const double PoseKey=Flying || (InFront&&Distance<4000.)?Raw:FMath::FloorToDouble(Raw*15.)/15.;
        const FTransform Wanted=Show?FTransform(Sample.Bird.Direction.Rotation(),Sample.Bird.Position,FVector(Sample.Bird.Scale)):FTransform(FQuat::Identity,Pond.HiddenAnchors[Id%2],FVector::ZeroVector);
        const bool RootMoved=!Wanted.Equals(Pond.Current[Id],.00001);
        const bool PoseChanged=Reset || (!Fixed&&Animate&&PoseKey!=Slot.PoseKey);
        const bool RootChanged=Reset || (!Fixed&&RootMoved&&(Flying || (InFront&&Distance<15000.) || Now>=Slot.NextRoot));
        if(!PoseChanged&&!RootChanged)
        {
            if(Slot.Settled)continue;
            HouseCalderWildlifeBirds::Pack(Slot.Controls,Data.GetData());HouseCalderWildlifeBirds::Pack(Slot.Controls,Data.GetData()+112);Data[224]=Show?Sample.Bird.Scale:0.f;
            Pond.Previous[Id]=Pond.Current[Id];PondParts[0]->SetCustomData(Id,MakeArrayView(Data.GetData(),Data.Num()),false);Slot.Settled=true;Changed=true;continue;
        }
        const FTransform Root=RootChanged?Wanted:Pond.Current[Id];
        if(!Reset)HouseCalderWildlifeBirds::Pack(Slot.Controls,Data.GetData()+112);
        if(PoseChanged)
        {
            const double PoseClock=PoseKey-FMath::FloorToDouble(PoseKey/1800.)*1800.;
            HouseCalderWildlifeBirds::Pose(0,PoseClock+double(Sample.Bird.Seed%1009u)*.013,Root,Slot.Controls,Sample.Mouth,Sample.Flap,Sample.Fold,Sample.Neck);Slot.PoseKey=PoseKey;
        }
        else for(FTransform& Control:Slot.Controls)Control=Control.GetRelativeTransform(Pond.Current[Id])*Root;
        if(Show&&Pond.ShoreEnabled&&Sample.ShoreRoute>=0&&Animate)
        {
            auto Evaluate=[&Pond,Id](double At,HouseCalderWildlifeGoosePaths::FCycleSample& Out){Pond.Sample(At,Id/8,Id%8,Out);};
            HouseCalderWildlifeGooseGait::Apply(Evaluate,Raw,Sample,Root,Distance>=4000.,Slot.Feet,Slot.Controls);
        }
        HouseCalderWildlifeBirds::Pack(Slot.Controls,Data.GetData());
        if(Reset || Fixed)HouseCalderWildlifeBirds::Pack(Slot.Controls,Data.GetData()+112);Data[224]=Show?Sample.Bird.Scale:0.f;
        Pond.Previous[Id]=Reset || Fixed?Root:Pond.Current[Id];Pond.Current[Id]=Root;PondParts[0]->SetCustomData(Id,MakeArrayView(Data.GetData(),Data.Num()),false);
        Slot.Visible=Show;Slot.Settled=Fixed;Slot.Scene=Raw;Slot.NextRoot=Now+.1;Slot.Seed=Sample.Bird.Seed;Changed=true;
    }
    if(Pond.Shown>0)Pond.HideAfter=Now+5.;const bool Visible=Pond.Shown>0 || (Pond.Visible&&Now<Pond.HideAfter);if(Visible!=Pond.Visible){PondParts[0]->SetVisibility(Visible);Pond.Visible=Visible;}
    if(Changed)PondParts[0]->BatchUpdateInstancesTransforms(0,Pond.Current,Pond.Previous,false,false,false);
    if(Changed&&Now>=Pond.NextBounds){Pond.NextBounds=Now+.5;PondParts[0]->UpdateBounds();PondParts[0]->MarkRenderTransformDirty();}
    if(Now>=Pond.StatsAt+10.){Pond.StatsAt=Now;UE_LOG(LogTemp,Display,TEXT("CALDER_POND_FRAME raw=%.6f shown=%d onWater=%d serverStates=%d authority=%d hash=%08x"),Raw,Pond.Shown,Pond.OnWater,PondStates.Items.Num(),int32(HasAuthority()),Pond.Plan.Hash);}
}
#if !UE_BUILD_SHIPPING
bool AHouseCalderWildlifeDirector::ReadPondForReview(int32 Id,FTransform& Root,bool& Visible,int32& Phase,double& Raw)const
{
    if(!Runtime || !Runtime->Pond.Visuals || !Runtime->Pond.Slots.IsValidIndex(Id))return false;
    Raw=Runtime->Pond.Slots[Id].Scene;HouseCalderWildlifeGoosePaths::FCycleSample Sample;Runtime->Pond.Sample(Raw,Id/8,Id%8,Sample);Phase=Sample.Phase;Visible=Runtime->Pond.Slots[Id].Visible;return PondParts[0]->GetInstanceTransform(Id,Root,true);
}
uint32 AHouseCalderWildlifeDirector::PondHashForReview()const{return Runtime&&Runtime->Pond.Ready?Runtime->Pond.Plan.Hash:0u;}
uint32 AHouseCalderWildlifeDirector::ShoreHashForReview()const{return Runtime&&Runtime->Pond.Ready&&Runtime->Pond.ShoreEnabled?Runtime->Pond.Shore.Hash:0u;}
double AHouseCalderWildlifeDirector::PondChangeWallForReview(int32 Id)const{return Runtime&&Id>=0&&Id<16?Runtime->Pond.ChangeWall[Id]:0.;}
bool AHouseCalderWildlifeDirector::ReadPondSolesForReview(int32 Id,FCalderGooseSoleProbe& Out)const
{
    Out={};if(!Runtime || !Runtime->Pond.ShoreEnabled || !Runtime->Pond.Slots.IsValidIndex(Id) || PondParts.IsEmpty())return false;
    const auto& Pond=Runtime->Pond;const auto& Slot=Pond.Slots[Id];if(!Slot.Visible)return false;
    HouseCalderWildlifeGoosePaths::FCycleSample Sample;Pond.Sample(Slot.Scene,Id/8,Id%8,Sample);Out.Stand=Sample.Stand;
    const auto* Part=PondParts[0].Get();const auto& Data=Part->PerInstanceSMCustomData;if(Data.Num()<(Id+1)*225 || !Part->GetStaticMesh()->GetRenderData())return false;
    const float* Actual=Data.GetData()+Id*225;const auto& L=Part->GetStaticMesh()->GetRenderData()->LODResources[0];
    for(int32 Leg=0;Leg<2;++Leg){Out.Cycles[Leg]=Slot.Feet.CurrentCycles[Leg];if(Slot.Feet.Planted[Leg]&&Sample.Stand>.999)Out.PlantedMask|=1<<Leg;}
    for(uint32 V=0;V<L.GetNumVertices();++V)
    {
        const FVector Local(L.VertexBuffers.PositionVertexBuffer.VertexPosition(V));const auto Weight=L.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V,1);const int32 Joint=FMath::RoundToInt(Weight.X);if((Joint!=11&&Joint!=13)||Local.Z> -22.20 || Weight.Y<.99)continue;
        const int32 Leg=Joint==11?0:1,Offset=Joint*7;const FQuat Q(Actual[Offset],Actual[Offset+1],Actual[Offset+2],Actual[Offset+3]);
        const FVector P=Q.RotateVector(Local*Actual[224])+FVector(Actual[Offset+4],Actual[Offset+5],Actual[Offset+6]);Out.Points[Leg].Add(P);
        if(Out.PlantedMask&(1<<Leg))Out.Gaps.Add(FMath::Abs(P.Z-HouseCalderTerrain::SurfaceZ(P.X,P.Y)));
    }
    return !Out.Points[0].IsEmpty()&&!Out.Points[1].IsEmpty();
}
bool AHouseCalderWildlifeDirector::PondReadyForReview()const{return Runtime&&Runtime->Pond.Ready;}
bool AHouseCalderWildlifeDirector::PondFailedForReview()const{return Runtime&&Runtime->Pond.Failed;}
#endif
