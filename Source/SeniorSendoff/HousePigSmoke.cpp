#include "HouseRoamingPig.h"
#include "HouseWorldScale.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Scalability.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "Sound/SoundBase.h"

namespace
{
struct FPigSmoke
{
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs;
    TWeakObjectPtr<AExistingHouseDoor> Door;
    TWeakObjectPtr<AHousePigSnackPile> Food;
    int32 Stage=0, FoodStarted=0, RespawnsStarted=0, SnacksStarted[2]={0,0}, IndependentFrames=0;
    double Began=0, StageBegan=0;
    float AirSeconds[2]={0,0};
    float HurtHealthAfterDamage=0.f;
    FVector Start[2];
    FString Capture;
    TWeakObjectPtr<AExistingHouseDoor> TestGate;
    bool bSettingsApplied=false, bClosingTestGate=false;
    bool ReachedDownstairs[2]={false,false}, ReachedGreatRoom[2]={false,false};
    int32 LastLoggedStage=INDEX_NONE;
    double NextProgressLog=0;
};
bool SendE(APlayerController* PC,EInputEvent Event)
{
    if (!PC||!PC->PlayerInput||!FSlateApplication::IsInitialized()) return false;
    PC->InputKey(FInputKeyEventArgs(nullptr,FSlateApplication::Get().GetInputDeviceIdForMouse(),EKeys::E,Event,1.f,false,0)); return true;
}
bool PlacePlayer(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,FVector Feet,FVector Look,const TCHAR* Diagnostic=nullptr)
{
    FCollisionQueryParams Query; Query.AddIgnoredActor(Pawn);
    for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
    FHitResult Floor;
    if (!World->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,35),Feet-FVector(0,0,70),ECC_Visibility,Query)||Floor.ImpactNormal.Z<.65f)
    {
        if (Diagnostic) UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_OBSERVER_REJECT site=%s feet=%s reason=floor actor=%s component=%s hit=%d normal=%s"),
            Diagnostic,*Feet.ToString(),*GetNameSafe(Floor.GetActor()),*GetNameSafe(Floor.GetComponent()),Floor.bBlockingHit?1:0,*Floor.ImpactNormal.ToString());
        return false;
    }
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Center=Floor.ImpactPoint+FVector(0,0,Half+2.f);
    FCollisionQueryParams SpaceQuery; SpaceQuery.AddIgnoredActor(Pawn);
    if (World->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(
        Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),SpaceQuery))
    {
        if (Diagnostic)
        {
            TArray<FOverlapResult> Hits;
            World->OverlapMultiByChannel(Hits,Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(
                Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),SpaceQuery);
            for (const FOverlapResult& Hit:Hits) if (Hit.bBlockingHit)
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_OBSERVER_REJECT site=%s feet=%s reason=capsule actor=%s component=%s floor=%s center=%s"),
                    Diagnostic,*Feet.ToString(),*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*GetNameSafe(Floor.GetActor()),*Center.ToString());
        }
        return false;
    }
    Pawn->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics);
    Pawn->GetCharacterMovement()->StopMovementImmediately(); Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    const FRotator Aim=(Look-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
    PC->SetControlRotation(Aim); Pawn->SetActorRotation(FRotator(0,Aim.Yaw,0));
    // The harness may compare several setup viewpoints in one frame. Refresh
    // the observer camera so range/visibility checks use that exact viewpoint.
    if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
    return true;
}

bool SendPigUpgradeKey(APlayerController* PC,FKey Key,EInputEvent Event)
{
    if (!PC||!PC->PlayerInput||!FSlateApplication::IsInitialized()) return false;
    PC->InputKey(FInputKeyEventArgs(nullptr,FSlateApplication::Get().GetInputDeviceIdForMouse(),Key,Event,1.f,false,0));
    return true;
}

struct FPigUpgradeSmoke
{
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs; // Smaller, then larger.
    TWeakObjectPtr<AExistingHouseDoor> Door;
    TWeakObjectPtr<AHousePigSnackPile> Food;
    int32 Stage=0, LastStage=INDEX_NONE, CarryIndex=0, RefillsBefore=0, PausedFood=0, PausedRefills=0, BlockedPickupNode=INDEX_NONE;
    int32 HallPickupNode=INDEX_NONE, ReturnPoint=0, HallReturnPickups=0, HallReturnPlacements=0;
    int32 AteBefore[2]={0,0}, PickupsBefore=0, PlacesBefore=0, EscapeBefore[2]={0,0};
    float CarriedTravel=0, CarriedGait=0, SleepBodyZ[2]={0,0}, SleepBodyScaleZ[2]={0,0}, AirSeconds[2]={0,0};
    float DamagedHealth=0, CarryMoveDistance[2]={0,0};
    int32 BigWakeBefore=0, SmallWakeBefore=0;
    FVector PlayerMoveStart, PigCarryStart, FeedingStart[2], HallCarryPlayerStart, HallCarryLastPlayer;
    float HallCarryDistance=0;
    FString Capture;
    TArray<FString> Captures;
    double Began=0, StageBegan=0, ProgressAt=0;
    bool bConfigured=false, bMoveKeyDown=false, bBreathing[2]={false,false}, bSleepRequestedOnEntry=false;
};

UStaticMeshComponent* PigUpgradePart(AHouseRoamingPig* Pig,const TCHAR* Name)
{
    TArray<UStaticMeshComponent*> Parts; Pig->GetComponents(Parts);
    for (UStaticMeshComponent* Part:Parts) if (Part->GetFName()==FName(Name)) return Part;
    return nullptr;
}

UBoxComponent* PigUpgradeHitBox(AHouseRoamingPig* Pig)
{
    TArray<UBoxComponent*> Boxes; Pig->GetComponents(Boxes);
    for (UBoxComponent* Box:Boxes) if (Box->GetFName()==TEXT("PigWeaponHitBox")) return Box;
    return nullptr;
}

float PigUpgradeLowestVertex(UStaticMeshComponent* Part)
{
    UStaticMesh* Mesh=Part?Part->GetStaticMesh():nullptr;
    if (!Mesh||!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.IsEmpty()) return -BIG_NUMBER;
    const FPositionVertexBuffer& Vertices=Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    float Lowest=BIG_NUMBER;
    for (uint32 i=0;i<Vertices.GetNumVertices();++i)
        Lowest=FMath::Min(Lowest,Part->GetComponentTransform().TransformPosition(FVector(Vertices.VertexPosition(i))).Z);
    return Lowest;
}

bool PigUpgradeFloor(UWorld* World,AHouseRoamingPig* Pig,APawn* Observer,FHitResult& Hit)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigUpgradeSupport),false,Pig);
    Query.AddIgnoredActor(Observer);
    for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
    const FVector Feet=Pig->FeetLocation();
    return World->LineTraceSingleByChannel(Hit,Feet+FVector(0,0,25),Feet-FVector(0,0,75),ECC_Visibility,Query)
        && Hit.ImpactNormal.Z>.7f && Feet.Z-Hit.ImpactPoint.Z>=-2.f && Feet.Z-Hit.ImpactPoint.Z<12.f;
}

bool PigUpgradeInteractionView(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,AHouseRoamingPig* Pig)
{
    // Setup viewpoints may teleport the observer; the tested pickup, carry movement,
    // placement and pig approaches all use normal gameplay operations.
    const FVector Feet=Pig->FeetLocation();
    for (float Radius:{155.f,175.f,195.f})
        for (float Angle:{-90.f,0.f,90.f,180.f,-45.f,45.f,135.f,-135.f})
        {
            const FVector Candidate=Feet+FRotator(0,Angle,0).Vector()*Radius;
            if (Candidate.X<-2925.f||Candidate.X>-2395.f||Candidate.Y<-1330.f||Candidate.Y>-805.f) continue;
            if (!PlacePlayer(World,Pawn,PC,FVector(Candidate.X,Candidate.Y,302.3),Pig->GetInteractionLocation())) continue;
            if (Pawn->FocusedHousePigForInteraction()==Pig) return true;
        }
    return false;
}

bool PigUpgradeHallInteractionView(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,AHouseRoamingPig* Pig)
{
    const FVector Feet=Pig->FeetLocation();
    // Prefer the long axis of the hallway; lateral viewpoints can put the
    // player inside a neighboring room or behind that room's closed door.
    for (float Radius:{155.f,175.f,195.f})
        for (float Angle:{-90.f,90.f,-70.f,70.f,-110.f,110.f})
        {
            const FVector Candidate=Feet+FRotator(0,Angle,0).Vector()*Radius;
            if (!PlacePlayer(World,Pawn,PC,FVector(Candidate.X,Candidate.Y,302.3),Pig->GetInteractionLocation())) continue;
            if (Pawn->FocusedHousePigForInteraction()==Pig) return true;
        }
    return false;
}

bool PigUpgradeCarrySpace(UWorld* World,AHouseRoamingPig* Pig,APawn* Carrier)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigUpgradeCarriedBody),false,Pig); Query.AddIgnoredActor(Carrier);
    const FVector Center=Pig->GetActorLocation()+Pig->GetActorRotation().RotateVector(FVector(8.5f*Pig->VisualScale,0,0));
    return !World->OverlapBlockingTestByChannel(Center,Pig->GetActorQuat(),ECC_Pawn,
        FCollisionShape::MakeBox(FVector(78,29,45)*Pig->VisualScale*.98f),Query);
}

void TickHousePigUpgradeSmoke(UWorld* World)
{
    if (!World||UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    static FPigUpgradeSmoke S;
    if (S.World.Get()!=World) { S=FPigUpgradeSmoke(); S.World=World; }
    if (S.Stage<0) return;
    const double Now=FPlatformTime::Seconds(); if (!S.Began) S.Began=Now;
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn||!PC->PlayerInput||!World->GetGameViewport()) return;
    auto Finish=[&](bool Pass,const FString& Detail)
    {
        if (S.bMoveKeyDown) SendPigUpgradeKey(PC,EKeys::W,IE_Released);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_SMOKE_RESULT passed=%d elapsed=%.2f carryMovement=[%.1f,%.1f] sleepingBreath=[%d,%d] detail=%s"),
            Pass?1:0,Now-S.Began,S.CarryMoveDistance[0],S.CarryMoveDistance[1],S.bBreathing[0]?1:0,S.bBreathing[1]?1:0,*Detail);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_RESULT passed=%d detail=%s"),Pass?1:0,*Detail);
        TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
        Report->SetBoolField(TEXT("passed"),Pass); Report->SetStringField(TEXT("detail"),Detail);
        Report->SetNumberField(TEXT("elapsedSeconds"),Now-S.Began); Report->SetNumberField(TEXT("lastStage"),S.Stage);
        Report->SetNumberField(TEXT("escapedPigCarryDistance"),S.HallCarryDistance);
        Report->SetBoolField(TEXT("escapedPigReturned"),Pass&&S.Stage>=58);
        TArray<TSharedPtr<FJsonValue>> CaptureJson;
        for (const FString& Path:S.Captures)
        {
            TSharedPtr<FJsonObject> Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("path"),Path);
            Item->SetNumberField(TEXT("bytes"),IFileManager::Get().FileSize(*Path));
            CaptureJson.Add(MakeShared<FJsonValueObject>(Item));
        }
        Report->SetArrayField(TEXT("captures"),CaptureJson);
        TArray<TSharedPtr<FJsonValue>> PigJson;
        for (int32 i=0;i<S.Pigs.Num();++i) if (AHouseRoamingPig* Pig=S.Pigs[i].Get())
        {
            TSharedPtr<FJsonObject> Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("name"),Pig->PigName);
            Item->SetNumberField(TEXT("scale"),Pig->VisualScale); Item->SetNumberField(TEXT("maxHealth"),Pig->MaxHealth);
            Item->SetNumberField(TEXT("health"),Pig->Health); Item->SetNumberField(TEXT("runSpeed"),Pig->RunSpeed);
            Item->SetNumberField(TEXT("pickups"),Pig->Pickups); Item->SetNumberField(TEXT("placements"),Pig->Placements);
            Item->SetNumberField(TEXT("sleepStarts"),Pig->SleepStarts); Item->SetNumberField(TEXT("wakeEvents"),Pig->WakeEvents);
            Item->SetNumberField(TEXT("snacksEaten"),Pig->SnacksEaten); Item->SetNumberField(TEXT("escapes"),Pig->Escapes);
            Item->SetNumberField(TEXT("carryMovement"),S.CarryMoveDistance[i]); Item->SetBoolField(TEXT("sleepBreathing"),S.bBreathing[i]);
            PigJson.Add(MakeShared<FJsonValueObject>(Item));
        }
        Report->SetArrayField(TEXT("pigs"),PigJson);
        FString Json; FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        const FString ReportPath=FPaths::ProjectSavedDir()/TEXT("HousePigUpgradeSmoke/result.json");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true); FFileHelper::SaveStringToFile(Json,*ReportPath);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_REPORT path=%s"),*ReportPath);
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_PIG name=%s scale=%.3f walk=%.1f run=%.1f HP=%.1f/%.1f feet=%s sleeps=%d wakes=%d pickups=%d placements=%d food=%d escapes=%d floorStops=%d %s"),
                    *Pig->PigName,Pig->VisualScale,Pig->WalkSpeed,Pig->RunSpeed,Pig->Health,Pig->MaxHealth,*Pig->FeetLocation().ToString(),
                    Pig->SleepStarts,Pig->WakeEvents,Pig->Pickups,Pig->Placements,Pig->SnacksEaten,Pig->Escapes,Pig->UnsafeFloorStops,*Pig->MovementDebug());
        if (S.Food.IsValid())
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_FOOD food=%d visible=%d refills=%d occupancyPause=%d"),
                S.Food->FoodRemaining,S.Food->Cheetos->GetInstanceCount(),S.Food->FoodRespawns,S.Stage>=41?1:0);
        S.Stage=-1; FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    auto Advance=[&](int32 Stage) { S.Stage=Stage; S.StageBegan=Now; S.bSleepRequestedOnEntry=false; };
    auto Capture=[&](const TCHAR* Name)
    {
        S.Capture=FPaths::ProjectSavedDir()/TEXT("HousePigUpgradeSmoke")/Name;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.Capture),true);
        IFileManager::Get().Delete(*S.Capture,false,true);
        FScreenshotRequest::RequestScreenshot(S.Capture,true,false); S.Captures.Add(S.Capture);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_CAPTURE path=%s"),*S.Capture);
    };
    auto CaptureSaved=[&]() { return IFileManager::Get().FileSize(*S.Capture)>0; };
    if (Now-S.Began>210.) { Finish(false,TEXT("Bounded upgrade gameplay check timed out")); return; }
    if (S.Stage!=S.LastStage)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_STAGE stage=%d elapsed=%.2f"),S.Stage,Now-S.Began);
        S.LastStage=S.Stage;
    }
    if (Now>=S.ProgressAt)
    {
        S.ProgressAt=Now+4.;
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_PROGRESS stage=%d name=%s held=%d sleep=%d feet=%s %s"),
                    S.Stage,*Pig->PigName,Pig->IsCarried()?1:0,Pig->IsSleepingOnBed()?1:0,*Pig->FeetLocation().ToString(),*Pig->MovementDebug());
    }
    if (S.Stage==0)
    {
        if (Now-S.Began<3.) return;
        Scalability::FQualityLevels Quality; Quality.SetFromSingleQualityLevel(1); Scalability::SetQualityLevels(Quality);
        if (GEngine)
        {
            GEngine->Exec(World,TEXT("r.SetRes 1280x720w")); GEngine->Exec(World,TEXT("r.ScreenPercentage 100"));
            GEngine->Exec(World,TEXT("t.MaxFPS 60"));
            // Existing house diagnostics remain enabled for users; captures omit them.
            GEngine->Exec(World,TEXT("DisableAllScreenMessages"));
        }
        if (AGameModeBase* Mode=World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
        for (TActorIterator<AHouseRoamingPig> It(World);It;++It) S.Pigs.Add(*It);
        if (S.Pigs.Num()!=2) { Finish(false,TEXT("Expected exactly two installed pigs")); return; }
        S.Pigs.Sort([](const TWeakObjectPtr<AHouseRoamingPig>& A,const TWeakObjectPtr<AHouseRoamingPig>& B) { return A->VisualScale<B->VisualScale; });
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It) if (It->DoorId==TEXT("HallEnd_West")) { S.Door=*It; break; }
        AHouseRoamingPig* Little=S.Pigs[0].Get(); AHouseRoamingPig* Big=S.Pigs[1].Get();
        S.Food=Little->SnackPile;
        if (!S.Door.IsValid()||S.Door->bLocked||S.Door->IsOpen()||!S.Food.IsValid()||Big->SnackPile!=S.Food.Get())
        { Finish(false,TEXT("Saved pigsty gate must start closed and both pigs need shared snack pile")); return; }
        if (!(Little->VisualScale>0.4f&&Little->VisualScale<Big->VisualScale*.9f&&Big->VisualScale<.99f
            &&Little->RunSpeed>Big->RunSpeed*1.1f&&Little->WalkSpeed>Big->WalkSpeed&&Big->WalkSpeed>110.f
            &&Big->MaxHealth>Little->MaxHealth*1.2f))
        { Finish(false,TEXT("Saved pig sizes, faster movement or larger-pig health were not differentiated")); return; }
        const float SmallRadius=Little->GetCapsuleComponent()->GetScaledCapsuleRadius(),BigRadius=Big->GetCapsuleComponent()->GetScaledCapsuleRadius();
        UBoxComponent* SmallBox=PigUpgradeHitBox(Little); UBoxComponent* BigBox=PigUpgradeHitBox(Big);
        UStaticMeshComponent* SmallBody=PigUpgradePart(Little,TEXT("PigBody")); UStaticMeshComponent* BigBody=PigUpgradePart(Big,TEXT("PigBody"));
        const float SmallBodyX=SmallBody&&SmallBody->GetStaticMesh()?SmallBody->GetStaticMesh()->GetBounds().BoxExtent.X*FMath::Abs(SmallBody->GetComponentTransform().GetScale3D().X):0.f;
        const float BigBodyX=BigBody&&BigBody->GetStaticMesh()?BigBody->GetStaticMesh()->GetBounds().BoxExtent.X*FMath::Abs(BigBody->GetComponentTransform().GetScale3D().X):0.f;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_SIZES visual=[%.3f,%.3f] localScaledBodyX=[%.1f,%.1f] capsuleR=[%.1f,%.1f] capsuleH=[%.1f,%.1f] hitX=[%.1f,%.1f] speeds=[%.1f,%.1f] HP=[%.1f,%.1f]"),
            Little->VisualScale,Big->VisualScale,SmallBodyX,BigBodyX,
            SmallRadius,BigRadius,Little->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Big->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
            SmallBox?SmallBox->GetScaledBoxExtent().X:0.f,BigBox?BigBox->GetScaledBoxExtent().X:0.f,Little->RunSpeed,Big->RunSpeed,Little->MaxHealth,Big->MaxHealth);
        if (!SmallBox||!BigBox||!SmallBody||!BigBody||!Little->BodyMesh||!Little->HeadMesh||!Big->BodyMesh||!Big->HeadMesh
            ||SmallRadius>=BigRadius||BigRadius>30.1f
            ||Little->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()>=Big->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
            ||SmallBox->GetScaledBoxExtent().X>=BigBox->GetScaledBoxExtent().X
            ||SmallBodyX<=0.f||SmallBodyX>=BigBodyX)
        { Finish(false,TEXT("Actual body, collision capsule and weapon hitbox do not match different smaller pig sizes")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); FHitResult Floor;
            if (!Pig->IsInsidePigsty()||!PigUpgradeFloor(World,Pig,Pawn,Floor)) { Finish(false,TEXT("Saved smaller pig does not spawn on real floor")); return; }
            int32 Outside=INDEX_NONE;
            for (int32 n=0;n<Pig->RoamGraph.Num();++n) if (!Pig->RoamGraph[n].bInsidePigsty) { Outside=n; break; }
            if (Outside==INDEX_NONE||Pig->RequestRoamNode(Outside)) { Finish(false,TEXT("Closed pigsty door permits escape")); return; }
            Pig->SleepDuration=40.f;
            S.AteBefore[i]=Pig->SnacksEaten; S.FeedingStart[i]=Pig->FeetLocation();
            if (!Pig->RequestFeeding()) { Finish(false,TEXT("Pig cannot request physical feeding approach")); return; }
        }
        S.Food->RefillInterval=3.f; S.RefillsBefore=S.Food->FoodRespawns;
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2680,-1100,355)))
        { Finish(false,TEXT("Distant observer point is blocked")); return; }
        S.bConfigured=true; Advance(1); return;
    }
    for (int32 i=0;i<2;++i)
    {
        AHouseRoamingPig* Pig=S.Pigs[i].Get(); if (!Pig) { Finish(false,TEXT("Pig disappeared during upgrade check")); return; }
        if (!Pig->IsCarried())
        {
            FHitResult Floor; const bool Supported=PigUpgradeFloor(World,Pig,Pawn,Floor);
            S.AirSeconds[i]=Supported?0.f:S.AirSeconds[i]+World->GetDeltaSeconds();
            if (S.AirSeconds[i]>.7f||Pig->FeetLocation().Z<-80.f) { Finish(false,TEXT("Pig lost floor support after resize, sleep or placement")); return; }
        }
        if (S.Food->Cheetos->GetInstanceCount()!=S.Food->FoodRemaining)
        { Finish(false,TEXT("Snack number disagrees with actual visible instances")); return; }
    }
    if (S.Stage==1)
    {
        if (S.Door->IsOpen()||!S.Pigs[0]->IsInsidePigsty()||!S.Pigs[1]->IsInsidePigsty())
        { Finish(false,TEXT("Pig crossed or opened its initially closed door")); return; }
        if (Now-S.StageBegan>25.) { Finish(false,TEXT("Both pigs must physically reach and eat Cheetos before sleeping")); return; }
        if (S.Pigs[0]->SnacksEaten<=S.AteBefore[0]||S.Pigs[1]->SnacksEaten<=S.AteBefore[1]||S.Food->FoodRespawns<=S.RefillsBefore) return;
        if (!PlacePlayer(World,Pawn,PC,FVector(-2675,-1325,302.3),FVector(-2675,-1110,355)))
        { Finish(false,TEXT("Food and differentiated health viewpoint is blocked")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_EATING refill=%d food=%d visible=%d snacks=[%d,%d]"),
            S.Food->FoodRespawns,S.Food->FoodRemaining,S.Food->Cheetos->GetInstanceCount(),S.Pigs[0]->SnacksEaten,S.Pigs[1]->SnacksEaten);
        Capture(TEXT("pig_sizes_food.png")); Advance(2); return;
    }
    if (S.Stage==2)
    {
        if (!CaptureSaved()) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Food/detail capture failed")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2825,-900,355)))
        { Finish(false,TEXT("Sleep observer point is blocked")); return; }
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs) if (!Ref->RequestSleep())
        { Finish(false,TEXT("Pig cannot request its real mattress sleep approach")); return; }
        Advance(3); return;
    }
    if (S.Stage==3)
    {
        if (Now-S.StageBegan>35.) { Finish(false,TEXT("Both pigs did not physically reach separate mattress sleep places")); return; }
        if (!S.Pigs[0]->IsSleepingOnBed()||!S.Pigs[1]->IsSleepingOnBed()) return;
        // Mood changes at arrival; the visible body eases into its lying pose.
        // Observe the completed pose before evaluating mattress contact/captures.
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
        {
            UStaticMeshComponent* Body=PigUpgradePart(Ref.Get(),TEXT("PigBody"));
            if (!Body) { Finish(false,TEXT("Sleeping pig is missing its rendered body")); return; }
            if (FMath::Abs(Body->GetComponentRotation().Roll)<73.f||Ref->GetVelocity().Size2D()>3.f) return;
        }
        if (FVector::Dist2D(S.Pigs[0]->FeetLocation(),S.Pigs[1]->FeetLocation())<55.f)
        { Finish(false,TEXT("Sleeping pigs overlap their bed places")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); FHitResult Floor;
            UStaticMeshComponent* Body=PigUpgradePart(Pig,TEXT("PigBody"));
            if (!Body||!PigUpgradeFloor(World,Pig,Pawn,Floor)||FVector::Dist2D(Pig->FeetLocation(),Pig->BedLocation)>30.f
                ||FMath::Abs(Pig->FeetLocation().Z-Pig->BedLocation.Z)>9.f||Floor.ImpactPoint.Z<315.f
                ||PigUpgradeLowestVertex(Body)<Floor.ImpactPoint.Z-2.f||FMath::Abs(Body->GetComponentRotation().Roll)<45.f||Pig->GetVelocity().Size2D()>3.f)
            { Finish(false,TEXT("Sleeping pose is not a supported lying pig on the actual floor mattress")); return; }
            S.SleepBodyZ[i]=Body->GetRelativeLocation().Z; S.SleepBodyScaleZ[i]=Body->GetRelativeScale3D().Z;
        }
        S.BigWakeBefore=S.Pigs[1]->WakeEvents; S.SmallWakeBefore=S.Pigs[0]->WakeEvents;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_SLEEP grounded=2 separateSlots=1 physicallyApproached=1"));
        Capture(TEXT("pigs_sleeping_mattress.png")); Advance(4); return;
    }
    if (S.Stage==4)
    {
        for (int32 i=0;i<2;++i)
        {
            UStaticMeshComponent* Body=PigUpgradePart(S.Pigs[i].Get(),TEXT("PigBody"));
            if (!S.Pigs[i]->IsSleepingOnBed()) { Finish(false,TEXT("Pig woke before its requested bed breathing check")); return; }
            S.bBreathing[i]|=FMath::Abs(Body->GetRelativeLocation().Z-S.SleepBodyZ[i])>.015f
                ||FMath::Abs(Body->GetRelativeScale3D().Z-S.SleepBodyScaleZ[i])>.0001f;
        }
        if (Now-S.StageBegan<1.5||!CaptureSaved()) return;
        if (!S.bBreathing[0]||!S.bBreathing[1]) { Finish(false,TEXT("Sleeping pigs do not visibly breathe")); return; }
        AHouseRoamingPig* Big=S.Pigs[1].Get(); const FVector Aim=Big->GetInteractionLocation();
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PigUpgradeWeaponHit),false,Pawn); Query.AddIgnoredActor(S.Pigs[0].Get());
        FHitResult Hit; FVector TraceStart=FVector::ZeroVector; bool BodyHit=false;
        // The north bed is close to the north wall. Choose an actual clear
        // weapon origin inside the room, not an arbitrary side beyond that wall.
        for (const FVector& Direction:{-Big->GetActorRightVector(),Big->GetActorForwardVector(),-Big->GetActorForwardVector(),Big->GetActorRightVector()})
        {
            const FVector Candidate=Aim+Direction*150.f;
            if (Candidate.X<Big->PigstyMinimum.X+15.f||Candidate.X>Big->PigstyMaximum.X-15.f
                ||Candidate.Y<Big->PigstyMinimum.Y+15.f||Candidate.Y>Big->PigstyMaximum.Y-15.f) continue;
            if (World->OverlapBlockingTestByChannel(Candidate,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(1.f),Query)) continue;
            const bool Blocking=World->LineTraceSingleByChannel(Hit,Candidate,Aim,ECC_Visibility,Query);
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_SLEEP_WEAPON_TRACE origin=%s target=%s blocked=%d actor=%s component=%s"),
                *Candidate.ToString(),*Aim.ToString(),Blocking?1:0,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()));
            if (Blocking&&Hit.GetActor()==Big) { TraceStart=Candidate; BodyHit=true; break; }
        }
        if (!BodyHit)
        { Finish(false,TEXT("Real weapon trace misses the sleeping larger pig's resized hitbox")); return; }
        const float Before=Big->Health;
        UGameplayStatics::ApplyPointDamage(Big,25.f,(Aim-TraceStart).GetSafeNormal(),Hit,PC,Pawn,UDamageType::StaticClass());
        S.DamagedHealth=Before-25.f;
        if (FMath::Abs(Big->Health-S.DamagedHealth)>.01f||Big->IsSleepingOnBed()||Big->WakeEvents<=S.BigWakeBefore
            ||!S.Pigs[0]->IsSleepingOnBed()||S.Pigs[0]->Health!=S.Pigs[0]->MaxHealth||S.Pigs[0]->WakeEvents!=S.SmallWakeBefore)
        { Finish(false,TEXT("Individual sleeping damage/health/wake affected the wrong pig or failed")); return; }
        Advance(5); return;
    }
    if (S.Stage==5)
    {
        if (!S.Pigs[1]->RequestSleep()) { Finish(false,TEXT("Damaged larger pig cannot return to sleep")); return; }
        Advance(6); return;
    }
    if (S.Stage==6)
    {
        if (Now-S.StageBegan>15.) { Finish(false,TEXT("Damaged pig did not return to its mattress")); return; }
        if (!S.Pigs[1]->IsSleepingOnBed()) return;
        // Rejection checks use the same production authority API as E, without
        // accidentally opening a door through the E fallback during this phase.
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),S.Pigs[0]->GetInteractionLocation()))
        { Finish(false,TEXT("Out-of-range pickup observer point is blocked")); return; }
        if (Pawn->FocusedHousePigForInteraction()||S.Pigs[0]->TryPickup(Pawn)||Pawn->CarriedHousePig())
        { Finish(false,TEXT("A pig can be grabbed from outside interaction range")); return; }
        // Physically walk one pig to an inside approach near the closed door.
        // The separate visibility test below is within pickup distance, so it
        // cannot succeed merely because the wall-hidden pig is too far away.
        AHouseRoamingPig* Big=S.Pigs[1].Get(); int32 Nearest=INDEX_NONE; float Distance=BIG_NUMBER;
        FHousePigRoamNode Node; Node.Feet=FVector(-2415,-1245,302.3); Node.bInsidePigsty=true;
        for (int32 n=0;n<Big->RoamGraph.Num();++n) if (Big->RoamGraph[n].bInsidePigsty)
        {
            const float D=FVector::DistSquared(Big->RoamGraph[n].Feet,Node.Feet);
            if (D<Distance) { Distance=D; Nearest=n; }
        }
        if (Nearest==INDEX_NONE) { Finish(false,TEXT("No inside node for in-range wall pickup check")); return; }
        Node.Links.Add(Nearest); S.BlockedPickupNode=Big->RoamGraph.Add(Node);
        Big->RoamGraph[Nearest].Links.AddUnique(S.BlockedPickupNode);
        if (!PlacePlayer(World,Pawn,PC,FVector(-2130,-1245,302.3),Big->GetInteractionLocation())||!Big->RequestRoamNode(S.BlockedPickupNode))
        { Finish(false,TEXT("Closed-door visibility approach cannot be requested physically")); return; }
        Advance(7); return;
    }
    if (S.Stage==7)
    {
        AHouseRoamingPig* Big=S.Pigs[1].Get();
        if (Now-S.StageBegan>20.) { Finish(false,TEXT("Pig did not physically reach closed-door visibility approach")); return; }
        // Ordinary roam goals deliberately stop within 22cm. Wait for that
        // physically finished approach, then choose a clear nearby hall point.
        if (FVector::Dist2D(Big->FeetLocation(),Big->RoamGraph[S.BlockedPickupNode].Feet)>22.5f||Big->GetVelocity().Size2D()>5.f) return;
        float ViewDistance=BIG_NUMBER; bool ViewReady=false;
        for (float X:{-2280.f,-2265.f,-2250.f})
        {
            if (!PlacePlayer(World,Pawn,PC,FVector(X,-1245,302.3),Big->GetInteractionLocation())) continue;
            ViewDistance=FVector::Distance(Pawn->FirstPersonCamera->GetComponentLocation(),Big->GetInteractionLocation());
            if (ViewDistance<=215.f) { ViewReady=true; break; }
        }
        if (!ViewReady) { Finish(false,TEXT("No physically clear closed-door player viewpoint is within pickup distance")); return; }
        FCollisionQueryParams WallQuery(SCENE_QUERY_STAT(PigUpgradeClosedDoorLOS),false,Pawn); WallQuery.AddIgnoredActor(Big);
        FHitResult SightHit,BodyHit;
        const FVector Eye=Pawn->FirstPersonCamera->GetComponentLocation(),Target=Big->GetInteractionLocation();
        const bool SightBlocked=World->LineTraceSingleByChannel(SightHit,Eye,Target,ECC_Visibility,WallQuery);
        const bool BodyBlocked=World->LineTraceSingleByChannel(BodyHit,Eye,Target,ECC_Pawn,WallQuery);
        AHouseRoamingPig* Focused=Pawn->FocusedHousePigForInteraction();
        const bool CanPickup=Big->CanPickup(Pawn),Picked=Big->TryPickup(Pawn),DoorOpen=S.Door->IsOpen();
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_CLOSED_DOOR_TEST viewDistance=%.1f doorOpen=%d focused=%s canPickup=%d picked=%d carried=%s sightBlocked=%d sightActor=%s sightComponent=%s bodyBlocked=%d bodyActor=%s bodyComponent=%s"),
            ViewDistance,DoorOpen?1:0,*GetNameSafe(Focused),CanPickup?1:0,Picked?1:0,*GetNameSafe(Pawn->CarriedHousePig()),
            SightBlocked?1:0,*GetNameSafe(SightHit.GetActor()),*GetNameSafe(SightHit.GetComponent()),
            BodyBlocked?1:0,*GetNameSafe(BodyHit.GetActor()),*GetNameSafe(BodyHit.GetComponent()));
        if (DoorOpen||Focused||CanPickup||Picked||Pawn->CarriedHousePig()||(!SightBlocked&&!BodyBlocked))
        { Finish(false,TEXT("An in-range pig can be picked up through the closed door")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_PICKUP_REJECTIONS range=1 closedDoor=1 wallTestViewDistance=%.1f"),ViewDistance);
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2825,-900,355))||!Big->RequestSleep())
        { Finish(false,TEXT("Pig cannot return from door test to its mattress")); return; }
        S.CarryIndex=0; Advance(10); return;
    }
    if (S.Stage==10)
    {
        AHouseRoamingPig* Pig=S.Pigs[S.CarryIndex].Get();
        if (!S.bSleepRequestedOnEntry)
        {
            if (!Pig->IsSleepingOnBed()&&!Pig->RequestSleep()) { Finish(false,TEXT("Pig cannot wait safely for pickup")); return; }
            S.bSleepRequestedOnEntry=true;
        }
        // The first placed pig must finish its own normal bed approach before
        // the observer moves near it for the second pickup. Otherwise a rightly
        // spooked peer can run into every safe placement candidate below.
        const bool PeerReady=S.CarryIndex==0||S.Pigs[0]->IsSleepingOnBed();
        if (!Pig->IsSleepingOnBed()||!PeerReady)
        {
            if (Now-S.StageBegan>20.) { Finish(false,TEXT("Pigs did not finish their separate bed approaches before actual pickup")); return; }
            return;
        }
        if (!PigUpgradeInteractionView(World,Pawn,PC,Pig)) { Finish(false,TEXT("No normal visible, in-range pickup viewpoint can focus the pig")); return; }
        S.PickupsBefore=Pig->Pickups; S.PlacesBefore=Pig->Placements; Advance(11); return;
    }
    if (S.Stage==11)
    {
        if (Now-S.StageBegan<.2) return;
        AHouseRoamingPig* Pig=S.Pigs[S.CarryIndex].Get();
        if (Pawn->FocusedHousePigForInteraction()!=Pig||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual E pickup cannot target the visible pig")); return; }
        Advance(12); return;
    }
    if (S.Stage==12) { SendE(PC,IE_Released); Advance(13); return; }
    if (S.Stage==13)
    {
        if (Now-S.StageBegan<.85) return;
        AHouseRoamingPig* Pig=S.Pigs[S.CarryIndex].Get();
        if (Pawn->CarriedHousePig()!=Pig||!Pig->IsCarried()||Pig->Carrier!=Pawn||Pig->Pickups!=S.PickupsBefore+1||Pig->GetVelocity().Size()>1.f
            ||Pig->GetCharacterMovement()->MovementMode!=MOVE_None)
        { Finish(false,TEXT("Actual player E failed to carry the pig and suspend AI movement")); return; }
        S.CarriedTravel=Pig->TravelDistance; S.CarriedGait=Pig->GaitDistance; S.PigCarryStart=Pig->GetActorLocation();
        const FVector Target(-2675,-1230,302.3);
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FRotator Yaw(0,(Target-Feet).Rotation().Yaw,0); PC->SetControlRotation(Yaw); Pawn->SetActorRotation(Yaw);
        S.PlayerMoveStart=Pawn->GetActorLocation();
        if (!SendPigUpgradeKey(PC,EKeys::W,IE_Pressed)) { Finish(false,TEXT("Actual W carry movement input could not be sent")); return; }
        S.bMoveKeyDown=true; Advance(14); return;
    }
    if (S.Stage==14)
    {
        AHouseRoamingPig* Pig=S.Pigs[S.CarryIndex].Get();
        if (!Pig->IsCarried()||Pawn->CarriedHousePig()!=Pig) { Finish(false,TEXT("Pig detached while its player physically carried it")); return; }
        S.CarryMoveDistance[S.CarryIndex]=FVector::Dist2D(S.PlayerMoveStart,Pawn->GetActorLocation());
        if (S.CarryMoveDistance[S.CarryIndex]<90.f&&Now-S.StageBegan<1.2) return;
        SendPigUpgradeKey(PC,EKeys::W,IE_Released); S.bMoveKeyDown=false;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_CARRY_MOVEMENT name=%s playerDelta=%.2f pigDelta=%.2f velocity=%.2f gaitDelta=%.2f mood=%d held=%d player=%s pig=%s"),
            *Pig->PigName,S.CarryMoveDistance[S.CarryIndex],FVector::Dist2D(S.PigCarryStart,Pig->GetActorLocation()),
            Pig->GetVelocity().Size(),Pig->GaitDistance-S.CarriedGait,static_cast<int32>(Pig->Mood),Pig->IsCarried()?1:0,
            *Pawn->GetActorLocation().ToString(),*Pig->GetActorLocation().ToString());
        if (S.CarryMoveDistance[S.CarryIndex]<50.f||FVector::Dist2D(S.PigCarryStart,Pig->GetActorLocation())<40.f
            ||Pig->GetVelocity().Size()>1.f||Pig->Mood!=EHousePigMood::Carried||FMath::Abs(Pig->GaitDistance-S.CarriedGait)>2.f)
        { Finish(false,TEXT("Physical carried movement did not work or carried pig continued walking AI/gait")); return; }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        // Observe the held pose after actual carrying, before looking down to
        // place it. A snapshot at the initial turn can miss the interpolated pig.
        Capture(S.CarryIndex==0?TEXT("small_pig_carried.png"):TEXT("large_pig_carried.png"));
        Advance(15); return;
    }
    if (S.Stage==15)
    {
        if (Now-S.StageBegan<.25) return;
        if (!CaptureSaved()) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Actual carry screenshot failed")); return; }
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FVector Aim=Feet+Pawn->GetActorForwardVector()*155.f;
        PC->SetControlRotation((Aim-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_BEFORE_PLACE name=%s held=%d carrier=%s focusedDoor=%s player=%s pig=%s view=%s facing=%s"),
            *S.Pigs[S.CarryIndex]->PigName,S.Pigs[S.CarryIndex]->IsCarried()?1:0,*GetNameSafe(Pawn->CarriedHousePig()),
            *GetNameSafe(Pawn->FocusedDoorForInteraction()),*Pawn->GetActorLocation().ToString(),
            *S.Pigs[S.CarryIndex]->GetActorLocation().ToString(),*Pawn->FirstPersonCamera->GetComponentLocation().ToString(),*PC->GetControlRotation().ToString());
        if (!SendE(PC,IE_Pressed)) { Finish(false,TEXT("Actual E placement input could not be sent")); return; }
        Advance(16); return;
    }
    if (S.Stage==16) { SendE(PC,IE_Released); Advance(17); return; }
    if (S.Stage==17)
    {
        if (Now-S.StageBegan<.5) return;
        AHouseRoamingPig* Pig=S.Pigs[S.CarryIndex].Get(); FHitResult Floor;
        const bool FloorSupported=PigUpgradeFloor(World,Pig,Pawn,Floor);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_AFTER_PLACE name=%s held=%d carrier=%s placements=%d expected=%d inside=%d floorSupported=%d floor=%s actor=%s feet=%s"),
            *Pig->PigName,Pig->IsCarried()?1:0,*GetNameSafe(Pawn->CarriedHousePig()),Pig->Placements,S.PlacesBefore+1,
            Pig->IsInsidePigsty()?1:0,FloorSupported?1:0,*GetNameSafe(Floor.GetActor()),*Pig->GetActorLocation().ToString(),*Pig->FeetLocation().ToString());
        if (Pig->IsCarried()||Pawn->CarriedHousePig()||Pig->Placements!=S.PlacesBefore+1||!Pig->IsInsidePigsty()
            ||!FloorSupported)
        { Finish(false,TEXT("Actual E did not put the carried pig safely back on the room's floor")); return; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PigUpgradePlacedSpace),false,Pig);
        if (World->OverlapBlockingTestByChannel(Pig->GetActorLocation(),Pig->GetActorQuat(),ECC_Pawn,
            FCollisionShape::MakeCapsule(Pig->GetCapsuleComponent()->GetScaledCapsuleRadius()*.96f,
                Pig->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.96f),Query))
        { Finish(false,TEXT("Placed pig overlaps a wall, object, player or other pig")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_CARRY_DONE name=%s actualE=1 actualW=1 moved=%.1f safelyPlaced=1"),*Pig->PigName,S.CarryMoveDistance[S.CarryIndex]);
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2825,-900,355)))
        { Finish(false,TEXT("Post-placement observer point is blocked")); return; }
        Pig->RequestSleep();
        if (++S.CarryIndex<2) { Advance(10); return; }
        Advance(20); return;
    }
    if (S.Stage==20)
    {
        if (!S.bSleepRequestedOnEntry)
        {
            for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
                if (!Ref->IsSleepingOnBed()&&!Ref->RequestSleep())
                { Finish(false,TEXT("Safely placed pig could not resume its mattress sleep approach")); return; }
            S.bSleepRequestedOnEntry=true;
        }
        if (!S.Pigs[0]->IsSleepingOnBed()||!S.Pigs[1]->IsSleepingOnBed())
        {
            if (Now-S.StageBegan>30.) { Finish(false,TEXT("Both safely placed pigs did not resume mattress sleeping AI")); return; }
            return;
        }
        FVector Label[2]; FVector2D Screen[2];
        if (!PlacePlayer(World,Pawn,PC,FVector(-2675,-1220,302.3),FVector(-2825,-910,340)))
        { Finish(false,TEXT("Final separate-healthbar observer point is blocked")); return; }
        for (int32 i=0;i<2;++i)
        {
            if (!S.Pigs[i]->ShouldShowHealthTo(PC,Label[i])||!PC->ProjectWorldLocationToScreen(Label[i],Screen[i],true))
            { Finish(false,TEXT("Each different-sized pig cannot display its independent health bar")); return; }
        }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_HUD labels=[%s,%s] HP=[%.1f/%.1f,%.1f/%.1f]"),
            *Screen[0].ToString(),*Screen[1].ToString(),S.Pigs[0]->Health,S.Pigs[0]->MaxHealth,S.Pigs[1]->Health,S.Pigs[1]->MaxHealth);
        Capture(TEXT("pigs_returned_health.png")); Advance(21); return;
    }
    if (S.Stage==21)
    {
        if (!CaptureSaved()) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Final healthbar snapshot failed")); return; }
        S.Food->RefillInterval=60.f;
        // A new physical feeding approach after placement proves that normal AI
        // resumes and leaves visible missing food for the later occupancy test.
        for (int32 i=0;i<2;++i)
        {
            S.AteBefore[i]=S.Pigs[i]->SnacksEaten;
            if (!S.Pigs[i]->RequestFeeding()) { Finish(false,TEXT("Placed pig did not resume its feeding AI")); return; }
        }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2675,-1110,350)))
        { Finish(false,TEXT("Returned feeding observer point is blocked")); return; }
        Advance(22); return;
    }
    if (S.Stage==22)
    {
        if (Now-S.StageBegan>25.) { Finish(false,TEXT("Returned pigs did not eat enough real food to check occupancy pause")); return; }
        if (S.Pigs[0]->SnacksEaten<=S.AteBefore[0]||S.Pigs[1]->SnacksEaten<=S.AteBefore[1]||S.Food->FoodRemaining>16) return;
        if (!PlacePlayer(World,Pawn,PC,FVector(-2130,-1245,302.3),S.Door->DoorwayCenter))
        { Finish(false,TEXT("Player cannot stand normally outside the closed pigsty door")); return; }
        Advance(30); return;
    }
    if (S.Stage==30)
    {
        if (Now-S.StageBegan<.2) return;
        if (Pawn->FocusedDoorForInteraction()!=S.Door.Get()||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual player E cannot open the preserved pigsty door")); return; }
        Advance(31); return;
    }
    if (S.Stage==31) { SendE(PC,IE_Released); Advance(32); return; }
    if (S.Stage==32)
    {
        if (Now-S.StageBegan<1.) return;
        if (!S.Door->IsOpen()||S.Door->IsActorTickEnabled()) { Finish(false,TEXT("Door did not physically finish actual E opening")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2334.8,-1245,350)))
        { Finish(false,TEXT("Escape observer point is blocked")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); S.EscapeBefore[i]=Pig->Escapes;
            int32 Goal=INDEX_NONE; float Best=BIG_NUMBER; const FVector Target(-2177,i==0?-350.f:300.f,302.3);
            for (int32 n=0;n<Pig->RoamGraph.Num();++n) if (!Pig->RoamGraph[n].bInsidePigsty)
            {
                const float Distance=FVector::DistSquared(Pig->RoamGraph[n].Feet,Target);
                if (Distance<Best) { Goal=n; Best=Distance; }
            }
            if (!Pig->RequestRoamNode(Goal)) { Finish(false,TEXT("Open room door does not permit the real escape route")); return; }
        }
        Advance(40); return;
    }
    if (S.Stage==40)
    {
        if (Now-S.StageBegan>35.) { Finish(false,TEXT("Both resized pigs did not physically escape after player opened their door")); return; }
        if (S.Pigs[0]->IsInsidePigsty()||S.Pigs[1]->IsInsidePigsty()||S.Pigs[0]->Escapes<=S.EscapeBefore[0]||S.Pigs[1]->Escapes<=S.EscapeBefore[1]) return;
        if (S.Food->FoodRemaining>=S.Food->FoodCount) { Finish(false,TEXT("Food pause check requires visible missing pieces")); return; }
        S.PausedFood=S.Food->FoodRemaining; S.PausedRefills=S.Food->FoodRespawns; S.Food->RefillInterval=1.f;
        Advance(41); return;
    }
    if (S.Stage==41)
    {
        if (S.Pigs[0]->IsInsidePigsty()||S.Pigs[1]->IsInsidePigsty()) { Finish(false,TEXT("A pig returned during the bounded outside-only refill check")); return; }
        if (S.Food->FoodRemaining!=S.PausedFood||S.Food->FoodRespawns!=S.PausedRefills)
        { Finish(false,TEXT("Cheetos kept respawning with neither pig in its room")); return; }
        if (Now-S.StageBegan<3.) return;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_FOOD_PAUSED food=%d refills=%d elapsed=%.2f bothOutside=1"),
            S.Food->FoodRemaining,S.Food->FoodRespawns,Now-S.StageBegan);
        AHouseRoamingPig* Big=S.Pigs[1].Get(); float Nearest=BIG_NUMBER;
        for (int32 n=0;n<Big->RoamGraph.Num();++n)
        {
            const FHousePigRoamNode& Node=Big->RoamGraph[n];
            if (Node.bInsidePigsty||!Node.GateDoorId.IsNone()||Node.Feet.Z<280.f||Node.Feet.Z>330.f||Node.Feet.X<-2340.f) continue;
            const float D=FVector::DistSquared(Big->FeetLocation(),Node.Feet);
            if (D<Nearest) { S.HallPickupNode=n; Nearest=D; }
        }
        if (S.HallPickupNode==INDEX_NONE||!Big->RequestRoamNode(S.HallPickupNode))
        { Finish(false,TEXT("Escaped pig has no physical upstairs hall pickup approach")); return; }
        Advance(50); return;
    }
    if (S.Stage==50)
    {
        if (Now-S.StageBegan>15.) { Finish(false,TEXT("Escaped larger pig did not reach a safe actual hall pickup approach")); return; }
        AHouseRoamingPig* Big=S.Pigs[1].Get();
        if (Big->IsInsidePigsty()||Big->FeetLocation().Z<280.f) { Finish(false,TEXT("Escaped pig left the upstairs hall return context")); return; }
        if (FVector::Dist2D(Big->FeetLocation(),Big->RoamGraph[S.HallPickupNode].Feet)>30.f||Big->GetVelocity().Size2D()>15.f) return;
        if (!PigUpgradeHallInteractionView(World,Pawn,PC,Big)) { Finish(false,TEXT("No in-range visible player viewpoint can pick up escaped larger pig in hall")); return; }
        S.HallReturnPickups=Big->Pickups; S.HallReturnPlacements=Big->Placements;
        Advance(51); return;
    }
    if (S.Stage==51)
    {
        AHouseRoamingPig* Big=S.Pigs[1].Get();
        const FRotator Aim=(Big->GetInteractionLocation()-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
        PC->SetControlRotation(Aim);
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        if (Now-S.StageBegan<.2) return;
        if (Pawn->FocusedHousePigForInteraction()!=Big||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual E could not pick up escaped, independently moving larger pig in hall")); return; }
        Advance(52); return;
    }
    if (S.Stage==52) { SendE(PC,IE_Released); Advance(53); return; }
    if (S.Stage==53)
    {
        if (Now-S.StageBegan<.85) return;
        AHouseRoamingPig* Big=S.Pigs[1].Get();
        if (Pawn->CarriedHousePig()!=Big||!Big->IsCarried()||Big->Pickups!=S.HallReturnPickups+1
            ||Big->GetCharacterMovement()->MovementMode!=MOVE_None||Big->Mood!=EHousePigMood::Carried)
        { Finish(false,TEXT("Escaped pig's actual E pickup failed to suspend its AI")); return; }
        if (!S.Door->IsOpen()||S.Door->IsActorTickEnabled()||!PigUpgradeCarrySpace(World,Big,Pawn))
        { Finish(false,TEXT("Escaped pig carry starts penetrating house geometry or gate is not open")); return; }
        S.HallCarryPlayerStart=S.HallCarryLastPlayer=Pawn->GetActorLocation(); S.ReturnPoint=0;
        S.CarriedGait=Big->GaitDistance;
        const FVector PlayerFeet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        const FRotator Facing(0,(FVector(-2177,-1245,302.3)-PlayerFeet).Rotation().Yaw,0);
        PC->SetControlRotation(Facing); Pawn->SetActorRotation(Facing);
        Capture(TEXT("escaped_large_pig_carried.png"));
        if (!SendPigUpgradeKey(PC,EKeys::W,IE_Pressed)) { Finish(false,TEXT("Physical W return movement could not be sent")); return; }
        S.bMoveKeyDown=true; Advance(54); return;
    }
    if (S.Stage==54)
    {
        AHouseRoamingPig* Big=S.Pigs[1].Get();
        if (Now-S.StageBegan>25.) { Finish(false,TEXT("Actual player W carrying did not reach pigsty through open doorway")); return; }
        if (!Big->IsCarried()||Pawn->CarriedHousePig()!=Big||Big->GetVelocity().Size()>1.f||Big->Mood!=EHousePigMood::Carried
            ||FMath::Abs(Big->GaitDistance-S.CarriedGait)>2.f)
        { Finish(false,TEXT("Escaped pig resumed AI or detached during physical doorway return")); return; }
        if (!PigUpgradeCarrySpace(World,Big,Pawn)) { Finish(false,TEXT("Full carried pig body penetrated a hallway wall, door or object")); return; }
        S.HallCarryDistance+=FVector::Dist2D(S.HallCarryLastPlayer,Pawn->GetActorLocation()); S.HallCarryLastPlayer=Pawn->GetActorLocation();
        const TArray<FVector> Points={FVector(-2177,-1245,302.3),FVector(-2320,-1245,302.3),FVector(-2440,-1245,302.3),FVector(-2650,-1245,302.3)};
        const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        if (FVector::Dist2D(Feet,Points[S.ReturnPoint])<22.f)
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_RETURN_WAYPOINT index=%d playerFeet=%s pigFeet=%s distance=%.1f"),
                S.ReturnPoint,*Feet.ToString(),*Big->FeetLocation().ToString(),S.HallCarryDistance);
            if (++S.ReturnPoint>=Points.Num())
            {
                SendPigUpgradeKey(PC,EKeys::W,IE_Released); S.bMoveKeyDown=false;
                Pawn->GetCharacterMovement()->StopMovementImmediately();
                const FVector PigFeet=Big->FeetLocation();
                const bool InRoomXY=PigFeet.X>=Big->PigstyMinimum.X&&PigFeet.X<=Big->PigstyMaximum.X
                    &&PigFeet.Y>=Big->PigstyMinimum.Y&&PigFeet.Y<=Big->PigstyMaximum.Y;
                if (!InRoomXY||S.HallCarryDistance<400.f)
                { Finish(false,TEXT("Escaped pig did not actually cross from house hall back into pigsty")); return; }
                Advance(55); return;
            }
        }
        const FRotator Facing(0,(Points[S.ReturnPoint]-Feet).Rotation().Yaw,0);
        PC->SetControlRotation(Facing); Pawn->SetActorRotation(Facing); return;
    }
    if (S.Stage==55)
    {
        if (Now-S.StageBegan<.3) return;
        if (!CaptureSaved()) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Escaped pig carry snapshot failed")); return; }
        if (!SendE(PC,IE_Pressed)) { Finish(false,TEXT("Actual E cannot place escaped pig back inside its room")); return; }
        Advance(56); return;
    }
    if (S.Stage==56) { SendE(PC,IE_Released); Advance(57); return; }
    if (S.Stage==57)
    {
        if (Now-S.StageBegan<.5) return;
        AHouseRoamingPig* Big=S.Pigs[1].Get(); FHitResult Floor;
        if (Pawn->CarriedHousePig()||Big->IsCarried()||Big->Placements!=S.HallReturnPlacements+1
            ||!Big->IsInsidePigsty()||!PigUpgradeFloor(World,Big,Pawn,Floor))
        { Finish(false,TEXT("Actual E failed to place escaped pig safely back on pigsty floor")); return; }
        FCollisionQueryParams Query(SCENE_QUERY_STAT(PigUpgradeReturnedCapsule),false,Big);
        if (World->OverlapBlockingTestByChannel(Big->GetActorLocation(),Big->GetActorQuat(),ECC_Pawn,
            FCollisionShape::MakeCapsule(Big->GetCapsuleComponent()->GetScaledCapsuleRadius()*.96f,
                Big->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.96f),Query))
        { Finish(false,TEXT("Returned escaped pig overlaps player, wall or another pig")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_UPGRADE_ESCAPED_RETURN actualE=1 actualW=1 fullBodyClear=1 floor=1 distance=%.1f"),S.HallCarryDistance);
        Capture(TEXT("escaped_pig_returned.png")); Advance(58); return;
    }
    if (S.Stage==58)
    {
        if (!CaptureSaved()) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Escaped pig return capture failed")); return; }
        for (const FString& Path:S.Captures) if (IFileManager::Get().FileSize(*Path)<=0)
        { Finish(false,TEXT("A requested detail, sleep, carry, return or HUD capture was missing")); return; }
        Finish(true,TEXT("Different smaller grounded pigs have faster little-pig movement and greater big-pig health; visible Cheetos eaten and replenished only while occupied; separate mattress sleep/breathing and damage wake verified; actual E pickup/place and W physical carrying passed for both; range/wall rejection, closed-door containment and player-open escape passed; escaped larger pig physically carried through open doorway and safely placed back in pigsty with no body penetration"));
    }
}
struct FPigMotionMetrics
{
    bool Initialized=false;
    int32 Samples=0, PlantSamples=0, Footfalls[4]={0,0,0,0}, DiagnosticLines[4]={0,0,0,0};
    bool Planted[4]={false,false,false,false};
    FVector PlantStart[4], LastHoof[4];
    float PlantAge[4]={0,0,0,0}, MaxPlantDrift=0.f, Distance=0.f;
    FVector LastFeet=FVector::ZeroVector;
    float LastYaw=0.f;
    TArray<float> FloorGaps;
    TArray<TPair<int32,float>> Events;
};
struct FPigMotionSmoke
{
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs;
    TWeakObjectPtr<AExistingHouseDoor> Door;
    int32 Stage=0, LastStage=-1, Index=0, RoutePoint=0, GateNode=INDEX_NONE, FailedStage=INDEX_NONE, PlaceAttempt=0;
    TArray<int32> Course[2];
    FPigMotionMetrics Walk[2], Run[2];
    double WallBegan=0;
    float Began=0, StageAt=0, PickupAt=0, HeldAt=0, PlaceAt=0, PlaceYaw=0;
    bool Entry=false, Moving=false, ReachObserved[2]={false,false}, LiftObserved[2]={false,false}, LowerObserved[2]={false,false}, ContactBeforeLift[2]={false,false};
    FVector PickupRoot, InitialHand, PlayerStart, HoldRoot, LastGrip;
    FQuat FingerBefore=FQuat::Identity;
    float HandErrorMax[2]={0,0}, FingerChange[2]={0,0}, LowerDuration[2]={0,0}, CarryDistance[2]={0,0}, SwingMax[2]={0,0};
    float ReachContactMin[2]={BIG_NUMBER,BIG_NUMBER};
    TArray<float> HandErrors[2];
    TArray<float> MovingHandErrors[2];
    int32 GripSamples[2]={0,0}, PlacementsBefore=0;
    FString Capture;
    FString PendingFailure;
    TArray<FString> Captures;
};

FVector PigMotionSole(UStaticMeshComponent* Part)
{
    UStaticMesh* Mesh=Part?Part->GetStaticMesh():nullptr;
    if (!Mesh||!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.IsEmpty()) return FVector::ZeroVector;
    const FPositionVertexBuffer& V=Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    float Bottom=BIG_NUMBER;
    for (uint32 i=0;i<V.GetNumVertices();++i) Bottom=FMath::Min(Bottom,V.VertexPosition(i).Z);
    FVector Sum=FVector::ZeroVector; int32 Count=0;
    for (uint32 i=0;i<V.GetNumVertices();++i)
        if (V.VertexPosition(i).Z<Bottom+.2f) { Sum+=FVector(V.VertexPosition(i)); ++Count; }
    return Count?Part->GetComponentTransform().TransformPosition(Sum/Count):FVector::ZeroVector;
}
float PigMotionPercentile(TArray<float> Values,float Fraction)
{
    if (Values.IsEmpty()) return BIG_NUMBER;
    Values.Sort(); return Values[FMath::Clamp(FMath::FloorToInt((Values.Num()-1)*Fraction),0,Values.Num()-1)];
}
FQuat PigMotionFinger(AStoryFirstPersonCharacter* Pawn)
{
    if (!Pawn->FirstPersonArms||Pawn->FirstPersonArms->GetBoneIndex(TEXT("index_02_r"))==INDEX_NONE) return FQuat::Identity;
    return Pawn->FirstPersonArms->GetSocketTransform(TEXT("index_02_r")).GetRelativeTransform(
        Pawn->FirstPersonArms->GetSocketTransform(TEXT("index_01_r"))).GetRotation();
}
void PigMotionObserveGait(UWorld* World,AHouseRoamingPig* Pig,APawn* Observer,FPigMotionMetrics& M,float Now,float Delta)
{
    const FVector Feet=Pig->FeetLocation();
    if (!M.LastFeet.IsZero()) M.Distance+=FVector::Dist2D(Feet,M.LastFeet);
    M.LastFeet=Feet;
    const float Turn=FMath::Abs(FMath::FindDeltaAngleDegrees(M.LastYaw,Pig->GetActorRotation().Yaw)); M.LastYaw=Pig->GetActorRotation().Yaw;
    const bool Straight=Turn<1.2f && Pig->GetVelocity().Size2D()>45.f;
    FCollisionQueryParams Ground(SCENE_QUERY_STAT(PigMotionHoofFloor),false,Pig); Ground.AddIgnoredActor(Observer);
    for (TActorIterator<APawn> It(World);It;++It) Ground.AddIgnoredActor(*It);
    for (int32 i=0;i<4;++i)
    {
        UStaticMeshComponent* Lower=PigUpgradePart(Pig,*FString::Printf(TEXT("LowerLeg%d"),i));
        const FVector Hoof=PigMotionSole(Lower); if (Hoof.IsZero()) continue;
        const bool Plant=Pig->IsHoofPlanted(i);
        if (!M.Initialized) M.PlantStart[i]=Hoof;
        if (M.Initialized && Straight && Plant && !M.Planted[i]) { ++M.Footfalls[i]; M.Events.Emplace(i,Now); M.PlantStart[i]=Hoof; M.PlantAge[i]=0.f; }
        if (Straight && Plant)
        {
            ++M.PlantSamples; M.PlantAge[i]+=Delta;
            const float Drift=M.Planted[i]&&M.PlantAge[i]>.06f?FVector::Dist2D(M.PlantStart[i],Hoof):0.f;
            M.MaxPlantDrift=FMath::Max(M.MaxPlantDrift,Drift);
            FHitResult Floor;
            if (World->LineTraceSingleByChannel(Floor,Hoof+FVector(0,0,20),Hoof-FVector(0,0,30),ECC_Visibility,Ground)
                && Floor.ImpactNormal.Z>.65f)
            {
                const float Gap=FMath::Abs(Hoof.Z-Floor.ImpactPoint.Z); M.FloorGaps.Add(Gap);
                if ((Gap>8.f||Drift>12.f)&&M.DiagnosticLines[i]<3)
                {
                    ++M.DiagnosticLines[i];
                    UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_HOOF name=%s leg=%d line=%d time=%.3f hoof=%s feet=%s yaw=%.2f speed=%.1f planted=%d plantAge=%.3f plantStart=%s gap=%.2f drift=%.2f trotBlend=%.3f mood=%d floor=%s component=%s floorPoint=%s normal=%s"),
                        *Pig->PigName,i,M.DiagnosticLines[i],Now,*Hoof.ToString(),*Feet.ToString(),Pig->GetActorRotation().Yaw,
                        Pig->GetVelocity().Size2D(),Plant?1:0,M.PlantAge[i],*M.PlantStart[i].ToString(),Gap,Drift,Pig->GetTrotBlend(),int32(Pig->Mood),
                        *GetNameSafe(Floor.GetActor()),*GetNameSafe(Floor.GetComponent()),*Floor.ImpactPoint.ToString(),*Floor.ImpactNormal.ToString());
                }
            }
        }
        if (!Straight || !Plant) { M.PlantStart[i]=Hoof; M.PlantAge[i]=0.f; }
        M.Planted[i]=Plant; M.LastHoof[i]=Hoof;
    }
    if (Straight) ++M.Samples;
    M.Initialized=true;
}
float PigMotionPairedFraction(const FPigMotionMetrics& M)
{
    int32 Paired=0;
    for (int32 i=0;i<M.Events.Num();++i)
        for (int32 j=0;j<M.Events.Num();++j)
            if (i!=j && M.Events[i].Key!=M.Events[j].Key && FMath::Abs(M.Events[i].Value-M.Events[j].Value)<.040f)
            { ++Paired; break; }
    return M.Events.IsEmpty()?0.f:float(Paired)/M.Events.Num();
}
bool PigMotionBodyClear(UWorld* World,AHouseRoamingPig* Pig,APawn* Player)
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigMotionHeldBody),false,Pig); Query.AddIgnoredActor(Player);
    const FVector Center=Pig->GetActorLocation()+Pig->GetActorQuat().RotateVector(FVector(8.5f,0,-2.f)*Pig->VisualScale);
    return !World->OverlapBlockingTestByChannel(Center,Pig->GetActorQuat(),ECC_Pawn,
        FCollisionShape::MakeBox(FVector(78,29,45)*Pig->VisualScale*.97f),Query);
}

bool PigMotionPlaceGaitObserver(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,
    AHouseRoamingPig* Pig,AHouseRoamingPig* Companion,const TArray<int32>& Course)
{
    // Check the actual floor and capsule for every candidate. Keep the observer
    // away from the entire physical route, including the approach from bed.
    const TArray<FVector> Candidates={FVector(-2675,-900,302.3),FVector(-2675,-850,302.3),
        FVector(-2660,-925,302.3),FVector(-2915,-875,302.3),FVector(-2915,-985,302.3)};
    for (const FVector& Candidate:Candidates)
    {
        float RouteDistance=FVector::Dist2D(Candidate,Pig->FeetLocation());
        FVector Previous=Pig->FeetLocation();
        for (int32 Step=0;Step<=Course.Num();++Step)
        {
            FVector A=Previous,B=Pig->RoamGraph[Course[Step%Course.Num()]].Feet;
            A.Z=0; B.Z=0; FVector P=Candidate; P.Z=0;
            RouteDistance=FMath::Min(RouteDistance,float(FVector::Dist2D(P,FMath::ClosestPointOnSegment(P,A,B))));
            Previous=B;
        }
        if (RouteDistance<120.f||FVector::Dist2D(Candidate,Companion->FeetLocation())<85.f)
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_OBSERVER_REJECT feet=%s reason=proximity routeDistance=%.1f companionDistance=%.1f"),
                *Candidate.ToString(),RouteDistance,FVector::Dist2D(Candidate,Companion->FeetLocation()));
            continue;
        }
        if (!PlacePlayer(World,Pawn,PC,Candidate,Pig->GetInteractionLocation(),TEXT("gait"))) continue;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_OBSERVER_SELECTED name=%s feet=%s routeDistance=%.1f companionDistance=%.1f"),
            *Pig->PigName,*Candidate.ToString(),RouteDistance,FVector::Dist2D(Candidate,Companion->FeetLocation()));
        return true;
    }
    return false;
}

bool PigMotionRearAnkleView(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,AHouseRoamingPig* Pig)
{
    // The player must stand close enough to reach the real back hoof with a
    // bent arm. A body-centered, maximum-range interaction viewpoint cannot
    // validate that physical contact. Keep normal capsule and focus checks.
    Pawn->FirstPersonCamera->SetRelativeLocation(FVector(0,0,Pawn->BaseEyeHeight));
    const FVector Ankle=Pig->GetCarryGripPoint(); int32 Clear=0,Reachable=0,ReachLogs=0;
    for (float Radius:{40.f,50.f,60.f,70.f,80.f,35.f,30.f,25.f})
        for (float Angle:{180.f,-135.f,135.f,-90.f,90.f,-45.f,45.f,0.f})
        {
            const FVector Offset=FRotator(0,Pig->GetActorRotation().Yaw+Angle,0).Vector()*Radius;
            const FVector Candidate(Ankle.X+Offset.X,Ankle.Y+Offset.Y,302.3);
            if (!PlacePlayer(World,Pawn,PC,Candidate,Pig->GetInteractionLocation(),TEXT("rear_ankle"))) continue;
            ++Clear;
            const bool Reach=Pawn->CanReachPigGrip(Ankle,Pig->GetCarryGripDirection()); Reachable+=Reach?1:0;
            if (!Reach&&ReachLogs++<3&&Pawn->FirstPersonArms)
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_REACH_REJECT name=%s radius=%.1f player=%s ankle=%s shoulder=%s elbow=%s wrist=%s camera=%s cameraRelativeZ=%.2f capsuleHalf=%.2f eyeHeight=%.2f"),
                    *Pig->PigName,Radius,*Pawn->GetActorLocation().ToString(),*Ankle.ToString(),
                    *Pawn->FirstPersonArms->GetSocketLocation(TEXT("upperarm_r")).ToString(),*Pawn->FirstPersonArms->GetSocketLocation(TEXT("lowerarm_r")).ToString(),
                    *Pawn->FirstPersonArms->GetSocketLocation(TEXT("hand_r")).ToString(),*Pawn->FirstPersonCamera->GetComponentLocation().ToString(),
                    Pawn->FirstPersonCamera->GetRelativeLocation().Z,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Pawn->BaseEyeHeight);
            if (!Reach||Pawn->FocusedHousePigForInteraction()!=Pig) continue;
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_PICKUP_VIEW name=%s feet=%s ankle=%s radius=%.1f reachable=1 actualFocus=1"),
                *Pig->PigName,*Candidate.ToString(),*Ankle.ToString(),Radius);
            return true;
        }
    UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_PICKUP_VIEW_FAILED name=%s ankle=%s clearCandidates=%d reachableCandidates=%d"),
        *Pig->PigName,*Ankle.ToString(),Clear,Reachable);
    return false;
}

void PigMotionLogHeldPose(UWorld* World,AHouseRoamingPig* Pig,AStoryFirstPersonCharacter* Pawn,APlayerController* PC)
{
    const FVector Grip=Pig->GetCarryGripPoint(),Hand=Pawn->GetPigHandGripPoint();
    UStaticMeshComponent* Body=PigUpgradePart(Pig,TEXT("PigBody"));
    UStaticMeshComponent* Rear=PigUpgradePart(Pig,TEXT("LowerLeg2")),*Other=PigUpgradePart(Pig,TEXT("LowerLeg3"));
    const FVector RearPoint=Rear?Rear->GetComponentTransform().TransformPosition(FVector(0,0,-20)):FVector::ZeroVector;
    const FVector OtherPoint=Other?Other->GetComponentTransform().TransformPosition(FVector(0,0,-20)):FVector::ZeroVector;
    FVector View; FRotator ViewRotation; PC->GetPlayerViewPoint(View,ViewRotation);
    UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_HELD_POSE name=%s phase=%d blend=%.3f root=%s rotation=%s grip=%s rear=%s otherRear=%s bodyOrigin=%s hand=%s handGap=%.2f bodyBelowGrip=%.2f otherRearGap=%.2f player=%s view=%s viewRotation=%s"),
        *Pig->PigName,int32(Pig->GetCarryPhase()),Pig->GetCarryBlend(),*Pig->GetActorLocation().ToString(),*Pig->GetActorRotation().ToString(),
        *Grip.ToString(),*RearPoint.ToString(),*OtherPoint.ToString(),*FVector(Body?Body->Bounds.Origin:FVector::ZeroVector).ToString(),
        *Hand.ToString(),FVector::Dist(Grip,Hand),Body?Grip.Z-Body->Bounds.Origin.Z:-BIG_NUMBER,FVector::Dist(Grip,OtherPoint),
        *Pawn->GetActorLocation().ToString(),*View.ToString(),*ViewRotation.ToString());
    // Diagnose the actual full-body path toward the stationary held pose.
    // Query only: no change to pig motion, collision, camera or acceptance.
    const FQuat TargetRotation=FRotator(-80.f,ViewRotation.Yaw+90.f,-8.f).Quaternion();
    const FVector TargetGrip=View+FRotator(0,ViewRotation.Yaw,0).RotateVector(FVector(45,23,-30));
    const FVector LocalGrip=Pig->GetActorTransform().InverseTransformPosition(Grip);
    const FVector Target=TargetGrip-TargetRotation.RotateVector(LocalGrip),Start=Pig->GetActorLocation();
    const FQuat StartRotation=Pig->GetActorQuat();
    const FVector LocalCenter=FVector(8.5,0,-2)*Pig->VisualScale;
    const FCollisionShape Shape=FCollisionShape::MakeBox(FVector(78,29,45)*Pig->VisualScale);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigMotionBlockedHang),false,Pig); Query.AddIgnoredActor(Pawn);
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(FMath::RadiansToDegrees(StartRotation.AngularDistance(TargetRotation))/4.f),1,24);
    FVector PreviousCenter=Start+StartRotation.RotateVector(LocalCenter);
    for (int32 Step=1;Step<=Steps;++Step)
    {
        const float Alpha=float(Step)/Steps;
        const FQuat Facing=FQuat::Slerp(StartRotation,TargetRotation,Alpha).GetNormalized();
        const FVector Center=FMath::Lerp(Start,Target,Alpha)+Facing.RotateVector(LocalCenter);
        TArray<FOverlapResult> Overlaps; World->OverlapMultiByChannel(Overlaps,Center,Facing,ECC_Pawn,Shape,Query);
        int32 Blocking=0;
        for (const FOverlapResult& Hit:Overlaps) if (Hit.bBlockingHit&&Blocking++<3)
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_HANG_BLOCK step=%d/%d reason=overlap actor=%s component=%s center=%s rotation=%s target=%s targetGrip=%s"),
                Step,Steps,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),*Center.ToString(),*Facing.Rotator().ToString(),*Target.ToString(),*TargetGrip.ToString());
        if (Blocking) return;
        FHitResult Hit;
        if (World->SweepSingleByChannel(Hit,PreviousCenter,Center,Facing,ECC_Pawn,Shape,Query))
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_HANG_BLOCK step=%d/%d reason=sweep actor=%s component=%s startPenetrating=%d penetrationDepth=%.2f hitPoint=%s center=%s rotation=%s target=%s targetGrip=%s"),
                Step,Steps,*GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.GetComponent()),Hit.bStartPenetrating?1:0,Hit.PenetrationDepth,
                *Hit.ImpactPoint.ToString(),*Center.ToString(),*Facing.Rotator().ToString(),*Target.ToString(),*TargetGrip.ToString());
            return;
        }
        PreviousCenter=Center;
    }
    UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_HANG_PATH_CLEAR name=%s target=%s targetGrip=%s"),*Pig->PigName,*Target.ToString(),*TargetGrip.ToString());
}

void TickHousePigMotionSmoke(UWorld* World)
{
    if (!World||UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    static FPigMotionSmoke S;
    if (S.World.Get()!=World) { S=FPigMotionSmoke(); S.World=World; }
    if (S.Stage<0) return;
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn||!PC->PlayerInput||!World->GetGameViewport()) return;
    const float Now=World->GetTimeSeconds(), Delta=World->GetDeltaSeconds();
    const bool GripOnly=FParse::Param(FCommandLine::Get(),TEXT("HousePigGripOnly"));
    if (!S.WallBegan) { S.WallBegan=FPlatformTime::Seconds(); S.Began=Now; }
    auto Advance=[&](int32 Stage) { S.Stage=Stage; S.StageAt=Now; S.Entry=false; };
    auto Capture=[&](const FString& Name)
    {
        S.Capture=FPaths::ProjectSavedDir()/TEXT("HousePigMotionSmoke")/(Name+TEXT(".png"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.Capture),true); IFileManager::Get().Delete(*S.Capture,false,true);
        FScreenshotRequest::RequestScreenshot(S.Capture,true,false); S.Captures.Add(S.Capture);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_CAPTURE path=%s"),*S.Capture);
    };
    auto Finish=[&](bool Pass,const FString& Detail)
    {
        if (S.Moving) SendPigUpgradeKey(PC,EKeys::W,IE_Released);
        if (!Pass&&S.Stage!=90)
        {
            S.Moving=false; S.PendingFailure=Detail; S.FailedStage=S.Stage;
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_FAILURE stage=%d index=%d detail=%s"),S.Stage,S.Index,*Detail);
            const FString Name=S.Pigs.IsValidIndex(S.Index)&&S.Pigs[S.Index].IsValid()?S.Pigs[S.Index]->PigName.ToLower():TEXT("setup");
            Capture(FString::Printf(TEXT("%s_failed_stage_%d"),*Name,S.Stage)); Advance(90); return;
        }
        TSharedPtr<FJsonObject> R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("passed"),Pass); R->SetStringField(TEXT("detail"),Detail);
        R->SetBoolField(TEXT("gripOnly"),GripOnly); R->SetStringField(TEXT("scope"),GripOnly?TEXT("pickup, carry, placement and closed-door guard only; gait skipped"):TEXT("gait and pickup motion"));
        R->SetNumberField(TEXT("worldSeconds"),Now-S.Began); R->SetNumberField(TEXT("lastStage"),S.FailedStage==INDEX_NONE?S.Stage:S.FailedStage);
        TArray<TSharedPtr<FJsonValue>> P;
        for (int32 i=0;i<S.Pigs.Num();++i)
        {
            TSharedPtr<FJsonObject> J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("name"),S.Pigs[i]->PigName);
            J->SetNumberField(TEXT("walkDistance"),S.Walk[i].Distance); J->SetNumberField(TEXT("runDistance"),S.Run[i].Distance);
            J->SetNumberField(TEXT("walkContactP95"),PigMotionPercentile(S.Walk[i].FloorGaps,.95f));
            J->SetNumberField(TEXT("runContactP95"),PigMotionPercentile(S.Run[i].FloorGaps,.95f));
            J->SetNumberField(TEXT("walkPlantedDriftMax"),S.Walk[i].MaxPlantDrift); J->SetNumberField(TEXT("runPlantedDriftMax"),S.Run[i].MaxPlantDrift);
            J->SetNumberField(TEXT("walkPairedFootfallFraction"),PigMotionPairedFraction(S.Walk[i]));
            J->SetNumberField(TEXT("runPairedFootfallFraction"),PigMotionPairedFraction(S.Run[i]));
            J->SetNumberField(TEXT("handContactP95"),PigMotionPercentile(S.HandErrors[i],.95f)); J->SetNumberField(TEXT("handContactMax"),S.HandErrorMax[i]);
            J->SetNumberField(TEXT("movingHandContactP95"),PigMotionPercentile(S.MovingHandErrors[i],.95f));
            J->SetNumberField(TEXT("fingerFlexChangeDegrees"),S.FingerChange[i]); J->SetNumberField(TEXT("lowerDuration"),S.LowerDuration[i]);
            J->SetNumberField(TEXT("physicalCarryDistance"),S.CarryDistance[i]); J->SetNumberField(TEXT("swingMaxDegrees"),S.SwingMax[i]);
            J->SetBoolField(TEXT("reachBeforeLift"),S.ReachObserved[i]); J->SetBoolField(TEXT("intermediateLiftObserved"),S.LiftObserved[i]);
            J->SetBoolField(TEXT("handContactBeforeLift"),S.ContactBeforeLift[i]); J->SetNumberField(TEXT("reachContactMin"),S.ReachContactMin[i]);
            J->SetBoolField(TEXT("asyncLowerObserved"),S.LowerObserved[i]); P.Add(MakeShared<FJsonValueObject>(J));
        }
        R->SetArrayField(TEXT("pigs"),P); TArray<TSharedPtr<FJsonValue>> C;
        for (const FString& Path:S.Captures) C.Add(MakeShared<FJsonValueString>(Path)); R->SetArrayField(TEXT("captures"),C);
        FString Json; FJsonSerializer::Serialize(R.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        const FString Path=FPaths::ProjectSavedDir()/TEXT("HousePigMotionSmoke/result.json");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true); FFileHelper::SaveStringToFile(Json,*Path);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_SMOKE_RESULT passed=%d worldSeconds=%.2f detail=%s report=%s"),Pass?1:0,Now-S.Began,*Detail,*Path);
        S.Stage=-1; FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    if (FPlatformTime::Seconds()-S.WallBegan>120.) { Finish(false,TEXT("Bounded motion check timed out")); return; }
    if (S.Stage!=S.LastStage) { UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_STAGE stage=%d index=%d time=%.2f"),S.Stage,S.Index,Now-S.Began); S.LastStage=S.Stage; }
    if (S.Stage==90)
    {
        if (IFileManager::Get().FileSize(*S.Capture)>0||Now-S.StageAt>3.f) Finish(false,S.PendingFailure);
        return;
    }
    if (S.Stage==0)
    {
        if (Now-S.Began<2.f) return;
        Scalability::FQualityLevels Q; Q.SetFromSingleQualityLevel(1); Scalability::SetQualityLevels(Q);
        if (GEngine) { GEngine->Exec(World,TEXT("r.SetRes 1280x720w")); GEngine->Exec(World,TEXT("r.ScreenPercentage 100")); GEngine->Exec(World,TEXT("t.MaxFPS 60")); GEngine->Exec(World,TEXT("DisableAllScreenMessages")); }
        if (AGameModeBase* M=World->GetAuthGameMode()) M->SetActorTickInterval(0.f);
        for (TActorIterator<AHouseRoamingPig> It(World);It;++It) S.Pigs.Add(*It);
        S.Pigs.Sort([](const TWeakObjectPtr<AHouseRoamingPig>& A,const TWeakObjectPtr<AHouseRoamingPig>& B) { return A->VisualScale<B->VisualScale; });
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It) if (It->DoorId==TEXT("HallEnd_West")) S.Door=*It;
        if (S.Pigs.Num()!=2||!S.Door.IsValid()||S.Door->IsOpen()) { Finish(false,TEXT("Motion needs two installed pigs behind the saved closed door")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2825,-920,350))) { Finish(false,TEXT("Sleep setup observer has no clear floor")); return; }
        const TArray<FVector> Points={FVector(-2860,-1215,302.3),FVector(-2440,-1215,302.3),FVector(-2500,-1050,302.3),FVector(-2690,-1060,302.3)};
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); Pig->SleepDuration=120.f;
            for (const FVector& Point:Points) { FHousePigRoamNode N; N.Feet=Point; N.bInsidePigsty=true; S.Course[i].Add(Pig->RoamGraph.Add(N)); }
            for (int32 p=0;p<4;++p)
            { const int32 A=S.Course[i][p],B=S.Course[i][(p+1)%4]; Pig->RoamGraph[A].Links.AddUnique(B); Pig->RoamGraph[B].Links.AddUnique(A); }
            Pig->RoamGraph[S.Course[i][0]].Links.AddUnique(5); Pig->RoamGraph[5].Links.AddUnique(S.Course[i][0]);
            if (!Pig->RequestSleep()) { Finish(false,TEXT("Installed pig cannot physically request mattress sleep")); return; }
        }
        Advance(1); return;
    }
    AHouseRoamingPig* Pig=S.Pigs[S.Index].Get(); const FString Prefix=Pig->PigName.ToLower();
    if (S.Stage==1)
    {
        if (!S.Pigs[0]->IsSleepingOnBed()||!S.Pigs[1]->IsSleepingOnBed()) { if (Now-S.StageAt>18.f) Finish(false,TEXT("Both pigs failed physical mattress preparation")); return; }
        if (GripOnly) { Advance(10); return; }
        if (!PigMotionPlaceGaitObserver(World,Pawn,PC,Pig,S.Pigs[1-S.Index].Get(),S.Course[S.Index]))
        { Finish(false,TEXT("No physically clear gait observer outside pig proximity and route")); return; }
        S.RoutePoint=0; Pig->RequestRoamNode(S.Course[S.Index][0]); Pig->Mood=EHousePigMood::Walking; Advance(2); return;
    }
    if (S.Stage==2)
    {
        Pig->Mood=EHousePigMood::Walking;
        if (FVector::Dist2D(Pig->FeetLocation(),Pig->RoamGraph[S.Course[S.Index][0]].Feet)>24.f) { if (Now-S.StageAt>12.f) Finish(false,TEXT("Pig failed to physically reach gait course")); return; }
        S.RoutePoint=1; Pig->RequestRoamNode(S.Course[S.Index][1]); Pig->Mood=EHousePigMood::Walking; Advance(3); return;
    }
    if (S.Stage==3||S.Stage==4)
    {
        const bool Run=S.Stage==4; Pig->Mood=Run?EHousePigMood::Running:EHousePigMood::Walking;
        const FVector Goal=Pig->RoamGraph[S.Course[S.Index][S.RoutePoint]].Feet;
        if (FVector::Dist2D(Pig->FeetLocation(),Goal)<24.f) { S.RoutePoint=(S.RoutePoint+1)%4; Pig->RequestRoamNode(S.Course[S.Index][S.RoutePoint]); Pig->Mood=Run?EHousePigMood::Running:EHousePigMood::Walking; }
        FPigMotionMetrics& M=Run?S.Run[S.Index]:S.Walk[S.Index];
        PigMotionObserveGait(World,Pig,Pawn,M,Now,Delta);
        const FRotator Look=(Pig->GetInteractionLocation()-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation(); PC->SetControlRotation(Look);
        if (Now-S.StageAt<(Run?2.2f:3.2f)) return;
        int32 Minimum=MAX_int32; for (int32 Foot=0;Foot<4;++Foot) Minimum=FMath::Min(Minimum,M.Footfalls[Foot]);
        const float Contact=PigMotionPercentile(M.FloorGaps,.95f),Pairs=PigMotionPairedFraction(M);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_GAIT name=%s mode=%s frames=%d plants=%d distance=%.1f soleP95=%.2f plantDriftMax=%.2f footfalls=[%d,%d,%d,%d] pairedFraction=%.2f"),
            *Pig->PigName,Run?TEXT("trot"):TEXT("walk"),M.Samples,M.PlantSamples,M.Distance,Contact,M.MaxPlantDrift,M.Footfalls[0],M.Footfalls[1],M.Footfalls[2],M.Footfalls[3],Pairs);
        if (M.Samples<25||M.PlantSamples<50||M.Distance<150.f||Minimum<2||Contact>5.f||M.MaxPlantDrift>8.f)
        { Finish(false,TEXT("Observed physical gait did not keep planted hoof meshes supported and stable")); return; }
        if ((!Run&&Pairs>.45f)||(Run&&Pairs<.35f)) { Finish(false,TEXT("Observed footfalls did not distinguish four-beat walking from paired trot")); return; }
        Capture(Prefix+(Run?TEXT("_trot"):TEXT("_walk")));
        if (!Run) { Advance(4); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),Pig->GetInteractionLocation())||!Pig->RequestSleep()) { Finish(false,TEXT("Gait pig cannot return to its mattress")); return; }
        Advance(5); return;
    }
    if (S.Stage==5)
    {
        if (!S.Pigs[0]->IsSleepingOnBed()||!S.Pigs[1]->IsSleepingOnBed()) { if (Now-S.StageAt>15.f) Finish(false,TEXT("Gait pig failed return to sleep")); return; }
        if (S.Index==0) { S.Index=1; Advance(1); return; }
        S.Index=0; Advance(10); return;
    }
    if (S.Stage==10)
    {
        if (!S.Entry)
        {
            if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),Pig->GetInteractionLocation()))
            { Finish(false,TEXT("Far pickup-pose observer has no clear floor")); return; }
            Pawn->FirstPersonCamera->SetRelativeLocation(FVector(0,0,Pawn->BaseEyeHeight));
            // After Pepper's real E placement, leave it on that floor. Returning
            // it to the neighboring bed immediately blocks Coal's rear ankle.
            for (int32 i=S.Index;i<S.Pigs.Num();++i) if (!S.Pigs[i]->IsSleepingOnBed()) S.Pigs[i]->RequestSleep();
            S.Entry=true; S.StageAt=Now;
        }
        // Let the previous hand pose and camera recover while the observer is
        // far away. Waiting after the close approach wakes the sleeping pig
        // and moves the very ankle that the viewpoint was chosen to reach.
        if (Now-S.StageAt<.3f) return;
        if (!Pig->IsSleepingOnBed()||(S.Index==0&&!S.Pigs[1]->IsSleepingOnBed()))
        { if (Now-S.StageAt>18.f) Finish(false,TEXT("Pickup pig failed safe physical mattress preparation")); return; }
        if (!PigMotionRearAnkleView(World,Pawn,PC,Pig)) { Finish(false,TEXT("No clear, anatomically reachable rear-ankle pickup focus")); return; }
        // Search, record the settled baseline and press actual E in one frame.
        S.InitialHand=Pawn->GetPigHandGripPoint(); S.FingerBefore=PigMotionFinger(Pawn); S.PickupRoot=Pig->GetActorLocation(); S.PickupAt=Now;
        if (!SendE(PC,IE_Pressed)) { Finish(false,TEXT("E pickup input unavailable")); return; } Advance(11); return;
    }
    if (S.Stage==11)
    {
        SendE(PC,IE_Released);
        const auto Phase=Pig->GetCarryPhase();
        const float RootMovement=FVector::Dist(S.PickupRoot,Pig->GetActorLocation());
        if (Phase==EHousePigCarryPhase::Reaching)
        {
            const float Contact=FVector::Dist(Pig->GetCarryGripPoint(),Pawn->GetPigHandGripPoint());
            S.ReachContactMin[S.Index]=FMath::Min(S.ReachContactMin[S.Index],Contact);
            if (Contact<8.f&&RootMovement<3.f&&!S.ContactBeforeLift[S.Index])
            {
                S.ContactBeforeLift[S.Index]=true;
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_CONTACT_BEFORE_LIFT name=%s handGap=%.2f bodyMovement=%.2f age=%.3f"),
                    *Pig->PigName,Contact,RootMovement,Now-S.PickupAt);
                Capture(Prefix+TEXT("_contact_before_lift"));
            }
        }
        if (RootMovement>3.f&&!S.ContactBeforeLift[S.Index])
        { Finish(false,TEXT("Pig body moved before actual rendered hand reached the rear ankle")); return; }
        if (Phase==EHousePigCarryPhase::Reaching && Now-S.PickupAt>.12f && !S.ReachObserved[S.Index])
        {
            const float RootMove=FVector::Dist(S.PickupRoot,Pig->GetActorLocation()),HandMove=FVector::Dist(S.InitialHand,Pawn->GetPigHandGripPoint());
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_REACH name=%s rootMovement=%.2f handMovement=%.2f bend=%.2f"),*Pig->PigName,RootMove,HandMove,Pig->GetPickupBend());
            if (RootMove>3.f||HandMove<1.f||Pig->GetPickupBend()<.08f) { Finish(false,TEXT("Pickup must reach/bend before lifting the pig")); return; }
            S.ReachObserved[S.Index]=true; Capture(Prefix+TEXT("_reaching"));
        }
        if (Phase==EHousePigCarryPhase::Lifting && Pig->GetCarryBlend()>.45f && Pig->GetCarryBlend()<.98f && !S.LiftObserved[S.Index])
        {
            if (FVector::Dist(S.PickupRoot,Pig->GetActorLocation())<10.f) { Finish(false,TEXT("Lifting phase has no actual visible pig motion")); return; }
            S.LiftObserved[S.Index]=true; Capture(Prefix+TEXT("_lifting"));
        }
        if (Phase!=EHousePigCarryPhase::Held) { if (Now-S.PickupAt>4.f) Finish(false,TEXT("Pickup did not reach supported held phase")); return; }
        if (!S.ReachObserved[S.Index]||!S.LiftObserved[S.Index]||!S.ContactBeforeLift[S.Index])
        { Finish(false,TEXT("Pickup skipped observable reach, rear-ankle hand contact or intermediate lift")); return; }
        S.HeldAt=Now; S.HoldRoot=Pig->GetActorLocation(); Advance(12); return;
    }
    if (S.Stage==12||S.Stage==13)
    {
        if (!Pig->IsCarried()||Pig->Carrier!=Pawn||Pig->GetVelocity().Size()>1.f||Pig->Mood!=EHousePigMood::Carried)
        { Finish(false,TEXT("Carried pig resumed AI or detached")); return; }
        if (!PigMotionBodyClear(World,Pig,Pawn)) { Finish(false,TEXT("Full hanging pig body overlaps physical house geometry or peer")); return; }
        if (Now-S.HeldAt>1.f)
        {
            const FVector Grip=Pig->GetCarryGripPoint(),Hand=Pawn->GetPigHandGripPoint(); const float Error=FVector::Dist(Grip,Hand);
            S.HandErrors[S.Index].Add(Error); S.HandErrorMax[S.Index]=FMath::Max(S.HandErrorMax[S.Index],Error); ++S.GripSamples[S.Index];
            if (S.Stage==13) S.MovingHandErrors[S.Index].Add(Error);
            UStaticMeshComponent* Rear=PigUpgradePart(Pig,TEXT("LowerLeg2")),*OtherRear=PigUpgradePart(Pig,TEXT("LowerLeg3"));
            UStaticMeshComponent* Body=PigUpgradePart(Pig,TEXT("PigBody"));
            if (!Rear||!OtherRear||!Body||FVector::Dist(Grip,Rear->GetComponentTransform().TransformPosition(FVector(0,0,-20)))>3.f
                ||FVector::Dist(Grip,OtherRear->GetComponentTransform().TransformPosition(FVector(0,0,-20)))<12.f*Pig->VisualScale
                ||Grip.Z-Body->Bounds.Origin.Z<15.f*Pig->VisualScale)
            { PigMotionLogHeldPose(World,Pig,Pawn,PC); Finish(false,TEXT("Held pose is not suspended from exactly one rear leg")); return; }
            S.FingerChange[S.Index]=FMath::Max(S.FingerChange[S.Index],FMath::RadiansToDegrees(PigMotionFinger(Pawn).AngularDistance(S.FingerBefore)));
            S.SwingMax[S.Index]=FMath::Max(S.SwingMax[S.Index],FMath::Abs(Pig->GetActorRotation().Pitch+80.f));
        }
        if (S.Stage==12)
        {
            if (Now-S.StageAt<1.6f) return;
            if (S.GripSamples[S.Index]<20||PigMotionPercentile(S.HandErrors[S.Index],.95f)>8.f||S.FingerChange[S.Index]<5.f)
            { Finish(false,TEXT("Actual rendered right-hand grip or finger closure does not hold the rear leg")); return; }
            if (!S.Entry) { Capture(Prefix+TEXT("_held_back_leg")); S.Entry=true; return; }
            if (IFileManager::Get().FileSize(*S.Capture)<=0)
            { if (Now-S.StageAt>4.6f) Finish(false,TEXT("Held pose capture did not save before camera turn")); return; }
            S.PlayerStart=Pawn->GetActorLocation();
            const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            const FRotator Face(0,(FVector(-2675,-1230,302.3)-Feet).Rotation().Yaw,0); PC->SetControlRotation(Face); Pawn->SetActorRotation(Face);
            SendPigUpgradeKey(PC,EKeys::W,IE_Pressed); S.Moving=true; Advance(13); return;
        }
        S.CarryDistance[S.Index]=FVector::Dist2D(S.PlayerStart,Pawn->GetActorLocation());
        if (S.CarryDistance[S.Index]<90.f&&Now-S.StageAt<1.1f) return;
        SendPigUpgradeKey(PC,EKeys::W,IE_Released); S.Moving=false; Pawn->GetCharacterMovement()->StopMovementImmediately();
        if (S.CarryDistance[S.Index]<50.f||S.SwingMax[S.Index]>32.f) { Finish(false,TEXT("Physical carried movement failed or hanging swing was unbounded")); return; }
        if (S.MovingHandErrors[S.Index].Num()<3||PigMotionPercentile(S.MovingHandErrors[S.Index],.95f)>8.f)
        { Finish(false,TEXT("Post-animation rendered hand lost rear-leg contact during physical carrying")); return; }
        S.PlacementsBefore=Pig->Placements;
        S.PlaceAttempt=0; S.PlaceYaw=PC->GetControlRotation().Yaw; Advance(16); return;
    }
    if (S.Stage==16)
    {
        // A peer can wander under the carried pig. Try ordinary E placement
        // toward another floor angle; every production guard remains active.
        static const float Angles[]={0.f,-45.f,45.f,-90.f,90.f,180.f};
        if (!S.Entry)
        {
            const float Yaw=S.PlaceYaw+Angles[S.PlaceAttempt];
            const FVector Feet=Pawn->GetActorLocation()-FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            const FRotator Aim=(Feet+FRotator(0,Yaw,0).Vector()*155.f-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation();
            PC->SetControlRotation(Aim); Pawn->SetActorRotation(FRotator(0,Yaw,0));
            if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_PLACE_AIM name=%s attempt=%d yaw=%.1f peerFeet=%s player=%s"),
                *Pig->PigName,S.PlaceAttempt,Yaw,*S.Pigs[1-S.Index]->FeetLocation().ToString(),*Pawn->GetActorLocation().ToString());
            S.Entry=true;
        }
        if (Now-S.StageAt<.25f) return;
        if (!SendE(PC,IE_Pressed)) { Finish(false,TEXT("E put-down input unavailable")); return; }
        S.PlaceAt=Now; Advance(14); return;
    }
    if (S.Stage==14)
    {
        SendE(PC,IE_Released);
        if (Pig->IsPlacing()&&Now-S.PlaceAt>.18f&&!S.LowerObserved[S.Index])
        {
            if (!Pig->IsCarried()||Pig->Placements!=S.PlacementsBefore) { Finish(false,TEXT("Put-down released before the lowering animation")); return; }
            S.LowerObserved[S.Index]=true; Capture(Prefix+TEXT("_lowering"));
        }
        if (Pig->IsCarried())
        {
            if (Pig->GetCarryPhase()==EHousePigCarryPhase::Held&&Now-S.PlaceAt>.12f)
            {
                if (++S.PlaceAttempt<6) { Advance(16); return; }
                Finish(false,TEXT("Six ordinary E floor angles were blocked; no collision guard was bypassed")); return;
            }
            if (Now-S.PlaceAt>3.f) Finish(false,TEXT("Accepted E put-down did not finish its safe lowering")); return;
        }
        S.LowerDuration[S.Index]=Now-S.PlaceAt; FHitResult Floor;
        if (!S.LowerObserved[S.Index]||S.LowerDuration[S.Index]<.55f||Pig->Placements!=S.PlacementsBefore+1||!PigUpgradeFloor(World,Pig,Pawn,Floor)||!Pig->IsInsidePigsty())
        { Finish(false,TEXT("Animated put-down did not end on supported pigsty floor")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_MOTION_CARRY name=%s handP95=%.2f handMax=%.2f fingerDegrees=%.1f carriedDistance=%.1f lowerSeconds=%.2f oneRearLeg=1"),
            *Pig->PigName,PigMotionPercentile(S.HandErrors[S.Index],.95f),S.HandErrorMax[S.Index],S.FingerChange[S.Index],S.CarryDistance[S.Index],S.LowerDuration[S.Index]);
        Capture(Prefix+TEXT("_placed"));
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),Pig->GetInteractionLocation()))
        { Finish(false,TEXT("Observer cannot leave safely placed pig")); return; }
        if (S.Index==1&&!Pig->RequestSleep()) { Finish(false,TEXT("Placed larger pig cannot return to sleep")); return; }
        if (S.Index==0) { S.Index=1; Advance(10); return; }
        Advance(20); return;
    }
    if (S.Stage==20)
    {
        if (!S.Entry)
        {
            FHousePigRoamNode N; N.Feet=FVector(-2415,-1245,302.3); N.bInsidePigsty=true; N.Links.Add(6);
            S.GateNode=Pig->RoamGraph.Add(N); Pig->RoamGraph[6].Links.AddUnique(S.GateNode);
            PlacePlayer(World,Pawn,PC,FVector(-2177,-1245,302.3),Pig->GetInteractionLocation());
            if (!Pig->RequestRoamNode(S.GateNode)) { Finish(false,TEXT("Closed-door approach cannot be reached physically")); return; } S.Entry=true;
        }
        if (FVector::Dist2D(Pig->FeetLocation(),Pig->RoamGraph[S.GateNode].Feet)>24.f||Pig->GetVelocity().Size2D()>5.f)
        { if (Now-S.StageAt>12.f) Finish(false,TEXT("Closed-door approach failed")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2280,-1245,302.3),Pig->GetInteractionLocation())) { Finish(false,TEXT("Closed-door rejection observer blocked")); return; }
        FVector View; FRotator R; PC->GetPlayerViewPoint(View,R); FCollisionQueryParams Q(SCENE_QUERY_STAT(PigMotionClosedLeaf),false,Pig); Q.AddIgnoredActor(Pawn); FHitResult H;
        if (FVector::Dist(View,Pig->GetInteractionLocation())>220.f||!World->LineTraceSingleByChannel(H,View,Pig->GetInteractionLocation(),ECC_Pawn,Q)
            ||S.Door->IsOpen()||Pig->CanPickup(Pawn)||Pig->TryPickup(Pawn)||Pawn->FocusedHousePigForInteraction())
        { Finish(false,TEXT("In-range closed physical door does not reject pig pickup")); return; }
        for (const auto& Ref:S.Pigs)
            if (!Ref->SnackPile||Ref->SnackPile->FoodRemaining<0||Ref->SnackPile->FoodRemaining>24||Ref->SnackPile->Cheetos->GetInstanceCount()!=Ref->SnackPile->FoodRemaining)
            { Finish(false,TEXT("Existing bounded visible Cheetos state was damaged by motion upgrade")); return; }
        for (const FString& Path:S.Captures) if (IFileManager::Get().FileSize(*Path)<=0) { Finish(false,TEXT("Motion capture was not saved")); return; }
        Finish(true,GripOnly?TEXT("GRIP-ONLY CHECK: gait was skipped; both sizes used actual E reach, intermediate lift, one-back-leg rendered hand/finger contact, physical clear carrying and async supported placement; closed door rejected in-range pickup; mattress and visible food preserved"):
            TEXT("Both sizes physically walked with four distinct grounded footfalls and trotted with paired timing; actual E reached before lifting, suspended each pig by one back leg with rendered hand/finger contact, carried without body penetration, then lowered asynchronously onto supported floor; closed door rejected in-range pickup; mattress and bounded visible food preserved"));
    }
}

struct FPigAnimalMetrics
{
    FPigMotionMetrics Hooves;
    bool WasSniffing=false,AwaitResume=false,EarsInitialized=false,RollInitialized=false,SteadyPause=false;
    float SniffAt=0,EndedAt=0,PreviousEnd=-1,RollMin=BIG_NUMBER,RollMax=-BIG_NUMBER,LastRoll=0,LastRollCrossing=-1;
    FVector PauseFeet,SniffStartFeet,ResumeFeet,LastFeet,SnoutLocal;
    FQuat EarPrevious[2]={FQuat::Identity,FQuat::Identity};
    int32 Completed=0,Resumed=0,PauseSamples=0,WalkFrames=0,RunFrames=0,EarIndependentFrames=0,AudioFrames=0,SnortsBefore=0;
    float WalkDistance=0,RunDistance=0,EarTravel[2]={0,0},EarMax=0,PauseTravelMax=0,StoppingTravelMax=0,SnoutGapMin=BIG_NUMBER;
    TArray<float> Durations,Intervals,Starts,SnoutGaps,PauseSpeeds,RollPeriods,ResumeDistances;
    bool SniffCapture=false,RunCapture=false;
};
struct FPigAnimalSmoke
{
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs;
    TWeakObjectPtr<ACameraActor> Camera;
    FPigAnimalMetrics Metrics[2];
    TArray<int32> SeedNodes[2];
    bool NaturalValidated=false,DirectedRunCoverage=false;
    int32 RunRequests[2]={0,0},NaturalSniffs[2]={0,0},NaturalResumes[2]={0,0};
    float AutonomousObservationSeconds=0,LastRunRequest[2]={-1000.f,-1000.f};
    int32 Stage=0,Focus=0;
    float Began=0,ObserveAt=0,FailureAt=0;
    double WallBegan=0;
    FString Failure,Capture;
    TArray<FString> Captures;
};
bool PigAnimalRepeatedSniffSynchronization(const TArray<float>& A,const TArray<float>& B,int32& Samples,int32& Unpaired)
{
    Samples=Unpaired=0;
    // The initial directed journey can share a deadline. Compare every later
    // start symmetrically; one chance coincidence is not repeated synchrony.
    for (int32 Pig=0;Pig<2;++Pig)
    {
        const TArray<float>& Starts=Pig?B:A; const TArray<float>& Other=Pig?A:B;
        for (int32 i=1;i<Starts.Num();++i)
        {
            float Nearest=BIG_NUMBER;
            for (int32 j=1;j<Other.Num();++j) Nearest=FMath::Min(Nearest,FMath::Abs(Starts[i]-Other[j]));
            ++Samples; if (Nearest>.25f) ++Unpaired;
        }
    }
    return A.Num()>=3&&B.Num()>=3&&Unpaired==0;
}
FVector PigAnimalSnoutLocal(UStaticMeshComponent* Head)
{
    UStaticMesh* Mesh=Head?Head->GetStaticMesh():nullptr;
    if (!Mesh||!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.IsEmpty()) return FVector::ZeroVector;
    const FPositionVertexBuffer& Vertices=Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    float Front=-BIG_NUMBER;
    for (uint32 i=0;i<Vertices.GetNumVertices();++i) Front=FMath::Max(Front,Vertices.VertexPosition(i).X);
    FVector Sum=FVector::ZeroVector; int32 Count=0;
    for (uint32 i=0;i<Vertices.GetNumVertices();++i)
    {
        const FVector V(Vertices.VertexPosition(i));
        if (V.X>Front-2.f&&FMath::Abs(V.Y)<10.f) { Sum+=V; ++Count; }
    }
    return Count?Sum/Count:FVector::ZeroVector;
}
void PigAnimalCamera(UWorld* World,APlayerController* PC,ACameraActor* Camera,AHouseRoamingPig* Pig)
{
    const FVector Target=Pig->FeetLocation()+FVector(0,0,35.f*Pig->VisualScale);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PigAnimalCamera),false,Pig);
    for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
    for (float Angle:{-55.f,55.f,-100.f,100.f,180.f})
    {
        const FVector Position=Pig->FeetLocation()+FRotator(0,Pig->GetActorRotation().Yaw+Angle,0).Vector()*180.f+FVector(0,0,105);
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit,Position,Target,ECC_Visibility,Query)) continue;
        Camera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());
        if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(0.f);
        return;
    }
}
void TickHousePigAnimalSmoke(UWorld* World)
{
    if (!World||UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    static FPigAnimalSmoke S;
    if (S.World.Get()!=World) { S=FPigAnimalSmoke(); S.World=World; }
    if (S.Stage<0) return;
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn||!PC->PlayerInput||!World->GetGameViewport()) return;
    const float Now=World->GetTimeSeconds(),Delta=World->GetDeltaSeconds();
    if (!S.WallBegan) { S.WallBegan=FPlatformTime::Seconds(); S.Began=Now; }
    auto Capture=[&](const FString& Name)
    {
        S.Capture=FPaths::ProjectSavedDir()/TEXT("HousePigAnimalSmoke")/(Name+TEXT(".png"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.Capture),true); IFileManager::Get().Delete(*S.Capture,false,true);
        FScreenshotRequest::RequestScreenshot(S.Capture,true,false); S.Captures.Add(S.Capture);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_CAPTURE path=%s"),*S.Capture);
    };
    auto Finish=[&](bool Pass,const FString& Detail)
    {
        if (!Pass&&S.Stage!=90)
        { S.Failure=Detail; S.FailureAt=Now; Capture(TEXT("failed_pose")); S.Stage=90; return; }
        TSharedPtr<FJsonObject> R=MakeShared<FJsonObject>(); R->SetBoolField(TEXT("passed"),Pass);
        R->SetStringField(TEXT("detail"),Detail); R->SetNumberField(TEXT("worldSeconds"),Now-S.ObserveAt);
        R->SetBoolField(TEXT("soundEnabled"),!FParse::Param(FCommandLine::Get(),TEXT("nosound")));
        R->SetBoolField(TEXT("directedRunCoverage"),S.DirectedRunCoverage);
        R->SetNumberField(TEXT("autonomousObservationSeconds"),S.AutonomousObservationSeconds);
        TArray<TSharedPtr<FJsonValue>> PigJson;
        for (int32 i=0;i<S.Pigs.Num();++i)
        {
            const FPigAnimalMetrics& M=S.Metrics[i]; AHouseRoamingPig* Pig=S.Pigs[i].Get();
            TSharedPtr<FJsonObject> J=MakeShared<FJsonObject>(); J->SetStringField(TEXT("name"),Pig->PigName);
            J->SetNumberField(TEXT("observedSniffCompletes"),M.Completed); J->SetNumberField(TEXT("observedMovementResumes"),M.Resumed);
            J->SetNumberField(TEXT("productionSniffStarts"),Pig->SniffStarts); J->SetNumberField(TEXT("productionSniffCompletes"),Pig->SniffCompletes);
            J->SetNumberField(TEXT("directedRunRequests"),S.RunRequests[i]);
            J->SetNumberField(TEXT("autonomousSniffCompletesBeforeRunCoverage"),S.NaturalSniffs[i]);
            J->SetNumberField(TEXT("autonomousResumesBeforeRunCoverage"),S.NaturalResumes[i]);
            J->SetNumberField(TEXT("snoutFloorGapP95"),PigMotionPercentile(M.SnoutGaps,.95f)); J->SetNumberField(TEXT("pauseSpeedP95"),PigMotionPercentile(M.PauseSpeeds,.95f));
            J->SetNumberField(TEXT("pauseTravelMax"),M.PauseTravelMax); J->SetNumberField(TEXT("brakingTravelMax"),M.StoppingTravelMax);
            J->SetNumberField(TEXT("snoutFloorGapMin"),M.SnoutGapMin); J->SetNumberField(TEXT("walkDistance"),M.WalkDistance); J->SetNumberField(TEXT("runDistance"),M.RunDistance);
            J->SetNumberField(TEXT("torsoRollMin"),M.RollMin); J->SetNumberField(TEXT("torsoRollMax"),M.RollMax);
            J->SetNumberField(TEXT("leftEarTravelDegrees"),M.EarTravel[0]); J->SetNumberField(TEXT("rightEarTravelDegrees"),M.EarTravel[1]);
            J->SetNumberField(TEXT("earAngularMax"),M.EarMax); J->SetNumberField(TEXT("earIndependentFrames"),M.EarIndependentFrames); J->SetNumberField(TEXT("runFrames"),M.RunFrames);
            J->SetNumberField(TEXT("snortTriggers"),Pig->SnortEvents-M.SnortsBefore); J->SetNumberField(TEXT("audioPlayingFrames"),M.AudioFrames);
            J->SetNumberField(TEXT("authoredSnortDuration"),Pig->SnortSound?Pig->SnortSound->GetDuration():0.f);
            J->SetNumberField(TEXT("hoofFloorGapP95"),PigMotionPercentile(M.Hooves.FloorGaps,.95f)); J->SetNumberField(TEXT("plantedHoofDriftMax"),M.Hooves.MaxPlantDrift);
            auto Array=[&](const TCHAR* Key,const TArray<float>& Values) { TArray<TSharedPtr<FJsonValue>> A; for (float V:Values) A.Add(MakeShared<FJsonValueNumber>(V)); J->SetArrayField(Key,A); };
            Array(TEXT("sniffStartedAt"),M.Starts); Array(TEXT("sniffDurations"),M.Durations); Array(TEXT("betweenSniffSeconds"),M.Intervals);
            Array(TEXT("resumeDistances"),M.ResumeDistances); Array(TEXT("torsoRollPeriods"),M.RollPeriods);
            PigJson.Add(MakeShared<FJsonValueObject>(J));
        }
        R->SetArrayField(TEXT("pigs"),PigJson); TArray<TSharedPtr<FJsonValue>> C;
        for (const FString& Path:S.Captures) C.Add(MakeShared<FJsonValueString>(Path)); R->SetArrayField(TEXT("captures"),C);
        FString Json; FJsonSerializer::Serialize(R.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        const FString Path=FPaths::ProjectSavedDir()/TEXT("HousePigAnimalSmoke/result.json");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true); FFileHelper::SaveStringToFile(Json,*Path);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_SMOKE_RESULT passed=%d seconds=%.2f detail=%s report=%s"),Pass?1:0,Now-S.ObserveAt,*Detail,*Path);
        S.Stage=-1; FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    if (S.Stage==90) { if (IFileManager::Get().FileSize(*S.Capture)>0||Now-S.FailureAt>3.f) Finish(false,S.Failure); return; }
    if (FPlatformTime::Seconds()-S.WallBegan>180.) { Finish(false,TEXT("Bounded autonomous animal observation timed out")); return; }
    if (S.Stage==0)
    {
        if (Now-S.Began<2.f) return;
        if (FParse::Param(FCommandLine::Get(),TEXT("nosound"))) { Finish(false,TEXT("Animal check requires audio enabled to observe the actual snort component")); return; }
        Scalability::FQualityLevels Q; Q.SetFromSingleQualityLevel(1); Scalability::SetQualityLevels(Q);
        if (GEngine) { GEngine->Exec(World,TEXT("r.SetRes 1280x720w")); GEngine->Exec(World,TEXT("r.ScreenPercentage 100")); GEngine->Exec(World,TEXT("t.MaxFPS 60")); GEngine->Exec(World,TEXT("DisableAllScreenMessages")); }
        for (TActorIterator<AHouseRoamingPig> It(World);It;++It) S.Pigs.Add(*It);
        S.Pigs.Sort([](const TWeakObjectPtr<AHouseRoamingPig>& A,const TWeakObjectPtr<AHouseRoamingPig>& B) { return A->VisualScale<B->VisualScale; });
        if (S.Pigs.Num()!=2||!S.Pigs[0]->HasAuthority()||!S.Pigs[1]->HasAuthority()) { Finish(false,TEXT("Animal check needs the two actual authority pigs")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2910,-1310,302.3),FVector(-2680,-1110,350),TEXT("animal_far")))
        { Finish(false,TEXT("Far physical observer has no clear floor")); return; }
        FActorSpawnParameters Spawn; Spawn.ObjectFlags|=RF_Transient;
        S.Camera=World->SpawnActor<ACameraActor>(Spawn);
        if (!S.Camera.IsValid()) { Finish(false,TEXT("Transient animal camera could not be created")); return; }
        S.Camera->GetCameraComponent()->SetFieldOfView(75.f); PC->SetViewTarget(S.Camera.Get());
        const TArray<FVector> Points={FVector(-2810,-1215,302.3),FVector(-2440,-1215,302.3),FVector(-2500,-1050,302.3),FVector(-2690,-1060,302.3)};
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); FPigAnimalMetrics& M=S.Metrics[i];
            UStaticMeshComponent* Head=PigUpgradePart(Pig,TEXT("PigHead"));
            if (!Head||!PigUpgradePart(Pig,TEXT("PigLeftEar"))||!PigUpgradePart(Pig,TEXT("PigRightEar"))||!Pig->SnortSound||Pig->SnortSound->GetDuration()<.1f)
            { Finish(false,TEXT("Animal head, independently authored ears or playable recorded snort are missing")); return; }
            M.SnoutLocal=PigAnimalSnoutLocal(Head); M.SnortsBefore=Pig->SnortEvents; M.LastFeet=Pig->FeetLocation();
            TArray<int32> Nodes;
            for (const FVector& P:Points) { FHousePigRoamNode N; N.Feet=P; N.bInsidePigsty=true; Nodes.Add(Pig->RoamGraph.Add(N)); }
            for (int32 p=0;p<4;++p) { const int32 A=Nodes[p],B=Nodes[(p+1)%4]; Pig->RoamGraph[A].Links.AddUnique(B); Pig->RoamGraph[B].Links.AddUnique(A); }
            for (const TPair<int32,int32>& Link:{TPair<int32,int32>(Nodes[0],5),TPair<int32,int32>(Nodes[2],2),TPair<int32,int32>(Nodes[3],1)})
            { Pig->RoamGraph[Link.Key].Links.AddUnique(Link.Value); Pig->RoamGraph[Link.Value].Links.AddUnique(Link.Key); }
            S.SeedNodes[i]=Nodes;
            if (!Pig->RequestRoamNode(Nodes[i?0:1])) { Finish(false,TEXT("Initial ordinary animal route was not physically reachable")); return; }
        }
        S.ObserveAt=Now; S.Stage=1;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_BEGIN authority=1 autonomous=1 observerFar=1 soundEnabled=1")); return;
    }
    bool Ready=true,NaturalReady=true,RunCovered[2]={false,false};
    for (int32 i=0;i<2;++i)
    {
        AHouseRoamingPig* Pig=S.Pigs[i].Get(); FPigAnimalMetrics& M=S.Metrics[i];
        const FVector Feet=Pig->FeetLocation(); const float Speed=Pig->GetVelocity().Size2D(),Travel=FVector::Dist2D(Feet,M.LastFeet); M.LastFeet=Feet;
        const bool Sniff=Pig->IsGroundSniffing();
        if (Sniff&&!M.WasSniffing)
        {
            M.SniffAt=Now; M.SniffStartFeet=Feet; M.SteadyPause=false; M.Starts.Add(Now-S.ObserveAt);
            if (M.PreviousEnd>=0.f) M.Intervals.Add(Now-M.PreviousEnd);
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_SNIFF_START name=%s at=%.2f feet=%s"),*Pig->PigName,Now-S.ObserveAt,*Feet.ToString());
        }
        if (Sniff&&Now-M.SniffAt>.9f&&Pig->GetGroundSniffBlend()>.7f)
        {
            if (!M.SteadyPause) { M.PauseFeet=Feet; M.SteadyPause=true; M.StoppingTravelMax=FMath::Max(M.StoppingTravelMax,FVector::Dist2D(Feet,M.SniffStartFeet)); }
            M.PauseSpeeds.Add(Speed); ++M.PauseSamples; M.PauseTravelMax=FMath::Max(M.PauseTravelMax,FVector::Dist2D(Feet,M.PauseFeet));
            UStaticMeshComponent* Head=PigUpgradePart(Pig,TEXT("PigHead")); const FVector Snout=Head->GetComponentTransform().TransformPosition(M.SnoutLocal);
            FCollisionQueryParams Q(SCENE_QUERY_STAT(PigAnimalSnoutFloor),false,Pig); for (TActorIterator<APawn> It(World);It;++It) Q.AddIgnoredActor(*It);
            FHitResult Floor;
            if (World->LineTraceSingleByChannel(Floor,Snout+FVector(0,0,45),Snout-FVector(0,0,65),ECC_Visibility,Q)&&Floor.ImpactNormal.Z>.65f)
            {
                const float Gap=Snout.Z-Floor.ImpactPoint.Z; M.SnoutGaps.Add(Gap); M.SnoutGapMin=FMath::Min(M.SnoutGapMin,Gap);
            }
            // Unreal has one screenshot slot per frame. Let the other pig
            // capture on the next frame instead of replacing a queued image.
            if (!M.SniffCapture&&M.SnoutGaps.Num()>3&&M.SnoutGaps.Last()<18.f&&!FScreenshotRequest::IsScreenshotRequested())
            { S.Focus=i; PigAnimalCamera(World,PC,S.Camera.Get(),Pig); Capture(Pig->PigName.ToLower()+TEXT("_floor_sniff")); M.SniffCapture=true; }
        }
        if (!Sniff&&M.WasSniffing)
        {
            const float Duration=Now-M.SniffAt;
            if (Pig->LastSniffCompletedAt>=M.SniffAt-.2f)
            {
                ++M.Completed; M.Durations.Add(Duration); M.EndedAt=Now; M.PreviousEnd=Now; M.ResumeFeet=Feet; M.AwaitResume=true;
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_SNIFF_END name=%s duration=%.2f completed=%d"),*Pig->PigName,Duration,M.Completed);
            }
        }
        M.WasSniffing=Sniff;
        if (M.AwaitResume&&!Sniff&&Speed>40.f&&FVector::Dist2D(Feet,M.ResumeFeet)>35.f)
        { ++M.Resumed; M.ResumeDistances.Add(FVector::Dist2D(Feet,M.ResumeFeet)); M.AwaitResume=false; }
        if (Pig->IsSnortAudioPlaying()) ++M.AudioFrames;
        const bool Walking=Pig->Mood==EHousePigMood::Walking&&Speed>45.f&&!Sniff;
        const bool Running=Pig->Mood==EHousePigMood::Running&&Speed>80.f&&!Sniff;
        if (Walking||Running) PigMotionObserveGait(World,Pig,Pawn,M.Hooves,Now,Delta);
        else { M.Hooves.LastFeet=Feet; for (int32 Leg=0;Leg<4;++Leg) { M.Hooves.Planted[Leg]=Pig->IsHoofPlanted(Leg); M.Hooves.PlantAge[Leg]=0; M.Hooves.PlantStart[Leg]=PigMotionSole(PigUpgradePart(Pig,*FString::Printf(TEXT("LowerLeg%d"),Leg))); } }
        TArray<USceneComponent*> Parts; Pig->GetComponents(Parts); USceneComponent* Root=nullptr;
        for (USceneComponent* Part:Parts) if (Part->GetFName()==TEXT("PigVisualRoot")) Root=Part;
        if (Walking&&Root)
        {
            ++M.WalkFrames; M.WalkDistance+=Travel; const float Roll=Root->GetRelativeRotation().Roll;
            M.RollMin=FMath::Min(M.RollMin,Roll); M.RollMax=FMath::Max(M.RollMax,Roll);
            if (M.RollInitialized&&M.LastRoll<0.f&&Roll>=0.f)
            { if (M.LastRollCrossing>=0.f) M.RollPeriods.Add(Now-M.LastRollCrossing); M.LastRollCrossing=Now; }
            M.LastRoll=Roll; M.RollInitialized=true;
        }
        else M.RollInitialized=false;
        float EarDelta[2]={0,0};
        for (int32 Ear=0;Ear<2;++Ear)
        {
            UStaticMeshComponent* Part=PigUpgradePart(Pig,Ear?TEXT("PigRightEar"):TEXT("PigLeftEar"));
            const FQuat Rotation=Part->GetRelativeRotation().Quaternion();
            if (Running&&M.EarsInitialized)
            { EarDelta[Ear]=FMath::RadiansToDegrees(Rotation.AngularDistance(M.EarPrevious[Ear])); M.EarTravel[Ear]+=EarDelta[Ear]; M.EarMax=FMath::Max(M.EarMax,FMath::RadiansToDegrees(Rotation.AngularDistance(FQuat::Identity))); }
            M.EarPrevious[Ear]=Rotation;
        }
        M.EarsInitialized=true;
        if (Running)
        {
            ++M.RunFrames; M.RunDistance+=Travel; if (FMath::Abs(EarDelta[0]-EarDelta[1])>.04f) ++M.EarIndependentFrames;
            if (!M.RunCapture&&M.RunFrames>30&&!FScreenshotRequest::IsScreenshotRequested()) { S.Focus=i; PigAnimalCamera(World,PC,S.Camera.Get(),Pig); Capture(Pig->PigName.ToLower()+TEXT("_running_ears")); M.RunCapture=true; }
        }
        const float HoofGap=PigMotionPercentile(M.Hooves.FloorGaps,.95f);
        const bool DurationsOkay=M.Durations.Num()>=2&&M.Durations[0]>.8f&&M.Durations[0]<5.f&&M.Durations[1]>.8f&&M.Durations[1]<5.f;
        const bool Natural=M.Completed>=2&&M.Resumed>=2&&DurationsOkay&&M.PauseSamples>=30&&PigMotionPercentile(M.PauseSpeeds,.95f)<15.f
            &&M.PauseTravelMax<40.f&&M.SnoutGaps.Num()>=30&&PigMotionPercentile(M.SnoutGaps,.95f)<18.f&&M.SnoutGapMin>-4.f
            &&M.WalkFrames>=30&&M.WalkDistance>80.f&&M.RollMax-M.RollMin>.6f&&M.RollMin>-10.f&&M.RollMax<10.f
            &&Pig->SnortEvents-M.SnortsBefore>=2&&M.AudioFrames>0&&M.Hooves.PlantSamples>=50&&HoofGap<5.f&&M.Hooves.MaxPlantDrift<8.f&&M.SniffCapture;
        RunCovered[i]=M.RunFrames>=30&&M.RunDistance>80.f&&M.EarTravel[0]>18.f&&M.EarTravel[1]>18.f&&M.EarMax<45.f&&M.EarIndependentFrames>=10&&M.RunCapture;
        NaturalReady &= Natural; Ready &= Natural&&RunCovered[i];
    }
    PigAnimalCamera(World,PC,S.Camera.Get(),S.Pigs[S.Focus].Get());
    const float Elapsed=Now-S.ObserveAt;
    if (Elapsed>=45.f&&NaturalReady&&!S.NaturalValidated)
    {
        S.NaturalValidated=true; S.AutonomousObservationSeconds=Elapsed;
        for (int32 i=0;i<2;++i) { S.NaturalSniffs[i]=S.Metrics[i].Completed; S.NaturalResumes[i]=S.Metrics[i].Resumed; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_AUTONOMOUS_COMPLETE seconds=%.2f sniffs=[%d,%d] resumes=[%d,%d]"),
            Elapsed,S.NaturalSniffs[0],S.NaturalSniffs[1],S.NaturalResumes[0],S.NaturalResumes[1]);
    }
    if (S.NaturalValidated&&!Ready)
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get();
            if (RunCovered[i]||Pig->IsGroundSniffing()||Pig->Mood==EHousePigMood::Running||S.RunRequests[i]>=12||Now-S.LastRunRequest[i]<2.5f) continue;
            int32 Goal=INDEX_NONE; float Distance=-1.f;
            for (int32 Node:S.SeedNodes[i])
            {
                const float D=FVector::DistSquared2D(Pig->FeetLocation(),Pig->RoamGraph[Node].Feet);
                if (D>Distance) { Distance=D; Goal=Node; }
            }
            S.DirectedRunCoverage=true; S.LastRunRequest[i]=Now; ++S.RunRequests[i];
            const bool Accepted=Pig->RequestRoamNode(Goal);
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_DIRECTED_RUN name=%s request=%d accepted=%d personalityMood=%d goal=%d feet=%s"),
                *Pig->PigName,S.RunRequests[i],Accepted?1:0,int32(Pig->Mood),Goal,*Pig->FeetLocation().ToString());
        }
    if (Elapsed>=45.f&&Ready)
    {
        int32 TimingSamples=0,Unpaired=0;
        const bool RepeatedSync=PigAnimalRepeatedSniffSynchronization(S.Metrics[0].Starts,S.Metrics[1].Starts,TimingSamples,Unpaired);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_SNIFF_TIMING laterSamples=%d unpaired=%d repeatedSynchronization=%d tolerance=.25 initialExcluded=1"),TimingSamples,Unpaired,RepeatedSync?1:0);
        if (RepeatedSync) { Finish(false,TEXT("Every later sniff start remained synchronized across repeated bouts in both pigs")); return; }
        for (const FString& Path:S.Captures) if (IFileManager::Get().FileSize(*Path)<=0) return;
        Finish(true,S.DirectedRunCoverage?
            TEXT("Autonomous authority AI independently sniffed the actual floor, resumed travel, walked with bounded torso sway and played recorded snorts; a separate ordinary route-command phase covered personality-selected physical running and independent ear flop; actual planted hoof support and observer separation preserved"):
            TEXT("Normal authority AI independently paused twice to sniff the actual floor and resumed physical travel; recorded spatial snorts played; both running ears rotated independently; walking torso roll was nonzero and bounded; actual planted hoof meshes retained floor support; observer stayed outside flee range")); return;
    }
    if ((!S.NaturalValidated&&Elapsed>90.f)||(S.NaturalValidated&&Elapsed-S.AutonomousObservationSeconds>32.f))
    {
        for (int32 i=0;i<2;++i)
        {
            const FPigAnimalMetrics& M=S.Metrics[i]; AHouseRoamingPig* Pig=S.Pigs[i].Get();
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_ANIMAL_INCOMPLETE name=%s sniffs=%d resumes=%d pauseFrames=%d pauseP95=%.2f snoutP95=%.2f walkFrames=%d walkDistance=%.1f roll=[%.2f,%.2f] runFrames=%d runDistance=%.1f earTravel=[%.1f,%.1f] independentFrames=%d snorts=%d audioFrames=%d hoofP95=%.2f hoofDrift=%.2f nextSniffAt=%.2f %s"),
                *Pig->PigName,M.Completed,M.Resumed,M.PauseSamples,PigMotionPercentile(M.PauseSpeeds,.95f),PigMotionPercentile(M.SnoutGaps,.95f),M.WalkFrames,M.WalkDistance,M.RollMin,M.RollMax,
                M.RunFrames,M.RunDistance,M.EarTravel[0],M.EarTravel[1],M.EarIndependentFrames,Pig->SnortEvents-M.SnortsBefore,M.AudioFrames,PigMotionPercentile(M.Hooves.FloorGaps,.95f),M.Hooves.MaxPlantDrift,Pig->GetNextGroundSniffAt(),*Pig->MovementDebug());
        }
        Finish(false,S.NaturalValidated?TEXT("Bounded ordinary route requests did not cover physical running and ear metrics after natural AI observation"):
            TEXT("Natural 90-second observation did not satisfy the recorded animal-motion metrics"));
    }
}

// Current-room resize coverage uses the installed pigs' room bounds and graph.
// The older upgrade/gait harnesses retain their historical upstairs test course.
void TickHousePigResizeSmoke(UWorld* World)
{
    if (!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    struct FResizeState
    {
        TWeakObjectPtr<UWorld> World;
        TArray<TWeakObjectPtr<AHouseRoamingPig>> Pigs;
        TWeakObjectPtr<AExistingHouseDoor> Door;
        FTransform PlayerStart;
        int32 Stage=0, Escapes[2]={0,0};
        bool Escaped[2]={false,false};
        float Travel[2]={0,0}, AirSeconds[2]={0,0};
        double Began=0, StageAt=0;
    };
    static FResizeState S;
    if (S.World.Get()!=World) { S=FResizeState(); S.World=World; }
    if (S.Stage<0) return;
    const double Now=FPlatformTime::Seconds();
    if (!S.Began) S.Began=Now;
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn || !PC) return;
    auto Finish=[&](bool Pass,const TCHAR* Detail)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_RESIZE_SMOKE_RESULT passed=%d scale=%.3f detail=%s"),
            Pass?1:0,HouseWorldScale::Factor(World),Detail);
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (const AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_RESIZE_PIG name=%s actorScale=%s visualScale=%.3f capsule=[%.3f,%.3f] feet=%s inside=%d escapes=%d hall=%d travel=%.1f %s"),
                    *Pig->PigName,*Pig->GetActorScale3D().ToString(),Pig->VisualScale,
                    Pig->GetCapsuleComponent()->GetScaledCapsuleRadius(),Pig->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
                    *Pig->FeetLocation().ToString(),Pig->IsInsidePigsty()?1:0,Pig->Escapes,Pig->HallNodesVisited,Pig->TravelDistance,*Pig->MovementDebug());
        S.Stage=-1; FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    if (Now-S.Began>65.) { Finish(false,TEXT("Resized pig gate and physical escape check timed out")); return; }
    if (S.Stage==0)
    {
        if (Now-S.Began<3.) return;
        if (AGameModeBase* Mode=World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
        for (TActorIterator<AHouseRoamingPig> It(World);It;++It) S.Pigs.Add(*It);
        if (S.Pigs.Num()!=2) { Finish(false,TEXT("Expected two installed pigs")); return; }
        S.Pigs.Sort([](const TWeakObjectPtr<AHouseRoamingPig>& A,const TWeakObjectPtr<AHouseRoamingPig>& B) { return A->VisualScale<B->VisualScale; });
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It)
            if (It->DoorId==S.Pigs[0]->PigstyDoorId) { S.Door=*It; break; }
        if (!S.Door.IsValid() || S.Door->bLocked || S.Door->IsOpen() || S.Pigs[1]->PigstyDoorId!=S.Pigs[0]->PigstyDoorId)
        { Finish(false,TEXT("Installed shared pigsty door must start closed and unlocked")); return; }
        S.PlayerStart=Pawn->GetActorTransform();
        const float ExpectedVisual[]={.62f,.82f};
        for (int32 I=0;I<2;++I)
        {
            AHouseRoamingPig* Pig=S.Pigs[I].Get();
            if (!Pig->IsInsidePigsty() || !Pig->GetActorScale3D().Equals(FVector::OneVector,.001f)
                || !FMath::IsNearlyEqual(Pig->VisualScale,ExpectedVisual[I],.001f)
                || !FMath::IsNearlyEqual(Pig->GetCapsuleComponent()->GetScaledCapsuleRadius(),30.f*ExpectedVisual[I],.02f)
                || !FMath::IsNearlyEqual(Pig->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),48.f*ExpectedVisual[I],.02f))
            { Finish(false,TEXT("Pig spawn or unchanged visual/capsule size failed")); return; }
            int32 Outside=INDEX_NONE;
            for (int32 N=0;N<Pig->RoamGraph.Num();++N)
                if (!Pig->RoamGraph[N].bInsidePigsty && Pig->RoamGraph[N].GateDoorId.IsNone()) { Outside=N; break; }
            if (Outside==INDEX_NONE || Pig->RequestRoamNode(Outside))
            { Finish(false,TEXT("Closed installed pigsty gate permits an escape route")); return; }
            S.Escapes[I]=Pig->Escapes; S.Travel[I]=Pig->TravelDistance;
            if (!Pig->RequestFeeding()) { Finish(false,TEXT("Installed snack pile has no physical feeding approach")); return; }
        }
        S.Stage=1; S.StageAt=Now;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_RESIZE_CLOSED_GATE sizesUnchanged=1 gate=%s"),*S.Door->DoorId.ToString());
        return;
    }
    for (int32 I=0;I<S.Pigs.Num();++I)
    {
        AHouseRoamingPig* Pig=S.Pigs[I].Get();
        if (!Pig) { Finish(false,TEXT("Installed pig vanished")); return; }
        FHitResult Floor;
        const bool Supported=PigUpgradeFloor(World,Pig,Pawn,Floor);
        S.AirSeconds[I]=Supported?0.f:S.AirSeconds[I]+World->GetDeltaSeconds();
        if (S.AirSeconds[I]>.7f) { Finish(false,TEXT("Pig lost actual floor support after resize")); return; }
    }
    if (S.Stage==1)
    {
        if (S.Door->IsOpen() || !S.Pigs[0]->IsInsidePigsty() || !S.Pigs[1]->IsInsidePigsty())
        { Finish(false,TEXT("Pig crossed a closed resized room gate")); return; }
        if (Now-S.StageAt<5.) return;
        bool Placed=false;
        for (const FHousePigRoamNode& Node:S.Pigs[0]->RoamGraph)
        {
            const float Distance=FVector::Dist2D(Node.Feet,S.Door->DoorwayCenter);
            if (Node.bInsidePigsty || Distance<65.f || Distance>220.f) continue;
            if (PlacePlayer(World,Pawn,PC,Node.Feet,S.Door->DoorwayCenter,TEXT("resized_pig_gate"))) { Placed=true; break; }
        }
        if (!Placed || !S.Door->Interact(Pawn)) { Finish(false,TEXT("Player cannot reach and natively open resized pigsty gate")); return; }
        S.Stage=2; S.StageAt=Now; return;
    }
    if (S.Stage==2)
    {
        if (Now-S.StageAt<1.3) return;
        if (!S.Door->IsOpen() || S.Door->IsActorTickEnabled()) { Finish(false,TEXT("Resized gate did not finish opening")); return; }
        // Restore the player to their real initial spawn so they cannot block or
        // provoke the pigs while the two ordinary AI route requests run.
        Pawn->SetActorTransform(S.PlayerStart,false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        for (int32 I=0;I<2;++I)
        {
            AHouseRoamingPig* Pig=S.Pigs[I].Get();
            int32 Goal=INDEX_NONE; float Best=BIG_NUMBER;
            const float DesiredDistance=I==0?650.f:350.f;
            for (int32 N=0;N<Pig->RoamGraph.Num();++N)
            {
                const FHousePigRoamNode& Node=Pig->RoamGraph[N];
                const float Distance=FVector::Dist2D(Node.Feet,S.Door->DoorwayCenter);
                if (Node.bInsidePigsty || !Node.GateDoorId.IsNone() || Distance<170.f) continue;
                const float Difference=FMath::Abs(Distance-DesiredDistance);
                if (Difference<Best) { Best=Difference; Goal=N; }
            }
            if (Goal==INDEX_NONE || !Pig->RequestRoamNode(Goal)) { Finish(false,TEXT("Open resized gate has no physical graph escape route")); return; }
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_RESIZE_OPEN_ROUTE name=%s goal=%s"),*Pig->PigName,*Pig->RoamGraph[Goal].Feet.ToString());
        }
        S.Stage=3; S.StageAt=Now; return;
    }
    if (S.Stage==3)
    {
        bool BothEscaped=true;
        for (int32 I=0;I<2;++I)
        {
            // A pig may naturally return to its food after exploring. Preserve
            // each observed supported escape rather than requiring both animals
            // to happen to be outside on the exact same frame.
            if (!S.Escaped[I] && S.Pigs[I]->Escapes>S.Escapes[I] && !S.Pigs[I]->IsInsidePigsty()
                && S.Pigs[I]->HallNodesVisited>0 && S.Pigs[I]->TravelDistance-S.Travel[I]>150.f)
            {
                S.Escaped[I]=true;
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_RESIZE_ESCAPED name=%s feet=%s travel=%.1f hall=%d"),
                    *S.Pigs[I]->PigName,*S.Pigs[I]->FeetLocation().ToString(),S.Pigs[I]->TravelDistance-S.Travel[I],S.Pigs[I]->HallNodesVisited);
            }
            BothEscaped &= S.Escaped[I];
        }
        if (BothEscaped) Finish(true,TEXT("Both pig sizes stayed unchanged; closed door held for five seconds; native opening enabled two real physical escapes with floor support"));
    }
}
}
#endif

void TickHousePigSmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePigResizeSmoke"))) { TickHousePigResizeSmoke(World); return; }
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePigAnimalSmoke")))
    {
        static TWeakObjectPtr<UWorld> AnimalWorld;
        AnimalWorld=World;
        static const FDelegateHandle PostTick=FWorldDelegates::OnWorldPostActorTick.AddLambda(
            [](UWorld* TickedWorld,ELevelTick,float)
            { if (TickedWorld==AnimalWorld.Get()) TickHousePigAnimalSmoke(TickedWorld); });
        (void)PostTick;
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePigMotionSmoke")))
    {
        // GameMode calls this from PrePhysics. Observe motion only after every
        // actor/component tick, including first-person skeletal animation, has
        // completed; do not compare a new pig target with last frame's hand.
        static TWeakObjectPtr<UWorld> MotionWorld;
        MotionWorld=World;
        static const FDelegateHandle PostTick=FWorldDelegates::OnWorldPostActorTick.AddLambda(
            [](UWorld* TickedWorld,ELevelTick,float)
            { if (TickedWorld==MotionWorld.Get()) TickHousePigMotionSmoke(TickedWorld); });
        (void)PostTick;
        return;
    }
    if (FParse::Param(FCommandLine::Get(),TEXT("HousePigUpgradeSmoke"))) { TickHousePigUpgradeSmoke(World); return; }
    const bool StairSmoke=FParse::Param(FCommandLine::Get(),TEXT("HousePigStairSmoke"));
    if (!World||(!FParse::Param(FCommandLine::Get(),TEXT("HousePigSmoke"))&&!StairSmoke)
        || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House")) return;
    static FPigSmoke S;
    if (S.World.Get()!=World) { S=FPigSmoke(); S.World=World; }
    if (S.Stage<0) return;
    const double Now=FPlatformTime::Seconds(); if (!S.Began) S.Began=Now;
    if (!S.bSettingsApplied && GEngine)
    {
        Scalability::FQualityLevels Quality; Quality.SetFromSingleQualityLevel(1);
        Scalability::SetQualityLevels(Quality);
        GEngine->Exec(World,TEXT("r.SetRes 1280x720w"));
        GEngine->Exec(World,TEXT("r.ScreenPercentage 100"));
        GEngine->Exec(World,TEXT("t.MaxFPS 60"));
        if (AGameModeBase* Mode=World->GetAuthGameMode()) Mode->SetActorTickInterval(0.f);
        S.bSettingsApplied=true;
        auto Value=[](const TCHAR* Name) { const IConsoleVariable* V=IConsoleManager::Get().FindConsoleVariable(Name); return V?V->GetInt():INDEX_NONE; };
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_PROFILE quality=1 resolution=1280x720 runtimeOnly=1 savedSettings=0 shadowQuality=%d giQuality=%d reflectionQuality=%d"),
            Value(TEXT("sg.ShadowQuality")),Value(TEXT("sg.GlobalIlluminationQuality")),Value(TEXT("sg.ReflectionQuality")));
    }
    auto Finish=[&](bool Pass,const FString& Detail)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_RESULT passed=%d independentFrames=%d detail=%s"),Pass?1:0,S.IndependentFrames,*Detail);
        if (StairSmoke) UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_STAIR_SMOKE_RESULT passed=%d downstairs=[%d,%d] greatRoom=[%d,%d] detail=%s"),
            Pass?1:0,S.ReachedDownstairs[0]?1:0,S.ReachedDownstairs[1]?1:0,S.ReachedGreatRoom[0]?1:0,S.ReachedGreatRoom[1]?1:0,*Detail);
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (const AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_PIG name=%s feet=%s HP=%.1f snacks=%d escapes=%d hallNodes=%d travel=%.1f gait=%.1f floorStops=%d"),
                    *Pig->PigName,*Pig->FeetLocation().ToString(),Pig->Health,Pig->SnacksEaten,Pig->Escapes,Pig->HallNodesVisited,Pig->TravelDistance,Pig->GaitDistance,Pig->UnsafeFloorStops);
        S.Stage=-1; FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);
    };
    if (Now-S.Began>(StairSmoke?200.:100.)) { Finish(false,TEXT("Native feeding/escape/house traversal timed out")); return; }
    APlayerController* PC=World->GetFirstPlayerController();
    AStoryFirstPersonCharacter* Pawn=PC?Cast<AStoryFirstPersonCharacter>(PC->GetPawn()):nullptr;
    if (!Pawn||!PC->PlayerInput||!World->GetGameViewport()) return;
    if (S.Stage!=S.LastLoggedStage)
    {
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_STAGE stage=%d elapsed=%.2f"),S.Stage,Now-S.Began);
        S.LastLoggedStage=S.Stage;
    }
    if (S.Stage>0 && Now>=S.NextProgressLog)
    {
        S.NextProgressLog=Now+3.;
        for (const TWeakObjectPtr<AHouseRoamingPig>& Ref:S.Pigs)
            if (const AHouseRoamingPig* Pig=Ref.Get())
                UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_PROGRESS stage=%d name=%s feet=%s escapes=%d hall=%d %s"),
                    S.Stage,*Pig->PigName,*Pig->FeetLocation().ToString(),Pig->Escapes,Pig->HallNodesVisited,*Pig->MovementDebug());
    }
    if (S.Stage==0)
    {
        if (Now-S.Began<3.) return;
        for (TActorIterator<AHouseRoamingPig> It(World);It;++It) S.Pigs.Add(*It);
        if (S.Pigs.Num()!=2) { Finish(false,TEXT("Expected exactly two installed pigs")); return; }
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It) if (It->DoorId==TEXT("HallEnd_West")) { S.Door=*It; break; }
        S.Food=S.Pigs[0]->SnackPile;
        if (!S.Door.IsValid()||S.Door->bLocked||!S.Food.IsValid()||S.Pigs[1]->SnackPile!=S.Food.Get())
        { Finish(false,TEXT("Pigsty gate or shared edible snack pile is unconfigured")); return; }
        if (S.Door->IsOpen()) { Finish(false,TEXT("Test starts with the pigsty door closed; map gate was already open")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); S.Start[i]=Pig->FeetLocation();
            if (!Pig->BodyMesh||!Pig->HeadMesh||!Pig->UpperLegMesh||!Pig->LowerLegMesh||!Pig->IsInsidePigsty())
            { Finish(false,TEXT("Missing pig meshes or spawn outside the pigsty")); return; }
            int32 Outside=INDEX_NONE;
            for (int32 n=0;n<Pig->RoamGraph.Num();++n) if (!Pig->RoamGraph[n].bInsidePigsty) { Outside=n; break; }
            if (Outside==INDEX_NONE||Pig->RequestRoamNode(Outside)) { Finish(false,TEXT("Closed pigsty gate permits an escape route")); return; }
            if (!Pig->RequestFeeding()) { Finish(false,TEXT("Pig cannot start physical feeding approach")); return; }
        }
        const float Before=S.Pigs[0]->Health;
        AHouseRoamingPig* HurtPig=S.Pigs[0].Get();
        const FVector BodyPoint=HurtPig->FeetLocation()+HurtPig->GetActorForwardVector()*8.5f+FVector(0,0,35);
        const FVector TraceStart=BodyPoint+HurtPig->GetActorRightVector()*150.f;
        FCollisionQueryParams WeaponQuery(SCENE_QUERY_STAT(PigSmokeWeaponTrace),false,Pawn); FHitResult WeaponHit;
        if (!World->LineTraceSingleByChannel(WeaponHit,TraceStart,BodyPoint,ECC_Visibility,WeaponQuery)||WeaponHit.GetActor()!=HurtPig)
        { Finish(false,TEXT("Existing weapon visibility channel does not target the pig body")); return; }
        const float Damage=UGameplayStatics::ApplyPointDamage(HurtPig,25.f,(BodyPoint-TraceStart).GetSafeNormal(),WeaponHit,PC,Pawn,UDamageType::StaticClass());
        S.HurtHealthAfterDamage=Before-25.f;
        if (FMath::Abs(Damage-25.f)>.01f||FMath::Abs(S.Pigs[0]->Health-(Before-25.f))>.01f||S.Pigs[1]->Health!=S.Pigs[1]->MaxHealth)
        { Finish(false,TEXT("Actual per-pig TakeDamage health update failed")); return; }
        S.Pigs[0]->RequestFeeding();
        S.FoodStarted=S.Food->FoodRemaining;
        S.RespawnsStarted=S.Food->FoodRespawns;
        S.SnacksStarted[0]=S.Pigs[0]->SnacksEaten; S.SnacksStarted[1]=S.Pigs[1]->SnacksEaten;
        if (!PlacePlayer(World,Pawn,PC,FVector(-2675,-1325,302.3),FVector(-2675,-1110,420)))
        { Finish(false,TEXT("Feeding healthbar viewpoint is blocked")); return; }
        S.Stage=1; S.StageBegan=Now; return;
    }
    for (int32 i=0;i<2;++i)
    {
        AHouseRoamingPig* Pig=S.Pigs[i].Get();
        if (!Pig) { Finish(false,TEXT("Pig actor disappeared")); return; }
        FCollisionQueryParams Query; Query.AddIgnoredActor(Pawn);
        for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
        FHitResult Floor;
        const FVector Feet=Pig->FeetLocation();
        const bool Supported=World->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,25),Feet-FVector(0,0,75),ECC_Visibility,Query)
            && Floor.ImpactNormal.Z>.5f && Feet.Z-Floor.ImpactPoint.Z<60.f;
        S.AirSeconds[i]=Supported?0:S.AirSeconds[i]+World->GetDeltaSeconds();
        if (S.AirSeconds[i]>.7f||Feet.Z< -80.f)
        {
            const UCharacterMovementComponent* Movement=Pig->GetCharacterMovement();
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_FLOOR_FAILURE name=%s feet=%s traceActor=%s traceNormal=%s tracePoint=%s movingOnGround=%d actualFloorActor=%s actualFloorNormal=%s actualFloorDistance=%.2f"),
                *Pig->PigName,*Feet.ToString(),*GetNameSafe(Floor.GetActor()),*Floor.ImpactNormal.ToString(),*Floor.ImpactPoint.ToString(),Movement->IsMovingOnGround()?1:0,
                *GetNameSafe(Movement->CurrentFloor.HitResult.GetActor()),*Movement->CurrentFloor.HitResult.ImpactNormal.ToString(),Movement->CurrentFloor.FloorDist);
            Finish(false,TEXT("Pig lost safe floor support")); return;
        }
        if (S.Stage>=11)
        {
            S.ReachedDownstairs[i]|=Feet.Z<50.f&&Feet.X< -1850.f&&Feet.Y<850.f;
            S.ReachedGreatRoom[i]|=Feet.Z<50.f&&Feet.X> -1285.f&&Feet.Y< -550.f;
        }
    }
    if (FVector::Dist2D(S.Pigs[0]->FeetLocation()-S.Start[0],S.Pigs[1]->FeetLocation()-S.Start[1])>20.f) ++S.IndependentFrames;
    if (S.Stage==1)
    {
        if (S.Door->IsOpen()||!S.Pigs[0]->IsInsidePigsty()||!S.Pigs[1]->IsInsidePigsty())
        { Finish(false,TEXT("Pigs opened or crossed a closed door")); return; }
        if (Now-S.StageBegan<5.||S.Pigs[0]->SnacksEaten<=S.SnacksStarted[0]||S.Pigs[1]->SnacksEaten<=S.SnacksStarted[1]) return;
        if (S.Food->FoodRemaining!=S.FoodStarted+(S.Food->FoodRespawns-S.RespawnsStarted)
                -(S.Pigs[0]->SnacksEaten-S.SnacksStarted[0])-(S.Pigs[1]->SnacksEaten-S.SnacksStarted[1])
            ||S.Food->FoodRemaining<0||S.Food->FoodRemaining>S.Food->FoodCount||S.Food->Cheetos->GetInstanceCount()!=S.Food->FoodRemaining
            ||S.Pigs[0]->Health<=S.HurtHealthAfterDamage||S.Pigs[0]->DamageEvents!=1)
        { Finish(false,TEXT("Eating did not remove visible food and heal the damaged pig")); return; }
        FVector LabelA,LabelB; FVector2D ScreenA,ScreenB;
        // The approach can stop at different safe feeding positions. Aim at
        // the real label anchors, then select a nearby clear observer view.
        S.Pigs[0]->ShouldShowHealthTo(PC,LabelA); S.Pigs[1]->ShouldShowHealthTo(PC,LabelB);
        const FVector LabelCenter=(LabelA+LabelB)*.5f;
        int32 Width=0,Height=0; PC->GetViewportSize(Width,Height);
        const float Scale=FMath::Clamp(Width/1400.f,.65f,1.25f);
        auto LabelFits=[&](const FVector& WorldLabel,FVector2D& Screen)
        {
            if (!PC->ProjectWorldLocationToScreen(WorldLabel,Screen,true)) return false;
            const float HalfWidth=64*Scale;
            return Screen.X>=HalfWidth+10*Scale && Screen.X<=Width-HalfWidth-10*Scale
                && Screen.Y>=150*Scale && Screen.Y+36*Scale<=Height-140*Scale;
        };
        bool ViewFound=false;
        for (const FVector& Observer:{FVector(-2675,-1325,302.3),FVector(-2750,-1300,302.3),FVector(-2600,-1300,302.3),
            FVector(-2825,-1280,302.3),FVector(-2525,-1280,302.3),FVector(-2750,-1250,302.3),FVector(-2600,-1250,302.3)})
        {
            if (FVector::Dist2D(Observer,S.Pigs[0]->FeetLocation())<130.f||FVector::Dist2D(Observer,S.Pigs[1]->FeetLocation())<130.f) continue;
            if (!PlacePlayer(World,Pawn,PC,Observer,LabelCenter,TEXT("feeding_hud"))) continue;
            if (!S.Pigs[0]->ShouldShowHealthTo(PC,LabelA)||!S.Pigs[1]->ShouldShowHealthTo(PC,LabelB)
                ||!LabelFits(LabelA,ScreenA)||!LabelFits(LabelB,ScreenB)) continue;
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_FEEDING_VIEW observer=%s labelCenter=%s"),*Observer.ToString(),*LabelCenter.ToString());
            ViewFound=true; break;
        }
        if (!ViewFound)
        {
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_HEALTH_PROJECTION_FAILED viewport=%dx%d labels=[%s,%s] camera=%s"),
                Width,Height,*ScreenA.ToString(),*ScreenB.ToString(),*Pawn->FirstPersonCamera->GetComponentLocation().ToString());
            Finish(false,TEXT("Pig health label rectangles would be clipped or covered in the feeding snapshot")); return;
        }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_HEALTH_PROJECTED viewport=%dx%d labels=[%s,%s]"),Width,Height,*ScreenA.ToString(),*ScreenB.ToString());
        S.Capture=FPaths::ProjectSavedDir()/TEXT("HousePigSmoke/pig_health_feeding.png");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(S.Capture),true); IFileManager::Get().Delete(*S.Capture,false,true);
        FScreenshotRequest::RequestScreenshot(S.Capture,true,false);
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_FEEDING HP=[%.1f,%.1f] snacks=[%d,%d] food=%d visibleFood=%d capture=%s"),
            S.Pigs[0]->Health,S.Pigs[1]->Health,S.Pigs[0]->SnacksEaten,S.Pigs[1]->SnacksEaten,S.Food->FoodRemaining,S.Food->Cheetos->GetInstanceCount(),*S.Capture);
        S.Stage=2; S.StageBegan=Now; return;
    }
    if (S.Stage==2)
    {
        if (Now-S.StageBegan<.5) return;
        if (IFileManager::Get().FileSize(*S.Capture)<=0) { if (Now-S.StageBegan>5.) Finish(false,TEXT("Feeding healthbar snapshot was not saved")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2130,-1245,302.3),S.Door->DoorwayCenter))
        { Finish(false,TEXT("Player cannot stand at pigsty gate")); return; }
        S.Stage=3; S.StageBegan=Now; return;
    }
    if (S.Stage==3)
    {
        if (Now-S.StageBegan<.2) return;
        if (Pawn->FocusedDoorForInteraction()!=S.Door.Get()||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual player E gate-opening input cannot focus door")); return; }
        S.Stage=4; S.StageBegan=Now; return;
    }
    if (S.Stage==4) { SendE(PC,IE_Released); S.Stage=5; return; }
    if (S.Stage==5)
    {
        if (Now-S.StageBegan<1.) return;
        if (!S.Door->IsOpen()||S.Door->IsActorTickEnabled()) { Finish(false,TEXT("Player E did not finish opening pigsty gate")); return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2334.8,-1245,380)))
        { Finish(false,TEXT("Post-opening observer position is blocked")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); int32 Goal=INDEX_NONE;
            const FVector Aim(-2177,i==0?-350.f:300.f,302.3); float Best=BIG_NUMBER;
            for (int32 n=0;n<Pig->RoamGraph.Num();++n)
                if (!Pig->RoamGraph[n].bInsidePigsty)
                { const float D=FVector::DistSquared(Pig->RoamGraph[n].Feet,Aim); if (D<Best) { Goal=n; Best=D; } }
            if (!Pig->RequestRoamNode(Goal)) { Finish(false,TEXT("Player-open gate has no safe house graph route")); return; }
            UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_ESCAPE_GOAL name=%s node=%d %s"),*Pig->PigName,Goal,*Pig->MovementDebug());
        }
        S.Stage=6; S.StageBegan=Now; return;
    }
    if (S.Stage==6)
    {
        if (S.Pigs[0]->Escapes<1||S.Pigs[1]->Escapes<1||S.Pigs[0]->HallNodesVisited<3||S.Pigs[1]->HallNodesVisited<3) return;
        if (S.IndependentFrames<15||S.Pigs[0]->TravelDistance<400||S.Pigs[1]->TravelDistance<400
            ||S.Pigs[0]->GaitDistance<200||S.Pigs[1]->GaitDistance<200)
        { Finish(false,TEXT("Pigs did not independently walk with actual gait and travel")); return; }
        if (!StairSmoke)
        { Finish(true,TEXT("Two independent pigs ate finite visible food, own HP/healing/HUD passed, closed gate held, actual player E enabled physical escape and house-hall wandering; floor support remained valid")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_SMOKE_MAIN_CHECKS_PASSED continuingToStairs=1"));
        S.Stage=11; S.StageBegan=Now; return;
    }
    if (S.Stage==11 || S.Stage==15)
    {
        const bool GreatGate=S.Stage==15;
        const FName Id=GreatGate?FName(TEXT("SM_OriginalLeaf_Entrance_m1_m1_Solid_01")):FName(TEXT("SM_OriginalLeaf_Upper_p0_m2_Solid_04"));
        S.TestGate.Reset();
        for (TActorIterator<AExistingHouseDoor> It(World);It;++It) if (It->DoorId==Id) { S.TestGate=*It; break; }
        if (!S.TestGate.IsValid()||S.TestGate->bLocked) { Finish(false,TEXT("Required house traversal gate is missing/locked")); return; }
        const FVector Feet=GreatGate?FVector(-1500,-715.5,8):FVector(-2177,735,302.3);
        if (!PlacePlayer(World,Pawn,PC,Feet,S.TestGate->DoorwayCenter)) { Finish(false,TEXT("Player house-gate standing approach is blocked")); return; }
        S.bClosingTestGate=S.TestGate->IsOpen();
        S.Stage=12; S.StageBegan=Now; return;
    }
    if (S.Stage==12)
    {
        if (Now-S.StageBegan<.2) return;
        if (Pawn->FocusedDoorForInteraction()!=S.TestGate.Get()||!SendE(PC,IE_Pressed))
        { Finish(false,TEXT("Actual player E cannot focus house traversal gate")); return; }
        S.Stage=13; S.StageBegan=Now; return;
    }
    if (S.Stage==13) { SendE(PC,IE_Released); S.Stage=14; return; }
    if (S.Stage==14)
    {
        if (Now-S.StageBegan<1.) return;
        if (!S.TestGate.IsValid()||S.TestGate->IsActorTickEnabled()) return;
        if (S.bClosingTestGate)
        {
            if (S.TestGate->IsOpen()) { Finish(false,TEXT("House gate did not close before actual E opening test")); return; }
            S.bClosingTestGate=false; S.Stage=12; S.StageBegan=Now; return;
        }
        if (!S.TestGate->IsOpen()) { Finish(false,TEXT("Actual player E did not open house traversal gate")); return; }
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_STAIR_SMOKE_GATE_OPENED_BY_PLAYER id=%s"),*S.TestGate->DoorId.ToString());
        if (S.TestGate->DoorId==TEXT("SM_OriginalLeaf_Upper_p0_m2_Solid_04")) { S.Stage=15; S.StageBegan=Now; return; }
        if (!PlacePlayer(World,Pawn,PC,FVector(-2900,-1300,302.3),FVector(-2334.8,-1245,380)))
        { Finish(false,TEXT("Observer cannot stand clear of the pig traversal route")); return; }
        for (int32 i=0;i<2;++i)
        {
            AHouseRoamingPig* Pig=S.Pigs[i].Get(); int32 Goal=INDEX_NONE; float Best=BIG_NUMBER;
            const FVector Target(i==0?-1071.f:-1251.f,-715.5,8);
            for (int32 n=0;n<Pig->RoamGraph.Num();++n)
            { const float Distance=FVector::DistSquared(Pig->RoamGraph[n].Feet,Target); if (Distance<Best) { Goal=n; Best=Distance; } }
            if (!Pig->RequestRoamNode(Goal)) { Finish(false,TEXT("Player-open house gates still have no physical downstairs path")); return; }
        }
        S.Stage=16; S.StageBegan=Now; return;
    }
    if (S.Stage==16)
    {
        if (!S.ReachedDownstairs[0]||!S.ReachedDownstairs[1]||!S.ReachedGreatRoom[0]||!S.ReachedGreatRoom[1]) return;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_PIG_STAIR_SMOKE_TRAVERSED downstairs=2 greatRoom=2 elapsed=%.2f"),Now-S.StageBegan);
        Finish(true,TEXT("Both pigs physically descended the registered staircase and wandered into the downstairs great room after actual player E opened both gates; floor checks ignored all pawns and stayed supported"));
    }
#endif
}
