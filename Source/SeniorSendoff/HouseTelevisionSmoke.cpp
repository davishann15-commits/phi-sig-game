// Opt-in great-room TV verification in the saved campaign house.
#include "StoryCampaign.h"
#include "SeniorLobby.h"
#include "SeniorSauce.h"
#include "HouseInteractiveProp.h"
#include "HouseDamageSubsystem.h"
#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
namespace
{
struct FTVCheck
{
    int32 Phase=0, Shots=0, CountBefore=0, Brackets=0;
    double Started=0, Changed=0, Launched=0;
    FVector WestOrigin, EastOrigin;
    TWeakObjectPtr<AHouseInteractiveProp> West, East;
    TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
    FString Directory;
};
void Finish(FTVCheck& S,bool Passed,const FString& Detail)
{
    S.Report->SetBoolField(TEXT("passed"),Passed);S.Report->SetStringField(TEXT("detail"),Detail);
    S.Report->SetNumberField(TEXT("elapsedSeconds"),FPlatformTime::Seconds()-S.Started);
    FString Json;FJsonSerializer::Serialize(S.Report,TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*S.Directory,true);FFileHelper::SaveStringToFile(Json,*(S.Directory/TEXT("result.json")));
    UE_LOG(LogTemp,Display,TEXT("HOUSE_TV_CHECK_%s: %s"),Passed?TEXT("PASSED"):TEXT("FAILED"),*Detail);
    S.Phase=-1;FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
}
void Capture(FTVCheck& S,const TCHAR* Name)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("nullrhi"))&&!FScreenshotRequest::IsScreenshotRequested())
        FScreenshotRequest::RequestScreenshot(S.Directory/(FString(Name)+TEXT(".png")),false,false);
}
void Aim(AStoryFirstPersonCharacter* Pawn,APlayerController* PC,AHouseInteractiveProp* TV)
{
    PC->SetControlRotation((TV->Body->Bounds.Origin-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
    if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(0.f);
}
void Hit(AHouseInteractiveProp* TV,APlayerController* PC,AActor* Causer,float Damage)
{
    const FVector Dir=TV->GetActorRotation().RotateVector(FVector(0,-1,0));
    FHitResult H(TV,TV->Body,TV->Body->Bounds.Origin,-Dir);
    UGameplayStatics::ApplyPointDamage(TV,Damage,Dir,H,PC,Causer,UDamageType::StaticClass());
}
}
void TickHouseTelevisionSmoke(UWorld* World)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("HouseTelevisionSmoke")))return;
    static FTVCheck S;if(!World||S.Phase<0)return;
    const double Now=FPlatformTime::Seconds();
    if(S.Started==0){S.Started=Now;S.Changed=Now;FParse::Value(FCommandLine::Get(),TEXT("TelevisionEvidence="),S.Directory);if(S.Directory.IsEmpty())S.Directory=FPaths::ProjectSavedDir()/TEXT("TelevisionCheck");IFileManager::Get().MakeDirectory(*S.Directory,true);}
    if(Now-S.Started>80){Finish(S,false,TEXT("TV check timed out"));return;}
    auto* PC=World->GetFirstPlayerController();auto* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if(!Pawn)return;
    auto* Damage=World->GetSubsystem<UHouseDamageSubsystem>();if(!Damage)return;
    const double Age=Now-S.Changed;
    auto Next=[&](int32 P){S.Phase=P;S.Changed=Now;};
    auto* West=S.West.Get();auto* East=S.East.Get();
    if(S.Phase==0)
    {
        if(Age<3)return;
        for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
        {if(It->PropId==TEXT("GreatRoom_WestTV"))S.West=*It;if(It->PropId==TEXT("GreatRoom_EastTV"))S.East=*It;}
        for(TActorIterator<AActor> It(World);It;++It)if(It->ActorHasTag(TEXT("GreatRoomTVUpgrade"))&&!Cast<AHouseInteractiveProp>(*It))++S.Brackets;
        West=S.West.Get();East=S.East.Get();if(!West||!East||S.Brackets!=2){Finish(S,false,TEXT("Expected two televisions and two separate brackets"));return;}
        for(auto* TV:{West,East})
        {
            const FRotator Rotation=TV->GetActorRotation();
            const FVector Extent=TV->Body->Bounds.BoxExtent;
            if(FMath::Abs(Rotation.Pitch)>.1f||FMath::Abs(Rotation.Roll)>.1f||Extent.X>10.f||Extent.Y<80.f||Extent.Z<45.f)
            {Finish(S,false,TEXT("Television is tilted or is not a landscape screen facing into the room"));return;}
            if(!TV->bWallMounted||!TV->ActorHasTag(TEXT("HouseWallTelevision"))||TV->bPhysicsActive||TV->Body->IsSimulatingPhysics()||TV->IsActorTickEnabled())
            {Finish(S,false,TEXT("Untouched television is not securely mounted and parked"));return;}
            for(UStaticMesh* M:{TV->IntactMesh.Get(),TV->DamagedMesh.Get(),TV->HeavilyDamagedMesh.Get(),TV->BrokenMesh.Get()})
            {
                if(!M||!M->GetBodySetup()||M->GetBodySetup()->AggGeom.ConvexElems.IsEmpty()){Finish(S,false,TEXT("TV damage stage lacks fitted collision"));return;}
                for(int32 I=0;I<M->GetStaticMaterials().Num();++I)if(!M->GetMaterial(I)){Finish(S,false,TEXT("TV material missing"));return;}
            }
        }
        S.WestOrigin=West->GetActorLocation();S.EastOrigin=East->GetActorLocation();S.CountBefore=Damage->GetTotalBrokenCount();
        PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);Pawn->GetCharacterMovement()->StopMovementImmediately();Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        Pawn->SetActorLocation(FVector(-700,190,100),false,nullptr,ETeleportType::TeleportPhysics);Aim(Pawn,PC,West);
        S.Report->SetBoolField(TEXT("twoDetailedTVsAndSeparateMounts"),true);S.Report->SetBoolField(TEXT("televisionsFaceRoomWithLandscapeScreens"),true);S.Report->SetBoolField(TEXT("untouchedTVsRemainParked"),true);Capture(S,TEXT("01_Intact"));Next(1);return;
    }
    if(!West||!East){Finish(S,false,TEXT("TV vanished during check"));return;}
    if(S.Phase==1)
    {
        if(Age<1)return;Hit(West,PC,Pawn,20);
        if(West->DamageStage!=1||West->IsDetached()||West->bPhysicsActive||West->Body->IsSimulatingPhysics()||FVector::Dist(West->GetActorLocation(),S.WestOrigin)>.1)
        {Finish(S,false,TEXT("First LCD crack failed to stay mounted without floating"));return;}
        if(West->Body->GetStaticMesh()!=West->DamagedMesh){Finish(S,false,TEXT("First screen damage visual missing"));return;}
        S.Report->SetBoolField(TEXT("firstHitCracksWhileMounted"),true);Capture(S,TEXT("02_CrackedStillMounted"));Next(2);return;
    }
    if(S.Phase==2)
    {
        if(Age<1.2)return;Hit(West,PC,Pawn,20);
        if(West->DamageStage!=2||!West->IsDetached()||!West->Body->IsSimulatingPhysics()||West->Body->GetStaticMesh()!=West->HeavilyDamagedMesh)
        {Finish(S,false,TEXT("Repeated impact did not detach the badly cracked television"));return;}
        S.Report->SetBoolField(TEXT("secondHitDamagesAndReleasesMount"),true);Next(3);return;
    }
    if(S.Phase==3)
    {
        Aim(Pawn,PC,West);if(Age<4)return;
        const float Fall=S.WestOrigin.Z-West->GetActorLocation().Z;S.Report->SetNumberField(TEXT("westTVFallCm"),Fall);
        if(Fall<25||West->Body->GetPhysicsLinearVelocity().ContainsNaN()){Finish(S,false,TEXT("Released TV failed to fall under gravity"));return;}
        Capture(S,TEXT("03_FallenLCD"));Hit(West,PC,Pawn,20);
        if(!West->bBroken||West->DamageStage!=3||West->Body->GetStaticMesh()!=West->BrokenMesh||West->BreakEvents!=1||Damage->GetTotalBrokenCount()!=S.CountBefore+1)
        {Finish(S,false,TEXT("Final TV damage failed or was counted twice"));return;}
        S.Report->SetBoolField(TEXT("threeDistinctDamageStagesAndCountOnce"),true);Next(4);return;
    }
    if(S.Phase==4)
    {
        Aim(Pawn,PC,West);if(Age<3)return;Capture(S,TEXT("04_BrokenTVOnFloor"));
        auto* Selection=Pawn->GetPlayerState<ASeniorLobbyPlayerState>();if(!Selection){Finish(S,false,TEXT("Weapon selection missing"));return;}
        Selection->CharacterIndex=0;Selection->LoadoutIndex=1;Pawn->ApplySelectedCharacter();
        Pawn->SetActorLocation(FVector(890,169,100),false,nullptr,ETeleportType::TeleportPhysics);Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Flying);Aim(Pawn,PC,East);Next(5);return;
    }
    if(S.Phase==5)
    {
        Aim(Pawn,PC,East);
        if(Age<.5)return;
        if(S.Shots>=3&&East->DamageEvents>=3)
        {
            if(East->DamageStage!=1||East->IsDetached()||East->Body->IsSimulatingPhysics())
            {Finish(S,false,TEXT("Actual Braxton weapon hits failed to crack the mounted TV"));return;}
            S.Report->SetBoolField(TEXT("actualBraxtonWeaponHitsMountedTV"),true);S.Report->SetNumberField(TEXT("weaponThrows"),S.Shots);Capture(S,TEXT("05_RealWeaponScreenCracks"));Next(6);return;
        }
        if(Pawn->SaucePacket&&Pawn->SaucePacket->Phase==ESeniorSaucePhase::Ready&&Now-S.Launched>.5)
        {
            if(S.Shots>=3){Finish(S,false,TEXT("Native weapon flight missed the television"));return;}
            FHitResult H;FCollisionQueryParams Q(SCENE_QUERY_STAT(TVWeaponAim),false,Pawn);Q.AddIgnoredActor(Pawn->SaucePacket);
            const FVector View=Pawn->FirstPersonCamera->GetComponentLocation();
            if(!World->LineTraceSingleByChannel(H,View,East->Body->Bounds.Origin,ECC_Visibility,Q)||H.GetActor()!=East)
            {Finish(S,false,TEXT("Old wall geometry or furniture occludes the new television"));return;}
            Pawn->ServerThrowEquippedWeapon();++S.Shots;S.Launched=Now;
        }
        if(Age>15){Finish(S,false,TEXT("Braxton weapon check exceeded cooldown window"));return;}return;
    }
    if(S.Phase==6)
    {
        if(Age<1)return;Hit(East,PC,Pawn,20);
        if(!East->IsDetached()||East->DamageStage!=2){Finish(S,false,TEXT("Second TV mount did not release with heavy damage"));return;}
        Next(7);return;
    }
    if(S.Phase==7)
    {
        Aim(Pawn,PC,East);if(Age<4)return;Hit(East,PC,Pawn,East->Health);
        Hit(East,PC,Pawn,20);Hit(West,PC,Pawn,20);Next(8);return;
    }
    if(S.Phase==8)
    {
        Aim(Pawn,PC,East);if(Age<4)return;
        const float WestSpeed=West->Body->GetPhysicsLinearVelocity().Size(),EastSpeed=East->Body->GetPhysicsLinearVelocity().Size();
        if((WestSpeed>60||EastSpeed>60)&&Age<12)return;
        if(!West->bBroken||!East->bBroken||West->BreakEvents!=1||East->BreakEvents!=1||Damage->GetTotalBrokenCount()!=S.CountBefore+2||WestSpeed>60||EastSpeed>60)
        {Finish(S,false,TEXT("Broken televisions did not settle or house damage counted twice"));return;}
        S.Report->SetNumberField(TEXT("westSettledSpeed"),WestSpeed);S.Report->SetNumberField(TEXT("eastSettledSpeed"),EastSpeed);S.Report->SetNumberField(TEXT("eastTVFallCm"),S.EastOrigin.Z-East->GetActorLocation().Z);
        S.Report->SetBoolField(TEXT("bothTVsFallAndSettle"),true);S.Report->SetBoolField(TEXT("oneHouseDamageCountPerTV"),true);Capture(S,TEXT("06_SecondBrokenTV"));Next(9);return;
    }
    if(S.Phase==9&&Age>1)Finish(S,true,TEXT("Both saved TVs: mounted LCD cracks, three damage stages, real weapon hits, mount release, gravity fall, stable collision, and damage counted once."));
}
#else
void TickHouseTelevisionSmoke(UWorld*){}
#endif
