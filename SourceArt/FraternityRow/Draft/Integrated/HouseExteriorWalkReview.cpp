#include "HouseFraternityRow.h"
#include "HouseCarryTiersReview.h"
#if !UE_BUILD_SHIPPING
#include "StoryCampaign.h"
#include "HouseBlackHall.h"
#include "HouseExteriorSiteLayout.h"
#include "ExistingHouseDoor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void TickHouseExteriorWalkReview(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn||!Pawn->HasAuthority()||!Pawn->IsLocallyControlled())return;
    UWorld* W=Pawn->GetWorld();
    if(!W||UGameplayStatics::GetCurrentLevelName(W,true)!=TEXT("Chapter01_House"))return;
    auto* PC=Cast<APlayerController>(Pawn->GetController());if(!PC)return;
    struct FState {TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ACameraActor> Cam;int Phase=0,View=0,Route=0,Way=0;double Start=0,Overall=0;FString Dir,Log;};
    static FState S;const double Now=W->GetTimeSeconds();
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    auto Note=[&](const FString& M){S.Log+=M+TEXT("\n");UE_LOG(LogTemp,Display,TEXT("EXTERIOR_WALK_REVIEW %s"),*M);};
    auto Finish=[&](bool Pass,const FString& M){Note((Pass?TEXT("PASS "):TEXT("FAIL "))+M);FFileHelper::SaveStringToFile(S.Log,*FPaths::Combine(S.Dir,TEXT("result.txt")));S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);};
    auto Floor=[&](float X,float Y,FHitResult& H){FCollisionQueryParams Q(SCENE_QUERY_STAT(ExteriorWalkFloor),false,Pawn);return W->LineTraceSingleByChannel(H,FVector(X,Y,40),FVector(X,Y,-410),ECC_Pawn,Q)&&H.ImpactNormal.Z>.6f;};
    auto Place=[&](const FVector& P){FHitResult H;if(!Floor(P.X,P.Y,H))return false;Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->SetActorLocation(FVector(P.X,P.Y,H.ImpactPoint.Z+Half+2),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);return true;};
    auto Capture=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::Combine(S.Dir,Name),false,false);};
    const TArray<TArray<FVector>> Routes={
        {FVector(-36.65,HouseExteriorSite::FrontSidewalkInnerY-100,0),FVector(-36.65,-2670,0),FVector(-36.65,-1980,0),FVector(-36.65,-1180,0),FVector(-36.65,-1810,0),FVector(-1200,-1810,0),FVector(1160,-1810,0),FVector(-36.65,-1810,0),FVector(-36.65,-3500,0),FVector(1200,-3500,0)},
        {FVector(-336.65,-2580,0),FVector(-336.65,-1970,0),FVector(263.35,-1970,0),FVector(263.35,-2590,0)},
        {FVector(-3220,-2290,0),FVector(-3475,-2290,0),FVector(-3475,-1800,0),FVector(-3475,-400,0),FVector(-3475,1200,0),FVector(-3475,1630,0),FVector(-2800,1630,0),FVector(-1650,1630,0)},
        {FVector(3050,-2220,0),FVector(3398,-2220,0),FVector(3398,-1370,0),FVector(3398,-970,0),FVector(3398,-770,0),FVector(3398,-350,0),FVector(3405,1200,0),FVector(3405,1630,0),FVector(1700,1630,0)},
        {FVector(-1800,-3700,0),FVector(-1200,-3050,0),FVector(-1200,-3520,0),FVector(-36.65,-3520,0),FVector(1200,-3520,0),FVector(1200,-3700,0),FVector(-36.65,-3700,0)},
        {FVector(-36.65,HouseExteriorSite::FrontSidewalkInnerY+80,0),FVector(-36.65,HouseExteriorSite::FrontRoadNearY-200,0),FVector(3750,HouseExteriorSite::FrontRoadNearY-200,0),FVector(3750,-3300,0),FVector(3750,-2600,0)},
        {FVector(-5500,HouseExteriorSite::FrontRoadNearY-200,0),FVector(-5500,-3300,0),FVector(-5500,-2570,0)}
    };
    struct FView{const TCHAR* Name;FVector Pos,Target;float FOV;};
    const FView Views[]={
        {TEXT("FrontLawnOverview.png"),FVector(4600,-6050,2400),FVector(-250,-2800,20),78},
        {TEXT("LawnAndRoad.png"),FVector(-36.65,-2180,750),FVector(-36.65,HouseExteriorSite::FrontRoadFarY,-60),82},
        {TEXT("FrontSteps.png"),FVector(-36.65,-2510,135),FVector(-36.65,-2170,-10),75},
        {TEXT("FrontLanding.png"),FVector(-36.65,-2970,200),FVector(-36.65,-1530,180),80},
        {TEXT("FrontHouse.png"),FVector(-36.65,-3650,190),FVector(-36.65,-1500,360),75},
        {TEXT("ConcreteSidePath.png"),FVector(-3700,-2780,185),FVector(-2900,-2000,0),83},
        {TEXT("PerimeterOverview.png"),FVector(5000,-4100,2400),FVector(-150,-150,80),80}
    };
    if(S.World.Get()!=W)
    {
        S=FState();S.World=W;S.Start=S.Overall=Now;S.Dir=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("ExteriorWalkReview"));IFileManager::Get().MakeDirectory(*S.Dir,true);
        if(auto* G=UGameUserSettings::GetGameUserSettings()){G->SetOverallScalabilityLevel(2);G->ApplyNonResolutionSettings();}
    }
    if(S.Phase<0)return;
    if(Now-S.Overall>220){Finish(false,TEXT("Exterior walking timed out"));return;}
    if(S.Phase==0)
    {
        if(Now-S.Start<7)return;
        auto* Look=W->GetSubsystem<UHouseBlackHallSubsystem>();
        auto Surface=[&](float X,float Y,float ExpectedZ,FName Key)
        {
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(FrontLawnSurface),true,Pawn);Q.bReturnFaceIndex=true;
            const bool Hit=W->LineTraceSingleByChannel(H,FVector(X,Y,40),FVector(X,Y,-150),ECC_Pawn,Q);
            int32 Section=INDEX_NONE;
            auto* Actual=Hit&&H.GetComponent()?H.GetComponent()->GetMaterialFromCollisionFaceIndex(H.FaceIndex,Section):nullptr;
            const auto* Expected=Look?Look->Materials.Find(Key):nullptr;
            if(!Hit||FMath::Abs(H.ImpactPoint.Z-ExpectedZ)>1||!Expected||Actual!=Expected->Get())
            {Finish(false,FString::Printf(TEXT("Front site surface incorrect at %.0f,%.0f: z=%.1f expected=%.1f %s material=%s section=%d face=%d"),X,Y,H.ImpactPoint.Z,ExpectedZ,*Key.ToString(),*GetNameSafe(Actual),Section,H.FaceIndex));return false;}
            return true;
        };
        for(float X:{-1800.f,-900.f,900.f,1800.f})for(float Y:{-3000.f,-3300.f,-3700.f})if(!Surface(X,Y,-60,TEXT("EX_Grass")))return;
        Note(TEXT("The former front road/sidewalk strip is now supported grass on both sides of the approach"));
        for(float Y:{-3000.f,-3500.f,HouseExteriorSite::FrontSidewalkInnerY+20})if(!Surface(-36.65,Y,-57,TEXT("EX_WalkConcrete")))return;
        if(!Surface(800,HouseExteriorSite::FrontSidewalkInnerY-125,-57,TEXT("EX_Concrete")))return;
        for(float X:{-2000.f,0.f,2500.f})if(!Surface(X,HouseExteriorSite::FrontRoadNearY-200,-72,TEXT("EX_Asphalt")))return;
        if(!Surface(-5500,-3700,HouseFraternityRow::Enabled()?-60.f:-58.f,HouseFraternityRow::Enabled()?TEXT("EX_Grass"):TEXT("EX_Asphalt")))return;
        if(!Surface(3750,-3700,-58,TEXT("EX_Asphalt")))return;
        Note(FString::Printf(TEXT("Street set back %.0f world cm; continuous concrete approach, displaced sidewalk/asphalt and both driveway extensions verified"),HouseExteriorSite::FrontStreetSetback));
        for(float X:{3398.f,3445.f,3490.f,3530.f})
        {
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(ExteriorRightPathAudit),false,Pawn);
            const bool Block=W->SweepSingleByChannel(H,FVector(X,-2070,-57+Half+3),FVector(X,-1720,-57+Half+3),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q);
            Note(FString::Printf(TEXT("Right path x=%.0f blocked=%d by %s/%s at %s"),X,Block,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),*H.Location.ToString()));
        }
        for(float X:{-336.65f,-36.65f,263.35f})for(const FVector2D& P:{FVector2D(-2380,-57),FVector2D(-2280,-38.6667),FVector2D(-2220,-20.3333),FVector2D(-2150,-2)})
        {
            FHitResult H;if(!Floor(X,P.X,H)||FMath::Abs(H.ImpactPoint.Z-P.Y)>1){Finish(false,FString::Printf(TEXT("Stair support incorrect at %.0f,%.0f: %s/%s z=%.1f"),X,P.X,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),H.ImpactPoint.Z));return;}
        }
        Note(TEXT("Three shallow risers: all three player lanes supported at matching heights"));
        for(float X:{-1200.f,-36.65f,1160.f})for(float Y:{-2100.f,-1810.f})
        {FHitResult H;if(!Floor(X,Y,H)||FMath::Abs(H.ImpactPoint.Z+2)>1){Finish(false,TEXT("Concrete forecourt has an elevated collider or gap"));return;}}
        Note(TEXT("Expanded concrete landing is level with the porch and fitted front-door threshold"));
        for(float X:{-3475.f,3398.f})for(float Y:{-1800.f,-400.f,1200.f,1630.f})
        {
            FHitResult H;const float Expected=FMath::Lerp(-60.f,-362.f,FMath::Clamp(Y/1600.f,0.f,1.f))+3;
            if(!Floor(X,Y,H)||FMath::Abs(H.ImpactPoint.Z-Expected)>1){Finish(false,FString::Printf(TEXT("Side path incorrect at %.0f,%.0f: %s/%s z=%.1f expected=%.1f"),X,Y,*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent()),H.ImpactPoint.Z,Expected));return;}
        }
        Note(TEXT("Both perimeter paths follow the existing lawn-to-basement grade with no floor gaps"));
        for(float X:{-900.f,820.f})
        {FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(FrontBrickCheeks),false,Pawn);if(!W->LineTraceSingleByChannel(H,FVector(X,-2270,25),FVector(X,-2140,25),ECC_Pawn,Q)||GetNameSafe(H.GetComponent())!=TEXT("ExteriorWalks")){Finish(false,FString::Printf(TEXT("Visible brick retaining wall probe hit %s/%s"),*GetNameSafe(H.GetActor()),*GetNameSafe(H.GetComponent())));return;}}
        Note(TEXT("Both visible brick cheek walls have solid collision"));
        FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        S.Cam=W->SpawnActor<ACameraActor>(P);Pawn->SetActorLocation(FVector(0,0,6000),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->DisableMovement();PC->SetViewTarget(S.Cam.Get());S.Phase=1;S.Start=Now;return;
    }
    if(S.Phase==1)
    {
        const FView& V=Views[S.View];S.Cam->SetActorLocation(V.Pos);S.Cam->SetActorRotation((V.Target-V.Pos).Rotation());S.Cam->GetCameraComponent()->SetFieldOfView(V.FOV);
        S.Phase=2;S.Start=Now;return;
    }
    if(S.Phase==2){if(Now-S.Start<1.5)return;Capture(Views[S.View].Name);S.Phase=3;S.Start=Now;return;}
    if(S.Phase==3)
    {
        if(Now-S.Start<.4)return;
        if(++S.View<UE_ARRAY_COUNT(Views)){S.Phase=1;return;}
        PC->SetViewTarget(Pawn);S.Cam->Destroy();S.Cam=nullptr;
        if(!Place(Routes[0][0])){Finish(false,TEXT("Player cannot stand on the approach"));return;}
        S.Route=0;S.Way=1;S.Phase=4;S.Start=Now;return;
    }
    if(S.Phase==4)
    {
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);
        const FVector Target=Routes[S.Route][S.Way];FVector To=Target-Feet;To.Z=0;
        FHitResult H;
        if(!Floor(Feet.X,Feet.Y,H)||Feet.Z<H.ImpactPoint.Z-10||Feet.Z>H.ImpactPoint.Z+35)
        {Finish(false,FString::Printf(TEXT("Player lost support on route %d at %s"),S.Route,*Feet.ToString()));return;}
        if(To.Size()<18)
        {
            Pawn->GetCharacterMovement()->StopMovementImmediately();Note(FString::Printf(TEXT("Walked route %d segment %d with normal movement; feet=%s"),S.Route,S.Way,*Feet.ToString()));
            if(++S.Way>=Routes[S.Route].Num())
            {
                if(++S.Route>=Routes.Num())
                {
                    if(!Place(FVector(-36.65,-1180,0))){Finish(false,TEXT("No front door footing"));return;}
                    PC->SetControlRotation((FVector(-36.65,-989,130)-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());S.Phase=5;S.Start=Now;return;
                }
                if(!Place(Routes[S.Route][0])){Finish(false,TEXT("Cannot stand at perimeter route start"));return;}S.Way=1;
            }
            S.Start=Now;return;
        }
        if(Now-S.Start>22)
        {
            FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(ExteriorWalkingBlocked),false,Pawn);
            W->SweepSingleByChannel(Hit,Pawn->GetActorLocation()+FVector(0,0,1),Pawn->GetActorLocation()+To.GetSafeNormal()*90+FVector(0,0,1),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),Q);
            Finish(false,FString::Printf(TEXT("Walking blocked on route %d segment %d at %s by %s/%s"),S.Route,S.Way,*Feet.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent())));return;
        }
        Pawn->AddMovementInput(To.GetSafeNormal(),1);PC->SetControlRotation(To.Rotation());return;
    }
    if(S.Phase==5)
    {
        if(Now-S.Start<.6)return;
        auto* Door=Pawn->FocusedDoorForInteraction();if(!Door||Door->DoorGroup!=TEXT("FrontWalnutDoubleDoors")){Finish(false,TEXT("Front doors lost normal E focus"));return;}
        if(!Door->IsOpen()){PC->InputKey(FInputKeyParams(EKeys::E,IE_Pressed,1.0));PC->InputKey(FInputKeyParams(EKeys::E,IE_Released,0.0));}
        S.Phase=6;S.Start=Now;return;
    }
    if(S.Phase==6)
    {
        if(Now-S.Start<1)return;
        int Open=0;for(TActorIterator<AExistingHouseDoor> It(W);It;++It)if(It->DoorGroup==TEXT("FrontWalnutDoubleDoors")&&It->IsOpen())++Open;
        if(Open!=2){Finish(false,TEXT("Existing double-door group no longer opens"));return;}
        Note(TEXT("Existing walnut double doors still offer E and open together"));Capture(TEXT("DoorAndLanding.png"));S.Phase=7;S.Start=Now;return;
    }
    if(S.Phase==7)
    {
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Half);
        if(Feet.Y>-815){Note(TEXT("Player walked from the new concrete landing through the walnut doors into the original great room"));S.Phase=8;S.Start=Now;return;}
        if(Now-S.Start>12){Finish(false,TEXT("Front door passage blocked"));return;}Pawn->AddMovementInput(FVector(0,1,0),1);return;
    }
    if(S.Phase==8&&Now-S.Start>.5)Finish(true,TEXT("Rendered larger front lawn and displaced road; normal walking across the grass, extended approach, curbs, both driveway connections, original front steps/perimeter paths and real front-door entry verified."));
}
#else
void TickHouseExteriorWalkReview(AStoryFirstPersonCharacter*){}
#endif
