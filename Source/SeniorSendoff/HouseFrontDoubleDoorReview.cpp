#include "HouseCarryTiersReview.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "ExistingHouseDoor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void TickHouseFrontDoubleDoorReview(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* W=Pawn->GetWorld();
    if (!W || UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House")) return;
    auto* PC=Cast<APlayerController>(Pawn->GetController()); if (!PC) return;
    struct FState { TWeakObjectPtr<UWorld> World; TWeakObjectPtr<AExistingHouseDoor> L,R; int Phase=0; double Started=0,Overall=0; FString Dir,Log; };
    static FState S; const double Now=W->GetTimeSeconds();
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    auto Note=[&](const FString& M){S.Log+=M+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("FRONT_DOOR_REVIEW %s"),*M);};
    auto Finish=[&](bool Pass,const FString& M){Note((Pass?TEXT("PASS "):TEXT("FAIL "))+M);FFileHelper::SaveStringToFile(S.Log,*FPaths::Combine(S.Dir,TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);};
    auto Place=[&](float Y){FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(FrontDoorFloor),false,Pawn);
        if (!W->LineTraceSingleByChannel(H,FVector(-36.65,Y,45),FVector(-36.65,Y,-45),ECC_Pawn,Q)||H.ImpactNormal.Z<.65f)return false;
        Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(FVector(-36.65,Y,H.ImpactPoint.Z+Half+2),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetControlRotation((FVector(-36.65,-989,129.5)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());return true;};
    auto InputE=[&](){PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.0));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.0));};
    auto Pair=[&](bool Open){return S.L.IsValid()&&S.R.IsValid()&&S.L->IsOpen()==Open&&S.R->IsOpen()==Open;};
    auto Capture=[&](const TCHAR* N){FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,N),false,false);};
    auto Passage=[&](bool Closed){
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FrontDoubleDoorCapsule),false,Pawn);
        for (float X : {-106.f,-36.65f,32.7f}) for (float Side : {-1.f,1.f}) {
            FHitResult H;const bool Hit=W->SweepSingleByChannel(H,FVector(X,-989-Side*110,Half+3),FVector(X,-989+Side*110,Half+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Q);
            const auto* Door=Cast<AExistingHouseDoor>(H.GetActor());
            if (Closed ? !Hit||!Door||Door->DoorGroup!=TEXT("FrontWalnutDoubleDoors") : Hit) {Note(FString::Printf(TEXT("Capsule x=%.1f closed=%d hit=%s/%s at %s"),X,Closed,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.Location.ToString()));return false;}
        }
        return true;
    };
    if (S.World.Get()!=W) {
        S=FState();S.World=W;S.Started=S.Overall=Now;S.Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("FrontDoubleDoorReview"));IFileManager::Get().MakeDirectory(*S.Dir,true);
        if (auto* G=UGameUserSettings::GetGameUserSettings()){G->SetOverallScalabilityLevel(2);G->ApplyNonResolutionSettings();}
    }
    if (S.Phase<0) return;
    if (Now-S.Overall>90){Finish(false,TEXT("Entrance test timed out"));return;}
    if (S.Phase==0) {
        if (Now-S.Started<7)return;
        for (TActorIterator<AExistingHouseDoor> It(W);It;++It){if(It->DoorId==TEXT("FrontWalnut_Left"))S.L=*It;if(It->DoorId==TEXT("FrontWalnut_Right"))S.R=*It;}
        if(!Pair(false)||!Passage(true)){Finish(false,TEXT("Closed double doors do not block the entrance"));return;}
        Note(TEXT("Both closed leaves block all three full-size player lanes from both sides"));
        for (float Y : {-1230.f,-1100.f,-1020.f,-970.f,-910.f,-850.f}) {
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(FrontThresholdSupport),false,Pawn);
            if(!W->LineTraceSingleByChannel(H,FVector(-36.65,Y,45),FVector(-36.65,Y,-45),ECC_Pawn,Q)||H.ImpactNormal.Z<.65f||FMath::Abs(H.ImpactPoint.Z)>6){Finish(false,FString::Printf(TEXT("Missing entrance floor at y=%.0f"),Y));return;}
        }
        Note(TEXT("Front porch, threshold and indoor floor have continuous support"));
        if(!Place(-1370)){Finish(false,TEXT("Cannot stand on the front porch"));return;}S.Phase=1;S.Started=Now;return;
    }
    if(S.Phase==1){if(Now-S.Started<1.2)return;Capture(TEXT("FrontDoors.png"));S.Phase=2;S.Started=Now;return;}
    if(S.Phase==2){if(Now-S.Started<.5)return;if(!Place(-1180)){Finish(false,TEXT("No outside interaction footing"));return;}S.Phase=3;S.Started=Now;return;}
    if(S.Phase==3){
        if(Now-S.Started<.5)return;auto* Focus=Pawn->FocusedDoorForInteraction();
        if(!Focus||Focus->DoorGroup!=TEXT("FrontWalnutDoubleDoors")){Finish(false,TEXT("Normal outside E prompt cannot focus the doors"));return;}
        Note(TEXT("Normal outside door focus and E prompt available"));Capture(TEXT("FrontDoorsClose.png"));InputE();S.Phase=4;S.Started=Now;return;
    }
    if(S.Phase==4){
        if(Now-S.Started<1)return;
        if(!Pair(true)||!Passage(false)||S.L->IsActorTickEnabled()||S.R->IsActorTickEnabled()){Finish(false,TEXT("E did not open both leaves into a clear settled passage"));return;}
        Note(TEXT("Outside E opens both hinged leaves; six open passage sweeps clear"));Capture(TEXT("FrontDoorsOpen.png"));S.Phase=5;S.Started=Now;return;
    }
    if(S.Phase==5 || S.Phase==9){
        const float Target=S.Phase==5?-800.f:-1180.f;
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);
        if(FMath::Abs(Feet.Z)>15||Now-S.Started>12){Finish(false,FString::Printf(TEXT("Real entrance walking stalled at %s"),*Feet.ToString()));return;}
        if(FMath::Abs(Feet.Y-Target)<15){Pawn->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation((FVector(-36.65,-989,129.5)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());Note(S.Phase==5?TEXT("Player walked from porch into the house using normal character movement"):TEXT("Player walked back through the doors onto the porch"));++S.Phase;S.Started=Now;return;}
        Pawn->AddMovementInput(FVector(0,FMath::Sign(Target-Feet.Y),0),1);return;
    }
    if(S.Phase==6 || S.Phase==10){
        if(Now-S.Started<.5)return;auto* Focus=Pawn->FocusedDoorForInteraction();
        if(!Focus||Focus->DoorGroup!=TEXT("FrontWalnutDoubleDoors")){Finish(false,TEXT("Normal E cannot refocus the open entrance from the other side"));return;}
        InputE();++S.Phase;S.Started=Now;return;
    }
    if(S.Phase==7){if(Now-S.Started<1)return;if(!Pair(false)||!Passage(true)){Finish(false,TEXT("Inside E did not close both leaves with solid collision"));return;}Note(TEXT("Inside E closes both leaves; collision stays solid"));Capture(TEXT("FrontDoorsInside.png"));S.Phase=8;S.Started=Now;return;}
    if(S.Phase==8){if(Now-S.Started<.6)return;InputE();S.Phase=81;S.Started=Now;return;}
    if(S.Phase==81){if(Now-S.Started<1)return;if(!Pair(true)||!Passage(false)){Finish(false,TEXT("Inside E did not reopen the doors"));return;}Note(TEXT("Inside E reopens both leaves smoothly"));S.Phase=9;S.Started=Now;return;}
    if(S.Phase==11){if(Now-S.Started<1)return;if(!Pair(false)){Finish(false,TEXT("Return outside E did not close the doors"));return;}Finish(true,TEXT("Rendered front doors, normal E interaction both sides, full-size collision and real two-way walking verified."));}
}
#else
void TickHouseFrontDoubleDoorReview(AStoryFirstPersonCharacter*){}
#endif
