#include "HouseRoamingPig.h"
#include "ExistingHouseDoor.h"
#include "StoryCampaign.h"

#if !UE_BUILD_SHIPPING
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
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
bool PlacePlayer(UWorld* World,AStoryFirstPersonCharacter* Pawn,APlayerController* PC,FVector Feet,FVector Look)
{
    FCollisionQueryParams Query; Query.AddIgnoredActor(Pawn);
    for (TActorIterator<APawn> It(World);It;++It) Query.AddIgnoredActor(*It);
    FHitResult Floor;
    if (!World->LineTraceSingleByChannel(Floor,Feet+FVector(0,0,35),Feet-FVector(0,0,70),ECC_Visibility,Query)||Floor.ImpactNormal.Z<.65f) return false;
    const float Half=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Center=Floor.ImpactPoint+FVector(0,0,Half+2.f);
    FCollisionQueryParams SpaceQuery; SpaceQuery.AddIgnoredActor(Pawn);
    if (World->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(
        Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius(),Half),SpaceQuery)) return false;
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
}
#endif

void TickHousePigSmoke(UWorld* World)
{
#if !UE_BUILD_SHIPPING
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
        if (S.AirSeconds[i]>.7f||Feet.Z< -80.f) { Finish(false,TEXT("Pig lost safe floor support")); return; }
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
        if (!S.Pigs[0]->ShouldShowHealthTo(PC,LabelA)||!S.Pigs[1]->ShouldShowHealthTo(PC,LabelB))
        { Finish(false,TEXT("Independent healthbars are outside the feeding HUD context")); return; }
        int32 Width=0,Height=0; PC->GetViewportSize(Width,Height);
        const float Scale=FMath::Clamp(Width/1400.f,.65f,1.25f);
        auto LabelFits=[&](const FVector& WorldLabel,FVector2D& Screen)
        {
            if (!PC->ProjectWorldLocationToScreen(WorldLabel,Screen,true)) return false;
            const float HalfWidth=64*Scale;
            return Screen.X>=HalfWidth+10*Scale && Screen.X<=Width-HalfWidth-10*Scale
                && Screen.Y>=150*Scale && Screen.Y+36*Scale<=Height-140*Scale;
        };
        if (!LabelFits(LabelA,ScreenA)||!LabelFits(LabelB,ScreenB))
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
