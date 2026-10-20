// Explicit opt-in actual campaign collision, walking, door and loose-prop checks.
#include "HouseRearPorch.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseBasketball.h"
#include "HouseFurnitureInteractionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void TickHouseRearPorchReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("HouseRearPorchReview")) || !Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled())return;
    UWorld* W=Pawn->GetWorld();auto* PC=Cast<APlayerController>(Pawn->GetController());auto* Mover=Pawn->FurnitureInteraction.Get();if(!PC||!Mover)return;
    struct FState {TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AHouseCourtDoor> Door;TWeakObjectPtr<AHouseCourtProp> Football,Die;int Phase=0;double Start=0,Step=0;FString Dir,Log;};
    static FState S;const double Now=W->GetTimeSeconds();
    if(S.World.Get()!=W){S=FState();S.World=W;S.Start=S.Step=Now;S.Dir=FPaths::ProjectSavedDir()/TEXT("RearPorchReview");IFileManager::Get().MakeDirectory(*S.Dir,true);}
    auto Note=[&](const FString& T){S.Log+=T+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("REAR_PORCH_CHECK %s"),*T);};
    auto Finish=[&](bool Pass,const FString& Why){Note((Pass?TEXT("PASS "):TEXT("FAIL "))+Why);FFileHelper::SaveStringToFile(S.Log,*(S.Dir/TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);};
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(S.Dir/Name,false,false);};
    auto Place=[&](FVector At){Pawn->SetActorLocation(At,false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);};
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    auto Sweep=[&](FVector A,FVector B,FHitResult& H){FCollisionQueryParams Q(SCENE_QUERY_STAT(RearPorchCapsule),false,Pawn);return W->SweepSingleByChannel(H,A,B,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);};
    if(S.Phase<0)return;if(Now-S.Start>100){Finish(false,TEXT("Timed out"));return;}
    if(S.Phase==0)
    {
        if(Now-S.Step<8)return;
        int Doors=0,Football=0,Dice=0,Volleyball=0,Balls=0;
        for(TActorIterator<AHouseCourtDoor> It(W);It;++It){S.Door=*It;++Doors;}
        for(TActorIterator<AHouseBasketball> It(W);It;++It)++Balls;
        for(TActorIterator<AHouseCourtProp> It(W);It;++It)
        {
            if(!It->Body->GetStaticMesh()||!It->Body->IsSimulatingPhysics()){Finish(false,TEXT("Loose prop lacks mesh or real physics"));return;}
            if(It->Item==EHouseCourtItem::Football){++Football;S.Football=*It;}
            if(It->Item==EHouseCourtItem::Die){++Dice;S.Die=*It;}
            if(It->Item==EHouseCourtItem::Volleyball)++Volleyball;
        }
        if(Doors!=1||Balls!=3||Football!=1||Dice!=2||Volleyball!=1){Finish(false,TEXT("Court prop inventory incomplete"));return;}
        Note(TEXT("One usable door, three basketballs, football, volleyball and two separate dice found"));
        FCollisionQueryParams Q(SCENE_QUERY_STAT(RearPorchGround),false,Pawn);FHitResult H;
        for(float X:{-1000.f,-500.f,0.f,500.f,1000.f})for(float Y:{1740.f,1820.f,1890.f,2060.f})
            if(!W->LineTraceSingleByChannel(H,FVector(X,Y,HouseRear::Floor+40),FVector(X,Y,HouseRear::Floor-10),ECC_Pawn,Q)||FMath::Abs(H.ImpactPoint.Z-HouseRear::Floor)>1.5f)
            {Note(FString::Printf(TEXT("floor x=%.0f y=%.0f z=%.1f actor=%s/%s"),X,Y,H.ImpactPoint.Z,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent())));Finish(false,TEXT("Concrete route has a collision gap"));return;}
        Note(TEXT("20 floor samples across the former grass strip and court: continuous level concrete"));
        FVector G=HouseRear::GateCenter();G.Z=HouseRear::Floor+Half+2;
        if(!Sweep(G-FVector(120,0,0),G+FVector(120,0,0),H)||H.GetActor()!=S.Door.Get()){Finish(false,TEXT("Closed kitchen door does not block the capsule"));return;}
        if(!Sweep(FVector(-1650,1845,HouseRear::Floor+Half+2),FVector(-1330,1845,HouseRear::Floor+Half+2),H)){Finish(false,TEXT("Library-side wall gap remains open"));return;}
        Note(TEXT("Closed kitchen door and new library-side wall block a standing player"));
        Place(FVector(340,1340,HouseRear::Floor+Half+2));PC->SetControlRotation(FRotator(0,90,0));S.Phase=1;S.Step=Now;return;
    }
    if(S.Phase==1||S.Phase==2)
    {
        const float Dir=S.Phase==1?1.f:-1.f,Goal=S.Phase==1?2180.f:1340.f;
        Pawn->AddMovementInput(FVector(0,Dir,0),1.f);
        if(Dir*(Pawn->GetActorLocation().Y-Goal)>-12)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();Note(S.Phase==1?TEXT("Actual player walked from covered porch through arch onto court"):TEXT("Actual player walked back under porch, clear of bar and stages"));
            Shot(S.Phase==1?TEXT("ConcreteRouteOut.png"):TEXT("ConcreteRouteReturn.png"));++S.Phase;S.Step=Now;
        }
        else if(Now-S.Step>8){Note(Pawn->GetActorLocation().ToString());Finish(false,TEXT("Player caught on porch-to-court route"));}
        return;
    }
    if(S.Phase==3)
    {
        Place(FVector(1660,1845,HouseRear::Floor+Half+2));PC->SetControlRotation((HouseRear::GateCenter()-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=4;S.Step=Now;return;
    }
    if(S.Phase==4)
    {
        if(Now-S.Step<.3f)return;
        if(Pawn->FocusedDoorForInteraction()!=S.Door.Get()){Finish(false,TEXT("Normal door targeting cannot find the court door"));return;}
        if(!S.Door->Interact(Pawn)){Finish(false,TEXT("Court door refuses normal interaction"));return;}
        Note(TEXT("Normal E-door targeting and server interaction accepted"));S.Phase=5;S.Step=Now;return;
    }
    if(S.Phase==5)
    {
        if(Now-S.Step<.8)return;FHitResult H;FVector G=HouseRear::GateCenter();G.Z=HouseRear::Floor+Half+2;
        if(Sweep(G-FVector(120,0,0),G+FVector(120,0,0),H)){Note(GetNameSafe(H.GetActor())+TEXT(" / ")+GetNameSafe(H.GetComponent()));Finish(false,TEXT("Open door leaves an invisible blocker"));return;}
        Shot(TEXT("KitchenCourtDoorOpen.png"));S.Phase=6;S.Step=Now;return;
    }
    if(S.Phase==6||S.Phase==7)
    {
        const float Dir=S.Phase==6?-1.f:1.f,Goal=S.Phase==6?1290.f:1680.f;
        Pawn->AddMovementInput(FVector(Dir,0,0),1.f);
        if(Dir*(Pawn->GetActorLocation().X-Goal)>-12)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();Note(S.Phase==6?TEXT("Walked kitchen-side entrance -> court through open door"):TEXT("Walked court -> kitchen-side entrance through open door"));++S.Phase;S.Step=Now;
        }
        else if(Now-S.Step>6){Note(Pawn->GetActorLocation().ToString());Finish(false,TEXT("Door entrance walking failed"));}return;
    }
    if(S.Phase==8)
    {
        if(!S.Door->Interact(Pawn)){Finish(false,TEXT("Door cannot be closed after leaving"));return;}S.Phase=9;S.Step=Now;return;
    }
    if(S.Phase==9)
    {
        if(Now-S.Step<.8)return;FHitResult H;FVector G=HouseRear::GateCenter();G.Z=HouseRear::Floor+Half+2;
        if(!Sweep(G-FVector(120,0,0),G+FVector(120,0,0),H)||H.GetActor()!=S.Door.Get()){Finish(false,TEXT("Reclosed door lost collision"));return;}
        Note(TEXT("Door closes smoothly and restores its visible collision"));
        FVector At=S.Football->Body->GetComponentLocation();Place(FVector(At.X,At.Y-68,HouseRear::Floor+Half+2));PC->SetControlRotation((At-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=10;S.Step=Now;return;
    }
    if(S.Phase==10)
    {
        if(Now-S.Step<.4)return;PC->SetControlRotation((S.Football->Body->GetComponentLocation()-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if(Mover->FocusedFurnitureForInteraction()!=S.Football.Get()){Finish(false,TEXT("Football cannot be targeted for normal pickup"));return;}
        Mover->ToggleGrab();S.Phase=11;S.Step=Now;return;
    }
    if(S.Phase==11)
    {
        if(Now-S.Step<1)return;
        if(!Mover->bHoldingFurniture||Mover->GetHeldBody()!=S.Football->Body){Finish(false,TEXT("Normal football pickup failed"));return;}
        Note(TEXT("Football is held using the real one-hand carry system"));PC->SetControlRotation(FRotator(0,180,0));S.Phase=16;S.Step=Now;return;
    }
    if(S.Phase==16)
    {
        if(Now-S.Step<1)return;
        FVector2D Pixel;int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
        if(!PC->ProjectWorldLocationToScreen(S.Football->Body->Bounds.Origin,Pixel)
            ||Pixel.X<0||Pixel.X>Width||Pixel.Y<0||Pixel.Y>Height-24)
        {Finish(false,TEXT("Football resting grip is outside the view"));return;}
        Note(TEXT("Football resting grip is visibly inside the camera frame"));
        Shot(TEXT("FootballHeld.png"));S.Phase=17;S.Step=Now;return;
    }
    if(S.Phase==17)
    {
        // Let the renderer capture the settled grip before starting the throw.
        if(Now-S.Step<.3)return;
        PC->SetControlRotation(FRotator(12,180,0));Mover->ThrowHeldFurniture();S.Phase=12;S.Step=Now;return;
    }
    if(S.Phase==12)
    {
        if(Now-S.Step<.6)return;
        if(Mover->bHoldingFurniture||!S.Football->Body->IsSimulatingPhysics()){Finish(false,TEXT("Football throw did not release a physics body"));return;}
        Note(TEXT("Normal throw released the football with real physics"));
        const FVector At=S.Die->Body->GetComponentLocation();
        if(FMath::Abs(At.Z-(HouseRear::Floor+97.8f))>2){Finish(false,TEXT("Die does not rest on the table"));return;}
        Place(FVector(At.X,HouseRear::TableCenter().Y-126,HouseRear::Floor+Half+2));PC->SetControlRotation((At-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=13;S.Step=Now;return;
    }
    if(S.Phase==13)
    {
        if(Now-S.Step<.4)return;PC->SetControlRotation((S.Die->Body->GetComponentLocation()-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if(Mover->FocusedFurnitureForInteraction()!=S.Die.Get()){Finish(false,TEXT("Die cannot be picked up from the table"));return;}
        Mover->ToggleGrab();S.Phase=14;S.Step=Now;return;
    }
    if(S.Phase==14)
    {
        if(Now-S.Step<1)return;
        if(!Mover->bHoldingFurniture||Mover->GetHeldBody()!=S.Die->Body){Finish(false,TEXT("Die pickup failed"));return;}
        Note(TEXT("Rounded, numbered die rests on the table and supports real pickup"));PC->SetControlRotation(FRotator(0,90,0));S.Phase=18;S.Step=Now;return;
    }
    if(S.Phase==18)
    {
        if(Now-S.Step<1)return;
        FVector2D Pixel;int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
        if(!PC->ProjectWorldLocationToScreen(S.Die->Body->Bounds.Origin,Pixel)
            ||Pixel.X<0||Pixel.X>Width||Pixel.Y<0||Pixel.Y>Height-24)
        {Finish(false,TEXT("Die resting grip is outside the view"));return;}
        Note(TEXT("Die resting grip is visibly inside the camera frame"));
        Shot(TEXT("DieHeld.png"));S.Phase=19;S.Step=Now;return;
    }
    if(S.Phase==19)
    {
        if(Now-S.Step<.3)return;
        Mover->ToggleGrab();S.Phase=20;S.Step=Now;return;
    }
    if(S.Phase==20)
    {
        if(Now-S.Step<.7)return;
        if(Mover->bHoldingFurniture){Finish(false,TEXT("Die set-down did not release"));return;}
        Note(TEXT("Die set-down released the grip cleanly"));
        Finish(true,TEXT("Rear concrete route, both door directions, closed wall, football pickup/throw and dice contact/pickup passed; multiplayer not exercised"));
    }
}
#else
void TickHouseRearPorchReview(AStoryFirstPersonCharacter*){}
#endif
