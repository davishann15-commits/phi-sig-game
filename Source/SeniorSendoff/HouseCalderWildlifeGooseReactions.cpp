// Opt-in authority events only. Existing FastArray, no per-bird actors or
// heartbeat; every peer renders the same elapsed-time validated-route motion.
#include "HouseCalderWildlife.h"
#include "HouseCalderWildlifePrivate.h"
#include "HouseExteriorSiteLayout.h"
#include "StoryMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
void AHouseCalderWildlifeDirector::PondReactionStep(double Now,double Raw)
{
    if(!HasAuthority()||!Runtime||!Runtime->Pond.ReactionsEnabled)return;auto& Pond=Runtime->Pond;
    struct FPlayer{FVector At;double Scale;};TArray<FPlayer,TInlineAllocator<2>> Players;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        const auto* PC=It->Get();const APawn* Pawn=PC?PC->GetPawn():nullptr;if(!Pawn||!Pawn->IsPlayerControlled())continue;
        const FVector P=Pawn->GetActorLocation();if(P.Z-HouseExteriorSite::GroundHeight(P.X,P.Y)>500.)continue;
        const auto* C=Cast<ACharacter>(Pawn);const auto* M=C?Cast<UStoryMovementComponent>(C->GetCharacterMovement()):nullptr;
        Players.Add({P,M&&M->IsSprinting()?2.:C&&C->bIsCrouched?.6:1.});
    }
    auto Nearest=[&Players](const FVector& Root,double Scale)
    {double Best=1.e30;for(const auto& P:Players)Best=FMath::Min(Best,FVector::Distance(Root+FVector(0,0,45.*Scale),P.At)/P.Scale);return Best;};
    bool Changed=false;
    // Dormancy must end before changing replicated properties; waking after
    // mutation can initialize the channel's shadow with the new values.
    auto Wake=[&]{if(!Changed){FlushNetDormancy();SetNetDormancy(DORM_Awake);}};
    auto Natural=[&](FHouseCalderWildlifeState& State,const HouseCalderWildlifeGoosePaths::FCycleSample& Base)
    {
        Wake();
        State.Cause=uint8(Base.Phase);State.StartTime=float(Now);State.From=Pond.Plan.Homes[State.Id%8];State.To=Pond.Plan.Touches[State.Id%8];State.FacingYaw=0;Pond.Reactions[State.Id]={};PondStates.MarkItemDirty(State);Changed=true;
        UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_EVENT id=%d event=natural phase=%d authorityTime=%.6f raw=%.6f"),State.Id,Base.Phase,Now,Raw);
    };
    auto Event=[&](FHouseCalderWildlifeState& State,uint8 Mode,const HouseCalderWildlifeGoosePaths::FCycleSample& Live,double Arc)
    {
        Wake();
#if !UE_BUILD_SHIPPING
        Pond.ChangeWall[State.Id]=FPlatformTime::Seconds();
#endif
        State.Cause=Mode;State.StartTime=float(Now);State.From=Live.Bird.Position;State.To=Pond.Plan.Homes[State.Id%8];State.FacingYaw=FRotator::CompressAxisToShort(Live.Bird.Direction.Rotation().Yaw);
        auto& R=Pond.Reactions[State.Id];R.Mode=Mode;R.Start=double(State.StartTime)+HouseCalderWildlife::CycleReviewOffset();R.Arc=Mode==65?0.:Arc;R.Yaw=FRotator::DecompressAxisFromShort(State.FacingYaw);R.Expiry=Pond.Shore.HomeDeadline(Pond.Plan,R.Start,State.Id/8);
        PondStates.MarkItemDirty(State);Changed=true;
        UE_LOG(LogTemp,Display,TEXT("CALDER_GOOSE_REACTION_EVENT id=%d event=%s authorityTime=%.6f raw=%.6f arcCm=%.6f effectiveThreatCm=%.3f thresholdCm=600 checkHz=10 threeDimensional=1 anyPlayer=1 sprintScale=2 crouchScale=.6"),State.Id,Mode==64?TEXT("hissRetreat"):TEXT("returnShore"),Now,Raw,Arc,Nearest(Live.Bird.Position,Live.Bird.Scale));
    };
    for(auto& State:PondStates.Items)
    {
        if(State.Id%8<6)continue;const int32 Route=State.Id%8-6;const double Length=Pond.Shore.Routes[Route].Length;
        HouseCalderWildlifeGoosePaths::FCycleSample Live,Base;Pond.Sample(Raw,State.Id/8,State.Id%8,Live);Pond.Shore.Cycle(Pond.Plan,Raw,State.Id/8,State.Id%8,Base);
        if(State.Cause>=64)
        {
            const auto R=Pond.Reactions[State.Id];if(Raw>=R.Expiry){Natural(State,Base);continue;}
            if(State.Cause==65&&Live.Phase==6&&Live.WalkAge<0.&&Base.Phase==6&&Live.Bird.Position.Equals(Base.Bird.Position,.11)){Natural(State,Base);continue;}
            if(State.Cause==64)
            {
                if(Live.Phase!=11||Raw-R.Start<.8+R.Arc/35.+120.)continue;
                auto Bank=Live;Pond.Shore.At(Route,Length,false,Bank);
                const double ReturnAt=Pond.Shore.HomeDeadline(Pond.Plan,Raw,State.Id/8)-Length/35.;
                if(Nearest(Bank.Bird.Position,Bank.Bird.Scale)>1200.&&Raw+2.5+Length/35.+2.<ReturnAt-3.)Event(State,65,Live,0.);
                continue;
            }
        }
        const double ReturnAt=Pond.Shore.HomeDeadline(Pond.Plan,Raw,State.Id/8)-Length/35.;
        if(Live.Bird.Active&&Live.Stand>.999&&Live.ShoreArc>0.&&Raw<ReturnAt-3.&&Nearest(Live.Bird.Position,Live.Bird.Scale)<=600.)Event(State,64,Live,Live.ShoreArc);
    }
    if(Changed){Runtime->LastChange=Now;ForceNetUpdate();}
}
