#include "SeniorSauce.h"
#include "StoryCampaign.h"
#include "SeniorLobby.h"
#if WITH_EDITOR
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "UnrealClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

// Editor-only gameplay check. Launch Chapter01 with -SauceTest; its save slot
// and high-altitude collision fixture do not touch the player's campaign.
void TickSauceSmokeTest(AStoryFirstPersonCharacter* Pawn)
{
    if(FParse::Param(FCommandLine::Get(),TEXT("SauceVisualHouse")))
    {
        if(!Pawn->HasAuthority())return;
        static int32 VisualStep=0;
        static float VisualChanged=0.f;
        const float VisualNow=Pawn->GetWorld()->GetTimeSeconds();
        auto* VisualState=Pawn->GetPlayerState<ASeniorLobbyPlayerState>();
        if(!VisualState || !Pawn->GetController())return;
        if(VisualStep==0)
        {
            VisualState->CharacterIndex=0;VisualState->LoadoutIndex=1;
            VisualChanged=VisualNow;VisualStep=1;
        }
        else if(VisualStep==1 && Pawn->SaucePacket && VisualNow-VisualChanged>2.f)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Sauce_InHouse.png"),false,false);
            VisualChanged=VisualNow;VisualStep=2;
        }
        else if(VisualStep==2 && VisualNow-VisualChanged>1.f)
        {
            UE_LOG(LogTemp,Display,TEXT("SAUCE_HOUSE_VISUAL_PASSED: held packet captured in Chapter 1 lighting"));
            FPlatformMisc::RequestExitWithStatus(false,0);
            VisualStep=3;
        }
        return;
    }
    if(!FParse::Param(FCommandLine::Get(),TEXT("SauceTest")) || !Pawn->HasAuthority())return;
    static TWeakObjectPtr<ACharacter> Senior;
    static TWeakObjectPtr<ASeniorSaucePuddle> Puddle;
    static int32 Step=0;
    static float Began=-1.f, Changed=0.f;
    const float Now=Pawn->GetWorld()->GetTimeSeconds();
    if(Began<0.f)Began=Now;
    auto Check=[](bool Passed,const TCHAR* Message)
    {
        if(!Passed)
        {
            UE_LOG(LogTemp,Error,TEXT("SAUCE_TEST_FAILED: %s"),Message);
            FPlatformMisc::RequestExitWithStatus(false,1);
        }
        return Passed;
    };
    if(!Check(Now-Began<28.f,TEXT("timed out")))return;
    auto* State=Pawn->GetPlayerState<ASeniorLobbyPlayerState>();
    if(!State || !Pawn->GetController())return;
    if(Step==0)
    {
        State->CharacterIndex=0;State->LoadoutIndex=1;
        Pawn->SetActorLocation(FVector(0,0,8000),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetController()->SetControlRotation(FRotator::ZeroRotator);
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        auto* Floor=Pawn->GetWorld()->SpawnActor<AStaticMeshActor>(FVector(1000,0,7900),FRotator::ZeroRotator);
        auto* Mesh=Floor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetWorldScale3D(FVector(30,10,.2f));
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        // Match the hat test's neutral, movable light rig so the packet can
        // be reviewed visually outside the campaign without touching its map.
        for(const FVector Offset : {FVector(-330,285,105),FVector(850,0,105)})
        {
            auto* Panel=Pawn->GetWorld()->SpawnActor<AStaticMeshActor>(Pawn->GetActorLocation()+Offset,FRotator::ZeroRotator);
            auto* PanelMesh=Panel->GetStaticMeshComponent();
            PanelMesh->SetMobility(EComponentMobility::Movable);
            PanelMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            PanelMesh->SetWorldScale3D(FVector(.08f,16.f,8.f));
            PanelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            PanelMesh->SetCastShadow(false);
        }
        for(const TPair<FVector,float>& Key : {TPair<FVector,float>(FVector(155,15,145),18000.f),
                                               TPair<FVector,float>(FVector(220,-170,125),22000.f),
                                               TPair<FVector,float>(FVector(700,0,165),20000.f)})
        {
            if(auto* Light=Pawn->GetWorld()->SpawnActor<APointLight>(Pawn->GetActorLocation()+Key.Key,FRotator::ZeroRotator))
            {
                if(auto* Bulb=Cast<UPointLightComponent>(Light->GetLightComponent()))
                {
                    Bulb->SetMobility(EComponentMobility::Movable);
                    Bulb->SetIntensity(Key.Value);
                    Bulb->SetAttenuationRadius(1100.f);
                    Bulb->SetCastShadows(false);
                }
            }
        }
        Changed=Now;Step=1;
    }
    else if(Step==1 && Now-Changed>.5f)
    {
        if(!Pawn->SaucePacket)return;
        if(!Check(!Pawn->Douli && Pawn->SaucePacket->Phase==ESeniorSaucePhase::Ready,
            TEXT("second loadout did not equip sauce instead of douli")))return;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Sauce_FirstPerson.png"),false,false);
        Changed=Now;Step=11;
    }
    else if(Step==11 && Now-Changed>.5f)
    {
        Pawn->ServerThrowEquippedWeapon();
        if(!Check(Pawn->SaucePacket->Phase==ESeniorSaucePhase::Flying,
            TEXT("fire control did not throw sauce")))return;
        if(!Check(!Pawn->SaucePacket->BeginThrow(),TEXT("duplicate throw was accepted")))return;
        Changed=Now;Step=2;
    }
    else if(Step==2)
    {
        TArray<AActor*> Puddles;
        UGameplayStatics::GetAllActorsOfClass(Pawn->GetWorld(),ASeniorSaucePuddle::StaticClass(),Puddles);
        if(Puddles.IsEmpty())return;
        Puddle=Cast<ASeniorSaucePuddle>(Puddles[0]);
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Senior=Pawn->GetWorld()->SpawnActor<ACharacter>(Puddle->GetActorLocation()+FVector(500,0,92),FRotator::ZeroRotator,Params);
        if(!Check(Senior.IsValid(),TEXT("enemy fixture not spawned")))return;
        Senior->Tags.Add(TEXT("Enemy"));
        Senior->GetCharacterMovement()->MaxWalkSpeed=600.f;
        Senior->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        Senior->SetActorLocation(Puddle->GetActorLocation()+FVector(0,0,92),false,nullptr,ETeleportType::TeleportPhysics);
        Changed=Now;Step=3;
    }
    else if(Step==3 && Now-Changed>.25f)
    {
        if(!Check(FMath::IsNearlyEqual(Senior->GetCharacterMovement()->MaxWalkSpeed,270.f,1.f),
            TEXT("enemy did not slow to 45 percent inside sauce")))return;
        Senior->SetActorLocation(Puddle->GetActorLocation()+FVector(500,0,92),false,nullptr,ETeleportType::TeleportPhysics);
        Changed=Now;Step=4;
    }
    else if(Step==4 && Now-Changed>.25f)
    {
        if(!Check(FMath::IsNearlyEqual(Senior->GetCharacterMovement()->MaxWalkSpeed,600.f,1.f),
            TEXT("enemy speed not restored after leaving sauce")))return;
        Senior->SetActorLocation(Puddle->GetActorLocation()+FVector(0,0,92),false,nullptr,ETeleportType::TeleportPhysics);
        Changed=Now;Step=5;
    }
    else if(Step==5 && Now-Changed>.25f)
    {
        if(!Check(FMath::IsNearlyEqual(Senior->GetCharacterMovement()->MaxWalkSpeed,270.f,1.f),
            TEXT("enemy did not slow when re-entering sauce")))return;
        if(!Puddle.IsValid())
        {
            if(!Check(FMath::IsNearlyEqual(Senior->GetCharacterMovement()->MaxWalkSpeed,600.f,1.f),
                TEXT("enemy speed not restored when puddle expired")))return;
        }
        Changed=Now;Step=6;
    }
    else if(Step==6 && !Puddle.IsValid())
    {
        if(!Check(FMath::IsNearlyEqual(Senior->GetCharacterMovement()->MaxWalkSpeed,600.f,1.f),
            TEXT("puddle expiration did not restore enemy speed")))return;
        if(!Check(Pawn->SaucePacket && Pawn->SaucePacket->Phase==ESeniorSaucePhase::Ready,
            TEXT("packet did not rearm after cooldown")))return;
        // Request a late frame after shader compilation so the material review
        // does not mistake the engine's temporary checker fallback for art.
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Sauce_LateMaterial.png"),false,false);
        Changed=Now;
        Step=7;
    }
    else if(Step==7 && Now-Changed>.5f)
    {
        if(!Check(Pawn->SaucePacket->BeginThrow(),TEXT("rearmed packet could not be thrown")))return;
        UE_LOG(LogTemp,Display,TEXT("SAUCE_TEST_PASSED: second loadout, fire control, one-packet gate, puddle, slow, leave/re-enter, expiration and cooldown"));
        FPlatformMisc::RequestExitWithStatus(false,0);
        Step=8;
    }
}
#endif
