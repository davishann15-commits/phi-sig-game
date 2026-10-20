#include "HouseBasketball.h"
#if !UE_BUILD_SHIPPING
#include "HouseBasketballCourt.h"
#include "HouseFurnitureInteractionComponent.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"

void TickHouseBasketballReview(AStoryFirstPersonCharacter* Pawn)
{
    bool Review=FParse::Param(FCommandLine::Get(),TEXT("HouseBasketballReview"));
    bool Manual=FParse::Param(FCommandLine::Get(),TEXT("HouseCourtPlaytest"));
    if((!Review && !Manual) || !Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* W=Pawn->GetWorld();if(!W || UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    auto* PC=Cast<APlayerController>(Pawn->GetController());auto* Mover=Pawn->FurnitureInteraction.Get();if(!PC||!Mover)return;
    if(Manual && !Review)
    {
        // Convenience starting location only; E, aiming and shooting remain
        // entirely under the person playing. Never changes the saved PlayerStart.
        static TWeakObjectPtr<UWorld> Placed;
        if(Placed.Get()!=W && W->GetTimeSeconds()>8)
        {
            TActorIterator<AHouseBasketball> It(W);
            if(It)
            {
                FVector At=It->Body->GetComponentLocation();
                Pawn->SetActorLocation(FVector(At.X,At.Y-90,HouseCourt::Floor+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);
                Pawn->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((At-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
                if(auto* G=UGameUserSettings::GetGameUserSettings()){G->SetOverallScalabilityLevel(2);G->ApplyNonResolutionSettings();}
                Placed=W;UE_LOG(LogTemp,Display,TEXT("COURT_MANUAL_READY"));
            }
        }
        return;
    }
    struct FState {TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AHouseBasketball> Ball;int Phase=0,End=0,Way=1;double Start=0,Total=0;bool Falling=false,Charging=false;FString Dir,Log;FVector Last=FVector::ZeroVector;};
    static FState S;double Now=W->GetTimeSeconds();
    auto Finish=[&](bool Pass,const FString& Why){S.Phase=-1;S.Log+=(Pass?TEXT("PASS "):TEXT("FAIL "))+Why+TEXT("\n");FFileHelper::SaveStringToFile(S.Log,*FPaths::Combine(S.Dir,TEXT("result.txt")));UE_LOG(LogTemp,Display,TEXT("COURT_REVIEW_%s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Why);FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);};
    auto Note=[&](const FString& Text){S.Log+=Text+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("COURT_CHECK %s"),*Text);};
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,Name),false,false);};
    if(S.World.Get()!=W){S=FState();S.World=W;S.Total=S.Start=Now;S.Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("BasketballReview"));IFileManager::Get().MakeDirectory(*S.Dir,true);if(auto* G=UGameUserSettings::GetGameUserSettings()){G->SetOverallScalabilityLevel(2);G->ApplyNonResolutionSettings();}}
    if(S.Phase<0)return;
    if(Now-S.Total>100){Finish(false,TEXT("Timed out"));return;}
    if(S.Phase==0)
    {
        if(Now-S.Start<7)return;
        int Count=0;float Closest=BIG_NUMBER;
        for(TActorIterator<AHouseBasketball> It(W);It;++It){++Count;float Dist=FVector::DistSquared(It->Body->GetComponentLocation(),HouseCourt::BallHome());if(Dist<Closest){Closest=Dist;S.Ball=*It;}}
        auto* Ball=S.Ball.Get();if(Count!=3 || !Ball || !Ball->Body->IsSimulatingPhysics()){Finish(false,TEXT("Expected three shared simulated basketballs"));return;}
        if(FMath::Abs(Ball->Body->Bounds.BoxExtent.GetMax()-12.4f)>1.f){Finish(false,TEXT("Basketball is not regulation-sized"));return;}
        Note(TEXT("Three replicated basketballs; primary ball has spherical collision and CCD"));
        FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtCollisionReview),false,Pawn);Q.AddIgnoredActor(Ball);
        for(float X:{-950.f,-400.f,0.f,400.f,950.f})for(float Y:{2120.f,2700.f,3460.f})
        {
            FHitResult H;if(!W->LineTraceSingleByChannel(H,FVector(X,Y,HouseCourt::Floor+70),FVector(X,Y,HouseCourt::Floor-30),ECC_Pawn,Q)||FMath::Abs(H.ImpactPoint.Z-HouseCourt::Floor)>2){Finish(false,TEXT("Court floor collision gap"));return;}
        }
        for(auto P:{FVector(-1490,2800,HouseCourt::Floor+100),FVector(1490,2800,HouseCourt::Floor+100),FVector(0,3820,HouseCourt::Floor+100)})
        {
            FVector D=P.Y>3800?FVector(0,90,0):FVector(90,0,0);FHitResult H;if(!W->SweepSingleByChannel(H,P-D,P+D,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(25),Q)){Finish(false,TEXT("Retaining wall does not block"));return;}
        }
        for(int I=0;I<2;++I)
        {
            FVector R=HouseCourt::Rim(I);FHitResult H;
            if(W->SweepSingleByChannel(H,R+FVector(0,0,75),R-FVector(0,0,75),FQuat::Identity,ECC_WorldDynamic,FCollisionShape::MakeSphere(12.4f),Q)){Finish(false,TEXT("Basket opening is sealed"));return;}
            FVector D(I==0?-1:1,0,0);
            if(!W->SweepSingleByChannel(H,R+FVector(0,0,55),R+D*60+FVector(0,0,55),FQuat::Identity,ECC_WorldDynamic,FCollisionShape::MakeSphere(12.4f),Q)){Finish(false,TEXT("Backboard does not collide"));return;}
        }
        Note(TEXT("15 floor samples, 3 retaining walls, both open rims and both colliding backboards passed"));
        Ball->Body->SetWorldLocation(HouseCourt::BallHome()+FVector(0,0,220),false,nullptr,ETeleportType::TeleportPhysics);Ball->Body->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->Body->WakeAllRigidBodies();S.Phase=1;S.Start=Now;return;
    }
    auto* Ball=S.Ball.Get();if(!Ball){Finish(false,TEXT("Ball vanished"));return;}
    if(S.Phase==1)
    {
        float V=Ball->Body->GetPhysicsLinearVelocity().Z;
        if(V<-100)S.Falling=true;
        if(S.Falling && V>150){Note(FString::Printf(TEXT("Real Chaos bounce passed: upward rebound %.0f cm/s"),V));S.Phase=2;S.Start=Now;return;}
        if(Now-S.Start>3){Finish(false,TEXT("Ball did not rebound"));return;}return;
    }
    if(S.Phase==2)
    {
        if(Now-S.Start<2)return;
        FVector At=Ball->Body->GetComponentLocation();float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Pawn->SetActorLocation(FVector(At.X,At.Y-60,HouseCourt::Floor+Half+2),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->SetControlRotation((At-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=3;S.Start=Now;return;
    }
    if(S.Phase==3)
    {
        if(Now-S.Start<.4)return;
        PC->SetControlRotation((Ball->Body->GetComponentLocation()-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if(Mover->FocusedFurnitureForInteraction()!=Ball)
        {
            if(Now-S.Start<3)return;
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(CourtFocusDiagnostic),false,Pawn);
            FVector V=Pawn->FirstPersonCamera->GetComponentLocation();W->LineTraceSingleByChannel(H,V,V+Pawn->GetBaseAimRotation().Vector()*Mover->GrabRange,ECC_Visibility,Q);
            Note(FString::Printf(TEXT("focus hit=%s/%s cam=%s ball=%s range=%.1f distance=%.1f scale=%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*V.ToString(),*Ball->Body->GetComponentLocation().ToString(),Mover->GrabRange,FVector::Dist(V,Ball->Body->GetComponentLocation()),*Ball->GetActorScale3D().ToString()));
            Shot(TEXT("PickupIssue.png"));Finish(false,TEXT("E focus does not find the ball"));return;
        }
        Mover->ToggleGrab();S.Phase=4;S.Start=Now;return;
    }
    if(S.Phase==4)
    {
        if(Now-S.Start<1.2)return;
        if(!Mover->bHoldingFurniture || Mover->GetHeldBody()!=Ball->Body){Finish(false,TEXT("Normal E pickup failed"));return;}
        PC->SetControlRotation((HouseCourt::Rim(S.End)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        Note(FString::Printf(TEXT("Normal carry pickup passed for shot %d"),S.End));S.Phase=5;S.Start=Now;return;
    }
    if(S.Phase==5)
    {
        if(Now-S.Start<.8)return;
        PC->SetControlRotation((HouseCourt::Rim(S.End)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if(!S.Charging){Mover->BeginBasketballShot();S.Charging=true;return;}
        if(Mover->GetBasketballPower()<Mover->GetBasketballIdealPower())return;
        Shot(S.End==0?TEXT("BallHeld.png"):TEXT("SecondBasketAim.png"));Mover->ReleaseBasketballShot();S.Charging=false;S.Phase=6;S.Start=Now;return;
    }
    if(S.Phase==6)
    {
        if(Now-S.Start<.6)return;
        if(Mover->bHoldingFurniture){Finish(false,TEXT("Shoot did not release the grip"));return;}
        if((S.End==0?Ball->LeftBaskets:Ball->RightBaskets)>0)
        {
            Note(FString::Printf(TEXT("Shot %d crossed the actual rim downward using physics"),S.End));
            Shot(S.End==0?TEXT("LeftBasketScored.png"):TEXT("RightBasketScored.png"));
            if(S.End++==0){S.Phase=2;S.Start=Now;return;}
            // Lose only the ball, then verify recovery restores this same actor.
            Ball->Body->SetWorldLocation(FVector(0,2820,-1450),false,nullptr,ETeleportType::TeleportPhysics);S.Phase=7;S.Start=Now;return;
        }
        if(Now-S.Start>7){Note(TEXT("Shot end ")+FString::FromInt(S.End)+TEXT(" ball ")+Ball->Body->GetComponentLocation().ToString());Finish(false,TEXT("A normal aimed shot missed the basket"));return;}return;
    }
    if(S.Phase==7)
    {
        if(Now-S.Start<.2)return;
        if(FVector::Dist2D(Ball->Body->GetComponentLocation(),HouseCourt::BallHome())>30 || Ball->Body->GetComponentLocation().Z<HouseCourt::Floor){Finish(false,TEXT("Lost ball recovery failed"));return;}
        Note(TEXT("Lost ball returned without spawning duplicates"));
        // Walk from the existing arcade onto the court and back, with real movement.
        Pawn->SetActorLocation(FVector(-380,1750,HouseCourt::Floor+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);S.Phase=8;S.Start=Now;S.Way=0;return;
    }
    if(S.Phase==8)
    {
        const FVector Goals[]={FVector(-380,3400,HouseCourt::Floor),FVector(800,3400,HouseCourt::Floor),FVector(800,2200,HouseCourt::Floor),FVector(-380,1750,HouseCourt::Floor)};
        FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());FVector To=Goals[S.Way]-Feet;
        if(To.Size2D()<25){if(++S.Way==4){Finish(true,TEXT("Court route, bounces, normal pickup, shots through both baskets, and lost-ball recovery passed. No network client was part of this check."));return;}S.Start=Now;return;}
        if(Now-S.Start>20 || FMath::Abs(Feet.Z-HouseCourt::Floor)>35){Finish(false,TEXT("Porch-to-court route is blocked or has a floor gap"));return;}
        To.Z=0;Pawn->AddMovementInput(To.GetSafeNormal(),1);PC->SetControlRotation(To.Rotation());
    }
}
#else
void TickHouseBasketballReview(AStoryFirstPersonCharacter*) {}
#endif
