#include "HouseCarryTiersReview.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseInteractiveProp.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void TickHouseGreatRoomCollisionReview(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()) return;
    UWorld* W = Pawn->GetWorld();
    if (!W || UGameplayStatics::GetCurrentLevelName(W, true) != TEXT("Chapter01_House")) return;
    auto* PC = Cast<APlayerController>(Pawn->GetController());
    if (!PC) return;
    struct FState { TWeakObjectPtr<UWorld> World; int Phase = 0, Way = 0; double Started = 0; FString Dir, Log; };
    static FState S;
    const double Now = W->GetTimeSeconds();
    const float Half = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const FVector Corners[] = { FVector(-1100,-450,0), FVector(1175,-450,0), FVector(1175,800,0), FVector(-1100,800,0) };
    auto Note = [&](const FString& Msg) { S.Log += Msg + TEXT("\n"); UE_LOG(LogTemp, Display, TEXT("GREAT_ROOM_CHECK %s"), *Msg); };
    auto Finish = [&](bool Pass, const FString& Msg) {
        Note((Pass ? TEXT("PASS ") : TEXT("FAIL ")) + Msg);
        FFileHelper::SaveStringToFile(S.Log, *FPaths::Combine(S.Dir, TEXT("result.txt")));
        S.Phase = -1; FPlatformMisc::RequestExitWithStatus(false, Pass ? 0 : 1);
    };
    if (S.World.Get() != W)
    {
        S = FState(); S.World = W; S.Started = Now;
        S.Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GreatRoomCollisionReview"));
        IFileManager::Get().MakeDirectory(*S.Dir, true);
        if (auto* G = UGameUserSettings::GetGameUserSettings()) { G->SetOverallScalabilityLevel(2); G->ApplyNonResolutionSettings(); }
    }
    if (S.Phase < 0) return;
    if (S.Phase == 0)
    {
        if (Now - S.Started < 7) return;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(GreatRoomPaths), false, Pawn);
        for (int I = 0; I < 4; ++I)
        {
            FHitResult H;
            if (W->SweepSingleByChannel(H, Corners[I] + FVector(0,0,Half+3), Corners[(I+1)%4] + FVector(0,0,Half+3),
                    FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius,Half), Q))
            { Finish(false, FString::Printf(TEXT("Aisle %d blocked by %s/%s at %s"), I, *GetNameSafe(H.GetActor()), *GetNameSafe(H.GetComponent()), *H.Location.ToString())); return; }
        }
        Note(TEXT("All four full-size player capsule aisle sweeps clear"));
        for (float X : {-1100.f, 0.f, 1175.f}) for (float Y : {-450.f, 700.f, 800.f})
        {
            FHitResult H;
            if (!W->LineTraceSingleByChannel(H,FVector(X,Y,40),FVector(X,Y,-40),ECC_Pawn,Q) || FMath::Abs(H.ImpactPoint.Z)>3)
            { Finish(false,TEXT("Room floor has a gap or elevated collision")); return; }
        }
        Note(TEXT("Nine room floor samples still solid at the original height"));
        FCollisionQueryParams SolidQ(SCENE_QUERY_STAT(GreatRoomSolidSurfaces),false,Pawn);
        for (TActorIterator<AHouseInteractiveProp> It(W); It; ++It) SolidQ.AddIgnoredActor(*It);
        for (int I = 0; I < 2; ++I)
        {
            const float Sign = I == 0 ? -1.f : 1.f;
            FHitResult H;
            const FVector Start(Sign*1090,200,100), End(Sign*1440,200,100);
            if (!W->SweepSingleByChannel(H,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(15),SolidQ)
                || !H.GetActor() || !H.GetActor()->GetActorNameOrLabel().StartsWith(TEXT("GreatRoom_Fireplace_")))
            { Finish(false,TEXT("A fireplace no longer blocks the player")); return; }
        }
        for (float Sign : {-1.f,1.f})
        {
            FHitResult H;
            if (!W->SweepSingleByChannel(H,FVector(Sign*1100,-400,100),FVector(Sign*1650,-400,100),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(15),SolidQ))
            { Finish(false,TEXT("A real side wall lost collision")); return; }
        }
        Note(TEXT("Both fireplaces and both real side walls still block"));
        Pawn->SetActorLocation(Corners[0]+FVector(0,0,Half+3),false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->SetControlRotation((FVector(300,250,150)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        S.Phase=1; S.Started=Now; return;
    }
    if (S.Phase == 1)
    {
        if (Now-S.Started<1) return;
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,TEXT("GreatRoom.png")),false,false);
        S.Phase=2; S.Way=1; S.Started=Now; return;
    }
    if (S.Phase == 2)
    {
        FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);
        FVector To=Corners[S.Way%4]-Feet;
        if (To.Size2D()<25)
        {
            Note(FString::Printf(TEXT("Walked aisle %d using normal character movement; feet=%s"),S.Way,*Feet.ToString()));
            if (++S.Way==5)
            {
                PC->SetControlRotation((FVector(400,400,150)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
                FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,TEXT("WalkComplete.png")),false,false);
                S.Phase=3; S.Started=Now; return;
            }
            Pawn->GetCharacterMovement()->StopMovementImmediately(); S.Started=Now; return;
        }
        if (Now-S.Started>20 || FMath::Abs(Feet.Z)>25)
        { Finish(false,FString::Printf(TEXT("Real walking stalled on aisle %d at %s"),S.Way,*Feet.ToString())); return; }
        To.Z=0; Pawn->AddMovementInput(To.GetSafeNormal(),1); PC->SetControlRotation(To.Rotation()); return;
    }
    if (S.Phase == 3 && Now-S.Started>1)
        Finish(true,TEXT("Real player walked all four great-room aisles; floor, walls and fireplaces remain solid."));
}
#else
void TickHouseGreatRoomCollisionReview(AStoryFirstPersonCharacter*) {}
#endif
