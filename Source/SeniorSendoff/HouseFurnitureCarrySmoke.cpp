#include "HouseFurnitureCarrySmoke.h"

#if !UE_BUILD_SHIPPING
#include "HouseFurnitureInteractionComponent.h"
#include "HouseInteractiveProp.h"
#include "SeniorLobby.h"
#include "StoryCampaign.h"
#include "StoryMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "StaticMeshResources.h"

namespace
{
constexpr float CarryFloorZ=8000.f;
const FVector CarryOrigin(0,6000,CarryFloorZ);

struct FHouseCarrySmokeState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<AHouseInteractiveProp> Light,Heavy;
    TArray<TWeakObjectPtr<AActor>> Transients;
    TSharedPtr<FJsonObject> Report;
    TArray<TSharedPtr<FJsonValue>> ContactSamples,RestReports,Captures;
    FTransform SavedPawn;
    FVector SavedVelocity=FVector::ZeroVector;
    FRotator SavedView=FRotator::ZeroRotator;
    FPostProcessSettings SavedExposure;
    float SavedExposureWeight=0.f,SavedGravity=1.f,SavedWalkSpeed=0.f;
    EMovementMode SavedMovement=MOVE_Walking;
    uint8 SavedCustomMovement=0;
    int32 SavedCharacter=0,SavedLoadout=0,Stage=0,LastStage=INDEX_NONE,FailureStage=0;
    bool bSavedCursor=false,bSavedMoveIgnored=false,bSavedLookIgnored=false;
    bool bSetup=false,bFinishing=false,bPassed=false;
    bool bReachCaptured=false,bHoldCaptured=false,bHeavyCaptured=false;
    double Started=0,WallStarted=0,StageStarted=0,FinishedAt=0,RestSince=-1,LastContactSample=-1;
    FString Evidence,Detail;
    FRotator LightInitialAim=FRotator::ZeroRotator;
    FVector LightThrowStart=FVector::ZeroVector,HeavyShoveStart=FVector::ZeroVector;
    FVector HeavyDragStart=FVector::ZeroVector,PawnDragStart=FVector::ZeroVector;
    float LightThrowSpeed=0.f,HeavyShoveSpeed=0.f,LightThrowDistance=0.f,HeavyShoveDistance=0.f;
    float MaxHeavyGap=-10000.f,MaxHeavyLookUpGap=-10000.f;
    float RightHandWorst=0.f,LeftHandWorst=0.f;
};

void CarryCapture(FHouseCarrySmokeState& S,const TCHAR* Name)
{
    if(FScreenshotRequest::IsScreenshotRequested())return;
    const FString Path=FPaths::Combine(S.Evidence,FString(Name)+TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Path,false,false);
    S.Captures.Add(MakeShared<FJsonValueString>(Path));
}

void CarryAim(AStoryFirstPersonCharacter* Pawn,APlayerController* PC,const FVector& Point)
{
    PC->SetControlRotation((Point-Pawn->FirstPersonCamera->GetComponentLocation()).Rotation());
    if(PC->PlayerCameraManager)PC->PlayerCameraManager->UpdateCamera(0.f);
}

// A rotated mesh's enclosing box contains empty corners. Measure the actual
// rendered positions, never those corners, when checking floor contact.
float CarrySmokeVisibleBottom(UStaticMeshComponent* Body,FString* OutMethod=nullptr)
{
    if(!Body || !Body->GetStaticMesh())
    { if(OutMethod)*OutMethod=TEXT("unavailable");return BIG_NUMBER; }
    UStaticMesh* Mesh=Body->GetStaticMesh();
    const FTransform Transform=Body->GetComponentTransform();
    if(Mesh->GetRenderData() && !Mesh->GetRenderData()->LODResources.IsEmpty())
    {
        const FPositionVertexBuffer& Vertices=Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
        if(Vertices.GetVertexData() && Vertices.GetNumVertices()>0)
        {
            float Lowest=BIG_NUMBER;
            for(uint32 Index=0;Index<Vertices.GetNumVertices();++Index)
                Lowest=FMath::Min(Lowest,float(Transform.TransformPosition(FVector(Vertices.VertexPosition(Index))).Z));
            if(OutMethod)*OutMethod=TEXT("transformed_visible_vertices");
            return Lowest;
        }
    }
    // Cooked buffers can lack CPU positions. Sample the real triangle surface
    // from below, also from above for one-sided sheets. Cache a resting result
    // so this dense, test-only fallback does not repeatedly query sleeping props.
    struct FProbeCache { TWeakObjectPtr<UStaticMesh> Mesh;FTransform Transform;float Bottom=BIG_NUMBER; };
    static TMap<TWeakObjectPtr<UStaticMeshComponent>,FProbeCache> Cache;
    const TWeakObjectPtr<UStaticMeshComponent> Key(Body);
    if(const FProbeCache* Previous=Cache.Find(Key);Previous && Previous->Mesh.Get()==Mesh && Previous->Transform.Equals(Transform,.0001f))
    { if(OutMethod)*OutMethod=Previous->Bottom<BIG_NUMBER?TEXT("complex_surface_grid"):TEXT("unavailable");return Previous->Bottom; }
    const FBox Box=Body->Bounds.GetBox();
    const int32 StepsX=FMath::Clamp(FMath::CeilToInt(Box.GetSize().X/4.f),16,80);
    const int32 StepsY=FMath::Clamp(FMath::CeilToInt(Box.GetSize().Y/4.f),16,80);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HouseCarrySmokeVisibleSurface),true);
    float Lowest=BIG_NUMBER;
    for(int32 X=0;X<=StepsX;++X)
        for(int32 Y=0;Y<=StepsY;++Y)
        {
            const float PX=FMath::Lerp(Box.Min.X,Box.Max.X,(float(X)+.05f)/(StepsX+.1f));
            const float PY=FMath::Lerp(Box.Min.Y,Box.Max.Y,(float(Y)+.05f)/(StepsY+.1f));
            const FVector Below(PX,PY,Box.Min.Z-2.f),Above(PX,PY,Box.Max.Z+2.f);
            FHitResult Hit;
            if(Body->LineTraceComponent(Hit,Below,Above,Query))Lowest=FMath::Min(Lowest,float(Hit.ImpactPoint.Z));
            if(Body->LineTraceComponent(Hit,Above,Below,Query))Lowest=FMath::Min(Lowest,float(Hit.ImpactPoint.Z));
        }
    FProbeCache& Stored=Cache.FindOrAdd(Key);Stored.Mesh=Mesh;Stored.Transform=Transform;Stored.Bottom=Lowest;
    if(OutMethod)*OutMethod=Lowest<BIG_NUMBER?TEXT("complex_surface_grid"):TEXT("unavailable");
    return Lowest;
}

float FloorGap(const AHouseInteractiveProp* Prop)
{
    return CarrySmokeVisibleBottom(Prop->Body)-CarryFloorZ;
}

AHouseInteractiveProp* FindCarryReference(UWorld* World,bool Heavy)
{
    AHouseInteractiveProp* TableFallback=nullptr;
    for(TActorIterator<AHouseInteractiveProp> It(World);It;++It)
    {
        if(!It->IntactMesh || !It->Body || It->bBroken || !It->CanBeMoved())continue;
        const FString Name=(It->PropName+TEXT(" ")+It->PropId.ToString()).ToLower();
        const FVector Size=It->Body->Bounds.BoxExtent*2.f;
        if(!Heavy)
        {
            if((Name.Contains(TEXT("pillow")) || Name.Contains(TEXT("cushion")))
                && Size.GetMax()>=25.f && Size.GetMax()<=90.f && Size.Z>=8.f
                && FMath::Min(Size.X,Size.Y)>=24.f)
                return *It;
        }
        else if(Size.Z>40.f && Size.Z<180.f && FMath::Max(Size.X,Size.Y)>100.f
            && FMath::Max(Size.X,Size.Y)<500.f)
        {
            if(Name.Contains(TEXT("sofa")) || Name.Contains(TEXT("couch")) || Name.Contains(TEXT("sectional")))
                return *It;
            if(!TableFallback && Name.Contains(TEXT("table")))TableFallback=*It;
        }
    }
    return TableFallback;
}

AHouseInteractiveProp* SpawnCarryProp(FHouseCarrySmokeState& S,UWorld* World,bool Heavy)
{
    AHouseInteractiveProp* Reference=FindCarryReference(World,Heavy);
    const FTransform Transform(Reference?Reference->GetActorRotation():FRotator::ZeroRotator,
        CarryOrigin+FVector(150,0,100),
        Reference?Reference->GetActorScale3D():(Heavy?FVector(1.8f,1.3f,.75f):FVector(.42f,.34f,.24f)));
    AHouseInteractiveProp* Prop=World->SpawnActorDeferred<AHouseInteractiveProp>(
        AHouseInteractiveProp::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!Prop)return nullptr;
    Prop->SetFlags(RF_Transient);
    Prop->IntactMesh=Reference?Reference->IntactMesh.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    Prop->PropName=Heavy?TEXT("carry smoke heavy sofa"):TEXT("carry smoke light pillow");
    Prop->PropId=Heavy?TEXT("CarrySmoke_Heavy"):TEXT("CarrySmoke_Light");
    Prop->MassKg=Heavy?180.f:8.f;
    // Keep the original two-hand and drag expectations for this check.
    Prop->CarrySize=Heavy?ESSOCarrySize::Large:ESSOCarrySize::Medium;
    Prop->bMovable=true;
    Prop->bBreakable=false;
    Prop->bWallMounted=false;
    if(Reference)
        for(int32 Slot=0;Slot<Reference->Body->GetNumMaterials();++Slot)
            Prop->MaterialOverrides.Add(Reference->Body->GetMaterial(Slot));
    Prop->FinishSpawning(Transform);
    S.Transients.Add(Prop);
    const FBox Box=Prop->Body->Bounds.GetBox();
    // Put the nearest visible edge at a realistic reach, keeping the complete
    // original model and its collision hull at the authored scale.
    const FVector Delta(CarryOrigin.X+(Heavy?125.f:105.f)-Box.Min.X,
        CarryOrigin.Y-Prop->Body->Bounds.Origin.Y,CarryFloorZ+2.f-Box.Min.Z);
    Prop->SetActorLocation(Prop->GetActorLocation()+Delta,false,nullptr,ETeleportType::TeleportPhysics);
    Prop->WakeForInteraction();
    S.Report->SetStringField(Heavy?TEXT("heavy_source_prop"):TEXT("light_source_prop"),
        Reference?Reference->PropId.ToString():TEXT("engine_cube_fallback"));
    S.Report->SetStringField(Heavy?TEXT("heavy_mesh"):TEXT("light_mesh"),GetNameSafe(Prop->IntactMesh));
    S.Report->SetNumberField(Heavy?TEXT("heavy_mass_kg"):TEXT("light_mass_kg"),Prop->Body->GetMass());
    return Prop;
}

bool RecordHands(FHouseCarrySmokeState& S,AStoryFirstPersonCharacter* Pawn,double Now,const TCHAR* Phase,bool RequireContact)
{
    UHouseFurnitureInteractionComponent* Mover=Pawn->FurnitureInteraction;
    FVector RightContact,RightNormal,LeftContact,LeftNormal;
    const bool bRight=Mover->GetHandContact(true,RightContact,RightNormal);
    const bool bLeft=Mover->GetHandContact(false,LeftContact,LeftNormal);
    const FVector RightPalm=Pawn->GetFurnitureHandGripPoint(true);
    const FVector LeftPalm=Pawn->GetFurnitureHandGripPoint(false);
    const float RightDistance=bRight?FVector::Dist(RightPalm,RightContact):100000.f;
    const float LeftDistance=bLeft?FVector::Dist(LeftPalm,LeftContact):100000.f;
    if(RequireContact)
    {
        S.RightHandWorst=FMath::Max(S.RightHandWorst,RightDistance);
        S.LeftHandWorst=FMath::Max(S.LeftHandWorst,LeftDistance);
    }
    if(Now-S.LastContactSample>.08 || RequireContact)
    {
        S.LastContactSample=Now;
        TSharedPtr<FJsonObject> Sample=MakeShared<FJsonObject>();
        Sample->SetStringField(TEXT("phase"),Phase);
        Sample->SetNumberField(TEXT("seconds"),Now-S.Started);
        Sample->SetNumberField(TEXT("right_distance_cm"),RightDistance);
        Sample->SetNumberField(TEXT("left_distance_cm"),LeftDistance);
        Sample->SetStringField(TEXT("right_palm_cm"),RightPalm.ToString());
        Sample->SetStringField(TEXT("right_surface_cm"),RightContact.ToString());
        Sample->SetStringField(TEXT("left_palm_cm"),LeftPalm.ToString());
        Sample->SetStringField(TEXT("left_surface_cm"),LeftContact.ToString());
        Sample->SetNumberField(TEXT("carry_pose_blend"),Mover->GetCarryPoseBlend());
        Sample->SetNumberField(TEXT("finger_grip_blend"),Mover->GetFingerGripBlend());
        Sample->SetNumberField(TEXT("pickup_bend_cm"),Mover->GetPickupBend());
        Sample->SetBoolField(TEXT("uses_two_hands"),Mover->UsesTwoHands());
        S.ContactSamples.Add(MakeShared<FJsonValueObject>(Sample));
    }
    return bRight && bLeft && !RightPalm.IsNearlyZero() && !LeftPalm.IsNearlyZero()
        && RightDistance<=8.f && LeftDistance<=8.f;
}

bool IsCarryResting(FHouseCarrySmokeState& S,AHouseInteractiveProp* Prop,double Now,const TCHAR* Phase)
{
    if(!Prop || !Prop->Body->IsSimulatingPhysics() || !Prop->Body->IsGravityEnabled())return false;
    FString GapMethod;
    const float Gap=CarrySmokeVisibleBottom(Prop->Body,&GapMethod)-CarryFloorZ;
    const float Speed=Prop->Body->GetPhysicsLinearVelocity().Size();
    const float Spin=Prop->Body->GetPhysicsAngularVelocityInDegrees().Size();
    S.Report->SetNumberField(TEXT("last_rest_floor_gap_cm"),Gap);
    S.Report->SetNumberField(TEXT("last_rest_linear_speed_cm_s"),Speed);
    S.Report->SetNumberField(TEXT("last_rest_angular_speed_deg_s"),Spin);
    S.Report->SetStringField(TEXT("last_rest_actor_location"),Prop->GetActorLocation().ToString());
    S.Report->SetStringField(TEXT("last_rest_actor_rotation"),Prop->GetActorRotation().ToString());
    S.Report->SetStringField(TEXT("last_rest_floor_gap_method"),GapMethod);
    S.Report->SetNumberField(TEXT("last_rest_bounds_floor_gap_cm"),Prop->Body->Bounds.GetBox().Min.Z-CarryFloorZ);
    const bool Resting=Gap<=8.f && Gap>=-12.f && Speed<12.f && Spin<18.f;
    if(!Resting)S.RestSince=-1.;
    else if(S.RestSince<0.)S.RestSince=Now;
    if(S.RestSince<0. || Now-S.RestSince<.45)return false;
    TSharedPtr<FJsonObject> Rest=MakeShared<FJsonObject>();
    Rest->SetStringField(TEXT("phase"),Phase);
    Rest->SetNumberField(TEXT("floor_gap_cm"),Gap);
    Rest->SetStringField(TEXT("floor_gap_method"),GapMethod);
    Rest->SetStringField(TEXT("actor_rotation"),Prop->GetActorRotation().ToString());
    Rest->SetNumberField(TEXT("linear_speed_cm_s"),Speed);
    Rest->SetNumberField(TEXT("angular_speed_deg_s"),Spin);
    Rest->SetBoolField(TEXT("gravity_enabled"),Prop->Body->IsGravityEnabled());
    Rest->SetNumberField(TEXT("seconds_to_settle"),Now-S.StageStarted);
    S.RestReports.Add(MakeShared<FJsonValueObject>(Rest));
    return true;
}
}

void TickHouseFurnitureCarrySmoke(AStoryFirstPersonCharacter* Pawn)
{
    if(!Pawn || !Pawn->HasAuthority() || !Pawn->IsLocallyControlled()
        || !FParse::Param(FCommandLine::Get(),TEXT("HouseFurnitureCarrySmoke")))return;
    UWorld* World=Pawn->GetWorld();
    if(!World || UGameplayStatics::GetCurrentLevelName(World,true)!=TEXT("Chapter01_House"))return;
    APlayerController* PC=Cast<APlayerController>(Pawn->GetController());
    ASeniorLobbyPlayerState* Selection=Pawn->GetPlayerState<ASeniorLobbyPlayerState>();
    UHouseFurnitureInteractionComponent* Mover=Pawn->FurnitureInteraction;
    if(!PC || !Selection || !Mover || !Pawn->FirstPersonCamera || !Pawn->FirstPersonArms)return;
    static FHouseCarrySmokeState S;
    const double Now=World->GetTimeSeconds();
    if(S.World.Get()!=World)
    {
        S=FHouseCarrySmokeState();S.World=World;S.Started=S.StageStarted=Now;S.WallStarted=FPlatformTime::Seconds();
        S.Report=MakeShared<FJsonObject>();
        FParse::Value(FCommandLine::Get(),TEXT("HouseFurnitureCarryEvidence="),S.Evidence,false);
        if(S.Evidence.IsEmpty())S.Evidence=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("HouseFurnitureCarrySmoke"));
        S.Evidence=FPaths::ConvertRelativePathToFull(S.Evidence);
        IFileManager::Get().MakeDirectory(*S.Evidence,true);
    }
    if(S.Stage<0)return;
    auto Stage=[&](int32 Next){S.Stage=Next;S.StageStarted=Now;S.RestSince=-1.;};
    auto Finish=[&](bool Passed,const FString& Detail)
    {
        S.bFinishing=true;S.bPassed=Passed;S.Detail=Detail;S.FailureStage=S.Stage;S.FinishedAt=Now;
        if(!Passed)CarryCapture(S,TEXT("carry_failure"));
    };
    if(S.bFinishing)
    {
        if(Now-S.FinishedAt<.25 || (FScreenshotRequest::IsScreenshotRequested() && Now-S.FinishedAt<2.))return;
        S.Report->SetBoolField(TEXT("passed"),S.bPassed);
        S.Report->SetStringField(TEXT("detail"),S.Detail);
        S.Report->SetNumberField(TEXT("stage"),S.FailureStage);
        S.Report->SetNumberField(TEXT("seconds"),Now-S.Started);
        S.Report->SetStringField(TEXT("method"),TEXT("Native furniture E grab/drop and held-object weapon-input throw, real animated palm contacts, original authored clone geometry, isolated transient floor at 8000 cm; no map saves."));
        S.Report->SetNumberField(TEXT("worst_full_hold_right_contact_cm"),S.RightHandWorst);
        S.Report->SetNumberField(TEXT("worst_full_hold_left_contact_cm"),S.LeftHandWorst);
        S.Report->SetNumberField(TEXT("light_throw_peak_speed_cm_s"),S.LightThrowSpeed);
        S.Report->SetNumberField(TEXT("heavy_shove_peak_speed_cm_s"),S.HeavyShoveSpeed);
        S.Report->SetNumberField(TEXT("light_throw_horizontal_distance_cm"),S.LightThrowDistance);
        S.Report->SetNumberField(TEXT("heavy_shove_horizontal_distance_cm"),S.HeavyShoveDistance);
        S.Report->SetNumberField(TEXT("heavy_max_visible_floor_gap_cm"),S.MaxHeavyGap);
        S.Report->SetNumberField(TEXT("heavy_look_up_max_visible_floor_gap_cm"),S.MaxHeavyLookUpGap);
        S.Report->SetArrayField(TEXT("hand_contact_samples"),S.ContactSamples);
        S.Report->SetArrayField(TEXT("gravity_settling"),S.RestReports);
        S.Report->SetArrayField(TEXT("captures"),S.Captures);
        if(S.bSetup)
        {
            Mover->ReleaseGrab();
            Selection->CharacterIndex=S.SavedCharacter;Selection->LoadoutIndex=S.SavedLoadout;
            Pawn->ApplySelectedCharacter();
            Pawn->FirstPersonCamera->PostProcessSettings=S.SavedExposure;
            Pawn->FirstPersonCamera->PostProcessBlendWeight=S.SavedExposureWeight;
            Pawn->GetCharacterMovement()->GravityScale=S.SavedGravity;
            Pawn->GetCharacterMovement()->MaxWalkSpeed=S.SavedWalkSpeed;
            Pawn->GetCharacterMovement()->SetMovementMode(S.SavedMovement,S.SavedCustomMovement);
            Pawn->SetActorTransform(S.SavedPawn,false,nullptr,ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->Velocity=S.SavedVelocity;
            PC->SetControlRotation(S.SavedView);PC->bShowMouseCursor=S.bSavedCursor;
            S.Report->SetBoolField(TEXT("input_ignore_state_preserved"),
                PC->IsMoveInputIgnored()==S.bSavedMoveIgnored && PC->IsLookInputIgnored()==S.bSavedLookIgnored);
        }
        for(TWeakObjectPtr<AActor> Actor:S.Transients)if(Actor.IsValid())Actor->Destroy();
        FString Json;FJsonSerializer::Serialize(S.Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(S.Evidence,TEXT("result.json")));
        UE_LOG(LogTemp,Display,TEXT("HOUSE_FURNITURE_CARRY_SMOKE_RESULT passed=%d stage=%d detail=%s"),
            S.bPassed?1:0,S.FailureStage,*S.Detail);
        S.Stage=-1;
        FPlatformMisc::RequestExitWithStatus(false,S.bPassed?0:1);
        return;
    }
    if(FPlatformTime::Seconds()-S.WallStarted>100.)
    {Finish(false,TEXT("Carry smoke exceeded 100 seconds."));return;}
    if(S.Stage!=S.LastStage)
    {
        S.LastStage=S.Stage;
        UE_LOG(LogTemp,Display,TEXT("HOUSE_FURNITURE_CARRY_SMOKE_STAGE stage=%d seconds=%.2f"),S.Stage,Now-S.Started);
    }
    const float Age=Now-S.StageStarted;
    AHouseInteractiveProp* Light=S.Light.Get();
    AHouseInteractiveProp* Heavy=S.Heavy.Get();
    if(S.Stage==0)
    {
        if(Age<2.f)return;
        FString UserDir;
        if(!FParse::Value(FCommandLine::Get(),TEXT("UserDir="),UserDir) || UserDir.IsEmpty())
        {Finish(false,TEXT("Carry smoke needs an isolated UserDir to protect player saves."));return;}
        if(Mover->bHoldingFurniture || Pawn->CarriedHousePig())
        {Finish(false,TEXT("Carry smoke starts with empty hands."));return;}
        S.SavedPawn=Pawn->GetActorTransform();S.SavedView=PC->GetControlRotation();
        S.SavedVelocity=Pawn->GetCharacterMovement()->Velocity;
        S.SavedGravity=Pawn->GetCharacterMovement()->GravityScale;
        S.SavedWalkSpeed=Pawn->GetCharacterMovement()->MaxWalkSpeed;
        S.SavedMovement=Pawn->GetCharacterMovement()->MovementMode;
        S.SavedCustomMovement=Pawn->GetCharacterMovement()->CustomMovementMode;
        S.SavedCharacter=Selection->CharacterIndex;S.SavedLoadout=Selection->LoadoutIndex;
        S.SavedExposure=Pawn->FirstPersonCamera->PostProcessSettings;
        S.SavedExposureWeight=Pawn->FirstPersonCamera->PostProcessBlendWeight;
        S.bSavedCursor=PC->bShowMouseCursor;S.bSavedMoveIgnored=PC->IsMoveInputIgnored();S.bSavedLookIgnored=PC->IsLookInputIgnored();
        S.bSetup=true;
        Selection->CharacterIndex=0;Pawn->ApplySelectedCharacter();PC->bShowMouseCursor=false;
        FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Floor=World->SpawnActor<AStaticMeshActor>(CarryOrigin-FVector(0,0,20),FRotator::ZeroRotator,Params);
        if(!Floor){Finish(false,TEXT("Could not create carry test floor."));return;}
        Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
        Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Floor->SetActorScale3D(FVector(80,80,.4f));
        Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
        S.Transients.Add(Floor);
        for(const FVector& Offset:{FVector(150,-180,350),FVector(220,220,450)})
            if(APointLight* Lamp=World->SpawnActor<APointLight>(CarryOrigin+Offset,FRotator::ZeroRotator,Params))
            {
                UPointLightComponent* Bulb=Cast<UPointLightComponent>(Lamp->GetLightComponent());
                Bulb->SetMobility(EComponentMobility::Movable);Bulb->SetIntensityUnits(ELightUnits::Lumens);
                Bulb->SetIntensity(180000.f);Bulb->SetAttenuationRadius(2000.f);Bulb->SetCastShadows(false);
                S.Transients.Add(Lamp);
            }
        FPostProcessSettings& Exposure=Pawn->FirstPersonCamera->PostProcessSettings;
        Exposure.bOverride_AutoExposureMethod=true;Exposure.AutoExposureMethod=AEM_Basic;
        Exposure.bOverride_AutoExposureMinBrightness=true;Exposure.AutoExposureMinBrightness=8.f;
        Exposure.bOverride_AutoExposureMaxBrightness=true;Exposure.AutoExposureMaxBrightness=8.f;
        Exposure.bOverride_AutoExposureBias=true;Exposure.AutoExposureBias=0.f;
        Pawn->FirstPersonCamera->PostProcessBlendWeight=1.f;
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Pawn->SetActorLocation(CarryOrigin+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),
            false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator::ZeroRotator);
        S.Light=SpawnCarryProp(S,World,false);
        if(!S.Light.IsValid()){Finish(false,TEXT("Could not create light carry object."));return;}
        Stage(1);return;
    }
    if(S.Stage==1)
    {
        if(Age<.8f)return;
        CarryAim(Pawn,PC,Light->Body->Bounds.Origin);
        S.LightInitialAim=PC->GetControlRotation();
        if(Mover->FocusedFurnitureForInteraction()!=Light)
        {Finish(false,TEXT("Light object was not reachable under the real furniture focus trace."));return;}
        Mover->ToggleGrab();
        if(Mover->GetHeldBody()!=Light->Body || Mover->IsDraggingHeavyFurniture())
        {Finish(false,TEXT("Native light pickup failed or was classified as heavy."));return;}
        if(!Mover->UsesTwoHands())
        {Finish(false,TEXT("The selected broad light test object did not provide two real grip surfaces."));return;}
        Stage(2);return;
    }
    if(S.Stage==2)
    {
        if(!Mover->bHoldingFurniture){Finish(false,TEXT("Light pickup released unexpectedly."));return;}
        RecordHands(S,Pawn,Now,TEXT("light_reach"),false);
        if(Age>.16f && !S.bReachCaptured){CarryCapture(S,TEXT("light_reach"));S.bReachCaptured=true;}
        // Raise the gaze naturally during the lift, rather than keeping the
        // camera aimed at the floor after the object reaches the hands.
        const float Turn=FMath::SmoothStep(.35f,1.4f,Age);
        PC->SetControlRotation(FMath::Lerp(S.LightInitialAim,FRotator::ZeroRotator,Turn));
        if(Age<2.3f)return;
        if(Mover->GetCarryPoseBlend()<.985f || Mover->GetFingerGripBlend()<.98f
            || !RecordHands(S,Pawn,Now,TEXT("light_full_hold"),true))
        {Finish(false,TEXT("Light full hold did not place both animated palms within 8 cm of their real mesh contacts."));return;}
        if(!S.bHoldCaptured){CarryCapture(S,TEXT("light_two_hands_hold"));S.bHoldCaptured=true;}
        S.Report->SetNumberField(TEXT("light_movement_scale"),Mover->GetMovementScale());
        S.Report->SetNumberField(TEXT("light_held_floor_gap_cm"),FloorGap(Light));
        Stage(3);return;
    }
    if(S.Stage==3)
    {
        if(Age<.45f)return;
        if(!RecordHands(S,Pawn,Now,TEXT("light_full_hold"),true))
        {Finish(false,TEXT("Light hand contact drifted during a steady hold."));return;}
        Mover->ToggleGrab();
        S.Report->SetBoolField(TEXT("light_native_drop_released"),!Mover->bHoldingFurniture && Light->Body->IsGravityEnabled());
        Stage(4);return;
    }
    if(S.Stage==4)
    {
        if(IsCarryResting(S,Light,Now,TEXT("light_drop")))
        {
            CarryAim(Pawn,PC,Light->Body->Bounds.Origin);
            Stage(44);return;
        }
        if(Age>7.f)
        {
            CarryAim(Pawn,PC,Light->Body->Bounds.Origin);
            Finish(false,TEXT("Dropped light object did not fall and settle under gravity."));return;
        }
    }
    if(S.Stage==44)
    {
        if(Age<.05f)return;
        CarryCapture(S,TEXT("light_drop_settled_downlook"));
        Stage(45);return;
    }
    if(S.Stage==45)
    {
        // Let the down-looking resting screenshot complete before relocating
        // the pawn to grab the item where physics actually placed it.
        if(Age<.2f || FScreenshotRequest::IsScreenshotRequested())return;
        Pawn->SetActorLocation(FVector(Light->Body->Bounds.Origin.X-105.f,
            Light->Body->Bounds.Origin.Y,CarryFloorZ+Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),
            false,nullptr,ETeleportType::TeleportPhysics);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Stage(5);return;
    }
    if(S.Stage==5)
    {
        if(Age<.45f)return;
        CarryAim(Pawn,PC,Light->Body->Bounds.Origin);
        Mover->ToggleGrab();
        if(Mover->GetHeldBody()!=Light->Body){Finish(false,TEXT("Could not regrab the landed light object."));return;}
        Stage(6);return;
    }
    if(S.Stage==6)
    {
        PC->SetControlRotation(FRotator(12.f,0,0));
        if(Age<1.8f)return;
        S.LightThrowStart=Light->Body->Bounds.Origin;
        Pawn->ThrowEquippedWeapon();
        if(Mover->bHoldingFurniture){Finish(false,TEXT("Native held-object weapon input did not throw the light object."));return;}
        Stage(7);return;
    }
    if(S.Stage==7)
    {
        if(Age<.35f)S.LightThrowSpeed=FMath::Max(S.LightThrowSpeed,float(Light->Body->GetPhysicsLinearVelocity().Size()));
        S.LightThrowDistance=FMath::Max(S.LightThrowDistance,float(FVector::Dist2D(S.LightThrowStart,Light->Body->Bounds.Origin)));
        if(Age>.25f && S.LightThrowSpeed<300.f){Finish(false,TEXT("Light item did not receive a real throw velocity."));return;}
        if(Age>.6f && IsCarryResting(S,Light,Now,TEXT("light_throw")))
        {
            Pawn->SetActorLocation(CarryOrigin+FVector(0,0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f),
                false,nullptr,ETeleportType::TeleportPhysics);
            Pawn->GetCharacterMovement()->StopMovementImmediately();PC->SetControlRotation(FRotator::ZeroRotator);
            S.Heavy=SpawnCarryProp(S,World,true);
            if(!S.Heavy.IsValid()){Finish(false,TEXT("Could not create heavy furniture clone."));return;}
            Stage(8);return;
        }
        if(Age>9.f){Finish(false,TEXT("Thrown light object failed to settle back on the test floor."));return;}
    }
    if(S.Stage==8)
    {
        if(Age<.9f)return;
        CarryAim(Pawn,PC,Heavy->Body->Bounds.Origin);
        if(Mover->FocusedFurnitureForInteraction()!=Heavy)
        {Finish(false,TEXT("Authored heavy furniture was not reachable by the real focus trace."));return;}
        Mover->ToggleGrab();
        if(Mover->GetHeldBody()!=Heavy->Body || !Mover->IsDraggingHeavyFurniture())
        {Finish(false,TEXT("180 kg furniture was not held as a grounded drag."));return;}
        S.Report->SetNumberField(TEXT("heavy_movement_scale"),Mover->GetMovementScale());
        if(Mover->GetMovementScale()>=.6f)
        {Finish(false,TEXT("Heavy dragging did not slow the player's movement."));return;}
        UStoryMovementComponent* Movement=Cast<UStoryMovementComponent>(Pawn->GetCharacterMovement());
        if(!Movement || !Movement->IsMovingOnGround() || Movement->IsCrouching()
            || Movement->bWantsToCrouch || Movement->IsSliding())
        {Finish(false,TEXT("Heavy slide check requires the actual grounded standing movement state."));return;}
        const FVector BeforeSlideVelocity=Movement->Velocity;
        const float AttemptSpeed=FMath::Max(Movement->SlideStartSpeed+30.f,Movement->SlideEndSpeed+1.f);
        Movement->Velocity=FVector(AttemptSpeed,0,0);
        // Exercise slide entry itself: a crouch request at sufficient speed
        // used to grant SlideEntrySpeed even when GetMaxSpeed was carry-limited.
        Movement->SetSlideHeld(true);
        Movement->UpdateCharacterStateBeforeMovement(1.f/60.f);
        const bool bSlidePrerequisites=Movement->IsMovingOnGround() && Movement->IsCrouching()
            && Movement->bWantsToCrouch && Mover->bHoldingFurniture;
        const bool bEnteredSlide=Movement->IsSliding();
        const float AfterAttemptSpeed=Movement->Velocity.Size2D();
        Movement->SetSlideHeld(false);
        Movement->UpdateCharacterStateBeforeMovement(1.f/60.f);
        Movement->Velocity=BeforeSlideVelocity;
        const bool bSlideStateRestored=!Movement->IsCrouching() && !Movement->bWantsToCrouch && !Movement->IsSliding();
        S.Report->SetBoolField(TEXT("heavy_crouch_slide_prerequisites_exercised"),bSlidePrerequisites);
        S.Report->SetBoolField(TEXT("heavy_slide_entry_blocked"),!bEnteredSlide);
        S.Report->SetNumberField(TEXT("heavy_slide_attempt_speed_cm_s"),AttemptSpeed);
        S.Report->SetNumberField(TEXT("heavy_slide_after_request_speed_cm_s"),AfterAttemptSpeed);
        S.Report->SetBoolField(TEXT("heavy_slide_flags_and_velocity_restored"),bSlideStateRestored && Movement->Velocity.Equals(BeforeSlideVelocity));
        if(!bSlidePrerequisites || bEnteredSlide || AfterAttemptSpeed>AttemptSpeed+.5f || !bSlideStateRestored)
        {Finish(false,TEXT("Crouching at slide speed while holding heavy furniture entered a slide, boosted velocity, or failed to restore movement state."));return;}
        S.HeavyDragStart=Heavy->Body->Bounds.Origin;S.PawnDragStart=Pawn->GetActorLocation();
        Stage(9);return;
    }
    if(S.Stage==9)
    {
        if(!Mover->bHoldingFurniture){Finish(false,TEXT("Heavy drag released unexpectedly."));return;}
        S.MaxHeavyGap=FMath::Max(S.MaxHeavyGap,FloorGap(Heavy));
        if(Age>.8f && Age<1.8f)Pawn->AddMovementInput(-FVector::ForwardVector,.2f);
        if(Age>2.f)Pawn->GetCharacterMovement()->StopMovementImmediately();
        RecordHands(S,Pawn,Now,TEXT("heavy_drag"),false);
        if(Age<2.5f)return;
        if(S.MaxHeavyGap>8.f){Finish(false,TEXT("Heavy furniture floated more than 8 cm above the floor while being dragged."));return;}
        S.Report->SetNumberField(TEXT("heavy_drag_travel_cm"),FVector::Dist2D(S.HeavyDragStart,Heavy->Body->Bounds.Origin));
        S.Report->SetNumberField(TEXT("player_drag_travel_cm"),FVector::Dist2D(S.PawnDragStart,Pawn->GetActorLocation()));
        if(FVector::Dist2D(S.HeavyDragStart,Heavy->Body->Bounds.Origin)<3.f
            || FVector::Dist2D(S.PawnDragStart,Pawn->GetActorLocation())<3.f)
        {Finish(false,TEXT("The native movement input did not actually drag the heavy furniture across the floor."));return;}
        if(!RecordHands(S,Pawn,Now,TEXT("heavy_full_hold"),true))
        {Finish(false,TEXT("Heavy grip did not put both animated palms within 8 cm of the mesh contacts."));return;}
        CarryCapture(S,TEXT("heavy_two_hands_drag"));S.bHeavyCaptured=true;
        Stage(10);return;
    }
    if(S.Stage==10)
    {
        PC->SetControlRotation(FRotator(65.f,0,0));
        S.MaxHeavyLookUpGap=FMath::Max(S.MaxHeavyLookUpGap,FloorGap(Heavy));
        if(S.MaxHeavyLookUpGap>8.f){Finish(false,TEXT("Looking upward lifted the heavy furniture off the floor."));return;}
        if(Age<1.5f)return;
        CarryCapture(S,TEXT("heavy_look_up_stays_grounded"));
        Mover->ToggleGrab();
        S.Report->SetBoolField(TEXT("heavy_native_drop_released"),!Mover->bHoldingFurniture && Heavy->Body->IsGravityEnabled());
        Stage(11);return;
    }
    if(S.Stage==11)
    {
        if(IsCarryResting(S,Heavy,Now,TEXT("heavy_drop")))
        {
            CarryAim(Pawn,PC,Heavy->Body->Bounds.Origin);
            Stage(12);return;
        }
        if(Age>7.f){Finish(false,TEXT("Released heavy furniture did not settle with gravity enabled."));return;}
    }
    if(S.Stage==12)
    {
        if(Age<.35f)return;
        CarryAim(Pawn,PC,Heavy->Body->Bounds.Origin);
        Mover->ToggleGrab();
        if(Mover->GetHeldBody()!=Heavy->Body){Finish(false,TEXT("Could not regrab heavy furniture for the shove test."));return;}
        Stage(13);return;
    }
    if(S.Stage==13)
    {
        if(Age<1.8f)return;
        PC->SetControlRotation(FRotator(65.f,0,0));
        S.HeavyShoveStart=Heavy->Body->Bounds.Origin;
        Pawn->ThrowEquippedWeapon();
        if(Mover->bHoldingFurniture){Finish(false,TEXT("Native held-object weapon input did not release the heavy shove."));return;}
        Stage(14);return;
    }
    if(S.Stage==14)
    {
        if(Age<.35f)S.HeavyShoveSpeed=FMath::Max(S.HeavyShoveSpeed,float(Heavy->Body->GetPhysicsLinearVelocity().Size()));
        S.HeavyShoveDistance=FMath::Max(S.HeavyShoveDistance,float(FVector::Dist2D(S.HeavyShoveStart,Heavy->Body->Bounds.Origin)));
        if(Age>.6f && IsCarryResting(S,Heavy,Now,TEXT("heavy_shove")))
        {
            if(S.HeavyShoveSpeed>=S.LightThrowSpeed*.5f || S.HeavyShoveDistance>=S.LightThrowDistance*.5f)
            {Finish(false,TEXT("Heavy furniture shove was not substantially shorter/slower than the light throw."));return;}
            if(S.HeavyShoveSpeed<5.f){Finish(false,TEXT("Heavy furniture did not receive a measurable shove."));return;}
            S.Report->SetBoolField(TEXT("heavy_shove_shorter_than_light_throw"),true);
            Finish(true,TEXT("Both animated hands contact carried furniture; light objects lift, drop and throw; 180 kg furniture drags, slows the player, stays grounded while looking up, shoves a short distance, and settles under gravity."));
            return;
        }
        if(Age>8.f){Finish(false,TEXT("Heavy shove did not settle on the floor."));return;}
    }
}
#else
void TickHouseFurnitureCarrySmoke(AStoryFirstPersonCharacter*) {}
#endif
