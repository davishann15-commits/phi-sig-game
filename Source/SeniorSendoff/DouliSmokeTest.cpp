#include "SeniorDouli.h"
#include "SeniorDouliIdle.h"
#include "StoryCampaign.h"
#if WITH_EDITOR
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraActor.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
void TickDouliSmokeTest(AStoryFirstPersonCharacter* P)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("DouliTest"))) return;
    static float Start=-1, Changed=0;
    static int32 Step=0;
    static bool bWindupShot=false, bReleaseShot=false;
    static TWeakObjectPtr<ASeniorDouliTestTarget> Target;
    const float Now=P->GetWorld()->GetTimeSeconds();
    if(Start<0) Start=Now;
    auto Check=[](bool B,const TCHAR* Text) {
        if(!B) { UE_LOG(LogTemp,Error,TEXT("DOULI_TEST_FAILED: %s"),Text); FPlatformMisc::RequestExitWithStatus(false,1); }
        return B;
    };
    if(!Check(Now-Start<65,TEXT("test timeout"))) return;
    if(!P->Douli || !P->GetController()) return;
    // Interactive input must not compete with this explicit automation sequence.
    if(Step==0) P->DisableInput(Cast<APlayerController>(P->GetController()));
    auto* W=P->Douli.Get();
    auto CheckGrip=[&]() -> bool {
        const auto* Body=P->GetBraxtonBodyMesh();
        if(!Body || !Body->GetSkeletalMeshAsset() || !P->FirstPersonArms->GetSkeletalMeshAsset())
            return Check(false,TEXT("Braxton's hand meshes are unavailable"));
        const FVector Rim=W->Hat->GetComponentTransform().TransformPosition(-FSeniorDouliIdle::HatOffset());
        const float WorldGap=FVector::Dist(Rim,Body->GetSocketLocation(TEXT("hand_r")));
        const float FirstPersonGap=FVector::Dist(P->DouliGrip(true).GetLocation(),
            P->FirstPersonArms->GetSocketLocation(TEXT("hand_r")));
        UE_LOG(LogTemp,Display,TEXT("DOULI_GRIP_CHECK world_rim_to_hand=%.1f cm fp_center_to_hand=%.1f cm phase=%d"),
            WorldGap,FirstPersonGap,int32(W->Phase));
        return Check(WorldGap<30.f && FirstPersonGap<25.f,
            TEXT("hat rim and animated hands separated during held pose"));
    };
    auto Shot=[](const TCHAR* Name) {
        const FString File=FParse::Param(FCommandLine::Get(),TEXT("DouliBodyTest")) ? FString(Name).Replace(TEXT("Douli_"),TEXT("DouliBody_")) : FString(Name);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/File,false,false);
    };
    auto MakeTarget=[&](float Distance,bool Enemy) {
        Target=P->GetWorld()->SpawnActor<ASeniorDouliTestTarget>();
        auto* Box=NewObject<UBoxComponent>(Target.Get());Target->AddInstanceComponent(Box);Target->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10,75,75));Box->SetCollisionProfileName(TEXT("BlockAll"));Box->RegisterComponent();
        if(Enemy)Target->Tags.Add(TEXT("Enemy"));
        Target->SetActorLocation(P->GetPawnViewLocation()+FVector(Distance,0,-12));
    };
    if(Step==0 && Now-Start>6)
    {
        P->SetActorLocation(FVector(0,0,8000),false,nullptr,ETeleportType::TeleportPhysics);
        P->GetController()->SetControlRotation(FRotator::ZeroRotator);
        P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        if(FParse::Param(FCommandLine::Get(),TEXT("DouliVisualTest")))
        {
            // Neutral test-only panels make the hat and both hands readable
            // against the otherwise black collision arena. They have no
            // collision, cannot affect the projectile, and never save to a map.
            UStaticMesh* PanelMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
            if(PanelMesh)
            {
                for(const FVector Offset : {FVector(-330,285,105),FVector(850,0,105)})
                {
                    auto* Panel=P->GetWorld()->SpawnActor<AStaticMeshActor>(P->GetActorLocation()+Offset,
                        FRotator::ZeroRotator);
                    Panel->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                    Panel->GetStaticMeshComponent()->SetStaticMesh(PanelMesh);
                    Panel->GetStaticMeshComponent()->SetWorldScale3D(FVector(.08f,16.f,8.f));
                    Panel->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    Panel->GetStaticMeshComponent()->SetCastShadow(false);
                }
            }
            for(const TPair<FVector,float>& Key : {TPair<FVector,float>(FVector(155,15,145),18000.f),
                                                   TPair<FVector,float>(FVector(220,-170,125),22000.f),
                                                   TPair<FVector,float>(FVector(700,0,165),20000.f)})
            {
                auto* Light=P->GetWorld()->SpawnActor<APointLight>(P->GetActorLocation()+Key.Key,
                    FRotator::ZeroRotator);
                Light->PointLightComponent->SetMobility(EComponentMobility::Movable);
                Light->PointLightComponent->SetIntensity(Key.Value);
                Light->PointLightComponent->SetAttenuationRadius(1100.f);
                Light->PointLightComponent->SetCastShadows(false);
            }
        }
        if(FParse::Param(FCommandLine::Get(),TEXT("DouliBodyTest")))
        {
            const FVector Position=P->GetActorLocation()+FVector(280,-260,65);
            auto* Camera=P->GetWorld()->SpawnActor<ACameraActor>(Position,(P->GetActorLocation()+FVector(0,0,10)-Position).Rotation());
            CastChecked<APlayerController>(P->GetController())->SetViewTarget(Camera);
        }
        MakeTarget(600,true);Changed=Now;Step=1;
    }
    else if(Step==1 && Now-Changed>3)
    {
        Shot(TEXT("Douli_Held.png"));
        if(!Check(W->Hat->GetStaticMesh()!=nullptr,TEXT("hat mesh missing")))return;
        if(!CheckGrip())return;
        if(!Check(W->BeginThrow() && !W->BeginThrow(),TEXT("must reject duplicate throw during windup")))return;
        Changed=Now;Step=2;
    }
    else if(Step==2 && W->Phase==EDouliPhase::Windup && W->GetPhaseAge()>.14f && !bWindupShot)
    { if(!CheckGrip())return; Shot(TEXT("Douli_Windup.png")); bWindupShot=true; }
    else if(Step==2 && W->Phase==EDouliPhase::Outbound && W->GetPhaseAge()<.09f && !bReleaseShot)
    { Shot(TEXT("Douli_Release.png")); bReleaseShot=true; }
    else if(Step==2 && W->Phase==EDouliPhase::Returning)
    {
        if(!Check(FMath::IsNearlyEqual(Target->ReceivedDamage,35.f),TEXT("enemy hit must apply damage exactly once")))return;
        Shot(TEXT("Douli_Return.png"));Changed=Now;Step=3;
    }
    else if(Step==3 && W->Phase==EDouliPhase::Ready)
    {
        if(!Check(FMath::IsNearlyEqual(Target->ReceivedDamage,35.f),TEXT("return flight must not damage again")))return;
        Target->Destroy();Target.Reset();
        if(!Check(W->BeginThrow(),TEXT("catch must rearm")))return;
        Changed=Now;Step=4;
    }
    else if(Step==4 && W->Phase==EDouliPhase::Outbound && W->GetPhaseAge()>.28f)
    { Shot(TEXT("Douli_Throw.png"));Step=5; }
    else if(Step==5 && W->Phase==EDouliPhase::Returning)
    { P->AddActorWorldOffset(FVector(0,300,0));Step=6; }
    else if(Step==6 && W->Phase==EDouliPhase::Ready)
    {
        if(!Check(Now-Changed<7,TEXT("miss recall or moving-owner catch failed")))return;
        Shot(TEXT("Douli_Caught.png"));Changed=Now;Step=7;
    }
    else if(Step==7 && Now-Changed>1)
    { MakeTarget(45,false);W->BeginThrow();Changed=Now;Step=8; }
    else if(Step==8 && Now-Changed>1.5f && W->Phase==EDouliPhase::Ready)
    {
        if(!Check(Target->ReceivedDamage==0,TEXT("wall incorrectly received enemy damage")))return;
        Target->Destroy();MakeTarget(35,true);
        W->BeginThrow();Changed=Now;Step=9;
    }
    else if(Step==9 && Now-Changed>1.5f && W->Phase==EDouliPhase::Ready)
    {
        if(!Check(FMath::IsNearlyEqual(Target->ReceivedDamage,35.f),TEXT("point blank enemy must receive one hit")))return;
        auto* State=P->GetPlayerState<ASeniorLobbyPlayerState>();State->LoadoutIndex=1;
        if(!Check(!W->BeginThrow(),TEXT("unequipped slot allowed throw")))return;
        Target->Destroy();Step=10;
        UE_LOG(LogTemp,Display,TEXT("DOULI_TEST_PASSED: mesh, one-hat gate, enemy damage, harmless recall, miss return, moving catch, wall return, point blank hit and loadout gate"));
        FPlatformMisc::RequestExitWithStatus(false,0);
    }
}
#endif
