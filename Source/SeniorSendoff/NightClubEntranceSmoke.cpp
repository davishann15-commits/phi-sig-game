#include "NightClubEntranceSmoke.h"
#if !UE_BUILD_SHIPPING
#include "ExistingHouseDoor.h"
#include "HouseInteractiveProp.h"
#include "HouseNightClub.h"
#include "StoryCampaign.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

void TickNightClubEntranceSmoke(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("NightClubEntranceSmoke"))) return;
    static int32 Stage=0,DoorIndex=0,InteriorStep=0;
    static double Started=0,Action=0,Progress=0;
    static uint64 WarmFrame=0;
    static float Radius=0,HalfHeight=0,BestDistance=BIG_NUMBER;
    static FVector Approach,Interior,WalkStart,Inward;
    static TArray<FVector> InteriorRoute;
    static TArray<TWeakObjectPtr<AExistingHouseDoor>> Doors;
    static TArray<TSharedPtr<FJsonValue>> Results;
    if (!World || Stage<0 || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn || !Pawn->FirstPersonCamera || !World->GetGameViewport()) return;
    UCharacterMovementComponent* Movement=Pawn->GetCharacterMovement();
    const double Now=World->GetTimeSeconds();
    if (!Started)
    { Started=FPlatformTime::Seconds();Action=Now;WarmFrame=GFrameCounter;if(AGameModeBase* Mode=World->GetAuthGameMode())Mode->SetActorTickInterval(0); }
    auto Finish=[&](bool Passed,const FString& Why)
    {
        Movement->StopMovementImmediately();
        TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("status"),Passed?TEXT("PASS"):TEXT("FAIL"));
        Report->SetStringField(TEXT("detail"),Why);
        Report->SetStringField(TEXT("scope"),TEXT("Native E opening/closing from both sides, actual player movement into and out of all club entries, real closed/open capsule collision. One initial approach teleport per door only."));
        Report->SetNumberField(TEXT("elapsedSeconds"),FPlatformTime::Seconds()-Started);
        Report->SetArrayField(TEXT("checks"),Results);
        FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
        const FString Directory=FPaths::ProjectSavedDir()/TEXT("NightClub");
        IFileManager::Get().MakeDirectory(*Directory,true);
        FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("entrance-verification.json")));
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_ENTRANCE_%s: stage=%d door=%d %s"),Passed?TEXT("PASSED"):TEXT("FAILED"),Stage,DoorIndex,*Why);
        Stage=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
    };
    if (FPlatformTime::Seconds()-Started>180.){Finish(false,TEXT("Bounded actual entrance check timed out"));return;}
    auto Record=[&](const FString& Name,bool Passed)
    {
        TSharedRef<FJsonObject> R=MakeShared<FJsonObject>();
        R->SetStringField(TEXT("name"),Name);R->SetStringField(TEXT("door"),Doors.IsValidIndex(DoorIndex)&&Doors[DoorIndex].IsValid()?Doors[DoorIndex]->DoorId.ToString():TEXT("none"));
        R->SetBoolField(TEXT("passed"),Passed);R->SetStringField(TEXT("pawnPosition"),Pawn->GetActorLocation().ToCompactString());
        Results.Add(MakeShared<FJsonValueObject>(R));
        UE_LOG(LogTemp,Display,TEXT("NIGHT_CLUB_ENTRANCE_CHECK: %s passed=%d pawn=%s"),*Name,Passed,*Pawn->GetActorLocation().ToCompactString());
        return Passed;
    };
    auto FloorAt=[&](const FVector& P,float Expected)
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(NightClubEntranceFloor),false,Pawn);
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)Q.AddIgnoredActor(*It);
        for(TActorIterator<AExistingHouseDoor> It(World);It;++It)Q.AddIgnoredActor(*It);
        FHitResult Hit;
        return World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,Expected+25),FVector(P.X,P.Y,Expected-30),ECC_Pawn,Q)
            && !Hit.bStartPenetrating && Hit.ImpactNormal.Z>.7f && FMath::Abs(Hit.ImpactPoint.Z-Expected)<8.f;
    };
    auto PlaceApproach=[&]()
    {
        AExistingHouseDoor* Door=Doors[DoorIndex].Get();
        if(!Door || Door->bLocked || Door->IsOpen())return false;
        const float Floor=DoorIndex==1?332.53f:.88f;
        Inward=DoorIndex==2?FVector(0,-1,0):FVector(1,0,0);
        Approach=Door->DoorwayCenter-Inward*140;Approach.Z=Floor+HalfHeight+2;
        Interior=Door->DoorwayCenter+Inward*78;Interior.Z=Floor+HalfHeight+2;
        // Walk clear of the real leaf's full closing arc before operating it
        // from inside. The north offset avoids the lounge sofa without walking
        // into the opened leaf; the final point stays within native E reach.
        const FVector Side(-Inward.Y,Inward.X,0);
        FVector Final=Door->DoorwayCenter+Inward*225+Side*41;Final.Z=Floor+HalfHeight+2;
        InteriorRoute={Interior,Interior+Side*41,Final};
        InteriorStep=0;
        if(!FloorAt(Approach,Floor)||!FloorAt(Interior,Floor))return false;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(NightClubEntranceApproach),false,Pawn);
        if(World->OverlapBlockingTestByChannel(Approach,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Q))return false;
        Movement->StopMovementImmediately();Pawn->SetActorLocation(Approach,false,nullptr,ETeleportType::TeleportPhysics);
        Movement->SetMovementMode(MOVE_Walking);PC->SetControlRotation((Door->DoorwayCenter-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        WalkStart=Pawn->GetActorLocation();Action=Now;return true;
    };
    auto Passage=[&](bool Closed)
    {
        AExistingHouseDoor* Door=Doors[DoorIndex].Get();
        // This full-height aperture probe clears the existing 4.73 cm hall
        // board which real walking already steps over. The actual approach,
        // closed-door walk, and entry/exit routes retain their normal feet pose
        // and all furniture collision; the probe still fits below the header.
        FVector Center=Door->DoorwayCenter;Center.Z=(DoorIndex==1?332.53:.88)+HalfHeight+8;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(NightClubActualDoorPassage),false,Pawn);
        for(float Direction:{-1.f,1.f})
        {
            FHitResult Hit;
            const bool Blocked=World->SweepSingleByChannel(Hit,Center+Inward*(Direction*90),Center-Inward*(Direction*90),
                FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Q);
            if(Closed?(!Blocked||Hit.GetActor()!=Door):Blocked)
            {UE_LOG(LogTemp,Error,TEXT("NIGHT_CLUB_ENTRANCE_BLOCKER: actor=%s component=%s closed=%d impact=%s"),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Closed,*Hit.ImpactPoint.ToCompactString());return false;}
        }
        return true;
    };
    auto PressE=[&]()
    {
        AExistingHouseDoor* Door=Doors[DoorIndex].Get();
        PC->SetControlRotation((Door->DoorwayCenter-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if(Pawn->FocusedDoorForInteraction()!=Door)
        {UE_LOG(LogTemp,Error,TEXT("NIGHT_CLUB_ENTRANCE_FOCUS_FAILED: intended=%s actual=%s distance=%.2f pawn=%s"),*Door->DoorId.ToString(),*GetNameSafe(Pawn->FocusedDoorForInteraction()),FVector::Distance(Pawn->GetActorLocation(),Door->DoorwayCenter),*Pawn->GetActorLocation().ToCompactString());return false;}
        PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.0));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.0));Action=Now;return true;
    };
    if(Stage==0)
    {
        if(GFrameCounter-WarmFrame<64||Now-Action<2.)return;
        FString UserDir;if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir)||UserDir.IsEmpty()){Finish(false,TEXT("Requires isolated UserDir"));return;}
        bool HasLoungeEntry=false;
        for(TActorIterator<AHouseNightClub> It(World);It;++It)HasLoungeEntry|=!It->LowerAdditionalDoorways.IsEmpty();
        Doors.SetNum(HasLoungeEntry?3:2);
        for(TActorIterator<AExistingHouseDoor> It(World);It;++It)
        {
            if(It->DoorId==TEXT("NightClubLower"))Doors[0]=*It;
            if(It->DoorId==TEXT("HallEnd_East"))Doors[1]=*It;
            if(Doors.Num()>2 && It->DoorId==TEXT("SM_OriginalLeaf_Entrance_m1_m2_Solid_03"))Doors[2]=*It;
        }
        if(!Doors[0].IsValid()||!Doors[1].IsValid()||(Doors.Num()>2&&!Doors[2].IsValid())){Finish(false,TEXT("Both intended entrances must exist"));return;}
        Radius=Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();HalfHeight=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if(!PlaceApproach()){Finish(false,TEXT("Lower entry lacks a clear real hallway approach/floor"));return;}Stage=1;return;
    }
    if(!Doors[DoorIndex].IsValid()){Finish(false,TEXT("An entrance disappeared"));return;}
    AExistingHouseDoor* Door=Doors[DoorIndex].Get();
    if(Stage==1)
    {
        if(Now-Action<.8){Pawn->AddMovementInput(Inward,1,true);return;}
        Movement->StopMovementImmediately();
        const bool Stopped=!Door->IsOpen()&&FVector::DotProduct(Door->DoorwayCenter-Pawn->GetActorLocation(),Inward)>=Radius-1
            &&FVector::Dist2D(WalkStart,Pawn->GetActorLocation())>15;
        if(!Record(TEXT("walkStoppedAtClosedDoor"),Stopped&&Passage(true))||!PressE()){Finish(false,TEXT("Closed door/focus/native E failed"));return;}
        Stage=2;return;
    }
    if(Stage==2)
    {
        if(Now-Action<1)return;
        if(!Record(TEXT("nativeEOpenedClearPassage"),Door->IsOpen()&&Passage(false))){Finish(false,TEXT("Opened entrance remains blocked"));return;}
        BestDistance=BIG_NUMBER;Progress=Now;Stage=3;
    }
    if(Stage==3||Stage==7)
    {
        const FVector Target=Stage==3?InteriorRoute[InteriorStep]:Approach;
        const FVector To=Target-Pawn->GetActorLocation();const float Distance=To.Size2D();
        const float Floor=DoorIndex==1?332.53f:.88f;
        if(!Movement->IsMovingOnGround()||FMath::Abs(Pawn->GetActorLocation().Z-HalfHeight-Floor)>12){Finish(false,TEXT("Entrance walking lost its supporting floor"));return;}
        if(Distance<8)
        {
            if(Stage==3 && InteriorStep<InteriorRoute.Num()-1)
            {
                Record(InteriorStep==0?TEXT("walkedAcrossActualThreshold"):TEXT("walkedAlongClearInteriorLane"),true);
                ++InteriorStep;BestDistance=BIG_NUMBER;Progress=Now;return;
            }
            Movement->StopMovementImmediately();Record(Stage==3?TEXT("walkedIntoClub"):TEXT("walkedOutIntoHall"),true);
            PC->SetControlRotation((Door->DoorwayCenter-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
            Action=Now;Stage=Stage==3?4:8;return;
        }
        if(Distance+1<BestDistance){BestDistance=Distance;Progress=Now;}
        if(Now-Progress>4){Finish(false,TEXT("Real player walking stalled at the club entry"));return;}
        PC->SetControlRotation(To.GetSafeNormal2D().Rotation());Pawn->AddMovementInput(To.GetSafeNormal2D(),Distance<90?.35f:1.f,true);return;
    }
    if(Stage==4||Stage==8)
    {
        if(Now-Action<.2)return;
        if(!PressE()){Finish(false,TEXT("Native close E focus failed from inside or outside"));return;}
        Stage=Stage==4?5:9;return;
    }
    if(Stage==5)
    {
        if(Now-Action<1)return;
        const bool StayedInside=FVector::DotProduct(Pawn->GetActorLocation()-Door->DoorwayCenter,Inward)>Radius+20;
        if(!Record(TEXT("closedFromInsideAndStillBlocks"),!Door->IsOpen()&&Passage(true)&&StayedInside)||!PressE()){Finish(false,TEXT("Closing/reopening from inside failed"));return;}
        Stage=6;return;
    }
    if(Stage==6)
    {
        if(Now-Action<1)return;
        if(!Door->IsOpen()||!Passage(false)){Finish(false,TEXT("Reopening from inside did not clear entry"));return;}
        BestDistance=BIG_NUMBER;Progress=Now;Stage=7;return;
    }
    if(Stage==9)
    {
        if(Now-Action<1)return;
        if(!Record(TEXT("closedFromOutsideAndStillBlocks"),!Door->IsOpen()&&Passage(true))){Finish(false,TEXT("Closing from hallway failed"));return;}
        ++DoorIndex;
        if(DoorIndex<Doors.Num()){if(!PlaceApproach()){Finish(false,TEXT("Upper entry lacks a clear real hallway approach/floor"));return;}Stage=1;return;}
        Finish(true,TEXT("All club doors stop the player when closed, open and close with native E from both sides, and admit real walking into and out of their rooms."));
    }
}
#else
void TickNightClubEntranceSmoke(UWorld*){}
#endif
