#include "HouseTreeReaction.h"

#if !UE_BUILD_SHIPPING
#include "HouseExterior.h"
#include "HouseLeafWind.h"
#include "HouseDamageSubsystem.h"
#include "HouseBasketball.h"
#include "SeniorDouli.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
void TickTreesLobby(UWorld* World,ELevelTick,float)
{
    if(!World||!World->IsGameWorld()||World->GetNetMode()!=NM_ListenServer||!FParse::Param(FCommandLine::Get(),TEXT("HouseTreesNetworkReview"))
        ||UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Lobby"))return;
    auto* Lobby=World->GetGameState<ASeniorLobbyGameState>();if(!Lobby||Lobby->GetMembers().Num()!=2||Lobby->bStarting)return;
    ASeniorLobbyController* Host=nullptr;ASeniorLobbyController* Remote=nullptr;
    for(FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
        if(auto* C=Cast<ASeniorLobbyController>(It->Get())){if(C->IsLocalController())Host=C;else Remote=C;}
    if(!Host||!Remote||!Remote->HasClientLoadedCurrentWorld())return;
    // Use normal lobby readiness/travel with different characters. The client
    // owns Braxton's weapon; the host tests the thrown-object collision path.
    Host->SetCharacter(1);Remote->SetCharacter(0);Host->SetReady(true);Remote->SetReady(true);
    if(Lobby->CanStart()&&Host->CanStartStory())
    {
        Host->StartStory(false);
        UE_LOG(LogTemp,Display,TEXT("TREES_REVIEW normal lobby travel: host=runner client=Braxton"));
    }
}
struct FTreesLobbyRegistration
{
    FTreesLobbyRegistration(){FWorldDelegates::OnWorldPostActorTick.AddStatic(&TickTreesLobby);}
} TreesLobbyRegistration;

struct FTreeReview
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ACameraActor> Camera;
    int32 Stage=0,Species=0,Before=0,BaseBroken=0,Shots=0,Peak=0;
    double Start=0,Step=0,LastShot=0;
    bool bFallShot=false,bLandShot=false,bFired=false;
    FString Dir,Report;
    FTransform Tree;
};
bool SelectTree(UWorld* W,int32 Kind,FTransform& Result)
{
    const FVector Places[]={FVector(2200,-2650,-65),FVector(-2350,-3250,-65),FVector(-1450,-3500,-65)};
    float Nearest=FLT_MAX;bool Found=false;
    for(TActorIterator<AHouseExteriorDetail> It(W);It;++It)
    {
        TArray<UHierarchicalInstancedStaticMeshComponent*> Parts;It->GetComponents(Parts);
        for(auto* H:Parts)if(H->GetName().StartsWith(FString::Printf(TEXT("ExteriorTree%s"),HouseNature::TreeName(HouseNature::ETreeKind(Kind)))))
            for(int32 K=0;K<H->GetInstanceCount();++K)
            {
                FTransform T;H->GetInstanceTransform(K,T,true);
                const float D=FVector::DistSquared(T.GetLocation(),Places[Kind]);
                if(D<Nearest){Nearest=D;Result=T;Found=true;}
            }
    }
    return Found&&Nearest<100.f;
}
void AimAtTree(AStoryFirstPersonCharacter* Pawn,APlayerController* PC,const FTransform& Tree)
{
    const FVector Base=Tree.GetLocation();
    const float Side=Pawn->GetNetMode()==NM_ListenServer&&PC->IsLocalController()?-190.f:190.f;
    Pawn->SetActorLocation(Base+FVector(Side,-360,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+7),false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None);
    PC->SetViewTarget(Pawn);
    PC->SetControlRotation((Base+FVector(0,0,130)-Pawn->GetPawnViewLocation()).Rotation());
}
}

void TickHouseTreesReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn||!Pawn->IsLocallyControlled()||!Pawn->GetWorld())return;
    UWorld* W=Pawn->GetWorld();auto* PC=Cast<APlayerController>(Pawn->GetController());if(!PC)return;
    TActorIterator<AHouseTreeReaction> ReactionIt(W);AHouseTreeReaction* Relay=ReactionIt?*ReactionIt:nullptr;
    TActorIterator<AHouseLeafWind> WindIt(W);AHouseLeafWind* Wind=WindIt?*WindIt:nullptr;
    if(!Relay||!Wind)return;
    static FTreeReview S;
    const double Now=W->GetTimeSeconds();
    const bool Network=FParse::Param(FCommandLine::Get(),TEXT("HouseTreesNetworkReview"));
    const bool Client=Pawn->GetNetMode()==NM_Client;
    if(S.World.Get()!=W)
    {
        S=FTreeReview();S.World=W;S.Start=S.Step=Now;
        S.Dir=FPaths::Combine(FPaths::ProjectSavedDir(),Network?(Client?TEXT("TreesReview/Client"):TEXT("TreesReview/Host")):TEXT("TreesReview/Single"));
        IFileManager::Get().MakeDirectory(*S.Dir,true);
        if(auto* Damage=W->GetSubsystem<UHouseDamageSubsystem>())S.BaseBroken=Damage->GetTotalBrokenCount();
    }
    auto Note=[&](const FString& T){S.Report+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("TREES_REVIEW %s"),*T);};
    auto Finish=[&](bool Pass,const FString& Why)
    {
        Note((Pass?TEXT("PASS "):TEXT("FAIL "))+Why);
        FFileHelper::SaveStringToFile(S.Report,*FPaths::Combine(S.Dir,TEXT("result.txt")));
        S.Stage=-1;FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    auto Shot=[&](const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,Name+TEXT(".png")),false,false);};
    if(S.Stage<0)return;
    if(Now-S.Start>90){Finish(false,TEXT("Tree interaction review timed out"));return;}
    S.Peak=FMath::Max(S.Peak,Wind->ActiveFallingLeaves());
    if(S.Peak>216){Finish(false,TEXT("Falling-leaf pool exceeded its limit"));return;}
    if(Network)
    {
        if(S.Stage==0)
        {
            if(Now-S.Start<9||(Client&&!Pawn->Douli))return;
            if(!Client&&(!W->GetGameState()||W->GetGameState()->PlayerArray.Num()<2))return;
            if(!SelectTree(W,1,S.Tree)){Finish(false,TEXT("Network oak unavailable"));return;}
            AimAtTree(Pawn,PC,S.Tree);
            if(!Client)
            {
                for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
                    if(APlayerController* Other=It->Get())if(Other!=PC)if(auto* P=Cast<AStoryFirstPersonCharacter>(Other->GetPawn()))
                        AimAtTree(P,Other,S.Tree);
            }
            S.Step=Now;S.Before=Relay->ReceivedEvents;S.Stage=1;Note(TEXT("Two-player tree review ready"));
        }
        if(S.Stage==1)
        {
            if(Client&&!S.bFired&&Now-S.Step>2)
            {
                PC->SetControlRotation((S.Tree.GetLocation()+FVector(0,0,130)-Pawn->GetPawnViewLocation()).Rotation());
                Pawn->ThrowDouli();S.bFired=true;Note(TEXT("Client requested normal weapon throw"));
            }
            if(!Client&&!S.bFired&&Relay->ReceivedEvents>S.Before&&Now-S.Step>5)
            {
                // The weapon's contact and the second physics impact must be
                // separate hits, outside the tree's duplicate-contact cooldown.
                if(S.LastShot==0)S.LastShot=Now;
                if(Now-S.LastShot<.9)return;
                TActorIterator<AHouseBasketball> Ball(W);
                if(Ball)
                {
                    Ball->Body->SetWorldLocation(S.Tree.GetLocation()+FVector(0,-200,110),false,nullptr,ETeleportType::TeleportPhysics);
                    Ball->Body->SetPhysicsLinearVelocity(FVector(0,700,0));Ball->Body->WakeAllRigidBodies();
                    S.bFired=true;Note(TEXT("Host threw a physics object into the tree"));
                }
            }
            if(Relay->ReceivedEvents>=S.Before+2&&Wind->ShedEvents>=2&&Wind->LandedLeaves>10)
            {
                Shot(TEXT("SharedOakLeaves"));S.Step=Now;S.Stage=2;
                Note(FString::Printf(TEXT("mode=%d events=%d local_sheds=%d landed=%d peak=%d"),int32(W->GetNetMode()),Relay->ReceivedEvents,Wind->ShedEvents,Wind->LandedLeaves,S.Peak));
            }
        }
        // Keep the listen server alive while the client's screenshot is saved.
        if(S.Stage==2&&Now-S.Step>(Client?.15:5.)&&!FScreenshotRequest::IsScreenshotRequested())
            Finish(true,TEXT("Client weapon and host physics hits both produced shared leaf events and local falling/settling leaves"));
        return;
    }
    if(!Pawn->HasAuthority())return;
    if(S.Stage==0)
    {
        if(Now-S.Start<7||!Pawn->Douli)return;
        if(!SelectTree(W,S.Species,S.Tree)){Finish(false,TEXT("Missing required tree species on the lawn"));return;}
        AimAtTree(Pawn,PC,S.Tree);
        FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TreeReviewTrunk),false,Pawn);Q.AddIgnoredActor(Pawn->Douli);
        const bool Hit=W->LineTraceSingleByChannel(H,Pawn->GetPawnViewLocation(),S.Tree.GetLocation()+FVector(0,0,130),ECC_Visibility,Q);
        if(!Hit||!H.GetComponent()||!H.GetComponent()->GetName().StartsWith(TEXT("ExteriorTree"))){Finish(false,TEXT("Weapon aim is not blocked by a visible tree trunk"));return;}
        Note(FString::Printf(TEXT("%s trunk blocks weapon at instance=%d"),HouseNature::TreeName(HouseNature::ETreeKind(S.Species)),H.Item));
        S.Before=Relay->AcceptedHits;S.Step=Now;S.Stage=1;S.bFired=false;S.bFallShot=S.bLandShot=false;
    }
    if(S.Stage==1)
    {
        if(!S.bFired&&Now-S.Step>.35){Pawn->ThrowDouli();S.bFired=true;}
        if(Relay->AcceptedHits>S.Before)
        {
            if(!S.Camera.IsValid())
            {
                S.Camera=W->SpawnActor<ACameraActor>();S.Camera->GetCameraComponent()->SetFieldOfView(76.f);
                S.Camera->GetCameraComponent()->PostProcessSettings=Pawn->FirstPersonCamera->PostProcessSettings;
            }
            const FVector Base=S.Tree.GetLocation(),From=Base+(S.Species==0?FVector(300,-1550,150):FVector(950,-1450,180)),Target=Base+FVector(0,0,450);
            S.Camera->SetActorLocationAndRotation(From,(Target-From).Rotation());PC->SetViewTarget(S.Camera.Get());
            S.Step=Now;S.Stage=2;
            Note(FString::Printf(TEXT("Actual %s weapon hit accepted; active leaves=%d"),HouseNature::TreeName(HouseNature::ETreeKind(S.Species)),Wind->ActiveFallingLeaves()));
        }
        else if(Now-S.Step>5){Finish(false,TEXT("Thrown weapon did not cause a leaf burst"));return;}
    }
    if(S.Stage==2)
    {
        if(!S.bFallShot&&Now-S.Step>2.3){Shot(FString::Printf(TEXT("%sLeavesFalling"),HouseNature::TreeName(HouseNature::ETreeKind(S.Species))));S.bFallShot=true;}
        if(!S.bLandShot&&Now-S.Step>8.8){Shot(FString::Printf(TEXT("%sLeavesSettling"),HouseNature::TreeName(HouseNature::ETreeKind(S.Species))));S.bLandShot=true;}
        if(Now-S.Step>10&&!FScreenshotRequest::IsScreenshotRequested())
        {
            if(++S.Species<3){S.Stage=0;return;}
            S.Species=1;SelectTree(W,1,S.Tree);AimAtTree(Pawn,PC,S.Tree);
            S.Before=Relay->AcceptedHits;S.Step=Now;S.Stage=3;
            // A real thrown physics object uses the component-hit path.
            TActorIterator<AHouseBasketball> It(W);
            if(It)
            {
                It->Body->SetWorldLocation(S.Tree.GetLocation()+FVector(0,-200,110),false,nullptr,ETeleportType::TeleportPhysics);
                It->Body->SetPhysicsLinearVelocity(FVector(0,700,0));It->Body->WakeAllRigidBodies();
            }
        }
    }
    if(S.Stage==3)
    {
        if(Now-S.Step<2)return;
        if(Relay->AcceptedHits<=S.Before){Finish(false,TEXT("Physics-object hit did not shed leaves"));return;}
        Note(TEXT("Thrown physics object knocked leaves from the oak"));S.Stage=4;S.Step=Now;
    }
    if(S.Stage==4)
    {
        if(S.Shots<7&&Now-S.LastShot>1.15&&Pawn->Douli->IsHeld())
        {Pawn->ThrowDouli();S.LastShot=Now;++S.Shots;}
        if(Now-S.Step>11)
        {
            if(Relay->AcceptedHits<S.Before+6||Wind->LandedLeaves<30){Finish(false,TEXT("Repeated hits or leaf settling failed"));return;}
            if(auto* Damage=W->GetSubsystem<UHouseDamageSubsystem>())if(Damage->GetTotalBrokenCount()!=S.BaseBroken){Finish(false,TEXT("Shedding leaves unexpectedly counted as breaking the house"));return;}
            Note(FString::Printf(TEXT("accepted=%d local_sheds=%d landed=%d peak=%d capacity=216"),Relay->AcceptedHits,Wind->ShedEvents,Wind->LandedLeaves,S.Peak));
            Finish(true,TEXT("All three species, actual weapon throws, physics impacts, leaf settling, bounded repeated hits and unchanged house-damage count"));
        }
    }
}
#else
void TickHouseTreesReview(AStoryFirstPersonCharacter*) {}
#endif
